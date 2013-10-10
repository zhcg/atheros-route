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

#ifndef _ATH_AR9300_H_
#define _ATH_AR9300_H_

#include "ah_internal.h"
#include "ah_eeprom.h"
#include "ah_devid.h"
#include "ar9300eep.h"  /* For Eeprom definitions */
#include "asf_amem.h"

#define AR9300_MAGIC    0x19741014


/* MAC register values */

#define INIT_CONFIG_STATUS  0x00000000
#define INIT_RSSI_THR       0x00000700  /* Missed beacon counter initialized to 0x7 (max is 0xff) */

/*
 * Various fifo fill before Tx start, in 64-byte units
 * i.e. put the frame in the air while still DMAing
 */
#define MIN_TX_FIFO_THRESHOLD   0x1
#define MAX_TX_FIFO_THRESHOLD   (( 4096 / 64) - 1)
#define INIT_TX_FIFO_THRESHOLD  MIN_TX_FIFO_THRESHOLD

#ifdef AR9340_EMULATION
	#define CHANSEL_DIV		15
	#define FCLK			40
#else
#ifdef AR9300_EMULATION_BB
#if defined(AR9330_EMULATION) || defined(AR9485_EMULATION)
	/* Assume frequency of signal @ XTAL pins = 24 MHz,
	 * (4/3) * 0x20000 * (1/24) = (1/9) * 0x10000
	 */
	#define CHANSEL_DIV		9
#else
	#define CHANSEL_DIV		6
#endif
	#define FCLK			2
#else
	#define CHANSEL_DIV		15
	#define FCLK			40
#endif
#endif

#define COEFF ((FCLK * 5) / 2)
#define CHANSEL_2G(_freq)	(((_freq) * 0x10000) / CHANSEL_DIV)
#define CHANSEL_5G(_freq)	(((_freq) * 0x8000) / CHANSEL_DIV)
#define CHANSEL_5G_DOT5MHZ	2188

/*
 * Receive Queue Fifo depth.
 */
enum RX_FIFO_DEPTH {
    HAL_HP_RXFIFO_DEPTH             = 16,
    HAL_LP_RXFIFO_DEPTH             = 128,
};

/*
 * Gain support.
 */
#define NUM_CORNER_FIX_BITS_2133    7
#define CCK_OFDM_GAIN_DELTA         15

enum GAIN_PARAMS {
    GP_TXCLIP,
    GP_PD90,
    GP_PD84,
    GP_GSEL
};

enum GAIN_PARAMS_2133 {
    GP_MIXGAIN_OVR,
    GP_PWD_138,
    GP_PWD_137,
    GP_PWD_136,
    GP_PWD_132,
    GP_PWD_131,
    GP_PWD_130,
};

enum {
    HAL_RESET_POWER_ON,
    HAL_RESET_WARM,
    HAL_RESET_COLD,
};

typedef struct _gainOptStep {
    int16_t paramVal[NUM_CORNER_FIX_BITS_2133];
    int32_t stepGain;
    int8_t  stepName[16];
} GAIN_OPTIMIZATION_STEP;

typedef struct {
    u_int32_t   numStepsInLadder;
    u_int32_t   defaultStepNum;
    GAIN_OPTIMIZATION_STEP optStep[10];
} GAIN_OPTIMIZATION_LADDER;

typedef struct {
    u_int32_t   currStepNum;
    u_int32_t   currGain;
    u_int32_t   targetGain;
    u_int32_t   loTrig;
    u_int32_t   hiTrig;
    u_int32_t   gainFCorrection;
    u_int32_t   active;
    const GAIN_OPTIMIZATION_STEP *currStep;
} GAIN_VALUES;

typedef struct {
    u_int16_t   synth_center;
    u_int16_t   ctl_center;
    u_int16_t   ext_center;
} CHAN_CENTERS;

/* RF HAL structures */
typedef struct RfHalFuncs {
    HAL_BOOL  (*setChannel)(struct ath_hal *, HAL_CHANNEL_INTERNAL *);
    HAL_BOOL  (*getChipPowerLim)(struct ath_hal *ah, HAL_CHANNEL *chans,
                       u_int32_t nchancs);
} RF_HAL_FUNCS;

struct ar9300AniDefault {
	u_int16_t   m1ThreshLow;
	u_int16_t   m2ThreshLow;
	u_int16_t   m1Thresh;
	u_int16_t   m2Thresh;
	u_int16_t   m2CountThr;
	u_int16_t   m2CountThrLow;
	u_int16_t   m1ThreshLowExt;
	u_int16_t   m2ThreshLowExt;
	u_int16_t   m1ThreshExt;
	u_int16_t   m2ThreshExt;
	u_int16_t   firstep;
	u_int16_t   firstepLow;
	u_int16_t   cycpwrThr1;
	u_int16_t   cycpwrThr1Ext;
};

/*
 * Per-channel ANI state private to the driver.
 */
struct ar9300AniState {
    HAL_CHANNEL c;
    HAL_BOOL    mustRestore;
    HAL_BOOL    ofdmsTurn;
    u_int8_t    ofdmNoiseImmunityLevel;
    u_int8_t    cckNoiseImmunityLevel;
    u_int8_t    spurImmunityLevel;
    u_int8_t    firstepLevel;
    u_int8_t    ofdmWeakSigDetectOff;
    u_int8_t    mrcCCKOff;

    /* Thresholds */
    u_int32_t   listenTime;
    u_int32_t   ofdmTrigHigh;
    u_int32_t   ofdmTrigLow;
    int32_t     cckTrigHigh;
    int32_t     cckTrigLow;
    int32_t     rssiThrLow;
    int32_t     rssiThrHigh;

    int32_t     rssi;       /* The current RSSI */
    u_int32_t   txFrameCount;   /* Last txFrameCount */
    u_int32_t   rxFrameCount;   /* Last rx Frame count */
    u_int32_t   cycleCount; /* Last cycleCount (can detect wrap-around) */
    u_int32_t   ofdmPhyErrCount;/* OFDM err count since last reset */
    u_int32_t   cckPhyErrCount; /* CCK err count since last reset */
    u_int32_t   ofdmPhyErrBase; /* Base value for ofdm err counter */
    u_int32_t   cckPhyErrBase;  /* Base value for cck err counters */

	struct ar9300AniDefault iniDef;   /* INI default values for ANI registers */
    HAL_BOOL    phyNoiseSpur; /* based on OFDM/CCK Phy errors */
};

#define AR9300_ANI_POLLINTERVAL    1000    /* 1000 milliseconds between ANI poll */

#define AR9300_CHANNEL_SWITCH_TIME_USEC 1000  /* 1 millisecond needed to change channel */

#define HAL_PROCESS_ANI     0x00000001  /* ANI state setup */
#define HAL_RADAR_EN        0x80000000  /* Radar detect is capable */
#define HAL_AR_EN           0x40000000  /* AR detect is capable */

#define DO_ANI(ah) \
    ((AH9300(ah)->ah_procPhyErr & HAL_PROCESS_ANI))

struct ar9300Stats {
    u_int32_t   ast_ani_niup;   /* ANI increased noise immunity */
    u_int32_t   ast_ani_nidown; /* ANI decreased noise immunity */
    u_int32_t   ast_ani_spurup; /* ANI increased spur immunity */
    u_int32_t   ast_ani_spurdown;/* ANI descreased spur immunity */
    u_int32_t   ast_ani_ofdmon; /* ANI OFDM weak signal detect on */
    u_int32_t   ast_ani_ofdmoff;/* ANI OFDM weak signal detect off */
    u_int32_t   ast_ani_cckhigh;/* ANI CCK weak signal threshold high */
    u_int32_t   ast_ani_ccklow; /* ANI CCK weak signal threshold low */
    u_int32_t   ast_ani_stepup; /* ANI increased first step level */
    u_int32_t   ast_ani_stepdown;/* ANI decreased first step level */
    u_int32_t   ast_ani_ofdmerrs;/* ANI cumulative ofdm phy err count */
    u_int32_t   ast_ani_cckerrs;/* ANI cumulative cck phy err count */
    u_int32_t   ast_ani_reset;  /* ANI parameters zero'd for non-STA */
    u_int32_t   ast_ani_lzero;  /* ANI listen time forced to zero */
    u_int32_t   ast_ani_lneg;   /* ANI listen time calculated < 0 */
    HAL_MIB_STATS   ast_mibstats;   /* MIB counter stats */
    HAL_NODE_STATS  ast_nodestats;  /* Latest rssi stats from driver */
};

struct ar9300RadReader {
    u_int16_t   rd_index;
    u_int16_t   rd_expSeq;
    u_int32_t   rd_resetVal;
    u_int8_t    rd_start;
};

struct ar9300RadWriter {
    u_int16_t   wr_index;
    u_int16_t   wr_seq;
};

struct ar9300RadarEvent {
    u_int32_t   re_ts;      /* 32 bit time stamp */
    u_int8_t    re_rssi;    /* rssi of radar event */
    u_int8_t    re_dur;     /* duration of radar pulse */
    u_int8_t    re_chanIndex;   /* Channel of event */
};

struct ar9300RadarQElem {
    u_int32_t   rq_seqNum;
    u_int32_t   rq_busy;        /* 32 bit to insure atomic read/write */
    struct ar9300RadarEvent rq_event;   /* Radar event */
};

struct ar9300RadarQInfo {
    u_int16_t   ri_qsize;       /* q size */
    u_int16_t   ri_seqSize;     /* Size of sequence ring */
    struct ar9300RadReader ri_reader;   /* State for the q reader */
    struct ar9300RadWriter ri_writer;   /* state for the q writer */
};

#define HAL_MAX_ACK_RADAR_DUR   511
#define HAL_MAX_NUM_PEAKS   3
#define HAL_ARQ_SIZE        4096        /* 8K AR events for buffer size */
#define HAL_ARQ_SEQSIZE     4097        /* Sequence counter wrap for AR */
#define HAL_RADARQ_SIZE     1024        /* 1K radar events for buffer size */
#define HAL_RADARQ_SEQSIZE  1025        /* Sequence counter wrap for radar */
#define HAL_NUMRADAR_STATES 64      /* Number of radar channels we keep state for */

struct ar9300ArState {
    u_int16_t   ar_prevTimeStamp;
    u_int32_t   ar_prevWidth;
    u_int32_t   ar_phyErrCount[HAL_MAX_ACK_RADAR_DUR];
    u_int32_t   ar_ackSum;
    u_int16_t   ar_peakList[HAL_MAX_NUM_PEAKS];
    u_int32_t   ar_packetThreshold; /* Thresh to determine traffic load */
    u_int32_t   ar_parThreshold;    /* Thresh to determine peak */
    u_int32_t   ar_radarRssi;       /* Rssi threshold for AR event */
};

