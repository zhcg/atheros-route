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
#include "ah_desc.h"
#include "ah_internal.h"

#include "ar9300/ar9300phy.h"
#include "ar9300/ar9300.h"
#include "ar9300/ar9300reg.h"
#include "ar9300/ar9300desc.h"

/*
 * Default 9300 spectral scan parameters
 */
#define AR9300_SPECTRAL_SCAN_ENA                0
#define AR9300_SPECTRAL_SCAN_ACTIVE             0
#define AR9300_SPECTRAL_SCAN_FFT_PERIOD         8
#define AR9300_SPECTRAL_SCAN_PERIOD             1
#define AR9300_SPECTRAL_SCAN_COUNT              16 //used to be 128
#define AR9300_SPECTRAL_SCAN_SHORT_REPEAT       1

/* constants */
#define MAX_RADAR_DC_PWR_THRESH 127
#define MAX_RADAR_RSSI_THRESH 0x3f
#define MAX_RADAR_HEIGHT 0x3f
#define MAX_CCA_THRESH 127
#define ENABLE_ALL_PHYERR 0xffffffff

void ar9300DisableCCK(struct ath_hal *ah);
void ar9300DisableRadar(struct ath_hal *ah);
void ar9300DisableRestart(struct ath_hal *ah);
void ar9300SetRadarDcThresh(struct ath_hal *ah);
void ar9300DisableWeakSignal(struct ath_hal *ah);
void ar9300DisableStrongSignal(struct ath_hal *ah);
void ar9300PrepSpectralScan(struct ath_hal *ah);

void
ar9300DisableCCK(struct ath_hal *ah)
{
    u_int32_t val;

    val = OS_REG_READ(ah, AR_PHY_MODE);
    val &= ~(AR_PHY_MODE_DYN_CCK_DISABLE);

    OS_REG_WRITE(ah, AR_PHY_MODE, val);
}
void
ar9300DisableRadar(struct ath_hal *ah)
{
    u_int32_t val;

    // Enable radar FFT
    val = OS_REG_READ(ah, AR_PHY_RADAR_0);
    val |= AR_PHY_RADAR_0_FFT_ENA;

    // set radar detect thresholds to max to effectively disable radar
    val &= ~AR_PHY_RADAR_0_RRSSI;
    val |= SM(MAX_RADAR_RSSI_THRESH, AR_PHY_RADAR_0_RRSSI);

    val &= ~AR_PHY_RADAR_0_HEIGHT;
    val |= SM(MAX_RADAR_HEIGHT, AR_PHY_RADAR_0_HEIGHT);

    val &= ~(AR_PHY_RADAR_0_ENA);
    OS_REG_WRITE(ah, AR_PHY_RADAR_0, val);

    // disable extension radar detect
    val = OS_REG_READ(ah, AR_PHY_RADAR_EXT);
    OS_REG_WRITE(ah, AR_PHY_RADAR_EXT, val & ~AR_PHY_RADAR_EXT_ENA);

    val = OS_REG_READ(ah, AR_RX_FILTER);
    val |= (1 << 13);
    OS_REG_WRITE(ah, AR_RX_FILTER, val);
}

void ar9300DisableRestart(struct ath_hal *ah)
{
    u_int32_t val;
    val = OS_REG_READ(ah, AR_PHY_RESTART);
    val &= ~AR_PHY_RESTART_ENA;
    OS_REG_WRITE(ah, AR_PHY_RESTART, val);

    val = OS_REG_READ(ah, AR_PHY_RESTART);
}

void ar9300SetRadarDcThresh(struct ath_hal *ah)
{
    u_int32_t val;
    val = OS_REG_READ(ah, AR_PHY_RADAR_EXT);
    val &= ~AR_PHY_RADAR_DC_PWR_THRESH;
    val |= SM(MAX_RADAR_DC_PWR_THRESH, AR_PHY_RADAR_DC_PWR_THRESH);
    OS_REG_WRITE(ah, AR_PHY_RADAR_EXT, val);

    val = OS_REG_READ(ah, AR_PHY_RADAR_EXT);
}

