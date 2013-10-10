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
 
#ifdef ART_BUILD
#include <math.h> /* pow() */
#endif /* ART_BUILD */

#include "opt_ah.h"

#ifdef AH_SUPPORT_AR9300

#include "ah.h"
#include "ah_xr.h"
#include "ah_internal.h"
#include "ah_devid.h"

#include "ar9300.h"
#include "ar9300reg.h"
#include "ar9300phy.h"
#include "ar9300desc.h"

#define FIX_NOISE_FLOOR     1

/* Additional Time delay to wait after activiting the Base band */
#define BASE_ACTIVATE_DELAY         100     /* usec */
#define RTC_PLL_SETTLE_DELAY        100     /* usec */
#define COEF_SCALE_S                24
#ifdef AR9300_EMULATION_BB
#define HT40_CHANNEL_CENTER_SHIFT   0      /* MHz      */
#else
#define HT40_CHANNEL_CENTER_SHIFT   10      /* MHz      */
#endif

#define DELPT 32

extern  HAL_BOOL ar9300ResetTxQueue(struct ath_hal *ah, u_int q);
extern  u_int32_t ar9300NumTxPending(struct ath_hal *ah, u_int q);
extern  HAL_BOOL ar9300SetGlobalTxTimeout(struct ath_hal *, u_int);

/* NB: public for RF backend use */
void    ar9300GetLowerUpperValues(u_int16_t value,
                u_int16_t *pList, u_int16_t listSize,
                u_int16_t *pLowerValue, u_int16_t *pUpperValue);
void    ar9300ModifyRfBuffer(u_int32_t *rfBuf, u_int32_t reg32,
                u_int32_t numBits, u_int32_t firstBit, u_int32_t column);

#ifdef ATH_SUPPORT_TxBF
extern void ar9300InitBf(struct ath_hal *ah);			/*for TxBf*/
#endif
#ifndef AR9300_EMULATION_BB /* To avoid compilation warnings. Function not used when EMULATION. */

#define MAX_MEASUREMENT 8
struct coeff_t {
	int32_t mag_coeff[AR9300_MAX_CHAINS][MAX_MEASUREMENT];
	int32_t phs_coeff[AR9300_MAX_CHAINS][MAX_MEASUREMENT];
	int32_t iqc_coeff[2];
};

HAL_BOOL ar9300_tx_iq_cal_hw_run(struct ath_hal *ah);
static void ar9300_tx_iq_cal_post_proc(struct ath_hal *ah);
static void ar9300_tx_iq_cal_outlier_detection(struct ath_hal *ah, u_int32_t num_chains, struct coeff_t *coeff);

#ifdef ART_BUILD
void ar9300_init_otp(struct ath_hal *ah);
#endif
#endif

#ifdef HOST_OFFLOAD
/* 
 * For usb offload solution, some USB registers must be tuned 
 * to gain better stability/performance but these registers
 * might be changed while doing wlan reset so do this here 
  */
#define WAR_USB_DISABLE_PLL_LOCK_DETECT(__ah) \
do { \
    if (AR_SREV_HORNET(__ah) || AR_SREV_WASP(__ah)) { \
        volatile u_int32_t *usb_ctrl_r1 = (u_int32_t *) 0xb8116c84; \
        volatile u_int32_t *usb_ctrl_r2 = (u_int32_t *) 0xb8116c88; \
        *usb_ctrl_r1 = (*usb_ctrl_r1 & 0xffefffff); \
        *usb_ctrl_r2 = (*usb_ctrl_r2 & 0xfc1fffff) | (1 << 21) | (3 << 22); \
    } \
} while(0)
#else
#define WAR_USB_DISABLE_PLL_LOCK_DETECT(__ah)
#endif

static inline void
ar9300AttachHwPlatform(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);

    ahp->ah_hwp = HAL_TRUE_CHIP;
    return;
}

/* Adjust various register settings based on half/quarter rate clock setting.
 * This includes: +USEC, TX/RX latency, 
 *                + IFS params: slot, eifs, misc etc.
 * SIFS stays the same.
 */
static void 
ar9300SetIFSTiming(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    u_int32_t txLat, rxLat, usec, slot, regval, eifs;

    regval = OS_REG_READ(ah, AR_USEC);
    regval &= ~(AR_USEC_RX_LATENCY | AR_USEC_TX_LATENCY | AR_USEC_USEC);
    if (IS_CHAN_HALF_RATE(chan)) { /* half rates */
        slot = ar9300MacToClks(ah, AR_SLOT_HALF);
        eifs = ar9300MacToClks(ah, AR_EIFS_HALF);
        if (IS_5GHZ_FAST_CLOCK_EN(ah, chan)) { /* fast clock */
             rxLat = SM(AR_RX_LATENCY_HALF_FAST_CLOCK, AR_USEC_RX_LATENCY);
             txLat = SM(AR_TX_LATENCY_HALF_FAST_CLOCK, AR_USEC_TX_LATENCY);
             usec = SM(AR_USEC_HALF_FAST_CLOCK, AR_USEC_USEC);
        } else {
             rxLat = SM(AR_RX_LATENCY_HALF, AR_USEC_RX_LATENCY);
             txLat = SM(AR_TX_LATENCY_HALF, AR_USEC_TX_LATENCY);
             usec = SM(AR_USEC_HALF, AR_USEC_USEC);
        }
    } else { /* quarter rate */
        slot = ar9300MacToClks(ah, AR_SLOT_QUARTER);
        eifs = ar9300MacToClks(ah, AR_EIFS_QUARTER);
        if (IS_5GHZ_FAST_CLOCK_EN(ah, chan)) { /* fast clock */
             rxLat = SM(AR_RX_LATENCY_QUARTER_FAST_CLOCK,
                        AR_USEC_RX_LATENCY);
             txLat = SM(AR_TX_LATENCY_QUARTER_FAST_CLOCK,
                        AR_USEC_TX_LATENCY);
             usec = SM(AR_USEC_QUARTER_FAST_CLOCK, AR_USEC_USEC);
        } else {
             rxLat = SM(AR_RX_LATENCY_QUARTER, AR_USEC_RX_LATENCY);
             txLat = SM(AR_TX_LATENCY_QUARTER, AR_USEC_TX_LATENCY);
             usec = SM(AR_USEC_QUARTER, AR_USEC_USEC);
	}
    }

    OS_REG_WRITE(ah, AR_USEC, (usec | regval | txLat | rxLat));
    OS_REG_WRITE(ah, AR_D_GBL_IFS_SLOT, slot);
    OS_REG_WRITE(ah, AR_D_GBL_IFS_EIFS, eifs);

    return;
}

/*
 * This inline function configures the chip either
 * to encrypt/decrypt management frames or pass thru
 */
static inline void
ar9300InitMFP(struct ath_hal * ah)
{
    u_int32_t   mfpcap;

    ath_hal_getcapability(ah, HAL_CAP_MFP, 0, &mfpcap);

    if(mfpcap == HAL_MFP_QOSDATA) {
        /* Treat like legacy hardware. Do not touch the MFP registers. */
        HDPRINTF(ah, HAL_DBG_RESET, "%s forced to use QOSDATA\n", __func__);
        return;
    }

    /* MFP support (Sowl 1.0 or greater) */
    if (mfpcap == HAL_MFP_HW_CRYPTO) {
        /* configure hardware MFP support */
        HDPRINTF(ah, HAL_DBG_RESET, "%s using HW crypto\n", __func__);
        OS_REG_RMW_FIELD(ah, AR_AES_MUTE_MASK1, AR_AES_MUTE_MASK1_FC_MGMT, 0xE7FF);
        OS_REG_RMW(ah, AR_PCU_MISC_MODE2, AR_PCU_MISC_MODE2_MGMT_CRYPTO_ENABLE,
            AR_PCU_MISC_MODE2_NO_CRYPTO_FOR_NON_DATA_PKT);
        OS_REG_RMW_FIELD(ah, AR_PCU_MISC_MODE2, AR_PCU_MISC_MODE2_MGMT_QOS, 0xF);
    } else if (mfpcap == HAL_MFP_PASSTHRU) {
        /* Disable en/decrypt by hardware */
        HDPRINTF(ah, HAL_DBG_RESET, "%s using passthru\n", __func__);
        OS_REG_RMW(ah, AR_PCU_MISC_MODE2, AR_PCU_MISC_MODE2_NO_CRYPTO_FOR_NON_DATA_PKT,
            AR_PCU_MISC_MODE2_MGMT_CRYPTO_ENABLE);
    }
}

void
ar9300GetChannelCenters(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan,
                        CHAN_CENTERS *centers)
{
    int8_t      extoff;
    struct ath_hal_9300 *ahp = AH9300(ah);

    if (!IS_CHAN_HT40(chan)) {
        centers->ctl_center = centers->ext_center =
        centers->synth_center = chan->channel;
        return;
    }

    HALASSERT(IS_CHAN_HT40(chan));

    /*
     * In 20/40 phy mode, the center frequency is
     * "between" the primary and extension channels.
     */
    if (chan->channelFlags & CHANNEL_HT40PLUS) {
        centers->synth_center = chan->channel + HT40_CHANNEL_CENTER_SHIFT;
        extoff = 1;
    }
    else {
        centers->synth_center = chan->channel - HT40_CHANNEL_CENTER_SHIFT;
        extoff = -1;
    }

     centers->ctl_center = centers->synth_center - (extoff *
                            HT40_CHANNEL_CENTER_SHIFT);
     centers->ext_center = centers->synth_center + (extoff *
                ((ahp->ah_extprotspacing == HAL_HT_EXTPROTSPACING_20)? HT40_CHANNEL_CENTER_SHIFT : 15));

}

void
ar9300UploadNoiseFloor(struct ath_hal *ah, int is2G,
                    int16_t nfarray[NUM_NF_READINGS])
{
    int16_t nf;

    /* Chain 0 Ctl */
    nf = MS(OS_REG_READ(ah, AR_PHY_CCA_0), AR_PHY_MINCCA_PWR);
    if (nf & 0x100)
        nf = 0 - ((nf ^ 0x1ff) + 1);
    if(nf > ar9300GetMaxUserNFVal(ah, is2G)) {
        HDPRINTF(ah, HAL_DBG_NF_CAL, "%s[%d] NF 0 out of MAX bound %d correcting \n", __func__, __LINE__, nf);
        nf = ar9300GetMaxUserNFVal(ah, is2G);
    } else if( nf < ar9300GetMinUserNFVal(ah, is2G)) {
        HDPRINTF(ah, HAL_DBG_NF_CAL, "%s[%d] NF 0 out of MIN bound %d correcting \n", __func__, __LINE__, nf);
        nf = ar9300GetMinUserNFVal(ah, is2G);
    }
    nfarray[0] = nf;

    HDPRINTF(ah, HAL_DBG_NF_CAL, "NF calibrated [ctl] [chain 0] is %d\n", nf);

    if (!AR_SREV_HORNET(ah) && !AR_SREV_POSEIDON(ah)) {
        /* Chain 1 Ctl */
        nf = MS(OS_REG_READ(ah, AR_PHY_CCA_1), AR_PHY_CH1_MINCCA_PWR);
        if (nf & 0x100)
            nf = 0 - ((nf ^ 0x1ff) + 1);
        if(nf > ar9300GetMaxUserNFVal(ah, is2G)) {
            HDPRINTF(ah, HAL_DBG_NF_CAL, "%s[%d] NF 1 out of MAX bound %d correcting \n", __func__, __LINE__, nf);
            nf = ar9300GetMaxUserNFVal(ah, is2G);
        } else if( nf < ar9300GetMinUserNFVal(ah, is2G)) {
            HDPRINTF(ah, HAL_DBG_NF_CAL, "%s[%d] NF 1 out of MIN bound %d correcting \n", __func__, __LINE__, nf);
            nf = ar9300GetMinUserNFVal(ah, is2G);
        }
        nfarray[1] = nf;

        HDPRINTF(ah, HAL_DBG_NF_CAL, "NF calibrated [ctl] [chain 1] is %d\n", nf);

        if (!AR_SREV_WASP(ah)) {
            /* Chain 2 Ctl */
            nf = MS(OS_REG_READ(ah, AR_PHY_CCA_2), AR_PHY_CH2_MINCCA_PWR);
            if (nf & 0x100)
                nf = 0 - ((nf ^ 0x1ff) + 1);
            if(nf > ar9300GetMaxUserNFVal(ah, is2G)) {
                HDPRINTF(ah, HAL_DBG_NF_CAL, "%s[%d] NF 2 out of MAX bound %d correcting \n", __func__, __LINE__, nf);
                nf = ar9300GetMaxUserNFVal(ah, is2G);
            } else if( nf < ar9300GetMinUserNFVal(ah, is2G)) {
                HDPRINTF(ah, HAL_DBG_NF_CAL, "%s[%d] NF 2 out of MIN bound %d correcting \n", __func__, __LINE__, nf);
                nf = ar9300GetMinUserNFVal(ah, is2G);
            }
            nfarray[2] = nf;

            HDPRINTF(ah, HAL_DBG_NF_CAL, "NF calibrated [ctl] [chain 2] is %d\n", nf);
        }
    }

    /* Chain 0 Ext */
    nf = MS(OS_REG_READ(ah, AR_PHY_EXT_CCA), AR_PHY_EXT_MINCCA_PWR);
    if (nf & 0x100)
        nf = 0 - ((nf ^ 0x1ff) + 1);
    if(nf > ar9300GetMaxUserNFVal(ah, is2G)) {
        HDPRINTF(ah, HAL_DBG_NF_CAL, "%s[%d] NF ext 0 out of MAX bound %d correcting \n", __func__, __LINE__, nf);
        nf = ar9300GetMaxUserNFVal(ah, is2G);
    } else if( nf < ar9300GetMinUserNFVal(ah, is2G)) {
        HDPRINTF(ah, HAL_DBG_NF_CAL, "%s[%d] NF ext 0 out of MIN bound %d correcting \n", __func__, __LINE__, nf);
        nf = ar9300GetMinUserNFVal(ah, is2G);
    }
    nfarray[3] = nf;
    HDPRINTF(ah, HAL_DBG_NF_CAL, "NF calibrated [ext] [chain 0] is %d\n", nf);

    if (!AR_SREV_HORNET(ah) && !AR_SREV_POSEIDON(ah)) {
        /* Chain 1 Ext */
        nf = MS(OS_REG_READ(ah, AR_PHY_EXT_CCA_1), AR_PHY_CH1_EXT_MINCCA_PWR);
        if (nf & 0x100)
            nf = 0 - ((nf ^ 0x1ff) + 1);
        if(nf > ar9300GetMaxUserNFVal(ah, is2G)) {
            HDPRINTF(ah, HAL_DBG_NF_CAL, "%s[%d] NF ext 1 out of MAX bound %d correcting \n", __func__, __LINE__, nf);
            nf = ar9300GetMaxUserNFVal(ah, is2G);
        } else if( nf < ar9300GetMinUserNFVal(ah, is2G)) {
            HDPRINTF(ah, HAL_DBG_NF_CAL, "%s[%d] NF ext 1 out of MIN bound %d correcting \n", __func__, __LINE__, nf);
            nf = ar9300GetMinUserNFVal(ah, is2G);
        }
        nfarray[4] = nf;
        HDPRINTF(ah, HAL_DBG_NF_CAL, "NF calibrated [ext] [chain 1] is %d\n", nf);

        if (!AR_SREV_WASP(ah)) {
            /* Chain 2 Ext */
            nf = MS(OS_REG_READ(ah, AR_PHY_EXT_CCA_2), AR_PHY_CH2_EXT_MINCCA_PWR);
            if (nf & 0x100)
                nf = 0 - ((nf ^ 0x1ff) + 1);
            if(nf > ar9300GetMaxUserNFVal(ah, is2G)) {
                HDPRINTF(ah, HAL_DBG_NF_CAL, "%s[%d] NF ext 2 out of MAX bound %d correcting \n", __func__, __LINE__, nf);
                nf = ar9300GetMaxUserNFVal(ah, is2G);
            } else if( nf < ar9300GetMinUserNFVal(ah, is2G)) {
                HDPRINTF(ah, HAL_DBG_NF_CAL, "%s[%d] NF ext 2 out of MIN bound %d correcting \n", __func__, __LINE__, nf);
                nf = ar9300GetMinUserNFVal(ah, is2G);
            }
            nfarray[5] = nf;
            HDPRINTF(ah, HAL_DBG_NF_CAL, "NF calibrated [ext] [chain 2] is %d\n", nf);
        }
    }
}

/* Used by the scan function for a quick read of the noise floor. This is used to detect
 * presence of CW interference such as video bridge. The noise floor is assumed to have been
 * already started during reset called during channel change. The function checks if the noise 
 * floor reading is done. In case it has been done, it reads the noise floor value. If the noise floor
 * calibration has not been finished, it assumes this is due to presence of CW interference an returns a high 
 * value for noise floor. derived from the CW interference threshold + margin fudge factor. 
 */
#define BAD_SCAN_NF_MARGIN (30)
int16_t ar9300GetMinCCAPwr(struct ath_hal *ah)
{
    int16_t nf;
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);

    if ((OS_REG_READ(ah, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) == 0) {
        nf = MS(OS_REG_READ(ah, AR_PHY_CCA_0), AR9280_PHY_MINCCA_PWR);

        if (nf & 0x100) nf = 0 - ((nf ^ 0x1ff) + 1);
    } else {
        /* NF calibration is not done, assume CW interference */
        nf = ahpriv->nfp->nominal + ahpriv->nf_cw_int_delta + BAD_SCAN_NF_MARGIN;    
    }

    return nf;
}


/*
 * The following functions get and set the noise floor filtering parameters.
 * This allows customers to easily test different NF filtering values, if
 * they choose to fine-tune NF parameters for their products, rather than
 * using the defaults.
 */
void
ar9300SetNominalUserNFVal(struct ath_hal *ah, int16_t val, HAL_BOOL is2GHz)
{
    if (is2GHz) {
        AH_PRIVATE(ah)->nf_2GHz.nominal = val;
    } else {
        AH_PRIVATE(ah)->nf_5GHz.nominal = val;
    }
}

int16_t
ar9300GetNominalUserNFVal(struct ath_hal *ah, HAL_BOOL is2GHz)
{
    return (is2GHz) ?
        AH_PRIVATE(ah)->nf_2GHz.nominal : AH_PRIVATE(ah)->nf_5GHz.nominal;
}

void
ar9300SetMinUserNFVal(struct ath_hal *ah, int16_t val, HAL_BOOL is2GHz)
{
    if (is2GHz) {
        AH_PRIVATE(ah)->nf_2GHz.min = val;
    } else {
        AH_PRIVATE(ah)->nf_5GHz.min = val;
    }
}

int16_t
ar9300GetMinUserNFVal(struct ath_hal *ah, HAL_BOOL is2GHz)
{
    return (is2GHz) ?
        AH_PRIVATE(ah)->nf_2GHz.min : AH_PRIVATE(ah)->nf_5GHz.min;
}

void
ar9300SetMaxUserNFVal(struct ath_hal *ah, int16_t val, HAL_BOOL is2GHz)
{
    if (is2GHz) {
        AH_PRIVATE(ah)->nf_2GHz.max = val;
    } else {
        AH_PRIVATE(ah)->nf_5GHz.max = val;
    }
}

int16_t
ar9300GetMaxUserNFVal(struct ath_hal *ah, HAL_BOOL is2GHz)
{
    return (is2GHz) ?
        AH_PRIVATE(ah)->nf_2GHz.max : AH_PRIVATE(ah)->nf_5GHz.max;
}

void
ar9300SetNfDeltaVal(struct ath_hal *ah, int16_t val)
{
    AH_PRIVATE(ah)->nf_cw_int_delta = val;
}

int16_t
ar9300GetNfDeltaVal(struct ath_hal *ah)
{
    return AH_PRIVATE(ah)->nf_cw_int_delta;
}

/* 
 * Noise Floor values for all chains. 
 * Most recently updated values from the NF history buffer are used.
 */
void ar9300ChainNoiseFloor(struct ath_hal *ah, int16_t *nfBuf,
                           HAL_CHANNEL *chan, int is_scan)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    int i, nf_hist_len, recentNFIndex = 0;
    HAL_NFCAL_HIST_FULL *h;
    u_int8_t rx_chainmask = ahp->ah_rxchainmask | (ahp->ah_rxchainmask << 3);
    HAL_CHANNEL_INTERNAL *ichan = ath_hal_checkchannel(ah, chan); 
    HALASSERT(ichan);

#ifdef ATH_NF_PER_CHAN
    /* Fill 0 if valid internal channel is not found */
    if (ichan == AH_NULL) {
        OS_MEMZERO(nfBuf, sizeof(nfBuf[0])*NUM_NF_READINGS);
        return;
    }
    h = &ichan->nfCalHist;
    nf_hist_len = HAL_NF_CAL_HIST_LEN_FULL;
#else
    /*
     * If a scan is not in progress, then the most recent value goes
     * into ahpriv->nfCalHist.  If a scan is in progress, then
     * the most recent value goes into ichan->nfCalHist.
     * Thus, return the value from ahpriv->nfCalHist if there's
     * no scan, and if the specified channel is the current channel.
     * Otherwise, return the noise floor from ichan->nfCalHist.
     */
    if ((!is_scan) && chan->channel == AH_PRIVATE(ah)->ah_curchan->channel){
        h = &AH_PRIVATE(ah)->nfCalHist;
        nf_hist_len = HAL_NF_CAL_HIST_LEN_FULL;
    } else {
        /* Fill 0 if valid internal channel is not found */
        if (ichan == AH_NULL) {
            OS_MEMZERO(nfBuf, sizeof(nfBuf[0])*NUM_NF_READINGS);
            return;
        }
       /*
        * It is okay to treat a HAL_NFCAL_HIST_SMALL struct as if it were a
        * HAL_NFCAL_HIST_FULL struct, as long as only the index 0 of the
        * nfCalBuffer is used (nfCalBuffer[0][0:NUM_NF_READINGS-1])
        */
        h = (HAL_NFCAL_HIST_FULL *) &ichan->nfCalHist;
        nf_hist_len = HAL_NF_CAL_HIST_LEN_SMALL;
    }
#endif
    /* Get most recently updated values from nf cal history buffer */
    recentNFIndex = (h->base.currIndex) ? h->base.currIndex - 1 : nf_hist_len - 1;

    for (i = 0; i < NUM_NF_READINGS; i ++) {
        /* Fill 0 for unsupported chains */
        if (!(rx_chainmask & (1 << i))) {
            nfBuf[i] = 0;
            continue;
        }
        nfBuf[i] = h->nfCalBuffer[recentNFIndex][i];
    }
}


/*
 *  Pick up the medium one in the noise floor buffer and update the corresponding range
 *  for valid noise floor values
 */
static int16_t
ar9300GetNfHistMid(struct ath_hal *ah, HAL_NFCAL_HIST_FULL *h, int reading, int hist_len)

{
    int16_t nfval;
    int16_t sort[HAL_NF_CAL_HIST_LEN_FULL]; /* upper bound for hist_len */
    int i,j;

    for (i = 0; i < hist_len; i ++) {
        sort[i] = h->nfCalBuffer[i][reading];
	HDPRINTF(ah, HAL_DBG_NF_CAL, "nfCalBuffer[%d][%d] = %d\n", i, reading, (int)sort[i]);
    }
    for (i = 0; i < hist_len-1; i ++) {
        for (j = 1; j < hist_len-i; j ++) {
            if (sort[j] > sort[j-1]) {
                nfval = sort[j];
                sort[j] = sort[j-1];
                sort[j-1] = nfval;
            }
        }
    }
    nfval = sort[(hist_len-1)>>1];

    return nfval;
}

static int16_t ar9300LimitNfRange(struct ath_hal *ah, int16_t nf)
{
    if (nf < AH_PRIVATE(ah)->nfp->min) {
        return AH_PRIVATE(ah)->nfp->nominal;
    } else if (nf > AH_PRIVATE(ah)->nfp->max) {
        return AH_PRIVATE(ah)->nfp->max;
    }
    return nf;
}

#ifndef ATH_NF_PER_CHAN
inline static void
ar9300ResetNfHistBuff(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *ichan)
{
    HAL_CHAN_NFCAL_HIST *h = &ichan->nfCalHist;
    HAL_NFCAL_HIST_FULL *home = &AH_PRIVATE(ah)->nfCalHist;
    int i;

    /* 
     * Copy the value for the channel in question into the home-channel
     * NF history buffer.  The channel NF is probably a value filled in by
     * a prior background channel scan, but if no scan has been done then
     * it is the nominal noise floor filled in by ath_hal_init_NF_buffer
     * for this chip and the channel's band.
     * Replicate this channel NF into all entries of the home-channel NF
     * history buffer.
     * If the channel NF was filled in by a channel scan, it has not had
     * bounds limits applied to it yet - do so now.  It is important to
     * apply bounds limits to the privNF value that gets loaded into the
     * WLAN chip's minCCApwr register field.  It is also necessary to
     * apply bounds limits to the nfCalBuffer[] elements.  Since we are
     * replicating a single NF reading into all nfCalBuffer elements,
     * if the single reading were above the CW_INT threshold, the CW_INT
     * check in ar9300GetNf would immediately conclude that CW interference
     * is present, even though we're not supposed to set CW_INT unless
     * NF values are _consistently_ above the CW_INT threshold.
     * Applying the bounds limits to the nfCalBuffer contents fixes this
     * problem.
     */
    for (i = 0; i < NUM_NF_READINGS; i ++) {
        int j;
        int16_t nf;
        /*
         * No need to set currIndex, since it already has a value in
         * the range [0..HAL_NF_CAL_HIST_LEN_FULL), and all nfCalBuffer
         * values will be the same.
         */
        nf = ar9300LimitNfRange(ah, h->nfCalBuffer[0][i]);
        for (j = 0; j < HAL_NF_CAL_HIST_LEN_FULL; j++) {
            home->nfCalBuffer[j][i] = nf;
        }
        AH_PRIVATE(ah)->nfCalHist.base.privNF[i] = nf;
    }
}
#endif

/*
 *  Update the noise floor buffer as a ring buffer
 */
static int16_t
ar9300UpdateNFHistBuff(struct ath_hal *ah, HAL_NFCAL_HIST_FULL *h, int16_t *nfarray,
                        int hist_len)
{
    int i, nr;
    int16_t nf_no_lim_chain0;

    nf_no_lim_chain0 = ar9300GetNfHistMid(ah, h, 0, hist_len);

    HDPRINTF(ah, HAL_DBG_NF_CAL, "%s[%d] BEFORE\n", __func__, __LINE__);
     for(nr = 0; nr < HAL_NF_CAL_HIST_LEN_FULL; nr++) {
         for(i = 0; i < NUM_NF_READINGS; i++) {
              HDPRINTF(ah, HAL_DBG_NF_CAL, "nfCalBuffer[%d][%d] = %d\n", nr, i, (int)h->nfCalBuffer[nr][i]);
         }
    }
    for (i = 0; i < NUM_NF_READINGS; i ++) {
        h->nfCalBuffer[h->base.currIndex][i] = nfarray[i];
        h->base.privNF[i] = ar9300LimitNfRange(ah, ar9300GetNfHistMid(ah, h, i, hist_len));
    }
    HDPRINTF(ah, HAL_DBG_NF_CAL, "%s[%d] AFTER\n", __func__, __LINE__);
     for(nr = 0; nr < HAL_NF_CAL_HIST_LEN_FULL; nr++) {
         for(i = 0; i < NUM_NF_READINGS; i++) {
              HDPRINTF(ah, HAL_DBG_NF_CAL, "nfCalBuffer[%d][%d] = %d\n", nr, i, (int)h->nfCalBuffer[nr][i]);
         }
    }

    if (++h->base.currIndex >= hist_len)
        h->base.currIndex = 0;

    return nf_no_lim_chain0;
}

#if UNUSED
static HAL_BOOL
getNoiseFloorThresh(struct ath_hal *ah, const HAL_CHANNEL_INTERNAL *chan,
        int16_t *nft)
{
    struct ath_hal_9300 *ahp = AH9300(ah);

    switch (chan->channelFlags & CHANNEL_ALL_NOTURBO) {
        case CHANNEL_A:
        case CHANNEL_A_HT20:
        case CHANNEL_A_HT40PLUS:
        case CHANNEL_A_HT40MINUS:
            *nft = (int8_t)ar9300EepromGet(ahp, EEP_NFTHRESH_5);
            break;
        case CHANNEL_B:
        case CHANNEL_G:
        case CHANNEL_G_HT20:
        case CHANNEL_G_HT40PLUS:
        case CHANNEL_G_HT40MINUS:
            *nft = (int8_t)ar9300EepromGet(ahp, EEP_NFTHRESH_2);
            break;
        default:
            HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: invalid channel flags 0x%x\n",
                    __func__, chan->channelFlags);
            return AH_FALSE;
    }
    return AH_TRUE;
}
#endif

/*
 * Read the NF and check it against the noise floor threshhold
 */
