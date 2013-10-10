/*
 * Copyright (c) 2010, Atheros Communications Inc.
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

/*
 * Atheros Wireless LAN controller driver for net80211 stack.
 */

#include "if_athvar.h"

#include "ath_cwm.h"
#include "if_ath_amsdu.h"
#include "if_ath_uapsd.h"
#include "if_ath_htc.h"
#include "if_llc.h"
#include "if_ath_aow.h"
 
#include "asf_amem.h"     /* asf_amem_setup */
#include "asf_print.h"    /* asf_print_setup */

#include "adf_os_mem.h"   /* adf_os_mem_alloc,free */
#include "adf_os_lock.h"  /* adf_os_spinlock_* */
#include "adf_os_types.h" /* adf_os_vprint */

#include "ath_lmac_state_event.h"

#if ATH_DEBUG
extern unsigned long ath_rtscts_enable;      /* defined in ah_osdep.c  */
#endif

/*
 * Mapping between WIRELESS_MODE_XXX to IEEE80211_MODE_XXX
 */
static enum ieee80211_phymode
ath_mode_map[WIRELESS_MODE_MAX] = {
    IEEE80211_MODE_11A,
    IEEE80211_MODE_11B,
    IEEE80211_MODE_11G,
    IEEE80211_MODE_TURBO_A,
    IEEE80211_MODE_TURBO_G,
    IEEE80211_MODE_11NA_HT20,
    IEEE80211_MODE_11NG_HT20,
    IEEE80211_MODE_11NA_HT40PLUS,
    IEEE80211_MODE_11NA_HT40MINUS,
    IEEE80211_MODE_11NG_HT40PLUS,
    IEEE80211_MODE_11NG_HT40MINUS,
    IEEE80211_MODE_MAX          /* XXX: for XR */
};

static void ath_net80211_rate_node_update(ieee80211_handle_t ieee, ieee80211_node_t node, int isnew);
static int ath_key_alloc(struct ieee80211vap *vap, struct ieee80211_key *k);
static int ath_key_delete(struct ieee80211vap *vap, const struct ieee80211_key *k,
                          struct ieee80211_node *ninfo);
static int ath_key_set(struct ieee80211vap *vap, const struct ieee80211_key *k,
                       const u_int8_t peermac[IEEE80211_ADDR_LEN]);
static void ath_key_update_begin(struct ieee80211vap *vap);
static void ath_key_update_end(struct ieee80211vap *vap);
static void ath_update_ps_mode(struct ieee80211vap *vap);
static void ath_net80211_set_config(struct ieee80211vap* vap);
static void ath_setTxPowerLimit(struct ieee80211com *ic, u_int32_t limit, u_int16_t tpcInDb, u_int32_t is2GHz);
static u_int8_t ath_net80211_get_common_power(struct ieee80211com *ic, struct ieee80211_channel *chan);
static u_int32_t ath_net80211_get_maxphyrate(struct ieee80211com *ic, struct ieee80211_node *ni,uint32_t shortgi);
#ifdef ATH_CCX
static int  ath_getrmcounters(struct ieee80211com *ic, struct ath_mib_cycle_cnts *pCnts);
static void ath_clearrmcounters(struct ieee80211com *ic);
static int  ath_updatermcounters(struct ieee80211com *ic, struct ath_mib_mac_stats* pStats);
static u_int64_t ath_getTSF64(struct ieee80211com *ic);
static void ath_setReceiveFilter(struct ieee80211com *ic);
static int ath_getMfgSerNum(struct ieee80211com *ic, u_int8_t *pSrn, int limit);
static int ath_net80211_get_chanData(struct ieee80211com *ic, struct ieee80211_channel *pChan, struct ath_chan_data *pData);
static u_int32_t ath_net80211_get_curRSSI(struct ieee80211com *ic);
#endif
static u_int32_t ath_getTSF32(struct ieee80211com *ic);
static u_int16_t ath_net80211_find_countrycode(struct ieee80211com *ic, char* isoName);
static void ath_setup_keycacheslot(struct ieee80211_node *ni);
static void ath_net80211_log_text(struct ieee80211com *ic, char *text);
static int ath_vap_join(struct ieee80211vap *vap);
static int ath_vap_up(struct ieee80211vap *vap) ;
static int ath_vap_listen(struct ieee80211vap *vap) ;
static bool ath_net80211_need_beacon_sync(struct ieee80211com *ic);
static u_int32_t ath_net80211_wpsPushButton(struct ieee80211com *ic);




#ifdef IEEE80211_DEBUG_NODELEAK
static void ath_net80211_debug_print_nodeq_info(struct ieee80211_node *ni);
#endif
static void
ath_net80211_pwrsave_set_state(struct ieee80211com *ic, IEEE80211_PWRSAVE_STATE newstate);

static u_int32_t ath_net80211_gettsf32(struct ieee80211com *ic);
static u_int64_t ath_net80211_gettsf64(struct ieee80211com *ic);
static void ath_net80211_update_node_txpow(struct ieee80211vap *vap, u_int16_t txpowlevel, u_int8_t *addr); 
/*static*/ void ath_net80211_enable_tpc(struct ieee80211com *ic);
/*static*/ void ath_net80211_get_max_txpwr(struct ieee80211com *ic, u_int32_t* txpower );
static int ath_net80211_vap_pause_control (struct ieee80211com *ic, struct ieee80211vap *vap, bool pause);
static void ath_net80211_get_bssid(ieee80211_handle_t ieee,  struct
        ieee80211_frame_min *shdr, u_int8_t *bssid);
#ifdef ATH_TX99_DIAG
static struct ieee80211_channel *
ath_net80211_find_channel(struct ath_softc *sc, int ieee, enum ieee80211_phymode mode);
#endif
static void ath_net80211_set_ldpcconfig(ieee80211_handle_t ieee, u_int8_t ldpc);
static void ath_net80211_set_stbcconfig(ieee80211_handle_t ieee, u_int8_t stbc, u_int8_t istx);

#ifdef ATH_SUPPORT_TxBF     // For TxBF RC
#ifdef TXBF_TODO
static void	ath_net80211_get_pos2_data(struct ieee80211com *ic, u_int8_t **p_data, 
                                u_int16_t* p_len,void **rx_status);
static bool ath_net80211_txbfrcupdate(struct ieee80211com *ic,void *rx_status,
                                u_int8_t *local_h, u_int8_t *CSIFrame,u_int8_t NESSA,u_int8_t NESSB, int BW); 
static void ath_net80211_ap_save_join_mac(struct ieee80211com *ic, u_int8_t *join_macaddr);
static void ath_net80211_start_smbf_cal(struct ieee80211com *ic);
#endif
static int  ath_net80211_txbf_alloc_key(struct ieee80211com *ic, struct ieee80211_node *ni);
static void ath_net80211_txbf_set_key(struct ieee80211com *ic, struct ieee80211_node *ni);
static void ath_net80211_init_sw_cvtimeout(struct ieee80211com *ic, struct ieee80211_node *ni);
static void ath_net80211_txbf_stats_rpt_inc(struct ieee80211com *ic, struct ieee80211_node *ni);
static void ath_net80211_txbf_set_rpt_received(struct ieee80211com *ic, struct ieee80211_node *ni);
#endif
static void ath_net80211_enablerifs_ldpcwar(struct ieee80211_node *ni, bool value);
#if UMAC_SUPPORT_VI_DBG
static void ath_net80211_set_vi_dbg_restart(struct ieee80211com *ic);
static void ath_net80211_set_vi_dbg_log(struct ieee80211com *ic, bool enable);
#endif

#if UMAC_SUPPORT_SMARTANTENNA
static u_int32_t ath_net80211_get_txbuf_free(struct ieee80211com *ic);
static u_int32_t ath_net80211_get_txbuf_used(struct ieee80211com *ic , int ac);
static void ath_net80211_prepare_rateset(struct ieee80211com *ic, struct ieee80211_node *ni);
static int8_t ath_net80211_get_smartantenna_enable(struct ieee80211com *ic);
static void ath_net80211_update_smartant_pertable (ieee80211_node_t node, void *pertab);
static void ath_net80211_get_smartantenna_ratestats(struct ieee80211_node *ni, void *rate_stats);
#endif
#if UMAC_SUPPORT_INTERNALANTENNA
static void ath_net80211_set_default_antenna(struct ieee80211com *ic, u_int32_t antenna);
static void ath_net80211_set_selected_smantennas(struct ieee80211_node *ni, int antenna, int rateset);
static u_int32_t ath_net80211_get_default_antenna(struct ieee80211com *ic);
#endif

#if ATH_SUPPORT_SPECTRAL
static int ath_net80211_get_control_duty_cycle(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    int temp;
    temp = scn->sc_ops->ath_dev_get_ctldutycycle(scn->sc_dev);
    return temp;
}

static int ath_net80211_get_extension_duty_cycle(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    int temp;
    temp=scn->sc_ops->ath_dev_get_extdutycycle(scn->sc_dev);
    return temp;
}
static void ath_net80211_start_spectral_scan(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->ath_dev_start_spectral_scan(scn->sc_dev);
}
static void ath_net80211_stop_spectral_scan(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->ath_dev_stop_spectral_scan(scn->sc_dev);
}
#endif

#if ATH_SUPPORT_IQUE
void ath_net80211_hbr_settrigger(ieee80211_node_t node, int event);
#endif

#if ATH_SUPPORT_VOWEXT
static u_int16_t ath_net80211_get_aid(ieee80211_node_t node);
#endif

static u_int32_t ath_net80211_txq_depth(struct ieee80211com *ic);
static u_int32_t ath_net80211_txq_depth_ac(struct ieee80211com *ic, int ac);
u_int32_t ath_net80211_getmfpsupport(struct ieee80211com *ic);

static int
ath_net80211_reg_vap_info_notify(
    struct ieee80211vap                 *vap,
    ath_vap_infotype                    infotype_mask,
    ieee80211_vap_ath_info_notify_func  callback,
    void                                *arg);

static int
ath_net80211_vap_info_update_notify(
    struct ieee80211vap                 *vap,
    ath_vap_infotype                    infotype_mask);

static int
ath_net80211_dereg_vap_info_notify(
    struct ieee80211vap                 *vap);

static int
ath_net80211_vap_info_get(
    struct ieee80211vap *vap,
    ath_vap_infotype    infotype,
    u_int32_t           *param1,
    u_int32_t           *param2);

/*---------------------
 * Support routines
 *---------------------
 */
static u_int
ath_chan2flags(struct ieee80211_channel *chan)
{
    u_int flags;
    static const u_int modeflags[] = {
        0,                   /* IEEE80211_MODE_AUTO           */
        CHANNEL_A,           /* IEEE80211_MODE_11A            */
        CHANNEL_B,           /* IEEE80211_MODE_11B            */
        CHANNEL_PUREG,       /* IEEE80211_MODE_11G            */
        0,                   /* IEEE80211_MODE_FH             */
        CHANNEL_108A,        /* IEEE80211_MODE_TURBO_A        */
        CHANNEL_108G,        /* IEEE80211_MODE_TURBO_G        */
        CHANNEL_A_HT20,      /* IEEE80211_MODE_11NA_HT20      */
        CHANNEL_G_HT20,      /* IEEE80211_MODE_11NG_HT20      */
        CHANNEL_A_HT40PLUS,  /* IEEE80211_MODE_11NA_HT40PLUS  */
        CHANNEL_A_HT40MINUS, /* IEEE80211_MODE_11NA_HT40MINUS */
        CHANNEL_G_HT40PLUS,  /* IEEE80211_MODE_11NG_HT40PLUS  */
        CHANNEL_G_HT40MINUS, /* IEEE80211_MODE_11NG_HT40MINUS */
    };

    flags = modeflags[ieee80211_chan2mode(chan)];

    if (IEEE80211_IS_CHAN_HALF(chan)) {
        flags |= CHANNEL_HALF;
    } else if (IEEE80211_IS_CHAN_QUARTER(chan)) {
        flags |= CHANNEL_QUARTER;
    }

    return flags;
}

/*
 * Compute numbner of chains based on chainmask
 */
static INLINE int
ath_get_numchains(int chainmask)
{
    int chains;

    switch (chainmask) {
    default:
        chains = 0;
        printk("%s: invalid chainmask\n", __func__);
        break;
    case 1:
    case 2:
    case 4:
        chains = 1;
        break;
    case 3:
    case 5:
    case 6:
        chains = 2;
        break;
    case 7:
        chains = 3;
        break;
    }
    return chains;
}

/*
 * Determine the capabilities that are passed to rate control module.
 */
u_int32_t
ath_set_ratecap(struct ath_softc_net80211 *scn, struct ieee80211_node *ni,
        struct ieee80211vap *vap)
{
    int numtxchains;
    u_int32_t ratecap;

    ratecap = 0;
    numtxchains =
        ath_get_numchains(scn->sc_ops->have_capability(scn->sc_dev, ATH_CAP_TXCHAINMASK));

#if ATH_SUPPORT_WAPI
    /*
     * WAPI engine only support 2 stream rates at most
     */
    if (ieee80211_vap_wapi_is_set(vap)) {
        int wapimaxtxchains = 
            scn->sc_ops->have_capability(scn->sc_dev, ATH_CAP_WAPI_MAXTXCHAINS);
        if (numtxchains > wapimaxtxchains) {
            numtxchains = wapimaxtxchains;
        }
    }
#endif

    /*
     *  Set three stream capability if all of the following are true
     *  - HAL supports three streams
     *  - three chains are available
     *  - remote node has advertised support for three streams
     */
    if (scn->sc_ops->have_capability(scn->sc_dev, ATH_CAP_TS) &&
        (numtxchains == 3) && (ni->ni_streams == 3))
    {
        ratecap |= ATH_RC_TS_FLAG;
    }
    /*
     *  Set two stream capability if all of the following are true
     *  - HAL supports two streams
     *  - two or more chains are available
     *  - remote node has advertised support for two or more streams
     */
    if (scn->sc_ops->have_capability(scn->sc_dev, ATH_CAP_DS) &&
        (numtxchains >= 2) && (ni->ni_streams >= 2))
    {
        ratecap |= ATH_RC_DS_FLAG;
    }

    /*
     * With SM power save, only singe stream rates can be used.
     */
    switch (ni->ni_htcap & IEEE80211_HTCAP_C_SM_MASK) {
    case IEEE80211_HTCAP_C_SMPOWERSAVE_DYNAMIC:
    case IEEE80211_HTCAP_C_SM_ENABLED:
        {
            if (ieee80211_vap_smps_is_set(vap))
            {
                ratecap &= ~(ATH_RC_TS_FLAG|ATH_RC_DS_FLAG);
            }
        }
        break;
    case IEEE80211_HTCAP_C_SMPOWERSAVE_STATIC:
        {
            ratecap &= ~(ATH_RC_TS_FLAG|ATH_RC_DS_FLAG);
            break;
        }
    default:
        break;
    }
    /*
     * If either of Implicit or Explicit TxBF is enabled for
     * the remote node, set the rate capability. 
     */
#ifdef  ATH_SUPPORT_TxBF
    if ((ni->ni_explicit_noncompbf == AH_TRUE) ||
        (ni->ni_explicit_compbf == AH_TRUE) ||
        (ni->ni_implicit_bf == AH_TRUE))
    {
        ratecap |= ATH_RC_TXBF_FLAG;
    }
#endif
    return ratecap;
}

WIRELESS_MODE
ath_ieee2wmode(enum ieee80211_phymode mode)
{
    WIRELESS_MODE wmode;
    
    for (wmode = 0; wmode < WIRELESS_MODE_MAX; wmode++) {
        if (ath_mode_map[wmode] == mode)
            break;
    }

    return wmode;
}

/* Query ATH layer for tx/rx chainmask and set in the com object via OS stack */
static void
ath_set_chainmask(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    int tx_chainmask = 0, rx_chainmask = 0;

    if (!scn->sc_ops->ath_get_config_param(scn->sc_dev, ATH_PARAM_TXCHAINMASK,
                                           &tx_chainmask))
        ieee80211com_set_tx_chainmask(ic, (u_int8_t) tx_chainmask);

    if (!scn->sc_ops->ath_get_config_param(scn->sc_dev, ATH_PARAM_RXCHAINMASK,
                                           &rx_chainmask))
        ieee80211com_set_rx_chainmask(ic, (u_int8_t) rx_chainmask);
}

#if ATH_SUPPORT_WAPI
/* Query ATH layer for max tx/rx chain supported by WAPI engine */
static void
ath_set_wapi_maxchains(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    ieee80211com_set_wapi_max_tx_chains(ic, 
        (u_int8_t)scn->sc_ops->have_capability(scn->sc_dev, ATH_CAP_WAPI_MAXTXCHAINS));
    ieee80211com_set_wapi_max_rx_chains(ic, 
        (u_int8_t)scn->sc_ops->have_capability(scn->sc_dev, ATH_CAP_WAPI_MAXRXCHAINS));
}
#endif

#ifdef  ATH_SUPPORT_TxBF
static void
ath_get_nodetxbfcaps(struct ieee80211_node *ni, ieee80211_txbf_caps_t **txbf)
{
    (*txbf)->channel_estimation_cap    = ni->ni_txbf.channel_estimation_cap;
    (*txbf)->csi_max_rows_bfer         = ni->ni_txbf.csi_max_rows_bfer;
    (*txbf)->comp_bfer_antennas        = ni->ni_txbf.comp_bfer_antennas;
    (*txbf)->noncomp_bfer_antennas     = ni->ni_txbf.noncomp_bfer_antennas;
    (*txbf)->csi_bfer_antennas         = ni->ni_txbf.csi_bfer_antennas;
    (*txbf)->minimal_grouping          = ni->ni_txbf.minimal_grouping;
    (*txbf)->explicit_comp_bf          = ni->ni_txbf.explicit_comp_bf;
    (*txbf)->explicit_noncomp_bf       = ni->ni_txbf.explicit_noncomp_bf;
    (*txbf)->explicit_csi_feedback     = ni->ni_txbf.explicit_csi_feedback;
    (*txbf)->explicit_comp_steering    = ni->ni_txbf.explicit_comp_steering;
    (*txbf)->explicit_noncomp_steering = ni->ni_txbf.explicit_noncomp_steering;
    (*txbf)->explicit_csi_txbf_capable = ni->ni_txbf.explicit_csi_txbf_capable;
    (*txbf)->calibration               = ni->ni_txbf.calibration;
    (*txbf)->implicit_txbf_capable     = ni->ni_txbf.implicit_txbf_capable;
    (*txbf)->tx_ndp_capable            = ni->ni_txbf.tx_ndp_capable;
    (*txbf)->rx_ndp_capable            = ni->ni_txbf.rx_ndp_capable;
    (*txbf)->tx_staggered_sounding     = ni->ni_txbf.tx_staggered_sounding;
    (*txbf)->rx_staggered_sounding     = ni->ni_txbf.rx_staggered_sounding;
    (*txbf)->implicit_rx_capable       = ni->ni_txbf.implicit_rx_capable;
}
#endif

/*------------------------------------------------------------
 * Callbacks for net80211 module, which will be hooked up as
 * ieee80211com vectors (ic->ic_xxx) accordingly.
 *------------------------------------------------------------
 */

static int
ath_init(struct ieee80211com *ic)
{
#define GREEN_AP_SUSPENDED    2
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ieee80211_channel *cc;
    HAL_CHANNEL hchan;
    int error = 0;

    /* stop protocol stack first */
    ieee80211_stop_running(ic);
    
    /* setup initial channel */
    cc = ieee80211_get_current_channel(ic);
    hchan.channel = ieee80211_chan2freq(ic, cc);
    hchan.channelFlags = ath_chan2flags(cc);
    
    /* open ath_dev */
    error = scn->sc_ops->open(scn->sc_dev, &hchan);
    if (error)
        return error;

    /* Set tx/rx chainmask */
    ath_set_chainmask(ic);

#if ATH_SUPPORT_WAPI
    /* Set WAPI max tx/rx chain */
    ath_set_wapi_maxchains(ic);
#endif

    /* Initialize CWM (Channel Width Management) */
    cwm_init(ic);

    /* kick start 802.11 state machine */
    ieee80211_start_running(ic);

    /* update max channel power to max regpower of current channel */
    ieee80211com_set_curchanmaxpwr(ic, cc->ic_maxregpower);

	/*
	** Enable the green AP function
	*/
    if(ic->ic_green_ap_get_enable(ic) == GREEN_AP_SUSPENDED) /* if it suspended */
    {
        ic->ic_green_ap_set_enable(ic,1);
    }
#undef GREEN_AP_SUSPENDED
    return error;
}

static void ath_vap_iter_vap_create(void *arg, wlan_if_t vap) 
{
    
    int *pid_mask = (int *) arg;
    u_int8_t myaddr[IEEE80211_ADDR_LEN];
    u_int8_t id = 0;

    ieee80211vap_get_macaddr(vap, myaddr);
    ATH_GET_VAP_ID(myaddr, wlan_vap_get_hw_macaddr(vap), id);
    (*pid_mask) |= (1 << id);
}

/**
 * createa  vap.
 * if IEEE80211_CLONE_BSSID flag is set then it will  allocate a new mac address.
 * if IEEE80211_CLONE_BSSID flag is not set then it will use passed in bssid as
 * the mac adddress.
 */

static struct ieee80211vap *
ath_vap_create(struct ieee80211com *ic,
               int                 opmode, 
               int                 scan_priority_base,
               int                 flags, 
               const u_int8_t      bssid[IEEE80211_ADDR_LEN])
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_vap_net80211 *avn;
    u_int8_t myaddr[IEEE80211_ADDR_LEN];
    struct ieee80211vap *vap;
    int id = 0, id_mask = 0;
    int nvaps = 0;
    int ic_opmode, ath_opmode = opmode;
    int nostabeacons = 0;
    struct ath_vap_config ath_vap_config;

    DPRINTF(scn, ATH_DEBUG_STATE, 
            "%s : enter., opmode=%x, flags=0x%x\n", 
            __func__,
	    opmode,
	    flags
	   );

    /* do a full search to mark all the allocated vaps */
    nvaps = wlan_iterate_vap_list(ic,ath_vap_iter_vap_create,(void *) &id_mask);

    id_mask |= scn->sc_prealloc_idmask; /* or in allocated ids */

    switch (opmode) {
    case IEEE80211_M_STA:   /* ap+sta for repeater application */
#if 0
        if ((nvaps != 0) && (!(flags & IEEE80211_NO_STABEACONS)))
            return NULL;   /* If using station beacons, must first up */
#endif
        if (flags & IEEE80211_NO_STABEACONS) {
            nostabeacons = 1;
            ic_opmode = IEEE80211_M_HOSTAP;	/* Run with chip in AP mode */
        } else {
            ic_opmode = opmode;
        }
        break;
    case IEEE80211_M_IBSS:
    case IEEE80211_M_MONITOR:
#if 0
    /*
     * TBD: Win7 usually has two STA ports created when configure one port in IBSS.
     */
        if (nvaps != 0)     /* only one */
            return NULL;
#endif
        ic_opmode = opmode;
        ath_opmode = opmode;
        break;
    case IEEE80211_M_HOSTAP:
    case IEEE80211_M_WDS:
        ic_opmode = IEEE80211_M_HOSTAP;
        break;
    case IEEE80211_M_BTAMP:
        ic_opmode = IEEE80211_M_HOSTAP;
        ath_opmode = IEEE80211_M_HOSTAP;
        break;
    default:
        return NULL;
    }

    /* calculate an interface id */
    if (flags & IEEE80211_P2PDEV_VAP) {
        id = ATH_P2PDEV_IF_ID;
    } else if ((flags & IEEE80211_CLONE_BSSID) &&
        nvaps != 0 && opmode != IEEE80211_M_WDS &&
        scn->sc_ops->have_capability(scn->sc_dev, ATH_CAP_BSSIDMASK)) {
        /*
         * Hardware supports the bssid mask and a unique bssid was
         * requested.  Assign a new mac address and expand our bssid
         * mask to cover the active virtual ap's with distinct
         * addresses.
         */
        KASSERT(nvaps <= ATH_BCBUF, ("too many virtual ap's: %d", nvaps));

        for (id = 0; id < ATH_BCBUF; id++) {
            /* get the first available slot */
            if ((id_mask & (1 << id)) == 0)
                break;
        }
    }

    if ((flags & IEEE80211_CLONE_BSSID) == 0 ) {
        /* do not clone use the one passed in */
 
        /* extract the id from the bssid */
        ATH_GET_VAP_ID(bssid, ic->ic_my_hwaddr, id);
        if ( (scn->sc_prealloc_idmask & (1 << id)) == 0) {
            /* the mac address was not pre allocated with ath_vap_alloc_macaddr */
            printk("%s: the vap mac address was not pre allocated \n",__func__);
            return NULL;
        }
        
        IEEE80211_ADDR_COPY(myaddr,ic->ic_my_hwaddr);
        /* generate the mac address from id and sanity check */
        ATH_SET_VAP_BSSID(myaddr,ic->ic_my_hwaddr, id);
        if (OS_MEMCMP(bssid,myaddr,IEEE80211_ADDR_LEN) != 0 ) {
            printk("%s: invalid (not locally administered) mac address was passed\n",__func__);
            return NULL;
        }
    }

    /* create the corresponding VAP */
    avn = (struct ath_vap_net80211 *)OS_ALLOC_VAP(scn->sc_osdev,
                                                    sizeof(struct ath_vap_net80211));
    if (avn == NULL) {
        printk("Can't allocate memory for ath_vap.\n");
        return NULL;
    }
    
    avn->av_sc = scn;
    avn->av_if_id = id;

    vap = &avn->av_vap;

    DPRINTF(scn, ATH_DEBUG_STATE, 
            " add an interface for ath_dev opmode %d ic_opmode %d .\n",ath_opmode,ic_opmode);

    if (nvaps) {
        /*
         * if there are more vaps. do not change the
         * opmode . the opmode will be changed dynamically 
         * whne vaps are brough up/down.
         */
        ic_opmode = ic->ic_opmode;
    }
    /* add an interface in ath_dev */
#if WAR_DELETE_VAP    
    if (scn->sc_ops->add_interface(scn->sc_dev, id, vap, ic_opmode, ath_opmode, nostabeacons, &vap->iv_athvap)) 
#else
    if (scn->sc_ops->add_interface(scn->sc_dev, id, vap, ic_opmode, ath_opmode, nostabeacons)) 
#endif
    {
        printk("Unable to add an interface for ath_dev.\n");
        OS_FREE_VAP(avn);
        return NULL;
    }


    ieee80211_vap_setup(ic, vap, opmode, scan_priority_base, flags, bssid);
    IEEE80211_VI_NEED_CWMIN_WORKAROUND_INIT(vap);
    /* override default ath_dev VAP configuration with IEEE VAP configuration */
    OS_MEMZERO(&ath_vap_config, sizeof(ath_vap_config));
    ath_vap_config.av_fixed_rateset = vap->iv_fixed_rateset;
    ath_vap_config.av_fixed_retryset = vap->iv_fixed_retryset; 
#ifdef ATH_SUPPORT_TxBF
    ath_vap_config.av_auto_cv_update = vap->iv_autocvupdate;
    ath_vap_config.av_cvupdate_per = vap->iv_cvupdateper;
#endif 
    ath_vap_config.av_short_gi =
            ieee80211com_has_htflags(ic, IEEE80211_HTF_SHORTGI40) ||
            ieee80211com_has_htflags(ic, IEEE80211_HTF_SHORTGI20);
    scn->sc_ops->config_interface(scn->sc_dev, id, &ath_vap_config);

    /* set up MAC address */
    ieee80211vap_get_macaddr(vap, myaddr);
    ATH_SET_VAP_BSSID(myaddr, wlan_vap_get_hw_macaddr((wlan_if_t)vap), id);
    ieee80211vap_set_macaddr(vap, myaddr);

    vap->iv_up = ath_vap_up;
    vap->iv_join = ath_vap_join;
    vap->iv_down = ath_vap_down;
    vap->iv_listen = ath_vap_listen;
    vap->iv_stopping = ath_vap_stopping;
    vap->iv_key_alloc = ath_key_alloc;
    vap->iv_key_delete = ath_key_delete;
    vap->iv_key_set = ath_key_set;
    vap->iv_key_update_begin = ath_key_update_begin;
    vap->iv_key_update_end = ath_key_update_end;

    vap->iv_reg_vap_ath_info_notify = ath_net80211_reg_vap_info_notify;
    vap->iv_vap_ath_info_update_notify = ath_net80211_vap_info_update_notify;
    vap->iv_dereg_vap_ath_info_notify = ath_net80211_dereg_vap_info_notify;
    vap->iv_vap_ath_info_get = ath_net80211_vap_info_get;

    vap->iv_update_ps_mode = ath_update_ps_mode;
    vap->iv_unit = id;
    vap->iv_update_node_txpow = ath_net80211_update_node_txpow;

    /* complete setup */
    (void) ieee80211_vap_attach(vap);

    ic->ic_opmode = ic_opmode;
    if (opmode == IEEE80211_M_STA)
        scn->sc_nstavaps++;

    /* Note that if it was pre allocated, we need an explicit ath_vap_free_macaddr to free it. */

    DPRINTF(scn, ATH_DEBUG_STATE, 
            "%s : exit. vap=0x%p is created.\n", 
            __func__,
            vap
           );

    return vap;
}

static void
ath_vap_delete(struct ieee80211vap *vap)
{
    struct ath_vap_net80211 *avn = ATH_VAP_NET80211(vap);
    struct ath_softc_net80211 *scn = avn->av_sc;
    int ret;

#ifndef ATH_SUPPORT_HTC
    KASSERT(ieee80211_vap_active_is_set(vap) == 0, ("vap not stopped"));
#else
#define WAIT_VAP_IDLE_INTERVALL 10000
    /* Because we will set vap active forcely to send p2p action frames, so we skipp active set check.
       And try to check vap->iv_state_info.iv_state = IEEE80211_S_INIT */
    KASSERT(vap->iv_state_info.iv_state == IEEE80211_S_INIT, ("vap not return to init state")); 
    /* Check action frame queue, set deleted for each found. */
    {
        wbuf_t wbuf;
        struct ath_usb_p2p_action_queue *cur_action_wbuf;
        struct ieee80211_node *action_ni;

        /* check action queue, remove related */
        IEEE80211_STATE_P2P_ACTION_LOCK_IRQ(scn);
        cur_action_wbuf = scn->sc_p2p_action_queue_head;
        while(cur_action_wbuf) {
            wbuf = cur_action_wbuf->wbuf;
            action_ni = wbuf_get_node(wbuf);
            if (action_ni->ni_vap == vap) {
                printk("====== Remove queued action frames vap %p, wbuf %p\n",vap,wbuf);
                cur_action_wbuf->deleted = 1;
                if (cur_action_wbuf == scn->sc_p2p_action_queue_tail)
                    cur_action_wbuf = NULL;
            }
            if (cur_action_wbuf)
                cur_action_wbuf = cur_action_wbuf->next;
        }
        IEEE80211_STATE_P2P_ACTION_UNLOCK_IRQ(scn);
        /* sleep a while to let ongoing action cb complete */
        OS_SLEEP(WAIT_VAP_IDLE_INTERVALL);
    }
#undef WAIT_VAP_IDLE_INTERVALL
#endif

    DPRINTF(scn, ATH_DEBUG_STATE, 
            "%s : enter. vap=0x%p\n", 
            __func__,
            vap
           );

    /* remove the interface from ath_dev */
#if WAR_DELETE_VAP
    ret = scn->sc_ops->remove_interface(scn->sc_dev, avn->av_if_id, vap->iv_athvap);
#else
    ret = scn->sc_ops->remove_interface(scn->sc_dev, avn->av_if_id);
#endif
    KASSERT(ret == 0, ("invalid interface id"));

    if (ieee80211vap_get_opmode(vap) == IEEE80211_M_STA)
        scn->sc_nstavaps--;

    IEEE80211_DELETE_VAP_TARGET(vap);

    /* detach VAP from the procotol stack */
    ieee80211_vap_detach(vap);

    OS_FREE_VAP(avn);
}


/*
 * pre allocate a ma caddress and return it in bssid 
 */
static int 
ath_vap_alloc_macaddr(struct ieee80211com *ic, u_int8_t *bssid)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    int id = 0, id_mask = 0;
    int nvaps = 0;

    DPRINTF(scn, ATH_DEBUG_STATE, "%s \n", __func__);

    /* do a full search to mark all the allocated vaps */
    nvaps = wlan_iterate_vap_list(ic,ath_vap_iter_vap_create,(void *) &id_mask);

    id_mask |= scn->sc_prealloc_idmask; /* or in allocated ids */

    for (id = 0; id < ATH_BCBUF; id++) {
         /* get the first available slot */
         if ((id_mask & (1 << id)) == 0)
             break;
    }
    if (id == ATH_BCBUF) {
        /* no more ids left */
        DPRINTF(scn, ATH_DEBUG_STATE, "%s Nomore free slots left \n", __func__);
        return -1;
    }

    /* set the allocated id in to the mask */
    scn->sc_prealloc_idmask |= (1 << id);

    IEEE80211_ADDR_COPY(bssid,ic->ic_my_hwaddr);
    /* copy the mac address into the bssid field */
    ATH_SET_VAP_BSSID(bssid,ic->ic_my_hwaddr, id);
    
    return 0;

}

/*
 * free a  pre allocateed  mac caddresses. 
 */
static int 
ath_vap_free_macaddr(struct ieee80211com *ic, u_int8_t *bssid)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    int id = 0;
    /* extract the id from the bssid */
    ATH_GET_VAP_ID(bssid, ic->ic_my_hwaddr, id);

#if UMAC_SUPPORT_P2P
    if (id == ATH_P2PDEV_IF_ID) {
        DPRINTF(scn, ATH_DEBUG_STATE, "%s P2P device mac address \n", __func__);
        return -1;
    }
#endif
    /* if it was pre allocated, remove it from pre allocated bitmap */
    if (scn->sc_prealloc_idmask & (1 << id) ) {
        scn->sc_prealloc_idmask &= ~(1 << id);
        return 0;
    } else {
        DPRINTF(scn, ATH_DEBUG_STATE, "%s not a pre allocated mac address \n", __func__);
        return -1;
    }
}

static void
ath_net80211_set_config(struct ieee80211vap* vap)
{
    struct ath_vap_net80211 *avn = ATH_VAP_NET80211(vap);
    struct ath_softc_net80211 *scn = avn->av_sc;
    struct ath_vap_config ath_vap_config;

    /* override default ath_dev VAP configuration with IEEE VAP configuration */
    OS_MEMZERO(&ath_vap_config, sizeof(ath_vap_config));
    ath_vap_config.av_fixed_rateset = vap->iv_fixed_rateset;
    ath_vap_config.av_fixed_retryset = vap->iv_fixed_retryset; 
#if ATH_SUPPORT_AP_WDS_COMBO
    ath_vap_config.av_no_beacon = vap->iv_no_beacon;
#endif
#ifdef ATH_SUPPORT_TxBF
    ath_vap_config.av_auto_cv_update = vap->iv_autocvupdate;
    ath_vap_config.av_cvupdate_per = vap->iv_cvupdateper;
#endif 
    ath_vap_config.av_short_gi =
            ieee80211com_has_htflags(vap->iv_ic, IEEE80211_HTF_SHORTGI40) ||
            ieee80211com_has_htflags(vap->iv_ic, IEEE80211_HTF_SHORTGI20);
    scn->sc_ops->config_interface(scn->sc_dev, avn->av_if_id, &ath_vap_config);
}

