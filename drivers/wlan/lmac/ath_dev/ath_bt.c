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
 */

#include "ath_internal.h"
#include "ath_bt.h"

#ifdef ATH_BT_COEX
#include "ieee80211_var.h" /* XXX to do: fix layer violation, using to resolve IEEE80211_BMISS_LIMIT */

/*
 * WLAN events processing
 */
void ath_bt_coex_connect(struct ath_softc *sc);
void ath_bt_coex_disconnect(struct ath_softc *sc);
void ath_bt_coex_reset_start(struct ath_softc *sc);
void ath_bt_coex_reset_end(struct ath_softc *sc);
void ath_bt_coex_scan(struct ath_softc *sc, u_int32_t scanning);
void ath_bt_coex_assoc(struct ath_softc *sc, u_int32_t assoc_state);
void ath_bt_coex_rssi_update(struct ath_softc *sc, int8_t *rssi);
void ath_bt_coex_fullsleep(struct ath_softc *sc);
void ath_bt_coex_aggr_frame_len(struct ath_softc *sc, u_int32_t *frameLen);

/*
 * BT coex agent events processing
 */
void ath_bt_coex_bt_inquiry(struct ath_softc *sc, u_int32_t state);
void ath_bt_coex_bt_connect(struct ath_softc *sc, u_int32_t state);
void ath_bt_coex_bt_profile(struct ath_softc *sc, ATH_COEX_PROFILE_INFO *pInfo);
void ath_bt_coex_bt_profile_type_update(struct ath_softc *sc, ATH_COEX_PROFILE_TYPE_INFO *pTypeInfo);
void ath_bt_coex_bt_role_switch(struct ath_softc *sc, ATH_COEX_ROLE_INFO *pRoleInfo);
void ath_bt_coex_bt_rcv_rate(struct ath_softc *sc, ATH_COEX_BT_RCV_RATE *pRate);

/*
 * Local functions
 */
static u_int32_t ath_bt_coex_sco_stomp(struct ath_softc *sc, u_int32_t value);
static HAL_BOOL ath_bt_coex_gpio_intr_3wire(struct ath_softc *sc);
static void ath_bt_coex_stop_scheme(struct ath_softc *sc);
static void ath_bt_coex_clear_scheme_info(ATH_COEX_SCHEME_INFO *schemeInfo);
static HAL_BOOL ath_bt_coex_is_same_scheme(ATH_COEX_SCHEME_INFO *schemeX, ATH_COEX_SCHEME_INFO *schemeY);
static void ath_bt_coex_adjust_aggr_limit(struct ath_softc *sc, ATH_COEX_SCHEME_INFO *scheme);
static void ath_bt_coex_lower_ACK_power(struct ath_softc *sc, ATH_BT_COEX_LOW_ACK_FLAG flag);

static void ath_bt_coex_bt_profile_add(struct ath_softc *sc, ATH_COEX_PROFILE_INFO *pInfo);
static void ath_bt_coex_bt_profile_delete(struct ath_softc *sc, ATH_COEX_PROFILE_INFO *pInfo);
static void ath_bt_coex_bt_active_profile(struct ath_softc *sc);

void ath_bt_coex_slave_pan_update(struct ath_softc *sc);

#ifdef ATH_USB
void ath_bt_coex_throughput(struct ath_softc *sc, u_int8_t enable);
extern void ath_wmi_set_btcoex(struct ath_softc *sc, u_int8_t dutyCycle, u_int16_t period, u_int8_t stompType);
#endif

/*
 * static settings for different BT modules
 */
static HAL_BT_COEX_CONFIG ath_bt_configs[HAL_MAX_BT_MODULES] = {
/* TIME_EXT     TXSTATE_EXT        TXFRM_EXT   MODE
 * QUIET        RXCLEAR_POLARITY   PRI_TIME    FST_SLOTTIME   HLD_RXCLEAR */

    /* HAL_BT_MODULE_CSR_V4 */
    {0,         AH_TRUE,           AH_TRUE,    HAL_BT_COEX_MODE_LEGACY,
     AH_FALSE,  AH_TRUE,           5,          29,            AH_TRUE},

    /* HAL_BT_MODULE_JANUS */
    {0,         AH_TRUE,           AH_TRUE,    HAL_BT_COEX_MODE_SLOTTED,
     AH_TRUE,   AH_TRUE,           2,          5,            AH_TRUE},

    /* HAL_BT_MODULE_HELIUS */
    {0,         AH_TRUE,           AH_TRUE,    HAL_BT_COEX_MODE_SLOTTED,
     AH_TRUE,   AH_TRUE,           2,          3,            AH_TRUE},
};

static void 
ath_bt_coex_bt_profile_add(struct ath_softc *sc, ATH_COEX_PROFILE_INFO *pInfo)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    int i;

    DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: the ACL connection handler is %d\n", __func__, pInfo->connHandle);

    for (i=0;i<MAX_NUM_BT_ACL_PROFILE;i++) {
        if ((btinfo->pList[i].connHandle == INVALID_PROFILE_TYPE_HANDLE) && (btinfo->pList[i].profileType == INVALID_PROFILE_TYPE_HANDLE)) {
            btinfo->pList[i].connHandle = pInfo->connHandle;
            btinfo->pList[i].connectionRole = pInfo->connectionRole;
            btinfo->pList[i].profileType = ATH_COEX_BT_PROFILE_ACL;
            break;
        }
        else if (btinfo->pList[i].connHandle == pInfo->connHandle) {
            break;
        }
    }

}

static void 
ath_bt_coex_bt_profile_delete(struct ath_softc *sc, ATH_COEX_PROFILE_INFO *pInfo)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    int i,j;

    DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: the ACL connection handler is %d\n", __func__, pInfo->connHandle);

    for (i=0;i<MAX_NUM_BT_ACL_PROFILE;i++) {
        if ((btinfo->pList[i].connHandle == INVALID_PROFILE_TYPE_HANDLE) && (btinfo->pList[i].profileType == INVALID_PROFILE_TYPE_HANDLE)) {
            DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: the handle is not a valid one %d\n", __func__, pInfo->connHandle);
            break;
        }
        else if (btinfo->pList[i].connHandle == pInfo->connHandle) {
            for (j=i+1;j<MAX_NUM_BT_ACL_PROFILE;j++) {
                btinfo->pList[j-1].connHandle = btinfo->pList[j].connHandle;
                btinfo->pList[j-1].profileType = btinfo->pList[j].profileType;
                btinfo->pList[j-1].connectionRole = btinfo->pList[j].connectionRole;
                if ((btinfo->pList[j].connHandle == INVALID_PROFILE_TYPE_HANDLE) && (btinfo->pList[j].profileType == INVALID_PROFILE_TYPE_HANDLE)) {
                    break;
                }
            }
            btinfo->pList[j-1].connHandle = INVALID_PROFILE_TYPE_HANDLE;
            btinfo->pList[j-1].profileType = INVALID_PROFILE_TYPE_HANDLE;
            btinfo->pList[j-1].connectionRole = INVALID_PROFILE_TYPE_HANDLE;
            break;
        }
    }

    ath_bt_coex_bt_active_profile(sc);
}

static void ath_bt_coex_bt_active_profile(struct ath_softc *sc)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    int i;

    btinfo->activeProfile = 0;
 
    for (i=0;i<MAX_NUM_BT_ACL_PROFILE;i++) {
        if ((btinfo->pList[i].connHandle == INVALID_PROFILE_TYPE_HANDLE) && (btinfo->pList[i].profileType == INVALID_PROFILE_TYPE_HANDLE)) {
            break;
        }
        else {
            if (btinfo->pList[i].profileType & ATH_COEX_BT_PROFILE_HID) {
                btinfo->activeProfile |= ATH_COEX_BT_PROFILE_HID;
                continue;
            }
            if (btinfo->pList[i].profileType & ATH_COEX_BT_PROFILE_PAN) {
                btinfo->activeProfile |= ATH_COEX_BT_PROFILE_PAN;
                continue;
            }
            if (btinfo->pList[i].profileType & ATH_COEX_BT_PROFILE_A2DP) {
                btinfo->activeProfile |= ATH_COEX_BT_PROFILE_A2DP;
                continue;
            }
            btinfo->activeProfile |= ATH_COEX_BT_PROFILE_ACL;
        }
    }

    if (BT_COEX_ACTIVE_PROFILE(btinfo, ATH_COEX_BT_PROFILE_HID) && (BT_COEX_LOWER_ACK_PWR_FLAG_CHECK(btinfo, BT_COEX_LOWER_ACK_PWR_HID) == 0)) {
        ath_bt_coex_lower_ACK_power(sc, ATH_BT_COEX_HID_ON);
    }
    if ((!BT_COEX_ACTIVE_PROFILE(btinfo, ATH_COEX_BT_PROFILE_HID)) && BT_COEX_LOWER_ACK_PWR_FLAG_CHECK(btinfo, BT_COEX_LOWER_ACK_PWR_HID)) {
        ath_bt_coex_lower_ACK_power(sc, ATH_BT_COEX_HID_OFF);
    }

    if (BT_COEX_ACTIVE_PROFILE(btinfo, ATH_COEX_BT_PROFILE_A2DP) && 
        (BT_COEX_LOWER_ACK_PWR_FLAG_CHECK(btinfo, BT_COEX_LOWER_ACK_PWR_A2DP) == 0)) 
    {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: A2DP is on\n", __func__);
        ath_bt_coex_lower_ACK_power(sc, ATH_BT_COEX_A2DP_ON);
    }
    if (BT_COEX_LOWER_ACK_PWR_FLAG_CHECK(btinfo, BT_COEX_LOWER_ACK_PWR_A2DP) && 
        (!BT_COEX_ACTIVE_PROFILE(btinfo, ATH_COEX_BT_PROFILE_A2DP))) 
    {
        ath_bt_coex_lower_ACK_power(sc, ATH_BT_COEX_A2DP_OFF);
    }

    ath_bt_coex_slave_pan_update(sc);
}

static void
ath_bt_coex_enable_prot_mode(struct ath_softc *sc, u_int8_t chainmask)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    struct ath_hal *ah = sc->sc_ah;

    /* Disable GPIO interrupt */
    if (btinfo->bt_gpioIntEnabled) {
        ath_hal_gpioSetIntr(ah, btinfo->bt_hwinfo.bt_gpioBTActive, HAL_GPIO_INTR_DISABLE);
        btinfo->bt_gpioIntEnabled = AH_FALSE;
    }

    /* Update chain mask */
    sc->sc_config.txchainmasklegacy = sc->sc_config.txchainmask = chainmask;
    sc->sc_config.rxchainmasklegacy = sc->sc_config.rxchainmask = chainmask;
    sc->sc_tx_chainmask = sc->sc_config.txchainmask;
    sc->sc_rx_chainmask = sc->sc_config.rxchainmask;
    sc->sc_rx_numchains = sc->sc_mask2chains[sc->sc_rx_chainmask];
    sc->sc_tx_numchains = sc->sc_mask2chains[sc->sc_tx_chainmask];
    ath_internal_reset(sc);
    sc->sc_ieee_ops->rate_newstate(sc->sc_ieee, sc->sc_vaps[0]->av_if_data);
}

static int
ath_bt_coex_active_check(struct ath_softc *sc)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    //struct ath_hal *ah = sc->sc_ah;

    if (!btinfo->bt_initStateDone) {
        btinfo->bt_initStateTime++;
        if (btinfo->bt_protectMode == ATH_BT_PROT_MODE_NONE) {
            if (btinfo->bt_initStateTime >= BT_COEX_INIT_STATE_TIME) {
                btinfo->bt_initStateDone = AH_TRUE;
            }
            if (btinfo->bt_activeCount >= BT_COEX_BT_ACTIVE_THRESHOLD) {
                btinfo->bt_timeOverThre++;
            }
            else {
                btinfo->bt_timeOverThre = 0;
            }
            if (btinfo->bt_activeCount >= BT_COEX_BT_ACTIVE_THRESHOLD2) {
                btinfo->bt_timeOverThre2++;
            }
            else {
                btinfo->bt_timeOverThre2 = 0;
            }
            if (btinfo->bt_timeOverThre >= BT_COEX_PROT_MODE_ON_TIME) {
                btinfo->bt_protectMode = ATH_BT_PROT_MODE_DEFER;
            }
        }
        else if (btinfo->bt_protectMode == ATH_BT_PROT_MODE_DEFER) {
            if (btinfo->bt_activeCount >= BT_COEX_BT_ACTIVE_THRESHOLD2) {
                btinfo->bt_timeOverThre2++;
            }
            else {
                if (btinfo->bt_timeOverThre2 >= Bt_COEX_INIT_STATE_SCAN_TIME) {
                    /* 
                     * BT_ACTIVE count is over threshold2 for too long. No BT device 
                     * is found. Go back to no protection mode.
                     */
                    btinfo->bt_protectMode = ATH_BT_PROT_MODE_NONE;
                }
                else {
                    /* BT device pairing. Turn on protection mode. */
                    btinfo->bt_protectMode = ATH_BT_PROT_MODE_ON;

                    ath_bt_coex_enable_prot_mode(sc, btinfo->bt_activeChainMask);
                }

                btinfo->bt_initStateDone = AH_TRUE;
                btinfo->bt_timeOverThre = 0;
                btinfo->bt_timeOverThre2 = 0;
            }
        }
    }
    else if ((btinfo->bt_protectMode == ATH_BT_PROT_MODE_NONE) && 
        (btinfo->bt_activeCount > BT_COEX_BT_ACTIVE_THRESHOLD)) {
        btinfo->bt_timeOverThre++;
        if (btinfo->bt_timeOverThre >= BT_COEX_PROT_MODE_ON_TIME) {
            btinfo->bt_timeOverThre = 0;
            btinfo->bt_protectMode = ATH_BT_PROT_MODE_ON;

            ath_bt_coex_enable_prot_mode(sc, btinfo->bt_activeChainMask);
        }
    }
    else {
        btinfo->bt_timeOverThre = 0;
    }

    btinfo->bt_activeCount = 0;
    return 0;
}

/*
 * Primitives to disable BT COEX in hardware.
 * BT and WLAN traffic will go out independently and would be prune to interference.
 */
static INLINE void
ath_bt_disable_coex(struct ath_softc *sc)
{
    ath_hal_bt_disable_coex(sc->sc_ah);
}

