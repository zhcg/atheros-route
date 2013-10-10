/*
 *  Copyright (c) 2009 Atheros Communications Inc.
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
 * \file ieee80211_me.c
 * \brief Atheros multicast enhancement algorithm, as part of 
 * Atheros iQUE feature set.
 *
 * This file contains the main implementation of the Atheros multicast 
 * enhancement functionality.
 *
 * The main purpose of this module is to convert (by translating or 
 * tunneling) the multicast stream into duplicated unicast streams for 
 * performance enhancement of home wireless applications. For more 
 * details, please refer to the design documentation.
 *
 */

#include <ieee80211_ique.h>
#if ATH_SUPPORT_IQUE
#include "osdep.h"
#include "ieee80211_me_priv.h"
#include "if_athproto.h"

static struct MC_GROUP_LIST_ENTRY *
ieee80211_me_SnoopListFindMember(struct ieee80211vap *vap, u_int8_t *gaL2, u_int8_t *saL2);
static struct MC_GROUP_LIST_ENTRY *
ieee80211_me_SnoopListFindEntry(struct ieee80211vap *vap, struct ieee80211_node *ni);
static void ieee80211_me_SnoopListDelEntry(struct ieee80211vap *vap, 
    struct MC_GROUP_LIST_ENTRY *pEntry, int entry_cnt);
static void ieee80211_me_SnoopListInit(struct ieee80211vap *vap);
static int ieee80211_me_SnoopListUpdate(struct ieee80211vap *vap, int cmd,
    u_int8_t *gaL2, u_int8_t *saL2, struct ieee80211_node *ni, u_int16_t vlan_id);
static int ieee80211_me_SnoopListGetGroup(struct ieee80211vap *vap, u_int8_t *gaL2, u_int8_t *table, u_int16_t vlan_id);
OS_DECLARE_TIMER(ieee80211_me_SnoopListTimer);
static void ieee80211_me_detach(struct ieee80211vap * vap);
static int ieee80211_me_SnoopConvert(struct ieee80211vap *vap, wbuf_t wbuf);
static void ieee80211_me_SnoopWDSNodeCleanup(struct ieee80211_node *ni);
static void ieee80211_me_SnoopInspecting(struct ieee80211vap *vap, struct ieee80211_node *ni, wbuf_t wbuf);
static void ieee80211_me_SnoopListDump(struct ieee80211vap *vap);

int bitfield_endianess;

/**********************************************************************
 * !
 * \brief Find the member of the snoop list with matching group address
 * and MAC address of STA
 *
 * \param   vap
 * 			gaL2 Group address
 * 			saL2 MAC address of STA
 *
 * 	\return entry for the matching item in the snoop list
 */

static struct MC_GROUP_LIST_ENTRY *
ieee80211_me_SnoopListFindMember(struct ieee80211vap *vap, u_int8_t *gaL2,
    u_int8_t *saL2)
{
    struct MC_SNOOP_LIST *ps; 
    struct MC_GROUP_LIST_ENTRY *tmpEntry;
    u_int8_t address[6]={0x00,0x00,0x00,0x00,0x00,0x00};
    rwlock_state_t lock_state;
    ps = &(vap->iv_me->ieee80211_me_snooplist);	
    IEEE80211_SNOOP_LOCK(ps, &lock_state);
    TAILQ_FOREACH(tmpEntry, &ps->msl_node, mgl_list) {
    
        if (IEEE80211_ADDR_EQ(gaL2, tmpEntry->mgl_group_addr) &&
            (IEEE80211_ADDR_EQ(saL2, tmpEntry->mgl_group_member) || IEEE80211_ADDR_EQ(address, tmpEntry->mgl_group_member)))
        {
            /* found match */
            IEEE80211_SNOOP_UNLOCK(ps, &lock_state);
            return tmpEntry;
        }
    }

    IEEE80211_SNOOP_UNLOCK(ps, &lock_state);
    return NULL;
}

/***************************************************************************
 * !
 * \brief Find the entry of the item in the snoop list matching the node pointer
 *
 * \param   vap Pointer to the vap
 * 			ni Pointer to the node
 *
 * 	\return Pointer to the matching item in the snoop list
 */
static struct MC_GROUP_LIST_ENTRY *
ieee80211_me_SnoopListFindEntry(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
    struct MC_SNOOP_LIST *ps;
    struct MC_GROUP_LIST_ENTRY *tmpEntry;
    rwlock_state_t lock_state;
    ps = &(vap->iv_me->ieee80211_me_snooplist);
    IEEE80211_SNOOP_LOCK(ps, &lock_state);
    TAILQ_FOREACH(tmpEntry, &ps->msl_node, mgl_list)
    {
        if(IEEE80211_ADDR_EQ(ni->ni_macaddr, tmpEntry->mgl_group_member))
        {
            IEEE80211_SNOOP_UNLOCK(ps, &lock_state);

            return(tmpEntry);
        }
    }
    IEEE80211_SNOOP_UNLOCK(ps, &lock_state);

    return(NULL);
}


/*****************************************************************************
 * !
 * \brief Delete one entry in the snoop list
 *
 * \param vap Pointer to the VAP 
 * 		  pEntry Pointer to the item to be deleted in the snoop list
 *
 * 	\return N/A
 */
