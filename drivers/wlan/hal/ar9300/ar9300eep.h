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

#ifndef _ATH_AR9300_EEP_H_
#define _ATH_AR9300_EEP_H_

#include "opt_ah.h"

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, ar9300, 1)
#endif


#define MSTATE 100
#define MOUTPUT 2048
#define MDEFAULT 15
#define MVALUE 100

enum CompressAlgorithm
{
    _CompressNone=0,
    _CompressLzma,
    _CompressPairs,
    _CompressBlock,
    _Compress4,
    _Compress5,
    _Compress6,
    _Compress7,
};


enum
{
	CalibrationDataNone=0,
	CalibrationDataFlash,
	CalibrationDataEeprom,
	CalibrationDataOtp,
	CalibrationDataFromHost,
#ifdef ATH_CAL_NAND_FLASH
	CalibrationDataNand,
#endif
	CalibrationDataDontLoad,
};


//
// DO NOT CHANGE THE DEFINTIONS OF THESE SYMBOLS.
// Add additional definitions to the end.
// Yes, the first one is 2. Do not use 0 or 1.
//
enum Ar9300EepromTemplate
{
	Ar9300EepromTemplateGeneric=2,
	Ar9300EepromTemplateHB112=3,
	Ar9300EepromTemplateHB116=4,
	Ar9300EepromTemplateXB112=5,
	Ar9300EepromTemplateXB113=6,
	Ar9300EepromTemplateXB114=7,
	Ar9300EepromTemplateTB417=8,
	Ar9300EepromTemplateAP111=9,
	Ar9300EepromTemplateAP121=10,
	Ar9300EepromTemplateHornetGeneric=11,
};

#define Ar9300EepromTemplateDefault Ar9300EepromTemplateGeneric
#define Ar9300EepromFormatDefault 2

#define ReferenceCurrent 0
#define CompressionHeaderLength 4
#define CompressionChecksumLength 2

#define OSPREY_EEP_VER               0xD000
#define OSPREY_EEP_VER_MINOR_MASK    0xFFF
#define OSPREY_EEP_MINOR_VER_1       0x1
#define OSPREY_EEP_MINOR_VER         OSPREY_EEP_MINOR_VER_1

// 16-bit offset location start of calibration struct
#define OSPREY_EEP_START_LOC         256
#define OSPREY_NUM_5G_CAL_PIERS      8
#define OSPREY_NUM_2G_CAL_PIERS      3
#define OSPREY_NUM_5G_20_TARGET_POWERS  8
#define OSPREY_NUM_5G_40_TARGET_POWERS  8
#define OSPREY_NUM_2G_CCK_TARGET_POWERS 2
#define OSPREY_NUM_2G_20_TARGET_POWERS  3
#define OSPREY_NUM_2G_40_TARGET_POWERS  3
//#define OSPREY_NUM_CTLS              21
#define OSPREY_NUM_CTLS_5G           9
#define OSPREY_NUM_CTLS_2G           12
#define OSPREY_CTL_MODE_M            0xF
#define OSPREY_NUM_BAND_EDGES_5G     8
#define OSPREY_NUM_BAND_EDGES_2G     4
#define OSPREY_NUM_PD_GAINS          4
#define OSPREY_PD_GAINS_IN_MASK      4
#define OSPREY_PD_GAIN_ICEPTS        5
#define OSPREY_EEPROM_MODAL_SPURS    5
#define OSPREY_MAX_RATE_POWER        63
#define OSPREY_NUM_PDADC_VALUES      128
#define OSPREY_NUM_RATES             16
#define OSPREY_BCHAN_UNUSED          0xFF
#define OSPREY_MAX_PWR_RANGE_IN_HALF_DB 64
#define OSPREY_OPFLAGS_11A           0x01
#define OSPREY_OPFLAGS_11G           0x02
#define OSPREY_OPFLAGS_5G_HT40       0x04
#define OSPREY_OPFLAGS_2G_HT40       0x08
#define OSPREY_OPFLAGS_5G_HT20       0x10
#define OSPREY_OPFLAGS_2G_HT20       0x20
#define OSPREY_EEPMISC_BIG_ENDIAN    0x01
#define OSPREY_EEPMISC_WOW           0x02
#define OSPREY_CUSTOMER_DATA_SIZE    20

#define FREQ2FBIN(x,y) ((y) ? ((x) - 2300) : (((x) - 4800) / 5))
#define FBIN2FREQ(x,y) ((y) ? (2300 + x) : (4800 + 5 * x))
#define OSPREY_MAX_CHAINS            3
#define OSPREY_ANT_16S               25
#define OSPREY_FUTURE_MODAL_SZ       6

