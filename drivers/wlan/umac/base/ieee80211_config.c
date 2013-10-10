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

#include <ieee80211_api.h>
#include <ieee80211_var.h>
#include <ieee80211_config.h>
#include <ieee80211_rateset.h>
#include <ieee80211_channel.h>
#include <ieee80211_resmgr.h>
#include <ieee80211_target.h>

int
wlan_set_param(wlan_if_t vaphandle, ieee80211_param param, u_int32_t val)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node_table *nt = &ic->ic_sta;
    int retv = 0;

    switch (param) {
    case IEEE80211_AUTO_ASSOC:
       if (val)
            IEEE80211_VAP_AUTOASSOC_ENABLE(vap);
        else
            IEEE80211_VAP_AUTOASSOC_DISABLE(vap);
        break;

    case IEEE80211_SAFE_MODE:
        if (val)
            IEEE80211_VAP_SAFEMODE_ENABLE(vap);
        else
            IEEE80211_VAP_SAFEMODE_DISABLE(vap);
        break;

    case IEEE80211_SEND_80211:
        if (val)
            IEEE80211_VAP_SEND_80211_ENABLE(vap);
        else
            IEEE80211_VAP_SEND_80211_DISABLE(vap);
        break;

    case IEEE80211_RECEIVE_80211:
        if (val)
            IEEE80211_VAP_DELIVER_80211_ENABLE(vap);
        else
            IEEE80211_VAP_DELIVER_80211_DISABLE(vap);
        break;

    case IEEE80211_FEATURE_DROP_UNENC:
        if (val)
            IEEE80211_VAP_DROP_UNENC_ENABLE(vap);
        else
            IEEE80211_VAP_DROP_UNENC_DISABLE(vap);
        break;

#ifdef ATH_COALESCING
    case IEEE80211_FEATURE_TX_COALESCING:
        ic->ic_tx_coalescing = val;
        break;
#endif
    case IEEE80211_SHORT_PREAMBLE:
        if (val)
           IEEE80211_ENABLE_CAP_SHPREAMBLE(ic);
        else
           IEEE80211_DISABLE_CAP_SHPREAMBLE(ic);
         retv = EOK;
        break;    
    
    case IEEE80211_SHORT_SLOT:
        if (val)
            ieee80211_set_shortslottime(ic, 1);
        else
            ieee80211_set_shortslottime(ic, 0);
        break;

    case IEEE80211_RTS_THRESHOLD:
        /* XXX This may force us to flush any packets for which we are 
           might have already calculated the RTS */
        if (val > IEEE80211_RTS_MAX)
            vap->iv_rtsthreshold = IEEE80211_RTS_MAX;
        else
            vap->iv_rtsthreshold = (u_int16_t)val;
        break;

    case IEEE80211_FRAG_THRESHOLD:
        /* XXX We probably should flush our tx path when changing fragthresh */
        if (val > 2346)
            vap->iv_fragthreshold = 2346;
        else if (val < 256)
            vap->iv_fragthreshold = 256;
        else
            vap->iv_fragthreshold = (u_int16_t)val;
        break;

    case IEEE80211_BEACON_INTVAL:
        ic->ic_intval = (u_int16_t)val;
        LIMIT_BEACON_PERIOD(ic->ic_intval);
        break;

#if ATH_SUPPORT_AP_WDS_COMBO
    case IEEE80211_NO_BEACON:
        vap->iv_no_beacon = (u_int8_t) val;
        ic->ic_set_config(vap);
        break;
#endif
    case IEEE80211_LISTEN_INTVAL:
        ic->ic_lintval = 1;
        vap->iv_bss->ni_lintval = ic->ic_lintval;
        break;

    case IEEE80211_ATIM_WINDOW:
        LIMIT_BEACON_PERIOD(val);
        vap->iv_atim_window = (u_int8_t)val;
        break;

    case IEEE80211_DTIM_INTVAL:
        LIMIT_DTIM_PERIOD(val);
        vap->iv_dtim_period = (u_int8_t)val;
        break;

    case IEEE80211_BMISS_COUNT_RESET:
        vap->iv_bmiss_count_for_reset = (u_int8_t)val;
        break;

    case IEEE80211_BMISS_COUNT_MAX:
        vap->iv_bmiss_count_max = (u_int8_t)val;
        break;

    case IEEE80211_TXPOWER:
        break;

    case IEEE80211_MULTI_DOMAIN:
        if(!ic->ic_country.isMultidomain)
            return -EINVAL;
        
        if (val)
            ic->ic_multiDomainEnabled = 1;
        else
            ic->ic_multiDomainEnabled = 0;
        break;

    case IEEE80211_MSG_FLAGS:
        /*
         * ic_debug will be overwritten every time the VAP debug level is changed.
         */
        ic->ic_debug = val;
        vap->iv_debug = ic->ic_debug;
        break;

    case IEEE80211_FEATURE_WMM:
        if (!(ic->ic_caps & IEEE80211_C_WME))
            return -EINVAL;
        
        if (val)
            ieee80211_vap_wme_set(vap);
        else
            ieee80211_vap_wme_clear(vap);
        break;

    case IEEE80211_FEATURE_PRIVACY:
        if (val)
            IEEE80211_VAP_PRIVACY_ENABLE(vap);
        else
            IEEE80211_VAP_PRIVACY_DISABLE(vap);
        break;

    case IEEE80211_FEATURE_WMM_PWRSAVE:
        /*
         * NB: AP WMM power save is a compilation option,
         * and can not be turned on/off at run time.
         */
        if (vap->iv_opmode != IEEE80211_M_STA)
            return -EINVAL;

        if (val)
            ieee80211_set_wmm_power_save(vap, 1);
        else
            ieee80211_set_wmm_power_save(vap, 0);
        break;

    case IEEE80211_FEATURE_UAPSD:
        if (vap->iv_opmode == IEEE80211_M_STA) {
            ieee80211_set_uapsd_flags(vap, (u_int8_t)(val & WME_CAPINFO_UAPSD_ALL));
            return ieee80211_pwrsave_uapsd_set_max_sp_length(vap, ((val & WME_CAPINFO_UAPSD_MAXSP_MASK)) >> WME_CAPINFO_UAPSD_MAXSP_SHIFT );
        }
        else {
            if (IEEE80211_IS_UAPSD_ENABLED(ic)) {
                if (val)
                    IEEE80211_VAP_UAPSD_ENABLE(vap);
                else
                    IEEE80211_VAP_UAPSD_DISABLE(vap);
            }
            else {
                return -EINVAL;
            }
        }
        break;

    case IEEE80211_WPS_MODE:
        vap->iv_wps_mode = (u_int8_t)val;
        break;

    case IEEE80211_NOBRIDGE_MODE:
        if (val)
            IEEE80211_VAP_NOBRIDGE_ENABLE(vap);
        else
            IEEE80211_VAP_NOBRIDGE_DISABLE(vap);
        break;

    case IEEE80211_MIN_BEACON_COUNT:
    case IEEE80211_IDLE_TIME:
        ieee80211_vap_pause_set_param(vaphandle,param,val);
        break;
    case IEEE80211_FEATURE_COUNTER_MEASURES:
        if (val)
            IEEE80211_VAP_COUNTERM_ENABLE(vap);
        else
            IEEE80211_VAP_COUNTERM_DISABLE(vap);
        break;
    case IEEE80211_FEATURE_WDS:
        if (val)
            IEEE80211_VAP_WDS_ENABLE(vap);
        else
            IEEE80211_VAP_WDS_DISABLE(vap);
        
        ieee80211_update_vap_target(vap);
        break;
    case IEEE80211_FEATURE_VAP_IND:
        if (val)
            ieee80211_vap_vap_ind_set(vap);
        else
            ieee80211_vap_vap_ind_clear(vap);
        break;
    case IEEE80211_FEATURE_HIDE_SSID:
        if (val){
            vap->iv_wps_mode = 0;
            IEEE80211_VAP_APPIE_UPDATE_DISABLE(vap);
            /* indicates that we have used this ie. */
            printk("disabling hidden ssid \n");
            /* wps should be disable if hidden ssid is set */
            IEEE80211_VAP_HIDESSID_ENABLE(vap);
        }
        else
            IEEE80211_VAP_HIDESSID_DISABLE(vap);
        break;
    case IEEE80211_FEATURE_PUREG:
        if (val)
            IEEE80211_VAP_PUREG_ENABLE(vap);
        else
            IEEE80211_VAP_PUREG_DISABLE(vap);
        break;
    case IEEE80211_FEATURE_PURE11N:
        if (val)
            IEEE80211_VAP_PURE11N_ENABLE(vap);
        else
            IEEE80211_VAP_PURE11N_DISABLE(vap);
        break;
    case IEEE80211_FEATURE_APBRIDGE:
        if (val == 0)
            IEEE80211_VAP_NOBRIDGE_ENABLE(vap);
        else
            IEEE80211_VAP_NOBRIDGE_DISABLE(vap);
        break;
    case IEEE80211_FEATURE_COPY_BEACON:
        if (val == 0)
            ieee80211_vap_copy_beacon_clear(vap);
        else
            ieee80211_vap_copy_beacon_set(vap);
        break;
    case IEEE80211_FIXED_RATE:
        if (val == IEEE80211_FIXED_RATE_NONE) {
             vap->iv_fixed_rate.mode = IEEE80211_FIXED_RATE_NONE;
             vap->iv_fixed_rateset = IEEE80211_FIXED_RATE_NONE;
             vap->iv_fixed_rate.series = IEEE80211_FIXED_RATE_NONE;
        } else {
             if (val & 0x80) {
             vap->iv_fixed_rate.mode   = IEEE80211_FIXED_RATE_MCS;
             } else {
                 vap->iv_fixed_rate.mode   = IEEE80211_FIXED_RATE_LEGACY;
             }
             vap->iv_fixed_rateset = val;
             vap->iv_fixed_rate.series = val;
        }
        ic->ic_set_config(vap);
        break;
    case IEEE80211_FIXED_RETRIES:
        vap->iv_fixed_retryset = val;
        vap->iv_fixed_rate.retries = val;
        ic->ic_set_config(vap);
        break;
    case IEEE80211_MCAST_RATE:
        vap->iv_mcast_fixedrate = val;
        ieee80211_set_mcast_rate(vap);
        break;
    case IEEE80211_SHORT_GI:
        if (val) {
            if (ieee80211com_has_htcap(ic, IEEE80211_HTCAP_C_SHORTGI40 | IEEE80211_HTCAP_C_SHORTGI20)) {
                if (ieee80211com_has_htcap(ic, IEEE80211_HTCAP_C_SHORTGI40))
                    ieee80211com_set_htflags(ic, IEEE80211_HTF_SHORTGI40);
                if (ieee80211com_has_htcap(ic, IEEE80211_HTCAP_C_SHORTGI20))
                    ieee80211com_set_htflags(ic, IEEE80211_HTF_SHORTGI20);
            }
            else
	            return -EINVAL;
        }
        else
            ieee80211com_clear_htflags(ic, IEEE80211_HTF_SHORTGI40 | IEEE80211_HTF_SHORTGI20);

        ic->ic_set_config(vap);
        break;
     case IEEE80211_FEATURE_STAFWD:
     	if (vap->iv_opmode == IEEE80211_M_STA) 
     	{
          if (val == 0)
              ieee80211_vap_sta_fwd_clear(vap);
          else
              ieee80211_vap_sta_fwd_set(vap);
     	}
         else 
         {
             return -EINVAL;
         }
         break;
    case IEEE80211_HT40_INTOLERANT:
        vap->iv_ht40_intolerant = val;
        break;
    case IEEE80211_CHWIDTH:
		vap->iv_chwidth = val;
        break;
    case IEEE80211_CHEXTOFFSET:
		vap->iv_chextoffset = val;
        break;
    case IEEE80211_DISABLE_2040COEXIST:
        if (val) {
            ic->ic_flags |= IEEE80211_F_COEXT_DISABLE;
        }
        else{
            //Resume to the state kept in registry key
            if (ic->ic_reg_parm.disable2040Coexist) {
                ic->ic_flags |= IEEE80211_F_COEXT_DISABLE;
            } else {
                ic->ic_flags &= ~IEEE80211_F_COEXT_DISABLE;
            }
        }
        break;
    case IEEE80211_DISABLE_HTPROTECTION:
        vap->iv_disable_HTProtection = val;
        break;
#ifdef ATH_SUPPORT_QUICK_KICKOUT        
    case IEEE80211_STA_QUICKKICKOUT:
        vap->iv_sko_th = val;        
        break;
#endif        
    case IEEE80211_CHSCANINIT:
        vap->iv_chscaninit = val;
        break;
    case IEEE80211_DRIVER_CAPS:
        vap->iv_caps = val;
        break;
    case IEEE80211_FEATURE_COUNTRY_IE:
        if (val)
            /* Enable the Country IE during tx of beacon and ProbeResp. */
            ieee80211_vap_country_ie_set(vap);
        else
            /* Disable the Country IE during tx of beacon and ProbeResp. */
            ieee80211_vap_country_ie_clear(vap);
        break;
    case IEEE80211_FEATURE_IC_COUNTRY_IE:
        if (val) {
            IEEE80211_ENABLE_COUNTRYIE(ic);
        } else {
            IEEE80211_DISABLE_COUNTRYIE(ic);
        }
        break;
    case IEEE80211_FEATURE_DOTH:
        if (val)
            /* Enable the dot h IE's for this VAP. */
            ieee80211_vap_doth_set(vap);
        else
            /* Disable the dot h IE's for this VAP. */
            ieee80211_vap_doth_clear(vap);
        break;

     case  IEEE80211_FEATURE_PSPOLL:       
         retv= ieee80211_sta_power_set_pspoll(vap->iv_pwrsave_sta, val);
         break;

    case IEEE80211_FEATURE_CONTINUE_PSPOLL_FOR_MOREDATA:
         retv= ieee80211_sta_power_set_pspoll_moredata_handling(vap->iv_pwrsave_sta, 
                                                       val?IEEE80211_CONTINUE_PSPOLL_FOR_MORE_DATA :
                                                       IEEE80211_WAKEUP_FOR_MORE_DATA);
         break;

#if ATH_SUPPORT_IQUE
	case IEEE80211_ME:
        if (vap->iv_me) {
		    vap->iv_me->mc_snoop_enable = val;
        }
		break;
	case IEEE80211_MEDEBUG:
        if (vap->iv_me) {
		    vap->iv_me->me_debug = val;
        }
		break;
	case IEEE80211_ME_SNOOPLENGTH:
        if (vap->iv_me) {
            vap->iv_me->ieee80211_me_snooplist.msl_max_length = 
                    (val > MAX_SNOOP_ENTRIES)? MAX_SNOOP_ENTRIES : val;
        }
		break;
	case IEEE80211_ME_TIMER:
        if (vap->iv_me) {
		    vap->iv_me->me_timer = val;
        }
		break;
	case IEEE80211_ME_TIMEOUT:
        if (vap->iv_me) {
		    vap->iv_me->me_timeout = val;
        }
		break;
	case IEEE80211_ME_DROPMCAST:
        if (vap->iv_me) {
		    vap->iv_me->mc_discard_mcast = val;
        }
		break;
    case  IEEE80211_ME_CLEARDENY:
        if (vap->iv_ique_ops.me_cleardeny) {
            vap->iv_ique_ops.me_cleardeny(vap);
        }    
        break;
    case IEEE80211_HBR_TIMER:
        if (vap->iv_hbr_list && vap->iv_ique_ops.hbr_settimer) {
            vap->iv_ique_ops.hbr_settimer(vap, val);
        } 
        break;   
#endif /*ATH_SUPPORT_IQUE*/
    case IEEE80211_WEP_MBSSID:
        vap->iv_wep_mbssid = (u_int8_t)val;
        break;

    case IEEE80211_MGMT_RATE:
        vap->iv_mgt_rate = (u_int16_t)val;
        break;
    case IEEE80211_FEATURE_AMPDU:
        if (!val) {
           IEEE80211_DISABLE_AMPDU(ic);
        } else {
           IEEE80211_ENABLE_AMPDU(ic);
        }
        break;

    case IEEE80211_MIN_FRAMESIZE:
        ic->ic_minframesize = val;
	break;

#if UMAC_SUPPORT_TDLS
    case IEEE80211_TDLS_MACADDR1:
        vap->iv_tdls_macaddr1 = val;
        break;

    case IEEE80211_TDLS_MACADDR2:
        vap->iv_tdls_macaddr2 = val;
        break;

    case IEEE80211_RESMGR_VAP_AIR_TIME_LIMIT:
        retv = ieee80211_resmgr_off_chan_sched_set_air_time_limit(ic->ic_resmgr, vap, val);
        break;

    case IEEE80211_TDLS_ACTION:
        vap->iv_tdls_action = val;
        break;
#endif
    case IEEE80211_UAPSD_MAXSP:
        retv = ieee80211_pwrsave_uapsd_set_max_sp_length(vap,val);
        break;

    case IEEE80211_AP_REJECT_DFS_CHAN:
        if (val == 0)
            ieee80211_vap_ap_reject_dfs_chan_clear(vap);
        else
            ieee80211_vap_ap_reject_dfs_chan_set(vap);
        break;
	case IEEE80211_CFG_INACT:
		nt->nt_inact_run = val / IEEE80211_INACT_WAIT;
		break;
	case IEEE80211_CFG_INACT_AUTH:
		nt->nt_inact_auth = val / IEEE80211_INACT_WAIT;
		break;
	case IEEE80211_CFG_INACT_INIT:
		nt->nt_inact_init = val / IEEE80211_INACT_WAIT;
		break;
	case IEEE80211_PWRTARGET:
		ieee80211com_set_curchanmaxpwr(ic, val);
		break;
	case IEEE80211_WDS_AUTODETECT:
        if (!val) {
           IEEE80211_VAP_WDS_AUTODETECT_DISABLE(vap);
        } else {
           IEEE80211_VAP_WDS_AUTODETECT_ENABLE(vap);
        }
		break;
	case IEEE80211_WEP_TKIP_HT:
		if (!val) {
           ieee80211_ic_wep_tkip_htrate_clear(ic);
        } else {
           ieee80211_ic_wep_tkip_htrate_set(ic);
        }
		break;
	case IEEE80211_IGNORE_11DBEACON:
        if (!val) {
           IEEE80211_DISABLE_IGNORE_11D_BEACON(ic);
        } else {
           IEEE80211_ENABLE_IGNORE_11D_BEACON(ic);
        }
		break;
#ifdef ATH_SUPPORT_TxBF
    case IEEE80211_TXBF_AUTO_CVUPDATE:
        vap->iv_autocvupdate = val;
        break;
    case IEEE80211_TXBF_CVUPDATE_PER:
        vap->iv_cvupdateper = val;
        break;
#endif

	case IEEE80211_FEATURE_MFP_TEST:
            if (!val) {
                ieee80211_vap_mfp_test_clear(vap);
            } else {
                ieee80211_vap_mfp_test_set(vap);
            }
            break;
    case IEEE80211_WEATHER_RADAR:
            ic->ic_no_weather_radar_chan = !!val;
            break;
    case IEEE80211_WEP_KEYCACHE:
            vap->iv_wep_keycache = !!val;
            break;
#if ATH_SUPPORT_WPA_SUPPLICANT_CHECK_TIME			
	case IEEE80211_REJOINT_ATTEMP_TIME:
			vap->iv_rejoint_attemp_time = val;
			break;
#endif	
    default:
        break;
    }

    return retv;
}

