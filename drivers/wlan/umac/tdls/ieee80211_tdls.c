/*
 *  Copyright (c) 2005 Atheros Communications Inc.  All rights reserved.
 */

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#endif

#include <ieee80211_var.h>
#include <ieee80211.h>
#include "ieee80211_wds.h"
#include "ieee80211_rateset.h" /* for rateset vars */

#if UMAC_SUPPORT_TDLS 
#include <osif_private.h>   /* for wireless_send_event */
#include <ieee80211_sm.h>
#include <ieee80211_tdls_priv.h>

#include <if_athvar.h> /* for definition of ATH_NODE_NET80211(ni) */

#define WRITE_TDLS_LENGTH_FIELD(th, _len)  \
                        ((struct ieee80211_tdls_frame *)th)->lnk_id.len = _len;

/* taken as reference from os/linux/src/osif_umac.c */
#define OSIF_TO_NETDEV(_osif) (((osif_dev *)(_osif))->netdev)
#define ieee80211_tdls_get_dev(vap) OSIF_TO_NETDEV((struct net_device *) \
            wlan_vap_get_registered_handle(vap))
            
/* TDLS message type packet length */
int frame_length[] = { 
    /* IEEE80211_TDLS_SETUP_REQ */
    IEEE80211_ASSOC_MGMT_REQ_SIZE + IEEE80211_TDLS_DIALOG_TKN_SIZE,
    /* IEEE80211_TDLS_SETUP_RESP */
    IEEE80211_ASSOC_MGMT_REQ_SIZE + IEEE80211_TDLS_DIALOG_TKN_SIZE + 2,
    /* IEEE80211_TDLS_SETUP_CONFIRM */
    /* XXX: ?? allocate more memory to fill SMK */
    IEEE80211_ASSOC_MGMT_REQ_SIZE + IEEE80211_TDLS_DIALOG_TKN_SIZE,
    /* IEEE80211_TDLS_TEARDOWN_REQ */
    IEEE80211_ASSOC_MGMT_REQ_SIZE + IEEE80211_TDLS_DIALOG_TKN_SIZE,
    /* IEEE80211_TDLS_TEARDOWN_RESP */ 
    IEEE80211_TDLS_DIALOG_TKN_SIZE + 2,
#ifdef CONFIG_RCPI
    /* IEEE80211_TDLS_TXPATH_SWITCH_REQ */
    IEEE80211_ASSOC_MGMT_REQ_SIZE + 4, /* Path(3-Octets) + Reason(1-Octet) */
    /* IEEE80211_TDLS_TXPATH_SWITCH_RESP */
    IEEE80211_ASSOC_MGMT_REQ_SIZE + 4, /* Path(3-Octets) + Reason(1-Octet) */
    /* IEEE80211_TDLS_RXPATH_SWITCH_REQ */
    IEEE80211_ASSOC_MGMT_REQ_SIZE + 4, /* Path(3-Octets) + Reason(1-Octet) */
    /* IEEE80211_TDLS_RXPATH_SWITCH_RESP */
    IEEE80211_ASSOC_MGMT_REQ_SIZE + 3, /* Path(3-Octets) */
    /* IEEE80211_TDLS_LINKRCPI_REQ */
    IEEE80211_ASSOC_MGMT_REQ_SIZE + 12, /* BSSID + STA Address */
    /* IEEE80211_TDLS_LINKRCPI_REPORT */
    IEEE80211_ASSOC_MGMT_REQ_SIZE + 14, /* BSSID + RCPI + STA Address + RCPI */
#endif
    IEEE80211_ASSOC_MGMT_REQ_SIZE + IEEE80211_TDLS_DIALOG_TKN_SIZE,
    /* IEEE80211_TDLS_LEARNED_ARP */
    IEEE80211_TDLS_LEARNED_ARP_SIZE,
};

#ifdef CONFIG_RCPI
int ieee80211_tdls_link_rcpi_report(struct ieee80211vap *vap, char * bssid,
                                char * mac, u_int8_t rcpi1,
                                u_int8_t rcpi2);

static void ieee80211_tdlsrcpi_timer_init(struct ieee80211com *ic,
                                void * timerArg);

static void ieee80211_tdlsrcpi_timer_stop(struct ieee80211com *ic,
                                unsigned long timerArg);

static int ieee80211_tdls_link_rcpi_req(struct ieee80211vap *vap, char * bssid,
                                char * mac);
#endif

static bool ieee80211_tdls_state_setupreq_event(void *ctx, u_int16_t event,
                u_int16_t event_data_len, void *event_data);
static bool ieee80211_tdls_state_setupresp_event(void *ctx, u_int16_t event,
                u_int16_t event_data_len, void *event_data);
static bool ieee80211_tdls_state_confirm_event(void *ctx, u_int16_t event,
                u_int16_t event_data_len, void *event_data);
static bool ieee80211_tdls_state_teardownreq_event(void *ctx, u_int16_t event,
                u_int16_t event_data_len, void *event_data);
static bool ieee80211_tdls_state_teardownresp_event(void *ctx, u_int16_t event,
                u_int16_t event_data_len, void *event_data);
static bool ieee80211_tdls_state_learnedarp_event(void *ctx, u_int16_t event,
                u_int16_t event_data_len, void *event_data);

void ieee80211_tdls_del_node(struct ieee80211_node *ni);

static inline 
int get_wpa_mode(struct ieee80211vap *vap) {                                       
    ieee80211_auth_mode modes[IEEE80211_AUTH_MAX];             
    int count, i;                                              
    int _wpa_ = 0;                                                 
    count = wlan_get_auth_modes(vap,modes,IEEE80211_AUTH_MAX); 
    for (i = 0; i < count; i++) {                              
        if (modes[i] == IEEE80211_AUTH_WPA)                      
            _wpa_ |= 0x1;                                        
        if (modes[i] == IEEE80211_AUTH_RSNA)                     
            _wpa_ |= 0x2;                                        
    }                                                          
return _wpa_;
}

static void ieee80211_tdls_sm_debug_print (void *ctx,const char *fmt,...) 
{
    char tmp_buf[256];
    va_list ap;
     
    va_start(ap, fmt);
    vsnprintf (tmp_buf,256, fmt, ap);
    va_end(ap);
    IEEE80211_DPRINTF(((wlan_tdls_sm_t) ctx)->tdls_vap, IEEE80211_MSG_IQUE,
        "%s", tmp_buf);
}


static bool ieee80211_tdls_state_setupreq_event(void *ctx, u_int16_t event,
                u_int16_t event_data_len, void *event_data)
{
    wlan_tdls_sm_t sm = (wlan_tdls_sm_t) ctx;

    switch(event) {
        case IEEE80211_TDLS_STATE_SETUPREQ:
            ieee80211_sm_transition_to(sm->tdls_sm_entry, IEEE80211_TDLS_STATE_SETUPRESP);
            return true;
        default:
            return false;
    }
    return false;
}

static bool ieee80211_tdls_state_setupresp_event(void *ctx, u_int16_t event,
                u_int16_t event_data_len, void *event_data)
{
    wlan_tdls_sm_t sm = (wlan_tdls_sm_t) ctx;

    switch(event) {
        case IEEE80211_TDLS_STATE_SETUPRESP:
            ieee80211_sm_transition_to(sm->tdls_sm_entry, IEEE80211_TDLS_STATE_CONFIRM);
            return true;
        default:
            return false;
    }
    return false;
}

static bool ieee80211_tdls_state_confirm_event(void *ctx, u_int16_t event,
                u_int16_t event_data_len, void *event_data)
{
//    wlan_tdls_sm_t sm = (wlan_tdls_sm_t) ctx;

    switch(event) {
        case IEEE80211_TDLS_STATE_CONFIRM:
            return true;
        default:
            return false;
    }
    return false;
}

static bool ieee80211_tdls_state_teardownreq_event(void *ctx, u_int16_t event,
                u_int16_t event_data_len, void *event_data)
{
    wlan_tdls_sm_t sm = (wlan_tdls_sm_t) ctx;

    switch(event) {
        case IEEE80211_TDLS_STATE_TEARDOWNREQ:
            ieee80211_sm_transition_to(sm->tdls_sm_entry, IEEE80211_TDLS_STATE_TEARDOWNRESP);
            return true;
        default:
            return false;
    }
    return false;
}

static bool ieee80211_tdls_state_teardownresp_event(void *ctx, u_int16_t event,
                u_int16_t event_data_len, void *event_data)
{
//    wlan_tdls_sm_t sm = (wlan_tdls_sm_t) ctx;

    switch(event) {
        case IEEE80211_TDLS_STATE_TEARDOWNRESP:
            return true;
        default:
            return false;
    }
    return false;
}

static bool ieee80211_tdls_state_learnedarp_event(void *ctx, u_int16_t event,
                u_int16_t event_data_len, void *event_data)
{
//    wlan_tdls_sm_t sm = (wlan_tdls_sm_t) ctx;

    switch(event) {
        case IEEE80211_TDLS_STATE_LEARNEDARP:
            return true;
        default:
            return false;
    }
    return false;
}


/*
 * Definition of the states for headline block removal state machine
 */

ieee80211_state_info ieee80211_tdls_sm_info[] = {
    { 
        (u_int8_t) IEEE80211_TDLS_STATE_SETUPREQ, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "SETUPREQ",
        NULL,
        NULL,
        ieee80211_tdls_state_setupreq_event
    },
    { 
        (u_int8_t) IEEE80211_TDLS_STATE_SETUPRESP, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "SETUPRESP",
        NULL,
        NULL,
        ieee80211_tdls_state_setupresp_event
    },
    { 
        (u_int8_t) IEEE80211_TDLS_STATE_CONFIRM, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "CONFIRM",
        NULL,
        NULL,
        ieee80211_tdls_state_confirm_event
    },
    { 
        (u_int8_t) IEEE80211_TDLS_STATE_TEARDOWNREQ, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "TEARDOWNREQ",
        NULL,
        NULL,
        ieee80211_tdls_state_teardownreq_event
    },
    { 
        (u_int8_t) IEEE80211_TDLS_STATE_TEARDOWNRESP, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "TEARDOWNRESP",
        NULL,
        NULL,
        ieee80211_tdls_state_teardownresp_event
    },
};

static const char*    tdls_event_name[] = {
    /* IEEE80211_TDLS_EVENT_WPAFTIE    */ "EVENT_WPAFTIE",
    /* IEEE80211_TDLS_EVENT_WPAFTIE    */ "EVENT_WPAFTIE",
};

/* Iterate through the list to find the state machine by the node's address */
static wlan_tdls_sm_t ieee80211_tdls_find_byaddr(struct ieee80211com *ic, u_int8_t *addr)
{
    wlan_tdls_sm_t sm;
    wlan_tdls_sm_t smt;
    struct ieee80211_tdls *td;
    rwlock_state_t lock_state;

    if (!addr)
        return NULL;	
    sm = NULL;
    td = ic->ic_tdls;
    OS_RWLOCK_WRITE_LOCK(&td->tdls_lock, &lock_state);
    TAILQ_FOREACH(smt, &td->tdls_node, tdlslist) {
        if (smt) {
            if(IEEE80211_ADDR_EQ(addr, smt->tdls_addr)) {	
                sm = smt;
                break;
            }
        }	
    }
    OS_RWLOCK_WRITE_UNLOCK(&td->tdls_lock, &lock_state);
    return sm;
}

static void ieee80211_tdls_delentry(struct ieee80211com *ic, struct ieee80211_node *ni)
{
    wlan_tdls_sm_t sm;
    struct ieee80211_tdls *td;

    td = ic->ic_tdls;
    if (ni == NULL) {
        return;      
    }      
    sm = ieee80211_tdls_find_byaddr(ic, ni->ni_macaddr);
    if (sm) {
        td->tdls_count --;
        TAILQ_REMOVE(&td->tdls_node, sm, tdlslist);
        ieee80211_sm_delete(sm->tdls_sm_entry);
        OS_FREE(sm); sm=NULL;
    }
    return;
}

/* Add one entry to the state machine list if no one state machine with 
 * this address is found in the list 
 */
static void ieee80211_tdls_addentry(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
    wlan_tdls_sm_t sm;
    struct ieee80211_tdls *td;
    rwlock_state_t lock_state;
    
    td = vap->iv_tdlslist;
    if (ni == NULL)
        return;            
    sm = ieee80211_tdls_find_byaddr(vap->iv_ic, ni->ni_macaddr);

    /*remove this entry if it exists previously, reset the state machine for this node*/
    if (sm) {
        OS_RWLOCK_WRITE_LOCK(&td->tdls_lock, &lock_state);
        ieee80211_tdls_delentry(vap->iv_ic, ni);       
        OS_RWLOCK_WRITE_UNLOCK(&td->tdls_lock, &lock_state);
    }
    sm = (wlan_tdls_sm_t)OS_MALLOC(vap-> iv_ic->ic_osdev, sizeof(struct _wlan_tdls_sm), GFP_KERNEL);
    sm->tdls_vap = vap;
    sm->tdls_ni = ni;
    sm->tdls_trigger = IEEE80211_TDLS_STATE_SETUPREQ;
    IEEE80211_ADDR_COPY(sm->tdls_addr, ni->ni_macaddr);
    /* We always reset the state of the state machine to ACTIVE, regardless it is
     * a new entry or one already exists in the list, because this function is
     * called by ieee80211_node_join which imply that we should reset the state
     * even if the entry already exists 
     */
    sm->tdls_sm_entry = ieee80211_sm_create(vap->iv_ic->ic_osdev, 
                                           "tdls",
                                           (void *)sm,
                                           IEEE80211_TDLS_STATE_SETUPREQ, /*init stats is always REQ*/
                                           ieee80211_tdls_sm_info,
                                           sizeof(ieee80211_tdls_sm_info)/sizeof(ieee80211_state_info),
                                           5,   
                                           0, /*no event data*/ 
                                           MESGQ_PRIORITY_NORMAL,
                                           OS_MESGQ_CAN_SEND_SYNC() ? IEEE80211_HSM_SYNCHRONOUS : IEEE80211_HSM_ASYNCHRONOUS, 
                                           ieee80211_tdls_sm_debug_print,
                                           tdls_event_name,
                                           IEEE80211_N(tdls_event_name));
    if (!sm->tdls_sm_entry) {
        IEEE80211_DPRINTF(vap,IEEE80211_MSG_TDLS,"%s : ieee80211_sm_create failed \n", __func__); 
        OS_FREE(sm); sm=NULL;
        return;
    }
    OS_RWLOCK_WRITE_LOCK(&td->tdls_lock, &lock_state);
    td->tdls_count ++;
    TAILQ_INSERT_TAIL(&td->tdls_node, sm, tdlslist);
    OS_RWLOCK_WRITE_UNLOCK(&td->tdls_lock, &lock_state);
    return;
}

