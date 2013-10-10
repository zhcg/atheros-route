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

#include <ieee80211_txrx_priv.h>

#include <if_llc.h>
#include <if_athproto.h>
#include <if_upperproto.h>
#include <ieee80211_aow.h>

#ifdef ATH_EXT_AP
#include <ieee80211_extap.h>
#endif

#ifdef CONFIG_RCPI
#include <if_athvar.h> /* for definition of ATH_NODE_NET80211(ni) */
#endif /* CONFIG_RCPI */
#if ATH_SW_WOW
#include <ieee80211_wow.h>
#endif
#include "ieee80211_vi_dbg.h"

typedef enum {
    FILTER_STATUS_ACCEPT = 0,
    FILTER_STATUS_REJECT
} ieee80211_privasy_filter_status;

#define IS_SNAP(_llc) ((_llc)->llc_dsap == LLC_SNAP_LSAP && \
                        (_llc)->llc_ssap == LLC_SNAP_LSAP && \
                        (_llc)->llc_control == LLC_UI)
#define RFC1042_SNAP_NOT_AARP_IPX(_llc) \
            ((_llc)->llc_snap.org_code[0] == RFC1042_SNAP_ORGCODE_0 && \
            (_llc)->llc_snap.org_code[1] == RFC1042_SNAP_ORGCODE_1 && \
            (_llc)->llc_snap.org_code[2] == RFC1042_SNAP_ORGCODE_2 \
            && !((_llc)->llc_snap.ether_type == htons(ETHERTYPE_AARP) || \
                (_llc)->llc_snap.ether_type == htons(ETHERTYPE_IPX)))
#define IS_BTEP(_llc) ((_llc)->llc_snap.org_code[0] == BTEP_SNAP_ORGCODE_0 && \
            (_llc)->llc_snap.org_code[1] == BTEP_SNAP_ORGCODE_1 && \
            (_llc)->llc_snap.org_code[2] == BTEP_SNAP_ORGCODE_2)
#define IS_ORG_BTAMP(_llc) ((_llc)->llc_snap.org_code[0] == BTAMP_SNAP_ORGCODE_0 && \
                            (_llc)->llc_snap.org_code[1] == BTAMP_SNAP_ORGCODE_1 && \
                            (_llc)->llc_snap.org_code[2] == BTAMP_SNAP_ORGCODE_2)
#define IS_ORG_AIRONET(_llc) ((_llc)->llc_snap.org_code[0] == AIRONET_SNAP_CODE_0 && \
                               (_llc)->llc_snap.org_code[1] == AIRONET_SNAP_CODE_1 && \
                               (_llc)->llc_snap.org_code[2] == AIRONET_SNAP_CODE_2)

static int ieee80211_qos_decap(struct ieee80211vap *vap, wbuf_t wbuf, int hdrlen);
static void ieee80211_input_data(struct ieee80211_node *ni, wbuf_t wbuf, 
                     struct ieee80211_rx_status *rs, int subtype, int dir);
static ieee80211_privasy_filter_status ieee80211_check_privacy_filters(struct ieee80211_node *ni, wbuf_t wbuf, int is_mcast);
static void ieee80211_deliver_data(struct ieee80211vap *vap, wbuf_t wbuf, struct ieee80211_node *ni, struct ieee80211_rx_status *rs,
                                          u_int32_t hdrspace, int is_mcast, u_int8_t subtype);
static wbuf_t ieee80211_decap(struct ieee80211vap *vap, wbuf_t wbuf, size_t hdrspace, struct ieee80211_rx_status *rs);



/*
 * check the frame against the registered  privacy flteres. 
 * returns 1 if the frame needs to be filtered out.
 * returns 0 if the frame needs to be indicated up.
 */

static ieee80211_privasy_filter_status
ieee80211_check_privacy_filters(struct ieee80211_node *ni, wbuf_t wbuf, int is_mcast)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct llc *llc;
    u_int16_t ether_type = 0;
    u_int32_t hdrspace;
    u_int32_t i;
    struct ieee80211_frame *wh;
    ieee80211_privacy_filter_packet_type packet_type;
    u_int8_t is_encrypted;

    /* Safemode must avoid the PrivacyExemptionList and ExcludeUnencrypted checking */
    if (IEEE80211_VAP_IS_SAFEMODE_ENABLED(vap)) {
        return FILTER_STATUS_ACCEPT;
    }

    wh = (struct ieee80211_frame *) wbuf_header(wbuf);

    if (IEEE80211_VAP_IS_DELIVER_80211_ENABLED(vap)) {
        /* QoS-Ctrl & Padding bytes have already been stripped off */
        hdrspace = ieee80211_hdrsize(wbuf_header(wbuf));

    } else {
        hdrspace = ieee80211_hdrspace(vap->iv_ic, wbuf_header(wbuf));
    }
    if (wbuf_get_pktlen(wbuf)  < ( hdrspace + LLC_SNAPFRAMELEN)) {
        IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_INPUT,
                            wh->i_addr2, "data",
                            "%s: too small packet 0x%x len %u hdrspace %u\n",__func__,
                              ether_type, wbuf_get_pktlen(wbuf),hdrspace);
        return FILTER_STATUS_REJECT; /* filter the packet */
    }

    llc = (struct llc *)(wbuf_header(wbuf) + hdrspace);
    if (IS_SNAP(llc) && (RFC1042_SNAP_NOT_AARP_IPX(llc) || IS_ORG_BTAMP(llc) ||
        IS_ORG_AIRONET(llc))) {
        ether_type = ntohs(llc->llc_snap.ether_type);
    } else {
        ether_type = htons(wbuf_get_pktlen(wbuf) - hdrspace);
    }

    is_encrypted = (wh->i_fc[1] & IEEE80211_FC1_WEP);
    wh->i_fc[1] &= ~IEEE80211_FC1_WEP; /* XXX: we don't need WEP bit from here */
    
    if (is_mcast) {
        packet_type = IEEE80211_PRIVACY_FILTER_PACKET_MULTICAST;
    } else {
        packet_type = IEEE80211_PRIVACY_FILTER_PACKET_UNICAST;
    }

    for (i=0; i < vap->iv_num_privacy_filters; i++) {
        /* skip if the ether type does not match */
        if (vap->iv_privacy_filters[i].ether_type != ether_type)
            continue;

        /* skip if the packet type does not match */
        if (vap->iv_privacy_filters[i].packet_type != packet_type &&
            vap->iv_privacy_filters[i].packet_type != IEEE80211_PRIVACY_FILTER_PACKET_BOTH) 
            continue;
        
        if (vap->iv_privacy_filters[i].filter_type == IEEE80211_PRIVACY_FILTER_ALLWAYS) {
            /*
             * In this case, we accept the frame if and only if it was originally
             * NOT encrypted.
             */
            if (is_encrypted) {
               IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_INPUT,
                            wh->i_addr2, "data",
                            "%s: packet encrypted ether type 0x%x len %u \n",__func__,
                            ether_type, wbuf_get_pktlen(wbuf));
                return FILTER_STATUS_REJECT;
            } else {
                return FILTER_STATUS_ACCEPT;
            }
        } else if (vap->iv_privacy_filters[i].filter_type  == IEEE80211_PRIVACY_FILTER_KEY_UNAVAILABLE) {
            /*
             * In this case, we reject the frame if it was originally NOT encrypted but 
             * we have the key mapping key for this frame.
             */
            if (!is_encrypted && !is_mcast && ni->ni_ucastkey.wk_valid) {
               IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_INPUT,
                            wh->i_addr2, "data",
                            "%s: node has a key ether type 0x%x len %u \n",__func__,
                            ether_type, wbuf_get_pktlen(wbuf));
                return FILTER_STATUS_REJECT;
            } else {
                return FILTER_STATUS_ACCEPT;
            }
        } else {
            /*
             * The privacy exemption does not apply to this frame.
             */
            break;
        }
    }

    /*
     * If the privacy exemption list does not apply to the frame, check ExcludeUnencrypted.
     * if ExcludeUnencrypted is not set, or if this was oringially an encrypted frame, 
     * it will be accepted.
     */
    if (!IEEE80211_VAP_IS_DROP_UNENC(vap) || is_encrypted) {
        /*
         * if the node is not authorized 
         * reject the frame.
         */
        if (!ieee80211_node_is_authorized(ni)) {
            IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_INPUT,
                                  wh->i_addr2, "data",
                                  "unauthorized port: ether type 0x%x len %u \n",
                                  ether_type, wbuf_get_pktlen(wbuf));
            vap->iv_stats.is_rx_unauth++;
            return FILTER_STATUS_REJECT;
        }
        return FILTER_STATUS_ACCEPT;
    }

    if (!is_encrypted && IEEE80211_VAP_IS_DROP_UNENC(vap)) {
        if (is_mcast) {
            vap->iv_multicast_stats.ims_rx_unencrypted++;
            vap->iv_multicast_stats.ims_rx_decryptcrc++;
        } else {
            vap->iv_unicast_stats.ims_rx_unencrypted++;
            vap->iv_unicast_stats.ims_rx_decryptcrc++;
        }
    }

    IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_INPUT,
                            wh->i_addr2, "data",
                          "%s: ether type 0x%x len %u \n",__func__,
                             ether_type, wbuf_get_pktlen(wbuf));
    return FILTER_STATUS_REJECT;
}

