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

extern int domainoverride;

/*
 * Clear all delay lines for all filter types
 */
void dfs_reset_alldelaylines(struct ath_softc *sc)
{
        struct ath_dfs *dfs=sc->sc_dfs;
        struct dfs_filtertype *ft = NULL;
        struct dfs_filter *rf;
        struct dfs_delayline *dl;
        struct dfs_pulseline *pl;
        int i,j;

        if (dfs == NULL) {
                DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: sc_dfs is NULL\n", __func__);
                return;
        }
        pl = dfs->pulses;
         /* reset the pulse log */
        pl->pl_firstelem = pl->pl_numelems = 0;
        pl->pl_lastelem = DFS_MAX_PULSE_BUFFER_MASK;

        for (i=0; i<DFS_MAX_RADAR_TYPES; i++) {
		if (dfs->dfs_radarf[i] != NULL) {
                    ft = dfs->dfs_radarf[i];
                    for (j=0; j<ft->ft_numfilters; j++) {
                            rf = &(ft->ft_filters[j]);
                            dl = &(rf->rf_dl);
                            if(dl != NULL) {
                                    OS_MEMZERO(dl, sizeof(struct dfs_delayline));
                                    dl->dl_lastelem = (0xFFFFFFFF) & DFS_MAX_DL_MASK;
                            }
                    }
            }
        }
        for (i=0; i<dfs->dfs_rinfo.rn_numbin5radars; i++) {
                OS_MEMZERO(&(dfs->dfs_b5radars[i].br_elems[0]), sizeof(struct dfs_bin5elem)*DFS_MAX_B5_SIZE);
                dfs->dfs_b5radars[i].br_firstelem = 0;
                dfs->dfs_b5radars[i].br_numelems = 0;
                dfs->dfs_b5radars[i].br_lastelem = (0xFFFFFFFF)&DFS_MAX_B5_MASK;
        }
}
/*
 * Clear only a single delay line
 */

void dfs_reset_delayline(struct dfs_delayline *dl)
{
	OS_MEMZERO(&(dl->dl_elems[0]), sizeof(dl->dl_elems));
	dl->dl_lastelem = (0xFFFFFFFF)&DFS_MAX_DL_MASK;
}

void dfs_reset_filter_delaylines(struct dfs_filtertype *dft)
{
        int i;
        struct dfs_filter *df;
        for (i=0; i< DFS_MAX_NUM_RADAR_FILTERS; i++) {
                df = &dft->ft_filters[i];
                dfs_reset_delayline(&(df->rf_dl));
        }
}

void
dfs_reset_radarq(struct ath_softc *sc)
{
	struct ath_dfs *dfs=sc->sc_dfs;
	struct dfs_event *event;
	if (dfs == NULL) {
		DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: sc_dfs is NULL\n", __func__);
		return;
	}
	ATH_DFSQ_LOCK(dfs);
	ATH_DFSEVENTQ_LOCK(dfs);
	while (!STAILQ_EMPTY(&(dfs->dfs_radarq))) {
		event = STAILQ_FIRST(&(dfs->dfs_radarq));
		STAILQ_REMOVE_HEAD(&(dfs->dfs_radarq), re_list);
		OS_MEMZERO(event, sizeof(struct dfs_event));
		STAILQ_INSERT_TAIL(&(dfs->dfs_eventq), event, re_list);
	}
	ATH_DFSEVENTQ_UNLOCK(dfs);
	ATH_DFSQ_UNLOCK(dfs);
}


/* This function Initialize the radar filter tables 
 * if the ath dfs domain is uninitalized or
 *    ath dfs domain is different from hal dfs domain
 */