static void 
ieee80211_me_SnoopListDelEntry(struct ieee80211vap *vap, 
    struct MC_GROUP_LIST_ENTRY *pEntry, int entry_cnt)
{
    struct MC_SNOOP_LIST *ps;
    rwlock_state_t lock_state;
    ps = &(vap->iv_me->ieee80211_me_snooplist);	
    if (vap->iv_me->me_debug & IEEE80211_ME_DBG_DEBUG)
    {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IQUE, "Snoop Entry Del [%2.2x] (%2.2x) <%10lu>\n",
            pEntry->mgl_group_addr[5], pEntry->mgl_group_member[5], jiffies);
    }

    IEEE80211_SNOOP_LOCK(ps, &lock_state);
    if (entry_cnt > 1) {
        TAILQ_REMOVE(&ps->msl_node, pEntry, mgl_list);
        ps->msl_group_list_count--;
        OS_FREE(pEntry);
    } else {
        /*
         * If this is the last entry for this group address (entry_cnt == 1),
         * we keep this empty entry but set the STA address as 0 
         * and do NOT delete this entry. 
         * Then, if mc_discard_mcast is set, all mcast frames to
         * the mcast address with empty entry will be discarded 
         * to save the bandwidth for other ucast streaming.
         * This feature can be turned off by setting mc_discard_mcast to 0.
         * */    
        OS_MEMSET(pEntry->mgl_group_member, 0, IEEE80211_ADDR_LEN);
    }
    IEEE80211_SNOOP_UNLOCK(ps, &lock_state);

    return;
}

/*
 * DEBUG
 * Dump the mc snoop table.
 */
static void
ieee80211_me_SnoopListDump(struct ieee80211vap *vap)
{
    struct MC_SNOOP_LIST *ps;
    struct MC_GROUP_LIST_ENTRY *tmpEntry;
    ps = &(vap->iv_me->ieee80211_me_snooplist);	
    printf("Snoop Dump:\n");
    printf("  count  %d\n", ps->msl_group_list_count);
    printf("  misc   %d\n", ps->msl_misc);
    printf("  headf  %p\n", ps->msl_node.tqh_first);
    printf("  headl  %p\n", ps->msl_node.tqh_last);
    if (ps->msl_node.tqh_last) {
        printf("  headl* %p\n", *ps->msl_node.tqh_last);
    } else {
        printf("  headl null\n");
    }
    if (TAILQ_EMPTY(&ps->msl_node)) {
        printf("  ---- Empty ----\n");
    } else {
        printf("  ---------------\n");

        TAILQ_FOREACH(tmpEntry, &ps->msl_node, mgl_list) {

            printf("    group     %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
                    tmpEntry->mgl_group_addr[0],
                    tmpEntry->mgl_group_addr[1],
                    tmpEntry->mgl_group_addr[2],
                    tmpEntry->mgl_group_addr[3],
                    tmpEntry->mgl_group_addr[4],
                    tmpEntry->mgl_group_addr[5]);
            printf("    member    %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
                    tmpEntry->mgl_group_member[0],
                    tmpEntry->mgl_group_member[1],
                    tmpEntry->mgl_group_member[2],
                    tmpEntry->mgl_group_member[3],
                    tmpEntry->mgl_group_member[4],
                    tmpEntry->mgl_group_member[5]);
            printf("    node      %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
                    tmpEntry->mgl_ni->ni_macaddr[0],
                    tmpEntry->mgl_ni->ni_macaddr[1],
                    tmpEntry->mgl_ni->ni_macaddr[2],
                    tmpEntry->mgl_ni->ni_macaddr[3],
                    tmpEntry->mgl_ni->ni_macaddr[4],
                    tmpEntry->mgl_ni->ni_macaddr[5]);
            printf("    timestamp %d\n", tmpEntry->mgl_timestamp);
            printf("	xmited: %d frames\n", tmpEntry->mgl_xmited);
            printf("	vlan_id: %x\n", tmpEntry->vlan_id);
        }
    }
}

/**************************************************************************
 * !
 * \brief Show the Group Deny Table
 *
 * \param vap
 *
 * \return N/A
 */
void ieee80211_me_SnoopShowDenyTable(struct ieee80211vap *vap)
{
    int idx;
    struct MC_SNOOP_LIST *ps;
    rwlock_state_t lock_state;
    ps = &(vap->iv_me->ieee80211_me_snooplist);	
    IEEE80211_SNOOP_LOCK(ps, &lock_state);
    if (ps->msl_deny_count == 0) {
        printk("ME Snoop Deny Table is empty!\n");
        IEEE80211_SNOOP_UNLOCK(ps, &lock_state);
        return;
    }
    printk("ME Snoop Deny Table is:\n");
    for (idx = 0 ; idx < ps->msl_deny_count; idx ++) {
        printk("%d - addr : %d.%d.%d.%d, mask : %d.%d.%d.%d\n", idx + 1,
                ps->msl_deny_group[idx][0],   
                ps->msl_deny_group[idx][1],   
                ps->msl_deny_group[idx][2],   
                ps->msl_deny_group[idx][3],   
                ps->msl_deny_mask[idx][0],   
                ps->msl_deny_mask[idx][1],   
                ps->msl_deny_mask[idx][2],   
                ps->msl_deny_mask[idx][3]);   
    }
    IEEE80211_SNOOP_UNLOCK(ps, &lock_state);
}

/*
 * Check if the address is in deny list
 */
static int
ieee80211_me_SnoopIsDenied(struct ieee80211vap *vap, u_int8_t *ga)
{
    int idx;
    struct MC_SNOOP_LIST *ps;
    ps = &(vap->iv_me->ieee80211_me_snooplist);	
    
    if (ps->msl_deny_count == 0) {
        return 0;
    }
    for (idx = 0 ; idx < ps->msl_deny_count; idx ++) {
        if ((ga[0] & ps->msl_deny_mask[idx][0]) != 
             (ps->msl_deny_group[idx][0] & ps->msl_deny_mask[idx][0]))
        {
             continue;   
        }
        if ((ga[1] & ps->msl_deny_mask[idx][1]) != 
             (ps->msl_deny_group[idx][1] & ps->msl_deny_mask[idx][1]))
        {
             continue;   
        }
        if ((ga[2] & ps->msl_deny_mask[idx][2]) != 
             (ps->msl_deny_group[idx][2] & ps->msl_deny_mask[idx][2]))
        {
             continue;   
        }
        if ((ga[3] & ps->msl_deny_mask[idx][3]) != 
             (ps->msl_deny_group[idx][3] & ps->msl_deny_mask[idx][3]))
        {
             continue;   
        }
        return 1;
    }
    return 0;
}

