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

#ifndef _ATH_AH_INTERAL_H_
#define _ATH_AH_INTERAL_H_

#include "asf_amem.h"   /* amalloc / afree */
#include "adf_os_mem.h" /* adf_os_mem_zero_outline */
#include <stdarg.h>

/*
 * Atheros Device Hardware Access Layer (HAL).
 *
 * Internal definitions.
 */
#define AH_NULL 0
#define AH_MIN(a,b) ((a)<(b)?(a):(b))
#define AH_MAX(a,b) ((a)>(b)?(a):(b))

#ifndef NBBY
#define NBBY    8           /* number of bits/byte */
#endif

#ifndef roundup
#define roundup(x, y)   ((((x)+((y)-1))/(y))*(y))  /* to any y */
#endif

#ifndef howmany
#define howmany(x, y)   (((x)+((y)-1))/(y))
#endif

#ifndef offsetof
#define offsetof(type, field)   ((size_t)(&((type *)0)->field))
#endif

/*
 * Remove const in a way that keeps the compiler happy.
 * This works for gcc but may require other magic for
 * other compilers (not sure where this should reside).
 * Note that uintptr_t is C99.
 */
#ifndef __DECONST
#define __DECONST(type, var)    ((type)(uintptr_t)(const void *)(var))
#endif

/* 1024 microseconds per time-unit */
#define TU_TO_USEC(_tu)  ((_tu) << 10)
/* 128 microseconds per eighth of a time-unit */
#define USEC_TO_ONE_EIGHTH_TU(_usec)  ((_usec) >> 7)
#define ONE_EIGHTH_TU_TO_USEC(tu8) ((tu8) << 7)
/* convert eighths of TUs to TUs (round down) */
#define ONE_EIGHTH_TU_TO_TU(tu8) ((tu8) >> 3)
#define TU_TO_ONE_EIGHTH_TU(tu) ((tu) << 3)

typedef enum {
    START_ADHOC_NO_11A,     /* don't use 802.11a channel */
    START_ADHOC_PER_11D,    /* 802.11a + 802.11d ad-hoc */
    START_ADHOC_IN_11A,     /* 802.11a ad-hoc */
    START_ADHOC_IN_11B,     /* 802.11b ad-hoc */
} START_ADHOC_OPTION;

#ifdef WIN32
#pragma pack(push, ah_internal, 1)
#endif

typedef struct {
    u_int8_t    id;         /* element ID */
    u_int8_t    len;        /* total length in bytes */
    u_int8_t    cc[3];      /* ISO CC+(I)ndoor/(O)utdoor */
    struct {
        u_int8_t schan;     /* starting channel */
        u_int8_t nchan;     /* number channels */
        u_int8_t maxtxpwr;  
    } band[4];              /* up to 4 sub bands */
}  __packed COUNTRY_INFO_LIST;

#ifdef WIN32
#pragma pack(pop, ah_internal)
#endif
  
typedef struct {
    u_int32_t   start;      /* first register */
    u_int32_t   end;        /* ending register or zero */
} HAL_REGRANGE;

/*
 * Transmit power scale factor.
 *
 * NB: This is not public because we want to discourage the use of
 *     scaling; folks should use the tx power limit interface.
 */
typedef enum {
    HAL_TP_SCALE_MAX    = 0,        /* no scaling (default) */
    HAL_TP_SCALE_50     = 1,        /* 50% of max (-3 dBm) */
    HAL_TP_SCALE_25     = 2,        /* 25% of max (-6 dBm) */
    HAL_TP_SCALE_12     = 3,        /* 12% of max (-9 dBm) */
    HAL_TP_SCALE_MIN    = 4,        /* min, but still on */
} HAL_TP_SCALE;


/* Regulatory domains */
typedef enum {       
        HAL_REGDMN_FCC     = 0x10,
        HAL_REGDMN_MKK     = 0x40,
        HAL_REGDMN_ETSI    = 0x30,
}HAL_REG_DMNS;
#define HAL_REG_DMN_MASK    0xF0
#define isRegDmnFCC(regDmn) (((regDmn & HAL_REG_DMN_MASK) == HAL_REGDMN_FCC)?1:0)
#define isRegDmnETSI(regDmn)(((regDmn & HAL_REG_DMN_MASK) == HAL_REGDMN_ETSI)?1:0)
#define isRegDmnMKK(regDmn) (((regDmn & HAL_REG_DMN_MASK) == HAL_REGDMN_MKK)?1:0)

 /* 
  * Enums of vendors used to modify reg domain flags if necessary
  */
 typedef enum {
    HAL_VENDOR_APPLE    = 1,
 } HAL_VENDORS;
 
#define HAL_NF_CAL_HIST_LEN_FULL  5
#define HAL_NF_CAL_HIST_LEN_SMALL 1
#define NUM_NF_READINGS           6   /* 3 chains * (ctl + ext) */
#define NF_LOAD_DELAY             1000 

typedef struct {
    u_int8_t    currIndex;
    int8_t      invalidNFcount; /* TO DO: REMOVE THIS! */
    int16_t     privNF[NUM_NF_READINGS];
} HAL_NFCAL_BASE;

typedef struct {
    HAL_NFCAL_BASE base;
    int16_t     nfCalBuffer[HAL_NF_CAL_HIST_LEN_SMALL][NUM_NF_READINGS];
} HAL_NFCAL_HIST_SMALL;

typedef struct {
    HAL_NFCAL_BASE base;
    int16_t     nfCalBuffer[HAL_NF_CAL_HIST_LEN_FULL][NUM_NF_READINGS];
} HAL_NFCAL_HIST_FULL;

#ifdef ATH_NF_PER_CHAN
typedef HAL_NFCAL_HIST_FULL HAL_CHAN_NFCAL_HIST;
#else
typedef HAL_NFCAL_HIST_SMALL HAL_CHAN_NFCAL_HIST;
#endif


/*
 * Internal form of a HAL_CHANNEL.  Note that the structure
 * must be defined such that you can cast references to a
 * HAL_CHANNEL so don't shuffle the first two members.
 */
typedef struct {
    u_int16_t channel;    /* NB: must be first for casting */
    u_int32_t   channelFlags;
    u_int8_t    privFlags;
    int8_t      maxRegTxPower;
    int8_t      maxTxPower;
    int8_t      minTxPower; /* as above... */
    u_int8_t  regClassId; /* Regulatory class id */
    u_int8_t    paprdDone:1,               /* 1: PAPRD DONE, 0: PAPRD Cal not done */
                paprdTableWriteDone:1;     /* 1: DONE, 0: Cal data write not done */

    HAL_BOOL    bssSendHere;
    u_int8_t  gainI;
    HAL_BOOL    iqCalValid;
    int32_t     CalValid;
    HAL_BOOL        oneTimeCalsDone;
    int8_t      iCoff;
    int8_t      qCoff;
    int16_t     rawNoiseFloor;
    int16_t   noiseFloorAdjust;
    int8_t    antennaMax;
    u_int32_t regDmnFlags;    /* Flags for channel use in reg */
    u_int32_t   conformanceTestLimit;   /* conformance test limit from reg domain */
    u_int64_t ah_tsf_last;            /* tsf @ which time accured is computed */
    u_int64_t ah_channel_time;        /* time on the channel  */
    u_int16_t mainSpur;       /* cached spur value for this cahnnel */
    u_int64_t   dfsTsf;
    /*
     * Each channels has a NF history buffer.
     * If ATH_NF_PER_CHAN is defined, this history buffer is full-sized
     * (HAL_NF_CAL_HIST_MAX elements).  Else, this history buffer only
     * stores a single element.
     */
    HAL_CHAN_NFCAL_HIST  nfCalHist;
} HAL_CHANNEL_INTERNAL;