void
ar9300DisableWeakSignal(struct ath_hal *ah)
{
    // set firpwr to max (signed)
    OS_REG_RMW_FIELD(ah, AR_PHY_FIND_SIG, AR_PHY_FIND_SIG_FIRPWR, 0x7f);
    OS_REG_CLR_BIT(ah, AR_PHY_FIND_SIG, AR_PHY_FIND_SIG_FIRPWR_SIGN_BIT);

    // set firstep to max
    OS_REG_RMW_FIELD(ah, AR_PHY_FIND_SIG, AR_PHY_FIND_SIG_FIRSTEP, 0x3f);

    // set relpwr to max (signed)
    OS_REG_RMW_FIELD(ah, AR_PHY_FIND_SIG, AR_PHY_FIND_SIG_RELPWR, 0x1f);
    OS_REG_CLR_BIT(ah, AR_PHY_FIND_SIG, AR_PHY_FIND_SIG_RELPWR_SIGN_BIT);

    // set relstep to max (signed)
    OS_REG_RMW_FIELD(ah, AR_PHY_FIND_SIG, AR_PHY_FIND_SIG_RELSTEP, 0x1f);
    OS_REG_CLR_BIT(ah, AR_PHY_FIND_SIG, AR_PHY_FIND_SIG_RELSTEP_SIGN_BIT);

    // set firpwr_low to max (signed)
    OS_REG_RMW_FIELD(ah, AR_PHY_FIND_SIG_LOW, AR_PHY_FIND_SIG_LOW_FIRPWR, 0x7f);
    OS_REG_CLR_BIT(
        ah, AR_PHY_FIND_SIG_LOW, AR_PHY_FIND_SIG_LOW_FIRPWR_SIGN_BIT);

    // set firstep_low to max
    OS_REG_RMW_FIELD(
        ah, AR_PHY_FIND_SIG_LOW, AR_PHY_FIND_SIG_LOW_FIRSTEP_LOW, 0x3f);

    // set relstep_low to max (signed)
    OS_REG_RMW_FIELD(
        ah, AR_PHY_FIND_SIG_LOW, AR_PHY_FIND_SIG_LOW_RELSTEP, 0x1f);
    OS_REG_CLR_BIT(
        ah, AR_PHY_FIND_SIG_LOW, AR_PHY_FIND_SIG_LOW_RELSTEP_SIGN_BIT);
}

void
ar9300DisableStrongSignal(struct ath_hal *ah)
{
    u_int32_t val;

    val = OS_REG_READ(ah, AR_PHY_TIMING5);
    val |= AR_PHY_TIMING5_RSSI_THR1A_ENA;
    OS_REG_WRITE(ah, AR_PHY_TIMING5, val);

    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING5, AR_PHY_TIMING5_RSSI_THR1A, 0x7f);

}
void
ar9300SetCcaThreshold(struct ath_hal *ah, u_int8_t thresh62)
{
    OS_REG_RMW_FIELD(ah, AR_PHY_CCA_0, AR_PHY_CCA_THRESH62, thresh62);
    OS_REG_RMW_FIELD(ah, AR_PHY_EXT_CCA0, AR_PHY_EXT_CCA0_THRESH62, thresh62);
    //OS_REG_RMW_FIELD(ah, AR_PHY_EXTCHN_PWRTHR1, AR_PHY_EXT_CCA0_THRESH62, thresh62);
    OS_REG_RMW_FIELD(ah, AR_PHY_EXT_CCA, AR_PHY_EXT_CCA_THRESH62, thresh62);
}

/*
 * ar9300SpectralScanCapable
 * -------------------------
 *  Returns the Spectral scan capability
 */
u_int32_t ar9300SpectralScanCapable(struct ath_hal *ah)
{
    return AH_TRUE;
}

void ar9300ClassifyStrongBins(struct ath_hal *ah)
{
    OS_REG_RMW_FIELD(ah, AR_PHY_RADAR_1, AR_PHY_RADAR_1_CF_BIN_THRESH, 0x1);
}

void ar9300DisableDcOffset(struct ath_hal *ah)
{
    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING2, AR_PHY_TIMING2_DC_OFFSET, 0);
}

void ar9300EnableCckDetect(struct ath_hal *ah)
{
    OS_REG_RMW_FIELD(ah, AR_PHY_MODE, AR_PHY_MODE_DISABLE_CCK, 0);
    HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: AR_PHY_MODE=0x%x\n", __func__, OS_REG_READ(ah, AR_PHY_MODE));
  
}

