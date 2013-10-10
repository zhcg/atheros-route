/*
 * Copyright (c) 2002-2009 Atheros Communications, Inc.
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

#if ATH_SUPPORT_RAW_ADC_CAPTURE

#define MAX_ADC_SAMPLES                 1024
#define AR9300_MAX_CHAINS               3
#define AR9300_RAW_ADC_SAMPLE_MIN        -512
#define AR9300_RAW_ADC_SAMPLE_MAX        511
#define RXTX_DESC_BUFFER_ADDRESS        0xE000
#define AR9300_MAX_ADC_POWER            5718    // 100 times (10 * log10(2 * AR9300_RAW_ADC_SAMPLE_MAX ^ 2) = 57.1787)

typedef struct adc_sample_t {
        int16_t i;
        int16_t q;
} ADC_SAMPLE;

/*
Recipe for 3 chains ADC capture:
1. tstdac_out_sel=2'b01
2. cf_tx_obs_sel=3'b111
3. cf_tx_obs_mux_sel=2'b11
4. cf_rx_obs_sel=5'b00000
5. cf_bbb_obs_sel=4'b0001
*/

static void ar9300SetupTestAddacMode(struct ath_hal *ah);

/* Chainmask to numchains mapping */
static u_int8_t mask2chains[8] = {
    0 /* 000 */,
    1 /* 001 */,
    1 /* 010 */,
    2 /* 011 */,
    1 /* 100 */,
    2 /* 101 */,
    2 /* 110 */,
    3 /* 111 */};

void
ar9300ShutdownRx(struct ath_hal *ah)
{
    int wait;

#define AH_RX_STOP_TIMEOUT 100000   /* usec */
#define AH_TIME_QUANTUM       100   /* usec */

    //ath_hal_printf(ah, "%s: called\n", __func__);

    /* (1) Set (RX_ABORT | RX_DIS) bits to reg MAC_DIAG_SW. */
    OS_REG_SET_BIT(ah, AR_DIAG_SW, AR_DIAG_RX_ABORT | AR_DIAG_RX_DIS);

    /*
     * (2) Poll (reg MAC_OBS_BUS_1[24:20] == 0) for 100ms
     * and if it doesn't become 0x0, print reg MAC_OBS_BUS_1.
     * Wait for Rx PCU state machine to become idle.
     */
    for (wait = AH_RX_STOP_TIMEOUT / AH_TIME_QUANTUM; wait != 0; wait--) {
        u_int32_t obs1 = OS_REG_READ(ah, AR_OBS_BUS_1);
        /* (MAC_PCU_OBS_BUS_1[24:20] == 0x0) - Check pcu_rxsm == IDLE */
        if ((obs1 & 0x01F00000) == 0) {
            break;
        }
        OS_DELAY(AH_TIME_QUANTUM);
    }

    /*
     * If bit 24:20 doesn't go to 0 within 100ms, print the value of
     * MAC_OBS_BUS_1 register on debug log.
     */
    if (wait == 0) {
        ath_hal_printf(ah,
            "%s: rx failed to go idle in %d us\n AR_OBS_BUS_1=0x%08x\n",
            __func__,
            AH_RX_STOP_TIMEOUT,
            OS_REG_READ(ah, AR_OBS_BUS_1));
    }

    /* (3) Set reg 0x0058 = 0x8100 in order to configure debug bus */
    OS_REG_WRITE(ah, 0x58, 0x8100);

    /*
     * (4) Poll (reg 0x00fc[11:8] == 0x0) for 100ms
     * wait for Rx DMA state machine to become idle
     */
    for (wait = AH_RX_STOP_TIMEOUT / AH_TIME_QUANTUM; wait != 0; wait--) {
        if ((OS_REG_READ(ah, 0x00fc) & 0xf00) == 0) {
            break;
        }
        OS_DELAY(AH_TIME_QUANTUM);
    }

    if (wait == 0) {
        ath_hal_printf(ah,
            "reg 0x00fc[11:8] is not 0, instead reg 0x00fc=0x%08x\n",
            OS_REG_READ(ah, 0xfc));
        // MAC_RXDP_SIZE register (0x70)
        ath_hal_printf(ah, "0x0070=0x%08x\n", OS_REG_READ(ah, 0x70));
    }

    /* (5) Set RXD bit to reg MAC_CR */
    OS_REG_WRITE(ah, AR_CR, AR_CR_RXD);

    /* (6) Poll MAC_CR.RXE = 0x0 for 100ms or until RXE goes low */
    for (wait = AH_RX_STOP_TIMEOUT / AH_TIME_QUANTUM; wait != 0; wait--) {
        if ((OS_REG_READ(ah, AR_CR) & AR_CR_RXE) == 0) {
            break;
        }
        OS_DELAY(AH_TIME_QUANTUM);
    }

    /* (7) If (RXE_LP|RXE_HP) doesn't go low within 100ms */
    if (wait == 0) {
        ath_hal_printf(ah,
            "%s: RXE_LP of MAC_CR reg failed to go low in %d us\n",
            __func__, AH_RX_STOP_TIMEOUT);
    }

    /* (8) Clear reg MAC_PCU_RX_FILTER */
    ar9300SetRxFilter(ah, 0);

#undef AH_RX_STOP_TIMEOUT
#undef AH_TIME_QUANTUM
}