struct ar9300RadarState {
    HAL_CHANNEL_INTERNAL *rs_chan;      /* Channel info */
    u_int8_t    rs_chanIndex;       /* Channel index in radar structure */
    u_int32_t   rs_numRadarEvents;  /* Number of radar events */
    int32_t     rs_firpwr;      /* Thresh to check radar sig is gone */
    u_int32_t   rs_radarRssi;       /* Thresh to start radar det (dB) */
    u_int32_t   rs_height;      /* Thresh for pulse height (dB)*/
    u_int32_t   rs_pulseRssi;       /* Thresh to check if pulse is gone (dB) */
    u_int32_t   rs_inband;      /* Thresh to check if pusle is inband (0.5 dB) */
};

#define AR9300_OPFLAGS_11A           0x01   /* if set, allow 11a */
#define AR9300_OPFLAGS_11G           0x02   /* if set, allow 11g */
#define AR9300_OPFLAGS_N_5G_HT40     0x04   /* if set, disable 5G HT40 */
#define AR9300_OPFLAGS_N_2G_HT40     0x08   /* if set, disable 2G HT40 */
#define AR9300_OPFLAGS_N_5G_HT20     0x10   /* if set, disable 5G HT20 */
#define AR9300_OPFLAGS_N_2G_HT20     0x20   /* if set, disable 2G HT20 */

/* 
 * For Kite and later chipsets, the following bits are not being programmed in EEPROM
 * and so need to be enabled always.
 * Bit 0: en_fcc_mid,  Bit 1: en_jap_mid,      Bit 2: en_fcc_dfs_ht40
 * Bit 3: en_jap_ht40, Bit 4: en_jap_dfs_ht40
 */
#define AR9300_RDEXT_DEFAULT  0x1F

#ifdef AR9340_EMULATION
#define AR9300_MAX_CHAINS            2
#else
#define AR9300_MAX_CHAINS            3
#endif
#define AR9300_NUM_CHAINS(chainmask) \
    (((chainmask >> 2) & 1) + ((chainmask >> 1) & 1) + (chainmask & 1))
#define AR9300_CHAIN0_MASK      0x1
#define AR9300_CHAIN1_MASK      0x2
#define AR9300_CHAIN2_MASK      0x4

#define FREQ2FBIN(x,y) ((y) ? ((x) - 2300) : (((x) - 4800) / 5))

/* Support for multiple INIs */
struct ar9300IniArray {
    u_int32_t *ia_array;
    u_int32_t ia_rows;
    u_int32_t ia_columns;
};
#define INIT_INI_ARRAY(iniarray, array, rows, columns) do {             \
    (iniarray)->ia_array = (u_int32_t *)(array);    \
    (iniarray)->ia_rows = (rows);       \
    (iniarray)->ia_columns = (columns); \
} while (0)
#define INI_RA(iniarray, row, column) (((iniarray)->ia_array)[(row) * ((iniarray)->ia_columns) + (column)])

#define INIT_CAL(_perCal)   \
    (_perCal)->calState = CAL_WAITING;  \
    (_perCal)->calNext = AH_NULL;

#define INSERT_CAL(_ahp, _perCal)   \
do {                    \
    if ((_ahp)->ah_cal_list_last == AH_NULL) {  \
        (_ahp)->ah_cal_list = (_ahp)->ah_cal_list_last = (_perCal); \
        ((_ahp)->ah_cal_list_last)->calNext = (_perCal);    \
    } else {    \
        ((_ahp)->ah_cal_list_last)->calNext = (_perCal);    \
        (_ahp)->ah_cal_list_last = (_perCal);   \
        (_perCal)->calNext = (_ahp)->ah_cal_list;   \
    }   \
} while (0)

typedef enum cal_types {
    IQ_MISMATCH_CAL = 0x1,
    TEMP_COMP_CAL   = 0x2,
} HAL_CAL_TYPES;

typedef enum cal_state {
    CAL_INACTIVE,
    CAL_WAITING,
    CAL_RUNNING,
    CAL_DONE
} HAL_CAL_STATE;            /* Calibrate state */

#define MIN_CAL_SAMPLES     1
#define MAX_CAL_SAMPLES    64
#define INIT_LOG_COUNT      5
#define PER_MIN_LOG_COUNT   2
#define PER_MAX_LOG_COUNT  10

#define AR9300_NUM_BT_WEIGHTS   4
#define AR9300_NUM_WLAN_WEIGHTS 2

/* Per Calibration data structure */
typedef struct per_cal_data {
    HAL_CAL_TYPES calType;           // Type of calibration
    u_int32_t     calNumSamples;     // Number of SW samples to collect
    u_int32_t     calCountMax;       // Number of HW samples to collect
    void (*calCollect)(struct ath_hal *, u_int8_t);  // Accumulator func
    void (*calPostProc)(struct ath_hal *, u_int8_t); // Post-processing func
} HAL_PERCAL_DATA;

/* List structure for calibration data */
typedef struct cal_list {
    const HAL_PERCAL_DATA  *calData;
    HAL_CAL_STATE          calState;
    struct cal_list        *calNext;
} HAL_CAL_LIST;

#define AR9300_NUM_CAL_TYPES        2
#define AR9300_PAPRD_TABLE_SZ       24
#define AR9300_PAPRD_GAIN_TABLE_SZ  32

/* Paprd tx power adjust data structure */
struct ar9300_paprd_pwr_adjust {
    u_int32_t     target_rate;     // rate index
    u_int32_t     reg_addr;        // register offset
    u_int32_t     reg_mask;        // mask of register
    u_int32_t     reg_mask_offset; // mask offset of register
    u_int32_t     sub_dB;          // offset value unit of dB
};

#define AR9300_MAX_RATES 36  /* legacy(4) + ofdm(8) + HTSS(8) + HTDS(8) + HTTS(8)*/

struct ath_hal_9300 {
    struct ath_hal_private_tables  ah_priv;    /* base class */

    /*
     * Information retrieved from EEPROM.
     */
    ar9300_eeprom_t  ah_eeprom;

    GAIN_VALUES ah_gainValues;

    u_int8_t    ah_macaddr[IEEE80211_ADDR_LEN];
    u_int8_t    ah_bssid[IEEE80211_ADDR_LEN];
    u_int8_t    ah_bssidmask[IEEE80211_ADDR_LEN];
    u_int16_t   ah_assocId;

    int16_t     ah_curchanRadIndex; /* cur. channel radar index */

    /*
     * Runtime state.
     */
    u_int32_t   ah_maskReg;         /* copy of AR_IMR */
    u_int32_t   ah_msiReg;          /* copy of AR_PCIE_MSI */
    os_atomic_t ah_ier_refcount;    /* reference count for enabling interrupts */
    struct ar9300Stats ah_stats;        /* various statistics */
    RF_HAL_FUNCS    ah_rfHal;
    u_int32_t   ah_txDescMask;      /* mask for TXDESC */
    u_int32_t   ah_txOkInterruptMask;
    u_int32_t   ah_txErrInterruptMask;
    u_int32_t   ah_txDescInterruptMask;
    u_int32_t   ah_txEolInterruptMask;
    u_int32_t   ah_txUrnInterruptMask;
    HAL_TX_QUEUE_INFO ah_txq[HAL_NUM_TX_QUEUES];
    HAL_POWER_MODE  ah_powerMode;
    HAL_SMPS_MODE   ah_smPowerMode;
    HAL_BOOL    ah_chipFullSleep;
    u_int32_t   ah_atimWindow;
    HAL_ANT_SETTING ah_diversityControl;    /* antenna setting */
    u_int16_t   ah_antennaSwitchSwap;       /* Controls mapping of OID request */
    u_int8_t    ah_tx_chainmask_cfg;        /* chain mask config */
    u_int8_t    ah_rx_chainmask_cfg;
    /* Calibration related fields */
    HAL_CAL_TYPES ah_suppCals;
    HAL_CAL_LIST  ah_iqCalData;         /* IQ Cal Data */
    HAL_CAL_LIST  ah_tempCompCalData;   /* Temperature Compensation Cal Data */
    HAL_CAL_LIST  *ah_cal_list;         /* ptr to first cal in list */
    HAL_CAL_LIST  *ah_cal_list_last;    /* ptr to last cal in list */
    HAL_CAL_LIST  *ah_cal_list_curr;    /* ptr to current cal */
// IQ Cal aliases
#define ah_totalPowerMeasI ah_Meas0.unsign
#define ah_totalPowerMeasQ ah_Meas1.unsign
#define ah_totalIqCorrMeas ah_Meas2.sign
    union {
        u_int32_t   unsign[AR9300_MAX_CHAINS];
        int32_t     sign[AR9300_MAX_CHAINS];
    } ah_Meas0;
    union {
        u_int32_t   unsign[AR9300_MAX_CHAINS];
        int32_t     sign[AR9300_MAX_CHAINS];
    } ah_Meas1;
    union {
        u_int32_t   unsign[AR9300_MAX_CHAINS];
        int32_t     sign[AR9300_MAX_CHAINS];
    } ah_Meas2;
    union {
        u_int32_t   unsign[AR9300_MAX_CHAINS];
        int32_t     sign[AR9300_MAX_CHAINS];
    } ah_Meas3;
    u_int16_t   ah_CalSamples;
    /* end - Calibration related fields */
    u_int32_t   ah_tx6PowerInHalfDbm;   /* power output for 6Mb tx */
    u_int32_t   ah_staId1Defaults;  /* STA_ID1 default settings */
    u_int32_t   ah_miscMode;        /* MISC_MODE settings */
    HAL_BOOL    ah_getPlcpHdr;      /* setting about MISC_SEL_EVM */
    enum {
        AUTO_32KHZ,     /* use it if 32kHz crystal present */
        USE_32KHZ,      /* do it regardless */
        DONT_USE_32KHZ,     /* don't use it regardless */
    } ah_enable32kHzClock;          /* whether to sleep at 32kHz */

    u_int32_t   ah_ofdmTxPower;
    int16_t     ah_txPowerIndexOffset;

