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
#include "ar9300.h"
#include "ar9300desc.h"
#include "ar9300reg.h"
#include "ar9300phy.h"

#ifdef ATH_SUPPORT_TxBF
#include "ar9300_txbf.h"

static u_int8_t const ns[2][3] = { // number of carrier
    { 50, 30, 16},
    {114, 58, 30}};
static u_int8_t const bitmap[8] = {0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff};
static u_int8_t mask2num[8] = {
    0 /* 000 */,
    1 /* 001 */,
    1 /* 010 */,
    2 /* 011 */,
    1 /* 100 */,
    2 /* 101 */,
    2 /* 110 */,
    3 /* 111 */
};

u_int8_t
ar9300GetNTX(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    int tx_chainmask;

    tx_chainmask = ahp->ah_txchainmask;

    return mask2num[tx_chainmask];
}

u_int8_t
ar9300GetNRX(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    int rx_chainmask;

    rx_chainmask = ahp->ah_rxchainmask;

    return mask2num[rx_chainmask];
}


void ar9300SetHwCvTimeout(struct ath_hal *ah,HAL_BOOL opt)
{
    struct ath_hal_private  *ap = AH_PRIVATE(ah);


    // if true use H/W settigs to update cv timeout valise
    if (opt) {
        ah->ah_txbf_hw_cvtimeout = ap->ah_config.ath_hal_cvtimeout;
    }
    OS_REG_WRITE(ah, AR_TXBF_TIMER,
        (ah->ah_txbf_hw_cvtimeout << AR_TXBF_TIMER_ATIMEOUT_S) |
        (ah->ah_txbf_hw_cvtimeout << AR_TXBF_TIMER_TIMEOUT_S));
}

void ar9300InitBf(struct ath_hal *ah)
{
    u_int32_t   tmp;
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    u_int8_t txbfCtl;

    /* set default settings for TxBF */
    OS_REG_WRITE(ah, AR_TXBF,
        (AR_TXBF_PSI_4_PHI_6 << AR_TXBF_CB_TX_S)    |   /* size of codebook entry set to psi 4bits, phi 6bits */
        (AR_TXBF_NUMBEROFBIT_8 << AR_TXBF_NB_TX_S)  |   /* set Number of bit_to 8 bits */
        (AR_TXBF_No_GROUP << AR_TXBF_NG_RPT_TX_S)   |   /* NG_RPT_TX set t0 No_GROUP */
        (AR_TXBF_SIXTEEN_CLIENTS << AR_TXBF_NG_CVCACHE_S)  |    /* TXBF_NG_CVCACHE set to 16 clients */
        (SM(AR_TXBF_MAX_POWER, AR_TXBF_TXCV_BFWEIGHT_METHOD))); /* set weighting method to max power */

    /* when ah_txbf_hw_cvtimeout = 0 , use setting values*/
    if (ah->ah_txbf_hw_cvtimeout == 0) {
        ar9300SetHwCvTimeout(ah, AH_TRUE);
    } else {
        ar9300SetHwCvTimeout(ah, AH_FALSE);
    }

    tmp = OS_REG_READ(ah, AR_SELFGEN);
    tmp |= SM(AR_SELFGEN_CEC_TWO_SPACETIMESTREAM, AR_CEC); /* CEC set to 2 stream for self_gen */
    tmp |= SM(AR_SELFGEN_MMSS_EIGHT_us, AR_MMSS);   /* initial selfgen Minimum MPDU */
                                                    /* Start Spacing to 8 us */
    OS_REG_WRITE(ah, AR_SELFGEN, tmp);

    /*  set initial for basic set */
    ahpriv->ah_lowest_txrate = MAXMCSRATE;
    ahpriv->ah_basic_set_buf = (1 << (ahpriv->ah_lowest_txrate+1))-1;
    OS_REG_WRITE(ah, AR_BASIC_SET, ahpriv->ah_basic_set_buf);

    //tmp=OS_REG_READ(ah, AR_PCU_MISC_MODE2);       /* read current settings*/
    //tmp|=(1<<28);
    //OS_REG_WRITE(ah,AR_PCU_MISC_MODE2,tmp);       /* force H upload */

    tmp = OS_REG_READ(ah,AR_PHY_TIMING2);
    tmp |= AR_PHY_TIMING2_HT_Fine_Timing_EN;
    OS_REG_WRITE(ah,AR_PHY_TIMING2,tmp);            /* HT fine timing enable */

    tmp = OS_REG_READ(ah,AR_PCU_MISC_MODE2);
    tmp |= AR_DECOUPLE_DECRYPTION;
    OS_REG_WRITE(ah,AR_PCU_MISC_MODE2,tmp);         /* decouple enable */

    tmp = OS_REG_READ(ah,AR_PHY_RESTART);
    tmp |= AR_PHY_RESTART_ENA;
    OS_REG_WRITE(ah,AR_PHY_RESTART,tmp);            /* enable restart. */

    tmp = OS_REG_READ(ah,AR_PHY_SEARCH_START_DELAY);
    tmp |= AR_PHY_ENABLE_FLT_SVD;
    OS_REG_WRITE(ah,AR_PHY_SEARCH_START_DELAY,tmp); /*enable flt SVD */

    /* initial sequence generate for auto reply mgmt frame*/
    OS_REG_WRITE(ah, AR_MGMT_SEQ, AR_MIN_HW_SEQ | 
        SM(AR_MAX_HW_SEQ,AR_MGMT_SEQ_MAX));

    txbfCtl = ahpriv->ah_config.ath_hal_txbfCtl;
    if ((txbfCtl & TxBFCtl_Non_ExBF_Immediately_Rpt) ||
        (txbfCtl & TxBFCtl_Comp_ExBF_Immediately_Rpt)) {
        tmp = OS_REG_READ(ah,AR_H_XFER_TIMEOUT);
        tmp |= AR_EXBF_IMMDIATE_RESP;
        tmp &= ~(AR_EXBF_NOACK_NO_RPT);
        /* enable immediately report */
        OS_REG_WRITE(ah,AR_H_XFER_TIMEOUT,tmp);
    }
}

int
ar9300GetNESS(struct ath_hal *ah,u_int8_t codeRate, u_int8_t CEC)
{
    u_int8_t Nss = 0, NESS = 0, NTX;

    NTX = ar9300GetNTX(ah);

    /* CEC+1 remote cap's for channel estimation in stream.*/

    if (NTX > (CEC + 2)) {  /* limit by remote's cap */
        NTX = CEC + 2;
    }

    if (codeRate < MIN_TWO_STREAM_RATE) {
        Nss = 1;
    } else if (codeRate < MIN_THREE_STREAM_RATE) {
        Nss = 2;
    } else {
        Nss = 4;
    }

    if (codeRate >= MIN_HT_RATE) { /* HT rate */
        if (NTX > Nss) {
            NESS = NTX - Nss;
        }
    }
    return NESS;
}

/*
 * function: ar9300Set11nTxBFSounding
 * purpose:  Set sounding frame
 * inputs:   codeRate: rate of this frame.
 *           CEC: Channel Estimation Capability . Extract from subfields
 *               of the Transmit Beamforming Capabilities field of remote.
 *           opt: control flag from upper layer
 */
