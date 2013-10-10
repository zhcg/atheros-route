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

#ifndef _IEEE80211_POWER_STA_H
#define _IEEE80211_POWER_STA_H

struct ieee80211_sta_power;
typedef struct ieee80211_sta_power *ieee80211_sta_power_t;

struct ieee80211_pwrsave_smps;
typedef struct ieee80211_pwrsave_smps *ieee80211_pwrsave_smps_t;

#define IEEE80211_MAX_POWER_STA_EVENT_HANDLERS 10 

typedef enum {
    IEEE80211_POWER_STA_PAUSED,
    IEEE80211_POWER_STA_UNPAUSED,
}ieee80211_sta_power_event; 


/*
 * attach sta power  module.
 */
ieee80211_sta_power_t ieee80211_sta_power_attach(struct ieee80211vap *, int fullsleep_enable, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t);

/*
 * detach sta power module.
 */
void ieee80211_sta_power_detach(ieee80211_sta_power_t handle);

/*
 * completed null frame sent by  the sta power module. 
 */
void    ieee80211_sta_power_complete_wbuf(ieee80211_sta_power_t handle, u_int32_t flags);

/*
 * send keep alive frame .
 */
void ieee80211_sta_power_send_keepalive(ieee80211_sta_power_t handle);


/*
 * initiate pause (enter fake sleep) operation.
 */
void ieee80211_sta_power_pause(ieee80211_sta_power_t handle);

/*
 * initiate unpause (exit fake sleep) operation.
 */
void ieee80211_sta_power_unpause(ieee80211_sta_power_t handle);

typedef void (*ieee80211_sta_power_event_handler) (ieee80211_sta_power_t powersta, ieee80211_sta_power_event event, void *arg);

/*
 * @register a power sta event handler.
 * ARGS :
 *  ieee80211_sta_power_event_handler : event handler
 *  arg                               : argument passed back via the evnt handler
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 * allows more than one event handler to be registered.
 */
int ieee80211_sta_power_register_event_handler(ieee80211_sta_power_t,ieee80211_sta_power_event_handler evhandler, void *arg);

/*
 * @unregister a power sta event handler.
 * ARGS :
 *  ieee80211_sta_power_event_handler : event handler
 *  arg                               : argument passed back via the evnt handler
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 * allows more than one event handler to be registered.
 */
int ieee80211_sta_power_unregister_event_handler(ieee80211_sta_power_t,ieee80211_sta_power_event_handler evhandler, void *arg);

/*
 * set wmm power state .
 */
void     ieee80211_sta_power_set_wmm_power_save(ieee80211_sta_power_t handle, u_int8_t enable);

/*
 * get wmm power state .
 */
u_int8_t ieee80211_sta_power_get_wmm_power_save(ieee80211_sta_power_t handle);

/*
 * powersave complete wbuf. 
 */
void    ieee80211_sta_power_pwrsave_complete_wbuf(ieee80211_sta_power_t handle, u_int32_t);

/*
 * smps powersave complete wbuf. 
 */
void    ieee80211_sta_power_smps_complete_wbuf(ieee80211_sta_power_t handle, u_int32_t);

/*
 * notify the vap connection state.
 */
void    ieee80211_sta_power_notify_connection_state(ieee80211_sta_power_t handle, bool is_connected);

void  ieee80211_sta_power_event_tim(ieee80211_sta_power_t handle);

void  ieee80211_sta_power_event_dtim(ieee80211_sta_power_t handle);

bool ieee80211_pwrsave_smps_dynamic_pwrsave_enabled(ieee80211_pwrsave_smps_t handle);

static INLINE void  ieee80211_pwrsave_notify_connection_state(struct ieee80211vap *vap, bool is_connected)
{
    ieee80211_sta_power_notify_connection_state(vap->iv_pwrsave,is_connected);
    ieee80211_sta_power_notify_connection_state(vap->iv_pwrsave_smps,is_connected);
}

static INLINE ieee80211_pwrsave_notify_complete_wbuf(struct ieee80211vap *vap,enum ieee80211_opmode opmode,
                                        wbuf_t wbuf, struct ieee80211_tx_status *ts)
{
    if (opmode == IEEE80211_M_STA) {
        if (wbuf_is_pwrsaveframe(wbuf)) {
            ieee80211_pwrsave_complete_wbuf(vap->iv_pwrsave_sta, ts->ts_flags);
        }

        if (wbuf_is_smpsactframe(wbuf)) {
            /* Special completion processing for smps action frames */
            ieee80211_smps_complete_wbuf(vap->iv_pwrsave_smps, ts->ts_flags);
        }
    }
}
#endif
