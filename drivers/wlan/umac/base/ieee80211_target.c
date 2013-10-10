/*
 * Copyright (c) 2010, Atheros Communications Inc.
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

/*
 * IEEE 802.11 generic handler
 */

#include <osdep.h> 

#include <ieee80211_var.h>
#include <if_athproto.h>

#include <adf_os_mem.h> /* adf_os_mem_copy */

#ifdef ATH_SUPPORT_HTC

#include "htc_ieee_common.h"

/* target node bitmap valid dump */
void 
print_target_node_bitmap(struct ieee80211_node *ni)
{
    int i;
    
    printk("%s : ", __FUNCTION__);
    for (i = 0; i < HTC_MAX_NODE_NUM; i++) {
        printk("%d ", ni->ni_ic->target_node_bitmap[i].ni_valid);    
    }    
    printk("\n");
}

/* add/update target node nt dump */
void
print_nt(struct ieee80211_node_target* nt)
{
    printk("------------------------------------------------------------\n");    
    printk("ni_associd: 0x%x\n", nt->ni_associd);
    //printk("ni_txpower: 0x%x\n", nt->ni_txpower);

    printk("mac: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
        nt->ni_macaddr[0], nt->ni_macaddr[1], nt->ni_macaddr[2],
        nt->ni_macaddr[3], nt->ni_macaddr[4], nt->ni_macaddr[5]);
    printk("bssid: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
        nt->ni_bssid[0], nt->ni_bssid[1], nt->ni_bssid[2],
        nt->ni_bssid[3], nt->ni_bssid[4], nt->ni_bssid[5]);     

    printk("nodeindex: %d\n", nt->ni_nodeindex);
    printk("vapindex: %d\n", nt->ni_vapindex);
    //printk("ni_vapnode: 0x%x\n", nt->ni_vapnode);
    printk("ni_flags: 0x%x\n", nt->ni_flags);
    printk("ni_htcap: %d\n", nt->ni_htcap);
    //printk("ni_valid: %d\n", nt->ni_valid);
    printk("ni_capinfo: 0x%x\n", nt->ni_capinfo);
    //printk("ni_ic: 0x%08x\n", nt->ni_ic);
    //printk("ni_vap: 0x%08x\n", nt->ni_vap);
    //printk("ni_txseqmgmt: %d\n", nt->ni_txseqmgmt);
    printk("ni_is_vapnode: %d\n", nt->ni_is_vapnode);   
    printk("ni_maxampdu: %d\n", nt->ni_maxampdu);   
    //printk("ni_iv16: %d\n", nt->ni_iv16);   
    //printk("ni_iv32: %d\n", nt->ni_iv32);   
#ifdef ENCAP_OFFLOAD
    //printk("ni_ucast_keytsc: %d\n", nt->ni_ucast_keytsc);   
    //printk("ni_ucast_keytsc: %d\n", nt->ni_ucast_keytsc);   
#endif
    printk("------------------------------------------------------------\n");    
}

u_int8_t
ieee80211_mac_addr_cmp(u_int8_t *src, u_int8_t *dst)
{
    int i;

    for (i = 0; i < IEEE80211_ADDR_LEN; i++) {
        if (src[i] != dst[i])
            return 1;
    }

    return 0;
}

void
ieee80211_add_vap_target(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211vap_target vt;
    int i;
    u_int8_t vapindex = 0;

#if 0
    /* Dump mac address */
    printk("%s, mac: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
        __FUNCTION__, vap->iv_myaddr[0], vap->iv_myaddr[1], vap->iv_myaddr[2],
        vap->iv_myaddr[3], vap->iv_myaddr[4], vap->iv_myaddr[5]);
#endif

    for (i = 0; i < HTC_MAX_VAP_NUM; i++) {
        /* avoid upper layer add the same vap twice */
        if (ieee80211_mac_addr_cmp(ic->target_vap_bitmap[i].vap_macaddr,
                                       vap->iv_myaddr) == 0) {
            printk("%s: add the same vap twice\n", __FUNCTION__);
            return;
        }

        if (ic->target_vap_bitmap[i].vap_valid == 0) {
            printk("%s: target_vap_bitmap(%d) is invalid\n", __FUNCTION__, i);

            OS_MEMCPY(ic->target_vap_bitmap[i].vap_macaddr, vap->iv_myaddr,
                IEEE80211_ADDR_LEN);
            ic->target_vap_bitmap[i].vap_valid = 1;
            vapindex = i;
            break;
        }
        else {
#if 0        	
            printk("%s: target_vap_bitmap(%d) vap_valid = %d\n", __FUNCTION__, i,
            ic->target_vap_bitmap[i].vap_valid);
#endif            
        }
    }

    printk("%s: vap index: %d\n", __FUNCTION__, vapindex);

    /* If we can't find any suitable vap entry */
    if (i == HTC_MAX_VAP_NUM) {
        printk("%s: exceed maximum vap number\n", __FUNCTION__);
        return;
    }

    vt.iv_create_flags = cpu_to_be32(vap->iv_create_flags);
    vt.iv_vapindex = vapindex;
    vt.iv_opmode = cpu_to_be32(vap->iv_opmode);
    vt.iv_mcast_rate = 0;
    vt.iv_rtsthreshold = cpu_to_be16(2304);
    adf_os_mem_copy(&vt.iv_myaddr, &(vap->iv_myaddr), IEEE80211_ADDR_LEN);

#if ENCAP_OFFLOAD
    adf_os_mem_copy(&vt.iv_myaddr, &(vap->iv_myaddr), IEEE80211_ADDR_LEN);
#endif
    /* save information to vap target */
    ic->target_vap_bitmap[vapindex].iv_vapindex = vapindex;
    ic->target_vap_bitmap[vapindex].iv_opmode = vap->iv_opmode;
    ic->target_vap_bitmap[vapindex].iv_mcast_rate = vap->iv_mcast_rate;
    ic->target_vap_bitmap[vapindex].iv_rtsthreshold = 2304;

    ic->ic_add_vap_target(ic, &vt, sizeof(vt));

    //printk("%s Exit \n", __FUNCTION__);
}

