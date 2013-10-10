/*-
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting, Atheros
 * Communications, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 *    unmodified.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following NO
 *    ''WARRANTY'' disclaimer below (''Disclaimer''), without
 *    modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 *    disclaimer similar to the Disclaimer below and any redistribution
 *    must be conditioned upon including a substantially similar
 *    Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 *    names of any contributors may be used to endorse or promote
 *    product derived from this software without specific prior written
 *    permission.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 *
 */

#ifndef _ATH_AH_H_
#define _ATH_AH_H_
/*
 * Atheros Hardware Access Layer
 *
 * Clients of the HAL call ath_hal_attach to obtain a reference to an ath_hal
 * structure for use with the device.  Hardware-related operations that
 * follow must call back into the HAL through interface, supplying the
 * reference as the first parameter.
 */

#include "wlan_opts.h"
#include "ah_osdep.h"

/*
 * __ahdecl is analogous to _cdecl; it defines the calling
 * convention used within the HAL.  For most systems this
 * can just default to be empty and the compiler will (should)
 * use _cdecl.  For systems where _cdecl is not compatible this
 * must be defined.  See linux/ah_osdep.h for an example.
 */
#ifndef __ahdecl
#define __ahdecl
#endif

#if ATH_SUPPORT_WIRESHARK
#include "ah_radiotap.h" /* ah_rx_radiotap_header */
#endif

/*
 * Status codes that may be returned by the HAL.  Note that
 * interfaces that return a status code set it only when an
 * error occurs--i.e. you cannot check it for success.
 */
typedef enum {
    HAL_OK          = 0,    /* No error */
    HAL_ENXIO       = 1,    /* No hardware present */
    HAL_ENOMEM      = 2,    /* Memory allocation failed */
    HAL_EIO         = 3,    /* Hardware didn't respond as expected */
    HAL_EEMAGIC     = 4,    /* EEPROM magic number invalid */
    HAL_EEVERSION   = 5,    /* EEPROM version invalid */
    HAL_EELOCKED    = 6,    /* EEPROM unreadable */
    HAL_EEBADSUM    = 7,    /* EEPROM checksum invalid */
    HAL_EEREAD      = 8,    /* EEPROM read problem */
    HAL_EEBADMAC    = 9,    /* EEPROM mac address invalid */
    HAL_EESIZE      = 10,   /* EEPROM size not supported */
    HAL_EEWRITE     = 11,   /* Attempt to change write-locked EEPROM */
    HAL_EINVAL      = 12,   /* Invalid parameter to function */
    HAL_ENOTSUPP    = 13,   /* Hardware revision not supported */
    HAL_ESELFTEST   = 14,   /* Hardware self-test failed */
    HAL_EINPROGRESS = 15,   /* Operation incomplete */
    HAL_FULL_RESET  = 16,   /* Full reset done */
    HAL_INV_PMODE   = 17,
} HAL_STATUS;

typedef enum {
    AH_FALSE = 0,       /* NB: lots of code assumes false is zero */
    AH_TRUE  = 1,
} HAL_BOOL;

/*
 * Device revision information.
 */
typedef enum {
    HAL_MAC_VERSION,        /* MAC version id */
    HAL_MAC_REV,            /* MAC revision */
    HAL_PHY_REV,            /* PHY revision */
    HAL_ANALOG5GHZ_REV,     /* 5GHz radio revision */
    HAL_ANALOG2GHZ_REV,     /* 2GHz radio revision */
} HAL_DEVICE_INFO;

typedef enum {
    HAL_CAP_REG_DMN                       = 0,  /* current regulatory domain */
    HAL_CAP_CIPHER                        = 1,  /* hardware supports cipher */
    HAL_CAP_TKIP_MIC                      = 2,  /* handle TKIP MIC in hardware */
    HAL_CAP_TKIP_SPLIT                    = 3,  /* hardware TKIP uses split keys */
    HAL_CAP_PHYCOUNTERS                   = 4,  /* hardware PHY error counters */
    HAL_CAP_DIVERSITY                     = 5,  /* hardware supports fast diversity */
    HAL_CAP_KEYCACHE_SIZE                 = 6,  /* number of entries in key cache */
    HAL_CAP_NUM_TXQUEUES                  = 7,  /* number of hardware xmit queues */
    HAL_CAP_VEOL                          = 9,  /* hardware supports virtual EOL */
    HAL_CAP_PSPOLL                        = 10, /* hardware has working PS-Poll support */
    HAL_CAP_DIAG                          = 11, /* hardware diagnostic support */
    HAL_CAP_COMPRESSION                   = 12, /* hardware supports compression */
    HAL_CAP_BURST                         = 13, /* hardware supports packet bursting */
    HAL_CAP_FASTFRAME                     = 14, /* hardware supoprts fast frames */
    HAL_CAP_TXPOW                         = 15, /* global tx power limit  */
    HAL_CAP_TPC                           = 16, /* per-packet tx power control  */
    HAL_CAP_PHYDIAG                       = 17, /* hardware phy error diagnostic */
    HAL_CAP_BSSIDMASK                     = 18, /* hardware supports bssid mask */
    HAL_CAP_MCAST_KEYSRCH                 = 19, /* hardware has multicast key search */
    HAL_CAP_TSF_ADJUST                    = 20, /* hardware has beacon tsf adjust */
    HAL_CAP_XR                            = 21, /* hardware has XR support  */
    HAL_CAP_WME_TKIPMIC                   = 22, /* hardware can support TKIP MIC when WMM is turned on */
    HAL_CAP_CHAN_HALFRATE                 = 23, /* hardware can support half rate channels */
    HAL_CAP_CHAN_QUARTERRATE              = 24, /* hardware can support quarter rate channels */
    HAL_CAP_RFSILENT                      = 25, /* hardware has rfsilent support  */
    HAL_CAP_TPC_ACK                       = 26, /* ack txpower with per-packet tpc */
    HAL_CAP_TPC_CTS                       = 27, /* cts txpower with per-packet tpc */
    HAL_CAP_11D                           = 28, /* 11d beacon support for changing cc */
    HAL_CAP_PCIE_PS                       = 29, /* pci express power save */
    HAL_CAP_HT                            = 30, /* hardware can support HT */
    HAL_CAP_GTT                           = 31, /* hardware supports global transmit timeout */
    HAL_CAP_FAST_CC                       = 32, /* hardware supports global transmit timeout */
    HAL_CAP_TX_CHAINMASK                  = 33, /* number of tx chains */
    HAL_CAP_RX_CHAINMASK                  = 34, /* number of rx chains */
    HAL_CAP_TX_TRIG_LEVEL_MAX             = 35, /* maximum Tx trigger level */
    HAL_CAP_NUM_GPIO_PINS                 = 36, /* number of GPIO pins */
    HAL_CAP_WOW                           = 37, /* WOW support */       
    HAL_CAP_CST                           = 38, /* hardware supports carrier sense timeout interrupt */
    HAL_CAP_RIFS_RX                       = 39, /* hardware supports RIFS receive */
    HAL_CAP_RIFS_TX                       = 40, /* hardware supports RIFS transmit */
    HAL_CAP_FORCE_PPM                     = 41, /* Force PPM */
    HAL_CAP_RTS_AGGR_LIMIT                = 42, /* aggregation limit with RTS */
    HAL_CAP_4ADDR_AGGR                    = 43, /* hardware is capable of 4addr aggregation */
    HAL_CAP_DFS_DMN                       = 44, /* DFS domain */
    HAL_CAP_EXT_CHAN_DFS                  = 45, /* DFS support for extension channel */
    HAL_CAP_COMBINED_RADAR_RSSI           = 46, /* Is combined RSSI for radar accurate */
    HAL_CAP_ENHANCED_PM_SUPPORT           = 47, /* hardware supports enhanced power management */
    HAL_CAP_AUTO_SLEEP                    = 48, /* hardware can go to network sleep automatically after waking up to receive TIM */
    HAL_CAP_MBSSID_AGGR_SUPPORT           = 49, /* Support for mBSSID Aggregation */
    HAL_CAP_4KB_SPLIT_TRANS               = 50, /* hardware is capable of splitting PCIe transanctions on 4KB boundaries */
    HAL_CAP_REG_FLAG                      = 51, /* Regulatory domain flags */
    HAL_CAP_BB_RIFS_HANG                  = 52, /* BB may hang due to RIFS */
    HAL_CAP_RIFS_RX_ENABLED               = 53, /* RIFS RX currently enabled */
    HAL_CAP_BB_DFS_HANG                   = 54, /* BB may hang due to DFS */
    HAL_CAP_WOW_MATCH_EXACT               = 55, /* WoW exact match pattern support */
    HAL_CAP_ANT_CFG_2GHZ                  = 56, /* Number of antenna configurations */
    HAL_CAP_ANT_CFG_5GHZ                  = 57, /* Number of antenna configurations */
    HAL_CAP_RX_STBC                       = 58,
    HAL_CAP_TX_STBC                       = 59,
    HAL_CAP_BT_COEX                       = 60, /* hardware is capable of bluetooth coexistence */
    HAL_CAP_DYNAMIC_SMPS                  = 61, /* Dynamic MIMO Power Save hardware support */
    HAL_CAP_WOW_MATCH_DWORD               = 62, /* Wow pattern match first dword */
    HAL_CAP_WPS_PUSH_BUTTON               = 63, /* hardware has WPS push button */
    HAL_CAP_WEP_TKIP_AGGR                 = 64, /* hw is capable of aggregation with WEP/TKIP */
    HAL_CAP_WEP_TKIP_AGGR_TX_DELIM        = 65, /* number of delimiters required by hw when transmitting aggregates with WEP/TKIP */
    HAL_CAP_WEP_TKIP_AGGR_RX_DELIM        = 66, /* number of delimiters required by hw when receiving aggregates with WEP/TKIP */
    HAL_CAP_DS                            = 67, /* hardware support double stream: HB93 1x2 only support single stream */
    HAL_CAP_BB_RX_CLEAR_STUCK_HANG        = 68, /* BB may hang due to Rx Clear Stuck Low */
    HAL_CAP_MAC_HANG                      = 69, /* MAC may hang */
    HAL_CAP_MFP                           = 70, /* Management Frame Proctection in hardware */
    HAL_CAP_DEVICE_TYPE                   = 71, /* Type of device */
    HAL_CAP_TS                            = 72, /* hardware supports three streams */
    HAL_INTR_MITIGATION_SUPPORTED         = 73, /* interrupt mitigation */
    HAL_CAP_MAX_TKIP_WEP_HT_RATE          = 74, /* max HT TKIP/WEP rate HW supports */
    HAL_CAP_ENHANCED_DMA_SUPPORT          = 75, /* hardware supports DMA enhancements */
    HAL_CAP_NUM_TXMAPS                    = 76, /* Number of buffers in a transmit descriptor */
    HAL_CAP_TXDESCLEN                     = 77, /* Length of transmit descriptor */
    HAL_CAP_TXSTATUSLEN                   = 78, /* Length of transmit status descriptor */
    HAL_CAP_RXSTATUSLEN                   = 79, /* Length of transmit status descriptor */
    HAL_CAP_RXFIFODEPTH                   = 80, /* Receive hardware FIFO depth */
    HAL_CAP_RXBUFSIZE                     = 81, /* Receive Buffer Length */
    HAL_CAP_NUM_MR_RETRIES                = 82, /* limit on multirate retries */
    HAL_CAP_GEN_TIMER                     = 83, /* Generic(TSF) timer */
    HAL_CAP_OL_PWRCTRL                    = 84, /* open-loop power control */
    HAL_CAP_MAX_WEP_TKIP_HT20_TX_RATEKBPS = 85,
    HAL_CAP_MAX_WEP_TKIP_HT40_TX_RATEKBPS = 86,
    HAL_CAP_MAX_WEP_TKIP_HT20_RX_RATEKBPS = 87,
    HAL_CAP_MAX_WEP_TKIP_HT40_RX_RATEKBPS = 88,
#if ATH_SUPPORT_WAPI
    HAL_CAP_WAPI_MIC                      = 89, /* handle WAPI MIC in hardware */
#endif
#if ATH_SUPPORT_SPECTRAL
    HAL_CAP_SPECTRAL_SCAN                 = 90,
#endif
    HAL_CAP_CFENDFIX                      = 91,
    HAL_CAP_BB_PANIC_WATCHDOG             = 92,
    HAL_CAP_BT_COEX_ASPM_WAR              = 93,
    HAL_CAP_EXTRADELIMWAR                 = 94,
    HAL_CAP_PROXYSTA                      = 95,
    HAL_CAP_HT20_SGI                      = 96,
#if ATH_SUPPORT_RAW_ADC_CAPTURE
    HAL_CAP_RAW_ADC_CAPTURE               = 97,
#endif
#ifdef  ATH_SUPPORT_TxBF
    HAL_CAP_TXBF                          = 98, /* tx beamforming */
#endif
    HAL_CAP_LDPC                 = 99,
    HAL_CAP_RXDESC_TIMESTAMPBITS = 100,
    HAL_CAP_RXTX_ABORT           = 101,
    HAL_CAP_ANI_POLL_INTERVAL    = 102, /* ANI poll interval in ms */
    HAL_CAP_PAPRD_ENABLED        = 103, /* PA Pre-destortion enabled */
    HAL_CAP_HW_UAPSD_TRIG        = 104, /* hardware UAPSD trigger support */
    HAL_CAP_ANT_DIV_COMB         = 105,   /* Enable antenna diversity/combining */ 
    HAL_CAP_PHYRESTART_CLR_WAR   = 106, /* in some cases, clear phy restart to fix bb hang */
    HAL_CAP_ENTERPRISE_MODE      = 107, /* Enterprise mode features */
    HAL_CAP_LDPCWAR              = 108, /* disable RIFS when both RIFS and LDPC is enabled to fix bb hang */
    HAL_CAP_CHANNEL_SWITCH_TIME_USEC  = 109,  /* Used by ResMgr off-channel scheduler */
    HAL_CAP_ENABLE_APM           = 110, /* APM enabled */
    HAL_CAP_PCIE_LCR_EXTSYNC_EN  = 111, /* Extended Sycn bit in LCR. */
    HAL_CAP_PCIE_LCR_OFFSET      = 112, /* PCI-E LCR offset*/
#if ATH_SUPPORT_DYN_TX_CHAINMASK
    HAL_CAP_DYN_TX_CHAINMASK     = 113, /* Dynamic Tx Chainmask. */
#endif /* ATH_SUPPORT_DYN_TX_CHAINMASK */
#if ATH_SUPPORT_WAPI
    HAL_CAP_WAPI_MAX_TX_CHAINS   = 114, /* MAX number of tx chains supported by WAPI engine */
    HAL_CAP_WAPI_MAX_RX_CHAINS   = 115, /* MAX number of rx chains supported by WAPI engine */
#endif
    HAL_CAP_OFDM_RTSCTS          = 116, /* The capability to use OFDM rate for RTS/CTS */
#ifdef ATH_SUPPORT_DFS
    HAL_CAP_ENHANCED_DFS_SUPPORT = 117, /* hardware supports DMA enhancements */
#endif
    HAL_CAP_SMARTANTENNA         = 119, /* Smart Antenna Capabilty */
#ifdef ATH_TRAFFIC_FAST_RECOVER
    HAL_CAP_TRAFFIC_FAST_RECOVER = 120, 
#endif
} HAL_CAPABILITY_TYPE;

typedef enum {
    HAL_CAP_RADAR    = 0,        /* Radar capability */
    HAL_CAP_AR       = 1,        /* AR capability */
    HAL_CAP_SPECTRAL = 2,        /* Spectral scan capability*/
} HAL_PHYDIAG_CAPS;