typedef struct {
        u_int           halChanSpreadSupport            : 1,
                        halSleepAfterBeaconBroken       : 1,
                        halCompressSupport              : 1,
                        halBurstSupport                 : 1,
                        halFastFramesSupport            : 1,
                        halChapTuningSupport            : 1,
                        halTurboGSupport                : 1,
                        halTurboPrimeSupport            : 1,
                        halXrSupport                    : 1,
                        halMicAesCcmSupport             : 1,
                        halMicCkipSupport               : 1,
                        halMicTkipSupport               : 1,
                        halCipherAesCcmSupport          : 1,
                        halCipherCkipSupport            : 1,
                        halCipherTkipSupport            : 1,
                        halPSPollBroken                 : 1,
                        halVEOLSupport                  : 1,
                        halBssIdMaskSupport             : 1,
                        halMcastKeySrchSupport          : 1,
                        halTsfAddSupport                : 1,
                        halChanHalfRate                 : 1,
                        halChanQuarterRate              : 1,
                        halHTSupport                    : 1,
                        halHT20SGISupport               : 1,
                        halRxStbcSupport                : 1,
                        halTxStbcSupport                : 1,
                        halGTTSupport                   : 1,
                        halFastCCSupport                : 1,
                        halRfSilentSupport              : 1,
                        halWowSupport                   : 1,
                        halCSTSupport                   : 1,
                        halRifsRxSupport                : 1,
                        halRifsTxSupport                : 1,
                        halforcePpmSupport              : 1,
                        halExtChanDfsSupport            : 1,
                        halUseCombinedRadarRssi         : 1,
                        halEnhancedPmSupport            : 1,
                        halAutoSleepSupport             : 1,
                        halMbssidAggrSupport            : 1,
                        hal4kbSplitTransSupport         : 1,
                        halWowMatchPatternExact         : 1,
                        halBtCoexSupport                : 1,
                        halGenTimerSupport              : 1,
                        halWowPatternMatchDword         : 1,
                        halWpsPushButton                : 1,
                        halWepTkipAggrSupport           : 1,
#if ATH_SUPPORT_SPECTRAL
                        halSpectralScan                 : 1,
#endif
#if ATH_SUPPORT_RAW_ADC_CAPTURE
                        halRawADCCapture                : 1,
#endif
                        halEnhancedDmaSupport           : 1,
#ifdef ATH_SUPPORT_DFS
                        hal_enhanced_dfs_support        : 1,
#endif
                        halIsrRacSupport                : 1,
                        halCfendFixSupport              : 1,
                        halBtCoexASPMWAR                : 1,
#ifdef ATH_SUPPORT_TxBF
                        halTxBFSupport                  : 1,
#endif
                        halAggrExtraDelimWar            : 1,
                        halRxTxAbortSupport             : 1,
                        halPaprdEnabled                 : 1,
                        halHwUapsdTrig                  : 1,
                        halAntDivCombSupport            : 1,
                        hal49Ghz                        : 1,
                        halLdpcSupport                  : 1,
                        halEnableApm                    : 1,
                        halPcieLcrExtsyncEn             : 1;
        u_int32_t       halWirelessModes;
        u_int16_t       halTotalQueues;
        u_int16_t       halKeyCacheSize;
        u_int16_t       halLow5GhzChan, halHigh5GhzChan;
        u_int16_t       halLow2GhzChan, halHigh2GhzChan;
        u_int16_t       halNumMRRetries;
        u_int16_t       halRtsAggrLimit;
        u_int16_t       halTxTrigLevelMax;
        u_int16_t       halRegCap;
        u_int16_t       halWepTkipAggrNumTxDelim;
        u_int16_t       halWepTkipAggrNumRxDelim;
        u_int8_t        halWepTkipMaxHtRate;
        u_int8_t        halTxChainMask;
        u_int8_t        halRxChainMask;
        u_int8_t        halNumGpioPins;
        u_int8_t        halNumAntCfg2GHz;
        u_int8_t        halNumAntCfg5GHz;
        u_int8_t        halNumTxMaps;
        u_int8_t        halTxDescLen;
        u_int8_t        halTxStatusLen;
        u_int8_t        halRxStatusLen;
        u_int8_t        halRxHPdepth;
        u_int8_t        halRxLPdepth;
        u_int8_t        halintr_mitigation;
        u_int32_t       halMfpSupport;
        u_int8_t        halProxySTASupport;
        u_int8_t        halRxDescTimestampBits;
        u_int16_t       halAniPollInterval; /* ANI poll interval in ms */
        u_int16_t       halChannelSwitchTimeUsec;
        u_int8_t        halPcieLcrOffset;
#if ATH_SUPPORT_WAPI
        u_int8_t        hal_wapi_max_tx_chains;
        u_int8_t        hal_wapi_max_rx_chains;
#endif
} HAL_CAPABILITIES;

/* Serialize Register Access Mode */
typedef enum {
    SER_REG_MODE_OFF    = 0,
    SER_REG_MODE_ON     = 1,
    SER_REG_MODE_AUTO   = 2,
} SER_REG_MODE;

/*****
** HAL Configuration
**
** This type contains all of the "factory default" configuration
** items that may be changed during initialization or operation.
** These were formerly global variables which are now PER INSTANCE
** values that are used to control the operation of the specific
** HAL instance
*/

