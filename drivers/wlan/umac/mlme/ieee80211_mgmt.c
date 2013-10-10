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
 *
 *  management processing code common for all opmodes.
 */
#include "ieee80211_mlme_priv.h"
#include "ieee80211_wds.h"
#ifdef ATH_SUPPORT_HTC
#include <ieee80211_target.h>
#endif

/*
 * xmit management processing code.
 */

/*
 * Set the direction field and address fields of an outgoing
 * non-QoS frame.  Note this should be called early on in
 * constructing a frame as it sets i_fc[1]; other bits can
 * then be or'd in.
 */
void
ieee80211_send_setup(
    struct ieee80211vap *vap,
    struct ieee80211_node *ni,
    struct ieee80211_frame *wh,
    u_int8_t type,
    const u_int8_t *sa,
    const u_int8_t *da,
    const u_int8_t *bssid)
{
#define WH4(wh)((struct ieee80211_frame_addr4 *)wh)

    wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | type;
    if ((type & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_DATA) {
        switch (vap->iv_opmode) {
        case IEEE80211_M_STA:
            wh->i_fc[1] = IEEE80211_FC1_DIR_TODS;
            IEEE80211_ADDR_COPY(wh->i_addr1, bssid);
            IEEE80211_ADDR_COPY(wh->i_addr2, sa);
            IEEE80211_ADDR_COPY(wh->i_addr3, da);
            break;
        case IEEE80211_M_IBSS:
        case IEEE80211_M_AHDEMO:
            wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
            IEEE80211_ADDR_COPY(wh->i_addr1, da);
            IEEE80211_ADDR_COPY(wh->i_addr2, sa);
            IEEE80211_ADDR_COPY(wh->i_addr3, bssid);
            break;
        case IEEE80211_M_HOSTAP:
            wh->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;
            IEEE80211_ADDR_COPY(wh->i_addr1, da);
            IEEE80211_ADDR_COPY(wh->i_addr2, bssid);
            IEEE80211_ADDR_COPY(wh->i_addr3, sa);
            break;
        case IEEE80211_M_WDS:
            wh->i_fc[1] = IEEE80211_FC1_DIR_DSTODS;
            /* XXX cheat, bssid holds RA */
            IEEE80211_ADDR_COPY(wh->i_addr1, bssid);
            IEEE80211_ADDR_COPY(wh->i_addr2, vap->iv_myaddr);
            IEEE80211_ADDR_COPY(wh->i_addr3, da);
            IEEE80211_ADDR_COPY(WH4(wh)->i_addr4, sa);
            break;
        default:/* NB: to quiet compiler */
            break;
        }
    } else {
        wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
        IEEE80211_ADDR_COPY(wh->i_addr1, da);
        IEEE80211_ADDR_COPY(wh->i_addr2, sa);
        IEEE80211_ADDR_COPY(wh->i_addr3, bssid);
    }
    *(u_int16_t *)&wh->i_dur[0] = 0;
    /* NB: use non-QoS tid */
    /* to avoid sw generated frame sequence the same as H/W generated frame,
     * the value lower than min_sw_seq is reserved for HW generated frame */ 
    if ((ni->ni_txseqs[IEEE80211_NON_QOS_SEQ]& IEEE80211_SEQ_MASK) < MIN_SW_SEQ){
        ni->ni_txseqs[IEEE80211_NON_QOS_SEQ] = MIN_SW_SEQ;  
    }
    *(u_int16_t *)&wh->i_seq[0] =
        htole16(ni->ni_txseqs[IEEE80211_NON_QOS_SEQ] << IEEE80211_SEQ_SEQ_SHIFT);
    ni->ni_txseqs[IEEE80211_NON_QOS_SEQ]++;
#undef WH4
}

static wbuf_t
ieee80211_getmgtframe(struct ieee80211_node *ni, int subtype, u_int8_t **frm, u_int8_t isboardcast)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    wbuf_t wbuf;
    struct ieee80211_frame *wh;
    u_int8_t broadcast_addr[IEEE80211_ADDR_LEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    wbuf = wbuf_alloc(ic->ic_osdev, WBUF_TX_MGMT, MAX_TX_RX_PACKET_SIZE);
    if (wbuf == NULL)
        return NULL;

    /* setup the wireless header */
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    if (isboardcast) {
        subtype = IEEE80211_FC0_SUBTYPE_ACTION;
        ieee80211_send_setup(vap, ni, wh,
                             IEEE80211_FC0_TYPE_MGT | subtype,
                             vap->iv_myaddr, broadcast_addr, ni->ni_bssid);

    } else {
        ieee80211_send_setup(vap, ni, wh,
                             IEEE80211_FC0_TYPE_MGT | subtype,
                             vap->iv_myaddr, ni->ni_macaddr, ni->ni_bssid);
    }
    *frm = (u_int8_t *)&wh[1];
    return wbuf;
}

int
ieee80211_send_mgmt(struct ieee80211vap *vap,struct ieee80211_node *ni, wbuf_t wbuf, bool force_send)
{
    struct ieee80211com      *ic = vap->iv_ic;
    u_int8_t  subtype;
    int retval;

    wbuf_set_node(wbuf, ieee80211_ref_node(ni));

    /* 
     * if forced sleep is set then turn on the powersave
     * bit on all management except for the probe request.
     */
    if (ieee80211_vap_forced_sleep_is_set(vap)) {
        struct ieee80211_frame *wh;
        wh = (struct ieee80211_frame *)wbuf_header(wbuf);
        subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
         
        if (subtype != IEEE80211_FC0_SUBTYPE_PROBE_REQ) {
            wh->i_fc[1] |= IEEE80211_FC1_PWR_MGT;
            wbuf_set_pwrsaveframe(wbuf);
        }
    }

    /*
     * call registered function to add any additional IEs.
     */
    if (vap->iv_output_mgmt_filter) {
        if (vap->iv_output_mgmt_filter(wbuf)) {
            /* 
             * filtered out and freed by the filter function,
             * nothing to do, just return.
             */ 
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER,
                              "[%s] frame filtered out; do not send\n",
                              __func__);
            return EOK;
        }
    }
    vap->iv_lastdata = OS_GET_TIMESTAMP();

    ieee80211_sta_power_tx_start(vap->iv_pwrsave_sta);
    /*
     * do not sent the frame is node is in power save (or) if the vap is paused
     * and the frame is is not marked as special force_send frame, and if the node
     * is temporary, don't do pwrsave
     */
    if (!force_send && 
          (ieee80211node_is_paused(ni)) &&
          !ieee80211node_has_flag(ni, IEEE80211_NODE_TEMP)) {
        wbuf_set_node(wbuf, NULL);
        ieee80211node_pause(ni); /* pause it to make sure that no one else unpaused it after the node_is_paused check above, pause operation is ref counted */  
        ieee80211_node_saveq_queue(ni,wbuf,IEEE80211_FC0_TYPE_MGT);
        ieee80211node_unpause(ni); /* unpause it if we are the last one, the frame will be flushed out */  
        ieee80211_free_node(ni);
        ieee80211_sta_power_tx_end(vap->iv_pwrsave_sta);
        return EOK;
    }

    /*
     * if the vap is not ready drop the frame.
     */
    if (!ieee80211_vap_active_is_set(vap)) {
        struct ieee80211_tx_status ts;
        ts.ts_flags = IEEE80211_TX_ERROR; 
        ts.ts_retries=0;
        /* 
         * complete buf will decrement the pending count.
         */
        ieee80211_complete_wbuf(wbuf,&ts);
        ieee80211_sta_power_tx_end(vap->iv_pwrsave_sta);
        return EOK;
    }

    retval = ic->ic_mgtstart(ic, wbuf);
    ieee80211_sta_power_tx_end(vap->iv_pwrsave_sta);
    
    return retval;
}

/*
 * Send a null data frame to the specified node.
 */
int
ieee80211_send_nulldata(struct ieee80211_node *ni, int pwr_save)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    wbuf_t wbuf;
    struct ieee80211_frame *wh;
    if(ieee80211_chan2ieee(ic,ieee80211_get_home_channel(vap)) !=
          ieee80211_chan2ieee(ic,ieee80211_get_current_channel(ic)))
	{
		
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
				  "%s[%d] cur chan %d flags 0x%x  is not same as home chan %d flags 0x%x \n",
				  __func__, __LINE__,
                          ieee80211_chan2ieee(ic, ic->ic_curchan),ieee80211_chan_flags(ic->ic_curchan), 
                          ieee80211_chan2ieee(ic, vap->iv_bsschan), ieee80211_chan_flags(vap->iv_bsschan));
		return EOK;
	}

    /*
     * XXX: It's the same as a management frame in the sense that
     * both are self-generated frames.
     */
    wbuf = wbuf_alloc(ic->ic_osdev, WBUF_TX_MGMT, sizeof(struct ieee80211_frame));
    if (wbuf == NULL)
        return -ENOMEM;

    /* setup the wireless header */
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    ieee80211_send_setup(vap, ni, wh,
                         IEEE80211_FC0_TYPE_DATA | IEEE80211_FC0_SUBTYPE_NODATA,
                         vap->iv_myaddr, ieee80211_node_get_macaddr(ni),
                         ieee80211_node_get_bssid(ni));

    /* NB: power management bit is never sent by an AP */
    if (pwr_save &&
        (vap->iv_opmode == IEEE80211_M_STA ||
         vap->iv_opmode == IEEE80211_M_IBSS)) {
        wh->i_fc[1] |= IEEE80211_FC1_PWR_MGT;
        wbuf_set_pwrsaveframe(wbuf);
    }

    if (vap->iv_ccx_evtable && vap->iv_ccx_evtable->wlan_ccx_vperf_pause){
        vap->iv_ccx_evtable->wlan_ccx_vperf_pause(vap->iv_ccx_arg, pwr_save);
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG | IEEE80211_MSG_DUMPPKTS,
                      "[%s] send null data frame on channel %u, pwr mgt %s\n",
                      ether_sprintf(ni->ni_macaddr),
                      ieee80211_chan2ieee(ic, ic->ic_curchan),
                      wh->i_fc[1] & IEEE80211_FC1_PWR_MGT ? "ena" : "dis");

    wbuf_set_pktlen(wbuf, sizeof(struct ieee80211_frame));
    wbuf_set_priority(wbuf, WME_AC_BE);

    vap->iv_lastdata = OS_GET_TIMESTAMP();
    /* null data  can not go out if the vap is in forced pause state */
    if (ieee80211_vap_is_force_paused(vap)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER,
                          "[%s] queue null data frame the vap is in forced pause state\n",__func__);
      ieee80211_node_saveq_queue(ni,wbuf,IEEE80211_FC0_TYPE_MGT);
      return EOK;
    } else {
       if (vap->iv_opmode == IEEE80211_M_STA ||
           vap->iv_opmode == IEEE80211_M_IBSS) {
           /* force send null data */
           return ieee80211_send_mgmt(vap,ni, wbuf, true);
       }
       else {
           /* allow power save for null data */
           return ieee80211_send_mgmt(vap,ni, wbuf, false);
       }
    }
}


/*
 * Send a probe request frame with the specified ssid
 * and any optional information element data.
 */
int
ieee80211_send_probereq(
    struct ieee80211_node *ni,
    const u_int8_t        *sa,
    const u_int8_t        *da,
    const u_int8_t        *bssid,
    const u_int8_t        *ssid, 
    const u_int32_t       ssidlen,
    const void            *optie, 
    const size_t          optielen)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    wbuf_t wbuf;
    enum ieee80211_phymode mode;
    struct ieee80211_frame *wh;
    u_int8_t *frm;

    wbuf = wbuf_alloc(ic->ic_osdev, WBUF_TX_MGMT, MAX_TX_RX_PACKET_SIZE);
    if (wbuf == NULL)
        return -ENOMEM;

    /* setup the wireless header */
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    ieee80211_send_setup(vap, ni, wh,
                         IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_PROBE_REQ,
                         sa, da, bssid);
    frm = (u_int8_t *)&wh[1];
    
    /*
     * prreq frame format
     *[tlv] ssid
     *[tlv] supported rates
     *[tlv] extended supported rates
     *[tlv] user-specified ie's
     */
    frm = ieee80211_add_ssid(frm, ssid, ssidlen);
    mode = ieee80211_get_current_phymode(ic);
    /* XXX: supported rates or operational rates? */
    frm = ieee80211_add_rates(frm, &vap->iv_op_rates[mode]);
    frm = ieee80211_add_xrates(frm, &vap->iv_op_rates[mode]);

    if (optie != NULL) {
        OS_MEMCPY(frm, optie, optielen);
        frm += optielen;
    }

    IEEE80211_VAP_LOCK(vap);
    if (vap->iv_app_ie[IEEE80211_FRAME_TYPE_PROBEREQ].length) {
        OS_MEMCPY(frm, vap->iv_app_ie[IEEE80211_FRAME_TYPE_PROBEREQ].ie, 
                  vap->iv_app_ie[IEEE80211_FRAME_TYPE_PROBEREQ].length);
        frm += vap->iv_app_ie[IEEE80211_FRAME_TYPE_PROBEREQ].length;
    }
    /* Add the Application IE's */
    frm = ieee80211_mlme_app_ie_append(vap, IEEE80211_FRAME_TYPE_PROBEREQ, frm);
    IEEE80211_VAP_UNLOCK(vap);

    wbuf_set_pktlen(wbuf, (frm - (u_int8_t *)wbuf_header(wbuf)));
    /*
     * send the frame out even if the vap is opaused.
     */ 
    return ieee80211_send_mgmt(vap,ni, wbuf,true);
}

/*
 * Send a authentication frame
 */
int
ieee80211_send_auth(
    struct ieee80211_node *ni,
    u_int16_t seq,
    u_int16_t status,
    u_int8_t *challenge_txt,
    u_int8_t challenge_len
    )
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211_rsnparms *rsn;
    wbuf_t wbuf;
    struct ieee80211_frame *wh;
    u_int8_t *frm;

    wbuf = wbuf_alloc(ic->ic_osdev, WBUF_TX_MGMT, MAX_TX_RX_PACKET_SIZE);
    if (wbuf == NULL)
        return -ENOMEM;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_AUTH,
                          "[%s] send auth frmae \n ", ether_sprintf(ni->ni_macaddr)); 

