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

static char debug_dup[33];
static int debug_dup_cnt;

static inline u_int8_t
dfs_process_pulse_dur(struct ath_softc *sc, u_int8_t re_dur) 
{
	struct ath_dfs *dfs=sc->sc_dfs;
	if (re_dur == 0) {
		return 1;
	} else {
		/* Convert 0.8us durations to TSF ticks (usecs) */
		return (u_int8_t)dfs_round((int32_t)((dfs->dur_multiplier)*re_dur));
	}
}

int
dfs_process_radarevent(struct ath_softc *sc, HAL_CHANNEL *chan)
{
	struct ath_dfs *dfs=sc->sc_dfs;
	struct ath_hal *ah=sc->sc_ah;
	struct dfs_event re,*event;
	struct dfs_state *rs=NULL;
	struct dfs_filtertype *ft;
	struct dfs_filter *rf;
	int found, retval=0,p, empty;
	int events_processed=0;
    u_int32_t tabledepth,rfilt, index;
	u_int64_t deltafull_ts = 0,this_ts, deltaT;
	HAL_CHANNEL *thischan;
	HAL_PHYERR_PARAM pe;
    struct dfs_pulseline *pl;
    static u_int32_t  test_ts  = 0;
    static u_int32_t  diff_ts  = 0;

        int ext_chan_event_flag=0;

	if (dfs == NULL) {
		DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: sc_sfs is NULL\n",
			__func__);
		return 0;
	}
	if ( ! (sc->sc_curchan.privFlags & CHANNEL_DFS)) {
	        DFS_DPRINTK(sc, ATH_DEBUG_DFS2, "%s: radar event on non-DFS chan\n",
                        __func__);
                dfs_reset_radarq(sc);
                dfs_reset_alldelaylines(sc);
        	return 0;
        }

    pl = dfs->pulses;

 	/* TEST : Simulate radar bang, make sure we add the channel to NOL (bug 29968) */
        if (dfs->dfs_bangradar) {
                    /* bangradar will always simulate radar found on the primary channel */
		     rs = &dfs->dfs_radar[dfs->dfs_curchan_radindex];
 		     dfs->dfs_bangradar = 0; /* reset */
	             DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: bangradar\n", __func__);
 		     retval = 1;                    
                     goto dfsfound;
 	 }


	ATH_DFSQ_LOCK(dfs);
	empty = STAILQ_EMPTY(&(dfs->dfs_radarq));
	ATH_DFSQ_UNLOCK(dfs);

	while ((!empty) && (!retval) && (events_processed < MAX_EVENTS)) {
		ATH_DFSQ_LOCK(dfs);
		event = STAILQ_FIRST(&(dfs->dfs_radarq));
		if (event != NULL)
			STAILQ_REMOVE_HEAD(&(dfs->dfs_radarq), re_list);
		ATH_DFSQ_UNLOCK(dfs);

		if (event == NULL) {
			empty = 1;
			break;
		}
                events_processed++;
                re = *event;

		OS_MEMZERO(event, sizeof(struct dfs_event));
		ATH_DFSEVENTQ_LOCK(dfs);
		STAILQ_INSERT_TAIL(&(dfs->dfs_eventq), event, re_list);
		ATH_DFSEVENTQ_UNLOCK(dfs);

		found = 0;
		if (re.re_chanindex < DFS_NUM_RADAR_STATES)
			rs = &dfs->dfs_radar[re.re_chanindex];
		else {
			ATH_DFSQ_LOCK(dfs);
			empty = STAILQ_EMPTY(&(dfs->dfs_radarq));
			ATH_DFSQ_UNLOCK(dfs);
			continue;
		}
		if (rs->rs_chan.privFlags & CHANNEL_INTERFERENCE) {
			ATH_DFSQ_LOCK(dfs);
			empty = STAILQ_EMPTY(&(dfs->dfs_radarq));
			ATH_DFSQ_UNLOCK(dfs);
			continue;
		}

		if (dfs->dfs_rinfo.rn_lastfull_ts == 0) {
			/*
			 * Either not started, or 64-bit rollover exactly to zero
			 * Just prepend zeros to the 15-bit ts
			 */
			dfs->dfs_rinfo.rn_ts_prefix = 0;
			this_ts = (u_int64_t) re.re_ts;
		} else {
                         /* WAR 23031- patch duplicate ts on very short pulses */
                        /* This pacth has two problems in linux environment.
                         * 1)The time stamp created and hence PRI depends entirely on the latency.
                         *   If the latency is high, it possibly can split two consecutive
                         *   pulses in the same burst so far away (the same amount of latency)
                         *   that make them look like they are from differenct bursts. It is
                         *   observed to happen too often. It sure makes the detection fail.
                         * 2)Even if the latency is not that bad, it simply shifts the duplicate
                         *   timestamps to a new duplicate timestamp based on how they are processed.
                         *   This is not worse but not good either.
                         *
                         *   Take this pulse as a good one and create a probable PRI later
                         */
                        if (re.re_dur == 0 && re.re_ts == dfs->dfs_rinfo.rn_last_unique_ts) {
                                debug_dup[debug_dup_cnt++] = '1';
                                DFS_DPRINTK(sc, ATH_DEBUG_DFS1, "\n %s deltaT is 0 \n", __func__);
                        } else {
                                dfs->dfs_rinfo.rn_last_unique_ts = re.re_ts;
                                debug_dup[debug_dup_cnt++] = '0';
                        }
                        if (debug_dup_cnt >= 32){
                                 debug_dup_cnt = 0;
                        }


			if (re.re_ts <= dfs->dfs_rinfo.rn_last_ts) {
				dfs->dfs_rinfo.rn_ts_prefix += 
					(((u_int64_t) 1) << DFS_TSSHIFT);
				/* Now, see if it's been more than 1 wrap */
				deltafull_ts = re.re_full_ts - dfs->dfs_rinfo.rn_lastfull_ts;
				if (deltafull_ts > 
				    ((u_int64_t)((DFS_TSMASK - dfs->dfs_rinfo.rn_last_ts) + 1 + re.re_ts)))
					deltafull_ts -= (DFS_TSMASK - dfs->dfs_rinfo.rn_last_ts) + 1 + re.re_ts;
				deltafull_ts = deltafull_ts >> DFS_TSSHIFT;
				if (deltafull_ts > 1) {
					dfs->dfs_rinfo.rn_ts_prefix += 
						((deltafull_ts - 1) << DFS_TSSHIFT);
				}
			} else {
				deltafull_ts = re.re_full_ts - dfs->dfs_rinfo.rn_lastfull_ts;
				if (deltafull_ts > (u_int64_t) DFS_TSMASK) {
					deltafull_ts = deltafull_ts >> DFS_TSSHIFT;
					dfs->dfs_rinfo.rn_ts_prefix += 
						((deltafull_ts - 1) << DFS_TSSHIFT);
				}
			}
			this_ts = dfs->dfs_rinfo.rn_ts_prefix | ((u_int64_t) re.re_ts);
		}
		dfs->dfs_rinfo.rn_lastfull_ts = re.re_full_ts;
		dfs->dfs_rinfo.rn_last_ts = re.re_ts;

		re.re_dur = dfs_process_pulse_dur(sc, re.re_dur);
	        if (re.re_dur != 1) {
                	this_ts -= re.re_dur;
                }

              /* Save the pulse parameters in the pulse buffer(pulse line) */
                index = (pl->pl_lastelem + 1) & DFS_MAX_PULSE_BUFFER_MASK;
                if (pl->pl_numelems == DFS_MAX_PULSE_BUFFER_SIZE)
                        pl->pl_firstelem = (pl->pl_firstelem+1) & DFS_MAX_PULSE_BUFFER_MASK;
                else
                        pl->pl_numelems++;
                pl->pl_lastelem = index;
                pl->pl_elems[index].p_time = this_ts;
                pl->pl_elems[index].p_dur = re.re_dur;
                pl->pl_elems[index].p_rssi = re.re_rssi;
                diff_ts = (u_int32_t)this_ts - test_ts;
                test_ts = (u_int32_t)this_ts;
                DFS_DPRINTK(sc, ATH_DEBUG_DFS1,"ts%u %u %u diff %u pl->pl_lastelem.p_time=%llu\n",(u_int32_t)this_ts, re.re_dur, re.re_rssi, diff_ts, (unsigned long long)pl->pl_elems[index].p_time);


		/* If diff_ts is very small, we might be getting false pulse detects 
		 * due to heavy interference. We might be getting spectral splatter 
		 * from adjacent channel. In order to prevent false alarms we 
		 * clear the delay-lines. This might impact positive detections under 
		 * harsh environments, but helps with false detects. */
		if (diff_ts < 100) {
	    		dfs_reset_alldelaylines(sc);
	    		dfs_reset_radarq(sc);
		}

		found = 0;
                if ((dfs->dfsdomain == DFS_FCC_DOMAIN) || (dfs->dfsdomain == DFS_MKK4_DOMAIN)) {
		    for (p=0; (p < dfs->dfs_rinfo.rn_numbin5radars) && (!found); p++) {
			    struct dfs_bin5radars *br;
			    u_int32_t b5_rssithresh;
			    br = &(dfs->dfs_b5radars[p]);
			    b5_rssithresh = br->br_pulse.b5_rssithresh;

                            /* Adjust the filter threshold for rssi in non TURBO mode*/
                            if( ! (sc->sc_curchan.channelFlags & CHANNEL_TURBO) ) {
                                b5_rssithresh += br->br_pulse.b5_rssimargin;
                            } 

			    if ((re.re_dur >= br->br_pulse.b5_mindur) &&
			        (re.re_dur <= br->br_pulse.b5_maxdur) &&
			        (re.re_rssi >= b5_rssithresh)) {

                                // This is a valid Bin5 pulse, check if it belongs to a burst
                                re.re_dur = dfs_retain_bin5_burst_pattern(sc, diff_ts, re.re_dur);
                                // Remember our computed duration for the next pulse in the burst (if needed)
                                dfs->dfs_rinfo.dfs_bin5_chirp_ts = this_ts;
                                dfs->dfs_rinfo.dfs_last_bin5_dur = re.re_dur;


				if( dfs_bin5_addpulse(sc, br, &re, this_ts) ) {
					found |= dfs_bin5_check(sc);
				}
			    } else 
			DFS_DPRINTK(sc, ATH_DEBUG_DFS3, "%s too low to be Bin5 pulse (%d)\n", __func__, re.re_dur);
		    }
                }
		if (found) {
			DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: Found bin5 radar\n", __func__);
			retval |= found;
			goto dfsfound;
		}
		tabledepth = 0;
		rf = NULL;
		DFS_DPRINTK(sc, ATH_DEBUG_DFS1,"  *** chan freq (%d): ts %llu dur %u rssi %u\n",
			rs->rs_chan.channel, (unsigned long long)this_ts, re.re_dur, re.re_rssi);

		while ((tabledepth < DFS_MAX_RADAR_OVERLAP) &&
		       ((dfs->dfs_radartable[re.re_dur])[tabledepth] != -1) &&
		       (!retval)) {
			ft = dfs->dfs_radarf[((dfs->dfs_radartable[re.re_dur])[tabledepth])];
			DFS_DPRINTK(sc, ATH_DEBUG_DFS2,"  ** RD (%d): ts %x dur %u rssi %u\n",
				       rs->rs_chan.channel,
				       re.re_ts, re.re_dur, re.re_rssi);

			if (re.re_rssi < ft->ft_rssithresh && re.re_dur > 4) {
	        		DFS_DPRINTK(sc, ATH_DEBUG_DFS2,"%s : Rejecting on rssi rssi=%u thresh=%u\n", __func__, re.re_rssi, ft->ft_rssithresh);
				tabledepth++;
				ATH_DFSQ_LOCK(dfs);
				empty = STAILQ_EMPTY(&(dfs->dfs_radarq));
				ATH_DFSQ_UNLOCK(dfs);
				continue;
			}
			deltaT = this_ts - ft->ft_last_ts;
			DFS_DPRINTK(sc, ATH_DEBUG_DFS2,"deltaT = %lld (ts: 0x%llx) (last ts: 0x%llx)\n",(unsigned long long)deltaT, (unsigned long long)this_ts, (unsigned long long)ft->ft_last_ts);



			if ((deltaT < ft->ft_minpri) && (deltaT !=0)){
                                /* This check is for the whole filter type. Individual filters
                                 will check this again. This is first line of filtering.*/
				DFS_DPRINTK(sc, ATH_DEBUG_DFS2, "%s: Rejecting on pri pri=%lld minpri=%u\n", __func__, (unsigned long long)deltaT, ft->ft_minpri);
                                tabledepth++;
				continue;
			}
			for (p=0, found = 0; (p<ft->ft_numfilters) && (!found); p++) {
				    rf = &(ft->ft_filters[p]);
                                    if ((re.re_dur >= rf->rf_mindur) && (re.re_dur <= rf->rf_maxdur)) {
                                        /* The above check is probably not necessary */
                                        deltaT = this_ts - rf->rf_dl.dl_last_ts;
                                        /* No need to check for deltaT < 0, since  deltaT is unsigned int */
                                        if ((deltaT < rf->rf_minpri) && (deltaT != 0)) {
                                                /* Second line of PRI filtering. */
                                                DFS_DPRINTK(sc, ATH_DEBUG_DFS2,
                                                "filterID %d : Rejecting on individual filter min PRI deltaT=%lld rf->rf_minpri=%u\n",
                                                rf->rf_pulseid, (unsigned long long)deltaT, rf->rf_minpri);
                                                continue;
                                        }

                                        if ( (deltaT > (2 * rf->rf_maxpri) ) || (deltaT < rf->rf_minpri) ) {
                                                DFS_DPRINTK(sc, ATH_DEBUG_DFS2,
                                                "filterID %d : Rejecting on individual filter max PRI deltaT=%lld rf->rf_minpri=%u\n",
                                                rf->rf_pulseid, (unsigned long long)deltaT, rf->rf_minpri);
                                                /* But update the last time stamp */
                                                rf->rf_dl.dl_last_ts = this_ts;
                                                continue;
                                        }
                                        dfs_add_pulse(sc, rf, &re, deltaT, this_ts);

                                        /* If this is an extension channel event, flag it for false alarm reduction */
        		                if (re.re_chanindex == dfs->dfs_extchan_radindex)
                                            ext_chan_event_flag = 1;

                                        if (rf->rf_patterntype == 2)
                                            found = dfs_staggered_check(sc, rf, (u_int32_t) deltaT, re.re_dur);
                                       else
                                         found = dfs_bin_check(sc, rf, (u_int32_t) deltaT, re.re_dur, ext_chan_event_flag);
                                        if ((sc->sc_debug) & ATH_DEBUG_DFS2) {
                                                dfs_print_delayline(sc, &rf->rf_dl);
                                        }
                                        rf->rf_dl.dl_last_ts = this_ts;
                                }
                            } 
			ft->ft_last_ts = this_ts;
			retval |= found;
			if (found) {
				DFS_DPRINTK(sc, ATH_DEBUG_DFS,
					"Found on channel minDur = %d, filterId = %d\n",ft->ft_mindur,
					rf != NULL ? rf->rf_pulseid : -1);
                        }
			tabledepth++;
		}
		ATH_DFSQ_LOCK(dfs);
		empty = STAILQ_EMPTY(&(dfs->dfs_radarq));
		ATH_DFSQ_UNLOCK(dfs);
	}