#define OSPREY_NUM_ANT_CHAIN_FIELDS     7
#define OSPREY_NUM_ANT_COMMON_FIELDS    4
#define OSPREY_SIZE_ANT_CHAIN_FIELD     3
#define OSPREY_SIZE_ANT_COMMON_FIELD    4
#define OSPREY_ANT_CHAIN_MASK           0x7
#define OSPREY_ANT_COMMON_MASK          0xf
#define OSPREY_CHAIN_0_IDX              0
#define OSPREY_CHAIN_1_IDX              1
#define OSPREY_CHAIN_2_IDX              2
#define OSPREY_1_CHAINMASK              1
#define OSPREY_2LOHI_CHAINMASK          5
#define OSPREY_2LOMID_CHAINMASK         3
#define OSPREY_3_CHAINMASK              7

#define AR928X_NUM_ANT_CHAIN_FIELDS     6
#define AR928X_SIZE_ANT_CHAIN_FIELD     2
#define AR928X_ANT_CHAIN_MASK           0x3

/* Delta from which to start power to pdadc table */
/* This offset is used in both open loop and closed loop power control
 * schemes. In open loop power control, it is not really needed, but for
 * the "sake of consistency" it was kept.
 * For certain AP designs, this value is overwritten by the value in the flag
 * "pwrTableOffset" just before writing the pdadc vs pwr into the chip registers.
 */
#define OSPREY_PWR_TABLE_OFFSET  0

//enable flags for voltage and temp compensation
#define ENABLE_TEMP_COMPENSATION 0x01
#define ENABLE_VOLT_COMPENSATION 0x02

#define FLASH_BASE_CALDATA_OFFSET  0x1000
#define AR9300_EEPROM_SIZE 16*1024  // byte addressable
#define FIXED_CCA_THRESHOLD 15
#define HOST_CALDATA_SIZE (16*1024)

typedef struct eepFlags {
    u_int8_t  opFlags;
    u_int8_t  eepMisc;
} __packed EEP_FLAGS;

typedef enum targetPowerHTRates {
    HT_TARGET_RATE_0_8_16,
    HT_TARGET_RATE_1_3_9_11_17_19,
    HT_TARGET_RATE_4,
    HT_TARGET_RATE_5,
    HT_TARGET_RATE_6,
    HT_TARGET_RATE_7,
    HT_TARGET_RATE_12,
    HT_TARGET_RATE_13,
    HT_TARGET_RATE_14,
    HT_TARGET_RATE_15,
    HT_TARGET_RATE_20,
    HT_TARGET_RATE_21,
    HT_TARGET_RATE_22,
    HT_TARGET_RATE_23
}TARGET_POWER_HT_RATES;

const static int mapRate2Index[24]=
{
    0,1,1,1,2,
    3,4,5,0,1,
    1,1,6,7,8,
    9,0,1,1,1,
    10,11,12,13
};

typedef enum targetPowerLegacyRates {
    LEGACY_TARGET_RATE_6_24,
    LEGACY_TARGET_RATE_36,
    LEGACY_TARGET_RATE_48,
    LEGACY_TARGET_RATE_54
}TARGET_POWER_LEGACY_RATES;

typedef enum targetPowerCckRates {
    LEGACY_TARGET_RATE_1L_5L,
    LEGACY_TARGET_RATE_5S,
    LEGACY_TARGET_RATE_11L,
    LEGACY_TARGET_RATE_11S
}TARGET_POWER_CCK_RATES;

#define OSPREY_CHECKSUM_LOCATION (OSPREY_EEP_START_LOC + 1)

#define MAX_MODAL_RESERVED 11
#define MAX_MODAL_FUTURE 10
#define MAX_BASE_EXTENSION_FUTURE 11


typedef struct osprey_BaseEepHeader {
    u_int16_t  regDmn[2]; //Does this need to be outside of this structure, if it gets written after calibration
    u_int8_t   txrxMask;  //4 bits tx and 4 bits rx
    EEP_FLAGS  opCapFlags;
    u_int8_t   rfSilent;
    u_int8_t   blueToothOptions;
    u_int8_t   deviceCap;
    u_int8_t   deviceType; // takes lower byte in eeprom location
    int8_t     pwrTableOffset; // offset in dB to be added to beginning of pdadc table in calibration
	u_int8_t   params_for_tuning_caps[2];  //placeholder, get more details from Don
    u_int8_t   featureEnable; //bit0 - enable tx temp comp 
                             //bit1 - enable tx volt comp
                             //bit2 - enable fastClock - default to 1
                             //bit3 - enable doubling - default to 1
							 //bit4 - enable internal regulator - default to 1
							 //bit5 - enable paprd - default to 0
							 //bit6 - enable TuningCaps - default to 0
    u_int8_t   miscConfiguration; //misc flags: bit0 - turn down drivestrength
									// bit 1:2 - 0=don't force, 1=force to thermometer 0, 2=force to thermometer 1, 3=force to thermometer 2
									// bit 3 - reduce chain mask from 0x7 to 0x3 on 2 stream rates 
									// bit 4 - enable quick drop
	u_int8_t   eepromWriteEnableGpio;
	u_int8_t   wlanDisableGpio;
	u_int8_t   wlanLedGpio;
	u_int8_t   rxBandSelectGpio;
	u_int8_t   txrxgain;
	u_int32_t   swreg;    // SW controlled internal regulator fields
} __packed OSPREY_BASE_EEP_HEADER;