u_int32_t
wlan_get_param(wlan_if_t vaphandle, ieee80211_param param)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node_table *nt = &ic->ic_sta;
    u_int32_t val = 0;
    
    switch (param) {
    case IEEE80211_AUTO_ASSOC:
        val = IEEE80211_VAP_IS_AUTOASSOC_ENABLED(vap) ? 1 : 0;
        break;
    case IEEE80211_SAFE_MODE:
        val = IEEE80211_VAP_IS_SAFEMODE_ENABLED(vap) ? 1 : 0;
        break;
        
    case IEEE80211_SEND_80211:
        val = IEEE80211_VAP_IS_SEND_80211_ENABLED(vap) ? 1 : 0;
        break;

    case IEEE80211_RECEIVE_80211:
        val = IEEE80211_VAP_IS_DELIVER_80211_ENABLED(vap) ? 1 : 0;
        break;

    case IEEE80211_FEATURE_DROP_UNENC:
        val = IEEE80211_VAP_IS_DROP_UNENC(vap) ? 1 : 0;
        break;
    
#ifdef ATH_COALESCING
    case IEEE80211_FEATURE_TX_COALESCING:
        val = ic->ic_tx_coalescing;
        break;
#endif
    case IEEE80211_SHORT_PREAMBLE:
        val = IEEE80211_IS_CAP_SHPREAMBLE_ENABLED(ic) ? 1 : 0;
        break;
    
    case IEEE80211_SHORT_SLOT:
        val = IEEE80211_IS_SHSLOT_ENABLED(ic) ? 1 : 0;
        break;
    
    case IEEE80211_RTS_THRESHOLD:
        val = vap->iv_rtsthreshold;
        break;

    case IEEE80211_FRAG_THRESHOLD:
        val = vap->iv_fragthreshold;
        break;

    case IEEE80211_BEACON_INTVAL:
        if (vap->iv_opmode == IEEE80211_M_STA) {
            val = vap->iv_bss->ni_intval;
        } else {
            val = ic->ic_intval;
        }
        break;

#if ATH_SUPPORT_AP_WDS_COMBO
    case IEEE80211_NO_BEACON:
        val = vap->iv_no_beacon;
        break;
#endif

    case IEEE80211_LISTEN_INTVAL:
        val = ic->ic_lintval;
        break;

    case IEEE80211_DTIM_INTVAL:
        val = vap->iv_dtim_period;
        break;

    case IEEE80211_BMISS_COUNT_RESET:
        val = vap->iv_bmiss_count_for_reset ;
        break;

    case IEEE80211_BMISS_COUNT_MAX:
        val = vap->iv_bmiss_count_max;
        break;

    case IEEE80211_ATIM_WINDOW:
        val = vap->iv_atim_window;
        break;

    case IEEE80211_TXPOWER:
        /* 
         * here we'd better return ni_txpower for it's more accurate to
         * current txpower and it must be less than or equal to
         * ic_txpowlimit/ic_curchanmaxpwr, and it's in 0.5 dbm.
         * This value is updated when channel is changed or setTxPowerLimit
         * is called.
         */
        val = vap->iv_bss->ni_txpower;
        break;

    case IEEE80211_MULTI_DOMAIN:
        val = ic->ic_multiDomainEnabled;
        break;

    case IEEE80211_MSG_FLAGS:
        val = vap->iv_debug;
        break;

    case IEEE80211_FEATURE_WMM:
        val = ieee80211_vap_wme_is_set(vap) ? 1 : 0;
        break;

    case IEEE80211_FEATURE_PRIVACY:
        val =  IEEE80211_VAP_IS_PRIVACY_ENABLED(vap) ? 1 : 0;
        break;

    case IEEE80211_FEATURE_WMM_PWRSAVE:
        val = ieee80211_get_wmm_power_save(vap) ? 1 : 0;
        break;

    case IEEE80211_FEATURE_UAPSD:
        if (vap->iv_opmode == IEEE80211_M_STA) {
            val = ieee80211_get_uapsd_flags(vap);
        }
        else {
            val = IEEE80211_VAP_IS_UAPSD_ENABLED(vap) ? 1 : 0;
        }
        break;
    case IEEE80211_FEATURE_IC_COUNTRY_IE:
	val = (IEEE80211_IS_COUNTRYIE_ENABLED(ic) != 0);
        break;
    case IEEE80211_PERSTA_KEYTABLE_SIZE:
        /*
         * XXX: We should return the number of key tables (each table has 4 key slots),
         * not the actual number of key slots. Use the node hash table size as an estimation
         * of max supported ad-hoc stations.
         */
        val = IEEE80211_NODE_HASHSIZE;
        break;

    case IEEE80211_WPS_MODE:
        val = vap->iv_wps_mode;
        break;

    case IEEE80211_MIN_BEACON_COUNT:
    case IEEE80211_IDLE_TIME:
        val = ieee80211_vap_pause_get_param(vaphandle,param);
        break;
    case IEEE80211_FEATURE_COUNTER_MEASURES:
        val = IEEE80211_VAP_IS_COUNTERM_ENABLED(vap) ? 1 : 0;
        break;
    case IEEE80211_FEATURE_WDS:
        val = IEEE80211_VAP_IS_WDS_ENABLED(vap) ? 1 : 0;
#ifdef ATH_EXT_AP
        if (val == 0) {
	    val = IEEE80211_VAP_IS_EXT_AP_ENABLED(vap) ? 1 : 0; 
        }
#endif
        break;
    case IEEE80211_FEATURE_VAP_IND:
        val = ieee80211_vap_vap_ind_is_set(vap) ? 1 : 0;
        break;
    case IEEE80211_FEATURE_HIDE_SSID:
        val = IEEE80211_VAP_IS_HIDESSID_ENABLED(vap) ? 1 : 0;
        break;
    case IEEE80211_FEATURE_PUREG:
        val = IEEE80211_VAP_IS_PUREG_ENABLED(vap) ? 1 : 0;
        break;
    case IEEE80211_FEATURE_PURE11N:
        val = IEEE80211_VAP_IS_PURE11N_ENABLED(vap) ? 1 : 0;
        break;
    case IEEE80211_FEATURE_APBRIDGE:
        val = IEEE80211_VAP_IS_NOBRIDGE_ENABLED(vap) ? 0 : 1;
        break;
     case  IEEE80211_FEATURE_PSPOLL:       
         val = ieee80211_sta_power_get_pspoll(vap->iv_pwrsave_sta);
         break;

    case IEEE80211_FEATURE_CONTINUE_PSPOLL_FOR_MOREDATA:
         if (ieee80211_sta_power_get_pspoll_moredata_handling(vap->iv_pwrsave_sta) == 
             IEEE80211_CONTINUE_PSPOLL_FOR_MORE_DATA ) {
             val = true;
         } else {
             val = false;
         }
         break;
    case IEEE80211_MCAST_RATE:
        val = vap->iv_mcast_fixedrate;
        break;
    case IEEE80211_HT40_INTOLERANT:
        val = vap->iv_ht40_intolerant;
        break;
    case IEEE80211_CHWIDTH:
		/*
		** Channel width is a radio parameter, not a VAP parameter.  When being configured by the AP
		** through this interface, it's being set in the ic
		*/
		val = ic->ic_cwm_get_width(ic);
        break;
    case IEEE80211_CHEXTOFFSET:
		/*
		** Extension channel is set through the channel mode selected by AP.  When configured
		** through this interface, it's stored in the ic.
		*/
        val = ic->ic_cwm_get_extoffset(ic);
        break;
    case IEEE80211_DISABLE_HTPROTECTION:
        val = vap->iv_disable_HTProtection;
        break;
#ifdef ATH_SUPPORT_QUICK_KICKOUT        
    case IEEE80211_STA_QUICKKICKOUT:
        val = vap->iv_sko_th;
        break;
#endif        
    case IEEE80211_CHSCANINIT:
        val = vap->iv_chscaninit;
        break;
    case IEEE80211_FEATURE_STAFWD:
        val = ieee80211_vap_sta_fwd_is_set(vap) ? 1 : 0;
        break;
    case IEEE80211_DRIVER_CAPS:
        val = vap->iv_caps;
        break;
    case IEEE80211_FEATURE_COUNTRY_IE:
        val = ieee80211_vap_country_ie_is_set(vap) ? 1 : 0;
        break;
    case IEEE80211_FEATURE_DOTH:
        val = ieee80211_vap_doth_is_set(vap) ? 1 : 0;
        break;
#if ATH_SUPPORT_IQUE
    case IEEE80211_IQUE_CONFIG:
        ic->ic_get_iqueconfig(ic);
        break;
	case IEEE80211_ME:
        if (vap->iv_me) {
		    val = vap->iv_me->mc_snoop_enable;
        }
		break;
	case IEEE80211_MEDUMP:
	    val = 0;
        if (vap->iv_ique_ops.me_dump) {
            vap->iv_ique_ops.me_dump(vap);
        }
		break;
	case IEEE80211_MEDEBUG:
        if (vap->iv_me) {
		    val = vap->iv_me->me_debug;
        }
		break;
	case IEEE80211_ME_SNOOPLENGTH:
        if (vap->iv_me) {
    		val = vap->iv_me->ieee80211_me_snooplist.msl_max_length;
        }
		break;
	case IEEE80211_ME_TIMER:
        if (vap->iv_me) {
		    val = vap->iv_me->me_timer;
        }
		break;
	case IEEE80211_ME_TIMEOUT:
        if (vap->iv_me) {
		    val = vap->iv_me->me_timeout;
        }
		break;
	case IEEE80211_ME_DROPMCAST:
        if (vap->iv_me) {
		    val = vap->iv_me->mc_discard_mcast;
        }
		break;
    case IEEE80211_ME_SHOWDENY:
        if (vap->iv_ique_ops.me_showdeny) {
            vap->iv_ique_ops.me_showdeny(vap);
        }    
        break;
#if 0        
    case IEEE80211_HBR_TIMER:
        if (vap->iv_hbr_list) {
            val = vap->iv_hbr_list->hbr_timeout;
        }    
        break;
#endif
#endif        
    case IEEE80211_SHORT_GI:
        val = ieee80211com_has_htflags(ic, IEEE80211_HTF_SHORTGI40 | IEEE80211_HTF_SHORTGI20);
        break;
    case IEEE80211_FIXED_RATE:
        val = vap->iv_fixed_rateset;
        break;
    case IEEE80211_FIXED_RETRIES:
        val = vap->iv_fixed_retryset;
        break;
    case IEEE80211_WEP_MBSSID:
        val = vap->iv_wep_mbssid;
        break;
    case IEEE80211_MGMT_RATE:
        val = vap->iv_mgt_rate;
        break;
        
    case IEEE80211_MIN_FRAMESIZE:
        val = ic->ic_minframesize;
        break;
    case IEEE80211_RESMGR_VAP_AIR_TIME_LIMIT:
        val = ieee80211_resmgr_off_chan_sched_get_air_time_limit(ic->ic_resmgr, vap);
        break;

#if UMAC_SUPPORT_TDLS
    case IEEE80211_TDLS_MACADDR1:
        val = vap->iv_tdls_macaddr1;
        break;

    case IEEE80211_TDLS_MACADDR2:
        val = vap->iv_tdls_macaddr2;
        break;

    case IEEE80211_TDLS_ACTION:
        val = vap->iv_tdls_action;
        break;
#endif
	case IEEE80211_CFG_INACT:
		val = nt->nt_inact_run * IEEE80211_INACT_WAIT;
		break;
	case IEEE80211_CFG_INACT_AUTH:
		val = nt->nt_inact_auth * IEEE80211_INACT_WAIT;
		break;
	case IEEE80211_CFG_INACT_INIT:
		val = nt->nt_inact_init * IEEE80211_INACT_WAIT;
		break;
	case IEEE80211_PWRTARGET:
		val = ic->ic_curchanmaxpwr;
		break;
	case IEEE80211_ABOLT:
        /*
        * Map capability bits to abolt settings.
        */
        val = 0;
        if (vap->iv_ath_cap & IEEE80211_ATHC_COMP)
            val |= IEEE80211_ABOLT_COMPRESSION;
        if (vap->iv_ath_cap & IEEE80211_ATHC_FF)
            val |= IEEE80211_ABOLT_FAST_FRAME;
        if (vap->iv_ath_cap & IEEE80211_ATHC_XR)
            val |= IEEE80211_ABOLT_XR;
        if (vap->iv_ath_cap & IEEE80211_ATHC_BURST)
            val |= IEEE80211_ABOLT_BURST;
        if (vap->iv_ath_cap & IEEE80211_ATHC_TURBOP)
            val |= IEEE80211_ABOLT_TURBO_PRIME;
        if (vap->iv_ath_cap & IEEE80211_ATHC_AR)
            val |= IEEE80211_ABOLT_AR;
        if (vap->iv_ath_cap & IEEE80211_ATHC_WME)
            val |= IEEE80211_ABOLT_WME_ELE;
		break;
    case IEEE80211_COMP:
        val = (vap->iv_ath_cap & IEEE80211_ATHC_COMP) != 0;
        break;
    case IEEE80211_FF:
        val = (vap->iv_ath_cap & IEEE80211_ATHC_FF) != 0;
        break;
    case IEEE80211_TURBO:
        val = (vap->iv_ath_cap & IEEE80211_ATHC_TURBOP) != 0;
        break;
    case IEEE80211_BURST:
        val = (vap->iv_ath_cap & IEEE80211_ATHC_BURST) != 0;
        break;
    case IEEE80211_AR:
        val = (vap->iv_ath_cap & IEEE80211_ATHC_AR) != 0;
        break;
	case IEEE80211_SLEEP:
		val = vap->iv_bss->ni_flags & IEEE80211_NODE_PWR_MGT;
		break;
	case IEEE80211_EOSPDROP:
		val = IEEE80211_VAP_IS_EOSPDROP_ENABLED(vap) != 0;
		break;
	case IEEE80211_MARKDFS:
        val = (ic->ic_flags_ext & IEEE80211_FEXT_MARKDFS) != 0;
		break;
	case IEEE80211_WDS_AUTODETECT:
        val = IEEE80211_VAP_IS_WDS_AUTODETECT_ENABLED(vap) != 0;
		break;		
	case IEEE80211_WEP_TKIP_HT:
		val = ieee80211_ic_wep_tkip_htrate_is_set(ic);
		break;
	case IEEE80211_ATH_RADIO:
        /*
        ** Extract the radio name from the ATH device object
        */
        //printk("IC Name: %s\n",ic->ic_osdev->name);
        //val = ic->ic_dev->name[4] - 0x30;
		break;
	case IEEE80211_IGNORE_11DBEACON:
		val = IEEE80211_IS_11D_BEACON_IGNORED(ic) != 0;
		break;
        case IEEE80211_FEATURE_MFP_TEST:
            val = ieee80211_vap_mfp_test_is_set(vap) ? 1 : 0;
            break;
#ifdef ATH_SUPPORT_TxBF
    case IEEE80211_TXBF_AUTO_CVUPDATE:
        val = vap->iv_autocvupdate;
        break;
    case IEEE80211_TXBF_CVUPDATE_PER:
        val = vap->iv_cvupdateper;
        break;
#endif
    case IEEE80211_WEATHER_RADAR:
        val = ic->ic_no_weather_radar_chan;
        break;
    case IEEE80211_WEP_KEYCACHE:
        val = vap->iv_wep_keycache;
        break;
    default:
        break;
    }

    return val;
}

