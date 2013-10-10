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

#ifndef IEEE80211_POWER_PRIV_H
#define IEEE80211_POWER_PRIV_H

#include <ieee80211_var.h>

void ieee80211_protect_set_tim(struct ieee80211vap *vap);
void ieee80211_set_tim(struct ieee80211_node *ni, int set,bool isr_context);
int ieee80211_power_alloc_tim_bitmap(struct ieee80211vap *vap);

#if UMAC_SUPPORT_AP_POWERSAVE 
void    ieee80211_ap_power_vattach(struct ieee80211vap *vap);
void    ieee80211_ap_power_vdetach(struct ieee80211vap *);
#else /* UMAC_SUPPORT_AP_POWERSAVE */
#define ieee80211_ap_power_vattach(vap) /* */
#define ieee80211_ap_power_vdetach(vap) /* */
#endif

#if UMAC_SUPPORT_STA_POWERSAVE 
/*
 * attach sta power  module.
 */
ieee80211_sta_power_t ieee80211_sta_power_vattach(struct ieee80211vap *, int fullsleep_enable, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t,u_int32_t);

/*
 * detach sta power module.
 */
void ieee80211_sta_power_vdetach(ieee80211_sta_power_t handle);

void ieee80211_sta_power_vap_event_handler( ieee80211_sta_power_t    powersta, 
                                            ieee80211_vap_event *event);

#else
#define ieee80211_sta_power_vattach(vap, fullsleep_enable, a,b,c,d,e,f,g)  NULL 
#define ieee80211_sta_power_vdetach(vap)                                /* */
#define ieee80211_pwrsave_init(vap)                                     /* */
#define ieee80211_sta_power_vap_event_handler(a,b)                      /* */
#endif

#if UMAC_SUPPORT_STA_SMPS 
/*
 * attach MIMO powersave module.
 */
ieee80211_pwrsave_smps_t ieee80211_pwrsave_smps_attach(struct ieee80211vap *vap, u_int32_t smpsDynamic);


/*
 * detach MIMO powersave module.
 */
void ieee80211_pwrsave_smps_detach(ieee80211_pwrsave_smps_t smps);
#else
#define ieee80211_pwrsave_smps_attach(vap,smpsDynamic) NULL
#define ieee80211_pwrsave_smps_detach(smps)  /**/


#endif

#endif