/*
 * "States" for setting the LED.  These correspond to
 * the possible 802.11 operational states and there may
 * be a many-to-one mapping between these states and the
 * actual hardware states for the LED's (i.e. the hardware
 * may have fewer states).
 *
 * State HAL_LED_SCAN corresponds to external scan, and it's used inside
 * the LED module only.
 */
typedef enum {
    HAL_LED_RESET   = 0,
    HAL_LED_INIT    = 1,
    HAL_LED_READY   = 2,
    HAL_LED_SCAN    = 3,
    HAL_LED_AUTH    = 4,
    HAL_LED_ASSOC   = 5,
    HAL_LED_RUN     = 6
} HAL_LED_STATE;

/*
 * Transmit queue types/numbers.  These are used to tag
 * each transmit queue in the hardware and to identify a set
 * of transmit queues for operations such as start/stop dma.
 */
typedef enum {
    HAL_TX_QUEUE_INACTIVE   = 0,        /* queue is inactive/unused */
    HAL_TX_QUEUE_DATA       = 1,        /* data xmit q's */
    HAL_TX_QUEUE_BEACON     = 2,        /* beacon xmit q */
    HAL_TX_QUEUE_CAB        = 3,        /* "crap after beacon" xmit q */
    HAL_TX_QUEUE_UAPSD      = 4,        /* u-apsd power save xmit q */
    HAL_TX_QUEUE_PSPOLL     = 5,        /* power-save poll xmit q */
    HAL_TX_QUEUE_CFEND      = 6,         /* CF END queue to send cf-end frames*/ 
    HAL_TX_QUEUE_PAPRD      = 7         /* PAPRD queue to send PAPRD traningframes*/ 
} HAL_TX_QUEUE;

#define    HAL_NUM_TX_QUEUES    10        /* max possible # of queues */
#define    HAL_NUM_DATA_QUEUES  4

#ifndef AH_MAX_CHAINS
#define AH_MAX_CHAINS 3
#endif

/*
 * Receive queue types.  These are used to tag
 * each transmit queue in the hardware and to identify a set
 * of transmit queues for operations such as start/stop dma.
 */
typedef enum {
    HAL_RX_QUEUE_HP   = 0,        /* high priority recv queue */
    HAL_RX_QUEUE_LP   = 1,        /* low priority recv queue */
} HAL_RX_QUEUE;

#define    HAL_NUM_RX_QUEUES    2        /* max possible # of queues */

#define    HAL_TXFIFO_DEPTH     8        /* transmit fifo depth */

/*
 * Transmit queue subtype.  These map directly to
 * WME Access Categories (except for UPSD).  Refer
 * to Table 5 of the WME spec.
 */
typedef enum {
    HAL_WME_AC_BK   = 0,            /* background access category */
    HAL_WME_AC_BE   = 1,            /* best effort access category*/
    HAL_WME_AC_VI   = 2,            /* video access category */
    HAL_WME_AC_VO   = 3,            /* voice access category */
    HAL_WME_UPSD    = 4,            /* uplink power save */
    HAL_XR_DATA     = 5,            /* uplink power save */
} HAL_TX_QUEUE_SUBTYPE;

/*
 * Transmit queue flags that control various
 * operational parameters.
 */
typedef enum {
    TXQ_FLAG_TXOKINT_ENABLE            = 0x0001, /* enable TXOK interrupt */
    TXQ_FLAG_TXERRINT_ENABLE           = 0x0001, /* enable TXERR interrupt */
    TXQ_FLAG_TXDESCINT_ENABLE          = 0x0002, /* enable TXDESC interrupt */
    TXQ_FLAG_TXEOLINT_ENABLE           = 0x0004, /* enable TXEOL interrupt */
    TXQ_FLAG_TXURNINT_ENABLE           = 0x0008, /* enable TXURN interrupt */
    TXQ_FLAG_BACKOFF_DISABLE           = 0x0010, /* disable Post Backoff  */
    TXQ_FLAG_COMPRESSION_ENABLE        = 0x0020, /* compression enabled */
    TXQ_FLAG_RDYTIME_EXP_POLICY_ENABLE = 0x0040, /* enable ready time
                                                    expiry policy */
    TXQ_FLAG_FRAG_BURST_BACKOFF_ENABLE = 0x0080, /* enable backoff while
                                                    sending fragment burst*/
} HAL_TX_QUEUE_FLAGS;

typedef struct {
    u_int32_t               tqi_ver;        /* hal TXQ version */
    HAL_TX_QUEUE_SUBTYPE    tqi_subtype;    /* subtype if applicable */
    HAL_TX_QUEUE_FLAGS      tqi_qflags;     /* flags (see above) */
    u_int32_t               tqi_priority;   /* (not used) */
    u_int32_t               tqi_aifs;       /* aifs */
    u_int32_t               tqi_cwmin;      /* cwMin */
    u_int32_t               tqi_cwmax;      /* cwMax */
    u_int16_t               tqi_shretry;    /* rts retry limit */
    u_int16_t               tqi_lgretry;    /* long retry limit (not used)*/
    u_int32_t               tqi_cbrPeriod;
    u_int32_t               tqi_cbrOverflowLimit;
    u_int32_t               tqi_burstTime;
    u_int32_t               tqi_readyTime; /* specified in msecs */
    u_int32_t               tqi_compBuf;    /* compression buffer phys addr */
} HAL_TXQ_INFO;

#if ATH_WOW
/*
 * Pattern Types.
 */
#define AH_WOW_USER_PATTERN_EN      0x1
#define AH_WOW_MAGIC_PATTERN_EN     0x2
#define AH_WOW_LINK_CHANGE          0x4
#define AH_WOW_BEACON_MISS          0x8
#define AH_WOW_MAX_EVENTS           4

#endif

#define HAL_TQI_NONVAL 0xffff

/* token to use for aifs, cwmin, cwmax */
#define    HAL_TXQ_USEDEFAULT    ((u_int32_t) -1)

/* compression definitions */
#define HAL_COMP_BUF_MAX_SIZE           9216            /* 9K */
#define HAL_COMP_BUF_ALIGN_SIZE         512
#define HAL_DECOMP_MASK_SIZE            128

/* ReadyTime % lo and hi bounds */
#define HAL_READY_TIME_LO_BOUND         50
#define HAL_READY_TIME_HI_BOUND         96  /* considering the swba and dma 
                                               response times for cabQ */

/*
 * Transmit packet types.  This belongs in ah_desc.h, but
 * is here so we can give a proper type to various parameters
 * (and not require everyone include the file).
 *
 * NB: These values are intentionally assigned for
 *     direct use when setting up h/w descriptors.
 */
typedef enum {
    HAL_PKT_TYPE_NORMAL     = 0,
    HAL_PKT_TYPE_ATIM       = 1,
    HAL_PKT_TYPE_PSPOLL     = 2,
    HAL_PKT_TYPE_BEACON     = 3,
    HAL_PKT_TYPE_PROBE_RESP = 4,
    HAL_PKT_TYPE_CHIRP      = 5,
    HAL_PKT_TYPE_GRP_POLL   = 6,
} HAL_PKT_TYPE;

/* Rx Filter Frame Types */
typedef enum {
    HAL_RX_FILTER_UCAST     = 0x00000001,   /* Allow unicast frames */
    HAL_RX_FILTER_MCAST     = 0x00000002,   /* Allow multicast frames */
    HAL_RX_FILTER_BCAST     = 0x00000004,   /* Allow broadcast frames */
    HAL_RX_FILTER_CONTROL   = 0x00000008,   /* Allow control frames */
    HAL_RX_FILTER_BEACON    = 0x00000010,   /* Allow beacon frames */
    HAL_RX_FILTER_PROM      = 0x00000020,   /* Promiscuous mode */
    HAL_RX_FILTER_XRPOLL    = 0x00000040,   /* Allow XR poll frame */
    HAL_RX_FILTER_PROBEREQ  = 0x00000080,   /* Allow probe request frames */
    HAL_RX_FILTER_PHYERR    = 0x00000100,   /* Allow phy errors */
    HAL_RX_FILTER_MYBEACON  = 0x00000200,   /* Filter beacons other than mine */
    HAL_RX_FILTER_PHYRADAR  = 0x00002000,   /* Allow phy radar errors*/
    HAL_RX_FILTER_PSPOLL    = 0x00004000,   /* Allow PSPOLL frames */
    HAL_RX_FILTER_ALL_MCAST_BCAST   = 0x00008000,   /* Allow all MCAST,BCAST frames */
        /* 
        ** PHY "Pseudo bits" should be in the upper 16 bits since the lower
        ** 16 bits actually correspond to register 0x803c bits
        */
} HAL_RX_FILTER;

typedef enum {
    HAL_PM_AWAKE            = 0,
    HAL_PM_FULL_SLEEP       = 1,
    HAL_PM_NETWORK_SLEEP    = 2,
    HAL_PM_UNDEFINED        = 3
} HAL_POWER_MODE;

typedef enum {
    HAL_SMPS_DEFAULT = 0,
    HAL_SMPS_SW_CTRL_LOW_PWR,     /* Software control, low power setting */
    HAL_SMPS_SW_CTRL_HIGH_PWR,    /* Software control, high power setting */
    HAL_SMPS_HW_CTRL              /* Hardware Control */
} HAL_SMPS_MODE;

#define AH_ENT_DUAL_BAND_DISABLE        0x00000001
#define AH_ENT_CHAIN2_DISABLE           0x00000002
#define AH_ENT_5MHZ_DISABLE             0x00000004
#define AH_ENT_10MHZ_DISABLE            0x00000008
#define AH_ENT_49GHZ_DISABLE            0x00000010
#define AH_ENT_LOOPBACK_DISABLE         0x00000020
#define AH_ENT_TPC_PERF_DISABLE         0x00000040
#define AH_ENT_MIN_PKT_SIZE_DISABLE     0x00000080
#define AH_ENT_SPECTRAL_PRECISION       0x00000300
#define AH_ENT_SPECTRAL_PRECISION_S     8
#define AH_ENT_RTSCTS_DELIM_WAR         0x00010000
 
#define AH_FIRST_DESC_NDELIMS 60
/*
 * NOTE WELL:
 * These are mapped to take advantage of the common locations for many of
 * the bits on all of the currently supported MAC chips. This is to make
 * the ISR as efficient as possible, while still abstracting HW differences.
 * When new hardware breaks this commonality this enumerated type, as well
 * as the HAL functions using it, must be modified. All values are directly
 * mapped unless commented otherwise.
 */
typedef enum {
    HAL_INT_RX        = 0x00000001,   /* Non-common mapping */
    HAL_INT_RXDESC    = 0x00000002,
    HAL_INT_RXERR     = 0x00000004,
    HAL_INT_RXHP      = 0x00000001,   /* EDMA mapping */
    HAL_INT_RXLP      = 0x00000002,   /* EDMA mapping */
    HAL_INT_RXNOFRM   = 0x00000008,
    HAL_INT_RXEOL     = 0x00000010,
    HAL_INT_RXORN     = 0x00000020,
    HAL_INT_TX        = 0x00000040,   /* Non-common mapping */
    HAL_INT_TXDESC    = 0x00000080,
    HAL_INT_TIM_TIMER = 0x00000100,
    HAL_INT_BBPANIC   = 0x00000400,
    HAL_INT_TXURN     = 0x00000800,
    HAL_INT_MIB       = 0x00001000,
    HAL_INT_RXPHY     = 0x00004000,
    HAL_INT_RXKCM     = 0x00008000,
    HAL_INT_SWBA      = 0x00010000,
    HAL_INT_BMISS     = 0x00040000,
    HAL_INT_BNR       = 0x00100000,   /* Non-common mapping */
    HAL_INT_TIM       = 0x00200000,   /* Non-common mapping */
    HAL_INT_DTIM      = 0x00400000,   /* Non-common mapping */
    HAL_INT_DTIMSYNC  = 0x00800000,   /* Non-common mapping */
    HAL_INT_GPIO      = 0x01000000,
    HAL_INT_CABEND    = 0x02000000,   /* Non-common mapping */
    HAL_INT_TSFOOR    = 0x04000000,   /* Non-common mapping */
    HAL_INT_GENTIMER  = 0x08000000,   /* Non-common mapping */
    HAL_INT_CST       = 0x10000000,   /* Non-common mapping */
    HAL_INT_GTT       = 0x20000000,   /* Non-common mapping */
    HAL_INT_FATAL     = 0x40000000,   /* Non-common mapping */
    HAL_INT_GLOBAL    = 0x80000000,   /* Set/clear IER */
    HAL_INT_BMISC     = HAL_INT_TIM
            | HAL_INT_DTIM
            | HAL_INT_DTIMSYNC
            | HAL_INT_TSFOOR
            | HAL_INT_CABEND,

    /* Interrupt bits that map directly to ISR/IMR bits */
    HAL_INT_COMMON    = HAL_INT_RXNOFRM
            | HAL_INT_RXDESC
            | HAL_INT_RXERR
            | HAL_INT_RXEOL
            | HAL_INT_RXORN
            | HAL_INT_TXURN
            | HAL_INT_TXDESC
            | HAL_INT_MIB
            | HAL_INT_RXPHY
            | HAL_INT_RXKCM
            | HAL_INT_SWBA
            | HAL_INT_BMISS
            | HAL_INT_GPIO,
    HAL_INT_NOCARD   = 0xffffffff    /* To signal the card was removed */
} HAL_INT;

/*
 * MSI vector assignments
 */
typedef enum {
    HAL_MSIVEC_MISC = 0,
    HAL_MSIVEC_TX   = 1,
    HAL_MSIVEC_RXLP = 2,
    HAL_MSIVEC_RXHP = 3,
} HAL_MSIVEC;

typedef enum {
    HAL_INT_LINE = 0,
    HAL_INT_MSI  = 1,
} HAL_INT_TYPE;

/* For interrupt mitigation registers */
typedef enum {
    HAL_INT_RX_FIRSTPKT=0,
    HAL_INT_RX_LASTPKT,
    HAL_INT_TX_FIRSTPKT,
    HAL_INT_TX_LASTPKT,
    HAL_INT_THRESHOLD
} HAL_INT_MITIGATION;

typedef enum {
    HAL_RFGAIN_INACTIVE         = 0,
    HAL_RFGAIN_READ_REQUESTED   = 1,
    HAL_RFGAIN_NEED_CHANGE      = 2
} HAL_RFGAIN;

/*
 * Channels are specified by frequency.
 */
typedef struct {
    u_int16_t   channel;        /* setting in Mhz */
    u_int32_t   channelFlags;   /* see below */
    u_int8_t    privFlags;
    int8_t      maxRegTxPower;  /* max regulatory tx power in dBm */
    int8_t      maxTxPower;     /* max true tx power in 0.5 dBm */
    int8_t      minTxPower;     /* min true tx power in 0.5 dBm */
    u_int8_t    regClassId;     /* regulatory class id of this channel */
    u_int8_t    paprdDone:1,               /* 1: PAPRD DONE, 0: PAPRD Cal not done */
                paprdTableWriteDone:1;     /* 1: DONE, 0: Cal data write not done */
} HAL_CHANNEL;

/* channelFlags */
#define CHANNEL_CW_INT    0x00002 /* CW interference detected on channel */
#define CHANNEL_TURBO     0x00010 /* Turbo Channel */
#define CHANNEL_CCK       0x00020 /* CCK channel */
#define CHANNEL_OFDM      0x00040 /* OFDM channel */
#define CHANNEL_2GHZ      0x00080 /* 2 GHz spectrum channel. */
#define CHANNEL_5GHZ      0x00100 /* 5 GHz spectrum channel */
#define CHANNEL_PASSIVE   0x00200 /* Only passive scan allowed in the channel */
#define CHANNEL_DYN       0x00400 /* dynamic CCK-OFDM channel */
#define CHANNEL_XR        0x00800 /* XR channel */
#define CHANNEL_STURBO    0x02000 /* Static turbo, no 11a-only usage */
#define CHANNEL_HALF      0x04000 /* Half rate channel */
#define CHANNEL_QUARTER   0x08000 /* Quarter rate channel */
#define CHANNEL_HT20      0x10000 /* HT20 channel */
#define CHANNEL_HT40PLUS  0x20000 /* HT40 channel with extention channel above */
#define CHANNEL_HT40MINUS 0x40000 /* HT40 channel with extention channel below */