#ifdef IEEE80211_DEBUG_REFCNT
    printk("%s ,line %u: increase node %p <%s> refcnt to %d\n",
           __func__, __LINE__, ni, ether_sprintf(ni->ni_macaddr),
           ieee80211_node_refcnt(ni));
#endif
    
    /* setup the wireless header */
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    ieee80211_send_setup(vap, ni, wh,
                         IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_AUTH,
                         vap->iv_myaddr, ni->ni_macaddr, ni->ni_bssid);
    frm = (u_int8_t *)&wh[1];

    /*
     * auth frame format
     *[2] algorithm
     *[2] sequence
     *[2] status
     *[tlv*] challenge
     */
    // MP
    // Regardless of iv_opmode, we should always take iv_rsn
    // because of iv_rsn is our configuration. so commenting 
    // below line and correct code in next line
    // rsn = (vap->iv_opmode == IEEE80211_M_STA) ? &vap->iv_rsn : &vap->iv_bss->ni_rsn;
    rsn = &vap->iv_rsn;

    /* when auto auth is set in ap dont send shared auth response
     * for open auth request. In sta mode send shared auth response
     *  only mode is shared and ni == vap->iv_bss
     */

    if (RSN_AUTH_IS_SHARED_KEY(rsn) && (ni == vap->iv_bss || ni->ni_authmode == IEEE80211_AUTH_SHARED)) {
        *((u_int16_t *)frm) = htole16(IEEE80211_AUTH_ALG_SHARED);
        frm += 2;
    } else {
        *((u_int16_t *)frm) = htole16(IEEE80211_AUTH_ALG_OPEN);
        frm += 2;
    }
    *((u_int16_t *)frm) = htole16(seq); frm += 2;
    *((u_int16_t *)frm) = htole16(status); frm += 2;
    if (challenge_txt != NULL && challenge_len != 0) {
        ASSERT(RSN_AUTH_IS_SHARED_KEY(rsn));
        
        *((u_int16_t *)frm) = htole16((challenge_len << 8) | IEEE80211_ELEMID_CHALLENGE);
        frm += 2;
        OS_MEMCPY(frm, challenge_txt, challenge_len);
        frm += challenge_len;

        if (seq == IEEE80211_AUTH_SHARED_RESPONSE) {
            /* We also need to turn on WEP bit of the frame */
            wh->i_fc[1] |= IEEE80211_FC1_WEP;
        }
    }
    
    wbuf_set_pktlen(wbuf, (frm - (u_int8_t *)wbuf_header(wbuf)));

    return ieee80211_send_mgmt(vap,ni, wbuf,false);
}

/*
 * Send a deauth frame
 */
int
ieee80211_send_deauth(struct ieee80211_node *ni, u_int16_t reason)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    wbuf_t wbuf;
    struct ieee80211_frame *wh;
    u_int8_t *frm;

    wbuf = wbuf_alloc(ic->ic_osdev, WBUF_TX_MGMT, sizeof(struct ieee80211_frame));
    if (wbuf == NULL)
        return -ENOMEM;

#if ATH_SUPPORT_WAPI
    if (vap->iv_opmode == IEEE80211_M_STA) {
        if (ieee80211_vap_wapi_is_set(vap)) {
            /* clear the WAPI flag in vap */
            ieee80211_vap_wapi_clear(vap);
        }
    }
#endif

#ifdef IEEE80211_DEBUG_REFCNT
    printk("%s ,line %u: increase node %p <%s> refcnt to %d\n",
           __func__, __LINE__, ni, ether_sprintf(ni->ni_macaddr),
           ieee80211_node_refcnt(ni));
#endif
    
    /* setup the wireless header */
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    ieee80211_send_setup(vap, ni, wh,
                         IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_DEAUTH,
                         vap->iv_myaddr, ni->ni_macaddr, ni->ni_bssid);

    if ((vap->iv_ccx_evtable && vap->iv_ccx_evtable->wlan_ccx_is_mfp &&
        vap->iv_ccx_evtable->wlan_ccx_is_mfp(vap->iv_ifp)) ||
        (ieee80211_vap_mfp_test_is_set(vap) && ni->ni_ucastkey.wk_valid)) {
        /* MFP is enabled and a key is established. */
        /* We need to turn on WEP bit of the frame */
        wh->i_fc[1] |= IEEE80211_FC1_WEP;
    }

    frm = (u_int8_t *)&wh[1];
    *(u_int16_t *)frm = htole16(reason);
    frm += 2;
    
    wbuf_set_pktlen(wbuf, (frm - (u_int8_t *)wbuf_header(wbuf)));

    return ieee80211_send_mgmt(vap,ni, wbuf,false);
}

/*
 * Send a disassociate frame
 */
int
ieee80211_send_disassoc(struct ieee80211_node *ni, u_int16_t reason)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    wbuf_t wbuf;
    struct ieee80211_frame *wh;
    u_int8_t *frm;

    wbuf = wbuf_alloc(ic->ic_osdev, WBUF_TX_MGMT, MAX_TX_RX_PACKET_SIZE);
    if (wbuf == NULL)
        return -ENOMEM;

#if ATH_SUPPORT_WAPI
    if (vap->iv_opmode == IEEE80211_M_STA) {
        if (ieee80211_vap_wapi_is_set(vap)) {
            /* clear the WAPI flag in vap */
            ieee80211_vap_wapi_clear(vap);
        }
    }
#endif

#ifdef IEEE80211_DEBUG_REFCNT
    printk("%s ,line %u: increase node %p <%s> refcnt to %d\n",
           __func__, __LINE__, ni, ether_sprintf(ni->ni_macaddr),
           ieee80211_node_refcnt(ni));
#endif
    
    /* setup the wireless header */
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    ieee80211_send_setup(vap, ni, wh,
                         IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_DISASSOC,
                         vap->iv_myaddr, ni->ni_macaddr, ni->ni_bssid);

    if ((vap->iv_ccx_evtable && vap->iv_ccx_evtable->wlan_ccx_is_mfp &&
        vap->iv_ccx_evtable->wlan_ccx_is_mfp(vap->iv_ifp)) ||
        (ieee80211_vap_mfp_test_is_set(vap) && ni->ni_ucastkey.wk_valid)) {
        /* MFP is enabled and a key is established. */
        /* We need to turn on WEP bit of the frame */
        wh->i_fc[1] |= IEEE80211_FC1_WEP;
    }

    frm = (u_int8_t *)&wh[1];
    *(u_int16_t *)frm = htole16(reason);
    frm += 2;
    
    wbuf_set_pktlen(wbuf, (frm - (u_int8_t *)wbuf_header(wbuf)));

    return ieee80211_send_mgmt(vap,ni, wbuf,false);
}


int
ieee80211_send_action(
    struct ieee80211_node *ni,
    struct ieee80211_action_mgt_args *actionargs,
    struct ieee80211_action_mgt_buf  *actionbuf
    )
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;
    int error = EOK;

    switch (actionargs->category) {
#if ATH_SUPPORT_IBSS_DFS
    case IEEE80211_ACTION_CAT_SPECTRUM: {
        struct ieee80211_action_spectrum_channel_switch *csaaction;
        struct ieee80211_action_measrep_header *measrep_header;
        struct ieee80211_measurement_report_ie *measrep_ie;
        struct ieee80211_measurement_report_ie *fill_in_measrep_ie;
        switch(actionargs->action) {
        case IEEE80211_ACTION_CHAN_SWITCH:
            wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 1);
            if (wbuf == NULL) {
                error = -ENOMEM;
                break;
            }
           
            csaaction = (struct ieee80211_action_spectrum_channel_switch *)frm;
            frm += sizeof(struct ieee80211_action_spectrum_channel_switch);
            csaaction->csa_header.ia_category     = IEEE80211_ACTION_CAT_SPECTRUM;
            csaaction->csa_header.ia_action       = actionargs->action;
            csaaction->csa_element.ie             = IEEE80211_ELEMID_CHANSWITCHANN;
            csaaction->csa_element.len            = IEEE80211_CHANSWITCHANN_BYTES - sizeof(struct ieee80211_ie_header);
            csaaction->csa_element.switchmode     = 1; 
            csaaction->csa_element.newchannel     = ic->ic_chanchange_chan;
            csaaction->csa_element.tbttcount      = ic->ic_chanchange_tbtt;
            
            break;
        case IEEE80211_ACTION_MEAS_REPORT:
            wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 1);
            if (wbuf == NULL) {
                error = -ENOMEM;
                break;
            }

            /* fill header */
            measrep_header = (struct ieee80211_action_measrep_header *)frm;
            frm += sizeof(struct ieee80211_action_measrep_header);
            measrep_header->action_header.ia_category = IEEE80211_ACTION_CAT_SPECTRUM;
            measrep_header->action_header.ia_action   = IEEE80211_ACTION_MEAS_REPORT;
            measrep_header->dialog_token              = actionargs->arg1;  /* always 0 for now */
            
            /* fill IE */
            if (actionargs->arg4 == NULL) {
                error = -EINVAL;
                break;
            }
            fill_in_measrep_ie = (struct ieee80211_measurement_report_ie *)actionargs->arg4;
            measrep_ie = (struct ieee80211_measurement_report_ie *)frm;
            frm += sizeof(struct ieee80211_measurement_report_ie); 
            OS_MEMCPY(measrep_ie, fill_in_measrep_ie, (fill_in_measrep_ie->len + sizeof(struct ieee80211_ie_header)));
             
        break;
        default:
            IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                           "%s: invalid spectrum action mgt frame", __func__);
            error = -EINVAL;
            break;        
        }
        
        break;
    }
#endif /* ATH_SUPPORT_IBSS_DFS */

    case IEEE80211_ACTION_CAT_QOS:
        IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                       "%s: QoS action mgt frames not supported", __func__);
        error = -EINVAL;
        break;

    case IEEE80211_ACTION_CAT_BA: {
        struct ieee80211_action_ba_addbarequest *addbarequest;
        struct ieee80211_action_ba_addbaresponse *addbaresponse;
        struct ieee80211_action_ba_delba *delba;
        struct ieee80211_ba_parameterset baparamset;
        struct ieee80211_ba_seqctrl basequencectrl;
        struct ieee80211_delba_parameterset delbaparamset;
        u_int16_t batimeout;
        u_int16_t statuscode;
        u_int16_t reasoncode;
        u_int16_t buffersize;
        u_int8_t tidno;
        u_int16_t temp;
        
        /* extract TID */
        tidno = actionargs->arg1;
        switch (actionargs->action) {
        case IEEE80211_ACTION_BA_ADDBA_REQUEST:
            wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);
            if (wbuf == NULL) {
                error = -ENOMEM;
                break;
            }

            addbarequest = (struct ieee80211_action_ba_addbarequest *)frm;
            frm += sizeof(struct ieee80211_action_ba_addbarequest);
            
            addbarequest->rq_header.ia_category     = IEEE80211_ACTION_CAT_BA;
            addbarequest->rq_header.ia_action       = actionargs->action;
            addbarequest->rq_dialogtoken            = tidno + 1;
            buffersize                              = actionargs->arg2;
            
            ic->ic_addba_requestsetup(ni, tidno,
                                      &baparamset,
                                      &batimeout,
                                      &basequencectrl,
                                      buffersize);
            /* "struct ieee80211_action_ba_addbarequest" is annotated __packed, 
               if accessing fields, like rq_baparamset or rq_basequencectrl, 
               by using u_int16_t* directly, it will cause byte alignment issue.
               Some platform that cannot handle this issue will cause exception.
               Use OS_MEMCPY to move data byte by byte */
            temp = htole16(*(u_int16_t *)&baparamset);
            OS_MEMCPY(&addbarequest->rq_baparamset, &temp, sizeof(u_int16_t)); 
            addbarequest->rq_batimeout = htole16(batimeout);
            temp =htole16(*(u_int16_t *)&basequencectrl);
            OS_MEMCPY(&addbarequest->rq_basequencectrl, &temp, sizeof(u_int16_t));

            IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                           "%s: ADDBA request action mgt frame. TID %d, buffer size %d",
                           __func__, tidno, baparamset.buffersize);
            break;

        case IEEE80211_ACTION_BA_ADDBA_RESPONSE:
            wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);
            if (wbuf == NULL) {
                error = -ENOMEM;
                break;
            }

            addbaresponse = (struct ieee80211_action_ba_addbaresponse *)frm;
            frm += sizeof(struct ieee80211_action_ba_addbaresponse);
            
            addbaresponse->rs_header.ia_category    = IEEE80211_ACTION_CAT_BA;
            addbaresponse->rs_header.ia_action      = actionargs->action;

            ic->ic_addba_responsesetup(ni, tidno,
                                       &addbaresponse->rs_dialogtoken,
                                       &statuscode,
                                       &baparamset,
                                       &batimeout);
            /* "struct ieee80211_action_ba_addbaresponse" is annotated __packed, 
               if accessing fields, like rs_baparamset, by using u_int16_t* directly, 
               it will cause byte alignment issue.
               Some platform that cannot handle this issue will cause exception.
               Use OS_MEMCPY to move data byte by byte */
            temp = htole16(*(u_int16_t *)&baparamset);
            OS_MEMCPY(&addbaresponse->rs_baparamset, &temp, sizeof(u_int16_t)); 
            addbaresponse->rs_batimeout  = htole16(batimeout);
            addbaresponse->rs_statuscode = htole16(statuscode);

            IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                           "%s: ADDBA response action mgt frame. TID %d, buffer size %d, status %d",
                           __func__, tidno, baparamset.buffersize, statuscode);
            break;

        case IEEE80211_ACTION_BA_DELBA:
            wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);
            if (wbuf == NULL) {
                error = -ENOMEM;
                break;
            }

            delba = (struct ieee80211_action_ba_delba *)frm;
            frm += sizeof(struct ieee80211_action_ba_delba);
            
            delba->dl_header.ia_category = IEEE80211_ACTION_CAT_BA;
            delba->dl_header.ia_action = actionargs->action;

            delbaparamset.reserved0 = 0;
            delbaparamset.initiator = actionargs->arg2; 
            delbaparamset.tid = tidno;
            reasoncode = actionargs->arg3; 
            /* "struct ieee80211_action_ba_delba" is annotated __packed, 
               if accessing fields, like dl_delbaparamset, by using u_int16_t* directly, 
               it will cause byte alignment issue.
               Some platform that cannot handle this issue will cause exception.
               Use OS_MEMCPY to move data byte by byte */
            temp = htole16(*(u_int16_t *)&delbaparamset);
            OS_MEMCPY(&delba->dl_delbaparamset, &temp, sizeof(u_int16_t)); 
            delba->dl_reasoncode = htole16(reasoncode);

            IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                           "%s: DELBA action mgt frame. TID %d, initiator %d, reason %d",
                           __func__, tidno, delbaparamset.initiator, reasoncode);
            break;

        default:
            IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                           "%s: invalid BA action mgt frame", __func__);
            error = -EINVAL;
            break;
        }
        break;
    }
    case IEEE80211_ACTION_CAT_HT: {
        struct ieee80211_action_ht_txchwidth *txchwidth;
        struct ieee80211_action_ht_smpowersave *smpsframe;
#ifdef ATH_SUPPORT_TxBF
#ifdef TXBF_TODO
        struct ieee80211_action_ht_CSI *CSIframe;
#endif
#endif
        switch (actionargs->action) {
        case IEEE80211_ACTION_HT_TXCHWIDTH:
            {
                enum ieee80211_cwm_width cw_width = ic->ic_cwm_get_width(ic);
            IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                           "%s: HT txchwidth action mgt frame. Width %d",
                        __func__, cw_width);

            wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);
            if (wbuf == NULL) {
                error = -ENOMEM;
                break;
            }
            txchwidth = (struct ieee80211_action_ht_txchwidth *)frm;
            frm += sizeof(struct ieee80211_action_ht_txchwidth);

            txchwidth->at_header.ia_category = IEEE80211_ACTION_CAT_HT;
            txchwidth->at_header.ia_action  = IEEE80211_ACTION_HT_TXCHWIDTH;
                txchwidth->at_chwidth =  (cw_width == IEEE80211_CWM_WIDTH40) ?
                IEEE80211_A_HT_TXCHWIDTH_2040 : IEEE80211_A_HT_TXCHWIDTH_20;
            }
            break;
        case IEEE80211_ACTION_HT_SMPOWERSAVE:
            IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                           "%s: HT mimo pwr save action mgt frame", __func__);

            wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);
            if (wbuf == NULL) {
                error = -ENOMEM;
                break;
            }
            smpsframe = (struct ieee80211_action_ht_smpowersave *)frm;
            frm += sizeof(struct ieee80211_action_ht_smpowersave);

            smpsframe->as_header.ia_category = IEEE80211_ACTION_CAT_HT;
            smpsframe->as_header.ia_action 	 = IEEE80211_ACTION_HT_SMPOWERSAVE;
            smpsframe->as_control =  (actionargs->arg1 << 0) | (actionargs->arg2 << 1);

            /* Mark frame for appropriate action on completion */
            wbuf_set_smpsactframe(wbuf);		    
            break;
            
