/*
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2005 Atheros Communications, Inc.
 * Copyright (c) 2010, Atheros Communications Inc.
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

#include "ah.h"
#include "ah_internal.h"
#include "ah_devid.h"
#include "ah_eeprom.h"


#ifdef AH_SUPPORT_AR5212
extern  struct ath_hal *ar5212Attach(u_int16_t,
    HAL_ADAPTER_HANDLE, HAL_SOFTC,
    HAL_BUS_TAG, HAL_BUS_HANDLE, HAL_BUS_TYPE, 
    asf_amem_instance_handle amem_handle,
    struct hal_reg_parm *, HAL_STATUS*);
#endif
#ifdef AH_SUPPORT_AR5416
extern  struct ath_hal *ar5416Attach(u_int16_t,
    HAL_ADAPTER_HANDLE, HAL_SOFTC,
    HAL_BUS_TAG, HAL_BUS_HANDLE, HAL_BUS_TYPE,
    asf_amem_instance_handle amem_handle,
    struct hal_reg_parm *, HAL_STATUS*);
#endif

#ifdef AH_SUPPORT_AR9300
extern  struct ath_hal *ar9300Attach(u_int16_t,
    HAL_ADAPTER_HANDLE, HAL_SOFTC,
    HAL_BUS_TAG, HAL_BUS_HANDLE, HAL_BUS_TYPE,
    asf_amem_instance_handle amem_handle,
    struct hal_reg_parm *, HAL_STATUS*);
#endif

#include "version.h"
char ath_hal_version[] = ATH_HAL_VERSION;
const char* ath_hal_buildopts[] = {
#ifdef AH_SUPPORT_AR5210
    "AR5210",
#endif
#ifdef AH_SUPPORT_AR5211
    "AR5211",
#endif
#ifdef AH_SUPPORT_AR5212
    "AR5212",
#endif
#ifdef AH_SUPPORT_AR5312
    "AR5312",
#endif
#ifdef AH_SUPPORT_AR5416
    "AR5416",
#endif
#ifdef AH_SUPPORT_AR9300
    "AR9380",
#endif
#ifdef AH_SUPPORT_5111
    "RF5111",
#endif
#ifdef AH_SUPPORT_5112
    "RF5112",
#endif
#ifdef AH_SUPPORT_2413
    "RF2413",
#endif
#ifdef AH_SUPPORT_5413
    "RF5413",
#endif
#ifdef AH_SUPPORT_2316
    "RF2316",
#endif
#ifdef AH_SUPPORT_2317
    "RF2317",
#endif
#ifdef AH_DEBUG
    "DEBUG",
#endif
#ifdef AH_ASSERT
    "ASSERT",
#endif
#ifdef AH_DEBUG_ALQ
    "DEBUG_ALQ",
#endif
#ifdef AH_REGOPS_FUNC
    "REGOPS_FUNC",
#endif
#ifdef AH_ENABLE_TX_TPC
    "ENABLE_TX_TPC",
#endif
#ifdef AH_PRIVATE_DIAG
    "PRIVATE_DIAG",
#endif
#ifdef AH_SUPPORT_WRITE_EEPROM
    "WRITE_EEPROM",
#endif
#ifdef AH_WRITE_REGDOMAIN
    "WRITE_REGDOMAIN",
#endif
#ifdef AH_NEED_DESC_SWAP
    "TX_DESC_SWAP",
#endif
#ifdef AH_NEED_TX_DATA_SWAP
    "TX_DATA_SWAP",
#endif
#ifdef AH_NEED_RX_DATA_SWAP
    "RX_DATA_SWAP",
#endif
#ifdef AR5416_ISR_READ_CLEAR_SUPPORT
    "AR5416_ISR_READ_CLEAR_SUPPORT",
#endif
#ifdef AH_SUPPORT_DFS
    "DFS",
   #if AH_RADAR_CALIBRATE >= 3
    "RADAR_CALIBRATE (detailed)",
   #elif AH_RADAR_CALIBRATE >= 2
    "RADAR_CALIBRATE (minimal)",
   #elif AH_RADAR_CALIBRATE > 0
    "RADAR_CALIBRATE",
   #endif
   #endif /* AH_SUPPORT_DFS */
#ifdef AH_SUPPORT_XR
    "XR",
#endif
#ifdef AH_SUPPORT_WRITE_REG
    "WRITE_REG",
#endif
#ifdef AH_DISABLE_WME
    "DISABLE_WME",
#endif

#ifdef AH_SUPPORT_11D
    "11D",
#endif
    AH_NULL
};

static const char*
ath_hal_devname(u_int16_t devid)
{
    switch (devid) {
    case AR5210_PROD:
    case AR5210_DEFAULT:
        return "Atheros 5210";

    case AR5211_DEVID:
    case AR5311_DEVID:
    case AR5211_DEFAULT:
        return "Atheros 5211";
    case AR5211_FPGA11B:
        return "Atheros 5211 (FPGA)";

    case AR5212_FPGA:
        return "Atheros 5212 (FPGA)";
    case AR5212_AR5312_REV2:
    case AR5212_AR5312_REV7:
        return "Atheros 5312 WiSoC";
    case AR5212_AR2315_REV6:
    case AR5212_AR2315_REV7:
        return "Atheros 2315 WiSoC";
    case AR5212_AR2317_REV1:
    case AR5212_AR2317_REV2:
        return "Atheros 2317 WiSoC";
    case AR5212_AR2313_REV8:
        return "Atheros 2313 WiSoC";
	case AR5212_AR5424:
	case AR5212_DEVID_FF19:
		return "Atheros 5424";
    case AR5212_DEVID:
    case AR5212_DEVID_IBM:
    case AR5212_AR2413:
    case AR5212_AR5413:
    case AR5212_DEFAULT:
        return "Atheros 5212";
    case AR5212_AR2417:
        return "Atheros 5212/2417";
#ifdef AH_SUPPORT_AR5416
    case AR5416_DEVID_PCI:
    case AR5416_DEVID_PCIE:
        return "Atheros 5416";
    case AR5416_DEVID_AR9160_PCI:
        return "Atheros 9160";
	case AR5416_AR9100_DEVID:
		return "Atheros AR9100 WiSoC";
    case AR5416_DEVID_AR9280_PCI:
    case AR5416_DEVID_AR9280_PCIE:
        return "Atheros 9280";
	case AR5416_DEVID_AR9285_PCIE:
        return "Atheros 9285";
    case AR5416_DEVID_AR9285G_PCIE:
        return "Atheros 9285G";
	case AR5416_DEVID_AR9287_PCI:
	case AR5416_DEVID_AR9287_PCIE:
        return "Atheros 9287";

#endif
	case AR9300_DEVID_AR9380_PCIE:
        return "Atheros 9380";
	case AR9300_DEVID_AR9340:
        return "Atheros 9340";
	case AR9300_DEVID_AR9485_PCIE:
        return "Atheros 9485";
    case AR9300_DEVID_AR9580_PCIE:
        return "Atheros 9580";
    }
    return AH_NULL;
}

const char* __ahdecl
ath_hal_probe(u_int16_t vendorid, u_int16_t devid)
{
    return (vendorid == ATHEROS_VENDOR_ID ||
        vendorid == ATHEROS_3COM_VENDOR_ID ||
        vendorid == ATHEROS_3COM2_VENDOR_ID ?
            ath_hal_devname(devid) : 0);
}

/*
 * Attach detects device chip revisions, initializes the hwLayer
 * function list, reads EEPROM information,
 * selects reset vectors, and performs a short self test.
 * Any failures will return an error that should cause a hardware
 * disable.
 */
struct ath_hal* __ahdecl
ath_hal_attach(u_int16_t devid, HAL_ADAPTER_HANDLE osdev, HAL_SOFTC sc,
    HAL_BUS_TAG st, HAL_BUS_HANDLE sh, HAL_BUS_TYPE bustype,
    asf_amem_instance_handle amem_handle,
    struct hal_reg_parm *hal_conf_parm, HAL_STATUS *error)
{
    struct ath_hal *ah=AH_NULL;

