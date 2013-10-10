/*
 *  Copyright (c) 2009 Atheros Communications Inc.  All rights reserved.
 */

#ifndef _IEEE80211_P2P_GO_H_
#define _IEEE80211_P2P_GO_H_

/*
 * Routine to inform the P2P GO that there is a new NOA IE.
 * @param p2p_go_handle : handle to the p2p object .
 * @param new_noa_ie    : pointer to new NOA IE schedule to update.
 *                        If new_noa_ie is NULL, then disable the append of NOA IE.
 */
void wlan_p2p_go_update_noa_ie(
    wlan_p2p_go_t                           p2p_go_handle,
    struct ieee80211_p2p_sub_element_noa    *new_noa_ie,
    u_int32_t                               tsf_offset);

#endif  //_IEEE80211_P2P_GO_H_