/*
 * Clear the Snoop Deny Table
 */
void ieee80211_me_SnoopClearDenyTable(struct ieee80211vap *vap)
{
    struct MC_SNOOP_LIST *ps;
    ps = &(vap->iv_me->ieee80211_me_snooplist);	
    ps->msl_deny_count = 0;
    return;
}

/*
 * Add the Snoop Deny Table entry
 */
void ieee80211_me_SnoopAddDenyEntry(struct ieee80211vap *vap, int *addr)
{
    u_int8_t idx, i;
    struct MC_SNOOP_LIST *ps;
    rwlock_state_t lock_state;
    ps = &(vap->iv_me->ieee80211_me_snooplist);	
    idx = ps->msl_deny_count;
    IEEE80211_SNOOP_LOCK(ps, &lock_state);
    ps->msl_deny_count ++;
    for (i = 0; i < 4; i ++) {
        ps->msl_deny_group[idx][i] = (u_int8_t)addr[i];
    }
    ps->msl_deny_mask[idx][0] = 255;
    ps->msl_deny_mask[idx][1] = 255;
    ps->msl_deny_mask[idx][2] = 255;
    ps->msl_deny_mask[idx][3] = 0;

    IEEE80211_SNOOP_UNLOCK(ps, &lock_state);
    return;
}

/**************************************************************************
 * !
 * \brief Initialize the mc snooping and encapsulation feature.
 *
 * \param vap
 *
 * \return N/A
 */
static void ieee80211_me_SnoopListInit(struct ieee80211vap *vap)
{
    vap->iv_me->ieee80211_me_snooplist.msl_group_list_count = 0;
    vap->iv_me->ieee80211_me_snooplist.msl_misc = 0;
    IEEE80211_SNOOP_LOCK_INIT(&vap->iv_me->ieee80211_me_snooplist, "Snoop Table");
    TAILQ_INIT(&vap->iv_me->ieee80211_me_snooplist.msl_node);
}

/**************************************************************************
 * !
 * \brief Add or remove entries in the mc snoop table based on
 * IGMP JOIN or LEAVE messages.
 *
 * \param vap
 * 		  cmd	IGMP Command
 * 		  gaL2	Group address
 * 		  saL2	MAC address of the STA
 * 		  ni	Pointer to the node
 *
 * \return		  
 */
static int ieee80211_me_SnoopListUpdate(struct ieee80211vap *vap, int cmd,
    u_int8_t *gaL2, u_int8_t *saL2, struct ieee80211_node *ni, u_int16_t vlan_id)
{
    struct MC_SNOOP_LIST *ps = &(vap->iv_me->ieee80211_me_snooplist);	
    struct MC_GROUP_LIST_ENTRY *pNew, *pOld;
    systime_t timestamp; 
    rwlock_state_t lock_state;
    int sta_entry;
    
    if (cmd == IGMP_SNOOP_CMD_JOIN) {

        pNew = ieee80211_me_SnoopListFindMember(vap, gaL2, saL2);

        if (pNew) {
            IEEE80211_SNOOP_LOCK(ps, &lock_state);
            timestamp  = OS_GET_TIMESTAMP();
            pNew->mgl_timestamp = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(timestamp);
            IEEE80211_ADDR_COPY(pNew->mgl_group_member, saL2);
            pNew->vlan_id=vlan_id;
            IEEE80211_SNOOP_UNLOCK(ps, &lock_state);

            if (vap->iv_me->me_debug & IEEE80211_ME_DBG_DEBUG) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IQUE, "Snoop Entry Found [%2.2x] (%2.2x)\n", gaL2[5], saL2[5]);
                printk("Snoop Entry Found [%2.2x] (%2.2x)\n", gaL2[5], saL2[5]);
            }
        }
        else {
            if (vap->iv_me->me_debug & IEEE80211_ME_DBG_DEBUG) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IQUE, "Snoop Entry Add [%2.2x] (%2.2x) <%10lu>\n",
                    gaL2[5], saL2[5], jiffies);
                printk("Snoop Entry Add [%2.2x] (%2.2x) <%10lu>\n",
                    gaL2[5], saL2[5], jiffies);
            }

            if (ps->msl_group_list_count < ps->msl_max_length) {
                IEEE80211_SNOOP_LOCK(ps, &lock_state);
                ps->msl_group_list_count++;
                IEEE80211_SNOOP_UNLOCK(ps, &lock_state);

                pNew = (struct MC_GROUP_LIST_ENTRY *)OS_MALLOC(
                        vap-> iv_ic->ic_osdev,       
                        sizeof(struct MC_GROUP_LIST_ENTRY),
                        GFP_KERNEL);
        
                if (pNew != NULL) {
                    IEEE80211_SNOOP_LOCK(ps, &lock_state);
                    TAILQ_INSERT_TAIL(&ps->msl_node, pNew, mgl_list);
                    IEEE80211_ADDR_COPY(pNew->mgl_group_addr, gaL2);
                    IEEE80211_ADDR_COPY(pNew->mgl_group_member, saL2);
                    timestamp  = OS_GET_TIMESTAMP();
                    pNew->mgl_timestamp = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(timestamp);
                    pNew->mgl_ni = ni;
                    pNew->mgl_xmited = 0;
                    pNew->vlan_id=vlan_id;
                    IEEE80211_SNOOP_UNLOCK(ps, &lock_state);
                }
            } 	
        }
    }
    else if (cmd == IGMP_SNOOP_CMD_LEAVE) {

        pOld = ieee80211_me_SnoopListFindMember(vap, gaL2, saL2);
        if (pOld) {
            sta_entry = ieee80211_me_SnoopListGetGroup(vap, gaL2, NULL, 0xFFFF);
            ieee80211_me_SnoopListDelEntry(vap, pOld, sta_entry);
        }
        else {
            if (vap->iv_me->me_debug & IEEE80211_ME_DBG_DEBUG) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IQUE, "Snoop Entry Not Found [%2.2x] (%2.2x)\n", gaL2[5], saL2[5]);
                printk("Snoop Entry Not Found [%2.2x] (%2.2x)\n", gaL2[5], saL2[5]);
            }
        }
    }

    if (vap->iv_me->me_debug & IEEE80211_ME_DBG_DUMP) {
        ieee80211_me_SnoopListDump(vap);
    }

    return 0;
}

