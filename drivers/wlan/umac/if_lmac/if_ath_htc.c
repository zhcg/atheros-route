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

#include "if_athvar.h"

#ifdef ATH_SUPPORT_HTC

#include "if_ath_htc.h"
#include "htc.h"
#include "htc_host_api.h"
#include "wmi.h"
#include "wmi_host_api.h"
#include "host_target_interface.h"
#include <htc_common.h>

#ifdef MAGPIE_HIF_GMAC
#include <osdep.h>
#include "htc_thread.h"

enum{
    IEEE80211_SWBA_DEFER_PENDING    =0x1,
    IEEE80211_SWBA_DEFER_DONE       =0x2,
};

#endif

extern u_int8_t
ieee80211_mac_addr_cmp(u_int8_t *src, u_int8_t *dst);

#if defined(MAGPIE_HIF_GMAC) || defined(MAGPIE_HIF_USB)
int ath_find_tgt_node_index(struct ieee80211_node *ni);

u_int32_t
ath_net80211_htc_node_getrate(const struct ieee80211_node *ni, u_int8_t type)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    u_int8_t *vtar = NULL;
    u_int32_t size = 0;
    u_int8_t nodeindex = 0;
    
    nodeindex = ath_find_tgt_node_index((struct ieee80211_node *)ni);
    vtar = &nodeindex;
    size = sizeof(nodeindex);

    switch (type) {
        case IEEE80211_RATE_TX:
            ATH_NODE_NET80211(ni)->an_avgtxrate = scn->sc_ops->ath_wmi_node_getrate(scn->sc_dev, vtar, size);        
            return (ATH_NODE_NET80211(ni)->an_avgtxrate);
        case IEEE80211_LASTRATE_TX:
            return (ATH_NODE_NET80211(ni)->an_lasttxrate) ;
        case IEEE80211_RATECODE_TX:
            return (ATH_NODE_NET80211(ni)->an_txratecode);
        case IEEE80211_RATE_RX:
            return ATH_RATE_OUT(ATH_NODE_NET80211(ni)->an_avgrxrate);
        default:
            return 0;
    }

}
#endif

#if defined(MAGPIE_HIF_GMAC)
static void
ath_net80211_drain_uapsd(void *arg,  struct ieee80211_node *ni)
{
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211 *)arg;
    ath_node_t node = ATH_NODE_NET80211(ni)->an_sta;
    if (ni->ni_flags & IEEE80211_NODE_UAPSD_CREDIT_UPDATE) {
        scn->sc_ops->ath_htc_uapsd_credit_update(scn->sc_dev, node);
    }     
}
#endif
void
ath_net80211_uapsd_creditupdate(ieee80211_handle_t ieee)
{
#ifdef MAGPIE_HIF_GMAC     
     struct ieee80211com         *ic  = NET80211_HANDLE(ieee);
     struct ath_softc_net80211   *scn = ATH_SOFTC_NET80211(ic);
    
     ieee80211_iterate_node(ic, ath_net80211_drain_uapsd, scn);
#endif
}


void
ath_add_vap_target(struct ieee80211com *ic, void *vtar, int size)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_ops->ath_wmi_add_vap(scn->sc_dev, vtar, size);
}

void
ath_delete_vap_target(struct ieee80211com *ic, void *vtar, int size)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->ath_wmi_delete_vap(scn->sc_dev, vtar, size);
}

void
ath_add_node_target(struct ieee80211com *ic, void *ntar, int size)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_ops->ath_wmi_add_node(scn->sc_dev, ntar, size);
}

void
ath_delete_node_target(struct ieee80211com *ic, void *ntar, int size)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_ops->ath_wmi_delete_node(scn->sc_dev, ntar, size);
}

void
ath_update_node_target(struct ieee80211com *ic, void *ntar, int size)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_ops->ath_wmi_update_node(scn->sc_dev, ntar, size);
}

#if ENCAP_OFFLOAD
void
ath_update_vap_target(struct ieee80211com *ic, void *vtgt, int size)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->ath_wmi_update_vap(scn->sc_dev, vtgt, size);
}
#endif
void
ath_htc_ic_update_target(struct ieee80211com *ic, void *target, int size)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_ops->ath_wmi_ic_update_target(scn->sc_dev, target, size);
}

