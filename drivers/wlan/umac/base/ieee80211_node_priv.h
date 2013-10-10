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

#ifndef _IEEE80211_NODE_PRIV_H

#define _IEEE80211_NODE_PRIV_H

#include <osdep.h>
#include <ieee80211_var.h>
#include <ieee80211_channel.h>
#include <ieee80211_rateset.h>
#include <ieee80211_regdmn.h>
#include <ieee80211_target.h>

/*
 * Association id's are managed with a bit vector.
 */
#define	IEEE80211_AID_SET(_vap, _b) \
    ((_vap)->iv_aid_bitmap[IEEE80211_AID(_b) / 32] |= \
    (1 << (IEEE80211_AID(_b) % 32)))
#define	IEEE80211_AID_CLR(_vap, _b) \
    ((_vap)->iv_aid_bitmap[IEEE80211_AID(_b) / 32] &= \
    ~(1 << (IEEE80211_AID(_b) % 32)))
#define	IEEE80211_AID_ISSET(_vap, _b) \
    ((_vap)->iv_aid_bitmap[IEEE80211_AID(_b) / 32] & (1 << (IEEE80211_AID(_b) % 32)))


int ieee80211_setup_node( struct ieee80211_node *ni, ieee80211_scan_entry_t scan_entry );
int ieee80211_setup_node_rsn( struct ieee80211_node *ni, ieee80211_scan_entry_t scan_entry );




#endif