typedef struct {
    int         ath_hal_dma_beacon_response_time;
    int         ath_hal_sw_beacon_response_time;
    int         ath_hal_additional_swba_backoff;
    int         ath_hal_6mb_ack;
    int         ath_hal_cwmIgnoreExtCCA;
#ifdef ATH_FORCE_BIAS
    /* workaround Fowl bug for orientation sensitivity */
    int         ath_hal_forceBias;
    int         ath_hal_forceBiasAuto ;
#endif
    u_int8_t    ath_hal_soft_eeprom;
    u_int8_t    ath_hal_emiNoRtcReset; //war for EV81239
    u_int8_t    ath_hal_pciePowerSaveEnable;
    u_int8_t    ath_hal_pcieSerDesWrite;
    u_int8_t    ath_hal_pcieL1SKPEnable;
    u_int8_t    ath_hal_pcieClockReq;
#define         AR_PCIE_PLL_PWRSAVE_CONTROL               (1<<0)
#define         AR_PCIE_PLL_PWRSAVE_ON_D3                 (1<<1)
#define         AR_PCIE_PLL_PWRSAVE_ON_D0                 (1<<2) 

    u_int8_t    ath_hal_desc_tpc;
    u_int32_t   ath_hal_pcieWaen; /****************/
    u_int32_t   ath_hal_pcieDetach; /* clear bit 14 of AR_WA when detach */
    int         ath_hal_pciePowerReset;
    u_int8_t    ath_hal_pcieRestore;
    u_int8_t    ath_hal_pllPwrSave;
    u_int8_t	ath_hal_analogShiftRegDelay;
    u_int8_t    ath_hal_htEnable;
    u_int8_t    ath_hal_redchn_maxpwr;
#ifdef ATH_SUPERG_DYNTURBO
    int         ath_hal_disableTurboG;
#endif
    u_int32_t   ath_hal_ofdmTrigLow;
    u_int32_t   ath_hal_ofdmTrigHigh;
    u_int32_t   ath_hal_cckTrigHigh;
    u_int32_t   ath_hal_cckTrigLow;
    u_int32_t   ath_hal_enableANI;
    u_int8_t    ath_hal_noiseImmunityLvl;
    u_int32_t   ath_hal_ofdmWeakSigDet;
    u_int32_t   ath_hal_cckWeakSigThr;
    u_int8_t    ath_hal_spurImmunityLvl;
    u_int8_t    ath_hal_firStepLvl;
    int8_t      ath_hal_rssiThrHigh;
    int8_t      ath_hal_rssiThrLow;
    u_int16_t   ath_hal_diversityControl;
    u_int16_t   ath_hal_antennaSwitchSwap;
    int         ath_hal_rifs_enable;
    int         ath_hal_serializeRegMode; /* serialize all reg accesses mode */
    int         ath_hal_intrMitigationRx;
    int         ath_hal_intrMitigationTx;
    int         ath_hal_fastClockEnable;

#ifdef AH_DEBUG
    int         ath_hal_debug;
#endif
#define         SPUR_DISABLE        0
#define         SPUR_ENABLE_IOCTL   1
#define         SPUR_ENABLE_EEPROM  2
#define         AR_EEPROM_MODAL_SPURS   5
#define         AR_SPUR_5413_1      1640    /* Freq 2464 */
#define         AR_SPUR_5413_2      1200    /* Freq 2420 */
#define         AR_NO_SPUR      0x8000
#define         AR_BASE_FREQ_2GHZ   2300
#define         AR_BASE_FREQ_5GHZ   4900
#define         AR_SPUR_FEEQ_BOUND_HT40  19
#define         AR_SPUR_FEEQ_BOUND_HT20  10

    int         ath_hal_spurMode; 
    u_int16_t   ath_hal_spurChans[AR_EEPROM_MODAL_SPURS][2];
    u_int8_t    ath_hal_defaultAntCfg;
    u_int8_t    ath_hal_enableMSI;
    u_int32_t   ath_hal_mfpSupport;
    u_int32_t   ath_hal_fullResetWarEnable;
    u_int8_t    ath_hal_legacyMinTxPowerLimit;
    u_int8_t    ath_hal_skipEepromRead;
    u_int8_t    ath_hal_txTrigLevelMax;
    u_int8_t    ath_hal_disPeriodicPACal;
    u_int32_t   ath_hal_keepAliveTimeout;  /* in ms */
#ifdef ATH_SUPPORT_TxBF
    u_int8_t    ath_hal_cvtimeout;
    u_int16_t   ath_hal_txbfCtl;        // TxBf control flag
#define TxBFCtl_ImBF                        0x01
#define TxBFCtl_ImBF_S                      0
#define TxBFCtl_Non_ExBF                    0x02
#define TxBFCtl_Non_ExBF_S                  1
#define TxBFCtl_Comp_ExBF                   0x04
#define TxBFCtl_Comp_ExBF_S                 2
#define TxBFCtl_ImBF_FB                     0x08
#define TxBFCtl_ImBF_FB_S                   3
#define TxBFCtl_Non_ExBF_Immediately_Rpt    0x10
#define TxBFCtl_Non_ExBF_Immediately_Rpt_S  4
#define TxBFCtl_Comp_ExBF_Immediately_Rpt   0x20
#define TxBFCtl_Comp_ExBF_Immediately_Rpt_S 5

#define TxBFCtl_Non_ExBF_delay_Rpt          0x40
#define TxBFCtl_Non_ExBF_delay_Rpt_S        6
#define TxBFCtl_Comp_ExBF_delay_Rpt         0x80
#define TxBFCtl_Comp_ExBF_delay_Rpt_S       7

#define TxBFCTL_Disable_Steering            0x100

#define Delay_Rpt                   1
#define Immediately_Rpt             2
#endif
    HAL_BOOL     ath_hal_lowerHTtxgain;
    u_int16_t    ath_hal_xpblevel5;
    HAL_BOOL     ath_hal_sta_update_tx_pwr_enable;
    HAL_BOOL     ath_hal_sta_update_tx_pwr_enable_S1;
    HAL_BOOL     ath_hal_sta_update_tx_pwr_enable_S2;
    HAL_BOOL     ath_hal_sta_update_tx_pwr_enable_S3;
    u_int32_t    ath_hal_war70c;
    u_int8_t     ath_hal_show_bb_panic;
} HAL_OPS_CONFIG;

typedef enum {
    DFS_UNINIT_DOMAIN   = 0,    /* Uninitialized dfs domain */
    DFS_FCC_DOMAIN      = 1,    /* FCC3 dfs domain */
    DFS_ETSI_DOMAIN     = 2,    /* ETSI dfs domain */
    DFS_MKK4_DOMAIN = 3,    /* Japan dfs domain */
} HAL_DFS_DOMAIN;

#define MAX_PACAL_SKIPCOUNT    8
 typedef struct {
    int32_t   prevOffset; /* Previous value of PA offset value */
    int8_t    maxSkipCount;   /* Max No. of time PACAl can be skipped */
    int8_t    skipCount;  /* No. of time the PACAl to be skipped */
} HAL_PACAL_INFO;

/* nf - parameters related to noise floor filtering */
struct noise_floor_limits {
    int16_t nominal; /* what is the expected NF for this chip / band */
    int16_t min;     /* maximum expected NF for this chip / band */
    int16_t max;     /* minimum expected NF for this chip / band */
};

/*
 * Definitions for ah_flags in ath_hal_private
 */
#define AH_USE_EEPROM   0x1
#define AH_IS_HB63      0x2

/*
 * The ``private area'' follows immediately after the ``public area''
 * in the data structure returned by ath_hal_attach.  Private data are
 * used by device-independent code such as the regulatory domain support.
 * This data is not resident in the public area as a client may easily
 * override them and, potentially, bypass access controls.  In general,
 * code within the HAL should never depend on data in the public area.
 * Instead any public data needed internally should be shadowed here.
 *
 * When declaring a device-specific ath_hal data structure this structure
 * is assumed to at the front; e.g.
 *
 *  struct ath_hal_5212 {
 *      struct ath_hal_private  ah_priv;
 *      ...
 *  };
 *
 * It might be better to manage the method pointers in this structure
 * using an indirect pointer to a read-only data structure but this would
 * disallow class-style method overriding (and provides only minimally
 * better protection against client alteration).
 */
struct ath_hal_private {
    struct ath_hal  h;          /* public area */