/*
 * Primitives to enable BT COEX in hardware.
 * Hardware will prioritize BT and WLAN traffic based on the programmed weights.
 */
static INLINE void
ath_bt_enable_coex(struct ath_softc *sc, struct ath_bt_info *btinfo)
{
    struct ath_hal *ah = sc->sc_ah;

    /* set BT/WLAN weights */
    ath_hal_bt_setweights(ah, btinfo->coexScheme.bt_stompType);

    if (btinfo->bt_state != ATH_BT_STATE_OFF) {
        ath_pcie_pwrsave_btcoex_enable(sc, 1);
    }
    
    ath_hal_bt_enable_coex(ah);
#ifdef ATH_USB
    if (btinfo->bt_coexAgent) {
        btinfo->bt_hwinfo.bt_period = sc->sc_btinfo.coexScheme.bt_period;
        btinfo->bt_hwinfo.bt_dutyCycle = sc->sc_btinfo.coexScheme.bt_dutyCycle;
    }
    ath_wmi_set_btcoex(sc, btinfo->bt_hwinfo.bt_dutyCycle, btinfo->bt_hwinfo.bt_period, 
                                                sc->sc_btinfo.coexScheme.bt_stompType);
#endif
}

static int
ath_bt_watchdog_timer(struct ath_softc *sc)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;

    btinfo->watchdog_cnt++;

    if (btinfo->bt_mgtExtendTime > 0)
        btinfo->bt_mgtExtendTime--;

    /* periodic timer */
    return 0;
}

int
ath_bt_coex_attach(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    u_int8_t cfg;
    int i;

    OS_MEMZERO(btinfo, sizeof(struct ath_bt_info));

    btinfo->bt_hwinfo.bt_coexConfig = sc->sc_reg_parm.bt_reg.btCoexEnable;
    btinfo->bt_hwinfo.bt_gpioBTActive = sc->sc_reg_parm.bt_reg.btActiveGpio;
    btinfo->bt_hwinfo.bt_gpioWLANActive = sc->sc_reg_parm.bt_reg.wlanActiveGpio;
    btinfo->bt_hwinfo.bt_gpioBTPriority = sc->sc_reg_parm.bt_reg.btPriorityGpio;
    btinfo->bt_hwinfo.bt_activePolarity = sc->sc_reg_parm.bt_reg.btActivePolarity;
    btinfo->bt_hwinfo.bt_module = sc->sc_reg_parm.bt_reg.btModule;
    btinfo->bt_hwinfo.bt_singleAnt = (HAL_BOOL) sc->sc_reg_parm.bt_reg.btSingleAnt;
    btinfo->bt_hwinfo.bt_isolation = sc->sc_reg_parm.bt_reg.btWlanIsolation;
    btinfo->bt_hwinfo.bt_dutyCycle = sc->sc_reg_parm.bt_reg.btDutyCycle;
    btinfo->bt_hwinfo.bt_period = sc->sc_reg_parm.bt_reg.btPeriod;
    /* 
     * These SCO/eSCO parameters should be removed. Coex agent would pass
     * these information through OIDs
     */
    btinfo->bt_sco_pspoll = 1;
    btinfo->bt_sco_tm_high = 1220;
    btinfo->bt_sco_tm_low = 1010;
    btinfo->bt_sco_tm_intrvl = 3750;//7500
    btinfo->bt_sco_tm_idle = 2580;//6000
    btinfo->bt_sco_delay = 0;
    btinfo->bt_sco_last_poll = 900;

    cfg = btinfo->bt_hwinfo.bt_coexConfig;

    for (i=0;i<MAX_NUM_BT_ACL_PROFILE;i++) {
        btinfo->pList[i].connHandle = INVALID_PROFILE_TYPE_HANDLE;
        btinfo->pList[i].profileType = INVALID_PROFILE_TYPE_HANDLE;
        btinfo->pList[i].connectionRole = INVALID_PROFILE_TYPE_HANDLE;
    }
    btinfo->activeProfile = 0;
    btinfo->slavePan = 0;

    if (cfg != ATH_BT_COEX_CFG_NONE) {
        sc->sc_hasbtcoex = 1;

        btinfo->bt_on = AH_FALSE;
        btinfo->bt_state = ATH_BT_STATE_OFF;
        btinfo->bt_bmissThresh = IEEE80211_BMISS_LIMIT; /* default */
        ATH_BT_LOCK_INIT(btinfo);

        /* Set btcoex information to hal */
        ath_hal_bt_setinfo(ah, &btinfo->bt_hwinfo);

        if ((cfg >= ATH_BT_COEX_CFG_2WIRE_2CH) && (cfg <= ATH_BT_COEX_CFG_2WIRE_CH0)) {
            /* 2-wire configuration */
            if (cfg == ATH_BT_COEX_CFG_2WIRE_CH0) {
                /* Use chain 0 when BT_ACTIVE is detected */
                btinfo->bt_activeChainMask = 1;
            }
            else if (cfg == ATH_BT_COEX_CFG_2WIRE_CH1) {
                /* Use chain 1 when BT_ACTIVE is detected */
                btinfo->bt_activeChainMask = 2;
            }
            else if (cfg == ATH_BT_COEX_CFG_2WIRE_2CH) {
                /* Use two chains when BT_ACTIVE is detected */
                btinfo->bt_activeChainMask = 3;
            }
            /* 2 wire */
            DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: 2-wire configuration.\n", __func__);
            ath_hal_bt_enable_coex(ah);

            /* Start BT_ACTIVE monitoring */
            btinfo->bt_gpioIntEnabled = AH_TRUE;
            btinfo->bt_preGpioState = btinfo->bt_hwinfo.bt_activePolarity;
            ath_hal_gpioSetIntr(ah, btinfo->bt_hwinfo.bt_gpioBTActive, btinfo->bt_hwinfo.bt_activePolarity);
            ath_initialize_timer(sc->sc_osdev, &btinfo->bt_activeTimer, 
                                 BT_COEX_BT_ACTIVE_MEASURE, 
                                 (timer_handler_function)ath_bt_coex_active_check,
                                 sc);
            ath_start_timer(&btinfo->bt_activeTimer);
        }
        else if (cfg == ATH_BT_COEX_CFG_3WIRE) {
            btinfo->bt_coexAgent = sc->sc_reg_parm.bt_reg.btCoexAgent;
            /* Disable PTA when RSSI is high enough for Helius only to solve the throughput drop when in G mode */
            if (btinfo->bt_hwinfo.bt_module == HAL_BT_MODULE_HELIUS) {
                btinfo->disablePTA.enable = 1;
                btinfo->disablePTA.rssiThreshold = 40;
            }
            else {
                btinfo->disablePTA.enable = sc->sc_reg_parm.bt_reg.btCoexDisablePTA & 0x01;
                btinfo->disablePTA.rssiThreshold = (sc->sc_reg_parm.bt_reg.btCoexDisablePTA >> 16) & 0xff;
            }

            /* Initialize btRcvThreshold */
            btinfo->btRcvThreshold = 0xFFFF;

            DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: 3-wire with bt_module = %d\n", __func__, btinfo->bt_hwinfo.bt_module);
            DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: btCoexAgent = %d, disabltPTA = %d, rssiThreshold = %d\n",
                    __func__, btinfo->bt_coexAgent, btinfo->disablePTA.enable, btinfo->disablePTA.rssiThreshold);
            ASSERT(btinfo->bt_hwinfo.bt_module < HAL_MAX_BT_MODULES);

            /*Enable Antenna Diversity */
            btinfo->antDivEnable = sc->sc_reg_parm.bt_reg.btCoexAntDivEnable & 0x01;
            btinfo->antDivState = 0;

            /* Set low ack parameters */
            if (sc->sc_reg_parm.bt_reg.btCoexLowACKPwr & 0xFF) {
                btinfo->lowACKPower = sc->sc_reg_parm.bt_reg.btCoexLowACKPwr & 0xFF;
                btinfo->btRcvThreshold = (sc->sc_reg_parm.bt_reg.btCoexLowACKPwr >> 8) & 0xFF;
                btinfo->rssiTxLimit = sc->sc_reg_parm.bt_reg.btCoexLowACKPwr >> 16;

                DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: lowACKPower = %d, rssiTxLimit = %d, btRcvThreshold = %dkbps\n", 
                        __func__, btinfo->lowACKPower, btinfo->rssiTxLimit, btinfo->btRcvThreshold);
            }

            ath_hal_bt_config(ah, &ath_bt_configs[btinfo->bt_hwinfo.bt_module]);

            ath_initialize_timer(sc->sc_osdev, &btinfo->watchdog_timer, 
                                 1000, 
                                 (timer_handler_function)ath_bt_watchdog_timer,
                                 sc);
            ath_start_timer(&btinfo->watchdog_timer);

            ath_bt_coex_clear_scheme_info(&btinfo->coexScheme);

            ath_bt_disable_coex(sc);
        }
        else {
            DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Unknown configuration %d.\n", __func__, cfg);
        }
    }

    return 0;
}

void ath_bt_coex_gpio_intr(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    u_int32_t   value;

    if (btinfo->bt_hwinfo.bt_coexConfig == ATH_BT_COEX_CFG_3WIRE) {
        /* 3-wire */
        ath_bt_coex_gpio_intr_3wire(sc);
    }
    else {
        /* 2-wire */
        value = ath_hal_gpioget(ah, btinfo->bt_hwinfo.bt_gpioBTActive);
        if (value == btinfo->bt_preGpioState) {
            /* It's not our GPIO int */
            return;
        }

        if (value != btinfo->bt_hwinfo.bt_activePolarity) {
            btinfo->bt_activeCount++;
        }
       
        btinfo->bt_preGpioState = value;
        ath_hal_gpioSetIntr(ah, btinfo->bt_hwinfo.bt_gpioBTActive, !value);
    }
}

void
ath_bt_coex_detach(struct ath_softc *sc)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    struct ath_hal *ah = sc->sc_ah;

    if (!sc->sc_hasbtcoex) {
        return;
    }

    DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: bt_coex = 0x%x\n", __func__, btinfo->bt_coex);

    if (btinfo->bt_coex) {
        btinfo->detaching = 1;

        ath_bt_coex_stop_scheme(sc);

        btinfo->detaching = 0;
    }

    if (ath_timer_is_initialized(&btinfo->bt_activeTimer)) {
        ath_cancel_timer(&btinfo->bt_activeTimer, CANCEL_NO_SLEEP);
        ath_free_timer(&btinfo->bt_activeTimer);
    }

    if (ath_timer_is_initialized(&btinfo->watchdog_timer)) {
        ath_cancel_timer(&btinfo->watchdog_timer, CANCEL_NO_SLEEP);
        ath_free_timer(&btinfo->watchdog_timer);
    }

    if (btinfo->bt_gpioIntEnabled) {
        ath_hal_gpioSetIntr(ah, btinfo->bt_hwinfo.bt_gpioBTActive, HAL_GPIO_INTR_DISABLE);
        btinfo->bt_gpioIntEnabled = AH_FALSE;
    }

    ATH_BT_LOCK_DESTROY(btinfo);
}

/*
 * Primitives to stomp all BT traffic in hardware.
 */
static INLINE void
ath_bt_stomp_coex(struct ath_softc *sc, struct ath_bt_info *btinfo, HAL_BT_COEX_STOMP_TYPE stompType)
{
    struct ath_hal *ah = sc->sc_ah;

    switch(stompType) {
        case HAL_BT_COEX_STOMP_ALL:
        case HAL_BT_COEX_STOMP_NONE:
        case HAL_BT_COEX_STOMP_ALL_FORCE:
        case HAL_BT_COEX_STOMP_LOW_FORCE:
            if (btinfo->forceStomp && (stompType == HAL_BT_COEX_STOMP_ALL)) {
                ath_hal_bt_setweights(ah, HAL_BT_COEX_STOMP_ALL_FORCE);
            }
            else {
                ath_hal_bt_setweights(ah, stompType);
            }
            break;
        case HAL_BT_COEX_STOMP_LOW:
            if (btinfo->btForcedWeight) {
                ath_hal_bt_setweights(ah, btinfo->btForcedWeight);
            }
            else {
                if (btinfo->forceStomp) {
                    ath_hal_bt_setweights(ah, HAL_BT_COEX_STOMP_LOW_FORCE);
                }
                else {
                    ath_hal_bt_setweights(ah, HAL_BT_COEX_STOMP_LOW);
                }
            }
            break;
        default:
            ath_hal_bt_setweights(ah, HAL_BT_COEX_STOMP_NONE);
            break;
    }

    ath_hal_bt_enable_coex(ah);
}

static void
ath_bt_coex_enter_fake_sleep(struct ath_softc *sc)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;

    if ((sc->sc_ieee_ops->bt_coex_ps_enable) && (btinfo->coexScheme.scheme == ATH_BT_COEX_SCHEME_PS)) {
        sc->sc_ieee_ops->bt_coex_ps_enable(sc->sc_ieee, ATH_BT_COEX_PS_ENABLE);
    }
}

static void
ath_bt_coex_fake_wake_up(struct ath_softc *sc)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;

    if ((sc->sc_ieee_ops->bt_coex_ps_enable) && (btinfo->coexScheme.scheme == ATH_BT_COEX_SCHEME_PS)) {
        sc->sc_ieee_ops->bt_coex_ps_enable(sc->sc_ieee, ATH_BT_COEX_PS_DISABLE);
    }
}

/*
 * SCO coexistence functions
 */

/*
 * This function accurately measures the time difference between
 * rising and falling BT_ACTIVE signal. This is done periodically
 * to synchronize with SCO packets and thereby BT master clock.
 *
 * NB: This function MUST be called in the ISR context
 * to avoid interrupt latency on host system.
 */