static struct ieee80211_node *
ath_net80211_node_alloc(struct ieee80211_node_table *nt, struct ieee80211vap *vap, bool tmpnode)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_vap_net80211 *avn = ATH_VAP_NET80211(vap);
    struct ath_node_net80211 *anode;
    int i;

    anode = (struct ath_node_net80211 *)OS_MALLOC(scn->sc_osdev,
                                                  sizeof(struct ath_node_net80211),
                                                  GFP_ATOMIC);
    if (anode == NULL)
        return NULL;
    
    OS_MEMZERO(anode, sizeof(struct ath_node_net80211));
    anode->an_avgbrssi  = ATH_RSSI_DUMMY_MARKER;
    anode->an_avgdrssi  = ATH_RSSI_DUMMY_MARKER;
    anode->an_avgrssi   = ATH_RSSI_DUMMY_MARKER;
    anode->an_avgtxrssi = ATH_RSSI_DUMMY_MARKER;
    anode->an_avgtxrate = ATH_RATE_DUMMY_MARKER;
    anode->an_avgrxrate = ATH_RATE_DUMMY_MARKER;
    anode->an_lasttxrate = ATH_RATE_DUMMY_MARKER;
    anode->an_lastrxrate = ATH_RATE_DUMMY_MARKER;
    for (i = 0; i < ATH_MAX_ANTENNA; ++i) {
        anode->an_avgtxchainrssi[i]    = ATH_RSSI_DUMMY_MARKER;
        anode->an_avgtxchainrssiext[i] = ATH_RSSI_DUMMY_MARKER;
        anode->an_avgbchainrssi[i]      = ATH_RSSI_DUMMY_MARKER;
        anode->an_avgbchainrssiext[i]   = ATH_RSSI_DUMMY_MARKER;
        anode->an_avgdchainrssi[i]      = ATH_RSSI_DUMMY_MARKER;
        anode->an_avgdchainrssiext[i]   = ATH_RSSI_DUMMY_MARKER;
        anode->an_avgchainrssi[i]      = ATH_RSSI_DUMMY_MARKER;
        anode->an_avgchainrssiext[i]   = ATH_RSSI_DUMMY_MARKER;
    }

    /* attach a node in ath_dev module */
    anode->an_sta = scn->sc_ops->alloc_node(scn->sc_dev, avn->av_if_id, anode, tmpnode);
    if (anode->an_sta == NULL) {
        OS_FREE(anode);
        return NULL;
    }

#ifdef ATH_AMSDU
    ath_amsdu_node_attach(scn, anode);
#endif

    anode->an_node.ni_vap = vap;
    return &anode->an_node;
}

static void
ath_force_ppm_enable (void *arg, struct ieee80211_node *ni)
{
    struct ieee80211com          *ic  = arg;
    struct ath_softc_net80211    *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_ops->force_ppm_notify(scn->sc_dev, ATH_FORCE_PPM_ENABLE, ni->ni_macaddr);
}

static void
ath_net80211_node_free(struct ieee80211_node *ni)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    ath_node_t an = ATH_NODE_NET80211(ni)->an_sta;

    //First get rid of ni from scn->sc_keyixmap if created
    //using ni->ni_ucastkey.wk_keyix;    
    if(ni->ni_ucastkey.wk_valid && 
        (ni->ni_ucastkey.wk_keyix != IEEE80211_KEYIX_NONE)
       )
    {
        IEEE80211_KEYMAP_LOCK(scn);

        if (scn->sc_keyixmap[ni->ni_ucastkey.wk_keyix] == ni)
        {
            scn->sc_keyixmap[ni->ni_ucastkey.wk_keyix] = NULL;
        }

        IEEE80211_KEYMAP_UNLOCK(scn);
    }

#ifdef ATH_AMSDU
    ath_amsdu_node_detach(scn, ATH_NODE_NET80211(ni));
#endif

    scn->sc_node_free(ni);
    scn->sc_ops->free_node(scn->sc_dev, an);
}

static void
ath_net80211_node_cleanup(struct ieee80211_node *ni)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_node_net80211 *anode = (struct ath_node_net80211 *)ni;

    /*
     * If AP mode, enable ForcePPM if only one Station is connected, or 
     * disable it otherwise.
     */
    if (ni->ni_vap && ieee80211vap_get_opmode(ni->ni_vap) == IEEE80211_M_HOSTAP) {
        if (ieee80211com_can_enable_force_ppm(ic)) {
            ieee80211_iterate_node(ic, ath_force_ppm_enable, ic);
        }
        else {
            scn->sc_ops->force_ppm_notify(scn->sc_dev, ATH_FORCE_PPM_DISABLE, NULL);
        }
    }

    /* remove the ni from scn->sc_keyixmap if created
     * using ni->ni_ucastkey.wk_keyix.
     * since this node will be removed from the node table after cleanup.
     */
    if(ni->ni_ucastkey.wk_valid && 
        (ni->ni_ucastkey.wk_keyix != IEEE80211_KEYIX_NONE)
       )
    {
        IEEE80211_KEYMAP_LOCK(scn);

        if (scn->sc_keyixmap[ni->ni_ucastkey.wk_keyix] == ni)
        {
            scn->sc_keyixmap[ni->ni_ucastkey.wk_keyix] = NULL;
        }

        IEEE80211_KEYMAP_UNLOCK(scn);
    }

    scn->sc_ops->cleanup_node(scn->sc_dev, anode->an_sta);
#ifdef ATH_SWRETRY
    scn->sc_ops->set_swretrystate(scn->sc_dev, anode->an_sta, AH_FALSE);
    DPRINTF(scn, ATH_DEBUG_SWR, "%s: swr disable for ni %s\n", __func__, ether_sprintf(ni->ni_macaddr));
#endif
    scn->sc_node_cleanup(ni);
}

static int8_t
ath_net80211_node_getrssi(const struct ieee80211_node *ni,int8_t chain, u_int8_t flags )
{
    struct ath_node_net80211 *anode = (struct ath_node_net80211 *)ni;
    int32_t avgrssi;
    int32_t rssi;


    /*
     * When only one frame is received there will be no state in
     * avgrssi so fallback on the value recorded by the 802.11 layer.
     */

    avgrssi = ATH_RSSI_DUMMY_MARKER;

    if (flags & IEEE80211_RSSI_BEACON) {
        if (chain == -1) {
            avgrssi = anode->an_avgbrssi;
        } else {
            if (!(flags & IEEE80211_RSSI_EXTCHAN)) {
                avgrssi = anode->an_avgbchainrssi[chain];
            } else {
                avgrssi = anode->an_avgbchainrssiext[chain];
            }
        }
    } else if (flags & IEEE80211_RSSI_RXDATA) {
        if (chain == -1) {
            avgrssi = anode->an_avgdrssi;
        } else {
            if (!(flags & IEEE80211_RSSI_EXTCHAN)) {
                avgrssi = anode->an_avgdchainrssi[chain];
            } else {
                avgrssi = anode->an_avgdchainrssiext[chain];
            }
        }
    } else if (flags & IEEE80211_RSSI_RX) {
        if (chain == -1) {
            avgrssi = anode->an_avgrssi;
        } else {
            if (!(flags & IEEE80211_RSSI_EXTCHAN)) {
                avgrssi = anode->an_avgchainrssi[chain];
            } else {
                avgrssi = anode->an_avgchainrssiext[chain];
            }
        }
    } else if (flags & IEEE80211_RSSI_TX) {
        if (chain == -1) {
            avgrssi = anode->an_avgtxrssi;
        } else {
            if (!(flags & IEEE80211_RSSI_EXTCHAN)) {
                avgrssi = anode->an_avgtxchainrssi[chain];
            } else {
                avgrssi = anode->an_avgtxchainrssiext[chain];
            }
        }
    }

    if (avgrssi != ATH_RSSI_DUMMY_MARKER)
        rssi = ATH_EP_RND(avgrssi, HAL_RSSI_EP_MULTIPLIER);
    else
        rssi =  -1; 

    return rssi;
}

static u_int32_t
ath_net80211_node_getrate(const struct ieee80211_node *ni, u_int8_t type) 
{
    switch (type) {
    case IEEE80211_RATE_TX:
        return ATH_RATE_OUT(ATH_NODE_NET80211(ni)->an_avgtxrate);
    case IEEE80211_LASTRATE_TX:
        return (ATH_NODE_NET80211(ni)->an_lasttxrate) ;
    case IEEE80211_RATECODE_TX:
        return (ATH_NODE_NET80211(ni)->an_txratecode);
    case IEEE80211_RATE_RX:
        return ATH_RATE_OUT(ATH_NODE_NET80211(ni)->an_avgrxrate);
    default:
        return 0;
    }

}

static INLINE int
ath_net80211_node_get_extradelimwar(ieee80211_node_t n)
{
    struct ieee80211_node *ni = (struct ieee80211_node *)n;
    
    if (ni) return ni->ni_flags & IEEE80211_NODE_EXTRADELIMWAR;

    return 0;
}


static int
ath_net80211_reset_start(struct ieee80211com *ic, bool no_flush)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_ops->reset_start(scn->sc_dev, no_flush, 0, 0);
}

static int
ath_net80211_reset_end(struct ieee80211com *ic, bool no_flush)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_ops->reset_end(scn->sc_dev, no_flush);
}

static int
ath_net80211_reset(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_ops->reset(scn->sc_dev);
}

static void ath_vap_iter_scan_start(void *arg, wlan_if_t vap) 
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(vap->iv_ic);

    if (ieee80211vap_get_opmode(vap) == IEEE80211_M_STA) {
        scn->sc_ops->force_ppm_notify(scn->sc_dev, ATH_FORCE_PPM_SUSPEND, NULL);
    }
}

static void ath_vap_iter_post_set_channel(void *arg, wlan_if_t vap) 
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(vap->iv_ic);
    struct ath_vap_net80211 *avn;

    if (ieee80211_vap_ready_is_set(vap)) {
        /*
         * Always configure the beacon.
         * In ad-hoc, we may be the only peer in the network.
         * In infrastructure, we need to detect beacon miss
         * if the AP goes away while we are scanning.
         */


        avn = ATH_VAP_NET80211(vap);

       /*
         * if current channel is not the vaps bss channel,
         * ignore it.
         */
        if ( ieee80211_get_current_channel(vap->iv_ic) != vap->iv_bsschan) {
            return;
        }

       /*
        * TBD: if multiple vaps are running, only sync the first one.
        * RM should decide which one to synch to.
        */
       if (!scn->sc_syncbeacon) {
            scn->sc_ops->sync_beacon(scn->sc_dev, avn->av_if_id);
       }

        if (ieee80211vap_get_opmode(vap) == IEEE80211_M_IBSS) {
            /*
             * if tsf is 0. we are alone.
             * no need to sync tsf from beacons.
             */
            if (ieee80211_node_get_tsf(ieee80211vap_get_bssnode(vap)) != 0) 
                scn->sc_syncbeacon = 1;
        } else {
          /*
           * if scheduler is active then we don not want to wait for beacon for sync.
           * the tsf is already maintained acoss channel changes and the tsf should be in
           * sync with the AP and waiting for a beacon (syncbeacon == 1) forces us to
           * to keep the chip awake until beacon is received and affects the powersave
           * functionality severely. imagine we are switching channel every 50msec for
           * STA + GO  (AP) operating on different channel with powersave tunred on on both
           * STA and GO. if there is no activity on both STA and GO we will have to wait for 
           * say approximately 25 msec on the STAs BSS  channel after switching channel. without 
           * this fix  the chip needs to  be awake for 25 msec every 100msec (25% of the time).
           */   
          if (!ieee80211_resmgr_oc_scheduler_is_active(vap->iv_ic->ic_resmgr)) {
           scn->sc_syncbeacon = 1;
          }
        }

       /* Notify CWM */
        DPRINTF(scn, ATH_DEBUG_CWM, "%s\n", __func__);

        ath_cwm_up(NULL, vap);

        /* Resume ForcePPM operation as we return to home channel */
        if (ieee80211vap_get_opmode(vap) == IEEE80211_M_STA) {
            scn->sc_ops->force_ppm_notify(scn->sc_dev, ATH_FORCE_PPM_RESUME, NULL);
        }
    }
}

struct ath_iter_vaps_ready_arg {
    u_int8_t num_sta_vaps_ready;
    u_int8_t num_ibss_vaps_ready; 
    u_int8_t num_ap_vaps_ready;
};

static void ath_vap_iter_vaps_ready(void *arg, wlan_if_t vap) 
{
    struct ath_iter_vaps_ready_arg *params = (struct ath_iter_vaps_ready_arg *) arg;  
    if (ieee80211_vap_ready_is_set(vap)) {
        switch(ieee80211vap_get_opmode(vap)) {
        case IEEE80211_M_HOSTAP:
        case IEEE80211_M_BTAMP:
            params->num_ap_vaps_ready++;
            break;

        case IEEE80211_M_IBSS:
            params->num_ibss_vaps_ready++;
            break;

        case IEEE80211_M_STA:
            params->num_sta_vaps_ready++;
            break;

        default:
            break;

        }
    }
}


static int
ath_net80211_set_channel(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ieee80211_channel *chan;
    HAL_CHANNEL hchan;

    /*
     * Convert to a HAL channel description with
     * the flags constrained to reflect the current
     * operating mode.
     */
    chan = ieee80211_get_current_channel(ic);
    hchan.channel = ieee80211_chan2freq(ic, chan);
    hchan.channelFlags = ath_chan2flags(chan);
    KASSERT(hchan.channel != 0,
            ("bogus channel %u/0x%x", hchan.channel, hchan.channelFlags));
    /*
     * if scheduler is active then we don not want to wait for beacon for sync.
     * the tsf is already maintained acoss channel changes and the tsf should be in
     * sync with the AP and waiting for a beacon (syncbeacon == 1) forces us to
     * to keep the chip awake until beacon is received and affects the powersave
     * functionality severely. imagine we are switching channel every 50msec for
     * STA + GO  (AP) operating on different channel with powersave tunred on on both
     * STA and GO. if there is no activity on both STA and GO we will have to wait for 
     * say approximately 25 msec on the STAs BSS  channel after switching. without 
     * this fix  the chip needs to  be awake for 25 msec every 100msec (25% of the time).
     */   
    if (ieee80211_resmgr_oc_scheduler_is_active(ic->ic_resmgr)) {
        scn->sc_syncbeacon = 0;
    }

    /*  
     * Initialize CWM (Channel Width Management) .
     * this needs to be called before calling set channel in to the ath layer.
     * because the mac mode (ht40, ht20) is initialized by the cwm_init
     * and set_channel in ath layer gets the mac mode from the CWM module
     * and passes it down to HAL. if CWM is initialized after set_channel then
     * wrong mac mode is passed down to HAL and can result in TX hang when 
     * switching from a HT40 to HT20 channel (ht40 mac mode is used for HT20 channel).  
     */
    cwm_init(ic);

    /* set h/w channel */
    scn->sc_ops->set_channel(scn->sc_dev, &hchan, 0, 0, false);
    
    /* update max channel power to max regpower of current channel */
    ieee80211com_set_curchanmaxpwr(ic, chan->ic_maxregpower);


    /*
     * If we are returning to our bss channel then mark state
     * so the next recv'd beacon's tsf will be used to sync the
     * beacon timers.  Note that since we only hear beacons in
     * sta/ibss mode this has no effect in other operating modes.
     */
    if (!scn->sc_isscan) {
        if ((ic->ic_opmode == IEEE80211_M_HOSTAP) || (ic->ic_opmode == IEEE80211_M_BTAMP)) {
            struct ath_iter_vaps_ready_arg params; 
            params.num_sta_vaps_ready = params.num_ap_vaps_ready = params.num_ibss_vaps_ready = 0; 
            /* there is atleast one AP VAP active */
            wlan_iterate_vap_list(ic,ath_vap_iter_vaps_ready,(void *) &params);
            if (params.num_ap_vaps_ready) {
                scn->sc_ops->sync_beacon(scn->sc_dev, ATH_IF_ID_ANY);
            }
        } else  {
             wlan_iterate_vap_list(ic,ath_vap_iter_post_set_channel,NULL);
                
        }
    }

    return 0;
}

static void
ath_net80211_newassoc(struct ieee80211_node *ni, int isnew)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_node_net80211 *an = (struct ath_node_net80211 *)ni;
    struct ieee80211vap *vap = ni->ni_vap;


    ath_net80211_rate_node_update(ic, ni, isnew);
    scn->sc_ops->new_assoc(scn->sc_dev, an->an_sta, isnew, ((ni->ni_flags & IEEE80211_NODE_UAPSD)? 1: 0));
    
    /*
     * If AP mode, 
     * a) Setup the keycacheslot for open - required for UAPSD
     *    for other modes hostapd sets the key cache entry
     *    for static WEP case, not setting the key cache entry
     *    since, AP does not know the key index used by station 
     *    in case of multiple WEP key scenario during assoc.
     * b) enable ForcePPM if only one Station is connected, or 
     * disable it otherwise. ForcePPM applies only to 2GHz channels.
     */
    if (ieee80211vap_get_opmode(vap) == IEEE80211_M_HOSTAP) {
        if (isnew && !vap->iv_wps_mode) {
            ni->ni_ath_defkeyindex = IEEE80211_INVAL_DEFKEY;
            if (!RSN_AUTH_IS_WPA(&vap->iv_bss->ni_rsn) &&
                !RSN_AUTH_IS_WPA2(&vap->iv_bss->ni_rsn) &&
                !RSN_AUTH_IS_WAI(&vap->iv_bss->ni_rsn) &&
                !RSN_AUTH_IS_8021X(&vap->iv_bss->ni_rsn)) {
                ath_setup_keycacheslot(ni);
            }
        }
        if (IEEE80211_IS_CHAN_2GHZ(ni->ni_chan)) {
            enum ath_force_ppm_event_t    event = ieee80211com_can_enable_force_ppm(ic) ?
                ATH_FORCE_PPM_ENABLE : ATH_FORCE_PPM_DISABLE;

            scn->sc_ops->force_ppm_notify(scn->sc_dev, event, ni->ni_macaddr);
        }
#ifdef ATH_SWRETRY
		if (scn->sc_ops->have_capability(scn->sc_dev, ATH_CAP_SWRETRY_SUPPORT)) {
            scn->sc_ops->set_swretrystate(scn->sc_dev, ATH_NODE_NET80211(ni)->an_sta, AH_TRUE);
            DPRINTF(scn, ATH_DEBUG_SWR, "%s: swr enable for ni %s\n", __func__, ether_sprintf(ni->ni_macaddr));
        }
#endif
    }
#ifdef ATH_SUPPORT_TxBF
        // initial keycache for txbf.
    if ((ieee80211vap_get_opmode(vap) == IEEE80211_M_IBSS) || (ieee80211vap_get_opmode(vap) == IEEE80211_M_HOSTAP)) {
        if (ni->ni_explicit_compbf || ni->ni_explicit_noncompbf || ni->ni_implicit_bf) {
            struct ieee80211com     *ic = vap->iv_ic;
            
            ieee80211_set_TxBF_keycache(ic,ni);
            ni->ni_bf_update_cv = 1;
            ni->ni_allow_cv_update = 1;
        }
    }
#endif
}

struct ath_iter_new_opmode_arg {
    struct ieee80211vap *vap;
    u_int8_t num_sta_vaps_active;
    u_int8_t num_ibss_vaps_active; 
    u_int8_t num_ap_vaps_active;
    u_int8_t num_ap_sleep_vaps_active; /* ap vaps that can sleep (P2P GO) */
    u_int8_t num_sta_nobeacon_vaps_active; /* sta vap for repeater application */
    u_int8_t num_btamp_vaps_active;
    bool     vap_active;
};


static void ath_vap_iter_new_opmode(void *arg, wlan_if_t vap) 
{
    struct ath_iter_new_opmode_arg *params = (struct ath_iter_new_opmode_arg *) arg;  
    bool vap_active;


        if (vap != params->vap) {
            vap_active = ieee80211_vap_active_is_set(vap);
        } else {
            vap_active = params->vap_active;
        } 
        if (vap_active) {

            DPRINTF(ATH_SOFTC_NET80211(vap->iv_ic), ATH_DEBUG_STATE, "%s: vap %p tmpvap %p opmode %d\n", __func__,
                params->vap, vap, ieee80211vap_get_opmode(vap));

            switch(ieee80211vap_get_opmode(vap)) {
            case IEEE80211_M_HOSTAP:
            case IEEE80211_M_BTAMP:
	        if (ieee80211_vap_cansleep_is_set(vap)) {
                   params->num_ap_sleep_vaps_active++;
                } else {
                   params->num_ap_vaps_active++;
                   if (ieee80211vap_get_opmode(vap) == IEEE80211_M_BTAMP) {
                       params->num_btamp_vaps_active++;
                   }
                }
                break;

            case IEEE80211_M_IBSS:
                params->num_ibss_vaps_active++;
                break;

            case IEEE80211_M_STA:
                if (vap->iv_create_flags & IEEE80211_NO_STABEACONS) {
                    params->num_sta_nobeacon_vaps_active++;
                }
                else {
                    params->num_sta_vaps_active++;
                }
                break;

            default:
                break;

            }
        }
}
/*
 * determine the new opmode whhen one of the vaps
 * changes its state.
 */
static enum ieee80211_opmode
ath_new_opmode(struct ieee80211vap *vap, bool vap_active)
{
    struct ieee80211com *ic = vap->iv_ic;
    enum ieee80211_opmode opmode;
    struct ath_iter_new_opmode_arg params; 

    DPRINTF(ATH_SOFTC_NET80211(ic), ATH_DEBUG_STATE, "%s: active state %d\n", __func__, vap_active);

    params.num_sta_vaps_active = params.num_ibss_vaps_active = 0; 
    params.num_ap_vaps_active = params.num_ap_sleep_vaps_active = 0;
    params.num_sta_nobeacon_vaps_active = 0;
    params.vap = vap;
    params.vap_active = vap_active;
    wlan_iterate_vap_list(ic,ath_vap_iter_new_opmode,(void *) &params);
    /*
     * we cant support all 3 vap types active at the same time.
     */
    ASSERT(!(params.num_ap_vaps_active && params.num_sta_vaps_active && params.num_ibss_vaps_active));
    DPRINTF(ATH_SOFTC_NET80211(ic), ATH_DEBUG_STATE, "%s: ibss %d, ap %d, sta %d sta_nobeacon %d \n", __func__, params.num_ibss_vaps_active, params.num_ap_vaps_active, params.num_sta_vaps_active, params.num_sta_nobeacon_vaps_active);

    if (params.num_ibss_vaps_active) {
        opmode = IEEE80211_M_IBSS;
        return opmode;
    }
    if (params.num_ap_vaps_active) {
        opmode = IEEE80211_M_HOSTAP;
        return opmode;
    }
    if (params.num_sta_nobeacon_vaps_active) {
        opmode = IEEE80211_M_HOSTAP;
        return opmode;
    }
    if (params.num_sta_vaps_active) {
        opmode = IEEE80211_M_STA;
        return opmode;
    }
    if (params.num_ap_sleep_vaps_active) {
        opmode = IEEE80211_M_STA;
        return opmode;
    }

    return (ieee80211vap_get_opmode(vap));
}

#ifdef ATH_BT_COEX
/*
 * determine the bt coex mode when one of the vaps
 * changes its state.
 */
static void
ath_bt_coex_opmode(struct ieee80211vap *vap, bool vap_active)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ath_softc_net80211    *scn = ATH_SOFTC_NET80211(ic);
    struct ath_iter_new_opmode_arg params; 

    DPRINTF(scn, ATH_DEBUG_STATE, "%s: active state %d\n", __func__, vap_active);

    params.num_sta_vaps_active = params.num_ibss_vaps_active = 0; 
    params.num_ap_vaps_active = params.num_ap_sleep_vaps_active = 0;
    params.num_sta_nobeacon_vaps_active = 0;
    params.num_btamp_vaps_active = 0;
    params.vap = vap;
    params.vap_active = vap_active;
    wlan_iterate_vap_list(ic,ath_vap_iter_new_opmode,(void *) &params);
    /*
     * we cant support all 3 vap types active at the same time.
     */
    ASSERT(!(params.num_ap_vaps_active && params.num_sta_vaps_active && params.num_ibss_vaps_active));
    DPRINTF(scn, ATH_DEBUG_BTCOEX, "%s: ibss %d, btamp %d, ap %d, sta %d sta_nobeacon %d \n", __func__, params.num_ibss_vaps_active, 
        params.num_btamp_vaps_active, params.num_ap_vaps_active, params.num_sta_vaps_active, params.num_sta_nobeacon_vaps_active);

    /*
     * IBSS can't be active with other vaps active at the same time. If there is an IBSS vap, set coex opmode to IBSS.
     * STA vap will have higher priority than the BTAMP and HOSTAP vaps.
     * BTAMP vap will be next.
     * HOSTAP will have the lowest priority.
     */
    if (params.num_ibss_vaps_active) {
        ic->ic_bt_coex_opmode = IEEE80211_M_IBSS;
        return;
    }
    if (params.num_sta_vaps_active) {
        ic->ic_bt_coex_opmode = IEEE80211_M_STA;
        return;
    }
    if (params.num_btamp_vaps_active) {
        ic->ic_bt_coex_opmode = IEEE80211_M_BTAMP;
        return;
    }
    if (params.num_ap_vaps_active) {
        ic->ic_bt_coex_opmode = IEEE80211_M_HOSTAP;
        return;
    }
    if (params.num_sta_nobeacon_vaps_active) {
        ic->ic_bt_coex_opmode = IEEE80211_M_HOSTAP;
        return;
    }
    if (params.num_ap_sleep_vaps_active) {
        ic->ic_bt_coex_opmode = IEEE80211_M_HOSTAP;
        return;
    }
}
#endif

struct ath_iter_newstate_arg {
    struct ieee80211vap *vap;
    int flags;
    bool is_ap_vap_running;
    bool is_any_vap_running;
    bool is_any_vap_active;
};

static void ath_vap_iter_newstate(void *arg, wlan_if_t vap) 
{
    struct ath_iter_newstate_arg *params = (struct ath_iter_newstate_arg *) arg;  
    if (params->vap != vap) {
        if (ieee80211_vap_ready_is_set(vap)) {
            if (ieee80211vap_get_opmode(vap) == IEEE80211_M_HOSTAP ||
                ieee80211vap_get_opmode(vap) == IEEE80211_M_BTAMP) {
                params->is_ap_vap_running = true;
            }
            params->is_any_vap_running = true;
        }

        if (ieee80211_vap_active_is_set(vap)) {
            params->is_any_vap_active = true;
        }
    }
}

struct ath_iter_vaptype_params {
    enum ieee80211_opmode   opmode;
    wlan_if_t               vap;
    u_int8_t                vap_active;
};

static void ath_vap_iter_vaptype(void *arg, wlan_if_t vap) 
{
    struct ath_iter_vaptype_params  *params = (struct ath_iter_vaptype_params *) arg;

    if (ieee80211vap_get_opmode(vap) == params->opmode) {
        if (ieee80211_vap_ready_is_set(vap)) {
            params->vap_active ++;
            if (!params->vap)
                params->vap = vap;
        }
    }
}

static void ath_wme_amp_overloadparams(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ath_iter_vaptype_params  params;

    OS_MEMZERO(&params, sizeof(struct ath_iter_vaptype_params));
    params.opmode = IEEE80211_M_BTAMP;
    wlan_iterate_vap_list(ic, ath_vap_iter_vaptype, &params);
    if ((ieee80211vap_get_opmode(vap) == IEEE80211_M_BTAMP && !params.vap_active) ||
        (ieee80211vap_get_opmode(vap) != IEEE80211_M_BTAMP && params.vap_active)) {
        ieee80211_wme_amp_overloadparams_locked(ic);
        //OS_EXEC_INTSAFE(ic->ic_osdev, ieee80211_wme_amp_overloadparams_locked, ic);
    }
}

static void ath_wme_amp_restoreparams(struct ieee80211com *ic)
{
    struct ath_iter_vaptype_params params;

    do {
        OS_MEMZERO(&params, sizeof(struct ath_iter_vaptype_params));
        params.opmode = IEEE80211_M_BTAMP;
        wlan_iterate_vap_list(ic, ath_vap_iter_vaptype, &params);
        if (params.vap_active)
            break;

        OS_MEMZERO(&params, sizeof(struct ath_iter_vaptype_params));
        params.opmode = IEEE80211_M_STA;
        wlan_iterate_vap_list(ic, ath_vap_iter_vaptype, &params);
        if (params.vap_active) {
            ieee80211_wme_updateparams_locked(params.vap);
            //OS_EXEC_INTSAFE(ic->ic_osdev, ieee80211_wme_updateparams_locked, params.vap);
            break;
        }

        OS_MEMZERO(&params, sizeof(struct ath_iter_vaptype_params));
        params.opmode = IEEE80211_M_HOSTAP;
        wlan_iterate_vap_list(ic, ath_vap_iter_vaptype, &params);
        if (params.vap_active) {
            ieee80211_wme_updateparams_locked(params.vap);
            //OS_EXEC_INTSAFE(ic->ic_osdev, ieee80211_wme_updateparams_locked, params.vap);
            break;
        }
    } while (0);
}

static int ath_vap_join(struct ieee80211vap *vap) 
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_vap_net80211 *avn = ATH_VAP_NET80211(vap);
    enum ieee80211_opmode opmode = ieee80211vap_get_opmode(vap);
    struct ieee80211_node *ni = vap->iv_bss;
    struct ath_iter_newstate_arg params;
    u_int flags = 0;


    params.vap = vap;
    params.is_any_vap_active = false;
    wlan_iterate_vap_list(ic,ath_vap_iter_newstate,(void *) &params);


    if (!ieee80211_resmgr_exists(ic) && !params.is_any_vap_active) {
        ath_net80211_pwrsave_set_state(ic,IEEE80211_PWRSAVE_AWAKE);
    }

    if (opmode != IEEE80211_M_STA && opmode != IEEE80211_M_IBSS) {
        DPRINTF(scn, ATH_DEBUG_STATE,
                "%s: remaining join operation is only for STA/IBSS mode\n",
                __func__);
        return 0;
    }

    ath_cwm_join(NULL, vap);
#ifdef ATH_SWRETRY
    scn->sc_ops->set_swretrystate(scn->sc_dev, ATH_NODE_NET80211(ni)->an_sta, AH_FALSE);
    DPRINTF(scn, ATH_DEBUG_SWR, "%s: swr disable for ni %s\n", __func__, ether_sprintf(ni->ni_macaddr));
#endif

    ic->ic_opmode = ath_new_opmode(vap,true);
    scn->sc_ops->switch_opmode(scn->sc_dev, ic->ic_opmode);

#ifdef ATH_BT_COEX
    ath_bt_coex_opmode(vap,true);
#endif

    flags = params.is_any_vap_active? 0: ATH_IF_HW_ON;
    if (IEEE80211_NODE_USE_HT(ni))
        flags |= ATH_IF_HT;
#ifdef ATH_BT_COEX
    {
        u_int32_t bt_event_param = ATH_COEX_WLAN_ASSOC_START;
        scn->sc_ops->bt_coex_event(scn->sc_dev, ATH_COEX_EVENT_WLAN_ASSOC, &bt_event_param);
    }
#endif

    return scn->sc_ops->join(scn->sc_dev, avn->av_if_id, ni->ni_bssid, flags);
}

static int ath_vap_up(struct ieee80211vap *vap) 
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_vap_net80211 *avn = ATH_VAP_NET80211(vap);
    enum ieee80211_opmode opmode = ieee80211vap_get_opmode(vap);
    struct ieee80211_node *ni = vap->iv_bss;
    u_int flags = 0;
    int error = 0;
    int aid = 0;
    bool   is_ap_vap_running=false;
    struct ath_iter_newstate_arg params;

    /*
     * if it is the first AP VAP moving to RUN state then beacon
     * needs to be reconfigured.
     */
    params.vap = vap;
    params.is_ap_vap_running = false;
    params.is_any_vap_active = false;
    wlan_iterate_vap_list(ic,ath_vap_iter_newstate,(void *) &params);
    is_ap_vap_running = params.is_ap_vap_running;

    if (!ieee80211_resmgr_exists(ic) && !params.is_any_vap_active) {
        ath_net80211_pwrsave_set_state(ic,IEEE80211_PWRSAVE_AWAKE);
    }

    ath_cwm_up(NULL, vap);
    ath_wme_amp_overloadparams(vap);

#ifdef ATH_SWRETRY
    scn->sc_ops->set_swretrystate(scn->sc_dev, ATH_NODE_NET80211(ni)->an_sta, AH_FALSE);
    DPRINTF(scn, ATH_DEBUG_SWR, "%s: swr disable for ni %s\n", __func__, ether_sprintf(ni->ni_macaddr));
#endif
    switch (opmode) {
    case IEEE80211_M_HOSTAP:
    case IEEE80211_M_BTAMP:
    case IEEE80211_M_IBSS:
#ifndef ATHR_RNWF
        /* Set default key index for static wep case */
        ni->ni_ath_defkeyindex = IEEE80211_INVAL_DEFKEY;
        if (!RSN_AUTH_IS_WPA(&ni->ni_rsn) &&
            !RSN_AUTH_IS_WPA2(&ni->ni_rsn) &&
            !RSN_AUTH_IS_8021X(&ni->ni_rsn) &&
            !RSN_AUTH_IS_WAI(&ni->ni_rsn) &&
            (vap->iv_def_txkey != IEEE80211_KEYIX_NONE)) {
            ni->ni_ath_defkeyindex = vap->iv_def_txkey;
        }
#endif

        if (ieee80211vap_has_athcap(vap, IEEE80211_ATHC_TURBOP))
            flags |= ATH_IF_DTURBO;
             
        if (IEEE80211_VAP_IS_PRIVACY_ENABLED(vap))
            flags |= ATH_IF_PRIVACY;

        if(!is_ap_vap_running) {
            flags |= ATH_IF_BEACON_ENABLE;

            if (!ieee80211_vap_ready_is_set(vap))
                flags |= ATH_IF_HW_ON;
                
            /*
             * if tsf is 0. we are starting a new ad-hoc network.
             * no need to wait and sync for beacons.
             */
            if (ieee80211_node_get_tsf(ni) != 0) {
                scn->sc_syncbeacon = 1;
                flags |= ATH_IF_BEACON_SYNC;
            } else {
                /*  
                 *  Fix bug 27870.
                 *  When system wakes up from power save mode, we don't 
                 *  know if the peers have left the ad hoc network or not,
                 *  so we have to configure beacon (& ~ATH_IF_BEACON_SYNC)
                 *  and also synchronize to older beacons (sc_syncbeacon
                 *  = 1).
                 *  
                 *  The merge function should take care of it, but during
                 *  resume, sometimes the tsf in rx_status shows the
                 *  synchorized value, so merge routine does not get
                 *  called. It is safer we turn on sc_syncbeacon now.
                 *
                 *  There is no impact to synchronize twice, so just enable
                 *  sc_syncbeacon as long as it is ad hoc mode.
                 */
                if (opmode == IEEE80211_M_IBSS)
                    scn->sc_syncbeacon = 1;
                else
                    scn->sc_syncbeacon = 0;
            }
        }
        break;
            
    case IEEE80211_M_STA:
        aid = ni->ni_associd;
        scn->sc_syncbeacon = 1;
        flags |= ATH_IF_BEACON_SYNC; /* sync with next received beacon */

        if (ieee80211node_has_athflag(ni, IEEE80211_ATHC_TURBOP))
            flags |= ATH_IF_DTURBO;

        if (IEEE80211_NODE_USE_HT(ni))
            flags |= ATH_IF_HT;

#ifdef ATH_SWRETRY
        if (scn->sc_ops->have_capability(scn->sc_dev, ATH_CAP_SWRETRY_SUPPORT)) {
            /* We need to allocate keycache slot here 
             * only if we enable sw retry mechanism
             */
            if (!vap->iv_wps_mode &&!RSN_AUTH_IS_WPA(&vap->iv_bss->ni_rsn) &&
                    !RSN_AUTH_IS_WPA2(&vap->iv_bss->ni_rsn) &&
                    !RSN_AUTH_IS_WAI(&vap->iv_bss->ni_rsn) &&
                    !RSN_AUTH_IS_8021X(&vap->iv_bss->ni_rsn)) {
                ath_setup_keycacheslot(ni);
            }

            /* Enabling SW Retry mechanism only for Infrastructure 
             * mode and only when STA associates to AP and entered into
             * into RUN state.
             */
            scn->sc_ops->set_swretrystate(scn->sc_dev, ATH_NODE_NET80211(ni)->an_sta, AH_TRUE);
            DPRINTF(scn, ATH_DEBUG_SWR, "%s: swr enable for ni %s\n", __func__, ether_sprintf(ni->ni_macaddr));
        }
#endif
#ifdef ATH_BT_COEX
        {
            u_int32_t bt_event_param = ATH_COEX_WLAN_ASSOC_END_SUCCESS;
            scn->sc_ops->bt_coex_event(scn->sc_dev, ATH_COEX_EVENT_WLAN_ASSOC, &bt_event_param);
        }
#endif
        break;

        
    default:
        break;
    }
    /*
     * determine the new op mode depending upon how many
     * vaps are running and switch to the new opmode.
     */
    ic->ic_opmode = ath_new_opmode(vap,true);
    scn->sc_ops->switch_opmode(scn->sc_dev, ic->ic_opmode);