void ar9300PrepSpectralScan(struct ath_hal *ah)
{
    ar9300DisableRadar(ah);
    ar9300ClassifyStrongBins(ah);
    ar9300DisableDcOffset(ah);

    if (AR_SREV_AR9580_10_OR_LATER(ah)) {       /* Peacock+ */
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: chip is AR9580 or later\n", __func__);
    } else {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: chip is before AR9580\n", __func__);
    }

    /* The following addresses Osprey bug and is not required for Peacock+ */
    if (AH_PRIVATE(ah)->ah_curchan &&
        IS_5GHZ_FAST_CLOCK_EN(ah, AH_PRIVATE(ah)->ah_curchan) &&
        (!AR_SREV_AR9580_10_OR_LATER(ah))) {       /* fast clock  and earlier than peacock */
        ar9300EnableCckDetect(ah);
    }
#ifdef DEMO_MODE
    ar9300DisableStrongSignal(ah);
    ar9300DisableWeakSignal(ah);
    ar9300SetRadarDcThresh(ah);
    ar9300SetCcaThreshold(ah, MAX_CCA_THRESH);
    //ar9300DisableRestart(ah);
#endif
   OS_REG_WRITE(ah, AR_PHY_ERR, HAL_PHYERR_SPECTRAL);
}

/*--ar9300ConfigureFftPeriod--
 * Sets the FFT period inVal 
 */
void ar9300ConfigFftPeriod(struct ath_hal *ah, u_int32_t inVal)
{
    u_int32_t val;
    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
    val &= ~AR_PHY_SPECTRAL_SCAN_FFT_PERIOD;
    val |= SM(inVal , AR_PHY_SPECTRAL_SCAN_FFT_PERIOD);
    OS_REG_WRITE(ah, AR_PHY_SPECTRAL_SCAN, val );
}

/*--ar9300ConfigureScanPeriod--
 * Sets the scan period inVal
 */
void ar9300ConfigScanPeriod(struct ath_hal *ah, u_int32_t inVal)
{
    u_int32_t val;
    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
    val &= ~AR_PHY_SPECTRAL_SCAN_PERIOD;
    val |= SM(inVal, AR_PHY_SPECTRAL_SCAN_PERIOD);
    OS_REG_WRITE(ah, AR_PHY_SPECTRAL_SCAN, val );
}


/*--ar9300ConfigureScanCount--
 * Sets the scan count to inVal 
 */
void ar9300ConfigScanCount(struct ath_hal *ah, u_int32_t inVal)
{
    u_int32_t val;
    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
    val &= ~AR_PHY_SPECTRAL_SCAN_COUNT;
    val |= SM(inVal, AR_PHY_SPECTRAL_SCAN_COUNT);
    OS_REG_WRITE(ah, AR_PHY_SPECTRAL_SCAN, val );
}

/*--ar9300ConfigureShortRpt--
 * Sets/resets the short report bit based on inVal 
 */
void ar9300ConfigShortRpt(struct ath_hal *ah, u_int32_t inVal)
{
    u_int32_t val;
    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
    if (inVal) {
        val |= AR_PHY_SPECTRAL_SCAN_SHORT_REPEAT;
    } else  {
        val &= ~(AR_PHY_SPECTRAL_SCAN_SHORT_REPEAT);
    }
    OS_REG_WRITE(ah, AR_PHY_SPECTRAL_SCAN, val );
}