typedef struct osprey_BaseExtension_1 {
	u_int8_t  ant_div_control;
	u_int8_t  future[MAX_BASE_EXTENSION_FUTURE];
    int8_t  quickDropLow;           
    int8_t  quickDropHigh;           
} __packed OSPREY_BASE_EXTENSION_1;

typedef struct osprey_BaseExtension_2 {
	int8_t    tempSlopeLow;
	int8_t    tempSlopeHigh;
    u_int8_t   xatten1DBLow[OSPREY_MAX_CHAINS];           // 3  //xatten1_db for merlin (0xa20c/b20c 5:0)
    u_int8_t   xatten1MarginLow[OSPREY_MAX_CHAINS];          // 3  //xatten1_margin for merlin (0xa20c/b20c 16:12
    u_int8_t   xatten1DBHigh[OSPREY_MAX_CHAINS];           // 3  //xatten1_db for merlin (0xa20c/b20c 5:0)
    u_int8_t   xatten1MarginHigh[OSPREY_MAX_CHAINS];          // 3  //xatten1_margin for merlin (0xa20c/b20c 16:12
} __packed OSPREY_BASE_EXTENSION_2;

typedef struct spurChanStruct {
    u_int16_t spurChan;
    u_int8_t  spurRangeLow;
    u_int8_t  spurRangeHigh;
} __packed SPUR_CHAN;

//Note the order of the fields in this structure has been optimized to put all fields likely to change together
typedef struct ospreyModalEepHeader {
    u_int32_t  antCtrlCommon;                         // 4   idle, t1, t2, b (4 bits per setting)
    u_int32_t  antCtrlCommon2;                        // 4    ra1l1, ra2l1, ra1l2, ra2l2, ra12
    u_int16_t  antCtrlChain[OSPREY_MAX_CHAINS];       // 6   idle, t, r, rx1, rx12, b (2 bits each)
    u_int8_t   xatten1DB[OSPREY_MAX_CHAINS];           // 3  //xatten1_db for merlin (0xa20c/b20c 5:0)
    u_int8_t   xatten1Margin[OSPREY_MAX_CHAINS];          // 3  //xatten1_margin for merlin (0xa20c/b20c 16:12
	int8_t    tempSlope;
	int8_t    voltSlope;
    u_int8_t spurChans[OSPREY_EEPROM_MODAL_SPURS];  // spur channels in usual fbin coding format
    int8_t    noiseFloorThreshCh[OSPREY_MAX_CHAINS]; // 3    //Check if the register is per chain
	u_int8_t   reserved[MAX_MODAL_RESERVED];                 
	int8_t quickDrop;
    u_int8_t   xpaBiasLvl;                            // 1
    u_int8_t   txFrameToDataStart;                    // 1
    u_int8_t   txFrameToPaOn;                         // 1
    u_int8_t   txClip;                                     // 4 bits tx_clip, 4 bits dac_scale_cck
    int8_t    antennaGain;                           // 1
    u_int8_t   switchSettling;                        // 1
    int8_t    adcDesiredSize;                        // 1
    u_int8_t   txEndToXpaOff;                         // 1
    u_int8_t   txEndToRxOn;                           // 1
    u_int8_t   txFrameToXpaOn;                        // 1
    u_int8_t   thresh62;                              // 1
	u_int32_t papdRateMaskHt20;  
	u_int32_t papdRateMaskHt40;
	u_int8_t    futureModal[MAX_MODAL_FUTURE];
	// last 12 bytes stolen and moved to newly created base extension structure
} __packed OSPREY_MODAL_EEP_HEADER;                    // == 100 B