/* Run each state machine */
static int ieee80211_tdls_step(wlan_tdls_sm_t sm, void *arg1, void *arg2)
{
    ieee80211_sm_dispatch(sm->tdls_sm_entry, sm->tdls_trigger, 0, NULL);
    return 0;
}

/* Iterate the state machine list with running the function *f. If *f returns 1, break out of the loop;
 * If *f returns 0, continue to loop 
 */
static void ieee80211_tdls_iterate(struct ieee80211vap *vap, ieee80211_tdls_iterate_func *f, void *arg1, void *arg2)
{
    wlan_tdls_sm_t smt;
    struct ieee80211_tdls *td;
    rwlock_state_t lock_state;

    td = vap->iv_tdlslist;
    OS_RWLOCK_WRITE_LOCK(&td->tdls_lock, &lock_state);
    TAILQ_FOREACH(smt, &td->tdls_node, tdlslist) {
        if (smt) {
            if ((*f)(smt, arg1, arg2))
                break;
            else
                continue;
        }
    }
    OS_RWLOCK_WRITE_UNLOCK(&td->tdls_lock, &lock_state);
}

int 
ieee80211_tdls_attach(struct ieee80211com *ic)
{

    if (!ic) return -EFAULT;

     ic->ic_tdls = (struct ieee80211_tdls *) OS_MALLOC(ic->ic_osdev,
     					sizeof(struct ieee80211_tdls), GFP_KERNEL);

    if (!ic->ic_tdls) {
        return -ENOMEM;
    }

    /* Disabled by default. If you want to enable it use iwpriv command
       to enable it */
    ic->ic_tdls->tdls_enable = 0;
	ic->tdls_recv_mgmt = ieee80211_tdls_recv_mgmt;
	
#ifdef CONFIG_RCPI
        ieee80211_tdlsrcpi_timer_init(ic, (void *)ic);
#endif
    return 0;
}

int 
ieee80211_tdls_sm_attach(struct ieee80211com *ic, struct ieee80211vap *vap)
{
static int is_vap_set=0;

    if(0==is_vap_set) {
    vap->iv_tdlslist = ic->ic_tdls;
    vap->iv_tdlslist->tdlslist_vap = vap;  
    vap->iv_tdlslist->tdls_count = 0;
    OS_RWLOCK_INIT(&vap->iv_tdlslist->tdls_lock);
    TAILQ_INIT(&vap->iv_tdlslist->tdls_node);

    /* attach functions */
    vap->iv_tdls_ops.tdls_detach = ieee80211_tdls_detach;
    vap->iv_tdls_ops.tdls_wpaftie = ieee80211_wpa_tdls_ftie;

    is_vap_set = 1;
    }
    
    return 0;
}

int 
ieee80211_tdls_sm_detach(struct ieee80211com *ic, struct ieee80211vap *vap)
{
    wlan_tdls_sm_t sm;
    wlan_tdls_sm_t smt;
    struct ieee80211_tdls *td;
    rwlock_state_t lock_state;

    td = ic->ic_tdls;
	if(!td)
	    return 0;

    sm = smt = NULL;
    OS_RWLOCK_WRITE_LOCK(&td->tdls_lock, &lock_state);
    TAILQ_FOREACH_SAFE(sm,  &td->tdls_node, tdlslist, smt) {
        if (sm) {
            ieee80211_tdls_delentry(ic, sm->tdls_ni);
        }
    }
    OS_RWLOCK_WRITE_UNLOCK(&td->tdls_lock, &lock_state);

    return 0;
}

int 
ieee80211_tdls_detach(struct ieee80211com *ic)
{
    struct ieee80211_tdls *td;

    if (!ic) return -EFAULT;
#ifdef CONFIG_RCPI
        ieee80211_tdlsrcpi_timer_stop(ic, (unsigned long)ic->ic_tdls);
#endif
    td = ic->ic_tdls;
    if(td->tdlslist_vap)
        ieee80211_tdls_sm_detach(ic, td->tdlslist_vap);
    
    OS_FREE(ic->ic_tdls); ic->ic_tdls=NULL;
    
    return 0;
}

/* Delete tdls node */
void 
ieee80211_tdls_del_node(struct ieee80211_node *ni)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_tdls *td;
    wlan_tdls_sm_t sm;
    rwlock_state_t lock_state;

    td = vap->iv_ic->ic_tdls;

	/* clear entry from state machine */
    sm = ieee80211_tdls_find_byaddr(vap->iv_ic, ni->ni_macaddr);
    if (sm) {
        /*remove this entry if it exists, reset the state machine for this node*/
        OS_RWLOCK_WRITE_LOCK(&td->tdls_lock, &lock_state);
        ieee80211_tdls_delentry(vap->iv_ic, ni);       
        OS_RWLOCK_WRITE_UNLOCK(&td->tdls_lock, &lock_state);
    }
    
    /* Reomve only TDLS nodes here */
    if (ni->ni_flags & IEEE80211_NODE_TDLS) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "Deleting the node..."
                          " refcnt:%d\n", ieee80211_node_refcnt(ni));
        ieee80211_node_leave(ni);
    }

}

u8 
ieee80211_tdls_processie(struct ieee80211_node *ni, 
        struct ieee80211_tdls_frame * tf, u16 len) {

#if ATH_DEBUG
    struct ieee80211vap *vap = ni->ni_vap;
#endif
  	u8 * sbuf = NULL;
  	u8 * ebuf = NULL;
    u_int8_t  *rates, *xrates, *htcap;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, 
        "Updating IEs for MAC:%s\n", ether_sprintf(ni->ni_macaddr));

    rates = xrates = htcap = NULL;

    if(!tf) return -EINVAL;

    if (tf) {
	    sbuf = (u8*)(tf);
  	    ebuf  = sbuf + len;
    	/* Parse thro frame to get FTIE, Update len = ie_len && _buf = ie */
        switch(tf->tdls_pkt_type) {
        case IEEE80211_TDLS_SETUP_REQ:
            sbuf = (u8 *)( (u8*)(tf+1)
                         + sizeof(struct ieee80211_tdls_token) );
        break;
        case IEEE80211_TDLS_SETUP_RESP:
        case IEEE80211_TDLS_SETUP_CONFIRM:
            sbuf = (u8 *)( (u8*)(tf+1) + 
                         + sizeof(struct ieee80211_tdls_status)
                         + sizeof(struct ieee80211_tdls_token) );
        break;
        case IEEE80211_TDLS_TEARDOWN_REQ:
            sbuf = (u8 *)( (u8*)(tf+1) + 
                         + sizeof(struct ieee80211_tdls_status) );
        break;
        default:
            /* Default: Should not hit */
            sbuf = NULL;
        break;
        }
    }

    while (sbuf < ebuf) {
        switch (*sbuf) {
        case IEEE80211_ELEMID_RATES:
            rates = sbuf;
            break;
        case IEEE80211_ELEMID_XRATES:
            xrates = sbuf;
            break;
        case IEEE80211_ELEMID_HTCAP:
            if (htcap == NULL) {
                htcap = (u_int8_t *)&((struct ieee80211_ie_htcap *)sbuf)->hc_ie;
            }
            break;
        case IEEE80211_ELEMID_VENDOR:
            if(ishtcap(sbuf)) {
                if (htcap == NULL) {
                    htcap = (u_int8_t *)&((struct vendor_ie_htcap *)sbuf)->hc_ie;
                }
            }
            break;
        }
        sbuf += sbuf[1] + 2;
    }

    if (!rates || (rates[1] > IEEE80211_RATE_MAXSIZE)) {
        /* XXX: msg + stats */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, 
            "%s: Failed to get HTRATE or rate set mismatch for MAC:%s\n", 
            __func__, ether_sprintf(ni->ni_macaddr));
        return -EINVAL;
    }

    if (!ieee80211_setup_rates(ni, rates, xrates,
        IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE |
        IEEE80211_F_DOXSECT)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, 
            "%s: Failed to update HTRATE for MAC:%s\n", 
            __func__, ether_sprintf(ni->ni_macaddr));
        return -EINVAL;
    }

    if(htcap) {
        /*
         * Flush any state from a previous handshake.
         */
        ieee80211node_clear_flag(ni, IEEE80211_NODE_HT);
        IEEE80211_NODE_CLEAR_HTCAP(ni);

        /* record capabilities, mark node as capable of HT */
        ieee80211_parse_htcap(ni, htcap);

        if (!ieee80211_setup_ht_rates(ni, htcap,
            IEEE80211_F_DOFRATE | IEEE80211_F_DOXSECT |
            IEEE80211_F_DOBRS)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, 
            "%s: Failed to update HTCAP for MAC:%s\n", 
                __func__, ether_sprintf(ni->ni_macaddr));
            return -EINVAL;
        }
    } else {
        /*
         * Flush any state from a previous handshake.
         */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, 
            "%s: Failed: flushing HTRATE for MAC:%s\n", 
            __func__, ether_sprintf(ni->ni_macaddr));

        ni->ni_flags &= ~IEEE80211_NODE_HT;
        ni->ni_htcap = 0;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, 
        "niflags:%x, HTCAP:%x, CAPINFO:%x \n",
        ni->ni_flags, ni->ni_htcap, ni->ni_capinfo);

    return 0;
}

u8 * 
ieee80211_tdls_get_ftie(struct ieee80211_tdls_frame * tf, u16 len) {

  	u8 * sbuf = NULL;
  	u8 * eof  = NULL;

    if (tf) {
	    sbuf = (u8*)(tf);
  	    eof  = sbuf + len;
    	/* Parse thro frame to get FTIE, Update len = ie_len && _buf = ie */
        switch(tf->tdls_pkt_type) {
        case IEEE80211_TDLS_SETUP_REQ:
            sbuf = (u8 *)( (u8*)(tf+1)
                         + sizeof(struct ieee80211_tdls_token) );
        break;
        case IEEE80211_TDLS_SETUP_RESP:
        case IEEE80211_TDLS_SETUP_CONFIRM:
            sbuf = (u8 *)( (u8*)(tf+1) + 
                         + sizeof(struct ieee80211_tdls_status)
                         + sizeof(struct ieee80211_tdls_token) );
        break;
        case IEEE80211_TDLS_TEARDOWN_REQ:
            sbuf = (u8 *)( (u8*)(tf+1) + 
                         + sizeof(struct ieee80211_tdls_status) );
        break;
        default:
            /* Default: Should not hit */
            sbuf = NULL;
        break;
        }

    	do {
            if(*sbuf == IEEE80211_ELEMID_FTIE) {
                break;
            } else {
                sbuf++; /* To access len */
                sbuf += *sbuf;
                sbuf++; /* To offset ie */
            }
        } while((eof-sbuf) > 0);

        if(*sbuf != IEEE80211_ELEMID_FTIE)
            sbuf = NULL;

    }
    return sbuf;
}

static void 
tdls_notify_supplicant(struct ieee80211vap *vap, char *mac, 
                            enum ieee80211_tdls_pkt_type type, 
                            u8 * ftie)
{
    uint8_t len = 0;
    u8 * _buf = NULL;
    static const char *tag = "TDLSSTART.request=";
    struct net_device *dev = ieee80211_tdls_get_dev(vap);
    union iwreq_data wrqu;
    char buf[512], *cur_buf;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,  "tdls_notify_supplicant: %s\n",
                      ether_sprintf(mac));

    if(ftie) {
        if(*ftie == IEEE80211_ELEMID_FTIE) {
            ftie++; /* To access len */
            len = *ftie;
            ftie++; /* To access value */
		    _buf = ftie;
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, ", smk_len=%d. \n", len);
        }
    }
	
	cur_buf = buf;

    snprintf(cur_buf, sizeof(buf), "%s", tag);
	cur_buf += strlen(tag);

	memcpy(cur_buf, mac, ETH_ALEN);
	cur_buf += ETH_ALEN;

	*(uint16_t*)cur_buf = type;
	cur_buf += 2;

	*(uint16_t*)cur_buf = (u16)len;
	cur_buf += 2;

	if(_buf && len) {
	    /* len is checked to avoid buffer overflow attack */
            if(len <= sizeof(buf) + buf - cur_buf) {
	        memcpy(cur_buf, _buf, len);
                cur_buf += len;
	    } else {
	        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,"Message length "
				"too high, len=%d\n", len);
	        return;
	    }
	}

    memset(&wrqu, 0, sizeof(wrqu));
    wrqu.data.length = cur_buf - buf;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "%s: %s: %s, %x len:%d\n", 
                      __FUNCTION__, ether_sprintf(mac), buf, type, wrqu.data.length);

	/* HACK in driver to get around IWEVCUSTOM size limit */
	/* 
	 * IWEVASSOCREQIE: Linux wireless extension events are not really
	 * going to work very well in some cases. Its assumed, this doesn't apply 
	 * to target systems with 64-bit kernel and 32-bit userspace, if so, 
	 * please note that this will break.
	 *
	 */
	wireless_send_event(dev, IWEVCUSTOM, &wrqu, buf);
}

/* Notification to TdlsDiscovery process on TearDownReq from Supplicant 
   also used on other occassions like channel change and others
   */
void 
tdls_notify_teardownreq(struct ieee80211vap *vap, char *mac, 
                            enum ieee80211_tdls_pkt_type type, 
                            struct ieee80211_tdls_frame *tf)
{
    uint16_t len = 0;
    static const char *tag = "STKSTART.request=TearDown";
    struct net_device *dev = ieee80211_tdls_get_dev(vap);
    union iwreq_data wrqu;
    char buf[512], *cur_buf;

    //IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,  "tdls_notify_teardownreq: %s\n",
      //               ether_sprintf(mac));

	cur_buf = buf;

        snprintf(cur_buf, sizeof(buf), "%s", tag);
	cur_buf += strlen(tag);

	memcpy(cur_buf, mac, ETH_ALEN);
	cur_buf += ETH_ALEN;

	*(uint16_t*)cur_buf = type;
	cur_buf += 2;

	*(uint16_t*)cur_buf = len;
	cur_buf += 2;

        memset(&wrqu, 0, sizeof(wrqu));
        wrqu.data.length = cur_buf - buf;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "%s: %s: %s, %x len:%d\n", 
                      __FUNCTION__, ether_sprintf(mac), buf, type, wrqu.data.length);

	/* HACK in driver to get around IWEVCUSTOM size limit */
	/* 
	 * IWEVASSOCREQIE: Linux wireless extension events are not really
	 * going to work very well in some cases. Its assumed, this doesn't apply 
	 * to target systems with 64-bit kernel and 32-bit userspace, if so, 
	 * please note that this will break.
	 *
	 */
	wireless_send_event(dev, IWEVASSOCREQIE, &wrqu, buf);
}

/** 
 * check wpa/rsn ie is present in the ie buffer passed in. 
 */