    u_int       ah_slottime;        /* user-specified slot time */
    u_int       ah_acktimeout;      /* user-specified ack timeout */
    u_int       ah_ctstimeout;      /* user-specified cts timeout */
    u_int       ah_globaltxtimeout; /* user-specified global tx timeout */
    /*
     * XXX
     * 11g-specific stuff; belongs in the driver.
     */
    u_int8_t    ah_gBeaconRate;     /* fixed rate for G beacons */
    u_int32_t   ah_gpioMask;        /* copy of enabled GPIO mask */
    /*
     * RF Silent handling; setup according to the EEPROM.
     */
    u_int32_t   ah_gpioSelect;      /* GPIO pin to use */
    u_int32_t   ah_polarity;        /* polarity to disable RF */
    u_int32_t   ah_gpioBit;     /* after init, prev value */
    HAL_BOOL    ah_eepEnabled;      /* EEPROM bit for capability */

#ifdef ATH_BT_COEX
    /*
     * Bluetooth coexistence static setup according to the registry
     */
    HAL_BT_MODULE ah_btModule;           /* Bluetooth module identifier */
    u_int8_t    ah_btCoexConfigType;         /* BT coex configuration */
    u_int8_t    ah_btActiveGpioSelect;   /* GPIO pin for BT_ACTIVE */
    u_int8_t    ah_btPriorityGpioSelect; /* GPIO pin for BT_PRIORITY */
    u_int8_t    ah_wlanActiveGpioSelect; /* GPIO pin for WLAN_ACTIVE */
    u_int8_t    ah_btActivePolarity;     /* Polarity of BT_ACTIVE */
    HAL_BOOL    ah_btCoexSingleAnt;      /* Single or dual antenna configuration */
    u_int8_t    ah_btWlanIsolation;      /* Isolation between BT and WLAN in dB */
    /*
     * Bluetooth coexistence runtime settings
     */
    HAL_BOOL    ah_btCoexEnabled;        /* If Bluetooth coexistence is enabled */
    u_int32_t   ah_btCoexMode;           /* Register setting for AR_BT_COEX_MODE */
    u_int32_t   ah_btCoexBTWeight[AR9300_NUM_BT_WEIGHTS];     /* Register setting for AR_BT_COEX_WEIGHT */
    u_int32_t   ah_btCoexWLANWeight[AR9300_NUM_WLAN_WEIGHTS]; /* Register setting for AR_BT_COEX_WEIGHT */
    u_int32_t   ah_btCoexMode2;          /* Register setting for AR_BT_COEX_MODE2 */
    u_int32_t   ah_btCoexFlag;           /* Special tuning flags for BT coex */
#endif

    /*
     * Generic timer support
     */
    u_int32_t   ah_availGenTimers;       /* mask of available timers */
    u_int32_t   ah_intrGenTimerTrigger;  /* generic timer trigger interrupt state */
    u_int32_t   ah_intrGenTimerThresh;   /* generic timer trigger interrupt state */
    HAL_BOOL    ah_enableTSF2;           /* enable TSF2 for gen timer 8-15. */

    /*
     * ANI & Radar support.
     */
    u_int32_t   ah_procPhyErr;      /* Process Phy errs */
    HAL_BOOL    ah_hasHwPhyCounters;    /* Hardware has phy counters */
    u_int32_t   ah_aniPeriod;       /* ani update list period */
    struct ar9300AniState   *ah_curani; /* cached last reference */
    struct ar9300AniState   ah_ani[255]; /* per-channel state */
    struct ar9300RadarState ah_radar[HAL_NUMRADAR_STATES];  /* Per-Channel Radar detector state */
    struct ar9300RadarQElem *ah_radarq; /* radar event queue */
    struct ar9300RadarQInfo ah_radarqInfo;  /* radar event q read/write state */
    struct ar9300ArState    ah_ar;      /* AR detector state */
    struct ar9300RadarQElem *ah_arq;    /* AR event queue */
    struct ar9300RadarQInfo ah_arqInfo; /* AR event q read/write state */

    /*
     * Ani tables that change between the 9300 and 5312.
     * These get set at attach time.
     * XXX don't belong here
     * XXX need better explanation
     */
    int     ah_totalSizeDesired[5];
    int     ah_coarseHigh[5];
    int     ah_coarseLow[5];
    int     ah_firpwr[5];

    /*
     * Transmit power state.  Note these are maintained
     * here so they can be retrieved by diagnostic tools.
     */
    u_int16_t   ah_ratesArray[16];

    /*
     * Tx queue interrupt state.
     */
    u_int32_t   ah_intrTxqs;

    HAL_BOOL    ah_intrMitigationRx; /* rx Interrupt Mitigation Settings */
    HAL_BOOL    ah_intrMitigationTx; /* tx Interrupt Mitigation Settings */

    /*
     * Extension Channel Rx Clear State
     */
    u_int32_t   ah_cycleCount;
    u_int32_t   ah_ctlBusy;
    u_int32_t   ah_extBusy;

    /* HT CWM state */
    HAL_HT_EXTPROTSPACING ah_extprotspacing;
    u_int8_t    ah_txchainmask; /* tx chain mask */
    u_int8_t    ah_rxchainmask; /* rx chain mask */

    int         ah_hwp;
    void        *ah_cal_mem;
    HAL_BOOL    ah_emu_eeprom;

    HAL_ANI_CMD ah_ani_function;
    HAL_BOOL    ah_rifs_enabled;
    u_int32_t   ah_rifs_reg[11];
    u_int32_t   ah_rifs_sec_cnt;

    /* open-loop power control */
    u_int32_t originalGain[22];
    int32_t   initPDADC;
    int32_t   PDADCdelta;

    /* cycle counts for beacon stuck diagnostics */
    u_int32_t   ah_cycles;
    u_int32_t   ah_rx_clear;
    u_int32_t   ah_rx_frame;
    u_int32_t   ah_tx_frame;

#define BB_HANG_SIG1 0
#define BB_HANG_SIG2 1
#define BB_HANG_SIG3 2
#define BB_HANG_SIG4 3
#define MAC_HANG_SIG1 4
#define MAC_HANG_SIG2 5
    /* bb hang detection */
    int		ah_hang[6];
    hal_hw_hangs_t  ah_hang_wars;
    /*
     * Support for ar9300 multiple INIs
     */
    struct ar9300IniArray ah_iniPcieSerdes;
    struct ar9300IniArray ah_iniPcieSerdesLowPower;
    struct ar9300IniArray ah_iniModesAdditional;
    struct ar9300IniArray ah_iniModesAdditional_40M;
    struct ar9300IniArray ah_iniModesRxgain;
    struct ar9300IniArray ah_iniModesTxgain;
    struct ar9300IniArray ah_iniJapan2484;
    /* 
     * New INI format starting with Osprey 2.0 INI.
     * Pre, core, post arrays for each sub-system (mac, bb, radio, soc)
     */
    #define ATH_INI_PRE     0
    #define ATH_INI_CORE    1
    #define ATH_INI_POST    2
    #define ATH_INI_NUM_SPLIT   (ATH_INI_POST + 1)
    struct ar9300IniArray ah_iniMac[ATH_INI_NUM_SPLIT];     /* New INI format */
    struct ar9300IniArray ah_iniBB[ATH_INI_NUM_SPLIT];      /* New INI format */
    struct ar9300IniArray ah_iniRadio[ATH_INI_NUM_SPLIT];   /* New INI format */
    struct ar9300IniArray ah_iniSOC[ATH_INI_NUM_SPLIT];     /* New INI format */


    /* 
     * Added to support DFS postamble array in INI that we need to apply
     * in DFS channels
     */

    struct ar9300IniArray ah_ini_dfs;
#ifdef AR9300_EMULATION
    /* Common to both old and new INI formats */
    struct ar9300IniArray ah_iniRxGainEmu;
    /* 
     * New INI format starting with Osprey 2.0 INI.
     * Pre, core, post arrays for each sub-system.
     */
    struct ar9300IniArray ah_iniMacEmu[ATH_INI_NUM_SPLIT];     /* New INI format */
    struct ar9300IniArray ah_iniBBEmu[ATH_INI_NUM_SPLIT];      /* New INI format */
    struct ar9300IniArray ah_iniRadioEmu[ATH_INI_NUM_SPLIT];   /* New INI format */
    struct ar9300IniArray ah_iniSOCEmu[ATH_INI_NUM_SPLIT];     /* New INI format */
#endif
#if ATH_WOW
    struct ar9300IniArray ah_iniPcieSerdesWow;  /* SerDes values during WOW sleep */
#endif

    /* To indicate EEPROM mapping used */
    u_int32_t ah_immunity_vals[6];
    HAL_BOOL ah_immunity_on;
    /*
     * snap shot of counter register for debug purposes
     */
#ifdef AH_DEBUG
    u_int32_t lasttf;
    u_int32_t lastrf;
    u_int32_t lastrc;
    u_int32_t lastcc;
#endif
    HAL_BOOL    ah_dma_stuck; /* Set to true when RX/TX DMA failed to stop. */
    u_int32_t   nf_tsf32; /* timestamp for NF calibration duration */

    u_int32_t  regDmn;                 /* Regulatory Domain */
    int16_t    twiceAntennaGain;       /* Antenna Gain */
    u_int16_t  twiceAntennaReduction;  /* Antenna Gain Allowed */

    /*
     * Upper limit after factoring in the regulatory max, antenna gain and 
     * multichain factor. No TxBF, CDD or STBC gain factored 
     */
    int16_t upperLimit[AR9300_MAX_CHAINS]; 

    /* adjusted power for descriptor-based TPC for 1, 2, or 3 chains */
    int16_t txPower[AR9300_MAX_RATES][AR9300_MAX_CHAINS];

#ifdef  ATH_SUPPORT_TxBF
    /* adjusted power for descriptor-based TPC for 1, 2, or 3 chains with TxBF*/
    int16_t txPower_txbf[AR9300_MAX_RATES][AR9300_MAX_CHAINS];
#endif

    /* adjusted power for descriptor-based TPC for 1, 2, or 3 chains with STBC*/
    int16_t txPower_stbc[AR9300_MAX_RATES][AR9300_MAX_CHAINS];

    /* Transmit Status ring support */
    struct ar9300_txs    *ts_ring;
    u_int16_t            ts_tail;
    u_int16_t            ts_size;
    u_int32_t            ts_paddr_start;
    u_int32_t            ts_paddr_end;

    /* Receive Buffer size */
#define HAL_RXBUFSIZE_DEFAULT 0xfff
    u_int16_t            rxBufSize;

#ifdef  ATH_SUPPORT_TxBF
    HAL_TXBF_CAPS        txbf_caps;
#endif 
    u_int32_t            ah_WARegVal; // Store the permanent value of Reg 0x4004 so we dont have to R/M/W. (We should not be reading this register when in sleep states).

    /* Indicate the PLL source clock rate is 25Mhz or not.
     * clk_25mhz = 0 by default.
     */
    u_int8_t             clk_25mhz;
    /* For PAPRD uses */
    u_int16_t   smallSignalGain[AH_MAX_CHAINS];
    u_int32_t   PA_table[AH_MAX_CHAINS][AR9300_PAPRD_TABLE_SZ];
    u_int32_t   paprd_gain_table_entries[AR9300_PAPRD_GAIN_TABLE_SZ];
    u_int32_t   paprd_gain_table_index[AR9300_PAPRD_GAIN_TABLE_SZ];
    u_int32_t   ah_2G_papdRateMaskHt20; /* Copy of eep->modalHeader2G.papdRateMaskHt20 */ 
    u_int32_t   ah_2G_papdRateMaskHt40; /* Copy of eep->modalHeader2G.papdRateMaskHt40 */ 
    u_int32_t   ah_5G_papdRateMaskHt20; /* Copy of eep->modalHeader5G.papdRateMaskHt20 */ 
    u_int32_t   ah_5G_papdRateMaskHt40; /* Copy of eep->modalHeader5G.papdRateMaskHt40 */ 
    u_int32_t   paprd_training_power;
    /* For GreenTx use to store the default tx power */
    u_int8_t    ah_default_tx_power[ar9300RateSize];
   
