/*
 * Copyright (c) 2005, Atheros Communications Inc.
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

/*
 *  ath_wow.c:  implementation of WakeonWireless in ath layer 
 */
 
#include "ath_internal.h" 

#if ATH_WOW

#define ATH_WAKE_UP_PATTERN_MATCH          0x00000001
#define ATH_WAKE_UP_MAGIC_PACKET           0x00000002
#define ATH_WAKE_UP_LINK_CHANGE            0x00000004

/* Forward declarations*/
int ath_wow_create_pattern(ath_dev_t dev);

int
ath_get_wow_support(ath_dev_t dev)
{
    return ((ATH_DEV_TO_SC(dev))->sc_hasWow && (ATH_DEV_TO_SC(dev))->sc_wowenable);
}

int
ath_set_wow_enable(ath_dev_t dev, int clearbssid)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t wakeUpEvents = sc->sc_wowInfo->wakeUpEvents;
	HAL_INT status;

    if (sc->sc_invalid)
        return -EIO;
    
    ATH_PS_WAKEUP(sc);

    // Set SCR to force wake
    ath_pwrsave_awake(sc);

    /* Enable the pattern matching registers */
    if (wakeUpEvents & ATH_WAKE_UP_PATTERN_MATCH) {
        ath_wow_create_pattern(sc);
    }

	// Save the current interrupt mask so that we can restore it after system wakes up 
    sc->sc_wowInfo->intrMaskBeforeSleep = ath_hal_intrget(ah);
    // Disable interrupts and poll on isr exiting.
    sc->sc_invalid = 1;
    ath_hal_intrset(ah, 0);
    ath_wait_sc_inuse_cnt(sc, 1000);

    /*
     * disable interrupt and clear pending isr again in case "ath_handle_intr"
     * was running at the same time.
     */
    ath_hal_intrset(ah, 0);
    if (ath_hal_intrpend(ah)) {
        ath_hal_getisr(ah, &status, HAL_INT_LINE, HAL_MSIVEC_MISC);
    }

    /*
     * To avoid false wake, we enable beacon miss interrupt only when system sleep.
     */
    sc->sc_imask = HAL_INT_GLOBAL;
    if (wakeUpEvents & ATH_WAKE_UP_BEACON_MISS) {
        sc->sc_imask |= HAL_INT_BMISS;
    }    
    sc->sc_invalid = 0;
    ath_hal_intrset(ah, sc->sc_imask);
    /*
     * Call ath_hal_intrset(ah, sc->sc_imask) again since ath_hal_intrset(ah, 0)
     * was called twice previously. ah_ier_refcount has been bump up to 2.
     * Without the second call, AR_IER won't be enabled when compile flag
     * HAL_INTR_REFCOUNT_DISABLE is set to 1. EV 75289.
     */
    ath_hal_intrset(ah, sc->sc_imask);

    /* TBD: should not pass wakeUpEvents if "ath_hal_enable_wow_xxx_events" is implemented */
    /*
            The clearbssid parameter is set only for the case where the wake needs to be
            on a timeout. The timeout mechanism, at this time, is specific to Maverick. For a
            manuf test, the system is put into sleep with a timer. On timer expiry, the chip
            interrupts(timer) and wakes the system. Clearbssid is specified as TRUE since
            we do not want a spurious beacon miss. If specified as true, we clear the bssid regs.
           The timeout value is specified out of band (via ath_set_wow_timeout) and is held in the 
           wow info struct.
         */

    if (ath_hal_wowEnable(ah, wakeUpEvents, sc->sc_wowInfo->timeoutinsecs, clearbssid) == -1) {
        printk("%s[%d]: Error entering wow mode\n", __func__, __LINE__);
        return (-1);
    }
    else
    {
        USHORT   word = PCIE_PM_CSR_STATUS | PCIE_PM_CSR_PME_ENABLE |
            PCIE_PM_CSR_D3;
    
        OS_PCI_WRITE_CONFIG(sc->sc_osdev, PCIE_PM_CSR, &word, sizeof(USHORT));
    }

    sc->sc_wow_sleep = 1;

    ATH_PS_SLEEP(sc);
    return 0;
}

