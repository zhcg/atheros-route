/*
 * Copyright (c) 2009, Atheros Communications Inc.
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
 * 
 */

#include "ath_internal.h"
#include "ath_antdiv.h"

#ifndef REMOVE_PKT_LOG
#include "pktlog.h"
extern struct ath_pktlog_funcs *g_pktlog_funcs;
#define MAX_LOG 128
#endif

/*
 * Conditional compilation to avoid errors if ATH_SLOW_ANT_DIV is not defined
 */
#if ATH_SLOW_ANT_DIV

void
ath_slow_ant_div_init(struct ath_antdiv *antdiv, struct ath_softc *sc, int32_t rssitrig,
                      u_int32_t min_dwell_time, u_int32_t settle_time)
{
    int    default_antenna;

    spin_lock_init(&(antdiv->antdiv_lock));
    
    antdiv->antdiv_sc             = sc;
    antdiv->antdiv_rssitrig       = rssitrig;
    antdiv->antdiv_min_dwell_time = min_dwell_time * 1000;
    antdiv->antdiv_settle_time    = settle_time * 1000;

    if (! ath_hal_get_config_param(sc->sc_ah, HAL_CONFIG_DEFAULTANTENNASET, &default_antenna))
    {
        default_antenna = 0;
    }
    antdiv->antdiv_def_antcfg = default_antenna;
}

void
ath_slow_ant_div_start(struct ath_antdiv *antdiv, u_int8_t num_antcfg, const u_int8_t *bssid)
{
    spin_lock(&antdiv->antdiv_lock);
    
    antdiv->antdiv_num_antcfg   = num_antcfg < ATH_ANT_DIV_MAX_CFG ? num_antcfg : ATH_ANT_DIV_MAX_CFG;
    antdiv->antdiv_state        = ATH_ANT_DIV_IDLE;
    antdiv->antdiv_curcfg       = 0;
    antdiv->antdiv_bestcfg      = 0;
    antdiv->antdiv_laststatetsf = 0;
    antdiv->antdiv_suspend      = 0;

    OS_MEMCPY(antdiv->antdiv_bssid, bssid, sizeof(antdiv->antdiv_bssid));
    
    antdiv->antdiv_start = 1;
    
    spin_unlock(&antdiv->antdiv_lock);
}

void
ath_slow_ant_div_stop(struct ath_antdiv *antdiv)
{
    spin_lock(&antdiv->antdiv_lock);

    /* 
     * Lose suspend/resume state if Antenna Diversity is disabled.
     */    
    antdiv->antdiv_suspend = 0;
    antdiv->antdiv_start   = 0;
    
    spin_unlock(&antdiv->antdiv_lock);
}

void
ath_slow_ant_div_suspend(ath_dev_t dev)
{
    struct ath_softc    *sc     = ATH_DEV_TO_SC(dev);
    struct ath_hal      *ah     = sc->sc_ah;
    struct ath_antdiv   *antdiv = &sc->sc_antdiv;
    
    spin_lock(&antdiv->antdiv_lock);
    
    /* 
     * Suspend Antenna Diversity and select default antenna.
     */    
    if (antdiv->antdiv_start) {
        antdiv->antdiv_suspend = 1;
        if (HAL_OK == ath_hal_selectAntConfig(ah, antdiv->antdiv_def_antcfg)) {
            antdiv->antdiv_curcfg = antdiv->antdiv_def_antcfg;
        }
    }
    
    spin_unlock(&antdiv->antdiv_lock);
}

void
ath_slow_ant_div_resume(ath_dev_t dev)
{
    struct ath_softc    *sc     = ATH_DEV_TO_SC(dev);
    struct ath_antdiv   *antdiv = &sc->sc_antdiv;

    spin_lock(&antdiv->antdiv_lock);
    
    /* 
     * Resume Antenna Diversity.
     * No need to restore previous setting. Algorithm will automatically
     * choose a different antenna if the default is not appropriate.
     */    
    antdiv->antdiv_suspend = 0;
    
    spin_unlock(&antdiv->antdiv_lock);
}

static void find_max_sdev(int8_t *val_in, u_int8_t num_val, u_int8_t *max_index, u_int16_t *sdev)
{
    int8_t   *max_val, *val = val_in, *val_last = val_in + num_val;

    if (val_in && num_val) {
        max_val = val;
        while (++val != val_last) {
            if (*val > *max_val)
                max_val = val;
        };

        if (max_index)
            *max_index = (u_int8_t)(max_val - val_in);

        if (sdev) {
            val = val_in;
            *sdev = 0;
            do {
                *sdev += (*max_val - *val);
            } while (++val != val_last);
        }
    }
}