dfsfound:
	if (retval) {
                /* Collect stats */
                dfs->ath_dfs_stats.num_radar_detects++;
		thischan = &rs->rs_chan;
		DFS_DPRINTK(sc, ATH_DEBUG_DFS, "Found on channel %d\n", thischan->channel);
                /* If radar is detected in 40MHz mode, add both the primary and the 
                   extension channels to the NOL. chan is the channel data we return 
                   to the ath_dev layer which passes it on to the 80211 layer. 
                   As we want the AP to change channels and send out a CSA, 
                   we always pass back the primary channel data to the ath_dev layer.*/
                chan->channel = sc->sc_curchan.channel;
		chan->channelFlags = sc->sc_curchan.channelFlags;
		chan->privFlags  = sc->sc_curchan.privFlags;
		chan->maxRegTxPower = sc->sc_curchan.maxRegTxPower;
		chan->privFlags |= CHANNEL_INTERFERENCE;

		if ((dfs->dfs_rinfo.rn_use_nol == 1) &&
		    (sc->sc_opmode == HAL_M_HOSTAP || sc->sc_opmode == HAL_M_IBSS)) {
                        HAL_CHANNEL *ext_ch;
			/* If HT40 mode, always add both primary and extension channels 
                           to the NOL irrespective of where radar was detected */    
			sc->sc_curchan.privFlags |= CHANNEL_INTERFERENCE;
			dfs_nol_addchan(sc, &sc->sc_curchan, DFS_NOL_TIMEOUT_S);

	                ext_ch=ath_hal_get_extension_channel(ah);
                        if (ext_ch) {
			            sc->sc_extchan.privFlags |= CHANNEL_INTERFERENCE;
        			    dfs_nol_addchan(sc, &sc->sc_extchan, DFS_NOL_TIMEOUT_S);
                        } 
                }
		/* Disable radar for now */
		rfilt = ath_hal_getrxfilter(ah);
		rfilt &= ~HAL_RX_FILTER_PHYRADAR;
		ath_hal_setrxfilter(ah, rfilt);

		dfs_reset_radarq(sc);
		dfs_reset_alldelaylines(sc);
		/* XXX Should we really enable again? Maybe not... */
		pe.pe_firpwr = rs->rs_firpwr;
		pe.pe_rrssi = rs->rs_radarrssi;
		pe.pe_height = rs->rs_height;
		pe.pe_prssi = rs->rs_pulserssi;
		pe.pe_inband = rs->rs_inband;
		/* 5413 specific */
		pe.pe_relpwr = rs->rs_relpwr;
		pe.pe_relstep = rs->rs_relstep;
		pe.pe_maxlen = rs->rs_maxlen;

		ath_hal_enabledfs(ah, &pe);
		rfilt |= HAL_RX_FILTER_PHYRADAR;
		ath_hal_setrxfilter(ah, rfilt);

                DFS_DPRINTK(sc, ATH_DEBUG_DFS1, "Primary channel freq = %u flags=0x%x\n", chan->channel, chan->privFlags);
                if ((sc->sc_curchan.channel != thischan->channel)) {
                    DFS_DPRINTK(sc, ATH_DEBUG_DFS1, "Ext channel freq = %u flags=0x%x\n", thischan->channel, thischan->privFlags);
                }
	}
	return retval;
}

#endif /* ATH_SUPPORT_DFS */
