/*
 * Copyright (c) 2002-2006, Atheros Communications Inc.
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
#include <osdep.h>
#include "sys/queue.h"
#ifdef __NetBSD__
#include <net/if_media.h>
#endif


#include "if_athioctl.h"
#include "if_athvar.h"
#include "dfs_ioctl.h"
#include "dfs.h"

int domainoverride=DFS_UNINIT_DOMAIN;

/*
 ** channel switch announcement (CSA)
 ** usenol=1 (default) make CSA and switch to a new channel on radar detect
 ** usenol=0, make CSA with next channel same as current on radar detect
 ** usenol=2, no CSA and stay on the same channel on radar detect
 **/

int usenol=1;
u_int32_t dfs_debug_level=ATH_DEBUG_DFS;


int
dfs_attach(struct ath_softc *sc)
{
	int i, n;
	struct ath_dfs *dfs = sc->sc_dfs;
#define	N(a)	(sizeof(a)/sizeof(a[0]))

	if (dfs != NULL) {
		DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: sc_dfs was not NULL\n",
			__func__);
		return 1;
	}
	dfs = (struct ath_dfs *)OS_MALLOC(sc->sc_osdev, sizeof(struct ath_dfs), GFP_KERNEL);
	if (dfs == NULL) {
		DFS_DPRINTK(sc, ATH_DEBUG_DFS,
			"%s: ath_dfs allocation failed\n", __func__);
		return 1;
	}

	OS_MEMZERO(dfs, sizeof (struct ath_dfs));
        sc->sc_dfs = dfs;
        dfs->dfs_nol=NULL;
        dfs_clear_stats(sc);

        /* Get capability information - can extension channel radar be detected and should we use combined radar RSSI or not.*/
        if (ath_hal_getcapability(sc->sc_ah, HAL_CAP_COMBINED_RADAR_RSSI, 0, 0) 
                                   == HAL_OK) {
                sc->sc_dfs->sc_dfs_combined_rssi_ok = 1;
        } else {
                sc->sc_dfs->sc_dfs_combined_rssi_ok = 0;
        }
        if (ath_hal_getcapability(sc->sc_ah, HAL_CAP_EXT_CHAN_DFS, 0, 0) 
                                    == HAL_OK) {
            sc->sc_dfs->sc_dfs_ext_chan_ok = 1;
        } else {
            sc->sc_dfs->sc_dfs_ext_chan_ok = 0;
        }

        if (ath_hal_hasenhanceddfssupport(sc->sc_ah)) {
            sc->sc_dfs->sc_dfs_use_enhancement = 1;
            DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: use DFS enhancements\n", __func__);
        } else {
            sc->sc_dfs->sc_dfs_use_enhancement = 0;
        }
sc->sc_dfs->sc_dfs_cac_time = ATH_DFS_WAIT_MS;
        sc->sc_dfs->sc_dfstesttime = ATH_DFS_TEST_RETURN_PERIOD_MS;
	ATH_DFSQ_LOCK_INIT(dfs);
	STAILQ_INIT(&dfs->dfs_radarq);
	ATH_ARQ_LOCK_INIT(dfs);
	STAILQ_INIT(&dfs->dfs_arq);
	STAILQ_INIT(&(dfs->dfs_eventq));
	ATH_DFSEVENTQ_LOCK_INIT(dfs);
	dfs->events = (struct dfs_event *)OS_MALLOC(sc->sc_osdev,
                       sizeof(struct dfs_event)*DFS_MAX_EVENTS,
                       GFP_KERNEL);
	if (dfs->events == NULL) {
		OS_FREE(dfs);
                sc->sc_dfs = NULL;
		DFS_DPRINTK(sc, ATH_DEBUG_DFS,
			"%s: events allocation failed\n", __func__);
		return 1;
	}
	for (i=0; i<DFS_MAX_EVENTS; i++) {
		STAILQ_INSERT_TAIL(&(dfs->dfs_eventq), &dfs->events[i], re_list);
	}

        dfs->pulses = (struct dfs_pulseline *)OS_MALLOC(sc->sc_osdev, sizeof(struct dfs_pulseline), GFP_KERNEL);

        if (dfs->pulses == NULL) {
                OS_FREE(dfs->events);   
                dfs->events = NULL;
                OS_FREE(dfs);
                sc->sc_dfs = NULL;
                DFS_DPRINTK(sc, ATH_DEBUG_DFS,
                        "%s: pulse buffer allocation failed\n", __func__);
                return 1;
        }

        dfs->pulses->pl_lastelem = DFS_MAX_PULSE_BUFFER_MASK;