typedef struct ospCalDataPerFreqOpLoop {
    int8_t refPower;    /*   */
    u_int8_t voltMeas; /* pdadc voltage at power measurement */
    u_int8_t tempMeas;  /* pcdac used for power measurement   */
    int8_t rxNoisefloorCal; /*range is -60 to -127 create a mapping equation 1db resolution */
    int8_t rxNoisefloorPower; /*range is same as noisefloor */
    u_int8_t rxTempMeas; /*temp measured when noisefloor cal was performed */
} __packed OSP_CAL_DATA_PER_FREQ_OP_LOOP;

typedef struct CalTargetPowerLegacy {
    u_int8_t  tPow2x[4];
} __packed CAL_TARGET_POWER_LEG;

typedef struct ospCalTargetPowerHt {
    u_int8_t  tPow2x[14];
} __packed OSP_CAL_TARGET_POWER_HT;

#if AH_BYTE_ORDER == AH_BIG_ENDIAN
typedef struct CalCtlEdgePwr {
    u_int8_t  flag  :2,
              tPower :6;
} __packed CAL_CTL_EDGE_PWR;
#else
typedef struct CalCtlEdgePwr {
    u_int8_t  tPower :6,
             flag   :2;
} __packed CAL_CTL_EDGE_PWR;
#endif

typedef struct ospCalCtlData_5G {
    CAL_CTL_EDGE_PWR  ctlEdges[OSPREY_NUM_BAND_EDGES_5G];
} __packed OSP_CAL_CTL_DATA_5G;

typedef struct ospCalCtlData_2G {
    CAL_CTL_EDGE_PWR  ctlEdges[OSPREY_NUM_BAND_EDGES_2G];
} __packed OSP_CAL_CTL_DATA_2G;

typedef struct ospreyEeprom {
    u_int8_t  eepromVersion;
    u_int8_t  templateVersion;
    u_int8_t  macAddr[6];
    u_int8_t  custData[OSPREY_CUSTOMER_DATA_SIZE];

    OSPREY_BASE_EEP_HEADER    baseEepHeader;

    OSPREY_MODAL_EEP_HEADER   modalHeader2G;
	OSPREY_BASE_EXTENSION_1 base_ext1;
	u_int8_t            calFreqPier2G[OSPREY_NUM_2G_CAL_PIERS];
    OSP_CAL_DATA_PER_FREQ_OP_LOOP calPierData2G[OSPREY_MAX_CHAINS][OSPREY_NUM_2G_CAL_PIERS];
	u_int8_t calTarget_freqbin_Cck[OSPREY_NUM_2G_CCK_TARGET_POWERS];
    u_int8_t calTarget_freqbin_2G[OSPREY_NUM_2G_20_TARGET_POWERS];
    u_int8_t calTarget_freqbin_2GHT20[OSPREY_NUM_2G_20_TARGET_POWERS];
    u_int8_t calTarget_freqbin_2GHT40[OSPREY_NUM_2G_40_TARGET_POWERS];
    CAL_TARGET_POWER_LEG calTargetPowerCck[OSPREY_NUM_2G_CCK_TARGET_POWERS];
    CAL_TARGET_POWER_LEG calTargetPower2G[OSPREY_NUM_2G_20_TARGET_POWERS];
    OSP_CAL_TARGET_POWER_HT  calTargetPower2GHT20[OSPREY_NUM_2G_20_TARGET_POWERS];
    OSP_CAL_TARGET_POWER_HT  calTargetPower2GHT40[OSPREY_NUM_2G_40_TARGET_POWERS];
    u_int8_t   ctlIndex_2G[OSPREY_NUM_CTLS_2G];
    u_int8_t   ctl_freqbin_2G[OSPREY_NUM_CTLS_2G][OSPREY_NUM_BAND_EDGES_2G];
    OSP_CAL_CTL_DATA_2G   ctlPowerData_2G[OSPREY_NUM_CTLS_2G];

    OSPREY_MODAL_EEP_HEADER   modalHeader5G;
	OSPREY_BASE_EXTENSION_2 base_ext2;
    u_int8_t            calFreqPier5G[OSPREY_NUM_5G_CAL_PIERS];
    OSP_CAL_DATA_PER_FREQ_OP_LOOP calPierData5G[OSPREY_MAX_CHAINS][OSPREY_NUM_5G_CAL_PIERS];
    u_int8_t calTarget_freqbin_5G[OSPREY_NUM_5G_20_TARGET_POWERS];
    u_int8_t calTarget_freqbin_5GHT20[OSPREY_NUM_5G_20_TARGET_POWERS];
    u_int8_t calTarget_freqbin_5GHT40[OSPREY_NUM_5G_40_TARGET_POWERS];
    CAL_TARGET_POWER_LEG calTargetPower5G[OSPREY_NUM_5G_20_TARGET_POWERS];
    OSP_CAL_TARGET_POWER_HT  calTargetPower5GHT20[OSPREY_NUM_5G_20_TARGET_POWERS];
    OSP_CAL_TARGET_POWER_HT  calTargetPower5GHT40[OSPREY_NUM_5G_40_TARGET_POWERS];
    u_int8_t   ctlIndex_5G[OSPREY_NUM_CTLS_5G];
    u_int8_t   ctl_freqbin_5G[OSPREY_NUM_CTLS_5G][OSPREY_NUM_BAND_EDGES_5G];
    OSP_CAL_CTL_DATA_5G   ctlPowerData_5G[OSPREY_NUM_CTLS_5G];
} __packed ar9300_eeprom_t;