/* privFlags */
#define CHANNEL_INTERFERENCE    0x01 /* Software use: channel interference 
                                        used for as AR as well as RADAR 
                                        interference detection */
#define CHANNEL_DFS             0x02 /* DFS required on channel */
#define CHANNEL_4MS_LIMIT       0x04 /* 4msec packet limit on this channel */
#define CHANNEL_DFS_CLEAR       0x08 /* if channel has been checked for DFS */
#define CHANNEL_DISALLOW_ADHOC  0x10 /* ad hoc not allowed on this channel */
#define CHANNEL_PER_11D_ADHOC   0x20 /* ad hoc support is per 11d */
#define CHANNEL_EDGE_CH         0x40 /* Edge Channel */
#define CHANNEL_NO_HOSTAP       0x80 /* Non AP Channel */

#define CHANNEL_A           (CHANNEL_5GHZ|CHANNEL_OFDM)
#define CHANNEL_B           (CHANNEL_2GHZ|CHANNEL_CCK)
#define CHANNEL_PUREG       (CHANNEL_2GHZ|CHANNEL_OFDM)
#ifdef notdef
#define CHANNEL_G           (CHANNEL_2GHZ|CHANNEL_DYN)
#else
#define CHANNEL_G           (CHANNEL_2GHZ|CHANNEL_OFDM)
#endif
#define CHANNEL_T           (CHANNEL_5GHZ|CHANNEL_OFDM|CHANNEL_TURBO)
#define CHANNEL_ST          (CHANNEL_T|CHANNEL_STURBO)
#define CHANNEL_108G        (CHANNEL_2GHZ|CHANNEL_OFDM|CHANNEL_TURBO)
#define CHANNEL_108A        CHANNEL_T
#define CHANNEL_X           (CHANNEL_5GHZ|CHANNEL_OFDM|CHANNEL_XR)
#define CHANNEL_G_HT20      (CHANNEL_2GHZ|CHANNEL_HT20)
#define CHANNEL_A_HT20      (CHANNEL_5GHZ|CHANNEL_HT20)
#define CHANNEL_G_HT40PLUS  (CHANNEL_2GHZ|CHANNEL_HT40PLUS)
#define CHANNEL_G_HT40MINUS (CHANNEL_2GHZ|CHANNEL_HT40MINUS)
#define CHANNEL_A_HT40PLUS  (CHANNEL_5GHZ|CHANNEL_HT40PLUS)
#define CHANNEL_A_HT40MINUS (CHANNEL_5GHZ|CHANNEL_HT40MINUS)
#define CHANNEL_ALL \
        (CHANNEL_OFDM|CHANNEL_CCK| CHANNEL_2GHZ | CHANNEL_5GHZ | CHANNEL_TURBO | CHANNEL_HT20 | CHANNEL_HT40PLUS | CHANNEL_HT40MINUS)
#define CHANNEL_ALL_NOTURBO (CHANNEL_ALL &~ CHANNEL_TURBO)


#define HAL_ANTENNA_MIN_MODE  0
#define HAL_ANTENNA_FIXED_A   1
#define HAL_ANTENNA_FIXED_B   2
#define HAL_ANTENNA_MAX_MODE  3

typedef struct {
    u_int32_t   ackrcv_bad;
    u_int32_t   rts_bad;
    u_int32_t   rts_good;
    u_int32_t   fcs_bad;
    u_int32_t   beacons;
} HAL_MIB_STATS;

typedef u_int16_t HAL_CTRY_CODE;        /* country code */
typedef u_int16_t HAL_REG_DOMAIN;       /* regulatory domain code */

enum {
    CTRY_DEBUG      = 0x1ff,    /* debug country code */
    CTRY_DEFAULT    = 0         /* default country code */
};

typedef enum {
        REG_EXT_FCC_MIDBAND            = 0,
        REG_EXT_JAPAN_MIDBAND          = 1,
        REG_EXT_FCC_DFS_HT40           = 2,
        REG_EXT_JAPAN_NONDFS_HT40      = 3,
        REG_EXT_JAPAN_DFS_HT40         = 4
} REG_EXT_BITMAP;       

/*
 * Regulatory related information
 */
typedef struct _HAL_COUNTRY_ENTRY{
    u_int16_t   countryCode;  /* HAL private */
    u_int16_t   regDmnEnum;   /* HAL private */
    u_int16_t   regDmn5G;
    u_int16_t   regDmn2G;
    u_int8_t    isMultidomain;
    u_int8_t    iso[3];       /* ISO CC + (I)ndoor/(O)utdoor or both ( ) */
} HAL_COUNTRY_ENTRY;

enum {
    HAL_MODE_11A              = 0x00001,      /* 11a channels */
    HAL_MODE_TURBO            = 0x00002,      /* 11a turbo-only channels */
    HAL_MODE_11B              = 0x00004,      /* 11b channels */
    HAL_MODE_PUREG            = 0x00008,      /* 11g channels (OFDM only) */
#ifdef notdef                 
    HAL_MODE_11G              = 0x00010,      /* 11g channels (OFDM/CCK) */
#else                         
    HAL_MODE_11G              = 0x00008,      /* XXX historical */
#endif                        
    HAL_MODE_108G             = 0x00020,      /* 11a+Turbo channels */
    HAL_MODE_108A             = 0x00040,      /* 11g+Turbo channels */
    HAL_MODE_XR               = 0x00100,      /* XR channels */
    HAL_MODE_11A_HALF_RATE    = 0x00200,      /* 11A half rate channels */
    HAL_MODE_11A_QUARTER_RATE = 0x00400,      /* 11A quarter rate channels */
    HAL_MODE_11NG_HT20        = 0x00800,      /* 11N-G HT20 channels */
    HAL_MODE_11NA_HT20        = 0x01000,      /* 11N-A HT20 channels */
    HAL_MODE_11NG_HT40PLUS    = 0x02000,      /* 11N-G HT40 + channels */
    HAL_MODE_11NG_HT40MINUS   = 0x04000,      /* 11N-G HT40 - channels */
    HAL_MODE_11NA_HT40PLUS    = 0x08000,      /* 11N-A HT40 + channels */
    HAL_MODE_11NA_HT40MINUS   = 0x10000,      /* 11N-A HT40 - channels */
    HAL_MODE_ALL              = 0xffffffff
};

#define HAL_MODE_11N_MASK \
  ( HAL_MODE_11NG_HT20 | HAL_MODE_11NA_HT20 | HAL_MODE_11NG_HT40PLUS | \
    HAL_MODE_11NG_HT40MINUS | HAL_MODE_11NA_HT40PLUS | HAL_MODE_11NA_HT40MINUS )

typedef struct {
    int     rateCount;      /* NB: for proper padding */
    u_int8_t    rateCodeToIndex[256];    /* back mapping */
    struct {
        u_int8_t    valid;      /* valid for rate control use */
        u_int8_t    phy;        /* CCK/OFDM/XR */
        u_int32_t   rateKbps;   /* transfer rate in kbs */
        u_int8_t    rateCode;   /* rate for h/w descriptors */
        u_int8_t    shortPreamble;  /* mask for enabling short
                         * preamble in CCK rate code */
        u_int8_t    dot11Rate;  /* value for supported rates
                         * info element of MLME */
        u_int8_t    controlRate;    /* index of next lower basic
                         * rate; used for dur. calcs */
        u_int16_t   lpAckDuration;  /* long preamble ACK duration */
        u_int16_t   spAckDuration;  /* short preamble ACK duration*/
    } info[36];
} HAL_RATE_TABLE;

typedef struct {
    u_int       rs_count;       /* number of valid entries */
    u_int8_t    rs_rates[36];       /* rates */
} HAL_RATE_SET;

typedef enum {
    HAL_ANT_VARIABLE = 0,           /* variable by programming */
    HAL_ANT_FIXED_A  = 1,           /* fixed to 11a frequencies */
    HAL_ANT_FIXED_B  = 2,           /* fixed to 11b frequencies */
} HAL_ANT_SETTING;

typedef enum {
    HAL_M_IBSS              = 0,    /* IBSS (adhoc) station */
    HAL_M_STA               = 1,    /* infrastructure station */
    HAL_M_HOSTAP            = 6,    /* Software Access Point */
    HAL_M_MONITOR           = 8,    /* Monitor mode */
    HAL_M_RAW_ADC_CAPTURE   = 10    /* Raw ADC capture mode */
} HAL_OPMODE;

#if ATH_DEBUG
static inline const char *ath_hal_opmode_name(HAL_OPMODE opmode)
{
    switch (opmode) {
    case HAL_M_IBSS:
        return "IBSS";
    case HAL_M_STA:
        return "STA";
    case HAL_M_HOSTAP:
        return "HOSTAP";
    case HAL_M_MONITOR:
        return "MONITOR";
    case HAL_M_RAW_ADC_CAPTURE:
        return "RAW_ADC";
    default:
        return "UNKNOWN";
    }
}
#else
#define ath_hal_opmode_name(opmode) ((const char *) 0x0)
#endif /* ATH_DEBUG */

typedef struct {
    u_int8_t    kv_type;        /* one of HAL_CIPHER */
    u_int8_t    kv_apsd;        /* mask for APSD enabled ACs */
    u_int16_t   kv_len;         /* length in bits */
    u_int8_t    kv_val[16];     /* enough for 128-bit keys */
    u_int8_t    kv_mic[8];      /* TKIP Rx MIC key */
    u_int8_t    kv_txmic[8];    /* TKIP Tx MIC key */
} HAL_KEYVAL;

typedef enum {
    HAL_KEY_TYPE_CLEAR,
    HAL_KEY_TYPE_WEP,
    HAL_KEY_TYPE_AES,
    HAL_KEY_TYPE_TKIP,
#if ATH_SUPPORT_WAPI
    HAL_KEY_TYPE_WAPI,
#endif
    HAL_KEY_PROXY_STA_MASK = 0x10
} HAL_KEY_TYPE;

typedef enum {
    HAL_CIPHER_WEP      = 0,
    HAL_CIPHER_AES_OCB  = 1,
    HAL_CIPHER_AES_CCM  = 2,
    HAL_CIPHER_CKIP     = 3,
    HAL_CIPHER_TKIP     = 4,
    HAL_CIPHER_CLR      = 5,        /* no encryption */
#if ATH_SUPPORT_WAPI
    HAL_CIPHER_WAPI     = 6,        /* WAPI encryption */
#endif
    HAL_CIPHER_MIC      = 127       /* TKIP-MIC, not a cipher */
} HAL_CIPHER;

enum {
    HAL_SLOT_TIME_6  = 6,
    HAL_SLOT_TIME_9  = 9,
    HAL_SLOT_TIME_13 = 13,
    HAL_SLOT_TIME_20 = 20,
    HAL_SLOT_TIME_21 = 21,
};

/* 11n */
typedef enum {
        HAL_HT_MACMODE_20       = 0,            /* 20 MHz operation */
        HAL_HT_MACMODE_2040     = 1,            /* 20/40 MHz operation */
} HAL_HT_MACMODE;

typedef enum {
    HAL_HT_EXTPROTSPACING_20    = 0,            /* 20 MHZ spacing */
    HAL_HT_EXTPROTSPACING_25    = 1,            /* 25 MHZ spacing */
} HAL_HT_EXTPROTSPACING;

typedef struct {
    HAL_HT_MACMODE              ht_macmode;             /* MAC - 20/40 mode */
    HAL_HT_EXTPROTSPACING       ht_extprotspacing;      /* ext channel protection spacing */
} HAL_HT_CWM;

typedef enum {
    HAL_RX_CLEAR_CTL_LOW        = 0x1,  /* force control channel to appear busy */
    HAL_RX_CLEAR_EXT_LOW        = 0x2,  /* force extension channel to appear busy */
} HAL_HT_RXCLEAR;

typedef enum {
    HAL_FREQ_BAND_5GHZ          = 0,
    HAL_FREQ_BAND_2GHZ          = 1,
} HAL_FREQ_BAND;

#define HAL_RATESERIES_RTS_CTS  0x0001  /* use rts/cts w/this series */
#define HAL_RATESERIES_2040     0x0002  /* use ext channel for series */
#define HAL_RATESERIES_HALFGI   0x0004  /* use half-gi for series */
#define HAL_RATESERIES_STBC     0x0008  /* use STBC for series */
#ifdef  ATH_SUPPORT_TxBF
#define HAL_RATESERIES_TXBF     0x0010  /* use TxBF for series */
#endif

typedef struct {
    u_int   Tries;
    u_int   Rate;
    u_int   PktDuration;
    u_int   ChSel;
    u_int   RateFlags;
    u_int   RateIndex;
    u_int   TxPowerCap;     /* in 1/2 dBm units */
} HAL_11N_RATE_SERIES;

enum {
    HAL_TRUE_CHIP = 1,
    HAL_MAC_TO_MAC_EMU,
    HAL_MAC_BB_EMU
};

/*
 * Per-station beacon timer state.  Note that the specified
 * beacon interval (given in TU's) can also include flags
 * to force a TSF reset and to enable the beacon xmit logic.
 * If bs_cfpmaxduration is non-zero the hardware is setup to
 * coexist with a PCF-capable AP.
 */
typedef struct {
    u_int32_t   bs_nexttbtt;        /* next beacon in TU */
    u_int32_t   bs_nextdtim;        /* next DTIM in TU */
    u_int32_t   bs_intval;          /* beacon interval+flags */
#define HAL_BEACON_PERIOD       0x0000ffff  /* beacon interval mask for TU */
#define HAL_BEACON_PERIOD_TU8   0x0007ffff  /* beacon interval mask for TU/8 */
#define HAL_BEACON_ENA          0x00800000  /* beacon xmit enable */
#define HAL_BEACON_RESET_TSF    0x01000000  /* clear TSF */
#define HAL_TSFOOR_THRESHOLD    0x00004240 /* TSF OOR threshold (16k us) */ 
    u_int32_t   bs_dtimperiod;
    u_int16_t   bs_cfpperiod;       /* CFP period in TU */
    u_int16_t   bs_cfpmaxduration;  /* max CFP duration in TU */
    u_int32_t   bs_cfpnext;         /* next CFP in TU */
    u_int16_t   bs_timoffset;       /* byte offset to TIM bitmap */
    u_int16_t   bs_bmissthreshold;  /* beacon miss threshold */
    u_int32_t   bs_sleepduration;   /* max sleep duration */
    u_int32_t   bs_tsfoor_threshold;/* TSF out of range threshold */
} HAL_BEACON_STATE;

/*
 * Per-node statistics maintained by the driver for use in
 * optimizing signal quality and other operational aspects.
 */
typedef struct {
    u_int32_t   ns_avgbrssi;    /* average beacon rssi */
    u_int32_t   ns_avgrssi;     /* average rssi of all received frames */
    u_int32_t   ns_avgtxrssi;   /* average tx rssi */
    u_int32_t   ns_avgtxrate;   /* average IEEE tx rate (in 500kbps units) */
#ifdef ATH_SWRETRY    
    u_int32_t    ns_swretryfailcount; /*number of sw retries which got failed*/
    u_int32_t    ns_swretryframecount; /*total number of frames that are sw retried*/
#endif        
} HAL_NODE_STATS;