static HAL_BOOL
ath_bt_coex_gpio_intr_3wire(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    int select = btinfo->bt_hwinfo.bt_gpioBTActive;

    if (btinfo->bt_gpioIntEnabled == AH_FALSE) {
        ath_hal_gpioSetIntr(ah, select, HAL_GPIO_INTR_DISABLE);
        return AH_FALSE;
    }

    ASSERT(btinfo->bt_coex);

    if (btinfo->coexScheme.scheme == ATH_BT_COEX_SCHEME_PS_POLL)
    {
        struct ath_bt_coex_ps_poll_info *sco = ATH_BT_TO_PSPOLL(btinfo);
        if (ath_hal_gpioGet(ah, select) == 1) {
            /* rising edge trigger */
            sco->sample = 1;    /* start next sampling period */
            sco->tstamp = ath_gen_timer_gettsf32(sc, sco->hwtimer);

            /* configure for the falling edge */
            ath_hal_gpioSetIntr(ah, select, HAL_GPIO_INTR_LOW);

        } else {
            if (sco->sample) {
                u_int32_t diff;
                
                /* falling edge trigger */
                sco->sample = 0; /* end of this sampling period */

                /* simple unsigned arithmetic will be enough to handle TSF wrap-around */
                diff = ath_gen_timer_gettsf32(sc, sco->hwtimer) - sco->tstamp; 

	            /* check if time difference corresponds to SCO packet transmission time */
                if (diff > btinfo->bt_sco_tm_low && diff < btinfo->bt_sco_tm_high) {
                    /*
                     * Configure hardware generic TSF timer to interrupt at
                     * tstamp + BT_SCO_TM_INTRVL + FUDGE
                     *
                     * XXX: We need a fudge factor in case TSF drifts ahead of
                     * BT clock too much, so that a PS-Poll could be sent out
                     * before BT_Priority is asserted. In the meantime, it's okay
                     * that TSF drifts behind BT clock.
                     */
#define FUDGE 15
#ifdef ATH_COEX_SCO_LATE_POLL
                    ath_gen_timer_start(sc, sco->hwtimer,
                                      sco->tstamp + btinfo->bt_sco_tm_intrvl + diff + btinfo->bt_sco_delay,
                                      btinfo->bt_sco_tm_intrvl);
#else
                    ath_gen_timer_start(sc, sco->hwtimer,
                                      sco->tstamp + btinfo->bt_sco_tm_intrvl +  FUDGE,
                                      btinfo->bt_sco_tm_intrvl);
#endif

                    btinfo->bt_hwtimerEnabled = AH_TRUE;
#undef FUDGE
                    
                    /* Synchronization complete. Disable GPIO interrupt. */
                    ath_hal_gpioSetIntr(sc->sc_ah, btinfo->bt_hwinfo.bt_gpioBTActive,
                                        HAL_GPIO_INTR_DISABLE);
                    btinfo->bt_gpioIntEnabled = AH_FALSE;
                    sco->resync = 0;

                    /* Only send NULL data frame to AP when we can sync to SCO pattern */
                    return AH_FALSE;
                }
            }
            ath_hal_gpioSetIntr(ah, select, HAL_GPIO_INTR_HIGH);
        }
    }

    return AH_FALSE;
}

/*
 * Function to turn on GPIO interrupt.
 * NB: it must be called in ISR context, i.e., with IRQ disabled.
 */
static void
ath_bt_sco_start_sync(struct ath_softc *sc)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    struct ath_bt_coex_ps_poll_info *sco = ATH_BT_TO_PSPOLL(btinfo);    

    SCO_RESET_SYNC_STATE(sco);

    /* enable GPIO interrupt on BT_ACTIVE */
    ath_hal_gpioSetIntr(sc->sc_ah, btinfo->bt_hwinfo.bt_gpioBTActive,
                        HAL_GPIO_INTR_LOW);

    /* mark GPIO as enabled */
    btinfo->bt_gpioIntEnabled = AH_TRUE;
}

/*
 * Function to stop hardware sampling and timers.
 * NB: it must be called in ISR(DIRQL) contex
 */
static void
ath_bt_sco_stop_sync(struct ath_softc *sc)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    struct ath_bt_coex_ps_poll_info *sco = ATH_BT_TO_PSPOLL(btinfo);

    if (btinfo->bt_gpioIntEnabled) { /* disable GPIO interrupt */
        ath_hal_gpioSetIntr(sc->sc_ah, btinfo->bt_hwinfo.bt_gpioBTActive,
                            HAL_GPIO_INTR_DISABLE);
        btinfo->bt_gpioIntEnabled = AH_FALSE;
    }

    if (btinfo->bt_hwtimerEnabled) { /* disable timer interrupt */
        ath_gen_timer_stop(sc, sco->hwtimer);
        btinfo->bt_hwtimerEnabled = AH_FALSE;
    }

    if (btinfo->bt_hwtimerEnabled2) { /* disable timer interrupt */
        ath_gen_timer_stop(sc, sco->hwtimer2);
        btinfo->bt_hwtimerEnabled2 = AH_FALSE;
    }
    sco->resync = 0;
}

/* Re-enable GPIO interrupt to synchronize with BT clock. */
static INLINE void
ath_bt_sco_resync(struct ath_softc *sc, struct ath_bt_info *btinfo)
{
    struct ath_bt_coex_ps_poll_info *sco = ATH_BT_TO_PSPOLL(btinfo);

    sco->resync = 1;

    /* disable hardware timer */
    if (btinfo->bt_hwtimerEnabled) {
        ath_gen_timer_stop(sc, sco->hwtimer);
        btinfo->bt_hwtimerEnabled = AH_FALSE;
    }

    /* start synchronize again */
    ath_bt_sco_start_sync(sc);
}

/*
 * Hardware timer ISR handler, which gets called once after syncing with
 * BT_ACTIVE SCO packet in gpio_isr_handler. The hardware timer is set
 * to interrupt periodically every 3.75 msec.
 * NB: this function is called in DPC context.
 */
static void
ath_bt_sco_hwtimer(void *arg)
{
    struct ath_softc *sc = (struct ath_softc *)arg;
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    struct ath_bt_coex_ps_poll_info *sco = ATH_BT_TO_PSPOLL(btinfo);
    u_int32_t   tstamp;

    if (btinfo->bt_hwtimerEnabled == AH_FALSE)
        return;

    tstamp = ath_gen_timer_gettsf32(sc, sco->hwtimer);

    if (btinfo->bt_sco_pspoll) {
        if (!btinfo->bt_stomping) {
#ifdef ATH_COEX_SCO_LATE_POLL
            sco->lastpolltstamp = (tstamp + btinfo->bt_sco_tm_idle - btinfo->bt_sco_last_poll);
#else
            sco->lastpolltstamp = tstamp + btinfo->bt_sco_tm_intrvl - btinfo->bt_sco_last_poll;
#endif
            if (btinfo->bt_hwtimerEnabled2) {
                ath_gen_timer_stop(sc, sco->hwtimer2);
            }
            ath_gen_timer_start(sc, sco->hwtimer2,
                              sco->lastpolltstamp,
                              btinfo->bt_sco_tm_intrvl);

            btinfo->bt_hwtimerEnabled2 = AH_TRUE;

            if (sc->sc_ieee_ops->bt_coex_ps_poll) {
                sc->sc_ieee_ops->bt_coex_ps_poll(sc->sc_ieee, sco->lastpolltstamp);
            }
        }
        else {
            /* It's stomping BT */
            if (++sco->stomp_cnt >= 3) {
                ath_bt_coex_sco_stomp(sc, 0);
                sco->stomp_cnt = 0;
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Stomp SCO for too long. Remove stomp.\n", __func__);
            }
        }
    }

    /* Resync with BT_ACTIVE every BT_SCO_CYCLES_PER_SYNC intervals */
    if (((++sco->sync_cnt) >= BT_SCO_CYCLES_PER_SYNC) && (btinfo->bt_stomping == 0)) {
        ath_bt_sco_resync(sc, btinfo);
    }
#ifndef ATH_COEX_SCO_LATE_POLL
    else if (ath_hal_gpioGet(sc->sc_ah, btinfo->bt_hwinfo.bt_gpioBTActive) == 0) {
        ath_bt_sco_resync(sc, btinfo);
    }
#endif

}

/*
 * Hardware timer overflow ISR handler, which could mean
 * TSF is already out of sync with internal clock
 */
static void
ath_bt_sco_hwtimer_overflow(void *arg)
{
    struct ath_softc *sc = (struct ath_softc *)arg;
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    struct ath_bt_coex_ps_poll_info *sco = ATH_BT_TO_PSPOLL(btinfo);

    if (btinfo->bt_hwtimerEnabled == AH_FALSE)
        return;

    /* timer overflowed. TSF could be out of sync with BT clock already. */
    if (((++sco->overflow_cnt) >= BT_SCO_OVERFLOW_LIMIT) && (btinfo->bt_stomping == 0)) {
        ath_bt_sco_resync(sc, btinfo);
    }
}

/* This function is called in DPC context. */
static void
ath_bt_sco_hwtimer2(void *arg)
{
    struct ath_softc *sc = (struct ath_softc *)arg;
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    struct ath_bt_coex_ps_poll_info *sco = ATH_BT_TO_PSPOLL(btinfo);

    if (btinfo->bt_hwtimerEnabled2 == AH_FALSE)
        return;

    if (sc->sc_ieee_ops->bt_coex_ps_poll) {
        sc->sc_ieee_ops->bt_coex_ps_poll(sc->sc_ieee, 0);
    }

    ath_gen_timer_stop(sc, sco->hwtimer2);
    btinfo->bt_hwtimerEnabled2 = AH_FALSE;
}

#ifdef UNUSED
/*
 * Hardware timer overflow ISR handler, which could mean
 * TSF is already out of sync with internal clock
 */
static void
ath_bt_sco_hwtimer2_overflow(void *arg)
{
    struct ath_softc *sc = (struct ath_softc *)arg;
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    struct ath_bt_coex_ps_poll_info *sco = ATH_BT_TO_PSPOLL(btinfo);

    if (btinfo->bt_hwtimerEnabled2 == AH_FALSE)
        return;

    ath_gen_timer_stop(sc, sco->hwtimer2);
    btinfo->bt_hwtimerEnabled2 = AH_FALSE;
}
#endif /* UNUSED */

static int
ath_bt_sco_init(struct ath_softc *sc, struct ath_bt_info *btinfo)
{
    struct ath_hal *ah = sc->sc_ah;

    DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: TM_LOW: %d,   TM_HIGH: %d,   TM_INTRVL: %d, TM_IDLE: %d\n", 
        __func__, btinfo->bt_sco_tm_low, btinfo->bt_sco_tm_high, btinfo->bt_sco_tm_intrvl, btinfo->bt_sco_tm_idle);
    DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: ps poll:%d, eSCO delay: %d, Last Poll: %d\n", 
        __func__, btinfo->bt_sco_pspoll, btinfo->bt_sco_delay, btinfo->bt_sco_last_poll);

    /*
     * set QCU threshold to AC_BE, i.e.,
     * BE/VI/VO will be treated as high priority WLAN traffic.
     */
    ath_hal_bt_setqcuthresh(ah, sc->sc_haltype2q[HAL_WME_AC_BE]);

    /* set BMISS threshold */
    ath_hal_bt_setbmissthresh(ah, btinfo->bt_bmissThresh);

    return 0;
}

/*
 * Resume Bluetooth coexistence scheme for SCO/eSCO
 */
static int
ath_bt_sco_resume(struct ath_softc *sc, struct ath_bt_info *btinfo)
{
    struct ath_bt_coex_ps_poll_info *sco = ATH_BT_TO_PSPOLL(btinfo);

    ath_bt_enable_coex(sc, btinfo);

    /*
     * 1. send a NULL frame to AP for fake sleep
     * 2. setup seperate queue for ps-poll
     */
    if (sc->sc_ieee_ops->bt_coex_ps_enable && !sco->in_reset) {
        sc->sc_ieee_ops->bt_coex_ps_enable(sc->sc_ieee, ATH_BT_COEX_PS_ENABLE);
    }

    OS_EXEC_INTSAFE(sc->sc_osdev, ath_bt_sco_start_sync, sc);
    return 0;
}

/*
 * Pause Bluetooth coexistence scheme for SCO/eSCO
 */
static int
ath_bt_sco_pause(struct ath_softc *sc, struct ath_bt_info *btinfo)
{
    struct ath_bt_coex_ps_poll_info *sco = ATH_BT_TO_PSPOLL(btinfo);

    if (btinfo->detaching) {
        return 0;
    }

    if (sc->sc_ieee_ops->bt_coex_ps_enable && !sco->in_reset) {
        sc->sc_ieee_ops->bt_coex_ps_enable(sc->sc_ieee, ATH_BT_COEX_PS_DISABLE);
    }

    OS_EXEC_INTSAFE(sc->sc_osdev, ath_bt_sco_stop_sync, sc);
    return 0;
}

static void
ath_bt_sco_free(struct ath_softc *sc, struct ath_bt_info *btinfo)
{
    struct ath_bt_coex_ps_poll_info *sco = ATH_BT_TO_PSPOLL(btinfo);
    struct ath_hal *ah = sc->sc_ah;

    if (btinfo->bt_gpioIntEnabled) {
        ath_hal_gpioSetIntr(ah, btinfo->bt_hwinfo.bt_gpioBTActive, HAL_GPIO_INTR_DISABLE);
        btinfo->bt_gpioIntEnabled = AH_FALSE;
    }
    if (btinfo->bt_hwtimerEnabled) {
        ath_gen_timer_stop(sc, sco->hwtimer);
        btinfo->bt_hwtimerEnabled = AH_FALSE;
    }

    ath_gen_timer_free(sc, sco->hwtimer);
    if (btinfo->bt_sco_pspoll) {
        if (btinfo->bt_hwtimerEnabled2) {
            ath_gen_timer_stop(sc, sco->hwtimer2);
            btinfo->bt_hwtimerEnabled2 = AH_FALSE;
        }
        ath_gen_timer_free(sc, sco->hwtimer2);
    }
}

static struct ath_bt_coex_info *
ath_bt_sco_alloc(struct ath_softc *sc)
{
    struct ath_bt_coex_ps_poll_info *sco;
    struct ath_bt_coex_info *scheme;
    struct ath_bt_info *btinfo = &sc->sc_btinfo;

    sco = &btinfo->bt_scheme_info.ps_poll_info;

    OS_MEMZERO(sco, sizeof(struct ath_bt_coex_ps_poll_info));

    scheme = &sco->coex_ps_poll;
    scheme->init = ath_bt_sco_init;
    scheme->resume = ath_bt_sco_resume;
    scheme->pause = ath_bt_sco_pause;
    scheme->free = ath_bt_sco_free;

    /* allocate a generic TSF timer for this SCO scheme */
    sco->hwtimer = ath_gen_timer_alloc(sc,
                                       HAL_GEN_TIMER_TSF_ANY,
                                       ath_bt_sco_hwtimer,
                                       ath_bt_sco_hwtimer_overflow,
                                       NULL,
                                       sc);
    if (sco->hwtimer == NULL) {
        return NULL;
    }

    if (sc->sc_btinfo.bt_sco_pspoll) {
        sco->hwtimer2 = ath_gen_timer_alloc(sc,
                                            HAL_GEN_TIMER_TSF_ANY,
                                            ath_bt_sco_hwtimer2,
                                            ath_bt_sco_hwtimer2,
                                            NULL,
                                            sc);
        if (sco->hwtimer2 == NULL) {
            return NULL;
        }
    }

    return scheme;
}

