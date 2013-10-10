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

#ifdef AH_SUPPORT_AR5416

#include "ah.h"
#include "ah_internal.h"

#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"

extern u_int32_t ar5416NumTxPending(struct ath_hal *ah, u_int q);

/*
 * Initializes all of the hardware registers used to
 * send beacons.  Note that for station operation the
 * driver calls ar5416SetStaBeaconTimers instead.
 */
void
ar5416BeaconInit(struct ath_hal *ah,
    u_int32_t next_beacon, u_int32_t beacon_period, HAL_OPMODE opmode)
{
    struct ath_hal_private  *ap  = AH_PRIVATE(ah);
    int                     flags = 0;
    u_int32_t               beacon_period_usec;

    ENABLE_REG_WRITE_BUFFER

    switch (opmode) {
    case HAL_M_STA:
    case HAL_M_MONITOR:
    case HAL_M_RAW_ADC_CAPTURE:
        OS_REG_WRITE(
            ah, AR_NEXT_TBTT_TIMER, ONE_EIGHTH_TU_TO_USEC(next_beacon));
        OS_REG_WRITE(ah, AR_NEXT_DMA_BEACON_ALERT, 0xffff);
        OS_REG_WRITE(ah, AR_NEXT_SWBA, 0x7ffff);
        flags |= AR_TBTT_TIMER_EN;
        break;
    case HAL_M_IBSS:
        OS_REG_SET_BIT(ah, AR_TXCFG, AR_TXCFG_ADHOC_BEACON_ATIM_TX_POLICY);

    case HAL_M_HOSTAP:
        OS_REG_WRITE(
            ah, AR_NEXT_TBTT_TIMER, ONE_EIGHTH_TU_TO_USEC(next_beacon));
        OS_REG_WRITE(ah, AR_NEXT_DMA_BEACON_ALERT,
            (ONE_EIGHTH_TU_TO_USEC(next_beacon) -
            ap->ah_config.ath_hal_dma_beacon_response_time));
        OS_REG_WRITE(ah, AR_NEXT_SWBA,
            (ONE_EIGHTH_TU_TO_USEC(next_beacon) -
            ap->ah_config.ath_hal_sw_beacon_response_time));
        flags |= AR_TBTT_TIMER_EN | AR_DBA_TIMER_EN | AR_SWBA_TIMER_EN;
        break;
    }

    beacon_period_usec =
        ONE_EIGHTH_TU_TO_USEC(beacon_period & HAL_BEACON_PERIOD_TU8);
    OS_REG_WRITE(ah, AR_BEACON_PERIOD, beacon_period_usec);
    OS_REG_WRITE(ah, AR_DMA_BEACON_PERIOD, beacon_period_usec);
    OS_REG_WRITE(ah, AR_SWBA_PERIOD, beacon_period_usec);

    OS_REG_WRITE_FLUSH(ah);
    DISABLE_REG_WRITE_BUFFER

    /* no adhoc power save/atim window in owl */

    /*
     * reset TSF if required
     */
    if (beacon_period & HAL_BEACON_RESET_TSF) ar5416ResetTsf(ah);

    /* enable timers */
    OS_REG_SET_BIT(ah, AR_TIMER_MODE, flags);
}

/*
 * Wait for the beacon and CAB queue to finish.
 */
HAL_BOOL
ar5416WaitForBeaconDone(struct ath_hal *ah, HAL_BUS_ADDR baddr)
{
    int i;

    for (i = 0; i < 1000; i++) {
        /* XXX no check of CAB */
        if (ar5416NumTxPending(ah, HAL_TX_QUEUE_BEACON) == 0)
            break;
        OS_DELAY(10);
    }
    return (i < 1000);
}

