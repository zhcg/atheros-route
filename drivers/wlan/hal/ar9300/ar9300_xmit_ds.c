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
#include "ah_xr.h"
#include "ah_internal.h"

#include "ar9300/ar9300desc.h"
#include "ar9300/ar9300.h"
#include "ar9300/ar9300reg.h"
#include "ar9300/ar9300phy.h"
#include "ah_devid.h"

#if AH_BYTE_ORDER == AH_BIG_ENDIAN
static void ar9300SwapTxDesc(void *ds);
#endif

void
ar9300TxReqIntrDesc(struct ath_hal *ah, void *ds)
{
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "%s:Desc Interrupt not supported\n", __func__);
}

static inline u_int16_t
ar9300CalcPtrChkSum(struct ar9300_txc *ads)
{
    u_int checksum;
    u_int16_t ptrchecksum;

    //checksum = __bswap32(ads->ds_info) + ads->ds_link
    checksum =    ads->ds_info + ads->ds_link
                + ads->ds_data0 + ads->ds_ctl3
                + ads->ds_data1 + ads->ds_ctl5
                + ads->ds_data2 + ads->ds_ctl7
                + ads->ds_data3 + ads->ds_ctl9;

    ptrchecksum = ((checksum & 0xffff) + (checksum >> 16)) & AR_TxPtrChkSum;
    return ptrchecksum;
}

#ifdef AR9300_EMULATION
void dumpdesc(struct ar9300_txc *ads)
{
    unsigned int *desc;
    int i;

    desc = (unsigned int *)ads;
    for (i = 0; i < 23; i += 4) {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            "%08x %08x %08x %08x\n",
                desc[0 + i], desc[1 + i], desc[2 + i], desc[3 + i]);
    }
}
#endif

HAL_BOOL
ar9300FillTxDesc(
    struct ath_hal *ah,
    void *ds,
    dma_addr_t *bufAddr,
    u_int32_t *segLen,
    u_int descId,
    u_int qcu,
    HAL_KEY_TYPE keyType,
    HAL_BOOL firstSeg,
    HAL_BOOL lastSeg,
    const void *ds0)
{
    struct ar9300_txc *ads = AR9300TXC(ds);

    /* Fill TXC info field */
    ads->ds_info = TXC_INFO(qcu);

    /* Set the buffer addresses */
    ads->ds_data0 = bufAddr[0];
    ads->ds_data1 = bufAddr[1];
    ads->ds_data2 = bufAddr[2];
    ads->ds_data3 = bufAddr[3];

    /* Set the buffer lengths */
    ads->ds_ctl3 = (segLen[0] << AR_BufLen_S) & AR_BufLen;
    ads->ds_ctl5 = (segLen[1] << AR_BufLen_S) & AR_BufLen;
    ads->ds_ctl7 = (segLen[2] << AR_BufLen_S) & AR_BufLen;
    ads->ds_ctl9 = (segLen[3] << AR_BufLen_S) & AR_BufLen;

    /* Fill in pointer checksum and descriptor id */
    ads->ds_ctl10 = (descId << AR_TxDescId_S) | ar9300CalcPtrChkSum(ads);

    if (firstSeg) {
        /*
         * First descriptor, don't clobber xmit control data
         * setup by ar9300Set11nTxDesc.
         *
         * Note: AR_EncrType is already setup in the first descriptor by
         *       Set11nTxDesc().
         */
        ads->ds_ctl12 |= (lastSeg ? 0 : AR_TxMore);
    } else if (lastSeg) { /* !firstSeg && lastSeg */
        /*
         * Last descriptor in a multi-descriptor frame,
         * copy the multi-rate transmit parameters from
         * the first frame for processing on completion.
         */
        ads->ds_ctl11 = 0;
        ads->ds_ctl12 = 0;
#ifdef AH_NEED_DESC_SWAP
        ads->ds_ctl13 = __bswap32(AR9300TXC_CONST(ds0)->ds_ctl13);
        ads->ds_ctl14 = __bswap32(AR9300TXC_CONST(ds0)->ds_ctl14);
        ads->ds_ctl17 = __bswap32(SM(keyType, AR_EncrType));
#else
        ads->ds_ctl13 = AR9300TXC_CONST(ds0)->ds_ctl13;
        ads->ds_ctl14 = AR9300TXC_CONST(ds0)->ds_ctl14;
        ads->ds_ctl17 = SM(keyType, AR_EncrType);
#endif
    } else { /* !firstSeg && !lastSeg */
        /*
         * XXX Intermediate descriptor in a multi-descriptor frame.
         */
        ads->ds_ctl11 = 0;
        ads->ds_ctl12 = AR_TxMore;
        ads->ds_ctl13 = 0;
        ads->ds_ctl14 = 0;
        ads->ds_ctl17 = SM(keyType, AR_EncrType);
    }

    return AH_TRUE;
}

/*
 * Fill encryption key type in descriptor, this is called
 * while sending encrypted challenge response in shared
 * authentication.
 */
HAL_BOOL
ar9300FillKeyTxDesc(struct ath_hal *ah, void *ds, HAL_KEY_TYPE keyType)
{
    struct ar9300_txc *ads = AR9300TXC(ds);

    ads->ds_ctl17 = SM(keyType, AR_EncrType);
    return AH_TRUE;
}