int
ath_wow_wakeup(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    struct wow_info  *wowInfo;
    u_int16_t word;
    u_int32_t wowStatus;

    DPRINTF(sc, ATH_DEBUG_ANY, "%s: Wakingup due to wow signal\n", __func__);

    ASSERT(sc->sc_wowInfo);
    wowInfo = sc->sc_wowInfo;

    sc->sc_wow_sleep = 0;

    ATH_PS_WAKEUP(sc);

    // Set SCR to force wake
    ath_pwrsave_awake(sc);

    word = PCIE_PM_CSR_STATUS;

    OS_PCI_WRITE_CONFIG(sc->sc_osdev, PCIE_PM_CSR, &word, sizeof(USHORT));

    /* restore system interrupt after wake up */
    ath_hal_intrset(ah, wowInfo->intrMaskBeforeSleep);
    sc->sc_imask = wowInfo->intrMaskBeforeSleep;
    
    wowStatus = ath_hal_wowWakeUp(ah);
    if (sc->sc_wow_bmiss_intr) {
        /*
         * Workaround for the hw that have problems detecting Beacon Miss
         * during WOW sleep.
         */
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: add beacon miss to wowStatus.\n", __func__);
        wowStatus |= AH_WOW_BEACON_MISS;
        sc->sc_wow_bmiss_intr = 0;
    }
    DPRINTF(sc, ATH_DEBUG_ANY, "%s, wowStatus = 0x%x\n", __func__, wowStatus);
    wowInfo->wowWakeupReason =  wowStatus & 0x0FFF;
    sc->sc_stats.ast_wow_wakeups++;
    if (wowStatus) {
        sc->sc_stats.ast_wow_wakeupsok++;
    }

    ATH_PS_SLEEP(sc);
    return 0;
}

/*
 * Function can be called only if WOW is supported by HW.
 */
