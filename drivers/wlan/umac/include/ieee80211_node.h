/*
 *  Copyright (c) 2008 Atheros Communications Inc.
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

#ifndef _ATH_STA_IEEE80211_NODE_H
#define _ATH_STA_IEEE80211_NODE_H

#include <osdep.h>
#include <sys/queue.h>


#include "ieee80211_scan.h"
#include <ieee80211_smartantenna.h>
//#define IEEE80211_DEBUG_REFCNT
//#define IEEE80211_DEBUG_NODELEAK

/* Forward declarations */
struct ieee80211com;
struct ieee80211vap;
struct ieee80211_node_table;
struct ieee80211_rsnparms;

#ifndef ATH_USB
typedef rwlock_t   ieee80211_node_lock_t;
typedef spinlock_t ieee80211_node_state_lock_t;

#define IEEE80211_NODE_STATE_LOCK_INIT(_node)     spin_lock_init(&(_node)->ni_state_lock);
#define IEEE80211_NODE_STATE_LOCK_DESTROY(_node)  spin_lock_destroy(&(_node)->ni_state_lock)
#define IEEE80211_NODE_STATE_LOCK(_node)          spin_lock(&(_node)->ni_state_lock)
#define IEEE80211_NODE_STATE_UNLOCK(_node)        spin_unlock(&(_node)->ni_state_lock)
#else
typedef usb_readwrite_lock_t ieee80211_node_lock_t;
typedef usblock_t            ieee80211_node_state_lock_t;

#define IEEE80211_NODE_STATE_LOCK_INIT(_node)     OS_USB_LOCK_INIT(&(_node)->ni_state_lock)
#define IEEE80211_NODE_STATE_LOCK_DESTROY(_node)  OS_USB_LOCK_DESTROY(&(_node)->ni_state_lock)
#define IEEE80211_NODE_STATE_LOCK(_node)          OS_USB_LOCK(&(_node)->ni_state_lock)
#define IEEE80211_NODE_STATE_UNLOCK(_node)        OS_USB_UNLOCK(&(_node)->ni_state_lock)
#endif

#define WDS_AGING_COUNT 2
#define WDS_AGING_TIME 60000   /* in ms */
#define WDS_AGING_TIMER_VAL (WDS_AGING_TIME/2)

#if UMAC_SUPPORT_INTERNALANTENNA
struct per_ratetable {
    u_int32_t nframes;
    u_int32_t nbad;
    int32_t chain0_rssi;
    int32_t chain1_rssi;
    int32_t chain2_rssi;
};

struct ratetoindex {     /* Valid Rate Index table from RC module */
    u_int8_t ratecode;
    u_int8_t rateindex;
};

struct smartant_rateset {
    u_int8_t num_of_rates;
    struct ratetoindex rates[MAX_RATES_PERSET];  /* 7 rates per set*/
    u_int8_t selected_antenna;
};

struct smartantenna_state {
    int8_t rateset;
    u_int8_t rateidx;
    u_int8_t antenna;
    u_int8_t iteration;
    u_int32_t pending_packets; /* no of pending packets to send for training which are not sent previously*/
};

struct node_smartant_per {
    u_int8_t antenna;
    u_int8_t ratecode;
    u_int32_t nFrames;
    u_int32_t nBad;
    int8_t chain0_rssi;
    int8_t chain1_rssi;
    int8_t chain2_rssi;
};
#endif



/*
 * Node information. A node could represents a BSS in infrastructure network,
 * or an ad-hoc station in IBSS mode, or an associated station in HOSTAP mode.
 */