void
ar9300SetDescLink(struct ath_hal *ah, void *ds, u_int32_t link)
{
    struct ar9300_txc *ads = AR9300TXC(ds);

    ads->ds_link = link;

    /* TODO - checksum is calculated twice for subframes
     * Once in filldesc and again when linked. Need to fix.
     */
    /* Fill in pointer checksum.  Preserve descriptor id */
    ads->ds_ctl10 &= ~AR_TxPtrChkSum;
    ads->ds_ctl10 |= ar9300CalcPtrChkSum(ads);
}

#ifdef _WIN64
void
ar9300GetDescLinkPtr(struct ath_hal *ah, void *ds, u_int32_t __unaligned **link)
#else
void
ar9300GetDescLinkPtr(struct ath_hal *ah, void *ds, u_int32_t **link)
#endif
{
    struct ar9300_txc *ads = AR9300TXC(ds);

    *link = &ads->ds_link;
}

void
ar9300ClearTxDescStatus(struct ath_hal *ah, void *ds)
{
    struct ar9300_txs *ads = AR9300TXS(ds);
    ads->status1 = ads->status2 = 0;
    ads->status3 = ads->status4 = 0;
    ads->status5 = ads->status6 = 0;
    ads->status7 = ads->status8 = 0;
}

#ifdef ATH_SWRETRY
void
ar9300ClearDestMask(struct ath_hal *ah, void *ds)
{
    struct ar9300_txc *ads = AR9300TXC(ds);
    ads->ds_ctl11 |= AR_ClrDestMask;
}
#endif

#if AH_BYTE_ORDER == AH_BIG_ENDIAN
/* XXX what words need swapping */
/* Swap transmit descriptor */
static __inline void
ar9300SwapTxDesc(void *dsp)
{
    struct ar9300_txs *ds = (struct ar9300_txs *)dsp;

    ds->ds_info = __bswap32(ds->ds_info);
    ds->status1 = __bswap32(ds->status1);
    ds->status2 = __bswap32(ds->status2);
    ds->status3 = __bswap32(ds->status3);
    ds->status4 = __bswap32(ds->status4);
    ds->status5 = __bswap32(ds->status5);
    ds->status6 = __bswap32(ds->status6);
    ds->status7 = __bswap32(ds->status7);
    ds->status8 = __bswap32(ds->status8);
}
#endif


/*
 * Extract the transmit rate code.
 */
void
ar9300GetTxRateCode(struct ath_hal *ah, void *ds, struct ath_tx_status *ts)
{
    struct ar9300_txc *ads = AR9300TXC(ds);

    switch (ts->ts_rateindex) {
    case 0:
        ts->ts_ratecode = MS(ads->ds_ctl14, AR_XmitRate0);
        break;
    case 1:
        ts->ts_ratecode = MS(ads->ds_ctl14, AR_XmitRate1);
        break;
    case 2:
        ts->ts_ratecode = MS(ads->ds_ctl14, AR_XmitRate2);
        break;
    case 3:
        ts->ts_ratecode = MS(ads->ds_ctl14, AR_XmitRate3);
        break;
    }
    ar9300_set_selfgenrate_limit(ah, ts->ts_ratecode);
}

/*
 * Get TX Status descriptor contents.
 */
void
ar9300GetRawTxDesc(struct ath_hal *ah, u_int32_t *txstatus)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ar9300_txs *ads;

    ads = &ahp->ts_ring[ahp->ts_tail];

    OS_MEMCPY(txstatus, ads, sizeof(struct ar9300_txs));
}

/*
 * Processing of HW TX descriptor.
 */
HAL_STATUS
ar9300ProcTxDesc(struct ath_hal *ah, void *txstatus)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ar9300_txs *ads;
    struct ath_tx_status *ts = (struct ath_tx_status *)txstatus;
    u_int32_t dsinfo;

    ads = &ahp->ts_ring[ahp->ts_tail];

    if ((ads->status8 & AR_TxDone) == 0) {
        return HAL_EINPROGRESS;
    }
    /* Increment the tail to point to the next status element. */
    ahp->ts_tail = (ahp->ts_tail + 1) & (ahp->ts_size-1);

    /*
    ** For big endian systems, ds_info is not swapped as the other
    ** registers are.  Ensure we use the bswap32 version (which is
    ** defined to "nothing" in little endian systems
    */

    /*
     * Sanity check
     */