#ifdef ATH_SUPPORT_TxBF
#ifdef TXBF_TODO
		case IEEE80211_ACTION_HT_CSI:
			/* Send CSI Frame 
			actionargs.category = IEEE80211_ACTION_CAT_HT;
			actionargs.action   = IEEE80211_ACTION_HT_CSI;
			actionargs.arg1     = buf_len;
			actionargs.arg2     = CSI_buf;
			OS_MEMCPY(actionargs.buf, MIMO_Control, MIMO_Control_Len);
			*/
			IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                "%s: HT CSI action mgt frame", __func__);
            wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);
		    if (wbuf == NULL) {
		        error = -ENOMEM;
		        break;
		    }
			CSIframe = (struct ieee80211_action_ht_CSI *)frm;
            frm += sizeof(struct ieee80211_action_ht_CSI);
			/* HT - CSI Frame 
			struct ieee80211_action_ht_CSI {
				struct ieee80211_action     as_header;
				u_int8_t                    MIMO_Control[MIMO_Control_Len];
			} __packed;
			*/
            CSIframe->as_header.ia_category = IEEE80211_ACTION_CAT_HT;
            CSIframe->as_header.ia_action  = IEEE80211_ACTION_HT_CSI;
            OS_MEMCPY(CSIframe->mimo_control, actionbuf->buf,  MIMO_CONTROL_LEN);
			/*Copy CSI Report*/
			OS_MEMCPY(frm, actionargs->arg4, actionargs->arg1);
			frm += actionargs->arg1;
			break;
#endif
#endif
        default:
            IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                           "%s: invalid HT action mgt frame", __func__);
            error = -EINVAL;
            break;
        }
        break;
    }
    case IEEE80211_ACTION_CAT_WMM_QOS: {
        struct ieee80211_action_wmm_qos *tsframe;
        struct ieee80211_wme_tspec *tsdata = (struct ieee80211_wme_tspec *) &actionbuf->buf;
        struct ieee80211_frame *wh;
        u_int8_t    tsrsiev[16];
        u_int8_t    tsrsvlen = 0;
        u_int32_t   minphyrate;

        /* TSPEC action mamangement frames */
        switch (actionargs->action) {
        case IEEE80211_WMM_QOS_ACTION_SETUP_REQ:
        case IEEE80211_WMM_QOS_ACTION_TEARDOWN:
            wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);
            if (wbuf == NULL) {
                error = -ENOMEM;
                break;
            }
            wh = (struct ieee80211_frame*)wbuf_header(wbuf);

            tsframe = (struct ieee80211_action_wmm_qos *)frm;
            tsframe->ts_header.ia_category = IEEE80211_ACTION_CAT_WMM_QOS;
            tsframe->ts_header.ia_action = actionargs->action;
            tsframe->ts_dialogtoken = IEEE80211_WMM_QOS_DIALOG_SETUP;
            tsframe->ts_statuscode = 0;
            /* fill in the basic structure for tspec IE */
            frm = ieee80211_add_wmeinfo((u_int8_t *) &tsframe->ts_tspecie, ni,
                                        WME_TSPEC_OUI_SUBTYPE, (u_int8_t *) &tsdata->ts_tsinfo,
                                        sizeof (struct ieee80211_wme_tspec) - offsetof(struct ieee80211_wme_tspec, ts_tsinfo));
            if (actionargs->action == IEEE80211_WMM_QOS_ACTION_SETUP_REQ) {
                /* Save the tspec to be used in next assoc request */
                if (((struct ieee80211_tsinfo_bitmap *)(&tsdata->ts_tsinfo))->tid == IEEE80211_WMM_QOS_TSID_SIG_TSPEC)
                    OS_MEMCPY(&ic->ic_sigtspec, (u_int8_t *) &tsframe->ts_tspecie, sizeof(struct ieee80211_wme_tspec));
                else 
                    OS_MEMCPY(&ic->ic_datatspec, (u_int8_t *) &tsframe->ts_tspecie, sizeof(struct ieee80211_wme_tspec));
                minphyrate = *((u_int32_t *) &tsdata->ts_min_phy[0]);
                if (vap->iv_ccx_evtable && vap->iv_ccx_evtable->wlan_ccx_fill_tsrsie) {
                    vap->iv_ccx_evtable->wlan_ccx_fill_tsrsie(vap->iv_ccx_arg, 
                         ((struct ieee80211_tsinfo_bitmap *) &tsdata->ts_tsinfo[0])->tid, 
                         minphyrate, &tsrsiev[0], &tsrsvlen);
                }
                if (tsrsvlen > 0) {
                    *frm++ = IEEE80211_ELEMID_VENDOR;
                    *frm++ = tsrsvlen;
                    OS_MEMCPY(frm, &tsrsiev[0], tsrsvlen);
                    frm += tsrsvlen;
                }
            } else if (actionargs->action == IEEE80211_WMM_QOS_ACTION_TEARDOWN) {
                /* if sending DELTS, wipeout stored TSPECs unconditionally */
                OS_MEMZERO(&ic->ic_sigtspec, sizeof(struct ieee80211_wme_tspec));
                OS_MEMZERO(&ic->ic_datatspec, sizeof(struct ieee80211_wme_tspec));
                /* Disable QoS handling internally */
                wlan_set_tspecActive(vap, 0);
                if (vap->iv_ccx_evtable && vap->iv_ccx_evtable->wlan_ccx_set_vperf) {
                    vap->iv_ccx_evtable->wlan_ccx_set_vperf(vap->iv_ccx_arg, 0);
                }
            }
            if (vap->iv_ccx_evtable && vap->iv_ccx_evtable->wlan_ccx_is_mfp &&
                vap->iv_ccx_evtable->wlan_ccx_is_mfp(vap->iv_ifp)) {
                /* MFP is enabled and a key is established. */
                /* We need to turn on WEP bit of the frame */
                wh->i_fc[1] |= IEEE80211_FC1_WEP;
            }
            break;

        case IEEE80211_WMM_QOS_ACTION_SETUP_RESP:
            /* Station will never send out a response. No code needed for now */
            break;

        default:
            break;
        }
    }
        break;

    case IEEE80211_ACTION_CAT_COEX: {
        if (vap->iv_opmode == IEEE80211_M_STA) 
        {
            switch(actionargs->action)
            {
            case 0:
                if (!(ic->ic_flags & IEEE80211_F_COEXT_DISABLE)) {
                    IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                           "%s: HT 2040 coexist action mgt frame.",__func__);
                           
                    do {
                        struct ieee80211_action *header;
                        struct ieee80211_ie_bss_coex *coexist;
                        struct ieee80211_ie_intolerant_report *intolerantchanreport;
                        u_int8_t *p;
                        u_int32_t i;

                        wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);
                        if (wbuf == NULL) {
                            error = -ENOMEM;
                            break;
                        }

                        header = (struct ieee80211_action *)frm;
                        header->ia_category = IEEE80211_ACTION_CAT_COEX;
                        header->ia_action = actionargs->action;

                        frm += sizeof(struct ieee80211_action);

                        coexist = (struct ieee80211_ie_bss_coex *)frm;
                        OS_MEMZERO(coexist, sizeof(struct ieee80211_ie_bss_coex));
                        coexist->elem_id = IEEE80211_ELEMID_2040_COEXT;
                        coexist->elem_len = 1;
                        coexist->ht20_width_req = actionargs->arg1;

                        frm += sizeof(struct ieee80211_ie_bss_coex);

                        intolerantchanreport = (struct ieee80211_ie_intolerant_report*)frm;

                        intolerantchanreport->elem_id = IEEE80211_ELEMID_2040_INTOL;
                        intolerantchanreport->elem_len = actionargs->arg3 + 1;
                        intolerantchanreport->reg_class = actionargs->arg2;
                        p = intolerantchanreport->chan_list;
                        for (i=0; i<actionargs->arg3; i++) {
                            *p++ = actionbuf->buf[i];
                        }

                        frm += intolerantchanreport->elem_len + 2;
                    } while (FALSE);

                } else {
                    error = -EINVAL;
                }
                break;
            default:
                IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                   "%s: action mgt frame has invalid action %d", __func__, actionargs->action);
                error = -EINVAL;
                break;
            }
         }
     }
         break;

    default:
        IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                       "%s: action mgt frame has invalid category %d", __func__, actionargs->category);
        error = -EINVAL;
        break;
    }

    if (error == EOK) {
        ASSERT(wbuf != NULL);
        ASSERT(frm != NULL);
        
        if (ieee80211_vap_mfp_test_is_set(vap) && ni->ni_ucastkey.wk_valid) {
            struct ieee80211_frame *wh;
            wh = (struct ieee80211_frame*)wbuf_header(wbuf);
            wh->i_fc[1] |= IEEE80211_FC1_WEP;
        }

        wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));

#ifdef IEEE80211_DEBUG_REFCNT
        printk("%s ,line %u: increase node %p <%s> refcnt to %d\n",
               __func__, __LINE__, ni, ether_sprintf(ni->ni_macaddr),
               ieee80211_node_refcnt(ni));
#endif
        
        error = ieee80211_send_mgmt(vap,ni, wbuf,false);
    }
    
    return error;
}

#ifdef ATH_SUPPORT_TxBF
int
ieee80211_send_v_cv_action(struct ieee80211_node *ni, u_int8_t *data_buf, u_int16_t buf_len)
{
    struct ieee80211vap *vap = ni->ni_vap;
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;
    int error = 0;

    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);
    if (wbuf == NULL) {
        return -ENOMEM;
    }
    ASSERT(frm != NULL);

    /*Copy V/CV Report*/
    OS_MEMCPY(frm, data_buf, buf_len);
    frm += buf_len;

    wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));
    error = ieee80211_send_mgmt(vap, ni, wbuf, false);

    return error;
}
#endif

/*
 * Send a BAR frame
 */
int
ieee80211_send_bar(struct ieee80211_node *ni, u_int8_t tidno, u_int16_t seqno)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    wbuf_t wbuf;
    struct ieee80211_frame_bar *bar;

    /*
     * XXX: It's the same as a management frame in the sense that
     * both are self-generated frames.
     */
    wbuf = wbuf_alloc(ic->ic_osdev, WBUF_TX_MGMT, MAX_TX_RX_PACKET_SIZE);
    if (wbuf == NULL)
        return -ENOMEM;


#ifdef IEEE80211_DEBUG_REFCNT
    printk("%s ,line %u: increase node %p <%s> refcnt to %d\n",
           __func__, __LINE__, ni, ether_sprintf(ni->ni_macaddr),
           ieee80211_node_refcnt(ni));