typedef struct ieee80211_node {
    TAILQ_ENTRY(ieee80211_node) ni_list; /* node table list */
    LIST_ENTRY(ieee80211_node)  ni_hash; /* hash list */
#ifdef IEEE80211_DEBUG_NODELEAK
    TAILQ_ENTRY(ieee80211_node) ni_alloc_list; /* all allocated nodes */
#endif

#ifdef IEEE80211_DEBUG_REFCNT
#define NUM_TRACE_BUF           (1 << 5)
    const char  *func[NUM_TRACE_BUF];
    int         line[NUM_TRACE_BUF];
    atomic_t    refcnt[NUM_TRACE_BUF];
    atomic_t    index;
#endif

    struct ieee80211_node_table *ni_table;
    struct ieee80211vap     *ni_vap;
    struct ieee80211com     *ni_ic;
    struct ieee80211_node   *ni_bss_node;
    ieee80211_node_state_lock_t    ni_state_lock;   /* Node for the WAR for bug 58187 */
    atomic_t                ni_refcnt;
    u_int8_t                ni_authmode;            /* authentication algorithm */
    u_int32_t               ni_flags;               /* special-purpose state */
#define IEEE80211_NODE_AUTH     0x00000001          /* authorized for data */
#define IEEE80211_NODE_QOS      0x00000002          /* QoS enabled */
#define IEEE80211_NODE_ERP      0x00000004          /* ERP enabled */
#define IEEE80211_NODE_HT       0x00000008          /* HT enabled */
/* NB: this must have the same value as IEEE80211_FC1_PWR_MGT */
#define IEEE80211_NODE_PWR_MGT  0x00000010          /* power save mode enabled */
#define IEEE80211_NODE_UAPSD    0x00000040          /* U-APSD power save enabled */
#define IEEE80211_NODE_UAPSD_TRIG 0x00000080        /* U-APSD triggerable state */
#define IEEE80211_NODE_UAPSD_SP 0x00000100          /* U-APSD SP in progress */
#define IEEE80211_NODE_ATH      0x00000200          /* Atheros Owl or follow-on device */
#define IEEE80211_NODE_OWL_WDSWAR 0x00000400        /* Owl WDS workaround needed*/
#define IEEE80211_NODE_WDS      0x00000800          /* WDS link */
#define	IEEE80211_NODE_NOAMPDU  0x00001000          /* No AMPDU support */
#define IEEE80211_NODE_WEPTKIPAGGR 0x00002000       /* Atheros proprietary wep/tkip aggregation support */
#define IEEE80211_NODE_WEPTKIP  0x00004000
#define IEEE80211_NODE_TEMP     0x00008000          /* temp node (not in the node table) */
#define IEEE80211_NODE_40_INTOLERANT    0x00020000  /* HT40 Intolerant  */
#define IEEE80211_NODE_PAUSED   0x00040000          /* node is  paused*/
#define IEEE80211_NODE_EXTRADELIMWAR 0x00080000
#define IEEE80211_NODE_NAWDS 0x00100000          /* node is an NAWDS repeater */
#define IEEE80211_NODE_TDLS     0x00200000          /* TDLS node */
#define IEEE80211_NODE_REQ_HT20     0x00400000      /* HT20 requesting node */
#define IEEE80211_NODE_ATH_PAUSED 0x00800000         /* all the tid queues in ath layer are paused*/ 
#define IEEE80211_NODE_UAPSD_CREDIT_UPDATE 0x01000000  /*Require credit update*/
#define IEEE80211_NODE_KICK_OUT_DEAUTH 0x02000000  /*Require send deauth when h/w queue no data*/
   
    u_int8_t                ni_ath_flags;       /* Atheros feature flags */
/* NB: These must have the same values as IEEE80211_ATHC_* */
#define IEEE80211_NODE_TURBOP   0x01          /* Turbo prime enable */
#define IEEE80211_NODE_COMP     0x02          /* Compresssion enable */
#define IEEE80211_NODE_FF       0x04          /* Fast Frame capable */
#define IEEE80211_NODE_XR       0x08          /* Atheros WME enable */
#define IEEE80211_NODE_AR       0x10          /* AR capable */
#define IEEE80211_NODE_BOOST    0x80 

    u_int16_t               ni_ath_defkeyindex; /* Atheros def key index */
#define IEEE80211_INVAL_DEFKEY  0x7FFF

    u_int16_t               ni_associd; /* association id */
    u_int32_t               ni_scangen;
    systick_t               ni_assocuptime;  /* association up time */
    systick_t               ni_assocstarttime;   /* association start time */
    systick_t               ni_assoctime;        /* association process time */
    u_int16_t               ni_assocstatus;  /* association status code */
    u_int16_t               ni_txpower; /* current transmit power */

    u_int16_t               ni_vlan;    /* vlan tag */
    u_int32_t               *ni_challenge;  /* shared-key challenge */
    u_int8_t                *ni_wpa_ie;     /* captured WPA/RSN ie */
    u_int8_t                *ni_wps_ie;     /* captured WSC ie */
    u_int8_t                 ni_cc[3];      /* captured country code */

    u_int8_t                *ni_ath_ie; /* captured Atheros ie */
    u_int8_t                *ni_wme_ie; /* captured WME ie */
    u_int16_t               ni_txseqs[IEEE80211_TID_SIZE];      /* tx seq per-tid */
    u_int16_t               ni_rxseqs[IEEE80211_TID_SIZE+1];/* rx seq previous per-tid,
                                                             * the additional one is for check seq on
                                                             * management frames. */
    u_int16_t               ni_last_rxseqs[IEEE80211_TID_SIZE+1];/* rx seq 2nd last(previous) per-tid,
                                                                  * the additional one is for check seq on
                                                                  * management frames. */
    systime_t               ni_rxfragstamp; /* timestamp of last rx frag */
    atomic_t                ni_rxfrag_lock; /* indicate timer handler is currently executing */
    wbuf_t                  ni_rxfrag[1];  /* rx frag reassembly.
                                             * XXX ???: do we have to have a reassembly line
                                             * for each TID */
    
    struct ieee80211_rsnparms ni_rsn;       /* RSN/WPA parameters */
    struct ieee80211_key    ni_ucastkey;    /* unicast key */
    struct {   // for adhoc mcast rx
        struct ieee80211_key nips_hwkey; // allocated clear key used to hand frame to sw
        struct ieee80211_key nips_swkey[IEEE80211_WEP_NKID]; // key used by sw to decrypt
    } ni_persta;
    int                     ni_rxkeyoff;    /* Receive key offset */
#if ATH_SUPPORT_WEP_MBSSID
    struct ieee80211_wep_mbssid ni_wep_mbssid;
#endif /*ATH_SUPPORT_WEP_MBSSID*/

    /*support for WAPI: keys for WAPI*/
#if ATH_SUPPORT_WAPI
	int ni_wkused;
	u32 ni_wapi_rekey_pkthresh;  /*wapi packets threshold for rekey, unicast or multicast depending on node*/
#endif

    /* 11n Capabilities */
    u_int16_t               ni_htcap;              /* HT capabilities */
    u_int16_t               ni_maxampdu;           /* maximum rx A-MPDU length */
    u_int32_t               ni_mpdudensity;        /* MPDU density in nano-sec */

#ifdef ATH_SUPPORT_TxBF
    union ieee80211_hc_txbf ni_txbf;               /* txbf capabilities */
#endif

    u_int8_t                ni_obssnonhtpresent;   /* OBSS non-HT STA present */
    u_int8_t                ni_txburstlimit;       /* TX burst limit */
    u_int8_t                ni_nongfpresent;       /* Non-green field devices present */
    u_int8_t                ni_streams;            /* number of streams supported */

    /* 11n information */
    enum ieee80211_cwm_width ni_chwidth;        /* recommended tx channel width */
    u_int8_t                ni_newchwidth;      /* channel width changed */
    int8_t                  ni_extoffset;       /* recommended ext channel offset */
    u_int8_t                ni_updaterates;     /* update rate table on SM power save */
#define	IEEE80211_NODE_SM_EN                    1
#define	IEEE80211_NODE_SM_PWRSAV_STAT	        2
#define	IEEE80211_NODE_SM_PWRSAV_DYN	        4
#define	IEEE80211_NODE_RATECHG                  8

    /* activity indicators */
    systime_t               ni_beacon_rstamp; /* host timestamp of received beacon and probes */
    u_int32_t               ni_probe_ticks;   /* inactivity mark count */

    u_int8_t		        ni_weptkipaggr_rxdelim; /* number of delimeters required to receive wep/tkip w/aggr */

    /* in activity indicators for AP mode */
    u_int16_t               ni_inact;       /* inactivity mark count */
    u_int16_t               ni_inact_reload;/* inactivity reload value */

#define ATH_TX_MAX_CONSECUTIVE_XRETRIES     50 /* sta gets kicked out after this */
    /* kick out STA when excessive retries occur */
    u_int16_t               ni_consecutive_xretries;

    /* hardware, not just beacon and probes */
    u_int8_t                ni_rssi;    /* recv ssi */

    u_int8_t                ni_macaddr[IEEE80211_ADDR_LEN]; /* MAC address */
    u_int8_t                ni_bssid[IEEE80211_ADDR_LEN]; /* BSSID */

    /* beacon, probe response */
    union {
        u_int8_t            data[8];
        u_int64_t           tsf;
    } ni_tstamp;                        /* from last rcv'd beacon */

    u_int16_t               ni_intval;  /* beacon interval */
    u_int16_t               ni_capinfo; /* negociated capabilities */
    
    u_int16_t               ni_ext_caps;/* exteneded node capabilities */
#define IEEE80211_NODE_C_QOS    0x0002  /* Wmm capable */
#define IEEE80211_NODE_C_UAPSD  0x0004  /* U-APSD capable */  

    u_int8_t                ni_esslen;
    u_int8_t                ni_essid[IEEE80211_NWID_LEN+1];

    struct ieee80211_rateset ni_rates;   /* negotiated rate set */
    struct ieee80211_rateset ni_htrates; /* negotiated ht rate set */
    struct ieee80211_channel *ni_chan;

    u_int8_t                ni_erp;     /* ERP from beacon/probe resp */

    u_int32_t               ni_wait0_ticks;       /* ticks that we stay in dot11_assoc_state_zero state */
    u_int8_t                ni_dtim_period;       /* DTIM period */
    u_int8_t                ni_dtim_count;        /* DTIM count for last bcn */
    u_int16_t               ni_lintval;           /* listen interval */
    u_int8_t                ni_minbasicrate;      /* Min basic rate */

    systime_t               ss_last_data_time;    /* last time data RX/TX time */ 

    u_int16_t               ni_pause_count;
    /* power save queues */
    IEEE80211_NODE_POWERSAVE_QUEUE(ni_dataq)
    IEEE80211_NODE_POWERSAVE_QUEUE(ni_mgmtq)

    /* AP side UAPSD */
    u_int8_t                ni_uapsd;	/* U-APSD per-node flags matching WMM STA Qos Info field */
    u_int8_t                ni_uapsd_maxsp; /* maxsp from flags above */
    u_int16_t               ni_uapsd_trigseq[WME_NUM_AC]; /* trigger suppression on retry */
    u_int8_t                ni_uapsd_ac_trigena[WME_NUM_AC];    /* U-APSD per-node flags matching WMM STA Qos Info field */
    u_int8_t                ni_uapsd_ac_delivena[WME_NUM_AC];    /* U-APSD per-node flags matching WMM STA Qos Info field */

    /* node statistics */
    struct ieee80211_nodestats   ni_stats; 

    u_int8_t                ni_assoc_state; /* IBSS only */
#define IEEE80211_NODE_ADHOC_STATE_ZERO             0
#define IEEE80211_NODE_ADHOC_STATE_UNAUTH_UNASSOC   1
#define IEEE80211_NODE_ADHOC_STATE_AUTH_ASSOC       2

    u_int8_t                ni_chanswitch_tbtt;
#ifdef ATH_SUPPORT_TxBF    
    /* beam forming flag */
    u_int8_t                ni_bf_update_cv        : 1, /* 1: request CV update */                 
                            ni_explicit_noncompbf  : 1, /* 1: set explicit non-compressed bf */    
                            ni_explicit_compbf     : 1, /* 1: set explicit compressed bf*/         
                            ni_implicit_bf         : 1, /* 1: set implicit bf */                   
                            Calibration_Done       : 1,     
                            ni_rpt_received        : 1, /* 1: V/CV report recieved */    
                            ni_txbf_timer_initialized   : 1,    /* 1: txbf related timer initialized */
                            ni_hw_cv_requested     : 1,     /* 1: cv requested by incorrect HW status*/
                            ni_allow_cv_update     : 1;     /* 1: sw time out , allow cv request */    

    u_int8_t                ni_mmss;
    u_int32_t               ni_sw_cv_timeout;
    os_timer_t              ni_cv_timer;
    os_timer_t              ni_report_timer;
    u_int32_t               ni_cvtstamp;
    u_int8_t                ni_cvretry;
#endif

    struct ieee80211vap     *ni_wdsvap;     /* associated WDS vap */
#if ATH_SUPPORT_IQUE
    u_int8_t    ni_hbr_block;
    u_int32_t   ni_ique_flag;    
#endif
#if UMAC_SUPPORT_TDLS
    struct ieee80211_tdls_node *ni_tdls;
#endif /* UMAC_SUPPORT_TDLS */
#ifdef ATH_SUPPORT_P2P
    u_int8_t    ni_p2p_awared;
#endif
#if UMAC_SUPPORT_SMARTANTENNA
    struct per_ratetable permap[MAX_SMART_ANTENNAS][MAX_HT_RATES];
    u_int8_t maxrtsets;
    u_int8_t  current_tx_antenna;
    u_int8_t  default_rx_antenna;
    u_int8_t  reciever_rx_antenna;
    u_int8_t  current_rate_index;
    u_int8_t  smartantenna_state;
    u_int8_t  iteration;
    u_int8_t  is_training;
    u_int8_t  train_type; /* frame based or protocol based */
    u_int32_t pending_pkts; 
    u_int32_t ns_prev_tx_ucast; /* tx unicast packets trasmited in retraining intervel */
    u_int8_t  retrain_miss;  /* Number of consecutive retrain intervels with out doing retraining*/
    u_int8_t  smartant_prev_ratemax;
#endif    
#if UMAC_SUPPORT_INTERNALANTENNA    
    struct smartant_rateset rtset[SMARTANT_MAX_RATESETS];
#endif
#if UMAC_SUPPORT_SMARTANTENNA
    struct smartantenna_state train_state; /* current training sate info */
    TAILQ_ENTRY(ieee80211_node) smartant_nodelist; /* smart antenna node table list */
#endif
#ifdef ATH_SUPPORT_IBSS_NETLINK_NOTIFICATION
    u_int8_t	ni_rssi_class;
#endif

} IEEE80211_NODE, *PIEEE80211_NODE;