#ifdef ATH_BT_COEX
    ath_bt_coex_opmode(vap,true);
#endif

    error = scn->sc_ops->up(scn->sc_dev, avn->av_if_id, ni->ni_bssid, aid, flags);
    if (opmode == IEEE80211_M_STA && !ieee80211_vap_ready_is_set(vap)) {
        enum ath_force_ppm_event_t    event = ATH_FORCE_PPM_DISABLE;

        if (IEEE80211_IS_CHAN_2GHZ(ni->ni_chan)) {
            event = ATH_FORCE_PPM_ENABLE;
        }
        scn->sc_ops->force_ppm_notify(scn->sc_dev, event, ieee80211_node_get_bssid(vap->iv_bss));
    }
    return 0;
}

int ath_vap_stopping(struct ieee80211vap *vap)
{
    int error = 0;

    struct ieee80211com *ic = vap->iv_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_vap_net80211 *avn = ATH_VAP_NET80211(vap);

#ifdef ATH_SWRETRY
    scn->sc_ops->set_swretrystate(scn->sc_dev, ATH_NODE_NET80211(vap->iv_bss)->an_sta, AH_FALSE);
    DPRINTF(scn, ATH_DEBUG_SWR, "%s: swr disable for ni %s\n", __func__, ether_sprintf(vap->iv_bss->ni_macaddr));
#endif

    error = scn->sc_ops->stopping(scn->sc_dev, avn->av_if_id);
    return error;
}

static int ath_vap_listen(struct ieee80211vap *vap) 
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_vap_net80211 *avn = ATH_VAP_NET80211(vap);
    struct ath_iter_newstate_arg params;

    params.vap = vap;
    params.is_any_vap_active = false;
    wlan_iterate_vap_list(ic,ath_vap_iter_newstate,(void *) &params);

    if (params.is_any_vap_active)
        return 0;

    if (!ieee80211_resmgr_exists(ic)) {
        ath_net80211_pwrsave_set_state(ic,IEEE80211_PWRSAVE_AWAKE);
    }

    ic->ic_opmode = ath_new_opmode(vap,true);
    scn->sc_ops->switch_opmode(scn->sc_dev, ic->ic_opmode);

#ifdef ATH_BT_COEX
    ath_bt_coex_opmode(vap,true);
    scn->sc_ops->bt_coex_event(scn->sc_dev, ATH_COEX_EVENT_WLAN_DISCONNECT, NULL);
#endif

    return scn->sc_ops->listen(scn->sc_dev, avn->av_if_id);
}

int ath_vap_down(struct ieee80211vap *vap) 
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_vap_net80211 *avn = ATH_VAP_NET80211(vap);
    enum ieee80211_opmode opmode = ieee80211vap_get_opmode(vap);
    enum ieee80211_opmode old_ic_opmode = ic->ic_opmode;
    
    u_int flags = 0;
    int error = 0;
    struct ath_iter_newstate_arg params;

    ath_cwm_down(vap);
    ath_wme_amp_restoreparams(ic);

    /*
     * if there is no vap left in active state, turn off hardware
     */
    params.vap = vap;
    params.is_any_vap_active = false;
    wlan_iterate_vap_list(ic,ath_vap_iter_newstate,(void *) &params);

#ifdef ATH_BT_COEX
    {
        u_int32_t bt_event_param = ATH_COEX_WLAN_ASSOC_END_FAIL;
        scn->sc_ops->bt_coex_event(scn->sc_dev, ATH_COEX_EVENT_WLAN_ASSOC, &bt_event_param);
    }
    if (!params.is_any_vap_active) {
        scn->sc_ops->bt_coex_event(scn->sc_dev, ATH_COEX_EVENT_WLAN_DISCONNECT, NULL);
    }
#endif

    flags = params.is_any_vap_active? 0: ATH_IF_HW_OFF;
    error = scn->sc_ops->down(scn->sc_dev, avn->av_if_id, flags);

    /*
     * determine the new op mode depending upon how many
     * vaps are running and switch to the new opmode.
     */
    /*
     * If HW is to be shut off, do not change opmode. Switch_opmode
     * will enable global interrupt.
     */
    if (!(flags & ATH_IF_HW_OFF)) {
        ic->ic_opmode = ath_new_opmode(vap,false);
        scn->sc_ops->switch_opmode(scn->sc_dev, ic->ic_opmode);
    }

#ifdef ATH_BT_COEX
    ath_bt_coex_opmode(vap,false);
#endif

    if (opmode == IEEE80211_M_STA) {
        scn->sc_ops->force_ppm_notify(scn->sc_dev, ATH_FORCE_PPM_DISABLE, NULL);
        scn->sc_syncbeacon = 1;
    }
    /*
     * if we switched opmode to STA then we need to resync beacons.
     */
    if (old_ic_opmode != ic->ic_opmode && ic->ic_opmode == IEEE80211_M_STA) {
        scn->sc_syncbeacon = 1;
    }

    if (!ieee80211_resmgr_exists(ic) && !params.is_any_vap_active) {
        ath_net80211_pwrsave_set_state(ic,IEEE80211_PWRSAVE_FULL_SLEEP);
    }

    return error;
}

static void
ath_net80211_scan_start(struct ieee80211com *ic)
{
    struct ath_softc_net80211    *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_isscan = 1;
    scn->sc_syncbeacon = 0;
    scn->sc_ops->scan_start(scn->sc_dev);
    ath_cwm_scan_start(ic);

    /* Suspend ForcePPM since we are going off-channel */
    wlan_iterate_vap_list(ic,ath_vap_iter_scan_start,NULL);
}

static void
ath_net80211_scan_end(struct ieee80211com *ic)
{
    struct ath_softc_net80211    *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_isscan = 0;
    scn->sc_ops->scan_end(scn->sc_dev);
    ath_cwm_scan_end(ic);
}

static void
ath_net80211_scan_enable_txq(struct ieee80211com *ic)
{
   struct ath_softc_net80211    *scn = ATH_SOFTC_NET80211(ic);
   scn->sc_ops->scan_enable_txq(scn->sc_dev);
}

static void
ath_net80211_led_enter_scan(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->led_scan_start(scn->sc_dev);
}

static void
ath_net80211_led_leave_scan(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->led_scan_end(scn->sc_dev);
}

static void
ath_beacon_update(struct ieee80211_node *ni, int rssi)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ni->ni_ic);

#ifdef ATH_BT_COEX
    /*
     * BT coex module will only look at beacon from one opmode at a time.
     * Beacons from other vap will be ignored.
     * TODO: This might be further optimized if BT coex module can periodically
     * check the RSSI from each vap. So these steps at each beacon receiving can
     * be removed.
     */
    if (ieee80211vap_get_opmode(ni->ni_vap) == ni->ni_ic->ic_bt_coex_opmode) {
        int8_t avgrssi;
        /* The return value of ATH_RSSI_OUT might be ATH_RSSI_DUMMY_MARKER which
         * is 0x127 (more than one byte). Make sure we dont' assign 0x127 to
         * avgrssi which is only one byte.
         */
        avgrssi =  (ATH_RSSI_OUT(ATH_NODE_NET80211(ni)->an_avgbrssi) == ATH_RSSI_DUMMY_MARKER ) ? 
                    0 : ATH_RSSI_OUT(ATH_NODE_NET80211(ni)->an_avgbrssi);
        scn->sc_ops->bt_coex_event(scn->sc_dev, ATH_COEX_EVENT_WLAN_RSSI_UPDATE, (void *)&avgrssi);
    }
#endif

    if (ieee80211vap_get_opmode(ni->ni_vap) != IEEE80211_M_BTAMP) {
    /* Update beacon-related information - rssi and others */
    scn->sc_ops->update_beacon_info(scn->sc_dev,
                                    ATH_NODE_NET80211(ni)->an_avgbrssi);
    
    if (scn->sc_syncbeacon) {
        scn->sc_ops->sync_beacon(scn->sc_dev, (ATH_VAP_NET80211(ni->ni_vap))->av_if_id);
        scn->sc_syncbeacon = 0;
    }
}
}

static int
ath_wmm_update(struct ieee80211com *ic)
{
#define	ATH_EXPONENT_TO_VALUE(v)    ((1<<v)-1)
#define	ATH_TXOP_TO_US(v)           (v<<5)
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    int ac;
    struct wmeParams *wmep;
    HAL_TXQ_INFO qi;

    for (ac = 0; ac < WME_NUM_AC; ac++) {
        int error;

        wmep = ieee80211com_wmm_chanparams(ic, ac);

        qi.tqi_aifs = wmep->wmep_aifsn;
        qi.tqi_cwmin = ATH_EXPONENT_TO_VALUE(wmep->wmep_logcwmin);
        qi.tqi_cwmax = ATH_EXPONENT_TO_VALUE(wmep->wmep_logcwmax);
        qi.tqi_burstTime = ATH_TXOP_TO_US(wmep->wmep_txopLimit);
        /*
         * XXX Set the readyTime appropriately if used.
         */
        qi.tqi_readyTime = 0;

        /* WAR: Improve tx BE-queue performance for USB device */
        ATH_USB_UPDATE_CWIN_FOR_BE(qi);
        error = scn->sc_ops->txq_update(scn->sc_dev, scn->sc_ac2q[ac], &qi);
        ATH_USB_RESTORE_CWIN_FOR_BE(qi);

        if (error != 0)
            return -EIO;

        if (ac == WME_AC_BE)
            scn->sc_ops->txq_update(scn->sc_dev, scn->sc_beacon_qnum, &qi);

        ath_uapsd_txq_update(scn, &qi, ac);
    }
    return 0;
#undef ATH_TXOP_TO_US
#undef ATH_EXPONENT_TO_VALUE
}

void ath_htc_wmm_update_params(struct ieee80211com *ic)
{
    ath_wmm_update(ic);
}

static void
ath_keyprint(const char *tag, u_int ix,
             const HAL_KEYVAL *hk, const u_int8_t mac[IEEE80211_ADDR_LEN])
{
    static const char *ciphers[] = {
        "WEP",
        "AES-OCB",
        "AES-CCM",
        "CKIP",
        "TKIP",
        "CLR",
#if ATH_SUPPORT_WAPI
        "WAPI",
#endif
    };
    int i, n;

    printk("%s: [%02u] %-7s ", tag, ix, ciphers[hk->kv_type]);
    for (i = 0, n = hk->kv_len; i < n; i++)
        printk("%02x", hk->kv_val[i]);
    if (mac) {
        printk(" mac %02x-%02x-%02x-%02x-%02x-%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        printk(" mac 00-00-00-00-00-00");
    }
    if (hk->kv_type == HAL_CIPHER_TKIP) {
        printk(" mic ");
        for (i = 0; i < sizeof(hk->kv_mic); i++)
            printk("%02x", hk->kv_mic[i]);
        printk(" txmic ");
		for (i = 0; i < sizeof(hk->kv_txmic); i++)
			printk("%02x", hk->kv_txmic[i]);
    }
#if ATH_SUPPORT_WAPI
	else if (hk->kv_type == HAL_CIPHER_WAPI) {
        printk(" Mic Key ");
		for (i = 0; i < (hk->kv_len/2); i++)
			printk("%02x", hk->kv_mic[i]);
		for (i = 0; i < (hk->kv_len/2); i++)
			printk("%02x", hk->kv_txmic[i]);
    }
#endif /*ATH_SUPPORT_WAPI*/
    
    printk("\n");
}

/*
 * Allocate one or more key cache slots for a uniacst key.  The
 * key itself is needed only to identify the cipher.  For hardware
 * TKIP with split cipher+MIC keys we allocate two key cache slot
 * pairs so that we can setup separate TX and RX MIC keys.  Note
 * that the MIC key for a TKIP key at slot i is assumed by the
 * hardware to be at slot i+64.  This limits TKIP keys to the first
 * 64 entries.
 */
static int
ath_key_alloc(struct ieee80211vap *vap, struct ieee80211_key *k)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    int opmode;
    u_int keyix;

    if (k->wk_flags & IEEE80211_KEY_GROUP) {
        opmode = ieee80211vap_get_opmode(vap);

        switch (opmode) {
        case IEEE80211_M_STA:
            /*
             * Allocate the key slot for WEP not from 0, but from 4, 
             * if WEP on MBSSID is enabled in STA mode.
             */
            if (!ieee80211_wep_mbssid_cipher_check(k)) {
                if (!((&vap->iv_nw_keys[0] <= k) &&
                      (k < &vap->iv_nw_keys[IEEE80211_WEP_NKID]))) {
                    /* should not happen */
                    DPRINTF(scn, ATH_DEBUG_KEYCACHE,
                            "%s: bogus group key\n", __func__);
                    return IEEE80211_KEYIX_NONE;
                }
                keyix = k - vap->iv_nw_keys;
                return keyix;
            }
            break;

        case IEEE80211_M_IBSS:
            //ASSERT(scn->sc_mcastkey);
            if ((k->wk_flags & IEEE80211_KEY_PERSTA) == 0) {

                /* 
                 * Multicast key search doesn't work on certain hardware, don't use shared key slot(0-3)
                 * for default Tx broadcast key in that case. This affects broadcast traffic reception with AES-CCMP.
                 */
                if ((!scn->sc_mcastkey) &&
					(k->wk_cipher->ic_cipher == IEEE80211_CIPHER_AES_CCM)) {
                    keyix = scn->sc_ops->key_alloc_single(scn->sc_dev);
                    if (keyix == -1)
                        return IEEE80211_KEYIX_NONE;
                    else
                        return keyix;
                }

                if (!((&vap->iv_nw_keys[0] <= k) &&
                      (k < &vap->iv_nw_keys[IEEE80211_WEP_NKID]))) {
                    /* should not happen */
                    DPRINTF(scn, ATH_DEBUG_KEYCACHE,
                            "%s: bogus group key\n", __func__);
                    return IEEE80211_KEYIX_NONE;
                }
                keyix = k - vap->iv_nw_keys;
                return keyix;

            } else if (!(k->wk_flags & IEEE80211_KEY_RECV)) {
                return IEEE80211_KEYIX_NONE;
            }

            if (k->wk_flags & IEEE80211_KEY_PERSTA) {
                if (k->wk_valid) {
                    return k->wk_keyix;
                }
            }
            /* fall thru to allocate a slot for _PERSTA keys */
            break;

        case IEEE80211_M_HOSTAP:
            /*
             * Group key allocation must be handled specially for
             * parts that do not support multicast key cache search
             * functionality.  For those parts the key id must match
             * the h/w key index so lookups find the right key.  On
             * parts w/ the key search facility we install the sender's
             * mac address (with the high bit set) and let the hardware
             * find the key w/o using the key id.  This is preferred as
             * it permits us to support multiple users for adhoc and/or
             * multi-station operation.
             * wep keys need to be allocated in fisrt 4 slots.
             */
            if ((!scn->sc_mcastkey || (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_WEP)) && 
                                                                    (vap->iv_wep_mbssid == 0)) {
                if (!(&vap->iv_nw_keys[0] <= k &&
                      k < &vap->iv_nw_keys[IEEE80211_WEP_NKID])) {
                    /* should not happen */
                    DPRINTF(scn, ATH_DEBUG_KEYCACHE,
                            "%s: bogus group key\n", __func__);
                    return IEEE80211_KEYIX_NONE;
                }
                keyix = k - vap->iv_nw_keys;
                /*
                 * XXX we pre-allocate the global keys so
                 * have no way to check if they've already been allocated.
                 */
                return keyix;
            }
            /* fall thru to allocate a key cache slot */
            break;
            
        default:
            return IEEE80211_KEYIX_NONE;
            break;
        }
    }

    /*
     * We alloc two pair for WAPI when using the h/w to do
     * the SMS4 MIC
     */
#if ATH_SUPPORT_WAPI
    if (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_WAPI)
        return scn->sc_ops->key_alloc_pair(scn->sc_dev);
#endif

    /*
     * We allocate two pair for TKIP when using the h/w to do
     * the MIC.  For everything else, including software crypto,
     * we allocate a single entry.  Note that s/w crypto requires
     * a pass-through slot on the 5211 and 5212.  The 5210 does
     * not support pass-through cache entries and we map all
     * those requests to slot 0.
     *
     * Allocate 1 pair of keys for WEP case. Make sure the key
     * is not a shared-key.
     */
    if (k->wk_flags & IEEE80211_KEY_SWCRYPT) {
        keyix = scn->sc_ops->key_alloc_single(scn->sc_dev);
    } else if ((k->wk_cipher->ic_cipher == IEEE80211_CIPHER_TKIP) &&
               ((k->wk_flags & IEEE80211_KEY_SWMIC) == 0))
    {
        if (scn->sc_splitmic) {
            keyix = scn->sc_ops->key_alloc_2pair(scn->sc_dev);
        } else {
            keyix = scn->sc_ops->key_alloc_pair(scn->sc_dev);
        }
    } else {
        keyix = scn->sc_ops->key_alloc_single(scn->sc_dev);
    }

    if (keyix == -1)
        keyix = IEEE80211_KEYIX_NONE;
    
    // Allocate clear key slot only after the rx key slot is allocated.
    // It will ensure that key cache search for incoming frame will match
    // correct index.
    if (k->wk_flags & IEEE80211_KEY_MFP) {
        /* Allocate a clear key entry for sw encryption of mgmt frames */
        k->wk_clearkeyix = scn->sc_ops->key_alloc_single(scn->sc_dev);
    }
    return keyix;
}

/*
 * Delete an entry in the key cache allocated by ath_key_alloc.
 */
static int
ath_key_delete(struct ieee80211vap *vap, const struct ieee80211_key *k,
               struct ieee80211_node *ninfo)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    const struct ieee80211_cipher *cip = k->wk_cipher;
    struct ieee80211_node *ni;
    u_int keyix = k->wk_keyix;
    int rxkeyoff = 0;
    int freeslot;

    DPRINTF(scn, ATH_DEBUG_KEYCACHE, "%s: delete key %u\n", __func__, keyix);

    /*
     * Don't touch keymap entries for global keys so
     * they are never considered for dynamic allocation.
     */
    freeslot = (keyix >= IEEE80211_WEP_NKID) ? 1 : 0;

    scn->sc_ops->key_delete(scn->sc_dev, keyix, freeslot);
    /*
     * Check the key->node map and flush any ref.
     */
    IEEE80211_KEYMAP_LOCK(scn);
    ni = scn->sc_keyixmap[keyix];
    if (ni != NULL) {
        scn->sc_keyixmap[keyix] = NULL;
        IEEE80211_KEYMAP_UNLOCK(scn);
    } else {
        IEEE80211_KEYMAP_UNLOCK(scn);
    }

    /*
     * Handle split tx/rx keying required for WAPI with h/w MIC.
     */
#if ATH_SUPPORT_WAPI
    if (cip->ic_cipher == IEEE80211_CIPHER_WAPI)
    {
        IEEE80211_KEYMAP_LOCK(scn);

        ni = scn->sc_keyixmap[keyix+64];
        if (ni != NULL) {           /* as above... */
            //scn->sc_keyixmap[keyix+32] = NULL;
            scn->sc_keyixmap[keyix+64] = NULL;
        }

        IEEE80211_KEYMAP_UNLOCK(scn);

        scn->sc_ops->key_delete(scn->sc_dev, keyix+64, freeslot);   /* TX key MIC */
    }
#endif

    /*
     * Handle split tx/rx keying required for TKIP with h/w MIC.
     */
    if ((cip->ic_cipher == IEEE80211_CIPHER_TKIP) &&
        ((k->wk_flags & IEEE80211_KEY_SWMIC) == 0))
    {
        if (scn->sc_splitmic) {
            scn->sc_ops->key_delete(scn->sc_dev, keyix+32, freeslot);   /* RX key */
            IEEE80211_KEYMAP_LOCK(scn);
            ni = scn->sc_keyixmap[keyix+32];
            if (ni != NULL) {           /* as above... */
                scn->sc_keyixmap[keyix+32] = NULL;
            }            
            IEEE80211_KEYMAP_UNLOCK(scn);
            
            scn->sc_ops->key_delete(scn->sc_dev, keyix+32+64, freeslot);   /* RX key MIC */
            ASSERT(scn->sc_keyixmap[keyix+32+64] == NULL);
        }
        
        /* 
         * When splitmic, this key+64 is Tx MIC key. When non-splitmic, this
         * key+64 is Rx/Tx (combined) MIC key.
         */
        scn->sc_ops->key_delete(scn->sc_dev, keyix+64, freeslot);
        ASSERT(scn->sc_keyixmap[keyix+64] == NULL);
    }

    /* Remove the clear key allocated for MFP */
    if(k->wk_flags & IEEE80211_KEY_MFP) {
        scn->sc_ops->key_delete(scn->sc_dev, k->wk_clearkeyix, freeslot);
    }

    /* Remove receive key entry if one exists for static WEP case */
    if (ninfo != NULL) {
        rxkeyoff = ninfo->ni_rxkeyoff;
        if (rxkeyoff != 0) {
            ninfo->ni_rxkeyoff = 0;
            scn->sc_ops->key_delete(scn->sc_dev, keyix+rxkeyoff, freeslot);
            IEEE80211_KEYMAP_LOCK(scn);
            ni = scn->sc_keyixmap[keyix+rxkeyoff];
            if (ni != NULL) {   /* as above... */
                scn->sc_keyixmap[keyix+rxkeyoff] = NULL;
            }
            IEEE80211_KEYMAP_UNLOCK(scn);
        }
    }

    return 1;
}

/*
 * Set a TKIP key into the hardware.  This handles the
 * potential distribution of key state to multiple key
 * cache slots for TKIP.
 * NB: return 1 for success, 0 otherwise.
 */
static int
ath_keyset_tkip(struct ath_softc_net80211 *scn, const struct ieee80211_key *k,
                HAL_KEYVAL *hk, const u_int8_t mac[IEEE80211_ADDR_LEN])
{
#define	IEEE80211_KEY_TXRX	(IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV)

    KASSERT(k->wk_cipher->ic_cipher == IEEE80211_CIPHER_TKIP,
            ("got a non-TKIP key, cipher %u", k->wk_cipher->ic_cipher));

    if ((k->wk_flags & IEEE80211_KEY_TXRX) == IEEE80211_KEY_TXRX) {
        if (!scn->sc_splitmic) {
            /*
             * data key goes at first index,
             * the hal handles the MIC keys at index+64.
             */
            OS_MEMCPY(hk->kv_mic, k->wk_rxmic, sizeof(hk->kv_mic));
            OS_MEMCPY(hk->kv_txmic, k->wk_txmic, sizeof(hk->kv_txmic));
            KEYPRINTF(scn, k->wk_keyix, hk, mac);
            return (scn->sc_ops->key_set(scn->sc_dev, k->wk_keyix, hk, mac));
        } else {
            /*
             * TX key goes at first index, RX key at +32.
             * The hal handles the MIC keys at index+64.
             */
            OS_MEMCPY(hk->kv_mic, k->wk_txmic, sizeof(hk->kv_mic));
            KEYPRINTF(scn, k->wk_keyix, hk, NULL);
            if (!scn->sc_ops->key_set(scn->sc_dev, k->wk_keyix, hk, NULL)) {
		/*
		 * Txmic entry failed. No need to proceed further.
		 */
                return 0;
            }

            OS_MEMCPY(hk->kv_mic, k->wk_rxmic, sizeof(hk->kv_mic));
            KEYPRINTF(scn, k->wk_keyix+32, hk, mac);
            /* XXX delete tx key on failure? */
            return (scn->sc_ops->key_set(scn->sc_dev, k->wk_keyix+32, hk, mac)); 
        }
    } else if (k->wk_flags & IEEE80211_KEY_TXRX) {
        /*
         * TX/RX key goes at first index.
         * The hal handles the MIC keys are index+64.
         */
        if ((!scn->sc_splitmic) &&
            (k->wk_keyix >= IEEE80211_WEP_NKID)) {
                printk("Cannot support setting tx and rx keys individually\n");
                return 0;
        }
        OS_MEMCPY(hk->kv_mic, k->wk_flags & IEEE80211_KEY_XMIT ?
               k->wk_txmic : k->wk_rxmic, sizeof(hk->kv_mic));
        KEYPRINTF(scn, k->wk_keyix, hk, mac);
        return (scn->sc_ops->key_set(scn->sc_dev, k->wk_keyix, hk, mac));
    }
    /* XXX key w/o xmit/recv; need this for compression? */
    return 0;
#undef IEEE80211_KEY_TXRX
}

/*
 * Set the key cache contents for the specified key.  Key cache
 * slot(s) must already have been allocated by ath_key_alloc.
 * NB: return 1 for success, 0 otherwise.
 */
static int
ath_key_set(struct ieee80211vap *vap,
            const struct ieee80211_key *k,
            const u_int8_t peermac[IEEE80211_ADDR_LEN])
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(vap->iv_ic);
    /* Cipher MAP has to be in the same order as ieee80211_cipher_type */
    static const u_int8_t ciphermap[] = {
        HAL_CIPHER_WEP,		/* IEEE80211_CIPHER_WEP     */
        HAL_CIPHER_TKIP,	/* IEEE80211_CIPHER_TKIP    */
        HAL_CIPHER_AES_OCB,	/* IEEE80211_CIPHER_AES_OCB */
        HAL_CIPHER_AES_CCM,	/* IEEE80211_CIPHER_AES_CCM */
#if ATH_SUPPORT_WAPI
        HAL_CIPHER_WAPI,	/* IEEE80211_CIPHER_WAPI    */
#else      
        (u_int8_t) 0xff,	/* IEEE80211_CIPHER_WAPI    */
#endif
        HAL_CIPHER_CKIP,	/* IEEE80211_CIPHER_CKIP    */
        HAL_CIPHER_CLR,		/* IEEE80211_CIPHER_NONE    */
    };
    const struct ieee80211_cipher *cip = k->wk_cipher;
    u_int8_t gmac[IEEE80211_ADDR_LEN];
    const u_int8_t *mac = NULL;
    HAL_KEYVAL hk;
    int opmode, status;

    ASSERT(cip != NULL);
    if (cip == NULL)
        return 0;

    if (k->wk_keyix == IEEE80211_KEYIX_NONE)
        return 0;

    opmode = ieee80211vap_get_opmode(vap);
    memset(&hk, 0, sizeof(hk));
    /*
     * Software crypto uses a "clear key" so non-crypto
     * state kept in the key cache are maintained and
     * so that rx frames have an entry to match.
     */
    if ((k->wk_flags & IEEE80211_KEY_SWCRYPT) == 0) {
        KASSERT(cip->ic_cipher < (sizeof(ciphermap)/sizeof(ciphermap[0])),
                ("invalid cipher type %u", cip->ic_cipher));
        hk.kv_type = ciphermap[cip->ic_cipher];
#if ATH_SUPPORT_WAPI
        if (hk.kv_type == HAL_CIPHER_WAPI)
            OS_MEMCPY(hk.kv_mic, k->wk_txmic, IEEE80211_MICBUF_SIZE);
#endif          
        hk.kv_len  = k->wk_keylen;
        OS_MEMCPY(hk.kv_val, k->wk_key, k->wk_keylen);
    } else
        hk.kv_type = HAL_CIPHER_CLR;

    /*
     *  Strategy:
     *   For _M_STA mc tx, we will not setup a key at all since we never tx mc.
     *       _M_STA mc rx, we will use the keyID.
     *   for _M_IBSS mc tx, we will use the keyID, and no macaddr.
     *   for _M_IBSS mc rx, we will alloc a slot and plumb the mac of the peer node. BUT we 
     *       will plumb a cleartext key so that we can do perSta default key table lookup
     *       in software.
     */
    if (k->wk_flags & IEEE80211_KEY_GROUP) {
        switch (opmode) {
        case IEEE80211_M_STA:
            /* default key:  could be group WPA key or could be static WEP key */
            if (ieee80211_wep_mbssid_mac(vap, k, gmac))
                mac = gmac;
            else
                mac = NULL;
            break;

        case IEEE80211_M_IBSS:
            if (k->wk_flags & IEEE80211_KEY_RECV) {
                if (k->wk_flags & IEEE80211_KEY_PERSTA) {
                    //ASSERT(scn->sc_mcastkey); /* require this for perSta keys */
                    ASSERT(k->wk_keyix >= IEEE80211_WEP_NKID);

                    /*
                     * Group keys on hardware that supports multicast frame
                     * key search use a mac that is the sender's address with
                     * the bit 0 set instead of the app-specified address.
                     * This is a flag to indicate to the HAL that this is 
                     * multicast key. Using any other bits for this flag will
                     * corrupt the MAC address.
                     * XXX: we should use a new parameter called "Multicast" and
                     * pass it to key_set routines instead of embedding this flag.
                     */
                    IEEE80211_ADDR_COPY(gmac, peermac);
                    gmac[0] |= 0x01;
                    mac = gmac;
                } else {
                    /* static wep */
                    mac = NULL;
                }
            } else if (k->wk_flags & IEEE80211_KEY_XMIT) {
                ASSERT(k->wk_keyix < IEEE80211_WEP_NKID);
                mac = NULL;
            } else {
                ASSERT(0);
                status = 0;
                goto done;
            }
            break;
            
        case IEEE80211_M_HOSTAP:
            if (scn->sc_mcastkey) {
                /*
                 * Group keys on hardware that supports multicast frame
                 * key search use a mac that is the sender's address with
                 * the bit 0 set instead of the app-specified address.
                 * This is a flag to indicate to the HAL that this is 
                 * multicast key. Using any other bits for this flag will
                 * corrupt the MAC address.
                 * XXX: we should use a new parameter called "Multicast" and
                 * pass it to key_set routines instead of embedding this flag.
                 */
                IEEE80211_ADDR_COPY(gmac, vap->iv_bss->ni_macaddr);
                gmac[0] |= 0x01;
                mac = gmac;
            } else
                mac = peermac;
            break;
            
        default:
            ASSERT(0);
            break;
        }
    } else {
        /* key mapping key */
        ASSERT(k->wk_keyix >= IEEE80211_WEP_NKID);
        mac = peermac;
    }

#ifdef ATH_SUPPORT_UAPSD
    /*
     * For MACs that support trigger classification using keycache
     * set the bits to indicate trigger-enabled ACs.
     */
    if ((scn->sc_ops->have_capability(scn->sc_dev, ATH_CAP_EDMA_SUPPORT)) &&
       (opmode == IEEE80211_M_HOSTAP))
    {
        struct ieee80211_node *ni;
        u_int8_t ac;
        ni = ieee80211_find_node(&vap->iv_ic->ic_sta, mac);
        if (ni) {
            for (ac = 0; ac < WME_NUM_AC; ac++) {
                hk.kv_apsd |= (ni->ni_uapsd_ac_trigena[ac]) ? (1 << ac) : 0;
            }
            ieee80211_free_node(ni);
        }
    }
#endif


    if (hk.kv_type == HAL_CIPHER_TKIP &&
        (k->wk_flags & IEEE80211_KEY_SWMIC) == 0)
    {
        status = ath_keyset_tkip(scn, k, &hk, mac);
    } else {
        KEYPRINTF(scn, k->wk_keyix, &hk, mac);
        status = (scn->sc_ops->key_set(scn->sc_dev, k->wk_keyix, &hk, mac) != 0);
    }

    if ((mac != NULL) && (cip->ic_cipher == IEEE80211_CIPHER_TKIP)) {
        struct ieee80211com *ic = vap->iv_ic;
        struct ieee80211_node *ni;
        ni = ieee80211_find_node(&ic->ic_sta, mac);
        if (ni) {
            ni->ni_flags |= IEEE80211_NODE_WEPTKIP;
            ath_net80211_rate_node_update(ic, ni, 1);
            ieee80211_free_node(ni);
        }
    }

    if ((k->wk_flags & IEEE80211_KEY_MFP) && (opmode == IEEE80211_M_STA)) {
        /* Create a clear key entry to be used for MFP */
        hk.kv_type = HAL_CIPHER_CLR;
        KEYPRINTF(scn, k->wk_clearkeyix, &hk, mac);
        status = (scn->sc_ops->key_set(scn->sc_dev, k->wk_clearkeyix, &hk, NULL) != 0);
    }

done:
    return status;
}


/*
 * key cache management
 * Key cache slot is allocated for open and wep cases
 */

 /*
 * Allocate a key cache slot to the station so we can
 * setup a mapping from key index to node. The key cache
 * slot is needed for managing antenna state and for
 * compression when stations do not use crypto.  We do
 * it uniliaterally here; if crypto is employed this slot
 * will be reassigned.
 */
 
static void
ath_setup_stationkey(struct ieee80211_node *ni)
{
    struct ieee80211vap *vap = ni->ni_vap;
    u_int16_t keyix;

    keyix = ath_key_alloc(vap, &ni->ni_ucastkey);
    if (keyix == IEEE80211_KEYIX_NONE) {
        /*
         * Key cache is full; we'll fall back to doing
         * the more expensive lookup in software.  Note
         * this also means no h/w compression.
         */
        /* XXX msg+statistic */
        return;
    } else {
        ni->ni_ucastkey.wk_keyix = keyix;
        ni->ni_ucastkey.wk_valid = AH_TRUE;
        /* NB: this will create a pass-thru key entry */
        ath_key_set(vap, &ni->ni_ucastkey, ni->ni_macaddr);

    }
	
    return;
}