void
ar9300EnableTestAddacMode(struct ath_hal *ah)
{
    ar9300ShutdownRx(ah);
}

void
ar9300DisableTestAddacMode(struct ath_hal *ah)
{
    /* Clear the TST_ADDAC register to disable */
    OS_REG_WRITE(ah, AR_TST_ADDAC, 0);
    OS_DELAY(100);
}

static void
ar9300SetupTestAddacMode(struct ath_hal *ah)
{
    OS_REG_WRITE(ah, AR_PHY_TEST,0);
    OS_REG_WRITE(ah, AR_PHY_TEST_CTL_STATUS,0);

    /*cf_bbb_obs_sel=4'b0001*/
    OS_REG_RMW_FIELD(ah, AR_PHY_TEST, AR_PHY_TEST_BBB_OBS_SEL, 0x1);

    /*cf_rx_obs_sel=5'b00000, this is the 5th bit */
    OS_REG_CLR_BIT(ah, AR_PHY_TEST, AR_PHY_TEST_RX_OBS_SEL_BIT5); 

    //cf_tx_obs_sel=3'b111
    OS_REG_RMW_FIELD(
        ah, AR_PHY_TEST_CTL_STATUS, AR_PHY_TEST_CTL_TX_OBS_SEL, 0x7);

    // cf_tx_obs_mux_sel=2'b11
    OS_REG_RMW_FIELD(
        ah, AR_PHY_TEST_CTL_STATUS, AR_PHY_TEST_CTL_TX_OBS_MUX_SEL, 0x3);

    // enable TSTADC
    OS_REG_SET_BIT(ah, AR_PHY_TEST_CTL_STATUS,AR_PHY_TEST_CTL_TSTADC_EN);
    OS_REG_SET_BIT(ah, AR_PHY_TEST_CTL_STATUS, AR_PHY_TEST_CTL_TSTDAC_EN);

    /*cf_rx_obs_sel=5'b00000, these are the first 4 bits*/
    OS_REG_RMW_FIELD(
        ah, AR_PHY_TEST_CTL_STATUS, AR_PHY_TEST_CTL_RX_OBS_SEL, 0x0);

    /*tstdac_out_sel=2'b01 */
    OS_REG_RMW_FIELD(ah, AR_PHY_TEST, AR_PHY_TEST_CHAIN_SEL, 0x1); 
}