int
ieee80211_amsdu_input(struct ieee80211_node *ni, 
                      wbuf_t wbuf, struct ieee80211_rx_status *rs,
                      int is_mcast, u_int8_t subtype)
{
#define AMSDU_LLC_SIZE  (sizeof(struct ether_header) + sizeof(struct llc))
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_phy_stats *phy_stats;
    struct ieee80211_mac_stats *mac_stats;
    struct ether_header *subfrm_hdr;
    u_int32_t subfrm_len, subfrm_datalen, frm_len = 0;
    u_int32_t hdrsize;
    wbuf_t wbuf_new, wbuf_subfrm, wbuf_save = wbuf;
    struct ieee80211_qosframe_addr4 *wh;

    phy_stats = &ic->ic_phy_stats[ic->ic_curmode];
    mac_stats = IEEE80211_IS_MULTICAST(wbuf_header(wbuf)) ? &vap->iv_multicast_stats : &vap->iv_unicast_stats;

    hdrsize = ieee80211_hdrspace(ic, wbuf_header(wbuf));
    /* point to ieee80211 header */
    wh = (struct ieee80211_qosframe_addr4 *)wbuf_header(wbuf);
    wbuf_pull(wbuf, hdrsize);

    frm_len = wbuf_get_pktlen(wbuf);
    while (wbuf_next(wbuf) != NULL) {
        wbuf = wbuf_next(wbuf);
        frm_len += wbuf_get_pktlen(wbuf);
    }

    wbuf = wbuf_save;
    
    while (frm_len >= AMSDU_LLC_SIZE) {
        if ((wbuf_get_pktlen(wbuf)) < AMSDU_LLC_SIZE) { /* wbuf left too less */
            if ((wbuf_new = wbuf_next(wbuf)) == NULL) {
                IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_ANY,
                    ni->ni_macaddr, "amsdu", 
                    "A-MSDU: pullup failed, len %u", frm_len);
                goto err_amsdu;
            }
            /* check new wbuf for data length */
            else if (wbuf_get_pktlen(wbuf_new) < AMSDU_LLC_SIZE) {
                goto err_amsdu;
            }
            wbuf = wbuf_new;    /* update wbuf */
        }

        subfrm_hdr = (struct ether_header *)wbuf_header(wbuf);
        subfrm_datalen = ntohs(subfrm_hdr->ether_type);
        subfrm_len = subfrm_datalen + sizeof(struct ether_header);

        if (subfrm_len < sizeof(LLC_SNAPFRAMELEN)) {
            IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_INPUT, 
                subfrm_hdr->ether_shost, "amsdu", 
                "A-MSDU sub-frame too short: len %u", 
                subfrm_len);
            goto err_amsdu;
        }

        if (frm_len != subfrm_len && frm_len < roundup(subfrm_len, 4)) {
            IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_INPUT, 
                subfrm_hdr->ether_shost, "amsdu", "Short A-MSDU: len %u", frm_len);
            goto err_amsdu;
        }

        wbuf_subfrm = wbuf_alloc(ic->ic_osdev, 
                                 WBUF_RX_INTERNAL, 
                                 subfrm_datalen+hdrsize);

        if (wbuf_subfrm == NULL) {
            IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_INPUT, 
                subfrm_hdr->ether_shost, "amsdu", "%s", "amsdu A-MSDU no memory");
            goto err_amsdu;
        }

        wbuf_init(wbuf_subfrm, subfrm_datalen + hdrsize);

        switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
        case IEEE80211_FC1_DIR_NODS:
            IEEE80211_ADDR_COPY(wh->i_addr1, subfrm_hdr->ether_dhost);
            IEEE80211_ADDR_COPY(wh->i_addr2, subfrm_hdr->ether_shost);
            break;

        case IEEE80211_FC1_DIR_TODS:
            IEEE80211_ADDR_COPY(wh->i_addr2, subfrm_hdr->ether_shost);
            IEEE80211_ADDR_COPY(wh->i_addr3, subfrm_hdr->ether_dhost);
            break;

        case IEEE80211_FC1_DIR_FROMDS:
            IEEE80211_ADDR_COPY(wh->i_addr1, subfrm_hdr->ether_dhost);
            IEEE80211_ADDR_COPY(wh->i_addr3, subfrm_hdr->ether_shost);
            break;

        case IEEE80211_FC1_DIR_DSTODS:
            IEEE80211_ADDR_COPY(wh->i_addr3, subfrm_hdr->ether_dhost);
            IEEE80211_ADDR_COPY(wh->i_addr4, subfrm_hdr->ether_shost);
            break;

        default:
            wbuf_free(wbuf_subfrm);
            goto err_amsdu;
        }
        
        wbuf_set_priority(wbuf_subfrm, wbuf_get_priority(wbuf));
        if (wbuf_is_qosframe(wbuf)) {
            wbuf_set_qosframe(wbuf_subfrm);
        }

        OS_MEMCPY(wbuf_header(wbuf_subfrm), wh, hdrsize);
        OS_MEMCPY(wbuf_header(wbuf_subfrm) + hdrsize, wbuf_header(wbuf) + sizeof(struct ether_header), subfrm_datalen);

        /* update stats for received frames (all fragments) */
        phy_stats->ips_rx_packets++;
        if (is_mcast)
            phy_stats->ips_rx_multicast++; 
        mac_stats->ims_rx_packets++;
		mac_stats->ims_rx_bytes += subfrm_len;
        
        if (ieee80211_check_privacy_filters(ni, wbuf_subfrm, is_mcast) == FILTER_STATUS_REJECT) {
            wbuf_free(wbuf_subfrm);
        } else {
            ieee80211_deliver_data(vap, wbuf_subfrm, ni, rs, hdrsize, is_mcast, subtype);
        }

        if (frm_len > subfrm_len) {
            wbuf_pull(wbuf, roundup(subfrm_len, 4));/* point to next e_header */
            frm_len -= roundup(subfrm_len, 4); 
        } else {
            frm_len = 0;
        }
    }
	/* we count 802.11 header into our rx_bytes */
	mac_stats->ims_rx_bytes += hdrsize;

err_amsdu:
    while (wbuf_save) {
	wbuf = wbuf_next(wbuf_save);
        wbuf_free(wbuf_save);
	wbuf_save = wbuf;
    }

    return IEEE80211_FC0_TYPE_DATA;
}

static int
ieee80211_qos_decap(struct ieee80211vap *vap, wbuf_t wbuf, int hdrlen)
{
    struct ieee80211_frame *wh;
    u_int32_t hdrsize;

    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    if (IEEE80211_QOS_HAS_SEQ(wh)) {
        u_int16_t qoslen = sizeof(struct ieee80211_qoscntl);
        u_int8_t *qos;
#ifdef ATH_SUPPORT_TxBF
        /* Qos frame with Order bit set indicates an HTC frame */
        if (wh->i_fc[1] & IEEE80211_FC1_ORDER) {
            qoslen += sizeof(struct ieee80211_htc);
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,"==>%s: receive +HTC frame\n",__func__);
        }
#endif
        if (vap->iv_ic->ic_flags & IEEE80211_F_DATAPAD) {
            hdrsize = ieee80211_hdrsize(wh);

            /* If the frame has padding, include padding length as well */
            if ((hdrsize % sizeof(u_int32_t) != 0)) {
                qoslen = roundup(qoslen, sizeof(u_int32_t));
            }
        }

        if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS) {
            qos = &((struct ieee80211_qosframe_addr4 *)wh)->i_qos[0];
        } else {
            qos = &((struct ieee80211_qosframe *)wh)->i_qos[0];
        }

        /* save priority */
        wbuf_set_qosframe(wbuf);
        wbuf_set_priority(wbuf, (qos[0] & IEEE80211_QOS_TID));

        if (vap->iv_ccx_evtable && vap->iv_ccx_evtable->wlan_ccx_process_qos) {
            vap->iv_ccx_evtable->wlan_ccx_process_qos(vap->iv_ccx_arg, IEEE80211_RX_QOS_FRAME, (qos[0] & IEEE80211_QOS_TID));
        }

        /* remove QoS filed from header */
        hdrlen -= qoslen;
        memmove((u_int8_t *)wh + qoslen, wh, hdrlen);
        wh = (struct ieee80211_frame *)wbuf_pull(wbuf, qoslen);

        /* clear QoS bit */
        wh->i_fc[0] &= ~IEEE80211_FC0_SUBTYPE_QOS;

    } else {
        u_int16_t padlen = 0;

        /* Non-Qos Frames, remove padding if any */
        if (vap->iv_ic->ic_flags & IEEE80211_F_DATAPAD) {
            hdrsize = ieee80211_hdrsize(wh);
            padlen = roundup((hdrsize), sizeof(u_int32_t)) - hdrsize;
        }

        if (padlen) {
            /* remove padding from header */
            hdrlen -= padlen;
            memmove((u_int8_t *)wh + padlen, wh, hdrlen);
            wh = (struct ieee80211_frame *)wbuf_pull(wbuf, padlen);
        }
    }

    return 0;
}

