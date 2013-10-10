/*
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2005 Atheros Communications, Inc.
 * Copyright (c) 2008-2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "opt_ah.h"

#ifdef AH_SUPPORT_AR5212

#include "ah.h"
#include "ah_internal.h"

#include "ar5212/ar5212.h"
#include "ar5212/ar5212reg.h"
#include "ar5212/ar5212desc.h"

/*
 * Note: The key cache hardware requires that each double-word
 * pair be written in even/odd order (since the destination is
 * a 64-bit register).  Don't reorder the writes in this code
 * w/o considering this!
 */
#define	KEY_XOR			0xaa

#define	IS_MIC_ENABLED(ah) \
	(AH5212(ah)->ah_staId1Defaults & AR_STA_ID1_CRPT_MIC_ENABLE)

/* work around for writing key cache */
static void 
OS_KC_WRITE(struct ath_hal *ah, u_int reg, u_int32_t val)
{
    OS_REG_WRITE(ah, reg, val);

    /* Read from a PCI clock domain register to flush previous write */
    if (AH_PRIVATE(ah)->ah_singleWriteKC) {
        OS_REG_READ(ah, AR_PCICFG);
    }
}

/*
 * Return the size of the hardware key cache.
 */
u_int32_t
ar5212GetKeyCacheSize(struct ath_hal *ah)
{
	return AH_PRIVATE(ah)->ah_caps.halKeyCacheSize;
}

/*
 * Return true if the specific key cache entry is valid.
 */
HAL_BOOL
ar5212IsKeyCacheEntryValid(struct ath_hal *ah, u_int16_t entry)
{
	if (entry < AH_PRIVATE(ah)->ah_caps.halKeyCacheSize) {
		u_int32_t val = OS_REG_READ(ah, AR_KEYTABLE_MAC1(entry));
		if (val & AR_KEYTABLE_VALID)
			return AH_TRUE;
	}
	return AH_FALSE;
}

/*
 * Clear the specified key cache entry and any associated MIC entry.
 */
HAL_BOOL
ar5212ResetKeyCacheEntry(struct ath_hal *ah, u_int16_t entry)
{
	u_int32_t keyType;

	if (entry >= AH_PRIVATE(ah)->ah_caps.halKeyCacheSize) {
		HDPRINTF(ah, HAL_DBG_KEYCACHE, "%s: entry %u out of range\n", __func__, entry);
		return AH_FALSE;
	}
	keyType = OS_REG_READ(ah, AR_KEYTABLE_TYPE(entry));

	/* XXX why not clear key type/valid bit first? */
	OS_KC_WRITE(ah, AR_KEYTABLE_KEY0(entry), 0);
	OS_KC_WRITE(ah, AR_KEYTABLE_KEY1(entry), 0);
	OS_KC_WRITE(ah, AR_KEYTABLE_KEY2(entry), 0);
	OS_KC_WRITE(ah, AR_KEYTABLE_KEY3(entry), 0);
	OS_KC_WRITE(ah, AR_KEYTABLE_KEY4(entry), 0);
	OS_KC_WRITE(ah, AR_KEYTABLE_TYPE(entry), AR_KEYTABLE_TYPE_CLR);
	OS_KC_WRITE(ah, AR_KEYTABLE_MAC0(entry), 0);
	OS_KC_WRITE(ah, AR_KEYTABLE_MAC1(entry), 0);
	if (keyType == AR_KEYTABLE_TYPE_TKIP && IS_MIC_ENABLED(ah)) {
		u_int16_t micentry = entry+64;	/* MIC goes at slot+64 */

		HALASSERT(micentry < AH_PRIVATE(ah)->ah_caps.halKeyCacheSize);
		OS_KC_WRITE(ah, AR_KEYTABLE_KEY0(micentry), 0);
		OS_KC_WRITE(ah, AR_KEYTABLE_KEY1(micentry), 0);
		OS_KC_WRITE(ah, AR_KEYTABLE_KEY2(micentry), 0);
		OS_KC_WRITE(ah, AR_KEYTABLE_KEY3(micentry), 0);
		/* NB: key type and MAC are known to be ok */
	}
	return AH_TRUE;
}

/*
 * Sets the mac part of the specified key cache entry (and any
 * associated MIC entry) and mark them valid.
 */