int
ath_wow_create_pattern(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    int         pattern_num;
    int         pattern_count = 0;
    u_int8_t*   bssid;
    u_int32_t   i, byte_cnt;
    u_int8_t    dis_deauth_pattern[MAX_PATTERN_SIZE];
    u_int8_t    dis_deauth_mask[MAX_PATTERN_SIZE];
    WOW_PATTERN *pattern;

/*
 * FIXME: Is it possible to move this out of common code ?
 * Maverick wants to set std patterns - deauth &/or deassoc
 * selectively.
 */
    struct wow_info  *wowInfo = sc->sc_wowInfo;

    OS_MEMZERO(dis_deauth_pattern, MAX_PATTERN_SIZE);
    OS_MEMZERO(dis_deauth_mask, MAX_PATTERN_SIZE);


    /* Create Dissassociate / Deauthenticate packet filter
     *     2 bytes        2 byte    6 bytes   6 bytes  6 bytes
     *  +--------------+----------+---------+--------+--------+----
     *  + Frame Control+ Duration +   DA    +  SA    +  BSSID +
     *  +--------------+----------+---------+--------+--------+----
     *
     * The above is the management frame format for disassociate/deauthenticate
     * pattern, from this we need to match the first byte of 'Frame Control'
     * and DA, SA, and BSSID fields (skipping 2nd byte of FC and Duration feild. 
     * 
     * Disassociate pattern
     * --------------------
     * Frame control = 00 00 1010
     * DA, SA, BSSID = x:x:x:x:x:x
     * Pattern will be A0000000 | x:x:x:x:x:x | x:x:x:x:x:x | x:x:x:x:x:x  -- 22 bytes
     * Deauthenticate pattern
     * ----------------------
     * Frame control = 00 00 1100
     * DA, SA, BSSID = x:x:x:x:x:x
     * Pattern will be C0000000 | x:x:x:x:x:x | x:x:x:x:x:x | x:x:x:x:x:x  -- 22 bytes
     */

    /* Create Disassociate Pattern first */
    byte_cnt = 0;
    OS_MEMZERO((char*)&dis_deauth_pattern[0], MAX_PATTERN_SIZE);

    /* Fill out the mask with all FF's */
    for (i = 0; i < MAX_PATTERN_MASK_SIZE; i++) {
        dis_deauth_mask[i] = 0xff;
    }

    /* Copy the 1st byte of frame control field */
    dis_deauth_pattern[byte_cnt] = 0xA0;
    byte_cnt++;

    /* Skip 2nd byte of frame control and Duration field */
    byte_cnt += 3;

    /* Need not match the destination mac address, it can be a broadcast
     * mac address or an unicast to this staion 
     */
    byte_cnt += 6;

    /* Copy the Source mac address */
    bssid = sc->sc_curbssid;
    for (i = 0; i < IEEE80211_ADDR_LEN; i++) {
        dis_deauth_pattern[byte_cnt] = bssid[i];
        byte_cnt++;
    }

    /* Copy the BSSID , it's same as the source mac address */
    for (i = 0; i < IEEE80211_ADDR_LEN; i++) {
        dis_deauth_pattern[byte_cnt] = bssid[i];
        byte_cnt++;
    }

    /* Create Disassociate pattern mask */
    if (ath_hal_wowMatchPatternExact(ah)) {
        if (ath_hal_wowMatchPatternDword(ah)) {
            /*
             * This is a WAR for Bug 36529. For Merlin, because of hardware bug 31424 
             * that the first 4 bytes have to be matched for all patterns. The mask 
             * for disassociation and de-auth pattern matching need to enable the
             * first 4 bytes. Also the duration field needs to be filled. We assume
             * the duration of de-auth/disassoc frames and association resp are the same.
             */
            dis_deauth_mask[0] = 0xf0;

            /* Fill in duration field */
            dis_deauth_pattern[2] = wowInfo->wowDuration & 0xff;
            dis_deauth_pattern[3] = (wowInfo->wowDuration >> 8) & 0xff;
        }
        else {
            dis_deauth_mask[0] = 0xfe;
        }
        dis_deauth_mask[1] = 0x03;
        dis_deauth_mask[2] = 0xc0;
    } else {
        dis_deauth_mask[0] = 0xef;
        dis_deauth_mask[1] = 0x3f;
        dis_deauth_mask[2] = 0x00;
        dis_deauth_mask[3] = 0xfc;
    }

    ath_hal_wowApplyPattern(ah, dis_deauth_pattern, dis_deauth_mask, pattern_count, byte_cnt);
    pattern_count++;

    /* For de-authenticate pattern, only the 1st byte of the frame control
     * feild gets changed from 0xA0 to 0xC0
     */
    dis_deauth_pattern[0] = 0xC0;

    /* Now configure the Deauthenticate pattern to the pattern 1 registries */
    ath_hal_wowApplyPattern(ah, dis_deauth_pattern, dis_deauth_mask, pattern_count, byte_cnt);
    pattern_count++;

    for (pattern_num = 0; pattern_num < MAX_NUM_USER_PATTERN; pattern_num++) {
        pattern = &sc->sc_wowInfo->patterns[pattern_num];
        if (pattern->valid) {
            ath_hal_wowApplyPattern(ah, pattern->patternBytes, pattern->maskBytes, pattern_count, pattern->patternLen);
            pattern_count++;
        }
    }

    /* 
     * For the WOW Merlin WAR(But 36529), the first 4 bytes all need to be matched. We could
     * receive a de-auth frame that has been retransmitted. Add another de-auth pattern with 
     * retry bit set in frame control if there is a space available. 
     */
    if (ath_hal_wowMatchPatternDword(ah) && (pattern_count < MAX_NUM_PATTERN)) {
        dis_deauth_pattern[1] = 0x08;
        ath_hal_wowApplyPattern(ah, dis_deauth_pattern, dis_deauth_mask, pattern_count, byte_cnt);
        pattern_count++;
    }
    return 0;
}

/*
 * Function can be called only if WOW is supported by HW.
 */
void 
ath_set_wow_events(ath_dev_t dev, u_int32_t wowEvents)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    
    sc->sc_wowInfo->wakeUpEvents = wowEvents;

    /* TBD: Need to call ath_hal_enable_wow_xxx_events instead of passing this flag */
}

/*
 * Function can be called only if WOW is supported by HW.
 */
int
ath_get_wow_events(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    
    return sc->sc_wowInfo->wakeUpEvents;
}

/*
 * Function can be called only if WOW is supported by HW.
 */
