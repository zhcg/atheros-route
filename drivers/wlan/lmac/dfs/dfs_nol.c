/*
 * Copyright (c) 2002-2010, Atheros Communications Inc.
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

#ifdef ATH_SUPPORT_DFS
#include "dfs.h"
#include "dfs_ioctl.h"
#include "adf_os_time.h" /* adf_os_time_t, adf_os_time_after */

extern int usenol;

OS_TIMER_FUNC(dfs_remove_from_nol)
{
        struct ath_softc *sc;
        uint32_t nchans=0;
        HAL_CHANNEL *chans = NULL;
        struct dfs_nol_timer_arg *nol_arg;
        u_int16_t delfreq=0;

        OS_GET_TIMER_ARG(nol_arg, struct dfs_nol_timer_arg *);

        sc = nol_arg->sc;
        delfreq= nol_arg->delfreq;

        if (sc->sc_opmode == HAL_M_HOSTAP || sc->sc_opmode == HAL_M_IBSS)
        {
            chans = (HAL_CHANNEL *)OS_MALLOC(sc->sc_osdev,
                                         IEEE80211_CHAN_MAX * sizeof(HAL_CHANNEL),
                                         GFP_ATOMIC);
            if (chans == NULL) 
            {
                DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: unable to allocate channel table\n", __func__);
                goto done;
            }

            nchans = dfs_check_nol(sc, chans, IEEE80211_CHAN_MAX, delfreq);
            if (nchans > 0 && sc->sc_ieee_ops->setup_channel_list) 
            {
                sc->sc_ieee_ops->setup_channel_list(sc->sc_ieee, CLIST_DFS_UPDATE,
                                                chans, nchans, NULL, 0,
                                                CTRY_DEFAULT);
            }
            OS_FREE(chans);
        }
        done: 
             OS_FREE(nol_arg);
            return;

}


void dfs_add_to_nol(struct ath_softc *sc, HAL_CHANNEL *chans)
{
    uint32_t nchans=1;

    if (sc->sc_opmode == HAL_M_HOSTAP || sc->sc_opmode == HAL_M_IBSS)
    {
        if (sc->sc_ieee_ops->setup_channel_list) 
        {
            sc->sc_ieee_ops->setup_channel_list(sc->sc_ieee, CLIST_NOL_UPDATE,
                                                chans, nchans, NULL, 0,
                                                CTRY_DEFAULT);
        }
    }
}


void dfs_print_nol(struct ath_softc *sc)
{
	struct ath_dfs *dfs=sc->sc_dfs;
	struct dfs_nolelem *nol;
        int i=0;
        uint32_t diff=0;

	if (dfs == NULL) {
		DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: sc_dfs is NULL\n",
			__func__);
		return;
	}
	nol = dfs->dfs_nol;
        printk("NOL\n");
	while (nol != NULL) {
	        diff = (int)(nol->nol_ticksfree - OS_GET_TICKS());
	        diff = CONVERT_SYSTEM_TIME_TO_SEC(diff);
        	DFS_DPRINTK(sc, ATH_DEBUG_DFS, "nol:%d channel=%d time left=%u seconds ticksfree=%llu \n", i++, nol->nol_chan.channel, diff, (unsigned long long)nol->nol_ticksfree);
                nol=nol->nol_next;
        }
}

uint32_t dfs_get_nol_chan(struct ath_softc *sc, HAL_CHANNEL *chan)
{
        
	struct ath_dfs *dfs=sc->sc_dfs;
	struct dfs_nolelem *nol;
        int i=0;
        uint32_t diff=0;

	if (dfs == NULL) {
		DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: sc_dfs is NULL\n",
			__func__);
		return 0;
	}
	nol = dfs->dfs_nol;
	while (nol != NULL) {
		if (nol->nol_chan.channel == chan->channel) {
			diff = (int)(nol->nol_ticksfree - OS_GET_TICKS());
			diff = CONVERT_SYSTEM_TIME_TO_MS(diff);
			DFS_DPRINTK(sc, ATH_DEBUG_DFS, "nol:%d channel=%d time left=%u ms\n", i++, nol->nol_chan.channel, diff);
			break;
		}
		nol=nol->nol_next;
        }
        return diff;
}
 

void dfs_get_nol(struct ath_softc *sc, struct dfsreq_nolelem *dfs_nol, int *nchan) 
{
    struct ath_dfs *dfs=sc->sc_dfs;
    struct dfs_nolelem *nol;

    *nchan = 0;
	
    if (dfs == NULL) {
        DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: sc_dfs is NULL\n",
	    __func__);
        return;
    }

    nol=dfs->dfs_nol;
    while (nol != NULL) {
    dfs_nol[*nchan].nolfreq = nol->nol_chan.channel;
    dfs_nol[*nchan].ticksfree = nol->nol_ticksfree;
    ++(*nchan); 
    nol = nol->nol_next;
    }
}


void dfs_set_nol(struct ath_softc *sc, struct dfsreq_nolelem *dfs_nol, int nchan)
{
#define TIME_IN_MS 1000
#define TIME_IN_US (TIME_IN_MS * 1000)
    struct ath_dfs *dfs=sc->sc_dfs;
    u_int32_t dfs_nol_timeout; 
    HAL_CHANNEL chan;
    int i;

    if (dfs == NULL) {
        DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: sc_dfs is NULL\n",
	    __func__);
        return;
    }

    for (i =0; i < nchan; i++)
    {
        if (adf_os_time_after(dfs_nol[i].ticksfree, OS_GET_TICKS())) {
	    chan.channel = dfs_nol[i].nolfreq;
            chan.channelFlags = 0;
            chan.privFlags = 0;
            dfs_add_to_nol(sc, &chan);	
            dfs_nol_timeout = CONVERT_SYSTEM_TIME_TO_MS((long)dfs_nol[i].ticksfree - (long)OS_GET_TICKS())/TIME_IN_MS;
            dfs_nol_addchan(sc, &chan, dfs_nol_timeout);
        }
    }