#endif
    
    /* setup the wireless header */
    bar = (struct ieee80211_frame_bar *)wbuf_header(wbuf);
    bar->i_fc[1] = IEEE80211_FC1_DIR_NODS;
    bar->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_CTL | IEEE80211_FC0_SUBTYPE_BAR;
    bar->i_ctl = htole16((tidno << IEEE80211_BAR_CTL_TID_S) | IEEE80211_BAR_CTL_COMBA);
    bar->i_seq = htole16(seqno << IEEE80211_SEQ_SEQ_SHIFT);
    IEEE80211_ADDR_COPY(bar->i_ra, ni->ni_macaddr);
    IEEE80211_ADDR_COPY(bar->i_ta, vap->iv_myaddr);
    
    wbuf_set_pktlen(wbuf, sizeof(struct ieee80211_frame_bar));
    wbuf_set_tid(wbuf, tidno);

    return ieee80211_send_mgmt(vap,ni, wbuf,false);
}

/*
 * Send a PS-POLL to the specified node.
 */
int
ieee80211_send_pspoll(struct ieee80211_node *ni)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    wbuf_t wbuf;
    struct ieee80211_frame_pspoll *pspoll;

    /*
     * XXX: It's the same as a management frame in the sense that
     * both are self-generated frames.
     */
    wbuf = wbuf_alloc(ic->ic_osdev, WBUF_TX_MGMT, MAX_TX_RX_PACKET_SIZE);
    if (wbuf == NULL)
        return -ENOMEM;


    /* setup the wireless header */
    pspoll = (struct ieee80211_frame_pspoll *)wbuf_header(wbuf);
    pspoll->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_CTL | IEEE80211_FC0_SUBTYPE_PS_POLL;
    pspoll->i_fc[1] = IEEE80211_FC1_DIR_NODS | IEEE80211_FC1_PWR_MGT;
    /* Set bits 14 and 15 to 1 when duration field carries Association ID */
    *(u_int16_t *)pspoll->i_aid = htole16(ni->ni_associd | IEEE80211_FIELD_TYPE_AID);
    IEEE80211_ADDR_COPY(pspoll->i_bssid, ni->ni_bssid);
    IEEE80211_ADDR_COPY(pspoll->i_ta, vap->iv_myaddr);
    
    wbuf_set_pwrsaveframe(wbuf);
    wbuf_set_pktlen(wbuf, sizeof(struct ieee80211_frame_pspoll));

    vap->iv_lastdata = OS_GET_TIMESTAMP();

    /* pspoll can not go out if the vap is in forced pause state */
    if (ieee80211_vap_is_force_paused(vap)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER,
                          "[%s] queue null data frame the vap is in forced pause state\n",__func__);
      ieee80211_node_saveq_queue(ni,wbuf,IEEE80211_FC0_TYPE_MGT);
      return EOK;
    } else {
       return ieee80211_send_mgmt(vap,ni, wbuf,true);
    } 
}

/*
 * Send a self-CTS frame
 */
int
ieee80211_send_cts(struct ieee80211_node *ni, int flags)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    wbuf_t wbuf;
    struct ieee80211_frame_cts *cts;

    /*
     * It's the same as a management frame in the sense that
     * both are self-generated frames.
     */
    wbuf = wbuf_alloc(ic->ic_osdev, WBUF_TX_MGMT, MAX_TX_RX_PACKET_SIZE);
    if (wbuf == NULL)
        return -ENOMEM;

    /* setup the wireless header */
    cts = (struct ieee80211_frame_cts *)wbuf_header(wbuf);
    cts->i_fc[1] = IEEE80211_FC1_DIR_NODS;
    cts->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_CTL | IEEE80211_FC0_SUBTYPE_CTS;
    IEEE80211_ADDR_COPY(cts->i_ra, ni->ni_macaddr);
    
    wbuf_set_pktlen(wbuf, sizeof(struct ieee80211_frame_cts));

    if (flags == IEEE80211_CTS_SMPS)
        wbuf_set_smpsframe(wbuf);

    return ieee80211_send_mgmt(vap, ni, wbuf, true);
}

/*
 * Return a prepared QoS NULL frame.
 */
void
ieee80211_prepare_qosnulldata(struct ieee80211_node *ni, wbuf_t wbuf, int ac)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211_qosframe *qwh;
    int tid;

    qwh = (struct ieee80211_qosframe *)wbuf_header(wbuf);

    ieee80211_send_setup(vap, ni, (struct ieee80211_frame *)qwh,
        IEEE80211_FC0_TYPE_DATA,
        vap->iv_myaddr, /* SA */
        ni->ni_macaddr, /* DA */
        ni->ni_bssid);

    qwh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA |
        IEEE80211_FC0_SUBTYPE_QOS_NULL;

    if (IEEE80211_VAP_IS_SLEEPING(ni->ni_vap) || ieee80211_vap_forced_sleep_is_set(vap)) {
        qwh->i_fc[1] |= IEEE80211_FC1_PWR_MGT;
    }

    /* map from access class/queue to 11e header priority value */
    tid = WME_AC_TO_TID(ac);
    qwh->i_qos[0] = tid & IEEE80211_QOS_TID;
    if (vap->iv_opmode != IEEE80211_M_STA && vap->iv_opmode != IEEE80211_M_IBSS)
        qwh->i_qos[0] |= IEEE80211_QOS_EOSP;

    if (ic->ic_wme.wme_wmeChanParams.cap_wmeParams[ac].wmep_noackPolicy)
    {
        qwh->i_qos[0] |= (1 << IEEE80211_QOS_ACKPOLICY_S) & IEEE80211_QOS_ACKPOLICY;
    }

    qwh->i_qos[1] = 0;
    if (vap->iv_ique_ops.hbr_set_probing)
        vap->iv_ique_ops.hbr_set_probing(vap, ni, wbuf, (u_int8_t)tid);            
}

/*
 * send a QoSNull frame
 */
int ieee80211_send_qosnulldata(struct ieee80211_node *ni, int ac, int pwr_save)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    wbuf_t wbuf;

    if (ieee80211_get_home_channel(vap) !=
        ieee80211_get_current_channel(ic))
    {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                          "%s[%d] cur chan %d is not same as home chan %d\n",
                          __func__, __LINE__,
                          ieee80211_chan2ieee(ic, ic->ic_curchan),ieee80211_chan2ieee(ic, vap->iv_bsschan));
        return EOK;
    }

    /*
     * XXX: It's the same as a management frame in the sense that
     * both are self-generated frames.
     */
    wbuf = wbuf_alloc(ic->ic_osdev, WBUF_TX_MGMT, sizeof(struct ieee80211_qosframe));
    if (wbuf == NULL)
        return -ENOMEM;

    ieee80211_prepare_qosnulldata(ni, wbuf, ac);

    wbuf_set_pktlen(wbuf, sizeof(struct ieee80211_qosframe));

    vap->iv_lastdata = OS_GET_TIMESTAMP();
    /* qos null data  can not go out if the vap is in forced pause state */
    if (ieee80211_vap_is_force_paused(vap)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER,
                          "[%s] queue null data frame the vap is in forced pause state\n",__func__);
      ieee80211_node_saveq_queue(ni,wbuf,IEEE80211_FC0_TYPE_MGT);
      return EOK;
    } else {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER,
                          "[%s] send qos null data frame \n",__func__);
       return ieee80211_send_mgmt(vap,ni, wbuf,true);
    }
}

/*
 * receive management processing code.
 */


static enum ieee80211_phymode
ieee80211_get_phy_type (
    struct ieee80211com               *ic,
    u_int8_t                          *rates,
    u_int8_t                          *xrates,
    struct ieee80211_ie_htcap_cmn     *htcap,
    struct ieee80211_ie_htinfo_cmn    *htinfo
    )
{
    enum ieee80211_phymode phymode;
    u_int16_t    htcapabilities = 0;

    /*
     * Determine BSS phyType
     */

    if (htcap != NULL) {
        htcapabilities = le16toh(htcap->hc_cap);
    }

    if (IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan)) {
        if (htcap && htinfo) {
            if ((htcapabilities & IEEE80211_HTCAP_C_CHWIDTH40) &&
                (htinfo->hi_extchoff == IEEE80211_HTINFO_EXTOFFSET_ABOVE) &&
                IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NA_HT40PLUS)) {
                phymode = IEEE80211_MODE_11NA_HT40PLUS;
            } else if ((htcapabilities & IEEE80211_HTCAP_C_CHWIDTH40) &&
                       (htinfo->hi_extchoff == IEEE80211_HTINFO_EXTOFFSET_BELOW) &&
                       IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NA_HT40MINUS)) {
                phymode = IEEE80211_MODE_11NA_HT40MINUS;
            } else if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NA_HT20)) {
                phymode = IEEE80211_MODE_11NA_HT20;
            } else {
                phymode = IEEE80211_MODE_11A;
            }
        } else {
            phymode = IEEE80211_MODE_11A;
        }
    } else {

        /* Check for HT capability only if we as well support HT mode */
        if (htcap && htinfo && IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NG_HT20)) {
            if ((htcapabilities & IEEE80211_HTCAP_C_CHWIDTH40) &&
                (htinfo->hi_extchoff == IEEE80211_HTINFO_EXTOFFSET_ABOVE) &&
                IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NG_HT40PLUS)) {
                phymode = IEEE80211_MODE_11NG_HT40PLUS;
            } else if ((htcapabilities & IEEE80211_HTCAP_C_CHWIDTH40) &&
                       (htinfo->hi_extchoff == IEEE80211_HTINFO_EXTOFFSET_BELOW) &&
                       IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NG_HT40MINUS)) {
                phymode = IEEE80211_MODE_11NG_HT40MINUS;
            } else {
                phymode = IEEE80211_MODE_11NG_HT20;
            }
        } else if (xrates != NULL) {
            /*
             * XXX: This is probably the most reliable way to tell the difference
             * between 11g and 11b beacons.
             */
            if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11G)) {
                phymode = IEEE80211_MODE_11G;
            } else {
                phymode = IEEE80211_MODE_11B;
            }
        }
        else {
            /* Some mischievous g-only APs do not set extended rates */
            if (rates != NULL) {
                u_int8_t    *tmpPtr  = rates + 2;
                u_int8_t    tmpSize = rates[1];
                u_int8_t    *tmpPtrTail = tmpPtr + tmpSize;
                int         found11g = 0;

                for (; tmpPtr < tmpPtrTail; tmpPtr++) {
                    found11g = ieee80211_find_puregrate(*tmpPtr);
                    if (found11g)
                        break;
                }

                if (found11g) {
                    if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11G)) {
                        phymode = IEEE80211_MODE_11G;
                    } else {
                        phymode = IEEE80211_MODE_11B;
                    }
                } else {
                    phymode = IEEE80211_MODE_11B;
                }
            } else {
                phymode = IEEE80211_MODE_11B;
            }
        }
    }

    return phymode;
}

static int
bss_intol_channel_check(struct ieee80211_node *ni,
                        struct ieee80211_ie_intolerant_report *intol_ie)
{
    struct ieee80211com *ic = ni->ni_ic;
    int i, j;
    u_int8_t intol_chan  = 0;
    u_int8_t *chan_list = &intol_ie->chan_list[0];
    u_int8_t myBand;

    /*
    ** Determine the band the node is operating in currently
    */

    if( ni->ni_chan->ic_ieee > 15 )
        myBand = 5;
    else
        myBand = 2;

    if (intol_ie->elem_len <= 1) 
        return 0;

    /*
    ** Check the report against the channel list.
    */

    for (i = 0; i < intol_ie->elem_len-1; i++) {
        intol_chan = *chan_list++;

        /*
        ** If the intolarant channel is not in my band, ignore
        */

        if( (intol_chan > 15 && myBand == 2) || (intol_chan < 16 && myBand == 5))
            continue;

        /*
        ** Check against the channels supported by the "device"
        ** (note: Should this be limited by "WIRELESS_MODE"
        */

        for (j = 0; j < ic->ic_nchans; j++) {
            if (intol_chan == ic->ic_channels[j].ic_ieee) {
                IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_ACTION, ni,
                               "%s: Found intolerant channel %d",
                               __func__, intol_chan);
                return 1;
            }
        }
    }
    return 0;
}

int
ieee80211_parse_beacon(struct ieee80211vap                  *vap,
                       struct ieee80211_beacon_frame        *beacon_frame,
                       const struct ieee80211_frame         *wh,
                       u_int32_t                            beacon_frame_length,
                       int                                  subtype,
                       struct ieee80211_scanentry_params    *scan_entry_parameters)
{
/*
 * Offset of IE's in beacon frames: header size plus timestamp, beacon_interval and capability.
 */
#define BEACON_INFO_ELEMENT_OFFSET    (offsetof(struct ieee80211_beacon_frame, info_elements))

    struct ieee80211com              *ic = vap->iv_ic;
    struct ieee80211_ie_header       *info_element;
    u_int32_t                        remaining_ie_length, halfquarter;
    struct ieee80211_channel         *curchan = ieee80211_get_current_channel(ic);
    u_int8_t                         bchan;

    /*
     * beacon/probe response frame format
     *  [8] time stamp
     *  [2] beacon interval
     *  [2] capability information
     *  [tlv] ssid
     *  [tlv] supported rates
     *  [tlv] country information
     *  [tlv] parameter set (FH/DS)
     *  [tlv] erp information
     *  [tlv] extended supported rates
     *  [tlv] WME
     *  [tlv] WPA or RSN
     *  [tlv] Atheros Advanced Capabilities
     */
    IEEE80211_VERIFY_LENGTH(beacon_frame_length, BEACON_INFO_ELEMENT_OFFSET);

    info_element = &(beacon_frame->info_elements);
    remaining_ie_length = beacon_frame_length - BEACON_INFO_ELEMENT_OFFSET;

    OS_MEMZERO(scan_entry_parameters, sizeof(*scan_entry_parameters));

    bchan = ieee80211_chan2ieee(ic, curchan);

    scan_entry_parameters->tsf              = (u_int8_t *) &(beacon_frame->timestamp);
    scan_entry_parameters->bintval          = le16toh(beacon_frame->beacon_interval);
    scan_entry_parameters->capinfo          = le16toh(beacon_frame->capability);
    scan_entry_parameters->chan             = curchan;
    scan_entry_parameters->channel_mismatch = false;