static bool ieee80211_check_wpaie(struct ieee80211vap *vap, u_int8_t *iebuf, u_int32_t length)
{
    u_int8_t *iebuf_end = iebuf + length;
    struct ieee80211_rsnparms tmp_rsn;
    bool add_wpa_ie = true;
    while (add_wpa_ie && ((iebuf+1) < iebuf_end )) {
        if (iebuf[0] == IEEE80211_ELEMID_VENDOR) {
            if (iswpaoui(iebuf) &&  (ieee80211_parse_wpa(vap, iebuf, &tmp_rsn) == 0)) {
                if (RSN_CIPHER_IS_CLEAR(&vap->iv_rsn) || vap->iv_rsn.rsn_ucastcipherset == 0) {
                    vap->iv_rsn = tmp_rsn;
                }
                /* found WPA IE */
                add_wpa_ie = false;
            }
        }
        else if(iebuf[0] == IEEE80211_ELEMID_RSN) {
            if (ieee80211_parse_rsn(vap, iebuf, &tmp_rsn) == 0) {
                /* found RSN IE */
                if (RSN_CIPHER_IS_CLEAR(&vap->iv_rsn) || vap->iv_rsn.rsn_ucastcipherset == 0) {
                vap->iv_rsn = tmp_rsn;
                }
                add_wpa_ie = false;
            }
        }
        iebuf += iebuf[1] + 2;
        continue;
    }
    return add_wpa_ie;
}           

/*
 * Setup an association/reassociation request,
 * and returns the frame length.
 * TDLS: Modification for Draft9.0
 */
static u_int16_t
ieee80211_setup_assoc(
    struct ieee80211_node *ni,
    struct ieee80211_frame *wh,
    int reassoc,
    u_int8_t *previous_bssid
    )
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
    u_int8_t *frm;
    u_int16_t capinfo = 0;
    u_int8_t subtype = reassoc ? IEEE80211_FC0_SUBTYPE_REASSOC_REQ :
        IEEE80211_FC0_SUBTYPE_ASSOC_REQ;
    bool  add_wpa_ie;

    frm = (u_int8_t *)wh;

    /*
     * asreq frame format
     *[2] capability information
     *[2] listen interval (Not Needed)
     *[6*] current AP address (reassoc only) (Not Needed)
     *[tlv] ssid (Not Needed)
     *[tlv] supported rates
     *[4] power capability (Not Needed)
     *[28] supported channels element
     *[tlv] extended supported rates
     *[tlv] WME [if enabled and AP capable] (Not Needed)
     *[tlv] HT Capabilities
     *      [tlv] Atheros advanced capabilities
     *[tlv] user-specified ie's
     */
    if (vap->iv_opmode == IEEE80211_M_IBSS)
        capinfo |= IEEE80211_CAPINFO_IBSS;
    else if (vap->iv_opmode == IEEE80211_M_STA || vap->iv_opmode == IEEE80211_M_BTAMP)
        capinfo |= IEEE80211_CAPINFO_ESS;
    else
        ASSERT(0);
    
    if (IEEE80211_VAP_IS_PRIVACY_ENABLED(vap))
        capinfo |= IEEE80211_CAPINFO_PRIVACY;
    /*
     * NB: Some 11a AP's reject the request when
     *     short premable is set.
     */
    if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
        IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
        capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
    if ((ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME) &&
        (ic->ic_flags & IEEE80211_F_SHSLOT))
        capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
    if (ieee80211_ic_doth_is_set(ic) && ieee80211_vap_doth_is_set(vap))
        capinfo |= IEEE80211_CAPINFO_SPECTRUM_MGMT;

	/* TODO: Add capinfo as IE */
    //*(u_int16_t *)frm = htole16(capinfo);
    //frm += 2;

    frm = ieee80211_add_rates(frm, &ni->ni_rates);
    frm = ieee80211_add_xrates(frm, &ni->ni_rates);

    /*
     * XXX: don't do WMM association with safe mode enabled, because
     * Vista OS doesn't know how to decrypt QoS frame.
     */
    if (ieee80211_vap_wme_is_set(vap) &&
        (ni->ni_ext_caps & IEEE80211_NODE_C_QOS) &&
        !IEEE80211_VAP_IS_SAFEMODE_ENABLED(vap)) {
        struct ieee80211_wme_tspec *sigtspec = &ni->ni_ic->ic_sigtspec;
        struct ieee80211_wme_tspec *datatspec = &ni->ni_ic->ic_datatspec;

        frm = ieee80211_add_wmeinfo(frm, ni, WME_INFO_OUI_SUBTYPE, NULL, 0);
        if (iswmetspec((u_int8_t *)sigtspec))
            frm = ieee80211_add_wmeinfo(frm, ni, WME_TSPEC_OUI_SUBTYPE, 
                                        &sigtspec->ts_tsinfo[0], 
                                        sizeof (struct ieee80211_wme_tspec) - offsetof(struct ieee80211_wme_tspec, ts_tsinfo));
            
        if (iswmetspec((u_int8_t *)datatspec))
            frm = ieee80211_add_wmeinfo(frm, ni, WME_TSPEC_OUI_SUBTYPE, 
                                        &datatspec->ts_tsinfo[0], 
                                        sizeof (struct ieee80211_wme_tspec) - offsetof(struct ieee80211_wme_tspec, ts_tsinfo));
                    
    }

    frm = ieee80211_add_htcap(frm, ni, subtype);

    if (!IEEE80211_IS_HTVIE_ENABLED(ic)) {
        frm = ieee80211_add_htcap_pre_ana(frm, ni, IEEE80211_FC0_SUBTYPE_PROBE_RESP);
    } 
    else {
        frm = ieee80211_add_htcap_vendor_specific(frm, ni, subtype);
    }

    /* 20/40 BSS Coexistence */
    if (!(ic->ic_flags & IEEE80211_F_COEXT_DISABLE)) {
        frm = ieee80211_add_extcap(frm, ni);
    }

    /* Insert ieee80211_ie_ath_extcap IE to beacon */
    if ((ni->ni_flags & IEEE80211_NODE_ATH) &&
        ic->ic_ath_extcap) {
        u_int16_t ath_extcap = 0;
        u_int8_t  ath_rxdelim = 0;
        
        if (ieee80211_has_weptkipaggr(ni)) {
                ath_extcap |= IEEE80211_ATHEC_WEPTKIPAGGR;
                ath_rxdelim = ic->ic_weptkipaggr_rxdelim;
        }

        if ((ni->ni_flags & IEEE80211_NODE_OWL_WDSWAR) &&
            (ic->ic_ath_extcap & IEEE80211_ATHEC_OWLWDSWAR)) {
                ath_extcap |= IEEE80211_ATHEC_OWLWDSWAR;
        }

        if (ieee80211com_has_extradelimwar(ic))
            ath_extcap |= IEEE80211_ATHEC_EXTRADELIMWAR;

        frm = ieee80211_add_athextcap(frm, ath_extcap, ath_rxdelim);
    }

#ifdef notyet
    if (ni->ni_ath_ie != NULL)
        frm = ieee80211_add_ath(frm, ni);
#endif

    add_wpa_ie=true;
    /*
     * check if os shim has setup RSN IE it self.
     */
    IEEE80211_VAP_LOCK(vap);
    if (vap->iv_app_ie[IEEE80211_FRAME_TYPE_ASSOCREQ].length) {
        add_wpa_ie = ieee80211_check_wpaie(vap, vap->iv_app_ie[IEEE80211_FRAME_TYPE_ASSOCREQ].ie,
                                           vap->iv_app_ie[IEEE80211_FRAME_TYPE_ASSOCREQ].length);
    }
    if (vap->iv_opt_ie.length) {
        add_wpa_ie = ieee80211_check_wpaie(vap, vap->iv_opt_ie.ie,
                                           vap->iv_opt_ie.length);
    }
    IEEE80211_VAP_UNLOCK(vap);

    if (add_wpa_ie) {
        if (RSN_AUTH_IS_RSNA(rsn)) {
            frm = ieee80211_setup_rsn_ie(vap, frm);
        }
    }

#if ATH_SUPPORT_WAPI
    if (RSN_AUTH_IS_WAI(rsn))
        frm = ieee80211_setup_wapi_ie(vap, frm);
#endif

    /*
     * add wpa/rsn ie if os did not setup one.
     */
    if (add_wpa_ie) {
        if (RSN_AUTH_IS_WPA(rsn))
            frm = ieee80211_setup_wpa_ie(vap, frm);

        if (RSN_AUTH_IS_CCKM(rsn)) {
            ASSERT(!RSN_AUTH_IS_RSNA(rsn) && !RSN_AUTH_IS_WPA(rsn));
        
            /*
             * CCKM AKM can be either added to WPA IE or RSN IE,
             * depending on the AP's configuration
             */
            if (RSN_AUTH_IS_RSNA(&ni->ni_rsn))
                frm = ieee80211_setup_rsn_ie(vap, frm);
            else if (RSN_AUTH_IS_WPA(&ni->ni_rsn))
                frm = ieee80211_setup_wpa_ie(vap, frm);
        }
    }

    IEEE80211_VAP_LOCK(vap);
    if (vap->iv_opt_ie.length){
        OS_MEMCPY(frm, vap->iv_opt_ie.ie,
                  vap->iv_opt_ie.length);
        frm += vap->iv_opt_ie.length;
    }
    if (vap->iv_app_ie[IEEE80211_FRAME_TYPE_ASSOCREQ].length){
        OS_MEMCPY(frm, vap->iv_app_ie[IEEE80211_FRAME_TYPE_ASSOCREQ].ie,
                  vap->iv_app_ie[IEEE80211_FRAME_TYPE_ASSOCREQ].length);
        frm += vap->iv_app_ie[IEEE80211_FRAME_TYPE_ASSOCREQ].length;
    }
    IEEE80211_VAP_UNLOCK(vap);

    return (frm - (u_int8_t *)wh);
}

/* Make frame hdrs */
static uint8_t * 
tdls_mgmt_frame(struct ieee80211vap *vap, uint8_t type, 
                                  void *arg, wbuf_t *wbuf)
{
    uint8_t *frm=NULL;

    /* *skb = ieee80211_getmgtframe(&frm, sizeof(struct ieee80211_tdls_frame) 
                                       + frame_length[type]+2, 
                                 sizeof(struct ether_header)); */

    *wbuf = wbuf_alloc(vap->iv_ic->ic_osdev, WBUF_TX_MGMT, 
    		sizeof(struct ieee80211_tdls_frame) + frame_length[type]+2);

    if (!*wbuf) {
        KASSERT(wbuf != NULL, ("FATAL:wbuf NULL!"));
    }
	frm = wbuf_raw_data(*wbuf);
	
    /* Fill the LLC/SNAP header and set ethertype to TDLS ether type */
    IEEE80211_TDLS_LLC_HDR_SET(frm); /* nothing happens here, its dummy as of now */

    /* Fill the TDLS header */
    IEEE80211_TDLS_HDR_SET(frm, type, IEEE80211_TDLS_LNKID_LEN, vap, arg);

    return frm;
};

/* Send tdls mgmt frame */
static uint8_t 
tdls_send_mgmt_frame(struct ieee80211com *ic, wbuf_t wbuf)
{
    struct ieee80211_node *ni = wbuf_get_node(wbuf);
    struct ieee80211vap *vap  = ni->ni_vap;
    uint8_t ret=0;

    IEEE80211_NODE_STAT(ni, tx_data);

    wbuf_set_priority(wbuf, WME_AC_VI);
    wbuf_set_tid(wbuf, WME_AC_TO_TID(WME_AC_VI));
    /* ni->ni_flags |= N_TDLS; */ /*TODO: to make ath_dev use this flag */
    ret = vap->iv_evtable->wlan_dev_xmit_queue(vap->iv_ifp, wbuf);
	
    if (ret) ieee80211_free_node(ni);

    return ret; 
}

/* Create the Wireless ARP message for WDS and send it out to all the
 * TDLS nodes */
static int 
tdls_send_arp(struct ieee80211com *ic, struct ieee80211vap *vap, 
                  void *arg, uint8_t *buf, uint16_t buf_len)
{
    wbuf_t wbuf;
    uint8_t              *frm;
    struct ieee80211_node *ni = vap->iv_bss;
    uint8_t  bcast_addr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    uint16_t               len=0;

    /* If TDLS not enabled, don't send any ARP messages */
    if (!(vap->iv_ath_cap & IEEE80211_ATHC_TDLS)) {
        printk("TDLS Disabled \n");
        return 0;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "%s, Sending out WDS ARP for %s "
                      "behind me\n", __FUNCTION__, ether_sprintf(arg));
 
    frm = tdls_mgmt_frame(vap, IEEE80211_TDLS_LEARNED_ARP, bcast_addr,
                              &wbuf);
    if (!frm) {
       printk("frm returns NULL\n");
       return -ENOMEM;
    }

	len = sizeof(struct ieee80211_tdls_frame);

    IEEE80211_ADDR_COPY(frm, arg);
    len += 6;

    N_NODE_SET(wbuf, ieee80211_ref_node(ni));

    wbuf_set_pktlen(wbuf, len);

    IEEE80211_TDLS_WHDR_SET(wbuf, vap, bcast_addr);

	/* temporary to be removed later */
	dump_pkt((wbuf->data), wbuf->len);

    tdls_send_mgmt_frame(ic, wbuf);

    return 0;
}