#ifdef ATH_ENABLE_AR
	if (ath_hal_getcapability(sc->sc_ah, HAL_CAP_PHYDIAG,
				  HAL_CAP_AR, NULL) == HAL_OK) {
		dfs_reset_ar(sc);
		dfs_reset_arq(sc);
		dfs->dfs_proc_phyerr |= DFS_AR_EN;
	}
#endif
	if (ath_hal_getcapability(sc->sc_ah, HAL_CAP_PHYDIAG,
				  HAL_CAP_RADAR, NULL) == HAL_OK) {
		u_int32_t val;
		/* 
		 * If we have fast diversity capability, read off
		 * Strong Signal fast diversity count set in the ini
		 * file, and store so we can restore the value when
		 * radar is disabled
		 */
		if (ath_hal_getcapability(sc->sc_ah, HAL_CAP_DIVERSITY, HAL_CAP_STRONG_DIV,
					  &val) == HAL_OK) {
			dfs->dfs_rinfo.rn_fastdivGCval = val;
		}
		dfs->dfs_proc_phyerr |= DFS_RADAR_EN;

                /* Allocate memory for radar filters */
		for (n=0; n<DFS_MAX_RADAR_TYPES; n++) {
			dfs->dfs_radarf[n] = (struct dfs_filtertype *)OS_MALLOC(sc->sc_osdev, sizeof(struct dfs_filtertype),GFP_KERNEL);
			if (dfs->dfs_radarf[n] == NULL) {
				DFS_DPRINTK(sc,ATH_DEBUG_DFS,
					"%s: cannot allocate memory for radar filter types\n",
					__func__);
				goto bad1;
			}
			OS_MEMZERO(dfs->dfs_radarf[n], sizeof(struct dfs_filtertype));  
		}
                /* Allocate memory for radar table */
		dfs->dfs_radartable = (int8_t **)OS_MALLOC(sc->sc_osdev, 256*sizeof(int8_t *), GFP_KERNEL);
		if (dfs->dfs_radartable == NULL) {
			DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: cannot allocate memory for radar table\n",
				__func__);
			goto bad1;
		}
		for (n=0; n<256; n++) {
			dfs->dfs_radartable[n] = OS_MALLOC(sc->sc_osdev, DFS_MAX_RADAR_OVERLAP*sizeof(int8_t),
							 GFP_KERNEL);
			if (dfs->dfs_radartable[n] == NULL) {
				DFS_DPRINTK(sc, ATH_DEBUG_DFS,
					"%s: cannot allocate memory for radar table entry\n",
					__func__);
				goto bad2;
			}
		}
		if (usenol != 1) {
			DFS_DPRINTK(sc, ATH_DEBUG_DFS, " %s: Disabling Channel NOL\n", __func__);
                }
		dfs->dfs_rinfo.rn_use_nol = usenol;

                /* Init the cached extension channel busy for false alarm reduction */
	        dfs->dfs_rinfo.ext_chan_busy_ts = ath_hal_gettsf64(sc->sc_ah);
                dfs->dfs_rinfo.dfs_ext_chan_busy = 0;
                /* Init the Bin5 chirping related data */
                dfs->dfs_rinfo.dfs_bin5_chirp_ts = dfs->dfs_rinfo.ext_chan_busy_ts;
                dfs->dfs_rinfo.dfs_last_bin5_dur = MAX_BIN5_DUR;

                dfs->dfs_b5radars = NULL;
                if ( dfs_init_radar_filters( sc ) ) {
			DFS_DPRINTK(sc, ATH_DEBUG_DFS, 
                            " %s: Radar Filter Intialization Failed \n", 
                            __func__);
                    return 1;
                }
	}
	return 0;