void
ar9300Set11nTxBFSounding(
    struct ath_hal *ah,
    void *ds,
    HAL_11N_RATE_SERIES series[],
    u_int8_t CEC,
    u_int8_t opt)
{
    struct ar9300_txc *ads = AR9300TXC(ds);
    u_int8_t NESS = 0, NESS1 = 0, NESS2 = 0, NESS3 = 0;

    ads->ds_ctl19 &= (~AR_Not_Sounding); /*set sounding frame */

    if (opt == HAL_TXDESC_STAG_SOUND) {
        NESS  = ar9300GetNESS(ah,series[0].Rate, CEC);
        NESS1 = ar9300GetNESS(ah,series[1].Rate, CEC);
        NESS2 = ar9300GetNESS(ah,series[2].Rate, CEC);
        NESS3 = ar9300GetNESS(ah,series[3].Rate, CEC);
    }
    ads->ds_ctl19 |= SM(NESS, AR_NESS);
    ads->ds_ctl20 |= SM(NESS1, AR_NESS1);
    ads->ds_ctl21 |= SM(NESS2, AR_NESS2);
    ads->ds_ctl22 |= SM(NESS3, AR_NESS3);

    /*
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s:sounding rate series %x,%x,%x,%x \n",
        __func__,
        series[0].Rate, series[1].Rate, series[2].Rate, series[3].Rate);
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s:Ness1 %d,NESS2 %d, NESS3 %d, NESS4 %d\n",
        __func__, NESS, NESS1, NESS2, NESS3);
    */
    /* disable other series that don't support sounding at legacy rate */
    if (series[1].Rate < MIN_HT_RATE) {
        ads->ds_ctl13 &=
            ~(AR_XmitDataTries1 | AR_XmitDataTries2 | AR_XmitDataTries3);
    } else if (series[2].Rate < MIN_HT_RATE) {
        ads->ds_ctl13 &= ~(AR_XmitDataTries2 | AR_XmitDataTries3);
    } else if (series[3].Rate < MIN_HT_RATE) {
        ads->ds_ctl13 &= ~(AR_XmitDataTries3);
    }
}

#ifdef TXBF_TO_DO
void
ar9300Set11nTxBFCal(
    struct ath_hal *ah,
    void *ds,
    u_int8_t calPos,
    u_int8_t codeRate,
    u_int8_t CEC,
    u_int8_t opt)
{
    struct ar9300_txc *ads = AR9300TXC(ds);

    /* set descriptor for calibration*/
    if (calPos == 1) {             
        /* cal start frame */
        /* set vmf to enable burst mode */
        ads->ds_ctl11 |= AR_VirtMoreFrag; 
    } else if (calPos == 3) {      
        /* cal complete frame */
        /* set calibrating indicator to BB */
        ads->ds_ctl17 |= AR_Calibrating; 
        //ar9300Set11nTxBFSounding(ah,ds,codeRate,CEC,opt);
    }
}

static void
ar9300TxBFWriteCache(struct ath_hal *ah, u_int32_t cvtmp, u_int16_t idx)
{
    cvtmp |= AR_CVCACHE_WRITE;                  /* this operation for writing */
    OS_REG_WRITE(ah, AR_CVCACHE(idx), cvtmp);   /* write cvcache */
    do  {
        cvtmp = OS_REG_READ(ah, AR_CVCACHE(idx));
    } while ((cvtmp & AR_CVCACHE_DONE) == 0);   /* exit when done */
}

// search CVchach address according to keyidx
static u_int16_t
ar9300TxBFLRUSearch(struct ath_hal *ah, u_int8_t keyidx)
{
    u_int32_t   txbf_swbf = 0;
    u_int16_t   cvcacheidx = 0;

    OS_REG_WRITE(ah, AR_TXBF_SW, 0);     // make sure disable LRU search
    txbf_swbf = (SM(keyidx, AR_DEST_IDX) | AR_LRU_EN);
    OS_REG_WRITE(ah, AR_TXBF_SW, txbf_swbf);     // enable LRU search

    do  {
        txbf_swbf = OS_REG_READ(ah, AR_TXBF_SW);
    } while ((txbf_swbf & AR_LRU_ACK) != 1);       // wait for serching done

    cvcacheidx = MS(txbf_swbf, AR_LRU_ADDR);

    OS_REG_WRITE(ah, AR_TXBF_SW, 0);     // disable LRU search
    return cvcacheidx;
}

/*
 * Function ar9300TxBFSaveCVFromCompress
 * purpose: save compress report from segmeneted compress report frame into
 *     CVcache.
 * input:
 *     ah:
 *     keyidx: key cache index
 *     mimocontrol: copy of first two bytes of MIMO control field
 *     CompressRpt: pointer to the buffer which collect segmented compressed
 *         beamforming report together and keep beamforming feedback matrix
 *         only. (remove SNR report)
 * output: AH_FALSE: incorrect parameters for Nc and Nr.
 *         AH_TRUE:  success.
 */
HAL_BOOL
ar9300TxBFSaveCVFromCompress(struct ath_hal *ah, u_int8_t keyidx,
        u_int16_t mimocontrol, u_int8_t *CompressRpt)
{
    u_int8_t    ncidx, nridx, bandwidth, ng, nb, ci, psi_bit, phi_bit, i, j;
    u_int32_t   cvtmp = 0;
    u_int16_t   cvcacheidx = 0;
    u_int16_t   idx;
    char        psi[3], phi[3];
    u_int8_t    ng_sys = 0;
    u_int8_t    bit_left = 8, rpt_idx = 0, Na_2, Ns, Ns_rpt, currentdata;
    u_int8_t    samples, sampleidx, average_shift;
    u_int8_t const  na[3][3] = {// number of angles
        {0, 0, 0}, // index 0: N/A
        {2, 2, 0},
        {4, 6, 6}};

    /* get required parameters from MIMO control field */
    ncidx = (mimocontrol & AR_Nc_idx) + 1;
    nridx = (MS(mimocontrol, AR_Nr_idx)) + 1;
    if ((ncidx > 3) | (nridx > 3)) {
        return AH_FALSE;        // return false for row or col >3
    }

    Na_2 = na[ncidx][nridx] / 2;    // Na div by 2
    if (Na_2 == 0) {
        return AH_FALSE;       //incorrect Na
    }

    bandwidth = MS(mimocontrol, AR_bandwith);
    ng = MS(mimocontrol, AR_Ng);
    nb = MS(mimocontrol, AR_Nb);
    ci = MS(mimocontrol, AR_CI);
    psi_bit = ci + 1;       // # of bits for each psi
    phi_bit = ci + 3;       // # of bits for each phi

    /* search CVCache locaction by key cache idx*/
    cvcacheidx = ar9300TxBFLRUSearch(ah,keyidx);

    idx = cvcacheidx + 4;                   // fisrt CV address.
    ng_sys = MS(OS_REG_READ(ah, AR_TXBF), AR_TXBF_NG_CVCACHE);

    Ns = ns[bandwidth][ng_sys];   // Ns store desired final carrier size.
    if (ng <= ng_sys)     // keep ng of current report
    {
        samples = 1;      // no down sample required
        average_shift = 0;
    }
    else                // should reduce the ng of current report
    {
        average_shift = ng - ng_sys;
        samples = 1 << average_shift;       // down sample #
        ng = ng_sys;
    }

    bit_left = 8;
    rpt_idx = 0;
    currentdata = CompressRpt[rpt_idx];
    sampleidx = 0;
    Ns_rpt = ns[bandwidth][ng];    // carrier size in orignal report
    for (i = 0; i < Ns_rpt; i++) {
        for (j = 0; j < Na_2; j++) {
            if (bit_left < phi_bit) {
                rpt_idx++;
                // get next byte
                currentdata += CompressRpt[rpt_idx] << bit_left;
                bit_left += 8;
            }
            phi[j] = (currentdata & bitmap[phi_bit - 1]);    // get phi
            currentdata = currentdata >> phi_bit;
            bit_left -= phi_bit;

            if (bit_left < psi_bit) {
                rpt_idx++;
                // get next byte
                currentdata += CompressRpt[rpt_idx] << bit_left;
                bit_left += 8;
            }
            psi[j] = (currentdata& bitmap[psi_bit - 1]); // get psi
            currentdata = currentdata >> psi_bit;
            bit_left -= psi_bit;
        }
        sampleidx++;
        if (samples == sampleidx) { // write to CVcache once per  samples
            sampleidx = 0;
            cvtmp =
                 phi[0]        |
                (phi[1] <<  6) |
                (psi[0] << 12) |
                (psi[1] << 16) |
                (phi[2] << 20) |
                (psi[2] << 24);      // feed phi and psi into CVCache format
            ar9300TxBFWriteCache(ah, cvtmp, idx);
            idx += 4;         //next address
        }
    }

    /* write cvcache header*/
    cvtmp =
        SM(ng,AR_CVCACHE_Ng_IDX)      |
        SM(bandwidth,AR_CVCACHE_BW40) |
        SM(0,AR_CVCACHE_IMPLICIT)     |
        SM(ncidx,AR_CVCACHE_Nc_IDX)   |
        SM(nridx,AR_CVCACHE_Nr_IDX); // prepare header
    ar9300TxBFWriteCache(ah, cvtmp, cvcacheidx);

    return  AH_TRUE;
}