static INLINE int
ieee80211_is_mcastecho(struct ieee80211vap *vap, struct ieee80211_frame *wh)
{
    u_int8_t *sender;
    u_int8_t dir, subtype;
    int mcastecho = 0;

    KASSERT(vap->iv_opmode == IEEE80211_M_STA, ("!sta mode"));

    dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
    subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
    
    if (dir == IEEE80211_FC1_DIR_DSTODS) {
        if (subtype == IEEE80211_FC0_SUBTYPE_QOS) {
            struct ieee80211_qosframe_addr4 *wh4q = (struct ieee80211_qosframe_addr4 *)wh;

#ifdef IEEE80211_DEBUG_CHATTY 
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS, "*** MCAST QoS 4 addr *** "
                              "ADDR1 %s, ADDR2 %s, ADDR3 %s, ADDR4 %s\n",
                              ether_sprintf(wh4q->i_addr1),
                              ether_sprintf(wh4q->i_addr2),
                              ether_sprintf(wh4q->i_addr3),
                              ether_sprintf(wh4q->i_addr4));
#endif
            sender = wh4q->i_addr4;
           } else {
            struct ieee80211_frame_addr4 *wh4 = (struct ieee80211_frame_addr4 *)wh;

#ifdef IEEE80211_DEBUG_CHATTY 
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS, "*** MCAST 4 addr *** "
                              "ADDR1 %s, ADDR2 %s, ADDR3 %s, ADDR4 %s\n",
                              ether_sprintf(wh4->i_addr1),
                              ether_sprintf(wh4->i_addr2),
                              ether_sprintf(wh4->i_addr3),
                              ether_sprintf(wh4->i_addr4));
#endif
            sender = wh4->i_addr4;
           }
    } else {
#ifdef IEEE80211_DEBUG_CHATTY 
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS, "*** MCAST 3 addr  *** "
                          "ADDR1 %s, ADDR2 %s, ADDR3 %s\n",
                          ether_sprintf(wh->i_addr1),
                          ether_sprintf(wh->i_addr2),
                          ether_sprintf(wh->i_addr3));
#endif
        sender = wh->i_addr3;
    }

    
    if (IEEE80211_ADDR_EQ(sender, vap->iv_myaddr)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS, 
                          "*** multicast echo *** sender address equals own address (%s) \n",
                          ether_sprintf(sender));
        mcastecho = 1;
        goto done;
    }
    /*
     * if it is brodcasted by me on behalf of
     * a station behind me, drop it.
     *
     */
    if (IEEE80211_VAP_IS_WDS_ENABLED(vap)) mcastecho = wds_sta_chkmcecho(&(vap->iv_ic->ic_sta), sender);
done:
    return mcastecho;
}

 /*
  * processes data frames.
  * ieee80211_input_data consumes the wbuf .
  */
static void 
ieee80211_input_data(struct ieee80211_node *ni, wbuf_t wbuf, struct ieee80211_rx_status *rs, int subtype, int dir)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211_phy_stats *phy_stats;
    struct ieee80211_mac_stats *mac_stats;
    struct ieee80211_frame *wh;
    struct ieee80211_key *key;
    u_int16_t hdrspace;
    int is_mcast, is_amsdu = 0, is_bcast;
    struct ieee80211_node *ni_wds=NULL;
    struct ieee80211_node *temp_node=NULL;

    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    is_mcast = (dir == IEEE80211_FC1_DIR_DSTODS ||
                dir == IEEE80211_FC1_DIR_TODS ) ?
        IEEE80211_IS_MULTICAST(IEEE80211_WH4(wh)->i_addr3) :
        IEEE80211_IS_MULTICAST(wh->i_addr1);
    is_bcast = (dir == IEEE80211_FC1_DIR_FROMDS) ? IEEE80211_IS_BROADCAST(wh->i_addr1) : FALSE;

    phy_stats = &ic->ic_phy_stats[ic->ic_curmode];
    mac_stats = is_mcast ? &vap->iv_multicast_stats : &vap->iv_unicast_stats;

    hdrspace = ieee80211_hdrspace(ic, wbuf_header(wbuf));
    if (wbuf_get_pktlen(wbuf) < hdrspace) {
        mac_stats->ims_rx_discard++;
        IEEE80211_DISCARD(vap,
                          IEEE80211_MSG_INPUT, wh,
                          "data",
                          "too small len %d ", wbuf_get_pktlen(wbuf));
        goto bad;
    }
    
#if ATH_WDS_WAR_UNENCRYPTED
        /* Owl 2.2 WDS War for Non-Encrypted 4 Addr QoS Frames - Extra QoS Ctl field */
        if ((dir == IEEE80211_FC1_DIR_DSTODS) && IEEE80211_NODE_USEWDSWAR(ni) &&
            ((wh->i_fc[1] & IEEE80211_FC1_WEP) == 0) &&
            (subtype == IEEE80211_FC0_SUBTYPE_QOS)) {
            u_int8_t *header = (u_int8_t *)wh;
            int realhdrsize = ieee80211_hdrsize(wh); /* no _F_DATAPAD */
            int padsize = hdrspace - realhdrsize;

            if (padsize == 0) {
                /* IEEE80211_F_DATAPAD had no local effect,
                 * but we need to account for remote */
                padsize += roundup(sizeof(struct ieee80211_qoscntl), sizeof(u_int32_t));
            }

            if (padsize > 0) {
                memmove(header+padsize, header, realhdrsize);
                wbuf_pull(wbuf, padsize);
                /* data ptr moved */
                wh = (struct ieee80211_frame *)wbuf_header(wbuf);
            }
        }