void
ath_slow_ant_div(struct ath_antdiv *antdiv, struct ieee80211_frame *wh, struct ath_rx_status *rx_stats)
{
    struct ath_softc *sc = antdiv->antdiv_sc;
    struct ath_hal   *ah = sc->sc_ah;
    u_int64_t curtsf = 0;
    u_int8_t  bestcfg, bestcfg_chain[ATH_ANT_DIV_MAX_CHAIN], curcfg = antdiv->antdiv_curcfg;
    u_int16_t sdev_chain[ATH_ANT_DIV_MAX_CHAIN];
#ifndef REMOVE_PKT_LOG
    char logtext[MAX_LOG];
#endif

    spin_lock(&antdiv->antdiv_lock);
    
    if (antdiv->antdiv_start && 
        (! antdiv->antdiv_suspend) && 
        IEEE80211_IS_BEACON(wh) && 
        ATH_ADDR_EQ(wh->i_addr3, antdiv->antdiv_bssid)){
        antdiv->antdiv_lastbrssictl[0][curcfg] = rx_stats->rs_rssi_ctl0;
        antdiv->antdiv_lastbrssictl[1][curcfg] = rx_stats->rs_rssi_ctl1;
        antdiv->antdiv_lastbrssictl[2][curcfg] = rx_stats->rs_rssi_ctl2;
/* not yet
        antdiv->antdiv_lastbrssi[curcfg] = rx_stats->rs_rssi;
        antdiv->antdiv_lastbrssiext[0][curcfg] = rx_stats->rs_rssi_ext0;
        antdiv->antdiv_lastbrssiext[1][curcfg] = rx_stats->rs_rssi_ext1;
        antdiv->antdiv_lastbrssiext[2][curcfg] = rx_stats->rs_rssi_ext2;
*/
        antdiv->antdiv_lastbtsf[curcfg] = ath_hal_gettsf64(sc->sc_ah);
        curtsf = antdiv->antdiv_lastbtsf[curcfg];
    } else {
        spin_unlock(&antdiv->antdiv_lock);    
        return;
    }

    switch (antdiv->antdiv_state) {
    case ATH_ANT_DIV_IDLE:
        if ((curtsf - antdiv->antdiv_laststatetsf) > antdiv->antdiv_min_dwell_time) {
            int8_t min_rssi = antdiv->antdiv_lastbrssictl[0][curcfg];
            int      i;
            for (i = 1; i < ATH_ANT_DIV_MAX_CHAIN; i++) {
                if (antdiv->antdiv_lastbrssictl[i][curcfg] != (-128)) {
                    if (antdiv->antdiv_lastbrssictl[i][curcfg] < min_rssi) {
                        min_rssi = antdiv->antdiv_lastbrssictl[i][curcfg];
                    }
                }
            }
            if (min_rssi < antdiv->antdiv_rssitrig) {
                curcfg ++;
                if (curcfg == antdiv->antdiv_num_antcfg) {
                    curcfg = 0;
                }

                if (HAL_OK == ath_hal_selectAntConfig(ah, curcfg)) {
                    antdiv->antdiv_bestcfg = antdiv->antdiv_curcfg;
                    antdiv->antdiv_curcfg = curcfg;
                    antdiv->antdiv_laststatetsf = curtsf;
                    antdiv->antdiv_state = ATH_ANT_DIV_SCAN;
#ifndef REMOVE_PKT_LOG
                    /* do pktlog */
                    {
                        snprintf(logtext, MAX_LOG, "SlowAntDiv: select_antcfg = %d\n", antdiv->antdiv_curcfg);
                        ath_log_text(sc, logtext, 0); 
                    }
#endif
                }
            }
        }
        break;
    
    case ATH_ANT_DIV_SCAN:
        if ((curtsf - antdiv->antdiv_laststatetsf) < antdiv->antdiv_settle_time)
            break;

        curcfg ++;
        if (curcfg == antdiv->antdiv_num_antcfg) {
            curcfg = 0;
        }

        if (curcfg == antdiv->antdiv_bestcfg) {
            u_int16_t *max_sdev;
            int       i;
            for (i = 0; i < ATH_ANT_DIV_MAX_CHAIN; i++) {
                find_max_sdev(&antdiv->antdiv_lastbrssictl[i][0], antdiv->antdiv_num_antcfg, &bestcfg_chain[i], &sdev_chain[i]);
#ifndef REMOVE_PKT_LOG
                /* do pktlog */
                {
                    snprintf(logtext, MAX_LOG, "SlowAntDiv: best cfg (chain%d) = %d (cfg0:%d, cfg1:%d, sdev:%d)\n", i, 
                            bestcfg_chain[i], antdiv->antdiv_lastbrssictl[i][0],
                            antdiv->antdiv_lastbrssictl[i][1], sdev_chain[i]);
                    ath_log_text(sc, logtext, 0); 
                }
#endif
            }

            max_sdev = sdev_chain;
            for (i = 1; i < ATH_ANT_DIV_MAX_CHAIN; i++) {
                if (*(sdev_chain + i) > *max_sdev) {
                    max_sdev = sdev_chain + i;
                }
            }
            bestcfg = bestcfg_chain[(max_sdev - sdev_chain)];

            if (bestcfg != antdiv->antdiv_curcfg) {
                if (HAL_OK == ath_hal_selectAntConfig(ah, bestcfg)) {
                    antdiv->antdiv_bestcfg = bestcfg;
                    antdiv->antdiv_curcfg = bestcfg;
#ifndef REMOVE_PKT_LOG
                    /* do pktlog */
                    {
                        snprintf(logtext, MAX_LOG, "SlowAntDiv: select best cfg = %d\n", antdiv->antdiv_curcfg);
                        ath_log_text(sc, logtext, 0); 
                    }
#endif
                }
            }
            antdiv->antdiv_laststatetsf = curtsf;
            antdiv->antdiv_state = ATH_ANT_DIV_IDLE;

        } else {
            if (HAL_OK == ath_hal_selectAntConfig(ah, curcfg)) {
                antdiv->antdiv_curcfg = curcfg;
                antdiv->antdiv_laststatetsf = curtsf;
                antdiv->antdiv_state = ATH_ANT_DIV_SCAN;
#ifndef REMOVE_PKT_LOG
                /* do pktlog */
                {
                    snprintf(logtext, MAX_LOG, "SlowAntDiv: select ant cfg = %d\n", antdiv->antdiv_curcfg);
                    ath_log_text(sc, logtext, 0); 
                }
#endif
            }
        }

        break;
    }
    
    spin_unlock(&antdiv->antdiv_lock);    
}