static void
ar9300ForceAGCGain(struct ath_hal *ah)
{
#ifdef THIS_IS_OSPREY_2_0
    if (AR_SREV_OSPREY_20_OR_LATER(ah)) {
        /* TODO: for Osprey 2.0, we can set gain via baseband registers */
    } 
    else
#endif
    {
        // for Osprey 1.0, force Rx gain through long shift (analog) interface
        // this works for Osprey 2.0 too
        if (AH_PRIVATE(ah)->ah_curchan != NULL &&
            IS_CHAN_2GHZ(AH_PRIVATE(ah)->ah_curchan)) {
            // For 2G band:
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX2, AR_PHY_RX1DB_BIQUAD_LONG_SHIFT, 0x1);   // 10db=3
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX2, AR_PHY_RX6DB_BIQUAD_LONG_SHIFT, 0x2);  // 10db=2
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX2, AR_PHY_LNAGAIN_LONG_SHIFT,      0x7);  // 10db=6
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX2, AR_PHY_MXRGAIN_LONG_SHIFT,      0x3);  // 10db=3
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX2, AR_PHY_VGAGAIN_LONG_SHIFT,      0x0);  // 10db=0
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX1, AR_PHY_SCFIR_GAIN_LONG_SHIFT,   0x1);  // 10db=1
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX1, AR_PHY_MANRXGAIN_LONG_SHIFT,    0x1);  // 10db=1

            if (!AR_SREV_POSEIDON(ah)) {
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX2, AR_PHY_RX1DB_BIQUAD_LONG_SHIFT, 0x1);  
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX2, AR_PHY_RX6DB_BIQUAD_LONG_SHIFT, 0x2); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX2, AR_PHY_LNAGAIN_LONG_SHIFT,      0x7); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX2, AR_PHY_MXRGAIN_LONG_SHIFT,      0x3); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX2, AR_PHY_VGAGAIN_LONG_SHIFT,      0x0); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX1, AR_PHY_SCFIR_GAIN_LONG_SHIFT,   0x1); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX1, AR_PHY_MANRXGAIN_LONG_SHIFT,    0x1); 
    
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX2, AR_PHY_RX1DB_BIQUAD_LONG_SHIFT, 0x1); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX2, AR_PHY_RX6DB_BIQUAD_LONG_SHIFT, 0x2); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX2, AR_PHY_LNAGAIN_LONG_SHIFT,      0x7); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX2, AR_PHY_MXRGAIN_LONG_SHIFT,      0x3); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX2, AR_PHY_VGAGAIN_LONG_SHIFT,      0x0); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX1, AR_PHY_SCFIR_GAIN_LONG_SHIFT,   0x1); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX1, AR_PHY_MANRXGAIN_LONG_SHIFT,    0x1); 
            }
        } else {
            // For 5G band:
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX2, AR_PHY_RX1DB_BIQUAD_LONG_SHIFT, 0x2);  // was 3=10/15db, 2=+1db 
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX2, AR_PHY_RX6DB_BIQUAD_LONG_SHIFT, 0x2);  // was 1
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX2, AR_PHY_LNAGAIN_LONG_SHIFT,      0x7); 
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX2, AR_PHY_MXRGAIN_LONG_SHIFT,      0x3);  // was 2=15db, 3=10db
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX2, AR_PHY_VGAGAIN_LONG_SHIFT,      0x6); 
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX1, AR_PHY_SCFIR_GAIN_LONG_SHIFT,   0x1); 
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX1, AR_PHY_MANRXGAIN_LONG_SHIFT,    0x1); 

            if (!AR_SREV_POSEIDON(ah)) {
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX2, AR_PHY_RX1DB_BIQUAD_LONG_SHIFT, 0x2); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX2, AR_PHY_RX6DB_BIQUAD_LONG_SHIFT, 0x2); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX2, AR_PHY_LNAGAIN_LONG_SHIFT,      0x7); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX2, AR_PHY_MXRGAIN_LONG_SHIFT,      0x3);  // was 2 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX2, AR_PHY_VGAGAIN_LONG_SHIFT,      0x6); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX1, AR_PHY_SCFIR_GAIN_LONG_SHIFT,   0x1); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX1, AR_PHY_MANRXGAIN_LONG_SHIFT,    0x1);
   
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX2, AR_PHY_RX1DB_BIQUAD_LONG_SHIFT, 0x2); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX2, AR_PHY_RX6DB_BIQUAD_LONG_SHIFT, 0x2); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX2, AR_PHY_LNAGAIN_LONG_SHIFT,      0x7); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX2, AR_PHY_MXRGAIN_LONG_SHIFT,      0x3);  // was 2
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX2, AR_PHY_VGAGAIN_LONG_SHIFT,      0x6); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX1, AR_PHY_SCFIR_GAIN_LONG_SHIFT,   0x1); 
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX1, AR_PHY_MANRXGAIN_LONG_SHIFT,    0x1); 
            }
        }
    }
}