//    ath_hal_printf(ah,"CHH: ds_info 0x%x  status1: 0x%x  status8: 0x%x\n",
//                        ads->ds_info,ads->status1,ads->status8);

    dsinfo = ads->ds_info;

    if ((MS(dsinfo, AR_DescId) != ATHEROS_VENDOR_ID) ||
        (MS(dsinfo, AR_TxRxDesc) != 1))
    {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "%s: Tx Descriptor error %x\n",
                 __func__, dsinfo);
        HALASSERT(0);
        /* Zero out the status for reuse */
        OS_MEMZERO(ads, sizeof(struct ar9300_txs));
        return HAL_EIO;
    }

    /* Update software copies of the HW status */
    ts->queue_id = MS(dsinfo, AR_TxQcuNum);
    ts->desc_id = MS(ads->status1, AR_TxDescId);
    ts->ts_seqnum = MS(ads->status8, AR_SeqNum);
    ts->ts_tstamp = ads->status4;
    ts->ts_status = 0;
    ts->ts_flags  = 0;

    if (ads->status3 & AR_ExcessiveRetries) {
        ts->ts_status |= HAL_TXERR_XRETRY;
    }
    if (ads->status3 & AR_Filtered) {
        ts->ts_status |= HAL_TXERR_FILT;
    }
    if (ads->status3 & AR_FIFOUnderrun) {
        ts->ts_status |= HAL_TXERR_FIFO;
        ar9300UpdateTxTrigLevel(ah, AH_TRUE);
    }
    if (ads->status8 & AR_TxOpExceeded) {
        ts->ts_status |= HAL_TXERR_XTXOP;
    }
    if (ads->status3 & AR_TxTimerExpired) {
        ts->ts_status |= HAL_TXERR_TIMER_EXPIRED;
    }
    if (ads->status3 & AR_DescCfgErr) {
        ts->ts_flags |= HAL_TX_DESC_CFG_ERR;
    }
    if (ads->status3 & AR_TxDataUnderrun) {
        ts->ts_flags |= HAL_TX_DATA_UNDERRUN;
        ar9300UpdateTxTrigLevel(ah, AH_TRUE);
    }
    if (ads->status3 & AR_TxDelimUnderrun) {
        ts->ts_flags |= HAL_TX_DELIM_UNDERRUN;
        ar9300UpdateTxTrigLevel(ah, AH_TRUE);
    }
    if (ads->status2 & AR_TxBaStatus) {
        ts->ts_flags |= HAL_TX_BA;
        ts->ba_low = ads->status5;
        ts->ba_high = ads->status6;
    }

    /*
     * Extract the transmit rate.
     */
    ts->ts_rateindex = MS(ads->status8, AR_FinalTxIdx);

    ts->ts_rssi = MS(ads->status7, AR_TxRSSICombined);
    ts->ts_rssi_ctl0 = MS(ads->status2, AR_TxRSSIAnt00);
    ts->ts_rssi_ctl1 = MS(ads->status2, AR_TxRSSIAnt01);
    ts->ts_rssi_ctl2 = MS(ads->status2, AR_TxRSSIAnt02);
    ts->ts_rssi_ext0 = MS(ads->status7, AR_TxRSSIAnt10);
    ts->ts_rssi_ext1 = MS(ads->status7, AR_TxRSSIAnt11);
    ts->ts_rssi_ext2 = MS(ads->status7, AR_TxRSSIAnt12);
    ts->ts_shortretry = MS(ads->status3, AR_RTSFailCnt);
    ts->ts_longretry = MS(ads->status3, AR_DataFailCnt);
    ts->ts_virtcol = MS(ads->status3, AR_VirtRetryCnt);
    ts->ts_antenna = 0;

#ifdef  ATH_SUPPORT_TxBF
    /*
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s:mac version %x", __func__, ap->ah_macRev);
     */
    ts->ts_txbfstatus = 0;
    if (ads->status8 & AR_TxBF_BW_Mismatch) {
        ts->ts_txbfstatus |= AR_BW_Mismatch;
    }
    if (ads->status8 & AR_TxBF_Stream_Miss) {
        ts->ts_txbfstatus |= AR_Stream_Miss;
    }
    if (ads->status8 & AR_TxBF_Dest_Miss) {
        ts->ts_txbfstatus |= AR_Dest_Miss;
    }
    if (ads->status8 & AR_TxBF_Expired) {
        ts->ts_txbfstatus |= AR_Expired;
    }
    /*
    if (ts->ts_txbfstatus != 0) {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            "==>%s: txbf status %x for osprey2.0\n",
            __func__, ts->ts_txbfstatus);
    }
     */
#endif
    /* extract TID from block ack */
    ts->tid = MS(ads->status8, AR_TxTid);

    /* Zero out the status for reuse */
    OS_MEMZERO(ads, sizeof(struct ar9300_txs));

    return HAL_OK;
}

/*
 * Calculate air time of a transmit packet
 * if comp_wastedt is 1, calculate air time only for failed subframes
 * this is required for VOW_DCS ( dynamic channel selection )
 */
