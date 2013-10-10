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

#include <ieee80211_var.h>
#include <ieee80211_scan.h>
#include <ieee80211_channel.h>

#if UMAC_SUPPORT_ACS

#define IEEE80211_MAX_ACS_EVENT_HANDLERS 10
#define DEBUG_EACS 0

struct ieee80211_acs {
    /* Driver-wide data structures */
    wlan_dev_t                          acs_ic;
    wlan_if_t                           acs_vap;
    osdev_t                             acs_osdev;

    spinlock_t                          acs_lock;                /* acs lock */

    /* List of clients to be notified about scan events */
    u_int16_t                           acs_num_handlers;
    ieee80211_acs_event_handler         acs_event_handlers[IEEE80211_MAX_ACS_EVENT_HANDLERS];
    void                                *acs_event_handler_arg[IEEE80211_MAX_ACS_EVENT_HANDLERS];

    IEEE80211_SCAN_REQUESTOR            acs_scan_requestor;    /* requestor id assigned by scan module */
    IEEE80211_SCAN_ID                   acs_scan_id;           /* scan id assigned by scan scheduler */
    u_int8_t                            acs_in_progress:1,  /* flag for ACS in progress */
                                        acs_scan_2ghz_only:1; /* flag for scan 2.4 GHz channels only */
    struct ieee80211_channel            *acs_channel;

    u_int16_t                           acs_nchans;         /* # of all available chans */
    struct ieee80211_channel            *acs_chans[IEEE80211_CHAN_MAX];
    u_int8_t                            acs_chan_maps[IEEE80211_CHAN_MAX];       /* channel mapping array */
    u_int32_t                           acs_chan_maxrssi[IEEE80211_CHAN_MAX];    /* max rssi of these channels */
    int16_t                             acs_noisefloor[IEEE80211_CHAN_MAX];      /* Noise floor value read current channel */
    int16_t                             acs_channel_loading[IEEE80211_CHAN_MAX];      /* Noise floor value read current channel */
#if ATH_SUPPORT_5G_EACS
#if 0
    u_int16_t                           acs_phy_err_cnt[IEEE80211_CHAN_MAX];
#endif
    u_int32_t                           acs_chan_load[IEEE80211_CHAN_MAX];
    u_int32_t                           acs_cycle_count[IEEE80211_CHAN_MAX];
#endif
    u_int32_t                           acs_minrssi_11na;    /* min rssi in 5 GHz band selected channel */
    u_int32_t                           acs_avgrssi_11ng;    /* average rssi in 2.4 GHz band selected channel */
};

#if ATH_SUPPORT_5G_EACS
struct ieee80211_acs_adj_chan_stats {
    u_int32_t                           adj_chan_load;
    u_int32_t                           adj_chan_rssi;
    u_int8_t                            if_valid_stats;    
    u_int8_t                            adj_chan_idx;
};

#endif
/* These defines should match the table from ah_internal.h */
enum {
	DFS_UNINIT_DOMAIN	= 0,	/* Uninitialized dfs domain */
	DFS_FCC_DOMAIN		= 1,	/* FCC3 dfs domain */
	DFS_ETSI_DOMAIN		= 2,	/* ETSI dfs domain */
	DFS_MKK4_DOMAIN		= 3	/* Japan dfs domain */
};
#define WEATHER_RADAR_CHANNEL(freq)  (freq >= 5600) && (freq <= 5650)  

/* Forward declarations */
static void ieee80211_acs_free_scan_resource(ieee80211_acs_t acs);
static void ieee80211_free_ht40intol_scan_resource(ieee80211_acs_t acs);

static INLINE u_int8_t ieee80211_acs_in_progress(ieee80211_acs_t acs) 
{
    return (acs->acs_in_progress);
}

/*
 * Check for channel interference.
 */
static int
ieee80211_acs_channel_is_set(struct ieee80211vap *vap)
{
    struct ieee80211_channel    *chan = NULL;

    chan =  vap->iv_des_chan[vap->iv_des_mode];

    if ((chan == NULL) || (chan == IEEE80211_CHAN_ANYC)) {
        return (0);
    } else {
        return (1);
    }
}

/*
 * Check for channel interference.
 */
static int
ieee80211_acs_check_interference(struct ieee80211_channel *chan, struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;

    /*
   * (1) skip static turbo channel as it will require STA to be in
   * static turbo to work.
   * (2) skip channel which's marked with radar detection
   * (3) WAR: we allow user to config not to use any DFS channel
   * (4) skip excluded 11D channels. See bug 31246 
   */
    if ( IEEE80211_IS_CHAN_STURBO(chan) || 
         IEEE80211_IS_CHAN_RADAR(chan) ||
         (IEEE80211_IS_CHAN_DFSFLAG(chan) && ieee80211_ic_block_dfschan_is_set(ic)) ||
         IEEE80211_IS_CHAN_11D_EXCLUDED(chan) ) {
        return (1);
    } else {
        return (0);
    }
}
#if ATH_SUPPORT_5G_EACS
static void ieee80211_acs_get_adj_ch_stats(ieee80211_acs_t acs, struct ieee80211_channel *channel,
        struct ieee80211_acs_adj_chan_stats *adj_chan_stats )
{
#define ADJ_CHANS 8
    u_int8_t ieee_chan = ieee80211_chan2ieee(acs->acs_ic, channel);
    int i, k; 
    u_int32_t max_adj_chan_load = 0;
    int sec_chan, pri_chanix = 0, sec_chanix = 0;

    int32_t mode_mask = (IEEE80211_CHAN_11NA_HT20 |
                         IEEE80211_CHAN_11NA_HT40PLUS |
                         IEEE80211_CHAN_11NA_HT40MINUS );

    adj_chan_stats->if_valid_stats = 1;
    adj_chan_stats->adj_chan_load = 0;
    adj_chan_stats->adj_chan_rssi = 0;
    adj_chan_stats->adj_chan_idx = 0;

    switch (channel->ic_flags & mode_mask)
    {
        case IEEE80211_CHAN_11NA_HT40PLUS:
            sec_chan = ieee_chan+4;
            break;
        case IEEE80211_CHAN_11NA_HT40MINUS:
            sec_chan = ieee_chan-4;
            break;
        case IEEE80211_CHAN_11NA_HT20:
            sec_chan = ieee_chan;
        default: /* neither HT40+ nor HT40-, finish this call */
            sec_chan = ieee_chan;
            break;
    }

    for (i = 0; i < acs->acs_nchans; i++) {
        k = ieee80211_chan2ieee(acs->acs_ic, acs->acs_chans[i]);
        if ( k == ieee_chan){
            pri_chanix = i;
        }
        if ( k == sec_chan ){
            sec_chanix = i;
        }
    }
#if DEBUG_EACS
    printk ("%s: Pri ch: %d Sec ch: %d Sec ch load %d rssi: %d\n ", __func__, ieee_chan, sec_chan, 
            acs->acs_chan_load[sec_chan], acs->acs_chan_maxrssi[sec_chanix]);
#endif
    if ( sec_chan != ieee_chan ){
        if ( acs->acs_chan_maxrssi[sec_chanix] != 0 ){
            if ( acs->acs_chan_maxrssi[pri_chanix] != 0 ){
                adj_chan_stats->adj_chan_load = acs->acs_chan_load[sec_chan];
                adj_chan_stats->adj_chan_rssi = acs->acs_chan_maxrssi[sec_chanix];
                adj_chan_stats->adj_chan_idx = sec_chanix;
                max_adj_chan_load = acs->acs_chan_load[sec_chan]; 
            }
            else{
                /* As per the standard, if AP detects a beacon in the sec ch, 
                 * then it should switch to 20 MHz mode. So, reject this channel */
                adj_chan_stats->adj_chan_load = 100;
                adj_chan_stats->adj_chan_rssi = acs->acs_chan_maxrssi[sec_chanix];
                adj_chan_stats->adj_chan_idx = sec_chanix;
                return; 
            }
        }
    }