/* TDLS setup request */
static int 
tdls_send_setup_req(struct ieee80211com *ic, struct ieee80211vap *vap,
                        void* arg, uint8_t *buf, uint16_t buf_len)
{
    struct ieee80211_node *ni = vap->iv_bss, *ni_tdls;
    uint8_t              *frm;
    uint8_t                wpa;
    uint8_t                _token = 0;
    uint16_t               len=0;

    wbuf_t wbuf;


    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "%s, SETUP Message to %s\n", __FUNCTION__, 
                      ether_sprintf(arg));

    ni_tdls = ieee80211_find_node(&vap->iv_ic->ic_sta, arg);

    if (!ni_tdls) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,"%s,Node not present:%s\n",
                          __FUNCTION__, ether_sprintf(arg));
		return 0;
   }

   ni_tdls->ni_tdls->state = IEEE80211_TDLS_S_SETUP_REQ_INPROG;
   ni_tdls->ni_tdls->stk_st = 0;
   ieee80211_free_node(ni_tdls); /* for above find */

   frm = tdls_mgmt_frame(vap, IEEE80211_TDLS_SETUP_REQ, arg, &wbuf);

   //IEEE80211_TDLS_SET_STATUS(frm, vap, arg, _status); /* Not for REQ */
   IEEE80211_TDLS_SET_TOKEN(frm, vap, arg, _token);
   IEEE80211_TDLS_SET_LINKID(frm, vap, arg); // vicks
   
   if (!frm) {
       IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "%s, "
       						"unable to allocate wbuf\n", __FUNCTION__);
       return -ENOMEM;
   }

   /* Fill the TDLS information */
    len = ieee80211_setup_assoc(ni, (struct ieee80211_frame *)frm, 0,
                            vap->iv_bss->ni_bssid);

	frm += len;

	len += sizeof(struct ieee80211_tdls_frame) +
	      sizeof(struct ieee80211_tdls_token)  +
	      sizeof(struct ieee80211_tdls_lnk_id);
	
	/* temporary to be removed later */
	dump_pkt(wbuf->data, len);
    
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "SEND_TDLS_REQ: hdr_len=%d \n", len);
    if((wpa=get_wpa_mode(vap))) {
        if(buf && buf_len) {
            memcpy(frm, buf, buf_len);
            len += buf_len;
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, ", smk_len=%d.\n", buf_len);
        }
    }

    IEEE80211_NODE_STAT(ni, tx_data);

    N_NODE_SET(wbuf, ieee80211_ref_node(ni));

    wbuf_set_pktlen(wbuf, len);

    IEEE80211_TDLS_WHDR_SET(wbuf, vap, arg);

	/* temporary to be removed later */
	dump_pkt((wbuf->data), wbuf->len);

    tdls_send_mgmt_frame(ic, wbuf);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "SEND_TDLS_REQ: successfully sent ... \n", len);

    return 0;
}

/* TDLS setup response */
static int 
tdls_send_setup_resp(struct ieee80211com *ic, struct ieee80211vap *vap,
                        void* arg, uint8_t *buf, uint16_t buf_len)
{
    struct ieee80211_node *ni = vap->iv_bss, *ni_tdls=NULL;
    wbuf_t wbuf;
    uint8_t              *frm=NULL;
    uint8_t               wpa;
    uint8_t                _status = 0;
    uint16_t               len=0;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: SETUP RESPONSE Message to %s\n",
                      ether_sprintf(arg));

    ni_tdls = ieee80211_find_node(&vap->iv_ic->ic_sta, arg);

    if (!ni_tdls) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,"%s,Node not present:%s\n",
                          __FUNCTION__, ether_sprintf(arg));
		return 0;
    }

    ni_tdls->ni_tdls->state = IEEE80211_TDLS_S_SETUP_RESP_INPROG;

    ieee80211_free_node(ni_tdls); /* for above find */

    frm = tdls_mgmt_frame(vap, IEEE80211_TDLS_SETUP_RESP, arg, &wbuf);

   IEEE80211_TDLS_SET_STATUS(frm, vap, arg, _status);
   IEEE80211_TDLS_SET_TOKEN(frm, vap, arg, ni_tdls->ni_tdls->token);
   IEEE80211_TDLS_SET_LINKID(frm, vap, arg);
   
    if (!frm) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "%s, "
     			"unable to allocate wbuf\n", __FUNCTION__);
        return -ENOMEM;
    }

   /* Fill the TDLS information */
    len += ieee80211_setup_assoc(ni, (struct ieee80211_frame *)(frm+len), 0,
                            vap->iv_bss->ni_bssid);

	frm += len;

	len += sizeof(struct ieee80211_tdls_frame) +
	      sizeof(struct ieee80211_tdls_status) +
	      sizeof(struct ieee80211_tdls_token)  +
	      sizeof(struct ieee80211_tdls_lnk_id);

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "SEND_TDLS_RESP: hdr_len=%d \n", len);

	/* temporary to be removed later */
	dump_pkt(wbuf->data, len);
                             
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "SEND_TDLS_RESP: hdr_len=%d", len);
    if((wpa=get_wpa_mode(vap))) {
        if(buf && buf_len) {
            memcpy(frm, buf, buf_len);
            len += buf_len;
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, ", smk_len=%d.\n", buf_len);
        }
    }

    IEEE80211_NODE_STAT(ni, tx_data);

    N_NODE_SET(wbuf, ieee80211_ref_node(ni));

    wbuf_set_pktlen(wbuf, len);

    IEEE80211_TDLS_WHDR_SET(wbuf, vap, arg);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "SEND_TDLS_RESP: successfully sent ... \n", len);

	/* temporary to be removed later */
	dump_pkt((wbuf->data), wbuf->len);

    tdls_send_mgmt_frame(ic, wbuf);

    return 0;
}

/* Send TDLS setup confirm */
static int 
tdls_send_setup_confirm(struct ieee80211com *ic, struct ieee80211vap *vap,
                            void* arg, uint8_t *buf, uint16_t buf_len)
{
    struct ieee80211_node *ni = vap->iv_bss, *ni_tdls=NULL;
    wbuf_t wbuf;
    uint8_t              *frm=NULL;
    uint8_t                wpa;
    uint8_t                _status = 0;
    uint16_t               len=0;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: SETUP CONFIRM Message to %s\n",
                      ether_sprintf(arg));

    ni_tdls = ieee80211_find_node(&vap->iv_ic->ic_sta, arg);

    if (!ni_tdls) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,"%s,Node not present:%s\n",
                          __FUNCTION__, ether_sprintf(arg));
		return 0;
    }

    ni_tdls->ni_tdls->state = IEEE80211_TDLS_S_RUN;

    ieee80211_free_node(ni_tdls); /* for above find */

    frm = tdls_mgmt_frame(vap, IEEE80211_TDLS_SETUP_CONFIRM, arg, &wbuf);

    if (!frm) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "%s, "
        					"unable to allocate wbuf\n", __FUNCTION__);
        return -ENOMEM;
    }

    IEEE80211_TDLS_SET_STATUS(frm, vap, arg, _status);
    IEEE80211_TDLS_SET_TOKEN(frm, vap, arg, ni_tdls->ni_tdls->token);
    IEEE80211_TDLS_SET_LINKID(frm, vap, arg);

    /* TODO: Add EDCA parameter set IE */
    /* TODO: Add HT Operation set IE */
    /* TODO: update len suitably */

	len = sizeof(struct ieee80211_tdls_frame)  +
	      sizeof(struct ieee80211_tdls_status) +
	      sizeof(struct ieee80211_tdls_token)  +
	      sizeof(struct ieee80211_tdls_lnk_id);

	/* temporary to be removed later */
	dump_pkt(wbuf->data, len);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "SEND_TDLS_CONFIRM: hdr_len=%d", len);
    if((wpa=get_wpa_mode(vap))) {
        if(buf && buf_len) {
            memcpy(frm, buf, buf_len);
            len += buf_len;
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, ", smk_len=%d.\n", buf_len);
        }
    }
    
    IEEE80211_NODE_STAT(ni, tx_data);

    wbuf_set_pktlen(wbuf, len);

    N_NODE_SET(wbuf, ieee80211_ref_node(ni));

    IEEE80211_TDLS_WHDR_SET(wbuf, vap, arg);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "SEND_TDLS_CONFIRM: successfully sent ... \n", len);

	/* temporary to be removed later */
	dump_pkt((wbuf->data), wbuf->len);

    tdls_send_mgmt_frame(ic, wbuf);

#ifdef CONFIG_RCPI
    ieee80211_tdls_link_rcpi_req(ni->ni_vap, ni->ni_bssid, arg);
    ni_tdls->ni_tdls->link_st = 1;
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, ", Setting the Path to %d(TDLS link)\n",
                      ni_tdls->ni_tdls->link_st);
        //IEEE80211_TDLSRCPI_PATH_SET(ni);
#endif

    return 0;
}

/* TDLS teardown send request */
static int 
tdls_send_teardown_req(struct ieee80211com *ic, struct ieee80211vap *vap,
                           void* arg, uint8_t *buf, uint16_t buf_len)
{
    struct ieee80211_node *ni = vap->iv_bss, *ni_tdls=NULL;
    wbuf_t 				  wbuf;
    uint8_t              *frm=NULL;
    uint8_t                wpa;
    uint8_t                _reason = 0;
    uint16_t               len=0;

    ni_tdls = ieee80211_find_node(&vap->iv_ic->ic_sta, arg);

    /* Node is already deleted or teardown in progress just return */
    if (!ni_tdls || (ni_tdls->ni_tdls && 
        (ni_tdls->ni_tdls->state == IEEE80211_TDLS_S_TEARDOWN_REQ_INPROG ||
         ni_tdls->ni_tdls->state == IEEE80211_TDLS_S_TEARDOWN_RESP_INPROG))) {
         if (ni_tdls) ieee80211_free_node(ni_tdls);
        return 0;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: TEARDOWN REQ Message to %s\n",
                      ether_sprintf(arg));

    frm = tdls_mgmt_frame(vap, IEEE80211_TDLS_TEARDOWN_REQ, arg, &wbuf);

    if (!frm) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "%s, "
        					"unable to allocate wbuf\n", __FUNCTION__);
        return -ENOMEM;
    }

    IEEE80211_TDLS_SET_STATUS(frm, vap, arg, _reason);
    IEEE80211_TDLS_SET_LINKID(frm, vap, arg);

	len = sizeof(struct ieee80211_tdls_frame)  +
	      sizeof(struct ieee80211_tdls_status) +
	      sizeof(struct ieee80211_tdls_lnk_id);

    wbuf_set_pktlen(wbuf, len);

    N_NODE_SET(wbuf, ieee80211_ref_node(ni));

    IEEE80211_TDLS_WHDR_SET(wbuf, vap, arg);

    if((wpa=get_wpa_mode(vap))) /* XXX: what to send up? */
      tdls_notify_supplicant(vap, arg, IEEE80211_TDLS_TEARDOWN_REQ, NULL);

    /* Notification to TdlsDiscovery process on TearDownReq */
    tdls_notify_teardownreq(vap, arg, IEEE80211_TDLS_TEARDOWN_REQ, NULL);

	/* temporary to be removed later */
	dump_pkt((wbuf->data), wbuf->len);

    tdls_send_mgmt_frame(ic, wbuf);

    ni_tdls->ni_tdls->state = IEEE80211_TDLS_S_TEARDOWN_REQ_INPROG;

#ifdef CONFIG_RCPI
    ni_tdls->ni_tdls->link_st = 0;
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, ", Setting the Path to %d(AP link)\n",
                      ni_tdls->ni_tdls->link_st);
#endif

    ieee80211_free_node(ni_tdls);
    ieee80211_tdls_del_node(ni_tdls);
    return 0;
}

static int 
tdls_send_teardown_resp(struct ieee80211com *ic, struct ieee80211vap *vap,
                                void* arg, uint8_t *buf, uint16_t buf_len)
{
    struct ieee80211_node *ni = vap->iv_bss, *ni_tdls=NULL;
    wbuf_t 				  wbuf;
    uint8_t              *frm=NULL;
    uint16_t               len=0;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: TEARDOWN RESPONSE Message to %s\n",
                      ether_sprintf(arg));

    frm = tdls_mgmt_frame(vap, IEEE80211_TDLS_TEARDOWN_RESP, arg, &wbuf);

    if (!frm) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "%s, "
    			"unable to allocate wbuf\n", __FUNCTION__);
        return -ENOMEM;
    }

	len = sizeof(struct ieee80211_tdls_frame);

    wbuf_set_pktlen(wbuf, len);

    N_NODE_SET(wbuf, ieee80211_ref_node(ni));

    IEEE80211_TDLS_WHDR_SET(wbuf, vap, arg);

	/* temporary to be removed later */
	dump_pkt((wbuf->data), wbuf->len);

    tdls_send_mgmt_frame(ic, wbuf);

    ni_tdls = ieee80211_find_node(&ic->ic_sta, arg);

    if (ni_tdls) {
        ieee80211_free_node(ni_tdls); /* For above find */
        ieee80211_tdls_del_node(ni_tdls);
    }

    return 0;
}

#ifdef CONFIG_RCPI

/* TDLS Tx Switch send request */
static
int tdls_send_txpath_switch_req(struct ieee80211com *ic, struct ieee80211vap *vap,
                           void* arg, u_int8_t *buf, u_int16_t buf_len)
{
    struct ieee80211_node *ni = vap->iv_bss, *ni_tdls=NULL;
    wbuf_t        wbuf;
    u_int8_t     *frm=NULL;

    ni_tdls = ieee80211_find_node(&vap->iv_ic->ic_sta, arg);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: Tx Switch REQ Message to %s\n",
                      ether_sprintf(arg));
    frm = tdls_mgmt_frame(vap, IEEE80211_TDLS_TXPATH_SWITCH_REQ, arg, &wbuf);
    if (!frm) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s, "
                                                "unable to allocate wbuf\n", __FUNCTION__);
        return -ENOMEM;
    }
   IEEE80211_TDLS_SET_LINKID(frm, vap, arg);
   /* Status code (2 bytes) */
   *(u_int16_t *)frm = 0;
   frm += 2;
        /* Path  : The Path element contains the requested transmit path 
         *         0 - AP_path
         *         1 - TDLS_path
         * Reason: 0 - Unspecified
         *         1 - Change in power save mode 
         *         2 - Change in link state
         */
    memcpy(frm, buf, buf_len);
    frm += buf_len;
    wbuf_set_pktlen(wbuf, frm - (u_int8_t *)wbuf_raw_data(wbuf));
    N_NODE_SET(wbuf, ieee80211_ref_node(ni));
    IEEE80211_TDLS_WHDR_SET(wbuf, vap, arg);
    tdls_send_mgmt_frame(ic, wbuf);
    ni_tdls->ni_tdls->state = IEEE80211_TDLS_S_TXSWITCH_REQ_INPROG;
    ieee80211_free_node(ni_tdls); /* For above find */
    return 0;
}

/* TDLS Tx Switch send response */
static
int tdls_send_txpath_switch_resp(struct ieee80211com *ic, struct ieee80211vap *vap,
                           void* arg, u_int8_t *buf, u_int16_t buf_len)
{
    struct ieee80211_node *ni = vap->iv_bss, *ni_tdls=NULL;
    wbuf_t        wbuf;
    u_int8_t      *frm=NULL;
    ni_tdls = ieee80211_find_node(&vap->iv_ic->ic_sta, arg);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: Tx Switch RESP Message to %s\n",
                      ether_sprintf(arg));
    frm = tdls_mgmt_frame(vap, IEEE80211_TDLS_TXPATH_SWITCH_RESP, arg, &wbuf);
    if (!frm) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s, "
                                                "unable to allocate wbuf\n", __FUNCTION__);
        return -ENOMEM;
    }
   IEEE80211_TDLS_SET_LINKID(frm, vap, arg);
   if (!frm) {
       IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s, "
       						"unable to allocate wbuf\n", __FUNCTION__);
       return -ENOMEM;
   }
    /* Status code (2 bytes) */
   *(u_int16_t *)frm = 0;
   frm += 2;
        /* Path  :  The Path element echoes the requested transmit transmit path.
         * Reason:  0 - Accept
         *          1 - Reject because of entering power save mode
         *          2 - Reject because of the link status
         *          3 - Reject because of unspecified reason
         */
    memcpy(frm, buf, buf_len);
    frm += buf_len;
    wbuf_set_pktlen(wbuf, frm - (u_int8_t *)wbuf_raw_data(wbuf));
    N_NODE_SET(wbuf, ieee80211_ref_node(ni));
    IEEE80211_TDLS_WHDR_SET(wbuf, vap, arg);

