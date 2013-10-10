#include "opt_ah.h"
#include "ah.h"
#include "ar9300.h"
#include "ah_internal.h"
#include "ar9300papd.h"
#include "ar9300reg.h"


#if ATH_SUPPORT_PAPRD

static struct ar9300_paprd_pwr_adjust ar9300_paprd_pwr_adj_array[] = {
/*   rate index     ,   register offset   ,  mask of register               , */
  {ALL_TARGET_HT20_5, AR_PHY_POWERTX_RATE5, AR_PHY_POWERTX_RATE5_POWERTXHT20_3,
/* mask offset of register             ,  offset  dB*/
   AR_PHY_POWERTX_RATE5_POWERTXHT20_3_S,      1},
  {ALL_TARGET_HT20_6, AR_PHY_POWERTX_RATE6, AR_PHY_POWERTX_RATE6_POWERTXHT20_4,
   AR_PHY_POWERTX_RATE6_POWERTXHT20_4_S,      2},
  {ALL_TARGET_HT20_7, AR_PHY_POWERTX_RATE6, AR_PHY_POWERTX_RATE6_POWERTXHT20_5,
   AR_PHY_POWERTX_RATE6_POWERTXHT20_5_S,      2},
  {ALL_TARGET_HT40_5, AR_PHY_POWERTX_RATE7, AR_PHY_POWERTX_RATE7_POWERTXHT40_3,
   AR_PHY_POWERTX_RATE7_POWERTXHT40_3_S,      1},
  {ALL_TARGET_HT40_6, AR_PHY_POWERTX_RATE8, AR_PHY_POWERTX_RATE8_POWERTXHT40_4,
   AR_PHY_POWERTX_RATE8_POWERTXHT40_4_S,      2},
  {ALL_TARGET_HT40_7, AR_PHY_POWERTX_RATE8, AR_PHY_POWERTX_RATE8_POWERTXHT40_5,
   AR_PHY_POWERTX_RATE8_POWERTXHT40_5_S,      2},
  {ALL_TARGET_LEGACY_54,AR_PHY_POWERTX_RATE2,AR_PHY_POWERTX_RATE2_POWERTX54M_7,
   AR_PHY_POWERTX_RATE2_POWERTX54M_7_S,       2},
};
//static u_int32_t training_power = 0;
unsigned int ar9300GetDesiredGainForChain(struct ath_hal *ah, int chainNum,
    int target_power);
void ar9300TxForceGain(struct ath_hal *ah, unsigned int gain_index);
int ar9300CreatePACurve(struct ath_hal *ah, u_int32_t * PA_table,
    u_int32_t * smallSignalGain);
HAL_BOOL create_PA_curve(u_int32_t * PaprdTrainDataL,
    u_int32_t * PaprdTrainDataU, u_int32_t * PA_table, u_int32_t * G_fxp_ext,
    int * PA_in);
void ar9300GainTableEntries(struct ath_hal *ah);
int ar9300PAPDSetupSingleTable(struct ath_hal *ah);

#define AR9300_IS_CHAN(_c,_f)       (((_c)->channelFlags & _f) || 0)

static int Ar9300PAPDSetupSingleTable(struct ath_hal *ah, HAL_CHANNEL * chan)
{
    int is2G = AR9300_IS_CHAN(chan, CHANNEL_2GHZ);
    struct ath_hal_9300 *ahp = AH9300(ah);
    int isHT40 = 0;
    u_int32_t am_mask = 0;
    u_int32_t val = OS_REG_READ(ah, AR_2040_MODE);
    u_int8_t targetPowerValT2[ar9300RateSize];
    int      powerTblindex = 0, powerDelta = 0;
    int      paprdScaleFactor = 5;

    const u_int8_t mask2num[8] = {
        0 /* 000 */,
        1 /* 001 */,
        1 /* 010 */,
        2 /* 011 */,
        1 /* 100 */,
        2 /* 101 */,
        2 /* 110 */,
        3 /* 111 */
    };

	ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;

#define ABS(_x, _y) ((int)_x > (int)_y ? (int)_x - (int)_y : (int)_y - (int)_x)

    ar9300SetTargetPowerFromEeprom(ah, chan->channel, targetPowerValT2);
    if (val & HAL_HT_MACMODE_2040) {
        isHT40 = 1;
    }

	// Note on paprdScaleFactor
	// This factor is saved in eeprom as 3 bit fields in following fashion.
	// In 5G there are 3 scale factors -- upper, mid and lower band.
	// Upper band scale factor is coded in bits 25-27 of modalHeader5G.papdRateMaskHt20.
	// Mid band scale factor is coded in bits 28-30 of modalHeader5G.papdRateMaskHt40.
	// Lower band scale factor is coded in bits 25-27 of modalHeader5G.papdRateMaskHt40.
	// For 2G there is only one scale factor. It is saved in bits 25-27 of modalHeader2G.papdRateMaskHt20.
    AH_PAPRD_GET_SCALE_FACTOR(paprdScaleFactor, eep, is2G, chan->channel);
    if (is2G) {
        if (isHT40) {
            am_mask = ahp->ah_2G_papdRateMaskHt40 & AH_PAPRD_AM_PM_MASK;
            powerTblindex = ALL_TARGET_HT40_0_8_16;
        } else {
            am_mask = ahp->ah_2G_papdRateMaskHt20 & AH_PAPRD_AM_PM_MASK;
            powerTblindex = ALL_TARGET_HT20_0_8_16;
        }
        if(AR_SREV_HORNET(ah) || AR_SREV_WASP(ah)) {
            if (isHT40) {
                ahp->paprd_training_power =
                    targetPowerValT2[ALL_TARGET_HT40_7] + 2;
            } else {
                ahp->paprd_training_power =
                    targetPowerValT2[ALL_TARGET_HT20_7] + 2;
            }
        } else if (AR_SREV_POSEIDON(ah)) {
            ahp->paprd_training_power = 25;
        } else {
#ifdef ART_BUILD
                ahp->paprd_training_power = 
                    targetPowerValT2[powerTblindex]-4;
#else
            ahp->paprd_training_power =
                OS_REG_READ_FIELD_ALT(ah, AR_PHY_POWERTX_RATE5,
                AR_PHY_POWERTX_RATE5_POWERTXHT20_0);
            if (ABS(targetPowerValT2[powerTblindex], ahp->paprd_training_power) >  paprdScaleFactor)
            {
                HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s[%d]: Chan %d paprd failing EEP PWR 0x%08x TGT PWR 0x%08x\n",
                    __func__, __LINE__, chan->channel, targetPowerValT2[powerTblindex], ahp->paprd_training_power);
                goto FAILED;
            }
            powerDelta =  ABS(ahp->paprd_training_power, targetPowerValT2[powerTblindex]);

            powerDelta = powerDelta > 4 ? 0 : 4 - powerDelta;
            ahp->paprd_training_power = ahp->paprd_training_power - powerDelta;
#endif
        }
    } else {
		//

        if (isHT40) {
		    ahp->paprd_training_power =
			    OS_REG_READ_FIELD_ALT(ah, AR_PHY_POWERTX_RATE8,
				AR_PHY_POWERTX_RATE8_POWERTXHT40_5);
            am_mask = ahp->ah_5G_papdRateMaskHt40 & AH_PAPRD_AM_PM_MASK;
			switch(mask2num[ahp->ah_txchainmask])
            {
                case 1:
					powerDelta = 6;
                    break;
                case 2:
					powerDelta = 4;
                    break;
                case 3:
					powerDelta = 2;
                    break;
                default:
                    goto FAILED;
                break;
            }
            powerTblindex = ALL_TARGET_HT40_7;
        } else {
		    ahp->paprd_training_power =
			    OS_REG_READ_FIELD_ALT(ah, AR_PHY_POWERTX_RATE6,
				AR_PHY_POWERTX_RATE6_POWERTXHT20_5);
            am_mask = ahp->ah_5G_papdRateMaskHt20 & AH_PAPRD_AM_PM_MASK;
			switch(mask2num[ahp->ah_txchainmask])
            {
                case 1:
					powerDelta = 6;
                    break;
                case 2:
					powerDelta = 4;
                    break;
                case 3:
					powerDelta = 2;
                    break;
                default:
                    goto FAILED;
                break;
            }
            powerTblindex = ALL_TARGET_HT20_7;
        }
#ifndef ART_BUILD
   ahp->paprd_training_power = ahp->paprd_training_power + paprdScaleFactor; // Adjust for scale factor
		//ath_hal_printf(ah, "%s[%d] paprdScaleFactor %d powerDelta %d\n", __func__, __LINE__, paprdScaleFactor, powerDelta);
	if (ABS(targetPowerValT2[powerTblindex], ahp->paprd_training_power) >  paprdScaleFactor)
        {
            HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s[%d]: Chan %d paprd failing EEP PWR 0x%08x TGT PWR 0x%08x\n",
                __func__, __LINE__, chan->channel, targetPowerValT2[powerTblindex], ahp->paprd_training_power);
            goto FAILED;
        }
#else
		ahp->paprd_training_power = targetPowerValT2[powerTblindex];