void
ieee80211_add_node_target(struct ieee80211_node *ni, struct ieee80211vap *vap,
    u_int8_t is_vap_node)
{
    struct ieee80211_node_target nt, *ntar;
    int i;
    u_int8_t nodeindex = 0;
    u_int8_t vapindex = 0xff;
    
    OS_MEMZERO(&nt, sizeof(struct ieee80211_node_target));

    //printk("%s: vap: 0x%08x, ni: 0x%08x, is_vap_node: %d\n", __FUNCTION__, vap, ni, is_vap_node);
    //print_target_node_bitmap(ni);

    for (i = 0; i < HTC_MAX_NODE_NUM; i++) {
        /* avoid upper layer add the same vap twice */
        if (ieee80211_mac_addr_cmp(ni->ni_ic->target_node_bitmap[i].ni_macaddr,
                                       ni->ni_macaddr) == 0) {
            printk("%s: add the same node twice, index: %d\n", __FUNCTION__, i);
            return;
        }

        if (ni->ni_ic->target_node_bitmap[i].ni_valid == 0) {
            OS_MEMCPY(ni->ni_ic->target_node_bitmap[i].ni_macaddr, ni->ni_macaddr,
                IEEE80211_ADDR_LEN);
            ni->ni_ic->target_node_bitmap[i].ni_valid = 1;
            nodeindex = i;
            break;
        }
    }

    if (i == HTC_MAX_NODE_NUM) {
        printk("%s: exceed maximum node number\n", __FUNCTION__);
        return;
    }

    /* search for vap index of this node */
    for (i = 0; i < HTC_MAX_VAP_NUM; i++) {
        if (ni->ni_ic->target_vap_bitmap[i].vap_valid == 1 && 
            ieee80211_mac_addr_cmp(ni->ni_ic->target_vap_bitmap[i].vap_macaddr,
                                           ni->ni_vap->iv_myaddr) == 0) {
            vapindex = i;
            break;
        }
    }

    /*
     * Fill the target node nt.
     */
    nt.ni_associd    = htons(ni->ni_associd);
    nt.ni_txpower    = htons(ni->ni_txpower);

    adf_os_mem_copy(&nt.ni_macaddr, &(ni->ni_macaddr), (IEEE80211_ADDR_LEN * sizeof(u_int8_t)));
    adf_os_mem_copy(&nt.ni_bssid, &(ni->ni_bssid), (IEEE80211_ADDR_LEN * sizeof(u_int8_t)));

    nt.ni_nodeindex  = nodeindex;//ni->ni_nodeindex;
    nt.ni_vapindex   = vapindex;
    nt.ni_vapnode    = 0;//add for rate control test
    nt.ni_flags      = htonl(ni->ni_flags);

    nt.ni_htcap      = htons(ni->ni_htcap);
    nt.ni_capinfo    = htons(ni->ni_capinfo);
    nt.ni_is_vapnode = is_vap_node; //1;
    nt.ni_maxampdu   = htons(ni->ni_maxampdu);
#ifdef ENCAP_OFFLOAD    
    nt.ni_ucast_keytsc = 0;
    nt.ni_mcast_keytsc = 0;
#endif

    ntar = &nt;
   
    /* save information to node target */
    ni->ni_ic->target_node_bitmap[nodeindex].ni_nodeindex = nodeindex;
    ni->ni_ic->target_node_bitmap[nodeindex].ni_vapindex = vapindex;
    ni->ni_ic->target_node_bitmap[nodeindex].ni_is_vapnode = is_vap_node;

    ni->ni_ic->ic_add_node_target(ni->ni_ic, ntar, sizeof(struct ieee80211_node_target));
    //print_nt(&nt);
}