int wlan_get_chanlist(wlan_if_t vaphandle, u_int8_t *chanlist)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;

    memcpy(chanlist, ic->ic_chan_active, sizeof(ic->ic_chan_active));
    return 0;
}


int wlan_get_chaninfo(wlan_if_t vaphandle, struct ieee80211_channel *chan, int *nchan)
{
#define IS_NEW_CHANNEL(c)		\
	((IEEE80211_IS_CHAN_5GHZ((c)) && isclr(reported_a, (c)->ic_ieee)) || \
	 (IEEE80211_IS_CHAN_2GHZ((c)) && isclr(reported_bg, (c)->ic_ieee)))

    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    u_int8_t reported_a[IEEE80211_CHAN_BYTES];	/* XXX stack usage? */
    u_int8_t reported_bg[IEEE80211_CHAN_BYTES];	/* XXX stack usage? */
    int i, j;

    memset(chan, 0, sizeof(struct ieee80211_channel)*IEEE80211_CHAN_MAX);
    *nchan = 0;
    memset(reported_a, 0, sizeof(reported_a));
    memset(reported_bg, 0, sizeof(reported_bg));
    for (i = 0; i < ic->ic_nchans; i++)
    {
        const struct ieee80211_channel *c = &ic->ic_channels[i];

        if (IS_NEW_CHANNEL(c) || IEEE80211_IS_CHAN_HALF(c) ||
            IEEE80211_IS_CHAN_QUARTER(c))
        {
            if (IEEE80211_IS_CHAN_5GHZ(c))
                setbit(reported_a, c->ic_ieee);
            else
                setbit(reported_bg, c->ic_ieee);

            chan[*nchan].ic_ieee = c->ic_ieee;
            chan[*nchan].ic_freq = c->ic_freq;
            chan[*nchan].ic_flags = c->ic_flags;
            chan[*nchan].ic_regClassId = c->ic_regClassId;
            /*
             * Copy HAL extention channel flags to IEEE channel.This is needed 
             * by the wlanconfig to report the DFS channels. 
             */
            chan[*nchan].ic_flagext = c->ic_flagext;  
            if (++(*nchan) >= IEEE80211_CHAN_MAX)
                break;
        }
        else if (!IS_NEW_CHANNEL(c)){
            for (j = 0; j < *nchan; j++){
                if (chan[j].ic_freq == c->ic_freq){
                    chan[j].ic_flags |= c->ic_flags;
                    continue;
                }
            }
        }
    }
    return 0;
#undef IS_NEW_CHANNEL
}