struct ieee80211_wds_addr {
	LIST_ENTRY(ieee80211_wds_addr)    wds_hash;
	u_int8_t    wds_macaddr[IEEE80211_ADDR_LEN];
	struct ieee80211_node    *wds_ni;
    /* ni_macaddr can be accessed from ni pointer. In case of quick
     * disconnect and connect, wds entry for this would move from active to
     * staged state.
     * When in staged, this should not be refering to stale
     * node pointer. So cache the mac address alone, so that, we can use
     * this information to figure out the actual node pointer. Also new hash
     */
    u_int8_t    wds_ni_macaddr[IEEE80211_ADDR_LEN];

#define IEEE80211_NODE_F_WDS_BEHIND   0x00001
#define IEEE80211_NODE_F_WDS_REMOTE   0x00002
#define IEEE80211_NODE_F_WDS_STAGE    0x00004
/* Set for first time, when node is added (for TDLS)*/
#define IEEE80211_NODE_F_WDS_START    0x00010 
    u_int32_t   flags;
	u_int16_t   wds_agingcount;
    u_int16_t   wds_staging_age;
    systime_t   wds_last_pkt_time;
};

#ifdef IEEE80211_DEBUG_REFCNT
#define TRACENODE(_ni, _func, _line) do {                             \
    atomic_t index = atomic_inc(&(_ni)->index) & (NUM_TRACE_BUF - 1); \
    (_ni)->func[index] = _func;                                       \
    (_ni)->line[index] = _line;                                       \
    (_ni)->refcnt[index] = ieee80211_node_refcnt(_ni);                \
} while (0)
#endif

