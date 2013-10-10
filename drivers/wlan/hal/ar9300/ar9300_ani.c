/*
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

#ifdef AH_SUPPORT_AR9300

#include "ah.h"
#include "ah_internal.h"
#include "ah_desc.h"
#include "ah_pktlog.h"

#include "ar9300/ar9300.h"
#include "ar9300/ar9300reg.h"
#include "ar9300/ar9300phy.h"

extern  void ar9300SetRxFilter(struct ath_hal *ah, u_int32_t bits);
extern  u_int32_t ar9300GetRxFilter(struct ath_hal *ah);

#define HAL_ANI_DEBUG 0

/*
 * Anti noise immunity support.  We track phy errors and react
 * to excessive errors by adjusting the noise immunity parameters.
 */

/******************************************************************************
 *
 * New Ani Algorithm for Station side only
 *
 *****************************************************************************/

#define HAL_ANI_OFDM_TRIG_HIGH        1000 /* units are errors per second */
#define HAL_ANI_OFDM_TRIG_LOW        400  /* units are errors per second */
#define HAL_ANI_CCK_TRIG_HIGH        600  /* units are errors per second */
#define HAL_ANI_CCK_TRIG_LOW        300  /* units are errors per second */
#define HAL_ANI_USE_OFDM_WEAK_SIG    AH_TRUE
#define HAL_ANI_ENABLE_MRC_CCK      AH_TRUE /* default is enabled */
#define HAL_ANI_DEF_SPUR_IMMUNE_LVL    3
#define HAL_ANI_DEF_FIRSTEP_LVL        2
#define HAL_ANI_RSSI_THR_HIGH        40
#define HAL_ANI_RSSI_THR_LOW        7
#define HAL_ANI_PERIOD                1000

#define HAL_NOISE_DETECT_PERIOD     100
#define HAL_NOISE_RECOVER_PERIOD    5000

#define HAL_SIG_FIRSTEP_SETTING_MIN         0
#define HAL_SIG_FIRSTEP_SETTING_MAX        20
#define HAL_SIG_SPUR_IMM_SETTING_MIN     0
#define HAL_SIG_SPUR_IMM_SETTING_MAX    22

#define HAL_EP_RND(x, mul) \
    ((((x) % (mul)) >= ((mul) / 2)) ? ((x) + ((mul) - 1)) / (mul) : (x) / (mul))
#define    BEACON_RSSI(ahp) \
    HAL_EP_RND(ahp->ah_stats.ast_nodestats.ns_avgbrssi, \
        HAL_RSSI_EP_MULTIPLIER)

typedef int TABLE[];
//                             level:    0   1   2   3   4   5   6   7   8
// firstep_table:    lvl 0-8, default 2
static const TABLE firstep_table    = { -4, -2,  0,  2,  4,  6,  8, 10, 12};
// cycpwrThr1_table: lvl 0-7, default 3
static const TABLE cycpwrThr1_table = { -6, -4, -2,  0,  2,  4,  6,  8 };
// values here are relative to the INI

typedef struct _HAL_ANI_OFDM_LEVEL_ENTRY {
    int spur_immunity_level;
    int fir_step_level;
    int ofdm_weak_signal_on;
} HAL_ANI_OFDM_LEVEL_ENTRY;
static const HAL_ANI_OFDM_LEVEL_ENTRY ofdm_level_table[] = {
//     SI  FS  WS
     {  0,  0,  1  }, // lvl 0
     {  1,  1,  1  }, // lvl 1
     {  2,  2,  1  }, // lvl 2
     {  3,  2,  1  }, // lvl 3  (default)
     {  4,  3,  1  }, // lvl 4
     {  5,  4,  1  }, // lvl 5
     {  6,  5,  1  }, // lvl 6
     {  7,  6,  1  }, // lvl 7
     {  7,  7,  1  }, // lvl 8
     {  7,  8,  0  }  // lvl 9
};
#define HAL_ANI_OFDM_NUM_LEVEL \
    (sizeof(ofdm_level_table) / sizeof(ofdm_level_table[0]))
#define HAL_ANI_OFDM_MAX_LEVEL  (HAL_ANI_OFDM_NUM_LEVEL - 1)
#define HAL_ANI_OFDM_DEF_LEVEL  3  // default level - matches the INI settings

typedef struct _HAL_ANI_CCK_LEVEL_ENTRY {
    int fir_step_level;
    int mrc_cck_on;
} HAL_ANI_CCK_LEVEL_ENTRY;
static const HAL_ANI_CCK_LEVEL_ENTRY cck_level_table[] = {
//     FS  MRC-CCK
     {  0,  1  },  // lvl 0
     {  1,  1  },  // lvl 1
     {  2,  1  },  // lvl 2  (default)
     {  3,  1  },  // lvl 3
     {  4,  0  },  // lvl 4
     {  5,  0  },  // lvl 5
     {  6,  0  },  // lvl 6
     {  7,  0  },  // lvl 7 (only for high rssi)
     {  8,  0  }   // lvl 8 (only for high rssi)
};
#define HAL_ANI_CCK_NUM_LEVEL \
    (sizeof(cck_level_table) / sizeof(cck_level_table[0]))
#define HAL_ANI_CCK_MAX_LEVEL           (HAL_ANI_CCK_NUM_LEVEL - 1)
#define HAL_ANI_CCK_MAX_LEVEL_LOW_RSSI  (HAL_ANI_CCK_NUM_LEVEL - 3)
#define HAL_ANI_CCK_DEF_LEVEL  2  // default level - matches the INI settings

/*
 * register values to turn OFDM weak signal detection OFF
 */
static const int m1ThreshLow_off    = 127;
static const int m2ThreshLow_off    = 127;
static const int m1Thresh_off       = 127;
static const int m2Thresh_off       = 127;
static const int m2CountThr_off     =  31;
static const int m2CountThrLow_off  =  63;
static const int m1ThreshLowExt_off = 127;
static const int m2ThreshLowExt_off = 127;
static const int m1ThreshExt_off    = 127;
static const int m2ThreshExt_off    = 127;

void
ar9300EnableMIBCounters(struct ath_hal *ah)
{
    HDPRINTF(ah, HAL_DBG_RESET, "%s: Enable MIB counters\n", __func__);
    /* Clear the mib counters and save them in the stats */
    ar9300UpdateMibMacStats(ah);

    OS_REG_WRITE(ah, AR_FILT_OFDM, 0);
    OS_REG_WRITE(ah, AR_FILT_CCK, 0);
    OS_REG_WRITE(ah, AR_MIBC,
        ~(AR_MIBC_COW | AR_MIBC_FMC | AR_MIBC_CMC | AR_MIBC_MCS) & 0x0f);
    OS_REG_WRITE(ah, AR_PHY_ERR_MASK_1, AR_PHY_ERR_OFDM_TIMING);
    OS_REG_WRITE(ah, AR_PHY_ERR_MASK_2, AR_PHY_ERR_CCK_TIMING);

}

void
ar9300DisableMIBCounters(struct ath_hal *ah)
{
    HDPRINTF(ah, HAL_DBG_RESET, "%s: Disabling MIB counters\n", __func__);

    OS_REG_WRITE(ah, AR_MIBC,  AR_MIBC_FMC | AR_MIBC_CMC);

    /* Clear the mib counters and save them in the stats */
    ar9300UpdateMibMacStats(ah);

    OS_REG_WRITE(ah, AR_FILT_OFDM, 0);
    OS_REG_WRITE(ah, AR_FILT_CCK, 0);
}

/*
 * This routine returns the index into the aniState array that
 * corresponds to the channel in *chan.  If no match is found and the
 * array is still not fully utilized, a new entry is created for the
 * channel.  We assume the attach function has already initialized the
 * ah_ani values and only the channel field needs to be set.
 */
