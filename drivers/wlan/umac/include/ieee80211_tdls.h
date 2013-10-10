/*
 *  Copyright (c) 2005 Atheros Communications Inc.  All rights reserved.
 */

#ifndef _IEEE80211_TDLS_SHIM_H_
#define _IEEE80211_TDLS_SHIM_H_

/* Used in either case, even if TDLS is not enabled, we check 
 * for this frame type, if it is we drop the frame. No business
 * for this frame if TDLS is disabled. 
 */
#define IEEE80211_TDLS_ETHERTYPE        0x890d

/* To represent MAC in string format xx:xx:xx:xx:xx:xx */
#define MACSTR_LEN 17

#if UMAC_SUPPORT_TDLS

#define IEEE80211_TDLS_ATTACH(ic)               ieee80211_tdls_attach(ic)
#define IEEE80211_TDLS_DETACH(ic)               ieee80211_tdls_detach(ic)
#define IEEE80211_TDLS_IOCTL(vap, type, mac)    ieee80211_tdls_ioctl(vap, type, mac)
#define IEEE80211_TDLS_RCV_MGMT(ic, ni, skb)    ieee80211_tdls_recv_mgmt(ic, ni, skb)
#define IEEE80211_TDLS_SND_MGMT(vap, type, arg) ieee80211_tdls_send_mgmt(vap, type, arg)
#define IEEE80211_TDLS_MGMT_FRAME(llc)          (llc->llc_snap.ether_type == IEEE80211_TDLS_ETHERTYPE)
#define IEEE80211_TDLS_ENABLED(vap)             (vap->iv_ath_cap & IEEE80211_ATHC_TDLS)
#define IEEE80211_IS_TDLS_NODE(ni)              (ni->ni_flags & IEEE80211_NODE_TDLS)
#define IEEE80211_IS_TDLS_TIMEOUT(ni) \
	((ni->ni_tdls->state == IEEE80211_TDLS_S_TEARDOWN_REQ_INPROG) || \
	 (ni->ni_tdls->state == IEEE80211_TDLS_S_TEARDOWN_RESP_INPROG) )

#ifdef CONFIG_RCPI
#define IEEE80211_IS_TDLSRCPI_PATH_SET(ni) (ni->ni_tdls->link_st)
#endif /* CONFIG_RCPI */

/* TDLS frame format */
enum ieee80211_tdls_pkt_type {
    IEEE80211_TDLS_SETUP_REQ,
    IEEE80211_TDLS_SETUP_RESP,
    IEEE80211_TDLS_SETUP_CONFIRM,
    IEEE80211_TDLS_TEARDOWN_REQ,
    IEEE80211_TDLS_TEARDOWN_RESP,
#ifdef CONFIG_RCPI
    IEEE80211_TDLS_TXPATH_SWITCH_REQ,
    IEEE80211_TDLS_TXPATH_SWITCH_RESP,
    IEEE80211_TDLS_RXPATH_SWITCH_REQ,
    IEEE80211_TDLS_RXPATH_SWITCH_RESP,
    IEEE80211_TDLS_LINKRCPI_REQ,
    IEEE80211_TDLS_LINKRCPI_REPORT,
#endif /* CONFIG_RCPI */
    IEEE80211_TDLS_LEARNED_ARP,
    IEEE80211_TDLS_MSG_MAX
};

struct ieee80211_tdls {
    /* Clients behind this node */
    ATH_LIST_HEAD(, ieee80211_tdlsarp) tdls_arp_hash[IEEE80211_NODE_HASHSIZE];
	int (*recv_mgmt)(struct ieee80211com *ic, 
	        struct ieee80211_node *ni, wbuf_t wbuf);
    int tdls_enable;
#ifdef CONFIG_RCPI
        struct timer_list tmrhdlr;
        u_int16_t    timer;
        u_int8_t     hithreshold;
        u_int8_t     lothreshold;
        u_int8_t     margin;
        u_int8_t     hi_tmp;
        u_int8_t     lo_tmp;
        u_int8_t     mar_tmp;
#endif /* CONFIG_RCPI */