/*
 * Function ar9300TxBFSaveCVFromNonCompress
 * purpose: save compress report from segmeneted compress report frame into
 *     CVcache.
 * input:
 *     ah:
 *     keyidx: key cache index
 *     mimocontrol: copy of first two bytes of MIMO control field
 *     NonCompressRpt: pointer to the buffer which collect segmented
 *         Noncompressed beamforming report together and keep beamforming
 *         feedback matrix only. (remove SNR report)
 * output: AH_FALSE: incorrect parameters for Nc and Nr.
 *         AH_TRUE:  success.
 */
HAL_BOOL
ar9300TxBFSaveCVFromNonCompress(struct ath_hal *ah, u_int8_t keyidx,
        u_int16_t mimocontrol, u_int8_t *NonCompressRpt)
{
    u_int8_t  ncidx, nridx, bandwidth, ng, nb, i, j, k;
    u_int32_t cvtmp = 0;
    u_int16_t cvcacheidx = 0;
    u_int16_t idx;
    u_int8_t  psi[3], phi[3];
    u_int8_t  ng_sys = 0;
    u_int8_t  bit_left = 8, rpt_idx = 0, Ns, Ns_rpt, currentdata;
    u_int8_t  samples, sampleidx, average_shift;
    COMPLEX   V[3][3];

    /* get required parameters from MIMO control field*/
    ncidx = (mimocontrol & AR_Nc_idx) + 1;
    nridx = (MS(mimocontrol, AR_Nr_idx)) + 1;
    if ((ncidx > 3) | (nridx > 3)) {
        return AH_FALSE;        // return false for row or col >3
    }

    bandwidth = MS(mimocontrol, AR_bandwith);
    ng = MS(mimocontrol, AR_Ng);
    switch (MS(mimocontrol, AR_Nb)) {
    case 0:
        nb = 4;
        break;
    case 1:
        nb = 2;
        break;
    case 2:
        nb = 6;
        break;
    case 3:
        nb = 8;
        break;
    default:
        nb = 8;
        break;
    }

    /* search CVCache locaction by key cache idx*/
    cvcacheidx = ar9300TxBFLRUSearch(ah, keyidx);

    idx = cvcacheidx + 4;                   // fisrt CV address.
    ng_sys = MS(OS_REG_READ(ah, AR_TXBF), AR_TXBF_NG_CVCACHE);

    Ns = ns[bandwidth][ng_sys];   // Ns store desired final carrier size.

    if (ng <= ng_sys) {   // keep ng of current report
        samples = 1;      // no down sample required
        average_shift = 0;
    } else {              // should reduce the ng of current report
        average_shift = ng - ng_sys;
        samples = 1 << average_shift;       // down sample #
        ng = ng_sys;
    }

    currentdata = NonCompressRpt[rpt_idx];
    sampleidx = 0;
    Ns_rpt = ns[bandwidth][ng];    // carrier size in orignal report
    for (i = 0; i < Ns_rpt; i++) {
           for (j = 0; j < nridx; j++) {
            for (k = 0; k < ncidx; k++) {
                if (bit_left < nb) {
                    rpt_idx++;
                    // get next byte
                    currentdata += NonCompressRpt[rpt_idx] << bit_left;
                    bit_left += 8;
                }    
                V[j][k].real = currentdata & bitmap[nb - 1];
                currentdata = currentdata >> nb;
                bit_left -= nb;
                if (bit_left < nb) {
                    rpt_idx++;
                    // get next byte
                    currentdata += NonCompressRpt[rpt_idx] << bit_left;
                    bit_left += 8;
                }    
                V[j][k].imag = currentdata & bitmap[nb - 1];
                currentdata = currentdata >> nb;
                bit_left -= nb;
            }
        }            

        compress_v(V, nridx,ncidx, phi, psi);

        sampleidx++;
        if (samples == sampleidx) {
            sampleidx = 0;
            cvtmp =
                 phi[0]        |
                (phi[1] <<  6) |
                (psi[0] << 12) |
                (psi[1] << 16) |
                (phi[2] << 20) |
                (psi[2] << 24);      // feed phi and psi into CVCache format
            ar9300TxBFWriteCache(ah, cvtmp, idx);
            idx += 4;         //next address
        }
    }

    /* write cvcache header*/
    cvtmp =
        SM(ng,AR_CVCACHE_Ng_IDX) |
        SM(bandwidth,AR_CVCACHE_BW40) |
        SM(0,AR_CVCACHE_IMPLICIT) |
        SM(ncidx,AR_CVCACHE_Nc_IDX) |
        SM(nridx,AR_CVCACHE_Nr_IDX); // prepare header
    ar9300TxBFWriteCache(ah, cvtmp, cvcacheidx);

    return  AH_TRUE;
}

/*
 * Function:ar9300Get_local_h
 * purpose: Get H from local buffer, separate it into complex format
 */