/* for VOW_DCS */
typedef struct {
    u_int32_t   cyclecnt_diff;    /* delta cycle count */
    u_int32_t   rxclr_cnt;        /* rx clear count */
    u_int32_t   txframecnt_diff;  /* delta tx frame count */
    u_int32_t   rxframecnt_diff;  /* delta rx frame count */    
    u_int32_t   listen_time;      /* listen time in msec - time for which ch is free */
    u_int32_t   ofdmphyerr_cnt;   /* OFDM err count since last reset */
    u_int32_t   cckphyerr_cnt;    /* CCK err count since last reset */
    HAL_BOOL    valid;             /* if the stats are valid*/        
} HAL_ANISTATS;

typedef struct {
    u_int8_t txctl_offset;
    u_int8_t txctl_numwords;
    u_int8_t txstatus_offset;
    u_int8_t txstatus_numwords;

    u_int8_t rxctl_offset;
    u_int8_t rxctl_numwords;
    u_int8_t rxstatus_offset;
    u_int8_t rxstatus_numwords;

    u_int8_t macRevision;
} HAL_DESC_INFO;

#define HAL_RSSI_EP_MULTIPLIER  (1<<7)  /* pow2 to optimize out * and / */
#define HAL_RATE_EP_MULTIPLIER  (1<<7)  /* pow2 to optimize out * and / */
#define HAL_IS_MULTIPLE_CHAIN(x)    (((x) & ((x) - 1)) != 0)
#define HAL_IS_SINGLE_CHAIN(x)      (((x) & ((x) - 1)) == 0)

#if UMAC_SUPPORT_INTERNALANTENNA
#define HAL_GPIOPIN_RUCKUSDATA   4   /* GPIO pin for data in Ospery 2.2 */
#define HAL_GPIOPIN_RUCKUSSTROBE 5   /* GPIO pin for strobe in Ospery 2.2 */

#define HAL_GPIOPIN_SMARTANT_CTRL0 12   /* GPIO pin for peacock smart antenna control 0 */
#define HAL_GPIOPIN_SMARTANT_CTRL1 13   /* GPIO pin for peacock smart antenna control 1 */
#define HAL_GPIOPIN_SMARTANT_CTRL2 14   /* GPIO pin for peacock smart antenna control 2 */
#endif

/*
 * GPIO Output mux can select from a number of different signals as input.
 * The current implementation uses 5 of these input signals:
 *     - An output value specified by the caller;
 *     - The Attention LED signal provided by the PCIE chip;
 *     - The Power     LED signal provided by the PCIE chip;
 *     - The Network LED pin controlled by the chip's MAC;
 *     - The Power   LED pin controlled by the chip's MAC.
 */
typedef enum {
    HAL_GPIO_OUTPUT_MUX_AS_OUTPUT,
    HAL_GPIO_OUTPUT_MUX_AS_PCIE_ATTENTION_LED,
    HAL_GPIO_OUTPUT_MUX_AS_PCIE_POWER_LED,
    HAL_GPIO_OUTPUT_MUX_AS_MAC_NETWORK_LED,
    HAL_GPIO_OUTPUT_MUX_AS_MAC_POWER_LED,
    HAL_GPIO_OUTPUT_MUX_AS_WLAN_ACTIVE,
    HAL_GPIO_OUTPUT_MUX_AS_TX_FRAME,
    HAL_GPIO_OUTPUT_MUX_AS_RUCKUS_STROBE,
    HAL_GPIO_OUTPUT_MUX_AS_RUCKUS_DATA,
    HAL_GPIO_OUTPUT_MUX_AS_SMARTANT_CTRL0,
    HAL_GPIO_OUTPUT_MUX_AS_SMARTANT_CTRL1,
    HAL_GPIO_OUTPUT_MUX_AS_SMARTANT_CTRL2,
    HAL_GPIO_OUTPUT_MUX_NUM_ENTRIES    // always keep this type last; it must map to the number of entries in this enumeration
} HAL_GPIO_OUTPUT_MUX_TYPE;

typedef enum {
    HAL_GPIO_INTR_LOW = 0,
    HAL_GPIO_INTR_HIGH,
    HAL_GPIO_INTR_DISABLE
} HAL_GPIO_INTR_TYPE;

/*
** Definition of HAL operating parameters
**
** This ENUM provides a mechanism for external objects to update HAL operating
** parameters through a common ioctl-like interface.  The parameter IDs are
** used to identify the specific parameter in question to the get/set interface.
*/

typedef enum {
    HAL_CONFIG_DMA_BEACON_RESPONSE_TIME = 0,
    HAL_CONFIG_SW_BEACON_RESPONSE_TIME,
    HAL_CONFIG_ADDITIONAL_SWBA_BACKOFF,
    HAL_CONFIG_6MB_ACK,                
    HAL_CONFIG_CWMIGNOREEXTCCA,        
    HAL_CONFIG_FORCEBIAS,              
    HAL_CONFIG_FORCEBIASAUTO,
    HAL_CONFIG_PCIEPOWERSAVEENABLE,    
    HAL_CONFIG_PCIEL1SKPENABLE,        
    HAL_CONFIG_PCIECLOCKREQ,           
    HAL_CONFIG_PCIEWAEN,               
    HAL_CONFIG_PCIEDETACH,
    HAL_CONFIG_PCIEPOWERRESET,         
    HAL_CONFIG_PCIERESTORE,         
    HAL_CONFIG_HTENABLE,               
    HAL_CONFIG_DISABLETURBOG,          
    HAL_CONFIG_OFDMTRIGLOW,            
    HAL_CONFIG_OFDMTRIGHIGH,           
    HAL_CONFIG_CCKTRIGHIGH,            
    HAL_CONFIG_CCKTRIGLOW,             
    HAL_CONFIG_ENABLEANI,              
    HAL_CONFIG_NOISEIMMUNITYLVL,       
    HAL_CONFIG_OFDMWEAKSIGDET,         
    HAL_CONFIG_CCKWEAKSIGTHR,          
    HAL_CONFIG_SPURIMMUNITYLVL,        
    HAL_CONFIG_FIRSTEPLVL,             
    HAL_CONFIG_RSSITHRHIGH,            
    HAL_CONFIG_RSSITHRLOW,             
    HAL_CONFIG_DIVERSITYCONTROL,       
    HAL_CONFIG_ANTENNASWITCHSWAP,
    HAL_CONFIG_FULLRESETWARENABLE,
    HAL_CONFIG_DISABLEPERIODICPACAL,
    HAL_CONFIG_DEBUG,
    HAL_CONFIG_KEEPALIVETIMEOUT,
    HAL_CONFIG_DEFAULTANTENNASET,
    HAL_CONFIG_REGREAD_BASE,
#ifdef ATH_SUPPORT_TxBF
    HAL_CONFIG_TxBF_CTL,
#endif
#if ATH_SUPPORT_IBSS_WPA2
    HAL_CONFIG_KEYCACHE_PRINT,
#endif
    HAL_CONFIG_SHOW_BB_PANIC,
    HAL_CONFIG_INTR_MITIGATION_RX,

    HAL_CONFIG_OPS_PARAM_MAX
} HAL_CONFIG_OPS_PARAMS_TYPE;

/*
 * Diagnostic interface.  This is an open-ended interface that
 * is opaque to applications.  Diagnostic programs use this to
 * retrieve internal data structures, etc.  There is no guarantee
 * that calling conventions for calls other than HAL_DIAG_REVS
 * are stable between HAL releases; a diagnostic application must
 * use the HAL revision information to deal with ABI/API differences.
 */
enum {
    HAL_DIAG_REVS           = 0,    /* MAC/PHY/Radio revs */
    HAL_DIAG_EEPROM         = 1,    /* EEPROM contents */
    HAL_DIAG_EEPROM_EXP_11A = 2,    /* EEPROM 5112 power exp for 11a */
    HAL_DIAG_EEPROM_EXP_11B = 3,    /* EEPROM 5112 power exp for 11b */
    HAL_DIAG_EEPROM_EXP_11G = 4,    /* EEPROM 5112 power exp for 11g */
    HAL_DIAG_ANI_CURRENT    = 5,    /* ANI current channel state */
    HAL_DIAG_ANI_OFDM       = 6,    /* ANI OFDM timing error stats */
    HAL_DIAG_ANI_CCK        = 7,    /* ANI CCK timing error stats */
    HAL_DIAG_ANI_STATS      = 8,    /* ANI statistics */
    HAL_DIAG_RFGAIN         = 9,    /* RfGain GAIN_VALUES */
    HAL_DIAG_RFGAIN_CURSTEP = 10,   /* RfGain GAIN_OPTIMIZATION_STEP */
    HAL_DIAG_PCDAC          = 11,   /* PCDAC table */
    HAL_DIAG_TXRATES        = 12,   /* Transmit rate table */
    HAL_DIAG_REGS           = 13,   /* Registers */
    HAL_DIAG_ANI_CMD        = 14,   /* ANI issue command */
    HAL_DIAG_SETKEY         = 15,   /* Set keycache backdoor */
    HAL_DIAG_RESETKEY       = 16,   /* Reset keycache backdoor */
    HAL_DIAG_EEREAD         = 17,   /* Read EEPROM word */
    HAL_DIAG_EEWRITE        = 18,   /* Write EEPROM word */
    HAL_DIAG_TXCONT         = 19,   /* TX99 settings */
    HAL_DIAG_SET_RADAR      = 20,   /* Set Radar thresholds */
    HAL_DIAG_GET_RADAR      = 21,   /* Get Radar thresholds */
    HAL_DIAG_USENOL         = 22,   /* Turn on/off the use of the radar NOL */
    HAL_DIAG_GET_USENOL     = 23,   /* Get status of the use of the radar NOL */
    HAL_DIAG_REGREAD        = 24,   /* Reg reads */
    HAL_DIAG_REGWRITE       = 25,   /* Reg writes */
    HAL_DIAG_GET_REGBASE    = 26,   /* Get register base */
    HAL_DIAG_PRINT_REG      = 27,
    HAL_DIAG_RDWRITE        = 28,   /* Write regulatory domain */
    HAL_DIAG_RDREAD         = 29    /* Get regulatory domain */
};

/* For register print */
#define HAL_DIAG_PRINT_REG_COUNTER  0x00000001 /* print tf, rf, rc, cc counters */
#define HAL_DIAG_PRINT_REG_ALL      0x80000000 /* print all registers */


#ifdef ATH_CCX
typedef struct {
    u_int8_t    clockRate;
    u_int32_t   noiseFloor;
    u_int8_t    ccaThreshold;
} HAL_CHANNEL_DATA;

typedef struct halCounters {
    u_int32_t   txFrameCount;
    u_int32_t   rxFrameCount;
    u_int32_t   rxClearCount;
    u_int32_t   cycleCount;
    u_int8_t    isRxActive;     // AH_TRUE (1) or AH_FALSE (0)
    u_int8_t    isTxActive;     // AH_TRUE (1) or AH_FALSE (0)
} HAL_COUNTERS;
#endif

/* DFS defines */

struct  dfs_pulse {
    u_int32_t    rp_numpulses;     /* Num of pulses in radar burst */
    u_int32_t    rp_pulsedur;      /* Duration of each pulse in usecs */
    u_int32_t    rp_pulsefreq;     /* Frequency of pulses in burst */
    u_int32_t    rp_max_pulsefreq; /* Frequency of pulses in burst */
    u_int32_t    rp_patterntype;   /* fixed or variable pattern type*/
    u_int32_t    rp_pulsevar;      /* Time variation of pulse duration for
                                      matched filter (single-sided) in usecs */
    u_int32_t    rp_threshold;     /* Threshold for MF output to indicate 
                                      radar match */
    u_int32_t    rp_mindur;        /* Min pulse duration to be considered for
                                      this pulse type */
    u_int32_t    rp_maxdur;        /* Max pusle duration to be considered for
                                      this pulse type */
    u_int32_t    rp_rssithresh;    /* Minimum rssi to be considered a radar pulse */
    u_int32_t    rp_meanoffset;    /* Offset for timing adjustment */
    int32_t      rp_rssimargin;    /* rssi threshold margin. In Turbo Mode HW reports rssi 3dBm */
                                   /* lower than in non TURBO mode. This will be used to offset that diff.*/
    u_int32_t    rp_pulseid;       /* Unique ID for identifying filter */
};

struct  dfs_staggered_pulse {
    u_int32_t    rp_numpulses;        /* Num of pulses in radar burst */
    u_int32_t    rp_pulsedur;         /* Duration of each pulse in usecs */
    u_int32_t    rp_min_pulsefreq;    /* Frequency of pulses in burst */
    u_int32_t    rp_max_pulsefreq;    /* Frequency of pulses in burst */
    u_int32_t    rp_patterntype;      /* fixed or variable pattern type*/
    u_int32_t    rp_pulsevar;         /* Time variation of pulse duration for
                                         matched filter (single-sided) in usecs */
    u_int32_t    rp_threshold;        /* Thershold for MF output to indicate 
                                         radar match */
    u_int32_t    rp_mindur;           /* Min pulse duration to be considered for
                                         this pulse type */
    u_int32_t    rp_maxdur;           /* Max pusle duration to be considered for
                                         this pulse type */
    u_int32_t    rp_rssithresh;       /* Minimum rssi to be considered a radar pulse */
    u_int32_t    rp_meanoffset;       /* Offset for timing adjustment */
    int32_t      rp_rssimargin;       /* rssi threshold margin. In Turbo Mode HW reports rssi 3dBm */
                                      /* lower than in non TURBO mode. This will be used to offset that diff.*/
    u_int32_t    rp_pulseid;          /* Unique ID for identifying filter */
};

struct dfs_bin5pulse {
        u_int32_t       b5_threshold;          /* Number of bin5 pulses to indicate detection */
        u_int32_t       b5_mindur;             /* Min duration for a bin5 pulse */
        u_int32_t       b5_maxdur;             /* Max duration for a bin5 pulse */
        u_int32_t       b5_timewindow;         /* Window over which to count bin5 pulses */
        u_int32_t       b5_rssithresh;         /* Min rssi to be considered a pulse */
        u_int32_t       b5_rssimargin;         /* rssi threshold margin. In Turbo Mode HW reports rssi 3dB */
};

typedef struct {
        int32_t         pe_firpwr;      /* FIR pwr out threshold */
        int32_t         pe_rrssi;       /* Radar rssi thresh */
        int32_t         pe_height;      /* Pulse height thresh */
        int32_t         pe_prssi;       /* Pulse rssi thresh */
        int32_t         pe_inband;      /* Inband thresh */
        /* The following params are only for AR5413 and later */
        u_int32_t       pe_relpwr;      /* Relative power threshold in 0.5dB steps */
        u_int32_t       pe_relstep;     /* Pulse Relative step threshold in 0.5dB steps */
        u_int32_t       pe_maxlen;      /* Max length of radar sign in 0.8us units */
        HAL_BOOL        pe_usefir128;   /* Use the average in-band power measured over 128 cycles */         
        HAL_BOOL        pe_blockradar;  /* Enable to block radar check if pkt detect is done via OFDM
                                           weak signal detect or pkt is detected immediately after tx
                                           to rx transition */
        HAL_BOOL        pe_enmaxrssi;   /* Enable to use the max rssi instead of the last rssi during
                                           fine gain changes for radar detection */
} HAL_PHYERR_PARAM;

#define HAL_PHYERR_PARAM_NOVAL  65535
#define HAL_PHYERR_PARAM_ENABLE 0x8000  /* Enable/Disable if applicable */

/* DFS defines end */

#ifdef ATH_BT_COEX
/*
 * BT Co-existence definitions
 */
typedef enum {
    HAL_BT_MODULE_CSR_BC4       = 0,    /* CSR BlueCore v4 */
    HAL_BT_MODULE_JANUS         = 1,    /* Kite + Valkyrie combo */
    HAL_BT_MODULE_HELIUS        = 2,    /* Kiwi + Valkyrie combo */
    HAL_MAX_BT_MODULES
} HAL_BT_MODULE;