#endif
		ahp->paprd_training_power = ahp->paprd_training_power + powerDelta;
	}

    HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s 2G %d HT40 %d am_mask 0x%08x\n",
        __func__, is2G, isHT40, am_mask);
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_AM2AM, AR_PHY_PAPRD_AM2AM_MASK,
        am_mask);
    if(AR_SREV_HORNET(ah)) {
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_AM2PM, AR_PHY_PAPRD_AM2PM_MASK,
            0);//am_mask
    }
    else {
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_AM2PM, AR_PHY_PAPRD_AM2PM_MASK,
            am_mask);
    }

    //FieldWrite("BB_paprd_ht40_mask.paprd_ht40_mask", 0x1ffffff);
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_HT40, AR_PHY_PAPRD_HT40_MASK,
        AR_PHY_PAPRD_HT40_MASK);
    // chain0
    if (AH9300(ah)->ah_txchainmask & AR9300_CHAIN0_MASK) {
        //FieldWrite("BB_paprd_ctrl0_b0.paprd_adaptive_use_single_table_0", 1);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B0,
            AR_PHY_PAPRD_CTRL0_B0_USE_SINGLE_TABLE_MASK, 1);
        //FieldWrite("BB_paprd_ctrl1_b0.paprd_adaptive_am2pm_enable_0", 1);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B0,
            AR_PHY_PAPRD_CTRL1_B0_ADAPTIVE_AM2PM_ENABLE_0, 1);
        //FieldWrite("BB_paprd_ctrl1_b0.paprd_adaptive_am2am_enable_0", 1);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B0,
            AR_PHY_PAPRD_CTRL1_B0_ADAPTIVE_AM2AM_ENABLE_0, 1);
        //FieldWrite("BB_paprd_ctrl1_b0.paprd_adaptive_scaling_enable_0", 0);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B0,
            AR_PHY_PAPRD_CTRL1_B0_ADAPTIVE_SCALING_ENA, 0);
        //FieldWrite("BB_paprd_ctrl1_b0.pa_gain_scale_factor_0", 181);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B0,
            AR_PHY_PAPRD_CTRL1_B0_PA_GAIN_SCALE_FACT_0_MASK, 181);
        //FieldWrite("BB_paprd_ctrl1_b0.paprd_mag_scale_factor_0", 361);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B0,
            AR_PHY_PAPRD_CTRL1_B0_PAPRD_MAG_SCALE_FACT_0, 361);
        //FieldWrite("BB_paprd_ctrl1_b0.paprd_adaptive_scaling_enable_0", 0);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B0,
            AR_PHY_PAPRD_CTRL1_B0_ADAPTIVE_SCALING_ENA, 0);
        //FieldWrite("BB_paprd_ctrl0_b0.paprd_mag_thrsh_0", 3);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B0,
            AR_PHY_PAPRD_CTRL0_B0_PAPRD_MAG_THRSH_0, 3);
    }

    // chain1
    if (AH9300(ah)->ah_txchainmask & AR9300_CHAIN1_MASK) {
        //FieldWrite("BB_paprd_ctrl0_b1.paprd_adaptive_use_single_table_1", 1);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B1,
            AR_PHY_PAPRD_CTRL0_B1_PAPRD_ADAPTIVE_USE_SINGLE_TABLE_1, 1);
        //FieldWrite("BB_paprd_ctrl1_b1.paprd_adaptive_am2pm_enable_1", 1);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B1,
            AR_PHY_PAPRD_CTRL1_B1_ADAPTIVE_AM2PM_ENABLE_1, 1);
        //FieldWrite("BB_paprd_ctrl1_b1.paprd_adaptive_am2am_enable_1", 1);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B1,
            AR_PHY_PAPRD_CTRL1_B1_ADAPTIVE_AM2AM_ENABLE_1, 1);
        //FieldWrite("BB_paprd_ctrl1_b1.paprd_adaptive_scaling_enable_1", 0);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B1,
            AR_PHY_PAPRD_CTRL1_B1_ADAPTIVE_SCALING_ENA, 0);
        //FieldWrite("BB_paprd_ctrl1_b1.pa_gain_scale_factor_1", 181);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B1,
            AR_PHY_PAPRD_CTRL1_B1_PA_GAIN_SCALE_FACT_1_MASK, 181);
        //FieldWrite("BB_paprd_ctrl1_b1.paprd_mag_scale_factor_1", 361);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B1,
            AR_PHY_PAPRD_CTRL1_B1_PAPRD_MAG_SCALE_FACT_1, 361);
        //FieldWrite("BB_paprd_ctrl1_b1.paprd_adaptive_scaling_enable_1", 0);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B1,
            AR_PHY_PAPRD_CTRL1_B1_ADAPTIVE_SCALING_ENA, 0);
        //FieldWrite("BB_paprd_ctrl0_b1.paprd_mag_thrsh_1", 3);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B1,
            AR_PHY_PAPRD_CTRL0_B1_PAPRD_MAG_THRSH_1, 3);
     }
     // chain2
     if (AH9300(ah)->ah_txchainmask & AR9300_CHAIN2_MASK) {
        //FieldWrite("BB_paprd_ctrl0_b2.paprd_adaptive_use_single_table_2", 1);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B2,
            AR_PHY_PAPRD_CTRL0_B2_PAPRD_ADAPTIVE_USE_SINGLE_TABLE_2, 1);
        //FieldWrite("BB_paprd_ctrl1_b2.paprd_adaptive_am2pm_enable_2", 1);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B2,
            AR_PHY_PAPRD_CTRL1_B2_ADAPTIVE_AM2PM_ENABLE_2, 1);
        //FieldWrite("BB_paprd_ctrl1_b2.paprd_adaptive_am2am_enable_2", 1);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B2,
            AR_PHY_PAPRD_CTRL1_B2_ADAPTIVE_AM2AM_ENABLE_2, 1);
        //FieldWrite("BB_paprd_ctrl1_b2.paprd_adaptive_scaling_enable_2", 0);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B2,
            AR_PHY_PAPRD_CTRL1_B2_ADAPTIVE_SCALING_ENA, 0);
        //FieldWrite("BB_paprd_ctrl1_b2.pa_gain_scale_factor_2", 181);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B2,
            AR_PHY_PAPRD_CTRL1_B2_PA_GAIN_SCALE_FACT_2_MASK, 181);
        //FieldWrite("BB_paprd_ctrl1_b2.paprd_mag_scale_factor_2", 361);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B2,
            AR_PHY_PAPRD_CTRL1_B2_PAPRD_MAG_SCALE_FACT_2, 361);
        //FieldWrite("BB_paprd_ctrl1_b2.paprd_adaptive_scaling_enable_2", 0);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B2,
            AR_PHY_PAPRD_CTRL1_B2_ADAPTIVE_SCALING_ENA, 0);
        //FieldWrite("BB_paprd_ctrl0_b2.paprd_mag_thrsh_2", 3);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B2,
            AR_PHY_PAPRD_CTRL0_B2_PAPRD_MAG_THRSH_2, 3);
     }

    ar9300EnablePAPD(ah, AH_FALSE, chan);

    if (AR_SREV_POSEIDON(ah)) {
        //FieldWrite("BB_paprd_trainer_cntl1.cf_paprd_lb_skip", 0x30);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_LB_SKIP, 0x30);
        //FieldWrite("BB_paprd_trainer_cntl1.cf_paprd_lb_enable", 1);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_LB_ENABLE, 1);
        //FieldWrite("BB_paprd_trainer_cntl1.cf_paprd_tx_gain_force", 1);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_TX_GAIN_FORCE, 1);
        //FieldWrite("BB_paprd_trainer_cntl1.cf_paprd_rx_bb_gain_force", 0);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_RX_BB_GAIN_FORCE, 0);
        //FieldWrite("BB_paprd_trainer_cntl1.cf_paprd_iqcorr_enable", 0);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_IQCORR_ENABLE, 0);
        //FieldWrite("BB_paprd_trainer_cntl1.cf_paprd_agc2_settling", 28);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_AGC2_SETTLING, 28);
        //FieldWrite("BB_paprd_trainer_cntl1.cf_paprd_train_enable", 1);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_CF_PAPRD_TRAIN_ENABLE, 1);
        //FieldWrite("BB_paprd_trainer_cntl2.cf_paprd_init_rx_bb_gain",  147);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL2_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL2_CF_PAPRD_INIT_RX_BB_GAIN, 148);
        //FieldWrite("BB_paprd_trainer_cntl3.cf_paprd_fine_corr_len", 4);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_FINE_CORR_LEN, 4);
        //FieldWrite("BB_paprd_trainer_cntl3.cf_paprd_coarse_corr_len", 4);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_COARSE_CORR_LEN, 4);
        //FieldWrite("BB_paprd_trainer_cntl3.cf_paprd_num_corr_stages", 7);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_NUM_CORR_STAGES, 7);
        //FieldWrite("BB_paprd_trainer_cntl3.cf_paprd_min_loopback_del", 1);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_MIN_LOOPBACK_DEL, 1);
	    //FieldWrite("BB_paprd_trainer_cntl3.cf_paprd_quick_drop", -3);
	    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
	        AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_QUICK_DROP, -3);
        //FieldWrite("BB_paprd_trainer_cntl3.cf_paprd_adc_desired_size", -15);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_ADC_DESIRED_SIZE, -15);
        //FieldWrite("BB_paprd_trainer_cntl3.cf_paprd_bbtxmix_disable", 1);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_BBTXMIX_DISABLE, 1);
        //FieldWrite("BB_paprd_trainer_cntl4.cf_paprd_safety_delta", 0);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL4_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL4_CF_PAPRD_SAFETY_DELTA, 0);
        //FieldWrite("BB_paprd_trainer_cntl4.cf_paprd_min_corr", 400);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL4_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL4_CF_PAPRD_MIN_CORR, 400);
        //FieldWrite("BB_paprd_trainer_cntl4.cf_paprd_num_train_samples", 100);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL4_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL4_CF_PAPRD_NUM_TRAIN_SAMPLES, 100);
    } else {
        //FieldWrite("BB_paprd_trainer_cntl1.cf_paprd_lb_skip", 0x30);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_LB_SKIP, 0x30);
        //FieldWrite("BB_paprd_trainer_cntl1.cf_paprd_lb_enable", 1);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_LB_ENABLE, 1);
        //FieldWrite("BB_paprd_trainer_cntl1.cf_paprd_tx_gain_force", 1);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_TX_GAIN_FORCE, 1);
        //FieldWrite("BB_paprd_trainer_cntl1.cf_paprd_rx_bb_gain_force", 0);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_RX_BB_GAIN_FORCE, 0);
        //FieldWrite("BB_paprd_trainer_cntl1.cf_paprd_iqcorr_enable", 0);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_IQCORR_ENABLE, 0);
        //FieldWrite("BB_paprd_trainer_cntl1.cf_paprd_agc2_settling", 28);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_AGC2_SETTLING, 28);
        //FieldWrite("BB_paprd_trainer_cntl1.cf_paprd_train_enable", 1);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_CF_PAPRD_TRAIN_ENABLE, 1);
        //FieldWrite("BB_paprd_trainer_cntl2.cf_paprd_init_rx_bb_gain",  147);
		if(AR_SREV_WASP(ah) && !is2G) {
	        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL2,
	            AR_PHY_PAPRD_TRAINER_CNTL2_CF_PAPRD_INIT_RX_BB_GAIN, 137);
		} else {
	        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL2,
	            AR_PHY_PAPRD_TRAINER_CNTL2_CF_PAPRD_INIT_RX_BB_GAIN, 147);
        }
        //FieldWrite("BB_paprd_trainer_cntl3.cf_paprd_fine_corr_len", 4);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_FINE_CORR_LEN, 4);
        //FieldWrite("BB_paprd_trainer_cntl3.cf_paprd_coarse_corr_len", 4);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_COARSE_CORR_LEN, 4);
        //FieldWrite("BB_paprd_trainer_cntl3.cf_paprd_num_corr_stages", 7);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_NUM_CORR_STAGES, 7);
        //FieldWrite("BB_paprd_trainer_cntl3.cf_paprd_min_loopback_del", 1);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_MIN_LOOPBACK_DEL, 1);
        if(AR_SREV_HORNET(ah) || AR_SREV_WASP(ah))
        {
    	    //FieldWrite("BB_paprd_trainer_cntl3.cf_paprd_quick_drop", -3);
    	    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
    	        AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_QUICK_DROP, -3);
        }
        else {
    	    //FieldWrite("BB_paprd_trainer_cntl3.cf_paprd_quick_drop", -6);
    	    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
    	        AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_QUICK_DROP, -6);
        }
		if(AR_SREV_WASP(ah) && !is2G) {
	        //FieldWrite("BB_paprd_trainer_cntl3.cf_paprd_adc_desired_size", -15);
	        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
	            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_ADC_DESIRED_SIZE, -10);
		} else {
	        //FieldWrite("BB_paprd_trainer_cntl3.cf_paprd_adc_desired_size", -15);
	        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
	            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_ADC_DESIRED_SIZE, -15);
        }
        //FieldWrite("BB_paprd_trainer_cntl3.cf_paprd_bbtxmix_disable", 1);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_BBTXMIX_DISABLE, 1);
        //FieldWrite("BB_paprd_trainer_cntl4.cf_paprd_safety_delta", 0);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL4,
            AR_PHY_PAPRD_TRAINER_CNTL4_CF_PAPRD_SAFETY_DELTA, 0);
        //FieldWrite("BB_paprd_trainer_cntl4.cf_paprd_min_corr", 400);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL4,
            AR_PHY_PAPRD_TRAINER_CNTL4_CF_PAPRD_MIN_CORR, 400);
        //FieldWrite("BB_paprd_trainer_cntl4.cf_paprd_num_train_samples", 100);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL4,
            AR_PHY_PAPRD_TRAINER_CNTL4_CF_PAPRD_NUM_TRAIN_SAMPLES, 100);
    }

    /*
     * FieldWrite(
     *     "BB_paprd_pre_post_scale_0_b0.paprd_pre_post_scaling_0_0", 261376);
     */
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_PRE_POST_SCALE_0_B0,
        AR_PHY_PAPRD_PRE_POST_SCALE_0_B0_PAPRD_PRE_POST_SCALING_0_0, 261376);
    /*
     * FieldWrite(
     *    "BB_paprd_pre_post_scale_1_b0.paprd_pre_post_scaling_1_0", 248079);
     */
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_PRE_POST_SCALE_1_B0,
        AR_PHY_PAPRD_PRE_POST_SCALE_1_B0_PAPRD_PRE_POST_SCALING_1_0, 248079);
    /*
     * FieldWrite(
     *     "BB_paprd_pre_post_scale_2_b0.paprd_pre_post_scaling_2_0", 233759);
     */
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_PRE_POST_SCALE_2_B0,
        AR_PHY_PAPRD_PRE_POST_SCALE_2_B0_PAPRD_PRE_POST_SCALING_2_0, 233759);
    /*
     * FieldWrite(
     *     "BB_paprd_pre_post_scale_3_b0.paprd_pre_post_scaling_3_0", 220464);
     */
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_PRE_POST_SCALE_3_B0,
        AR_PHY_PAPRD_PRE_POST_SCALE_3_B0_PAPRD_PRE_POST_SCALING_3_0, 220464);
	/*
     * FieldWrite(
     *     "BB_paprd_pre_post_scale_4_b0.paprd_pre_post_scaling_4_0", 208194);
     */
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_PRE_POST_SCALE_4_B0,
        AR_PHY_PAPRD_PRE_POST_SCALE_4_B0_PAPRD_PRE_POST_SCALING_4_0, 208194);
    /*
     * FieldWrite(
     *     "BB_paprd_pre_post_scale_5_b0.paprd_pre_post_scaling_5_0", 196949);
     */
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_PRE_POST_SCALE_5_B0,
        AR_PHY_PAPRD_PRE_POST_SCALE_5_B0_PAPRD_PRE_POST_SCALING_5_0, 196949);
    /*
     * FieldWrite(
     *     "BB_paprd_pre_post_scale_6_b0.paprd_pre_post_scaling_6_0", 185706);
     */
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_PRE_POST_SCALE_6_B0,
        AR_PHY_PAPRD_PRE_POST_SCALE_6_B0_PAPRD_PRE_POST_SCALING_6_0, 185706);
    /*
     * FieldWrite(
     *     "BB_paprd_pre_post_scale_7_b0.paprd_pre_post_scaling_7_0", 175487);
     */
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_PRE_POST_SCALE_7_B0,
        AR_PHY_PAPRD_PRE_POST_SCALE_7_B0_PAPRD_PRE_POST_SCALING_7_0, 175487);
    return 0;

FAILED:
    return -1;
#undef ABS
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