static int
ar9300GetAniChannelIndex(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{
#define N(a)     (sizeof(a) / sizeof(a[0]))
    struct ath_hal_9300 *ahp = AH9300(ah);
    int i;

    for (i = 0; i < N(ahp->ah_ani); i++) {
        if (ahp->ah_ani[i].c.channel == chan->channel) {
            return i;
        }
        if (ahp->ah_ani[i].c.channel == 0) {
            ahp->ah_ani[i].c.channel = chan->channel;
            ahp->ah_ani[i].c.channelFlags = chan->channelFlags;
            return i;
        }
    }
    /* XXX statistic */
    HDPRINTF(ah, HAL_DBG_UNMASKABLE,
        "%s: No more channel states left. Using channel 0\n", __func__);
    return 0;        /* XXX gotta return something valid */
#undef N
}

/*
 * Return the current ANI state of the channel we're on
 */
struct ar9300AniState *
ar9300AniGetCurrentState(struct ath_hal *ah)
{
    return AH9300(ah)->ah_curani;
}

/*
 * Return the current statistics.
 */
struct ar9300Stats *
ar9300AniGetCurrentStats(struct ath_hal *ah)
{
    return &AH9300(ah)->ah_stats;
}

/*
 * Setup ANI handling.  Sets all thresholds and levels to default level AND
 * resets the channel statistics
 */

void
ar9300AniAttach(struct ath_hal *ah)
{
#define N(a)     (sizeof(a) / sizeof(a[0]))
    struct ath_hal_9300 *ahp = AH9300(ah);
    int i;

    /* owl has phy counters */
    ahp->ah_hasHwPhyCounters = 1;

    OS_MEMZERO(ahp->ah_ani, sizeof(ahp->ah_ani));
    for (i = 0; i < N(ahp->ah_ani); i++) {
        ahp->ah_ani[i].ofdmTrigHigh = HAL_ANI_OFDM_TRIG_HIGH;
        ahp->ah_ani[i].ofdmTrigLow = HAL_ANI_OFDM_TRIG_LOW;
        ahp->ah_ani[i].cckTrigHigh = HAL_ANI_CCK_TRIG_HIGH;
        ahp->ah_ani[i].cckTrigLow = HAL_ANI_CCK_TRIG_LOW;
        ahp->ah_ani[i].rssiThrHigh = HAL_ANI_RSSI_THR_HIGH;
        ahp->ah_ani[i].rssiThrLow = HAL_ANI_RSSI_THR_LOW;
        ahp->ah_ani[i].ofdmNoiseImmunityLevel = HAL_ANI_OFDM_DEF_LEVEL;
        ahp->ah_ani[i].cckNoiseImmunityLevel = HAL_ANI_CCK_DEF_LEVEL;
        ahp->ah_ani[i].ofdmWeakSigDetectOff = !HAL_ANI_USE_OFDM_WEAK_SIG;
        ahp->ah_ani[i].spurImmunityLevel = HAL_ANI_DEF_SPUR_IMMUNE_LVL;
        ahp->ah_ani[i].firstepLevel = HAL_ANI_DEF_FIRSTEP_LVL;
        ahp->ah_ani[i].mrcCCKOff = !HAL_ANI_ENABLE_MRC_CCK;
        ahp->ah_ani[i].ofdmsTurn = AH_TRUE;
        ahp->ah_ani[i].mustRestore = AH_FALSE;
        if (ahp->ah_hasHwPhyCounters) {
            ahp->ah_ani[i].ofdmPhyErrBase = 0;
            ahp->ah_ani[i].cckPhyErrBase = 0;
        }
    }

    /*
     * Since we expect some ongoing maintenance on the tables,
     * let's sanity check here.
     * The default level should not modify INI setting.
     */
    HALASSERT(firstep_table[HAL_ANI_DEF_FIRSTEP_LVL] == 0);
    HALASSERT(cycpwrThr1_table[HAL_ANI_DEF_SPUR_IMMUNE_LVL] == 0);
    HALASSERT(
        ofdm_level_table[HAL_ANI_OFDM_DEF_LEVEL].fir_step_level ==
        HAL_ANI_DEF_FIRSTEP_LVL);
    HALASSERT(
        ofdm_level_table[HAL_ANI_OFDM_DEF_LEVEL].spur_immunity_level ==
        HAL_ANI_DEF_SPUR_IMMUNE_LVL);
    HALASSERT(
        cck_level_table[HAL_ANI_CCK_DEF_LEVEL].fir_step_level ==
        HAL_ANI_DEF_FIRSTEP_LVL);

    if (ahp->ah_hasHwPhyCounters) {
        HDPRINTF(ah, HAL_DBG_ANI,
            "Setting OfdmErrBase = 0x%08x\n", ahp->ah_ani[0].ofdmPhyErrBase);
        HDPRINTF(ah, HAL_DBG_ANI,
            "Setting cckErrBase = 0x%08x\n", ahp->ah_ani[0].cckPhyErrBase);
        /* Enable MIB Counters */
        OS_REG_WRITE(ah, AR_PHY_ERR_1, ahp->ah_ani[0].ofdmPhyErrBase);
        OS_REG_WRITE(ah, AR_PHY_ERR_2, ahp->ah_ani[0].cckPhyErrBase);
        ar9300EnableMIBCounters(ah);
    }
    ahp->ah_aniPeriod = HAL_ANI_PERIOD;
    if (AH_PRIVATE(ah)->ah_config.ath_hal_enableANI) {
        ahp->ah_procPhyErr |= HAL_PROCESS_ANI;
    }
#undef N
}

/*
 * Cleanup any ANI state setup.
 */
void
ar9300AniDetach(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);

    HDPRINTF(ah, HAL_DBG_ANI, "%s: Detaching Ani\n", __func__);
    if (ahp->ah_hasHwPhyCounters) {
        ar9300DisableMIBCounters(ah);
        OS_REG_WRITE(ah, AR_PHY_ERR_1, 0);
        OS_REG_WRITE(ah, AR_PHY_ERR_2, 0);
    }
}


#define OS_GET_FIELD(v, _f) (((v & _f) >> _f##_S))

/*
 * Initialize the ANI register values with default (ini) values.
 * This routine is called during a (full) hardware reset after
 * all the registers are initialised from the INI.
 */
