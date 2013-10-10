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
#include "ah_devid.h"

#include "ar9300/ar9300desc.h"
#include "ar9300/ar9300.h"
#include "ar9300/ar9300reg.h"
#include "ar9300/ar9300phy.h"
#include "ar9300/ar9300papd.h"


/* Add static register initialization vectors */
#include "ar9300/ar9300_osprey22.ini"
#include "ar9300/ar9330_11.ini"
#include "ar9300/ar9330_12.ini"
#include "ar9300/ar9340.ini"
#include "ar9300/ar9485.ini"
#include "ar9300/ar9485_1_1.ini"
#include "ar9300/ar9580.ini"

#ifdef MDK_AP          // MDK_AP is defined only for NART. 
#include "linux_hw.h"
#endif
#define N(a)    (sizeof(a)/sizeof(a[0]))

static HAL_BOOL ar9300GetChipPowerLimits(struct ath_hal *ah,
        HAL_CHANNEL *chans, u_int32_t nchans);

static inline void ar9300AniSetup(struct ath_hal *ah);
static inline int ar9300GetRadioRev(struct ath_hal *ah);
static inline HAL_STATUS ar9300InitMacAddr(struct ath_hal *ah);
static inline HAL_STATUS ar9300HwAttach(struct ath_hal *ah);
static inline void ar9300HwDetach(struct ath_hal *ah);
static int16_t ar9300GetNfAdjust(struct ath_hal *ah, const HAL_CHANNEL_INTERNAL *c);
int ar9300GetCalIntervals(struct ath_hal *ah, HAL_CALIBRATION_TIMER **timerp, HAL_CAL_QUERY query);
#if ATH_TRAFFIC_FAST_RECOVER
unsigned long ar9300GetPll3SqsumDvc(struct ath_hal *ah);
#endif
static int ar9300InitOffsets(struct ath_hal *ah, u_int16_t devid);

static void
ar9300DisablePciePhy(struct ath_hal *ah);
#ifdef ATH_CCX
static HAL_BOOL
ar9300RecordSerialNumber(struct ath_hal *ah);
#endif

static const HAL_PERCAL_DATA iq_cal_single_sample =
                          {IQ_MISMATCH_CAL,
                          MIN_CAL_SAMPLES,
                          PER_MAX_LOG_COUNT,
                          ar9300IQCalCollect,
                          ar9300IQCalibration};

static HAL_CALIBRATION_TIMER ar9300_cals[] =
                          { {IQ_MISMATCH_CAL,               // Cal type
						     1200000,                       // Cal interval
						     0                              // Cal timestamp
							},
                          {TEMP_COMP_CAL,
							 5000,
						     0
							},
                          };
                          

/* WIN32 does not support C99 */
static const struct ath_hal_private ar9300hal = {
    {
        AR9300_MAGIC,
        HAL_ABI_VERSION,
        0,
        0,
        AH_NULL,
        AH_NULL,
        0,
        0,
        0,
        CTRY_DEFAULT,

        0,
        0,
        0,
        0,
        0,
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // decomp array
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        },

#ifdef __CARRIER_PLATFORM__
        0, /* ah_legacy_dev */
#endif
#ifdef ATH_TX99_DIAG
        0, /* ah_pwr_offset */
#endif
        0, /* ah_rxNumCurAggrGood */
        {0,0,0,0,0,0},
        ar9300GetRateTable,                 /* ah_getRateTable */
        ar9300Detach,                       /* ah_detach */

        /* Reset Functions */
        ar9300Reset,                        /* ah_reset */
        ar9300PhyDisable,                   /* ah_phyDisable */
        ar9300Disable,                      /* ah_disable */
        ar9300ConfigPciPowerSave,           /* ah_configPciPowerSave */
        ar9300SetPCUConfig,                 /* ah_setPCUConfig */
        ar9300Calibration,                  /* ah_perCalibration */
        ar9300ResetCalValid,                /* ah_resetCalValid */
        ar9300SetTxPowerLimit,              /* ah_setTxPowerLimit */

        ar9300RadarWait,                    /* ah_radarWait */

        /*New DFS functions*/
        ar9300CheckDfs,                     /* ah_arCheckDfs */
        ar9300DfsFound,                     /* ah_arDfsFound */
        ar9300EnableDfs,                    /* ah_arEnableDfs */
        ar9300GetDfsThresh,                 /* ah_arGetDfsThresh */
        ar9300GetDfsRadars,                 /* ah_arGetDfsRadars */
        ar9300GetExtensionChannel,          /* ah_getExtensionChannel */
        ar9300IsFastClockEnabled,           /* ah_isFastClockEnabled */
        ar9300_adjust_difs,                  /* ah_adjust_DIFS */
        ar9300_dfs_cac_war,                  /* ah_dfs_cac_war */
        ar9300_cac_tx_quiet,                /* ah_cac_tx_quiet */

        /* Xr Functions */
        AH_NULL,                            /* ah_xrEnable */
        AH_NULL,                            /* ah_xrDisable */

        /* Transmit functions */
        ar9300UpdateTxTrigLevel,            /* ah_updateTxTrigLevel */
        ar9300GetTxTrigLevel,               /* ah_getTxTrigLevel */
        ar9300SetupTxQueue,                 /* ah_setupTxQueue */
        ar9300SetTxQueueProps,              /* ah_setTxQueueProps */
        ar9300GetTxQueueProps,              /* ah_getTxQueueProps */
        ar9300ReleaseTxQueue,               /* ah_releaseTxQueue */
        ar9300ResetTxQueue,                 /* ah_resetTxQueue */
        ar9300GetTxDP,                      /* ah_getTxDP */
        ar9300SetTxDP,                      /* ah_setTxDP */
        ar9300NumTxPending,                 /* ah_numTxPending */
        ar9300StartTxDma,                   /* ah_startTxDma */
        ar9300StopTxDma,                    /* ah_stopTxDma */
        ar9300AbortTxDma,                   /* ah_abortTxDma */
        ar9300FillTxDesc,                   /* ah_fillTxDesc */
        ar9300SetDescLink,                  /* ah_setDescLink */
        ar9300GetDescLinkPtr,               /* ah_getDescLinkPtr */
        ar9300ClearTxDescStatus,            /* ah_clearTxDescStatus */
#ifdef ATH_SWRETRY
        ar9300ClearDestMask,                /* ah_clearDestMask */
#endif
        ar9300FillKeyTxDesc,                /* ah_clearDestMask */
        ar9300ProcTxDesc,                   /* ah_procTxDesc */
        ar9300GetRawTxDesc,                 /* ah_getRawTxDesc */
        ar9300GetTxRateCode,                /* ah_getTxRateCode */
        AH_NULL /* ar9300GetTxIntrQueue */, /* ah_getTxIntrQueue */
        ar9300SetGlobalTxTimeout,           /* ah_setGlobalTxTimeout */
        ar9300GetGlobalTxTimeout,           /* ah_getGlobalTxTimeout */
        ar9300TxReqIntrDesc,                /* ah_reqTxIntrDesc */
        ar9300CalcTxAirtime,                /* ah_calcTxAirtime */
        ar9300SetupTxStatusRing,            /* ah_setupTxStatusRing */

        /* RX Functions */
        ar9300GetRxDP,                      /* ah_getRxDP */
        ar9300SetRxDP,                      /* ah_setRxDP */
        ar9300EnableReceive,                /* ah_enableReceive */
        ar9300StopDmaReceive,               /* ah_stopDmaReceive */
        ar9300StartPcuReceive,              /* ah_startPcuReceive */
        ar9300StopPcuReceive,               /* ah_stopPcuReceive */
        ar9300AbortPcuReceive,              /* ah_abortPcuReceive */
        ar9300SetMulticastFilter,           /* ah_setMulticastFilter */
        ar9300SetMulticastFilterIndex,      /* ah_setMulticastFilterIndex */
        ar9300ClrMulticastFilterIndex,      /* ah_clrMulticastFilterIndex */
        ar9300GetRxFilter,                  /* ah_getRxFilter */
        ar9300SetRxFilter,                  /* ah_setRxFilter */
        ar9300SetRxSelEvm,                  /* ah_setRxSelEvm */
        ar9300SetRxAbort,                   /* ah_setRxAbort */
        AH_NULL /* ar9300SetupRxDesc */,    /* ah_setupRxDesc */
        ar9300ProcRxDesc,                   /* ah_procRxDesc */
        ar9300GetRxKeyIdx,                  /* ah_getRxKeyIdx */
        ar9300ProcRxDescFast,               /* ah_procRxDescFast */
        ar9300AniArPoll,                    /* ah_rxMonitor */
        ar9300ProcessMibIntr,               /* ah_procMibEvent */

        /* Misc Functions */
        ar9300GetCapability,                /* ah_getCapability */
        ar9300SetCapability,                /* ah_setCapability */
        ar9300GetDiagState,                 /* ah_getDiagState */
        ar9300GetMacAddress,                /* ah_getMacAddress */
        ar9300SetMacAddress,                /* ah_setMacAddress */
        ar9300GetBssIdMask,                 /* ah_getBssIdMask */
        ar9300SetBssIdMask,                 /* ah_setBssIdMask */
        ar9300SetRegulatoryDomain,          /* ah_setRegulatoryDomain */
        ar9300SetLedState,                  /* ah_setLedState */
        ar9300SetPowerLedState,             /* ah_setpowerledstate */
        ar9300SetNetworkLedState,           /* ah_setnetworkledstate */
        ar9300WriteAssocid,                 /* ah_writeAssocid */
        ar9300ForceTSFSync,                 /* ah_ForceTSFSync */
        ar9300GpioCfgInput,                 /* ah_gpioCfgInput */
        ar9300GpioCfgOutput,                /* ah_gpioCfgOutput */
        ar9300GpioGet,                      /* ah_gpioGet */
        ar9300GpioSet,                      /* ah_gpioSet */
        ar9300GpioSetIntr,                  /* ah_gpioSetIntr */
        ar9300GetTsf32,                     /* ah_getTsf32 */
        ar9300GetTsf64,                     /* ah_getTsf64 */
        ar9300GetTsf2_32,                   /* ah_getTsf2_32 */
        ar9300GetTsf2_64,                   /* ah_getTsf2_64 */
        ar9300ResetTsf,                     /* ah_resetTsf */
        ar9300DetectCardPresent,            /* ah_detectCardPresent */
        ar9300UpdateMibMacStats,            /* ah_updateMibMacStats */
        ar9300GetMibMacStats,               /* ah_getMibMacStats */
        ar9300GetRfgain,                    /* ah_getRfGain */
        ar9300GetDefAntenna,                /* ah_getDefAntenna */
        ar9300SetDefAntenna,                /* ah_setDefAntenna */
        ar9300SetSlotTime,                  /* ah_setSlotTime */
        ar9300GetSlotTime,                  /* ah_getSlotTime */
        ar9300SetAckTimeout,                /* ah_setAckTimeout */
        ar9300GetAckTimeout,                /* ah_getAckTimeout */
        ar9300SetCTSTimeout,                /* ah_setCTSTimeout */
        ar9300GetCTSTimeout,                /* ah_getCTSTimeout */
        AH_NULL /* ar9300SetDecompMask */,  /* ah_setDecompMask */
        ar9300SetCoverageClass,             /* ah_setCoverageClass */
        ar9300SetQuiet,                     /* ah_setQuiet */
        ar9300SetAntennaSwitch,             /* ah_setAntennaSwitch */
        ar9300GetDescInfo,                  /* ah_getDescInfo */
        ar9300MarkPhyInactive,              /* ah_markPhyInactive */
        ar9300SelectAntConfig,              /* ah_selectAntConfig */
        ar9300GetNumAntCfg,                 /* ah_getNumAntCfg */
        ar9300SetEifsMask,                  /* ah_setEifsMask */
        ar9300GetEifsMask,                  /* ah_getEifsMask */
        ar9300SetEifsDur,                   /* ah_setEifsDur */
        ar9300GetEifsDur,                   /* ah_getEifsDur */
	ar9300AntCtrlCommonGet,	
        ar9300EnableTPC,                    /* ah_EnableTPC */
        AH_NULL,                            /* ah_olpcTempCompensation */
        ar9300DisablePhyRestart,            /* ah_disablePhyRestart */
        ar9300_enable_keysearch_always,
        ar9300InterferenceIsPresent,        /* ah_InterferenceIsPresent */
        ar9300DispTPCTables,                /* ah_DispTPCTables */
        /* Key Cache Functions */
        ar9300GetKeyCacheSize,              /* ah_getKeyCacheSize */
        ar9300ResetKeyCacheEntry,           /* ah_resetKeyCacheEntry */
        ar9300IsKeyCacheEntryValid,         /* ah_isKeyCacheEntryValid */
        ar9300SetKeyCacheEntry,             /* ah_setKeyCacheEntry */
        ar9300SetKeyCacheEntryMac,          /* ah_setKeyCacheEntryMac */
        ar9300PrintKeyCache,                /* ah_PrintKeyCache */

        /* Power Management Functions */
        ar9300SetPowerMode,                 /* ah_setPowerMode */
        ar9300GetPowerMode,                 /* ah_getPowerMode */
        ar9300SetSmPowerMode,               /* ah_setSmPsMode */
#if ATH_WOW
        ar9300WowApplyPattern,              /* ah_wowApplyPattern */
        ar9300WowEnable,                    /* ah_wowEnable */
        ar9300WowWakeUp,                    /* ah_wowWakeUp */
#endif

        /* Get Channel Noise */
        ath_hal_getChanNoise,               /* ah_getChanNoise */
        ar9300ChainNoiseFloor,              /* ah_getChainNoiseFloor */

        /* Beacon Functions */
        ar9300BeaconInit,                   /* ah_beaconInit */
        ar9300SetStaBeaconTimers,           /* ah_setStationBeaconTimers */
        ar9300ResetStaBeaconTimers,         /* ah_resetStationBeaconTimers */
        ar9300WaitForBeaconDone,            /* ah_waitForBeaconDone */

        /* Interrupt Functions */
        ar9300IsInterruptPending,           /* ah_isInterruptPending */
        ar9300GetPendingInterrupts,         /* ah_getPendingInterrupts */
        ar9300GetInterrupts,                /* ah_getInterrupts */
        ar9300SetInterrupts,                /* ah_setInterrupts */
        ar9300SetIntrMitigationTimer,       /* ah_SetIntrMitigationTimer */
        ar9300GetIntrMitigationTimer,       /* ah_GetIntrMitigationTimer */
	ar9300ForceVCS,
        ar9300SetDfs3StreamFix,
        ar9300Get3StreamSignature,

        /* 11n specific functions (NOT applicable to ar9300) */
        ar9300Set11nTxDesc,                 /* ah_set11nTxDesc */
        /* Start PAPRD functions  */
        ar9300SetPAPRDTxDesc,               /* ah_setPAPRDTxDesc */
        ar9300PAPDInitTable,                /* ah_PAPRDInitTable */
        ar9300PaprdSetupGainTable,          /* ah_PAPRDSetupGainTable */
        ar9300PAPRDCreateCurve,             /* ah_PAPRDCreateCurve */
        ar9300PAPRDisDone,                  /* ah_PAPRDisDone */
        ar9300EnablePAPD,                   /* ah_PAPRDEnable */
        ar9300PopulatePaprdSingleTable,     /* ah_PAPRDPopulateTable */
        ar9300IsTxDone,                     /* ah_isTxDone */
        ar9300_paprd_dec_tx_pwr,            /* ah_paprd_dec_tx_pwr*/
        ar9300PAPRDThermalSend,             /* ah_PAPRDThermalSend */
        /* End PAPRD functions */
#ifdef ATH_SUPPORT_TxBF
        /* TxBf specific functions */
        ar9300Set11nTxBFSounding,           /* ah_set11nTxBFSounding*/
#ifdef TXBF_TODO
        ar9300Set11nTxBFCal,                /* ah_set11nTxBFCal */
        ar9300TxBFSaveCVFromCompress,       /* ah_TxBFSaveCVFromCompress */
        ar9300TxBFSaveCVFromNonCompress,    /* ah_TxBFSaveCVFromNonCompress */
        ar9300TxBFRCUpdate,                 /* ah_TxBFRCUpdate */
        ar9300FillCSIFrame,                 /* ah_FillCSIFrame */
#endif
        ar9300GetTxBFCapabilities,          /* ah_getTxBFCapability */
        ar9300ReadKeyCacheMac,              /* ah_readKeyCacheMac */
        ar9300TxBFSetKey,                   /* ah_TxBFSetKey*/
        ar9300SetHwCvTimeout,               /* ah_SetHwCvTimeout*/
        ar9300FillTxBFCapabilities,         /* ah_FillTxBFCap*/
        ar9300_reset_lowest_txrate,         /* ah_reset_lowest_txrate */
        ar9300GetPerRateTxBFFlag,           /* ah_getPerRateTxBFFlag */
#endif
        ar9300Set11nRateScenario,           /* ah_set11nRateScenario */
        ar9300Set11nAggrFirst,              /* ah_set11nAggrFirst */
        ar9300Set11nAggrMiddle,             /* ah_set11nAggrMiddle */
        ar9300Set11nAggrLast,               /* ah_set11nAggrLast */
        ar9300Clr11nAggr,                   /* ah_clr11nAggr */
        ar9300Set11nRifsBurstMiddle,        /* ah_set11nRifsBurstMiddle */
        ar9300Set11nRifsBurstLast,          /* ah_set11nRifsBurstLast */
        ar9300Clr11nRifsBurst,              /* ah_clr11nRifsBurst */
        ar9300Set11nAggrRifsBurst,          /* ah_set11nAggrRifsBurst */
        ar9300Set11nRxRifs,                 /* ah_set11nRxRifs */
        ar9300Get11nRxRifs,                 /* ah_get11nRxRifs */
        ar9300SetSmartAntenna,              /* ah_setSmartAntenna */
        ar9300DetectBbHang,                 /* ah_detectBbHang */
        ar9300DetectMacHang,                /* ah_detectMacHang */
        ar9300SetImmunity,                  /* ah_immunity */
        ar9300GetHwHangs,                   /* ah_getHangTypes */
        ar9300Set11nBurstDuration,          /* ah_set11nBurstDuration */
        ar9300Set11nVirtualMoreFrag,        /* ah_set11nVirtualMoreFrag */
        ar9300Get11nExtBusy,                /* ah_get11nExtBusy */
        ar9300Set11nMac2040,                /* ah_set11nMac2040 */
        ar9300Get11nRxClear,                /* ah_get11nRxClear */
        ar9300Set11nRxClear,                /* ah_set11nRxClear */
        ar9300Get11nHwPlatform,             /* ah_get11nHwPlatform */
        ar9300UpdateNavReg,                 /* ah_update_navReg */ 
        ar9300GetMibCycleCountsPct,         /* ah_getMibCycleCountsPct */
        ar9300DmaRegDump,                   /* ah_dmaRegDump */

        /* ForcePPM specific functions */
        ar9300PpmGetRssiDump,               /* ah_ppmGetRssiDump */
        ar9300PpmArmTrigger,                /* ah_ppmArmTrigger */
        ar9300PpmGetTrigger,                /* ah_ppmGetTrigger */
        ar9300PpmForce,                     /* ah_ppmForce */
        ar9300PpmUnForce,                   /* ah_ppmUnForce */
        ar9300PpmGetForceState,             /* ah_ppmGetForceState */

        ar9300_getSpurInfo,                 /* ah_getSpurInfo */
        ar9300_setSpurInfo,                 /* ah_getSpurInfo */

        ar9300GetMinCCAPwr,                 /* ah_arGetNoiseFloorVal */
        ar9300SetNominalUserNFVal,          /* ah_setNfNominal */
        ar9300GetNominalUserNFVal,          /* ah_getNfNominal */
        ar9300SetMinUserNFVal,              /* ah_setNfMin */
        ar9300GetMinUserNFVal,              /* ah_getNfMin */
        ar9300SetMaxUserNFVal,              /* ah_setNfMax */
        ar9300GetMaxUserNFVal,              /* ah_getNfMax */
        ar9300SetNfDeltaVal,                /* ah_setNfCwIntDelta */
        ar9300GetNfDeltaVal,                /* ah_getNfCwIntDelta */

        ar9300GreenApPsOnOff,               /* ah_setRxGreenApPsOnOff */
        ar9300IsSingleAntPowerSavePossible, /* ah_isSingleAntPowerSavePossible */
        ar9300_get_vow_stats,              /* ah_get_vow_stats */
#ifdef ATH_CCX
        /* radio measurement specific functions */
        ar9300GetMibCycleCounts,            /* ah_getMibCycleCounts */
        ar9300ClearMibCounters,             /* ah_clearMibCounters */
        ar9300GetCcaThreshold,              /* ah_getCcaThreshold */
        ar9300GetCurRssi,                   /* ah_getCurRSSI */
#endif
#ifdef ATH_BT_COEX
        /* Bluetooth Coexistence functions */
        ar9300SetBTCoexInfo,                /* ah_setBTCoexInfo */
        ar9300BTCoexConfig,                 /* ah_btCoexConfig */
        ar9300BTCoexSetQcuThresh,           /* ah_btCoexSetQcuThresh */
        ar9300BTCoexSetWeights,             /* ah_btCoexSetWeights */
        ar9300BTCoexSetupBmissThresh,       /* ah_btCoexSetBmissThresh */
        ar9300BTCoexSetParameter,           /* ah_btCoexSetParameter */
        ar9300BTCoexDisable,                /* ah_btCoexDisable */
        ar9300BTCoexEnable,                 /* ah_btCoexEnable */
#endif
        /* Generic Timer functions */
        ar9300AllocGenericTimer,            /* ah_gentimerAlloc */
        ar9300FreeGenericTimer,             /* ah_gentimerFree */
        ar9300StartGenericTimer,            /* ah_gentimerStart */
        ar9300StopGenericTimer,             /* ah_gentimerStop */
        ar9300GetGenTimerInterrupts,        /* ah_gentimerGetIntr */

        ar9300SetDcsMode,                   /* ah_setDcsMode */
        ar9300GetDcsMode,                   /* ah_getDcsMode */
        
#if ATH_ANT_DIV_COMB
        ar9300AntDivCombGetConfig,
        ar9300AntDivCombSetConfig,
#endif

        ar9300GetBbPanicInfo,               /* ah_getBbPanicInfo */
        ar9300_handle_radar_bb_panic,       /* ah_handle_radar_bb_panic */
        ar9300SetHalResetReason,            /* ah_setHalResetReason */

#if ATH_SUPPORT_SPECTRAL        
        /* Spectral scan */
        ar9300ConfigFftPeriod,              /* ah_arConfigFftPeriod */
        ar9300ConfigScanPeriod,             /* ah_arConfigScanPeriod */
        ar9300ConfigScanCount,              /* ah_arConfigScanCount */
        ar9300ConfigShortRpt,               /* ah_arConfigShortRpt */
        ar9300ConfigureSpectralScan,        /* ah_arConfigureSpectral */
        ar9300GetSpectralParams,            /* ah_arGetSpectralConfig */
        ar9300StartSpectralScan,                
        ar9300StopSpectralScan,
        ar9300IsSpectralEnabled,            /* ah_arIsSpectralEnabled */
        ar9300IsSpectralActive,             /* ah_arIsSpectralActive */
        ar9300SpectralScanCapable,          /* ah_arIsSpectralScanCapable */
        ar9300GetCtlChanNF,
        ar9300GetExtChanNF,
#endif  /*  ATH_SUPPORT_SPECTRAL */ 

#if ATH_SUPPORT_RAW_ADC_CAPTURE
        ar9300EnableTestAddacMode,          /* ah_arEnableTestAddacMode */
        ar9300DisableTestAddacMode,         /* ah_arDisableTestAddacMode */
        ar9300BeginAdcCapture,              /* ah_arBeginAdcCapture */
        ar9300RetrieveCaptureData,          /* ah_arRetrieveCaptureData */
        ar9300CalculateADCRefPowers,        /* ah_arCalculateADCRefPowers */
        ar9300GetMinAGCGain,                /* ah_arGetMinAGCGain */
#endif

        ar9300PromiscMode, 
        ar9300ReadPktlogReg,
        ar9300WritePktlogReg,
        ar9300SetProxySTA,                 /* ah_setProxySTA */
        ar9300GetCalIntervals,              /* ah_getCalIntervals */
#if ATH_SUPPORT_WIRESHARK
        ar9300FillRadiotapHdr,              /* ah_fillRadiotapHdr */
#endif
#if ATH_TRAFFIC_FAST_RECOVER
        ar9300GetPll3SqsumDvc,              /* ah_getPll3SqsumDvc */
#endif
#ifdef ATH_SUPPORT_HTC
        AH_NULL,
        AH_FALSE,
        AH_NULL,
#endif

#ifdef ATH_TX99_DIAG
        /* Tx99 functions */
#ifdef ATH_SUPPORT_HTC
        AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL,
#else
        AH_NULL,
        AH_NULL,
        ar9300_tx99_channel_pwr_update,
        ar9300_tx99_start,
        ar9300_tx99_stop,
        ar9300_tx99_chainmsk_setup,
        ar9300_tx99_set_single_carrier,
#endif
#endif
        AH_NULL,
        {{AH_NULL}},
        AH_NULL,
        {AH_NULL},
        {AH_NULL},
        ar9300ChkRSSIUpdateTxPwr,
        ar9300_is_skip_paprd_by_greentx,   /* ah_is_skip_paprd_by_greentx */
        ar9300_hwgreentx_set_pal_spare,    /* ah_hwgreentx_set_pal_spare */
        ar9300_is_ani_noise_spur,         /* ah_is_ani_noise_spur */
        AH_NULL,
#ifdef ATH_SUPPORT_WAPI
        AH_NULL,
#endif
    },

    ar9300GetChannelEdges,                  /* ah_getChannelEdges */
    ar9300GetWirelessModes,                 /* ah_getWirelessModes */
    ar9300EepromReadWord,                   /* ah_eepromRead */