#define IEEE80211_RSSI_RX       0x00000001
#define IEEE80211_RSSI_TX       0x00000002
#define IEEE80211_RSSI_EXTCHAN  0x00000004
#define IEEE80211_RSSI_BEACON   0x00000008
#define IEEE80211_RSSI_RXDATA   0x00000010

#define IEEE80211_RATE_TX 0
#define IEEE80211_RATE_RX 1
#define IEEE80211_LASTRATE_TX 2
#define IEEE80211_LASTRATE_RX 3
#define IEEE80211_RATECODE_TX 4

#if UMAC_SUPPORT_P2P
#define WME_UAPSD_NODE_MAXQDEPTH   100 
#else
#define WME_UAPSD_NODE_MAXQDEPTH   8
#endif

#define WME_UAPSD_AC_CAN_TRIGGER(_ac, _ni)  \
                (((_ni)->ni_flags & IEEE80211_NODE_UAPSD_TRIG) && ((_ni)->ni_uapsd_ac_trigena[(_ac)]))
#define WME_UAPSD_AC_ISDELIVERYENABLED(_ac, _ni)  \
        (((_ni)->ni_flags & IEEE80211_NODE_UAPSD_TRIG) && ((_ni)->ni_uapsd_ac_delivena[(_ac)]))
#define IEEE80211_NODE_UAPSD_USETIM(_ni) (((_ni)->ni_uapsd & 0xF) == 0xF )
#define WME_UAPSD_NODE_INVALIDSEQ    0xffff
#define WME_UAPSD_NODE_TRIGSEQINIT(_ni)    (memset(&(_ni)->ni_uapsd_trigseq[0], 0xff, sizeof((_ni)->ni_uapsd_trigseq)))


