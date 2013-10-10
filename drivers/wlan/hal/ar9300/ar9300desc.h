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


 /* Contains descriptor definitions for Osprey */


#ifndef _ATH_AR9300_DESC_H_
#define _ATH_AR9300_DESC_H_

#ifdef WIN32
#pragma pack(push, ar9300desc, 1)
#endif

#ifndef AH_DESC_NOPACK
#define AH_DESC_PACKED __packed
#else
#define AH_DESC_PACKED 
#endif


/* Osprey Status Descriptor. */
struct ar9300_txs {
    u_int32_t   ds_info;
    u_int32_t   status1;
    u_int32_t   status2;
    u_int32_t   status3;
    u_int32_t   status4;
    u_int32_t   status5;
    u_int32_t   status6;
    u_int32_t   status7;
    u_int32_t   status8;
};

struct ar9300_rxs {
    u_int32_t   ds_info;
    u_int32_t   status1;
    u_int32_t   status2;
    u_int32_t   status3;
    u_int32_t   status4;
    u_int32_t   status5;
    u_int32_t   status6;
    u_int32_t   status7;
    u_int32_t   status8;
    u_int32_t   status9;
    u_int32_t   status10;
    u_int32_t   status11;
};

/* Transmit Control Descriptor */
struct ar9300_txc {
    u_int32_t   ds_info;   /* descriptor information */
    u_int32_t   ds_link;   /* link pointer */
    u_int32_t   ds_data0;  /* data pointer to 1st buffer */
    u_int32_t   ds_ctl3;   /* DMA control 3  */
    u_int32_t   ds_data1;  /* data pointer to 2nd buffer */
    u_int32_t   ds_ctl5;   /* DMA control 5  */
    u_int32_t   ds_data2;  /* data pointer to 3rd buffer */
    u_int32_t   ds_ctl7;   /* DMA control 7  */
    u_int32_t   ds_data3;  /* data pointer to 4th buffer */
    u_int32_t   ds_ctl9;   /* DMA control 9  */
    u_int32_t   ds_ctl10;  /* DMA control 10 */
    u_int32_t   ds_ctl11;  /* DMA control 11 */
    u_int32_t   ds_ctl12;  /* DMA control 12 */
    u_int32_t   ds_ctl13;  /* DMA control 13 */
    u_int32_t   ds_ctl14;  /* DMA control 14 */
    u_int32_t   ds_ctl15;  /* DMA control 15 */
    u_int32_t   ds_ctl16;  /* DMA control 16 */
    u_int32_t   ds_ctl17;  /* DMA control 17 */
    u_int32_t   ds_ctl18;  /* DMA control 18 */
    u_int32_t   ds_ctl19;  /* DMA control 19 */
    u_int32_t   ds_ctl20;  /* DMA control 20 */
    u_int32_t   ds_ctl21;  /* DMA control 21 */
    u_int32_t   ds_ctl22;  /* DMA control 22 */
    u_int32_t   ds_pad[9]; /* pad to cache line (128 bytes/32 dwords) */
};


#define AR9300RXS(_rxs)        ((struct ar9300_rxs *)(_rxs))
#define AR9300TXS(_txs)        ((struct ar9300_txs *)(_txs))
#define AR9300TXC(_ds)         ((struct ar9300_txc *)(_ds))

#define AR9300TXC_CONST(_ds)   ((const struct ar9300_txc *)(_ds))


/* ds_info */
#define AR_DescLen          0x000000ff
#define AR_RxPriority       0x00000100
#define AR_TxQcuNum         0x00000f00
#define AR_TxQcuNum_S       8
#define AR_CtrlStat         0x00004000
#define AR_CtrlStat_S       14
#define AR_TxRxDesc         0x00008000
#define AR_TxRxDesc_S       15
#define AR_DescId           0xffff0000
#define AR_DescId_S         16

/***********
 * TX Desc *
 ***********/

/* ds_ctl3 */
/* ds_ctl5 */
/* ds_ctl7 */
/* ds_ctl9 */
#define AR_BufLen           0x0fff0000
#define AR_BufLen_S         16