u_int32_t
ar9300CalcTxAirtime(struct ath_hal *ah, void *ds, struct ath_tx_status *ts, 
        HAL_BOOL comp_wastedt, u_int8_t nbad, u_int8_t nframes )
{
    struct ar9300_txc *ads = AR9300TXC(ds);
    int finalindex_tries;
    u_int32_t airtime, lastrate_dur;
    

    /*
     * Number of attempts made on the final index
     * Note: If no BA was recv, then the data_fail_cnt is the number of tries
     * made on the final index.  If BA was recv, then add 1 to account for the
     * successful attempt.
     */
    if ( !comp_wastedt ){
        finalindex_tries = ts->ts_longretry + (ts->ts_flags & HAL_TX_BA)? 1 : 0;
    } else {
        finalindex_tries = ts->ts_longretry ;
    }

    /*
     * Calculate time of transmit on air for packet including retries
     * at different rates.
     */
    switch (ts->ts_rateindex) {
    case 0:
        lastrate_dur = MS(ads->ds_ctl15, AR_PacketDur0);
        airtime = (lastrate_dur * finalindex_tries);
        break;
    case 1:
        lastrate_dur = MS(ads->ds_ctl15, AR_PacketDur1);
        airtime = (lastrate_dur * finalindex_tries) +
            (MS(ads->ds_ctl13, AR_XmitDataTries0) *
             MS(ads->ds_ctl15, AR_PacketDur0));
        break;
    case 2:
        lastrate_dur = MS(ads->ds_ctl16, AR_PacketDur2);
        airtime = (lastrate_dur * finalindex_tries) +
            (MS(ads->ds_ctl13, AR_XmitDataTries1) *
             MS(ads->ds_ctl15, AR_PacketDur1)) +
            (MS(ads->ds_ctl13, AR_XmitDataTries0) *
             MS(ads->ds_ctl15, AR_PacketDur0));
        break;
    case 3:
        lastrate_dur = MS(ads->ds_ctl16, AR_PacketDur3);
        airtime = (lastrate_dur * finalindex_tries) +
            (MS(ads->ds_ctl13, AR_XmitDataTries2) *
             MS(ads->ds_ctl16, AR_PacketDur2)) +
            (MS(ads->ds_ctl13, AR_XmitDataTries1) *
             MS(ads->ds_ctl15, AR_PacketDur1)) +
            (MS(ads->ds_ctl13, AR_XmitDataTries0) *
             MS(ads->ds_ctl15, AR_PacketDur0));
        break;
    default:
        HALASSERT(0);
        return 0;
    }

    if ( comp_wastedt && (ts->ts_flags & HAL_TX_BA)){
        airtime += nbad?((lastrate_dur*nbad) / nframes):0;  
    }
    return airtime;

}

#ifdef AH_PRIVATE_DIAG
void
ar9300_ContTxMode(struct ath_hal *ah, void *ds, int mode)
{
#if 0
    static int qnum = 0;
    int i;
    unsigned int qbits, val, val1, val2;
    int prefetch;
    struct ar9300_txs *ads = AR9300TXS(ds);

    if (mode == 10) {
        return;
    }

    if (mode == 7) {                      // print status from the cont tx desc
        if (ads) {
            val1 = ads->ds_txstatus1;
            val2 = ads->ds_txstatus2;
            HDPRINTF(ah, HAL_DBG_TXDESC, "s0(%x) s1(%x)\n",
                                       (unsigned)val1, (unsigned)val2);
        }
        HDPRINTF(ah, HAL_DBG_TXDESC, "txe(%x) txd(%x)\n",
                                   OS_REG_READ(ah, AR_Q_TXE),
                                   OS_REG_READ(ah, AR_Q_TXD)
                );
        for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
            val = OS_REG_READ(ah, AR_QTXDP(i));
            val2 = OS_REG_READ(ah, AR_QSTS(i)) & AR_Q_STS_PEND_FR_CNT;
            HDPRINTF(ah, HAL_DBG_TXDESC, "[%d] %x %d\n", i, val, val2);
        }
        return;
    }
    if (mode == 8) {                      // set TXE for qnum
        OS_REG_WRITE(ah, AR_Q_TXE, 1 << qnum);
        return;
    }
    if (mode == 9) {
        prefetch = (int)ds;
        return;
    }

    if (mode >= 1) {                    // initiate cont tx operation
        /* Disable AGC to A2 */
        qnum = (int) ds;

        OS_REG_WRITE(ah, AR_PHY_TEST,
            (OS_REG_READ(ah, AR_PHY_TEST) | PHY_AGC_CLR) );

        OS_REG_WRITE(ah, 0x9864, OS_REG_READ(ah, 0x9864) | 0x7f000);
        OS_REG_WRITE(ah, 0x9924, OS_REG_READ(ah, 0x9924) | 0x7f00fe);
        OS_REG_WRITE(ah, AR_DIAG_SW,
            (OS_REG_READ(ah, AR_DIAG_SW) |
             (AR_DIAG_FORCE_RX_CLEAR + AR_DIAG_IGNORE_VIRT_CS)) );


        OS_REG_WRITE(ah, AR_CR, AR_CR_RXD);     // set receive disable

        if (mode == 3 || mode == 4) {
            int txcfg;

            if (mode == 3) {
                OS_REG_WRITE(ah, AR_DLCL_IFS(qnum), 0);
                OS_REG_WRITE(ah, AR_DRETRY_LIMIT(qnum), 0xffffffff);
                OS_REG_WRITE(ah, AR_D_GBL_IFS_SIFS, 100);
                OS_REG_WRITE(ah, AR_D_GBL_IFS_EIFS, 100);
                OS_REG_WRITE(ah, AR_TIME_OUT, 2);
                OS_REG_WRITE(ah, AR_D_GBL_IFS_SLOT, 100);
            }

            OS_REG_WRITE(ah, AR_DRETRY_LIMIT(qnum), 0xffffffff);
            OS_REG_WRITE(ah, AR_D_FPCTL, 0x10|qnum); // enable prefetch on qnum
            txcfg = 5 | (6 << AR_FTRIG_S);
            OS_REG_WRITE(ah, AR_TXCFG, txcfg);

            OS_REG_WRITE(ah, AR_QMISC(qnum),        // set QCU modes
                         AR_Q_MISC_DCU_EARLY_TERM_REQ
                         + AR_Q_MISC_FSP_ASAP
                         + AR_Q_MISC_CBR_INCR_DIS1
                         + AR_Q_MISC_CBR_INCR_DIS0
                        );

            /* stop tx dma all all except qnum */
            qbits = 0x3ff;
            qbits &= ~(1 << qnum);
            for (i = 0; i < 10; i++) {
                if (i == qnum) {
                    continue;
                }
                OS_REG_WRITE(ah, AR_Q_TXD, 1 << i);
            }

            OS_REG_WRITE(ah, AR_Q_TXD, qbits);

            /* clear and freeze MIB counters */
            OS_REG_WRITE(ah, AR_MIBC, AR_MIBC_CMC);
            OS_REG_WRITE(ah, AR_MIBC, AR_MIBC_FMC);

            OS_REG_WRITE(ah, AR_DMISC(qnum),
                         (AR_D_MISC_ARB_LOCKOUT_CNTRL_GLOBAL <<
                          AR_D_MISC_ARB_LOCKOUT_CNTRL_S)
                         + (AR_D_MISC_ARB_LOCKOUT_IGNORE)
                         + (AR_D_MISC_POST_FR_BKOFF_DIS)
                         + (AR_D_MISC_VIR_COL_HANDLING_IGNORE <<
                            AR_D_MISC_VIR_COL_HANDLING_S));

            for (i = 0; i < HAL_NUM_TX_QUEUES + 2; i++) {  // disconnect QCUs
                if (i == qnum) {
                    continue;
                }
                OS_REG_WRITE(ah, AR_DQCUMASK(i), 0);
            }
        }
    }
    if (mode == 0) {
        OS_REG_WRITE(ah, AR_PHY_TEST,
            (OS_REG_READ(ah, AR_PHY_TEST) & ~PHY_AGC_CLR));
        OS_REG_WRITE(ah, AR_DIAG_SW,
            (OS_REG_READ(ah, AR_DIAG_SW) &
             ~(AR_DIAG_FORCE_RX_CLEAR + AR_DIAG_IGNORE_VIRT_CS)));
    }
