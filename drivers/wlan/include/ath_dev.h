/*
 * Copyright (c) 2005, Atheros Communications Inc.
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

#ifndef ATH_DEV_H
#define ATH_DEV_H

/*
 * Public Interface of ATH layer
 */

#include <osdep.h>
#include <ah.h>
#include <wbuf.h>
#include <if_athioctl.h>

#include <ieee80211.h>
#include <umac_lmac_common.h>
#include "ath_opts.h"
#ifdef ATH_SUPPORT_HTC
#include "htc_host_struct.h"
#endif
#ifdef ATH_BT_COEX
#include "ath_bt_registry.h"
#endif

/**
 * @defgroup ath_dev ath_dev - Atheros Device Module
 * @{
 * ath_dev implements the Atheros' specific low-level functions of the wireless driver.
 */

/**
 * @brief Clients of ATH layer call ath_attach to obtain a reference to an ath_dev structure.
 * Hardware-related operation that follow must call back into ATH through interface,
 * supplying the reference as the first parameter.
 */
typedef void *ath_dev_t;

#include "ath_hwtimer.h"

/**
 * @brief Opaque handle of an associated node (including BSS itself).
 */
typedef void *ath_node_t;

/**
 * @defgroup ieee80211_if - 802.11 Protocal Interface required by Atheros Device Module
 * @{
 * @ingroup ath_dev
 */

/**
 * @brief Opaque handle of 802.11 protocal layer.
 * ATH module must call back into protocal layer through callbacks passed in attach time,
 * supplying the reference as the first parameter.
 */
typedef void *ieee80211_handle_t;

/**
 * @brief Opaque handle of network instance in 802.11 protocal layer.
 */
typedef void *ieee80211_if_t;

/**
 * @brief Opaque handle of per-destination information in 802.11 protocal layer.
 */
typedef void *ieee80211_node_t;

/* 
 * Number of (OEM-defined) functions using GPIO pins currently defined 
 *
 * Function 0: Link/Power LED
 * Function 1: Network/Activity LED
 * Function 2: Connection LED
 */
#define NUM_GPIO_FUNCS             3

/**
 * @brief Wireless Mode Definition
 */
typedef enum {
    WIRELESS_MODE_11a = 0,
    WIRELESS_MODE_11b,
    WIRELESS_MODE_11g,
    WIRELESS_MODE_108a,
    WIRELESS_MODE_108g,
    WIRELESS_MODE_11NA_HT20,
    WIRELESS_MODE_11NG_HT20,
    WIRELESS_MODE_11NA_HT40PLUS,
    WIRELESS_MODE_11NA_HT40MINUS,
    WIRELESS_MODE_11NG_HT40PLUS,
    WIRELESS_MODE_11NG_HT40MINUS,
    WIRELESS_MODE_XR,
    WIRELESS_MODE_MAX
} WIRELESS_MODE;

/*
 * @breif Wireless Mode Mask
 */
typedef enum {
    MODE_SELECT_11A       = 0x00001,
    MODE_SELECT_TURBO     = 0x00002,
    MODE_SELECT_11B       = 0x00004,
    MODE_SELECT_11G       = 0x00008,
    MODE_SELECT_108G      = 0x00020,
    MODE_SELECT_108A      = 0x00040,
    MODE_SELECT_XR        = 0x00100,
    MODE_SELECT_49_27     = 0x00200,
    MODE_SELECT_49_54     = 0x00400,
    MODE_SELECT_11NG_HT20 = 0x00800,
    MODE_SELECT_11NA_HT20 = 0x01000,
    MODE_SELECT_11NG_HT40PLUS  = 0x02000,       /* 11N-G HT40+ channels */
    MODE_SELECT_11NG_HT40MINUS = 0x04000,       /* 11N-G HT40- channels */
    MODE_SELECT_11NA_HT40PLUS  = 0x08000,       /* 11N-A HT40+ channels */
    MODE_SELECT_11NA_HT40MINUS = 0x10000,       /* 11N-A HT40- channels */

    MODE_SELECT_2GHZ      = (MODE_SELECT_11B | MODE_SELECT_11G | MODE_SELECT_108G | MODE_SELECT_11NG_HT20),

    MODE_SELECT_5GHZ      = (MODE_SELECT_11A | MODE_SELECT_TURBO | MODE_SELECT_108A | MODE_SELECT_11NA_HT20),

    MODE_SELECT_ALL       = (MODE_SELECT_5GHZ | MODE_SELECT_2GHZ | MODE_SELECT_49_27 | MODE_SELECT_49_54 | MODE_SELECT_11NG_HT40PLUS | MODE_SELECT_11NG_HT40MINUS | MODE_SELECT_11NA_HT40PLUS | MODE_SELECT_11NA_HT40MINUS),

} WIRELESS_MODES_SELECT;

/*
 * Spatial Multiplexing Modes.
 */
typedef enum {
    ATH_SM_ENABLE,
    ATH_SM_PWRSAV_STATIC,
    ATH_SM_PWRSAV_DYNAMIC,
} ATH_SM_PWRSAV;

/*
 * Rate
 */
typedef enum {
    NORMAL_RATE = 0,
    HALF_RATE,
    QUARTER_RATE
} RATE_TYPE;

/**
 * @brief Protection Moide
 */
typedef enum {
    PROT_M_NONE = 0,
    PROT_M_RTSCTS,
    PROT_M_CTSONLY
} PROT_MODE;

/**
 * @brief beacon configuration
 */
typedef struct {
    u_int16_t       beacon_interval;
    u_int16_t       listen_interval;
    u_int16_t       dtim_period;
    u_int16_t       bmiss_timeout;
    u_int8_t        dtim_count;
    u_int8_t        tim_offset;
    union {
        u_int64_t   last_tsf;
        u_int8_t    last_tstamp[8];
    } u;    /* last received beacon/probe response timestamp of this BSS. */
} ieee80211_beacon_config_t;

/**
 * @brief offsets in a beacon frame for
 * quick acess of beacon content by low-level driver
 */
typedef struct {
    u_int8_t        *bo_tim;    /* start of atim/dtim */
} ieee80211_beacon_offset_t;

/**
 * @brief per-frame tx control block
 */
typedef struct {
    ath_node_t      an;         /* destination to sent to */
    int             if_id;      /* only valid for cab traffic */
    int             qnum;       /* h/w queue number */

    u_int           ismcast:1;  /* if it's a multicast frame */
    u_int           istxfrag:1; /* if it's a tx fragment */
    u_int           ismgmt:1;   /* if it's a management frame */
    u_int           isdata:1;   /* if it's a data frame */
    u_int           isqosdata:1; /* if it's a qos data frame */
    u_int           ps:1;       /* if one or more stations are in PS mode */
    u_int           shortPreamble:1; /* use short preamble */
    u_int           ht:1;       /* if it can be transmitted using HT */
    u_int           use_minrate:1; /* if this frame should transmitted using specified
                                    * mininum rate */
    u_int           isbar:1;    /* if it is a block ack request */
    u_int           ispspoll:1; /* if it is a PS-Poll frame */
    u_int           calcairtime:1; /* requests airtime be calculated when set for tx frame */
    u_int           iseap:1; /* Is this an EAP packet? */
#ifdef ATH_SUPPORT_UAPSD
    u_int           isuapsd:1;  /* if this frame needs uapsd handling */
#endif
#ifdef ATH_SUPPORT_TxBF
    u_int           isdelayrpt:1;
#endif

    HAL_PKT_TYPE    atype;      /* Atheros packet type */
    u_int32_t       flags;      /* HAL flags */
    u_int32_t       keyix;      /* key index */
    HAL_KEY_TYPE    keytype;    /* key type */
    u_int16_t       txpower;    /* transmit power */
    u_int16_t       seqno;      /* sequence number */
    u_int16_t       tidno;      /* tid number */

    u_int16_t       frmlen;     /* frame length */
#ifdef USE_LEGACY_HAL
    u_int16_t       hdrlen;     /* header length of this frame */
    int             compression; /* compression scheme */
    u_int8_t        ivlen;      /* iv length for compression */
    u_int8_t        icvlen;     /* icv length for compression */
    u_int8_t        antenna;    /* antenna control */
#endif

    int             min_rate;   /* minimum rate */
    int             mcast_rate; /* multicast rate */
    u_int16_t       nextfraglen; /* next fragment length */

    /* below is set only by ath_dev */
    ath_dev_t       dev;        /* device handle */
    u_int8_t        priv[64];   /* private rate control info */

    OS_DMA_MEM_CONTEXT(dmacontext) /* OS specific DMA context */
#ifdef ATH_SUPPORT_HTC
    u_int8_t nodeindex;
    u_int8_t vapindex;
#ifdef ENCAP_OFFLOAD    
    u_int8_t keyid;
    u_int8_t key_mapping_key;
#endif
#endif
} ieee80211_tx_control_t;

/*
 * @brief per frame tx status block
 */
typedef struct {
    int             retries;    /* number of retries to successufully transmit this frame */
    int             flags;      /* status of transmit */
#define ATH_TX_ERROR        0x01
#define ATH_TX_XRETRY       0x02

#ifdef  ATH_SUPPORT_TxBF
    int             txbf_status;    /* status of TxBF */
#define	ATH_TxBF_BW_mismatched            0x1
#define	ATH_TXBF_stream_missed            0x2
#define	ATH_TXBF_CV_missed                0x4
#define	ATH_TXBF_Destination_missed       0x8

    u_int32_t       tstamp;         /* tx time stamp*/
#endif
} ieee80211_tx_status_t;

/*
 * @brief txbf capability block
 */