#endif

    vap->iv_lastdata = OS_GET_TIMESTAMP();
    phy_stats->ips_rx_fragments++;
    vap->iv_txrxbytes += wbuf_get_pktlen(wbuf);

    /* 
     * Store timestamp for actual (non-NULL) data frames.
     * This provides other modules such as SCAN and LED with correct 
     * information about the actual data traffic in the system.
     * We don't take broadcast traffic into consideration.
     */
    if (!is_bcast && IEEE80211_CONTAIN_DATA(subtype)) {
        vap->iv_last_traffic_indication = OS_GET_TIMESTAMP();
    }

    switch (vap->iv_opmode) {
    case IEEE80211_M_STA:
        /* 
         * allow all frames with FromDS bit set .
         * allow Frames with DStoDS if the vap is WDS capable.
         */
        if (!(dir == IEEE80211_FC1_DIR_FROMDS || IEEE80211_IS_TDLS_NODE(ni) ||
              (dir == IEEE80211_FC1_DIR_DSTODS && (IEEE80211_VAP_IS_WDS_ENABLED(vap))))) {
            if (dir == IEEE80211_FC1_DIR_DSTODS) {
                IEEE80211_DISCARD(vap,
                                  IEEE80211_MSG_INPUT |
                                  IEEE80211_MSG_WDS, wh,
                                  "4-address data",
                                  "%s", "WDS not enabled");
                vap->iv_stats.is_rx_nowds++;
            } else {
                IEEE80211_DISCARD(vap,
                                  IEEE80211_MSG_INPUT, wh,
                                  "data",
                                  "invalid dir 0x%x", dir);
                vap->iv_stats.is_rx_wrongdir++;
            }
            mac_stats->ims_rx_discard++;
            goto bad;
        }
            
        /*
         * In IEEE802.11 network, multicast packet
         * sent from me is broadcasted from AP.
         * It should be silently discarded.
         */
        if (is_mcast && ieee80211_is_mcastecho(vap, wh)) {
            IEEE80211_DISCARD(vap, IEEE80211_MSG_INPUT |
                              IEEE80211_MSG_WDS,
                              wh, "data", "multicast echo dir 0x%x", dir);
            vap->iv_stats.is_rx_mcastecho++;
            goto bad;
        }

        if (is_mcast) {
            /* Report last multicast/broadcast frame to Power Save Module */
            if (!(wh->i_fc[1] & IEEE80211_FC1_MORE_DATA)) {
                if (vap->iv_txrx_event_info.iv_txrx_event_filter & IEEE80211_VAP_INPUT_EVENT_LAST_MCAST) {
                    ieee80211_vap_txrx_event evt;
                    evt.type = IEEE80211_VAP_INPUT_EVENT_LAST_MCAST;
                    ieee80211_vap_txrx_deliver_event(vap,&evt);
                }
            }
        } else {
            /* Report unicast frames to Power Save Module */
            if (vap->iv_txrx_event_info.iv_txrx_event_filter & (IEEE80211_VAP_INPUT_EVENT_UCAST | IEEE80211_VAP_INPUT_EVENT_EOSP)) {
                if (vap->iv_txrx_event_info.iv_txrx_event_filter & IEEE80211_VAP_INPUT_EVENT_UCAST) {
                    ieee80211_vap_txrx_event evt;
                    evt.type = IEEE80211_VAP_INPUT_EVENT_UCAST;
                    evt.u.more_data = wh->i_fc[1] & IEEE80211_FC1_MORE_DATA;
                    ieee80211_vap_txrx_deliver_event(vap,&evt);
                } 
                if (vap->iv_txrx_event_info.iv_txrx_event_filter & IEEE80211_VAP_INPUT_EVENT_EOSP && 
                    ( subtype == IEEE80211_FC0_SUBTYPE_QOS || subtype == IEEE80211_FC0_SUBTYPE_QOS_NULL)) {
                    if(((struct ieee80211_qosframe *)wh)->i_qos[0] & IEEE80211_QOS_EOSP) {
                        ieee80211_vap_txrx_event evt;
                        evt.type = IEEE80211_VAP_INPUT_EVENT_EOSP;
                        ieee80211_vap_txrx_deliver_event(vap,&evt);
                    }
                } 
            }
        }

	if (dir == IEEE80211_FC1_DIR_DSTODS){
	    struct ieee80211_node_table *nt;
	    struct ieee80211_frame_addr4 *wh4;
	    wh4 = (struct ieee80211_frame_addr4 *) wbuf_header(wbuf);
	    nt = &ic->ic_sta;
	    ni_wds = ieee80211_find_wds_node(nt, wh4->i_addr4);

	    if (ni_wds == NULL)
                {
		    /*
		     * In STA mode, add wds entries for hosts behind us, but
		     * not for hosts behind the rootap.
		     */
		    if (!IEEE80211_ADDR_EQ(wh4->i_addr2, vap->iv_bss->ni_bssid))
			{
			    temp_node = ieee80211_find_node(nt,wh4->i_addr4);
			    if (temp_node == NULL)
				{
					ieee80211_add_wds_addr(nt, ni, wh4->i_addr4, 
                                        IEEE80211_NODE_F_WDS_REMOTE);
				}
			    else if (!IEEE80211_ADDR_EQ(temp_node->ni_macaddr, vap->iv_myaddr))
				{
				    ieee80211_node_leave(temp_node);
				    ieee80211_free_node(temp_node);
				    ieee80211_add_wds_addr(nt, ni, wh4->i_addr4,
                                    IEEE80211_NODE_F_WDS_REMOTE);
				}
			}
                }
	    else
                {
                    ieee80211_remove_wds_addr(nt,wh4->i_addr4,
                             IEEE80211_NODE_F_WDS_BEHIND|IEEE80211_NODE_F_WDS_REMOTE);
		    ieee80211_free_node(ni_wds);
                }
	}

        break;

    case IEEE80211_M_IBSS:
        if (dir != IEEE80211_FC1_DIR_NODS) {
            mac_stats->ims_rx_discard++;
            goto bad;
        }

        /*
         * If it is a data frame from a peer, also update the receive 
         * time stamp. This can reduce false beacon miss detection.
         */
        ni->ni_beacon_rstamp = OS_GET_TIMESTAMP();
        ni->ni_probe_ticks   = 0;
        break;

    case IEEE80211_M_HOSTAP:
        /* 
         * allow all frames with ToDS bit set .
         * allow Frames with DStoDS if the vap is WDS capable.
         */
        if (!((dir == IEEE80211_FC1_DIR_TODS) || 
              (dir == IEEE80211_FC1_DIR_DSTODS && (IEEE80211_VAP_IS_WDS_ENABLED(vap))))) {
            if (dir == IEEE80211_FC1_DIR_DSTODS) {
                IEEE80211_DISCARD(vap,
                                  IEEE80211_MSG_INPUT |
                                  IEEE80211_MSG_WDS, wh,
                                  "4-address data",
                                  "%s", "WDS not enabled");
                vap->iv_stats.is_rx_nowds++;
            } else {
                IEEE80211_DISCARD(vap,
                                  IEEE80211_MSG_INPUT, wh,
                                  "data",
                                  "invalid dir 0x%x", dir);
                vap->iv_stats.is_rx_wrongdir++;
            }
            goto bad;
        }
        
        /* if NAWDS learning feature is enabled, add the mac to NAWDS table */
        if ((ni == vap->iv_bss) &&
            (dir == IEEE80211_FC1_DIR_DSTODS) &&
            (ieee80211_nawds_enable_learning(vap))) {
            ieee80211_nawds_learn(vap, wh->i_addr2);
            /* current node is bss node so drop it to avoid sending dis-assoc. packet */
            goto bad;
        }

        /* check if source STA is associated */
        if (ni == vap->iv_bss) {
            IEEE80211_DISCARD(vap, IEEE80211_MSG_INPUT,
                              wh, "data", "%s", "unknown src");
            
            /* NB: caller deals with reference */
            if (ieee80211_vap_ready_is_set(vap)) {
                ni = ieee80211_tmp_node(vap, wh->i_addr2);
                if (ni != NULL) {
                    ieee80211_send_deauth(ni, IEEE80211_REASON_NOT_AUTHED);

                    /* claim node immediately */
                    ieee80211_free_node(ni);
                }
            }
            vap->iv_stats.is_rx_notassoc++;
            goto bad;
        }

        if (ni->ni_associd == 0) {
            IEEE80211_DISCARD(vap, IEEE80211_MSG_INPUT,
                              wh, "data", "%s", "unassoc src");
            ieee80211_send_disassoc(ni, IEEE80211_REASON_NOT_ASSOCED);
            vap->iv_stats.is_rx_notassoc++;
            goto bad;
        }
	

	/* If we're a 4 address packet, make sure we have an entry in
            the node table for the packet source address (addr4).  If not,
            add one */
	if (dir == IEEE80211_FC1_DIR_DSTODS){
	    wds_update_rootwds_table(ni,&ic->ic_sta, wbuf);
	}


#ifdef IEEE80211_DWDS
        /*
         * For 4-address packets handle WDS discovery
         * notifications.  Once a WDS link is setup frames
         * are just delivered to the WDS vap (see below).
         */
        if (dir == IEEE80211_FC1_DIR_DSTODS &&
            ni->ni_wdsvap == NULL) {
            if (!ieee80211_node_is_authorized(ni)) {
                IEEE80211_DISCARD(vap,
                                  IEEE80211_MSG_INPUT |
                                  IEEE80211_MSG_WDS, wh,
                                  "4-address data",
                                  "%s", "unauthorized port");
                vap->iv_stats.is_rx_unauth++;
                IEEE80211_NODE_STAT(ni, rx_unauth);
                goto err;
            }
            ieee80211_wds_discover(ni, m);
            goto bad;
        }
#endif
        break;

    case IEEE80211_M_WDS:
        if (dir != IEEE80211_FC1_DIR_DSTODS) {
            IEEE80211_DISCARD(vap, IEEE80211_MSG_ANY,
                              wh, "data", "invalid dir 0x%x", dir);
            vap->iv_stats.is_rx_wrongdir++;
            goto bad;
        }
#ifdef IEEE80211_WDSLEGACY
        if ((vap->iv_flags_ext & IEEE80211_FEXT_WDSLEGACY) == 0) {
            IEEE80211_DISCARD(vap, IEEE80211_MSG_ANY,
                              wh, "data", "not legacy wds, flags 0x%x",
                              vap->iv_flags_ext);
            vap->iv_stats.is_rx_nowds++;    /* XXX */
            goto bad;
        }
#endif
        break;

    default:
        break;
    }

    /*
     *  Safemode prevents us from calling decap.
     */
    if (!IEEE80211_VAP_IS_SAFEMODE_ENABLED(vap) &&
        (wh->i_fc[1] & IEEE80211_FC1_WEP)) {
        key = ieee80211_crypto_decap(ni, wbuf, hdrspace, rs);
        if (key == NULL) {
            mac_stats->ims_rx_decryptcrc++;
            IEEE80211_DISCARD (vap, IEEE80211_MSG_INPUT, wh,
                          "key is" , "%s"," null");
            goto bad;
        } else {
            mac_stats->ims_rx_decryptok++;
        }

        wh = (struct ieee80211_frame *) wbuf_header(wbuf);
        /* NB: We clear the Protected bit later */
    } else {
        key = NULL;
    }

    /*
     * Next up, any defragmentation. A list of wbuf will be returned.
     * However, do not defrag when in safe mode.
     */
    if (!IEEE80211_VAP_IS_SAFEMODE_ENABLED(vap) && !is_mcast) {
        wbuf = ieee80211_defrag(ni, wbuf, hdrspace);
        if (wbuf == NULL) {
            /* Fragment dropped or frame not complete yet */
            IEEE80211_DISCARD(vap,
                          IEEE80211_MSG_INPUT, wh,
                          "defarg","%s",
                          "failed");
            goto out;
        }
    }

    if (subtype == IEEE80211_FC0_SUBTYPE_QOS) {
        is_amsdu = (dir != IEEE80211_FC1_DIR_DSTODS) ?
            (((struct ieee80211_qosframe *)wh)->i_qos[0] & IEEE80211_QOS_AMSDU) :
            (((struct ieee80211_qosframe_addr4 *)wh)->i_qos[0] & IEEE80211_QOS_AMSDU);
    }

    /*
     * Next strip any MSDU crypto bits.
     */
    ASSERT(!IEEE80211_VAP_IS_SAFEMODE_ENABLED(vap) || (key == NULL));
    if ((key != NULL) && !ieee80211_crypto_demic(vap, key, wbuf, hdrspace, 0, rs)) {
        IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_INPUT,
                              ni->ni_macaddr, "data", "%s", "demic error");
        /* IEEE80211_NODE_STAT(ni, rx_demicfail); */
         IEEE80211_DISCARD(vap,
                          IEEE80211_MSG_INPUT, wh,
                          "demic","%s",
                          "failed");
        goto bad;
    }

    /*
     * decapsulate the QoS header if the OS asks us to deliver standard 802.11
     * headers. if OS does not want us to deliver 802.11 header then it wants us
     * to deliver ethernet header in which case the qos header will be decapped
     * along with 802.11 header ieee80211_decap function.
     */
    if (IEEE80211_VAP_IS_DELIVER_80211_ENABLED(vap) &&
        ieee80211_qos_decap(vap, wbuf, hdrspace)) {
        IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_INPUT,
                              ni->ni_macaddr, "data", "%s", "decap error");
        vap->iv_stats.is_rx_decap++;
        goto bad;
    }

    if ((subtype == IEEE80211_FC0_SUBTYPE_QOS_NULL) || (subtype == IEEE80211_FC0_SUBTYPE_NODATA)) {
        /* no need to process the null data frames any further */
        goto bad;
    }