int wlan_get_txpow(wlan_if_t vaphandle, int *txpow, int *fixed)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;

    *txpow = vap->iv_bss->ni_txpower/2;
    *fixed = (ic->ic_flags & IEEE80211_F_TXPOW_FIXED) != 0;
    return 0;
}

int wlan_set_txpow(wlan_if_t vaphandle, int txpow)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    int is2GHz = IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan);
    int fixed = (ic->ic_flags & IEEE80211_F_TXPOW_FIXED) != 0;

    if (txpow > 0) {
        if ((ic->ic_caps & IEEE80211_C_TXPMGT) == 0)
            return -EINVAL;
        /*
         * txpow is in dBm while we store in 0.5dBm units
         */
        ic->ic_set_txPowerLimit(ic, 2*txpow, 2*txpow, is2GHz);
        ic->ic_flags |= IEEE80211_F_TXPOW_FIXED;
    }
    else {
        if (!fixed) return EOK;
        ic->ic_set_txPowerLimit(ic,IEEE80211_TXPOWER_MAX,
                                   IEEE80211_TXPOWER_MAX, is2GHz);
        ic->ic_flags &= ~IEEE80211_F_TXPOW_FIXED;
    }
    return EOK;
}

u_int32_t
wlan_get_HWcapabilities(wlan_dev_t devhandle, ieee80211_cap cap)
{
    struct ieee80211com *ic = devhandle;
    u_int32_t val = 0;

    switch (cap) {
    case IEEE80211_CAP_SHSLOT:
        val = (ic->ic_caps & IEEE80211_C_SHSLOT) ? 1 : 0;
        break;

    case IEEE80211_CAP_SHPREAMBLE:
        val = (ic->ic_caps & IEEE80211_F_SHPREAMBLE) ? 1 : 0;
        break;

    case IEEE80211_CAP_MULTI_DOMAIN:
        val = (ic->ic_country.isMultidomain) ? 1 : 0;
        break;

    case IEEE80211_CAP_WMM:
        val = (ic->ic_caps & IEEE80211_C_WME) ? 1 : 0;
        break;

    case IEEE80211_CAP_HT:
        val = (ic->ic_caps & IEEE80211_C_HT) ? 1 : 0;
        break;

    default:
        break;
    }

    return val;
}