    /* adjacent channel = [primary ch -ADJ_CHANS, secondary ch + ADJ_CHANS] */
    for (i = 0 ; i < acs->acs_nchans; i++) {
        k = ieee80211_chan2ieee(acs->acs_ic, acs->acs_chans[i]);

        if ( (k == ieee_chan) || (k == sec_chan) ) continue; 

        if ( ieee_chan >= sec_chan )
        {
            if ( (k < (sec_chan - ADJ_CHANS))||(k > (ieee_chan + ADJ_CHANS)) ){
                continue;
            }
        }
        else
        {
            if ( (k < (ieee_chan - ADJ_CHANS))||(k > (sec_chan + ADJ_CHANS)) ){
                continue;
            }
        }
        /* sum of adjacent channel load */
        if(acs->acs_chan_maxrssi[i]){
            adj_chan_stats->adj_chan_load += acs->acs_chan_load[k];
        }
        /* max adjacent channel rssi */
        if(acs->acs_chan_maxrssi[i] > adj_chan_stats->adj_chan_rssi){
            adj_chan_stats->adj_chan_rssi = acs->acs_chan_maxrssi[i];
            adj_chan_stats->adj_chan_idx = i;
        }
        /* max adj channel loadd */
        if (acs->acs_chan_load[k] > max_adj_chan_load ) {
            max_adj_chan_load = acs->acs_chan_load[k];
        }
        
#if DEBUG_EACS
        printk ("%s: Adj ch: %d %d ch load: %d rssi: %d\n", 
                __func__, i, k, acs->acs_chan_load[k], acs->acs_chan_maxrssi[i] );
#endif
    }
    adj_chan_stats->adj_chan_load = MAX(adj_chan_stats->adj_chan_load, max_adj_chan_load ); 
#if DEBUG_EACS
    printk ("%s: Adj ch stats valid: %d ind: %d rssi: %d load: %d\n",__func__, 
            adj_chan_stats->if_valid_stats, adj_chan_stats->adj_chan_idx, 
            adj_chan_stats->adj_chan_rssi, adj_chan_stats->adj_chan_load );
#endif

#undef ADJ_CHANS
}
#endif


/*
 * In 5 GHz, if the channel is unoccupied the max rssi
 * should be zero; just take it.Otherwise
 * track the channel with the lowest rssi and
 * use that when all channels appear occupied.
 */
static INLINE struct ieee80211_channel *
ieee80211_acs_find_best_11na_centerchan(ieee80211_acs_t acs)
{
    struct ieee80211_channel *channel;
    int best_center_chanix = -1, i;
#if ATH_SUPPORT_5G_EACS
#define MIN_ADJ_CH_RSSI_THRESH 8
#define CH_LOAD_INACCURACY 0

    u_int32_t min_max_chan_load = 0xffffffff;
    u_int32_t min_sum_chan_load = 0xffffffff, sum;
    int min_sum_chan_load_idx = -1;
    u_int32_t min_cur_chan_load = 0xffffffff;
    int min_cur_chan_load_idx = -1;
    u_int32_t min_highpower_chan_load = 0xffffffff;
    int min_highpower_chan_load_idx = -1;

    int cur_ch_load_thresh = 0;
    u_int16_t nchans = 0;
    u_int8_t chan_ind[IEEE80211_CHAN_MAX];
    u_int8_t random;
    int j;
    struct ieee80211_acs_adj_chan_stats *adj_chan_stats;
    adj_chan_stats = (struct ieee80211_acs_adj_chan_stats *) OS_MALLOC(acs->acs_osdev, 
            IEEE80211_CHAN_MAX * sizeof(struct ieee80211_acs_adj_chan_stats), 0);

    if (adj_chan_stats) {
        OS_MEMZERO(adj_chan_stats, sizeof(struct ieee80211_acs_adj_chan_stats)*IEEE80211_CHAN_MAX);
    }
    else {
        adf_os_print("%s: malloc failed \n",__func__);
        return NULL;//ENOMEM;
    }

#endif

    acs->acs_minrssi_11na = 0xffffffff;

#if ATH_SUPPORT_5G_EACS
do {
#endif

    for (i = 0; i < acs->acs_nchans; i++) {
        channel = acs->acs_chans[i];

        /* Check for channel interference. If found, skip the channel */
        if (ieee80211_acs_check_interference(channel, acs->acs_vap)) {
            continue;
        }
        /* CAC for weather radar channel is 10 minutes so we are avoiding these channels */
        if (IEEE80211_IS_CHAN_5GHZ(channel)) {
            if(acs->acs_ic->ic_no_weather_radar_chan) {
                if(WEATHER_RADAR_CHANNEL(ieee80211_chan2freq(acs->acs_ic, channel))
                        && (acs->acs_ic->ic_get_dfsdomain(acs->acs_ic) == DFS_ETSI_DOMAIN)) {
                    continue;
                }
            }

#if ATH_SUPPORT_5G_EACS
            /* check neighboring channel load 
             * pending - first check the operating mode from beacon( 20MHz/40 MHz) and 
             * based on that find the interfering channel */
            if( !adj_chan_stats[i].if_valid_stats){ 
                ieee80211_acs_get_adj_ch_stats(acs, channel, &adj_chan_stats[i]);
                /* find minimum of MAX( adj_chan_load, cur channel load)*/
                if ( min_max_chan_load > MAX (adj_chan_stats[i].adj_chan_load , 
                            acs->acs_chan_load[ieee80211_chan2ieee(acs->acs_ic, channel)])){
                    min_max_chan_load = MAX (adj_chan_stats[i].adj_chan_load , 
                            acs->acs_chan_load[ieee80211_chan2ieee(acs->acs_ic, channel)]) ;
                } 
            }
#if DEBUG_EACS
            printk( "%s chan: %d maxrssi: %d, cl: %d Adj cl: %d ind: %d cl thresh: %d\n", __func__,
                    ieee80211_chan2ieee(acs->acs_ic, channel), acs->acs_chan_maxrssi[i],
                    acs->acs_chan_load[ieee80211_chan2ieee(acs->acs_ic, channel)], 
                    adj_chan_stats[i].adj_chan_load, i, cur_ch_load_thresh);
#endif      
            if (acs->acs_chan_maxrssi[i] < acs->acs_minrssi_11na) {
                acs->acs_minrssi_11na = acs->acs_chan_maxrssi[i];
            }

            if( acs->acs_chan_load[ieee80211_chan2ieee(acs->acs_ic, channel)] > cur_ch_load_thresh) {
                continue;
            }

            /* look for max rssi in beacons found in this channel */
            if ( (acs->acs_chan_maxrssi[i] == 0) && 
                    (adj_chan_stats[i].adj_chan_rssi == 0)) {
                
                if (acs->acs_chan_load[ieee80211_chan2ieee(acs->acs_ic, channel) ] <= 0) { 
                    chan_ind[nchans++] = i;
                }
                /* minimize current ch load */ 
                if (acs->acs_chan_load[ieee80211_chan2ieee(acs->acs_ic, channel) ] < min_cur_chan_load) {
                    min_cur_chan_load = acs->acs_chan_load[ieee80211_chan2ieee(acs->acs_ic, channel) ] ;
                    min_cur_chan_load_idx = i;
                }
                /* prefer high power channel first */
                if ( ieee80211_chan2ieee(acs->acs_ic, channel) >= 149 ){
                    if (acs->acs_chan_load[ieee80211_chan2ieee(acs->acs_ic, channel) ] < 
                            min_highpower_chan_load) {
                        min_highpower_chan_load = acs->acs_chan_load[ieee80211_chan2ieee(acs->acs_ic, channel)];
                        min_highpower_chan_load_idx = i;
                        if(min_highpower_chan_load == 0) break;
                    }
                }
            
            } else {

                if ( adj_chan_stats[i].adj_chan_rssi < MIN_ADJ_CH_RSSI_THRESH ){
                    sum = acs->acs_chan_load[ieee80211_chan2ieee(acs->acs_ic, channel)];
                } else {
                    sum = adj_chan_stats[i].adj_chan_load + 
                    acs->acs_chan_load[ieee80211_chan2ieee(acs->acs_ic, channel)];
                }
                /* minimize sum of adj ch load */
                if (sum < min_sum_chan_load) {
                    min_sum_chan_load = sum;
                    min_sum_chan_load_idx = i;
                }
            }

#else       
            /* look for max rssi in beacons found in this channel */
            if (acs->acs_chan_maxrssi[i] == 0) {
                acs->acs_minrssi_11na = 0;
                best_center_chanix = i;
                break;
            }

            if (acs->acs_chan_maxrssi[i] < acs->acs_minrssi_11na) {
                acs->acs_minrssi_11na = acs->acs_chan_maxrssi[i];
                best_center_chanix = i;
            }

#endif

        } else {
            /* Skip non-5GHZ channel */
            continue;
        }
    }

#if ATH_SUPPORT_5G_EACS
    if (min_cur_chan_load_idx != -1){

        /* prefer high power channel*/
        if (min_cur_chan_load == min_highpower_chan_load){
            best_center_chanix = min_highpower_chan_load_idx;
#if DEBUG_EACS
            printk( "%s Prefering high power chan\n",__func__ );
#endif
        }else if (nchans != 0){
            OS_GET_RANDOM_BYTES(&random, sizeof(random));
            j = random % nchans;
            best_center_chanix = chan_ind[j];
        }else{
            best_center_chanix = min_cur_chan_load_idx;
        }
    } else if(min_sum_chan_load_idx != -1){
            /* minimize sum of adj chan load and curr chan load, 
             * when max( adj chan load, curr chan load) is less than 
             * min_max_chan_load + CH_LOAD_INACCURACY
             */
            best_center_chanix = min_sum_chan_load_idx;
    }else if(cur_ch_load_thresh){
        break;
    }
    cur_ch_load_thresh += min_max_chan_load + CH_LOAD_INACCURACY;

} while(best_center_chanix == -1 );
OS_FREE(adj_chan_stats);
#undef MIN_ADJ_CH_RSSI_THRESH
#undef CH_LOAD_INACCURACY

#endif

    if (best_center_chanix != -1) {
        channel = acs->acs_chans[best_center_chanix];
        printk( "Found best 11na chan: %d\n", 
                ieee80211_chan2ieee(acs->acs_ic, channel));
    } else {
        channel = ieee80211_find_dot11_channel(acs->acs_ic, 0, acs->acs_vap->iv_des_mode);
        /* no suitable channel, should not happen */
#if DEBUG_EACS      
        printk( "%s: no suitable channel! (should not happen)\n", __func__);
#endif
    }

    return channel;
}