/*
 * A2DP coexistence functions
 */
static int
ath_bt_a2dp_period_timer(void *arg)
{
    struct ath_softc *sc = (struct ath_softc *)arg;
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    struct ath_bt_coex_ps_info *a2dp = ATH_BT_TO_PS(btinfo);

    ATH_BT_LOCK(btinfo);

    ath_bt_stomp_coex(sc, btinfo, btinfo->coexScheme.bt_stompType);

    /*
     * Restart duty cycle timer.
     */
    if (btinfo->bt_hwtimerEnabled2) {
        ath_gen_timer_stop(sc, a2dp->a2dp_dcycle_timer);
    }
    ath_gen_timer_start(sc, 
                      a2dp->a2dp_dcycle_timer,
                      (ath_gen_timer_gettsf32(sc, a2dp->a2dp_period_timer) + a2dp->a2dp_dcycle),
                      a2dp->a2dp_dcycle*20);
    btinfo->bt_hwtimerEnabled2 = AH_TRUE;

    /* Wake up from fake sleep */
    ath_bt_coex_fake_wake_up(sc);

    ATH_BT_UNLOCK(btinfo);

    /* return 0 to reschedule itself as a perodic timer */
    return 0;
}

static int
ath_bt_a2dp_dcycle_timer(void *arg)
{
    struct ath_softc *sc = (struct ath_softc *)arg;
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    struct ath_bt_coex_ps_info *a2dp = ATH_BT_TO_PS(btinfo);

    ATH_BT_LOCK(btinfo);

    /*
     * Send NULL frame to AP for fake sleep
     */
    ath_bt_coex_enter_fake_sleep(sc);

    /* Wait for null frame completion */
    if (a2dp->wait_for_complete) {
        if (a2dp->defer_dcycle) {
            /* We have stomp BT too much, give BT a break. */
            if (a2dp->numStompDcycle >= 3) {
                a2dp->numStompDcycle = 0;
                if (btinfo->bt_hwtimerEnabled) {
                    ath_gen_timer_stop(sc, a2dp->a2dp_period_timer);
                }
                ath_gen_timer_start(sc, 
                                  a2dp->a2dp_period_timer,
                                  (ath_gen_timer_gettsf32(sc, a2dp->a2dp_period_timer) + a2dp->a2dp_dcycle*2),
                                  a2dp->a2dp_period);
            }
            else {
                a2dp->numStompDcycle++;
            }

            a2dp->defer_dcycle = 0;

            /*
             * When A2DP duty cycle starts, WLAN traffic must be put to a low priority.
             */
            ath_bt_stomp_coex(sc, btinfo, btinfo->coexScheme.bt_stompType);
        }
        else {
            a2dp->defer_dcycle = 1;
        }
    }
    else {
        /*
         * When A2DP duty cycle starts, WLAN traffic must be put to a low priority.
         */
        ath_bt_stomp_coex(sc, btinfo, btinfo->coexScheme.bt_stompType);
    }

    ATH_BT_UNLOCK(btinfo);

    /* return 1 for one-shot timer */
    return 1;
}

static int
ath_bt_a2dp_init(struct ath_softc *sc, struct ath_bt_info *btinfo)
{
    struct ath_hal *ah = sc->sc_ah;
    struct ath_bt_coex_ps_info *a2dp = ATH_BT_TO_PS(btinfo);

    a2dp->wait_for_complete = 1;

    /*
     * set QCU threshold to AC_BE, i.e.,
     * BE/VI/VO will be treated as high priority WLAN traffic.
     */
    ath_hal_bt_setqcuthresh(ah, sc->sc_haltype2q[HAL_WME_AC_BE]);

    /* set BMISS threshold */
    ath_hal_bt_setbmissthresh(ah, btinfo->bt_bmissThresh);

    return 0;
}

/*
 * Resume Bluetooth coexistence scheme for A2DP
 */
static int
ath_bt_a2dp_resume(struct ath_softc *sc, struct ath_bt_info *btinfo)
{
    struct ath_bt_coex_ps_info *a2dp = ATH_BT_TO_PS(btinfo);

    DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s\n", __func__);

    ath_bt_enable_coex(sc, btinfo);

     /* Send a NULL frame to AP for fake sleep */
    ath_bt_coex_enter_fake_sleep(sc);

    /* start periodic timer for A2DP */
    ath_gen_timer_start(sc, 
                      a2dp->a2dp_period_timer,
                      (ath_gen_timer_gettsf32(sc, a2dp->a2dp_period_timer) + a2dp->a2dp_dcycle),
                      a2dp->a2dp_period);
    btinfo->bt_hwtimerEnabled = AH_TRUE;

    return 0;
}

/*
 * Pause Bluetooth coexistence scheme for A2DP
 */
static int
ath_bt_a2dp_pause(struct ath_softc *sc, struct ath_bt_info *btinfo)
{
    struct ath_bt_coex_ps_info *a2dp = ATH_BT_TO_PS(btinfo);

    DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s\n", __func__);

    /* cancel all the timers */
    if (btinfo->bt_hwtimerEnabled)
        ath_gen_timer_stop(sc, a2dp->a2dp_period_timer);
    btinfo->bt_hwtimerEnabled = AH_FALSE;

    if (btinfo->bt_hwtimerEnabled2)
        ath_gen_timer_stop(sc, a2dp->a2dp_dcycle_timer);
    btinfo->bt_hwtimerEnabled2 = AH_FALSE;

    if (!btinfo->detaching) {
        /* Send a NULL frame to AP for fake wake up */
        ath_bt_coex_fake_wake_up(sc);
    }

    return 0;
}

static void
ath_bt_a2dp_free(struct ath_softc *sc, struct ath_bt_info *btinfo)
{
    struct ath_bt_coex_ps_info *a2dp = ATH_BT_TO_PS(btinfo);

    ath_gen_timer_free(sc, a2dp->a2dp_period_timer);
    ath_gen_timer_free(sc, a2dp->a2dp_dcycle_timer);
}

static struct ath_bt_coex_info *
ath_bt_a2dp_alloc(struct ath_softc *sc)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    struct ath_bt_coex_ps_info *a2dp;
    struct ath_bt_coex_info *scheme;

    a2dp = &btinfo->bt_scheme_info.ps_info;

    OS_MEMZERO(a2dp, sizeof(struct ath_bt_coex_ps_info));

    scheme = &a2dp->coex_ps;
    scheme->init = ath_bt_a2dp_init;
    scheme->resume = ath_bt_a2dp_resume;
    scheme->pause = ath_bt_a2dp_pause;
    scheme->free = ath_bt_a2dp_free;

    /* initialize A2DP specific info */
    a2dp->a2dp_period = btinfo->coexScheme.bt_period;
    a2dp->a2dp_dcycle = btinfo->coexScheme.bt_dutyCycle;

    a2dp->a2dp_period_timer = ath_gen_timer_alloc(sc,
                                                  HAL_GEN_TIMER_TSF_ANY,
                                                  (ath_hwtimer_function)ath_bt_a2dp_period_timer,
                                                  (ath_hwtimer_function)ath_bt_a2dp_period_timer,
                                                  NULL,
                                                  sc);
    if (a2dp->a2dp_period_timer == NULL) {
        return NULL;
    }
    a2dp->a2dp_dcycle_timer = ath_gen_timer_alloc(sc,
                                                  HAL_GEN_TIMER_TSF_ANY,
                                                  (ath_hwtimer_function)ath_bt_a2dp_dcycle_timer,
                                                  (ath_hwtimer_function)ath_bt_a2dp_dcycle_timer,
                                                  NULL,
                                                  sc);
    if (a2dp->a2dp_dcycle_timer == NULL) {
        return NULL;
    }

    a2dp->a2dp_dcycle = ((100 - a2dp->a2dp_dcycle) * a2dp->a2dp_period) * 10;
    a2dp->a2dp_period *= 1000;
    DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: New Period = %dus, New Duty Cycle = %dus\n", 
        __func__, a2dp->a2dp_period, a2dp->a2dp_dcycle);

    return scheme;
}

/*
 * HW3WIRE coexistence functions
 */
static int
ath_bt_hw3wire_period_timer(void *arg)
{
    struct ath_softc *sc = (struct ath_softc *)arg;
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    struct ath_bt_coex_hw3wire_info *hw3wire = ATH_BT_TO_HW3WIRE(btinfo);
    u_int32_t timerNext;

    if (BT_COEX_IS_11G_MODE(sc) || BT_COEX_ACTIVE_PROFILE(btinfo, ATH_COEX_BT_PROFILE_PAN)) {
        /*
         * Enable forceStomp mode when WLAN has rx activity in 11G mode.
         */
        btinfo->waitTime += btinfo->coexScheme.bt_period;

        if (btinfo->waitTime >= BT_COEX_FORCE_STOMP_CHECK_TIME) {

            if ((btinfo->forceStomp & BT_COEX_FORCE_STOMP_RX_TRAFFIC) 
                 && (btinfo->wlanRxPktNum < (BT_COEX_FORCE_STOMP_THRESHOLD - 10))) 
            {
                btinfo->forceStomp &= (~BT_COEX_FORCE_STOMP_RX_TRAFFIC);
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: No Rx traffic. Turn off force stomp = %d\n", __func__, btinfo->forceStomp);
            }
            else if (((btinfo->forceStomp & BT_COEX_FORCE_STOMP_RX_TRAFFIC) == 0) && (btinfo->wlanRxPktNum >= BT_COEX_FORCE_STOMP_THRESHOLD)) {
                btinfo->forceStomp |= BT_COEX_FORCE_STOMP_RX_TRAFFIC;
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Rx traffic enables force stomp = %d\n", __func__, btinfo->forceStomp);
            }

            btinfo->waitTime = 0;
            btinfo->wlanRxPktNum = 0;
        }
    }

    timerNext = ath_gen_timer_gettsf32(sc, hw3wire->no_stomp_timer) + hw3wire->hw3wire_no_stomp;

    ath_bt_stomp_coex(sc, btinfo, btinfo->coexScheme.bt_stompType);

    if (hw3wire->hw3wire_period != hw3wire->hw3wire_no_stomp) {
        if (btinfo->bt_hwtimerEnabled2) {
            ath_gen_timer_stop(sc, hw3wire->no_stomp_timer);
        }
        ath_gen_timer_start(sc, 
                          hw3wire->no_stomp_timer,
                          timerNext,
                          (hw3wire->hw3wire_no_stomp << 4));
        btinfo->bt_hwtimerEnabled2 = AH_TRUE;
    }

    /* return 0 to reschedule itself as a perodic timer */
    return 0;
}

static int
ath_bt_hw3wire_no_stomp_timer(void *arg)
{
    struct ath_softc *sc = (struct ath_softc *)arg;
    struct ath_bt_info *btinfo = &sc->sc_btinfo;

    ath_bt_stomp_coex(sc, btinfo, HAL_BT_COEX_STOMP_NONE);

    /* return 1 as one shot timer */
    return 1;
}

static int
ath_bt_hw3wire_init(struct ath_softc *sc, struct ath_bt_info *btinfo)
{
    struct ath_hal *ah = sc->sc_ah;

    /*
     * set QCU threshold to AC_BE, i.e.,
     * BE/VI/VO will be treated as high priority WLAN traffic.
     */
    ath_hal_bt_setqcuthresh(ah, sc->sc_haltype2q[HAL_WME_AC_BE]);

    /* set BMISS threshold */
    ath_hal_bt_setbmissthresh(ah, btinfo->bt_bmissThresh);

    return 0;
}

/*
 * Resume HW3wire coex scheme
 */
static int
ath_bt_hw3wire_resume(struct ath_softc *sc, struct ath_bt_info *btinfo)
{
    struct ath_bt_coex_hw3wire_info *hw3wire = ATH_BT_TO_HW3WIRE(btinfo);

    ath_bt_enable_coex(sc, btinfo);
#ifndef ATH_USB
    btinfo->waitTime = 0;
    btinfo->forceStomp &= (~BT_COEX_FORCE_STOMP_RX_TRAFFIC);
    btinfo->wlanRxPktNum = 0;

    if (hw3wire->hw3wire_period == hw3wire->hw3wire_no_stomp) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Always stomp low priority BT. period = %dus\n", __func__, hw3wire->hw3wire_period);
    }

    if (sc->sc_reg_parm.bt_reg.btCoexRSSIModeProfile & RSSI_MODE_PROFILE_ENABLE) {
        hw3wire->rssiProfile = sc->sc_reg_parm.bt_reg.btCoexRSSIModeProfile & RSSI_MODE_PROFILE_ENABLE;
        hw3wire->rssiProfileThreshold = (sc->sc_reg_parm.bt_reg.btCoexRSSIModeProfile >> 8) & 0xff;
        hw3wire->rssiProfile11g = (sc->sc_reg_parm.bt_reg.btCoexRSSIModeProfile >> 16) & 0xff;
        hw3wire->rssiProfile11n = (sc->sc_reg_parm.bt_reg.btCoexRSSIModeProfile >> 24) & 0xff;
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: RSSI_MODE_PROFILE_ENABLE, rssiProfileThreshold = %d, rssiProfile11g = %d, rssiProfile11n = %d\n", 
            __func__, hw3wire->rssiProfileThreshold, hw3wire->rssiProfile11g, hw3wire->rssiProfile11n);
    }

#ifdef USE_ATH_TIMER
    ath_start_timer(&hw3wire->period_timer);
#else
    ath_gen_timer_start(sc, 
                      hw3wire->period_timer,
                      (ath_gen_timer_gettsf32(sc, hw3wire->period_timer) + hw3wire->hw3wire_no_stomp),
                      hw3wire->hw3wire_period);
    btinfo->bt_hwtimerEnabled = AH_TRUE;
#endif
#endif
    return 0;
}

/*
 * Pause HW3wire scheme
 */