void
ar9300Get_local_h(
    struct ath_hal *ah,
    u_int8_t *local_h,
    int local_h_length,
    int Ntone,
    COMPLEX(*output_h)[3][Tone_40M])
{
    u_int8_t  k;
    u_int8_t  Nc, Nr, Nbit_left, ncidx, nridx;
    u_int32_t bitmask, idx, currentdata, h_data, h_idx;

    Nr = ar9300GetNRX(ah);
    // total bits = 20*Nr*Nc*Ntone
    Nc = (int) (local_h_length * 8) / (int) (20 * Nr * Ntone);

    Nbit_left = 16; // process 16 bit once a time
    bitmask = (1 << 10) - 1; // 10 bit resoluation for H real and imag
    idx = h_idx = 0;
    h_data = local_h[idx++];
    h_data += (local_h[idx++] << 8);
    currentdata = h_data & ((1 << 16) - 1); // get low 16 first
    for (k = 0; k < Ntone; k++) {
        for (ncidx = 0; ncidx < Nc; ncidx++) {
            for (nridx = 0; nridx < Nr; nridx++) {
                if ((Nbit_left - 10) < 0) {
                    h_data = local_h[idx++];
                    h_data += (local_h[idx++] << 8);
                    currentdata += h_data << Nbit_left;
                    Nbit_left += 16;      // get 16bit
                }
                output_h[nridx][ncidx][k].imag = currentdata & bitmask;
                if ((output_h[nridx][ncidx][k].imag) & (1 << 9)) { //sign bit
                    // two's complement
                    output_h[nridx][ncidx][k].imag -= (1 << 10);
                }
                Nbit_left -= 10;
                currentdata = currentdata >> 10;    // shift out used bits

                if ((Nbit_left - 10) < 0) {
                    h_data = local_h[idx++];
                    h_data += (local_h[idx++] << 8);
                    currentdata += h_data << Nbit_left;
                    Nbit_left += 16;      // get 16bit
                }
                output_h[nridx][ncidx][k].real = currentdata & bitmask;
                if ((output_h[nridx][ncidx][k].real) & (1 << 9)) { //sign bit
                    // two's complement
                    output_h[nridx][ncidx][k].real -= (1 << 10);
                }
                Nbit_left -= 10;
                currentdata = currentdata >> 10;    // shift out used bits
            }
        }
    }
}

/*
 * Function ar9300TxBFRCUpdate
 * purpose: Calculation the radio coefficients from CSI1 and CSI2 and save it
 *     into hardware.
 * input:
 *     ah:
 *     rx_status: pointer to Rx status of Rx CSI frame.
 *     local_h: pointer to RXDP+12. Which is the beginning point of h
 *         reported by hardware.
 *     CSIframe: pointer to mimo control field of CSI frame, it should
 *         include mimo control field and CSI report field
 *     NESSA: Local NESS;
 *     NESSB: Rx NESS;
 *     BW: Bandwidth of this calibration.
 *         1: 40 MHz, 2: 20 M lower, 3: 20M upper.
 * output: AH_FALSE : calibration fails.
 *          AH_TRUE  : success.
 */
HAL_BOOL
ar9300TxBFRCUpdate(
    struct ath_hal *ah,
    struct ath_rx_status *rx_status,
    u_int8_t *local_h,
    u_int8_t *CSIFrame,
    u_int8_t NESSA,
    u_int8_t NESSB,
    int BW)
{
    u_int8_t  Ntx_A, Nrx_A, Ntx_B, Nrx_B;
    u_int16_t i, k, Ntone;

    HAL_BOOL  okay;
    COMPLEX   (*Hba_eff)[3][Tone_40M];
    COMPLEX   (*H_eff_quan)[3][Tone_40M];
    COMPLEX   Ka[3][Tone_40M];
    u_int8_t  M_H[Tone_40M],SNR[3];
    u_int8_t  Nc, Nr, Nbit_left, ncidx, nridx, ng, nb;
    u_int32_t bitmask, idx, currentdata, RCtmp;
    int       local_h_length;
    u_int16_t mimocontrol;
    char      EVM[6][3];
    u_int32_t tmpvalue;
    u_int16_t j;
    char      count, sum;

    //HAL_CHANNEL_INTERNAL    *chan = AH_PRIVATE(ah)->ah_curchan;

    tmpvalue = OS_REG_READ(ah, AR_PHY_GEN_CTRL);
    /*
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s: Current channel flag %x,BB reg setting %x, Rx flag %x\n",
        __func__, chan->channelFlags, tmpvalue, rx_status->rs_flags);
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s: Rx EVM: %x %x %x %x %x \n",
        __func__, rx_status->evm0, rx_status->evm1,
        rx_status->evm2, rx_status->evm3, rx_status->evm4);
     */

    if (rx_status->rs_flags & HAL_RX_2040) {    // Rx bandwith
        BW = BW_40M;
    } else {
        if (tmpvalue & AR_PHY_GC_DYN2040_PRI_CH) {  // location
            BW = BW_20M_low;
        } else {
            BW = BW_20M_up;
        }
    }
    // check Rx EVM first
    EVM[0][0] =  rx_status->evm0        & 0xff;
    EVM[0][1] = (rx_status->evm0 >>  8) & 0xff;
    EVM[0][2] = (rx_status->evm0 >> 16) & 0xff;
    EVM[1][0] = (rx_status->evm0 >> 24) & 0xff;

    EVM[1][1] =  rx_status->evm1        & 0xff;
    EVM[1][2] = (rx_status->evm1 >>  8) & 0xff;
    EVM[2][0] = (rx_status->evm1 >> 16) & 0xff;
    EVM[2][1] = (rx_status->evm1 >> 24) & 0xff;

    EVM[2][2] =  rx_status->evm2        & 0xff;
    EVM[3][0] = (rx_status->evm2 >>  8) & 0xff;
    EVM[3][1] = (rx_status->evm2 >> 16) & 0xff;
    EVM[3][2] = (rx_status->evm2 >> 24) & 0xff;
 
    EVM[4][0] =  rx_status->evm3        & 0xff;
    EVM[4][1] = (rx_status->evm3 >>  8) & 0xff;
    EVM[4][2] = (rx_status->evm3 >> 16) & 0xff;
    EVM[5][0] = (rx_status->evm3 >> 24) & 0xff;

    EVM[5][1] =  rx_status->evm4        & 0xff;
    EVM[5][2] = (rx_status->evm4 >>  8) & 0xff;

    //HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"==>%s:",__func__);
    for (i = 0; i < 5; i++) {
        //HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"EVM%d ",i);
        sum = count = 0;
        for (j = 0; j < 3; j++) {
            //HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"stream%d %d ,",j,EVM[i][j]);
            if (EVM[i][j] != -128) {
                if (EVM[i][j] != 0) {
                    /*
                    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                        "stream%d %d ,", j, EVM[i][j]);
                     */
                    sum += EVM[i][j];
                    count++;
                }
            }
            //HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"sum %d ;",sum);
        }
        if (count != 0) {
            sum /= count;
            if (sum < EVM_TH) {
                /*
                HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                    "==>%s:RC calculation fail!! EVM too small!! "
                    "sum %d, count %d\n",
                    __func__,sum, count);
                 */
                return AH_FALSE;
            }
        }
    }
    //HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"\n");

    Ntx_A = ar9300GetNTX(ah);
    Nrx_A = ar9300GetNRX(ah);

    if (BW == BW_40M) {
        Ntone = Tone_40M;
    } else {
        Ntone = Tone_20M;
    }
    local_h_length = rx_status->rs_datalen;
#if debugWinRC
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s: local H:length %d",__func__, local_h_length);
    for (i = 0; i < (local_h_length / 9); i++) {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            "%#x, %#x, %#x, %#x, %#x, %#x, %#x, %#x, %#x,\n",
            local_h[9 * i], local_h[9 * i + 1], local_h[9 * i + 2],
            local_h[9 * i + 3], local_h[9 * i + 4], local_h[9 * i + 5],
            local_h[9 * i + 6], local_h[9 * i + 7], local_h[9 * i + 8]);
    }
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "\n");
#endif
#if debugRC
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s: local H:length %d", __func__, local_h_length);
    for (i = 0; i < local_h_length; i++) {
        if (i % 16 == 0) {
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "\n");
        }
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, " %#x,", local_h[i]);
    }
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "\n");
#endif

    Nr = Nrx_A;
    // total bits = 20*Nr*Nc*Ntone
    Nc = (local_h_length * 8) / (20 * Nr * Ntone);
    Ntx_B = Nc;
    if ((Nc < 1) || (Nc > 3)) {
        return AH_FALSE;
    }
    // get CSI2 from CSI report
    /* get required parameters from MIMO control field*/

    mimocontrol = CSIFrame[0] + (CSIFrame[1] << 8);

    Nr = MS(mimocontrol, AR_Nr_idx) + 1;
    Nrx_B = Nr;
    Nc = (mimocontrol & AR_Nc_idx) + 1;

    if (Nc != Ntx_A) {     // Bfee, BFer mismatched
        /*
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            "==>%s:Nc %d, Ntx_a %d, mismatched\n",
            __func__, Nc, Ntx_A);
         */
        return AH_FALSE;
    }
    if ((Nr > 3) | (Nc > 3)) {
        /*
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            "==>%s: Nr %d,Nc %d >3 ",
            __func__, Nr, Nc);
         */
        return AH_FALSE;        // return false for row or col >3
    }
    ng = MS(mimocontrol, AR_Ng);
    if (ng != 0) {
        /*
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            "==>%s: Ng %d, !=0 ", __func__, ng);
         */
        return AH_FALSE;       // should be no group
    }
    switch (MS(mimocontrol, AR_Nb)) {
    case 0:
        nb = 4;
        break;
    case 1:
        nb = 5;
        break;
    case 2:
        nb = 6;
        break;
    case 3:
        nb = 8;
        break;
    default:
        nb = 8;
        break;
    }

    Hba_eff =
        ath_hal_malloc(ah->ah_osdev, 3 * 3 * Tone_40M * sizeof(COMPLEX));
    H_eff_quan =
        ath_hal_malloc(ah->ah_osdev, 3 * 3 * Tone_40M * sizeof(COMPLEX));
    // get CSI1 from local H
    ar9300Get_local_h(ah, local_h, local_h_length, Ntone, Hba_eff);

    idx = 6; // CSI report fiedl from byte 6

    for (i = 0; i < Nr; i++) {
        SNR[i] = CSIFrame[idx++];
    }
    Nbit_left = 8;
    currentdata = CSIFrame[idx++];
    bitmask = (1 << nb) - 1;
    for (k = 0; k < Ntone; k++) {
        if ((Nbit_left - 3) < 0) {  // get magnitude
            currentdata += CSIFrame[idx++] << Nbit_left;
            Nbit_left += 8;       // get 8 bit
        }
        M_H[k] = currentdata & ((1 << 3) - 1);    // get 3 bits for magnitude
        Nbit_left -= 3;
        currentdata = currentdata >> 3; // shift out used bits

        for (nridx = 0; nridx < Nr; nridx++) {
            for (ncidx = 0; ncidx < Nc; ncidx++) {
                if ((Nbit_left - nb) < 0) {
                    currentdata += CSIFrame[idx++] << Nbit_left;
                    Nbit_left += 8;       // get 8 bits
                }
                // get nb bits
                H_eff_quan[nridx][ncidx][k].real = currentdata & bitmask;
                if (H_eff_quan[nridx][ncidx][k].real & (1 << 7)) { //sign bit
                    // two's complement
                    H_eff_quan[nridx][ncidx][k].real -= (1 << 8);
                }
                Nbit_left -= nb;
                currentdata = currentdata >> nb;    // shift out used bits

                if ((Nbit_left - nb) < 0) {
                    currentdata += CSIFrame[idx++] << Nbit_left;
                    Nbit_left += 8;       //get 8bit
                }
                // get nb bits;
                H_eff_quan[nridx][ncidx][k].imag = currentdata & bitmask;
                if (H_eff_quan[nridx][ncidx][k].imag & (1 << 7)) { //sign bit
                    // two's complement
                    H_eff_quan[nridx][ncidx][k].imag -= (1 << 8);
                }
                Nbit_left -= nb;
                currentdata = currentdata >> nb;    // shift out bits
            }
        }
    }