#if 0    
	/* temporary to be removed later */
	dump_pkt((wbuf->data), wbuf->len);
#endif
    tdls_send_mgmt_frame(ic, wbuf);
    ni_tdls->ni_tdls->state = IEEE80211_TDLS_S_TXSWITCH_RESP_INPROG;
    ieee80211_free_node(ni_tdls);
    return 0;
}

/* TDLS Rx Switch send request */
static
int tdls_send_rxpath_switch_req(struct ieee80211com *ic, struct ieee80211vap *vap,
                           void* arg, u_int8_t *buf, u_int16_t buf_len)
{
   struct ieee80211_node *ni = vap->iv_bss, *ni_tdls=NULL;
   wbuf_t        wbuf;
   u_int8_t      *frm=NULL;

   ni_tdls = ieee80211_find_node(&vap->iv_ic->ic_sta, arg);
   IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: Rx Switch REQ Message to %s\n",
                      ether_sprintf(arg));
   frm = tdls_mgmt_frame(vap, IEEE80211_TDLS_RXPATH_SWITCH_REQ, arg, &wbuf);
   if (!frm) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s, "
                         "unable to allocate wbuf\n", __FUNCTION__);
        return -ENOMEM;
   }
   IEEE80211_TDLS_SET_LINKID(frm, vap, arg);
   
   /* Status code (2 bytes) */
   *(u_int16_t *)frm = 0;
   frm += 2;
        /* Path  : The Path element contains the requested receive path 
         *         0 - AP_path
         *         1 - TDLS_path
         * Reason: 0 - Unspecified
         *         1 - Change in power save mode 
         *         2 - Change in link state
         */
    memcpy(frm, buf, buf_len);
    frm += buf_len;
    wbuf_set_pktlen(wbuf, frm - (u_int8_t *)wbuf_raw_data(wbuf));
    N_NODE_SET(wbuf, ieee80211_ref_node(ni));
    IEEE80211_TDLS_WHDR_SET(wbuf, vap, arg);
    
    tdls_send_mgmt_frame(ic, wbuf);
    ni_tdls->ni_tdls->state = IEEE80211_TDLS_S_RXSWITCH_REQ_INPROG;
    ieee80211_free_node(ni_tdls);
    
    return 0;
}

/* TDLS Rx Switch send response */
static
int tdls_send_rxpath_switch_resp(struct ieee80211com *ic, struct ieee80211vap *vap,
                           void* arg, u_int8_t *buf, u_int16_t buf_len)
{
    struct ieee80211_node *ni = vap->iv_bss, *ni_tdls=NULL;
    wbuf_t        wbuf;
    u_int8_t      *frm=NULL;

    ni_tdls = ieee80211_find_node(&vap->iv_ic->ic_sta, arg);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: Rx Switch RESP Message to %s\n",
                      ether_sprintf(arg));

    frm = tdls_mgmt_frame(vap, IEEE80211_TDLS_RXPATH_SWITCH_RESP, arg, &wbuf);
    if (!frm) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s, "
                                                "unable to allocate wbuf\n", __FUNCTION__);
        return -ENOMEM;
    }

   IEEE80211_TDLS_SET_LINKID(frm, vap, arg);
   if (!frm) {
       IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s, "
       						"unable to allocate wbuf\n", __FUNCTION__);
       return -ENOMEM;
   }

    /* Status code (2 bytes) */
    *(u_int16_t *)frm = 0;
    frm += 2;

        /* Path  :  The Path element echoes the requested transmit transmit path.
         */
    memcpy(frm, buf, buf_len);
    frm += buf_len;
    wbuf_set_pktlen(wbuf, frm - (u_int8_t *)wbuf_raw_data(wbuf));
    N_NODE_SET(wbuf, ieee80211_ref_node(ni));
    IEEE80211_TDLS_WHDR_SET(wbuf, vap, arg);
    tdls_send_mgmt_frame(ic, wbuf);
    ni_tdls->ni_tdls->state = IEEE80211_TDLS_S_RXSWITCH_RESP_INPROG;
    ieee80211_free_node(ni_tdls); /* For above find */

    return 0;
}

/* TDLS Link RCPI Request */
static
int tdls_send_link_rcpi_req(struct ieee80211com *ic, struct ieee80211vap *vap,
                           void* arg, u_int8_t *buf, u_int16_t buf_len, u_int8_t link)
{
    struct ieee80211_node *ni = vap->iv_bss, *ni_tdls=NULL;
    wbuf_t        wbuf;
    u_int8_t      *frm=NULL;

    ni_tdls = ieee80211_find_node(&ic->ic_sta, arg);
    frm = tdls_mgmt_frame(vap, IEEE80211_TDLS_LINKRCPI_REQ, arg, &wbuf);
    if (!frm) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "%s, "
                                                "unable to allocate wbuf\n", __FUNCTION__);
        return -ENOMEM;
    }

    IEEE80211_TDLS_SET_LINKID(frm, vap, arg);
    /* Status code (2 bytes) */
    *(u_int16_t *)frm = 0;
    frm += 2;
    memcpy(frm, buf, buf_len);
    frm += buf_len;
    wbuf_set_pktlen(wbuf, frm - (u_int8_t *)wbuf_raw_data(wbuf));

    /* BSSID :       To get AP's RSSI value of Peer Station */
    /* STA Address: To get Direct Link RSSI value of Peer Station */
    if(link)
        N_NODE_SET(wbuf, ieee80211_ref_node(ni_tdls));
    else
        N_NODE_SET(wbuf, ieee80211_ref_node(ni));

    IEEE80211_TDLS_WHDR_SET(wbuf, vap, arg);
    tdls_send_mgmt_frame(ic, wbuf);
    ni_tdls->ni_tdls->state = IEEE80211_TDLS_S_LINKRCPI_REQ_INPROG;
    ieee80211_free_node(ni_tdls); /* For above find */

    return 0;
}

/* TDLS Link RCPI report */
static
int tdls_send_link_rcpi_report(struct ieee80211com *ic, struct ieee80211vap *vap,
                           void* arg, u_int8_t *buf, u_int16_t buf_len)
{
    struct ieee80211_node *ni = vap->iv_bss, *ni_tdls=NULL;
    wbuf_t        wbuf;
    u_int8_t      *frm=NULL;

    ni_tdls = ieee80211_find_node(&ic->ic_sta, arg);
    
    frm = tdls_mgmt_frame(vap, IEEE80211_TDLS_LINKRCPI_REPORT, arg, &wbuf);
    if (!frm) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "%s, "
                                                "unable to allocate wbuf\n", __FUNCTION__);
        return -ENOMEM;
    }
    IEEE80211_TDLS_SET_LINKID(frm, vap, arg);
    /* Status code (2 bytes) */
    *(u_int16_t *)frm = 0;
    frm += 2;

    /* BSSID:         To get AP's RCPI value */
    /* RCPI_1 value   For frames from AP in dB*/
    /* STA Address:   To get Direct Link RCPI value */
    /* RCPI_2 value   For frames thro Direct Link in dB*/
    memcpy(frm, buf, buf_len);
    frm += buf_len;

    wbuf_set_pktlen(wbuf, frm - (u_int8_t *)wbuf_raw_data(wbuf));
    N_NODE_SET(wbuf, ieee80211_ref_node(ni));
    IEEE80211_TDLS_WHDR_SET(wbuf, vap, arg);
    tdls_send_mgmt_frame(ic, wbuf);
    ni_tdls->ni_tdls->state = IEEE80211_TDLS_S_LINKRCPI_REPORT_INPROG;
    ieee80211_free_node(ni_tdls); /* For above find */

    return 0;
}

/* TDLS Tx Switch request */
static 
int tdls_recv_txpath_switch_req(struct ieee80211com *ic,
                           struct ieee80211_node *ni, void *arg, u16 dummy)
{
    int ret = 0; 
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_tdls_frame *tf = (struct ieee80211_tdls_frame *)arg; 

    struct ieee80211_tdls_lnk_id * linkid = NULL;
    struct ieee80211_tdls_rcpi_switch *txreq = NULL;
    struct ieee80211_tdls_rcpi_switch txresp;
    struct ieee80211_node *ni_ap    = NULL;

	u_int8_t    path;
	u_int8_t    reason;
	u_int8_t    hithreshold;
	u_int8_t    lothreshold;
	u_int8_t    margin;
	u_int8_t    link_st;
	u_int8_t    ap_rssi  = 0;
	u_int8_t    sta_rssi = 0;
	int         diff;

    linkid = (struct ieee80211_tdls_lnk_id *)(tf+1);
    txreq = (struct ieee80211_tdls_rcpi_switch*) (((char*)(linkid+1))+2);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: TXSWITCH REQ Message from %s to switch to \n",
                      ether_sprintf(linkid->sa));

    ni->ni_tdls->state = IEEE80211_TDLS_S_TXSWITCH_REQ_INPROG;
    
    /* process_the_request_suitably_to_send_response */
	/* Path  : The Path element contains the requested transmit path 
	 *  	   0 - AP_path
	 *  	   1 - TDLS_path
	 * Reason: 0 - Unspecified
	 * 	       1 - Change in power save mode 
	 * 	       2 - Change in link state
	 */

	path   = txreq->path.path;
	reason = txreq->reason;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, " new link=%s, reason=%d "
                      " current link=%d\n", path == 0? "AP Path":"TDLS Path",
                      reason, ni->ni_tdls->link_st);

	hithreshold 	= 	ic->ic_tdls->hithreshold;
	lothreshold 	= 	ic->ic_tdls->lothreshold;
	margin 		    = 	ic->ic_tdls->margin;

	ni_ap 			= 	ni->ni_vap->iv_bss;
	ap_rssi 		= 	(ATH_NODE_NET80211(ni_ap)->an_avgrssi);

	link_st			= 	ni->ni_tdls->link_st;
	sta_rssi 		= 	(ATH_NODE_NET80211(ni)->an_avgrssi);
	diff        	= 	ap_rssi - sta_rssi;

	memset(&txresp, 0, sizeof(txresp));
	memcpy(&txresp, txreq, sizeof(txresp));

    if(reason == REASON_CHANGE_IN_LINK_STATE) 
    {
        /* check if reason is OK to switch */
		/* received REQ for switch to Direct Path */
		if(path && (link_st == AP_PATH) && (sta_rssi > hithreshold))  
        {
            IEEE80211_TDLSRCPI_PATH_SET(ni);
			txresp.reason = ACCEPT;
		    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, 
		                    "TDLS: Switching to Direct Path: %s, TDLS link rssi=%d, "
                            "AP link rssi=%d, hiT=%d\n", ether_sprintf(linkid->sa), 
                            sta_rssi, ap_rssi, hithreshold);
         }
			/* received REQ for switch to AP Path */
	     else if(!path && (link_st == DIRECT_PATH) && (sta_rssi < lothreshold) && (diff > margin))  
         {

			IEEE80211_TDLSRCPI_PATH_RESET(ni);
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, 
			                   "TDLS: Switching to AP Path: %s, TDLS link rssi=%d, "
                                "AP link rssi=%d, diff=%d, margin=%d\n", ether_sprintf(linkid->sa), 
                                 sta_rssi, ap_rssi, diff, margin);
			txresp.reason = ACCEPT;
		  } 
          else 
	  	  /* REJECT request */
          {
			 IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, 
                			    "TDLS: Rejecting the path switch request: %s TDLS link rssi=%d,"
                                " AP link rssi=%d, diff=%d, margin=%d\n", ether_sprintf(linkid->sa), 
             sta_rssi, ap_rssi, diff, margin);
			 txresp.reason = REJECT_LINK_STATUS;
          }
				
	}
    /* Send response to Peer node suitably */
 	ret = tdls_send_txpath_switch_resp(ic, vap, linkid->sa, (u_int8_t*)&txresp, sizeof(txresp));

    return ret;
}

/* TDLS Tx Switch response */
static 
int tdls_recv_txpath_switch_resp(struct ieee80211com *ic,
                           struct ieee80211_node *ni, void *arg, u16 dummy)
{
    int ret = 0; 
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_tdls_frame *tf = (struct ieee80211_tdls_frame *)arg; 

    struct ieee80211_tdls_lnk_id * linkid = NULL;
	struct ieee80211_tdls_rcpi_switch *txresp = NULL;

    linkid = (struct ieee80211_tdls_lnk_id *)(tf+1);
	txresp = (struct ieee80211_tdls_rcpi_switch*)  (((char*)(linkid+1))+2);;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: TXSWITCH RESP Message from %s, reason=%d\n",
                      ether_sprintf(linkid->sa), txresp->reason);
    ni->ni_tdls->state = IEEE80211_TDLS_S_TXSWITCH_RESP_INPROG;
    
    /* process_the_response */
	/* Path  :  The Path element echoes the requested transmit transmit path.
	 * Reason:  0 - Accept
	 *  		1 - Reject because of entering power save mode
	 *  		2 - Reject because of the link status
	 *  		3 - Reject because of unspecified reason
	 */

	if(txresp->reason == ACCEPT) 
    {
		/* make a Switch */
        if(txresp->path.path == DIRECT_PATH) 
        {
    		ni->ni_tdls->link_st = 1;
	    	IEEE80211_TDLSRCPI_PATH_SET(ni);
	        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, 
	                	    "TDLS: Switching to Direct Path: %s\n", ether_sprintf(linkid->sa));
		} 
        else 
        {
		    ni->ni_tdls->link_st = 0;
			IEEE80211_TDLSRCPI_PATH_RESET(ni);
		    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, 
		                    "TDLS: Switching to AP Path: %s\n", ether_sprintf(linkid->sa));
		}
	} 
    else 
    {
		 /* Nothing doing here */
	}
	
    return ret;
}

/* TDLS Rx Switch request */
static 
int tdls_recv_rxpath_switch_req(struct ieee80211com *ic,
                           struct ieee80211_node *ni, void *arg, u16 dummy)
{
    int ret = 0; 
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_tdls_frame *tf = (struct ieee80211_tdls_frame *)arg; 
    struct ieee80211_tdls_lnk_id * linkid = NULL;
    struct ieee80211_tdls_rcpi_switch *rxreq = NULL;
    struct ieee80211_tdls_rcpi_switch rxresp;
    u_int8_t path;
    u_int8_t reason;