void ar9300EnablePAPD(struct ath_hal *ah, HAL_BOOL enableFlag, HAL_CHANNEL * chan)
{
	HAL_BOOL enable=enableFlag;
    u_int32_t am_mask = 0;
    u_int32_t val = OS_REG_READ(ah, AR_2040_MODE);
    int is2G = AR9300_IS_CHAN(chan, CHANNEL_2GHZ);
    int isHT40 = 0;
    struct ath_hal_9300 *ahp = AH9300(ah);

    if (val & HAL_HT_MACMODE_2040) {
        isHT40 = 1;
    }
    if (enableFlag == AH_TRUE) {

		ar9300_eeprom_t *eep = &AH9300(ah)->ah_eeprom;


	    if (!is2G) {
			// 3 bits for modalHeader5G.papdRateMaskHt20 is used for sub band disabling of paprd
			// 5G band is divided into 3 sub bands -- upper, mid, lower.
			// If bit 30 of modalHeader5G.papdRateMaskHt20 is set to one -- disable paprd for upper 5G
			// If bit 29 of modalHeader5G.papdRateMaskHt20 is set to one -- disable paprd for mid 5G
			// If bit 28 of modalHeader5G.papdRateMaskHt20 is set to one -- disable paprd for lower 5G
			//u_int32_t am_mask = eep->modalHeader5G.papdRateMaskHt20;

			if(chan->channel >= UPPER_5G_SUB_BANDSTART){
				if(eep->modalHeader5G.papdRateMaskHt20&(1<<30)) {
					enable = AH_FALSE;
				}
			} else if((UPPER_5G_SUB_BANDSTART < chan->channel) && (chan->channel >= MID_5G_SUB_BANDSTART)) {
				if(eep->modalHeader5G.papdRateMaskHt20&(1<<29)) {
					enable = AH_FALSE;
				}
			} else { // must be in the lower 5G subband
				if(eep->modalHeader5G.papdRateMaskHt20&(1<<28)) {
					enable = AH_FALSE;
				}
			}
		}
	}
	if(enable) {
/*******/
        if (is2G) {
            if (isHT40) {
                am_mask = ahp->ah_2G_papdRateMaskHt40 & AH_PAPRD_AM_PM_MASK;
            } else {
                am_mask = ahp->ah_2G_papdRateMaskHt20 & AH_PAPRD_AM_PM_MASK;
            }
        } else {
            if (isHT40) {
                am_mask = ahp->ah_5G_papdRateMaskHt40 & AH_PAPRD_AM_PM_MASK;
            } else {
                am_mask = ahp->ah_5G_papdRateMaskHt20 & AH_PAPRD_AM_PM_MASK;
            }
        }
        /* Earlier we promgrammed TGT Power with Scaled down value, since
                * PAPRD CAL was not done.
                * Now we finish PAPRD CAL, so bump up the TGT PWR to original
                * EEPROM Power. CTLs calc and appled in "ar9300EepromSetTransmitPower"
                */
    {
        HAL_CHANNEL_INTERNAL *ichan = ar9300CheckChan(ah, chan);
        ichan->paprdTableWriteDone = 1;
        chan->paprdTableWriteDone = 1;
        //ath_hal_printf(ah, "%s[%d] EepromSetTransmitPower PAPRD\n", __func__, __LINE__);
        if (ar9300EepromSetTransmitPower(ah, &ahp->ah_eeprom, ichan,
            ath_hal_getctl(ah, chan), ath_hal_getantennaallowed(ah, chan),
            ichan->maxRegTxPower * 2,
            AH_MIN(MAX_RATE_POWER, AH_PRIVATE(ah)->ah_powerLimit)) != HAL_OK) {
            ichan->paprdTableWriteDone = 0;
            chan->paprdTableWriteDone = 0;
            /* Intentional print */
            ath_hal_printf(ah, "%s[%d] EepromSetTransmitPower failed ABORT PAPRD\n", __func__, __LINE__);

            //FieldWrite("BB_paprd_ctrl0_b0.paprd_enable_0", 0x0);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B0,
                AR_PHY_PAPRD_CTRL0_B0_PAPRD_ENABLE_0, 0);
            if (!AR_SREV_POSEIDON(ah) && !AR_SREV_HORNET(ah)) {
                //FieldWrite("BB_paprd_ctrl0_b1.paprd_enable_1", 0x0);
                OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B1,
                    AR_PHY_PAPRD_CTRL0_B1_PAPRD_ENABLE_1, 0);
                //FieldWrite("BB_paprd_ctrl0_b2.paprd_enable_2", 0x0);
                OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B2,
                    AR_PHY_PAPRD_CTRL0_B2_PAPRD_ENABLE_2, 0);
            }
            return;
       }
    }

        HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s 2G %d HT40 %d am_mask 0x%08x\n",
            __func__, is2G, isHT40, am_mask);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_AM2AM, AR_PHY_PAPRD_AM2AM_MASK,
            am_mask);
        if(AR_SREV_HORNET(ah)) {
	        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_AM2PM, AR_PHY_PAPRD_AM2PM_MASK,
	            0);//am_mask
        }
        else {
	        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_AM2PM, AR_PHY_PAPRD_AM2PM_MASK,
	            am_mask);
        }

        //FieldWrite("BB_paprd_ht40_mask.paprd_ht40_mask", 0x1ffffff);
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_HT40, AR_PHY_PAPRD_HT40_MASK,
            AR_PHY_PAPRD_HT40_MASK);
        // chain0
        if(AH9300(ah)->ah_txchainmask & AR9300_CHAIN0_MASK) {
            //FieldWrite("BB_paprd_ctrl0_b0.paprd_adaptive_use_single_table_0", 1);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B0,
                AR_PHY_PAPRD_CTRL0_B0_USE_SINGLE_TABLE_MASK, 1);
            //FieldWrite("BB_paprd_ctrl1_b0.paprd_adaptive_am2pm_enable_0", 1);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B0,
                AR_PHY_PAPRD_CTRL1_B0_ADAPTIVE_AM2PM_ENABLE_0, 1);
            //FieldWrite("BB_paprd_ctrl1_b0.paprd_adaptive_am2am_enable_0", 1);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B0,
                AR_PHY_PAPRD_CTRL1_B0_ADAPTIVE_AM2AM_ENABLE_0, 1);
            //FieldWrite("BB_paprd_ctrl1_b0.paprd_adaptive_scaling_enable_0", 0);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B0,
                AR_PHY_PAPRD_CTRL1_B0_ADAPTIVE_SCALING_ENA, 0);
            //FieldWrite("BB_paprd_ctrl1_b0.pa_gain_scale_factor_0", 181);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B0,
                AR_PHY_PAPRD_CTRL1_B0_PA_GAIN_SCALE_FACT_0_MASK, 181);
            //FieldWrite("BB_paprd_ctrl1_b0.paprd_mag_scale_factor_0", 361);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B0,
                AR_PHY_PAPRD_CTRL1_B0_PAPRD_MAG_SCALE_FACT_0, 361);
            //FieldWrite("BB_paprd_ctrl1_b0.paprd_adaptive_scaling_enable_0", 0);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B0,
                AR_PHY_PAPRD_CTRL1_B0_ADAPTIVE_SCALING_ENA, 0);
            //FieldWrite("BB_paprd_ctrl0_b0.paprd_mag_thrsh_0", 3);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B0,
                AR_PHY_PAPRD_CTRL0_B0_PAPRD_MAG_THRSH_0, 3);
        }
        // chain1
        if(AH9300(ah)->ah_txchainmask & AR9300_CHAIN1_MASK) {
            //FieldWrite("BB_paprd_ctrl0_b1.paprd_adaptive_use_single_table_1", 1);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B1,
                AR_PHY_PAPRD_CTRL0_B1_PAPRD_ADAPTIVE_USE_SINGLE_TABLE_1, 1);
            //FieldWrite("BB_paprd_ctrl1_b1.paprd_adaptive_am2pm_enable_1", 1);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B1,
                AR_PHY_PAPRD_CTRL1_B1_ADAPTIVE_AM2PM_ENABLE_1, 1);
            //FieldWrite("BB_paprd_ctrl1_b1.paprd_adaptive_am2am_enable_1", 1);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B1,
                AR_PHY_PAPRD_CTRL1_B1_ADAPTIVE_AM2AM_ENABLE_1, 1);
            //FieldWrite("BB_paprd_ctrl1_b1.paprd_adaptive_scaling_enable_1", 0);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B1,
                AR_PHY_PAPRD_CTRL1_B1_ADAPTIVE_SCALING_ENA, 0);
            //FieldWrite("BB_paprd_ctrl1_b1.pa_gain_scale_factor_1", 181);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B1,
                AR_PHY_PAPRD_CTRL1_B1_PA_GAIN_SCALE_FACT_1_MASK, 181);
            //FieldWrite("BB_paprd_ctrl1_b1.paprd_mag_scale_factor_1", 361);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B1,
                AR_PHY_PAPRD_CTRL1_B1_PAPRD_MAG_SCALE_FACT_1, 361);
            //FieldWrite("BB_paprd_ctrl1_b1.paprd_adaptive_scaling_enable_1", 0);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B1,
                AR_PHY_PAPRD_CTRL1_B1_ADAPTIVE_SCALING_ENA, 0);
            //FieldWrite("BB_paprd_ctrl0_b1.paprd_mag_thrsh_1", 3);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B1,
                AR_PHY_PAPRD_CTRL0_B1_PAPRD_MAG_THRSH_1, 3);
        }
        // chain2
        if(AH9300(ah)->ah_txchainmask & AR9300_CHAIN2_MASK) {
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B2,
                AR_PHY_PAPRD_CTRL0_B2_PAPRD_ADAPTIVE_USE_SINGLE_TABLE_2, 1);
            //FieldWrite("BB_paprd_ctrl1_b2.paprd_adaptive_am2pm_enable_2", 1);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B2,
                AR_PHY_PAPRD_CTRL1_B2_ADAPTIVE_AM2PM_ENABLE_2, 1);
            //FieldWrite("BB_paprd_ctrl1_b2.paprd_adaptive_am2am_enable_2", 1);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B2,
                AR_PHY_PAPRD_CTRL1_B2_ADAPTIVE_AM2AM_ENABLE_2, 1);
            //FieldWrite("BB_paprd_ctrl1_b2.paprd_adaptive_scaling_enable_2", 0);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B2,
                AR_PHY_PAPRD_CTRL1_B2_ADAPTIVE_SCALING_ENA, 0);
            //FieldWrite("BB_paprd_ctrl1_b2.pa_gain_scale_factor_2", 181);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B2,
                AR_PHY_PAPRD_CTRL1_B2_PA_GAIN_SCALE_FACT_2_MASK, 181);
            //FieldWrite("BB_paprd_ctrl1_b2.paprd_mag_scale_factor_2", 361);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B2,
                AR_PHY_PAPRD_CTRL1_B2_PAPRD_MAG_SCALE_FACT_2, 361);
            //FieldWrite("BB_paprd_ctrl1_b2.paprd_adaptive_scaling_enable_2", 0);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B2,
                AR_PHY_PAPRD_CTRL1_B2_ADAPTIVE_SCALING_ENA, 0);
            //FieldWrite("BB_paprd_ctrl0_b2.paprd_mag_thrsh_2", 3);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B2,
                AR_PHY_PAPRD_CTRL0_B2_PAPRD_MAG_THRSH_2, 3);
        }


/*******/
        if(AH9300(ah)->ah_txchainmask & AR9300_CHAIN0_MASK) {
	    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B0,
		    AR_PHY_PAPRD_CTRL0_B0_PAPRD_ENABLE_0, 1);
        }
        if(AH9300(ah)->ah_txchainmask & AR9300_CHAIN1_MASK) {
    	    //FieldWrite("BB_paprd_ctrl0_b1.paprd_enable_1", 0x1);
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B1,
                AR_PHY_PAPRD_CTRL0_B1_PAPRD_ENABLE_1, 1);
        }
        if(AH9300(ah)->ah_txchainmask & AR9300_CHAIN2_MASK) {
                OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B2,
                    AR_PHY_PAPRD_CTRL0_B2_PAPRD_ENABLE_2, 1);
        }
	} else {
	    if(AH9300(ah)->ah_txchainmask & AR9300_CHAIN0_MASK) {
	    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B0,
		    AR_PHY_PAPRD_CTRL0_B0_PAPRD_ENABLE_0, 0);
        }
        if(AH9300(ah)->ah_txchainmask & AR9300_CHAIN1_MASK) {
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B1,
                AR_PHY_PAPRD_CTRL0_B1_PAPRD_ENABLE_1, 0);
        }
        if(AH9300(ah)->ah_txchainmask & AR9300_CHAIN2_MASK) {
                OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B2,
                    AR_PHY_PAPRD_CTRL0_B2_PAPRD_ENABLE_2, 0);
        }
	}
}

static void Ar9300GainTableEntries(struct ath_hal *ah)
{
    int i;
    u_int32_t reg;
    u_int32_t *gain_table_entries = AH9300(ah)->paprd_gain_table_entries;
    u_int32_t *gain_vs_table_index = AH9300(ah)->paprd_gain_table_index;

    reg = AR_PHY_TXGAIN_TAB(1);

    for (i = 0; i < 32; i++) {
        gain_table_entries[i] = OS_REG_READ(ah, reg);
        gain_vs_table_index[i] = (gain_table_entries[i] >> 24) & 0xff;
        /*
         * ath_hal_printf(
         *     ah, "+++reg 0x%08x gain_table_entries[%d] = 0x%08x \n",
         *     reg, i, gain_table_entries[i]);
         */
        reg = reg + 4;
    }
}

// Get gain index for Target power
static unsigned int Ar9300GetDesiredGainForChain(struct ath_hal *ah,
    int chainNum, int target_power)
{
    int olpc_gain_delta = 0;
    int alpha_therm = 0, alpha_volt = 0;
    int therm_cal_value = 0, volt_cal_value = 0;
    int latest_therm_value = 0, latest_volt_value = 0, olpc_gain_delta_tmp = 0;
    int thermal_gain_corr = 0, voltage_gain_corr = 0, desired_scale = 0;
    int desired_gain = 0;
	int cl_gain_mod = 0;

    // Clear the training done bit
    //FieldWrite("BB_paprd_trainer_stat1.paprd_train_done", 0);
    if (AR_SREV_POSEIDON(ah)) {
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_TRAIN_DONE, 0);
    } else {
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_TRAIN_DONE, 0);
    }
    //FieldRead("BB_tpc_12.desired_scale_ht40_5", &desired_scale);
    desired_scale =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TPC_12,
        AR_PHY_TPC_12_DESIRED_SCALE_HT40_5);
    //FieldRead("BB_tpc_19.alpha_therm", &alpha_therm);
    alpha_therm =
        OS_REG_READ_FIELD(ah, AR_PHY_TPC_19, AR_PHY_TPC_19_ALPHA_THERM);
    //FieldRead("BB_tpc_19.alpha_volt", &alpha_volt);
    alpha_volt =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TPC_19, AR_PHY_TPC_19_ALT_ALPHA_VOLT);

    //FieldRead("BB_tpc_18.therm_cal_value", &therm_cal_value);
    therm_cal_value =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TPC_18,
        AR_PHY_TPC_18_ALT_THERM_CAL_VALUE);
    //FieldRead("BB_tpc_18.volt_cal_value", &volt_cal_value);
    volt_cal_value =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TPC_18,
        AR_PHY_TPC_18_ALT_VOLT_CAL_VALUE);

    //FieldRead("BB_therm_adc_4.latest_therm_value", &latest_therm_value);
    latest_therm_value =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_THERM_ADC_4,
        AR_PHY_THERM_ADC_4_LATEST_THERM_VALUE);
    //FieldRead("BB_therm_adc_4.latest_volt_value", &latest_volt_value);
    latest_volt_value =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_THERM_ADC_4,
        AR_PHY_THERM_ADC_4_LATEST_VOLT_VALUE);

    /*
     * sprintf(
     *     fieldName, "%s%d%s%d\0", "BB_tpc_11_b",
     *     chainNum, ".olpc_gain_delta_", chainNum);
     */
    //FieldRead(fieldName, &olpc_gain_delta_tmp);


    if (chainNum == 0) {
        olpc_gain_delta_tmp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_TPC_11_B0,
            AR_PHY_TPC_11_B0_OLPC_GAIN_DELTA_0);
		cl_gain_mod = OS_REG_READ_FIELD_ALT(ah, AR_PHY_CL_TAB_0,
			AR_PHY_CL_TAB_0_CL_GAIN_MOD);
    } else if (chainNum == 1) {
        if (!AR_SREV_POSEIDON(ah)) {
            olpc_gain_delta_tmp =
                OS_REG_READ_FIELD_ALT(ah, AR_PHY_TPC_11_B1,
                AR_PHY_TPC_11_B1_OLPC_GAIN_DELTA_1);
            cl_gain_mod = OS_REG_READ_FIELD_ALT(ah, AR_PHY_CL_TAB_1,
                AR_PHY_CL_TAB_1_CL_GAIN_MOD);
        }
    } else if (chainNum == 2) {
        if (!AR_SREV_POSEIDON(ah)) {
            olpc_gain_delta_tmp =
                OS_REG_READ_FIELD_ALT(ah, AR_PHY_TPC_11_B2,
                    AR_PHY_TPC_11_B2_OLPC_GAIN_DELTA_2);
                cl_gain_mod = OS_REG_READ_FIELD_ALT(ah, AR_PHY_CL_TAB_2,
                    AR_PHY_CL_TAB_2_CL_GAIN_MOD);
        }
    } else {
        // invalid chainNUm
    }

    if (olpc_gain_delta_tmp < 128) {
        olpc_gain_delta = olpc_gain_delta_tmp;
    } else {
        olpc_gain_delta = olpc_gain_delta_tmp - 256;
    }

    thermal_gain_corr =
        (int) (alpha_therm * (latest_therm_value - therm_cal_value) +
        128) >> 8;
    voltage_gain_corr =
        (int) (alpha_volt * (latest_volt_value - volt_cal_value) + 64) >> 7;
    desired_gain =
        target_power - olpc_gain_delta - thermal_gain_corr -
        voltage_gain_corr + desired_scale + cl_gain_mod;
    /*
     * ath_hal_printf(ah,
     *     "olpc_gain_delta %d, desired_gain %d\n",
     *     olpc_gain_delta, desired_gain);
     */