/**********************************************************************************
 * !
 * \brief Fill in tmp table of all unicast destinations for the given group.
 *
 * \param vap Pointer to VAP
 * 			gaL2 Group address
 * 			table Temporary table containing the matching items
 *
 * \return Count of table rows filled in.
 */
static int ieee80211_me_SnoopListGetGroup(struct ieee80211vap *vap, u_int8_t *gaL2,	u_int8_t *table, u_int16_t vlan_id)
{
    struct MC_SNOOP_LIST *ps = &(vap->iv_me->ieee80211_me_snooplist);	
    struct MC_GROUP_LIST_ENTRY *tmpEntry;
    rwlock_state_t lock_state;
    int idx = 0;
    int count = 0;

    IEEE80211_SNOOP_LOCK(ps, &lock_state);
    TAILQ_FOREACH(tmpEntry, &ps->msl_node, mgl_list) {
        if (IEEE80211_ADDR_EQ(gaL2, tmpEntry->mgl_group_addr)) {
            if (table) {
                if (tmpEntry->vlan_id==vlan_id) {
                    IEEE80211_ADDR_COPY(&table[idx], tmpEntry->mgl_group_member);
                    idx += IEEE80211_ADDR_LEN;
                    count++;
                }
            }
            else
                count++;
        }
    }
    IEEE80211_SNOOP_UNLOCK(ps, &lock_state);

    return count;
}


/*************************************************************************
 * !
 * \brief Function of the timer to update the snoop list 
 *
 * \param unsigned long arg been casted to pointer to vap
 *
 * \return N/A
 */
OS_TIMER_FUNC(ieee80211_me_SnoopListTimer)
{
    struct ieee80211vap *vap;
    struct MC_SNOOP_LIST *ps;	
    struct MC_GROUP_LIST_ENTRY *tmpEntry, *tmpEntry2;
    rwlock_state_t lock_state;
    systime_t timestamp  = OS_GET_TIMESTAMP();
    u_int32_t now        = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(timestamp);
    int sta_entry;

    OS_GET_TIMER_ARG(vap, struct ieee80211vap *);
    ps = (struct MC_SNOOP_LIST *)&(vap->iv_me->ieee80211_me_snooplist);	


    IEEE80211_SNOOP_LOCK(ps, &lock_state);
    tmpEntry = TAILQ_FIRST(&ps->msl_node);
    
    TAILQ_FOREACH_SAFE(tmpEntry, &ps->msl_node, mgl_list, tmpEntry2) {
        if (now - tmpEntry->mgl_timestamp > vap->iv_me->me_timeout) {
            /* Avoid dead lock in multi-cpu platform. */
            IEEE80211_SNOOP_UNLOCK(ps, &lock_state);
            sta_entry = ieee80211_me_SnoopListGetGroup
                    (vap, tmpEntry->mgl_group_addr, NULL, 0xFFFF);
            ieee80211_me_SnoopListDelEntry(vap, tmpEntry, sta_entry);
            IEEE80211_SNOOP_LOCK(ps, &lock_state);
        }
    }
    IEEE80211_SNOOP_UNLOCK(ps, &lock_state);
    OS_SET_TIMER(&vap->iv_me->snooplist_timer, vap->iv_me->me_timer);
    return;
}

/*************************************************************************
 * !
 * \brief Build up the snoop list by monitoring the IGMP packets
 *
 * \param ni Pointer to the node
 * 		  skb Pointer to the skb buffer
 *
 * \return N/A
 */
