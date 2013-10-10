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
#include <osdep.h>
#include <ieee80211_power_priv.h>

#if UMAC_SUPPORT_AP_POWERSAVE 

void
ieee80211_ap_power_vattach(struct ieee80211vap *vap)
{
    vap->iv_set_tim = ieee80211_set_tim;
    /*
     * Allocate these only if needed
     */
    if (!vap->iv_tim_bitmap) {
        vap->iv_tim_len = howmany(vap->iv_max_aid,8) * sizeof(u_int8_t);
        vap->iv_tim_bitmap = (u_int8_t *) OS_MALLOC(vap->iv_ic->ic_osdev,vap->iv_tim_len,0);
        if (vap->iv_tim_bitmap == NULL) {
            printf("%s: no memory for TIM bitmap!\n", __func__);
            vap->iv_tim_len = 0;
        } else {
            OS_MEMZERO(vap->iv_tim_bitmap,vap->iv_tim_len);
        }
    }
    
}

void
ieee80211_ap_power_vdetach(struct ieee80211vap *vap)
{
    if (vap->iv_tim_bitmap != NULL) {
        OS_FREE(vap->iv_tim_bitmap);
        vap->iv_tim_bitmap = NULL;
        vap->iv_tim_len = 0;
        vap->iv_ps_sta = 0;
    }
}
void 
ieee80211_protect_set_tim(struct ieee80211vap *vap)
{
	int set = vap->iv_tim_infor.set;
	u_int16_t aid = vap->iv_tim_infor.aid;
	if (set != (isset(vap->iv_tim_bitmap, aid) != 0)) {
	
		if (set) {
			setbit(vap->iv_tim_bitmap, aid);
			vap->iv_ps_pending++;
		} else {
			vap->iv_ps_pending--;
			clrbit(vap->iv_tim_bitmap, aid);
		}		
		IEEE80211_VAP_TIMUPDATE_ENABLE(vap);
	}
}

/*
 * Indicate whether there are frames queued for a station in power-save mode.
 */
void
ieee80211_set_tim(struct ieee80211_node *ni, int set,bool isr_context)
{
    struct ieee80211vap *vap = ni->ni_vap;
    u_int16_t aid;
    rwlock_state_t lock_state;
    unsigned long flags;

    KASSERT(vap->iv_opmode == IEEE80211_M_HOSTAP 
            || vap->iv_opmode == IEEE80211_M_IBSS ,
            ("operating mode %u", vap->iv_opmode));

    aid = IEEE80211_AID(ni->ni_associd);
    KASSERT(aid < vap->iv_max_aid,
            ("bogus aid %u, max %u", aid, vap->iv_max_aid));

	vap->iv_tim_infor.set = set;
	vap->iv_tim_infor.aid = aid;

	if (!isr_context)
	{
#if ATH_SUPPORT_SHARED_IRQ	
		struct ieee80211com *ic = vap->iv_ic;
		OS_EXEC_INTSAFE(ic->ic_osdev, ieee80211_protect_set_tim, vap);
#else
		OS_EXEC_INTSAFE(NULL, ieee80211_protect_set_tim, vap);
#endif		
		
	}
	else
	{
        /* avoid the race with beacon update */
        OS_RWLOCK_WRITE_LOCK_IRQSAVE(&vap->iv_tim_update_lock, &lock_state, flags);

	    if (set != (isset(vap->iv_tim_bitmap, aid) != 0)) {

	        if (set) {
	            setbit(vap->iv_tim_bitmap, aid);
	            vap->iv_ps_pending++;
	        } else {
	        	vap->iv_ps_pending--;
	            clrbit(vap->iv_tim_bitmap, aid);
	        }		
	        IEEE80211_VAP_TIMUPDATE_ENABLE(vap);
	    }
        OS_RWLOCK_WRITE_UNLOCK_IRQRESTORE(&vap->iv_tim_update_lock, &lock_state, flags);
	}
}

int ieee80211_power_alloc_tim_bitmap(struct ieee80211vap *vap)
{
    u_int8_t *tim_bitmap = NULL;
    u_int32_t old_len = vap->iv_tim_len;

    //printk("[%s] entry\n",__func__);

    vap->iv_tim_len = howmany(vap->iv_max_aid, 8) * sizeof(u_int8_t);
    tim_bitmap = OS_MALLOC(vap->iv_ic->ic_osdev, vap->iv_tim_len, 0);
    if(!tim_bitmap) {
        vap->iv_tim_len = old_len;
        return -1;
    }    
    OS_MEMZERO(tim_bitmap, vap->iv_tim_len);
    if (vap->iv_tim_bitmap) { 
        OS_MEMCPY(tim_bitmap, vap->iv_tim_bitmap,
            vap->iv_tim_len > old_len ? old_len : vap->iv_tim_len);
        OS_FREE(vap->iv_tim_bitmap);
    }
    vap->iv_tim_bitmap = tim_bitmap;

    //printk("[%s] exits\n",__func__);

    return 0;
}
#endif