typedef struct {
    HAL_BT_MODULE   bt_module;
    u_int8_t        bt_coexConfig;
    u_int8_t        bt_gpioBTActive;
    u_int8_t        bt_gpioBTPriority;
    u_int8_t        bt_gpioWLANActive;
    u_int8_t        bt_activePolarity;
    HAL_BOOL        bt_singleAnt;
    u_int8_t        bt_dutyCycle;
    u_int8_t        bt_isolation;
    u_int8_t        bt_period;
} HAL_BT_COEX_INFO;

typedef enum {
    HAL_BT_COEX_MODE_LEGACY     = 0,    /* legacy rx_clear mode */
    HAL_BT_COEX_MODE_UNSLOTTED  = 1,    /* untimed/unslotted mode */
    HAL_BT_COEX_MODE_SLOTTED    = 2,    /* slotted mode */
    HAL_BT_COEX_MODE_DISALBED   = 3,    /* coexistence disabled */
} HAL_BT_COEX_MODE;

typedef enum {
    HAL_BT_COEX_CFG_NONE,               /* No bt coex enabled */
    HAL_BT_COEX_CFG_2WIRE_2CH,          /* 2-wire with 2 chains */
    HAL_BT_COEX_CFG_2WIRE_CH1,          /* 2-wire with ch1 */
    HAL_BT_COEX_CFG_2WIRE_CH0,          /* 2-wire with ch0 */
    HAL_BT_COEX_CFG_3WIRE,              /* 3-wire */
} HAL_BT_COEX_CFG;

typedef enum {
    HAL_BT_COEX_SET_ACK_PWR     = 0,    /* Change ACK power setting */
    HAL_BT_COEX_LOWER_TX_PWR,           /* Change transmit power */
    HAL_BT_COEX_ANTENNA_DIVERSITY,      /* Enable RX diversity for Kite */
} HAL_BT_COEX_SET_PARAMETER;

#define HAL_BT_COEX_FLAG_LOW_ACK_PWR        0x00000001
#define HAL_BT_COEX_FLAG_LOWER_TX_PWR       0x00000002
#define HAL_BT_COEX_FLAG_ANT_DIV_ALLOW      0x00000004    /* Check Rx Diversity is allowed */
#define HAL_BT_COEX_FLAG_ANT_DIV_ENABLE     0x00000008    /* Check Diversity is on or off */

#define HAL_BT_COEX_ANTDIV_CONTROL1_ENABLE  0x0b
#define HAL_BT_COEX_ANTDIV_CONTROL2_ENABLE  0x09          /* main: LNA1, alt: LNA2 */
#define HAL_BT_COEX_ANTDIV_CONTROL1_FIXED_A 0x04
#define HAL_BT_COEX_ANTDIV_CONTROL2_FIXED_A 0x09
#define HAL_BT_COEX_ANTDIV_CONTROL1_FIXED_B 0x02
#define HAL_BT_COEX_ANTDIV_CONTROL2_FIXED_B 0x06

#define HAL_BT_COEX_ISOLATION_FOR_NO_COEX   30

#define HAL_BT_COEX_ANT_DIV_SWITCH_COM      0x66666666

#define HAL_BT_COEX_HELIUS_CHAINMASK        0x02

#define HAL_BT_COEX_LOW_ACK_POWER           0x0
#define HAL_BT_COEX_HIGH_ACK_POWER          0x3f3f3f

typedef enum {
    HAL_BT_COEX_NO_STOMP = 0,
    HAL_BT_COEX_STOMP_ALL,
    HAL_BT_COEX_STOMP_LOW,
    HAL_BT_COEX_STOMP_NONE,
    HAL_BT_COEX_STOMP_ALL_FORCE,
    HAL_BT_COEX_STOMP_LOW_FORCE,
} HAL_BT_COEX_STOMP_TYPE;

/*
 * Weight table configurations.
 */
#define AR5416_BT_WGHT                     0xff55
#define AR5416_STOMP_ALL_WLAN_WGHT         0xfcfc
#define AR5416_STOMP_LOW_WLAN_WGHT         0xa8a8
#define AR5416_STOMP_NONE_WLAN_WGHT        0x0000
#define AR5416_STOMP_ALL_FORCE_WLAN_WGHT   0xffff       // Stomp BT even when WLAN is idle
#define AR5416_STOMP_LOW_FORCE_WLAN_WGHT   0xaaaa       // Stomp BT even when WLAN is idle

#define AR9300_BT_WGHT                     0xcccc4444
#define AR9300_STOMP_ALL_WLAN_WGHT0        0xfffffff0
#define AR9300_STOMP_ALL_WLAN_WGHT1        0xfffffff0
#define AR9300_STOMP_LOW_WLAN_WGHT0        0x88888880
#define AR9300_STOMP_LOW_WLAN_WGHT1        0x88888880
#define AR9300_STOMP_NONE_WLAN_WGHT0       0x00000000
#define AR9300_STOMP_NONE_WLAN_WGHT1       0x00000000
#define AR9300_STOMP_ALL_FORCE_WLAN_WGHT0  0xffffffff   // Stomp BT even when WLAN is idle
#define AR9300_STOMP_ALL_FORCE_WLAN_WGHT1  0xffffffff
#define AR9300_STOMP_LOW_FORCE_WLAN_WGHT0  0x88888888   // Stomp BT even when WLAN is idle
#define AR9300_STOMP_LOW_FORCE_WLAN_WGHT1  0x88888888

typedef struct {
    u_int8_t            bt_timeExtend;      /* extend rx_clear after tx/rx to protect
                                             * the burst (in usec). */
    HAL_BOOL            bt_txstateExtend;   /* extend rx_clear as long as txsm is
                                             * transmitting or waiting for ack. */
    HAL_BOOL            bt_txframeExtend;   /* extend rx_clear so that when tx_frame
                                             * is asserted, rx_clear will drop. */
    HAL_BT_COEX_MODE    bt_mode;            /* coexistence mode */
    HAL_BOOL            bt_quietCollision;  /* treat BT high priority traffic as
                                             * a quiet collision */
    HAL_BOOL            bt_rxclearPolarity; /* invert rx_clear as WLAN_ACTIVE */
    u_int8_t            bt_priorityTime;    /* slotted mode only. indicate the time in usec
                                             * from the rising edge of BT_ACTIVE to the time
                                             * BT_PRIORITY can be sampled to indicate priority. */
    u_int8_t            bt_firstSlotTime;   /* slotted mode only. indicate the time in usec
                                             * from the rising edge of BT_ACTIVE to the time
                                             * BT_PRIORITY can be sampled to indicate tx/rx and
                                             * BT_FREQ is sampled. */
    HAL_BOOL            bt_holdRxclear;     /* slotted mode only. rx_clear and bt_ant decision
                                             * will be held the entire time that BT_ACTIVE is asserted,
                                             * otherwise the decision is made before every slot boundry. */
} HAL_BT_COEX_CONFIG;
#endif

/* SPECTRAL SCAN defines begin */
typedef struct {
        u_int16_t       ss_fft_period;  /* Skip interval for FFT reports */
        u_int16_t       ss_period;      /* Spectral scan period */
        u_int16_t       ss_count;       /* # of reports to return from ss_active */
        HAL_BOOL        ss_short_report;/* Set to report ony 1 set of FFT results */
        u_int8_t        radar_bin_thresh_sel;
} HAL_SPECTRAL_PARAM;
#define HAL_SPECTRAL_PARAM_NOVAL  0xFFFF
#define HAL_SPECTRAL_PARAM_ENABLE 0x8000  /* Enable/Disable if applicable */
/* SPECTRAL SCAN defines end */

typedef struct hal_calibration_timer {
    const u_int32_t    cal_timer_group;
    const u_int32_t    cal_timer_interval;
    u_int64_t          cal_timer_ts;
} HAL_CALIBRATION_TIMER;

typedef enum hal_cal_query {
	HAL_QUERY_CALS,
	HAL_QUERY_RERUN_CALS
} HAL_CAL_QUERY;

typedef struct mac_dbg_regs {
    u_int32_t dma_dbg_0;
    u_int32_t dma_dbg_1;
    u_int32_t dma_dbg_2;
    u_int32_t dma_dbg_3;
    u_int32_t dma_dbg_4;
    u_int32_t dma_dbg_5;
    u_int32_t dma_dbg_6;
    u_int32_t dma_dbg_7;
} mac_dbg_regs_t;

typedef enum hal_mac_hangs {
    dcu_chain_state = 0x1,
    dcu_complete_state = 0x2,
    qcu_state = 0x4,
    qcu_fsp_ok = 0x8,
    qcu_fsp_state = 0x10,
    qcu_stitch_state = 0x20,
    qcu_fetch_state = 0x40,
    qcu_complete_state = 0x80
} hal_mac_hangs_t;

typedef struct hal_mac_hang_check {
    u_int8_t dcu_chain_state;
    u_int8_t dcu_complete_state;
    u_int8_t qcu_state;
    u_int8_t qcu_fsp_ok;
    u_int8_t qcu_fsp_state;
    u_int8_t qcu_stitch_state;
    u_int8_t qcu_fetch_state;
    u_int8_t qcu_complete_state;
} hal_mac_hang_check_t;

typedef struct hal_bb_hang_check {
    u_int16_t hang_reg_offset;
    u_int32_t hang_val;
    u_int32_t hang_mask;
    u_int32_t hang_offset;
} hal_hw_hang_check_t; 

typedef enum hal_hw_hangs {
    HAL_DFS_BB_HANG_WAR          = 0x1,
    HAL_RIFS_BB_HANG_WAR         = 0x2,
    HAL_RX_STUCK_LOW_BB_HANG_WAR = 0x4,
    HAL_MAC_HANG_WAR             = 0x8,
    HAL_PHYRESTART_CLR_WAR       = 0x10,
    HAL_MAC_HANG_DETECTED        = 0x40000000,
    HAL_BB_HANG_DETECTED         = 0x80000000
} hal_hw_hangs_t;

#ifndef REMOVE_PKT_LOG
/* Hal packetlog defines */
typedef struct hal_log_data_ani {
    u_int8_t phyStatsDisable;
    u_int8_t noiseImmunLvl;
    u_int8_t spurImmunLvl;
    u_int8_t ofdmWeakDet;
    u_int8_t cckWeakThr;
    u_int16_t firLvl;
    u_int16_t listenTime;
    u_int32_t cycleCount;
    u_int32_t ofdmPhyErrCount;
    u_int32_t cckPhyErrCount;
    int8_t rssi;
    int32_t misc[8];            /* Can be used for HT specific or other misc info */
    /* TBD: Add other required log information */
} HAL_LOG_DATA_ANI;
/* Hal packetlog defines end*/
#endif

/*
 * MFP decryption options for initializing the MAC.
 */

typedef enum {
    HAL_MFP_QOSDATA = 0,        /* Decrypt MFP frames like QoS data frames. All chips before Merlin. */
    HAL_MFP_PASSTHRU,       /* Don't decrypt MFP frames at all. Passthrough */            
    HAL_MFP_HW_CRYPTO       /* hardware decryption enabled. Merlin can do it. */
} HAL_MFP_OPT_t;

/* LNA config supported */
typedef enum {
    HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2 = 0,
    HAL_ANT_DIV_COMB_LNA2            = 1,
    HAL_ANT_DIV_COMB_LNA1            = 2,
    HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2  = 3,
} HAL_ANT_DIV_COMB_LNA_CONF;

#ifdef ATH_ANT_DIV_COMB
typedef struct {
    u_int8_t main_lna_conf;
    u_int8_t alt_lna_conf;
    u_int8_t fast_div_bias;
    u_int8_t main_gaintb;
    u_int8_t alt_gaintb;
    u_int8_t antdiv_configgroup;
    int8_t   lna1_lna2_delta;
} HAL_ANT_COMB_CONFIG;

#define DEFAULT_ANTDIV_CONFIG_GROUP      0x00
#define HAL_ANTDIV_CONFIG_GROUP_1        0x01   /* Hornet 1.1 */
#define HAL_ANTDIV_CONFIG_GROUP_2        0x02   /* Poseidon 1.1 */
#define HAL_ANTDIV_CONFIG_GROUP_3        0x03   /* not yet used */
#endif

/*
** Transmit Beamforming Capabilities
*/  
#ifdef  ATH_SUPPORT_TxBF    
typedef struct hal_txbf_caps {
    u_int8_t channel_estimation_cap;
    u_int8_t csi_max_rows_bfer;
    u_int8_t comp_bfer_antennas;
    u_int8_t noncomp_bfer_antennas;
    u_int8_t csi_bfer_antennas;
    u_int8_t minimal_grouping;
    u_int8_t explicit_comp_bf;
    u_int8_t explicit_noncomp_bf;
    u_int8_t explicit_csi_feedback;
    u_int8_t explicit_comp_steering;
    u_int8_t explicit_noncomp_steering;
    u_int8_t explicit_csi_txbf_capable;
    u_int8_t calibration;
    u_int8_t implicit_txbf_capable;
    u_int8_t tx_ndp_capable;
    u_int8_t rx_ndp_capable;
    u_int8_t tx_staggered_sounding;
    u_int8_t rx_staggered_sounding;
    u_int8_t implicit_rx_capable;
} HAL_TXBF_CAPS;
#endif

/*
 * Generic Timer domain
 */
typedef enum {
    HAL_GEN_TIMER_TSF = 0,
    HAL_GEN_TIMER_TSF2,
    HAL_GEN_TIMER_TSF_ANY
} HAL_GEN_TIMER_DOMAIN;

/* ah_reset_reason */
enum{
    HAL_RESET_NONE = 0x0,
    HAL_RESET_BBPANIC = 0x1,
};

/*
 * Green Tx, Based on different RSSI of Received Beacon thresholds, 
 * using different tx power by modified register tx power related values.
 * The thresholds are decided by system team.
 */
#define GreenTX_thres1  56   /* in dB */
#define GreenTX_thres2  36   /* in dB */

typedef enum {
    HAL_RSSI_TX_POWER_NONE      = 0,
    HAL_RSSI_TX_POWER_SHORT     = 1,    /* short range, reduce OB/DB bias current and disable PAL */
    HAL_RSSI_TX_POWER_MIDDLE    = 2,    /* middle range, reduce OB/DB bias current and PAL is enabled */
    HAL_RSSI_TX_POWER_LONG      = 3,    /* long range, orig. OB/DB bias current and PAL is enabled */
} HAL_RSSI_TX_POWER;

/*
** Forward Reference
*/      

struct ath_desc;
struct ath_rx_status;
struct ath_tx_status;

typedef struct halvowstats {
    u_int32_t   tx_frame_count;
    u_int32_t   rx_frame_count;
    u_int32_t   rx_clear_count;
    u_int32_t   cycle_count;
    u_int32_t   ext_cycle_count;
} HAL_VOWSTATS;

struct hal_bb_panic_info {
    u_int32_t status;
    u_int32_t tsf;
    u_int32_t phy_panic_wd_ctl1;
    u_int32_t phy_panic_wd_ctl2;
    u_int32_t phy_gen_ctrl;
    u_int32_t rxc_pcnt;
    u_int32_t rxf_pcnt;
    u_int32_t txf_pcnt;
    u_int32_t cycles;
    u_int32_t wd;
    u_int32_t det;
    u_int32_t rdar;
    u_int32_t rODFM;
    u_int32_t rCCK;
    u_int32_t tODFM;
    u_int32_t tCCK;
    u_int32_t agc;
    u_int32_t src;
};

#ifdef AH_ANALOG_SHADOW_READ
#define RF_BEGIN 0x7800
#define RF_END 0x7900
#endif
/*
 * Hardware Access Layer (HAL) API.
 *
 * Clients of the HAL call ath_hal_attach to obtain a reference to an
 * ath_hal structure for use with the device.  Hardware-related operations
 * that follow must call back into the HAL through interface, supplying
 * the reference as the first parameter.  Note that before using the
 * reference returned by ath_hal_attach the caller should verify the
 * ABI version number.
 */