void
ar9300BeginAdcCapture(struct ath_hal *ah, int auto_agc_gain)
{
    ar9300SetupTestAddacMode(ah);

    // First clear TST_ADDAC register
    ar9300DisableTestAddacMode(ah);

    if (!auto_agc_gain) {
        /* Force max AGC gain */
        ar9300ForceAGCGain(ah);
    }

    /* Now enable TST mode */
    OS_REG_SET_BIT(ah, AR_TST_ADDAC, AR_TST_ADDAC_TST_MODE);
    OS_REG_SET_BIT(ah, AR_TST_ADDAC, AR_TST_ADDAC_TST_LOOP_ENA);
    OS_DELAY(10);

    /* Begin capture and wait 1ms before reading the capture data*/
    OS_REG_SET_BIT(ah, AR_TST_ADDAC, AR_TST_ADDAC_BEGIN_CAPTURE);
    OS_DELAY(1000);
}

static inline int16_t convert_to_signed(int sample)
{
    if (sample > AR9300_RAW_ADC_SAMPLE_MAX) {
        sample -= (AR9300_RAW_ADC_SAMPLE_MAX + 1) << 1;
    }
    return sample;
}

HAL_STATUS
ar9300RetrieveCaptureData(
    struct ath_hal *ah,
    u_int16_t chain_mask,
    int disable_dc_filter,
    void *sample_buf,
    u_int32_t* num_samples)
{
    u_int32_t val, i, ch;
    int descr_address, num_chains;
    ADC_SAMPLE* sample;
    int32_t i_sum[AR9300_MAX_CHAINS] = {0};
    int32_t q_sum[AR9300_MAX_CHAINS] = {0};

    num_chains = mask2chains[chain_mask];
    if (*num_samples < (MAX_ADC_SAMPLES * num_chains)) {
        /*
         * supplied buffer is too small -
         * update to inform caller of required size
         */
        *num_samples = MAX_ADC_SAMPLES * num_chains;
        return HAL_ENOMEM;
    }

    /* Make sure we are reading TXBUF */
    OS_REG_CLR_BIT(ah, AR_RXFIFO_CFG, AR_RXFIFO_CFG_REG_RD_ENA);

    sample = (ADC_SAMPLE*)sample_buf;
    descr_address = RXTX_DESC_BUFFER_ADDRESS;
    for (i = 0;
         i < MAX_ADC_SAMPLES;
         i++, descr_address += 4, sample += num_chains)
    {
        val = OS_REG_READ(ah, descr_address);
        /* Get bits [29:20]from TXBUF - chain#0 I */
        i_sum[0] += sample[0].i = convert_to_signed((val >> 20) & 0x3ff);
        /* Get bits [19:10] from TXBUF - chain#0 Q */
        q_sum[0] += sample[0].q = convert_to_signed((val >> 10) & 0x3ff);
        if (num_chains >= 2) {
            /* Get bits [9:0] from TXBUF - chain#1 I */
            i_sum[1] += sample[1].i = convert_to_signed(val & 0x3ff);
        }
    }

    if (num_chains >= 2) {
        /* Make sure we are reading RXBUF */
        OS_REG_SET_BIT(ah, AR_RXFIFO_CFG, AR_RXFIFO_CFG_REG_RD_ENA);
    
        sample = (ADC_SAMPLE*)sample_buf;
        descr_address = RXTX_DESC_BUFFER_ADDRESS;
        for (i = 0;
             i < MAX_ADC_SAMPLES;
             i++, descr_address += 4, sample += num_chains)
        {
            val = OS_REG_READ(ah, descr_address);
            /* Get bits [27:18] from RXBUF - chain #1 Q */
            q_sum[1] += sample[1].q = convert_to_signed((val >> 20) & 0x3ff);
            if (num_chains >= 3) {
                /* Get bits [19:10] from RXBUF - chain #2 I */
                i_sum[2] += sample[2].i =
                    convert_to_signed((val >> 10) & 0x3ff);
                /* Get bits [9:0] from RXBUF - chain#2 Q */
                q_sum[2] += sample[2].q =
                    convert_to_signed(val & 0x3ff);
            }
        }
    }

    if (!disable_dc_filter) {
        /*
        ath_hal_printf(ah, "%s: running DC filter over %d chains now...\n",
            __func__, num_chains);
         */
        for (ch = 0; ch < num_chains; ch++) { 
            i_sum[ch] /= MAX_ADC_SAMPLES;
            q_sum[ch] /= MAX_ADC_SAMPLES;
            sample = (ADC_SAMPLE*)sample_buf;
            for (i = 0; i < MAX_ADC_SAMPLES; i++, sample++) {
                sample[ch].i -= i_sum[ch];
                sample[ch].q -= q_sum[ch];
            }
        }
    }

    /* inform caller of returned sample count */
    *num_samples = MAX_ADC_SAMPLES * num_chains;
    return HAL_OK;
}

