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

#include <osdep.h>

#include "ieee80211_power_priv.h"

struct ieee80211_power {
    spinlock_t           pm_state_lock;  /* lock for the data */
    u_int8_t             pm_sleep_count; /* how many modules want to put the vap to network sleep */
};

static void ieee80211_power_vap_event_handler(ieee80211_vap_t     vap, 
                                              ieee80211_vap_event *event, 
                                              void                *arg);

void ieee80211_power_attach(struct ieee80211com *ic)
{
}

void ieee80211_power_detach(struct ieee80211com *ic)
{
}

void
ieee80211_power_vattach(struct ieee80211vap *vap,
                        int fullsleepEnable,
                        u_int32_t sleepTimerPwrSaveMax,
                        u_int32_t sleepTimerPwrSave,
                        u_int32_t sleepTimePerf,
                        u_int32_t inactTimerPwrsaveMax,
                        u_int32_t inactTimerPwrsave,
                        u_int32_t inactTimerPerf,
                        u_int32_t smpsDynamic,
                        u_int32_t pspollEnabled)
{
    osdev_t                  os_handle = vap->iv_ic->ic_osdev;

    vap->iv_power = (ieee80211_power_t)OS_MALLOC(os_handle,sizeof(struct ieee80211_power),0);

    if (!vap->iv_power) {
        return;
    }

    OS_MEMZERO(vap->iv_power, sizeof(struct ieee80211_power));
    spin_lock_init(&vap->iv_power->pm_state_lock);
    switch (vap->iv_opmode) {
    case IEEE80211_M_HOSTAP:
        ieee80211_ap_power_vattach(vap);
        vap->iv_pwrsave_sta = NULL;
        vap->iv_pwrsave_smps = NULL; 
        break;
    case IEEE80211_M_STA:
        vap->iv_pwrsave_sta = ieee80211_sta_power_vattach(vap,
                                                          fullsleepEnable,
                                                          sleepTimerPwrSaveMax,
                                                          sleepTimerPwrSave,
                                                          sleepTimePerf,
                                                          inactTimerPwrsaveMax,
                                                          inactTimerPwrsave,
                                                          inactTimerPerf,
                                                          pspollEnabled);
        vap->iv_pwrsave_smps =  ieee80211_pwrsave_smps_attach(vap,smpsDynamic);
        break;
    default:
        vap->iv_pwrsave_sta = NULL;
        vap->iv_pwrsave_smps = NULL; 
        break;
    }
    ieee80211_vap_register_event_handler(vap,ieee80211_power_vap_event_handler,(void *)NULL );
}

void
ieee80211_power_vdetach(struct ieee80211vap *vap)
{
    ieee80211_vap_unregister_event_handler(vap,ieee80211_power_vap_event_handler,(void *)NULL );
    if (vap->iv_power) {
        spin_lock_destroy(&vap->iv_power->pm_state_lock);
        OS_FREE(vap->iv_power);
    }
    switch (vap->iv_opmode) {
    case IEEE80211_M_HOSTAP:
        ieee80211_ap_power_vdetach(vap);
        break;
    case IEEE80211_M_STA:
        ieee80211_sta_power_vdetach(vap->iv_pwrsave_sta);
        ieee80211_pwrsave_smps_detach(vap->iv_pwrsave_smps);
        vap->iv_pwrsave_sta = NULL;
        vap->iv_pwrsave_smps = NULL;
        break;
    default:
        break;
    }
}

void
ieee80211_set_uapsd_flags(struct ieee80211vap *vap, u_int8_t flags)
{
    vap->iv_uapsd = (u_int8_t) (flags & WME_CAPINFO_UAPSD_ALL);
}

u_int8_t
ieee80211_get_uapsd_flags(struct ieee80211vap *vap)
{
    return (u_int8_t) vap->iv_uapsd;
}

void
ieee80211_set_wmm_power_save(struct ieee80211vap *vap, u_int8_t enable)
{
    vap->iv_wmm_power_save = enable;
}

u_int8_t
ieee80211_get_wmm_power_save(struct ieee80211vap *vap)
{
    return (u_int8_t) vap->iv_wmm_power_save;
}


/*
 * allows multiple modules call this API.
 * to enter network sleep.
 */
int ieee80211_power_enter_nwsleep(struct ieee80211vap *vap)
{
    ieee80211_power_t power_handle = vap->iv_power;
    ieee80211_vap_event    evt;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s sleep_count %d \n", __func__,power_handle->pm_sleep_count );
    /*
     * ignore any requests while the vap is not ready.
     */
    spin_lock(&power_handle->pm_state_lock);
    if (!ieee80211_vap_ready_is_set(vap)) {
        spin_unlock(&power_handle->pm_state_lock);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s vap is not ready \n", __func__);
        return EINVAL;
    }

    power_handle->pm_sleep_count++;
    /* if this is the first request put the vap to network sleep */
    if (power_handle->pm_sleep_count == 1) {
        spin_unlock(&power_handle->pm_state_lock);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s \n", __func__);
        evt.type = IEEE80211_VAP_NETWORK_SLEEP;
        ieee80211_vap_deliver_event(vap, &evt);
        if (!ieee80211_resmgr_exists(vap->iv_ic)) {
            vap->iv_ic->ic_pwrsave_set_state(vap->iv_ic, IEEE80211_PWRSAVE_NETWORK_SLEEP);
        }
    } else {
        spin_unlock(&power_handle->pm_state_lock);
    }

    return EOK;

}


/*
 * allows multiple modules call this API.
 * to exit network sleep.
 */
int ieee80211_power_exit_nwsleep(struct ieee80211vap *vap)
{
    ieee80211_power_t power_handle = vap->iv_power;
    ieee80211_vap_event    evt;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s sleep_count %d \n", __func__,power_handle->pm_sleep_count );
    /*
     * ignore any requests while the vap is not ready.
     */
    spin_lock(&power_handle->pm_state_lock);
    if (!ieee80211_vap_ready_is_set(vap)) {
        spin_unlock(&power_handle->pm_state_lock);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s vap is not ready \n", __func__);
        return EINVAL;
    }

    power_handle->pm_sleep_count--;
    /* if this is the first request put the vap to network sleep */
    if (power_handle->pm_sleep_count == 0) {
        spin_unlock(&power_handle->pm_state_lock);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s \n", __func__);
        evt.type = IEEE80211_VAP_ACTIVE;
        ieee80211_vap_deliver_event(vap, &evt);
        if (!ieee80211_resmgr_exists(vap->iv_ic)) {
            vap->iv_ic->ic_pwrsave_set_state(vap->iv_ic, IEEE80211_PWRSAVE_AWAKE);
        }
    } else {
        spin_unlock(&power_handle->pm_state_lock);
    }

    return EOK;

}

static void ieee80211_power_vap_event_handler(ieee80211_vap_t     vap, 
                                              ieee80211_vap_event *event, 
                                              void                *arg)
{
    ieee80211_power_t power_handle = vap->iv_power;

    switch(event->type) {

    case IEEE80211_VAP_FULL_SLEEP:
    case IEEE80211_VAP_DOWN:
        spin_lock(&power_handle->pm_state_lock);
        power_handle->pm_sleep_count=0;
        spin_unlock(&power_handle->pm_state_lock);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s VAP_DOWN , so reset the state \n", __func__);
        break;
    default:
        break;
    }
    if (vap->iv_pwrsave_sta) {
        ieee80211_sta_power_vap_event_handler( vap->iv_pwrsave_sta,event); 
    }

}