#ifdef AH_SUPPORT_WRITE_EEPROM
    ar9300EepromWrite,                      /* ah_eepromWrite */
#else
    AH_NULL,
#endif
    ar9300EepromDumpSupport,                /* ah_eepromDump */
    ar9300GetChipPowerLimits,               /* ah_getChipPowerLimits */

    ar9300GetNfAdjust,                      /* ah_getNfAdjust */
    /* rest is zero'd by compiler */
};

/*
 * Read MAC version/revision information from Chip registers and initialize
 * local data structures.
 */
void
ar9300ReadRevisions(struct ath_hal *ah)
{
    u_int32_t val;

    /* XXX verify if this is the correct way to read revision on Osprey */
    /* new SREV format for Sowl and later */
    val = OS_REG_READ(ah, AR_HOSTIF_REG(ah, AR_SREV));

    if(AH_PRIVATE(ah)->ah_devid == AR9300_DEVID_AR9340) {
        /* XXX: AR_SREV register in Wasp reads 0 */
        AH_PRIVATE(ah)->ah_macVersion = AR_SREV_VERSION_WASP;
    } else {
        /* Include 6-bit Chip Type (masked to 0) to differentiate from pre-Sowl versions */
        AH_PRIVATE(ah)->ah_macVersion = (val & AR_SREV_VERSION2) >> AR_SREV_TYPE2_S;
    }
#ifdef AR9300_EMULATION_BB
    /* Osprey full system emulation does not have the correct SREV */
    AH_PRIVATE(ah)->ah_macVersion = AR_SREV_VERSION_OSPREY;
#endif

#ifdef AR9330_EMULATION
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "macVersion: 0x%x\n", AH_PRIVATE(ah)->ah_macVersion);
    AH_PRIVATE(ah)->ah_macVersion = AR_SREV_VERSION_HORNET;
#endif

#ifdef AR9340_EMULATION
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "macVersion: 0x%x\n", AH_PRIVATE(ah)->ah_macVersion);
    AH_PRIVATE(ah)->ah_macVersion = AR_SREV_VERSION_WASP;
#endif

#ifdef AR9485_EMULATION
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "macVersion: 0x%x\n", AH_PRIVATE(ah)->ah_macVersion);
    AH_PRIVATE(ah)->ah_macVersion = AR_SREV_VERSION_POSEIDON;
#endif

#ifdef AH_SUPPORT_HORNET
/*
 *  EV74984, due to Hornet 1.1 didn't update WMAC revision, 
 *  so that have to read SoC's revision ID instead 
 */
    if( AH_PRIVATE(ah)->ah_macVersion == AR_SREV_VERSION_HORNET )
    {
#ifdef _WIN64
#define AR_SOC_RST_REVISION_ID         0xB8060090ULL
#else
#define AR_SOC_RST_REVISION_ID         0xB8060090
#endif
#ifdef ART_BUILD
		MyRegisterRead(AR_SOC_RST_REVISION_ID, &val);
    if( (val&AR_SREV_REVISION_HORNET_11_MASK) 
         == AR_SREV_REVISION_HORNET_11 )
        AH_PRIVATE(ah)->ah_macRev = AR_SREV_REVISION_HORNET_11;
    else
        AH_PRIVATE(ah)->ah_macRev = MS(val, AR_SREV_REVISION2);
#else
#define REG_READ(_reg)                 *((volatile u_int32_t *)(_reg))

    if( (REG_READ(AR_SOC_RST_REVISION_ID)&AR_SREV_REVISION_HORNET_11_MASK) 
         == AR_SREV_REVISION_HORNET_11 )
        AH_PRIVATE(ah)->ah_macRev = AR_SREV_REVISION_HORNET_11;
    else
        AH_PRIVATE(ah)->ah_macRev = MS(val, AR_SREV_REVISION2);

#undef REG_READ
#endif
#undef AR_SOC_RST_REVISION_ID
    }
    else
#endif
    if(AH_PRIVATE(ah)->ah_macVersion == AR_SREV_VERSION_WASP)
    {
#ifdef _WIN64
#define AR_SOC_RST_REVISION_ID         0xB8060090ULL
#else
#define AR_SOC_RST_REVISION_ID         0xB8060090
#endif
#ifdef ART_BUILD
		MyRegisterRead(AR_SOC_RST_REVISION_ID, &val);
        AH_PRIVATE(ah)->ah_macRev = val & AR_SREV_REVISION_WASP_MASK;
#else
#define REG_READ(_reg)                 *((volatile u_int32_t *)(_reg))

        AH_PRIVATE(ah)->ah_macRev = 
            REG_READ(AR_SOC_RST_REVISION_ID) & AR_SREV_REVISION_WASP_MASK; 
#undef REG_READ
#endif
#undef AR_SOC_RST_REVISION_ID
    }
    else
        AH_PRIVATE(ah)->ah_macRev = MS(val, AR_SREV_REVISION2);

    AH_PRIVATE(ah)->ah_isPciExpress = (val & AR_SREV_TYPE2_HOST_MODE) ? 0 : 1;
}

/*
 * Attach for an AR9300 part.
 */
struct ath_hal *
ar9300Attach(u_int16_t devid, HAL_ADAPTER_HANDLE osdev, HAL_SOFTC sc,
    HAL_BUS_TAG st, HAL_BUS_HANDLE sh, HAL_BUS_TYPE bustype,
    asf_amem_instance_handle amem_handle,
    struct hal_reg_parm *hal_conf_parm, HAL_STATUS *status)
{
    struct ath_hal_9300     *ahp;
    struct ath_hal          *ah;
    struct ath_hal_private  *ahpriv;
    HAL_STATUS              ecode;
    #ifdef ART_BUILD
   	u_int32_t tmpReg;
   	#endif
            
    /* NB: memory is returned zero'd */
    ahp = ar9300NewState(
        devid, osdev, sc, st, sh, bustype, amem_handle, hal_conf_parm, status);
    if (ahp == AH_NULL) return AH_NULL;
    ah = &ahp->ah_priv.priv.h;
    ar9300InitOffsets(ah, devid);

    ahpriv = AH_PRIVATE(ah);
    ah->ah_st = st;
    ah->ah_sh = sh;
    ah->ah_bustype = bustype;

    /* interrupt mitigation */
#ifdef AR5416_INT_MITIGATION
    if (ahpriv->ah_config.ath_hal_intrMitigationRx != 0) {
        ahp->ah_intrMitigationRx = AH_TRUE;
    }
#else
    /* Enable Rx mitigation (default) */
    ahp->ah_intrMitigationRx = AH_TRUE;
    ahpriv->ah_config.ath_hal_intrMitigationRx = 1;
#endif
    if (ahpriv->ah_config.ath_hal_intrMitigationTx != 0) {
        ahp->ah_intrMitigationTx = AH_TRUE;
    }

    
    // Read back AR_WA into a permanent copy and set bits 14 and 17. 
    // We need to do this to avoid RMW of this register. 
    // Do this before calling ar9300SetResetReg. 
    // If not, the AR_WA register which was inited via EEPROM will get wiped out.
    ahp->ah_WARegVal = OS_REG_READ(ah,  AR_HOSTIF_REG(ah, AR_WA));
    // Set Bits 14 and 17 in the AR_WA register.
    ahp->ah_WARegVal |= (AR_WA_D3_TO_L1_DISABLE | AR_WA_ASPM_TIMER_BASED_DISABLE);
    
    if (!ar9300SetResetReg(ah, HAL_RESET_POWER_ON)) {    /* reset chip */
        HDPRINTF(ah, HAL_DBG_RESET, "%s: couldn't reset chip\n", __func__);
        ecode = HAL_EIO;
        goto bad;
    }

#ifndef AR9330_EMULATION
    if (AR_SREV_HORNET(ah)) {

#ifdef AH_SUPPORT_HORNET
        if (!AR_SREV_HORNET_11(ah)) {
            /* Do not check bootstrap register, which cannot be trusted due to s26 switch issue on CUS164/AP121. */
            ahp->clk_25mhz = 1;
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"Bootstrap clock 25MHz\n");
        } else {
            /* check bootstrap clock setting */
#ifdef _WIN64
#define AR_SOC_SEL_25M_40M         0xB80600ACULL
#else
#define AR_SOC_SEL_25M_40M         0xB80600AC
#endif
#ifdef ART_BUILD
            MyRegisterRead(AR_SOC_SEL_25M_40M, &tmpReg);
            if(tmpReg & 0x1) {
                ahp->clk_25mhz = 0;
                HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"Bootstrap clock 40MHz\n");
            } else {
                ahp->clk_25mhz = 1;
                HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"Bootstrap clock 25MHz\n");
            }
#else

#define REG_WRITE(_reg,_val)    *((volatile u_int32_t *)(_reg)) = (_val);
#define REG_READ(_reg)          (*((volatile u_int32_t *)(_reg)))

            if (REG_READ(AR_SOC_SEL_25M_40M) & 0x1) {
                ahp->clk_25mhz = 0;
                HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"Bootstrap clock 40MHz\n");
            } else {
                ahp->clk_25mhz = 1;
                HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"Bootstrap clock 25MHz\n");
            }
#undef REG_READ
#undef REG_WRITE
#undef AR_SOC_SEL_25M_40M
#endif
        }
#endif
    }
#endif

#ifndef AR9340_EMULATION
    if (AR_SREV_WASP(ah)) {
        /* check bootstrap clock setting */
#ifdef _WIN64
#define AR9340_SOC_SEL_25M_40M         0xB80600B0ULL
#else
#define AR9340_SOC_SEL_25M_40M         0xB80600B0
#endif
#ifdef ART_BUILD
#define AR9340_REF_CLK_40              (1 << 4) /* 0 - 25MHz   1 - 40 MHz */
#ifdef MDK_AP
        if (FullAddrRead(AR9340_SOC_SEL_25M_40M) & AR9340_REF_CLK_40) {
            ahp->clk_25mhz = 0;
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"Bootstrap clock 40MHz\n");
        } else {
            ahp->clk_25mhz = 1;
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"Bootstrap clock 25MHz\n");
        }
#else
        MyRegisterRead(AR_SOC_SEL_25M_40M, &tmpReg);
        if(tmpReg & AR9340_REF_CLK_40) {
            ahp->clk_25mhz = 0;
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"Bootstrap clock 40MHz\n");
        } else 
        {
            ahp->clk_25mhz = 1;
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"Bootstrap clock 25MHz\n");
        }
#endif
#undef AR9340_REF_CLK_40
#else
#define AR9340_REF_CLK_40              (1 << 4) /* 0 - 25MHz   1 - 40 MHz */
#define REG_READ(_reg)          (*((volatile u_int32_t *)(_reg)))

        if (REG_READ(AR9340_SOC_SEL_25M_40M) & AR9340_REF_CLK_40) {
            ahp->clk_25mhz = 0;
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"Bootstrap clock 40MHz\n");
        } else {
            ahp->clk_25mhz = 1;
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"Bootstrap clock 25MHz\n");
        }
#undef REG_READ
#undef AR9340_SOC_SEL_25M_40M
#undef AR9340_REF_CLK_40
#endif
    }
#endif
    ar9300InitPLL(ah, AH_NULL);
#ifdef AR9300_EMULATION
    OS_DELAY(1000);
#endif

    if (!ar9300SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE)) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s: couldn't wakeup chip\n", __func__);
        ecode = HAL_EIO;
        goto bad;
    }

    /* No serialization of Register Accesses needed. */
    ahpriv->ah_config.ath_hal_serializeRegMode = SER_REG_MODE_OFF;
    HDPRINTF(ah, HAL_DBG_RESET, "%s: ath_hal_serializeRegMode is %d\n",
             __func__, ahpriv->ah_config.ath_hal_serializeRegMode);

    /*
     * Add mac revision check when needed.
     * - Osprey 1.0 and 2.0 no longer supported.
     */
    if (((ahpriv->ah_macVersion == AR_SREV_VERSION_OSPREY) &&
          (ahpriv->ah_macRev <= AR_SREV_REVISION_OSPREY_20)) ||
        (ahpriv->ah_macVersion != AR_SREV_VERSION_OSPREY &&
        ahpriv->ah_macVersion != AR_SREV_VERSION_WASP && 
        ahpriv->ah_macVersion != AR_SREV_VERSION_HORNET &&
        ahpriv->ah_macVersion != AR_SREV_VERSION_POSEIDON) ) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s: Mac Chip Rev 0x%02x.%x is not supported by "
            "this driver\n", __func__,
            ahpriv->ah_macVersion,
            ahpriv->ah_macRev);
        ecode = HAL_ENOTSUPP;
        goto bad;
    }

    ahpriv->ah_phyRev = OS_REG_READ(ah, AR_PHY_CHIP_ID);

    /* Setup supported calibrations */
    ahp->ah_iqCalData.calData = &iq_cal_single_sample;
    ahp->ah_suppCals = IQ_MISMATCH_CAL;

    /* Enable ANI */
    ahp->ah_ani_function = HAL_ANI_ALL;

#ifdef AR9300_EMULATION
    /* Disable RIFS */
    ahp->ah_rifs_enabled = AH_FALSE;
#else
    /* Enable RIFS */
    ahp->ah_rifs_enabled = AH_TRUE;
#endif

    HDPRINTF(ah, HAL_DBG_RESET, "%s: This Mac Chip Rev 0x%02x.%x is \n", __func__,
        ahpriv->ah_macVersion,
        ahpriv->ah_macRev);

    if (AR_SREV_HORNET_12(ah)) {
        /* mac */
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_CORE],
                        ar9331_hornet1_2_mac_core, 
                        N(ar9331_hornet1_2_mac_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_POST],
                        ar9331_hornet1_2_mac_postamble, 
                        N(ar9331_hornet1_2_mac_postamble), 5);

        /* bb */
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_CORE],
                        ar9331_hornet1_2_baseband_core, 
                        N(ar9331_hornet1_2_baseband_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_POST],
                        ar9331_hornet1_2_baseband_postamble, 
                        N(ar9331_hornet1_2_baseband_postamble), 5);

        /* radio */
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_CORE], 
                        ar9331_hornet1_2_radio_core, 
                        N(ar9331_hornet1_2_radio_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_POST], NULL, 0, 0);

        /* soc */
        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_PRE],
                ar9331_hornet1_2_soc_preamble, 
                N(ar9331_hornet1_2_soc_preamble), 2);
        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_CORE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_POST],
                ar9331_hornet1_2_soc_postamble, 
                N(ar9331_hornet1_2_soc_postamble), 2);

        /* rx/tx gain */
        INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, 
                       ar9331_common_rx_gain_hornet1_2, 
                       N(ar9331_common_rx_gain_hornet1_2), 2);
       	INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, 
                       ar9331_modes_lowest_ob_db_tx_gain_hornet1_2, 
                       N(ar9331_modes_lowest_ob_db_tx_gain_hornet1_2), 5);

        /*ath_hal_pciePowerSaveEnable should be 2 for OWL/Condor and 0 for merlin */
        ahpriv->ah_config.ath_hal_pciePowerSaveEnable = 0;

        /* Japan 2484Mhz CCK settings */
        INIT_INI_ARRAY(&ahp->ah_iniJapan2484,
                 ar9331_hornet1_2_baseband_core_txfir_coeff_japan_2484,
                 N(ar9331_hornet1_2_baseband_core_txfir_coeff_japan_2484), 2);
    
#if 0 //ATH_WOW
        /* SerDes values during WOW sleep */
        INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesWow, ar9300PciePhy_AWOW,
                       N(ar9300PciePhy_AWOW), 2);
#endif
    
#ifdef AR9330_EMULATION
        INIT_INI_ARRAY(&ahp->ah_iniModesAdditional, NULL, 0, 0);