static void
ieee80211_me_SnoopInspecting(struct ieee80211vap *vap, struct ieee80211_node *ni, wbuf_t wbuf)
{
    struct ether_header *eh; 
    struct vlan_hdr *vhdr = NULL;
    u_int16_t vlan_id=0xFFFF;
        
    eh = (struct ether_header *) wbuf_header(wbuf);
    if (vap->iv_opmode == IEEE80211_M_HOSTAP ) {
        if (IEEE80211_IS_MULTICAST(eh->ether_dhost) &&
            !IEEE80211_IS_BROADCAST(eh->ether_dhost))
            {
                if ((eh->ether_type == htobe16(ETHERTYPE_IP))|| (eh->ether_type == htobe16(ETHERTYPE_VLAN))) {
                    const struct ip_header *ip = NULL;          
                    if (eh->ether_type == htobe16(ETHERTYPE_IP))
                        ip = (struct ip_header *)(wbuf_header(wbuf) + sizeof (struct ether_header));
                    if (eh->ether_type == htobe16(ETHERTYPE_VLAN)) {
                        ip = (struct ip_header *)(wbuf_header(wbuf) + sizeof (struct ether_header)+ sizeof(struct vlan_hdr));
                        vhdr = (struct vlan_hdr *)(wbuf_header(wbuf) + sizeof (struct ether_header)); 
                        vlan_id = vhdr->h_vlan_TCI & VLAN_VID_MASK;
                    }
                    if (ip->protocol == 2) {
                    /* ver1, ver2 */
                    int ip_headerlen;
                    const struct igmp_header *igmp = NULL;
                    const struct igmp_v3_report *igmpr3;
                    u_int32_t	groupAddr = 0;
                    int		cmd = IGMP_SNOOP_CMD_OTHER;
                    u_int8_t    *srcAddr = eh->ether_shost;
                    u_int8_t    groupAddrL2[IEEE80211_ADDR_LEN], groupAddrIP[4];

                    if (bitfield_endianess == BITFIELD_BIG_ENDIAN) {
                        ip_headerlen = ip->version_ihl & 0x0F;      
                    } else {
                        ip_headerlen = (ip->version_ihl & 0xF0) >> 4;      
                    }
                        
                    if (eh->ether_type == htobe16(ETHERTYPE_IP))
                        igmp = (struct igmp_header *)(wbuf_header(wbuf) + 
					   sizeof (struct ether_header) + 
					   (4 * ip_headerlen));
                    if (eh->ether_type == htobe16(ETHERTYPE_VLAN))
                        igmp = (struct igmp_header *)(wbuf_header(wbuf) + 
					   sizeof (struct ether_header) + 
					   sizeof(struct vlan_hdr) + 
					   (4 * ip_headerlen));
                    /* ver 3*/
                    igmpr3 = (struct igmp_v3_report *) igmp;

                    switch (igmp->type) {
                    case 0x11:
                        /* query */
                        groupAddr = htobe32(igmp->group);
                        break;
                    case 0x12:
                        /* V1 report */
                        groupAddr = htobe32(igmp->group);
                        cmd = IGMP_SNOOP_CMD_JOIN;
                        break;
                    case 0x16:
                        /* V2 report */
                        groupAddr = htobe32(igmp->group);
                        cmd = IGMP_SNOOP_CMD_JOIN;
                        break;
                    case 0x17:
                        /* V2 leave */
                        groupAddr = htobe32(igmp->group);
                        cmd = IGMP_SNOOP_CMD_LEAVE;
                        break;
                    case 0x22:
                        /* V3 report */
                        groupAddr = htobe32(igmpr3->grec[0].grec_mca);
                        if (htobe32(igmpr3->grec[0].grec_type) == IGMPV3_CHANGE_TO_EXCLUDE ||
                            htobe32(igmpr3->grec[0].grec_type) == IGMPV3_MODE_IS_EXCLUDE)
                        {
                            cmd = IGMP_SNOOP_CMD_JOIN;
                        }
                        else if (htobe32(igmpr3->grec[0].grec_type) == IGMPV3_CHANGE_TO_INCLUDE ||
                            htobe32(igmpr3->grec[0].grec_type) == IGMPV3_MODE_IS_INCLUDE)
                        {
                            cmd = IGMP_SNOOP_CMD_LEAVE;
                        }
                        break;
                    default:
                        break;
                    }
                    /*
                     * Check whether the group address is in the deny table
                     */
                    groupAddrIP[0] = (groupAddr >> 24) & 0xFF;
                    groupAddrIP[1] = (groupAddr >> 16) & 0xFF;
                    groupAddrIP[2] = (groupAddr >> 8) & 0xFF;
                    groupAddrIP[3] = groupAddr & 0xFF;
                    if (!(ieee80211_me_SnoopIsDenied(vap, groupAddrIP)) &&
                            (cmd == IGMP_SNOOP_CMD_JOIN || cmd == IGMP_SNOOP_CMD_LEAVE)) {
                        if (vap->iv_me->me_debug & IEEE80211_ME_DBG_INFO) {
                            ieee80211_note(vap, "IGMP %s %2.2x [%2.2x] %8.8x - %02x:%02x:%02x:%02x:%02x:%02x\n",
                                cmd == 2 ? "Leave" : (cmd == 1 ? "Join " : "other"),
                                igmp->type,
                                igmp->type == 0x22 ? htobe32(igmpr3->grec[0].grec_type) : 0,
                                groupAddr,
                                srcAddr[0],srcAddr[1],srcAddr[2],
                                srcAddr[3],srcAddr[4],srcAddr[5]);
                        }
                        groupAddrL2[0] = 0x01;
                        groupAddrL2[1] = 0x00;
                        groupAddrL2[2] = 0x5e;
                        groupAddrL2[3] = (groupAddr >> 16) & 0x7f;
                        groupAddrL2[4] = (groupAddr >>  8) & 0xff;
                        groupAddrL2[5] = (groupAddr >>  0) & 0xff;
                        ieee80211_me_SnoopListUpdate(vap, cmd, &groupAddrL2[0], srcAddr, ni, vlan_id);
                    }
                }
            }
        }
    }

    /* multicast tunnel egress */
    if (eh->ether_type == htobe16(ATH_ETH_TYPE) &&
        vap->iv_opmode == IEEE80211_M_STA &&
        vap->iv_me->mc_snoop_enable)
    {
        struct athl2p_tunnel_hdr *tunHdr;

        tunHdr = (struct athl2p_tunnel_hdr *) wbuf_pull(wbuf, sizeof(struct ether_header));
        /* check protocol subtype */
        eh = (struct ether_header *) wbuf_pull(wbuf, sizeof(struct athl2p_tunnel_hdr));
    }
#ifdef notyet
#ifdef PRINT_MCENCAP_DBG
    ieee80211_note(vap,"ieee80211_deliver_data() %d eh-%02x:%02x:%02x:%02x:%02x:%02x-%02x:%02x:%02x:%02x:%02x:%02x-%04x-%02x:%02x:%02x:%02x:%02x:%02x\n",
        skb_headroom(skb),
        eh->ether_dhost[0],eh->ether_dhost[1],eh->ether_dhost[2],
        eh->ether_dhost[3],eh->ether_dhost[4],eh->ether_dhost[5],
        eh->ether_shost[0],eh->ether_shost[1],eh->ether_shost[2],
        eh->ether_shost[3],eh->ether_shost[4],eh->ether_shost[5],
        eh->ether_type,
        *((unsigned char *)(skb->data) + 14),
        *((unsigned char *)(skb->data) + 15),
        *((unsigned char *)(skb->data) + 16),
        *((unsigned char *)(skb->data) + 17),
        *((unsigned char *)(skb->data) + 18),
        *((unsigned char *)(skb->data) + 19)
        );
#endif /* PRINT_MCENCAP_DBG */
#endif /* notyet */
    return;
}