    switch (devid) {
#ifdef AH_SUPPORT_AR5212
    case AR5212_DEVID_IBM:
    case AR5212_AR2413:
    case AR5212_AR5413:
    case AR5212_AR5424:
    case AR5212_DEVID_FF19: /* XXX PCI Express extra */
        devid = AR5212_DEVID;
        /* fall thru... */
    case AR5212_AR2417:
    case AR5212_DEVID:
    case AR5212_FPGA:
    case AR5212_DEFAULT:
        ah = ar5212Attach(devid, osdev, sc, st, sh, bustype, amem_handle,
                          hal_conf_parm, error);
        break;
#endif

#ifdef AH_SUPPORT_AR5416
	case AR5416_DEVID_PCI:
	case AR5416_DEVID_PCIE:
	case AR5416_DEVID_AR9160_PCI:
	case AR5416_AR9100_DEVID:
    case AR5416_DEVID_AR9280_PCI:
    case AR5416_DEVID_AR9280_PCIE:
	case AR5416_DEVID_AR9285_PCIE:
    case AR5416_DEVID_AR9285G_PCIE:
	case AR5416_DEVID_AR9287_PCI:
	case AR5416_DEVID_AR9287_PCIE:
        ah = ar5416Attach(devid, osdev, sc, st, sh, bustype, amem_handle,
                          hal_conf_parm, error);
                break;
#endif
#ifdef AH_SUPPORT_AR9300
    case AR9300_DEVID_AR9380_PCIE:
    case AR9300_DEVID_AR9340:
    case AR9300_DEVID_AR9485_PCIE:
    case AR9300_DEVID_AR9580_PCIE:
#ifdef AR9300_EMULATION
    case AR9300_DEVID_EMU_PCIE:
#endif
        ah = ar9300Attach(devid, osdev, sc, st, sh, bustype, amem_handle,
                          hal_conf_parm, error);
        break;
#endif
    default:
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "devid=0x%x not supported.\n",devid);
        ah = AH_NULL;
        *error = HAL_ENXIO;
        break;
    }
    if (ah != AH_NULL) {
        /* copy back private state to public area */
        ah->ah_devid = AH_PRIVATE(ah)->ah_devid;
        ah->ah_subvendorid = AH_PRIVATE(ah)->ah_subvendorid;
        ah->ah_macVersion = AH_PRIVATE(ah)->ah_macVersion;
        ah->ah_macRev = AH_PRIVATE(ah)->ah_macRev;
        ah->ah_phyRev = AH_PRIVATE(ah)->ah_phyRev;
        ah->ah_analog5GhzRev = AH_PRIVATE(ah)->ah_analog5GhzRev;
        ah->ah_analog2GhzRev = AH_PRIVATE(ah)->ah_analog2GhzRev;
#if ICHAN_WAR_SYNCH
        ah->ah_ichan_set = AH_FALSE;
        spin_lock_init(&ah->ah_ichan_lock);
#endif
    }
    return ah;
}

 /*
  * Set the vendor ID for the driver.  This is used by the
  * regdomain to make vendor specific changes.
  */
HAL_BOOL __ahdecl
 ath_hal_setvendor(struct ath_hal *ah, u_int32_t vendor)
 {
    AH_PRIVATE(ah)->ah_vendor = vendor;
    return AH_TRUE;
 }


/*
 * Poll the register looking for a specific value.
 * Waits up to timeout (usec).
 */
HAL_BOOL
ath_hal_wait(struct ath_hal *ah, u_int reg, u_int32_t mask, u_int32_t val,
             u_int32_t timeout)
{
/* 
 * WDK documentation states execution should not be stalled for more than 
 * 10us at a time.
 */
#define AH_TIME_QUANTUM        10
    int i;

    HALASSERT(timeout >= AH_TIME_QUANTUM);

    for (i = 0; i < (timeout / AH_TIME_QUANTUM); i++) {
        if ((OS_REG_READ(ah, reg) & mask) == val)
            return AH_TRUE;
#if !defined(ART_BUILD) || !defined(_WINDOWS)
        OS_DELAY(AH_TIME_QUANTUM);
#endif
    }
    HDPRINTF(ah, HAL_DBG_PHY_IO,
             "%s: timeout (%d us) on reg 0x%x: 0x%08x & 0x%08x != 0x%08x\n",
            __func__,
            timeout,
            reg, 
            OS_REG_READ(ah, reg), mask, val);
    return AH_FALSE;
#undef AH_TIME_QUANTUM
}

/*
 * Reverse the bits starting at the low bit for a value of
 * bit_count in size
 */
u_int32_t
ath_hal_reverseBits(u_int32_t val, u_int32_t n)
{
    u_int32_t retval;
    int i;

    for (i = 0, retval = 0; i < n; i++) {
        retval = (retval << 1) | (val & 1);
        val >>= 1;
    }
    return retval;
}

/*
 * Compute the time to transmit a frame of length frameLen bytes
 * using the specified rate, phy, and short preamble setting.
 */
u_int16_t __ahdecl
ath_hal_computetxtime(struct ath_hal *ah,
    const HAL_RATE_TABLE *rates, u_int32_t frameLen, u_int16_t rateix,
    HAL_BOOL shortPreamble)
{
    u_int32_t bitsPerSymbol, numBits, numSymbols, phyTime, txTime;
    u_int32_t kbps;

    kbps = rates->info[rateix].rateKbps;
    /*
     * index can be invalid duting dynamic Turbo transitions. 
     */
    if(kbps == 0) return 0;
    switch (rates->info[rateix].phy) {

    case IEEE80211_T_CCK:
#define CCK_SIFS_TIME        10
#define CCK_PREAMBLE_BITS   144
#define CCK_PLCP_BITS        48
        phyTime     = CCK_PREAMBLE_BITS + CCK_PLCP_BITS;
        if (shortPreamble && rates->info[rateix].shortPreamble)
            phyTime >>= 1;
        numBits     = frameLen << 3;
        txTime      = phyTime
                + ((numBits * 1000)/kbps);
        break;
#undef CCK_SIFS_TIME
#undef CCK_PREAMBLE_BITS
#undef CCK_PLCP_BITS

    case IEEE80211_T_OFDM:
/* #define OFDM_SIFS_TIME        16 earlier value see EV# 94662*/
#define OFDM_SIFS_TIME        6
#define OFDM_PREAMBLE_TIME    20
#define OFDM_PLCP_BITS        22
#define OFDM_SYMBOL_TIME       4

#define OFDM_SIFS_TIME_HALF 32
#define OFDM_PREAMBLE_TIME_HALF 40
#define OFDM_PLCP_BITS_HALF 22
#define OFDM_SYMBOL_TIME_HALF   8

#define OFDM_SIFS_TIME_QUARTER      64
#define OFDM_PREAMBLE_TIME_QUARTER  80
#define OFDM_PLCP_BITS_QUARTER      22
#define OFDM_SYMBOL_TIME_QUARTER    16

        if (AH_PRIVATE(ah)->ah_curchan && 
            IS_CHAN_QUARTER_RATE(AH_PRIVATE(ah)->ah_curchan)) {
            bitsPerSymbol   = (kbps * OFDM_SYMBOL_TIME_QUARTER) / 1000;
            HALASSERT(bitsPerSymbol != 0);

            numBits     = OFDM_PLCP_BITS + (frameLen << 3);
            numSymbols  = howmany(numBits, bitsPerSymbol);
            txTime      = OFDM_SIFS_TIME_QUARTER 
                        + OFDM_PREAMBLE_TIME_QUARTER
                    + (numSymbols * OFDM_SYMBOL_TIME_QUARTER);
        } else if (AH_PRIVATE(ah)->ah_curchan &&
                IS_CHAN_HALF_RATE(AH_PRIVATE(ah)->ah_curchan)) {
            bitsPerSymbol   = (kbps * OFDM_SYMBOL_TIME_HALF) / 1000;
            HALASSERT(bitsPerSymbol != 0);

            numBits     = OFDM_PLCP_BITS + (frameLen << 3);
            numSymbols  = howmany(numBits, bitsPerSymbol);
            txTime      = OFDM_SIFS_TIME_HALF + 
                        OFDM_PREAMBLE_TIME_HALF
                    + (numSymbols * OFDM_SYMBOL_TIME_HALF);
        } else { /* full rate channel */
            bitsPerSymbol   = (kbps * OFDM_SYMBOL_TIME) / 1000;
            HALASSERT(bitsPerSymbol != 0);

            numBits     = OFDM_PLCP_BITS + (frameLen << 3);
            numSymbols  = howmany(numBits, bitsPerSymbol);
            txTime      = OFDM_SIFS_TIME + OFDM_PREAMBLE_TIME
                    + (numSymbols * OFDM_SYMBOL_TIME);
        }
        break;

#undef OFDM_SIFS_TIME
#undef OFDM_PREAMBLE_TIME
#undef OFDM_PLCP_BITS
#undef OFDM_SYMBOL_TIME


    case IEEE80211_T_TURBO:
#define TURBO_SIFS_TIME         8
#define TURBO_PREAMBLE_TIME    14
#define TURBO_PLCP_BITS        22
#define TURBO_SYMBOL_TIME       4
        /* we still save OFDM rates in kbps - so double them */
        bitsPerSymbol = ((kbps << 1) * TURBO_SYMBOL_TIME) / 1000;
        HALASSERT(bitsPerSymbol != 0);

        numBits       = TURBO_PLCP_BITS + (frameLen << 3);
        numSymbols    = howmany(numBits, bitsPerSymbol);
        txTime        = TURBO_SIFS_TIME + TURBO_PREAMBLE_TIME
                  + (numSymbols * TURBO_SYMBOL_TIME);
        break;
#undef TURBO_SIFS_TIME
#undef TURBO_PREAMBLE_TIME
#undef TURBO_PLCP_BITS
#undef TURBO_SYMBOL_TIME

    case ATHEROS_T_XR:
#define XR_SIFS_TIME            16
#define XR_PREAMBLE_TIME(_kpbs) (((_kpbs) < 1000) ? 173 : 76)
#define XR_PLCP_BITS            22
#define XR_SYMBOL_TIME           4
        bitsPerSymbol = (kbps * XR_SYMBOL_TIME) / 1000;
        HALASSERT(bitsPerSymbol != 0);

        numBits       = XR_PLCP_BITS + (frameLen << 3);
        numSymbols    = howmany(numBits, bitsPerSymbol);
        txTime        = XR_SIFS_TIME + XR_PREAMBLE_TIME(kbps)
                   + (numSymbols * XR_SYMBOL_TIME);
        break;
#undef XR_SIFS_TIME
#undef XR_PREAMBLE_TIME
#undef XR_PLCP_BITS
#undef XR_SYMBOL_TIME

    default:
        HDPRINTF(ah, HAL_DBG_PHY_IO, "%s: unknown phy %u (rate ix %u)\n",
            __func__, rates->info[rateix].phy, rateix);
        txTime = 0;
        break;
    }
    return txTime;
}

