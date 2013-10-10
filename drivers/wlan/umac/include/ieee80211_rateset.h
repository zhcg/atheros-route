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

#ifndef _NET80211_IEEE80211_RATESET_H
#define _NET80211_IEEE80211_RATESET_H

#include <ieee80211_var.h>

/*
 * Internal API's for rate/rateset handling
 */
int	ieee80211_iserp_rateset(struct ieee80211com *, struct ieee80211_rateset *);
void ieee80211_set11gbasicrates(struct ieee80211_rateset *, enum ieee80211_phymode);
void ieee80211_setbasicrates(struct ieee80211_rateset *rs, struct ieee80211_rateset *rs_sup);
void ieee80211_setpuregbasicrates(struct ieee80211_rateset *);
int ieee80211_find_puregrate(u_int8_t rate);

/* flags for ieee80211_fix_rate() */
#define	IEEE80211_F_DOSORT	0x00000001	/* sort rate list */
#define	IEEE80211_F_DOFRATE	0x00000002	/* use fixed rate */
#define	IEEE80211_F_DOXSECT	0x00000004	/* intersection of rates  */
#define	IEEE80211_F_DOBRS	0x00000008	/* check for basic rates */

int ieee80211_setup_rates(struct ieee80211_node *ni, const u_int8_t *rates, const u_int8_t *xrates, int flags);
int ieee80211_setup_ht_rates(struct ieee80211_node *ni, u_int8_t *ie, int flags);
void ieee80211_setup_basic_ht_rates(struct ieee80211_node *,u_int8_t *);
void ieee80211_rateset_vattach(struct ieee80211vap *vap);

void ieee80211_init_rateset(struct ieee80211com *ic);
void ieee80211_init_node_rates(struct ieee80211_node *ni, struct ieee80211_channel *chan);
int ieee80211_check_node_rates(struct ieee80211_node *ni);

int ieee80211_check_rate(struct ieee80211vap *vap, struct ieee80211_channel *, struct ieee80211_rateset *);
int ieee80211_check_ht_rate(struct ieee80211vap *vap, struct ieee80211_channel *, struct ieee80211_rateset *);
int ieee80211_fixed_htrate_check(struct ieee80211_node *ni, struct ieee80211_rateset *nrs);
void ieee80211_set_mcast_rate(struct ieee80211vap *vap);
u_int8_t ieee80211_is_multistream(struct ieee80211_node *ni);
u_int8_t ieee80211_getstreams(struct ieee80211com *ic, u_int8_t chainmask);
int ieee80211_check_11b_rates(struct ieee80211vap *vap, struct ieee80211_rateset *rrs);

#define IEEE80211_SUPPORTED_RATES(_ic, _mode)   (&((_ic)->ic_sup_rates[(_mode)]))
#define IEEE80211_HT_RATES(_ic, _mode)          (&((_ic)->ic_sup_ht_rates[(_mode)]))
#define IEEE80211_XR_RATES(_ic)                 (&((_ic)->ic_sup_xr_rates))
#define IEEE80211_HALF_RATES(_ic)               (&((_ic)->ic_sup_half_rates))
#define IEEE80211_QUARTER_RATES(_ic)            (&((_ic)->ic_sup_quarter_rates))

static INLINE u_int64_t
ieee80211_rate2linkpeed(u_int8_t rate)
{
    return ((u_int64_t)500000) * rate;
}

#endif /* _NET80211_IEEE80211_RATESET_H */