#if ATH_RXBUF_RECYCLE
	if (is_mcast || is_bcast) {
		wbuf_set_cloned(wbuf);
	} else {
 		wbuf_clear_cloned(wbuf);
	}
#endif
    if (!is_amsdu) {
        if (ieee80211_check_privacy_filters(ni, wbuf, is_mcast) == FILTER_STATUS_REJECT) {
             IEEE80211_DISCARD_MAC(vap,
                                   IEEE80211_MSG_INPUT, wh->i_addr2, "data",
                              "privacy filter check","%s \n",
                              "failed");
            goto bad;
        }
    } else {
        ieee80211_amsdu_input(ni, wbuf, rs, is_mcast, subtype);
        goto out;
    }

    /* update stats for received frames (all fragments) */
    phy_stats->ips_rx_packets++;
    if (is_mcast)
        phy_stats->ips_rx_multicast++;
    mac_stats->ims_rx_packets++;
	mac_stats->ims_rx_bytes += wbuf_get_pktlen(wbuf);

    /* consumes the wbuf */
    ieee80211_deliver_data(vap, wbuf, ni, rs, hdrspace, is_mcast, subtype);

out:
    return;

bad:
/*  FIX ME: linux specific netdev struct iv_destats has to be replaced*/
//    vap->iv_devstats.rx_errors++; 
    wbuf_free(wbuf);
}

/*
 * Process a received frame.  The node associated with the sender
 * should be supplied.  If nothing was found in the node table then
 * the caller is assumed to supply a reference to iv_bss instead.
 * The RSSI and a timestamp are also supplied.  The RSSI data is used
 * during AP scanning to select a AP to associate with; it can have
 * any units so long as values have consistent units and higher values
 * mean ``better signal''.  
 */