/*
** SWAP Functions
** used to read EEPROM data, which is apparently stored in little
** endian form.  We have included both forms of the swap functions,
** one for big endian and one for little endian.  The indices of the
** array elements are the differences
*/
#if AH_BYTE_ORDER == AH_BIG_ENDIAN

#define AR9300_EEPROM_MAGIC         0x5aa5
#define SWAP16(_x) ( (u_int16_t)( (((const u_int8_t *)(&_x))[0] ) |\
                     ( ( (const u_int8_t *)( &_x ) )[1]<< 8) ) )

#define SWAP32(_x) ((u_int32_t)(                       \
                    (((const u_int8_t *)(&_x))[0]) |        \
                    (((const u_int8_t *)(&_x))[1]<< 8) |    \
                    (((const u_int8_t *)(&_x))[2]<<16) |    \
                    (((const u_int8_t *)(&_x))[3]<<24)))

#else // AH_BYTE_ORDER

#define AR9300_EEPROM_MAGIC         0xa55a
#define    SWAP16(_x) ( (u_int16_t)( (((const u_int8_t *)(&_x))[1] ) |\
                        ( ( (const u_int8_t *)( &_x ) )[0]<< 8) ) )

#define SWAP32(_x) ((u_int32_t)(                       \
                    (((const u_int8_t *)(&_x))[3]) |        \
                    (((const u_int8_t *)(&_x))[2]<< 8) |    \
                    (((const u_int8_t *)(&_x))[1]<<16) |    \
                    (((const u_int8_t *)(&_x))[0]<<24)))

#endif // AH_BYTE_ORDER

// OTP registers for OSPREY

#define AR_GPIO_IN_OUT            0x4048 // GPIO input / output register
#define OTP_MEM_START_ADDRESS     0x14000
#define OTP_STATUS0_OTP_SM_BUSY   0x00015f18
#define OTP_STATUS1_EFUSE_READ_DATA 0x00015f1c

#define OTP_LDO_CONTROL_ENABLE    0x00015f24
#define OTP_LDO_STATUS_POWER_ON   0x00015f2c
#define OTP_INTF0_EFUSE_WR_ENABLE_REG_V 0x00015f00
// OTP register for WASP
#define OTP_MEM_START_ADDRESS_WASP           0x00030000 
#define OTP_STATUS0_OTP_SM_BUSY_WASP         (OTP_MEM_START_ADDRESS_WASP + 0x1018)
#define OTP_STATUS1_EFUSE_READ_DATA_WASP     (OTP_MEM_START_ADDRESS_WASP + 0x101C)
#define OTP_LDO_CONTROL_ENABLE_WASP          (OTP_MEM_START_ADDRESS_WASP + 0x1024)
#define OTP_LDO_STATUS_POWER_ON_WASP         (OTP_MEM_START_ADDRESS_WASP + 0x102C)
#define OTP_INTF0_EFUSE_WR_ENABLE_REG_V_WASP (OTP_MEM_START_ADDRESS_WASP + 0x1000)
// Below control the access timing of OTP read/write
#define OTP_PG_STROBE_PW_REG_V_WASP              (OTP_MEM_START_ADDRESS_WASP + 0x1008)
#define OTP_RD_STROBE_PW_REG_V_WASP              (OTP_MEM_START_ADDRESS_WASP + 0x100C)
#define OTP_VDDQ_HOLD_TIME_DELAY_WASP            (OTP_MEM_START_ADDRESS_WASP + 0x1030)
#define OTP_PGENB_SETUP_HOLD_TIME_DELAY_WASP     (OTP_MEM_START_ADDRESS_WASP + 0x1034)
#define OTP_STROBE_PULSE_INTERVAL_DELAY_WASP     (OTP_MEM_START_ADDRESS_WASP + 0x1038)
#define OTP_CSB_ADDR_LOAD_SETUP_HOLD_DELAY_WASP  (OTP_MEM_START_ADDRESS_WASP + 0x103C)