#if debugWinRC
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s: CSI report,length %d ", __func__, idx);
    i = 0;
    while ((i + 16) < idx) {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            " %#x, %#x, %#x, %#x, %#x, %#x, %#x, %#x, "
            "%#x, %#x, %#x, %#x, %#x, %#x, %#x, %#x,\n",
            CSIFrame[i], CSIFrame[i + 1], CSIFrame[i + 2],
            CSIFrame[i + 3], CSIFrame[i + 4], CSIFrame[i + 5],
            CSIFrame[i + 6], CSIFrame[i + 7], CSIFrame[i + 8],
            CSIFrame[i + 9], CSIFrame[i + 10], CSIFrame[i + 11],
            CSIFrame[i + 12], CSIFrame[i + 13], CSIFrame[i + 14],
            CSIFrame[i + 15]);
        i += 16;
    }
    while (i < idx) {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, " %#x,", CSIFrame[i]);
        i++;
    }
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "\n");
#endif
#if debugRC
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s: CSI report,length %d ",__func__,idx);
    for (i = 0; i < idx; i++) {
        if (i % 16 == 0) {
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "\n");
        }
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, " %#x,", CSIFrame[i]);
    }
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "\n");
#endif

    OS_MEMZERO(Ka, sizeof(Ka));
    /*
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s,Ntx_A %x,Nrx_A %x,Ntx_B %x,Nrx_B %x,NESSA %x,NESSB %x, BW %x \n",
        __func__, Ntx_A, Nrx_A, Ntx_B, Nrx_B, NESSA, NESSB, BW);
     */
    okay = Ka_Caculation(
        ah, Ntx_A, Nrx_A, Ntx_B, Nrx_B,
        Hba_eff, H_eff_quan, M_H, Ka, NESSA, NESSB, BW);
    if (okay != AH_TRUE) {
        goto fail;   // Ka (calculation result)invalid
    }

#if (debugRC | debugWinRC)
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "==>%s ka: \n", __func__);
    for (i = 0; i < Ntone; i++) {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            "          %4d %4d, %4d %4d,%4d %4d;\n",
            Ka[0][i].real, Ka[0][i].imag,
            Ka[1][i].real, Ka[1][i].imag,
            Ka[2][i].real, Ka[2][i].imag);
    }
