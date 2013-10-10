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

int adjust_pri_per_chan_busy(int ext_chan_busy, int pri_margin)
{
    int adjust_pri=0;

    if(ext_chan_busy > DFS_EXT_CHAN_LOADING_THRESH) {

       adjust_pri = (ext_chan_busy - DFS_EXT_CHAN_LOADING_THRESH) * (pri_margin);
       adjust_pri /= 100;

    }
    return adjust_pri;
}

int adjust_thresh_per_chan_busy(int ext_chan_busy, int thresh)
{
    int adjust_thresh=0;

    if(ext_chan_busy > DFS_EXT_CHAN_LOADING_THRESH) {

       adjust_thresh = (ext_chan_busy - DFS_EXT_CHAN_LOADING_THRESH) * thresh;
       adjust_thresh /= 100;

    }
    return adjust_thresh;
}
/* For the extension channel, if legacy traffic is present, we see a lot of false alarms, 
so make the PRI margin narrower depending on the busy % for the extension channel.*/

int dfs_get_pri_margin(struct ath_softc *sc, int is_extchan_detect, int is_fixed_pattern)
{

    int adjust_pri=0, ext_chan_busy=0;
    int pri_margin;
    struct ath_dfs *dfs = sc->sc_dfs;

    if (is_fixed_pattern)
        pri_margin = DFS_DEFAULT_FIXEDPATTERN_PRI_MARGIN;
    else
        pri_margin = DFS_DEFAULT_PRI_MARGIN;

    if (IS_CHAN_HT40(&sc->sc_curchan)) {
        ext_chan_busy = ath_hal_get11nextbusy(sc->sc_ah);
        if(ext_chan_busy >= 0) {
            dfs->dfs_rinfo.ext_chan_busy_ts = ath_hal_gettsf64(sc->sc_ah);
            dfs->dfs_rinfo.dfs_ext_chan_busy = ext_chan_busy;
        } else {
            // Check to see if the cached value of ext_chan_busy can be used
            ext_chan_busy = 0;
            if (dfs->dfs_rinfo.dfs_ext_chan_busy ) {
                if (dfs->dfs_rinfo.rn_lastfull_ts < dfs->dfs_rinfo.ext_chan_busy_ts) {
                    ext_chan_busy = dfs->dfs_rinfo.dfs_ext_chan_busy; 
                    DFS_DPRINTK(sc, ATH_DEBUG_DFS2," PRI Use cached copy of ext_chan_busy extchanbusy=%d \n", ext_chan_busy);
                }
            }
        }
        adjust_pri = adjust_pri_per_chan_busy(ext_chan_busy, pri_margin);

        pri_margin -= adjust_pri;
    }
    return pri_margin;
}

/* For the extension channel, if legacy traffic is present, we see a lot of false alarms, 
so make the thresholds higher depending on the busy % for the extension channel.*/

int dfs_get_filter_threshold(struct ath_softc *sc, struct dfs_filter *rf, int is_extchan_detect)
{
    int ext_chan_busy=0;
    int thresh, adjust_thresh=0;
    struct ath_dfs *dfs = sc->sc_dfs;

    thresh = rf->rf_threshold;    

    if (IS_CHAN_HT40(&sc->sc_curchan)) {
        ext_chan_busy = ath_hal_get11nextbusy(sc->sc_ah);
        if(ext_chan_busy >= 0) {
	        dfs->dfs_rinfo.ext_chan_busy_ts = ath_hal_gettsf64(sc->sc_ah);
            dfs->dfs_rinfo.dfs_ext_chan_busy = ext_chan_busy;
        } else {
            // Check to see if the cached value of ext_chan_busy can be used
            ext_chan_busy = 0;
            if (dfs->dfs_rinfo.dfs_ext_chan_busy) {
	        if (dfs->dfs_rinfo.rn_lastfull_ts < dfs->dfs_rinfo.ext_chan_busy_ts) {
                        ext_chan_busy = dfs->dfs_rinfo.dfs_ext_chan_busy; 
                        DFS_DPRINTK(sc, ATH_DEBUG_DFS2," THRESH Use cached copy of ext_chan_busy extchanbusy=%d rn_lastfull_ts=%llu ext_chan_busy_ts=%llu\n", ext_chan_busy ,(unsigned long long)dfs->dfs_rinfo.rn_lastfull_ts, (unsigned long long)dfs->dfs_rinfo.ext_chan_busy_ts);
                }
            }
        }

        adjust_thresh = adjust_thresh_per_chan_busy(ext_chan_busy, thresh);

        DFS_DPRINTK(sc, ATH_DEBUG_DFS2," filterID=%d extchanbusy=%d adjust_thresh=%d\n", rf->rf_pulseid, ext_chan_busy, adjust_thresh);

        thresh += adjust_thresh;
    }
    return thresh;
}