    /* To store offsets of host interface registers */
    struct {
        u_int32_t AR_RC;
        u_int32_t AR_WA;
        u_int32_t AR_PM_STATE;
        u_int32_t AR_H_INFOL;
        u_int32_t AR_H_INFOH;
        u_int32_t AR_PCIE_PM_CTRL;
        u_int32_t AR_HOST_TIMEOUT;
        u_int32_t AR_EEPROM;
        u_int32_t AR_SREV;
        u_int32_t AR_INTR_SYNC_CAUSE;
        u_int32_t AR_INTR_SYNC_CAUSE_CLR;
        u_int32_t AR_INTR_SYNC_ENABLE;
        u_int32_t AR_INTR_ASYNC_MASK;
        u_int32_t AR_INTR_SYNC_MASK;
        u_int32_t AR_INTR_ASYNC_CAUSE_CLR;
        u_int32_t AR_INTR_ASYNC_CAUSE;
        u_int32_t AR_INTR_ASYNC_ENABLE;
        u_int32_t AR_PCIE_SERDES;
        u_int32_t AR_PCIE_SERDES2;
        u_int32_t AR_GPIO_OUT;
        u_int32_t AR_GPIO_IN;
        u_int32_t AR_GPIO_OE_OUT;
        u_int32_t AR_GPIO_OE1_OUT;
        u_int32_t AR_GPIO_INTR_POL;
        u_int32_t AR_GPIO_INPUT_EN_VAL;
        u_int32_t AR_GPIO_INPUT_MUX1;
        u_int32_t AR_GPIO_INPUT_MUX2;
        u_int32_t AR_GPIO_OUTPUT_MUX1;
        u_int32_t AR_GPIO_OUTPUT_MUX2;
        u_int32_t AR_GPIO_OUTPUT_MUX3;
        u_int32_t AR_INPUT_STATE;
        u_int32_t AR_SPARE;
        u_int32_t AR_PCIE_CORE_RESET_EN;
        u_int32_t AR_CLKRUN;
        u_int32_t AR_EEPROM_STATUS_DATA;
        u_int32_t AR_OBS;
        u_int32_t AR_RFSILENT;
        u_int32_t AR_GPIO_PDPU;
        u_int32_t AR_GPIO_DS;
        u_int32_t AR_MISC;
        u_int32_t AR_PCIE_MSI;
        u_int32_t AR_TSF_SNAPSHOT_BT_ACTIVE;
        u_int32_t AR_TSF_SNAPSHOT_BT_PRIORITY;
        u_int32_t AR_TSF_SNAPSHOT_BT_CNTL;
        u_int32_t AR_PCIE_PHY_LATENCY_NFTS_ADJ;
        u_int32_t AR_TDMA_CCA_CNTL;
        u_int32_t AR_TXAPSYNC;
        u_int32_t AR_TXSYNC_INIT_SYNC_TMR;
        u_int32_t AR_INTR_PRIO_SYNC_CAUSE;
        u_int32_t AR_INTR_PRIO_SYNC_ENABLE;
        u_int32_t AR_INTR_PRIO_ASYNC_MASK;
        u_int32_t AR_INTR_PRIO_SYNC_MASK;
        u_int32_t AR_INTR_PRIO_ASYNC_CAUSE;
        u_int32_t AR_INTR_PRIO_ASYNC_ENABLE;
    } ah_hostifregs;
    u_int32_t ah_enterprise_mode;
    u_int32_t ah_radar1;
    u_int32_t ah_dc_offset;
    u_int32_t ah_disable_cck;
    u_int8_t  ah_force_power_on_reset;
    HAL_BOOL  ah_hw_green_tx_enable; /* 1:enalbe H/W Green Tx */
    HAL_BOOL  ah_smartantenna_enable; /* 1:enalbe H/W */

    /*
     * Different types of memory where the calibration data might be stored.
     * All types are searched in Ar9300EepromRestore() in the order flash, eeprom, otp.
     */
    int TryFromHost;
    int TryFlash;
    int TryEeprom;
    int TryOtp;
#ifdef ATH_CAL_NAND_FLASH
    int TryNand;
#endif

    /*
     * This is where we found the calibration data.
     */
    int CalibrationDataSource;
    int CalibrationDataSourceAddress;

    /*
     * This is where we look for the calibration data. must be set before ath_attach() is called
     */
    int CalibrationDataTry;
    int CalibrationDataTryAddress;
    u_int8_t            ah_cac_quiet_enabled;
};

#define AH9300(_ah) ((struct ath_hal_9300 *)(_ah))

#define IS_9300_EMU(ah) \
    (AH_PRIVATE(ah)->ah_devid == AR9300_DEVID_EMU_PCIE)

#define ar9300EepDataInFlash(_ah) \
    (!(AH_PRIVATE(_ah)->ah_flags & AH_USE_EEPROM))

#define IS_5GHZ_FAST_CLOCK_EN(_ah, _c) \
        (IS_CHAN_5GHZ(_c) && \
        ((AH_PRIVATE(_ah))->ah_config.ath_hal_fastClockEnable))

#if notyet
// Need these additional conditions for IS_5GHZ_FAST_CLOCK_EN when we have valid eeprom contents.
&& \
        ((ar9300EepromGet(AH9300(_ah), EEP_MINOR_REV) <= AR9300_EEP_MINOR_VER_16) || \
        (ar9300EepromGet(AH9300(_ah), EEP_FSTCLK_5G))))
#endif

/*
 * WAR for bug 6773.  OS_DELAY() does a PIO READ on the PCI bus which allows
 * other cards' DMA reads to complete in the middle of our reset.
 */
#define WAR_6773(x) do {                \
        if ((++(x) % 64) == 0)          \
                OS_DELAY(1);            \
} while (0)

#define REG_WRITE_ARRAY(iniarray, column, regWr) do {                   \
        int r;                                                          \
        for (r = 0; r < ((iniarray)->ia_rows); r++) {    \
                OS_REG_WRITE(ah, INI_RA((iniarray), (r), 0), INI_RA((iniarray), r, (column)));\
                WAR_6773(regWr);                                        \
        }                                                               \
} while (0)

#define UPPER_5G_SUB_BANDSTART 5700
#define MID_5G_SUB_BANDSTART 5400
#define TRAINPOWER_DB_OFFSET 6 

#define AH_PAPRD_GET_SCALE_FACTOR(_scale, _eep, _is2G, _channel) do{ if(_is2G) { _scale = (_eep->modalHeader2G.papdRateMaskHt20>>25)&0x7; \
                                                                } else { \
                                                                    if(_channel >= UPPER_5G_SUB_BANDSTART){ _scale = (_eep->modalHeader5G.papdRateMaskHt20>>25)&0x7;} \
                                                                    else if((UPPER_5G_SUB_BANDSTART < _channel) && (_channel >= MID_5G_SUB_BANDSTART)) \
                                                                        { _scale = (_eep->modalHeader5G.papdRateMaskHt40>>28)&0x7;} \
                                                                        else { _scale = (_eep->modalHeader5G.papdRateMaskHt40>>25)&0x7;} } }while(0)

#ifdef AH_ASSERT
    #define ar9300FeatureNotSupported(feature, ah, func)    \
        ath_hal_printf(ah, # feature                        \
            " not supported but called from %s\n", (func)), \
        hal_assert(0)
#else
    #define ar9300FeatureNotSupported(feature, ah, func)    \
        ath_hal_printf(ah, # feature                        \
            " not supported but called from %s\n", (func))
#endif /* AH_ASSERT */


/*
 * Green Tx, Based on different RSSI of Received Beacon thresholds, 
 * using different tx power by modified register tx power related values.
 * The thresholds are decided by system team.
 */
#define WB225_SW_GREEN_TX_THRES1_DB              56  /* in dB */
#define WB225_SW_GREEN_TX_THRES2_DB              41  /* in dB */
#define WB225_OB_CALIBRATION_VALUE               5   /* For Green Tx OLPC Delta
                                                        Calibration Offset */
#define WB225_OB_GREEN_TX_SHORT_VALUE            1   /* For Green Tx OB value
                                                        in short distance*/
#define WB225_OB_GREEN_TX_MIDDLE_VALUE           3   /* For Green Tx OB value
                                                        in middle distance */
#define WB225_OB_GREEN_TX_LONG_VALUE             5   /* For Green Tx OB value
                                                        in long distance */
#define WB225_BBPWRTXRATE9_SW_GREEN_TX_SHORT_VALUE  0x06060606 /* For SwGreen Tx 
                                                        BB_powertx_rate9 reg
                                                        value in short 
                                                        distance */
#define WB225_BBPWRTXRATE9_SW_GREEN_TX_MIDDLE_VALUE 0x0E0E0E0E /* For SwGreen Tx 
                                                        BB_powertx_rate9 reg
                                                        value in middle 
                                                        distance */


/* Tx power for short distacnce in SwGreenTx.*/
static const u_int8_t wb225_sw_gtx_tp_distance_short[ar9300RateSize] = {
        6,  /*ALL_TARGET_LEGACY_6_24*/
        6,  /*ALL_TARGET_LEGACY_36*/
        6,  /*ALL_TARGET_LEGACY_48*/
        4,  /*ALL_TARGET_LEGACY_54*/
        6,  /*ALL_TARGET_LEGACY_1L_5L*/
        6,  /*ALL_TARGET_LEGACY_5S*/
        6,  /*ALL_TARGET_LEGACY_11L*/
        6,  /*ALL_TARGET_LEGACY_11S*/
        6,  /*ALL_TARGET_HT20_0_8_16*/
        6,  /*ALL_TARGET_HT20_1_3_9_11_17_19*/
        4,  /*ALL_TARGET_HT20_4*/
        4,  /*ALL_TARGET_HT20_5*/
        4,  /*ALL_TARGET_HT20_6*/
        2,  /*ALL_TARGET_HT20_7*/
        0,  /*ALL_TARGET_HT20_12*/
        0,  /*ALL_TARGET_HT20_13*/
        0,  /*ALL_TARGET_HT20_14*/
        0,  /*ALL_TARGET_HT20_15*/
        0,  /*ALL_TARGET_HT20_20*/
        0,  /*ALL_TARGET_HT20_21*/
        0,  /*ALL_TARGET_HT20_22*/
        0,  /*ALL_TARGET_HT20_23*/
        6,  /*ALL_TARGET_HT40_0_8_16*/
        6,  /*ALL_TARGET_HT40_1_3_9_11_17_19*/
        4,  /*ALL_TARGET_HT40_4*/
        4,  /*ALL_TARGET_HT40_5*/
        4,  /*ALL_TARGET_HT40_6*/
        2,  /*ALL_TARGET_HT40_7*/
        0,  /*ALL_TARGET_HT40_12*/
        0,  /*ALL_TARGET_HT40_13*/
        0,  /*ALL_TARGET_HT40_14*/
        0,  /*ALL_TARGET_HT40_15*/
        0,  /*ALL_TARGET_HT40_20*/
        0,  /*ALL_TARGET_HT40_21*/
        0,  /*ALL_TARGET_HT40_22*/
        0   /*ALL_TARGET_HT40_23*/
};