    /* NB: all methods go first to simplify initialization */
    HAL_BOOL    (*ah_getChannelEdges)(struct ath_hal*,
                u_int16_t channelFlags,
                u_int16_t *lowChannel, u_int16_t *highChannel);
    u_int       (*ah_getWirelessModes)(struct ath_hal*);
    HAL_BOOL    (*ah_eepromRead)(struct ath_hal *, u_int off,
                u_int16_t *data);
    HAL_BOOL    (*ah_eepromWrite)(struct ath_hal *, u_int off,
                u_int16_t data);
    u_int       (*ah_eepromDump)(struct ath_hal *ah, void **ppE);
    HAL_BOOL    (*ah_getChipPowerLimits)(struct ath_hal *,
                HAL_CHANNEL *, u_int32_t);
    int16_t     (*ah_getNfAdjust)(struct ath_hal *,
                const HAL_CHANNEL_INTERNAL*);

    u_int16_t   (*ah_eepromGetSpurChan)(struct ath_hal *, u_int16_t, HAL_BOOL);
    /*
     * Device revision information.
     */
    u_int16_t           ah_devid;           /* PCI device ID */
    u_int16_t           ah_subvendorid;     /* PCI subvendor ID */
    u_int32_t           ah_macVersion;      /* MAC version id */
    u_int16_t           ah_macRev;          /* MAC revision */
    u_int16_t           ah_phyRev;          /* PHY revision */
    u_int16_t           ah_analog5GhzRev;   /* 2GHz radio revision */
    u_int16_t           ah_analog2GhzRev;   /* 5GHz radio revision */
    u_int32_t           ah_flags;           /* misc flags */
    HAL_VENDORS         ah_vendor;          /* Vendor ID */

    HAL_OPMODE          ah_opmode;          /* operating mode from reset */
    HAL_OPS_CONFIG      ah_config;          /* Operating Configuration */
    HAL_CAPABILITIES    ah_caps;            /* device capabilities */
    u_int32_t           ah_diagreg;         /* user-specified AR_DIAG_SW */
    int16_t             ah_powerLimit;      /* tx power cap */
    u_int16_t           ah_maxPowerLevel;   /* calculated max tx power */
    u_int               ah_tpScale;         /* tx power scale factor */
    u_int16_t           ah_extra_txpow;     /* low rates extra-txpower */

    /*
     * State for regulatory domain handling.
     */
    HAL_REG_DOMAIN      ah_currentRD;       /* Current regulatory domain */
    HAL_REG_DOMAIN      ah_currentRDExt;    /* Regulatory domain Extension reg from EEPROM*/
    HAL_CTRY_CODE       ah_countryCode;     /* current country code */
    HAL_REG_DOMAIN      ah_currentRDInUse;  /* Current 11d domain in used */
    HAL_REG_DOMAIN      ah_currentRD5G;     /* Current 5G regulatory domain */
    HAL_REG_DOMAIN      ah_currentRD2G;     /* Current 2G regulatory domain */
    char                ah_iso[4];          /* current country iso + NULL */       
    HAL_DFS_DOMAIN      ah_dfsDomain;       /* current dfs domain */
    START_ADHOC_OPTION  ah_adHocMode;    /* ad-hoc mode handling */
    HAL_BOOL            ah_commonMode;      /* common mode setting */
    /* NB: 802.11d stuff is not currently used */
    HAL_BOOL            ah_cc11d;       /* 11d country code */
    COUNTRY_INFO_LIST   ah_cc11dInfo;     /* 11d country code element */
    u_int               ah_nchan;       /* valid channels in list */
    HAL_CHANNEL_INTERNAL *ah_curchan;   /* current channel */

    u_int8_t            ah_coverageClass;       /* coverage class */
    HAL_BOOL            ah_regdomainUpdate;     /* regdomain is updated? */
    u_int64_t           ah_tsf_channel;         /* tsf @ which last channel change happened */
    HAL_BOOL            ah_cwCalRequire;
    /*
     * RF Silent handling; setup according to the EEPROM.
     */
    u_int16_t   ah_rfsilent;        /* GPIO pin + polarity */
    HAL_BOOL    ah_rfkillEnabled;   /* enable/disable RfKill */
    HAL_BOOL    ah_rfkillINTInit;   /* RFkill INT initialization */

    HAL_BOOL    ah_isPciExpress;    /* XXX: only used for ar5212 MAC (Condor/Hawk/Swan) */
    HAL_BOOL    ah_eepromTxPwr;     /* If EEPROM is programmed with TxPower (Condor/Swan/Hawk) */

    u_int8_t    ah_singleWriteKC;   /* write key cache one word at time */
    u_int8_t    ah_txTrigLevel;     /* Stores the current prefetch trigger
                                       level so that it can be applied across reset */
    struct noise_floor_limits *nfp; /* points to either nf_2GHz or nf_5GHz */
    struct noise_floor_limits nf_2GHz;
    struct noise_floor_limits nf_5GHz;
    int16_t nf_cw_int_delta; /* diff btwn nominal NF and CW interf threshold */
#ifndef ATH_NF_PER_CHAN
    HAL_NFCAL_HIST_FULL  nfCalHist;
#endif
#ifdef ATH_CCX
    u_int8_t        serNo[13];
#endif

#if ATH_WOW
    u_int32_t   ah_wowEventMask;   /* Mask to the AR_WOW_PATTERN_REG for the */
                                    /* enabled patterns and WOW Events. */
#endif
    u_int16_t   ah_devType;     /* Type of device */

    /* greenApPsOn -- Used to indicate that the Power save status is set */
    u_int16_t   greenApPsOn;

    u_int32_t   ah_dcs_enable;

    HAL_PACAL_INFO ah_paCalInfo;

    u_int32_t   ah_bbPanicTimeoutMs;    /* 0 to disable Watchdog */
    u_int32_t   ah_bbPanicLastStatus;   /* last occurrence status saved */
    u_int8_t    ah_reset_reason;        /* reason for ath hal reset */ 
    /*  Phy restart is disabled for Osprey,if any BB Panic due to RX_OFDM is seen */
    u_int8_t    ah_phyrestart_disabled;
    asf_amem_instance_handle amem_handle;
#if AH_REGREAD_DEBUG
/* In order to save memory, the buff to record the register reads are
 * divided by 8 segments, with each the size is 8192 (0x2000),
 * and the ah_regaccessbase is from 0 to 7.
 */
    u_int32_t   ah_regaccess[8192];
    u_int8_t    ah_regaccessbase;
    u_int64_t   ah_regtsf_start;
#endif
	u_int8_t    ah_enable_keysearch_always;
#ifdef ATH_SUPPORT_TxBF
    u_int32_t   ah_basic_set_buf;       /* buffer for basic set register*/
    u_int8_t    ah_lowest_txrate;       /* current lowest tx rate */
#endif
};
#define AH_PRIVATE(_ah) ((struct ath_hal_private *)(_ah))

#ifdef ATH_NO_5G_SUPPORT
#define AH_MAX_CHANNELS 64
#else
#define AH_MAX_CHANNELS 256
#endif