#ifdef ATH_SUPPORT_TxBF
typedef struct {
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
} ieee80211_txbf_caps_t;
#endif

#define ATH_MAX_ANTENNA 3
#define ATH_MAX_SM_RATESETS 3

/*
 * @brief headline block removal 
 */
/*
 * HBR_TRIGGER_PER_HIGH is the per threshold to
 * trigger the headline block removal state machine
 * into PROBING state; HBR_TRIGGER_PER_LOW is the 
 * per threshold to trigger the state machine to
 * ACTIVE mode from PROBING. Since log(5/25)/log(7/8)=12,
 * which means 12 continuous QoSNull probing frames
 * been sent successfully.
 */
#define HBR_TRIGGER_PER_HIGH    25
#define HBR_TRIGGER_PER_LOW        5


/*
 * @brief per frame rx status block
 */
typedef struct {
    u_int64_t       tsf;        /* mac tsf */
    int8_t          rssi;       /* RSSI (noise floor ajusted) */
    int8_t          rssictl[ATH_MAX_ANTENNA];       /* RSSI (noise floor ajusted) */
    int8_t          rssiextn[ATH_MAX_ANTENNA];       /* RSSI (noise floor ajusted) */
    int8_t          abs_rssi;   /* absolute RSSI */
    u_int8_t        rateieee;   /* data rate received (IEEE rate code) */
    u_int8_t        ratecode;   /* phy rate code */
    u_int8_t        nomoreaggr; /* single frame or last of aggregation */
    int             rateKbps;   /* data rate received (Kbps) */
    int             flags;      /* status of associated wbuf */
#define ATH_RX_FCS_ERROR        0x01
#define ATH_RX_MIC_ERROR        0x02
#define ATH_RX_DECRYPT_ERROR    0x04
#define ATH_RX_RSSI_VALID       0x08
#define ATH_RX_CHAIN_RSSI_VALID 0x10 /* if any of ctl,extn chain rssis are valid */
#define ATH_RX_RSSI_EXTN_VALID  0x20 /* if extn chain rssis are valid */
#define ATH_RX_40MHZ            0x40 /* set if 40Mhz, clear if 20Mhz */
#define ATH_RX_SHORT_GI         0x80 /* set if short GI, clear if full GI */
#define ATH_RX_SM_ENABLE        0x100
#define ATH_RX_KEYMISS          0x200
#define ATH_RX_KEYINVALID       0x400

#ifdef ATH_SUPPORT_TxBF
    u_int8_t    bf_flags;           /*Handle OS depend function*/
#define ATH_BFRX_PKT_MORE_ACK           0x01
#define ATH_BFRX_PKT_MORE_QoSNULL       0x02
#define ATH_BFRX_PKT_MORE_DELAY         0x04
#define ATH_BFRX_DATA_UPLOAD_ACK        0x10
#define ATH_BFRX_DATA_UPLOAD_QosNULL    0x20
#define ATH_BFRX_DATA_UPLOAD_DELAY      0x40

    u_int32_t   rpttstamp;      /* cv report time statmp*/
#endif
#if ATH_VOW_EXT_STATS
    u_int32_t vow_extstats_offset;  /* Lower 16 bits holds the udp checksum offset in the data pkt */
                                    /* Higher 16 bits contains offset in the data pkt at which vow ext stats are embedded */
#endif    
    u_int8_t  isaggr;               /* Is this frame part of an aggregate */
    u_int8_t  isapsd;               /* Is this frame an APSD trigger frame */
    int16_t   noisefloor;           /* Noise floor */
    u_int16_t channel;              /* Channel */
} ieee80211_rx_status_t;

typedef struct {
    int             rssi;       /* RSSI (noise floor ajusted) */
    int             rssictl[ATH_MAX_ANTENNA];       /* RSSI (noise floor ajusted) */
    int             rssiextn[ATH_MAX_ANTENNA];       /* RSSI (noise floor ajusted) */
    int             rateieee;   /* data rate xmitted (IEEE rate code) */
    int             rateKbps;   /* data rate xmitted (Kbps) */
    int             ratecode;   /* phy rate code */
    int             flags;      /* validity flags */
#define ATH_TX_CHAIN_RSSI_VALID 0x01 /* if any of ctl,extn chain rssis are valid */
#define ATH_TX_RSSI_EXTN_VALID  0x02 /* if extn chain rssis are valid */
    u_int32_t       airtime;    /* time on air per final tx rate */
} ieee80211_tx_stat_t;

#ifdef ATH_SUPPORT_DFS
/* DFS channel notification status */
#if 0
enum ieee80211_dfs_chan_status {
    IEEE80211_DFS_RADAR         = 0,    /* radar found on channel n */
    IEEE80211_DFS_ROAMED_TO     = 1,    /* roamed to channel n */
    IEEE80211_DFS_CAC_START     = 2,    /* cac started on channel n */
    IEEE80211_DFS_CAC_END       = 3,    /* cac complete on channel n */
};
struct ieee80211_dfs_chan_status_event {
    uint16_t    iev_freq;    /* freq radar detected on */
    uint8_t        iev_ieee;    /* chan radar detected on */
    uint8_t        iev_status;    /* dfs channel status */
};
#endif

#endif

/* VAP configuration (from protocol layer) */
struct ath_vap_config {
    u_int32_t        av_fixed_rateset;
    u_int32_t        av_fixed_retryset;
#if ATH_SUPPORT_AP_WDS_COMBO
    u_int8_t	     av_no_beacon; 		/* vap will not xmit beacon/probe resp. */
#endif
#ifdef ATH_SUPPORT_TxBF
    u_int8_t         av_auto_cv_update;
    u_int8_t         av_cvupdate_per;
#endif
    u_int8_t         av_short_gi;
};

#ifdef ATH_CCX
struct ath_mib_cycle_cnts {
    u_int32_t   txFrameCount;
    u_int32_t   rxFrameCount;
    u_int32_t   rxClearCount;
    u_int32_t   cycleCount;
    u_int8_t    isRxActive;
    u_int8_t    isTxActive;
};

struct ath_mib_mac_stats {
    u_int32_t   ackrcv_bad;
    u_int32_t   rts_bad;
    u_int32_t   rts_good;
    u_int32_t   fcs_bad;
    u_int32_t   beacons;
#ifdef ATH_SUPPORT_HTC
    HTC_HOST_TGT_MIB_STATS  tgt_stats;
#endif
};

struct ath_chan_data {
    u_int8_t    clockRate;
    u_int32_t   noiseFloor;
    u_int8_t    ccaThreshold;
};
#endif
struct ieee80211_ops {
    int         (*get_netif_settings)(ieee80211_handle_t);
#define ATH_NETIF_RUNNING       0x01
#define ATH_NETIF_PROMISCUOUS   0x02
#define ATH_NETIF_ALLMULTI      0x04
#define ATH_NETIF_NO_BEACON     0x08
    void        (*netif_mcast_merge)(ieee80211_handle_t, u_int32_t mfilt[2]);
    
    void        (*setup_channel_list)(ieee80211_handle_t, enum ieee80211_clist_cmd cmd,
                                      const HAL_CHANNEL *chans, int nchan,
                                      const u_int8_t *regclassids, u_int nregclass,
                                      int countrycode);
    int         (*set_countrycode)(ieee80211_handle_t, char *isoName, u_int16_t cc, enum ieee80211_clist_cmd cmd);
    void        (*update_txpow)(ieee80211_handle_t, u_int16_t txpowlimit, u_int16_t txpowlevel);
    void        (*get_beacon_config)(ieee80211_handle_t, int if_id, ieee80211_beacon_config_t *conf);
    wbuf_t      (*get_beacon)(ieee80211_handle_t, int if_id, ieee80211_beacon_offset_t *bo, ieee80211_tx_control_t *txctl);
    int         (*update_beacon)(ieee80211_handle_t, int if_id, ieee80211_beacon_offset_t *bo, wbuf_t wbuf, int mcast);
    void        (*notify_beacon_miss)(ieee80211_handle_t);
    void        (*proc_tim)(ieee80211_handle_t);
    int         (*send_bar)(ieee80211_node_t, u_int8_t tidno, u_int16_t seqno);
    void        (*notify_txq_status)(ieee80211_handle_t, u_int16_t queue_depth);
    void        (*tx_complete)(wbuf_t wbuf, ieee80211_tx_status_t *);
    void        (*tx_status)(ieee80211_node_t, ieee80211_tx_stat_t *);
    int         (*rx_indicate)(ieee80211_handle_t, wbuf_t wbuf, ieee80211_rx_status_t *status, u_int16_t keyix);
    int         (*rx_subframe)(ieee80211_node_t, wbuf_t wbuf, ieee80211_rx_status_t *status);
    HAL_HT_MACMODE (*cwm_macmode)(ieee80211_handle_t);
#ifdef ATH_SUPPORT_DFS
    /* DFS related ops */
    void        (*ath_net80211_dfs_test_return)(ieee80211_handle_t ieee, u_int8_t ieeeChan);
    void        (*ath_net80211_mark_dfs)(ieee80211_handle_t ieee, struct ieee80211_channel *ichan);
#endif
    void        (*ath_net80211_set_vap_state)(ieee80211_handle_t ieee,u_int8_t if_id, u_int8_t state);
    void        (*ath_net80211_change_channel)(ieee80211_handle_t ieee, struct ieee80211_channel *chan);
    void        (*ath_net80211_switch_mode_static20)(ieee80211_handle_t ieee);
    void        (*ath_net80211_switch_mode_dynamic2040)(ieee80211_handle_t ieee);
    /* Rate control related ops */
    void        (*setup_rate)(ieee80211_handle_t, WIRELESS_MODE mode, RATE_TYPE type, const HAL_RATE_TABLE *rt);
    void        (*update_txrate)(ieee80211_node_t, int txrate);
    void        (*rate_newstate)(ieee80211_handle_t ieee, ieee80211_if_t if_data);
    void        (*rate_node_update)(ieee80211_handle_t ieee, ieee80211_node_t, int isnew);