void
ar9300AniInitDefaults(struct ath_hal *ah, HAL_HT_MACMODE macmode)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ar9300AniState *aniState;
    HAL_CHANNEL_INTERNAL *chan = AH_PRIVATE(ah)->ah_curchan;
    int index;
    u_int32_t val;

    HALASSERT(chan != AH_NULL);
    index = ar9300GetAniChannelIndex(ah, chan);
    aniState = &ahp->ah_ani[index];
    ahp->ah_curani = aniState;

    HDPRINTF(ah, HAL_DBG_ANI,
        "%s: ver %d.%d opmode %u chan %d Mhz/0x%x macmode %d\n",
        __func__, AH_PRIVATE(ah)->ah_macVersion, AH_PRIVATE(ah)->ah_macRev,
        AH_PRIVATE(ah)->ah_opmode, chan->channel, chan->channelFlags, macmode);

    val = OS_REG_READ(ah, AR_PHY_SFCORR);
    aniState->iniDef.m1Thresh = OS_GET_FIELD(val, AR_PHY_SFCORR_M1_THRESH);
    aniState->iniDef.m2Thresh = OS_GET_FIELD(val, AR_PHY_SFCORR_M2_THRESH);
    aniState->iniDef.m2CountThr =
        OS_GET_FIELD(val, AR_PHY_SFCORR_M2COUNT_THR);
    val = OS_REG_READ(ah, AR_PHY_SFCORR_LOW);
    aniState->iniDef.m1ThreshLow =
        OS_GET_FIELD(val, AR_PHY_SFCORR_LOW_M1_THRESH_LOW);
    aniState->iniDef.m2ThreshLow =
        OS_GET_FIELD(val, AR_PHY_SFCORR_LOW_M2_THRESH_LOW);
    aniState->iniDef.m2CountThrLow =
        OS_GET_FIELD(val, AR_PHY_SFCORR_LOW_M2COUNT_THR_LOW);
    val = OS_REG_READ(ah, AR_PHY_SFCORR_EXT);
    aniState->iniDef.m1ThreshExt =
        OS_GET_FIELD(val, AR_PHY_SFCORR_EXT_M1_THRESH);
    aniState->iniDef.m2ThreshExt =
        OS_GET_FIELD(val, AR_PHY_SFCORR_EXT_M2_THRESH);
    aniState->iniDef.m1ThreshLowExt =
        OS_GET_FIELD(val, AR_PHY_SFCORR_EXT_M1_THRESH_LOW);
    aniState->iniDef.m2ThreshLowExt =
        OS_GET_FIELD(val, AR_PHY_SFCORR_EXT_M2_THRESH_LOW);
    aniState->iniDef.firstep =
        OS_REG_READ_FIELD(ah, AR_PHY_FIND_SIG, AR_PHY_FIND_SIG_FIRSTEP);
    aniState->iniDef.firstepLow =
        OS_REG_READ_FIELD(
            ah, AR_PHY_FIND_SIG_LOW, AR_PHY_FIND_SIG_LOW_FIRSTEP_LOW);
    aniState->iniDef.cycpwrThr1 =
        OS_REG_READ_FIELD(ah, AR_PHY_TIMING5, AR_PHY_TIMING5_CYCPWR_THR1);
    aniState->iniDef.cycpwrThr1Ext =
        OS_REG_READ_FIELD(ah, AR_PHY_EXT_CCA, AR_PHY_EXT_CYCPWR_THR1);

    // these levels just got reset to defaults by the INI
    aniState->spurImmunityLevel = HAL_ANI_DEF_SPUR_IMMUNE_LVL;
    aniState->firstepLevel = HAL_ANI_DEF_FIRSTEP_LVL;
    aniState->ofdmWeakSigDetectOff = !HAL_ANI_USE_OFDM_WEAK_SIG;
    aniState->mrcCCKOff = !HAL_ANI_ENABLE_MRC_CCK;

    aniState->cycleCount = 0;
}

/*
 * Set the ANI settings to match an OFDM level.
 */
static void
ar9300AniSetOdfmNoiseImmunityLevel(struct ath_hal *ah, u_int8_t immunityLevel)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ar9300AniState *aniState = ahp->ah_curani;
    u_int8_t ofdmNoiseImmunityLevel;

    aniState->rssi = BEACON_RSSI(ahp);
    HDPRINTF(ah, HAL_DBG_ANI,
        "**** %s: ofdmlevel %d=>%d, rssi=%d[lo=%d hi=%d]\n",
        __func__, aniState->ofdmNoiseImmunityLevel, immunityLevel,
        aniState->rssi, aniState->rssiThrLow, aniState->rssiThrHigh);

    ofdmNoiseImmunityLevel = aniState->ofdmNoiseImmunityLevel = immunityLevel;

    if (aniState->spurImmunityLevel !=
        ofdm_level_table[ofdmNoiseImmunityLevel].spur_immunity_level)
    {
        ar9300AniControl(
            ah, HAL_ANI_SPUR_IMMUNITY_LEVEL,
            ofdm_level_table[ofdmNoiseImmunityLevel].spur_immunity_level);
    }

    if (aniState->firstepLevel !=
            ofdm_level_table[ofdmNoiseImmunityLevel].fir_step_level &&
        ofdm_level_table[ofdmNoiseImmunityLevel].fir_step_level >=
            cck_level_table[aniState->cckNoiseImmunityLevel].fir_step_level)
    {
        ar9300AniControl(
            ah, HAL_ANI_FIRSTEP_LEVEL,
            ofdm_level_table[ofdmNoiseImmunityLevel].fir_step_level);
    }

    if ((AH_PRIVATE(ah)->ah_opmode != HAL_M_STA ||
        aniState->rssi <= aniState->rssiThrHigh))
    {
        if (aniState->ofdmWeakSigDetectOff) {
            /*
             * force on ofdm weak sig detect.
             */
            ar9300AniControl(ah, HAL_ANI_OFDM_WEAK_SIGNAL_DETECTION, AH_TRUE);
        }
    } else if (aniState->ofdmWeakSigDetectOff ==
               ofdm_level_table[ofdmNoiseImmunityLevel].ofdm_weak_signal_on) {
        ar9300AniControl(
            ah, HAL_ANI_OFDM_WEAK_SIGNAL_DETECTION,
            ofdm_level_table[ofdmNoiseImmunityLevel].ofdm_weak_signal_on);
    }
}

/*
 * Set the ANI settings to match an CCK level.
 */
static void
ar9300AniSetCckNoiseImmunityLevel(struct ath_hal *ah, u_int8_t immunityLevel)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ar9300AniState *aniState = ahp->ah_curani;
    u_int8_t cckNoiseImmunityLevel;

    aniState->rssi = BEACON_RSSI(ahp);
    HDPRINTF(ah, HAL_DBG_ANI,
        "**** %s: ccklevel %d=>%d, rssi=%d[lo=%d hi=%d]\n",
        __func__, aniState->cckNoiseImmunityLevel, immunityLevel,
        aniState->rssi, aniState->rssiThrLow, aniState->rssiThrHigh);

    if (AH_PRIVATE(ah)->ah_opmode == HAL_M_STA &&
        aniState->rssi <= aniState->rssiThrLow &&
        immunityLevel > HAL_ANI_CCK_MAX_LEVEL_LOW_RSSI)
    {
        immunityLevel = HAL_ANI_CCK_MAX_LEVEL_LOW_RSSI;
    }

    cckNoiseImmunityLevel = aniState->cckNoiseImmunityLevel = immunityLevel;

    if (aniState->firstepLevel !=
            cck_level_table[cckNoiseImmunityLevel].fir_step_level &&
        cck_level_table[cckNoiseImmunityLevel].fir_step_level >=
            ofdm_level_table[aniState->ofdmNoiseImmunityLevel].fir_step_level)
    {
        ar9300AniControl(
            ah, HAL_ANI_FIRSTEP_LEVEL,
            cck_level_table[cckNoiseImmunityLevel].fir_step_level);
    }

    if (aniState->mrcCCKOff ==
        cck_level_table[cckNoiseImmunityLevel].mrc_cck_on)
    {
        ar9300AniControl(
            ah, HAL_ANI_MRC_CCK,
            cck_level_table[cckNoiseImmunityLevel].mrc_cck_on);
    }
}

/*
 * Control Adaptive Noise Immunity Parameters
 */