/* ds_ctl10 */
#define AR_TxDescId         0xffff0000
#define AR_TxDescId_S       16
#define AR_TxPtrChkSum      0x0000ffff

/* ds_ctl11 */
#define AR_FrameLen         0x00000fff
#define AR_VirtMoreFrag     0x00001000
#define AR_TxCtlRsvd00      0x00002000
#define AR_LowRxChain       0x00004000
#define AR_TxClearRetry     0x00008000
#define AR_XmitPower0       0x003f0000
#define AR_XmitPower0_S     16
#define AR_RTSEnable        0x00400000
#define AR_VEOL             0x00800000
#define AR_ClrDestMask      0x01000000
#define AR_TxBf0            0x02000000
#define AR_TxBf1            0x04000000
#define AR_TxBf2            0x08000000
#define AR_TxBf3            0x10000000
#define	AR_TxBfSteered		0x1e000000			/* for TxBF*/ 
#define AR_TxIntrReq        0x20000000
#define AR_DestIdxValid     0x40000000
#define AR_CTSEnable        0x80000000

/* ds_ctl12 */
#define AR_TxCtlRsvd02      0x000001ff
#define AR_PAPRDChainMask   0x00000e00
#define AR_PAPRDChainMask_S 9
#define AR_TxMore           0x00001000
#define AR_DestIdx          0x000fe000
#define AR_DestIdx_S        13
#define AR_FrameType        0x00f00000
#define AR_FrameType_S      20
#define AR_NoAck            0x01000000
#define AR_InsertTS         0x02000000
#define AR_CorruptFCS       0x04000000
#define AR_ExtOnly          0x08000000
#define AR_ExtAndCtl        0x10000000
#define AR_MoreAggr         0x20000000
#define AR_IsAggr           0x40000000
#define AR_MoreRifs         0x80000000

/* ds_ctl13 */
#define AR_BurstDur         0x00007fff
#define AR_BurstDur_S       0
#define AR_DurUpdateEna     0x00008000
#define AR_XmitDataTries0   0x000f0000
#define AR_XmitDataTries0_S 16
#define AR_XmitDataTries1   0x00f00000
#define AR_XmitDataTries1_S 20
#define AR_XmitDataTries2   0x0f000000
#define AR_XmitDataTries2_S 24
#define AR_XmitDataTries3   0xf0000000
#define AR_XmitDataTries3_S 28

/* ds_ctl14 */
#define AR_XmitRate0        0x000000ff
#define AR_XmitRate0_S      0
#define AR_XmitRate1        0x0000ff00
#define AR_XmitRate1_S      8
#define AR_XmitRate2        0x00ff0000
#define AR_XmitRate2_S      16
#define AR_XmitRate3        0xff000000
#define AR_XmitRate3_S      24

/* ds_ctl15 */
#define AR_PacketDur0       0x00007fff
#define AR_PacketDur0_S     0
#define AR_RTSCTSQual0      0x00008000
#define AR_PacketDur1       0x7fff0000
#define AR_PacketDur1_S     16
#define AR_RTSCTSQual1      0x80000000

/* ds_ctl16 */
#define AR_PacketDur2       0x00007fff
#define AR_PacketDur2_S     0
#define AR_RTSCTSQual2      0x00008000
#define AR_PacketDur3       0x7fff0000
#define AR_PacketDur3_S     16
#define AR_RTSCTSQual3      0x80000000

/* ds_ctl17 */
#define AR_AggrLen          0x0000ffff
#define AR_AggrLen_S        0
#define AR_TxCtlRsvd60      0x00030000
#define AR_PadDelim         0x03fc0000
#define AR_PadDelim_S       18
#define AR_EncrType         0x1c000000
#define AR_EncrType_S       26
#define AR_TxDcApStaSel     0x40000000
#define AR_TxCtlRsvd61      0xc0000000
#define AR_Calibrating      0x40000000
#define AR_LDPC             0x80000000