static const struct {
    int freq;                          /* Mhz */
    int16_t offset[AR9300_MAX_CHAINS]; /* 100 times units (0.01 db) */
} gain_cal_data_osprey[] = {
          /* freq,  chain0_offset,
					chain1_offset, 
                    chain2_offset */
/*ch  1*/  { 2412, {  500,   /* apl-adj 10/8/10: -250, was: 750=13.3-5.8  */
                      880,   /* apl-adj 10/8/10: -130, was: 1210=16.3-4.2 */   
                      730}}, /* apl-adj 10/8/10: -100, was: 1130=16.3-5.0 */   
/*ch  6*/  { 2437, {  420,   /* apl-adj 10/8/10: -250, was: 670=13.1-6.4  */   
                      860,   /* apl-adj 10/8/10: -230, was: 1140=16.1-4.7 */   
                      700}}, /* apl-adj 10/8/10: -200, was: 1050=14.1-3.6 */   
/*ch 11*/  { 2462, {  440,   /* apl-adj 10/8/10: -250, was: 720 =12.3-5.1 */   
                      860,   /* apl-adj 10/8/10: -150, was: 1160=15.3-3.7 */   
                      710}}, /* apl-adj 10/8/10: -100, was: 1110=17.3-6.2 */   
/*ch 36*/  { 5180, { 1260,   /* apl-adj 10/8/10: -350, was: 2480=25.3-0.5 */   
                      680,   /* apl-adj 10/8/10: -500, was: 1680=19.3-2.5 */   
                     1070}}, /* apl-adj 10/8/10: -450, was: 2170=25.3-3.6 */   
/*ch 44*/  { 5220, { 1250,   /* apl-adj 10/8/10: -400, was: 2450=25.0-0.5 */   
                      580,   /* apl-adj 10/8/10: -700, was: 1580=19.0-3.2 */   
                     1000}}, /* apl-adj 10/8/10: -550, was: 2050=23.0-2.5 */   
/*ch 64*/  { 5320, { 1260,   /* apl-adj 10/8/10: -200, was: 2510=23.9+1.2 */   
                      510,   /* apl-adj 10/8/10: -700, was: 1510=16.9-1.8 */   
                      850}}, /* apl-adj 10/8/10: -600, was: 1850=19.9-1.4 */   
           { 5400, { 2220,   /* 22.2                                      */   
                     1520,   /* 15.2                                      */   
                     1620}}, /* 16.2                                      */   
/*ch100*/  { 5500, { 1230,   /* apl-adj 10/8/10: -350, was: 2280=18.9+3.9 */   
                      550,   /* apl-adj 10/8/10: -750, was: 1450=12.9+1.6 */   
                      410}}, /* apl-adj 10/8/10: -700, was: 1510=13.9+1.2 */   
/*ch120*/  { 5600, { 1160,   /* apl-adj 10/8/10: -400, was: 2060=15.8+4.8 */   
                      720,   /* apl-adj 10/8/10: -700, was: 1420=10.8+3.4 */   
                      580}}, /* apl-adj 10/8/10: -600, was: 1580=14.8+1.0 */   
/*ch140*/  { 5700, { 1360,   /* apl-adj 10/8/10: -350, was: 1910=13.6+5.5 */   
                      910,   /* apl-adj 10/8/10: -500, was: 1360=8.6+5.0  */   
                      870}}, /* apl-adj 10/8/10: -450, was: 1520=9.6+5.6  */   
/*ch149*/  { 5745, { 1410,   /* apl-adj 10/8/10: -300, was: 1910=13.6+5.5 */   
                     1010,   /* apl-adj 10/8/10: -400, was: 1360=8.6+5.0  */   
                      920}}, /* apl-adj 10/8/10: -400, was: 1520=9.6+5.6  */   
/*ch157*/  { 5785, { 1520,   /* apl-adj 10/8/10: -400, was: 1920=12.7+6.5 */   
                     1110,   /* apl-adj 10/8/10: -400, was: 1460=8.7+5.9  */   
                     1030}}, /* apl-adj 10/8/10: -400, was: 1530=11.7+3.6 */   
/*ch165*/  { 5825, { 1570,   /* apl-adj 10/8/10: -350, was: 1920=12.7+6.5 */   
                     1160,   /* apl-adj 10/8/10: -350, was: 1460=8.7+5.9, */   
                     1080}}, /* apl-adj 10/8/10: -350, was: 1530=11.7+3.6 */   
           {    0, {}     }    
};

