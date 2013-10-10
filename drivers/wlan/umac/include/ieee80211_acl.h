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

#ifndef _IEEE80211_ACL_H
#define _IEEE80211_ACL_H

#if UMAC_SUPPORT_ACL

typedef struct ieee80211_acl    *ieee80211_acl_t;

int ieee80211_acl_attach(wlan_if_t vap);
int ieee80211_acl_detach(wlan_if_t vap);
int ieee80211_acl_add(wlan_if_t vap, const u_int8_t mac[IEEE80211_ADDR_LEN]);
int ieee80211_acl_remove(wlan_if_t vap, const u_int8_t mac[IEEE80211_ADDR_LEN]);
int ieee80211_acl_get(wlan_if_t vap, u_int8_t *macList, int len, int *num_mac);
int ieee80211_acl_check(wlan_if_t vap, const u_int8_t mac[IEEE80211_ADDR_LEN]);
int ieee80211_acl_flush(wlan_if_t vap);
int ieee80211_acl_setpolicy(wlan_if_t vap, int);
int ieee80211_acl_getpolicy(wlan_if_t vap);

#else /* UMAC_SUPPORT_ACL */

typedef void    *ieee80211_acl_t;

#define ieee80211_acl_attach(vap)            /**/
#define ieee80211_acl_detach(vap)            /**/
/* 
 * defined this way to get rid of compiler warning about unused var 
 */
#define ieee80211_acl_add(vap, mac)          (EINVAL)
#define ieee80211_acl_remove(vap, mac)       (EINVAL)
#define ieee80211_acl_get(vap, macList, len) (EINVAL)
#define ieee80211_acl_check(vap, mac)        (1)
#define ieee80211_acl_flush(vap)             /**/
#define ieee80211_acl_setpolicy(vap, policy) /**/
#define ieee80211_acl_getpolicy(vap)         (IEEE80211_MACCMD_POLICY_OPEN)

#endif /* UMAC_SUPPORT_ACL */

#endif