int
ieee80211_input(struct ieee80211_node *ni, wbuf_t wbuf, struct ieee80211_rx_status *rs)
{
#define QOS_NULL   (IEEE80211_FC0_TYPE_DATA | IEEE80211_FC0_SUBTYPE_QOS_NULL)
#define HAS_SEQ(type, subtype)   (((type & 0x4) == 0) && ((type | subtype) != QOS_NULL))
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211_frame *wh;
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_phy_stats *phy_stats;
    struct ieee80211_mac_stats *mac_stats;
    struct ieee80211_stats *vap_stats;
    int type = -1, subtype, dir;
    u_int16_t rxseq;
    u_int8_t *bssid;

    KASSERT((wbuf_get_pktlen(wbuf) >= ic->ic_minframesize),
            ("frame length too short: %u", wbuf_get_pktlen(wbuf)));

    wbuf_set_node(wbuf, ni);


    if (wbuf_get_pktlen(wbuf) < ic->ic_minframesize) {
        goto bad1;
    }

    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    if ((wh->i_fc[0] & IEEE80211_FC0_VERSION_MASK) != IEEE80211_FC0_VERSION_0) {
        /* XXX: no stats for it. */
        goto bad1;
    }

    type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
    dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;


    if (OS_ATOMIC_CMPXCHG(&vap->iv_rx_gate, 0, 1) != 0) {
        goto bad1;
    }

    if (!ieee80211_vap_active_is_set(vap)) {
        if (vap->iv_input_mgmt_filter && type == IEEE80211_FC0_TYPE_MGT && IEEE80211_ADDR_EQ(wh->i_addr1, vap->iv_myaddr) ) {
              vap->iv_input_mgmt_filter(ni,wbuf,subtype,rs) ;
        } 
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "vap in not active, %s \n", "discard the frame");
        goto bad;
    }
    /* Mark node as WDS */
    if (dir == IEEE80211_FC1_DIR_DSTODS)
	    ni->ni_flags |= IEEE80211_NODE_WDS;

    /*
     * deliver 802.11 frame if the OS is interested in it.
     * if os returns a non zero value, drop the frame .
     */
    if (vap->iv_evtable && vap->iv_evtable->wlan_receive_filter_80211) {
        if (vap->iv_evtable->wlan_receive_filter_80211(vap->iv_ifp, wbuf, type, subtype, rs)) {
            goto bad;
        }
    }

    phy_stats = &ic->ic_phy_stats[ic->ic_curmode];
    mac_stats = IEEE80211_IS_MULTICAST(wh->i_addr1) ? &vap->iv_multicast_stats : &vap->iv_unicast_stats;
    vap_stats = &vap->iv_stats;

    /*
     * XXX Validate received frame if we're not scanning.
     * why do we receive only data frames when we are scanning and 
     * current (foreign channel) channel is the bss channel ?
     * should we simplify this to if (vap->iv_bsschan == ic->ic_curchan) ?
     */
    if ((ieee80211_scan_in_home_channel(ic->ic_scanner)) ||
        ((vap->iv_bsschan == ic->ic_curchan) && (type == IEEE80211_FC0_TYPE_DATA))) {
        switch (vap->iv_opmode) {
        case IEEE80211_M_STA:
            bssid = wh->i_addr2;
            if (ni != vap->iv_bss && dir != IEEE80211_FC1_DIR_DSTODS)
                bssid = wh->i_addr3; 

            if (!IEEE80211_TDLS_ENABLED(vap) &&
                !IEEE80211_ADDR_EQ(bssid, ieee80211_node_get_bssid(ni))) {
                /*
                 * If this is a beacon from APs other than the one we are connected, update scan entry
                 * to have fast roaming response in case we need to. Note, this only helps when both APs
                 * are in the same channel.
                 */
                if ((type == IEEE80211_FC0_TYPE_MGT) && 
                    ((subtype == IEEE80211_FC0_SUBTYPE_BEACON) ||
                     (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP))) {
                    ieee80211_update_beacon(ni, wbuf, wh, subtype, rs);
                }
                
                mac_stats->ims_rx_discard++;
                goto bad;
            }

            /*
             * WAR for excessive beacon miss on SoC.
             * Reset bmiss counter when we receive a non-probe request
             * frame from our home AP, and save the time stamp.
             */
             /* If phy mode mismatches between scan entry and VAP
                beacon miss reset has to be skipped */
            if ((ieee80211_vap_ready_is_set(vap)) &&
                (!((type == IEEE80211_FC0_TYPE_MGT) &&
                   (subtype == IEEE80211_FC0_SUBTYPE_PROBE_REQ)))&&
                   (vap->iv_lastbcn_phymode_mismatch == 0)) {

                if (vap->iv_bmiss_count > 0) {
#ifdef ATH_SWRETRY                     
                    /* Turning on the sw retry mechanism. This should not
                     * produce issues even if we are in the middle of 
                     * cleaning sw retried frames
                     */
                    ic->ic_set_swretrystate(vap->iv_bss, TRUE);
#endif                    
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
                                      "clear beacon miss. frm type=%02x, subtype=%02x\n",
                                      type, subtype);
                    ieee80211_mlme_reset_bmiss(vap);    
                }

                /* 
                 * Beacon timestamp will be set when beacon is processed.
                 * Set directed frame timestamp if frame is not multicast or
                 * broadcast.
                 */
                if (! IEEE80211_IS_MULTICAST(wh->i_addr1)) {
                    vap->iv_last_directed_frame = OS_GET_TIMESTAMP();
                }
            }
            break;

        case IEEE80211_M_IBSS:
        case IEEE80211_M_AHDEMO:
            if (dir != IEEE80211_FC1_DIR_NODS)
                bssid = wh->i_addr1;
            else if (type == IEEE80211_FC0_TYPE_CTL)
                bssid = wh->i_addr1;
            else {
                if (wbuf_get_pktlen(wbuf) < sizeof(struct ieee80211_frame)) {
                    mac_stats->ims_rx_discard++;
                    goto bad;
                }
                bssid = wh->i_addr3;
            }

            if (type != IEEE80211_FC0_TYPE_DATA)
                break;

            /*
             * Data frame, validate the bssid.
             */
            if (!IEEE80211_ADDR_EQ(bssid, ieee80211_node_get_bssid(vap->iv_bss)) &&
                !IEEE80211_ADDR_EQ(bssid, IEEE80211_GET_BCAST_ADDR(ic)) &&
                subtype != IEEE80211_FC0_SUBTYPE_BEACON) {
                /* not interested in */
                mac_stats->ims_rx_discard++;
                goto bad;
            }
            break;

        case IEEE80211_M_HOSTAP:
            if (dir != IEEE80211_FC1_DIR_NODS)
                bssid = wh->i_addr1;
            else if (type == IEEE80211_FC0_TYPE_CTL)
                bssid = wh->i_addr1;
            else {
                if (wbuf_get_pktlen(wbuf) < sizeof(struct ieee80211_frame)) {
                    mac_stats->ims_rx_discard++;
                    goto bad;
                }
                bssid = wh->i_addr3;
            }
            if (type != IEEE80211_FC0_TYPE_DATA)
                break;
            /*
             * Data frame, validate the bssid.
             */
            if (!IEEE80211_ADDR_EQ(bssid, ieee80211_node_get_bssid(vap->iv_bss)) &&
                !IEEE80211_ADDR_EQ(bssid, IEEE80211_GET_BCAST_ADDR(ic)) &&
                subtype != IEEE80211_FC0_SUBTYPE_BEACON) {
                /*
                 * NAWDS repeaters send ADDBA with the BSSID 
                 * set to their BSSID and not ours 
                 * We should not drop those frames
                 * However these are control frames with 
                 * type = IEEE80211_FC0_TYPE_MGT &
                 * subtype = IEEE80211_FC0_SUBTYPE_ACTION
                 * So will not reach here.
                 * Any packet reaching here, not passing the bssid
                 * validation needs to be dropped.
                 */
                 mac_stats->ims_rx_discard++;
                 goto bad;
            }
            break;

        case IEEE80211_M_WDS:
            if (wbuf_get_pktlen(wbuf) < sizeof(struct ieee80211_frame_addr4)) {
                if ((type != IEEE80211_FC0_TYPE_MGT) && 
                    (subtype != IEEE80211_FC0_SUBTYPE_DEAUTH)) {
                    vap_stats->is_rx_tooshort++;
                    goto bad;
                }
            }
            bssid = wh->i_addr1;
            if (!IEEE80211_ADDR_EQ(bssid, vap->iv_bss->ni_bssid) &&
                !IEEE80211_ADDR_EQ(bssid, IEEE80211_GET_BCAST_ADDR(ic))) {
                /* not interested in */
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "*** WDS *** "
                                  "WDS ADDR1 %s, BSSID %s\n",
                                  ether_sprintf(bssid),
                                  ether_sprintf(ieee80211_node_get_bssid(vap->iv_bss)));
                vap_stats->is_rx_wrongbss++;
                goto bad; 
            }
            break;
            
        case IEEE80211_M_BTAMP:
            break;
            
        default:
            goto bad;
        }

        ni->ni_rssi = rs->rs_rssi;

        /* Check duplicates */
        if (HAS_SEQ(type, subtype)) {
            u_int8_t tid;
            if (IEEE80211_QOS_HAS_SEQ(wh)) {
                if (dir == IEEE80211_FC1_DIR_DSTODS) {
                    tid = ((struct ieee80211_qosframe_addr4 *)wh)->
                        i_qos[0] & IEEE80211_QOS_TID;
                } else {
                    tid = ((struct ieee80211_qosframe *)wh)->
                        i_qos[0] & IEEE80211_QOS_TID;
                }
                if (TID_TO_WME_AC(tid) >= WME_AC_VI)
                    ic->ic_wme.wme_hipri_traffic++;
            } else {
                if (type == IEEE80211_FC0_TYPE_MGT)
                    tid = IEEE80211_TID_SIZE; /* use different pool for rx mgt seq number */
                else
                    tid = IEEE80211_NON_QOS_SEQ;
            }

            rxseq = le16toh(*(u_int16_t *)wh->i_seq);
            if ((wh->i_fc[1] & IEEE80211_FC1_RETRY) &&
                (rxseq == ni->ni_rxseqs[tid])) {
                phy_stats->ips_rx_dup++;
                
                if (ni->ni_last_rxseqs[tid] == ni->ni_rxseqs[tid]) {
                    phy_stats->ips_rx_mdup++;
                }
                ni->ni_last_rxseqs[tid] = ni->ni_rxseqs[tid];
                goto bad;
            }
            ni->ni_rxseqs[tid] = rxseq;

#if ATH_SUPPORT_AOW
            rs->rs_rxseq = rxseq >> IEEE80211_SEQ_SEQ_SHIFT;
#endif
        }
    }

    /*
     * Check for power save state change.
     */
    if ((vap->iv_opmode == IEEE80211_M_HOSTAP) && 
        (ni != vap->iv_bss) &&
        !(type == IEEE80211_FC0_TYPE_MGT && subtype == IEEE80211_FC0_SUBTYPE_PROBE_REQ))
    {
        if ((wh->i_fc[1] & IEEE80211_FC1_PWR_MGT) ^
            (ni->ni_flags & IEEE80211_NODE_PWR_MGT)) {
            ic->ic_node_psupdate(ni, wh->i_fc[1] & IEEE80211_FC1_PWR_MGT, 0);
            ieee80211_mlme_node_pwrsave(ni, wh->i_fc[1] & IEEE80211_FC1_PWR_MGT);
        }
    }

    if (type == IEEE80211_FC0_TYPE_DATA) {
        if (!ieee80211_vap_ready_is_set(vap)) {
            goto bad;
        }
        /* ieee80211_input_data consumes the wbuf */
        ieee80211_node_activity(ni); /* node has activity */
#if ATH_SW_WOW
        if (wlan_get_wow(vap)) {
            ieee80211_wow_magic_parser(ni, wbuf);
        }
#endif
        ieee80211_input_data(ni, wbuf, rs, subtype, dir); 
    } else if (type == IEEE80211_FC0_TYPE_MGT) {
        /* ieee80211_recv_mgmt does not consume the wbuf */
        if (subtype != IEEE80211_FC0_SUBTYPE_PROBE_REQ) {
            ieee80211_node_activity(ni); /* node has activity */
        }
        if (vap->iv_input_mgmt_filter == NULL || 
            (vap->iv_input_mgmt_filter && vap->iv_input_mgmt_filter(ni,wbuf,subtype,rs) == 0)) {
	        /*
             * if no filter function is installed (0)
             * if not filtered by the filter function
             * then process  management frame.
             */
            ieee80211_recv_mgmt(ni, wbuf, subtype, rs);
        }
        /*
         * deliver the frame to the os. the handler cosumes the wbuf.
         */
        if (vap->iv_evtable) {
            vap->iv_evtable->wlan_receive(vap->iv_ifp, wbuf, type, subtype, rs);
        }
    } else if (type == IEEE80211_FC0_TYPE_CTL) {
        vap->iv_stats.is_rx_ctl++;
        ieee80211_recv_ctrl(ni, wbuf, subtype, rs);
        /*
         * deliver the frame to the os. the handler cosumes the wbuf.
         */
        if (vap->iv_evtable) {
            vap->iv_evtable->wlan_receive(vap->iv_ifp, wbuf, type, subtype, rs);
        }
    } else {
        goto bad;
    }

    (void) OS_ATOMIC_CMPXCHG(&vap->iv_rx_gate, 1, 0);
    return type;

bad:
    (void) OS_ATOMIC_CMPXCHG(&vap->iv_rx_gate, 1, 0);
bad1:
    wbuf_free(wbuf);
    return type; 
#undef HAS_SEQ
#undef QOS_NULL
}

#ifdef IEEE80211_DWDS
/*
 * Handle WDS discovery on receipt of a 4-address frame in
 * ap mode.  Queue the frame and post an event for someone
 * to plumb the necessary WDS vap for this station.  Frames
 * received prior to the vap set running will then be reprocessed
 * as if they were just received.
 */