    if (!((IEEE80211_BINTVAL_MIN <= scan_entry_parameters->bintval) &&
          (scan_entry_parameters->bintval <= IEEE80211_BINTVAL_MAX))) {
          IEEE80211_DISCARD(vap, IEEE80211_MSG_SCAN,
                            wh, "beacon", "invalid beacon interval (%u)",
                            scan_entry_parameters->bintval);
          return -EINVAL;
    }
    /* 
     * initialize erp with a value as if  the beacon is received from a 11B AP
     * the following code will parse and update the erp with the actual value if 
     * found.
     */
    scan_entry_parameters->erp = IEEE80211_ERP_NON_ERP_PRESENT;

    /* Walk through to check nothing is malformed */
    while (remaining_ie_length >= sizeof(struct ieee80211_ie_header)) {
        /* At least one more header is present */
        remaining_ie_length -= sizeof(struct ieee80211_ie_header);
        
        if (info_element->length == 0) {
            info_element += 1;    /* next IE */
            continue;
        }

        if (remaining_ie_length < info_element->length) {
            /* Incomplete/bad info element */
            return -EINVAL;
        }

        /* New info_element needs also be added in ieee80211_scan_entry_update */
        switch (info_element->element_id) {
        case IEEE80211_ELEMID_SSID:
            scan_entry_parameters->ie_list.ssid = (u_int8_t *) info_element;
            break;
        case IEEE80211_ELEMID_RATES:
            scan_entry_parameters->ie_list.rates = (u_int8_t *) info_element;
            break;
        case IEEE80211_ELEMID_FHPARMS:
            if (ic->ic_phytype == IEEE80211_T_FH) {
                struct ieee80211_fh_ie    *fh_parms = (struct ieee80211_fh_ie *) info_element;
                u_int                     channel;
                u_int                     freq;

                channel = IEEE80211_FH_CHAN(fh_parms->hop_set, fh_parms->hop_pattern);
                freq    = ieee80211_ieee2mhz(channel, IEEE80211_CHAN_FHSS);

                scan_entry_parameters->fhdwell = fh_parms->dwell_time;
                scan_entry_parameters->chan    = ieee80211_find_channel(ic, freq, IEEE80211_CHAN_FHSS);
                scan_entry_parameters->fhindex = fh_parms->hop_index;
            }
            break;
        case IEEE80211_ELEMID_DSPARMS:
            /*
             * XXX hack this since depending on phytype
             * is problematic for multi-mode devices.
             */
            if (ic->ic_phytype != IEEE80211_T_FH) {
                struct ieee80211_ds_ie    *ds_parms = (struct ieee80211_ds_ie *) info_element;

                bchan = ds_parms->current_channel;
            } 
            break;
        case IEEE80211_ELEMID_TIM: {
                struct ieee80211_tim_ie    *tim = (struct ieee80211_tim_ie *) info_element;

                /* XXX ATIM? */
                scan_entry_parameters->ie_list.tim = (u_int8_t *) info_element;
                scan_entry_parameters->timoff      = (tim->tim_bitctl >> 1);
            }
            break;
        case IEEE80211_ELEMID_IBSSPARMS:
            break;
        case IEEE80211_ELEMID_COUNTRY:
            scan_entry_parameters->ie_list.country = (u_int8_t *) info_element;
            break;
        case IEEE80211_ELEMID_QBSS_LOAD:
            scan_entry_parameters->ie_list.qbssload = (u_int8_t *) info_element;
            break;
        case IEEE80211_ELEMID_PWRCNSTR:
            break;
        case IEEE80211_ELEMID_CHANSWITCHANN:
            if (ieee80211_ic_doth_is_set(ic))
                scan_entry_parameters->ie_list.csa = (u_int8_t *) info_element;
            break;
#if ATH_SUPPORT_IBSS_DFS
        case IEEE80211_ELEMID_IBSSDFS:
            scan_entry_parameters->ie_list.ibssdfs = (u_int8_t *) info_element;
            break;
#endif /* ATH_SUPPORT_IBSS_DFS */
        case IEEE80211_ELEMID_QUIET:
            scan_entry_parameters->ie_list.quiet = (u_int8_t *) info_element;
            break;
        case IEEE80211_ELEMID_ERP:
            scan_entry_parameters->erp = ((struct ieee80211_erp_ie *) info_element)->value;
            break;
        case IEEE80211_ELEMID_HTCAP_ANA:
            scan_entry_parameters->ie_list.htcap = (u_int8_t *) &(((struct ieee80211_ie_htcap *) info_element)->hc_ie);
            break;
        case IEEE80211_ELEMID_RESERVED_47:
            break;
        case IEEE80211_ELEMID_RSN:
            scan_entry_parameters->ie_list.rsn = (u_int8_t *) info_element;
            break;
        case IEEE80211_ELEMID_XRATES:
            scan_entry_parameters->ie_list.xrates = (u_int8_t *) info_element;
            break;
        case IEEE80211_ELEMID_HTCAP:
            /* we only care if there isn't already an HT IE (ANA) */
            if (scan_entry_parameters->ie_list.htcap == NULL) {
                scan_entry_parameters->ie_list.htcap = (u_int8_t *) &(((struct ieee80211_ie_htcap *) info_element)->hc_ie);
            }
            break;
        case IEEE80211_ELEMID_HTINFO:
            /* we only care if there isn't already an HT IE (ANA) */
            if (scan_entry_parameters->ie_list.htinfo == NULL) {
                scan_entry_parameters->ie_list.htinfo = (u_int8_t *) &(((struct ieee80211_ie_htinfo *) info_element)->hi_ie);
            }
            break;
        case IEEE80211_ELEMID_EXTCHANSWITCHANN:
            if (ieee80211_ic_doth_is_set(ic))
                scan_entry_parameters->ie_list.xcsa = (u_int8_t *) info_element;
            break;
        case IEEE80211_ELEMID_HTINFO_ANA: {
            scan_entry_parameters->ie_list.htinfo = (u_int8_t *) &(((struct ieee80211_ie_htinfo *) info_element)->hi_ie);
            /*
             * XXX hack this since depending on phytype
             * is problematic for multi-mode devices.
             */
            bchan = ((struct ieee80211_ie_htinfo_cmn *) (scan_entry_parameters->ie_list.htinfo))->hi_ctrlchannel;
            break;
        }
#if ATH_SUPPORT_WAPI
        case IEEE80211_ELEMID_WAPI:
            scan_entry_parameters->ie_list.wapi = (u_int8_t *) info_element;
            break;
#endif
        case IEEE80211_ELEMID_XCAPS:
            break;
        case IEEE80211_ELEMID_RESERVED_133:
            break;
        case IEEE80211_ELEMID_TPC:
            break;
        case IEEE80211_ELEMID_VENDOR:
            if (scan_entry_parameters->ie_list.vendor == NULL) {
                scan_entry_parameters->ie_list.vendor = (u_int8_t *) info_element;
            }

            if (iswpaoui((u_int8_t *) info_element))
                scan_entry_parameters->ie_list.wpa = (u_int8_t *) info_element;
            else if (iswpsoui((u_int8_t *) info_element)) {
                scan_entry_parameters->ie_list.wps = (u_int8_t *) info_element;
                /* WCN IE should be a subset of WPS IE */
                if (iswcnoui((u_int8_t *) info_element)) {
                    scan_entry_parameters->ie_list.wcn = (u_int8_t *) info_element;
                }
            }
            else if (iswmeparam((u_int8_t *) info_element))
                scan_entry_parameters->ie_list.wmeparam = (u_int8_t *) info_element;
            else if (iswmeinfo((u_int8_t *) info_element))
                scan_entry_parameters->ie_list.wmeinfo = (u_int8_t *) info_element;
            else if (isatherosoui((u_int8_t *) info_element))
                scan_entry_parameters->ie_list.athcaps = (u_int8_t *) info_element;
            else if (isatheros_extcap_oui((u_int8_t *) info_element))
                scan_entry_parameters->ie_list.athextcaps = (u_int8_t *) info_element;
            else if (issfaoui((u_int8_t *) info_element))
                scan_entry_parameters->ie_list.sfa = (u_int8_t *) info_element;
            else if (IEEE80211_IS_HTVIE_ENABLED(ic)) {
                /*  we only care if there isn't already an HT IE (ANA) */
                if (ishtcap((u_int8_t *) info_element)) {
                    if (scan_entry_parameters->ie_list.htcap == NULL) {
                        scan_entry_parameters->ie_list.htcap = (u_int8_t *) &(((struct vendor_ie_htcap *) info_element)->hc_ie);
                    }
                }
                else if (ishtinfo((u_int8_t *) info_element)) {
                    /*  we only care if there isn't already an HT IE (ANA) */
                    if (scan_entry_parameters->ie_list.htinfo == NULL) {
                        scan_entry_parameters->ie_list.htinfo = (u_int8_t *) &(((struct vendor_ie_htinfo *) info_element)->hi_ie);
                    }
                }
            }
            break;
        default:
            IEEE80211_DISCARD_IE(vap, IEEE80211_MSG_ELEMID,
                                 "unhandled",
                                 "id %u, len %u",
                                 info_element->element_id,
                                 info_element->length);
            break;
        }

        /* Consume info element */
        remaining_ie_length -= info_element->length;

        /* Go to next IE */
        info_element = (struct ieee80211_ie_header *) 
            (((u_int8_t *) info_element) + sizeof(struct ieee80211_ie_header) + info_element->length); 
    }

    IEEE80211_VERIFY_ELEMENT(scan_entry_parameters->ie_list.rates, IEEE80211_RATE_MAXSIZE);

    if (scan_entry_parameters->ie_list.ssid != NULL) {
        IEEE80211_VERIFY_ELEMENT(scan_entry_parameters->ie_list.ssid, IEEE80211_NWID_LEN);
    }

    if (scan_entry_parameters->ie_list.xrates != NULL)
        IEEE80211_VERIFY_ELEMENT(scan_entry_parameters->ie_list.xrates, IEEE80211_RATE_MAXSIZE);

    scan_entry_parameters->phy_mode = 
        ieee80211_get_phy_type(        
            ic,  
            scan_entry_parameters->ie_list.rates,
            scan_entry_parameters->ie_list.xrates,
            (struct ieee80211_ie_htcap_cmn  *) scan_entry_parameters->ie_list.htcap,
            (struct ieee80211_ie_htinfo_cmn *) scan_entry_parameters->ie_list.htinfo);

    if (bchan != 0) {
        if (bchan != ieee80211_chan2ieee(ic, scan_entry_parameters->chan)) {
	    if (ieee80211_vap_vap_ind_is_set(vap)) { 
                /* ignore leaked beacons when ind vap is set */
                return EINVAL;  
            } 
            /* Incorrect channel, probably because of leakage */
            scan_entry_parameters->channel_mismatch = true;
        }

        /* 
         * Assign the right channel.
         * This is necessary for two reasons:
         *     -update channel flags which may have changed even if the channel
         *      number did not change
         *     -we may have received a beacon from an adjacent channel which is not 
         *      supported by our regulatory domain. In this case we want 
         *      scan_entry_parameters->chan to be set to NULL so that we know it is
         *      an invalid channel.
         */
        halfquarter = scan_entry_parameters->chan->ic_flags &
           (IEEE80211_CHAN_HALF | IEEE80211_CHAN_QUARTER);
        scan_entry_parameters->chan = ieee80211_find_dot11_channel(ic, bchan, scan_entry_parameters->phy_mode | halfquarter);
 
        /*
         * The most common reasons why ieee80211_find_dot11_channel can't find
         * a channel are:
         *     -Channel is not allowed for the current RegDomain
         *     -Channel flags do not match
         *
         * For the second case, *if* there beacon was transmitted in the 
         * current channel (channel_mismatch is false), then it is OK to use
         * the current IC channel as the beacon channel.
         * The underlying assumption is that the Station will *not* scan 
         * channels not allowed by the current regulatory domain/11d settings.
         */
        if ((scan_entry_parameters->chan == NULL) && !scan_entry_parameters->channel_mismatch) {
            scan_entry_parameters->chan = curchan;
        }
   }

    return EOK;
}

ieee80211_scan_entry_t
ieee80211_update_beacon(struct ieee80211_node      *ni,
                        wbuf_t                     wbuf,
                        struct ieee80211_frame     *wh,
                        int                        subtype,
                        struct ieee80211_rx_status *rs)
{
    struct ieee80211com       *ic = ni->ni_ic;
    ieee80211_scan_entry_t    scan_entry;

    scan_entry = ieee80211_scan_table_update(ni->ni_vap,
                                             wh,
                                             wbuf_get_pktlen(wbuf),
                                             subtype,
                                             rs->rs_rssi);

    if (scan_entry != NULL) {
        u_int8_t    *bssid = ieee80211_scan_entry_bssid(scan_entry);
        bool        bssid_match = false;

        if (bssid != NULL) {
            if (IEEE80211_ADDR_EQ(ni->ni_bssid, bssid)) {
                bssid_match = true;
            }
        }

        ieee80211_scan_scanentry_received(ic->ic_scanner, bssid_match);
    }

    return scan_entry;
}

static INLINE int
ieee80211_recv_beacon(struct ieee80211_node *ni, wbuf_t wbuf, int subtype, struct ieee80211_rx_status *rs)
{
    struct ieee80211vap                          *vap = ni->ni_vap;
    struct ieee80211_frame                       *wh;
    ieee80211_scan_entry_t                       scan_entry;

    wh = (struct ieee80211_frame *)wbuf_header(wbuf);

    scan_entry = ieee80211_update_beacon(ni, wbuf, wh, subtype, rs);
    /*
     * The following code MUST be SSID independant.
     * 
     * RefCount of scan_entry = 1.
     */
    if (scan_entry != NULL) {

        switch (vap->iv_opmode) {
        case IEEE80211_M_STA:
            ieee80211_recv_beacon_sta(ni,wbuf,subtype,rs,scan_entry);
            break;

        case IEEE80211_M_IBSS:
            ieee80211_recv_beacon_ibss(ni,wbuf,subtype,rs,scan_entry);
            break;
            
        case IEEE80211_M_HOSTAP:
            ieee80211_recv_beacon_ap(ni,wbuf,subtype,rs,scan_entry);
            break;
            
        case IEEE80211_M_BTAMP:
            ieee80211_recv_beacon_btamp(ni,wbuf,subtype,rs,scan_entry);
            break;

        default:
            break;
        }
    } /* scan_entry != NULL */

    return EOK;
}