#define IS(_c,_f)       (((_c)->channelFlags & _f) || 0)
static int
ar9300StoreNewNf(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan, int is_scan)
{
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    int nf_hist_len;
    int16_t nf_no_lim;
    int16_t nfarray[NUM_NF_READINGS]= {0};
    HAL_NFCAL_HIST_FULL *h;
    int is2G = 0;

    if (OS_REG_READ(ah, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) {
        u_int32_t tsf32, nf_cal_dur_tsf;
        /*
         * The reason the NF calibration did not complete may just be that
         * not enough time has passed since the NF calibration was started,
         * because under certain conditions (when first moving to a new
         * channel) the NF calibration may be checked very repeatedly.
         * Or, there may be CW interference keeping the NF calibration
         * from completing.  Check the delta time between when the NF
         * calibration was started and now to see whether the NF calibration
         * should have already completed (but hasn't, probably due to CW
         * interference), or hasn't had enough time to finish yet.
         */
        /*
         * AH_NF_CAL_DUR_MAX_TSF - A conservative maximum time that the
         *     HW should need to finish a NF calibration.  If the HW
         *     does not complete a NF calibration within this time period,
         *     there must be a problem - probably CW interference.
         * AH_NF_CAL_PERIOD_MAX_TSF - A conservative maximum time between
         *     check of the HW's NF calibration being finished.
         *     If the difference between the current TSF and the TSF
         *     recorded when the NF calibration started is larger than this
         *     value, the TSF must have been reset.
         *     In general, we expect the TSF to only be reset during
         *     regular operation for STAs, not for APs.  However, an
         *     AP's TSF could be reset when joining an IBSS.
         *     There's an outside chance that this could result in the
         *     CW_INT flag being erroneously set, if the TSF adjustment
         *     is smaller than AH_NF_CAL_PERIOD_MAX_TSF but larger than
         *     AH_NF_CAL_DUR_TSF.  However, even if this does happen,
         *     it shouldn't matter, as the IBSS case shouldn't be
         *     concerned about CW_INT.
         */
        #define AH_NF_CAL_DUR_TSF (90*1000*1000) /* 90 sec in usec units */
        #define AH_NF_CAL_PERIOD_MAX_TSF (180*1000*1000) /* 180 sec in usec */
        /* wraparound handled by using unsigned values */
        tsf32 = ar9300GetTsf32(ah);
        nf_cal_dur_tsf = tsf32 - AH9300(ah)->nf_tsf32;
        if (nf_cal_dur_tsf > AH_NF_CAL_PERIOD_MAX_TSF) {
            /*
             * The TSF must have gotten reset during the NF cal -
             * just reset the NF TSF timestamp, so the next time
             * this function is called, the timestamp comparison
             * will be valid.
             */
            AH9300(ah)->nf_tsf32 = tsf32;
        } else if (nf_cal_dur_tsf > AH_NF_CAL_DUR_TSF) {
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "%s: NF did not complete in calibration window\n", __func__);
            /* the NF incompletion is probably due to CW interference */
            chan->channelFlags |= CHANNEL_CW_INT;
        }
        return 0; /* HW's NF measurement not finished */
    }
    HDPRINTF(ah, HAL_DBG_NF_CAL, "%s[%d] chan %d\n", __func__, __LINE__, chan->channel);
    is2G = IS(chan, CHANNEL_2GHZ);
    ar9300UploadNoiseFloor(ah, is2G, nfarray);

    /* Update the NF buffer for each chain masked by chainmask */
#ifdef ATH_NF_PER_CHAN
    h = &chan->nfCalHist;
    nf_hist_len = HAL_NF_CAL_HIST_LEN_FULL;
#else
    if (is_scan) {
        /*
         * This channel's NF cal info is just a HAL_NFCAL_HIST_SMALL struct
         * rather than a HAL_NFCAL_HIST_FULL struct.
         * As long as we only use the first history element of nfCalBuffer
         * (nfCalBuffer[0][0:NUM_NF_READINGS-1]), we can use
         * HAL_NFCAL_HIST_SMALL and HAL_NFCAL_HIST_FULL interchangeably.
         */
        h = (HAL_NFCAL_HIST_FULL *) &chan->nfCalHist;
        nf_hist_len = HAL_NF_CAL_HIST_LEN_SMALL;
    } else {
        h = &AH_PRIVATE(ah)->nfCalHist;
        nf_hist_len = HAL_NF_CAL_HIST_LEN_FULL;
    }
#endif


    /*
     * nf_no_lim = median value from NF history buffer without bounds limits,
     * privNF = median value from NF history buffer with bounds limits.
     */
    nf_no_lim = ar9300UpdateNFHistBuff(ah, h, nfarray, nf_hist_len);
    chan->rawNoiseFloor = h->base.privNF[0];

    /* check if there is interference */
    chan->channelFlags &= (~CHANNEL_CW_INT);
    if (!IS_9300_EMU(ah) &&
        nf_no_lim > ahpriv->nfp->nominal + ahpriv->nf_cw_int_delta) {
        /*
         * Since this CW interference check is being applied to the
         * median element of the NF history buffer, this indicates that
         * the CW interference is persistent.  A single high NF reading
         * will not show up in the median, and thus will not cause the
         * CW_INT flag to be set.
         */
         HDPRINTF(ah, HAL_DBG_NF_CAL,
                "%s: NF Cal: CW interferer detected through NF: %d \n", __func__, nf_no_lim); 
         chan->channelFlags |= CHANNEL_CW_INT;
    }
    return 1; /* HW's NF measurement finished */
}
#undef IS

static inline void
ar9300GetDeltaSlopeValues(struct ath_hal *ah, u_int32_t coef_scaled,
                          u_int32_t *coef_mantissa, u_int32_t *coef_exponent)
{
    u_int32_t coef_exp, coef_man;

    /*
     * ALGO -> coef_exp = 14-floor(log2(coef));
     * floor(log2(x)) is the highest set bit position
     */
    for (coef_exp = 31; coef_exp > 0; coef_exp--)
            if ((coef_scaled >> coef_exp) & 0x1)
                    break;
    /* A coef_exp of 0 is a legal bit position but an unexpected coef_exp */
    HALASSERT(coef_exp);
    coef_exp = 14 - (coef_exp - COEF_SCALE_S);

    /*
     * ALGO -> coef_man = floor(coef* 2^coef_exp+0.5);
     * The coefficient is already shifted up for scaling
     */
    coef_man = coef_scaled + (1 << (COEF_SCALE_S - coef_exp - 1));

    *coef_mantissa = coef_man >> (COEF_SCALE_S - coef_exp);
    *coef_exponent = coef_exp - 16;
}

#define MAX_ANALOG_START        319             /* XXX */

/*
 * Delta slope coefficient computation.
 * Required for OFDM operation.
 */
static void
ar9300SetDeltaSlope(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{
    u_int32_t coef_scaled, ds_coef_exp, ds_coef_man;
    u_int32_t fclk = COEFF; /* clock * 2.5 */

    u_int32_t clockMhzScaled = 0x1000000 * fclk;
    CHAN_CENTERS centers;


    /*
     * half and quarter rate can divide the scaled clock by 2 or 4
     * scale for selected channel bandwidth
     */
    if (IS_CHAN_HALF_RATE(chan)) {
        clockMhzScaled = clockMhzScaled >> 1;
    } else if (IS_CHAN_QUARTER_RATE(chan)) {
        clockMhzScaled = clockMhzScaled >> 2;
    }

    /*
     * ALGO -> coef = 1e8/fcarrier*fclock/40;
     * scaled coef to provide precision for this floating calculation
     */
    ar9300GetChannelCenters(ah, chan, &centers);
    coef_scaled = clockMhzScaled / centers.synth_center;

    ar9300GetDeltaSlopeValues(ah, coef_scaled, &ds_coef_man, &ds_coef_exp);

    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING3,
                AR_PHY_TIMING3_DSC_MAN, ds_coef_man);
    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING3,
                AR_PHY_TIMING3_DSC_EXP, ds_coef_exp);

    /*
     * For Short GI,
     * scaled coeff is 9/10 that of normal coeff
     */
    coef_scaled = (9 * coef_scaled)/10;

    ar9300GetDeltaSlopeValues(ah, coef_scaled, &ds_coef_man, &ds_coef_exp);

    /* for short gi */
    OS_REG_RMW_FIELD(ah, AR_PHY_SGI_DELTA, AR_PHY_SGI_DSC_MAN, ds_coef_man);
    OS_REG_RMW_FIELD(ah, AR_PHY_SGI_DELTA, AR_PHY_SGI_DSC_EXP, ds_coef_exp);
}

#define IS(_c,_f)       (((_c)->channelFlags & _f) || 0)

static inline HAL_CHANNEL_INTERNAL*
ar9300CheckChan(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    if ((IS(chan, CHANNEL_2GHZ) ^ IS(chan, CHANNEL_5GHZ)) == 0) {
        HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: invalid channel %u/0x%x; not marked as "
                 "2GHz or 5GHz\n", __func__,
                chan->channel, chan->channelFlags);
        return AH_NULL;
    }

    if ((IS(chan, CHANNEL_OFDM) ^ IS(chan, CHANNEL_CCK) ^
         IS(chan, CHANNEL_HT20) ^ IS(chan, CHANNEL_HT40PLUS) ^
         IS(chan, CHANNEL_HT40MINUS)) == 0)
    {
        HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: invalid channel %u/0x%x; not marked as "
                "OFDM or CCK or HT20 or HT40PLUS or HT40MINUS\n", __func__,
                chan->channel, chan->channelFlags);
        return AH_NULL;
    }

    return (ath_hal_checkchannel(ah, chan));
}
#undef IS




static void
ar9300Set11nRegs(struct ath_hal *ah, HAL_CHANNEL *chan, HAL_HT_MACMODE macmode)
{
    u_int32_t phymode;
    struct ath_hal_9300 *ahp = AH9300(ah);
#ifndef AR9300_EMULATION_BB
    u_int32_t enableDacFifo;
#endif

#ifndef AR9300_EMULATION_BB
        /* XXX */
    enableDacFifo = (OS_REG_READ(ah, AR_PHY_GEN_CTRL) & AR_PHY_GC_ENABLE_DAC_FIFO);
#endif

    /* Enable 11n HT, 20 MHz */
    phymode = AR_PHY_GC_HT_EN | AR_PHY_GC_SINGLE_HT_LTF1 | AR_PHY_GC_SHORT_GI_40
#ifndef AR9300_EMULATION_BB
            | enableDacFifo;
#else
		;
#endif
    /* Configure baseband for dynamic 20/40 operation */
    if (IS_CHAN_HT40(chan)) {
        phymode |= AR_PHY_GC_DYN2040_EN;
        /* Configure control (primary) channel at +-10MHz */
        if (chan->channelFlags & CHANNEL_HT40PLUS) {
            phymode |= AR_PHY_GC_DYN2040_PRI_CH;
        }

        /* Configure 20/25 spacing */
        if (ahp->ah_extprotspacing == HAL_HT_EXTPROTSPACING_25) {
            phymode |= AR_PHY_GC_DYN2040_EXT_CH;
        }
    }

    /* make sure we preserve INI settings */
    phymode |= OS_REG_READ(ah, AR_PHY_GEN_CTRL);

	/* EV 62881/64991 - turn off Green Field detection for Maverick STA beta */
    phymode &= ~AR_PHY_GC_GF_DETECT_EN;

    OS_REG_WRITE(ah, AR_PHY_GEN_CTRL, phymode);

    /* Set IFS timing for half/quarter rates */
    if (IS_CHAN_HALF_RATE(chan) || IS_CHAN_QUARTER_RATE(chan)) {
        u_int32_t modeselect = OS_REG_READ(ah, AR_PHY_MODE);

        if (IS_CHAN_HALF_RATE(chan)) {
            modeselect |= AR_PHY_MS_HALF_RATE;
        } else if (IS_CHAN_QUARTER_RATE(chan)) {
            modeselect |= AR_PHY_MS_QUARTER_RATE;
        }
        OS_REG_WRITE(ah, AR_PHY_MODE, modeselect);

        ar9300SetIFSTiming(ah, chan);
        OS_REG_RMW_FIELD(ah, AR_PHY_FRAME_CTL, AR_PHY_FRAME_CTL_CF_OVERLAP_WINDOW, 0x3);
    }

    /* Configure MAC for 20/40 operation */
    ar9300Set11nMac2040(ah, macmode);

    /* global transmit timeout (25 TUs default)*/
    /* XXX - put this elsewhere??? */
    OS_REG_WRITE(ah, AR_GTXTO, 25 << AR_GTXTO_TIMEOUT_LIMIT_S) ;

    /* carrier sense timeout */
    OS_REG_WRITE(ah, AR_CST, 0xF << AR_CST_TIMEOUT_LIMIT_S);
}

/*
 * Spur mitigation for MRC CCK
 */
static void
ar9300SpurMitigateMrcCck(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    int i;
    u_int32_t spur_freq_for_Osprey[4] = { 2420, 2440, 2464, 2480 }; /* Hardcoded by Systems team for now. */
    int cur_bb_spur, negative=0, cck_spur_freq;
    u_int8_t* spurFbinsPtr = NULL;
    int synth_freq;
    int range = 10;
    int max_spurcounts = OSPREY_EEPROM_MODAL_SPURS; 

    /*
     * Need to verify range +/- 10 MHz in control channel, otherwise spur
     * is out-of-band and can be ignored.
     */

    if(AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah) || AR_SREV_WASP(ah)) {
        
        spurFbinsPtr = ar9300EepromGetSpurChansPtr(ah, 1);

        if (spurFbinsPtr[0] == 0) {
            return;      /* No spur in the mode */
        }  

        if (IS_CHAN_HT40(chan)) {
            range = 19;
            if (OS_REG_READ_FIELD(ah, AR_PHY_GEN_CTRL, AR_PHY_GC_DYN2040_PRI_CH) == 0x0) {
                synth_freq = chan->channel + 10;
            } 
            else {
                synth_freq = chan->channel - 10;
            }
        } 
        else {
            range = 10;
            synth_freq = chan->channel;
        }

    } else {
        range = 10;
        max_spurcounts = 4;/* Hardcoded by Osprey Systems team for now. */
        synth_freq = chan->channel;
    }

    for (i = 0; i < max_spurcounts; i++) {
        negative = 0;

        if(AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah) || AR_SREV_WASP(ah)) {
            cur_bb_spur = FBIN2FREQ(spurFbinsPtr[i], HAL_FREQ_BAND_2GHZ) - synth_freq;
        } else {
            cur_bb_spur = spur_freq_for_Osprey[i] - synth_freq;
        }
        
        if(cur_bb_spur < 0) {
           negative = 1;
           cur_bb_spur = -cur_bb_spur;
        }
        if (cur_bb_spur < range) {
            cck_spur_freq = (int)((cur_bb_spur << 19) / 11);
            if(negative == 1) {
                cck_spur_freq = -cck_spur_freq;
            }
            cck_spur_freq = cck_spur_freq & 0xfffff;
            //OS_REG_WRITE_field(ah, BB_agc_control.ycok_max, 0x7);
            OS_REG_RMW_FIELD(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_YCOK_MAX, 0x7);
    
            //OS_REG_WRITE_field(ah, BB_cck_spur_mit.spur_rssi_thr, 0x7f);
            OS_REG_RMW_FIELD(ah, AR_PHY_CCK_SPUR_MIT, AR_PHY_CCK_SPUR_MIT_SPUR_RSSI_THR, 0x7f);
            
            //OS_REG_WRITE(ah, BB_cck_spur_mit.spur_filter_type, 0x2);
            OS_REG_RMW_FIELD(ah, AR_PHY_CCK_SPUR_MIT, AR_PHY_CCK_SPUR_MIT_SPUR_FILTER_TYPE, 0x2);
    
            //OS_REG_WRITE(ah, BB_cck_spur_mit.use_cck_spur_mit, 0x1);
            OS_REG_RMW_FIELD(ah, AR_PHY_CCK_SPUR_MIT, AR_PHY_CCK_SPUR_MIT_USE_CCK_SPUR_MIT, 0x1);
    
            //OS_REG_WRITE(ah, BB_cck_spur_mit.cck_spur_freq, cck_spur_freq);
            OS_REG_RMW_FIELD(ah, AR_PHY_CCK_SPUR_MIT, AR_PHY_CCK_SPUR_MIT_CCK_SPUR_FREQ, cck_spur_freq);
            return; 
        }
    }

    //OS_REG_WRITE(ah, BB_agc_control.ycok_max, 0x5);
	OS_REG_RMW_FIELD(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_YCOK_MAX, 0x5);

    //OS_REG_WRITE(ah, BB_cck_spur_mit.use_cck_spur_mit, 0x0);
	OS_REG_RMW_FIELD(ah, AR_PHY_CCK_SPUR_MIT, AR_PHY_CCK_SPUR_MIT_USE_CCK_SPUR_MIT, 0x0);

    //OS_REG_WRITE(ah, BB_cck_spur_mit.cck_spur_freq, 0x0);
	OS_REG_RMW_FIELD(ah, AR_PHY_CCK_SPUR_MIT, AR_PHY_CCK_SPUR_MIT_CCK_SPUR_FREQ, 0x0);
}

// Spur mitigation for OFDM
static void ar9300SpurMitigateOfdm(struct ath_hal *ah, HAL_CHANNEL *chan)
{ 
    int synth_freq;
    int range = 10;
    int freq_offset = 0;
    int spur_freq_sd = 0;
    int spur_subchannel_sd = 0;
    int spur_delta_phase = 0;
    int mask_index = 0;
    int i;
    int mode;
    u_int8_t* spurChansPtr;
    struct ath_hal_9300 *ahp;
    ahp = AH9300(ah);

    if (IS_CHAN_5GHZ(chan)) {
        spurChansPtr = ar9300EepromGetSpurChansPtr(ah, 0);
        mode = 0;
    } 
    else {
        spurChansPtr = ar9300EepromGetSpurChansPtr(ah, 1);
        mode = 1;
    }

    if (spurChansPtr[0] == 0) {
        return;      // No spur in the mode
    }

    if (IS_CHAN_HT40(chan)) {
        range = 19;
        if (OS_REG_READ_FIELD(ah, AR_PHY_GEN_CTRL, AR_PHY_GC_DYN2040_PRI_CH) == 0x0) {
            synth_freq = chan->channel - 10;
        } 
        else {
            synth_freq = chan->channel + 10;
        }
    } 
    else {
        range = 10;
        synth_freq = chan->channel;
    }

    // Clean all spur register fields
    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_SPUR_FILTER, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING11, AR_PHY_TIMING11_SPUR_FREQ_SD, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING11, AR_PHY_TIMING11_SPUR_DELTA_PHASE, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_EXT, AR_PHY_SFCORR_EXT_SPUR_SUBCHANNEL_SD, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING11, AR_PHY_TIMING11_USE_SPUR_FILTER_IN_AGC, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING11, AR_PHY_TIMING11_USE_SPUR_FILTER_IN_SELFCOR, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_SPUR_RSSI, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_SPUR_REG, AR_PHY_SPUR_REG_EN_VIT_SPUR_RSSI, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_SPUR_REG, AR_PHY_SPUR_REG_ENABLE_NF_RSSI_SPUR_MIT, 0);

    OS_REG_RMW_FIELD(ah, AR_PHY_SPUR_REG, AR_PHY_SPUR_REG_ENABLE_MASK_PPM, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_PILOT_MASK, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_CHAN_MASK, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_PILOT_SPUR_MASK, AR_PHY_PILOT_SPUR_MASK_CF_PILOT_MASK_IDX_A, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_SPUR_MASK_A, AR_PHY_SPUR_MASK_A_CF_PUNC_MASK_IDX_A, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_CHAN_SPUR_MASK, AR_PHY_CHAN_SPUR_MASK_CF_CHAN_MASK_IDX_A, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_PILOT_SPUR_MASK, AR_PHY_PILOT_SPUR_MASK_CF_PILOT_MASK_A, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_CHAN_SPUR_MASK, AR_PHY_CHAN_SPUR_MASK_CF_CHAN_MASK_A, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_SPUR_MASK_A, AR_PHY_SPUR_MASK_A_CF_PUNC_MASK_A, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_SPUR_REG, AR_PHY_SPUR_REG_MASK_RATE_CNTL, 0);


    //  spurChansPtr[0] = 164; // channel 2464
    i = 0;
    while (spurChansPtr[i] && i < 5)
    {
        freq_offset = FBIN2FREQ(spurChansPtr[i], mode) - synth_freq;
        if (abs(freq_offset) < range) {
            // printf("Spur Mitigation for OFDM: Synth Frequency = %d, Spur Frequency = %d\n", synth_freq, FBIN2FREQ(spurChansPtr[i], mode));
            if (IS_CHAN_HT40(chan)) {
                if (freq_offset < 0) {
                    if (OS_REG_READ_FIELD(ah, AR_PHY_GEN_CTRL, AR_PHY_GC_DYN2040_PRI_CH) == 0x0){
                        spur_subchannel_sd = 1;
                    } 
                    else {
                        spur_subchannel_sd = 0;
                    }
                    spur_freq_sd = ((freq_offset+10)<<9)/11;
                } 
                else {
                    if (OS_REG_READ_FIELD(ah, AR_PHY_GEN_CTRL, AR_PHY_GC_DYN2040_PRI_CH) == 0x0){
                        spur_subchannel_sd = 0;
                    } 
                    else {
                        spur_subchannel_sd = 1;
                    }
                    spur_freq_sd = ((freq_offset-10)<<9)/11;
                }
                spur_delta_phase = (freq_offset<<17)/5;
            } 
            else {
                //} elsif (($::MODE == MODE_2G_HT20) || ($::MODE == MODE_2G_LEGACY) || ($::MODE == MODE_5G_HT20) || ($::MODE == MODE_5G_LEGACY)) {
                spur_subchannel_sd = 0;
                spur_freq_sd = (freq_offset<<9)/11;
                spur_delta_phase = (freq_offset<<18)/5;
            }
            spur_freq_sd = spur_freq_sd & 0x3ff;
            spur_delta_phase = spur_delta_phase & 0xfffff;
            //printf("spur_subchannel_sd = %d, spur_freq_sd = 0x%x, spur_delta_phase = 0x%x\n", spur_subchannel_sd, spur_freq_sd, spur_delta_phase);

            // OFDM Spur mitigation
            OS_REG_RMW_FIELD(ah, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_SPUR_FILTER, 0x1);
            OS_REG_RMW_FIELD(ah, AR_PHY_TIMING11, AR_PHY_TIMING11_SPUR_FREQ_SD, spur_freq_sd);
            OS_REG_RMW_FIELD(ah, AR_PHY_TIMING11, AR_PHY_TIMING11_SPUR_DELTA_PHASE, spur_delta_phase);
            OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_EXT, AR_PHY_SFCORR_EXT_SPUR_SUBCHANNEL_SD, spur_subchannel_sd);
            OS_REG_RMW_FIELD(ah, AR_PHY_TIMING11, AR_PHY_TIMING11_USE_SPUR_FILTER_IN_AGC, 0x1);
            OS_REG_RMW_FIELD(ah, AR_PHY_TIMING11, AR_PHY_TIMING11_USE_SPUR_FILTER_IN_SELFCOR, 0x1);
            OS_REG_RMW_FIELD(ah, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_SPUR_RSSI, 0x1);
            OS_REG_RMW_FIELD(ah, AR_PHY_SPUR_REG, AR_PHY_SPUR_REG_SPUR_RSSI_THRESH, 34);
            OS_REG_RMW_FIELD(ah, AR_PHY_SPUR_REG, AR_PHY_SPUR_REG_EN_VIT_SPUR_RSSI, 1);

            /*
             * Do not subtract spur power from noise floor for wasp.
             * This causes the maximum client test (on Veriwave) to fail
             * when run on spur channel (2464 MHz).
             * Refer to ev#82746 and ev#82744.
             */
            if (!AR_SREV_WASP(ah) &&
                (OS_REG_READ_FIELD(ah, AR_PHY_MODE, AR_PHY_MODE_DYNAMIC) == 0x1)) {
                OS_REG_RMW_FIELD(ah, AR_PHY_SPUR_REG, AR_PHY_SPUR_REG_ENABLE_NF_RSSI_SPUR_MIT, 1);
            }

            mask_index = (freq_offset<<4)/5;
            if (mask_index < 0) {
                mask_index = mask_index - 1;
            }
            mask_index = mask_index & 0x7f;
            //printf("Bin 0x%x\n", mask_index);

            OS_REG_RMW_FIELD(ah, AR_PHY_SPUR_REG, AR_PHY_SPUR_REG_ENABLE_MASK_PPM, 0x1);
            OS_REG_RMW_FIELD(ah, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_PILOT_MASK, 0x1);
            OS_REG_RMW_FIELD(ah, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_CHAN_MASK, 0x1);
            OS_REG_RMW_FIELD(ah, AR_PHY_PILOT_SPUR_MASK, AR_PHY_PILOT_SPUR_MASK_CF_PILOT_MASK_IDX_A, mask_index);
            OS_REG_RMW_FIELD(ah, AR_PHY_SPUR_MASK_A, AR_PHY_SPUR_MASK_A_CF_PUNC_MASK_IDX_A, mask_index);
            OS_REG_RMW_FIELD(ah, AR_PHY_CHAN_SPUR_MASK, AR_PHY_CHAN_SPUR_MASK_CF_CHAN_MASK_IDX_A, mask_index);
            OS_REG_RMW_FIELD(ah, AR_PHY_PILOT_SPUR_MASK, AR_PHY_PILOT_SPUR_MASK_CF_PILOT_MASK_A, 0xc);
            OS_REG_RMW_FIELD(ah, AR_PHY_CHAN_SPUR_MASK, AR_PHY_CHAN_SPUR_MASK_CF_CHAN_MASK_A, 0xc);
            OS_REG_RMW_FIELD(ah, AR_PHY_SPUR_MASK_A, AR_PHY_SPUR_MASK_A_CF_PUNC_MASK_A, 0xa0);
            OS_REG_RMW_FIELD(ah, AR_PHY_SPUR_REG, AR_PHY_SPUR_REG_MASK_RATE_CNTL, 0xff);

            //printf("BB_timing_control_4 = 0x%x\n", OS_REG_READ(ah, AR_PHY_TIMING4));
            //printf("BB_timing_control_11 = 0x%x\n", OS_REG_READ(ah, AR_PHY_TIMING11));
            //printf("BB_ext_chan_scorr_thr = 0x%x\n", OS_REG_READ(ah, AR_PHY_SFCORR_EXT));
            //printf("BB_spur_mask_controls = 0x%x\n", OS_REG_READ(ah, AR_PHY_SPUR_REG));
            //printf("BB_pilot_spur_mask = 0x%x\n", OS_REG_READ(ah, AR_PHY_PILOT_SPUR_MASK));
            //printf("BB_chan_spur_mask = 0x%x\n", OS_REG_READ(ah, AR_PHY_CHAN_SPUR_MASK));
            //printf("BB_vit_spur_mask_A = 0x%x\n", OS_REG_READ(ah, AR_PHY_SPUR_MASK_A));

            break;
        }
        i++;
    }
}


/*
 * Convert to baseband spur frequency given input channel frequency 
 * and compute register settings below.
 */
static void
ar9300SpurMitigate(struct ath_hal *ah, HAL_CHANNEL *chan)
{
	ar9300SpurMitigateOfdm(ah, chan);
    ar9300SpurMitigateMrcCck(ah, chan);
}

/**************************************************************
 * ar9300ChannelChange
 * Assumes caller wants to change channel, and not reset.
 */
static inline HAL_BOOL
ar9300ChannelChange(struct ath_hal *ah, HAL_CHANNEL *chan,
                    HAL_CHANNEL_INTERNAL *ichan, HAL_HT_MACMODE macmode)
{
    u_int32_t synthDelay, qnum;
    struct ath_hal_9300 *ahp = AH9300(ah);

    /* TX must be stopped by now */
    for (qnum = 0; qnum < AR_NUM_QCU; qnum++) {
        if (ar9300NumTxPending(ah, qnum)) {
            HDPRINTF(ah, HAL_DBG_QUEUE, "%s: Transmit frames pending on queue %d\n", __func__, qnum);
            HALASSERT(0);
            return AH_FALSE;
        }
    }

    /*
     * Kill last Baseband Rx Frame - Request analog bus grant
     */
    OS_REG_WRITE(ah, AR_PHY_RFBUS_REQ, AR_PHY_RFBUS_REQ_EN);
    if (!ath_hal_wait(ah, AR_PHY_RFBUS_GRANT, AR_PHY_RFBUS_GRANT_EN,
                          AR_PHY_RFBUS_GRANT_EN, AH_WAIT_TIMEOUT)) {
        HDPRINTF(ah, HAL_DBG_PHY_IO, "%s: Could not kill baseband RX\n", __func__);
        return AH_FALSE;
    }

    /* Setup 11n MAC/Phy mode registers */
    ar9300Set11nRegs(ah, chan, macmode);

    /*
     * Change the synth
     */
    if (!ahp->ah_rfHal.setChannel(ah, ichan)) {
        HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: failed to set channel\n", __func__);
        return AH_FALSE;
    }

    /*
     * Setup the transmit power values.
     *
     * After the public to private hal channel mapping, ichan contains the
     * valid regulatory power value.
     * ath_hal_getctl and ath_hal_getantennaallowed look up ichan from chan.
     */
#ifndef AR9300_EMULATION_BB
    if (ar9300EepromSetTransmitPower(ah, &ahp->ah_eeprom, ichan,
         ath_hal_getctl(ah,chan), ath_hal_getantennaallowed(ah, chan),
         ath_hal_getTwiceMaxRegPower(AH_PRIVATE(ah), ichan, chan),
         AH_MIN(MAX_RATE_POWER, AH_PRIVATE(ah)->ah_powerLimit)) != HAL_OK)
    {
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: error init'ing transmit power\n", __func__);
        return AH_FALSE;
    }
#endif

    /*
     * Wait for the frequency synth to settle (synth goes on via PHY_ACTIVE_EN).
     * Read the phy active delay register. Value is in 100ns increments.
     */
    synthDelay = OS_REG_READ(ah, AR_PHY_RX_DELAY) & AR_PHY_RX_DELAY_DELAY;
    if (IS_CHAN_CCK(chan)) {
        synthDelay = (4 * synthDelay) / 22;
    } else {
        synthDelay /= 10;
    }

    OS_DELAY(synthDelay + BASE_ACTIVATE_DELAY);

    /*
     * Release the RFBus Grant.
     */
     OS_REG_WRITE(ah, AR_PHY_RFBUS_REQ, 0);

    /*
     * Calibrations need to be triggered after RFBus Grant is released.
     * Otherwise, cals may not be able to complete.
     */
    if (!ichan->oneTimeCalsDone) {
        /*
         * Start offset and carrier leak cals
         */
    }

    /*
     * Write spur immunity and delta slope for OFDM enabled modes (A, G, Turbo)
     */
    if (IS_CHAN_OFDM(chan)|| IS_CHAN_HT(chan)) {
        ar9300SetDeltaSlope(ah, ichan);
    }

    ar9300SpurMitigate(ah, chan);
    
    if (!ichan->oneTimeCalsDone) {
        /*
         * wait for end of offset and carrier leak cals
         */
        ichan->oneTimeCalsDone = AH_TRUE;
    }


    return AH_TRUE;
}

void
ar9300SetOperatingMode(struct ath_hal *ah, int opmode)
{
    u_int32_t val;

    val = OS_REG_READ(ah, AR_STA_ID1);
    val &= ~(AR_STA_ID1_STA_AP | AR_STA_ID1_ADHOC);
    switch (opmode) {
    case HAL_M_HOSTAP:
        OS_REG_WRITE(ah, AR_STA_ID1, val | AR_STA_ID1_STA_AP
                                | AR_STA_ID1_KSRCH_MODE);
        OS_REG_CLR_BIT(ah, AR_CFG, AR_CFG_AP_ADHOC_INDICATION);
        break;
    case HAL_M_IBSS:
        OS_REG_WRITE(ah, AR_STA_ID1, val | AR_STA_ID1_ADHOC
                                | AR_STA_ID1_KSRCH_MODE);
        OS_REG_SET_BIT(ah, AR_CFG, AR_CFG_AP_ADHOC_INDICATION);
        break;
    case HAL_M_STA:
    case HAL_M_MONITOR:
        OS_REG_WRITE(ah, AR_STA_ID1, val | AR_STA_ID1_KSRCH_MODE);
        break;
    }
}

/* XXX need the logic for Osprey */
inline void
ar9300InitPLL(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    u_int32_t pll;
    u_int8_t clk_25mhz = AH9300(ah)->clk_25mhz;

#ifdef AR9300_EMULATION
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                 "FIXME: ar9300InitPLL\n");
#ifdef AR9300_EMULATION_BB
    // Set bypass bit for BB emulation
    pll = 0x19428;
#else
    pll = 0x9428;
#endif

    OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL, pll);