/* ds_ctl18 */
#define AR_2040_0           0x00000001
#define AR_GI0              0x00000002
#define AR_ChainSel0        0x0000001c
#define AR_ChainSel0_S      2
#define AR_2040_1           0x00000020
#define AR_GI1              0x00000040
#define AR_ChainSel1        0x00000380
#define AR_ChainSel1_S      7
#define AR_2040_2           0x00000400
#define AR_GI2              0x00000800
#define AR_ChainSel2        0x00007000
#define AR_ChainSel2_S      12
#define AR_2040_3           0x00008000
#define AR_GI3              0x00010000
#define AR_ChainSel3        0x000e0000
#define AR_ChainSel3_S      17
#define AR_RTSCTSRate       0x0ff00000
#define AR_RTSCTSRate_S     20
#define AR_STBC0            0x10000000
#define AR_STBC1            0x20000000
#define AR_STBC2            0x40000000
#define AR_STBC3            0x80000000

/* ds_ctl19 */
#define AR_TxAnt0           0x00ffffff
#define AR_TxAntSel0        0x80000000
#define	AR_RTS_HTC_TRQ      0x10000000	/* bit 28 for rts_htc_TRQ*/ /*for TxBF*/
#define AR_Not_Sounding     0x20000000 
#define AR_NESS				0xc0000000 
#define AR_NESS_S			30

/* ds_ctl20 */
#define AR_TxAnt1           0x00ffffff
#define AR_XmitPower1       0x3f000000
#define AR_XmitPower1_S     24
#define AR_TxAntSel1        0x80000000
#define AR_NESS1			0xc0000000 
#define AR_NESS1_S			30

/* ds_ctl21 */
#define AR_TxAnt2           0x00ffffff
#define AR_XmitPower2       0x3f000000
#define AR_XmitPower2_S     24
#define AR_TxAntSel2        0x80000000
#define AR_NESS2			0xc0000000 
#define AR_NESS2_S			30

/* ds_ctl22 */
#define AR_TxAnt3           0x00ffffff
#define AR_XmitPower3       0x3f000000
#define AR_XmitPower3_S     24
#define AR_TxAntSel3        0x80000000
#define AR_NESS3			0xc0000000 
#define AR_NESS3_S			30

/*************
 * TX Status *
 *************/

/* ds_status1 */
#define AR_TxStatusRsvd     0x0000ffff

/* ds_status2 */
#define AR_TxRSSIAnt00      0x000000ff
#define AR_TxRSSIAnt00_S    0
#define AR_TxRSSIAnt01      0x0000ff00
#define AR_TxRSSIAnt01_S    8
#define AR_TxRSSIAnt02      0x00ff0000
#define AR_TxRSSIAnt02_S    16
#define AR_TxStatusRsvd00   0x3f000000
#define AR_TxBaStatus       0x40000000
#define AR_TxStatusRsvd01   0x80000000

/* ds_status3 */
#define AR_FrmXmitOK        0x00000001
#define AR_ExcessiveRetries 0x00000002
#define AR_FIFOUnderrun     0x00000004
#define AR_Filtered         0x00000008
#define AR_RTSFailCnt       0x000000f0
#define AR_RTSFailCnt_S     4
#define AR_DataFailCnt      0x00000f00
#define AR_DataFailCnt_S    8
#define AR_VirtRetryCnt     0x0000f000
#define AR_VirtRetryCnt_S   12
#define AR_TxDelimUnderrun  0x00010000
#define AR_TxDataUnderrun   0x00020000
#define AR_DescCfgErr       0x00040000
#define AR_TxTimerExpired   0x00080000
#define AR_TxStatusRsvd10   0xfff00000

/* ds_status7 */
#define AR_TxRSSIAnt10      0x000000ff
#define AR_TxRSSIAnt10_S    0
#define AR_TxRSSIAnt11      0x0000ff00
#define AR_TxRSSIAnt11_S    8
#define AR_TxRSSIAnt12      0x00ff0000
#define AR_TxRSSIAnt12_S    16
#define AR_TxRSSICombined   0xff000000
#define AR_TxRSSICombined_S 24