#endif
}
#endif

void
ar9300SetPAPRDTxDesc(struct ath_hal *ah, void *ds, int chainNum)
{
    struct ar9300_txc *ads = AR9300TXC(ds);

    ads->ds_ctl12 |= SM((1 << chainNum), AR_PAPRDChainMask);
}
HAL_STATUS
ar9300IsTxDone(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ar9300_txs *ads;

    ads = &ahp->ts_ring[ahp->ts_tail];

    if (ads->status8 & AR_TxDone) {
        return HAL_OK;
    }
    return HAL_EINPROGRESS;
}

void
ar9300Set11nTxDesc(
    struct ath_hal *ah,
    void *ds,
    u_int pktLen,
    HAL_PKT_TYPE type,
    u_int txPower,
    u_int keyIx,
    HAL_KEY_TYPE keyType,
    u_int flags)
{
    struct ar9300_txc *ads = AR9300TXC(ds);
    struct ath_hal_9300 *ahp = AH9300(ah);

    HALASSERT(isValidPktType(type));
    HALASSERT(isValidKeyType(keyType));

    txPower += ahp->ah_txPowerIndexOffset;
    if (txPower > 63) {
        txPower = 63;
    }
    ads->ds_ctl11 =
        (pktLen & AR_FrameLen)
      | (flags & HAL_TXDESC_VMF ? AR_VirtMoreFrag : 0)
      | SM(txPower, AR_XmitPower0)
      | (flags & HAL_TXDESC_VEOL ? AR_VEOL : 0)
      | (flags & HAL_TXDESC_CLRDMASK ? AR_ClrDestMask : 0)
      | (keyIx != HAL_TXKEYIX_INVALID ? AR_DestIdxValid : 0)
      | (flags & HAL_TXDESC_LOWRXCHAIN ? AR_LowRxChain : 0);

    ads->ds_ctl12 =
        (keyIx != HAL_TXKEYIX_INVALID ? SM(keyIx, AR_DestIdx) : 0)
      | SM(type, AR_FrameType)
      | (flags & HAL_TXDESC_NOACK ? AR_NoAck : 0)
      | (flags & HAL_TXDESC_EXT_ONLY ? AR_ExtOnly : 0)
      | (flags & HAL_TXDESC_EXT_AND_CTL ? AR_ExtAndCtl : 0);

    ads->ds_ctl17 =
        SM(keyType, AR_EncrType) | (flags & HAL_TXDESC_LDPC ? AR_LDPC : 0);

    ads->ds_ctl18 = 0;
    ads->ds_ctl19 = AR_Not_Sounding; /* set not sounding for normal frame */

    /* Clear Ness1/2/3 (Number of Extension Spatial Streams) fields.
     * Ness0 is cleared in ctl19.  See EV66059 (BB panic).
     */
    ads->ds_ctl20 = 0;
    ads->ds_ctl21 = 0;
    ads->ds_ctl22 = 0;
}