#else
    if (AR_SREV_HORNET(ah)) {
        if (clk_25mhz) {
            /* Hornet uses PLL_CONTROL_2. Xtal is 25MHz for Hornet. REFDIV set to 0x1.
             * $xtal_freq = 25;
             * $PLL2_div = (704/$xtal_freq); # 176 * 4 = 704. MAC and BB run at 176 MHz.
             * $PLL2_divint = int($PLL2_div);
             * $PLL2_divfrac = $PLL2_div - $PLL2_divint;
             * $PLL2_divfrac = int($PLL2_divfrac * 0x4000); # 2^14
             * $PLL2_Val = ($PLL2_divint & 0x3f) << 19 | (0x1) << 14 | $PLL2_divfrac & 0x3fff;
             * Therefore, $PLL2_Val = 0xe04a3d
             */
#define DPLL2_KD_VAL            0x1D
#define DPLL2_KI_VAL            0x06
#define DPLL3_PHASE_SHIFT_VAL   0x1

            /* Rewrite DDR PLL2 and PLL3 */
            /* program DDR PLL ki and kd value, ki=0x6, kd=0x1d */
            OS_REG_WRITE(ah, AR_HORNET_CH0_DDR_DPLL2, 0x18e82f01);

            /* program DDR PLL phase_shift to 0x1 */
            OS_REG_RMW_FIELD(ah, AR_HORNET_CH0_DDR_DPLL3,
                AR_PHY_BB_DPLL3_PHASE_SHIFT, DPLL3_PHASE_SHIFT_VAL);

            OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL, 0x1142c);
            OS_DELAY(1000);

            /* program refdiv, nint, frac to RTC register */
            OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL2, 0xe04a3d);
        
            /* program BB PLL ki and kd value, ki=0x6, kd=0x1d */
        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2,
                AR_PHY_BB_DPLL2_KD, DPLL2_KD_VAL);
        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2,
                AR_PHY_BB_DPLL2_KI, DPLL2_KI_VAL);
        
            /* program BB PLL phase_shift to 0x1 */
        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL3,
                AR_PHY_BB_DPLL3_PHASE_SHIFT, DPLL3_PHASE_SHIFT_VAL);
        } else { /* 40MHz */
#undef  DPLL2_KD_VAL
#undef  DPLL2_KI_VAL
#define DPLL2_KD_VAL            0x3D
#define DPLL2_KI_VAL            0x06
            /* Rewrite DDR PLL2 and PLL3 */
            /* program DDR PLL ki and kd value, ki=0x6, kd=0x3d */
            OS_REG_WRITE(ah, AR_HORNET_CH0_DDR_DPLL2, 0x19e82f01);
        
            /* program DDR PLL phase_shift to 0x1 */
            OS_REG_RMW_FIELD(ah, AR_HORNET_CH0_DDR_DPLL3,
                AR_PHY_BB_DPLL3_PHASE_SHIFT, DPLL3_PHASE_SHIFT_VAL);
        
            OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL, 0x1142c);
        OS_DELAY(1000);
        
            /* program refdiv, nint, frac to RTC register */
            OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL2, 0x886666);
			
            /* program BB PLL ki and kd value, ki=0x6, kd=0x3d */
			OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2,
                AR_PHY_BB_DPLL2_KD, DPLL2_KD_VAL);
            OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2,
                AR_PHY_BB_DPLL2_KI, DPLL2_KI_VAL);
			
            /* program BB PLL phase_shift to 0x1 */
            OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL3,
                AR_PHY_BB_DPLL3_PHASE_SHIFT, DPLL3_PHASE_SHIFT_VAL);
        }
        OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL, 0x142c);
        OS_DELAY(1000);
    } else if (AR_SREV_POSEIDON(ah)) {
        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2, 
            AR_PHY_BB_DPLL2_PLL_PWD, 0x1);

        /* program BB PLL ki and kd value, ki=0x4, kd=0x40 */
        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2, 
            AR_PHY_BB_DPLL2_KD, 0x40);
        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2, 
            AR_PHY_BB_DPLL2_KI, 0x4);

        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL1, 
            AR_PHY_BB_DPLL1_REFDIV, 0x5);
        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL1, 
            AR_PHY_BB_DPLL1_NINI, 0x58);
        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL1, 
            AR_PHY_BB_DPLL1_NFRAC, 0x0);

        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2, 
            AR_PHY_BB_DPLL2_OUTDIV, 0x1);
        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2, 
            AR_PHY_BB_DPLL2_LOCAL_PLL, 0x1);
        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2, 
            AR_PHY_BB_DPLL2_EN_NEGTRIG, 0x1); 

        /* program BB PLL phase_shift to 0x6 */
        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL3, 
            AR_PHY_BB_DPLL3_PHASE_SHIFT, 0x6); 

        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2, 
            AR_PHY_BB_DPLL2_PLL_PWD, 0x0); 
        OS_DELAY(1000);

        OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL, 0x142c);
        OS_DELAY(1000);
    } else if (AR_SREV_WASP(ah)) {
#define SRIF_PLL 1
        u_int32_t regdata, pll2_divint, pll2_divfrac, pll2_clkmode;

#ifdef SRIF_PLL
        u_int32_t refdiv;
#endif
        if (clk_25mhz) {
#ifndef SRIF_PLL
            pll2_divint = 0x1c;
            pll2_divfrac = 0xa3d7;
#else
            pll2_divint = 0x54;
            pll2_divfrac = 0x1eb85;
            refdiv=3;
#endif
        } else {
#ifndef SRIF_PLL
            pll2_divint = 0x11;
            pll2_divfrac = 0x26666;
#else
            pll2_divint = 88;
            pll2_divfrac = 0;
            refdiv=5;
#endif
        }
        pll2_clkmode = 0x3d;
        /* PLL programming through SRIF Local Mode */
        OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL, 0x1142c);      // Bypass mode
        OS_DELAY(1000);
        do {
            regdata = OS_REG_READ(ah, AR_PHY_PLL_MODE);
            regdata = regdata | (0x1 << 16);
            OS_REG_WRITE(ah, AR_PHY_PLL_MODE, regdata);       // PWD_PLL set to 1
            OS_DELAY(100);
#ifndef SRIF_PLL
            OS_REG_WRITE(ah, AR_PHY_PLL_CONTROL, ((1 << 27) | (pll2_divint << 18) |
                         pll2_divfrac));  // override int, frac, refdiv
#else
            OS_REG_WRITE(ah, AR_PHY_PLL_CONTROL, ((refdiv << 27) | (pll2_divint << 18) |
                         pll2_divfrac));  // override int, frac, refdiv
#endif
            OS_DELAY(100);
            regdata = OS_REG_READ(ah, AR_PHY_PLL_MODE);
#ifndef SRIF_PLL
            regdata = (regdata & 0x80071fff) | (0x1<<30) | (0x1<<13) | (0x6<<26) | (pll2_clkmode << 19);
#else
            regdata = (regdata & 0x80071fff) | (0x1<<30) | (0x1<<13) | (0x4<<26) | (0x18 << 19);
#endif
            OS_REG_WRITE(ah, AR_PHY_PLL_MODE, regdata);       // Ki, Kd, Local PLL, Outdiv
            regdata = OS_REG_READ(ah, AR_PHY_PLL_MODE);
            regdata = (regdata & 0xfffeffff);
            OS_REG_WRITE(ah, AR_PHY_PLL_MODE, regdata);       // PWD_LL set to 0
            OS_DELAY(1000);
            /* clear do measure */
            regdata = OS_REG_READ(ah, AR_PHY_PLL_BB_DPLL3);
            regdata &= ~(1 << 30);
            OS_REG_WRITE(ah, AR_PHY_PLL_BB_DPLL3, regdata);
            OS_DELAY(100);

            /* set do measure */
            regdata = OS_REG_READ(ah, AR_PHY_PLL_BB_DPLL3);
            regdata |= (1 << 30);
            OS_REG_WRITE(ah, AR_PHY_PLL_BB_DPLL3, regdata);

            /* wait for measure done */
            do {
                regdata = OS_REG_READ(ah, AR_PHY_PLL_BB_DPLL4);
            } while ((regdata & (1 << 3)) == 0);

            /* clear do measure */
            regdata = OS_REG_READ(ah, AR_PHY_PLL_BB_DPLL3);
            regdata &= ~(1 << 30);
            OS_REG_WRITE(ah, AR_PHY_PLL_BB_DPLL3, regdata);

            /* get measure sqsum dvc */
            regdata = (OS_REG_READ(ah, AR_PHY_PLL_BB_DPLL3) & 0x007FFFF8) >> 3;
        } while (regdata >= 0x40000);

        OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL, 0x142c);       // Remove from Bypass mode
        OS_DELAY(1000);
    } else {
        pll = SM(0x5, AR_RTC_PLL_REFDIV);
  
        /* Supposedly not needed on Osprey */
#if 0
        if (chan && IS_CHAN_HALF_RATE(chan)) {
            pll |= SM(0x1, AR_RTC_PLL_CLKSEL);
        } else if (chan && IS_CHAN_QUARTER_RATE(chan)) {
            pll |= SM(0x2, AR_RTC_PLL_CLKSEL);
        }
#endif
        if (chan && IS_CHAN_5GHZ(chan)) {
            pll |= SM(0x28, AR_RTC_PLL_DIV);
            /* 
             * When doing fast clock, set PLL to 0x142c
             */
            if (IS_5GHZ_FAST_CLOCK_EN(ah, chan)) {
                pll = 0x142c;
            }
        } 
        else {
            pll |= SM(0x2c, AR_RTC_PLL_DIV);
        }

        OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL, pll);

    }
#endif

    /* TODO:
    * For multi-band owl, switch between bands by reiniting the PLL.
    */

#ifdef AR9300_EMULATION
    OS_DELAY(1000);
#else
    OS_DELAY(RTC_PLL_SETTLE_DELAY);
#endif

    OS_REG_WRITE(ah, AR_RTC_SLEEP_CLK, AR_RTC_FORCE_DERIVED_CLK | AR_RTC_PCIE_RST_PWDN_EN);

}

static inline HAL_BOOL
ar9300SetReset(struct ath_hal *ah, int type)
{
    u_int32_t rst_flags;
    u_int32_t tmpReg;

#ifdef AH_ASSERT
    if (type != HAL_RESET_WARM && type != HAL_RESET_COLD) {
        HALASSERT(0);
    }
#endif

    /*
     * RTC Force wake should be done before resetting the MAC.
     * MDK/ART does it that way.
     */
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_WA), AH9300(ah)->ah_WARegVal);
    OS_DELAY(10); // delay to allow AR_WA reg write to kick in.
    OS_REG_WRITE(ah, AR_RTC_FORCE_WAKE, AR_RTC_FORCE_WAKE_EN |
                                     AR_RTC_FORCE_WAKE_ON_INT);

    /* Reset AHB */
    /* Bug26871 */
    tmpReg = OS_REG_READ(ah, AR_HOSTIF_REG(ah, AR_INTR_SYNC_CAUSE));
    if (tmpReg & (AR_INTR_SYNC_LOCAL_TIMEOUT|AR_INTR_SYNC_RADM_CPL_TIMEOUT)) {
        OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_INTR_SYNC_ENABLE), 0);
        OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_RC), AR_RC_HOSTIF);
    }
    else {
        //NO AR_RC_AHB in Osprey
        //OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_RC), AR_RC_AHB);
    }

    rst_flags = AR_RTC_RC_MAC_WARM;
    if (type == HAL_RESET_COLD) {
        rst_flags |= AR_RTC_RC_MAC_COLD;
    }

#ifdef AH_SUPPORT_HORNET
    /* Hornet WAR: trigger SoC to reset WMAC if ...
     * (1) doing cold reset. Ref: EV 69254 
     * (2) beacon pending. Ref: EV 70983
     */
    if (AR_SREV_HORNET(ah) && (ar9300NumTxPending(ah, AH_PRIVATE(ah)->ah_caps.halTotalQueues - 1) != 0 || type == HAL_RESET_COLD)) {
        u_int32_t time_out;
#ifdef _WIN64
#define AR_SOC_RST_RESET         0xB806001CULL
#else
#define AR_SOC_RST_RESET         0xB806001C
#define AR_SOC_BOOT_STRAP        0xB80600AC
#endif
#define AR_SOC_WLAN_RST          0x00000800 // WLAN reset
#define REG_WRITE(_reg,_val)    *((volatile u_int32_t *)(_reg)) = (_val);
#define REG_READ(_reg)          *((volatile u_int32_t *)(_reg))
        HDPRINTF(ah, HAL_DBG_RESET, "%s: Hornet SoC reset WMAC.\n", __func__);

#ifdef ART_BUILD
				MyRegisterRead(AR_SOC_RST_RESET, &tmpReg);
				MyRegisterWrite(AR_SOC_RST_RESET, tmpReg | AR_SOC_WLAN_RST);
				MyRegisterRead(AR_SOC_RST_RESET, &tmpReg);
				MyRegisterWrite(AR_SOC_RST_RESET, tmpReg & (~AR_SOC_WLAN_RST));
				time_out = 0;
				while(1)
				{
					MyRegisterRead(AR_SOC_BOOT_STRAP, &tmpReg);
					if(tmpReg & 0x10 != 0)
					{
						break;
					}
					if(time_out > 0x100)
					{
						break;
					}
					time_out++;	
				}
#else        
        REG_WRITE(AR_SOC_RST_RESET, REG_READ(AR_SOC_RST_RESET) | AR_SOC_WLAN_RST);
        REG_WRITE(AR_SOC_RST_RESET, REG_READ(AR_SOC_RST_RESET) & (~AR_SOC_WLAN_RST));

        time_out = 0;

        while (1) {
            tmpReg = REG_READ(AR_SOC_BOOT_STRAP);
            if ((tmpReg & 0x10) == 0) {
                break;
            }
            if (time_out > 20) {
                break;
            }
            OS_DELAY(10000);
            time_out++;
        }

#endif
        OS_REG_WRITE(ah, AR_RTC_RESET, 1);
#undef REG_READ
#undef REG_WRITE
#undef AR_SOC_WLAN_RST
#undef AR_SOC_RST_RESET
#undef AR_SOC_BOOT_STRAP
    }
#endif

    /*
     * Set Mac(BB,Phy) Warm Reset
     */
    OS_REG_WRITE(ah, AR_RTC_RC, rst_flags);

    OS_DELAY(50); /* XXX 50 usec */
#ifdef AR9300_EMULATION
    OS_DELAY(10000);
#endif

    /*
     * Clear resets and force wakeup
     */
    OS_REG_WRITE(ah, AR_RTC_RC, 0);
#ifdef AR9300_EMULATION
        OS_DELAY(10000);
#endif
    if (!ath_hal_wait(ah, AR_RTC_RC, AR_RTC_RC_M, 0, AH_WAIT_TIMEOUT)) {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: RTC stuck in MAC reset\n", __FUNCTION__);
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: AR_RTC_RC = 0x%x\n", __func__, OS_REG_READ(ah, AR_RTC_RC));
        return AH_FALSE;
    }

    /* Clear AHB reset */
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_RC), 0);

    ar9300AttachHwPlatform(ah);

    return AH_TRUE;
}

static inline HAL_BOOL
ar9300SetResetPowerOn(struct ath_hal *ah)
{
    /* Force wake */
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_WA), AH9300(ah)->ah_WARegVal);
    OS_DELAY(10); // delay to allow AR_WA reg write to kick in.
    OS_REG_WRITE(ah, AR_RTC_FORCE_WAKE, AR_RTC_FORCE_WAKE_EN |
                                     AR_RTC_FORCE_WAKE_ON_INT);
#ifdef AR9300_EMULATION_BB
    OS_DELAY(10000);
#endif
    /*
     * RTC reset and clear. Some delay in between is needed 
     * to give the chip time to settle.
     */
    OS_REG_WRITE(ah, AR_RTC_RESET, 0);
#ifdef AR9300_EMULATION
    OS_DELAY(10000);
#else
    OS_DELAY(2);
#endif
    OS_REG_WRITE(ah, AR_RTC_RESET, 1);
#ifdef AR9300_EMULATION
    OS_DELAY(10000);
#endif

    /*
     * Poll till RTC is ON
     */
    if (!ath_hal_wait(ah, AR_RTC_STATUS, AR_RTC_STATUS_M, AR_RTC_STATUS_ON,
#ifndef AR9300_EMULATION_BB
        AH_WAIT_TIMEOUT)) {
#else
        500000)) {
#endif
	    HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: RTC not waking up for %d\n", __FUNCTION__, AH_WAIT_TIMEOUT);
        return AH_FALSE;
    }

    /*
     * Read Revisions from Chip right after RTC is on for the first time.
     * This helps us detect the chip type early and initialize it accordingly.
     */
    ar9300ReadRevisions(ah);

    /*
     * Warm reset if we aren't really powering on,
     * just restarting the driver.
     */
    return ar9300SetReset(ah, HAL_RESET_WARM);
}

/*
 * Write the given reset bit mask into the reset register
 */
HAL_BOOL
ar9300SetResetReg(struct ath_hal *ah, u_int32_t type)
{
    /*
     * Set force wake
     */
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_WA), AH9300(ah)->ah_WARegVal);
    OS_DELAY(10); // delay to allow AR_WA reg write to kick in.
    OS_REG_WRITE(ah, AR_RTC_FORCE_WAKE,
             AR_RTC_FORCE_WAKE_EN | AR_RTC_FORCE_WAKE_ON_INT);

    switch (type) {
    case HAL_RESET_POWER_ON:
        return ar9300SetResetPowerOn(ah);
        break;
    case HAL_RESET_WARM:
    case HAL_RESET_COLD:
        return ar9300SetReset(ah, type);
        break;
    default:
        return AH_FALSE;
    }
}

/*
 * Places the PHY and Radio chips into reset.  A full reset
 * must be called to leave this state.  The PCI/MAC/PCU are
 * not placed into reset as we must receive interrupt to
 * re-enable the hardware.
 */
HAL_BOOL
ar9300PhyDisable(struct ath_hal *ah)
{
    if (!ar9300SetResetReg(ah, HAL_RESET_WARM)) {
        return AH_FALSE;
    }

#ifdef ATH_SUPPORT_LED
    if (AR_SREV_WASP(ah)) {
#define REG_READ(_reg)          *((volatile u_int32_t *)(_reg))
#define REG_WRITE(_reg, _val)   *((volatile u_int32_t *)(_reg)) = (_val);
#define ATH_GPIO_OE             0xB8040000
        if (IS_CHAN_2GHZ((AH_PRIVATE(ah)->ah_curchan))) {
            REG_WRITE(ATH_GPIO_OE, (REG_READ(ATH_GPIO_OE) | (0x1 << 13)));
        }
        else {
            REG_WRITE(ATH_GPIO_OE, (REG_READ(ATH_GPIO_OE) | (0x1 << 12)));
        }
    }
#endif
    if ( AR_SREV_OSPREY(ah) ) { 
        OS_REG_RMW(ah, AR_HOSTIF_REG(ah, AR_GPIO_OUTPUT_MUX1), 0x0, 0x1f);
    }

    ar9300InitPLL(ah, AH_NULL);

    return AH_TRUE;
}

/*
 * Places all of hardware into reset
 */
HAL_BOOL
ar9300Disable(struct ath_hal *ah)
{
    if (!ar9300SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE))
        return AH_FALSE;

    if (!ar9300SetResetReg(ah, HAL_RESET_COLD)) {
        return AH_FALSE;
    }

    ar9300InitPLL(ah, AH_NULL);

    return AH_TRUE;
}

/*
 * TODO: Only write the PLL if we're changing to or from CCK mode
 *
 * WARNING: The order of the PLL and mode registers must be correct.
 */
static inline void
ar9300SetRfMode(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    u_int32_t rfMode = 0;

    if (chan == AH_NULL)
        return;

    switch (ar9300Get11nHwPlatform(ah)) {
        case HAL_TRUE_CHIP:
            rfMode |= (IS_CHAN_B(chan) || IS_CHAN_G(chan))
                      ? AR_PHY_MODE_DYNAMIC : AR_PHY_MODE_OFDM;
            break;
        default:
            HALASSERT(0);
            break;
    }
    /*  Phy mode bits for 5GHz channels requiring Fast Clock */
    if ( IS_5GHZ_FAST_CLOCK_EN(ah, chan)) {
        rfMode |= (AR_PHY_MODE_DYNAMIC | AR_PHY_MODE_DYN_CCK_DISABLE);
    }
    OS_REG_WRITE(ah, AR_PHY_MODE, rfMode);
}

/*
 * Places the hardware into reset and then pulls it out of reset
 */
HAL_BOOL
ar9300ChipReset(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    struct ath_hal_9300     *ahp = AH9300(ah);
    OS_MARK(ah, AH_MARK_CHIPRESET, chan ? chan->channel : 0);

    /*
     * Warm reset is optimistic.
     */
    if (!ar9300SetResetReg(ah, HAL_RESET_WARM)) {
        return AH_FALSE;
    }

    /* Bring out of sleep mode (AGAIN) */
    if (!ar9300SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE)) {
        return AH_FALSE;
    }

    ahp->ah_chipFullSleep = AH_FALSE;

    if(AR_SREV_HORNET(ah)) {
        if(!ar9300InternalRegulatorApply(ah))
        {
	        return AH_FALSE;
        }
    }

    ar9300InitPLL(ah, chan);
#ifdef AR9300_EMULATION
    OS_DELAY(1000);
#endif

#ifndef AR9300_EMULATION
    /*
     * Perform warm reset before the mode/PLL/turbo registers
     * are changed in order to deactivate the radio.  Mode changes
     * with an active radio can result in corrupted shifts to the
     * radio device.
     */
    ar9300SetRfMode(ah, chan);
#endif

    return AH_TRUE;
}

/* ar9300SetupCalibration
 * Setup HW to collect samples used for current cal
 */
inline static void
ar9300SetupCalibration(struct ath_hal *ah, HAL_CAL_LIST *currCal)
{
    /* Select calibration to run */
    switch(currCal->calData->calType) {
	case IQ_MISMATCH_CAL:
	{
		/* Start calibration w/ 2^(INIT_IQCAL_LOG_COUNT_MAX+1) samples */
		OS_REG_RMW_FIELD(ah, AR_PHY_TIMING4,
						 AR_PHY_TIMING4_IQCAL_LOG_COUNT_MAX,
						 currCal->calData->calCountMax);
        OS_REG_WRITE(ah, AR_PHY_CALMODE, AR_PHY_CALMODE_IQ);

        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "%s: starting IQ Mismatch Calibration\n", __func__);

		/* Kick-off cal */
		OS_REG_SET_BIT(ah, AR_PHY_TIMING4, AR_PHY_TIMING4_DO_CAL);

        break;
	}
	case TEMP_COMP_CAL:
	{
		if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah) || AR_SREV_WASP(ah)) {
			OS_REG_RMW_FIELD(ah, AR_HORNET_CH0_THERM, AR_PHY_65NM_CH0_THERM_LOCAL, 1);
			OS_REG_RMW_FIELD(ah, AR_HORNET_CH0_THERM, AR_PHY_65NM_CH0_THERM_START, 1);
		}
		else {
			OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_THERM, AR_PHY_65NM_CH0_THERM_LOCAL, 1);
			OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_THERM, AR_PHY_65NM_CH0_THERM_START, 1);
		}

		HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "%s: starting Temperature Compensation Calibration\n", __func__);
		break;
	}
	default:
		HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s called with incorrect calibration type.\n", __func__);
    }
}

/* ar9300ResetCalibration
 * Initialize shared data structures and prepare a cal to be run.
 */
inline static void
ar9300ResetCalibration(struct ath_hal *ah, HAL_CAL_LIST *currCal)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    int i;

    /* Setup HW for new calibration */
    ar9300SetupCalibration(ah, currCal);

    /* Change SW state to RUNNING for this calibration */
    currCal->calState = CAL_RUNNING;

    /* Reset data structures shared between different calibrations */
    for(i = 0; i < AR9300_MAX_CHAINS; i++) {
        ahp->ah_Meas0.sign[i] = 0;
        ahp->ah_Meas1.sign[i] = 0;
        ahp->ah_Meas2.sign[i] = 0;
        ahp->ah_Meas3.sign[i] = 0;
    }

    ahp->ah_CalSamples = 0;
}

/*
 * Find out which of the RX chains are enabled
 */
static u_int32_t
ar9300GetRxChainMask( struct ath_hal *ah)
{
    u_int32_t retVal = OS_REG_READ(ah, AR_PHY_RX_CHAINMASK);
    /* The bits [2:0] indicate the rx chain mask and are to be
     * interpreted as follows:
     * 00x => Only chain 0 is enabled
     * 01x => Chain 1 and 0 enabled
     * 1xx => Chain 2,1 and 0 enabled
     */
    return (retVal & 0x7);
}

static void
ar9300LoadNF(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan, int is_scan)
{
    HAL_NFCAL_BASE *h_base;
    int i, j;
    int32_t val;
    /* XXX where are EXT regs defined */
    const u_int32_t ar9300_cca_regs[] = {
        AR_PHY_CCA_0,
        AR_PHY_CCA_1,
        AR_PHY_CCA_2,
        AR_PHY_EXT_CCA,
        AR_PHY_EXT_CCA_1,
        AR_PHY_EXT_CCA_2,
    };
    u_int8_t rxChainStatus, chainmask;

    /*
     * Find the chains that are active because of Power save mode
     */
    rxChainStatus = ar9300GetRxChainMask(ah);

    /*
     * Force NF calibration for all chains, otherwise Vista station
     * would conduct a bad performance
     */
    if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah)) {
        chainmask = 0x9;
    } else if (AR_SREV_WASP(ah)) {
        chainmask = 0x1b;
    } else {
        chainmask = 0x3F;
    }

    /*
     * Write filtered NF values into maxCCApwr register parameter
     * so we can load below.
     */
#ifdef ATH_NF_PER_CHAN
    h_base = &chan->nfCalHist.base;
#else
    if (is_scan) {
        /*
         * The channel we are currently on is not the home channel,
         * so we shouldn't use the home channel NF buffer's values on
         * this channel.  Instead, use the NF single value already
         * read for this channel.  (Or, if we haven't read the NF for
         * this channel yet, the SW default for this chip/band will
         * be used.)
         */
        h_base = &chan->nfCalHist.base;
    } else {
        /* use the home channel NF info */
        h_base = &AH_PRIVATE(ah)->nfCalHist.base;
    }
#endif

    // XXX: Change for HT40
    for (i = 0; i < NUM_NF_READINGS; i ++) {
        if (chainmask & (1 << i)) {
            val = OS_REG_READ(ah, ar9300_cca_regs[i]);
            val &= 0xFFFFFE00;
            val |= (((u_int32_t)(h_base->privNF[i]) << 1) & 0x1ff);
            OS_REG_WRITE(ah, ar9300_cca_regs[i], val);
        }
    }

    /*
     * Load software filtered NF value into baseband internal minCCApwr
     * variable.
     */
    OS_REG_CLR_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_ENABLE_NF);
    OS_REG_CLR_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NO_UPDATE_NF);
    OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);

    /* Wait for load to complete, should be fast, a few 10s of us. */
    /* Changed the max delay 250us back to 10000us, since 250us often
     * results in NF load timeout and causes deaf condition
     * during stress testing 12/12/2009 */
    for (j = 0; j < 10000; j++) {
        if ((OS_REG_READ(ah, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) == 0)
            break;
#if defined(AR9330_EMULATION) || defined(AR9485_EMULATION)
        OS_DELAY(100);
#else
        OS_DELAY(10);
#endif
    }
    if (j == 10000) {
        /*
         * We timed out waiting for the noisefloor to load, probably due to an in-progress rx.
         * Simply return here and allow the load plenty of time to complete before the next 
         * calibration interval.  We need to avoid trying to load -50 (which happens below) 
         * while the previous load is still in progress as this can cause rx deafness 
         * (see EV 66368,62830).  Instead by returning here, the baseband nf cal will 
         * just be capped by our present noisefloor until the next calibration timer.
         */
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "%s: *** TIMEOUT while waiting for nf to load: AR_PHY_AGC_CONTROL=0x%x ***\n", 
                 __func__, OS_REG_READ(ah, AR_PHY_AGC_CONTROL));
        return;
    }

    /*
     * Restore maxCCAPower register parameter again so that we're not capped
     * by the median we just loaded.  This will be initial (and max) value
     * of next noise floor calibration the baseband does.
     */
    for (i = 0; i < NUM_NF_READINGS; i ++) {
        if (chainmask & (1 << i)) {
            val = OS_REG_READ(ah, ar9300_cca_regs[i]);
            val &= 0xFFFFFE00;
            val |= (((u_int32_t)(-50) << 1) & 0x1ff);
            OS_REG_WRITE(ah, ar9300_cca_regs[i], val);
        }
    }
}

/* ar9300PerCalibration
 * Generic calibration routine.
 * Recalibrate the lower PHY chips to account for temperature/environment
 * changes.
 */
inline static void
ar9300PerCalibration(struct ath_hal *ah,  HAL_CHANNEL_INTERNAL *ichan,
                     u_int8_t rxchainmask, HAL_CAL_LIST *currCal,
                     HAL_BOOL *isCalDone)
{
    struct ath_hal_9300     *ahp = AH9300(ah);

    /* Cal is assumed not done until explicitly set below */
    *isCalDone = AH_FALSE;

    /* Calibration in progress. */
    if (currCal->calState == CAL_RUNNING) {
        /* Check to see if it has finished. */
        if (!(OS_REG_READ(ah,
            AR_PHY_TIMING4) & AR_PHY_TIMING4_DO_CAL)) {
            int i, numChains = 0;
            for (i = 0; i < AR9300_MAX_CHAINS; i++) {
                if (rxchainmask & (1 << i)) {
                    numChains++;
                }
            }

            /*
             * Accumulate cal measures for active chains
             */
            currCal->calData->calCollect(ah, numChains);

            ahp->ah_CalSamples++;

            if (ahp->ah_CalSamples >= currCal->calData->calNumSamples) {
                /*
                 * Process accumulated data
                 */
                currCal->calData->calPostProc(ah, numChains);

                /* Calibration has finished. */
                ichan->CalValid |= currCal->calData->calType;
                currCal->calState = CAL_DONE;
                *isCalDone = AH_TRUE;
            } else {
                /* Set-up collection of another sub-sample until we
                 * get desired number
                 */
                ar9300SetupCalibration(ah, currCal);
            }
        }
    } else if (!(ichan->CalValid & currCal->calData->calType)) {
        /* If current cal is marked invalid in channel, kick it off */
        ar9300ResetCalibration(ah, currCal);
    }
}

static void
ar9300StartNFCal(struct ath_hal *ah)
{
    OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_ENABLE_NF);
    OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NO_UPDATE_NF);
    OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);
    AH9300(ah)->nf_tsf32 = ar9300GetTsf32(ah);
}

/* ar9300Calibration
 * Wrapper for a more generic Calibration routine. Primarily to abstract to
 * upper layers whether there is 1 or more calibrations to be run.
 */
