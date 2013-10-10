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

#ifdef AH_SUPPORT_AR5416

#include "ah.h"
#include "ah_internal.h"

#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"

typedef struct gen_timer_configuation {
    u_int32_t   next_addr;
    u_int32_t   period_addr;
    u_int32_t   mode_addr;
    u_int32_t   mode_mask;
}  GEN_TIMER_CONFIGURATION;

static const GEN_TIMER_CONFIGURATION genTimerConfiguration[] =
{
    {AR_NEXT_NDP_TIMER          , AR_NDP_PERIOD         , AR_TIMER_MODE     , 0x0080},
    {AR_NEXT_NDP_TIMER          , AR_NDP_PERIOD         , AR_TIMER_MODE     , 0x0080},
    {AR_NEXT_NDP_TIMER          , AR_NDP_PERIOD         , AR_TIMER_MODE     , 0x0080},
    {AR_NEXT_NDP_TIMER          , AR_NDP_PERIOD         , AR_TIMER_MODE     , 0x0080},
    {AR_NEXT_NDP_TIMER          , AR_NDP_PERIOD         , AR_TIMER_MODE     , 0x0080},
    {AR_NEXT_NDP_TIMER          , AR_NDP_PERIOD         , AR_TIMER_MODE     , 0x0080},
    {AR_NEXT_NDP_TIMER          , AR_NDP_PERIOD         , AR_TIMER_MODE     , 0x0080},
    {AR_NEXT_NDP_TIMER          , AR_NDP_PERIOD         , AR_TIMER_MODE     , 0x0080},
    {AR_NEXT_NDP2_TIMER         , AR_NDP2_PERIOD        , AR_NDP2_TIMER_MODE, 0x0001}, 
    {AR_NEXT_NDP2_TIMER + 1*4   , AR_NDP2_PERIOD + 1*4  , AR_NDP2_TIMER_MODE, 0x0002}, 
    {AR_NEXT_NDP2_TIMER + 2*4   , AR_NDP2_PERIOD + 2*4  , AR_NDP2_TIMER_MODE, 0x0004}, 
    {AR_NEXT_NDP2_TIMER + 3*4   , AR_NDP2_PERIOD + 3*4  , AR_NDP2_TIMER_MODE, 0x0008}, 
    {AR_NEXT_NDP2_TIMER + 4*4   , AR_NDP2_PERIOD + 4*4  , AR_NDP2_TIMER_MODE, 0x0010}, 
    {AR_NEXT_NDP2_TIMER + 5*4   , AR_NDP2_PERIOD + 5*4  , AR_NDP2_TIMER_MODE, 0x0020}, 
    {AR_NEXT_NDP2_TIMER + 6*4   , AR_NDP2_PERIOD + 6*4  , AR_NDP2_TIMER_MODE, 0x0040}, 
    {AR_NEXT_NDP2_TIMER + 7*4   , AR_NDP2_PERIOD + 7*4  , AR_NDP2_TIMER_MODE, 0x0080}
};

#define AR_GENTMR_BIT(_index)   (1 << (_index))

int
ar5416AllocGenericTimer(struct ath_hal *ah, HAL_GEN_TIMER_DOMAIN tsf)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    u_int32_t           i, mask;
    u_int32_t           availTimerStart, availTimerEnd;

    if (AR_SREV_KIWI(ah)) {
        if (tsf == HAL_GEN_TIMER_TSF) {
            availTimerStart = AR_FIRST_KIWI_NDP_TIMER;
            availTimerEnd = AR_GEN_TIMER_BANK_1_LEN;
        }
        else {
            availTimerStart = AR_GEN_TIMER_BANK_1_LEN;
            availTimerEnd = AR_NUM_GEN_TIMERS_POST_MERLIN;
        }
    }
    else {
        if (tsf == HAL_GEN_TIMER_TSF) {
            availTimerStart = AR_FIRST_NDP_TIMER;
            availTimerEnd = AR_NUM_GEN_TIMERS_POST_MERLIN;
        }
        else {
            /* Only Kiwi supports TSF2. */
            return -1;
        }
    }

    /* Find the first availabe timer index */
    i = availTimerStart;
    mask = ahp->ah_availGenTimers >> i;

    for ( ; mask && (i < availTimerEnd) ; mask >>= 1, i++ ) {
        if (mask & 0x1) {
            ahp->ah_availGenTimers &= ~(AR_GENTMR_BIT(i));

            if ((tsf == HAL_GEN_TIMER_TSF2) && !ahp->ah_enableTSF2) {
                ahp->ah_enableTSF2 = AH_TRUE;
                ar5416StartTsf2(ah);
            }
            return i;
        }
    }
    return -1;
}

void ar5416StartTsf2(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);

    if (ahp->ah_enableTSF2) {
        OS_REG_SET_BIT(ah, AR_DIRECT_CONNECT, AR_DC_AP_STA_EN);
        OS_REG_SET_BIT(ah, AR_RESET_TSF, AR_RESET_TSF2_ONCE);
    }
}

void
ar5416FreeGenericTimer(struct ath_hal *ah, int index)
{
    struct ath_hal_5416 *ahp = AH5416(ah);

    ar5416StopGenericTimer(ah, index);
    ahp->ah_availGenTimers |= AR_GENTMR_BIT(index);
}

void
ar5416StartGenericTimer(struct ath_hal *ah, int index, u_int32_t timer_next, u_int32_t timer_period)
{
    if ((index < AR_FIRST_NDP_TIMER) || (index >= AR_NUM_GEN_TIMERS_POST_MERLIN)) {
        return;
    }

    /*
     * Program generic timer registers
     */
    OS_REG_WRITE(ah, genTimerConfiguration[index].next_addr, timer_next);
    OS_REG_WRITE(ah, genTimerConfiguration[index].period_addr, timer_period);
    OS_REG_SET_BIT(ah, genTimerConfiguration[index].mode_addr, genTimerConfiguration[index].mode_mask);

    /* Enable both trigger and thresh interrupt masks */
    OS_REG_SET_BIT(ah, AR_IMR_S5,
                   (SM(AR_GENTMR_BIT(index), AR_IMR_S5_GENTIMER_THRESH) |
                    SM(AR_GENTMR_BIT(index), AR_IMR_S5_GENTIMER_TRIG)));
}

void
ar5416StopGenericTimer(struct ath_hal *ah, int index)
{
    if ((index < AR_FIRST_NDP_TIMER) || (index >= AR_NUM_GEN_TIMERS_POST_MERLIN)) {
        return;
    }

    /*
     * Clear generic timer enable bits.
     */
    OS_REG_CLR_BIT(ah, genTimerConfiguration[index].mode_addr, genTimerConfiguration[index].mode_mask);

    /* Disable both trigger and thresh interrupt masks */
    OS_REG_CLR_BIT(ah, AR_IMR_S5,
                   (SM(AR_GENTMR_BIT(index), AR_IMR_S5_GENTIMER_THRESH) |
                    SM(AR_GENTMR_BIT(index), AR_IMR_S5_GENTIMER_TRIG)));
}

void
ar5416GetGenTimerInterrupts(struct ath_hal *ah, u_int32_t *trigger, u_int32_t *thresh)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    *trigger = ahp->ah_intrGenTimerTrigger;
    *thresh = ahp->ah_intrGenTimerThresh;
}

#endif /* AH_SUPPORT_AR5416 */