/* Tx power for middle distacnce in SwGreenTx.*/
static const u_int8_t wb225_sw_gtx_tp_distance_middle[ar9300RateSize] =  {
        14, /*ALL_TARGET_LEGACY_6_24*/
        14, /*ALL_TARGET_LEGACY_36*/
        14, /*ALL_TARGET_LEGACY_48*/
        12, /*ALL_TARGET_LEGACY_54*/
        14, /*ALL_TARGET_LEGACY_1L_5L*/
        14, /*ALL_TARGET_LEGACY_5S*/
        14, /*ALL_TARGET_LEGACY_11L*/
        14, /*ALL_TARGET_LEGACY_11S*/
        14, /*ALL_TARGET_HT20_0_8_16*/
        14, /*ALL_TARGET_HT20_1_3_9_11_17_19*/
        14, /*ALL_TARGET_HT20_4*/
        14, /*ALL_TARGET_HT20_5*/
        12, /*ALL_TARGET_HT20_6*/
        10, /*ALL_TARGET_HT20_7*/
        0,  /*ALL_TARGET_HT20_12*/
        0,  /*ALL_TARGET_HT20_13*/
        0,  /*ALL_TARGET_HT20_14*/
        0,  /*ALL_TARGET_HT20_15*/
        0,  /*ALL_TARGET_HT20_20*/
        0,  /*ALL_TARGET_HT20_21*/
        0,  /*ALL_TARGET_HT20_22*/
        0,  /*ALL_TARGET_HT20_23*/
        14, /*ALL_TARGET_HT40_0_8_16*/
        14, /*ALL_TARGET_HT40_1_3_9_11_17_19*/
        14, /*ALL_TARGET_HT40_4*/
        14, /*ALL_TARGET_HT40_5*/
        12, /*ALL_TARGET_HT40_6*/
        10, /*ALL_TARGET_HT40_7*/
        0,  /*ALL_TARGET_HT40_12*/
        0,  /*ALL_TARGET_HT40_13*/
        0,  /*ALL_TARGET_HT40_14*/
        0,  /*ALL_TARGET_HT40_15*/
        0,  /*ALL_TARGET_HT40_20*/
        0,  /*ALL_TARGET_HT40_21*/
        0,  /*ALL_TARGET_HT40_22*/
        0   /*ALL_TARGET_HT40_23*/
};

/* OLPC DeltaCalibration Offset unit in half dB.*/
static const u_int8_t wb225_gtx_olpc_cal_offset[6] =  {
        0,  /* OB0*/
        16, /* OB1*/
        9,  /* OB2*/
        5,  /* OB3*/
        2,  /* OB4*/
        0,  /* OB5*/
};

/*
 * Definitions for HwGreenTx
 */
#define AR9485_HW_GREEN_TX_THRES1_DB              56  /* in dB */
#define AR9485_HW_GREEN_TX_THRES2_DB              41  /* in dB */
#define AR9485_BBPWRTXRATE9_HW_GREEN_TX_SHORT_VALUE 0x0C0C0A0A /* For HwGreen Tx 
                                                        BB_powertx_rate9 reg
                                                        value in short 
                                                        distance */
#define AR9485_BBPWRTXRATE9_HW_GREEN_TX_MIDDLE_VALUE 0x10100E0E /* For HwGreenTx 
                                                        BB_powertx_rate9 reg
                                                        value in middle 
                                                        distance */

/* Tx power for short distacnce in HwGreenTx.*/
static const u_int8_t ar9485_hw_gtx_tp_distance_short[ar9300RateSize] = {
        14, /*ALL_TARGET_LEGACY_6_24*/
        14, /*ALL_TARGET_LEGACY_36*/
        8,  /*ALL_TARGET_LEGACY_48*/
        2,  /*ALL_TARGET_LEGACY_54*/
        14, /*ALL_TARGET_LEGACY_1L_5L*/
        14, /*ALL_TARGET_LEGACY_5S*/
        14, /*ALL_TARGET_LEGACY_11L*/
        14, /*ALL_TARGET_LEGACY_11S*/
        12, /*ALL_TARGET_HT20_0_8_16*/
        12, /*ALL_TARGET_HT20_1_3_9_11_17_19*/
        12, /*ALL_TARGET_HT20_4*/
        12, /*ALL_TARGET_HT20_5*/
        8,  /*ALL_TARGET_HT20_6*/
        2,  /*ALL_TARGET_HT20_7*/
        0,  /*ALL_TARGET_HT20_12*/
        0,  /*ALL_TARGET_HT20_13*/
        0,  /*ALL_TARGET_HT20_14*/
        0,  /*ALL_TARGET_HT20_15*/
        0,  /*ALL_TARGET_HT20_20*/
        0,  /*ALL_TARGET_HT20_21*/
        0,  /*ALL_TARGET_HT20_22*/
        0,  /*ALL_TARGET_HT20_23*/
        10, /*ALL_TARGET_HT40_0_8_16*/
        10, /*ALL_TARGET_HT40_1_3_9_11_17_19*/
        10, /*ALL_TARGET_HT40_4*/
        10, /*ALL_TARGET_HT40_5*/
        6,  /*ALL_TARGET_HT40_6*/
        2,  /*ALL_TARGET_HT40_7*/
        0,  /*ALL_TARGET_HT40_12*/
        0,  /*ALL_TARGET_HT40_13*/
        0,  /*ALL_TARGET_HT40_14*/
        0,  /*ALL_TARGET_HT40_15*/
        0,  /*ALL_TARGET_HT40_20*/
        0,  /*ALL_TARGET_HT40_21*/
        0,  /*ALL_TARGET_HT40_22*/
        0   /*ALL_TARGET_HT40_23*/
};

/* Tx power for middle distacnce in HwGreenTx.*/
static const u_int8_t ar9485_hw_gtx_tp_distance_middle[ar9300RateSize] =  {
        18, /*ALL_TARGET_LEGACY_6_24*/
        18, /*ALL_TARGET_LEGACY_36*/
        14, /*ALL_TARGET_LEGACY_48*/
        12, /*ALL_TARGET_LEGACY_54*/
        18, /*ALL_TARGET_LEGACY_1L_5L*/
        18, /*ALL_TARGET_LEGACY_5S*/
        18, /*ALL_TARGET_LEGACY_11L*/
        18, /*ALL_TARGET_LEGACY_11S*/
        16, /*ALL_TARGET_HT20_0_8_16*/
        16, /*ALL_TARGET_HT20_1_3_9_11_17_19*/
        16, /*ALL_TARGET_HT20_4*/
        16, /*ALL_TARGET_HT20_5*/
        14, /*ALL_TARGET_HT20_6*/
        12, /*ALL_TARGET_HT20_7*/
        0,  /*ALL_TARGET_HT20_12*/
        0,  /*ALL_TARGET_HT20_13*/
        0,  /*ALL_TARGET_HT20_14*/
        0,  /*ALL_TARGET_HT20_15*/
        0,  /*ALL_TARGET_HT20_20*/
        0,  /*ALL_TARGET_HT20_21*/
        0,  /*ALL_TARGET_HT20_22*/
        0,  /*ALL_TARGET_HT20_23*/
        14, /*ALL_TARGET_HT40_0_8_16*/
        14, /*ALL_TARGET_HT40_1_3_9_11_17_19*/
        14, /*ALL_TARGET_HT40_4*/
        14, /*ALL_TARGET_HT40_5*/
        14, /*ALL_TARGET_HT40_6*/
        12, /*ALL_TARGET_HT40_7*/
        0,  /*ALL_TARGET_HT40_12*/
        0,  /*ALL_TARGET_HT40_13*/
        0,  /*ALL_TARGET_HT40_14*/
        0,  /*ALL_TARGET_HT40_15*/
        0,  /*ALL_TARGET_HT40_20*/
        0,  /*ALL_TARGET_HT40_21*/
        0,  /*ALL_TARGET_HT40_22*/
        0   /*ALL_TARGET_HT40_23*/
};

/* MIMO Modes used in TPC calculations */
typedef enum {
    AR9300_DEF_MODE = 0, /* Could be CDD or Direct */
    AR9300_TXBF_MODE,        
    AR9300_STBC_MODE
} AR9300_TXMODES;

typedef enum {
    POSEIDON_STORED_REG_OBDB    = 0,    /* default OB/DB setting from ini */
    POSEIDON_STORED_REG_TPC     = 1,    /* default txpower value in TPC reg */
    POSEIDON_STORED_REG_BB_PWRTX_RATE9 = 2, /* default txpower value in 
                                             *  BB_powertx_rate9 reg 
                                             */
    POSEIDON_STORED_REG_SZ              /* Can not add anymore */
} POSEIDON_STORED_REGS;

typedef enum {
    POSEIDON_STORED_REG_G2_OLPC_OFFSET  = 0,/* default OB/DB setting from ini */
    POSEIDON_STORED_REG_G2_SZ               /* should not exceed 3 */
} POSEIDON_STORED_REGS_G2;
extern void ar9300Detach(struct ath_hal *ah);
extern void ar9300GetDescInfo(struct ath_hal *ah, HAL_DESC_INFO *desc_info);

#if AH_NEED_TX_DATA_SWAP
#if AH_NEED_RX_DATA_SWAP
#define ar9300_init_cfg_reg(ah) OS_REG_RMW(ah, AR_CFG, AR_CFG_SWTB | AR_CFG_SWRB,0)
#else
#define ar9300_init_cfg_reg(ah) OS_REG_RMW(ah, AR_CFG, AR_CFG_SWTB,0)
#endif
#elif AH_NEED_RX_DATA_SWAP
#define ar9300_init_cfg_reg(ah) OS_REG_RMW(ah, AR_CFG, AR_CFG_SWRB,0)
#else
#define ar9300_init_cfg_reg(ah) OS_REG_RMW(ah, AR_CFG, AR_CFG_SWTD | AR_CFG_SWRD,0)
#endif

extern  HAL_BOOL ar9300RfAttach(struct ath_hal *, HAL_STATUS *);

struct ath_hal;

extern  u_int32_t ar9300RadioAttach(struct ath_hal *ah);
extern  struct ath_hal_9300 * ar9300NewState(u_int16_t devid,
        HAL_ADAPTER_HANDLE osdev, HAL_SOFTC sc,
        HAL_BUS_TAG st, HAL_BUS_HANDLE sh, HAL_BUS_TYPE bustype,
        asf_amem_instance_handle amem_handle,
        struct hal_reg_parm *hal_conf_parm, HAL_STATUS *status);