static void
ieee80211_wds_discover(struct ieee80211_node *ni, wbuf_t wbuf)
{
#ifdef IEEE80211_DEBUG
    struct ieee80211vap *vap = ni->ni_vap;
#endif
    struct ieee80211com *ic = ni->ni_ic;
    int qlen, age;

    IEEE80211_NODE_WDSQ_LOCK(ni);
    if (_IF_QFULL(&ni->ni_wdsq)) {
        _IF_DROP(&ni->ni_wdsq);
        IEEE80211_NODE_WDSQ_UNLOCK(ni);
        IEEE80211_DISCARD(vap, IEEE80211_MSG_INPUT,
                          (struct ieee80211_frame *)wbuf_header(wbuf), "wds data",
                          "wds pending q overflow, drops %d (size %d)",
                          ni->ni_wdsq.ifq_drops, IEEE80211_PS_MAX_QUEUE);
#ifdef IEEE80211_DEBUG
        if (ieee80211_msg_dumppkts(vap))
            ieee80211_dump_pkt(ic, (u_int8_t*)wbuf_header(wbuf), wbuf_get_pktlen(wbuf), -1, -1);
#endif
        wbuf_free(wbuf);
        return;
    }
    /*
     * Tag the frame with it's expiry time and insert
     * it in the queue.  The aging interval is 4 times
     * the listen interval specified by the station. 
     * Frames that sit around too long are reclaimed
     * using this information.
     */
    /* XXX handle overflow? */
    /* XXX per/vap beacon interval? */
    age = ((ni->ni_intval * ic->ic_lintval) << 2) / 1024; /* TU -> secs */
    _IEEE80211_NODE_WDSQ_ENQUEUE(ni, wbuf, qlen, age);
    IEEE80211_NODE_WDSQ_UNLOCK(ni);

    IEEE80211_NOTE(vap, IEEE80211_MSG_WDS, ni,
                   "save frame, %u now queued", qlen);

    ieee80211_notify_wds_discover(ni);
}
#endif

static wbuf_t
ieee80211_decap(struct ieee80211vap *vap, wbuf_t wbuf, size_t hdrspace, struct ieee80211_rx_status *rs)
{
    struct ieee80211_qosframe_addr4 wh;    /* max size address frame */
    struct ether_header *eh;
    struct llc *llc;
    u_int16_t ether_type = 0;
    struct ieee80211_frame *whhp;

    if (wbuf_get_pktlen(wbuf) < (hdrspace + sizeof(*llc))) {
        /* XXX stat, msg */
        wbuf_free(wbuf);
        wbuf = NULL;
        goto done;
    }

    /* 
     * Store the WME QoS Priority in the wbuf before 802.11 header
     * decapsulation so we can apply 802.1q/802.1p QoS Priority to the VLAN Header.
     */
    whhp = (struct ieee80211_frame *)wbuf_header(wbuf);
    if (IEEE80211_QOS_HAS_SEQ(whhp)) {
        u_int8_t *qos;

        if ((whhp->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS) {
            qos = &((struct ieee80211_qosframe_addr4 *)whhp)->i_qos[0];
        } else {
            qos = &((struct ieee80211_qosframe *)whhp)->i_qos[0];
        }

        /* save priority */
        wbuf_set_qosframe(wbuf);
        wbuf_set_priority(wbuf, (qos[0] & IEEE80211_QOS_TID));
    }

    OS_MEMCPY(&wh, wbuf_header(wbuf), hdrspace);
    llc = (struct llc *)(wbuf_header(wbuf) + hdrspace);

    if (IS_SNAP(llc) && RFC1042_SNAP_NOT_AARP_IPX(llc)) {
       /* leave ether_tyep in  in network order */
        ether_type = llc->llc_un.type_snap.ether_type;
        wbuf_pull(wbuf, (u_int16_t) (hdrspace + sizeof(struct llc) - sizeof(*eh)));
        llc = NULL;
    } else if (IS_SNAP(llc) && IS_BTEP(llc)) {
        /* for bridge-tunnel encap, remove snap and 802.11 headers, keep llc ptr for type */
        wbuf_pull(wbuf,
                  (u_int16_t) (hdrspace + sizeof(struct llc) - sizeof(*eh)));
    } else {
        wbuf_pull(wbuf, (u_int16_t) (hdrspace - sizeof(*eh)));
    }
    eh = (struct ether_header *)(wbuf_header(wbuf));

    ieee80211_rptplacement_input(vap, wbuf, eh);
    ieee80211_smartantenna_input(vap, wbuf, eh, rs);

    switch (wh.i_fc[1] & IEEE80211_FC1_DIR_MASK) {
    case IEEE80211_FC1_DIR_NODS:
        /* If not from BSS then its from TDLS node to WDS */
        if (IEEE80211_TDLS_ENABLED(vap) &&
            !IEEE80211_ADDR_EQ(vap->iv_bss->ni_macaddr, wh.i_addr3)) {
            IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr3);
        }
        else  {
            IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr1);
        }
        IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr2);
        break;
    case IEEE80211_FC1_DIR_TODS:
        IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr3);
        IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr2);
        break;
    case IEEE80211_FC1_DIR_FROMDS:
        IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr1);
        IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr3);
        break;
    case IEEE80211_FC1_DIR_DSTODS:
        IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr3);
        IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr4);
        break;
    } 
#ifdef ATH_EXT_AP
    if ((vap->iv_opmode == IEEE80211_M_STA) &&
        IEEE80211_VAP_IS_EXT_AP_ENABLED(vap)) {
        if (ieee80211_extap_input(vap, eh)) {
            dev_kfree_skb(wbuf);
            return NULL;
        }
    } else {
#ifdef EXTAP_DEBUG
        extern char *arps[];
        eth_arphdr_t *arp = (eth_arphdr_t *)(eh + 1);
        if (eh->ether_type == ETHERTYPE_ARP) {
            printk("InP %s\t" eaistr "\t" eamstr "\t" eaistr "\t" eamstr "\n"
                   "s: " eamstr "\td: " eamstr "\n",
                arps[arp->ar_op],
                eaip(arp->ar_sip), eamac(arp->ar_sha),
                eaip(arp->ar_tip), eamac(arp->ar_tha),
                eamac(eh->ether_shost), eamac(eh->ether_dhost));
        }
#endif
    }
#endif /* ATH_EXT_AP */

    if (llc != NULL) {
        if (IS_BTEP(llc)) {
            /* leave ether_tyep in  in network order */
            eh->ether_type = llc->llc_snap.ether_type; 
        } else {
            eh->ether_type = htons(wbuf_get_pktlen(wbuf) - sizeof(*eh));
        }
    }
    else {
        eh->ether_type = ether_type;
    }
done:
    return wbuf;
}

/* 
 * delivers the data to the OS .
 *  will deliver standard 802.11 frames (with qos control removed)
 *  if IEEE80211_DELIVER_80211 param is set. 
 *  will deliver ethernet frames (with 802.11 header decapped)
 *  if IEEE80211_DELIVER_80211 param is not set. 
 *  this funcction consumes the  passed in wbuf.
 */
static void 
ieee80211_deliver_data(struct ieee80211vap *vap, wbuf_t wbuf, struct ieee80211_node *ni, struct ieee80211_rx_status *rs,
                       u_int32_t hdrspace, int is_mcast, u_int8_t subtype) 
{
    if (!IEEE80211_VAP_IS_DELIVER_80211_ENABLED(vap)) {
        /*
         * if the OS is interested in ethernet frame,
         * decap the 802.11 frame and convert into 
         * ethernet frame.
         */
        wbuf = ieee80211_decap(vap, wbuf, hdrspace, rs);
        if (!wbuf) {
         IEEE80211_DPRINTF(vap,
                          IEEE80211_MSG_INPUT,
                          "decap %s",
                          "failed");
            return;
        }

        /*
         * If IQUE is not enabled, the ops table is NULL and the following
         * steps will be skipped;
         * If IQUE is enabled, the packet will be checked to see whether it
         * is an IGMP packet or not, and update the mcast snoop table if necessary
         */
        if (vap->iv_ique_ops.me_inspect) {
            vap->iv_ique_ops.me_inspect(vap, ni, wbuf);
        }
    }


    if (IEEE80211_ENAB_AOW(ic)) {
        ieee80211_audio_receive(vap, wbuf, rs);
    }
    
    /* perform as a bridge within the AP */
    if ((vap->iv_opmode == IEEE80211_M_HOSTAP) &&
        !IEEE80211_VAP_IS_NOBRIDGE_ENABLED(vap)) {
        wbuf_t wbuf_cpy = NULL;

        if (is_mcast) {
            wbuf_cpy = wbuf_clone(vap->iv_ic->ic_osdev, wbuf);
#if ATH_RXBUF_RECYCLE
			wbuf_set_cloned(wbuf_cpy);
#endif
        } else {
            struct ieee80211_node *ni1;
            /*
             * Check if destination is associated with the
             * same vap and authorized to receive traffic.
             * Beware of traffic destined for the vap itself;
             * sending it will not work; just let it be
             * delivered normally.
             */
            if (IEEE80211_VAP_IS_DELIVER_80211_ENABLED(vap)) {
                struct ieee80211_frame *wh = (struct ieee80211_frame *) wbuf_header(wbuf);
                ni1 = ieee80211_find_node(&vap->iv_ic->ic_sta, wh->i_addr3);
            } else {
                struct ether_header *eh= (struct ether_header *) wbuf_header(wbuf);
                ni1 = ieee80211_find_node(&vap->iv_ic->ic_sta, eh->ether_dhost);
                if (ni1 == NULL) {
                   ni1 = ieee80211_find_wds_node(&vap->iv_ic->ic_sta, eh->ether_dhost);
                }
            }
            if (ni1 != NULL) {
                if (ni1->ni_vap == vap &&
                    ieee80211_node_is_authorized(ni1) &&
                    ni1 != vap->iv_bss) {
                    wbuf_cpy = wbuf;
                    wbuf = NULL;
                }
                ieee80211_free_node(ni1);
            }
        }
        if (wbuf_cpy != NULL) {
            /*
             * send the frame copy back to the interface.
             * this frame is either multicast frame. or unicast frame
             * to one of the stations.
             */
            vap->iv_evtable->wlan_vap_xmit_queue(vap->iv_ifp, wbuf_cpy);
        }
    }
    if (wbuf != NULL) {
#ifdef IEEE80211_WDS
        if (is_mcast) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS,
                "%s: RX MCAST VAP from SA %s\n", __func__, 
                ether_sprintf(eh->ether_shost));

            if (vap->iv_opmode == IEEE80211_M_HOSTAP)
            {
                ieee80211_internal_send_dwds_multicast(vap, m);
            }
        }