bad2:
	OS_FREE(dfs->dfs_radartable);
	dfs->dfs_radartable = NULL;
bad1:	
        for (n=0; n<DFS_MAX_RADAR_TYPES; n++) {
		if (dfs->dfs_radarf[n] != NULL) {
			OS_FREE(dfs->dfs_radarf[n]);
			dfs->dfs_radarf[n] = NULL;
		}
	}
        if (dfs->pulses) {
		OS_FREE(dfs->pulses);
		dfs->pulses = NULL;
	}
	if (dfs->events) {
		OS_FREE(dfs->events);
		dfs->events = NULL;
	}

	if (sc->sc_dfs) {
		OS_FREE(sc->sc_dfs);
		sc->sc_dfs = NULL;
	}
	return 1;
#undef N
}

void
dfs_detach(struct ath_softc *sc)
{
	struct ath_dfs *dfs = sc->sc_dfs;
	int n, empty;

	if (dfs == NULL) {
	        DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: sc_dfs is NULL\n",
			__func__);
		return;
	}
  
        /* Bug 29099 make sure all outstanding timers are cancelled*/

        if (dfs->sc_rtasksched) {
            OS_CANCEL_TIMER(&dfs->sc_dfs_task_timer);
            dfs->sc_rtasksched = 0;
        }
        if (dfs->sc_dfswait) {
            OS_CANCEL_TIMER(&dfs->sc_dfswaittimer);
            dfs->sc_dfswait = 0;
        }
        if (dfs->sc_dfstest) {
            OS_CANCEL_TIMER(&dfs->sc_dfstesttimer);
            dfs->sc_dfstest = 0;
        }
        
        OS_CANCEL_TIMER(&dfs->sc_dfs_war_timer);

	if (dfs->dfs_nol != NULL) {
	    struct dfs_nolelem *nol, *next;
	    nol = dfs->dfs_nol;
                /* Bug 29099 - each NOL element has its own timer, cancel it and 
                   free the element*/
		while (nol != NULL) {
                       OS_CANCEL_TIMER(&nol->nol_timer);
		       next = nol->nol_next;
		       OS_FREE(nol);
		       nol = next;
		}
		dfs->dfs_nol = NULL;
	}
        /* Return radar events to free q*/
        dfs_reset_radarq(sc);
	dfs_reset_alldelaylines(sc);

        /* Free up pulse log*/
        if (dfs->pulses != NULL) {
                OS_FREE(dfs->pulses);
                dfs->pulses = NULL;
        }

	for (n=0; n<DFS_MAX_RADAR_TYPES;n++) {
		if (dfs->dfs_radarf[n] != NULL) {
			OS_FREE(dfs->dfs_radarf[n]);
			dfs->dfs_radarf[n] = NULL;
		}
	}


	if (dfs->dfs_radartable != NULL) {
		for (n=0; n<256; n++) {
			if (dfs->dfs_radartable[n] != NULL) {
				OS_FREE(dfs->dfs_radartable[n]);
				dfs->dfs_radartable[n] = NULL;
			}
		}
		OS_FREE(dfs->dfs_radartable);
		dfs->dfs_radartable = NULL;
		dfs->sc_dfs_isdfsregdomain = 0;
	}
        
	if (dfs->dfs_b5radars != NULL) {
		OS_FREE(dfs->dfs_b5radars);
		dfs->dfs_b5radars=NULL;
	}

	dfs_reset_ar(sc);

	ATH_ARQ_LOCK(dfs);
	empty = STAILQ_EMPTY(&(dfs->dfs_arq));
	ATH_ARQ_UNLOCK(dfs);
	if (!empty) {
		dfs_reset_arq(sc);
	}
        if (dfs->events != NULL) {
                OS_FREE(dfs->events);
                dfs->events = NULL;
        }
	OS_FREE(dfs);
        sc->sc_dfs = NULL;
}