#if 0
#define HT_RC_2_MCS(_rc)        ((_rc) & 0x0f)
static const u_int8_t baDurationDelta[] = {
    24,     //  0: BPSK
    12,     //  1: QPSK 1/2
    12,     //  2: QPSK 3/4
     4,     //  3: 16-QAM 1/2
     4,     //  4: 16-QAM 3/4
     4,     //  5: 64-QAM 2/3
     4,     //  6: 64-QAM 3/4
     4,     //  7: 64-QAM 5/6
    24,     //  8: BPSK
    12,     //  9: QPSK 1/2
    12,     // 10: QPSK 3/4
     4,     // 11: 16-QAM 1/2
     4,     // 12: 16-QAM 3/4
     4,     // 13: 64-QAM 2/3
     4,     // 14: 64-QAM 3/4
     4,     // 15: 64-QAM 5/6
};
#endif


u_int8_t
ar9300GetTxMode(u_int rateFlags)
{

#ifdef ATH_SUPPORT_TxBF
        /* Check whether TXBF is enabled */
        if (rateFlags & HAL_RATESERIES_TXBF) {
            return AR9300_TXBF_MODE;
        }
#endif
        /* Check whether STBC is enabled if TxBF is not enabled */
        if (rateFlags & HAL_RATESERIES_STBC){
            return AR9300_STBC_MODE;
        }
        return AR9300_DEF_MODE;
}