#if 0
    ath_hal_printf(ah,
        "+++ target_power %d olpc_gain_delta %d, cl_gain_mod %d,"
        "thermal_gain_corr %d  voltage_gain_corr %d desired_scale %d"
        "desired_gain %d\n",
        target_power, olpc_gain_delta, cl_gain_mod, thermal_gain_corr,
        voltage_gain_corr,
        desired_scale, desired_gain);
#endif
    return (unsigned int) desired_gain;
}

static void Ar9300TxForceGain(struct ath_hal *ah, unsigned int gain_index)
{
    int selected_gain_entry, txbb1dbgain, txbb6dbgain, txmxrgain;
    int padrvgnA, padrvgnB, padrvgnC, padrvgnD;
    u_int32_t *gain_table_entries = AH9300(ah)->paprd_gain_table_entries;

    //u_int32_t *gain_vs_table_index = ah->paprd_gain_table_index;
    selected_gain_entry = gain_table_entries[gain_index];
    txbb1dbgain = selected_gain_entry & 0x7;
    txbb6dbgain = (selected_gain_entry >> 3) & 0x3;
    txmxrgain = (selected_gain_entry >> 5) & 0xf;
    padrvgnA = (selected_gain_entry >> 9) & 0xf;
    padrvgnB = (selected_gain_entry >> 13) & 0xf;
    padrvgnC = (selected_gain_entry >> 17) & 0xf;
    padrvgnD = (selected_gain_entry >> 21) & 0x3;

    //FieldWrite("BB_tx_forced_gain.forced_txbb1dbgain", txbb1dbgain);
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_TXBB1DBGAIN, txbb1dbgain);
    //FieldWrite("BB_tx_forced_gain.forced_txbb6dbgain", txbb6dbgain);
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_TXBB6DBGAIN, txbb6dbgain);
    //FieldWrite("BB_tx_forced_gain.forced_txmxrgain", txmxrgain);
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_TXMXRGAIN, txmxrgain);
    //FieldWrite("BB_tx_forced_gain.forced_padrvgnA", padrvgnA);
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_PADRVGNA, padrvgnA);
    //FieldWrite("BB_tx_forced_gain.forced_padrvgnB", padrvgnB);
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_PADRVGNB, padrvgnB);
    //FieldWrite("BB_tx_forced_gain.forced_padrvgnC", padrvgnC);
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_PADRVGNC, padrvgnC);
    //FieldWrite("BB_tx_forced_gain.forced_padrvgnD", padrvgnD);
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_PADRVGND, padrvgnD);
    //FieldWrite("BB_tx_forced_gain.forced_enable_PAL", 0);
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_ENABLE_PAL, 0);
    //FieldWrite("BB_tx_forced_gain.force_tx_gain", 0);
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCE_TX_GAIN, 0);

    //FieldWrite("BB_tpc_1.forced_dac_gain", 0);
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_TPC_1, AR_PHY_TPC_1_FORCED_DAC_GAIN, 0);
    //FieldWrite("BB_tpc_1.force_dac_gain", 0);
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_TPC_1, AR_PHY_TPC_1_FORCE_DAC_GAIN, 0);
}

#ifdef ART_PAPD_DEBUG
void AR9300PAPDDebugPrint(struct ath_hal *ah)
{
    int temp;
    int txbb1dbgain, txbb6dbgain, txmxrgain;
    int padrvgnA, padrvgnB, padrvgnC, padrvgnD;

    if (AR_SREV_POSEIDON(ah)) {
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_lb_skip", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_LB_SKIP);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl1.cf_paprd_lb_skip=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_lb_enable", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_LB_ENABLE);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl1.cf_paprd_lb_enable=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_tx_gain_force", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_TX_GAIN_FORCE);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl1.cf_paprd_tx_gain_force=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_rx_bb_gain_force", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_RX_BB_GAIN_FORCE);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl1.cf_paprd_rx_bb_gain_force=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_iqcorr_enable", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_IQCORR_ENABLE);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl1.cf_paprd_iqcorr_enable=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_agc2_settling", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_AGC2_SETTLING);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl1.cf_paprd_agc2_settling=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_train_enable", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_CF_PAPRD_TRAIN_ENABLE);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl1.cf_paprd_train_enable=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl2.cf_paprd_init_rx_bb_gain", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL2_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL2_CF_PAPRD_INIT_RX_BB_GAIN);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl2.cf_paprd_init_rx_bb_gain=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_fine_corr_len", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_FINE_CORR_LEN);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl3.cf_paprd_fine_corr_len=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_coarse_corr_len", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_COARSE_CORR_LEN);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl3.cf_paprd_coarse_corr_len=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_num_corr_stages", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_NUM_CORR_STAGES);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl3.cf_paprd_num_corr_stages=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_min_loopback_del", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_MIN_LOOPBACK_DEL);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl3.cf_paprd_min_loopback_del=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_quick_drop", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_QUICK_DROP);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl3.cf_paprd_quick_drop=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_adc_desired_size", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_ADC_DESIRED_SIZE);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl3.cf_paprd_adc_desired_size=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_bbtxmix_disable", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_BBTXMIX_DISABLE);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl3.cf_paprd_bbtxmix_disable=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl4.cf_paprd_safety_delta", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL4_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL4_CF_PAPRD_SAFETY_DELTA);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl4.cf_paprd_safety_delta=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl4.cf_paprd_min_corr", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL4_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL4_CF_PAPRD_MIN_CORR);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl4.cf_paprd_min_corr=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_agc2_pwr", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_AGC2_PWR);
        ath_hal_printf(ah, "    paprd_agc2_pwr              = 0x%02x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_rx_gain_idx", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_RX_GAIN_IDX);
        ath_hal_printf(ah, "    paprd_rx_gain_idx           = 0x%02x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_train_active", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_TRAIN_ACTIVE);
        ath_hal_printf(ah, "    paprd_train_active          = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_corr_err", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_CORR_ERR);
        ath_hal_printf(ah, "    paprd_corr_err              = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_train_incomplete", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_TRAIN_INCOMPLETE);
        ath_hal_printf(ah, "    paprd_train_incomplete      = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_train_done", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_TRAIN_DONE);
        ath_hal_printf(ah, "    paprd_train_done            = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat2.paprd_fine_idx", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT2_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT2_PAPRD_FINE_IDX);
        ath_hal_printf(ah, "    paprd_fine_idx              = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat2.paprd_coarse_idx", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT2_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT2_PAPRD_COARSE_IDX);
        ath_hal_printf(ah, "    paprd_coarse_idx            = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat2.paprd_fine_val", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT2_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT2_PAPRD_FINE_VAL);
        ath_hal_printf(ah, "    paprd_fine_val              = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat3.paprd_train_samples_cnt", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT3_PAPRD_TRAIN_SAMPLES_CNT);
        ath_hal_printf(ah, "    paprd_train_samples_cnt     = 0x%08x\n", temp);
    } else {
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_lb_skip", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_LB_SKIP);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl1.cf_paprd_lb_skip=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_lb_enable", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_LB_ENABLE);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl1.cf_paprd_lb_enable=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_tx_gain_force", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_TX_GAIN_FORCE);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl1.cf_paprd_tx_gain_force=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_rx_bb_gain_force", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_RX_BB_GAIN_FORCE);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl1.cf_paprd_rx_bb_gain_force=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_iqcorr_enable", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_IQCORR_ENABLE);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl1.cf_paprd_iqcorr_enable=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_agc2_settling", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_AGC2_SETTLING);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl1.cf_paprd_agc2_settling=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_train_enable", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_CF_PAPRD_TRAIN_ENABLE);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl1.cf_paprd_train_enable=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl2.cf_paprd_init_rx_bb_gain", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL2,
            AR_PHY_PAPRD_TRAINER_CNTL2_CF_PAPRD_INIT_RX_BB_GAIN);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl2.cf_paprd_init_rx_bb_gain=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_fine_corr_len", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_FINE_CORR_LEN);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl3.cf_paprd_fine_corr_len=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_coarse_corr_len", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_COARSE_CORR_LEN);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl3.cf_paprd_coarse_corr_len=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_num_corr_stages", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_NUM_CORR_STAGES);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl3.cf_paprd_num_corr_stages=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_min_loopback_del", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_MIN_LOOPBACK_DEL);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl3.cf_paprd_min_loopback_del=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_quick_drop", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_QUICK_DROP);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl3.cf_paprd_quick_drop=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_adc_desired_size", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_ADC_DESIRED_SIZE);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl3.cf_paprd_adc_desired_size=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_bbtxmix_disable", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_BBTXMIX_DISABLE);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl3.cf_paprd_bbtxmix_disable=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl4.cf_paprd_safety_delta", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL4,
            AR_PHY_PAPRD_TRAINER_CNTL4_CF_PAPRD_SAFETY_DELTA);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl4.cf_paprd_safety_delta=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl4.cf_paprd_min_corr", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL4,
            AR_PHY_PAPRD_TRAINER_CNTL4_CF_PAPRD_MIN_CORR);
        ath_hal_printf(ah, "BB_paprd_trainer_cntl4.cf_paprd_min_corr=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_agc2_pwr", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_AGC2_PWR);
        ath_hal_printf(ah, "    paprd_agc2_pwr              = 0x%02x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_rx_gain_idx", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_RX_GAIN_IDX);
        ath_hal_printf(ah, "    paprd_rx_gain_idx           = 0x%02x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_train_active", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_TRAIN_ACTIVE);
        ath_hal_printf(ah, "    paprd_train_active          = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_corr_err", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_CORR_ERR);
        ath_hal_printf(ah, "    paprd_corr_err              = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_train_incomplete", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_TRAIN_INCOMPLETE);
        ath_hal_printf(ah, "    paprd_train_incomplete      = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_train_done", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_TRAIN_DONE);
        ath_hal_printf(ah, "    paprd_train_done            = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat2.paprd_fine_idx", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT2,
            AR_PHY_PAPRD_TRAINER_STAT2_PAPRD_FINE_IDX);
        ath_hal_printf(ah, "    paprd_fine_idx              = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat2.paprd_coarse_idx", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT2,
            AR_PHY_PAPRD_TRAINER_STAT2_PAPRD_COARSE_IDX);
        ath_hal_printf(ah, "    paprd_coarse_idx            = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat2.paprd_fine_val", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT2,
            AR_PHY_PAPRD_TRAINER_STAT2_PAPRD_FINE_VAL);
        ath_hal_printf(ah, "    paprd_fine_val              = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat3.paprd_train_samples_cnt", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT3,
            AR_PHY_PAPRD_TRAINER_STAT3_PAPRD_TRAIN_SAMPLES_CNT);
        ath_hal_printf(ah, "    paprd_train_samples_cnt     = 0x%08x\n", temp);
    }

    //FieldRead("BB_tpc_1.force_dac_gain", &temp);
    temp =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TPC_1, AR_PHY_TPC_1_FORCE_DAC_GAIN);
    ath_hal_printf(ah, "    dac_gain_forced     = 0x%08x\n", temp);
    //FieldRead("BB_tpc_1.forced_dac_gain", &temp);
    temp =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TPC_1, AR_PHY_TPC_1_FORCED_DAC_GAIN);
    ath_hal_printf(ah, "    forced_dac_gain     = 0x%08x\n", temp);

    //FieldRead("BB_paprd_ctrl0_b0.paprd_enable_0", &temp);
    temp =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B0,
        AR_PHY_PAPRD_CTRL0_B0_PAPRD_ENABLE_0);
    ath_hal_printf(ah, "    BB_paprd_ctrl0_b0.paprd_enable_0     = 0x%08x\n", temp);
    if (!AR_SREV_POSEIDON(ah)) {
        //FieldRead("BB_paprd_ctrl0_b1.paprd_enable_1", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B1,
                AR_PHY_PAPRD_CTRL0_B1_PAPRD_ENABLE_1);
        ath_hal_printf(ah, "    BB_paprd_ctrl0_b1.paprd_enable_1     = 0x%08x\n", temp);
        //FieldRead("BB_paprd_ctrl0_b2.paprd_enable_2", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B2,
                AR_PHY_PAPRD_CTRL0_B2_PAPRD_ENABLE_2);
        ath_hal_printf(ah, "    BB_paprd_ctrl0_b2.paprd_enable_2     = 0x%08x\n", temp);
    }

    //FieldRead("BB_tx_forced_gain.forced_txbb1dbgain", &txbb1dbgain);
    txbb1dbgain =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_TXBB1DBGAIN);
    //FieldRead("BB_tx_forced_gain.forced_txbb6dbgain", &txbb6dbgain);
    txbb6dbgain =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_TXBB6DBGAIN);
    //FieldRead("BB_tx_forced_gain.forced_txmxrgain", &txmxrgain);
    txmxrgain =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_TXMXRGAIN);
    //FieldRead("BB_tx_forced_gain.forced_padrvgnA", &padrvgnA);
    padrvgnA =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_PADRVGNA);
    //FieldRead("BB_tx_forced_gain.forced_padrvgnB", &padrvgnB);
    padrvgnB =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_PADRVGNB);
    //FieldRead("BB_tx_forced_gain.forced_padrvgnC", &padrvgnC);
    padrvgnC =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_PADRVGNC);
    //FieldRead("BB_tx_forced_gain.forced_padrvgnD", &padrvgnD);
    padrvgnD =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_PADRVGND);

    ath_hal_printf(ah, "txbb1dbgain=0x%x, txbb6dbgain=0x%x, txmxrgain=0x%x\n", txbb1dbgain,
        txbb6dbgain, txmxrgain);
    ath_hal_printf(ah, "padrvgnA=0x%x, padrvgnB=0x%x\n", padrvgnA, padrvgnB);
    ath_hal_printf(ah, "padrvgnC=0x%x, padrvgnD=0x%x\n", padrvgnC, padrvgnD);

    /*
     * FieldWrite(
     *     "BB_paprd_pre_post_scale_0_b0.paprd_pre_post_scaling_0_0", 261376);
     */
    ath_hal_printf(ah, "BB_paprd_pre_post_scale_0_b0= 0x%08x\n", OS_REG_READ(ah,
            AR_PHY_PAPRD_PRE_POST_SCALE_0_B0));
    ath_hal_printf(ah, "BB_paprd_pre_post_scale_1_b0= 0x%08x\n", OS_REG_READ(ah,
            AR_PHY_PAPRD_PRE_POST_SCALE_1_B0));
    ath_hal_printf(ah, "BB_paprd_pre_post_scale_2_b0= 0x%08x\n", OS_REG_READ(ah,
            AR_PHY_PAPRD_PRE_POST_SCALE_2_B0));
    ath_hal_printf(ah, "BB_paprd_pre_post_scale_3_b0= 0x%08x\n", OS_REG_READ(ah,
            AR_PHY_PAPRD_PRE_POST_SCALE_3_B0));
    ath_hal_printf(ah, "BB_paprd_pre_post_scale_4_b0= 0x%08x\n", OS_REG_READ(ah,
            AR_PHY_PAPRD_PRE_POST_SCALE_4_B0));
    ath_hal_printf(ah, "BB_paprd_pre_post_scale_5_b0= 0x%08x\n", OS_REG_READ(ah,
            AR_PHY_PAPRD_PRE_POST_SCALE_5_B0));
    ath_hal_printf(ah, "BB_paprd_pre_post_scale_6_b0= 0x%08x\n", OS_REG_READ(ah,
            AR_PHY_PAPRD_PRE_POST_SCALE_6_B0));
    ath_hal_printf(ah, "BB_paprd_pre_post_scale_7_b0= 0x%08x\n", OS_REG_READ(ah,
            AR_PHY_PAPRD_PRE_POST_SCALE_7_B0));
}
#endif