/* The following 3 functions have a lot of overlap.
   If possible, they should be combined into a single function.
   DCD 4/2/09
 */

WIRELESS_MODE
ath_hal_chan2wmode(struct ath_hal *ah, const HAL_CHANNEL *chan)
{
    if (IS_CHAN_CCK(chan))
        return WIRELESS_MODE_11b;
    if (IS_CHAN_G(chan))
        return WIRELESS_MODE_11g;
    if (IS_CHAN_108G(chan))
        return WIRELESS_MODE_108g;
    if (IS_CHAN_TURBO(chan))
        return WIRELESS_MODE_TURBO;
    if (IS_CHAN_XR(chan))
        return WIRELESS_MODE_XR;
    return WIRELESS_MODE_11a;
}


/* Note: WIRELESS_MODE defined in ah_internal.h is used here 
         not that enumerated in ath_dev.h */

WIRELESS_MODE
ath_hal_chan2htwmode(struct ath_hal *ah, const HAL_CHANNEL *chan)
{
    if (IS_CHAN_CCK(chan))
        return WIRELESS_MODE_11b;
    /* Tag HT channels with _NG / _NA, instead of falling through to _11g/_11a */
    if (IS_CHAN_HT(chan) && IS_CHAN_G(chan))
        return WIRELESS_MODE_11NG;
    if (IS_CHAN_HT(chan) && IS_CHAN_A(chan))
        return WIRELESS_MODE_11NA;
    if (IS_CHAN_G(chan))
        return WIRELESS_MODE_11g;
    if (IS_CHAN_108G(chan))
        return WIRELESS_MODE_108g;
    if (IS_CHAN_TURBO(chan))
        return WIRELESS_MODE_TURBO;
    if (IS_CHAN_XR(chan))
        return WIRELESS_MODE_XR;
    return WIRELESS_MODE_11a;
}