HAL_BOOL
ar9300Calibration(struct ath_hal *ah,  HAL_CHANNEL *chan, u_int8_t rxchainmask,
                  HAL_BOOL do_nf_cal, HAL_BOOL *isCalDone, int is_scan, u_int32_t *sched_cals)
{
#ifndef AR9340_EMULATION
    struct ath_hal_9300 *ahp = AH9300(ah);
    HAL_CAL_LIST *currCal = ahp->ah_cal_list_curr;
    HAL_CHANNEL_INTERNAL *ichan = ath_hal_checkchannel(ah, chan);

    *isCalDone = AH_TRUE;

    /* XXX: For initial wasp bringup - disable periodic calibration */
    /* Invalid channel check */
    if (ichan == AH_NULL) {
        HDPRINTF(ah, HAL_DBG_CHANNEL,
                 "%s: invalid channel %u/0x%x; no mapping\n",
                 __func__, chan->channel, chan->channelFlags);
        return AH_FALSE;
    }

    HDPRINTF(ah, HAL_DBG_CALIBRATE,
             "%s: Entering, Doing NF Cal = %d\n", __func__, do_nf_cal);

    HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: Chain 0 Rx IQ Cal Correction 0x%08x\n",
			 __func__, OS_REG_READ(ah, AR_PHY_RX_IQCAL_CORR_B0));
    if (!AR_SREV_HORNET(ah) && !AR_SREV_POSEIDON(ah)) {
        HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: Chain 1 Rx IQ Cal Correction 0x%08x\n",
			 __func__, OS_REG_READ(ah, AR_PHY_RX_IQCAL_CORR_B1));
        if (!AR_SREV_WASP(ah)) {
            HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: Chain 2 Rx IQ Cal Correction 0x%08x\n",
			     __func__, OS_REG_READ(ah, AR_PHY_RX_IQCAL_CORR_B2));
        }
    }

    OS_MARK(ah, AH_MARK_PERCAL, chan->channel);

    /* For given calibration:
     * 1. Call generic cal routine
     * 2. When this cal is done (isCalDone) if we have more cals waiting
     *    (eg after reset), mask this to upper layers by not propagating
     *    isCalDone if it is set to TRUE.
     *    Instead, change isCalDone to FALSE and setup the waiting cal(s)
     *    to be run.
     */
    if (currCal && (currCal->calData->calType & *sched_cals) &&
        (currCal->calState == CAL_RUNNING ||
         currCal->calState == CAL_WAITING))
    {
        ar9300PerCalibration(ah, ichan, rxchainmask, currCal, isCalDone);

        if (*isCalDone == AH_TRUE) {
            ahp->ah_cal_list_curr = currCal = currCal->calNext;

            if (currCal && currCal->calState == CAL_WAITING) {
                *isCalDone = AH_FALSE;
                ar9300ResetCalibration(ah, currCal);
            } else {
				*sched_cals &= ~IQ_MISMATCH_CAL;
			}
        }
    }

    /* Do NF cal only at longer intervals */
    if (do_nf_cal) {
        int nf_done;

        /* Get the value from the previous NF cal and update history buffer */
        nf_done = ar9300StoreNewNf(ah, ichan, is_scan);
        if (ichan->channelFlags & CHANNEL_CW_INT) {
             chan->channelFlags |= CHANNEL_CW_INT;
        }
        ichan->channelFlags &= (~CHANNEL_CW_INT);



        if (nf_done) {
            /*
             * Load the NF from history buffer of the current channel.
             * NF is slow time-variant, so it is OK to use a historical value.
             */
            ar9300LoadNF(ah, AH_PRIVATE(ah)->ah_curchan, is_scan);
    
            /* start NF calibration, without updating BB NF register*/
            ar9300StartNFCal(ah);
        }
    }
#endif
    return AH_TRUE;
}

/* ar9300IQCalCollect
 * Collect data from HW to later perform IQ Mismatch Calibration
 */
void
ar9300IQCalCollect(struct ath_hal *ah, u_int8_t numChains)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    int i;

    /*
     * Accumulate IQ cal measures for active chains
     */
    for (i = 0; i < numChains; i++) {
        ahp->ah_totalPowerMeasI[i] = OS_REG_READ(ah, AR_PHY_CAL_MEAS_0(i));
        ahp->ah_totalPowerMeasQ[i] = OS_REG_READ(ah, AR_PHY_CAL_MEAS_1(i));
        ahp->ah_totalIqCorrMeas[i] = (int32_t)OS_REG_READ(ah,
                                              AR_PHY_CAL_MEAS_2(i));
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "%d: Chn %d "
                 "Reg Offset(0x%04x)pmi=0x%08x; "
                 "Reg Offset(0x%04x)pmq=0x%08x; "
                 "Reg Offset (0x%04x)iqcm=0x%08x;\n",
                 ahp->ah_CalSamples,
                 i,
                 (unsigned) AR_PHY_CAL_MEAS_0(i),
                 ahp->ah_totalPowerMeasI[i],
                 (unsigned) AR_PHY_CAL_MEAS_1(i),
                 ahp->ah_totalPowerMeasQ[i],
                 (unsigned) AR_PHY_CAL_MEAS_2(i),
                 ahp->ah_totalIqCorrMeas[i]);
    }
}

/* ar9300IQCalibration
 * Use HW data to perform IQ Mismatch Calibration
 */
void
ar9300IQCalibration(struct ath_hal *ah, u_int8_t numChains)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    u_int32_t powerMeasQ, powerMeasI, iqCorrMeas;
    u_int32_t qCoffDenom, iCoffDenom;
    int32_t qCoff, iCoff;
    int iqCorrNeg, i;
	static const u_int32_t offset_array[3] = {
                                              AR_PHY_RX_IQCAL_CORR_B0,
		                                      AR_PHY_RX_IQCAL_CORR_B1,
		                                      AR_PHY_RX_IQCAL_CORR_B2,
								             };

    for (i = 0; i < numChains; i++) {
        if (AR_SREV_POSEIDON(ah)) {
        	HALASSERT(numChains == 0x1);
        }
        powerMeasI = ahp->ah_totalPowerMeasI[i];
        powerMeasQ = ahp->ah_totalPowerMeasQ[i];
        iqCorrMeas = ahp->ah_totalIqCorrMeas[i];

        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "Starting IQ Cal and Correction for Chain %d\n", i);

        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "Orignal: Chn %diq_corr_meas = 0x%08x\n",
                 i, ahp->ah_totalIqCorrMeas[i]);

        iqCorrNeg = 0;

        /* iqCorrMeas is always negative. */
        if (iqCorrMeas > 0x80000000)  {
            iqCorrMeas = (0xffffffff - iqCorrMeas) + 1;
            iqCorrNeg = 1;
        }

        HDPRINTF(ah, HAL_DBG_CALIBRATE, "Chn %d pwr_meas_i = 0x%08x\n",
                 i, powerMeasI);
        HDPRINTF(ah, HAL_DBG_CALIBRATE, "Chn %d pwr_meas_q = 0x%08x\n",
                 i, powerMeasQ);
        HDPRINTF(ah, HAL_DBG_CALIBRATE, "iqCorrNeg is 0x%08x\n", iqCorrNeg);

        iCoffDenom = (powerMeasI/2 + powerMeasQ/2)/ 256;
        qCoffDenom = powerMeasQ / 64;

        /* Protect against divide-by-0 */
        if ((iCoffDenom != 0) && (qCoffDenom != 0)) {
            /* IQ corr_meas is already negated if iqcorr_neg == 1 */
            iCoff = iqCorrMeas/iCoffDenom;
            qCoff = powerMeasI/qCoffDenom - 64;
            HDPRINTF(ah, HAL_DBG_CALIBRATE, "Chn %d iCoff = 0x%08x\n",
                     i, iCoff);
            HDPRINTF(ah, HAL_DBG_CALIBRATE, "Chn %d qCoff = 0x%08x\n",
                     i, qCoff);

			/* Force bounds on iCoff */
            if (iCoff >= 63) {
                iCoff = 63;
            } else if (iCoff <= -63) {
                iCoff = -63;
            }

            /* Negate iCoff if iqCorrNeg == 0 */
            if (iqCorrNeg == 0x0) {
                iCoff = -iCoff;
            }

			/* Force bounds on qCoff */
            if (qCoff >= 63) {
                qCoff = 63;
            } else if (qCoff <= -63) {
                qCoff = -63;
            }

            iCoff = iCoff & 0x7f;
            qCoff = qCoff & 0x7f;

            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                     "Chn %d : iCoff = 0x%x  qCoff = 0x%x\n", i, iCoff, qCoff);

            HDPRINTF(ah, HAL_DBG_CALIBRATE,
		     "Register offset (0x%04x) before update = 0x%x\n",
		     offset_array[i], OS_REG_READ(ah, offset_array[i]));

            OS_REG_RMW_FIELD(ah, offset_array[i],
                             AR_PHY_RX_IQCAL_CORR_IQCORR_Q_I_COFF, iCoff);
            OS_REG_RMW_FIELD(ah, offset_array[i],
                             AR_PHY_RX_IQCAL_CORR_IQCORR_Q_Q_COFF, qCoff);

            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                     "Register offset (0x%04x) QI COFF (bitfields 0x%08x) after update = 0x%x\n",
                     offset_array[i], AR_PHY_RX_IQCAL_CORR_IQCORR_Q_I_COFF,
                     OS_REG_READ(ah, offset_array[i]));
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                     "Register offset (0x%04x) QQ COFF (bitfields 0x%08x) after update = 0x%x\n",
                     offset_array[i], AR_PHY_RX_IQCAL_CORR_IQCORR_Q_Q_COFF,
                     OS_REG_READ(ah, offset_array[i]));

            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                     "IQ Cal and Correction done for Chain %d\n", i);
        }
    }

    OS_REG_SET_BIT(ah, AR_PHY_RX_IQCAL_CORR_B0, 
                   AR_PHY_RX_IQCAL_CORR_IQCORR_ENABLE);

    HDPRINTF(ah, HAL_DBG_CALIBRATE,
             "IQ Cal and Correction (offset 0x%04x) enabled "
             "(bit position 0x%08x). New Value 0x%08x\n",
             (unsigned) (AR_PHY_RX_IQCAL_CORR_B0),
             AR_PHY_RX_IQCAL_CORR_IQCORR_ENABLE,
             OS_REG_READ(ah, AR_PHY_RX_IQCAL_CORR_B0));
}

/*
 * Set a limit on the overall output power.  Used for dynamic
 * transmit power control and the like.
 *
 * NB: limit is in units of 0.5 dbM.
 */
HAL_BOOL
ar9300SetTxPowerLimit(struct ath_hal *ah, u_int32_t limit, u_int16_t extra_txpow, u_int16_t tpcInDb)
{
#ifndef AR9300_EMULATION_BB
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    HAL_CHANNEL_INTERNAL *ichan = ahpriv->ah_curchan;
    HAL_CHANNEL *chan = (HAL_CHANNEL *)ichan;

    if (NULL == chan) {
        return AH_FALSE; 
    }
    ahpriv->ah_powerLimit = AH_MIN(limit, MAX_RATE_POWER);
    ahpriv->ah_extra_txpow = extra_txpow;
    if (ar9300EepromSetTransmitPower(ah, &ahp->ah_eeprom, ichan,
        ath_hal_getctl(ah, chan), ath_hal_getantennaallowed(ah, chan),
        ath_hal_getTwiceMaxRegPower(ahpriv, ichan, chan),
        AH_MIN(MAX_RATE_POWER, ahpriv->ah_powerLimit)) != HAL_OK)
        return AH_FALSE;
#endif
    return AH_TRUE;
}

/*
 * Exported call to check for a recent gain reading and return
 * the current state of the thermal calibration gain engine.
 */
HAL_RFGAIN
ar9300GetRfgain(struct ath_hal *ah)
{
    return HAL_RFGAIN_INACTIVE;
}

/*
 * Search a list for a specified value v that is within
 * EEP_DELTA of the search values.  Return the closest
 * values in the list above and below the desired value.
 * EEP_DELTA is a factional value; everything is scaled
 * so only integer arithmetic is used.
 *
 * NB: the input list is assumed to be sorted in ascending order
 */
void
ar9300GetLowerUpperValues(u_int16_t v, u_int16_t *lp, u_int16_t listSize,
                          u_int16_t *vlo, u_int16_t *vhi)
{
        u_int32_t target = v * EEP_SCALE;
        u_int16_t *ep = lp+listSize;

        /*
         * Check first and last elements for out-of-bounds conditions.
         */
        if (target < (u_int32_t)(lp[0] * EEP_SCALE - EEP_DELTA)) {
                *vlo = *vhi = lp[0];
                return;
        }
        if (target > (u_int32_t)(ep[-1] * EEP_SCALE + EEP_DELTA)) {
                *vlo = *vhi = ep[-1];
                return;
        }

        /* look for value being near or between 2 values in list */
        for (; lp < ep; lp++) {
                /*
                 * If value is close to the current value of the list
                 * then target is not between values, it is one of the values
                 */
                if (abs((int16_t)lp[0] * EEP_SCALE - (int16_t)target) < EEP_DELTA) {
                        *vlo = *vhi = lp[0];
                        return;
                }
                /*
                 * Look for value being between current value and next value
                 * if so return these 2 values
                 */
                if (target < (u_int32_t)(lp[1] * EEP_SCALE - EEP_DELTA)) {
                        *vlo = lp[0];
                        *vhi = lp[1];
                        return;
                }
        }
}

/*
 * Perform analog "swizzling" of parameters into their location
 */
void
ar9300ModifyRfBuffer(u_int32_t *rfBuf, u_int32_t reg32, u_int32_t numBits,
                     u_int32_t firstBit, u_int32_t column)
{
        u_int32_t tmp32, mask, arrayEntry, lastBit;
        int32_t bitPosition, bitsLeft;

        HALASSERT(column <= 3);
        HALASSERT(numBits <= 32);
        HALASSERT(firstBit + numBits <= MAX_ANALOG_START);

        tmp32 = ath_hal_reverseBits(reg32, numBits);
        arrayEntry = (firstBit - 1) / 8;
        bitPosition = (firstBit - 1) % 8;
        bitsLeft = numBits;
        while (bitsLeft > 0) {
                lastBit = (bitPosition + bitsLeft > 8) ?
                        8 : bitPosition + bitsLeft;
                mask = (((1 << lastBit) - 1) ^ ((1 << bitPosition) - 1)) <<
                        (column * 8);
                rfBuf[arrayEntry] &= ~mask;
                rfBuf[arrayEntry] |= ((tmp32 << bitPosition) <<
                        (column * 8)) & mask;
                bitsLeft -= 8 - bitPosition;
                tmp32 = tmp32 >> (8 - bitPosition);
                bitPosition = 0;
                arrayEntry++;
        }
}

#define HAL_GREEN_AP_RX_MASK 0x1

static inline void
ar9300InitChainMasks(struct ath_hal *ah, int rx_chainmask, int tx_chainmask)
{
    if( AH_PRIVATE(ah)->greenApPsOn ) rx_chainmask = HAL_GREEN_AP_RX_MASK;
    
    if (rx_chainmask == 0x5) {
        OS_REG_SET_BIT(ah, AR_PHY_ANALOG_SWAP, AR_PHY_SWAP_ALT_CHAIN);
    }
    OS_REG_WRITE(ah, AR_PHY_RX_CHAINMASK, rx_chainmask);
    OS_REG_WRITE(ah, AR_PHY_CAL_CHAINMASK, rx_chainmask);

    /*
     * Adaptive Power Management:
     * Some 3 stream chips exceed the PCIe power requirements.
     * This workaround will reduce power consumption by using 2 tx chains
     * for 1 and 2 stream rates (5 GHz only).
     *
     * Set the self gen mask to 2 tx chains when APM is enabled.
     *
     */
    if (AH_PRIVATE(ah)->ah_caps.halEnableApm && (tx_chainmask == 0x7)) {
        OS_REG_WRITE(ah, AR_SELFGEN_MASK, 0x3);
    } else {
    OS_REG_WRITE(ah, AR_SELFGEN_MASK, tx_chainmask);
    }

    if (tx_chainmask == 0x5) {
        OS_REG_SET_BIT(ah, AR_PHY_ANALOG_SWAP, AR_PHY_SWAP_ALT_CHAIN);
    }
}

/*
 * Override INI values with chip specific configuration.
 */
static inline void
ar9300OverrideIni(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    u_int32_t val;
    HAL_CAPABILITIES *pCap = &AH_PRIVATE(ah)->ah_caps;

    /*
     * Set the RX_ABORT and RX_DIS and clear it only after
     * RXE is set for MAC. This prevents frames with
     * corrupted descriptor status.
     */
    OS_REG_SET_BIT(ah, AR_DIAG_SW, (AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT));
    /*
     * For Merlin and above, there is a new feature that allows Multicast search
     * based on both MAC Address and Key ID. By default, this feature is enabled.
     * But since the driver is not using this feature, we switch it off; otherwise
     * multicast search based on MAC addr only will fail.
     */
    val = OS_REG_READ(ah, AR_PCU_MISC_MODE2) & (~AR_ADHOC_MCAST_KEYID_ENABLE);
    OS_REG_WRITE(ah, AR_PCU_MISC_MODE2, val | AR_BUG_58603_FIX_ENABLE | AR_AGG_WEP_ENABLE);


#ifdef AR9300_EMULATION
    val = OS_REG_READ(ah, AR_MAC_PCU_LOGIC_ANALYZER);
    OS_REG_WRITE(ah, AR_MAC_PCU_LOGIC_ANALYZER, val | AR_MAC_PCU_LOGIC_ANALYZER_DISBUG20768);
#endif


    /* Osprey revision specific configuration */

    /* Osprey 2.0+ - if SW RAC support is disabled, must also disable
     * the Osprey 2.0 hardware RAC fix.
     */
    if (pCap->halIsrRacSupport == AH_FALSE) {
        OS_REG_CLR_BIT(ah, AR_CFG, AR_CFG_MISSING_TX_INTR_FIX_ENABLE);
    }
#ifdef AR9340_EMULATION
    OS_REG_WRITE(ah, 0xa238, 0xcfbc1018); // BB_frame_control
    OS_REG_WRITE(ah, 0xa2d8, 0x7999a83a); // BB_cl_cal_ctrl
    OS_REG_WRITE(ah, 0xae04, 0x00001000); // BB_gain_force_max_gains_b1
    OS_REG_WRITE(ah, 0xa3f8, 0x0cdbd381); // BB_tpc_1
    OS_REG_WRITE(ah, 0x9e08, 0x0040233c); // BB_gains_min_offsets
    OS_REG_WRITE(ah, 0xa204, 0x37c0);    // Static HT20
#endif

#ifndef ART_BUILD
    /* try to enable old pal if it is needed for h/w green tx */
    ar9300_hwgreentx_set_pal_spare(ah, 1);
#endif
}

static inline void
ar9300ProgIni(struct ath_hal *ah, struct ar9300IniArray *iniArr, int column)
{
    int i, regWrites = 0;

    /* New INI format: Array may be undefined (pre, core, post arrays) */
    if (iniArr->ia_array == NULL) {
        return;
    }

    /*
     * New INI format: Pre, core, and post arrays for a given subsystem may be
     * modal (> 2 columns) or non-modal (2 columns).
     * Determine if the array is non-modal and force the column to 1.
     */
    if (column >= iniArr->ia_columns) {
        column = 1;
    }

    for (i = 0; i < iniArr->ia_rows; i++) {
        u_int32_t reg = INI_RA(iniArr, i, 0);
        u_int32_t val = INI_RA(iniArr, i, column);

        /*
        ** Determine if this is a shift register value 
        ** (reg >= 0x16000 && reg < 0x17000 for Osprey) , 
        ** and insert the configured delay if so. 
        ** -this delay is not required for Osprey (EV#71410)
        */
        OS_REG_WRITE(ah, reg, val);
        WAR_6773(regWrites);

#ifdef AR9300_EMULATION_BB
#if defined(AR9330_EMULATION) || defined(AR9485_EMULATION) /* Follow the MDK setting */
        if (AR_SREV_POSEIDON(ah)) {
            if (reg >=0x7800 && reg <=0x7898) {
                OS_DELAY(400);
            } else {
        OS_DELAY(200);
            }
        } else {
        OS_DELAY(200);
        }
#else
        OS_DELAY(10);

#ifdef AR9340_EMULATION
        if (reg >= 0x7800 && reg <=0x7898) {
            int j;
            OS_DELAY(100);
            for (j=0; j<10; j++)
                OS_REG_READ(ah, 0xa25c);
        }
#endif
#endif
#endif
	}
}

static inline HAL_STATUS
ar9300ProcessIni(struct ath_hal *ah, HAL_CHANNEL *chan,
                 HAL_CHANNEL_INTERNAL *ichan,
                 HAL_HT_MACMODE macmode)
{
#define N(a)    (sizeof(a) / sizeof(a[0]))
	int regWrites = 0;
    struct ath_hal_9300 *ahp = AH9300(ah);
    u_int modesIndex, freqIndex;
    int i;
//#ifdef NOTYET
#ifndef AR9300_EMULATION_BB
    HAL_STATUS status;
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
//#endif
#endif
    /* Setup the indices for the next set of register array writes */
    switch (chan->channelFlags & CHANNEL_ALL) {
        /* TODO:
         * If the channel marker is indicative of the current mode rather
         * than capability, we do not need to check the phy mode below.
         */
        case CHANNEL_A:
        case CHANNEL_A_HT20:
            modesIndex = 1;
            freqIndex  = 1;
            break;

        case CHANNEL_A_HT40PLUS:
        case CHANNEL_A_HT40MINUS:
            modesIndex = 2;
            freqIndex  = 1;
            break;

        case CHANNEL_PUREG:
        case CHANNEL_G_HT20:
        case CHANNEL_B:
            modesIndex = 4;
            freqIndex  = 2;
            break;

        case CHANNEL_G_HT40PLUS:
        case CHANNEL_G_HT40MINUS:
            modesIndex = 3;
            freqIndex  = 2;
            break;

        case CHANNEL_108G:
            modesIndex = 5;
            freqIndex  = 2;
            break;

        default:
            HALASSERT(0);
            return HAL_EINVAL;
    }

#if 0
    /* Set correct Baseband to analog shift setting to access analog chips. */
    OS_REG_WRITE(ah, AR_PHY(0), 0x00000007);
#endif

    HDPRINTF(ah, HAL_DBG_RESET, "ar9300ProcessIni: Skipping OS-REG-WRITE(ah, AR-PHY(0), 0x00000007)\n");

    HDPRINTF(ah, HAL_DBG_RESET, "ar9300ProcessIni: no ADDac programming\n");

    /* Osprey 2.0+ - new INI format.  Each subsystem has a pre, core, and post array. */
    for (i = 0; i < ATH_INI_NUM_SPLIT; i++) {
        ar9300ProgIni(ah, &ahp->ah_iniSOC[i], modesIndex);
        ar9300ProgIni(ah, &ahp->ah_iniMac[i], modesIndex);
        ar9300ProgIni(ah, &ahp->ah_iniBB[i], modesIndex);
        ar9300ProgIni(ah, &ahp->ah_iniRadio[i], modesIndex);

#ifdef AR9300_EMULATION
        ar9300ProgIni(ah, &ahp->ah_iniSOCEmu[i], modesIndex);
        ar9300ProgIni(ah, &ahp->ah_iniMacEmu[i], modesIndex);
        ar9300ProgIni(ah, &ahp->ah_iniBBEmu[i], modesIndex);
        ar9300ProgIni(ah, &ahp->ah_iniRadioEmu[i], modesIndex);
#endif
    }

    if (!(AR_SREV_SOC(ah))) {
        /* Doubler issue : Some board doesn't work well with MCS15. Turn off doubler after freq locking is complete*/
        //ath_hal_printf(ah, "%s[%d] ==== before reg[0x%08x] = 0x%08x\n", __func__, __LINE__, AR_PHY_65NM_CH0_RXTX2, OS_REG_READ(ah, AR_PHY_65NM_CH0_RXTX2));
        OS_REG_RMW(ah, AR_PHY_65NM_CH0_RXTX2, 1 << AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK_S | 
                       1 << AR_PHY_65NM_CH0_RXTX2_SYNTHOVR_MASK_S, 0); /*Set synthon, synthover */
        //ath_hal_printf(ah, "%s[%d] ==== after reg[0x%08x] = 0x%08x\n", __func__, __LINE__, AR_PHY_65NM_CH0_RXTX2, OS_REG_READ(ah, AR_PHY_65NM_CH0_RXTX2));
            
        OS_REG_RMW(ah, AR_PHY_65NM_CH1_RXTX2, 1 << AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK_S | 
                       1 << AR_PHY_65NM_CH0_RXTX2_SYNTHOVR_MASK_S, 0); /*Set synthon, synthover */
        OS_REG_RMW(ah, AR_PHY_65NM_CH2_RXTX2, 1 << AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK_S | 
                       1 << AR_PHY_65NM_CH0_RXTX2_SYNTHOVR_MASK_S, 0); /*Set synthon, synthover */
        OS_DELAY(200);
            
        //ath_hal_printf(ah, "%s[%d] ==== before reg[0x%08x] = 0x%08x\n", __func__, __LINE__, AR_PHY_65NM_CH0_RXTX2, OS_REG_READ(ah, AR_PHY_65NM_CH0_RXTX2));
        OS_REG_CLR_BIT(ah, AR_PHY_65NM_CH0_RXTX2, AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK); /* clr synthon */
        OS_REG_CLR_BIT(ah, AR_PHY_65NM_CH1_RXTX2, AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK); /* clr synthon */
        OS_REG_CLR_BIT(ah, AR_PHY_65NM_CH2_RXTX2, AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK); /* clr synthon */
        //ath_hal_printf(ah, "%s[%d] ==== after reg[0x%08x] = 0x%08x\n", __func__, __LINE__, AR_PHY_65NM_CH0_RXTX2, OS_REG_READ(ah, AR_PHY_65NM_CH0_RXTX2));
            
        OS_DELAY(1);
            
        //ath_hal_printf(ah, "%s[%d] ==== before reg[0x%08x] = 0x%08x\n", __func__, __LINE__, AR_PHY_65NM_CH0_RXTX2, OS_REG_READ(ah, AR_PHY_65NM_CH0_RXTX2));
        OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX2, AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK, 1); /* set synthon */
        OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX2, AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK, 1); /* set synthon */
        OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX2, AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK, 1); /* set synthon */
        //ath_hal_printf(ah, "%s[%d] ==== after reg[0x%08x] = 0x%08x\n", __func__, __LINE__, AR_PHY_65NM_CH0_RXTX2, OS_REG_READ(ah, AR_PHY_65NM_CH0_RXTX2));
            
        OS_DELAY(200);
            
        //ath_hal_printf(ah, "%s[%d] ==== before reg[0x%08x] = 0x%08x\n", __func__, __LINE__, AR_PHY_65NM_CH0_SYNTH12, OS_REG_READ(ah, AR_PHY_65NM_CH0_SYNTH12));
        OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_SYNTH12, AR_PHY_65NM_CH0_SYNTH12_VREFMUL3, 0xf);
        //OS_REG_CLR_BIT(ah, AR_PHY_65NM_CH0_SYNTH12, 1<< 16); /* clr charge pump */
        //ath_hal_printf(ah, "%s[%d] ==== After  reg[0x%08x] = 0x%08x\n", __func__, __LINE__, AR_PHY_65NM_CH0_SYNTH12, OS_REG_READ(ah, AR_PHY_65NM_CH0_SYNTH12));
            
        OS_REG_RMW(ah, AR_PHY_65NM_CH0_RXTX2, 0, 1 << AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK_S | 
                       1 << AR_PHY_65NM_CH0_RXTX2_SYNTHOVR_MASK_S); /*Clr synthon, synthover */
        OS_REG_RMW(ah, AR_PHY_65NM_CH1_RXTX2, 0, 1 << AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK_S | 
                       1 << AR_PHY_65NM_CH0_RXTX2_SYNTHOVR_MASK_S); /*Clr synthon, synthover */
        OS_REG_RMW(ah, AR_PHY_65NM_CH2_RXTX2, 0, 1 << AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK_S | 
                       1 << AR_PHY_65NM_CH0_RXTX2_SYNTHOVR_MASK_S); /*Clr synthon, synthover */
        //ath_hal_printf(ah, "%s[%d] ==== after reg[0x%08x] = 0x%08x\n", __func__, __LINE__, AR_PHY_65NM_CH0_RXTX2, OS_REG_READ(ah, AR_PHY_65NM_CH0_RXTX2));
    }

    /* Write rxgain Array Parameters */
    REG_WRITE_ARRAY(&ahp->ah_iniModesRxgain, 1, regWrites);
    HDPRINTF(ah, HAL_DBG_RESET, "ar9300ProcessIni: Rx Gain programming\n");

#ifdef AR9300_EMULATION
    ar9300ProgIni(ah, &ahp->ah_iniRxGainEmu, 1);
#endif

    /* Write txgain Array Parameters */
    REG_WRITE_ARRAY(&ahp->ah_iniModesTxgain, modesIndex, regWrites);
    HDPRINTF(ah, HAL_DBG_RESET, "ar9300ProcessIni: Tx Gain programming\n");

    /* For 5GHz channels requiring Fast Clock, apply different modal values */
    if (IS_5GHZ_FAST_CLOCK_EN(ah, chan)) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s: Fast clock enabled, use special ini values\n", __func__);
        REG_WRITE_ARRAY(&ahp->ah_iniModesAdditional, modesIndex, regWrites);
    }

    if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah)) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s: use xtal ini for AH9300(ah)->clk_25mhz: %d\n", __func__, AH9300(ah)->clk_25mhz);
        REG_WRITE_ARRAY(&ahp->ah_iniModesAdditional, 1/*modesIndex*/, regWrites);
    }

    if (AR_SREV_WASP(ah) && (AH9300(ah)->clk_25mhz == 0)) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s: Apply 40MHz ini settings\n", __func__);
        REG_WRITE_ARRAY(&ahp->ah_iniModesAdditional_40M, 1/*modesIndex*/, regWrites);
    }

    if (2484 == chan->channel) {
        ar9300ProgIni(ah, &ahp->ah_iniJapan2484, 1);
    }

    /* Override INI with chip specific configuration */
    ar9300OverrideIni(ah, chan);

    /* Setup 11n MAC/Phy mode registers */
    ar9300Set11nRegs(ah, chan, macmode);

    /*
     * Moved ar9300InitChainMasks() here to ensure the swap bit is set before
     * the pdadc table is written.  Swap must occur before any radio dependent
     * replicated register access.  The pdadc curve addressing in particular
     * depends on the consistent setting of the swap bit.
     */
     ar9300InitChainMasks(ah, ahp->ah_rxchainmask, ahp->ah_txchainmask);

    /*
     * Setup the transmit power values.
     *
     * After the public to private hal channel mapping, ichan contains the
     * valid regulatory power value.
     * ath_hal_getctl and ath_hal_getantennaallowed look up ichan from chan.
     */
//#ifdef NOTYET
#ifndef AR9300_EMULATION_BB
    status = ar9300EepromSetTransmitPower(ah, &ahp->ah_eeprom, ichan,
             ath_hal_getctl(ah, chan), ath_hal_getantennaallowed(ah, chan),
             ath_hal_getTwiceMaxRegPower(ahpriv, ichan, chan),
             AH_MIN(MAX_RATE_POWER, ahpriv->ah_powerLimit));
    if (status != HAL_OK) {
        HDPRINTF(ah, HAL_DBG_POWER_MGMT, "%s: error init'ing transmit power\n", __func__);
        return HAL_EIO;
    }
//#endif
#endif

    return HAL_OK;
#undef N
}

/* ar9300IsCalSupp
 * Determine if calibration is supported by device and channel flags
 */
inline static HAL_BOOL
ar9300IsCalSupp(struct ath_hal *ah, HAL_CHANNEL *chan, HAL_CAL_TYPES calType) 
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    HAL_BOOL retval = AH_FALSE;

    switch(calType & ahp->ah_suppCals) {
    case IQ_MISMATCH_CAL:
        /* Run IQ Mismatch for non-CCK only */
        if (!IS_CHAN_B(chan))
            {retval = AH_TRUE;}
        break;
	case TEMP_COMP_CAL:
		retval = AH_TRUE;
		break;
    }

    return retval;
}