#else
        /* additional clock settings */
        if (AH9300(ah)->clk_25mhz) {
            INIT_INI_ARRAY(&ahp->ah_iniModesAdditional, 
                            ar9331_hornet1_2_xtal_25M,
                            N(ar9331_hornet1_2_xtal_25M), 2);
        } else {
            INIT_INI_ARRAY(&ahp->ah_iniModesAdditional, 
                            ar9331_hornet1_2_xtal_40M,
                            N(ar9331_hornet1_2_xtal_40M), 2);
        }
#endif

#ifdef AR9330_EMULATION
        /* mac emu */
        INIT_INI_ARRAY(&ahp->ah_iniMacEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniMacEmu[ATH_INI_CORE],
                        ar9330Common_mac_osprey2_0_emulation, N(ar9330Common_mac_osprey2_0_emulation), 2);
        INIT_INI_ARRAY(&ahp->ah_iniMacEmu[ATH_INI_POST], NULL, 0, 0);

        /* bb emu */
        INIT_INI_ARRAY(&ahp->ah_iniBBEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniBBEmu[ATH_INI_CORE],
                        ar9330Common_osprey2_0_emulation, N(ar9330Common_osprey2_0_emulation), 2);
        INIT_INI_ARRAY(&ahp->ah_iniBBEmu[ATH_INI_POST], ar9330Modes_osprey2_0_emulation, N(ar9330Modes_osprey2_0_emulation), 5);

        /* radio emu */
        INIT_INI_ARRAY(&ahp->ah_iniRadioEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniRadioEmu[ATH_INI_CORE],
                        ar9330_130nm_radio_emulation, N(ar9330_130nm_radio_emulation), 2);
        INIT_INI_ARRAY(&ahp->ah_iniRadioEmu[ATH_INI_POST], NULL, 0, 0);

        /* soc emu */
        INIT_INI_ARRAY(&ahp->ah_iniSOCEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniSOCEmu[ATH_INI_CORE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniSOCEmu[ATH_INI_POST], NULL, 0, 0);

        /* rx gain emu */
        INIT_INI_ARRAY(&ahp->ah_iniRxGainEmu, ar9330Common_rx_gain_osprey2_0_emulation,
                       N(ar9330Common_rx_gain_osprey2_0_emulation), 2);
#endif
    } else if (AR_SREV_HORNET_11(ah)) {
        /* mac */
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_CORE],
                        ar9331_hornet1_1_mac_core, N(ar9331_hornet1_1_mac_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_POST],
                        ar9331_hornet1_1_mac_postamble, N(ar9331_hornet1_1_mac_postamble), 5);

        /* bb */
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_CORE],
                        ar9331_hornet1_1_baseband_core, N(ar9331_hornet1_1_baseband_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_POST],
                        ar9331_hornet1_1_baseband_postamble, N(ar9331_hornet1_1_baseband_postamble), 5);

        /* radio */
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_CORE], 
                        ar9331_hornet1_1_radio_core, N(ar9331_hornet1_1_radio_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_POST], NULL, 0, 0);

        /* soc */
        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_PRE],
                ar9331_hornet1_1_soc_preamble, N(ar9331_hornet1_1_soc_preamble), 2);
        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_CORE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_POST],
                ar9331_hornet1_1_soc_postamble, N(ar9331_hornet1_1_soc_postamble), 2);

        /* rx/tx gain */
        INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, 
                       ar9331_common_rx_gain_hornet1_1, 
                       N(ar9331_common_rx_gain_hornet1_1), 2);
       	INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, 
                       ar9331_modes_lowest_ob_db_tx_gain_hornet1_1, 
                       N(ar9331_modes_lowest_ob_db_tx_gain_hornet1_1), 5);

        /*ath_hal_pciePowerSaveEnable should be 2 for OWL/Condor and 0 for merlin */
        ahpriv->ah_config.ath_hal_pciePowerSaveEnable = 0;

        /* Japan 2484Mhz CCK settings */
        INIT_INI_ARRAY(&ahp->ah_iniJapan2484,
                 ar9331_hornet1_1_baseband_core_txfir_coeff_japan_2484,
                 N(ar9331_hornet1_1_baseband_core_txfir_coeff_japan_2484), 2);
    
#if 0 //ATH_WOW
        /* SerDes values during WOW sleep */
        INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesWow, ar9300PciePhy_AWOW,
                       N(ar9300PciePhy_AWOW), 2);
#endif
    
#ifdef AR9330_EMULATION
        INIT_INI_ARRAY(&ahp->ah_iniModesAdditional, NULL, 0, 0);
#else
        /* additional clock settings */
        if (AH9300(ah)->clk_25mhz) {
            INIT_INI_ARRAY(&ahp->ah_iniModesAdditional, ar9331_hornet1_1_xtal_25M,
                            N(ar9331_hornet1_1_xtal_25M), 2);
        }
        else {
            INIT_INI_ARRAY(&ahp->ah_iniModesAdditional, ar9331_hornet1_1_xtal_40M,
                            N(ar9331_hornet1_1_xtal_40M), 2);
        }
#endif

#ifdef AR9330_EMULATION
        /* mac emu */
        INIT_INI_ARRAY(&ahp->ah_iniMacEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniMacEmu[ATH_INI_CORE],
                        ar9330Common_mac_osprey2_0_emulation, N(ar9330Common_mac_osprey2_0_emulation), 2);
        INIT_INI_ARRAY(&ahp->ah_iniMacEmu[ATH_INI_POST], NULL, 0, 0);

        /* bb emu */
        INIT_INI_ARRAY(&ahp->ah_iniBBEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniBBEmu[ATH_INI_CORE],
                        ar9330Common_osprey2_0_emulation, N(ar9330Common_osprey2_0_emulation), 2);
        INIT_INI_ARRAY(&ahp->ah_iniBBEmu[ATH_INI_POST], ar9330Modes_osprey2_0_emulation, N(ar9330Modes_osprey2_0_emulation), 5);

        /* radio emu */
        INIT_INI_ARRAY(&ahp->ah_iniRadioEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniRadioEmu[ATH_INI_CORE],
                        ar9330_130nm_radio_emulation, N(ar9330_130nm_radio_emulation), 2);
        INIT_INI_ARRAY(&ahp->ah_iniRadioEmu[ATH_INI_POST], NULL, 0, 0);

        /* soc emu */
        INIT_INI_ARRAY(&ahp->ah_iniSOCEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniSOCEmu[ATH_INI_CORE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniSOCEmu[ATH_INI_POST], NULL, 0, 0);

        /* rx gain emu */
        INIT_INI_ARRAY(&ahp->ah_iniRxGainEmu, ar9330Common_rx_gain_osprey2_0_emulation,
                       N(ar9330Common_rx_gain_osprey2_0_emulation), 2);
#endif
    }else if (AR_SREV_POSEIDON_11(ah)) {
        /* mac */
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_CORE],
            ar9485_poseidon1_1_mac_core, N( ar9485_poseidon1_1_mac_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_POST],
            ar9485_poseidon1_1_mac_postamble, 
            N(ar9485_poseidon1_1_mac_postamble), 5);

        /* bb */
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_PRE], 
            ar9485_poseidon1_1, N(ar9485_poseidon1_1), 2);
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_CORE],
            ar9485_poseidon1_1_baseband_core, 
            N(ar9485_poseidon1_1_baseband_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_POST],
            ar9485_poseidon1_1_baseband_postamble, 
            N(ar9485_poseidon1_1_baseband_postamble), 5);

        /* radio */
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_CORE], 
            ar9485_poseidon1_1_radio_core, N(ar9485_poseidon1_1_radio_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_POST],
            ar9485_poseidon1_1_radio_postamble, 
            N(ar9485_poseidon1_1_radio_postamble), 2);

        /* soc */
        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_PRE],
            ar9485_poseidon1_1_soc_preamble, 
            N(ar9485_poseidon1_1_soc_preamble), 2);

        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_CORE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_POST], NULL, 0, 0);

        /* rx/tx gain */
        INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, 
            ar9485_common_wo_xlna_rx_gain_poseidon1_1, 
            N(ar9485_common_wo_xlna_rx_gain_poseidon1_1), 2);
       	INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, 
            ar9485_modes_lowest_ob_db_tx_gain_poseidon1_1, 
            N(ar9485_modes_lowest_ob_db_tx_gain_poseidon1_1), 5);

        /* Japan 2484Mhz CCK settings */
        INIT_INI_ARRAY(&ahp->ah_iniJapan2484,
            ar9485_poseidon1_1_baseband_core_txfir_coeff_japan_2484,
            N(ar9485_poseidon1_1_baseband_core_txfir_coeff_japan_2484), 2);

        /* Load PCIE SERDES settings from INI */
        if (ahpriv->ah_config.ath_hal_pcieClockReq) {
            /* Pci-e Clock Request = 1 */
            if (ahpriv->ah_config.ath_hal_pllPwrSave 
                & AR_PCIE_PLL_PWRSAVE_CONTROL)
            {
                /* Sleep Setting */
                if (ahpriv->ah_config.ath_hal_pllPwrSave & 
                    AR_PCIE_PLL_PWRSAVE_ON_D3) 
                {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, 
                        ar9485_poseidon1_1_pcie_phy_clkreq_enable_L1,
                        N(ar9485_poseidon1_1_pcie_phy_clkreq_enable_L1), 2);
                } else {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, 
                        ar9485_poseidon1_1_pcie_phy_pll_on_clkreq_enable_L1,
                        N(ar9485_poseidon1_1_pcie_phy_pll_on_clkreq_enable_L1),
                        2);
                }    
                /* Awake Setting */
                if (ahpriv->ah_config.ath_hal_pllPwrSave & 
                    AR_PCIE_PLL_PWRSAVE_ON_D0)
                {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, 
                        ar9485_poseidon1_1_pcie_phy_clkreq_enable_L1,
                        N(ar9485_poseidon1_1_pcie_phy_clkreq_enable_L1), 2);
                } else {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, 
                        ar9485_poseidon1_1_pcie_phy_pll_on_clkreq_enable_L1,
                        N(ar9485_poseidon1_1_pcie_phy_pll_on_clkreq_enable_L1),
                        2);
                }    
                
            } else {
                /*Use driver default setting*/
                /* Sleep Setting */
                INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, 
                    ar9485_poseidon1_1_pcie_phy_clkreq_enable_L1,
                    N(ar9485_poseidon1_1_pcie_phy_clkreq_enable_L1), 2);
                /* Awake Setting */
                INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, 
                    ar9485_poseidon1_1_pcie_phy_clkreq_enable_L1,
                    N(ar9485_poseidon1_1_pcie_phy_clkreq_enable_L1), 2);
            }
        } else {
            /* Pci-e Clock Request = 0 */
            if (ahpriv->ah_config.ath_hal_pllPwrSave 
                & AR_PCIE_PLL_PWRSAVE_CONTROL)
            {
                /* Sleep Setting */
                if (ahpriv->ah_config.ath_hal_pllPwrSave & 
                    AR_PCIE_PLL_PWRSAVE_ON_D3) 
                {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, 
                        ar9485_poseidon1_1_pcie_phy_clkreq_disable_L1,
                        N(ar9485_poseidon1_1_pcie_phy_clkreq_disable_L1), 2);
                } else {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, 
                        ar9485_poseidon1_1_pcie_phy_pll_on_clkreq_disable_L1,
                        N(ar9485_poseidon1_1_pcie_phy_pll_on_clkreq_disable_L1),
                        2);
                }    
                /* Awake Setting */
                if (ahpriv->ah_config.ath_hal_pllPwrSave & 
                    AR_PCIE_PLL_PWRSAVE_ON_D0)
                {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, 
                        ar9485_poseidon1_1_pcie_phy_clkreq_disable_L1,
                        N(ar9485_poseidon1_1_pcie_phy_clkreq_disable_L1), 2);
                } else {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, 
                        ar9485_poseidon1_1_pcie_phy_pll_on_clkreq_disable_L1,
                        N(ar9485_poseidon1_1_pcie_phy_pll_on_clkreq_disable_L1),
                        2);
                }    
                
            } else {
                /*Use driver default setting*/
                /* Sleep Setting */
                INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, 
                    ar9485_poseidon1_1_pcie_phy_clkreq_disable_L1,
                    N(ar9485_poseidon1_1_pcie_phy_clkreq_disable_L1), 2);
                /* Awake Setting */
                INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, 
                    ar9485_poseidon1_1_pcie_phy_clkreq_disable_L1,
                    N(ar9485_poseidon1_1_pcie_phy_clkreq_disable_L1), 2);
            }
        }
        /*ath_hal_pciePowerSaveEnable should be 2 for OWL/Condor and 0 for merlin */
        /* pcie ps setting will honor registry setting, default is 0 */
        //ahpriv->ah_config.ath_hal_pciePowerSaveEnable = 0;    
   } else if (AR_SREV_POSEIDON(ah)) {
        /* mac */
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_CORE],
            ar9485_poseidon1_0_mac_core, N( ar9485_poseidon1_0_mac_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_POST],
            ar9485_poseidon1_0_mac_postamble, 
            N(ar9485_poseidon1_0_mac_postamble), 5);

        /* bb */
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_PRE], 
            ar9485_poseidon1_0, N(ar9485_poseidon1_0), 2);
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_CORE],
            ar9485_poseidon1_0_baseband_core, 
            N(ar9485_poseidon1_0_baseband_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_POST],
            ar9485_poseidon1_0_baseband_postamble, 
            N(ar9485_poseidon1_0_baseband_postamble), 5);

        /* radio */
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_CORE], 
            ar9485_poseidon1_0_radio_core, N(ar9485_poseidon1_0_radio_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_POST],
            ar9485_poseidon1_0_radio_postamble, 
            N(ar9485_poseidon1_0_radio_postamble), 2);

        /* soc */
        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_PRE],
            ar9485_poseidon1_0_soc_preamble, 
            N(ar9485_poseidon1_0_soc_preamble), 2);

        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_CORE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_POST], NULL, 0, 0);

        /* rx/tx gain */
        INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, 
            ar9485Common_wo_xlna_rx_gain_poseidon1_0, 
            N(ar9485Common_wo_xlna_rx_gain_poseidon1_0), 2);
       	INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, 
            ar9485Modes_lowest_ob_db_tx_gain_poseidon1_0, 
            N(ar9485Modes_lowest_ob_db_tx_gain_poseidon1_0), 5);

        /* Japan 2484Mhz CCK settings */
        INIT_INI_ARRAY(&ahp->ah_iniJapan2484,
            ar9485_poseidon1_0_baseband_core_txfir_coeff_japan_2484,
            N(ar9485_poseidon1_0_baseband_core_txfir_coeff_japan_2484), 2);

#ifndef AR9485_EMULATION
        /* Load PCIE SERDES settings from INI */
        if (ahpriv->ah_config.ath_hal_pcieClockReq) {
            /* Pci-e Clock Request = 1 */
            if (ahpriv->ah_config.ath_hal_pllPwrSave 
                & AR_PCIE_PLL_PWRSAVE_CONTROL)
            {
                /* Sleep Setting */
                if (ahpriv->ah_config.ath_hal_pllPwrSave & 
                    AR_PCIE_PLL_PWRSAVE_ON_D3) 
                {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, 
                        ar9485_poseidon1_0_pcie_phy_clkreq_enable_L1,
                        N(ar9485_poseidon1_0_pcie_phy_clkreq_enable_L1), 2);
                } else {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, 
                        ar9485_poseidon1_0_pcie_phy_pll_on_clkreq_enable_L1,
                        N(ar9485_poseidon1_0_pcie_phy_pll_on_clkreq_enable_L1),
                        2);
                }    
                /* Awake Setting */
                if (ahpriv->ah_config.ath_hal_pllPwrSave & 
                    AR_PCIE_PLL_PWRSAVE_ON_D0)
                {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, 
                        ar9485_poseidon1_0_pcie_phy_clkreq_enable_L1,
                        N(ar9485_poseidon1_0_pcie_phy_clkreq_enable_L1), 2);
                } else {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, 
                        ar9485_poseidon1_0_pcie_phy_pll_on_clkreq_enable_L1,
                        N(ar9485_poseidon1_0_pcie_phy_pll_on_clkreq_enable_L1),
                        2);
                }    
                
            } else {
                /*Use driver default setting*/
                /* Sleep Setting */
                INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, 
                    ar9485_poseidon1_0_pcie_phy_pll_on_clkreq_enable_L1,
                    N(ar9485_poseidon1_0_pcie_phy_pll_on_clkreq_enable_L1), 2);
                /* Awake Setting */
                INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, 
                    ar9485_poseidon1_0_pcie_phy_pll_on_clkreq_enable_L1,
                    N(ar9485_poseidon1_0_pcie_phy_pll_on_clkreq_enable_L1), 2);
            }
        } else {
            /* Pci-e Clock Request = 0 */
            if (ahpriv->ah_config.ath_hal_pllPwrSave 
                & AR_PCIE_PLL_PWRSAVE_CONTROL)
            {
                /* Sleep Setting */
                if (ahpriv->ah_config.ath_hal_pllPwrSave & 
                    AR_PCIE_PLL_PWRSAVE_ON_D3) 
                {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, 
                        ar9485_poseidon1_0_pcie_phy_clkreq_disable_L1,
                        N(ar9485_poseidon1_0_pcie_phy_clkreq_disable_L1), 2);
                } else {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, 
                        ar9485_poseidon1_0_pcie_phy_pll_on_clkreq_disable_L1,
                        N(ar9485_poseidon1_0_pcie_phy_pll_on_clkreq_disable_L1),
                        2);
                }    
                /* Awake Setting */
                if (ahpriv->ah_config.ath_hal_pllPwrSave & 
                    AR_PCIE_PLL_PWRSAVE_ON_D0)
                {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, 
                        ar9485_poseidon1_0_pcie_phy_clkreq_disable_L1,
                        N(ar9485_poseidon1_0_pcie_phy_clkreq_disable_L1), 2);
                } else {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, 
                        ar9485_poseidon1_0_pcie_phy_pll_on_clkreq_disable_L1,
                        N(ar9485_poseidon1_0_pcie_phy_pll_on_clkreq_disable_L1),
                        2);
                }    
                
            } else {
                /*Use driver default setting*/
                /* Sleep Setting */
                INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, 
                    ar9485_poseidon1_0_pcie_phy_pll_on_clkreq_disable_L1,
                    N(ar9485_poseidon1_0_pcie_phy_pll_on_clkreq_disable_L1), 2);
                /* Awake Setting */
                INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, 
                    ar9485_poseidon1_0_pcie_phy_pll_on_clkreq_disable_L1,
                    N(ar9485_poseidon1_0_pcie_phy_pll_on_clkreq_disable_L1), 2);
            }
        }
#endif
        /*ath_hal_pciePowerSaveEnable should be 2 for OWL/Condor and 0 for merlin */
        /* pcie ps setting will honor registry setting, default is 0 */
        //ahpriv->ah_config.ath_hal_pciePowerSaveEnable = 0;
    
#if 0 //ATH_WOW
        /* SerDes values during WOW sleep */
        INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesWow, ar9300PciePhy_AWOW,
                       N(ar9300PciePhy_AWOW), 2);
#endif