HAL_BOOL
ar9300AniControl(struct ath_hal *ah, HAL_ANI_CMD cmd, int param)
{
#define N(a) (sizeof(a) / sizeof(a[0]))
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ar9300AniState *aniState = ahp->ah_curani;
    HAL_CHANNEL_INTERNAL *chan = AH_PRIVATE(ah)->ah_curchan;
    int32_t value, value2;
    u_int level = param;
    u_int is_on;

    if (chan == NULL && cmd != HAL_ANI_MODE) {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE,
            "%s: ignoring cmd 0x%02x - no channel\n", __func__, cmd);
        return AH_FALSE;
    }

    switch (cmd & ahp->ah_ani_function) {
    case HAL_ANI_OFDM_WEAK_SIGNAL_DETECTION: 
        {
            int m1ThreshLow, m2ThreshLow;
            int m1Thresh, m2Thresh;
            int m2CountThr, m2CountThrLow;
            int m1ThreshLowExt, m2ThreshLowExt;
            int m1ThreshExt, m2ThreshExt;
            /*
             * is_on == 1 means ofdm weak signal detection is ON
             * (default, less noise imm)
             * is_on == 0 means ofdm weak signal detection is OFF
             * (more noise imm)
             */
            is_on = param ? 1 : 0;

            /*
             * make register setting for default (weak sig detect ON)
             * come from INI file
             */
            m1ThreshLow    = is_on ?
                aniState->iniDef.m1ThreshLow    : m1ThreshLow_off;
            m2ThreshLow    = is_on ?
                aniState->iniDef.m2ThreshLow    : m2ThreshLow_off;
            m1Thresh       = is_on ?
                aniState->iniDef.m1Thresh       : m1Thresh_off;
            m2Thresh       = is_on ?
                aniState->iniDef.m2Thresh       : m2Thresh_off;
            m2CountThr     = is_on ?
                aniState->iniDef.m2CountThr     : m2CountThr_off;
            m2CountThrLow  = is_on ?
                aniState->iniDef.m2CountThrLow  : m2CountThrLow_off;
            m1ThreshLowExt = is_on ?
                aniState->iniDef.m1ThreshLowExt : m1ThreshLowExt_off;
            m2ThreshLowExt = is_on ?
                aniState->iniDef.m2ThreshLowExt : m2ThreshLowExt_off;
            m1ThreshExt    = is_on ?
                aniState->iniDef.m1ThreshExt    : m1ThreshExt_off;
            m2ThreshExt    = is_on ?
                aniState->iniDef.m2ThreshExt    : m2ThreshExt_off;
            OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_LOW,
                AR_PHY_SFCORR_LOW_M1_THRESH_LOW, m1ThreshLow);
            OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_LOW,
                AR_PHY_SFCORR_LOW_M2_THRESH_LOW, m2ThreshLow);
            OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR, AR_PHY_SFCORR_M1_THRESH,
                m1Thresh);
            OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR, AR_PHY_SFCORR_M2_THRESH,
                m2Thresh);
            OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR, AR_PHY_SFCORR_M2COUNT_THR,
                m2CountThr);
            OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_LOW,
                AR_PHY_SFCORR_LOW_M2COUNT_THR_LOW, m2CountThrLow);
            OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_EXT,
                AR_PHY_SFCORR_EXT_M1_THRESH_LOW, m1ThreshLowExt);
            OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_EXT,
                AR_PHY_SFCORR_EXT_M2_THRESH_LOW, m2ThreshLowExt);
            OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_EXT, AR_PHY_SFCORR_EXT_M1_THRESH,
                m1ThreshExt);
            OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_EXT, AR_PHY_SFCORR_EXT_M2_THRESH,
                m2ThreshExt);
            if (is_on) {
                OS_REG_SET_BIT(ah, AR_PHY_SFCORR_LOW,
                    AR_PHY_SFCORR_LOW_USE_SELF_CORR_LOW);
            } else {
                OS_REG_CLR_BIT(ah, AR_PHY_SFCORR_LOW,
                    AR_PHY_SFCORR_LOW_USE_SELF_CORR_LOW);
            }
            if (!is_on != aniState->ofdmWeakSigDetectOff) {
                HDPRINTF(ah, HAL_DBG_ANI,
                    "%s: ** ch %d: ofdm weak signal: %s=>%s\n",
                    __func__, chan->channel,
                    !aniState->ofdmWeakSigDetectOff ? "on" : "off",
                    is_on ? "on" : "off");
                if (is_on) {
                    ahp->ah_stats.ast_ani_ofdmon++;
                } else {
                    ahp->ah_stats.ast_ani_ofdmoff++;
                }
                aniState->ofdmWeakSigDetectOff = !is_on;
            }
            break;
        }
    case HAL_ANI_FIRSTEP_LEVEL:
        if (level >= N(firstep_table)) {
            HDPRINTF(ah, HAL_DBG_UNMASKABLE,
                "%s: HAL_ANI_FIRSTEP_LEVEL level out of range (%u > %u)\n",
                __func__, level, (unsigned) N(firstep_table));
            return AH_FALSE;
        }
        /*
         * make register setting relative to default
         * from INI file & cap value
         */
        value =
            firstep_table[level] -
            firstep_table[HAL_ANI_DEF_FIRSTEP_LVL] +
            aniState->iniDef.firstep;
        if (value < HAL_SIG_FIRSTEP_SETTING_MIN) {
            value = HAL_SIG_FIRSTEP_SETTING_MIN;
        }
        if (value > HAL_SIG_FIRSTEP_SETTING_MAX) {
            value = HAL_SIG_FIRSTEP_SETTING_MAX;
        }
        OS_REG_RMW_FIELD(ah, AR_PHY_FIND_SIG, AR_PHY_FIND_SIG_FIRSTEP, value);
        /*
         * we need to set first step low register too
         * make register setting relative to default from INI file & cap value
         */
        value2 =
            firstep_table[level] -
            firstep_table[HAL_ANI_DEF_FIRSTEP_LVL] +
            aniState->iniDef.firstepLow;
        if (value2 < HAL_SIG_FIRSTEP_SETTING_MIN) {
            value2 = HAL_SIG_FIRSTEP_SETTING_MIN;
        }
        if (value2 > HAL_SIG_FIRSTEP_SETTING_MAX) {
            value2 = HAL_SIG_FIRSTEP_SETTING_MAX;
        }
        OS_REG_RMW_FIELD(ah, AR_PHY_FIND_SIG_LOW,
            AR_PHY_FIND_SIG_LOW_FIRSTEP_LOW, value2);

        if (level != aniState->firstepLevel) {
            HDPRINTF(ah, HAL_DBG_ANI,
                "%s: ** ch %d: level %d=>%d[def:%d] firstep[level]=%d ini=%d\n",
                __func__, chan->channel, aniState->firstepLevel, level,
                HAL_ANI_DEF_FIRSTEP_LVL, value, aniState->iniDef.firstep);
            HDPRINTF(ah, HAL_DBG_ANI,
                "%s: ** ch %d: level %d=>%d[def:%d] "
                "firstep_low[level]=%d ini=%d\n",
                __func__, chan->channel, aniState->firstepLevel, level,
                HAL_ANI_DEF_FIRSTEP_LVL, value2, aniState->iniDef.firstepLow);
            if (level > aniState->firstepLevel) {
                ahp->ah_stats.ast_ani_stepup++;
            } else if (level < aniState->firstepLevel) {
                ahp->ah_stats.ast_ani_stepdown++;
            }
            aniState->firstepLevel = level;
        }
        break;
    case HAL_ANI_SPUR_IMMUNITY_LEVEL:
        if (level >= N(cycpwrThr1_table)) {
            HDPRINTF(ah, HAL_DBG_UNMASKABLE,
                "%s: HAL_ANI_SPUR_IMMUNITY_LEVEL level "
                "out of range (%u > %u)\n",
                __func__, level, (unsigned) N(cycpwrThr1_table));
            return AH_FALSE;
        }
        /*
         * make register setting relative to default from INI file & cap value
         */
        value =
            cycpwrThr1_table[level] -
            cycpwrThr1_table[HAL_ANI_DEF_SPUR_IMMUNE_LVL] +
            aniState->iniDef.cycpwrThr1;
        if (value < HAL_SIG_SPUR_IMM_SETTING_MIN) {
            value = HAL_SIG_SPUR_IMM_SETTING_MIN;
        }
        if (value > HAL_SIG_SPUR_IMM_SETTING_MAX) {
            value = HAL_SIG_SPUR_IMM_SETTING_MAX;
        }
        OS_REG_RMW_FIELD(ah, AR_PHY_TIMING5, AR_PHY_TIMING5_CYCPWR_THR1, value);

        // set AR_PHY_EXT_CCA for extension channel
        // make register setting relative to default from INI file & cap value
        value2 =
            cycpwrThr1_table[level] -
            cycpwrThr1_table[HAL_ANI_DEF_SPUR_IMMUNE_LVL] +
            aniState->iniDef.cycpwrThr1Ext;
        if (value2 < HAL_SIG_SPUR_IMM_SETTING_MIN) {
            value2 = HAL_SIG_SPUR_IMM_SETTING_MIN;
        }
        if (value2 > HAL_SIG_SPUR_IMM_SETTING_MAX) {
            value2 = HAL_SIG_SPUR_IMM_SETTING_MAX;
        }
        OS_REG_RMW_FIELD(ah, AR_PHY_EXT_CCA, AR_PHY_EXT_CYCPWR_THR1, value2);

        if (level != aniState->spurImmunityLevel) {
            HDPRINTF(ah, HAL_DBG_ANI,
                "%s: ** ch %d: level %d=>%d[def:%d] "
                "cycpwrThr1[level]=%d ini=%d\n",
                __func__, chan->channel, aniState->spurImmunityLevel, level,
                HAL_ANI_DEF_SPUR_IMMUNE_LVL, value,
                aniState->iniDef.cycpwrThr1);
            HDPRINTF(ah, HAL_DBG_ANI,
                "%s: ** ch %d: level %d=>%d[def:%d] "
                "cycpwrThr1Ext[level]=%d ini=%d\n",
                __func__, chan->channel, aniState->spurImmunityLevel, level,
                HAL_ANI_DEF_SPUR_IMMUNE_LVL, value2,
                aniState->iniDef.cycpwrThr1Ext);
            if (level > aniState->spurImmunityLevel) {
                ahp->ah_stats.ast_ani_spurup++;
            } else if (level < aniState->spurImmunityLevel) {
                ahp->ah_stats.ast_ani_spurdown++;
            }
            aniState->spurImmunityLevel = level;
        }
        break;
    case HAL_ANI_MRC_CCK:
        /*
         * is_on == 1 means MRC CCK ON (default, less noise imm)
         * is_on == 0 means MRC CCK is OFF (more noise imm)
         */
        is_on = param ? 1 : 0;
        if (!AR_SREV_POSEIDON(ah)) {
            OS_REG_RMW_FIELD(ah, AR_PHY_MRC_CCK_CTRL,
                AR_PHY_MRC_CCK_ENABLE, is_on);
            OS_REG_RMW_FIELD(ah, AR_PHY_MRC_CCK_CTRL,
                AR_PHY_MRC_CCK_MUX_REG, is_on);
        }
        if (!is_on != aniState->mrcCCKOff) {
            HDPRINTF(ah, HAL_DBG_ANI,
                "%s: ** ch %d: MRC CCK: %s=>%s\n", __func__, chan->channel,
                !aniState->mrcCCKOff ? "on" : "off", is_on ? "on" : "off");
            if (is_on) {
                ahp->ah_stats.ast_ani_ccklow++;
            } else {
                ahp->ah_stats.ast_ani_cckhigh++;
            }
            aniState->mrcCCKOff = !is_on;
        }
        break;
    case HAL_ANI_PRESENT:
        break;