#if 0
/* ar9285PACal
 * PA Calibration for Kite 1.1 and later versions of Kite.
 * - from system's team.
 */
static inline void
ar9285PACal(struct ath_hal *ah)
{
    u_int32_t regVal;
    int i, loGn, offs_6_1, offs_0;
    u_int8_t reflo;
    u_int32_t phyTest2RegVal, phyAdcCtlRegVal, anTop2RegVal, phyTstDacRegVal;


    /* Kite 1.1 WAR for Bug 35666 
     * Increase the LDO value to 1.28V before accessing analog Reg */
    if (AR_SREV_KITE_11(ah))
        OS_REG_WRITE(ah, AR9285_AN_TOP4, (AR9285_AN_TOP4_DEFAULT | 0x14) );

    anTop2RegVal = OS_REG_READ(ah, AR9285_AN_TOP2);

    /* set pdv2i pdrxtxbb */
    regVal = OS_REG_READ(ah, AR9285_AN_RXTXBB1);
    regVal |= ((0x1 << 5) | (0x1 << 7));
    OS_REG_WRITE(ah, AR9285_AN_RXTXBB1, regVal);

    /* clear pwddb */
    regVal = OS_REG_READ(ah, AR9285_AN_RF2G7);
    regVal &= 0xfffffffd;
    OS_REG_WRITE(ah, AR9285_AN_RF2G7, regVal);

    /* clear enpacal */
    regVal = OS_REG_READ(ah, AR9285_AN_RF2G1);
    regVal &= 0xfffff7ff;
    OS_REG_WRITE(ah, AR9285_AN_RF2G1, regVal);

    /* set offcal */
    regVal = OS_REG_READ(ah, AR9285_AN_RF2G2);
    regVal |= (0x1 << 12);
    OS_REG_WRITE(ah, AR9285_AN_RF2G2, regVal);

    /* set pdpadrv1=pdpadrv2=pdpaout=1 */
    regVal = OS_REG_READ(ah, AR9285_AN_RF2G1);
    regVal |= (0x7 << 23);
    OS_REG_WRITE(ah, AR9285_AN_RF2G1, regVal);

    /* Read back reflo, increase it by 1 and write it. */
    regVal = OS_REG_READ(ah, AR9285_AN_RF2G3);
    reflo = ((regVal >> 26) & 0x7);
    
    if (reflo < 0x7)
        reflo++;
    regVal = ((regVal & 0xe3ffffff) | (reflo << 26));
    OS_REG_WRITE(ah, AR9285_AN_RF2G3, regVal);

    regVal = OS_REG_READ(ah, AR9285_AN_RF2G3);
    reflo = ((regVal >> 26) & 0x7);

    /* use TX single carrier to transmit
     * dac const
     * reg. 15
     */
    phyTstDacRegVal = OS_REG_READ(ah, AR_PHY_TSTDAC_CONST);
    OS_REG_WRITE(ah, AR_PHY_TSTDAC_CONST, ((0x7ff << 11) | 0x7ff)); 
    regVal = OS_REG_READ(ah, AR_PHY_TSTDAC_CONST);

    /* source is dac const
     * reg. 2
     */
    phyTest2RegVal = OS_REG_READ(ah, AR_PHY_TEST2);
    OS_REG_WRITE(ah, AR_PHY_TEST2,((0x1 << 7) | (0x1 << 1)));
    regVal = OS_REG_READ(ah, AR_PHY_TEST2);

    /* set dac on
     * reg. 11
     */
    phyAdcCtlRegVal = OS_REG_READ(ah, AR_PHY_ADC_CTL);
    OS_REG_WRITE(ah, AR_PHY_ADC_CTL, 0x80008000);
    regVal = OS_REG_READ(ah, AR_PHY_ADC_CTL);

    OS_REG_WRITE(ah, AR9285_AN_TOP2, (0x1 << 27) | (0x1 << 17) | (0x1 << 16) |
              (0x1 << 14) | (0x1 << 12) | (0x1 << 11) |
              (0x1 << 7) | (0x1 << 5));

    OS_DELAY(10); /* 10 usec */

    /* clear off[6:0] */
    regVal = OS_REG_READ(ah, AR9285_AN_RF2G6);
    regVal &= 0xfc0fffff;
    OS_REG_WRITE(ah, AR9285_AN_RF2G6, regVal);
    regVal = OS_REG_READ(ah, AR9285_AN_RF2G3);
    regVal &= 0xfdffffff;
    OS_REG_WRITE(ah, AR9285_AN_RF2G3, regVal);

    offs_6_1 = 0;
    for (i = 6; i > 0; i--) 
    {
        /* sef off[$k]==1 */
        regVal = OS_REG_READ(ah, AR9285_AN_RF2G6);
        regVal &= 0xfc0fffff;
        regVal = regVal |(0x1 << (19 + i))|((offs_6_1) << 20);
        OS_REG_WRITE(ah, AR9285_AN_RF2G6, regVal);
        loGn= (OS_REG_READ(ah, AR9285_AN_RF2G9)) & 0x1;
        offs_6_1=offs_6_1 | (loGn << (i - 1));
    }

    regVal = OS_REG_READ(ah, AR9285_AN_RF2G6);
    regVal &= 0xfc0fffff;
    regVal = regVal |((offs_6_1 - 1) << 20);
    OS_REG_WRITE(ah, AR9285_AN_RF2G6, regVal);

    /* set off_0=1; */
    regVal = OS_REG_READ(ah, AR9285_AN_RF2G3);
    regVal &= 0xfdffffff;
    regVal = regVal | (0x1 << 25);
    OS_REG_WRITE(ah, AR9285_AN_RF2G3, regVal);

    loGn = OS_REG_READ(ah, AR9285_AN_RF2G9) & 0x1;
    offs_0 = loGn;

    regVal = OS_REG_READ(ah, AR9285_AN_RF2G3);
    regVal &= 0xfdffffff;
    regVal = regVal |(offs_0 << 25);
    OS_REG_WRITE(ah, AR9285_AN_RF2G3, regVal);

    /* clear pdv2i */
    regVal = OS_REG_READ(ah, AR9285_AN_RXTXBB1);
    regVal &= 0xffffff5f;
    OS_REG_WRITE(ah, AR9285_AN_RXTXBB1, regVal);

    /* set enpacal */
    regVal = OS_REG_READ(ah, AR9285_AN_RF2G1);
    regVal |= (0x1 << 11);
    OS_REG_WRITE(ah, AR9285_AN_RF2G1, regVal);

    /* clear offcal */
    regVal = OS_REG_READ(ah, AR9285_AN_RF2G2);
    regVal &= 0xffffefff;
    OS_REG_WRITE(ah, AR9285_AN_RF2G2, regVal);

    /* set pdpadrv1=pdpadrv2=pdpaout=0 */
    regVal = OS_REG_READ(ah, AR9285_AN_RF2G1);
    regVal &= 0xfc7fffff;
    OS_REG_WRITE(ah, AR9285_AN_RF2G1, regVal);

    /* Read back reflo, decrease it by 1 and write it. */
    regVal = OS_REG_READ(ah, AR9285_AN_RF2G3);
    reflo = (regVal >> 26) & 0x7;
    if (reflo) 
        reflo--;
    regVal = ((regVal & 0xe3ffffff) | (reflo << 26));
    OS_REG_WRITE(ah, AR9285_AN_RF2G3, regVal);
    regVal = OS_REG_READ(ah, AR9285_AN_RF2G3);
    reflo = (regVal >> 26) & 0x7;

    /* write back registers */
    OS_REG_WRITE(ah, AR_PHY_TSTDAC_CONST, phyTstDacRegVal);
    OS_REG_WRITE(ah, AR_PHY_TEST2, phyTest2RegVal);
    OS_REG_WRITE(ah, AR_PHY_ADC_CTL, phyAdcCtlRegVal);
    OS_REG_WRITE(ah, AR9285_AN_TOP2, anTop2RegVal);

    /* Kite 1.1 WAR for Bug 35666 
     * Decrease the LDO value back to 1.20V */
    if (AR_SREV_KITE_11(ah))
        OS_REG_WRITE(ah, AR9285_AN_TOP4, AR9285_AN_TOP4_DEFAULT);

}
#endif

/* ar9300RunInitCals
 * Runs non-periodic calibrations
 */
inline static HAL_BOOL
ar9300RunInitCals(struct ath_hal *ah, int init_cal_count)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    HAL_CHANNEL_INTERNAL ichan; // bogus
    HAL_BOOL isCalDone;
    HAL_CAL_LIST *currCal = ahp->ah_cal_list_curr;
    const HAL_PERCAL_DATA *calData;
    int i;

    if (currCal == AH_NULL)
        return AH_FALSE;

    calData = currCal->calData;

    ichan.CalValid=0;

    for (i=0; i < init_cal_count; i++) {
        /* Reset this Cal */
        ar9300ResetCalibration(ah, currCal);
        /* Poll for offset calibration complete */
        if (!ath_hal_wait(ah, AR_PHY_TIMING4,
            AR_PHY_TIMING4_DO_CAL, 0, AH_WAIT_TIMEOUT)) {
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                     "%s: Cal %d failed to complete in 100ms.\n",
                     __func__, calData->calType);
            /* Re-initialize list pointers for periodic cals */
            ahp->ah_cal_list = ahp->ah_cal_list_last = ahp->ah_cal_list_curr
               = AH_NULL;
            return AH_FALSE;
        }
        /* Run this cal */
        ar9300PerCalibration(ah, &ichan, ahp->ah_rxchainmask,
                             currCal, &isCalDone);
        if (isCalDone == AH_FALSE) {
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                     "%s: Not able to run Init Cal %d.\n", __func__,
                     calData->calType);
        }
        if (currCal->calNext) {
            currCal = currCal->calNext;
            calData = currCal->calData;
        }
    }

    /* Re-initialize list pointers for periodic cals */
    ahp->ah_cal_list = ahp->ah_cal_list_last = ahp->ah_cal_list_curr = AH_NULL;
    return AH_TRUE;
}

/* TxCarrierLeakWAR
 * 
 */
void TxCarrierLeakWAR(struct ath_hal *ah) 
{
	unsigned long tx_gain_table_max;
	unsigned long reg_BB_cl_map_0_b0 = 0xffffffff;
	unsigned long reg_BB_cl_map_1_b0 = 0xffffffff;
	unsigned long reg_BB_cl_map_2_b0 = 0xffffffff;
	unsigned long reg_BB_cl_map_3_b0 = 0xffffffff;
	unsigned long tx_gain, cal_run=0;
	unsigned long cal_gain[AR_PHY_TPC_7_TX_GAIN_TABLE_MAX + 1];
    unsigned long cal_gain_index[AR_PHY_TPC_7_TX_GAIN_TABLE_MAX + 1];
    unsigned long new_gain[AR_PHY_TPC_7_TX_GAIN_TABLE_MAX + 1];
	int i, j;

	memset(new_gain, 0, sizeof(new_gain));
	//printf("     Runing TxCarrierLeakWAR\n");

	//process tx gain table, we use cl_map_hw_gen=0.
	OS_REG_RMW_FIELD(ah, AR_PHY_CL_CAL_CTL, AR_PHY_CL_MAP_HW_GEN, 0);

	//the table we used is txbb_gc[2:0], 1dB[2:1].

	tx_gain_table_max = OS_REG_READ_FIELD(ah, AR_PHY_TPC_7, AR_PHY_TPC_7_TX_GAIN_TABLE_MAX);

	for (i=0; i<=tx_gain_table_max; i++) {
		tx_gain  = OS_REG_READ(ah, AR_PHY_TXGAIN_TAB(1) + i *4);
		cal_gain[i] = (((tx_gain>>5)& 0x7) <<2) |
				  (((tx_gain>>1)& 0x3) <<0) ;
		if (i==0) {
			cal_gain_index[i] = cal_run;     
			new_gain[i] = 1;
			cal_run++;
		} else {
			new_gain[i]= 1;
			for (j=0; j<i; j++) {
				//printf("i=%d, j=%d cal_gain[$i]=0x%04x\n", i, j, cal_gain[i]);
				if (new_gain[i]) {
					if ((cal_gain[i]!= cal_gain[j])) {
					new_gain[i] = 1;
					} 
					else {
						//if old gain found, use old cal_run value.
						new_gain[i] = 0;  
						cal_gain_index[i] = cal_gain_index[j];  
					}
				}             
			}
			//if new gain found, increase cal_run
			if (new_gain[i]==1) {
			cal_gain_index[i] = cal_run;     
			cal_run++;
		 }           
		}

		reg_BB_cl_map_0_b0 = (reg_BB_cl_map_0_b0 & ~(0x1<<i)) | ((cal_gain_index[i]>>0 & 0x1)<<i);
		reg_BB_cl_map_1_b0 = (reg_BB_cl_map_1_b0 & ~(0x1<<i)) | ((cal_gain_index[i]>>1 & 0x1)<<i);
		reg_BB_cl_map_2_b0 = (reg_BB_cl_map_2_b0 & ~(0x1<<i)) | ((cal_gain_index[i]>>2 & 0x1)<<i);
		reg_BB_cl_map_3_b0 = (reg_BB_cl_map_3_b0 & ~(0x1<<i)) | ((cal_gain_index[i]>>3 & 0x1)<<i);          

		//printf("i=%2d, cal_gain[$i]= 0x%04x, cal_run= %d, cal_gain_index[i]=%d, new_gain[i] = %d\n", i, cal_gain[i], cal_run, cal_gain_index[i],new_gain[i]);
	}
	OS_REG_WRITE(ah, AR_PHY_CL_MAP_0_B0, reg_BB_cl_map_0_b0);
	OS_REG_WRITE(ah, AR_PHY_CL_MAP_1_B0, reg_BB_cl_map_1_b0);
	OS_REG_WRITE(ah, AR_PHY_CL_MAP_2_B0, reg_BB_cl_map_2_b0);
	OS_REG_WRITE(ah, AR_PHY_CL_MAP_3_B0, reg_BB_cl_map_3_b0);
	if(AR_SREV_WASP(ah)) {
		OS_REG_WRITE(ah, AR_PHY_CL_MAP_0_B1, reg_BB_cl_map_0_b0);
		OS_REG_WRITE(ah, AR_PHY_CL_MAP_1_B1, reg_BB_cl_map_1_b0);
		OS_REG_WRITE(ah, AR_PHY_CL_MAP_2_B1, reg_BB_cl_map_2_b0);
		OS_REG_WRITE(ah, AR_PHY_CL_MAP_3_B1, reg_BB_cl_map_3_b0);
	}
}


/* ar9300InitCal
 * Initialize Calibration infrastructure
 */
static inline HAL_BOOL
ar9300InitCal(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    HAL_CHANNEL_INTERNAL *ichan = ath_hal_checkchannel(ah, chan);
    HAL_BOOL txiqcal_success_flag=AH_FALSE;

    HALASSERT(ichan);

    if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah)) {
        /* Hornet: 1 x 1 */
        ar9300InitChainMasks(ah, 0x1, 0x1);
    } else if (AR_SREV_WASP(ah)) {
        /* Wasp: 2 x 2 */
        ar9300InitChainMasks(ah, 0x3, 0x3);
    } else {
        /*
         * Osprey needs to be configured for the correct chain mode
         * before running AGC/TxIQ cals.
         */
        if(ahp->ah_enterprise_mode & AR_ENT_OTP_CHAIN2_DISABLE) {
            /* chain 2 disabled - 2 chain mode */
            ar9300InitChainMasks(ah, 0x3, 0x3);
        } else {
        ar9300InitChainMasks(ah, 0x7, 0x7);
        }
	}


#ifndef AR9300_EMULATION_BB
    /* EV 74233: Tx IQ fails to complete for half/quarter rates */
    if (!(IS_CHAN_HALF_RATE(ichan) || IS_CHAN_QUARTER_RATE(ichan))) {
		if (AR_SREV_POSEIDON(ah)) { 
			// for poseidon/jupiter/follow-up chips
			// Tx IQ cal HW run will be a part of AGC calibration
			txiqcal_success_flag = AH_TRUE;//always set to 1 to run ar9300_tx_iq_cal_post_proc if following AGC cal passes
		    OS_REG_RMW_FIELD(ah, AR_PHY_TX_IQCAL_CONTROL_1_POSEIDON,
					 AR_PHY_TX_IQCAQL_CONTROL_1_IQCORR_I_Q_COFF_DELPT, DELPT); // this should be eventually movde to INI file
		} else{			
			// enable Tx IQ Calibration HW for osprey/hornet/wasp 
		    OS_REG_RMW_FIELD(ah, AR_PHY_TX_IQCAL_CONTROL_1,
					 AR_PHY_TX_IQCAQL_CONTROL_1_IQCORR_I_Q_COFF_DELPT, DELPT); // this should be eventually moved to INI file
	        txiqcal_success_flag = ar9300_tx_iq_cal_hw_run(ah);
            OS_REG_WRITE(ah, AR_PHY_ACTIVE, AR_PHY_ACTIVE_DIS);
            OS_DELAY(5);
            OS_REG_WRITE(ah, AR_PHY_ACTIVE, AR_PHY_ACTIVE_EN);
		}
    }
#endif

    /* TxCarrierLeakWAR */
#if 0
    if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah) || AR_SREV_WASP(ah)) {
        TxCarrierLeakWAR(ah);
    }
#endif
    /* Calibrate the AGC */
	// Tx IQ cal is a part of AGC cal for Jupiter/Poseidon, etc.
	// please enable the bit of txiqcal_control_0[31] in INI file for Jupiter/Poseidon/etc.
    OS_REG_WRITE(ah, AR_PHY_AGC_CONTROL,
                 OS_REG_READ(ah, AR_PHY_AGC_CONTROL) |
                 AR_PHY_AGC_CONTROL_CAL);

    /* Poll for offset calibration complete */
    if (!ath_hal_wait(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_CAL, 0,
        AH_WAIT_TIMEOUT)) {
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "%s: offset calibration failed to complete in 1ms; "
                 "noisy environment?\n", __func__);
        return AH_FALSE;
    }
 
#if 0
    /* Do PA Calibration */
    if (AR_SREV_KITE(ah) && AR_SREV_KITE_11_OR_LATER(ah)) {
        ar9285PACal(ah);
    }
#endif

#ifndef AR9300_EMULATION_BB
	// Tx IQ cal post-processing in SW
	// this part of code should be common to all chips, 
	//no chip specific code for Jupiter/Posdeion except for register names
    if (txiqcal_success_flag) {
       ar9300_tx_iq_cal_post_proc(ah);
    }
#endif

	/* Revert chainmasks to their original values before NF cal */
	ar9300InitChainMasks(ah, ahp->ah_rxchainmask, ahp->ah_txchainmask);

#if !FIX_NOISE_FLOOR
    /*
     * Do NF calibration after DC offset and other CALs.
     * Per system engineers, noise floor value can sometimes be 20 dB
     * higher than normal value if DC offset and noise floor cal are
     * triggered at the same time.
     */
    OS_REG_WRITE(ah, AR_PHY_AGC_CONTROL,
                 OS_REG_READ(ah, AR_PHY_AGC_CONTROL) |
                 AR_PHY_AGC_CONTROL_NF);
#endif 

    /* Initialize list pointers */
    ahp->ah_cal_list = ahp->ah_cal_list_last = ahp->ah_cal_list_curr = AH_NULL;

    /*
     * Enable IQ, ADC Gain, ADC DC Offset Cals
     */
    /* Setup all non-periodic, init time only calibrations */
    // XXX: Init DC Offset not working yet
#ifdef not_yet
    if (AH_TRUE == ar9300IsCalSupp(ah, chan, ADC_DC_INIT_CAL)) {
        INIT_CAL(&ahp->ah_adcDcCalInitData);
        INSERT_CAL(ahp, &ahp->ah_adcDcCalInitData);
    }

    /* Initialize current pointer to first element in list */
    ahp->ah_cal_list_curr = ahp->ah_cal_list;

    if (ahp->ah_cal_list_curr) {
        if (ar9300RunInitCals(ah, 0) == AH_FALSE)
            {return AH_FALSE;}
    }
#endif
    /* end - Init time calibrations */

    /* If Cals are supported, add them to list via INIT/INSERT_CAL */
    if (AH_TRUE == ar9300IsCalSupp(ah, chan, IQ_MISMATCH_CAL)) {
        INIT_CAL(&ahp->ah_iqCalData);
        INSERT_CAL(ahp, &ahp->ah_iqCalData);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "%s: enabling IQ Calibration.\n", __func__);
    }
    if (AH_TRUE == ar9300IsCalSupp(ah, chan, TEMP_COMP_CAL)) {
        INIT_CAL(&ahp->ah_tempCompCalData);
        INSERT_CAL(ahp, &ahp->ah_tempCompCalData);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "%s: enabling Temperature Compensation Calibration.\n", __func__);
    }

    /* Initialize current pointer to first element in list */
    ahp->ah_cal_list_curr = ahp->ah_cal_list;

    /* Reset state within current cal */
    if (ahp->ah_cal_list_curr)
        ar9300ResetCalibration(ah, ahp->ah_cal_list_curr);

    /* Mark all calibrations on this channel as being invalid */
    ichan->CalValid = 0;


    return AH_TRUE;
}

/* ar9300ResetCalValid
 * Entry point for upper layers to restart current cal.
 * Reset the calibration valid bit in channel.
 */
void
ar9300ResetCalValid(struct ath_hal *ah, HAL_CHANNEL *chan, HAL_BOOL *isCalDone, u_int32_t calType)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    HAL_CHANNEL_INTERNAL *ichan = ath_hal_checkchannel(ah, chan);
    HAL_CAL_LIST *currCal = ahp->ah_cal_list_curr;

    *isCalDone = AH_TRUE;

    if (currCal == AH_NULL) {
        return;
    }

    if (ichan == AH_NULL) {
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "%s: invalid channel %u/0x%x; no mapping\n",
                 __func__, chan->channel, chan->channelFlags);
        return;
    }

    if (!(calType & IQ_MISMATCH_CAL)) {
		*isCalDone = AH_FALSE;
		return;
	}

    /* Expected that this calibration has run before, post-reset.
     * Current state should be done
     */
    if (currCal->calState != CAL_DONE) {
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "%s: Calibration state incorrect, %d\n",
                 __func__, currCal->calState);
        return;
    }

    /* Verify Cal is supported on this channel */
    if (ar9300IsCalSupp(ah, chan, currCal->calData->calType) == AH_FALSE) {
        return;
    }

    HDPRINTF(ah, HAL_DBG_CALIBRATE,
             "%s: Resetting Cal %d state for channel %u/0x%x\n",
             __func__, currCal->calData->calType, chan->channel,
             chan->channelFlags);

    /* Disable cal validity in channel */
    ichan->CalValid &= ~currCal->calData->calType;
    currCal->calState = CAL_WAITING;
    /* Indicate to upper layers that we need polling */
    *isCalDone = AH_FALSE;
}

static inline void
ar9300SetDma(struct ath_hal *ah)
{
    u_int32_t   regval;
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    HAL_CAPABILITIES *pCap = &ahpriv->ah_caps;

#if 0
    /*
     * set AHB_MODE not to do cacheline prefetches
     */
    regval = OS_REG_READ(ah, AR_AHB_MODE);
    OS_REG_WRITE(ah, AR_AHB_MODE, regval | AR_AHB_PREFETCH_RD_EN);
#endif

    /*
     * let mac dma reads be in 128 byte chunks
     */
    regval = OS_REG_READ(ah, AR_TXCFG) & ~AR_TXCFG_DMASZ_MASK;
    OS_REG_WRITE(ah, AR_TXCFG, regval | AR_TXCFG_DMASZ_128B);

    /*
     * Restore TX Trigger Level to its pre-reset value.
     * The initial value depends on whether aggregation is enabled, and is
     * adjusted whenever underruns are detected.
     */
    //OS_REG_RMW_FIELD(ah, AR_TXCFG, AR_FTRIG, AH_PRIVATE(ah)->ah_txTrigLevel);
    /* 
     * Osprey 1.0 bug (EV 61936). Don't change trigger level from .ini default.
     * Osprey 2.0 - hardware recommends using the default INI settings.
     */
#if 0
    OS_REG_RMW_FIELD(ah, AR_TXCFG, AR_FTRIG, 0x3f);
#endif
    /*
     * let mac dma writes be in 128 byte chunks
     */
    regval = OS_REG_READ(ah, AR_RXCFG) & ~AR_RXCFG_DMASZ_MASK;
    OS_REG_WRITE(ah, AR_RXCFG, regval | AR_RXCFG_DMASZ_128B);

    /*
     * Setup receive FIFO threshold to hold off TX activities
     */
    OS_REG_WRITE(ah, AR_RXFIFO_CFG, 0x200);

    /*
     * reduce the number of usable entries in PCU TXBUF to avoid
     * wrap around bugs. (bug 20428)
     */
    
    if (AR_SREV_WASP(ah) && 
        (AH_PRIVATE((ah))->ah_macRev > AR_SREV_REVISION_WASP_12)) {
        /* Wasp 1.3 fix for EV#85395 requires usable entries 
         * to be set to 0x500 
         */
        OS_REG_WRITE(ah, AR_PCU_TXBUF_CTRL, 0x500);
    } else {
        OS_REG_WRITE(ah, AR_PCU_TXBUF_CTRL, AR_PCU_TXBUF_CTRL_USABLE_SIZE);
    }

    /*
     * Enable HPQ for UAPSD
     */
    if (pCap->halHwUapsdTrig == AH_TRUE) {
    /* Only enable this if HAL capabilities says it is OK */
        if (AH_PRIVATE(ah)->ah_opmode == HAL_M_HOSTAP) {
            OS_REG_WRITE(ah, AR_HP_Q_CONTROL,
                    AR_HPQ_ENABLE | AR_HPQ_UAPSD | AR_HPQ_UAPSD_TRIGGER_EN);
        }
    } else {
        /* use default value from ini file - which disable HPQ queue usage */
    }

    /*
     * set the transmit status ring
     */
    ar9300ResetTxStatusRing(ah);

    /*
     * set rxbp threshold.  Must be non-zero for RX_EOL to occur.
     * For Osprey 2.0+, keep the original thresholds
     * otherwise performance is lost due to excessive RX EOL interrupts.
     */
    OS_REG_RMW_FIELD(ah, AR_RXBP_THRESH, AR_RXBP_THRESH_HP, 0x1);
    OS_REG_RMW_FIELD(ah, AR_RXBP_THRESH, AR_RXBP_THRESH_LP, 0x1);

    /*
     * set receive buffer size.
     */
    if (ahp->rxBufSize)
        OS_REG_WRITE(ah, AR_DATABUF, ahp->rxBufSize);
}

static inline void
ar9300InitBB(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    u_int32_t synthDelay;

    /*
     * Wait for the frequency synth to settle (synth goes on
     * via AR_PHY_ACTIVE_EN).  Read the phy active delay register.
     * Value is in 100ns increments.
     */
    synthDelay = OS_REG_READ(ah, AR_PHY_RX_DELAY) & AR_PHY_RX_DELAY_DELAY;
    if (IS_CHAN_CCK(chan)) {
        synthDelay = (4 * synthDelay) / 22;
    } else {
        synthDelay /= 10;
    }

    /* Activate the PHY (includes baseband activate + synthesizer on) */
    OS_REG_WRITE(ah, AR_PHY_ACTIVE, AR_PHY_ACTIVE_EN);

    /*
     * There is an issue if the AP starts the calibration before
     * the base band timeout completes.  This could result in the
     * rx_clear false triggering.  As a workaround we add delay an
     * extra BASE_ACTIVATE_DELAY usecs to ensure this condition
     * does not happen.
     */
#ifdef AR9300_EMULATION_BB
    OS_DELAY(10000);
#endif
    OS_DELAY(synthDelay + BASE_ACTIVATE_DELAY);
}

static inline void
ar9300InitInterruptMasks(struct ath_hal *ah, HAL_OPMODE opmode)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    u_int32_t msiCfg = 0;
    u_int32_t sync_en_def = AR_INTR_SYNC_DEFAULT;

    /*
     * Setup interrupt handling.  Note that ar9300ResetTxQueue
     * manipulates the secondary IMR's as queues are enabled
     * and disabled.  This is done with RMW ops to insure the
     * settings we make here are preserved.
     */
    ahp->ah_maskReg = AR_IMR_TXERR | AR_IMR_TXURN | AR_IMR_RXERR | AR_IMR_RXORN
                    | AR_IMR_BCNMISC;

    if (ahp->ah_intrMitigationRx) {
        /* enable interrupt mitigation for rx */
        ahp->ah_maskReg |= AR_IMR_RXINTM | AR_IMR_RXMINTR | AR_IMR_RXOK_HP;
        msiCfg |= AR_INTCFG_MSI_RXINTM | AR_INTCFG_MSI_RXMINTR;
    } else {
        ahp->ah_maskReg |= AR_IMR_RXOK_LP | AR_IMR_RXOK_HP;
        msiCfg |= AR_INTCFG_MSI_RXOK;
    }
    if (ahp->ah_intrMitigationTx) {
        /* enable interrupt mitigation for tx */
        ahp->ah_maskReg |= AR_IMR_TXINTM | AR_IMR_TXMINTR;
        msiCfg |= AR_INTCFG_MSI_TXINTM | AR_INTCFG_MSI_TXMINTR;
    } else {
        ahp->ah_maskReg |= AR_IMR_TXOK;
        msiCfg |= AR_INTCFG_MSI_TXOK;
    }

    if (opmode == HAL_M_HOSTAP) ahp->ah_maskReg |= AR_IMR_MIB;

    OS_REG_WRITE(ah, AR_IMR, ahp->ah_maskReg);

    OS_REG_WRITE(ah, AR_IMR_S2, OS_REG_READ(ah, AR_IMR_S2) | AR_IMR_S2_GTT);

    if (AH_PRIVATE(ah)->ah_config.ath_hal_enableMSI) {
        /* Cache MSI register value */
        ahp->ah_msiReg = OS_REG_READ(ah, AR_HOSTIF_REG(ah, AR_PCIE_MSI));
        ahp->ah_msiReg |= AR_PCIE_MSI_HW_DBI_WR_EN;
        if (AR_SREV_POSEIDON(ah)) {
            ahp->ah_msiReg &= AR_PCIE_MSI_HW_INT_PENDING_ADDR_MSI_64;
        } else {
            ahp->ah_msiReg &= AR_PCIE_MSI_HW_INT_PENDING_ADDR;
        }
        /* Program MSI configuration */
        OS_REG_WRITE(ah, AR_INTCFG, msiCfg);
    }

    /*
     * debug - enable to see all synchronous interrupts status
     */
    /* Clear any pending sync cause interrupts */
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_INTR_SYNC_CAUSE), 0xFFFFFFFF);

    /* Allow host interface sync interrupt sources to set cause bit */
    if (AR_SREV_POSEIDON(ah)) {
        sync_en_def = AR_INTR_SYNC_DEF_NO_HOST1_PERR;
    }
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_INTR_SYNC_ENABLE), sync_en_def);

    /* _Disable_ host interface sync interrupt when cause bits set */
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_INTR_SYNC_MASK), 0);

    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_INTR_PRIO_ASYNC_ENABLE), 0);
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_INTR_PRIO_ASYNC_MASK), 0);
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_INTR_PRIO_SYNC_ENABLE), 0);
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_INTR_PRIO_SYNC_MASK), 0);
}