struct ath_hal_private_tables {
    struct ath_hal_private priv;
    HAL_CHANNEL_INTERNAL ah_channels[AH_MAX_CHANNELS];  /* calculated channel list */
};
#define AH_TABLES(_ah) ((struct ath_hal_private_tables *)(_ah))

#define ath_hal_getChannelEdges(_ah, _cf, _lc, _hc) \
    AH_PRIVATE(_ah)->ah_getChannelEdges(_ah, _cf, _lc, _hc)
#define ath_hal_getWirelessModes(_ah) \
    AH_PRIVATE(_ah)->ah_getWirelessModes(_ah)
#define ath_hal_eepromRead(_ah, _off, _data) \
    AH_PRIVATE(_ah)->ah_eepromRead(_ah, (_off), (_data))
#define ath_hal_eepromWrite(_ah, _off, _data) \
    AH_PRIVATE(_ah)->ah_eepromWrite(_ah, (_off), (_data))
#define ath_hal_eepromDump(_ah, _data) \
    AH_PRIVATE(_ah)->ah_eepromDump(_ah, (_data))
#define ath_hal_gpioCfgOutput(_ah, _gpio, _signalType) \
    (_ah)->ah_gpioCfgOutput(_ah, (_gpio), (_signalType))
#define ath_hal_gpioCfgInput(_ah, _gpio) \
    (_ah)->ah_gpioCfgInput(_ah, (_gpio))
#define ath_hal_gpioGet(_ah, _gpio) \
    (_ah)->ah_gpioGet(_ah, (_gpio))
#define ath_hal_gpioSet(_ah, _gpio, _val) \
    (_ah)->ah_gpioSet(_ah, (_gpio), (_val))
#define ath_hal_gpioSetIntr(_ah, _gpio, _ilevel) \
    (_ah)->ah_gpioSetIntr(_ah, (_gpio), (_ilevel))
#define ath_hal_getpowerlimits(_ah, _chans, _nchan) \
    AH_PRIVATE(_ah)->ah_getChipPowerLimits(_ah, (_chans), (_nchan))
#define ath_hal_getNfAdjust(_ah, _c) \
    AH_PRIVATE(_ah)->ah_getNfAdjust(_ah, _c)

#if !defined(_NET_IF_IEEE80211_H_) && !defined(_NET80211__IEEE80211_H_)
/*
 * Stuff that would naturally come from _ieee80211.h
 */
#define IEEE80211_ADDR_LEN      6

#define IEEE80211_WEP_KEYLEN            5   /* 40bit */
#define IEEE80211_WEP_IVLEN         3   /* 24bit */
#define IEEE80211_WEP_KIDLEN            1   /* 1 octet */
#define IEEE80211_WEP_CRCLEN            4   /* CRC-32 */

#define IEEE80211_CRC_LEN           4

#define IEEE80211_MTU               1500
#define IEEE80211_MAX_LEN           (2300 + IEEE80211_CRC_LEN + \
    (IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_CRCLEN))
#define IEEE80211_AMPDU_LIMIT_MAX   (64 * 1024 - 1)

enum {
    IEEE80211_T_DS,         /* direct sequence spread spectrum */
    IEEE80211_T_FH,         /* frequency hopping */
    IEEE80211_T_OFDM,       /* frequency division multiplexing */
    IEEE80211_T_TURBO,      /* high rate DS */
        IEEE80211_T_HT,                 /* HT - Full GI */

        IEEE80211_T_MAX
};
#define IEEE80211_T_CCK IEEE80211_T_DS  /* more common nomenclatur */
#endif /* _NET_IF_IEEE80211_H_ */

/* NB: these are defined privately until XR support is announced */
enum {
    ATHEROS_T_XR    = IEEE80211_T_TURBO+1,  /* extended range */
};

#define HAL_COMP_BUF_MAX_SIZE   9216       /* 9K */
#define HAL_COMP_BUF_ALIGN_SIZE 512

#define HAL_TXQ_USE_LOCKOUT_BKOFF_DIS   0x00000001

#define INIT_AIFS       2
#define INIT_CWMIN      15
#define INIT_CWMIN_11B      31
#define INIT_CWMAX      1023
#define INIT_SH_RETRY       2 /* Per rate rts fail count before switching to next rate */
#define INIT_LG_RETRY       10
#define INIT_SSH_RETRY      32
#define INIT_SLG_RETRY      32

typedef struct {
    u_int32_t   tqi_ver;        /* HAL TXQ verson */
    HAL_TX_QUEUE    tqi_type;       /* hw queue type*/
    HAL_TX_QUEUE_SUBTYPE tqi_subtype;   /* queue subtype, if applicable */
    HAL_TX_QUEUE_FLAGS tqi_qflags;      /* queue flags */
    u_int32_t   tqi_priority;
    u_int32_t   tqi_aifs;       /* aifs */
    u_int32_t   tqi_cwmin;      /* cwMin */
    u_int32_t   tqi_cwmax;      /* cwMax */
    u_int16_t   tqi_shretry;        /* frame short retry limit */
    u_int16_t   tqi_lgretry;        /* frame long retry limit */
    u_int32_t   tqi_cbrPeriod;
    u_int32_t   tqi_cbrOverflowLimit;
    u_int32_t   tqi_burstTime;
    u_int32_t   tqi_readyTime;
    u_int32_t   tqi_physCompBuf;
    u_int32_t   tqi_intFlags;       /* flags for internal use */
} HAL_TX_QUEUE_INFO;

extern  HAL_BOOL ath_hal_setTxQProps(struct ath_hal *ah,
        HAL_TX_QUEUE_INFO *qi, const HAL_TXQ_INFO *qInfo);
extern  HAL_BOOL ath_hal_getTxQProps(struct ath_hal *ah,
        HAL_TXQ_INFO *qInfo, const HAL_TX_QUEUE_INFO *qi);

typedef enum {
    HAL_ANI_PRESENT = 0x1,            /* is ANI support present */
    HAL_ANI_NOISE_IMMUNITY_LEVEL = 0x2,            /* set level */
    HAL_ANI_OFDM_WEAK_SIGNAL_DETECTION = 0x4, /* enable/disable */
    HAL_ANI_CCK_WEAK_SIGNAL_THR = 0x8,        /* enable/disable */
    HAL_ANI_FIRSTEP_LEVEL = 0x10,                  /* set level */
    HAL_ANI_SPUR_IMMUNITY_LEVEL = 0x20,            /* set level */
    HAL_ANI_MODE = 0x40,              /* 0 => manual, 1 => auto */
    HAL_ANI_PHYERR_RESET = 0x80,       /* reset phy error stats */
    HAL_ANI_MRC_CCK = 0x100,                  /* enable/disable */
    HAL_ANI_ALL = 0xffff
} HAL_ANI_CMD;

#define HAL_SPUR_VAL_MASK       0x3FFF
#define HAL_SPUR_CHAN_WIDTH     87
#define HAL_BIN_WIDTH_BASE_100HZ    3125
#define HAL_BIN_WIDTH_TURBO_100HZ   6250
#define HAL_MAX_BINS_ALLOWED        28