#define NF_WEIGHT_FACTOR (2)
#define CHANLOAD_WEIGHT_FACTOR (4)
#define CHANLOAD_INCREASE_AVERAGE_RSSI (10)

static u_int32_t ieee80211_acs_find_average_rssi(ieee80211_acs_t acs, const int *chanlist, int chancount, u_int8_t centChan)
{
    u_int8_t chan;
    int i, j;
    u_int32_t average = 0;
    u_int32_t totalrssi = 0; /* total rssi for all channels so far */

    if (chancount <= 0) {
        /* return a large enough number for not to choose this channel */
        return 0xffffffff;
    }

    for (i = 0; i < chancount; i++) {
        chan = chanlist[i];

        j = acs->acs_chan_maps[chan];
        if (j != 0xff) { 
            totalrssi += acs->acs_chan_maxrssi[j];
#if DEBUG_EACS
            printk( "%s chan: %d maxrssi: %d\n", __func__, chan, acs->acs_chan_maxrssi[j]);
#endif
        }
    }
    
    average = totalrssi/chancount;
#if DEBUG_EACS   
    printk( "Channel %d average beacon RSSI %d noisefloor %d\n", 
       centChan, average, acs->acs_noisefloor[centChan]);
#endif
    /* add the weighted noise floor */
    average += acs->acs_noisefloor[centChan] * NF_WEIGHT_FACTOR;
#if ATH_SUPPORT_SPECTRAL
    /* If channel loading is greater, add RSSI factor to the average_rssi for that channel */
    average +=  ((acs->acs_channel_loading[centChan] * CHANLOAD_INCREASE_AVERAGE_RSSI) / 100);
#endif    
    return (average);
}

static INLINE struct ieee80211_channel *
ieee80211_acs_find_best_11ng_centerchan(ieee80211_acs_t acs)
{
    struct ieee80211_channel *channel;
    int best_center_chanix = -1, i;
    u_int8_t chan, band;
    u_int32_t avg_rssi;

    /*
   * The following center chan data structures are invented to allow calculating
   * the average rssi in 20Mhz band for a certain center chan. 
   *
   * We would then pick the band which has the minimum rsi of all the 20Mhz bands.
   */

    /* For 20Mhz band with center chan 1, we would see beacons only on channels 1,2. */
    static const u_int center1[] = { 1, 2 };

    /* For 20Mhz band with center chan 6, we would see beacons on channels 4,5,6 & 7. */
    static const u_int center6[] = { 4, 5, 6, 7 };

    /* For 20Mhz band with center chan 11, we would see beacons on channels 9,10 & 11. */
    static const u_int center11[] = { 9, 10, 11 };

    struct centerchan {
        int count;               /* number of chans to average the rssi */
        const u_int *chanlist;   /* the possible beacon channels around center chan */
    };

#define X(a)    sizeof(a)/sizeof(a[0]), a

    struct centerchan centerchans[] = {
        { X(center1) },
        { X(center6) },
        { X(center11) }
    };

    acs->acs_avgrssi_11ng = 0xffffffff;
   
    for (i = 0; i < acs->acs_nchans; i++) {
        channel = acs->acs_chans[i];
        chan = ieee80211_chan2ieee(acs->acs_ic, channel);

        if ((chan != 1) && (chan != 6) && (chan != 11)) {
            /* Don't bother with other center channels except for 1, 6 & 11 */
            continue;
        }

        switch (chan) {
            case 1:
                band = 0;
                break;
            case 6:
                band = 1;
                break;
            case 11:
                band = 2;
                break;
            default: 
                band = 0;
                break;
        }

        /* find the average rssi for this 20Mhz band */
        avg_rssi = ieee80211_acs_find_average_rssi(acs, centerchans[band].chanlist, centerchans[band].count, chan);
#if DEBUG_EACS
        printk( "%s chan: %d beacon RSSI + weighted noisefloor: %d\n", __func__, chan, avg_rssi);
#endif
        if (avg_rssi == 0) {
            acs->acs_avgrssi_11ng = avg_rssi;
            best_center_chanix = i;
            break;
        }

        if (avg_rssi < acs->acs_avgrssi_11ng) {
            acs->acs_avgrssi_11ng = avg_rssi;
            best_center_chanix = i;
        }
    }


    if (best_center_chanix != -1) {
        channel = acs->acs_chans[best_center_chanix];
#if DEBUG_EACS
        printk( "%s found best 11ng center chan: %d rssi: %d\n", 
            __func__, ieee80211_chan2ieee(acs->acs_ic, channel), acs->acs_avgrssi_11ng);
#endif
    } else {
        channel = ieee80211_find_dot11_channel(acs->acs_ic, 0, acs->acs_vap->iv_des_mode);
        /* no suitable channel, should not happen */
#if DEBUG_EACS    
        printk( "%s: no suitable channel! (should not happen)\n", __func__);
#endif
    }

    return channel;
}

static INLINE struct ieee80211_channel *
ieee80211_acs_find_best_auto_centerchan(ieee80211_acs_t acs)
{
    struct ieee80211_channel *channel_11na, *channel_11ng;

    channel_11na = ieee80211_acs_find_best_11na_centerchan(acs);
    channel_11ng = ieee80211_acs_find_best_11ng_centerchan(acs);
#if DEBUG_EACS
    printk( "%s 11na chan: %d rssi: %d, 11ng chan: %d rssi: %d\n", 
        __func__, ieee80211_chan2ieee(acs->acs_ic, channel_11na), acs->acs_minrssi_11na,
        ieee80211_chan2ieee(acs->acs_ic, channel_11ng), acs->acs_avgrssi_11ng);
#endif
    if (acs->acs_minrssi_11na > acs->acs_avgrssi_11ng) {
        return channel_11ng;
    } else {
        return channel_11na;
    }
}