    void        (*drain_amsdu)(ieee80211_handle_t);
    int         (*node_get_extradelimwar)(ieee80211_node_t ni);
#if ATH_SUPPORT_SPECTRAL
    void        (*spectral_indicate)(ieee80211_handle_t, void*, u_int32_t);
#endif
#ifdef ATH_SUPPORT_UAPSD
    bool        (*check_uapsdtrigger)(ieee80211_handle_t ieee, struct ieee80211_qosframe *qwh, u_int16_t keyix, bool isr_context);
    void        (*uapsd_eospindicate)(ieee80211_node_t node, wbuf_t wbuf, int txok, int force_eosp);
    wbuf_t      (*uapsd_allocqosnullframe)(ieee80211_handle_t ieee);
    wbuf_t      (*uapsd_getqosnullframe)(ieee80211_node_t node, wbuf_t wbuf, int ac);
    void        (*uapsd_retqosnullframe)(ieee80211_node_t node, wbuf_t wbuf);
    void        (*uapsd_deliverdata)(ieee80211_handle_t ieee, struct ieee80211_qosframe *qwh, u_int16_t keyix, u_int8_t is_trig, bool isr_context);
#endif
    u_int16_t   (*get_htmaxrate)(ieee80211_node_t);
#if ATH_SUPPORT_IQUE
    void        (*hbr_settrigger)(ieee80211_node_t node, int state);
    u_int8_t    (*get_hbr_block_state)(ieee80211_node_t node);    
#endif    
#if ATH_SUPPORT_VOWEXT
    u_int16_t   (*get_aid)(ieee80211_node_t);
#endif
#ifdef ATH_SUPPORT_LINUX_STA
    void	(*ath_net80211_suspend)(ieee80211_handle_t ieee);
    void	(*ath_net80211_resume)(ieee80211_handle_t ieee);
#endif
#ifdef ATH_BT_COEX
    void        (*bt_coex_ps_enable)(ieee80211_handle_t, u_int32_t mode);
    void        (*bt_coex_ps_poll)(ieee80211_handle_t, u_int32_t lastpoll);
#endif
#ifdef ATH_SUPPORT_HTC
    u_int8_t    (*ath_htc_gettargetnodeid)(ieee80211_handle_t, ieee80211_node_t);
    void	    (*ath_htc_wmm_update)(ieee80211_handle_t ieee);
    u_int8_t    (*ath_htc_gettargetvapid)(ieee80211_handle_t, ieee80211_node_t);
    void        (*ath_net80211_uapsd_creditupdate)(ieee80211_handle_t);
#endif
#if ATH_SUPPORT_CFEND
    wbuf_t      (*cfend_alloc)( ieee80211_handle_t ieee);
#endif
#ifndef REMOVE_PKTLOG_PROTO
    u_int8_t    *(*parse_frm)(ieee80211_handle_t ieee, wbuf_t, ieee80211_node_t,
                              void *frm, u_int16_t keyix);
#endif

#if  ATH_SUPPORT_AOW
    void        (*ieee80211_send2all_nodes)(void *reqvap, void *data, int len,
                                            u_int32_t seqno, u_int64_t tsf);
    int         (*ieee80211_consume_audio_data)(ieee80211_handle_t ieee,
                                                 u_int64_t curTime);
#endif  /* ATH_SUPPORT_AOW */
    void        (*get_bssid)(ieee80211_handle_t ieee,
                    struct ieee80211_frame_min *shdr, u_int8_t *bssid);
#ifdef ATH_TX99_DIAG
    struct ieee80211_channel *  (*ath_net80211_find_channel)(struct ath_softc *sc, int ieee, enum ieee80211_phymode mode);
#endif      
    void        (*set_stbc_config)(ieee80211_handle_t, u_int8_t stbc, u_int8_t istx);
    void        (*set_ldpc_config)(ieee80211_handle_t, u_int8_t ldpc);
#if UMAC_SUPPORT_SMARTANTENNA
    void        (*update_smartant_pertable) (ieee80211_node_t, void *pertab);
#endif
#if ATH_SUPPORT_VOW_DCS
     /**  
     * callback to umac for handling interference   
     *  @param     ieee       : handle to umac radio .
     */
    void        (*interference_handler)(ieee80211_handle_t ieee);
#endif
#ifdef ATH_SWRETRY
    u_int16_t   (*get_pwrsaveq_len)(ieee80211_node_t node);
    void	(*set_tim)(ieee80211_node_t node, u_int8_t setflag);
#endif 
};

/**
 * @}
 */

/*
 * Device revision information.
 */
typedef enum {
    ATH_MAC_VERSION,        /* MAC version id */
    ATH_MAC_REV,            /* MAC revision */
    ATH_PHY_REV,            /* PHY revision */
    ATH_ANALOG5GHZ_REV,     /* 5GHz radio revision */
    ATH_ANALOG2GHZ_REV,     /* 2GHz radio revision */
    ATH_WIRELESSMODE,       /* Wireless mode select */
} ATH_DEVICE_INFO;


#if ATH_SUPPORT_VOWEXT
/**
 * @brief List of VOW capabilities that can be set or unset
 *        dynamically
 * @see VOW feature document for details on each feature
 * 
 */
#define	ATH_CAP_VOWEXT_FAIR_QUEUING     0x1
#define	ATH_CAP_VOWEXT_AGGR_SIZING      0x4
#define	ATH_CAP_VOWEXT_BUFF_REORDER     0x8
#define ATH_CAP_VOWEXT_RATE_CTRL_N_AGGR   0x10 /* for RCA */
#define ATH_CAP_VOWEXT_MASK (ATH_CAP_VOWEXT_FAIR_QUEUING |\
                            ATH_CAP_VOWEXT_AGGR_SIZING|\
                            ATH_CAP_VOWEXT_BUFF_REORDER|\
			          ATH_CAP_VOWEXT_RATE_CTRL_N_AGGR) 
#endif

#ifdef VOW_TIDSCHED
#define ATH_TIDSCHED 0                  /* Disable TID scheduling */

/* Access category relative queue weights (default) */
#define ATH_TIDSCHED_VOQW 1000          /* Voice queue heavily weighted to ensure that */
                                        /* voice traffic is always put at the head of the queue */
#define ATH_TIDSCHED_VIQW 4             /* Video queue weight */
#define ATH_TIDSCHED_BKQW 1             /* Background queue weight */
#define ATH_TIDSCHED_BEQW 1             /* Best-effort queue weight */

/* Access category queue timeouts (defaults) - time in us after which queue weight */
/* is overridden and queue is moved to the head of the scheduling queue */
#define ATH_TIDSCHED_VOTO 10*1000       /* Voice queue timeout (10ms) */
#define ATH_TIDSCHED_VITO 0             /* Video queue timeout (disabled) */
#define ATH_TIDSCHED_BKTO 0             /* Background queue timeout (disabled) */
#define ATH_TIDSCHED_BETO 0             /* Best-effort queue timeout (disabled) */
#endif

#ifdef VOW_LOGLATENCY
#define ATH_LOGLATENCY 0
#endif

/**
 * @brief Capabilities of Atheros Devices
 */
typedef enum {
    ATH_CAP_FF = 0,
    ATH_CAP_BURST,
    ATH_CAP_COMPRESSION,
    ATH_CAP_TURBO,
    ATH_CAP_XR,
    ATH_CAP_TXPOWER,
    ATH_CAP_DIVERSITY,
    ATH_CAP_BSSIDMASK,
    ATH_CAP_TKIP_SPLITMIC,
    ATH_CAP_MCAST_KEYSEARCH,
    ATH_CAP_TKIP_WMEMIC,
    ATH_CAP_WMM,
    ATH_CAP_HT,
    ATH_CAP_HT20_SGI,
    ATH_CAP_RX_STBC,
    ATH_CAP_TX_STBC,
    ATH_CAP_LDPC,
    ATH_CAP_4ADDR_AGGR,
    ATH_CAP_ENHANCED_PM_SUPPORT,
    ATH_CAP_MBSSID_AGGR_SUPPORT,
#ifdef ATH_SWRETRY 
    ATH_CAP_SWRETRY_SUPPORT,
#endif
#ifdef ATH_SUPPORT_UAPSD
    ATH_CAP_UAPSD,
#endif
    ATH_CAP_NUMTXCHAINS,
    ATH_CAP_NUMRXCHAINS,
    ATH_CAP_RXCHAINMASK,
    ATH_CAP_TXCHAINMASK,
    ATH_CAP_DYNAMIC_SMPS,
    ATH_CAP_WPS_BUTTON,
    ATH_CAP_EDMA_SUPPORT,
    ATH_CAP_WEP_TKIP_AGGR,
    ATH_CAP_DS,
    ATH_CAP_TS,
    ATH_CAP_DEV_TYPE,
#ifdef  ATH_SUPPORT_TxBF
    ATH_CAP_TXBF,
#endif
    ATH_CAP_EXTRADELIMWAR, /* for osprey 1.0 and older 11n chips */
    ATH_CAP_LDPCWAR, /* for osprey <= 2.2 and Peacock */
    ATH_CAP_ZERO_MPDU_DENSITY,
#if ATH_SUPPORT_DYN_TX_CHAINMASK
    ATH_CAP_DYN_TXCHAINMASK,
#endif /* ATH_CAP_DYN_TXCHAINMASK */
#if ATH_SUPPORT_WAPI
    ATH_CAP_WAPI_MAXTXCHAINS,
    ATH_CAP_WAPI_MAXRXCHAINS,
#endif
#if UMAC_SUPPORT_SMARTANTENNA    
    ATH_CAP_SMARTANTENNA,
#endif    
} ATH_CAPABILITY_TYPE;