void
ar9300Set11nRateScenario(
    struct ath_hal *ah,
    void *ds,
    void *lastds,
    u_int durUpdateEn,
    u_int rtsctsRate,
    u_int rtsctsDuration,
    HAL_11N_RATE_SERIES series[],
    u_int nseries,
    u_int flags, 
    u_int32_t smart_antenna)
{
    struct ath_hal_private *ap = AH_PRIVATE(ah);
    struct ar9300_txc *ads = AR9300TXC(ds);
    struct ar9300_txc *last_ads = AR9300TXC(lastds);
    u_int32_t ds_ctl11;
    u_int8_t ant, calPkt = 0;
    u_int mode, txMode=AR9300_DEF_MODE;

    HALASSERT(nseries == 4);
    (void)nseries;
    (void)rtsctsDuration;   /* use H/W to calculate RTSCTSDuration */

    ds_ctl11 = ads->ds_ctl11;
    /*
     * Rate control settings override
     */
    if (flags & (HAL_TXDESC_RTSENA | HAL_TXDESC_CTSENA)) {
        if (flags & HAL_TXDESC_RTSENA) {
            ds_ctl11 &= ~AR_CTSEnable;
            ds_ctl11 |= AR_RTSEnable;
        } else {
            ds_ctl11 &= ~AR_RTSEnable;
            ds_ctl11 |= AR_CTSEnable;
        }
    } else {
        ds_ctl11 = (ds_ctl11 & ~(AR_RTSEnable | AR_CTSEnable));
    }

    mode = ath_hal_get_curmode(ah, ap->ah_curchan);
    calPkt = (ads->ds_ctl12 & AR_PAPRDChainMask)?1:0;

    if (ap->ah_config.ath_hal_desc_tpc ) {
        int16_t txpower;

        if (!calPkt) {
            /* Series 0 TxPower */
            txMode = ar9300GetTxMode(series[0].RateFlags);
            txpower = ar9300GetRateTxPower(ah, mode, series[0].RateIndex,
                                       series[0].ChSel, txMode);
        } else {
            txpower = AH9300(ah)->paprd_training_power;
        }
        ds_ctl11 &= ~AR_XmitPower0;
        ds_ctl11 |= set11nTxPower(0, AH_MIN(txpower, series[0].TxPowerCap));
    }
    ads->ds_ctl11 = ds_ctl11;

#ifdef  ATH_SUPPORT_TxBF
    if (MS(flags, HAL_TXDESC_TXBF_SOUND) == 0){ // not sounding frame
        u_int32_t steerflag;

        steerflag = set11nTxBfFlags(series, 0)             //set steered
                    |  set11nTxBfFlags(series, 1)
                    |  set11nTxBfFlags(series, 2)
                    |  set11nTxBfFlags(series, 3);

        /***
         * OSPREY chip 2.2 and before have a H/W bug when LDPC enable at
         * 2 stream data rate. LDPC should be disabe at this case.
         * If the problem is solved in a future revision of the chip,
         * it should be fixed again to enable LDPC.
         */
        /*
         * Modify algorthim for LDPC co-existence problem with TxBF at
         * two stream rate.  Check data rate for series 0 first.
         * If series 0 rate is two stream rate then disable LDPC.
         * If sereis 0 rate is not two stream rate then enable LDPC
         * and don't not steer BF for other series with 2 stream rate.
         */
        if (steerflag != 0) {   // should disable LDPC for txbf 2 stream rate
            if (AR_SREV_OSPREY_22(ah) && IS_3CHAIN_TX(ah)) {
                /* Only needed for OSPREY chip ver. 2.2 and before 2.2 ver.
                 * This  bugs is for 3 stream Tx only */
                if (ads->ds_ctl17 & AR_LDPC){ // LDPC on
                    if (not_two_stream_rate(series[0].Rate)){
                        steerflag = set11nTxBfFlags(series, 0);
                        if (not_two_stream_rate(series[1].Rate)){
                            steerflag |= set11nTxBfFlags(series, 1);
                        }
                        if (not_two_stream_rate(series[2].Rate)){
                            steerflag |= set11nTxBfFlags(series, 2);
                        }
                        if (not_two_stream_rate(series[3].Rate)){
                            steerflag |= set11nTxBfFlags(series, 3);
                        }
                    } else {
                        ads->ds_ctl17 &=  ~(AR_LDPC);
                    }
                }
            }
            /* set steer flag when not disable steering */
            if ((ap->ah_config.ath_hal_txbfCtl & TxBFCTL_Disable_Steering)==0) {
                ads->ds_ctl11 |= steerflag;
            }           
        }
    }
    {
        u_int32_t tmp;
        if (flags & HAL_TXDESC_CAL){ //change CSD while calibration
            tmp = OS_REG_READ(ah, AR_PHY_PERCHAIN_CSD);
            tmp |= SM(8,AR_PHY_PERCHAIN_CSD_chn1_2chains);
            tmp |= SM(4,AR_PHY_PERCHAIN_CSD_chn1_3chains);
            tmp |= SM(8,AR_PHY_PERCHAIN_CSD_chn2_3chains);
            OS_REG_WRITE(ah,AR_PHY_PERCHAIN_CSD,tmp);
        } else {
            tmp = OS_REG_READ(ah, AR_PHY_PERCHAIN_CSD);
            tmp |= SM(2,AR_PHY_PERCHAIN_CSD_chn1_2chains);
            tmp |= SM(2,AR_PHY_PERCHAIN_CSD_chn1_3chains);
            tmp |= SM(4,AR_PHY_PERCHAIN_CSD_chn2_3chains); // original settings
            OS_REG_WRITE(ah,AR_PHY_PERCHAIN_CSD,tmp);
        }
    }
#endif

    ads->ds_ctl13 = set11nTries(series, 0)
                             |  set11nTries(series, 1)
                             |  set11nTries(series, 2)
                             |  set11nTries(series, 3)
                             |  (durUpdateEn ? AR_DurUpdateEna : 0)
                             |  SM(0, AR_BurstDur);

    ads->ds_ctl14 = set11nRate(series, 0)
                             |  set11nRate(series, 1)
                             |  set11nRate(series, 2)
                             |  set11nRate(series, 3);

    ads->ds_ctl15 = set11nPktDurRTSCTS(series, 0)
                             |  set11nPktDurRTSCTS(series, 1);

    ads->ds_ctl16 = set11nPktDurRTSCTS(series, 2)
                             |  set11nPktDurRTSCTS(series, 3);

    ads->ds_ctl18 = set11nRateFlags(series, 0)
                             |  set11nRateFlags(series, 1)
                             |  set11nRateFlags(series, 2)
                             |  set11nRateFlags(series, 3)
                             | SM(rtsctsRate, AR_RTSCTSRate);
#ifdef ATH_SUPPORT_TxBF
    /*
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s:control flag %x, opt %x ",
        __func__, flags, MS(flags,HAL_TXDESC_TXBF_SOUND));
     */
    if ((MS(flags,HAL_TXDESC_TXBF_SOUND) == HAL_TXDESC_STAG_SOUND) ||
        (MS(flags,HAL_TXDESC_TXBF_SOUND) == HAL_TXDESC_SOUND)) {
        ar9300Set11nTxBFSounding(
            ah, ds, series,
            MS(flags, HAL_TXDESC_CEC), MS(flags, HAL_TXDESC_TXBF_SOUND));
    } else {
        //set not sounding for normal frame
        ads->ds_ctl19 = AR_Not_Sounding;
    }

#else
    /* set not sounding for normal frame */
    ads->ds_ctl19 = AR_Not_Sounding;
#endif

    if (ap->ah_config.ath_hal_desc_tpc ) {

        int16_t txpower;

        if (!calPkt) {
            /* Series 1 TxPower */
            txMode = ar9300GetTxMode(series[1].RateFlags);
            txpower = ar9300GetRateTxPower(
                ah, mode, series[1].RateIndex, series[1].ChSel, txMode);
        } else {
            txpower = AH9300(ah)->paprd_training_power;
        }
        ads->ds_ctl20 |=
            set11nTxPower(1, AH_MIN(txpower, series[1].TxPowerCap));
               

        /* Series 2 TxPower */
        if (!calPkt) {
            txMode = ar9300GetTxMode(series[2].RateFlags);
            txpower = ar9300GetRateTxPower(
                ah, mode, series[2].RateIndex, series[2].ChSel, txMode);
        } else {
            txpower = AH9300(ah)->paprd_training_power;
        }
        ads->ds_ctl21 |=
            set11nTxPower(2, AH_MIN(txpower, series[2].TxPowerCap));

        /* Series 3 TxPower */
        if (!calPkt) {
            txMode = ar9300GetTxMode(series[3].RateFlags);
            txpower = ar9300GetRateTxPower(
                ah, mode, series[3].RateIndex, series[3].ChSel, txMode);
        } else {
            txpower = AH9300(ah)->paprd_training_power;
        }
        ads->ds_ctl22 |=
            set11nTxPower(3, AH_MIN(txpower, series[3].TxPowerCap));
    }

    if (smart_antenna != 0xffffffff)
    {
        /* TX DESC dword 19 to 23 are used for smart antenna configuaration
         * ctl19 for rate series 0 ... ctrl22 for series 3
         * bits[2:0] used to configure smart anntenna
         */
        ant = (smart_antenna&0x000000ff);
        ads->ds_ctl19 |= ant;  /* rateseries 0 */

        ant = (smart_antenna&0x0000ff00)>>8;
        ads->ds_ctl20 |= ant ; /* rateseries 1 */

        ant = (smart_antenna&0x00ff0000)>>16;
        ads->ds_ctl21 |= ant ; /* rateseries 2 */

        ant = (smart_antenna&0xff000000)>>24;
        ads->ds_ctl22 |= ant ; /* rateseries 3 */
    }

#ifdef AH_NEED_DESC_SWAP
    last_ads->ds_ctl13 = __bswap32(ads->ds_ctl13);
    last_ads->ds_ctl14 = __bswap32(ads->ds_ctl14);
#else
    last_ads->ds_ctl13 = ads->ds_ctl13;
    last_ads->ds_ctl14 = ads->ds_ctl14;
#endif
}