struct ath_hal {
    u_int32_t   ah_magic;   /* consistency check magic number */
    u_int32_t   ah_abi;     /* HAL ABI version */
#define HAL_ABI_VERSION 0x05071100  /* YYMMDDnn */
    u_int16_t   ah_devid;   /* PCI device ID */
    u_int16_t   ah_subvendorid; /* PCI subvendor ID */
    HAL_ADAPTER_HANDLE ah_osdev; /* back pointer to OS adapter handle */
    HAL_SOFTC   ah_sc;      /* back pointer to driver/os state */
    HAL_BUS_TAG ah_st;      /* params for register r+w */
    HAL_BUS_HANDLE  ah_sh;
    HAL_BUS_TYPE    ah_bustype;
    HAL_CTRY_CODE   ah_countryCode;

    u_int32_t   ah_macVersion;  /* MAC version id */
    u_int16_t   ah_macRev;  /* MAC revision */
    u_int16_t   ah_phyRev;  /* PHY revision */
    /* NB: when only one radio is present the rev is in 5Ghz */
    u_int16_t   ah_analog5GhzRev;/* 5GHz radio revision */
    u_int16_t   ah_analog2GhzRev;/* 2GHz radio revision */
    u_int8_t    ah_decompMask[HAL_DECOMP_MASK_SIZE]; /* decomp mask array */
#ifdef __CARRIER_PLATFORM__
    u_int8_t    ah_legacy_dev; /* is the device legacy(11a/b/g) or 11n */
#endif
#ifdef ATH_TX99_DIAG
    u_int16_t   ah_pwr_offset;
#endif

    u_int8_t    ah_rxNumCurAggrGood; /* number of good frames in current aggr */
    u_int8_t    ah_rxLastAggrBssid[6];
    const HAL_RATE_TABLE *__ahdecl(*ah_getRateTable)(struct ath_hal *,
                u_int mode);
    void      __ahdecl(*ah_detach)(struct ath_hal*);

    /* Reset functions */
    HAL_BOOL  __ahdecl(*ah_reset)(struct ath_hal *, HAL_OPMODE,
                HAL_CHANNEL *, HAL_HT_MACMODE, u_int8_t,
                                u_int8_t, HAL_HT_EXTPROTSPACING,
                                HAL_BOOL bChannelChange, HAL_STATUS *status,
                                int is_scan);

    HAL_BOOL  __ahdecl(*ah_phyDisable)(struct ath_hal *);
    HAL_BOOL  __ahdecl(*ah_disable)(struct ath_hal *);
        void  __ahdecl(*ah_configPciPowerSave)(struct ath_hal *, int, int);
    void      __ahdecl(*ah_setPCUConfig)(struct ath_hal *);
    HAL_BOOL  __ahdecl(*ah_perCalibration)(struct ath_hal*, HAL_CHANNEL *, 
                u_int8_t, HAL_BOOL, HAL_BOOL *, int, u_int32_t *);
    void      __ahdecl(*ah_resetCalValid)(struct ath_hal*, HAL_CHANNEL *,
                       HAL_BOOL *, u_int32_t);
    HAL_BOOL  __ahdecl(*ah_setTxPowerLimit)(struct ath_hal *, u_int32_t,
                u_int16_t, u_int16_t);
    HAL_BOOL  __ahdecl(*ah_radarWait)(struct ath_hal *, HAL_CHANNEL *);

    /* New DFS declarations*/
    void       __ahdecl(*ah_arCheckDfs)(struct ath_hal *, HAL_CHANNEL *chan);
    void       __ahdecl(*ah_arDfsFound)(struct ath_hal *, HAL_CHANNEL *chan,
                 u_int64_t nolTime);
    void       __ahdecl(*ah_arEnableDfs)(struct ath_hal *,
                 HAL_PHYERR_PARAM *pe);
    void       __ahdecl(*ah_arGetDfsThresh)(struct ath_hal *,
                 HAL_PHYERR_PARAM *pe);
    struct dfs_pulse *__ahdecl(*ah_arGetDfsRadars)(struct ath_hal *ah,
                 u_int32_t dfsdomain, int *numradars,
                 struct dfs_bin5pulse **bin5pulses, int *numb5radars,
                 HAL_PHYERR_PARAM *pe);
    HAL_CHANNEL* __ahdecl(*ah_getExtensionChannel)(struct ath_hal*ah);
    HAL_BOOL __ahdecl(*ah_isFastClockEnabled)(struct ath_hal*ah);
    void      __ahdecl(*ah_adjust_difs)(struct ath_hal *, u_int32_t);
    void      __ahdecl(*ah_dfs_cac_war)(struct ath_hal *, u_int32_t);
    void      __ahdecl(*ah_cac_tx_quiet)(struct ath_hal *, bool);
    /* Xr Functions */
    void      __ahdecl(*ah_xrEnable)(struct ath_hal *);
    void      __ahdecl(*ah_xrDisable)(struct ath_hal *);

    /* Transmit functions */
    HAL_BOOL  __ahdecl(*ah_updateTxTrigLevel)(struct ath_hal*,
                HAL_BOOL incTrigLevel);
    u_int16_t __ahdecl(*ah_getTxTrigLevel)(struct ath_hal *);
    int   __ahdecl(*ah_setupTxQueue)(struct ath_hal *, HAL_TX_QUEUE,
                const HAL_TXQ_INFO *qInfo);
    HAL_BOOL  __ahdecl(*ah_setTxQueueProps)(struct ath_hal *, int q,
                const HAL_TXQ_INFO *qInfo);
    HAL_BOOL  __ahdecl(*ah_getTxQueueProps)(struct ath_hal *, int q,
                HAL_TXQ_INFO *qInfo);
    HAL_BOOL  __ahdecl(*ah_releaseTxQueue)(struct ath_hal *ah, u_int q);
    HAL_BOOL  __ahdecl(*ah_resetTxQueue)(struct ath_hal *ah, u_int q);
    u_int32_t __ahdecl(*ah_getTxDP)(struct ath_hal*, u_int);
    HAL_BOOL  __ahdecl(*ah_setTxDP)(struct ath_hal*, u_int, u_int32_t txdp);
    u_int32_t __ahdecl(*ah_numTxPending)(struct ath_hal *, u_int q);
    HAL_BOOL  __ahdecl(*ah_startTxDma)(struct ath_hal*, u_int);
    HAL_BOOL  __ahdecl(*ah_stopTxDma)(struct ath_hal*, u_int, u_int timeout);
    HAL_BOOL  __ahdecl(*ah_abortTxDma)(struct ath_hal *);
    HAL_BOOL  __ahdecl(*ah_fillTxDesc)(struct ath_hal *ah, void *ds, dma_addr_t *bufAddr,
                u_int32_t *segLen, u_int descId, u_int qcu, HAL_KEY_TYPE keyType, HAL_BOOL firstSeg,
                HAL_BOOL lastSeg, const void *ds0);
    void      __ahdecl(*ah_setDescLink)(struct ath_hal *, void *ds, u_int32_t link);
#ifdef _WIN64
    void      __ahdecl(*ah_getDescLinkPtr)(struct ath_hal *, void *ds, u_int32_t __unaligned **link);
#else
    void      __ahdecl(*ah_getDescLinkPtr)(struct ath_hal *, void *ds, u_int32_t **link);
#endif
    void       __ahdecl(*ah_clearTxDescStatus)(struct ath_hal *, void *);
#ifdef ATH_SWRETRY
    void       __ahdecl(*ah_clearDestMask)(struct ath_hal *, void *);
#endif
    HAL_BOOL   __ahdecl(*ah_fillKeyTxDesc)(struct ath_hal *, void *, HAL_KEY_TYPE);
    HAL_STATUS __ahdecl(*ah_procTxDesc)(struct ath_hal *, void *);
    void       __ahdecl(*ah_getRawTxDesc)(struct ath_hal *, u_int32_t *);
    void       __ahdecl(*ah_getTxRateCode)(struct ath_hal *, void*, struct ath_tx_status *);
    void       __ahdecl(*ah_getTxIntrQueue)(struct ath_hal *, u_int32_t *);
    HAL_BOOL   __ahdecl(*ah_setGlobalTxTimeout)(struct ath_hal*, u_int);
    u_int      __ahdecl(*ah_getGlobalTxTimeout)(struct ath_hal*);
    void       __ahdecl(*ah_reqTxIntrDesc)(struct ath_hal *, void*);
    u_int32_t  __ahdecl(*ah_calcTxAirtime)(struct ath_hal *, void*, struct ath_tx_status *, 
           HAL_BOOL comp_wastedt, u_int8_t nbad, u_int8_t nframes );
    void       __ahdecl(*ah_setupTxStatusRing)(struct ath_hal *, void *, u_int32_t , u_int16_t);


    /* Receive Functions */
    u_int32_t __ahdecl(*ah_getRxDP)(struct ath_hal*, HAL_RX_QUEUE);
    void      __ahdecl(*ah_setRxDP)(struct ath_hal*, u_int32_t rxdp, HAL_RX_QUEUE qtype);
    void      __ahdecl(*ah_enableReceive)(struct ath_hal*);
    HAL_BOOL  __ahdecl(*ah_stopDmaReceive)(struct ath_hal*, u_int timeout);
    void      __ahdecl(*ah_startPcuReceive)(struct ath_hal*, HAL_BOOL is_scanning);
    void      __ahdecl(*ah_stopPcuReceive)(struct ath_hal*);
    void      __ahdecl(*ah_abortPcuReceive)(struct ath_hal*);
    void      __ahdecl(*ah_setMulticastFilter)(struct ath_hal*,
                u_int32_t filter0, u_int32_t filter1);
    HAL_BOOL  __ahdecl(*ah_setMulticastFilterIndex)(struct ath_hal*,
                u_int32_t index);
    HAL_BOOL  __ahdecl(*ah_clrMulticastFilterIndex)(struct ath_hal*,
                u_int32_t index);
    u_int32_t __ahdecl(*ah_getRxFilter)(struct ath_hal*);
    void      __ahdecl(*ah_setRxFilter)(struct ath_hal*, u_int32_t);
    HAL_BOOL  __ahdecl(*ah_setRxSelEvm)(struct ath_hal*, HAL_BOOL, HAL_BOOL);
    HAL_BOOL  __ahdecl(*ah_setRxAbort)(struct ath_hal*, HAL_BOOL);
    HAL_BOOL  __ahdecl(*ah_setupRxDesc)(struct ath_hal *, struct ath_desc *,
                u_int32_t size, u_int flags);
    HAL_STATUS __ahdecl(*ah_procRxDesc)(struct ath_hal *, struct ath_desc *,
                u_int32_t phyAddr, struct ath_desc *next,
                u_int64_t tsf);
    HAL_STATUS __ahdecl(*ah_getRxKeyIdx)(struct ath_hal *, struct ath_desc *,
                u_int8_t *keyix, u_int8_t *status);
    HAL_STATUS __ahdecl(*ah_procRxDescFast)(struct ath_hal *ah,
                struct ath_desc *ds, u_int32_t pa, struct ath_desc *nds,
                struct ath_rx_status *rx_stats, void *buf_addr);
    void      __ahdecl(*ah_rxMonitor)(struct ath_hal *,
                const HAL_NODE_STATS *, HAL_CHANNEL *, HAL_ANISTATS *);
    void      __ahdecl(*ah_procMibEvent)(struct ath_hal *,
                const HAL_NODE_STATS *);

    /* Misc Functions */
    HAL_STATUS __ahdecl(*ah_getCapability)(struct ath_hal *,
                HAL_CAPABILITY_TYPE, u_int32_t capability,
                u_int32_t *result);
    HAL_BOOL   __ahdecl(*ah_setCapability)(struct ath_hal *,
                HAL_CAPABILITY_TYPE, u_int32_t capability,
                u_int32_t setting, HAL_STATUS *);
    HAL_BOOL   __ahdecl(*ah_getDiagState)(struct ath_hal *, int request,
                const void *args, u_int32_t argsize,
                void **result, u_int32_t *resultsize);
    void      __ahdecl(*ah_getMacAddress)(struct ath_hal *, u_int8_t *);
    HAL_BOOL  __ahdecl(*ah_setMacAddress)(struct ath_hal *, const u_int8_t*);
    void      __ahdecl(*ah_getBssIdMask)(struct ath_hal *, u_int8_t *);
    HAL_BOOL  __ahdecl(*ah_setBssIdMask)(struct ath_hal *, const u_int8_t*);
    HAL_BOOL  __ahdecl(*ah_setRegulatoryDomain)(struct ath_hal*,
                u_int16_t, HAL_STATUS *);
    void      __ahdecl(*ah_setLedState)(struct ath_hal*, HAL_LED_STATE);
    void      __ahdecl(*ah_setpowerledstate)(struct ath_hal *, u_int8_t);
    void      __ahdecl(*ah_setnetworkledstate)(struct ath_hal *, u_int8_t);
    void      __ahdecl(*ah_writeAssocid)(struct ath_hal*,
                const u_int8_t *bssid, u_int16_t assocId);
    void      __ahdecl(*ah_ForceTSFSync)(struct ath_hal*,
                const u_int8_t *bssid, u_int16_t assocId);
    HAL_BOOL  __ahdecl(*ah_gpioCfgInput)(struct ath_hal *, u_int32_t gpio);
    HAL_BOOL  __ahdecl(*ah_gpioCfgOutput)(struct ath_hal *,
                u_int32_t gpio, HAL_GPIO_OUTPUT_MUX_TYPE signalType);
    u_int32_t __ahdecl(*ah_gpioGet)(struct ath_hal *, u_int32_t gpio);
    HAL_BOOL  __ahdecl(*ah_gpioSet)(struct ath_hal *,
                u_int32_t gpio, u_int32_t val);
    void      __ahdecl(*ah_gpioSetIntr)(struct ath_hal*, u_int, u_int32_t);
    u_int32_t __ahdecl(*ah_getTsf32)(struct ath_hal*);
    u_int64_t __ahdecl(*ah_getTsf64)(struct ath_hal*);
    u_int32_t __ahdecl(*ah_getTsf2_32)(struct ath_hal*);
    u_int64_t __ahdecl(*ah_getTsf2_64)(struct ath_hal*);
    void      __ahdecl(*ah_resetTsf)(struct ath_hal*);
    HAL_BOOL  __ahdecl(*ah_detectCardPresent)(struct ath_hal*);
    void      __ahdecl(*ah_updateMibMacStats)(struct ath_hal*);
    void      __ahdecl(*ah_getMibMacStats)(struct ath_hal*, HAL_MIB_STATS*);
    HAL_RFGAIN __ahdecl(*ah_getRfGain)(struct ath_hal*);
    u_int     __ahdecl(*ah_getDefAntenna)(struct ath_hal*);
    void      __ahdecl(*ah_setDefAntenna)(struct ath_hal*, u_int);
    HAL_BOOL  __ahdecl(*ah_setSlotTime)(struct ath_hal*, u_int);
    u_int     __ahdecl(*ah_getSlotTime)(struct ath_hal*);
    HAL_BOOL  __ahdecl(*ah_setAckTimeout)(struct ath_hal*, u_int);
    u_int     __ahdecl(*ah_getAckTimeout)(struct ath_hal*);
    HAL_BOOL  __ahdecl(*ah_setCTSTimeout)(struct ath_hal*, u_int);
    u_int     __ahdecl(*ah_getCTSTimeout)(struct ath_hal*);
    HAL_BOOL  __ahdecl(*ah_setDecompMask)(struct ath_hal*, u_int16_t, int);
    void      __ahdecl(*ah_setCoverageClass)(struct ath_hal*, u_int8_t, int);
    HAL_STATUS __ahdecl(*ah_setQuiet)(struct ath_hal *,
                u_int16_t,u_int16_t,u_int16_t,u_int16_t);
    HAL_BOOL  __ahdecl(*ah_setAntennaSwitch)(struct ath_hal *,
                HAL_ANT_SETTING, HAL_CHANNEL *,
                u_int8_t *, u_int8_t *, u_int8_t *);
    void      __ahdecl(*ah_getDescInfo)(struct ath_hal*, HAL_DESC_INFO *);
    void      __ahdecl (*ah_markPhyInactive)(struct ath_hal *);
    HAL_STATUS  __ahdecl(*ah_selectAntConfig)(struct ath_hal *ah,
                u_int32_t cfg);
    int       __ahdecl(*ah_getNumAntCfg)(struct ath_hal *ah);
    HAL_BOOL  __ahdecl(*ah_setEifsMask)(struct ath_hal*, u_int);
    u_int     __ahdecl(*ah_getEifsMask)(struct ath_hal*);
    HAL_BOOL  __ahdecl(*ah_setEifsDur)(struct ath_hal*, u_int);
    u_int     __ahdecl(*ah_getEifsDur)(struct ath_hal*);
    u_int32_t  __ahdecl(*ah_ant_ctrl_common_get)(struct ath_hal *ah, HAL_BOOL is_2ghz);
    void      __ahdecl (*ah_EnableTPC)(struct ath_hal *);
    void      __ahdecl (*ah_olpcTempCompensation)(struct ath_hal *);
    void      __ahdecl (*ah_disablePhyRestart)(struct ath_hal *, int disable_phy_restart);
    void      __ahdecl (*ah_enable_keysearch_always)(struct ath_hal *, int enable);
    HAL_BOOL  __ahdecl(*ah_interferenceIsPresent)(struct ath_hal*);
    void      __ahdecl(*ah_DispTPCTables)(struct ath_hal *ah);
    /* Key Cache Functions */
    u_int32_t __ahdecl(*ah_getKeyCacheSize)(struct ath_hal*);
    HAL_BOOL  __ahdecl(*ah_resetKeyCacheEntry)(struct ath_hal*, u_int16_t);
    HAL_BOOL  __ahdecl(*ah_isKeyCacheEntryValid)(struct ath_hal *,
                u_int16_t);
    HAL_BOOL  __ahdecl(*ah_setKeyCacheEntry)(struct ath_hal*,
                u_int16_t, const HAL_KEYVAL *,
                const u_int8_t *, int);
    HAL_BOOL  __ahdecl(*ah_setKeyCacheEntryMac)(struct ath_hal*,
                u_int16_t, const u_int8_t *);
    HAL_BOOL  __ahdecl(*ah_PrintKeyCache)(struct ath_hal*);