typedef enum {
    ATH_RX_NON_CONSUMED = 0,
    ATH_RX_CONSUMED
} ATH_RX_TYPE;

#ifdef ATH_RB
typedef enum {
    ATH_RB_MODE_OFF,
    ATH_RB_MODE_DETECT,
    ATH_RB_MODE_FORCE
} ath_rb_mode_t;
#endif

// Parameters for get/set _txqproperty()
#define     TXQ_PROP_PRIORITY           0x00   /* not used */
#define     TXQ_PROP_AIFS               0x01   /* aifs */
#define     TXQ_PROP_CWMIN              0x02   /* cwmin */
#define     TXQ_PROP_CWMAX              0x03   /* cwmax */
#define     TXQ_PROP_SHORT_RETRY_LIMIT  0x04   /* short retry limit */
#define     TXQ_PROP_LONG_RETRY_LIMIT   0x05   /* long retry limit - not used */
#define     TXQ_PROP_CBR_PERIOD         0x06   /*  */
#define     TXQ_PROP_CBR_OVFLO_LIMIT    0x07   /*  */
#define     TXQ_PROP_BURST_TIME         0x08   /*  */
#define     TXQ_PROP_READY_TIME         0x09   /* specified in msecs */
#define     TXQ_PROP_COMP_BUF           0x0a   /* compression buffer phys addr */

struct ATH_HW_REVISION
{
    u_int32_t   macversion;     /* MAC version id */
    u_int16_t   macrev;         /* MAC revision */
    u_int16_t   phyrev;         /* PHY revision */
    u_int16_t   analog5GhzRev;  /* 5GHz radio revision */
    u_int16_t   analog2GhzRev;  /* 2GHz radio revision */
};


#define IEEE80211_ADDR_LEN    6

/* Reset flag */
#define RESET_RETRY_TXQ     0x00000001

typedef enum {
    ATH_MFP_QOSDATA = 0,
    ATH_MFP_PASSTHRU,
    ATH_MFP_HW_CRYPTO
} ATH_MFP_OPT_t;

typedef void (*ath_notify_tx_beacon_function)(void *arg, int beacon_id, u_int32_t tx_status);
typedef void (*ath_vap_info_notify_func)(void *arg, ath_vap_infotype infotype,
                                         u_int32_t param1, u_int32_t param2);

struct ath_ops {
    /* To query bit masks of available modes */
    u_int       (*get_wirelessmodes)(ath_dev_t dev);
    
    /* To query hardware version and revision */
    u_int32_t   (*get_device_info)(ath_dev_t dev, u_int32_t type);

    /* To query hardware capabilities */
    int         (*have_capability)(ath_dev_t, ATH_CAPABILITY_TYPE type);
    int         (*has_cipher)(ath_dev_t, HAL_CIPHER cipher);
    
    /* To initialize/de-initialize */
    int         (*open)(ath_dev_t, HAL_CHANNEL *initial_chan);
    int         (*stop)(ath_dev_t);

    /* To attach/detach virtual interface */
#define ATH_IF_ID_ANY   0xff
#if WAR_DELETE_VAP    
    int         (*add_interface)(ath_dev_t, int if_id, ieee80211_if_t if_data, HAL_OPMODE opmode, HAL_OPMODE iv_opmode, int nostabeacon, void **athvap);
    int         (*remove_interface)(ath_dev_t, int if_id, void *athvap);
#else
    int         (*add_interface)(ath_dev_t, int if_id, ieee80211_if_t if_data, HAL_OPMODE opmode, HAL_OPMODE iv_opmode, int nostabeacon);
    int         (*remove_interface)(ath_dev_t, int if_id);
#endif
    int         (*config_interface)(ath_dev_t, int if_id, struct ath_vap_config *if_config);

    /* Notification of event/state of virtual interface */
    int         (*down)(ath_dev_t, int if_id, u_int flags);
    int         (*listen)(ath_dev_t, int if_id);
    int         (*join)(ath_dev_t, int if_id, const u_int8_t bssid[IEEE80211_ADDR_LEN], u_int flags);
    int         (*up)(ath_dev_t, int if_id, const u_int8_t bssid[IEEE80211_ADDR_LEN], u_int8_t aid, u_int flags);
    int         (*stopping)(ath_dev_t dev, int if_id);

#define ATH_IF_HW_OFF           0x0001  /* hardware state needs to turn off */
#define ATH_IF_HW_ON            0x0002  /* hardware state needs to turn on */
#define ATH_IF_HT               0x0004  /* STA only: the associated AP is HT capable */
#define ATH_IF_PRIVACY          0x0008  /* AP/IBSS only: current BSS has privacy on */
#define ATH_IF_BEACON_ENABLE    0x0010  /* AP/IBSS only: enable beacon */
#define ATH_IF_BEACON_SYNC      0x0020  /* IBSS only: need to sync beacon */
#define ATH_IF_DTURBO           0x0040  /* STA: the associated AP is dynamic turbo capable
                                         * AP: current BSS is in turbo mode */
    
    /* To attach/detach per-destination info (i.e., node) */
    ath_node_t  (*alloc_node)(ath_dev_t, int if_id, ieee80211_node_t, bool tmpnode);
    void        (*free_node)(ath_dev_t, ath_node_t);
    void        (*cleanup_node)(ath_dev_t, ath_node_t);
    void        (*update_node_pwrsave)(ath_dev_t, ath_node_t, int, int);

    /* notify a newly associated node */
    void        (*new_assoc)(ath_dev_t, ath_node_t, int isnew, int isuapsd);

    /* interrupt processing */
    void        (*enable_interrupt)(ath_dev_t);
    void        (*disable_interrupt)(ath_dev_t);
#define ATH_ISR_NOSCHED         0x0000  /* Do not schedule bottom half/DPC                */
#define ATH_ISR_SCHED           0x0001  /* Schedule the bottom half for execution        */
#define ATH_ISR_NOTMINE         0x0002  /* This was not my interrupt - for shared IRQ's    */
    int         (*isr)(ath_dev_t);
    int         (*msisr)(ath_dev_t, u_int);
    void        (*handle_intr)(ath_dev_t);

    /* reset */
    int         (*reset_start)(ath_dev_t, HAL_BOOL, int tx_timeout, int rx_timeout);
    int         (*reset)(ath_dev_t);
    int         (*reset_end)(ath_dev_t, HAL_BOOL);
    int         (*switch_opmode)(ath_dev_t, HAL_OPMODE opmode);

    /* set and get channel */
    void        (*set_channel)(ath_dev_t, HAL_CHANNEL *, int tx_timeout, int rx_timeout, bool flush);
	HAL_CHANNEL *(*get_channel)(ath_dev_t dev);

    /* scan notifications */
    void        (*scan_start)(ath_dev_t);
    void        (*scan_end)(ath_dev_t);
    void        (*scan_enable_txq)(ath_dev_t);
    void        (*led_scan_start)(ath_dev_t);
    void        (*led_scan_end)(ath_dev_t);

    /* force ppm notications */
    void        (*force_ppm_notify)(ath_dev_t, int event, u_int8_t *bssid);

    /* beacon synchronization */
    void        (*sync_beacon)(ath_dev_t, int if_id);
    void        (*update_beacon_info)(ath_dev_t, int avgrssi);

#ifdef ATH_USB
    /* Heart beat callback */
    void        (*heartbeat_callback)(ath_dev_t);
#endif

    /* tx callbacks */
    int         (*tx_init)(ath_dev_t, int nbufs);
    int         (*tx_cleanup)(ath_dev_t);
    int         (*tx_get_qnum)(ath_dev_t, int qtype, int haltype);
    int         (*txq_update)(ath_dev_t, int qnum, HAL_TXQ_INFO *qi);
    int         (*tx)(ath_dev_t, wbuf_t wbuf, ieee80211_tx_control_t *txctl);
    void        (*tx_proc)(ath_dev_t);
    void        (*tx_flush)(ath_dev_t);
    u_int32_t   (*txq_depth)(ath_dev_t, int qnum);
    u_int32_t   (*txq_aggr_depth)(ath_dev_t, int qnum);
    void        (*txq_set_minfree)(ath_dev_t, int qtype, int haltype, u_int minfree);

    /* rx callbacks */
    int         (*rx_init)(ath_dev_t, int nbufs);
    void        (*rx_cleanup)(ath_dev_t);
    int         (*rx_proc)(ath_dev_t, int flushing);
    void        (*rx_requeue)(ath_dev_t, wbuf_t wbuf);
    int         (*rx_proc_frame)(ath_dev_t, ath_node_t, int is_ampdu,
                                 wbuf_t wbuf, ieee80211_rx_status_t *rx_status,
                                 ATH_RX_TYPE *status);