#define    IEEE80211_NODE_STAT(ni,stat)    (ni->ni_stats.ns_##stat++)
#define    IEEE80211_NODE_STAT_ADD(ni,stat,v)    (ni->ni_stats.ns_##stat += v)
#define    IEEE80211_NODE_STAT_SET(ni,stat,v)    (ni->ni_stats.ns_##stat = v)

/*
 * Table of node instances.
 */

#define	IEEE80211_NODE_HASHSIZE	32

/* simple hash is enough for variation of macaddr */
#define	IEEE80211_NODE_HASH(addr)   \
    (((const u_int8_t *)(addr))[IEEE80211_ADDR_LEN - 1] % IEEE80211_NODE_HASHSIZE)

struct ieee80211_node_table {
    struct ieee80211com             *nt_ic;         /* back reference */
    ieee80211_node_lock_t           nt_nodelock;    /* on node table */
    ieee80211_node_lock_t           nt_wds_nodelock;    /* on node table */
    TAILQ_HEAD(, ieee80211_node)    nt_node;        /* information of all nodes */
#if UMAC_SUPPORT_SMARTANTENNA    
    TAILQ_HEAD(, ieee80211_node)    nt_smartant_node;  /* information of all nodes in smart antena train list */
#endif    
    ATH_LIST_HEAD(, ieee80211_node) nt_hash[IEEE80211_NODE_HASHSIZE];
    ATH_LIST_HEAD(, ieee80211_wds_addr) nt_wds_hash[IEEE80211_NODE_HASHSIZE];
    const char                      *nt_name;
    u_int32_t                       nt_scangen;
    int                             nt_inact_init;  /* initial node inact setting */
    int                             nt_inact_auth;  /* node inact setting in auth */
    int                             nt_inact_run;   /* node inact setting in run*/
    int                             nt_inact_probe; /* node inact setting for probe */
    os_timer_t                      nt_wds_aging_timer;    /* timer to age out wds entries */
};

void ieee80211_node_attach(struct ieee80211com *ic);
void ieee80211_node_detach(struct ieee80211com *ic);
void ieee80211_node_vattach(struct ieee80211vap *vap);
void ieee80211_node_vdetach(struct ieee80211vap *vap);
int ieee80211_node_latevattach(struct ieee80211vap *vap);
void ieee80211_node_latevdetach(struct ieee80211vap *vap);