/* need confirmation that these are ETSI except for weather channel */    
/* make sure these are consistent with definition in ah_regdomain.h */

#define    CTRY_KOREA_ROC           410     /* South Korea */
#define    CTRY_KOREA_ROC3          412     /* South Korea */


int dfs_radar_enable(struct ath_softc *sc)
{
	struct ath_dfs *dfs=sc->sc_dfs;
	u_int32_t rfilt;
	HAL_CHANNEL *chan=&sc->sc_curchan;
	struct ath_hal *ah = sc->sc_ah;
	HAL_CHANNEL *ext_ch=ath_hal_get_extension_channel(ah);
        HAL_CTRY_CODE cc;

	if (dfs == NULL) {
		DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: sc_dfs is NULL\n",
			__func__);
		return -EIO;
	}
	rfilt = ath_hal_getrxfilter(ah);
	if ((dfs->dfs_proc_phyerr & (DFS_RADAR_EN | DFS_AR_EN)) &&
	    (sc->sc_opmode == HAL_M_HOSTAP || sc->sc_opmode == HAL_M_IBSS)) {
            	if (chan->privFlags & CHANNEL_DFS) {
			struct dfs_state *rs_pri=NULL, *rs_ext=NULL;
			u_int8_t index_pri, index_ext;
			HAL_PHYERR_PARAM pe;
			dfs->sc_dfs_cac_time = ATH_DFS_WAIT_MS;
                        ath_hal_getcountrycode(ah, &cc);
                        if (dfs->dfsdomain == DFS_ETSI_DOMAIN &&
                            (cc != CTRY_KOREA_ROC) &&
                            (cc != CTRY_KOREA_ROC3)) {
                            if(IS_CHANNEL_WEATHER_RADAR(chan->channel)) {
                                dfs->sc_dfs_cac_time = ATH_DFS_WEATHER_CHANNEL_WAIT_MS;
                            } else {
                                 if (ext_ch && IS_CHANNEL_WEATHER_RADAR(ext_ch->channel)) {
                                     dfs->sc_dfs_cac_time = ATH_DFS_WEATHER_CHANNEL_WAIT_MS;
                                 }
                            }
                        }
                        if(dfs->sc_dfs_cac_time != ATH_DFS_WAIT_MS)
                            printk("WARNING!!! 10 minute CAC period as channel is a weather radar channel\n");
			/*
			  Disable radar detection in case we need to setup
			 * a new channel state and radars are somehow being
			 * reported. Avoids locking problem.
			 */

			rfilt = ath_hal_getrxfilter(ah);
			rfilt &= ~HAL_RX_FILTER_PHYRADAR;
			ath_hal_setrxfilter(ah, rfilt);
			/* Enable strong signal fast antenna diversity */
			ath_hal_setcapability(ah, HAL_CAP_DIVERSITY, 
					      HAL_CAP_STRONG_DIV, dfs->dfs_rinfo.rn_fastdivGCval, NULL);
			dfs_reset_alldelaylines(sc);

			rs_pri = dfs_getchanstate(sc, &index_pri, 0);
			if (ext_ch) {
                            rs_ext = dfs_getchanstate(sc, &index_ext, 1);
                            sc->sc_extchan = *ext_ch;
                        }
    			if (rs_pri != NULL && ((ext_ch==NULL)||(rs_ext != NULL))) {
				if (index_pri != dfs->dfs_curchan_radindex)
					dfs_reset_alldelaylines(sc);

				dfs->dfs_curchan_radindex = (int16_t) index_pri;
                                
                                if (rs_ext)
			            dfs->dfs_extchan_radindex = (int16_t) index_ext;

				pe.pe_firpwr = rs_pri->rs_firpwr;
				pe.pe_rrssi = rs_pri->rs_radarrssi;
				pe.pe_height = rs_pri->rs_height;
				pe.pe_prssi = rs_pri->rs_pulserssi;
				pe.pe_inband = rs_pri->rs_inband;
				/* 5413 specific */
				pe.pe_relpwr = rs_pri->rs_relpwr;
				pe.pe_relstep = rs_pri->rs_relstep;
				pe.pe_maxlen = rs_pri->rs_maxlen;


				/* Disable strong signal fast antenna diversity */
				ath_hal_setcapability(ah, HAL_CAP_DIVERSITY,
						      HAL_CAP_STRONG_DIV, 1, NULL);

				ath_hal_enabledfs(sc->sc_ah, &pe);
				rfilt |= HAL_RX_FILTER_PHYRADAR;
				ath_hal_setrxfilter(ah, rfilt);
				DFS_DPRINTK(sc, ATH_DEBUG_DFS, "Enabled radar detection on channel %d\n",
					chan->channel);

                                dfs->dur_multiplier =  (ath_hal_is_fast_clock_enabled(sc->sc_ah) ? (DFS_FAST_CLOCK_MULTIPLIER) : (DFS_NO_FAST_CLOCK_MULTIPLIER));
                    	DFS_DPRINTK(sc, ATH_DEBUG_DFS3,
			"%s: duration multiplier is %d\n", __func__, dfs->dur_multiplier);

				/*
				 * Fast antenna diversity for strong signals disturbs
				 * radar detection of 1-2us pusles. Enable fast diveristy
				 * but disable the strong signal aspect of it
				 */
				if (ath_hal_getcapability(ah, HAL_CAP_DIVERSITY, 1, NULL) == HAL_OK) {
					ath_hal_setcapability(ah, HAL_CAP_DIVERSITY, 
							      HAL_CAP_STRONG_DIV, 0, NULL);
				}
			} else
				DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: No more radar states left\n",
					__func__);
		} else {
			if (!(chan->channelFlags & CHANNEL_2GHZ)) {
				/* Enable strong signal fast antenna diversity */
				ath_hal_setcapability(ah, HAL_CAP_DIVERSITY, 
						      HAL_CAP_STRONG_DIV, dfs->dfs_rinfo.rn_fastdivGCval, NULL);
				/* Disable Radar if not 2GHZ channel and not DFS */
				rfilt &= ~HAL_RX_FILTER_PHYRADAR;
				ath_hal_setrxfilter(ah, rfilt);
			}
		}
	} else {
		/* Enable strong signal fast antenna diversity */
		ath_hal_setcapability(ah, HAL_CAP_DIVERSITY, 
				      HAL_CAP_STRONG_DIV, dfs->dfs_rinfo.rn_fastdivGCval, NULL);
		/* Disable Radar if RADAR or AR not enabled */
		rfilt &= ~HAL_RX_FILTER_PHYRADAR;
		ath_hal_setrxfilter(ah, rfilt);
	}
	return 0;
}