#endif    /* ATH_SLOW_ANT_DIV */

#if ATH_ANT_DIV_COMB

static int
ath_ant_div_comb_alt_check(u_int config_group, int alt_ratio, int curr_main_set, int curr_alt_set, int alt_rssi_avg, int main_rssi_avg)
{
    int result = 0;
    switch(config_group) {
    case HAL_ANTDIV_CONFIG_GROUP_1:
    case HAL_ANTDIV_CONFIG_GROUP_2:
        if((((curr_main_set == HAL_ANT_DIV_COMB_LNA2) && (curr_alt_set == HAL_ANT_DIV_COMB_LNA1) && (alt_rssi_avg >= main_rssi_avg - 5)) ||
            ((curr_main_set == HAL_ANT_DIV_COMB_LNA1) && (curr_alt_set == HAL_ANT_DIV_COMB_LNA2) && (alt_rssi_avg >= main_rssi_avg - 2)) ||
            (alt_ratio > ANT_DIV_COMB_ALT_ANT_RATIO)) &&
            (alt_rssi_avg >= 4))
        {
            result = 1;
        } else {
            result = 0;
        }
        break;

    case DEFAULT_ANTDIV_CONFIG_GROUP:
    default:
        (alt_ratio > ANT_DIV_COMB_ALT_ANT_RATIO) ? (result = 1) : (result = 0);
        break;
    }
    
    return result;
}

static void
ath_ant_adjust_fast_div_bias_tp(struct ath_antcomb *antcomb, int alt_ratio, u_int config_group, HAL_ANT_COMB_CONFIG *pdiv_ant_conf)
{

    if(config_group == HAL_ANTDIV_CONFIG_GROUP_1)
    {
        
        switch ((pdiv_ant_conf->main_lna_conf << 4) | pdiv_ant_conf->alt_lna_conf) {
            case (0x01): //A-B LNA2
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x02): //A-B LNA1
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x03): //A-B A+B
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;        
            case (0x10): //LNA2 A-B
                if ((antcomb->scan == 0) && (alt_ratio > ANT_DIV_COMB_ALT_ANT_RATIO)) {
                  pdiv_ant_conf->fast_div_bias = 0x3f;
                }else{
                  pdiv_ant_conf->fast_div_bias = 0x1;
                }  
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;        
            case (0x12): //LNA2 LNA1
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x13): //LNA2 A+B
                if ((antcomb->scan == 0) && (alt_ratio > ANT_DIV_COMB_ALT_ANT_RATIO)) {
                  pdiv_ant_conf->fast_div_bias = 0x3f;
                }else{
                  pdiv_ant_conf->fast_div_bias = 0x1;
                }
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;         
            case (0x20): //LNA1 A-B
                if ((antcomb->scan == 0) && (alt_ratio > ANT_DIV_COMB_ALT_ANT_RATIO)) {
                    pdiv_ant_conf->fast_div_bias = 0x3f;
                }else{
                    pdiv_ant_conf->fast_div_bias = 0x1;
                }           
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;        
            case (0x21): //LNA1 LNA2
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x23): //LNA1 A+B
                if ((antcomb->scan == 0) && (alt_ratio > ANT_DIV_COMB_ALT_ANT_RATIO)) {
                  pdiv_ant_conf->fast_div_bias = 0x3f;
                }else{
                  pdiv_ant_conf->fast_div_bias = 0x1;
                }    
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;                            
            case (0x30): //A+B A-B
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;        
            case (0x31): //A+B LNA2
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x32): //A+B LNA1
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;  
            default:
                break;
          }    
    } else if (config_group == HAL_ANTDIV_CONFIG_GROUP_2) {
        switch ((pdiv_ant_conf->main_lna_conf << 4) | pdiv_ant_conf->alt_lna_conf) {
            case (0x01): //A-B LNA2
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x02): //A-B LNA1
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x03): //A-B A+B
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x10): //LNA2 A-B
                if ((antcomb->scan == 0) && (alt_ratio > ANT_DIV_COMB_ALT_ANT_RATIO)) {
                    pdiv_ant_conf->fast_div_bias = 0x1;
                } else {
                    pdiv_ant_conf->fast_div_bias = 0x2;
                }
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x12): //LNA2 LNA1
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x13): //LNA2 A+B
                if ((antcomb->scan == 0) && (alt_ratio > ANT_DIV_COMB_ALT_ANT_RATIO)) {
                    pdiv_ant_conf->fast_div_bias = 0x1;
                } else {
                    pdiv_ant_conf->fast_div_bias = 0x2;
                }
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x20): //LNA1 A-B
                if ((antcomb->scan == 0) && (alt_ratio > ANT_DIV_COMB_ALT_ANT_RATIO)) {
                    pdiv_ant_conf->fast_div_bias = 0x1;
                } else {
                    pdiv_ant_conf->fast_div_bias = 0x2;
                }
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x21): //LNA1 LNA2
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x23): //LNA1 A+B
                if ((antcomb->scan == 0) && (alt_ratio > ANT_DIV_COMB_ALT_ANT_RATIO)) {
                    pdiv_ant_conf->fast_div_bias = 0x1;
                } else {
                    pdiv_ant_conf->fast_div_bias = 0x2;
                }
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x30): //A+B A-B
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x31): //A+B LNA2
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x32): //A+B LNA1
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            default:
                break;
        }
    } else { /* DEFAULT_ANTDIV_CONFIG_GROUP */
        switch ((pdiv_ant_conf->main_lna_conf << 4) | pdiv_ant_conf->alt_lna_conf) {
            case (0x01): //A-B LNA2
                 pdiv_ant_conf->fast_div_bias = 0x3b;
                 break;
             case (0x02): //A-B LNA1
                 pdiv_ant_conf->fast_div_bias = 0x3d;
                 break;
             case (0x03): //A-B A+B
                 pdiv_ant_conf->fast_div_bias = 0x1;
                 break;        
             case (0x10): //LNA2 A-B
                 pdiv_ant_conf->fast_div_bias = 0x7;
                 break;        
             case (0x12): //LNA2 LNA1
                 pdiv_ant_conf->fast_div_bias = 0x2;
                 break;
             case (0x13): //LNA2 A+B
                 pdiv_ant_conf->fast_div_bias = 0x7;
                 break;         
             case (0x20): //LNA1 A-B
                 pdiv_ant_conf->fast_div_bias = 0x6;
                 break;        
             case (0x21): //LNA1 LNA2
                 pdiv_ant_conf->fast_div_bias = 0x0;
                 break;
             case (0x23): //LNA1 A+B
                 pdiv_ant_conf->fast_div_bias = 0x6;
                 break;                            
             case (0x30): //A+B A-B
                 pdiv_ant_conf->fast_div_bias = 0x1;
                 break;        
             case (0x31): //A+B LNA2
                 pdiv_ant_conf->fast_div_bias = 0x3b;
                 break;
             case (0x32): //A+B LNA1
                 pdiv_ant_conf->fast_div_bias = 0x3d;
                 break;  
             default:
                 break;
           }    
    }
}