/***************************************************************************
 * !
 * \brief Cleanup the snoop list for the specific node
 *
 * \param ni Pointer to the node
 *
 * \return N/A
 */
static void ieee80211_me_SnoopWDSNodeCleanup(struct ieee80211_node *ni)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct MC_SNOOP_LIST *ps = &(vap->iv_me->ieee80211_me_snooplist);
    int sta_entry;

    if(vap->iv_opmode == IEEE80211_M_HOSTAP &&
        vap->iv_me->mc_snoop_enable &&
        ps != NULL)
    {
        struct MC_GROUP_LIST_ENTRY *pEntry;

        while((pEntry = ieee80211_me_SnoopListFindEntry(vap, ni)) != NULL)
        {
#ifdef IEEE80211_DEBUG
            if(vap->iv_me->me_debug & IEEE80211_ME_DBG_DEBUG)
            {
                ieee80211_note(vap,
                    "%s: WDS node %02x:%02x:%02x:%02x:%02x:%02x\n", __func__,
                    ni->ni_macaddr[0],ni->ni_macaddr[1],ni->ni_macaddr[2],
                    ni->ni_macaddr[3],ni->ni_macaddr[4],ni->ni_macaddr[5]);
            }
#endif
    
            sta_entry = ieee80211_me_SnoopListGetGroup
                    (vap, pEntry->mgl_group_addr, NULL, 0xFFFF);
            ieee80211_me_SnoopListDelEntry(vap, pEntry, sta_entry);
        }
    }

    return;
}

/*******************************************************************
 * !
 * \brief Mcast enhancement option 1: Tunneling, or option 2: Translate
 *
 * Add an IEEE802.3 header to the mcast packet using node's MAC address
 * as the destination address
 *
 * \param 
 * 			vap Pointer to the virtual AP
 * 			wbuf Pointer to the wbuf
 *
 * \return number of packets converted and transmitted, or 0 if failed
 */