extern  struct ath_hal * ar9300Attach(u_int16_t devid,
        HAL_ADAPTER_HANDLE osdev, HAL_SOFTC sc,
        HAL_BUS_TAG st, HAL_BUS_HANDLE sh, HAL_BUS_TYPE bustype,
        asf_amem_instance_handle amem_handle,
        struct hal_reg_parm *hal_conf_parm,
        HAL_STATUS *status);
extern  void ar9300Detach(struct ath_hal *ah);
extern void ar9300ReadRevisions(struct ath_hal *ah);
extern  HAL_BOOL ar9300ChipTest(struct ath_hal *ah);
extern  HAL_BOOL ar9300GetChannelEdges(struct ath_hal *ah,
                u_int16_t flags, u_int16_t *low, u_int16_t *high);
extern  HAL_BOOL ar9300FillCapabilityInfo(struct ath_hal *ah);

extern  void ar9300BeaconInit(struct ath_hal *ah,
                              u_int32_t next_beacon, u_int32_t beacon_period, HAL_OPMODE opmode);
extern  HAL_BOOL ar9300WaitForBeaconDone(struct ath_hal *, HAL_BUS_ADDR baddr);
extern  void ar9300ResetStaBeaconTimers(struct ath_hal *ah);
extern  void ar9300SetStaBeaconTimers(struct ath_hal *ah,
        const HAL_BEACON_STATE *);
extern  int ar9300UpdateNavReg(struct ath_hal *ah);
extern  HAL_BOOL ar9300IsInterruptPending(struct ath_hal *ah);
extern  HAL_BOOL ar9300GetPendingInterrupts(struct ath_hal *ah, HAL_INT *, HAL_INT_TYPE, u_int8_t, HAL_BOOL);
extern  HAL_INT ar9300GetInterrupts(struct ath_hal *ah);
extern  HAL_INT ar9300SetInterrupts(struct ath_hal *ah, HAL_INT ints, HAL_BOOL);
extern  void ar9300SetIntrMitigationTimer(struct ath_hal* ah,
        HAL_INT_MITIGATION reg, u_int32_t value);
extern  u_int32_t ar9300GetIntrMitigationTimer(struct ath_hal* ah,
        HAL_INT_MITIGATION reg);
extern  u_int32_t ar9300GetKeyCacheSize(struct ath_hal *);
extern  HAL_BOOL ar9300IsKeyCacheEntryValid(struct ath_hal *, u_int16_t entry);
extern  HAL_BOOL ar9300ResetKeyCacheEntry(struct ath_hal *ah, u_int16_t entry);
extern  HAL_BOOL ar9300SetKeyCacheEntryMac(struct ath_hal *,
            u_int16_t entry, const u_int8_t *mac);
extern  HAL_BOOL ar9300SetKeyCacheEntry(struct ath_hal *ah, u_int16_t entry,
                       const HAL_KEYVAL *k, const u_int8_t *mac, int xorKey);
extern  HAL_BOOL ar9300PrintKeyCache(struct ath_hal *ah);

extern  void ar9300GetMacAddress(struct ath_hal *ah, u_int8_t *mac);
extern  HAL_BOOL ar9300SetMacAddress(struct ath_hal *ah, const u_int8_t *);
extern  void ar9300GetBssIdMask(struct ath_hal *ah, u_int8_t *mac);
extern  HAL_BOOL ar9300SetBssIdMask(struct ath_hal *, const u_int8_t *);
extern  HAL_STATUS ar9300SelectAntConfig(struct ath_hal *ah, u_int32_t cfg);
extern  int ar9300GetNumAntCfg( struct ath_hal *ah);
extern  HAL_BOOL ar9300SetRegulatoryDomain(struct ath_hal *ah,
                                    u_int16_t regDomain, HAL_STATUS *stats);
extern  u_int ar9300GetWirelessModes(struct ath_hal *ah);
extern  void ar9300EnableRfKill(struct ath_hal *);
extern  HAL_BOOL ar9300GpioCfgOutput(struct ath_hal *, u_int32_t gpio, HAL_GPIO_OUTPUT_MUX_TYPE signalType);
extern  HAL_BOOL ar9300GpioCfgInput(struct ath_hal *, u_int32_t gpio);
extern  HAL_BOOL ar9300GpioSet(struct ath_hal *, u_int32_t gpio, u_int32_t val);
extern  u_int32_t ar9300GpioGet(struct ath_hal *ah, u_int32_t gpio);
extern  void ar9300GpioSetIntr(struct ath_hal *ah, u_int, u_int32_t ilevel);
extern  void ar9300SetLedState(struct ath_hal *ah, HAL_LED_STATE state);
extern  void ar9300SetPowerLedState(struct ath_hal *ah, u_int8_t enable);
extern  void ar9300SetNetworkLedState(struct ath_hal *ah, u_int8_t enable);
extern  void ar9300WriteAssocid(struct ath_hal *ah, const u_int8_t *bssid,
        u_int16_t assocId);
extern  u_int32_t ar9300PpmGetRssiDump(struct ath_hal *);
extern  u_int32_t ar9300PpmArmTrigger(struct ath_hal *);
extern  int ar9300PpmGetTrigger(struct ath_hal *);
extern  u_int32_t ar9300PpmForce(struct ath_hal *);
extern  void ar9300PpmUnForce(struct ath_hal *);
extern  u_int32_t ar9300PpmGetForceState(struct ath_hal *);
extern  u_int32_t ar9300PpmGetForceState(struct ath_hal *);
extern  void ar9300SetDcsMode(struct ath_hal *ah, u_int32_t);
extern  u_int32_t ar9300GetDcsMode(struct ath_hal *ah);
extern  u_int32_t ar9300GetTsf32(struct ath_hal *ah);
extern  u_int64_t ar9300GetTsf64(struct ath_hal *ah);
extern  u_int32_t ar9300GetTsf2_32(struct ath_hal *ah);
extern  u_int64_t ar9300GetTsf2_64(struct ath_hal *ah);
extern  void ar9300SetTsf64(struct ath_hal *ah, u_int64_t tsf);
extern  void ar9300ResetTsf(struct ath_hal *ah);
extern  void ar9300SetBasicRate(struct ath_hal *ah, HAL_RATE_SET *pSet);
extern  u_int32_t ar9300GetRandomSeed(struct ath_hal *ah);
extern  HAL_BOOL ar9300DetectCardPresent(struct ath_hal *ah);
extern  void ar9300UpdateMibMacStats(struct ath_hal *ah);
extern  void ar9300GetMibMacStats(struct ath_hal *ah, HAL_MIB_STATS* stats);
extern  HAL_BOOL ar9300IsJapanChannelSpreadSupported(struct ath_hal *ah);
extern  u_int32_t ar9300GetCurRssi(struct ath_hal *ah);
extern  u_int ar9300GetDefAntenna(struct ath_hal *ah);
extern  void ar9300SetDefAntenna(struct ath_hal *ah, u_int antenna);
extern  HAL_BOOL ar9300SetAntennaSwitch(struct ath_hal *ah,
        HAL_ANT_SETTING settings, HAL_CHANNEL *chan, u_int8_t *, u_int8_t *, u_int8_t *);
extern  HAL_BOOL ar9300IsSleepAfterBeaconBroken(struct ath_hal *ah);
extern  HAL_BOOL ar9300SetSlotTime(struct ath_hal *, u_int);
extern  u_int ar9300GetSlotTime(struct ath_hal *);
extern  HAL_BOOL ar9300SetAckTimeout(struct ath_hal *, u_int);
extern  u_int ar9300GetAckTimeout(struct ath_hal *);
extern  HAL_BOOL ar9300SetEifsMask(struct ath_hal *, u_int);
extern  u_int ar9300GetEifsMask(struct ath_hal *);
extern  HAL_BOOL ar9300SetEifsDur(struct ath_hal *, u_int);
extern  u_int ar9300GetEifsDur(struct ath_hal *);
extern  HAL_BOOL ar9300SetCTSTimeout(struct ath_hal *, u_int);
extern  HAL_STATUS ar9300SetQuiet(struct ath_hal *ah,u_int16_t period, u_int16_t duration, u_int16_t nextStart, u_int16_t enabled);
extern  u_int ar9300GetCTSTimeout(struct ath_hal *);
extern  void ar9300SetPCUConfig(struct ath_hal *);
extern  HAL_STATUS ar9300GetCapability(struct ath_hal *, HAL_CAPABILITY_TYPE,
        u_int32_t, u_int32_t *);
extern  HAL_BOOL ar9300SetCapability(struct ath_hal *, HAL_CAPABILITY_TYPE,
        u_int32_t, u_int32_t, HAL_STATUS *);
extern  HAL_BOOL ar9300GetDiagState(struct ath_hal *ah, int request,
        const void *args, u_int32_t argsize,
        void **result, u_int32_t *resultsize);
extern void ar9300GetDescInfo(struct ath_hal *ah, HAL_DESC_INFO *desc_info);
extern  int8_t ar9300Get11nExtBusy(struct ath_hal *ah);
extern  void ar9300Set11nMac2040(struct ath_hal *ah, HAL_HT_MACMODE mode);
extern  HAL_HT_RXCLEAR ar9300Get11nRxClear(struct ath_hal *ah);
extern  void ar9300Set11nRxClear(struct ath_hal *ah, HAL_HT_RXCLEAR rxclear);
extern  HAL_BOOL ar9300SetPowerMode(struct ath_hal *ah, HAL_POWER_MODE mode,
        int setChip);
extern  HAL_POWER_MODE ar9300GetPowerMode(struct ath_hal *ah);
extern HAL_BOOL ar9300SetPowerModeAwake(struct ath_hal *ah, int setChip);
extern  void ar9300SetSmPowerMode(struct ath_hal *ah, HAL_SMPS_MODE mode);

extern void ar9300ConfigPciPowerSave(struct ath_hal *ah, int restore, int powerOff);

extern  void ar9300ForceTSFSync(struct ath_hal *ah, const u_int8_t *bssid, 
                                u_int16_t assocId);

#ifdef AR9300_EMULATION
extern  void ar9300InitMacTrace(struct ath_hal *ah);
extern  void ar9300StopMacTrace(struct ath_hal *ah);
#endif /* AR9300_EMULATION */

#if ATH_WOW
extern  void ar9300WowApplyPattern(struct ath_hal *ah, u_int8_t *pAthPattern,
        u_int8_t *pAthMask, int32_t pattern_count, u_int32_t athPatternLen);
extern  u_int32_t ar9300WowWakeUp(struct ath_hal *ah);
extern  HAL_BOOL ar9300WowEnable(struct ath_hal *ah, u_int32_t patternEnable, u_int32_t timeoutInSeconds, int clearbssid);
#endif

extern HAL_BOOL ar9300Reset(struct ath_hal *ah, HAL_OPMODE opmode,
        HAL_CHANNEL *chan, HAL_HT_MACMODE macmode, u_int8_t txchainmask,
        u_int8_t rxchainmask, HAL_HT_EXTPROTSPACING extprotspacing,
        HAL_BOOL bChannelChange, HAL_STATUS *status, int is_scan);