void 
ath_get_config_chainmask(struct ieee80211com *ic, void *vtar)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_ops->ath_get_config_param(scn->sc_dev, ATH_PARAM_TXCHAINMASK, vtar);
}


#ifdef MAGPIE_HIF_GMAC
void
ath_swba_event_defer(void *data)
{
   struct ath_softc_net80211 *scn = (struct ath_softc_net80211*)data;
   struct ath_swba_data *swba_event = &scn->sc_htc_swba_data;
   scn->sc_ops->ath_wmi_beacon(scn->sc_dev, swba_event->currentTsf, 
                              swba_event->beaconPendingCount, 1);
    
   atomic_set(&swba_event->flags,IEEE80211_SWBA_DEFER_DONE);
}

void
ath_swba_event(void *Context, void *data, u_int32_t datalen)
{
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211 *)Context;
    struct ieee80211com *ic = &scn->sc_ic;
    WMI_SWBA_EVENT *swba_event = (WMI_SWBA_EVENT *)data;

    /* XXX: need to fix if defer has not run & more events are arriving */
    if(atomic_read(&scn->sc_htc_swba_data.flags) == IEEE80211_SWBA_DEFER_PENDING)
    {
        /*adf_os_print("SWBA Event processing is pending ignoring event  \n");*/
    } else if(adf_os_unlikely(ic->ic_flags & IEEE80211_F_CHANSWITCH)){
        if (ic->ic_chanchange_cnt) {
            atomic_set(&scn->sc_htc_swba_data.flags, IEEE80211_SWBA_DEFER_PENDING);
            scn->sc_ops->ath_wmi_beacon(scn->sc_dev, swba_event->currentTsf, swba_event->beaconPendingCount, 0);
            atomic_set(&scn->sc_htc_swba_data.flags, IEEE80211_SWBA_DEFER_DONE);
        }
        else {
            scn->sc_htc_swba_data.currentTsf         = swba_event->currentTsf; 
            scn->sc_htc_swba_data.beaconPendingCount = swba_event->beaconPendingCount;
            atomic_set(&scn->sc_htc_swba_data.flags, IEEE80211_SWBA_DEFER_PENDING);

            OS_PUT_DEFER_ITEM(scn->sc_osdev, 
                    ath_swba_event_defer, 
                    WORK_ITEM_SET_BEACON_DEFERED, 
                    scn, NULL, NULL);

        }
    } else {
        scn->sc_ops->ath_wmi_beacon(scn->sc_dev, swba_event->currentTsf, swba_event->beaconPendingCount, 0);
    }
}
#else
void
ath_swba_event(void *Context, void *data, u_int32_t datalen)
{
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211 *)Context;
    WMI_SWBA_EVENT *swba_event = (WMI_SWBA_EVENT *)data;

    scn->sc_ops->ath_wmi_beacon(scn->sc_dev, swba_event->currentTsf,
        swba_event->beaconPendingCount, 0);
}
#endif
void
ath_bmiss_event(void *Context, void *data, u_int32_t datalen)
{
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211 *)Context;

    scn->sc_ops->ath_wmi_bmiss(scn->sc_dev);
}

void
host_htc_eptx_comp(void *context, wbuf_t skb, HTC_ENDPOINT_ID epid)
{

	/* for HTC, since we don't have the "real" tx-status, we do them here. */
    struct ath_softc_net80211 	*scn = (struct ath_softc_net80211 *)context;
	ath_dev_t  				 	sc_dev = scn->sc_dev;
	struct ath_softc 			*sc = ATH_DEV_TO_SC(sc_dev);
	u_int16_t 					min_len_req_by_stats = sizeof(ath_data_hdr_t)+sizeof(struct ieee80211_frame);

	if( wbuf_get_pktlen(skb)> min_len_req_by_stats &&
		wbuf_get_tid(skb) < ATH_HTC_WME_NUM_TID &&(
		epid == sc->sc_data_BE_ep ||
		epid == sc->sc_data_BK_ep ||
		epid == sc->sc_data_VI_ep ||
		epid == sc->sc_data_VO_ep	)){
		
		struct ieee80211_node 		*ni = wbuf_get_node(skb);
		struct ieee80211vap 		*vap = ni->ni_vap;
		struct ieee80211_frame 		*wh;
		struct ieee80211_tx_status 	ts;
		struct ieee80211_mac_stats 	*mac_stats;
		u_int8_t 					type, subtype;
		u_int8_t 					is_mcast;
				
		wh = (struct ieee80211_frame *)(wbuf_header(skb)+sizeof(ath_data_hdr_t));
		type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
		subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
		/* we fake ts becuase we don't have ts in HTC tx */
		ts.ts_flags = 0;                                    
    	ts.ts_retries = 0;  

		ieee80211_update_stats(vap, skb, wh, type, subtype, &ts);
		
		/* patch: HTC tx only ath_data_hdr length */
		is_mcast = IEEE80211_IS_MULTICAST(wh->i_addr1) ? 1 : 0;
		mac_stats = is_mcast ? &vap->iv_multicast_stats : &vap->iv_unicast_stats;
		mac_stats->ims_tx_bytes -= sizeof(ath_data_hdr_t);

 	}
	
    ieee80211_free_node(wbuf_get_node(skb));
}