/* ds_status8 */
#define AR_TxDone           0x00000001
#define AR_SeqNum           0x00001ffe
#define AR_SeqNum_S         1
#define AR_TxStatusRsvd80   0x0001e000
#define AR_TxOpExceeded     0x00020000
#define AR_TxStatusRsvd81   0x001c0000
#define	AR_TXBFStatus		0x001c0000
#define	AR_TXBFStatus_S		18
#define AR_TxBF_BW_Mismatch 0x00040000
#define AR_TxBF_Stream_Miss 0x00080000
#define AR_FinalTxIdx       0x00600000
#define AR_FinalTxIdx_S     21
#define AR_TxBF_Dest_Miss   0x00800000
#define AR_TxBF_Expired     0x01000000
#define AR_PowerMgmt        0x02000000
#define AR_TxStatusRsvd83   0x0c000000
#define AR_TxTid            0xf0000000
#define AR_TxTid_S          28


/*************
 * Rx Status *
 *************/

/* ds_status1 */
#define AR_RxRSSIAnt00      0x000000ff
#define AR_RxRSSIAnt00_S    0
#define AR_RxRSSIAnt01      0x0000ff00
#define AR_RxRSSIAnt01_S    8
#define AR_RxRSSIAnt02      0x00ff0000
#define AR_RxRSSIAnt02_S    16
#define AR_RxRate           0xff000000
#define AR_RxRate_S         24

/* ds_status2 */
#define AR_DataLen          0x00000fff
#define AR_RxMore           0x00001000
#define AR_NumDelim         0x003fc000
#define AR_NumDelim_S       14
#define AR_HwUploadData     0x00400000
#define AR_HwUploadData_S   22
#define AR_RxStatusRsvd10   0xff800000


/* ds_status4 */
#define AR_GI               0x00000001
#define AR_2040             0x00000002
#define AR_Parallel40       0x00000004
#define AR_Parallel40_S     2
#define AR_RxStbc           0x00000008
#define AR_RxNotSounding    0x00000010
#define AR_RxNess           0x00000060
#define AR_RxNess_S         5
#define AR_HwUploadDataValid    0x00000080
#define AR_HwUploadDataValid_S  7    
#define AR_RxAntenna	    0xffffff00
#define AR_RxAntenna_S	    8

/* ds_status5 */
#define AR_RxRSSIAnt10            0x000000ff
#define AR_RxRSSIAnt10_S          0
#define AR_RxRSSIAnt11            0x0000ff00
#define AR_RxRSSIAnt11_S          8
#define AR_RxRSSIAnt12            0x00ff0000
#define AR_RxRSSIAnt12_S          16
#define AR_RxRSSICombined         0xff000000
#define AR_RxRSSICombined_S       24

/* ds_status6 */
#define AR_RxEVM0           status6

/* ds_status7 */
#define AR_RxEVM1           status7

/* ds_status8 */
#define AR_RxEVM2           status8

/* ds_status9 */
#define AR_RxEVM3           status9

/* ds_status11 */
#define AR_RxDone           0x00000001
#define AR_RxFrameOK        0x00000002
#define AR_CRCErr           0x00000004
#define AR_DecryptCRCErr    0x00000008
#define AR_PHYErr           0x00000010
#define AR_MichaelErr       0x00000020
#define AR_PreDelimCRCErr   0x00000040
#define AR_ApsdTrig         0x00000080
#define AR_RxKeyIdxValid    0x00000100
#define AR_KeyIdx           0x0000fe00
#define AR_KeyIdx_S         9
#define AR_PHYErrCode       0x0000ff00
#define AR_PHYErrCode_S     8
#define AR_RxMoreAggr       0x00010000
#define AR_RxAggr           0x00020000
#define AR_PostDelimCRCErr  0x00040000
#define AR_RxStatusRsvd71   0x01f80000
#define AR_HwUploadDataType 0x06000000
#define AR_HwUploadDataType_S   25
#define AR_HiRxChain        0x10000000
#define AR_RxFirstAggr      0x20000000
#define AR_DecryptBusyErr   0x40000000
#define AR_KeyMiss          0x80000000

#define TXCTL_OFFSET(ah)      11
#define TXCTL_NUMWORDS(ah)    12
#define TXSTATUS_OFFSET(ah)   2
#define TXSTATUS_NUMWORDS(ah) 7