#ifdef AR9485_EMULATION
        /* mac emu */
        INIT_INI_ARRAY(&ahp->ah_iniMacEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniMacEmu[ATH_INI_CORE],
            ar9485_poseidon1_0_mac_core_emulation, 
            N(ar9485_poseidon1_0_mac_core_emulation), 2);
        INIT_INI_ARRAY(&ahp->ah_iniMacEmu[ATH_INI_POST], 
            ar9485_poseidon1_0_mac_postamble_emulation, 
            N(ar9485_poseidon1_0_mac_postamble_emulation), 5);

        /* bb emu */
        INIT_INI_ARRAY(&ahp->ah_iniBBEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniBBEmu[ATH_INI_CORE],
            ar9485_poseidon1_0_baseband_core_emulation, 
            N(ar9485_poseidon1_0_baseband_core_emulation), 2);
        INIT_INI_ARRAY(&ahp->ah_iniBBEmu[ATH_INI_POST], 
            ar9485_poseidon1_0_baseband_postamble_emulation, 
            N(ar9485_poseidon1_0_baseband_postamble_emulation), 5);

        /* radio emu */
        INIT_INI_ARRAY(&ahp->ah_iniRadioEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniRadioEmu[ATH_INI_CORE],
            ar9485_130nm_radio_emulation, N(ar9485_130nm_radio_emulation), 2);
        INIT_INI_ARRAY(&ahp->ah_iniRadioEmu[ATH_INI_POST], NULL, 0, 0);

        /* soc emu */
        INIT_INI_ARRAY(&ahp->ah_iniSOCEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniSOCEmu[ATH_INI_CORE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniSOCEmu[ATH_INI_POST], NULL, 0, 0);

        /* rx gain emu */
        INIT_INI_ARRAY(&ahp->ah_iniRxGainEmu, 
            ar9485Common_rx_gain_poseidon1_0_emulation,
            N(ar9485Common_rx_gain_poseidon1_0_emulation), 2);
#endif
    } else if (AR_SREV_WASP(ah)) {
        /* mac */
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_CORE],
                        ar9340_wasp_1p0_mac_core, N(ar9340_wasp_1p0_mac_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_POST],
                        ar9340_wasp_1p0_mac_postamble, N(ar9340_wasp_1p0_mac_postamble), 5);

        /* bb */
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_CORE],
                        ar9340_wasp_1p0_baseband_core, N(ar9340_wasp_1p0_baseband_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_POST],
                        ar9340_wasp_1p0_baseband_postamble, N(ar9340_wasp_1p0_baseband_postamble), 5);

        /* radio */
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_CORE],
                        ar9340_wasp_1p0_radio_core, N(ar9340_wasp_1p0_radio_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_POST],
                        ar9340_wasp_1p0_radio_postamble, N(ar9340_wasp_1p0_radio_postamble), 5);

        /* soc */
        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_PRE],
                        ar9340_wasp_1p0_soc_preamble, N(ar9340_wasp_1p0_soc_preamble), 2);
        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_CORE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_POST],
                        ar9340_wasp_1p0_soc_postamble, N(ar9340_wasp_1p0_soc_postamble), 5);

        /* rx/tx gain */
        INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, ar9340Common_wo_xlna_rx_gain_table_wasp_1p0,
                       N(ar9340Common_wo_xlna_rx_gain_table_wasp_1p0), 2);
        INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9340Modes_high_ob_db_tx_gain_table_wasp_1p0,
                       N(ar9340Modes_high_ob_db_tx_gain_table_wasp_1p0), 5);

        /*ath_hal_pciePowerSaveEnable should be 2 for OWL/Condor and 0 for merlin */
        ahpriv->ah_config.ath_hal_pciePowerSaveEnable = 0;

        /* Fast clock modal settings */
        INIT_INI_ARRAY(&ahp->ah_iniModesAdditional, ar9340Modes_fast_clock_wasp_1p0,
                       N(ar9340Modes_fast_clock_wasp_1p0), 3);

        /* Additional setttings for 40Mhz */
        INIT_INI_ARRAY(&ahp->ah_iniModesAdditional_40M, ar9340_wasp_1p0_radio_core_40M,
                       N(ar9340_wasp_1p0_radio_core_40M), 2);
        /* DFS */
        INIT_INI_ARRAY(&ahp->ah_ini_dfs,
            ar9340_wasp_1p0_baseband_postamble_dfs_channel,
            ARRAY_LENGTH(ar9340_wasp_1p0_baseband_postamble_dfs_channel), 3);

#ifdef AR9340_EMULATION
        /* mac emu */
        INIT_INI_ARRAY(&ahp->ah_iniMacEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniMacEmu[ATH_INI_CORE],
                        ar9340_wasp_1p0_mac_core_emulation, N(ar9340_wasp_1p0_mac_core_emulation), 2);
        INIT_INI_ARRAY(&ahp->ah_iniMacEmu[ATH_INI_POST],
                        ar9340_wasp_1p0_mac_postamble_emulation, N(ar9340_wasp_1p0_mac_postamble_emulation), 5);

        /* bb emu */
        INIT_INI_ARRAY(&ahp->ah_iniBBEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniBBEmu[ATH_INI_CORE],
                        ar9340_wasp_1p0_baseband_core_emulation, N(ar9340_wasp_1p0_baseband_core_emulation), 2);
        INIT_INI_ARRAY(&ahp->ah_iniBBEmu[ATH_INI_POST],
                        ar9340_wasp_1p0_mac_postamble_emulation, N(ar9340_wasp_1p0_mac_postamble_emulation), 5);

        /* radio emu */
        INIT_INI_ARRAY(&ahp->ah_iniRadioEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniRadioEmu[ATH_INI_CORE],
                        ar9340_merlin_2p0_radio_core, N(ar9340_merlin_2p0_radio_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniRadioEmu[ATH_INI_POST], NULL, 0, 0);

        /* soc emu */
        INIT_INI_ARRAY(&ahp->ah_iniSOCEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniSOCEmu[ATH_INI_CORE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniSOCEmu[ATH_INI_POST], NULL, 0, 0);

        /* rx gain emu */
        INIT_INI_ARRAY(&ahp->ah_iniRxGainEmu, ar9340Common_rx_gain_table_merlin_2p0,
                       N(ar9340Common_rx_gain_table_merlin_2p0), 2);
#endif
    } else if (AR_SREV_AR9580(ah)) {
        /* AR9580/Peacock -  new INI format (pre, core, post arrays per subsystem) */

        /* mac */
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_CORE],
                        ar9300_ar9580_1p0_mac_core, N(ar9300_ar9580_1p0_mac_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_POST],
                        ar9300_ar9580_1p0_mac_postamble, N(ar9300_ar9580_1p0_mac_postamble), 5);

        /* bb */
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_CORE],
                        ar9300_ar9580_1p0_baseband_core, N(ar9300_ar9580_1p0_baseband_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_POST],
                        ar9300_ar9580_1p0_baseband_postamble, N(ar9300_ar9580_1p0_baseband_postamble), 5);

        /* radio */
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_CORE],
                        ar9300_ar9580_1p0_radio_core, N(ar9300_ar9580_1p0_radio_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_POST],
                        ar9300_ar9580_1p0_radio_postamble, N(ar9300_ar9580_1p0_radio_postamble), 5);

        /* soc */
        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_PRE],
                        ar9300_ar9580_1p0_soc_preamble, N(ar9300_ar9580_1p0_soc_preamble), 2);
        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_CORE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_POST],
                        ar9300_ar9580_1p0_soc_postamble, N(ar9300_ar9580_1p0_soc_postamble), 5);

        /* rx/tx gain */
        INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, ar9300Common_rx_gain_table_ar9580_1p0,
                       N(ar9300Common_rx_gain_table_ar9580_1p0), 2);
        INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9300Modes_lowest_ob_db_tx_gain_table_ar9580_1p0,
                       N(ar9300Modes_lowest_ob_db_tx_gain_table_ar9580_1p0), 5);

        /* DFS */
        INIT_INI_ARRAY(&ahp->ah_ini_dfs,
            ar9300_ar9580_1p0_baseband_postamble_dfs_channel,
            ARRAY_LENGTH(ar9300_ar9580_1p0_baseband_postamble_dfs_channel), 3);

#ifndef AR9300_EMULATION
        /* Load PCIE SERDES settings from INI */

        /*D3 Setting */
        if  (ahpriv->ah_config.ath_hal_pcieClockReq) {
            if (ahpriv->ah_config.ath_hal_pllPwrSave & AR_PCIE_PLL_PWRSAVE_CONTROL) { //registry control
                if (ahpriv->ah_config.ath_hal_pllPwrSave & AR_PCIE_PLL_PWRSAVE_ON_D3) { //bit1, in to D3
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, ar9300PciePhy_clkreq_enable_L1_ar9580_1p0,
                               N(ar9300PciePhy_clkreq_enable_L1_ar9580_1p0), 2);
                } else {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, ar9300PciePhy_pll_on_clkreq_disable_L1_ar9580_1p0,
                               N(ar9300PciePhy_pll_on_clkreq_disable_L1_ar9580_1p0), 2);
                }
            } else {//no registry control, default is pll on
                INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, ar9300PciePhy_pll_on_clkreq_disable_L1_ar9580_1p0,
                               N(ar9300PciePhy_pll_on_clkreq_disable_L1_ar9580_1p0), 2);
            }
        } else {
            if (ahpriv->ah_config.ath_hal_pllPwrSave & AR_PCIE_PLL_PWRSAVE_CONTROL) { //registry control
                if (ahpriv->ah_config.ath_hal_pllPwrSave & AR_PCIE_PLL_PWRSAVE_ON_D3) { //bit1, in to D3
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, ar9300PciePhy_clkreq_disable_L1_ar9580_1p0,
                               N(ar9300PciePhy_clkreq_disable_L1_ar9580_1p0), 2);
                } else {
                     INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, ar9300PciePhy_pll_on_clkreq_disable_L1_ar9580_1p0,
                               N(ar9300PciePhy_pll_on_clkreq_disable_L1_ar9580_1p0), 2);
                }
            } else {//no registry control, default is pll on
                INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, ar9300PciePhy_pll_on_clkreq_disable_L1_ar9580_1p0,
                               N(ar9300PciePhy_pll_on_clkreq_disable_L1_ar9580_1p0), 2);
            }
        }

        /*D0 Setting */
        if  (ahpriv->ah_config.ath_hal_pcieClockReq) {
             if (ahpriv->ah_config.ath_hal_pllPwrSave & AR_PCIE_PLL_PWRSAVE_CONTROL) { //registry control
                if (ahpriv->ah_config.ath_hal_pllPwrSave & AR_PCIE_PLL_PWRSAVE_ON_D0){ //bit2, out of D3
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, ar9300PciePhy_clkreq_enable_L1_ar9580_1p0,
                           N(ar9300PciePhy_clkreq_enable_L1_ar9580_1p0), 2);
                } else {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, ar9300PciePhy_pll_on_clkreq_disable_L1_ar9580_1p0,
                           N(ar9300PciePhy_pll_on_clkreq_disable_L1_ar9580_1p0), 2);
                }
             } else { //no registry control, default is pll on
                 INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, ar9300PciePhy_pll_on_clkreq_disable_L1_ar9580_1p0,
                           N(ar9300PciePhy_pll_on_clkreq_disable_L1_ar9580_1p0), 2);
             }
        } else {
            if (ahpriv->ah_config.ath_hal_pllPwrSave & AR_PCIE_PLL_PWRSAVE_CONTROL) {//registry control
                if (ahpriv->ah_config.ath_hal_pllPwrSave & AR_PCIE_PLL_PWRSAVE_ON_D0){//bit2, out of D3
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, ar9300PciePhy_clkreq_disable_L1_ar9580_1p0,
                           N(ar9300PciePhy_clkreq_disable_L1_ar9580_1p0), 2);
                } else {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, ar9300PciePhy_pll_on_clkreq_disable_L1_ar9580_1p0,
                           N(ar9300PciePhy_pll_on_clkreq_disable_L1_ar9580_1p0), 2);
                }
            } else { //no registry control, default is pll on
                 INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, ar9300PciePhy_pll_on_clkreq_disable_L1_ar9580_1p0,
                           N(ar9300PciePhy_pll_on_clkreq_disable_L1_ar9580_1p0), 2);
            }
        }

#endif
        /*ath_hal_pciePowerSaveEnable should be 2 for OWL/Condor and 0 for merlin */
        ahpriv->ah_config.ath_hal_pciePowerSaveEnable = 0;

#if 0 //ATH_WOW
        /* SerDes values during WOW sleep */
        INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesWow, ar9300PciePhy_AWOW,
                       N(ar9300PciePhy_AWOW), 2);
#endif

        /* Fast clock modal settings */
        INIT_INI_ARRAY(&ahp->ah_iniModesAdditional, ar9300Modes_fast_clock_ar9580_1p0,
                       N(ar9300Modes_fast_clock_ar9580_1p0), 3);
        INIT_INI_ARRAY(&ahp->ah_iniJapan2484,
                 ar9300_ar9580_1p0_baseband_core_txfir_coeff_japan_2484,
                 N(ar9300_ar9580_1p0_baseband_core_txfir_coeff_japan_2484), 2);

#ifdef AR9300_EMULATION
        /* mac emu */
        INIT_INI_ARRAY(&ahp->ah_iniMacEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniMacEmu[ATH_INI_CORE],
                        ar9300_ar9580_1p0_mac_core_emulation, N(ar9300_ar9580_1p0_mac_core_emulation), 2);
        INIT_INI_ARRAY(&ahp->ah_iniMacEmu[ATH_INI_POST],
                        ar9300_ar9580_1p0_mac_postamble_emulation, N(ar9300_ar9580_1p0_mac_postamble_emulation), 5);

        /* bb emu */
        INIT_INI_ARRAY(&ahp->ah_iniBBEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniBBEmu[ATH_INI_CORE],
                        ar9300_ar9580_1p0_baseband_core_emulation, N(ar9300_ar9580_1p0_baseband_core_emulation), 2);
        INIT_INI_ARRAY(&ahp->ah_iniBBEmu[ATH_INI_POST],
                        ar9300_ar9580_1p0_baseband_postamble_emulation, N(ar9300_ar9580_1p0_baseband_postamble_emulation), 5);

        /* radio emu */
        INIT_INI_ARRAY(&ahp->ah_iniRadioEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniRadioEmu[ATH_INI_CORE],
                        ar9200_merlin_2p2_radio_core, N(ar9200_merlin_2p2_radio_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniRadioEmu[ATH_INI_POST], NULL, 0, 0);

        /* soc emu */
        INIT_INI_ARRAY(&ahp->ah_iniSOCEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniSOCEmu[ATH_INI_CORE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniSOCEmu[ATH_INI_POST], NULL, 0, 0);

        /* rx gain emu */
        INIT_INI_ARRAY(&ahp->ah_iniRxGainEmu, ar9300Common_rx_gain_table_merlin_2p2,
                       N(ar9300Common_rx_gain_table_merlin_2p2), 2);
#endif
    } else {
        /* Osprey 2.2 -  new INI format (pre, core, post arrays per subsystem) */

        /* mac */
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_CORE],
                        ar9300_osprey_2p2_mac_core, N(ar9300_osprey_2p2_mac_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniMac[ATH_INI_POST],
                        ar9300_osprey_2p2_mac_postamble, N(ar9300_osprey_2p2_mac_postamble), 5);

        /* bb */
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_CORE],
                        ar9300_osprey_2p2_baseband_core, N(ar9300_osprey_2p2_baseband_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniBB[ATH_INI_POST],
                        ar9300_osprey_2p2_baseband_postamble, N(ar9300_osprey_2p2_baseband_postamble), 5);

        /* radio */
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_CORE],
                        ar9300_osprey_2p2_radio_core, N(ar9300_osprey_2p2_radio_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniRadio[ATH_INI_POST],
                        ar9300_osprey_2p2_radio_postamble, N(ar9300_osprey_2p2_radio_postamble), 5);

        /* soc */
        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_PRE],
                        ar9300_osprey_2p2_soc_preamble, N(ar9300_osprey_2p2_soc_preamble), 2);
        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_CORE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniSOC[ATH_INI_POST],
                        ar9300_osprey_2p2_soc_postamble, N(ar9300_osprey_2p2_soc_postamble), 5);

        /* rx/tx gain */
        INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, ar9300Common_rx_gain_table_osprey_2p2,
                       N(ar9300Common_rx_gain_table_osprey_2p2), 2);
        INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9300Modes_lowest_ob_db_tx_gain_table_osprey_2p2,
                       N(ar9300Modes_lowest_ob_db_tx_gain_table_osprey_2p2), 5);

        /* DFS */
        INIT_INI_ARRAY(&ahp->ah_ini_dfs,
            ar9300_osprey_2p2_baseband_postamble_dfs_channel,
            ARRAY_LENGTH(ar9300_osprey_2p2_baseband_postamble_dfs_channel), 3);

#ifndef AR9300_EMULATION
        /* Load PCIE SERDES settings from INI */

        /*D3 Setting */
        if  (ahpriv->ah_config.ath_hal_pcieClockReq) {
            if (ahpriv->ah_config.ath_hal_pllPwrSave & AR_PCIE_PLL_PWRSAVE_CONTROL) { //registry control
                if (ahpriv->ah_config.ath_hal_pllPwrSave & AR_PCIE_PLL_PWRSAVE_ON_D3) { //bit1, in to D3
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, ar9300PciePhy_clkreq_enable_L1_osprey_2p2,
                               N(ar9300PciePhy_clkreq_enable_L1_osprey_2p2), 2);
                } else {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, ar9300PciePhy_pll_on_clkreq_disable_L1_osprey_2p2,
                               N(ar9300PciePhy_pll_on_clkreq_disable_L1_osprey_2p2), 2);
                }
             } else {
#ifndef ATH_BUS_PM
        //no registry control, default is pll on
                INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, ar9300PciePhy_pll_on_clkreq_disable_L1_osprey_2p2,
                               N(ar9300PciePhy_pll_on_clkreq_disable_L1_osprey_2p2), 2);
#else
        //no registry control, default is pll off
        INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, ar9300PciePhy_clkreq_disable_L1_osprey_2p2,
                           N(ar9300PciePhy_clkreq_disable_L1_osprey_2p2), 2);
#endif
            }
        } else {
            if (ahpriv->ah_config.ath_hal_pllPwrSave & AR_PCIE_PLL_PWRSAVE_CONTROL) { //registry control
                if (ahpriv->ah_config.ath_hal_pllPwrSave & AR_PCIE_PLL_PWRSAVE_ON_D3) { //bit1, in to D3
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, ar9300PciePhy_clkreq_disable_L1_osprey_2p2,
                               N(ar9300PciePhy_clkreq_disable_L1_osprey_2p2), 2);
                } else {
                     INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, ar9300PciePhy_pll_on_clkreq_disable_L1_osprey_2p2,
                               N(ar9300PciePhy_pll_on_clkreq_disable_L1_osprey_2p2), 2);
                }
             } else {
#ifndef ATH_BUS_PM
        //no registry control, default is pll on
                INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, ar9300PciePhy_pll_on_clkreq_disable_L1_osprey_2p2,
                               N(ar9300PciePhy_pll_on_clkreq_disable_L1_osprey_2p2), 2);
#else
        //no registry control, default is pll off
        INIT_INI_ARRAY(&ahp->ah_iniPcieSerdes, ar9300PciePhy_clkreq_disable_L1_osprey_2p2,
                           N(ar9300PciePhy_clkreq_disable_L1_osprey_2p2), 2);
#endif
            }
        }

        /*D0 Setting */
        if  (ahpriv->ah_config.ath_hal_pcieClockReq) {
             if (ahpriv->ah_config.ath_hal_pllPwrSave & AR_PCIE_PLL_PWRSAVE_CONTROL) { //registry control
                if (ahpriv->ah_config.ath_hal_pllPwrSave & AR_PCIE_PLL_PWRSAVE_ON_D0){ //bit2, out of D3
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, ar9300PciePhy_clkreq_enable_L1_osprey_2p2,
                           N(ar9300PciePhy_clkreq_enable_L1_osprey_2p2), 2);
                } else {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, ar9300PciePhy_pll_on_clkreq_disable_L1_osprey_2p2,
                           N(ar9300PciePhy_pll_on_clkreq_disable_L1_osprey_2p2), 2);
                }
             } else { //no registry control, default is pll on
                 INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, ar9300PciePhy_pll_on_clkreq_disable_L1_osprey_2p2,
                           N(ar9300PciePhy_pll_on_clkreq_disable_L1_osprey_2p2), 2);
             }
        } else {
            if (ahpriv->ah_config.ath_hal_pllPwrSave & AR_PCIE_PLL_PWRSAVE_CONTROL) {//registry control
                if (ahpriv->ah_config.ath_hal_pllPwrSave & AR_PCIE_PLL_PWRSAVE_ON_D0){//bit2, out of D3
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, ar9300PciePhy_clkreq_disable_L1_osprey_2p2,
                           N(ar9300PciePhy_clkreq_disable_L1_osprey_2p2), 2);
                } else {
                    INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, ar9300PciePhy_pll_on_clkreq_disable_L1_osprey_2p2,
                           N(ar9300PciePhy_pll_on_clkreq_disable_L1_osprey_2p2), 2);
                }
            } else { //no registry control, default is pll on
                 INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesLowPower, ar9300PciePhy_pll_on_clkreq_disable_L1_osprey_2p2,
                           N(ar9300PciePhy_pll_on_clkreq_disable_L1_osprey_2p2), 2);
            }
        }

#endif
        /*ath_hal_pciePowerSaveEnable should be 2 for OWL/Condor and 0 for merlin */
        ahpriv->ah_config.ath_hal_pciePowerSaveEnable = 0;

#ifdef ATH_BUS_PM
        /*Use HAL to config PCI powersave by writing into the SerDes Registers */
        ahpriv->ah_config.ath_hal_pcieSerDesWrite = 1;
#endif

#if 0 //ATH_WOW
        /* SerDes values during WOW sleep */
        INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesWow, ar9300PciePhy_AWOW,
                       N(ar9300PciePhy_AWOW), 2);
#endif

        /* Fast clock modal settings */
        INIT_INI_ARRAY(&ahp->ah_iniModesAdditional, ar9300Modes_fast_clock_osprey_2p2,
                       N(ar9300Modes_fast_clock_osprey_2p2), 3);
        INIT_INI_ARRAY(&ahp->ah_iniJapan2484,
                 ar9300_osprey_2p2_baseband_core_txfir_coeff_japan_2484,
                 N(ar9300_osprey_2p2_baseband_core_txfir_coeff_japan_2484), 2);