#ifdef AH_PRIVATE_DIAG
    case HAL_ANI_MODE:
        if (param == 0) {
            ahp->ah_procPhyErr &= ~HAL_PROCESS_ANI;
            /* Turn off HW counters if we have them */
            ar9300AniDetach(ah);
            /* Turn off phy error interrupts if we enabled them */
            if (!ahp->ah_hasHwPhyCounters) {
                ar9300SetRxFilter(
                    ah, ar9300GetRxFilter(ah) &~ HAL_RX_FILTER_PHYERR);
            }
            if (AH_PRIVATE(ah)->ah_curchan == NULL) {
                return AH_TRUE;
            }
            // if we're turning off ANI, reset regs back to INI settings    
            if (AH_PRIVATE(ah)->ah_config.ath_hal_enableANI) {
                HAL_ANI_CMD savefunc = ahp->ah_ani_function;
                // temporarly allow all functions so we can reset
                ahp->ah_ani_function = HAL_ANI_ALL;
                HDPRINTF(ah, HAL_DBG_ANI,
                    "%s: disable all ANI functions\n", __func__);
                ar9300AniSetOdfmNoiseImmunityLevel(ah, HAL_ANI_OFDM_DEF_LEVEL);
                ar9300AniSetCckNoiseImmunityLevel(ah, HAL_ANI_CCK_DEF_LEVEL);
                ahp->ah_ani_function = savefunc;
            }
        } else {            /* normal/auto mode */
            HDPRINTF(ah, HAL_DBG_ANI, "%s: enabled\n", __func__);
            ahp->ah_procPhyErr |= HAL_PROCESS_ANI;
            if (AH_PRIVATE(ah)->ah_curchan == NULL) {
                return AH_TRUE;
            }
            ar9300EnableMIBCounters(ah);
            ar9300AniReset(ah, AH_FALSE);
            aniState = ahp->ah_curani;
        }
        HDPRINTF(ah, HAL_DBG_ANI, "5 ANC: ahp->ah_procPhyErr %x \n",
                 ahp->ah_procPhyErr);
        break;
    case HAL_ANI_PHYERR_RESET:
        ahp->ah_stats.ast_ani_ofdmerrs = 0;
        ahp->ah_stats.ast_ani_cckerrs = 0;
        break;
#endif /* AH_PRIVATE_DIAG */
    default:
#if HAL_ANI_DEBUG
        HDPRINTF(ah, HAL_DBG_ANI,
            "%s: invalid cmd 0x%02x (allowed=0x%02x)\n",
            __func__, cmd, ahp->ah_ani_function);
#endif
        return AH_FALSE;
    }

#if HAL_ANI_DEBUG
    HDPRINTF(ah, HAL_DBG_ANI,
        "%s: ANI parameters: SI=%d, ofdmWS=%s FS=%d MRCcck=%s listenTime=%d "
        "CC=%d listen=%d ofdmErrs=%d cckErrs=%d\n",
        __func__, aniState->spurImmunityLevel,
        !aniState->ofdmWeakSigDetectOff ? "on" : "off",
        aniState->firstepLevel, !aniState->mrcCCKOff ? "on" : "off",
        aniState->listenTime, aniState->cycleCount,
        aniState->listenTime, aniState->ofdmPhyErrCount,
        aniState->cckPhyErrCount);
#endif

#ifndef REMOVE_PKT_LOG
    /* do pktlog */
    {
        struct log_ani log_data;

        /* Populate the ani log record */
        log_data.phyStatsDisable = DO_ANI(ah);
        log_data.noiseImmunLvl = aniState->ofdmNoiseImmunityLevel;
        log_data.spurImmunLvl = aniState->spurImmunityLevel;
        log_data.ofdmWeakDet = aniState->ofdmWeakSigDetectOff;
        log_data.cckWeakThr = aniState->cckNoiseImmunityLevel;
        log_data.firLvl = aniState->firstepLevel;
        log_data.listenTime = aniState->listenTime;
        log_data.cycleCount = aniState->cycleCount;
        /* express ofdmPhyErrCount as errors/second */
        log_data.ofdmPhyErrCount = aniState->listenTime ?
            aniState->ofdmPhyErrCount * 1000 / aniState->listenTime : 0;
        /* express cckPhyErrCount as errors/second */
        log_data.cckPhyErrCount =  aniState->listenTime ?
            aniState->cckPhyErrCount * 1000 / aniState->listenTime  : 0;
        log_data.rssi = aniState->rssi;

        /* clear interrupt context flag */
        ath_hal_log_ani(ah->ah_sc, &log_data, 0);
    }
#endif

    return AH_TRUE;
#undef    N
}