int wlan_get_maxphyrate(wlan_if_t vaphandle)
{
 struct ieee80211vap *vap = vaphandle;
 struct ieee80211com *ic = vap->iv_ic; 
 uint32_t shortgi;
 shortgi = ieee80211com_has_htflags(ic, IEEE80211_HTF_SHORTGI40 | IEEE80211_HTF_SHORTGI20);
 return(ic->ic_get_maxphyrate(ic, vap->iv_bss,shortgi) * 1000);
} 

int wlan_get_current_phytype(struct ieee80211com *ic)
{
  return(ic->ic_phytype);
}

int
ieee80211_get_desired_ssid(struct ieee80211vap *vap, int index, ieee80211_ssid **ssid)
{
    if (index > vap->iv_des_nssid) {
        return -EOVERFLOW;
    }

    *ssid = &(vap->iv_des_ssid[index]);
    return 0;
}

int
ieee80211_get_desired_ssidlist(struct ieee80211vap *vap, 
                               ieee80211_ssid *ssidlist,
                               int nssid)
{
    int i;

    if (nssid < vap->iv_des_nssid)
        return -EOVERFLOW;
    
    for (i = 0; i < vap->iv_des_nssid; i++) {
        ssidlist[i].len = vap->iv_des_ssid[i].len;
        OS_MEMCPY(ssidlist[i].ssid,
                  vap->iv_des_ssid[i].ssid,
                  ssidlist[i].len);
    }

    return vap->iv_des_nssid;
}