u_int
ath_hal_get_curmode(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{
    /*
     * Pick a default mode at bootup. A channel change is inevitable.
     */
    if (!chan) return HAL_MODE_11NG_HT20;

    /* check for NA_HT before plain A, since IS_CHAN_A includes NA_HT */
    if (IS_CHAN_NA_20(chan))      return HAL_MODE_11NA_HT20;
    if (IS_CHAN_NA_40PLUS(chan))  return HAL_MODE_11NA_HT40PLUS;
    if (IS_CHAN_NA_40MINUS(chan)) return HAL_MODE_11NA_HT40MINUS;
    if (IS_CHAN_A(chan))          return HAL_MODE_11A;

    /* check for NG_HT before plain G, since IS_CHAN_G includes NG_HT */
    if (IS_CHAN_NG_20(chan))      return HAL_MODE_11NG_HT20;
    if (IS_CHAN_NG_40PLUS(chan))  return HAL_MODE_11NG_HT40PLUS;
    if (IS_CHAN_NG_40MINUS(chan)) return HAL_MODE_11NG_HT40MINUS;
    if (IS_CHAN_G(chan))          return HAL_MODE_11G;

    if (IS_CHAN_B(chan))          return HAL_MODE_11B;

    /*
     * TODO: Current callers don't care about SuperX.
     */
    HALASSERT(0);
    return HAL_MODE_11NG_HT20;
}

/*
 * Convert GHz frequency to IEEE channel number.
 */
u_int __ahdecl
ath_hal_mhz2ieee(struct ath_hal *ah, u_int freq, u_int flags)
{
    if (flags & CHANNEL_2GHZ) { /* 2GHz band */
        if (freq == 2484)
            return 14;
        if (freq < 2484)
            return (freq - 2407) / 5;
        else
            return 15 + ((freq - 2512) / 20);
    } else if (flags & CHANNEL_5GHZ) {/* 5Ghz band */
        if (ath_hal_ispublicsafetysku(ah) && 
            IS_CHAN_IN_PUBLIC_SAFETY_BAND(freq)) {
            return ((freq * 10) + 
                (((freq % 5) == 2) ? 5 : 0) - 49400) / 5;
        } else if ((flags & CHANNEL_A) && (freq <= 5000)) {
            return (freq - 4000) / 5;
        } else {
            return (freq - 5000) / 5;
        }
    } else {            /* either, guess */
        if (freq == 2484)
            return 14;
        if (freq < 2484)
            return (freq - 2407) / 5;
        if (freq < 5000) {
            if (ath_hal_ispublicsafetysku(ah) 
                && IS_CHAN_IN_PUBLIC_SAFETY_BAND(freq)) {
                return ((freq * 10) +   
                    (((freq % 5) == 2) ? 5 : 0) - 49400)/5;
            } else if (freq > 4900) {
                return (freq - 4000) / 5;
            } else {
                return 15 + ((freq - 2512) / 20);
            }
        }
        return (freq - 5000) / 5;
    }
}

/*
 * Convert between microseconds and core system clocks.
 */
                                     
/* PG: Added support for determining different clock rates
       for 11NGHT20, 11NAHT20, 11NGHT40PLUS/MINUS, 11NAHT40PLUS/MINUS */

                                          /* 11a Turbo  11b  11g  108g  XR  NA  NG */
static const u_int8_t CLOCK_RATE[]  =      { 40,  80,   44,  44,   88,  40, 40, 44 };
static const u_int8_t FAST_CLOCK_RATE[]  = { 44,  80,   44,  44,   88,  40, 44, 44 };

#define ath_hal_is_fast_clock_en(_ah)   ((*(_ah)->ah_isFastClockEnabled)((ah)))

u_int
ath_hal_mac_clks(struct ath_hal *ah, u_int usecs)
{
    const HAL_CHANNEL *c = (const HAL_CHANNEL *) AH_PRIVATE(ah)->ah_curchan;

    /* NB: ah_curchan may be null when called attach time */
    if (c != AH_NULL) {
        if (AH_TRUE == ath_hal_is_fast_clock_en(ah)) {
            if (IS_CHAN_HALF_RATE(c))
                return usecs * FAST_CLOCK_RATE[ath_hal_chan2wmode(ah, c)] / 2;
            else if (IS_CHAN_QUARTER_RATE(c))
                return usecs * FAST_CLOCK_RATE[ath_hal_chan2wmode(ah, c)] / 4;
            else
                return usecs * FAST_CLOCK_RATE[ath_hal_chan2wmode(ah, c)];
        } else {
            if (IS_CHAN_HALF_RATE(c))
                return usecs * CLOCK_RATE[ath_hal_chan2wmode(ah, c)] / 2;
            else if (IS_CHAN_QUARTER_RATE(c))
                return usecs * CLOCK_RATE[ath_hal_chan2wmode(ah, c)] / 4;
            else
                return usecs * CLOCK_RATE[ath_hal_chan2wmode(ah, c)];
        }
    } else
        return usecs * CLOCK_RATE[WIRELESS_MODE_11b];
}

u_int
ath_hal_mac_usec(struct ath_hal *ah, u_int clks)
{
    const HAL_CHANNEL *c = (const HAL_CHANNEL *) AH_PRIVATE(ah)->ah_curchan;

    /* NB: ah_curchan may be null when called attach time */
    if (c != AH_NULL) {
        if (AH_TRUE == ath_hal_is_fast_clock_en(ah)) {
            return clks / FAST_CLOCK_RATE[ath_hal_chan2wmode(ah, c)];
        } else {
            return clks / CLOCK_RATE[ath_hal_chan2wmode(ah, c)];
        }
    } else
        return clks / CLOCK_RATE[WIRELESS_MODE_11b];
}

u_int8_t
ath_hal_chan_2_clockrateMHz(struct ath_hal *ah)
{
        const HAL_CHANNEL *c = (const HAL_CHANNEL *) AH_PRIVATE(ah)->ah_curchan;
        u_int8_t clockrateMHz;

        if (AH_TRUE == ath_hal_is_fast_clock_en(ah)) {
            clockrateMHz = FAST_CLOCK_RATE[ath_hal_chan2htwmode(ah,c)];
        } else {
            clockrateMHz = CLOCK_RATE[ath_hal_chan2htwmode(ah,c)];
        }

        if (c && IS_CHAN_HT40(c))
            return (clockrateMHz * 2);
        else
            return (clockrateMHz);
}

/*
 * Setup a h/w rate table's reverse lookup table and
 * fill in ack durations.  This routine is called for
 * each rate table returned through the ah_getRateTable
 * method.  The reverse lookup tables are assumed to be
 * initialized to zero (or at least the first entry).
 * We use this as a key that indicates whether or not
 * we've previously setup the reverse lookup table.
 *
 * XXX not reentrant, but shouldn't matter
 */
void
ath_hal_setupratetable(struct ath_hal *ah, HAL_RATE_TABLE *rt)
{
    int i;

    if (rt->rateCodeToIndex[0] != 0)    /* already setup */
        return;
    for (i = 0; i < 256; i++)
        rt->rateCodeToIndex[i] = (u_int8_t) -1;
    for (i = 0; i < rt->rateCount; i++) {
        u_int8_t code = rt->info[i].rateCode;
        u_int8_t cix = rt->info[i].controlRate;

        rt->rateCodeToIndex[code] = i;
        rt->rateCodeToIndex[code | rt->info[i].shortPreamble] = i;
        /*
         * XXX for 11g the control rate to use for 5.5 and 11 Mb/s
         *     depends on whether they are marked as basic rates;
         *     the static tables are setup with an 11b-compatible
         *     2Mb/s rate which will work but is suboptimal
         */
        rt->info[i].lpAckDuration = ath_hal_computetxtime(ah, rt,
            WLAN_CTRL_FRAME_SIZE, cix, AH_FALSE);
        rt->info[i].spAckDuration = ath_hal_computetxtime(ah, rt,
            WLAN_CTRL_FRAME_SIZE, cix, AH_TRUE);
    }
}

HAL_STATUS
ath_hal_getcapability(struct ath_hal *ah, HAL_CAPABILITY_TYPE type,
    u_int32_t capability, u_int32_t *result)
{
    const HAL_CAPABILITIES *pCap = &AH_PRIVATE(ah)->ah_caps;

    switch (type) {
    case HAL_CAP_REG_DMN:       /* regulatory domain */
        *result = AH_PRIVATE(ah)->ah_currentRD;
        return HAL_OK;
    case HAL_CAP_DFS_DMN:
        *result = AH_PRIVATE(ah)->ah_dfsDomain;
        return HAL_OK;
    case HAL_CAP_COMBINED_RADAR_RSSI:
         return (pCap->halUseCombinedRadarRssi ? HAL_OK : HAL_ENOTSUPP);
    case HAL_CAP_EXT_CHAN_DFS:
         return (pCap->halExtChanDfsSupport ? HAL_OK : HAL_ENOTSUPP);
    case HAL_CAP_CIPHER:        /* cipher handled in hardware */
    case HAL_CAP_TKIP_MIC:      /* handle TKIP MIC in hardware */
        return HAL_ENOTSUPP;
    case HAL_CAP_TKIP_SPLIT:    /* hardware TKIP uses split keys */
        return HAL_ENOTSUPP;
    case HAL_CAP_WME_TKIPMIC:   /* hardware can do TKIP MIC when WMM is turned on */
        return HAL_ENOTSUPP;
    case HAL_CAP_DIVERSITY:     /* hardware supports fast diversity */
        return HAL_ENOTSUPP;
    case HAL_CAP_KEYCACHE_SIZE: /* hardware key cache size */
        *result =  pCap->halKeyCacheSize;
        return HAL_OK;
    case HAL_CAP_NUM_TXQUEUES:  /* number of hardware tx queues */
        *result = pCap->halTotalQueues;
        return HAL_OK;
    case HAL_CAP_VEOL:      /* hardware supports virtual EOL */
        return pCap->halVEOLSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_PSPOLL:        /* hardware PS-Poll support works */
        return pCap->halPSPollBroken ? HAL_ENOTSUPP : HAL_OK;
    case HAL_CAP_COMPRESSION:
        return pCap->halCompressSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_BURST:
        return pCap->halBurstSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_FASTFRAME:
        return pCap->halFastFramesSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_DIAG:      /* hardware diagnostic support */
        *result = AH_PRIVATE(ah)->ah_diagreg;
        return HAL_OK;
    case HAL_CAP_TXPOW:     /* global tx power limit  */
        switch (capability) {
        case 0:         /* facility is supported */
            return HAL_OK;
        case 1:         /* current limit */
            *result = AH_PRIVATE(ah)->ah_powerLimit;
            return HAL_OK;
        case 2:         /* current max tx power */
            *result = AH_PRIVATE(ah)->ah_maxPowerLevel;
            return HAL_OK;
        case 3:         /* scale factor */
            *result = AH_PRIVATE(ah)->ah_tpScale;
            return HAL_OK;
        case 4:         /* extra power for certain rates */
            *result = AH_PRIVATE(ah)->ah_extra_txpow;
            return HAL_OK;
        }
        return HAL_ENOTSUPP;
    case HAL_CAP_BSSIDMASK:     /* hardware supports bssid mask */
        return pCap->halBssIdMaskSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_MCAST_KEYSRCH: /* multicast frame keycache search */
        return pCap->halMcastKeySrchSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_TSF_ADJUST:    /* hardware has beacon tsf adjust */
        return HAL_ENOTSUPP;
    case HAL_CAP_XR:
        return pCap->halXrSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_CHAN_HALFRATE:
        return pCap->halChanHalfRate ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_CHAN_QUARTERRATE:
        return pCap->halChanQuarterRate ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_RFSILENT:      /* rfsilent support  */
        switch (capability) {
        case 0:         /* facility is supported */
            return pCap->halRfSilentSupport ? HAL_OK : HAL_ENOTSUPP;
        case 1:         /* current setting */
            return AH_PRIVATE(ah)->ah_rfkillEnabled ?
                HAL_OK : HAL_ENOTSUPP;
        case 2:         /* rfsilent config */
            /*
             * lower 16 bits for GPIO pin,
             * and upper 16 bits for GPIO polarity
             */
            *result = MS(AH_PRIVATE(ah)->ah_rfsilent, AR_EEPROM_RFSILENT_POLARITY);
            *result <<= 16;
            *result |= MS(AH_PRIVATE(ah)->ah_rfsilent, AR_EEPROM_RFSILENT_GPIO_SEL);
            return HAL_OK;
        case 3:         /* use GPIO INT to detect rfkill switch */
        default:
            return HAL_ENOTSUPP;
        }
    case HAL_CAP_11D:
#ifdef AH_SUPPORT_11D
        return HAL_OK;
#else
        return HAL_ENOTSUPP;
#endif

    case HAL_CAP_PCIE_PS:
        switch (capability) {
        case 0:
            return AH_PRIVATE(ah)->ah_isPciExpress ? HAL_OK : HAL_ENOTSUPP;
        case 1:
            return (AH_PRIVATE(ah)->ah_config.ath_hal_pciePowerSaveEnable == 1) ? HAL_OK : HAL_ENOTSUPP;
        }
        return HAL_ENOTSUPP;

    case HAL_CAP_HT:
        return pCap->halHTSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_HT20_SGI:
        return pCap->halHT20SGISupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_GTT:
        return pCap->halGTTSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_FAST_CC:
        return pCap->halFastCCSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_TX_CHAINMASK:
        *result = pCap->halTxChainMask;
        return HAL_OK;
    case HAL_CAP_RX_CHAINMASK:
        *result = pCap->halRxChainMask;
        return HAL_OK;
    case HAL_CAP_TX_TRIG_LEVEL_MAX:
        *result = pCap->halTxTrigLevelMax;
        return HAL_OK;
    case HAL_CAP_NUM_GPIO_PINS:
        *result = pCap->halNumGpioPins;
        return HAL_OK;
#if ATH_WOW                
    case HAL_CAP_WOW:
        return pCap->halWowSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_WOW_MATCH_EXACT:
        return pCap->halWowMatchPatternExact ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_WOW_MATCH_DWORD:
        return pCap->halWowPatternMatchDword ? HAL_OK : HAL_ENOTSUPP;
#endif                
    case HAL_CAP_CST:
        return pCap->halCSTSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_RIFS_RX:
        return pCap->halRifsRxSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_RIFS_TX:
        return pCap->halRifsTxSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_FORCE_PPM:
        return pCap->halforcePpmSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_RTS_AGGR_LIMIT:
        *result = pCap->halRtsAggrLimit;
        return pCap->halRtsAggrLimit ? HAL_OK : HAL_ENOTSUPP;
	case HAL_CAP_ENHANCED_PM_SUPPORT:
		return pCap->halEnhancedPmSupport ? HAL_OK : HAL_ENOTSUPP;
	case HAL_CAP_AUTO_SLEEP:
        return pCap->halAutoSleepSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_MBSSID_AGGR_SUPPORT:   
        return pCap->halMbssidAggrSupport ? HAL_OK : HAL_ENOTSUPP;
 	case HAL_CAP_4KB_SPLIT_TRANS:
        return pCap->hal4kbSplitTransSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_REG_FLAG:
        *result = pCap->halRegCap;
        return HAL_OK;
    case HAL_CAP_ANT_CFG_5GHZ:
    case HAL_CAP_ANT_CFG_2GHZ:
        *result = 1; /* Default only one configuration */
        return HAL_OK;
    case HAL_CAP_MFP:  /* MFP option configured */
        *result = pCap->halMfpSupport;
        return HAL_OK;
    case HAL_CAP_RX_STBC:
        return HAL_ENOTSUPP;
    case HAL_CAP_TX_STBC:
        return HAL_ENOTSUPP;
    case HAL_CAP_LDPC:
        return HAL_ENOTSUPP;
#ifdef ATH_BT_COEX
    case HAL_CAP_BT_COEX:
        return pCap->halBtCoexSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_BT_COEX_ASPM_WAR:
        return pCap->halBtCoexASPMWAR ? HAL_OK : HAL_ENOTSUPP;
#endif
    case HAL_CAP_WPS_PUSH_BUTTON:
        return pCap->halWpsPushButton ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_ENHANCED_DMA_SUPPORT:
        return pCap->halEnhancedDmaSupport ? HAL_OK : HAL_ENOTSUPP;
#ifdef ATH_SUPPORT_DFS
    case HAL_CAP_ENHANCED_DFS_SUPPORT:
        return pCap->hal_enhanced_dfs_support ? HAL_OK : HAL_ENOTSUPP;
#endif
    case HAL_CAP_PROXYSTA:
        return pCap->halProxySTASupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_NUM_TXMAPS:
        *result = pCap->halNumTxMaps;
        return pCap->halNumTxMaps ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_TXDESCLEN:
        *result = pCap->halTxDescLen;
        return pCap->halTxDescLen ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_TXSTATUSLEN:
        *result = pCap->halTxStatusLen;
        return pCap->halTxStatusLen ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_RXSTATUSLEN:
        *result = pCap->halRxStatusLen;
        return pCap->halRxStatusLen ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_RXFIFODEPTH:
        switch (capability) {
        case HAL_RX_QUEUE_HP:
            *result = pCap->halRxHPdepth;
            break;
        case HAL_RX_QUEUE_LP:
            *result = pCap->halRxLPdepth;
            break;
        default:
            return HAL_ENOTSUPP;
        }
        return HAL_OK;
    case HAL_CAP_WEP_TKIP_AGGR:
        return pCap->halWepTkipAggrSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_WEP_TKIP_AGGR_TX_DELIM:
        *result = pCap->halWepTkipAggrNumTxDelim;
        return HAL_OK;
    case HAL_CAP_WEP_TKIP_AGGR_RX_DELIM:
        *result = pCap->halWepTkipAggrNumRxDelim;
        return HAL_OK;
    case HAL_CAP_DEVICE_TYPE:
        *result = (u_int32_t)AH_PRIVATE(ah)->ah_devType;
        return HAL_OK;
    case HAL_INTR_MITIGATION_SUPPORTED:
        *result = pCap->halintr_mitigation;
        return HAL_OK;
    case HAL_CAP_MAX_TKIP_WEP_HT_RATE:
        *result = pCap->halWepTkipMaxHtRate;
        return HAL_OK;
    case HAL_CAP_NUM_MR_RETRIES:
        *result = pCap->halNumMRRetries;
        return HAL_OK;
    case HAL_CAP_GEN_TIMER:
        return pCap->halGenTimerSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_MAX_WEP_TKIP_HT20_TX_RATEKBPS:
    case HAL_CAP_MAX_WEP_TKIP_HT40_TX_RATEKBPS:
    case HAL_CAP_MAX_WEP_TKIP_HT20_RX_RATEKBPS:
    case HAL_CAP_MAX_WEP_TKIP_HT40_RX_RATEKBPS:
        *result = (u_int32_t)(-1);
        return HAL_OK;
    case HAL_CAP_CFENDFIX:
        *result = pCap->halCfendFixSupport;
        return HAL_OK;
#if ATH_SUPPORT_SPECTRAL
    case HAL_CAP_SPECTRAL_SCAN:
        return pCap->halSpectralScan ? HAL_OK : HAL_ENOTSUPP;
#endif
#if ATH_SUPPORT_RAW_ADC_CAPTURE
    case HAL_CAP_RAW_ADC_CAPTURE:
        return pCap->halRawADCCapture ? HAL_OK : HAL_ENOTSUPP;
#endif
    case HAL_CAP_EXTRADELIMWAR:
        *result = pCap->halAggrExtraDelimWar;
        return HAL_OK;
    case HAL_CAP_RXDESC_TIMESTAMPBITS:
        *result = pCap->halRxDescTimestampBits;
        return HAL_OK;
    case HAL_CAP_RXTX_ABORT:
        return pCap->halRxTxAbortSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_ANI_POLL_INTERVAL:
        *result = pCap->halAniPollInterval;
        return HAL_OK;
    case HAL_CAP_PAPRD_ENABLED:
        *result = pCap->halPaprdEnabled;
        return pCap->halPaprdEnabled ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_HW_UAPSD_TRIG:
        return pCap->halHwUapsdTrig ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_ANT_DIV_COMB:
        return pCap->halAntDivCombSupport ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_CHANNEL_SWITCH_TIME_USEC:
        *result = pCap->halChannelSwitchTimeUsec;
        return HAL_OK;
#if ATH_SUPPORT_WAPI
    case HAL_CAP_WAPI_MAX_TX_CHAINS:
        *result = pCap->hal_wapi_max_tx_chains;
        return HAL_OK;
    case HAL_CAP_WAPI_MAX_RX_CHAINS:
        *result = pCap->hal_wapi_max_rx_chains;
        return HAL_OK;
#endif
    default:
        return HAL_EINVAL;
    }
}

HAL_BOOL
ath_hal_setcapability(struct ath_hal *ah, HAL_CAPABILITY_TYPE type,
    u_int32_t capability, u_int32_t setting, HAL_STATUS *status)
{

    switch (type) {
    case HAL_CAP_TXPOW:
        switch (capability) {
        case 3:
            /* Make sure we do not go beyond the MIN value */
            if (setting > HAL_TP_SCALE_MIN) 
                setting = HAL_TP_SCALE_MIN;
            AH_PRIVATE(ah)->ah_tpScale = setting;
            return AH_TRUE;
        }
        break;
    case HAL_CAP_RFSILENT:      /* rfsilent support  */
        /*
         * NB: allow even if halRfSilentSupport is false
         *     in case the EEPROM is misprogrammed.
         */
        switch (capability) {
        case 1:         /* current setting */
            if (!AH_PRIVATE(ah)->ah_rfkillEnabled && (setting != 0)) {
                /* 
                 * When ah_rfkillEnabled is set from 0 to 1, clear ah_rfkillINTInit to 0.
                 */
                AH_PRIVATE(ah)->ah_rfkillINTInit = AH_FALSE;
            }
            AH_PRIVATE(ah)->ah_rfkillEnabled = (setting != 0);
            return AH_TRUE;
        case 2:         /* rfsilent config */
            return AH_FALSE;
        }
        break;
#if ATH_SUPPORT_IBSS_WPA2
    case HAL_CAP_MCAST_KEYSRCH:
        (*(ah->ah_setCapability))(ah, type, capability, setting, status);
        break;
#endif
#ifdef AH_PRIVATE_DIAG
    case HAL_CAP_REG_DMN:           /* regulatory domain */
        AH_PRIVATE(ah)->ah_currentRD = setting;
        return AH_TRUE;
#endif
    default:
        break;
    }
    if (status)
        *status = HAL_EINVAL;
    return AH_FALSE;
}

/* 
 * Common support for getDiagState method.
 */

static u_int
ath_hal_getregdump(struct ath_hal *ah, const HAL_REGRANGE *regs,
    void *dstbuf, int space)
{
    u_int32_t *dp = dstbuf;
    int i;

    for (i = 0; space >= 2*sizeof(u_int32_t); i++) {
        u_int r = regs[i].start;
        u_int e = regs[i].end;
        *dp++ = (r<<16) | e;
        space -= sizeof(u_int32_t);
        do {
            *dp++ = OS_REG_READ(ah, r);
            r += sizeof(u_int32_t);
            space -= sizeof(u_int32_t);
        } while (r <= e && space >= sizeof(u_int32_t));
    }
    return (char *) dp - (char *) dstbuf;
}

HAL_BOOL
ath_hal_getdiagstate(struct ath_hal *ah, int request,
    const void *args, u_int32_t argsize,
    void **result, u_int32_t *resultsize)
{
    switch (request) {
    case HAL_DIAG_REVS:
        *result = &AH_PRIVATE(ah)->ah_devid;
        *resultsize = sizeof(HAL_REVS);
        return AH_TRUE;
    case HAL_DIAG_REGS:
        *resultsize = ath_hal_getregdump(ah, args, *result,*resultsize);
        return AH_TRUE;
    case HAL_DIAG_REGREAD: {
            if (argsize != sizeof(u_int))
           return AH_FALSE;
        **(u_int32_t **)result = OS_REG_READ(ah, *(const u_int *)args);
        *resultsize = sizeof(u_int32_t);
        return AH_TRUE;    
    }
    case HAL_DIAG_REGWRITE: {
            const HAL_DIAG_REGVAL *rv;
        if (argsize != sizeof(HAL_DIAG_REGVAL))
            return AH_FALSE;
        rv = (const HAL_DIAG_REGVAL *)args;
        OS_REG_WRITE(ah, rv->offset, rv->val);
        return AH_TRUE;   
    }
    case HAL_DIAG_GET_REGBASE: {
        /* Should be HAL_BUS_HANDLE but compiler warns and hal build set to
           treat warnings as errors. */
        **(uintptr_t **)result = (uintptr_t) ah->ah_sh;
        *resultsize = sizeof(HAL_BUS_HANDLE);
        return AH_TRUE;
    }
#ifdef AH_PRIVATE_DIAG
    case HAL_DIAG_SETKEY: {
        const HAL_DIAG_KEYVAL *dk;

        if (argsize != sizeof(HAL_DIAG_KEYVAL))
            return AH_FALSE;
        dk = (const HAL_DIAG_KEYVAL *)args;
        return ah->ah_setKeyCacheEntry(ah, dk->dk_keyix,
            &dk->dk_keyval, dk->dk_mac, dk->dk_xor);
    }
    case HAL_DIAG_RESETKEY:
        if (argsize != sizeof(u_int16_t))
            return AH_FALSE;
        return ah->ah_resetKeyCacheEntry(ah, *(const u_int16_t *)args);
#endif /* AH_PRIVATE_DIAG */
    case HAL_DIAG_EEREAD:
        if (argsize != sizeof(u_int16_t))
            return AH_FALSE;
        if (!ath_hal_eepromRead(ah, *(const u_int16_t *)args, *result))
            return AH_FALSE;
        *resultsize = sizeof(u_int16_t);
        return AH_TRUE;
    case HAL_DIAG_EEPROM:
        {
            void *dstbuf;
            *resultsize = ath_hal_eepromDump(ah, &dstbuf);
            *result = dstbuf;
        }
        return AH_TRUE;
#ifdef AH_SUPPORT_WRITE_EEPROM
    case HAL_DIAG_EEWRITE: {
        const HAL_DIAG_EEVAL *ee;

        if (argsize != sizeof(HAL_DIAG_EEVAL))
            return AH_FALSE;
        ee = (const HAL_DIAG_EEVAL *)args;
        return ath_hal_eepromWrite(ah, ee->ee_off, ee->ee_data);
    }
#endif /* AH_SUPPORT_WRITE_EEPROM */
    case HAL_DIAG_RDWRITE: {
        HAL_STATUS status;
        const HAL_REG_DOMAIN *rd;
    
        if (argsize != sizeof(HAL_REG_DOMAIN))
            return AH_FALSE;
        rd = (u_int16_t *) args;
        if (!ath_hal_setcapability(ah, HAL_CAP_REG_DMN, 0, *rd, &status))
            return AH_FALSE;
        return AH_TRUE;
    }   
    case HAL_DIAG_RDREAD: {
        u_int32_t rd;
        if (ath_hal_getcapability(ah, HAL_CAP_REG_DMN, 0, &rd) != HAL_OK){
            return AH_FALSE;
        }
        *resultsize = sizeof(u_int16_t);
        *((HAL_REG_DOMAIN *) (*result)) = (u_int16_t) rd;
        return AH_TRUE;
    }
 
    }
    return AH_FALSE;
}

/*
 * Set the properties of the tx queue with the parameters
 * from qInfo.
 */
HAL_BOOL
ath_hal_setTxQProps(struct ath_hal *ah,
    HAL_TX_QUEUE_INFO *qi, const HAL_TXQ_INFO *qInfo)
{
    u_int32_t cw;

    if (qi->tqi_type == HAL_TX_QUEUE_INACTIVE) {
        HDPRINTF(ah, HAL_DBG_QUEUE, "%s: inactive queue\n", __func__);
        return AH_FALSE;
    }

    HDPRINTF(ah, HAL_DBG_QUEUE, "%s: queue %p\n", __func__, qi);

    /* XXX validate parameters */
    qi->tqi_ver = qInfo->tqi_ver;
    qi->tqi_subtype = qInfo->tqi_subtype;
    qi->tqi_qflags = qInfo->tqi_qflags;
    qi->tqi_priority = qInfo->tqi_priority;
    if (qInfo->tqi_aifs != HAL_TXQ_USEDEFAULT)
        qi->tqi_aifs = AH_MIN(qInfo->tqi_aifs, 255);
    else
        qi->tqi_aifs = INIT_AIFS;
    if (qInfo->tqi_cwmin != HAL_TXQ_USEDEFAULT) {
        cw = AH_MIN(qInfo->tqi_cwmin, 1024);
        /* make sure that the CWmin is of the form (2^n - 1) */
        qi->tqi_cwmin = 1;
        while (qi->tqi_cwmin < cw)
            qi->tqi_cwmin = (qi->tqi_cwmin << 1) | 1;
    } else
        qi->tqi_cwmin = qInfo->tqi_cwmin;
    if (qInfo->tqi_cwmax != HAL_TXQ_USEDEFAULT) {
        cw = AH_MIN(qInfo->tqi_cwmax, 1024);
        /* make sure that the CWmax is of the form (2^n - 1) */
        qi->tqi_cwmax = 1;
        while (qi->tqi_cwmax < cw)
            qi->tqi_cwmax = (qi->tqi_cwmax << 1) | 1;
    } else
        qi->tqi_cwmax = INIT_CWMAX;
    /* Set retry limit values */
    if (qInfo->tqi_shretry != 0)
        qi->tqi_shretry = AH_MIN(qInfo->tqi_shretry, 15);
    else
        qi->tqi_shretry = INIT_SH_RETRY;
    if (qInfo->tqi_lgretry != 0)
        qi->tqi_lgretry = AH_MIN(qInfo->tqi_lgretry, 15);
    else
        qi->tqi_lgretry = INIT_LG_RETRY;
    qi->tqi_cbrPeriod = qInfo->tqi_cbrPeriod;
    qi->tqi_cbrOverflowLimit = qInfo->tqi_cbrOverflowLimit;
    qi->tqi_burstTime = qInfo->tqi_burstTime;
    qi->tqi_readyTime = qInfo->tqi_readyTime;

    switch (qInfo->tqi_subtype) {
    case HAL_WME_UPSD:
        if (qi->tqi_type == HAL_TX_QUEUE_DATA)
            qi->tqi_intFlags = HAL_TXQ_USE_LOCKOUT_BKOFF_DIS;
        break;
    default:
        break;      /* NB: silence compiler */
    }
    return AH_TRUE;
}

HAL_BOOL
ath_hal_getTxQProps(struct ath_hal *ah,
    HAL_TXQ_INFO *qInfo, const HAL_TX_QUEUE_INFO *qi)
{
    if (qi->tqi_type == HAL_TX_QUEUE_INACTIVE) {
        HDPRINTF(ah, HAL_DBG_QUEUE, "%s: inactive queue\n", __func__);
        return AH_FALSE;
    }

    qInfo->tqi_qflags = qi->tqi_qflags;
    qInfo->tqi_ver = qi->tqi_ver;
    qInfo->tqi_subtype = qi->tqi_subtype;
    qInfo->tqi_qflags = qi->tqi_qflags;
    qInfo->tqi_priority = qi->tqi_priority;
    qInfo->tqi_aifs = qi->tqi_aifs;
    qInfo->tqi_cwmin = qi->tqi_cwmin;
    qInfo->tqi_cwmax = qi->tqi_cwmax;
    qInfo->tqi_shretry = qi->tqi_shretry;
    qInfo->tqi_lgretry = qi->tqi_lgretry;
    qInfo->tqi_cbrPeriod = qi->tqi_cbrPeriod;
    qInfo->tqi_cbrOverflowLimit = qi->tqi_cbrOverflowLimit;
    qInfo->tqi_burstTime = qi->tqi_burstTime;
    qInfo->tqi_readyTime = qi->tqi_readyTime;
    return AH_TRUE;
}

                                     /* 11a Turbo  11b  11g  108g  XR */
static const int16_t NOISE_FLOOR[] = { -96, -93,  -98, -96,  -93, -96 };

/*
 * Read the current channel noise floor and return.
 * If nf cal hasn't finished, channel noise floor should be 0
 * and we return a nominal value based on band and frequency.
 *
 * NB: This is a private routine used by per-chip code to
 *     implement the ah_getChanNoise method.
 */
int16_t
ath_hal_getChanNoise(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    HAL_CHANNEL_INTERNAL *ichan;

    ichan = ath_hal_checkchannel(ah, chan);
    if (ichan == AH_NULL) {
        HDPRINTF(ah, HAL_DBG_NF_CAL, "%s: invalid channel %u/0x%x; no mapping\n",
            __func__, chan->channel, chan->channelFlags);
        return 0;
    }
    if (ichan->rawNoiseFloor == 0) {
        WIRELESS_MODE mode = ath_hal_chan2wmode(ah, chan);

        HALASSERT(mode < WIRELESS_MODE_MAX);
        return NOISE_FLOOR[mode] + ath_hal_getNfAdjust(ah, ichan);
    } else
        return ichan->rawNoiseFloor;
}

/*
 * Enable single Write Key cache
 */
void __ahdecl
ath_hal_set_singleWriteKC(struct ath_hal *ah, u_int8_t singleWriteKC)
{
    AH_PRIVATE(ah)->ah_singleWriteKC = singleWriteKC;
}

HAL_BOOL __ahdecl ath_hal_enabledANI(struct ath_hal *ah)
{
    return AH_PRIVATE(ah)->ah_config.ath_hal_enableANI;
}

u_int32_t ath_hal_get_device_info(struct ath_hal *ah,HAL_DEVICE_INFO type)
{
    struct ath_hal_private  *ap = AH_PRIVATE(ah);
    u_int32_t result = 0;

    switch (type) {
    case HAL_MAC_VERSION:        /* MAC version id */
        result = ap->ah_macVersion;
        break;
    case HAL_MAC_REV:            /* MAC revision */
        result = ap->ah_macRev;
        break;
    case HAL_PHY_REV:            /* PHY revision */
        result = ap->ah_phyRev;
        break;
    case HAL_ANALOG5GHZ_REV:     /* 5GHz radio revision */
        result = ap->ah_analog5GhzRev;
        break;
    case HAL_ANALOG2GHZ_REV:     /* 2GHz radio revision */
        result = ap->ah_analog2GhzRev;
        break;
    default:
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: Unknown type %d\n",__func__,type);
        HALASSERT(0);
    }
    return result;
}

#if AH_REGREAD_DEBUG
inline void
ath_hal_regread_dump(struct ath_hal *ah)
{
    struct ath_hal_private *ahp = AH_PRIVATE(ah);
    int i;
    u_int64_t tsfnow;
    tsfnow = ah->ah_getTsf64(ah);
    printk("REG base = 0x%x, total time is %lld x10^-6 sec\n", 
           ahp->ah_regaccessbase<<13, 
           tsfnow - ahp->ah_regtsf_start);
/* In order to save memory, the register address space are
 * divided by 8 segments, with each the size is 8192 (0x2000),
 * and the ah_regaccessbase is from 0 to 7.
 */
    for (i = 0; i < 8192; i ++) {
        if (ahp->ah_regaccess[i] > 0) {
            printk("REG #%x, counts %d\n", 
                   i+ahp->ah_regaccessbase*8192, ahp->ah_regaccess[i]);
        }
    }
}
#endif

/******************************************************************************/
/*!
**  \brief Set HAL Private parameter
**
**  This function will set the specific parameter ID to the value passed in
**  the buffer.  The caller will have to ensure the proper type is used.
**
**  \param ah Pointer to HAL object (this)
**  \param p Parameter ID
**  \param b Parameter value in binary format.
**
**  \return 0 On success
**  \return -1 on invalid parameter
*/

u_int32_t
ath_hal_set_config_param(struct ath_hal *ah,
                         HAL_CONFIG_OPS_PARAMS_TYPE p,
                         void *valBuff)
{
    struct ath_hal_private  *ap = AH_PRIVATE(ah);
    int                     retval = 0;
    
    /*
    ** Code Begins
    ** Switch on parameter
    */
    
    switch(p)
    {
        case HAL_CONFIG_DMA_BEACON_RESPONSE_TIME:
            ap->ah_config.ath_hal_dma_beacon_response_time = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_SW_BEACON_RESPONSE_TIME:
            ap->ah_config.ath_hal_sw_beacon_response_time = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_ADDITIONAL_SWBA_BACKOFF:
            ap->ah_config.ath_hal_additional_swba_backoff = *(int *)valBuff;
        break;                                         
                                                   
        case HAL_CONFIG_6MB_ACK:                        
            ap->ah_config.ath_hal_6mb_ack = *(int *)valBuff;
        break;                                         
                                                   
        case HAL_CONFIG_CWMIGNOREEXTCCA:                
            ap->ah_config.ath_hal_cwmIgnoreExtCCA = *(int *)valBuff;
        break;                                         
        
#ifdef ATH_FORCE_BIAS
        case HAL_CONFIG_FORCEBIAS:              
            ap->ah_config.ath_hal_forceBias = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_FORCEBIASAUTO:
            ap->ah_config.ath_hal_forceBiasAuto = *(int *)valBuff;
        break;
#endif
        
        case HAL_CONFIG_PCIEPOWERSAVEENABLE:    
            ap->ah_config.ath_hal_pciePowerSaveEnable = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_PCIEL1SKPENABLE:        
            ap->ah_config.ath_hal_pcieL1SKPEnable = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_PCIECLOCKREQ:           
            ap->ah_config.ath_hal_pcieClockReq = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_PCIEWAEN:               
            ap->ah_config.ath_hal_pcieWaen = *(int *)valBuff;
        break;

        case HAL_CONFIG_PCIEDETACH:               
            ap->ah_config.ath_hal_pcieDetach = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_PCIEPOWERRESET:         
            ap->ah_config.ath_hal_pciePowerReset = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_PCIERESTORE:         
            ap->ah_config.ath_hal_pcieRestore = *(int *)valBuff;
        break;

        case HAL_CONFIG_HTENABLE:               
            ap->ah_config.ath_hal_htEnable = *(int *)valBuff;
        break;
        
#ifdef ATH_SUPERG_DYNTURBO
        case HAL_CONFIG_DISABLETURBOG:          
            ap->ah_config.ath_hal_disableTurboG = *(int *)valBuff;
        break;
#endif
        case HAL_CONFIG_OFDMTRIGLOW:            
            ap->ah_config.ath_hal_ofdmTrigLow = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_OFDMTRIGHIGH:           
            ap->ah_config.ath_hal_ofdmTrigHigh = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_CCKTRIGHIGH:            
            ap->ah_config.ath_hal_cckTrigHigh = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_CCKTRIGLOW:             
            ap->ah_config.ath_hal_cckTrigLow = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_ENABLEANI:              
            ap->ah_config.ath_hal_enableANI = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_NOISEIMMUNITYLVL:       
            ap->ah_config.ath_hal_noiseImmunityLvl = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_OFDMWEAKSIGDET:         
            ap->ah_config.ath_hal_ofdmWeakSigDet = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_CCKWEAKSIGTHR:          
            ap->ah_config.ath_hal_cckWeakSigThr = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_SPURIMMUNITYLVL:        
            ap->ah_config.ath_hal_spurImmunityLvl = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_FIRSTEPLVL:             
            ap->ah_config.ath_hal_firStepLvl = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_RSSITHRHIGH:            
            ap->ah_config.ath_hal_rssiThrHigh = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_RSSITHRLOW:             
            ap->ah_config.ath_hal_rssiThrLow = *(int *)valBuff;
        break;
        case HAL_CONFIG_DIVERSITYCONTROL:       
            ap->ah_config.ath_hal_diversityControl = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_ANTENNASWITCHSWAP:
            ap->ah_config.ath_hal_antennaSwitchSwap = *(int *)valBuff;
        break;

        case HAL_CONFIG_FULLRESETWARENABLE:
            ap->ah_config.ath_hal_fullResetWarEnable = *(int *)valBuff;
        break;
        case HAL_CONFIG_DISABLEPERIODICPACAL:
            ap->ah_config.ath_hal_disPeriodicPACal = *(int *)valBuff;
        break;
#if ATH_SUPPORT_IBSS_WPA2
        case HAL_CONFIG_KEYCACHE_PRINT:
            if (ah->ah_PrintKeyCache)
                (*ah->ah_PrintKeyCache)(ah);
        break;
#endif
#if AH_DEBUG
        case HAL_CONFIG_DEBUG:
            ap->ah_config.ath_hal_debug = *(int *)valBuff;
        break;
#endif
        case HAL_CONFIG_DEFAULTANTENNASET:
            ap->ah_config.ath_hal_defaultAntCfg = *(int *)valBuff;
        break;

#if AH_REGREAD_DEBUG
        case HAL_CONFIG_REGREAD_BASE:
            ap->ah_regaccessbase = *(int *)valBuff;
            OS_MEMZERO(ap->ah_regaccess, 8192*sizeof(u_int32_t));
            ap->ah_regtsf_start = ah->ah_getTsf64(ah);
        break;
#endif
#ifdef ATH_SUPPORT_TxBF
        case HAL_CONFIG_TxBF_CTL:
            ap->ah_config.ath_hal_txbfCtl = *(int *)valBuff;
            ah->ah_FillTxBFCap(ah);         // update txbf cap by new settings
        break;
#endif

        case HAL_CONFIG_SHOW_BB_PANIC:
            ap->ah_config.ath_hal_show_bb_panic = !!*(int *)valBuff;
        break;
        case HAL_CONFIG_INTR_MITIGATION_RX:
            ap->ah_config.ath_hal_intrMitigationRx = *(int *)valBuff;
        break;

        default:
            retval = -1;
    }
    
    return (retval);
}

/******************************************************************************/
/*!
**  \brief Get HAL Private Parameter
**
**  Gets the value of the parameter and returns it in the provided buffer. It
**  is assumed that the value is ALWAYS returned as an integre value.
**
**  \param ah      Pointer to HAL object (this)
**  \param p       Parameter ID
**  \param valBuff Parameter value in binary format.
**
**  \return number of bytes put into the buffer
**  \return 0 on invalid parameter ID
*/

u_int32_t
ath_hal_get_config_param(struct ath_hal *ah,
                         HAL_CONFIG_OPS_PARAMS_TYPE p, 
                         void *valBuff)
{
    struct ath_hal_private  *ap = AH_PRIVATE(ah);
    int                     retval = 0;
    
    /*
    ** Code Begins
    ** Switch on parameter
    */
    
    switch (p)
    {
        case HAL_CONFIG_DMA_BEACON_RESPONSE_TIME:
            *(int *)valBuff = ap->ah_config.ath_hal_dma_beacon_response_time;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_SW_BEACON_RESPONSE_TIME:
            *(int *)valBuff = ap->ah_config.ath_hal_sw_beacon_response_time;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_ADDITIONAL_SWBA_BACKOFF:
            *(int *)valBuff = ap->ah_config.ath_hal_additional_swba_backoff;
            retval = sizeof(int);
        break;                                         
                                                   
        case HAL_CONFIG_6MB_ACK:                        
            *(int *)valBuff = ap->ah_config.ath_hal_6mb_ack;
            retval = sizeof(int);
        break;                                         
                                                   
        case HAL_CONFIG_CWMIGNOREEXTCCA:                
            *(int *)valBuff = ap->ah_config.ath_hal_cwmIgnoreExtCCA;
            retval = sizeof(int);
        break;                                         
        
#ifdef ATH_FORCE_BIAS
        case HAL_CONFIG_FORCEBIAS:              
            *(int *)valBuff = ap->ah_config.ath_hal_forceBias;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_FORCEBIASAUTO:
            *(int *)valBuff = ap->ah_config.ath_hal_forceBiasAuto;
            retval = sizeof(int);
        break;
#endif
        
        case HAL_CONFIG_PCIEPOWERSAVEENABLE:    
            *(int *)valBuff = ap->ah_config.ath_hal_pciePowerSaveEnable;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_PCIEL1SKPENABLE:        
            *(int *)valBuff = ap->ah_config.ath_hal_pcieL1SKPEnable;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_PCIECLOCKREQ:           
            *(int *)valBuff = ap->ah_config.ath_hal_pcieClockReq;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_PCIEWAEN:               
            *(int *)valBuff = ap->ah_config.ath_hal_pcieWaen;
            retval = sizeof(int);
        break;

        case HAL_CONFIG_PCIEDETACH:               
            *(int *)valBuff = ap->ah_config.ath_hal_pcieDetach;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_PCIEPOWERRESET:         
            *(int *)valBuff = ap->ah_config.ath_hal_pciePowerReset;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_PCIERESTORE:         
            *(int *)valBuff = ap->ah_config.ath_hal_pcieRestore;
            retval = sizeof(int);
        break;

        case HAL_CONFIG_HTENABLE:               
            *(int *)valBuff = ap->ah_config.ath_hal_htEnable;
            retval = sizeof(int);
        break;
        
#ifdef ATH_SUPERG_DYNTURBO
        case HAL_CONFIG_DISABLETURBOG:          
            *(int *)valBuff = ap->ah_config.ath_hal_disableTurboG;
            retval = sizeof(int);
        break;
#endif
        case HAL_CONFIG_OFDMTRIGLOW:            
            *(int *)valBuff = ap->ah_config.ath_hal_ofdmTrigLow;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_OFDMTRIGHIGH:           
            *(int *)valBuff = ap->ah_config.ath_hal_ofdmTrigHigh;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_CCKTRIGHIGH:            
            *(int *)valBuff = ap->ah_config.ath_hal_cckTrigHigh;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_CCKTRIGLOW:             
            *(int *)valBuff = ap->ah_config.ath_hal_cckTrigLow;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_ENABLEANI:              
            *(int *)valBuff = ap->ah_config.ath_hal_enableANI;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_NOISEIMMUNITYLVL:       
            *(int *)valBuff = ap->ah_config.ath_hal_noiseImmunityLvl;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_OFDMWEAKSIGDET:         
            *(int *)valBuff = ap->ah_config.ath_hal_ofdmWeakSigDet;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_CCKWEAKSIGTHR:          
            *(int *)valBuff = ap->ah_config.ath_hal_cckWeakSigThr;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_SPURIMMUNITYLVL:        
            *(int *)valBuff = ap->ah_config.ath_hal_spurImmunityLvl;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_FIRSTEPLVL:             
            *(int *)valBuff = ap->ah_config.ath_hal_firStepLvl;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_RSSITHRHIGH:            
            *(int *)valBuff = ap->ah_config.ath_hal_rssiThrHigh;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_RSSITHRLOW:             
            *(int *)valBuff = ap->ah_config.ath_hal_rssiThrLow;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_DIVERSITYCONTROL:       
            *(int *)valBuff = ap->ah_config.ath_hal_diversityControl;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_ANTENNASWITCHSWAP:
            *(int *)valBuff = ap->ah_config.ath_hal_antennaSwitchSwap;
            retval = sizeof(int);
        break;

        case HAL_CONFIG_FULLRESETWARENABLE:
            *(int *)valBuff = ap->ah_config.ath_hal_fullResetWarEnable;
            retval = sizeof(int);
        break;

        case HAL_CONFIG_DISABLEPERIODICPACAL:
            *(int *)valBuff = ap->ah_config.ath_hal_disPeriodicPACal;
            retval = sizeof(int);
        break;

#if ATH_SUPPORT_IBSS_WPA2
        case HAL_CONFIG_KEYCACHE_PRINT:
            if (ah->ah_PrintKeyCache)
                (*ah->ah_PrintKeyCache)(ah);
            retval = sizeof(int);
        break;
#endif

#if AH_DEBUG
        case HAL_CONFIG_DEBUG:
            *(int *)valBuff = ap->ah_config.ath_hal_debug;
            retval = sizeof(int);
        break;
#endif
        
        case HAL_CONFIG_DEFAULTANTENNASET:
            *(int *)valBuff = ap->ah_config.ath_hal_defaultAntCfg;
        break;

#if AH_REGREAD_DEBUG
        case HAL_CONFIG_REGREAD_BASE:
            ath_hal_regread_dump(ah);           
            retval = sizeof(int);
        break;    
#endif

#ifdef ATH_SUPPORT_TxBF
        case HAL_CONFIG_TxBF_CTL:        
            *(int *)valBuff = ap->ah_config.ath_hal_txbfCtl;
            retval = sizeof(int);           
        break;
#endif

        case HAL_CONFIG_SHOW_BB_PANIC:
            *(int *)valBuff = ap->ah_config.ath_hal_show_bb_panic;
            retval = sizeof(int);           
        break;

        default:
            retval = 0;
    }
    
    return (retval);
}
#ifdef ATH_CCX
/*
 * ath_hal_get_sernum
 * Copy the HAL's serial number buffer into a buffer provided by the caller.
 * The caller can optionally specify a limit on the number of characters
 * to copy into the pSn buffer.
 * This limit is only used if it is a positive number.
 * The number of characters truncated during the copy is returned.
 */
int
ath_hal_get_sernum(struct ath_hal *ah, u_int8_t *pSn, int limit)
{
    struct ath_hal_private *ahp = AH_PRIVATE(ah);
    u_int8_t    i;
    int bytes_truncated = 0;

    if (limit <= 0 || limit >= sizeof(ahp->serNo)) {
        limit = sizeof(ahp->serNo);
    } else {
        bytes_truncated = sizeof(ahp->serNo) - limit;
    }

    for (i = 0; i < limit; i++) {
        pSn[i] = ahp->serNo[i];
    }
    return bytes_truncated;
}    

HAL_BOOL
ath_hal_get_chandata(struct ath_hal *ah, HAL_CHANNEL *chan, HAL_CHANNEL_DATA* pChanData)
{
    WIRELESS_MODE       mode = ath_hal_chan2wmode(ah, chan);

    if (mode > WIRELESS_MODE_XR) {
        return AH_FALSE;
    }

    if (AH_TRUE == ath_hal_is_fast_clock_en(ah)) {
        pChanData->clockRate    = FAST_CLOCK_RATE[mode];
    } else {
        pChanData->clockRate    = CLOCK_RATE[mode];
    }
    pChanData->noiseFloor   = NOISE_FLOOR[mode];
    pChanData->ccaThreshold = ah->ah_getCcaThreshold(ah);

    return AH_TRUE;
}
#endif

#ifdef DBG
u_int32_t
ath_hal_readRegister(struct ath_hal *ah, u_int32_t offset)
{
    return OS_REG_READ(ah, offset);
}

void
ath_hal_writeRegister(struct ath_hal *ah, u_int32_t offset, u_int32_t value)
{
    OS_REG_WRITE(ah, offset, value);
}
#endif

void
ath_hal_display_tpctables(struct ath_hal *ah)
{
    if (ah->ah_DispTPCTables != AH_NULL) {
        ah->ah_DispTPCTables(ah);   
    }
}