#ifdef AR9300_EMULATION
        /* mac emu */
        INIT_INI_ARRAY(&ahp->ah_iniMacEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniMacEmu[ATH_INI_CORE],
                        ar9300_osprey_2p2_mac_core_emulation, N(ar9300_osprey_2p2_mac_core_emulation), 2);
        INIT_INI_ARRAY(&ahp->ah_iniMacEmu[ATH_INI_POST],
                        ar9300_osprey_2p2_mac_postamble_emulation, N(ar9300_osprey_2p2_mac_postamble_emulation), 5);

        /* bb emu */
        INIT_INI_ARRAY(&ahp->ah_iniBBEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniBBEmu[ATH_INI_CORE],
                        ar9300_osprey_2p2_baseband_core_emulation, N(ar9300_osprey_2p2_baseband_core_emulation), 2);
        INIT_INI_ARRAY(&ahp->ah_iniBBEmu[ATH_INI_POST],
                        ar9300_osprey_2p2_baseband_postamble_emulation, N(ar9300_osprey_2p2_baseband_postamble_emulation), 5);

        /* radio emu */
        INIT_INI_ARRAY(&ahp->ah_iniRadioEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniRadioEmu[ATH_INI_CORE],
                        ar9200_merlin_2p2_radio_core, N(ar9200_merlin_2p2_radio_core), 2);
        INIT_INI_ARRAY(&ahp->ah_iniRadioEmu[ATH_INI_POST], NULL, 0, 0);

        /* soc emu */
        INIT_INI_ARRAY(&ahp->ah_iniSOCEmu[ATH_INI_PRE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniSOCEmu[ATH_INI_CORE], NULL, 0, 0);
        INIT_INI_ARRAY(&ahp->ah_iniSOCEmu[ATH_INI_POST], NULL, 0, 0);

        /* rx gain emu */
        INIT_INI_ARRAY(&ahp->ah_iniRxGainEmu, ar9300Common_rx_gain_table_merlin_2p2,
                       N(ar9300Common_rx_gain_table_merlin_2p2), 2);
#endif
    }

    if(AR_SREV_WASP(ah))
    {
#ifdef _WIN64
#define AR_SOC_RST_OTP_INTF  0xB80600B4ULL
#else
#define AR_SOC_RST_OTP_INTF  0xB80600B4
#endif
#define REG_READ(_reg)       *((volatile u_int32_t *)(_reg))

        ahp->ah_enterprise_mode = REG_READ(AR_SOC_RST_OTP_INTF);
        ath_hal_printf (ah, "Wasp Enterprise mode: 0x%08x\n", ahp->ah_enterprise_mode);
#undef REG_READ
#undef AR_SOC_RST_OTP_INTF
    } else {
        ahp->ah_enterprise_mode = OS_REG_READ(ah, AR_ENT_OTP);
    }


    if (ahpriv->ah_isPciExpress) {
        ar9300ConfigPciPowerSave(ah, 0, 0);
    } else {
        ar9300DisablePciePhy(ah);
    }

    ecode = ar9300HwAttach(ah);
    if (ecode != HAL_OK)
        goto bad;

    /* set gain table pointers according to values read from the eeprom */
    ar9300TxGainTableApply(ah);
    ar9300RxGainTableApply(ah);

    /* XXX Do we need this? */
#if 0
    /* rxgain table */
    if (AR_SREV_MERLIN_20(ah)) {
        if (ar9300EepromGet(ahp, EEP_MINOR_REV) >= AR9300_EEP_MINOR_VER_17) {
            u_int32_t rxgainType = ar9300EepromGet(ahp, EEP_RXGAIN_TYPE);

            if (rxgainType == AR9300_EEP_RXGAIN_13dB_BACKOFF) {
                INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, ar9280Modes_backoff_13db_rxgain_merlin2, 
                           N(ar9280Modes_backoff_13db_rxgain_merlin2), 6);
            } else if (rxgainType == AR9300_EEP_RXGAIN_23dB_BACKOFF){
                INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, ar9280Modes_backoff_23db_rxgain_merlin2, 
                           N(ar9280Modes_backoff_23db_rxgain_merlin2), 6);
            } else {
                INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, ar9280Modes_original_rxgain_merlin2, 
                           N(ar9280Modes_original_rxgain_merlin2), 6);
            }
        } else {
            INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, ar9280Modes_original_rxgain_merlin2, 
                           N(ar9280Modes_original_rxgain_merlin2), 6);
        }
    }
#endif


    /*
    **
    ** Got everything we need now to setup the capabilities.
    */

    if (!ar9300FillCapabilityInfo(ah)) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s:failed ar9300FillCapabilityInfo\n", __func__);
        ecode = HAL_EEREAD;
        goto bad;
    }
    ecode = ar9300InitMacAddr(ah);
    if (ecode != HAL_OK) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s: failed initializing mac address\n", __func__);
        goto bad;
    }

    /*
     * Initialize receive buffer size to MAC default
     */
    ahp->rxBufSize = HAL_RXBUFSIZE_DEFAULT;

#if ATH_WOW
#if 0
    /*
     * Needs to be removed once we stop using XB92 XXX
     * FIXME: Check with latest boards too - SriniK
     */
    ar9300WowSetGpioResetLow(ah);
#endif

    /*
     * Clear the Wow Status.
     */
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_PCIE_PM_CTRL), OS_REG_READ(ah, AR_HOSTIF_REG(ah, AR_PCIE_PM_CTRL)) |
        AR_PMCTRL_WOW_PME_CLR);
    OS_REG_WRITE(ah, AR_WOW_PATTERN_REG,
        AR_WOW_CLEAR_EVENTS(OS_REG_READ(ah, AR_WOW_PATTERN_REG)));
#endif

    /*
     * Set the curTrigLevel to a value that works all modes - 11a/b/g or 11n
     * with aggregation enabled or disabled.
     */
    ahpriv->ah_txTrigLevel = (AR_FTRIG_512B >> AR_FTRIG_S);

    if (AR_SREV_HORNET(ah)) {
        ahpriv->nf_2GHz.nominal = AR_PHY_CCA_NOM_VAL_HORNET_2GHZ;
        ahpriv->nf_2GHz.max     = AR_PHY_CCA_MAX_GOOD_VAL_OSPREY_2GHZ;
        ahpriv->nf_2GHz.min     = AR_PHY_CCA_MIN_GOOD_VAL_OSPREY_2GHZ;
        ahpriv->nf_5GHz.nominal = AR_PHY_CCA_NOM_VAL_OSPREY_5GHZ;
        ahpriv->nf_5GHz.max     = AR_PHY_CCA_MAX_GOOD_VAL_OSPREY_5GHZ;
        ahpriv->nf_5GHz.min     = AR_PHY_CCA_MIN_GOOD_VAL_OSPREY_5GHZ;
        ahpriv->nf_cw_int_delta = AR_PHY_CCA_CW_INT_DELTA;
    } else {
        ahpriv->nf_2GHz.nominal = AR_PHY_CCA_NOM_VAL_OSPREY_2GHZ;
        ahpriv->nf_2GHz.max     = AR_PHY_CCA_MAX_GOOD_VAL_OSPREY_2GHZ;
        ahpriv->nf_2GHz.min     = AR_PHY_CCA_MIN_GOOD_VAL_OSPREY_2GHZ;
        if (AR_SREV_AR9580(ah)) {
            ahpriv->nf_5GHz.nominal = AR_PHY_CCA_NOM_VAL_PEACOCK_5GHZ;
        } else {
            ahpriv->nf_5GHz.nominal = AR_PHY_CCA_NOM_VAL_OSPREY_5GHZ;
        }
        ahpriv->nf_5GHz.max     = AR_PHY_CCA_MAX_GOOD_VAL_OSPREY_5GHZ;
        ahpriv->nf_5GHz.min     = AR_PHY_CCA_MIN_GOOD_VAL_OSPREY_5GHZ;
        ahpriv->nf_cw_int_delta = AR_PHY_CCA_CW_INT_DELTA;
     }

    /* init BB Panic Watchdog timeout */
    if (AR_SREV_HORNET(ah)) {
        ahpriv->ah_bbPanicTimeoutMs = HAL_BB_PANIC_WD_TMO_HORNET;
    } else {
        ahpriv->ah_bbPanicTimeoutMs = HAL_BB_PANIC_WD_TMO;
    }

    return ah;

bad:
    if (ahp)
        ar9300Detach((struct ath_hal *) ahp);
    if (status)
        *status = ecode;
    return AH_NULL;
}

void
ar9300Detach(struct ath_hal *ah)
{
    HALASSERT(ah != AH_NULL);
    HALASSERT(ah->ah_magic == AR9300_MAGIC);


    /* Make sure that chip is awake before writing to it */
    ar9300SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE);

    ar9300HwDetach(ah);
    ar9300SetPowerMode(ah, HAL_PM_FULL_SLEEP, AH_TRUE);
    ath_hal_free(ah, ah);
}

extern int
ar9300Get11nHwPlatform(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);

    return ahp->ah_hwp;
}

struct ath_hal_9300 *
ar9300NewState(u_int16_t devid, HAL_ADAPTER_HANDLE osdev, HAL_SOFTC sc,
    HAL_BUS_TAG st, HAL_BUS_HANDLE sh, HAL_BUS_TYPE bustype,
    asf_amem_instance_handle amem_handle,
    struct hal_reg_parm *hal_conf_parm, HAL_STATUS *status)
{
    static const u_int8_t defbssidmask[IEEE80211_ADDR_LEN] =
        { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    struct ath_hal_9300 *ahp;
    struct ath_hal *ah;

    /* NB: memory is returned zero'd */
    ahp = amalloc_adv(
        amem_handle, sizeof(struct ath_hal_9300), adf_os_mem_zero_outline);
    if (ahp == AH_NULL) {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                 "%s: cannot allocate memory for state block\n",
                 __func__);
        *status = HAL_ENOMEM;
        return AH_NULL;
    }

    ah = &ahp->ah_priv.priv.h;
    /* set initial values */

    /* Attach Osprey structure as default hal structure */
    OS_MEMCPY(&ahp->ah_priv.priv, &ar9300hal, sizeof(ahp->ah_priv.priv));

    AH_PRIVATE(ah)->amem_handle = amem_handle;
    ah->ah_osdev = osdev;
    ah->ah_sc = sc;
    ah->ah_st = st;
    ah->ah_sh = sh;
    ah->ah_bustype = bustype;

    AH_PRIVATE(ah)->ah_devid = devid;
    AH_PRIVATE(ah)->ah_subvendorid = 0; /* XXX */

    AH_PRIVATE(ah)->ah_flags = 0;
   
    /*
    ** Initialize factory defaults in the private space
    */
    ath_hal_factory_defaults(AH_PRIVATE(ah), hal_conf_parm);

    if (!hal_conf_parm->calInFlash)
         AH_PRIVATE(ah)->ah_flags |= AH_USE_EEPROM;
   
#if 0
#ifndef WIN32
    if (ar9300EepDataInFlash(ah)) {
        ahp->ah_priv.priv.ah_eepromRead  = ar9300FlashRead;
        ahp->ah_priv.priv.ah_eepromDump  = AH_NULL;
#ifdef AH_SUPPORT_WRITE_EEPROM
        ahp->ah_priv.priv.ah_eepromWrite = ar9300FlashWrite;
#endif
    } else {
        ahp->ah_priv.priv.ah_eepromRead  = ar9300EepromReadWord;
#ifdef AH_SUPPORT_WRITE_EEPROM
        ahp->ah_priv.priv.ah_eepromWrite = ar9300EepromWrite;
#endif
    }
#else /* WIN32 */

    ahp->ah_priv.priv.ah_eepromRead  = ar9300EepromReadWord;
#ifdef AH_SUPPORT_WRITE_EEPROM
    ahp->ah_priv.priv.ah_eepromWrite = ar9300EepromWrite;
#endif

#endif /* WIN32 */
#endif

    AH_PRIVATE(ah)->ah_powerLimit = MAX_RATE_POWER;
    AH_PRIVATE(ah)->ah_tpScale = HAL_TP_SCALE_MAX;  /* no scaling */

    ahp->ah_atimWindow = 0;         /* [0..1000] */
    ahp->ah_diversityControl = AH_PRIVATE(ah)->ah_config.ath_hal_diversityControl;
    ahp->ah_antennaSwitchSwap = AH_PRIVATE(ah)->ah_config.ath_hal_antennaSwitchSwap;

    /*
     * Enable MIC handling.
     */
    ahp->ah_staId1Defaults = AR_STA_ID1_CRPT_MIC_ENABLE;
    ahp->ah_enable32kHzClock = DONT_USE_32KHZ;/* XXX */
    ahp->ah_slottime = (u_int) -1;
    ahp->ah_acktimeout = (u_int) -1;
    ahp->ah_ctstimeout = (u_int) -1;
    ahp->ah_globaltxtimeout = (u_int) -1;
    OS_MEMCPY(&ahp->ah_bssidmask, defbssidmask, IEEE80211_ADDR_LEN);

    /*
     * 11g-specific stuff
     */
    ahp->ah_gBeaconRate = 0;        /* adhoc beacon fixed rate */

    /* SM power mode: Attach time, disable any setting */
    ahp->ah_smPowerMode = HAL_SMPS_DEFAULT;

    return ahp;
}

HAL_BOOL
ar9300ChipTest(struct ath_hal *ah)
{
    //u_int32_t regAddr[2] = { AR_STA_ID0, AR_PHY_BASE+(8 << 2) };
    u_int32_t regAddr[2] = { AR_STA_ID0 };
    u_int32_t regHold[2];
    u_int32_t patternData[4] =
        { 0x55555555, 0xaaaaaaaa, 0x66666666, 0x99999999 };
    int i, j;

    /* Test PHY & MAC registers */
    for (i = 0; i < 1; i++) {
        u_int32_t addr = regAddr[i];
        u_int32_t wrData, rdData;

        regHold[i] = OS_REG_READ(ah, addr);
        for (j = 0; j < 0x100; j++) {
            wrData = (j << 16) | j;
            OS_REG_WRITE(ah, addr, wrData);
            rdData = OS_REG_READ(ah, addr);
            if (rdData != wrData) {
                HDPRINTF(ah, HAL_DBG_REG_IO,
                    "%s: address test failed addr: 0x%08x - wr:0x%08x != rd:0x%08x\n",
                    __func__, addr, wrData, rdData);
                return AH_FALSE;
            }
        }
        for (j = 0; j < 4; j++) {
            wrData = patternData[j];
            OS_REG_WRITE(ah, addr, wrData);
            rdData = OS_REG_READ(ah, addr);
            if (wrData != rdData) {
                HDPRINTF(ah, HAL_DBG_REG_IO,
                    "%s: address test failed addr: 0x%08x - wr:0x%08x != rd:0x%08x\n",
                    __func__, addr, wrData, rdData);
                return AH_FALSE;
            }
        }
        OS_REG_WRITE(ah, regAddr[i], regHold[i]);
    }
    OS_DELAY(100);
    return AH_TRUE;
}

/*
 * Store the channel edges for the requested operational mode
 */
HAL_BOOL
ar9300GetChannelEdges(struct ath_hal *ah,
    u_int16_t flags, u_int16_t *low, u_int16_t *high)
{
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    HAL_CAPABILITIES *pCap = &ahpriv->ah_caps;

    if (flags & CHANNEL_5GHZ) {
        *low = pCap->halLow5GhzChan;
        *high = pCap->halHigh5GhzChan;
        return AH_TRUE;
    }
    if ((flags & CHANNEL_2GHZ)) {
        *low = pCap->halLow2GhzChan;
        *high = pCap->halHigh2GhzChan;

        return AH_TRUE;
    }
    return AH_FALSE;
}

HAL_BOOL
ar9300RegulatoryDomainOverride(struct ath_hal *ah, u_int16_t regdmn)
{
    AH_PRIVATE(ah)->ah_currentRD = regdmn;
    return AH_TRUE;
}

/*
 * Fill all software cached or static hardware state information.
 * Return failure if capabilities are to come from EEPROM and
 * cannot be read.
 */
HAL_BOOL
ar9300FillCapabilityInfo(struct ath_hal *ah)
{
#define AR_KEYTABLE_SIZE    128
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    HAL_CAPABILITIES *pCap = &ahpriv->ah_caps;
    u_int16_t capField = 0, eeval;

    ahpriv->ah_devType = (u_int16_t)ar9300EepromGet(ahp, EEP_DEV_TYPE);
    eeval = ar9300EepromGet(ahp, EEP_REG_0);

    /* XXX record serial number */
    AH_PRIVATE(ah)->ah_currentRD = eeval;

    pCap->halintr_mitigation = AH_TRUE;
    eeval = ar9300EepromGet(ahp, EEP_REG_1);
    AH_PRIVATE(ah)->ah_currentRDExt = eeval | AR9300_RDEXT_DEFAULT;

    /* Read the capability EEPROM location */
    capField = ar9300EepromGet(ahp, EEP_OP_CAP);

    /* Modify reg domain on newer cards that need to work with older sw */
    if (ahpriv->ah_opmode != HAL_M_HOSTAP &&
        ahpriv->ah_subvendorid == AR_SUBVENDOR_ID_NEW_A) {
        if (ahpriv->ah_currentRD == 0x64 ||
            ahpriv->ah_currentRD == 0x65)
            ahpriv->ah_currentRD += 5;
        else if (ahpriv->ah_currentRD == 0x41)
            ahpriv->ah_currentRD = 0x43;
        HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: regdomain mapped to 0x%x\n",
            __func__, ahpriv->ah_currentRD);
    }

    /* Construct wireless mode from EEPROM */
    pCap->halWirelessModes = 0;
    eeval = ar9300EepromGet(ahp, EEP_OP_MODE);

    if (eeval & AR9300_OPFLAGS_11A) {
        pCap->halWirelessModes |= HAL_MODE_11A |
         ((!ahpriv->ah_config.ath_hal_htEnable || (eeval & AR9300_OPFLAGS_N_5G_HT20)) ?
           0 : (HAL_MODE_11NA_HT20 | ((eeval & AR9300_OPFLAGS_N_5G_HT40) ?
           0 : (HAL_MODE_11NA_HT40PLUS | HAL_MODE_11NA_HT40MINUS))));
    }
    if (eeval & AR9300_OPFLAGS_11G) {
        pCap->halWirelessModes |= HAL_MODE_11B | HAL_MODE_11G |
        ((!ahpriv->ah_config.ath_hal_htEnable || (eeval & AR9300_OPFLAGS_N_2G_HT20)) ?
          0 : (HAL_MODE_11NG_HT20 | ((eeval & AR9300_OPFLAGS_N_2G_HT40) ?
          0 : (HAL_MODE_11NG_HT40PLUS | HAL_MODE_11NG_HT40MINUS))));
    }

    /* Get chainamsks from eeprom */
    pCap->halTxChainMask = ar9300EepromGet(ahp, EEP_TX_MASK);
    pCap->halRxChainMask = ar9300EepromGet(ahp, EEP_RX_MASK);

    /*
     * This being a newer chip supports TKIP non-splitmic mode.
     *
     */
    ahp->ah_miscMode |= AR_PCU_MIC_NEW_LOC_ENA;


    pCap->halLow2GhzChan = 2312;
    pCap->halHigh2GhzChan = 2732;

    pCap->halLow5GhzChan = 4920;
    pCap->halHigh5GhzChan = 6100;

    pCap->halCipherCkipSupport = AH_FALSE;
    pCap->halCipherTkipSupport = AH_TRUE;
    pCap->halCipherAesCcmSupport = AH_TRUE;

    pCap->halMicCkipSupport    = AH_FALSE;
    pCap->halMicTkipSupport    = AH_TRUE;
    pCap->halMicAesCcmSupport  = AH_TRUE;

    pCap->halChanSpreadSupport = AH_TRUE;
    pCap->halSleepAfterBeaconBroken = AH_TRUE;

    pCap->halCompressSupport   = AH_FALSE;
    pCap->halBurstSupport = AH_TRUE;
    pCap->halChapTuningSupport = AH_TRUE;
    pCap->halTurboPrimeSupport = AH_TRUE;
    pCap->halFastFramesSupport = AH_FALSE;

    pCap->halTurboGSupport = pCap->halWirelessModes & HAL_MODE_108G;

    pCap->halXrSupport = AH_FALSE;

    pCap->halHTSupport = ahpriv->ah_config.ath_hal_htEnable ? AH_TRUE : AH_FALSE;
    pCap->halGTTSupport = AH_TRUE;
    pCap->halPSPollBroken = AH_TRUE;    /* XXX fixed in later revs? */
    pCap->halHT20SGISupport = AH_TRUE;
    pCap->halVEOLSupport = AH_TRUE;
    pCap->halBssIdMaskSupport = AH_TRUE;
    pCap->halMcastKeySrchSupport = AH_TRUE;    /* Bug 26802, fixed in later revs? */
    pCap->halTsfAddSupport = AH_TRUE;

    if (capField & AR_EEPROM_EEPCAP_MAXQCU)
        pCap->halTotalQueues = MS(capField, AR_EEPROM_EEPCAP_MAXQCU);
    else
        pCap->halTotalQueues = HAL_NUM_TX_QUEUES;

    if (capField & AR_EEPROM_EEPCAP_KC_ENTRIES)
        pCap->halKeyCacheSize =
            1 << MS(capField, AR_EEPROM_EEPCAP_KC_ENTRIES);
    else
        pCap->halKeyCacheSize = AR_KEYTABLE_SIZE;

    pCap->halFastCCSupport = AH_TRUE;
    pCap->halNumMRRetries   = 4;
    pCap->halTxTrigLevelMax = MAX_TX_FIFO_THRESHOLD;

    pCap->halNumGpioPins = AR93XX_NUM_GPIO;