static void
ath_ant_tune_comb_LNA1_value(HAL_ANT_COMB_CONFIG         *pdiv_ant_conf)
{
    uint8_t config_group = pdiv_ant_conf->antdiv_configgroup;
    if ((config_group == HAL_ANTDIV_CONFIG_GROUP_1) ||
        (config_group == HAL_ANTDIV_CONFIG_GROUP_2)) {
        pdiv_ant_conf->lna1_lna2_delta = -9;
    } else { /* DEFAULT_ANTDIV_CONFIG_GROUP */
        pdiv_ant_conf->lna1_lna2_delta = -3;
    }
}

void
ath_ant_div_comb_init(struct ath_antcomb *antcomb, struct ath_softc *sc)
{
    HAL_ANT_COMB_CONFIG div_ant_conf;
    struct ath_hal *ah = sc->sc_ah;
    OS_MEMZERO(antcomb, sizeof(struct ath_antcomb));
    antcomb->antcomb_sc = sc;
    antcomb->count = ANT_DIV_COMB_INIT_COUNT; 
    ath_hal_getAntDivCombConf(ah, &div_ant_conf);
    ath_ant_tune_comb_LNA1_value(&div_ant_conf);
}


void 
ath_ant_div_comb_scan(struct ath_antcomb *antcomb, struct ath_rx_status *rx_stats)
{
    struct ath_softc *sc = antcomb->antcomb_sc;
    struct ath_hal *ah = sc->sc_ah;
    int alt_ratio, alt_rssi_avg, main_rssi_avg, curr_alt_set;
    int curr_main_set, curr_bias;
    int8_t main_rssi = rx_stats->rs_rssi_ctl0;
    int8_t alt_rssi = rx_stats->rs_rssi_ctl1;
    int8_t end_st = 0;
    int8_t rx_ant_conf,  main_ant_conf;
    u_int8_t rx_pkt_moreaggr = rx_stats->rs_moreaggr;
    u_int8_t short_scan = 0;
    u_int32_t comb_delta_time = 0;
    HAL_ANT_COMB_CONFIG div_ant_conf;

    /* Data Collection */
    rx_ant_conf = (rx_stats->rs_rssi_ctl2 >> 4) & 0x3;
    main_ant_conf= (rx_stats->rs_rssi_ctl2 >> 2) & 0x3;

    /* Only Record packet with main_rssi/alt_rssi is positive */
    if ((main_rssi > 0) && (alt_rssi > 0)) {
        antcomb->total_pkt_count++;
        antcomb->main_total_rssi += main_rssi;
        antcomb->alt_total_rssi  += alt_rssi;
        if (main_ant_conf == rx_ant_conf) {          
           antcomb->main_recv_cnt++;
        }
        else {
           antcomb->alt_recv_cnt++;
        }
    } 

    /* Short scan check */
    /* If alt is good when count=100, and alt is bad when scan=1, 
     * reduce pkt number to 128 to reduce dip
     * Don't care aggregation
     */
    short_scan = 0;
    if (antcomb->scan && antcomb->alt_good) {
        comb_delta_time = (CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP()) - 
                           antcomb->scan_start_time);
        if (comb_delta_time > ANT_DIV_COMB_SHORT_SCAN_INTR)
            short_scan = 1;
        else{
            if (antcomb->total_pkt_count == ANT_DIV_COMB_SHORT_SCAN_PKTCOUNT)  
            {
               if (antcomb->total_pkt_count == 0) {
                  alt_ratio = 0;
               } else {
                  alt_ratio = ((antcomb->alt_recv_cnt * 100) / 
                                antcomb->total_pkt_count);
               }          
               if (alt_ratio < ANT_DIV_COMB_ALT_ANT_RATIO)
                   short_scan = 1;
            }
        }
    }
 
    /* Diversity and Combining algorithm */
    if (((antcomb->total_pkt_count < ANT_DIV_COMB_MAX_PKTCOUNT) ||
         (rx_pkt_moreaggr != 0)) &&
         (short_scan == 0)) {
        return;
    }

    /* When collect more than 512 packets, and finish aggregation packet
     * Or short scan=1.
     * Start to do decision
     */
    if (antcomb->total_pkt_count == 0) {
        alt_ratio = 0;
        main_rssi_avg = 0;
        alt_rssi_avg = 0;
    } else {
        alt_ratio = ((antcomb->alt_recv_cnt * 100) / antcomb->total_pkt_count);
        main_rssi_avg = (antcomb->main_total_rssi / antcomb->total_pkt_count);
        alt_rssi_avg  = (antcomb->alt_total_rssi / antcomb->total_pkt_count);
    }

 
    ath_hal_getAntDivCombConf(ah, &div_ant_conf);
    curr_alt_set = div_ant_conf.alt_lna_conf; 
    curr_main_set = div_ant_conf.main_lna_conf; 
    curr_bias     = div_ant_conf.fast_div_bias;

    antcomb->count++;

    if (antcomb->count == ANT_DIV_COMB_MAX_COUNT) {
        if (alt_ratio > ANT_DIV_COMB_ALT_ANT_RATIO) {
            antcomb->alt_good = 1;
            antcomb->quick_scan_cnt = 0;
                
            if (curr_main_set == HAL_ANT_DIV_COMB_LNA2) {
                   antcomb->rssi_lna2 = main_rssi_avg;
            } else if (curr_main_set == HAL_ANT_DIV_COMB_LNA1) {
                   antcomb->rssi_lna1 = main_rssi_avg;
            }      
            switch ((curr_main_set << 4) | curr_alt_set) {
                case (0x10): //LNA2 A-B
                    antcomb->main_conf = HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2;
                    antcomb->first_quick_scan_conf = HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2;
                    antcomb->second_quick_scan_conf = HAL_ANT_DIV_COMB_LNA1;
                    break;
                case (0x20):
                    antcomb->main_conf = HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2;
                    antcomb->first_quick_scan_conf = HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2;
                    antcomb->second_quick_scan_conf = HAL_ANT_DIV_COMB_LNA2;
                    break;
                case (0x21): //LNA1 LNA2
                    antcomb->main_conf = HAL_ANT_DIV_COMB_LNA2;
                    antcomb->first_quick_scan_conf = HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2;
                    antcomb->second_quick_scan_conf = HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2;
                    break;
                case (0x12): //LNA2 LNA1
                    antcomb->main_conf = HAL_ANT_DIV_COMB_LNA1;
                    antcomb->first_quick_scan_conf = HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2;
                    antcomb->second_quick_scan_conf = HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2;                        
                    break;     
                case (0x13): //LNA2 A+B
                    antcomb->main_conf = HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2;
                    antcomb->first_quick_scan_conf = HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2;
                    antcomb->second_quick_scan_conf = HAL_ANT_DIV_COMB_LNA1;                            
                    break;
                case (0x23): //LNA1 A+B
                    antcomb->main_conf = HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2;
                    antcomb->first_quick_scan_conf = HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2;
                    antcomb->second_quick_scan_conf = HAL_ANT_DIV_COMB_LNA2;                         
                    break;
                default:
                    break;
            }
        }    
        else {
            antcomb->alt_good=0;
        }
        antcomb->count = 0;
        antcomb->scan = 1;
        antcomb->scan_not_start = 1;        
    }   

    if (ath_ant_div_comb_alt_check(div_ant_conf.antdiv_configgroup, alt_ratio, curr_main_set, curr_alt_set, alt_rssi_avg, main_rssi_avg) && 
        (antcomb->scan == 0)) {
        if (curr_alt_set==HAL_ANT_DIV_COMB_LNA2) {
            // Switch main and alt LNA
            div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA2;
            div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1;
            end_st = 24;
        } 
        else if (curr_alt_set==HAL_ANT_DIV_COMB_LNA1) {       
            div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA1;
            div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA2;
            end_st = 25;
        }
        else {
            end_st = 1;
        }
        goto div_comb_done;
    }

    if ((curr_alt_set != HAL_ANT_DIV_COMB_LNA1) && 
        (curr_alt_set != HAL_ANT_DIV_COMB_LNA2) && 
        (antcomb->scan != 1)) {
        /* Set alt to another LNA*/  
        if (curr_main_set==HAL_ANT_DIV_COMB_LNA2) { //main LNA2, alt LNA1
            div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA2;
            div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1;   
            end_st = 22;        
        }else if(curr_main_set==HAL_ANT_DIV_COMB_LNA1) { //main LNA1, alt LNA2
            div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA1;
            div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA2;   
            end_st = 23;        
        }
        else {
            end_st = 2;
        }
        goto div_comb_done;
    }

    if ((alt_rssi_avg < (main_rssi_avg + div_ant_conf.lna1_lna2_delta)) && 
        (antcomb->scan != 1)) {
        end_st = 3;
        goto div_comb_done;
    }
    if (antcomb->scan_not_start == 0) {
        switch (curr_alt_set) {
            case HAL_ANT_DIV_COMB_LNA2:
                antcomb->rssi_lna2 = alt_rssi_avg;
                antcomb->rssi_lna1 = main_rssi_avg;           
                antcomb->scan = 1;
                /* set to A+B */
                div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA1;
                div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2;               
                end_st = 4;
                break;
            case HAL_ANT_DIV_COMB_LNA1:
                antcomb->rssi_lna1 = alt_rssi_avg;
                antcomb->rssi_lna2 = main_rssi_avg;
                antcomb->scan = 1;
                /* set to A+B */
                div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA2;
                div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2;
                end_st = 4;
                break;
            case HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2:
                antcomb->rssi_add = alt_rssi_avg;
                antcomb->scan = 1;
                /* set to A-B */
                div_ant_conf.main_lna_conf = curr_main_set;
                div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2;
                end_st = 5;
                break;
            case HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2:
                antcomb->rssi_sub = alt_rssi_avg;
                antcomb->scan = 0;
                if (antcomb->rssi_lna2 > 
                    (antcomb->rssi_lna1 + ANT_DIV_COMB_LNA1_LNA2_SWITCH_DELTA)) { 
                    //use LNA2 as main LNA
                    if ((antcomb->rssi_add > antcomb->rssi_lna1) && 
                        (antcomb->rssi_add > antcomb->rssi_sub)) {
                        /* set to A+B */
                        div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA2;
                        div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2; 
                        end_st = 7;
                    }
                    else if (antcomb->rssi_sub > antcomb->rssi_lna1) {
                        /* set to A-B */
                        div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA2;
                        div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2;
                        end_st = 6;
                    }
                    else {
                        /* set to LNA1 */
                        div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA2;
                        div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1;
                        end_st = 8;
                    }
                }
                else { //use LNA1 as main LNA
                    if ((antcomb->rssi_add > antcomb->rssi_lna2) && 
                        (antcomb->rssi_add > antcomb->rssi_sub)) {
                        /* set to A+B */
                        div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA1;
                        div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2; 
                        end_st = 27;
                    }
                    else if (antcomb->rssi_sub > antcomb->rssi_lna1) {
                        /* set to A-B */
                        div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA1;
                        div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2;
                        end_st = 26;
                    }
                    else {
                        /* set to LNA2 */
                        div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA1;
                        div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA2;
                        end_st = 28;
                    }                        
                }
                break;
            default:
                break;
        } /* End Switch*/
    } /* End scan_not_start = 0 */
    else {
        if (antcomb->alt_good != 1) {
            antcomb->scan_not_start= 0;
            /* Set alt to another LNA */
            if (curr_main_set==HAL_ANT_DIV_COMB_LNA2) {
                div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA2;
                div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1;                       
            }else if (curr_main_set==HAL_ANT_DIV_COMB_LNA1) {
                div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA1;
                div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA2;                     
            }
            end_st = 9;    
            goto div_comb_done;
        }
        /* alt_good == 1*/
        switch (antcomb->quick_scan_cnt) {
        case 0:
            /* set alt to main, and alt to first conf */
            end_st = 10;    
            /* Set configuration */
            div_ant_conf.main_lna_conf = antcomb->main_conf;
            div_ant_conf.alt_lna_conf = antcomb->first_quick_scan_conf;
            break;
        case 1:
            /* set alt to main, and alt to first conf */
            end_st = 11;    
            /* Set configuration */
            div_ant_conf.main_lna_conf = antcomb->main_conf;
            div_ant_conf.alt_lna_conf = antcomb->second_quick_scan_conf;
            antcomb->rssi_first = main_rssi_avg;
            antcomb->rssi_second = alt_rssi_avg;  
            /* if alt have some good and also RSSI is larger than 
             * (main+ANT_DIV_COMB_LNA1_DELTA_HI), choose alt. 
             * But be careful when RSSI is high (diversity is off), 
             * we won't have any packets.
             * So add a threshold of 50 packets                          
             * Or alt already have larger RSSI
             */
            if (antcomb->main_conf == HAL_ANT_DIV_COMB_LNA1) { //main is LNA1
                if ((((alt_ratio >= ANT_DIV_COMB_ALT_ANT_RATIO2) && 
                    (alt_rssi_avg>main_rssi_avg + ANT_DIV_COMB_LNA1_DELTA_HI)) || 
                    (alt_rssi_avg>main_rssi_avg + ANT_DIV_COMB_LNA1_DELTA_LOW)) && 
                    (antcomb->total_pkt_count > 50)) {
                    antcomb->first_ratio = 1;
                }
                else {
                    antcomb->first_ratio = 0;
                }                        
            }
            else if (antcomb->main_conf == HAL_ANT_DIV_COMB_LNA2) { // main is LNA2
                if ((((alt_ratio >= ANT_DIV_COMB_ALT_ANT_RATIO2) && 
                    (alt_rssi_avg>main_rssi_avg + ANT_DIV_COMB_LNA1_DELTA_MID)) || 
                    (alt_rssi_avg>main_rssi_avg + ANT_DIV_COMB_LNA1_DELTA_LOW)) && 
                     (antcomb->total_pkt_count > 50)) {
                     antcomb->first_ratio = 1;
                }
                else {
                     antcomb->first_ratio = 0;
                }                        
            }                  
            else {
                if ((((alt_ratio >= ANT_DIV_COMB_ALT_ANT_RATIO2) && 
                    (alt_rssi_avg>main_rssi_avg + ANT_DIV_COMB_LNA1_DELTA_HI)) || 
                    (alt_rssi_avg > main_rssi_avg)) && 
                    (antcomb->total_pkt_count > 50)) {
                    antcomb->first_ratio = 1;
                }
                else {
                    antcomb->first_ratio = 0;
                }                        
            }
            break;
        case 2:
            antcomb->alt_good=0;
            antcomb->scan_not_start=0;
            antcomb->scan = 0;
            antcomb->rssi_first = main_rssi_avg;
            antcomb->rssi_third = alt_rssi_avg;  
            /* if alt have some good and also RSSI is larger than 
             * (main + ANT_DIV_COMB_LNA1_DELTA_HI), choose alt. 
             * But be careful when RSSI is high (diversity is off), 
             * we won't have any packets.
             * So add a threshould of 50 packets                          
             * Or alt already have larger RSSI
             */
             
            if (antcomb->second_quick_scan_conf == HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2)
            {
               if (antcomb->main_conf == HAL_ANT_DIV_COMB_LNA2)
               {
                  antcomb->rssi_lna2 = main_rssi_avg;
               }
               else if (antcomb->main_conf == HAL_ANT_DIV_COMB_LNA1)
               {
                  antcomb->rssi_lna1 = main_rssi_avg;
               }
            } else if (antcomb->second_quick_scan_conf == HAL_ANT_DIV_COMB_LNA2) {
                  antcomb->rssi_lna2 = alt_rssi_avg;
            } else if (antcomb->second_quick_scan_conf == HAL_ANT_DIV_COMB_LNA1) {
                  antcomb->rssi_lna1 = alt_rssi_avg;
            }
            
            //select which is main LNA
            if (antcomb->rssi_lna2 > antcomb->rssi_lna1 + ANT_DIV_COMB_LNA1_LNA2_SWITCH_DELTA ) {
                div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA2;
            }else {
                div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA1;
            }             

            if (antcomb->main_conf == HAL_ANT_DIV_COMB_LNA1) { // main is LNA1
                if ((((alt_ratio >= ANT_DIV_COMB_ALT_ANT_RATIO2) && 
                     (alt_rssi_avg>main_rssi_avg + ANT_DIV_COMB_LNA1_DELTA_HI)) || 
                     (alt_rssi_avg>main_rssi_avg + ANT_DIV_COMB_LNA1_DELTA_LOW)) &&
                    (antcomb->total_pkt_count > 50)) {
                    antcomb->second_ratio=1;
                } 
                else {
                    antcomb->second_ratio=0;
                }                        
            }
            else if (antcomb->main_conf == HAL_ANT_DIV_COMB_LNA2) { // main is LNA2
                if ((((alt_ratio >= ANT_DIV_COMB_ALT_ANT_RATIO2) && 
                     (alt_rssi_avg>main_rssi_avg + ANT_DIV_COMB_LNA1_DELTA_MID)) || 
                     (alt_rssi_avg>main_rssi_avg + ANT_DIV_COMB_LNA1_DELTA_LOW)) &&
                    (antcomb->total_pkt_count > 50)) {
                    antcomb->second_ratio=1;
                } 
                else {
                    antcomb->second_ratio=0;
                }                        
            }
            else {
                if ((((alt_ratio >= ANT_DIV_COMB_ALT_ANT_RATIO2) && 
                    (alt_rssi_avg > main_rssi_avg + ANT_DIV_COMB_LNA1_DELTA_HI)) ||
                    (alt_rssi_avg>main_rssi_avg)) &&
                    (antcomb->total_pkt_count > 50)) {
                    antcomb->second_ratio = 1;
                } 
                else {
                    antcomb->second_ratio = 0;
                }                        
            }

            /* judgement, set alt to the conf with maximun ratio */
            if (antcomb->first_ratio) {
                if (antcomb->second_ratio) {
                    /* compare RSSI of alt*/
                    if (antcomb->rssi_second > antcomb->rssi_third){
                        /*first alt*/
                        if ((antcomb->first_quick_scan_conf == HAL_ANT_DIV_COMB_LNA1) ||
                            (antcomb->first_quick_scan_conf == HAL_ANT_DIV_COMB_LNA2)) {
                            /* Set alt LNA1 or LNA2*/
                            if (div_ant_conf.main_lna_conf == HAL_ANT_DIV_COMB_LNA2) {
                                div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA1;
                            } else {
                                div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA2;
                            }
                            end_st = 12;                          
                        }
                        else {
                            /* Set alt to A+B or A-B */
                            div_ant_conf.alt_lna_conf = antcomb->first_quick_scan_conf;
   
                            end_st = 13;                             
                        }                               
                    }
                    else {
                        /*second alt*/
                        if ((antcomb->second_quick_scan_conf == HAL_ANT_DIV_COMB_LNA1) ||
                            (antcomb->second_quick_scan_conf == HAL_ANT_DIV_COMB_LNA2)) {
                            /* Set alt LNA1 or LNA2*/
                            if (div_ant_conf.main_lna_conf == HAL_ANT_DIV_COMB_LNA2) {
                                div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA1;
                            } else {
                                div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA2;
                            }
                            end_st = 14;                          
                        } 
                        else {
                            /* Set alt to A+B or A-B */
                            div_ant_conf.alt_lna_conf = antcomb->second_quick_scan_conf;   
                            end_st = 15;                             
                        }                            
                    }
                }
                else {
                    /* first alt*/
                    if ((antcomb->first_quick_scan_conf == HAL_ANT_DIV_COMB_LNA1) ||
                        (antcomb->first_quick_scan_conf == HAL_ANT_DIV_COMB_LNA2)) {
                        /* Set alt LNA1 or LNA2*/
                            if (div_ant_conf.main_lna_conf == HAL_ANT_DIV_COMB_LNA2) {
                                div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA1;
                            } else {
                                div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA2;
                            }   
                        end_st = 16;                          
                    }
                    else {
                        /* Set alt to A+B or A-B */
                        div_ant_conf.alt_lna_conf = antcomb->first_quick_scan_conf;    
                        end_st = 17;                             
                    }                            
                }
            }
            else {
                if (antcomb->second_ratio) {
                    /* second alt*/
                    if ((antcomb->second_quick_scan_conf == HAL_ANT_DIV_COMB_LNA1) ||
                        (antcomb->second_quick_scan_conf == HAL_ANT_DIV_COMB_LNA2)) {
                        /* Set alt LNA1 or LNA2*/
                        if (div_ant_conf.main_lna_conf == HAL_ANT_DIV_COMB_LNA2) {
                            div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA1;
                        } else {
                            div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA2;
                        }   
                        end_st = 18;                          
                    }
                    else {
                        /* Set alt to A+B or A-B */ 
                        div_ant_conf.alt_lna_conf = antcomb->second_quick_scan_conf;      
                        end_st = 19;                             
                    }                           
                } 
                else {
                    /* main is largest*/
                    if ((antcomb->main_conf == HAL_ANT_DIV_COMB_LNA1) || 
                        (antcomb->main_conf == HAL_ANT_DIV_COMB_LNA2)) {
                        /* Set alt LNA1 or LNA2*/
                        if (div_ant_conf.main_lna_conf == HAL_ANT_DIV_COMB_LNA2) {
                            div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA1;
                        } else {
                            div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA2;
                        }  
                        end_st = 20;

                    } 
                    else {
                        /* Set alt to A+B or A-B */ 
                        div_ant_conf.alt_lna_conf = antcomb->main_conf;     
                        end_st = 21;                             
                    } 
                }                        
            }
            break;
        default:
            break;
        } /* end of quick_scan_cnt switch */
        antcomb->quick_scan_cnt++; 
    } /* End scan_not_start != 0*/ 