static inline void
ar9300InitQOS(struct ath_hal *ah)
{
    OS_REG_WRITE(ah, AR_MIC_QOS_CONTROL, 0x100aa);  /* XXX magic */
    OS_REG_WRITE(ah, AR_MIC_QOS_SELECT, 0x3210);    /* XXX magic */

    /* Turn on NOACK Support for QoS packets */
    OS_REG_WRITE(ah, AR_QOS_NO_ACK,
            SM(2, AR_QOS_NO_ACK_TWO_BIT) |
            SM(5, AR_QOS_NO_ACK_BIT_OFF) |
            SM(0, AR_QOS_NO_ACK_BYTE_OFF));

    /*
     * initialize TXOP for all TIDs
     */
    OS_REG_WRITE(ah, AR_TXOP_X, AR_TXOP_X_VAL);
    OS_REG_WRITE(ah, AR_TXOP_0_3, 0xFFFFFFFF);
    OS_REG_WRITE(ah, AR_TXOP_4_7, 0xFFFFFFFF);
    OS_REG_WRITE(ah, AR_TXOP_8_11, 0xFFFFFFFF);
    OS_REG_WRITE(ah, AR_TXOP_12_15, 0xFFFFFFFF);
}

static inline void
ar9300InitUserSettings(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);

    /* Restore user-specified settings */
    HDPRINTF(ah, HAL_DBG_RESET, "--AP %s ahp->ah_miscMode 0x%x\n", __func__, ahp->ah_miscMode);
    if (ahp->ah_miscMode != 0)
        OS_REG_WRITE(ah, AR_PCU_MISC, OS_REG_READ(ah, AR_PCU_MISC) | ahp->ah_miscMode);
    if (ahp->ah_getPlcpHdr)
        OS_REG_CLR_BIT(ah, AR_PCU_MISC, AR_PCU_SEL_EVM);
    if (ahp->ah_slottime != (u_int) -1)
        ar9300SetSlotTime(ah, ahp->ah_slottime);
    if (ahp->ah_acktimeout != (u_int) -1)
        ar9300SetAckTimeout(ah, ahp->ah_acktimeout);
    if (ahp->ah_ctstimeout != (u_int) -1)
        ar9300SetCTSTimeout(ah, ahp->ah_ctstimeout);
    if (ahp->ah_globaltxtimeout != (u_int) -1)
        ar9300SetGlobalTxTimeout(ah, ahp->ah_globaltxtimeout);
    if (AH_PRIVATE(ah)->ah_diagreg != 0)
        OS_REG_SET_BIT(ah, AR_DIAG_SW, AH_PRIVATE(ah)->ah_diagreg);

    if (ahp->ah_cac_quiet_enabled) {
        ar9300_cac_tx_quiet(ah, 1);
    }
}


int
ar9300_getSpurInfo(struct ath_hal * ah, int *enable, int len, u_int16_t *freq)
{
    struct ath_hal_private *ap = AH_PRIVATE(ah);
    int i, j;

    for (i = 0; i < len; i++) {
        freq[i] =  0;
    }

    *enable =  ap->ah_config.ath_hal_spurMode;
    for (i = 0, j = 0; i < AR_EEPROM_MODAL_SPURS; i++) {
        if (ap->ah_config.ath_hal_spurChans[i][0] != AR_NO_SPUR) {
            freq[j++] = ap->ah_config.ath_hal_spurChans[i][0];
            HDPRINTF(ah, HAL_DBG_ANI,
                     "1. get spur %d\n", ap->ah_config.ath_hal_spurChans[i][0]);
        }
        if (ap->ah_config.ath_hal_spurChans[i][1] != AR_NO_SPUR) {
            freq[j++] = ap->ah_config.ath_hal_spurChans[i][1];
            HDPRINTF(ah, HAL_DBG_ANI,
                     "2. get spur %d\n", ap->ah_config.ath_hal_spurChans[i][1]);
        }
    }

    return 0;
}

#define ATH_HAL_2GHZ_FREQ_MIN   20000
#define ATH_HAL_2GHZ_FREQ_MAX   29999
#define ATH_HAL_5GHZ_FREQ_MIN   50000
#define ATH_HAL_5GHZ_FREQ_MAX   59999

int
ar9300_setSpurInfo(struct ath_hal * ah, int enable, int len, u_int16_t *freq)
{
    struct ath_hal_private *ap = AH_PRIVATE(ah);
    int i, j, k;

    ap->ah_config.ath_hal_spurMode = enable;

    if (ap->ah_config.ath_hal_spurMode == SPUR_ENABLE_IOCTL) {
        for (i = 0; i < AR_EEPROM_MODAL_SPURS; i++) {
            ap->ah_config.ath_hal_spurChans[i][0] = AR_NO_SPUR;
            ap->ah_config.ath_hal_spurChans[i][1] = AR_NO_SPUR;
        }

        for (i = 0, j = 0, k = 0; i < len; i++) {
            /* 2GHz Spur */
            if (freq[i] > ATH_HAL_2GHZ_FREQ_MIN &&
                freq[i] < ATH_HAL_2GHZ_FREQ_MAX)
            {
                if (j < AR_EEPROM_MODAL_SPURS) {
                    ap->ah_config.ath_hal_spurChans[j++][1] =  freq[i];
                    HDPRINTF(ah, HAL_DBG_ANI, "1 set spur %d\n", freq[i]);
                }
            }
            /* 5Ghz Spur */
            else if (freq[i] > ATH_HAL_5GHZ_FREQ_MIN &&
                     freq[i] < ATH_HAL_5GHZ_FREQ_MAX)
            {
                if (k < AR_EEPROM_MODAL_SPURS) {
                    ap->ah_config.ath_hal_spurChans[k++][0] =  freq[i];
                    HDPRINTF(ah, HAL_DBG_ANI, "2 set spur %d\n", freq[i]);
                }
            }
        }
    }

    return 0;
}

#define ar9300CheckOpMode(_opmode) \
    ((_opmode == HAL_M_STA) || (_opmode == HAL_M_IBSS) ||\
     (_opmode == HAL_M_HOSTAP) || (_opmode == HAL_M_MONITOR))
 

#ifdef AR9300_EMULATION_BB
HAL_BOOL
ar9300EmulRadioInit(struct ath_hal *ah)
{
#define RXTXBB1_CH1                     0x7800
#define RXTXBB2_CH1                     0x7804
#define RF2G1_CH0                       0x7840
#define RF2G2_CH0                       0x7844
#define SYNTH4                          0x7854
#define RTC_SYNC_STATUS                 0x7044
#define RTC_SYNC_STATUS_PLL_CHANGING    0x20

    if(!AR_SREV_POSEIDON(ah)) {
    OS_REG_WRITE(ah, RXTXBB1_CH1, 0x00000000);
    OS_REG_WRITE(ah, RXTXBB2_CH1, 0xdb002812);
    }

    if(AR_SREV_POSEIDON(ah)) {
        OS_REG_WRITE(ah, RF2G1_CH0, 0x6d801300);
        OS_REG_WRITE(ah, RF2G2_CH0, 0x0019beff);
    }

    OS_REG_WRITE(ah, SYNTH4,      0x12025809);

    OS_DELAY(200);
    if(AR_SREV_POSEIDON(ah)) {
        OS_DELAY(10000);    
    }

    if (!ath_hal_wait(ah, RTC_SYNC_STATUS, RTC_SYNC_STATUS_PLL_CHANGING,
                      0, 500000)) {
        ath_hal_printf(ah, "%s: Failing in RTC SYNC STATUS\n", __func__);
        return AH_FALSE;
    }

    OS_DELAY(10);

    if(!AR_SREV_POSEIDON(ah)) {
    OS_REG_WRITE(ah, RXTXBB1_CH1, 0x00000000);
    }
    return AH_TRUE;
}

#endif

#ifdef AR9300_EMULATION_BB
static void
ar9300ForceTxGain(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *ichan)
{
    OS_REG_RMW_FIELD(ah, AR_PHY_TPC(1), AR_PHY_TPCGR1_FORCED_DAC_GAIN, 0);
    OS_REG_WRITE(ah, AR_PHY_TPC(1),
		 (OS_REG_READ(ah, AR_PHY_TPC(1)) | AR_PHY_TPCGR1_FORCE_DAC_GAIN));
    OS_REG_WRITE(ah, AR_PHY_TX_FORCED_GAIN,
		 (OS_REG_READ(ah, AR_PHY_TX_FORCED_GAIN) | AR_PHY_TXGAIN_FORCE));
    OS_REG_RMW_FIELD(ah, AR_PHY_TX_FORCED_GAIN, AR_PHY_TXGAIN_FORCED_PADVGNRD, 1);
    OS_REG_RMW_FIELD(ah, AR_PHY_TX_FORCED_GAIN, AR_PHY_TXGAIN_FORCED_PADVGNRA, 1);
    OS_REG_RMW_FIELD(ah, AR_PHY_TX_FORCED_GAIN, AR_PHY_TXGAIN_FORCED_TXMXRGAIN, 2);
    OS_REG_RMW_FIELD(ah, AR_PHY_TX_FORCED_GAIN, AR_PHY_TXGAIN_FORCED_TXBB1DBGAIN, 2);
    if (IS_CHAN_5GHZ(ichan)) {
        OS_REG_RMW_FIELD(ah, AR_PHY_TX_FORCED_GAIN, AR_PHY_TXGAIN_FORCED_PADVGNRB, 2);
    } else {
        OS_REG_RMW_FIELD(ah, AR_PHY_TX_FORCED_GAIN, AR_PHY_TXGAIN_FORCED_PADVGNRB, 1);
    }
}
#endif

/*
 * Places the device in and out of reset and then places sane
 * values in the registers based on EEPROM config, initialization
 * vectors (as determined by the mode), and station configuration
 *
 * bChannelChange is used to preserve DMA/PCU registers across
 * a HW Reset during channel change.
 */
HAL_BOOL
ar9300Reset(struct ath_hal *ah, HAL_OPMODE opmode, HAL_CHANNEL *chan,
           HAL_HT_MACMODE macmode, u_int8_t txchainmask, u_int8_t rxchainmask,
           HAL_HT_EXTPROTSPACING extprotspacing, HAL_BOOL bChannelChange,
           HAL_STATUS *status, int is_scan)
{
#define FAIL(_code)     do { ecode = _code; goto bad; } while (0)
    u_int32_t               saveLedState;
    struct ath_hal_9300     *ahp = AH9300(ah);
    struct ath_hal_private  *ap  = AH_PRIVATE(ah);
    HAL_CHANNEL_INTERNAL    *ichan;
    HAL_CHANNEL_INTERNAL    *curchan = ap->ah_curchan;
    u_int32_t               saveDefAntenna;
    u_int32_t               macStaId1;
    HAL_STATUS              ecode;
    int                     i, rx_chainmask, NfHistBuffReset = 0;
#ifdef ATH_FORCE_PPM
    u_int32_t saveForceVal, tmpReg;
#endif
    HAL_BOOL                stopped;
    u_int8_t                clk_25mhz = AH9300(ah)->clk_25mhz;

    if (OS_REG_READ(ah, AR_IER) == AR_IER_ENABLE) {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "** Reset called with WLAN "
                "interrupt enabled %08x **\n", ar9300GetInterrupts(ah));
    }
	/*
     * CHH: Setting the status to zero by default to cover the cases
     * where we return AH_FALSE without going to "bad"
	*/
    HALASSERT(status);
	*status = HAL_OK;

    if ((AH_PRIVATE(ah)->ah_config.ath_hal_sta_update_tx_pwr_enable)) {
        ah->green_tx_status = HAL_RSSI_TX_POWER_NONE;
    }

    ahp->ah_extprotspacing = extprotspacing;
    ahp->ah_txchainmask = txchainmask & ap->ah_caps.halTxChainMask;
    ahp->ah_rxchainmask = rxchainmask & ap->ah_caps.halRxChainMask;

    HALASSERT(ar9300CheckOpMode(opmode));

    OS_MARK(ah, AH_MARK_RESET, bChannelChange);

#if AR9300_EMULATION_BB
    bChannelChange = AH_FALSE;
#endif
    /*
     * Map public channel to private.
     */
    ichan = ar9300CheckChan(ah, chan);
    if (ichan == AH_NULL) {
        HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: invalid channel %u/0x%x; no mapping\n",
                __func__, chan->channel, chan->channelFlags);
        FAIL(HAL_EINVAL);
    }

    ichan->paprdTableWriteDone = 0;  // Clear PAPRD flags
    chan->paprdTableWriteDone = 0;  // Clear PAPRD flags

    if(ar9300GetPowerMode(ah) != HAL_PM_FULL_SLEEP) {
        /* Need to stop RX DMA before reset otherwise chip might hang */
        stopped = ar9300SetRxAbort(ah, AH_TRUE); /* abort and disable PCU */
        ar9300SetRxFilter(ah, 0);
        stopped &= ar9300StopDmaReceive(ah, 0);	/* stop and disable RX DMA */
        if(!stopped) {
            /*
             * During the transition from full sleep to reset, recv DMA regs are not available to be read
             */
            HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s[%d]: ar9300StopDmaReceive failed\n",
                    __func__, __LINE__);
            bChannelChange = AH_FALSE;
        }
    }
    else
    {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s[%d]: Chip is already in full sleep\n",
                __func__, __LINE__);
    }
    
    /* Bring out of sleep mode */
    if (!ar9300SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE)) {
		*status = HAL_INV_PMODE;
        return AH_FALSE;
    }

    /* Check the Rx mitigation config again, it might have changed
     * during attach in ath_vap_attach.
     */
    if (AH_PRIVATE(ah)->ah_config.ath_hal_intrMitigationRx != 0) {
        ahp->ah_intrMitigationRx = AH_TRUE;
    } else {
        ahp->ah_intrMitigationRx = AH_FALSE;
    }

    /* Get the value from the previous NF cal and update history buffer */
    if (curchan && (ahp->ah_chipFullSleep != AH_TRUE)) {
        ar9300StoreNewNf(ah, curchan, is_scan);
    }

    /*
     * Account for the effect of being in either the 2 GHz or 5 GHz band
     * on the nominal, max allowable, and min allowable noise floor values.
     */
    ap->nfp = IS_CHAN_2GHZ(chan) ? &ap->nf_2GHz : &ap->nf_5GHz;

#ifndef ATH_NF_PER_CHAN
    /*
     * If there's only one full-size home-channel NF history buffer
     * rather than a full-size NF history buffer per channel, decide
     * whether to (re)initialize the home-channel NF buffer.
     * If this is just a channel change for a scan, or if the channel
     * is not being changed, don't mess up the home channel NF history
     * buffer with NF values from this scanned channel.  If we're
     * changing the home channel to a new channel, reset the home-channel
     * NF history buffer with the most accurate NF known for the new channel.
     */
    if (!is_scan && (!ap->ah_curchan ||
        ap->ah_curchan->channel != chan->channel ||
        ap->ah_curchan->channelFlags != chan->channelFlags))
    {
        if (AR_SREV_POSEIDON(ah) || AR_SREV_HORNET(ah) || AR_SREV_WASP(ah)) {
            NfHistBuffReset = 1;
        }
        ar9300ResetNfHistBuff(ah, ichan);
    }
#endif

    /*
     * Fast channel change (Change synthesizer based on channel freq without resetting chip)
     * Don't do it when
     *   - Flag is not set
     *   - Chip is just coming out of full sleep
     *   - Channel to be set is same as current channel
     *   - Channel flags are different, like when moving from 2GHz to 5GHz channels
     *   - Merlin: Switching in/out of fast clock enabled channels
                   (not currently coded, since fast clock is enabled across the 5GHz band
     *              and we already do a full reset when switching in/out of 5GHz channels)
     */
    if (bChannelChange &&
        (ahp->ah_chipFullSleep != AH_TRUE) &&
        (AH_PRIVATE(ah)->ah_curchan != AH_NULL) &&
        (chan->channel != AH_PRIVATE(ah)->ah_curchan->channel) &&
        ((chan->channelFlags & (CHANNEL_ALL|CHANNEL_HALF|CHANNEL_QUARTER)) ==
         (AH_PRIVATE(ah)->ah_curchan->channelFlags &
          (CHANNEL_ALL|CHANNEL_HALF|CHANNEL_QUARTER))))
    {
        if (ar9300ChannelChange(ah, chan, ichan, macmode)) {
            chan->channelFlags = ichan->channelFlags;
            chan->privFlags = ichan->privFlags;
            AH_PRIVATE(ah)->ah_curchan->ah_channel_time=0;
            AH_PRIVATE(ah)->ah_curchan->ah_tsf_last = ar9300GetTsf64(ah);

            /*
             * Load the NF from history buffer of the current channel.
             * NF is slow time-variant, so it is OK to use a historical value.
             */
            ar9300LoadNF(ah, AH_PRIVATE(ah)->ah_curchan, is_scan);

            /* start NF calibration, without updating BB NF register*/
            ar9300StartNFCal(ah);

            /*
             * If ChannelChange completed and DMA was stopped
             * successfully - skip the rest of reset
             */
            if (AH9300(ah)->ah_dma_stuck != AH_TRUE) {
                WAR_USB_DISABLE_PLL_LOCK_DETECT(ah);
                return AH_TRUE;
            }
         }
    }

    AH9300(ah)->ah_dma_stuck = AH_FALSE;
#ifdef ATH_FORCE_PPM
    /* Preserve force ppm state */
    saveForceVal =
        OS_REG_READ(ah, AR_PHY_TIMING2) &
        (AR_PHY_TIMING2_USE_FORCE | AR_PHY_TIMING2_FORCE_VAL);
#endif

    /*
     * Preserve the antenna on a channel change
     */
    saveDefAntenna = OS_REG_READ(ah, AR_DEF_ANTENNA);
    if (0 == ahp->ah_smartantenna_enable ) {
   	if (saveDefAntenna == 0) {
	    saveDefAntenna = 1;
	}
    } 

    /* Save hardware flag before chip reset clears the register */
    macStaId1 = OS_REG_READ(ah, AR_STA_ID1) & AR_STA_ID1_BASE_RATE_11B;

    /* Save led state from pci config register */
    saveLedState = OS_REG_READ(ah, AR_CFG_LED) &
            (AR_CFG_LED_ASSOC_CTL | AR_CFG_LED_MODE_SEL |
             AR_CFG_LED_BLINK_THRESH_SEL | AR_CFG_LED_BLINK_SLOW);

    /* Mark PHY inactive prior to reset, to be undone in ar9300InitBB () */
    ar9300MarkPhyInactive(ah);

    if (!ar9300ChipReset(ah, chan)) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s: chip reset failed\n", __func__);
        FAIL(HAL_EIO);
    }

    OS_MARK(ah, AH_MARK_RESET_LINE, __LINE__);

#ifdef AR9300_EMULATION
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_HOST_TIMEOUT), 0xfff0fff0);
#endif

#ifndef AR9300_EMULATION
    /* Disable JTAG */
    OS_REG_SET_BIT(ah, AR_HOSTIF_REG(ah, AR_GPIO_INPUT_EN_VAL), AR_GPIO_JTAG_DISABLE);
#endif

    /*
     * Note that ar9300InitChainMasks() is called from within ar9300ProcessIni()
     * to ensure the swap bit is set before the pdadc table is written.
     */

    ecode = ar9300ProcessIni(ah, chan, ichan, macmode);
    if (ecode != HAL_OK) {
        goto bad;
    }
    ahp->ah_immunity_on = AH_FALSE;
 
    /*
     * Configuring WMAC PLL values for 25/40 MHz 
     */
    if(AR_SREV_WASP(ah)) {
        if(clk_25mhz) {
            OS_REG_WRITE(ah, AR_RTC_DERIVED_RTC_CLK, (0x17c << 1)); // 32KHz sleep clk
        } else {
            OS_REG_WRITE(ah, AR_RTC_DERIVED_RTC_CLK, (0x261 << 1)); // 32KHz sleep clk
        }
        OS_DELAY(100);
    }

#if !defined AR9300_EMULATION && !defined AR9300_EMULATION_BB
    /* For devices with full HW RIFS Rx support (Sowl/Howl/Merlin, etc),
     * restore register settings from prior to reset.
     */
    if ((AH_PRIVATE(ah)->ah_curchan != AH_NULL) &&
        (ar9300GetCapability(ah, HAL_CAP_LDPCWAR, 0, AH_NULL)
         == HAL_OK))
    {
        /* Re-program RIFS Rx policy after reset */
        ar9300SetRifsDelay(ah, ahp->ah_rifs_enabled);
    }
#endif

    /* Initialize Management Frame Protection */
    ar9300InitMFP(ah);

    ahp->ah_immunity_vals[0] = OS_REG_READ_FIELD(ah, AR_PHY_SFCORR_LOW,
        AR_PHY_SFCORR_LOW_M1_THRESH_LOW);
    ahp->ah_immunity_vals[1] = OS_REG_READ_FIELD(ah, AR_PHY_SFCORR_LOW,
        AR_PHY_SFCORR_LOW_M2_THRESH_LOW);
    ahp->ah_immunity_vals[2] = OS_REG_READ_FIELD(ah, AR_PHY_SFCORR,
        AR_PHY_SFCORR_M1_THRESH);
    ahp->ah_immunity_vals[3] = OS_REG_READ_FIELD(ah, AR_PHY_SFCORR,
        AR_PHY_SFCORR_M2_THRESH);
    ahp->ah_immunity_vals[4] = OS_REG_READ_FIELD(ah, AR_PHY_SFCORR,
        AR_PHY_SFCORR_M2COUNT_THR);
    ahp->ah_immunity_vals[5] = OS_REG_READ_FIELD(ah, AR_PHY_SFCORR_LOW,
        AR_PHY_SFCORR_LOW_M2COUNT_THR_LOW);

    /* Write delta slope for OFDM enabled modes (A, G, Turbo) */
    if (IS_CHAN_OFDM(chan) || IS_CHAN_HT(chan)) {
        ar9300SetDeltaSlope(ah, ichan);
    }

    ar9300SpurMitigate(ah, chan);

    if (!ar9300EepromSetBoardValues(ah, ichan)) {
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: error setting board options\n", __func__);
        FAIL(HAL_EIO);
    }

    OS_MARK(ah, AH_MARK_RESET_LINE, __LINE__);

    OS_REG_WRITE(ah, AR_STA_ID0, LE_READ_4(ahp->ah_macaddr));
    OS_REG_WRITE(ah, AR_STA_ID1, LE_READ_2(ahp->ah_macaddr + 4)
            | macStaId1
            | AR_STA_ID1_RTS_USE_DEF
            | (ap->ah_config.ath_hal_6mb_ack ? AR_STA_ID1_ACKCTS_6MB : 0)
            | ahp->ah_staId1Defaults
    );
    ar9300SetOperatingMode(ah, opmode);

    /* Set Venice BSSID mask according to current state */
    OS_REG_WRITE(ah, AR_BSSMSKL, LE_READ_4(ahp->ah_bssidmask));
    OS_REG_WRITE(ah, AR_BSSMSKU, LE_READ_2(ahp->ah_bssidmask + 4));

    /* Restore previous antenna */
    OS_REG_WRITE(ah, AR_DEF_ANTENNA, saveDefAntenna);
#ifdef ATH_FORCE_PPM
    /* Restore force ppm state */
    tmpReg = OS_REG_READ(ah, AR_PHY_TIMING2) &
                ~(AR_PHY_TIMING2_USE_FORCE | AR_PHY_TIMING2_FORCE_VAL);
    OS_REG_WRITE(ah, AR_PHY_TIMING2, tmpReg | saveForceVal);
#endif

    /* then our BSSID and assocID */
    OS_REG_WRITE(ah, AR_BSS_ID0, LE_READ_4(ahp->ah_bssid));
    OS_REG_WRITE(ah, AR_BSS_ID1, LE_READ_2(ahp->ah_bssid + 4) |
                                 ((ahp->ah_assocId & 0x3fff) << AR_BSS_ID1_AID_S));

    OS_REG_WRITE(ah, AR_ISR, ~0);           /* cleared on write */

    OS_REG_WRITE(ah, AR_RSSI_THR, INIT_RSSI_THR);

#ifdef AR9300_EMULATION_BB
    if (AH_FALSE == ar9300EmulRadioInit(ah)) {
        FAIL(HAL_EIO);
    }

    ar9300ForceTxGain(ah, ichan);
#endif

    /*
     * Set Channel now modifies bank 6 parameters for FOWL workaround
     * to force rf_pwd_icsyndiv bias current as function of synth
     * frequency.Thus must be called after ar9300ProcessIni() to ensure
     * analog register cache is valid.
     */
    if (!ahp->ah_rfHal.setChannel(ah, ichan)) {
        FAIL(HAL_EIO);
    }

#ifdef AR9300_EMULATION_BB
    OS_DELAY(100);
#endif

    OS_MARK(ah, AH_MARK_RESET_LINE, __LINE__);

    /* Set 1:1 QCU to DCU mapping for all queues */
    for (i = 0; i < AR_NUM_DCU; i++) {
        OS_REG_WRITE(ah, AR_DQCUMASK(i), 1 << i);
    }

    ahp->ah_intrTxqs = 0;
    for (i = 0; i < AH_PRIVATE(ah)->ah_caps.halTotalQueues; i++) {
        ar9300ResetTxQueue(ah, i);
    }

    ar9300InitInterruptMasks(ah, opmode);

    /* Reset ier reference count to disabled */
    ahp->ah_ier_refcount = 1;

    if (ath_hal_isrfkillenabled(ah))
        ar9300EnableRfKill(ah);

    ar9300AniInitDefaults(ah, macmode); /* must be called AFTER ini is processed */

    ar9300InitQOS(ah);

    ar9300InitUserSettings(ah);

#if ATH_SUPPORT_WAPI
    /*
     * Enable WAPI deaggregation and AR_PCU_MISC_MODE2_BC_MC_WAPI_MODE
     */
        OS_REG_SET_BIT(ah, AR_MAC_PCU_LOGIC_ANALYZER, AR_MAC_PCU_LOGIC_WAPI_DEAGGR_ENABLE);
        if(ah->ah_hal_keytype == HAL_CIPHER_WAPI) {
            OS_REG_SET_BIT(ah, AR_PCU_MISC_MODE2, AR_PCU_MISC_MODE2_BC_MC_WAPI_MODE);
        }
#endif

    AH_PRIVATE(ah)->ah_opmode = opmode; /* record operating mode */

    OS_MARK(ah, AH_MARK_RESET_DONE, 0);

    /*
     * disable seq number generation in hw
     */
    OS_REG_WRITE(ah, AR_STA_ID1,
                 OS_REG_READ(ah, AR_STA_ID1) | AR_STA_ID1_PRESERVE_SEQNUM);

    ar9300SetDma(ah);

    /*
     * program OBS bus to see MAC interrupts
     */
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_OBS), 8);

#ifdef AR9300_EMULATION
#if 0
    /*
     * configure MAC logic analyzer (emulation only)
     */
    if (IS_9300_EMU(ah)) {
        ar9300InitMacTrace(ah);
    }
#endif
#endif

    /* enabling AR_GTTM_IGNORE_IDLE in GTTM register so that
       GTT timer will not increment if the channel idle indicates 
       the air is busy or NAV is still counting down */
    OS_REG_WRITE(ah, AR_GTTM, AR_GTTM_IGNORE_IDLE);

    /*
     * GTT debug mode setting
     */
    // OS_REG_WRITE(ah, 0x64, 0x00320000);
    // OS_REG_WRITE(ah, 0x68, 7);
    // OS_REG_WRITE(ah, 0x4080, 0xC);
    /*
     * Disable general interrupt mitigation by setting MIRT = 0x0
     * Rx and tx interrupt mitigation are conditionally enabled below.
     */
    OS_REG_WRITE(ah, AR_MIRT, 0);
    if (ahp->ah_intrMitigationRx) {
        /*
         * Enable Interrupt Mitigation for Rx.
         * If no build-specific limits for the rx interrupt mitigation
         * timer have been specified, use conservative defaults.
         */
        #ifndef AH_RIMT_VAL_LAST
            #define AH_RIMT_LAST_MICROSEC 500
        #endif
        #ifndef AH_RIMT_VAL_FIRST
            #define AH_RIMT_FIRST_MICROSEC 2000
        #endif
        OS_REG_RMW_FIELD(ah, AR_RIMT, AR_RIMT_LAST, AH_RIMT_LAST_MICROSEC);
        OS_REG_RMW_FIELD(ah, AR_RIMT, AR_RIMT_FIRST, AH_RIMT_FIRST_MICROSEC);
    }

    if (ahp->ah_intrMitigationTx) {
        /*
         * Enable Interrupt Mitigation for Tx.
         * If no build-specific limits for the tx interrupt mitigation
         * timer have been specified, use the values preferred for
         * the carrier group's products.
         */
        #ifndef AH_TIMT_LAST
            #define AH_TIMT_LAST_MICROSEC 300
        #endif
        #ifndef AH_TIMT_FIRST
            #define AH_TIMT_FIRST_MICROSEC 750
        #endif
        OS_REG_RMW_FIELD(ah, AR_TIMT, AR_TIMT_LAST, AH_TIMT_LAST_MICROSEC);
        OS_REG_RMW_FIELD(ah, AR_TIMT, AR_TIMT_FIRST, AH_TIMT_FIRST_MICROSEC);
    }

    rx_chainmask = ahp->ah_rxchainmask;

    OS_REG_WRITE(ah, AR_PHY_RX_CHAINMASK, rx_chainmask);
    OS_REG_WRITE(ah, AR_PHY_CAL_CHAINMASK, rx_chainmask);

    ar9300InitBB(ah, chan);
    
    //BB Step 7: Calibration
    if (!ar9300InitCal(ah, chan)) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s: Init Cal Failed\n", __func__);
        FAIL(HAL_ESELFTEST);
    }


#ifndef AR9300_EMULATION
#ifdef  ATH_SUPPORT_TxBF
    ar9300InitBf(ah);			/*for TxBF*/
#endif
#endif
#if 0
    /*
     * WAR for owl 1.0 - restore chain mask for 2-chain cfgs after cal
     */
    rx_chainmask = ahp->ah_rxchainmask;
    if ((rx_chainmask == 0x5) || (rx_chainmask == 0x3)) {
        OS_REG_WRITE(ah, AR_PHY_RX_CHAINMASK, rx_chainmask);
        OS_REG_WRITE(ah, AR_PHY_CAL_CHAINMASK, rx_chainmask);
    }
#endif

    /* Restore previous led state */
    OS_REG_WRITE(ah, AR_CFG_LED, saveLedState | AR_CFG_SCLK_32KHZ);

#ifdef ATH_BT_COEX
    if (ahp->ah_btCoexConfigType != HAL_BT_COEX_CFG_NONE) {
        ar9300InitBTCoex(ah);
    }
#endif

    /* Start TSF2 for generic timer 8-15. */
    ar9300StartTsf2(ah);

    /* MIMO Power save setting */
    if ((ar9300GetCapability(ah, HAL_CAP_DYNAMIC_SMPS, 0, AH_NULL) == HAL_OK)) {
        ar9300SetSmPowerMode(ah, ahp->ah_smPowerMode);
    }

    /*
     * For big endian systems turn on swapping for descriptors
     */