/* Setup WEP key for the station if compression is negotiated.
 * When station and AP are using same default key index, use single key
 * cache entry for receive and transmit, else two key cache entries are
 * created. One for receive with MAC address of station and one for transmit
 * with NULL mac address. On receive key cache entry de-compression mask
 * is enabled.
 */

static void
ath_setup_stationwepkey(struct ieee80211_node *ni)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_key *ni_key;
    struct ieee80211_key tmpkey;
    struct ieee80211_key *rcv_key, *xmit_key;
    int    txkeyidx, rxkeyidx = IEEE80211_KEYIX_NONE,i;
    u_int8_t null_macaddr[IEEE80211_ADDR_LEN] = {0,0,0,0,0,0};

    KASSERT(ni->ni_ath_defkeyindex < IEEE80211_WEP_NKID,
            ("got invalid node key index 0x%x", ni->ni_ath_defkeyindex));
    KASSERT(vap->iv_def_txkey < IEEE80211_WEP_NKID,
            ("got invalid vap def key index 0x%x", vap->iv_def_txkey));

    /* Allocate a key slot first */
    if (!ieee80211_crypto_newkey(vap, 
                                 IEEE80211_CIPHER_WEP, 
                                 IEEE80211_KEY_XMIT|IEEE80211_KEY_RECV, 
                                 &ni->ni_ucastkey)) {
        goto error;
    }

    txkeyidx = ni->ni_ucastkey.wk_keyix;
    xmit_key = &vap->iv_nw_keys[vap->iv_def_txkey];

    /* Do we need seperate rx key? */
    if (ni->ni_ath_defkeyindex != vap->iv_def_txkey) {
        ni->ni_ucastkey.wk_keyix = IEEE80211_KEYIX_NONE;
        if (!ieee80211_crypto_newkey(vap, 
                                     IEEE80211_CIPHER_WEP, 
                                     IEEE80211_KEY_XMIT|IEEE80211_KEY_RECV,
                                     &ni->ni_ucastkey)) {
            ni->ni_ucastkey.wk_keyix = txkeyidx;
            ieee80211_crypto_delkey(vap, &ni->ni_ucastkey, ni);
            goto error;
        }
        rxkeyidx = ni->ni_ucastkey.wk_keyix;
        ni->ni_ucastkey.wk_keyix = txkeyidx;

        rcv_key = &vap->iv_nw_keys[ni->ni_ath_defkeyindex];
    } else {
        rcv_key = xmit_key;
        rxkeyidx = txkeyidx;
    }

    /* Remember receive key offset */
    ni->ni_rxkeyoff = rxkeyidx - txkeyidx;

    /* Setup xmit key */
    ni_key = &ni->ni_ucastkey;
    if (rxkeyidx != txkeyidx) {
        ni_key->wk_flags = IEEE80211_KEY_XMIT;
    } else {
        ni_key->wk_flags = IEEE80211_KEY_XMIT|IEEE80211_KEY_RECV;
    }
    ni_key->wk_keylen = xmit_key->wk_keylen;
    for(i=0;i<IEEE80211_TID_SIZE;++i)
        ni_key->wk_keyrsc[i] = xmit_key->wk_keyrsc[i];
    ni_key->wk_keytsc = 0; 
    OS_MEMZERO(ni_key->wk_key, sizeof(ni_key->wk_key));
    OS_MEMCPY(ni_key->wk_key, xmit_key->wk_key, xmit_key->wk_keylen);
    ieee80211_crypto_setkey(vap, &ni->ni_ucastkey, 
                            (rxkeyidx == txkeyidx) ? ni->ni_macaddr : null_macaddr, NULL);

    if (rxkeyidx != txkeyidx) {
        /* Setup recv key */
        ni_key = &tmpkey;
        ni_key->wk_keyix = rxkeyidx;
        ni_key->wk_flags = IEEE80211_KEY_RECV;
        ni_key->wk_keylen = rcv_key->wk_keylen;

        for(i = 0; i < IEEE80211_TID_SIZE; ++i)
            ni_key->wk_keyrsc[i] = rcv_key->wk_keyrsc[i];

        ni_key->wk_keytsc = 0;
        ni_key->wk_cipher = rcv_key->wk_cipher;
        ni_key->wk_private = rcv_key->wk_private;
        OS_MEMZERO(ni_key->wk_key, sizeof(ni_key->wk_key));
        OS_MEMCPY(ni_key->wk_key, rcv_key->wk_key, rcv_key->wk_keylen);
        ieee80211_crypto_setkey(vap, &tmpkey, ni->ni_macaddr, NULL);
    }

    return;

error:
    ni->ni_ath_flags &= ~IEEE80211_NODE_COMP;
    return;
}

/* Create a keycache entry for given node in clearcase as well as static wep.
 * Handle compression state if required.
 * For non clearcase/static wep case, the key is plumbed by hostapd.
 */
static void
ath_setup_keycacheslot(struct ieee80211_node *ni)
{
    struct ieee80211vap *vap = ni->ni_vap;

    if (ni->ni_ucastkey.wk_keyix != IEEE80211_KEYIX_NONE) {
        ieee80211_crypto_delkey(vap, &ni->ni_ucastkey, ni);
    }

    /* Only for clearcase and WEP case */
    if (!IEEE80211_VAP_IS_PRIVACY_ENABLED(vap) ||
        (ni->ni_ath_defkeyindex != IEEE80211_INVAL_DEFKEY)) {

        if (!IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)) {
            KASSERT(ni->ni_ucastkey.wk_keyix  \
                    == IEEE80211_KEYIX_NONE, \
                    ("new node with a ucast key already setup (keyix %u)",\
                     ni->ni_ucastkey.wk_keyix));
            /* 
             * For now, all chips support clr key.
             * // NB: 5210 has no passthru/clr key support
             * if (scn->sc_ops->has_cipher(scn->sc_dev, HAL_CIPHER_CLR))
             *   ath_setup_stationkey(ni);
             */
            ath_setup_stationkey(ni);

        } else {
            ath_setup_stationwepkey(ni);
        }
    }

    return;
}

/*
 * Block/unblock tx+rx processing while a key change is done.
 * We assume the caller serializes key management operations
 * so we only need to worry about synchronization with other
 * uses that originate in the driver.
 */
static void
ath_key_update_begin(struct ieee80211vap *vap)
{
#if ATH_SUPPORT_FLOWMAC_MODULE
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(vap->iv_ic);
#endif

    DPRINTF(ATH_SOFTC_NET80211(vap->iv_ic), ATH_DEBUG_KEYCACHE, "%s:\n",
        __func__);
#if ATH_SUPPORT_FLOWMAC_MODULE
    /*
     * When called from the rx tasklet we cannot use
     * tasklet_disable because it will block waiting
     * for us to complete execution.
     *
     * XXX Using in_softirq is not right since we might
     * be called from other soft irq contexts than
     * ath_rx_tasklet.
     */
    if (scn->sc_ops->netif_stop_queue) {
        scn->sc_ops->netif_stop_queue(scn->sc_dev);
    }
#endif
}

static void
ath_key_update_end(struct ieee80211vap *vap)
{
#if ATH_SUPPORT_FLOWMAC_MODULE
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211((vap->iv_ic));
#endif
    DPRINTF(ATH_SOFTC_NET80211(vap->iv_ic), ATH_DEBUG_KEYCACHE, "%s:\n",
        __func__);
#if ATH_SUPPORT_FLOWMAC_MODULE
    if(scn->sc_ops->netif_wake_queue) {
        scn->sc_ops->netif_wake_queue(scn->sc_dev);
    }
#endif
}

static void
ath_update_ps_mode(struct ieee80211vap *vap)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211((vap->iv_ic));
    struct ath_vap_net80211 *avn = ATH_VAP_NET80211(vap);
    
    /*
     * if vap is not running (or)
     * if we are waiting for syncbeacon
     * nothing to do. 
     */
    if (!ieee80211_vap_ready_is_set(vap) || scn->sc_syncbeacon)
        return;

    /*
     * reconfigure the beacon timers.
     */
    scn->sc_ops->sync_beacon(scn->sc_dev, avn->av_if_id);
}

static void
ath_net80211_pwrsave_set_state(struct ieee80211com *ic, IEEE80211_PWRSAVE_STATE newstate)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    switch (newstate) {
    case IEEE80211_PWRSAVE_AWAKE:
		if (!ieee80211_ic_ignoreDynamicHalt_is_set(ic)) {
	        ath_resume(scn);
		}
        scn->sc_ops->awake(scn->sc_dev);
        break;
    case IEEE80211_PWRSAVE_NETWORK_SLEEP:
        scn->sc_ops->netsleep(scn->sc_dev);
        break;
    case IEEE80211_PWRSAVE_FULL_SLEEP:
		if (!ieee80211_ic_ignoreDynamicHalt_is_set(ic)) {
	        ath_suspend(scn);
		}
		else if(ath_net80211_txq_depth(ic)) {
			break;
		}
        scn->sc_ops->fullsleep(scn->sc_dev);
        break;
    default:
        DPRINTF(scn, ATH_DEBUG_STATE, "%s: wrong power save state %u\n",
                __func__, newstate);
    }
}

#ifdef ENCAP_OFFLOAD
int
ath_get_cipher_map(u_int32_t  cipher, u_int32_t *halKeyType)
{
    if (cipher == IEEE80211_CIPHER_WEP)          *halKeyType = HAL_KEY_TYPE_WEP;
    else if (cipher == IEEE80211_CIPHER_TKIP)    *halKeyType = HAL_KEY_TYPE_TKIP;
    else if (cipher == IEEE80211_CIPHER_AES_OCB) *halKeyType = HAL_KEY_TYPE_AES;
    else if (cipher == IEEE80211_CIPHER_AES_CCM) *halKeyType = HAL_KEY_TYPE_AES;
    else if (cipher == IEEE80211_CIPHER_CKIP)    *halKeyType = HAL_KEY_TYPE_WEP;
    else if (cipher == IEEE80211_CIPHER_NONE)    *halKeyType = HAL_KEY_TYPE_CLEAR;
    else return 1;
    return 0;
}

int
ath_tx_data_prepare(struct ath_softc_net80211 *scn, wbuf_t wbuf, int nextfraglen,
        ieee80211_tx_control_t *txctl)
{
    struct ieee80211_node *ni = wbuf_get_node(wbuf);
    struct ieee80211com *ic = &scn->sc_ic;
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
    struct ieee80211_key *key = NULL;
    struct ether_header *eh;
    int keyix = 0, pktlen = 0;
    u_int8_t keyid = 0;
    HAL_KEY_TYPE keytype = HAL_KEY_TYPE_CLEAR;

#if defined(ATH_SUPPORT_UAPSD) && !defined(MAGPIE_HIF_GMAC)
    if (wbuf_is_uapsd(wbuf))
        return ath_tx_prepare(scn, wbuf, nextfraglen, txctl);
#endif
    
    OS_MEMZERO(txctl, sizeof(ieee80211_tx_control_t));
    eh = (struct ether_header *)wbuf_header(wbuf);

    txctl->ismcast = IEEE80211_IS_MULTICAST(eh->ether_dhost);
    txctl->istxfrag = 0;  /* currently hardcoding to 0, revisit */

    pktlen = wbuf_get_pktlen(wbuf);
    
    /*
     * Set per-packet exemption type
     */
    if ((eh->ether_type != htons(ETHERTYPE_PAE)) && 
        IEEE80211_VAP_IS_PRIVACY_ENABLED(vap))
        wbuf_set_exemption_type(wbuf, WBUF_EXEMPT_NO_EXEMPTION);
    else if ((eh->ether_type == htons(ETHERTYPE_PAE)) && 
             (RSN_AUTH_IS_WPA(rsn) || RSN_AUTH_IS_WPA2(rsn)))
        wbuf_set_exemption_type(wbuf, 
                WBUF_EXEMPT_ON_KEY_MAPPING_KEY_UNAVAILABLE);
    else
        wbuf_set_exemption_type(wbuf, WBUF_EXEMPT_ALWAYS);

    if (IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)) {    /* crypto is on */
        /*
         * Find the key that would be used to encrypt the frame if the 
         * frame were to be encrypted. For unicast frame, search the 
         * matching key in the key mapping table first. If not found,
         * used default key. For multicast frame, only use the default key.
         */
        if (vap->iv_opmode == IEEE80211_M_STA ||
            !IEEE80211_IS_MULTICAST(eh->ether_dhost) ||
            (vap->iv_opmode == IEEE80211_M_WDS &&
            IEEE80211_VAP_IS_STATIC_WDS_ENABLED(vap)))
            /* use unicast key */
            key = &ni->ni_ucastkey;

        if (key && key->wk_valid) { 
            txctl->key_mapping_key = 1;
            keyid = 0;
        }else if (vap->iv_def_txkey != IEEE80211_KEYIX_NONE) {
            key   = &vap->iv_nw_keys[vap->iv_def_txkey];
            key   = key->wk_valid ? key : NULL;
            keyid = key ? (a_uint8_t)vap->iv_def_txkey : 0;
        }else
            key = NULL;
           
        if (key) {
            keyix = (a_int8_t)key->wk_keyix;
            if (ath_get_cipher_map(key->wk_cipher->ic_cipher, &keytype) != 0) {
                DPRINTF(scn,ATH_DEBUG_ANY,"%s: Failed to identify Hal Key Type for ic_cipher %d \n",
                    __func__, key->wk_cipher->ic_cipher);
                return 1;
            }
        }
    } else if ((ni->ni_ucastkey.wk_cipher == &ieee80211_cipher_none) &&
               (keyix != IEEE80211_KEYIX_NONE))
        keyix =  ni->ni_ucastkey.wk_keyix;
    else
        keyix = HAL_TXKEYIX_INVALID;
        
   
    pktlen += IEEE80211_CRC_LEN;

    txctl->frmlen      = pktlen;
    txctl->keyix       = keyix;
    txctl->keyid       = keyid;
    txctl->keytype     = keytype;
    txctl->txpower     = ieee80211_node_get_txpower(ni);
    txctl->nextfraglen = nextfraglen;

    /* 
     * NB: the 802.11 layer marks whether or not we should
     * use short preamble based on the current mode and
     * negotiated parameters.
     */
    if (IEEE80211_IS_SHPREAMBLE_ENABLED(ic) &&
        !IEEE80211_IS_BARKER_ENABLED(ic) &&
        ieee80211node_has_cap(ni, IEEE80211_CAPINFO_SHORT_PREAMBLE))
        txctl->shortPreamble = 1;
    

#if !defined(ATH_SWRETRY) || !defined(ATH_SWRETRY_MODIFY_DSTMASK)
    txctl->flags = HAL_TXDESC_CLRDMASK;    /* XXX needed for crypto errs */
#endif

    txctl->isdata = 1;
    txctl->atype  = HAL_PKT_TYPE_NORMAL;     /* default */

    if (txctl->ismcast)
        txctl->mcast_rate = vap->iv_mcast_rate;

    if (IEEE80211_NODE_USEAMPDU(ni) || ni->ni_flags & IEEE80211_NODE_QOS) {
        int ac = wbuf_get_priority(wbuf);
        txctl->isqosdata = 1;

        /* XXX validate frame priority, remove mask */
        txctl->qnum = scn->sc_ac2q[ac & 0x3];
        if (ieee80211com_wmm_chanparams(ic, ac)->wmep_noackPolicy)
            txctl->flags |= HAL_TXDESC_NOACK;
    }
    else {
        /*
         * Default all non-QoS traffic to the best-effort queue.
         */
        txctl->qnum = scn->sc_ac2q[WME_AC_BE];
        wbuf_set_priority(wbuf, WME_AC_BE);
    }

    /*
    * For HT capable stations, we save tidno for later use.
    * We also override seqno set by upper layer with the one
    * in tx aggregation state.
     */
    if (!txctl->ismcast && ieee80211node_has_flag(ni, IEEE80211_NODE_HT))
        txctl->ht = 1;

    /* Update the uapsd ctl for all frames */
    ath_uapsd_txctl_update(scn, wbuf, txctl);
    /*
    * If we are servicing one or more stations in power-save mode.
     */
    txctl->if_id = (ATH_VAP_NET80211(vap))->av_if_id;
    if (ieee80211vap_has_pssta(vap))
        txctl->ps = 1;

    /*
    * Calculate miscellaneous flags.
     */
    if (wbuf_is_eapol(wbuf)) {
        txctl->use_minrate = 1;
    }

    if (txctl->ismcast) {
        txctl->flags |= HAL_TXDESC_NOACK;   /* no ack on broad/multicast */
    } else if (pktlen > ieee80211vap_get_rtsthreshold(vap)) {
        txctl->flags |= HAL_TXDESC_RTSENA;  /* RTS based on frame length */
    }

    /* Frame to enable SM power save */
    if (wbuf_is_smpsframe(wbuf)) {
        txctl->flags |= HAL_TXDESC_LOWRXCHAIN;
    }

    IEEE80211_HTC_SET_NODE_INDEX(txctl, wbuf);

    return 0;

}
#endif
struct ieee80211_txctl_cap {
	u_int8_t ismgmt;
	u_int8_t ispspoll;
	u_int8_t isbar;
	u_int8_t isdata;
	u_int8_t isqosdata;
	u_int8_t use_minrate;
	u_int8_t atype;
	u_int8_t ac;
	u_int8_t use_ni_minbasicrate;
	u_int8_t use_mgt_rate;
};
enum {
	IEEE80211_MGMT_DEFAULT	= 0,
	IEEE80211_MGMT_BEACON	= 1,
	IEEE80211_MGMT_PROB_RESP = 2,
	IEEE80211_MGMT_PROB_REQ = 3,
	IEEE80211_MGMT_ATIM	= 4,
	IEEE80211_CTL_DEFAULT	= 5,
	IEEE80211_CTL_PSPOLL	= 6,
	IEEE80211_CTL_BAR	= 7,
	IEEE80211_DATA_DEFAULT	= 8,
	IEEE80211_DATA_NODATA	= 9,
	IEEE80211_DATA_QOS	= 10,
	IEEE80211_TYPE4TXCTL_MAX= 11,
};
struct ieee80211_txctl_cap txctl_cap[IEEE80211_TYPE4TXCTL_MAX] = {
		{ 1, 0, 0, 0, 0, 1, HAL_PKT_TYPE_NORMAL, WME_AC_VO, 1, 1}, 	/*default for mgmt*/
		{ 1, 0, 0, 0, 0, 1, HAL_PKT_TYPE_BEACON, WME_AC_VO, 1, 1}, 	/*beacon*/
		{ 1, 0, 0, 0, 0, 1, HAL_PKT_TYPE_PROBE_RESP, WME_AC_VO, 1, 1}, /*prob resp*/
		{ 1, 0, 0, 0, 0, 1, HAL_PKT_TYPE_NORMAL, WME_AC_VO, 0, 1}, 	/*prob req*/
		{ 1, 0, 0, 0, 0, 1, HAL_PKT_TYPE_ATIM, WME_AC_VO, 1, 1},  		/*atim*/
		{ 0, 0, 0, 0, 0, 1, HAL_PKT_TYPE_NORMAL, WME_AC_VO, 0, 0}, 	/*default for ctl*/
		{ 0, 1, 0, 0, 0, 1, HAL_PKT_TYPE_PSPOLL, WME_AC_VO, 0, 0}, 	/*pspoll*/
		{ 0, 0, 1, 0, 0, 1, HAL_PKT_TYPE_NORMAL, WME_AC_VO, 0, 0}, 	/*bar*/
		{ 0, 0, 0, 1, 0, 0, HAL_PKT_TYPE_NORMAL, WME_AC_BE, 0, 1}, 	/*default for data*/
		{ 1, 0, 0, 0, 0, 1, HAL_PKT_TYPE_NORMAL, WME_AC_VO, 1, 1},		/*nodata*/
		{ 0, 0, 0, 1, 1, 0, HAL_PKT_TYPE_NORMAL, WME_AC_BE, 0, 1}, 	/*qos data, the AC to be modified based on pkt's ac*/
};


int
ath_tx_prepare(struct ath_softc_net80211 *scn, wbuf_t wbuf, int nextfraglen,
               ieee80211_tx_control_t *txctl)
{
    struct ieee80211_node *ni = wbuf_get_node(wbuf);
    struct ieee80211com *ic = &scn->sc_ic;
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_frame *wh;
    int keyix, hdrlen, pktlen;
    int type, subtype;
	int txctl_tab_index;
	u_int32_t txctl_flag_mask = 0;
	u_int8_t acnum, use_ni_minbasicrate, use_mgt_rate;
	
    HAL_KEY_TYPE keytype = HAL_KEY_TYPE_CLEAR;
	HAL_KEY_TYPE keytype_table[IEEE80211_CIPHER_MAX] = {
	    HAL_KEY_TYPE_WEP,	/*IEEE80211_CIPHER_WEP*/
    	HAL_KEY_TYPE_TKIP,	/*IEEE80211_CIPHER_TKIP*/
	    HAL_KEY_TYPE_AES,	/*IEEE80211_CIPHER_AES_OCB*/
    	HAL_KEY_TYPE_AES,	/*IEEE80211_CIPHER_AES_CCM*/
#if ATH_SUPPORT_WAPI
	    HAL_KEY_TYPE_WAPI,	/*IEEE80211_CIPHER_WAPI*/
#else
		HAL_KEY_TYPE_CLEAR,
#endif
    	HAL_KEY_TYPE_WEP,	/*IEEE80211_CIPHER_CKIP*/
		HAL_KEY_TYPE_CLEAR,	/*IEEE80211_CIPHER_NONE*/
	};

    OS_MEMZERO(txctl, sizeof(ieee80211_tx_control_t));

    wh = (struct ieee80211_frame *)wbuf_header(wbuf);

    txctl->ismcast = IEEE80211_IS_MULTICAST(wh->i_addr1);
    txctl->istxfrag = (wh->i_fc[1] & IEEE80211_FC1_MORE_FRAG) ||
        (((le16toh(*((u_int16_t *)&(wh->i_seq[0]))) >>
           IEEE80211_SEQ_FRAG_SHIFT) & IEEE80211_SEQ_FRAG_MASK) > 0);
    type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

    /*
     * Packet length must not include any
     * pad bytes; deduct them here.
     */
    hdrlen = ieee80211_anyhdrsize(wh);
    pktlen = wbuf_get_pktlen(wbuf);
    pktlen -= (hdrlen & 3);

    if (IEEE80211_VAP_IS_SAFEMODE_ENABLED(vap)) {
        /* For Safe Mode, the encryption and its encap is already done
           by the upper layer software. Driver do not modify the packet. */
        keyix = HAL_TXKEYIX_INVALID;
    }
    else if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
        const struct ieee80211_cipher *cip;
        struct ieee80211_key *k;

        /*
         * Construct the 802.11 header+trailer for an encrypted
         * frame. The only reason this can fail is because of an
         * unknown or unsupported cipher/key type.
         */

        /* FFXXX: change to handle linked wbufs */
        k = ieee80211_crypto_encap(ni, wbuf);
        if (k == NULL) {
            /*
             * This can happen when the key is yanked after the
             * frame was queued.  Just discard the frame; the
             * 802.11 layer counts failures and provides
             * debugging/diagnostics.
             */
            return -EIO;
        }
        /* update the value of wh since encap can reposition the header */
        wh = (struct ieee80211_frame *)wbuf_header(wbuf);

        /*
         * Adjust the packet + header lengths for the crypto
         * additions and calculate the h/w key index. When
         * a s/w mic is done the frame will have had any mic
         * added to it prior to entry so wbuf pktlen above will
         * account for it. Otherwise we need to add it to the
         * packet length.
         */
        cip = k->wk_cipher;
        hdrlen += cip->ic_header;
#ifndef __CARRIER_PLATFORM__
        pktlen += cip->ic_header + cip->ic_trailer;
#else
        if (wbuf_is_encap_done(wbuf))
            pktlen += cip->ic_trailer;
        else
            pktlen += cip->ic_header + cip->ic_trailer;
#endif

        if ((k->wk_flags & IEEE80211_KEY_SWMIC) == 0) {
            if ( ! txctl->istxfrag)
                pktlen += cip->ic_miclen;
            else {
                if (cip->ic_cipher != IEEE80211_CIPHER_TKIP)
                    pktlen += cip->ic_miclen;
            }
        }
        else{
            pktlen += cip->ic_miclen;
        }
		if (cip->ic_cipher < IEEE80211_CIPHER_MAX) {
			keytype = keytype_table[cip->ic_cipher];
		}
        if (((k->wk_flags & IEEE80211_KEY_MFP) && IEEE80211_IS_MFP_FRAME(wh))) {	
			if (cip->ic_cipher == IEEE80211_CIPHER_TKIP) {
            	DPRINTF(scn, ATH_DEBUG_KEYCACHE, "%s: extend MHDR IE\n", __func__);
	            /* mfp packet len could be extended by MHDR IE */
    	        pktlen += sizeof(struct ieee80211_ccx_mhdr_ie);
			}

            keyix = k->wk_clearkeyix;
            keytype = HAL_KEY_TYPE_CLEAR;
        }
        else 
            keyix = k->wk_keyix;


    }  else if (ni->ni_ucastkey.wk_cipher == &ieee80211_cipher_none) {
        /*
         * Use station key cache slot, if assigned.
         */
        keyix = ni->ni_ucastkey.wk_keyix;
        if (keyix == IEEE80211_KEYIX_NONE)
            keyix = HAL_TXKEYIX_INVALID;
    } else
        keyix = HAL_TXKEYIX_INVALID;

    pktlen += IEEE80211_CRC_LEN;

    txctl->frmlen = pktlen;
    txctl->keyix = keyix;
    txctl->keytype = keytype;
    txctl->txpower = ieee80211_node_get_txpower(ni);
    txctl->nextfraglen = nextfraglen;
#ifdef USE_LEGACY_HAL
    txctl->hdrlen = hdrlen;
#endif
#if ATH_SUPPORT_IQUE
    txctl->tidno = wbuf_get_tid(wbuf);
#endif
    /*
     * NB: the 802.11 layer marks whether or not we should
     * use short preamble based on the current mode and
     * negotiated parameters.
     */
    if (IEEE80211_IS_SHPREAMBLE_ENABLED(ic) &&
        !IEEE80211_IS_BARKER_ENABLED(ic) &&
        ieee80211node_has_cap(ni, IEEE80211_CAPINFO_SHORT_PREAMBLE)) {
        txctl->shortPreamble = 1;
    }

#if !defined(ATH_SWRETRY) || !defined(ATH_SWRETRY_MODIFY_DSTMASK)
    txctl->flags = HAL_TXDESC_CLRDMASK;    /* XXX needed for crypto errs */
#endif

    /*
     * Calculate Atheros packet type from IEEE80211
     * packet header and select h/w transmit queue.
     */
	if (type == IEEE80211_FC0_TYPE_MGT) {
		if (subtype == IEEE80211_FC0_SUBTYPE_BEACON) {
			txctl_tab_index = IEEE80211_MGMT_BEACON;
		} else if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP) {
			txctl_tab_index = IEEE80211_MGMT_PROB_RESP;
		} else if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_REQ) {
			txctl_tab_index = IEEE80211_MGMT_PROB_REQ;
		} else if (subtype == IEEE80211_FC0_SUBTYPE_ATIM) {
			txctl_tab_index = IEEE80211_MGMT_ATIM;
		} else {
			txctl_tab_index = IEEE80211_MGMT_DEFAULT;
		}
	} else if (type == IEEE80211_FC0_TYPE_CTL) {
		if (subtype == IEEE80211_FC0_SUBTYPE_PS_POLL) {
			txctl_tab_index = IEEE80211_CTL_PSPOLL;
		} else if (subtype == IEEE80211_FC0_SUBTYPE_BAR) {
			txctl_tab_index = IEEE80211_CTL_BAR;
		} else {
			txctl_tab_index = IEEE80211_CTL_DEFAULT;
		}
	} else if (type == IEEE80211_FC0_TYPE_DATA) {
		if (subtype == IEEE80211_FC0_SUBTYPE_NODATA) {
			txctl_tab_index = IEEE80211_DATA_NODATA;
		} else if (subtype & IEEE80211_FC0_SUBTYPE_QOS) {
			txctl_tab_index = IEEE80211_DATA_QOS;
		} else {
			txctl_tab_index = IEEE80211_DATA_DEFAULT;
		}
	} else {
        printk("bogus frame type 0x%x (%s)\n",
               wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK, __func__);
        /* XXX statistic */
        return -EIO;
	}
	txctl->ismgmt = txctl_cap[txctl_tab_index].ismgmt;
	txctl->ispspoll = txctl_cap[txctl_tab_index].ispspoll;
	txctl->isbar = txctl_cap[txctl_tab_index].isbar;
	txctl->isdata = txctl_cap[txctl_tab_index].isdata;
	txctl->isqosdata = txctl_cap[txctl_tab_index].isqosdata;
	txctl->use_minrate = txctl_cap[txctl_tab_index].use_minrate;
	txctl->atype = txctl_cap[txctl_tab_index].atype;
	acnum = txctl_cap[txctl_tab_index].ac;
	use_ni_minbasicrate = txctl_cap[txctl_tab_index].use_ni_minbasicrate;
	use_mgt_rate = txctl_cap[txctl_tab_index].use_mgt_rate;
	
	/* set Tx delayed report indicator */
#ifdef ATH_SUPPORT_TxBF
    if ((type == IEEE80211_FC0_TYPE_MGT)&&(subtype == IEEE80211_FC0_SUBTYPE_ACTION)){
        u_int8_t *v_cv_data = (u_int8_t *)(wbuf_header(wbuf) + sizeof(struct ieee80211_frame));
        
        if ((*(v_cv_data+1) == IEEE80211_ACTION_HT_COMP_BF)
            || (*(v_cv_data+1) == IEEE80211_ACTION_HT_NONCOMP_BF))
        {
            txctl->isdelayrpt = 1;
        }
    }
#endif

	/*
	 * Update some txctl fields
	 */
	if (type == IEEE80211_FC0_TYPE_DATA && subtype != IEEE80211_FC0_SUBTYPE_NODATA) {
        if (wbuf_is_eapol(wbuf)) {
            txctl->use_minrate = 1;
		}
        if (txctl->ismcast) {
            txctl->mcast_rate = vap->iv_mcast_rate;
		}
        if (subtype & IEEE80211_FC0_SUBTYPE_QOS) {
            /* XXX validate frame priority, remove mask */
            acnum = wbuf_get_priority(wbuf) & 0x03;
            
            if (ieee80211com_wmm_chanparams(ic, acnum)->wmep_noackPolicy)
                txctl_flag_mask |= HAL_TXDESC_NOACK;

#ifdef ATH_SUPPORT_TxBF
            /* Qos frame with Order bit set indicates an HTC frame */
            if (wh->i_fc[1] & IEEE80211_FC1_ORDER) {
                int is4addr;
                u_int8_t *htc;
                u_int8_t  *tmpdata;

                is4addr = ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) ==
                            IEEE80211_FC1_DIR_DSTODS) ? 1 : 0;
                if (!is4addr) {
                    htc = ((struct ieee80211_qosframe_htc *)wh)->i_htc;
        		} else {
                    htc= ((struct ieee80211_qosframe_htc_addr4 *)wh)->i_htc;
                }
      
                tmpdata=(u_int8_t *) wh;
                /* This is a sounding frame */
                if ((htc[2] == IEEE80211_HTC2_CSI_COMP_BF) ||   
                    (htc[2] == IEEE80211_HTC2_CSI_NONCOMP_BF) ||
                    ((htc[2] & IEEE80211_HTC2_CalPos)==3))
                {
                    //printk("==>%s,txctl flag before attach sounding%x,\n",__func__,txctl->flags);
                    if (ic->ic_txbf.tx_staggered_sounding &&
                        ni->ni_txbf.rx_staggered_sounding)
                    {
                        //txctl->flags |= HAL_TXDESC_STAG_SOUND;
                        txctl_flag_mask|=(HAL_TXDESC_STAG_SOUND<<HAL_TXDESC_TXBF_SOUND_S);
                    } else {
                        txctl_flag_mask |= (HAL_TXDESC_SOUND<<HAL_TXDESC_TXBF_SOUND_S);
                    }
                    txctl_flag_mask |= (ni->ni_txbf.channel_estimation_cap<<HAL_TXDESC_CEC_S);
                    //printk("==>%s,txctl flag %x,tx staggered sounding %x, rx staggered sounding %x\n"
                    //    ,__func__,txctl->flags,ic->ic_txbf.tx_staggered_sounding,ni->ni_txbf.rx_staggered_sounding);
                }

                if ((htc[2] & IEEE80211_HTC2_CalPos)!=0)    // this is a calibration frame
                {
                     txctl_flag_mask|=HAL_TXDESC_CAL;
                }    
            }
#endif

        } else {
            /*
             * Default all non-QoS traffic to the best-effort queue.
             */
            wbuf_set_priority(wbuf, WME_AC_BE);
        }

        ath_uapsd_txctl_update(scn, wbuf, txctl);

        txctl_flag_mask |=
        	   (ieee80211com_has_htcap(ic, IEEE80211_HTCAP_C_ADVCODING) &&
               (ni->ni_htcap & IEEE80211_HTCAP_C_ADVCODING)) ?
               HAL_TXDESC_LDPC : 0;
                            
        /*
         * For HT capable stations, we save tidno for later use.
         * We also override seqno set by upper layer with the one
         * in tx aggregation state.
         */
        if (!txctl->ismcast && ieee80211node_has_flag(ni, IEEE80211_NODE_HT))
            txctl->ht = 1;
	}
	/*
	 * Set min rate and qnum in txctl based on acnum
	 */
	if (txctl->use_minrate) {
		if (use_ni_minbasicrate) {
            /*
             * Send out all mangement frames except Probe request
             * at minimum rate set by AP.
             */
            if (vap->iv_opmode == IEEE80211_M_STA &&
                (ni->ni_minbasicrate != 0)) {
                txctl->min_rate = ni->ni_minbasicrate;
            }
		}
        
        /*
         * if management rate is set, then use it.
         */
        if (use_mgt_rate) {
			if (vap->iv_mgt_rate) {
    	        txctl->min_rate = vap->iv_mgt_rate;
        	}
		}
	}
    txctl->qnum = scn->sc_ac2q[acnum];
     /* fix for 103867 - get the mgmt tid for NULL data*/
     if(type == IEEE80211_FC0_TYPE_DATA && subtype == IEEE80211_FC0_SUBTYPE_NODATA) {
         printk("Should overwrite TID here for NULL data\n");
         txctl->tidno = WME_AC_TO_TID(acnum);
         wbuf_set_priority(wbuf, acnum);
         wbuf_set_tid(wbuf, txctl->tidno);
     }
    /* Update the uapsd ctl for all frames */
    ath_uapsd_txctl_update(scn, wbuf, txctl);


    /*
     * If we are servicing one or more stations in power-save mode.
     */
    txctl->if_id = (ATH_VAP_NET80211(vap))->av_if_id;
    if (ieee80211vap_has_pssta(vap))
        txctl->ps = 1;
    
    /*
     * Calculate miscellaneous flags.
     */
    if (txctl->ismcast) {
        txctl_flag_mask |= HAL_TXDESC_NOACK;	/* no ack on broad/multicast */
    } else if (pktlen > ieee80211vap_get_rtsthreshold(vap)) { 
            txctl_flag_mask |= HAL_TXDESC_RTSENA;	/* RTS based on frame length */
    }

    /* Frame to enable SM power save */
    if (wbuf_is_smpsframe(wbuf)) {
        txctl_flag_mask |= HAL_TXDESC_LOWRXCHAIN;
    }

	/*
	 * Update txctl->flags based on the flag mask
	 */
	txctl->flags |= txctl_flag_mask;
    IEEE80211_HTC_SET_NODE_INDEX(txctl, wbuf);

    return 0;
}