static int
ath_bt_hw3wire_pause(struct ath_softc *sc, struct ath_bt_info *btinfo)
{
    struct ath_bt_coex_hw3wire_info *hw3wire = ATH_BT_TO_HW3WIRE(btinfo);

    DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s\n", __func__);

#ifdef ATH_USB
    ath_wmi_set_btcoex(sc, 0, 0, HAL_BT_COEX_STOMP_NONE);
#else

    /* cancel all the timers */
#ifdef USE_ATH_TIMER
    ath_cancel_timer(&hw3wire->period_timer, CANCEL_NO_SLEEP);
#else
    if (btinfo->bt_hwtimerEnabled)
        ath_gen_timer_stop(sc, hw3wire->period_timer);
    btinfo->bt_hwtimerEnabled = AH_FALSE;
#endif

    if (btinfo->bt_hwtimerEnabled2)
        ath_gen_timer_stop(sc, hw3wire->no_stomp_timer);
    btinfo->bt_hwtimerEnabled2 = AH_FALSE;

#endif
    return 0;
}

static void
ath_bt_hw3wire_free(struct ath_softc *sc, struct ath_bt_info *btinfo)
{
    struct ath_bt_coex_hw3wire_info *hw3wire = ATH_BT_TO_HW3WIRE(btinfo);


#ifdef ATH_USB
    ath_wmi_set_btcoex(sc, 0, 0, HAL_BT_COEX_STOMP_NONE);
#else

#ifdef USE_ATH_TIMER
    ath_free_timer(&hw3wire->period_timer);
#else
    ath_gen_timer_free(sc, hw3wire->period_timer);
#endif
    ath_gen_timer_free(sc, hw3wire->no_stomp_timer);
#endif
}

static struct ath_bt_coex_info *
ath_bt_hw3wire_alloc(struct ath_softc *sc)
{
    struct ath_bt_coex_hw3wire_info *hw3wire;
    struct ath_bt_coex_info *scheme;
    struct ath_bt_info *btinfo = &sc->sc_btinfo;

    hw3wire = &btinfo->bt_scheme_info.hw3wire_info;
    
    if (hw3wire == NULL)
        return NULL;
    OS_MEMZERO(hw3wire, sizeof(struct ath_bt_coex_hw3wire_info));

    scheme = &hw3wire->coex_hw3wire;
    scheme->init = ath_bt_hw3wire_init;
    scheme->resume = ath_bt_hw3wire_resume;
    scheme->pause = ath_bt_hw3wire_pause;
    scheme->free = ath_bt_hw3wire_free;

    /* initialize HW3WIRE specific info */
    hw3wire->hw3wire_period = sc->sc_btinfo.coexScheme.bt_period * 1000;
    hw3wire->hw3wire_no_stomp = (100 - sc->sc_btinfo.coexScheme.bt_dutyCycle) * hw3wire->hw3wire_period / 100;
#ifndef ATH_USB
#ifdef USE_ATH_TIMER
    ath_initialize_timer(sc->sc_osdev, &hw3wire->period_timer,
                         sc->sc_btinfo.coexScheme.bt_period,        // in ms
                         (timer_handler_function)ath_bt_hw3wire_period_timer,
                         sc);
#else
    hw3wire->period_timer = ath_gen_timer_alloc(sc,
                                                HAL_GEN_TIMER_TSF_ANY,
                                                (ath_hwtimer_function)ath_bt_hw3wire_period_timer,
                                                (ath_hwtimer_function)ath_bt_hw3wire_period_timer,
                                                NULL,
                                                sc);
    if (hw3wire->period_timer == NULL) {
        return NULL;
    }
#endif

    hw3wire->no_stomp_timer = ath_gen_timer_alloc(sc,
                                                  HAL_GEN_TIMER_TSF_ANY,
                                                  (ath_hwtimer_function)ath_bt_hw3wire_no_stomp_timer,
                                                  (ath_hwtimer_function)ath_bt_hw3wire_no_stomp_timer,
                                                  NULL,
                                                  sc);
    if (hw3wire->no_stomp_timer == NULL) {
        return NULL;
    }
#endif
    return scheme;
}

/*
 * Common (non scheme specific) Bluetooth coexistence functions
 */

static int
ath_bt_coex_newscheme(struct ath_softc *sc, ATH_BT_COEX_SCHEME stype)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    struct ath_bt_coex_info *coex_scheme;

    /* create a new coex scheme instance */
    switch (stype) {
    case ATH_BT_COEX_SCHEME_PS_POLL:
        coex_scheme = ath_bt_sco_alloc(sc);
        break;
    
    case ATH_BT_COEX_SCHEME_PS:
        coex_scheme = ath_bt_a2dp_alloc(sc);
        break;
    
    case ATH_BT_COEX_SCHEME_HW3WIRE:
        coex_scheme = ath_bt_hw3wire_alloc(sc);
        break;

    default:
        coex_scheme = NULL;
        break;
    }

    if (coex_scheme == NULL) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Unable to create a new coex scheme %d\n", __func__, stype);
        return -ENOMEM;
    }

    btinfo->bt_coex = coex_scheme;
    return 0;
}

static void
ath_bt_coex_stop_scheme(struct ath_softc *sc)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;

    if (btinfo->coexScheme.scheme == ATH_BT_COEX_SCHEME_NONE) {
        return;
    }

    /* Stop current scheme */
    DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Stop current scheme %d\n", __func__, btinfo->coexScheme.scheme);

    if (btinfo->bt_coex) {
        btinfo->bt_coex->pause(sc, btinfo);
    }

    ath_bt_disable_coex(sc);
    ath_bt_coex_clear_scheme_info(&btinfo->coexScheme);

    if (btinfo->bt_coex) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: free scheme\n", __func__);
        btinfo->bt_coex->free(sc, btinfo);
        btinfo->bt_coex = NULL;
    }
}

static void
ath_bt_coex_clear_scheme_info(ATH_COEX_SCHEME_INFO *schemeInfo)
{
    schemeInfo->scheme = ATH_BT_COEX_SCHEME_NONE;
    schemeInfo->bt_period = 0;
    schemeInfo->bt_dutyCycle = 0;
    schemeInfo->bt_stompType = 0;
    schemeInfo->wlan_aggrLimit = 0;
}

static HAL_BOOL
ath_bt_coex_is_same_scheme(ATH_COEX_SCHEME_INFO *schemeX, ATH_COEX_SCHEME_INFO *schemeY)
{
    if ((schemeX->scheme == schemeY->scheme) &&
        (schemeX->bt_period == schemeY->bt_period) &&
        (schemeX->bt_dutyCycle == schemeY->bt_dutyCycle) &&
        (schemeX->bt_stompType == schemeY->bt_stompType) &&
        (schemeX->wlan_aggrLimit == schemeY->wlan_aggrLimit))
    {
        return AH_TRUE;
    }
    else {
        return AH_FALSE;
    }
}

static void 
ath_bt_coex_adjust_aggr_limit(struct ath_softc *sc, ATH_COEX_SCHEME_INFO *scheme)
{
    u_int32_t   wlanAirTime;

    wlanAirTime = (scheme->bt_period * (100 - scheme->bt_dutyCycle)) / 100;

    /*
     * wlanAirTime is in ms. wlan_aggrLimit is in 0.25ms.
     * When wlanAirTime is less than 4ms, wlan_aggrLimit needs to be adjusted
     * to 1/2 of wlanAirTime to make sure the aggr can go through without internal
     * collision because of bt_active.
     */
    if ((wlanAirTime <= 4) &&
        ((scheme->wlan_aggrLimit == 0) ||
         (scheme->wlan_aggrLimit > (wlanAirTime << 1))))
    {
        scheme->wlan_aggrLimit = (wlanAirTime << 1);
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: btPeriod = %dms, btDutyCycle = %d, adjust aggrLimit to %dus.\n",
                __func__, scheme->bt_period, scheme->bt_dutyCycle, scheme->wlan_aggrLimit * 250);
    }
}

static void
ath_bt_coex_lower_ACK_power(struct ath_softc *sc, ATH_BT_COEX_LOW_ACK_FLAG flag)
{
    struct ath_bt_info  *btinfo = &sc->sc_btinfo;
    HAL_BOOL            old_state, new_state;

    old_state = BT_COEX_NEED_LOWER_ACK_PWR(btinfo);

    switch (flag)
    {
        case ATH_BT_COEX_LOW_RSSI:
            btinfo->lowerTxPower &= (~BT_COEX_LOWER_ACK_PWR_RSSI);
            break;

        case ATH_BT_COEX_LOW_RCV_XPUT:
            btinfo->lowerTxPower &= (~BT_COEX_LOWER_ACK_PWR_XPUT);
            break;

        case ATH_BT_COEX_HIGH_RSSI:
            btinfo->lowerTxPower |= BT_COEX_LOWER_ACK_PWR_RSSI;
            break;

        case ATH_BT_COEX_HIGH_RCV_XPUT:
            btinfo->lowerTxPower |= BT_COEX_LOWER_ACK_PWR_XPUT;
            break;

        case ATH_BT_COEX_HID_ON:
            btinfo->lowerTxPower |= BT_COEX_LOWER_ACK_PWR_HID;
            break;

        case ATH_BT_COEX_HID_OFF:
            btinfo->lowerTxPower &= (~BT_COEX_LOWER_ACK_PWR_HID);
            break;

        case ATH_BT_COEX_SCO_ON:
            btinfo->lowerTxPower |= BT_COEX_LOWER_ACK_PWR_SCO;
            break;

        case ATH_BT_COEX_SCO_OFF:
            btinfo->lowerTxPower &= (~BT_COEX_LOWER_ACK_PWR_SCO);
            break;

        case ATH_BT_COEX_A2DP_ON:
            btinfo->lowerTxPower |= BT_COEX_LOWER_ACK_PWR_A2DP;
            break;

        case ATH_BT_COEX_A2DP_OFF:
            btinfo->lowerTxPower &= (~BT_COEX_LOWER_ACK_PWR_A2DP);
            break;

        default:
            break;
    }

    new_state = BT_COEX_NEED_LOWER_ACK_PWR(btinfo);
    DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: the flag is 0x%x, the old state is %d and the new state is %d\n", __func__, flag, old_state, new_state);
    if (old_state && !new_state) {
        ath_hal_bt_setParameter(sc->sc_ah, HAL_BT_COEX_SET_ACK_PWR, 0);
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Normal ACK power. flag = %d\n", __func__, btinfo->lowerTxPower);
    }
    else if (!old_state && new_state) {
        ath_hal_bt_setParameter(sc->sc_ah, HAL_BT_COEX_SET_ACK_PWR, 1);
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Lowered ACK power. flag = %d\n", __func__, btinfo->lowerTxPower);
    }
}

int
ath_bt_coex_switch_scheme(struct ath_softc *sc, HAL_BOOL forceSwitch)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    ATH_COEX_SCHEME_INFO newSchemeInfo;
    int useFixedDutycycle = AH_FALSE;

    if (btinfo->disablePTA.enable && (btinfo->disablePTA.state == ATH_BT_COEX_PTA_DISABLE))
    {
        /* No scheme is needed */
        newSchemeInfo = btProfileToScheme[0];
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: DisablePTA is on. No scheme required.\n",
                    __func__);
    }
    else if ((btinfo->bt_state == ATH_BT_STATE_ON) && 
        (btinfo->wlan_state == ATH_WLAN_STATE_CONNECT)) 
    {
        newSchemeInfo = btProfileToScheme[GET_SCHEME_INDEX(btinfo)];
    }
#ifdef ATH_USB
    else if ((btinfo->bt_state == ATH_BT_STATE_OFF) && (btinfo->wlan_state == ATH_WLAN_STATE_CONNECT) &&
        (btinfo->wlan_tx_state == ATH_USB_WLAN_TX_LOW)) {
        newSchemeInfo = stateToScheme[0][0];
    }