static void
ar9300AniRestart(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ar9300AniState *aniState;

    if (!DO_ANI(ah)) {
        return;
    }

    aniState = ahp->ah_curani;

    aniState->listenTime = 0;
    if (ahp->ah_hasHwPhyCounters) {
        aniState->ofdmPhyErrBase = 0;
        aniState->cckPhyErrBase = 0;
#if HAL_ANI_DEBUG
        HDPRINTF(ah, HAL_DBG_ANI,
            "%s: Writing ofdmbase=%08x   cckbase=%08x\n",
            __func__, aniState->ofdmPhyErrBase, aniState->cckPhyErrBase);
#endif
        OS_REG_WRITE(ah, AR_PHY_ERR_1, aniState->ofdmPhyErrBase);
        OS_REG_WRITE(ah, AR_PHY_ERR_2, aniState->cckPhyErrBase);
        OS_REG_WRITE(ah, AR_PHY_ERR_MASK_1, AR_PHY_ERR_OFDM_TIMING);
        OS_REG_WRITE(ah, AR_PHY_ERR_MASK_2, AR_PHY_ERR_CCK_TIMING);

        /* Clear the mib counters and save them in the stats */
        ar9300UpdateMibMacStats(ah);
    }
    aniState->ofdmPhyErrCount = 0;
    aniState->cckPhyErrCount = 0;
}

static void
ar9300AniOfdmErrTrigger(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ar9300AniState *aniState;

    if (!DO_ANI(ah)) {
        return;
    }

    aniState = ahp->ah_curani;

    if (aniState->ofdmNoiseImmunityLevel < HAL_ANI_OFDM_MAX_LEVEL) {
        ar9300AniSetOdfmNoiseImmunityLevel(
            ah, aniState->ofdmNoiseImmunityLevel + 1);
    }
}

static void
ar9300AniCckErrTrigger(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ar9300AniState *aniState;

    if (!DO_ANI(ah)) {
        return;
    }

    aniState = ahp->ah_curani;

    if (aniState->cckNoiseImmunityLevel < HAL_ANI_CCK_MAX_LEVEL) {
        ar9300AniSetCckNoiseImmunityLevel(
            ah, aniState->cckNoiseImmunityLevel + 1);
    }
}

/*
 * Restore the ANI parameters in the HAL and reset the statistics.
 * This routine should be called for every hardware reset and for
 * every channel change.
 */
void
ar9300AniReset(struct ath_hal *ah, HAL_BOOL is_scanning)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ar9300AniState *aniState;
    HAL_CHANNEL_INTERNAL *chan = AH_PRIVATE(ah)->ah_curchan;
    int index;

    HALASSERT(chan != AH_NULL);

    if (!DO_ANI(ah)) {
        return;
    }

    /*
     * we need to re-point to the correct ANI state since the channel
     * may have changed due to a fast channel change
    */
    index = ar9300GetAniChannelIndex(ah, chan);
    aniState = &ahp->ah_ani[index];
    HALASSERT(aniState != AH_NULL);
    ahp->ah_curani = aniState;

    ahp->ah_stats.ast_ani_reset++;

    aniState->phyNoiseSpur = 0;

    // only allow a subset of functions in AP mode
    if (AH_PRIVATE(ah)->ah_opmode == HAL_M_HOSTAP) {
        if (IS_CHAN_2GHZ(chan)) {
            ahp->ah_ani_function = (HAL_ANI_SPUR_IMMUNITY_LEVEL |
                                    HAL_ANI_FIRSTEP_LEVEL |
                                    HAL_ANI_MRC_CCK);
        } else {
            ahp->ah_ani_function = 0;
        }
    }
    // always allow mode (on/off) to be controlled
    ahp->ah_ani_function |= HAL_ANI_MODE;

    if (is_scanning ||
        (AH_PRIVATE(ah)->ah_opmode != HAL_M_STA &&
         AH_PRIVATE(ah)->ah_opmode != HAL_M_IBSS))
    {
        /*
         * If we're scanning or in AP mode, the defaults (ini) should be
         * in place.
         * For an AP we assume the historical levels for this channel are
         * probably outdated so start from defaults instead.
         */
        if (aniState->ofdmNoiseImmunityLevel != HAL_ANI_OFDM_DEF_LEVEL ||
            aniState->cckNoiseImmunityLevel != HAL_ANI_CCK_DEF_LEVEL)
        {
            HDPRINTF(ah, HAL_DBG_ANI,
                "%s: Restore defaults: opmode %u chan %d Mhz/0x%x "
                "is_scanning=%d restore=%d ofdm:%d cck:%d\n",
                __func__, AH_PRIVATE(ah)->ah_opmode, chan->channel,
                chan->channelFlags, is_scanning, aniState->mustRestore,
                aniState->ofdmNoiseImmunityLevel,
                aniState->cckNoiseImmunityLevel);
            /*
             * for STA/IBSS, we want to restore the historical values later
             * (when we're not scanning)
             */
            if (AH_PRIVATE(ah)->ah_opmode == HAL_M_STA ||
                AH_PRIVATE(ah)->ah_opmode == HAL_M_IBSS)
            {
                ar9300AniControl(ah, HAL_ANI_SPUR_IMMUNITY_LEVEL,
                    HAL_ANI_DEF_SPUR_IMMUNE_LVL);
                ar9300AniControl(
                    ah, HAL_ANI_FIRSTEP_LEVEL, HAL_ANI_DEF_FIRSTEP_LVL);
                ar9300AniControl(ah, HAL_ANI_OFDM_WEAK_SIGNAL_DETECTION,
                    HAL_ANI_USE_OFDM_WEAK_SIG);
                ar9300AniControl(ah, HAL_ANI_MRC_CCK, HAL_ANI_ENABLE_MRC_CCK);
                aniState->mustRestore = AH_TRUE;
            } else {
                ar9300AniSetOdfmNoiseImmunityLevel(ah, HAL_ANI_OFDM_DEF_LEVEL);
                ar9300AniSetCckNoiseImmunityLevel(ah, HAL_ANI_CCK_DEF_LEVEL);
            }
        }
    } else {
        /*
         * restore historical levels for this channel
         */
        HDPRINTF(ah, HAL_DBG_ANI,
            "%s: Restore history: opmode %u chan %d Mhz/0x%x is_scanning=%d "
            "restore=%d ofdm:%d cck:%d\n",
            __func__, AH_PRIVATE(ah)->ah_opmode, chan->channel,
            chan->channelFlags, is_scanning, aniState->mustRestore,
            aniState->ofdmNoiseImmunityLevel, aniState->cckNoiseImmunityLevel);
        ar9300AniSetOdfmNoiseImmunityLevel(
            ah, aniState->ofdmNoiseImmunityLevel);
        ar9300AniSetCckNoiseImmunityLevel(ah, aniState->cckNoiseImmunityLevel);
        aniState->mustRestore = AH_FALSE;
    }

    /*
     * enable phy counters if hw supports or if not, enable phy interrupts
     * (so we can count each one)
     */
    if (ahp->ah_hasHwPhyCounters) {
        ar9300AniRestart(ah);
        OS_REG_WRITE(ah, AR_PHY_ERR_MASK_1, AR_PHY_ERR_OFDM_TIMING);
        OS_REG_WRITE(ah, AR_PHY_ERR_MASK_2, AR_PHY_ERR_CCK_TIMING);
    } else {
        ar9300AniRestart(ah);
        /* only turn on phy error interrupts if we don't have hw phy counters */
        ar9300SetRxFilter(ah, ar9300GetRxFilter(ah) | HAL_RX_FILTER_PHYERR);
    }
}

/*
 * Process a MIB interrupt.  We may potentially be invoked because
 * any of the MIB counters overflow/trigger so don't assume we're
 * here because a PHY error counter triggered.
 */