#ifdef HAL_PAPD_DEBUG
static void AR9300PAPDDebugPrint(struct ath_hal *ah)
{
    int temp;
    int txbb1dbgain, txbb6dbgain, txmxrgain;
    int padrvgnA, padrvgnB, padrvgnC, padrvgnD;

    if (AR_SREV_POSEIDON(ah)) {
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_lb_skip", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_LB_SKIP);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl1.cf_paprd_lb_skip=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_lb_enable", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_LB_ENABLE);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl1.cf_paprd_lb_enable=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_tx_gain_force", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_TX_GAIN_FORCE);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl1.cf_paprd_tx_gain_force=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_rx_bb_gain_force", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_RX_BB_GAIN_FORCE);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl1.cf_paprd_rx_bb_gain_force=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_iqcorr_enable", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_IQCORR_ENABLE);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl1.cf_paprd_iqcorr_enable=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_agc2_settling", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_AGC2_SETTLING);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl1.cf_paprd_agc2_settling=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_train_enable", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_CF_PAPRD_TRAIN_ENABLE);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl1.cf_paprd_train_enable=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl2.cf_paprd_init_rx_bb_gain", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL2_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL2_CF_PAPRD_INIT_RX_BB_GAIN);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl2.cf_paprd_init_rx_bb_gain=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_fine_corr_len", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_FINE_CORR_LEN);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl3.cf_paprd_fine_corr_len=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_coarse_corr_len", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_COARSE_CORR_LEN);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl3.cf_paprd_coarse_corr_len=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_num_corr_stages", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_NUM_CORR_STAGES);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl3.cf_paprd_num_corr_stages=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_min_loopback_del", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_MIN_LOOPBACK_DEL);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl3.cf_paprd_min_loopback_del=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_quick_drop", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_QUICK_DROP);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl3.cf_paprd_quick_drop=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_adc_desired_size", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_ADC_DESIRED_SIZE);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl3.cf_paprd_adc_desired_size=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_bbtxmix_disable", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_BBTXMIX_DISABLE);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl3.cf_paprd_bbtxmix_disable=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl4.cf_paprd_safety_delta", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL4_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL4_CF_PAPRD_SAFETY_DELTA);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl4.cf_paprd_safety_delta=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl4.cf_paprd_min_corr", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL4_POSEIDON,
            AR_PHY_PAPRD_TRAINER_CNTL4_CF_PAPRD_MIN_CORR);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl4.cf_paprd_min_corr=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_agc2_pwr", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_AGC2_PWR);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    paprd_agc2_pwr              = 0x%02x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_rx_gain_idx", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_RX_GAIN_IDX);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    paprd_rx_gain_idx           = 0x%02x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_train_active", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_TRAIN_ACTIVE);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    paprd_train_active          = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_corr_err", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_CORR_ERR);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    paprd_corr_err              = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_train_incomplete", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_TRAIN_INCOMPLETE);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    paprd_train_incomplete      = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_train_done", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_TRAIN_DONE);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    paprd_train_done            = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat2.paprd_fine_idx", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT2_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT2_PAPRD_FINE_IDX);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    paprd_fine_idx              = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat2.paprd_coarse_idx", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT2_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT2_PAPRD_COARSE_IDX);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    paprd_coarse_idx            = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat2.paprd_fine_val", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT2_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT2_PAPRD_FINE_VAL);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    paprd_fine_val              = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat3.paprd_train_samples_cnt", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT3_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT3_PAPRD_TRAIN_SAMPLES_CNT);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    paprd_train_samples_cnt     = 0x%08x\n", temp);
    } else {
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_lb_skip", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_LB_SKIP);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl1.cf_paprd_lb_skip=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_lb_enable", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_LB_ENABLE);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl1.cf_paprd_lb_enable=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_tx_gain_force", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_TX_GAIN_FORCE);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl1.cf_paprd_tx_gain_force=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_rx_bb_gain_force", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_RX_BB_GAIN_FORCE);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl1.cf_paprd_rx_bb_gain_force=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_iqcorr_enable", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_IQCORR_ENABLE);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl1.cf_paprd_iqcorr_enable=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_agc2_settling", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_PAPRD_AGC2_SETTLING);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl1.cf_paprd_agc2_settling=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl1.cf_paprd_train_enable", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL1,
            AR_PHY_PAPRD_TRAINER_CNTL1_CF_CF_PAPRD_TRAIN_ENABLE);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl1.cf_paprd_train_enable=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl2.cf_paprd_init_rx_bb_gain", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL2,
            AR_PHY_PAPRD_TRAINER_CNTL2_CF_PAPRD_INIT_RX_BB_GAIN);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl2.cf_paprd_init_rx_bb_gain=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_fine_corr_len", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_FINE_CORR_LEN);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl3.cf_paprd_fine_corr_len=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_coarse_corr_len", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_COARSE_CORR_LEN);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl3.cf_paprd_coarse_corr_len=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_num_corr_stages", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_NUM_CORR_STAGES);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl3.cf_paprd_num_corr_stages=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_min_loopback_del", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_MIN_LOOPBACK_DEL);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl3.cf_paprd_min_loopback_del=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_quick_drop", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_QUICK_DROP);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl3.cf_paprd_quick_drop=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_adc_desired_size", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_ADC_DESIRED_SIZE);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl3.cf_paprd_adc_desired_size=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl3.cf_paprd_bbtxmix_disable", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_BBTXMIX_DISABLE);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl3.cf_paprd_bbtxmix_disable=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl4.cf_paprd_safety_delta", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL4,
            AR_PHY_PAPRD_TRAINER_CNTL4_CF_PAPRD_SAFETY_DELTA);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl4.cf_paprd_safety_delta=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_cntl4.cf_paprd_min_corr", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL4,
            AR_PHY_PAPRD_TRAINER_CNTL4_CF_PAPRD_MIN_CORR);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "BB_paprd_trainer_cntl4.cf_paprd_min_corr=0x%x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_agc2_pwr", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_AGC2_PWR);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    paprd_agc2_pwr              = 0x%02x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_rx_gain_idx", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_RX_GAIN_IDX);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    paprd_rx_gain_idx           = 0x%02x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_train_active", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_TRAIN_ACTIVE);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    paprd_train_active          = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_corr_err", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_CORR_ERR);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    paprd_corr_err              = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_train_incomplete", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_TRAIN_INCOMPLETE);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    paprd_train_incomplete      = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat1.paprd_train_done", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_TRAIN_DONE);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    paprd_train_done            = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat2.paprd_fine_idx", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT2,
            AR_PHY_PAPRD_TRAINER_STAT2_PAPRD_FINE_IDX);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    paprd_fine_idx              = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat2.paprd_coarse_idx", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT2,
            AR_PHY_PAPRD_TRAINER_STAT2_PAPRD_COARSE_IDX);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    paprd_coarse_idx            = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat2.paprd_fine_val", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT2,
            AR_PHY_PAPRD_TRAINER_STAT2_PAPRD_FINE_VAL);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    paprd_fine_val              = 0x%08x\n", temp);
        //FieldRead("BB_paprd_trainer_stat3.paprd_train_samples_cnt", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT3,
            AR_PHY_PAPRD_TRAINER_STAT3_PAPRD_TRAIN_SAMPLES_CNT);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    paprd_train_samples_cnt     = 0x%08x\n", temp);
    }

    //FieldRead("BB_tpc_1.force_dac_gain", &temp);
    temp =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TPC_1, AR_PHY_TPC_1_FORCE_DAC_GAIN);
    HDPRINTF(ah, HAL_DBG_CALIBRATE, "    dac_gain_forced     = 0x%08x\n",
        temp);
    //FieldRead("BB_tpc_1.forced_dac_gain", &temp);
    temp =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TPC_1, AR_PHY_TPC_1_FORCED_DAC_GAIN);
    HDPRINTF(ah, HAL_DBG_CALIBRATE, "    forced_dac_gain     = 0x%08x\n",
        temp);

    //FieldRead("BB_paprd_ctrl0_b0.paprd_enable_0", &temp);
    temp =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B0,
        AR_PHY_PAPRD_CTRL0_B0_PAPRD_ENABLE_0);
    HDPRINTF(ah, HAL_DBG_CALIBRATE,
        "    BB_paprd_ctrl0_b0.paprd_enable_0     = 0x%08x\n", temp);
    if (!AR_SREV_POSEIDON(ah)) {
        //FieldRead("BB_paprd_ctrl0_b1.paprd_enable_1", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B1,
            AR_PHY_PAPRD_CTRL0_B1_PAPRD_ENABLE_1);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    BB_paprd_ctrl0_b1.paprd_enable_1     = 0x%08x\n", temp);
        //FieldRead("BB_paprd_ctrl0_b2.paprd_enable_2", &temp);
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL0_B2,
            AR_PHY_PAPRD_CTRL0_B2_PAPRD_ENABLE_2);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "    BB_paprd_ctrl0_b2.paprd_enable_2     = 0x%08x\n", temp);
    }

    //FieldRead("BB_tx_forced_gain.forced_txbb1dbgain", &txbb1dbgain);
    txbb1dbgain =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_TXBB1DBGAIN);
    //FieldRead("BB_tx_forced_gain.forced_txbb6dbgain", &txbb6dbgain);
    txbb6dbgain =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_TXBB6DBGAIN);
    //FieldRead("BB_tx_forced_gain.forced_txmxrgain", &txmxrgain);
    txmxrgain =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_TXMXRGAIN);
    //FieldRead("BB_tx_forced_gain.forced_padrvgnA", &padrvgnA);
    padrvgnA =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_PADRVGNA);
    //FieldRead("BB_tx_forced_gain.forced_padrvgnB", &padrvgnB);
    padrvgnB =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_PADRVGNB);
    //FieldRead("BB_tx_forced_gain.forced_padrvgnC", &padrvgnC);
    padrvgnC =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_PADRVGNC);
    //FieldRead("BB_tx_forced_gain.forced_padrvgnD", &padrvgnD);
    padrvgnD =
        OS_REG_READ_FIELD_ALT(ah, AR_PHY_TX_FORCED_GAIN,
        AR_PHY_TX_FORCED_GAIN_FORCED_PADRVGND);

    HDPRINTF(ah, HAL_DBG_CALIBRATE,
        "txbb1dbgain=0x%x, txbb6dbgain=0x%x, txmxrgain=0x%x\n", txbb1dbgain,
        txbb6dbgain, txmxrgain);
    HDPRINTF(ah, HAL_DBG_CALIBRATE, "padrvgnA=0x%x, padrvgnB=0x%x\n", padrvgnA,
        padrvgnB);
    HDPRINTF(ah, HAL_DBG_CALIBRATE, "padrvgnC=0x%x, padrvgnD=0x%x\n", padrvgnC,
        padrvgnD);
}
#endif

static int Ar9300CreatePACurve(struct ath_hal *ah, u_int32_t * PA_table,
    u_int32_t * smallSignalGain, int * PA_in)
{
    int i;

    //char fieldName[100];
    u_int32_t PaprdTrainDataL[48], PaprdTrainDataU[48];
    u_int32_t reg;
    int status;

#if defined(ART_PAPD_DEBUG) || defined(HAL_PAPD_DEBUG)
    AR9300PAPDDebugPrint(ah);
#endif
    //FieldWrite("BB_chaninfo_ctrl.chaninfomem_s2_read", 0x0);
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_CHAN_INFO_MEMORY,
        AR_PHY_CHAN_INFO_MEMORY_CHANINFOMEM_S2_READ, 0);
    reg = AR_PHY_CHAN_INFO_TAB_0;

    for (i = 0; i < 48; i++) {
        /*
         * sprintf(
         *     fieldName, "%s%d%s\0", "BB_chan_info_chan_tab_b0[",
         *     i, "].chaninfo_word");
         */
        //FieldRead(fieldName, &PaprdTrainDataL[i]);
        PaprdTrainDataL[i] = OS_REG_READ(ah, reg);
        reg = reg + 4;
    }

    //FieldWrite("BB_chaninfo_ctrl.chaninfomem_s2_read", 0x1);
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_CHAN_INFO_MEMORY,
        AR_PHY_CHAN_INFO_MEMORY_CHANINFOMEM_S2_READ, 1);
    reg = AR_PHY_CHAN_INFO_TAB_0;

    for (i = 0; i < 48; i++) {
        /*
         * sprintf(
         *     fieldName, "%s%d%s\0", "BB_chan_info_chan_tab_b0[",
         *     i, "].chaninfo_word");
         */
        // FieldRead(fieldName, &PaprdTrainDataU[i]);
        PaprdTrainDataU[i] = OS_REG_READ(ah, reg);
        reg = reg + 4;
    }

    /*
     * for(i=0; i<48; i++)
     *     ath_hal_printf(
     *         ah, "%08x%08x\n", PaprdTrainDataU[i], PaprdTrainDataL[i]);
     */
    status = 0;
    if (create_PA_curve(
            PaprdTrainDataL, PaprdTrainDataU,
            PA_table, smallSignalGain, PA_in) ==
            AH_FALSE)
    {
        status = -2;
    }
    // Clear the training done bit
    // FieldWrite("BB_paprd_trainer_stat1.paprd_train_done",0);
    if (AR_SREV_POSEIDON(ah)) {
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1_POSEIDON,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_TRAIN_DONE, 0);
    } else {
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1,
            AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_TRAIN_DONE, 0);
    }
    return status;
}