#define RXCTL_OFFSET(ah)      0
#define RXCTL_NUMWORDS(ah)    0
#define RXSTATUS_OFFSET(ah)   1
#define RXSTATUS_NUMWORDS(ah) 11


#define TXC_INFO(_qcu) (ATHEROS_VENDOR_ID << AR_DescId_S) \
                        | (1 << AR_TxRxDesc_S) \
                        | (1 << AR_CtrlStat_S) \
                        | (_qcu << AR_TxQcuNum_S) \
                        | (0x17)

#define VALID_KEY_TYPES \
        ((1 << HAL_KEY_TYPE_CLEAR) | (1 << HAL_KEY_TYPE_WEP)|\
         (1 << HAL_KEY_TYPE_AES)   | (1 << HAL_KEY_TYPE_TKIP))
#define isValidKeyType(_t)      ((1 << (_t)) & VALID_KEY_TYPES)

#define set11nTries(_series, _index) \
        (SM((_series)[_index].Tries, AR_XmitDataTries##_index))

#define set11nRate(_series, _index) \
        (SM((_series)[_index].Rate, AR_XmitRate##_index))

#define set11nPktDurRTSCTS(_series, _index) \
        (SM((_series)[_index].PktDuration, AR_PacketDur##_index) |\
         ((_series)[_index].RateFlags & HAL_RATESERIES_RTS_CTS   ?\
         AR_RTSCTSQual##_index : 0))
        
#define not_two_stream_rate(_rate) (((_rate) >0x8f) || ((_rate)<0x88))

#define set11nTxBFLDPC( _series) \
        ((( not_two_stream_rate((_series)[0].Rate) && (not_two_stream_rate((_series)[1].Rate)|| \
        (!(_series)[1].Tries)) && (not_two_stream_rate((_series)[2].Rate)||(!(_series)[2].Tries)) \
         && (not_two_stream_rate((_series)[3].Rate)||(!(_series)[3].Tries)))) \
        ? AR_LDPC : 0)

#define set11nRateFlags(_series, _index) \
        ((_series)[_index].RateFlags & HAL_RATESERIES_2040 ? AR_2040_##_index : 0) \
        |((_series)[_index].RateFlags & HAL_RATESERIES_HALFGI ? AR_GI##_index : 0) \
        |((_series)[_index].RateFlags & HAL_RATESERIES_STBC ? AR_STBC##_index : 0) \
        |SM((_series)[_index].ChSel, AR_ChainSel##_index)

#define set11nTxPower(_index, _txpower) \
        SM(_txpower, AR_XmitPower##_index)

#ifdef ATH_SUPPORT_TxBF
#define set11nTxBfFlags(_series, _index) \
        ((_series)[_index].RateFlags & HAL_RATESERIES_TXBF ? AR_TxBf##_index : 0)
#endif

#define IS_3CHAIN_TX(_ah) (AH9300(_ah)->ah_txchainmask == 7)

/*
 * Descriptor Access Functions
 */
/* XXX valid Tx rates will change for 3 stream support */
#define VALID_PKT_TYPES \
        ((1<<HAL_PKT_TYPE_NORMAL)|(1<<HAL_PKT_TYPE_ATIM)|\
         (1<<HAL_PKT_TYPE_PSPOLL)|(1<<HAL_PKT_TYPE_PROBE_RESP)|\
         (1<<HAL_PKT_TYPE_BEACON))
#define isValidPktType(_t)      ((1<<(_t)) & VALID_PKT_TYPES)
#define VALID_TX_RATES \
        ((1<<0x0b)|(1<<0x0f)|(1<<0x0a)|(1<<0x0e)|(1<<0x09)|(1<<0x0d)|\
         (1<<0x08)|(1<<0x0c)|(1<<0x1b)|(1<<0x1a)|(1<<0x1e)|(1<<0x19)|\
         (1<<0x1d)|(1<<0x18)|(1<<0x1c))
#define isValidTxRate(_r)       ((1<<(_r)) & VALID_TX_RATES)

        /* TX common functions */

extern  HAL_BOOL ar9300UpdateTxTrigLevel(struct ath_hal *,
        HAL_BOOL IncTrigLevel);
extern  u_int16_t ar9300GetTxTrigLevel(struct ath_hal *);
extern  HAL_BOOL ar9300SetTxQueueProps(struct ath_hal *ah, int q,
        const HAL_TXQ_INFO *qInfo);
extern  HAL_BOOL ar9300GetTxQueueProps(struct ath_hal *ah, int q,
        HAL_TXQ_INFO *qInfo);
extern  int ar9300SetupTxQueue(struct ath_hal *ah, HAL_TX_QUEUE type,
        const HAL_TXQ_INFO *qInfo);
extern  HAL_BOOL ar9300ReleaseTxQueue(struct ath_hal *ah, u_int q);
extern  HAL_BOOL ar9300ResetTxQueue(struct ath_hal *ah, u_int q);
extern  u_int32_t ar9300GetTxDP(struct ath_hal *ah, u_int q);
extern  HAL_BOOL ar9300SetTxDP(struct ath_hal *ah, u_int q, u_int32_t txdp);
extern  HAL_BOOL ar9300StartTxDma(struct ath_hal *ah, u_int q);
extern  u_int32_t ar9300NumTxPending(struct ath_hal *ah, u_int q);
extern  HAL_BOOL ar9300StopTxDma(struct ath_hal *ah, u_int q, u_int timeout);
extern  HAL_BOOL ar9300AbortTxDma(struct ath_hal *ah);
extern  void ar9300GetTxIntrQueue(struct ath_hal *ah, u_int32_t *);
extern  HAL_BOOL ar9300SetGlobalTxTimeout(struct ath_hal *, u_int);
extern  u_int ar9300GetGlobalTxTimeout(struct ath_hal *);

extern  void ar9300TxReqIntrDesc(struct ath_hal *ah, void *ds);
extern  HAL_BOOL ar9300FillTxDesc(struct ath_hal *ah, void *ds, dma_addr_t *bufAddr,
        u_int32_t *segLen, u_int descId, u_int qcu, HAL_KEY_TYPE keyType, HAL_BOOL firstSeg,
        HAL_BOOL lastSeg, const void *ds0);
extern  void ar9300SetDescLink(struct ath_hal *, void *ds, u_int32_t link);
#ifdef _WIN64
extern  void ar9300GetDescLinkPtr(struct ath_hal *, void *ds, u_int32_t __unaligned **link);
#else
extern  void ar9300GetDescLinkPtr(struct ath_hal *, void *ds, u_int32_t **link);
#endif
extern  void ar9300ClearTxDescStatus(struct ath_hal *ah, void *ds);
#ifdef ATH_SWRETRY
extern void ar9300ClearDestMask(struct ath_hal *ah, void *ds);
#endif
extern HAL_BOOL ar9300FillKeyTxDesc(struct ath_hal *ah, void *ds,
       HAL_KEY_TYPE keyType);
extern  HAL_STATUS ar9300ProcTxDesc(struct ath_hal *ah, void *);
extern  void ar9300GetRawTxDesc(struct ath_hal *ah, u_int32_t *);
extern  void ar9300GetTxRateCode(struct ath_hal *ah, void *, struct ath_tx_status *);
extern  u_int32_t ar9300CalcTxAirtime(struct ath_hal *ah, void *, struct ath_tx_status *, 
        HAL_BOOL comp_wastedt, u_int8_t nbad, u_int8_t nframes);
extern  void ar9300SetupTxStatusRing(struct ath_hal *ah, void *, u_int32_t , u_int16_t);
extern void ar9300SetPAPRDTxDesc(struct ath_hal *ah, void *ds, int chainNum);
HAL_STATUS ar9300IsTxDone(struct ath_hal *ah);
extern void ar9300Set11nTxDesc(struct ath_hal *ah, void *ds,
       u_int pktLen, HAL_PKT_TYPE type, u_int txPower,
       u_int keyIx, HAL_KEY_TYPE keyType, u_int flags);

/* for TxBF*/       
#ifdef ATH_SUPPORT_TxBF 
extern void ar9300Set11nTxBFCal(struct ath_hal *ah, 
				void *ds,u_int8_t CalPos,u_int8_t codeRate,u_int8_t CEC,u_int8_t opt);
#endif
/* for TxBF*/
         
extern void ar9300Set11nRateScenario(struct ath_hal *ah, void *ds,
        void *lastds, u_int durUpdateEn, u_int rtsctsRate, u_int rtsctsDuration, HAL_11N_RATE_SERIES series[],
       u_int nseries, u_int flags, u_int32_t smartAntenna);
extern void ar9300Set11nAggrFirst(struct ath_hal *ah, void *ds,
       u_int aggrLen);
extern void ar9300Set11nAggrMiddle(struct ath_hal *ah, void *ds,
       u_int numDelims);
extern void ar9300Set11nAggrLast(struct ath_hal *ah, void *ds);
extern void ar9300Clr11nAggr(struct ath_hal *ah, void *ds);
extern void ar9300Set11nBurstDuration(struct ath_hal *ah, void *ds,
       u_int burstDuration);
extern void ar9300Set11nRifsBurstMiddle(struct ath_hal *ah, void *ds);
extern void ar9300Set11nRifsBurstLast(struct ath_hal *ah, void *ds);
extern void ar9300Clr11nRifsBurst(struct ath_hal *ah, void *ds);
extern void ar9300Set11nAggrRifsBurst(struct ath_hal *ah, void *ds);
extern void ar9300Set11nVirtualMoreFrag(struct ath_hal *ah, void *ds,
       u_int vmf);
#ifdef AH_PRIVATE_DIAG
extern void ar9300_ContTxMode(struct ath_hal *ah, void *ds, int mode);
#endif

	/* RX common functions */

extern  u_int32_t ar9300GetRxDP(struct ath_hal *ath, HAL_RX_QUEUE qtype);
extern  void ar9300SetRxDP(struct ath_hal *ah, u_int32_t rxdp, HAL_RX_QUEUE qtype);
extern  void ar9300EnableReceive(struct ath_hal *ah);
extern  HAL_BOOL ar9300StopDmaReceive(struct ath_hal *ah, u_int timeout);
extern  void ar9300StartPcuReceive(struct ath_hal *ah, HAL_BOOL is_scanning);
extern  void ar9300StopPcuReceive(struct ath_hal *ah);
extern  void ar9300AbortPcuReceive(struct ath_hal *ah);
extern  void ar9300SetMulticastFilter(struct ath_hal *ah,
        u_int32_t filter0, u_int32_t filter1);
extern  HAL_BOOL ar9300ClrMulticastFilterIndex(struct ath_hal *, u_int32_t ix);
extern  HAL_BOOL ar9300SetMulticastFilterIndex(struct ath_hal *, u_int32_t ix);
extern  u_int32_t ar9300GetRxFilter(struct ath_hal *ah);
extern  void ar9300SetRxFilter(struct ath_hal *ah, u_int32_t bits);
extern  HAL_BOOL ar9300SetRxSelEvm(struct ath_hal *ah, HAL_BOOL, HAL_BOOL);
extern	HAL_BOOL ar9300SetRxAbort(struct ath_hal *ah, HAL_BOOL);

extern  HAL_STATUS ar9300ProcRxDesc(struct ath_hal *ah,
        struct ath_desc *, u_int32_t, struct ath_desc *, u_int64_t);
extern  HAL_STATUS ar9300GetRxKeyIdx(struct ath_hal *ah,
        struct ath_desc *, u_int8_t *, u_int8_t *);
extern  HAL_STATUS ar9300ProcRxDescFast(struct ath_hal *ah, struct ath_desc *,
        u_int32_t, struct ath_desc *, struct ath_rx_status *, void *);

extern  void ar9300PromiscMode(struct ath_hal *ah, HAL_BOOL enable);
extern  void ar9300ReadPktlogReg(struct ath_hal *ah, u_int32_t *, u_int32_t *, u_int32_t *, u_int32_t *);
extern  void ar9300WritePktlogReg(struct ath_hal *ah, HAL_BOOL , u_int32_t , u_int32_t , u_int32_t , u_int32_t );

#ifdef WIN32
#pragma pack(pop, ar9300desc)
#endif

#endif