    /* aggregation callbacks */
    int         (*check_aggr)(ath_dev_t, ath_node_t, u_int8_t tidno);
    void        (*set_ampdu_params)(ath_dev_t, ath_node_t, u_int16_t maxampdu, u_int32_t mpdudensity);
    void        (*set_weptkip_rxdelim)(ath_dev_t, ath_node_t, u_int8_t rxdelim);
    void        (*addba_request_setup)(ath_dev_t, ath_node_t, u_int8_t tidno,
                                       struct ieee80211_ba_parameterset *baparamset,
                                       u_int16_t *batimeout,
                                       struct ieee80211_ba_seqctrl *basequencectrl,
                                       u_int16_t buffersize);
    void        (*addba_response_setup)(ath_dev_t, ath_node_t an,
                                        u_int8_t tidno, u_int8_t *dialogtoken,
                                        u_int16_t *statuscode,
                                        struct ieee80211_ba_parameterset *baparamset,
                                        u_int16_t *batimeout);
    int         (*addba_request_process)(ath_dev_t, ath_node_t an,
                                         u_int8_t dialogtoken,
                                         struct ieee80211_ba_parameterset *baparamset,
                                         u_int16_t batimeout,
                                         struct ieee80211_ba_seqctrl basequencectrl);
    void        (*addba_response_process)(ath_dev_t, ath_node_t,
                                          u_int16_t statuscode,
                                          struct ieee80211_ba_parameterset *baparamset,
                                          u_int16_t batimeout);
    void        (*addba_clear)(ath_dev_t, ath_node_t);
    void        (*addba_cancel_timers)(ath_dev_t, ath_node_t);
    void        (*delba_process)(ath_dev_t, ath_node_t an,
                                 struct ieee80211_delba_parameterset *delbaparamset,
                                 u_int16_t reasoncode);
    u_int16_t   (*addba_status)(ath_dev_t, ath_node_t, u_int8_t tidno);
    void        (*aggr_teardown)(ath_dev_t, ath_node_t, u_int8_t tidno, u_int8_t initiator);
    void        (*set_addbaresponse)(ath_dev_t, ath_node_t, u_int8_t tidno, u_int16_t statuscode);
    void        (*clear_addbaresponsestatus)(ath_dev_t, ath_node_t);
    
    /* Misc runtime configuration */
    void        (*set_slottime)(ath_dev_t, int slottime);
    void        (*set_protmode)(ath_dev_t, PROT_MODE);
    int         (*set_txpowlimit)(ath_dev_t, u_int16_t txpowlimit, u_int32_t is2GHz);
    void        (*enable_tpc)(ath_dev_t);
    
    /* get/set MAC address, multicast list, etc. */
    void        (*get_macaddr)(ath_dev_t, u_int8_t macaddr[IEEE80211_ADDR_LEN]);
    void        (*get_hw_macaddr)(ath_dev_t, u_int8_t macaddr[IEEE80211_ADDR_LEN]);
    void        (*set_macaddr)(ath_dev_t, const u_int8_t macaddr[IEEE80211_ADDR_LEN]);
    void        (*set_mcastlist)(ath_dev_t);
    void        (*set_rxfilter)(ath_dev_t);
    void        (*mc_upload)(ath_dev_t);

    /* key cache callbacks */
    u_int16_t   (*key_alloc_2pair)(ath_dev_t);
    u_int16_t   (*key_alloc_pair)(ath_dev_t);
    u_int16_t   (*key_alloc_single)(ath_dev_t);
    void        (*key_delete)(ath_dev_t, u_int16_t keyix, int freeslot);
    int         (*key_set)(ath_dev_t, u_int16_t keyix, HAL_KEYVAL *hk,
                           const u_int8_t mac[IEEE80211_ADDR_LEN]);
    u_int       (*keycache_size)(ath_dev_t);
    
    /* PHY state */
    int         (*get_sw_phystate)(ath_dev_t);
    int         (*get_hw_phystate)(ath_dev_t);
    void        (*set_sw_phystate)(ath_dev_t, int onoff);
    int         (*radio_enable)(ath_dev_t);
    int         (*radio_disable)(ath_dev_t);
    
    /* LED control */
    void        (*led_suspend)(ath_dev_t);
    
    /* power management */
    void        (*awake)(ath_dev_t);
    void        (*netsleep)(ath_dev_t);
    void        (*fullsleep)(ath_dev_t);
    int         (*getpwrsavestate)(ath_dev_t);
    void        (*setpwrsavestate)(ath_dev_t, int);

    /* regdomain callbacks */
    int         (*set_country)(ath_dev_t, char *iso_name, u_int16_t, enum ieee80211_clist_cmd);
    void        (*get_current_country)(ath_dev_t, HAL_COUNTRY_ENTRY *ctry);
    int         (*set_regdomain)(ath_dev_t, int regdomain, HAL_BOOL);
    int         (*get_regdomain)(ath_dev_t);
    int         (*get_dfsdomain)(ath_dev_t);
    int         (*set_quiet)(ath_dev_t, u_int16_t period, u_int16_t duration,
                             u_int16_t nextStart, u_int16_t enabled);
    u_int16_t   (*find_countrycode)(ath_dev_t, char* isoName);
    u_int8_t    (*get_ctl_by_country)(ath_dev_t, u_int8_t *country, HAL_BOOL is2G);
    u_int16_t   (*dfs_isdfsregdomain)(ath_dev_t);
    u_int16_t   (*dfs_in_cac)(ath_dev_t);
    u_int16_t   (*dfs_usenol)(ath_dev_t);

    /* antenna select */
    int         (*set_antenna_select)(ath_dev_t, u_int32_t antenna_select);
    u_int32_t   (*get_current_tx_antenna)(ath_dev_t);
    u_int32_t   (*get_default_antenna)(ath_dev_t);

    /* PnP */
    void        (*notify_device_removal)(ath_dev_t);
    int         (*detect_card_present)(ath_dev_t);
    
    /* convert frequency to channel number */
    u_int       (*mhz2ieee)(ath_dev_t, u_int freq, u_int flags);

    /* statistics */
    struct ath_phy_stats    *(*get_phy_stats)(ath_dev_t, WIRELESS_MODE);
    struct ath_stats        *(*get_ath_stats)(ath_dev_t);
    struct ath_11n_stats    *(*get_11n_stats)(ath_dev_t);
    void                    (*clear_stats)(ath_dev_t);
    
    /* channel width management */
    void        (*set_macmode)(ath_dev_t, HAL_HT_MACMODE);
    void        (*set_extprotspacing)(ath_dev_t, HAL_HT_EXTPROTSPACING);
    int         (*get_extbusyper)(ath_dev_t);

#ifdef ATH_SUPERG_FF
    /* fast frame */
    int         (*check_ff)(ath_dev_t, ath_node_t, int, int, u_int32_t,u_int32_t);
#endif
#ifdef ATH_SUPPORT_DFS
/* DFS ioctl support */
    int         (*ath_dfs_control)(ath_dev_t dev, u_int id,
                                   void *indata, u_int32_t insize,
                                   void *outdata, u_int32_t *outsize);
#endif                 
#if ATH_WOW        
    /* Wake on Wireless */
    int         (*ath_get_wow_support)(ath_dev_t);
    int         (*ath_set_wow_enable)(ath_dev_t, int clearbssid);
    int         (*ath_wow_wakeup)(ath_dev_t);
    void        (*ath_set_wow_events)(ath_dev_t, u_int32_t);
    int         (*ath_get_wow_events)(ath_dev_t);
    int         (*ath_wow_add_wakeup_pattern)(ath_dev_t, u_int8_t *, u_int8_t *, u_int32_t);
    int         (*ath_wow_remove_wakeup_pattern)(ath_dev_t, u_int8_t *, u_int8_t *);
    int         (*ath_get_wow_wakeup_reason)(ath_dev_t);
    int         (*ath_wow_matchpattern_exact)(ath_dev_t);
    void        (*ath_wow_set_duration)(ath_dev_t, u_int16_t);
    void        (*ath_set_wow_timeout)(ath_dev_t, u_int32_t);
	u_int32_t   (*ath_get_wow_timeout)(ath_dev_t); 
#endif    

    /* Configuration Interface */
    int         (*ath_get_config_param)(ath_dev_t dev, ath_param_ID_t ID, void *buff);
    int         (*ath_set_config_param)(ath_dev_t dev, ath_param_ID_t ID, void *buff);

    /* Noise floor func */
    short       (*get_noisefloor)(ath_dev_t dev, unsigned short    freq,  unsigned int chan_flags);
	void        (*get_chainnoisefloor)(ath_dev_t dev, unsigned short freq, unsigned int chan_flags, int16_t *nfBuf);

#if ATH_SUPPORT_RAW_ADC_CAPTURE
    int         (*get_min_agc_gain)(ath_dev_t dev, unsigned short freq, int32_t *gain_buf, int num_gain_buf);
#endif

    void        (*ath_sm_pwrsave_update)(ath_dev_t, ath_node_t,
                                         ATH_SM_PWRSAV mode, int);

    /* Transmit rate */
    u_int32_t   (*node_gettxrate)(ath_node_t);
    void        (*node_setfixedrate)(ath_node_t, u_int8_t);
    u_int32_t   (*node_getmaxphyrate)(ath_dev_t, ath_node_t,uint32_t shortgi);

    /* Rate control */
    int         (*ath_rate_newassoc)(ath_dev_t dev, ath_node_t , int isnew, unsigned int capflag,
                                     struct ieee80211_rateset *negotiated_rates,
                                     struct ieee80211_rateset *negotiated_htrates);
    void        (*ath_rate_newstate)(ath_dev_t dev, int if_id, int up);

#ifdef DBG
    /* Debug feature: register access */
    u_int32_t   (*ath_register_read)(ath_dev_t, u_int32_t);
    void        (*ath_register_write)(ath_dev_t, u_int32_t, u_int32_t);
#endif