int
ath_wow_add_wakeup_pattern(ath_dev_t dev, u_int8_t *patternBytes, u_int8_t *maskBytes, u_int32_t patternLen)
{

    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t i;
    struct wow_info  *wowInfo;
    WOW_PATTERN *pattern;

    ASSERT(sc->sc_wowInfo);
    wowInfo = sc->sc_wowInfo;

    /* check for duplicate patterns */
    for (i = 0; i < MAX_NUM_USER_PATTERN; i++) {
        pattern = &wowInfo->patterns[i];
        if (pattern->valid) {
            if (!OS_MEMCMP(patternBytes, pattern->patternBytes, MAX_PATTERN_SIZE) &&
                (!OS_MEMCMP(maskBytes, pattern->maskBytes, MAX_PATTERN_SIZE)))
            {
                printk("Pattern added already \n");
                return 0;
            }
        }
    }

    /* find a empty pattern entry */
    for (i = 0; i < MAX_NUM_USER_PATTERN; i++) {
        pattern = &wowInfo->patterns[i];
        if (!pattern->valid) {
            break;
        }
    }

    if (i == MAX_NUM_USER_PATTERN) {
        printk("Error : All the %d pattern are in use. Cannot add a new pattern \n", MAX_NUM_PATTERN);
        return (-1);
    }

    DPRINTF(sc, ATH_DEBUG_ANY, "Pattern added to entry %d \n",i);

    // add the pattern
    pattern = &wowInfo->patterns[i];
    OS_MEMCPY(pattern->maskBytes, maskBytes, MAX_PATTERN_SIZE);
    OS_MEMCPY(pattern->patternBytes, patternBytes, MAX_PATTERN_SIZE);
    pattern->patternLen = patternLen;
    pattern->valid = 1;

    return 0;
}

/*
 * Function can be called only if WOW is supported by HW.
 */
int
ath_wow_remove_wakeup_pattern(ath_dev_t dev, u_int8_t *patternBytes, u_int8_t *maskBytes)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t i;
    struct wow_info  *wowInfo;
    WOW_PATTERN *pattern;

    ASSERT(sc->sc_wowInfo);
    wowInfo = sc->sc_wowInfo;

    DPRINTF(sc, ATH_DEBUG_ANY,"%s: Remove wake up pattern \n", __func__);

#if defined(_WIN64)
    DPRINTF(sc, ATH_DEBUG_ANY, "%s: Pmask = %p pat = %p \n", __func__, maskBytes,patternBytes);
#else
    printk("mask = %x pat = %x \n",(u_int32_t)maskBytes, (u_int32_t)patternBytes);
#endif

    /* remove the pattern and return if present */
    for (i = 0; i < MAX_NUM_USER_PATTERN; i++) {
        pattern = &wowInfo->patterns[i];
        if (pattern->valid) {
            if (!OS_MEMCMP(patternBytes, pattern->patternBytes, MAX_PATTERN_SIZE) &&
                !OS_MEMCMP(maskBytes, pattern->maskBytes, MAX_PATTERN_SIZE))
            {
                pattern->valid = 0;
                OS_MEMZERO(pattern->patternBytes, MAX_PATTERN_SIZE);
                OS_MEMZERO(pattern->maskBytes, MAX_PATTERN_SIZE);
                DPRINTF(sc, ATH_DEBUG_ANY, "Pattern Removed from entry %d \n",i); 
                return 0;
            }
        }
    }

    // pattern not found
    DPRINTF(sc, ATH_DEBUG_ANY, "%s : Error : Pattern not found \n", __func__); 

    return (-1);
}

int
ath_get_wow_wakeup_reason(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    
    return sc->sc_wowInfo->wowWakeupReason;
}    

int
ath_wow_matchpattern_exact(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    return ((ath_hal_wowMatchPatternExact(ah)) ? TRUE : FALSE);
}   

/*
 * This is a WAR for Bug 36529. For Merlin, because of hardware bug 31424 
 * that the first 4 bytes have to be matched for all patterns. The duration 
 * field needs to be filled. We assume the duration of de-auth/disassoc 
 * frames and association resp are the same.
 *
 * This function is called by net80211 layer to set the duration field for 
 * pattern matching.
 */
void 
ath_wow_set_duration(ath_dev_t dev, u_int16_t duration)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (sc->sc_hasWow && (sc->sc_wowInfo != NULL)) {
        sc->sc_wowInfo->wowDuration = duration;
    }
}


void
ath_set_wow_timeout(ath_dev_t dev, u_int32_t timeoutinsecs)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (sc && sc->sc_wowInfo) {
        if (sc->sc_hasWow) {
            sc->sc_wowInfo->timeoutinsecs = timeoutinsecs;
        }
        else {
            sc->sc_wowInfo->timeoutinsecs = 0;
        }             
    }
}

u_int32_t
ath_get_wow_timeout(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    if (sc && sc->sc_wowInfo) {
        if (sc->sc_hasWow) {
            return sc->sc_wowInfo->timeoutinsecs;
        }
    }
    return 0;
}


#endif