#define CHANNEL_XR_A    (CHANNEL_A | CHANNEL_XR)
#define CHANNEL_XR_G    (CHANNEL_PUREG | CHANNEL_XR)
#define CHANNEL_XR_T    (CHANNEL_T | CHANNEL_XR)

/*
 * A    = 5GHZ|OFDM
 * T    = 5GHZ|OFDM|TURBO
 * XR_T = 2GHZ|OFDM|XR
 *
 * IS_CHAN_A(T) or IS_CHAN_A(XR_T) will return TRUE.  This is probably
 * not the default behavior we want.  We should migrate to a better mask --
 * perhaps CHANNEL_ALL.
 *
 * For now, IS_CHAN_G() masks itself with CHANNEL_108G.
 *
 */

#define IS_CHAN_A(_c)   ((((_c)->channelFlags & CHANNEL_A) == CHANNEL_A) || \
             (((_c)->channelFlags & CHANNEL_A_HT20) == CHANNEL_A_HT20) || \
             (((_c)->channelFlags & CHANNEL_A_HT40PLUS) == CHANNEL_A_HT40PLUS) || \
             (((_c)->channelFlags & CHANNEL_A_HT40MINUS) == CHANNEL_A_HT40MINUS))
#define IS_CHAN_B(_c)   (((_c)->channelFlags & CHANNEL_B) == CHANNEL_B)
#define IS_CHAN_G(_c)   ((((_c)->channelFlags & (CHANNEL_108G|CHANNEL_G)) == CHANNEL_G) ||  \
             (((_c)->channelFlags & CHANNEL_G_HT20) == CHANNEL_G_HT20) || \
             (((_c)->channelFlags & CHANNEL_G_HT40PLUS) == CHANNEL_G_HT40PLUS) || \
             (((_c)->channelFlags & CHANNEL_G_HT40MINUS) == CHANNEL_G_HT40MINUS))
#define IS_CHAN_108G(_c)(((_c)->channelFlags & CHANNEL_108G) == CHANNEL_108G)
#define IS_CHAN_T(_c)   (((_c)->channelFlags & CHANNEL_T) == CHANNEL_T)
#define IS_CHAN_X(_c)   (((_c)->channelFlags & CHANNEL_X) == CHANNEL_X)
#define IS_CHAN_PUREG(_c) \
    (((_c)->channelFlags & CHANNEL_PUREG) == CHANNEL_PUREG)

#define IS_CHAN_TURBO(_c)   (((_c)->channelFlags & CHANNEL_TURBO) != 0)
#define IS_CHAN_CCK(_c)     (((_c)->channelFlags & CHANNEL_CCK) != 0)
#define IS_CHAN_OFDM(_c)    (((_c)->channelFlags & CHANNEL_OFDM) != 0)
#define IS_CHAN_XR(_c)      (((_c)->channelFlags & CHANNEL_XR) != 0)
#define IS_CHAN_5GHZ(_c)    (((_c)->channelFlags & CHANNEL_5GHZ) != 0)
#define IS_CHAN_2GHZ(_c)    (((_c)->channelFlags & CHANNEL_2GHZ) != 0)
#define IS_CHAN_PASSIVE(_c) (((_c)->channelFlags & CHANNEL_PASSIVE) != 0)
#define IS_CHAN_HALF_RATE(_c)   (((_c)->channelFlags & CHANNEL_HALF) != 0)
#define IS_CHAN_QUARTER_RATE(_c) (((_c)->channelFlags & CHANNEL_QUARTER) != 0)
#define IS_CHAN_HT20(_c)        (((_c)->channelFlags & CHANNEL_HT20) != 0)
#define IS_CHAN_HT40(_c)        ((((_c)->channelFlags & CHANNEL_HT40PLUS) != 0) || (((_c)->channelFlags & CHANNEL_HT40MINUS) != 0))
#define IS_CHAN_HT(_c)          (IS_CHAN_HT20((_c)) || IS_CHAN_HT40((_c)))

#define IS_CHAN_NA_20(_c) \
    (((_c)->channelFlags & CHANNEL_A_HT20) == CHANNEL_A_HT20)
#define IS_CHAN_NA_40PLUS(_c) \
    (((_c)->channelFlags & CHANNEL_A_HT40PLUS) == CHANNEL_A_HT40PLUS)
#define IS_CHAN_NA_40MINUS(_c) \
    (((_c)->channelFlags & CHANNEL_A_HT40MINUS) == CHANNEL_A_HT40MINUS)

#define IS_CHAN_NG_20(_c) \
    (((_c)->channelFlags & CHANNEL_G_HT20) == CHANNEL_G_HT20)
#define IS_CHAN_NG_40PLUS(_c) \
    (((_c)->channelFlags & CHANNEL_G_HT40PLUS) == CHANNEL_G_HT40PLUS)
#define IS_CHAN_NG_40MINUS(_c) \
    (((_c)->channelFlags & CHANNEL_G_HT40MINUS) == CHANNEL_G_HT40MINUS)

#define IS_CHAN_IN_PUBLIC_SAFETY_BAND(_c) ((_c) > 4940 && (_c) < 4990)

#ifdef ATH_NF_PER_CHAN
#define AH_HOME_CHAN_NFCAL_HIST(ah) (AH_PRIVATE(ah)->ah_curchan ? &AH_PRIVATE(ah)->ah_curchan->nfCalHist: NULL)
#else
#define AH_HOME_CHAN_NFCAL_HIST(ah) (&AH_PRIVATE(ah)->nfCalHist)
#endif
/*
 * Deduce if the host cpu has big- or litt-endian byte order.
 */
static inline int
isBigEndian(void)
{
    union {
        int32_t i;
        char c[4];
    } u;
    u.i = 1;
    return (u.c[0] == 0);
}

/* unalligned little endian access */     
#define LE_READ_2(p)                            \
    ((u_int16_t)                            \
     ((((const u_int8_t *)(p))[0]    ) | (((const u_int8_t *)(p))[1]<< 8)))
#define LE_READ_4(p)                            \
    ((u_int32_t)                            \
     ((((const u_int8_t *)(p))[0]    ) | (((const u_int8_t *)(p))[1]<< 8) |\
      (((const u_int8_t *)(p))[2]<<16) | (((const u_int8_t *)(p))[3]<<24)))

/*
 * Register manipulation macros that expect bit field defines
 * to follow the convention that an _S suffix is appended for
 * a shift count, while the field mask has no suffix.
 */
#define SM(_v, _f)  (((_v) << _f##_S) & _f)
#define MS(_v, _f)  (((_v) & _f) >> _f##_S)

#ifndef ATH_SUPPORT_HTC
#define OS_REG_RMW(_a, _r, _set, _clr)    \
        OS_REG_WRITE(_a, _r, (OS_REG_READ(_a, _r) & ~(_clr)) | (_set))