/*
 * The function to send a frame (i.e., hardstart). The wbuf should already be
 * associated with the actual frame, and have a valid node instance.
 */

int
ath_tx_send(wbuf_t wbuf)
{
    struct ieee80211_node *ni = wbuf_get_node(wbuf);
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    wbuf_t next_wbuf;

#if defined(ATH_SUPPORT_DFS)
    /*
     * If we detect radar on the current channel, stop sending data
     * packets. There is a DFS requirment that the AP should stop 
     * sending data packet within 200 ms of radar detection
     */

    if (ic->ic_curchan->ic_flags & IEEE80211_CHAN_RADAR) {
        DPRINTF(scn, ATH_DEBUG_STATE, "%s: RADAR FOUND ON CHANNEL\n", __func__);
        goto bad;
    }
#endif

    /*
     * XXX TODO: Fast frame here
     */

    ath_uapsd_pwrsave_check(wbuf, ni);

#ifdef ATH_AMSDU
    /* Check whether AMSDU is supported in this BlockAck agreement */
    if (IEEE80211_NODE_USEAMPDU(ni) &&
        scn->sc_ops->get_amsdusupported(scn->sc_dev,
                                        ATH_NODE_NET80211(ni)->an_sta,
                                        wbuf_get_tid(wbuf)))
    {
        wbuf = ath_amsdu_send(wbuf);
        if (wbuf == NULL)
            return 0;
    }
#endif

    /*
     * Encapsulate the packet for transmission
     */
#if defined(ENCAP_OFFLOAD) && defined(ATH_SUPPORT_UAPSD) && !defined(MAGPIE_HIF_GMAC)
    if (wbuf_is_uapsd(wbuf))
        wbuf = ieee80211_encap_force(ni, wbuf);
    else
#endif
    wbuf = ieee80211_encap(ni, wbuf);
    if (wbuf == NULL) {
        goto bad;
    }

    /*
     * If node is HT capable, then send out ADDBA if
     * we haven't done so.
     *
     * XXX: send ADDBA here to avoid re-entrance of other
     * tx functions.
     */
    if (IEEE80211_NODE_USEAMPDU(ni) &&
        ic->ic_addba_mode == ADDBA_MODE_AUTO) {
        u_int8_t tidno = wbuf_get_tid(wbuf);
        struct ieee80211_action_mgt_args actionargs;

        if (
#ifdef ATH_SUPPORT_UAPSD
           (!IEEE80211_NODE_AC_UAPSD_ENABLED(ni, TID_TO_WME_AC(tidno))) &&
#endif
           (scn->sc_ops->check_aggr(scn->sc_dev, ATH_NODE_NET80211(ni)->an_sta, tidno)) &&
           /* don't allow EAPOL frame to cause addba to avoid auth timeouts */
           !wbuf_is_eapol(wbuf))
        {
            /* Send ADDBA request */
            actionargs.category = IEEE80211_ACTION_CAT_BA;
            actionargs.action   = IEEE80211_ACTION_BA_ADDBA_REQUEST;
            actionargs.arg1     = tidno;
            actionargs.arg2     = WME_MAX_BA;
            actionargs.arg3     = 0;

            ieee80211_send_action(ni, &actionargs, NULL);
        }
    }
    
    /* send down each fragment */
    while (wbuf != NULL) {
        int nextfraglen = 0;
        int error = 0;
        ATH_DEFINE_TXCTL(txctl, wbuf);
        HTC_WBUF_TX_DELCARE

        next_wbuf = wbuf_next(wbuf);
        if (next_wbuf != NULL)
            nextfraglen = wbuf_get_pktlen(next_wbuf);

#ifdef ENCAP_OFFLOAD
        if (ath_tx_data_prepare(scn, wbuf, nextfraglen, txctl) != 0)
            goto bad;
#else
        /* prepare this frame */
        if (ath_tx_prepare(scn, wbuf, nextfraglen, txctl) != 0)
            goto bad;
#endif
        /* send this frame to hardware */
        txctl->an = (ATH_NODE_NET80211(ni))->an_sta;

#if ATH_DEBUG
        /* For testing purpose, set the RTS/CTS flag according to global setting */
        if (!txctl->ismcast) {
            if (ath_rtscts_enable == 2)
                    txctl->flags |= HAL_TXDESC_RTSENA;
            else if (ath_rtscts_enable == 1)
                    txctl->flags |= HAL_TXDESC_CTSENA;
        }
#endif

        HTC_WBUF_TX_DATA_PREPARE(ic, scn);

        if (error == 0) {
            if (scn->sc_ops->tx(scn->sc_dev, wbuf, txctl) != 0)
                goto bad;
            else {
                HTC_WBUF_TX_DATA_COMPLETE_STATUS(ic);
            }
        }

        wbuf = next_wbuf;
    }
    
    return 0;

bad:
    /* drop rest of the un-sent fragments */
    while (wbuf != NULL) {
        next_wbuf = wbuf_next(wbuf);
        IEEE80211_TX_COMPLETE_WITH_ERROR(wbuf);

        wbuf = next_wbuf;
    }
    
    return -EIO;
}


/*
 * The function to send a management frame. The wbuf should already
 * have a valid node instance.
 */
int
ath_tx_mgt_send(struct ieee80211com *ic, wbuf_t wbuf)
{
    struct ieee80211_node *ni = wbuf_get_node(wbuf);
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc  *sc  = ATH_DEV_TO_SC(scn->sc_dev);
    int error = 0;
  #ifdef ATH_SUPPORT_HTC
    struct ath_usb_p2p_action_queue *p2p_action_wbuf = NULL;
  #endif
    ATH_DEFINE_TXCTL(txctl, wbuf);

    /* Just bypass fragmentation and fast frame. */
    error = ath_tx_prepare(scn, wbuf, 0, txctl);
    if (!error) {
        HTC_WBUF_TX_DELCARE

        /* send this frame to hardware */
        txctl->an = (ATH_NODE_NET80211(ni))->an_sta;

        HTC_WBUF_TX_MGT_PREPARE(ic, scn, txctl);
        HTC_WBUF_TX_MGT_COMPLETE_STATUS(ic);

      #ifdef ATH_SUPPORT_HTC
        if (ni) {
            HTC_WBUF_TX_MGT_P2P_PREPARE(scn, ni, wbuf, p2p_action_wbuf);
            HTC_WBUF_TX_MGT_P2P_INQUEUE(scn, p2p_action_wbuf);
        }
      #endif

        sc->sc_stats.ast_tx_mgmt++;
        error = scn->sc_ops->tx(scn->sc_dev, wbuf, txctl);
        if (!error) {
            return 0;
        } else {
          DPRINTF(scn,ATH_DEBUG_ANY,"%s: send mgt frame failed \n",__func__);
          #ifdef ATH_SUPPORT_HTC
            HTC_WBUF_TX_MGT_P2P_DEQUEUE(scn, p2p_action_wbuf);
          #endif
            HTC_WBUF_TX_MGT_ERROR_STATUS(ic);
        }
    }

    /* fall thru... */
    IEEE80211_TX_COMPLETE_WITH_ERROR(wbuf);
    return error;
}

static u_int32_t
ath_net80211_txq_depth(struct ieee80211com *ic)
{
    int ac, qdepth = 0;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    
    for (ac = WME_AC_BE; ac <= WME_AC_VO; ac++) {
        qdepth += scn->sc_ops->txq_depth(scn->sc_dev, scn->sc_ac2q[ac]);
    }
    return qdepth;
}

static u_int32_t
ath_net80211_txq_depth_ac(struct ieee80211com *ic,int ac)
{
    int qdepth = 0;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    qdepth = scn->sc_ops->txq_depth(scn->sc_dev, scn->sc_ac2q[ac]);
    return qdepth;
}

static void
ath_net80211_tx_complete(wbuf_t wbuf, ieee80211_tx_status_t *tx_status)
{
    struct ieee80211_tx_status ts;


    ts.ts_flags =
        ((tx_status->flags & ATH_TX_ERROR) ? IEEE80211_TX_ERROR : 0) |
        ((tx_status->flags & ATH_TX_XRETRY) ? IEEE80211_TX_XRETRY : 0);
    ts.ts_retries = tx_status->retries;

#ifdef  ATH_SUPPORT_TxBF
    ts.ts_txbfstatus = tx_status->txbf_status;
#endif
    ath_update_txbf_tx_status(ts, tx_status);
#if ATH_SUPPORT_FLOWMAC_MODULE
    ts.ts_flowmac_flags |= IEEE80211_TX_FLOWMAC_DONE;
#endif
#ifdef  ATH_SUPPORT_TxBF
    ts.ts_txbfstatus = tx_status->txbf_status;
#endif
    
    ieee80211_complete_wbuf(wbuf, &ts);

}

static void
ath_net80211_tx_status(ieee80211_node_t node, ieee80211_tx_stat_t *txstatus)
{
    struct ieee80211_node *ni = (struct ieee80211_node *)node;
	int i;

    /* Incase of udp downlink only traffic, 
     * reload the ni_inact every time when a 
     * frame is successfully acked by station. 
     */
    ni->ni_inact = ni->ni_inact_reload;

    ATH_RSSI_LPF(ATH_NODE_NET80211(ni)->an_avgtxrssi, txstatus->rssi);
	
    ATH_NODE_NET80211(ni)->an_lasttxrate = txstatus->rateKbps;
    if (txstatus->rateKbps) {
        ATH_RATE_LPF(ATH_NODE_NET80211(ni)->an_avgtxrate, txstatus->rateKbps);
    }
    ATH_NODE_NET80211(ni)->an_txratecode = txstatus->ratecode;
    
    if (txstatus->flags & ATH_TX_CHAIN_RSSI_VALID) {
        for(i=0;i<ATH_MAX_ANTENNA;++i) {
            ATH_RSSI_LPF(ATH_NODE_NET80211(ni)->an_avgtxchainrssi[i], txstatus->rssictl[i]);
            ATH_RSSI_LPF(ATH_NODE_NET80211(ni)->an_avgtxchainrssiext[i], txstatus->rssiextn[i]);
        }
    }
}

static void
ath_net80211_updateslot(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    int slottime;

    slottime = (IEEE80211_IS_SHSLOT_ENABLED(ic)) ?
        HAL_SLOT_TIME_9 : HAL_SLOT_TIME_20;

    if (IEEE80211_IS_CHAN_HALF(ic->ic_curchan))
        slottime = HAL_SLOT_TIME_13;
    if (IEEE80211_IS_CHAN_QUARTER(ic->ic_curchan))
        slottime = HAL_SLOT_TIME_21;

    scn->sc_ops->set_slottime(scn->sc_dev, slottime);
}

static void
ath_net80211_update_protmode(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    PROT_MODE mode = PROT_M_NONE;

    if (IEEE80211_IS_PROTECTION_ENABLED(ic)) {
        if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
            mode = PROT_M_RTSCTS;
        else if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
            mode = PROT_M_CTSONLY;
    }
    scn->sc_ops->set_protmode(scn->sc_dev, mode);
}

static void
ath_net80211_set_ampduparams(struct ieee80211_node *ni)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_ops->set_ampdu_params(scn->sc_dev,
                                 ATH_NODE_NET80211(ni)->an_sta,
                                 ieee80211_node_get_maxampdu(ni),
                                 ni->ni_mpdudensity);
}

static void
ath_net80211_set_weptkip_rxdelim(struct ieee80211_node *ni, u_int8_t rxdelim)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_ops->set_weptkip_rxdelim(scn->sc_dev,
                                 ATH_NODE_NET80211(ni)->an_sta,
                                 rxdelim);
}


static void
ath_net80211_addba_requestsetup(struct ieee80211_node *ni,
                                u_int8_t tidno,
                                struct ieee80211_ba_parameterset *baparamset,
                                u_int16_t *batimeout,
                                struct ieee80211_ba_seqctrl *basequencectrl,
                                u_int16_t buffersize
                                )
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->addba_request_setup(scn->sc_dev, ATH_NODE_NET80211(ni)->an_sta,
                                     tidno, baparamset, batimeout, basequencectrl,
                                     buffersize);
}

static void
ath_net80211_addba_responsesetup(struct ieee80211_node *ni,
                                 u_int8_t tidno,
                                 u_int8_t *dialogtoken, u_int16_t *statuscode,
                                 struct ieee80211_ba_parameterset *baparamset,
                                 u_int16_t *batimeout
                                 )
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->addba_response_setup(scn->sc_dev, ATH_NODE_NET80211(ni)->an_sta,
                                      tidno, dialogtoken, statuscode,
                                      baparamset, batimeout);
}

static int
ath_net80211_addba_requestprocess(struct ieee80211_node *ni,
                                  u_int8_t dialogtoken,
                                  struct ieee80211_ba_parameterset *baparamset,
                                  u_int16_t batimeout,
                                  struct ieee80211_ba_seqctrl basequencectrl
                                  )
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_ops->addba_request_process(scn->sc_dev, ATH_NODE_NET80211(ni)->an_sta,
                                              dialogtoken, baparamset, batimeout,
                                              basequencectrl);
}

static void
ath_net80211_addba_responseprocess(struct ieee80211_node *ni,
                                   u_int16_t statuscode,
                                   struct ieee80211_ba_parameterset *baparamset,
                                   u_int16_t batimeout
                                   )
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->addba_response_process(scn->sc_dev,
                                        ATH_NODE_NET80211(ni)->an_sta,
                                        statuscode, baparamset, batimeout);
}

static void
ath_net80211_addba_clear(struct ieee80211_node *ni)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->addba_clear(scn->sc_dev, ATH_NODE_NET80211(ni)->an_sta);
}

static void
ath_net80211_delba_process(struct ieee80211_node *ni,
                           struct ieee80211_delba_parameterset *delbaparamset,
                           u_int16_t reasoncode
                           )
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->delba_process(scn->sc_dev, ATH_NODE_NET80211(ni)->an_sta,
                               delbaparamset, reasoncode);
}

#ifdef ATH_SUPPORT_TxBF // for TxBF RC
static void
ath_net80211_CSI_Frame_send(struct ieee80211_node *ni,
						u_int8_t	*CSI_buf,
                        u_int16_t	buf_len,
						u_int8_t    *mimo_control)
{
    //struct ieee80211com *ic = ni->ni_ic;
    //struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ieee80211_action_mgt_args actionargs;
    struct ieee80211_action_mgt_buf  actionbuf;

	//buf_len = wbuf_get_pktlen(wbuf);

    /* Send CSI Frame */
    actionargs.category = IEEE80211_ACTION_CAT_HT;
    actionargs.action   = IEEE80211_ACTION_HT_CSI;
    actionargs.arg1     = buf_len;
    actionargs.arg2     = 0;
    actionargs.arg3     = 0;
    actionargs.arg4     = CSI_buf;
	//actionargs.CSI_buf  = CSI_buf;
	/*MIMO control field (2B) + Sounding TimeStamp(4B)*/
	OS_MEMCPY(actionbuf.buf, mimo_control, MIMO_CONTROL_LEN);

    ieee80211_send_action(ni, &actionargs,(void *)&actionbuf);
}

static void
ath_net80211_v_cv_send(struct ieee80211_node *ni,
                       u_int8_t *data_buf,
                       u_int16_t buf_len)
{
    ieee80211_send_v_cv_action(ni, data_buf, buf_len);
}
static void
ath_net80211_txbf_stats_rpt_inc(struct ieee80211com *ic, 
                                struct ieee80211_node *ni)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc          *sc  = ATH_DEV_TO_SC(scn->sc_dev);

    sc->sc_stats.ast_txbf_rpt_count++;
}

static void
ath_net80211_txbf_set_rpt_received(struct ieee80211com *ic, 
                                struct ieee80211_node *ni)
{
    struct ath_node_net80211 *anode = (struct ath_node_net80211 *)ni;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    
    scn->sc_ops->txbf_set_rpt_received(anode->an_sta);
}
#endif // For TxBF RC

static int
ath_net80211_addba_send(struct ieee80211_node *ni,
                        u_int8_t tidno,
                        u_int16_t buffersize)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ieee80211_action_mgt_args actionargs;

    if (IEEE80211_NODE_USEAMPDU(ni) &&
        scn->sc_ops->check_aggr(scn->sc_dev, ATH_NODE_NET80211(ni)->an_sta, tidno)) {

        actionargs.category = IEEE80211_ACTION_CAT_BA;
        actionargs.action   = IEEE80211_ACTION_BA_ADDBA_REQUEST;
        actionargs.arg1     = tidno;
        actionargs.arg2     = buffersize;
        actionargs.arg3     = 0;

        ieee80211_send_action(ni, &actionargs, NULL);
        return 0;
    }

    return 1;
}


static void
ath_net80211_addba_status(struct ieee80211_node *ni, u_int8_t tidno, u_int16_t *status)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    *status = scn->sc_ops->addba_status(scn->sc_dev, ATH_NODE_NET80211(ni)->an_sta, tidno);
}

static void
ath_net80211_delba_send(struct ieee80211_node *ni,
                        u_int8_t tidno,
                        u_int8_t initiator,
                        u_int16_t reasoncode)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ieee80211_action_mgt_args actionargs;

    /* tear down aggregation first */
    scn->sc_ops->aggr_teardown(scn->sc_dev, ATH_NODE_NET80211(ni)->an_sta, tidno, initiator);

    /* Send DELBA request */
    actionargs.category = IEEE80211_ACTION_CAT_BA;
    actionargs.action   = IEEE80211_ACTION_BA_DELBA;
    actionargs.arg1     = tidno;
    actionargs.arg2     = initiator;
    actionargs.arg3     = reasoncode;

    ieee80211_send_action(ni, &actionargs, NULL);
}

static void
ath_net80211_addba_setresponse(struct ieee80211_node *ni, u_int8_t tidno, u_int16_t statuscode)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_ops->set_addbaresponse(scn->sc_dev, ATH_NODE_NET80211(ni)->an_sta,
                                   tidno, statuscode); 
}

static void
ath_net80211_addba_clearresponse(struct ieee80211_node *ni)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_ops->clear_addbaresponsestatus(scn->sc_dev, ATH_NODE_NET80211(ni)->an_sta);
}

static int
ath_net80211_set_country(struct ieee80211com *ic, char *isoName, u_int16_t cc, enum ieee80211_clist_cmd cmd)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_ops->set_country(scn->sc_dev, isoName, cc, cmd);
}

static void
ath_net80211_get_currentCountry(struct ieee80211com *ic, IEEE80211_COUNTRY_ENTRY *ctry)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->get_current_country(scn->sc_dev, (HAL_COUNTRY_ENTRY *)ctry);
}

static int
ath_net80211_set_regdomain(struct ieee80211com *ic, int regdomain)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_ops->set_regdomain(scn->sc_dev, regdomain, AH_TRUE);
}

static int
ath_net80211_set_quiet(struct ieee80211_node *ni, u_int8_t *quiet_elm)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ni->ni_ic);
    struct ieee80211_quiet_ie *quiet = (struct ieee80211_quiet_ie *)quiet_elm;

    return scn->sc_ops->set_quiet(scn->sc_dev, 
                                  quiet->period,
                                  quiet->period,
                                  quiet->offset + quiet->tbttcount*ni->ni_intval,
                                  1);
}

static u_int16_t ath_net80211_find_countrycode(struct ieee80211com *ic, char* isoName)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    return scn->sc_ops->find_countrycode(scn->sc_dev, isoName);
}

#ifdef ATH_SUPPORT_TxBF
#ifdef TXBF_TODO
static void	
ath_net80211_get_pos2_data(struct ieee80211com *ic, u_int8_t **p_data, 
    u_int16_t* p_len,void **rx_status)
{
	struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
	scn->sc_ops->rx_get_pos2_data(scn->sc_dev, p_data, p_len, rx_status);
}

static bool 
ath_net80211_txbfrcupdate(struct ieee80211com *ic,void *rx_status,u_int8_t *local_h,
    u_int8_t *CSIFrame, u_int8_t NESSA, u_int8_t NESSB, int BW)
{

	struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
	
	return scn->sc_ops->rx_txbfrcupdate(scn->sc_dev,rx_status, local_h, CSIFrame,
        NESSA, NESSB, BW); 
}

static void
ath_net80211_ap_save_join_mac(struct ieee80211com *ic, u_int8_t *join_macaddr)
{
	struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

	scn->sc_ops->ap_save_join_mac(scn->sc_dev, join_macaddr);
}

static void
ath_net80211_start_imbf_cal(struct ieee80211com *ic)
{
	struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

	scn->sc_ops->start_imbf_cal(scn->sc_dev);
}
#endif

static int
ath_net80211_txbf_alloc_key(struct ieee80211com *ic, struct ieee80211_node *ni)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ieee80211vap *vap = ni->ni_vap;
    u_int16_t keyidx = IEEE80211_KEYIX_NONE, status;
    
    status = scn->sc_ops->txbf_alloc_key(scn->sc_dev, ni->ni_macaddr, &keyidx);
    
    if (status != AH_FALSE) {
        ath_key_set(vap, &ni->ni_ucastkey, ni->ni_macaddr);
    }
    
    return keyidx;
}

static void
ath_net80211_txbf_set_key(struct ieee80211com *ic, struct ieee80211_node *ni)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    
    scn->sc_ops->txbf_set_key(scn->sc_dev,ni->ni_ucastkey.wk_keyix,ni->ni_txbf.rx_staggered_sounding
        ,ni->ni_txbf.channel_estimation_cap,ni->ni_mmss);  
}

static void
ath_net80211_init_sw_cv_timeout(struct ieee80211com *ic, struct ieee80211_node *ni)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc          *sc  =  ATH_DEV_TO_SC(scn->sc_dev);
    struct ath_hal *ah = sc->sc_ah;
    int power=scn->sc_ops->getpwrsavestate(scn->sc_dev);

    ni->ni_sw_cv_timeout = sc->sc_reg_parm.TxBFSwCvTimeout;

    // when ni_sw_cv_timeout=0, it use H/W timer directly 
    // when ni_sw_cv_timeout!=0, set H/W timer to 1 ms to trigger S/W timer
    /* WAR: EV:88809 Always Wake up chip before setting Phy related Registers 
     * Restore the pwrsavestate after the setting is done
     */
    scn->sc_ops->setpwrsavestate(scn->sc_dev,ATH_PWRSAVE_AWAKE);

    if (ni->ni_sw_cv_timeout) {    // use S/W timer  
        ah->ah_txbf_hw_cvtimeout = 1;                
        ath_hal_setHwCvTimeout(ah, AH_FALSE);
    } else {
        ath_hal_setHwCvTimeout(ah, AH_TRUE);
    }
    scn->sc_ops->setpwrsavestate(scn->sc_dev,power);
}
//==================================
#endif  // for TxBF RC

static u_int8_t   ath_net80211_get_ctl_by_country(struct ieee80211com *ic, u_int8_t *country, bool is2G)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_ops->get_ctl_by_country(scn->sc_dev, country, is2G);
}

static u_int16_t   ath_net80211_dfs_isdfsregdomain(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_ops->dfs_isdfsregdomain(scn->sc_dev);
}

static int  ath_net80211_getdfsdomain(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_ops->get_dfsdomain(scn->sc_dev);
}

static u_int16_t   ath_net80211_dfs_in_cac(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_ops->dfs_in_cac(scn->sc_dev);
}

static u_int16_t   ath_net80211_dfs_usenol(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_ops->dfs_usenol(scn->sc_dev);
}

static u_int
ath_net80211_mhz2ieee(struct ieee80211com *ic, u_int freq, u_int flags)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_ops->mhz2ieee(scn->sc_dev, freq, flags);
}

/*------------------------------------------------------------
 * Callbacks for ath_dev module, which calls net80211 API's
 * (ieee80211_xxx) accordingly.
 *------------------------------------------------------------
 */

static void
ath_net80211_channel_setup(ieee80211_handle_t ieee,
                           enum ieee80211_clist_cmd cmd,
                           const HAL_CHANNEL *chans, int nchan,
                           const u_int8_t *regclassids, u_int nregclass,
                           int countrycode)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ieee80211_channel *ichan;
    const HAL_CHANNEL *c;
    int i, j;

    if (cmd == CLIST_DFS_UPDATE) {
        for (i = 0; i < nchan; i++) {
            ieee80211_enumerate_channels(ichan, ic, j) {
                if (chans[i].channel == ieee80211_chan2freq(ic, ichan))
                    IEEE80211_CHAN_CLR_RADAR(ichan);
            }
        }

        return;
    }

    if (cmd == CLIST_NOL_UPDATE) {
        for (i = 0; i < nchan; i++) {
            ieee80211_enumerate_channels(ichan, ic, j) {
                if (chans[i].channel == ieee80211_chan2freq(ic, ichan))
                    IEEE80211_CHAN_SET_RADAR(ichan);
            }
        }

        return;
    }
    
    if ((countrycode == CTRY_DEFAULT) || (cmd == CLIST_NEW_COUNTRY)) {
        /*
         * Convert HAL channels to ieee80211 ones.
         */
        for (i = 0; i < nchan; i++) {
            c = &chans[i];
            ichan = ieee80211_get_channel(ic, i);
            IEEE80211_CHAN_SETUP(ichan,
                                 scn->sc_ops->mhz2ieee(scn->sc_dev, c->channel, c->channelFlags),
                                 c->channel,
                                 c->channelFlags,
                                 0, /* ic_flagext */
                                 c->maxRegTxPower,  /* dBm */
                                 c->maxTxPower / 4, /* 1/4 dBm */
                                 c->minTxPower / 4,  /* 1/4 dBm */
                                 c->regClassId
                                 );
            if (c->privFlags & CHANNEL_DFS) {
                IEEE80211_CHAN_SET_DFS(ichan);
            }
            if (c->privFlags & CHANNEL_DFS_CLEAR) {
                IEEE80211_CHAN_SET_DFS_CLEAR(ichan);
            }
            if (c->privFlags & CHANNEL_DISALLOW_ADHOC){
                IEEE80211_CHAN_SET_DISALLOW_ADHOC(ichan);
            }
            if (c->privFlags & CHANNEL_NO_HOSTAP){
                IEEE80211_CHAN_SET_DISALLOW_HOSTAP(ichan);
            }
        }
        ieee80211_set_nchannels(ic, nchan);
    }
    else {
        /*
         * Logic AND the country channel and domain channels.
         */
        ieee80211_enumerate_channels(ichan, ic, i) {
            c = chans;
            for (j = 0; j < nchan; j++) {
                if (IEEE80211_CHAN_MATCH(ichan, c->channel, c->channelFlags, (~CHANNEL_PASSIVE))) {
                    IEEE80211_CHAN_SETUP(ichan,
                                         scn->sc_ops->mhz2ieee(scn->sc_dev, c->channel, c->channelFlags),
                                         c->channel,
                                         c->channelFlags,
                                         0, /* ic_flagext */
                                         c->maxRegTxPower,  /* dBm */
                                         c->maxTxPower / 4, /* 1/4 dBm */
                                         c->minTxPower / 4,  /* 1/4 dBm */
                                         c->regClassId
                                         );
                    if (c->privFlags & CHANNEL_DFS) {
                        IEEE80211_CHAN_SET_DFS(ichan);
                    }
                    if (c->privFlags & CHANNEL_DFS_CLEAR) {
                        IEEE80211_CHAN_SET_DFS_CLEAR(ichan);
                    }
                    if (c->privFlags & CHANNEL_DISALLOW_ADHOC) {
                        IEEE80211_CHAN_SET_DISALLOW_ADHOC(ichan);
                    }
                    if (c->privFlags & CHANNEL_NO_HOSTAP){
                        IEEE80211_CHAN_SET_DISALLOW_HOSTAP(ichan);
                    }
                    break;
                }
                c++;
            }

            if (j == nchan) {
                /* channel not found, exclude it from the channel list */
                IEEE80211_CHAN_EXCLUDE_11D(ichan);
            }
        }
    }

    /*
     * Copy regclass ids
     */
    ieee80211_set_regclassids(ic, regclassids, nregclass);
}


static int
ath_net80211_set_countrycode(ieee80211_handle_t ieee, char *isoName, u_int16_t cc, enum ieee80211_clist_cmd cmd)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);

    return wlan_set_countrycode(ic, isoName, cc, cmd);
}

static void 
ath_net80211_update_node_txpow(struct ieee80211vap *vap, u_int16_t txpowlevel, u_int8_t *addr)
{
    struct ieee80211_node *ni;
    ni = ieee80211_find_txnode(vap, addr);
    ASSERT(ni);
    if (!ni)
        return;
    ieee80211node_set_txpower(ni, txpowlevel);
}

/*
 * Temporarily change this to a non-static function.
 * This avoids a compiler warning / error about a static function being
 * defined but unused.
 * Once this function is referenced, it should be changed back to static.
 * The function declaration will also need to be changed back to static.
 * 
 * The same applies to the ath_net80211_get_max_txpwr function below.
 */
/*static*/ void
ath_net80211_enable_tpc(struct ieee80211com *ic)
{
    struct ath_softc_net80211   *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->enable_tpc(scn->sc_dev);
}

/*static*/ void 
ath_net80211_get_max_txpwr(struct ieee80211com *ic, u_int32_t* txpower )
{
    struct ath_softc_net80211   *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->get_maxtxpower(scn->sc_dev, txpower);
}

static void ath_vap_iter_update_txpow(void *arg, wlan_if_t vap) 
{
    struct ieee80211_node *ni;
    u_int16_t txpowlevel = *((u_int16_t *) arg);
    ni = ieee80211vap_get_bssnode(vap);
    ASSERT(ni);
    ieee80211node_set_txpower(ni, txpowlevel);
}

static void
ath_net80211_update_txpow(ieee80211_handle_t ieee,
                          u_int16_t txpowlimit, u_int16_t txpowlevel)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);

    ieee80211com_set_txpowerlimit(ic, txpowlimit);
    
   wlan_iterate_vap_list(ic,ath_vap_iter_update_txpow,(void *) &txpowlevel);
}

struct ath_iter_update_beaconconfig_arg {
    struct ieee80211com *ic;
    int if_id;
    ieee80211_beacon_config_t *conf;
};

static void ath_vap_iter_update_beaconconfig(void *arg, wlan_if_t vap) 
{
    struct ath_iter_update_beaconconfig_arg* params = (struct ath_iter_update_beaconconfig_arg*) arg;  
    struct ieee80211_node *ni;
    if ((params->if_id == ATH_IF_ID_ANY && ieee80211vap_get_opmode(vap) == params->ic->ic_opmode) || 
        ((ATH_VAP_NET80211(vap))->av_if_id == params->if_id) ||
        (params->if_id == ATH_IF_ID_ANY && params->ic->ic_opmode == IEEE80211_M_HOSTAP &&
         ieee80211vap_get_opmode(vap) == IEEE80211_M_BTAMP)) {
        ni = ieee80211vap_get_bssnode(vap);
        params->conf->beacon_interval = ni->ni_intval;
        params->conf->listen_interval = ni->ni_lintval;
        params->conf->dtim_period = ni->ni_dtim_period;
        params->conf->dtim_count = ni->ni_dtim_count;
        params->conf->bmiss_timeout = vap->iv_ic->ic_bmisstimeout;
        params->conf->u.last_tsf = ni->ni_tstamp.tsf;
    }
}

static void
ath_net80211_get_beaconconfig(ieee80211_handle_t ieee, int if_id,
                              ieee80211_beacon_config_t *conf)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    struct ath_iter_update_beaconconfig_arg params;  
    params.ic = ic;
    params.if_id = if_id;
    params.conf = conf;

    wlan_iterate_vap_list(ic,ath_vap_iter_update_beaconconfig,(void *) &params);
}

static int16_t
ath_net80211_get_noisefloor(struct ieee80211com *ic, struct ieee80211_channel *chan)
{

    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    return scn->sc_ops->get_noisefloor(scn->sc_dev, chan->ic_freq,  ath_chan2flags(chan));
}

static void 
ath_net80211_get_chainnoisefloor(struct ieee80211com *ic, struct ieee80211_channel *chan, int16_t *nfBuf)
{

    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_ops->get_chainnoisefloor(scn->sc_dev, chan->ic_freq,  ath_chan2flags(chan), nfBuf);
}

struct ath_iter_update_beaconalloc_arg {
    struct ieee80211com *ic;
    int if_id;
    ieee80211_beacon_offset_t *bo;
    ieee80211_tx_control_t *txctl;
    wbuf_t wbuf;
};

static void ath_vap_iter_beacon_alloc(void *arg, wlan_if_t vap) 
{
    struct ath_iter_update_beaconalloc_arg* params = (struct ath_iter_update_beaconalloc_arg *) arg;  
    struct ath_vap_net80211 *avn;
    struct ieee80211_node *ni;
#define USE_SHPREAMBLE(_ic)                   \
    (IEEE80211_IS_SHPREAMBLE_ENABLED(_ic) &&  \
     !IEEE80211_IS_BARKER_ENABLED(_ic))

        if ((ATH_VAP_NET80211(vap))->av_if_id == params->if_id) {
            ni = vap->iv_bss;
            avn = ATH_VAP_NET80211(vap);
            params->wbuf = ieee80211_beacon_alloc(ni, &avn->av_beacon_offsets);
            if (params->wbuf) {

                /* set up tim offset */
                params->bo->bo_tim = avn->av_beacon_offsets.bo_tim;

                /* setup tx control block for this beacon */
                params->txctl->txpower = ieee80211_node_get_txpower(ni);
                if (USE_SHPREAMBLE(vap->iv_ic))
                    params->txctl->shortPreamble = 1;
                params->txctl->min_rate = vap->iv_mgt_rate;

                /* send this frame to hardware */
                params->txctl->an = (ATH_NODE_NET80211(ni))->an_sta;

            }
        }

}

static wbuf_t
ath_net80211_beacon_alloc(ieee80211_handle_t ieee, int if_id,
                          ieee80211_beacon_offset_t *bo,
                          ieee80211_tx_control_t *txctl)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    struct ath_iter_update_beaconalloc_arg params; 

    ASSERT(if_id != ATH_IF_ID_ANY);

    params.ic = ic;
    params.if_id = if_id;
    params.txctl = txctl;
    params.bo = bo;
    params.wbuf = NULL;

    wlan_iterate_vap_list(ic,ath_vap_iter_beacon_alloc,(void *) &params);
    return params.wbuf;

#undef USE_SHPREAMBLE
}

static int
ath_net80211_beacon_update(ieee80211_handle_t ieee, int if_id,
                           ieee80211_beacon_offset_t *bo, wbuf_t wbuf,
                           int mcast)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    struct ieee80211vap *vap = NULL;
    struct ath_vap_net80211 *avn;
    int error = 0;

    ASSERT(if_id != ATH_IF_ID_ANY);
    
    /*
     * get a vap with the given id.
     * this function is called from SWBA.
     * in most of the platform this is directly called from
     * the interrupt handler, so we need to find our vap without using any 
     * spin locks.
     */

    TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {                               
        if ((ATH_VAP_NET80211(vap))->av_if_id == if_id) {
            break;
        }
    }

    if (vap == NULL)
        return -EINVAL;