extern  HAL_BOOL ar9300SetResetReg(struct ath_hal *ah, u_int32_t type);
extern  void ar9300InitPLL(struct ath_hal *ah, HAL_CHANNEL *chan);
extern  void ar9300GreenApPsOnOff( struct ath_hal *ah, u_int16_t rxMask);
extern  u_int16_t ar9300IsSingleAntPowerSavePossible(struct ath_hal *ah);
extern  void ar9300SetOperatingMode(struct ath_hal *ah, int opmode);
extern  HAL_BOOL ar9300PhyDisable(struct ath_hal *ah);
extern  HAL_BOOL ar9300Disable(struct ath_hal *ah);
extern  HAL_BOOL ar9300ChipReset(struct ath_hal *ah, HAL_CHANNEL *);
extern  HAL_BOOL ar9300Calibration(struct ath_hal *ah,  HAL_CHANNEL *chan,
        u_int8_t rxchainmask, HAL_BOOL longcal, HAL_BOOL *isIQdone, int is_scan, u_int32_t *sched_cals);
extern  void ar9300ResetCalValid(struct ath_hal *ah,  HAL_CHANNEL *chan,
                                 HAL_BOOL *isIQdone, u_int32_t calType);
extern void ar9300IQCalCollect(struct ath_hal *ah, u_int8_t numChains);
extern void ar9300IQCalibration(struct ath_hal *ah, u_int8_t numChains);
extern void ar9300TempCompCalCollect(struct ath_hal *ah);
extern void ar9300TempCompCalibration(struct ath_hal *ah, u_int8_t numChains);
extern int16_t ar9300GetMinCCAPwr(struct ath_hal *ah);
extern void ar9300UploadNoiseFloor(struct ath_hal *ah, int is2G, int16_t nfarray[]);
extern void ar9300SetNominalUserNFVal(struct ath_hal *ah, int16_t val,
        HAL_BOOL is2GHz);
extern int16_t ar9300GetNominalUserNFVal(struct ath_hal *ah, HAL_BOOL is2GHz);
extern void ar9300SetMinUserNFVal(struct ath_hal *ah, int16_t val,
        HAL_BOOL is2GHz);
extern int16_t ar9300GetMinUserNFVal(struct ath_hal *ah, HAL_BOOL is2GHz);
extern void ar9300SetMaxUserNFVal(struct ath_hal *ah, int16_t val,
        HAL_BOOL is2GHz);
extern int16_t ar9300GetMaxUserNFVal(struct ath_hal *ah, HAL_BOOL is2GHz);
extern void ar9300SetNfDeltaVal(struct ath_hal *ah, int16_t val);
extern int16_t ar9300GetNfDeltaVal(struct ath_hal *ah);

extern HAL_BOOL ar9300SetTxPowerLimit(struct ath_hal *ah, u_int32_t limit,
                                       u_int16_t extra_txpow, u_int16_t tpcInDb);
extern void ar9300ChainNoiseFloor(struct ath_hal *ah, int16_t *nfBuf,
                                    HAL_CHANNEL *chan, int is_scan);

extern HAL_RFGAIN ar9300GetRfgain(struct ath_hal *ah);
extern const HAL_RATE_TABLE *ar9300GetRateTable(struct ath_hal *, u_int mode);
extern int16_t ar9300GetRateTxPower(struct ath_hal *ah, u_int mode,
                   u_int8_t rate_index, u_int8_t chainmask, u_int8_t mimo_mode);
extern void ar9300InitRateTxPower(struct ath_hal *ah, u_int mode,
                                   HAL_CHANNEL_INTERNAL *chan,
                                   u_int8_t powerPerRate[],
                                   u_int8_t chainmask);
extern void ar9300AdjustRegTxPowerCDD(struct ath_hal *ah, 
                                   u_int8_t powerPerRate[]);
extern void ar9300ResetTxStatusRing(struct ath_hal *ah);
extern  void ar9300EnableMIBCounters(struct ath_hal *);
extern  void ar9300DisableMIBCounters(struct ath_hal *);
extern  void ar9300AniAttach(struct ath_hal *);
extern  void ar9300AniDetach(struct ath_hal *);
extern  struct ar9300AniState *ar9300AniGetCurrentState(struct ath_hal *);
extern  struct ar9300Stats *ar9300AniGetCurrentStats(struct ath_hal *);
extern  HAL_BOOL ar9300AniControl(struct ath_hal *, HAL_ANI_CMD cmd, int param);
struct ath_rx_status;

extern  void ar9300ProcessMibIntr(struct ath_hal *, const HAL_NODE_STATS *);
extern  void ar9300AniArPoll(struct ath_hal *, const HAL_NODE_STATS *,
                 HAL_CHANNEL *, HAL_ANISTATS *);
extern  void ar9300AniReset(struct ath_hal *, HAL_BOOL is_scanning);
extern  void ar9300AniInitDefaults(struct ath_hal *ah, HAL_HT_MACMODE macmode);
extern  void ar9300EnableTPC(struct ath_hal *);

extern void ar9300RxGainTableApply(struct ath_hal *ah);
extern void ar9300TxGainTableApply(struct ath_hal *ah);

#ifdef AH_SUPPORT_AR9300
/* BB Panic Watchdog declarations */
#define HAL_BB_PANIC_WD_TMO                 25 /* in ms, 0 to disable */
#define HAL_BB_PANIC_WD_TMO_HORNET          85
extern void ar9300ConfigBbPanicWatchdog(struct ath_hal *);
extern void ar9300HandleBbPanic(struct ath_hal *);
extern int ar9300GetBbPanicInfo(struct ath_hal *ah, struct hal_bb_panic_info *bb_panic);
extern HAL_BOOL ar9300_handle_radar_bb_panic(struct ath_hal *ah);
#endif
extern void ar9300SetHalResetReason(struct ath_hal *ah, u_int8_t resetreason);

/* DFS declarations */

extern  void ar9300CheckDfs(struct ath_hal *ah, HAL_CHANNEL *chan);
extern  void ar9300DfsFound(struct ath_hal *ah, HAL_CHANNEL *chan, u_int64_t nolTime);
extern  void ar9300EnableDfs(struct ath_hal *ah, HAL_PHYERR_PARAM *pe);
extern  void ar9300GetDfsThresh(struct ath_hal *ah, HAL_PHYERR_PARAM *pe);
extern  HAL_BOOL ar9300RadarWait(struct ath_hal *ah, HAL_CHANNEL *chan);
extern struct dfs_pulse * ar9300GetDfsRadars(struct ath_hal *ah, u_int32_t dfsdomain, int *numradars, struct dfs_bin5pulse **bin5pulses, int *numb5radars, HAL_PHYERR_PARAM *pe);
extern HAL_CHANNEL* ar9300GetExtensionChannel(struct ath_hal *ah);
extern void ar9300MarkPhyInactive(struct ath_hal *ah);
extern HAL_BOOL ar9300IsFastClockEnabled(struct ath_hal *ah);
extern void ar9300_adjust_difs(struct ath_hal *ah, u_int32_t val);
extern void ar9300_dfs_cac_war(struct ath_hal *ah, u_int32_t start);
extern  void ar9300_cac_tx_quiet(struct ath_hal *ah, bool enable);
/* Spectral scan declarations */
extern void ar9300ConfigureSpectralScan(struct ath_hal *ah, HAL_SPECTRAL_PARAM *ss);
extern void ar9300SetCcaThreshold(struct ath_hal *ah, u_int8_t thresh62);
extern void ar9300ConfigFftPeriod(struct ath_hal *ah, u_int32_t inVal);
extern void ar9300ConfigScanPeriod(struct ath_hal *ah, u_int32_t inVal);
extern void ar9300ConfigScanCount(struct ath_hal *ah, u_int32_t inVal);
extern void ar9300ConfigShortRpt(struct ath_hal *ah, u_int32_t inVal);
extern void ar9300GetSpectralParams(struct ath_hal *ah, HAL_SPECTRAL_PARAM *ss);
extern HAL_BOOL ar9300IsSpectralActive(struct ath_hal *ah);
extern HAL_BOOL ar9300IsSpectralEnabled(struct ath_hal *ah);
extern void ar9300StartSpectralScan(struct ath_hal *ah);
extern void ar9300StopSpectralScan(struct ath_hal *ah);
extern u_int32_t ar9300SpectralScanCapable( struct ath_hal *ah);
extern u_int32_t ar9300GetSpectralConfig(struct ath_hal *ah);
extern void ar9300RestoreSpectralConfig(struct ath_hal *ah, u_int32_t restoreval);
int16_t ar9300GetCtlChanNF(struct ath_hal *ah);
int16_t ar9300GetExtChanNF(struct ath_hal *ah);
/* End spectral scan declarations */

/* Raw ADC capture functions */
extern void ar9300EnableTestAddacMode(struct ath_hal *ah);
extern void ar9300DisableTestAddacMode(struct ath_hal *ah);
extern void ar9300BeginAdcCapture(struct ath_hal *ah, int auto_agc_gain);
extern HAL_STATUS ar9300RetrieveCaptureData(struct ath_hal *ah, u_int16_t chain_mask, int disable_dc_filter, void *sample_buf, u_int32_t *max_samples);
extern HAL_STATUS ar9300CalculateADCRefPowers(struct ath_hal *ah, int freqMhz, int16_t *sample_min, int16_t *sample_max, int32_t *chain_ref_pwr, int num_chain_ref_pwr);
extern HAL_STATUS ar9300GetMinAGCGain(struct ath_hal *ah, int freqMhz, int32_t *chain_gain, int num_chain_gain);

extern  HAL_BOOL ar9300Reset11n(struct ath_hal *ah, HAL_OPMODE opmode,
        HAL_CHANNEL *chan, HAL_BOOL bChannelChange, HAL_STATUS *status);
extern void ar9300SetCoverageClass(struct ath_hal *ah, u_int8_t coverageclass, int now);

extern int ar9300Get11nHwPlatform(struct ath_hal *ah);
extern void ar9300GetChannelCenters(struct ath_hal *ah,
                                    HAL_CHANNEL_INTERNAL *chan,
                                    CHAN_CENTERS *centers);
extern u_int16_t ar9300GetCtlCenter(struct ath_hal *ah,
                                        HAL_CHANNEL_INTERNAL *chan);
extern u_int16_t ar9300GetExtCenter(struct ath_hal *ah,
                                        HAL_CHANNEL_INTERNAL *chan);
extern u_int32_t ar9300GetMibCycleCountsPct(struct ath_hal *, u_int32_t*, u_int32_t*, u_int32_t*);

extern void ar9300DmaRegDump(struct ath_hal *);
extern  HAL_BOOL ar9300Set11nRxRifs(struct ath_hal *ah, HAL_BOOL enable);
extern HAL_BOOL ar9300Get11nRxRifs(struct ath_hal *ah);
extern  HAL_BOOL ar9300SetSmartAntenna(struct ath_hal *ah, HAL_BOOL enable);
extern HAL_BOOL ar9300SetRifsDelay(struct ath_hal *ah, HAL_BOOL enable);
extern HAL_BOOL ar9300DetectBbHang(struct ath_hal *ah);
extern HAL_BOOL ar9300DetectMacHang(struct ath_hal *ah);