static INLINE struct ieee80211_channel *
ieee80211_acs_find_best_centerchan(ieee80211_acs_t acs)
{
    struct ieee80211_channel *channel;

    switch (acs->acs_vap->iv_des_mode)
    {
    case IEEE80211_MODE_11A:
    case IEEE80211_MODE_TURBO_A:
    case IEEE80211_MODE_11NA_HT20:
    case IEEE80211_MODE_11NA_HT40PLUS:
    case IEEE80211_MODE_11NA_HT40MINUS:
    case IEEE80211_MODE_11NA_HT40:
        channel = ieee80211_acs_find_best_11na_centerchan(acs);
        break;

    case IEEE80211_MODE_11B:
    case IEEE80211_MODE_11G:
    case IEEE80211_MODE_TURBO_G:
    case IEEE80211_MODE_11NG_HT20:
    case IEEE80211_MODE_11NG_HT40PLUS:
    case IEEE80211_MODE_11NG_HT40MINUS:
    case IEEE80211_MODE_11NG_HT40:
        channel = ieee80211_acs_find_best_11ng_centerchan(acs);
        break;

    default:
        if (acs->acs_scan_2ghz_only) {
            channel = ieee80211_acs_find_best_11ng_centerchan(acs);
        } else {
            channel = ieee80211_acs_find_best_auto_centerchan(acs);
        }
        break;
    }

    return channel;
}

static struct ieee80211_channel *
ieee80211_acs_pickup_random_channel(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_channel *channel;
    u_int16_t nchans = 0;
    struct ieee80211_channel **chans;
    u_int8_t random;
    int i,index =0;

    chans = (struct ieee80211_channel **)
	    OS_MALLOC(ic->ic_osdev,
		      IEEE80211_CHAN_MAX * sizeof(struct ieee80211_channel *),
		      0);
    if (chans == NULL)
	    return NULL;

    ieee80211_enumerate_channels(channel, ic, i) {
        /* skip if found channel interference */
        if (ieee80211_acs_check_interference(channel, vap)) {
#if DEBUG_EACS      
      printk( "%s: Skip channel %d due to channel interference, flags=0x%x, flagext=0x%d\n",
                __func__, ieee80211_chan2ieee(ic, channel), ieee80211_chan_flags(channel), ieee80211_chan_flagext(channel));
#endif
            continue;
        }

        if (vap->iv_des_mode == ieee80211_chan2mode(channel)) {
#if DEBUG_EACS      
      printk( "%s: Put channel %d into channel list for pick up later\n",
                __func__, ieee80211_chan2ieee(ic, channel));
#endif
            chans[nchans++] = channel;
        }
    }

    if (nchans == 0) {
#if DEBUG_EACS
        printk( "%s: no any available channel for pick up\n",
            __func__);
#endif
	OS_FREE(chans);
        return NULL;
    }

    /* Pick up a random channel to start */
pickchannel:
    OS_GET_RANDOM_BYTES(&random, sizeof(random));
    i = random % nchans;
    channel = chans[i];
    /* CAC for weather radar channel is 10 minutes so we are avoiding these channels */
    if (IEEE80211_IS_CHAN_5GHZ(channel)) {
        if(ic->ic_no_weather_radar_chan) {
            if(WEATHER_RADAR_CHANNEL(ieee80211_chan2freq(ic, channel))
                    && (ic->ic_get_dfsdomain(ic) == DFS_ETSI_DOMAIN)) {
#define MAX_RETRY 5
#define BYPASS_WRADAR_CHANNEL_CNT 3                
                if(index >= MAX_RETRY) {
#if DEBUG_EACS
                        printk( "%s: no any available channel for pick up\n",
                                __func__);
#endif
                        channel = chans[(i + BYPASS_WRADAR_CHANNEL_CNT) % nchans];
                        /* there can be three channels in 
                           this range so adding 3 */
                        OS_FREE(chans);
                        return channel;
                }
#undef MAX_RETRY 
#undef BYPASS_WRADAR_CHANNEL_CNT 
                index++;
                goto pickchannel;
            }
        }
    }

    OS_FREE(chans);
#if DEBUG_EACS
    printk( "%s: Pick up a random channel=%d to start\n",
        __func__, ieee80211_chan2ieee(ic, channel));
#endif
    return channel;
}

static void
ieee80211_find_ht40intol_overlap(ieee80211_acs_t acs,
                                     struct ieee80211_channel *channel)
{
#define CEILING 12
#define FLOOR    1
#define HT40_NUM_CHANS 5
    struct ieee80211_channel *iter_channel;
    u_int8_t ieee_chan = ieee80211_chan2ieee(acs->acs_ic, channel);
    int i, j, k; 


    int mean_chan; 

    int32_t mode_mask = (IEEE80211_CHAN_11NG_HT40PLUS |
                         IEEE80211_CHAN_11NG_HT40MINUS);


    switch (channel->ic_flags & mode_mask)
    {
        case IEEE80211_CHAN_11NG_HT40PLUS:
            mean_chan = ieee_chan+2;
            break;
        case IEEE80211_CHAN_11NG_HT40MINUS:
            mean_chan = ieee_chan-2;
            break;
        default: /* neither HT40+ nor HT40-, finish this call */
            return;
    }


    /* We should mark the intended channel as IEEE80211_CHAN_HTINTOL 
       if the intended frequency overlaps the iterated channel partially */

    /* According to 802.11n 2009, affected channel = [mean_chan-5, mean_chan+5] */
    for (j=MAX(mean_chan-HT40_NUM_CHANS, FLOOR); j<=MIN(CEILING,mean_chan+HT40_NUM_CHANS); j++) {
          for (i = 0; i < acs->acs_nchans; i++) {
              iter_channel = acs->acs_chans[i];
              k = ieee80211_chan2ieee(acs->acs_ic, iter_channel);

              /* exactly overlapping is allowed. Continue */
              if (k == ieee_chan) continue; 

              if (k  == j) {
                  if (iter_channel->ic_flags & IEEE80211_CHAN_HT40INTOL) {
#if DEBUG_EACS
                      printk( "%s Found on channel %d\n", __func__, j);
#endif
                      channel->ic_flags |= IEEE80211_CHAN_HT40INTOL;
                   }
              }
          }
    }



#undef CEILING
#undef FLOOR
#undef HT40_NUM_CHANS
}

/* 
 * Find all channels with active AP's.
 */
static int
ieee80211_get_ht40intol(void *arg, wlan_scan_entry_t se)
{
    struct ieee80211_channel *scan_chan = ieee80211_scan_entry_channel(se),
                             *iter_chan;
    ieee80211_acs_t acs = (ieee80211_acs_t) arg;
    int i;

    for (i = 0; i < acs->acs_nchans; i++) {
        iter_chan = acs->acs_chans[i];
        if ((ieee80211_chan2ieee(acs->acs_ic, scan_chan)) ==
            ieee80211_chan2ieee(acs->acs_ic, iter_chan)) {
            if (!(iter_chan->ic_flags & IEEE80211_CHAN_HT40INTOL)) {
#if DEBUG_EACS
                printk( "%s Marking channel %d\n", __func__,
                                  ieee80211_chan2ieee(acs->acs_ic, iter_chan));
#endif
            }
            iter_chan->ic_flags |= IEEE80211_CHAN_HT40INTOL;
        }
    }
    return EOK;
}

/* 
 * Trace all entries to record the max rssi found for every channel.
 */
static int
ieee80211_acs_get_channel_maxrssi(void *arg, wlan_scan_entry_t se)
{
    ieee80211_acs_t acs = (ieee80211_acs_t) arg;
    int i,j;
    struct ieee80211_channel *channel_se = ieee80211_scan_entry_channel(se);
    struct ieee80211_channel *channel;
    u_int8_t rssi_se = ieee80211_scan_entry_rssi(se);
    u_int8_t channel_ieee_se = ieee80211_chan2ieee(acs->acs_ic, channel_se);
#if DEBUG_EACS
    printk( "%s chan %d rssi %d noise: %d \n", 
                      __func__, channel_ieee_se, rssi_se, acs->acs_noisefloor[channel_ieee_se]);
#endif

    for (i = 0; i < acs->acs_nchans; i++) {
        channel = acs->acs_chans[i];


        if (channel_ieee_se == ieee80211_chan2ieee(acs->acs_ic, channel)) {
            j = acs->acs_chan_maps[ieee80211_chan2ieee(acs->acs_ic, channel)];
            if (rssi_se > acs->acs_chan_maxrssi[j]) {
                acs->acs_chan_maxrssi[j] = rssi_se;
            }
            break;
        }
    }

    return EOK;
}