    /* TX Power Limit */
    int         (*ath_set_txPwrAdjust)(ath_dev_t dev, int32_t adjust, u_int32_t is2GHz);
    int         (*ath_set_txPwrLimit)(ath_dev_t dev, u_int32_t limit, u_int16_t tpcInDb, u_int32_t is2GHz);
    u_int8_t    (*get_common_power)(u_int16_t freq);

    /* TSF 32/64 */
    u_int32_t   (*ath_get_tsf32)(ath_dev_t dev);
    u_int64_t   (*ath_get_tsf64)(ath_dev_t dev);

    /* Rx Filter */
    void        (*ath_set_rxfilter)(ath_dev_t dev);
	/* Select PLCP header or EVM data */
    int         (*ath_set_sel_evm)(ath_dev_t dev, int selEvm, int justQuery);
#ifdef ATH_CCX
    /* MIB Control */
    int         (*ath_update_mibMacstats)(ath_dev_t dev);
    int         (*ath_get_mibMacstats)(ath_dev_t dev, struct ath_mib_mac_stats *pStats);
    u_int8_t    (*rcRateValueToPer)(ath_dev_t, struct ath_node *, int);
    int         (*ath_get_mibCycleCounts)(ath_dev_t dev, struct ath_mib_cycle_cnts *pCnts);
    void        (*ath_clear_mibCounters)(ath_dev_t dev);

    /* Get Serial Number */
    int         (*ath_get_sernum)(ath_dev_t dev, u_int8_t *pSn, int limit);

    /* Get Channel Data */
    int         (*ath_get_chandata)(ath_dev_t dev, struct ieee80211_channel *pChan, struct ath_chan_data *pData);

    /* Get Current RSSI */
    u_int32_t   (*ath_get_curRSSI)(ath_dev_t dev);
#endif // ATH_CCX
    /* A-MSDU frame */
    int         (*get_amsdusupported)(ath_dev_t, ath_node_t, int);

#ifdef ATH_SWRETRY
    void        (*set_swretrystate)(ath_dev_t dev, ath_node_t node, int flag);
    int         (*ath_handle_pspoll)(ath_dev_t dev, ath_node_t node);
    /* Check whether there is pending frames in tid queue */
    int         (*get_exist_pendingfrm_tidq)(ath_dev_t dev, ath_node_t node);
    int         (*reset_paused_tid)(ath_dev_t dev, ath_node_t node);
#endif
#ifdef ATH_SUPPORT_UAPSD
    /* UAPSD */
    int         (*process_uapsd_trigger)(ath_dev_t dev, ath_node_t node, u_int8_t maxsp,
                                         u_int8_t ac, u_int8_t flush, u_int8_t maxqdepth);
    u_int32_t   (*uapsd_depth)(ath_node_t node);
#endif
#ifndef REMOVE_PKT_LOG
    int         (*pktlog_start)(ath_dev_t, int log_state);
    int         (*pktlog_read_hdr)(ath_dev_t, void *buf, u_int32_t buf_len,
                                   u_int32_t *required_len,
                                   u_int32_t *actual_len);
    int         (*pktlog_read_buf)(ath_dev_t, void *buf, u_int32_t buf_len,
                                   u_int32_t *required_len,
                                   u_int32_t *actual_len);
#endif
#if ATH_SUPPORT_IQUE
    /* Set and Get AC and Rate Control parameters */
    void        (*ath_set_acparams)(ath_dev_t, u_int8_t ac, u_int8_t use_rts,
                                    u_int8_t aggrsize_scaling, u_int32_t min_kbps);

    void        (*ath_set_rtparams)(ath_dev_t, u_int8_t rt_index, u_int8_t perThresh,
                                    u_int8_t probeInterval);
    void        (*ath_get_iqueconfig)(ath_dev_t);
    void        (*ath_set_hbrparams)(ath_dev_t, u_int8_t ac, u_int8_t enable, u_int8_t per);
#endif
    void        (*do_pwr_workaround)(ath_dev_t dev, u_int16_t channel, u_int32_t channelflags, u_int16_t stamode);
    void        (*get_txqproperty)(ath_dev_t dev, u_int32_t q_id, u_int32_t property, void *retval);
    void        (*set_txqproperty)(ath_dev_t dev, u_int32_t q_id, u_int32_t property, void *value);
    void        (*get_hwrevs)(ath_dev_t dev, struct ATH_HW_REVISION *hwrev);
    u_int32_t   (*rc_rate_maprix)(ath_dev_t dev, u_int16_t curmode, int isratecode);
    int         (*rc_rate_set_mcast_rate)(ath_dev_t dev, u_int32_t req_rate);
    void        (*set_defaultantenna)(ath_dev_t dev, u_int antenna);
    void        (*set_diversity)(ath_dev_t dev, u_int diversity);
    void        (*set_rx_chainmask)(ath_dev_t dev, u_int8_t mask);
    void        (*set_tx_chainmask)(ath_dev_t dev, u_int8_t mask);
    void        (*set_rx_chainmasklegacy)(ath_dev_t dev, u_int8_t mask);
    void        (*set_tx_chainmasklegacy)(ath_dev_t dev, u_int8_t mask);
    void        (*get_maxtxpower) (ath_dev_t dev, u_int32_t *txpow);
    void        (*ath_eeprom_read)(ath_dev_t dev, u_int16_t address, u_int32_t len, u_int16_t **value, u_int32_t *bytesread);
    void        (*log_text_fmt)(ath_dev_t dev,  u_int32_t env_flags, const char *fmt, ...);
    void        (*log_text)(ath_dev_t dev, const char *text);
#if AR_DEBUG
    void        (*node_queue_stats) (struct ath_softc *sc , ath_node_t node);
#endif
    int         (*rate_newstate_set) (ath_dev_t dev, int if_idx, int up);
    u_int8_t    (*rate_findrc) (ath_dev_t dev, u_int8_t rate);
#ifdef ATH_SUPPORT_HTC
    void        (*ath_wmi_get_target_stats)(ath_dev_t, HTC_HOST_TGT_MIB_STATS *);  /* ath_wmi_get_target_stats */ 
    void        (*ath_wmi_beacon)(ath_dev_t, u_int64_t, u_int8_t, u_int8_t);     /* ath_wmi_beacon */
    void        (*ath_wmi_bmiss)(ath_dev_t);                           /* ath_wmi_bmiss */
    void        (*ath_wmi_add_vap)(ath_dev_t, void *, int);            /* ath_wmi_add_vap */
    void        (*ath_wmi_add_node)(ath_dev_t, void *, int);           /* ath_wmi_add_node */
    void        (*ath_wmi_delete_node)(ath_dev_t, void *, int);        /* ath_wmi_delete_node */
    void        (*ath_wmi_update_node)(ath_dev_t, void *, int);        /* ath_wmi_update_node */
#if ENCAP_OFFLOAD    
    void        (*ath_wmi_update_vap)(ath_dev_t, void *, int);         /* ath_wmi_update_vap */
#endif    
    void        (*ath_htc_rx)(ath_dev_t, wbuf_t);                      /* ath_htc_rx */
    void        (*ath_wmi_ic_update_target)(ath_dev_t, void *, int);   /* ath_wmi_ic_update_target */
    void        (*ath_wmi_delete_vap)(ath_dev_t, void *, int);         /* ath_wmi_delete_vap */
    void        (*ath_schedule_wmm_update)(ath_dev_t);                 /* ath_schedule_wmm_update */
#ifdef ATH_HTC_TX_SCHED
    void         (*ath_htc_tx_schedule)(osdev_t, int epid);
    void         (*ath_htc_uapsd_credit_update)(ath_dev_t dev, ath_node_t an);
#endif
    void        (*ath_wmi_generc_timer)(ath_dev_t, u_int32_t, u_int32_t, u_int32_t);
    void        (*ath_wmi_pause_ctrl)(ath_dev_t, ath_node_t, u_int32_t);
#endif

    void        (*ath_printreg)(ath_dev_t dev, u_int32_t printctrl);
    /* Get MFP support level */
    u_int32_t   (*ath_get_mfpsupport)(ath_dev_t dev);

    void        (*ath_set_rxtimeout)(ath_dev_t, u_int8_t ac, u_int8_t rxtimeout);

#ifdef ATH_USB
    void        (*ath_usb_suspend)(ath_dev_t);                /*ath_usb_suspend*/
    void        (*ath_usb_rx_cleanup)(ath_dev_t);             /*ath_usb_rx_cleanup*/
#endif
#if ATH_SUPPORT_WIRESHARK
    void        (*ath_fill_radiotap_hdr)(ath_dev_t dev,
                                         struct ah_rx_radiotap_header *rh, struct ah_ppi_data *ppi,
                                         wbuf_t wbuf);
    int         (*ath_monitor_filter_phyerr)(ath_dev_t dev, int filterPhyErr, int justQuery);
#endif
#ifdef ATH_BT_COEX
    u_int32_t   (*bt_coex_ps_complete) (ath_dev_t dev, u_int32_t type);
    int         (*bt_coex_get_info) (ath_dev_t dev, u_int32_t type, void *);
    int         (*bt_coex_event) (ath_dev_t dev, u_int32_t coexevent, void *param);
#endif
    /* GreenAP stuff */
    void        (*ath_green_ap_dev_set_enable)( ath_dev_t dev, int val ); 
    int         (*ath_green_ap_dev_get_enable)( ath_dev_t dev);
    void        (*ath_green_ap_dev_set_transition_time)( ath_dev_t dev, int val );
    int         (*ath_green_ap_dev_get_transition_time)( ath_dev_t dev);
    void        (*ath_green_ap_dev_set_on_time)( ath_dev_t dev, int val );
    int         (*ath_green_ap_dev_get_on_time)( ath_dev_t dev);
    int16_t     (*ath_dev_get_noisefloor)( ath_dev_t dev);
#if ATH_SUPPORT_5G_EACS  
    void        (*ath_dev_get_chan_stats)(ath_dev_t dev, void *chan_stats);
#endif 
    int         (*ath_spectral_control)(ath_dev_t dev, u_int id,
                                        void *indata, u_int32_t insize,
                                        void *outdata, u_int32_t *outsize);
#if ATH_SUPPORT_SPECTRAL
    int         (*ath_dev_get_ctldutycycle)(ath_dev_t dev);
    int         (*ath_dev_get_extdutycycle)(ath_dev_t dev);
    void        (*ath_dev_start_spectral_scan)(ath_dev_t dev);
    void        (*ath_dev_stop_spectral_scan)(ath_dev_t dev);
#endif

#ifdef ATH_GEN_TIMER
    struct ath_gen_timer *  
                (*ath_tsf_timer_alloc)(ath_dev_t dev, int tsf_id,
                                        ath_hwtimer_function trigger_action,
                                        ath_hwtimer_function overflow_action,
                                        ath_hwtimer_function outofrange_action,
                                        void *arg);