#if 0
    /* XXX Verify support in Osprey */
    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        pCap->halWowSupport = AH_TRUE;
        pCap->halWowMatchPatternExact = AH_TRUE;
        if (AR_SREV_MERLIN(ah)) {
            pCap->halWowPatternMatchDword = AH_TRUE;
        }
    } else {
        pCap->halWowSupport = AH_FALSE;
        pCap->halWowMatchPatternExact = AH_FALSE;
    }
#endif
    pCap->halWowSupport = AH_TRUE;
    if (AR_SREV_POSEIDON(ah)) {
        pCap->halWowMatchPatternExact = AH_TRUE;
    }
    pCap->halCSTSupport = AH_TRUE;

    pCap->halRifsRxSupport = AH_TRUE;
    pCap->halRifsTxSupport = AH_TRUE;

    pCap->halRtsAggrLimit = IEEE80211_AMPDU_LIMIT_MAX;

    pCap->halMfpSupport = ahpriv->ah_config.ath_hal_mfpSupport;

    pCap->halforcePpmSupport = AH_TRUE;
    pCap->halEnhancedPmSupport = AH_TRUE;
    
    /* ar9300 - has the HW UAPSD trigger support,
     * but it has the following limitations
     * The power state change from the following
     * frames are not put in High priority queue.
     *     i) Mgmt frames
     *     ii) NoN QoS frames
     *     iii) QoS frames form the access categories for which UAPSD is not enabled.
     * so we can not enable this feature currently.
     * could be enabled, if these limitations are fixed in later versions of ar9300 chips
     */
    pCap->halHwUapsdTrig = AH_FALSE;

    /* Number of buffers that can be help in a single TxD */
    pCap->halNumTxMaps = 4;

    pCap->halTxDescLen = sizeof(struct ar9300_txc);
    pCap->halTxStatusLen = sizeof(struct ar9300_txs);
    pCap->halRxStatusLen = sizeof(struct ar9300_rxs);

    pCap->halRxHPdepth = HAL_HP_RXFIFO_DEPTH;
    pCap->halRxLPdepth = HAL_LP_RXFIFO_DEPTH;

    /* Enable extension channel DFS support */
    pCap->halUseCombinedRadarRssi = AH_TRUE;
    pCap->halExtChanDfsSupport = AH_TRUE;
#if ATH_SUPPORT_SPECTRAL
       pCap->halSpectralScan = AH_TRUE;
#endif

    ahpriv->ah_rfsilent = ar9300EepromGet(ahp, EEP_RF_SILENT);
    if (ahpriv->ah_rfsilent & EEP_RFSILENT_ENABLED) {
        ahp->ah_gpioSelect = MS(ahpriv->ah_rfsilent, EEP_RFSILENT_GPIO_SEL);
        ahp->ah_polarity   = MS(ahpriv->ah_rfsilent, EEP_RFSILENT_POLARITY);

        ath_hal_enable_rfkill(ah, AH_TRUE);
        pCap->halRfSilentSupport = AH_TRUE;
    }

    /* XXX */
    pCap->halWpsPushButton = AH_FALSE;

#ifdef ATH_BT_COEX
    pCap->halBtCoexSupport = AH_TRUE;
    pCap->halBtCoexASPMWAR = AH_FALSE;
#endif

    pCap->halGenTimerSupport = AH_TRUE;
    ahp->ah_availGenTimers = ~((1 << AR_FIRST_NDP_TIMER) - 1);
    ahp->ah_availGenTimers &= (1 << AR_NUM_GEN_TIMERS) - 1;

    pCap->halAutoSleepSupport = AH_TRUE;

    pCap->halMbssidAggrSupport = AH_TRUE;
    pCap->halProxySTASupport = AH_TRUE;

    /* XXX Mark it AH_TRUE after it is verfied as fixed */
    pCap->hal4kbSplitTransSupport = AH_FALSE;

    /* Read regulatory domain flag */
    if (AH_PRIVATE(ah)->ah_currentRDExt & (1 << REG_EXT_JAPAN_MIDBAND)) {
        /*
         * If REG_EXT_JAPAN_MIDBAND is set, turn on U1 EVEN, U2, and MIDBAND.
         */
        pCap->halRegCap = AR_EEPROM_EEREGCAP_EN_KK_NEW_11A | AR_EEPROM_EEREGCAP_EN_KK_U1_EVEN |
                          AR_EEPROM_EEREGCAP_EN_KK_U2 | AR_EEPROM_EEREGCAP_EN_KK_MIDBAND;
    }
    else {
        pCap->halRegCap = AR_EEPROM_EEREGCAP_EN_KK_NEW_11A | AR_EEPROM_EEREGCAP_EN_KK_U1_EVEN;
    }

    /* For AR9300 and above, midband channels are always supported */
    pCap->halRegCap |= AR_EEPROM_EEREGCAP_EN_FCC_MIDBAND;

    pCap->halNumAntCfg5GHz = ar9300EepromGetNumAntConfig(ahp, HAL_FREQ_BAND_5GHZ);
    pCap->halNumAntCfg2GHz = ar9300EepromGetNumAntConfig(ahp, HAL_FREQ_BAND_2GHZ);

    /* STBC supported */
    pCap->halRxStbcSupport = 1;  /* number of streams for STBC recieve. */
    if(AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah)) {
        pCap->halTxStbcSupport = 0;
    } else {
        pCap->halTxStbcSupport = 1;        
    }

    pCap->halEnhancedDmaSupport = AH_TRUE;
#ifdef ATH_SUPPORT_DFS
    pCap->hal_enhanced_dfs_support = AH_TRUE;
#endif

    /*
     *  EV61133 (missing interrupts due to AR_ISR_RAC).
     *  Fixed in Osprey 2.0.
     */
    pCap->halIsrRacSupport = AH_TRUE;

    pCap->halWepTkipAggrSupport = AH_TRUE;
    pCap->halWepTkipAggrNumTxDelim = 10;    /* TBD */
    pCap->halWepTkipAggrNumRxDelim = 10;    /* TBD */
    pCap->halWepTkipMaxHtRate = 15;         /* TBD */
    pCap->halCfendFixSupport = AH_FALSE;
    pCap->halAggrExtraDelimWar = AH_FALSE;
    pCap->halRxDescTimestampBits = 32;
    pCap->halRxTxAbortSupport = AH_TRUE;
    pCap->halAniPollInterval = AR9300_ANI_POLLINTERVAL;
    pCap->halChannelSwitchTimeUsec = AR9300_CHANNEL_SWITCH_TIME_USEC;
    
#if ATH_SUPPORT_RAW_ADC_CAPTURE
    pCap->halRawADCCapture = AH_TRUE;
#endif
  
    /* Transmit Beamforming supported, fill capabilities */
#ifdef  ATH_SUPPORT_TxBF 
    pCap->halTxBFSupport = AH_TRUE;
    ar9300FillTxBFCapabilities(ah);
#endif
    pCap->halPaprdEnabled = ar9300EepromGet(ahp, EEP_PAPRD_ENABLED);
    pCap->halChanHalfRate =
                 !(ahp->ah_enterprise_mode & AR_ENT_OTP_10MHZ_DISABLE);
    pCap->halChanQuarterRate =
                 !(ahp->ah_enterprise_mode & AR_ENT_OTP_5MHZ_DISABLE);
    pCap->hal49Ghz = !(ahp->ah_enterprise_mode & AR_ENT_OTP_49GHZ_DISABLE);

    if (AR_SREV_POSEIDON(ah) || AR_SREV_HORNET(ah)) {
        /* LDPC supported */
        /* Poseidon doesn't support LDPC, or it will cause receiver CRC Error */
        pCap->halLdpcSupport = AH_FALSE;
        /* PCI_E LCR offset */
        if (AR_SREV_POSEIDON(ah)) {
            pCap->halPcieLcrOffset = 0x80; /*for Poseidon*/
        }
        /*WAR method for APSM L0s with Poseidon 1.0*/
        if (AR_SREV_POSEIDON_10(ah)) {
            pCap->halPcieLcrExtsyncEn = AH_TRUE;
        }
    } else {
        pCap->halLdpcSupport = AH_TRUE;
    }

    pCap->halEnableApm  = ar9300EepromGet(ahp, EEP_CHAIN_MASK_REDUCE);
    
    if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON_11(ah)) {
        if (ahp->ah_diversityControl == HAL_ANT_VARIABLE) {
            u_int8_t ant_div_control1 = ar9300EepromGet(ahp, EEP_ANTDIV_control);
            /* if enable_lnadiv is 0x1 and enable_fast_div is 0x1, 
             * we enable the diversity-combining algorithm. 
             */
            if ((ant_div_control1 >> 0x6) == 0x3) {
                pCap->halAntDivCombSupport = AH_TRUE;
            }
        }
    }

#if ATH_SUPPORT_WAPI
    /*
     * WAPI engine support 2 stream rates at most currently
     */
    pCap->hal_wapi_max_tx_chains = 2;
    pCap->hal_wapi_max_rx_chains = 2;
#endif

    return AH_TRUE;
#undef AR_KEYTABLE_SIZE
}

HAL_STATUS
ar9300RadioAttach(struct ath_hal *ah)
{
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "%s: Need analog access recipe!!\n", __func__);
#if 0
    u_int32_t val;

    /*
     * Set correct Baseband to analog shift
     * setting to access analog chips.
     */
    OS_REG_WRITE(ah, AR_PHY(0), 0x00000007);

    val = ar9300GetRadioRev(ah);
    switch (val & AR_RADIO_SREV_MAJOR) {
        case 0:
            /* TODO:
             * WAR for bug 10062.  When RF_Silent is used, the
             * analog chip is reset.  So when the system boots
             * up with the radio switch off we cannot determine
             * the RF chip rev.  To workaround this check the mac/
             * phy revs and set radio rev.
             */
            val = AR_RAD5133_SREV_MAJOR;
            break;
        case AR_RAD5133_SREV_MAJOR:
        case AR_RAD5122_SREV_MAJOR:
        case AR_RAD2133_SREV_MAJOR:
        case AR_RAD2122_SREV_MAJOR:
            break;
        default:
#ifdef AH_DEBUG
            HDPRINTF(ah, HAL_DBG_CHANNEL,
                "%s: 5G Radio Chip Rev 0x%02X is not supported by this driver\n",
                __func__,AH_PRIVATE(ah)->ah_analog5GhzRev);
#endif
            return HAL_ENOTSUPP;
    }

    AH_PRIVATE(ah)->ah_analog5GhzRev = val;

#endif
    return HAL_OK;
}

static inline void
ar9300AniSetup(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    int i;

    const int totalSizeDesired[] = { -55, -55, -55, -55, -62 };
    const int coarseHigh[]       = { -14, -14, -14, -14, -12 };
    const int coarseLow[]        = { -64, -64, -64, -64, -70 };
    const int firpwr[]           = { -78, -78, -78, -78, -80 };

    for (i = 0; i < 5; i++) {
        ahp->ah_totalSizeDesired[i] = totalSizeDesired[i];
        ahp->ah_coarseHigh[i] = coarseHigh[i];
        ahp->ah_coarseLow[i] = coarseLow[i];
        ahp->ah_firpwr[i] = firpwr[i];
    }
}

static HAL_BOOL
ar9300GetChipPowerLimits(struct ath_hal *ah, HAL_CHANNEL *chans, u_int32_t nchans)
{
    struct ath_hal_9300 *ahp = AH9300(ah);

    return ahp->ah_rfHal.getChipPowerLim(ah, chans, nchans);
}

/*
 * Disable PLL when in L0s as well as receiver clock when in L1.
 * This power saving option must be enabled through the Serdes.
 *
 * Programming the Serdes must go through the same 288 bit serial shift
 * register as the other analog registers.  Hence the 9 writes.
 *
 * XXX Clean up the magic numbers.
 */
void
ar9300ConfigPciPowerSave(struct ath_hal *ah, int restore, int powerOff)
{
#ifndef AR9340_EMULATION
    struct ath_hal_9300 *ahp = AH9300(ah);
    int i;
#endif

    if (AH_PRIVATE(ah)->ah_isPciExpress != AH_TRUE) {
        return;
    }

    /* Do not touch SERDES registers */
    if (AH_PRIVATE(ah)->ah_config.ath_hal_pciePowerSaveEnable == 2) {
        return;
    }

    /* Nothing to do on restore for 11N */
    if (!restore) {
        /* set bit 19 to allow forcing of pcie core into L1 state */
        OS_REG_SET_BIT(ah, AR_HOSTIF_REG(ah, AR_PCIE_PM_CTRL), AR_PCIE_PM_CTRL_ENA);

#ifndef AR9340_EMULATION
        /*
         * Set PCIE workaround config only if requested, else use the reset
         * value of this register.
         */
        if (AH_PRIVATE(ah)->ah_config.ath_hal_pcieWaen) {
            OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_WA), AH_PRIVATE(ah)->ah_config.ath_hal_pcieWaen);
        } else {
            // Set Bits 17 and 14 in the AR_WA register.
            OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_WA), ahp->ah_WARegVal);
        }
    }

    /* Configure PCIE after Ini init. SERDES values now come from ini file */
    if (AH_PRIVATE(ah)->ah_config.ath_hal_pcieSerDesWrite) {
        if (powerOff) {
            for (i = 0; i < ahp->ah_iniPcieSerdes.ia_rows; i++) {
                OS_REG_WRITE(ah, INI_RA(&ahp->ah_iniPcieSerdes, i, 0), INI_RA(&ahp->ah_iniPcieSerdes, i, 1));
            }
        } else {
            for (i = 0; i < ahp->ah_iniPcieSerdesLowPower.ia_rows; i++) {
                OS_REG_WRITE(ah, INI_RA(&ahp->ah_iniPcieSerdesLowPower, i, 0), INI_RA(&ahp->ah_iniPcieSerdesLowPower, i, 1));
            }
        }
#endif
    }

}

/*
 * Recipe from charles to turn off PCIe PHY in PCI mode for power savings
 */
void
ar9300DisablePciePhy(struct ath_hal *ah)
{
    /* Osprey does not support PCI mode */
}

static inline int
ar9300GetRadioRev(struct ath_hal *ah)
{
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "%s: Need analog access recipe!!\n", __func__);
    return 0xdeadbeef;
#if 0
    u_int32_t val;
    int i;

    /* Read Radio Chip Rev Extract */
    OS_REG_WRITE(ah, AR_PHY(0x36), 0x00007058);
    for (i = 0; i < 8; i++)
        OS_REG_WRITE(ah, AR_PHY(0x20), 0x00010000);
    val = (OS_REG_READ(ah, AR_PHY(256)) >> 24) & 0xff;
    val = ((val & 0xf0) >> 4) | ((val & 0x0f) << 4);
    return ath_hal_reverseBits(val, 8);
#endif
}

static inline HAL_STATUS
ar9300InitMacAddr(struct ath_hal *ah)
{
    u_int32_t sum;
    int i;
    u_int16_t eeval;
    struct ath_hal_9300 *ahp = AH9300(ah);
    u_int32_t EEP_MAC [] = { EEP_MAC_LSW, EEP_MAC_MID, EEP_MAC_MSW };

    sum = 0;
    for (i = 0; i < 3; i++) {
        eeval = ar9300EepromGet(ahp, EEP_MAC[i]);
        sum += eeval;
        ahp->ah_macaddr[2*i] = eeval >> 8;
        ahp->ah_macaddr[2*i + 1] = eeval & 0xff;
    }
    if (sum == 0 || sum == 0xffff*3) {
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: mac address read failed: %s\n",
            __func__, ath_hal_ether_sprintf(ahp->ah_macaddr));
        return HAL_EEBADMAC;
    }

    return HAL_OK;
}

/*
 * Code for the "real" chip i.e. non-emulation. Review and revisit
 * when actual hardware is at hand.
 */
static inline HAL_STATUS
ar9300HwAttach(struct ath_hal *ah)
{
    HAL_STATUS ecode;

    if (!ar9300ChipTest(ah)) {
        HDPRINTF(ah, HAL_DBG_REG_IO, "%s: hardware self-test failed\n", __func__);
        return HAL_ESELFTEST;
    }

    ecode = ar9300RadioAttach(ah);
    if (ecode != HAL_OK)
        return ecode;

    ecode = ar9300EepromAttach(ah);
    if (ecode != HAL_OK)
        return ecode;
#ifdef ATH_CCX
    ar9300RecordSerialNumber(ah);
#endif
    if (!ar9300RfAttach(ah, &ecode)) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s: RF setup failed, status %u\n",
            __func__, ecode);
    }

    if (ecode != HAL_OK)
        return ecode;

#ifndef AR9300_EMULATION
    /* Enabling ANI */
    ar9300AniSetup(ah); /* setup 9300-specific ANI tables */
    ar9300AniAttach(ah);
#endif

    return HAL_OK;
}

static inline void
ar9300HwDetach(struct ath_hal *ah)
{
#ifndef AR9300_EMULATION
    /* XXX EEPROM allocated state */
    ar9300AniDetach(ah);
#endif
}

static int16_t
ar9300GetNfAdjust(struct ath_hal *ah, const HAL_CHANNEL_INTERNAL *c)
{
    return 0;
}

#ifdef ATH_CCX
static HAL_BOOL
ar9300RecordSerialNumber(struct ath_hal *ah)
{
    int      i;
    struct ath_hal_9300 *ahp = AH9300(ah);
    u_int8_t    *sn = (u_int8_t*)ahp->ah_priv.priv.serNo;
    u_int8_t    *data = ar9300EepromGetCustData(ahp);
    for (i = 0; i < AR_EEPROM_SERIAL_NUM_SIZE; i++) {
        sn[i] = data[i];
    }

    sn[AR_EEPROM_SERIAL_NUM_SIZE] = '\0';

    return AH_TRUE;
}
#endif

void
ar9300SetImmunity(struct ath_hal *ah, HAL_BOOL enable)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    u_int32_t m1ThreshLow = enable ? 127 : ahp->ah_immunity_vals[0],
              m2ThreshLow = enable ? 127 : ahp->ah_immunity_vals[1],
              m1Thresh = enable ? 127 : ahp->ah_immunity_vals[2],
              m2Thresh = enable ? 127 : ahp->ah_immunity_vals[3],
              m2CountThr = enable ? 31 : ahp->ah_immunity_vals[4],
              m2CountThrLow = enable ? 63 : ahp->ah_immunity_vals[5];

    if (ahp->ah_immunity_on == enable) {
        return;
    }

    ahp->ah_immunity_on = enable;

    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_LOW,
                     AR_PHY_SFCORR_LOW_M1_THRESH_LOW, m1ThreshLow);
    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_LOW,
                     AR_PHY_SFCORR_LOW_M2_THRESH_LOW, m2ThreshLow);
    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR,
                     AR_PHY_SFCORR_M1_THRESH, m1Thresh);
    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR,
                     AR_PHY_SFCORR_M2_THRESH, m2Thresh);
    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR,
                     AR_PHY_SFCORR_M2COUNT_THR, m2CountThr);
    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_LOW,
                     AR_PHY_SFCORR_LOW_M2COUNT_THR_LOW, m2CountThrLow);

    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_EXT,
                     AR_PHY_SFCORR_EXT_M1_THRESH_LOW, m1ThreshLow);
    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_EXT,
                     AR_PHY_SFCORR_EXT_M2_THRESH_LOW, m2ThreshLow);
    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_EXT,
                     AR_PHY_SFCORR_EXT_M1_THRESH, m1Thresh);
    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_EXT,
                     AR_PHY_SFCORR_EXT_M2_THRESH, m2Thresh);

    if (!enable) {
        OS_REG_SET_BIT(ah, AR_PHY_SFCORR_LOW,
                       AR_PHY_SFCORR_LOW_USE_SELF_CORR_LOW);
    } else {
        OS_REG_CLR_BIT(ah, AR_PHY_SFCORR_LOW,
                       AR_PHY_SFCORR_LOW_USE_SELF_CORR_LOW);
    }
}