#define OS_REG_RMW_FIELD(_a, _r, _f, _v) \
    OS_REG_WRITE(_a, _r, \
        (OS_REG_READ(_a, _r) &~ _f) | (((_v) << _f##_S) & _f))
#else
#define OS_REG_RMW(_a, _r, _set, _clr)    \
        ath_hal_wmi_reg_rmw(_a, _r, _clr, _set)
#define OS_REG_RMW_FIELD(_a, _r, _f, _v) \
        ath_hal_wmi_reg_rmw(_a, _r, _f, (((_v) << _f##_S) & _f))
#endif




#define OS_REG_RMW_FIELD_ALT(_a, _r, _f, _v) \
    OS_REG_WRITE(_a, _r, \
	(OS_REG_READ(_a, _r) &~(_f<<_f##_S)) | (((_v) << _f##_S) & (_f<<_f##_S)))

#define OS_REG_READ_FIELD(_a, _r, _f) \
        (((OS_REG_READ(_a, _r) & _f) >> _f##_S))

#define OS_REG_READ_FIELD_ALT(_a, _r, _f) \
	((OS_REG_READ(_a, _r) >> (_f##_S))&(_f))

#ifndef  ATH_SUPPORT_HTC
#define OS_REG_SET_BIT(_a, _r, _f) \
    OS_REG_WRITE(_a, _r, OS_REG_READ(_a, _r) | _f)
#define OS_REG_CLR_BIT(_a, _r, _f) \
    OS_REG_WRITE(_a, _r, OS_REG_READ(_a, _r) &~ _f)
#else
#define OS_REG_SET_BIT(_a, _r, _f) \
    OS_REG_RMW(_a, _r, _f, 0)
#define OS_REG_CLR_BIT(_a, _r, _f) \
    OS_REG_RMW(_a, _r, 0, _f)
#endif




#define OS_REG_IS_BIT_SET(_a, _r, _f) \
        ((OS_REG_READ(_a, _r) & _f) != 0)

/* 
 * Regulatory domain support.
 */

/*
 * Return the max allowed antenna gain based on the current
 * regulatory domain.
 */
extern  u_int ath_hal_getantennareduction(struct ath_hal *,
        HAL_CHANNEL *, u_int twiceGain);
/*
 *
 */
u_int
ath_hal_getantennaallowed(struct ath_hal *ah, HAL_CHANNEL *chan);

/*
 * Return the test group for the specific channel based on
 * the current regulator domain.
 */
extern  u_int ath_hal_getctl(struct ath_hal *, HAL_CHANNEL *);
/*
 * Return whether or not a noise floor check is required
 * based on the current regulatory domain for the specified
 * channel.
 */
extern  u_int ath_hal_getnfcheckrequired(struct ath_hal *, HAL_CHANNEL *);

/*
 * Map a public channel definition to the corresponding
 * internal data structure.  This implicitly specifies
 * whether or not the specified channel is ok to use
 * based on the current regulatory domain constraints.
 */
extern  HAL_CHANNEL_INTERNAL *ath_hal_checkchannel(struct ath_hal *,
        const HAL_CHANNEL *);

/*
 * Convert Country / RegionDomain to RegionDomain
 * rd could be region or country code.
*/
extern HAL_REG_DOMAIN
ath_hal_getDomain(struct ath_hal *ah, u_int16_t rd);

/*
 * wait for the register contents to have the specified value. 
 * timeout is in usec.
 */
extern  HAL_BOOL ath_hal_wait(struct ath_hal *, u_int reg,
        u_int32_t mask, u_int32_t val, u_int32_t timeout);
#define AH_WAIT_TIMEOUT     100000   /* default timeout (usec) */

/* return the first n bits in val reversed */
extern  u_int32_t ath_hal_reverseBits(u_int32_t val, u_int32_t n);

/* printf interfaces */
extern  void __ahdecl ath_hal_printf(struct ath_hal *, const char*, ...)
        __printflike(2,3);
extern  void __ahdecl ath_hal_vprintf(struct ath_hal *, const char*, __va_list)
        __printflike(2, 0);
extern  const char* __ahdecl ath_hal_ether_sprintf(const u_int8_t *mac);

/*
 * Make ath_hal_malloc a macro rather than an inline function.
 * This allows the underlying amalloc macro to keep track of the
 * __FILE__ and __LINE__ of the callers to ath_hal_malloc, which
 * is a lot more interesting than only being able to see the
 * __FILE__ and __LINE__ of the def of a inline function.
 */
#define ath_hal_malloc(ah, size) \
    amalloc_adv(AH_PRIVATE(ah)->amem_handle, size, adf_os_mem_zero_outline)
#define ath_hal_free(ah, p) afree_adv(AH_PRIVATE(ah)->amem_handle, p)

/* read PCI information */
extern u_int32_t ath_hal_read_pci_config_space(struct ath_hal *ah, u_int32_t offset, void *pBuffer, u_int32_t length);

/**
**  Debugging Interface
**
**  Implemented a "DPRINTF" like interface that allows for selective enable/disable
**  of debug information.  This is completely compiled out if AH_DEBUG is not defined.
**  Bit mask is used to specify debug level.  Renamed from HALDEBUG to identify as a
**  seperately defined interface.
**
**  Note that the parameter that is used to enable debug levels is the instance variable
**  ath_hal_debug, defined in the config structure in the common hal object (ah).  This
**  can be set at startup, or can be dynamically changed through the HAL configuration
**  interface using  OOIDS or the iwpriv interface.  Allows a user to change debugging
**  emphasis without having to recompile the HAL.
**/

#ifdef AH_DEBUG

/*
** Debug Level Definitions
*/

#define HAL_DBG_RESET           0x00000001
#define HAL_DBG_PHY_IO          0x00000002
#define HAL_DBG_REG_IO          0x00000004      
#define HAL_DBG_RF_PARAM        0x00000008
#define HAL_DBG_QUEUE           0x00000010
#define HAL_DBG_EEPROM_DUMP     0x00000020
#define HAL_DBG_EEPROM          0x00000040
#define HAL_DBG_NF_CAL          0x00000080
#define HAL_DBG_CALIBRATE       0x00000100
#define HAL_DBG_CHANNEL         0x00000200
#define HAL_DBG_INTERRUPT       0x00000400
#define HAL_DBG_DFS             0x00000800
#define HAL_DBG_DMA             0x00001000
#define HAL_DBG_REGULATORY      0x00002000
#define HAL_DBG_TX              0x00004000
#define HAL_DBG_TXDESC          0x00008000
#define HAL_DBG_RX              0x00010000
#define HAL_DBG_RXDESC          0x00020000
#define HAL_DBG_ANI             0x00040000
#define HAL_DBG_BEACON          0x00080000
#define HAL_DBG_KEYCACHE        0x00100000        
#define HAL_DBG_POWER_MGMT      0x00200000          
#define HAL_DBG_MALLOC			0x00400000          
#define HAL_DBG_FORCE_BIAS		0x00800000          
#define HAL_DBG_POWER_OVERRIDE	0x01000000
#define HAL_DBG_SPUR_MITIGATE	0x02000000
#define HAL_DBG_PRINT_REG       0x04000000
#define HAL_DBG_TIMER           0x08000000

#define HAL_DBG_UNMASKABLE      0xFFFFFFFF

/*
** External reference to hal dprintf function
*/

extern  void __ahdecl HDPRINTF(struct ath_hal *ah, u_int dmask, const char* fmt, ...)
    __printflike(3,4);
#else
#define HDPRINTF(_ah, _level, _fmt, ...)
#endif /* AH_DEBUG */

/*
** Prototype for factory initialization function
*/

extern void __ahdecl ath_hal_factory_defaults(struct ath_hal_private *ap,
                                              struct hal_reg_parm *hal_conf_parm);


/*
 * Register logging definitions shared with ardecode.
 */
#include "ah_decode.h"

/*
 * Common assertion interface.  Note: it is a bad idea to generate
 * an assertion failure for any recoverable event.  Instead catch
 * the violation and, if possible, fix it up or recover from it; either
 * with an error return value or a diagnostic messages.  System software
 * does not panic unless the situation is hopeless.
 */
#ifdef AH_ASSERT
extern  void ath_hal_assert_failed(const char* filename,
        int lineno, const char* msg);

#define HALASSERT(_x) do {                  \
    if (!(_x)) {                        \
        ath_hal_assert_failed(__FILE__, __LINE__, #_x); \
    }                           \
} while (0)

/*
 * Provide an inline function wrapper for HALASSERT, to allow assertions
 * to be used in code locations where the macro can't be used.
 * Only use this when the macro doesn't work, because wrapping HALASSERT
 * in a function (even an inline function) defeats the purpose of the
 * __FILE__ and __LINE__ diagnostic info and condition display inside
 * HALASSERT.
 */
inline void hal_assert(int condition)
{
    HALASSERT(condition);
}
#else
#define HALASSERT(_x)
#define hal_assert(condition) 0
#endif /* AH_ASSERT */

/*
 * Convert between microseconds and core system clocks.
 */
extern  u_int ath_hal_mac_clks(struct ath_hal *ah, u_int usecs);
extern  u_int ath_hal_mac_usec(struct ath_hal *ah, u_int clks);

/*
 * Generic get/set capability support.  Each chip overrides
 * this routine to support chip-specific capabilities.
 */
extern  HAL_STATUS ath_hal_getcapability(struct ath_hal *ah,
        HAL_CAPABILITY_TYPE type, u_int32_t capability,
        u_int32_t *result);
extern  HAL_BOOL ath_hal_setcapability(struct ath_hal *ah,
        HAL_CAPABILITY_TYPE type, u_int32_t capability,
        u_int32_t setting, HAL_STATUS *status);

/* 
 * Macros to set/get RF KILL capabilities.
 * These macros define inside the HAL layer some of macros defined in 
 * if_athvar.h, which are visible only by the ATH layer.
 */
#define ath_hal_isrfkillenabled(_ah)  \
    ((*(_ah)->ah_getCapability)(_ah, HAL_CAP_RFSILENT, 1, AH_NULL) == HAL_OK)
#define ath_hal_enable_rfkill(_ah, _v) \
    (*(_ah)->ah_setCapability)(_ah, HAL_CAP_RFSILENT, 1, _v, AH_NULL)
#define ath_hal_hasrfkillInt(_ah)  \
    ((*(_ah)->ah_getCapability)(_ah, HAL_CAP_RFSILENT, 3, AH_NULL) == HAL_OK)

/*
 * Device revision information.
 */
typedef struct {
    u_int16_t   ah_devid;       /* PCI device ID */
    u_int16_t   ah_subvendorid;     /* PCI subvendor ID */
    u_int32_t   ah_macVersion;      /* MAC version id */
    u_int16_t   ah_macRev;      /* MAC revision */
    u_int16_t   ah_phyRev;      /* PHY revision */
    u_int16_t   ah_analog5GhzRev;   /* 2GHz radio revision */
    u_int16_t   ah_analog2GhzRev;   /* 5GHz radio revision */
} HAL_REVS;

/*
 * Argument payload for HAL_DIAG_SETKEY.
 */
typedef struct {
    HAL_KEYVAL  dk_keyval;
    u_int16_t   dk_keyix;   /* key index */
    u_int8_t    dk_mac[IEEE80211_ADDR_LEN];
    int     dk_xor;     /* XOR key data */
} HAL_DIAG_KEYVAL;

/*
 * Argument payload for HAL_DIAG_EEWRITE.
 */
typedef struct {
    u_int16_t   ee_off;     /* eeprom offset */
    u_int16_t   ee_data;    /* write data */
} HAL_DIAG_EEVAL;

typedef struct {
        u_int offset;           /* reg offset */
        u_int32_t val;          /* reg value  */
} HAL_DIAG_REGVAL;

extern  HAL_BOOL ath_hal_getdiagstate(struct ath_hal *ah, int request,
            const void *args, u_int32_t argsize,
            void **result, u_int32_t *resultsize);

/*
 * Setup a h/w rate table for use.
 */
extern  void ath_hal_setupratetable(struct ath_hal *ah, HAL_RATE_TABLE *rt);

/*
 * Common routine for implementing getChanNoise api.
 */
extern  int16_t ath_hal_getChanNoise(struct ath_hal *ah, HAL_CHANNEL *chan);

#ifdef ATH_TX99_DIAG
/* Tx99 functions */
#ifdef ATH_SUPPORT_HTC
extern void ah_tx99_tx_stopdma(struct ath_hal *ah, u_int32_t hwq_num);
extern void ah_tx99_drain_alltxq(struct ath_hal *ah);
extern void ah_tx99_start(struct ath_hal *ah, u_int8_t *data);
extern void ah_tx99_stop(struct ath_hal *ah);
#endif
#endif

/*
 * The following are for direct integration of Atheros code.
 */
typedef enum {
    WIRELESS_MODE_11a   = 0,
    WIRELESS_MODE_TURBO = 1,
    WIRELESS_MODE_11b   = 2,
    WIRELESS_MODE_11g   = 3,
    WIRELESS_MODE_108g  = 4,
    WIRELESS_MODE_XR    = 5,
    WIRELESS_MODE_11NA  = 6,
    WIRELESS_MODE_11NG  = 7,

    WIRELESS_MODE_MAX
} WIRELESS_MODE;

extern  WIRELESS_MODE ath_hal_chan2wmode(struct ath_hal *, const HAL_CHANNEL *);
extern  WIRELESS_MODE ath_hal_chan2htwmode(struct ath_hal *, const HAL_CHANNEL *);
extern  u_int ath_hal_get_curmode(struct ath_hal *, HAL_CHANNEL_INTERNAL *);

extern  u_int8_t ath_hal_chan_2_clockrateMHz(struct ath_hal *);

extern int8_t ath_hal_getTwiceMaxRegPower(struct ath_hal_private *ahpriv, HAL_CHANNEL_INTERNAL *ichan,
                            HAL_CHANNEL *chan);

#ifndef REMOVE_PKT_LOG
#include "ah_pktlog.h"
extern void ath_hal_log_ani(
            HAL_SOFTC sc, struct log_ani *log_data, u_int16_t iflags);
#endif

#define FRAME_DATA      2   /* Data frame */
#define SUBT_DATA_CFPOLL    2   /* Data + CF-Poll */
#define SUBT_NODATA_CFPOLL  6   /* No Data + CF-Poll */
#define WLAN_CTRL_FRAME_SIZE    (2+2+6+4)   /* ACK+FCS */

#define MAX_REG_ADD_COUNT   129


#endif /* _ATH_AH_INTERAL_H_ */