static int
ieee80211_me_SnoopConvert(struct ieee80211vap *vap, wbuf_t wbuf)
{
    struct ieee80211_node *ni = NULL;
    struct ether_header *eh;
    u_int8_t *dstmac;						/* reference to frame dst addr */
    u_int8_t srcmac[IEEE80211_ADDR_LEN];	/* copy of original frame src addr */
    u_int8_t grpmac[IEEE80211_ADDR_LEN];	/* copy of original frame group addr */
                                            /* table of tunnel group dest mac addrs */
    u_int8_t empty_entry_mac[IEEE80211_ADDR_LEN];
    u_int8_t newmac[MAX_SNOOP_ENTRIES][IEEE80211_ADDR_LEN];
    int newmaccnt = 0;						/* count of entries in newmac */
    int newmacidx = 0;						/* local index into newmac */
    int xmited = 0;							/* Number of packets converted and xmited */
    int sta_entry;
    struct ether_header *eh2;				/* eth hdr of tunnelled frame */
    struct athl2p_tunnel_hdr *tunHdr;		/* tunnel header */
    wbuf_t wbuf1 = NULL;			        /* cloned wbuf if necessary */
    struct MC_GROUP_LIST_ENTRY *mgle;
    systime_t timestamp;
    struct vlan_hdr * vhdr;
    u_int16_t vlan_id = 0xFFFF;

    if (vap->iv_me->mc_snoop_enable == 0) {
        /*
         * if mcast enhancement is disabled, return -1 to indicate 
         * that the frame's not been transmitted and continue the 
         * regular xmit path in wlan_vap_send
         */            
        return -1;
    }
    eh = (struct ether_header *)wbuf_header(wbuf);

    /* Get members of this group */
    /* save original frame's src addr once */
    IEEE80211_ADDR_COPY(srcmac, eh->ether_shost);
    IEEE80211_ADDR_COPY(grpmac, eh->ether_dhost);
    OS_MEMSET(empty_entry_mac, 0, IEEE80211_ADDR_LEN);
    dstmac = eh->ether_dhost;
    if (eh->ether_type == htobe16(ETHERTYPE_VLAN)) {
	vhdr = (struct vlan_hdr *)(wbuf_header(wbuf) + 
				   sizeof (struct ether_header)); 
	vlan_id = vhdr->h_vlan_TCI & VLAN_VID_MASK;
    }
    
    newmaccnt = ieee80211_me_SnoopListGetGroup(vap, eh->ether_dhost, &newmac[0][0],vlan_id);

    /* save original frame's src addr once */
    /*
     * If newmaccnt is 0: not multicast or multicast not in snoop table,
     *                    no tunnel
     *                 1: mc in table, only one dest, skb cloning not needed
     *                >1: multiple mc in table, skb cloned for each entry past
     *                    first one.
     */

    /* Maitain the original wbuf avoid being modified.
     */
    wbuf1 = wbuf_copy(wbuf);
    wbuf_complete(wbuf);
    wbuf = wbuf1;
    wbuf1 = NULL;

    /* loop start */
    do {	

        if (newmaccnt > 0) {	
            /* reference new dst mac addr */
            dstmac = &newmac[newmacidx][0];
        

            /*
             * Note - cloned here pre-tunnel due to the way ieee80211_classify()
             * currently works. This is not efficient.  Better to split 
             * ieee80211_classify() functionality into per node and per frame,
             * then clone the post-tunnelled copy.
             * Currently, the priority must be determined based on original frame,
             * before tunnelling.
             */
            if (newmaccnt > 1) {
                wbuf1 = wbuf_copy(wbuf);
                if(wbuf1 != NULL) {
                    wbuf_set_node(wbuf1, ni);    
                    wbuf_clear_flags(wbuf1);
                }
            }
        }
        
        /* In case of loop */
        if(IEEE80211_ADDR_EQ(dstmac, srcmac)) {
            goto bad;
        }
        
        /* In case the entry is an empty one, it indicates that
         * at least one STA joined the group and then left. For this
         * case, if mc_discard_mcast is enabled, this mcast frame will
         * be discarded to save the bandwidth for other ucast streaming 
         */
        if (IEEE80211_ADDR_EQ(dstmac, empty_entry_mac)) {
            if (newmaccnt > 1 || vap->iv_me->mc_discard_mcast) {   
                goto bad;
            } else {
                /*
                 * If empty entry AND not to discard the mcast frames,
                 * restore dstmac to the mcast address
                 */    
                newmaccnt = 0;
                dstmac = eh->ether_dhost;
            }
        }

        /* Look up destination */
        ni = ieee80211_find_txnode(vap, dstmac);

        /* Drop frame if dest not found in tx table */
        if (ni == NULL) {
            /* NB: ieee80211_find_txnode does stat+msg */
            if (newmaccnt > 0) {
                mgle = ieee80211_me_SnoopListFindMember(vap, grpmac, dstmac);
                if(mgle != NULL) {
                    sta_entry = ieee80211_me_SnoopListGetGroup
                        (vap, mgle->mgl_group_addr, NULL, 0xFFFF);
                    ieee80211_me_SnoopListDelEntry(vap, mgle, sta_entry);
                }
            }
            goto bad2;
        }

        /* Drop frame if dest sta not associated */
        if (ni->ni_associd == 0 && ni != vap->iv_bss) {
            /* the node hasn't been associated */

            if(ni != NULL) {
                ieee80211_free_node(ni);
            }
            
            if (newmaccnt > 0) {
                ieee80211_me_SnoopWDSNodeCleanup(ni);
            }
            goto bad;
        }

        /* calculate priority so drivers can find the tx queue */
        if (ieee80211_classify(ni, wbuf)) {
            IEEE80211_NOTE(vap, IEEE80211_MSG_IQUE, ni,
                "%s: discard, classification failure", __func__);

            /*XXX Should we del this entry for this case???*/
#if 0			
            mgle = ath_me_SnoopListFindMember(sc, grpmac, dstmac);
            if(mgle != NULL) {
                ath_me_SnoopListDelEntry(sc, mgle);
            }
#endif			
            goto bad2;
        }

        /* Insert tunnel header
         * eh is the pointer to the ethernet header of the original frame.
         * eh2 is the pointer to the ethernet header of the encapsulated frame.
         *
         */
        if (newmaccnt > 0 && vap->iv_me->mc_snoop_enable) {
            /*Option 1: Tunneling*/
            if (vap->iv_me->mc_snoop_enable & 1) {
    
                /* Encapsulation */
                tunHdr = (struct athl2p_tunnel_hdr *) wbuf_push(wbuf, sizeof(struct athl2p_tunnel_hdr));
                eh2 = (struct ether_header *) wbuf_push(wbuf, sizeof(struct ether_header));
        
                /* ATH_ETH_TYPE protocol subtype */
                tunHdr->proto = 17;
            
                /* copy original src addr */
                IEEE80211_ADDR_COPY(&eh2->ether_shost[0], srcmac);
    
                /* copy new ethertype */
                eh2->ether_type = htobe16(ATH_ETH_TYPE);

            }
            /*Option 2: Translating*/
            else if (vap->iv_me->mc_snoop_enable & 2) {
                eh2 = (struct ether_header *)wbuf_header(wbuf);
            }
            
            else {
                eh2 = (struct ether_header *)wbuf_header(wbuf);
            }

            /* copy new dest addr */
            IEEE80211_ADDR_COPY(&eh2->ether_dhost[0], &newmac[newmacidx][0]);

            /*
             *  Headline block removal: if the state machine is in
             *  BLOCKING or PROBING state, transmision of UDP data frames
             *  are blocked untill swtiches back to ACTIVE state.
             */
            if (vap->iv_ique_ops.hbr_dropblocked) {
                if (vap->iv_ique_ops.hbr_dropblocked(vap, ni, wbuf)) {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IQUE,
                                     "%s: packet dropped coz it blocks the headline\n",
                                     __func__);
                    goto bad2;
                }
            }

            mgle = ieee80211_me_SnoopListFindMember(vap, grpmac, dstmac);
            mgle->mgl_xmited ++;
            timestamp  = OS_GET_TIMESTAMP();
            mgle->mgl_timestamp = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(timestamp);

            if(vap->iv_me->me_debug & IEEE80211_ME_DBG_ALL) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IQUE, "Package from %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
                    srcmac[0], srcmac[1], srcmac[2], srcmac[3], srcmac[4], srcmac[5]);
            }
        }
        if (!ieee80211_vap_ready_is_set(vap)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT,
                              "%s: ignore data packet, vap is not active\n",
                              __func__);
            goto bad2;
        }
#ifdef IEEE80211_WDS
        if (vap->iv_opmode == IEEE80211_M_WDS)
            wbuf_set_wdsframe(wbuf);
        else
            wbuf_clear_wdsframe(wbuf);
#endif
        
        wbuf_set_node(wbuf, ni);

        /* power-save checks */
        if ((!WME_UAPSD_AC_ISDELIVERYENABLED(wbuf_get_priority(wbuf), ni)) && 
            (ieee80211node_is_paused(ni)) && 
            !ieee80211node_has_flag(ni, IEEE80211_NODE_TEMP)) {
            ieee80211node_pause(ni); 
            /* pause it to make sure that no one else unpaused it 
               after the node_is_paused check above, 
               pause operation is ref counted */
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT,
                    "%s: could not send packet, STA (%s) powersave %d paused %d\n", 
                    __func__, ether_sprintf(ni->ni_macaddr), 
                    (ni->ni_flags & IEEE80211_NODE_PWR_MGT) ?1 : 0, 
                    ieee80211node_is_paused(ni));
            ieee80211_node_saveq_queue(ni, wbuf, IEEE80211_FC0_TYPE_DATA);
            ieee80211node_unpause(ni); 
            /* unpause it if we are the last one, the frame will be flushed out */  
        }
        else
        {
            ieee80211_vap_pause_update_xmit_stats(vap,wbuf); /* update the stats for vap pause module */
            ieee80211_send_wbuf(vap, ni, wbuf);
        }

        /* ieee80211_send_wbuf will increase refcnt for frame to be sent, so decrease refcnt here for the increase by find_txnode. */
        ieee80211_free_node(ni); 

        goto loop_end;

    bad2:
        if (ni != NULL) {
            ieee80211_free_node(ni);
        } 
    bad:
        /* No point in checking if wbuf == NULL, here */ 
        wbuf_complete(wbuf);
        wbuf_set_status(wbuf, WB_STATUS_TX_ERROR);
         
        if (IEEE80211_IS_MULTICAST(dstmac))
            vap->iv_multicast_stats.ims_tx_discard++;
        else
            vap->iv_unicast_stats.ims_tx_discard++;
        
    
    loop_end:
        /* loop end */
        if (wbuf1 != NULL) {
            wbuf = wbuf1;
        }
        wbuf1 = NULL;
        newmacidx++;
        xmited ++;
    } while (--newmaccnt > 0 && vap->iv_me->mc_snoop_enable); 
    return xmited;
}