void
ieee80211_delete_node_target(struct ieee80211_node *ni, struct ieee80211com *ic,
    struct ieee80211vap *vap, u_int8_t is_reset_bss)
{
    int i;
    u_int8_t nodeindex = 0xff;

    //printk("%s: vap: 0x%08x, ni: 0x%08x, is_reset_bss: %d\n", __FUNCTION__, vap, ni, is_reset_bss);

    /* Dump mac address */
    /* printk("%s, mac: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
        __FUNCTION__, ni->ni_macaddr[0], ni->ni_macaddr[1], ni->ni_macaddr[2],
        ni->ni_macaddr[3], ni->ni_macaddr[4], ni->ni_macaddr[5]); */

    for (i = 0; i < HTC_MAX_NODE_NUM; i++) {
        if (ni->ni_ic->target_node_bitmap[i].ni_valid == 1) {
            /* avoid upper layer add the same vap twice */
            if (ieee80211_mac_addr_cmp(ic->target_node_bitmap[i].ni_macaddr,
                                           ni->ni_macaddr) == 0) {
                if(is_reset_bss == 1 && ic->target_node_bitmap[i].ni_is_vapnode == 1)
                {
                    return;
                }                                            
                OS_MEMZERO(ic->target_node_bitmap[i].ni_macaddr, IEEE80211_ADDR_LEN);
                ic->target_node_bitmap[i].ni_valid = 0;
                nodeindex = i;
                break;
            }
        }
    }

    if (i == HTC_MAX_NODE_NUM) {
        /*printk("%s: Can't find desired node\n", __FUNCTION__);*/
        return;
    }

    /* printk("%s: ni: 0x%08x, nodeindex: %d, is_reset_bss: %d\n", __FUNCTION__,
    (u_int32_t)ni, nodeindex, is_reset_bss); */

    ic->ic_delete_node_target(ic, &nodeindex, sizeof(nodeindex));
}

#if ATH_SUPPORT_HTC
void
ieee80211_update_node_target(struct ieee80211_node *ni, struct ieee80211vap *vap)
{
    struct ieee80211_node_target nt, *ntar;
    int i;
    u_int8_t nodeindex = 0;
    u_int8_t vapindex = 0xff;

    OS_MEMZERO(&nt, sizeof(struct ieee80211_node_target));
    
    //printk("%s: vap: 0x%08x, ni: 0x%08x\n", __FUNCTION__, vap, ni);
    //print_target_node_bitmap(ni);

    for (i = 0; i < HTC_MAX_NODE_NUM; i++) {
        if (ieee80211_mac_addr_cmp(ni->ni_ic->target_node_bitmap[i].ni_macaddr,
                                       ni->ni_macaddr) == 0) {                                        
            break;
        }
    }
    
    if ((i < HTC_MAX_NODE_NUM) && (ni->ni_ic->target_node_bitmap[i].ni_valid == 1)) {
        //printk("%s : find node index (%d)\n", __FUNCTION__, i);
        nodeindex = i;            
    }
    else {
        //printk("%s:  can't find the update node\n", __FUNCTION__);
        return;
    }

    /* search for vap index of this node */
    for (i = 0; i < HTC_MAX_VAP_NUM; i++) {
        if (ni->ni_ic->target_vap_bitmap[i].vap_valid == 1 && 
            ieee80211_mac_addr_cmp(ni->ni_ic->target_vap_bitmap[i].vap_macaddr,
                                           ni->ni_vap->iv_myaddr) == 0) {
            vapindex = i;
            break;
        }
    }

    /*
     * Fill the target node nt.
     */
    nt.ni_associd    = htons(ni->ni_associd);
    nt.ni_txpower    = htons(ni->ni_txpower);

    adf_os_mem_copy(&nt.ni_macaddr, &(ni->ni_macaddr), (IEEE80211_ADDR_LEN * sizeof(u_int8_t)));
    adf_os_mem_copy(&nt.ni_bssid, &(ni->ni_bssid), (IEEE80211_ADDR_LEN * sizeof(u_int8_t)));

    nt.ni_nodeindex  = nodeindex;//ni->ni_nodeindex;
    nt.ni_vapindex   = vapindex;
    nt.ni_vapnode    = 0;//add for rate control test
    nt.ni_flags      = htonl(ni->ni_flags);

    nt.ni_htcap      = htons(ni->ni_htcap);
    nt.ni_capinfo    = htons(ni->ni_capinfo);
    nt.ni_is_vapnode = ni->ni_ic->target_node_bitmap[nodeindex].ni_is_vapnode;
    nt.ni_maxampdu   = htons(ni->ni_maxampdu);
#ifdef ENCAP_OFFLOAD    
    nt.ni_ucast_keytsc = 0;
    nt.ni_mcast_keytsc = 0;
#endif

    ntar = &nt;

    ni->ni_ic->ic_update_node_target(ni->ni_ic, ntar, sizeof(struct ieee80211_node_target));

    /*
     * debug dump nt
     */
    //print_nt(&nt);   
}
#endif