void ieee80211_copy_bss(struct ieee80211_node *nbss, const struct ieee80211_node *obss);
int ieee80211_reset_bss(struct ieee80211vap *vap);
int ieee80211_sta_join_bss(struct ieee80211_node *selbs);
struct ieee80211_node *
ieee80211_ref_bss_node(struct ieee80211vap *vap);

struct ieee80211_node *
ieee80211_add_neighbor(struct ieee80211_node *ni,
                       ieee80211_scan_entry_t scan_entry);

struct ieee80211_node *
ieee80211_tmp_node(struct ieee80211vap *vap, const u_int8_t *macaddr);

int
ieee80211_dup_ibss(struct ieee80211vap *vap, struct ieee80211_node *org_ibss);

int
ieee80211_sta_join(struct ieee80211vap *vap, ieee80211_scan_entry_t scan_entry);

int
ieee80211_join_ibss(struct ieee80211vap *vap, ieee80211_scan_entry_t scan_entry);

int
ieee80211_ibss_merge(struct ieee80211_node *ni, ieee80211_scan_entry_t scan_entry);

int
ieee80211_create_ibss(struct ieee80211vap *vap,
                      const u_int8_t *bssid,
                      const u_int8_t *essid,
                      const u_int16_t esslen);
int
ieee80211_create_infra_bss(struct ieee80211vap *vap,
                      const u_int8_t *essid,
                           const u_int16_t esslen);

struct ieee80211_node *
ieee80211_alloc_node(struct ieee80211_node_table *nt,
                     struct ieee80211vap *vap,
                     const u_int8_t *macaddr);


void ieee80211_timeout_stations(struct ieee80211_node_table *nt);

/*
 * ieee80211_node_refcnt	reference count for printing (only)
 */
#define	ieee80211_node_refcnt(_ni)	atomic_read(&(_ni)->ni_refcnt)

#ifdef IEEE80211_DEBUG_REFCNT

void ieee80211_free_node_debug(struct ieee80211_node *,
                               const char *func, int line);
struct ieee80211_node *
ieee80211_find_node_debug(struct ieee80211_node_table *nt,
                                                 const u_int8_t *macaddr,
                                                 const char *func, int line);
struct ieee80211_node *
ieee80211_find_txnode_debug(struct ieee80211vap *vap, const u_int8_t *macaddr,
                            const char *func, int line);
struct ieee80211_node *
ieee80211_find_rxnode_debug(struct ieee80211com *ic,
                            const struct ieee80211_frame_min *wh,
                            const char *func, int line);
/* No debug version */
struct ieee80211_node *
ieee80211_find_rxnode_nolock(struct ieee80211com *ic,
                      const struct ieee80211_frame_min *wh);

struct ieee80211_node *
ieee80211_ref_node_debug(struct ieee80211_node *ni,
                         const char *func, int line);
void
ieee80211_unref_node_debug(struct ieee80211_node **ni,
                           const char *func, int line);
bool 
ieee80211_node_leave_debug(struct ieee80211_node *ni,
                           const char *func, int line);
bool 
ieee80211_sta_leave_debug(struct ieee80211_node *ni,
                           const char *func, int line);

#define	ieee80211_free_node(ni) \
    ieee80211_free_node_debug(ni, __func__, __LINE__)
#define ieee80211_find_node(nt, mac)    \
    ieee80211_find_node_debug(nt, mac, __func__, __LINE__)
#define ieee80211_find_txnode(vap, mac) \
    ieee80211_find_txnode_debug(vap, mac, __func__, __LINE__)
#define ieee80211_find_rxnode(ic, wh)   \
    ieee80211_find_rxnode_debug(ic, wh, __func__, __LINE__)

#define	ieee80211_ref_node(ni) \
    ieee80211_ref_node_debug(ni, __func__, __LINE__)
#define	ieee80211_unref_node(ni) \
    ieee80211_unref_node_debug(ni, __func__, __LINE__)

#define ieee80211_node_leave(ni) \
    ieee80211_node_leave_debug(ni, __func__, __LINE__)

#define ieee80211_sta_leave(ni) \
    ieee80211_sta_leave_debug(ni, __func__, __LINE__)

#else  /* !IEEE80211_DEBUG_REFCNT */

void ieee80211_free_node(struct ieee80211_node *ni);
struct ieee80211_node *
ieee80211_find_node(struct ieee80211_node_table *nt,
                    const u_int8_t *macaddr);
struct ieee80211_node *
ieee80211_find_txnode(struct ieee80211vap *vap,
                      const u_int8_t *macaddr);
struct ieee80211_node *
ieee80211_find_rxnode(struct ieee80211com *ic,
                      const struct ieee80211_frame_min *wh);


struct ieee80211_node *
ieee80211_find_rxnode_nolock(struct ieee80211com *ic,
                      const struct ieee80211_frame_min *wh);

