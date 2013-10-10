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

#ifndef _IEEE80211_ACS_H
#define _IEEE80211_ACS_H

typedef struct ieee80211_acs    *ieee80211_acs_t;

#if UMAC_SUPPORT_ACS

int ieee80211_acs_attach(ieee80211_acs_t *acs, 
                          struct ieee80211com *ic, 
                          osdev_t             osdev);

int ieee80211_acs_detach(ieee80211_acs_t *acs);

#else /* UMAC_SUPPORT_ACS */

static INLINE int ieee80211_acs_attach(ieee80211_acs_t *acs, 
                                        struct ieee80211com *ic, 
                                        osdev_t             osdev) 
{
    return 0;
}
#define ieee80211_acs_detach(acs) /**/

#endif /* UMAC_SUPPORT_ACS */

#endif