void
ar9300ConfigureSpectralScan(struct ath_hal *ah, HAL_SPECTRAL_PARAM *ss)
{
    u_int32_t val;
    struct ath_hal_9300 *ahp = AH9300(ah);
    HAL_POWER_MODE pwrmode = ahp->ah_powerMode;

    if (AR_SREV_WASP(ah) && (pwrmode == HAL_PM_FULL_SLEEP)) {
        ar9300SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE);
    }

    ar9300PrepSpectralScan(ah);

    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);

    if (ss->ss_fft_period != HAL_SPECTRAL_PARAM_NOVAL) {
        val &= ~AR_PHY_SPECTRAL_SCAN_FFT_PERIOD;
        val |= SM(ss->ss_fft_period, AR_PHY_SPECTRAL_SCAN_FFT_PERIOD);
    }

    if (ss->ss_period != HAL_SPECTRAL_PARAM_NOVAL) {
        val &= ~AR_PHY_SPECTRAL_SCAN_PERIOD;
        val |= SM(ss->ss_period, AR_PHY_SPECTRAL_SCAN_PERIOD);
    }

    if (ss->ss_count != HAL_SPECTRAL_PARAM_NOVAL) {
        val &= ~AR_PHY_SPECTRAL_SCAN_COUNT;
        /* Remnants of a Merlin bug, 128 translates to 0 for
         * continuous scanning. Instead we do piecemeal captures
         * of 64 samples for Osprey.
         */
        if (ss->ss_count == 128)
            val |= SM(0, AR_PHY_SPECTRAL_SCAN_COUNT);
        else
            val |= SM(ss->ss_count, AR_PHY_SPECTRAL_SCAN_COUNT);
    }

    if (ss->ss_period != HAL_SPECTRAL_PARAM_NOVAL) {
        val &= ~AR_PHY_SPECTRAL_SCAN_PERIOD;
        val |= SM(ss->ss_period, AR_PHY_SPECTRAL_SCAN_PERIOD);
    }

    if (ss->ss_short_report == AH_TRUE) {
        val |= AR_PHY_SPECTRAL_SCAN_SHORT_REPEAT;
    } else {
        val &= ~AR_PHY_SPECTRAL_SCAN_SHORT_REPEAT;
    }

    // Enable spectral scan
    OS_REG_WRITE(ah, AR_PHY_SPECTRAL_SCAN, val | AR_PHY_SPECTRAL_SCAN_ENABLE);

    ar9300GetSpectralParams(ah, ss);

    if (AR_SREV_WASP(ah) && (pwrmode == HAL_PM_FULL_SLEEP)) {
        ar9300SetPowerMode(ah, HAL_PM_FULL_SLEEP, AH_TRUE);
    }
}

/*
 * Get the spectral parameter values and return them in the pe
 * structure
 */

void
ar9300GetSpectralParams(struct ath_hal *ah, HAL_SPECTRAL_PARAM *ss)
{
    u_int32_t val;
    struct ath_hal_9300 *ahp = AH9300(ah);
    HAL_POWER_MODE pwrmode = ahp->ah_powerMode;

    if (AR_SREV_WASP(ah) && (pwrmode == HAL_PM_FULL_SLEEP)) {
        ar9300SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE);
    }

    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);

    ss->ss_fft_period = MS(val, AR_PHY_SPECTRAL_SCAN_FFT_PERIOD);
    ss->ss_period = MS(val, AR_PHY_SPECTRAL_SCAN_PERIOD);
    ss->ss_count = MS(val, AR_PHY_SPECTRAL_SCAN_COUNT);
    ss->ss_short_report = MS(val, AR_PHY_SPECTRAL_SCAN_SHORT_REPEAT);

    if (AR_SREV_WASP(ah) && (pwrmode == HAL_PM_FULL_SLEEP)) {
        ar9300SetPowerMode(ah, HAL_PM_FULL_SLEEP, AH_TRUE);
    }
}

HAL_BOOL
ar9300IsSpectralActive(struct ath_hal *ah)
{
    u_int32_t val;

    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
    return MS(val, AR_PHY_SPECTRAL_SCAN_ACTIVE);
}

HAL_BOOL
ar9300IsSpectralEnabled(struct ath_hal *ah)
{
    u_int32_t val;

    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
    return MS(val,AR_PHY_SPECTRAL_SCAN_ENABLE);
}

void ar9300StartSpectralScan(struct ath_hal *ah)
{
    u_int32_t val;
    struct ath_hal_9300 *ahp = AH9300(ah);
    HAL_POWER_MODE pwrmode = ahp->ah_powerMode;

    if (AR_SREV_WASP(ah) && (pwrmode == HAL_PM_FULL_SLEEP)) {
        ar9300SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE);
    }

    ar9300PrepSpectralScan(ah);

    // Activate spectral scan
    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);

    /* This is a hardware bug fix, the enable and active bits should 
     * not be set/reset in the same write operation to the register 
     */ 
    if (!(val & AR_PHY_SPECTRAL_SCAN_ENABLE)) {
        val |= AR_PHY_SPECTRAL_SCAN_ENABLE;
        OS_REG_WRITE(ah, AR_PHY_SPECTRAL_SCAN, val);
        val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
    }
    val |= AR_PHY_SPECTRAL_SCAN_ACTIVE;
    OS_REG_WRITE(ah, AR_PHY_SPECTRAL_SCAN, val);

    val = OS_REG_READ(ah, AR_PHY_ERR_MASK_REG);
    OS_REG_WRITE(ah, AR_PHY_ERR_MASK_REG, val | AR_PHY_ERR_RADAR);

    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);

    if (AR_SREV_WASP(ah) && (pwrmode == HAL_PM_FULL_SLEEP)) {
        ar9300SetPowerMode(ah, HAL_PM_FULL_SLEEP, AH_TRUE);
    }
    HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: AR_PHY_MODE=0x%x\n", __func__, OS_REG_READ(ah, AR_PHY_MODE));
}