    linkid = (struct ieee80211_tdls_lnk_id *)(tf+1);
    rxreq = (struct ieee80211_tdls_rcpi_switch*)  (((char*)(linkid+1))+2);;
    
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: RXSWITCH REQ Message from %s\n",
                      ether_sprintf(linkid->sa));

    ni->ni_tdls->state = IEEE80211_TDLS_S_RXSWITCH_REQ_INPROG;
    
    /* process_the_request_suitably_to_send_response */
	path   = rxreq->path.path;
	reason = rxreq->reason;

	memset(&rxresp, 0, sizeof(rxresp));
	memcpy(&rxresp, rxreq, sizeof(rxresp));
	
	if(reason == REASON_CHANGE_IN_LINK_STATE) 
    {
        if(1) /* TODO:RCPI: check if reason is OK to switch */
            rxresp.reason = ACCEPT;
		else
			rxresp.reason = REJECT_LINK_STATUS;

    }
    else if (reason == REASON_CHANGE_IN_POWERSAVE_MODE) 
    {
		if(1) /* TODO:RCPI: check if reason is OK to switch */
			rxresp.reason = ACCEPT;
		else
			rxresp.reason = REJECT_ENTERING_POWER_SAVE;
				
	} 
    else 
    {   /* REASON_UNSPECIFIED */
		if( 1 ) /* TODO: RCPI: Unspecified reason */
			rxresp.reason = ACCEPT;
		else
			rxresp.reason = REJECT_UNSPECIFIED_REASON;
	}
	/* Send response to Peer node suitably */
    ret = tdls_send_txpath_switch_resp(ic, vap, linkid->sa, 
		    					(u_int8_t*)&rxresp, sizeof(rxresp));
	/* if we are OK to make a switch */
	if(rxresp.reason == ACCEPT) 
    {
	    	if(path)
				; /* TODO:RCPI: switch to Direct Path as requested */
	    	else 
				; /* TODO:RCPI: switch to AP Path as requested */
	}

    return ret;
}

/* TDLS Rx Switch response */
static 
int tdls_recv_rxpath_switch_resp(struct ieee80211com *ic,
                           struct ieee80211_node *ni, void *arg, u16 dummy)
{
    int ret = 0; 
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_tdls_frame *tf = (struct ieee80211_tdls_frame *)arg; 
    struct ieee80211_tdls_lnk_id * linkid = NULL;
    linkid = (struct ieee80211_tdls_lnk_id *)(tf+1);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: RXSWITCH RESP Message from %s\n",
                      ether_sprintf(linkid->sa));

    ni->ni_tdls->state = IEEE80211_TDLS_S_RXSWITCH_RESP_INPROG;
    
	/* TODO:RCPI: make a Switch */
    return ret;
}

/* TDLS Link RCPI Request */
static 
int tdls_recv_link_rcpi_req(struct ieee80211com *ic,
                            struct ieee80211_node *ni, void *arg, u16 dummy)
{
    int ret = 0; 
    IEEE80211_TDLS_RCPI_HDR *tf = (IEEE80211_TDLS_RCPI_HDR *)arg; 
    struct ieee80211_tdls_lnk_id * linkid = NULL;    

	struct ieee80211_tdls_rcpi_link_req *lnk_req = NULL;
	u_int8_t bssid[ETH_ALEN];
	u_int8_t sta_addr[ETH_ALEN];

    linkid = (struct ieee80211_tdls_lnk_id *)(tf+1);
    lnk_req= (struct ieee80211_tdls_rcpi_link_req *) (((char*)(linkid+1))+2);;
    
    ni->ni_tdls->state = IEEE80211_TDLS_S_LINKRCPI_REPORT_INPROG;

    IEEE80211_ADDR_COPY(bssid,    (u_int8_t*)lnk_req->bssid);
    IEEE80211_ADDR_COPY(sta_addr, (u_int8_t*)lnk_req->sta_addr);

#if 0  
    /* No reports etc. right now. send some frame and calculate RSSI 
     * based on recieved frame
     */
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "bssid: %s, ", 
                      ether_sprintf(bssid));
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "sta_addr:%s. \n",
                      ether_sprintf(sta_addr));

	/* TODO:RCPI: Temporarily commented */
	/* Check if BSSID and STA_ADDR are proper and start measurement */
	if( IEEE80211_ADDR_EQ(bssid, ni->ni_bssid) &&
		IEEE80211_ADDR_EQ(sta_addr, vap->iv_myaddr)) {
            int ap_rssi=0, sta_rssi=0;
			/* Send RCPI measurement details */
			ap_rssi = ni->ni_vap->iv_bss->ap_avgrssi;
			sta_rssi = ni->avgrssi;
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
				"TDLS: Link RCPI request: RSSI_AP-%d RSSI_STA-%d\n",
					ap_rssi, sta_rssi);
			/* Need to send report now!!! */
			ieee80211_tdls_link_rcpi_report(vap, bssid, ni->ni_macaddr,
					ap_rssi, sta_rssi);
	} else {
		/* Link RCPI request frame not for us. */
	}
#endif
    return ret;
}

/* TDLS Link RCPI Report */
static 
int tdls_recv_link_rcpi_report(struct ieee80211com *ic,
                            struct ieee80211_node *ni, void *arg, u16 dummy)
{
    int ret = 0; 
    struct ieee80211vap *vap = ni->ni_vap;
    IEEE80211_TDLS_RCPI_HDR *tf = (IEEE80211_TDLS_RCPI_HDR *)arg; 
	struct ieee80211_tdls_rcpi_link_report lnk_rep;
	char * cur_ptr;
	char bssid[ETH_ALEN];
	char sta_addr[ETH_ALEN];
    struct ieee80211_tdls_lnk_id * linkid = NULL;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: Link RCPI Report from %s\n",
                      ether_sprintf(linkid->sa));

    ni->ni_tdls->state = IEEE80211_TDLS_S_LINKRCPI_REPORT_INPROG;

    linkid = (struct ieee80211_tdls_lnk_id *)(tf+1);
	cur_ptr = (char*) (((char*)(linkid+1))+2);

	memset(&lnk_rep, 0, sizeof(lnk_rep));
	memcpy(&lnk_rep, cur_ptr, sizeof(lnk_rep));

	IEEE80211_ADDR_COPY(bssid,    &lnk_rep.bssid);
	IEEE80211_ADDR_COPY(sta_addr, &lnk_rep.sta_addr);

	/* Check if BSSID and STA_ADDR are proper and accept measurement */
	if( IEEE80211_ADDR_EQ(bssid, ni->ni_bssid) &&
		IEEE80211_ADDR_EQ(sta_addr, ni->ni_macaddr))
	{
		/* update measurement for STA_ADDR */
		/* TODO: check if RCPI is OK to make switch */

		IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: Link RCPI Report: BSSID: %s, RSSI=%d \n",
                      ether_sprintf(lnk_rep.bssid), lnk_rep.rcpi1);
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: Link RCPI Report: STA_ADDR:%s, RSSI=%d \n",
                      ether_sprintf(lnk_rep.sta_addr), lnk_rep.rcpi2);

#if 0
		ni->ap_avgrssi = lnk_rep.rcpi1;
		ni->avgrssi    = lnk_rep.rcpi2;
#endif
	} 
    else 
    {
		/* Link RCPI request frame not for STA_ADDR in our list. */
	}
	
    return ret;
}

/*****************************************************************************/
/*  Here comes the list of func() calls which is used by the switching logic
 *  to send frames or make request suitably to make switch and reports.
 *	ieee80211_tdls_txpath_switch_req()
 *	ieee80211_tdls_rxpath_switch_req()
 *	ieee80211_tdls_link_rcpi_req()
 *	ieee80211_tdls_link_rcpi_report()
 *
 */

/* Send TDLS Tx Switch Request to Sta-mac address */
static int 
ieee80211_tdls_txpath_switch_req(struct ieee80211vap *vap, char *mac, 
			u_int8_t path, u_int8_t reason)
{
    struct ieee80211_tdls_rcpi_switch txreq;
	memset(&txreq, 0, sizeof(txreq));
	IEEE80211_TDLS_RCPI_SWITCH_SET(txreq, path, reason);
	return tdls_send_txpath_switch_req(vap->iv_ic, vap, mac, 
			   (u_int8_t*)&txreq, 
			   sizeof(txreq));
}

/* Send TDLS Rx Switch Request to Sta-mac address */
int 
ieee80211_tdls_rxpath_switch_req(struct ieee80211vap *vap, char *mac, 
			u_int8_t path, u_int8_t reason)
{
    struct ieee80211_tdls_rcpi_switch rxreq;
	memset(&rxreq, 0, sizeof(rxreq));
	IEEE80211_TDLS_RCPI_SWITCH_SET(rxreq, path, reason);
	return tdls_send_rxpath_switch_req(vap->iv_ic, vap, mac, 
			   (u_int8_t*)&rxreq, 
			   sizeof(rxreq));
}

/* Send TDLS Link RCPI Request for RSSI measurment on AP and Direct path */
static int 
ieee80211_tdls_link_rcpi_req(struct ieee80211vap *vap, char * bssid, 
			char * mac)
{
    struct ieee80211_tdls_rcpi_link_req lnkreq;
	memset(&lnkreq, 0, sizeof(lnkreq));
	IEEE80211_TDLS_RCPI_LINK_ADDR(lnkreq, bssid, mac);
	
	if(tdls_send_link_rcpi_req(vap->iv_ic, vap, mac,
		   (u_int8_t*)&lnkreq, sizeof(lnkreq), 0))
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,"Error in sending"
				"TDLS_RCPI_Request to %s thro AP path \n", 
				ether_sprintf(mac));
			
	if(tdls_send_link_rcpi_req(vap->iv_ic, vap, mac, 
		   (u_int8_t*)&lnkreq, sizeof(lnkreq), 1))
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,"Error in sending"
				"TDLS_RCPI_Request to %s thro Direct path \n", 
				ether_sprintf(mac));
			
	return 0;
}

/* Send TDLS Link RCPI Report for measured RSSI on AP and Direct path */
int 
ieee80211_tdls_link_rcpi_report(struct ieee80211vap *vap, char * bssid, 
			char * mac, u_int8_t rcpi1, 
			u_int8_t rcpi2)
{
    struct ieee80211_tdls_rcpi_link_report lnkrep;
	memset(&lnkrep, 0, sizeof(lnkrep));
	IEEE80211_TDLS_RCPI_LINK_ADDR(lnkrep, bssid, mac);
	lnkrep.rcpi1 = rcpi1;
	lnkrep.rcpi2 = rcpi2;
	
    return tdls_send_link_rcpi_report(vap->iv_ic, vap, mac, 
		 (u_int8_t*)&lnkrep, sizeof(lnkrep));
									   
}

/*
 * Handler RCPI timer
 *
 */
static OS_TIMER_FUNC(ieee80211_tdlsrcpi_timer)
{
    struct ieee80211com *ic;
    struct ieee80211_node *ni       = NULL;
    struct ieee80211_node *ni_ap    = NULL;
    struct ieee80211_node_table *nt = NULL;

	int      	diff;
	u_int8_t    hithreshold;
	u_int8_t    lothreshold;
	u_int8_t    margin;
	u_int8_t    link_st;
	u_int8_t	ap_rssi;
	u_int8_t	sta_rssi;

    OS_GET_TIMER_ARG(ic, struct ieee80211com *);
    
	/* Here comes the logic to make switching between AP/Direct path */
	
	/* 1. Select each node, Nada_Peer
	 * 2. Get the Average RSSI value for AP & STA path and get the difference.
	 * 3. if (link_st == 1-DIRECT_PATH),
	 * 4. 	if (diff < margin)&&(sta_rssi < loThreshold) switch to AP's path
	 *  	else continue to use existing Direct path.
	 * 5. else if (link_st == 0-AP_PATH),
	 * 6.	if (sta_rssi > hithreshold) switch to Direct Path
	 * 		else continue to use existing AP Path.
	 * 7. Re-Initialize the TIMER = 250ms, for next scheduling 
	 */ 

    diff        = 0;
	ap_rssi		= 0;
	sta_rssi	= 0;
	hithreshold 	= 	ic->ic_tdls->hithreshold;
	lothreshold 	= 	ic->ic_tdls->lothreshold;
	margin 		    = 	ic->ic_tdls->margin;

	nt = &ic->ic_sta;
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) 
    {	

	    if(!ni_ap) 
        {
		   ni_ap 	= ni->ni_vap->iv_bss;
		   ap_rssi 	= (ATH_NODE_NET80211(ni_ap)->an_avgrssi);
	    }

    	if(!IEEE80211_IS_TDLS_NODE(ni)|| ni == ni->ni_vap->iv_bss)	
        {
    		/* as Node is not TDLS supported, either AP/Non-TDLS Station */
	    	continue; 
	    }

    	/* Continue with RCPI work, as this Node supports TDLS */
    	link_st		= ni->ni_tdls->link_st;
	    sta_rssi 	= (ATH_NODE_NET80211(ni)->an_avgrssi);
    	diff        = ap_rssi - sta_rssi;

    #if 0
    	IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_TDLS, "TDLS:RCPI: "
	                    	"ap_rssi:%ddB, sta_rssi:%ddB, diff:%ddB \n",
                    		 ap_rssi, sta_rssi, diff);
    	IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_TDLS, "TDLS:RCPI: "
	                    	"link_st=%d, hithreshold=%d, lothreshold=%d, margin=%d\n ",
                    		 link_st, hithreshold, lothreshold, margin);
    #endif
				
    	/* Just a check to make sure existing TDLS Link is RUNNING. 
	     * If the link it TEARED_DOWN then no use of switch action.
    	 */				
    	if(ni->ni_tdls->state >= IEEE80211_TDLS_S_RUN) 
        {
    		/* To get RCPI Link Report and update RSSI value */
	    	/* TODO:RCPI: Currently RCPI_Link_Request_frame() is used 
		     * for calculation of RSSI measurement in both AP and Direct Link,
    		 * so send this frame in both AP and Direct Path.
	    	 */
		    ieee80211_tdls_link_rcpi_req(ni->ni_vap, ni->ni_bssid, ni->ni_macaddr);

    		if((link_st == AP_PATH) && (sta_rssi > hithreshold)) 
            {
    			/* Request to re-switch to Direct Link */
	    		IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_TDLS, 
		                    		"Sending DIRECT_PATH_SWITCH frame to %s , "
                                    "direct link rssi=%d, threshold=%d\n", 
                    				ether_sprintf(ni->ni_macaddr), sta_rssi, hithreshold);
						
        		ieee80211_tdls_txpath_switch_req(ni->ni_vap, ni->ni_macaddr,
				DIRECT_PATH, REASON_CHANGE_IN_LINK_STATE);
    			continue;
				
    		} 
            else if( (link_st == DIRECT_PATH) && (sta_rssi < lothreshold) && (diff > margin) )
            {
    
	    		/* Request to switch to AP Path */
		    	IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_TDLS, 
			                       "Sending AP_PATH_SWITCH frame to %s, direct link rssi=%d, "
                                   "lothrehold=%d, diff:%d, margin:%d \n", 
                        			ether_sprintf(ni->ni_macaddr), sta_rssi, lothreshold, diff, margin);
				
    			ieee80211_tdls_txpath_switch_req(ni->ni_vap, ni->ni_macaddr, 
                                                    AP_PATH, REASON_CHANGE_IN_LINK_STATE);
			    continue;
				
    		} 
            else 
            {
			    /* Nothing to do, Select the next Node */
    			continue;
		    }

		} /* if condition */
        else 
        {
            IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_TDLS,
                  " Link state is not RUN: %s, current state:%d \n", ether_sprintf(ni->ni_macaddr),
                      ni->ni_tdls->state);
        }
	} /* TAILQ_FOREACH */	

    /* re-queue the timer handler */
	OS_SET_TIMER(&ic->ic_tdls->tmrhdlr, ic->ic_tdls->timer);

	return;
}