    void        (*ath_tsf_timer_free)(ath_dev_t dev, struct ath_gen_timer *timer);

    void        (*ath_tsf_timer_start)(ath_dev_t dev, struct ath_gen_timer *timer,
                                        u_int32_t timer_next, u_int32_t period);


    void        (*ath_tsf_timer_stop)(ath_dev_t dev, struct ath_gen_timer *timer);
#endif	//ifdef ATH_GEN_TIMER

#if ATH_SUPPORT_AOW
    /**
     * @brief       callback to start the AoW software timer, this starts the timer
     *              function that triggers the consuming of received audio packets
     * @param[in]   dev     : handle to LMAC device
     * @param[in]   tsf     : 32bit TSF value
     * @param[in]   period  : Timer interval
     */

    void        (*ath_start_aow_timer)(ath_dev_t dev, u_int32_t tsf, u_int32_t period);

    /**
     * @brief       callback to stop the AoW software timer, this stops the timer
     *              function that triggers the consuming of received audio packets
     * @param[in]   dev : handle to LMAC device
     */

    void        (*ath_stop_aow_timer)(ath_dev_t dev);

    /**
     * @brief       Callback to toggle the GPIO, mainly used for debugging
     * @param[in]   dev     : handle to LMAC device
     * @param[in]   flag    : flag to indicate the toggle state
     */

    void        (*ath_gpio11_toggle_ptr)(ath_dev_t dev, u_int16_t flg);

    /**
     * @brief       Callback to handle the AoW control data, this function 
     *              does two functions, sets the channel/address mapping for
     *              if the received data is command data, else it sends the
     *              data to all the associated nodes
     * @param[in]   dev     : handle to LMAC device
     * @param[in]   id      : index
     * @param[in]   indata  : pointer Input buffer 
     * @param[in]   insize  : Input buffer size
     * @param[in]   outdata : pointer to output buffer
     * @param[in]   outsize : Output buffer size
     * @param[in]   seqno   : AoW Packet sequence number
     * @param[in]   tsf     : 64 bit current TSF value
     * @return      Always return zero
     */

    int         (*ath_aow_control)(ath_dev_t dev, u_int id,
                           void *indata, u_int insize,
                           void *outdata, u_int *outsize, u_int32_t seqno, u_int64_t tsf);


    /**
    * @brief       Get the current sequence number
    * @param[in]   dev  :  handle to LMAC device
    * @return      current sequence number
    */

    u_int32_t   (*ath_get_aow_seqno)(ath_dev_t dev);
    /**
     * @brief       Start the AoW Proc timer. This starts the timer function that
     *              triggers processing of received audio data
     * @param[in]   dev : handle to LMAC device
     */

    void        (*ath_aow_proc_timer_start)( ath_dev_t dev);

    /**
     * @brief       Stop the AoW Proc timer, this stops the timer function that
     *              triggers processing of received audio data
     * @param[in]   dev : handle to LMAC device
     */

    void        (*ath_aow_proc_timer_stop)( ath_dev_t dev);
#endif  /* ATH_SUPPORT_AOW */

#ifdef ATH_RFKILL
    void        (*enableRFKillPolling)(ath_dev_t dev, int enable);
#endif	//ifdef ATH_RFKILL
#if ATH_SLOW_ANT_DIV
    void        (*ath_antenna_diversity_suspend)(ath_dev_t dev);
    void        (*ath_antenna_diversity_resume)(ath_dev_t dev);
#endif  //if ATH_SLOW_ANT_DIV

    int         (*ath_reg_notify_tx_bcn)(ath_dev_t dev,
                                         ath_notify_tx_beacon_function callback,
                                         void *arg);
    int         (*ath_dereg_notify_tx_bcn)(ath_dev_t dev);
    int         (*ath_reg_notify_vap_info)(ath_dev_t dev,
                                             int vap_if_id,
                                             ath_vap_infotype infotype_mask,
                                             ath_vap_info_notify_func callback,
                                             void *arg);
    int         (*ath_vap_info_update_notify)(ath_dev_t dev,
                                              int vap_if_id,
                                              ath_vap_infotype infotype_mask);
    int         (*ath_dereg_notify_vap_info)(ath_dev_t dev, int vap_if_id);
    int         (*ath_vap_info_get)(ath_dev_t dev, int vap_if_id, ath_vap_infotype type,
                                   u_int32_t *param1, u_int32_t *param2);
    void        (*ath_vap_pause_control)(ath_dev_t dev,int if_id, bool pause );
    void        (*ath_node_pause_control)(ath_dev_t dev,ath_node_t node, bool pause);
    u_int32_t   (*ath_get_goodput) ( ath_dev_t dev, ath_node_t node);
#ifdef  ATH_SUPPORT_TxBF
    int         (*get_txbf_caps)(ath_dev_t dev, ieee80211_txbf_caps_t **txbf_caps);
#ifdef TXBF_TODO
	void		(*rx_get_pos2_data)(ath_dev_t, u_int8_t **p_data, u_int16_t* p_len, 
                    void **rx_status);
	bool		(*rx_txbfrcupdate)(ath_dev_t dev, void *rx_status, u_int8_t *local_h,
                    u_int8_t *CSIFrame, u_int8_t NESSA, u_int8_t NESSB, int BW); 
	void		(*ap_save_join_mac)(ath_dev_t dev, u_int8_t *join_macaddr);
	void        (*start_imbf_cal)(ath_dev_t dev);
#endif
	HAL_BOOL   (*txbf_alloc_key)(ath_dev_t dev, u_int8_t *mac, u_int16_t *keyix);
	void        (*txbf_set_key)(ath_dev_t dev, u_int16_t keyidx, u_int8_t rx_staggered_sounding, 
                    u_int8_t channel_estimation_cap , u_int8_t MMSS);
    void        (*txbf_set_hw_cvtimeout)(ath_dev_t dev, HAL_BOOL opt);
    void        (*txbf_print_cv_cache)(ath_dev_t dev);
    void        (*txbf_set_rpt_received)(ath_dev_t dev);
#endif
    void        (*set_rifs)(ath_dev_t dev, bool enable);    
#if defined(MAGPIE_HIF_GMAC) || defined(MAGPIE_HIF_USB)
    u_int32_t   (*ath_wmi_node_getrate)(ath_dev_t, void *, int);
#endif
#if UMAC_SUPPORT_VI_DBG
    void         (*ath_set_vi_dbg_restart)(ath_dev_t dev);
    void         (*ath_set_vi_dbg_log)(ath_dev_t dev, bool enable);
#endif
#if ATH_SUPPORT_FLOWMAC_MODULE
    void        (*netif_stop_queue)(ath_dev_t);
    void        (*netif_wake_queue)(ath_dev_t);
    void        (*netif_set_watchdog_timeout)(ath_dev_t, int);
    void        (*flowmac_pause)(ath_dev_t, int stall );
#endif
#if ATH_SUPPORT_IBSS_DFS
    void        (*ath_ibss_beacon_update_start)(ath_dev_t dev);
    void        (*ath_ibss_beacon_update_stop)(ath_dev_t dev);
#endif /* ATH_SUPPORT_IBSS_DFS */
    u_int16_t    (*ath_get_tid_seqno)(ath_dev_t dev, ath_node_t node,
                                      u_int8_t tidno);
    void        (*ath_set_bssid_mask)(ath_dev_t dev, const u_int8_t bssid_mask[IEEE80211_ADDR_LEN]);
    
#if UMAC_SUPPORT_SMARTANTENNA
    /*
     * Get valid rates from rate conrol module for a partcular node
     * at the time of association; this is used by smart antenna module for training
     */
   void         (*smartant_prepare_rateset)(ath_dev_t, ath_node_t node ,void *prtset);

   /*
    * Total free buffer avaliable in common pool of buffers
    */ 
   u_int32_t    (*get_txbuf_free)(ath_dev_t);

   /*
    * Number of used buffer used per queue
    */ 
   u_int32_t    (*get_txbuf_used)(ath_dev_t, int ac);

   /*
    * Smart Antenna enable staus to upper layers
    */
   int8_t       (*get_smartantenna_enable) (ath_dev_t);
   void         (*get_smartant_ratestats) (ath_node_t node, void *rate_stats);
#endif    
#if UMAC_SUPPORT_INTERNALANTENNA   
   /*
    * Set selected antenna combination for rateset 
    */
   void         (*set_selected_smantennas) (ath_node_t node, int antenna, int rateset);
#endif   
};