static INLINE int
ieee80211_recv_auth(struct ieee80211_node *ni, wbuf_t wbuf, int subtype,
                    struct ieee80211_rx_status *rs)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_frame *wh;
    u_int8_t *frm, *efrm;
    u_int16_t algo, seq, status;
    u_int8_t *challenge = NULL, challenge_len = 0;
    int deref_reqd = 0;
    int ret_val = EOK;

    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    /*
     * can only happen for HOST AP mode .
     */
    if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
        struct ieee80211_key *key;

        key = ieee80211_crypto_decap(ni, wbuf, ieee80211_hdrspace(ni->ni_ic,wh), rs);
        if (key) {
            wh = (struct ieee80211_frame *) wbuf_header(wbuf);
            wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
        }
        /*
         * if crypto decap fails (key == null) let the 
         * ieee80211_mlme_recv_auth will discard the request(algo and/or
         * auth seq # is going to be bogus) and also handle any stattion
         * ref count. 
         */
    }

    frm = (u_int8_t *)&wh[1];
    efrm = wbuf_header(wbuf) + wbuf_get_pktlen(wbuf);

    /* 
     * XXX bug fix 89056: Station Entry exists in the node table, 
     * But the node is associated with the other vap, so we are 
     * deleting that node and creating the new node 
     *
     */
    if (!IEEE80211_ADDR_EQ(wh->i_addr1, vap->iv_myaddr)) {
  
        struct ieee80211vap *tmpvap;
        struct ieee80211com *ic = ni->ni_ic;

        TAILQ_FOREACH(tmpvap, &(ic)->ic_vaps, iv_next) {

           if(IEEE80211_ADDR_EQ(wh->i_addr1, tmpvap->iv_myaddr) &&
              (tmpvap->iv_opmode == IEEE80211_M_HOSTAP)) {

               if (ni != vap->iv_bss) {
                  IEEE80211_NOTE(vap, IEEE80211_MSG_MLME, ni,
                            "%s", "Removing the node from the station node list\n");
                  ieee80211_ref_node(ni);
                  if(ieee80211_node_leave(ni)) {
                     /* Call MLME indication handler if node is in associated state */
                     IEEE80211_DELIVER_EVENT_MLME_DISASSOC_INDICATION(vap,
                                                                          ni->ni_macaddr,
                                                                          IEEE80211_REASON_ASSOC_LEAVE);
                  }
                  ieee80211_free_node(ni);
                  /* Referencing the BSS node */

                  ni = ieee80211_ref_bss_node(tmpvap); 

                  /* Note that ieee80211_ref_bss_node must have a */
                  /* corresponding ieee80211_free_bss_node        */

                  deref_reqd = 1;
                  vap = ni->ni_vap;
              }
              break;
           }
        }
    }
    /*
     * XXX: when we're scanning, we may receive auth frames
     * of other stations in the same BSS.
     */
    IEEE80211_VERIFY_ADDR(ni);

    /*
     * auth frame format
     *  [2] algorithm
     *  [2] sequence
     *  [2] status
     *  [tlv*] challenge
     */
    IEEE80211_VERIFY_LENGTH(efrm - frm, 6);
    algo = le16toh(*(u_int16_t *)frm); frm += 2;
    seq = le16toh(*(u_int16_t *)frm); frm += 2;
    status = le16toh(*(u_int16_t *)frm); frm += 2;

    /* Validate challenge TLV if any */
    if (algo == IEEE80211_AUTH_ALG_SHARED) {  
        if (frm + 1 < efrm) {
            if ((frm[1] + 2) > (efrm - frm)) {
                IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_AUTH,
                                      ni->ni_macaddr, "shared key auth",
                                      "ie %d/%d too long",
                                      frm[0], (frm[1] + 2) - (efrm - frm));
                vap->iv_stats.is_rx_bad_auth++;
                ret_val = -EINVAL;
                goto exit;
            }
            if (frm[0] == IEEE80211_ELEMID_CHALLENGE) {
                challenge = frm + 2;
                challenge_len = frm[1];
            }
        }

        if (seq == IEEE80211_AUTH_SHARED_CHALLENGE ||
            seq == IEEE80211_AUTH_SHARED_RESPONSE) {
            if ((challenge == NULL || challenge_len == 0) && (status == 0)) {
                IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_AUTH,
                                      ni->ni_macaddr, "shared key auth",
                                      "%s", "no challenge");
                vap->iv_stats.is_rx_bad_auth++;
                ret_val = -EINVAL;
                goto exit;
            }
        }
    }

    ieee80211_mlme_recv_auth(ni, algo, seq, status, challenge, challenge_len,wbuf);
exit:
    /* Note that ieee80211_ref_bss_node must have a */
    /* corresponding ieee80211_free_bss_node        */

    if (deref_reqd) 
        ieee80211_free_node(ni);
    return ret_val;
}

static INLINE int
ieee80211_recv_deauth(struct ieee80211_node *ni, wbuf_t wbuf, int subtype)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_frame *wh;
    u_int8_t *frm, *efrm;
    u_int16_t reason;

    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    frm = (u_int8_t *)&wh[1];
    efrm = wbuf_header(wbuf) + wbuf_get_pktlen(wbuf);

    IEEE80211_VERIFY_ADDR(ni);
    
    /*
     * deauth frame format
     *  [2] reason
     */
    IEEE80211_VERIFY_LENGTH(efrm - frm, 2);
    reason = le16toh(*(u_int16_t *)frm);

    if ((vap->iv_opmode == IEEE80211_M_STA) && 
        (vap->iv_ccx_evtable && vap->iv_ccx_evtable->wlan_ccx_is_mfp &&
            vap->iv_ccx_evtable->wlan_ccx_is_mfp(vap->iv_ifp)) && 
        !(wh->i_fc[1] & IEEE80211_FC1_WEP)) {
        /*
         * Check if MFP is enabled for connection.
         */
        IEEE80211_DISCARD(vap, IEEE80211_MSG_INPUT,
                          wh, ieee80211_mgt_subtype_name[
                              subtype >> IEEE80211_FC0_SUBTYPE_SHIFT],
                          "%s", "deauth frame is not encrypted");
        return -EINVAL;
    }

    ieee80211_mlme_recv_deauth(ni, reason);

    return EOK;
}

static INLINE int
ieee80211_recv_disassoc(struct ieee80211_node *ni, wbuf_t wbuf, int subtype)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_frame *wh;
    u_int8_t *frm, *efrm;
    u_int16_t reason;

    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    frm = (u_int8_t *)&wh[1];
    efrm = wbuf_header(wbuf) + wbuf_get_pktlen(wbuf);

    IEEE80211_VERIFY_ADDR(ni);

    /*
     * disassoc frame format
     *  [2] reason
     */
    IEEE80211_VERIFY_LENGTH(efrm - frm, 2);
    reason = le16toh(*(u_int16_t *)frm);

    if ((vap->iv_opmode == IEEE80211_M_STA) && 
        (vap->iv_ccx_evtable && vap->iv_ccx_evtable->wlan_ccx_is_mfp &&
            vap->iv_ccx_evtable->wlan_ccx_is_mfp(vap->iv_ifp)) && 
        !(wh->i_fc[1] & IEEE80211_FC1_WEP)) {
        /*
         * Check if MFP is enabled for connection.
         */
        IEEE80211_DISCARD(vap, IEEE80211_MSG_INPUT,
                          wh, ieee80211_mgt_subtype_name[
                              subtype >> IEEE80211_FC0_SUBTYPE_SHIFT],
                          "%s", "disassoc frame is not encrypted");
        return -EINVAL;
    }

    ieee80211_mlme_recv_disassoc(ni, reason);

    return EOK;
}