#endif
        ieee80211_vap_pause_update_recv_stats(vap,wbuf); /* update the stats for vap pause module */

#if UMAC_SUPPORT_TDLS
        /* deliver the data frame to TDLS handler, don't cosume wbuf as os_handler will do */
        vap->iv_ic->tdls_recv_mgmt(vap->iv_ic, ni, wbuf);
#endif /* UMAC_SUPPORT_TDLS */

#if UMAC_SUPPORT_VI_DBG
        ieee80211_vi_dbg_input(vap, wbuf);
#endif
        /*
         * deliver the data frame to the os. the handler cosumes the wbuf.
         */
        vap->iv_evtable->wlan_receive(vap->iv_ifp, wbuf, IEEE80211_FC0_TYPE_DATA, subtype, rs);
    }
}

struct ieee80211_iter_input_all_arg {
    wbuf_t wbuf;
    struct ieee80211_rx_status *rs;
    int type;
};

static INLINE void
ieee80211_iter_input_all(void *arg, struct ieee80211vap *vap, bool is_last_vap)
{
    struct ieee80211_iter_input_all_arg *params = (struct ieee80211_iter_input_all_arg *) arg; 
    struct ieee80211_node *ni;
    wbuf_t wbuf1;

    if (!ieee80211_vap_active_is_set(vap) && !vap->iv_input_mgmt_filter ) {
        return;
    }

    if (!is_last_vap) {
        wbuf1 = wbuf_clone(vap->iv_ic->ic_osdev, params->wbuf);
        if (wbuf1 == NULL) {
            /* XXX stat+msg */
            return;
        }
    } else {
        wbuf1 = params->wbuf;
        params->wbuf = NULL;
    }

    ni = vap->iv_bss;
    params->type = ieee80211_input(ni, wbuf1, params->rs);
}

int
ieee80211_input_all(struct ieee80211com *ic,
    wbuf_t wbuf, struct ieee80211_rx_status *rs)
{
    struct ieee80211_iter_input_all_arg params; 
    u_int32_t num_vaps;

    params.wbuf = wbuf;
    params.rs = rs;
    params.type = -1;

    ieee80211_iterate_vap_list_internal(ic,ieee80211_iter_input_all,(void *)&params,num_vaps);
    if (params.wbuf != NULL)        /* no vaps, reclaim wbuf */
        wbuf_free(params.wbuf);
    return params.type;
}


static INLINE void
ieee80211_input_monitor_iter_func(void *arg, struct ieee80211vap *vap, bool is_last_vap)
{
    struct ieee80211_iter_input_all_arg *params = (struct ieee80211_iter_input_all_arg *) arg;

    if (!params->wbuf)
        return;

    /*
     * deliver the frame to the os.
     */
    if (vap->iv_opmode == IEEE80211_M_MONITOR && ieee80211_vap_ready_is_set(vap)) {
        /* remove padding from header */
        u_int8_t *header = (u_int8_t *)wbuf_header(params->wbuf);
        u_int32_t hdrspace = ieee80211_anyhdrspace(vap->iv_ic, header);
        u_int32_t realhdrsize = ieee80211_anyhdrsize(header); /* no _F_DATAPAD */
        u_int32_t padsize = hdrspace - realhdrsize;

        if (padsize > 0) {
            memmove(header+padsize, header, realhdrsize);
            wbuf_pull(params->wbuf, padsize);
        }

        vap->iv_evtable->wlan_receive_monitor_80211(vap->iv_ifp, params->wbuf, params->rs);

        /* For now, only allow one vap to be monitoring */
        params->wbuf = NULL;
    }
}

/*
 * this should be called only if there exists a monitor mode vap.
 */
int
ieee80211_input_monitor(struct ieee80211com *ic, wbuf_t wbuf, struct ieee80211_rx_status *rs)
{
    u_int32_t num_vaps;
    struct ieee80211_iter_input_all_arg params;

    params.wbuf = wbuf;
    params.rs = rs;
    params.type = -1;

    ieee80211_iterate_vap_list_internal(ic,ieee80211_input_monitor_iter_func,(void *)&params,num_vaps);
    if (params.wbuf)
        wbuf_free(params.wbuf);
    return 0;
}

int ieee80211_vap_txrx_unregister_event_handler(ieee80211_vap_t vap,ieee80211_vap_txrx_event_handler evhandler, void *arg)
{
    int i,j;
    ieee80211_txrx_event_info *info = &vap->iv_txrx_event_info;
    IEEE80211_VAP_LOCK(vap);
    for (i=0;i<IEEE80211_MAX_VAP_TXRX_EVENT_HANDLERS; ++i) {
        if ( info->iv_txrx_event_handler[i] == evhandler  && info->iv_txrx_event_handler_arg[i] == arg) {
            info->iv_txrx_event_handler[i] = NULL; 
            info->iv_txrx_event_handler_arg[i] = NULL;
            /* recompute event filters */
            info->iv_txrx_event_filter = 0;
            for (j=0;j<IEEE80211_MAX_VAP_TXRX_EVENT_HANDLERS; ++j) {
              if ( info->iv_txrx_event_handler[j]) {
                info->iv_txrx_event_filter |= info->iv_txrx_event_filters[j] ;
              }
            }
            IEEE80211_VAP_UNLOCK(vap);
            return EOK;
        }
    }
    IEEE80211_VAP_UNLOCK(vap);
    return -EEXIST;
}


int ieee80211_vap_txrx_register_event_handler(ieee80211_vap_t vap,ieee80211_vap_txrx_event_handler evhandler, void *arg, u_int32_t event_filter)
{
    int i;
    ieee80211_txrx_event_info *info = &vap->iv_txrx_event_info;
    ieee80211_vap_txrx_unregister_event_handler(vap,evhandler,arg);
    IEEE80211_VAP_LOCK(vap);
    for (i=0;i<IEEE80211_MAX_VAP_TXRX_EVENT_HANDLERS; ++i) {
        if ( info->iv_txrx_event_handler[i] == NULL) {
            info->iv_txrx_event_handler[i] = evhandler; 
            info->iv_txrx_event_handler_arg[i] = arg;
            info->iv_txrx_event_filters[i] = event_filter;
            info->iv_txrx_event_filter |= event_filter;
            IEEE80211_VAP_UNLOCK(vap);
            return EOK;
        }
    }
    IEEE80211_VAP_UNLOCK(vap);
    return -ENOMEM;
}

void ieee80211_vap_txrx_deliver_event(ieee80211_vap_t vap,ieee80211_vap_txrx_event *evt)
{
    ieee80211_txrx_event_info *info = &vap->iv_txrx_event_info;
    int i;

    IEEE80211_VAP_LOCK(vap);
    for(i=0;i<IEEE80211_MAX_VAP_TXRX_EVENT_HANDLERS; ++i) {   
        if (info->iv_txrx_event_handler[i] && (info->iv_txrx_event_filters[i] & evt->type) ) { 
            ieee80211_vap_txrx_event_handler evhandler = info->iv_txrx_event_handler[i]; 
            void *arg = info->iv_txrx_event_handler_arg[i]; 
        
            IEEE80211_VAP_UNLOCK(vap);
            (* evhandler)                                      
                (vap, evt, arg);                   
            IEEE80211_VAP_LOCK(vap);
        }                                                                              
    }                                                                                   
    IEEE80211_VAP_UNLOCK(vap);
}

#if ATH_SUPPORT_IWSPY
int ieee80211_input_iwspy_update_rssi(struct ieee80211com *ic, u_int8_t *address, int8_t rssi)
{
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	
	if (vap)
	{
		vap->iv_evtable->wlan_iwspy_update(vap->iv_ifp, address, rssi);
	}
		
	return 0;
}
#endif