#define AR9300_EEPROM_MAGIC_OFFSET  0x0
/* reg_off = 4 * (eep_off) */
#define AR9300_EEPROM_S             2
#define AR9300_EEPROM_OFFSET        0x2000
#ifdef AR9100
#define AR9300_EEPROM_START_ADDR    0x1fff1000
#else
#define AR9300_EEPROM_START_ADDR    0x503f1200
#endif
#define AR9300_FLASH_CAL_START_OFFSET	    0x1000
#define AR9300_EEPROM_MAX           0xae0
#define IS_EEP_MINOR_V3(_ahp) (ar9300EepromGet((_ahp), EEP_MINOR_REV)  >= AR9300_EEP_MINOR_VER_3)

#define ar9300_get_ntxchains(_txchainmask) \
    (((_txchainmask >> 2) & 1) + ((_txchainmask >> 1) & 1) + (_txchainmask & 1))

/* RF silent fields in \ */
#define EEP_RFSILENT_ENABLED        0x0001  /* bit 0: enabled/disabled */
#define EEP_RFSILENT_ENABLED_S      0       /* bit 0: enabled/disabled */
#define EEP_RFSILENT_POLARITY       0x0002  /* bit 1: polarity */
#define EEP_RFSILENT_POLARITY_S     1       /* bit 1: polarity */
#define EEP_RFSILENT_GPIO_SEL       0x001c  /* bits 2..4: gpio PIN */
#define EEP_RFSILENT_GPIO_SEL_S     2       /* bits 2..4: gpio PIN */
#define AR9300_EEP_VER               0xE
#define AR9300_BCHAN_UNUSED          0xFF
#define AR9300_MAX_RATE_POWER        63

typedef enum {
    CALDATA_AUTO=0,
    CALDATA_EEPROM,
    CALDATA_FLASH,
    CALDATA_OTP
} CALDATA_TYPE;

typedef enum {
    EEP_NFTHRESH_5,
    EEP_NFTHRESH_2,
    EEP_MAC_MSW,
    EEP_MAC_MID,
    EEP_MAC_LSW,
    EEP_REG_0,
    EEP_REG_1,
    EEP_OP_CAP,
    EEP_OP_MODE,
    EEP_RF_SILENT,
    EEP_OB_5,
    EEP_DB_5,
    EEP_OB_2,
    EEP_DB_2,
    EEP_MINOR_REV,
    EEP_TX_MASK,
    EEP_RX_MASK,
    EEP_FSTCLK_5G,
    EEP_RXGAIN_TYPE,
    EEP_OL_PWRCTRL,
    EEP_TXGAIN_TYPE,
    EEP_RC_CHAIN_MASK,
    EEP_DAC_HPWR_5G,
    EEP_FRAC_N_5G,
    EEP_DEV_TYPE,
    EEP_TEMPSENSE_SLOPE,
    EEP_TEMPSENSE_SLOPE_PAL_ON,
    EEP_PWR_TABLE_OFFSET,
    EEP_DRIVE_STRENGTH,
    EEP_INTERNAL_REGULATOR,
    EEP_SWREG,
	EEP_PAPRD_ENABLED,
	EEP_ANTDIV_control,
	EEP_CHAIN_MASK_REDUCE,
} EEPROM_PARAM;

#define AR9300_RATES_OFDM_OFFSET    0
#define AR9300_RATES_CCK_OFFSET     4
#define AR9300_RATES_HT20_OFFSET    8
#define AR9300_RATES_HT40_OFFSET    22

typedef enum ar9300_Rates {
    ALL_TARGET_LEGACY_6_24,
    ALL_TARGET_LEGACY_36,
    ALL_TARGET_LEGACY_48,
    ALL_TARGET_LEGACY_54,
    ALL_TARGET_LEGACY_1L_5L,
    ALL_TARGET_LEGACY_5S,
    ALL_TARGET_LEGACY_11L,
    ALL_TARGET_LEGACY_11S,
    ALL_TARGET_HT20_0_8_16,
    ALL_TARGET_HT20_1_3_9_11_17_19,
    ALL_TARGET_HT20_4,
    ALL_TARGET_HT20_5,
    ALL_TARGET_HT20_6,
    ALL_TARGET_HT20_7,
    ALL_TARGET_HT20_12,
    ALL_TARGET_HT20_13,
    ALL_TARGET_HT20_14,
    ALL_TARGET_HT20_15,
    ALL_TARGET_HT20_20,
    ALL_TARGET_HT20_21,
    ALL_TARGET_HT20_22,
    ALL_TARGET_HT20_23,
    ALL_TARGET_HT40_0_8_16,
    ALL_TARGET_HT40_1_3_9_11_17_19,
    ALL_TARGET_HT40_4,
    ALL_TARGET_HT40_5,
    ALL_TARGET_HT40_6,
    ALL_TARGET_HT40_7,
    ALL_TARGET_HT40_12,
    ALL_TARGET_HT40_13,
    ALL_TARGET_HT40_14,
    ALL_TARGET_HT40_15,
    ALL_TARGET_HT40_20,
    ALL_TARGET_HT40_21,
    ALL_TARGET_HT40_22,
    ALL_TARGET_HT40_23,
    ar9300RateSize
} AR9300_RATES;