HAL_BOOL
ar5212SetKeyCacheEntryMac(struct ath_hal *ah, u_int16_t entry, const u_int8_t *mac)
{
	u_int32_t macHi, macLo;
        u_int32_t validBit = AR_KEYTABLE_VALID;

	if (entry >= AH_PRIVATE(ah)->ah_caps.halKeyCacheSize) {
		HDPRINTF(ah, HAL_DBG_KEYCACHE, "%s: entry %u out of range\n", __func__, entry);
		return AH_FALSE;
	}
	/*
	 * Set MAC address -- shifted right by 1.  MacLo is
	 * the 4 MSBs, and MacHi is the 2 LSBs.
	 */
	if (mac != AH_NULL) {
		/*
		 *  If upper layers have requested mcast MACaddr lookup, then
		 *  signify this to the hw by setting the (poorly named) validBit
		 *  to 0.  Yes, really 0. The hardware specs, pcu_registers.txt, is
         *  has incorrectly named ValidBit. It should be called "Unicast".
         *  When the Key Cache entry is to decrypt Unicast frames, then this
         *  bit should be '1'; for multicast and broadcast frames, this bit is '0'.
		 */
		if (mac[0] & 0x01) {
			validBit = 0;
		}

		macHi = (mac[5] << 8) | mac[4];
		macLo = (mac[3] << 24)| (mac[2] << 16)
                    | (mac[1] << 8) | mac[0];
		macLo >>= 1;        /* Note that the bit 0 is shifted out. This bit was used to
                             * indicate that this is a multicast key cache. */
		macLo |= (macHi & 1) << 31;	/* carry */
		macHi >>= 1;

	} else {
		macLo = macHi = 0;
	}

	OS_KC_WRITE(ah, AR_KEYTABLE_MAC0(entry), macLo);
	OS_KC_WRITE(ah, AR_KEYTABLE_MAC1(entry), macHi | validBit);
	return AH_TRUE;
}

/*
 * Gets the contents of the specified key cache entry.
 */
HAL_BOOL
ar5212PrintKeyCache(struct ath_hal *ah)
{
#ifdef AH_DEBUG
    const HAL_CAPABILITIES *pCap = &AH_PRIVATE(ah)->ah_caps;
    u_int32_t key0, key1, key2, key3, key4;
    u_int32_t macHi, macLo;
    u_int16_t entry = 0;
        u_int32_t valid = 0;
    u_int32_t keyType;

    ath_hal_printf(ah, "Slot   Key\t\t\t          Valid  Type  Mac  \n");

    for ( entry = 0 ; entry < pCap->halKeyCacheSize; entry++) {

    key0 = OS_REG_READ(ah, AR_KEYTABLE_KEY0(entry));
    key1 = OS_REG_READ(ah, AR_KEYTABLE_KEY1(entry));
    key2 = OS_REG_READ(ah, AR_KEYTABLE_KEY2(entry));
    key3 = OS_REG_READ(ah, AR_KEYTABLE_KEY3(entry));
    key4 = OS_REG_READ(ah, AR_KEYTABLE_KEY4(entry));

    keyType = OS_REG_READ(ah, AR_KEYTABLE_TYPE(entry));

    macLo = OS_REG_READ(ah, AR_KEYTABLE_MAC0(entry));
    macHi = OS_REG_READ(ah, AR_KEYTABLE_MAC1(entry));

    if (macHi & AR_KEYTABLE_VALID)
      valid = 1;
    else
      valid = 0;

    if ( (macHi != 0) && (macLo != 0)) {
      macHi &= ~0x8000;
      macHi <<= 1;
      macHi |= ((macLo & (1 << 31) )) >> 31;
      macLo <<= 1;
    }

    ath_hal_printf(ah,
        "%03d    "
        "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x   "
        "%02d     %02d    %02x:%02x:%02x:%02x:%02x:%02x \n",
        entry,
        (key0 << 24) >> 24, (key0 << 16) >> 24, (key0 << 8) >> 24, key0 >> 24,
        (key1 << 24) >> 24, (key1 << 16) >> 24, //(key1 << 8) >> 24, key1 >> 24,
        (key2 << 24) >> 24, (key2 << 16) >> 24, (key2 << 8) >> 24, key2 >> 24,
        (key3 << 24) >> 24, (key3 << 16) >> 24, //(key3 << 8) >> 24, key3 >> 24,
        (key4 << 24) >> 24, (key4 << 16) >> 24, (key4 << 8) >> 24, key4 >> 24,
        valid, keyType,
        (macLo << 24) >> 24, (macLo << 16) >> 24, (macLo << 8) >> 24,
        (macLo) >> 24, (macHi << 24) >> 24, (macHi << 16) >> 24 );
    }
#endif /* AH_DEBUG */
    return AH_TRUE;
}

/*
 * Sets the contents of the specified key cache entry
 * and any associated MIC entry.
 */