div_comb_done:
    /* Adjust the fast_div_bias based on main and alt lna conf */
    ath_ant_adjust_fast_div_bias_tp(antcomb, alt_ratio, div_ant_conf.antdiv_configgroup, &div_ant_conf);

    /* Write new setting to register*/
    ath_hal_setAntDivCombConf(ah, &div_ant_conf); 

    /* get system time*/
    antcomb->scan_start_time = CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());

#if 0
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB current time is 0x%x \n", antcomb->scan_start_time);
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB total pkt num   %3d \n", antcomb->total_pkt_count);
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB count num       %3d \n", antcomb->count);
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB total main num  %3d \n", antcomb->main_recv_cnt);
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB total alt  num  %3d \n", antcomb->alt_recv_cnt);
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB Ratio           %3d \n", alt_ratio);
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB RSSI main avg   %3d \n", main_rssi_avg);
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB RSSI alt  avg   %3d \n", alt_rssi_avg);
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB current alt setting is %d, END %d div_scan=%d scan_not_start=%d\n", 
        curr_alt_set, end_st, antcomb->scan,antcomb->scan_not_start);
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB Current setting main=%d alt=%d bias =0x%x \n",curr_main_set,curr_alt_set,curr_bias);
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB next_main=%d next_alt=%d next_bias =0x%x \n",
        div_ant_conf.main_lna_conf,div_ant_conf.alt_lna_conf,div_ant_conf.fast_div_bias);
#endif

    /* Clear variable */
    antcomb->total_pkt_count = 0;
    antcomb->main_total_rssi = 0;
    antcomb->alt_total_rssi = 0;
    antcomb->main_recv_cnt = 0;
    antcomb->alt_recv_cnt = 0;
}

#endif    /* ATH_ANT_DIV_COMB */