#endif
    else {
        newSchemeInfo = stateToScheme[btinfo->bt_state][btinfo->wlan_state];
    }

    if (btinfo->antDivEnable)
    {
        if (btinfo->antDivState && ((btinfo->bt_state != ATH_BT_STATE_OFF) ||
                (btinfo->wlan_state != ATH_WLAN_STATE_CONNECT)))
        {
            btinfo->antDivState = AH_FALSE;

            DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Turn off antenna diversity. bt_state = %d, wlan_state = %d\n",
                    __func__, btinfo->bt_state, btinfo->wlan_state);

            /* Change the register to turn off antenna diversity */
            ath_hal_bt_setParameter(sc->sc_ah, HAL_BT_COEX_ANTENNA_DIVERSITY, AH_FALSE);
        }
        else if (!btinfo->antDivState && (btinfo->bt_state == ATH_BT_STATE_OFF) && 
                (btinfo->wlan_state == ATH_WLAN_STATE_CONNECT)&&
                BT_COEX_IS_11G_MODE(sc))
        {
            btinfo->antDivState = AH_TRUE;

            DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Turn on antenna diversity. bt_state = %d, wlan_state = %d\n",
                    __func__, btinfo->bt_state, btinfo->wlan_state);

            /* Change the register to turn on antenna diversity */
            ath_hal_bt_setParameter(sc->sc_ah, HAL_BT_COEX_ANTENNA_DIVERSITY, AH_TRUE);
        }

        if (btinfo->antDivState) {
            newSchemeInfo = stateToScheme[ATH_BT_STATE_OFF][ATH_WLAN_STATE_CONNECT];
        }
        if (btinfo->disablePTA.enable && (btinfo->disablePTA.state == ATH_BT_COEX_PTA_DISABLE)) {
            if (btinfo->antDivState == AH_TRUE) {
                ath_hal_bt_setParameter(sc->sc_ah, HAL_BT_COEX_LOWER_TX_PWR, 0);
            }
            else {
                ath_hal_bt_setParameter(sc->sc_ah, HAL_BT_COEX_LOWER_TX_PWR, 1);
            }
        }
    }

    if ((btinfo->coexScheme.scheme == ATH_BT_COEX_SCHEME_HW3WIRE) && (btinfo->numACLProfile == 1)) {
        struct ath_bt_coex_hw3wire_info *hw3wire = ATH_BT_TO_HW3WIRE(btinfo);
        if ((hw3wire->rssiProfile) && (hw3wire->rssiSpecialCase)) {
            useFixedDutycycle = AH_TRUE;
            if (BT_COEX_IS_11G_MODE(sc)) {
                newSchemeInfo.bt_dutyCycle = hw3wire->rssiProfile11g;
            }
            else {
                newSchemeInfo.bt_dutyCycle = hw3wire->rssiProfile11n;
            }
        }
    }

    if (ath_bt_coex_is_same_scheme(&btinfo->coexScheme, &newSchemeInfo) &&
        !forceSwitch) {
        return 0;
    }

    if (btinfo->coexScheme.scheme != ATH_BT_COEX_SCHEME_NONE) {
        /* Stop current scheme */
        ath_bt_coex_stop_scheme(sc);

        ath_pcie_pwrsave_btcoex_enable(sc, 0);
    }

    if ((btinfo->bt_state != ATH_BT_STATE_OFF) && !btinfo->bt_on) {
        btinfo->bt_on = AH_TRUE;
	    ath_beacon_config(sc, ATH_BEACON_CONFIG_REASON_RESET, ATH_IF_ID_ANY);   /* restart beacons */
    }
    else if ((btinfo->bt_state == ATH_BT_STATE_OFF) && btinfo->bt_on) {
        btinfo->bt_on = AH_FALSE;
	    ath_beacon_config(sc, ATH_BEACON_CONFIG_REASON_RESET, ATH_IF_ID_ANY);   /* restart beacons */

        /*
         * Clear lowerTxPower RCV_XPUT flag if there is no active BT profile.
         */
        if (btinfo->lowerTxPower & BT_COEX_LOWER_ACK_PWR_XPUT) {
            ath_bt_coex_lower_ACK_power(sc, ATH_BT_COEX_LOW_RCV_XPUT);
        }
    }
    
    btinfo->wlan_channel = sc->sc_curchan.channel;

    if ((newSchemeInfo.scheme != ATH_BT_COEX_SCHEME_NONE) &&
        (btinfo->wlan_channel >= BT_LOW_FREQ) && (btinfo->wlan_channel <= BT_HIGH_FREQ))
    {
        /* Start a new scheme */
        btinfo->coexScheme = newSchemeInfo;

        /* Adjust dutycycle for basic rate devices */
        if (btinfo->btBDRHandle && btinfo->coexScheme.bt_dutyCycle) {
            if (useFixedDutycycle != AH_TRUE) {
                btinfo->coexScheme.bt_dutyCycle += ATH_BT_COEX_BDR_DUTYCYCLE_ADJUST;
                if (btinfo->coexScheme.bt_dutyCycle > ATH_BT_COEX_MAX_DUTYCYCLE) {
                    btinfo->coexScheme.bt_dutyCycle = ATH_BT_COEX_MAX_DUTYCYCLE;
                }
            }
            DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: BDR device connected. Dutycycle set to %d\n", 
                    __func__, btinfo->coexScheme.bt_dutyCycle);
        }

        if (BT_COEX_IS_11G_MODE(sc)) {
            /* Reduce bt_period in half if we're in 11G mode. */
            btinfo->coexScheme.bt_period >>= 1;
        }
        else {
            /* Adjust wlan_aggrLimit when wlan's share of air time is less than 4ms */
            ath_bt_coex_adjust_aggr_limit(sc, &btinfo->coexScheme);
        }

        if(sc->sc_reg_parm.bt_reg.btCoexEnhanceA2DP &&
           BT_COEX_ACTIVE_PROFILE(btinfo, ATH_COEX_BT_PROFILE_A2DP) && 
           (btinfo->numACLProfile == 1) && 
           (btinfo->numSCOProfile == 0))
        {
            /* increase BT duty cycle for A2DP profile if registry key exists. */
            btinfo->coexScheme.bt_dutyCycle = sc->sc_reg_parm.bt_reg.btCoexEnhanceA2DP;
        }

        if (btinfo->slavePan) {
            btinfo->coexScheme.bt_dutyCycle = btinfo->coexScheme.bt_dutyCycle - BT_SLAVE_PAN_DUTYCYCLE_ADJUSTMENT;
        }

        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Start a new scheme %d, bt_period = %dms, bt_dutyCycle = %d, stompType = %d, aggrlimit = %dus\n", 
            __func__, btinfo->coexScheme.scheme, btinfo->coexScheme.bt_period, btinfo->coexScheme.bt_dutyCycle, 
            btinfo->coexScheme.bt_stompType, btinfo->coexScheme.wlan_aggrLimit * 250);

        if (ath_bt_coex_newscheme(sc, btinfo->coexScheme.scheme) == 0) {

            if (btinfo->bt_coex) {
                btinfo->bt_coex->init(sc, btinfo);
                btinfo->bt_coex->resume(sc, btinfo);
            }
        }
        else
        {
            DPRINTF(sc, ATH_DEBUG_ANY, "%s: Coex new scheme %d failed\n",
                __func__, btinfo->coexScheme.scheme);

            ath_bt_coex_clear_scheme_info(&btinfo->coexScheme);
        }
    }
    else {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: No new scheme starts. wlan channel:%d, bt_state: %d, wlan_state:%d, scheme:%d\n",
            __func__, btinfo->wlan_channel, btinfo->bt_state, btinfo->wlan_state, newSchemeInfo.scheme);
    }
    return 0;
}

int
ath_bt_coex_event(ath_dev_t dev, u_int32_t nevent, void *param)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    int error = 0;

    ATH_BT_LOCK(btinfo);

    if (btinfo->bt_hwinfo.bt_coexConfig != ATH_BT_COEX_CFG_3WIRE) {
        ATH_BT_UNLOCK(btinfo);
        return error;
    }
    ATH_BT_UNLOCK(btinfo);

    switch(nevent) {
    case ATH_COEX_EVENT_WLAN_AGGR_FRAME_LEN:
        ath_bt_coex_aggr_frame_len(sc, (u_int32_t *)param);
        break;
    case ATH_COEX_EVENT_WLAN_CONNECT:
        ath_bt_coex_connect(sc);
        break;
    case ATH_COEX_EVENT_WLAN_DISCONNECT:
        ath_bt_coex_disconnect(sc);
        break;
    case ATH_COEX_EVENT_WLAN_RESET_START:
        ath_bt_coex_reset_start(sc);
        break;
    case ATH_COEX_EVENT_WLAN_RESET_END:
        ath_bt_coex_reset_end(sc);
        break;
    case ATH_COEX_EVENT_WLAN_SCAN:
        ath_bt_coex_scan(sc, *((u_int32_t *)param));
        break;
    case ATH_COEX_EVENT_WLAN_ASSOC:
        ath_bt_coex_assoc(sc, *((u_int32_t *)param));
        break;
    case ATH_COEX_EVENT_WLAN_RSSI_UPDATE:
        ath_bt_coex_rssi_update(sc, (int8_t *)param);
        break;
    case ATH_COEX_EVENT_WLAN_FULL_SLEEP:
        ath_bt_coex_fullsleep(sc);
        break;
    case ATH_COEX_EVENT_BT_INQUIRY:
        ath_bt_coex_bt_inquiry(sc, *(u_int32_t *)param);
        break;
    case ATH_COEX_EVENT_BT_CONNECT:
        ath_bt_coex_bt_connect(sc, *(u_int32_t *)param);
        break;
    case ATH_COEX_EVENT_BT_ROLE_SWITCH:
        ath_bt_coex_bt_role_switch(sc, param);
        break;
    case ATH_COEX_EVENT_BT_PROFILE:
        ath_bt_coex_bt_profile(sc, param);
        break;
    case ATH_COEX_EVENT_BT_SCO_PKTINFO:
        break;
    case ATH_COEX_EVENT_BT_PROFILE_INFO:
        ath_bt_coex_bt_profile_type_update(sc, param);
        break;
    case ATH_COEX_EVENT_BT_RCV_RATE:
        ath_bt_coex_bt_rcv_rate(sc, (ATH_COEX_BT_RCV_RATE *)param);
        break;
#ifdef ATH_USB
    case ATH_COEX_EVENT_WLAN_TRAFFIC:
        ath_bt_coex_throughput(sc, *(u_int8_t *)param);
        break;
#endif        
    default:
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: invalid event %d\n", __func__, nevent);
        break;
    }
    
    return 1;
}

/*
 * WLAN events processing
 * btinfo should be locked already.
 */

void
ath_bt_coex_aggr_frame_len(struct ath_softc *sc, u_int32_t *frameLen)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;

    /*
     * ATH_BT_LOCK(btinfo) and ATH_BT_UNLOCK(btinfo) were removed 
     * to avoid potential deadlock found in EV 68724.
     */

    if (btinfo->coexScheme.wlan_aggrLimit && (*frameLen))
    {
        /*
         * The input frameLen is the frame length for 4ms aggr.
         * wlan_aggrLimit is in 1/4ms. Frame length will be changed to 
         * what wlan_aggrLimit specifies.
         */
        *frameLen = (*frameLen * btinfo->coexScheme.wlan_aggrLimit) >> 4;
    }
}

void
ath_bt_coex_connect(struct ath_softc *sc)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;

    ATH_BT_LOCK(btinfo);
    do
    {
        if (btinfo->wlan_state == ATH_WLAN_STATE_CONNECT) {
            break;
        }

        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: coexAgent = %d\n", __func__, btinfo->bt_coexAgent);

        btinfo->wlan_state = ATH_WLAN_STATE_CONNECT;
        btinfo->wlan_channel = sc->sc_curchan.channel;
#ifdef ATH_USB
        btinfo->wlan_tx_state = ATH_USB_WLAN_TX_LOW;
#endif
        
        if (btinfo->bt_coexAgent) {
            ath_bt_coex_switch_scheme(sc, AH_FALSE);
        }
        else {
            /* Get settings from registry */
            btinfo->bt_on = AH_TRUE;
            btinfo->bt_state = ATH_BT_STATE_ON;
            btinfo->coexScheme.bt_period = sc->sc_reg_parm.bt_reg.btPeriod;
            btinfo->coexScheme.bt_dutyCycle = sc->sc_reg_parm.bt_reg.btDutyCycle;
            btinfo->coexScheme.scheme = sc->sc_reg_parm.bt_reg.btScheme;
            btinfo->coexScheme.bt_stompType = sc->sc_reg_parm.bt_reg.btCoexStompType;
            btinfo->coexScheme.wlan_aggrLimit = sc->sc_reg_parm.bt_reg.btCoexAggrLimit;
            btinfo->btForcedWeight = sc->sc_reg_parm.bt_reg.btCoexWeight;

            /* Adjust wlan_aggrLimit when wlan's share of air time is less than 4ms */
            ath_bt_coex_adjust_aggr_limit(sc, &btinfo->coexScheme);

            DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Start a new scheme %d, bt_period = %dms, bt_dutyCycle = %d, stompType = %d, aggrlimit = %dus, forcedWeight = 0x%x\n", 
                __func__, btinfo->coexScheme.scheme, btinfo->coexScheme.bt_period, btinfo->coexScheme.bt_dutyCycle, 
                btinfo->coexScheme.bt_stompType, btinfo->coexScheme.wlan_aggrLimit * 250, btinfo->btForcedWeight);

            if (ath_bt_coex_newscheme(sc, btinfo->coexScheme.scheme) == 0) {

                btinfo->bt_coex->init(sc, btinfo);
                btinfo->bt_coex->resume(sc, btinfo);
            }
            else
            {
                DPRINTF(sc, ATH_DEBUG_ANY, "%s: Coex new scheme %d failed\n",
                    __func__, btinfo->coexScheme.scheme);

                ath_bt_coex_clear_scheme_info(&btinfo->coexScheme);
            }
        }
    } while (0);
    ATH_BT_UNLOCK(btinfo);
}

void
ath_bt_coex_disconnect(struct ath_softc *sc)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;

    ATH_PS_WAKEUP(sc);
    ATH_BT_LOCK(btinfo);
    do
    {
        if (btinfo->wlan_state == ATH_WLAN_STATE_DISCONNECT) {
                break;
        }

        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s\n", __func__);

        btinfo->wlan_state = ATH_WLAN_STATE_DISCONNECT;

        if (btinfo->bt_coexAgent) {
            ath_bt_coex_switch_scheme(sc, AH_FALSE);
        }
        else {
            ath_bt_coex_stop_scheme(sc);

            ath_pcie_pwrsave_btcoex_enable(sc, 0);
        }
    } while (0);

    ATH_BT_UNLOCK(btinfo);
    ATH_PS_SLEEP(sc);
}


void 
ath_bt_coex_reset_start(struct ath_softc *sc)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;

    ATH_BT_LOCK(btinfo);
    if ((btinfo->bt_coex) && btinfo->bt_on) {
        if (btinfo->coexScheme.scheme == ATH_BT_COEX_SCHEME_PS_POLL) {
            struct ath_bt_coex_ps_poll_info *sco = ATH_BT_TO_PSPOLL(btinfo);

            sco->in_reset = 1;
        }
        btinfo->bt_coex->pause(sc, btinfo);

        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s\n", __func__);
    }
    ATH_BT_UNLOCK(btinfo);
}

void 
ath_bt_coex_reset_end(struct ath_softc *sc)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;

    ATH_BT_LOCK(btinfo);
    if ((btinfo->bt_coex) && btinfo->bt_on) {
        btinfo->bt_coex->resume(sc, btinfo);

        if (btinfo->coexScheme.scheme == ATH_BT_COEX_SCHEME_PS_POLL) {
            struct ath_bt_coex_ps_poll_info *sco = ATH_BT_TO_PSPOLL(btinfo);

            sco->in_reset = 0;
        }
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s\n", __func__);
    }
    ATH_BT_UNLOCK(btinfo);
}