#if AH_BYTE_ORDER == AH_BIG_ENDIAN
    if (AR_SREV_HORNET(ah) || AR_SREV_WASP(ah)) {
        OS_REG_RMW(ah, AR_CFG, AR_CFG_SWTB | AR_CFG_SWRB, 0);
    } else {
        ar9300_init_cfg_reg(ah);
    }
#endif

    if ( AR_SREV_OSPREY(ah) || AR_SREV_WASP(ah) ) {
        OS_REG_RMW(ah, AR_CFG_LED, AR_CFG_LED_ASSOC_CTL ,AR_CFG_LED_ASSOC_CTL );
    }

#ifndef ART_BUILD
#ifdef ATH_SUPPORT_LED
    if ( AR_SREV_WASP(ah)) {
#define REG_WRITE(_reg, _val)	*((volatile u_int32_t *)(_reg)) = (_val);
#define REG_READ(_reg)		*((volatile u_int32_t *)(_reg))
#define ATH_GPIO_OUT_FUNCTION3  0xB8040038
#define ATH_GPIO_OE             0xB8040000
        if (IS_CHAN_2GHZ((AH_PRIVATE(ah)->ah_curchan))) {
            REG_WRITE(ATH_GPIO_OUT_FUNCTION3, ( REG_READ(ATH_GPIO_OUT_FUNCTION3) & (~(0xff << 8))) | (0x33 << 8) );
            REG_WRITE(ATH_GPIO_OE, ( REG_READ(ATH_GPIO_OE) & (~(0x1 << 13) )));
        }
        else {
            /* Disable 2G WLAN LED. By default channel is in 2G Mode. Hence 
            to avoid both from blinking, disable 2G led while in 5G mode */
            REG_WRITE(ATH_GPIO_OE, ( REG_READ(ATH_GPIO_OE) | (1 << 13) ));
            REG_WRITE(ATH_GPIO_OUT_FUNCTION3, ( REG_READ(ATH_GPIO_OUT_FUNCTION3) & (~(0xff))) | (0x33) );
            REG_WRITE(ATH_GPIO_OE, ( REG_READ(ATH_GPIO_OE) & (~(0x1 << 12) )));		
        }

#undef REG_READ
#undef REG_WRITE
    }
#endif
#endif
    chan->channelFlags = ichan->channelFlags;
    chan->privFlags = ichan->privFlags;

#if FIX_NOISE_FLOOR
    if(NfHistBuffReset == 1)    
    {
#ifndef ATH_NF_PER_CHAN
        int j;
        HAL_NFCAL_HIST_FULL *h;

        if ((!is_scan) &&
            chan->channel == AH_PRIVATE(ah)->ah_curchan->channel)
        {
            h = &AH_PRIVATE(ah)->nfCalHist;
        } else {
            h = (HAL_NFCAL_HIST_FULL *) &ichan->nfCalHist;
        }
        
        ar9300StartNFCal(ah);
        for (j = 0; j < 1000; j++) {
            if ((OS_REG_READ(ah, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) == 0)
                break;
#if defined(AR9330_EMULATION) || defined(AR9485_EMULATION)
            OS_DELAY(100);
#else
            OS_DELAY(10);
#endif
		}
        h->base.currIndex = 0;
        ar9300StoreNewNf(ah, ichan, is_scan);

        /*
         * See if the NF value from the old channel should be
         * retained when switching to a new channel.
         * TBD: this may need to be changed, as it wipes out the
         * purpose of saving NF values for each channel.
         */
        for(i=0;i<NUM_NF_READINGS;i++)
        {
        	if(IS_CHAN_2GHZ(chan))
        	{     
        		if(h->nfCalBuffer[0][i] <
                    AR_PHY_CCA_NOM_VAL_OSPREY_2GHZ)
                    {
                        ichan->nfCalHist.nfCalBuffer[0][i]=
                            AH_PRIVATE(ah)->nfCalHist.nfCalBuffer[0][i];
                    }
        	} else {
                    if (AR_SREV_AR9580(ah)) {
                        if(h->nfCalBuffer[0][i] <
                        AR_PHY_CCA_NOM_VAL_PEACOCK_5GHZ)
                        {
                            ichan->nfCalHist.nfCalBuffer[0][i]=
                                AH_PRIVATE(ah)->nfCalHist.nfCalBuffer[0][i];
                        }
                    } else {
                        if(h->nfCalBuffer[0][i] <
                           AR_PHY_CCA_NOM_VAL_OSPREY_5GHZ)
                        {
                            ichan->nfCalHist.nfCalBuffer[0][i]=
                                AH_PRIVATE(ah)->nfCalHist.nfCalBuffer[0][i];
                        }
                    }
        	}
        }
        /*
         * Copy the channel's NF buffer, which may have been modified
         * just above here, to the full NF history buffer.
         */
        ar9300ResetNfHistBuff(ah, ichan);
#endif /* ATH_NF_PER_CHAN */
    }
        ar9300LoadNF(ah, AH_PRIVATE(ah)->ah_curchan, is_scan);
        ar9300StartNFCal(ah);  
#endif

#ifdef AH_SUPPORT_AR9300
    /* BB Panic Watchdog */
    if ((ar9300GetCapability(ah, HAL_CAP_BB_PANIC_WATCHDOG, 0, AH_NULL) == HAL_OK)) {
        ar9300ConfigBbPanicWatchdog(ah);
    }
#endif
#ifdef AR9340_EMULATION 
    /* XXX: Check if this is required for chip */
    OS_REG_WRITE(ah, 0x409c, 0x1);
#endif

    /* While receiving unsupported rate frame receive state machine
     * gets into a state 0xb and if PhyRestart happens when rx
     * state machine is in 0xb state, BB would go hang, if we
     * see 0xb state after first bb panic, make sure that we
     * disable the PhyRestart.
     * 
     * There may be multiple panics, make sure that we always do
     * this if we see this panic at least once. This is required
     * because reset seems to be writing from INI file.
     */
    if ((ar9300GetCapability(ah, HAL_CAP_PHYRESTART_CLR_WAR, 0, AH_NULL)
         == HAL_OK) && (((MS((AH_PRIVATE(ah)->ah_bbPanicLastStatus),
                AR_PHY_BB_WD_RX_OFDM_SM)) == 0xb) ||
            AH_PRIVATE(ah)->ah_phyrestart_disabled) )
    {
        ar9300DisablePhyRestart(ah, 1);
    }

#ifdef ART_BUILD
    /* init OTP control setting */
    if(AR_SREV_HORNET(ah) || AR_SREV_WASP(ah)) {
        ar9300_init_otp(ah);
    }
#endif

#ifdef AR9485_EMULATION
    {
        /* 
         *  Fixed TSF timer inaccurately issue on emulation (faster than real chip)
         *  Below 2 methods are workable
         */
        u_int32_t usec;
#if 0 
        /* TSF from WMAC clock */
        usec = (OS_REG_READ(ah, AR_USEC) & ~AR_USEC_USEC) | 0x2;
        OS_REG_WRITE(ah, AR_USEC, usec);
        OS_REG_SET_BIT(ah, AR_SLP32_MODE, 0x00400000);
#else 
        /* TSF from 32KHz clock */
        OS_REG_WRITE(ah, AR_SLP32_INC, 0xaaa);
#endif
    }
#endif
    ahp->ah_radar1 = MS(OS_REG_READ(ah, AR_PHY_RADAR_1),
                        AR_PHY_RADAR_1_CF_BIN_THRESH);
    ahp->ah_dc_offset = MS(OS_REG_READ(ah, AR_PHY_TIMING2),
                        AR_PHY_TIMING2_DC_OFFSET);
    ahp->ah_disable_cck = MS(OS_REG_READ(ah, AR_PHY_MODE),
                        AR_PHY_MODE_DISABLE_CCK);

    if (ap->ah_enable_keysearch_always) {
        ar9300_enable_keysearch_always(ah, 1);
    }

#if ATH_LOW_POWER_ENABLE
#define REG_WRITE(_reg, _val)	*((volatile u_int32_t *)(_reg)) = (_val);
#define REG_READ(_reg)		*((volatile u_int32_t *)(_reg))
    if (AR_SREV_OSPREY(ah)) {
        REG_WRITE(0xb4000080, REG_READ(0xb4000080) | 3);
        OS_REG_WRITE(ah, AR_RTC_RESET, 1);
        OS_REG_SET_BIT(ah, AR_HOSTIF_REG(ah, AR_PCIE_PM_CTRL), AR_PCIE_PM_CTRL_ENA);
        OS_REG_SET_BIT(ah, AR_HOSTIF_REG(ah, AR_SPARE), 0xffffffff);
    }
#undef REG_READ
#undef REG_WRITE
#endif	/* ATH_LOW_POWER_ENABLE */

    WAR_USB_DISABLE_PLL_LOCK_DETECT(ah);

	/* H/W Green TX */
    ar9300_control_signals_for_green_tx_mode(ah);
    /* Fix for EVID 108445 - Consecutive beacon stuck */
    /* set AR_PCU_MISC_MODE2 bit 7 CFP_IGNORE to 1*/
    OS_REG_WRITE(ah, AR_PCU_MISC_MODE2,
                (OS_REG_READ(ah, AR_PCU_MISC_MODE2)|0x80));
    ar9300SetSmartAntenna(ah, ahp->ah_smartantenna_enable);
    return AH_TRUE;
bad:
    OS_MARK(ah, AH_MARK_RESET_DONE, ecode);
    *status = ecode;
    return AH_FALSE;
#undef FAIL
}

void
ar9300GreenApPsOnOff( struct ath_hal *ah, u_int16_t onOff)
{
    /* Set/reset the ps flag */
    AH_PRIVATE(ah)->greenApPsOn = !!onOff;
}

/*
 * This function returns 1, where it is possible to do
 * single-chain power save.
 */
u_int16_t
ar9300IsSingleAntPowerSavePossible(struct ath_hal *ah)
{
    return AH_TRUE;
}

#ifndef AR9300_EMULATION_BB /* To avoid compilation warnings. Functions not used when EMULATION. */
#ifndef AR9340_EMULATION
/*
 * ar9300FindMagApprox()
 */
static int32_t
ar9300FindMagApprox(struct ath_hal *ah, int32_t in_re, int32_t in_im)
{
	int32_t abs_i = abs(in_re),
	        abs_q = abs(in_im),
		    max_abs, min_abs;

    if (abs_i > abs_q) {
        max_abs = abs_i;
        min_abs = abs_q;
    } else {
        max_abs = abs_q;
        min_abs = abs_i; 
    }

    return (max_abs - (max_abs / 32) + (min_abs / 8) + (min_abs / 4));
}

/* 
 * ar9300SolveIqCal()       
 * solve 4x4 linear equation used in loopback iq cal.
 */
static HAL_BOOL
ar9300SolveIqCal(struct ath_hal *ah,
				 int32_t sin_2phi_1,
				 int32_t cos_2phi_1,
				 int32_t sin_2phi_2,
				 int32_t cos_2phi_2,
				 int32_t mag_a0_d0,
				 int32_t phs_a0_d0,
				 int32_t mag_a1_d0,
				 int32_t phs_a1_d0,
				 int32_t solved_eq[])
{
	int32_t f1 = cos_2phi_1 - cos_2phi_2,
	        f3 = sin_2phi_1 - sin_2phi_2,
		    f2;
	int32_t mag_tx, phs_tx, mag_rx, phs_rx;
	const int32_t result_shift = 1 << 15;

        f2 = ((f1>>3) * (f1>>3) + (f3>>3) * (f3>>3))>>9;

	if (0 == f2) {
		HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: Divide by 0(%d).\n",
				 __func__, __LINE__);
		return AH_FALSE;
	}

	mag_tx = f1 * (mag_a0_d0  - mag_a1_d0) + f3 * (phs_a0_d0 - phs_a1_d0); // mag mismatch, tx
	phs_tx = f3 * (-mag_a0_d0 + mag_a1_d0) + f1 * (phs_a0_d0 - phs_a1_d0); // phs mismatch, tx

	mag_tx = (mag_tx / f2);
	phs_tx = (phs_tx / f2);

	mag_rx = mag_a0_d0 - (cos_2phi_1 * mag_tx + sin_2phi_1 * phs_tx) / result_shift; // mag mismatch, rx
	phs_rx = phs_a0_d0 + (sin_2phi_1 * mag_tx - cos_2phi_1 * phs_tx) / result_shift; // phs mismatch, rx

	solved_eq[0] = mag_tx;
	solved_eq[1] = phs_tx;
	solved_eq[2] = mag_rx;
	solved_eq[3] = phs_rx;

	return AH_TRUE;
}

/*
 * ar9300CalcIqCorr()
 */
static HAL_BOOL
ar9300CalcIqCorr(struct ath_hal *ah, int32_t chain_idx , const int32_t iq_res[],
				 int32_t iqc_coeff[])
{
    int32_t i2_m_q2_a0_d0, i2_p_q2_a0_d0, iq_corr_a0_d0,
		    i2_m_q2_a0_d1, i2_p_q2_a0_d1, iq_corr_a0_d1,
            i2_m_q2_a1_d0, i2_p_q2_a1_d0, iq_corr_a1_d0,
            i2_m_q2_a1_d1, i2_p_q2_a1_d1, iq_corr_a1_d1;
	int32_t mag_a0_d0, mag_a1_d0, mag_a0_d1, mag_a1_d1,
		    phs_a0_d0, phs_a1_d0, phs_a0_d1, phs_a1_d1,
            sin_2phi_1, cos_2phi_1, sin_2phi_2, cos_2phi_2;
    int32_t mag_tx, phs_tx, mag_rx, phs_rx;
	int32_t	solved_eq[4], mag_corr_tx, phs_corr_tx, mag_corr_rx, phs_corr_rx,
		    q_q_coff, q_i_coff;
	const int32_t res_scale = 1 << 15;
	const int32_t delpt_shift = 1 << 8;
	int32_t mag1, mag2;


    i2_m_q2_a0_d0 = iq_res[0] & 0xfff;
    i2_p_q2_a0_d0 = (iq_res[0] >> 12) & 0xfff;
    iq_corr_a0_d0 = ((iq_res[0] >> 24) & 0xff) + ((iq_res[1] & 0xf) << 8);

    if (i2_m_q2_a0_d0 > 0x800)  {
	    i2_m_q2_a0_d0 = -((0xfff - i2_m_q2_a0_d0) + 1);
	}
	if (i2_p_q2_a0_d0 > 0x800)  {
	    i2_p_q2_a0_d0 = -((0xfff - i2_p_q2_a0_d0) + 1);
	}
	if (iq_corr_a0_d0 > 0x800)  {
	    iq_corr_a0_d0 = -((0xfff - iq_corr_a0_d0) + 1);
	}

    i2_m_q2_a0_d1 = (iq_res[1] >> 4) & 0xfff;
    i2_p_q2_a0_d1 = (iq_res[2] & 0xfff); 
    iq_corr_a0_d1 = (iq_res[2] >> 12) & 0xfff;

    if (i2_m_q2_a0_d1 > 0x800)  {
	    i2_m_q2_a0_d1 = -((0xfff - i2_m_q2_a0_d1) + 1);
	}
	if (i2_p_q2_a0_d1 > 0x800)  {
	    i2_p_q2_a0_d1 = -((0xfff - i2_p_q2_a0_d1) + 1);
	}
	if (iq_corr_a0_d1 > 0x800)  {
	    iq_corr_a0_d1 = -((0xfff - iq_corr_a0_d1) + 1);
	}

    i2_m_q2_a1_d0 = ((iq_res[2] >> 24) & 0xff) + ((iq_res[3] & 0xf) << 8);
    i2_p_q2_a1_d0 = (iq_res[3] >> 4) & 0xfff; 
    iq_corr_a1_d0 = iq_res[4] & 0xfff;

    if (i2_m_q2_a1_d0 > 0x800)  {
	    i2_m_q2_a1_d0 = -((0xfff - i2_m_q2_a1_d0) + 1);
	}
	if (i2_p_q2_a1_d0 > 0x800)  {
	    i2_p_q2_a1_d0 = -((0xfff - i2_p_q2_a1_d0) + 1);
	}
	if (iq_corr_a1_d0 > 0x800)  {
	    iq_corr_a1_d0 = -((0xfff - iq_corr_a1_d0) + 1);
	}

    i2_m_q2_a1_d1 = (iq_res[4] >> 12) & 0xfff;
    i2_p_q2_a1_d1 = ((iq_res[4] >> 24) & 0xff) + ((iq_res[5] & 0xf) << 8); 
    iq_corr_a1_d1 = (iq_res[5] >> 4) & 0xfff;

    if (i2_m_q2_a1_d1 > 0x800)  {
	    i2_m_q2_a1_d1 = -((0xfff - i2_m_q2_a1_d1) + 1);
	}
	if (i2_p_q2_a1_d1 > 0x800)  {
	    i2_p_q2_a1_d1 = -((0xfff - i2_p_q2_a1_d1) + 1);
	}
	if (iq_corr_a1_d1 > 0x800)  {
	    iq_corr_a1_d1 = -((0xfff - iq_corr_a1_d1) + 1);
	}

    if ((i2_p_q2_a0_d0 == 0) ||
		(i2_p_q2_a0_d1 == 0) ||
		(i2_p_q2_a1_d0 == 0) ||
		(i2_p_q2_a1_d1 == 0)) {
		HDPRINTF(ah, HAL_DBG_CALIBRATE,
				 "%s: Divide by 0(%d):\na0_d0=%d\na0_d1=%d\na2_d0=%d\na1_d1=%d\n",
				 __func__, __LINE__, i2_p_q2_a0_d0, i2_p_q2_a0_d1, i2_p_q2_a1_d0,
				 i2_p_q2_a1_d1);
		return AH_FALSE;
	}

	mag_a0_d0 = (i2_m_q2_a0_d0 * res_scale) / i2_p_q2_a0_d0;
	phs_a0_d0 = (iq_corr_a0_d0 * res_scale) / i2_p_q2_a0_d0;

	mag_a0_d1 = (i2_m_q2_a0_d1 * res_scale) / i2_p_q2_a0_d1;
	phs_a0_d1 = (iq_corr_a0_d1 * res_scale) / i2_p_q2_a0_d1;

	mag_a1_d0 = (i2_m_q2_a1_d0 * res_scale) / i2_p_q2_a1_d0;
	phs_a1_d0 = (iq_corr_a1_d0 * res_scale) / i2_p_q2_a1_d0;

	mag_a1_d1 = (i2_m_q2_a1_d1 * res_scale) / i2_p_q2_a1_d1;
	phs_a1_d1 = (iq_corr_a1_d1 * res_scale) / i2_p_q2_a1_d1;

    sin_2phi_1 = (((mag_a0_d0 - mag_a0_d1) * delpt_shift) / DELPT); // w/o analog phase shift
    cos_2phi_1 = (((phs_a0_d1 - phs_a0_d0) * delpt_shift) / DELPT); // w/o analog phase shift
    sin_2phi_2 = (((mag_a1_d0 - mag_a1_d1) * delpt_shift) / DELPT); // w/  analog phase shift
    cos_2phi_2 = (((phs_a1_d1 - phs_a1_d0) * delpt_shift) / DELPT); // w/  analog phase shift

    // force sin^2 + cos^2 = 1; 
    // find magnitude by approximation
    mag1 = ar9300FindMagApprox(ah, cos_2phi_1, sin_2phi_1);
    mag2 = ar9300FindMagApprox(ah, cos_2phi_2, sin_2phi_2);

    if ((mag1 == 0) ||
		(mag2 == 0)) {
		HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: Divide by 0(%d): mag1=%d, mag2=%d\n",
				 __func__, __LINE__, mag1, mag2);
		return AH_FALSE;
	}

    // normalization sin and cos by mag
    sin_2phi_1 = (sin_2phi_1 * res_scale / mag1);
    cos_2phi_1 = (cos_2phi_1 * res_scale / mag1);
    sin_2phi_2 = (sin_2phi_2 * res_scale / mag2);
    cos_2phi_2 = (cos_2phi_2 * res_scale / mag2);

    // calculate IQ mismatch
    if (AH_FALSE == ar9300SolveIqCal(ah, sin_2phi_1, cos_2phi_1, sin_2phi_2, cos_2phi_2,
                                     mag_a0_d0, phs_a0_d0, mag_a1_d0, phs_a1_d0, solved_eq)) {
		HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: Call to ar9300SolveIqCal failed.\n",
				 __func__);
		return AH_FALSE;
	}
   
    mag_tx = solved_eq[0];
    phs_tx = solved_eq[1];
    mag_rx = solved_eq[2];
    phs_rx = solved_eq[3];

	HDPRINTF(ah, HAL_DBG_CALIBRATE,
			 "%s: chain %d: mag mismatch=%d phase mismatch=%d\n",
			 __func__, chain_idx, mag_tx/res_scale, phs_tx/res_scale);
  
    if (res_scale == mag_tx) {
		HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: Divide by 0(%d): mag_tx=%d, res_scale=%d\n",
				 __func__, __LINE__, mag_tx, res_scale);
		return AH_FALSE;
	}

	// calculate and quantize Tx IQ correction factor
	mag_corr_tx = (mag_tx * res_scale) / (res_scale - mag_tx);
	phs_corr_tx = -phs_tx;

	q_q_coff = (mag_corr_tx * 128 / res_scale);
	q_i_coff = (phs_corr_tx * 256 / res_scale);

	HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: tx chain %d: mag corr=%d  phase corr=%d\n",
			 __func__, chain_idx, q_q_coff, q_i_coff);

	if (q_i_coff < -63)
		{q_i_coff = -63;}
	if (q_i_coff > 63)
		{q_i_coff = 63;}
	if (q_q_coff < -63)
		{q_q_coff = -63;}
	if (q_q_coff > 63)
		{q_q_coff = 63;}

	iqc_coeff[0] = (q_q_coff * 128) + q_i_coff;

	HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: tx chain %d: iq corr coeff=%x\n",
			 __func__, chain_idx, iqc_coeff[0]);  

    if (-mag_rx == res_scale) {
		HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: Divide by 0(%d): mag_rx=%d, res_scale=%d\n",
				 __func__, __LINE__, mag_rx, res_scale);
		return AH_FALSE;
	}

	// calculate and quantize Rx IQ correction factors
    mag_corr_rx = (-mag_rx * res_scale) / (res_scale + mag_rx);
    phs_corr_rx = -phs_rx;

    q_q_coff = (mag_corr_rx * 128 / res_scale);
    q_i_coff = (phs_corr_rx * 256 / res_scale);

	HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: rx chain %d: mag corr=%d  phase corr=%d\n",
			 __func__, chain_idx, q_q_coff, q_i_coff);

    if (q_i_coff < -63)
		{q_i_coff = -63;}
    if (q_i_coff > 63)
		{q_i_coff = 63;}
    if (q_q_coff < -63)
		{q_q_coff = -63;}
    if (q_q_coff > 63)
		{q_q_coff = 63;}
   
    iqc_coeff[1] = (q_q_coff * 128) + q_i_coff;

    HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: rx chain %d: iq corr coeff=%x\n",
			 __func__, chain_idx, iqc_coeff[1]);  

    return AH_TRUE;
}

#define MAX_MAG_DELTA 11 //maximum magnitude mismatch delta across gains
#define MAX_PHS_DELTA 10 //maximum phase mismatch delta across gains
#define ABS(x) ((x)>=0?(x):(-(x)))
	
static void 
ar9300_tx_iq_cal_outlier_detection(struct ath_hal *ah, u_int32_t num_chains, 
    struct coeff_t *coeff)
{
	int nmeasurement, ch_idx, im;
	int32_t magnitude,phase;
	int32_t magnitude_max,phase_max;
	int32_t magnitude_min,phase_min;

	int32_t magnitude_max_idx,phase_max_idx;
	int32_t magnitude_min_idx,phase_min_idx;

	int32_t magnitude_avg,phase_avg;
	int32_t outlier_mag_idx=0;
	int32_t outlier_phs_idx=0;

	u_int32_t tx_corr_coeff[MAX_MEASUREMENT][AR9300_MAX_CHAINS] = {
		{AR_PHY_TX_IQCAL_CORR_COEFF_01_B0, 
		    AR_PHY_TX_IQCAL_CORR_COEFF_01_B1, 
		    AR_PHY_TX_IQCAL_CORR_COEFF_01_B2},
		{AR_PHY_TX_IQCAL_CORR_COEFF_01_B0, 
		    AR_PHY_TX_IQCAL_CORR_COEFF_01_B1, 
		    AR_PHY_TX_IQCAL_CORR_COEFF_01_B2},
		{AR_PHY_TX_IQCAL_CORR_COEFF_23_B0, 
		    AR_PHY_TX_IQCAL_CORR_COEFF_23_B1, 
		    AR_PHY_TX_IQCAL_CORR_COEFF_23_B2},
		{AR_PHY_TX_IQCAL_CORR_COEFF_23_B0, 
		    AR_PHY_TX_IQCAL_CORR_COEFF_23_B1, 
		    AR_PHY_TX_IQCAL_CORR_COEFF_23_B2},
		{AR_PHY_TX_IQCAL_CORR_COEFF_45_B0, 
		    AR_PHY_TX_IQCAL_CORR_COEFF_45_B1, 
		    AR_PHY_TX_IQCAL_CORR_COEFF_45_B2},
		{AR_PHY_TX_IQCAL_CORR_COEFF_45_B0, 
		    AR_PHY_TX_IQCAL_CORR_COEFF_45_B1, 
		    AR_PHY_TX_IQCAL_CORR_COEFF_45_B2},
		{AR_PHY_TX_IQCAL_CORR_COEFF_67_B0, 
		    AR_PHY_TX_IQCAL_CORR_COEFF_67_B1, 
		    AR_PHY_TX_IQCAL_CORR_COEFF_67_B2},
		{AR_PHY_TX_IQCAL_CORR_COEFF_67_B0, 
		    AR_PHY_TX_IQCAL_CORR_COEFF_67_B1, 
		    AR_PHY_TX_IQCAL_CORR_COEFF_67_B2},
	};
	
    if (AR_SREV_POSEIDON(ah)) {

        HALASSERT(num_chains == 0x1);

        tx_corr_coeff[0][0] = AR_PHY_TX_IQCAL_CORR_COEFF_01_B0_POSEIDON;
        tx_corr_coeff[1][0] = AR_PHY_TX_IQCAL_CORR_COEFF_01_B0_POSEIDON;
        tx_corr_coeff[2][0] = AR_PHY_TX_IQCAL_CORR_COEFF_23_B0_POSEIDON;
        tx_corr_coeff[3][0] = AR_PHY_TX_IQCAL_CORR_COEFF_23_B0_POSEIDON;
        tx_corr_coeff[4][0] = AR_PHY_TX_IQCAL_CORR_COEFF_45_B0_POSEIDON;
        tx_corr_coeff[5][0] = AR_PHY_TX_IQCAL_CORR_COEFF_45_B0_POSEIDON;
        tx_corr_coeff[6][0] = AR_PHY_TX_IQCAL_CORR_COEFF_67_B0_POSEIDON;
        tx_corr_coeff[7][0] = AR_PHY_TX_IQCAL_CORR_COEFF_67_B0_POSEIDON;
    }	
	
	for (ch_idx = 0; ch_idx < num_chains; ch_idx++) {

        if (AR_SREV_POSEIDON(ah))
    		nmeasurement=OS_REG_READ_FIELD(ah,AR_PHY_TX_IQCAL_STATUS_B0_POSEIDON,AR_PHY_CALIBRATED_GAINS_0);
        else
    		nmeasurement=OS_REG_READ_FIELD(ah,AR_PHY_TX_IQCAL_STATUS_B0,AR_PHY_CALIBRATED_GAINS_0);
		if (nmeasurement>MAX_MEASUREMENT) {
			nmeasurement=MAX_MEASUREMENT;
		}

        // reset max/min variable to min/max values so that we always start with 1st calibrated gain value
	    magnitude_max = -64;
		phase_max     = -64;
	    magnitude_min = 63;
		phase_min     = 63;
		magnitude_avg = 0;
	    phase_avg     = 0;
		magnitude_max_idx = 0;
		magnitude_min_idx = 0;
		phase_max_idx = 0;
		phase_min_idx = 0;

        // detect outlier only if nmeasurement>1
		if (nmeasurement>1){
			//printf("----------- start outlier detection -----------\n");
			// find max/min and phase/mag mismatch across all calibrated gains
			for (im=0; im<nmeasurement; im++) {
				magnitude=coeff->mag_coeff[ch_idx][im];
				phase=coeff->phs_coeff[ch_idx][im];

				magnitude_avg = magnitude_avg + magnitude;
				phase_avg = phase_avg + phase;
				if (magnitude>magnitude_max) {
					magnitude_max = magnitude;
					magnitude_max_idx = im;
				}
				if (magnitude<magnitude_min) {
					magnitude_min = magnitude;
					magnitude_min_idx = im;
				}
				if (phase>phase_max) {
					phase_max = phase;
					phase_max_idx = im;
				}
				if (phase<phase_min) {
					phase_min = phase;
					phase_min_idx = im;
				}
  			}
			// find average (exclude max abs value)
			for (im=0; im<nmeasurement; im++) {
				magnitude=coeff->mag_coeff[ch_idx][im];
				phase=coeff->phs_coeff[ch_idx][im];
				if ((ABS(magnitude)<ABS(magnitude_max)) || (ABS(magnitude)<ABS(magnitude_min))) {
					magnitude_avg = magnitude_avg + magnitude;
				}
				if ((ABS(phase)<ABS(phase_max)) || (ABS(phase)<ABS(phase_min))) {
					phase_avg = phase_avg + phase;
				}
  			}
			magnitude_avg = magnitude_avg/(nmeasurement-1);
			phase_avg = phase_avg/(nmeasurement-1);
			//detect mag outlier
			if (ABS(magnitude_max-magnitude_min)>MAX_MAG_DELTA){
				if (ABS(magnitude_max-magnitude_avg)> ABS(magnitude_min-magnitude_avg)){
					// max is outlier, force to avg
					outlier_mag_idx = magnitude_max_idx;
				}else{
					// min is outlier, force to avg
					outlier_mag_idx = magnitude_min_idx;
				}
				coeff->mag_coeff[ch_idx][outlier_mag_idx] = magnitude_avg;
				coeff->phs_coeff[ch_idx][outlier_mag_idx] = phase_avg;
				//printf("[ch%d][outlier mag gain%d]:: mag_avg = %d (/128), phase_avg = %d (/256)\n", ch_idx, outlier_mag_idx, magnitude_avg, phase_avg);
			}
			//detect phs outlier
		    if (ABS(phase_max - phase_min)>MAX_PHS_DELTA) {
				if (ABS(phase_max-phase_avg)> ABS(phase_min-phase_avg)){
					// max is outlier, force to avg
					outlier_phs_idx = phase_max_idx;
				}else{
					// min is outlier, force to avg
					outlier_phs_idx = phase_min_idx;
				}
				coeff->mag_coeff[ch_idx][outlier_phs_idx] = magnitude_avg;
				coeff->phs_coeff[ch_idx][outlier_phs_idx] = phase_avg;
				//printf("[ch%d][outlier phs gain%d]:: mag_avg = %d (/128), phase_avg = %d (/256)\n", ch_idx, outlier_phs_idx, magnitude_avg, phase_avg);
			}
		}

		//printf("------------ after outlier detection -------------\n");
		for (im=0; im<nmeasurement; im++) {
		    magnitude=coeff->mag_coeff[ch_idx][im];
		    phase=coeff->phs_coeff[ch_idx][im];

			//printf("[ch%d][gain%d]:: mag = %d (/128), phase = %d (/256)\n",
			//	ch_idx, im, magnitude, phase);

			coeff->iqc_coeff[0] = (magnitude & 0x7f) | ((phase & 0x7f) << 7);

			if((im%2)==0) {
				OS_REG_RMW_FIELD(ah, tx_corr_coeff[im][ch_idx],
							 AR_PHY_TX_IQCAL_CORR_COEFF_00_COEFF_TABLE, coeff->iqc_coeff[0]); 
			} else {
				OS_REG_RMW_FIELD(ah, tx_corr_coeff[im][ch_idx],
							 AR_PHY_TX_IQCAL_CORR_COEFF_01_COEFF_TABLE, coeff->iqc_coeff[0]); 
			}
		}
	}