#if UMAC_SUPPORT_WDS
    /* disable beacon if VAP is operating in NAWDS bridge mode */
    if (ieee80211_nawds_disable_beacon(vap))
        return -EIO;
#endif

    avn = ATH_VAP_NET80211(vap);
    error = ieee80211_beacon_update(vap->iv_bss, &avn->av_beacon_offsets,
                                    wbuf, mcast);
    if (!error) {
        /* set up tim offset */
        bo->bo_tim = avn->av_beacon_offsets.bo_tim;
    }
   
#ifdef ATH_SUPPORT_DFS
    /* Make sure no beacons go out during the DFS wait period (CAC timer)*/
    if (ic->ic_dfs_in_cac(ic)) {
        return -EIO;
    }
#endif
    
    return error;
}

static void
ath_net80211_beacon_miss(ieee80211_handle_t ieee)
{
    ieee80211_beacon_miss(NET80211_HANDLE(ieee));
}

static void
ath_net80211_proc_tim(ieee80211_handle_t ieee)
{
    /* To-DO */
    /* 
     * when we start relying on HW for TIM bit processing.
     * we need to hook up this.
     */ 
}

struct ath_iter_set_state_arg {
    int if_id;
    u_int8_t state;
};

static void ath_vap_iter_set_state(void *arg, wlan_if_t vap) 
{
    struct ath_iter_set_state_arg *params = (struct ath_iter_set_state_arg *) arg;  

    if ((ATH_VAP_NET80211(vap))->av_if_id == params->if_id) {
        ieee80211_state_event(vap,params->state);
    }
}

/******************************************************************************/
/*!
**  \brief shim callback to set VAP state
** 
** This routine is used by DFS to change the state of the VAP after a CAC
** period, or when doing a channel change.  Required for layer seperation.
**
**  \param ieee     Pointer to shim structure (this)
**  \param if_id    VAP Index (1-4).  Zero is invalid.
**  \param state    Flag indicating INIT (0) or RUN (1) state
**
**  \return N/A
*/

static void
ath_net80211_set_vap_state(ieee80211_handle_t ieee,u_int8_t if_id, u_int8_t state)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    struct ath_iter_set_state_arg params;  

    params.if_id = if_id;

    /*
     * EV82313 - remove LMAC dependency on UMAC
     *
     * to remove dependency LMAC state events are different form UMAC.
     * We map LMAC states to UMAC states here
     */

    switch (state) {
    case LMAC_STATE_EVENT_UP:
        state = IEEE80211_STATE_EVENT_UP;
        break;
    case LMAC_STATE_EVENT_DFS_WAIT:
        state = IEEE80211_STATE_EVENT_DFS_WAIT;
        break;
    case LMAC_STATE_EVENT_DFS_CLEAR:
        state = IEEE80211_STATE_EVENT_DFS_CLEAR;
        break;
    case LMAC_STATE_EVENT_CHAN_SET:
        state = IEEE80211_STATE_EVENT_CHAN_SET;
        break;
    default:
        break;
    }
    params.state = state;

    wlan_iterate_vap_list(ic,ath_vap_iter_set_state,(void *) &params);
}

static int
ath_net80211_send_bar(ieee80211_node_t node, u_int8_t tidno, u_int16_t seqno)
{
    return ieee80211_send_bar((struct ieee80211_node *)node, tidno, seqno);
}

static void
ath_net80211_notify_qstatus(ieee80211_handle_t ieee, u_int16_t qdepth)
{
    ieee80211_notify_queue_status(NET80211_HANDLE(ieee), qdepth);
}

static INLINE void
ath_rxstat2ieee(struct ieee80211com *ic,
                ieee80211_rx_status_t *rx_status,
                struct ieee80211_rx_status *rs)
{
    rs->rs_flags =
        ((rx_status->flags & ATH_RX_FCS_ERROR) ? IEEE80211_RX_FCS_ERROR : 0) |
        ((rx_status->flags & ATH_RX_MIC_ERROR) ? IEEE80211_RX_MIC_ERROR : 0) |
        ((rx_status->flags & ATH_RX_DECRYPT_ERROR) ? IEEE80211_RX_DECRYPT_ERROR : 0)
    | ((rx_status->flags & ATH_RX_KEYMISS) ? IEEE80211_RX_KEYMISS : 0);

    rs->rs_phymode = ic->ic_curmode;
    rs->rs_freq = ic->ic_curchan->ic_freq;
    rs->rs_rssi = rx_status->rssi;
    rs->rs_abs_rssi = rx_status->abs_rssi;
    rs->rs_datarate = rx_status->rateKbps;
    rs->rs_rateieee = rx_status->rateieee;
    rs->rs_ratephy  = rx_status->ratecode;
    rs->rs_isaggr = rx_status->isaggr;
    rs->rs_isapsd = rx_status->isapsd;
    rs->rs_noisefloor = rx_status->noisefloor;
    rs->rs_channel = rx_status->channel;   
    
    rs->rs_tstamp.tsf = rx_status->tsf;

    ath_txbf_update_rx_status( rs, rx_status);

    OS_MEMCPY(rs->rs_rssictl, rx_status->rssictl, IEEE80211_MAX_ANTENNA);
    OS_MEMCPY(rs->rs_rssiextn, rx_status->rssiextn, IEEE80211_MAX_ANTENNA);
#if ATH_VOW_EXT_STATS
    rs->vow_extstats_offset = rx_status->vow_extstats_offset;
#endif    
}

int
ath_net80211_input(ieee80211_node_t node, wbuf_t wbuf, ieee80211_rx_status_t *rx_status)
{
    struct ieee80211_node *ni = (struct ieee80211_node *)node;
    struct ieee80211_rx_status rs;

    ath_rxstat2ieee(ni->ni_ic, rx_status, &rs);
    return ieee80211_input(ni, wbuf, &rs);
}

#ifdef ATH_SUPPORT_TxBF
void
ath_net80211_bf_rx(struct ieee80211com *ic, wbuf_t wbuf, ieee80211_rx_status_t *status)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);

    /* Only UMAC code base need do this check, not need for XP */
    sc->do_node_check = 1;

    switch (status->bf_flags) {
        case ATH_BFRX_PKT_MORE_ACK:
        case ATH_BFRX_PKT_MORE_QoSNULL:
        /*Not ready yet*/
            break;
        case ATH_BFRX_PKT_MORE_DELAY:
            {   /*Find & save Node*/
                struct ieee80211_node *ni_T;
                struct ieee80211_frame_min *wh1;
                
                wh1 = (struct ieee80211_frame_min *)wbuf_header(wbuf);
                ni_T = ic->ic_ieee80211_find_node(&ic->ic_sta, wh1->i_addr2);

                if (ni_T == NULL) {
                    DPRINTF(scn, ATH_DEBUG_RECV, "%s Can't Find the NODE of V/CV ?\n", __FUNCTION__);
                } else {
                    DPRINTF(scn, ATH_DEBUG_RECV, "%s save node of V/CV \n ", __FUNCTION__);
                    sc->v_cv_node = ni_T;
                }
            }
            break;
        case ATH_BFRX_DATA_UPLOAD_ACK:
        case ATH_BFRX_DATA_UPLOAD_QosNULL:
            /*Not ready yet*/
            break;
        case ATH_BFRX_DATA_UPLOAD_DELAY:
            {
                u_int8_t	*v_cv_data = (u_int8_t *) wbuf_header(wbuf);
                u_int16_t	buf_len = wbuf_get_pktlen(wbuf);
                
                if (sc->v_cv_node) {
                    DPRINTF(scn, ATH_DEBUG_RECV, "Send its C CV Report (%d) \n", buf_len);
                    ic->ic_v_cv_send(sc->v_cv_node, v_cv_data, buf_len);
                }
//                 else {
//                    DPRINTF(scn, ATH_DEBUG_RECV, " v_cv_node to be NULL ? \n");
//                }
                sc->v_cv_node = NULL;
            }
            break;
        default:
            break;
    }
}
#endif

int
ath_net80211_rx(ieee80211_handle_t ieee, wbuf_t wbuf, ieee80211_rx_status_t *rx_status, u_int16_t keyix)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ieee80211_node *ni;
    struct ieee80211_frame *wh;
    int type;
    ATH_RX_TYPE status;
    struct ieee80211_qosframe_addr4      *whqos_4addr;
    int tid;

#if ATH_SUPPORT_AOW    
    /* Record ES/ESS statistics for AoW L2 Packet Error Stats */
    if ((AOW_ES_ENAB(ic) || AOW_ESS_ENAB(ic))
            && (rx_status->flags & ATH_RX_FCS_ERROR)) {
            
        u_int32_t crc32;
        struct audio_pkt *apkt;
        u_int16_t ether_type = ETHERTYPE_IP;

        if (wbuf_get_pktlen(wbuf) < sizeof(struct ieee80211_qosframe) +
                                    sizeof(struct llc) +
                                    sizeof(struct audio_pkt)) {
                wbuf_free(wbuf);
                return -1;
        } 
                
        apkt = ATH_AOW_ACCESS_APKT(wbuf);

        /* Start off checksumming with receiver & sender addresses */
        crc32 = ath_aow_chksum_crc32((unsigned char*)ATH_AOW_ACCESS_RECVR(wbuf),
                                      sizeof(struct ether_addr) * 2);

        /* Add the Ether Type to the checksum */
        crc32 = ath_aow_cumultv_chksum_crc32(crc32,
                                             (unsigned char*)&ether_type,
                                             sizeof(ether_type));

        /* Add the Audio Pkt to the checksum */
        crc32 = ath_aow_cumultv_chksum_crc32(crc32,
                                             (unsigned char*)apkt,
                                             sizeof(struct audio_pkt) -
                                             sizeof(u_int32_t));
    
        if (crc32 == apkt->crc32) {
            /* High probability that this frame was indeed for us */
            if (AOW_ES_ENAB(ic) || AOW_ESS_SYNC_SET(apkt->params)) {
                ath_aow_l2pe_record(ic, false);
            }
        }
        
        /* If ER/Monitor mode is not enabled, we are no longer interested in 
           doing anything more with this frame */
        if (!AOW_ER_ENAB(ic)) {
            wbuf_free(wbuf);
            return -1;
        }
    }
#endif  /* ATH_SUPPORT_AOW */    

#if ATH_SUPPORT_IWSPY
	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	if (rx_status->flags & ATH_RX_RSSI_VALID)
	{
		ieee80211_input_iwspy_update_rssi(ic, wh->i_addr2, rx_status->rssi);
	}
#endif

    if (ic->ic_opmode == IEEE80211_M_MONITOR) {
        /*
         * Monitor mode: discard anything shorter than
         * an ack or cts, clean the skbuff, fabricate
         * the Prism header existing tools expect,
         * and dispatch.
         */
        if (wbuf_get_pktlen(wbuf) < IEEE80211_ACK_LEN) {
            DPRINTF(scn, ATH_DEBUG_RECV,
                    "%s: runt packet %d\n", __func__, wbuf_get_pktlen(wbuf));
            wbuf_free(wbuf);
        } else {
            struct ieee80211_rx_status rs;
            ath_rxstat2ieee(ic, rx_status, &rs);
            ieee80211_input_monitor(ic, wbuf, &rs);
        }

        return -1;
    }
   
    /*
     * From this point on we assume the frame is at least
     * as large as ieee80211_frame_min; verify that.
     */
    if (wbuf_get_pktlen(wbuf) < (ic->ic_minframesize + IEEE80211_CRC_LEN)) {
        DPRINTF(scn, ATH_DEBUG_RECV, "%s: short packet %d\n",
                    __func__, wbuf_get_pktlen(wbuf));
        wbuf_free(wbuf);
        return -1;
    }
    
#ifdef ATH_SUPPORT_TxBF
    ath_net80211_bf_rx(ic, wbuf, rx_status);
#endif
    /*
     * Normal receive.
     */
    wbuf_trim(wbuf, IEEE80211_CRC_LEN);

    if (scn->sc_debug & ATH_DEBUG_RECV) {
        ieee80211_dump_pkt(ic, wbuf_header(wbuf), wbuf_get_pktlen(wbuf) + IEEE80211_CRC_LEN,
                           rx_status->rateKbps, rx_status->rssi);
    }

    /*   
     * Handle packets with keycache miss if WEP on MBSSID
     * is enabled.
     */
    {
        struct ieee80211_rx_status rs;
        ath_rxstat2ieee(ic, rx_status, &rs);

        if (ieee80211_crypto_handle_keymiss(ic, wbuf, &rs))
            return -1;
    }

    /*
     * Locate the node for sender, track state, and then
     * pass the (referenced) node up to the 802.11 layer
     * for its use.  If the sender is unknown spam the
     * frame; it'll be dropped where it's not wanted.
     */
    IEEE80211_KEYMAP_LOCK(scn);
    ni = (keyix != HAL_RXKEYIX_INVALID) ? scn->sc_keyixmap[keyix] : NULL;
    if (ni == NULL) {
        IEEE80211_KEYMAP_UNLOCK(scn);
        /*
         * No key index or no entry, do a lookup and
         * add the node to the mapping table if possible.
         */
        ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)
                                   wbuf_header(wbuf));
        if (ni == NULL) {
            struct ieee80211_rx_status rs;
            ath_rxstat2ieee(ic, rx_status, &rs);
            return ieee80211_input_all(ic, wbuf, &rs);
        }

        /*
         * If the station has a key cache slot assigned
         * update the key->node mapping table.
         */
        IEEE80211_KEYMAP_LOCK(scn);

        keyix = ni->ni_ucastkey.wk_keyix;
        if (keyix != IEEE80211_KEYIX_NONE && scn->sc_keyixmap[keyix] == NULL &&
                 ni->ni_ucastkey.wk_valid )
            scn->sc_keyixmap[keyix] = ni;//ieee80211_ref_node(ni);

        IEEE80211_KEYMAP_UNLOCK(scn);
    } else {
        ieee80211_ref_node(ni);
        IEEE80211_KEYMAP_UNLOCK(scn);
    }

    /*
     * update node statistics
     */
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    if (IEEE80211_IS_DATA(wh)) {
        ATH_RATE_LPF(ATH_NODE_NET80211(ni)->an_avgrxrate, rx_status->rateKbps);
        ATH_NODE_NET80211(ni)->an_lastrxrate = rx_status->rateKbps;
        ATH_NODE_NET80211(ni)->an_rxratecode = rx_status->ratecode;
    }

    /* For STA, update RSSI info from associated BSSID only. Don't update RSSI, if we
       recv pkt from another BSSID(probe resp etc.)
    */
    if (rx_status->flags & ATH_RX_RSSI_VALID &&
        ((ni->ni_vap->iv_opmode != IEEE80211_M_STA) ||
        IEEE80211_IS_TDLS_NODE(ni) ||
        IEEE80211_ADDR_EQ(wh->i_addr2, ieee80211_node_get_bssid(ni)))) {
        int i;

        ATH_RSSI_LPF(ATH_NODE_NET80211(ni)->an_avgrssi, rx_status->rssi);
        if (IEEE80211_IS_BEACON(wh))
            ATH_RSSI_LPF(ATH_NODE_NET80211(ni)->an_avgbrssi, rx_status->rssi);
        else
            ATH_RSSI_LPF(ATH_NODE_NET80211(ni)->an_avgdrssi, rx_status->rssi);
        
        if (rx_status->flags & ATH_RX_CHAIN_RSSI_VALID) {
            for(i=0;i<ATH_MAX_ANTENNA;++i) {
                ATH_RSSI_LPF(ATH_NODE_NET80211(ni)->an_avgchainrssi[i], rx_status->rssictl[i]);
                if (IEEE80211_IS_BEACON(wh))
                    ATH_RSSI_LPF(ATH_NODE_NET80211(ni)->an_avgbchainrssi[i], rx_status->rssictl[i]);
                else
                    ATH_RSSI_LPF(ATH_NODE_NET80211(ni)->an_avgdchainrssi[i], rx_status->rssictl[i]);
            }
            if (rx_status->flags & ATH_RX_RSSI_EXTN_VALID) {
                for(i=0;i<ATH_MAX_ANTENNA;++i) {
                    ATH_RSSI_LPF(ATH_NODE_NET80211(ni)->an_avgchainrssiext[i], rx_status->rssiextn[i]);
                    if (IEEE80211_IS_BEACON(wh))
                        ATH_RSSI_LPF(ATH_NODE_NET80211(ni)->an_avgbchainrssiext[i], rx_status->rssiextn[i]);
                    else
                        ATH_RSSI_LPF(ATH_NODE_NET80211(ni)->an_avgdchainrssiext[i], rx_status->rssiextn[i]);
                }
            }
        }
            
    }

    /*
     * Let ath_dev do some special rx frame processing. If the frame is not
     * consumed by ath_dev, indicate it up to the stack.
     */
    type = scn->sc_ops->rx_proc_frame(scn->sc_dev, ATH_NODE_NET80211(ni)->an_sta,
                                      IEEE80211_NODE_USEAMPDU(ni),
                                      wbuf, rx_status, &status);
 

    /* For OWL specific HW bug, 4addr aggr needs to be denied in 
    * some cases. So check for delba send and send delba
    */
    if ( (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS ) {
        if (IEEE80211_NODE_WDSWAR_ISSENDDELBA(ni) ) {
	    whqos_4addr = (struct ieee80211_qosframe_addr4 *) wh;
            tid = whqos_4addr->i_qos[0] & IEEE80211_QOS_TID;
	    ath_net80211_delba_send(ni, tid, 0, IEEE80211_REASON_UNSPECIFIED);
	}
    }

    if (status != ATH_RX_CONSUMED) {
        /*
         * Not consumed by ath_dev for out-of-order delivery,
         * indicate up the stack now.
         */
        type = ath_net80211_input(ni, wbuf, rx_status);
    }
    
    ieee80211_free_node(ni);
    return type;
}

static void
ath_net80211_drain_amsdu(ieee80211_handle_t ieee)
{
#ifdef ATH_AMSDU
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    ath_amsdu_tx_drain(scn);
#endif
}

#if ATH_SUPPORT_SPECTRAL
static void
ath_net80211_spectral_indicate(ieee80211_handle_t ieee, void* spectral_buf, u_int32_t buf_size)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    IEEE80211_COMM_LOCK(ic);
    /* TBD: There should be only one ic_evtable */
    if (ic->ic_evtable[0]  && ic->ic_evtable[0]->wlan_dev_spectral_indicate) {
        (*ic->ic_evtable[0]->wlan_dev_spectral_indicate)(scn->sc_osdev, spectral_buf, buf_size);
    }
    IEEE80211_COMM_UNLOCK(ic);
}
#endif

static void
ath_net80211_sm_pwrsave_update(struct ieee80211_node *ni, int smen, int dyn,
	int ratechg)
{
	struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ni->ni_ic);
	ATH_SM_PWRSAV mode;

	if (smen) {
		mode = ATH_SM_ENABLE;
	} else {
		if (dyn) {
			mode = ATH_SM_PWRSAV_DYNAMIC;
		} else {
			mode = ATH_SM_PWRSAV_STATIC;
		}
	}

	DPRINTF(scn, ATH_DEBUG_PWR_SAVE,
	    "%s: smen: %d, dyn: %d, ratechg: %d\n",
	    __func__, smen, dyn, ratechg);
	(scn->sc_ops->ath_sm_pwrsave_update)(scn->sc_dev,
	    ATH_NODE_NET80211(ni)->an_sta, mode, ratechg);
}

static void
ath_net80211_node_ps_update(struct ieee80211_node *ni, int pwrsave,
    int pause_resume) 
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ni->ni_ic);
    DPRINTF(scn, ATH_DEBUG_PWR_SAVE,
	    "%s: pwrsave %d \n", __func__, pwrsave);
    (scn->sc_ops->update_node_pwrsave)(scn->sc_dev,
                                       ATH_NODE_NET80211(ni)->an_sta, pwrsave, 
                                       pause_resume);
    
}

static void
ath_net80211_rate_setup(ieee80211_handle_t ieee, WIRELESS_MODE wMode,
                        RATE_TYPE type, const HAL_RATE_TABLE *rt)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    enum ieee80211_phymode mode = ath_mode_map[wMode];
    struct ieee80211_rateset *rs;
    int i, maxrates, rix;

    if (mode >= IEEE80211_MODE_MAX) {
        DPRINTF(ATH_SOFTC_NET80211(ic), ATH_DEBUG_ANY,
            "%s: unsupported mode %u\n", __func__, mode);
        return;
    }
    
    if (rt->rateCount > IEEE80211_RATE_MAXSIZE) {
        DPRINTF(ATH_SOFTC_NET80211(ic), ATH_DEBUG_ANY,
                "%s: rate table too small (%u > %u)\n",
                __func__, rt->rateCount, IEEE80211_RATE_MAXSIZE);
        maxrates = IEEE80211_RATE_MAXSIZE;
    } else {
        maxrates = rt->rateCount;
    }

    switch (type) {
    case NORMAL_RATE:
        rs = IEEE80211_SUPPORTED_RATES(ic, mode);
        break;
    case HALF_RATE:
        rs = IEEE80211_HALF_RATES(ic);
        break;
    case QUARTER_RATE:
        rs = IEEE80211_QUARTER_RATES(ic);
        break;
    default:
        DPRINTF(ATH_SOFTC_NET80211(ic), ATH_DEBUG_ANY,
            "%s: unknown rate type%u\n", __func__, type);
        return;
    }
    
    /* supported rates (non HT) */
    rix = 0;
    for (i = 0; i < maxrates; i++) {
        if ((rt->info[i].phy == IEEE80211_T_HT))
            continue;
        rs->rs_rates[rix++] = rt->info[i].dot11Rate;
    }
    rs->rs_nrates = (u_int8_t)rix;
    if ((mode == IEEE80211_MODE_11NA_HT20)     || (mode == IEEE80211_MODE_11NG_HT20)      ||
        (mode == IEEE80211_MODE_11NA_HT40PLUS) || (mode == IEEE80211_MODE_11NA_HT40MINUS) ||
        (mode == IEEE80211_MODE_11NG_HT40PLUS) || (mode == IEEE80211_MODE_11NG_HT40MINUS)) {
        /* supported rates (HT) */
        rix = 0;
        rs = IEEE80211_HT_RATES(ic, mode);
        for (i = 0; i < maxrates; i++) {
            if (rt->info[i].phy == IEEE80211_T_HT) {
                rs->rs_rates[rix++] = rt->info[i].dot11Rate;
            }
        }
        rs->rs_nrates = (u_int8_t)rix;
    }
}

static void ath_net80211_update_txrate(ieee80211_node_t node, int txrate)
{
}

static void ath_net80211_rate_node_update(ieee80211_handle_t ieee, ieee80211_node_t node, int isnew)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    struct ieee80211_node *ni = (struct ieee80211_node *)node;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_node_net80211 *an = (struct ath_node_net80211 *)ni;
    struct ieee80211vap *vap = ieee80211_node_get_vap(ni);
    u_int32_t capflag = 0;
    enum ieee80211_cwm_width ic_cw_width = ic->ic_cwm_get_width(ic);

    if (ni->ni_flags & IEEE80211_NODE_HT) {
        capflag |=  ATH_RC_HT_FLAG;
        if ((ni->ni_chwidth == IEEE80211_CWM_WIDTH40) && 
            (ic_cw_width == IEEE80211_CWM_WIDTH40))
        {
            capflag |=  ATH_RC_CW40_FLAG;
        }
        if (((ni->ni_htcap & IEEE80211_HTCAP_C_SHORTGI40) && (ic_cw_width == IEEE80211_CWM_WIDTH40)) ||
            ((ni->ni_htcap & IEEE80211_HTCAP_C_SHORTGI20) && (ic_cw_width == IEEE80211_CWM_WIDTH20))) {
            capflag |= ATH_RC_SGI_FLAG;
        }

        /* Rx STBC is a 2-bit mask. Needs to convert from ieee definition to ath definition. */ 
        capflag |= (((ni->ni_htcap & IEEE80211_HTCAP_C_RXSTBC) >> IEEE80211_HTCAP_C_RXSTBC_S) 
                    << ATH_RC_RX_STBC_FLAG_S);

        capflag |= ath_set_ratecap(scn, ni, vap);

        if (ni->ni_flags & IEEE80211_NODE_WEPTKIP) {

            capflag |= ATH_RC_WEP_TKIP_FLAG;
            if (ieee80211_ic_wep_tkip_htrate_is_set(ic)) {
                /* TKIP supported at HT rates */
                if (ieee80211_has_weptkipaggr(ni)) {
                    /* Pass proprietary rx delimiter count for tkip w/aggr to ath_dev */
                    scn->sc_ops->set_weptkip_rxdelim(scn->sc_dev, an->an_sta, ni->ni_weptkipaggr_rxdelim);
                } else {
                    /* Atheros proprietary wep/tkip aggregation mode is not supported */
                    ni->ni_flags |= IEEE80211_NODE_NOAMPDU;
                }
            } else {
                /* no TKIP support at HT rates => disable HT and aggregation */
                capflag &= ~ATH_RC_HT_FLAG;
                ni->ni_flags |= IEEE80211_NODE_NOAMPDU;
            }
        }
    }

    if (ni->ni_flags & IEEE80211_NODE_UAPSD) {
        capflag |= ATH_RC_UAPSD_FLAG;
    }

#ifdef  ATH_SUPPORT_TxBF
    capflag |= (((ni->ni_txbf.channel_estimation_cap) << ATH_RC_CEC_FLAG_S) & ATH_RC_CEC_FLAG);
#endif
    ((struct ath_node *)an->an_sta)->an_cap = capflag; 
    scn->sc_ops->ath_rate_newassoc(scn->sc_dev, an->an_sta, isnew, capflag,
                                   &ni->ni_rates, &ni->ni_htrates);
}

/* Iterator function */
static void
rate_cb(void *arg, struct ieee80211_node *ni)
{
    struct ieee80211vap *vap = (struct ieee80211vap *)arg;

    if ((ni != ni->ni_vap->iv_bss) && (vap == ni->ni_vap)) {
        ath_net80211_rate_node_update(vap->iv_ic, ni, 1);
    }
}

static void ath_net80211_rate_newstate(ieee80211_handle_t ieee, ieee80211_if_t if_data)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ieee80211vap *vap = (struct ieee80211vap *)if_data;
    struct ieee80211_node* ni = ieee80211vap_get_bssnode(vap);
    u_int32_t capflag = 0;
    ath_node_t an;
    enum ieee80211_cwm_width ic_cw_width = ic->ic_cwm_get_width(ic);

    ASSERT(ni);

    if (ieee80211vap_get_opmode(vap) != IEEE80211_M_STA) {
        /*
         * Sync rates for associated stations and neighbors.
         */
        wlan_iterate_station_list(vap, rate_cb, (void *)vap);
        
        if (ic_cw_width == IEEE80211_CWM_WIDTH40) {
            capflag |= ATH_RC_CW40_FLAG;
        }
        if (IEEE80211_IS_CHAN_11N(vap->iv_bsschan)) {
            capflag |= ATH_RC_HT_FLAG;
    	    if (scn->sc_ops->have_capability(scn->sc_dev, ATH_CAP_TS)) {
                capflag |= ATH_RC_TS_FLAG;
            }
    	    if (scn->sc_ops->have_capability(scn->sc_dev, ATH_CAP_DS)) {
                capflag |= ATH_RC_DS_FLAG;
            }
        }
    } else {
        if (ni->ni_flags & IEEE80211_NODE_HT) {
            capflag |=  ATH_RC_HT_FLAG;
            capflag |= ath_set_ratecap(scn, ni, vap);
        }

        if ((vap->iv_bss->ni_chwidth == IEEE80211_CWM_WIDTH40) && 
            (ic_cw_width == IEEE80211_CWM_WIDTH40)) 
        {
            capflag |= ATH_RC_CW40_FLAG;
        }
    }
    if (((ni->ni_htcap & IEEE80211_HTCAP_C_SHORTGI40) && (ic_cw_width == IEEE80211_CWM_WIDTH40)) ||
        ((ni->ni_htcap & IEEE80211_HTCAP_C_SHORTGI20) && (ic_cw_width == IEEE80211_CWM_WIDTH20))) {
        capflag |= ATH_RC_SGI_FLAG;
    }

    /* Rx STBC is a 2-bit mask. Needs to convert from ieee definition to ath definition. */
    capflag |= (((ni->ni_htcap & IEEE80211_HTCAP_C_RXSTBC) >> IEEE80211_HTCAP_C_RXSTBC_S)
                << ATH_RC_RX_STBC_FLAG_S);

    if (ni->ni_flags & IEEE80211_NODE_WEPTKIP) {
        capflag |= ATH_RC_WEP_TKIP_FLAG;
    }

    if (ni->ni_flags & IEEE80211_NODE_UAPSD) {
        capflag |= ATH_RC_UAPSD_FLAG;
    }

#ifdef  ATH_SUPPORT_TxBF
    capflag |= (((ni->ni_txbf.channel_estimation_cap) << ATH_RC_CEC_FLAG_S) & ATH_RC_CEC_FLAG);
#endif
    an = ((struct ath_node_net80211 *)ni)->an_sta;
    scn->sc_ops->ath_rate_newassoc(scn->sc_dev, an, 1, capflag,
                                   &ni->ni_rates, &ni->ni_htrates);
}

static HAL_HT_MACMODE ath_net80211_cwm_macmode(ieee80211_handle_t ieee)
{
   return ath_cwm_macmode(ieee);
}

static void ath_net80211_chwidth_change(struct ieee80211_node *ni)
{
   ath_cwm_chwidth_change(ni);
}

#ifndef REMOVE_PKTLOG_PROTO
static u_int8_t *ath_net80211_parse_frm(ieee80211_handle_t ieee, wbuf_t wbuf,
                                         ieee80211_node_t node,
                                         void *frm, u_int16_t keyix)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ieee80211_node *ni = (struct ieee80211_node *)node;
    struct ieee80211_frame *wh;
    u_int8_t *llc = NULL;
    int icv_len, llc_start_offset, len;

    if (ni == NULL && keyix != HAL_RXKEYIX_INVALID) { /* rx node */
        ni = scn->sc_keyixmap[keyix];
        if (ni == NULL)
            return NULL;
    }

    wh = (struct ieee80211_frame *)frm;
    
    if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) != IEEE80211_FC0_TYPE_DATA) {
        return NULL;
    }

    if (ni) {
        if (ni->ni_ucastkey.wk_cipher == &ieee80211_cipher_none) {
            icv_len = 0;
        } else {
            struct ieee80211_key *k = &ni->ni_ucastkey;
            const struct ieee80211_cipher *cip = k->wk_cipher;

            icv_len = cip->ic_header;
        }
    }
    else {
        icv_len = 0;
    }
    
    /*
     * Get llc offset.
     */
    llc_start_offset = ieee80211_anyhdrspace(ic, wh) + icv_len;

    if (wbuf) {
        while(llc_start_offset > 0) {
            len = wbuf_get_len(wbuf);
            if (len >= llc_start_offset + sizeof(struct llc)) {
                llc = (u_int8_t *)(wbuf_raw_data(wbuf)) + llc_start_offset;
                break;
            }
            else {
                wbuf = wbuf_next_buf(wbuf);
                if (!wbuf) {
                    return NULL;
                }
            
            }
            llc_start_offset -= len;    
        }
        
        if (!llc){
            return NULL;
        }
    }
    else {
        llc = (u_int8_t *)wh + ieee80211_anyhdrspace(ic, wh) + icv_len;
    }

    return llc;
}
#endif

#if ATH_SUPPORT_IQUE
void ath_net80211_hbr_settrigger(ieee80211_node_t node, int signal)
{
	struct ieee80211_node *ni;
    struct ieee80211vap *vap;
	ni = (struct ieee80211_node *)node;
	/* Node is associated, and not the AP self */
	if (ni && ni->ni_associd && ni != ni->ni_vap->iv_bss) {
        vap = ni->ni_vap;
        if (vap->iv_ique_ops.hbr_sendevent)
    		vap->iv_ique_ops.hbr_sendevent(vap, ni->ni_macaddr, signal);
	}
}

static u_int8_t ath_net80211_get_hbr_block_state(ieee80211_node_t node)
{
    return ieee80211_node_get_hbr_block_state(node);
}
#endif /*ATH_SUPPORT_IQUE*/

#if ATH_SUPPORT_VOWEXT
static u_int16_t ath_net80211_get_aid(ieee80211_node_t node)
{
    return ieee80211_node_get_associd(node);
}
#endif

#if ATH_SUPPORT_VOW_DCS
static void ath_net80211_interference_handler(ieee80211_handle_t ieee)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    struct ieee80211vap *vap;
    /* Check if CW Interference is already been found and being handled */
    if (ic->cw_inter_found) return;

    spin_lock(ic->ic_lock);
    /* Set the CW interference flag so that ACS does not bail out */
    ic->cw_inter_found = 1;

    TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
        if ((vap->iv_opmode == IEEE80211_M_HOSTAP) ){
            wlan_set_channel(vap, IEEE80211_CHAN_ANY);
            goto done;
        }
    }
done:
    spin_unlock(ic->ic_lock);

}
#endif

#ifdef ATH_SWRETRY
static u_int16_t ath_net80211_get_pwrsaveq_len(ieee80211_node_t node)
{
    return ieee80211_node_get_pwrsaveq_len(node);
}

static void ath_net80211_set_tim(ieee80211_node_t node,u_int8_t setflag)
{
    struct ieee80211_node *ni;
    struct ieee80211vap *vap;
    ni = (struct ieee80211_node *)node;
    /* Node is associated, and not the AP self */
    if (ni && ni->ni_associd && ni != ni->ni_vap->iv_bss) {
        vap = ni->ni_vap;
    if (vap->iv_set_tim != NULL)
        vap->iv_set_tim(ni, setflag, false);
    }
}

#endif