int
dfs_control(ath_dev_t dev, u_int id,
                void *indata, u_int32_t insize,
                void *outdata, u_int32_t *outsize)
{
	struct ath_softc *sc = ATH_DEV_TO_SC(dev);
	int error = 0;
	u_int32_t *data = NULL;
	HAL_PHYERR_PARAM peout;
	struct dfsreq_nolinfo *nol;
	struct ath_dfs *dfs = sc->sc_dfs;
	struct dfs_ioctl_params *dfsparams;
        u_int32_t val=0;

	if (dfs == NULL) {
		error = -EINVAL;
                DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s DFS is null\n", __func__);
		goto bad;
	}

	switch (id) {
	case DFS_MUTE_TIME:
		if (insize < sizeof(u_int32_t) || !indata) {
			error = -EINVAL;
			break;
		}
		data = (u_int32_t *) indata;
		sc->sc_dfs->sc_dfstesttime = *data;
		sc->sc_dfs->sc_dfstesttime *= (1000); //convert sec into ms
		break;
	case DFS_SET_THRESH:
		if (insize < sizeof(HAL_PHYERR_PARAM) || !indata) {
			error = -EINVAL;
			break;
		}
		dfsparams = (struct dfs_ioctl_params *) indata;
		if (!dfs_set_thresholds(sc, DFS_PARAM_FIRPWR, dfsparams->dfs_firpwr))
			error = -EINVAL;
		if (!dfs_set_thresholds(sc, DFS_PARAM_RRSSI, dfsparams->dfs_rrssi))
			error = -EINVAL;
		if (!dfs_set_thresholds(sc, DFS_PARAM_HEIGHT, dfsparams->dfs_height))
			error = -EINVAL;
		if (!dfs_set_thresholds(sc, DFS_PARAM_PRSSI, dfsparams->dfs_prssi))
			error = -EINVAL;
		if (!dfs_set_thresholds(sc, DFS_PARAM_INBAND, dfsparams->dfs_inband))
			error = -EINVAL;
		/* 5413 speicfic */
		if (!dfs_set_thresholds(sc, DFS_PARAM_RELPWR, dfsparams->dfs_relpwr))
			error = -EINVAL;
		if (!dfs_set_thresholds(sc, DFS_PARAM_RELSTEP, dfsparams->dfs_relstep))
			error = -EINVAL;
		if (!dfs_set_thresholds(sc, DFS_PARAM_MAXLEN, dfsparams->dfs_maxlen))
			error = -EINVAL;
		break;
	case DFS_GET_THRESH:
		if (!outdata || !outsize || *outsize <sizeof(struct dfs_ioctl_params)) {
			error = -EINVAL;
			break;
		}
		*outsize = sizeof(struct dfs_ioctl_params);
		ath_hal_getdfsthresh(sc->sc_ah, &peout);
		dfsparams = (struct dfs_ioctl_params *) outdata;
		dfsparams->dfs_firpwr = peout.pe_firpwr;
		dfsparams->dfs_rrssi = peout.pe_rrssi;
		dfsparams->dfs_height = peout.pe_height;
		dfsparams->dfs_prssi = peout.pe_prssi;
		dfsparams->dfs_inband = peout.pe_inband;
		/* 5413 specific */
		dfsparams->dfs_relpwr = peout.pe_relpwr;
		dfsparams->dfs_relstep = peout.pe_relstep;
		dfsparams->dfs_maxlen = peout.pe_maxlen;
                break;
	case DFS_GET_USENOL:
		if (!outdata || !outsize || *outsize < sizeof(u_int32_t)) {
			error = -EINVAL;
			break;
		}
		*outsize = sizeof(u_int32_t);
		*((u_int32_t *)outdata) = dfs->dfs_rinfo.rn_use_nol;
		break;
	case DFS_SET_USENOL:
		if (insize < sizeof(u_int32_t) || !indata) {
			error = -EINVAL;
			break;
		}
		dfs->dfs_rinfo.rn_use_nol = *(u_int32_t *)indata;
		/* iwpriv markdfs in linux can do the same thing... */
		break;
        case DFS_SHOW_NOL:
                dfs_print_nol(sc);
                break;
	case DFS_BANGRADAR:
                if(sc->sc_nostabeacons)
                {
                        printk("No radar detection Enabled \n");
                        break;
                }
		dfs->dfs_bangradar = 1;     
                sc->sc_rtasksched = 1;
                OS_SET_TIMER(&sc->sc_dfs->sc_dfs_task_timer, 0);
		break;
	case DFS_RADARDETECTS:
		if (!outdata || !outsize || *outsize < sizeof(u_int32_t)) {
			error = -EINVAL;
			break;
		}
		*outsize = sizeof (u_int32_t);
		*((u_int32_t *)outdata) = dfs->ath_dfs_stats.num_radar_detects;
		break;
        case DFS_DISABLE_DETECT:
                printk("%s disable detects\n", __func__);
                dfs->dfs_proc_phyerr &= ~DFS_RADAR_EN;
                break;
        case DFS_ENABLE_DETECT:
		dfs->dfs_proc_phyerr |= DFS_RADAR_EN;
                printk("%s enable detects\n", __func__);
                break;
        case DFS_DISABLE_FFT:
#define AR_PHY_RADAR_0      0x9954      /* radar detection settings */
#define AR_PHY_RADAR_DISABLE_FFT 0x7FFFFFFF
                val = OS_REG_READ(sc->sc_ah, AR_PHY_RADAR_0);
                val &= AR_PHY_RADAR_DISABLE_FFT;
                OS_REG_WRITE(sc->sc_ah, AR_PHY_RADAR_0, val);
                val = OS_REG_READ(sc->sc_ah, AR_PHY_RADAR_0);
#undef AR_PHY_RADAR_0
#undef AR_PHY_RADAR_DISABLE_FFT
                printk("%s disable FFT val=0x%x \n", __func__, val);
                break;
        case DFS_ENABLE_FFT:
#define AR_PHY_RADAR_0      0x9954      /* radar detection settings */
#define AR_PHY_RADAR_ENABLE_FFT 0x80000000
                val = OS_REG_READ(sc->sc_ah, AR_PHY_RADAR_0);
                val |= AR_PHY_RADAR_ENABLE_FFT;
                OS_REG_WRITE(sc->sc_ah, AR_PHY_RADAR_0, val);
                val = OS_REG_READ(sc->sc_ah, AR_PHY_RADAR_0);
#undef AR_PHY_RADAR_ENABLE_FFT
#undef AR_PHY_RADAR_0            /* radar detection settings */
                printk("%s enable FFT val=0x%x \n", __func__, val);
                break;
        case DFS_SET_DEBUG_LEVEL:
		if (insize < sizeof(u_int32_t) || !indata) {
                        error = -EINVAL;
			break;
		}
		dfs_debug_level = *(u_int32_t *)indata;
		dfs_debug_level = (ATH_DEBUG_DFS << dfs_debug_level);
                printk("%s debug level now = 0x%x \n", __func__, dfs_debug_level);
                break;
	case DFS_GET_NOL:
		if (!outdata || !outsize || *outsize < sizeof(struct dfsreq_nolinfo)) {
			error = -EINVAL;
			break;
		}
		*outsize = sizeof(struct dfsreq_nolinfo);
		nol = (struct dfsreq_nolinfo *)outdata;
		dfs_get_nol(sc, (struct dfsreq_nolelem *)nol->dfs_nol, &nol->ic_nchans);
		break;
	case DFS_SET_NOL:
		if (insize < sizeof(struct dfsreq_nolinfo) || !indata) {
                        error = -EINVAL;
                        break;
                }
                nol = (struct dfsreq_nolinfo *) indata;
		dfs_set_nol(sc, (struct dfsreq_nolelem *)nol->dfs_nol, nol->ic_nchans);
		break;
	default:
		error = -EINVAL;
	}
bad:
	return error;
}