static INLINE int
ieee80211_recv_action(struct ieee80211_node *ni, wbuf_t wbuf, int subtype, struct ieee80211_rx_status *rs)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211_frame *wh;
    u_int8_t *frm, *efrm;
    struct ieee80211_action *ia;
    bool   fgActionForMe = FALSE, fgBCast = FALSE;

    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    frm = (u_int8_t *)&wh[1];
    efrm = wbuf_header(wbuf) + wbuf_get_pktlen(wbuf);

    if (!(IEEE80211_ADDR_EQ(wh->i_addr3, ni->ni_bssid)) &&
        !((ni->ni_flags & IEEE80211_NODE_NAWDS))) {
        IEEE80211_DISCARD(vap, IEEE80211_MSG_INPUT,
                          wh, ieee80211_mgt_subtype_name[
                              IEEE80211_FC0_SUBTYPE_ACTION >> IEEE80211_FC0_SUBTYPE_SHIFT],
                          "%s", "action frame not in same BSS");
        return -EINVAL;
    }

    if (!ieee80211_vap_ready_is_set(vap)) {
        vap->iv_stats.is_rx_mgtdiscard++;
        return -EINVAL;
    }

    if (IEEE80211_ADDR_EQ(wh->i_addr1, vap->iv_myaddr)) {
        fgActionForMe = TRUE;
    }
    else if(IEEE80211_ADDR_EQ(wh->i_addr1, IEEE80211_GET_BCAST_ADDR(ic)))
    {
        fgBCast = TRUE;
    }

    IEEE80211_VERIFY_LENGTH(efrm - frm, sizeof(struct ieee80211_action));
    ia = (struct ieee80211_action *) frm;

    vap->iv_stats.is_rx_action++;
    IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                   "%s: action mgt frame (cat %d, act %d)",__func__, ia->ia_category, ia->ia_action);
    if (ia->ia_category == IEEE80211_ACTION_CAT_SPECTRUM && 
         (ia->ia_action == IEEE80211_ACTION_CHAN_SWITCH || 
          ia->ia_action == IEEE80211_ACTION_MEAS_REPORT)
       ) {
        if (!(fgBCast || fgActionForMe)) {
            /*CSA action frame could be in broadcast.
               return if unicast not for me && not boradcast Action frame.*/
            return -EINVAL;
        }
    }
    else{

        if (!fgActionForMe) {
            return -EINVAL;
        }
    }

    switch (ia->ia_category) {
    case IEEE80211_ACTION_CAT_SPECTRUM:
        switch (ia->ia_action) {
            case IEEE80211_ACTION_CHAN_SWITCH:
                if (ieee80211_process_csa_ecsa_ie(ni,  ia) != EOK) {
                    /*
                     * If failed to switch the channel, mark the AP as radar detected and disconnect from the AP.
                     */
                    ieee80211_mlme_recv_csa(ni, IEEE80211_RADAR_DETECT_DEFAULT_DELAY,true);
                }
                break;
#if ATH_SUPPORT_IBSS_DFS
            case IEEE80211_ACTION_MEAS_REPORT:
                ieee80211_process_meas_report_ie(ni, ia);
                break;
#endif /* ATH_SUPPORT_IBSS_DFS */

            default:
                break;
        }
 	break;

    case IEEE80211_ACTION_CAT_QOS:
        IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                       "%s: QoS action mgt frames not supported", __func__);
        vap->iv_stats.is_rx_mgtdiscard++;
        break;

    case IEEE80211_ACTION_CAT_WMM_QOS:
        /* WiFi WMM QoS TSPEC action management frames */
        IEEE80211_VERIFY_LENGTH(efrm - frm, sizeof(struct ieee80211_action_wmm_qos));
        switch (ia->ia_action) {
        case IEEE80211_WMM_QOS_ACTION_SETUP_REQ:
            /* ADDTS received by AP */
            /* Not implemented */
            break;

        case IEEE80211_WMM_QOS_ACTION_SETUP_RESP: {
            /* ADDTS response from AP */
            struct ieee80211_wme_tspec *tspecie;
            if ((vap->iv_opmode == IEEE80211_M_STA) && 
                (vap->iv_ccx_evtable && vap->iv_ccx_evtable->wlan_ccx_is_mfp &&
                    vap->iv_ccx_evtable->wlan_ccx_is_mfp(vap->iv_ifp)) && 
                !(wh->i_fc[1] & IEEE80211_FC1_WEP)) {
                /*
                 * Check if MFP is enabled for connection.
                 */
                IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                               "%s: QoS action mgt frame is not encrypted", __func__);
                vap->iv_stats.is_rx_mgtdiscard++;
                break;
            }

            if (((struct ieee80211_action_wmm_qos *)ia)->ts_statuscode != 0) {
                /* tspec was not accepted */
                /* Indicate to CCX and break. */
                /* AP will send us a disassoc anyway if we try assoc */
                wlan_set_tspecActive(vap, 0);
                if (vap->iv_ccx_evtable && vap->iv_ccx_evtable->wlan_ccx_set_vperf) {
                    vap->iv_ccx_evtable->wlan_ccx_set_vperf(vap->iv_ccx_arg, 0);
                }
                /* Trigger Roam for unspecified QOS-related reason. */
                //Sta11DeauthIndication(vap->iv_mlme_arg, ni->ni_macaddr, IEEE80211_REASON_QOS);
                //StaCcxTriggerRoam(vap->iv_mlme_arg, IEEE80211_REASON_QOS);
                if (vap->iv_ccx_evtable && vap->iv_ccx_evtable->wlan_ccx_trigger_roam) {
                    vap->iv_ccx_evtable->wlan_ccx_trigger_roam(vap->iv_ccx_arg, IEEE80211_REASON_QOS);
                }
                break;
            }
            tspecie = &((struct ieee80211_action_wmm_qos *)ia)->ts_tspecie;
            ieee80211_parse_tspecparams(vap, (u_int8_t *) tspecie);
            wlan_set_tspecActive(vap, 1);
            if (vap->iv_ccx_evtable && vap->iv_ccx_evtable->wlan_ccx_set_vperf) {
                vap->iv_ccx_evtable->wlan_ccx_set_vperf(vap->iv_ccx_arg, 1);
            }
            break;
        }

        case IEEE80211_WMM_QOS_ACTION_TEARDOWN:
            break;

        default:
            IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                           "%s: invalid WME action mgt frame", __func__);
            vap->iv_stats.is_rx_mgtdiscard++;
        }
        break;

    case IEEE80211_ACTION_CAT_BA: {
        struct ieee80211_action_ba_addbarequest *addbarequest;
        struct ieee80211_action_ba_addbaresponse *addbaresponse;
        struct ieee80211_action_ba_delba *delba;
        struct ieee80211_ba_seqctrl basequencectrl;
        struct ieee80211_ba_parameterset baparamset;
        struct ieee80211_delba_parameterset delbaparamset;
        struct ieee80211_action_mgt_args actionargs;
        u_int16_t statuscode;
        u_int16_t batimeout;
        u_int16_t reasoncode;
       
        switch (ia->ia_action) {
        case IEEE80211_ACTION_BA_ADDBA_REQUEST:            
            IEEE80211_VERIFY_LENGTH(efrm - frm, sizeof(struct ieee80211_action_ba_addbarequest));
            addbarequest = (struct ieee80211_action_ba_addbarequest *) frm;

            /* "struct ieee80211_action_ba_addbarequest" is annotated __packed, 
               if accessing fields, like rq_baparamset or rq_basequencectrl, 
               by using u_int16_t* directly, it will cause byte alignment issue.
               Some platform that cannot handle this issue will cause exception.
               Use OS_MEMCPY to move data byte by byte */
            OS_MEMCPY(&baparamset, &addbarequest->rq_baparamset, sizeof(baparamset));
            *(u_int16_t *)&baparamset = le16toh(*(u_int16_t*)&baparamset); 
            batimeout = le16toh(addbarequest->rq_batimeout);
            OS_MEMCPY(&basequencectrl, &addbarequest->rq_basequencectrl, sizeof(basequencectrl));
            *(u_int16_t *)&basequencectrl = le16toh(*(u_int16_t*)&basequencectrl);

            IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                           "%s: ADDBA request action mgt frame. TID %d, buffer size %d",
                           __func__, baparamset.tid, baparamset.buffersize);

            /*
             * NB: The user defined ADDBA response status code is overloaded for
             * non HT capable node and WDS node
             */
            if (!IEEE80211_NODE_USEAMPDU(ni)) {
                /* The node is not HT capable - set the ADDBA status to refused */
                ic->ic_addba_setresponse(ni, baparamset.tid, IEEE80211_STATUS_REFUSED);
            } else if (IEEE80211_VAP_IS_WDS_ENABLED(ni->ni_vap)) {
                /* The node is WDS and HT capable */
                if (ieee80211_node_wdswar_isaggrdeny(ni)) {
                    /*Aggr is denied for WDS - set the addba response to REFUSED*/
                    ic->ic_addba_setresponse(ni, baparamset.tid, IEEE80211_STATUS_REFUSED);
                } else {
                    /*Aggr is allowed for WDS - set the addba response to SUCCESS*/
                    ic->ic_addba_setresponse(ni, baparamset.tid, IEEE80211_STATUS_SUCCESS);
                }
            }

            /* Process ADDBA request and save response in per TID data structure */
            if (ic->ic_addba_requestprocess(ni, addbarequest->rq_dialogtoken,           
                                         &baparamset, batimeout, basequencectrl) == 0) {
                /* Send ADDBA response */
                actionargs.category     = IEEE80211_ACTION_CAT_BA;
                actionargs.action       = IEEE80211_ACTION_BA_ADDBA_RESPONSE;
                actionargs.arg1         = baparamset.tid;
                actionargs.arg2         = 0;
                actionargs.arg3         = 0;

                ieee80211_send_action(ni, &actionargs, NULL);
            }
            break;

        case IEEE80211_ACTION_BA_ADDBA_RESPONSE:
            if (!IEEE80211_NODE_USE_HT(ni)) {
                IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                               "%s: ADDBA response frame ignored for non-HT association)", __func__);
                break;
            }
            IEEE80211_VERIFY_LENGTH(efrm - frm, sizeof(struct ieee80211_action_ba_addbaresponse));
            addbaresponse = (struct ieee80211_action_ba_addbaresponse *) frm;

            statuscode = le16toh(addbaresponse->rs_statuscode);
            /* "struct ieee80211_action_ba_addbaresponse" is annotated __packed, 
               if accessing fields, like rs_baparamset, by using u_int16_t* directly, 
               it will cause byte alignment issue.
               Some platform that cannot handle this issue will cause exception.
               Use OS_MEMCPY to move data byte by byte */
            OS_MEMCPY(&baparamset, &addbaresponse->rs_baparamset, sizeof(baparamset));
            *(u_int16_t *)&baparamset = le16toh(*(u_int16_t*)&baparamset);
            batimeout = le16toh(addbaresponse->rs_batimeout);

            ic->ic_addba_responseprocess(ni, statuscode, &baparamset, batimeout);

            IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                           "%s: ADDBA response action mgt frame. TID %d, buffer size %d",
                           __func__, baparamset.tid, baparamset.buffersize);
            break;

        case IEEE80211_ACTION_BA_DELBA:
            if (!IEEE80211_NODE_USE_HT(ni)) {
                IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                               "%s: DELBA frame ignored for non-HT association)", __func__);
                break;
            }
            IEEE80211_VERIFY_LENGTH(efrm - frm, sizeof(struct ieee80211_action_ba_delba));

            delba = (struct ieee80211_action_ba_delba *) frm;
            /* "struct ieee80211_action_ba_delba" is annotated __packed, 
               if accessing fields, like dl_delbaparamset, by using u_int16_t* directly, 
               it will cause byte alignment issue.
               Some platform that cannot handle this issue will cause exception.
               Use OS_MEMCPY to move data byte by byte */
            OS_MEMCPY(&delbaparamset, &delba->dl_delbaparamset, sizeof(delbaparamset));
            *(u_int16_t *)&delbaparamset= le16toh(*(u_int16_t*)&delbaparamset);
            reasoncode = le16toh(delba->dl_reasoncode);

            ic->ic_delba_process(ni, &delbaparamset, reasoncode);

            IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                           "%s: DELBA action mgt frame. TID %d, initiator %d, reason code %d",
                           __func__, delbaparamset.tid, delbaparamset.initiator, reasoncode);
            break;

        default:
            IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                           "%s: invalid BA action mgt frame", __func__);
            vap->iv_stats.is_rx_mgtdiscard++;
            break;
        }
        break;
    }

    case IEEE80211_ACTION_CAT_COEX: {
         IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                        "%s: Received Coex Category, action %d\n",
                       __func__, ia->ia_action);
         /* Only process coex action frame if this VAP is in AP and the associated node is in HT mode */
         if (vap->iv_opmode == IEEE80211_M_HOSTAP && (ni->ni_flags & IEEE80211_NODE_HT)) {
            struct ieee80211_ie_bss_coex* coex_ie = NULL;
            struct ieee80211_ie_intolerant_report* intol_chan_report = NULL;
            int intol_check = 0;
             switch(ia->ia_action)
             {
                 case 0:
                     frm = frm + sizeof(struct ieee80211_action);
                     while (((frm+1) < efrm) && (frm + frm[1] +2 <= efrm)) {
                         switch (*frm) {
                         case IEEE80211_ELEMID_2040_COEXT:
                             IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni, "%s Processing IEEE80211_ELEMID_2040_COEXT: \n", __func__);
                             coex_ie = (struct ieee80211_ie_bss_coex*)frm;
                             break;
                         case IEEE80211_ELEMID_2040_INTOL:
                             IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni, "%s Processing IEEE80211_ELEMID_2040_INTOL:: \n", __func__);
                             intol_chan_report = (struct ieee80211_ie_intolerant_report *)frm;
                             intol_check += bss_intol_channel_check(ni, intol_chan_report);
                             break;
                         default:
                             break;
                         }
                         frm += frm[1] + 2;
                     }
                     if (coex_ie != NULL) {
                         IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                               "%s: Element 0\n"
                               "inf request = %d\t"
                               "40 intolerant = %d\t"
                               "20 width req = %d\t"
                               "obss exempt req = %d\t"
                               "obss exempt grant = %d\n", __func__,
                               coex_ie->inf_request,
                               coex_ie->ht40_intolerant,
                               coex_ie->ht20_width_req,
                               coex_ie->obss_exempt_req,
                               coex_ie->obss_exempt_grant);

                         if (intol_check ||
                             coex_ie->ht40_intolerant ||
                             coex_ie->ht20_width_req) {
                             ni->ni_flags |= IEEE80211_NODE_REQ_HT20;
                         } else {
                             ni->ni_flags &= ~IEEE80211_NODE_REQ_HT20;
                         }

                         ieee80211_change_cw(ic);
                     }
                     break;
                default:
                    IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                                   "%s: invalid Coex action mgt frame", __func__);
                                   vap->iv_stats.is_rx_mgtdiscard++;
            }
        }
        break;
    }

    case IEEE80211_ACTION_CAT_HT: {
        struct ieee80211_action_ht_txchwidth *iachwidth;
        enum ieee80211_cwm_width  chwidth;
        struct ieee80211_action_ht_smpowersave *iasmpowersave;
#ifdef ATH_SUPPORT_TxBF
#ifdef TXBF_TODO
        struct ieee80211_action_ht_CSI *CSIframe;
        u_int16_t CSI_Report_Len = 0;
#endif
#endif  // for TxBF RC

        if (!IEEE80211_NODE_USEAMPDU(ni)) {
            IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                           "%s: HT action mgt frame ignored for non-HT association)", __func__);
            break;
        }
        switch (ia->ia_action) {
        case IEEE80211_ACTION_HT_TXCHWIDTH:
            IEEE80211_VERIFY_LENGTH(efrm - frm, sizeof(struct ieee80211_action_ht_txchwidth));
    
            iachwidth = (struct ieee80211_action_ht_txchwidth *) frm;
            chwidth = (iachwidth->at_chwidth == IEEE80211_A_HT_TXCHWIDTH_2040) ?
                IEEE80211_CWM_WIDTH40 : IEEE80211_CWM_WIDTH20;
    
            /* Check for channel width change */
            if (chwidth != ni->ni_chwidth) {
                u_int32_t  rxlinkspeed, txlinkspeed; /* bits/sec */
                
                 /* update node's recommended tx channel width */
                ni->ni_chwidth = chwidth;
                ic->ic_chwidth_change(ni);

                mlme_get_linkrate(ni, &rxlinkspeed, &txlinkspeed);
                IEEE80211_DELIVER_EVENT_LINK_SPEED(vap, rxlinkspeed, txlinkspeed);
            }
    
            IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                           "%s: HT txchwidth action mgt frame. Width %d (%s)",
                           __func__, chwidth, ni->ni_newchwidth? "new" : "no change");
            break;
            
        case IEEE80211_ACTION_HT_SMPOWERSAVE:
            if (vap->iv_opmode != IEEE80211_M_HOSTAP) {
                IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                               "%s: HT SM pwrsave request ignored for non-AP)", __func__);
                break;
            }           
            IEEE80211_VERIFY_LENGTH(efrm - frm, sizeof(struct ieee80211_action_ht_smpowersave));

            iasmpowersave = (struct ieee80211_action_ht_smpowersave *) frm;

            if (iasmpowersave->as_control & IEEE80211_A_HT_SMPOWERSAVE_ENABLED) {
                if (iasmpowersave->as_control & IEEE80211_A_HT_SMPOWERSAVE_MODE) {
                    if ((ni->ni_htcap & IEEE80211_HTCAP_C_SM_MASK) !=
                        IEEE80211_HTCAP_C_SMPOWERSAVE_DYNAMIC) {
                        /* 
                         * Station just enabled dynamic SM power save therefore
                         * we should precede each packet we send to it with an RTS.
                         */
                        ni->ni_htcap &= (~IEEE80211_HTCAP_C_SM_MASK);
                        ni->ni_htcap |= IEEE80211_HTCAP_C_SMPOWERSAVE_DYNAMIC;
                        ni->ni_updaterates = IEEE80211_NODE_SM_PWRSAV_DYN;
                    }
                } else {
                    if ((ni->ni_htcap & IEEE80211_HTCAP_C_SM_MASK) !=
                        IEEE80211_HTCAP_C_SMPOWERSAVE_STATIC) {
                        /* 
                         * Station just enabled static SM power save therefore
                         * we can only send to it at single-stream rates.
                         */
                        ni->ni_htcap &= (~IEEE80211_HTCAP_C_SM_MASK);
                        ni->ni_htcap |= IEEE80211_HTCAP_C_SMPOWERSAVE_STATIC;
                        ni->ni_updaterates = IEEE80211_NODE_SM_PWRSAV_STAT;
                    }
                }
            } else {
                if ((ni->ni_htcap & IEEE80211_HTCAP_C_SM_MASK) !=
                    IEEE80211_HTCAP_C_SM_ENABLED) {
                    /* 
                     * Station just disabled SM Power Save therefore we can
                     * send to it at full SM/MIMO. 
                     */
                    ni->ni_htcap &= (~IEEE80211_HTCAP_C_SM_MASK);
                    ni->ni_htcap |= IEEE80211_HTCAP_C_SM_ENABLED;
                    ni->ni_updaterates = IEEE80211_NODE_SM_EN;
                }
            }
            if (ni->ni_updaterates) {
                ni->ni_updaterates |= IEEE80211_NODE_RATECHG;
            }
            /* Update MIMO powersave flags and node rates */
            ieee80211_update_noderates(ni);

#ifdef ATH_SUPPORT_HTC
            /* Update target node's HT flags. */
            ieee80211_update_node_target(ni, ni->ni_vap); 
#endif            
            break;