struct ieee80211_ops net80211_ops = {
    ath_get_netif_settings,                 /* get_netif_settings */
    ath_mcast_merge,                        /* netif_mcast_merge  */
    ath_net80211_channel_setup,             /* setup_channel_list */
    ath_net80211_set_countrycode,           /* set_countrycode    */
    ath_net80211_update_txpow,              /* update_txpow       */
    ath_net80211_get_beaconconfig,          /* get_beacon_config  */
    ath_net80211_beacon_alloc,              /* get_beacon         */
    ath_net80211_beacon_update,             /* update_beacon      */
    ath_net80211_beacon_miss,               /* notify_beacon_miss */
    ath_net80211_proc_tim,                  /* proc_tim           */
    ath_net80211_send_bar,                  /* send_bar           */
    ath_net80211_notify_qstatus,            /* notify_txq_status  */
    ath_net80211_tx_complete,               /* tx_complete        */
    ath_net80211_tx_status,                 /* tx_status          */
    ath_net80211_rx,                        /* rx_indicate        */
    ath_net80211_input,                     /* rx_subframe        */
    ath_net80211_cwm_macmode,               /* cwm_macmode        */
#ifdef ATH_SUPPORT_DFS
    ath_net80211_dfs_test_return,           /* dfs_test_return    */    
    ath_net80211_mark_dfs,                  /* mark_dfs           */
#endif
    ath_net80211_set_vap_state,             /* set_vap_state      */
    ath_net80211_change_channel,            /* change channel     */
    ath_net80211_switch_mode_static20,      /* change mode to static20 */
    ath_net80211_switch_mode_dynamic2040,   /* change mode to dynamic2040 */
    ath_net80211_rate_setup,                /* setup_rate */
    ath_net80211_update_txrate,             /* update_txrate */
    ath_net80211_rate_newstate,             /* rate_newstate */
    ath_net80211_rate_node_update,          /* rate_node_update */
    ath_net80211_drain_amsdu,               /* drain_amsdu */
    ath_net80211_node_get_extradelimwar,    /* node_get_extradelimwar */
#if ATH_SUPPORT_SPECTRAL
    ath_net80211_spectral_indicate,         /* spectral_indicate */
#endif
#ifdef ATH_SUPPORT_UAPSD
    ath_net80211_check_uapsdtrigger,        /* check_uapsdtrigger */
    ath_net80211_uapsd_eospindicate,        /* uapsd_eospindicate */
    ath_net80211_uapsd_allocqosnullframe,   /* uapsd_allocqosnull */
    ath_net80211_uapsd_getqosnullframe,     /* uapsd_getqosnullframe */
    ath_net80211_uapsd_retqosnullframe,     /* uapsd_retqosnullframe */
    ath_net80211_uapsd_deliverdata,         /* uapsd_deliverdata */
#endif
    NULL,                                   /* get_htmaxrate) */
#if ATH_SUPPORT_IQUE
    ath_net80211_hbr_settrigger,            /* hbr_settrigger */
    ath_net80211_get_hbr_block_state,       /* get_hbr_block_state */    
#endif    
#if ATH_SUPPORT_VOWEXT
    ath_net80211_get_aid,                   /* get_aid */
#endif   
#ifdef ATH_SUPPORT_LINUX_STA
    NULL,                                   /* ath_net80211_suspend */
    NULL,                                   /* ath_net80211_resume */
#endif
#ifdef ATH_BT_COEX
    NULL,                                   /* bt_coex_ps_enable */
    NULL,                                   /*bt_coex_ps_poll */
#endif
#ifdef ATH_SUPPORT_HTC
    NULL,                                   /*ath_htc_gettargetnodeid */
#endif
#ifdef ATH_USB
    NULL,                                   /* ath_usb_wmm_update */
#endif
#ifdef ATH_SUPPORT_HTC
    NULL,                                   /*ath_htc_gettargetvapid */
    NULL,                                   /*ath_net80211_uapsd_credit_update*/
#endif
#if ATH_SUPPORT_CFEND
    ath_net80211_cfend_alloc,               /* cfend_alloc */
#endif
#ifndef REMOVE_PKTLOG_PROTO
    ath_net80211_parse_frm,                 /* parse_frm */
#endif

#if  ATH_SUPPORT_AOW
    ath_net80211_aow_send2all_nodes,        /* Send audio packet to all */
    ath_net80211_aow_consume_audio_data,    /* Process the received Rx data */
#endif  /* ATH_SUPPORT_AOW */

    ath_net80211_get_bssid,
#ifdef ATH_TX99_DIAG
    ath_net80211_find_channel,
#endif
    ath_net80211_set_stbcconfig,            /* set_stbc_config */
    ath_net80211_set_ldpcconfig,            /* set_ldpc_config */
#if UMAC_SUPPORT_SMARTANTENNA
    ath_net80211_update_smartant_pertable,  /* update_smartant_pertable */
#endif
#if ATH_SUPPORT_VOW_DCS
    ath_net80211_interference_handler,
#endif    
#ifdef ATH_SWRETRY
    ath_net80211_get_pwrsaveq_len,          /* get_pwrsaveq_len */
    ath_net80211_set_tim,                   /* set_tim */
#endif 
};

static void 
ath_net80211_get_bssid(ieee80211_handle_t ieee,  struct
        ieee80211_frame_min *hdr, u_int8_t *bssid)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    struct ieee80211_node *ni;

    ni = ieee80211_find_rxnode_nolock(ic, hdr);
    if (ni) {
        OS_MEMCPY(bssid, ni->ni_bssid, IEEE80211_ADDR_LEN); 
        /* 
         * find node would increment the ref count, if
         * node is identified make sure that is unrefed again
         */
        ieee80211_unref_node(&ni);
    }
}

void ath_net80211_switch_mode_dynamic2040(ieee80211_handle_t ieee)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    ath_cwm_switch_mode_dynamic2040(ic);
}

void
ath_net80211_switch_mode_static20(ieee80211_handle_t ieee)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    ath_cwm_switch_mode_static20(ic);
}


#ifdef ATH_SUPPORT_DFS
void
ath_net80211_dfs_test_return(ieee80211_handle_t ieee, u_int8_t ieeeChan)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
#ifdef MAGPIE_HIF_GMAC    
    struct ieee80211vap *tmp_vap = NULL;
#endif    
    /* Return to the original channel we were on before the test mute */
    DPRINTF(ATH_SOFTC_NET80211(ieee), ATH_DEBUG_ANY,
        "Returning to channel %d\n", ieeeChan);
    ic->ic_chanchange_chan = ieeeChan;
    ic->ic_chanchange_tbtt = IEEE80211_RADAR_11HCOUNT;
#ifdef MAGPIE_HIF_GMAC
    TAILQ_FOREACH(tmp_vap, &ic->ic_vaps, iv_next) {
        ic->ic_chanchange_cnt += ic->ic_chanchange_tbtt; 
    }
#endif    
    ic->ic_flags |= IEEE80211_F_CHANSWITCH;
}

void        
ath_net80211_mark_dfs(ieee80211_handle_t ieee, struct ieee80211_channel *ichan)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);

    DPRINTF(ATH_SOFTC_NET80211(ieee), ATH_DEBUG_ANY,
        "%s : Radar found on channel %d (%d MHz)\n",
        __func__, ichan->ic_ieee, ichan->ic_freq);
    ieee80211_mark_dfs(ic, ichan);
}

#endif

void
ath_net80211_change_channel(ieee80211_handle_t ieee, struct ieee80211_channel *chan)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    ic->ic_curchan = chan;
    ic->ic_set_channel(ic);
}

void
wlan_setTxPowerLimit(struct ieee80211com *ic, u_int32_t limit, u_int16_t tpcInDb, u_int32_t is2GHz)
{
    ath_setTxPowerLimit(ic, limit, tpcInDb, is2GHz);
}

void
ath_setTxPowerLimit(struct ieee80211com *ic, u_int32_t limit, u_int16_t tpcInDb, u_int32_t is2GHz)
{
    struct ath_softc_net80211    *scn = ATH_SOFTC_NET80211(ic);
    /* called by ccx association for setting tx power */
    scn->sc_ops->ath_set_txPwrLimit(scn->sc_dev, limit, tpcInDb, is2GHz);
}

u_int8_t
ath_net80211_get_common_power(struct ieee80211com *ic, struct ieee80211_channel *chan)
{
    struct ath_softc_net80211    *scn = ATH_SOFTC_NET80211(ic);

    return scn->sc_ops->get_common_power(chan->ic_freq);
}
    
static u_int32_t
ath_net80211_get_maxphyrate(struct ieee80211com *ic, struct ieee80211_node *ni,uint32_t shortgi)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_node_net80211 *an = (struct ath_node_net80211 *)ni;

    return scn->sc_ops->node_getmaxphyrate(scn->sc_dev, an->an_sta,shortgi);
}

#ifdef ATH_CCX
int
ath_getrmcounters(struct ieee80211com *ic, struct ath_mib_cycle_cnts *pCnts)
{
    struct ath_softc_net80211    *scn = ATH_SOFTC_NET80211(ic);

    return (scn->sc_ops->ath_get_mibCycleCounts(scn->sc_dev, pCnts));
}

void
ath_clearrmcounters(struct ieee80211com *ic)
{
    struct ath_softc_net80211    *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_ops->ath_clear_mibCounters(scn->sc_dev);
}

int
ath_updatermcounters(struct ieee80211com *ic, struct ath_mib_mac_stats *pStats)
{
    struct ath_softc_net80211    *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_ops->ath_update_mibMacstats(scn->sc_dev);
    return scn->sc_ops->ath_get_mibMacstats(scn->sc_dev, pStats);
}

u_int8_t
ath_net80211_rcRateValueToPer(struct ieee80211com *ic, struct ieee80211_node *ni, int txRateKbps)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return (scn->sc_ops->rcRateValueToPer(scn->sc_dev, (struct ath_node *)(ATH_NODE_NET80211(ni)->an_sta),
            txRateKbps));
}

u_int64_t
ath_getTSF64(struct ieee80211com *ic)
{
    struct ath_softc_net80211    *scn = ATH_SOFTC_NET80211(ic);

    return scn->sc_ops->ath_get_tsf64(scn->sc_dev);
}

void
ath_setReceiveFilter(struct ieee80211com *ic)
{
    struct ath_softc_net80211    *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_ops->ath_set_rxfilter(scn->sc_dev);
}

int
ath_getMfgSerNum(struct ieee80211com *ic, u_int8_t *pSrn, int limit)
{
    struct ath_softc_net80211    *scn = ATH_SOFTC_NET80211(ic);

    return scn->sc_ops->ath_get_sernum(scn->sc_dev, pSrn, limit);
}

int
ath_net80211_get_chanData(struct ieee80211com *ic, struct ieee80211_channel *pChan, struct ath_chan_data *pData)
{
    struct ath_softc_net80211    *scn = ATH_SOFTC_NET80211(ic);

    return scn->sc_ops->ath_get_chandata(scn->sc_dev, pChan, pData);

}

u_int32_t
ath_net80211_get_curRSSI(struct ieee80211com *ic)
{
    struct ath_softc_net80211    *scn = ATH_SOFTC_NET80211(ic);

    return scn->sc_ops->ath_get_curRSSI(scn->sc_dev);
}
#endif

#ifdef ATH_SWRETRY
/* Interface function for the IEEE layer to manipulate
 * the software retry state. Used during BMISS and 
 * scanning state machine in IEEE layer
 */
void 
ath_net80211_set_swretrystate(struct ieee80211_node *ni, int flag)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    ath_node_t node = ATH_NODE_NET80211(ni)->an_sta;

    scn->sc_ops->set_swretrystate(scn->sc_dev, node, flag);
    DPRINTF(scn, ATH_DEBUG_SWR, "%s: swr %s for ni %s\n", __func__, flag?"enable":"disable", ether_sprintf(ni->ni_macaddr));
}

/* Interface function for the IEEE layer to schedule one single
 * frame in LMAC upon PS-Poll frame
 */
int
ath_net80211_handle_pspoll(struct ieee80211com *ic, struct ieee80211_node *ni)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    ath_node_t node = ATH_NODE_NET80211(ni)->an_sta;

    return scn->sc_ops->ath_handle_pspoll(scn->sc_dev, node);
}

/* Check whether there is pending frame in LMAC tid Q */
int 
ath_net80211_exist_pendingfrm_tidq(struct ieee80211com *ic, struct ieee80211_node *ni)
{   
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    ath_node_t node = ATH_NODE_NET80211(ni)->an_sta;
    
    return scn->sc_ops->get_exist_pendingfrm_tidq(scn->sc_dev, node);
}  

int 
ath_net80211_reset_pasued_tid(struct ieee80211com *ic, struct ieee80211_node *ni)
{   
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    ath_node_t node = ATH_NODE_NET80211(ni)->an_sta;

    return scn->sc_ops->reset_paused_tid(scn->sc_dev, node);
}  

#endif

#if ATH_SUPPORT_IQUE
void
ath_net80211_set_acparams(struct ieee80211com *ic, u_int8_t ac, u_int8_t use_rts,
                          u_int8_t aggrsize_scaling, u_int32_t min_kbps)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_ops->ath_set_acparams(scn->sc_dev, ac, use_rts, aggrsize_scaling, min_kbps);
}

void
ath_net80211_set_rtparams(struct ieee80211com *ic, u_int8_t ac, u_int8_t perThresh,
                          u_int8_t probeInterval)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_ops->ath_set_rtparams(scn->sc_dev, ac, perThresh, probeInterval);
}

void
ath_net80211_get_iqueconfig(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_ops->ath_get_iqueconfig(scn->sc_dev);
}

void
ath_net80211_set_hbrparams(struct ieee80211vap *iv, u_int8_t ac, u_int8_t enable, u_int8_t per)
{
	struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(iv->iv_ic);

	scn->sc_ops->ath_set_hbrparams(scn->sc_dev, ac, enable, per);
	/* Send ACTIVE signal to all nodes. Otherwise, if the hbr_enable is turned off when
	 * one state machine is in BLOCKING or PROBING state, the ratecontrol module 
	 * will never send ACTIVE signals after hbr_enable is turned off, therefore
	 * the state machine will stay in the PROBING state forever 
	 */
	/* TODO ieee80211_hbr_setstate_all(iv, HBR_SIGNAL_ACTIVE); */
}
#endif /*ATH_SUPPORT_IQUE*/


static u_int32_t ath_net80211_get_goodput(struct ieee80211_node *ni)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_node_net80211 *an = (struct ath_node_net80211 *)ni;

    if (scn->sc_ops->ath_get_goodput)
	    return (scn->sc_ops->ath_get_goodput(scn->sc_dev, an->an_sta))/100;
    return 0;
}

u_int32_t
ath_getTSF32(struct ieee80211com *ic)
{
    struct ath_softc_net80211    *scn = ATH_SOFTC_NET80211(ic);

    return scn->sc_ops->ath_get_tsf32(scn->sc_dev);
}

static void
ath_update_phystats(struct ieee80211com *ic, enum ieee80211_phymode mode)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    WIRELESS_MODE wmode;
    struct ath_phy_stats *ath_phystats;
    struct ieee80211_phy_stats *phy_stats;

    wmode = ath_ieee2wmode(mode);
    if (wmode == WIRELESS_MODE_MAX) {
        ASSERT(0);
        return;
    }

    /* get corresponding IEEE PHY stats array */
    phy_stats = &ic->ic_phy_stats[mode];

    /* get ath_dev PHY stats array */
    ath_phystats = scn->sc_ops->get_phy_stats(scn->sc_dev, wmode);

    /* update interested counters */
    phy_stats->ips_rx_fifoerr += ath_phystats->ast_rx_fifoerr;
    phy_stats->ips_rx_decrypterr += ath_phystats->ast_rx_decrypterr;
    phy_stats->ips_rx_crcerr += ath_phystats->ast_rx_crcerr;
    phy_stats->ips_tx_rts += ath_phystats->ast_tx_rts;
    phy_stats->ips_tx_longretry += ath_phystats->ast_tx_longretry;
}

static void
ath_clear_phystats(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->clear_stats(scn->sc_dev);
}

static int
ath_net80211_set_macaddr(struct ieee80211com *ic, u_int8_t *macaddr)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->set_macaddr(scn->sc_dev, macaddr);
    return 0;
}

static int 
ath_net80211_set_chain_mask(struct ieee80211com *ic, ieee80211_device_param type, u_int32_t mask)
{
    u_int32_t curmask;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    switch(type) {
    case IEEE80211_DEVICE_TX_CHAIN_MASK:
        curmask=scn->sc_ops->have_capability(scn->sc_dev, ATH_CAP_TXCHAINMASK);
        if (mask != curmask) {
           scn->sc_ops->set_tx_chainmask(scn->sc_dev, mask);
        }
        break;
    case IEEE80211_DEVICE_RX_CHAIN_MASK:
        curmask=scn->sc_ops->have_capability(scn->sc_dev, ATH_CAP_RXCHAINMASK);
        if (mask != curmask) {
           scn->sc_ops->set_rx_chainmask(scn->sc_dev, mask);
        }
        break;
        /* always set the legacy chainmasks to avoid inconsistency between sc_config
         * and sc_tx/rx_chainmask
         */
    case IEEE80211_DEVICE_TX_CHAIN_MASK_LEGACY:
        scn->sc_ops->set_tx_chainmasklegacy(scn->sc_dev, mask);
        break;
    case IEEE80211_DEVICE_RX_CHAIN_MASK_LEGACY:
        scn->sc_ops->set_rx_chainmasklegacy(scn->sc_dev, mask);
        break;
    default:
        break;
    }
    return 0;
}

/*
 * Get the number of spatial streams supported, and set it
 * in the protocol layer.
 */
static void
ath_set_spatialstream(struct ath_softc_net80211 *scn)
{
    u_int8_t    stream;

    if (scn->sc_ops->have_capability(scn->sc_dev, ATH_CAP_DS)) {
        if (scn->sc_ops->have_capability(scn->sc_dev, ATH_CAP_TS))
            stream = 3;
        else
            stream = 2;
    } else {
        stream = 1;
    }

    ieee80211com_set_spatialstreams(&scn->sc_ic, stream);
}

#ifdef ATH_SUPPORT_TxBF
static int
ath_set_txbfcapability(struct ieee80211com *ic)
{
    struct ath_softc_net80211    *scn = ATH_SOFTC_NET80211(ic);
    ieee80211_txbf_caps_t *txbf_cap;
    int error;

    error = scn->sc_ops->get_txbf_caps(scn->sc_dev, &txbf_cap);

    if (AH_FALSE == error)
        return error;

    ic->ic_txbf.channel_estimation_cap    = txbf_cap->channel_estimation_cap;
    ic->ic_txbf.csi_max_rows_bfer         = txbf_cap->csi_max_rows_bfer;
    ic->ic_txbf.comp_bfer_antennas        = txbf_cap->comp_bfer_antennas;
    ic->ic_txbf.noncomp_bfer_antennas     = txbf_cap->noncomp_bfer_antennas;
    ic->ic_txbf.csi_bfer_antennas         = txbf_cap->csi_bfer_antennas;
    ic->ic_txbf.minimal_grouping          = txbf_cap->minimal_grouping;
    ic->ic_txbf.explicit_comp_bf          = txbf_cap->explicit_comp_bf;
    ic->ic_txbf.explicit_noncomp_bf       = txbf_cap->explicit_noncomp_bf;
    ic->ic_txbf.explicit_csi_feedback     = txbf_cap->explicit_csi_feedback;
    ic->ic_txbf.explicit_comp_steering    = txbf_cap->explicit_comp_steering;
    ic->ic_txbf.explicit_noncomp_steering = txbf_cap->explicit_noncomp_steering;
    ic->ic_txbf.explicit_csi_txbf_capable = txbf_cap->explicit_csi_txbf_capable;
    ic->ic_txbf.calibration               = txbf_cap->calibration;
    ic->ic_txbf.implicit_txbf_capable     = txbf_cap->implicit_txbf_capable;
    ic->ic_txbf.tx_ndp_capable            = txbf_cap->tx_ndp_capable;
    ic->ic_txbf.rx_ndp_capable            = txbf_cap->rx_ndp_capable;
    ic->ic_txbf.tx_staggered_sounding     = txbf_cap->tx_staggered_sounding;
    ic->ic_txbf.rx_staggered_sounding     = txbf_cap->rx_staggered_sounding;
    ic->ic_txbf.implicit_rx_capable       = txbf_cap->implicit_rx_capable;

    return 0;
}
#endif

static void ath_net80211_green_ap_set_enable( struct ieee80211com *ic, int val )
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->ath_green_ap_dev_set_enable(scn->sc_dev, val);
}
	
static int ath_net80211_green_ap_get_enable( struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_ops->ath_green_ap_dev_get_enable(scn->sc_dev);
}


static void ath_net80211_green_ap_set_transition_time( struct ieee80211com *ic, int val )
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->ath_green_ap_dev_set_transition_time(scn->sc_dev, val);    
}

static int ath_net80211_green_ap_get_transition_time( struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_ops->ath_green_ap_dev_get_transition_time(scn->sc_dev);
}

static void ath_net80211_green_ap_set_on_time( struct ieee80211com *ic, int val )
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->ath_green_ap_dev_set_on_time(scn->sc_dev, val);    
}

static int ath_net80211_green_ap_get_on_time( struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_ops->ath_green_ap_dev_get_on_time(scn->sc_dev);
}

static int16_t ath_net80211_get_cur_chan_noisefloor(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_ops->ath_dev_get_noisefloor(scn->sc_dev);
}

#if ATH_SUPPORT_5G_EACS 
static void  
ath_net80211_get_cur_chan_stats(struct ieee80211com *ic, struct ieee80211_chan_stats *chan_stats)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->ath_dev_get_chan_stats(scn->sc_dev, (void *) chan_stats );
}
#endif

static u_int32_t
ath_net80211_wpsPushButton(struct ieee80211com *ic)
{
    struct ath_softc_net80211  *scn = ATH_SOFTC_NET80211(ic);
    struct ath_ops             *ops = scn->sc_ops;

    return (ops->have_capability(scn->sc_dev, ATH_CAP_WPS_BUTTON));
}


static struct ieee80211_tsf_timer *
ath_net80211_tsf_timer_alloc(struct ieee80211com *ic,
                            tsftimer_clk_id tsf_id,
                            ieee80211_tsf_timer_function trigger_action,
                            ieee80211_tsf_timer_function overflow_action,
                            ieee80211_tsf_timer_function outofrange_action,
                            void *arg)
{
#ifdef ATH_GEN_TIMER
    struct ath_softc_net80211   *scn = ATH_SOFTC_NET80211(ic);
    struct ath_gen_timer        *ath_timer;

    /* Note: If there is a undefined field, ath_tsf_timer_alloc, during compile, it is because ATH_GEN_TIMER undefined. */
    ath_timer = scn->sc_ops->ath_tsf_timer_alloc(scn->sc_dev, tsf_id, 
                                                 ATH_TSF_TIMER_FUNC(trigger_action), 
                                                 ATH_TSF_TIMER_FUNC(overflow_action),
                                                 ATH_TSF_TIMER_FUNC(outofrange_action),
                                                 arg);
    return NET80211_TSF_TIMER(ath_timer);
#else
    return NULL;

#endif
}

static void
ath_net80211_tsf_timer_free(struct ieee80211com *ic, struct ieee80211_tsf_timer *timer)
{
#ifdef ATH_GEN_TIMER
    struct ath_softc_net80211   *scn = ATH_SOFTC_NET80211(ic);
    struct ath_gen_timer        *ath_timer = ATH_TSF_TIMER(timer);

    scn->sc_ops->ath_tsf_timer_free(scn->sc_dev, ath_timer);
#endif
}

static void
ath_net80211_tsf_timer_start(struct ieee80211com *ic, struct ieee80211_tsf_timer *timer,
                            u_int32_t timer_next, u_int32_t period)
{
#ifdef ATH_GEN_TIMER
    struct ath_softc_net80211   *scn = ATH_SOFTC_NET80211(ic);
    struct ath_gen_timer        *ath_timer = ATH_TSF_TIMER(timer);

    scn->sc_ops->ath_tsf_timer_start(scn->sc_dev, ath_timer, timer_next, period);
#endif
}
    
static void
ath_net80211_tsf_timer_stop(struct ieee80211com *ic, struct ieee80211_tsf_timer *timer)
{
#ifdef ATH_GEN_TIMER
    struct ath_softc_net80211   *scn = ATH_SOFTC_NET80211(ic);
    struct ath_gen_timer        *ath_timer = ATH_TSF_TIMER(timer);

    scn->sc_ops->ath_tsf_timer_stop(scn->sc_dev, ath_timer);
#endif
}


#if UMAC_SUPPORT_P2P
static int
ath_net80211_reg_notify_tx_bcn(struct ieee80211com *ic,
                               ieee80211_tx_bcn_notify_func callback,
                               void *arg)
{
    struct ath_softc_net80211   *scn = ATH_SOFTC_NET80211(ic);
    int                         retval = 0;

    /* Note: if you get compile error for undeclared ath_reg_notify_tx_bcn, then it is because
       the ATH_SUPPORT_P2P compile flag is not enabled. */
    retval = scn->sc_ops->ath_reg_notify_tx_bcn(scn->sc_dev,
                                            ATH_BCN_NOTIFY_FUNC(callback), arg);
    return retval;
}
    
static int
ath_net80211_dereg_notify_tx_bcn(struct ieee80211com *ic)
{
    struct ath_softc_net80211   *scn = ATH_SOFTC_NET80211(ic);
    int                         retval = 0;

    retval = scn->sc_ops->ath_dereg_notify_tx_bcn(scn->sc_dev);
    return retval;
}
#endif  //UMAC_SUPPORT_P2P

static int
ath_net80211_reg_vap_info_notify(
    struct ieee80211vap                 *vap,
    ath_vap_infotype                    infotype_mask,
    ieee80211_vap_ath_info_notify_func  callback,
    void                                *arg)
{
    struct ath_softc_net80211   *scn = ATH_SOFTC_NET80211(vap->iv_ic);
    int                         retval = 0;

    /* 
     * Note: if you get compile error for undeclared ath_reg_vap_info_notify, 
     * then it is because the ATH_SUPPORT_P2P compile flag is not enabled.
     */
    retval = scn->sc_ops->ath_reg_notify_vap_info(scn->sc_dev,
                                            vap->iv_unit,
                                            infotype_mask,
                                            ATH_VAP_NOTIFY_FUNC(callback), arg);
    return retval;
}

static int
ath_net80211_vap_info_update_notify(
    struct ieee80211vap                 *vap,
    ath_vap_infotype                    infotype_mask)
{
    struct ath_softc_net80211   *scn = ATH_SOFTC_NET80211(vap->iv_ic);
    int                         retval = 0;

    /* 
     * Note: if you get compile error for undeclared ath_vap_info_update_notify, 
     * then it is because the ATH_SUPPORT_P2P compile flag is not enabled.
     */
    retval = scn->sc_ops->ath_vap_info_update_notify(scn->sc_dev,
                                            vap->iv_unit,
                                            infotype_mask);
    return retval;
}
    
static int
ath_net80211_dereg_vap_info_notify(
    struct ieee80211vap                 *vap)
{
    struct ath_softc_net80211   *scn = ATH_SOFTC_NET80211(vap->iv_ic);
    int                         retval = 0;

    retval = scn->sc_ops->ath_dereg_notify_vap_info(scn->sc_dev, vap->iv_unit);
    return retval;
}

static int
ath_net80211_vap_info_get(
    struct ieee80211vap *vap,
    ath_vap_infotype    infotype,
    u_int32_t           *param1,
    u_int32_t           *param2)
{
    struct ath_softc_net80211   *scn = ATH_SOFTC_NET80211(vap->iv_ic);
    int                         retval = 0;

    retval = scn->sc_ops->ath_vap_info_get(scn->sc_dev, vap->iv_unit,
                                           infotype, param1, param2);
    return retval;
}

#ifdef ATH_BT_COEX
static int
ath_get_bt_coex_info(struct ieee80211com *ic, u_int32_t infoType)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_ops->bt_coex_get_info(scn->sc_dev, infoType, NULL);
}
#endif

#if ATH_SLOW_ANT_DIV
static void
ath_net80211_antenna_diversity_suspend(struct ieee80211com *ic)
{
    struct ath_softc_net80211    *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_ops->ath_antenna_diversity_suspend(scn->sc_dev);
}

static void
ath_net80211_antenna_diversity_resume(struct ieee80211com *ic)
{
    struct ath_softc_net80211    *scn = ATH_SOFTC_NET80211(ic);

    scn->sc_ops->ath_antenna_diversity_resume(scn->sc_dev);
}
#endif

u_int32_t
ath_net80211_getmfpsupport(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return(scn->sc_ops->ath_get_mfpsupport(scn->sc_dev));
}
static void
ath_net80211_unref_node(struct ieee80211_node *ni)
{
            ieee80211_unref_node(&ni);
}

int
ath_attach(u_int16_t devid, void *base_addr,
           struct ath_softc_net80211 *scn,
           osdev_t osdev, struct ath_reg_parm *ath_conf_parm,
           struct hal_reg_parm *hal_conf_parm, IEEE80211_REG_PARAMETERS *ieee80211_conf_parm)
{
    static int              first = 1;
    ath_dev_t               dev;
    struct ath_ops          *ops;
    struct ieee80211com     *ic;
    int                     error;
    int                     weptkipaggr_rxdelim = 0;
    int                     channel_switching_time_usec = 4000;

    /*
     * Configure the shared asf_amem and asf_print instances with ADF
     * function pointers for mem alloc/free, lock/unlock, and print.
     * (Do this initialization just once.)
     */
    if (first) {
        static adf_os_spinlock_t asf_amem_lock;
        static adf_os_spinlock_t asf_print_lock;

        first = 0;

        adf_os_spinlock_init(&asf_amem_lock);
        asf_amem_setup(
            (asf_amem_alloc_fp) adf_os_mem_alloc_outline,
            (asf_amem_free_fp) adf_os_mem_free_outline,
            (void *) osdev,
            (asf_amem_lock_fp) adf_os_spin_lock_bh_outline,
            (asf_amem_unlock_fp) adf_os_spin_unlock_bh_outline,
            (void *) &asf_amem_lock);
        adf_os_spinlock_init(&asf_print_lock);
        asf_print_setup(
            (asf_vprint_fp) adf_os_vprint,
            (asf_print_lock_fp) adf_os_spin_lock_bh_outline,
            (asf_print_unlock_fp) adf_os_spin_unlock_bh_outline,
            (void *) &asf_print_lock);
    }
    /*
     * Also allocate our own dedicated asf_amem instance.
     * For now, this dedicated amem instance will be used by the
     * HAL's ath_hal_malloc.
     * Later this dedicated amem instance will be used throughout
     * the driver, rather than using the shared asf_amem instance.
     *
     * The platform-specific code that calls this ath_attach function
     * may have already set up an amem instance, if it had to do
     * memory allocation before calling ath_attach.  So, check if
     * scn->amem.handle is initialized already - if not, set it up here.
     */
    if (!scn->amem.handle) {
        adf_os_spinlock_init(&scn->amem.lock);
        scn->amem.handle = asf_amem_create(
            NULL, /* name */
            0,  /* no limit on allocations */
            (asf_amem_alloc_fp) adf_os_mem_alloc_outline,
            (asf_amem_free_fp) adf_os_mem_free_outline,
            (void *) osdev,
            (asf_amem_lock_fp) adf_os_spin_lock_bh_outline,
            (asf_amem_unlock_fp) adf_os_spin_unlock_bh_outline,
            (void *) &scn->amem.lock,
            NULL /* use adf_os_mem_alloc + osdev to alloc this amem object */);
        if (!scn->amem.handle) {
            adf_os_spinlock_destroy(&scn->amem.lock);
            return -ENOMEM;
        }
    }

    scn->sc_osdev = osdev;
    scn->sc_debug = DBG_DEFAULT;
#if UMAC_SUPPORT_P2P
    scn->sc_prealloc_idmask = (1 << ATH_P2PDEV_IF_ID);
#endif
    ic = &scn->sc_ic;
    ic->ic_osdev = osdev;
    ic->ic_debug = IEEE80211_DEBUG_DEFAULT;
#ifdef ATH_EXT_AP
    ic->ic_miroot = NULL;
#endif

    spin_lock_init(&ic->ic_lock);
    IEEE80211_STATE_LOCK_INIT(ic);
    /*
     * Create an Atheros Device object
     */
    error = ath_dev_attach(devid, base_addr,
                           ic, &net80211_ops, osdev,
                           &scn->sc_dev, &scn->sc_ops,
                           scn->amem.handle,
                           ath_conf_parm, hal_conf_parm);
    if (error != 0) return error;

    dev = scn->sc_dev;
    ops = scn->sc_ops;

    /* attach channel width management */
    error = ath_cwm_attach(scn, ath_conf_parm);
    if (error) {
        ath_dev_free(dev);
        return error;
    }

#ifdef ATH_AMSDU
    /* attach amsdu transmit handler */
    ath_amsdu_attach(scn);
#endif

    /* setup ieee80211 flags */
    ieee80211com_clear_cap(ic, -1);
    ieee80211com_clear_athcap(ic, -1);
    ieee80211com_clear_athextcap(ic, -1);

    /* XXX not right but it's not used anywhere important */
    ieee80211com_set_phytype(ic, IEEE80211_T_OFDM);

    /* 
     * Set the Atheros Advanced Capabilities from station config before 
     * starting 802.11 state machine.
     */
    ieee80211com_set_athcap(ic, (ops->have_capability(dev, ATH_CAP_BURST) ? IEEE80211_ATHC_BURST : 0));

    /* Set Atheros Extended Capabilities */
    ieee80211com_set_athextcap(ic,
        ((ops->have_capability(dev, ATH_CAP_HT) &&
          !ops->have_capability(dev, ATH_CAP_4ADDR_AGGR))
         ? IEEE80211_ATHEC_OWLWDSWAR : 0));
    ieee80211com_set_athextcap(ic,
        ((ops->have_capability(dev, ATH_CAP_HT) &&
          ops->have_capability(dev, ATH_CAP_WEP_TKIP_AGGR) &&
          ieee80211_conf_parm->htEnableWepTkip)
         ? IEEE80211_ATHEC_WEPTKIPAGGR : 0));

    /* set ic caps to require badba workaround */
    ieee80211com_set_athextcap(ic,
            (ops->have_capability(dev, ATH_CAP_EXTRADELIMWAR) &&
            ieee80211_conf_parm->htEnableWepTkip)
            ? IEEE80211_ATHEC_EXTRADELIMWAR: 0);

    ieee80211com_set_cap(ic,
                         IEEE80211_C_IBSS           /* ibss, nee adhoc, mode */
                         | IEEE80211_C_HOSTAP       /* hostap mode */
                         | IEEE80211_C_MONITOR      /* monitor mode */
                         | IEEE80211_C_SHPREAMBLE   /* short preamble supported */
                         | IEEE80211_C_SHSLOT       /* short slot time supported */
                         | IEEE80211_C_PMGT         /* capable of power management*/
                         | IEEE80211_C_WPA          /* capable of WPA1+WPA2 */
                         | IEEE80211_C_BGSCAN       /* capable of bg scanning */
        );

    /*
     * WMM enable
     */
    if (ops->have_capability(dev, ATH_CAP_WMM))
        ieee80211com_set_cap(ic, IEEE80211_C_WME);

    /* set up WMM AC to h/w qnum mapping */
    scn->sc_ac2q[WME_AC_BE] = ops->tx_get_qnum(dev, HAL_TX_QUEUE_DATA, HAL_WME_AC_BE);
    scn->sc_ac2q[WME_AC_BK] = ops->tx_get_qnum(dev, HAL_TX_QUEUE_DATA, HAL_WME_AC_BK);
    scn->sc_ac2q[WME_AC_VI] = ops->tx_get_qnum(dev, HAL_TX_QUEUE_DATA, HAL_WME_AC_VI);
    scn->sc_ac2q[WME_AC_VO] = ops->tx_get_qnum(dev, HAL_TX_QUEUE_DATA, HAL_WME_AC_VO);
    scn->sc_beacon_qnum = ops->tx_get_qnum(dev, HAL_TX_QUEUE_BEACON, 0);

    ath_uapsd_attach(scn);
    