/*
 * callback hadlers: most of the actual processing is deffered.
 */
void
host_htc_eprx_comp(void *context, wbuf_t skb, HTC_ENDPOINT_ID epid) //void *rxstat, a_int32_t  epid)
{
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211 *)context;

    scn->sc_ops->ath_htc_rx(scn->sc_dev, skb);
}

int ath_find_tgt_vap_index(struct ieee80211_node *ni)
{
    struct ieee80211com *ic = ni->ni_ic;
    u_int8_t i;
    u_int8_t vapindex = 0xff;

    if (ic == NULL)
    {
    	/* The case is in the initializing stage, so return zero immediately. */
    	/* The target firmware can always handle the ZERO-th node.            */
        return 0;        	
    }

    //printk("%s: mac: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", __FUNCTION__,
    //    ni->ni_macaddr[0], ni->ni_macaddr[1], ni->ni_macaddr[2],
    //    ni->ni_macaddr[3], ni->ni_macaddr[4], ni->ni_macaddr[5]);

    for(i = 0; i < HTC_MAX_NODE_NUM; i++) {
        if ((ic->target_node_bitmap[i].ni_valid == 1) && 
            ieee80211_mac_addr_cmp(ic->target_node_bitmap[i].ni_macaddr,
                                       ni->ni_macaddr) == 0) {
            vapindex = ic->target_node_bitmap[i].ni_vapindex;
            break;
        }
    }
    return vapindex;
}

u_int8_t ath_net80211_find_tgt_vap_index(ieee80211_handle_t ieee, ieee80211_node_t node)
{
    struct ieee80211_node *ni = (struct ieee80211_node *) node;

    return ath_find_tgt_vap_index(ni);
}

int ath_find_tgt_node_index(struct ieee80211_node *ni)
{
    struct ieee80211com *ic = ni->ni_ic;
    u_int8_t i;
    u_int8_t nodeindex = 0xff;

    if (ic == NULL)
    {
    	/* The case is in the initializing stage, so return zero immediately. */
    	/* The target firmware can always handle the ZERO-th node.            */
        return 0;        	
    }

    //printk("%s: mac: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", __FUNCTION__,
    //    ni->ni_macaddr[0], ni->ni_macaddr[1], ni->ni_macaddr[2],
    //    ni->ni_macaddr[3], ni->ni_macaddr[4], ni->ni_macaddr[5]);

    for(i = 0; i < HTC_MAX_NODE_NUM; i++) {
        if ((ic->target_node_bitmap[i].ni_valid == 1) && 
            ieee80211_mac_addr_cmp(ic->target_node_bitmap[i].ni_macaddr,
                                       ni->ni_macaddr) == 0) {
            nodeindex = i;
            break;
        }
    }

    /* check nodexindex again */
    if (nodeindex == 0xff) {
      #ifdef ATH_SUPPORT_P2P
        struct ieee80211vap *vap = ni->ni_vap;
        if (vap->iv_create_flags & IEEE80211_P2P_DEVICE) {
            struct ieee80211_node *iv_bss = ni->ni_vap->iv_bss;
            for(i = 0; i < HTC_MAX_NODE_NUM; i++) {
                if ((ic->target_node_bitmap[i].ni_valid == 1) && 
                    ieee80211_mac_addr_cmp(ic->target_node_bitmap[i].ni_macaddr,
                                           iv_bss->ni_macaddr) == 0) {
                    nodeindex = i;
                    break;
                }
            }
            if (nodeindex == 0xff) {
                //printk("%s still can't find\n", __FUNCTION__);
                nodeindex = 0;
            } else {
                printk("%s new found nodeindex(%d) for p2p\n", __FUNCTION__, nodeindex);
            }
        } else
      #endif
        nodeindex = 0;
    }

    //printk("%s: nodeindex: %d\n", __FUNCTION__, nodeindex);
    return nodeindex;
}