/* Load-time configuration for ATH device */
struct ath_reg_parm {
    u_int16_t            busConfig;                       /* PCI target retry and TRDY timeout */
    u_int8_t             networkAddress[6];               /* Number of map registers for DMA mapping */
    u_int16_t            calibrationTime;                 /* how often to do calibrations, in seconds (0=off) */
    u_int8_t             gpioPinFuncs[NUM_GPIO_FUNCS];    /* GPIO pin for each associated function */
    u_int8_t             gpioActHiFuncs[NUM_GPIO_FUNCS];  /* Set gpioPinFunc0 active high */
    u_int8_t             WiFiLEDEnable;                   /* Toggle WIFI recommended LED mode controlled by GPIO */
    u_int8_t             softLEDEnable;                   /* Enable AR5213 software LED control to work in WIFI LED mode */
    u_int8_t             swapDefaultLED;                  /* Enable default LED swap */
    u_int16_t            linkLedFunc;                     /* Led registry entry for setting the Link Led operation */
    u_int16_t            activityLedFunc;                 /* Led registry entry for setting the Activity Led operation */
    u_int16_t            connectionLedFunc;               /* Led registry entry for setting the Connection Led operation */
    u_int8_t             gpioLedCustom;                   /* Defines customer-specific blinking requirements (OWL) */
    u_int8_t             linkLEDDefaultOn;                /* State of link LED while not connected */
    u_int8_t             DisableLED01;                    /* LED_0 or LED_1 in PCICFG register is used for other purposes */
    u_int8_t             sharedGpioFunc[NUM_GPIO_FUNCS];  /* gpio pins may be shared with other devices */
    u_int16_t            diversityControl;                /* Enable/disable antenna diversity */
    u_int32_t            hwTxRetries;                     /* num of times hw retries the frame */
    u_int16_t            tpScale;                         /* Scaling factor to be applied to max transmit power */
    u_int8_t             extendedChanMode;                /* Extended Channel Mode flag */
    u_int16_t            overRideTxPower;                 /* Override of transmit power */
    u_int8_t             enableFCC3;                      /* TRUE only if card has been FCC3-certified */
    u_int32_t            DmaStopWaitTime;                 /* Overrides default dma wait time */
    u_int8_t             pciDetectEnable;                 /* Chipenabled. */
    u_int8_t             singleWriteKC;                   /* write key cache one word at time */
    u_int32_t            smbiosEnable;                    /* Bit 1 - enable device ID check, Bit 2 - enable smbios check */
    u_int8_t             userCoverageClass;               /* User defined coverage class */
    u_int16_t            pciCacheLineSize;                /* User cache line size setting */
    u_int16_t            pciCmdRegister;                  /* User command register setting */
    u_int32_t            regdmn;                          /* Regulatory domain code for private test */
    char                 countryName[4];                  /* country name */
    u_int32_t            wModeSelect;                     /* Software limiting of device capability for SKU reduction */
    u_int32_t            NetBand;                         /* User's choice of a, b, turbo, g, etc. from registry */
    u_int8_t             pcieDisableAspmOnRfWake;         /* Only use ASPM when RF Silenced */
    u_int8_t             pcieAspm;                        /* ASPM bit settings */
    u_int8_t             txAggrEnable;                    /* Enable Tx aggregation */
    u_int8_t             rxAggrEnable;                    /* Enable Rx aggregation */
    u_int8_t             txAmsduEnable;                   /* Enable Tx AMSDU */
    int                  aggrLimit;                       /* A-MPDU length limit */
    int                  aggrSubframes;                   /* A-MPDU subframe limit */
    u_int8_t             aggrProtEnable;                  /* Enable aggregation protection */
    u_int32_t            aggrProtDuration;                /* Aggregation protection duration */
    u_int32_t            aggrProtMax;                     /* Aggregation protection threshold */
    u_int8_t             txRifsEnable;                    /* Enable Tx RIFS */
    int                  rifsAggrDiv;                     /* RIFS aggregation divisor */
#if ATH_SUPPORT_SPECTRAL
    u_int8_t             spectralEnable;
#endif
#ifdef ATH_RB
    int                  rxRifsEnable;                    /* Enable Rx RIFS */
#define ATH_RB_DEF_TIMEOUT      7000
#define ATH_RB_DEF_SKIP_THRESH  5
    u_int16_t            rxRifsTimeout;                   /* Rx RIFS timeout */
    u_int8_t             rxRifsSkipThresh;                /* Rx RIFS skip thresh */
#endif
    u_int8_t             txChainMask;                     /* Tx ChainMask */
    u_int8_t             rxChainMask;                     /* Rx ChainMask */
    u_int8_t             txChainMaskLegacy;               /* Tx ChainMask in legacy mode */
    u_int8_t             rxChainMaskLegacy;               /* Rx ChainMask in legacy mode */
    u_int8_t             rxChainDetect;                   /* Rx chain detection for Lenovo */
    u_int8_t             rxChainDetectRef;                /* Rx chain detection reference RSSI */
    u_int8_t             rxChainDetectThreshA;            /* Rx chain detection RSSI threshold in 5GHz */
    u_int8_t             rxChainDetectThreshG;            /* Rx chain detection RSSI threshold in 2GHz */
    u_int8_t             rxChainDetectDeltaA;             /* Rx chain detection RSSI delta in 5GHz */
    u_int8_t             rxChainDetectDeltaG;             /* Rx chain detection RSSI delta in 2GHz */
#if ATH_WOW    
    u_int8_t             wowEnable;
#endif    
    u_int32_t            shMemAllocRetry;                 /* Shared memory allocation no of retries */
    u_int8_t             forcePpmEnable;                  /* Force PPM */
    u_int16_t            forcePpmUpdateTimeout;           /* Force PPM window update interval in ms. */
    u_int8_t             enable2GHzHt40Cap;               /* Enable HT40 in 2GHz channels */
    u_int8_t             swBeaconProcess;                 /* Process received beacons in SW (vs HW) */
#ifdef ATH_RFKILL
    u_int8_t             disableRFKill;                   /* Disable RFKill */
#endif
    u_int8_t             rfKillDelayDetect;               /* Enable WAR for slow rise for RfKill on power resume */
#ifdef ATH_SWRETRY
    u_int8_t             numSwRetries;                    /* Num of sw retries to be done */
#endif
    u_int8_t             slowAntDivEnable;                /* Enable slow antenna diversity */
    int8_t               slowAntDivThreshold;             /* Rssi threshold for slow antenna diversity trigger */
    u_int32_t            slowAntDivMinDwellTime;          /* Minimum dwell time on the best antenna configuration */
    u_int32_t            slowAntDivSettleTime;            /* Time spent on probing antenna configurations */
    u_int8_t             stbcEnable;                      /* Enable STBC */
    u_int8_t             ldpcEnable;                      /* Enable LDPC */
#ifdef ATH_BT_COEX
    struct ath_bt_registry bt_reg;
#endif
    u_int8_t             cwmEnable;                       /* Enable Channel Width Management (CWM) */
    u_int8_t             wpsButtonGpio;                   /* GPIO associated with Push Button */
    u_int8_t             pciForceWake;                    /* PCI Force Wake */
    u_int8_t             paprdEnable;                    /* Enable PAPRD feature */
#ifdef ATH_C3_WAR
    u_int32_t            c3WarTimerPeriod;
#endif
    u_int8_t             cddSupport;                      /* CDD - Cyclic Delay Diversity : Use Single chain for Ch 36/40/44/48 */
#ifdef ATH_SUPPORT_TxBF
    u_int32_t            TxBFSwCvTimeout;
#endif
#if ATH_TX_BUF_FLOW_CNTL
    u_int                ACBKMinfree;
    u_int                ACBEMinfree;
    u_int                ACVIMinfree;
    u_int                ACVOMinfree;
    u_int                CABMinfree;
    u_int                UAPSDMinfree;
#endif
#if ATH_SUPPORT_FLOWMAC_MODULE
    u_int8_t             osnetif_flowcntrl:1;               /* enable flowcontrol with os netif */
    u_int8_t             os_ethflowmac_enable:1;            /* enable interaction with flowmac module that forces PAUSE/RESUME on Ethernet */
    u_int32_t            os_flowmac_debug;
#endif
    u_int8_t             burst_beacons:1;                /* burst or staggered beacons for multiple vaps */
#if ATH_SUPPORT_AGGR_BURST
    u_int8_t             burstEnable;
    u_int32_t            burstDur;
#endif
};

/*
struct ath_vow_stats {
    u_int32_t   tx_frame_count;
    u_int32_t   rx_frame_count;
    u_int32_t   rx_clear_count;
    u_int32_t   cycle_count;
    u_int32_t   ext_cycle_count;
} HAL_VOWSTATS;
*/

/**
 * @brief Create and attach an ath_dev object
 */
int ath_dev_attach(u_int16_t devid, void *base_addr,
                   ieee80211_handle_t ieee, struct ieee80211_ops *ieee_ops,
                   osdev_t osdev,
                   ath_dev_t *dev, struct ath_ops **ops,
                   asf_amem_instance_handle amem_handle,
                   struct ath_reg_parm *ath_conf_parm,
                   struct hal_reg_parm *hal_conf_parm);

/**
 * @brief Free an ath_dev object
 */
void ath_dev_free(ath_dev_t);

/**
 * @}
 */

#ifdef DEBUG
#define ATH_TRACE   printk("%s[%d]\n", __func__, __LINE__);
#else
#define ATH_TRACE 
#endif /* DEBUG */

#endif /* ATH_DEV_H */