static int16_t gainCalOffset(int freqMhz, int chain)
{
    int i;
    for (i = 0; gain_cal_data_osprey[i].freq != 0; i++) {
        if (gain_cal_data_osprey[i + 0].freq == freqMhz ||
            gain_cal_data_osprey[i + 1].freq > freqMhz ||
            gain_cal_data_osprey[i + 1].freq == 0) {
            return(gain_cal_data_osprey[i].offset[chain]);
        }
    }
    ath_hal_printf(AH_NULL,
        "%s: **Warning: no cal offset found for freq %d chain %d\n",
        __func__, freqMhz, chain);
    return 0;
}

HAL_STATUS
ar9300GetMinAGCGain(
    struct ath_hal *ah,
    int freqMhz,
    int32_t *chain_gain,
    int num_chain_gain)
{
    int ch;
    for (ch = 0; ch < num_chain_gain && ch < AR9300_MAX_CHAINS; ch++) {
        /*
         * cal offset 100 times units (0.01 db)
         * round to nearest and return (gain is -ve)
         */
        chain_gain[ch] = -(gainCalOffset(freqMhz, ch) + 50) / 100;
    }
    return HAL_OK;
}


// values are db*100
#define AR9300_GI_2GHZ  7100  /* was 7600: -10db=6600, -5db=7100 */
#define AR9300_GI_5GHZ  8600  /* was 9100: -15db=7600, -10db=8100, -5db=8600 */ 

HAL_STATUS
ar9300CalculateADCRefPowers(
    struct ath_hal *ah,
    int freqMhz,
    int16_t *sample_min,
    int16_t *sample_max, 
    int32_t *chain_ref_pwr,
    int num_chain_ref_pwr)
{
    int32_t ref_power;
    int ch;
    const int32_t GI = (freqMhz < 5000) ? AR9300_GI_2GHZ : AR9300_GI_5GHZ;
    for (ch = 0; ch < num_chain_ref_pwr && ch < AR9300_MAX_CHAINS; ch++) {
        /*
         * calculation is done in 100 times units (0.01 db)
         */
        ref_power = -GI + gainCalOffset(freqMhz, ch) - AR9300_MAX_ADC_POWER;
        chain_ref_pwr[ch] = (ref_power - 50) / 100; 
        /*
        ath_hal_printf(AH_NULL, "Abs power at freq %d for chain %d: %3.2f\n",
            freqMhz, ch, chain_ref_pwr[ch]);
         */
    }
    *sample_min = AR9300_RAW_ADC_SAMPLE_MIN;
    *sample_max = AR9300_RAW_ADC_SAMPLE_MAX;
    return HAL_OK;
}

#endif

#else
/*
 * Raw capture mode not enabled - insert dummy code to keep the compiler happy
 */
typedef int ar9300_dummy_adc_capture;

#endif /* AH_SUPPORT_AR9300*/