int
dfs_set_thresholds(struct ath_softc *sc, const u_int32_t threshtype,
		   const u_int32_t value)
{
	struct ath_dfs *dfs=sc->sc_dfs;
	int16_t chanindex;
	struct dfs_state *rs;
	HAL_PHYERR_PARAM pe;

	if (dfs == NULL) {
		DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: sc_dfs is NULL\n",
			__func__);
		return 0;
	}

	chanindex = dfs->dfs_curchan_radindex;
	if ((chanindex <0) || (chanindex >= DFS_NUM_RADAR_STATES))
		return 0;
	pe.pe_firpwr = HAL_PHYERR_PARAM_NOVAL;
	pe.pe_height = HAL_PHYERR_PARAM_NOVAL;
	pe.pe_prssi = HAL_PHYERR_PARAM_NOVAL;
	pe.pe_inband = HAL_PHYERR_PARAM_NOVAL;
	pe.pe_rrssi = HAL_PHYERR_PARAM_NOVAL;
	/* 5413 specific */
	pe.pe_relpwr = HAL_PHYERR_PARAM_NOVAL;
	pe.pe_relstep = HAL_PHYERR_PARAM_NOVAL;
	pe.pe_maxlen = HAL_PHYERR_PARAM_NOVAL;

	rs = &(dfs->dfs_radar[chanindex]);
	switch (threshtype) {
	case DFS_PARAM_FIRPWR:
		rs->rs_firpwr = (int32_t) value;
		pe.pe_firpwr = rs->rs_firpwr;
		break;
	case DFS_PARAM_RRSSI:
		rs->rs_radarrssi = value;
		pe.pe_rrssi = value;
		break;
	case DFS_PARAM_HEIGHT:
		rs->rs_height = value;
		pe.pe_height = value;
		break;
	case DFS_PARAM_PRSSI:
		rs->rs_pulserssi = value;
		pe.pe_prssi = value;
		break;
	case DFS_PARAM_INBAND:
		rs->rs_inband = value;
		pe.pe_inband = value;
		break;
	/* 5413 specific */
	case DFS_PARAM_RELPWR:
		rs->rs_relpwr = value;
		pe.pe_relpwr = value;
		break;
	case DFS_PARAM_RELSTEP:
		rs->rs_relstep = value;
		pe.pe_relstep = value;
		break;
	case DFS_PARAM_MAXLEN:
		rs->rs_maxlen = value;
		pe.pe_maxlen = value;
		break;
	}
	ath_hal_enabledfs(sc->sc_ah, &pe);
	return 1;
}