static INLINE void ieee80211_acs_check_2ghz_only(ieee80211_acs_t acs, u_int16_t nchans_2ghz)
{
    /* No 5 GHz channels available, skip 5 GHz scan */
    if ((nchans_2ghz) && (acs->acs_nchans == nchans_2ghz)) {
        acs->acs_scan_2ghz_only = 1;
#if DEBUG_EACS
        printk( "%s: No 5 GHz channels available, skip 5 GHz scan\n", __func__);
#endif
    }
}

static INLINE void ieee80211_acs_get_phymode_channels(ieee80211_acs_t acs, enum ieee80211_phymode mode)
{
    struct ieee80211_channel *channel;
    int i;
#if DEBUG_EACS
    printk( "%s: get channels with phy mode=%d\n", __func__, mode);
#endif
    ieee80211_enumerate_channels(channel, acs->acs_ic, i) {
#if DEBUG_EACS
    printk("%s ic_freq: %d ic_ieee: %d ic_flags: %d \n", __func__, channel->ic_freq, channel->ic_ieee, channel->ic_flags);
#endif

        if (mode == ieee80211_chan2mode(channel)) {
#if ATH_SUPPORT_IBSS_ACS
            /*
             * ACS : filter out DFS channels for IBSS mode
             */
            if((wlan_vap_get_opmode(acs->acs_vap) == IEEE80211_M_IBSS) && IEEE80211_IS_CHAN_DISALLOW_ADHOC(channel)) {
#if DEBUG_EACS
                printk( "%s skip DFS-check channel %d\n", __func__, channel->ic_freq);
#endif
                continue;
            }
#endif     	
            if ((wlan_vap_get_opmode(acs->acs_vap) == IEEE80211_M_HOSTAP) && IEEE80211_IS_CHAN_DISALLOW_HOSTAP(channel)) {
#if DEBUG_EACS
                printk( "%s skip Station only channel %d\n", __func__, channel->ic_freq);
#endif
                continue;
            }
#if DEBUG_EACS
            printk("%s adding channel %d %d %d to list\n", __func__, 
                    channel->ic_freq, channel->ic_flags, channel->ic_ieee);
#endif
            
            acs->acs_chan_maps[ieee80211_chan2ieee(acs->acs_ic, channel)] = acs->acs_nchans;
            acs->acs_chans[acs->acs_nchans++] = channel;
        }
    }
}

/*
 * construct the available channel list
 */
static void ieee80211_acs_construct_chan_list(ieee80211_acs_t acs, enum ieee80211_phymode mode)
{
    u_int16_t nchans_2ghz = 0;

    /* reset channel mapping array */
    OS_MEMSET(&acs->acs_chan_maps, 0xff, sizeof(acs->acs_chan_maps));
    acs->acs_nchans = 0;

    if (mode == IEEE80211_MODE_AUTO) {
        /* HT20 only if IEEE80211_MODE_AUTO */
        ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11NG_HT20);
        nchans_2ghz = acs->acs_nchans;
        ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11NA_HT20);
        ieee80211_acs_check_2ghz_only(acs, nchans_2ghz);

        /* If no any HT channel available */
        if (acs->acs_nchans == 0) {
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11G);
            nchans_2ghz = acs->acs_nchans;
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11A);
            ieee80211_acs_check_2ghz_only(acs, nchans_2ghz);
        }

        /* If still no channel available */
        if (acs->acs_nchans == 0) {
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11B);
            acs->acs_scan_2ghz_only = 1;
        }
    } else if (mode == IEEE80211_MODE_11NG_HT40){
        /* if PHY mode is not AUTO, get channel list by PHY mode directly */
        ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11NG_HT40PLUS);
        ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11NG_HT40MINUS);
        acs->acs_scan_2ghz_only = 1;
    } else if (mode == IEEE80211_MODE_11NA_HT40){
        /* if PHY mode is not AUTO, get channel list by PHY mode directly */
        ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11NA_HT40PLUS);
        ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11NA_HT40MINUS);
    } else {
        /* if PHY mode is not AUTO, get channel list by PHY mode directly */
        ieee80211_acs_get_phymode_channels(acs, mode);
    }
}

int ieee80211_acs_attach(ieee80211_acs_t *acs, 
                         wlan_dev_t devhandle, 
                         osdev_t osdev)
{
    if (*acs) 
        return EINPROGRESS; /* already attached ? */

    *acs = (ieee80211_acs_t) OS_MALLOC(osdev, sizeof(struct ieee80211_acs), 0);
    if (*acs) {
        OS_MEMZERO(*acs, sizeof(struct ieee80211_acs));

        /* Save handles to required objects.*/
        (*acs)->acs_ic     = devhandle; 
        (*acs)->acs_osdev  = osdev; 

        spin_lock_init(&((*acs)->acs_lock));

        return EOK;
    }

    return ENOMEM;
}

int ieee80211_acs_detach(ieee80211_acs_t *acs)
{
    if (*acs == NULL) 
        return EINPROGRESS; /* already detached ? */

    /*
   * Free synchronization objects
   */
    spin_lock_destroy(&((*acs)->acs_lock));

    OS_FREE(*acs);

    *acs = NULL;

    return EOK;
}

static void 
ieee80211_acs_post_event(ieee80211_acs_t                 acs,     
                         struct ieee80211_channel       *channel) 
{
    int                                 i,num_handlers;
    ieee80211_acs_event_handler         acs_event_handlers[IEEE80211_MAX_ACS_EVENT_HANDLERS];
    void                                *acs_event_handler_arg[IEEE80211_MAX_ACS_EVENT_HANDLERS];

    /*
     * make a local copy of event handlers list to avoid 
     * the call back modifying the list while we are traversing it.
     */ 
    spin_lock(&acs->acs_lock);
    num_handlers=acs->acs_num_handlers;
    for (i=0; i < num_handlers; ++i) {
        acs_event_handlers[i] = acs->acs_event_handlers[i];
        acs_event_handler_arg[i] = acs->acs_event_handler_arg[i];
    }
    spin_unlock(&acs->acs_lock);
    for (i = 0; i < num_handlers; ++i) {
        (acs_event_handlers[i]) (acs_event_handler_arg[i], channel);
    }
} 

/*
 * scan handler used for scan complete
 */
static void ieee80211_ht40intol_evhandler(struct ieee80211vap *originator,
                                              ieee80211_scan_event *event,
                                              void *arg)
{
    ieee80211_acs_t acs = (ieee80211_acs_t) arg;
    struct ieee80211vap *vap = acs->acs_vap;

    /* 
     * we don't need lock in evhandler since 
     * 1. scan module would guarantee that event handlers won't get called simultaneously
     * 2. acs_in_progress prevent furher access to ACS module
     */
#if 0
#if DEBUG_EACS
    printk( "%s scan_id %08X event %d reason %d \n", __func__, event->scan_id, event->type, event->reason);
#endif
#endif

#if ATH_SUPPORT_MULTIPLE_SCANS
    /*
     * Ignore notifications received due to scans requested by other modules
     * and handle new event IEEE80211_SCAN_DEQUEUED.
     */
    ASSERT(0);

    /* Ignore events reported by scans requested by other modules */
    if (acs->acs_scan_id != event->scan_id) {
        return;
    }
#endif    /* ATH_SUPPORT_MULTIPLE_SCANS */

    if ((event->type != IEEE80211_SCAN_COMPLETED) &&
        (event->type != IEEE80211_SCAN_DEQUEUED)) {
        return;
    }

    if (event->reason != IEEE80211_REASON_COMPLETED) {
#if DEBUG_EACS
        printk( "%s: Scan not totally complete. "
                          "Should not occur normally! Investigate.\n",
                         __func__);
#endif
        goto scan_done;
    }

    wlan_scan_table_iterate(vap, ieee80211_get_ht40intol, acs);
    ieee80211_find_ht40intol_overlap(acs, vap->iv_des_chan[vap->iv_des_mode]);

scan_done:
    ieee80211_free_ht40intol_scan_resource(acs);
    ieee80211_acs_post_event(acs, vap->iv_des_chan[vap->iv_des_mode]);

    acs->acs_in_progress = false;

    return;
}