static int find_expn(int num)
{
    int tmp, exp;

    exp = 0;
    tmp = num >> 1;

    while (tmp != 0) {
        tmp = tmp >> 1;
        exp++;
    }

    return exp;
}

static int find_proper_scale(int expn, int N)
{
    int Q_pw;

    Q_pw = (expn > N) ? expn - 10 : 0;
    return Q_pw;
}

static int find_max(int *array, int length)
{
    int i, loc_max;

    loc_max = 0;

    for (i = 0; i < length; i++) {
        if (array[i] > loc_max) {
            loc_max = array[i];
        }
    }

    return loc_max;
}

static int paprd_abs(int num)
{
    if (num < 0) {
        return -num;
    }
    return num;
}

#define NUM_BIN 23

HAL_BOOL create_PA_curve(u_int32_t * PaprdTrainDataL,
    u_int32_t * PaprdTrainDataU, u_int32_t * PA_table, u_int32_t * G_fxp_ext,
    int * PA_in)
{
    unsigned int accum_cnt[NUM_BIN + 1], accum_tx[NUM_BIN + 1],
        accum_rx[NUM_BIN + 1], accum_ang[NUM_BIN + 1], thresh_accum_cnt;
    int x_est[NUM_BIN + 1], Y[NUM_BIN + 1], max_index, scale_factor,
        theta[NUM_BIN + 1];
    int y_sqr[NUM_BIN + 1], y_quad[NUM_BIN + 1], //PA_in[NUM_BIN + 1],
        theta_tilde[NUM_BIN + 1], PA_angle[NUM_BIN + 1];
    int B1_tmp[NUM_BIN + 1], B2_tmp[NUM_BIN + 1], B1_abs[NUM_BIN + 1],
        B2_abs[NUM_BIN + 1];
    int Y_lin[NUM_BIN + 1], y_est[NUM_BIN + 1], x_est_fxp1_nonlin[NUM_BIN + 1],
        x_tilde[NUM_BIN + 1], x_tilde_abs[NUM_BIN + 1];
    int G_fxp, Y_intercept, order_x_by_y, M, I, L, sum_y_sqr, sum_y_quad, Q_x,
        Q_B1, Q_B2, beta_raw, alpha_raw, scale_B;
    int Q_scale_B, Q_beta, Q_alpha, alpha, beta, order_1, order_2, order1_5x,
        order2_3x, order1_5x_rem, order2_3x_rem, y5, y3, tmp;
    int i;
    int theta_low_bin = 0;

    // [15:00] u16, accum_cnt[15:00]: number of samples in the bin
    // [42:16] u27, accum_tx[26:00]: sum(tx amplitude) of the bin
    /*
     * [63:43] u21, accum_rx[20:00]:
     *     sum(rx amplitude distance to lower bin edge) of the bin
     */
    // [90:64] s27, accum_ang[26:00]: sum(angles) of the bin
    max_index = 0;
    /*
     * Disregard any bin that contains less than
     * or equal to 16 counts of samples
     */
    thresh_accum_cnt = 16;
    scale_factor = 5;

    for (i = 0; i < NUM_BIN; i++) {
        accum_cnt[i] = PaprdTrainDataL[i] & 0xffff;
        // lower 16 bit ORed  upper 11 bits
        accum_tx[i] =
            ((PaprdTrainDataL[i] >> 16) & 0xffff) |
            ((PaprdTrainDataU[i] & 0x7ff) << 16);
        accum_rx[i] =
            ((PaprdTrainDataU[i] >> 11) & 0x1f) | ((PaprdTrainDataL[i +
                    23] & 0xffff) << 5);
        accum_ang[i] =
            ((PaprdTrainDataL[i + 23] >> 16) & 0xffff) | ((PaprdTrainDataU[i +
                    23] & 0x7ff) << 16);
        /*
         * printf(
         *     "%d\t%d\t%d\t%d\n", accum_cnt[i], accum_tx[i],
         *     accum_rx[i], accum_ang[i]);
         */

        if (accum_cnt[i] > thresh_accum_cnt) {
            /* accum_cnt[i] will be non-zero at this point */
            x_est[i + 1] =
                ((((accum_tx[i] << scale_factor) +
                        accum_cnt[i]) / accum_cnt[i]) + 32) >> scale_factor;
            Y[i + 1] =
                (((((accum_rx[i] << scale_factor) +
                            accum_cnt[i]) / accum_cnt[i]) +
                    32) >> scale_factor) + (1 << scale_factor) * max_index +
                16;
            if (accum_ang[i] >= (1 << 26)) {
                theta[i + 1] =
                    ((accum_ang[i] - (1 << 27)) * (1 << scale_factor) +
                    accum_cnt[i]);
                theta[i + 1] = theta[i + 1] / (int) accum_cnt[i];
                /*
                 *  theta[i+1] =
                 *      ((accum_ang[i] - (1 << 27)) *
                 *      (1 << scale_factor) + zz) / zz;
                 */
            } else {
                theta[i + 1] =
                    ((accum_ang[i] * (1 << scale_factor)) +
                    accum_cnt[i]) / accum_cnt[i];
            }

            max_index++;
        }
        /*
         * printf(
         *     "i=%d, theta[i+1]=%d\t%d\t%d\t%d\t%d\n",
         *     i, theta[i+1], accum_cnt[i],
         *     accum_tx[i], accum_rx[i], accum_ang[i]);
         */
    }

    /*
     * Find average theta of first 5 bin and all of those to same value.
     * Curve is linear at that range.
     */
    {
        for (i = 1; i < 6; i++) {
            theta_low_bin += theta[i];
        }
        theta_low_bin = theta_low_bin / 5;
        for (i = 1; i < 6; i++) {
            theta[i] = theta_low_bin;
        }
    }

    // Set values at origin
    theta[0] = theta_low_bin;

    for (i = 0; i <= max_index; i++) {
        theta[i] = theta[i] - theta_low_bin;
        // printf("i=%d, theta[i] = %d\n", i,theta[i]);
    }

    x_est[0] = 0;
    Y[0] = 0;
    scale_factor = 8;
    // low signal gain
    if (x_est[6] == x_est[3]) {
        return AH_FALSE;
    }
    G_fxp =
        (((Y[6] - Y[3]) * 1 << scale_factor) + (x_est[6] -
            x_est[3])) / (x_est[6] - x_est[3]);
    if (G_fxp == 0) {
        /*
         * ath_hal_printf(
         *     NULL, "%s[%d] Potential divide by zero error\n",
         *     __func__, __LINE__);
         */
        return AH_FALSE;
    }

    for (i = 0; i <= max_index; i++) {
        Y_lin[i] =
            (G_fxp * (x_est[i] - x_est[3]) +
            (1 << scale_factor)) / (1 << scale_factor) + Y[3];
    }
    Y_intercept = Y_lin[0];

    for (i = 0; i <= max_index; i++) {
        y_est[i] = Y[i] - Y_intercept;
        Y_lin[i] = Y_lin[i] - Y_intercept;
    }

    for (i = 0; i <= 3; i++) {
        y_est[i] = i * 32;
        /* G_fxp was checked for zero already */
        x_est[i] = ((y_est[i] * 1 << scale_factor) + G_fxp) / G_fxp;
    }

    /*
     *  for(i=0; i<=max_index; i++) {
     *      printf("y_est[%d] = %d, x_est[%d]=%d\n", i, y_est[i], i, x_est[i]);
     *  }
     */
    for (i = 0; i <= max_index; i++) {
        x_est_fxp1_nonlin[i] =
            x_est[i] - ((1 << scale_factor) * y_est[i] + G_fxp) / G_fxp;
        //  printf("x_est_fxp1_nonlin[%d] = %d\n", i, x_est_fxp1_nonlin[i]);
    }

    /* Check for divide by 0 */
    if (y_est[max_index] == 0) {
        return AH_FALSE;
    }
    order_x_by_y =
        (x_est_fxp1_nonlin[max_index] + y_est[max_index]) / y_est[max_index];
    if (order_x_by_y == 0) {
        M = 10;
    } else if (order_x_by_y == 1) {
        M = 9;
    } else {
        M = 8;
    }

    I = (max_index > 15) ? 7 : max_index >> 1;
    L = max_index - I;
    scale_factor = 8;
    sum_y_sqr = 0;
    sum_y_quad = 0;

    for (i = 0; i <= L; i++) {
        if (y_est[i + I] == 0) {
            /*
             * ath_hal_printf(
             *     NULL, "%s Potential divide by zero error\n", __func__);
             */
            return AH_FALSE;
        }

        x_tilde[i] =
            (x_est_fxp1_nonlin[i + I] * (1 << M) + y_est[i + I]) / y_est[i +
            I];
        x_tilde[i] = (x_tilde[i] * (1 << M) + y_est[i + I]) / y_est[i + I];
        x_tilde[i] = (x_tilde[i] * (1 << M) + y_est[i + I]) / y_est[i + I];

        y_sqr[i] =
            (y_est[i + I] * y_est[i + I] +
            (scale_factor * scale_factor)) / (scale_factor * scale_factor);
        x_tilde_abs[i] = paprd_abs(x_tilde[i]);
        y_quad[i] = y_sqr[i] * y_sqr[i];
        sum_y_sqr = sum_y_sqr + y_sqr[i];
        sum_y_quad = sum_y_quad + y_quad[i];
    }

    // printf("sum_y_sqr = %d, sum_y_quad=%d\n", sum_y_sqr, sum_y_quad);

    for (i = 0; i <= L; i++) {
        B1_tmp[i] = y_sqr[i] * (L + 1) - sum_y_sqr;
        B2_tmp[i] = sum_y_quad - sum_y_sqr * y_sqr[i];
        B1_abs[i] = paprd_abs(B1_tmp[i]);
        B2_abs[i] = paprd_abs(B2_tmp[i]);

        /*
         * printf(
         *     "i=%d, B1_tmp[i] = %d, B2_tmp[i] = %d\n",
         *     i, B1_tmp[i], B2_tmp[i]);
         */
    }

    Q_x = find_proper_scale(find_expn(find_max(x_tilde_abs, L + 1)), 10);
    Q_B1 = find_proper_scale(find_expn(find_max(B1_abs, L + 1)), 10);
    Q_B2 = find_proper_scale(find_expn(find_max(B2_abs, L + 1)), 10);

    beta_raw = 0;
    alpha_raw = 0;

    for (i = 0; i <= L; i++) {
        x_tilde[i] = x_tilde[i] / (1 << Q_x);
        B1_tmp[i] = B1_tmp[i] / (1 << Q_B1);
        B2_tmp[i] = B2_tmp[i] / (1 << Q_B2);

        /*
         * printf(
         *     "i=%d, B1_tmp[i]=%d B2_tmp[i]=%d x_tilde[i] = %d\n",
         *     i, B1_tmp[i], B2_tmp[i], x_tilde[i]);
         */
        beta_raw = beta_raw + B1_tmp[i] * x_tilde[i];
        alpha_raw = alpha_raw + B2_tmp[i] * x_tilde[i];
    }

    scale_B =
        ((sum_y_quad / scale_factor) * (L + 1) -
        (sum_y_sqr / scale_factor) * sum_y_sqr) * scale_factor;
    Q_scale_B = find_proper_scale(find_expn(paprd_abs(scale_B)), 10);
    scale_B = scale_B / (1 << Q_scale_B);
    /* Check for divide by 0 */
    if (scale_B == 0) {
        return AH_FALSE;
    }
    Q_beta = find_proper_scale(find_expn(paprd_abs(beta_raw)), 10);
    Q_alpha = find_proper_scale(find_expn(paprd_abs(alpha_raw)), 10);

    beta_raw = beta_raw / (1 << Q_beta);
    alpha_raw = alpha_raw / (1 << Q_alpha);
    alpha = (alpha_raw << 10) / scale_B;
    beta = (beta_raw << 10) / scale_B;
    order_1 = 3 * M - Q_x - Q_B1 - Q_beta + 10 + Q_scale_B;
    order_2 = 3 * M - Q_x - Q_B2 - Q_alpha + 10 + Q_scale_B;

    order1_5x = order_1 / 5;
    order2_3x = order_2 / 3;

    order1_5x_rem = order_1 - 5 * order1_5x;
    order2_3x_rem = order_2 - 3 * order2_3x;

    for (i = 0; i < AR9300_PAPRD_TABLE_SZ; i++) {
        tmp = i * 32;
        y5 = ((beta * tmp) >> 6) >> order1_5x;
        y5 = (y5 * tmp) >> order1_5x;
        y5 = (y5 * tmp) >> order1_5x;
        y5 = (y5 * tmp) >> order1_5x;
        y5 = (y5 * tmp) >> order1_5x;

        y5 = y5 >> order1_5x_rem;
        y3 = (alpha * tmp) >> order2_3x;
        y3 = (y3 * tmp) >> order2_3x;
        y3 = (y3 * tmp) >> order2_3x;

        y3 = y3 >> order2_3x_rem;
        /* G_fxp was checked for zero already */
        PA_in[i] = y5 + y3 + (256 * tmp) / G_fxp;
    }

    for (i = 1; i < 23; i++) {
        tmp = PA_in[i + 1] - PA_in[i];
        if (tmp < 0) {
            PA_in[i + 1] = PA_in[i] + (PA_in[i] - PA_in[i - 1]);
        }
    }

    for (i = 0; i < AR9300_PAPRD_TABLE_SZ; i++) {
        PA_in[i] = (PA_in[i] < 1400) ? PA_in[i] : 1400;
        // printf("i=%d, PA_in[i]=%d\n", i, PA_in[i]);
    }

    beta_raw = 0;
    alpha_raw = 0;

    for (i = 0; i <= L; i++) {
        /*
         *  printf(
         *      "i=%d I=%d M=%d theta[i+I]=%d y_est[i+I]=%d\n",
         *      i, I, M, theta[i+I], y_est[i+I]);
         */
        /* y_est[] was already checked for zero */
        theta_tilde[i] = ((theta[i + I] << M) + y_est[i + I]) / y_est[i + I];
        theta_tilde[i] = ((theta_tilde[i] << M) + y_est[i + I]) / y_est[i + I];
        theta_tilde[i] = ((theta_tilde[i] << M) + y_est[i + I]) / y_est[i + I];

        // printf("i=%d theta_tilde[i]=%d\n", i, theta_tilde[i]);
        beta_raw = beta_raw + B1_tmp[i] * theta_tilde[i];
        alpha_raw = alpha_raw + B2_tmp[i] * theta_tilde[i];

        // printf("i=%d, alpha_raw=%d, beta_raw=%d\n", i, alpha_raw, beta_raw);
    }

    Q_beta = find_proper_scale(find_expn(paprd_abs(beta_raw)), 10);
    Q_alpha = find_proper_scale(find_expn(paprd_abs(alpha_raw)), 10);

    beta_raw = beta_raw / (1 << Q_beta);
    alpha_raw = alpha_raw / (1 << Q_alpha);
    /* scale_B checked for zero previously */
    alpha = (alpha_raw << 10) / scale_B;
    beta = (beta_raw << 10) / scale_B;
    order_1 = 3 * M - Q_x - Q_B1 - Q_beta + 10 + Q_scale_B + 5;
    order_2 = 3 * M - Q_x - Q_B2 - Q_alpha + 10 + Q_scale_B + 5;

    order1_5x = order_1 / 5;
    order2_3x = order_2 / 3;

    order1_5x_rem = order_1 - 5 * order1_5x;
    order2_3x_rem = order_2 - 3 * order2_3x;

    for (i = 0; i < AR9300_PAPRD_TABLE_SZ; i++) {
        tmp = i * 32;

        if (beta > 0) {
            y5 = (((beta * tmp - 64) >> 6) -
                (1 << order1_5x)) / (1 << order1_5x);
        } else {
            y5 = ((((beta * tmp - 64) >> 6) +
                    (1 << order1_5x)) / (1 << order1_5x));
        }

        y5 = (y5 * tmp) / (1 << order1_5x);
        y5 = (y5 * tmp) / (1 << order1_5x);
        y5 = (y5 * tmp) / (1 << order1_5x);
        y5 = (y5 * tmp) / (1 << order1_5x);

        y5 = y5 / (1 << order1_5x_rem);

        if (beta > 0) {
            y3 = (alpha * tmp - (1 << order2_3x)) / (1 << order2_3x);
        } else {
            y3 = (alpha * tmp + (1 << order2_3x)) / (1 << order2_3x);
        }

        y3 = (y3 * tmp) / (1 << order2_3x);
        y3 = (y3 * tmp) / (1 << order2_3x);

        y3 = y3 / (1 << order2_3x_rem);
        PA_angle[i] = y5 + y3;
        //printf("i=%d, y5 = %d, y3=%d\n", i, y5, y3);
        PA_angle[i] =
            (PA_angle[i] < -150) ? -150 : ((PA_angle[i] >
                150) ? 150 : PA_angle[i]);
    }

    PA_angle[0] = 0;
    PA_angle[1] = 0;
    PA_angle[2] = 0;
    PA_angle[3] = 0;

    PA_angle[4] = (PA_angle[5] + 2) >> 1;

    for (i = 0; i < AR9300_PAPRD_TABLE_SZ; i++) {
        PA_table[i] = ((PA_in[i] & 0x7ff) << 11) + (PA_angle[i] & 0x7ff);
        /*
         * HDPRINTF(
         *     NULL, HAL_DBG_UNMASKABLE,"%d\t%d\t0x%x\n",
         *     PA_in[i], PA_angle[i], PA_table[i]);
         */
    }

    //HDPRINTF(NULL, HAL_DBG_UNMASKABLE, "G_fxp = %d\n", G_fxp);
    *G_fxp_ext = G_fxp;
    return AH_TRUE;
}