int
wlan_get_desired_ssidlist(wlan_if_t vaphandle, ieee80211_ssid *ssidlist, int nssid)
{
    return ieee80211_get_desired_ssidlist(vaphandle, ssidlist, nssid);
}

int
wlan_set_desired_ssidlist(wlan_if_t vaphandle,
                          u_int16_t nssid, 
                          ieee80211_ssid *ssidlist)
{
    struct ieee80211vap *vap = vaphandle;
    int i;

    if (nssid > IEEE80211_SCAN_MAX_SSID) {
        return -EOVERFLOW;
    }

    for (i = 0; i < nssid; i++) {
        vap->iv_des_ssid[i].len = ssidlist[i].len;
        if (vap->iv_des_ssid[i].len) {
            OS_MEMCPY(vap->iv_des_ssid[i].ssid,
                      ssidlist[i].ssid,
                      ssidlist[i].len);
        }
    }

    vap->iv_des_nssid = nssid;    
    return 0; 
}

void
wlan_get_bss_essid(wlan_if_t vaphandle, ieee80211_ssid *essid)
{
    struct ieee80211vap *vap = vaphandle;
    essid->len = vap->iv_bss->ni_esslen;
    OS_MEMCPY(essid->ssid,vap->iv_bss->ni_essid, vap->iv_bss->ni_esslen);
}