    /* Power Management Functions */
    HAL_BOOL  __ahdecl(*ah_setPowerMode)(struct ath_hal*,
                HAL_POWER_MODE mode, int setChip);
    HAL_POWER_MODE __ahdecl(*ah_getPowerMode)(struct ath_hal*);
    void      __ahdecl(*ah_setSmPsMode)(struct ath_hal*, HAL_SMPS_MODE mode);
#if ATH_WOW        
    void      __ahdecl(*ah_wowApplyPattern)(struct ath_hal *ah,
                u_int8_t *pAthPattern, u_int8_t *pAthMask,
                int32_t pattern_count, u_int32_t athPatternLen);
    HAL_BOOL  __ahdecl(*ah_wowEnable)(struct ath_hal *ah,
                u_int32_t patternEnable, u_int32_t timoutInSeconds, int clearbssid);
    u_int32_t __ahdecl(*ah_wowWakeUp)(struct ath_hal *ah);
#endif        

    int16_t   __ahdecl(*ah_getChanNoise)(struct ath_hal *, HAL_CHANNEL *);
    void      __ahdecl(*ah_getChainNoiseFloor)(struct ath_hal *, int16_t *, HAL_CHANNEL *, int);

    /* Beacon Management Functions */
    void      __ahdecl(*ah_beaconInit)(struct ath_hal *,
                u_int32_t nexttbtt, u_int32_t intval, HAL_OPMODE opmode);
    void      __ahdecl(*ah_setStationBeaconTimers)(struct ath_hal*,
                const HAL_BEACON_STATE *);
    void      __ahdecl(*ah_resetStationBeaconTimers)(struct ath_hal*);
    HAL_BOOL  __ahdecl(*ah_waitForBeaconDone)(struct ath_hal *,
                HAL_BUS_ADDR);

    /* Interrupt functions */
    HAL_BOOL  __ahdecl(*ah_isInterruptPending)(struct ath_hal*);
    HAL_BOOL  __ahdecl(*ah_getPendingInterrupts)(struct ath_hal*, HAL_INT*, HAL_INT_TYPE, u_int8_t, HAL_BOOL);
    HAL_INT   __ahdecl(*ah_getInterrupts)(struct ath_hal*);
    HAL_INT   __ahdecl(*ah_setInterrupts)(struct ath_hal*, HAL_INT, HAL_BOOL);
    void      __ahdecl(*ah_SetIntrMitigationTimer)(struct ath_hal* ah,
                HAL_INT_MITIGATION reg, u_int32_t value);
    u_int32_t __ahdecl(*ah_GetIntrMitigationTimer)(struct ath_hal* ah,
                HAL_INT_MITIGATION reg);
    /*Force Virtual Carrier Sense*/
    HAL_BOOL  __ahdecl(*ah_forceVCS)(struct ath_hal*);
    HAL_BOOL  __ahdecl(*ah_setDfs3StreamFix)(struct ath_hal*, u_int32_t);
    HAL_BOOL  __ahdecl(*ah_get3StreamSignature)(struct ath_hal*);
    /* 11n Functions */
    void      __ahdecl(*ah_set11nTxDesc)(struct ath_hal *ah,
                void *ds, u_int pktLen, HAL_PKT_TYPE type,
                u_int txPower, u_int keyIx, HAL_KEY_TYPE keyType,
                u_int flags);
/* PAPRD Functions */
    void      __ahdecl(*ah_setPAPRDTxDesc)(struct ath_hal *ah,
                void *ds, int chainNum);
    int      __ahdecl(*ah_PAPRDInitTable)(struct ath_hal *ah, HAL_CHANNEL *chan);
    HAL_STATUS  __ahdecl(*ah_PAPRDSetupGainTable)(struct ath_hal *ah,
                int chainNum);
    HAL_STATUS  __ahdecl(*ah_PAPRDCreateCurve)(struct ath_hal *ah,
                HAL_CHANNEL *chan, int chainNum);
    int         __ahdecl(*ah_PAPRDisDone)(struct ath_hal *ah);
    void        __ahdecl(*ah_PAPRDEnable)(struct ath_hal *ah,
                HAL_BOOL enableFlag, HAL_CHANNEL *chan);
    void        __ahdecl(*ah_PAPRDPopulateTable)(struct ath_hal *ah,
                HAL_CHANNEL *chan, int chainNum);
    HAL_STATUS  __ahdecl(*ah_isTxDone)(struct ath_hal *ah);
    void        __ahdecl(*ah_paprd_dec_tx_pwr)(struct ath_hal *ah);
    int         __ahdecl(*ah_PAPRDThermalSend)(struct ath_hal *ah);
/* End PAPRD Functions */    
#ifdef ATH_SUPPORT_TxBF
    /* TxBF functions */
    void      __ahdecl(*ah_set11nTxBFSounding)(struct ath_hal *ah, 
				void *ds,HAL_11N_RATE_SERIES series[],u_int8_t CEC,u_int8_t opt);
#ifdef TXBF_TODO
    void      __ahdecl(*ah_set11nTxBFCal)(struct ath_hal *ah, void *ds, 
                u_int8_t CalPos, u_int8_t codeRate, u_int8_t CEC, u_int8_t opt);
	HAL_BOOL  __ahdecl(*ah_TxBFSaveCVFromCompress)(struct ath_hal *ah, u_int8_t keyidx,
				u_int16_t mimocontrol, u_int8_t *CompressRpt);
	HAL_BOOL  __ahdecl(*ah_TxBFSaveCVFromNonCompress)(struct ath_hal *ah, u_int8_t keyidx,
				u_int16_t mimocontrol, u_int8_t *NonCompressRpt);
	HAL_BOOL  __ahdecl(*ah_TxBFRCUpdate)(struct ath_hal *ah, struct ath_rx_status *rx_status,
                u_int8_t *local_h, u_int8_t *CSIFrame, u_int8_t NESSA, u_int8_t NESSB, int BW);
    int       __ahdecl(*ah_FillCSIFrame)(struct ath_hal *ah,struct ath_rx_status *rx_status,
                u_int8_t BW, u_int8_t *local_h, u_int8_t *CSIFrameBody);
#endif
    HAL_TXBF_CAPS*  __ahdecl(*ah_getTxBFCapability)(struct ath_hal *ah);
    HAL_BOOL  __ahdecl(*ah_readKeyCacheMac)(struct ath_hal *ah, u_int16_t entry,
                u_int8_t *mac);
    void      __ahdecl(*ah_TxBFSetKey)(struct ath_hal *ah, u_int16_t entry, u_int8_t rx_staggered_sounding,
                u_int8_t channel_estimation_cap, u_int8_t MMSS);
    void      __ahdecl((*ah_setHwCvTimeout) (struct ath_hal *ah,HAL_BOOL opt));
    void      __ahdecl(*ah_FillTxBFCap)(struct ath_hal *ah);
    void      __ahdecl(*ah_reset_lowest_txrate)(struct ath_hal *ah);
    void     __ahdecl(*ah_getPerRateTxBFFlag)(struct ath_hal *ah, u_int8_t txDisableFlag[][AH_MAX_CHAINS]);
#endif
    void      __ahdecl(*ah_set11nRateScenario)(struct ath_hal *ah,
                void *ds, void *lastds, u_int durUpdateEn, u_int rtsctsRate, u_int rtsctsDuration,
                HAL_11N_RATE_SERIES series[], u_int nseries, u_int flags, u_int32_t smartAntenna);
    void      __ahdecl(*ah_set11nAggrFirst)(struct ath_hal *ah,
                void *ds, u_int aggrLen);
    void      __ahdecl(*ah_set11nAggrMiddle)(struct ath_hal *ah,
                void *ds, u_int numDelims);
    void      __ahdecl(*ah_set11nAggrLast)(struct ath_hal *ah,
                void *ds);
    void      __ahdecl(*ah_clr11nAggr)(struct ath_hal *ah,
                void *ds);
    void      __ahdecl(*ah_set11nRifsBurstMiddle)(struct ath_hal *ah,
                void *ds);
    void      __ahdecl(*ah_set11nRifsBurstLast)(struct ath_hal *ah,
                void *ds);
    void      __ahdecl(*ah_clr11nRifsBurst)(struct ath_hal *ah,
                void *ds);
    void      __ahdecl(*ah_set11nAggrRifsBurst)(struct ath_hal *ah,
                void *ds);
    HAL_BOOL  __ahdecl(*ah_set11nRxRifs)(struct ath_hal *ah, HAL_BOOL enable);
    HAL_BOOL  __ahdecl(*ah_get11nRxRifs)(struct ath_hal *ah);
    HAL_BOOL  __ahdecl(*ah_setSmartAntenna)(struct ath_hal *ah, HAL_BOOL enable);
    HAL_BOOL  __ahdecl(*ah_detectBbHang)(struct ath_hal *ah);
    HAL_BOOL  __ahdecl(*ah_detectMacHang)(struct ath_hal *ah);
    void      __ahdecl(*ah_immunity)(struct ath_hal *ah, HAL_BOOL enable);
    void      __ahdecl(*ah_getHangTypes)(struct ath_hal *, hal_hw_hangs_t *);
    void      __ahdecl(*ah_set11nBurstDuration)(struct ath_hal *ah,
                void *ds, u_int burstDuration);
    void      __ahdecl(*ah_set11nVirtualMoreFrag)(struct ath_hal *ah,
                void *ds, u_int vmf);
    int8_t __ahdecl(*ah_get11nExtBusy)(struct ath_hal *ah);
    void      __ahdecl(*ah_set11nMac2040)(struct ath_hal *ah, HAL_HT_MACMODE);
    HAL_HT_RXCLEAR __ahdecl(*ah_get11nRxClear)(struct ath_hal *ah);
    void      __ahdecl(*ah_set11nRxClear)(struct ath_hal *ah, HAL_HT_RXCLEAR rxclear);
    int       __ahdecl(*ah_get11nHwPlatform)(struct ath_hal *ah);
    int      __ahdecl(*ah_update_navReg)(struct ath_hal *ah);
    u_int32_t __ahdecl(*ah_getMibCycleCountsPct)(struct ath_hal *ah, u_int32_t*, u_int32_t*, u_int32_t*);
    void      __ahdecl(*ah_dmaRegDump)(struct ath_hal *);
    u_int32_t __ahdecl(*ah_ppmGetRssiDump)(struct ath_hal *);
    u_int32_t __ahdecl(*ah_ppmArmTrigger)(struct ath_hal *);
    int       __ahdecl(*ah_ppmGetTrigger)(struct ath_hal *);
    u_int32_t __ahdecl(*ah_ppmForce)(struct ath_hal *);
    void      __ahdecl(*ah_ppmUnForce)(struct ath_hal *);
    u_int32_t __ahdecl(*ah_ppmGetForceState)(struct ath_hal *);
    int       __ahdecl(*ah_getSpurInfo)(struct ath_hal *, int *, int, u_int16_t *);
    int       __ahdecl(*ah_setSpurInfo)(struct ath_hal *, int, int, u_int16_t *);

    /* Noise floor functions */
    int16_t   __ahdecl(*ah_arGetNoiseFloorVal)(struct ath_hal *ah);
    void      __ahdecl(*ah_setNfNominal)(struct ath_hal *, int16_t, HAL_BOOL);
    int16_t   __ahdecl(*ah_getNfNominal)(struct ath_hal *, HAL_BOOL);
    void      __ahdecl(*ah_setNfMin)(struct ath_hal *, int16_t, HAL_BOOL);
    int16_t   __ahdecl(*ah_getNfMin)(struct ath_hal *, HAL_BOOL);
    void      __ahdecl(*ah_setNfMax)(struct ath_hal *, int16_t, HAL_BOOL);
    int16_t   __ahdecl(*ah_getNfMax)(struct ath_hal *, HAL_BOOL);
    void      __ahdecl(*ah_setNfCwIntDelta)(struct ath_hal *ah, int16_t);
    int16_t   __ahdecl(*ah_getNfCwIntDelta)(struct ath_hal *ah);

    void      __ahdecl(*ah_setRxGreenApPsOnOff)(struct ath_hal *ah,
                u_int16_t rxMask);
    u_int16_t __ahdecl(*ah_isSingleAntPowerSavePossible)(struct ath_hal *ah);