void
ath_bt_coex_scan(struct ath_softc *sc, u_int32_t scan_state)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    u_int8_t changed = 0;
    HAL_BOOL forceSwitch = AH_FALSE;

    ATH_BT_LOCK(btinfo);
    do
    {
        if (!btinfo->bt_coexAgent) {
            if ((scan_state == ATH_COEX_WLAN_SCAN_RESET) && btinfo->bt_coex) {
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: scan_state = %d\n", __func__, scan_state);
                btinfo->bt_coex->pause(sc, btinfo);
                btinfo->bt_coex->resume(sc, btinfo);
            }
            break;
        }

        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: scan_state: %d, wlan_scanState = %d, wlan_state = %d, wlan_preState = %d, rssi = %d\n",
            __func__, scan_state, btinfo->wlan_scanState, btinfo->wlan_state, btinfo->wlan_preState, btinfo->wlan_rssi);

        if (scan_state == ATH_COEX_WLAN_SCAN_START) {
            if (btinfo->wlan_state != ATH_WLAN_STATE_SCAN) {
                btinfo->wlan_preState = btinfo->wlan_state;
                changed = 1;
            }
            btinfo->wlan_state = ATH_WLAN_STATE_SCAN;
            btinfo->wlan_scanState = ATH_COEX_WLAN_SCAN_START;
        }
        else if ((scan_state == ATH_COEX_WLAN_SCAN_END) && 
                (btinfo->wlan_scanState == ATH_COEX_WLAN_SCAN_START)) 
        {
            btinfo->wlan_scanState = ATH_COEX_WLAN_SCAN_END;
        }
        else if ((scan_state == ATH_COEX_WLAN_SCAN_RESET) && 
                (btinfo->wlan_scanState == ATH_COEX_WLAN_SCAN_END))
        {
            btinfo->wlan_state = btinfo->wlan_preState;
            btinfo->wlan_scanState = ATH_COEX_WLAN_SCAN_NONE;
            changed = 1;
            forceSwitch = AH_TRUE;
        }
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: wlan_state = %d, changed = %d\n", __func__, btinfo->wlan_state, changed);

        if (changed) {
            ath_bt_coex_switch_scheme(sc, forceSwitch);
        }
    } while (0);
    ATH_BT_UNLOCK(btinfo);
}

void
ath_bt_coex_assoc(struct ath_softc *sc, u_int32_t assoc_state)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    u_int8_t changed = 0;

    ATH_BT_LOCK(btinfo);
    do
    {
        if (!btinfo->bt_coexAgent) {
                break;
        }

        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: assoc_state = %d, wlan_state = %d, wlan_preState = %d\n",
            __func__, assoc_state, btinfo->wlan_state, btinfo->wlan_preState);

        switch(assoc_state) {
            case ATH_COEX_WLAN_ASSOC_START:
                if (btinfo->wlan_state != ATH_WLAN_STATE_ASSOC) {
                    btinfo->wlan_preState = btinfo->wlan_state;
                    btinfo->wlan_state = ATH_WLAN_STATE_ASSOC;
                    changed = 1;

                    /*
                     * Change to forceStomp mode only when there is no
                     * SCO/eSCO profile and no HID.
                     * Need to add HID check once BT firmware is ready.
                     */
                    if (btinfo->numSCOProfile == 0) {
                        btinfo->forceStomp |= BT_COEX_FORCE_STOMP_ASSOC;
                        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Association enables force stomp = %d\n", __func__, btinfo->forceStomp);
                    }
                }
                break;
            case ATH_COEX_WLAN_ASSOC_END_SUCCESS:
                if (btinfo->wlan_state == ATH_WLAN_STATE_ASSOC) {
                    btinfo->wlan_state = ATH_WLAN_STATE_CONNECT;

                    if (btinfo->forceStomp & BT_COEX_FORCE_STOMP_ASSOC) {
                        btinfo->forceStomp &= (~BT_COEX_FORCE_STOMP_ASSOC);
                        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Association end disables force stomp = %d\n", __func__, btinfo->forceStomp);
                    }
                    changed = 1;
                }
                break;
            case ATH_COEX_WLAN_ASSOC_END_FAIL:
                if (btinfo->wlan_state == ATH_WLAN_STATE_ASSOC) {
                    btinfo->wlan_state = btinfo->wlan_preState;

                    if (btinfo->forceStomp & BT_COEX_FORCE_STOMP_ASSOC) {
                        btinfo->forceStomp &= (~BT_COEX_FORCE_STOMP_ASSOC);
                        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Association end disables force stomp = %d\n", __func__, btinfo->forceStomp);
                    }
                    changed = 1;
                }
                break;
            default:
                break;
        }
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: wlan_state = %d\n", __func__, btinfo->wlan_state);

        if (changed) {
            ath_bt_coex_switch_scheme(sc, AH_FALSE);
        }
        else {
            DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Assoc event no effect, event is %d\n", __func__, assoc_state);
        }
    } while (0);
    ATH_BT_UNLOCK(btinfo);
}

void
ath_bt_coex_rssi_update(struct ath_softc *sc, int8_t *rssi)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    ATH_COEX_DISABLE_PTA *disablePTA = &btinfo->disablePTA;

    ATH_BT_LOCK(btinfo);

    btinfo->wlan_rssi = *rssi;

    if (disablePTA->enable) {
        if (BT_COEX_IS_HELIUS(btinfo) && (!BT_COEX_IS_11G_MODE(sc) || 
            (BT_COEX_ACTIVE_PROFILE(btinfo, ATH_COEX_BT_PROFILE_A2DP) && BT_COEX_IS_11G_MODE(sc))))
        {
            /* Only enable disablePTA for Helius when it's in 11G mode */
            if (disablePTA->state == ATH_BT_COEX_PTA_DISABLE) {
                disablePTA->state = ATH_BT_COEX_PTA_ENABLE;
                ath_bt_coex_switch_scheme(sc, AH_FALSE);
                ath_hal_bt_setParameter(sc->sc_ah, HAL_BT_COEX_LOWER_TX_PWR, 0);
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: disable PTA is off!!!!\n",
                        __func__);
            }
        }
        else if ((btinfo->wlan_rssi >= disablePTA->rssiThreshold) &&
            (disablePTA->state == ATH_BT_COEX_PTA_ENABLE))
        {
            disablePTA->disableCnt++;

            /* Disable PTA since the RSSI is high enough */
            if (disablePTA->disableCnt >= ATH_COEX_RSSI_SWITCH_CNT) {
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Disable PTA because rssi (%d) is high. Isolation = %d dB\n",
                        __func__, btinfo->wlan_rssi, btinfo->bt_hwinfo.bt_isolation);
                disablePTA->disableCnt = 0;
                disablePTA->state = ATH_BT_COEX_PTA_DISABLE;

                ath_hal_bt_setParameter(sc->sc_ah, HAL_BT_COEX_LOWER_TX_PWR, 1);
                ath_bt_coex_switch_scheme(sc, AH_FALSE);
            }
        }
        else if ((btinfo->wlan_rssi < (disablePTA->rssiThreshold - ATH_COEX_SWITCH_EXTRA_RSSI)) &&
                 (disablePTA->state == ATH_BT_COEX_PTA_DISABLE))
        {
            disablePTA->enableCnt++;

            /* Enable PTA since RSSI is lower than threshold */
            if (disablePTA->enableCnt >= ATH_COEX_RSSI_SWITCH_CNT) {
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Enable PTA because rssi (%d) is low.\n",
                        __func__, btinfo->wlan_rssi);
                disablePTA->enableCnt = 0;
                disablePTA->state = ATH_BT_COEX_PTA_ENABLE;

                ath_bt_coex_switch_scheme(sc, AH_FALSE);
                ath_hal_bt_setParameter(sc->sc_ah, HAL_BT_COEX_LOWER_TX_PWR, 0);
            }
        }
        else {
            disablePTA->disableCnt = 0;
            disablePTA->enableCnt = 0;
        }
    }

    if (btinfo->coexScheme.scheme == ATH_BT_COEX_SCHEME_HW3WIRE) {
        struct ath_bt_coex_hw3wire_info *hw3wire = ATH_BT_TO_HW3WIRE(btinfo);
        if ( hw3wire == NULL ) {
            DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: COEX Info is unavailable. Skip the Rssi update\n",
                    __func__);
            return;
        }

        if (hw3wire->rssiProfile) {
            if ((hw3wire->rssiSpecialCase == 1) && (btinfo->wlan_rssi > (hw3wire->rssiProfileThreshold+2)))
            {
                hw3wire->rssiProfileDisableCnt++;

                if (hw3wire->rssiProfileDisableCnt >= ATH_COEX_RSSI_PROFILE_CNT) {
                    /* Need to change back to normal profile */
                    hw3wire->rssiSpecialCase = 0;
                    ath_bt_coex_switch_scheme(sc, 0);
                    DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: rssi is high! And profile should be back to normal!\n", __func__);
                }
            }
            else if ((hw3wire->rssiSpecialCase == 0) && (btinfo->wlan_rssi <= hw3wire->rssiProfileThreshold))
            {
                hw3wire->rssiProfileEnableCnt++;

                if (hw3wire->rssiProfileEnableCnt >= ATH_COEX_RSSI_PROFILE_CNT) {
                    /* Need to profile to special case if conditions are met */
                    hw3wire->rssiSpecialCase = 1;
                    ath_bt_coex_switch_scheme(sc, 0);
					DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: rssi is low! And profile should be set to special case!\n", __func__);
                }
            }
            else {
                hw3wire->rssiProfileDisableCnt = 0;
                hw3wire->rssiProfileEnableCnt = 0;
            }
        }
    }

    if (btinfo->lowACKPower) {
        if ((btinfo->lowerTxPower & BT_COEX_LOWER_ACK_PWR_RSSI) && 
            (btinfo->wlan_rssi < (btinfo->rssiTxLimit - ATH_COEX_SWITCH_EXTRA_RSSI)))
        {
            btinfo->lowACKDisableCnt++;

            if (btinfo->lowACKDisableCnt >= ATH_COEX_RSSI_SWITCH_CNT) {
                /* Need to change back to normal ACK power */
                ath_bt_coex_lower_ACK_power(sc, ATH_BT_COEX_LOW_RSSI);
            }
        }
        else if (((btinfo->lowerTxPower & BT_COEX_LOWER_ACK_PWR_RSSI) == 0) && 
            (btinfo->wlan_rssi >= btinfo->rssiTxLimit))
        {
            btinfo->lowACKEnableCnt++;

            if (btinfo->lowACKEnableCnt >= ATH_COEX_RSSI_SWITCH_CNT) {
                /* Need to lower ACK power if conditions are met */
                ath_bt_coex_lower_ACK_power(sc, ATH_BT_COEX_HIGH_RSSI);
            }
        }
        else {
            btinfo->lowACKDisableCnt = 0;
            btinfo->lowACKEnableCnt = 0;
        }
    }

    ATH_BT_UNLOCK(btinfo);
}

void 
ath_bt_coex_fullsleep(struct ath_softc *sc)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;

    ATH_BT_LOCK(btinfo);
    ath_bt_coex_stop_scheme(sc);
    ATH_BT_UNLOCK(btinfo);
}

/*
 * BT coex agent events processing
 */
void
ath_bt_coex_bt_inquiry(struct ath_softc *sc, u_int32_t state)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    u_int8_t    oldState = btinfo->bt_state;

    ATH_BT_LOCK(btinfo);
    do
    {
        if ((!btinfo->bt_coexAgent) || (state > ATH_COEX_BT_INQUIRY_END)){
                break;
        }
        if (state == ATH_COEX_BT_INQUIRY_START) {
            btinfo->bt_mgtPending++;
            btinfo->bt_mgtExtendTime = 0;
        }
        else {
            if (btinfo->bt_mgtPending) {
                btinfo->bt_mgtPending--;
                if (btinfo->bt_mgtPending == 0)
                    btinfo->bt_mgtExtendTime = MGMT_EXTEND_TIME;
            } else
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: BT state mismatch. mgtPending = %d, state = %d\n",
                        __func__, btinfo->bt_mgtPending, state);
        }

        /*
         * Change BT state to STATE_MGT whenever BT starts inquiry.
         * Make sure to stomp some high priority BT to maintain 
         * WLAN connection. (See EV 74665)
         */
        if (btinfo->bt_mgtPending) {
            btinfo->bt_state = ATH_BT_STATE_MGT;
        }
        else if ((oldState == ATH_BT_STATE_MGT) && (btinfo->bt_mgtPending == 0)) {
            if ((btinfo->numACLProfile == 0) && (btinfo->numSCOProfile == 0)) {
                btinfo->bt_state = ATH_BT_STATE_OFF;
            }
            else {
                btinfo->bt_state = ATH_BT_STATE_ON;
            }
        }

        if (oldState != btinfo->bt_state) {
            DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: BT state change: old:%d, new:%d\n",
                    __func__, oldState, btinfo->bt_state);
            ath_bt_coex_switch_scheme(sc, AH_FALSE);
        }
    } while (0);
    ATH_BT_UNLOCK(btinfo);
}

void
ath_bt_coex_bt_connect(struct ath_softc *sc, u_int32_t state)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    u_int8_t    oldState = btinfo->bt_state;

    ATH_BT_LOCK(btinfo);
    do
    {
        if ((!btinfo->bt_coexAgent) || (state > ATH_COEX_BT_CONNECT_END)) {
                break;
        }
        if (state == ATH_COEX_BT_CONNECT_START) {
            btinfo->bt_mgtPending++;
            btinfo->bt_mgtExtendTime = 0;
        }
        else {
            if (btinfo->bt_mgtPending) {
                btinfo->bt_mgtPending--;
                if (btinfo->bt_mgtPending == 0)
                    btinfo->bt_mgtExtendTime = MGMT_EXTEND_TIME;
            } else
                DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: BT state mismatch. mgtPending = %d, state = %d\n",
                        __func__, btinfo->bt_mgtPending, state);
        }

        if ((oldState == ATH_BT_STATE_OFF) && (btinfo->bt_mgtPending)) {
            btinfo->bt_state = ATH_BT_STATE_MGT;
        }
        else if ((oldState == ATH_BT_STATE_MGT) && (btinfo->bt_mgtPending == 0)) {
            btinfo->bt_state = ATH_BT_STATE_OFF;
        }

        if (oldState != btinfo->bt_state) {
            DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: BT state change: old:%d, new:%d\n",
                    __func__, oldState, btinfo->bt_state);
            ath_bt_coex_switch_scheme(sc, AH_FALSE);
        }
    } while (0);
    ATH_BT_UNLOCK(btinfo);
}