u_int8_t ath_htc_find_tgt_node_index(wbuf_t wbuf)
{
    struct ieee80211_node *ni = wbuf_get_node(wbuf);

    return ath_find_tgt_node_index(ni);
}

u_int8_t ath_net80211_find_tgt_node_index(ieee80211_handle_t ieee, ieee80211_node_t node)
{
    struct ieee80211_node *ni = (struct ieee80211_node *) node;

    return ath_find_tgt_node_index(ni);
}

int
ath_htc_wmm_update_enable(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    /* For Windows, we just need to set the flag sc_htc_wmm_update_enabled, because
       we have dedicated thread, but in current Linux driver, we use worker queue
       for us, thus we need to trigger the worker queue */

    scn->sc_htc_wmm_update_enabled = 1;
    scn->sc_ops->ath_schedule_wmm_update(scn->sc_dev);
    return 0;
}

void
ath_htc_wmm_update(ieee80211_handle_t ieee)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);

    /* Because we change the callback function for wme_update, we can't use
       ic->ic_wme.wme_update for WMM paramter delay update, or we might get
       problems */

    ath_htc_wmm_update_params(ic);
}

/*
 * callback hadlers: Action frames TX Complete.
 */
void
ath_action_tx_event(void *Context, int8_t tx_status)
{
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211 *)Context;
    struct ath_usb_p2p_action_queue *p2p_action_wbuf;

    IEEE80211_STATE_P2P_ACTION_LOCK_IRQ(scn);
    if (scn->sc_p2p_action_queue_head) {
        ieee80211_vap_complete_buf_handler handler;
        void *arg;
        wbuf_t wbuf;

        p2p_action_wbuf = scn->sc_p2p_action_queue_head;
        if (scn->sc_p2p_action_queue_head == scn->sc_p2p_action_queue_tail) {
            scn->sc_p2p_action_queue_head = scn->sc_p2p_action_queue_tail = NULL;
        } else {
            scn->sc_p2p_action_queue_head = scn->sc_p2p_action_queue_head->next;
        }

        wbuf = p2p_action_wbuf->wbuf;
        if (!p2p_action_wbuf->deleted) {
            wbuf_get_complete_handler(wbuf,(void **)&handler, &arg);
            if (handler) {
                struct ieee80211_node *ni = wbuf_get_node(wbuf);
                struct ieee80211_frame *wh = (struct ieee80211_frame *)wbuf_header(wbuf);
                wlan_if_t vap = ni->ni_vap;
                struct ieee80211_tx_status ts;

                ts.ts_flags = tx_status;
                ts.ts_retries = 0;
                handler(vap, wbuf, arg, wh->i_addr1, wh->i_addr2, wh->i_addr3, &ts);
            }
        } else {
#ifndef MAGPIE_HIF_GMAC 
            printk("### action frame %p marked as deleted\n", wbuf);
#endif        
        }
        wbuf_release(scn->sc_osdev, p2p_action_wbuf->wbuf);
        OS_FREE(p2p_action_wbuf);

        //printk("### %s (%d) : Action TX EVENT DONE...\n", __FUNCTION__, __LINE__);
    }
    IEEE80211_STATE_P2P_ACTION_UNLOCK_IRQ(scn);
}

void
ath_gen_timer_event(void *Context, void *data)
{
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211 *)Context;
    WMI_GENTIMER_EVENT *gentimerEvt = (WMI_GENTIMER_EVENT *)data;

    if (!data) 
        return;

    scn->sc_ops->ath_wmi_generc_timer(scn->sc_dev, gentimerEvt->trigger_mask, gentimerEvt->thresh_mask, gentimerEvt->curr_tsf);
    OS_FREE(data);    
}
#endif /* #ifdef ATH_SUPPORT_HTC */