int
dfs_get_thresholds(struct ath_softc *sc, HAL_PHYERR_PARAM *param)
{
	ath_hal_getdfsthresh(sc->sc_ah, param);
	return 1;
}

#ifndef __NetBSD__
#ifdef __linux__
#ifndef ATH_WLAN_COMBINE
/*
 * Linux Module glue.
 */
static char *dev_info = "ath_dfs";

MODULE_AUTHOR("Atheros Communications, Inc.");
MODULE_DESCRIPTION("DFS Support for Atheros 802.11 wireless LAN cards.");
MODULE_SUPPORTED_DEVICE("Atheros WLAN cards");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Proprietary");
#endif

static int __init
init_ath_dfs(void)
{
	printk (KERN_INFO "%s: Version 2.0.0\n"
		"Copyright (c) 2005-2006 Atheros Communications, Inc. "
		"All Rights Reserved\n",dev_info);
	return 0;
}
module_init(init_ath_dfs);

static void __exit
exit_ath_dfs(void)
{
	printk (KERN_INFO "%s: driver unloaded\n", dev_info);
}
module_exit(exit_ath_dfs);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,52))
MODULE_PARM(domainoverride, "i");
MODULE_PARM(usenol, "i");
MODULE_PARM_DESC(domainoverride, "Override dfs domain");
MODULE_PARM_DESC(usenol, "Override the use of channel NOL");
#else
#include <linux/moduleparam.h>
module_param(domainoverride, int, 0600);
module_param(usenol, int, 0600);
#endif

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#endif

EXPORT_SYMBOL(dfs_getchanstate);
EXPORT_SYMBOL(dfs_process_radarevent);
EXPORT_SYMBOL(dfs_check_nol);
EXPORT_SYMBOL(dfs_get_nol_chan);
EXPORT_SYMBOL(dfs_attach);
EXPORT_SYMBOL(dfs_detach);
EXPORT_SYMBOL(ath_ar_enable);
EXPORT_SYMBOL(dfs_radar_enable);
EXPORT_SYMBOL(ath_ar_disable);
EXPORT_SYMBOL(ath_process_phyerr);
EXPORT_SYMBOL(dfs_process_ar_event);
EXPORT_SYMBOL(dfs_control);
EXPORT_SYMBOL(dfs_get_thresholds);
EXPORT_SYMBOL(dfs_init_radar_filters);
EXPORT_SYMBOL(dfs_clear_stats);

#endif /* #ifndef ATH_WLAN_COMBINE */
#endif /* __linux__ */
#endif /* __netbsd__ */

#endif /* ATH_UPPORT_DFS */