int dfs_init_radar_filters( struct ath_softc *sc )
{
    u_int32_t T, Tmax, haldfsdomain;
    int numpulses,p,n, i;
    int numradars = 0, numb5radars = 0;
    struct ath_dfs *dfs = sc->sc_dfs;
    struct dfs_filtertype *ft = NULL;
    struct dfs_filter *rf=NULL;
    struct dfs_pulse *dfs_radars;
    struct dfs_bin5pulse *b5pulses=NULL;
    int32_t min_rssithresh=DFS_MAX_RSSI_VALUE;
    u_int32_t max_pulsedur=0;

    if ( dfs == NULL ) {
        DFS_DPRINTK(sc, ATH_DEBUG_DFS, "dfs is NULL %s",__func__);
    	return 1;
    }

    /* clear up the dfs domain flag first */
    dfs->sc_dfs_isdfsregdomain = 0;

    if (ath_hal_getcapability(sc->sc_ah, HAL_CAP_PHYDIAG,
				  HAL_CAP_RADAR, NULL) == HAL_OK) {
    /* Init DFS radar filters */
    ath_hal_getcapability(sc->sc_ah, HAL_CAP_DFS_DMN, 0, &haldfsdomain);

    if ( (dfs->dfsdomain == DFS_UNINIT_DOMAIN) ||
	 ( dfs->dfsdomain != haldfsdomain ) )
    {
            /* set the dfs domain from HAL */
            dfs->dfsdomain = haldfsdomain;
	    DFS_DPRINTK(sc, ATH_DEBUG_DFS2, "DFS domain=%d\n",dfs->dfsdomain);
	    if (domainoverride != DFS_UNINIT_DOMAIN) {
		DFS_DPRINTK(sc, ATH_DEBUG_DFS, 
			   "Overriding DFS domain with %d\n",domainoverride); 
		dfs->dfsdomain = domainoverride;
	    }
	    dfs_radars = ath_hal_getdfsradars(sc->sc_ah, dfs->dfsdomain,
			     &numradars, &b5pulses, &numb5radars,
			     &(dfs->dfs_defaultparams));
	    if (dfs_radars == NULL) {
		DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: Unknown dfs domain %d\n",
			    __func__, dfs->dfsdomain);
		/* Disable radar detection since we don't have a radar domain */
		dfs->dfs_proc_phyerr &= ~DFS_RADAR_EN;
		return 0;
	    }
        dfs->sc_dfs_isdfsregdomain = 1;

	    dfs->dfs_rinfo.rn_numradars = 0;
	    /* Clear filter type table */
	    for (n=0; n<256; n++) {
		for (i=0;i<DFS_MAX_RADAR_OVERLAP; i++)
		    (dfs->dfs_radartable[n])[i] = -1;
            }
	    /* Now, initialize the radar filters */
	    for (p=0; p<numradars; p++) {
		ft = NULL;
		for (n=0; n<dfs->dfs_rinfo.rn_numradars; n++) {
		    if ((dfs_radars[p].rp_pulsedur == dfs->dfs_radarf[n]->ft_filterdur) &&
		       (dfs_radars[p].rp_numpulses == dfs->dfs_radarf[n]->ft_numpulses) &&
		       (dfs_radars[p].rp_mindur == dfs->dfs_radarf[n]->ft_mindur) &&
		       (dfs_radars[p].rp_maxdur == dfs->dfs_radarf[n]->ft_maxdur)) {
		       ft = dfs->dfs_radarf[n];
		       break;
		    }
		}
		if (ft == NULL) {
		    /* No filter of the appropriate dur was found */
		    if ((dfs->dfs_rinfo.rn_numradars+1) >DFS_MAX_RADAR_TYPES) {
			DFS_DPRINTK(sc, ATH_DEBUG_DFS, "%s: Too many filter types\n",
			__func__);
			goto bad4;
		    }
		    ft = dfs->dfs_radarf[dfs->dfs_rinfo.rn_numradars];
		    ft->ft_numfilters = 0;
		    ft->ft_numpulses = dfs_radars[p].rp_numpulses;
		    ft->ft_patterntype = dfs_radars[p].rp_patterntype;
		    ft->ft_mindur = dfs_radars[p].rp_mindur;
		    ft->ft_maxdur = dfs_radars[p].rp_maxdur;
		    ft->ft_filterdur = dfs_radars[p].rp_pulsedur;
		    ft->ft_rssithresh = dfs_radars[p].rp_rssithresh;
		    ft->ft_rssimargin = dfs_radars[p].rp_rssimargin;
		    ft->ft_minpri = 1000000;

		    if (ft->ft_rssithresh < min_rssithresh)
		    min_rssithresh = ft->ft_rssithresh;
		    if (ft->ft_maxdur > max_pulsedur)
		    max_pulsedur = ft->ft_maxdur;
		    for (i=ft->ft_mindur; i<=ft->ft_maxdur; i++) {
			u_int32_t stop=0,tableindex=0;
			while ((tableindex < DFS_MAX_RADAR_OVERLAP) && (!stop)) {
			    if ((dfs->dfs_radartable[i])[tableindex] == -1)
				stop = 1;
			    else
				tableindex++;
			}
			if (stop) {
			    (dfs->dfs_radartable[i])[tableindex] =
				(int8_t) (dfs->dfs_rinfo.rn_numradars);
			} else {
			    DFS_DPRINTK(sc, ATH_DEBUG_DFS,
				"%s: Too many overlapping radar filters\n",
				__func__);
			    goto bad4;
			}
		    }
		    dfs->dfs_rinfo.rn_numradars++;
		}
		rf = &(ft->ft_filters[ft->ft_numfilters++]);
		dfs_reset_delayline(&rf->rf_dl);
		numpulses = dfs_radars[p].rp_numpulses;

		rf->rf_numpulses = numpulses;
		rf->rf_patterntype = dfs_radars[p].rp_patterntype;
		rf->rf_pulseid = dfs_radars[p].rp_pulseid;
		rf->rf_mindur = dfs_radars[p].rp_mindur;
		rf->rf_maxdur = dfs_radars[p].rp_maxdur;
		rf->rf_numpulses = dfs_radars[p].rp_numpulses;

		T = (100000000/dfs_radars[p].rp_max_pulsefreq) -
        		100*(dfs_radars[p].rp_meanoffset);
                rf->rf_minpri =
		        dfs_round((int32_t)T - (100*(dfs_radars[p].rp_pulsevar)));
		Tmax = (100000000/dfs_radars[p].rp_pulsefreq) -
		        100*(dfs_radars[p].rp_meanoffset);
        	rf->rf_maxpri =
	        	dfs_round((int32_t)Tmax + (100*(dfs_radars[p].rp_pulsevar)));

	        if( rf->rf_minpri < ft->ft_minpri )
		    ft->ft_minpri = rf->rf_minpri;

		rf->rf_threshold = dfs_radars[p].rp_threshold;
		rf->rf_filterlen = rf->rf_maxpri * rf->rf_numpulses;

		DFS_DPRINTK(sc, ATH_DEBUG_DFS2, "minprf = %d maxprf = %d pulsevar = %d thresh=%d\n",
		dfs_radars[p].rp_pulsefreq, dfs_radars[p].rp_max_pulsefreq, dfs_radars[p].rp_pulsevar, rf->rf_threshold);
		DFS_DPRINTK(sc, ATH_DEBUG_DFS2,
		"minpri = %d maxpri = %d filterlen = %d filterID = %d\n",
		rf->rf_minpri, rf->rf_maxpri, rf->rf_filterlen, rf->rf_pulseid);
	    }

#ifdef DFS_DEBUG
	    dfs_print_filters(sc);
#endif
	    dfs->dfs_rinfo.rn_numbin5radars  = numb5radars;
	    if ( dfs->dfs_b5radars == NULL ) {
		dfs->dfs_b5radars = (struct dfs_bin5radars *)OS_MALLOC(sc->sc_osdev, numb5radars * sizeof(struct dfs_bin5radars),
		GFP_KERNEL);
		if (dfs->dfs_b5radars == NULL) {
		    DFS_DPRINTK(sc, ATH_DEBUG_DFS, 
		    "%s: cannot allocate memory for bin5 radars\n",
		    __func__);
		    goto bad4;
		}
	    }
	    for (n=0; n<numb5radars; n++) {
		dfs->dfs_b5radars[n].br_pulse = b5pulses[n];
		dfs->dfs_b5radars[n].br_pulse.b5_timewindow *= 1000000;
		if (dfs->dfs_b5radars[n].br_pulse.b5_rssithresh < min_rssithresh)
		min_rssithresh = dfs->dfs_b5radars[n].br_pulse.b5_rssithresh;
		if (dfs->dfs_b5radars[n].br_pulse.b5_maxdur > max_pulsedur )
		max_pulsedur = dfs->dfs_b5radars[n].br_pulse.b5_maxdur;
	    }
	    dfs_reset_alldelaylines(sc);
	    dfs_reset_radarq(sc);
	    dfs->dfs_curchan_radindex = -1;
	    dfs->dfs_extchan_radindex = -1;
	    dfs->dfs_rinfo.rn_minrssithresh = min_rssithresh;
	    /* Convert durations to TSF ticks */
	    dfs->dfs_rinfo.rn_maxpulsedur = dfs_round((int32_t)((max_pulsedur*100/80)*100));
	    /* relax the max pulse duration a little bit due to inaccuracy caused by chirping. */
	    dfs->dfs_rinfo.rn_maxpulsedur = dfs->dfs_rinfo.rn_maxpulsedur +20;
	    DFS_DPRINTK (sc, ATH_DEBUG_DFS, "DFS min filter rssiThresh = %d\n",min_rssithresh);
	    DFS_DPRINTK (sc, ATH_DEBUG_DFS, "DFS max pulse dur = %d ticks\n", dfs->dfs_rinfo.rn_maxpulsedur);
	    return 0;

	bad4:
	     for (n=0; n<ft->ft_numfilters; n++) {
		 rf = &(ft->ft_filters[n]);
	     }
	     return 1;
        }
     }
     return 0;
}

void
dfs_clear_stats(struct ath_softc *sc)
{
    struct ath_dfs *dfs = sc->sc_dfs;
    if (dfs == NULL) 
            return;
    OS_MEMZERO(&dfs->ath_dfs_stats, sizeof (struct dfs_stats));
    dfs->ath_dfs_stats.last_reset_tstamp = ath_hal_gettsf64(sc->sc_ah);
}

#endif /* ATH_SUPPORT_DFS */