int
ar9300GetCalIntervals(struct ath_hal *ah, HAL_CALIBRATION_TIMER **timerp, HAL_CAL_QUERY query)
{
#define AR9300_IS_CHAIN_RX_IQCAL_INVALID(_ah, _reg) ((OS_REG_READ((_ah), _reg) & 0x3fff) == 0)
#define AR9300_IS_RX_IQCAL_DISABLED(_ah) (!(OS_REG_READ((_ah), AR_PHY_RX_IQCAL_CORR_B0) & AR_PHY_RX_IQCAL_CORR_IQCORR_ENABLE))
#ifndef AR9300_EMULATION_BB /* To avoid comilation warnings. Variables are not used when EMULATION. */
	struct ath_hal_9300 *ahp = AH9300(ah);
	u_int8_t rxchainmask = ahp->ah_rxchainmask, i;
	int rx_iqcal_invalid = 0, numChains = 0;
	static const u_int32_t offset_array[3] = {
                                              AR_PHY_RX_IQCAL_CORR_B0,
		                                      AR_PHY_RX_IQCAL_CORR_B1,
		                                      AR_PHY_RX_IQCAL_CORR_B2,
								             };
#endif

	*timerp = ar9300_cals;

	switch (query) {
	case HAL_QUERY_CALS:
#ifdef AR9300_EMULATION_BB
    /* cals not supported by emulation */
        return 0;
#else
        return AR9300_NUM_CAL_TYPES;
#endif
	case HAL_QUERY_RERUN_CALS:
#ifdef AR9300_EMULATION_BB
    /* cals not supported by emulation */
        return 0;
#else
        for (i = 0; i < AR9300_MAX_CHAINS; i++) {
            if (rxchainmask & (1 << i)) {
                numChains++;
            }
        }
		for (i = 0; i < numChains; i++) {
            if (AR_SREV_POSEIDON(ah)) {
                HALASSERT(numChains == 0x1);
            }
			if (AR9300_IS_CHAIN_RX_IQCAL_INVALID(ah, offset_array[i])) {
				rx_iqcal_invalid = 1;
			}
		}
        if (AR9300_IS_RX_IQCAL_DISABLED(ah)) {
	        rx_iqcal_invalid = 1;
		}

		return rx_iqcal_invalid;
#endif
	default:
		HALASSERT(0);
	}
	return 0;
}

#if ATH_TRAFFIC_FAST_RECOVER
#define PLL3 0x16188
#define PLL3_DO_MEAS_MASK 0x40000000
#define PLL4 0x1618c
#define PLL4_MEAS_DONE    0x8
#define SQSUM_DVC_MASK 0x007ffff8
unsigned long
ar9300GetPll3SqsumDvc(struct ath_hal *ah)
{
    if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah) || AR_SREV_WASP(ah)) {
        OS_REG_WRITE(ah, PLL3, (OS_REG_READ(ah, PLL3) & ~(PLL3_DO_MEAS_MASK)));
        OS_DELAY(100);
        OS_REG_WRITE(ah, PLL3, (OS_REG_READ(ah, PLL3) | PLL3_DO_MEAS_MASK));

        while ( (OS_REG_READ(ah, PLL4) & PLL4_MEAS_DONE) == 0) {
            OS_DELAY(100);
        }

        return (( OS_REG_READ(ah, PLL3) & SQSUM_DVC_MASK ) >> 3);
    } else {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE,
                 "%s: unable to get pll3_sqsum_dvc\n",
                 __func__);
        return 0;
    }
}
#endif

void ar9300RxGainTableApply(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);

    switch(ar9300RxGainIndexGet(ah))
    {
        case 0:
        default:
            if (AR_SREV_HORNET_12(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, 
                    ar9331_common_rx_gain_hornet1_2, 
                    sizeof(ar9331_common_rx_gain_hornet1_2)/sizeof(ar9331_common_rx_gain_hornet1_2[0]), 2);
            } else if (AR_SREV_HORNET_11(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, 
                    ar9331_common_rx_gain_hornet1_1, 
                    sizeof(ar9331_common_rx_gain_hornet1_1)/sizeof(ar9331_common_rx_gain_hornet1_1[0]), 2);
            }else if (AR_SREV_POSEIDON_11(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, 
                    ar9485_common_wo_xlna_rx_gain_poseidon1_1,
                    sizeof(ar9485_common_wo_xlna_rx_gain_poseidon1_1) /
                    sizeof(ar9485_common_wo_xlna_rx_gain_poseidon1_1[0]), 2);
            } else if (AR_SREV_POSEIDON(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, 
                    ar9485Common_wo_xlna_rx_gain_poseidon1_0,
                    sizeof(ar9485Common_wo_xlna_rx_gain_poseidon1_0) /
                    sizeof(ar9485Common_wo_xlna_rx_gain_poseidon1_0[0]), 2);
            } else if (AR_SREV_AR9580(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, ar9300Common_rx_gain_table_ar9580_1p0,
                    sizeof(ar9300Common_rx_gain_table_ar9580_1p0)/sizeof(ar9300Common_rx_gain_table_ar9580_1p0[0]), 2);
            } else if (AR_SREV_WASP(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, ar9340Common_rx_gain_table_wasp_1p0,
                    sizeof(ar9340Common_rx_gain_table_wasp_1p0)/sizeof(ar9340Common_rx_gain_table_wasp_1p0[0]), 2);
            } else {
                INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, ar9300Common_rx_gain_table_osprey_2p2,
                    sizeof(ar9300Common_rx_gain_table_osprey_2p2)/sizeof(ar9300Common_rx_gain_table_osprey_2p2[0]), 2);
            }
            break;
        case 1:
            if (AR_SREV_HORNET_12(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, 
                    ar9331_common_wo_xlna_rx_gain_hornet1_2,
                    sizeof(ar9331_common_wo_xlna_rx_gain_hornet1_2)/sizeof(ar9331_common_wo_xlna_rx_gain_hornet1_2[0]), 2);
            } else if (AR_SREV_HORNET_11(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, 
                    ar9331_common_wo_xlna_rx_gain_hornet1_1,
                    sizeof(ar9331_common_wo_xlna_rx_gain_hornet1_1)/sizeof(ar9331_common_wo_xlna_rx_gain_hornet1_1[0]), 2);
            }else if (AR_SREV_POSEIDON_11(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, 
                    ar9485_common_wo_xlna_rx_gain_poseidon1_1,
                    sizeof(ar9485_common_wo_xlna_rx_gain_poseidon1_1) /
                    sizeof(ar9485_common_wo_xlna_rx_gain_poseidon1_1[0]), 2);
            } else if (AR_SREV_POSEIDON(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, 
                    ar9485Common_wo_xlna_rx_gain_poseidon1_0,
                    sizeof(ar9485Common_wo_xlna_rx_gain_poseidon1_0) /
                    sizeof(ar9485Common_wo_xlna_rx_gain_poseidon1_0[0]), 2);
            } else if (AR_SREV_AR9580(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, ar9300Common_wo_xlna_rx_gain_table_ar9580_1p0,
                    sizeof(ar9300Common_wo_xlna_rx_gain_table_ar9580_1p0)/sizeof(ar9300Common_wo_xlna_rx_gain_table_ar9580_1p0[0]), 2);
            } else if (AR_SREV_WASP(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, ar9340Common_wo_xlna_rx_gain_table_wasp_1p0,
                    sizeof(ar9340Common_wo_xlna_rx_gain_table_wasp_1p0)/sizeof(ar9340Common_wo_xlna_rx_gain_table_wasp_1p0[0]), 2);
            } else {
                INIT_INI_ARRAY(&ahp->ah_iniModesRxgain, ar9300Common_wo_xlna_rx_gain_table_osprey_2p2,
                    sizeof(ar9300Common_wo_xlna_rx_gain_table_osprey_2p2)/sizeof(ar9300Common_wo_xlna_rx_gain_table_osprey_2p2[0]), 2);
            }
            break;
    }
}

void ar9300TxGainTableApply(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);

    switch(ar9300TxGainIndexGet(ah))
    {
        case 0:
        default:
            if (AR_SREV_HORNET_12(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, 
                    ar9331_modes_lowest_ob_db_tx_gain_hornet1_2, 
                    sizeof(ar9331_modes_lowest_ob_db_tx_gain_hornet1_2)/sizeof(ar9331_modes_lowest_ob_db_tx_gain_hornet1_2[0]), 5);
            } else if (AR_SREV_HORNET_11(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, 
                    ar9331_modes_lowest_ob_db_tx_gain_hornet1_1, 
                    sizeof(ar9331_modes_lowest_ob_db_tx_gain_hornet1_1)/sizeof(ar9331_modes_lowest_ob_db_tx_gain_hornet1_1[0]), 5);
            }else if (AR_SREV_POSEIDON_11(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9485_modes_lowest_ob_db_tx_gain_poseidon1_1, 
                    sizeof(ar9485_modes_lowest_ob_db_tx_gain_poseidon1_1)/sizeof(ar9485_modes_lowest_ob_db_tx_gain_poseidon1_1[0]), 5);
            } else if (AR_SREV_POSEIDON(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9485Modes_lowest_ob_db_tx_gain_poseidon1_0, 
                    sizeof(ar9485Modes_lowest_ob_db_tx_gain_poseidon1_0)/sizeof(ar9485Modes_lowest_ob_db_tx_gain_poseidon1_0[0]), 5);
            } else if (AR_SREV_AR9580(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9300Modes_lowest_ob_db_tx_gain_table_ar9580_1p0,
                    sizeof(ar9300Modes_lowest_ob_db_tx_gain_table_ar9580_1p0)/sizeof(ar9300Modes_lowest_ob_db_tx_gain_table_ar9580_1p0[0]), 5);
            } else if (AR_SREV_WASP(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9340Modes_lowest_ob_db_tx_gain_table_wasp_1p0,
                    sizeof(ar9340Modes_lowest_ob_db_tx_gain_table_wasp_1p0)/sizeof(ar9340Modes_lowest_ob_db_tx_gain_table_wasp_1p0[0]), 5);
            } else {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9300Modes_lowest_ob_db_tx_gain_table_osprey_2p2,
                    sizeof(ar9300Modes_lowest_ob_db_tx_gain_table_osprey_2p2)/sizeof(ar9300Modes_lowest_ob_db_tx_gain_table_osprey_2p2[0]), 5);
            }
            break;
        case 1:
            if (AR_SREV_HORNET_12(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, 
                    ar9331_modes_high_ob_db_tx_gain_hornet1_2, 
                    sizeof(ar9331_modes_high_ob_db_tx_gain_hornet1_2)/sizeof(ar9331_modes_high_ob_db_tx_gain_hornet1_2[0]), 5);
            } else if (AR_SREV_HORNET_11(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, 
                    ar9331_modes_high_ob_db_tx_gain_hornet1_1, 
                    sizeof(ar9331_modes_high_ob_db_tx_gain_hornet1_1)/sizeof(ar9331_modes_high_ob_db_tx_gain_hornet1_1[0]), 5);
            }else if (AR_SREV_POSEIDON_11(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9485_modes_high_ob_db_tx_gain_poseidon1_1, 
                    sizeof(ar9485_modes_high_ob_db_tx_gain_poseidon1_1)/sizeof(ar9485_modes_high_ob_db_tx_gain_poseidon1_1[0]), 5);
            } else if (AR_SREV_POSEIDON(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9485Modes_high_ob_db_tx_gain_poseidon1_0, 
                    sizeof(ar9485Modes_high_ob_db_tx_gain_poseidon1_0)/sizeof(ar9485Modes_high_ob_db_tx_gain_poseidon1_0[0]), 5);
            } else if (AR_SREV_AR9580(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9300Modes_high_ob_db_tx_gain_table_ar9580_1p0,
                    sizeof(ar9300Modes_high_ob_db_tx_gain_table_ar9580_1p0)/sizeof(ar9300Modes_high_ob_db_tx_gain_table_ar9580_1p0[0]), 5);
            } else if (AR_SREV_WASP(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9340Modes_high_ob_db_tx_gain_table_wasp_1p0,
                    sizeof(ar9340Modes_high_ob_db_tx_gain_table_wasp_1p0)/sizeof(ar9340Modes_high_ob_db_tx_gain_table_wasp_1p0[0]), 5);
            } else {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9300Modes_high_ob_db_tx_gain_table_osprey_2p2,
                    sizeof(ar9300Modes_high_ob_db_tx_gain_table_osprey_2p2)/sizeof(ar9300Modes_high_ob_db_tx_gain_table_osprey_2p2[0]), 5);
            }
            break;
        case 2:
            if (AR_SREV_HORNET_12(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, 
                    ar9331_modes_low_ob_db_tx_gain_hornet1_2, 
                    sizeof(ar9331_modes_low_ob_db_tx_gain_hornet1_2)/sizeof(ar9331_modes_low_ob_db_tx_gain_hornet1_2[0]), 5);
            } else if (AR_SREV_HORNET_11(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, 
                    ar9331_modes_low_ob_db_tx_gain_hornet1_1, 
                    sizeof(ar9331_modes_low_ob_db_tx_gain_hornet1_1)/sizeof(ar9331_modes_low_ob_db_tx_gain_hornet1_1[0]), 5);
            }else if (AR_SREV_POSEIDON_11(ah)) { // WB225 board
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9485_modes_low_ob_db_tx_gain_poseidon1_1, 
                    sizeof(ar9485_modes_low_ob_db_tx_gain_poseidon1_1)/sizeof(ar9485_modes_low_ob_db_tx_gain_poseidon1_1[0]), 5);
            } else if (AR_SREV_POSEIDON(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9485Modes_low_ob_db_tx_gain_poseidon1_0, 
                    sizeof(ar9485Modes_low_ob_db_tx_gain_poseidon1_0)/sizeof(ar9485Modes_low_ob_db_tx_gain_poseidon1_0[0]), 5);
            } else if (AR_SREV_AR9580(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9300Modes_low_ob_db_tx_gain_table_ar9580_1p0,
                    sizeof(ar9300Modes_low_ob_db_tx_gain_table_ar9580_1p0)/sizeof(ar9300Modes_low_ob_db_tx_gain_table_ar9580_1p0[0]), 5);
            } else if (AR_SREV_WASP(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9340Modes_low_ob_db_tx_gain_table_wasp_1p0,
                    sizeof(ar9340Modes_low_ob_db_tx_gain_table_wasp_1p0)/sizeof(ar9340Modes_low_ob_db_tx_gain_table_wasp_1p0[0]), 5);
            } else {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9300Modes_low_ob_db_tx_gain_table_osprey_2p2,
                    sizeof(ar9300Modes_low_ob_db_tx_gain_table_osprey_2p2)/sizeof(ar9300Modes_low_ob_db_tx_gain_table_osprey_2p2[0]), 5);
            }
            break;
        case 3:
            if (AR_SREV_HORNET_12(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, 
                    ar9331_modes_high_power_tx_gain_hornet1_2, 
                    sizeof(ar9331_modes_high_power_tx_gain_hornet1_2)/sizeof(ar9331_modes_high_power_tx_gain_hornet1_2[0]), 5);
            } else if (AR_SREV_HORNET_11(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, 
                    ar9331_modes_high_power_tx_gain_hornet1_1, 
                    sizeof(ar9331_modes_high_power_tx_gain_hornet1_1)/sizeof(ar9331_modes_high_power_tx_gain_hornet1_1[0]), 5);
            }else if (AR_SREV_POSEIDON_11(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9485_modes_high_power_tx_gain_poseidon1_1, 
                    sizeof(ar9485_modes_high_power_tx_gain_poseidon1_1)/sizeof(ar9485_modes_high_power_tx_gain_poseidon1_1[0]), 5);
            } else if (AR_SREV_POSEIDON(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9485Modes_high_power_tx_gain_poseidon1_0, 
                    sizeof(ar9485Modes_high_power_tx_gain_poseidon1_0)/sizeof(ar9485Modes_high_power_tx_gain_poseidon1_0[0]), 5);
            } else if (AR_SREV_AR9580(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9300Modes_high_power_tx_gain_table_ar9580_1p0,
                        sizeof(ar9300Modes_high_power_tx_gain_table_ar9580_1p0)/sizeof(ar9300Modes_high_power_tx_gain_table_ar9580_1p0[0]), 5);
            } else if (AR_SREV_WASP(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9340Modes_high_power_tx_gain_table_wasp_1p0,
                        sizeof(ar9340Modes_high_power_tx_gain_table_wasp_1p0)/sizeof(ar9340Modes_high_power_tx_gain_table_wasp_1p0[0]), 5);
            } else {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9300Modes_high_power_tx_gain_table_osprey_2p2,
                        sizeof(ar9300Modes_high_power_tx_gain_table_osprey_2p2)/sizeof(ar9300Modes_high_power_tx_gain_table_osprey_2p2[0]), 5);
            }
            break;
        case 4:
            if (AR_SREV_WASP(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9340Modes_mixed_ob_db_tx_gain_table_wasp_1p0,
                        sizeof(ar9340Modes_mixed_ob_db_tx_gain_table_wasp_1p0)/sizeof(ar9340Modes_mixed_ob_db_tx_gain_table_wasp_1p0[0]), 5);
            }else{
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9300Modes_mixed_ob_db_tx_gain_table_osprey_2p2,
                        sizeof(ar9300Modes_mixed_ob_db_tx_gain_table_osprey_2p2)/sizeof(ar9300Modes_mixed_ob_db_tx_gain_table_osprey_2p2[0]), 5);
            }
            break;
	case 5:
            /* HW Green TX */
            if (AR_SREV_POSEIDON(ah)) {
                if (AR_SREV_POSEIDON_11(ah)) {
                    INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, 
                        ar9485_modes_green_ob_db_tx_gain_poseidon1_1,
                        sizeof(ar9485_modes_green_ob_db_tx_gain_poseidon1_1) /
                        sizeof(ar9485_modes_green_ob_db_tx_gain_poseidon1_1[0]), 5);
                } else {
                    INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, 
                        ar9485_modes_green_ob_db_tx_gain_poseidon1_0,
                        sizeof(ar9485_modes_green_ob_db_tx_gain_poseidon1_0) /
                        sizeof(ar9485_modes_green_ob_db_tx_gain_poseidon1_0[0]), 5);
                }
				ahp->ah_hw_green_tx_enable = 1;
            }
            else if (AR_SREV_WASP(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, ar9340Modes_ub124_tx_gain_table_wasp_1p0,
                        sizeof(ar9340Modes_ub124_tx_gain_table_wasp_1p0)/sizeof(ar9340Modes_ub124_tx_gain_table_wasp_1p0[0]), 5);
            }
            else if (AR_SREV_AR9580(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain,
                    ar9300Modes_type5_tx_gain_table_ar9580_1p0,
                    ARRAY_LENGTH( ar9300Modes_type5_tx_gain_table_ar9580_1p0),
                    5);
            }
            else if (AR_SREV_OSPREY_22(ah)) {
                INIT_INI_ARRAY(&ahp->ah_iniModesTxgain,
                    ar9300Modes_number_5_tx_gain_table_osprey_2p2,
                    ARRAY_LENGTH( ar9300Modes_number_5_tx_gain_table_osprey_2p2),
                    5);
            }

            break;
	case 6:
        if (AR_SREV_WASP(ah)) {
            INIT_INI_ARRAY(&ahp->ah_iniModesTxgain,
            ar9340_modes_low_ob_db_and_spur_tx_gain_table_wasp_1p0,
            sizeof(ar9340_modes_low_ob_db_and_spur_tx_gain_table_wasp_1p0) /
            sizeof(ar9340_modes_low_ob_db_and_spur_tx_gain_table_wasp_1p0[0]), 5);
        }
            /* HW Green TX SPUR fixed */
        else if (AR_SREV_POSEIDON(ah)) {
                if (AR_SREV_POSEIDON_11(ah)) {
                    INIT_INI_ARRAY(&ahp->ah_iniModesTxgain, 
                        ar9485_modes_green_spur_ob_db_tx_gain_poseidon1_1,
                        sizeof(ar9485_modes_green_spur_ob_db_tx_gain_poseidon1_1) /
                        sizeof(ar9485_modes_green_spur_ob_db_tx_gain_poseidon1_1[0]), 5);
                }
				ahp->ah_hw_green_tx_enable = 1;
	}
        else if (AR_SREV_AR9580(ah)) {
            INIT_INI_ARRAY(&ahp->ah_iniModesTxgain,
                ar9300Modes_type6_tx_gain_table_ar9580_1p0,
                ARRAY_LENGTH( ar9300Modes_type6_tx_gain_table_ar9580_1p0),
                5);
        }

	break;
    }
}