#endif
    //HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"==>%s: BW is %d \n",__func__,BW);
    if (BW == BW_40M) {    //40M
        RCtmp = OS_REG_READ(ah, AR_TXBF);
        RCtmp &= (~AR_TXBF_RC_40_DONE);  // clear done first
        OS_REG_WRITE(ah, AR_TXBF, RCtmp);
        idx = 2 * 4;          // skip -60,-59
        for (i = 0; i < Tone_40M; i++) {
            RCtmp =
                (Ka[0][i].real & 0xff)        |
               ((Ka[0][i].imag & 0xff) <<  8) |
               ((Ka[1][i].real & 0xff) << 16) |
               ((Ka[1][i].imag & 0xff) << 24);
            OS_REG_WRITE(ah, AR_RC0(idx), RCtmp);
            /*
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "tone %d: RC0 %x,", i, RCtmp);
             */
            RCtmp = OS_REG_READ(ah, AR_RC0(idx));
            /*
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                "read back address %x value %x,", AR_RC0(idx), RCtmp);
             */

            RCtmp = (Ka[2][i].real & 0xff) | ((Ka[2][i].imag & 0xff) << 8);
            OS_REG_WRITE(ah, AR_RC1(idx), RCtmp);

            //HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE," RC1 %x,", RCtmp);
            RCtmp = OS_REG_READ(ah, AR_RC1(idx));
            /*
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                "read back address %x value %x\n", AR_RC1(idx), RCtmp);
             */

            idx += 4;
        }
        RCtmp = OS_REG_READ(ah, AR_TXBF);
        RCtmp |= AR_TXBF_RC_40_DONE;     // set 40M done
        OS_REG_WRITE(ah, AR_TXBF, RCtmp);
    } else if (BW == BW_20M_up) { // 20M up
        RCtmp = OS_REG_READ(ah,AR_TXBF);
        RCtmp &= (~AR_TXBF_RC_20_L_DONE);    // clear done first
        OS_REG_WRITE(ah, AR_TXBF, RCtmp);
        idx = 61 * 4;   //58+3 ; place from tone +4 to +60
        for (i = 0; i < Tone_20M / 2; i++) {
            RCtmp =
                 (Ka[0][i].real & 0xff)        |
                ((Ka[0][i].imag & 0xff) <<  8) |
                ((Ka[1][i].real & 0xff) << 16) |
                ((Ka[1][i].imag & 0xff) << 24);
            OS_REG_WRITE(ah,AR_RC0(idx), RCtmp);

            /*
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "tone %d: RC0 %x,", i, RCtmp);
             */
            RCtmp = OS_REG_READ(ah, AR_RC0(idx));
            /*
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                "read back address %x value %x,", AR_RC0(idx), RCtmp);
             */

            RCtmp = (Ka[2][i].real & 0xff) | ((Ka[2][i].imag & 0xff) << 8);
            OS_REG_WRITE(ah, AR_RC1(idx), RCtmp);

            //HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE," RC1 %x,",RCtmp);
            RCtmp = OS_REG_READ(ah, AR_RC1(idx));
            /* 
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                "read back address %x value %x\n", AR_RC1(idx), RCtmp);
             */

            idx += 4;
        }
        idx = 90 * 4;
        k = i;
        for (i = k; i < (Tone_20M / 2 + k); i++) {
            RCtmp =
                 (Ka[0][i].real & 0xff)        |
                ((Ka[0][i].imag & 0xff) <<  8) |
                ((Ka[1][i].real & 0xff) << 16) |
                ((Ka[1][i].imag & 0xff) << 24);
            OS_REG_WRITE(ah, AR_RC0(idx), RCtmp);
            /*
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"tone %d: RC0 %x,", i, RCtmp);
             */
            RCtmp = OS_REG_READ(ah, AR_RC0(idx));
            /*
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                "read back address %x value %x,", AR_RC0(idx), RCtmp);
             */

            RCtmp = (Ka[2][i].real & 0xff) | ((Ka[2][i].imag & 0xff) << 8);
            OS_REG_WRITE(ah, AR_RC1(idx), RCtmp);

            //HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"RC1 %x,",RCtmp);
            RCtmp = OS_REG_READ(ah, AR_RC1(idx));
            /*
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                "read back address %x value %x \n", AR_RC1(idx), RCtmp);
             */

            idx += 4;
        }
        RCtmp = OS_REG_READ(ah, AR_TXBF);
        RCtmp |= AR_TXBF_RC_20_U_DONE;   // set 20M up done
        OS_REG_WRITE(ah, AR_TXBF, RCtmp);
    } else if (BW == BW_20M_low) { // 20M low
        RCtmp = OS_REG_READ(ah, AR_TXBF);
        RCtmp &= (~AR_TXBF_RC_20_U_DONE);    // clear done first
        OS_REG_WRITE(ah,AR_TXBF, RCtmp);
        idx = 0;
        for (i = 0; i < Tone_20M / 2; i++) {
            RCtmp =
                 (Ka[0][i].real & 0xff)        |
                ((Ka[0][i].imag & 0xff) <<  8) |
                ((Ka[1][i].real & 0xff) << 16) |
                ((Ka[1][i].imag & 0xff) << 24);
            OS_REG_WRITE(ah,AR_RC0(idx), RCtmp);
            /*
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "tone %d: RC0 %x,", i, RCtmp);
             */
            RCtmp = OS_REG_READ(ah, AR_RC0(idx));
            /*
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                "read back address %x value %x,", AR_RC0(idx), RCtmp);
             */

            RCtmp = (Ka[2][i].real & 0xff) | ((Ka[2][i].imag & 0xff) << 8);
            OS_REG_WRITE(ah, AR_RC1(idx), RCtmp);

            //HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE," RC1 %x,",RCtmp);
            RCtmp = OS_REG_READ(ah, AR_RC1(idx));
            /*
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                "read back address %x value %x\n", AR_RC1(idx), RCtmp);
             */

            idx += 4;
        }
        idx = 29 * 4;
        k = i;
        for (i = k; i < (Tone_20M / 2 + k); i++) {
            RCtmp =
                 (Ka[0][i].real & 0xff)        |
                ((Ka[0][i].imag & 0xff) <<  8) |
                ((Ka[1][i].real & 0xff) << 16) |
                ((Ka[1][i].imag & 0xff) << 24);
            OS_REG_WRITE(ah, AR_RC0(idx), RCtmp);
            /*
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "tone %d: RC0 %x,", i, RCtmp);
             */
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "tone %d: RC0 %x,", i, RCtmp);
            RCtmp = OS_REG_READ(ah, AR_RC0(idx));
            /*
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                "read back address %x value %x,", AR_RC0(idx), RCtmp);
             */
            RCtmp = (Ka[2][i].real & 0xff) | ((Ka[2][i].imag & 0xff) << 8);
            OS_REG_WRITE(ah, AR_RC1(idx), RCtmp);

            //HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"RC1 %x,",RCtmp);
            RCtmp = OS_REG_READ(ah, AR_RC1(idx));
            /*
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                "read back address %x value %x \n", AR_RC1(idx), RCtmp);
             */

            idx += 4;
        }
        RCtmp = OS_REG_READ(ah, AR_TXBF);
        RCtmp |= AR_TXBF_RC_20_L_DONE;   // set 20M low done
        OS_REG_WRITE(ah, AR_TXBF, RCtmp);
    }
    idx = 0;
    /*
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s:RC read back for 120", __func__);
     */
    /*
    for (i = 0; i < 120; i++) {
        if (i % 4 == 0) {
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "\n");
        }
        RCtmp = OS_REG_READ(ah, AR_RC0(idx));
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            " addr %x:%x", AR_RC0(idx),RCtmp);
        RCtmp=OS_REG_READ(ah, AR_RC1(idx));
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            " addr %x:%x;", AR_RC1(idx),RCtmp);
        idx += 4;
    }
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "\n");
     */
    ath_hal_free(ah, Hba_eff);
    ath_hal_free(ah, H_eff_quan);
    return  AH_TRUE;