/*
 * scan handler used for scan complete
 */
static void ieee80211_acs_scan_evhandler(struct ieee80211vap *originator, ieee80211_scan_event *event, void *arg)
{
    ieee80211_acs_t acs = (ieee80211_acs_t) arg;
    struct ieee80211_channel *channel;
#if ATH_SUPPORT_5G_EACS
    struct ieee80211_chan_stats chan_stats;
    u_int32_t temp = 0;
#endif 
    /* 
     * we don't need lock in evhandler since 
     * 1. scan module would guarantee that event handlers won't get called simultaneously
     * 2. acs_in_progress prevent furher access to ACS module
     */
#if DEBUG_EACS
    printk( "%s scan_id %08X event %d reason %d \n", 
                      __func__, event->scan_id, event->type, event->reason);
#endif
    
#if ATH_SUPPORT_MULTIPLE_SCANS
    /*
     * Ignore notifications received due to scans requested by other modules
     * and handle new event IEEE80211_SCAN_DEQUEUED.
     */
    ASSERT(0);

    /* Ignore events reported by scans requested by other modules */
    if (acs->acs_scan_id != event->scan_id) {
        return;
    }
#endif    /* ATH_SUPPORT_MULTIPLE_SCANS */

    /* 
     * Store the Noise floor information in case of channel change and 
     * restart the noise floor computation for the next channel 
     */
    if( event->type == IEEE80211_SCAN_FOREIGN_CHANNEL_GET_NF ) {
        struct ieee80211com *ic = originator->iv_ic;
        /* Get the noise floor value */
        acs->acs_noisefloor[ieee80211_chan2ieee(originator->iv_ic,event->chan)] = ic->ic_get_cur_chan_nf(ic);
#if ATH_SUPPORT_SPECTRAL
        acs->acs_channel_loading[ieee80211_chan2ieee(originator->iv_ic,event->chan)] = ic->ic_get_control_duty_cycle(ic);
#endif

#if ATH_SUPPORT_5G_EACS
        /* get final chan stats for the current channel*/
       (void) ic->ic_get_cur_chan_stats(ic, &chan_stats);

       acs->acs_chan_load[ieee80211_chan2ieee(originator->iv_ic,event->chan)] = 
           chan_stats.chan_clr_cnt - acs->acs_chan_load[ieee80211_chan2ieee(originator->iv_ic,event->chan)] ;
       acs->acs_cycle_count[ieee80211_chan2ieee(originator->iv_ic,event->chan)] = 
           chan_stats.cycle_cnt - acs->acs_cycle_count[ieee80211_chan2ieee(originator->iv_ic,event->chan)] ;
       /* make sure when new clr_cnt is more than old clr cnt, ch utilization is non-zero */
       if (acs->acs_chan_load[ieee80211_chan2ieee(originator->iv_ic,event->chan)] > 1000 ){
           temp = (u_int32_t)(100* acs->acs_chan_load[min((u_int8_t)254,
                       (u_int8_t)ieee80211_chan2ieee(originator->iv_ic,event->chan))]);
           temp = (u_int32_t)( temp/
                   (acs->acs_cycle_count[ieee80211_chan2ieee(originator->iv_ic,event->chan)]));
           temp = MAX( 1,temp);
           acs->acs_chan_load[ieee80211_chan2ieee(originator->iv_ic,event->chan)] = temp;
       } else {
           acs->acs_chan_load[ieee80211_chan2ieee(originator->iv_ic,event->chan)] = 0;
       }
#if DEBUG_EACS  
       printk("%s: final_stats noise: %d ch: %d ch load: %d cycle cnt: %d \n ",__func__, 
               acs->acs_noisefloor[min((u_int8_t)254,
                   (u_int8_t)ieee80211_chan2ieee(originator->iv_ic,event->chan))], 
               ieee80211_chan2ieee( originator->iv_ic,event->chan),
               acs->acs_chan_load[min((u_int8_t)254,
                   (u_int8_t)ieee80211_chan2ieee(originator->iv_ic,event->chan))],
               acs->acs_cycle_count[min((u_int8_t)254,
                   (u_int8_t)ieee80211_chan2ieee(originator->iv_ic,event->chan))] ); 
#endif
#endif
    }
#if ATH_SUPPORT_5G_EACS
    if ( event->type == IEEE80211_SCAN_FOREIGN_CHANNEL ){
        /* get initial chan stats for the current channel */
        struct ieee80211com *ic = originator->iv_ic;
        (void)ic->ic_get_cur_chan_stats(ic, &chan_stats);
        acs->acs_chan_load[ieee80211_chan2ieee(originator->iv_ic,event->chan)] = chan_stats.chan_clr_cnt;
        acs->acs_cycle_count[ieee80211_chan2ieee(originator->iv_ic,event->chan)] = chan_stats.cycle_cnt;
#if DEBUG_EACS 
       printk("%s: initial_stats noise %d ch: %d ch load: %d cycle cnt %d \n ", __func__, 
               acs->acs_noisefloor[min((u_int8_t)254,
                   (u_int8_t)ieee80211_chan2ieee(originator->iv_ic,event->chan))],
               ieee80211_chan2ieee( originator->iv_ic,event->chan), 
               chan_stats.chan_clr_cnt,chan_stats.cycle_cnt ); 
#endif
    }
#endif    

    if (event->type != IEEE80211_SCAN_COMPLETED) {
        return;
    }

    if (event->reason != IEEE80211_REASON_COMPLETED) {
#if DEBUG_EACS
        printk( "%s: Scan not totally complete. Should not occur normally! Investigate.\n",
            __func__);
#endif
        
        channel = ieee80211_find_dot11_channel(acs->acs_ic, 0, acs->acs_vap->iv_des_mode);
        goto scan_done;
    }

    wlan_scan_table_iterate(acs->acs_vap, ieee80211_acs_get_channel_maxrssi, acs);

    channel = ieee80211_acs_find_best_centerchan(acs);

    switch (acs->acs_vap->iv_des_mode) {
    case IEEE80211_MODE_11NG_HT40PLUS:
    case IEEE80211_MODE_11NG_HT40MINUS:
    case IEEE80211_MODE_11NG_HT40:
        wlan_scan_table_iterate(acs->acs_vap, ieee80211_get_ht40intol, acs);
        ieee80211_find_ht40intol_overlap(acs, channel);
    default:
    break;
    }

scan_done:
    ieee80211_acs_free_scan_resource(acs);
    ieee80211_acs_post_event(acs, channel);

    acs->acs_in_progress = false;

    return;
}

static void ieee80211_acs_free_scan_resource(ieee80211_acs_t acs)
{
    int    rc;
    
    /* unregister scan event handler */
    rc = wlan_scan_unregister_event_handler(acs->acs_vap, 
                                            ieee80211_acs_scan_evhandler, 
                                            (void *) acs);
    if (rc != EOK) {
        IEEE80211_DPRINTF(acs->acs_vap, IEEE80211_MSG_ACS,
                          "%s: wlan_scan_unregister_event_handler() failed handler=%08p,%08p rc=%08X\n", 
                          __func__, ieee80211_acs_scan_evhandler, acs, rc);
    }
    wlan_scan_clear_requestor_id(acs->acs_vap, acs->acs_scan_requestor);
}

static void ieee80211_free_ht40intol_scan_resource(ieee80211_acs_t acs)
{
    int    rc;
    
    /* unregister scan event handler */
    rc = wlan_scan_unregister_event_handler(acs->acs_vap,
                                            ieee80211_ht40intol_evhandler,
                                            (void *) acs);
    if (rc != EOK) {
        IEEE80211_DPRINTF(acs->acs_vap, IEEE80211_MSG_ACS,
                          "%s: wlan_scan_unregister_event_handler() failed handler=%08p,%08p rc=%08X\n", 
                          __func__, ieee80211_ht40intol_evhandler, acs, rc);
    }
    wlan_scan_clear_requestor_id(acs->acs_vap, acs->acs_scan_requestor);
}