/* after adding the new staging flag for the wds, ieee80211_find_wds_node
 * would get called in the context of ieee80211_find_node OR
 * ieee80211_find_txnode. Calling function without '_' would call recursion.
 * To avoid this, _ieee80211_find_node, is made global by removing the
 * static, also moved the definition here. 
 */
struct ieee80211_node *
#ifdef IEEE80211_DEBUG_REFCNT
_ieee80211_find_node_debug(
        struct ieee80211_node_table *nt, const u_int8_t *macaddr,
        const char *func, int line);
#else
_ieee80211_find_node(struct ieee80211_node_table *nt,
                    const u_int8_t *macaddr);
#endif

static INLINE struct ieee80211_node *
ieee80211_ref_node(struct ieee80211_node *ni)
{
    atomic_inc(&(ni->ni_refcnt));
    return ni;
}

static INLINE void
ieee80211_unref_node(struct ieee80211_node **ni)
{
    atomic_dec(&((*ni)->ni_refcnt));
    *ni = NULL;			/* guard against use */
}

bool ieee80211_node_leave(struct ieee80211_node *ni);

bool ieee80211_sta_leave(struct ieee80211_node *ni);

#endif /* IEEE80211_DEBUG_REFCNT */

#ifdef IEEE80211_DEBUG_NODELEAK

void 
ieee80211_dump_alloc_nodes(struct ieee80211com *ic);

#endif


void ieee80211_node_set_chan(struct ieee80211_node *ni);
int ieee80211_node_join(struct ieee80211_node *ni);
struct ieee80211_node *
ieee80211_dup_bss(struct ieee80211vap *vap, const u_int8_t *macaddr);




static INLINE int
ieee80211_node_is_authorized(const struct ieee80211_node *ni)
{
    return (ni->ni_flags & IEEE80211_NODE_AUTH);
}

void ieee80211_node_authorize(struct ieee80211_node *ni);

void ieee80211_node_unauthorize(struct ieee80211_node *ni);

static INLINE void
ieee80211_node_activity(struct ieee80211_node *ni)
{
    ni->ni_inact = ni->ni_inact_reload;
}

#define	IEEE80211_INACT_INIT    (30/IEEE80211_INACT_WAIT)   /* initial */
#define IEEE80211_INACT_AUTH    (180/IEEE80211_INACT_WAIT)    /* associated but not authorized */
#define IEEE80211_INACT_RUN     (300/IEEE80211_INACT_WAIT)    /* authorized */
#define IEEE80211_INACT_PROBE   (90/IEEE80211_INACT_WAIT)    /* probe */


typedef void ieee80211_iter_func(void *, struct ieee80211_node *);
void ieee80211_iterate_node(struct ieee80211com *ic, ieee80211_iter_func *func, void *arg);




/*
 * Accessor methods for node
 */

/* Get the VAP object that this node belongs to */
static INLINE struct ieee80211vap *
ieee80211_node_get_vap(struct ieee80211_node *ni)
{
    return ni->ni_vap;
}

static INLINE u_int16_t
ieee80211_node_get_txpower(struct ieee80211_node *ni)
{
    return ni->ni_txpower;
}

/* Return the beacon interval of associated BSS */
static INLINE u_int16_t
ieee80211_node_get_beacon_interval(struct ieee80211_node *ni)
{
    return ni->ni_intval;
}

static INLINE u_int64_t
ieee80211_node_get_tsf(struct ieee80211_node *ni)
{
    return ni->ni_tstamp.tsf;
}

static INLINE u_int16_t
ieee80211_node_get_associd(struct ieee80211_node *ni)
{
    return ni->ni_associd;
}

static INLINE u_int8_t *
ieee80211_node_get_bssid(struct ieee80211_node *ni)
{
    return ni->ni_bssid;
}

static INLINE u_int8_t *
ieee80211_node_get_macaddr(struct ieee80211_node *ni)
{
    return ni->ni_macaddr;
}

static INLINE u_int16_t
ieee80211_node_get_maxampdu(struct ieee80211_node *ni)
{
    return ni->ni_maxampdu;
}

#ifdef ATH_SWRETRY
static INLINE u_int16_t
ieee80211_node_get_pwrsaveq_len(struct ieee80211_node *ni)
{
    struct node_powersave_queue *dataq,*mgmtq;
    dataq = &ni->ni_dataq;
    mgmtq  = &ni->ni_mgmtq;

    return (dataq->nsq_len + mgmtq->nsq_len);
}
#endif

#define IEEE80211_NODE_CLEAR_HTCAP(_ni)     ((_ni)->ni_htcap = 0)

#define IEEE80211_NODE_USE_HT(_ni)          ((_ni)->ni_flags & IEEE80211_NODE_HT)
#define IEEE80211_NODE_USEAMPDU(_ni)        ieee80211node_use_ampdu(_ni)