fail:
    ath_hal_free(ah, Hba_eff);
    ath_hal_free(ah, H_eff_quan);
    return  AH_FALSE;
}
/*
 * function: ar9300FillCSIFrame
 * purpose: Use local_h and Rx status report to form MIMO control and CSI
 *     report field of CSI action frame.
 * input:
 *     ah:
 *     rx_status: rx status report of received pos_3 frame
 *     BW:        bandwitdth:1:40 M, 0:20M
 *     local_h: pointer to RXDP+12. Which is the beginning point of h
 *         reported by hardware.
 *     local_h_length: length of h;
 *     CSIFrameBody: point to a buffer for CSI frame;
 * output: int: CSI framelength
 */
int
ar9300FillCSIFrame(
    struct ath_hal *ah,
    struct ath_rx_status *rx_status,
    u_int8_t BW,
    u_int8_t *local_h,
    u_int8_t *CSIFrameBody)
{
    u_int8_t    Nc, Nr, nridx, ncidx;
    u_int32_t   idx = 0;
    u_int8_t    bit_idx = 0;
    u_int16_t   mimocontrol, Ntone, i;
    u_int16_t   currentdata = 0;
    COMPLEX     (*H)[3][Tone_40M];
    u_int8_t    *M_H;
    int         local_h_length;


    local_h_length = rx_status->rs_datalen;
    if (rx_status->rs_flags & HAL_RX_2040) {    // Rx bandwith
        BW = BW_40M;
    }
#if debugWinRC
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"==>%s: original H: \n",__func__);
    for (i = 0; i < (local_h_length / 9); i++) {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            "%#x, %#x, %#x, %#x, %#x, %#x, %#x, %#x, %#x,\n",
            local_h[i], local_h[i + 1], local_h[i + 2], local_h[i + 3],
            local_h[i + 4], local_h[i + 5], local_h[i + 6],
            local_h[i + 7], local_h[i + 8], local_h[i + 9]);
    }
#endif

    Nr = ar9300GetNRX(ah);
    if (BW == BW_40M) {
        Ntone = Tone_40M;
    } else {
        Ntone = Tone_20M;
    }

    Nr = ar9300GetNRX(ah);
    // total bits = 20*Nr*Nc*Ntone
    Nc = (local_h_length * 8) / (20 * Nr * Ntone);
    if ((Nc < 1) || (Nc > 3)) {
        return 0;
    }

    H = ath_hal_malloc(ah->ah_osdev, 3 * 3 * Tone_40M * sizeof(COMPLEX));
    M_H = ath_hal_malloc(ah->ah_osdev, Tone_40M);

    mimocontrol =
        SM((Nc - 1), AR_Nc_idx) |
        SM((Nr - 1), AR_Nr_idx) |
        SM(BW, AR_bandwith) |
        SM(3, AR_Nb);
    CSIFrameBody[idx++] = mimocontrol & 0xff;
    CSIFrameBody[idx++] = (mimocontrol >> 8) & 0xff;
    // rx sounding timestamp
    CSIFrameBody[idx++] =  rx_status->rs_tstamp        & 0xff;
    CSIFrameBody[idx++] = (rx_status->rs_tstamp >> 8)  & 0xff;
    CSIFrameBody[idx++] = (rx_status->rs_tstamp >> 16) & 0xff;
    CSIFrameBody[idx++] = (rx_status->rs_tstamp >> 24) & 0xff;
    //Rx chain rsi
    CSIFrameBody[idx++] = rx_status->rs_rssi_ctl0;
    if (Nr > 1) {
        CSIFrameBody[idx++] = rx_status->rs_rssi_ctl1;
    }
    if (Nr > 2) {
        CSIFrameBody[idx++] = rx_status->rs_rssi_ctl2;
    }
    ar9300Get_local_h(ah, local_h, local_h_length, Ntone, H);
    H_to_CSI(ah, Nc, Nr, H, M_H, Ntone);
    currentdata = bit_idx = 0;
    for (i = 0; i < Ntone; i++) {
        currentdata += (M_H[i] & 0x7) << bit_idx;;
        bit_idx += 3;         //add 3 bits for M_H
        if (bit_idx >= 8) {
            CSIFrameBody[idx++] = currentdata & 0xff;  // form one byte
            currentdata >>= 8;
            bit_idx -= 8;
        }
        for (nridx = 0; nridx < Nr; nridx++) {
            for (ncidx = 0; ncidx < Nc; ncidx++) {
                currentdata += (H[nridx][ncidx][i].real & 0xff) << bit_idx;
                bit_idx += 8;         //get new data

                CSIFrameBody[idx++] = currentdata & 0xff;
                currentdata >>= 8;
                bit_idx -= 8;         // form one byte

                currentdata += (H[nridx][ncidx][i].imag & 0xff) << bit_idx;
                bit_idx += 8;         //get new data

                CSIFrameBody[idx++] = currentdata & 0xff;
                currentdata >>= 8;
                bit_idx -= 8;         // form one byte
            }
        }
    }
    ath_hal_free(ah, H);
    ath_hal_free(ah, M_H);
    return idx;
}
#endif