void
ar9300ProcessMibIntr(struct ath_hal *ah, const HAL_NODE_STATS *stats)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    u_int32_t phyCnt1, phyCnt2;

    //HDPRINTF(ah, HAL_DBG_ANI, "%s: Processing Mib Intr\n", __func__);

    /* Reset these counters regardless */
    OS_REG_WRITE(ah, AR_FILT_OFDM, 0);
    OS_REG_WRITE(ah, AR_FILT_CCK, 0);
    if (!(OS_REG_READ(ah, AR_SLP_MIB_CTRL) & AR_SLP_MIB_PENDING)) {
        OS_REG_WRITE(ah, AR_SLP_MIB_CTRL, AR_SLP_MIB_CLEAR);
    }

    /* Clear the mib counters and save them in the stats */
    ar9300UpdateMibMacStats(ah);
    ahp->ah_stats.ast_nodestats = *stats;

    if (!DO_ANI(ah)) {
        /*
         * We must always clear the interrupt cause by resetting
         * the phy error regs.
         */
        OS_REG_WRITE(ah, AR_PHY_ERR_1, 0);
        OS_REG_WRITE(ah, AR_PHY_ERR_2, 0);
        return;
    }

    /* NB: these are not reset-on-read */
    phyCnt1 = OS_REG_READ(ah, AR_PHY_ERR_1);
    phyCnt2 = OS_REG_READ(ah, AR_PHY_ERR_2);
#if HAL_ANI_DEBUG
    HDPRINTF(ah, HAL_DBG_ANI,
        "%s: Errors: OFDM=0x%08x-0x%08x=%d   CCK=0x%08x-0x%08x=%d\n",
        __func__, phyCnt1, ahp->ah_curani->ofdmPhyErrBase,
        phyCnt1-ahp->ah_curani->ofdmPhyErrBase, phyCnt2,
        ahp->ah_curani->cckPhyErrBase,  phyCnt2-ahp->ah_curani->cckPhyErrBase);
#endif
    if (((phyCnt1 & AR_MIBCNT_INTRMASK) == AR_MIBCNT_INTRMASK) ||
        ((phyCnt2 & AR_MIBCNT_INTRMASK) == AR_MIBCNT_INTRMASK)) {
        /* NB: always restart to insure the h/w counters are reset */
        ar9300AniRestart(ah);
    }
}


static void
ar9300AniLowerImmunity(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ar9300AniState *aniState = ahp->ah_curani;

    if (aniState->ofdmNoiseImmunityLevel > 0 &&
        (aniState->ofdmsTurn || aniState->cckNoiseImmunityLevel == 0)) {
        /*
         * lower OFDM noise immunity
         */
        ar9300AniSetOdfmNoiseImmunityLevel(
            ah, aniState->ofdmNoiseImmunityLevel - 1);

        /*
         * only lower either OFDM or CCK errors per turn
         * we lower the other one next time
         */
        return;
    }

    if (aniState->cckNoiseImmunityLevel > 0) {
        /*
         * lower CCK noise immunity
         */
        ar9300AniSetCckNoiseImmunityLevel(
            ah, aniState->cckNoiseImmunityLevel - 1);
    }
}

/* convert HW counter values to ms using mode specifix clock rate */
#define CLOCK_RATE(_ah)  (ath_hal_chan_2_clockrateMHz(_ah) * 1000)

/*
 * Return an approximation of the time spent ``listening'' by
 * deducting the cycles spent tx'ing and rx'ing from the total
 * cycle count since our last call.  A return value <0 indicates
 * an invalid/inconsistent time.
 */
static int32_t
ar9300AniGetListenTime(struct ath_hal *ah, HAL_ANISTATS *ani_stats)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ar9300AniState *aniState;
    u_int32_t txFrameCount, rxFrameCount, cycleCount;
    int32_t listenTime;

    txFrameCount = OS_REG_READ(ah, AR_TFCNT);
    rxFrameCount = OS_REG_READ(ah, AR_RFCNT);
    cycleCount = OS_REG_READ(ah, AR_CCCNT);

    aniState = ahp->ah_curani;
#if ATH_SUPPORT_VOW_DCS 
    if (aniState->cycleCount == 0 || aniState->cycleCount > cycleCount ||
            aniState->txFrameCount > txFrameCount ||
            aniState->rxFrameCount > rxFrameCount) {
#else
    if (aniState->cycleCount == 0 || aniState->cycleCount > cycleCount) {
#endif
        /*
         * Cycle counter wrap (or initial call); it's not possible
         * to accurately calculate a value because the registers
         * right shift rather than wrap--so punt and return 0.
         */
        listenTime = 0;
        ahp->ah_stats.ast_ani_lzero++;
#if HAL_ANI_DEBUG
        HDPRINTF(ah, HAL_DBG_ANI,
            "%s: 1st call: aniState->cycleCount=%d\n",
            __func__, aniState->cycleCount);
#endif
    } else {
        int32_t ccdelta = cycleCount - aniState->cycleCount;
        int32_t rfdelta = rxFrameCount - aniState->rxFrameCount;
        int32_t tfdelta = txFrameCount - aniState->txFrameCount;
        listenTime = (ccdelta - rfdelta - tfdelta) / CLOCK_RATE(ah);
#if ATH_SUPPORT_VOW_DCS 
        ani_stats->cyclecnt_diff = ccdelta;
        ani_stats->txframecnt_diff = tfdelta;
        ani_stats->rxframecnt_diff = rfdelta;    
        ani_stats->valid  = AH_TRUE;
#endif

#if HAL_ANI_DEBUG
        HDPRINTF(ah, HAL_DBG_ANI,
            "%s: cyclecount=%d, rfcount=%d, tfcount=%d, listenTime=%d "
            "CLOCK_RATE=%d\n",
            __func__, ccdelta, rfdelta, tfdelta, listenTime, CLOCK_RATE(ah));
#endif
    }
#if ATH_SUPPORT_VOW_DCS 
    ani_stats->rxclr_cnt =  OS_REG_READ(ah, AR_RCCNT);
#endif
    aniState->cycleCount = cycleCount;
    aniState->txFrameCount = txFrameCount;
    aniState->rxFrameCount = rxFrameCount;
    return listenTime;
}

/*
 * Do periodic processing.  This routine is called from a timer
 */