static INLINE void
ieee80211_acs_iter_vap_channel(void *arg, struct ieee80211vap *vap, bool is_last_vap)
{
    struct ieee80211vap *current_vap = (struct ieee80211vap *) arg; 
    struct ieee80211_channel *channel;

    if (wlan_vap_get_opmode(vap) != IEEE80211_M_HOSTAP) {
        return;
    }
    if (ieee80211_acs_channel_is_set(current_vap)) {
        return;
    }
    if (vap == current_vap) {
        return;
    }

    if (ieee80211_acs_channel_is_set(vap)) {
        channel =  vap->iv_des_chan[vap->iv_des_mode];
        current_vap->iv_ic->ic_acs->acs_channel = channel;
    }

}

static int ieee80211_find_ht40intol_bss(struct ieee80211vap *vap)
{
/* XXX tunables */
#define MIN_DWELL_TIME        200  /* 200 ms */
#define MAX_DWELL_TIME        300  /* 300 ms */
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_acs_t acs = ic->ic_acs;
    ieee80211_scan_params scan_params; 
    int rc;

    spin_lock(&acs->acs_lock);
    if (acs->acs_in_progress) {
        /* Just wait for acs done */
        spin_unlock(&acs->acs_lock);
        return EINPROGRESS;
    }

    acs->acs_in_progress = true;

    spin_unlock(&acs->acs_lock);

    /* acs_in_progress prevents others from reaching here so unlocking is OK */

    acs->acs_vap = vap;

    /* reset channel mapping array */
    OS_MEMSET(&acs->acs_chan_maps, 0xff, sizeof(acs->acs_chan_maps));
    acs->acs_nchans = 0;
    /* Get 11NG HT20 channels */
    ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11NG_HT20);

    if (acs->acs_nchans == 0) {
#if DEBUG_EACS
        printk("%s: Cannot construct the available channel list.\n",
                          __func__);
#endif
        goto err; 
    }

    /* register scan event handler */
    rc = wlan_scan_register_event_handler(vap, ieee80211_ht40intol_evhandler, (void *) acs);
    if (rc != EOK) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                          "%s: wlan_scan_register_event_handler() failed handler=%08p,%08p rc=%08X\n",
                          __func__, ieee80211_ht40intol_evhandler, acs, rc);
    }
    wlan_scan_get_requestor_id(vap, (u_int8_t*)"acs", &acs->acs_scan_requestor);

    /* Fill scan parameter */
    OS_MEMZERO(&scan_params, sizeof(ieee80211_scan_params));
    wlan_set_default_scan_parameters(vap,&scan_params,IEEE80211_M_HOSTAP,
                                     false,true,false,true,0,NULL,0);

    scan_params.type = IEEE80211_SCAN_FOREGROUND;
    scan_params.flags = IEEE80211_SCAN_PASSIVE;
    scan_params.min_dwell_time_passive = MIN_DWELL_TIME;
    scan_params.max_dwell_time_passive = MAX_DWELL_TIME;
    scan_params.flags |= IEEE80211_SCAN_2GHZ; 

    /* Try to issue a scan */
    if (wlan_scan_start(vap,
                        &scan_params,
                        acs->acs_scan_requestor,
                        IEEE80211_SCAN_PRIORITY_LOW,
                        &(acs->acs_scan_id)) != EOK) {
#if DEBUG_EACS
        printk( "%s: Issue a scan fail.\n",
                          __func__);
#endif
        goto err;
    }

    return EOK;

err:
    ieee80211_free_ht40intol_scan_resource(acs);
    acs->acs_in_progress = false;
    ieee80211_acs_post_event(acs, vap->iv_des_chan[vap->iv_des_mode]);
    return EOK;
#undef MIN_DWELL_TIME
#undef MAX_DWELL_TIME
}

static int ieee80211_autoselect_infra_bss_channel(struct ieee80211vap *vap)
{
/* XXX tunables */
#define MIN_DWELL_TIME        200  /* 200 ms */
#define MAX_DWELL_TIME        300  /* 300 ms */

    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_acs_t acs = ic->ic_acs;
    struct ieee80211_channel *channel;
    ieee80211_scan_params scan_params; 
    u_int32_t num_vaps;
    int rc;

    spin_lock(&acs->acs_lock);
    if (acs->acs_in_progress) {
        /* Just wait for acs done */
        spin_unlock(&acs->acs_lock);
        return EINPROGRESS;
    }

    /* check if any VAP already set channel */
    acs->acs_channel = NULL;
    ieee80211_iterate_vap_list_internal(ic, ieee80211_acs_iter_vap_channel,vap,num_vaps);

    acs->acs_in_progress = true;

    spin_unlock(&acs->acs_lock);

    /* acs_in_progress prevents others from reaching here so unlocking is OK */

    if (acs->acs_channel && (!ic->cw_inter_found)) {
        /* wlan scanner not yet started so acs_in_progress = true is OK */
        ieee80211_acs_post_event(acs, acs->acs_channel);
        acs->acs_in_progress = false;
        return EOK;
    }

    acs->acs_vap = vap;

#if 0
    /* during startup, random channel selection is not required */
    if (ic->ic_dfs_isdfsregdomain(ic) &&
#if ATH_SUPPORT_IBSS_ACS
        (wlan_vap_get_opmode(vap) != IEEE80211_M_IBSS) && /* IBSS mode : force ACS */
#endif    	
        ((vap->iv_des_mode == IEEE80211_MODE_11A) ||
        (vap->iv_des_mode == IEEE80211_MODE_11NA_HT20) ||
        (vap->iv_des_mode == IEEE80211_MODE_11NA_HT40PLUS) ||
        (vap->iv_des_mode == IEEE80211_MODE_11NA_HT40MINUS)))
    {
        channel = ieee80211_acs_pickup_random_channel(vap);
        ieee80211_acs_post_event(acs, channel);
        acs->acs_in_progress = false;
        goto acs_done;
    }
#endif


    ieee80211_acs_construct_chan_list(acs,acs->acs_vap->iv_des_mode);
    if (acs->acs_nchans == 0) {
#if DEBUG_EACS
        printk( "%s: Cannot construct the available channel list.\n", __func__);
#endif
        goto err; 
    }

    /* register scan event handler */
    rc = wlan_scan_register_event_handler(vap, ieee80211_acs_scan_evhandler, (void *) acs);
    if (rc != EOK) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACS,
                          "%s: wlan_scan_register_event_handler() failed handler=%08p,%08p rc=%08X\n", 
                          __func__, ieee80211_acs_scan_evhandler, (void *) acs, rc);
    }
    wlan_scan_get_requestor_id(vap,(u_int8_t*)"acs", &acs->acs_scan_requestor);

    /* Fill scan parameter */
    OS_MEMZERO(&scan_params,sizeof(ieee80211_scan_params));
    wlan_set_default_scan_parameters(vap,&scan_params,IEEE80211_M_HOSTAP,false,true,false,true,0,NULL,0);

    scan_params.type = IEEE80211_SCAN_FOREGROUND;
    scan_params.flags = IEEE80211_SCAN_PASSIVE;
    scan_params.min_dwell_time_passive = MIN_DWELL_TIME;
    scan_params.max_dwell_time_passive = MAX_DWELL_TIME;

    switch (vap->iv_des_mode)
    {
    case IEEE80211_MODE_11A:
    case IEEE80211_MODE_TURBO_A:
    case IEEE80211_MODE_11NA_HT20:
    case IEEE80211_MODE_11NA_HT40PLUS:
    case IEEE80211_MODE_11NA_HT40MINUS:
    case IEEE80211_MODE_11NA_HT40:
        scan_params.flags |= IEEE80211_SCAN_5GHZ; 
        break;

    case IEEE80211_MODE_11B:
    case IEEE80211_MODE_11G:
    case IEEE80211_MODE_TURBO_G:
    case IEEE80211_MODE_11NG_HT20:
    case IEEE80211_MODE_11NG_HT40PLUS:
    case IEEE80211_MODE_11NG_HT40MINUS:
    case IEEE80211_MODE_11NG_HT40:
        scan_params.flags |= IEEE80211_SCAN_2GHZ; 
        break;

    default:
        if (acs->acs_scan_2ghz_only) {
            scan_params.flags |= IEEE80211_SCAN_2GHZ; 
        } else {
            scan_params.flags |= IEEE80211_SCAN_ALLBANDS; 
        }
        break;
    }

    /* Try to issue a scan */