void
ar9300FillTxBFCapabilities(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    HAL_TXBF_CAPS       *txbf = &ahp->txbf_caps;
    struct ath_hal_private  *ahpriv = AH_PRIVATE(ah);
    u_int32_t val;
    u_int8_t txbfCtl;

    OS_MEMZERO(txbf, sizeof(HAL_TXBF_CAPS));
    if (ahpriv->ah_config.ath_hal_txbfCtl == 0) {
        /* doesn't support TxBF, let txbf ie = 0*/
        return;
    }
    /* CEC for osprey is always 1 (2 stream) */ 
    txbf->channel_estimation_cap = 1;

    /* For calibration, always 2 (3 stream) for osprey */
    txbf->csi_max_rows_bfer = 2;
    
    /* Compressed Steering Number of Beamformer Antennas Supported is
    * limited by local's anntena */
    txbf->comp_bfer_antennas = ar9300GetNTX(ah)-1;
    
    /* Compressed Steering Number of Beamformer Antennas Supported
    * is limited by local's anttena */
    txbf->noncomp_bfer_antennas = ar9300GetNTX(ah)-1;
    
    /* NOT SUPPORT CSI */
    txbf->csi_bfer_antennas = 0;
    
    /* 1,2 4 group is supported */
    txbf->minimal_grouping = ALL_GROUP;
   
    /* Explicit compressed Beamforming Feedback Capable */
    if (ahpriv->ah_config.ath_hal_txbfCtl & TxBFCtl_Comp_ExBF_delay_Rpt) {
        /* support delay report by settings */
        txbf->explicit_comp_bf |= Delay_Rpt;
    }
    if (ahpriv->ah_config.ath_hal_txbfCtl & TxBFCtl_Comp_ExBF_Immediately_Rpt) {
        /* support immediately report by settings.*/
        txbf->explicit_comp_bf |= Immediately_Rpt;
    }

    //Explicit non-Compressed Beamforming Feedback Capable
    if (ahpriv->ah_config.ath_hal_txbfCtl & TxBFCtl_Non_ExBF_delay_Rpt) {
        txbf->explicit_noncomp_bf |= Delay_Rpt;
    }
    if (ahpriv->ah_config.ath_hal_txbfCtl & TxBFCtl_Non_ExBF_Immediately_Rpt) {
        txbf->explicit_noncomp_bf |= Immediately_Rpt;
    }

    /* not support csi feekback */
    txbf->explicit_csi_feedback = 0; 

    /* Explicit compressed Steering Capable from settings */
    txbf->explicit_comp_steering =
        MS(ahpriv->ah_config.ath_hal_txbfCtl, TxBFCtl_Comp_ExBF);
        
    /* Explicit Non-compressed Steering Capable from settings */
    txbf->explicit_noncomp_steering =
        MS(ahpriv->ah_config.ath_hal_txbfCtl, TxBFCtl_Non_ExBF);
    
    /* not support CSI */
    txbf->explicit_csi_txbf_capable = AH_FALSE; 
    
#ifdef TXBF_TODO
    /* initial and respond calibration */
    txbf->calibration = INIT_RESP_CAL;
    
    /* set implcit by settings */
    txbf->implicit_txbf_capable = MS(ahpriv->ah_config.ath_hal_txbfCtl, ImBF);
    txbf->implicit_rx_capable = MS(ahpriv->ah_config.ath_hal_txbfCtl, ImBF_FB);
#else
    /* not support imbf and calibration */
    txbf->calibration = No_CALIBRATION;
    txbf->implicit_txbf_capable = AH_FALSE;
    txbf->implicit_rx_capable  = AH_FALSE;
#endif
    /* not support NDP */
    txbf->tx_ndp_capable = AH_FALSE;    
    txbf->rx_ndp_capable = AH_FALSE;
    
    /* support stagger sounding. */
    txbf->tx_staggered_sounding = AH_TRUE;  
    txbf->rx_staggered_sounding = AH_TRUE;

    /* set immediately or delay report to H/W */
    val = OS_REG_READ(ah, AR_H_XFER_TIMEOUT);
    txbfCtl = ahpriv->ah_config.ath_hal_txbfCtl;
    if (((txbfCtl & TxBFCtl_Non_ExBF_Immediately_Rpt) == 0) &&
        ((txbfCtl & TxBFCtl_Comp_ExBF_Immediately_Rpt) == 0)) {
        val &= ~(AR_EXBF_IMMDIATE_RESP);
        val |= AR_EXBF_NOACK_NO_RPT;
        OS_REG_WRITE(ah, AR_H_XFER_TIMEOUT, val); // enable delayed report
    } else {
        val |= AR_EXBF_IMMDIATE_RESP;
        val &= ~(AR_EXBF_NOACK_NO_RPT);
        OS_REG_WRITE(ah,AR_H_XFER_TIMEOUT, val); // enable immediately report
    }
    /*
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%sTxBfCtl= %x \n", __func__, ahpriv->ah_config.ath_hal_txbfCtl);
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s:Compress ExBF= %x FB %x\n",
        __func__, txbf->explicit_comp_steering, txbf->explicit_comp_bf);
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s:NonCompress ExBF= %x FB %x\n",
        __func__, txbf->explicit_noncomp_steering, txbf->explicit_noncomp_bf);
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s:ImBF= %x FB %x\n",
        __func__, txbf->implicit_txbf_capable, txbf->implicit_rx_capable);
     */
}

HAL_TXBF_CAPS *
ar9300GetTxBFCapabilities(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);

    return &ahp->txbf_caps;
}

/* ar9300TxBFSetKey is used to set TXBF related field in key cache.
*/
void ar9300TxBFSetKey(
    struct ath_hal *ah,
    u_int16_t entry,
    u_int8_t rx_staggered_sounding,
    u_int8_t channel_estimation_cap,
    u_int8_t MMSS)
{
    u_int32_t tmp, txbf;

    // 1 for 2 stream, 0 for 1 stream , should add 1 for H/W
    channel_estimation_cap += 1;

    txbf = (
        SM(rx_staggered_sounding,AR_KEYTABLE_STAGGED) |
        SM(channel_estimation_cap,AR_KEYTABLE_CEC)    |
        SM(MMSS,AR_KEYTABLE_MMSS));
    tmp = OS_REG_READ(ah, AR_KEYTABLE_TYPE(entry)); //get original value;
    if (txbf !=
        (tmp & (AR_KEYTABLE_STAGGED | AR_KEYTABLE_CEC | AR_KEYTABLE_MMSS))) {
        // update key cache for txbf
        OS_REG_WRITE(ah, AR_KEYTABLE_TYPE(entry), tmp | txbf);
        /*
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            "==>%s: update keyid %d, value %x,orignal %x\n",
            __func__, entry, txbf, tmp);
         */
    }
/*
    else {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s: parameters no changes : %x, don't need updtate key!\n",
        __func__,tmp);
    }
 */
}

/* ar9300ReadKeyCacheMac is used to read back "entry" keycache's MAC address
 *    it will return false if bus is not accessable.  
 */

HAL_BOOL
ar9300ReadKeyCacheMac(struct ath_hal *ah, u_int16_t entry,u_int8_t *mac)
{
    u_int32_t macHi, macLo;

    macLo = OS_REG_READ(ah, AR_KEYTABLE_MAC0(entry));
    macHi = OS_REG_READ(ah, AR_KEYTABLE_MAC1(entry));

    if ((macHi == 0xdeadbeef) || (macLo == 0xdeadbeef)) {
        return AH_FALSE;
    }

    macHi <<= 1;
    macHi |= (macLo >> 31);
    macLo <<= 1;
    /*
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s: set read keyid %d, MAC low %x: %x,Mac high %x: %x\n",
        __func__, entry, AR_KEYTABLE_MAC0(entry), macHi,
        AR_KEYTABLE_MAC1(entry), macLo); //hcl
     */
    mac[0] =  macLo        & 0xff;
    mac[1] = (macLo >>  8) & 0xff;
    mac[2] = (macLo >> 16) & 0xff;
    mac[3] = (macLo >> 24) & 0xff;
    mac[4] =  macHi        & 0xff;
    mac[5] = (macHi >>  8) & 0xff;
    return AH_TRUE;
}


/*
 * Limit self-generate frame rate (such as CV report ) by lowest Tx rate to
 * guarantee the success of CV report frame 
 */
void
ar9300_set_selfgenrate_limit(struct ath_hal *ah, u_int8_t rateidx)
{
    struct      ath_hal_private *ahp = AH_PRIVATE(ah);
    u_int32_t   selfgen_rate; 
   
    if (rateidx & HT_RATE){
        rateidx &= ~ (HT_RATE);
        if (rateidx < ahp->ah_lowest_txrate){
            ahp->ah_lowest_txrate = rateidx;
            selfgen_rate = (1 << ((ahp->ah_lowest_txrate)+ 1))- 1;
            if (selfgen_rate !=  ahp->ah_basic_set_buf){
                ahp->ah_basic_set_buf = selfgen_rate;
                OS_REG_WRITE(ah, AR_BASIC_SET, ahp->ah_basic_set_buf);
            }
        }
    }
}

/* reset current lowest Tx rate to max MCS*/
void
ar9300_reset_lowest_txrate(struct ath_hal *ah)
{
    struct ath_hal_private *ahp = AH_PRIVATE(ah);

    /* reset rate to maximum tx rate*/
    ahp->ah_lowest_txrate = MAXMCSRATE;
}   

#endif /* ATH_SUPPORT_TxBF*/
#endif /* AH_SUPPORT_AR9300 */