void
ar9300AniArPoll(struct ath_hal *ah, const HAL_NODE_STATS *stats,
                HAL_CHANNEL *chan, HAL_ANISTATS *ani_stats)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ar9300AniState *aniState;
    int32_t listenTime;
    u_int32_t ofdmPhyErrRate, cckPhyErrRate;
    HAL_BOOL old_phy_noise_spur;

    aniState = ahp->ah_curani;
    ahp->ah_stats.ast_nodestats = *stats;        /* XXX optimize? */

    if (aniState == NULL) {
        // should not happen
        HDPRINTF(ah, HAL_DBG_UNMASKABLE,
            "%s: can't poll - no ANI not initialized for this channel\n",
            __func__);
#if ATH_SUPPORT_VOW_DCS
        ani_stats->valid = AH_FALSE;
#endif
        return;
    }

    /*
     * ar9300AniArPoll is never called while scanning but we may have been
     * scanning and now just restarted polling.  In this case we need to
     * restore historical values.
     */
    if (aniState->mustRestore) {
        HDPRINTF(ah, HAL_DBG_ANI,
            "%s: must restore - calling ar9300AniRestart\n", __func__);
        ar9300AniReset(ah, AH_FALSE);
#if ATH_SUPPORT_VOW_DCS
        ani_stats->valid = AH_FALSE;
#endif
        return;
    }

    listenTime = ar9300AniGetListenTime(ah, ani_stats);
    if (listenTime <= 0) {
        ahp->ah_stats.ast_ani_lneg++;
        /* restart ANI period if listenTime is invalid */
        HDPRINTF(ah, HAL_DBG_ANI,
            "%s: listenTime=%d - calling ar9300AniRestart\n",
            __func__, listenTime);
        ar9300AniRestart(ah);
#if ATH_SUPPORT_VOW_DCS
        ani_stats->valid = AH_FALSE;
#endif
        return;
    }
    /* XXX beware of overflow? */
    aniState->listenTime += listenTime;

    if (ahp->ah_hasHwPhyCounters) {
        u_int32_t phyCnt1, phyCnt2;
        u_int32_t ofdmPhyErrCnt, cckPhyErrCnt;

        /* Clear the mib counters and save them in the stats */
        ar9300UpdateMibMacStats(ah);
        /* NB: these are not reset-on-read */
        phyCnt1 = OS_REG_READ(ah, AR_PHY_ERR_1);
        phyCnt2 = OS_REG_READ(ah, AR_PHY_ERR_2);
        /* XXX sometimes zero, why? */
        if (phyCnt1 < aniState->ofdmPhyErrBase ||
            phyCnt2 < aniState->cckPhyErrBase) {
            if (phyCnt1 < aniState->ofdmPhyErrBase) {
                HDPRINTF(ah, HAL_DBG_ANI, "%s: phyCnt1 0x%x, resetting "
                    "counter value to 0x%x\n", __func__,
                    phyCnt1, aniState->ofdmPhyErrBase);
                OS_REG_WRITE(ah, AR_PHY_ERR_1, aniState->ofdmPhyErrBase);
                OS_REG_WRITE(ah, AR_PHY_ERR_MASK_1, AR_PHY_ERR_OFDM_TIMING);
            }
            if (phyCnt2 < aniState->cckPhyErrBase) {
                HDPRINTF(ah, HAL_DBG_ANI, "%s: phyCnt2 0x%x, resetting "
                    "counter value to 0x%x\n", __func__,
                    phyCnt2, aniState->cckPhyErrBase);
                OS_REG_WRITE(ah, AR_PHY_ERR_2, aniState->cckPhyErrBase);
                OS_REG_WRITE(ah, AR_PHY_ERR_MASK_2, AR_PHY_ERR_CCK_TIMING);
            }
#if ATH_SUPPORT_VOW_DCS
        ani_stats->valid = AH_FALSE;
#endif
            return;        /* XXX */
        }
        /* NB: only use ast_ani_*errs with AH_PRIVATE_DIAG */
        ofdmPhyErrCnt = phyCnt1 - aniState->ofdmPhyErrBase;
        ahp->ah_stats.ast_ani_ofdmerrs +=
            ofdmPhyErrCnt - aniState->ofdmPhyErrCount;
        aniState->ofdmPhyErrCount = ofdmPhyErrCnt;

        cckPhyErrCnt = phyCnt2 - aniState->cckPhyErrBase;
        ahp->ah_stats.ast_ani_cckerrs +=
            cckPhyErrCnt - aniState->cckPhyErrCount;
        aniState->cckPhyErrCount = cckPhyErrCnt;

#if ATH_SUPPORT_VOW_DCS
        ani_stats->listen_time = aniState->listenTime;
        ani_stats->ofdmphyerr_cnt = ofdmPhyErrCnt;
        ani_stats->cckphyerr_cnt = cckPhyErrCnt;  
#endif


#if HAL_ANI_DEBUG
        HDPRINTF(ah, HAL_DBG_ANI,
            "%s: Errors: OFDM=0x%08x-0x%08x=%d   CCK=0x%08x-0x%08x=%d\n",
            __func__, phyCnt1, aniState->ofdmPhyErrBase, ofdmPhyErrCnt,
            phyCnt2, aniState->cckPhyErrBase, cckPhyErrCnt);
#endif
    }
    /*
     * If ani is not enabled, return after we've collected
     * statistics
     */
    if (!DO_ANI(ah)) {
        return;
    }

    ofdmPhyErrRate = aniState->ofdmPhyErrCount * 1000 / aniState->listenTime;
    cckPhyErrRate =  aniState->cckPhyErrCount * 1000 / aniState->listenTime;

    HDPRINTF(ah, HAL_DBG_ANI,
        "%s: listenTime=%d OFDM:%d errs=%d/s CCK:%d errs=%d/s ofdm_turn=%d\n",
        __func__, listenTime, aniState->ofdmNoiseImmunityLevel, ofdmPhyErrRate,
        aniState->cckNoiseImmunityLevel, cckPhyErrRate, aniState->ofdmsTurn);

    if(aniState->listenTime >= HAL_NOISE_DETECT_PERIOD) {
        old_phy_noise_spur = aniState->phyNoiseSpur;
        if (ofdmPhyErrRate <= aniState->ofdmTrigLow &&
            cckPhyErrRate <= aniState->cckTrigLow) {
            if (aniState->listenTime >= HAL_NOISE_RECOVER_PERIOD) {
                aniState->phyNoiseSpur = 0;
            }
        } else {
            aniState->phyNoiseSpur = 1;
        }
        if(old_phy_noise_spur != aniState->phyNoiseSpur) {
            HDPRINTF(ah, HAL_DBG_ANI,
                     "%s: enviroment change from %d to %d\n",
                     __func__, old_phy_noise_spur, aniState->phyNoiseSpur);
        }
    }

    if (aniState->listenTime > 5 * ahp->ah_aniPeriod) {
        /*
         * Check to see if need to lower immunity if
         * 5 aniPeriods have passed
         */
        if (ofdmPhyErrRate <= aniState->ofdmTrigLow &&
            cckPhyErrRate <= aniState->cckTrigLow)
        {
            HDPRINTF(ah, HAL_DBG_ANI,
                "%s: 1. listenTime=%d OFDM:%d errs=%d/s(<%d)  "
                "CCK:%d errs=%d/s(<%d) -> ar9300AniLowerImmunity\n",
                __func__, aniState->listenTime,
                aniState->ofdmNoiseImmunityLevel, ofdmPhyErrRate,
                aniState->ofdmTrigLow, aniState->cckNoiseImmunityLevel,
                cckPhyErrRate, aniState->cckTrigLow);
            ar9300AniLowerImmunity(ah);
            aniState->ofdmsTurn = !aniState->ofdmsTurn;
        }
        HDPRINTF(ah, HAL_DBG_ANI,
            "%s: 1 listenTime=%d ofdm=%d/s cck=%d/s - "
            "calling ar9300AniRestart\n",
            __func__, aniState->listenTime, ofdmPhyErrRate, cckPhyErrRate);
        ar9300AniRestart(ah);
     } else if (aniState->listenTime > ahp->ah_aniPeriod) {
        /* check to see if need to raise immunity */
        if (ofdmPhyErrRate > aniState->ofdmTrigHigh &&
            (cckPhyErrRate <= aniState->cckTrigHigh || aniState->ofdmsTurn)) {
            HDPRINTF(ah, HAL_DBG_ANI,
                "%s: 2 listenTime=%d OFDM:%d errs=%d/s(>%d) -> "
                "ar9300AniOfdmErrTrigger\n",
                __func__, aniState->listenTime,
                aniState->ofdmNoiseImmunityLevel, ofdmPhyErrRate,
                aniState->ofdmTrigHigh);
            ar9300AniOfdmErrTrigger(ah);
            ar9300AniRestart(ah);
            aniState->ofdmsTurn = AH_FALSE;
        } else if (cckPhyErrRate > aniState->cckTrigHigh) {
            HDPRINTF(ah, HAL_DBG_ANI,
                "%s: 3 listenTime=%d CCK:%d errs=%d/s(>%d) -> "
                "ar9300AniCckErrTrigger\n",
                __func__, aniState->listenTime,
                aniState->cckNoiseImmunityLevel, cckPhyErrRate,
                aniState->cckTrigHigh);
            ar9300AniCckErrTrigger(ah);
            ar9300AniRestart(ah);
            aniState->ofdmsTurn = AH_TRUE;
        }
    }
}

/*
 * The poll function above calculates short noise spurs, caused by non-80211
 * devices, based on OFDM/CCK Phy errs.
 * If the noise is short enough, we don't want our ratectrl Algo to stop probing
 * higher rates, due to bad PER.
 */
HAL_BOOL
ar9300_is_ani_noise_spur(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ar9300AniState *aniState;
    
    aniState = ahp->ah_curani;

    return aniState->phyNoiseSpur;
}

#endif /* AH_SUPPORT_AR9300 */