int wlan_set_wmm_param(wlan_if_t vaphandle, wlan_wme_param param, u_int8_t isbss, u_int8_t ac, u_int32_t val)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    int retval=EOK;
    struct ieee80211_wme_state *wme = &ic->ic_wme;

    switch (param)
    {
    case WLAN_WME_CWMIN:
        if (isbss)
        {
            wme->wme_wmeBssChanParams.cap_wmeParams[ac].wmep_logcwmin = val;
            if ((wme->wme_flags & WME_F_AGGRMODE) == 0)
            {
                wme->wme_bssChanParams.cap_wmeParams[ac].wmep_logcwmin = val;
            }
        }
        else
        {
            wme->wme_wmeChanParams.cap_wmeParams[ac].wmep_logcwmin = val;
            wme->wme_chanParams.cap_wmeParams[ac].wmep_logcwmin = val;
        }
        ieee80211_wme_updateparams(vap);
        break;
    case WLAN_WME_CWMAX:
        if (isbss)
        {
            wme->wme_wmeBssChanParams.cap_wmeParams[ac].wmep_logcwmax = val;
            if ((wme->wme_flags & WME_F_AGGRMODE) == 0)
            {
                wme->wme_bssChanParams.cap_wmeParams[ac].wmep_logcwmax = val;
            }
        }
        else
        {
            wme->wme_wmeChanParams.cap_wmeParams[ac].wmep_logcwmax = val;
            wme->wme_chanParams.cap_wmeParams[ac].wmep_logcwmax = val;
        }
        ieee80211_wme_updateparams(vap);
        break;
    case WLAN_WME_AIFS:
        if (isbss)
        {
            wme->wme_wmeBssChanParams.cap_wmeParams[ac].wmep_aifsn = val;
            if ((wme->wme_flags & WME_F_AGGRMODE) == 0)
            {
                wme->wme_bssChanParams.cap_wmeParams[ac].wmep_aifsn = val;
            }
        }
        else
        {
            wme->wme_wmeChanParams.cap_wmeParams[ac].wmep_aifsn = val;
            wme->wme_chanParams.cap_wmeParams[ac].wmep_aifsn = val;
        }
        ieee80211_wme_updateparams(vap);
        break;
    case WLAN_WME_TXOPLIMIT:
        if (isbss)
        {
            wme->wme_wmeBssChanParams.cap_wmeParams[ac].wmep_txopLimit
                = IEEE80211_US_TO_TXOP(val);
            if ((wme->wme_flags & WME_F_AGGRMODE) == 0)
            {
                wme->wme_bssChanParams.cap_wmeParams[ac].wmep_txopLimit
                    = IEEE80211_US_TO_TXOP(val);
            }
        }
        else
        {
            wme->wme_wmeChanParams.cap_wmeParams[ac].wmep_txopLimit
                = IEEE80211_US_TO_TXOP(val);
            wme->wme_chanParams.cap_wmeParams[ac].wmep_txopLimit
                = IEEE80211_US_TO_TXOP(val);
        }
        ieee80211_wme_updateparams(vap);
        break;
    case WLAN_WME_ACM:
        if (!isbss)
            return -EINVAL;
        /* ACM bit applies to BSS case only */
        wme->wme_wmeBssChanParams.cap_wmeParams[ac].wmep_acm = val;
        if ((wme->wme_flags & WME_F_AGGRMODE) == 0)
            wme->wme_bssChanParams.cap_wmeParams[ac].wmep_acm = val;
        break;
    case WLAN_WME_ACKPOLICY:
        if (isbss)
            return -EINVAL;
        /* ack policy applies to non-BSS case only */
        wme->wme_wmeChanParams.cap_wmeParams[ac].wmep_noackPolicy = val;
        wme->wme_chanParams.cap_wmeParams[ac].wmep_noackPolicy = val;
        break;
    default:
        break;
    }
    return retval;
}

u_int32_t wlan_get_wmm_param(wlan_if_t vaphandle, wlan_wme_param param, u_int8_t isbss, u_int8_t ac)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_wme_state *wme = &ic->ic_wme;
    struct chanAccParams *chanParams = (isbss == 0) ?
            &(wme->wme_chanParams)
            : &(wme->wme_bssChanParams);

    switch (param)
    {
    case WLAN_WME_CWMIN:
        return chanParams->cap_wmeParams[ac].wmep_logcwmin;
        break;
    case WLAN_WME_CWMAX:
        return chanParams->cap_wmeParams[ac].wmep_logcwmax;
        break;
    case WLAN_WME_AIFS:
        return chanParams->cap_wmeParams[ac].wmep_aifsn;
        break;
    case WLAN_WME_TXOPLIMIT:
        return IEEE80211_TXOP_TO_US(chanParams->cap_wmeParams[ac].wmep_txopLimit);
        break;
    case WLAN_WME_ACM:
        return wme->wme_wmeBssChanParams.cap_wmeParams[ac].wmep_acm;
        break;
    case WLAN_WME_ACKPOLICY:
        return wme->wme_wmeChanParams.cap_wmeParams[ac].wmep_noackPolicy;
        break;
    default:
        break;
    }
    return 0;
}

int wlan_set_chanswitch(wlan_if_t vaphandle, int8_t *extra)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    int *param = (int *) extra;
    int freq;
#ifdef MAGPIE_HIF_GMAC
    struct ieee80211vap *tmp_vap = NULL;
#endif   
    freq = ieee80211_ieee2mhz(param[0], 0);

    if (ieee80211_find_channel(ic, freq, ic->ic_curchan->ic_flags) == NULL)
    {
        /* Switching between different modes is not allowed, print ERROR */
        printk("%s(): Channel capabilities do not match, chan flags 0x%x\n",
            __func__, ic->ic_curchan->ic_flags);
        return 0;
    }
    
    /*  flag the beacon update to include the channel switch IE */
    ic->ic_chanchange_chan = param[0];
    ic->ic_chanchange_tbtt = param[1];