#if ATH_SUPPORT_SPECTRAL
    ic->ic_start_spectral_scan(ic);
#endif

    /*Flush scan table before starting scan */
    wlan_scan_table_flush(vap);
    if (wlan_scan_start(vap,
                        &scan_params,
                        acs->acs_scan_requestor,
                        IEEE80211_SCAN_PRIORITY_LOW,
                        &(acs->acs_scan_id)) != EOK) {
#if DEBUG_EACS
        printk( "%s: Issue a scan fail.\n",
            __func__);
#endif
        goto err2; 
    }

    return EOK;

err2:
    ieee80211_acs_free_scan_resource(acs);
err:
    /* select the first available channel to start */
    channel = ieee80211_find_dot11_channel(ic, 0, vap->iv_des_mode);
    ieee80211_acs_post_event(acs, channel);
#if DEBUG_EACS
    printk( "%s: Use the first available channel=%d to start\n",
        __func__, ieee80211_chan2ieee(ic, channel));
#endif

#if 0
acs_done:
#endif
#if ATH_SUPPORT_SPECTRAL
    ic->ic_stop_spectral_scan(ic);
#endif
    acs->acs_in_progress = false;
    return EOK;
#undef MIN_DWELL_TIME
#undef MAX_DWELL_TIME
}

static int ieee80211_acs_register_event_handler(ieee80211_acs_t          acs, 
                                         ieee80211_acs_event_handler evhandler, 
                                         void                         *arg)
{
    int    i;

    for (i = 0; i < IEEE80211_MAX_ACS_EVENT_HANDLERS; ++i) {
        if ((acs->acs_event_handlers[i] == evhandler) &&
            (acs->acs_event_handler_arg[i] == arg)) {
            return EEXIST; /* already exists */
        }
    }

    if (acs->acs_num_handlers >= IEEE80211_MAX_ACS_EVENT_HANDLERS) {
        return ENOSPC;
    }

    spin_lock(&acs->acs_lock);
    acs->acs_event_handlers[acs->acs_num_handlers] = evhandler;
    acs->acs_event_handler_arg[acs->acs_num_handlers++] = arg;
    spin_unlock(&acs->acs_lock);

    return EOK;
}

static int ieee80211_acs_unregister_event_handler(ieee80211_acs_t          acs, 
                                           ieee80211_acs_event_handler evhandler, 
                                           void                         *arg)
{
    int    i;

    spin_lock(&acs->acs_lock);
    for (i = 0; i < IEEE80211_MAX_ACS_EVENT_HANDLERS; ++i) {
        if ((acs->acs_event_handlers[i] == evhandler) &&
            (acs->acs_event_handler_arg[i] == arg)) {
            /* replace event handler being deleted with the last one in the list */
            acs->acs_event_handlers[i]    = acs->acs_event_handlers[acs->acs_num_handlers - 1];
            acs->acs_event_handler_arg[i] = acs->acs_event_handler_arg[acs->acs_num_handlers - 1];

            /* clear last event handler in the list */
            acs->acs_event_handlers[acs->acs_num_handlers - 1]    = NULL;
            acs->acs_event_handler_arg[acs->acs_num_handlers - 1] = NULL;
            acs->acs_num_handlers--;

            spin_unlock(&acs->acs_lock);

            return EOK;
        }
    }
    spin_unlock(&acs->acs_lock);

    return ENXIO;
   
}

int wlan_autoselect_register_event_handler(wlan_if_t                    vaphandle, 
                                           ieee80211_acs_event_handler evhandler, 
                                           void                         *arg)
{
    return ieee80211_acs_register_event_handler(vaphandle->iv_ic->ic_acs,
                                                 evhandler, 
                                                 arg);
}

int wlan_autoselect_unregister_event_handler(wlan_if_t                    vaphandle, 
                                             ieee80211_acs_event_handler evhandler,
                                             void                         *arg)
{
    return ieee80211_acs_unregister_event_handler(vaphandle->iv_ic->ic_acs, evhandler, arg);
}

int wlan_autoselect_in_progress(wlan_if_t vaphandle)
{
    return ieee80211_acs_in_progress(vaphandle->iv_ic->ic_acs);
}

int wlan_autoselect_find_infra_bss_channel(wlan_if_t             vaphandle)
{
    return ieee80211_autoselect_infra_bss_channel(vaphandle);
}

int wlan_attempt_ht40_bss(wlan_if_t             vaphandle)
{
    return ieee80211_find_ht40intol_bss(vaphandle);
}

int wlan_acs_find_best_channel(struct ieee80211vap *vap, int *bestfreq, int num)
{
    
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_channel *best_11na = NULL;
    struct ieee80211_channel *best_11ng = NULL;
    struct ieee80211_channel *best_overall = NULL;
    //struct ieee80211_acs myacs;
    int retv = 0;
     
    ieee80211_acs_t acs = ic->ic_acs;
    ieee80211_acs_t temp_acs;
    
    
    //temp_acs = &myacs;

    //temp_acs = (ieee80211_acs_t) kmalloc(sizeof(struct ieee80211_acs), GFP_KERNEL);
    temp_acs = (ieee80211_acs_t) OS_MALLOC(acs->acs_osdev, sizeof(struct ieee80211_acs), 0);
    
    if (temp_acs) {
        OS_MEMZERO(temp_acs, sizeof(struct ieee80211_acs));
    }
    else {
       adf_os_print("%s: malloc failed \n",__func__);
       return ENOMEM;
    }
    
    temp_acs->acs_ic = ic;
    temp_acs->acs_vap = vap;
    temp_acs->acs_osdev = acs->acs_osdev;
    temp_acs->acs_num_handlers = 0;
    temp_acs->acs_in_progress = true;
    temp_acs->acs_scan_2ghz_only = 0;
    temp_acs->acs_channel = NULL;
    temp_acs->acs_nchans = 0;
    
    
    
    ieee80211_acs_construct_chan_list(temp_acs,IEEE80211_MODE_AUTO);
    if (temp_acs->acs_nchans == 0) {
#if DEBUG_EACS
        printk( "%s: Cannot construct the available channel list.\n", __func__);
#endif
        retv = -1;
        goto err; 
    }
    
        
    
    wlan_scan_table_iterate(temp_acs->acs_vap, ieee80211_acs_get_channel_maxrssi, temp_acs);

    best_11na = ieee80211_acs_find_best_11na_centerchan(temp_acs);
    best_11ng = ieee80211_acs_find_best_11ng_centerchan(temp_acs);

    if (temp_acs->acs_minrssi_11na > temp_acs->acs_avgrssi_11ng) {
        best_overall = best_11ng;
    } else {
        best_overall = best_11na;
    }    
 
    if( best_11na==NULL || best_11ng==NULL || best_overall==NULL) {
        adf_os_print("%s: null channel \n",__func__);
        retv = -1;
        goto err;
    }
    
    bestfreq[0] = (int) best_11na->ic_freq;
    bestfreq[1] = (int) best_11ng->ic_freq;
    bestfreq[2] = (int) best_overall->ic_freq;

err:
    OS_FREE(temp_acs);
    //kfree(temp_acs);
    return retv;
    
}

#else

int wlan_autoselect_register_event_handler(wlan_if_t                    vaphandle, 
                                           ieee80211_acs_event_handler evhandler, 
                                           void                         *arg)
{
    return EPERM;
}

int wlan_autoselect_unregister_event_handler(wlan_if_t                    vaphandle, 
                                             ieee80211_acs_event_handler evhandler,
                                             void                         *arg)
{
    return EPERM;
}

int wlan_autoselect_in_progress(wlan_if_t vaphandle)
{
    return 0;
}

int wlan_autoselect_find_infra_bss_channel(wlan_if_t             vaphandle)
{
    return EPERM;
}

int wlan_attempt_ht40_bss(wlan_if_t             vaphandle)
{
    return EPERM;
}

int wlan_acs_find_best_channel(struct ieee80211vap *vap, int *bestfreq, int num){

    return EINVAL;
}

#undef DEBUG_EACS 
#endif