#if ATH_ANT_DIV_COMB
void
ar9300AntDivCombGetConfig(struct ath_hal *ah, HAL_ANT_COMB_CONFIG* divCombConf)
{
    u_int32_t regVal = OS_REG_READ(ah, AR_PHY_MC_GAIN_CTRL);
    divCombConf->main_lna_conf = 
        MULTICHAIN_GAIN_CTRL__ANT_DIV_MAIN_LNACONF__READ(regVal);
    divCombConf->alt_lna_conf = 
        MULTICHAIN_GAIN_CTRL__ANT_DIV_ALT_LNACONF__READ(regVal);
    divCombConf->fast_div_bias = 
        MULTICHAIN_GAIN_CTRL__ANT_FAST_DIV_BIAS__READ(regVal); 
    if (AR_SREV_HORNET_11(ah)) {
        divCombConf->antdiv_configgroup = HAL_ANTDIV_CONFIG_GROUP_1;
    } else if (AR_SREV_POSEIDON_11(ah)) {
        divCombConf->antdiv_configgroup = HAL_ANTDIV_CONFIG_GROUP_2;
    } else {
        divCombConf->antdiv_configgroup = DEFAULT_ANTDIV_CONFIG_GROUP;
    }
}

void
ar9300AntDivCombSetConfig(struct ath_hal *ah, HAL_ANT_COMB_CONFIG* divCombConf)
{
    u_int32_t regVal = OS_REG_READ(ah, AR_PHY_MC_GAIN_CTRL);
    regVal &= ~(MULTICHAIN_GAIN_CTRL__ANT_DIV_MAIN_LNACONF__MASK | 
                MULTICHAIN_GAIN_CTRL__ANT_DIV_ALT_LNACONF__MASK |
                MULTICHAIN_GAIN_CTRL__ANT_FAST_DIV_BIAS__MASK |
		        MULTICHAIN_GAIN_CTRL__ANT_DIV_MAIN_GAINTB__MASK |
		        MULTICHAIN_GAIN_CTRL__ANT_DIV_ALT_GAINTB__MASK );
    regVal |=
	    MULTICHAIN_GAIN_CTRL__ANT_DIV_MAIN_GAINTB__WRITE(divCombConf->main_gaintb);
    regVal |=
	    MULTICHAIN_GAIN_CTRL__ANT_DIV_ALT_GAINTB__WRITE(divCombConf->alt_gaintb);
    regVal |= 
        MULTICHAIN_GAIN_CTRL__ANT_DIV_MAIN_LNACONF__WRITE(divCombConf->main_lna_conf);
    regVal |= 
        MULTICHAIN_GAIN_CTRL__ANT_DIV_ALT_LNACONF__WRITE(divCombConf->alt_lna_conf);
    regVal |= 
        MULTICHAIN_GAIN_CTRL__ANT_FAST_DIV_BIAS__WRITE(divCombConf->fast_div_bias);
    OS_REG_WRITE(ah, AR_PHY_MC_GAIN_CTRL, regVal);

}
#endif /* ATH_ANT_DIV_COMB */

static inline void 
ar9300InitHostifOffsets(struct ath_hal *ah)
{
    AR_HOSTIF_REG(ah, AR_RC)               = AR9300_HOSTIF_OFFSET(HOST_INTF_RESET_CONTROL);
    AR_HOSTIF_REG(ah, AR_WA)               = AR9300_HOSTIF_OFFSET(HOST_INTF_WORK_AROUND);                   
    AR_HOSTIF_REG(ah, AR_PM_STATE)         = AR9300_HOSTIF_OFFSET(HOST_INTF_PM_STATE);
    AR_HOSTIF_REG(ah, AR_H_INFOL)          = AR9300_HOSTIF_OFFSET(HOST_INTF_CXPL_DEBUG_INFOL);
    AR_HOSTIF_REG(ah, AR_H_INFOH)          = AR9300_HOSTIF_OFFSET(HOST_INTF_CXPL_DEBUG_INFOH);
    AR_HOSTIF_REG(ah, AR_PCIE_PM_CTRL)     = AR9300_HOSTIF_OFFSET(HOST_INTF_PM_CTRL);
    AR_HOSTIF_REG(ah, AR_HOST_TIMEOUT)     = AR9300_HOSTIF_OFFSET(HOST_INTF_TIMEOUT);
    AR_HOSTIF_REG(ah, AR_EEPROM)           = AR9300_HOSTIF_OFFSET(HOST_INTF_EEPROM_CTRL);
    AR_HOSTIF_REG(ah, AR_SREV)             = AR9300_HOSTIF_OFFSET(HOST_INTF_SREV);
    AR_HOSTIF_REG(ah, AR_INTR_SYNC_CAUSE)  = AR9300_HOSTIF_OFFSET(HOST_INTF_INTR_SYNC_CAUSE);
    AR_HOSTIF_REG(ah, AR_INTR_SYNC_CAUSE_CLR) = AR9300_HOSTIF_OFFSET(HOST_INTF_INTR_SYNC_CAUSE);
    AR_HOSTIF_REG(ah, AR_INTR_SYNC_ENABLE)    = AR9300_HOSTIF_OFFSET(HOST_INTF_INTR_SYNC_ENABLE);
    AR_HOSTIF_REG(ah, AR_INTR_ASYNC_MASK)     = AR9300_HOSTIF_OFFSET(HOST_INTF_INTR_ASYNC_MASK);
    AR_HOSTIF_REG(ah, AR_INTR_SYNC_MASK)      = AR9300_HOSTIF_OFFSET(HOST_INTF_INTR_SYNC_MASK);
    AR_HOSTIF_REG(ah, AR_INTR_ASYNC_CAUSE_CLR) = AR9300_HOSTIF_OFFSET(HOST_INTF_INTR_ASYNC_CAUSE);
    AR_HOSTIF_REG(ah, AR_INTR_ASYNC_CAUSE)     = AR9300_HOSTIF_OFFSET(HOST_INTF_INTR_ASYNC_CAUSE);
    AR_HOSTIF_REG(ah, AR_INTR_ASYNC_ENABLE)          = AR9300_HOSTIF_OFFSET(HOST_INTF_INTR_ASYNC_ENABLE);
    AR_HOSTIF_REG(ah, AR_PCIE_SERDES)                = AR9300_HOSTIF_OFFSET(HOST_INTF_PCIE_PHY_RW);
    AR_HOSTIF_REG(ah, AR_PCIE_SERDES2)               = AR9300_HOSTIF_OFFSET(HOST_INTF_PCIE_PHY_LOAD);
    AR_HOSTIF_REG(ah, AR_GPIO_OUT)                   = AR9300_HOSTIF_OFFSET(HOST_INTF_GPIO_OUT);
    AR_HOSTIF_REG(ah, AR_GPIO_IN)                    = AR9300_HOSTIF_OFFSET(HOST_INTF_GPIO_IN);
    AR_HOSTIF_REG(ah, AR_GPIO_OE_OUT)                = AR9300_HOSTIF_OFFSET(HOST_INTF_GPIO_OE);
    AR_HOSTIF_REG(ah, AR_GPIO_OE1_OUT)               = AR9300_HOSTIF_OFFSET(HOST_INTF_GPIO_OE1);
    AR_HOSTIF_REG(ah, AR_GPIO_INTR_POL)              = AR9300_HOSTIF_OFFSET(HOST_INTF_GPIO_INTR_POLAR);
    AR_HOSTIF_REG(ah, AR_GPIO_INPUT_EN_VAL)          = AR9300_HOSTIF_OFFSET(HOST_INTF_GPIO_INPUT_VALUE);
    AR_HOSTIF_REG(ah, AR_GPIO_INPUT_MUX1)            = AR9300_HOSTIF_OFFSET(HOST_INTF_GPIO_INPUT_MUX1);
    AR_HOSTIF_REG(ah, AR_GPIO_INPUT_MUX2)            = AR9300_HOSTIF_OFFSET(HOST_INTF_GPIO_INPUT_MUX2);
    AR_HOSTIF_REG(ah, AR_GPIO_OUTPUT_MUX1)           = AR9300_HOSTIF_OFFSET(HOST_INTF_GPIO_OUTPUT_MUX1);
    AR_HOSTIF_REG(ah, AR_GPIO_OUTPUT_MUX2)           = AR9300_HOSTIF_OFFSET(HOST_INTF_GPIO_OUTPUT_MUX2);
    AR_HOSTIF_REG(ah, AR_GPIO_OUTPUT_MUX3)           = AR9300_HOSTIF_OFFSET(HOST_INTF_GPIO_OUTPUT_MUX3);
    AR_HOSTIF_REG(ah, AR_INPUT_STATE)                = AR9300_HOSTIF_OFFSET(HOST_INTF_GPIO_INPUT_STATE);
    AR_HOSTIF_REG(ah, AR_SPARE)                      = AR9300_HOSTIF_OFFSET(HOST_INTF_SPARE);
    AR_HOSTIF_REG(ah, AR_PCIE_CORE_RESET_EN)         = AR9300_HOSTIF_OFFSET(HOST_INTF_PCIE_CORE_RST_EN);
    AR_HOSTIF_REG(ah, AR_CLKRUN)                     = AR9300_HOSTIF_OFFSET(HOST_INTF_CLKRUN);
    AR_HOSTIF_REG(ah, AR_EEPROM_STATUS_DATA)         = AR9300_HOSTIF_OFFSET(HOST_INTF_EEPROM_STS);
    AR_HOSTIF_REG(ah, AR_OBS)                        = AR9300_HOSTIF_OFFSET(HOST_INTF_OBS_CTRL);
    AR_HOSTIF_REG(ah, AR_RFSILENT)                   = AR9300_HOSTIF_OFFSET(HOST_INTF_RFSILENT);
    AR_HOSTIF_REG(ah, AR_GPIO_PDPU)                  = AR9300_HOSTIF_OFFSET(HOST_INTF_GPIO_PDPU);
    AR_HOSTIF_REG(ah, AR_GPIO_DS)                    = AR9300_HOSTIF_OFFSET(HOST_INTF_GPIO_DS);
    AR_HOSTIF_REG(ah, AR_MISC)                       = AR9300_HOSTIF_OFFSET(HOST_INTF_MISC);
    AR_HOSTIF_REG(ah, AR_PCIE_MSI)                   = AR9300_HOSTIF_OFFSET(HOST_INTF_PCIE_MSI);
#if 0   /* Offsets are not defined in reg_map structure */ 
    AR_HOSTIF_REG(ah, AR_TSF_SNAPSHOT_BT_ACTIVE)     = AR9300_HOSTIF_OFFSET(HOST_INTF_TSF_SNAPSHOT_BT_ACTIVE);
    AR_HOSTIF_REG(ah, AR_TSF_SNAPSHOT_BT_PRIORITY)   = AR9300_HOSTIF_OFFSET(HOST_INTF_TSF_SNAPSHOT_BT_PRIORITY);
    AR_HOSTIF_REG(ah, AR_TSF_SNAPSHOT_BT_CNTL)       = AR9300_HOSTIF_OFFSET(HOST_INTF_MAC_TSF_SNAPSHOT_BT_CNTL);
#endif
    AR_HOSTIF_REG(ah, AR_PCIE_PHY_LATENCY_NFTS_ADJ)  = AR9300_HOSTIF_OFFSET(HOST_INTF_PCIE_PHY_LATENCY_NFTS_ADJ);
    AR_HOSTIF_REG(ah, AR_TDMA_CCA_CNTL)              = AR9300_HOSTIF_OFFSET(HOST_INTF_MAC_TDMA_CCA_CNTL);
    AR_HOSTIF_REG(ah, AR_TXAPSYNC)                   = AR9300_HOSTIF_OFFSET(HOST_INTF_MAC_TXAPSYNC);
    AR_HOSTIF_REG(ah, AR_TXSYNC_INIT_SYNC_TMR)       = AR9300_HOSTIF_OFFSET(HOST_INTF_MAC_TXSYNC_INITIAL_SYNC_TMR);
    AR_HOSTIF_REG(ah, AR_INTR_PRIO_SYNC_CAUSE)       = AR9300_HOSTIF_OFFSET(HOST_INTF_INTR_PRIORITY_SYNC_CAUSE);
    AR_HOSTIF_REG(ah, AR_INTR_PRIO_SYNC_ENABLE)      = AR9300_HOSTIF_OFFSET(HOST_INTF_INTR_PRIORITY_SYNC_ENABLE);
    AR_HOSTIF_REG(ah, AR_INTR_PRIO_ASYNC_MASK)       = AR9300_HOSTIF_OFFSET(HOST_INTF_INTR_PRIORITY_ASYNC_MASK);
    AR_HOSTIF_REG(ah, AR_INTR_PRIO_SYNC_MASK)        = AR9300_HOSTIF_OFFSET(HOST_INTF_INTR_PRIORITY_SYNC_MASK);
    AR_HOSTIF_REG(ah, AR_INTR_PRIO_ASYNC_CAUSE)      = AR9300_HOSTIF_OFFSET(HOST_INTF_INTR_PRIORITY_ASYNC_CAUSE);
    AR_HOSTIF_REG(ah, AR_INTR_PRIO_ASYNC_ENABLE)     = AR9300_HOSTIF_OFFSET(HOST_INTF_INTR_PRIORITY_ASYNC_ENABLE);
}

static inline void 
ar9340InitHostifOffsets(struct ath_hal *ah)
{
    AR_HOSTIF_REG(ah, AR_RC)               = AR9340_HOSTIF_OFFSET(HOST_INTF_RESET_CONTROL);
    AR_HOSTIF_REG(ah, AR_WA)               = AR9340_HOSTIF_OFFSET(HOST_INTF_WORK_AROUND);                   
    AR_HOSTIF_REG(ah, AR_PCIE_PM_CTRL)     = AR9340_HOSTIF_OFFSET(HOST_INTF_PM_CTRL);
    AR_HOSTIF_REG(ah, AR_HOST_TIMEOUT)     = AR9340_HOSTIF_OFFSET(HOST_INTF_TIMEOUT);
    AR_HOSTIF_REG(ah, AR_SREV)             = AR9340_HOSTIF_OFFSET(HOST_INTF_SREV);
    AR_HOSTIF_REG(ah, AR_INTR_SYNC_CAUSE)  = AR9340_HOSTIF_OFFSET(HOST_INTF_INTR_SYNC_CAUSE);
    AR_HOSTIF_REG(ah, AR_INTR_SYNC_CAUSE_CLR) = AR9340_HOSTIF_OFFSET(HOST_INTF_INTR_SYNC_CAUSE);
    AR_HOSTIF_REG(ah, AR_INTR_SYNC_ENABLE)    = AR9340_HOSTIF_OFFSET(HOST_INTF_INTR_SYNC_ENABLE);
    AR_HOSTIF_REG(ah, AR_INTR_ASYNC_MASK)     = AR9340_HOSTIF_OFFSET(HOST_INTF_INTR_ASYNC_MASK);
    AR_HOSTIF_REG(ah, AR_INTR_SYNC_MASK)      = AR9340_HOSTIF_OFFSET(HOST_INTF_INTR_SYNC_MASK);
    AR_HOSTIF_REG(ah, AR_INTR_ASYNC_CAUSE_CLR) = AR9340_HOSTIF_OFFSET(HOST_INTF_INTR_ASYNC_CAUSE);
    AR_HOSTIF_REG(ah, AR_INTR_ASYNC_CAUSE)     = AR9340_HOSTIF_OFFSET(HOST_INTF_INTR_ASYNC_CAUSE);
    AR_HOSTIF_REG(ah, AR_INTR_ASYNC_ENABLE)          = AR9340_HOSTIF_OFFSET(HOST_INTF_INTR_ASYNC_ENABLE);
    AR_HOSTIF_REG(ah, AR_GPIO_OUT)                   = AR9340_HOSTIF_OFFSET(HOST_INTF_GPIO_OUT);
    AR_HOSTIF_REG(ah, AR_GPIO_IN)                    = AR9340_HOSTIF_OFFSET(HOST_INTF_GPIO_IN);
    AR_HOSTIF_REG(ah, AR_GPIO_OE_OUT)                = AR9340_HOSTIF_OFFSET(HOST_INTF_GPIO_OE);
    AR_HOSTIF_REG(ah, AR_GPIO_OE1_OUT)               = AR9340_HOSTIF_OFFSET(HOST_INTF_GPIO_OE1);
    AR_HOSTIF_REG(ah, AR_GPIO_INTR_POL)              = AR9340_HOSTIF_OFFSET(HOST_INTF_GPIO_INTR_POLAR);
    AR_HOSTIF_REG(ah, AR_GPIO_INPUT_EN_VAL)          = AR9340_HOSTIF_OFFSET(HOST_INTF_GPIO_INPUT_VALUE);
    AR_HOSTIF_REG(ah, AR_GPIO_INPUT_MUX1)            = AR9340_HOSTIF_OFFSET(HOST_INTF_GPIO_INPUT_MUX1);
    AR_HOSTIF_REG(ah, AR_GPIO_INPUT_MUX2)            = AR9340_HOSTIF_OFFSET(HOST_INTF_GPIO_INPUT_MUX2);
    AR_HOSTIF_REG(ah, AR_GPIO_OUTPUT_MUX1)           = AR9340_HOSTIF_OFFSET(HOST_INTF_GPIO_OUTPUT_MUX1);
    AR_HOSTIF_REG(ah, AR_GPIO_OUTPUT_MUX2)           = AR9340_HOSTIF_OFFSET(HOST_INTF_GPIO_OUTPUT_MUX2);
    AR_HOSTIF_REG(ah, AR_GPIO_OUTPUT_MUX3)           = AR9340_HOSTIF_OFFSET(HOST_INTF_GPIO_OUTPUT_MUX3);
    AR_HOSTIF_REG(ah, AR_INPUT_STATE)                = AR9340_HOSTIF_OFFSET(HOST_INTF_GPIO_INPUT_STATE);
    AR_HOSTIF_REG(ah, AR_CLKRUN)                     = AR9340_HOSTIF_OFFSET(HOST_INTF_CLKRUN);
    AR_HOSTIF_REG(ah, AR_EEPROM_STATUS_DATA)         = AR9340_HOSTIF_OFFSET(HOST_INTF_EEPROM_STS);
    AR_HOSTIF_REG(ah, AR_OBS)                        = AR9340_HOSTIF_OFFSET(HOST_INTF_OBS_CTRL);
    AR_HOSTIF_REG(ah, AR_RFSILENT)                   = AR9340_HOSTIF_OFFSET(HOST_INTF_RFSILENT);
    AR_HOSTIF_REG(ah, AR_MISC)                       = AR9340_HOSTIF_OFFSET(HOST_INTF_MISC);
    AR_HOSTIF_REG(ah, AR_PCIE_MSI)                   = AR9340_HOSTIF_OFFSET(HOST_INTF_PCIE_MSI);
    AR_HOSTIF_REG(ah, AR_TDMA_CCA_CNTL)              = AR9340_HOSTIF_OFFSET(HOST_INTF_MAC_TDMA_CCA_CNTL);
    AR_HOSTIF_REG(ah, AR_TXAPSYNC)                   = AR9340_HOSTIF_OFFSET(HOST_INTF_MAC_TXAPSYNC);
    AR_HOSTIF_REG(ah, AR_TXSYNC_INIT_SYNC_TMR)       = AR9340_HOSTIF_OFFSET(HOST_INTF_MAC_TXSYNC_INITIAL_SYNC_TMR);
    AR_HOSTIF_REG(ah, AR_INTR_PRIO_SYNC_CAUSE)       = AR9340_HOSTIF_OFFSET(HOST_INTF_INTR_PRIORITY_SYNC_CAUSE);
    AR_HOSTIF_REG(ah, AR_INTR_PRIO_SYNC_ENABLE)      = AR9340_HOSTIF_OFFSET(HOST_INTF_INTR_PRIORITY_SYNC_ENABLE);
    AR_HOSTIF_REG(ah, AR_INTR_PRIO_ASYNC_MASK)       = AR9340_HOSTIF_OFFSET(HOST_INTF_INTR_PRIORITY_ASYNC_MASK);
    AR_HOSTIF_REG(ah, AR_INTR_PRIO_SYNC_MASK)        = AR9340_HOSTIF_OFFSET(HOST_INTF_INTR_PRIORITY_SYNC_MASK);
    AR_HOSTIF_REG(ah, AR_INTR_PRIO_ASYNC_CAUSE)      = AR9340_HOSTIF_OFFSET(HOST_INTF_INTR_PRIORITY_ASYNC_CAUSE);
    AR_HOSTIF_REG(ah, AR_INTR_PRIO_ASYNC_ENABLE)     = AR9340_HOSTIF_OFFSET(HOST_INTF_INTR_PRIORITY_ASYNC_ENABLE);
}

/* 
 * Host interface register offsets are different for Osprey and Wasp 
 * and hence store the offsets in hal structure
 */
static int ar9300InitOffsets(struct ath_hal *ah, u_int16_t devid)
{
    if (devid == AR9300_DEVID_AR9340) {
        ar9340InitHostifOffsets(ah);
    } else {
        ar9300InitHostifOffsets(ah);
    }
    return 0;
}

#endif /* AH_SUPPORT_AR9300 */