void ar9300StopSpectralScan(struct ath_hal *ah)
{
    u_int32_t val;
    struct ath_hal_9300 *ahp = AH9300(ah);
    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);

    // Deactivate spectral scan
    // HW Bug fix -- Do not disable the spectral scan
    // only turn off the active bit
    //val &= ~AR_PHY_SPECTRAL_SCAN_ENABLE;
    val &= ~AR_PHY_SPECTRAL_SCAN_ACTIVE;
    OS_REG_WRITE(ah, AR_PHY_SPECTRAL_SCAN, val);
    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);

    OS_REG_RMW_FIELD(ah, AR_PHY_RADAR_1, AR_PHY_RADAR_1_CF_BIN_THRESH,
                     ahp->ah_radar1);
    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING2, AR_PHY_TIMING2_DC_OFFSET,
                     ahp->ah_dc_offset);
    OS_REG_WRITE(ah, AR_PHY_ERR, 0);

    if (AH_PRIVATE(ah)->ah_curchan &&
        IS_5GHZ_FAST_CLOCK_EN(ah, AH_PRIVATE(ah)->ah_curchan)) { /* fast clock */
        OS_REG_RMW_FIELD(ah, AR_PHY_MODE, AR_PHY_MODE_DISABLE_CCK,
                         ahp->ah_disable_cck);
    }

    HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: AR_PHY_MODE=0x%x\n", __func__, OS_REG_READ(ah, AR_PHY_MODE));

    val = OS_REG_READ(ah, AR_PHY_ERR_MASK_REG) & (~AR_PHY_ERR_RADAR);
    OS_REG_WRITE(ah, AR_PHY_ERR_MASK_REG, val);

    val = OS_REG_READ(ah, AR_PHY_ERR);
}

u_int32_t ar9300GetSpectralConfig(struct ath_hal *ah)
{
    u_int32_t val;
    struct ath_hal_9300 *ahp = AH9300(ah);
    HAL_POWER_MODE pwrmode = ahp->ah_powerMode;

    if (AR_SREV_WASP(ah) && (pwrmode == HAL_PM_FULL_SLEEP)) {
        ar9300SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE);
    }

    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);

    if (AR_SREV_WASP(ah) && (pwrmode == HAL_PM_FULL_SLEEP)) {
        ar9300SetPowerMode(ah, HAL_PM_FULL_SLEEP, AH_TRUE);
    }
    return val;
}

int16_t ar9300GetCtlChanNF(struct ath_hal *ah)
{
    int16_t nf;
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);

    if ( (OS_REG_READ(ah, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) == 0) {
        /* Noise floor calibration value is ready */
        nf = MS(OS_REG_READ(ah, AR_PHY_CCA_0), AR_PHY_MINCCA_PWR);
    } else {
        /* NF calibration is not done, return nominal value */
        nf = ahpriv->nfp->nominal;    
    }
    if (nf & 0x100)
        nf = (0 - ((nf ^ 0x1ff) + 1));
    return nf;
}

int16_t ar9300GetExtChanNF(struct ath_hal *ah)
{
    int16_t nf;
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);

    if ((OS_REG_READ(ah, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) == 0) {
        /* Noise floor calibration value is ready */
        nf = MS(OS_REG_READ(ah, AR_PHY_EXT_CCA), AR_PHY_EXT_MINCCA_PWR);
    } else {
        /* NF calibration is not done, return nominal value */
        nf = ahpriv->nfp->nominal;    
    }
    if (nf & 0x100)
        nf = (0 - ((nf ^ 0x1ff) + 1));
    return nf;
}

#endif