// Due to a hardware bug, when transmitting with just one chain the papd
// data for chain 0 is always used. So when using chain 2 or 4, the 
// corresponding data must be copied into the chain 0 area.
void ar9300SwizzlePaprdEntries(struct ath_hal *ah, unsigned int txchain)
{
	int i;
	u_int32_t *paprd_table_val = NULL;
    u_int32_t small_signal_gain = 0;
    u_int32_t reg = 0;

	reg = AR_PHY_PAPRD_MEM_TAB_B0;	
	switch (txchain) {
		case 0x1: 
		case 0x3:
		case 0x7:
			paprd_table_val = &AH9300(ah)->PA_table[0][0];
			small_signal_gain = AH9300(ah)->smallSignalGain[0];
			break;
		case 0x2:
			paprd_table_val = &AH9300(ah)->PA_table[1][0];
			small_signal_gain = AH9300(ah)->smallSignalGain[1];
			break;
		case 0x4:
			paprd_table_val = &AH9300(ah)->PA_table[2][0];
			small_signal_gain = AH9300(ah)->smallSignalGain[2];
			break;
		default:
			// Error out.
			ath_hal_printf(ah, "YAK! Bad chain mask %x\n", txchain);
			return;
	}
	for (i = 0; i < AR9300_PAPRD_TABLE_SZ; i++) { 
        OS_REG_WRITE(ah, reg, paprd_table_val[i]);
        HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s[%d] reg %08x = 0x%08x\n", __func__,
            __LINE__, reg, paprd_table_val[i]);
        
        reg = reg + 4;
    }
	OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PA_GAIN123_B0,	AR_PHY_PA_GAIN123_B0_PA_GAIN1_0, small_signal_gain);
    HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s[%d] reg %08x small_signal_gain 0x%08x\n", __func__, __LINE__,
		(unsigned) AR_PHY_PA_GAIN123_B0, OS_REG_READ(ah, AR_PHY_PA_GAIN123_B0));

	OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B0, AR_PHY_PAPRD_CTRL1_B0_PAPRD_POWER_AT_AM2AM_CAL_0,
		AH9300(ah)->paprd_training_power);
    HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s[%d] reg %08x = 0x%08x\n", __func__, __LINE__, 
		(unsigned) AR_PHY_PAPRD_CTRL1_B0, OS_REG_READ(ah, AR_PHY_PAPRD_CTRL1_B0));

}

void ar9300PopulatePaprdSingleTable(struct ath_hal *ah, HAL_CHANNEL * chan,
    int chainNum)
{
    int i;
    u_int32_t *paprd_table_val = &AH9300(ah)->PA_table[chainNum][0];
    u_int32_t small_signal_gain = AH9300(ah)->smallSignalGain[chainNum];
    u_int32_t reg = 0;

    HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s[%d]: channel %d paprdDone %d write %d\n", __func__,
        __LINE__, chan->channel, chan->paprdDone, chan->paprdTableWriteDone);

    if (chainNum == 0) {
        reg = AR_PHY_PAPRD_MEM_TAB_B0;
    } else if (chainNum == 1) {
        reg = AR_PHY_PAPRD_MEM_TAB_B1;
    } else if (chainNum == 2) {
        reg = AR_PHY_PAPRD_MEM_TAB_B2;
    }

    for (i = 0; i < AR9300_PAPRD_TABLE_SZ; i++) {
        if (AR_SREV_POSEIDON(ah)) {
            HALASSERT(chainNum == 0x1);
            if ((reg == AR_PHY_PAPRD_MEM_TAB_B1) ||
                (reg == AR_PHY_PAPRD_MEM_TAB_B2)) {
                continue;
            }
        }
        /*
         * sprintf(
         *     fieldName, "%s%d[%d]%s\0", "BB_paprd_mem_tab_b",
         *     chainNum, i, ".paprd_mem");
         */
        //FieldWrite(fieldName, paprd_table_val[i]);
        OS_REG_WRITE(ah, reg, paprd_table_val[i]);
        HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s[%d] reg %08x = 0x%08x\n", __func__,
            __LINE__, reg, paprd_table_val[i]);
        /*
         * ath_hal_printf(ah, 
         *     "%s[%d] reg %08x = 0x%08x\n",
         *     __func__, __LINE__, reg, paprd_table_val[i]);
         */
        reg = reg + 4;
    }

    if (chainNum == 0) {
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PA_GAIN123_B0,
            AR_PHY_PA_GAIN123_B0_PA_GAIN1_0, small_signal_gain);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "%s[%d] reg %08x small_signal_gain 0x%08x\n", __func__, __LINE__,
            (unsigned) AR_PHY_PA_GAIN123_B0, OS_REG_READ(ah,
                AR_PHY_PA_GAIN123_B0));
    } else if (chainNum == 1) {
        if (!AR_SREV_POSEIDON(ah) && !AR_SREV_HORNET(ah)) {
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PA_GAIN123_B1,
                AR_PHY_PA_GAIN123_B1_PA_GAIN1_1, small_signal_gain);
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "%s[%d] reg %08x small_signal_gain 0x%08x\n", __func__, __LINE__,
                (unsigned) AR_PHY_PA_GAIN123_B1, OS_REG_READ(ah,
                    AR_PHY_PA_GAIN123_B1));
        }
    } else if (chainNum == 2) {
        if (!AR_SREV_POSEIDON(ah) && !AR_SREV_HORNET(ah)) {
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PA_GAIN123_B2,
                AR_PHY_PA_GAIN123_B2_PA_GAIN1_2, small_signal_gain);
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "%s[%d] reg %08x small_signal_gain 0x%08x\n", __func__, __LINE__,
                (unsigned) AR_PHY_PA_GAIN123_B2, OS_REG_READ(ah,
                    AR_PHY_PA_GAIN123_B2));
        }
    } else {
        // invalid channel number
    }

    /*
     * FieldWrite(
     *     "BB_paprd_ctrl1_b0.paprd_power_at_am2am_cal_0", training_power);
     */
    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B0,
        AR_PHY_PAPRD_CTRL1_B0_PAPRD_POWER_AT_AM2AM_CAL_0, AH9300(ah)->paprd_training_power);
    HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s[%d] reg %08x = 0x%08x\n", __func__,
        __LINE__, (unsigned) AR_PHY_PAPRD_CTRL1_B0, OS_REG_READ(ah,
            AR_PHY_PAPRD_CTRL1_B0));

    if (!AR_SREV_POSEIDON(ah) && !AR_SREV_HORNET(ah)) {
        /*
         * FieldWrite(
         *     "BB_paprd_ctrl1_b1.paprd_power_at_am2am_cal_1", training_power);
         */
        OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B1,
            AR_PHY_PAPRD_CTRL1_B1_PAPRD_POWER_AT_AM2AM_CAL_1, AH9300(ah)->paprd_training_power);
        HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s[%d] reg %08x = 0x%08x\n", __func__,
            __LINE__, (unsigned) AR_PHY_PAPRD_CTRL1_B1, OS_REG_READ(ah,
                AR_PHY_PAPRD_CTRL1_B1));
        /*
         * FieldWrite(
         *     "BB_paprd_ctrl1_b2.paprd_power_at_am2am_cal_2", training_power);
         */
        if (!AR_SREV_WASP(ah)) {
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_CTRL1_B2,
                AR_PHY_PAPRD_CTRL1_B2_PAPRD_POWER_AT_AM2AM_CAL_2, AH9300(ah)->paprd_training_power);
            HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s[%d] reg %08x = 0x%08x\n", __func__,
                __LINE__, (unsigned) AR_PHY_PAPRD_CTRL1_B2, OS_REG_READ(ah,
                    AR_PHY_PAPRD_CTRL1_B2));
        }
    }


    //Ar9300EnablePAPD(ah, AH_TRUE);
}

HAL_STATUS ar9300PaprdSetupGainTable(struct ath_hal *ah, int chainNum)
{
    unsigned int i, desired_gain, gain_index;
    HDPRINTF(ah, HAL_DBG_CALIBRATE,
        "Run papredistortion single table algorithm:: Training power = %d\n",
        AH9300(ah)->paprd_training_power / 2);

    if (AH9300(ah)->ah_txchainmask & (1 << chainNum)) {
        // this is an active chain
        desired_gain =
            Ar9300GetDesiredGainForChain(ah, chainNum, AH9300(ah)->paprd_training_power);
        //find out gain index
        gain_index = 0;

        for (i = 0; i < 32; i++) {
            if (AH9300(ah)->paprd_gain_table_index[i] < desired_gain) {
                gain_index = gain_index + 1;
            } else {
                break;
            }
        }

        //ath_hal_printf(ah, "gain_index = %d\n", gain_index);
        //ath_hal_printf(ah, "++++ gain_index = %d\n", gain_index);
        Ar9300TxForceGain(ah, gain_index);
        if (!AR_SREV_POSEIDON(ah)) {
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1,
                AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_TRAIN_DONE, 0);
        } else {
            OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1_POSEIDON,
                AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_TRAIN_DONE, 0);
        }
    }

    return HAL_OK;
}

