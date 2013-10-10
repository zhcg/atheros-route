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
* 802.11 station sleep state machine implementation.
*/

#ifndef _IEEE80211_STA_SLEEP_PRIVATE_H_
#define _IEEE80211_STA_SLEEP_PRIVATE_H_


typedef _ieee80211_popwer_sta_smps { 
    u_int32_t               ips_smDynamicPwrSaveEnable;/* Dynamic MIMO power save enabled/disabled */
    IEEE80211_SM_PWRSAVE_STATE  ips_smPowerSaveState;  /* Current dynamic MIMO power save state */
    u_int16_t               ips_smpsDataHistory[IEEE80211_SMPS_DATAHIST_NUM]; /* Throughput history buffer used for enabling MIMO ps */
    u_int8_t                ips_smpsCurrDataIndex;     /* Index in throughput history buffer to be updated */
} ieee80211_popwer_sta_smps;

#endif