/********************************************************************
 * !
 * \brief Detach the resources for multicast enhancement
 *
 * \param sc Pointer to ATH object (this)
 *
 * \return N/A
 */
static void
ieee80211_me_detach(struct ieee80211vap *vap)
{
    if(vap->iv_me) {
        IEEE80211_ME_LOCK(vap);
        OS_CANCEL_TIMER(&vap->iv_me->snooplist_timer);	
        OS_FREE_TIMER(&vap->iv_me->snooplist_timer);	
        IEEE80211_ME_UNLOCK(vap);
        IEEE80211_ME_LOCK_DESTROY(vap);
        OS_FREE(vap->iv_me);
        vap->iv_me = NULL;
    }
}
/*******************************************************************
 * !
 * \brief Attach the ath_me module for multicast enhancement. 
 *
 * This function allocates the ath_me structure and attachs function 
 * entry points to the function table of ath_softc.
 *
 * \param  vap
 *
 * \return pointer to the mcastenhance op table (vap->iv_me_ops).
 */
int
ieee80211_me_attach(struct ieee80211vap * vap)
{
    struct ieee80211_ique_me *ame;
    union bitfield{
        u_int8_t byte;
        struct {
            u_int8_t  high:4,
                      low:4;                
        } bits;           
    } t;
    ame = (struct ieee80211_ique_me *)OS_MALLOC(
                    vap-> iv_ic->ic_osdev,       
                    sizeof(struct ieee80211_ique_me), GFP_KERNEL);
    if (ame == NULL)
            return 0;

    OS_MEMZERO(ame, sizeof(struct ieee80211_ique_me));

    /*Attach function entry points*/
    vap->iv_ique_ops.me_detach = ieee80211_me_detach;
    vap->iv_ique_ops.me_inspect = ieee80211_me_SnoopInspecting;
    vap->iv_ique_ops.me_convert = ieee80211_me_SnoopConvert;
    vap->iv_ique_ops.me_dump = ieee80211_me_SnoopListDump;
    vap->iv_ique_ops.me_clean = ieee80211_me_SnoopWDSNodeCleanup;
    vap->iv_ique_ops.me_showdeny = ieee80211_me_SnoopShowDenyTable;
    vap->iv_ique_ops.me_adddeny = ieee80211_me_SnoopAddDenyEntry;
    vap->iv_ique_ops.me_cleardeny = ieee80211_me_SnoopClearDenyTable;
    vap->iv_me = ame;
    ame->me_iv = vap;
    ame->ieee80211_me_snooplist.msl_max_length = MAX_SNOOP_ENTRIES;
    OS_INIT_TIMER(vap->iv_ic->ic_osdev, &ame->snooplist_timer, ieee80211_me_SnoopListTimer, vap);
    ame->me_timer = DEF_ME_TIMER;
    ame->me_timeout = DEF_ME_TIMEOUT;
    OS_SET_TIMER(&ame->snooplist_timer, vap->iv_me->me_timer);
    ame->mc_discard_mcast = 1;
    ame->ieee80211_me_snooplist.msl_deny_count = 2;
    ame->ieee80211_me_snooplist.msl_deny_group[0][0] = 224;
    ame->ieee80211_me_snooplist.msl_deny_group[0][1] = 0;
    ame->ieee80211_me_snooplist.msl_deny_group[0][2] = 0;
    ame->ieee80211_me_snooplist.msl_deny_group[0][3] = 1;
    ame->ieee80211_me_snooplist.msl_deny_mask[0][0] = 255;
    ame->ieee80211_me_snooplist.msl_deny_mask[0][1] = 255;
    ame->ieee80211_me_snooplist.msl_deny_mask[0][2] = 255;
    ame->ieee80211_me_snooplist.msl_deny_mask[0][3] = 0;
    ame->ieee80211_me_snooplist.msl_deny_group[1][0] = 239;
    ame->ieee80211_me_snooplist.msl_deny_group[1][1] = 255;
    ame->ieee80211_me_snooplist.msl_deny_group[1][2] = 255;
    ame->ieee80211_me_snooplist.msl_deny_group[1][3] = 1;
    ame->ieee80211_me_snooplist.msl_deny_mask[1][0] = 255;
    ame->ieee80211_me_snooplist.msl_deny_mask[1][1] = 255;
    ame->ieee80211_me_snooplist.msl_deny_mask[1][2] = 255;
    ame->ieee80211_me_snooplist.msl_deny_mask[1][3] = 0;
    IEEE80211_ME_LOCK_INIT(vap);
    ieee80211_me_SnoopListInit(vap);
    t.byte=0x12;
    if (t.bits.low == 0x2)
        bitfield_endianess = BITFIELD_BIG_ENDIAN;
    else
        bitfield_endianess = BITFIELD_LITTLE_ENDIAN;
    
    return 1;
}


#endif /* ATH_SUPPORT_IQUE */