#ifdef MAGPIE_HIF_GMAC
    TAILQ_FOREACH(tmp_vap, &ic->ic_vaps, iv_next) {
        ic->ic_chanchange_cnt += ic->ic_chanchange_tbtt;    
    }
#endif    
    ic->ic_flags |= IEEE80211_F_CHANSWITCH;
    return 0;
}

int wlan_set_clr_appopt_ie(wlan_if_t vaphandle)

{
    int ftype;

    IEEE80211_VAP_LOCK(vaphandle);

    /* Free app ie buffers */
    for (ftype = 0; ftype < IEEE80211_FRAME_TYPE_MAX; ftype++) {
        vaphandle->iv_app_ie_maxlen[ftype] = 0;
        if (vaphandle->iv_app_ie[ftype].ie) {
            OS_FREE(vaphandle->iv_app_ie[ftype].ie);
            vaphandle->iv_app_ie[ftype].ie = NULL;
            vaphandle->iv_app_ie[ftype].length = 0;
        }
    }

    /* Free opt ie buffer */
    vaphandle->iv_opt_ie_maxlen = 0;
    if (vaphandle->iv_opt_ie.ie) {
        OS_FREE(vaphandle->iv_opt_ie.ie);
        vaphandle->iv_opt_ie.ie = NULL;
        vaphandle->iv_opt_ie.length = 0;
    }

    /* Free beacon copy buffer */
    if (vaphandle->iv_beacon_copy_buf) {
        OS_FREE(vaphandle->iv_beacon_copy_buf);
        vaphandle->iv_beacon_copy_buf = NULL;
        vaphandle->iv_beacon_copy_len = 0;
    }        

    IEEE80211_VAP_UNLOCK(vaphandle);

    return 0;

}

/* set/get IQUE parameters */
#if ATH_SUPPORT_IQUE
int wlan_set_rtparams(wlan_if_t vaphandle, u_int8_t rt_index, u_int8_t per, u_int8_t probe_intvl)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    ic->ic_set_rtparams(ic, rt_index, per, probe_intvl);
    return 0;
}

int wlan_set_acparams(wlan_if_t vaphandle, u_int8_t ac, u_int8_t use_rts, u_int8_t aggrsize_scaling, u_int32_t min_kbps)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    ic->ic_set_acparams(ic, ac, use_rts, aggrsize_scaling, min_kbps);
    return 0;
}

int wlan_set_hbrparams(wlan_if_t vaphandle, u_int8_t ac, u_int8_t enable, u_int8_t per_low)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
	ic->ic_set_hbrparams(vap, ac, enable, per_low);
    return 0;
}

void wlan_get_hbrstate(wlan_if_t vaphandle)
{
    struct ieee80211vap *vap = vaphandle;
    if (vap->iv_ique_ops.hbr_dump) {
        vap->iv_ique_ops.hbr_dump(vap);
    }
}

int wlan_set_me_denyentry(wlan_if_t vaphandle, int *denyaddr)
{
    struct ieee80211vap *vap = vaphandle;
    if (vap->iv_ique_ops.me_adddeny) {
        vap->iv_ique_ops.me_adddeny(vap, denyaddr);
    }
    return 0;
}
#endif /* ATH_SUPPORT_IQUE */


#ifdef  ATH_SUPPORT_AOW

int
wlan_set_aow_param(wlan_if_t vaphandle, ieee80211_param param, u_int32_t value)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    int retv = 0;

    switch (param) {
        case IEEE80211_AOW_SWRETRIES:
            ic->ic_set_swretries(ic, value);
            break;
        case IEEE80211_AOW_LATENCY:
            ic->ic_set_aow_latency(ic, value);
            break;
        case IEEE80211_AOW_PLAY_LOCAL:
            ic->ic_set_aow_playlocal(ic, value);
            break;
        case IEEE80211_AOW_CLEAR_AUDIO_CHANNELS:
            ic->ic_aow_clear_audio_channels(ic, value);
            break;
        case IEEE80211_AOW_STATS:
            ieee80211_audio_stat_clear(ic);
            ieee80211_aow_clear_i2s_stats(ic);
            break;
        case IEEE80211_AOW_ESTATS:
            ic->ic_aow_clear_estats(ic);
            break;
        case IEEE80211_AOW_INTERLEAVE:
            ic->ic_aow.interleave = value ? 1:0;    
            break;
        case IEEE80211_AOW_ER:
            ic->ic_set_aow_er(ic, value);
            break;
        case IEEE80211_AOW_EC:
            ic->ic_set_aow_ec(ic, value);
            break;
        case IEEE80211_AOW_EC_FMAP:
            ic->ic_set_aow_ec_fmap(ic, value);
            break;
        case IEEE80211_AOW_ES:
            ic->ic_set_aow_es(ic, value);
            break;
        case IEEE80211_AOW_ESS:
            ic->ic_set_aow_ess(ic, value);
            break;
        case IEEE80211_AOW_ESS_COUNT:
            ic->ic_set_aow_ess_count(ic, value);
            break;
        case IEEE80211_AOW_ENABLE_CAPTURE:
            ieee80211_set_audio_data_capture(ic);
            break;
        case IEEE80211_AOW_FORCE_INPUT:
            ieee80211_set_force_aow_data(ic, value);
            break;
        case IEEE80211_AOW_PRINT_CAPTURE:
            ieee80211_audio_print_capture_data(ic);
            break;
        default:
            break;
    }

    return retv;
}    

u_int32_t
wlan_get_aow_param(wlan_if_t vaphandle, ieee80211_param param)
{

    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    u_int32_t val = 0;

    switch (param) {
        case IEEE80211_AOW_SWRETRIES:
            val = ic->ic_get_swretries(ic);
            break;
        case IEEE80211_AOW_LATENCY:
            val = ic->ic_get_aow_latency(ic);
            break;
        case IEEE80211_AOW_PLAY_LOCAL:
            val = ic->ic_get_aow_playlocal(ic);
            break;
        case IEEE80211_AOW_STATS:
            val = ieee80211_aow_get_stats(ic);
            break;
        case IEEE80211_AOW_ESTATS:
            val = ic->ic_aow_get_estats(ic);
            break;
        case IEEE80211_AOW_INTERLEAVE:
            val = ic->ic_aow.interleave;
            break;
        case IEEE80211_AOW_ER:
            val = ic->ic_get_aow_er(ic);
            break;
        case IEEE80211_AOW_EC:
            val = ic->ic_get_aow_ec(ic);
            break;
        case IEEE80211_AOW_EC_FMAP:
            val = ic->ic_get_aow_ec_fmap(ic);
            break;
        case IEEE80211_AOW_ES:
            val = ic->ic_get_aow_es(ic);
            break;
        case IEEE80211_AOW_ESS:
            val = ic->ic_get_aow_ess(ic);
            break;
        case IEEE80211_AOW_LIST_AUDIO_CHANNELS:
            ic->ic_list_audio_channel(ic);
            val = 0;
            break;
        default:
            break;
      
    }
    return val;

}

#endif  /* ATH_SUPPORT_AOW */