/**************************************************************************
 * fbin2freq
 *
 * Get channel value from binary representation held in eeprom
 * RETURNS: the frequency in MHz
 */
static inline u_int16_t
fbin2freq(u_int8_t fbin, HAL_BOOL is2GHz)
{
    /*
    * Reserved value 0xFF provides an empty definition both as
    * an fbin and as a frequency - do not convert
    */
    if (fbin == AR9300_BCHAN_UNUSED)
    {
        return fbin;
    }

    return (u_int16_t)((is2GHz) ? (2300 + fbin) : (4800 + 5 * fbin));
}

extern int CompressionHeaderUnpack(u_int8_t *best, int *code, int *reference, int *length, int *major, int *minor);
extern void Ar9300EepromFormatConvert(ar9300_eeprom_t *mptr);
extern HAL_BOOL ar9300EepromRestore(struct ath_hal *ah);
extern int ar9300EepromRestoreInternal(struct ath_hal *ah, ar9300_eeprom_t *mptr, int /*msize*/);
extern int ar9300EepromBaseAddress(struct ath_hal *ah);
extern int ar9300EepromVolatile(struct ath_hal *ah);
extern int ar9300EepromLowLimit(struct ath_hal *ah);
extern u_int16_t ar9300CompressionChecksum(u_int8_t *data, int dsize);
extern int ar9300CompressionHeaderUnpack(u_int8_t *best, int *code, int *reference, int *length, int *major, int *minor);

extern u_int16_t ar9300EepromStructSize(void);
extern ar9300_eeprom_t *ar9300EepromStructInit(int defaultIndex);
extern ar9300_eeprom_t *ar9300EepromStructGet(void);
extern ar9300_eeprom_t *ar9300EepromStructDefault(int defaultIndex);
extern ar9300_eeprom_t *ar9300EepromStructDefaultFindById(int ver);
extern int ar9300EepromStructDefaultMany(void);
extern int ar9300EepromUpdateCalPier(int pierIdx, int freq, int chain,
                          int pwrCorrection, int voltMeas, int tempMeas);
extern int ar9300PowerControlOverride(struct ath_hal *ah, int frequency, int *correction, int *voltage, int *temperature);

extern void ar9300EepromDisplayCalData(int for2GHz);
extern void ar9300EepromDisplayAll(void);
extern void ar9300SetTargetPowerFromEeprom(struct ath_hal *ah, u_int16_t freq,
                                           u_int8_t *targetPowerValT2);
extern HAL_BOOL ar9300EepromSetPowerPerRateTable(struct ath_hal *ah,
                                             ar9300_eeprom_t *pEepData,
                                             HAL_CHANNEL_INTERNAL *chan,
                                             u_int8_t *pPwrArray,
                                             u_int16_t cfgCtl,
                                             u_int16_t AntennaReduction,
                                             u_int16_t twiceMaxRegulatoryPower,
                                             u_int16_t powerLimit,
                                             u_int8_t chainmask); 
extern int ar9300TransmitPowerRegWrite(struct ath_hal *ah, u_int8_t *pPwrArray); 
extern int ar9300CompressionHeaderUnpack(u_int8_t *best, int *code, int *reference, int *length, int *major, int *minor);

extern u_int8_t ar9300EepromGetLegacyTrgtPwr(struct ath_hal *ah, u_int16_t rateIndex, u_int16_t freq, HAL_BOOL is2GHz);
extern u_int8_t ar9300EepromGetHT20TrgtPwr(struct ath_hal *ah, u_int16_t rateIndex, u_int16_t freq, HAL_BOOL is2GHz);
extern u_int8_t ar9300EepromGetHT40TrgtPwr(struct ath_hal *ah, u_int16_t rateIndex, u_int16_t freq, HAL_BOOL is2GHz);
extern u_int8_t ar9300EepromGetCckTrgtPwr(struct ath_hal *ah, u_int16_t rateIndex, u_int16_t freq);
extern HAL_BOOL ar9300InternalRegulatorApply(struct ath_hal *ah);
extern HAL_BOOL ar9300DriveStrengthApply(struct ath_hal *ah);
extern HAL_BOOL ar9300AttenuationApply(struct ath_hal *ah, u_int16_t channel);
extern int32_t ar9300ThermometerGet(struct ath_hal *ah);
extern HAL_BOOL ar9300ThermometerApply(struct ath_hal *ah);
extern u_int16_t ar9300QuickDropGet(struct ath_hal *ah, int chain, u_int16_t channel);
extern HAL_BOOL ar9300QuickDropApply(struct ath_hal *ah, u_int16_t channel);
extern u_int16_t ar9300txEndToXpaOffGet(struct ath_hal *ah, u_int16_t channel);
extern HAL_BOOL ar9300TxEndToXpabOffApply(struct ath_hal *ah, u_int16_t channel);

