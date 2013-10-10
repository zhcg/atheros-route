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
void
ath_process_phyerr(struct ath_softc *sc, struct ath_buf *bf, struct ath_rx_status *rxs, u_int64_t fulltsf)
{
#define EXT_CH_RADAR_FOUND 0x02
#define PRI_CH_RADAR_FOUND 0x01
#define EXT_CH_RADAR_EARLY_FOUND 0x04
        struct ath_dfs *dfs=sc->sc_dfs;
	HAL_CHANNEL *chan=&sc->sc_curchan;
	struct dfs_event *event;
	u_int8_t rssi;
	u_int8_t ext_rssi=0;
        u_int8_t pulse_bw_info=0, pulse_length_ext=0, pulse_length_pri=0;
	u_int32_t dur=0;
        u_int16_t datalen;
        int pri_found=1, ext_found=0, dc_found=0, early_ext=0, slope=0, add_dur=0;

	int empty;
        u_int32_t *last_word_ptr, *secondlast_word_ptr;
        u_int8_t *byte_ptr, last_byte_0, last_byte_1, last_byte_2, last_byte_3; 
        u_int8_t secondlast_byte_0, secondlast_byte_1, secondlast_byte_2, secondlast_byte_3; 

	if (((rxs->rs_phyerr != HAL_PHYERR_RADAR)) &&
	    ((rxs->rs_phyerr != HAL_PHYERR_FALSE_RADAR_EXT)))  {
		DFS_DPRINTK(sc, ATH_DEBUG_DFS3, "%s: rs_phyer=0x%x not a radar error\n",__func__, rxs->rs_phyerr);
        	return;
        }

        /* 
           At this time we have a radar pulse that we need to examine and queue. 
           But if dfs_process_radarevent already detected radar and set  
           CHANNEL_INTERFERENCE flag then do not queue any more radar data.
           When we are in a new channel this flag will be clear and we will
           start queueing data for new channel. (EV74162)
        */

        if (chan->privFlags & CHANNEL_INTERFERENCE) {
                DFS_DPRINTK(sc, ATH_DEBUG_DFS1, "%s: Radar already found in the channel, do not queue radar data\n", __func__);
                return;
        } 

	if (dfs == NULL) {
		DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: sc_dfs is NULL\n",__func__);
		return;
	}
        dfs->ath_dfs_stats.total_phy_errors++;
        datalen = rxs->rs_datalen;
        /* WAR: Never trust combined RSSI on radar pulses for <=
         * OWL2.0. For short pulses only the chain 0 rssi is present
         * and remaining descriptor data is all 0x80, for longer
         * pulses the descriptor is present, but the combined value is
         * inaccurate. This HW capability is queried in dfs_attach and stored in
         * the sc_dfs_combined_rssi_ok flag.*/

        if (sc->sc_dfs->sc_dfs_combined_rssi_ok) {
                rssi = (u_int8_t) rxs->rs_rssi;
        } else {
            rssi = (u_int8_t) rxs->rs_rssi_ctl0;
        }

        ext_rssi = (u_int8_t) rxs->rs_rssi_ext0;


        /* hardware stores this as 8 bit signed value.
         * we will cap it at 0 if it is a negative number 
         */

        if (rssi & 0x80)
            rssi = 0;

        if (ext_rssi & 0x80)
            ext_rssi = 0;

        last_word_ptr = (u_int32_t *)(((u_int8_t*)bf->bf_vdata) + datalen - (datalen%4));


        secondlast_word_ptr = last_word_ptr-1;

        byte_ptr = (u_int8_t*)last_word_ptr; 
        last_byte_0=(*(byte_ptr) & 0xff); 
        last_byte_1=(*(byte_ptr+1) & 0xff); 
        last_byte_2=(*(byte_ptr+2) & 0xff); 
        last_byte_3=(*(byte_ptr+3) & 0xff); 

        byte_ptr = (u_int8_t*)secondlast_word_ptr; 
        secondlast_byte_0=(*(byte_ptr) & 0xff); 
        secondlast_byte_1=(*(byte_ptr+1) & 0xff); 
        secondlast_byte_2=(*(byte_ptr+2) & 0xff); 
        secondlast_byte_3=(*(byte_ptr+3) & 0xff); 
       
        /* If radar can be detected on the extension channel (for SOWL onwards), we have to read radar data differently as the HW supplies bwinfo and duration for both primary and extension channel.*/
        if (sc->sc_dfs->sc_dfs_ext_chan_ok) {
        
        /* If radar can be detected on the extension channel, datalen zero pulses are bogus, discard them.*/
        if (!datalen) {
            dfs->ath_dfs_stats.datalen_discards++;
            return;
        }
        switch((datalen & 0x3)) {
        case 0:
            pulse_bw_info = secondlast_byte_3;
            pulse_length_ext = secondlast_byte_2;
            pulse_length_pri = secondlast_byte_1;
            break;
        case 1:
            pulse_bw_info = last_byte_0;
            pulse_length_ext = secondlast_byte_3;
            pulse_length_pri = secondlast_byte_2;
            break;
        case 2:
            pulse_bw_info = last_byte_1;
            pulse_length_ext = last_byte_0;
            pulse_length_pri = secondlast_byte_3;
           break;
        case 3:
            pulse_bw_info = last_byte_2;
            pulse_length_ext = last_byte_1;
            pulse_length_pri = last_byte_0;
            break;
        default:
            DFS_DPRINTK(sc, ATH_DEBUG_DFS, "datalen mod4=%d\n", (datalen%4));
        }

        /* Only the last 3 bits of the BW info are relevant, they indicate
        which channel the radar was detected in.*/
        pulse_bw_info &= 0x07;
        /* If pulse on DC, both primary and extension flags will be set */
        if (((pulse_bw_info & EXT_CH_RADAR_FOUND) && (pulse_bw_info & PRI_CH_RADAR_FOUND))) {

            /* Conducted testing, when pulse is on DC, both pri and ext durations are reported to be same
               Radiated testing, when pulse is on DC, different pri and ext durations are reported, so take the larger of the two */
            if (pulse_length_ext >= pulse_length_pri) {
                dur = pulse_length_ext;
                ext_found = 1;
            } else {
                dur = pulse_length_pri;
                pri_found = 1;
            }
            dfs->ath_dfs_stats.dc_phy_errors++;         

        } else {
        if (pulse_bw_info & EXT_CH_RADAR_FOUND) {
            dur = pulse_length_ext;
            pri_found = 0;
            ext_found = 1;
            dfs->ath_dfs_stats.ext_phy_errors++;         
        } 
        if (pulse_bw_info & PRI_CH_RADAR_FOUND) {
            dur = pulse_length_pri;
            pri_found = 1;
            ext_found = 0;
            dfs->ath_dfs_stats.pri_phy_errors++;         
        } 
        if (pulse_bw_info & EXT_CH_RADAR_EARLY_FOUND) {
             dur = pulse_length_ext;
             pri_found = 0;
             ext_found = 1; early_ext = 1;
             dfs->ath_dfs_stats.early_ext_phy_errors++;         
	     DFS_DPRINTK(sc, ATH_DEBUG_DFS2, "EARLY ext channel dur=%u rssi=%u datalen=%d\n",dur, rssi, datalen);
        } 
        if (!pulse_bw_info) {
	    DFS_DPRINTK(sc, ATH_DEBUG_DFS3, "ERROR channel dur=%u rssi=%u pulse_bw_info=0x%x datalen MOD 4 = %d\n",dur, rssi, pulse_bw_info, (datalen & 0x3));
            /* Bogus bandwidth info received in descriptor, 
            so ignore this PHY error */
            dfs->ath_dfs_stats.bwinfo_errors++;
            return; 
        }
    }

    if (sc->sc_dfs->sc_dfs_use_enhancement) {
        /*
         * for osprey (and Merlin) bw_info has implication for selecting RSSI value
         */

        switch (pulse_bw_info & 0x03) {
        case 0x00:
            /* No radar in ctrl or ext channel */
            rssi = 0;
            break;
        case 0x01:
            /* radar in ctrl channel */
            DFS_DPRINTK(sc, ATH_DEBUG_DFS3, "RAW RSSI: rssi=%u ext_rssi=%u\n", rssi, ext_rssi);
            if (ext_rssi >= (rssi + 3)) {
                /* cannot use ctrl channel RSSI if extension channel is stronger */
                rssi = 0;
            }
            break;
        case 0x02:
            /* radar in extension channel */
	    
            DFS_DPRINTK(sc, ATH_DEBUG_DFS3, "RAW RSSI: rssi=%u ext_rssi=%u\n", rssi, ext_rssi);
            if (rssi >= (ext_rssi + 12)) {
                /* cannot use extension channel RSSI if control channel is stronger */
                rssi = 0;
            } else {
                rssi = ext_rssi;
            }
            break;
        case 0x03:
           /* when both are present use stronger one */
            if (rssi < ext_rssi) {
                rssi = ext_rssi;
            }
            /* EV -- 107365 to compensate for the roll off of the filter if radar pulse is at DC */
            rssi += 7;
            break;
        } 
    } else {
        /* Always use combined RSSI reported, unless RSSI reported on 
           extension is stronger */
        if ((ext_rssi > rssi) && (ext_rssi < 128)) {
                    rssi = ext_rssi;
        }
    }
        DFS_DPRINTK(sc, ATH_DEBUG_DFS1, "pulse_bw_info=0x%x pulse_length_ext=%u pulse_length_pri=%u rssi=%u ext_rssi=%u phyerr=0x%x\n", pulse_bw_info, pulse_length_ext, pulse_length_pri, rssi, ext_rssi, rxs->rs_phyerr);

        /* HW has a known issue with chirping pulses injected at or around DC in 40MHz 
           mode. Such pulses are reported with much lower durations and SW then discards 
           them because they do not fit the minimum bin5 pulse duration.

           To work around this issue, if a pulse is within a 10us range of the 
           bin5 min duration, check if the pulse is chirping. If the pulse is chirping,
           bump up the duration to the minimum bin5 duration. 

           This makes sure that a valid chirping pulse will not be discarded because of 
           incorrect low duration.

            TBD - Is it possible to calculate the 'real' duration of the pulse using the 
            slope of the FFT data?
            TBD - Use FFT data to differentiate between radar pulses and false PHY errors.
            This will let us reduce the number of false alarms seen.

            BIN 5 chirping pulses are only for FCC or Japan MMK4 domain
        */
        if (((dfs->dfsdomain == DFS_FCC_DOMAIN) || (dfs->dfsdomain == DFS_MKK4_DOMAIN)) && 
            (dur >= MAYBE_BIN5_DUR) && (dur < MAX_BIN5_DUR)) {
            add_dur = dfs_check_chirping(sc, bf, rxs, pri_found, ext_found, &slope, &dc_found);
            if (add_dur) {
                DFS_DPRINTK(sc, ATH_DEBUG_DFS2, "old dur %d slope =%d\n", dur, slope);
                    // bump up to a random bin5 pulse duration
                    if (dur < MIN_BIN5_DUR) {
                        dur = dfs_get_random_bin5_dur(sc, fulltsf);
                    }
                DFS_DPRINTK(sc, ATH_DEBUG_DFS2, "new dur %d\n", dur);
            } else {
                dur = MAX_BIN5_DUR + 100;       /* set the duration so that it is rejected */
                DFS_DPRINTK(sc, ATH_DEBUG_DFS2,"is_chirping = %d dur=%d \n", add_dur, dur);
            }
        } else {
            /* we have a pulse that is either bigger than MAX_BIN5_DUR or 
             * less than MAYBE_BIN5_DUR 
             */
            if ((dfs->dfsdomain == DFS_FCC_DOMAIN) || (dfs->dfsdomain == DFS_MKK4_DOMAIN)) {
                if (dur >= MAX_BIN5_DUR) {
                    dur = MAX_BIN5_DUR + 50;        /* set the duration so that it is rejected */
                }
            }
        }
    } else {
        dfs->ath_dfs_stats.owl_phy_errors++;
     /* HW cannot detect extension channel radar so it only passes us primary channel radar data*/
            dur = (rxs->rs_datalen && bf->bf_vdata != NULL ?
               (u_int32_t)(*((u_int8_t *) bf->bf_vdata)) : 0) & 0xff;

            if ((rssi == 0) && (dur== 0)){
               return;
            }
            pri_found = 1;
            ext_found = 0;
        }

	ATH_DFSEVENTQ_LOCK(dfs);
	empty = STAILQ_EMPTY(&(dfs->dfs_eventq));
	ATH_DFSEVENTQ_UNLOCK(dfs);
	if (empty) {
		return;
        }

	if ((chan->channelFlags & CHANNEL_108G) == CHANNEL_108G) {
	        if (!(dfs->dfs_proc_phyerr & DFS_AR_EN)) {
                		DFS_DPRINTK(sc, ATH_DEBUG_DFS2, "%s: DFS_AR_EN not enabled\n",
				__func__);
                                return;
                }
		ATH_DFSEVENTQ_LOCK(dfs);
		event = STAILQ_FIRST(&(dfs->dfs_eventq));
		if (event == NULL) {
			ATH_DFSEVENTQ_UNLOCK(dfs);
			DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: no more events space left\n",
				__func__);
			return;
		}
		STAILQ_REMOVE_HEAD(&(dfs->dfs_eventq), re_list);
		ATH_DFSEVENTQ_UNLOCK(dfs);
		event->re_rssi = rssi;
		event->re_dur = dur;
		event->re_full_ts = fulltsf;
		event->re_ts = (rxs->rs_tstamp) & DFS_TSMASK;
        	event->re_chanindex = dfs->dfs_curchan_radindex;
		ATH_ARQ_LOCK(dfs);
		STAILQ_INSERT_TAIL(&(dfs->dfs_arq), event, re_list);
		ATH_ARQ_UNLOCK(dfs);
	}
	else {
		if (chan->privFlags & CHANNEL_DFS) {
	                if (!(dfs->dfs_proc_phyerr & DFS_RADAR_EN)) {
                		DFS_DPRINTK(sc, ATH_DEBUG_DFS3, "%s: DFS_RADAR_EN not enabled\n",
				__func__);
                                return;
                        }
                        /* rssi is not accurate for short pulses, so do not filter based on that for short duration pulses*/
                        if (sc->sc_dfs->sc_dfs_ext_chan_ok) {
			    if ((rssi < dfs->dfs_rinfo.rn_minrssithresh && (dur > 4))||
			        dur > (dfs->dfs_rinfo.rn_maxpulsedur) ) {
                                    dfs->ath_dfs_stats.rssi_discards++;
                		    DFS_DPRINTK(sc, ATH_DEBUG_DFS3, "Extension channel pulse is discarded %d, %d, %d, %d\n", 
						dur, dfs->dfs_rinfo.rn_maxpulsedur, rssi, dfs->dfs_rinfo.rn_minrssithresh);
				    return;
                            }
                        } else {

			    if (rssi < dfs->dfs_rinfo.rn_minrssithresh ||
			        dur > dfs->dfs_rinfo.rn_maxpulsedur) {
                                    dfs->ath_dfs_stats.rssi_discards++;
				    return;
                            }
                        }

			ATH_DFSEVENTQ_LOCK(dfs);
			event = STAILQ_FIRST(&(dfs->dfs_eventq));
			if (event == NULL) {
				ATH_DFSEVENTQ_UNLOCK(dfs);
				DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: no more events space left\n",
					__func__);
				return;
			}
			STAILQ_REMOVE_HEAD(&(dfs->dfs_eventq), re_list);
			ATH_DFSEVENTQ_UNLOCK(dfs);
			event->re_dur = dur;
			event->re_full_ts = fulltsf;
			event->re_ts = (rxs->rs_tstamp) & DFS_TSMASK;
			event->re_rssi = rssi;
                        if (pri_found == 1) {
        		    event->re_chanindex = dfs->dfs_curchan_radindex;
                        } else {
                            if (dfs->dfs_extchan_radindex == -1) { 
                                DFS_DPRINTK(sc, ATH_DEBUG_DFS3, "%s - phyerr on ext channel\n", __func__);
                            }
        		    event->re_chanindex = dfs->dfs_extchan_radindex;
                            DFS_DPRINTK(sc, ATH_DEBUG_DFS3, "%s New extension channel event is added to queue\n",__func__);

                        }
			ATH_DFSQ_LOCK(dfs);
			STAILQ_INSERT_TAIL(&(dfs->dfs_radarq), event, re_list);
			ATH_DFSQ_UNLOCK(dfs);
		}
    }
#undef EXT_CH_RADAR_FOUND
#undef PRI_CH_RADAR_FOUND
#undef EXT_CH_RADAR_EARLY_FOUND
}

#endif /* ATH_SUPPORT_DFS */