HAL_BOOL ar9300_paprd_retrain_pain(struct ath_hal * ah, int * PA_in)
{
    int count = 0, i;
    int capdiv_offset = 0, quick_drop_offset;
    int capdiv2G, quick_drop;

    capdiv2G = (OS_REG_READ(ah, AR_PHY_65NM_CH0_TXRF3) >> 1) & 0xF;
    if (!AR_SREV_POSEIDON(ah)) {
        quick_drop =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
                AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_QUICK_DROP);
    } else {
        quick_drop =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
                AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_QUICK_DROP);
    }

    if ( quick_drop != 0 )
		quick_drop -= 0x40;

    for (i = 0; i < (NUM_BIN + 1); i++) {
        if(PA_in[i] == 1400) {
            count++;
        }
    }

    if ( AR_SREV_POSEIDON(ah)){
        if ((PA_in[23] < 800) || (PA_in[23] == 1400)) {
            if (PA_in[23] < 800) {
                capdiv_offset = ((1000 - PA_in[23] + 75) / 150);
                capdiv2G = capdiv2G + capdiv_offset;
                if (capdiv2G > 7) {
                    capdiv2G = 7;
                    if (PA_in[23] < 600) {
                        quick_drop = quick_drop + 1;
                        if (quick_drop > 0) {
                            quick_drop = 0;
                        }
                    }
			    }
			    OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_TXRF3, 
			        AR_PHY_65NM_CH0_TXRF3_CAPDIV2G, capdiv2G);
                OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
    			    AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_QUICK_DROP,
                    quick_drop);
			    return AH_TRUE;
            } /* end of if (PA_in[23] < 800) */
            else if (PA_in[23] == 1400) {
                quick_drop_offset=(int)(count / 3);
                if (quick_drop_offset > 2) {
                    quick_drop_offset = 2;
                }
                quick_drop = quick_drop + quick_drop_offset;
                capdiv2G = capdiv2G + (int)(quick_drop_offset / 2);
                if (capdiv2G > 7) {
                    capdiv2G = 7;
                }
                if (quick_drop > 0) {
                    quick_drop = 0;
                    capdiv2G = capdiv2G - (int)(quick_drop_offset / 1);
				    if (capdiv2G < 0) {
                        capdiv2G = 0;
				    }
                }
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_TXRF3, 
			        AR_PHY_65NM_CH0_TXRF3_CAPDIV2G, capdiv2G);
                OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3_POSEIDON,
    			    AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_QUICK_DROP,
                    quick_drop);
			    return AH_TRUE;
            } /* end of if (PA_in[23] == 1400)*/
        } /* end of if ((PA_in[23] < 800) || (PA_in[23] == 1400)) */
    }else if (AR_SREV_HORNET(ah)) {
        if ((PA_in[23] < 1000) || (PA_in[23] == 1400)) {
            if (PA_in[23] < 1000) {
                capdiv_offset = ((1000 - PA_in[23]) / 100);
                capdiv2G = capdiv2G + capdiv_offset;
                if (capdiv_offset > 3) {
                    quick_drop_offset = 1;
                    quick_drop = quick_drop - quick_drop_offset;
                    capdiv2G = capdiv2G + 1;
                    if (capdiv2G > 6) {
                        capdiv2G = 6;
                    }
                    if (quick_drop < -4) {
                        quick_drop = -4;
                    }
                    OS_REG_RMW_FIELD(ah,
                        AR_PHY_65NM_CH0_TXRF3,
                        AR_PHY_65NM_CH0_TXRF3_CAPDIV2G,
                        capdiv2G);
                    OS_REG_RMW_FIELD_ALT(ah,
                        AR_PHY_PAPRD_TRAINER_CNTL3,
                        AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_QUICK_DROP,
                        quick_drop);
                    return AH_TRUE;
                } else {
                    capdiv2G = capdiv2G + capdiv_offset;
                    if (capdiv2G > 6) {
                        capdiv2G = 6;
                    }
                    if (quick_drop < -4) {
                        quick_drop = -4;
                    }
                    OS_REG_RMW_FIELD(ah,
                        AR_PHY_65NM_CH0_TXRF3,
                        AR_PHY_65NM_CH0_TXRF3_CAPDIV2G,
                        capdiv2G);
                    OS_REG_RMW_FIELD_ALT(ah,
                        AR_PHY_PAPRD_TRAINER_CNTL3,
                        AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_QUICK_DROP,
                        quick_drop);
                    return AH_TRUE;
                }
            } /* end of if (PA_in[23] < 1000) */
            else if (PA_in[23] == 1400) {
                if (count > 3) {
                    quick_drop_offset = 1;
                    quick_drop = quick_drop + quick_drop_offset;
                    capdiv2G = capdiv2G - (int)(count /4);
                    if (capdiv2G < 0) {
                        capdiv2G = 0;
                    }
                    if (quick_drop > -2) {
                        quick_drop = -2;
                    }
                    OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_TXRF3,
			            AR_PHY_65NM_CH0_TXRF3_CAPDIV2G, capdiv2G);
                    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
			            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_QUICK_DROP, quick_drop);
			        return AH_TRUE;
                } else {
                    capdiv2G = capdiv2G - 1;
                    if (capdiv2G < 0) {
                        capdiv2G = 0;
                    };
                    OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_TXRF3,
			            AR_PHY_65NM_CH0_TXRF3_CAPDIV2G, capdiv2G);
                    OS_REG_RMW_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_CNTL3,
			            AR_PHY_PAPRD_TRAINER_CNTL3_CF_PAPRD_QUICK_DROP, quick_drop);
			        return AH_TRUE;
                }
            } /* end of if (PA_in[23] == 1400)*/
        } /* end of if ((PA_in[23] < 900) || (PA_in[23] == 1400)) */
    }
    return AH_FALSE;
}


HAL_STATUS ar9300PAPRDCreateCurve(struct ath_hal * ah, HAL_CHANNEL * chan,
    int chainNum)
{
    int status = 0;
    u_int32_t *PA_table, smallSignalGain;
    int PA_in[NUM_BIN + 1];

    if (AH9300(ah)->ah_txchainmask & (1 << chainNum)) {
        PA_table = &AH9300(ah)->PA_table[chainNum][0];
        // Compute PA table and gain index
        status =
            Ar9300CreatePACurve(ah, &PA_table[0], &smallSignalGain, &PA_in[0]);
        if (status != 0) {
            ath_hal_printf(ah, "ERROR:: paprd failed with error code = %d\n",
                status);
            return -1;
        }
        AH9300(ah)->smallSignalGain[chainNum] = smallSignalGain;

        if (AR_SREV_POSEIDON(ah) || AR_SREV_HORNET(ah)) {
            if (ar9300_paprd_retrain_pain(ah, PA_in)) {
    		   return HAL_EINPROGRESS;
    		}
        }
    }
    return HAL_OK;
}

int ar9300PAPDInitTable(struct ath_hal *ah, HAL_CHANNEL * chan)
{
    if ((AR_SREV_WASP(ah) && IS_CHAN_5GHZ(chan)) ||
         Ar9300PAPDSetupSingleTable(ah, chan)) {
        goto FAIL;
    }
    OS_MEMZERO(AH9300(ah)->paprd_gain_table_entries,
        sizeof(AH9300(ah)->paprd_gain_table_entries));
    OS_MEMZERO(AH9300(ah)->paprd_gain_table_index,
        sizeof(AH9300(ah)->paprd_gain_table_index));

    Ar9300GainTableEntries(ah);
    return 0;
FAIL:
    return -1;
}

int ar9300PAPRDisDone(struct ath_hal *ah)
{
    int temp, agc2_pwr;

    //FieldRead("BB_paprd_trainer_stat1.paprd_train_done", &temp);
    if (!AR_SREV_POSEIDON(ah)) {
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1,
                AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_TRAIN_DONE);

        if (temp == 0x1) {
            agc2_pwr =
                OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1,
                   AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_AGC2_PWR);

            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                   "%s AGC2_PWR=0x%2x Training done=0x%2x\n",
                   __func__, agc2_pwr, temp);

            /* Retrain if agc2_pwr is not in ideal range */
            if (agc2_pwr <= AH_PAPRD_IDEAL_AGC2_PWR_RANGE) {
                temp = 0;
            }
	}
	
    } else {
        temp =
            OS_REG_READ_FIELD_ALT(ah, AR_PHY_PAPRD_TRAINER_STAT1_POSEIDON,
                AR_PHY_PAPRD_TRAINER_STAT1_PAPRD_TRAIN_DONE);
    }
    if (!temp) {
        //ath_hal_printf(ah, "%s[%d] PAPRD TEMp Error\n", __func__, __LINE__);
    }

    return temp;
}

/*
 * ar9300_paprd_dec_tx_pwr
 *
 * This function do decrease tx power if Paprd is off or train failed.
 */
void 
ar9300_paprd_dec_tx_pwr(struct ath_hal *ah)
{
    u_int32_t   pwr_temp, pwr_reg;
    int         i, loop_cnt;
    struct ar9300_paprd_pwr_adjust  *p_item;
    struct ath_hal_9300 *ahp = AH9300(ah);

    if (AR_SREV_POSEIDON(ah)) {
        loop_cnt = 
        (sizeof(ar9300_paprd_pwr_adj_array) / 
            sizeof(struct ar9300_paprd_pwr_adjust));
        for (i = 0; i < loop_cnt; i++ )
        {
            p_item = &ar9300_paprd_pwr_adj_array[i];
            pwr_reg = OS_REG_READ(ah, p_item->reg_addr);
            pwr_temp = ahp->ah_default_tx_power[p_item->target_rate];
            pwr_temp -= (p_item->sub_dB * 2);
            pwr_temp = pwr_temp << p_item->reg_mask_offset;
            pwr_temp |= (pwr_reg&~(p_item->reg_mask <<p_item->reg_mask_offset));
            if (pwr_temp != pwr_reg) 
            {
                OS_REG_WRITE(ah, p_item->reg_addr, pwr_temp);
            }
        }
    }
    return;
}

int ar9300PAPRDThermalSend(struct ath_hal *ah)
{
    if (AR_SREV_HORNET(ah)) {
        return OS_REG_READ(ah, AR_TFCNT);
    } else {
        return 1;
    }
}

#if 0
void ar9300PAPRDTestPrints(struct ath_hal *ah)
{
    u_int32_t i, reg = 0;

    HDPRINTF(NULL, HAL_DBG_CALIBRATE, "=====ar9300PAPRDTestPrints=======\n");
    //ath_hal_printf(ah, "=====ar9300PAPRDTestPrints=======\n");
    HDPRINTF(NULL, HAL_DBG_CALIBRATE, "BB_paprd_ctrl0_b0 = 0x%08x\n",
        OS_REG_READ(ah, AR_PHY_PAPRD_CTRL0_B0));
    /*
     * ath_hal_printf(ah, 
     *     "BB_paprd_ctrl0_b0 = 0x%08x\n",
     *     OS_REG_READ(ah, AR_PHY_PAPRD_CTRL0_B0));
     */
    if (!AR_SREV_POSEIDON(ah) && !AR_SREV_HORNET(ah)) {
        HDPRINTF(NULL, HAL_DBG_CALIBRATE, "BB_paprd_ctrl0_b1 = 0x%08x\n",
            OS_REG_READ(ah, AR_PHY_PAPRD_CTRL0_B1));
        /*
         * ath_hal_printf(ah, 
         *     "BB_paprd_ctrl0_b1 = 0x%08x\n",
         *     OS_REG_READ(ah, AR_PHY_PAPRD_CTRL0_B1));
         */
        HDPRINTF(NULL, HAL_DBG_CALIBRATE, "BB_paprd_ctrl0_b2 = 0x%08x\n",
            OS_REG_READ(ah, AR_PHY_PAPRD_CTRL0_B2));
        /*
         * ath_hal_printf(ah, 
         *     "BB_paprd_ctrl0_b2 = 0x%08x\n",
         *     OS_REG_READ(ah, AR_PHY_PAPRD_CTRL0_B2));
         */
    }

    reg = AR_PHY_PAPRD_MEM_TAB_B0;
    HDPRINTF(NULL, HAL_DBG_CALIBRATE,
        "%s[%d] reg %08lx small_signal_gain ch0 0x%08x\n", __func__, __LINE__,
        AR_PHY_PA_GAIN123_B0, OS_REG_READ(ah, AR_PHY_PA_GAIN123_B0));
    /*
     * ath_hal_printf(ah, 
     *     "%s[%d] reg %08lx small_signal_gain ch0 0x%08x\n",
     *     __func__,  __LINE__, AR_PHY_PA_GAIN123_B0,
     *     OS_REG_READ(ah, AR_PHY_PA_GAIN123_B0));
     */

    for (i = 0; i < 24; i++) {
        HDPRINTF(NULL, HAL_DBG_CALIBRATE, "%s[%d] reg %08x = 0x%08x\n",
            __func__, __LINE__, reg, OS_REG_READ(ah, reg));
        /*
         * ath_hal_printf(ah, 
         *     "%s[%d] reg %08x = 0x%08x\n", __func__, __LINE__,
         *     reg, OS_REG_READ(ah, reg));
         */
        reg = reg + 4;
    }

    AR9300PAPDDebugPrint(ah);
    HDPRINTF(NULL, HAL_DBG_CALIBRATE,
        "=====ar9300PAPRDTestPrints end=======\n");
    //ath_hal_printf(ah, "=====ar9300PAPRDTestPrints end=======\n");
    reg = AR_PHY_PAPRD_MEM_TAB_B1;
    ath_hal_printf(ah, "%s[%d] reg %08lx small_signal_gain ch1 0x%08x\n", __func__,
        __LINE__, AR_PHY_PA_GAIN123_B1, OS_REG_READ(ah, AR_PHY_PA_GAIN123_B1));
    for (i = 0; i < 24; i++) {
        OS_REG_WRITE(ah, reg, paprd_table_val[i]);
        HDPRINTF(NULL, HAL_DBG_CALIBRATE, "%s[%d] reg %08x = 0x%08x\n",
            __func__, __LINE__, reg, OS_REG_READ(ah, reg));
        ath_hal_printf(ah, "%s[%d] reg %08x = 0x%08x\n", __func__, __LINE__, reg,
            OS_REG_READ(ah, reg));
        reg = reg + 4;
    }

    reg = AR_PHY_PAPRD_MEM_TAB_B2;
    ath_hal_printf(ah, "%s[%d] reg %08lx small_signal_gain ch2 0x%08x\n", __func__,
        __LINE__, AR_PHY_PA_GAIN123_B2, OS_REG_READ(ah, AR_PHY_PA_GAIN123_B2));
}
#endif

#else
int ar9300PAPDInitTable(struct ath_hal *ah, HAL_CHANNEL * chan)
{
    return 0;
}

HAL_STATUS ar9300PaprdSetupGainTable(struct ath_hal * ah, int chainNum)
{
    return HAL_OK;
}

HAL_STATUS ar9300PAPRDCreateCurve(struct ath_hal * ah, HAL_CHANNEL * chan,
    int chainNum)
{
    return HAL_OK;
}

int ar9300PAPRDisDone(struct ath_hal *ah)
{
    return 0;
}

void ar9300EnablePAPD(struct ath_hal *ah, HAL_BOOL enableFlag, HAL_CHANNEL * chan)
{
    return;
}

void ar9300PopulatePaprdSingleTable(struct ath_hal *ah, HAL_CHANNEL * chan,
    int chainNum)
{
    return;
}

void 
ar9300_paprd_dec_tx_pwr(struct ath_hal *ah)
{
    return;
}

int ar9300PAPRDThermalSend(struct ath_hal *ah)
{
    return 1;
}
#endif                          /* ATH_SUPPORT_PAPRD */
