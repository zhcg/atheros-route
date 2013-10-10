/*
 *  Copyright (c) 2005 Atheros Communications Inc.  All rights reserved.
 */

#ifndef _IEEE80211_TDLS_H_
#define _IEEE80211_TDLS_H_

#if UMAC_SUPPORT_TDLS

#define IEEE80211_TDLS_MAX_CLIENTS      256
#define IEEE80211_TDLS_DIALOG_TKN_SIZE  1

#define IEEE80211_TDLS_LEARNED_ARP_SIZE 6


/* temporary to be removed later */
#define dump_pkt(buf, len) { \
int i=0; \
for (i=0; i<len;i++) { \
    printf("%x ", ((unsigned char*)buf)[i]); \
    if(i && i%50 ==0) printf("\n"); } \
printf("\n"); }

/* XXX Move out */
/*  Fix for 
    EV:57703	Carrier 1.4.0.86 TDLS with WPA-PSK not work
    EV:58041	TDLS-WPA link not stable and doesn't work at 5Ghz band.
    Cause:
    DH-Peer Key is not transmitted by the Peer properly with skb_buf=500,
    as sizeof(SMK_M2) msg is 371+hdr, Using 500 size we see last 17 bytes
    of DH-Peer Key is corrupted abd hence increasing this size to 700.
    It works and fixes the above 2 issues.
*/
#define IEEE80211_ASSOC_MGMT_REQ_SIZE   700

#define IEEE80211_TDLS_TID(x)             WME_AC_TO_TID(x)
#define IEEE80211_TDLS_RFTYPE      2
#define IEEE80211_ELEMID_TDLSLNK   101 /* as per Draft9.0 */
#define IEEE80211_TDLS_LNKID_LEN   20
#define IEEE80211_TDLS_CATEGORY    12
/* TODO: FTIE */
#define IEEE80211_ELEMID_FTIE      200 /* TODO */

/* Used by ioctls to add and delete nodes from upnp */

#define IEEE80211_TDLS_ADD_NODE    0
#define IEEE80211_TDLS_DEL_NODE    1

#define IEEE80211_TDLS_WHDR_SET(skb, vap, da) { \
    struct ether_header *eh = (struct ether_header *) \
                wbuf_push(skb, sizeof(struct ether_header)); \
        IEEE80211_ADDR_COPY(eh->ether_shost, vap->iv_myaddr); \
        IEEE80211_ADDR_COPY(eh->ether_dhost, da);\
        eh->ether_type = IEEE80211_TDLS_ETHERTYPE; \
    }

/* Remote Frame Type = 2, TDLS Frame Type, Category && Action */
#define IEEE80211_TDLS_HDR_SET(frm, _type_, _len_, vap, arg) { \
        struct ieee80211_tdls_frame  *th = \
                    (struct ieee80211_tdls_frame *) frm; \
        th->rftype = IEEE80211_TDLS_RFTYPE; \
        th->tdls_pkt_type = _type_; \
        th->category = IEEE80211_TDLS_CATEGORY; \
        th->action = _type_; \
        frm += sizeof(struct ieee80211_tdls_frame); \
        }

/* Status Code */
#define IEEE80211_TDLS_SET_STATUS(frm, vap, arg, _status_) { \
        struct ieee80211_tdls_status  *th = \
                    (struct ieee80211_tdls_status *) frm; \
        th->status = _status_; \
        frm += sizeof(struct ieee80211_tdls_status); \
        }

/* Dialogue Token */
#define IEEE80211_TDLS_SET_TOKEN(frm, vap, arg, _token_) { \
        struct ieee80211_tdls_token  *th = \
                    (struct ieee80211_tdls_token *) frm; \
        th->token = _token_; \
        frm += sizeof(struct ieee80211_tdls_token); \
        }

#define IEEE80211_TDLS_SET_LINKID(frm, vap, arg) { \
        struct ieee80211_tdls_lnk_id  *th = \
                    (struct ieee80211_tdls_lnk_id *) frm; \
        th->elem_id = IEEE80211_ELEMID_TDLSLNK; \
        th->len = IEEE80211_TDLS_LNKID_LEN; \
        IEEE80211_ADDR_COPY(th->bssid, vap->iv_bss->ni_bssid); \
        IEEE80211_ADDR_COPY(th->sa, vap->iv_myaddr); \
        IEEE80211_ADDR_COPY(th->da, (u_int8_t *)arg); \
        th->reg_class = 0; \
        th->channel = 0; \
        frm += sizeof(struct ieee80211_tdls_lnk_id); \
        }

#define IEEE80211_TDLS_LLC_HDR_SET(frm) 

struct ieee80211_tdls_status {
    int8_t       status; /* Status Code */
} __packed;

struct ieee80211_tdls_token {
    int8_t       token; /* Dialogue Token */
} __packed;

struct ieee80211_tdls_lnk_id {
    u_int8_t    elem_id;
    u_int8_t    len;
    u_int8_t    bssid[6];
    u_int8_t    sa[6];
    u_int8_t    da[6];
    u_int8_t    reg_class;
    u_int8_t    channel;
} __packed;