/*
 * Start RCPI timer
 *
 */
static void
ieee80211_tdlsrcpi_timer_init(struct ieee80211com *ic, void *timerArg)
{

    ic->ic_tdls->timer        = RCPI_TIMER;          /* 3000 */
    ic->ic_tdls->hithreshold  = RCPI_HIGH_THRESHOLD;
    ic->ic_tdls->lothreshold  = RCPI_LOW_THRESHOLD; 
    ic->ic_tdls->margin       = RCPI_MARGIN;         
    OS_INIT_TIMER(ic->ic_osdev, 
                &ic->ic_tdls->tmrhdlr, 
                ieee80211_tdlsrcpi_timer, 
                timerArg);
	OS_SET_TIMER(&ic->ic_tdls->tmrhdlr, ic->ic_tdls->timer);
	
	return;   
}

/*
 * Stop RCPI timer
 *
 */
static void
ieee80211_tdlsrcpi_timer_stop(struct ieee80211com *ic, unsigned long timerArg)
{
    OS_FREE_TIMER(&ic->ic_tdls->tmrhdlr);
    return;
}
#endif


/* TDLS receive setup req frame */
static int 
tdls_recv_setup_req(struct ieee80211com *ic, struct ieee80211_node *ni, 
                         void *arg, u16 len) 
{
    int ret = 0;
    uint8_t  wpa;
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_tdls_frame *tf = (struct ieee80211_tdls_frame *)arg; 
    struct ieee80211_tdls_token  * token = NULL;
    struct ieee80211_tdls_lnk_id * lnkid = NULL;
    u8 * ftie = NULL;

    token = (struct ieee80211_tdls_token *)(tf+1);
    lnkid = (struct ieee80211_tdls_lnk_id *)(token+1);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: SETUP REQ Message from %s\n",
                      ether_sprintf(lnkid->sa));

    if(ieee80211_tdls_processie(ni, tf, len)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: SETUP REQ, failed to process IE from %s\n",
                      ether_sprintf(lnkid->sa));
    }

    ftie = ieee80211_tdls_get_ftie(tf, len);

    if(!ftie) {
        if((wpa=get_wpa_mode(vap))) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: SETUP REQ, NO FTIE present %s\n",
                      ether_sprintf(lnkid->sa));
        return -1;
        }
    }

    //NEW_TDLS /* to process SMK_M1(REQ+FTIE) */
    if((wpa=get_wpa_mode(vap))) 
        tdls_notify_supplicant(vap, lnkid->sa, IEEE80211_TDLS_SETUP_REQ, ftie);
    else 
        ret = tdls_send_setup_resp(ic, vap, lnkid->sa, NULL, 0);

    /* Dialogue Token from Initiator */
    ni->ni_tdls->token = token->token;
    ni->ni_tdls->state = IEEE80211_TDLS_S_SETUP_RESP_INPROG;
    ni->ni_tdls->stk_st = 0;
    return ret;
}

/* TDLS receive response for setup  */
static int 
tdls_recv_setup_resp(struct ieee80211com *ic, struct ieee80211_node *ni,
                         void *arg, u16 len) 
{
    int ret = 0;
    uint8_t  wpa;
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_tdls_frame *tf = (struct ieee80211_tdls_frame *)arg; 
    struct ieee80211_tdls_status * sta   = NULL;
    struct ieee80211_tdls_token  * token = NULL;
    struct ieee80211_tdls_lnk_id * lnkid = NULL;
    u8 * ftie = NULL;

    sta = (struct ieee80211_tdls_status *)(tf+1);
    if(0 != sta->status) {
        /* TODO: what if Status != SUCCESS */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: SETUP RESPONSE Message from %s failed.",
                      ether_sprintf(ni->ni_macaddr));
        return -1;
    }

    token = (struct ieee80211_tdls_token *)(sta+1);
    if(token->token != ni->ni_tdls->token) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
            "TDLS: SETUP RESPONSE Message from %s failed,"
            " token mismatch sent=%d, revd=%d",
               ether_sprintf(ni->ni_macaddr),
               ni->ni_tdls->token, token->token);
        //return -1;
    }

    lnkid = (struct ieee80211_tdls_lnk_id *)(token+1);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: SETUP RESPONSE Message from %s, ",
                      ether_sprintf(lnkid->sa));
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      " TA: %s\n", ether_sprintf(ni->ni_macaddr));

    if(ieee80211_tdls_processie(ni, tf, len)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: SETUP RESP, failed to process IE from %s\n",
                      ether_sprintf(lnkid->sa));
    }

    ftie = ieee80211_tdls_get_ftie(tf, len);
    
    if(!ftie) {
        if((wpa=get_wpa_mode(vap))) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                    "TDLS: SETUP REQ, NO FTIE present %s\n",
                    ether_sprintf(lnkid->sa));
            return -1;
        }
    }

    if((wpa=get_wpa_mode(vap))) 
        tdls_notify_supplicant(vap, lnkid->sa, IEEE80211_TDLS_SETUP_RESP, ftie);
    else
        ret = tdls_send_setup_confirm(ic, vap, ni->ni_macaddr, NULL, 0);

    ni->ni_tdls->state = IEEE80211_TDLS_S_RUN;

    return ret;
}

static int 
tdls_recv_setup_confirm(struct ieee80211com *ic, 
                            struct ieee80211_node *ni, void *arg, u16 len) 
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_tdls_frame *tf = (struct ieee80211_tdls_frame *)arg; 
    uint8_t  wpa;
    struct ieee80211_tdls_status * sta   = NULL;
    struct ieee80211_tdls_token  * token = NULL;
    struct ieee80211_tdls_lnk_id * lnkid = NULL;
    u8 * ftie = NULL;

    sta   = (struct ieee80211_tdls_status *)(tf+1);
    token = (struct ieee80211_tdls_token *)(sta+1);
    lnkid = (struct ieee80211_tdls_lnk_id *)(token+1);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: SETUP CONFIRM Message from %s\n",
                      ether_sprintf(lnkid->sa));

    ftie = ieee80211_tdls_get_ftie(tf, len);

    if(!ftie) {
        if((wpa=get_wpa_mode(vap))) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                    "TDLS: SETUP REQ, NO FTIE present %s\n",
                    ether_sprintf(lnkid->sa));
            return -1;
        }
    }

    if((wpa=get_wpa_mode(vap))) 
      tdls_notify_supplicant(vap, lnkid->sa, IEEE80211_TDLS_SETUP_CONFIRM, ftie);

    ni->ni_tdls->state = IEEE80211_TDLS_S_RUN;

#ifdef CONFIG_RCPI
    ieee80211_tdls_link_rcpi_req(ni->ni_vap, ni->ni_bssid, lnkid->sa);
    ni->ni_tdls->link_st = 1;
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, ", Setting the Path to %d(TDLS link)\n",
                      ni->ni_tdls->link_st);
        //IEEE80211_TDLSRCPI_PATH_SET(ni);
#endif

    return 0;
}

/* TDLS teardown request */
static int 
tdls_recv_teardown_req(struct ieee80211com *ic,
                           struct ieee80211_node *ni, void *arg, u16 len)
{
    int ret = 0; 
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_tdls_frame *tf = (struct ieee80211_tdls_frame *)arg; 
    uint8_t  wpa;
    struct ieee80211_tdls_lnk_id *lnkid=NULL;

    lnkid = (struct ieee80211_tdls_lnk_id *)( (u8*)(tf+1) + 
                + sizeof(struct ieee80211_tdls_status) );

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: TEARDOWN REQ Message from %s\n",
                      ether_sprintf(lnkid->sa));

    if((wpa=get_wpa_mode(vap))) /* XXX: what to send up? */
      tdls_notify_supplicant(vap, lnkid->sa, IEEE80211_TDLS_TEARDOWN_REQ, NULL);

    /* Notification to TdlsDiscovery process on TearDownReq */
    tdls_notify_teardownreq(vap, lnkid->sa, IEEE80211_TDLS_TEARDOWN_REQ, NULL);

    ni->ni_tdls->state = IEEE80211_TDLS_S_TEARDOWN_RESP_INPROG;

#ifdef CONFIG_RCPI
    ni->ni_tdls->link_st = 0;
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, ", Setting the Path to %d(AP link)\n",
                      ni->ni_tdls->link_st);
#endif

    ret = tdls_send_teardown_resp(ic, vap, ni->ni_macaddr, NULL, 0);

    return ret;
}

/* TDLS teardown response */
static int 
tdls_recv_teardown_resp(struct ieee80211com *ic,
                            struct ieee80211_node *ni, void *arg, u16 len)
{
#if ATH_DEBUG
    struct ieee80211vap *vap = ni->ni_vap;
#endif
    int ret = 0; 
    struct ieee80211_tdls_frame *tf = (struct ieee80211_tdls_frame *)arg; 
    struct ieee80211_tdls_lnk_id *lnkid=NULL;

    lnkid = (struct ieee80211_tdls_lnk_id *)( (u8*)(tf+1) + 
                + sizeof(struct ieee80211_tdls_status) );

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                      "TDLS: TEARDOWN RESPONSE Message from %s\n",
                      ether_sprintf(lnkid->sa));

    ieee80211_tdls_del_node(ni);

    return ret;
}

/* Receive Wireless ARP and add it to our wds list */
static int 
tdls_recv_arp_mgmt_frame(struct ieee80211com *ic, 
                             struct ieee80211_node *ni, void *arg, u16 len)
{
#if ATH_DEBUG
    struct ieee80211vap   *vap = ni->ni_vap;
#endif
    uint8_t *macaddr;
    struct ieee80211_node *wds_ni=NULL;
    struct ieee80211_tdls_frame *tf = (struct ieee80211_tdls_frame *)arg;

    /* Get the data followed by TDLS header */
    /* XXX: Use skb_pull() instead? */
    macaddr  = (uint8_t *)(tf + 1);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, 
                      "Received Nada node MAC address:%s\n", 
                      ether_sprintf(macaddr));

    if ((wds_ni=ieee80211_find_wds_node(&ic->ic_sta, macaddr))) {
        ieee80211_free_node(wds_ni);
        return 1;
    }
    if (ieee80211_add_wds_addr(&ic->ic_sta, ni, macaddr , 
                               IEEE80211_NODE_F_WDS_REMOTE )) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "Failed to add %s "
        					"to WDS table", ether_sprintf(macaddr));
        return -EINVAL;
    }

    return 0;
}

typedef int (*tdls_recv_fn)(struct ieee80211com *, struct ieee80211_node *, void *, u16);

/* Function pointers for message types received */
tdls_recv_fn tdls_recv[IEEE80211_TDLS_MSG_MAX] = {
    tdls_recv_setup_req,
    tdls_recv_setup_resp,
    tdls_recv_setup_confirm,
    tdls_recv_teardown_req,
    tdls_recv_teardown_resp,
#ifdef CONFIG_RCPI
    tdls_recv_txpath_switch_req,
    tdls_recv_txpath_switch_resp,
    tdls_recv_rxpath_switch_req,
    tdls_recv_rxpath_switch_resp,
    tdls_recv_link_rcpi_req,
    tdls_recv_link_rcpi_report,
#endif
    tdls_recv_arp_mgmt_frame /* For WDS clients behind the other side */
};

typedef  int (*tdls_send_fn)(struct ieee80211com *, struct ieee80211vap *,
                             void *, uint8_t *, uint16_t); 

/* TDLS send message type functions */
tdls_send_fn tdls_send[IEEE80211_TDLS_MSG_MAX] = {
    tdls_send_setup_req,
    tdls_send_setup_resp,
    tdls_send_setup_confirm,
    tdls_send_teardown_req,
    tdls_send_teardown_resp,
#ifdef CONFIG_RCPI //dummy
    tdls_send_txpath_switch_req,
    tdls_send_txpath_switch_resp,
    tdls_send_rxpath_switch_req,
    tdls_send_rxpath_switch_resp,
    NULL, /* tdls_send_link_rcpi_req, to avoid Warning */
    tdls_send_link_rcpi_report,
#endif
    tdls_send_arp  /* For WDS clients behind the stations */
};

tdls_send_fn tdls_wpa_send[IEEE80211_TDLS_MSG_MAX] = {
	tdls_send_setup_req,    	/* IEEE80211_TDLS_SETUP_REQ */
	tdls_send_setup_resp,   	/* IEEE80211_TDLS_SETUP_RESP */
	tdls_send_setup_confirm,	/* IEEE80211_TDLS_SETUP_CONFIRM */
	tdls_send_teardown_req, 	/* IEEE80211_TDLS_TEARDOWN_REQ */
	tdls_send_teardown_resp, 	/* IEEE80211_TDLS_TEARDOWN_RESP */	
};