    /*
     * Query the hardware to figure out h/w crypto support.
     */
    if (ops->has_cipher(dev, HAL_CIPHER_WEP))
        ieee80211com_set_cap(ic, IEEE80211_C_WEP);
    if (ops->has_cipher(dev, HAL_CIPHER_AES_OCB))
        ieee80211com_set_cap(ic, IEEE80211_C_AES);
    if (ops->has_cipher(dev, HAL_CIPHER_AES_CCM))
        ieee80211com_set_cap(ic, IEEE80211_C_AES_CCM);
    if (ops->has_cipher(dev, HAL_CIPHER_CKIP))
        ieee80211com_set_cap(ic, IEEE80211_C_CKIP);
    if (ops->has_cipher(dev, HAL_CIPHER_TKIP)) {
        ieee80211com_set_cap(ic, IEEE80211_C_TKIP);
#if ATH_SUPPORT_WAPI
    if (ops->has_cipher(dev, HAL_CIPHER_WAPI)){
        ieee80211com_set_cap(ic, IEEE80211_C_WAPI);
    }
#endif
        /* Check if h/w does the MIC. */
        if (ops->has_cipher(dev, HAL_CIPHER_MIC)) {
            ieee80211com_set_cap(ic, IEEE80211_C_TKIPMIC);
            /*
             * Check if h/w does MIC correctly when
             * WMM is turned on.  If not, then disallow WMM.
             */
            if (ops->have_capability(dev, ATH_CAP_TKIP_WMEMIC)) {
                ieee80211com_set_cap(ic, IEEE80211_C_WME_TKIPMIC);
            } else {
                ieee80211com_clear_cap(ic, IEEE80211_C_WME);
            }

            /*
             * Check whether the separate key cache entries
             * are required to handle both tx+rx MIC keys.
             * With split mic keys the number of stations is limited
             * to 27 otherwise 59.
             */
            if (ops->have_capability(dev, ATH_CAP_TKIP_SPLITMIC))
                scn->sc_splitmic = 1;
                DPRINTF(scn, ATH_DEBUG_KEYCACHE, "%s\n", __func__);
        }
    }

    if (ops->have_capability(dev, ATH_CAP_MCAST_KEYSEARCH))
        scn->sc_mcastkey = 1;

    /* TPC enabled */
    if (ops->have_capability(dev, ATH_CAP_TXPOWER))
        ieee80211com_set_cap(ic, IEEE80211_C_TXPMGT);

    spin_lock_init(&(scn->sc_keyixmap_lock));
    /*
     * Default 11.h to start enabled.
     */
    ieee80211_ic_doth_set(ic);

    /*Default wradar channel filtering is disabled  */
    ic->ic_no_weather_radar_chan = 0; 

    /* 11n Capabilities */
    ieee80211com_set_num_tx_chain(ic,1);
    ieee80211com_set_num_rx_chain(ic,1);
    ieee80211com_clear_htcap(ic, -1);
    ieee80211com_clear_htextcap(ic, -1);
    if (ops->have_capability(dev, ATH_CAP_HT)) {
        ieee80211com_set_cap(ic, IEEE80211_C_HT);
        ieee80211com_set_htcap(ic, IEEE80211_HTCAP_C_SHORTGI40
                        | IEEE80211_HTCAP_C_CHWIDTH40
                        | IEEE80211_HTCAP_C_DSSSCCK40);
        if (ops->have_capability(dev, ATH_CAP_HT20_SGI))
            ieee80211com_set_htcap(ic, IEEE80211_HTCAP_C_SHORTGI20);
        if (ops->have_capability(dev, ATH_CAP_DYNAMIC_SMPS)) {
            ieee80211com_set_htcap(ic, IEEE80211_HTCAP_C_SMPOWERSAVE_DYNAMIC);
        } else {
            ieee80211com_set_htcap(ic, IEEE80211_HTCAP_C_SM_ENABLED);
        }
        ieee80211com_set_htextcap(ic, IEEE80211_HTCAP_EXTC_TRANS_TIME_5000
                        | IEEE80211_HTCAP_EXTC_MCS_FEEDBACK_NONE);
        ieee80211com_set_roaming(ic, IEEE80211_ROAMING_AUTO);
        ieee80211com_set_maxampdu(ic, IEEE80211_HTCAP_MAXRXAMPDU_65536);
        if (ops->have_capability(dev, ATH_CAP_ZERO_MPDU_DENSITY)) {
            ieee80211com_set_mpdudensity(ic, IEEE80211_HTCAP_MPDUDENSITY_NA);
        } else {
            ieee80211com_set_mpdudensity(ic, IEEE80211_HTCAP_MPDUDENSITY_8);
        }
        IEEE80211_ENABLE_AMPDU(ic);
        

        if (!scn->sc_ops->ath_get_config_param(scn->sc_dev, ATH_PARAM_WEP_TKIP_AGGR_RX_DELIM,
                                           &weptkipaggr_rxdelim)) {
            ieee80211com_set_weptkipaggr_rxdelim(ic, (u_int8_t) weptkipaggr_rxdelim);
        }

        /* Fetch channel switching time parameter from ATH layer */
        scn->sc_ops->ath_get_config_param(scn->sc_dev, ATH_PARAM_CHANNEL_SWITCHING_TIME_USEC,
            &channel_switching_time_usec);
        ieee80211com_set_channel_switching_time_usec(ic, (u_int16_t) channel_switching_time_usec);
        
        ieee80211com_set_num_rx_chain(ic,
                  ops->have_capability(dev,  ATH_CAP_NUMRXCHAINS));
        ieee80211com_set_num_tx_chain(ic,
                  ops->have_capability(dev,  ATH_CAP_NUMTXCHAINS));
        
#ifdef ATH_SUPPORT_TxBF
        if (ops->have_capability(dev, ATH_CAP_TXBF)) {
            ath_set_txbfcapability(ic);
            //printk("==>%s:ATH have TxBF cap, set ie= %x \n",__func__,ic->ic_txbf.value);
        }
#endif         
        //ieee80211com_set_ampdu_limit(ic, ath_configuration_parameters.aggrLimit);
        //ieee80211com_set_ampdu_subframes(ic, ath_configuration_parameters.aggrSubframes);
    }

    if (ops->have_capability(dev, ATH_CAP_TX_STBC)) {
        ieee80211com_set_htcap(ic, IEEE80211_HTCAP_C_TXSTBC);
    }

    /* Rx STBC is a 2-bit mask. Needs to convert from ath definition to ieee definition. */ 
    ieee80211com_set_htcap(ic, IEEE80211_HTCAP_C_RXSTBC &
                           (ops->have_capability(dev, ATH_CAP_RX_STBC) << IEEE80211_HTCAP_C_RXSTBC_S));

    if (ops->have_capability(dev, ATH_CAP_LDPC)) {
        ieee80211com_set_htcap(ic, IEEE80211_HTCAP_C_ADVCODING);
    }

    /* 11n configuration */
    ieee80211com_clear_htflags(ic, -1);
    if (ieee80211com_has_htcap(ic, IEEE80211_HTCAP_C_SHORTGI40))
	    ieee80211com_set_htflags(ic, IEEE80211_HTF_SHORTGI40);
    if (ieee80211com_has_htcap(ic, IEEE80211_HTCAP_C_SHORTGI20)) 
	    ieee80211com_set_htflags(ic, IEEE80211_HTF_SHORTGI20);

    /*
     * Check for misc other capabilities.
     */
    if (ops->have_capability(dev, ATH_CAP_BURST))
        ieee80211com_set_cap(ic, IEEE80211_C_BURST);

    /* Set spatial streams */
    ath_set_spatialstream(scn);

    /*
     * Indicate we need the 802.11 header padded to a
     * 32-bit boundary for 4-address and QoS frames.
     */
    IEEE80211_ENABLE_DATAPAD(ic);

    /* get current mac address */
    ops->get_macaddr(dev, ic->ic_myaddr);

    /* get mac address from EEPROM */
    ops->get_hw_macaddr(dev, ic->ic_my_hwaddr);
#if ATH_SUPPORT_AP_WDS_COMBO
    /* Assume the LSB bits 0-2 of last byte in the h/w MAC address to be 0 always */
    KASSERT((ic->ic_my_hwaddr[IEEE80211_ADDR_LEN - 1] & 0x07) == 0, 
		    ("Last 3 bits of h/w MAC addr is non-zero: %s", ether_sprintf(ic->ic_my_hwaddr)));
#endif

    HTC_SET_NET80211_OPS_FUNC(net80211_ops, ath_net80211_find_tgt_node_index,
        ath_htc_wmm_update, ath_net80211_find_tgt_vap_index, ath_net80211_uapsd_creditupdate);

  #ifdef ATH_SUPPORT_HTC
    scn->sc_p2p_action_queue_head = NULL;
    scn->sc_p2p_action_queue_tail = NULL;
    IEEE80211_STATE_P2P_ACTION_LOCK_INIT(scn);
  #endif

    /* get default country info for 11d */
    ops->get_current_country(dev, (HAL_COUNTRY_ENTRY *)&ic->ic_country);
    
    /*
     * Setup some ieee80211com methods
     */
    ic->ic_mgtstart = ath_tx_mgt_send;
    ic->ic_init = ath_init;
    ic->ic_reset_start = ath_net80211_reset_start;
    ic->ic_reset = ath_net80211_reset;
    ic->ic_reset_end = ath_net80211_reset_end;
    ic->ic_newassoc = ath_net80211_newassoc;
    ic->ic_updateslot = ath_net80211_updateslot;

    ic->ic_wme.wme_update = ath_wmm_update;
    
    ic->ic_get_currentCountry = ath_net80211_get_currentCountry;
    ic->ic_set_country = ath_net80211_set_country;
    ic->ic_set_regdomain = ath_net80211_set_regdomain;
    ic->ic_set_quiet = ath_net80211_set_quiet;
    ic->ic_find_countrycode = ath_net80211_find_countrycode;

#ifdef ATH_SUPPORT_TxBF // For TxBF RC
#ifdef TXBF_TODO
    ic->ic_get_pos2_data = ath_net80211_get_pos2_data;
    ic->ic_txbfrcupdate = ath_net80211_txbfrcupdate; 
    ic->ic_ieee80211_send_cal_qos_nulldata = ieee80211_send_cal_qos_nulldata;
    ic->ic_ap_save_join_mac = ath_net80211_ap_save_join_mac;
    ic->ic_start_imbf_cal = ath_net80211_start_imbf_cal;
    ic->ic_csi_report_send = ath_net80211_CSI_Frame_send;
#endif
    ic->ic_ieee80211_find_node = ieee80211_find_node;
    ic->ic_ieee80211_unref_node = ath_net80211_unref_node;
    ic->ic_v_cv_send = ath_net80211_v_cv_send;
    ic->ic_txbf_alloc_key = ath_net80211_txbf_alloc_key;
    ic->ic_txbf_set_key = ath_net80211_txbf_set_key;
    ic->ic_init_sw_cv_timeout = ath_net80211_init_sw_cv_timeout;
	ic->ic_set_txbf_caps = ath_set_txbfcapability;
	ic->ic_txbf_stats_rpt_inc = ath_net80211_txbf_stats_rpt_inc;
    ic->ic_txbf_set_rpt_received = ath_net80211_txbf_set_rpt_received;
#endif

    ic->ic_beacon_update = ath_beacon_update;
    ic->ic_txq_depth = ath_net80211_txq_depth;
    ic->ic_txq_depth_ac = ath_net80211_txq_depth_ac;

    ic->ic_chwidth_change = ath_net80211_chwidth_change;
    ic->ic_sm_pwrsave_update = ath_net80211_sm_pwrsave_update;
    ic->ic_update_protmode = ath_net80211_update_protmode;
    ic->ic_set_config = ath_net80211_set_config;

    /* This section Must be before calling ieee80211_ifattach() */
    ic->ic_tsf_timer_alloc = ath_net80211_tsf_timer_alloc;
    ic->ic_tsf_timer_free = ath_net80211_tsf_timer_free;
    ic->ic_tsf_timer_start = ath_net80211_tsf_timer_start;
    ic->ic_tsf_timer_stop = ath_net80211_tsf_timer_stop;
    ic->ic_get_TSF32        = ath_net80211_gettsf32;
    ic->ic_get_TSF64        = ath_net80211_gettsf64;
#if UMAC_SUPPORT_P2P
    ic->ic_reg_notify_tx_bcn = ath_net80211_reg_notify_tx_bcn;
    ic->ic_dereg_notify_tx_bcn = ath_net80211_dereg_notify_tx_bcn;
#endif

    /* Setup Min frame size */
    ic->ic_minframesize = sizeof(struct ieee80211_frame_min);

    /*
     * Attach ieee80211com object to net80211 protocal stack.
     */
    ieee80211_ifattach(ic, ieee80211_conf_parm);

    /*
     * Override default methods
     */
    ic->ic_vap_create = ath_vap_create;
    ic->ic_vap_delete = ath_vap_delete;
    ic->ic_vap_alloc_macaddr = ath_vap_alloc_macaddr;
    ic->ic_vap_free_macaddr = ath_vap_free_macaddr;
    ic->ic_node_alloc = ath_net80211_node_alloc;
    scn->sc_node_free = ic->ic_node_free;
    ic->ic_node_free = ath_net80211_node_free;
    ic->ic_node_getrssi = ath_net80211_node_getrssi;
#if defined(MAGPIE_HIF_GMAC) || defined(MAGPIE_HIF_USB)
    ic->ic_node_getrate = ath_net80211_htc_node_getrate;
#else    
    ic->ic_node_getrate = ath_net80211_node_getrate;
#endif    
    ic->ic_node_psupdate = ath_net80211_node_ps_update;
    scn->sc_node_cleanup = ic->ic_node_cleanup;
    ic->ic_node_cleanup = ath_net80211_node_cleanup;

    ic->ic_scan_start = ath_net80211_scan_start;
    ic->ic_scan_end = ath_net80211_scan_end;
    ic->ic_led_scan_start = ath_net80211_led_enter_scan;
    ic->ic_led_scan_end = ath_net80211_led_leave_scan;
    ic->ic_set_channel = ath_net80211_set_channel;

    ic->ic_pwrsave_set_state = ath_net80211_pwrsave_set_state;
    
    ic->ic_mhz2ieee = ath_net80211_mhz2ieee;

    ic->ic_set_ampduparams = ath_net80211_set_ampduparams;
    ic->ic_set_weptkip_rxdelim = ath_net80211_set_weptkip_rxdelim;
    ic->ic_addba_requestsetup = ath_net80211_addba_requestsetup;
    ic->ic_addba_responsesetup = ath_net80211_addba_responsesetup;
    ic->ic_addba_requestprocess = ath_net80211_addba_requestprocess;
    ic->ic_addba_responseprocess = ath_net80211_addba_responseprocess;
    ic->ic_addba_clear = ath_net80211_addba_clear;
    ic->ic_delba_process = ath_net80211_delba_process;
    ic->ic_addba_send = ath_net80211_addba_send;
    ic->ic_addba_status = ath_net80211_addba_status;
    ic->ic_delba_send = ath_net80211_delba_send;
    ic->ic_addba_setresponse = ath_net80211_addba_setresponse;
    ic->ic_addba_clearresponse = ath_net80211_addba_clearresponse;
    ic->ic_get_noisefloor = ath_net80211_get_noisefloor;
    ic->ic_get_chainnoisefloor = ath_net80211_get_chainnoisefloor;
    ic->ic_set_txPowerLimit = ath_setTxPowerLimit;
    ic->ic_get_common_power = ath_net80211_get_common_power;
    ic->ic_get_maxphyrate = ath_net80211_get_maxphyrate;
    ic->ic_get_TSF32        = ath_getTSF32;
#ifdef ATH_CCX
    ic->ic_rmgetcounters = ath_getrmcounters;
    ic->ic_rmclearcounters = ath_clearrmcounters;
    ic->ic_rmupdatecounters = ath_updatermcounters;
    ic->ic_rcRateValueToPer = ath_net80211_rcRateValueToPer;
    ic->ic_get_TSF32        = ath_getTSF32;
    ic->ic_get_TSF64        = ath_getTSF64;
    ic->ic_set_rxfilter     = ath_setReceiveFilter;
    ic->ic_get_mfgsernum    = ath_getMfgSerNum;
    ic->ic_get_chandata     = ath_net80211_get_chanData;
    ic->ic_get_curRSSI      = ath_net80211_get_curRSSI;
#endif
#ifdef ATH_SWRETRY
    ic->ic_set_swretrystate = ath_net80211_set_swretrystate;
    ic->ic_handle_pspoll = ath_net80211_handle_pspoll;
    ic->ic_exist_pendingfrm_tidq = ath_net80211_exist_pendingfrm_tidq;
    ic->ic_reset_pause_tid = ath_net80211_reset_pasued_tid;
#endif
    ic->ic_get_wpsPushButton = ath_net80211_wpsPushButton;
    ic->ic_update_phystats = ath_update_phystats;
    ic->ic_clear_phystats = ath_clear_phystats;
    ic->ic_set_macaddr = ath_net80211_set_macaddr;
    ic->ic_log_text = ath_net80211_log_text;
    ic->ic_set_chain_mask = ath_net80211_set_chain_mask;
    ic->ic_need_beacon_sync = ath_net80211_need_beacon_sync;
    /* Functions for the green_ap feature */
    ic->ic_green_ap_set_enable = ath_net80211_green_ap_set_enable;
    ic->ic_green_ap_get_enable = ath_net80211_green_ap_get_enable;
    ic->ic_green_ap_set_transition_time = ath_net80211_green_ap_set_transition_time;
    ic->ic_green_ap_get_transition_time = ath_net80211_green_ap_get_transition_time;      
    ic->ic_green_ap_set_on_time = ath_net80211_green_ap_set_on_time;
    ic->ic_green_ap_get_on_time = ath_net80211_green_ap_get_on_time;
    ic->ic_get_cur_chan_nf = ath_net80211_get_cur_chan_noisefloor; 
#if ATH_SUPPORT_5G_EACS
    ic->ic_get_cur_chan_stats = ath_net80211_get_cur_chan_stats; 
#endif
#if ATH_SUPPORT_SPECTRAL
    /* EACS with spectral functions */
    ic->ic_get_control_duty_cycle = ath_net80211_get_control_duty_cycle; 
    ic->ic_get_extension_duty_cycle = ath_net80211_get_extension_duty_cycle; 
    ic->ic_start_spectral_scan = ath_net80211_start_spectral_scan; 
    ic->ic_stop_spectral_scan = ath_net80211_stop_spectral_scan; 
#endif
#ifdef ATH_BT_COEX
    ic->ic_get_bt_coex_info = ath_get_bt_coex_info;
#endif
    ic->ic_get_mfpsupport = ath_net80211_getmfpsupport;
#ifdef IEEE80211_DEBUG_NODELEAK
    ic->ic_print_nodeq_info = ath_net80211_debug_print_nodeq_info;
#endif
#if ATH_SUPPORT_IQUE
    ic->ic_set_acparams = ath_net80211_set_acparams;
    ic->ic_set_rtparams = ath_net80211_set_rtparams;
    ic->ic_get_iqueconfig = ath_net80211_get_iqueconfig;
	ic->ic_set_hbrparams = ath_net80211_set_hbrparams;
#endif
#if ATH_SLOW_ANT_DIV
    ic->ic_antenna_diversity_suspend = ath_net80211_antenna_diversity_suspend;
    ic->ic_antenna_diversity_resume = ath_net80211_antenna_diversity_resume;
#endif
    ic->ic_get_goodput = ath_net80211_get_goodput;

    ic->ic_get_ctl_by_country = ath_net80211_get_ctl_by_country;
    ic->ic_dfs_isdfsregdomain = ath_net80211_dfs_isdfsregdomain;
    ic->ic_dfs_in_cac = ath_net80211_dfs_in_cac;
    ic->ic_dfs_usenol = ath_net80211_dfs_usenol;
    ic->ic_get_dfsdomain  = ath_net80211_getdfsdomain;

    /* The following functions are commented to avoid, comile
     * error
     *- KARTHI
     * Once the references to ath_net80211_enable_tpc and
     * ath_net80211_get_max_txpwr are restored, the function
     * declarations and definitions should be changed back to static.
     */
    //ic->ic_enable_hw_tpc = ath_net80211_enable_tpc;
    //ic->ic_get_tx_maxpwr = ath_net80211_get_max_txpwr;

    ic->ic_vap_pause_control = ath_net80211_vap_pause_control;
    ic->ic_enable_rifs_ldpcwar = ath_net80211_enablerifs_ldpcwar;    
    IEEE80211_HTC_SET_IC_CALLBACK(ic);
#if UMAC_SUPPORT_VI_DBG
    ic->ic_set_vi_dbg_restart = ath_net80211_set_vi_dbg_restart;
    ic->ic_set_vi_dbg_log     = ath_net80211_set_vi_dbg_log;
#endif
#if UMAC_SUPPORT_SMARTANTENNA
    ic->ic_prepare_rateset = ath_net80211_prepare_rateset;
    ic->ic_get_txbuf_free = ath_net80211_get_txbuf_free;
    ic->ic_get_txbuf_used = ath_net80211_get_txbuf_used;
    ic->ic_get_smartatenna_enable = ath_net80211_get_smartantenna_enable;
    ic->ic_get_smartantenna_ratestats = ath_net80211_get_smartantenna_ratestats;
#endif   
    ic->ic_scan_enable_txq = ath_net80211_scan_enable_txq; 
#if UMAC_SUPPORT_INTERNALANTENNA    
    ic->ic_set_default_antenna = ath_net80211_set_default_antenna;
    ic->ic_set_selected_smantennas = ath_net80211_set_selected_smantennas;
    ic->ic_get_default_antenna = ath_net80211_get_default_antenna;
#endif    
    /* Attach AoW module */
    ath_aow_attach(ic, scn);

    return 0;
}

int
ath_detach(struct ath_softc_net80211 *scn)
{
    struct ieee80211com *ic = &scn->sc_ic;
    
    ieee80211_stop_running(ic);
    spin_lock_destroy(&(scn->sc_keyixmap_lock));
    
    /*
     * NB: the order of these is important:
     * o call the 802.11 layer before detaching the hal to
     *   insure callbacks into the driver to delete global
     *   key cache entries can be handled
     * o reclaim the tx queue data structures after calling
     *   the 802.11 layer as we'll get called back to reclaim
     *   node state and potentially want to use them
     * o to cleanup the tx queues the hal is called, so detach
     *   it last
     * Other than that, it's straightforward...
     */
    ieee80211_ifdetach(ic);
    ath_cwm_detach(scn);
#ifdef ATH_AMSDU
    ath_amsdu_detach(scn);
#endif

  #ifdef ATH_SUPPORT_HTC
    IEEE80211_STATE_P2P_ACTION_LOCK_IRQ(scn);
    while (scn->sc_p2p_action_queue_head) {
        struct ath_usb_p2p_action_queue *p2p_action_queue_head;
        p2p_action_queue_head = scn->sc_p2p_action_queue_head;
        scn->sc_p2p_action_queue_head = scn->sc_p2p_action_queue_head->next;
        wbuf_release(scn->sc_osdev, p2p_action_queue_head->wbuf);
        OS_FREE(p2p_action_queue_head);
    }
    scn->sc_p2p_action_queue_head = NULL;
    scn->sc_p2p_action_queue_tail = NULL;
    IEEE80211_STATE_P2P_ACTION_UNLOCK_IRQ(scn);
    IEEE80211_STATE_P2P_ACTION_LOCK_DESTROY(scn);
  #endif

    ath_dev_free(scn->sc_dev);
    scn->sc_dev = NULL;
    scn->sc_ops = NULL;

    adf_os_spinlock_destroy(&scn->amem.lock);
    asf_amem_destroy(scn->amem.handle, NULL);
    scn->amem.handle = NULL;

    return 0;
}

/*
 * When the external power to our chip is switched off, the entire keycache memory
 * contains random values. This can happen when the system goes to hibernate.
 * We should re-initialized the keycache to prevent unwanted behavior.
 * 
 */
static void
ath_clear_keycache(struct ath_softc_net80211 *scn)
{
    int i;
    struct ath_softc *sc = scn->sc_dev;
    struct ath_hal *ah = sc->sc_ah;

    for (i = 0; i < sc->sc_keymax; i++) {
        ath_hal_keyreset(ah, (u_int16_t)i);
    }
}

int
ath_resume(struct ath_softc_net80211 *scn)
{
    /*
     * ignore if already resumed.
     */
    if (OS_ATOMIC_CMPXCHG(&(scn->sc_dev_enabled), 0, 1) == 1) return 0; 

    ATH_CLEAR_KEYCACHE(scn);
    return ath_init(&scn->sc_ic);
}

int
ath_suspend(struct ath_softc_net80211 *scn)
{
#ifdef ATH_SUPPORT_LINUX_STA
    struct ieee80211com *ic = &scn->sc_ic;
    int i=0;
#endif
    /*
     * ignore if already suspended;
     */
    if (OS_ATOMIC_CMPXCHG(&(scn->sc_dev_enabled), 1, 0) == 0) return 0; 

    /* stop protocol stack first */
    ieee80211_stop_running(&scn->sc_ic);
    cwm_stop(&scn->sc_ic);

#ifdef ATH_SUPPORT_LINUX_STA
    /*
     * Stopping hardware in SCAN state may cause driver hang up and device malfuicntion
     * Set IEEE80211_SIWSCAN_TIMEOUT as maximum delay 
     */
    while ((i < IEEE80211_SIWSCAN_TIMEOUT) && (ic->ic_flags & IEEE80211_F_SCAN))
    {
        OS_SLEEP(1000);
        i++;
    }
#endif

    return (*scn->sc_ops->stop)(scn->sc_dev);
}

static void ath_net80211_log_text(struct ieee80211com *ic, char *text)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->log_text(scn->sc_dev, text);
}


static bool ath_net80211_need_beacon_sync(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_syncbeacon;
}

#if ATH_SUPPORT_CFEND
wbuf_t
ath_net80211_cfend_alloc(ieee80211_handle_t ieee)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);

    return  ieee80211_cfend_alloc(ic);
}


#endif
#ifdef IEEE80211_DEBUG_NODELEAK
static void ath_net80211_debug_print_nodeq_info(struct ieee80211_node *ni)
{
#ifdef AR_DEBUG
    struct ieee80211com *ic = ni->ni_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    ath_node_t node = ATH_NODE_NET80211(ni)->an_sta;
    scn->sc_ops->node_queue_stats(scn->sc_dev, node);
#endif
}
#endif

static u_int32_t ath_net80211_gettsf32(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_ops->ath_get_tsf32(scn->sc_dev);
}

static u_int64_t ath_net80211_gettsf64(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_ops->ath_get_tsf64(scn->sc_dev);
}

struct pause_control_data {
    struct ieee80211vap *vap;
    struct ieee80211com *ic;
    bool  pause;
};

/* pause control iterator function */
static void
pause_control_cb(void *arg, struct ieee80211_node *ni)
{
    struct pause_control_data *pctrl_data = (struct pause_control_data *)arg;
    ath_node_t node = ATH_NODE_NET80211(ni)->an_sta;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(pctrl_data->ic);
    if (pctrl_data->vap == NULL || pctrl_data->vap == ni->ni_vap) {
        if (!((pctrl_data->pause !=0) ^ ((ni->ni_flags & IEEE80211_NODE_ATH_PAUSED) != 0))) {
            return;
        }
        if (pctrl_data->pause) {
           ni->ni_flags |= IEEE80211_NODE_ATH_PAUSED;
        } else {
           ni->ni_flags &= ~IEEE80211_NODE_ATH_PAUSED;
        }
        ath_net80211_uapsd_pause_control(ni, pctrl_data->pause);
        scn->sc_ops->ath_node_pause_control(scn->sc_dev, node, pctrl_data->pause);
    }
}
/*
 * pause/unpause vap(s).
 * if pause is true then perform pause operation.     
 * if pause is false then perform unpause operation.
 * if vap is null the performa the requested operation on all the vaps.
 * if vap is non null the performa the requested operation on the vap.
 * part of the vap pause , pause all the nodes and call into ath layer to
 * pause the data on the HW queue.
 * part of the vap unpause , unpause all the nodes.
 */
static int ath_net80211_vap_pause_control (struct ieee80211com *ic, struct ieee80211vap *vap, bool pause)
{

    struct ath_vap_net80211 *avn;
    int if_id=ATH_IF_ID_ANY;
    struct pause_control_data pctrl_data;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    if (vap) {
        avn = ATH_VAP_NET80211(vap);
        if_id = avn->av_if_id; 
    }

    pctrl_data.vap = vap;
    pctrl_data.ic = ic;
    pctrl_data.pause = pause;
   /* 
    * iterate all the nodes and issue the pause/unpause request.
    */
    ieee80211_iterate_node(ic, pause_control_cb, &pctrl_data );

    /* pause the vap data on the txq */
#ifdef ATH_SUPPORT_HTC
    ASSERT(scn->sc_ops->ath_wmi_pause_ctrl);
#ifdef not_yet
    if (vap) {
        struct ieee80211_node *ni = ieee80211vap_get_bssnode(vap);
       
        if (ni)
            scn->sc_ops->ath_wmi_pause_ctrl(scn->sc_dev, ((struct ath_node_net80211 *)ni)->an_sta, pause);
    }
#endif    
#else
    scn->sc_ops->ath_vap_pause_control(scn->sc_dev, if_id, pause);
#endif

    return 0;

}

#ifdef ATH_TX99_DIAG
static struct ieee80211_channel *  
ath_net80211_find_channel(struct ath_softc *sc, int ieee, enum ieee80211_phymode mode)
{
    struct ieee80211com *ic = (struct ieee80211com *)sc->sc_ieee;
    return ieee80211_find_dot11_channel(ic, ieee, mode);
}
#endif

/*
 * disable rxrifs if STA,AP has both rifs and ldpc enabled.
 * ic_ldpcsta_assoc used to count ldpc sta associated and enable
 * rxrifs back if no ldpc sta is associated.
 */

static void ath_net80211_enablerifs_ldpcwar(struct ieee80211_node *ni, bool value)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211vap *vap = ni->ni_vap;    
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    if (0 == scn->sc_ops->have_capability(scn->sc_dev, ATH_CAP_LDPCWAR))
    {
       if (vap->iv_opmode == IEEE80211_M_HOSTAP)
       {
            if (!value)
                ic->ic_ldpcsta_assoc++;
            else
                ic->ic_ldpcsta_assoc--;

            if (ic->ic_ldpcsta_assoc)
                value = 0;
            else
                value = 1;
       }
       scn->sc_ops->set_rifs(scn->sc_dev, value);    
    }
}

static void
ath_net80211_set_stbcconfig(ieee80211_handle_t ieee, u_int8_t stbc, u_int8_t istx)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    u_int16_t htcap_flag = 0;
    
    if(istx) {
        htcap_flag = IEEE80211_HTCAP_C_TXSTBC;
    } else {
        struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
        struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
        u_int32_t supported = 0;

        if (ath_hal_rxstbcsupport(sc->sc_ah, &supported)) {
            htcap_flag = IEEE80211_HTCAP_C_RXSTBC & (supported << IEEE80211_HTCAP_C_RXSTBC_S);
        }
    }

    if(stbc) {
        ieee80211com_set_htcap(ic, htcap_flag);
    } else {
        ieee80211com_clear_htcap(ic, htcap_flag);
    }
}

#if UMAC_SUPPORT_VI_DBG
static void ath_net80211_set_vi_dbg_restart(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    if (scn->sc_ops->ath_set_vi_dbg_restart) {
        scn->sc_ops->ath_set_vi_dbg_restart(scn->sc_dev);
    }
}

static void ath_net80211_set_vi_dbg_log(struct ieee80211com *ic, bool enable)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    if (scn->sc_ops->ath_set_vi_dbg_log) {
        scn->sc_ops->ath_set_vi_dbg_log(scn->sc_dev, enable);
    }
}
#endif

#if UMAC_SUPPORT_SMARTANTENNA

static void ath_net80211_prepare_rateset(struct ieee80211com *ic, struct ieee80211_node *ni)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_node_net80211 *an = (struct ath_node_net80211 *)ni;
    if (scn->sc_ops->smartant_prepare_rateset) {
        scn->sc_ops->smartant_prepare_rateset(scn->sc_dev, an->an_sta, &ni->rtset);
    }
}


static u_int32_t ath_net80211_get_txbuf_free(struct ieee80211com *ic)
{
     struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
     if (scn->sc_ops->get_txbuf_free) {
     return scn->sc_ops->get_txbuf_free(scn->sc_dev);
     }
     return 0;   
}

static u_int32_t ath_net80211_get_txbuf_used(struct ieee80211com *ic, int ac)
{
     struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
     if (scn->sc_ops->get_txbuf_used) {
     return scn->sc_ops->get_txbuf_used(scn->sc_dev , scn->sc_ac2q[ac]);
     }
     return 0;   
}  

static int8_t ath_net80211_get_smartantenna_enable(struct ieee80211com *ic)
{
     struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
     if (scn->sc_ops->get_smartantenna_enable) {
        return scn->sc_ops->get_smartantenna_enable(scn->sc_dev);
     }
     return 0;   
}

static void ath_net80211_update_smartant_pertable (ieee80211_node_t node, void *pertab)
{
     struct ieee80211_node *ni = (struct ieee80211_node *)node;
     struct node_smartant_per *tab = (struct node_smartant_per *)pertab;
     ni->permap[tab->antenna][(tab->ratecode&0x7f)].nframes += tab->nFrames;
     ni->permap[tab->antenna][(tab->ratecode&0x7f)].nbad += tab->nBad;
     if ((tab->chain0_rssi >0) && (tab->chain0_rssi < 127))
     {
         ni->permap[(tab->antenna)][(tab->ratecode&0x7f)].chain0_rssi = tab->chain0_rssi;
         ni->permap[(tab->antenna)][(tab->ratecode&0x7f)].chain1_rssi = tab->chain1_rssi;
         ni->permap[(tab->antenna)][(tab->ratecode&0x7f)].chain2_rssi = tab->chain2_rssi;
     }
     /* 
      * TODO: vicks, while averaging got some problem ; and values are over written
      * needed to debug further
      */

}

static void ath_net80211_get_smartantenna_ratestats(struct ieee80211_node *ni, void *rate_stats)
{
     struct ieee80211com *ic = ni->ni_ic;
     struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
     struct ath_node_net80211 *an = (struct ath_node_net80211 *)ni;
     if (scn->sc_ops->get_smartant_ratestats) {
         scn->sc_ops->get_smartant_ratestats(an->an_sta, rate_stats);
     }
}

#endif    

#if UMAC_SUPPORT_INTERNALANTENNA

static u_int32_t ath_net80211_get_default_antenna(struct ieee80211com *ic)
{
     struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
     if (scn->sc_ops->get_default_antenna) {
     return scn->sc_ops->get_default_antenna(scn->sc_dev);
     }
     return 0;   
}  

static void ath_net80211_set_default_antenna(struct ieee80211com *ic, u_int32_t antenna)
{
     struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
     if (scn->sc_ops->set_defaultantenna) {
        scn->sc_ops->set_defaultantenna(scn->sc_dev, antenna);
     }

}

static void ath_net80211_set_selected_smantennas(struct ieee80211_node *ni, int antenna, int rateset)
{
     struct ieee80211com *ic = ni->ni_ic;
     struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
     struct ath_node_net80211 *an = (struct ath_node_net80211 *)ni;
     if (scn->sc_ops->set_selected_smantennas) {
         scn->sc_ops->set_selected_smantennas(an->an_sta, antenna, rateset);
     }
}
#endif

static void
ath_net80211_set_ldpcconfig(ieee80211_handle_t ieee, u_int8_t ldpc)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    u_int16_t htcap_flag = IEEE80211_HTCAP_C_ADVCODING;
    u_int32_t supported = 0;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
 
    if(ldpc && ath_hal_ldpcsupport(sc->sc_ah, &supported)) {
        ieee80211com_set_htcap(ic, htcap_flag);
    } else {
        ieee80211com_clear_htcap(ic, htcap_flag);
    }
} 
