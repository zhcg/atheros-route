/*****************************************************************************/
/* \file ath_main.c
** \brief Main Rx/Tx code
**
**  This file contains the main implementation of the ATH layer.  Most member
**  functions of the ATH layer are defined here.
**
** Copyright (c) 2009, Atheros Communications Inc.
**
** Permission to use, copy, modify, and/or distribute this software for any
** purpose with or without fee is hereby granted, provided that the above
** copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
** WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
** ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
** WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
** ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
** OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
**
*/

#include "wlan_opts.h"
#include "ath_internal.h"
#include "osdep.h"
#include "ath_green_ap.h"
#include "dfs_ioctl.h"
#include "ratectrl.h"
#include "ath_aow.h"

#include "ath_lmac_state_event.h"

#ifdef ATH_SUPPORT_DFS
#include "dfs.h"
#endif

#ifdef ATH_TX99_DIAG
#include "ath_tx99.h"
#endif

#if ATH_SUPPORT_SPECTRAL
#include "spectral.h"
int ath_dev_get_ctldutycycle(ath_dev_t dev);
int ath_dev_get_extdutycycle(ath_dev_t dev);
#endif

#if ATH_RESET_SERIAL
int
_ath_tx_txqaddbuf(struct ath_softc *sc, struct ath_txq *txq, ath_bufhead *head);
#endif

#if ATH_SUPPORT_RAW_ADC_CAPTURE
#include "spectral_raw_adc_ioctl.h"
#endif

#ifndef REMOVE_PKT_LOG
#include "pktlog.h"
struct ath_pktlog_funcs *g_pktlog_funcs = NULL;
#endif

#ifdef ATH_SUPPORT_TxBF
#include "ath_txbf.h"
#endif

#if ATH_SUPPORT_FLOWMAC_MODULE
#include <flowmac_api.h>
#endif

#if ATH_SUPPORT_VOW_DCS
#define INTR_DETECTION_CNT 5
#define COCH_INTR_THRESH 30 
#define TXERR_THRESH 30
#define PHYERR_RATE_THRESH 300  /* Phyerr per sec */
#define RADARERR_THRESH    1000  /* radar error per sec */
#endif

/* Global configuration overrides */
static    const int countrycode = -1;
static    const int xchanmode = -1;
static    const int ath_outdoor = AH_FALSE;        /* enable outdoor use */
static    const int ath_indoor  = AH_FALSE;        /* enable indoor use  */
/*
 * Chainmask to numchains mapping.
 */
static    u_int8_t ath_numchains[8] = { 
    0 /* 000 */,
    1 /* 001 */,
    1 /* 010 */,
    2 /* 011 */,
    1 /* 100 */, 
    2 /* 101 */, 
    2 /* 110 */, 
    3 /* 111 */ 
};

const u_int8_t ath_bcast_mac[IEEE80211_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};


#ifdef ATH_CHAINMASK_SELECT
u_int32_t   ath_chainmask_sel_up_rssi_thres = ATH_CHAINMASK_SEL_UP_RSSI_THRES;
u_int32_t   ath_chainmask_sel_down_rssi_thres = ATH_CHAINMASK_SEL_DOWN_RSSI_THRES;
u_int32_t   ath_chainmask_sel_period = ATH_CHAINMASK_SEL_TIMEOUT;

static void ath_chainmask_sel_timerstart(struct ath_chainmask_sel *cm);
static void ath_chainmask_sel_timerstop(struct ath_chainmask_sel *cm);
static void ath_chainmask_sel_init(struct ath_softc *sc, struct ath_node *an);
#endif

#ifdef MAGPIE_HIF_GMAC
extern void ath_wmi_seek_credit(struct ath_softc *sc);
extern os_timer_t host_seek_credit_timer;
OS_TIMER_FUNC (ath_host_seek_credit_timer);
#endif

#ifdef ATH_SWRETRY
extern void ath_tx_flush_node_sxmitq(struct ath_softc *sc, struct ath_node *an);
#endif

#ifdef ATH_C3_WAR
extern int  ath_c3_war_handler(void *arg);
#endif

static void ath_printreg(ath_dev_t dev, u_int32_t printctrl);

static u_int32_t ath_getmfpsupport(ath_dev_t dev);

/*
** Internal Prototypes
*/

#if WAR_DELETE_VAP
static int ath_vap_attach(ath_dev_t dev, int if_id, ieee80211_if_t if_data, HAL_OPMODE opmode, HAL_OPMODE iv_opmode, int nostabeacons, void **ret_avp);
static int  ath_vap_detach(ath_dev_t dev, int if_id, void *athvap);
#else
static int ath_vap_attach(ath_dev_t dev, int if_id, ieee80211_if_t if_data, HAL_OPMODE opmode, HAL_OPMODE iv_opmode, int nostabeacons);
static int ath_vap_detach(ath_dev_t dev, int if_id);
#endif
static int ath_vap_config(ath_dev_t dev, int if_id, struct ath_vap_config *if_config);

static ath_node_t ath_node_attach(ath_dev_t dev, int if_id, ieee80211_node_t ni, bool tmpnode);
static void ath_node_detach(ath_dev_t dev, ath_node_t an);
static void ath_node_cleanup(ath_dev_t dev, ath_node_t an);
static void ath_node_update_pwrsave(ath_dev_t dev, ath_node_t node, int pwrsave, int pause_resume);

/* Key cache interfaces */
static u_int16_t ath_key_alloc_2pair(ath_dev_t);
static u_int16_t ath_key_alloc_pair(ath_dev_t);
static u_int16_t ath_key_alloc_single(ath_dev_t);
static void ath_key_reset(ath_dev_t, u_int16_t keyix, int freeslot);
static int ath_keyset(ath_dev_t, u_int16_t keyix, HAL_KEYVAL *hk,
                      const u_int8_t mac[IEEE80211_ADDR_LEN]);
static u_int ath_keycache_size(ath_dev_t);

static u_int ath_get_wirelessmodes(ath_dev_t dev);
static u_int32_t ath_get_device_info(ath_dev_t dev, u_int32_t type);

/* regdomain interfaces */
static int ath_set_country(ath_dev_t, char *isoName, u_int16_t, enum ieee80211_clist_cmd);
static void ath_get_currentCountry(ath_dev_t, HAL_COUNTRY_ENTRY *ctry);
static int ath_set_regdomain(ath_dev_t, int regdomain, HAL_BOOL);
static int ath_get_regdomain(ath_dev_t dev);
static int ath_get_dfsdomain(ath_dev_t dev);
static int ath_set_quiet(ath_dev_t, u_int16_t period, u_int16_t duration,
                         u_int16_t nextStart, u_int16_t enabled);
static u_int16_t ath_find_countrycode(ath_dev_t, char* isoName);
static u_int8_t ath_get_ctl_by_country(ath_dev_t, u_int8_t *country, HAL_BOOL is2G);
static u_int16_t ath_dfs_isdfsregdomain(ath_dev_t);
static u_int16_t ath_dfs_in_cac(ath_dev_t);
static u_int16_t ath_dfs_usenol(ath_dev_t);

static int ath_set_tx_antenna_select(ath_dev_t, u_int32_t antenna_select);
static u_int32_t ath_get_current_tx_antenna(ath_dev_t);
static u_int32_t ath_get_default_antenna(ath_dev_t);
static int16_t ath_get_noisefloor(ath_dev_t dev, u_int16_t    freq,  u_int chan_flags);
static void ath_get_chainnoisefloor(ath_dev_t dev, u_int16_t    freq,  u_int chan_flags, int16_t *nfBuf);

#if ATH_SUPPORT_RAW_ADC_CAPTURE
static int ath_get_min_agc_gain(ath_dev_t dev, u_int16_t freq, int32_t *gain_buf, int num_gain_buf);
#endif

static void ath_update_sm_pwrsave(ath_dev_t dev, ath_node_t node, ATH_SM_PWRSAV mode,
    int ratechg);
/* Rate control interfaces */
static u_int32_t ath_node_gettxrate(ath_node_t node);
void ath_node_set_fixedrate(ath_node_t node, u_int8_t fixed_ratecode);
static u_int32_t ath_node_getmaxphyrate(ath_dev_t dev, ath_node_t node,uint32_t shortgi);
static int athop_rate_newassoc(ath_dev_t dev, ath_node_t node, int isnew, unsigned int capflag,
                                  struct ieee80211_rateset *negotiated_rates,
                                  struct ieee80211_rateset *negotiated_htrates);
static void
athop_rate_newstate(ath_dev_t dev, int if_id, int up);
#ifdef DBG
static u_int32_t ath_register_read(ath_dev_t dev, u_int32_t offset);
static void ath_register_write(ath_dev_t dev, u_int32_t offset, u_int32_t value);
#endif

void ath_do_pwrworkaround(ath_dev_t dev, u_int16_t channel,  u_int32_t channel_flags, u_int16_t opmodeSta);
void ath_get_txqproperty(ath_dev_t dev, u_int32_t q_id,  u_int32_t property, void *retval);
void ath_set_txqproperty(ath_dev_t dev, u_int32_t q_id,  u_int32_t property, void *value);
void ath_get_hwrevs(ath_dev_t dev, struct ATH_HW_REVISION *hwrev);
u_int32_t ath_rc_rate_maprix(ath_dev_t dev, u_int16_t curmode, int isratecode);
int ath_rc_rate_set_mcast_rate(ath_dev_t dev, u_int32_t req_rate);
void ath_set_diversity(ath_dev_t dev, u_int diversity);
void ath_set_rx_chainmask(ath_dev_t dev, u_int8_t mask);
void ath_set_tx_chainmask(ath_dev_t dev, u_int8_t mask);
void ath_set_rx_chainmasklegacy(ath_dev_t dev, u_int8_t mask);
void ath_set_tx_chainmasklegacy(ath_dev_t dev, u_int8_t mask);
void ath_get_maxtxpower(ath_dev_t dev, u_int32_t *txpow);
void ath_read_from_eeprom(ath_dev_t dev, u_int16_t address, u_int32_t len, u_int16_t **value, u_int32_t *bytesread);
int ath_rate_newstate_set(ath_dev_t dev , int index, int up);
static void  ath_vap_pause_control(ath_dev_t dev,int if_id, bool pause );
static void  ath_node_pause_control(ath_dev_t dev,ath_node_t node, bool pause );
static void  ath_set_rifs(ath_dev_t dev, bool enable);

#if ATH_SUPPORT_CFEND
extern int ath_cfendq_config(struct ath_softc *sc);
#endif

static inline int ath_hw_tx_hang_check(struct ath_softc *sc);

#ifdef ATH_SUPPORT_DFS
static OS_TIMER_FUNC(ath_radar_task);
static OS_TIMER_FUNC(ath_dfs_war_task);
static OS_TIMER_FUNC(ath_check_dfs_clear);
static OS_TIMER_FUNC(ath_dfs_test_return);
static OS_TIMER_FUNC(ath_dfs_hang_war)
{
        struct ath_softc *sc;
        OS_GET_TIMER_ARG(sc, struct ath_softc *);
        sc->sc_dfs_hang.hang_war_activated=0;
        sc->sc_ieee_ops->ath_net80211_switch_mode_dynamic2040(sc->sc_ieee);
}
#if ATH_SUPPORT_IBSS_DFS
static void ath_ibss_beacon_update_start(ath_dev_t dev);
static void ath_ibss_beacon_update_stop(ath_dev_t dev);
#endif /* ATH_SUPPORT_IBSS_DFS */
#endif

#if UMAC_SUPPORT_SMARTANTENNA
static void ath_smartant_prepare_rateset(ath_dev_t dev, ath_node_t node, void *prtset);
static u_int32_t ath_get_txbuf_free(ath_dev_t dev);
static u_int32_t ath_get_txbuf_used(ath_dev_t dev, int ac);
static int8_t ath_get_smartantenna_enable(ath_dev_t dev);
static void ath_get_smartant_ratestats(ath_node_t node, void *rate_stats);
#endif
#if UMAC_SUPPORT_INTERNALANTENNA
static void ath_set_selected_smantennas(ath_node_t node, int antenna, int rtset);
#endif

/******************************************************************************/
/*!
**  Wait for other threads that may access sc to finish
**
**  \param timeout      maximum wait time in milli-second.
**
**  \return -EIO if timeout
**  \return  0 on success
*/
int
ath_wait_sc_inuse_cnt(struct ath_softc *sc, u_int32_t timeout)
{
    u_int32_t        elapsed_time = 0;

    /* convert timeout to usec */
    timeout = timeout * 1000;

    /* Wait for any hardware access to complete */
#define WAIT_SC_INTERVAL 100
    while (atomic_read(&sc->sc_inuse_cnt)){
        OS_DELAY(WAIT_SC_INTERVAL);
        elapsed_time += WAIT_SC_INTERVAL;

        if (elapsed_time > timeout) {
           printk("%s: sc_inuse count(%d) stuck after waiting for %d msec. Investigate!!!\n",
                  __func__, atomic_read(&sc->sc_inuse_cnt), timeout);
           return -EIO;
        }
    }
#undef WAIT_SC_INTERVAL

    return 0;
}



/******************************************************************************/
/*!
**  \brief Expand time stamp to TSF
**
**  Extend stamp from rx descriptor to
**  a full 64-bit TSF using the current h/w TSF.
**  The bit-width of the timestamp field is fetched from the HAL capabilities.
**
**  \param sc Pointer to ATH object (this)
**  \param rstamp Time stamp value
**
**  \return 64 bit TSF (Timer Synchronization Function) value
*/
u_int64_t
ath_extend_tsf(struct ath_softc *sc, u_int32_t rstamp)
{
    u_int64_t tsf;
    u_int32_t desc_timestamp_bits; 
    u_int32_t timestamp_mask = 0;
    u_int64_t timestamp_period = 0;

    /* Chips before Ar5416 contained 15 bits of timestamp in the Descriptor */
    desc_timestamp_bits = 15;
    
    ath_hal_getcapability(sc->sc_ah, HAL_CAP_RXDESC_TIMESTAMPBITS, 0, &desc_timestamp_bits);

    if (desc_timestamp_bits < 32) {
        timestamp_period = ((u_int64_t)1 << desc_timestamp_bits);
        timestamp_mask = timestamp_period - 1;
    }
    else {
        timestamp_period = (((u_int64_t) 1) << 32);
        timestamp_mask = 0xffffffff;
    }
    
    tsf = ath_hal_gettsf64(sc->sc_ah);

    /* Check to see if the TSF value has wrapped-around after the descriptor timestamp */    
    if ((tsf & timestamp_mask) < (rstamp & timestamp_mask)) {
        tsf -= timestamp_period;
    }
    
    return ((tsf &~ (u_int64_t) timestamp_mask) | (u_int64_t) (rstamp & timestamp_mask));
}

/******************************************************************************/
/*!
**  \brief Set current operating mode
**
**  This function initializes and fills the rate table in the ATH object based
**  on the operating mode.  The blink rates are also set up here, although
**  they have been superceeded by the ath_led module.
**
**  \param sc Pointer to ATH object (this).
**  \param mode WIRELESS_MODE Enumerated value indicating the
**              wireless operating mode
**
**  \return N/A
*/

static void
ath_setcurmode(struct ath_softc *sc, WIRELESS_MODE mode)
{
#define    N(a)    (sizeof(a)/sizeof(a[0]))
    /* NB: on/off times from the Atheros NDIS driver, w/ permission */
    static const struct {
        u_int        rate;        /* tx/rx 802.11 rate */
        u_int16_t    timeOn;        /* LED on time (ms) */
        u_int16_t    timeOff;    /* LED off time (ms) */
    } blinkrates[] = {
        { 108,  40,  10 },
        {  96,  44,  11 },
        {  72,  50,  13 },
        {  48,  57,  14 },
        {  36,  67,  16 },
        {  24,  80,  20 },
        {  22, 100,  25 },
        {  18, 133,  34 },
        {  12, 160,  40 },
        {  10, 200,  50 },
        {   6, 240,  58 },
        {   4, 267,  66 },
        {   2, 400, 100 },
        {   0, 500, 130 },
    };
    const HAL_RATE_TABLE *rt;
    int i, j;

    OS_MEMSET(sc->sc_rixmap, 0xff, sizeof(sc->sc_rixmap));
    rt = sc->sc_rates[mode];
    KASSERT(rt != NULL, ("no h/w rate set for phy mode %u", mode));
    for (i = 0; i < rt->rateCount; i++) {
        sc->sc_rixmap[rt->info[i].rateCode] = (u_int8_t)i;
    }
    OS_MEMZERO(sc->sc_hwmap, sizeof(sc->sc_hwmap));
    for (i = 0; i < 256; i++) {
        u_int8_t ix = rt->rateCodeToIndex[i];
        if (ix == 0xff) {
            sc->sc_hwmap[i].ledon = 500;
            sc->sc_hwmap[i].ledoff = 130;
            continue;
        }
        sc->sc_hwmap[i].ieeerate =
            rt->info[ix].dot11Rate & IEEE80211_RATE_VAL;
        sc->sc_hwmap[i].rateKbps = rt->info[ix].rateKbps;

        if (rt->info[ix].shortPreamble ||
            rt->info[ix].phy == IEEE80211_T_OFDM) {
            //sc->sc_hwmap[i].flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
        }
        /* setup blink rate table to avoid per-packet lookup */
        for (j = 0; j < N(blinkrates)-1; j++) {
            if (blinkrates[j].rate == sc->sc_hwmap[i].ieeerate) {
                break;
            }
        }
        /* NB: this uses the last entry if the rate isn't found */
        /* XXX beware of overlow */
        sc->sc_hwmap[i].ledon = blinkrates[j].timeOn;
        sc->sc_hwmap[i].ledoff = blinkrates[j].timeOff;
    }
    sc->sc_currates = rt;
    sc->sc_curmode = mode;

    /*
     * If use iwpriv to re-set the cts rate,
     * it should be not chage again.
     */
    if (!sc->sc_cus_cts_set)
    {
        /*
                * All protection frames are transmited at 2Mb/s for
                * 11g, otherwise at 1Mb/s.
                * XXX select protection rate index from rate table.
                */
	    if (HAL_OK == ath_hal_getcapability(sc->sc_ah, HAL_CAP_OFDM_RTSCTS, 0, NULL)) {
	        switch(mode) {
	        case WIRELESS_MODE_11g:
	        case WIRELESS_MODE_108g:
	        case WIRELESS_MODE_11NG_HT20:
	        case WIRELESS_MODE_11NG_HT40PLUS:
	        case WIRELESS_MODE_11NG_HT40MINUS:
	            sc->sc_protrix = 4;         /* use 6 Mb for RTSCTS */
	            break;
	        default:
	            sc->sc_protrix = 0;
	        }
	    }
	    else
	        sc->sc_protrix = (mode == WIRELESS_MODE_11g ? 1 : 0);
    }
    
    /* rate index used to send mgt frames */
    sc->sc_minrateix = 0;
}
#undef N

/******************************************************************************/
/*!
**  \brief Select Rate Table
**
**  Based on the wireless mode passed in, the rate table in the ATH object
**  is set to the mode specific rate table.  This also calls the callback
**  function to set the rate in the protocol layer object.
**
**  \param sc Pointer to ATH object (this)
**  \param mode Enumerated mode value used to select the specific rate table.
**
**  \return 0 if no valid mode is selected
**  \return 1 on success
*/

static int
ath_rate_setup(struct ath_softc *sc, WIRELESS_MODE mode)
{
    struct ath_hal *ah = sc->sc_ah;
    const HAL_RATE_TABLE *rt;

    switch (mode) {
    case WIRELESS_MODE_11a:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11A);
        break;
    case WIRELESS_MODE_11b:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11B);
        break;
    case WIRELESS_MODE_11g:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11G);
        break;
    case WIRELESS_MODE_108a:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_TURBO);
        break;
    case WIRELESS_MODE_108g:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_108G);
        break;
    case WIRELESS_MODE_11NA_HT20:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11NA_HT20);
        break;
    case WIRELESS_MODE_11NG_HT20:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11NG_HT20);
        break;
    case WIRELESS_MODE_11NA_HT40PLUS:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11NA_HT40PLUS);
        break;
    case WIRELESS_MODE_11NA_HT40MINUS:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11NA_HT40MINUS);
        break;
    case WIRELESS_MODE_11NG_HT40PLUS:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11NG_HT40PLUS);
        break;
    case WIRELESS_MODE_11NG_HT40MINUS:
        sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11NG_HT40MINUS);
        break;
    default:
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: invalid mode %u\n",
                __func__, mode);
        return 0;
    }
    rt = sc->sc_rates[mode];
    if (rt == NULL)
        return 0;

    /* setup rate set in 802.11 protocol layer */
    if (sc->sc_ieee_ops->setup_rate)
        sc->sc_ieee_ops->setup_rate(sc->sc_ieee, mode, NORMAL_RATE, rt);

    return 1;
}

/******************************************************************************/
/*!
**  \brief Setup half/quarter rate table support
**
**  Get pointers to the half and quarter rate tables.  Note that the mode
**  must have been selected previously.  Callbacks to the protocol object
**  are made if a table is selected.
**
**  \param sc Pointer to ATH object
**
**  \return N/A
*/

static void
ath_setup_subrates(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
    const HAL_RATE_TABLE *rt;

    sc->sc_half_rates = ath_hal_getratetable(ah, HAL_MODE_11A_HALF_RATE);
    rt = sc->sc_half_rates;
    if (rt != NULL) {
        if (sc->sc_ieee_ops->setup_rate)
            sc->sc_ieee_ops->setup_rate(sc->sc_ieee,
                                        WIRELESS_MODE_11a, HALF_RATE, rt);
    }

    sc->sc_quarter_rates = ath_hal_getratetable(ah,
                                                HAL_MODE_11A_QUARTER_RATE);
    rt = sc->sc_quarter_rates;
    if (rt != NULL) {
        if (sc->sc_ieee_ops->setup_rate)
            sc->sc_ieee_ops->setup_rate(sc->sc_ieee, WIRELESS_MODE_11a,
                                        QUARTER_RATE, rt);
    }
}

/******************************************************************************/
/*!
**  \brief Set Transmit power limit
**
**  This is intended as an "external" interface routine, since it uses the
**  ath_dev_t as input vice the ath_softc.
**
**  \param dev Void pointer to ATH object used by upper layers
**  \param txpowlimit 16 bit transmit power limit
**
**  \return N/A
*/

static int
ath_set_txpowlimit(ath_dev_t dev, u_int16_t txpowlimit, u_int32_t is2GHz)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int error = 0;

    /*
     * Just save it in configuration blob. It will take effect
     * after a chip reset.
     */
    if (is2GHz) {
        if (txpowlimit > ATH_TXPOWER_MAX_2G) {
            error = -EINVAL;
        } else {
            sc->sc_config.txpowlimit2G = txpowlimit;
        }
    } else {
        if (txpowlimit > ATH_TXPOWER_MAX_5G) {
            error = -EINVAL;
        } else {
            sc->sc_config.txpowlimit5G = txpowlimit;
        }
    }
    return error;
}

static void
ath_enable_tpc(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    ath_hal_enabletpc(ah);
    ath_wmi_set_desc_tpc(dev, 1);
}

void
ath_update_tpscale(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t tpscale;

    /* Get current tpscale setting and compare with the newly configured one */
    ath_hal_gettpscale(ah, &tpscale);

    if (sc->sc_config.tpscale != tpscale) {
        ath_hal_settpscale(ah, sc->sc_config.tpscale);
        /* If user tries to set lower than MIN, the value will be set to MIN. 
        Do a read back to make sure sc_config reflects the actual value set. */
        ath_hal_gettpscale(ah, &tpscale);
        sc->sc_config.tpscale = tpscale;
    }
    /* Must do a reset for the new tpscale to take effect */
    if(!sc->sc_invalid)
        ath_internal_reset(sc);
}

/******************************************************************************/
/*!
**  \brief Set Transmit power in HAL
**
**  This routine makes the actual HAL calls to set the new transmit power
**  limit.  This also calls back into the protocol layer setting the max
**  transmit power limit.
**
**  \param sc Pointer to ATH object (this)
**
**  \return N/A
*/

void
ath_update_txpow(struct ath_softc *sc, u_int16_t tpcInDb)
{
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t txpow, txpowlimit;
    int32_t txpow_adjust, txpow_changed;

    ATH_TXPOWER_LOCK(sc);
    if (IS_CHAN_2GHZ(&sc->sc_curchan)) {
        txpowlimit = sc->sc_config.txpowlimit2G;
        txpow_adjust = sc->sc_config.txpowupdown2G;
    } else {
        txpowlimit = sc->sc_config.txpowlimit5G;
        txpow_adjust = sc->sc_config.txpowupdown5G;
    }

    txpowlimit = (sc->sc_config.txpowlimit_override) ?
        sc->sc_config.txpowlimit_override : txpowlimit;
    txpow_changed = 0;
    
    /* if txpow_adjust is <0, update max txpower always firstly */
    if ((sc->sc_curtxpow != txpowlimit) ||
        (txpow_adjust < 0) || sc->sc_config.txpow_need_update) {
       ath_hal_settxpowlimit(ah, txpowlimit, 0, tpcInDb);
       /* read back in case value is clamped */
       ath_hal_gettxpowlimit(ah, &txpow);
       sc->sc_curtxpow = txpow;
       txpow_changed = 1;
       sc->sc_config.txpow_need_update = 0;
   }
    /*
     * Fetch max tx power level and update protocal stack
     */
    ath_hal_getmaxtxpow(sc->sc_ah, &txpow);

    if (txpow_adjust < 0) { //do nothing if >=0, can't go beyond power limit.
        /* set adjusted txpow to hal */
        txpow += txpow_adjust;
        ath_hal_settxpowlimit(ah, txpow, 0, tpcInDb);
        
        txpow_changed = 1;
        /* Fetch max tx power again */
        ath_hal_getmaxtxpow(sc->sc_ah, &txpow);
    } 

    if (txpow_changed) {
        /* Since Power is changed, apply PAPRD table again*/
       if (sc->sc_paprd_enable) {
           int retval;
           sc->sc_curchan.paprdTableWriteDone = 0;
           if (!sc->sc_scanning && (sc->sc_curchan.paprdDone == 1)) {
               retval = ath_apply_paprd_table(sc);
               DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: ath_apply_paprd_table retval %d \n", __func__, __LINE__, retval);
           }
       }
       DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: paprd_enable %d paprd_done %d Apply %d\n", __func__, __LINE__, sc->sc_paprd_enable, sc->sc_curchan.paprdDone, sc->sc_curchan.paprdTableWriteDone);
    }
    if (!sc->sc_paprd_enable) {
        ath_hal_paprd_dec_tx_pwr(ah);
    }
    
    if (sc->sc_ieee_ops->update_txpow)
        sc->sc_ieee_ops->update_txpow(sc->sc_ieee, sc->sc_curtxpow, txpow);

#ifdef ATH_SUPPORT_TxBF
    /* Update the per rate TxBF disable flag status from the HAL */
    if ((sc->sc_txbfsupport == AH_TRUE) && sc->sc_rc) { 
        struct atheros_softc *asc = sc->sc_rc;
        OS_MEMZERO(asc->sc_txbf_disable_flag,sizeof(asc->sc_txbf_disable_flag));
        ath_hal_get_perrate_txbfflag(sc->sc_ah, asc->sc_txbf_disable_flag);
     }
#endif
    ATH_TXPOWER_UNLOCK(sc);
}

/******************************************************************************/
/*!
**  \brief Set up New Node
**
** Setup driver-specific state for a newly associated node.  This routine
** really only applies if compression or XR are enabled, there is no code
** covering any other cases.
**
**  \param dev Void pointer to ATH object, to be called from protocol layre
**  \param node ATH node object that represents the other station
**  \param isnew Flag indicating that this is a "new" association
**
**  \return xxx
*/
static void
ath_newassoc(ath_dev_t dev, ath_node_t node, int isnew, int isuapsd)
{

    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = ATH_NODE(node);
    int tidno;
#if UMAC_SUPPORT_SMARTANTENNA    
    u_int8_t i=0;  
#endif    
    /*
     * if station reassociates, tear down the aggregation state.
     */
    if (!isnew) {
        for (tidno = 0; tidno < WME_NUM_TID; tidno++) {
            if (sc->sc_txaggr) {
                ath_tx_aggr_teardown(sc,an,tidno);
            }
            if (sc->sc_rxaggr) {
                ath_rx_aggr_teardown(sc,an,tidno);
            }
        }
    }
#ifdef ATH_SUPPORT_UAPSD
    an->an_flags = (isuapsd) ? ATH_NODE_UAPSD : 0;
#else
    an->an_flags=0;
#endif

#ifdef ATH_SUPERG_XR
    if(1) {
        struct ath_node *an = ATH_NODE(ni);
        if (ic->ic_ath_cap & an->an_node.ni_ath_flags & IEEE80211_ATHC_XR) {
            an->an_minffrate = ATH_MIN_FF_RATE;
        } else {
            an->an_minffrate = 0;
        }
        ath_grppoll_period_update(sc);
    }
#endif

    if ((sc->sc_opmode == HAL_M_HOSTAP) 
        && (an->is_sta_node != 1)) {
        ath_green_ap_state_mc( sc, ATH_PS_EVENT_INC_STA);
        /* Mark the node as a STA node */
        an->is_sta_node = 1;
    }
#if UMAC_SUPPORT_SMARTANTENNA    
    for (i=0; i<ATH_MAX_SM_RATESETS; i++) 
    {
       an->selected_smart_antenna[i] = sc->sc_defant;
    }
#endif    

}

/******************************************************************************/
/*!
**  \brief Convert Freq to channel number
**
**  Convert the frequency, provided in MHz, to an ieee 802.11 channel number.
**  This is done by calling a conversion routine in the ATH object.  The
**  sc pointer is required since this is a call from the protocol layer which
**  does not "know" about the internals of the ATH layer.
**
**  \param dev Void pointer to ATH object provided by protocol layer
**  \param freq Integer frequency, in MHz
**  \param flags Flag Value indicating if this is in the 2.4 GHz (CHANNEL_2GHZ)
**                    band or in the 5 GHz (CHANNEL_5GHZ).
**
**  \return Integer channel number
*/

static u_int
ath_mhz2ieee(ath_dev_t dev, u_int freq, u_int flags)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
     return ath_hal_mhz2ieee(sc->sc_ah, freq, flags);
}

/******************************************************************************/
/*!
**  \brief Set up channel list
**
**  Determines the proper set of channelflags based on the selected mode,
**  allocates a channel array, and passes it to the HAL for initialization.
**  If successful, the list is passed to the upper layer, then de-allocated.
**
**  \param sc Pointer to ATH object (this)
**  \param cc Integer Country Code value (normally read from EEPROM)
**  \param outDoor Boolean indicating if this transmitter is outdoors.
**  \param xchanMode Boolean indicating if extension channels are enabled.
**
**  \return -ENOMEM If channel strucure cannot be allocated
**  \return -EINVAL If channel initialization fails
**  \return 0 on success
*/

static int
ath_getchannels(struct ath_softc *sc, u_int cc,
                HAL_BOOL outDoor, HAL_BOOL xchanMode)
{
    struct ath_hal *ah = sc->sc_ah;
    HAL_CHANNEL *chans;
    int nchan;
    u_int8_t regclassids[ATH_REGCLASSIDS_MAX];
    u_int nregclass = 0;
    u_int wMode;
    u_int netBand;

    wMode = sc->sc_reg_parm.wModeSelect;

    if (!(wMode & HAL_MODE_11A)) {
        wMode &= ~(HAL_MODE_TURBO|HAL_MODE_108A|HAL_MODE_11A_HALF_RATE);
    }
    if (!(wMode & HAL_MODE_11G)) {
        wMode &= ~(HAL_MODE_108G);
    }
    netBand = sc->sc_reg_parm.NetBand;
    if (!(netBand & HAL_MODE_11A)) {
        netBand &= ~(HAL_MODE_TURBO|HAL_MODE_108A|HAL_MODE_11A_HALF_RATE);
    }
    if (!(netBand & HAL_MODE_11G)) {
        netBand &= ~(HAL_MODE_108G);
    }
    wMode &= netBand;

    chans = (HAL_CHANNEL *)OS_MALLOC(sc->sc_osdev, 
                      IEEE80211_CHAN_MAX * sizeof(HAL_CHANNEL), 
                      GFP_KERNEL);

    if (chans == NULL) {
        printk("%s: unable to allocate channel table\n", __func__);
        return -ENOMEM;
    }
    
    /*
     * remove some of the modes based on different compile time
     * flags.
     */
    ATH_DEV_RESET_HT_MODES(wMode);   /* reset the HT modes */

    if (!ath_hal_init_channels(ah, chans, IEEE80211_CHAN_MAX, (u_int *)&nchan,
                               regclassids, ATH_REGCLASSIDS_MAX, &nregclass,
                               cc, wMode, outDoor, xchanMode)) {
        u_int32_t rd;

        ath_hal_getregdomain(ah, &rd);
        printk("%s: unable to collect channel list from hal; "
               "regdomain likely %u country code %u\n",
               __func__, rd, cc);
        OS_FREE(chans);
        return -EINVAL;
    }

    if (sc->sc_ieee_ops->setup_channel_list) {
        sc->sc_ieee_ops->setup_channel_list(sc->sc_ieee, CLIST_UPDATE,
                                            chans, nchan, regclassids, nregclass,
                                            CTRY_DEFAULT);
    }

    OS_FREE(chans);
    return 0;
}

/******************************************************************************/
/*!
**  \brief Set Default Antenna
**
**  Call into the HAL to set the default antenna to use.  Not really valid for
**  MIMO technology.
**
**  \param context Void pointer to ATH object (called from Rate Control)
**  \param antenna Index of antenna to select
**
**  \return N/A
*/

void
ath_setdefantenna(void *context, u_int antenna)
{
    struct ath_softc *sc = (struct ath_softc *)context;
    struct ath_hal *ah = sc->sc_ah;
    
    ath_hal_setdefantenna(ah, antenna);
    if (sc->sc_defant != antenna)
        sc->sc_stats.ast_ant_defswitch++;
    sc->sc_defant = antenna;
    sc->sc_rxotherant = 0;
}

/******************************************************************************/
/*!
**  \brief Set Slot Time
**
**  This will wake up the chip if required, and set the slot time for the
**  frame (maximum transmit time).  Slot time is assumed to be already set
** in the ATH object member sc_slottime
**
**  \param sc Pointer to ATH object (this)
**
**  \return N/A
*/

void
ath_setslottime(struct ath_softc *sc)
{
    ATH_FUNC_ENTRY_VOID(sc);

    ATH_PS_WAKEUP(sc);
    ath_hal_setslottime(sc->sc_ah, sc->sc_slottime);
    sc->sc_updateslot = ATH_OK;
    ATH_PS_SLEEP(sc);
}

/******************************************************************************/
/*!
**  \brief Update slot time
**
**  This is a call interface used by the protocol layer to update the current
**  slot time setting based on updated settings.  This updates the hardware
**  also.
**
**  \param dev Void pointer to ATH object used by protocol layer
**  \param slottime Slot duration in microseconds
**
**  \return N/A
*/

static void
ath_updateslot(ath_dev_t dev, int slottime)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ATH_FUNC_ENTRY_VOID(sc);
    ATH_PS_WAKEUP(sc);

    /* save slottime setting */
    sc->sc_slottime = slottime;
    
    /*!
    ** When not coordinating the BSS, change the hardware
    ** immediately.  For other operation we defer the change
    ** until beacon updates have propagated to the stations.
     */
    if (sc->sc_opmode == HAL_M_HOSTAP)
        sc->sc_updateslot = ATH_UPDATE;
    else
        ath_setslottime(sc);

    ATH_PS_SLEEP(sc);
}

/******************************************************************************/
/*!
**  \brief Determine mode from channel flags
**
**  This routine will provide the enumerated WIRELESSS_MODE value based
**  on the settings of the channel flags.  If ho valid set of flags
**  exist, the lowest mode (11b) is selected.
**
**  \param chan Pointer to channel flags value
**
**  \return Higest channel mode selected by the flags.
*/

static WIRELESS_MODE
ath_halchan2mode(HAL_CHANNEL *chan)
{
    if ((chan->channelFlags & CHANNEL_108G) == CHANNEL_108G)
        return WIRELESS_MODE_108g;
    else if ((chan->channelFlags & CHANNEL_108A) == CHANNEL_108A)
        return WIRELESS_MODE_108a;
    else if ((chan->channelFlags & CHANNEL_A) == CHANNEL_A)
        return WIRELESS_MODE_11a;
    else if ((chan->channelFlags & CHANNEL_G) == CHANNEL_G)
        return WIRELESS_MODE_11g;
    else if ((chan->channelFlags & CHANNEL_B) == CHANNEL_B)
        return WIRELESS_MODE_11b;
    else if ((chan->channelFlags & CHANNEL_A_HT20) == CHANNEL_A_HT20)
        return WIRELESS_MODE_11NA_HT20;
    else if ((chan->channelFlags & CHANNEL_G_HT20) == CHANNEL_G_HT20)
        return WIRELESS_MODE_11NG_HT20;
    else if ((chan->channelFlags & CHANNEL_A_HT40PLUS) == CHANNEL_A_HT40PLUS)
        return WIRELESS_MODE_11NA_HT40PLUS;
    else if ((chan->channelFlags & CHANNEL_A_HT40MINUS) == CHANNEL_A_HT40MINUS)
        return WIRELESS_MODE_11NA_HT40MINUS;
    else if ((chan->channelFlags & CHANNEL_G_HT40PLUS) == CHANNEL_G_HT40PLUS)
        return WIRELESS_MODE_11NG_HT40PLUS;
    else if ((chan->channelFlags & CHANNEL_G_HT40MINUS) == CHANNEL_G_HT40MINUS)
        return WIRELESS_MODE_11NG_HT40MINUS;

    /* NB: should not get here */
    printk("%s: cannot map channel to mode; freq %u flags 0x%x\n",
           __func__, chan->channel, chan->channelFlags);
    return WIRELESS_MODE_11b;
}


#ifdef ATH_SUPPORT_DFS
/******************************************************************************/
/*!
**  \brief  Adjust DIFS value
**
**  This routine will adjust the DIFS value when VAP is set for 
**  fixed rate in FCC domain. This is a fix for EV753454
**
**  \param sc Pointer to ATH object (this)
**
**  \return xxx
*/

void ath_adjust_difs (struct ath_softc *sc)
{
    int i;
    u_int32_t val;
    int adjust = 0;
    struct ath_vap *vap;
 
    /*
     * for now we modify DIFS only for FCC/Japan
     */

    if (sc->sc_dfs->dfsdomain != DFS_FCC_DOMAIN && sc->sc_dfs->dfsdomain != DFS_MKK4_DOMAIN)  {
        return;
    }

    /*
     * check if the vap is set for fixed manual rate
     */

    for (i = 0; ((i < sc->sc_nvaps) && (adjust == 0)); i++) {
        vap = sc->sc_vaps[i];
        if (vap) {
            val = vap->av_config.av_fixed_rateset;
            if (val == IEEE80211_FIXED_RATE_NONE) {
                continue;
            }
            /*
             * we have a fixed rate. check if it is below 24 Mb
             *
             */
            switch (val & 0x7f) {
            case 0x0d:              /* 36 Mb */
            case 0x08:              /* 48 Mb */
            case 0x0c:              /* 54 Mb */
                DPRINTF(sc, ATH_DEBUG_DFS1, "%s: fixed rate=0x%x is over 24 Mb. Default DIFS OK\n",
                       __func__,val);
                break;
            case 0x1b:              /* 1 Mb  */
            case 0x1a:              /* 2 Mb  */
            case 0x19:              /* 5 Mb  */
            case 0x0b:              /* 6 Mb  */
            case 0x0f:              /* 9 Mb  */
            case 0x18:              /* 11 Mb */
            case 0x0a:              /* 12 Mb */
            case 0x0e:              /* 18 Mb */
            case 0x09:              /* 24 Mb */
                adjust = 1;
                DPRINTF(sc, ATH_DEBUG_DFS1, "%s: fixed rate=0x%x is at or below 24 Mb. Adjust DIFS\n",
                       __func__,val);
                break;
            default:
                break;
            }
        }
    }


    /*
     * set DIFS between packets to  the new value
     *
     */

    if (adjust) {
        DPRINTF(sc, ATH_DEBUG_DFS1, "%s: DFS_FCC_DOMAIN, adjust DIFS\n", __func__);
        ath_hal_adjust_difs(sc->sc_ah, 1);
    } 
 }

#endif

/******************************************************************************/
/*!
**  \brief Change Channels
**
**  Performs the actions to change the channel in the hardware, and set up
**  the current operating mode for the new channel.
**
**  \param sc Pointer to ATH object (this)
**  \param chan Pointer to channel descriptor for new channel
**
**  \return xxx
*/

void
ath_chan_change(struct ath_softc *sc, HAL_CHANNEL *chan)
{
    WIRELESS_MODE mode;

    mode = ath_halchan2mode(chan);

    ath_rate_setup(sc, mode);
    ath_setcurmode(sc, mode);

    /*
     * Trigger setcurmode on the target
     */
    ATH_HTC_SETMODE(sc, mode);

#ifdef ATH_SUPPORT_DFS
    /*
     * restore DIFS to default in case we had modified them
     */

    ath_hal_adjust_difs(sc->sc_ah, 0);
#if 0 /* Temporarily commented bcoz of Hang Issue with G mode */
        if ((hchan.channelFlags & CHANNEL_108G) == CHANNEL_108G) {
            DPRINTF(sc, ATH_DEBUG_DFS2, "Calling ath_ar_enable \n");
            ath_ar_enable(sc);
        }
#endif
        if (sc->sc_dfs != NULL) {
            sc->sc_dfs->dfs_curchan_radindex = -1;
            sc->sc_dfs->dfs_extchan_radindex = -1;
            
#if ATH_SUPPORT_IBSS_DFS
            /* EV 83794 and EV84163, in IBSS_DFS solution,
             * IBSS joiner did not enable radar detection.
             * We enable it using this WAR 
             */
            if((chan->channel == sc->sc_curchan.channel) &&
               (chan->privFlags != sc->sc_curchan.privFlags) &&
               (!sc->sc_scanning) &&
               (sc->sc_curchan.privFlags & CHANNEL_DFS)&&
               (sc->sc_opmode == HAL_M_IBSS))
            {
                   chan->privFlags = sc->sc_curchan.privFlags;
            }
#endif /* ATH_SUPPORT_IBSS_DFS */         

        /* Only enable radar when we are in a DFS channel and are not scanning (see bug 29099)*/
        /* Also WDS clients must not enable radar detection */
            if((chan->privFlags & CHANNEL_DFS) && (!sc->sc_scanning) &&(!sc->sc_nostabeacons)) {
                u_int8_t index;
                int error;
                error = dfs_radar_enable(sc);
                if (error) {
                    DPRINTF(sc, ATH_DEBUG_DFS, "%s: dfs_radar_enable failed\n",
                            __func__);
                }
                /* Get the index for both the primary and extension channels*/
                if (dfs_getchanstate(sc, &index, FALSE) != NULL)
                    sc->sc_dfs->dfs_curchan_radindex = index;
                if (dfs_getchanstate(sc, &index, TRUE) != NULL)
                    sc->sc_dfs->dfs_extchan_radindex = index;
                /*
                 * check if we need to modify DIFS value, do so if necessary 
                 */

                 ath_adjust_difs(sc);
                
            }
       }
#endif

#if ATH_SUPPORT_SPECTRAL
        if (!(chan->privFlags & CHANNEL_DFS) &&(!sc->sc_nostabeacons)) {
            struct ath_spectral *spectral=sc->sc_spectral;
            SPECTRAL_LOCK(spectral); 
            if(sc->sc_spectral_full_scan || sc->sc_spectral_scan) {
                int error = 0;    
                error = spectral_scan_enable(sc);
                if (error) {
                    DPRINTF(sc, ATH_DEBUG_ANY, "%s: spectral_scan__enable failed, HW may not be spectral capable \n", __func__);
                }
            }
            SPECTRAL_UNLOCK(spectral); 
        }
#endif

#ifdef notyet
    /*
     * Update BPF state.
     */
    sc->sc_tx_th.wt_chan_freq = sc->sc_rx_th.wr_chan_freq =
        htole16(chan->ic_freq);
    sc->sc_tx_th.wt_chan_flags = sc->sc_rx_th.wr_chan_flags =
        htole16(chan->ic_flags);
#endif
}

/******************************************************************************/
/*!
**  \brief Set the current channel
**
** Set/change channels.  If the channel is really being changed, it's done
** by reseting the chip.  To accomplish this we must first cleanup any pending
** DMA, then restart stuff after a la ath_init.
**
**  \param sc Pointer to ATH object (this)
**  \param hchan Pointer to channel descriptor for the new channel.
**
**  \return -EIO on failure
**  \return 0 on success
*/

static int
ath_chan_set(struct ath_softc *sc, HAL_CHANNEL *hchan, int tx_timeout, int rx_timeout, bool retry)
{
    struct ath_hal *ah = sc->sc_ah;
    u_int8_t tswitch=0;
    HAL_BOOL fastcc = AH_TRUE, stopped;
    HAL_HT_MACMODE ht_macmode = sc->sc_ieee_ops->cwm_macmode(sc->sc_ieee);
#ifdef ATH_SUPPORT_DFS
    u_int8_t chan_flags_changed=0;
#endif

    DPRINTF(sc, ATH_DEBUG_RESET, "%s: %u (%u MHz) -> %u (%u MHz)\n",
            __func__,
            ath_hal_mhz2ieee(ah, sc->sc_curchan.channel,
                             sc->sc_curchan.channelFlags),
            sc->sc_curchan.channel,
            ath_hal_mhz2ieee(ah, hchan->channel, hchan->channelFlags), hchan->channel);

    /* check if it is turbo mode switch */
    if (hchan->channel == sc->sc_curchan.channel &&
       (hchan->channelFlags & CHANNEL_TURBO) != 
        (sc->sc_curchan.channelFlags & CHANNEL_TURBO))
    {
        tswitch = 1;
    }

    /* As part of the Sowl TX hang WAR, we switch from dynamic HT40 mode to 
       static HT20 mode when we detect the hang. In this case, only the channel 
       flags change, so no need to start DFS wait or any of the other good stuff.*/
#ifdef ATH_SUPPORT_DFS
     if (hchan->channel == sc->sc_curchan.channel &&
        hchan->channelFlags != sc->sc_curchan.channelFlags &&
        !sc->sc_full_reset
        ) 
         chan_flags_changed = 1;

#endif
    if (hchan->channel != sc->sc_curchan.channel ||
        hchan->channelFlags != sc->sc_curchan.channelFlags ||
        sc->sc_update_chainmask ||
        sc->sc_full_reset
    )
    {
        HAL_STATUS status;

        /*!
        ** This is only performed if the channel settings have
        ** actually changed.
        **
        ** To switch channels clear any pending DMA operations;
        ** wait long enough for the RX fifo to drain, reset the
        ** hardware at the new frequency, and then re-enable
        ** the relevant bits of the h/w.
        */
    
        /* Set the flag so that other functions will not try to restart tx/rx DMA. */
        /* Note: we should re-organized the fellowing code to use ath_reset_start(),
           ath_reset(), and ath_reset_end() */
#if ATH_RESET_SERIAL
        ATH_RESET_ACQUIRE_MUTEX(sc);
        ATH_RESET_LOCK(sc);
        atomic_inc(&sc->sc_dpc_stop);
        printk("%s %d %d\n", __func__, 
__LINE__, sc->sc_dpc_stop);
        ATH_USB_TX_STOP(sc->sc_osdev);
        ATH_INTR_DISABLE(sc);
        ATH_RESET_UNLOCK(sc);
#else
        atomic_inc(&sc->sc_in_reset);
        ATH_USB_TX_STOP(sc->sc_osdev);
        ATH_INTR_DISABLE(sc);
#endif


#if ATH_RESET_SERIAL
        OS_DELAY(100);
        ATH_RESET_LOCK(sc);
        atomic_inc(&sc->sc_hold_reset);
        atomic_set(&sc->sc_reset_queue_flag, 1);
        ath_reset_wait_tx_rx_finished(sc);
        ATH_RESET_UNLOCK(sc);
        ath_reset_wait_intr_dpc_finished(sc);
        atomic_dec(&sc->sc_dpc_stop);
        printk("%s %d %d\n", __func__, 
__LINE__, sc->sc_dpc_stop);
        printk("Begin chan_set sequence......\n");
#endif
        ATH_DRAINTXQ(sc, retry, tx_timeout);     /* clear or requeue pending tx frames */
        stopped = ATH_STOPRECV_EX(sc, rx_timeout);  /* turn off frame recv */

        /* XXX: do not flush receive queue here. We don't want
         * to flush data frames already in queue because of
         * changing channel. */

        /* Set coverage class */
        if (sc->sc_scanning || !(hchan->channelFlags & CHANNEL_5GHZ))
            ath_hal_setcoverageclass(ah, 0, 0);
        else
            ath_hal_setcoverageclass(ah, sc->sc_coverageclass, 0);

        if (!stopped || sc->sc_full_reset)
            fastcc = AH_FALSE;

        ATH_HTC_TXRX_STOP(sc, fastcc);


#ifdef ATH_C3_WAR
        STOP_C3_WAR_TIMER(sc);
#endif

#if !ATH_RESET_SERIAL
        ATH_LOCK_PCI_IRQ(sc);
#endif
        sc->sc_paprd_done_chain_mask = 0;
        if (!ath_hal_reset(ah, sc->sc_opmode, hchan,
                           ht_macmode, sc->sc_tx_chainmask, sc->sc_rx_chainmask,
                           sc->sc_ht_extprotspacing, fastcc, &status,
                           sc->sc_scanning)) {
            printk("%s: unable to reset channel %u (%uMhz) "
                   "flags 0x%x hal status %u\n",
                   __func__,
                   ath_hal_mhz2ieee(ah, hchan->channel, hchan->channelFlags),
                   hchan->channel, hchan->channelFlags, status);
#if ATH_RESET_SERIAL
            atomic_set(&sc->sc_reset_queue_flag, 0);
            atomic_dec(&sc->sc_hold_reset);
            ATH_RESET_RELEASE_MUTEX(sc);
#else
            ATH_UNLOCK_PCI_IRQ(sc);
            atomic_dec(&sc->sc_in_reset);
#endif
            ASSERT(atomic_read(&sc->sc_in_reset) >= 0);

            if (ATH_STARTRECV(sc) != 0) {
                printk("%s: unable to restart recv logic\n", __func__);
            }
            return -EIO;
        }
        ATH_HTC_ABORTTXQ(sc);
#if !ATH_RESET_SERIAL
        ATH_UNLOCK_PCI_IRQ(sc);
#endif
        sc->sc_curchan = *hchan;
        sc->sc_curchan.paprdDone = 0;
        sc->sc_curchan.paprdTableWriteDone = 0;
        sc->sc_update_chainmask = 0;
        sc->sc_full_reset = 0;

#ifdef ATH_BT_COEX
        if (sc->sc_hasbtcoex && (!fastcc)) {
            u_int32_t   scanState = ATH_COEX_WLAN_SCAN_RESET;
            ath_bt_coex_event(sc, ATH_COEX_EVENT_WLAN_SCAN, &scanState);
        }
#endif

 #if ATH_RESET_SERIAL
            /* to keep lock sequence as same as ath_tx_start_dma->ath_tx_txqaddbuf,
                 * we have to lock all of txq firstly.
                 */
            if (!sc->sc_enhanceddmasupport) {
                int i;
                for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
                     ATH_TXQ_LOCK(&sc->sc_txq[i]);
                
                ATH_RESET_LOCK(sc);
                atomic_set(&sc->sc_reset_queue_flag, 0);
                atomic_dec(&sc->sc_hold_reset);
                if (atomic_read(&sc->sc_hold_reset))
                    printk("Wow, still multiple reset in process,--investigate---\n");
                
                 /* flush all the queued tx_buf */
                for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
                    
                    ath_bufhead *head, buf_head;
                    struct ath_buf *buf, *last_buf;
                    
                    //ATH_TXQ_LOCK(&sc->sc_txq[i]);
                    //ATH_RESET_LOCK_Q(sc, i);
                    
                    head = &(sc->sc_queued_txbuf[i].txbuf_head);
                    
                    do {
                        buf = TAILQ_FIRST(head);
                        if (!buf)
                            break;
                        last_buf = buf->bf_lastbf;
                        TAILQ_INIT(&buf_head);
                        TAILQ_REMOVE_HEAD_UNTIL(head, &buf_head, last_buf, bf_list);
                        _ath_tx_txqaddbuf(sc, &sc->sc_txq[i], &buf_head); //can't call directly
                    } while (!TAILQ_EMPTY(head));
                    
            
                    //ATH_RESET_UNLOCK_Q(sc, i);
                    //ATH_TXQ_UNLOCK(&sc->sc_txq[i]);
                }
                ATH_RESET_UNLOCK(sc);
                for (i = HAL_NUM_TX_QUEUES -1 ; i >= 0; i--)
                     ATH_TXQ_UNLOCK(&sc->sc_txq[i]);
            } else {
                atomic_set(&sc->sc_reset_queue_flag, 0);
                atomic_dec(&sc->sc_hold_reset);
            }
#else

        /* Clear the "In Reset" flag. */
        atomic_dec(&sc->sc_in_reset);
#endif
        ASSERT(atomic_read(&sc->sc_in_reset) >= 0);
        
        /*
         * Re-enable rx framework.
         */
        ath_wmi_start_recv(sc);

        if (ATH_STARTRECV(sc) != 0) {
            printk("%s: unable to restart recv logic\n",
                   __func__);
            return -EIO;
        }

        /*
         * Change channels and update the h/w rate map
         * if we're switching; e.g. 11a to 11b/g.
         */
        ath_chan_change(sc, hchan);

        ath_update_txpow(sc, sc->tx_power);        /* update tx power state */

#ifdef ATH_SUPPORT_DFS
        /* Only start DFS wait when we are in a DFS channel, in AP mode 
        and are not scanning (see bug 29099)*/

        /* Also if we are changing modes (in case of the Sowl TX hang WAR,
        only the channel flags have changed, so no need to start DFS wait again*/
        /* Also WDS clients must not start DFS wait */

        if ((sc->sc_opmode == HAL_M_HOSTAP || sc->sc_opmode == HAL_M_IBSS) &&
            (!sc->sc_scanning) &&
            (!chan_flags_changed) &&
            (!sc->sc_nostabeacons))
        {
            int i, j, diff;
            struct ath_vap *avp;
            if (sc->sc_curchan.privFlags & CHANNEL_DFS) {
                if (sc->sc_dfs->sc_dfswait) {
                    OS_CANCEL_TIMER(&sc->sc_dfs->sc_dfswaittimer);
                }

                printk("%s: start DFS WAIT period on channel %d\n",
                       __func__,sc->sc_curchan.channel);
                sc->sc_dfs->sc_dfswait=1;
                ath_hal_dfs_cac_war (ah, 1);
                ath_hal_cac_tx_quiet(sc->sc_ah, 1);

                /* Bug 32398 - if AP is in running state, detects radar and 
                   switches to another DFS channel, it starts a CAC for the new 
                   channel. If radar is detected during the CAC, no channel 
                   switch happens because the VAP state is running but the VAP 
                   is not beaconing (so no channel switch announcements,etc). To
                   fix this, if we detect radar in the middle of CAC, make sure 
                   that the VAP state is in INIT so that the channel switch 
                   happens okay.
                 */
                /* Bug 37930 VAP state must be INIT when we start DFS wait
                   irrespective of whether VAP was in RUN state before.
                   Move fix for 32398 to where we start DFS wait
                   so that we always start DFS WAIT with an INIT state
                 */
                j = 0;
                for (i=0; ((i < ATH_BCBUF) && (j < sc->sc_nvaps)); i++) {
                    avp = sc->sc_vaps[i];
                    if (avp) {
                        j++;
                        avp->av_dfswait_run=1;
                        sc->sc_ieee_ops->ath_net80211_set_vap_state(
                                sc->sc_ieee, i, LMAC_STATE_EVENT_DFS_WAIT);
                    }
                }
                diff = dfs_get_nol_chan(sc, hchan);
                if (diff != 0) {
                    OS_SET_TIMER(
                        &sc->sc_dfs->sc_dfswaittimer,
                        diff);
                } else {
                    OS_SET_TIMER(
                        &sc->sc_dfs->sc_dfswaittimer,
                        sc->sc_dfs->sc_dfs_cac_time);
                }
            } else {
                /* We are now in a non-DFS channel, but we may still have
                   wait timer from a previous DFS channel pending.
                   This can happen when radar is detected before the end of
                   the CAC wait time and the AP moves to a non-DFS channel
                 */
                j = 0;
                for (i=0; ((i < ATH_BCBUF) && (j < sc->sc_nvaps)); i++) {
                    avp = sc->sc_vaps[i];
                    if (avp) {
                        j++;
                        avp->av_dfswait_run=0;
                        if (sc->sc_dfs->sc_dfswait) {
                            sc->sc_ieee_ops->ath_net80211_set_vap_state(
                            sc->sc_ieee, i, LMAC_STATE_EVENT_CHAN_SET);
                            sc->sc_ieee_ops->ath_net80211_set_vap_state(
                            sc->sc_ieee,i,LMAC_STATE_EVENT_UP);
                        }
                    }
                }
                if (sc->sc_dfs->sc_dfswait) {
                   /* sc->sc_dfs->sc_dfswait=0;*/   /* We should not clear sc_dfswait here,
                                                       because if we clear it here, then
                                                       sc_dfswaittimer would return without doing anything 
                                                     */
                                                       
                    OS_CANCEL_TIMER(&sc->sc_dfs->sc_dfswaittimer);
                    printk(
                        "Non-DFS channel, cancelling previous DFS wait "
                        "timer channel %d\n", sc->sc_curchan.channel);
                    /* Bug 32399 - When AP detects radar in CAC period,
                       if it switches to a non-DFS channel, nothing sets
                       the VAP to running state. So no beaconing will start.
                       Fix is to cancel the DFS wait timer like before but
                       do all the cleanup that it would do at the end of CAC.
                     */
                    OS_SET_TIMER(&sc->sc_dfs->sc_dfswaittimer, 0);
                }
            }
        }
#endif
        /*!
        ** re configure beacons when it is a turbo mode switch.
        ** HW seems to turn off beacons during turbo mode switch.
        ** This is only needed for AP or IBSS mode. Also, we don't
        ** want to send beacons on a non-home channel during a
        ** background scan in IBSS mode.
         */
        if (sc->sc_opmode == HAL_M_HOSTAP ||
            sc->sc_opmode == HAL_M_IBSS) {
            if (tswitch && !sc->sc_scanning)
                ath_beacon_config(sc,ATH_BEACON_CONFIG_REASON_RESET,ATH_IF_ID_ANY);
        }

#ifdef ATH_GEN_TIMER
        /* Inform the generic timer module that a reset just happened. */
        ath_gen_timer_post_reset(sc);
#endif  //ATH_GEN_TIMER

        /*
         * Re-enable interrupts.
         */
        ATH_INTR_ENABLE(sc);
        ATH_USB_TX_START(sc->sc_osdev);
#if ATH_RESET_SERIAL
        ATH_RESET_RELEASE_MUTEX(sc);
#endif
    }
    return 0;
}

/******************************************************************************/
/*!
**  \brief Calculate the Calibration Interval
**
**  The interval must be the shortest necessary to satisfy ANI, 
**  short calibration and long calibration.
**
**  \param sc_Caldone: indicates whether calibration has already completed
**         enableANI: indicates whether ANI is enabled
**
**  \return The timer interval
*/

static u_int32_t
ath_get_cal_interval(HAL_BOOL sc_Caldone, u_int32_t ani_interval, HAL_BOOL isOpenLoopPwrCntrl, u_int32_t cal_interval)
{
    if (isOpenLoopPwrCntrl) {
        cal_interval = min(cal_interval, (u_int32_t)ATH_OLPC_CALINTERVAL);
    }

    if (ani_interval) { 
	    // non-zero means ANI enabled
        cal_interval = min(cal_interval, ani_interval);
    }
    if (! sc_Caldone) {
        cal_interval = min(cal_interval, (u_int32_t)ATH_SHORT_CALINTERVAL);
    }

    /*
     * If we wanted to be really precise we'd now compare cal_interval to the
     * time left before each of the calibrations must be run. 
     * For example, it is possible that sc_Caldone changed from FALSE to TRUE 
     * just 1 second before the long calibration interval would elapse. 
     * Is this case the timer interval is still set to ATH_LONG_CALINTERVAL, 
     * causing an extended delay before the long calibration is performed.
     *
     * Using values for ani and short and long calibrations that are not 
     * multiples of each other would cause a similar problem (try setting
     * ATH_SHORT_CALINTERVAL = 20000 and ATH_LONG_CALINTERVAL = 30000)
     */

    return cal_interval;
}

#ifdef ATH_TX_INACT_TIMER
static
OS_TIMER_FUNC(ath_inact)
{
    struct ath_softc    *sc;
    OS_GET_TIMER_ARG(sc, struct ath_softc *);

    if (!sc->sc_nvaps || sc->sc_invalid ) {
        goto out;
    }
#ifdef ATH_SUPPORT_DFS
    if (sc->sc_dfs && sc->sc_dfs->sc_dfswait) {
        OS_SET_TIMER(&sc->sc_inact, 60000 /* ms */);
        return;
    }
#endif

    if (sc->sc_tx_inact >= sc->sc_max_inact) {
    int i;
        sc->sc_tx_inact = 0;
    for (i = 0; i < 9; i++) { /* beacon stuck will handle queue 9 */
        if (ath_hal_numtxpending(sc->sc_ah, i)) {
        ath_internal_reset(sc);
        OS_SET_TIMER(&sc->sc_inact, 60000 /* ms */);
        return;
        }
        }
    } else {
        sc->sc_tx_inact ++;
    }

out:
    OS_SET_TIMER(&sc->sc_inact, 1000 /* ms */);
    return;
}
#endif

#if ATH_HW_TXQ_STUCK_WAR
#define MAX_QSTUCK_TIME 3 /* 3*1000=3 which means min of 2 sec and max of 3 sec */
#define QSTUCK_TIMER_VAL 1000 /* 1 sec */
static
OS_TIMER_FUNC(ath_txq_stuck_checker)
{
    struct ath_softc *sc;
    struct ath_txq *txq;
    u_int16_t i;
    u_int16_t reset_required = 0;

    OS_GET_TIMER_ARG(sc, struct ath_softc *);

    if (!sc->sc_nvaps || sc->sc_invalid ) {
        goto out;
    }
#ifdef ATH_SUPPORT_DFS
    if (sc->sc_dfs && sc->sc_dfs->sc_dfswait) {
        OS_SET_TIMER(&sc->sc_qstuck, 60000 /* ms */);
        return;
    }
#endif

    /* check for all queues */
    for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
        txq = &sc->sc_txq[i];
        ATH_TXQ_LOCK(txq);
        if(txq->axq_tailindex != txq->axq_headindex) {
            txq->tx_done_stuck_count++;
            if(txq->tx_done_stuck_count >= MAX_QSTUCK_TIME) {
                reset_required = 1;
                txq->tx_done_stuck_count = 0;
                DPRINTF(sc, ATH_DEBUG_ANY, "\n %s ******** Hardware Q-%d is stuck !!!!",__func__,i);
            }
        } else {
            /* no packets in hardware fifo */
            txq->tx_done_stuck_count = 0;
        }
        ATH_TXQ_UNLOCK(txq);
    }
    if (reset_required) {
        /* hardware q stuck detected !!!! reset hardware */
        DPRINTF(sc, ATH_DEBUG_ANY, "\n %s resetting hardware for Q stuck ",__func__);
        sc->sc_reset_type = ATH_RESET_NOLOSS;
        ath_internal_reset(sc);
        sc->sc_reset_type = ATH_RESET_DEFAULT;
        sc->sc_stats.ast_resetOnError++;
    }

out:
    OS_SET_TIMER(&sc->sc_qstuck, QSTUCK_TIMER_VAL);
    return;
}
#endif


static int
ath_up_beacon(void *arg)
{
    struct ath_softc    *sc = (struct ath_softc *)arg;

    sc->sc_up_beacon_purge = 0;
    return 1;   /* don't Re-Arm */
}

#if ATH_SUPPORT_VOW_DCS
static int
ath_sc_chk_intr(struct ath_softc *sc, HAL_ANISTATS *ani_stats)
{
    struct ath_dcs_params       *dcs_params;
    u_int32_t cur_ts;
    u_int32_t ts_d = 0;          /* elapsed time                */
    u_int32_t rxclr_d = 0;       /* differential rx_clear count */
    u_int32_t cur_thp, unusable_cu, rxc_cu;
    u_int32_t total_cu, tx_cu, rx_cu, wasted_txcu, total_wasted_cu, tx_err;
    u_int32_t maxerr_cnt, maxerr_rate, ofdmphyerr_rate, cckphyerr_rate;
    int higherr;
    dcs_params = &sc->sc_dcs_params;

    DPRINTF(sc, ATH_DEBUG_DCS, "DCS: dcrx %u dctx %u rxt %u wtxt %u\n",
            dcs_params->bytecntrx, dcs_params->bytecnttx, dcs_params->rxtime,
            dcs_params->wastedtxtime);
    /*DPRINTF(sc, ATH_DEBUG_DCS, "DCS: %u, %u, %u, %u, %u, %u, %u, %u\n",  
            ani_stats->cyclecnt_diff,  ani_stats->rxclr_cnt,  ani_stats->txframecnt_diff,
            ani_stats->rxframecnt_diff,  ani_stats->listen_time,  ani_stats->ofdmphyerr_cnt,
            ani_stats->cckphyerr_cnt,  ani_stats->valid );*/

    cur_ts = ath_hal_gettsf32(sc->sc_ah);
    if (dcs_params->prev_ts > cur_ts ){
        DPRINTF(sc, ATH_DEBUG_DCS, "DCS:%s timestamp Counter wrap!!!\n",__func__);
        dcs_params->intr_cnt = 0;
        dcs_params->samp_cnt = 0;
        goto terminate;
    }
    else {
        ts_d = cur_ts - dcs_params->prev_ts;
        /* compute current thp in Mbps */
        if ( dcs_params->bytecntrx + dcs_params->bytecnttx  > 0xfffffff ){
            cur_thp = ((( dcs_params->bytecntrx + dcs_params->bytecnttx ) >> 8 )/ (ts_d >> 8))*8;
        }
        else{
            cur_thp = ((( dcs_params->bytecntrx + dcs_params->bytecnttx )*8)/ts_d);
        }
        /* If thp is more than 150 Mbps, do not chk for interference
         * and increase timer value to 1 min. We can increase it further.
         */
        if ( cur_thp > 150 ){
            dcs_params->intr_cnt = 0;
            dcs_params->samp_cnt = 0;
            DPRINTF(sc, ATH_DEBUG_DCS,"DCS: %d %u \n", cur_thp, ts_d);
            goto terminate;
        }
    }
    
    if ((dcs_params->prev_rxclr > ani_stats->rxclr_cnt) || (!ani_stats->valid)){
        DPRINTF(sc, ATH_DEBUG_DCS,"DCS:%s Counter wrap!!!\n",__func__ );
        goto terminate;
    }
    else {
        rxclr_d = ani_stats->rxclr_cnt - dcs_params->prev_rxclr;

        total_cu = ( (rxclr_d >> 8 )* 100)/ (ani_stats->cyclecnt_diff >> 8 );
        tx_cu = ( (ani_stats->txframecnt_diff >>8) * 100)/ (ani_stats->cyclecnt_diff >> 8 ); 
        rx_cu =  ((dcs_params->rxtime >> 8) * 100) / (ts_d >> 8) ;
        rxc_cu = ( (ani_stats->rxframecnt_diff >>8) * 100)/ (ani_stats->cyclecnt_diff >> 8 ); 
        if (rx_cu > rxc_cu ){
            rx_cu = rxc_cu;
        }
        wasted_txcu = ((dcs_params->wastedtxtime >> 8) * 100) / (ts_d>>8) ;
        if (tx_cu < wasted_txcu){
            wasted_txcu = tx_cu;
        }
        tx_err = (tx_cu && wasted_txcu)? (wasted_txcu * 100)/tx_cu : 0;

        unusable_cu = (total_cu >= (tx_cu + rx_cu))? (total_cu - tx_cu - rx_cu) : 0;

        /* rx loss is included in unusable_cu */
        total_wasted_cu = unusable_cu + wasted_txcu;
        
        ofdmphyerr_rate = ani_stats->ofdmphyerr_cnt * 1000 / ani_stats->listen_time;
        cckphyerr_rate  = ani_stats->cckphyerr_cnt * 1000 / ani_stats->listen_time;
        maxerr_rate     = MAX( ofdmphyerr_rate, cckphyerr_rate);
        maxerr_cnt      = MAX(ani_stats->ofdmphyerr_cnt , ani_stats->cckphyerr_cnt);

        if ((maxerr_rate >=  sc->sc_phyerr_rate_thresh) && (maxerr_cnt >=  sc->sc_phyerr_rate_thresh)){
            higherr = 1;
        }else if(dcs_params->phyerrcnt > RADARERR_THRESH){
            higherr = 1;
             //this may happen because of hw limitations, not sure 
             //if we should change channal here 
        }else{
            higherr = 0;
        }

        /* following is only for debugging */ 

        DPRINTF(sc, ATH_DEBUG_DCS,"DCS: totcu %u txcu %u rxcu %u %u txerr %u\n",
                total_cu, tx_cu, rx_cu, rxc_cu, tx_err);
        DPRINTF(sc, ATH_DEBUG_DCS,"DCS: unusable_cu %d wasted_cu %u thp %u ts %u\n",
                unusable_cu, total_wasted_cu, cur_thp, ts_d );
        DPRINTF(sc, ATH_DEBUG_DCS,"DCS: err %d %u %u %u time %u\n", higherr, ani_stats->ofdmphyerr_cnt,
                ani_stats->cckphyerr_cnt, dcs_params->phyerrcnt, ani_stats->listen_time);

    }

     dcs_params->samp_cnt++;
    if ((unusable_cu >= sc->sc_coch_intr_thresh) || (total_wasted_cu >= MAX(40,(sc->sc_coch_intr_thresh+10))) ||
            ((tx_cu > 30) && (tx_err >= sc->sc_tx_err_thresh) && higherr) ) {
        /* far range case is excluded here, as wasted ch timer is updated 
           only when RSSI is above a threshold.  
           To Do list :
           Tune the parameters just after apup, when the performnace is good.
           Update sc_coch_intr_thresh with the curr ch load of channel
           chosen by EACS */
        dcs_params->intr_cnt++;
        DPRINTF(sc, ATH_DEBUG_DCS,"DCS: %s: INTR DETECTED\n",__func__ );
        if(dcs_params->intr_cnt >= INTR_DETECTION_CNT){
            DPRINTF(sc, ATH_DEBUG_DCS,"DCS: %s: Trigger EACS \n",__func__);
            printk("Interference Detected, Changing Channel\n");
            sc->sc_ieee_ops->interference_handler(sc->sc_ieee);
            dcs_params->intr_cnt = 0;
            dcs_params->samp_cnt = 0; 
        }
    }

    if( dcs_params->samp_cnt == 10 ){
         /* if this condition is true 50% of the time, change the channel */ 
        if(dcs_params->intr_cnt >= INTR_DETECTION_CNT){
            DPRINTF(sc, ATH_DEBUG_DCS,"DCS: %s: Trigger EACS \n",__func__);
            printk("Interference Detected, Changing Channel\n");
            sc->sc_ieee_ops->interference_handler(sc->sc_ieee);
        }
        dcs_params->intr_cnt = 0;
        dcs_params->samp_cnt = 0; 
     } 

terminate:
    /* reset counters */
    dcs_params->bytecntrx = 0;             /* Running received data sw counter */
    dcs_params->rxtime = 0;
    dcs_params->wastedtxtime = 0;
    dcs_params->bytecnttx = 0;           /* Running transmitted data sw counter */
    dcs_params->phyerrcnt = 0;
    dcs_params->prev_rxclr = ani_stats->rxclr_cnt;
    dcs_params->prev_ts = cur_ts;
     
    return 0;
}
#endif


/******************************************************************************/
/*!
**  \brief Calibration Task
**
**  This routine performs the periodic noise floor calibration function
**  that is used to adjust and optimize the chip performance.  This
**  takes environmental changes (location, temperature) into account.
**  When the task is complete, it reschedules itself depending on the
**  appropriate interval that was calculated.
**
**  \param xxx Parameters are OS dependant.  See the definition of the
**             OS_TIMER_FUNC macro
**
**  \return N/A
*/

static int
ath_calibrate(void *arg)
{
    struct ath_softc    *sc = (struct ath_softc *)arg;
    struct ath_hal      *ah;
    HAL_BOOL            longcal    = AH_FALSE;
    HAL_BOOL            shortcal   = AH_FALSE;
    HAL_BOOL            olpccal    = AH_FALSE;
    HAL_BOOL            aniflag    = AH_FALSE;
    HAL_BOOL            multical   = AH_FALSE;
    HAL_BOOL            work_to_do = AH_FALSE;
    systime_t           timestamp  = OS_GET_TIMESTAMP();
    u_int32_t           i, sched_cals;
    HAL_CALIBRATION_TIMER   *cal_timerp;
    u_int32_t           ani_interval, skip_paprd_cal = 0;
#ifdef __linux__
    int                 reset_serialize_locked = 0;
#endif

    ah = sc->sc_ah;

    if (!ath_hal_enabledANI(ah) || 
        !ath_hal_getanipollinterval(ah, &ani_interval)) {
        ani_interval = 0; // zero means ANI disabled
    }

    /* 
     * don't calibrate when we're scanning.
     * we are most likely not on our home channel. 
     */
    if (! sc->sc_scanning && !atomic_read(&sc->sc_in_reset) 
#ifdef __linux__
            && ATH_RESET_SERIALIZE_TRYLOCK(sc)
#endif
            ) {
		int8_t rerun_cals = 0;

#ifdef __linux__
                reset_serialize_locked = 1;
#endif

		/* Background scanning may cause calibration data to be lost. Query the HAL to see
		 * if calibration data is invalid. If so, reschedule all periodic cals to run
		 */
        ATH_PS_WAKEUP(sc);
		if (ath_hal_get_cal_intervals(ah, &sc->sc_cals, HAL_QUERY_RERUN_CALS)) {
			rerun_cals = 1;
   	        for (i=0; i<sc->sc_num_cals; i++) {
                HAL_CALIBRATION_TIMER *calp = &sc->sc_cals[i];
			    sc->sc_sched_cals |= calp->cal_timer_group;
		    }
		}
        ATH_PS_SLEEP(sc);

        /*
         * Long calibration runs independently of short calibration.
         */
        if (CONVERT_SYSTEM_TIME_TO_MS(timestamp - sc->sc_longcal_timer) >= ATH_LONG_CALINTERVAL) {
            longcal = AH_TRUE;
            sc->sc_longcal_timer = timestamp;
        }

        /*
         * Short calibration applies only while (sc->sc_Caldone == AH_FALSE)
         */
        if (! sc->sc_Caldone) {
            if (CONVERT_SYSTEM_TIME_TO_MS(timestamp - sc->sc_shortcal_timer) >= ATH_SHORT_CALINTERVAL) {
                shortcal = AH_TRUE;
                sc->sc_shortcal_timer = timestamp;
            }
        } else {
            for (i=0; i<sc->sc_num_cals; i++) {
                cal_timerp = &sc->sc_cals[i];

				if (rerun_cals ||
					CONVERT_SYSTEM_TIME_TO_MS(timestamp - cal_timerp->cal_timer_ts) >= cal_timerp->cal_timer_interval) {
					ath_hal_reset_calvalid(ah, &sc->sc_curchan, &sc->sc_Caldone, cal_timerp->cal_timer_group);
					if (sc->sc_Caldone == AH_TRUE) {
						cal_timerp->cal_timer_ts = timestamp;
					} else {
						sc->sc_sched_cals |= cal_timerp->cal_timer_group;
						multical = AH_TRUE;
					}
				}
			}
        }
 
        /*
         * Verify whether we must check ANI
         */
        if (ani_interval &&
			CONVERT_SYSTEM_TIME_TO_MS(timestamp - sc->sc_ani_timer) >= ani_interval) {
            aniflag = AH_TRUE;
            sc->sc_ani_timer = timestamp;
        }

        if (ath_hal_isOpenLoopPwrCntrl(ah)) {
            if (CONVERT_SYSTEM_TIME_TO_MS(timestamp - sc->sc_olpc_timer) >= ATH_OLPC_CALINTERVAL) {
                olpccal = AH_TRUE;
                sc->sc_olpc_timer = timestamp;
            }
        }

        /*
         * Decide whether there's any work to do
         */
        work_to_do = longcal || shortcal || aniflag || multical || olpccal;
    }

    /*
     * Skip all processing if there's nothing to do.
     */
    if (work_to_do) {
        ATH_PS_WAKEUP(sc);
        
        /* Call ANI routine if necessary */
        if (aniflag) {
            HAL_ANISTATS ani_stats;
            ath_hal_rxmonitor(ah, &sc->sc_halstats, &sc->sc_curchan, &ani_stats);
#if ATH_SUPPORT_VOW_DCS
            if (sc->sc_dcs_enable){
                if (!(sc->sc_dfs && sc->sc_dfs->sc_dfswait)) {
                    ath_sc_chk_intr(sc, &ani_stats);
                }
            }
#endif
        }

        if (olpccal) {
            ath_hal_olpcTempCompensation(ah);
        }
        
        /* Perform calibration if necessary */
        if (longcal || shortcal || multical) {
            HAL_BOOL    isCaldone = AH_FALSE;

            sc->sc_stats.ast_per_cal++;
        
            if (ath_hal_getrfgain(ah) == HAL_RFGAIN_NEED_CHANGE) {
                /*
                 * Rfgain is out of bounds, reset the chip
                 * to load new gain values.
                 */
                DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s: RFgain out of bounds, resetting device.\n", __func__);
                sc->sc_stats.ast_per_rfgain++;
                sc->sc_curchan.paprdDone = 0;
                skip_paprd_cal = 1;
                ath_internal_reset(sc);
            }
       
            sched_cals = sc->sc_sched_cals;
            if (ath_hal_calibrate(ah, &sc->sc_curchan, sc->sc_rx_chainmask, longcal, &isCaldone, sc->sc_scanning, &sc->sc_sched_cals)) {
				for (i=0; i<sc->sc_num_cals; i++) {
					cal_timerp = &sc->sc_cals[i];
					if (cal_timerp->cal_timer_group & sched_cals) {
						if (sched_cals ^ sc->sc_sched_cals) {
							cal_timerp->cal_timer_ts = timestamp;
						}
					}
				}
				
				/* XXX Need to investigate why ath_hal_getChanNoise is returning a 
                 * value that seems to be incorrect 
                 * (-66dBm on the last test using CB72).
                 */
                if (longcal) {
                    sc->sc_noise_floor = 
                        ath_hal_getChanNoise(ah, &sc->sc_curchan);
                }
		//printk("%s[%d] isCaldone %d\n", __func__, __LINE__, isCaldone);
                if (sc->sc_paprd_enable && isCaldone) {
                    if (ath_hal_is_skip_paprd_by_greentx(ah)) {
                        /* reset paprd related parameters */
                        sc->sc_paprd_lastchan_num = 0;
                        sc->sc_curchan.paprdDone  = 0;
                        sc->sc_paprd_done_chain_mask = 0;
                        ath_hal_paprd_enable(sc->sc_ah, AH_FALSE, 
                            &sc->sc_curchan);
                        DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: "
                           "disable paprd by green tx.\n", __func__, __LINE__);
                    } else {
                        int retval;
                        if (!sc->sc_scanning && 
                            (sc->sc_curchan.paprdDone == 0) && !skip_paprd_cal
                            && !sc->sc_paprd_cal_started)
                        {
                            sc->sc_paprd_retrain_count = 0;
                            sc->sc_paprd_cal_started = 1;
                            retval = ath_paprd_cal(sc);
                            DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: "
                            "ath_paprd_cal retval %d\n", __func__, __LINE__, 
                            retval);
                            if (sc->sc_curchan.paprdDone == 0) {
                                ath_hal_paprd_dec_tx_pwr(sc->sc_ah);
                            }
                        }
                        if (!sc->sc_scanning && (sc->sc_curchan.paprdDone == 1) 
                            && (sc->sc_curchan.paprdTableWriteDone == 0))
                        {
                            retval = ath_apply_paprd_table(sc);
                            DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: "
                                "ath_apply_paprd_table retval %d \n", __func__,
                                __LINE__, retval);
                        }
                        DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: isCaldone %d paprd_enable "
                            "%d paprdDone %d Apply %d\n", __func__, __LINE__, isCaldone,
                            sc->sc_paprd_enable, sc->sc_curchan.paprdDone, 
                            sc->sc_curchan.paprdTableWriteDone);
                    }
                }
                DPRINTF(sc, ATH_DEBUG_CALIBRATE,
                    "%d.%03d | %s: channel %u/%x noise_floor=%d\n",
                    ((u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(timestamp)) / 1000,
                    ((u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(timestamp)) % 1000,
                    __func__, 
                    sc->sc_curchan.channel, 
                    sc->sc_curchan.channelFlags,
                    sc->sc_noise_floor);
            } else {
                DPRINTF(sc, ATH_DEBUG_ANY,
                    "%d.%03d | %s: calibration of channel %u/%x failed\n",
                    ((u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(timestamp)) / 1000,
                    ((u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(timestamp)) % 1000,
                    __func__,
                    sc->sc_curchan.channel,
                    sc->sc_curchan.channelFlags);
                sc->sc_stats.ast_per_calfail++;
            }
            sc->sc_Caldone = isCaldone; 
#if ATH_SUPPORT_SPECTRAL
            if (sc->sc_curchan.channelFlags & CHANNEL_CW_INT) {
                spectral_send_eacs_msg(sc);
                sc->sc_curchan.channelFlags &= (~CHANNEL_CW_INT);
            }
#endif
        }

        ATH_PS_SLEEP(sc);
    }

#ifdef __linux__
    if (reset_serialize_locked)
        ATH_RESET_SERIALIZE_UNLOCK(sc);
#endif

    /*
     * Set timer interval based on previous results.
     * The interval must be the shortest necessary to satisfy ANI,
     * short calibration and long calibration.
     *
     */
    ath_set_timer_period(&sc->sc_cal_ch,
                         ath_get_cal_interval(sc->sc_Caldone, ani_interval, 
                                              ath_hal_isOpenLoopPwrCntrl(ah), sc->sc_min_cal));
    return 0;  /* Re-arm */
}

#if ATH_LOW_PRIORITY_CALIBRATE
static int
ath_calibrate_workitem(void *arg)
{
    struct ath_softc    *sc = (struct ath_softc *)arg;
    
    return ath_cal_workitem(sc, ath_calibrate, arg);
}
#endif

#if ATH_TRAFFIC_FAST_RECOVER
/* EV #75248 */
static int
ath_traffic_fast_recover(void *arg)
{
    struct ath_softc            *sc = (struct ath_softc *)arg;
    struct ath_hal              *ah;
    u_int32_t rx_packets;
    u_int32_t pll3_sqsum_dvc;
    
    static u_int32_t last_rx_packets = 0;

    rx_packets = sc->sc_stats.ast_11n_stats.rx_pkts;

    ah = sc->sc_ah;
    
    if (rx_packets - last_rx_packets <= 5) {
        pll3_sqsum_dvc = ath_hal_get_pll3_sqsum_dvc(ah);

        if (pll3_sqsum_dvc >= 0x40000) {
            ath_internal_reset(sc);
        }
    }

    last_rx_packets = rx_packets;

    return 0;
}
#endif
#ifdef ATH_SUPPORT_TxBF
#ifdef TXBF_TODO
#define BF_Period_Time	20*1000
#define BF_Retry_Time	2*1000
#define BF_Retry_limit	2

static int
ath_sc_start_imbf_cal(void *arg)//BF Timer
{
    struct ath_softc		*sc = (struct ath_softc *)arg;
	struct ieee80211com		*ic = (struct ieee80211com *)sc->sc_ieee;
	struct ieee80211_node	*ni; 

    DPRINTF(sc, ATH_DEBUG_ANY,"==>%s:\n", __func__);

    if (ic->ic_rc_calibrating) {
        /* Set Timer again for retry */

        DPRINTF(sc, ATH_DEBUG_ANY,"==>%s Retry(2*1000)-ath_set_timer_period of BF--------------\n", __FUNCTION__);
        ath_set_timer_period(&sc->sc_imbf_cal_short, 2000);
        if (sc->only_bf_cal_allow != 0) {
            DPRINTF(sc, ATH_DEBUG_ANY,"==>%s:previous cal not completed yet!!\n", __func__);
            return 0;
        }
        DPRINTF(sc, ATH_DEBUG_ANY,"==>%s:start RC calibration\n", __func__);
        ni = ic->ic_ieee80211_find_node(&ic->ic_sta, sc->join_macaddr); 
	/*	if(ni->ni_associd == 0) {
		  printk("==>%s New(ni_associd == 0) --ath_set_timer_period of BF---------\n",__FUNCTION__);
		  ath_set_timer_period(&sc->sc_imbf_cal_short,500);
		  sc->radio_cal_process = 0;
		  return 0;
        }*/
        if (sc->radio_cal_process == 0)
            ni->Calibration_Done = 0;
        }

        if(sc->radio_cal_process > 5) {
            DPRINTF(sc, ATH_DEBUG_ANY,"==>%s: RC calibration fail, stop retry!!\n", __func__);
            ic->ic_rc_calibrating = 0;
            ic->ic_rc_20M_valid = 0;
            ic->ic_rc_40M_valid = 0;
            ath_set_timer_period(&sc->sc_imbf_cal_short, 1000*3600);
            return 0;
        }
        if(ni) {
          DPRINTF(sc, ATH_DEBUG_ANY,"==>%s:Send QoS Null===============Start\n", __func__);
            sc->radio_cal_process++;
            sc->only_bf_cal_allow = 0xA;
            ic->ic_ieee80211_send_cal_qos_nulldata(ni, 0); 
            ic->ic_ieee80211_send_cal_qos_nulldata(ni, 1);
            sc->only_bf_cal_allow = 0;
            ic->ic_rc_wait_csi = 1;
            DPRINTF(sc, ATH_DEBUG_ANY,"==>Send QoS Null===============End, %d try\n", sc->radio_cal_process);
        } else {
            DPRINTF(sc, ATH_DEBUG_ANY,"==>%s:Not Having node-------------- \n", __func__);
        }
        }
    } else {
       ni = ic->ic_ieee80211_find_node(&ic->ic_sta, sc->join_macaddr); 
        if (ni){
            if (ni->Calibration_Done) {
                ni->Calibration_Done=0;
                ath_set_timer_period(&sc->sc_imbf_cal_short, 5000); // just calibrated, set long timer
                return 0;
            }
        }
        DPRINTF(sc, ATH_DEBUG_ANY,"==>%s:Set RC invalid:\n",__func__);
        ic->ic_rc_20M_valid = 0;
        ic->ic_rc_40M_valid = 0;
        ath_set_timer_period(&sc->sc_imbf_cal_short, 1000*3600);
    }

	return 0;
}
#endif

/* this timer routine is used to reset lowest tx rate for self-generated frame*/
static int
ath_sc_reset_lowest_txrate(void *arg)
{
    struct ath_softc		*sc = (struct ath_softc *)arg;
    struct ath_hal          *ah = sc->sc_ah;
    
    /* reset rate to maximum tx rate*/
    ath_hal_resetlowesttxrate(ah);
    
    /* restart timer*/
    ath_set_timer_period(&sc->sc_selfgen_timer, LOWEST_RATE_RESET_PERIOD);
    return 0;
}
#endif

/******************************************************************************/
/*!
**  \brief Start Scan
**
**  This function is called when starting a channel scan.  It will perform
**  power save wakeup processing, set the filter for the scan, and get the
**  chip ready to send broadcast packets out during the scan.
**
**  \param dev Void pointer to ATH object passed from protocol layer
**
**  \return N/A
*/

static void
ath_scan_start(ath_dev_t dev)
{
    struct ath_softc    *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal      *ah = sc->sc_ah;
    u_int32_t           rfilt;

    /*
     * Make sure chip is awake before modifying any registers.
     */
    ATH_PS_WAKEUP(sc);
    sc->sc_scanning = 1;
    rfilt = ath_calcrxfilter(sc);
    ath_hal_setrxfilter(ah, rfilt);
    ath_hal_setassocid(ah, ath_bcast_mac, 0);

#ifdef ATH_BT_COEX
    if (sc->sc_hasbtcoex) {
        u_int32_t   scanState = ATH_COEX_WLAN_SCAN_START;
        ath_bt_coex_event(sc, ATH_COEX_EVENT_WLAN_SCAN, &scanState);
    }
#endif

    /*
     * Restore previous power management state.
     */
    ATH_PS_SLEEP(sc);

    DPRINTF_TIMESTAMP(sc, ATH_DEBUG_STATE,
        "%s: RX filter 0x%x aid 0\n", __func__, rfilt);
}

/******************************************************************************/
/*!
**  \brief Scan End
**
**  This routine is called by the upper layer when the scan is completed.  This
**  will set the filters back to normal operating mode, set the BSSID to the
**  correct value, and restore the power save state.
**
**  \param dev Void pointer to ATH object passed from protocol layer
**
**  \return N/A
*/

static void
ath_scan_end(ath_dev_t dev)
{
    struct ath_softc    *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal      *ah = sc->sc_ah;
    u_int32_t           rfilt;

    /*
     * Make sure chip is awake before modifying any registers.
     */
    ATH_PS_WAKEUP(sc);
    sc->sc_scanning = 0;

    /*
     * Request for a full reset due to rx packet filter changes.
     */
    sc->sc_full_reset = 1;
    rfilt = ath_calcrxfilter(sc);
    ath_hal_setrxfilter(ah, rfilt);
    ath_hal_setassocid(ah, sc->sc_curbssid, sc->sc_curaid);

#ifdef ATH_BT_COEX
    if (sc->sc_hasbtcoex) {
        u_int32_t   scanState = ATH_COEX_WLAN_SCAN_END;
        ath_bt_coex_event(sc, ATH_COEX_EVENT_WLAN_SCAN, &scanState);
    }
#endif

    /*
     * Restore previous power management state.
     */
    ATH_PS_SLEEP(sc);

    DPRINTF_TIMESTAMP(sc, ATH_DEBUG_STATE, "%s: RX filter 0x%x aid 0x%x\n",
        __func__, rfilt, sc->sc_curaid);
}

/******************************************************************************
 *  * Schedule all transmit queues if they are not scheduled
 *  * Scanning disables all txqs (drain_txq), this has to be called
 *  * from both from entry_bss_chan and scan_terminate
 * ******************************************************************************/
void
ath_scan_enable_txq(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int i =0;
    struct ath_txq *txq = NULL;

    ATH_FUNC_ENTRY_VOID(sc);
    if (sc->sc_invalid)         /* if the device is invalid or removed */
        return;
    for(i=0; i < HAL_NUM_TX_QUEUES; i++) {
        txq = &sc->sc_txq[i];
        ATH_TXQ_LOCK(txq);
        if( (ath_hal_numtxpending(sc->sc_ah, i)) == 0)
          ath_txq_schedule(sc, txq);
        ATH_TXQ_UNLOCK(txq);
    }
}
/******************************************************************************/
/*!
**  \brief Set Channel
**
**  This is the entry point from the protocol level to chang the current
**  channel the AP is serving.
**
**  \param dev Void pointer to ATH object passed from the protocol layer
**  \param hchan Pointer to channel structure with new channel information
**  \param tx_timeout Time in us to wait for transmit DMA to stop (0 - use default timeout)
**  \param rx_timeout Time in us to wait for receive DMA to stop (0 - use default timeout)
**  \param flush If true, packets in the hw queue will be dropped, else they shall be requeued into sw queues and will be transmitted later
**
**  \return N/A
*/

void
ath_set_channel(ath_dev_t dev, HAL_CHANNEL *hchan, int tx_timeout, int rx_timeout, bool flush)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (sc->sc_invalid)         /* if the device is invalid or removed */
        return;

    ATH_FUNC_ENTRY_VOID(sc);

    /*
     * Make sure chip is awake before modifying any registers.
     */
    ATH_PS_WAKEUP(sc);
    
    (void) ath_chan_set(sc, hchan, tx_timeout, rx_timeout, !flush);

    /*
     * Restore previous power management state.
     */
    ATH_PS_SLEEP(sc);
}

/******************************************************************************/
/*!
**  \brief Return the current channel
**
**  \param dev Void pointer to ATH object passed from the protocol layer
**
**  \return pointer to current channel descriptor
*/
static HAL_CHANNEL *
ath_get_channel(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (sc->sc_invalid) return NULL; /* if the device is invalid or removed */
    return &sc->sc_curchan;
}

/******************************************************************************/
/*!
**  \brief Get MAC address
**
**  Copy the MAC address into the buffer provided
**
**  \param dev Void pointer to ATH object passed from protocol layer
**  \param macaddr Pointer to buffer to place MAC address into
**
**  \return N/A
*/

static void
ath_get_mac_address(ath_dev_t dev, u_int8_t macaddr[IEEE80211_ADDR_LEN])
{
    OS_MEMCPY(macaddr, ATH_DEV_TO_SC(dev)->sc_myaddr, IEEE80211_ADDR_LEN);
}

static void
ath_get_hw_mac_address(ath_dev_t dev, u_int8_t macaddr[IEEE80211_ADDR_LEN])
{
    OS_MEMCPY(macaddr, ATH_DEV_TO_SC(dev)->sc_my_hwaddr, IEEE80211_ADDR_LEN);
}

/******************************************************************************/
/*!
**  \brief set MAC address
**
**  Copies the provided MAC address into the proper ATH member, and sets the
**  value into the chipset through the HAL.
**
**  \param dev Void pointer to ATH object passed from protocol layer
**  \param macaddr buffer containing MAC address to set into hardware
**
**  \return N/A
*/

static void
ath_set_mac_address(ath_dev_t dev, const u_int8_t macaddr[IEEE80211_ADDR_LEN])
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    OS_MEMCPY(sc->sc_myaddr, macaddr, IEEE80211_ADDR_LEN);
    ath_hal_setmac(sc->sc_ah, sc->sc_myaddr);
}

static void
ath_set_rxfilter(ath_dev_t dev) {
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t        rxfilter;

    /*
     * Make sure chip is awake before modifying any registers.
     */
    ATH_PS_WAKEUP(sc);

    rxfilter = ath_calcrxfilter(sc);
    ath_hal_setrxfilter(sc->sc_ah, rxfilter);

    /*
     * Restore previous power management state.
     */
    ATH_PS_SLEEP(sc);
}

void ath_set_bssid_mask(ath_dev_t dev, const u_int8_t bssid_mask[IEEE80211_ADDR_LEN])
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    OS_MEMCPY(sc->sc_bssidmask, bssid_mask, IEEE80211_ADDR_LEN);
    ath_hal_setbssidmask(sc->sc_ah, sc->sc_bssidmask);
}

static int
ath_set_sel_evm(ath_dev_t dev, int selEvm, int justQuery) {
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int old_value;

    /*
     * Make sure chip is awake before modifying any registers.
     */
    if (!justQuery) ATH_PS_WAKEUP(sc);

    old_value = ath_hal_setrxselevm(sc->sc_ah, selEvm, justQuery);

    /*
     * Restore previous power management state.
     */
    if (!justQuery) ATH_PS_SLEEP(sc);

    return old_value;
}

#if ATH_SUPPORT_WIRESHARK
static int
ath_monitor_filter_phyerr(ath_dev_t dev, int filterPhyErr, int justQuery)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int old_val = sc->sc_wireshark.filter_phy_err;
    if (!justQuery) {
        sc->sc_wireshark.filter_phy_err = filterPhyErr != 0;
    }
    return old_val;
}
#endif /* ATH_SUPPORT_WIRESHARK */

/******************************************************************************/
/*!
**  \brief set multi-cast addresses
**
**  Copies the provided MAC address into the proper ATH member, and sets the
**  value into the chipset through the HAL.
**
**  \param dev Void pointer to ATH object passed from protocol layer
**  \param macaddr buffer containing MAC address to set into hardware
**
**  \return N/A
*/
static void
ath_set_mcastlist(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t mfilt[2];

    if (sc->sc_invalid)         /* if the device is invalid or removed */
        return;

    ATH_FUNC_ENTRY_VOID(sc);

    /* calculate and install multicast filter */
    if (sc->sc_ieee_ops->get_netif_settings(sc->sc_ieee) & ATH_NETIF_ALLMULTI) {
        mfilt[0] = mfilt[1] = ~0;
    }
    else {
        sc->sc_ieee_ops->netif_mcast_merge(sc->sc_ieee, mfilt);
    }

    /*
     * Make sure chip is awake before modifying any registers.
     */
    ATH_PS_WAKEUP(sc);

    ath_hal_setmcastfilter(ah, mfilt[0], mfilt[1]);

    /*
     * Restore previous power management state.
     */
    ATH_PS_SLEEP(sc);
}

/******************************************************************************/
/*!
**  \brief Enter Scan State
**
**  This routine will call the LED routine that "blinks" the light indicating
**  the device is in Scan mode. This sets no other status.
**
**  \param dev Void pointer to ATH object passed from protocol layer
**
**  \return N/A
*/

static void 
ath_enter_led_scan_state(ath_dev_t dev)
{
    ath_led_scan_start(&(ATH_DEV_TO_SC(dev)->sc_led_control));
}

/******************************************************************************/
/*!
**  \brief Leave Scan State
**
**  Calls the LED Scan End function to change the LED state back to normal.
**  Does not set any other states.
**
**  \param dev Void pointer to ATH object passed from protocol layer
**
**  \return N/A
*/

static void
ath_leave_led_scan_state(ath_dev_t dev)
{
    ath_led_scan_end(&(ATH_DEV_TO_SC(dev)->sc_led_control));
}

#ifdef ATH_USB
static void
ath_heartbeat_callback(ath_dev_t dev)
{
    //struct ath_softc   *sc = ATH_DEV_TO_SC(dev);

}
#endif

/******************************************************************************/
/*!
**  \brief Activate ForcePPM
**
**  Activates/deactivates ForcePPM.
**  Rules for ForcePPM activation depend on the operation mode:
**      -STA:   active when connected.
**      -AP:    active when a single STA is connected to it.
**      -ADHOC: ??
**
**  \param dev      Void pointer to ATH object passed from protocol layer
**  \param enable   Flag indicating whether to enable ForcePPM
**  \param bssid    Pointer to BSSID. Must point to a valid BSSID if flag 'enable' is set.
**
**  \return N/A
*/

static void
ath_notify_force_ppm(ath_dev_t dev, int enable, u_int8_t *bssid)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    /* Notify PPM module that we are returning to the home channel; report connection status */
    ath_force_ppm_notify(&sc->sc_ppm_info, enable, bssid);
}

/******************************************************************************/
/*!
**  \brief Down VAP instance
**
**  This routine will stop the indicated VAP and put it in a "down" state.
**  The down state is basically an initialization state that can be brought
**  back up by callint the opposite up routine.
**  This routine will bring the interface out of power save mode, set the
**  LED states, update the rate control processing, stop DMA transfers, and
**  set the VAP into the down state.
**
**  \param dev      Void pointer to ATH object passed from protocol layer
**  \param if_id    Devicescape style interface identifier, integer
**  \param flags    Flags indicating device capabilities
**
**  \return -EINVAL if the ID is invalid
**  \return  0 on success
*/

static int
ath_vap_stopping(ath_dev_t dev, int if_id)
{
#define WAIT_BEACON_INTERVAL 100
    u_int32_t elapsed_time = 0;
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    struct ath_vap *avp;

    avp = sc->sc_vaps[if_id];

    if (avp == NULL) {
        DPRINTF(sc, ATH_DEBUG_STATE, "%s: invalid interface id %u\n",
                __func__, if_id);
        return -EINVAL;
    }
#ifdef ATH_SUPPORT_DFS
    if (sc->sc_dfs->sc_dfswait) {
        /* To fix EV# 89566 and kernel panic observed when DFS Wait timer is 
         * started and AP state is pulled down before this timer is expired
         */
        if(avp->av_opmode == HAL_M_HOSTAP)
        {
            OS_CANCEL_TIMER(&sc->sc_dfs->sc_dfswaittimer);
            sc->sc_dfs->sc_dfswait = 0;
            ath_hal_cac_tx_quiet(sc->sc_ah, 0);
        }
    }
#endif

    ath_beacon_config(sc, ATH_BEACON_CONFIG_REASON_VAP_DOWN,if_id);

    atomic_set(&avp->av_stop_beacon, 1);

    /*
     * Wait until ath_beacon_generate complete
     */
    do {
        if (!atomic_read(&avp->av_beacon_cabq_use_cnt))
            break;

        OS_DELAY(WAIT_BEACON_INTERVAL);
        elapsed_time += WAIT_BEACON_INTERVAL;

        if (elapsed_time > (1000 * WAIT_BEACON_INTERVAL)) {
           DPRINTF(sc, ATH_DEBUG_ANY,"%s: Rx beacon_cabq_use_count stuck. Investigate!!!\n", __func__);
        }
    } while (1);

    /*
     * Reclaim any pending mcast bufs on the vap.
     * so that vap bss node ref could be decremented
     * and vap could be deleted.
     */
    ath_tx_mcast_draintxq(sc, &avp->av_mcastq);

    /* Reclaim beacon resources */
    if (avp->av_opmode == HAL_M_HOSTAP || avp->av_opmode == HAL_M_IBSS) {
        /* For now, there is only one beacon source. Just stop beacon dma */
        if (!sc->sc_removed)
            ath_hal_stoptxdma(ah, sc->sc_bhalq, 0);

        ath_beacon_return(sc, avp);
    }

    atomic_set(&avp->av_stop_beacon, 0);

    return 0;
#undef WAIT_BEACON_INTERVAL
}

static int
ath_vap_down(ath_dev_t dev, int if_id, u_int flags)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    struct ath_vap *avp;
    u_int32_t rfilt = 0;
    u_int32_t imask = 0;

    avp = sc->sc_vaps[if_id];
    if (avp == NULL) {
        DPRINTF(sc, ATH_DEBUG_STATE, "%s: invalid interface id %u\n",
                __func__, if_id);
        return -EINVAL;
    }

    ath_pwrsave_awake(sc);

    ATH_PS_WAKEUP(sc);

    if (avp->av_up) {
        if (avp->av_opmode == HAL_M_STA) {
            /* use atomic variable */
            atomic_dec(&sc->sc_nsta_vaps_up);
        } else if (avp->av_opmode == HAL_M_HOSTAP) {
            atomic_dec(&sc->sc_nap_vaps_up);
        }
        /* recalculate the rx filter */
        rfilt = ath_calcrxfilter(sc);
        ath_hal_setrxfilter(ah, rfilt);
    }

    avp->av_up=0;


#if ATH_SLOW_ANT_DIV
    if (sc->sc_slowAntDiv)
        ath_slow_ant_div_stop(&sc->sc_antdiv);
#endif

    /* set LEDs to INIT state */
    ath_led_set_state(&sc->sc_led_control, HAL_LED_INIT);

    /* update ratectrl about the new state */
    ath_rate_newstate(sc, avp, 0);


    if (flags & ATH_IF_HW_OFF) {
        HAL_BOOL gpio_intr = AH_FALSE;

#ifdef ATH_SUPPORT_TxBF
#ifdef TXBF_TODO
        if (sc->sc_txbfsupport == AH_TRUE) {
            ath_cancel_timer(&sc->sc_imbf_cal_short, CANCEL_NO_SLEEP);
        }
#endif
#endif  // for TxBF RC
        ath_cancel_timer(&sc->sc_cal_ch, CANCEL_NO_SLEEP);        /* periodic calibration timer */
        
        if (ath_timer_is_initialized(&sc->sc_up_beacon)) {
            ath_cancel_timer(&sc->sc_up_beacon, CANCEL_NO_SLEEP); /* beacon config timer */
            sc->sc_up_beacon_purge = 0;
        }
            
        sc->sc_imask &= ~(HAL_INT_SWBA | HAL_INT_BMISS);
#if ATH_TRAFFIC_FAST_RECOVER
        if (ath_timer_is_initialized(&sc->sc_traffic_fast_recover_timer)) {
            ath_cancel_timer(&sc->sc_traffic_fast_recover_timer, CANCEL_NO_SLEEP);
        }
#endif
        /*
         * Disable interrupts other than GPIO to capture RfKill event.
         */
#ifdef ATH_RFKILL
        if (sc->sc_hasrfkill && ath_rfkill_hasint(sc))
            gpio_intr = AH_TRUE;
#endif
        if (gpio_intr)
            ath_hal_intrset(ah, HAL_INT_GLOBAL | HAL_INT_GPIO);
        else
            ath_hal_intrset(ah, sc->sc_imask &~ HAL_INT_GLOBAL);

        imask = ath_hal_intrget(ah);
        if (imask & HAL_INT_GLOBAL)
            ath_hal_intrset(ah, 0);
        ath_draintxq(sc, AH_FALSE, 0);  /* stop xmit side */
        if (imask & HAL_INT_GLOBAL)
            ath_hal_intrset(ah, imask);
    }

#ifdef ATH_SUPERG_XR
    if (ieee80211vap_has_flag(vap, IEEE80211_F_XR) && sc->sc_xrgrppoll) {
        ath_grppoll_stop(vap);
    }
#endif
    ATH_PS_SLEEP(sc);
    return 0;
}

/******************************************************************************/
/*!
**  \brief VAP in Listen mode
**
**  This routine brings the VAP out of the down state into a "listen" state
**  where it waits for association requests.  This is used in AP and AdHoc
**  modes.
**
**  \param dev      Void pointer to ATH object passed from protocol layer
**  \param if_id    Devicescape style interface identifier, integer
**
**  \return -EINVAL if interface ID is not valid
**  \return  0 on success
*/

static int
ath_vap_listen(ath_dev_t dev, int if_id)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    struct ath_vap *avp;
    u_int32_t rfilt = 0;

    avp = sc->sc_vaps[if_id];
    if (avp == NULL) {
        DPRINTF(sc, ATH_DEBUG_STATE, "%s: invalid interface id %u\n",
                __func__, if_id);
        return -EINVAL;
    }

    ath_pwrsave_awake(sc);

    ATH_PS_WAKEUP(sc);

#ifdef ATH_SUPPORT_TxBF
#ifdef TXBF_TODO
    if (sc->sc_txbfsupport == AH_TRUE) {
        ath_cancel_timer(&sc->sc_imbf_cal_short, CANCEL_NO_SLEEP);
    }
#endif
#endif

    ath_cancel_timer(&sc->sc_cal_ch, CANCEL_NO_SLEEP);        /* periodic calibration timer */

#if ATH_TRAFFIC_FAST_RECOVER
    if (ath_timer_is_initialized(&sc->sc_traffic_fast_recover_timer)) {
        ath_cancel_timer(&sc->sc_traffic_fast_recover_timer, CANCEL_NO_SLEEP);
    }
#endif
#if ATH_SLOW_ANT_DIV
    if (sc->sc_slowAntDiv)
        ath_slow_ant_div_stop(&sc->sc_antdiv);
#endif

    /* set LEDs to READY state */
    ath_led_set_state(&sc->sc_led_control, HAL_LED_READY);

    /* update ratectrl about the new state */
    ath_rate_newstate(sc, avp, 0);

    rfilt = ath_calcrxfilter(sc);
    ath_hal_setrxfilter(ah, rfilt);

    if (sc->sc_opmode == HAL_M_STA || sc->sc_opmode == HAL_M_IBSS) {
        ath_hal_setassocid(ah, ath_bcast_mac, sc->sc_curaid);
    } else
        sc->sc_curaid = 0;

    DPRINTF(sc, ATH_DEBUG_STATE, "%s: RX filter 0x%x bssid %02x:%02x:%02x:%02x:%02x:%02x aid 0x%x\n",
            __func__, rfilt,
            sc->sc_curbssid[0], sc->sc_curbssid[1], sc->sc_curbssid[2],
            sc->sc_curbssid[3], sc->sc_curbssid[4], sc->sc_curbssid[5],
            sc->sc_curaid);

    /*
     * XXXX
     * Disable BMISS interrupt when we're not associated
     */
    ath_hal_intrset(ah,sc->sc_imask &~ (HAL_INT_SWBA | HAL_INT_BMISS));
    sc->sc_imask &= ~(HAL_INT_SWBA | HAL_INT_BMISS);

    avp->av_dfswait_run=0; /* reset the dfs wait flag */ 

    ATH_PS_SLEEP(sc);
    return 0;
}

/******************************************************************************/
/*!
**  \brief xxxxxe
**
**  -- Enter Detailed Description --
**
**  \param xx xxxxxxxxxxxxx
**  \param xx xxxxxxxxxxxxx
**
**  \return xxx
*/

static int
ath_vap_join(ath_dev_t dev, int if_id, const u_int8_t bssid[IEEE80211_ADDR_LEN], u_int flags)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    struct ath_vap *avp;
    u_int32_t rfilt = 0;

    avp = sc->sc_vaps[if_id];
    if (avp == NULL) {
        DPRINTF(sc, ATH_DEBUG_STATE, "%s: invalid interface id %u\n",
                __func__, if_id);
        return -EINVAL;
    }

    /* update ratectrl about the new state */
    ath_rate_newstate(sc, avp, 0);

    if (!(flags & ATH_IF_HW_ON)) {
        return 0;
    }

    ath_pwrsave_awake(sc);

    ATH_PS_WAKEUP(sc);

#ifdef ATH_SUPPORT_TxBF
#ifdef TXBF_TODO
    if (sc->sc_txbfsupport == AH_TRUE) {
        ath_cancel_timer(&sc->sc_imbf_cal_short, CANCEL_NO_SLEEP);
    }
#endif
#endif

    ath_cancel_timer(&sc->sc_cal_ch, CANCEL_NO_SLEEP);        /* periodic calibration timer */

#if ATH_TRAFFIC_FAST_RECOVER
    if (ath_timer_is_initialized(&sc->sc_traffic_fast_recover_timer)) {
        ath_cancel_timer(&sc->sc_traffic_fast_recover_timer, CANCEL_NO_SLEEP);
    }
#endif
    /* set LEDs to READY state */
    ath_led_set_state(&sc->sc_led_control, HAL_LED_READY);

    rfilt = ath_calcrxfilter(sc);
    ath_hal_setrxfilter(ah, rfilt);

    /*
     * set the bssid and aid values in the following 2 cases.
     * 1) no active stattion vaps and this is the first vap coming up.
     * 2) there is only one station vap and it is rejoining .
     * the case where a second STA vap is coming, do not touch the
     * bssid,aid registers. leave them to the existing  active STA vap. 
     */
    if (atomic_read(&sc->sc_nsta_vaps_up) == 0 ||
         ( atomic_read(&sc->sc_nsta_vaps_up) == 1 &&  avp->av_opmode ==  HAL_M_STA && 
           avp->av_up ) ) {
        ATH_ADDR_COPY(sc->sc_curbssid, bssid);
        sc->sc_curaid = 0;
        ath_hal_setassocid(ah, sc->sc_curbssid, sc->sc_curaid);
    }

    DPRINTF(sc, ATH_DEBUG_STATE, "%s: RX filter 0x%x bssid %02x:%02x:%02x:%02x:%02x:%02x aid 0x%x\n",
            __func__, rfilt,
            sc->sc_curbssid[0], sc->sc_curbssid[1], sc->sc_curbssid[2],
            sc->sc_curbssid[3], sc->sc_curbssid[4], sc->sc_curbssid[5],
            sc->sc_curaid);

    /*
     * Update tx/rx chainmask. For legacy association, hard code chainmask to 1x1,
     * for 11n association, use the chainmask configuration.
     */
    sc->sc_update_chainmask = 1;
    if (flags & ATH_IF_HT) {
        sc->sc_tx_chainmask = sc->sc_config.txchainmask;
        sc->sc_rx_chainmask = sc->sc_config.rxchainmask;
    } else {
        sc->sc_tx_chainmask = sc->sc_config.txchainmasklegacy; 
        sc->sc_rx_chainmask = sc->sc_config.rxchainmasklegacy; 
    }
    sc->sc_rx_numchains = sc->sc_mask2chains[sc->sc_rx_chainmask];
    sc->sc_tx_numchains = sc->sc_mask2chains[sc->sc_tx_chainmask];
    
    /*
     * Enable rx chain mask detection if configured to do so
     */
    if (sc->sc_reg_parm.rxChainDetect)
        sc->sc_rx_chainmask_detect = 1;
    else
        sc->sc_rx_chainmask_detect = 0;
        
    /*
    ** Set aggregation protection mode parameters
    */
    
    sc->sc_config.ath_aggr_prot = sc->sc_reg_parm.aggrProtEnable;
    sc->sc_config.ath_aggr_prot_duration = sc->sc_reg_parm.aggrProtDuration;
    sc->sc_config.ath_aggr_prot_max = sc->sc_reg_parm.aggrProtMax;
    
    /*
     * Reset our TSF so that its value is lower than the beacon that we are
     * trying to catch. Only then hw will update its TSF register with the
     * new beacon. Reset the TSF before setting the BSSID to avoid allowing
     * in any frames that would update our TSF only to have us clear it
     * immediately thereafter.
     */
    ath_hal_resettsf(ah);

    /*
     * XXXX
     * Disable BMISS interrupt when we're not associated
     */
    ath_hal_intrset(ah,sc->sc_imask &~ (HAL_INT_SWBA | HAL_INT_BMISS));
    sc->sc_imask &= ~(HAL_INT_SWBA | HAL_INT_BMISS);

    avp->av_dfswait_run=0; /* reset the dfs wait flag */ 

    ATH_PS_SLEEP(sc);
    return 0;
}

static int
ath_vap_up(ath_dev_t dev, int if_id, const u_int8_t bssid[IEEE80211_ADDR_LEN],
           u_int8_t aid, u_int flags)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    struct ath_vap *avp;
    u_int32_t rfilt = 0;
    int i, error = 0;
    systime_t timestamp = OS_GET_TIMESTAMP();

    ASSERT(if_id != ATH_IF_ID_ANY);
    avp = sc->sc_vaps[if_id];
    if (avp == NULL) {
        DPRINTF(sc, ATH_DEBUG_STATE, "%s: invalid interface id %u\n",
                __func__, if_id);
        return -EINVAL;
    }

    ath_pwrsave_awake(sc);

    ATH_PS_WAKEUP(sc);

#ifdef ATH_SUPPORT_TxBF
#ifdef TXBF_TODO
    if (sc->sc_txbfsupport == AH_TRUE) {
        ath_cancel_timer(&sc->sc_imbf_cal_short, CANCEL_NO_SLEEP);
    }
#endif
#endif

    ath_cancel_timer(&sc->sc_cal_ch, CANCEL_NO_SLEEP);        /* periodic calibration timer */

#if ATH_TRAFFIC_FAST_RECOVER
    if (ath_timer_is_initialized(&sc->sc_traffic_fast_recover_timer)) {
        ath_cancel_timer(&sc->sc_traffic_fast_recover_timer, CANCEL_NO_SLEEP);
    }
#endif
    /* set LEDs to RUN state */
    ath_led_set_state(&sc->sc_led_control, HAL_LED_RUN);

    /* update ratectrl about the new state */
    ath_rate_newstate(sc, avp, 1);


    /*
     * set the bssid and aid values in the following 2 cases.
     * 1) no active stattion vaps and this is the first vap coming up.
     * 2) IBSS vap is coming up. 
     * 3) there is only one station vap and it is rejoining .
     * the case where a second STA vap is coming, do not touch the
     * bssid,aid registers. leave them to the existing  active STA vap. 
     */
    if (atomic_read(&sc->sc_nsta_vaps_up) == 0 ||  avp->av_opmode == HAL_M_IBSS  ||
         ( atomic_read(&sc->sc_nsta_vaps_up) == 1 &&  avp->av_opmode ==  HAL_M_STA && 
           avp->av_up ) ) {
        ATH_ADDR_COPY(sc->sc_curbssid, bssid);
        sc->sc_curaid = aid;
        ath_hal_setassocid(ah, sc->sc_curbssid, sc->sc_curaid);
    }

    /* 
     * XXX : For AoW feature, we are forcing the TSF sync between the
     *       WDS client and WDS RootAP.
     *       The Transmitter and Receiver maintain their audio sync
     *       based on the TSF values, hence they all the nodes in the
     *       WDS should follow the RootAP's TSF value
     */
     if (ATH_ENAB_AOW(sc)) {
        DPRINTF(sc, ATH_DEBUG_ANY, "AoW Demo, Forcing the TSF sync to the RootAP %s\n", ether_sprintf(bssid));
        ATH_ADDR_COPY(sc->sc_curbssid, bssid);
        sc->sc_curaid = aid;
        ath_hal_resettsf(ah);
        ath_hal_forcetsf_sync(ah, sc->sc_curbssid, sc->sc_curaid);
     }


    DPRINTF(sc, ATH_DEBUG_STATE, "%s: RX filter 0x%x bssid %02x:%02x:%02x:%02x:%02x:%02x aid 0x%x\n",
            __func__, rfilt,
            sc->sc_curbssid[0], sc->sc_curbssid[1], sc->sc_curbssid[2],
            sc->sc_curbssid[3], sc->sc_curbssid[4], sc->sc_curbssid[5],
            sc->sc_curaid);

    if ((avp->av_opmode != HAL_M_STA) && (flags & ATH_IF_PRIVACY)) {
        for (i = 0; i < IEEE80211_WEP_NKID; i++)
            if (ath_hal_keyisvalid(ah, (u_int16_t)i))
                ath_hal_keysetmac(ah, (u_int16_t)i, bssid);
    }

    switch (avp->av_opmode) {
    case HAL_M_HOSTAP:
    case HAL_M_IBSS:
        /*
         * Allocate and setup the beacon frame.
         *
         * Stop any previous beacon DMA.  This may be
         * necessary, for example, when an ibss merge
         * causes reconfiguration; there will be a state
         * transition from RUN->RUN that means we may
         * be called with beacon transmission active.
         */
        ath_hal_stoptxdma(ah, sc->sc_bhalq, 0);

        error = ath_beacon_alloc(sc, if_id);
        if (error != 0)
            goto bad;

#ifdef ATH_BT_COEX
        if (sc->sc_hasbtcoex) {
            ath_bt_coex_event(sc, ATH_COEX_EVENT_WLAN_CONNECT, NULL);
        }
#endif

		if(avp->av_opmode == HAL_M_IBSS)
		{
			/*For HAL_M_IBSS mode, ath_vap_down() (/iawDown()) will be called
			in mlmeProcessJointFrame(), and interrupt will be disable. So re-enable
			the interrupt here */

			ath_hal_intrset(ah, sc->sc_imask | HAL_INT_GLOBAL);
		}
        break;

    case HAL_M_STA:
        /*
         * Record negotiated dynamic turbo state for
         * use by rate control modules.
         */
        sc->sc_dturbo = (flags & ATH_IF_DTURBO) ? 1 : 0;

        /*
         * start rx chain mask detection if it is enabled.
         * Use the default chainmask as starting point.
         */
        if (sc->sc_rx_chainmask_detect) {
            if (flags & ATH_IF_HT) {
                sc->sc_rx_chainmask = sc->sc_config.rxchainmask;
            } else {
                sc->sc_rx_chainmask = sc->sc_config.rxchainmasklegacy; 
            }
            sc->sc_rx_chainmask_start = 1;
            sc->sc_rx_numchains = sc->sc_mask2chains[sc->sc_rx_chainmask];
        }

#if ATH_SLOW_ANT_DIV
        /* Start slow antenna diversity */
        if (sc->sc_slowAntDiv)
        {
            u_int32_t num_antcfg;

            if (sc->sc_curchan.channelFlags & CHANNEL_5GHZ)
                ath_hal_getcapability(ah, HAL_CAP_ANT_CFG_5GHZ, 0, &num_antcfg);
            else
                ath_hal_getcapability(ah, HAL_CAP_ANT_CFG_2GHZ, 0, &num_antcfg);
     
            if (num_antcfg > 1)
                ath_slow_ant_div_start(&sc->sc_antdiv, num_antcfg, bssid);
        }
#endif
#ifdef ATH_BT_COEX
        if ((sc->sc_hasbtcoex) && (sc->sc_btinfo.bt_coexAgent == 0)) {
            ath_bt_coex_event(sc, ATH_COEX_EVENT_WLAN_CONNECT, NULL);
        }
#endif

        if (ath_timer_is_initialized(&sc->sc_up_beacon) && !ath_timer_is_active(&sc->sc_up_beacon)) {
            ath_start_timer(&sc->sc_up_beacon); 
            sc->sc_up_beacon_purge = 1;
        }

        break;

    default:
        break;
    }

#ifdef ATH_SUPPORT_DFS
    /*
     * if it is a DFS channel and has not been checked for radar 
     * do not let the 80211 state machine to go to RUN state.
     */
    if (sc->sc_dfs && sc->sc_dfs->sc_dfswait 
            && avp->av_opmode == HAL_M_HOSTAP) {
        /* push the vap to RUN state once DFS is cleared */
        DPRINTF(sc, ATH_DEBUG_STATE, "%s: avp  -> DFS_WAIT\n", __func__);
        avp->av_dfswait_run = 1; 
        error = EAGAIN;
        goto bad;
    }
#endif

    if (avp->av_opmode == HAL_M_STA) {
        /* use atomic variable */
        atomic_inc(&sc->sc_nsta_vaps_up);
    } else if (avp->av_opmode == HAL_M_HOSTAP) {
        atomic_inc(&sc->sc_nap_vaps_up);
    }

    rfilt = ath_calcrxfilter(sc);
    ath_hal_setrxfilter(ah, rfilt);

    /* Moved beacon_config after dfs_wait check
     * so that ath_beacon_config won't be called duing dfswait
     * period - this will fix the beacon stuck afer DFS 
     * CAC period issue
     */
    /*
     * Configure the beacon and sleep timers.
     * multiple AP VAPs case we  need to reconfigure beacon for 
     * vaps differently for each vap and chip mode combinations.
     * AP + STA vap with chip mode STA.
     */
     ath_beacon_config(sc, ATH_BEACON_CONFIG_REASON_VAP_UP,if_id);
     avp->av_up=1;
    /*
     * Reset rssi stats; maybe not the best place...
     */
    if (flags & ATH_IF_HW_ON) {
        sc->sc_halstats.ns_avgbrssi = ATH_RSSI_DUMMY_MARKER;
        sc->sc_halstats.ns_avgrssi = ATH_RSSI_DUMMY_MARKER;
        sc->sc_halstats.ns_avgtxrssi = ATH_RSSI_DUMMY_MARKER;
        sc->sc_halstats.ns_avgtxrate = ATH_RATE_DUMMY_MARKER;
    }

    /* start periodic recalibration timer */
    sc->sc_longcal_timer  = timestamp;
    sc->sc_shortcal_timer = timestamp;
    sc->sc_ani_timer      = timestamp;
	for (i=0; i<sc->sc_num_cals; i++) {
		HAL_CALIBRATION_TIMER *cal_timerp = &sc->sc_cals[i];
		cal_timerp->cal_timer_ts = timestamp;
		sc->sc_sched_cals |= cal_timerp->cal_timer_group;
	}
    if (!ath_timer_is_active(&sc->sc_cal_ch)) {
        u_int32_t ani_interval;
        if (!ath_hal_enabledANI(ah) || 
            !ath_hal_getanipollinterval(ah, &ani_interval)) {
            ani_interval = 0;
        }

        ath_set_timer_period(&sc->sc_cal_ch,  
                             ath_get_cal_interval(sc->sc_Caldone, ani_interval, 
                                                  ath_hal_isOpenLoopPwrCntrl(ah), sc->sc_min_cal));
        ath_start_timer(&sc->sc_cal_ch); 
    }

#ifdef ATH_SUPERG_XR
    if (ieee80211vap_has_flag(vap, IEEE80211_F_XR) && nstate == IEEE80211_S_RUN ) {
        ATH_SETUP_XR_VAP(sc,vap,rfilt);
    }
#endif

#if ATH_TRAFFIC_FAST_RECOVER
    if (ath_timer_is_initialized(&sc->sc_traffic_fast_recover_timer) && 
        !ath_timer_is_active(&sc->sc_traffic_fast_recover_timer)) {
        ath_start_timer(&sc->sc_traffic_fast_recover_timer);
    }
#endif
#if ATH_SUPPORT_FLOWMAC_MODULE
    if (sc->sc_osnetif_flowcntrl) {
        /* do not pause the ingress here */
        ath_netif_wake_queue(sc);
    }
#endif
bad:
    ATH_PS_SLEEP(sc);
    return error;
}

/*
************************************
*  PnP routines
************************************
*/

int
ath_stop_locked(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
#ifdef ATH_RB
    ath_rb_t *rb = &sc->sc_rb;
#endif
    u_int32_t    imask = 0;

    DPRINTF(sc, ATH_DEBUG_RESET, "%s: invalid %u\n",
            __func__, sc->sc_invalid);

// RNWF    if (dev->flags & IFF_RUNNING) {
    /*
     * Shutdown the hardware and driver:
     *    stop output from above
     *    reset 802.11 state machine
     *    (sends station deassoc/deauth frames)
     *    turn off timers
     *    disable interrupts
     *    clear transmit machinery
     *    clear receive machinery
     *    turn off the radio
     *    reclaim beacon resources
     *
     * Note that some of this work is not possible if the
     * hardware is gone (invalid).
     */

    /* Stop ForcePPM module */
    ath_force_ppm_halt(&sc->sc_ppm_info);

#ifdef ATH_RB
    /* Cancel RB timer */
    ath_cancel_timer(&rb->restore_timer, CANCEL_NO_SLEEP);
#endif

// RNWF upper layers will know to stop themselves
//        netif_stop_queue(dev);    /* XXX re-enabled by ath_newstate */
//        dev->flags &= ~IFF_RUNNING;    /* NB: avoid recursion */
    ATH_PS_WAKEUP(sc);

    // Stop LED module.
    // ath_stop_locked can be called multiple times, so we wait until we are
    // actually dettaching from the device (sc->sc_invalid = TRUE) before 
    // stopping the LED module.
    if (sc->sc_invalid) {
        ath_led_halt_control(&sc->sc_led_control);
    }

    if (!sc->sc_invalid) {
        if (ah != NULL) {
            if (ah->ah_setInterrupts != NULL)
                ath_hal_intrset(ah, 0);
            else
                printk("%s[%d] : ah_setInterrupts was NULL\n", __func__, __LINE__);

        }
        else {
            printk("%s[%d] : ah was NULL\n", __func__, __LINE__);
        }
    }

    if (!sc->sc_invalid && ah != NULL) {
        imask = ath_hal_intrget(ah);
        if (imask & HAL_INT_GLOBAL)
            ath_hal_intrset(ah, 0);
    }
    ath_draintxq(sc, AH_FALSE, 0);
    if (!sc->sc_invalid && ah != NULL) {
        if (imask & HAL_INT_GLOBAL)
            ath_hal_intrset(ah, imask);
    }

    if (!sc->sc_invalid) {
        ATH_STOPRECV(sc, 0);
        if (ah != NULL) {
            ath_hal_phydisable(ah);
        }
    } else
        sc->sc_rxlink = NULL;

 //    }
    ATH_PS_SLEEP(sc);
    return 0;
}
#ifndef ATH_SUPPORT_HTC
static int
ath_open(ath_dev_t dev, HAL_CHANNEL *initial_chan)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_STATUS status;
    int error = 0;
    HAL_HT_MACMODE ht_macmode = sc->sc_ieee_ops->cwm_macmode(sc->sc_ieee);
    u_int32_t imask;

    //ATH_LOCK(sc);

    DPRINTF(sc, ATH_DEBUG_RESET, "%s: mode %d\n", __func__, sc->sc_opmode);

    ath_pwrsave_init(sc);

    /*
     * Stop anything previously setup.  This is safe
     * whether this is the first time through or not.
     */
    ath_stop_locked(sc);

#if ATH_CAP_TPC
    ath_hal_setcapability(ah, HAL_CAP_TPC, 0, 1, NULL);
#endif

    /* Initialize chanmask selection */
    sc->sc_tx_chainmask = sc->sc_config.txchainmask;
    sc->sc_rx_chainmask = sc->sc_config.rxchainmask;
    sc->sc_rx_numchains = sc->sc_mask2chains[sc->sc_rx_chainmask];
    sc->sc_tx_numchains = sc->sc_mask2chains[sc->sc_tx_chainmask];

    /* Start ForcePPM module */
    ath_force_ppm_start(&sc->sc_ppm_info);

    /* Reset SERDES registers */
    ath_hal_configpcipowersave(ah, 0, 0);

    /*
     * The basic interface to setting the hardware in a good
     * state is ``reset''.  On return the hardware is known to
     * be powered up and with interrupts disabled.  This must
     * be followed by initialization of the appropriate bits
     * and then setup of the interrupt mask.
     */
    sc->sc_curchan = *initial_chan;
   /* EV96307: Reset PAPRD state vars during i/f up */
    sc->sc_curchan.paprdDone = 0;
    sc->sc_curchan.paprdTableWriteDone = 0;
    sc->sc_paprd_cal_started = 0;
    sc->sc_paprd_lastchan_num = 0;
    sc->sc_paprd_chain_num = 0;

    imask = ath_hal_intrget(ah);
    if (imask & HAL_INT_GLOBAL)
        ath_hal_intrset(ah, 0);

#ifdef ATH_C3_WAR
    STOP_C3_WAR_TIMER(sc);
#endif

#if ATH_RESET_SERIAL
    ATH_RESET_ACQUIRE_MUTEX(sc);
    ATH_RESET_LOCK(sc);
    atomic_inc(&sc->sc_hold_reset);
    ath_reset_wait_tx_rx_finished(sc);
    ATH_RESET_UNLOCK(sc);
#else
    ATH_LOCK_PCI_IRQ(sc);
#endif
    if (!ath_hal_reset(ah, sc->sc_opmode, &sc->sc_curchan, ht_macmode,
                       sc->sc_tx_chainmask, sc->sc_rx_chainmask,
                       sc->sc_ht_extprotspacing, AH_FALSE, &status,
                       sc->sc_scanning))
    {
        printk("%s: unable to reset hardware; hal status %u "
               "(freq %u flags 0x%x)\n", __func__, status,
               sc->sc_curchan.channel, sc->sc_curchan.channelFlags);
        error = -EIO;
#if ATH_RESET_SERIAL
        atomic_dec(&sc->sc_hold_reset);
        ATH_RESET_RELEASE_MUTEX(sc);
#else
        ATH_UNLOCK_PCI_IRQ(sc);
#endif

        if (imask & HAL_INT_GLOBAL)
            ath_hal_intrset(ah, imask);

        goto done;
    }
#if ATH_RESET_SERIAL
    atomic_dec(&sc->sc_hold_reset);
    ATH_RESET_RELEASE_MUTEX(sc);
#else
    ATH_UNLOCK_PCI_IRQ(sc);
#endif

    if (imask & HAL_INT_GLOBAL)
        ath_hal_intrset(ah, imask);

    /*
     * This is needed only to setup initial state
     * but it's best done after a reset.
     */
    ath_update_txpow(sc, sc->tx_power);

    /*
     * Setup the hardware after reset: the key cache
     * is filled as needed and the receive engine is
     * set going.  Frame transmit is handled entirely
     * in the frame output path; there's nothing to do
     * here except setup the interrupt mask.
     */

    if (ATH_STARTRECV(sc) != 0) {
        printk("%s: unable to start recv logic\n", __func__);
        error = -EIO;
        goto done;
    }

    /*
     *  Setup our intr mask.
     */
    sc->sc_imask = HAL_INT_TX
        | HAL_INT_RXEOL | HAL_INT_RXORN
        | HAL_INT_FATAL | HAL_INT_GLOBAL;

    if (sc->sc_enhanceddmasupport) {
        sc->sc_imask |= HAL_INT_RXHP | HAL_INT_RXLP;
    } else {
        sc->sc_imask |= HAL_INT_RX;
    }

    if (ath_hal_gttsupported(ah))
        sc->sc_imask |= HAL_INT_GTT;

    if (sc->sc_hashtsupport)
        sc->sc_imask |= HAL_INT_CST;

    /*
     * Enable MIB interrupts when there are hardware phy counters.
     * Note we only do this (at the moment) for station mode.
     */
    if ((sc->sc_opmode == HAL_M_STA) ||
        (sc->sc_opmode == HAL_M_IBSS)) {

#ifdef ATH_MIB_INTR_FILTER  
             /*  
             * EV 63034. We should not be enabling MIB interrupts unless  
             * we have all the counter-reading code enabled so it can 0 out  
             * counters and clear the interrupt condition (saturated counters).   
             */  
        if (sc->sc_needmib) {
            sc->sc_imask |= HAL_INT_MIB;
        }        
#endif // ATH_MIB_INTR_FILTER

        /*
         * Enable TSFOOR in STA and IBSS modes.
         */
        sc->sc_imask |= HAL_INT_TSFOOR;
    }

    /*
     * Some hardware processes the TIM IE and fires an
     * interrupt when the TIM bit is set.  For hardware
     * that does, if not overridden by configuration,
     * enable the TIM interrupt when operating as station.
     */
    if (ath_hal_hasenhancedpmsupport(ah) &&
        sc->sc_opmode == HAL_M_STA &&
        !sc->sc_config.swBeaconProcess) {
        sc->sc_imask |= HAL_INT_TIM;
    }

#ifdef ATH_RFKILL
    if (sc->sc_hasrfkill) {
        if (ath_rfkill_hasint(sc))
            sc->sc_imask |= HAL_INT_GPIO;

        /*
         * WAR for Bug 33276: In certain systems, the RfKill signal is slow to
         * stabilize when waking up from power suspend. The RfKill is an
         * active-low GPIO signal and the problem is the slow rise from 0V to VCC.
         * For this workaround, we will delayed implementing the new RfKill
         * state if there is a change in RfKill during the sleep. This WAR is only
         * when the previous RfKill state is OFF and the new awaken state is ON.
         */
        if (sc->sc_reg_parm.rfKillDelayDetect) {
            ath_rfkill_delay_detect(sc);
        }
    }
#endif
    
    if (sc->sc_wpsgpiointr)
        sc->sc_imask |= HAL_INT_GPIO;

#ifdef ATH_BT_COEX
    if (sc->sc_btinfo.bt_gpioIntEnabled) {
        sc->sc_imask |= HAL_INT_GPIO;
    }
#endif

    /*
     * At least while Osprey is in alpha,
     * default BB panic watchdog to be on to aid debugging
     */
    if (sc->sc_bbpanicsupport) {
        sc->sc_imask |= HAL_INT_BBPANIC;
    }

    /*
     *  Don't enable interrupts here as we've not yet built our
     *  vap and node data structures, which will be needed as soon
     *  as we start receiving.
     */
  
    ath_chan_change(sc, initial_chan);

    if (!sc->sc_reg_parm.pcieDisableAspmOnRfWake) {
        ath_pcie_pwrsave_enable(sc, 1);
    } else {
        ath_pcie_pwrsave_enable(sc, 0);
    }

    /* XXX: we must make sure h/w is ready and clear invalid flag
     * before turning on interrupt. */
    sc->sc_invalid = 0;

    /*
     * It has been seen that after a config change, when the channel
     * is set, the fast path is taken in the chipset reset code. The
     * net result is that the interrupt ref count is left hanging in
     * a non-zero state, causing interrupt to NOT get enabled. The fix 
     * here is that on ath_open, we set the sc_full_reset flag to 1
     * preventing the fast path from being taken on an initial config.
    */
    sc->sc_full_reset = 1;
    
    /* Start LED module; pass radio state as parameter */
    ath_led_start_control(&sc->sc_led_control, 
                          sc->sc_hw_phystate && sc->sc_sw_phystate);

done:
    //ATH_UNLOCK(sc);
    return error;
}
#endif

/*
 * Stop the device, grabbing the top-level lock to protect
 * against concurrent entry through ath_init (which can happen
 * if another thread does a system call and the thread doing the
 * stop is preempted).
 */
static int
ath_stop(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int error;

    //ATH_LOCK(sc);

    if (!sc->sc_invalid) {
        ath_pwrsave_awake(sc);

        ath_led_disable(&sc->sc_led_control);
    }

    error = ath_stop_locked(sc);
#if 0
    if (error == 0 && !sc->sc_invalid) {
        /*
         * Set the chip in full sleep mode.  Note that we are
         * careful to do this only when bringing the interface
         * completely to a stop.  When the chip is in this state
         * it must be carefully woken up or references to
         * registers in the PCI clock domain may freeze the bus
         * (and system).  This varies by chip and is mostly an
         * issue with newer parts that go to sleep more quickly.
         */
        ath_hal_setpower(sc->sc_ah, HAL_PM_FULL_SLEEP);
    }
#endif
    //ATH_UNLOCK(sc);
#ifdef MAGPIE_HIF_GMAC
    OS_CANCEL_TIMER(&host_seek_credit_timer);
#endif    

#if ATH_TRAFFIC_FAST_RECOVER
    if (ath_timer_is_initialized(&sc->sc_traffic_fast_recover_timer)) {
        ath_cancel_timer(&sc->sc_traffic_fast_recover_timer, CANCEL_NO_SLEEP);
    }
#endif
    return error;
}

static void
ath_reset_handle_fifo_intsafe(ath_dev_t dev)
{
    ath_rx_intr(dev, HAL_RX_QUEUE_HP);
    ath_rx_intr(dev, HAL_RX_QUEUE_LP);
}

/*
 * spin_lock is to exclude access from other thread(passive or dpc level).
 * intsafe is to exclude interrupt access. ath_rx_intr() normally is called
 * in ISR context.
 * note: spin_lock may hold a long time.
 */
static void
ath_reset_handle_fifo(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ATH_RXBUF_LOCK(sc);
    OS_EXEC_INTSAFE(sc->sc_osdev, ath_reset_handle_fifo_intsafe, dev);
    ATH_RXBUF_UNLOCK(sc);
}

#if ATH_RESET_SERIAL
/*
 * Reset routine waits until both ath_tx_txqaddbuf() and 
 * ath_rx_buf_link() finish their processing.
 */
int
ath_reset_wait_tx_rx_finished(struct ath_softc *sc)
{
    while(atomic_read(&sc->sc_tx_add_processing)) { //waiting
        DPRINTF(sc, ATH_DEBUG_STATE, "Waiting for tx_txqaddbuf:%s, %d\n",
            __func__, atomic_read(&sc->sc_tx_add_processing));
    }

    while(atomic_read(&sc->sc_rx_return_processing)) {
        DPRINTF(sc, ATH_DEBUG_STATE, "Waiting for rx_return:%s, %d\n",
            __func__, atomic_read(&sc->sc_rx_return_processing)); 
    }
    return 0;
}

int
ath_reset_wait_intr_dpc_finished(struct ath_softc *sc)
{
    int wait_count = 0;

    while(atomic_read(&sc->sc_intr_processing)){
        OS_DELAY(100);
        if (wait_count++ >= 100) {
            DPRINTF(sc, ATH_DEBUG_STATE, "Waiting for intr return timeout:%s, %d\n",
                    __func__, atomic_read(&sc->sc_intr_processing));
       }
    }

    wait_count = 0;
    while(atomic_read(&sc->sc_dpc_processing)){
        OS_DELAY(100);
        if (wait_count++ >= 100) {
            DPRINTF(sc, ATH_DEBUG_STATE, "Waiting for dpc return timeout:%s, %d\n",
                    __func__, atomic_read(&sc->sc_dpc_processing));
        }
    }
   return 0;
}
#endif

/*
 * Reset the hardware w/o losing operational state.  This is
 * basically a more efficient way of doing ath_stop, ath_init,
 * followed by state transitions to the current 802.11
 * operational state.  Used to recover from errors rx overrun
 * and to reset the hardware when rf gain settings must be reset.
 */
int
ath_reset_start(ath_dev_t dev, HAL_BOOL no_flush, int tx_timeout, int rx_timeout)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    ATH_PS_WAKEUP(sc);

#ifdef ATH_BT_COEX
    if (sc->sc_hasbtcoex)
        ath_bt_coex_event(sc, ATH_COEX_EVENT_WLAN_RESET_START, NULL);
#endif

#if ATH_RESET_SERIAL
    ATH_RESET_ACQUIRE_MUTEX(sc);
    ATH_RESET_LOCK(sc);
    atomic_inc(&sc->sc_dpc_stop);    
    printk("%s %d %d\n", __func__, 
__LINE__, sc->sc_dpc_stop);
    ath_hal_intrset(ah, 0);  
    ATH_RESET_UNLOCK(sc);
#else
    /* Set the flag so that other functions will not try to restart tx/rx DMA. */
    atomic_inc(&sc->sc_in_reset);
    ath_hal_intrset(ah, 0);                     /* disable interrupts */
#endif


#if ATH_RESET_SERIAL
    OS_DELAY(100);
    ATH_RESET_LOCK(sc);
    atomic_inc(&sc->sc_hold_reset);
    atomic_set(&sc->sc_reset_queue_flag, 1);
    ath_reset_wait_tx_rx_finished(sc);
    ATH_RESET_UNLOCK(sc);
    ath_reset_wait_intr_dpc_finished(sc);
    atomic_dec(&sc->sc_dpc_stop);
    printk("%s %d %d\n", __func__, 
__LINE__, sc->sc_dpc_stop);    
    printk("Begin reset sequence......\n");
#endif

    ath_reset_draintxq(sc, no_flush, tx_timeout); /* stop xmit side */
    ath_wmi_drain_txq(sc);                      /* Stop target firmware tx */


    /* In case of STA reset (because of BB panic), few subframes in rx FiFo are 
       getting dropped because of ATH_STOPRECV() call before ATH_RX_TASKLET() */
    if(no_flush && sc->sc_enhanceddmasupport)
    {
        ath_reset_handle_fifo(dev);
    }

    ATH_STOPRECV(sc, rx_timeout);                        /* stop recv side */

    /*
     * We cannot indicate received packets while holding a lock.
     * use cmpxchg here.
     */
    if (cmpxchg(&sc->sc_rxflush, 0, 1) == 0) {
        if (no_flush) {
            ath_reset(dev);
            ATH_RX_TASKLET(sc, RX_FORCE_PROCESS);
        } else {
            ATH_RX_TASKLET(sc, RX_DROP);
        }

        if (cmpxchg(&sc->sc_rxflush, 1, 0) != 1)
            DPRINTF(sc, ATH_DEBUG_RX_PROC, "%s: flush is not protected.\n", __func__);
    }

    /* Suspend ForcePPM when entering a reset */
    ath_force_ppm_notify(&sc->sc_ppm_info, ATH_FORCE_PPM_SUSPEND, NULL);

    ATH_PS_SLEEP(sc);
    return 0;
}

int
ath_reset_end(ath_dev_t dev, HAL_BOOL no_flush)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
        
    ATH_PS_WAKEUP(sc);


 #if ATH_RESET_SERIAL
    /* to keep lock sequence as same as ath_tx_start_dma->ath_tx_txqaddbuf,
     * we have to lock all of txq firstly.
     */
    /* osprey seems no need to sync with reset */
    if (!sc->sc_enhanceddmasupport) {
        int i;
        for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
            ATH_TXQ_LOCK(&sc->sc_txq[i]);
        
        ATH_RESET_LOCK(sc);
        atomic_set(&sc->sc_reset_queue_flag, 0);
        atomic_dec(&sc->sc_hold_reset);
        if (atomic_read(&sc->sc_hold_reset))
            printk("Wow, still multiple reset in process,--investigate---\n");

   
    
         /* flush all the queued tx_buf */
         
        for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
            
            ath_bufhead *head, buf_head;
            struct ath_buf *buf, *last_buf;
            
           // ATH_TXQ_LOCK(&sc->sc_txq[i]);
            ATH_RESET_LOCK_Q(sc, i);
            
            head = &(sc->sc_queued_txbuf[i].txbuf_head);
            
            do {
                buf = TAILQ_FIRST(head);
                if (!buf)
                    break;
                last_buf = buf->bf_lastbf;
                TAILQ_INIT(&buf_head);
                TAILQ_REMOVE_HEAD_UNTIL(head, &buf_head, last_buf, bf_list);
                _ath_tx_txqaddbuf(sc, &sc->sc_txq[i], &buf_head); //can't call directly
            } while (!TAILQ_EMPTY(head));
            

            ATH_RESET_UNLOCK_Q(sc, i);
           // ATH_TXQ_UNLOCK(&sc->sc_txq[i]);

        }
    
        ATH_RESET_UNLOCK(sc);
        for (i = HAL_NUM_TX_QUEUES -1 ; i >= 0; i--)
             ATH_TXQ_UNLOCK(&sc->sc_txq[i]);
    } else {
        atomic_set(&sc->sc_reset_queue_flag, 0);
        atomic_dec(&sc->sc_hold_reset);
    }
#else
        
    
    /* Clear the flag so that we can start hardware. */
    atomic_dec(&sc->sc_in_reset);
#endif

    ath_wmi_start_recv(sc);

    if (ATH_STARTRECV(sc) != 0) /* restart recv */
        printk("%s: unable to start recv logic\n", __func__);

    /*
     * We may be doing a reset in response to an ioctl
     * that changes the channel so update any state that
     * might change as a result.
     */
    ath_chan_change(sc, &sc->sc_curchan);

    ath_update_txpow(sc, sc->tx_power);        /* update tx power state */

    ath_beacon_config(sc,ATH_BEACON_CONFIG_REASON_RESET, ATH_IF_ID_ANY);   /* restart beacons */
    sc->sc_reset_type = ATH_RESET_DEFAULT; /* reset sc_reset_type */

    ath_hal_intrset(ah, sc->sc_imask);

#ifdef ATH_SUPERG_XR
    /*
     * restart the group polls.
     */
    if (sc->sc_xrgrppoll) {
        struct ieee80211vap *vap;
        TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next)
            if(vap && (vap->iv_flags & IEEE80211_F_XR)) break;
        ath_grppoll_stop(vap);
        ath_grppoll_start(vap,sc->sc_xrpollcount);
    }
#endif

    /* Resume ForcePPM after reset is completed */
    ath_force_ppm_notify(&sc->sc_ppm_info, ATH_FORCE_PPM_RESUME, NULL);

    /* Restart the txq */
    if (no_flush) {
        int i;
        for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
            if (ATH_TXQ_SETUP(sc, i)) {
                ATH_TXQ_LOCK(&sc->sc_txq[i]);
                ath_txq_schedule(sc, &sc->sc_txq[i]);
                ATH_TXQ_UNLOCK(&sc->sc_txq[i]);
            }
        }
    }

#ifdef ATH_GEN_TIMER
        /* Inform the generic timer module that a reset just happened. */
        ath_gen_timer_post_reset(sc);
#endif  //ATH_GEN_TIMER
#ifdef ATH_BT_COEX
    if (sc->sc_hasbtcoex)
        ath_bt_coex_event(sc, ATH_COEX_EVENT_WLAN_RESET_END, NULL);
#endif
    ATH_PS_SLEEP(sc);

#if ATH_RESET_SERIAL
    /* release the lock */
    ATH_RESET_RELEASE_MUTEX(sc);
#endif
    
    return 0;
}

int
ath_reset(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_STATUS status;
    int error = 0;
    HAL_HT_MACMODE ht_macmode = sc->sc_ieee_ops->cwm_macmode(sc->sc_ieee);

    ATH_PS_WAKEUP(sc);

    ATH_USB_TX_STOP(sc->sc_osdev);
    ATH_HTC_TXRX_STOP(sc, AH_FALSE);
#ifdef ATH_C3_WAR
    STOP_C3_WAR_TIMER(sc);
#endif

    /* NB: indicate channel change so we do a full reset */
#if !ATH_RESET_SERIAL
    ATH_LOCK_PCI_IRQ(sc);
#endif
    if (!ath_hal_reset(ah, sc->sc_opmode, &sc->sc_curchan,
                       ht_macmode,
                       sc->sc_tx_chainmask, sc->sc_rx_chainmask,
                       sc->sc_ht_extprotspacing,
                       AH_FALSE, &status, sc->sc_scanning))
    {
        printk("%s: unable to reset hardware; hal status %u,\n"
               "sc_opmode=%d, sc_curchan: chan=%d, Flags=0x%x, sc_scanning=%d\n",
               __func__, status,
               sc->sc_opmode, sc->sc_curchan.channel, sc->sc_curchan.channelFlags,
               sc->sc_scanning
               );
        error = -EIO;
    }
    ATH_HTC_ABORTTXQ(sc);
#if !ATH_RESET_SERIAL
    ATH_UNLOCK_PCI_IRQ(sc);
#endif

    ATH_USB_TX_START(sc->sc_osdev);
    ATH_PS_SLEEP(sc);
    return error;
}

static int
ath_switch_opmode(ath_dev_t dev, HAL_OPMODE opmode)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    if (sc->sc_opmode != opmode) {
        DPRINTF(sc, ATH_DEBUG_STATE, "%s: switch opmode from %d to %d\n", __func__,sc->sc_opmode, opmode);
        sc->sc_opmode = opmode;
        ath_beacon_config(sc,ATH_BEACON_CONFIG_REASON_OPMODE_SWITCH,ATH_IF_ID_ANY);
        ath_internal_reset(sc);
    }
    return 0;
}

static int
ath_suspend(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal   *ah = sc->sc_ah;

    /*
     * No I/O if device has been surprise removed
     */
    /*
    ** Ensure green AP is turned off when suspended
    */

    if(ath_green_ap_sc_get_enable(sc)) {
        ath_green_ap_stop(sc);
        ath_green_ap_suspend(sc); 
    }

    if (ath_get_sw_phystate(sc) && ath_get_hw_phystate(sc)) {
        /* stop the hardware */
        ath_stop(sc);

        if (sc->sc_invalid)
            return -EIO;
    }
    else {
        if (sc->sc_invalid)
            return -EIO;

        /*
         * Work around for bug #24058: OS hang during restart after software RF off.
         * Condore has problem waking up on some machines (no clue why) if ath_suspend is called
         * in which nothing is done when RF is off.
         */
        ath_pwrsave_awake(sc);

        /* Shut off the interrupt before setting sc->sc_invalid to '1' */
        ath_hal_intrset(ah, 0);
        
        /* Even though the radio is OFF, the LED might still be alive. Disable it. */
        ath_led_disable(&sc->sc_led_control);
    }
    
    /* XXX: we must make sure h/w will not generate any interrupt
     * before setting the invalid flag. */
    sc->sc_invalid = 1;

    /* Wait for any hardware access to complete */
    ath_wait_sc_inuse_cnt(sc, 1000);
    
    /* disable HAL and put h/w to sleep */
    ath_hal_disable(sc->sc_ah);

    ath_hal_configpcipowersave(sc->sc_ah, 1, 1);

    ath_pwrsave_fullsleep(sc);

    return 0;
}

static void
ath_enable_intr(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_hal_intrset(sc->sc_ah, sc->sc_imask);
}

static void
ath_disable_intr(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_hal_intrset(sc->sc_ah, 0);
}

#ifdef ATH_MIB_INTR_FILTER
/*
 * Filter excessive MIB interrupts so that the system doesn't freeze because
 * it cannot handle the lower-priority threads.
 *
 * Algorithm:
 *     -The beginning of a possible burst is characterized by two consecutive
 *      MIB interrupts arriving within MIB_FILTER_MAX_INTR_ELAPSED_TIME (20ms) 
 *      of each other.
 *
 *     -A MIB interrupt is considered part of a burst if it arrives within
 *      MIB_FILTER_MAX_INTR_ELAPSED_TIME (20ms) of the previous one AND within
 *      MIB_FILTER_MAX_BURST_ELAPSED_TIME (100ms) of the beginning of the burst.
 *
 *     -Once the number of consecutive MIB interrupts reaches
 *      MIB_FILTER_COUNT_THRESHOLD (500) we disable reception of MIB interrupts.
 *
 *     -Reception of a MIB interrupt that is longer part of the burst or 
 *      reception of a different interrupt cause the counting to start over.
 *
 *     -Once the MIB interrupts have been disabled, we wait for 
 *      MIB_FILTER_RECOVERY_TIME (50ms) and then reenable MIB interrupts upon   
 *      reception of the next non-MIB interrupt.
 *
 * Miscellaneous:
 *     -The algorithm is always enabled if ATH_MIB_INTR_FILTER is defined.
 */
static void ath_filter_mib_intr(struct ath_softc *sc, u_int8_t is_mib_intr)
{
    struct ath_intr_filter    *intr_filter  = &sc->sc_intr_filter;
    u_int32_t                 current_time  = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
    u_int32_t                 intr_interval = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(current_time - intr_filter->last_intr_time);
    u_int32_t                 burst_dur     = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(current_time - intr_filter->burst_start_time);

    switch (intr_filter->state) {
    case INTR_FILTER_OFF:
        /*
         * Two MIB interrupts arriving too close to each other may mark the
         * beggining of a burst of MIB interrupts.
         */
        if (is_mib_intr && (intr_interval <= MIB_FILTER_MAX_INTR_ELAPSED_TIME)) {
            intr_filter->state            = INTR_FILTER_DEGLITCHING;
            intr_filter->burst_start_time = current_time;
            intr_filter->intr_count++;
        }
        break;

    case INTR_FILTER_DEGLITCHING:
        /*
         * Consider a MIB interrupt arrived within a short time of the
         * previous one and withing a short time of the beginning of the burst
         * to be still part of the same burst.
         */
        if (is_mib_intr && 
            (intr_interval <= MIB_FILTER_MAX_INTR_ELAPSED_TIME) &&
            (burst_dur <= MIB_FILTER_MAX_BURST_ELAPSED_TIME)) {
            intr_filter->intr_count++;

            /*
             * Too many consecutive within a short time of each other 
             * characterize a MIB burst ("storm")
             */
            if (intr_filter->intr_count >= MIB_FILTER_COUNT_THRESHOLD) {
                /* MIB burst identified */
                intr_filter->activation_count++;
                intr_filter->state = INTR_FILTER_ON;

                /* Disable MIB interrupts */
                sc->sc_imask &= ~HAL_INT_MIB;

                DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR, "%d.%03d | %s: Start Mib Storm index=%3d intrCount=%8d start=%d.%03d duration=%6u ms\n",
                    current_time / 1000, current_time % 1000, __func__, 
                    intr_filter->activation_count,
                    intr_filter->intr_count,
                    ((u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(intr_filter->burst_start_time)) / 1000, 
                    ((u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(intr_filter->burst_start_time)) % 1000,
                    burst_dur);

                /* Want a register dump? */
                /* ath_printreg(sc, HAL_DIAG_PRINT_REG_COUNTER | HAL_DIAG_PRINT_REG_ALL); */
            }
        }
        else {
            /*
             * The gap from the previous interrupt is long enough to 
             * characterize the latest as not being part of a burst.
             * Go back to the initial state.
             */
            intr_filter->state      = INTR_FILTER_OFF;
            intr_filter->intr_count = 1;
        }
        break;

    case INTR_FILTER_ON:
        /* 
         * Wait for a non-MIB interrupt to be received more than a certain time
         * after the last MIB interrupt to consider it the end of the interrupt 
         * burst                                                      UE
         */
        if (! is_mib_intr) {
            if (intr_interval > MIB_FILTER_RECOVERY_TIME) {
                DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR, "%d.%03d | %s: Mib Storm index=%3d intrCount=%8d start=%d.%03d duration=%6u ms\n",
                    current_time / 1000, current_time % 1000, __func__, 
                    intr_filter->activation_count,
                    intr_filter->intr_count,
                    ((u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(intr_filter->burst_start_time)) / 1000, 
                    ((u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(intr_filter->burst_start_time)) % 1000,
                    burst_dur);

                /* Re-enable MIB interrupt */
                sc->sc_imask |= HAL_INT_MIB;

                /* Return to the initial state and monitor for new bursts */
                intr_filter->state      = INTR_FILTER_OFF;
                intr_filter->intr_count = 1;
            }
        }
        else {
            /* Another MIB interrupt generated was we were disabling them */
            intr_filter->intr_count++;
        }
        break;

    default:
        /* Bad state */
        ASSERT(0);
        break;
    }

    /* Do not save timestamp of non-MIB interrupts */
    if (is_mib_intr) {
        intr_filter->last_intr_time = current_time;
    }
}
#endif

#ifndef ATH_UPDATE_COMMON_INTR_STATS
#define ATH_UPDATE_COMMON_INTR_STATS(sc, status)
#endif
#ifndef ATH_UPDATE_INTR_STATS
#define ATH_UPDATE_INTR_STATS(sc, intr)
#endif

/*
 * Common Interrupt handler for MSI and Line interrutps.
 * Most of the actual processing is deferred.
 * It's the caller's responsibility to ensure the chip is awake.
 */
static inline int
ath_common_intr(ath_dev_t dev, HAL_INT status)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    int sched = ATH_ISR_NOSCHED;

    ATH_UPDATE_COMMON_INTR_STATS(sc, status);

    do {
#ifdef ATH_MIB_INTR_FILTER
        /* Notify the MIB interrupt filter that we received some other interrupt. */
        if (! (status & HAL_INT_MIB)) {
            ath_filter_mib_intr(sc, AH_FALSE);
        }
#endif

        if (status & HAL_INT_FATAL) {
            /* need a chip reset */
            sc->sc_stats.ast_hardware++;
            sched = ATH_ISR_SCHED;
        } else if ((status & HAL_INT_RXORN) && !sc->sc_enhanceddmasupport) {
            /* need a chip reset? */
#if ATH_SUPPORT_DESCFAST
            ath_rx_proc_descfast(dev);
#endif
            sc->sc_stats.ast_rxorn++;
            sched = ATH_ISR_SCHED;
        } else {
            if (status & HAL_INT_SWBA) {
#ifdef ATH_BEACON_DEFERRED_PROC
                /* Handle beacon transmission in deferred interrupt processing */
                sched = ATH_ISR_SCHED;
#else
                int needmark = 0;

                /*
                 * Software beacon alert--time to send a beacon.
                 * Handle beacon transmission directly; deferring
                 * this is too slow to meet timing constraints
                 * under load.
                 */
                ath_beacon_tasklet(sc, &needmark);

                if (needmark) {
                    /* We have a beacon stuck. Beacon stuck processing
                     * should be done in DPC instead of here. */
                    sched = ATH_ISR_SCHED;
                }
#endif /* ATH_BEACON_DEFERRED_PROC */
                ATH_UPDATE_INTR_STATS(sc, swba);
            }
            if (status & HAL_INT_TXURN) {
                sc->sc_stats.ast_txurn++;
                /* bump tx trigger level */
                ath_hal_updatetxtriglevel(ah, AH_TRUE);
            }
            if (sc->sc_enhanceddmasupport) {
#if ATH_OSPREY_RXDEFERRED
                if (status & HAL_INT_RXEOL) {
                    /* TODO - check rx fifo threshold here */
                    if (sc->sc_rxedma[HAL_RX_QUEUE_HP].rxfifodepth == 0 || 
                        sc->sc_rxedma[HAL_RX_QUEUE_LP].rxfifodepth == 0) 
                    {
                        //No buffers available - disable RXEOL/RXORN to avoid interrupt storm 
                        // Disable and then enable to satisfy global isr enable reference counter 
                        ; //For further investigation
                    }
                    sc->sc_imask &= ~(HAL_INT_RXEOL | HAL_INT_RXORN);
                    ath_hal_intrset(ah, sc->sc_imask);
                    sched = ATH_ISR_SCHED;
                    sc->sc_stats.ast_rxeol++;
                }
                if (status & (HAL_INT_RXHP | HAL_INT_RXLP | HAL_INT_RXORN)) {
                    sched = ATH_ISR_SCHED;
                }
                if (status & HAL_INT_RXORN) {
                    sc->sc_stats.ast_rxorn++;
                }
                if (status & HAL_INT_RX) {
                    ATH_UPDATE_INTR_STATS(sc, rx);
                }
#else // ATH_OSPREY_RXDEFERRED
                ath_rx_edma_intr(sc, status, &sched);                 
#endif // ATH_OSPREY_RXDEFERRED               
            } 
            else {         
                if ((status & HAL_INT_RXEOL)) {
                    /*
                     * NB: the hardware should re-read the link when
                     *     RXE bit is written, but it doesn't work at
                     *     least on older hardware revs.
                     */
#if ATH_SUPPORT_DESCFAST
                    ath_rx_proc_descfast(dev);
#endif
                    sc->sc_imask &= ~(HAL_INT_RXEOL | HAL_INT_RXORN);
                    ath_hal_intrset(ah, sc->sc_imask);
                    sc->sc_stats.ast_rxeol++;
                    sched = ATH_ISR_SCHED;
                }
                if (status & HAL_INT_RX) {
                    ATH_UPDATE_INTR_STATS(sc, rx);
#if ATH_SUPPORT_DESCFAST
                    ath_rx_proc_descfast(dev);
#endif
                    sched = ATH_ISR_SCHED;
                }
            }

            if (status & HAL_INT_TX) {
                ATH_UPDATE_INTR_STATS(sc, tx);
#ifdef ATH_SUPERG_DYNTURBO
                /*
                 * Check if the beacon queue caused the interrupt
                 * when a dynamic turbo switch
                 * is pending so we can initiate the change.
                 * XXX must wait for all vap's beacons
                 */

                if (sc->sc_opmode == HAL_M_HOSTAP && sc->sc_dturbo_switch) {
                    u_int32_t txqs= (1 << sc->sc_bhalq);
                    ath_hal_gettxintrtxqs(ah,&txqs);
                    if(txqs & (1 << sc->sc_bhalq)) {
                        sc->sc_dturbo_switch = 0;
                        /*
                         * Hack: defer switch for 10ms to permit slow
                         * clients time to track us.  This especially
                         * noticeable with Windows clients.
                         */
#ifdef notyet
                        mod_timer(&sc->sc_dturbo_switch_mode,
                                  jiffies + ((HZ * 10) / 1000));
#endif

                    }
                }
#endif
                sched = ATH_ISR_SCHED;
            }
            if (status & HAL_INT_BMISS) {
                sc->sc_stats.ast_bmiss++;
#if ATH_WOW
                if (sc->sc_wow_sleep) {
                    /* 
                     * The system is in WOW sleep and we used the BMISS intr to
                     * wake the system up. Note this BMISS by setting the 
                     * sc_wow_bmiss_intr flag but do not process this interrupt.
                     */
                    DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR,"%s: During Wow Sleep and got BMISS\n", __func__);
                    sc->sc_wow_bmiss_intr = 1;
                    sc->sc_wow_sleep = 0;
                    sched = ATH_ISR_NOSCHED;
                } else
#endif
                sched = ATH_ISR_SCHED;
            }
            if (status & HAL_INT_GTT) { /* tx timeout interrupt */
                sc->sc_stats.ast_txto++;
                sched = ATH_ISR_SCHED;
            }
            if (status & HAL_INT_CST) { /* carrier sense timeout */
                sc->sc_stats.ast_cst++;
                sched = ATH_ISR_SCHED;
            }

            if (status & HAL_INT_MIB) {
                sc->sc_stats.ast_mib++;
                /*
                 * Disable interrupts until we service the MIB
                 * interrupt; otherwise it will continue to fire.
                 */
                ath_hal_intrset(ah, 0);

#ifdef ATH_MIB_INTR_FILTER
                /* Check for bursts of MIB interrupts */
                ath_filter_mib_intr(sc, AH_TRUE);
#endif

                /*
                 * Let the hal handle the event.  We assume it will
                 * clear whatever condition caused the interrupt.
                 */
                ath_hal_mibevent(ah, &sc->sc_halstats);
                ath_hal_intrset(ah, sc->sc_imask);
            }
            if (status & HAL_INT_GPIO) {
                ATH_UPDATE_INTR_STATS(sc, gpio);
                /* Check if this GPIO interrupt is caused by RfKill */
#ifdef ATH_RFKILL
                if (ath_rfkill_gpio_isr(sc))
                    sched = ATH_ISR_SCHED;
#endif
                if (sc->sc_wpsgpiointr) {
                    /* Check for WPS push button press (GPIO polarity low) */
                    if (ath_hal_gpioget(sc->sc_ah, sc->sc_reg_parm.wpsButtonGpio) == 0) {
                        sc->sc_wpsbuttonpushed = 1;

                        /* Disable associated GPIO interrupt to prevent flooding */
                        ath_hal_gpioSetIntr(ah, sc->sc_reg_parm.wpsButtonGpio, HAL_GPIO_INTR_DISABLE);
                        sc->sc_wpsgpiointr = 0;
                    }
                }
#ifdef ATH_BT_COEX
                if (sc->sc_btinfo.bt_gpioIntEnabled) {
                    sched = ATH_ISR_SCHED;
                }
#endif
            }
            if (status & HAL_INT_TIM_TIMER) {
                ATH_UPDATE_INTR_STATS(sc, tim_timer);
                if (! sc->sc_hasautosleep) {
                    /* Clear RxAbort bit so that we can receive frames */
                    ath_hal_setrxabort(ah, 0);
                    /* Set flag indicating we're waiting for a beacon */
                    sc->sc_waitbeacon = 1;

                    sched = ATH_ISR_SCHED;
                }
            }
#ifdef ATH_GEN_TIMER
            if (status & HAL_INT_GENTIMER) {
                ATH_UPDATE_INTR_STATS(sc, gentimer);
                /* generic TSF timer interrupt */
                sched = ATH_ISR_SCHED;
            }
#endif

            if (status & HAL_INT_TSFOOR) {
                ATH_UPDATE_INTR_STATS(sc, tsfoor);
                DPRINTF(sc, ATH_DEBUG_PWR_SAVE,
                        "%s: HAL_INT_TSFOOR - syncing beacon\n",
                        __func__);
                /* Set flag indicating we're waiting for a beacon */
                sc->sc_waitbeacon = 1;

                sched = ATH_ISR_SCHED;
            }

            if (status & HAL_INT_BBPANIC) {
                ATH_UPDATE_INTR_STATS(sc, bbevent);
                /* schedule the DPC to get bb panic info */
                sched = ATH_ISR_SCHED;
            }

        }
    } while (0);

    if (sched == ATH_ISR_SCHED) {
        DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR, "%s: Scheduling BH/DPC\n",__func__);
        if (sc->sc_enhanceddmasupport) {
            /* For enhanced DMA turn off all interrupts except RXEOL, RXORN, SWBA.
             * Disable and then enable to satisfy the global isr enable reference counter. 
             */
#ifdef ATH_BEACON_DEFERRED_PROC
            ath_hal_intrset(ah, 0);
            ath_hal_intrset(ah, sc->sc_imask & (HAL_INT_GLOBAL | HAL_INT_RXEOL | HAL_INT_RXORN));
#else
            ath_hal_intrset(ah, 0);
            ath_hal_intrset(ah, sc->sc_imask & (HAL_INT_GLOBAL | HAL_INT_RXEOL | HAL_INT_RXORN | HAL_INT_SWBA));
#endif
        } else {
#ifdef ATH_BEACON_DEFERRED_PROC
            /* turn off all interrupts */
            ath_hal_intrset(ah, 0);
#else
            /* turn off every interrupt except SWBA */
            ath_hal_intrset(ah, (sc->sc_imask & HAL_INT_SWBA));
#endif
        }
    }

    return sched;
}

/*
 * Line Interrupt handler.  Most of the actual processing is deferred.
 *
 * It's the caller's responsibility to ensure the chip is awake.
 */
static int
ath_intr(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_INT status;
    int    isr_status = ATH_ISR_NOTMINE;

    atomic_inc(&sc->sc_inuse_cnt);

#if ATH_RESET_SERIAL
    atomic_inc(&sc->sc_intr_processing);
    if (atomic_read(&sc->sc_hold_reset)) {
        isr_status = ATH_ISR_NOTMINE;
        atomic_dec(&sc->sc_intr_processing);
        return isr_status;
    }
#endif

    do {
        if (!ath_hal_intrpend(ah)) {    /* shared irq, not for us */
            isr_status = ATH_ISR_NOTMINE;
            break;
        }

        if (sc->sc_invalid) {
            /*
             * The hardware is either not ready or is entering full sleep,
             * don't touch any RTC domain register.
             */
            ath_hal_intrset_nortc(ah, 0);
            ath_hal_getisr_nortc(ah, &status, 0, 0);
            isr_status = ATH_ISR_NOSCHED;
            DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR, "%s: recv interrupts when invalid.\n",__func__);
            break;
        }

        /*
         * Figure out the reason(s) for the interrupt.  Note
         * that the hal returns a pseudo-ISR that may include
         * bits we haven't explicitly enabled so we mask the
         * value to insure we only process bits we requested.
         */
        ath_hal_getisr(ah, &status, HAL_INT_LINE, 0);       /* NB: clears ISR too */
        DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR, "%s: status 0x%x  Mask: 0x%x\n",
                __func__, status, sc->sc_imask);
        if(status & HAL_INT_FATAL){ /* handle FATAL INTR here, disable further INTRs */
               printk("ath_intr status %x, handle FATAL_INTR here..", status);
               ath_hal_intrset(sc->sc_ah, 0);/* Disabling further INTRs until FATAL INTR is handled.
                                                ath_hal_internal reset is called by the tasklet; this 'reset' is the only 
						way this FATAL INTR can be recovered */
        }

        status &= sc->sc_imask;            /* discard unasked-for bits */

        /*
        ** If there are no status bits set, then this interrupt was not
        ** for me (should have been caught above).
        */

        if(!status)
        {
            DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR, "%s: Not My Interrupt\n",__func__);
            isr_status = ATH_ISR_NOSCHED;
            break;
        }

        sc->sc_intrstatus |= status;

        isr_status = ath_common_intr(dev, status);
    } while (FALSE);

#if ATH_RESET_SERIAL
    atomic_dec(&sc->sc_intr_processing);
#endif
    atomic_dec(&sc->sc_inuse_cnt);

    return isr_status;
}

/*
 * Message Interrupt handler.  Most of the actual processing is deferred.
 *
 * It's the caller's responsibility to ensure the chip is awake.
 */
static int
ath_mesg_intr (ath_dev_t dev, u_int mesgid)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_INT status;
    int    isr_status = ATH_ISR_NOTMINE;

    atomic_inc(&sc->sc_inuse_cnt);

#if ATH_RESET_SERIAL
    atomic_inc(&sc->sc_intr_processing);
    if (atomic_read(&sc->sc_hold_reset)) {
        isr_status = ATH_ISR_NOTMINE;
        atomic_dec(&sc->sc_intr_processing);
        return isr_status;
    } 
#endif

    do {
        /* handle all other interrutps */
        ath_hal_getisr(ah, &status, HAL_INT_MSI, mesgid);       /* NB: clears ISR too */

        DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR, "%s: status 0x%x  Mask: 0x%x\n",
                __func__, status, sc->sc_imask);

        if (sc->sc_invalid) {
            /*
             * The hardware is not ready/present, don't touch anything.
             * Note this can happen early on if the IRQ is shared.
             */
            ath_hal_intrset_nortc(ah, 0);
            ath_hal_getisr_nortc(ah, &status, 0, 0);
            isr_status = ATH_ISR_NOSCHED;
            DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR, "%s: recv interrupts when invalid.\n",__func__);
            break;
        }

        status &= sc->sc_imask;         /* discard unasked-for bits */
        sc->sc_intrstatus |= status;

        isr_status = ath_common_intr(dev, status);
    } while (FALSE);
#if ATH_RESET_SERIAL
    atomic_dec(&sc->sc_intr_processing);
#endif

    atomic_dec(&sc->sc_inuse_cnt);

    return isr_status;
}

struct ath_intr_status_params{
    struct ath_softc    *sc;
    u_int32_t           intr_status;
};

/*
 * intsafe is to exclude interrupt access. ath_handle_intr() normally is called
 * in DPC context.
 */
static void
ath_update_intrstatus_intsafe(struct ath_intr_status_params *p_params)
{
    p_params->intr_status = p_params->sc->sc_intrstatus;
    p_params->sc->sc_intrstatus &= (~p_params->intr_status);
}

static inline int 
ath_hw_tx_hang_check(struct ath_softc *sc)
{
    if(ath_hal_support_tx_hang_check(sc->sc_ah)) {
        if (ATH_MAC_HANG_WAR_REQ(sc)) {
            if (AH_TRUE == ath_hal_is_mac_hung(sc->sc_ah)) {
                ATH_MAC_GENERIC_HANG(sc);
                return 1;
            }
        }
    }
    return 0;
}

/*
 * Deferred interrupt processing
 */
static void
ath_handle_intr(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_intr_status_params params;
    u_int32_t status;
    u_int32_t rxmask;
    struct hal_bb_panic_info hal_bb_panic;
    struct ath_bb_panic_info bb_panic;
    int i;

    ATH_INTR_STATUS_LOCK(sc);
    /* Must synchronize with the ISR */
    params.sc = sc;
    params.intr_status = 0;
    OS_EXEC_INTSAFE(sc->sc_osdev, ath_update_intrstatus_intsafe, &params);
    status = params.intr_status;
    ATH_INTR_STATUS_UNLOCK(sc);

#if ATH_RESET_SERIAL
    atomic_inc(&sc->sc_dpc_processing);
    if (atomic_read(&sc->sc_dpc_stop)) {
        atomic_dec(&sc->sc_dpc_processing);
        return;
    }
#endif
    ATH_PS_WAKEUP(sc);

    do {
        if (sc->sc_invalid) {
            /*
             * The hardware is not ready/present, don't touch anything.
             * Note this can happen early on if the IRQ is shared.
             */
            DPRINTF(sc, ATH_DEBUG_INTR, "%s called when invalid.\n",__func__);
            break;
        }

        if (status & HAL_INT_FATAL) {
            /* need a chip reset */
            DPRINTF(sc, ATH_DEBUG_INTR, "%s: Got fatal intr\n", __func__);
            sc->sc_reset_type = ATH_RESET_NOLOSS;
            ath_internal_reset(sc);
            sc->sc_reset_type = ATH_RESET_DEFAULT;
            break;
        } else {
            if (status & HAL_INT_BBPANIC) {
                if (!ath_hal_get_bbpanic_info(sc->sc_ah, &hal_bb_panic)) {
                    bb_panic.status = hal_bb_panic.status;
                    bb_panic.tsf = hal_bb_panic.tsf;
                    bb_panic.wd = hal_bb_panic.wd;
                    bb_panic.det = hal_bb_panic.det;
                    bb_panic.rdar = hal_bb_panic.rdar;
                    bb_panic.rODFM = hal_bb_panic.rODFM;
                    bb_panic.rCCK = hal_bb_panic.rCCK;
                    bb_panic.tODFM = hal_bb_panic.tODFM;
                    bb_panic.tCCK = hal_bb_panic.tCCK;
                    bb_panic.agc = hal_bb_panic.agc;
                    bb_panic.src = hal_bb_panic.src;
                    bb_panic.phy_panic_wd_ctl1 = hal_bb_panic.phy_panic_wd_ctl1;
                    bb_panic.phy_panic_wd_ctl2 = hal_bb_panic.phy_panic_wd_ctl2;
                    bb_panic.phy_gen_ctrl = hal_bb_panic.phy_gen_ctrl;
                    bb_panic.cycles = hal_bb_panic.cycles;
                    bb_panic.rxc_pcnt = hal_bb_panic.rxc_pcnt;
                    bb_panic.rxf_pcnt = hal_bb_panic.rxf_pcnt;
                    bb_panic.txf_pcnt = hal_bb_panic.txf_pcnt;
                    bb_panic.valid = 1;

                    for (i = 0; i < MAX_BB_PANICS - 1; i++)
                        sc->sc_stats.ast_bb_panic[i] = sc->sc_stats.ast_bb_panic[i + 1];
                    sc->sc_stats.ast_bb_panic[MAX_BB_PANICS - 1] = bb_panic;
                }

                if (!(ath_hal_handle_radar_bbpanic(sc->sc_ah)) ){
                    /* reset to recover from the BB hang */
                    sc->sc_reset_type = ATH_RESET_NOLOSS;
                    ATH_RESET_LOCK(sc);
                    ath_hal_set_halreset_reason(sc->sc_ah, HAL_RESET_BBPANIC);
                    ATH_RESET_UNLOCK(sc);
                    ath_internal_reset(sc);
                    ATH_RESET_LOCK(sc);
                    ath_hal_clear_halreset_reason(sc->sc_ah);
                    ATH_RESET_UNLOCK(sc);
                    sc->sc_reset_type = ATH_RESET_DEFAULT;
                    sc->sc_stats.ast_resetOnError++;
                    /* EV92527 -- we are doing internal reset. break out */
                    break;
                }
                /* EV 92527 -- We are not doing any internal reset, continue normally */
            }
#ifdef ATH_BEACON_DEFERRED_PROC
            /* Handle SWBA first */
            if (status & HAL_INT_SWBA) {
                int needmark = 0;
                ath_beacon_tasklet(sc, &needmark);
            }
#endif

            if (((AH_TRUE == sc->sc_hang_check) && ath_hw_hang_check(sc)) ||
                (!sc->sc_noreset && (sc->sc_bmisscount >= (BSTUCK_THRESH_PERVAP * sc->sc_nvaps)))) {
                ath_bstuck_tasklet(sc);
                ATH_CLEAR_HANGS(sc);
                break;
            }
            /*
             * Howl needs DDR FIFO flush before any desc/dma data can be read.
             */
            ATH_FLUSH_FIFO();
            if (sc->sc_enhanceddmasupport) {
                rxmask = (HAL_INT_RXHP | HAL_INT_RXLP | HAL_INT_RXEOL | HAL_INT_RXORN);
            } else {
                rxmask = (HAL_INT_RX | HAL_INT_RXEOL | HAL_INT_RXORN);
            }

            if (status & rxmask 
#if ATH_SUPPORT_RX_PROC_QUOTA
                    || (sc->sc_rx_work)
#endif
                    ) {
                if (status & HAL_INT_RXORN) {
                    DPRINTF(sc, ATH_DEBUG_INTR, "%s: Got RXORN intr\n", __func__);
                }
                if (status & HAL_INT_RXEOL) {
                    DPRINTF(sc, ATH_DEBUG_INTR, "%s: Got RXEOL intr\n", __func__);
                }
#if ATH_SUPPORT_RX_PROC_QUOTA
                sc->sc_rx_work=0;
#endif                
                ath_handle_rx_intr(sc);
            }
            else if (sc->sc_rxfreebuf != NULL) {
                DPRINTF(sc, ATH_DEBUG_INTR, "%s[%d] ---- Athbuf FreeQ Not Empty - Calling AllocRxbufs for FreeList \n", __func__, __LINE__);
                // There are athbufs with no associated mbufs. Let's try to allocate some mbufs for these.            
                if (sc->sc_enhanceddmasupport) {
                    ath_edmaAllocRxbufsForFreeList(sc);
                }
                else {
                    ath_allocRxbufsForFreeList(sc);
                }
            } 
#if ATH_TX_POLL
            ath_handle_tx_intr(sc);
#else
            if (status & HAL_INT_TX) {
                sc->sc_consecutive_gtt_count = 0;
#ifdef ATH_TX_INACT_TIMER
                sc->sc_tx_inact = 0;
#endif
                ath_handle_tx_intr(sc);
            }
#endif
            if (status & HAL_INT_BMISS) {
                ath_bmiss_tasklet(sc);
            }

#define MAX_CONSECUTIVE_GTT_COUNT 5
            if (status & HAL_INT_GTT) {
                DPRINTF(sc, ATH_DEBUG_INTR, " GTT %d\n",sc->sc_consecutive_gtt_count);
                sc->sc_consecutive_gtt_count++;
                if(sc->sc_consecutive_gtt_count >= MAX_CONSECUTIVE_GTT_COUNT) {
                    DPRINTF(sc, ATH_DEBUG_INTR, "%s: Consecutive GTTs\n", __func__);
                    if (ath_hw_tx_hang_check(sc)) {
                        /* reset the chip */
                        sc->sc_reset_type = ATH_RESET_NOLOSS;
                        ath_internal_reset(sc);
                        sc->sc_reset_type = ATH_RESET_DEFAULT;
                        sc->sc_stats.ast_resetOnError++;
                        DPRINTF(sc, ATH_DEBUG_INTR, "%s: --- TX Hang detected resetting chip --- \n",__func__);
                    }
                    sc->sc_consecutive_gtt_count = 0;
                }
            }
 

            if (status & HAL_INT_CST) {
                ath_txto_tasklet(sc);
            }
            if (status & (HAL_INT_TIM | HAL_INT_DTIMSYNC)) {
                if (status & HAL_INT_TIM) {
                    if (sc->sc_ieee_ops->proc_tim)
                        sc->sc_ieee_ops->proc_tim(sc->sc_ieee);
                }
                if (status & HAL_INT_DTIMSYNC) {
                    DPRINTF(sc, ATH_DEBUG_INTR, "%s: Got DTIMSYNC intr\n", __func__);
                }
            }
            if (status & HAL_INT_GPIO) {
#ifdef ATH_RFKILL
                ath_rfkill_gpio_intr(sc);
#endif
#ifdef ATH_BT_COEX
                if (sc->sc_btinfo.bt_gpioIntEnabled) {
                    ath_bt_coex_gpio_intr(sc);
                }
#endif
            }

        if (ATH_ENAB_AOW(sc) && (status & HAL_INT_GENTIMER)) {
            #ifdef ATH_USB
            ath_gen_timer_isr(sc,0,0,0);
            #else
            ath_gen_timer_isr(sc);
            #endif
        }

#ifdef ATH_GEN_TIMER
        
            if (status & HAL_INT_TSFOOR) {
                /* There is a jump in the TSF time with this OUT OF RANGE interupt. */
                DPRINTF(sc, ATH_DEBUG_ANY, "%s: Got HAL_INT_TSFOOR intr\n", __func__);

                /* If the current mode is Station, then we need to reprogram the beacon timers. */
                if (sc->sc_opmode  ==  HAL_M_STA) {
                    ath_beacon_config(sc,ATH_BEACON_CONFIG_REASON_RESET,ATH_IF_ID_ANY);
                }

                ath_gen_timer_tsfoor_isr(sc);
            }
            
            if (status & HAL_INT_GENTIMER) {
                #ifdef ATH_USB
                ath_gen_timer_isr(sc,0,0,0);
                #else
                ath_gen_timer_isr(sc);
                #endif
            }
#endif
        }

#if ATH_RESET_SERIAL
        ATH_RESET_LOCK(sc);
        if (atomic_read(&sc->sc_dpc_stop)) {
            ATH_RESET_UNLOCK(sc);
            break;
        }
#endif
        /* re-enable hardware interrupt */
        if (sc->sc_enhanceddmasupport) {
            /* For enhanced DMA, certain interrupts are already enabled (e.g. RXEOL),
             * but now re-enable _all_ interrupts.
             * Note: disable and then enable to satisfy the global isr enable reference counter. 
             */
            ath_hal_intrset(sc->sc_ah, 0);
            ath_hal_intrset(sc->sc_ah, sc->sc_imask);
        } else {
            ath_hal_intrset(sc->sc_ah, sc->sc_imask);
        }
#if ATH_RESET_SERIAL
        ATH_RESET_UNLOCK(sc);
#endif
    } while (FALSE);

    ATH_PS_SLEEP(sc);
#if ATH_RESET_SERIAL
    atomic_dec(&sc->sc_dpc_processing);
#endif
}

/*
 * ATH symbols exported to the HAL.
 * Make table static so that clients can keep a pointer to it
 * if they so choose.
 */
static u_int32_t
ath_read_pci_config_space(struct ath_softc *sc, u_int32_t offset, void *buffer, u_int32_t len)
{
    return OS_PCI_READ_CONFIG(sc->sc_osdev, offset, buffer, len);
}

static const struct ath_hal_callback halCallbackTable = {
    /* Callback Functions */
    /* read_pci_config_space */ (HAL_BUS_CONFIG_READER)ath_read_pci_config_space
};

static int
ath_get_caps(ath_dev_t dev, ATH_CAPABILITY_TYPE type)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    int supported = 0;

    switch (type) {
    case ATH_CAP_FF:
        supported = ath_hal_fastframesupported(ah);
        break;
    case ATH_CAP_BURST:
        supported = ath_hal_burstsupported(ah);
        break;
    case ATH_CAP_COMPRESSION:
        supported = sc->sc_hascompression;
        break;
    case ATH_CAP_TURBO:
        supported = ath_hal_turboagsupported(ah, sc->sc_config.ath_countrycode);
        break;
    case ATH_CAP_XR:
        supported = ath_hal_xrsupported(ah);
        break;
    case ATH_CAP_TXPOWER:
        supported = (sc->sc_hastpc || ath_hal_hastxpowlimit(ah));
        break;
    case ATH_CAP_DIVERSITY:
        supported = sc->sc_diversity;
        break;
    case ATH_CAP_BSSIDMASK:
        supported = sc->sc_hasbmask;
        break;
    case ATH_CAP_TKIP_SPLITMIC:
        supported = ath_hal_tkipsplit(ah);
        break;
    case ATH_CAP_MCAST_KEYSEARCH:
        supported = ath_hal_getmcastkeysearch(ah);
        break;
    case ATH_CAP_TKIP_WMEMIC:
        supported = ath_hal_wmetkipmic(ah);
        break;
    case ATH_CAP_WMM:
        supported = sc->sc_haswme;
        break;
    case ATH_CAP_HT:
        supported = sc->sc_hashtsupport;
        break;
    case ATH_CAP_HT20_SGI:
        supported = sc->sc_ht20sgisupport;
        break;
    case ATH_CAP_RX_STBC:
        supported = sc->sc_rxstbcsupport;
        break;
    case ATH_CAP_TX_STBC:
        supported = sc->sc_txstbcsupport;
        break;
    case ATH_CAP_LDPC:
        supported = sc->sc_ldpcsupport;
        break;
    case ATH_CAP_4ADDR_AGGR:
        supported = ath_hal_4addraggrsupported(ah);
        break;
    case ATH_CAP_ENHANCED_PM_SUPPORT:
        supported = ath_hal_hasenhancedpmsupport(ah);
        break;
    case ATH_CAP_WPS_BUTTON:
        if (ath_hal_haswpsbutton(ah) && sc->sc_reg_parm.wpsButtonGpio) {

            /* Overload push button status when reporting capability */
            supported |= (ATH_WPS_BUTTON_EXISTS | ATH_WPS_BUTTON_DOWN_SUP | ATH_WPS_BUTTON_STATE_SUP);

            supported |= (sc->sc_wpsbuttonpushed ? ATH_WPS_BUTTON_PUSHED : 0);
            sc->sc_wpsbuttonpushed = 0;  /* Clear status after query */

            /* Get current push button status, GPIO polarity low */
            if (ath_hal_gpioget(sc->sc_ah, sc->sc_reg_parm.wpsButtonGpio)) {
                /* GPIO line normal, re-enable GPIO interrupt */
                ath_hal_gpioSetIntr(ah, sc->sc_reg_parm.wpsButtonGpio, HAL_GPIO_INTR_LOW);
                sc->sc_wpsgpiointr = 1;
            } else {
                supported |= ATH_WPS_BUTTON_PUSHED_CURR;
            }
        }
        break;
    case ATH_CAP_MBSSID_AGGR_SUPPORT:/* MBSSID Aggregation support capability*/
          supported = ath_hal_hasMbssidAggrSupport(ah);
          break;
#ifdef ATH_SWRETRY
    case ATH_CAP_SWRETRY_SUPPORT:
        supported = sc->sc_swRetryEnabled;
        break;
#endif
#ifdef ATH_SUPPORT_UAPSD
    case ATH_CAP_UAPSD:
        supported = sc->sc_uapsdsupported;
        break;
#endif
    case ATH_CAP_DYNAMIC_SMPS:
        supported = ath_hal_hasdynamicsmps(ah);
        break;
    case ATH_CAP_EDMA_SUPPORT:
        supported = ath_hal_hasenhanceddmasupport(ah);
        break;
    case ATH_CAP_WEP_TKIP_AGGR:
        supported = ath_hal_weptkipaggrsupport(ah);
        break;
    case ATH_CAP_DS:
        supported = ath_hal_getcapability(ah, HAL_CAP_DS, 0, NULL) == HAL_OK ? TRUE : FALSE;
        break;
    case ATH_CAP_TS:
        supported = ath_hal_getcapability(ah, HAL_CAP_TS, 0, NULL) == HAL_OK ? TRUE : FALSE;
        break;
    case ATH_CAP_NUMTXCHAINS:
        {
            int chainmask, ret;
            ret = ath_hal_gettxchainmask(ah, (u_int32_t *)&chainmask);
            supported = 0;
            if(sc->sc_tx_chainmask != chainmask)
            {
                printk("%s[%d] tx chainmask mismatch actual %d sc_chainmak %d\n", __func__, __LINE__,
                    chainmask, sc->sc_tx_chainmask);
            }
            switch (chainmask) {
            case 1:
                supported = 1;
                break;
            case 3:
            case 5:
            case 6:
                supported = 2;
                break;
            case 7:
                supported = 3;
                break;
            }
        }
        break;
    case ATH_CAP_NUMRXCHAINS:
        {
            int chainmask, ret;
            ret = ath_hal_getrxchainmask(ah, (u_int32_t *)&chainmask);
            supported = 0;
            if(sc->sc_rx_chainmask != chainmask)
            {
                printk("%s[%d] rx chainmask mismatch actual %d sc_chainmak %d\n", __func__, __LINE__,
                    chainmask, sc->sc_rx_chainmask);
            }
            switch (chainmask) {
            case 1:
                supported = 1;
                break;
            case 3:
            case 5:
            case 6:
                supported = 2;
                break;
            case 7:
                supported = 3;
                break;
            }
        }
        break;
    case ATH_CAP_RXCHAINMASK:
        supported = sc->sc_rx_chainmask;
        //ath_hal_gettxchainmask(ah, (u_int32_t *)&supported);
    break;
    case ATH_CAP_TXCHAINMASK:
        supported = sc->sc_tx_chainmask;
    break;
    case ATH_CAP_DEV_TYPE:
        ath_hal_getcapability(ah, HAL_CAP_DEVICE_TYPE, 0, (u_int32_t *)&supported);
        break;
    case ATH_CAP_EXTRADELIMWAR:
        ath_hal_getcapability(ah, HAL_CAP_EXTRADELIMWAR, 0,
                    (u_int32_t*)&supported);
        break;
#ifdef ATH_SUPPORT_TxBF
    case ATH_CAP_TXBF:
        supported = ath_hal_hastxbf(ah);
        break;
#endif
    case ATH_CAP_LDPCWAR:
        supported = ath_hal_getcapability(ah, HAL_CAP_LDPCWAR, 0, 0);
        break;
    case ATH_CAP_ZERO_MPDU_DENSITY:
        supported = sc->sc_ent_min_pkt_size_enable;
        break;
#if ATH_SUPPORT_DYN_TX_CHAINMASK
    case ATH_CAP_DYN_TXCHAINMASK:
        supported = sc->sc_dyn_txchainmask;
        break;
#endif /* ATH_SUPPORT_DYN_TX_CHAINMASK */
#if ATH_SUPPORT_WAPI
    case ATH_CAP_WAPI_MAXTXCHAINS:
        ath_hal_getwapimaxtxchains(ah, (u_int32_t *)&supported);
        break;
    case ATH_CAP_WAPI_MAXRXCHAINS:
        ath_hal_getwapimaxrxchains(ah, (u_int32_t *)&supported);
        break;
#endif
#if UMAC_SUPPORT_SMARTANTENNA        
    case ATH_CAP_SMARTANTENNA:
            supported = sc->sc_smartant_enable;
        break;
#endif        
    default:
        supported = 0;
        break;
    }
    return supported;
}

static void 
ath_suspend_led_control(ath_dev_t dev)
{
    ath_led_suspend(dev);
}

static int
ath_get_ciphers(ath_dev_t dev, HAL_CIPHER cipher)
{
    return ath_hal_ciphersupported(ATH_DEV_TO_SC(dev)->sc_ah, cipher);
}

static struct ath_phy_stats *
ath_get_phy_stats(ath_dev_t dev, WIRELESS_MODE wmode)
{
    return &(ATH_DEV_TO_SC(dev)->sc_phy_stats[wmode]);
}

static struct ath_stats *
ath_get_ath_stats(ath_dev_t dev)
{
    return &(ATH_DEV_TO_SC(dev)->sc_stats);
}

static struct ath_11n_stats *
ath_get_11n_stats(ath_dev_t dev)
{
    return &(ATH_DEV_TO_SC(dev)->sc_stats.ast_11n_stats);
}

static void
ath_clear_stats(ath_dev_t dev)
{
    OS_MEMZERO(&(ATH_DEV_TO_SC(dev)->sc_phy_stats[0]),
               sizeof(struct ath_phy_stats) * WIRELESS_MODE_MAX);
}

static void
ath_update_beacon_info(ath_dev_t dev, int avgbrssi)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    /* Update beacon RSSI */
    sc->sc_halstats.ns_avgbrssi = avgbrssi;

    if (! sc->sc_hasautosleep) {
        sc->sc_waitbeacon = 0;
    }
    ath_hal_chk_rssi(sc->sc_ah, ATH_RSSI_OUT(avgbrssi));
}

static void
ath_set_macmode(ath_dev_t dev, HAL_HT_MACMODE macmode)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ATH_FUNC_ENTRY_VOID(sc);
    ATH_PS_WAKEUP(sc);
    ath_hal_set11nmac2040(sc->sc_ah, macmode);
    ATH_PS_SLEEP(sc);
}

static void
ath_set_extprotspacing(ath_dev_t dev, HAL_HT_EXTPROTSPACING extprotspacing)
{
    ATH_DEV_TO_SC(dev)->sc_ht_extprotspacing = extprotspacing;
}

static int
ath_get_extbusyper(ath_dev_t dev)
{
    return ath_hal_get11nextbusy(ATH_DEV_TO_SC(dev)->sc_ah);
}

#ifndef REMOVE_PKT_LOG
static int
ath_start_pktlog(ath_dev_t dev, int log_state)
{
    int error;
    ath_pktlog_start(ATH_DEV_TO_SC(dev), log_state, error);
    return error;
}

static int
ath_read_pktlog_hdr(ath_dev_t dev, void *buf, u_int32_t buf_len,
                    u_int32_t *required_len,
                    u_int32_t *actual_len)
{
    int error;
    ath_pktlog_read_hdr(ATH_DEV_TO_SC(dev), buf, buf_len,
                        required_len, actual_len, error);
    return error;
}

static int
ath_read_pktlog_buf(ath_dev_t dev, void *buf, u_int32_t buf_len,
                    u_int32_t *required_len,
                    u_int32_t *actual_len)
{
    int error;
    ath_pktlog_read_buf(ATH_DEV_TO_SC(dev), buf, buf_len,
                        required_len, actual_len, error);
    return error;
}
#endif

#if ATH_SUPPORT_IQUE
static void
ath_setacparams(ath_dev_t dev, u_int8_t ac, u_int8_t use_rts,
                u_int8_t aggrsize_scaling, u_int32_t min_kbps)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (ac < WME_NUM_AC) {
        sc->sc_ac_params[ac].use_rts = use_rts;
        sc->sc_ac_params[ac].min_kbps = min_kbps;
        sc->sc_ac_params[ac].aggrsize_scaling = aggrsize_scaling;
    }
}

static void
ath_setrtparams(ath_dev_t dev, u_int8_t rt_index, u_int8_t perThresh,
                u_int8_t probeInterval)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (rt_index < 2) {
        sc->sc_rc_params[rt_index].per_threshold = perThresh;
        sc->sc_rc_params[rt_index].probe_interval = probeInterval;
    }
}

static void
ath_sethbrparams(ath_dev_t dev, u_int8_t ac, u_int8_t enable, u_int8_t perlow)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int8_t perhigh;
    perhigh = sc->sc_rc_params[0].per_threshold;
    if (perhigh < sc->sc_rc_params[1].per_threshold)
        perhigh = sc->sc_rc_params[1].per_threshold;

    /* Now we only support HBR for VI */
    if (ac == WME_AC_VI) {
        sc->sc_hbr_params[ac].hbr_enable = enable;
        sc->sc_hbr_params[ac].hbr_per_low = (perlow > (perhigh - 10))? (perhigh-10):perlow;
    }
}

/*
 * Dump all AC and rate control parameters related to IQUE.
 */
static void
ath_getiqueconfig(ath_dev_t dev)
{
#if ATH_DEBUG
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    printk("\n========= Rate Control Table Config========\n");
    printk("AC\tPER\tProbe Interval\n\n");
    printk("BE&BK\t%d\t\t%d \n",
           sc->sc_rc_params[0].per_threshold,
           sc->sc_rc_params[0].probe_interval);
    printk("VI&VO\t%d\t\t%d \n",
           sc->sc_rc_params[1].per_threshold,
           sc->sc_rc_params[1].probe_interval);

    printk("\n========= Access Category Config===========\n");
    printk("AC\tRTS \tAggr Scaling\tMin Rate(Kbps)\tHBR \tPER LO \tRXTIMEOUT\n\n");
    printk("BE\t%s\t\t%d\t%6d\t\t%s\t%d\t%d\n",
           sc->sc_ac_params[0].use_rts?"Yes":"No",
           sc->sc_ac_params[0].aggrsize_scaling,
           sc->sc_ac_params[0].min_kbps,
           sc->sc_hbr_params[0].hbr_enable?"Yes":"No",
           sc->sc_hbr_params[0].hbr_per_low,
           sc->sc_rxtimeout[0]);
    printk("BK\t%s\t\t%d\t%6d\t\t%s\t%d\t%d\n",
           sc->sc_ac_params[1].use_rts?"Yes":"No",
           sc->sc_ac_params[1].aggrsize_scaling,
           sc->sc_ac_params[1].min_kbps,
           sc->sc_hbr_params[1].hbr_enable?"Yes":"No",
           sc->sc_hbr_params[1].hbr_per_low,
           sc->sc_rxtimeout[1]);
    printk("VI\t%s\t\t%d\t%6d\t\t%s\t%d\t%d\n",
           sc->sc_ac_params[2].use_rts?"Yes":"No",
           sc->sc_ac_params[2].aggrsize_scaling,
           sc->sc_ac_params[2].min_kbps,
           sc->sc_hbr_params[2].hbr_enable?"Yes":"No",
           sc->sc_hbr_params[2].hbr_per_low,
           sc->sc_rxtimeout[2]);
    printk("VO\t%s\t\t%d\t%6d\t\t%s\t%d\t%d\n",
           sc->sc_ac_params[3].use_rts?"Yes":"No",
           sc->sc_ac_params[3].aggrsize_scaling,
           sc->sc_ac_params[3].min_kbps,
           sc->sc_hbr_params[3].hbr_enable?"Yes":"No",
           sc->sc_hbr_params[3].hbr_per_low,
           sc->sc_rxtimeout[3]);
#endif /* ATH_DEBUG */
}

#endif

static void
ath_setrxtimeout(ath_dev_t dev, u_int8_t ac, u_int8_t rxtimeout)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (ac < WME_NUM_AC) {
        sc->sc_rxtimeout[ac] = rxtimeout;
    }
}

static void
ath_set_ps_awake(ath_dev_t dev)
{
    ath_pwrsave_awake(dev);
}

static void
ath_set_ps_netsleep(ath_dev_t dev)
{
    ath_pwrsave_netsleep(dev);
}

static void
ath_set_ps_fullsleep(ath_dev_t dev)
{
    ath_pwrsave_fullsleep(dev);
}

static int
ath_get_ps_state(ath_dev_t dev)
{
    int state;
    state = ath_pwrsave_get_state(dev);
    return state;
}

static void 
ath_set_ps_state(ath_dev_t dev, int newstateint)
{
    ath_pwrsave_set_state(dev, newstateint);
}

static void ath_green_ap_dev_set_enable( ath_dev_t dev, int val )
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    
    if( val && (sc->sc_opmode != HAL_M_HOSTAP)) {
        printk("Cannot enable Green power save when not in HOSTAP mode\n");   
        return;
    }    
    ath_green_ap_sc_set_enable(sc, val);
}
    
static int ath_green_ap_dev_get_enable( ath_dev_t dev)
{
    return ath_green_ap_sc_get_enable(ATH_DEV_TO_SC(dev));
}


static void ath_green_ap_dev_set_transition_time( ath_dev_t dev, int val )
{
    ath_green_ap_sc_set_transition_time(ATH_DEV_TO_SC(dev), val);    
}

static int ath_green_ap_dev_get_transition_time( ath_dev_t dev)
{
    return ath_green_ap_sc_get_transition_time(ATH_DEV_TO_SC(dev));
}

static void ath_green_ap_dev_set_on_time( ath_dev_t dev, int val )
{
    ath_green_ap_sc_set_on_time(ATH_DEV_TO_SC(dev), val);    
}

static int ath_green_ap_dev_get_on_time( ath_dev_t dev)
{
    return ath_green_ap_sc_get_on_time(ATH_DEV_TO_SC(dev));
}

static int16_t ath_dev_noisefloor( ath_dev_t dev) 
{
    return ath_hal_get_noisefloor((ATH_DEV_TO_SC(dev))->sc_ah);
}

#if ATH_SUPPORT_5G_EACS
static void  ath_dev_get_chan_stats( ath_dev_t dev, void *chan_stats ) 
{
    HAL_VOWSTATS pcounts;
    struct ath_chan_stats *cur_chan_stats = ( struct ath_chan_stats *)chan_stats;  
    ath_hal_getVowStats((ATH_DEV_TO_SC(dev))->sc_ah,&pcounts);
    cur_chan_stats->cycle_cnt = pcounts.cycle_count;
    cur_chan_stats->chan_clr_cnt = pcounts.rx_clear_count;

#if 0
    printk ( "cnt %d %d %d %d %d \n",  pcounts.tx_frame_count, pcounts.rx_frame_count,  pcounts.rx_clear_count,  pcounts.cycle_count,  pcounts.ext_cycle_count);
    /*phy error count will be added later.*/
    struct ath_phy_stats phy_stats = (ATH_DEV_TO_SC(dev))->sc_phy_stats[(ATH_DEV_TO_SC(dev))->sc_curmode];
    printk ( "err %d %d %llu %llu %x \n",  (u_int64_t)phy_stats.ast_rx_err, phy_stats.ast_rx_crcerr,  phy_stats.ast_rx_fifoerr,  phy_stats.ast_rx_phyerr,  phy_stats.ast_rx_decrypterr);    
#endif
}
#endif

static u_int32_t ath_get_goodput(ath_dev_t dev, ath_node_t node)
{
    struct ath_node *an = ATH_NODE(node);
    if (an) return an->an_rc_node->txRateCtrl.rptGoodput;
    return 0;
}

#if ATH_SUPPORT_FLOWMAC_MODULE
void
ath_netif_flowmac_pause(ath_dev_t dev, int stall)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (sc->sc_ethflowmac_enable) {
        flowmac_pause(sc->sc_flowmac_dev, stall,
                        stall ? FLOWMAC_PAUSE_TIMEOUT:0);
    }
}
void
ath_netif_stop_queue(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (!sc->sc_devstopped) {
        OS_NETIF_STOP_QUEUE(sc->sc_osdev->netdev);
        sc->sc_devstopped = 1;
        sc->sc_stats.ast_tx_stop++;
        if (sc->sc_flowmac_debug) {
            DPRINTF(sc, ATH_DEBUG_XMIT, "devid 0x%p stopped stop_count %d\n",
                    sc, sc->sc_stats.ast_tx_stop);
        }
    }
}

void
ath_netif_wake_queue(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (sc->sc_devstopped) {
        OS_NETIF_WAKE_QUEUE(sc->sc_osdev->netdev);
        sc->sc_devstopped = 0;
        sc->sc_stats.ast_tx_resume++;

        if (sc->sc_flowmac_debug) {
            DPRINTF(sc, ATH_DEBUG_XMIT, "devid 0x%p resume %d\n",
                    sc, sc->sc_stats.ast_tx_resume);
        }

    }
}

void
ath_netif_set_wdtimeout(ath_dev_t dev, int val)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (sc->sc_osnetif_flowcntrl) {
        OS_SET_WDTIMEOUT(sc->sc_osdev->netdev, val);
    }
}


void
ath_netif_clr_flag(ath_dev_t dev, unsigned int flag)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (sc->sc_osnetif_flowcntrl) {
        OS_CLR_NETDEV_FLAG(sc->sc_osdev->netdev, flag);
    }
}

void
ath_netif_update_trans(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (sc->sc_osnetif_flowcntrl) {
        OS_NETDEV_UPDATE_TRANS(sc->sc_osdev->netdev);
    }
}
#endif

#if UMAC_SUPPORT_VI_DBG
/* This function is used to reset video debug static variables in lmac
 * related to logging pkt stats using athstats  
 */
static void ath_set_vi_dbg_restart(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    sc->sc_vi_dbg_start = 0;
}

/* This function is used to enable logging of rx pkt stats using 
 * athstats  
 */
static void ath_set_vi_dbg_log(ath_dev_t dev, bool enable)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    sc->sc_en_vi_dbg_log = enable;
}
#endif

/* This function returns the next valid sequence number for a given
 * node/tid combination. If tx addba is not established, it returns 0.
 */
static u_int16_t ath_get_tid_seqno(ath_dev_t dev, ath_node_t node, u_int8_t tidno)
{
    struct ath_atx_tid *tid = ATH_AN_2_TID(ATH_NODE(node), tidno);
    // Not holding locks since this is opportunistic
    u_int16_t seq = tid->seq_next;

    if (ath_aggr_query(tid)) { // If addba is completed or pending
        return seq;
    } else {
        return 0;
    }
}

/*
 * Callback table for Atheros ARXXXX MACs
 */
static const struct ath_ops ath_ar_ops = {
    ath_get_wirelessmodes,      /* get_wirelessmodes */
    ath_get_device_info,        /* get_device_info */
    ath_get_caps,               /* have_capability */
    ath_get_ciphers,            /* has_cipher */
#ifndef ATH_SUPPORT_HTC
    ath_open,                   /* open */
#else
    0,
#endif
    ath_suspend,                /* stop */
    ath_vap_attach,             /* add_interface */
    ath_vap_detach,             /* remove_interface */
    ath_vap_config,             /* config_interface */
    ath_vap_down,               /* down */
    ath_vap_listen,             /* listen */
    ath_vap_join,               /* join */
    ath_vap_up,                 /* up */
    ath_vap_stopping,           /* stopping */
    ath_node_attach,            /* alloc_node */
    ath_node_detach,            /* free_node */
    ath_node_cleanup,           /* cleanup_node */
    ath_node_update_pwrsave,    /* update_node_pwrsave */
    ath_newassoc,               /* new_assoc */
    ath_enable_intr,            /* enable_interrupt */
    ath_disable_intr,           /* disable_interrupt */
    ath_intr,                   /* isr */
    ath_mesg_intr,              /* msisr */
    ath_handle_intr,            /* handle_intr */
    ath_reset_start,            /* reset_start */
    ath_reset,                  /* reset */
    ath_reset_end,              /* reset_end */
    ath_switch_opmode,          /* switch_opmode */
    ath_set_channel,            /* set_channel */
    ath_get_channel,            /* get_channel */
    ath_scan_start,             /* scan_start */
    ath_scan_end,               /* scan_end */
    ath_scan_enable_txq,        /* scan_enable_txq */
    ath_enter_led_scan_state,   /* led_scan_start */
    ath_leave_led_scan_state,   /* led_scan_end */
    ath_notify_force_ppm,       /* force_ppm_notify */
    ath_beacon_sync,            /* sync_beacon */
    ath_update_beacon_info,     /* update_beacon_info */
#ifdef ATH_USB
    ath_heartbeat_callback,     /* heartbeat_callback */    
#endif
#if defined(ATH_SUPPORT_HTC) && defined(ATH_HTC_TX_SCHED)
    0,                          /* tx_init */
    0,                          /* tx_cleanup */
#else
    ath_tx_init,                /* tx_init */
    ath_tx_cleanup,             /* tx_cleanup */
#endif
    ath_tx_get_qnum,            /* tx_get_qnum */
    ath_txq_update,             /* txq_update */
#ifndef ATH_SUPPORT_HTC
    ath_tx_start,               /* tx */
#else
    0,                          /* tx */
#endif
    ath_tx_tasklet,             /* tx_proc */
    ath_tx_flush,               /* tx_flush */
    ath_txq_depth,              /* txq_depth */
    ath_txq_aggr_depth,         /* txq_aggr_depth */
#ifdef ATH_TX_BUF_FLOW_CNTL
    ath_txq_set_minfree,        /* txq_set_minfree */
#else
    0,
#endif
#ifndef ATH_SUPPORT_HTC
    ath_rx_init,      /* rx_init */
#else
    0,                /* rx_init */
#endif
    ath_rx_cleanup,             /* rx_cleanup */
    ath_rx_tasklet,             /* rx_proc */
    ath_rx_requeue,             /* rx_requeue */
    ath_rx_input,               /* rx_proc_frame */
    ath_aggr_check,             /* check_aggr */
    ath_set_ampduparams,        /* set_ampdu_params */
    ath_set_weptkip_rxdelim,    /* set_weptkip_rxdelim */
    ath_addba_requestsetup,     /* addba_request_setup */
    ath_addba_responsesetup,    /* addba_response_setup */
    ath_addba_requestprocess,   /* addba_request_process */
    ath_addba_responseprocess,  /* addba_response_process */
    ath_addba_clear,            /* addba_clear */
    ath_addba_cancel_timers,    /* addba_cancel_timers */
    ath_delba_process,          /* delba_process */
    ath_addba_status,           /* addba_status */
    ath_aggr_teardown,          /* aggr_teardown */
    ath_set_addbaresponse,      /* set_addbaresponse */
    ath_clear_addbaresponsestatus, /* clear_addbaresponsestatus */
    ath_updateslot,             /* set_slottime */
    ath_set_protmode,           /* set_protmode */
    ath_set_txpowlimit,         /* set_txpowlimit */
    ath_enable_tpc,             /* enable_tpc */
    ath_get_mac_address,        /* get_macaddr */
    ath_get_hw_mac_address,     /* get_hw_macaddr */
    ath_set_mac_address,        /* set_macaddr */
    ath_set_mcastlist,          /* set_mcastlist */
    ath_set_rxfilter,           /* set_rxfilter */
    ath_opmode_init,            /* mc_upload */
    ath_key_alloc_2pair,        /* key_alloc_2pair */
    ath_key_alloc_pair,         /* key_alloc_pair */
    ath_key_alloc_single,       /* key_alloc_single */
    ath_key_reset,              /* key_delete */
    ath_keyset,                 /* key_set */
    ath_keycache_size,          /* keycache_size */
    ath_get_sw_phystate,        /* get_sw_phystate */
    ath_get_hw_phystate,        /* get_hw_phystate */
    ath_set_sw_phystate,        /* set_sw_phystate */
    ath_radio_enable,           /* radio_enable */
    ath_radio_disable,          /* radio_disable */
    ath_suspend_led_control,    /* led_suspend */
    ath_set_ps_awake,           /* awake */
    ath_set_ps_netsleep,        /* netsleep */
    ath_set_ps_fullsleep,       /* fullsleep */
    ath_get_ps_state,           /* getpwrsavestate */
    ath_set_ps_state,           /* setpwrsavestate */
    ath_set_country,            /* set_country */
    ath_get_currentCountry,     /* get_current_country */
    ath_set_regdomain,          /* set_regdomain */
    ath_get_regdomain,          /* get_regdomain */
    ath_get_dfsdomain,          /* get_dfsdomain */
    ath_set_quiet,              /* set_quiet */
    ath_find_countrycode,       /* find_countrycode */
    ath_get_ctl_by_country,     /* get_ctl_by_country */
    ath_dfs_isdfsregdomain,     /* dfs_isdfsregdomain */
    ath_dfs_in_cac,             /* dfs_in_cac */
    ath_dfs_usenol,             /* dfs_usenol */
    ath_set_tx_antenna_select,  /* set_antenna_select */
    ath_get_current_tx_antenna, /* get_current_tx_antenna */
    ath_get_default_antenna,    /* get_default_antenna */
    ath_notify_device_removal,  /* notify_device_removal */
    ath_detect_card_present,    /* detect_card_present */
    ath_mhz2ieee,               /* mhz2ieee */
    ath_get_phy_stats,          /* get_phy_stats */
    ath_get_ath_stats,          /* get_ath_stats */
    ath_get_11n_stats,          /* get_11n_stats */
    ath_clear_stats,            /* clear_stats */
    ath_set_macmode,            /* set_macmode */
    ath_set_extprotspacing,     /* set_extprotspacing */
    ath_get_extbusyper,         /* get_extbusyper */
#ifdef ATH_SUPERG_FF
    ath_ff_check,               /* check_ff */
#endif
#ifdef ATH_SUPPORT_DFS
    ath_dfs_control,            /* ath_dfs_control */
#endif
#if ATH_WOW    
    ath_get_wow_support,        /* ath_get_wow_support */
    ath_set_wow_enable,         /* ath_set_wow_enable */
    ath_wow_wakeup,             /* ath_wow_wakeup */
    ath_set_wow_events,         /* ath_set_wow_events */
    ath_get_wow_events,         /* ath_get_wow_events */
    ath_wow_add_wakeup_pattern, /* ath_wow_add_wakeup_pattern */
    ath_wow_remove_wakeup_pattern, /* ath_wow_remove_wakeup_pattern */
    ath_get_wow_wakeup_reason,  /* ath_get_wow_wakeup_reason */
    ath_wow_matchpattern_exact, /* ath_wow_matchpattern_exact */
    ath_wow_set_duration,       /* ath_wow_set_duration */
    ath_set_wow_timeout,        /* ath_set_wow_timeout */
    ath_get_wow_timeout,        /* ath_get_wow_timeout */ 
#endif

    ath_get_config,             /* ath_get_config_param */
    ath_set_config,             /* ath_set_config_param */
    ath_get_noisefloor,         /* get_noisefloor */
    ath_get_chainnoisefloor,    /* get_chainnoisefloor */
#if ATH_SUPPORT_RAW_ADC_CAPTURE
    ath_get_min_agc_gain,       /* get_min_agc_gain */
#endif
    ath_update_sm_pwrsave,      /* update_sm_pwrsave */
    ath_node_gettxrate,         /* node_gettxrate */
    ath_node_set_fixedrate,     /* node_setfixedrate */
    ath_node_getmaxphyrate,     /* node_getmaxphyrate */
    athop_rate_newassoc,        /* ath_rate_newassoc */
    athop_rate_newstate,        /* ath_rate_newstate */
#ifdef DBG
    ath_register_read,          /* Read register value  */
    ath_register_write,         /* Write register value */
#endif
    ath_setTxPwrAdjust,         /* ath_set_txPwrAdjust */
    ath_setTxPwrLimit,          /* ath_set_txPwrLimit */
    ath_get_common_power,       /* get_common_power */
    ath_gettsf32,               /* ath_get_tsf32 */
    ath_gettsf64,               /* ath_get_tsf64 */
    ath_setrxfilter,            /* ath_set_rxfilter */
    ath_set_sel_evm,            /* ath_set_sel_evm */
#ifdef ATH_CCX
    ath_update_mib_macstats,    /* ath_update_mibMacstats */
    ath_get_mib_macstats,       /* ath_get_mibMacstats */
    ath_rcRateValueToPer,       /* rcRateValueToPer */
    ath_get_mib_cyclecounts,    /* ath_get_mibCycleCounts */
    ath_clear_mib_counters,     /* ath_clear_mibCounters */
    ath_getserialnumber,        /* ath_get_sernum */
    ath_getchandata,            /* ath_get_chandata */
    ath_getcurrssi,             /* ath_get_curRSSI */
#endif
    ath_get_amsdusupported,     /* get_amsdusupported */

#ifdef ATH_SWRETRY
    ath_set_swretrystate,       /* set_swretrystate */
    ath_handlepspoll,           /* ath_handle_pspoll */
    ath_exist_pendingfrm_tidq,  /* get_exist_pendingfrm_tidq */
    ath_reset_paused_tid,  		/* reset_paused_tid */
#endif
#ifdef ATH_SUPPORT_UAPSD
    ath_process_uapsd_trigger,  /* process_uapsd_trigger */
    ath_tx_uapsd_depth,         /* uapsd_depth */
#endif
#ifndef REMOVE_PKT_LOG
    ath_start_pktlog,           /* pktlog_start */
    ath_read_pktlog_hdr,        /* pktlog_read_hdr */
    ath_read_pktlog_buf,        /* pktlog_read_buf */
#endif
#if ATH_SUPPORT_IQUE
    ath_setacparams,            /* ath_set_acparams */
    ath_setrtparams,            /* ath_set_rtparams */
    ath_getiqueconfig,          /* ath_get_iqueconfig */
    ath_sethbrparams,            /* ath_set_hbrparams */
#endif
    ath_do_pwrworkaround,       /* do_pwr_workaround */
    ath_get_txqproperty,        /* get_txqproperty */
    ath_set_txqproperty,        /* set_txqproperty */
    ath_get_hwrevs,                /* get_hwrevs */
    ath_rc_rate_maprix,         /* rc_rate_maprix */
    ath_rc_rate_set_mcast_rate, /* rc_rate_set_mcast_rate */
    ath_setdefantenna,          /* set_defaultantenna */
    ath_set_diversity,          /* set_diversity */
    ath_set_rx_chainmask,       /* set_rx_chainmask */
    ath_set_tx_chainmask,       /* set_tx_chainmask */
    ath_set_rx_chainmasklegacy, /* set_rx_chainmasklegacy */
    ath_set_tx_chainmasklegacy, /* set_tx_chainmasklegacy */
    ath_get_maxtxpower,         /* get_maxtxpower */
    ath_read_from_eeprom,       /* ath_eeprom_read */
    ath_pkt_log_text,           /* log_text_fmt */
    ath_fixed_log_text,         /* log_text */
#ifdef AR_DEBUG
    ath_tx_node_queue_stats,     /* node_queue_stats */
#endif
    ath_rate_newstate_set,      /* rate_newstate_set*/
    ath_rate_findrc,            /* rate_findrc*/
#ifdef ATH_SUPPORT_HTC
    ath_wmi_get_target_stats,   /* ath_wmi_get_target_stats */
    ath_wmi_beacon,             /* ath_wmi_beacon */
    ath_wmi_bmiss,              /* ath_wmi_bmiss */
    ath_wmi_add_vap,            /* ath_wmi_add_vap */
    ath_wmi_add_node,           /* ath_wmi_add_node */
    ath_wmi_delete_node,        /* ath_wmi_delete_node */
    ath_wmi_update_node,        /* ath_wmi_update_node */
#if ENCAP_OFFLOAD    
    ath_wmi_update_vap,         /* ath_wmi_update_vap */ 
#endif    
    ath_htc_rx,                 /* ath_htc_rx */
    ath_wmi_ic_update_target,   /* ath_wmi_ic_update_target */
    ath_wmi_delete_vap,         /* ath_wmi_delete_vap */
    ath_schedule_wmm_update,    /* ath_schedule_wmm_update */
#ifdef ATH_HTC_TX_SCHED
    ath_htc_tx_schedule,
    ath_htc_uapsd_credit_update,
#endif
    ath_wmi_generc_timer,
    ath_wmi_pause_ctrl,
#endif
    ath_printreg,
    ath_getmfpsupport,          /* ath_get_mfpsupport */
    ath_setrxtimeout,           /* ath_set_rxtimeout */




#ifdef ATH_USB
    ath_usb_suspend,
    ath_usb_rx_cleanup,    
#endif    
#if ATH_SUPPORT_WIRESHARK
    ath_fill_radiotap_hdr,      /* ath_fill_radiotap_hdr */
    ath_monitor_filter_phyerr,  /* ath_monitor_filter_phyerr */
#endif    
#ifdef ATH_BT_COEX
    ath_bt_coex_ps_complete,    /* bt_coex_ps_complete */
    ath_bt_coex_get_info,       /* bt_coex_get_info */
    ath_bt_coex_event,          /* bt_coex_event */
#endif
    /* Green AP functions */
    ath_green_ap_dev_set_enable,
    ath_green_ap_dev_get_enable,
    ath_green_ap_dev_set_transition_time,
    ath_green_ap_dev_get_transition_time,    
    ath_green_ap_dev_set_on_time,
    ath_green_ap_dev_get_on_time,
    /* Noise floor function */
    ath_dev_noisefloor,
#if ATH_SUPPORT_5G_EACS
    ath_dev_get_chan_stats,
#endif
    ath_spectral_control,
#if ATH_SUPPORT_SPECTRAL
    ath_dev_get_ctldutycycle,
    ath_dev_get_extdutycycle,
    ath_dev_start_spectral_scan,
    ath_dev_stop_spectral_scan,
#endif

#ifdef ATH_GEN_TIMER
    ath_tsf_timer_alloc,        /* ath_tsf_timer_alloc */
    ath_tsf_timer_free,         /* ath_tsf_timer_free */
    ath_tsf_timer_start,        /* ath_tsf_timer_start */
    ath_tsf_timer_stop,         /* ath_tsf_timer_stop */
#endif  //ifdef ATH_GEN_TIMER

#if  ATH_SUPPORT_AOW
    ath_hwtimer_start,          /* ath_start_aow_timer */
    ath_hwtimer_stop,           /* ath_stop_aow_timer */
    ath_gpio12_toggle,          /* GPIO toggle */ 
    ath_aow_control,            /* Sock interface AoW */
    ath_get_aow_seqno,          /* Get the AoW Sequence number */
    ath_aow_proc_timer_start,   /* Start AoW proc timer */
    ath_aow_proc_timer_stop,    /* Stop AoW proc timer */
#endif  /* ATH_SUPPORT_AOW */
#ifdef ATH_RFKILL
	ath_enable_RFKillPolling,   /* enableRFKillPolling */
#endif	//ifdef ATH_RFKILL
#if ATH_SLOW_ANT_DIV
    ath_slow_ant_div_suspend,
    ath_slow_ant_div_resume,
#endif  //if ATH_SLOW_ANT_DIV

    ath_reg_notify_tx_bcn,      /* ath_reg_notify_tx_bcn */
    ath_dereg_notify_tx_bcn,    /* ath_dereg_notify_tx_bcn */
    ath_reg_vap_info_notify,    /* ath_reg_vap_info_notify */
    ath_vap_info_update_notify, /* ath_vap_info_update_notify */
    ath_dereg_vap_info_notify,  /* ath_dereg_vap_info_notify */
    ath_vap_info_get,           /* ath_vap_info_get */
    ath_vap_pause_control,      /* ath_vap_pause_control */
    ath_node_pause_control,     /* ath_node_pause_control */
    ath_get_goodput,
#ifdef ATH_SUPPORT_TxBF
    ath_get_txbfcaps,           /* get_txbf_caps */
#ifdef TXBF_TODO
    ath_rx_get_pos2_data,		/* rx_get_pos2_data*///For TxBF RC:Pass pos2 Data
    ath_rx_txbfrcupdate,		/* rx_TxBFRCUpdate*///For TxBF RC: RCUpdate 
    ath_ap_save_join_mac,	    /*AP_save_join_mac*///For TxBF RC: save join sta mac-addr
    ath_start_imbf_cal,
#endif
    ath_txbf_alloc_key,
    ath_txbf_set_key,
    ath_txbf_set_hw_cvtimeout,
    ath_txbf_print_cv_cache,
    ath_txbf_set_rpt_received,
#endif
    ath_set_rifs,
#if defined(MAGPIE_HIF_GMAC) || defined(MAGPIE_HIF_USB)
    ath_wmi_node_getrate,
#endif  
#if UMAC_SUPPORT_VI_DBG
    ath_set_vi_dbg_restart,     /* ath_set_vi_dbg_restart */
    ath_set_vi_dbg_log,         /* ath_set_vi_dbg_log */
#endif
#if ATH_SUPPORT_FLOWMAC_MODULE
    ath_netif_stop_queue,
    ath_netif_wake_queue,
    ath_netif_set_wdtimeout,
    ath_netif_flowmac_pause,
#endif
#if ATH_SUPPORT_IBSS_DFS
    ath_ibss_beacon_update_start,         /* ath_ibss_beacon_update_start */
    ath_ibss_beacon_update_stop,          /* ath_ibss_beacon_update_stop */
#endif
    ath_get_tid_seqno,          /* ath_get_tid_seqno */
    ath_set_bssid_mask,         /* ath_set_bssid_mask */
#if UMAC_SUPPORT_SMARTANTENNA
    ath_smartant_prepare_rateset,       /* smartant_prepare_rateset */
    ath_get_txbuf_free,                 /* get_txbuf_free */
    ath_get_txbuf_used,                 /* get_txbuf_used */
    ath_get_smartantenna_enable,        /* smart antenna enable staus to upper layers */
    ath_get_smartant_ratestats,         /* rate stats used for transmit in smart antenna retrain intervel*/
#endif    
#if UMAC_SUPPORT_INTERNALANTENNA    
    ath_set_selected_smantennas,        /* set_selected_smantennas */
#endif    
};

int ath_dev_attach(u_int16_t            devid,
                   void                 *bus_context,
                   ieee80211_handle_t   ieee,
                   struct ieee80211_ops *ieee_ops,
                   osdev_t              osdev,
                   ath_dev_t            *dev,
                   struct ath_ops       **ops,
                   asf_amem_instance_handle amem_handle,
                   struct ath_reg_parm  *ath_conf_parm,
                   struct hal_reg_parm  *hal_conf_parm)
{
#define    N(a)    (sizeof(a)/sizeof(a[0]))
    struct ath_softc *sc = NULL;
    struct ath_hal *ah = NULL;
    HAL_STATUS status;
    int error = 0, i;
    u_int32_t chainmask;
    u_int32_t rd;
    u_int32_t stbcsupport;
    u_int32_t ldpcsupport;
    u_int32_t pcieLcrOffset = 0;
    u_int32_t enableapm;
    u_int8_t empty_macaddr[IEEE80211_ADDR_LEN] = {0, 0, 0, 0, 0, 0};
    u_int32_t cal_interval, regval = 0;
    u_int8_t splitmic_required = 0;

    sc = (struct ath_softc *)OS_MALLOC(osdev, sizeof(struct ath_softc), GFP_KERNEL);
    if (sc == NULL) {
        printk("%s: unable to allocate device object.\n", __func__);
        error = -ENOMEM;
        goto bad;
    }
    OS_MEMZERO(sc, sizeof(struct ath_softc));

    *dev = (ath_dev_t)sc;

    /*
     * ath_ops is stored per device instance because EDMA devices
     * have EDMA specific functions.
     */
    sc->sc_ath_ops = ath_ar_ops;
    *ops = &sc->sc_ath_ops;

    ATH_HTC_SET_CALLBACK_TABLE(sc->sc_ath_ops);     /* Update HTC functions */

    sc->sc_osdev = osdev;
    sc->sc_ieee = ieee;
    sc->sc_ieee_ops = ieee_ops;
    sc->sc_reg_parm = *ath_conf_parm;
    sc->sc_mask2chains = ath_numchains;
    
    /* XXX: hardware will not be ready until ath_open() being called */
    sc->sc_invalid = 1;
    ATH_USB_SET_INVALID(sc, 1);

    sc->sc_debug = DBG_DEFAULT;

    sc->sc_limit_legacy_frames = 25;
    DPRINTF(sc, ATH_DEBUG_ANY, "%s: devid 0x%x\n", __func__, devid);

    ATH_HTC_SET_HANDLE(sc, osdev);
    ATH_HTC_SET_EPID(sc, osdev);

    /*
     * Cache line size is used to size and align various
     * structures used to communicate with the hardware.
     */
    ATH_GET_CACHELINE_SIZE(sc);

    //ATH_LOCK_INIT(sc);
    ATH_VAP_PAUSE_LOCK_INIT(sc);
    ATH_PS_LOCK_INIT(sc);
    ATH_PS_INIT_LOCK_INIT(sc);
    ATH_RESET_LOCK_INIT(sc);
    ATH_RESET_SERIALIZE_LOCK_INIT(sc);

    ATH_INTR_STATUS_LOCK_INIT(sc);
    

#if ATH_RESET_SERIAL
    ATH_RESET_INIT_MUTEX(sc);
    ATH_RESET_HAL_INIT_MUTEX(sc);
     for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
         ATH_RESET_LOCK_Q_INIT(sc, i);
         TAILQ_INIT(&(sc->sc_queued_txbuf[i].txbuf_head));
     }
#endif

#ifdef ATH_SWRETRY
    ATH_NODETABLE_LOCK_INIT(sc);
    LIST_INIT(&sc->sc_nt);
#endif

    /*
     * Attach the hal and verify ABI compatibility by checking
     * the hal's ABI signature against the one the driver was
     * compiled with.  A mismatch indicates the driver was
     * built with an ah.h that does not correspond to the hal
     * module loaded in the kernel.
     */
    ah = _ath_hal_attach(devid, osdev, sc, bus_context, hal_conf_parm,
                         amem_handle, &halCallbackTable, &status);
    if (ah == NULL) {
        printk("%s: unable to attach hardware; HAL status %u\n",
               __func__, status);
        error = -ENXIO;
        goto bad;
    }
    if (ah->ah_abi != HAL_ABI_VERSION) {
        printk("%s: HAL ABI msmatch; "
               "driver expects 0x%x, HAL reports 0x%x\n",
               __func__, HAL_ABI_VERSION, ah->ah_abi);
        error = -ENXIO;        /* XXX */
        goto bad;
    }
    sc->sc_ah = ah;

    /* save the mac address from EEPROM */
    ath_hal_getmac(ah, sc->sc_my_hwaddr);
    if (OS_MEMCMP(sc->sc_reg_parm.networkAddress, empty_macaddr, IEEE80211_ADDR_LEN) &&
        !IEEE80211_IS_MULTICAST(sc->sc_reg_parm.networkAddress) &&
        !IEEE80211_IS_BROADCAST(sc->sc_reg_parm.networkAddress) &&
        (sc->sc_reg_parm.networkAddress[0] & 0x02)) {
        /* change hal mac address */
        ath_hal_setmac(sc->sc_ah, sc->sc_reg_parm.networkAddress);
    }

    /*
     * Attach the enhanced DMA module.
     */
    ath_edma_attach(sc, ops);

    /*
     * Query the HAL for tx  and rx descriptor lengths
     */
    ath_hal_gettxdesclen(sc->sc_ah, &sc->sc_txdesclen);
    ath_hal_gettxstatuslen(sc->sc_ah, &sc->sc_txstatuslen);
    ath_hal_getrxstatuslen(sc->sc_ah, &sc->sc_rxstatuslen);

    /*
     * Check TxBF support
     */
#ifdef ATH_SUPPORT_TxBF
     sc->sc_txbfsupport = ath_hal_hastxbf(ah);
#endif
    /*
     * Get the chipset-specific aggr limit.
     */
    ath_hal_getrtsaggrlimit(sc->sc_ah, &sc->sc_rtsaggrlimit);

    /*
     * Check if the device has hardware counters for PHY
     * errors.  If so we need to enable the MIB interrupt
     * so we can act on stat triggers.
     */
    if (ath_hal_hwphycounters(ah)) {
        sc->sc_needmib = 1;
    }

    /*
     * Get the hardware key cache size.
     */
    sc->sc_keymax = ath_hal_keycachesize(ah);
    if (sc->sc_keymax > ATH_KEYMAX) {
        printk("%s: Warning, using only %u entries in %u key cache\n",
               __func__, ATH_KEYMAX, sc->sc_keymax);
        sc->sc_keymax = ATH_KEYMAX;
    }

    /*
     * Get the max number of buffers that can be stored in a TxD.
     */
    ath_hal_getnumtxmaps(ah, &sc->sc_num_txmaps);

    // single key cache writes
    // Someday Soon we should probe the chipset id to see if it is
    // one of the broken ones, then turn on the singleWriteKC, which
    // should be just a hal property, IMNSHO.
    // ath_hal_set_singleWriteKC(ah, TRUE);

    /*
     * Reset the key cache since some parts do not
     * reset the contents on initial power up.
     */
    for (i = 0; i < sc->sc_keymax; i++) {
        ath_hal_keyreset(ah, (u_int16_t)i);
    }
    /*
     * Mark key cache slots associated with global keys
     * as in use.  If we knew TKIP was not to be used we
     * could leave the +32, +64, and +32+64 slots free.
     * XXX only for splitmic.
     */
    if(ath_hal_ciphersupported(ah,HAL_CIPHER_MIC)) {
            splitmic_required = ath_hal_tkipsplit(ah);
    }
    for (i = 0; i < IEEE80211_WEP_NKID; i++) {
        setbit(sc->sc_keymap, i);
        if(splitmic_required) {
            setbit(sc->sc_keymap, i+32);
            setbit(sc->sc_keymap, i+64);
            setbit(sc->sc_keymap, i+32+64);
        }
    }

    /*
     * Initialize timer module
     */
    ath_initialize_timer_module(osdev);

    /*
     * Collect the channel list using the default country
     * code and including outdoor channels.  The 802.11 layer
     * is resposible for filtering this list based on settings
     * like the phy mode.
     */
    ath_hal_getregdomain(ah, &rd);

    if (sc->sc_reg_parm.regdmn) {
        ath_hal_setregdomain(ah, sc->sc_reg_parm.regdmn, &status);
    }

    sc->sc_config.ath_countrycode = CTRY_DEFAULT;
    if (countrycode != -1) {
        sc->sc_config.ath_countrycode = countrycode;
    }

    /*
     * Only allow set country code for debug domain.
     */
    if ((rd == 0) && sc->sc_reg_parm.countryName[0]) {
        sc->sc_config.ath_countrycode = findCountryCode((u_int8_t *)sc->sc_reg_parm.countryName);
    }

    sc->sc_config.ath_xchanmode = AH_TRUE;
    if (xchanmode != -1) {
        sc->sc_config.ath_xchanmode = xchanmode;
    } else {
        sc->sc_config.ath_xchanmode = sc->sc_reg_parm.extendedChanMode;
    }
    error = ath_getchannels(sc, sc->sc_config.ath_countrycode,
                            ath_outdoor, sc->sc_config.ath_xchanmode);
    if (error != 0) {
        goto bad;
    }

    /* default to STA mode */
    sc->sc_opmode = HAL_M_STA;

    if (HAL_OK == ath_hal_getcapability(ah, HAL_CAP_ENTERPRISE_MODE,
                                        0, &regval))
    {
        sc->sc_ent_min_pkt_size_enable = 
           (regval & AH_ENT_MIN_PKT_SIZE_DISABLE) ? 0 : 1;
        sc->sc_ent_spectral_mask = 
           (regval & AH_ENT_SPECTRAL_PRECISION) >> AH_ENT_SPECTRAL_PRECISION_S;
        sc->sc_ent_rtscts_delim_war = 
           (regval & AH_ENT_RTSCTS_DELIM_WAR) ? 1 : 0;
    }

    /*
     * Initialize rate tables.
     */
    ath_hal_get_max_weptkip_ht20tx_rateKbps(sc->sc_ah, &sc->sc_max_wep_tkip_ht20_tx_rateKbps);
    ath_hal_get_max_weptkip_ht40tx_rateKbps(sc->sc_ah, &sc->sc_max_wep_tkip_ht40_tx_rateKbps);
    ath_hal_get_max_weptkip_ht20rx_rateKbps(sc->sc_ah, &sc->sc_max_wep_tkip_ht20_rx_rateKbps);
    ath_hal_get_max_weptkip_ht40rx_rateKbps(sc->sc_ah, &sc->sc_max_wep_tkip_ht40_rx_rateKbps);
    ath_rate_table_init();

    /*
     * Setup rate tables for all potential media types.
     */
#ifndef ATH_NO_5G_SUPPORT
    ath_rate_setup(sc, WIRELESS_MODE_11a);
#endif
    ath_rate_setup(sc, WIRELESS_MODE_11b);
    ath_rate_setup(sc, WIRELESS_MODE_11g);
#ifndef ATH_NO_5G_SUPPORT
    ath_rate_setup(sc, WIRELESS_MODE_108a);
#endif
    ath_rate_setup(sc, WIRELESS_MODE_108g);
#ifndef ATH_NO_5G_SUPPORT
    ath_rate_setup(sc, WIRELESS_MODE_11NA_HT20);
#endif
    ath_rate_setup(sc, WIRELESS_MODE_11NG_HT20);
#ifndef ATH_NO_5G_SUPPORT
    ath_rate_setup(sc, WIRELESS_MODE_11NA_HT40PLUS);
    ath_rate_setup(sc, WIRELESS_MODE_11NA_HT40MINUS);
#endif
    ath_rate_setup(sc, WIRELESS_MODE_11NG_HT40PLUS);
    ath_rate_setup(sc, WIRELESS_MODE_11NG_HT40MINUS);

    /* Setup for half/quarter rates */
    ath_setup_subrates(sc);

    /* NB: setup here so ath_rate_update is happy */
#ifndef ATH_NO_5G_SUPPORT
    ath_setcurmode(sc, WIRELESS_MODE_11a);
#else
    ath_setcurmode(sc, WIRELESS_MODE_11g);
#endif

    /* This initializes Target Hardware Q's; It is independent of Host endpoint initialization */
    ath_htc_initialize_target_q(sc);

    /* Initialize the tasket for rx */
    ATH_USB_TQUEUE_INIT(sc, ath_htc_tasklet);
    ATH_HTC_TXTQUEUE_INIT(sc, ath_htc_tx_tasklet);
    ATH_HTC_UAPSD_CREDITUPDATE_INIT(sc, ath_htc_uapsd_creditupdate_tasklet);
#ifdef ATH_USB
    /* Initialize thread */
    ath_usb_create_thread(sc);
#endif

    /*
     * Allocate hardware transmit queues: one queue for
     * beacon frames and one data queue for each QoS
     * priority.  Note that the hal handles reseting
     * these queues at the needed time.
     *
     * XXX PS-Poll
     */
    sc->sc_bhalq = ath_beaconq_setup(sc, ah);
    if (sc->sc_bhalq == (u_int) -1) {
        printk("unable to setup a beacon xmit queue!\n");
        error = -EIO;
        goto bad2;
    }
    ATH_HTC_SET_BCNQNUM(sc, sc->sc_bhalq);

    sc->sc_cabq = ath_txq_setup(sc, HAL_TX_QUEUE_CAB, 0);
    if (sc->sc_cabq == NULL) {
        printk("unable to setup CAB xmit queue!\n");
        error = -EIO;
        goto bad2;
    }
#if ATH_BEACON_DEFERRED_PROC
    sc->sc_cabq->irq_shared = 0;
#else
    sc->sc_cabq->irq_shared = 1;
#endif
    
    sc->sc_config.cabqReadytime = ATH_CABQ_READY_TIME;

    ath_cabq_update(sc);

#if ATH_SUPPORT_CFEND
    /* at this point of time, we do not know whether device works in AP mode
     * or in any other modes, queue configuration should not depend on mode
    * Also make sure that we enable this only for ar5416chips
     */
   error = ath_cfendq_config(sc);
   if (error) goto bad2;
#endif
    for (i = 0; i < N(sc->sc_haltype2q); i++)
        sc->sc_haltype2q[i] = -1;

    /* NB: insure BK queue is the lowest priority h/w queue */
    if (!ATH_SET_TX(sc)) {
        error = -EIO;
        goto bad2;
    }
    
#if ATH_SUPPORT_AGGR_BURST
    if(sc->sc_reg_parm.burstEnable) {
        sc->sc_aggr_burst = true;
        sc->sc_aggr_burst_duration = sc->sc_reg_parm.burstDur;
    } else {
        sc->sc_aggr_burst = false;
        sc->sc_aggr_burst_duration = ATH_BURST_DURATION;
    }

    if (sc->sc_aggr_burst) {
        ath_txq_burst_update(sc, HAL_WME_AC_BE,
                             sc->sc_aggr_burst_duration);
    }
#endif

    ath_htc_host_txep_init(sc);

#ifdef ATH_SUPERG_COMP
    if (ath_hal_compressionsupported(ah))
        sc->sc_hascompression = 1;
#endif

#ifdef ATH_SUPERG_XR
    ath_xr_rate_setup(sc);
    sc->sc_xrpollint =  XR_DEFAULT_POLL_INTERVAL;
    sc->sc_xrpollcount = XR_DEFAULT_POLL_COUNT;
    strcpy(sc->sc_grppoll_str,XR_DEFAULT_GRPPOLL_RATE_STR);
    sc->sc_grpplq.axq_qnum=-1;
    sc->sc_xrtxq = ath_txq_setup(sc, HAL_TX_QUEUE_DATA,HAL_XR_DATA);
#endif

#if ATH_WOW
    /*
     * If hardware supports wow, allocate memory for wow information.
     */
    sc->sc_hasWow = ath_hal_hasWow(ah);
    if(sc->sc_hasWow)
    {
        sc->sc_wowInfo = (struct wow_info *)OS_MALLOC(sc->sc_osdev, sizeof(struct wow_info), GFP_KERNEL);
        if (sc->sc_wowInfo != NULL) {
            OS_MEMZERO(sc->sc_wowInfo, sizeof(struct wow_info));
        } else {
            goto bad2;
        }
    }
    sc->sc_wow_sleep = 0;
#endif

#if ATH_SUPPORT_HT
    if (ath_hal_htsupported(ah))
        sc->sc_hashtsupport=1;

    if (ath_hal_ht20sgisupported(ah))
        sc->sc_ht20sgisupport=1;
    else
        sc->sc_ht20sgisupport=0;
#endif

    if(sc->sc_reg_parm.stbcEnable) {
        if (ath_hal_txstbcsupport(ah, &stbcsupport))
            sc->sc_txstbcsupport = stbcsupport;

        if (ath_hal_rxstbcsupport(ah, &stbcsupport))
            sc->sc_rxstbcsupport = stbcsupport;
    }

    if (sc->sc_reg_parm.ldpcEnable) {
        if (ath_hal_ldpcsupport(ah, &ldpcsupport)) {
            sc->sc_ldpcsupport = ldpcsupport;
        }
    }

    if (ath_hal_hasrxtxabortsupport(ah)) {
        /* TODO: allow this to be runtime configurable */
        sc->sc_fastabortenabled = 1;
    }

    sc->sc_setdefantenna = ath_setdefantenna;
    sc->sc_rc = ath_rate_attach(ah);
    if (sc->sc_rc == NULL) {
        error = EIO;
        goto bad2;
    }

    if (!ath_hal_getanipollinterval(ah, &cal_interval)) {
        cal_interval = ATH_SHORT_CALINTERVAL;
    }
#if ATH_LOW_PRIORITY_CALIBRATE
    ath_initialize_timer(sc->sc_osdev, &sc->sc_cal_ch, cal_interval, ath_calibrate_workitem, sc);
#else
    ath_initialize_timer(sc->sc_osdev, &sc->sc_cal_ch, cal_interval, ath_calibrate, sc);
#endif
    ath_initialize_timer(sc->sc_osdev, &sc->sc_up_beacon, ATH_UP_BEACON_INTVAL, ath_up_beacon, sc);

#if ATH_TRAFFIC_FAST_RECOVER
    if (ath_hal_getcapability(sc->sc_ah, HAL_CAP_TRAFFIC_FAST_RECOVER, 0, NULL) == HAL_OK) {
        ath_initialize_timer(sc->sc_osdev, &sc->sc_traffic_fast_recover_timer, 100, ath_traffic_fast_recover, sc);
    }
#endif

#ifdef ATH_SUPPORT_TxBF
#ifdef TXBF_TODO
    if (sc->sc_txbfsupport == AH_TRUE) {
	   //DPRINTF(sc, ATH_DEBUG_ANY,"==>%s----ath_initialize_timer of BF---\n ",__FUNCTION__);
	   ath_initialize_timer(sc->sc_osdev, &sc->sc_imbf_cal_short, 10*1000, ath_sc_start_imbf_cal, sc);
	}
	sc->only_bf_cal_allow = 0;
	sc->radio_cal_process = 0;
#endif
    if (sc->sc_txbfsupport == AH_TRUE) {
        ath_initialize_timer(sc->sc_osdev, &sc->sc_selfgen_timer, 
            LOWEST_RATE_RESET_PERIOD, ath_sc_reset_lowest_txrate, sc);
	    ath_set_timer_period(&sc->sc_selfgen_timer, LOWEST_RATE_RESET_PERIOD);
        ath_start_timer(&sc->sc_selfgen_timer);
	}
#endif

#ifdef ATH_TX_INACT_TIMER
    OS_INIT_TIMER(sc->sc_osdev, &sc->sc_inact, ath_inact, sc);
    sc->sc_tx_inact = 0;
    sc->sc_max_inact = 5;
    OS_SET_TIMER(&sc->sc_inact, 60000 /* ms */);
#endif

#if ATH_HW_TXQ_STUCK_WAR
    if(sc->sc_enhanceddmasupport) {
        OS_INIT_TIMER(sc->sc_osdev, &sc->sc_qstuck, ath_txq_stuck_checker, sc);
        OS_SET_TIMER(&sc->sc_qstuck, QSTUCK_TIMER_VAL);
    }
#endif

#ifdef ATH_SUPPORT_DFS
    sc->sc_dfs = NULL;
    if (dfs_attach(sc)) {
        error = EIO;
        printk("%s DFS attach failed\n", __func__);
        goto bad2;
    }
    /*Initialize DFS related timers*/
    OS_INIT_TIMER(sc->sc_osdev, &sc->sc_dfs->sc_dfswaittimer, ath_check_dfs_clear, sc);
    OS_INIT_TIMER(sc->sc_osdev, &sc->sc_dfs->sc_dfstesttimer, ath_dfs_test_return, sc);
    OS_INIT_TIMER(sc->sc_osdev, &sc->sc_dfs->sc_dfs_task_timer, ath_radar_task, sc);
    OS_INIT_TIMER(sc->sc_osdev, &sc->sc_dfs->sc_dfs_war_timer, ath_dfs_war_task, sc);
    /* Initialize the Sowl TX hang WAR state */
    OS_INIT_TIMER(sc->sc_osdev, &sc->sc_dfs_hang.hang_war_timer, ath_dfs_hang_war, sc);
    sc->sc_dfs_hang.hang_war_timeout = (ATH_DFS_HANG_WAR_MIN_TIMEOUT_MS/2);
    sc->sc_dfs_hang.total_stuck=0;
    sc->sc_dfs_hang.last_dfs_hang_war_tstamp = 0;
    sc->sc_dfs_hang.hang_war_ht40count = 0;
    sc->sc_dfs_hang.hang_war_ht20count = 0;
    sc->sc_dfs_hang.hang_war_activated = 0;

#endif

#ifdef MAGPIE_HIF_GMAC
    OS_INIT_TIMER(sc->sc_osdev, &host_seek_credit_timer, ath_host_seek_credit_timer, (void *)sc);
#endif

#ifdef ATH_TX99_DIAG
    sc->sc_tx99 = NULL;
    if (tx99_attach(sc)) {
        error = EIO;
        printk("%s TX99 init failed\n", __func__);
    }
#endif

#if ATH_SUPPORT_SPECTRAL
    sc->sc_spectral = NULL;
    if ((ath_conf_parm->spectralEnable) && !spectral_attach(sc)) {
        error = EIO;
        printk("%s SPECTRAL attach failed, HW may not be spectral capable\n", __func__);
    }
#endif

#ifdef ATH_SUPERG_DYNTURBO
#ifdef notyet
    OS_INIT_TIMER(sc->sc_osdev, &sc->sc_dturbo_switch_mode, ath_turbo_switch_mode, sc);
#endif    
#endif

    /* attach the green_ap feature */
    sc->sc_green_ap = NULL;
    if(ath_hal_singleAntPwrSavePossible(ah)) {
        spin_lock_init(&sc->green_ap_ps_lock);
        if(ath_green_ap_attach(sc)) goto bad2;
    }
    // initialize LED module
    ath_led_initialize_control(sc, sc->sc_osdev, sc->sc_ah,
                               &sc->sc_led_control, &sc->sc_reg_parm,
                               !ath_hal_hasPciePwrsave(sc->sc_ah));

    sc->sc_sw_phystate = sc->sc_hw_phystate = 1;    /* default hw/sw phy state to be on */

#ifdef ATH_SUPERG_FF
    sc->sc_fftxqmin = ATH_FF_TXQMIN;
#endif

    if (ath_hal_ciphersupported(ah, HAL_CIPHER_TKIP)) {
        /*
         * Whether we should enable h/w TKIP MIC.
         * XXX: if we don't support WME TKIP MIC, then we wouldn't report WMM capable,
         * so it's always safe to turn on TKIP MIC in this case.
         */
        ath_hal_setcapability(sc->sc_ah, HAL_CAP_TKIP_MIC, 0, 1, NULL);
    }
    sc->sc_hasclrkey = ath_hal_ciphersupported(ah, HAL_CIPHER_CLR);

    /* turn on mcast key search if possible */
    if (ath_hal_hasmcastkeysearch(ah)) {
        (void) ath_hal_setmcastkeysearch(ah, 1);
    }

    /*
     * TPC support can be done either with a global cap or
     * per-packet support.  The latter is not available on
     * all parts.  We're a bit pedantic here as all parts
     * support a global cap.
     */
    sc->sc_hastpc = ath_hal_hastpc(ah);

    sc->sc_config.txpowlimit2G = ATH_TXPOWER_MAX_2G;
    sc->sc_config.txpowlimit5G = ATH_TXPOWER_MAX_5G;
    ATH_TXPOWER_INIT(sc);
    sc->sc_config.txpowlimit_override = sc->sc_reg_parm.overRideTxPower;

    /* 11n Capabilities */
    if (sc->sc_hashtsupport) {
        sc->sc_config.ampdu_limit = sc->sc_reg_parm.aggrLimit;
        sc->sc_config.ampdu_subframes = sc->sc_reg_parm.aggrSubframes;

        sc->sc_txaggr = (sc->sc_reg_parm.txAggrEnable) ? 1 : 0;
        sc->sc_rxaggr = (sc->sc_reg_parm.rxAggrEnable) ? 1 : 0;

        sc->sc_txamsdu = (sc->sc_reg_parm.txAmsduEnable) ? 1 : 0;

        if (ath_hal_hasrifstx(ah)) {
            sc->sc_txrifs = (sc->sc_reg_parm.txRifsEnable) ? 1 : 0;
            sc->sc_config.rifs_ampdu_div =
                sc->sc_reg_parm.rifsAggrDiv;
        }
#ifdef ATH_RB
        if (!ath_hal_hasrifsrx(ah)) {
            sc->sc_rxrifs_timeout = sc->sc_reg_parm.rxRifsTimeout;
            sc->sc_rxrifs_skipthresh =
                                 sc->sc_reg_parm.rxRifsSkipThresh;
            sc->sc_rxrifs = sc->sc_reg_parm.rxRifsEnable;
            sc->sc_do_rb_war = 1;
            ath_rb_init(sc);
        }
#endif
    }
    /*set the customer cts check flg to zero, 
    to use the default CTS rate for the dirver.
    if customer use the iwpriv command to reset 
    the cts rate, it will be set to 1 to avoid 
    interface down & up to re-set the customer
    modify*/
    sc->sc_cus_cts_set = 0;
    /*
     * Check for misc other capabilities.
     */
    sc->sc_hasbmask = ath_hal_hasbssidmask(ah);
    sc->sc_hastsfadd = ath_hal_hastsfadjust(ah);

    if (sc->sc_reg_parm.txChainMask) {
        chainmask = sc->sc_reg_parm.txChainMask;
    } else {
        if (!ath_hal_gettxchainmask(ah, &chainmask))
            chainmask = 0;
    }

#ifdef ATH_CHAINMASK_SELECT
    /*
     * If we cannot transmit on three chains, prevent chain mask
     * selection logic from switching between 2x3 and 3x3 chain
     * masks based on RSSI.
     * Even when we can transmit on three chains, our default
     * behavior is to use two transmit chains only.
     */
    if ((chainmask == ATH_CHAINMASK_SEL_3X3) &&
        (ath_hal_getcapability(ah, HAL_CAP_TS, 0, NULL) != HAL_OK))
    {
        sc->sc_no_tx_3_chains = AH_TRUE;
        chainmask = ATH_CHAINMASK_SEL_2X3;
    } else {
        sc->sc_no_tx_3_chains = AH_FALSE;
    }
    sc->sc_config.chainmask_sel = sc->sc_no_tx_3_chains;
#endif

    sc->sc_config.txchainmask = chainmask;

    if (sc->sc_reg_parm.rxChainMask) {
        chainmask = sc->sc_reg_parm.rxChainMask;
    } else {
        if (!ath_hal_getrxchainmask(ah, &chainmask))
            chainmask = 0;
    }

    sc->sc_config.rxchainmask = chainmask;

    /* legacy chain mask */
    if (sc->sc_reg_parm.txChainMaskLegacy) {
        chainmask = sc->sc_reg_parm.txChainMaskLegacy;
    } else {
        if (!ath_hal_gettxchainmask(ah, &chainmask))
            chainmask = 0;
    }
    sc->sc_config.txchainmasklegacy = chainmask;

    if (sc->sc_reg_parm.rxChainMaskLegacy) {
        chainmask = sc->sc_reg_parm.rxChainMaskLegacy;
    } else {
        if (!ath_hal_getrxchainmask(ah, &chainmask))
            chainmask = 0;
    }
    sc->sc_config.rxchainmasklegacy = chainmask;
#if ATH_SUPPORT_DYN_TX_CHAINMASK
    /* turn offDTCS by default */
    sc->sc_dyn_txchainmask = 0;
#endif /* ATH_SUPPORT_DYN_TX_CHAINMASK */

    /*
     * Configuration for rx chain detection
     */
    sc->sc_rxchaindetect_ref = sc->sc_reg_parm.rxChainDetectRef;
    sc->sc_rxchaindetect_thresh5GHz = sc->sc_reg_parm.rxChainDetectThreshA;
    sc->sc_rxchaindetect_thresh2GHz = sc->sc_reg_parm.rxChainDetectThreshG;
    sc->sc_rxchaindetect_delta5GHz = sc->sc_reg_parm.rxChainDetectDeltaA;
    sc->sc_rxchaindetect_delta2GHz = sc->sc_reg_parm.rxChainDetectDeltaG;

    /*
     * Adaptive Power Management:
     * Some 3 stream chips exceed the PCIe power requirements.
     * This workaround will reduce power consumption by using 2 tx chains
     * for 1 and 2 stream rates (5 GHz only)
     *
     */
    if (ath_hal_getenableapm(ah, &enableapm)) {
         sc->sc_enableapm = enableapm;
    }

    /* Initialize ForcePPM module */
    ath_force_ppm_attach(&sc->sc_ppm_info,
                                    sc,
                                    sc->sc_osdev,
                                    ah,
                                    &sc->sc_reg_parm);

    /*
     * Query the hal about antenna support
     * Enable rx fast diversity if hal has support
     */
    if (ath_hal_hasdiversity(ah)) {
        sc->sc_hasdiversity = 1;
        ath_hal_setdiversity(ah, AH_TRUE);
        sc->sc_diversity = 1;
    } else {
        sc->sc_hasdiversity = 0;
        sc->sc_diversity=0;
        ath_hal_setdiversity(ah, AH_FALSE);
    }
    sc->sc_defant = ath_hal_getdefantenna(ah);

    /*
     * Not all chips have the VEOL support we want to
     * use with IBSS beacons; check here for it.
     */
    sc->sc_hasveol = ath_hal_hasveol(ah);

    ath_hal_getmac(ah, sc->sc_myaddr);
    if (sc->sc_hasbmask) {
        ath_hal_getbssidmask(ah, sc->sc_bssidmask);
        ATH_SET_VAP_BSSID_MASK(sc->sc_bssidmask);
        ath_hal_setbssidmask(ah, sc->sc_bssidmask);
    }

#ifdef ATH_RFKILL
    if (ath_hal_hasrfkill(ah) && !sc->sc_reg_parm.disableRFKill) {
        sc->sc_hasrfkill = 1;
        ath_rfkill_attach(sc);
    }
#endif

    sc->sc_wpsgpiointr = 0;
    sc->sc_wpsbuttonpushed = 0;
    if (sc->sc_reg_parm.wpsButtonGpio && ath_hal_haswpsbutton(ah)) {
        /* Set corresponding line as input as older versions may have had it as output (bug 37281) */
        ath_hal_gpioCfgInput(ah, sc->sc_reg_parm.wpsButtonGpio);
        ath_hal_gpioSetIntr(ah, sc->sc_reg_parm.wpsButtonGpio, HAL_GPIO_INTR_LOW);
        sc->sc_wpsgpiointr = 1;
    }

    sc->sc_hasautosleep = ath_hal_hasautosleep(ah);
    sc->sc_waitbeacon   = 0;

#ifdef ATH_GEN_TIMER
    if (ath_hal_hasgentimer(ah)) {
        sc->sc_hasgentimer = 1;
        ath_gen_timer_attach(sc);
    }
#ifdef ATH_C3_WAR
    /* Work around for low PCIe bus utilization when CPU is in C3 state */
    if (sc->sc_reg_parm.c3WarTimerPeriod && sc->sc_hasgentimer) {
        sc->sc_c3_war_timer = ath_gen_timer_alloc(sc,
                                                  HAL_GEN_TIMER_TSF_ANY,
                                                  ath_c3_war_handler,
                                                  ath_c3_war_handler,
                                                  NULL,
                                                  sc);
        sc->c3_war_timer_active = 0;    
        atomic_set(&sc->sc_pending_tx_data_frames, 0);
        sc->sc_fifo_underrun = 0;
        spin_lock_init(&sc->sc_c3_war_lock);

    } else {
        sc->sc_c3_war_timer = NULL;
    }
#endif
#endif

#ifdef ATH_BT_COEX
    if ((sc->sc_reg_parm.bt_reg.btCoexEnable) && ath_hal_hasbtcoex(ah)) {
        ath_bt_coex_attach(sc);
    }
#endif
        
#if ATH_SUPPORT_HTC
#if ATH_K2_PS_BEACON_FILTER
    /*
     * Checkregistry to enable target beacon filter function
     */         
    if (hal_conf_parm->BeaconFilterEnable) {
        ath_wmi_beacon_filter_enable(sc);
    }
#endif
#endif

    sc->sc_slottime = HAL_SLOT_TIME_9; /* default to short slot time */
    
    /* initialize beacon slots */
    for (i = 0; i < N(sc->sc_bslot); i++)
        sc->sc_bslot[i] = ATH_IF_ID_ANY;
    
#ifndef REMOVE_PKT_LOG
    ath_pktlog_attach(sc);
#endif

    /* save MISC configurations */
#ifdef ATH_SWRETRY
    if (sc->sc_enhanceddmasupport) {
        if (sc->sc_reg_parm.numSwRetries) {
            sc->sc_swRetryEnabled = AH_TRUE;
            sc->sc_num_swretries = sc->sc_reg_parm.numSwRetries;
        }
    }
    else {
    if (sc->sc_reg_parm.numSwRetries &&
         (!(ath_hal_htsupported(ah)) || 
         (ath_hal_htsupported(ah) && !(sc->sc_txaggr)))) {
        sc->sc_swRetryEnabled = AH_TRUE;
        sc->sc_num_swretries = sc->sc_reg_parm.numSwRetries;
    }
    }
#endif
    sc->sc_config.pcieDisableAspmOnRfWake = sc->sc_reg_parm.pcieDisableAspmOnRfWake;
    sc->sc_config.pcieAspm = sc->sc_reg_parm.pcieAspm;
    ath_hal_pcieGetLcrOffset(ah, &pcieLcrOffset);
    sc->sc_config.pcieLcrOffset = (u_int8_t)pcieLcrOffset;
#if ATH_WOW    
    sc->sc_wowenable = sc->sc_reg_parm.wowEnable;
#endif

    sc->sc_config.swBeaconProcess = sc->sc_reg_parm.swBeaconProcess;
    ath_hal_get_hang_types(ah, &sc->sc_hang_war);
    sc->sc_slowAntDiv = (sc->sc_reg_parm.slowAntDivEnable) ? 1 : 0;
    sc->sc_toggle_immunity = 0;

#if ATH_SLOW_ANT_DIV
    if (sc->sc_slowAntDiv) {
        ath_slow_ant_div_init(&sc->sc_antdiv, sc, sc->sc_reg_parm.slowAntDivThreshold,
                              sc->sc_reg_parm.slowAntDivMinDwellTime,
                              sc->sc_reg_parm.slowAntDivSettleTime);
    }
#endif

#if ATH_ANT_DIV_COMB
    sc->sc_antDivComb = ath_hal_AntDivCombSupport(ah);
    if (sc->sc_antDivComb) {
        sc->sc_antDivComb = AH_TRUE;
        ath_ant_div_comb_init(&sc->sc_antcomb, sc);
    }
#endif

#if ATH_SUPPORT_IQUE
    /* Initialize Rate control and AC parameters with default values */
    sc->sc_rc_params[0].per_threshold = 55;
    sc->sc_rc_params[1].per_threshold = 35;
    sc->sc_rc_params[0].probe_interval = 50;
    sc->sc_rc_params[1].probe_interval = 50;
    sc->sc_hbr_params[WME_AC_VI].hbr_per_low = HBR_TRIGGER_PER_LOW;
    sc->sc_hbr_params[WME_AC_VO].hbr_per_low = HBR_TRIGGER_PER_LOW;
    sc->sc_hbr_per_low = HBR_TRIGGER_PER_LOW;
    sc->sc_hbr_per_high = HBR_TRIGGER_PER_HIGH;
    sc->sc_retry_duration = ATH_RC_DURATION_WITH_RETRIES;
#endif

    if (ATH_ENAB_AOW(sc)) {
        ath_aow_init(sc);
    }

    sc->sc_rxtimeout[WME_AC_VI] = ATH_RX_VI_TIMEOUT;
    sc->sc_rxtimeout[WME_AC_VO] = ATH_RX_VO_TIMEOUT;
    sc->sc_rxtimeout[WME_AC_BE] = ATH_RX_BE_TIMEOUT;
    sc->sc_rxtimeout[WME_AC_BK] = ATH_RX_BK_TIMEOUT;

#ifdef ATH_MIB_INTR_FILTER
    sc->sc_intr_filter.state            = INTR_FILTER_OFF;
    sc->sc_intr_filter.activation_count = 0;
    sc->sc_intr_filter.intr_count       = 0;
#endif

#if ATH_SUPPORT_VOWEXT
    /* Enable video features by default */
    sc->sc_vowext_flags = ATH_CAP_VOWEXT_FAIR_QUEUING |
                          ATH_CAP_VOWEXT_AGGR_SIZING  |
                          ATH_CAP_VOWEXT_BUFF_REORDER |
                          ATH_CAP_VOWEXT_RATE_CTRL_N_AGGR;
    sc->agtb_tlim = ATH_11N_AGTB_TLIM;
    sc->agtb_blim = ATH_11N_AGTB_BLIM;
    sc->agthresh  = ATH_11N_AGTB_THRESH;
#endif

#ifdef VOW_TIDSCHED
    TAILQ_INIT(&sc->tid_q);
    sc->tidsched = ATH_TIDSCHED;
    sc->acqw[3] = ATH_TIDSCHED_VOQW;
    sc->acqw[2] = ATH_TIDSCHED_VIQW;
    sc->acqw[1] = ATH_TIDSCHED_BKQW;
    sc->acqw[0] = ATH_TIDSCHED_BEQW;
    sc->acto[3] = ATH_TIDSCHED_VOTO;
    sc->acto[2] = ATH_TIDSCHED_VITO;
    sc->acto[1] = ATH_TIDSCHED_BKTO;
    sc->acto[0] = ATH_TIDSCHED_BETO;
#endif

#ifdef VOW_LOGLATENCY
    sc->loglatency = ATH_LOGLATENCY;
#endif

#ifdef ATH_SUPPORT_IQUE
    sc->total_delay_timeout = ATH_RC_TOTAL_DELAY_TIMEOUT;
#endif

#if ATH_SUPPORT_VOWEXT
    /* RCA variables */
    sc->rca_aggr_uptime = ATH_11N_RCA_AGGR_UPTIME;
    sc->aggr_aging_factor = ATH_11N_AGGR_AGING_FACTOR;
    sc->per_aggr_thresh = ATH_11N_PER_AGGR_THRESH;
    sc->excretry_aggr_red_factor = ATH_11N_EXCRETRY_AGGR_RED_FACTOR;
    sc->badper_aggr_red_factor = ATH_11N_BADPER_AGGR_RED_FACTOR;
#endif

#if ATH_SUPPORT_VOWEXT
    sc->vsp_enable       = VSP_ENABLE_DEFAULT;
    sc->vsp_threshold    = VSP_THRESHOLD_DEFAULT;
    sc->vsp_evalinterval = VSP_EVALINTERVAL_DEFAULT;
    sc->vsp_prevevaltime = 0;
    sc->vsp_bependrop    = 0;
    sc->vsp_bkpendrop    = 0;
    sc->vsp_vipendrop    = 0;
    sc->vsp_vicontention = 0;
#if ATH_SUPPORT_FLOWMAC_MODULE
    /* When there are no transmit buffers, if there is way to inform kernel
     * not to send any more frames to Ath layer, inform the kernel's network
     * stack not to forward any more frames.
     * Currently ath_reg_parm.osnetif_flowcntrl is enabled for only Linux
     */
    sc->sc_osnetif_flowcntrl = sc->sc_reg_parm.osnetif_flowcntrl;
    /* in AP mode, when bridged with Ethernet, make sure that we pause the
     * Ethernet Rx, when wifi tx is in trouble. The above and this work
     * together, but the second one is an option to chose. Applicable for
     * only Linux Today.
     * Not applicable for other OSes. They assume zero value for this.
     */
    sc->sc_ethflowmac_enable = sc->sc_reg_parm.os_ethflowmac_enable;
    DPRINTF(sc, ATH_DEBUG_XMIT, 
            "%s sc->sc_osnetif_flowcntrl %d sc->sc_ethflowmac_enable %d\n", 
            __func__, sc->sc_osnetif_flowcntrl, sc->sc_ethflowmac_enable);
#endif
#endif
    sc->sc_bbpanicsupport = (ath_hal_getcapability(ah, HAL_CAP_BB_PANIC_WATCHDOG, 0, NULL) == HAL_OK);
    sc->sc_reset_type = ATH_RESET_DEFAULT;

    sc->sc_num_cals = ath_hal_get_cal_intervals(ah, &sc->sc_cals, HAL_QUERY_CALS);

	sc->sc_min_cal = ATH_LONG_CALINTERVAL;
	for (i=0; i<sc->sc_num_cals; i++) {
		HAL_CALIBRATION_TIMER *cal_timerp = &sc->sc_cals[i];
		sc->sc_min_cal = min(sc->sc_min_cal, cal_timerp->cal_timer_interval);
	}

    /*
     * sc is still not valid and we are in suspended state
     * until ath_open is called 
     * keep the chip in disabled/full sleep state. 
     */

    /* disable HAL and put h/w to sleep */
    ath_hal_disable(sc->sc_ah);

    ath_hal_configpcipowersave(sc->sc_ah, 1,1);

    ath_pwrsave_fullsleep(sc);


    ath_beacon_attach(sc);
#if ATH_SUPPORT_VOWEXT
    sc->sc_phyrestart_disable = 0;
    sc->sc_keysearch_always_enable = 0;
#endif

#if UMAC_SUPPORT_SMARTANTENNA
    /*
     *  TODO: This change can be removed once masterd changes are done for GUI(Hybrid Boards).
     */
    if(HAL_OK == ath_hal_getcapability(sc->sc_ah, HAL_CAP_SMARTANTENNA, 0, 0))
    {
       /* Do hardware initialization now */
      ath_enable_smartantenna(sc);
      DPRINTF(sc, ATH_DEBUG_ANY," %s : Smart Antenna Enabled \n",__func__);
    }
#endif    
#if ATH_ANI_NOISE_SPUR_OPT
    /* disabled by default */
    sc->sc_config.ath_noise_spur_opt = 0;
#endif
#if ATH_SUPPORT_VOW_DCS
    OS_MEMZERO(&sc->sc_dcs_params, sizeof(struct ath_dcs_params));
    sc->sc_phyerr_rate_thresh = PHYERR_RATE_THRESH;
    sc->sc_coch_intr_thresh = COCH_INTR_THRESH;
    sc->sc_tx_err_thresh = TXERR_THRESH; 
    sc->sc_dcs_enable = 0; 
#endif

#if ATH_SUPPORT_RX_PROC_QUOTA    
    sc->sc_rx_work = 0;
    sc->sc_process_rx_num = RX_PKTS_PROC_PER_RUN;
#endif    
    return 0;

bad2:
    /* cleanup tx queues */
    for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
        if (ATH_TXQ_SETUP(sc, i))
            ath_tx_cleanupq(sc, &sc->sc_txq[i]);
#if ATH_WOW
     if (sc->sc_wowInfo) {
         OS_FREE(sc->sc_wowInfo);
     }
#endif
     if (sc->sc_rc) {
         ath_rate_detach(sc->sc_rc);
     }
#ifdef ATH_SUPPORT_DFS
     if (sc->sc_dfs) {
         dfs_detach(sc);
     }
#endif
#ifdef ATH_TX99_DIAG
    if (sc->sc_tx99) {
        tx99_detach(sc);
    }
#endif
#if ATH_SUPPORT_SPECTRAL
    if (sc->sc_spectral) {
        spectral_detach(sc);
    }
#endif
#ifdef ATH_RFKILL
    if (sc->sc_hasrfkill)
        ath_rfkill_detach(sc);
#endif    

    if (ATH_ENAB_AOW(sc)) {
        ath_aow_tmr_cleanup(sc);
    }        

#ifdef ATH_GEN_TIMER
#ifdef ATH_C3_WAR
    if (sc->sc_c3_war_timer) {
        ath_gen_timer_free(sc, sc->sc_c3_war_timer);
        sc->sc_c3_war_timer = NULL;
    }
#endif
    if (sc->sc_hasgentimer)
        ath_gen_timer_detach(sc);
#endif
#ifdef ATH_BT_COEX
    if (ah && ath_hal_hasbtcoex(ah)) {
        ath_bt_coex_detach(sc);
    }
#endif
    
    /* Free green_ap state */
    if( sc->sc_green_ap ) {
        ath_green_ap_detach(sc);
        spin_lock_destroy(&sc->green_ap_ps_lock); 
    }

bad:
    if (ah) {
        ath_hal_detach(ah);
    }
    ATH_INTR_STATUS_LOCK_DESTROY(sc);
    //ATH_LOCK_DESTROY(sc);
    ATH_VAP_PAUSE_LOCK_DESTROY(sc);

    if (sc)
        OS_FREE(sc);
    
    /* null "out" parameters */
    *dev = NULL;
    *ops = NULL;

    return error;
#undef N
}

void
ath_dev_free(ath_dev_t dev)
{
    struct ath_softc *sc = (struct ath_softc *)dev;
    struct ath_hal *ah = sc->sc_ah;
    int i;

    DPRINTF(sc, ATH_DEBUG_ANY, "%s\n", __func__);

    ATH_USB_TQUEUE_FREE(sc);
    ATH_HTC_TXTQUEUE_FREE(sc);
    ATH_HTC_UAPSD_CREDITUPDATE_FREE(sc);
    ath_stop(sc);

    if (!sc->sc_invalid) {
        ath_hal_setpower(sc->sc_ah, HAL_PM_AWAKE);
    }

#ifdef ATH_TX_INACT_TIMER
    OS_CANCEL_TIMER(&sc->sc_inact);
#endif

#if ATH_HW_TXQ_STUCK_WAR
    if(sc->sc_enhanceddmasupport) {
        OS_CANCEL_TIMER(&sc->sc_qstuck);
    }
#endif

#ifdef ATH_SUPPORT_DFS
    dfs_detach(sc);
     if (sc->sc_dfs_hang.hang_war_activated) {
        OS_CANCEL_TIMER(&sc->sc_dfs_hang.hang_war_timer);
        sc->sc_dfs_hang.hang_war_activated = 0;
     }
#endif

#ifdef MAGPIE_HIF_GMAC
    OS_CANCEL_TIMER(&host_seek_credit_timer);
#endif

#if ATH_WOW
     if (sc->sc_wowInfo) {
         OS_FREE(sc->sc_wowInfo);
     }
#endif

#if ATH_SUPPORT_SPECTRAL
   spectral_detach(sc);
#endif

    ath_rate_detach(sc->sc_rc);

    /* cleanup tx queues */
    for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
        if (ATH_TXQ_SETUP(sc, i))
            ath_tx_cleanupq(sc, &sc->sc_txq[i]);

#ifdef ATH_RFKILL
    if (sc->sc_hasrfkill)
        ath_rfkill_detach(sc);
#endif    

#ifdef ATH_GEN_TIMER
#ifdef ATH_C3_WAR
    if (sc->sc_c3_war_timer) {
        ath_gen_timer_free(sc, sc->sc_c3_war_timer);
        sc->sc_c3_war_timer = NULL;
    }
#endif

    if (ATH_ENAB_AOW(sc)) {
        ath_aow_tmr_cleanup(sc);
    }        

    if (sc->sc_hasgentimer)
        ath_gen_timer_detach(sc);
#endif   /* ATH_GEN_TIMER*/ 

#ifdef ATH_BT_COEX
    if (ath_hal_hasbtcoex(ah)) {
        ath_bt_coex_detach(sc);
    }
#endif    
    
    /* Free green_ap state */
    if( sc->sc_green_ap ) {
        ath_green_ap_detach(sc);
        spin_lock_destroy(&sc->green_ap_ps_lock);
    }

#ifndef REMOVE_PKT_LOG
    if (sc->pl_info)
        ath_pktlog_detach(sc);
#endif
    ATH_INTR_STATUS_LOCK_DESTROY(sc);
    ATH_TXPOWER_DESTROY(sc);
    //ATH_LOCK_DESTROY(sc);
    ATH_RESET_LOCK_DESTROY(sc);
    ATH_RESET_SERIALIZE_LOCK_DESTROY(sc);
    ATH_VAP_PAUSE_LOCK_DESTROY(sc);
    
#if ATH_RESET_SERIAL
     for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
         ATH_RESET_LOCK_Q_DESTROY(sc, i);
#endif

#ifdef ATH_SWRETRY
    ATH_NODETABLE_LOCK_DESTROY(sc);
#endif
#ifdef ATH_SUPPORT_TxBF
    if (sc->sc_txbfsupport == AH_TRUE) {
#ifdef TXBF_TODO    
        ath_cancel_timer(&sc->sc_imbf_cal_short, CANCEL_NO_SLEEP);
        ath_free_timer(&sc->sc_imbf_cal_short);
#endif
        ath_cancel_timer(&sc->sc_selfgen_timer, CANCEL_NO_SLEEP);
        ath_free_timer(&sc->sc_selfgen_timer);
    }
#endif  // for TxBF RC
    ath_cancel_timer(&sc->sc_cal_ch, CANCEL_NO_SLEEP);  
    ath_free_timer(&sc->sc_cal_ch);

    if (ath_timer_is_initialized(&sc->sc_up_beacon)) {
        ath_cancel_timer(&sc->sc_up_beacon, CANCEL_NO_SLEEP);  
        ath_free_timer(&sc->sc_up_beacon);
    }
#if ATH_TRAFFIC_FAST_RECOVER
    if (ath_timer_is_initialized(&sc->sc_traffic_fast_recover_timer)) {
        ath_cancel_timer(&sc->sc_traffic_fast_recover_timer, CANCEL_NO_SLEEP);
        ath_free_timer(&sc->sc_traffic_fast_recover_timer);
    }
#endif
    ath_led_free_control(&sc->sc_led_control);
    ath_beacon_detach(sc);

    /* Detach HAL after the timers which still access the HAL */
    ath_hal_detach(ah);

    OS_FREE(sc);
}

static const char* hal_opmode2string( HAL_OPMODE opmode)
{
    switch ( opmode )
    {
    case HAL_M_STA:
         return "HAL_M_STA";
    case HAL_M_IBSS:
	 return "HAL_M_IBSS";
    case HAL_M_HOSTAP:
	 return "HAL_M_HOSTAP";
    case HAL_M_MONITOR:
       	 return "HAL_M_MONITOR";

    default:
	 return "Unknown hal_opmode"; 
    }
};

#if WAR_DELETE_VAP
static int
ath_vap_attach(ath_dev_t dev, int if_id, ieee80211_if_t if_data, HAL_OPMODE opmode, HAL_OPMODE iv_opmode, int nostabeacons, void **ret_avp)
#else
static int
ath_vap_attach(ath_dev_t dev, int if_id, ieee80211_if_t if_data, HAL_OPMODE opmode, HAL_OPMODE iv_opmode, int nostabeacons)
#endif
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_vap *avp;
    int rx_mitigation;

    DPRINTF(sc, ATH_DEBUG_STATE, 
            "%s : enter. dev=0x%p, if_id=0x%x, if_data=0x%p, opmode=%s, iv_opmode=%s, nostabeacons=0x%x\n", 
            __func__,    dev,      if_id,      if_data,
	        hal_opmode2string(opmode),hal_opmode2string(iv_opmode), nostabeacons
	   );

#if WAR_DELETE_VAP
    // NOTE :: Not MP Safe. This is to be used to WAR an issue seen under Darwin while it is being root caused.
    if (if_id >= ATH_BCBUF)
    {
        printk("%s: Invalid interface id = %u\n", __func__, if_id);
        return -EINVAL;
    }
    
    if (sc->sc_vaps[if_id] != NULL) {
        // The previous delete did not go through. So, save this vap in the pending deletion list.
        sc->sc_vaps[if_id]->av_del_next = NULL;
        sc->sc_vaps[if_id]->av_del_prev = NULL;
        if (sc->sc_vap_del_pend_head == NULL) {
            sc->sc_vap_del_pend_head = sc->sc_vap_del_pend_tail = sc->sc_vaps[if_id];
        }
        else {
            sc->sc_vap_del_pend_tail->av_del_next = sc->sc_vaps[if_id];
            sc->sc_vaps[if_id]->av_del_prev = sc->sc_vap_del_pend_tail;
            sc->sc_vap_del_pend_tail = sc->sc_vaps[if_id];
        } 
        sc->sc_vap_del_pend_count++;
        //sc->sc_vaps[if_id].av_del_timestamp = OS_GET_TIMESTAMP();
        sc->sc_vaps[if_id] = NULL;
        //now work around our work around. we deleted an item from the array, so decrease the counter
        //fixme - we should never come here, we need to know why this happens.
        if (sc->sc_nvaps) sc->sc_nvaps--;        
        printk("%s: WAR_DELETE_VAP sc_nvaps=%d\n", __func__, sc->sc_nvaps);
    }
#else
    if (if_id >= ATH_BCBUF || sc->sc_vaps[if_id] != NULL) {
        printk("%s: Invalid interface id = %u\n", __func__, if_id);
        return -EINVAL;
    }
#endif    

    switch (opmode) {
    case HAL_M_STA:
        sc->sc_nostabeacons = nostabeacons;
        break;
    case HAL_M_IBSS:
    case HAL_M_MONITOR:
        break;
    case HAL_M_HOSTAP:
        /* copy nostabeacons - for WDS client */
        sc->sc_nostabeacons = nostabeacons;
        /* XXX not right, beacon buffer is allocated on RUN trans */
        if (TAILQ_EMPTY(&sc->sc_bbuf)) {
            return -ENOMEM;
        }
        break;
    default:
        return -EINVAL;
    }

    /* create ath_vap */
    avp = (struct ath_vap *)OS_MALLOC(sc->sc_osdev, sizeof(struct ath_vap), GFP_KERNEL);
    if (avp == NULL)
        return -ENOMEM;

    OS_MEMZERO(avp, sizeof(struct ath_vap));
    avp->av_if_data = if_data;
    /* Set the VAP opmode */
    avp->av_opmode = iv_opmode;

    /*
     * The 9300 HAL always enables Rx mitigation (default), but this fails the
     * TGnInteropTP 5.2.9 STA test (inquiryl script).
     * So disable Rx Intr mitigation for STAs, to pass this test.
     * Note: This overrides the setting (if any) in HAL attach.
     *
     * FIXME: The assumption is that, if the 1st intf is a STA, it is more
     * likely that there are no other AP intfs (AP intfs are always first).
     */
    if (if_id == 0 && avp->av_opmode == HAL_M_STA) {
        rx_mitigation = 0;
        ath_hal_set_config_param(sc->sc_ah, HAL_CONFIG_INTR_MITIGATION_RX,
                                 &rx_mitigation);
    }

    avp->av_atvp = ath_rate_create_vap(sc->sc_rc, avp);
    if (avp->av_atvp == NULL) {
        OS_FREE(avp);
        return -ENOMEM;
    }

    avp->av_bslot = -1;
    TAILQ_INIT(&avp->av_mcastq.axq_q);
#if ATH_BEACON_DEFERRED_PROC
    avp->av_mcastq.irq_shared = 0;
#else
    avp->av_mcastq.irq_shared = 1;
#endif

    ATH_TXQ_LOCK_INIT(&avp->av_mcastq);
    atomic_set(&avp->av_stop_beacon, 0);
    atomic_set(&avp->av_beacon_cabq_use_cnt, 0);

    if (sc->sc_hastsfadd) {
        /*
         * Multiple vaps are to transmit beacons and we
         * have h/w support for TSF adjusting; enable use
         * of staggered beacons.
         */
        /* XXX check for beacon interval too small */
        if (sc->sc_reg_parm.burst_beacons)
            sc->sc_stagbeacons = 0;
        else
            sc->sc_stagbeacons = 1;
    }
    if (sc->sc_hastsfadd)
        ath_hal_settsfadjust(sc->sc_ah, sc->sc_stagbeacons);


     /*
      * For AoW Demo, we are resetting the TSF adjust capability of the
      * device, this is needed for the TSF sync, which is used in the
      * latency measurements
      *
      * XXX : TODO : Check for the side effects
      *
      */

    if (ATH_ENAB_AOW(sc) && ath_hal_gettsfadjust(sc->sc_ah) ) {
        ath_hal_settsfadjust(sc->sc_ah, 0);
        printk("Aow Demo, Resetting the TSF Adjust capability\n");
    }

    sc->sc_vaps[if_id] = avp;
    sc->sc_nvaps++;
    /* Set the device opmode */
    sc->sc_opmode = opmode;
    if ( (sc->sc_nvaps) && (sc->sc_opmode == HAL_M_HOSTAP)  ) {
        /* If it is a HOSTAP stop the Green AP feature */
        ath_green_ap_start( sc );
    }

#ifdef ATH_SUPERG_XR
    if (ieee80211vap_has_flag(vap, IEEE80211_F_XR)) {
        if (ath_descdma_setup(sc, &sc->sc_grppolldma, &sc->sc_grppollbuf,
                              "grppoll",(sc->sc_xrpollcount+1)*HAL_ANTENNA_MAX_MODE,1,1, ATH_FRAG_PER_MSDU) != 0) {
            printk("%s:grppoll Buf allocation failed \n",__func__);
        }
        if(!sc->sc_xrtxq)
            sc->sc_xrtxq = ath_txq_setup(sc, HAL_TX_QUEUE_DATA,HAL_XR_DATA);
        if (sc->sc_hasdiversity) {
            /* Save current diversity state if user destroys XR vap */
            sc->sc_olddiversity = sc->sc_diversity;
            ath_hal_setdiversity(sc->sc_ah, 0);
            sc->sc_diversity=0;
        }
    }
#endif

    /* default VAP configuration */
    avp->av_config.av_fixed_rateset = IEEE80211_FIXED_RATE_NONE;
    avp->av_config.av_fixed_retryset = 0x03030303;
    /* If the mode is not HOSTAP, stop the GreenAP powersave feature */
    if( iv_opmode != HAL_M_HOSTAP ) {
        ath_green_ap_stop(sc);        
    }

#if WAR_DELETE_VAP
    // Load the return vap pointer. This will be passed in during deletion.
    *ret_avp = (void *)avp;
#endif

    ath_vap_info_attach(sc, avp);

    DPRINTF(sc, ATH_DEBUG_STATE,
            "%s: exit.\n",
            __func__
	    );

    return 0;
}

static int
#if WAR_DELETE_VAP    
ath_vap_detach(ath_dev_t dev, int if_id, void *athvap)
#else
ath_vap_detach(ath_dev_t dev, int if_id)
#endif
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_vap *avp;
#if 0
    /* Unused variables. */
    struct ath_hal *ah = sc->sc_ah;
    int flags;
#endif
 
    DPRINTF(sc, ATH_DEBUG_STATE,
            "%s: enter. dev=0x%p, if_id=0x%x\n",
            __func__,
	    dev,
	    if_id
	    );

    ATH_PS_WAKEUP(sc);
#if WAR_DELETE_VAP
    avp = (struct ath_vap *)athvap;
#else
    avp = sc->sc_vaps[if_id];
#endif
    if (avp == NULL) {
        DPRINTF(sc, ATH_DEBUG_STATE, "%s: invalid interface id %u\n",
                __func__, if_id);
        ATH_PS_SLEEP(sc);
        return -EINVAL;
    }

    ath_vap_info_detach(sc, avp);

#if 0
    /*
     * In multiple vap scenario, this will cause data loss in other vaps.
     * Presumbly, there should be no resource associate with this vap.
     */
    flags = sc->sc_ieee_ops->get_netif_settings(sc->sc_ieee);
    if (flags & ATH_NETIF_RUNNING) {
        /*
         * Quiesce the hardware while we remove the vap.  In
         * particular we need to reclaim all references to the
         * vap state by any frames pending on the tx queues.
         *
         * XXX can we do this w/o affecting other vap's?
         */
        ath_hal_intrset(ah, 0);         /* disable interrupts */
        ath_draintxq(sc, AH_FALSE, 0);  /* stop xmit side */
        ATH_STOPRECV(sc, 0);            /* stop recv side */
        ath_flushrecv(sc);              /* flush recv queue */
    }
#endif
    
    /*
     * Reclaim any pending mcast bufs on the vap.
     */
    ath_tx_mcast_draintxq(sc, &avp->av_mcastq);
    ATH_TXQ_LOCK_DESTROY(&avp->av_mcastq);

    if (sc->sc_opmode == HAL_M_HOSTAP && sc->sc_nostabeacons)
        sc->sc_nostabeacons = 0;

    ath_rate_free_vap(avp->av_atvp);
#if WAR_DELETE_VAP
    // Relink pointers in the queue.
    if (sc->sc_vap_del_pend_head) {
        if (avp == sc->sc_vap_del_pend_head) {
            sc->sc_vap_del_pend_head = avp->av_del_next;
            if (sc->sc_vap_del_pend_head) {
                sc->sc_vap_del_pend_head->av_del_prev = NULL;
            }
            else {
                sc->sc_vap_del_pend_tail = NULL;
            }
        }
        else if (avp == sc->sc_vap_del_pend_tail) {
            sc->sc_vap_del_pend_tail = avp->av_del_prev;
            sc->sc_vap_del_pend_tail->av_del_next = NULL;
        }
        else {
            if (avp->av_del_prev) {
                avp->av_del_prev->av_del_next = avp->av_del_next;
            }
            if (avp->av_del_next) {
                avp->av_del_next->av_del_prev = avp->av_del_prev;
            }
        }
        sc->sc_vap_del_pend_count--;
    }
#endif

#ifdef WAR_DELETE_VAP
    if (sc->sc_vaps[if_id] == avp) {
        sc->sc_vaps[if_id] = NULL;
    } else {
        //war case of a late delete, try to keep a sane sc_nvaps..
        sc->sc_nvaps++;
    }
#else
    sc->sc_vaps[if_id] = NULL;
#endif
    OS_FREE(avp);
    sc->sc_nvaps--;
    if ( (!sc->sc_nvaps) && (sc->sc_opmode == HAL_M_HOSTAP)  ) {
        /* If it is a HOSTAP stop the Green AP feature */
        ath_green_ap_stop( sc );
    }
#ifdef ATH_SUPERG_XR 
    /*
     * if its an XR vap ,free the memory allocated explicitly.
     * since the XR vap is not registered , OS can not free the memory.
     */
    if(ieee80211vap_has_flag(vap, IEEE80211_F_XR)) {
        ath_grppoll_stop(vap);
        ath_descdma_cleanup(sc,&sc->sc_grppolldma,&sc->sc_grppollbuf, BUS_DMA_FROMDEVICE);
        OS_MEMZERO(&sc->sc_grppollbuf, sizeof(sc->sc_grppollbuf));
        OS_MEMZERO(&sc->sc_grppolldma, sizeof(sc->sc_grppolldma));
#ifndef ATHR_RNWF
        if(vap->iv_xrvap)
            vap->iv_xrvap->iv_xrvap=NULL;
        kfree(vap->iv_dev);
#endif
        ath_tx_cleanupq(sc,sc->sc_xrtxq);
        sc->sc_xrtxq=NULL;
        if (sc->sc_hasdiversity) {
            /* Restore diversity setting to old diversity setting */
            ath_hal_setdiversity(ah, sc->sc_olddiversity);
            sc->sc_diversity = sc->sc_olddiversity;
        }
    }
#endif

#if 0
    /*
     * In multiple vap scenario, this will cause data loss in other vaps.
     * Presumbly, there should be no resource associate with this vap.
     */
    /* 
    ** restart H/W in case there are other VAPs, however
    ** we must ensure that it's "valid" to have interrupts on at
    ** this time
    */

    if ((flags & ATH_NETIF_RUNNING) && sc->sc_nvaps && !sc->sc_invalid) {
        /*
         * Restart rx+tx machines if device is still running.
         */
        if (ATH_STARTRECV(sc) != 0)    /* restart recv */
            printk("%s: unable to start recv logic\n",__func__);
        ath_hal_intrset(ah, sc->sc_imask);
    }
#endif

    ATH_PS_SLEEP(sc);

    DPRINTF(sc, ATH_DEBUG_STATE,
            "%s: exit.\n",
            __func__
	    );
#if ATH_TRAFFIC_FAST_RECOVER
    if (ath_timer_is_initialized(&sc->sc_traffic_fast_recover_timer)) {
        ath_cancel_timer(&sc->sc_traffic_fast_recover_timer, CANCEL_NO_SLEEP);
    }
#endif
    return 0;
}

static int
ath_vap_config(ath_dev_t dev, int if_id, struct ath_vap_config *if_config)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_vap *avp;

    if (if_id >= ATH_BCBUF) {
        printk("%s: Invalid interface id = %u\n", __func__, if_id);
        return -EINVAL;
    }

    avp = sc->sc_vaps[if_id];
    ASSERT(avp != NULL);

    if (avp) {
        OS_MEMCPY(&avp->av_config, if_config, sizeof(avp->av_config));
    } else {
       /* Intentinal print */
	printk("%s[%d]  NULL avp\n", __func__, __LINE__);
    }

    return 0;
}

#if AR_DEBUG
static void ath_sm_update_announce(ath_dev_t dev, ath_node_t node)
{
    char smpstate[32] = "";
    struct ath_node *an = ATH_NODE(node);
    struct ieee80211_node *ni = (struct ieee80211_node *) an->an_node;
    u_int8_t *mac = ni->ni_macaddr;
    int capflag = an->an_cap;
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (OS_MEMCMP(ni->ni_macaddr, ni->ni_bssid, IEEE80211_ADDR_LEN) == 0) {
        return;
    }
    OS_MEMCPY(smpstate,"", 1);
    if (capflag & ATH_RC_HT_FLAG) {
        if (an->an_smmode == ATH_SM_ENABLE) {
            OS_MEMCPY(smpstate, "SM Enabled", strlen("SM Enabled"));
        } else {
            if (an->an_smmode == ATH_SM_PWRSAV_DYNAMIC) {
                OS_MEMCPY(smpstate, "SM Dynamic pwrsav",
                    strlen("SM Dynamic pwrsav"));
            } else {
                OS_MEMCPY(smpstate,"SM Static pwrsav",
                    strlen("SM Static pwrsav"));
            }
        }
    }
    DPRINTF(((struct ath_softc *)sc), ATH_DEBUG_PWR_SAVE,
           "mac=%02x:%02x:%02x:"
           "%02x:%02x:%02x : %s capflag=0x%u Flags:%s%s%s%s\n",
           mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5], smpstate, capflag,
           (capflag & ATH_RC_HT_FLAG) ? " HT" : "",
           (capflag & ATH_RC_CW40_FLAG) ? " HT40" : "",
           (capflag & ATH_RC_SGI_FLAG) ? " SGI" : "",
           (capflag & ATH_RC_DS_FLAG) ? " DS" : "");
}
#endif /* AR_DEBUG */

static u_int
ath_get_wirelessmodes(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return ath_hal_getwirelessmodes(sc->sc_ah, sc->sc_ah->ah_countryCode);
}

static u_int32_t
ath_get_device_info(ath_dev_t dev, u_int32_t type)
{
    HAL_CHANNEL *chan;
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t result = 0;

    switch (type) {
    case ATH_MAC_VERSION:        /* MAC version id */
        result = ath_hal_get_device_info(ah, HAL_MAC_VERSION);
        break;
    case ATH_MAC_REV:            /* MAC revision */
        result = ath_hal_get_device_info(ah, HAL_MAC_REV);
        break;
    case ATH_PHY_REV:            /* PHY revision */
        result = ath_hal_get_device_info(ah, HAL_PHY_REV);
        break;
    case ATH_ANALOG5GHZ_REV:     /* 5GHz radio revision */
        result = ath_hal_get_device_info(ah, HAL_ANALOG5GHZ_REV);
        break;
    case ATH_ANALOG2GHZ_REV:     /* 2GHz radio revision */
        result = ath_hal_get_device_info(ah, HAL_ANALOG2GHZ_REV);
        break;
    case ATH_WIRELESSMODE:       /* Wireless mode */
        chan = &sc->sc_curchan;
        result = ath_halchan2mode(chan);
        break;
    default:
        //printk("AthDeviceInfo: Unknown type %d\n", type);
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: Unknown type %d\n", __func__, type);
        ASSERT(0);
    }
    return result;
}

static void
ath_update_sm_pwrsave(ath_dev_t dev, ath_node_t node, ATH_SM_PWRSAV mode,
    int ratechg)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = (struct ath_node *) node;

    switch (sc->sc_opmode) {
    case HAL_M_HOSTAP:
        /*
         * Update spatial multiplexing mode of client node.
         */
        if (an != NULL) {
            
            an->an_smmode = mode;
            
            DPRINTF(sc,ATH_DEBUG_PWR_SAVE,"%s: ancaps %#x\n", __func__, an->an_smmode);
            if (ratechg) {
                ath_rate_node_update(an);
#if AR_DEBUG
                ath_sm_update_announce(dev, node);
#endif
            }
        }
        break;

    case HAL_M_STA:
        switch (mode) {
        case ATH_SM_ENABLE:
            /* Spatial Multiplexing enabled, revert back to default SMPS register settings */
            ath_hal_setsmpsmode(sc->sc_ah, HAL_SMPS_DEFAULT);
            break;
        case ATH_SM_PWRSAV_STATIC:
            /*
             * Update current chainmask from configuration parameter.
             * A subsequent reset is needed for new chainmask to take effect.
             */
            sc->sc_rx_chainmask = sc->sc_config.rxchainmask;
            sc->sc_rx_numchains = sc->sc_mask2chains[sc->sc_rx_chainmask];
            break;
        case ATH_SM_PWRSAV_DYNAMIC:
            /* Enable hardware control of SM Power Save */
            ath_hal_setsmpsmode(sc->sc_ah, HAL_SMPS_HW_CTRL);
            break;
        default:
            break;
        }
        if (an != NULL) {
            an->an_smmode = mode;
            if (ratechg)
                ath_rate_node_update(an);
        }
        break;

    case HAL_M_IBSS:
    case HAL_M_MONITOR:
    default:
        /* Not supported */
        break;
    }
}

static u_int32_t
ath_node_gettxrate(ath_node_t node)
{
    struct ath_node *an = (struct ath_node *) node;

    return ath_rate_node_gettxrate(an);
}

void
ath_node_set_fixedrate(ath_node_t node, u_int8_t fixed_ratecode)
{
    struct ath_node *an = (struct ath_node *) node;
    struct ath_softc *sc = an->an_sc;
    const HAL_RATE_TABLE *rt = sc->sc_rates[sc->sc_curmode];

    u_int8_t i;
    u_int8_t ndx = 0;
         
    for (i = 0; i < rt->rateCount; i++) {
        if (rt->info[i].rateCode == fixed_ratecode) {
            ndx = i;
            break;
        }
    }  
 
    an->an_fixedrate_enable = fixed_ratecode == 0 ? 0 : 1;
    an->an_fixedrix = ndx;
    an->an_fixedratecode = fixed_ratecode;
    printk("%s: an_fixedrate_enable = %d, an_fixedrix = %d, an_fixedratecode = 0x%x\n",
            __func__, an->an_fixedrate_enable, an->an_fixedrix, an->an_fixedratecode);

    return;
}

static u_int32_t
ath_node_getmaxphyrate(ath_dev_t dev, ath_node_t node,uint32_t shortgi)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = (struct ath_node *) node;

    return ath_rate_getmaxphyrate(sc, an,shortgi);
}

static int
athop_rate_newassoc(ath_dev_t dev, ath_node_t node, int isnew, unsigned int capflag,
                                  struct ieee80211_rateset *negotiated_rates,
                                  struct ieee80211_rateset *negotiated_htrates)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = (struct ath_node *) node;

    ath_wmi_rc_node_update(dev, an, isnew, capflag, negotiated_rates, negotiated_htrates);

    return ath_rate_newassoc(sc, an, isnew, capflag, negotiated_rates, negotiated_htrates);
}

static void
athop_rate_newstate(ath_dev_t dev, int if_id, int up)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_vap *avp = sc->sc_vaps[if_id];

    ath_rate_newstate(sc, avp, up);
}


static ath_node_t
ath_node_attach(ath_dev_t dev, int if_id, ieee80211_node_t ni, bool tmpnode)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_vap *avp;
    struct ath_node *an;
#ifdef ATH_SWRETRY
    u_int8_t i;
#endif

    avp = sc->sc_vaps[if_id];
    ASSERT(avp != NULL);
    
    an = (struct ath_node *)OS_MALLOC(sc->sc_osdev, sizeof(struct ath_node), GFP_ATOMIC);
    if (an == NULL) {
        return NULL;
    }
    OS_MEMZERO(an, sizeof(*an));

    an->an_node = ni;
    an->an_sc = sc;
    an->an_avp = avp;
    an->an_decomp_index = INVALID_DECOMP_INDEX;

    an->an_rc_node = ath_rate_node_alloc(avp->av_atvp);
    if (an->an_rc_node == NULL) {
        OS_FREE(an);
        return NULL;
    }

    /* XXX no need to setup vap pointer in ni here.
     * It should already be taken care of by caller. */
    ath_rate_node_init(an->an_rc_node);

    if (tmpnode) {
        an->an_flags |= ATH_NODE_TEMP;
    }

    /* set up per-node tx/rx state */
    ath_tx_node_init(sc, an);
    ath_rx_node_init(sc, an);
    
#ifdef ATH_SUPPORT_UAPSD
    /* U-APSD init */
    TAILQ_INIT(&an->an_uapsd_q);
#ifdef ATH_VAP_PAUSE_SUPPORT
    TAILQ_INIT(&an->an_uapsd_stage_q);
#endif
    an->an_uapsd_qdepth = 0;
#endif

#ifdef ATH_SWRETRY
    /* As of now, SW Retry mechanism will be enabled only when 
     * STA enters into RUN state. Need to revisit this part 
     * if Sw retries are to be enabled right from JOIN state
     */
    ATH_NODE_SWRETRY_TXBUF_LOCK_INIT(an);
    TAILQ_INIT(&an->an_softxmit_q);
    an->an_swrenabled = AH_FALSE;
    an->an_softxmit_qdepth = 0;
    an->an_total_swrtx_successfrms = 0;
    an->an_total_swrtx_flushfrms = 0;
    an->an_total_swrtx_pendfrms = 0;
    an->an_pspoll_pending = AH_FALSE;
    an->an_tim_set = AH_FALSE;
#ifdef ATH_SUPPORT_UAPSD
    an->an_uapsd_num_addbuf = 0;
#endif
    for (i=0; i<HAL_NUM_TX_QUEUES; i++) {
        (an->an_swretry_info[i]).swr_state_filtering = AH_FALSE;
        (an->an_swretry_info[i]).swr_num_eligible_frms = 0;
    }
#endif
#ifdef ATH_CHAINMASK_SELECT
    ath_chainmask_sel_init(sc, an);
    ath_chainmask_sel_timerstart(&an->an_chainmask_sel);
#endif

#ifdef ATH_SWRETRY
    ATH_NODETABLE_LOCK(sc);
    LIST_INSERT_HEAD(&sc->sc_nt, an, an_le);
    ATH_NODETABLE_UNLOCK(sc);
#endif

#if ATH_SUPPORT_VOWEXT
    an->min_depth = ATH_AGGR_MIN_QDEPTH;
    an->throttle = 0;
#endif

    an->an_fixedrate_enable = 0;
    an->an_fixedrix = 0;    /* Lowest rate by default if fixed rate is enabled for the node */

    DPRINTF(sc, ATH_DEBUG_NODE, "%s: an %p\n", __func__, an);
    return an;
}

static void
ath_node_detach(ath_dev_t dev, ath_node_t node)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = ATH_NODE(node);

    ath_tx_node_free(sc, an);
    ath_rx_node_free(sc, an);

#ifdef ATH_SUPERG_XR
    ath_grppoll_period_update(sc);
#endif

    ath_rate_node_free(an->an_rc_node);

#ifdef ATH_SWRETRY
    ATH_NODETABLE_LOCK(sc);
    LIST_REMOVE(an, an_le);
    ATH_NODETABLE_UNLOCK(sc);
#endif
    /* Check if the mode is HOSTAP, in this case, decrement the node count */
    if((sc->sc_opmode == HAL_M_HOSTAP) && (an->is_sta_node)) {
        ath_green_ap_state_mc(sc, ATH_PS_EVENT_DEC_STA );
    }

    OS_FREE(an);
}

static void
ath_node_cleanup(ath_dev_t dev, ath_node_t node)
{
#define WAIT_TX_INTERVAL 100
    u_int32_t elapsed_time = 0;
    struct ath_node *an = ATH_NODE(node);
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

#ifdef ATH_CHAINMASK_SELECT
    ath_chainmask_sel_timerstop(&an->an_chainmask_sel);
#endif

    an->an_flags |= ATH_NODE_CLEAN;

    /*
     * Wait until pending tx threads complete
     */
    do {
        if (!atomic_read(&an->an_active_tx_cnt))
            break;

        OS_DELAY(WAIT_TX_INTERVAL);
        elapsed_time += WAIT_TX_INTERVAL;

        if (elapsed_time > (1000 * WAIT_TX_INTERVAL)) {
            DPRINTF(sc, ATH_DEBUG_ANY,"%s: an_active_tx_cnt stuck. Investigate!!!\n", __func__);
        }
    } while (1);

#ifdef ATH_SWRETRY
    if (sc->sc_swRetryEnabled) {
        ATH_NODE_SWRETRY_TXBUF_LOCK(an);
        an->an_flags |= ATH_NODE_SWRETRYQ_FLUSH;
        ATH_NODE_SWRETRY_TXBUF_UNLOCK(an);
        ath_tx_flush_node_sxmitq(sc, an);
    }
    if (an->an_tim_set == AH_TRUE)
    {
        ATH_NODE_SWRETRY_TXBUF_LOCK(an);
        an->an_tim_set = AH_FALSE;
        sc->sc_ieee_ops->set_tim(an->an_node,0);
        ATH_NODE_SWRETRY_TXBUF_UNLOCK(an);
    }
#endif
#ifdef ATH_SUPPORT_UAPSD
    if (an->an_flags & ATH_NODE_UAPSD) {
        an->an_flags &= ~ATH_NODE_UAPSD;
        ATH_TX_UAPSD_NODE_CLEANUP(sc, an);
    }
#endif
    ATH_TX_NODE_CLEANUP(sc, an);
    ath_rx_node_cleanup(sc, an);
    ath_rate_node_cleanup(an->an_rc_node);

#undef WAIT_TX_INTERVAL
}

static void
ath_node_update_pwrsave(ath_dev_t dev, ath_node_t node, int pwrsave, int pause_resume)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = ATH_NODE(node);

#ifdef ATH_SWRETRY
    ATH_NODE_SWRETRY_TXBUF_LOCK(an);
    an->an_pspoll_pending = AH_FALSE;
#endif

    if (pwrsave) {
        an->an_flags |= ATH_NODE_PWRSAVE;
        if (pause_resume) {
            ath_tx_node_pause(sc,an);
#ifdef ATH_SWRETRY		
            if (!an->an_tim_set && ((sc->sc_ieee_ops->get_pwrsaveq_len(an->an_node)==0)) && (ath_exist_pendingfrm_tidq(dev,an)==0))
            {
                sc->sc_ieee_ops->set_tim(an->an_node,1);
                an->an_tim_set = AH_TRUE;
            }		
#endif	
        }
    } else {
        an->an_flags &= ~ATH_NODE_PWRSAVE;	
        if (pause_resume) {
            ath_tx_node_resume(sc,an);
#ifdef ATH_SWRETRY		
            if (an->an_tim_set && (sc->sc_ieee_ops->get_pwrsaveq_len(an->an_node)==0) )
            {
                sc->sc_ieee_ops->set_tim(an->an_node,0);
            }	
            an->an_tim_set = AH_FALSE;
#endif	
        }
    }
#ifdef ATH_SWRETRY	
	ATH_NODE_SWRETRY_TXBUF_UNLOCK(an);
#endif
}
/*
 * Allocate data  key slots for TKIP.  We allocate two slots for
 * one for decrypt/encrypt and the other for the MIC.
 */
static u_int16_t
ath_key_alloc_pair(ath_dev_t dev)
{
#define    N(a)    (sizeof(a)/sizeof(a[0]))
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int i, keyix;

    /* XXX could optimize */
    for (i = 0; i < N(sc->sc_keymap)/2; i++) {
        u_int8_t b = sc->sc_keymap[i];
        if (b != 0xff) {
            /*
             * One or more slots in this byte are free.
             */
            keyix = i*NBBY;
            while (b & 1) {
        again:
                keyix++;
                b >>= 1;
            }
            /* XXX IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV */
            if ( isset(sc->sc_keymap, keyix+64) ) {
                /* pair unavailable */
                /* XXX statistic */
                if (keyix == (i+1)*NBBY) {
                    /* no slots were appropriate, advance */
                    continue;
                }
                goto again;
            }
            setbit(sc->sc_keymap, keyix);
            setbit(sc->sc_keymap, keyix+64);
            DPRINTF(sc, ATH_DEBUG_KEYCACHE,
                "%s: key pair %u ,%u\n",
                __func__, keyix, keyix+64 );
            return keyix;
        }
    }
    DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: out of pair space\n", __func__);
    return -1;
#undef N
}

/*
 * Allocate tx/rx key slots for TKIP.  We allocate two slots for
 * each key, one for decrypt/encrypt and the other for the MIC.
 */
static u_int16_t
ath_key_alloc_2pair(ath_dev_t dev)
{
#define    N(a)    (sizeof(a)/sizeof(a[0]))
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int i, keyix;

    KASSERT(ath_hal_tkipsplit(sc->sc_ah), ("key cache !split"));
    /* XXX could optimize */
    for (i = 0; i < N(sc->sc_keymap)/4; i++) {
        u_int8_t b = sc->sc_keymap[i];
        if (b != 0xff) {
            /*
             * One or more slots in this byte are free.
             */
            keyix = i*NBBY;
            while (b & 1) {
again:
                keyix++;
                b >>= 1;
            }
            /* XXX IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV */
            if (isset(sc->sc_keymap, keyix+32) ||
                isset(sc->sc_keymap, keyix+64) ||
                isset(sc->sc_keymap, keyix+32+64)) {
                /* full pair unavailable */
                /* XXX statistic */
                if (keyix == (i+1)*NBBY) {
                    /* no slots were appropriate, advance */
                    continue;
                }
                goto again;
            }
            setbit(sc->sc_keymap, keyix);
            setbit(sc->sc_keymap, keyix+64);
            setbit(sc->sc_keymap, keyix+32);
            setbit(sc->sc_keymap, keyix+32+64);
#if 0
            DPRINTF(sc, ATH_DEBUG_KEYCACHE,
                    "%s: key pair %u,%u %u,%u\n",
                    __func__, keyix, keyix+64,
                    keyix+32, keyix+32+64);
#endif
            return keyix;
        }
    }
    DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: out of pair space\n", __func__);
    return -1;
#undef N
}

/*
 * Allocate a single key cache slot.
 */
static u_int16_t
ath_key_alloc_single(ath_dev_t dev)
{
#define    N(a)    (sizeof(a)/sizeof(a[0]))
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int i, keyix;

    /* XXX try i,i+32,i+64,i+32+64 to minimize key pair conflicts */
    for (i = 0; i < N(sc->sc_keymap); i++) {
        u_int8_t b = sc->sc_keymap[i];
        if (b != 0xff) {
            /*
             * One or more slots are free.
             */
            keyix = i*NBBY;
            while (b & 1) {
                keyix++, b >>= 1;
            }
            setbit(sc->sc_keymap, keyix);
            DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: key %u\n",
                    __func__, keyix);
            return keyix;
        }
    }
    DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: out of space\n", __func__);
    return -1;
#undef N
}

static void
ath_key_reset(ath_dev_t dev, u_int16_t keyix, int freeslot)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ATH_PS_WAKEUP(sc);

    ath_hal_keyreset(sc->sc_ah, keyix);
    if (freeslot)
        clrbit(sc->sc_keymap, keyix);

    ATH_PS_SLEEP(sc);
}

static int
ath_keyset(ath_dev_t dev, u_int16_t keyix, HAL_KEYVAL *hk,
           const u_int8_t mac[IEEE80211_ADDR_LEN])
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    HAL_BOOL status;
    
    ATH_FUNC_ENTRY_CHECK(sc, 0);
    ATH_PS_WAKEUP(sc);

    status = ath_hal_keyset(sc->sc_ah, keyix, hk, mac);

    ATH_PS_SLEEP(sc);

    return (status != AH_FALSE);
}

static u_int
ath_keycache_size(ath_dev_t dev)
{
    return ATH_DEV_TO_SC(dev)->sc_keymax;
}

/*
 * Set the 802.11D country
 */
static int
ath_set_country(ath_dev_t dev, char *isoName, u_int16_t cc, enum ieee80211_clist_cmd cmd)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_CHANNEL *chans;
    int nchan;
    u_int8_t regclassids[ATH_REGCLASSIDS_MAX];
    u_int nregclass = 0;
    u_int wMode;
    u_int netBand;
    int outdoor = ath_outdoor;
    int indoor = ath_indoor;

    if(!isoName || !isoName[0] || !isoName[1]) {
        if (cc) 
            sc->sc_config.ath_countrycode = cc;
        else
            sc->sc_config.ath_countrycode = CTRY_DEFAULT;
    } else {
        sc->sc_config.ath_countrycode = findCountryCode((u_int8_t *)isoName);
 
        /* Map the ISO name ' ', 'I', 'O' */
        if (isoName[2] == 'O') {
            outdoor = AH_TRUE;
            indoor  = AH_FALSE;
        }
        else if (isoName[2] == 'I') {
            indoor  = AH_TRUE;
            outdoor = AH_FALSE;
        }
        else if ((isoName[2] == ' ') || (isoName[2] == 0)) {
            outdoor = AH_FALSE;
            indoor  = AH_FALSE;
        }
        else
            return -EINVAL;
    }

    wMode = sc->sc_reg_parm.wModeSelect;
    if (!(wMode & HAL_MODE_11A)) {
        wMode &= ~(HAL_MODE_TURBO|HAL_MODE_108A|HAL_MODE_11A_HALF_RATE);
    }
    if (!(wMode & HAL_MODE_11G)) {
        wMode &= ~(HAL_MODE_108G);
    }
    netBand = sc->sc_reg_parm.NetBand;
    if (!(netBand & HAL_MODE_11A)) {
        netBand &= ~(HAL_MODE_TURBO|HAL_MODE_108A|HAL_MODE_11A_HALF_RATE);
    }
    if (!(netBand & HAL_MODE_11G)) {
        netBand &= ~(HAL_MODE_108G);
    }
    wMode &= netBand;

    chans = (HAL_CHANNEL *)OS_MALLOC(sc->sc_osdev, 
                      IEEE80211_CHAN_MAX * sizeof(HAL_CHANNEL), 
                      GFP_ATOMIC);

    if (chans == NULL) {
        printk("%s: unable to allocate channel table\n", __func__);
        return -ENOMEM;
    }
    
    ATH_PS_WAKEUP(sc);
    ATH_DEV_RESET_HT_MODES(wMode);   /* reset the HT modes */
    if (!ath_hal_init_channels(ah, chans, IEEE80211_CHAN_MAX, (u_int *)&nchan,
                               regclassids, ATH_REGCLASSIDS_MAX, &nregclass,
                               sc->sc_config.ath_countrycode, wMode, outdoor, 
                               sc->sc_config.ath_xchanmode)) {
        OS_FREE(chans);
        ATH_PS_SLEEP(sc);
        return -EINVAL;
    }

#ifdef ATH_SUPPORT_DFS
    /* Setting country code might change the DFS domain
     * so  initialize the DFS Radar filters */
    dfs_init_radar_filters(sc);
#endif
    ATH_PS_SLEEP(sc);

    if (sc->sc_ieee_ops->setup_channel_list) {
        sc->sc_ieee_ops->setup_channel_list(sc->sc_ieee, cmd,
                                            chans, nchan, regclassids, nregclass,
                                            sc->sc_config.ath_countrycode);
    }

    OS_FREE(chans);     
    return 0;
}

/*
 * Return the current country and domain information
 */
static void
ath_get_currentCountry(ath_dev_t dev, HAL_COUNTRY_ENTRY *ctry)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ath_hal_getCurrentCountry(sc->sc_ah, ctry);

    /* If HAL not specific yet, since it is band dependent, use the one we passed in.*/
    if (ctry->countryCode == CTRY_DEFAULT) {
        ctry->iso[0] = 0;
        ctry->iso[1] = 0;
    }
    else if (ctry->iso[0] && ctry->iso[1]) {
        if (ath_outdoor)
            ctry->iso[2] = 'O';
        else if (ath_indoor)
            ctry->iso[2] = 'I';
        else
            ctry->iso[2] = ' ';
    }
}

static int
ath_set_regdomain(ath_dev_t dev, int regdomain, HAL_BOOL wrtEeprom)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    HAL_STATUS status;
    int ret;

    if (wrtEeprom) {   
        ret = ath_hal_setregdomain(sc->sc_ah, regdomain, &status); 
    } else {
        if (ath_hal_setcapability(sc->sc_ah, HAL_CAP_REG_DMN, 0, regdomain, &status))
            ret = !ath_getchannels(sc, CTRY_DEFAULT, AH_FALSE, AH_TRUE);
        else 
            ret = AH_FALSE;
    }

    if (ret == AH_TRUE)
        return 0;
    else
        return -EIO;
}

static int
ath_get_regdomain(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t regdomain;
    HAL_STATUS status;

    status = ath_hal_getregdomain(sc->sc_ah, &regdomain);
    KASSERT(status == HAL_OK, ("get_regdomain failed"));

    return (regdomain);
}

static int
ath_set_quiet(ath_dev_t dev, u_int16_t period, u_int16_t duration,
              u_int16_t nextStart, u_int16_t enabled)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return ath_hal_setQuiet(sc->sc_ah, period, period, nextStart, enabled);
}


static int
ath_get_dfsdomain(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int32_t regdomain;
    HAL_STATUS status;

	status = ath_hal_getcapability(sc->sc_ah, HAL_CAP_DFS_DMN, 0, &regdomain);
    KASSERT(status == HAL_OK, ("get_dfsdomain failed"));

    return (regdomain);
}

static u_int16_t
ath_find_countrycode(ath_dev_t dev, char* isoName)
{
    UNREFERENCED_PARAMETER(dev);

    return findCountryCode((u_int8_t*)isoName);
}

static u_int8_t
ath_get_ctl_by_country(ath_dev_t dev, u_int8_t* isoName, HAL_BOOL is2G)
{
    HAL_CTRY_CODE cc;
    UNREFERENCED_PARAMETER(dev);

    cc = findCountryCode(isoName);

    return findCTLByCountryCode(cc, is2G);
}

/*
 * this function returns 1 if we are in DFS regulatory domain.
 * otherwise it will return 0.
 * it makes use of the flag that is set in dfs_attach function in DFS
 * module
 */

static u_int16_t 
ath_dfs_isdfsregdomain(ath_dev_t dev)
{
#ifdef ATH_SUPPORT_DFS
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return sc->sc_dfs->sc_dfs_isdfsregdomain; 
#else
    return 0;
#endif
}
/*
 * this function returns 1 if we are in DFS CAC period
 * otherwise it returns 0.
 */

static u_int16_t 
ath_dfs_in_cac(ath_dev_t dev)  
{
#ifdef ATH_SUPPORT_DFS
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return (u_int16_t) sc->sc_dfs->sc_dfswait;
#else
    return 0;
#endif
}

/*
 * this function returns 1 if usenol value is set to 1 by radartool.
 * otherwise it returns 0.
 */

static u_int16_t 
ath_dfs_usenol(ath_dev_t dev)  
{
#ifdef ATH_SUPPORT_DFS
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return (u_int16_t) sc->sc_dfs->dfs_rinfo.rn_use_nol;
#else
    return 0;
#endif
}


static int
ath_set_tx_antenna_select(ath_dev_t dev, u_int32_t antenna_select)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    u_int8_t antenna_cfgd = 0;

    if (!sc->sc_cfg_txchainmask) {
        sc->sc_cfg_txchainmask = sc->sc_config.txchainmask;
        sc->sc_cfg_rxchainmask = sc->sc_config.rxchainmask;
        sc->sc_cfg_txchainmask_leg = sc->sc_config.txchainmasklegacy;
        sc->sc_cfg_rxchainmask_leg = sc->sc_config.rxchainmasklegacy;
    }

    if (ath_hal_setAntennaSwitch(sc->sc_ah, antenna_select, &sc->sc_curchan,
                                 &sc->sc_tx_chainmask, &sc->sc_rx_chainmask,
                                 &antenna_cfgd)) {
        if (antenna_cfgd) {
            if (antenna_select) {
                /* Overwrite chainmask configuration so that antenna selection is 
                 * retained during join.
                 */
                sc->sc_config.txchainmask = sc->sc_tx_chainmask;
                sc->sc_config.rxchainmask = sc->sc_rx_chainmask;
                sc->sc_config.txchainmasklegacy = sc->sc_tx_chainmask;
                sc->sc_config.rxchainmasklegacy = sc->sc_rx_chainmask;
            } else {
                /* Restore original chainmask config */
                sc->sc_config.txchainmask = sc->sc_cfg_txchainmask;
                sc->sc_config.rxchainmask = sc->sc_cfg_rxchainmask;
                sc->sc_config.txchainmasklegacy = sc->sc_cfg_txchainmask_leg;
                sc->sc_config.rxchainmasklegacy = sc->sc_cfg_rxchainmask_leg;
                sc->sc_cfg_txchainmask = 0;
            }
        }
        DPRINTF(sc, ATH_DEBUG_RESET, "%s: Tx Antenna Switch. Do internal reset.\n", __func__);
        ath_internal_reset(sc);
        return 0;
    }
    return -EIO;
}

static u_int32_t
ath_get_current_tx_antenna(ath_dev_t dev)
{
    return ATH_DEV_TO_SC(dev)->sc_cur_txant;
}

static u_int32_t
ath_get_default_antenna(ath_dev_t dev)
{
    return ATH_DEV_TO_SC(dev)->sc_defant;
}

#ifdef DBG
static u_int32_t
ath_register_read(ath_dev_t dev, u_int32_t offset)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return(ath_hal_readRegister(sc->sc_ah, offset));
}

static void
ath_register_write(ath_dev_t dev, u_int32_t offset, u_int32_t value)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_hal_writeRegister(sc->sc_ah, offset, value);
}
#endif

int
ath_setTxPwrAdjust(ath_dev_t dev, int32_t adjust, u_int32_t is2GHz)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int error = 0;

    if (!adjust)
        return 0;
    
    ATH_TXPOWER_LOCK(sc);
    if (is2GHz) {
        sc->sc_config.txpowupdown2G += adjust;
    } else {
        sc->sc_config.txpowupdown5G += adjust;
    }
    
    sc->sc_config.txpow_need_update = 1;
    ATH_TXPOWER_UNLOCK(sc);
    
    ath_update_txpow(sc, sc->tx_power);        /* update tx power state */
   
    
    return error;
}

int
ath_setTxPwrLimit(ath_dev_t dev, u_int32_t limit, u_int16_t tpcInDb, u_int32_t is2GHz)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int error = 0;
    int update_txpwr = 1;
    int i;
    struct ath_vap *avp;
    int active_vap = 0;

    if((sc->sc_curchan.channel == 0) ||
        (sc->sc_curchan.channelFlags == 0)) {
        /* no channel set */
        update_txpwr = 0;
    }
    /* check any active vaps ?? */
    for(i=0; i < sc->sc_nvaps; i++) {
        avp = sc->sc_vaps[i];
        if(avp) {
            if(avp->av_up) {
                active_vap = 1;
                break;
            }
        }
    }
    if(!active_vap) {
        /* no active vap */
        update_txpwr = 0;
    }

    if(update_txpwr) {
        if (is2GHz) {
            if (limit > ATH_TXPOWER_MAX_2G) {
                error = -EINVAL;
            } else {
                sc->sc_config.txpowlimit2G = (u_int16_t)limit;
            }
        } else {
            if (limit > ATH_TXPOWER_MAX_5G) {
                error = -EINVAL;
            } else {
            sc->sc_config.txpowlimit5G = (u_int16_t)limit;
            }
        }
    } else {
        if ((limit > ATH_TXPOWER_MAX_2G) ||
            (limit > ATH_TXPOWER_MAX_5G)) {
            error = -EINVAL;
        } else {
            /* reflect to both 2g and 5g txpowlimit */
            sc->sc_config.txpowlimit2G = (u_int16_t)limit;
            sc->sc_config.txpowlimit5G = (u_int16_t)limit;
        }
    }
    
    if (error == 0) {
        sc->tx_power = tpcInDb;
		if(update_txpwr)	
        	ath_update_txpow(sc, tpcInDb);
    }
    return error;
}

u_int8_t
ath_get_common_power(u_int16_t freq)
{
    return getCommonPower(freq);
}

#ifdef ATH_CHAINMASK_SELECT

static int
ath_chainmask_sel_timertimeout(void *timerArg)
{
    struct ath_chainmask_sel *cm = (struct ath_chainmask_sel *)timerArg;
    cm->switch_allowed = 1;
    return 1; /* no re-arm  */
}

/*
 * Start chainmask select timer
 */
static void
ath_chainmask_sel_timerstart(struct ath_chainmask_sel *cm)
{
    cm->switch_allowed = 0;
    ath_start_timer(&cm->timer);
}

/*
 * Stop chainmask select timer
 */
static void
ath_chainmask_sel_timerstop(struct ath_chainmask_sel *cm)
{
    cm->switch_allowed = 0;
    ath_cancel_timer(&cm->timer, CANCEL_NO_SLEEP);
}

int
ath_chainmask_sel_logic(struct ath_softc *sc, struct ath_node *an)
{
    struct ath_chainmask_sel *cm  = &an->an_chainmask_sel;

    /*
     * Disable auto-swtiching in one of the following if conditions.
     * sc_chainmask_auto_sel is used for internal global auto-switching
     * enabled/disabled setting
     */
    if ((sc->sc_no_tx_3_chains == AH_FALSE) ||
        (sc->sc_config.chainmask_sel == AH_FALSE)) {
        cm->cur_tx_mask = sc->sc_config.txchainmask;
        return cm->cur_tx_mask;
    }

    if (cm->tx_avgrssi == ATH_RSSI_DUMMY_MARKER) {
        return cm->cur_tx_mask;
    }

    if (cm->switch_allowed) {
        /* Switch down from tx 3 to tx 2. */
        if (cm->cur_tx_mask == ATH_CHAINMASK_SEL_3X3  &&
            ATH_RSSI_OUT(cm->tx_avgrssi) >= ath_chainmask_sel_down_rssi_thres) {
            cm->cur_tx_mask = sc->sc_config.txchainmask;

            /* Don't let another switch happen until this timer expires */
            ath_chainmask_sel_timerstart(cm);
        }
        /* Switch up from tx 2 to 3. */
        else if (cm->cur_tx_mask == sc->sc_config.txchainmask  &&
                 ATH_RSSI_OUT(cm->tx_avgrssi) <= ath_chainmask_sel_up_rssi_thres) {
            cm->cur_tx_mask =  ATH_CHAINMASK_SEL_3X3;

            /* Don't let another switch happen until this timer expires */
            ath_chainmask_sel_timerstart(cm);
        }
    }

    return cm->cur_tx_mask;
}

static void
ath_chainmask_sel_init(struct ath_softc *sc, struct ath_node *an)
{
    struct ath_chainmask_sel *cm  = &an->an_chainmask_sel;

    OS_MEMZERO(cm, sizeof(struct ath_chainmask_sel));

    cm->cur_tx_mask = sc->sc_config.txchainmask;
    cm->cur_rx_mask = sc->sc_config.rxchainmask;
    cm->tx_avgrssi = ATH_RSSI_DUMMY_MARKER;

    ath_initialize_timer(sc->sc_osdev, &cm->timer, ath_chainmask_sel_period,
                         ath_chainmask_sel_timertimeout, cm);
}

#endif

static int16_t
ath_get_noisefloor(ath_dev_t dev, u_int16_t    freq,  u_int chan_flags)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_CHANNEL hchan;
    
    hchan.channel = freq;
    hchan.channelFlags = chan_flags;

    return ah->ah_getChanNoise(ah,&hchan);
}

static void 
ath_get_chainnoisefloor(ath_dev_t dev, u_int16_t freq, u_int chan_flags, int16_t *nfBuf)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_CHANNEL hchan;
    
    hchan.channel = freq;
    hchan.channelFlags = chan_flags;

    /* If valid buffer is passed, populate individual chain nf values */
    if (nfBuf) {
        /* see if the specified channel is the home channel */

        ah->ah_getChainNoiseFloor(ah, nfBuf, &hchan, sc->sc_scanning);
    }
}

#if ATH_SUPPORT_RAW_ADC_CAPTURE
static int
ath_get_min_agc_gain(ath_dev_t dev, u_int16_t freq, int32_t *gain_buf, int num_gain_buf)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    if (ath_hal_getcapability(sc->sc_ah, HAL_CAP_RAW_ADC_CAPTURE, 0, NULL) != HAL_OK) {
        return -ENOTSUP;
    }

    if (ath_hal_get_min_agc_gain(ah, freq, gain_buf, num_gain_buf) != HAL_OK) {
        return -ENOTSUP;
    }
    return 0;
}
#endif

#ifdef MAGPIE_HIF_GMAC
OS_TIMER_FUNC (ath_host_seek_credit_timer)
{
    struct ath_softc *sc;

    OS_GET_TIMER_ARG(sc, struct ath_softc *);
    ath_wmi_seek_credit(sc);
}    
#endif

#ifdef ATH_SUPPORT_DFS
static
OS_TIMER_FUNC(ath_dfs_war_task)
{
    struct ath_softc *sc;
    
    OS_GET_TIMER_ARG(sc, struct ath_softc *); 
    OS_CANCEL_TIMER(&sc->sc_dfs->sc_dfs_war_timer);
    ath_hal_dfs3streamfix(sc->sc_ah, 0);
    sc->sc_3streamfix_engaged = 0;
}


static
OS_TIMER_FUNC(ath_radar_task)
{
        struct ath_softc *sc;
        struct ath_hal *ah;
        struct ieee80211_channel ichan;
        HAL_CHANNEL hchan;
        HAL_CHANNEL *ext_ch;
	int i;
       
        OS_GET_TIMER_ARG(sc, struct ath_softc *);
        ah = sc->sc_ah;
        ext_ch = ath_hal_get_extension_channel(ah);

        sc->sc_dfs->sc_rtasksched = 0;
        OS_CANCEL_TIMER(&sc->sc_dfs->sc_dfs_task_timer);
        if (dfs_process_radarevent(sc,&hchan)) {
                /*
                 * radar was detected on this channel, initiate channel change
                 */
                ichan.ic_ieee = ath_hal_mhz2ieee(ah, hchan.channel, hchan.channelFlags);
                ichan.ic_freq = hchan.channel;
                ichan.ic_flags = hchan.channelFlags;

                /* do channel switch only when usenol is set to true -
                 * otherwise, for test mode 
                 * disable channel switch - as per owl_sw_dev
                 */
                if (sc->sc_dfs->dfs_rinfo.rn_use_nol == 1) {
                    /* If HT40 mode, always mark radar detection for both 
                       primary and extension channels */
                    sc->sc_ieee_ops->ath_net80211_mark_dfs(sc->sc_ieee, &ichan);
                    if (ext_ch) {
                        struct ieee80211_channel iextchan;
                        iextchan.ic_ieee = ath_hal_mhz2ieee(ah, ext_ch->channel, ext_ch->channelFlags);
                        iextchan.ic_freq = ext_ch->channel;
                        iextchan.ic_flags = ext_ch->channelFlags;
                        sc->sc_ieee_ops->ath_net80211_mark_dfs(sc->sc_ieee, &iextchan);
                    }
                    /* Flush the TX data queues */
                    ath_tx_flush(sc);
                }

        if ((sc->sc_dfs->dfs_rinfo.rn_use_nol != 1) &&
                    (sc->sc_opmode == HAL_M_HOSTAP)) {
                /* TEST : To hold to the same channel, though detected RADAR */
                /* The actual channel change will be done in ieee80211_mark_dfs, 
                   but schedule the timer that will return us to the current channel 
                   here */
                       printk("Radar found on channel %d (%d MHz)\n",
                               ichan.ic_ieee,ichan.ic_freq);
                       sc->sc_dfs->sc_dfstest_ieeechan = ichan.ic_ieee;
                       sc->sc_dfs->sc_dfstest=1;
                       OS_CANCEL_TIMER(&sc->sc_dfs->sc_dfstesttimer);
                       OS_SET_TIMER(&sc->sc_dfs->sc_dfstesttimer, sc->sc_dfs->sc_dfstesttime);

		       /*
			* Mark the flags for all tid, so BAR will be sent.
			* This is WAR of the issue where the starting sequence is out of sync during
			* DFS testing due to lost of Block ACK.
			*/
		       for (i=0; i<WME_NUM_TID; i++)
		           sc->sc_dfs_radar_detected[i] = true;
                }
        }
}

static
OS_TIMER_FUNC(ath_dfs_test_return)
{
    struct ath_softc *sc;
    OS_GET_TIMER_ARG(sc, struct ath_softc *);
    sc->sc_dfs->sc_dfstest = 0;
    if (sc->sc_dfs->dfs_rinfo.rn_use_nol == 0) {
        sc->sc_ieee_ops->ath_net80211_dfs_test_return(sc->sc_ieee, sc->sc_dfs->sc_dfstest_ieeechan);
    }
}

int 
ath_dfs_control(ath_dev_t dev, u_int id,
                void *indata, u_int32_t insize,
                void *outdata, u_int32_t *outsize)
{
        int error=0;

        error = dfs_control(dev, id, indata, insize, outdata, outsize);
        return error;
}


/*
 * periodically checks for the hal to set
 * CHANNEL_DFS_CLEAR flag on current channel.
 * if the flag is set and a vap is waiting for it ,push
 * transition the vap to RUN state.
 */
static
OS_TIMER_FUNC(ath_check_dfs_clear)
{
        struct ath_softc *sc;
        HAL_CHANNEL hchan;
        int j, i;

        OS_GET_TIMER_ARG(sc, struct ath_softc *);

        if(!sc->sc_dfs->sc_dfswait) return;

        /* Check with HAL, HAL clears the relevant channel flags if 
           channel is good to go. */
        ath_hal_radar_wait(sc->sc_ah, &hchan);

        if(hchan.privFlags & CHANNEL_INTERFERENCE) return;

        if ((hchan.privFlags & CHANNEL_DFS_CLEAR) ||
                (!(hchan.privFlags & CHANNEL_DFS))) {

                /* Bug 32403 - Once the CAC is over, clear the DFS_CLEAR flag,
                   so that every single time a DFS channel is set, CAC will 
                   happen, irrespective of whether channel has been used before*/
                sc->sc_curchan.privFlags &= ~CHANNEL_DFS_CLEAR;
                sc->sc_dfs->sc_dfswait=0;
                ath_hal_dfs_cac_war (sc->sc_ah, 0);
                ath_hal_cac_tx_quiet(sc->sc_ah, 0);
                printk("End of DFS wait period\n");
                for (j = i = 0; j < sc->sc_nvaps; j++, i++) {

                        /* 
                        ** This will change later, an event will be sent to all
                        ** interested VAPs saying DFS wait is now clear and the
                        ** VAP will set its state to RUN after that.
                        */

                        struct ath_vap *avp = sc->sc_vaps[i];

                        /*
                         * Available vaps need not be in order. Multiple VAPs could have
                         * been created, and the first one torn down.
                         */
                        if (!avp) {
                                j--;
                                continue;
                        }

                        if(avp->av_dfswait_run) {
                            /* re alloc beacons to update new channel info */
                            int error;
                            error = ath_beacon_alloc(sc, i);
                            if(error) {
                                       printk("%s error allocating beacon\n", __func__);
                                       return;
                            }

                            /*
                            ** Pass the VAP ID number, gets resolved inside call
                            */

                            sc->sc_ieee_ops->ath_net80211_set_vap_state(sc->sc_ieee,i,LMAC_STATE_EVENT_DFS_CLEAR);
#ifdef ATH_SUPERG_XR
                            if(vap->iv_flags & IEEE80211_F_XR ) {
                                       u_int32_t rfilt=0;
                                       rfilt = ath_calcrxfilter(sc);
                                       ATH_SETUP_XR_VAP(sc,vap,rfilt);
                            }
#endif
                        avp->av_dfswait_run=0;
                        } else {
                            /* Integrate bug fix for 36126. Change VAP state
                               to SCAN or RUN depending on whether the VAP 
                               started the DFS wait*/
                            sc->sc_ieee_ops->ath_net80211_set_vap_state(sc->sc_ieee,i,LMAC_STATE_EVENT_UP);
                        }
                }
        } else {
                /* fire the timer again */
                    sc->sc_dfs->sc_dfswait=1;
                    ath_hal_cac_tx_quiet(sc->sc_ah, 1);
                    OS_CANCEL_TIMER(&sc->sc_dfs->sc_dfswaittimer);
                    OS_SET_TIMER(&sc->sc_dfs->sc_dfswaittimer, ATH_DFS_WAIT_POLL_PERIOD_MS);
        }
}
#if ATH_SUPPORT_IBSS_DFS
static void ath_ibss_beacon_update_start(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    ath_hal_stoptxdma(ah, sc->sc_bhalq, 0);

    sc->sc_hasveol = 0;
    sc->sc_imask |= HAL_INT_SWBA;
}

static void ath_ibss_beacon_update_stop(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    sc->sc_hasveol = 1;
    sc->sc_imask &= ~HAL_INT_SWBA;

    /* If the channel is not DFS */
    if ((sc->sc_curchan.privFlags & CHANNEL_DFS) == 0) {
        ath_beacon_config(sc,ATH_BEACON_CONFIG_REASON_RESET, ATH_IF_ID_ANY);
    }
}
#endif /* ATH_SUPPORT_IBSS_DFS */
#endif

u_int32_t
ath_gettsf32(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t retval;

    ATH_PS_WAKEUP(sc);
    retval=ath_hal_gettsf32(ah);
    ATH_PS_SLEEP(sc);

    return retval;
}

u_int64_t
ath_gettsf64(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    u_int64_t retval;

    ATH_PS_WAKEUP(sc);
    retval=ath_hal_gettsf64(ah);
    ATH_PS_SLEEP(sc);

    return retval;
}

void
ath_setrxfilter(ath_dev_t dev)
{
    struct ath_softc    *sc = ATH_DEV_TO_SC(dev);
    u_int32_t           rxfilt;

    rxfilt = ath_calcrxfilter(sc);
    ath_hal_setrxfilter(sc->sc_ah, rxfilt);
}

#ifdef ATH_CCX
int
ath_update_mib_macstats(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    ATH_PS_WAKEUP(sc);
    ath_hal_updateMibMacStats(ah);
    ATH_PS_SLEEP(sc);

    return 0;
}

int
ath_get_mib_macstats(ath_dev_t dev, struct ath_mib_mac_stats *pStats)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    if (pStats == NULL) {
        return -EINVAL;
    }

    ath_hal_getMibMacStats(ah, (HAL_MIB_STATS*)pStats);

#ifdef ATH_SUPPORT_HTC
    ath_wmi_get_target_stats(dev, &pStats->tgt_stats);
#endif

    return 0;
}

int
ath_get_mib_cyclecounts(ath_dev_t dev, struct ath_mib_cycle_cnts *pCnts)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    if (pCnts == NULL) {
        return -EINVAL;
    }

    ath_hal_getMibCycleCounts(ah, (HAL_COUNTERS*)pCnts);
    return 0;
}

void
ath_clear_mib_counters(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    ath_hal_clearMibCounters(ah);
}

int
ath_getserialnumber(ath_dev_t dev, u_int8_t *pSerNum, int limit)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    if (pSerNum == NULL) {
        return -EINVAL;
    }

    return ath_hal_get_sernum(ah, pSerNum, limit);
}

int
ath_getchandata(ath_dev_t dev, struct ieee80211_channel *pChan, struct ath_chan_data *pData)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    if (ath_hal_get_chandata(ah, (HAL_CHANNEL*)pChan, (HAL_CHANNEL_DATA*)pData) == AH_TRUE){
        return 0;
    } else {
        return -EINVAL;
    }
}

u_int32_t
ath_getcurrssi(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    return ath_hal_getCurRSSI(ah);
}
#endif /* ATH_CCX */


void
ath_do_pwrworkaround(ath_dev_t dev, u_int16_t channel,  u_int32_t channel_flags, u_int16_t opmodeSta )
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_CHANNEL dummychan;     /* used for ath hal reset */

    /*
     * re program the PCI powersave regs.
     */
    ath_hal_configpcipowersave(ah, 0, 0);

    /*
     * Power consumption WAR to keep the power consumption
     * minimum when loaded first time and we are in OFF state.
     * ath hal reset requires a valid channel in order to reset.
     * Do this if in STA mode.
     */    
    if (opmodeSta) { 
        ATH_PWRSAVE_STATE power;
        HAL_STATUS        status;

        // ATH_LOCK(sc->sc_osdev);
        
        power = ath_pwrsave_get_state(dev);
        ath_pwrsave_awake(sc);
        dummychan.channel = channel;
        dummychan.channelFlags = channel_flags;
        if (!ath_hal_reset(ah, sc->sc_opmode, &dummychan,
                           sc->sc_ieee_ops->cwm_macmode(sc->sc_ieee), 
                           sc->sc_tx_chainmask, sc->sc_rx_chainmask,
                           sc->sc_ht_extprotspacing, AH_FALSE, &status,
                           sc->sc_scanning))
        {
            printk("%s: unable to reset hardware; hal status %u\n", __func__, status);
        }
        ath_pwrsave_set_state(sc, power); 
        // ATH_UNLOCK(sc->sc_osdev);
    }

}

void
ath_get_txqproperty(ath_dev_t dev, u_int32_t q_id,  u_int32_t property, void *retval)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_TXQ_INFO qi;

    ath_hal_gettxqueueprops(ah, q_id, &qi);

    switch (property) {
    case TXQ_PROP_SHORT_RETRY_LIMIT:
        *(u_int16_t *)retval = qi.tqi_shretry;
        break;
    case TXQ_PROP_LONG_RETRY_LIMIT:
        *(u_int16_t *)retval = qi.tqi_lgretry;
        break;
    }
}


void
ath_set_txqproperty(ath_dev_t dev, u_int32_t q_id,  u_int32_t property, void *value)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    HAL_TXQ_INFO qi;

    qi.tqi_shretry = HAL_TQI_NONVAL;
    qi.tqi_lgretry = HAL_TQI_NONVAL;
    qi.tqi_cbrPeriod = HAL_TQI_NONVAL;
    qi.tqi_cbrOverflowLimit = HAL_TQI_NONVAL;
    qi.tqi_burstTime = HAL_TQI_NONVAL;
    qi.tqi_readyTime = HAL_TQI_NONVAL;

    switch (property) {
    case TXQ_PROP_SHORT_RETRY_LIMIT:
        qi.tqi_shretry = *(u_int16_t *)value;
        break;
    case TXQ_PROP_LONG_RETRY_LIMIT:
        qi.tqi_lgretry = *(u_int16_t *)value;
        break;
    }
    ath_hal_settxqueueprops(ah, q_id, &qi);
}

void
ath_get_hwrevs(ath_dev_t dev, struct ATH_HW_REVISION *hwrev)
{
    //ARTODO by MR: finish this function
}

u_int32_t
ath_rc_rate_maprix(ath_dev_t dev, u_int16_t curmode, int isratecode)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return ath_rate_maprix(sc, curmode, isratecode);
}

int
ath_rc_rate_set_mcast_rate(ath_dev_t dev, u_int32_t req_rate)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return ath_rate_set_mcast_rate(sc, req_rate);
}


void ath_set_diversity(ath_dev_t dev, u_int diversity)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    ath_hal_setdiversity(sc->sc_ah, diversity);
    sc->sc_diversity = diversity;

}
void ath_set_rx_chainmask(ath_dev_t dev, u_int8_t mask)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    sc->sc_config.rxchainmask = sc->sc_rx_chainmask = mask;
    sc->sc_rx_numchains = sc->sc_mask2chains[sc->sc_rx_chainmask];

    printk("%s[%d] sc->sc_config.rxchainmask %d mask %d\n", __func__, __LINE__,
        sc->sc_config.rxchainmask, mask);

}
void ath_set_tx_chainmask(ath_dev_t dev, u_int8_t mask)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    sc->sc_config.txchainmask = sc->sc_tx_chainmask = mask;
    sc->sc_tx_numchains = sc->sc_mask2chains[sc->sc_tx_chainmask];

    printk("%s[%d] sc->sc_config.txchainmask %d mask %d\n", __func__, __LINE__,
        sc->sc_config.txchainmask, mask);
}

void ath_set_rx_chainmasklegacy(ath_dev_t dev, u_int8_t mask)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    sc->sc_config.rxchainmasklegacy = sc->sc_rx_chainmask = mask;
    sc->sc_rx_numchains = sc->sc_mask2chains[sc->sc_rx_chainmask];
}
void ath_set_tx_chainmasklegacy(ath_dev_t dev, u_int8_t mask)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    sc->sc_config.txchainmasklegacy = sc->sc_tx_chainmask = mask;
    sc->sc_tx_numchains = sc->sc_mask2chains[sc->sc_tx_chainmask];
}

void ath_get_maxtxpower(ath_dev_t dev, u_int32_t *txpow)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_hal_getmaxtxpow(sc->sc_ah, txpow);
}

void ath_read_from_eeprom(ath_dev_t dev, u_int16_t address, u_int32_t len, u_int16_t **value, u_int32_t *bytesread)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_hal_getdiagstate(sc->sc_ah, 17 /*HAL_DIAG_EEREAD*/, (void *) &address, len, (void **)value, bytesread);
}

void ath_pkt_log_text (ath_dev_t dev,  u_int32_t iflags, const char *fmt, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
#ifndef REMOVE_PKT_LOG    
    ath_log_text(dev, buf, iflags);
#endif    
}

void ath_fixed_log_text (ath_dev_t dev, const char *text)
{
#ifndef REMOVE_PKT_LOG    
    ath_log_text(dev, (char *)text, 0);
#endif    
}

int ath_rate_newstate_set(ath_dev_t dev, int index, int up)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_rate_newstate(sc, sc->sc_vaps[index], up);
    return 0;
}

u_int8_t ath_rate_findrc(ath_dev_t dev, u_int8_t rate)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    const HAL_RATE_TABLE *rt = sc->sc_rates[sc->sc_curmode];
    u_int8_t i, rc = 0;

    for(i=0; i<rt->rateCount; i++) {
        if ((rt->info[i].dot11Rate & IEEE80211_RATE_VAL) == rate) {
            rc = rt->info[i].rateCode;
            break;
        }
    }
    return rc;
}

static void
ath_printreg(ath_dev_t dev, u_int32_t printctrl)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    ATH_PS_WAKEUP(sc);

    ath_hal_getdiagstate(ah, HAL_DIAG_PRINT_REG, &printctrl, sizeof(printctrl), NULL, 0);

    ATH_PS_SLEEP(sc);
}

u_int32_t
ath_getmfpsupport(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t        mfpsupport;
    ath_hal_getmfpsupport(ah, &mfpsupport);
    switch(mfpsupport) {
    case HAL_MFP_QOSDATA:
        return ATH_MFP_QOSDATA;
    case HAL_MFP_PASSTHRU:
        return ATH_MFP_PASSTHRU;
    case HAL_MFP_HW_CRYPTO:
        return ATH_MFP_HW_CRYPTO;
    default:
        return ATH_MFP_QOSDATA;
    };
}

int
ath_spectral_control(ath_dev_t dev, u_int id,
                void *indata, u_int32_t insize,
                void *outdata, u_int32_t *outsize)
{
    int error=0;
    switch (id) {
#if ATH_SUPPORT_RAW_ADC_CAPTURE
        case SPECTRAL_ADC_ENABLE_TEST_ADDAC_MODE:
            error = spectral_enter_raw_capture_mode(dev, indata);
            break;
        case SPECTRAL_ADC_DISABLE_TEST_ADDAC_MODE:
            error = spectral_exit_raw_capture_mode(dev);
            break;
        case SPECTRAL_ADC_RETRIEVE_DATA:
            error = spectral_retrieve_raw_capture(dev, outdata, outsize); 
            break;
#endif
        default:
#if ATH_SUPPORT_SPECTRAL
            if (id == SPECTRAL_ACTIVATE_SCAN || id == SPECTRAL_CLASSIFY_SCAN || id == SPECTRAL_ACTIVATE_FULL_SCAN)
                ATH_PS_WAKEUP(ATH_DEV_TO_SC(dev));
	    error = spectral_control(dev, id, indata, insize, outdata, outsize);
            if (id == SPECTRAL_STOP_SCAN || id == SPECTRAL_STOP_FULL_SCAN)
                ATH_PS_SLEEP(ATH_DEV_TO_SC(dev));
#endif
            break;
    }

    return error;
}

#if ATH_SUPPORT_SPECTRAL
void
ath_spectral_indicate(struct ath_softc *sc, void* spectral_buf, u_int32_t buf_size)
{
    if (sc->sc_ieee_ops->spectral_indicate)
        sc->sc_ieee_ops->spectral_indicate(sc->sc_ieee, spectral_buf, buf_size);
}

int ath_dev_get_ctldutycycle(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return get_eacs_control_duty_cycle(sc);
}

int ath_dev_get_extdutycycle(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return get_eacs_extension_duty_cycle(sc);
}

void ath_dev_start_spectral_scan(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_spectral *spectral = sc->sc_spectral;

    ATH_PS_WAKEUP(sc);
    SPECTRAL_LOCK(spectral);  
    start_spectral_scan(sc);
    SPECTRAL_UNLOCK(spectral);  
}

void ath_dev_stop_spectral_scan(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_spectral *spectral = sc->sc_spectral;

    SPECTRAL_LOCK(spectral);  
    stop_current_scan(sc);
    SPECTRAL_UNLOCK(spectral);  
    ATH_PS_SLEEP(sc);
}
#endif /* ATH_SUPPORT_SPECTRAL */

/*
 * pause/unpause dat trafic on HW queues for the given vap(s).
 * if pause is true then perform pause operation.
 * if pause is false then perform unpause operation.
 * if vap is null the perform the requested operation on all the vaps.
 * if vap is non null the perform the requested operation on the vap.
 */
static void  ath_vap_pause_control(ath_dev_t dev,int if_id, bool pause )
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_vap *avp=NULL;

    if (if_id != ATH_IF_ID_ANY) {
        avp = sc->sc_vaps[if_id];
    }
    ath_tx_vap_pause_control(sc,avp,pause);
}

/*
 * if pause == true then pause the node (pause all the tids )
 * if pause == false then unpause the node (unpause all the tids )
 */
static void  ath_node_pause_control(ath_dev_t dev,ath_node_t node, bool pause )
{

    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = ATH_NODE(node);
printk("%s %p %s\n", __func__, an, (pause)?"PAUSE":"UNPAUSE");

    if (pause) {
        ath_tx_node_pause(sc,an);
    } else {
        ath_tx_node_resume(sc,an);
    }

}

/*
 * if enable == true then enable rx rifs
 * if enable == false then diasable rx rifs
 */
static void ath_set_rifs(ath_dev_t dev, bool enable)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_hal_set_rifs(sc->sc_ah, enable);
}

#if UMAC_SUPPORT_INTERNALANTENNA
/*
 *  All hardware related initilization for smart antenna are done here
 *  TODO: This function needs to be modfied for Ospery 2.3
 */
void ath_enable_smartantenna(struct ath_softc *sc)
{   
#define ANT_MASK 0x0000000f
    u_int32_t antenna = 0;
    bool is_2ghz = 0;

    switch(sc->sc_curmode) 
    {
        case WIRELESS_MODE_11g:
        case WIRELESS_MODE_108g:
        case WIRELESS_MODE_11NG_HT20:
        case WIRELESS_MODE_11NG_HT40PLUS:
        case WIRELESS_MODE_11NG_HT40MINUS:
            is_2ghz = 1;
            break;
        default:
            is_2ghz = 0;
            break;
     }

    /* do hardware level enable */
    ath_hal_gpioCfgOutput(sc->sc_ah,HAL_GPIOPIN_SMARTANT_CTRL0, HAL_GPIO_OUTPUT_MUX_AS_SMARTANT_CTRL0);
    ath_hal_gpioCfgOutput(sc->sc_ah,HAL_GPIOPIN_SMARTANT_CTRL1, HAL_GPIO_OUTPUT_MUX_AS_SMARTANT_CTRL1);
    ath_hal_gpioCfgOutput(sc->sc_ah,HAL_GPIOPIN_SMARTANT_CTRL2, HAL_GPIO_OUTPUT_MUX_AS_SMARTANT_CTRL2);
    
    /* enable bit 0 of MAC_PCU_XRTO to 1 */
    ath_hal_set_smartantenna(sc->sc_ah, 1);

    /* Get default TX/RX antennas configuration from Hardware and Program */
    antenna = ath_hal_ant_ctrl_common_get(sc->sc_ah, is_2ghz);
    antenna = antenna & ANT_MASK;
    ath_setdefantenna(sc, antenna);
    sc->sc_smartant_enable = 1;
#undef ANT_MASK    
}

/*
 *  All hardware related shutdown or smart antenna are done here
 *  TODO: This function needs to be modfied for Ospery 2.3
 */
void ath_disable_smartantenna(struct ath_softc *sc)
{   
    /* do hardware level disable */

    ath_hal_set_smartantenna(sc->sc_ah, 0);
    sc->sc_smartant_enable = 0;
}

static void ath_set_selected_smantennas(ath_node_t node, int antenna, int rtset)
{
    struct ath_node *an = (struct ath_node *) node;
    an->selected_smart_antenna[rtset] = antenna;
}
#endif

#if UMAC_SUPPORT_SMARTANTENNA
/*
 *  Prepare rateset for smart antenna training
 */
static void
ath_smartant_prepare_rateset(ath_dev_t dev, ath_node_t node, void *prtset)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = (struct ath_node *) node;

    return ath_rate_smartant_prepare_rateset(sc, an, prtset);
}

/*
 * Total free buffer avaliable in common pool of buffers
 */
static u_int32_t ath_get_txbuf_free(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return sc->sc_txbuf_free;
}

/*
 * Number of used buffer used per queue
 */
static u_int32_t ath_get_txbuf_used(ath_dev_t dev, int ac)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return sc->sc_txq[ac].axq_num_buf_used;
}

static int8_t ath_get_smartantenna_enable(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    return sc->sc_smartant_enable;
}


/*
 * Rate used stats for transmition in two consecutive retraining intervals
 */

static void ath_get_smartant_ratestats(ath_node_t node, void *rate_stats)
{
#define MAX_HT_RATES 24 
    struct ath_node *an = ATH_NODE(node);
    /* Copy stats to umac smart antenna module */
    OS_MEMCPY((u_int32_t *)rate_stats, (u_int32_t *)&an->sm_ratestats[0], MAX_HT_RATES * sizeof(u_int32_t));
    /* Clear the results */
    OS_MEMZERO(&an->sm_ratestats[0], MAX_HT_RATES * sizeof(u_int32_t));
}

#endif