struct ieee80211_node *
ieee80211_create_tdls_node(struct ieee80211vap *vap, uint8_t *macaddr)
{
    struct ieee80211_node *ni;
    struct ieee80211com *ic = vap->iv_ic;

    ni = ieee80211_dup_bss(vap, macaddr);

    if (ni != NULL) {
        ni->ni_esslen = vap->iv_bss->ni_esslen;
        memcpy(ni->ni_essid, vap->iv_bss->ni_essid, vap->iv_bss->ni_esslen);
        IEEE80211_ADDR_COPY(ni->ni_bssid, vap->iv_bss->ni_bssid);
        memcpy(ni->ni_tstamp.data, vap->iv_bss->ni_tstamp.data, 
               sizeof(ni->ni_tstamp));

        if (vap->iv_opmode == IEEE80211_M_IBSS)
            ni->ni_capinfo = IEEE80211_CAPINFO_IBSS;
        else
            ni->ni_capinfo = IEEE80211_CAPINFO_ESS;

        if (vap->iv_flags & IEEE80211_F_PRIVACY)
            ni->ni_capinfo |= IEEE80211_CAPINFO_PRIVACY;

        if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
             IEEE80211_IS_CHAN_2GHZ(vap->iv_bsschan))
            ni->ni_capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;

        if (ic->ic_flags & IEEE80211_F_SHSLOT)
            ni->ni_capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;

        if (IEEE80211_IS_CHAN_11N(vap->iv_bsschan)){
            /* enable HT capability */
            ni->ni_flags |= IEEE80211_NODE_HT | IEEE80211_NODE_QOS |
                            IEEE80211_NODE_TDLS;/* XXX: | IEEE80211_NODE_HT_DS; */
            ni->ni_htcap = IEEE80211_HTCAP_C_SM_ENABLED;
        } else
	        ni->ni_flags |= IEEE80211_NODE_TDLS;

        if (ic->ic_htcap & IEEE80211_HTCAP_C_SHORTGI40)
            ni->ni_htcap |= IEEE80211_HTCAP_C_SHORTGI40;

        if (ic->ic_htcap & IEEE80211_HTCAP_C_CHWIDTH40)
            ni->ni_htcap |= IEEE80211_HTCAP_C_CHWIDTH40;

        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "HTCAP:%x, CAPINFO:%x \n",
                          ni->ni_htcap, ni->ni_capinfo);

        ni->ni_chan = vap->iv_bss->ni_chan;
        ni->ni_intval = vap->iv_bss->ni_intval;
        ni->ni_erp = vap->iv_bss->ni_erp;

        ni->ni_tdls = (struct ieee80211_tdls_node *) OS_MALLOC(ic->ic_osdev,
     					sizeof(struct ieee80211_tdls_node), GFP_KERNEL);

        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, " Added TDLS node, mac: %s\n", 
                          ether_sprintf(ni->ni_macaddr));
#ifdef CONFIG_RCPI
        /* Just start with some rssi above the low threshold+extra so that
           we dont switch to AP path */
         ATH_RSSI_LPF(ATH_NODE_NET80211(ni)->an_avgrssi, RCPI_HIGH_THRESHOLD*15);
         IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, " Setting avgrssi to %d dB\n",
                           (ATH_NODE_NET80211(ni)->an_avgrssi));
#endif

        ieee80211_node_join(ni);

		/* newma_specific: as ieee80211_node_join() fails to set tx_rate */
        /* Set up node tx rate */
        if (ic->ic_newassoc) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "Set up node tx rate: %s\n", 
                          ether_sprintf(ni->ni_macaddr));
            ic->ic_newassoc(ni, TRUE);
		}

        /* XXX Should we authorize the node here or after handshake */
		ieee80211_node_authorize(ni);

    }

    ieee80211_tdls_sm_attach(ic, vap);
	/* create entry into state machine */
    ieee80211_tdls_addentry(vap, ni);

    return ni;
}

/* TDLS Receive handler */
int 
_ieee80211_tdls_recv_mgmt(struct ieee80211com *ic, struct ieee80211_node *ni, 
                             wbuf_t wbuf)
{
#if ATH_DEBUG
    struct ieee80211vap         *vap = ni->ni_vap;
#endif
    struct ieee80211_tdls_frame *tf;
    struct ieee80211_node_table *nt = &ic->ic_sta;
    struct ieee80211_node *ni_tdls=NULL;
    struct ether_header  *eh; 
    u_int32_t ret=0, isnew=0; 
    char * sa = NULL;
    u16 len = 0;
    
//     IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "TDLS: Entering %s for node:%s\n",
//                       __FUNCTION__, ether_sprintf(ni->ni_macaddr));

    if (!(ni->ni_vap->iv_ath_cap & IEEE80211_ATHC_TDLS)) {
        printk("TDLS not enabled \n");
        return 0;
    }

     eh = (struct ether_header *)wbuf_raw_data(wbuf);
     tf = (struct ieee80211_tdls_frame *)(wbuf_raw_data(wbuf) + sizeof(struct ether_header));
     sa = eh->ether_shost;
     len = wbuf_get_pktlen(wbuf) - sizeof(struct ether_header); /* len of TDLS data */

    if (IEEE80211_ADDR_EQ(sa, ni->ni_vap->iv_myaddr)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, " %s My own echo, "
                          "discarding it. \n", __FUNCTION__);
        return 0;
    }

    /* Check to make sure its a valid setup request */
     if (eh->ether_type != IEEE80211_TDLS_ETHERTYPE || 
        tf->rftype != IEEE80211_TDLS_RFTYPE || 
        tf->tdls_pkt_type > IEEE80211_TDLS_LEARNED_ARP) {
       IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "TDLS: Invalid ether "
                         "type: %x, rftype: %x, pkt_type:%x \n",
                         eh->ether_type, tf->rftype, 
                         tf->tdls_pkt_type);
        return -EINVAL;
    }

    ni_tdls = ieee80211_find_node(nt, sa);

    /* Create a node, if its a new request */
    if (!ni_tdls && tf->tdls_pkt_type == IEEE80211_TDLS_SETUP_REQ) {
        ni_tdls = ieee80211_create_tdls_node(ni->ni_vap, sa);
        isnew = 1;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,
                          "Creating TDLS node:ni:%x, ni_tdls:%x \n",
                          ni_tdls, ni_tdls->ni_tdls);
    }

    if (!ni_tdls) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "Node not present.. %s, "
                          "type:%d\n",
                          ether_sprintf(sa), tf->tdls_pkt_type);
        ret = -EINVAL;
        goto ex;
    }

    if( (IEEE80211_TDLS_SETUP_REQ <= tf->tdls_pkt_type) &&
        (tf->tdls_pkt_type <= IEEE80211_TDLS_LEARNED_ARP) )
        ret = tdls_recv[tf->tdls_pkt_type](ic, ni_tdls, (void *)tf, len);

ex:
    if (ni_tdls /* && !isnew */ ) { 
         /* newma_specific: To decrement the ref count increased by 
          * ieee80211_create_tdls_node, as ieee80211_dup_bss() refer vap */
        ieee80211_free_node(ni_tdls);
    }

    return ret;
}

int 
ieee80211_tdls_recv_mgmt(struct ieee80211com *ic, struct ieee80211_node *ni, 
                             wbuf_t wbuf) {
    struct ieee80211vap    *vap = ni->ni_vap;

    if( vap->iv_opmode == IEEE80211_M_STA &&
        IEEE80211_TDLS_ENABLED(vap) ) {
			struct ether_header * eh;
			eh = (struct ether_header *)(wbuf->data);
            if (eh->ether_type == IEEE80211_TDLS_ETHERTYPE)
                _ieee80211_tdls_recv_mgmt(vap->iv_ic, ni, wbuf);
               
        }
    return 0;                             
}

int 
ieee80211_tdls_send_mgmt(struct ieee80211vap *vap, u_int32_t type, 
                                   void *arg)
{
    struct ieee80211com   *ic = vap->iv_ic;
    int ret = 0;

    if (!(vap->iv_ath_cap & IEEE80211_ATHC_TDLS)) {
        printk("TDLS Disabled \n");
        return 0;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "%s, type:%d, macaddr:%s\n",
                      __FUNCTION__, type, ether_sprintf(arg));

    ret = tdls_send[type](ic, vap, arg, NULL, 0);
	
    return ret;
}

/* Add remote mac address */
int 
ieee80211_tdls_ioctl(struct ieee80211vap *vap, int type, char *mac)
{
    struct ieee80211_node *ni_tdls=NULL;
    uint8_t  wpa;

    switch (type) {

    case IEEE80211_TDLS_ADD_NODE:
        ni_tdls = ieee80211_find_node(&vap->iv_ic->ic_sta, mac);

        /* Create a node, if its a new request */
        if (!ni_tdls) {
            ni_tdls = ieee80211_create_tdls_node(vap, mac);
        } else {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "%s, Already in the setup "
                 " process or node already exists.... %s\n",
                 __FUNCTION__, ether_sprintf(mac));
            /* To decrement the ref count increased by ieee80211_find_node */
            ieee80211_free_node(ni_tdls);
            return 0;
        }

        /* For TDLS send the node join event to wpa2 */
        if((wpa=get_wpa_mode(vap))) {
            tdls_notify_supplicant(vap, mac, IEEE80211_TDLS_SETUP_REQ, NULL);
        } else {
            ieee80211_tdls_send_mgmt(vap, IEEE80211_TDLS_SETUP_REQ, mac);
		}
        /* newma_specific: To decrement the ref count increased by 
         * ieee80211_create_tdls_node, as ieee80211_dup_bss() refer vap */
        ieee80211_free_node(ni_tdls);
        
        break;
    case IEEE80211_TDLS_DEL_NODE:
        ieee80211_tdls_send_mgmt(vap, IEEE80211_TDLS_TEARDOWN_REQ, mac);
        break;
    default:
        return -1;
    }

    return 0;
}

/** ieee80211_wpa_tdls_ftie - to process TDLS command from Supplicant
 *  buf - contains TDLS message Peer-MAC, TDLS_TYPE and
 *  SMK-FTIE if needed
 * 	len - length of the buffer
 */
int 
ieee80211_wpa_tdls_ftie(struct ieee80211vap *vap, u8 *buf, int len)
{
    char	mac[ETH_ALEN];
	u16		type 		= 0;
	u16		smk_len 	= 0;
	u8		*smk 		= NULL;
	u8 		err 		= 0;

    u8 *ieptr = NULL;
    u8 *ie = NULL;

	memcpy(mac, buf, ETH_ALEN); 
	buf += ETH_ALEN;
	type 	= *(u16*)buf; buf += 2;
	smk_len = *(u16*)buf; buf += 2;
	smk 	= buf;

	if(smk_len > (IEEE80211_ASSOC_MGMT_REQ_SIZE 
				- sizeof(struct ieee80211_tdls_frame)) ) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,"Message length "
					"too high, len=%d\n", smk_len);
		return err;
	}
	
	if(smk_len > 255) {
	
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,"Message length "
					"more than 255, len=%d\n", smk_len);
		return err;
	}
	
	/* Create a buf to have FTIE in TLV format */
	ie = (u8 *) OS_MALLOC(vap->iv_ic->ic_osdev, smk_len+2, GFP_KERNEL);
    if(!ie) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,"No memory"
		           " len=%d\n", smk_len);
	    return -ENOMEM;
	}
	
	ieptr = ie;
	*ieptr++ = IEEE80211_ELEMID_FTIE;
	*ieptr++ = (u8)smk_len;
    memcpy(ieptr, buf, smk_len);
	
#define TYPE(x) (x&0x7)
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "WPA_TDLS_CMD: PEER-MAC:%s "
			"for len:%d, type:%d, TYPE:%d, vap:%p\n", 
			ether_sprintf(mac), smk_len, type, TYPE(type), vap);


    err = tdls_wpa_send[TYPE(type)](vap->iv_ic, vap, mac, ie,
                                    smk_len+2);

    if (err) IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS,"error:%d\n", err);

    
    OS_FREE(ie);
    return err;
}

inline int
ieee80211_convert_mac_to_hex(struct ieee80211vap *vap, char *maddr)
{
    int i, j = 2;

    for(i=2; i<strlen(maddr); i+=3)
    {
        maddr[j++] = maddr[i+1];
        maddr[j++] = maddr[i+2];
    }
        
    for(i=0; i<12; i++)
    {
        /* check 0~9, A~F */
        maddr[i] = ((maddr[i]-48) < 10) ? (maddr[i] - 48) : (maddr[i] - 55);

        /* check a~f */ 
        if ( maddr[i] >= 42 )
            maddr[i] -= 32;

        if ( maddr[i] > 0xf )
            return -EFAULT;
    }
    
    for(i=0; i<6; i++)
    {
        maddr[i] = (maddr[(i<<1)] << 4) + maddr[(i<<1)+1];
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, "%s : %s \n",
                      __FUNCTION__, ether_sprintf(maddr));

    return 0;
}

int
ieee80211_ioctl_set_tdls_rmac(struct net_device *dev,
                              void *info, void *w,
                              char *extra)
{
    wlan_if_t ifvap = ((osif_dev *)ath_netdev_priv(dev))->os_if;
    struct ieee80211vap *vap = ifvap;
    struct iw_point *wri;
    char mac[18];

    if (!IEEE80211_TDLS_ENABLED(vap))
        return -EFAULT;

    wri = (struct iw_point *) w;

    if (wri->length > sizeof(mac))
    {
        printk("len > mac_len \n");
        return -EFAULT;
    }

    memcpy(&mac, extra, 17/*sizeof(mac)*/);
    mac[17]='\0';

    if (ieee80211_convert_mac_to_hex(vap, mac)) {
        printk(KERN_ERR "Invalid mac address \n");
        return -EINVAL;
    }

    IEEE80211_TDLS_IOCTL(vap, IEEE80211_TDLS_ADD_NODE, mac);

    return 0;
}

int
ieee80211_ioctl_clr_tdls_rmac(struct net_device *dev,
                              void *info, void *w,
                              char *extra)
{
    wlan_if_t ifvap = ((osif_dev *)ath_netdev_priv(dev))->os_if;
    struct ieee80211vap *vap = ifvap;
    struct iw_point *wri;
    char mac[18];

    if (!IEEE80211_TDLS_ENABLED(vap))
        return -EFAULT;

    wri = (struct iw_point *) w;

    if (wri->length > sizeof(mac))
    {
        printk("len > mac_len \n");
        return -EFAULT;
    }

    memcpy(&mac, extra, 17/*sizeof(mac)*/);
    mac[17]='\0';

    printk(" clearing the mac address : %s \n", mac);

    if (ieee80211_convert_mac_to_hex(vap, mac)) {
        printk(KERN_ERR "Invalid mac address\n");
        return -EINVAL;
    }

    IEEE80211_TDLS_IOCTL(vap, IEEE80211_TDLS_DEL_NODE, mac);

    return 0;
}

/* The function is used  to set the MAC address  of a device set via iwpriv */
void ieee80211_tdls_set_mac_addr(u_int8_t *mac_address, u_int32_t word1, u_int32_t word2)
{
    mac_address[0] =  (u_int8_t) ((word1 & 0xff000000) >> 24);
    mac_address[1] =  (u_int8_t) ((word1 & 0x00ff0000) >> 16);
    mac_address[2] =  (u_int8_t) ((word1 & 0x0000ff00) >> 8);
    mac_address[3] =  (u_int8_t) ((word1 & 0x000000ff));
    mac_address[4] =  (u_int8_t) ((word2 & 0x0000ff00) >> 8);
    mac_address[5] =  (u_int8_t) ((word2 & 0x000000ff));
}

#endif /* UMAC_SUPPORT_TDLS */