    void      __ahdecl(*ah_get_vow_stats)(struct ath_hal *ah, HAL_VOWSTATS* p_stats);
#ifdef ATH_CCX
    /* CCX Radio Measurement Specific Functions */
    void      __ahdecl(*ah_getMibCycleCounts)(struct ath_hal *ah, HAL_COUNTERS* pCnts);
    void      __ahdecl(*ah_clearMibCounters)(struct ath_hal *);
    u_int8_t  __ahdecl(*ah_getCcaThreshold)(struct ath_hal *);
    u_int32_t __ahdecl(*ah_getCurRSSI)(struct ath_hal *);
#endif
    
#ifdef ATH_BT_COEX
    /* Bluetooth Coexistence Functions */
    void      __ahdecl(*ah_setBTCoexInfo)(struct ath_hal *, HAL_BT_COEX_INFO *);
    void      __ahdecl(*ah_btCoexConfig)(struct ath_hal *, HAL_BT_COEX_CONFIG *);
    void      __ahdecl(*ah_btCoexSetQcuThresh)(struct ath_hal *, int);
    void      __ahdecl(*ah_btCoexSetWeights)(struct ath_hal *, u_int32_t);
    void      __ahdecl(*ah_btCoexSetBmissThresh)(struct ath_hal *, u_int32_t);
    void      __ahdecl(*ah_btCoexSetParameter)(struct ath_hal *, u_int32_t, u_int32_t);
    void      __ahdecl(*ah_btCoexDisable)(struct ath_hal *);
    int       __ahdecl(*ah_btCoexEnable)(struct ath_hal *);
#endif
    /* Generic Timer Functions */
    int       __ahdecl(*ah_gentimerAlloc)(struct ath_hal *, HAL_GEN_TIMER_DOMAIN);
    void      __ahdecl(*ah_gentimerFree)(struct ath_hal *, int);
    void      __ahdecl(*ah_gentimerStart)(struct ath_hal *, int, u_int32_t, u_int32_t);
    void      __ahdecl(*ah_gentimerStop)(struct ath_hal *, int);
    void      __ahdecl(*ah_gentimerGetIntr)(struct ath_hal *, u_int32_t *, u_int32_t *);

    /* DCS - dynamic (transmit) chainmask selection */
    void      __ahdecl(*ah_setDcsMode)(struct ath_hal *, u_int32_t);
    u_int32_t __ahdecl(*ah_getDcsMode)(struct ath_hal *);

#ifdef ATH_ANT_DIV_COMB
    void     __ahdecl(*ah_getAntDviCombConf)(struct ath_hal *, HAL_ANT_COMB_CONFIG *);
    void     __ahdecl(*ah_setAntDviCombConf)(struct ath_hal *, HAL_ANT_COMB_CONFIG *);
#endif

    /* Baseband panic functions */
    int        __ahdecl(*ah_getBbPanicInfo)(struct ath_hal *ah, struct hal_bb_panic_info *bb_panic);
    HAL_BOOL   __ahdecl(*ah_handle_radar_bb_panic)(struct ath_hal *ah);
    void       __ahdecl(*ah_setHalResetReason)(struct ath_hal *ah, u_int8_t);

#if ATH_SUPPORT_SPECTRAL
    /* Spectral Scan functions */
    void     __ahdecl(*ah_arConfigFftPeriod)(struct ath_hal *ah, u_int32_t inVal);
    void     __ahdecl(*ah_arConfigScanPeriod)(struct ath_hal *ah, u_int32_t inVal);
    void     __ahdecl(*ah_arConfigScanCount)(struct ath_hal *ah, u_int32_t inVal);
    void     __ahdecl(*ah_arConfigShortRpt)(struct ath_hal *ah, u_int32_t inVal);
    void     __ahdecl(*ah_arConfigureSpectral)(struct ath_hal *ah, HAL_SPECTRAL_PARAM *sp);
    void     __ahdecl(*ah_arGetSpectralConfig)(struct ath_hal *ah, HAL_SPECTRAL_PARAM *sp);
    void     __ahdecl(*ah_arStartSpectralScan)(struct ath_hal *);
    void     __ahdecl(*ah_arStopSpectralScan)(struct ath_hal *);
    HAL_BOOL __ahdecl(*ah_arIsSpectralEnabled)(struct ath_hal *);
    HAL_BOOL __ahdecl(*ah_arIsSpectralActive)(struct ath_hal *);
    u_int32_t __ahdecl(*ah_arIsSpectralScanCapable)(struct ath_hal *ah);
    int16_t   __ahdecl(*ah_arGetCtlNF)(struct ath_hal *ah);
    int16_t   __ahdecl(*ah_arGetExtNF)(struct ath_hal *ah);
#endif  /* ATH_SUPPORT_SPECTRAL */

#if ATH_SUPPORT_RAW_ADC_CAPTURE
    void       __ahdecl(*ah_arEnableTestAddacMode)(struct ath_hal *ah);
    void       __ahdecl(*ah_arDisableTestAddacMode)(struct ath_hal *ah);
    void       __ahdecl(*ah_arBeginAdcCapture)(struct ath_hal *ah, int auto_agc_gain);
    HAL_STATUS __ahdecl(*ah_arRetrieveCaptureData)(struct ath_hal *ah, u_int16_t chain_mask, int disable_dc_filter, void *data_buf, u_int32_t *data_buf_len);
	HAL_STATUS __ahdecl(*ah_arCalculateADCRefPowers)(struct ath_hal *ah, int freqMhz, int16_t *sample_min, int16_t *sample_max, int32_t *chain_ref_pwr, int num_chain_ref_pwr);
	HAL_STATUS __ahdecl(*ah_arGetMinAGCGain)(struct ath_hal *ah, int freqMhz, int32_t *chain_gain, int num_chain_gain);
#endif

    void       __ahdecl(*ah_promiscMode )(struct ath_hal *ah, HAL_BOOL);
    void       __ahdecl(*ah_ReadPktlogReg)(struct ath_hal *ah, u_int32_t *, u_int32_t *, u_int32_t *, u_int32_t *);
    void       __ahdecl(*ah_WritePktlogReg)(struct ath_hal *ah, HAL_BOOL, u_int32_t, u_int32_t, u_int32_t, u_int32_t );
    HAL_STATUS    __ahdecl(*ah_setProxySTA)(struct ath_hal *ah, HAL_BOOL);
    int      __ahdecl(*ah_getCalIntervals)(struct ath_hal *, HAL_CALIBRATION_TIMER **, HAL_CAL_QUERY);
#if ATH_SUPPORT_WIRESHARK
    void     __ahdecl(*ah_fillRadiotapHdr)(struct ath_hal *,
               struct ah_rx_radiotap_header *, struct ah_ppi_data *, struct ath_desc *, void *);
#endif
#if ATH_TRAFFIC_FAST_RECOVER
    unsigned long __ahdecl(*ah_getPll3SqsumDvc)(struct ath_hal *ah);
#endif

#ifdef ATH_SUPPORT_HTC
    u_int32_t   *hal_wmi_handle;
    HAL_BOOL    ah_htc_isResetInit;
    void      __ahdecl(*ah_htcResetInit)(struct ath_hal *);
#endif

#ifdef ATH_TX99_DIAG
    /* Tx99 functions */
    void       __ahdecl(*ah_tx99txstopdma)(struct ath_hal *, u_int32_t);
    void       __ahdecl(*ah_tx99drainalltxq)(struct ath_hal *);
    void       __ahdecl(*ah_tx99channelpwrupdate)(struct ath_hal *, HAL_CHANNEL *, u_int32_t);
    void       __ahdecl(*ah_tx99start)(struct ath_hal *, u_int8_t *);
    void       __ahdecl(*ah_tx99stop)(struct ath_hal *);
    void       __ahdecl(*ah_tx99_chainmsk_setup)(struct ath_hal *, int);
    void       __ahdecl(*ah_tx99_set_single_carrier)(struct ath_hal *, int, int);
#endif

    HAL_RSSI_TX_POWER   green_tx_status;   /* The status of Green TX function */
    u_int32_t  txPowerRecord[3][9];
    u_int32_t  PALRecord;
    u_int32_t  OBDB1[3];
    u_int32_t  DB2[3];
    void      __ahdecl(*ah_ChkRssiUpdateTxPwr)(struct ath_hal *, int rssi);
    HAL_BOOL    __ahdecl(*ah_is_skip_paprd_by_greentx)(struct ath_hal *);
    void      __ahdecl(*ah_hwgreentx_set_pal_spare)(struct ath_hal *ah, int value);
    HAL_BOOL  __ahdecl(*ah_is_ani_noise_spur)(struct ath_hal *ah);
    u_int32_t   hal_reg_write_buffer_flag;  /* switch flag for reg_write(original) and reg_write_delay(usb multiple) */

#ifdef ATH_SUPPORT_WAPI
    u_int8_t    ah_hal_keytype;        /* one of HAL_CIPHER */
#endif

#if ICHAN_WAR_SYNCH
    spinlock_t  ah_ichan_lock;
    HAL_BOOL    ah_ichan_set;
#endif
#ifdef ATH_SUPPORT_TxBF
    HAL_BOOL    ah_txbf_hw_cvtimeout;
#endif
    u_int32_t   ah_fccaifs;  /* fcc aifs is set  */
    u_int32_t   ah_use_cac_prssi;  /* fcc aifs is set  */
#ifdef AH_ANALOG_SHADOW_READ
    u_int32_t rfshadow[(RF_END-RF_BEGIN)/4+1];
#endif
};

/* 
 * Check the PCI vendor ID and device ID against Atheros' values
 * and return a printable description for any Atheros hardware.
 * AH_NULL is returned if the ID's do not describe Atheros hardware.
 */
extern  const char *__ahdecl ath_hal_probe(u_int16_t vendorid, u_int16_t devid);

/*
 * Attach the HAL for use with the specified device.  The device is
 * defined by the PCI device ID.  The caller provides an opaque pointer
 * to an upper-layer data structure (HAL_SOFTC) that is stored in the
 * HAL state block for later use.  Hardware register accesses are done
 * using the specified bus tag and handle.  On successful return a
 * reference to a state block is returned that must be supplied in all
 * subsequent HAL calls.  Storage associated with this reference is
 * dynamically allocated and must be freed by calling the ah_detach
 * method when the client is done.  If the attach operation fails a
 * null (AH_NULL) reference will be returned and a status code will
 * be returned if the status parameter is non-zero.
 */
extern  struct ath_hal * __ahdecl ath_hal_attach(u_int16_t devid, HAL_ADAPTER_HANDLE osdev, 
        HAL_SOFTC, HAL_BUS_TAG, HAL_BUS_HANDLE, HAL_BUS_TYPE,
        asf_amem_instance_handle amem_handle,
        struct hal_reg_parm *hal_conf_parm, HAL_STATUS* status);

 /*
  * Set the Vendor ID for Vendor SKU's which can modify the
  * channel properties returned by ath_hal_init_channels.
  * Return AH_TRUE if set succeeds
  */
 
extern  HAL_BOOL __ahdecl ath_hal_setvendor(struct ath_hal *, u_int32_t );

/*
 * Return a list of channels available for use with the hardware.
 * The list is based on what the hardware is capable of, the specified
 * country code, the modeSelect mask, and whether or not outdoor
 * channels are to be permitted.
 *
 * The channel list is returned in the supplied array.  maxchans
 * defines the maximum size of this array.  nchans contains the actual
 * number of channels returned.  If a problem occurred or there were
 * no channels that met the criteria then AH_FALSE is returned.
 */
extern  HAL_BOOL __ahdecl ath_hal_init_channels(struct ath_hal *,
        HAL_CHANNEL *chans, u_int maxchans, u_int *nchans,
        u_int8_t *regclassids, u_int maxregids, u_int *nregids,
        HAL_CTRY_CODE cc, u_int32_t modeSelect,
        HAL_BOOL enableOutdoor, HAL_BOOL enableExtendedChannels);

/*
 * Return bit mask of wireless modes supported by the hardware.
 */
extern  u_int __ahdecl ath_hal_getwirelessmodes(struct ath_hal*, HAL_CTRY_CODE);

/*
 * Return rate table for specified mode (11a, 11b, 11g, etc).
 */
extern  const HAL_RATE_TABLE * __ahdecl ath_hal_getratetable(struct ath_hal *,
        u_int mode);

/*
 * Calculate the transmit duration of a frame.
 */
extern u_int16_t __ahdecl ath_hal_computetxtime(struct ath_hal *,
        const HAL_RATE_TABLE *rates, u_int32_t frameLen,
        u_int16_t rateix, HAL_BOOL shortPreamble);

/*
 * Return if device is public safety.
 */
extern HAL_BOOL __ahdecl ath_hal_ispublicsafetysku(struct ath_hal *);

/*
 * Convert between IEEE channel number and channel frequency
 * using the specified channel flags; e.g. CHANNEL_2GHZ.
 */
extern  u_int __ahdecl ath_hal_mhz2ieee(struct ath_hal *, u_int mhz, u_int flags);

/*
 * Find the HAL country code from its ISO name.
 */
extern HAL_CTRY_CODE __ahdecl findCountryCode(u_int8_t *countryString);
extern u_int8_t  __ahdecl findCTLByCountryCode(HAL_CTRY_CODE countrycode, HAL_BOOL is2G);

/*
 * Find the HAL country code from its domain enum.
 */
extern HAL_CTRY_CODE __ahdecl findCountryCodeByRegDomain(HAL_REG_DOMAIN regdmn);

/*
 * Return the current regulatory domain information
 */
extern void __ahdecl ath_hal_getCurrentCountry(void *ah, HAL_COUNTRY_ENTRY* ctry);

/*
 * Return the 802.11D supported country table
 */
extern u_int __ahdecl ath_hal_get11DCountryTable(void *ah, HAL_COUNTRY_ENTRY* ctries, u_int nCtryBufCnt);

extern void __ahdecl ath_hal_set_singleWriteKC(struct ath_hal *ah, u_int8_t singleWriteKC);

extern HAL_BOOL __ahdecl ath_hal_enabledANI(struct ath_hal *ah);

/* 
 * Device info function 
 */
u_int32_t ath_hal_get_device_info(struct ath_hal *ah,HAL_DEVICE_INFO type);

/*
** Prototypes for the HAL layer configuration get and set functions
*/

u_int32_t ath_hal_set_config_param(struct ath_hal *ah,
                                   HAL_CONFIG_OPS_PARAMS_TYPE p,
                                   void *b);

u_int32_t ath_hal_get_config_param(struct ath_hal *ah,
                                   HAL_CONFIG_OPS_PARAMS_TYPE p,
                                   void *b);
/*
 * Display the TPC tables for the current channel and country
 */
void ath_hal_display_tpctables(struct ath_hal *ah);

/*
 * Return a version string for the HAL release.
 */
extern  char ath_hal_version[];
/*
 * Return a NULL-terminated array of build/configuration options.
 */
extern  const char* ath_hal_buildopts[];
#ifdef ATH_CCX
extern int ath_hal_get_sernum(struct ath_hal *ah, u_int8_t *pSn, int limit);
extern HAL_BOOL ath_hal_get_chandata(struct ath_hal * ah, HAL_CHANNEL * chan, HAL_CHANNEL_DATA* pData);
#endif

#ifdef DBG
extern u_int32_t ath_hal_readRegister(struct ath_hal *ah, u_int32_t offset);
extern void ath_hal_writeRegister(struct ath_hal *ah, u_int32_t offset, u_int32_t value);
#endif

/*
 * Return common mode power in 5Ghz
 */
extern u_int8_t __ahdecl getCommonPower(u_int16_t freq);

/* define used in TxBF rate control */
//before osprey 2.2 , it has a bug that will cause sounding invalid problem at three stream
// (sounding bug when NESS=0), so it should swap to  2 stream rate at three stream.
#define AR_SREV_VER_OSPREY 0x1C0
#define AR_SREV_REV_OSPREY_22            3      

#define ath_hal_support_ts_sounding(_ah) \
    (((_ah)->ah_macVersion > AR_SREV_VER_OSPREY) ||   \
     (((_ah)->ah_macVersion == AR_SREV_VER_OSPREY) && \
      ((_ah)->ah_macRev >= AR_SREV_REV_OSPREY_22)))

/* Tx hang check to be done only for osprey and later series */
#define ath_hal_support_tx_hang_check(_ah) \
    ((_ah)->ah_macVersion >= AR_SREV_VER_OSPREY)

extern void __ahdecl ath_hal_phyrestart_clr_war_enable(struct ath_hal *ah, int war_enable);
extern void __ahdecl ath_hal_keysearch_always_war_enable(struct ath_hal *ah, int war_enable);
#endif /* _ATH_AH_H_ */