	OS_REG_RMW_FIELD(ah, AR_PHY_TX_IQCAL_CONTROL_3,
		             AR_PHY_TX_IQCAL_CONTROL_3_IQCORR_EN, 0x1);
	OS_REG_RMW_FIELD(ah, AR_PHY_RX_IQCAL_CORR_B0,
					 AR_PHY_RX_IQCAL_CORR_B0_LOOPBACK_IQCORR_EN, 0x1);

    return;
      	
}

// this function is only needed for osprey/wasp/hornet
// it is not needed for jupiter/poseidon
HAL_BOOL
ar9300_tx_iq_cal_hw_run(struct ath_hal *ah)
{
    int isTxGainForced = 0;
    
    isTxGainForced = OS_REG_READ_FIELD(ah, AR_PHY_TX_FORCED_GAIN, AR_PHY_TXGAIN_FORCE);
    if (isTxGainForced) {
        //printf("Tx gain can not be forced during tx I/Q cal!\n");
        OS_REG_RMW_FIELD(ah, AR_PHY_TX_FORCED_GAIN, AR_PHY_TXGAIN_FORCE, 0);
    }

	// enable tx IQ cal
	OS_REG_RMW_FIELD(ah, AR_PHY_TX_IQCAL_START, AR_PHY_TX_IQCAL_START_DO_CAL,
					 AR_PHY_TX_IQCAL_START_DO_CAL);


	if (!ath_hal_wait(ah, AR_PHY_TX_IQCAL_START, AR_PHY_TX_IQCAL_START_DO_CAL,
				  0, AH_WAIT_TIMEOUT)) {
       HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: Tx IQ Cal is never completed.\n", __func__);
	   return AH_FALSE;
    }
	return AH_TRUE;
}

static void
ar9300_tx_iq_cal_post_proc(struct ath_hal *ah)
{

	int nmeasurement, im;
    struct ath_hal_9300     *ahp = AH9300(ah);
	u_int32_t txiqcal_status[AR9300_MAX_CHAINS] = {
		AR_PHY_TX_IQCAL_STATUS_B0,
		AR_PHY_TX_IQCAL_STATUS_B1,
		AR_PHY_TX_IQCAL_STATUS_B2,
	};
	
	const u_int32_t chan_info_tab[] = {
		AR_PHY_CHAN_INFO_TAB_0,
		AR_PHY_CHAN_INFO_TAB_1,
		AR_PHY_CHAN_INFO_TAB_2,
	};

	int32_t iq_res[6];
	int32_t ch_idx, j;
    u_int32_t line, num_chains = 0;
	struct coeff_t coeff;

    if (AR_SREV_POSEIDON(ah))
        txiqcal_status[0] = AR_PHY_TX_IQCAL_STATUS_B0_POSEIDON;

	for (ch_idx = 0; ch_idx < AR9300_MAX_CHAINS; ch_idx++) {
		if (ahp->ah_txchainmask & (1 << ch_idx)) {
			num_chains++;
		}
	}


	for (ch_idx = 0; ch_idx < num_chains; ch_idx++) {
        if AR_SREV_POSEIDON(ah)
		    nmeasurement=OS_REG_READ_FIELD(ah,AR_PHY_TX_IQCAL_STATUS_B0_POSEIDON,AR_PHY_CALIBRATED_GAINS_0);
        else 
		    nmeasurement=OS_REG_READ_FIELD(ah,AR_PHY_TX_IQCAL_STATUS_B0,AR_PHY_CALIBRATED_GAINS_0);
        
		if(nmeasurement>MAX_MEASUREMENT) {
			nmeasurement=MAX_MEASUREMENT;
		}

		for(im=0; im<nmeasurement; im++) {

			HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: Doing Tx IQ Cal for chain %d.\n",
					 __func__, ch_idx);
			if (OS_REG_READ(ah, txiqcal_status[ch_idx]) & AR_PHY_TX_IQCAL_STATUS_FAILED) {
				HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: Tx IQ Cal failed for chain %d.\n",
						 __func__, ch_idx);
				line = __LINE__;
				goto TX_IQ_CAL_FAILED;
			}
		
			for (j = 0; j < 3; j++) {
				u_int32_t idx = 2 * j;
				u_int32_t offset = 4 * (3*im + j);		// 3 registers for each calibration result

				OS_REG_RMW_FIELD(ah, AR_PHY_CHAN_INFO_MEMORY, AR_PHY_CHAN_INFO_TAB_S2_READ, 0);
				iq_res[idx] = OS_REG_READ(ah, chan_info_tab[ch_idx] + offset); // 32 bits
				OS_REG_RMW_FIELD(ah, AR_PHY_CHAN_INFO_MEMORY, AR_PHY_CHAN_INFO_TAB_S2_READ, 1);
				iq_res[idx+1] = 0xffff & OS_REG_READ(ah, chan_info_tab[ch_idx] + offset); // 16 bits

				HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: IQ RES[%d]=0x%x IQ_RES[%d]=0x%x\n",
						 __func__, idx, iq_res[idx], idx+1, iq_res[idx+1]);
			}

			if (AH_FALSE == ar9300CalcIqCorr(ah, ch_idx, iq_res, coeff.iqc_coeff)) {
				HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: Failed in calculation of IQ correction.\n",
						 __func__);
				line = __LINE__;
				goto TX_IQ_CAL_FAILED;
			}
			coeff.mag_coeff[ch_idx][im] = coeff.iqc_coeff[0] & 0x7f;
			coeff.phs_coeff[ch_idx][im] = (coeff.iqc_coeff[0]>>7) & 0x7f;
			if (coeff.mag_coeff[ch_idx][im]> 63) coeff.mag_coeff[ch_idx][im]-= 128;
			if (coeff.phs_coeff[ch_idx][im]> 63) coeff.phs_coeff[ch_idx][im]-= 128;			
			/*printf("[ch%d][gain%d]:: mag = %d (/128), phase = %d (/256)\n",
				ch_idx, im, coeff.mag_coeff[ch_idx][im], coeff.phs_coeff[ch_idx][im]);*/

		}
	}
	ar9300_tx_iq_cal_outlier_detection(ah, num_chains, &coeff);
	return;
TX_IQ_CAL_FAILED:
    //ath_hal_printf(ah, "Tx IQ Cal failed(%d)\n", line); // no need to print this, it is AGC failure not chip stuck
    return;

}

#endif
#endif

/*
 * ar9300DisablePhyRestart
 *
 * In some BBpanics, we can disable the phyrestart
 * disable_phy_restart
 *      != 0, disable the phy restart in h/w
 *      == 0, enable the phy restart in h/w
 */
void ar9300DisablePhyRestart(struct ath_hal *ah, int disable_phy_restart)
{
    u_int32_t val;

    val = OS_REG_READ(ah, AR_PHY_RESTART);
    if (disable_phy_restart) {
        val &= ~AR_PHY_RESTART_ENA;
        AH_PRIVATE(ah)->ah_phyrestart_disabled = 1;
    } else {
        val |= AR_PHY_RESTART_ENA;
        AH_PRIVATE(ah)->ah_phyrestart_disabled = 0;
    }
    OS_REG_WRITE(ah, AR_PHY_RESTART, val);

    val = OS_REG_READ(ah, AR_PHY_RESTART);
}

#ifdef ART_BUILD

void ar9300_get_hornet_pll_info(u_int32_t *cpu_freq, u_int32_t *ddr_freq, u_int32_t *ahb_freq)
{
#ifdef _WIN64
	#define AR_SOC_SEL_25M_40M      0xB80600ACULL
	#define AR_SOC_CPU_PLL_CONFIG   0xB8050000ULL
	#define AR_SOC_CLOCK_CONTROL    0xB8050008ULL
	#define AR_SOC_PLL_DITHER_FRAC  0xB8050010ULL
	#define AR_SOC_PLL_DITHER       0xB8050014ULL
#else    
	#define AR_SOC_SEL_25M_40M      0xB80600AC
	#define AR_SOC_CPU_PLL_CONFIG   0xB8050000
	#define AR_SOC_CLOCK_CONTROL    0xB8050008
	#define AR_SOC_PLL_DITHER_FRAC  0xB8050010
	#define AR_SOC_PLL_DITHER       0xB8050014
#endif
	
    u_int32_t regBoot_Strap;
    u_int32_t regCpu_Pll_Config;
    u_int32_t regClock_Control;
    u_int32_t regPll_Dither_Frac;
    u_int32_t regPll_Dither;  
    u_int32_t sel_25m_40m;
    u_int32_t outdiv, refdiv, nint, nfrac;
    u_int32_t nfrac_min, nfrac_max;
    u_int32_t dither_en;
    u_int32_t cpu_post_div, ddr_post_div, ahb_post_div, bypass;
    //====================================================================
    u_int32_t xtal, vco, pll_freq;
    u_int32_t vco_int, vco_frac, outdiv_decimal;
    //====================================================================

    MyRegisterRead(AR_SOC_SEL_25M_40M, &regBoot_Strap);
    sel_25m_40m = (regBoot_Strap >> 0) & 0x1;
    if (sel_25m_40m)
        xtal = 40*1000*1000;
    else
        xtal = 25*1000*1000;                

    MyRegisterRead(AR_SOC_CPU_PLL_CONFIG, &regCpu_Pll_Config);
    MyRegisterRead(AR_SOC_CLOCK_CONTROL, &regClock_Control);
    MyRegisterRead(AR_SOC_PLL_DITHER_FRAC, &regPll_Dither_Frac);
    MyRegisterRead(AR_SOC_PLL_DITHER, &regPll_Dither);

    outdiv = (regCpu_Pll_Config >> 23) & 0x7;
    refdiv = (regCpu_Pll_Config >> 16) & 0x1f;
    nint   = (regCpu_Pll_Config >> 10) & 0x3f;
    
    nfrac_max = (regPll_Dither_Frac >>  0) & 0x3ff; 
    nfrac_max <<= 8;
    nfrac_min = (regPll_Dither_Frac >> 10) & 0x3ff;
    nfrac_min <<= 8;  
    
    dither_en = (regPll_Dither >> 31) & 0x1;
    
    if (dither_en)
        nfrac = (nfrac_max + nfrac_min)/2;
    else
        nfrac = nfrac_min;
    
    cpu_post_div = (regClock_Control >>  5) & 0x3;
    ddr_post_div = (regClock_Control >> 10) & 0x3;
    ahb_post_div = (regClock_Control >> 15) & 0x3;
    
    bypass       = (regClock_Control >> 2) & 0x1;
    
    cpu_post_div++;
    ddr_post_div++;
    ahb_post_div++;
        
    if (bypass) {
        *cpu_freq = xtal/cpu_post_div;
        *ddr_freq = xtal/ddr_post_div;
        *ahb_freq = xtal/ahb_post_div;
        return;
    }
    
    vco_int  = (xtal / refdiv) * nint;    
    vco_frac = (xtal / (refdiv * 0x40000)) * nfrac;
    
    vco = vco_int + vco_frac;
    
    outdiv_decimal = pow(2, outdiv);

    pll_freq = vco / outdiv_decimal;
    
    *cpu_freq = pll_freq/cpu_post_div;
    *ddr_freq = pll_freq/ddr_post_div;
    *ahb_freq = pll_freq/ahb_post_div;       
}

void ar9300_get_wasp_pll_info(struct ath_hal *ah, u_int32_t *cpu_freq, u_int32_t *ddr_freq, u_int32_t *ahb_freq)
{
    /* Radio register */
    #define AR9340_BB_CPU_PLL1            0x161c0
    #define AR9340_BB_CPU_PLL2            0x161c4

    #define AR9340_BB_DDR_PLL1            0x16240
    #define AR9340_BB_DDR_PLL2            0x16244
        
    /* SoC register */
#ifdef _WIN64    
    #define AR9340_SOC_RST_BOOTSTRAP            0xB80600B0ULL
    #define AR9340_SOC_CPU_PLL_CONFIG 	        0xB8050000ULL
    #define AR9340_SOC_DDR_PLL_CONFIG 	        0xB8050004ULL
    #define AR9340_SOC_CPU_DDR_CLOCK_CONTROL    0xB8050008ULL
    #define AR9340_SOC_DDR_PLL_DITHER           0xB8050044ULL
    #define AR9340_SOC_CPU_PLL_DITHER           0xB8050048ULL
#else
    #define AR9340_SOC_RST_BOOTSTRAP            0xB80600B0
    #define AR9340_SOC_CPU_PLL_CONFIG 	        0xB8050000
    #define AR9340_SOC_DDR_PLL_CONFIG 	        0xB8050004
    #define AR9340_SOC_CPU_DDR_CLOCK_CONTROL    0xB8050008
    #define AR9340_SOC_DDR_PLL_DITHER           0xB8050044
    #define AR9340_SOC_CPU_PLL_DITHER           0xB8050048
#endif
    
    u_int32_t reg_soc_bootstrap, reg_soc_cpu_pll_cfg, reg_soc_ddr_pll_cfg, reg_soc_ddr_pll_clk_ctrl;
    u_int32_t reg_soc_ddr_pll_dither, reg_soc_cpu_pll_dither;

    u_int32_t reg_cpu_pll1, reg_cpu_pll2;
    u_int32_t reg_ddr_pll1, reg_ddr_pll2;

    /* AR9340_SOC_RST_BOOTSTRAP */
    u_int32_t xtal;
    
    /* AR9340_SOC_CPU_DDR_CLOCK_CONTROL */
    u_int32_t ahbclk_from_ddrpll, ddrclk_from_ddrpll, cpuclk_from_cpupll;
    u_int32_t cpu_post_div, ddr_post_div, ahb_post_div, cpu_bypass, ddr_bypass, ahb_bypass;
    
    u_int32_t cpu_refdiv, cpu_nint, cpu_nfrac, cpu_outdiv, cpu_local_pll;
    u_int32_t cpu_dither_en, cpu_nfrac_min, cpu_nfrac_max;
    u_int32_t ddr_refdiv, ddr_nint, ddr_nfrac, ddr_outdiv, ddr_local_pll;
    u_int32_t ddr_dither_en, ddr_nfrac_min, ddr_nfrac_max;
    
    //====================================================================
    u_int32_t cpu_vco, cpu_pll_freq;
    u_int32_t cpu_vco_int, cpu_vco_frac, cpu_outdiv_decimal;

    u_int32_t ddr_vco, ddr_pll_freq;
    u_int32_t ddr_vco_int, ddr_vco_frac, ddr_outdiv_decimal;
    //====================================================================

    MyRegisterRead(AR9340_SOC_RST_BOOTSTRAP, &reg_soc_bootstrap);
    MyRegisterRead(AR9340_SOC_CPU_PLL_CONFIG, &reg_soc_cpu_pll_cfg);
    MyRegisterRead(AR9340_SOC_DDR_PLL_CONFIG, &reg_soc_ddr_pll_cfg);
    MyRegisterRead(AR9340_SOC_CPU_DDR_CLOCK_CONTROL, &reg_soc_ddr_pll_clk_ctrl);
    MyRegisterRead(AR9340_SOC_DDR_PLL_DITHER, &reg_soc_ddr_pll_dither);
    MyRegisterRead(AR9340_SOC_CPU_PLL_DITHER, &reg_soc_cpu_pll_dither);

    reg_cpu_pll1 = OS_REG_READ(ah, AR9340_BB_CPU_PLL1);
    reg_cpu_pll2 = OS_REG_READ(ah, AR9340_BB_CPU_PLL2);
    reg_ddr_pll1 = OS_REG_READ(ah, AR9340_BB_DDR_PLL1);
    reg_ddr_pll2 = OS_REG_READ(ah, AR9340_BB_DDR_PLL2);

    /* AR9340_SOC_RST_BOOTSTRAP */
    if (reg_soc_bootstrap & (1 << 4))
        xtal = 40*1000*1000;
    else
        xtal = 25*1000*1000;

    /* AR9340_SOC_CPU_DDR_CLOCK_CONTROL */
    ahbclk_from_ddrpll = (reg_soc_ddr_pll_clk_ctrl >> 24) & 0x1;
    ddrclk_from_ddrpll = (reg_soc_ddr_pll_clk_ctrl >> 21) & 0x1;
    cpuclk_from_cpupll = (reg_soc_ddr_pll_clk_ctrl >> 20) & 0x1;
    cpu_post_div = (reg_soc_ddr_pll_clk_ctrl >> 5) & 0x1f;
    ddr_post_div = (reg_soc_ddr_pll_clk_ctrl >> 10) & 0x1f;
    ahb_post_div = (reg_soc_ddr_pll_clk_ctrl >> 15) & 0x1f;
    cpu_post_div++;
    ddr_post_div++;
    ahb_post_div++;
    cpu_bypass = (reg_soc_ddr_pll_clk_ctrl >> 2) & 0x1;
    ddr_bypass = (reg_soc_ddr_pll_clk_ctrl >> 3) & 0x1;
    ahb_bypass = (reg_soc_ddr_pll_clk_ctrl >> 4) & 0x1;

    cpu_local_pll = (reg_cpu_pll2 >> 30) & 0x1;
    ddr_local_pll = (reg_ddr_pll2 >> 30) & 0x1;
    
    if (cpu_local_pll) {
        cpu_refdiv  = (reg_cpu_pll1 >> 27) & 0x1f;
        cpu_nint    = (reg_cpu_pll1 >> 18) & 0x1ff;
        cpu_nfrac   = reg_cpu_pll1 & 0x3ffff;
        cpu_outdiv  = (reg_cpu_pll2 >> 13) & 0x7;

        cpu_vco_int  = (xtal / cpu_refdiv) * cpu_nint;    
        cpu_vco_frac = (xtal / (cpu_refdiv * 0x40000)) * cpu_nfrac;    
        cpu_vco = cpu_vco_int + cpu_vco_frac;
    }
    else {
        cpu_refdiv  = (reg_soc_cpu_pll_cfg >> 12) & 0x1f;
        cpu_nint    = (reg_soc_cpu_pll_cfg >> 6) & 0x3f;
        cpu_outdiv  = (reg_soc_cpu_pll_cfg >> 19) & 0x7;

        cpu_dither_en = reg_soc_cpu_pll_dither >> 31;        
        cpu_nfrac_min = (reg_soc_cpu_pll_dither >> 6) & 0x3f;
        cpu_nfrac_max = (reg_soc_cpu_pll_dither >> 0) & 0x3f; 
        
        cpu_nfrac_min <<= 12;
        cpu_nfrac_max <<= 12;

        if (cpu_dither_en)
            cpu_nfrac = (cpu_nfrac_max + cpu_nfrac_min)/2;
        else
            cpu_nfrac = cpu_nfrac_min;
        
        cpu_vco_int  = (xtal / cpu_refdiv) * cpu_nint;    
        cpu_vco_frac = (xtal / (cpu_refdiv * 0x40000)) * cpu_nfrac;
        cpu_vco = cpu_vco_int + cpu_vco_frac; 
    }
    
    cpu_outdiv_decimal = pow(2, cpu_outdiv);
    cpu_pll_freq = cpu_vco / cpu_outdiv_decimal;
   
    if (ddr_local_pll) {
        ddr_refdiv  = (reg_ddr_pll1 >> 27) & 0x1f;
        ddr_nint    = (reg_ddr_pll1 >> 18) & 0x1ff;
        ddr_nfrac   = reg_ddr_pll1 & 0x3ffff;
        ddr_outdiv  = (reg_ddr_pll2 >> 13) & 0x7;

        ddr_vco_int  = (xtal / ddr_refdiv) * ddr_nint;    
        ddr_vco_frac = (xtal / (ddr_refdiv * 0x40000)) * ddr_nfrac;    
        ddr_vco = ddr_vco_int + ddr_vco_frac;        
    }
    else {
        ddr_refdiv  = (reg_soc_ddr_pll_cfg >> 16) & 0x1f;
        ddr_nint    = (reg_soc_ddr_pll_cfg >> 10) & 0x3f;
        ddr_outdiv  = (reg_soc_ddr_pll_cfg >> 23) & 0x7;

        ddr_dither_en = reg_soc_ddr_pll_dither >> 31;        
        ddr_nfrac_min = (reg_soc_ddr_pll_dither >> 10) & 0x3ff;  
        ddr_nfrac_max = (reg_soc_ddr_pll_dither >> 0) & 0x3ff;

        ddr_nfrac_min <<= 8;
        ddr_nfrac_max <<= 8;

        if (ddr_dither_en)
            ddr_nfrac = (ddr_nfrac_max + ddr_nfrac_min)/2;
        else
            ddr_nfrac = ddr_nfrac_min;
        
        ddr_vco_int  = (xtal / ddr_refdiv) * ddr_nint;    
        ddr_vco_frac = (xtal / (ddr_refdiv * 0x40000)) * ddr_nfrac;
        ddr_vco = ddr_vco_int + ddr_vco_frac; 
    }
    
    ddr_outdiv_decimal = pow(2, ddr_outdiv);
    ddr_pll_freq = ddr_vco / ddr_outdiv_decimal;
        
    if (cpu_bypass) {
        *cpu_freq = xtal/cpu_post_div;
    }
    else {
        if (cpuclk_from_cpupll)
            *cpu_freq = cpu_pll_freq/cpu_post_div;
        else        
            *cpu_freq = ddr_pll_freq/cpu_post_div;
    }
    
    if (ddr_bypass) {
        *ddr_freq = xtal/ddr_post_div;
    }
    else {
        if (ddrclk_from_ddrpll)
            *ddr_freq = ddr_pll_freq/ddr_post_div;
        else        
            *ddr_freq = cpu_pll_freq/ddr_post_div;
    }
    
    if (ahb_bypass) {
        *ahb_freq = xtal/ahb_post_div;  
    }
    else {
        if (ahbclk_from_ddrpll)
            *ahb_freq = ddr_pll_freq/ahb_post_div;
        else        
            *ahb_freq = cpu_pll_freq/ahb_post_div;        
    }
}

void ar9300_init_otp(struct ath_hal *ah)
{
    #define OTP_PG_STROBE_PW_REG_V				0x15f08
    #define OTP_RD_STROBE_PW_REG_V				0x15f0c
    #define OTP_VDDQ_HOLD_TIME_DELAY			0x15f30
    #define OTP_PGENB_SETUP_HOLD_TIME_DELAY		0x15f34
    #define OTP_STROBE_PULSE_INTERVAL_DELAY		0x15f38
    #define OTP_CSB_ADDR_LOAD_SETUP_HOLD_DELAY	0x15f3c

    u_int32_t cpu_freq=0, ddr_freq=0, ahb_freq=0;
    unsigned long temp_reg=0;
	
	if(AR_SREV_HORNET(ah)) {
        ar9300_get_hornet_pll_info(&cpu_freq, &ddr_freq, &ahb_freq);
    }
    else if(AR_SREV_WASP(ah)) {
        ar9300_get_wasp_pll_info(ah, &cpu_freq, &ddr_freq, &ahb_freq);
    }
    else {
        cpu_freq = 400*1000*1000;
        ddr_freq = 400*1000*1000;
        ahb_freq = 200*1000*1000;
    }
	
    //	printf("CPU %d Hz, DDR %d Hz, AHB %d Hz\n", cpu_freq, ddr_freq, ahb_freq);
	
    temp_reg = ahb_freq * pow(10,-9) * 5000 * 10;
    if(fmod(temp_reg, 10) != 0)
        temp_reg = (temp_reg / 10) + 1;
    else
        temp_reg = temp_reg / 10;

    if(AR_SREV_HORNET(ah)) {
        //	printf("reg %x = %x\n", OTP_PG_STROBE_PW_REG_V, temp_reg);
        OS_REG_WRITE(ah, OTP_PG_STROBE_PW_REG_V, (u_int32_t)temp_reg);   //it need 5000ns for TSMC55nm
    }
    else if(AR_SREV_WASP(ah)) {
        //	printf("reg %x = %x\n", OTP_PG_STROBE_PW_REG_V_WASP, temp_reg);
        //OS_REG_WRITE(ah, OTP_PG_STROBE_PW_REG_V_WASP, 1250);//5000ns @55nm, and assume AHB=250 MHz,
        OS_REG_WRITE(ah, OTP_PG_STROBE_PW_REG_V_WASP, (u_int32_t)temp_reg);  //it need 5000ns for TSMC55nm
    }
    
    temp_reg = ahb_freq * pow(10,-9) * 35 * 10;
    if(fmod(temp_reg, 10) != 0)
        temp_reg = (temp_reg / 10) + 1;
    else
    temp_reg = temp_reg / 10;

    if(AR_SREV_HORNET(ah)) {	
        //	printf("reg %x = %x\n", OTP_RD_STROBE_PW_REG_V, temp_reg);
        OS_REG_WRITE(ah, OTP_RD_STROBE_PW_REG_V, (u_int32_t)temp_reg - 1);   //it need 35ns for TSMC55nm
    }
    else if(AR_SREV_WASP(ah)) {
        //	printf("reg %x = %x\n", OTP_RD_STROBE_PW_REG_V_WASP, temp_reg);
        //OS_REG_WRITE(ah, OTP_RD_STROBE_PW_REG_V_WASP, 9);// 35ns @55nm
        OS_REG_WRITE(ah, OTP_RD_STROBE_PW_REG_V_WASP, (u_int32_t)temp_reg - 1);  //it need 35ns for TSMC55nm        
    }
    
    temp_reg = ahb_freq * pow(10,-9) * 15 * 10;
    if(fmod(temp_reg, 10) != 0)
        temp_reg = (temp_reg / 10) + 1;
    else
        temp_reg = temp_reg / 10;
        
    if(AR_SREV_HORNET(ah)) {
        //	printf("reg %x = %x\n", OTP_VDDQ_HOLD_TIME_DELAY, temp_reg);
        OS_REG_WRITE(ah, OTP_VDDQ_HOLD_TIME_DELAY, (u_int32_t)temp_reg - 1); //it need 15ns for TSMC55nm
    }
    else if(AR_SREV_WASP(ah)) {
        //	printf("reg %x = %x\n", OTP_VDDQ_HOLD_TIME_DELAY_WASP, temp_reg);
        //OS_REG_WRITE(ah, OTP_VDDQ_HOLD_TIME_DELAY_WASP, 4); //15ns @55nm
        OS_REG_WRITE(ah, OTP_VDDQ_HOLD_TIME_DELAY_WASP, (u_int32_t)temp_reg - 1);    //it need 15ns for TSMC55nm  
    }
    
    temp_reg = ahb_freq * pow(10,-9) * 21.2 * 10;
    if(fmod(temp_reg, 10) != 0)
        temp_reg = (temp_reg / 10) + 1;
    else
        temp_reg = temp_reg / 10;

    if(AR_SREV_HORNET(ah)) {        
        //	printf("reg %x = %x\n", OTP_PGENB_SETUP_HOLD_TIME_DELAY, temp_reg);
        OS_REG_WRITE(ah, OTP_PGENB_SETUP_HOLD_TIME_DELAY, (u_int32_t)temp_reg - 1);	//it need 21.2ns for TSMC55nm
        OS_REG_WRITE(ah, OTP_STROBE_PULSE_INTERVAL_DELAY, 0x0);	//it need 0 for TSMC55nm
    }
    else if(AR_SREV_WASP(ah)) {
        //	printf("reg %x = %x\n", OTP_PGENB_SETUP_HOLD_TIME_DELAY_WASP, temp_reg);
        //OS_REG_WRITE(ah, OTP_PGENB_SETUP_HOLD_TIME_DELAY_WASP, 6); //21.2ns @55nm
        OS_REG_WRITE(ah, OTP_PGENB_SETUP_HOLD_TIME_DELAY_WASP, (u_int32_t)temp_reg - 1);	//it need 21.2ns for TSMC55nm
        //OS_REG_WRITE(ah, OTP_STROBE_PULSE_INTERVAL_DELAY_WASP, 0x0); // 0
        OS_REG_WRITE(ah, OTP_STROBE_PULSE_INTERVAL_DELAY_WASP, 0x0);	//it need 0 for TSMC55nm
    }
    
    temp_reg = ahb_freq * pow(10,-9) * 6.8 * 10;
    if(fmod(temp_reg, 10) != 0)
        temp_reg = (temp_reg / 10) + 1;
    else
        temp_reg = temp_reg / 10;

    if(AR_SREV_HORNET(ah)) { 
        //	printf("reg %x = %x\n", OTP_CSB_ADDR_LOAD_SETUP_HOLD_DELAY, temp_reg);
        OS_REG_WRITE(ah, OTP_CSB_ADDR_LOAD_SETUP_HOLD_DELAY, (u_int32_t)temp_reg - 1);	//it need 6.8ns for TSMC55nm
    }
    else if(AR_SREV_WASP(ah)) {
        //	printf("reg %x = %x\n", OTP_CSB_ADDR_LOAD_SETUP_HOLD_DELAY_WASP, temp_reg);
        //OS_REG_WRITE(ah, OTP_CSB_ADDR_LOAD_SETUP_HOLD_DELAY_WASP, 0x2); // 6.8ns@TSMC55nm
        OS_REG_WRITE(ah, OTP_CSB_ADDR_LOAD_SETUP_HOLD_DELAY_WASP, (u_int32_t)temp_reg - 1);	//it need 6.8ns for TSMC55nm        
    }
}

#endif



HAL_BOOL
ar9300InterferenceIsPresent(struct ath_hal *ah)
{
    int i;
    struct ath_hal_private  *ahpriv = AH_PRIVATE(ah);

    /* This function is called after a stuck beacon, if EACS is enabled. If CW interference is severe, then 
     * HW goes into a loop of continuous stuck beacons and resets. On reset the NF cal history is cleared.
     * So the median value of the history cannot be used - hence check if any value (Chain 0/Primary Channel)
     * is outside the bounds.
     */
    HAL_NFCAL_HIST_FULL *h = AH_HOME_CHAN_NFCAL_HIST(ah);
    for (i = 0; i < HAL_NF_CAL_HIST_LEN_FULL; i++) {
        if (h->nfCalBuffer[i][0] > ahpriv->nfp->nominal + ahpriv->nf_cw_int_delta) {
    		return AH_TRUE;
        }

    }
    return AH_FALSE;
}
#endif /* AH_SUPPORT_AR9300 */