#undef TIME_IN_MS
#undef TIME_IN_US
}


void dfs_nol_addchan(struct ath_softc *sc, HAL_CHANNEL *chan, u_int32_t dfs_nol_timeout)
{
#define TIME_IN_MS 1000
#define TIME_IN_US (TIME_IN_MS * 1000)
	struct ath_dfs *dfs=sc->sc_dfs;
	struct dfs_nolelem *nol, *elem, *prev;
	adf_os_time_t ticks;
        struct dfs_nol_timer_arg *dfs_nol_arg;

	if (dfs == NULL) {
		DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: sc_dfs is NULL\n",
			__func__);
		return;
	}
	nol = dfs->dfs_nol;
	prev = dfs->dfs_nol;
        elem=NULL;
	while (nol != NULL) {
                    if ((nol->nol_chan.channel == chan->channel) &&
		    (nol->nol_chan.channelFlags == chan->channelFlags) &&
		    (nol->nol_chan.privFlags == chan->privFlags)) {
			ticks=OS_GET_TICKS();
			nol->nol_ticksfree=ticks+CONVERT_MS_TO_SYSTEM_TIME(dfs_nol_timeout*TIME_IN_MS);
                        DFS_DPRINTK(sc, ATH_DEBUG_DFS2, "%s: Update OS Ticks for NOL channel %d\n", __func__, nol->nol_chan.channel);
                        OS_CANCEL_TIMER(&nol->nol_timer);
                        OS_SET_TIMER(&nol->nol_timer, dfs_nol_timeout*TIME_IN_MS);
			ath_hal_dfsfound(sc->sc_ah, chan, dfs_nol_timeout*TIME_IN_US);
			return;
		}
		prev = nol;
		nol = nol->nol_next;
	}
        /* Add a new element to the NOL*/
	elem = (struct dfs_nolelem *)OS_MALLOC(sc->sc_osdev, sizeof(struct dfs_nolelem),GFP_ATOMIC);
	if (elem == NULL) {
		DFS_DPRINTK(sc, ATH_DEBUG_DFS,
			"%s: failed to allocate memory for nol entry\n",
			 __func__);
		return;
	}
	dfs_nol_arg = (struct dfs_nol_timer_arg *)OS_MALLOC(sc->sc_osdev, sizeof(struct dfs_nol_timer_arg), GFP_ATOMIC);
	if (dfs_nol_arg == NULL) {
		DFS_DPRINTK(sc, ATH_DEBUG_DFS,
			"%s: failed to allocate memory for nol timer arg entry\n",
			 __func__);
        OS_FREE(elem);
		return;
	}
	elem->nol_chan = *chan;
	ticks = OS_GET_TICKS();	
	elem->nol_ticksfree=ticks+CONVERT_MS_TO_SYSTEM_TIME(dfs_nol_timeout*TIME_IN_MS);
	elem->nol_next = NULL;
	if (prev) {
            prev->nol_next = elem;
        } else {
            /* This is the first element in the NOL */
            dfs->dfs_nol = elem;
        }
        dfs_nol_arg->sc = sc;
        dfs_nol_arg->delfreq = chan->channel;

        OS_INIT_TIMER(sc->sc_osdev, &elem->nol_timer, dfs_remove_from_nol, dfs_nol_arg);
        OS_SET_TIMER(&elem->nol_timer, dfs_nol_timeout*TIME_IN_MS);
        DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: new NOL channel %d\n", __func__, elem->nol_chan.channel);
	ath_hal_dfsfound(sc->sc_ah, chan, dfs_nol_timeout*TIME_IN_US);
#undef TIME_IN_MS
#undef TIME_IN_US
}


u_int32_t
dfs_check_nol(struct ath_softc *sc, HAL_CHANNEL *chans, u_int32_t nchans, u_int16_t delfreq)
{
	struct ath_dfs *dfs=sc->sc_dfs;
	struct dfs_nolelem *nol,**prev_next;
	u_int32_t free_index=0;

	if (dfs == NULL) {
		DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: sc_dfs is NULL\n",
			__func__);
		return 0;
	}
        DFS_DPRINTK(sc, ATH_DEBUG_DFS2, "%s: remove channel=%d from NOL\n", __func__, delfreq);
	prev_next = &(dfs->dfs_nol);
	nol = dfs->dfs_nol;
	while (nol != NULL) {
	if (nol->nol_chan.channel  == delfreq) {
		*prev_next = nol->nol_next;
		nol->nol_chan.privFlags &= ~CHANNEL_INTERFERENCE;
		if (free_index < nchans) {
		    chans[free_index].channel = nol->nol_chan.channel;
		    chans[free_index].channelFlags = nol->nol_chan.channelFlags;
		    chans[free_index].privFlags = nol->nol_chan.privFlags;
		    free_index++;
		}
		ath_hal_checkdfs(sc->sc_ah, &nol->nol_chan);
	        DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s removing channel %d from NOL tstamp=%d\n", __func__, nol->nol_chan.channel, (CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP()) / 1000));
                OS_CANCEL_TIMER(&nol->nol_timer);
		OS_FREE(nol);
                nol=NULL;
		nol = *prev_next;
	} else {
			prev_next = &(nol->nol_next);
			nol = nol->nol_next;
		}
	}
	return free_index;
}

#endif /* ATH_SUPPORT_DFS */