void
ath_bt_coex_bt_profile(struct ath_softc *sc, ATH_COEX_PROFILE_INFO *pInfo)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;

    ATH_BT_LOCK(btinfo);
    do
    {
        if (!btinfo->bt_coexAgent) {
                break;
        }

        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: profile type = %d, state = %d, role = %d\n", 
                __func__, pInfo->profileType, pInfo->profileState, pInfo->connectionRole);
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Current ACL:%d, SCO:%d\n", 
                __func__, btinfo->numACLProfile, btinfo->numSCOProfile);

        if (pInfo->profileState == ATH_COEX_BT_PROFILE_START) {
            /* A new profile starts */
            if (pInfo->profileType == ATH_COEX_BT_PROFILE_TYPE_ACL) {
                if (btinfo->numACLProfile < MAX_NUM_BT_ACL_PROFILE) {
                    btinfo->numACLProfile++;
                }
                else {
                    DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Too many ACL profiles\n", __func__);
                }
                /* Save handle for BDR connection */
                if (pInfo->btRate == ATH_COEX_BT_RATE_BR) {
                    btinfo->btBDRHandle = pInfo->connHandle;
                }
                ath_bt_coex_bt_profile_add(sc, pInfo);
            }
            else if (pInfo->profileType == ATH_COEX_BT_PROFILE_TYPE_SCO_ESCO) {
                if (btinfo->numSCOProfile < MAX_NUM_BT_SCO_PROFILE) {
                    btinfo->numSCOProfile++;

                    if ((btinfo->lowerTxPower & BT_COEX_LOWER_ACK_PWR_SCO) == 0){
                        ath_bt_coex_lower_ACK_power(sc, ATH_BT_COEX_SCO_ON);
                    }

                    /*
                     * SCO/eSCO link is established on an ACL link. Normally before driver gets
                     * a SCO profile event, an ACL profile event should have been received.
                     * Unless coex agent or BT firmware do this for wlan driver, wlan driver has
                     * to remove the extra ACL profile for now.
                     */
                    DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Remove one ACL profile is there is any %d\n", __func__, btinfo->numACLProfile);
                    if (btinfo->numACLProfile) {
                        btinfo->numACLProfile--;
                    }
                }
                else {
                    DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Too many SCO profiles\n", __func__);
                }
            }
        }
        else {
            /* A profile ends */
            if (pInfo->profileType == ATH_COEX_BT_PROFILE_TYPE_ACL) {
                if (btinfo->numACLProfile) {
                    btinfo->numACLProfile--;
                }
                else {
                    DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Number of ACL profiles mismatch.\n", __func__);
                }
                if (btinfo->btBDRHandle == pInfo->connHandle) {
                    btinfo->btBDRHandle = 0;
                }
                ath_bt_coex_bt_profile_delete(sc, pInfo);
            }
            else if (pInfo->profileType == ATH_COEX_BT_PROFILE_TYPE_SCO_ESCO) {
                if (btinfo->numSCOProfile) {
                    btinfo->numSCOProfile--;

                    if (btinfo->lowerTxPower & BT_COEX_LOWER_ACK_PWR_SCO) {
                        ath_bt_coex_lower_ACK_power(sc, ATH_BT_COEX_SCO_OFF);
                    }

                    /*
                     * SCO/eSCO link is established on an ACL link. The SCO profile is stoped.
                     * The ACL link might still be there. Add back the ACL profile.
                     */
                    DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Add one ACL profile back %d\n", __func__, btinfo->numACLProfile);
                    btinfo->numACLProfile++;
                }
                else {
                    DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Number of SCO profiles mismatch.\n", __func__);
                }
            }
        }

        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: ACL = %d, SCO = %d\n", __func__, btinfo->numACLProfile, btinfo->numSCOProfile);

        if (GET_SCHEME_INDEX(btinfo) && (btinfo->bt_state != ATH_BT_STATE_ON)) {
            btinfo->bt_state = ATH_BT_STATE_ON;
        }
        else if (!GET_SCHEME_INDEX(btinfo) && btinfo->bt_mgtPending) {
            btinfo->bt_state = ATH_BT_STATE_MGT;
        }
        else if (!GET_SCHEME_INDEX(btinfo) && !btinfo->bt_mgtPending) {
            btinfo->bt_state = ATH_BT_STATE_OFF;
        }
        else if (!GET_SCHEME_INDEX(btinfo)) {
            btinfo->btBDRHandle = 0;
        }

        ath_bt_coex_switch_scheme(sc, AH_FALSE);
    } while (0);
    ATH_BT_UNLOCK(btinfo);
}

void ath_bt_coex_bt_profile_type_update(struct ath_softc *sc, ATH_COEX_PROFILE_TYPE_INFO *pTypeInfo)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    int i;

    ATH_BT_LOCK(btinfo);
    do
    {
        if (!btinfo->bt_coexAgent) {
                break;
        }

        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: the ACL profile type is %d and the connection handler is %d\n", __func__, pTypeInfo->profileType, pTypeInfo->connHandle);

        for (i=0;i<MAX_NUM_BT_ACL_PROFILE;i++) {
            if ((btinfo->pList[i].connHandle == INVALID_PROFILE_TYPE_HANDLE) && (btinfo->pList[i].profileType == INVALID_PROFILE_TYPE_HANDLE)) {
                break;
            }
            else if (btinfo->pList[i].connHandle == pTypeInfo->connHandle) {
                btinfo->pList[i].profileType = pTypeInfo->profileType;
                break;
            }
        }

        ath_bt_coex_bt_active_profile(sc);

        if(sc->sc_reg_parm.bt_reg.btCoexEnhanceA2DP &&
           BT_COEX_ACTIVE_PROFILE(btinfo, ATH_COEX_BT_PROFILE_A2DP) && 
           (btinfo->numACLProfile == 1) && 
           (btinfo->numSCOProfile == 0))
        {
            /* Force to switch schcem in order to update duty cycle. */
            ath_bt_coex_switch_scheme(sc, AH_TRUE);
        }

    } while (0);
    ATH_BT_UNLOCK(btinfo);
}

void
ath_bt_coex_bt_role_switch(struct ath_softc *sc, ATH_COEX_ROLE_INFO *pRoleInfo)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    int i;

    DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: the role is changed and the new role is %d\n", __func__, pRoleInfo->connectionRole);

    ATH_BT_LOCK(btinfo);
    do
    {
        if (!btinfo->bt_coexAgent) {
                break;
        }

        for (i=0;i<MAX_NUM_BT_ACL_PROFILE;i++) {
            if ((btinfo->pList[i].connHandle == INVALID_PROFILE_TYPE_HANDLE) && 
                (btinfo->pList[i].profileType == INVALID_PROFILE_TYPE_HANDLE)) 
            {
                break;
            }
            else if (btinfo->pList[i].connHandle == pRoleInfo->connHandle) {
                if (btinfo->pList[i].connectionRole != pRoleInfo->connectionRole) {
                    btinfo->pList[i].connectionRole = pRoleInfo->connectionRole;
                }
                break;
            }
        }
    } while (0);
    ATH_BT_UNLOCK(btinfo);

    ath_bt_coex_bt_active_profile(sc);
}

void
ath_bt_coex_bt_rcv_rate(struct ath_softc *sc, ATH_COEX_BT_RCV_RATE *pRate)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    HAL_BOOL needLower = AH_FALSE;

    ATH_BT_LOCK(btinfo);
    do
    {
        int i;
        if (!btinfo->bt_coexAgent) {
            break;
        }
        for (i = 0; i < 4; i ++) {
            /* Need to lower ACK tx power when BT receiving throughput exceeds threshold */
            if ((pRate->rateHandle[i] & 0xffff) >= btinfo->btRcvThreshold) {
                needLower = AH_TRUE;
                break;
            }
        }
        if (needLower && 
            ((btinfo->lowerTxPower & BT_COEX_LOWER_ACK_PWR_XPUT) == 0))
        {
            ath_bt_coex_lower_ACK_power(sc, ATH_BT_COEX_HIGH_RCV_XPUT);
        }
        else if (!needLower && (btinfo->lowerTxPower & BT_COEX_LOWER_ACK_PWR_XPUT)) {
            ath_bt_coex_lower_ACK_power(sc, ATH_BT_COEX_LOW_RCV_XPUT);
        }
    } while (0);
    ATH_BT_UNLOCK(btinfo);

    ath_bt_coex_bt_active_profile(sc);
}

/*
 * Functions for IEEE80211 layer
 */

static u_int32_t
ath_bt_coex_sco_stomp(struct ath_softc *sc, u_int32_t value)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    struct ath_bt_coex_ps_poll_info *sco = ATH_BT_TO_PSPOLL(btinfo);

    if (value) {
        if (!sco->resync) {
            ath_bt_stomp_coex(sc, btinfo, HAL_BT_COEX_STOMP_ALL);
            btinfo->bt_stomping = 1;
        }
        else {
            btinfo->bt_stomping = 0;
        }
    }
    else {
        ath_bt_stomp_coex(sc, btinfo, btinfo->coexScheme.bt_stompType);
        btinfo->bt_stomping = 0;
    }

    return btinfo->bt_stomping;
}

u_int32_t
ath_bt_coex_ps_complete(ath_dev_t dev, u_int32_t type)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_bt_info *btinfo = &sc->sc_btinfo;

    ATH_BT_LOCK(btinfo);

    if (!btinfo->bt_coex) {
        ATH_BT_UNLOCK(btinfo);
        return 0;
    }

    if (btinfo->coexScheme.scheme == ATH_BT_COEX_SCHEME_PS_POLL) {
        u_int32_t   isstomp;

        switch(type) {
            case ATH_BT_COEX_PS_POLL_DATA:
                isstomp = ath_bt_coex_sco_stomp(sc, 0);
                break;
            case ATH_BT_COEX_PS_POLL:
                isstomp = ath_bt_coex_sco_stomp(sc, 1);
                break;
            case ATH_BT_COEX_PS_POLL_ABORT:
                isstomp = ath_bt_coex_sco_stomp(sc, 0);
                break;
            default:
                isstomp = 0;
                break;
        }
    }
    else if ((type == ATH_BT_COEX_PS_ENABLE) && (btinfo->coexScheme.scheme == ATH_BT_COEX_SCHEME_PS)) {
        struct ath_bt_coex_ps_info *a2dp = ATH_BT_TO_PS(btinfo);

        if (a2dp->wait_for_complete && a2dp->defer_dcycle) {
            a2dp->defer_dcycle = 0;
            /*
             * When A2DP duty cycle starts, WLAN traffic must be put to a low priority.
             */
            ath_bt_stomp_coex(sc, btinfo, btinfo->coexScheme.bt_stompType);
        }

    }
    else if (btinfo->coexScheme.scheme == ATH_BT_COEX_SCHEME_HW3WIRE) {
        struct ath_bt_coex_hw3wire_info *hw3wire = ATH_BT_TO_HW3WIRE(btinfo);

        if ((hw3wire->psPending) && (type == ATH_BT_COEX_PS_DISABLE)) {
            hw3wire->psPending = 0;
        }
    }
    ATH_BT_UNLOCK(btinfo);

    return 1;
}
int
ath_bt_coex_get_info(ath_dev_t dev, u_int32_t infoType, void *info)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    u_int32_t retval = FALSE;

    switch (infoType) {
    case ATH_BT_COEX_INFO_SCHEME:
        retval = sc->sc_btinfo.coexScheme.scheme;
        break;
    case ATH_BT_COEX_INFO_BTBUSY:
        if (sc->sc_hasbtcoex) {
            retval = ( btinfo->bt_mgtPending || 
                   btinfo->bt_mgtExtendTime || 
                   btinfo->numACLProfile || 
                   btinfo->numSCOProfile ) ? TRUE : FALSE;
        } else {
            retval = FALSE;
        }
        break;
    case ATH_BT_COEX_INFO_ALL:
        {
            struct ath_bt_coex_comp_info *pCoexInfo = (struct ath_bt_coex_comp_info *)info;
            pCoexInfo->bt_enable = btinfo->bt_enable;
            pCoexInfo->bt_on     = btinfo->bt_on;
            pCoexInfo->bt_state = btinfo->bt_state;
            pCoexInfo->wlan_state = btinfo->wlan_state;
            pCoexInfo->wlan_preState = btinfo->wlan_preState;
            pCoexInfo->wlan_scanState = btinfo->wlan_scanState;
            pCoexInfo->wlan_channel = btinfo->wlan_channel;
            pCoexInfo->wlan_rssi = btinfo->wlan_rssi;
            pCoexInfo->bt_stomping = btinfo->bt_stomping;
            pCoexInfo->bt_coexAgent = btinfo->bt_coexAgent;
            pCoexInfo->bt_sco_tm_low = btinfo->bt_sco_tm_low;
            pCoexInfo->bt_sco_tm_high = btinfo->bt_sco_tm_high;
            pCoexInfo->bt_sco_tm_intrvl = btinfo->bt_enable;
            pCoexInfo->bt_sco_tm_idle = btinfo->bt_sco_tm_idle;
            pCoexInfo->bt_sco_delay = btinfo->bt_sco_delay;
        }
        break;
    default:
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: Unsupported info type %d.\n", __func__, infoType);
        retval = -1;
    }
    return retval;
}

void 
ath_bt_coex_slave_pan_update(struct ath_softc *sc)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;
    u_int8_t oldstate = btinfo->slavePan;
    
    if (BT_COEX_ACTIVE_PROFILE(btinfo, ATH_COEX_BT_PROFILE_PAN) && 
        (!(btinfo->lowerTxPower & BT_COEX_LOWER_ACK_PWR_XPUT)) &&
        ((btinfo->numACLProfile + btinfo->numSCOProfile) == 1) &&
        (btinfo->pList[0].connectionRole == ATH_COEX_BT_ROLE_SLAVE))
    {
        btinfo->slavePan = AH_TRUE;
    }
    else {
        btinfo->slavePan = AH_FALSE;
    }

    if (oldstate != btinfo->slavePan) {
        DPRINTF(sc, ATH_DEBUG_BTCOEX, "%s: slave pan is caled and btRole is %d, profile is %d, lowerTxPower is %x\n", 
            __func__, btinfo->pList[0].connectionRole, BT_COEX_ACTIVE_PROFILE(btinfo, ATH_COEX_BT_PROFILE_PAN), 
            btinfo->lowerTxPower);
        ath_bt_coex_switch_scheme(sc, AH_TRUE);
    }
}

#ifdef ATH_USB
void
ath_bt_coex_throughput(struct ath_softc *sc, u_int8_t enable)
{
    struct ath_bt_info *btinfo = &sc->sc_btinfo;

    ATH_BT_LOCK(btinfo);
    do
    {
        if (btinfo->wlan_state != ATH_WLAN_STATE_CONNECT) {
            break;
        }
        
        btinfo->wlan_tx_state = (enable)? ATH_USB_WLAN_TX_HIGH : ATH_USB_WLAN_TX_LOW;

        ath_bt_coex_switch_scheme(sc, AH_FALSE);
    } while (0);
    ATH_BT_UNLOCK(btinfo);
}
#endif

#endif /* ATH_BT_COEX */