u_int32_t dfs_round(int32_t val)
{
	u_int32_t ival,rem;

	if (val < 0)
		return 0;
	ival = val/100;
	rem = val-(ival*100);
	if (rem <50)
		return ival;
	else
		return(ival+1);
}


/*
 * Finds the radar state entry that matches the current channel
 */
struct dfs_state *
dfs_getchanstate(struct ath_softc *sc, u_int8_t *index, int ext_chan_flag)
{
	struct ath_dfs *dfs=sc->sc_dfs;
	struct dfs_state *rs=NULL;
	int i;
        HAL_CHANNEL *cmp_ch;

	if (dfs == NULL) {
		DFS_DPRINTK(sc, ATH_DEBUG_DFS,"%s: sc_dfs is NULL\n",
			__func__);
		return NULL;
	}

        if (ext_chan_flag) {
            cmp_ch = (ath_hal_get_extension_channel(sc->sc_ah));    
            if (cmp_ch) {
                DFS_DPRINTK(sc, ATH_DEBUG_DFS2, "Extension channel freq = %u flags=0x%x\n", cmp_ch->channel, cmp_ch->privFlags);
            } else {
                return NULL;
            }
            
        } else {
            cmp_ch = &sc->sc_curchan;
            DFS_DPRINTK(sc, ATH_DEBUG_DFS2, "Primary channel freq = %u flags=0x%x\n", cmp_ch->channel, cmp_ch->privFlags);
        }      
	for (i=0;i<DFS_NUM_RADAR_STATES; i++) {
                if ((dfs->dfs_radar[i].rs_chan.channel == cmp_ch->channel) &&
		    (dfs->dfs_radar[i].rs_chan.channelFlags == cmp_ch->channelFlags)) {
			if (index != NULL)
				*index = (u_int8_t) i;
			return &(dfs->dfs_radar[i]);
		}
	}
	/* No existing channel found, look for first free channel state entry */
	for (i=0; i<DFS_NUM_RADAR_STATES; i++) {
		if (dfs->dfs_radar[i].rs_chan.channel == 0) {
			rs = &(dfs->dfs_radar[i]);
			/* Found one, set channel info and default thresholds */
			rs->rs_chan = *cmp_ch;
			rs->rs_firpwr = dfs->dfs_defaultparams.pe_firpwr;
			rs->rs_radarrssi = dfs->dfs_defaultparams.pe_rrssi;
			rs->rs_height = dfs->dfs_defaultparams.pe_height;
			rs->rs_pulserssi = dfs->dfs_defaultparams.pe_prssi;
			rs->rs_inband = dfs->dfs_defaultparams.pe_inband;
			/* 5413 specific */
			rs->rs_relpwr = dfs->dfs_defaultparams.pe_relpwr;
			rs->rs_relstep = dfs->dfs_defaultparams.pe_relstep;
			rs->rs_maxlen = dfs->dfs_defaultparams.pe_maxlen;

			if (index != NULL)
				*index = (u_int8_t) i;
			return (rs);
		}
	}
	DFS_DPRINTK(sc, ATH_DEBUG_DFS2, "%s: No more radar states left.\n", __func__);
	return(NULL);
}



#endif /* ATH_SUPPORT_DFS */