HAL_BOOL
ar5212SetKeyCacheEntry(struct ath_hal *ah, u_int16_t entry,
                       const HAL_KEYVAL *k, const u_int8_t *mac,
                       int xorKey)
{
	const HAL_CAPABILITIES *pCap = &AH_PRIVATE(ah)->ah_caps;
	u_int32_t key0, key1, key2, key3, key4;
	u_int32_t keyType;
	u_int32_t xorMask = xorKey ?
		(KEY_XOR << 24 | KEY_XOR << 16 | KEY_XOR << 8 | KEY_XOR) : 0;

	if (entry >= pCap->halKeyCacheSize) {
		HDPRINTF(ah, HAL_DBG_KEYCACHE, "%s: entry %u out of range\n", __func__, entry);
		return AH_FALSE;
	}
	switch (k->kv_type) {
	case HAL_CIPHER_AES_OCB:
		keyType = AR_KEYTABLE_TYPE_AES;
		break;
	case HAL_CIPHER_AES_CCM:
		if (!pCap->halCipherAesCcmSupport) {
			HDPRINTF(ah, HAL_DBG_KEYCACHE, "%s: AES-CCM not supported by "
				"mac rev 0x%x\n",
				__func__, AH_PRIVATE(ah)->ah_macRev);
			return AH_FALSE;
		}
		keyType = AR_KEYTABLE_TYPE_CCM;
		break;
	case HAL_CIPHER_TKIP:
		keyType = AR_KEYTABLE_TYPE_TKIP;
		if (IS_MIC_ENABLED(ah) && entry+64 >= pCap->halKeyCacheSize) {
			HDPRINTF(ah, HAL_DBG_KEYCACHE, "%s: entry %u inappropriate for TKIP\n",
				__func__, entry);
			return AH_FALSE;
		}
		break;
	case HAL_CIPHER_WEP:
		if (k->kv_len < 40 / NBBY) {
			HDPRINTF(ah, HAL_DBG_KEYCACHE, "%s: WEP key length %u too small\n",
				__func__, k->kv_len);
			return AH_FALSE;
		}
		if (k->kv_len <= 40 / NBBY)
			keyType = AR_KEYTABLE_TYPE_40;
		else if (k->kv_len <= 104 / NBBY)
			keyType = AR_KEYTABLE_TYPE_104;
		else
			keyType = AR_KEYTABLE_TYPE_128;
		break;
	case HAL_CIPHER_CLR:
		keyType = AR_KEYTABLE_TYPE_CLR;
		break;
	default:
		HDPRINTF(ah, HAL_DBG_KEYCACHE, "%s: cipher %u not supported\n",
			__func__, k->kv_type);
		return AH_FALSE;
	}

	key0 = LE_READ_4(k->kv_val+0) ^ xorMask;
	key1 = (LE_READ_2(k->kv_val+4) ^ xorMask) & 0xffff;
	key2 = LE_READ_4(k->kv_val+6) ^ xorMask;
	key3 = (LE_READ_2(k->kv_val+10) ^ xorMask) & 0xffff;
	key4 = LE_READ_4(k->kv_val+12) ^ xorMask;
	if (k->kv_len <= 104 / NBBY)
		key4 &= 0xff;

	/*
	 * Note: key cache hardware requires that each double-word
	 * pair be written in even/odd order (since the destination is
	 * a 64-bit register).  Don't reorder these writes w/o
	 * considering this!
	 */
	if (keyType == AR_KEYTABLE_TYPE_TKIP && IS_MIC_ENABLED(ah)) {
        struct ath_hal_5212 *ahp = AH5212(ah);
		u_int16_t micentry = entry+64;	/* MIC goes at slot+64 */

		/*
		 * Invalidate the encrypt/decrypt key until the MIC
		 * key is installed so pending rx frames will fail
		 * with decrypt errors rather than a MIC error.
		 */
		OS_KC_WRITE(ah, AR_KEYTABLE_KEY0(entry), ~key0);
		OS_KC_WRITE(ah, AR_KEYTABLE_KEY1(entry), ~key1);
		OS_KC_WRITE(ah, AR_KEYTABLE_KEY2(entry), key2);
		OS_KC_WRITE(ah, AR_KEYTABLE_KEY3(entry), key3);
		OS_KC_WRITE(ah, AR_KEYTABLE_KEY4(entry), key4);
		OS_KC_WRITE(ah, AR_KEYTABLE_TYPE(entry), keyType);
		(void) ar5212SetKeyCacheEntryMac(ah, entry, mac);

		/*
		 * since the AR_MISC_MODE register was written with the contents of
		 * ah_miscMode (if any) in ar5212Attach, just check ah_miscMode and
		 * save a pci read per key set.
		 */
		if (ahp->ah_miscMode & AR_MISC_MODE_MIC_NEW_LOC_ENABLE) {
			u_int32_t mic0,mic1,mic2,mic3,mic4;
			/*
			 * both RX and TX mic values can be combined into
			 * one cache slot entry.
			 *  "<pre>8*N + 800         31:0    RX Michael key 0                              \n" .
			 *  "<pre>8*N + 804         15:0    TX Michael key 0 [31:16]                      \n" .
			 *  "<pre>8*N + 808         31:0    RX Michael key 1                              \n" .
			 *  "<pre>8*N + 80C         15:0    TX Michael key 0 [15:0]                       \n" .
			 *  "<pre>8*N + 810         31:0    TX Michael key 1                              \n" .
			 *  "<pre>8*N + 814         15:0    reserved                                      \n" .
			 *  "<pre>8*N + 818         31:0    reserved                                      \n" .
			 *  "<pre>8*N + 81C         14:0    reserved                                      \n" .
			 *  "<pre>                  15      key valid == 0                                \n" .
			 */
			/* RX mic */
			mic0 = LE_READ_4(k->kv_mic+0);
			mic2 = LE_READ_4(k->kv_mic+4);
			/* TX mic */
			mic1 = LE_READ_2(k->kv_txmic+2) & 0xffff;
			mic3 = LE_READ_2(k->kv_txmic+0) & 0xffff;
			mic4 = LE_READ_4(k->kv_txmic+4);
			OS_REG_WRITE(ah, AR_KEYTABLE_KEY0(micentry), mic0);
			OS_REG_WRITE(ah, AR_KEYTABLE_KEY1(micentry), mic1);
			OS_REG_WRITE(ah, AR_KEYTABLE_KEY2(micentry), mic2);
			OS_REG_WRITE(ah, AR_KEYTABLE_KEY3(micentry), mic3);
			OS_REG_WRITE(ah, AR_KEYTABLE_KEY4(micentry), mic4);
			OS_REG_WRITE(ah, AR_KEYTABLE_TYPE(micentry),
						 AR_KEYTABLE_TYPE_CLR);

		} else {
            u_int32_t mic0, mic2;

    		mic0 = LE_READ_4(k->kv_mic+0);
            mic2 = LE_READ_4(k->kv_mic+4);
            OS_KC_WRITE(ah, AR_KEYTABLE_KEY0(micentry), mic0);
            OS_KC_WRITE(ah, AR_KEYTABLE_KEY1(micentry), 0);
            OS_KC_WRITE(ah, AR_KEYTABLE_KEY2(micentry), mic2);
            OS_KC_WRITE(ah, AR_KEYTABLE_KEY3(micentry), 0);
            OS_KC_WRITE(ah, AR_KEYTABLE_KEY4(micentry), 0);
            OS_KC_WRITE(ah, AR_KEYTABLE_TYPE(micentry),
                AR_KEYTABLE_TYPE_CLR);
        }
		/* NB: MIC key is not marked valid and has no MAC address */
		OS_KC_WRITE(ah, AR_KEYTABLE_MAC0(micentry), 0);
		OS_KC_WRITE(ah, AR_KEYTABLE_MAC1(micentry), 0);

		/* correct intentionally corrupted key */
		OS_KC_WRITE(ah, AR_KEYTABLE_KEY0(entry), key0);
		OS_KC_WRITE(ah, AR_KEYTABLE_KEY1(entry), key1);
	} else {
		OS_KC_WRITE(ah, AR_KEYTABLE_KEY0(entry), key0);
		OS_KC_WRITE(ah, AR_KEYTABLE_KEY1(entry), key1);
		OS_KC_WRITE(ah, AR_KEYTABLE_KEY2(entry), key2);
		OS_KC_WRITE(ah, AR_KEYTABLE_KEY3(entry), key3);
		OS_KC_WRITE(ah, AR_KEYTABLE_KEY4(entry), key4);
		OS_KC_WRITE(ah, AR_KEYTABLE_TYPE(entry), keyType);

		(void) ar5212SetKeyCacheEntryMac(ah, entry, mac);
	}
	return AH_TRUE;
}
/*
 * Enable the Keysearch for every subframe of an aggregate
 * Not applicable for this HAL
 */
void
ar5212_enable_keysearch_always(struct ath_hal *ah, int enable)
{
}
#endif /* AH_SUPPORT_AR5212 */