#ifdef ATH_SUPPORT_TxBF
#ifdef TXBF_TODO
		case IEEE80211_ACTION_HT_CSI:

            /* CSI not for RC calibration */
            if (ic->ic_rc_wait_csi == 0) {        
                break;
            }
            /* CSI Report + sizeof(struct ieee80211_action_ht_CSI) */    
            CSI_Report_Len = efrm - frm;
            CSI_Report_Len = CSI_Report_Len - sizeof(struct ieee80211_action_ht_CSI);
            CSIframe = (struct ieee80211_action_ht_CSI *) frm;	
			
            /* Use CSI Report to "update radio calibration" */
            /* move to data point of CSI_Report */
            frm += sizeof(struct ieee80211_action_ht_CSI);
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                "**%s CSI_Report_Len = %d **\n", 
                __FUNCTION__, CSI_Report_Len);
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,"MIMO_Control 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x \n",
                CSIframe->mimo_control[0], CSIframe->mimo_control[1], CSIframe->mimo_control[2],
                CSIframe->mimo_control[3], CSIframe->mimo_control[4], CSIframe->mimo_control[5]);

            if (ni->Calibration_Done) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, 
                    "%s:Already Done Just the retry pkt of CSI report ***\n", __func__);
            } else {
                u_int16_t Len;
                u_int8_t* p_Data=NULL;
                void *rx_status;

                ic->ic_get_pos2_data(ic, &p_Data, &Len,&rx_status);
                if (p_Data && rx_status && (Len > 0)) {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                        "to get data of pos2 =====Len(%d)\n",Len);
                    if(ic->ic_txbfrcupdate(ic,rx_status,p_Data,CSIframe->mimo_control,2,2,0)) {
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                        "%s:Call & ar9300TxBFRCUpdate success========\n", __func__);
                        ni->Calibration_Done = 1;
                        if ((ni->ni_chwidth == IEEE80211_CWM_WIDTH40) && 
                            (ic->ic_cwm_get_width(ic) == IEEE80211_CWM_WIDTH40)) {
                            /* 40M bandwidth */
                            ic->ic_rc_40M_valid = 1;
                        } else {
                            ic->ic_rc_20M_valid = 1; 
                        }
                        ic->ic_rc_calibrating = 0;
                    } else {
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                        "%s:Fail for ar9300TxBFRCUpdate=====\n", __func__);
                    }
                } else {
				    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,"%s:Can't get data of pos2 \n",__func__);
				}
                if (p_Data != NULL) {
                    //After handle the data of pos2, free it.
                    //IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,"==>%s:prepare to free local h buffer!!\n",__func__);
                    OS_FREE(p_Data);
                    //IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,"==>%s:local h buffer free!!\n",__func__);
                }    		   
            }
        ic->ic_rc_wait_csi = 0;

        break;
#endif
    case IEEE80211_ACTION_HT_NONCOMP_BF:
    case IEEE80211_ACTION_HT_COMP_BF:
#ifdef TXBF_DEBUG
        ic->ic_txbf_check_cvcache(ic, ni);
#endif 
        /* report received , cancel timer */ 
        OS_CANCEL_TIMER(&(ni->ni_report_timer));
        ni->ni_cvtstamp = rs->rs_rpttstamp;

        ic->ic_txbf_set_rpt_received(ic, ni);
        ic->ic_txbf_stats_rpt_inc(ic, ni);

        /* skip action header and mimo control field*/			
        frm += sizeof(struct ieee80211_action_ht_txbf_rpt);
        
        /* EV 78384 CV/V report generated by osprey will be zero at some case,
        when it happens , it should get another c/cv report immediately to overwrite 
        wrong one*/    
        if ((frm[0] == 0) && (frm[1] ==0) && (frm[2]==0)){
            ni->ni_bf_update_cv = 1;    // request to update CV cache
        }        
        break;
#endif
        default:
            IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                           "%s: invalid HT action mgt frame", __func__);
            vap->iv_stats.is_rx_mgtdiscard++;
            break;
        }
        break;
    }
    default:
        IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                       "%s: action mgt frame has invalid category %d", __func__, ia->ia_category);
        vap->iv_stats.is_rx_mgtdiscard++;
        break;
    }

    return EOK;
}

int
ieee80211_recv_mgmt(struct ieee80211_node *ni,
                    wbuf_t wbuf,
                    int subtype,
                    struct ieee80211_rx_status *rs)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211_mac_stats *mac_stats;
    struct ieee80211_frame *wh;
    int    is_bcast;

    wh = (struct ieee80211_frame *) wbuf_header(wbuf);

    if(subtype ==  IEEE80211_FC0_SUBTYPE_AUTH ||
       subtype ==  IEEE80211_FC0_SUBTYPE_ASSOC_REQ||
       subtype ==  IEEE80211_FC0_SUBTYPE_REASSOC_REQ) {
	if (ni != ni->ni_vap->iv_bss) {
	    wds_clear_wds_table(ni,&ic->ic_sta, wbuf);
	} 
    }

    if (IEEE80211_IS_MFP_FRAME(wh)) {
       is_bcast = IEEE80211_IS_BROADCAST(wh->i_addr1);
       mac_stats = is_bcast ? &vap->iv_multicast_stats : &vap->iv_unicast_stats;
       IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "Received MFP frame with Subtype 0x%2X\n", subtype);
       if (NULL == ieee80211_crypto_decap(ni, wbuf, 
                    ieee80211_hdrspace(ic, wbuf_header(wbuf)), rs)) {
            mac_stats->ims_rx_decryptcrc++;
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "Decrypt Error MFP frame with Subtype 0x%2X\n", subtype);
            return -1;
        } else {
            mac_stats->ims_rx_decryptok++;
        }
        /* recalculate wh pointer, header may shift after decap */
        wh = (struct ieee80211_frame *) wbuf_header(wbuf);
        /* NB: We clear the Protected bit later */
    }

    switch (subtype) {
    case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
    case IEEE80211_FC0_SUBTYPE_BEACON:
        ieee80211_recv_beacon(ni, wbuf, subtype, rs);
        break;

    case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
        ieee80211_recv_probereq(ni, wbuf, subtype);
        break;

    case IEEE80211_FC0_SUBTYPE_AUTH:
        ieee80211_recv_auth(ni, wbuf, subtype, rs );
        break;

    case IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
    case IEEE80211_FC0_SUBTYPE_REASSOC_RESP:
        ieee80211_recv_asresp(ni, wbuf, subtype);
        break;

    case IEEE80211_FC0_SUBTYPE_ASSOC_REQ:
    case IEEE80211_FC0_SUBTYPE_REASSOC_REQ:
        ieee80211_recv_asreq(ni, wbuf, subtype);
        break;
        
    case IEEE80211_FC0_SUBTYPE_DEAUTH:
        ieee80211_recv_deauth(ni, wbuf, subtype);
        break;

    case IEEE80211_FC0_SUBTYPE_DISASSOC:
        ieee80211_recv_disassoc(ni, wbuf, subtype);
        break;
        
    case IEEE80211_FC0_SUBTYPE_ACTION:
    case IEEE80211_FCO_SUBTYPE_ACTION_NO_ACK:
        ieee80211_recv_action(ni, wbuf, subtype, rs);
        break;

    default:
        break;
    }

    return EOK;
}

int
ieee80211_recv_ctrl(struct ieee80211_node *ni,
                    wbuf_t wbuf,
                    int subtype,
                    struct ieee80211_rx_status *rs)
{
     struct ieee80211vap    *vap = ni->ni_vap;
     switch (vap->iv_opmode) {
     case IEEE80211_M_HOSTAP:
          ieee80211_recv_ctrl_ap(ni,wbuf,subtype);
          break;
     default:
          break;
     }
    return EOK;
}

/*
 * send an action frame.
 * @param vap      : vap pointer.
 * @param dst_addr : destination address.
 * @param src_addr : source address.(most of the cases vap mac address).
 * @param bssid    : BSSID or %NULL to use default
 * @param data     : data buffer conataining the action frame including action category  and type.
 * @param data_len : length of the data buffer.
 * @param handler  : hanlder called when the frame transmission completes.
 * @param arg      : opaque pointer passed back via the handler.
 * @ returns 0 if success, -ve if failed.
 */   
int ieee80211_vap_send_action_frame(struct ieee80211vap *vap,const u_int8_t *dst_addr,const  u_int8_t *src_addr, const u_int8_t *bssid,
                                    const u_int8_t *data, u_int32_t data_len, ieee80211_vap_complete_buf_handler handler, void *arg)
{
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;
    struct ieee80211_frame *wh;
    struct ieee80211_node *ni=NULL;
    struct ieee80211com *ic = vap->iv_ic;

    /*
     * sanity check the data length.
     */
    if ( (data_len + sizeof(struct ieee80211_frame)) >= MAX_TX_RX_PACKET_SIZE) {
        return  -ENOMEM;
    }

#ifdef ATH_SUPPORT_HTC
    /* check if vap is deleted */
    if (ieee80211_vap_deleted_is_set(vap)) {
        /* if vap is deleted then return an error */
        return  -EINVAL;
    }

    ieee80211_vap_active_set(vap);
#endif
    if (!ieee80211_vap_active_is_set(vap)) {
        /* if vap is not active then return an error */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,"%s: Error: vap is not set\n", __func__);
        return  -EINVAL;
    }

    wbuf = wbuf_alloc(ic->ic_osdev, WBUF_TX_MGMT, MAX_TX_RX_PACKET_SIZE);

    if (wbuf == NULL) {
        return  -ENOMEM;
    }

    /*
     * if a node exist with the given address already , use it.
     * if not use bss node.
     */
    ni = ieee80211_find_txnode(vap, dst_addr);
    if (ni == NULL) {
        ni = ieee80211_ref_node(vap->iv_bss);
    }
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);

    ieee80211_send_setup(vap, vap->iv_bss, wh,
                         IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_ACTION,
                         src_addr, dst_addr, bssid ? bssid : ni->ni_bssid);
    frm = (u_int8_t *)&wh[1];
    /*
     * copy the data part into the data area.
     */
    OS_MEMCPY(frm,data,data_len);

    wbuf_set_pktlen(wbuf, data_len + (u_int32_t)sizeof(struct ieee80211_frame));
    if (handler) {
        ieee80211_vap_set_complete_buf_handler(wbuf,handler,arg); 
    }
 
    /* management action frame can not go out if the vap is in forced pause state */
    if (ieee80211_vap_is_force_paused(vap)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER,
                          "[%s] queue action management frame, the vap is in forced pause state\n",__func__);
        ieee80211_node_saveq_queue(ni,wbuf,IEEE80211_FC0_TYPE_MGT);
    } else {
        if (ieee80211_send_mgmt(vap,ni, wbuf,false) != EOK) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT,
                              "[%s] failed to send management frame\n",__func__);
        }
    } 
    ieee80211_free_node(ni);

    return EOK;
}

/**
* send action management frame.
* @param freq     : channel to send on (only to validate/match with current channel)
* @param arg      : arg (will be used in the mlme_action_send_complete) 
* @param dst_addr : destination mac address
* @param src_addr : source mac address
* @param bssid    : bssid
* @param data     : includes total payload of the action management frame.
* @param data_len : data len.
* @returns 0 if succesful and -ve if failed.
* if the radio is not on the passedf in freq then it will return an error. 
* if returns 0 then mlme_action_send_complete will be called with the status of
* the frame transmission. 
*/
int wlan_vap_send_action_frame(wlan_if_t vap_handle, u_int32_t freq, 
                               wlan_action_frame_complete_handler handler, void *arg,const u_int8_t *dst_addr,
                               const u_int8_t *src_addr, const u_int8_t *bssid, const u_int8_t *data, u_int32_t data_len)
{
    struct ieee80211com *ic = vap_handle->iv_ic;
    /* send action frame */
    if (freq) {
        if(freq !=  ieee80211_chan2freq(ic,ic->ic_curchan)) {
            /* frequency does not match */
            IEEE80211_DPRINTF(vap_handle, IEEE80211_MSG_ANY,
                              "%s: Error: frequency does not match. req=%d, curr=%d\n",
                              __func__, freq, ieee80211_chan2freq(ic,ic->ic_curchan));
            return -EINVAL;
        }
    }
    return ieee80211_vap_send_action_frame(vap_handle,dst_addr,src_addr,bssid,data,data_len, 
                    handler,arg);
}

#if ATH_SUPPORT_CFEND
/*
 *  Allocate a CF-END frame and fillin the appropriate bits.
 *  */
wbuf_t
ieee80211_cfend_alloc(struct ieee80211com *ic)
{
    wbuf_t cfendbuf;
    struct ieee80211_ctlframe_addr2 *wh;
    const u_int8_t macAddr[6] = {0xff,0xff,0xff,0xff,0xff,0xff};

    cfendbuf = wbuf_alloc(ic->ic_osdev, WBUF_TX_CTL, sizeof(struct ieee80211_ctlframe_addr2));
    if (cfendbuf == NULL) {
        return cfendbuf;
    }
    
    wbuf_append(cfendbuf, sizeof(struct ieee80211_ctlframe_addr2));
    wh = (struct ieee80211_ctlframe_addr2*) wbuf_header(cfendbuf);
   
    *(u_int16_t *)(&wh->i_aidordur) = htole16(0x0000);
    wh->i_fc[0] = 0;
    wh->i_fc[1] = 0;
    wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_CTL |
        IEEE80211_FC0_SUBTYPE_CF_END;

    IEEE80211_ADDR_COPY(wh->i_addr1, macAddr);
    IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_my_hwaddr);

    /*if( vap->iv_opmode == IEEE80211_M_HOSTAP ) {
        wh->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;
    } else {
        wh->i_fc[1] = IEEE80211_FC1_DIR_TODS;
    }*/
    return cfendbuf;
}
#endif

void
wlan_addba_request_handler(void *arg, wlan_node_t node)
{
    struct ieee80211_addba_delba_request *ad = arg;
    struct ieee80211_node *ni = node;
    struct ieee80211com *ic = ad->ic;
    int ret;

    if (ni->ni_associd == 0 ||
        IEEE80211_AID(ni->ni_associd) != ad->aid) {
        return;
    }

    switch (ad->action)
    {
    default:
        return;
    case ADDBA_SEND:
        ret = ic->ic_addba_send(ni, ad->tid, ad->arg1);
        if (ret != 0)  {
            printk("ADDBA send failed: recipient is not a 11n node\n");
        }
        break;
    case ADDBA_STATUS:
        ic->ic_addba_status(ni, ad->tid, &(ad->status));
        break;
    case DELBA_SEND:
        ic->ic_delba_send(ni, ad->tid, ad->arg1, ad->arg2);
        break;
    case ADDBA_RESP:
        ic->ic_addba_setresponse(ni, ad->tid, ad->arg1);
        break;
    }
}