#define AR_BEACON_PERIOD_MAX    0xffff
void
ar5416ResetStaBeaconTimers(struct ath_hal *ah)
{
    u_int32_t val;

    OS_REG_WRITE(ah, AR_NEXT_TBTT_TIMER, 0);        /* no beacons */
    val = OS_REG_READ(ah, AR_STA_ID1);
    val |= AR_STA_ID1_PWR_SAV;      /* XXX */
    /* tell the h/w that the associated AP is not PCF capable */
    ENABLE_REG_WRITE_BUFFER
    OS_REG_WRITE(ah, AR_STA_ID1,
        val & ~(AR_STA_ID1_USE_DEFANT | AR_STA_ID1_PCF));
    OS_REG_WRITE(ah, AR_BEACON_PERIOD, AR_BEACON_PERIOD_MAX);
    OS_REG_WRITE(ah, AR_DMA_BEACON_PERIOD, AR_BEACON_PERIOD_MAX);
    OS_REG_WRITE_FLUSH(ah);
    DISABLE_REG_WRITE_BUFFER
}

/*
 * Set all the beacon related bits on the h/w for stations
 * i.e. initializes the corresponding h/w timers;
 */
void
ar5416SetStaBeaconTimers(struct ath_hal *ah, const HAL_BEACON_STATE *bs)
{
    u_int32_t nextTbtt, beaconintval, dtimperiod, beacontimeout;
    HAL_CAPABILITIES *pCap = &AH_PRIVATE(ah)->ah_caps;

    HALASSERT(bs->bs_intval != 0);

    ENABLE_REG_WRITE_BUFFER

    /* no cfp setting since h/w automatically takes care */
    OS_REG_WRITE(ah, AR_NEXT_TBTT_TIMER, TU_TO_USEC(bs->bs_nexttbtt));

    /*
     * Start the beacon timers by setting the BEACON register
     * to the beacon interval; no need to write tim offset since
     * h/w parses IEs.
     */
    OS_REG_WRITE(ah, AR_BEACON_PERIOD,
                 TU_TO_USEC(bs->bs_intval & HAL_BEACON_PERIOD));
    OS_REG_WRITE(ah, AR_DMA_BEACON_PERIOD,
                 TU_TO_USEC(bs->bs_intval & HAL_BEACON_PERIOD));
    OS_REG_WRITE_FLUSH(ah);

    DISABLE_REG_WRITE_BUFFER

    /*
     * Configure the BMISS interrupt.  Note that we
     * assume the caller blocks interrupts while enabling
     * the threshold.
     */
    HALASSERT(bs->bs_bmissthreshold <=
        (AR_RSSI_THR_BM_THR >> AR_RSSI_THR_BM_THR_S));
    OS_REG_RMW_FIELD(ah, AR_RSSI_THR,
        AR_RSSI_THR_BM_THR, bs->bs_bmissthreshold);

    /*
     * Program the sleep registers to correlate with the beacon setup.
     */

    /*
     * Oahu beacons timers on the station were used for power
     * save operation (waking up in anticipation of a beacon)
     * and any CFP function; Venice does sleep/power-save timers
     * differently - so this is the right place to set them up;
     * don't think the beacon timers are used by venice sta hw
     * for any useful purpose anymore
     * Setup venice's sleep related timers
     * Current implementation assumes sw processing of beacons -
     *   assuming an interrupt is generated every beacon which
     *   causes the hardware to become awake until the sw tells
     *   it to go to sleep again; beacon timeout is to allow for
     *   beacon jitter; cab timeout is max time to wait for cab
     *   after seeing the last DTIM or MORE CAB bit
     */
#define CAB_TIMEOUT_VAL         10 /* in TU */
#define BEACON_TIMEOUT_VAL      10 /* in TU */
#define MIN_BEACON_TIMEOUT_VAL   1 /* in 1/8 TU */
#define SLEEP_SLOP               3 /* in TU */

    /*
     * For max powersave mode we may want to sleep for longer than a
     * beacon period and not want to receive all beacons; modify the
     * timers accordingly; make sure to align the next TIM to the
     * next DTIM if we decide to wake for DTIMs only
     */
    beaconintval = bs->bs_intval & HAL_BEACON_PERIOD;
    HALASSERT(beaconintval != 0);
    if (bs->bs_sleepduration > beaconintval) {
        HALASSERT(roundup(bs->bs_sleepduration, beaconintval) ==
                bs->bs_sleepduration);
        beaconintval = bs->bs_sleepduration;
    }
    dtimperiod = bs->bs_dtimperiod;
    if (bs->bs_sleepduration > dtimperiod) {
        HALASSERT(dtimperiod == 0 ||
            roundup(bs->bs_sleepduration, dtimperiod) ==
                bs->bs_sleepduration);
        dtimperiod = bs->bs_sleepduration;
    }
    HALASSERT(beaconintval <= dtimperiod);
    if (beaconintval == dtimperiod)
        nextTbtt = bs->bs_nextdtim;
    else
        nextTbtt = bs->bs_nexttbtt;

    HDPRINTF(ah, HAL_DBG_BEACON, "%s: next DTIM %d\n", __func__, bs->bs_nextdtim);
    HDPRINTF(ah, HAL_DBG_BEACON, "%s: next beacon %d\n", __func__, nextTbtt);
    HDPRINTF(ah, HAL_DBG_BEACON, "%s: beacon period %d\n", __func__, beaconintval);
    HDPRINTF(ah, HAL_DBG_BEACON, "%s: DTIM period %d\n", __func__, dtimperiod);

    ENABLE_REG_WRITE_BUFFER

    OS_REG_WRITE(ah, AR_NEXT_DTIM, TU_TO_USEC(bs->bs_nextdtim - SLEEP_SLOP));
    OS_REG_WRITE(ah, AR_NEXT_TIM, TU_TO_USEC(nextTbtt - SLEEP_SLOP));

    /* cab timeout is now in 1/8 TU */
    OS_REG_WRITE(ah, AR_SLEEP1,
        SM((CAB_TIMEOUT_VAL << 3), AR_SLEEP1_CAB_TIMEOUT)
        | AR_SLEEP1_ASSUME_DTIM);

    /* beacon timeout is now in 1/8 TU */
    if (pCap->halAutoSleepSupport) {
        beacontimeout = (BEACON_TIMEOUT_VAL << 3);
    }
    else {
        /* 
         * Use a very small value to make sure the timeout occurs before the TBTT.
         * In this case the chip will not go back to sleep automatically, instead
         * waiting for the SW to explicitly set it to that mode. 
         */
        beacontimeout = MIN_BEACON_TIMEOUT_VAL;
    }

    OS_REG_WRITE(ah, AR_SLEEP2,
        SM(beacontimeout, AR_SLEEP2_BEACON_TIMEOUT));

    OS_REG_WRITE(ah, AR_TIM_PERIOD, TU_TO_USEC(beaconintval));
    OS_REG_WRITE(ah, AR_DTIM_PERIOD, TU_TO_USEC(dtimperiod));
    OS_REG_WRITE_FLUSH(ah);

    DISABLE_REG_WRITE_BUFFER
      
    /* clear HOST AP related timers first */    
    OS_REG_CLR_BIT(ah, AR_TIMER_MODE, (AR_DBA_TIMER_EN | AR_SWBA_TIMER_EN));

    OS_REG_SET_BIT(ah, AR_TIMER_MODE, AR_TBTT_TIMER_EN | AR_TIM_TIMER_EN 
                    | AR_DTIM_TIMER_EN);
    /* TSF out of range threshold */
    OS_REG_WRITE(ah, AR_TSFOOR_THRESHOLD, bs->bs_tsfoor_threshold);
 
#undef CAB_TIMEOUT_VAL
#undef BEACON_TIMEOUT_VAL
#undef SLEEP_SLOP
}
#endif /* AH_SUPPORT_AR5416 */