struct ieee80211_tdls_frame {
    int8_t       rftype;        /* Remote Frame Type = 2 */
    int8_t       tdls_pkt_type; /* enum ieee80211_tdls_pkt_type */
    int8_t       category;      /* Category */
    int8_t       action;        /* Action */
} __packed;


typedef enum {
    IEEE80211_TDLS_STATE_SETUPREQ,
    IEEE80211_TDLS_STATE_SETUPRESP,
    IEEE80211_TDLS_STATE_CONFIRM,
    IEEE80211_TDLS_STATE_TEARDOWNREQ,
    IEEE80211_TDLS_STATE_TEARDOWNRESP,
    IEEE80211_TDLS_STATE_LEARNEDARP,
}ieee80211_tdls_state;
    
typedef enum {
    IEEE80211_TDLS_EVENT_WPAFTIE,
    IEEE80211_TDLS_EVENT_RECV,
}ieee80211_tdls_event;


typedef struct _wlan_tdls_sm *wlan_tdls_sm_t;

/* Entry for each node and state machine (per MAC address) */

struct _wlan_tdls_sm {
    struct ieee80211vap    *tdls_vap;   /* back pointer to the vap */
    struct ieee80211_node  *tdls_ni;
        u_int8_t            tdls_addr[IEEE80211_ADDR_LEN];   /* node's mac addr */
    ieee80211_tdls_event    tdls_trigger;    /* event to trigger the transition of the state machine */
    ieee80211_hsm_t         tdls_sm_entry;   /* state machine entry for tdls */
    TAILQ_ENTRY(_wlan_tdls_sm)   tdlslist;
};

/* this type of function will be called while iterating the state machine list for each entry,
 * with two arguments, and the return value indicate to break (if return 1) or continue (if return 0)
 * iterating 
 */
typedef int ieee80211_tdls_iterate_func(wlan_tdls_sm_t, void *, void *);

#ifdef CONFIG_RCPI
/* TODO:RCPI: This is a temporary definition for RCPI Header, 
   refer 802.11k hdr */
typedef struct ieee80211_tdls_frame IEEE80211_TDLS_RCPI_HDR;

/* TODO:RCPI: Path Swicth Element ID */
#define TDLS_RCPI_PATH_ELEMENT_ID 0x1

#define AP_PATH         0
#define DIRECT_PATH     1

#define RCPI_TIMER           3000
#define RCPI_HIGH_THRESHOLD  35 
#define RCPI_LOW_THRESHOLD   10 
#define RCPI_MARGIN          20 

/* Reason for the Path Switch Request frame */
#define REASON_UNSPECIFIED                  0
#define REASON_CHANGE_IN_POWERSAVE_MODE     1
#define REASON_CHANGE_IN_LINK_STATE         2

/* Result for Tx Path Switch Response frame */
#define ACCEPT                                          0
#define REJECT_ENTERING_POWER_SAVE  1
#define REJECT_LINK_STATUS          2
#define REJECT_UNSPECIFIED_REASON   3

struct ieee80211_tdls_rcpi_path {
    u_int8_t     elem_id;    /* Element ID */
    u_int8_t     len;        /* Length of Element */
    u_int8_t     path;       /* Path: 0-AP, 1-Direct Link path */
} __packed;

struct ieee80211_tdls_rcpi_switch {
        struct ieee80211_tdls_rcpi_path path;
        u_int8_t reason;
} __packed;

struct ieee80211_tdls_rcpi_link_req {
        char bssid[ETH_ALEN];           /* BSSID: */
        char sta_addr[ETH_ALEN];        /* STA ADDR: */
} __packed;

struct ieee80211_tdls_rcpi_link_report {
        char bssid[ETH_ALEN];           /* BSSID: */
    u_int8_t     rcpi1;                 /* RCPI value for AP path */
        char sta_addr[ETH_ALEN];        /* STA ADDR: */
    u_int8_t     rcpi2;                 /* RCPI value for Direct Link path */
} __packed;


#define IEEE80211_TDLS_RCPI_SWITCH_SET(req, path, reason)       \
        req.path.elem_id = TDLS_RCPI_PATH_ELEMENT_ID;   \
        req.path.len     = 1;   \
        req.path.path    = path;        \
        req.reason       = reason;

#define IEEE80211_TDLS_RCPI_LINK_ADDR(req, bssid, mac)  \
    IEEE80211_ADDR_COPY(req.bssid, bssid);      \
    IEEE80211_ADDR_COPY(req.sta_addr, mac);

/* used in ieee80211_input() to calculate Avg.RSSI for each node */
#define IEEE80211_TDLS_UPDATE_RCPI ieee80211_tdls_update_rcpi

#define IEEE80211_TDLSRCPI_PATH_SET(ni)    (ni->ni_tdls->link_st = DIRECT_PATH)
#define IEEE80211_TDLSRCPI_PATH_RESET(ni)  (ni->ni_tdls->link_st = AP_PATH)
#define IEEE80211_IS_TDLSRCPI_PATH_SET(ni) (ni->ni_tdls->link_st)

#endif /* CONFIG_RCPI */

#endif /* UMAC_SUPPORT_TDLS */
#endif /* _IEEE80211_TDLS_H_ */