#if ENCAP_OFFLOAD
void
ieee80211_update_vap_target(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211vap_update_tgt tmp_vap_update;
    u_int8_t vapindex = 0;

    adf_os_mem_zero(&tmp_vap_update , sizeof(struct ieee80211vap_update_tgt)); 

    /*searching vapindex */
    for (vapindex = 0; vapindex < HTC_MAX_VAP_NUM; vapindex++) {
        if ((ic->target_vap_bitmap[vapindex].vap_valid) && 
                (!ieee80211_mac_addr_cmp(ic->target_vap_bitmap[vapindex].vap_macaddr,vap->iv_myaddr))) 
            break;
    }

    tmp_vap_update.iv_flags         = htonl(vap->iv_flags);
    tmp_vap_update.iv_flags_ext     = htonl(vap->iv_flags_ext);
    tmp_vap_update.iv_vapindex      = htonl(vapindex);
    tmp_vap_update.iv_rtsthreshold  = htons(vap->iv_rtsthreshold);

    ic->ic_update_vap_target(ic, &tmp_vap_update, sizeof(tmp_vap_update));
}
#endif

void
ieee80211_update_target_ic(struct ieee80211_node *ni)
{
    struct ieee80211com_target ic_tgt;
    //WLAN_DEV_INFO *pDev = pIaw->pDev;
    //WLAN_STA_CONFIG *pCfg = &pDev->staConfig;
    int val;

    ni->ni_ic->ic_get_config_chainmask(ni->ni_ic, &val);
    ic_tgt.ic_tx_chainmask = val;    

    /* FIXME: Currently we fix these values */

    ic_tgt.ic_flags = cpu_to_be32(0x400c2400);
    ic_tgt.ic_flags_ext = cpu_to_be32(0x106080);
    ic_tgt.ic_ampdu_limit = cpu_to_be32(0xffff);    
    ic_tgt.ic_ampdu_subframes = 20;
    ic_tgt.ic_tx_chainmask_legacy = 1;
    ic_tgt.ic_rtscts_ratecode = 0;
    ic_tgt.ic_protmode = 1;   
    
    ni->ni_ic->ic_htc_ic_update_target(ni->ni_ic, &ic_tgt, sizeof(struct ieee80211com_target));
}

void
ieee80211_delete_vap_target(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211vap_target vt;
    u_int8_t vapindex = 0;
    int i;

#if 0
    /* Dump mac address */
    printk("%s, mac: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
        __FUNCTION__, vap->iv_myaddr[0], vap->iv_myaddr[1], vap->iv_myaddr[2],
        vap->iv_myaddr[3], vap->iv_myaddr[4], vap->iv_myaddr[5]);
#endif

    for (i = 0; i < HTC_MAX_VAP_NUM; i++) {
        if (ic->target_vap_bitmap[i].vap_valid == 1) {
            /* Compare mac address */
            if (ieee80211_mac_addr_cmp(ic->target_vap_bitmap[i].vap_macaddr,
                                           vap->iv_myaddr) == 0) {
                vapindex = i;
                ic->target_vap_bitmap[i].vap_valid = 0;
                OS_MEMZERO(ic->target_vap_bitmap[i].vap_macaddr, IEEE80211_ADDR_LEN);
                break;
            }
        }
    }

    if (i == HTC_MAX_VAP_NUM) {
        printk("%s: Can't find desired vap\n", __FUNCTION__);
        return;
    }

    //printk("%s: vapindex: %d\n", __FUNCTION__, vapindex);

    vt.iv_vapindex = vapindex;//vap->iv_vapindex;
    vt.iv_opmode = 0;//vap->iv_opmode;
    vt.iv_mcast_rate = 0;//vap->iv_mcast_rate; 
    vt.iv_rtsthreshold = 0;//vap->iv_rtsthreshold;

    ic->ic_delete_vap_target(ic, &vt, sizeof(vt));

    //printk("%s Exit \n", __FUNCTION__);
}

#endif /* #ifdef ATH_SUPPORT_HTC */