extern int32_t ar9300MacAdressGet(u_int8_t *mac);
extern int32_t ar9300CustomerDataGet(u_int8_t *data, int32_t len);
extern int32_t ar9300ReconfigDriveStrengthGet(void);
extern int32_t ar9300EnableTempCompensationGet(void);
extern int32_t ar9300EnableVoltCompensationGet(void);
extern int32_t ar9300FastClockEnableGet(void);
extern int32_t ar9300EnableDoublingGet(void);

extern u_int16_t *ar9300RegulatoryDomainGet(struct ath_hal *ah);
extern int32_t ar9300EepromWriteEnableGpioGet(struct ath_hal *ah);
extern int32_t ar9300WlanLedGpioGet(struct ath_hal *ah);
extern int32_t ar9300WlanDisableGpioGet(struct ath_hal *ah);
extern int32_t ar9300RxBandSelectGpioGet(struct ath_hal *ah);
extern int32_t ar9300RxGainIndexGet(struct ath_hal *ah);
extern int32_t ar9300TxGainIndexGet(struct ath_hal *ah);
extern int32_t ar9300XpaBiasLevelGet(struct ath_hal *ah, HAL_BOOL is2GHz);
extern HAL_BOOL ar9300XpaBiasLevelApply(struct ath_hal *ah, HAL_BOOL is2GHz);
extern u_int32_t ar9300AntCtrlCommonGet(struct ath_hal *ah, HAL_BOOL is2GHz);
extern u_int32_t ar9300AntCtrlCommon2Get(struct ath_hal *ah, HAL_BOOL is2GHz);
extern u_int16_t ar9300AntCtrlChainGet(struct ath_hal *ah, int chain, HAL_BOOL is2GHz);
extern HAL_BOOL ar9300AntCtrlApply(struct ath_hal *ah, HAL_BOOL is2GHz);
/* since valid noise floor values are negative, returns 1 on error */
extern int32_t ar9300NoiseFloorCalOrPowerGet(
    struct ath_hal *ah, int32_t frequency, int32_t ichain, HAL_BOOL use_cal);
#define ar9300NoiseFloorGet(ah, frequency, ichain) \
    ar9300NoiseFloorCalOrPowerGet(ah, frequency, ichain, 1/*use_cal*/)
#define ar9300NoiseFloorPowerGet(ah, frequency, ichain) \
    ar9300NoiseFloorCalOrPowerGet(ah, frequency, ichain, 0/*use_cal*/)
extern void ar9300EepromTemplatePreference(int32_t value);
extern int32_t ar9300EepromTemplateInstall(struct ath_hal *ah, int32_t value);
extern void ar9300CalibrationDataSet(struct ath_hal *ah, int32_t source);
extern int32_t ar9300CalibrationDataGet(struct ath_hal *ah);
extern int32_t ar9300CalibrationDataAddressGet(struct ath_hal *ah);
extern void ar9300CalibrationDataAddressSet(struct ath_hal *ah, int32_t source);
extern HAL_BOOL ar9300CalibrationDataReadFlash(struct ath_hal *ah, long address, u_int8_t *buffer, int many);
extern HAL_BOOL ar9300CalibrationDataReadEeprom(struct ath_hal *ah, long address, u_int8_t *buffer, int many);
extern HAL_BOOL ar9300CalibrationDataReadOtp(struct ath_hal *ah, long address, u_int8_t *buffer, int many);
extern HAL_BOOL ar9300CalibrationDataRead(struct ath_hal *ah, long address, u_int8_t *buffer, int many);
extern int32_t ar9300EepromSize(struct ath_hal *ah);
extern HAL_BOOL ar9300CalibrationDataReadArray(struct ath_hal *ah, int address, u_int8_t *buffer, int many);
extern HAL_BOOL ar9300TuningCapsApply(struct ath_hal *ah);


#if defined(WIN32) || defined(WIN64)
#pragma pack (pop, ar9300)
#endif

#endif  /* _ATH_AR9300_EEP_H_ */