#ifdef ATH_BT_COEX
extern void ar9300SetBTCoexInfo(struct ath_hal *ah, HAL_BT_COEX_INFO *btinfo);
extern void ar9300BTCoexConfig(struct ath_hal *ah, HAL_BT_COEX_CONFIG *btconf);
extern void ar9300BTCoexSetQcuThresh(struct ath_hal *ah, int qnum);
extern void ar9300BTCoexSetWeights(struct ath_hal *ah, u_int32_t stompType);
extern void ar9300BTCoexSetupBmissThresh(struct ath_hal *ah, u_int32_t thresh);
extern void ar9300BTCoexSetParameter(struct ath_hal *ah, u_int32_t type, u_int32_t value);
extern void ar9300BTCoexDisable(struct ath_hal *ah);
extern int ar9300BTCoexEnable(struct ath_hal *ah);
extern void ar9300InitBTCoex(struct ath_hal *ah);
#endif
extern int ar9300AllocGenericTimer(struct ath_hal *ah, HAL_GEN_TIMER_DOMAIN tsf);
extern void ar9300FreeGenericTimer(struct ath_hal *ah, int index);
extern void ar9300StartGenericTimer(struct ath_hal *ah, int index, u_int32_t timer_next,
                                u_int32_t timer_period);
extern void ar9300StopGenericTimer(struct ath_hal *ah, int index);
extern void ar9300GetGenTimerInterrupts(struct ath_hal *ah, u_int32_t *trigger,
                                u_int32_t *thresh);
extern void ar9300StartTsf2(struct ath_hal *ah);

extern void ar9300ChkRSSIUpdateTxPwr(struct ath_hal *ah, int rssi);
extern HAL_BOOL ar9300_is_skip_paprd_by_greentx(struct ath_hal *ah);
extern void ar9300_control_signals_for_green_tx_mode(struct ath_hal *ah);
extern void ar9300_hwgreentx_set_pal_spare(struct ath_hal *ah, int value);
extern int ar9300_getSpurInfo(struct ath_hal * ah, int *enable, int len, u_int16_t *freq);
extern int ar9300_setSpurInfo(struct ath_hal * ah, int enable, int len, u_int16_t *freq);
extern void ar9300WowSetGpioResetLow(struct ath_hal * ah);
extern HAL_BOOL ar9300_is_ani_noise_spur(struct ath_hal *ah);
extern void ar9300_get_vow_stats(struct ath_hal *ah, HAL_VOWSTATS *p_stats);
#ifdef ATH_CCX
extern void ar9300GetMibCycleCounts(struct ath_hal *, HAL_COUNTERS*);
extern void ar9300ClearMibCounters(struct ath_hal *ah);
extern u_int8_t ar9300GetCcaThreshold(struct ath_hal *ah);
#endif

/* EEPROM interface functions */
/* Common Interface functions */
extern  HAL_STATUS ar9300EepromAttach(struct ath_hal *);
extern  u_int32_t ar9300EepromGet(struct ath_hal_9300 *ahp, EEPROM_PARAM param);

extern  u_int32_t ar9300INIFixup(struct ath_hal *ah,
                                    ar9300_eeprom_t *pEepData,
                                    u_int32_t reg,
                                    u_int32_t val);

extern  HAL_STATUS ar9300EepromSetTransmitPower(struct ath_hal *ah,
                     ar9300_eeprom_t *pEepData, HAL_CHANNEL_INTERNAL *chan,
                     u_int16_t cfgCtl, u_int16_t twiceAntennaReduction,
                     u_int16_t twiceMaxRegulatoryPower, u_int16_t powerLimit);
extern  void ar9300EepromSetAddac(struct ath_hal *, HAL_CHANNEL_INTERNAL *);
extern  HAL_BOOL ar9300EepromSetParam(struct ath_hal *ah, EEPROM_PARAM param, u_int32_t value);
extern  HAL_BOOL ar9300EepromSetBoardValues(struct ath_hal *, HAL_CHANNEL_INTERNAL *);
extern  HAL_BOOL ar9300EepromReadWord(struct ath_hal *, u_int off, u_int16_t *data);
extern  HAL_BOOL ar9300EepromRead(struct ath_hal *ah, long address, u_int8_t *buffer, int many);
extern  HAL_BOOL ar9300OtpRead(struct ath_hal *ah, u_int off, u_int32_t *data);
extern  HAL_BOOL ar9300FlashRead(struct ath_hal *, u_int off, u_int16_t *data);
#if AH_SUPPORT_WRITE_EEPROM
extern  HAL_BOOL ar9300EepromWrite(struct ath_hal *, u_int off, u_int16_t data);
extern  HAL_BOOL ar9300OtpWrite(struct ath_hal *ah, u_int off, u_int32_t data);
#endif
extern  HAL_BOOL ar9300FlashWrite(struct ath_hal *, u_int off, u_int16_t data);
extern  u_int ar9300EepromDumpSupport(struct ath_hal *ah, void **ppE);
extern  u_int8_t ar9300EepromGetNumAntConfig(struct ath_hal_9300 *ahp, HAL_FREQ_BAND freq_band);
extern  HAL_STATUS ar9300EepromGetAntCfg(struct ath_hal_9300 *ahp, HAL_CHANNEL_INTERNAL *chan,
                                     u_int8_t index, u_int16_t *config);
extern u_int8_t* ar9300EepromGetCustData(struct ath_hal_9300 *ahp);
extern u_int8_t *ar9300EepromGetSpurChansPtr(struct ath_hal *ah, HAL_BOOL is2GHz);
extern HAL_BOOL ar9300InterferenceIsPresent(struct ath_hal *ah);
extern void ar9300DispTPCTables(struct ath_hal *ah);

/* Common EEPROM Help function */
extern void ar9300SetImmunity(struct ath_hal *ah, HAL_BOOL enable);
extern void ar9300GetHwHangs(struct ath_hal *ah, hal_hw_hangs_t *hangs);

extern u_int ar9300MacToClks(struct ath_hal *ah, u_int clks);

/* TxBF interface */
#ifdef ATH_SUPPORT_TxBF
extern void ar9300InitBf(struct ath_hal *ah);
extern void ar9300Set11nTxBFSounding(struct ath_hal *ah, 
				void *ds, HAL_11N_RATE_SERIES series[], u_int8_t CEC, u_int8_t opt);
#ifdef TXBF_TODO
extern void ar9300Set11nTxBFCal(struct ath_hal *ah, 
			void *ds, u_int8_t CalPos, u_int8_t codeRate, u_int8_t CEC, u_int8_t opt);
extern HAL_BOOL ar9300TxBFSaveCVFromCompress(struct ath_hal *ah, u_int8_t keyidx,
            u_int16_t mimocontrol, u_int8_t *CompressRpt);
extern HAL_BOOL ar9300TxBFSaveCVFromNonCompress(struct ath_hal *ah, u_int8_t keyidx,
			u_int16_t mimocontrol, u_int8_t *NonCompressRpt);
extern HAL_BOOL ar9300TxBFRCUpdate(struct ath_hal *ah, struct ath_rx_status *rx_status, 
            u_int8_t *local_h, u_int8_t *CSIFrame, u_int8_t NESSA, u_int8_t NESSB, int BW);
extern int ar9300FillCSIFrame(struct ath_hal *ah, struct ath_rx_status *rx_status,
        u_int8_t BW, u_int8_t *local_h, u_int8_t *CSIFrameBody);
#endif
extern void ar9300FillTxBFCapabilities(struct ath_hal *ah);
extern HAL_TXBF_CAPS * ar9300GetTxBFCapabilities(struct ath_hal *ah);
extern HAL_BOOL ar9300ReadKeyCacheMac(struct ath_hal *, u_int16_t entry, u_int8_t *mac);
extern void ar9300TxBFSetKey(struct ath_hal *ah, u_int16_t entry, u_int8_t rx_staggered_sounding,
            u_int8_t channel_estimation_cap, u_int8_t MMSS);
extern void ar9300SetHwCvTimeout (struct ath_hal *ah, HAL_BOOL opt);
extern void ar9300_set_selfgenrate_limit(struct ath_hal *ah, u_int8_t rateidx);
extern void ar9300_reset_lowest_txrate(struct ath_hal *ah);
extern void ar9300GetPerRateTxBFFlag(struct ath_hal *ah, u_int8_t txDisableFlag[][AH_MAX_CHAINS]);
#else
#define ar9300_set_selfgenrate_limit(ah, ts_ratecode)
#endif

#if ATH_SUPPORT_WIRESHARK
#include "ah_radiotap.h"
extern void ar9300FillRadiotapHdr(struct ath_hal *ah,
    struct ah_rx_radiotap_header *rh, struct ah_ppi_data *ppi, struct ath_desc *ds, void *buf_addr);
#endif /* ATH_SUPPORT_WIRESHARK */
extern HAL_STATUS ar9300SetProxySTA(struct ath_hal *ah, HAL_BOOL enable);

extern HAL_BOOL ar9300RegulatoryDomainOverride(
    struct ath_hal *ah, u_int16_t regdmn);
#if ATH_ANT_DIV_COMB
extern void ar9300AntDivCombGetConfig(struct ath_hal *ah, HAL_ANT_COMB_CONFIG* divCombConf);
extern void ar9300AntDivCombSetConfig(struct ath_hal *ah, HAL_ANT_COMB_CONFIG* divCombConf);
#endif /* ATH_ANT_DIV_COMB */
extern void ar9300DisablePhyRestart(struct ath_hal *ah, int disable_phy_restart);
extern void ar9300_enable_keysearch_always(struct ath_hal *ah, int enable);
extern HAL_BOOL ar9300ForceVCS( struct ath_hal *ah);
extern HAL_BOOL ar9300SetDfs3StreamFix(struct ath_hal *ah, u_int32_t val);
extern HAL_BOOL ar9300Get3StreamSignature( struct ath_hal *ah);
#ifdef ATH_TX99_DIAG
#ifndef ATH_SUPPORT_HTC
extern void ar9300_tx99_channel_pwr_update(struct ath_hal *ah, HAL_CHANNEL *c, u_int32_t txpower);
extern void ar9300_tx99_chainmsk_setup(struct ath_hal *ah, int tx_chainmask); 
extern void ar9300_tx99_set_single_carrier(struct ath_hal *ah, int tx_chain_mask, int chtype);
extern void ar9300_tx99_start(struct ath_hal *ah, u_int8_t *data);
extern void ar9300_tx99_stop(struct ath_hal *ah);
#endif /* ATH_SUPPORT_HTC */
#endif /* ATH_TX99_DIAG */
#endif  /* _ATH_AR9300_H_ */