static INLINE int ieee80211node_use_ampdu(struct ieee80211_node *ni)
{
    if (IEEE80211_NODE_USE_HT(ni) && 
        !(ni->ni_flags & IEEE80211_NODE_NOAMPDU)) {
        return(1);  /* Supports AMPDU */
    }
    return(0);  /* Do not use AMPDU since non HT */
}

#define IEEE80211_NODE_AC_UAPSD_ENABLED(_ni, _ac) ((_ni)->ni_uapsd_ac_delivena[(_ac)])


/*
 * ************************************
 * IEEE80211_NODE Interfaces
 * ************************************
 */
static INLINE void
ieee80211node_set_flag(struct ieee80211_node *ni, u_int16_t flag)
{
    ni->ni_flags |= flag;
}

static INLINE void
ieee80211node_clear_flag(struct ieee80211_node *ni, u_int16_t flag)
{
    ni->ni_flags &= ~flag;
}

static INLINE int
ieee80211node_has_flag(struct ieee80211_node *ni, u_int16_t flag)
{
    return ((ni->ni_flags & flag) != 0);
}

static INLINE void
ieee80211node_set_athflag(struct ieee80211_node *ni, u_int8_t flag)
{
    ni->ni_ath_flags |= flag;
}

static INLINE void
ieee80211node_clear_athflag(struct ieee80211_node *ni, u_int8_t flag)
{
    ni->ni_ath_flags &= ~flag;
}

static INLINE int
ieee80211node_has_athflag(struct ieee80211_node *ni, u_int8_t flag)
{
    return ((ni->ni_ath_flags & flag) != 0);
}

static INLINE void
ieee80211node_set_txpower(struct ieee80211_node *ni, u_int16_t txpower)
{
    ni->ni_txpower = txpower;
}

static INLINE u_int8_t
ieee80211node_get_rssi(struct ieee80211_node *ni)
{
    return ni->ni_rssi;
}

static INLINE struct ieee80211_rateset *
ieee80211node_get_rateset(struct ieee80211_node *ni)
{
    return &ni->ni_rates;
}

static INLINE u_int8_t *
ieee80211node_get_tstamp(struct ieee80211_node *ni)
{
    return ni->ni_tstamp.data;
}

static INLINE u_int16_t
ieee80211node_get_capinfo(struct ieee80211_node *ni)
{
    return ni->ni_capinfo;
}

static INLINE int
ieee80211node_has_cap(struct ieee80211_node *ni, u_int16_t cap)
{
    return ((ni->ni_capinfo & cap) != 0);
}

static INLINE int
ieee80211node_is_paused(struct ieee80211_node *ni)
{
    return ( (ni->ni_flags & IEEE80211_NODE_PAUSED) != 0);  
}

static INLINE int
ieee80211node_pause(struct ieee80211_node *ni)
{
    u_int16_t pause_count;
    IEEE80211_NODE_STATE_LOCK(ni);    
    pause_count = ++ni->ni_pause_count;
    ni->ni_flags |= IEEE80211_NODE_PAUSED;   
    IEEE80211_NODE_STATE_UNLOCK(ni);
    return pause_count;
}

static INLINE int 
ieee80211node_unpause(struct ieee80211_node *ni)
{
	/*
	** Note:
	** Check NI is valid, just in case
	*/

	if( ni )
	{
        IEEE80211_NODE_STATE_LOCK(ni);

		/*
		** Simply set the pause count to zero.  This should
		** NOT have any detrimental effects
		****** PERMINANT FIX REQUIRED, THIS IS TEMPORARY *****
		*/

		if ( ni->ni_pause_count > 0 )
			ni->ni_pause_count--;
		else
			printk("%s: Pause Count already zero\n",__func__);

		/*
		** You can put a debug message here in case you want
		** notification of an extra unpause.  At this point, we
		** don't want to assert
		*/

    	if (ni->ni_pause_count == 0 && (ni->ni_flags & IEEE80211_NODE_PAUSED))
    	{
    	    ni->ni_flags &= ~IEEE80211_NODE_PAUSED;   
    	    ieee80211_node_saveq_flush(ni);
    	}
        IEEE80211_NODE_STATE_UNLOCK(ni);

    	return ni->ni_pause_count;
	}
	return (0);
}
/*
 * 11n
 */
static INLINE int
ieee80211_node_has_htcap(struct ieee80211_node *ni, u_int16_t htcap)
{
    return ((ni->ni_htcap & htcap) != 0);
}

#if ATH_SUPPORT_IQUE
static INLINE u_int8_t
ieee80211_node_get_hbr_block_state(struct ieee80211_node *ni)
{
    return ni->ni_hbr_block;
}
#endif

#define    IEEE80211_NODE_AID(ni)    IEEE80211_AID(ni->ni_associd)


/* Not supported */
#define IEEE80211_NODE_WDSWAR_ISSENDDELBA(_ni)    \
    ( 0 )

int wlan_node_alloc_aid_bitmap(wlan_if_t vap, u_int32_t old_len);

#endif /* end of _ATH_STA_IEEE80211_NODE_H */