    /* A list for all nodes (and state machines) */
    struct ieee80211vap *tdlslist_vap;       /* backpointer to vap */
    u_int16_t       tdls_count;
    rwlock_t        tdls_lock; 
    TAILQ_HEAD(TDLS_HEAD_TYPE, _wlan_tdls_sm) tdls_node;

    /*
     * functions for tdls
     */                               
    int    (*tdls_detach) (struct ieee80211com *);      
    int    (*tdls_wpaftie)(struct ieee80211vap *vap, u8 *buf, int len);
    int    (*tdls_recv)   (struct ieee80211com *ic, u8 *buf, int len);

};

#define ieee80211_tdlslist  ieee80211_tdls

/* Stores node related state for TDLS */
struct ieee80211_tdls_node {
#define IEEE80211_TDLS_S_INIT                   0
#define IEEE80211_TDLS_S_SETUP_REQ_INPROG       1
#define IEEE80211_TDLS_S_SETUP_RESP_INPROG      2
#define IEEE80211_TDLS_S_SETUP_CONF_INPROG      3
#define IEEE80211_TDLS_S_TEARDOWN_REQ_INPROG    4
#define IEEE80211_TDLS_S_TEARDOWN_RESP_INPROG   5 
#define IEEE80211_TDLS_S_RUN                    6
#ifdef CONFIG_RCPI
#define IEEE80211_TDLS_S_TXSWITCH_REQ_INPROG    7
#define IEEE80211_TDLS_S_TXSWITCH_RESP_INPROG   8 
#define IEEE80211_TDLS_S_RXSWITCH_REQ_INPROG    9
#define IEEE80211_TDLS_S_RXSWITCH_RESP_INPROG   10 
#define IEEE80211_TDLS_S_LINKRCPI_REQ_INPROG    11 
#define IEEE80211_TDLS_S_LINKRCPI_REPORT_INPROG 12 
    u_int8_t    link_st;
#endif /* CONFIG_RCPI */
    u_int8_t    stk_st;
    u_int8_t    token;
    u_int8_t    state;
};

int ieee80211_tdls_attach(struct ieee80211com *ic);
int ieee80211_tdls_detach(struct ieee80211com *ic);
int ieee80211_tdls_ioctl(struct ieee80211vap *vap, int type, char *mac);
int ieee80211_tdls_send_mgmt(struct ieee80211vap *vap, u_int32_t type, void *arg);
int ieee80211_tdls_recv_mgmt(struct ieee80211com *ic, struct ieee80211_node *ni,
                              struct sk_buff *skb);
/* The function is used  to set the MAC address  of a device set via iwpriv */
void ieee80211_tdls_set_mac_addr(u_int8_t *mac_address, 
                              u_int32_t word1, u_int32_t word2);
int ieee80211_ioctl_set_tdls_rmac(struct net_device *dev,
                              void *info, void *w, char *extra);
int ieee80211_ioctl_clr_tdls_rmac(struct net_device *dev,
                              void *info, void *w, char *extra);
int ieee80211_wpa_tdls_ftie(struct ieee80211vap *vap, u8 *buf, int len);

struct ieee80211_node *
ieee80211_find_tdlsnode(struct ieee80211vap *vap, const u_int8_t *macaddr);

/*  
 * tdls ops
 */
typedef struct ieee80211_tdls ieee80211_tdls_ops_t;


#else /* UMAC_SUPPORT_TDLS */

#define IEEE80211_TDLS_MGMT_FRAME(llc) (0)
#define IEEE80211_TDLS_ENABLED(vap) (0)
#define IEEE80211_IS_TDLS_NODE(ni) (0)
#define IEEE80211_IS_TDLS_TIMEOUT(ni) (0)
#define IEEE80211_TDLS_SND_MGMT(vap, type, arg) 
#define IEEE80211_TDLS_RCV_MGMT(vap, type, arg) 
#define IEEE80211_TDLS_ATTACH(ic) 
#define IEEE80211_TDLS_DETACH(ic) 
#define IEEE80211_TDLS_IOCTL(vap, type, mac) 
#ifdef CONFIG_RCPI
#define IEEE80211_IS_TDLSRCPI_PATH_SET(ni) (0)
#endif /* CONFIG_RCPI */
#endif /* UMAC_SUPPORT_TDLS */

#endif /* _IEEE80211_TDLS__SHIM_H_ */