void
ar9300Set11nAggrFirst(struct ath_hal *ah, void *ds, u_int aggrLen)
{
    struct ar9300_txc *ads = AR9300TXC(ds);

    ads->ds_ctl12 |= (AR_IsAggr | AR_MoreAggr);

    ads->ds_ctl17 &= ~AR_AggrLen;
    ads->ds_ctl17 |= SM(aggrLen, AR_AggrLen);
}

void
ar9300Set11nAggrMiddle(struct ath_hal *ah, void *ds, u_int numDelims)
{
    struct ar9300_txc *ads = AR9300TXC(ds);
    unsigned int ctl17;

    ads->ds_ctl12 |= (AR_IsAggr | AR_MoreAggr);

    /*
     * We use a stack variable to manipulate ctl6 to reduce uncached
     * read modify, modfiy, write.
     */
    ctl17 = ads->ds_ctl17;
    ctl17 &= ~AR_PadDelim;
    ctl17 |= SM(numDelims, AR_PadDelim);
    ads->ds_ctl17 = ctl17;
}

void
ar9300Set11nAggrLast(struct ath_hal *ah, void *ds)
{
    struct ar9300_txc *ads = AR9300TXC(ds);

    ads->ds_ctl12 |= AR_IsAggr;
    ads->ds_ctl12 &= ~AR_MoreAggr;
    ads->ds_ctl17 &= ~AR_PadDelim;
}

void
ar9300Clr11nAggr(struct ath_hal *ah, void *ds)
{
    struct ar9300_txc *ads = AR9300TXC(ds);

    ads->ds_ctl12 &= (~AR_IsAggr & ~AR_MoreAggr);
}

void
ar9300Set11nBurstDuration(struct ath_hal *ah, void *ds, u_int burstDuration)
{
    struct ar9300_txc *ads = AR9300TXC(ds);

    ads->ds_ctl13 &= ~AR_BurstDur;
    ads->ds_ctl13 |= SM(burstDuration, AR_BurstDur);
}

void
ar9300Set11nRifsBurstMiddle(struct ath_hal *ah, void *ds)
{
    struct ar9300_txc *ads = AR9300TXC(ds);

    ads->ds_ctl12 |= AR_MoreRifs | AR_NoAck;
}

void
ar9300Set11nRifsBurstLast(struct ath_hal *ah, void *ds)
{
    struct ar9300_txc *ads = AR9300TXC(ds);

    ads->ds_ctl12 &= (~AR_MoreAggr & ~AR_MoreRifs);
}

void
ar9300Clr11nRifsBurst(struct ath_hal *ah, void *ds)
{
    struct ar9300_txc *ads = AR9300TXC(ds);

    ads->ds_ctl12 &= (~AR_MoreRifs & ~AR_NoAck);
}

void
ar9300Set11nAggrRifsBurst(struct ath_hal *ah, void *ds)
{
    struct ar9300_txc *ads = AR9300TXC(ds);

    ads->ds_ctl12 |= AR_NoAck;
    ads->ds_ctl12 &= ~AR_MoreRifs;
}

void
ar9300Set11nVirtualMoreFrag(struct ath_hal *ah, void *ds,
                                                  u_int vmf)
{
    struct ar9300_txc *ads = AR9300TXC(ds);

    if (vmf) {
        ads->ds_ctl11 |=  AR_VirtMoreFrag;
    } else {
        ads->ds_ctl11 &= ~AR_VirtMoreFrag;
    }
}

void
ar9300GetDescInfo(struct ath_hal *ah, HAL_DESC_INFO *desc_info)
{
    desc_info->txctl_numwords = TXCTL_NUMWORDS(ah);
    desc_info->txctl_offset = TXCTL_OFFSET(ah);
    desc_info->txstatus_numwords = TXSTATUS_NUMWORDS(ah);
    desc_info->txstatus_offset = TXSTATUS_OFFSET(ah);

    desc_info->rxctl_numwords = RXCTL_NUMWORDS(ah);
    desc_info->rxctl_offset = RXCTL_OFFSET(ah);
    desc_info->rxstatus_numwords = RXSTATUS_NUMWORDS(ah);
    desc_info->rxstatus_offset = RXSTATUS_OFFSET(ah);
}

#endif /* AH_SUPPORT_AR9300 */
