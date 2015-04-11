/*
 * Copyright (c) 2005, Atheros Communications Inc.
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

/*! \file ath_beacon.c
**  \brief ATH Beacon Processing
**
**  This file contains the implementation of the common beacon support code for
**  the ATH layer, including any tasklets/threads used for beacon support.
**
*/

#include "ath_internal.h"
#include "ath_txseq.h"
#ifdef ATH_SUPPORT_DFS
#include "dfs.h"
#endif

#if ATH_SUPPORT_SPECTRAL
#include "spectral.h"
#endif

#include "ath_green_ap.h"
#include "ratectrl.h"

#ifndef REMOVE_PKT_LOG
#include "pktlog.h"
extern struct ath_pktlog_funcs *g_pktlog_funcs;
#endif

#define IEEE80211_MS_TO_TU(x)   (((x) * 1000) / 1024)

typedef struct _ath_vap_info {
    int if_id;
    HAL_OPMODE opmode;
} ath_vap_info;

typedef enum {
    ATH_ALLOC_BEACON_SLOT,
    ATH_FREE_BEACON_SLOT,
    ATH_ALLOC_FIRST_SLOT /* for sta vap chip in sta mode */
} ath_beacon_slot_op;

#define TU_TO_TSF(_tsf) ((_tsf)<<10)
#define TSF_TO_TU(_h,_l) \
    ((((u_int32_t)(_h)) << 22) | (((u_int32_t)(_l)) >> 10))

#if ATH_IBSS_DFS_CHANNEL_SUPPORT 
#define is_dfs_wait(_sc)            ((_sc)->sc_dfs && (_sc)->sc_dfs->sc_dfswait)
#else
#define is_dfs_wait(_sc)            false
#endif

#ifdef ATH_SUPPORT_DFS
static u_int32_t ath_get_phy_err_rate(struct ath_softc *sc);
int get_dfs_hang_war_timeout(struct ath_softc *sc);
#endif
static u_int32_t ath_beacon_config_sta_timers(struct ath_softc *sc, ieee80211_beacon_config_t *conf, int tbtt_timer_period, u_int64_t tsf);
static void ath_beacon_config_ap_timers(struct ath_softc *sc, u_int32_t nexttbtt, u_int32_t intval);
static void ath_beacon_config_ibss_timers(struct ath_softc *sc, u_int32_t nexttbtt, u_int32_t intval);
static void ath_beacon_config_sta_mode(struct ath_softc *sc, ath_beacon_config_reason reason, int if_id );
static void ath_beacon_config_ap_mode(struct ath_softc *sc, ath_beacon_config_reason reason, int if_id );
#if ATH_SUPPORT_IBSS
static void ath_beacon_config_ibss_mode(struct ath_softc *sc, ath_beacon_config_reason reason, int if_id );
#endif
static void ath_get_beacon_config(struct ath_softc *sc, int if_id, ieee80211_beacon_config_t *conf);
static void ath_set_num_slots(struct ath_softc *sc);
static void ath_beacon_config_beacon_slot(struct ath_softc *sc, ath_beacon_slot_op config_op, int if_id );
static void ath_beacon_config_tsf_offset(struct ath_softc *sc, int if_id );
static void ath_beacon_config_all_tsf_offset(struct ath_softc *sc );
static int ath_get_all_active_vap_info(struct ath_softc *sc, ath_vap_info *av_info, int exclude_id);


/******************************************************************************/
/*!
**  \brief Setup a h/w transmit queue for beacons.
**
**  This function allocates an information structure (HAL_TXQ_INFO) on the stack,
**  sets some specific parameters (zero out channel width min/max, and enable aifs)
**  The info structure does not need to be persistant.
**
**  \param sc Pointer to the SoftC structure for the ATH object
**  \param nbufs Number of buffers to allocate for the transmit queue
**
**  \return Returns the queue index number (priority), or -1 for failure
*/

int
ath_beaconq_setup(struct ath_softc *sc, struct ath_hal *ah)
{
    HAL_TXQ_INFO qi;

    OS_MEMZERO(&qi, sizeof(qi));
    qi.tqi_aifs = 1;
    qi.tqi_cwmin = 0;
    qi.tqi_cwmax = 0;
#ifdef ATH_SUPERG_DYNTURBO
    qi.tqi_qflags = TXQ_FLAG_TXDESCINT_ENABLE;
#endif

    /*
     * For the Tx Beacon Notify feature, we need to enable the interrupt for 
     * Beacon Queue. But the individual beacon buffer still needs to set its 
     * enable completion interrupt in order to get interrupted.
     */
    qi.tqi_qflags = TXQ_FLAG_TXDESCINT_ENABLE;

    if (sc->sc_enhanceddmasupport) {
        qi.tqi_qflags = TXQ_FLAG_TXOKINT_ENABLE | TXQ_FLAG_TXERRINT_ENABLE;
    }

    /* NB: don't enable any interrupts */
    return ath_hal_setuptxqueue(ah, HAL_TX_QUEUE_BEACON, &qi);
}


/******************************************************************************/
/*!
**  \brief Configure parameters for the beacon queue
**
**  This function will modify certain transmit queue properties depending on
**  the operating mode of the station (AP or AdHoc).  Parameters are AIFS
**  settings and channel width min/max
**
**  \param sc Pointer to ATH object ("this" pointer)
**
**  \return zero for failure, or 1 for success
*/

static int
ath_beaconq_config(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
    HAL_TXQ_INFO qi;

    ath_hal_gettxqueueprops(ah, sc->sc_bhalq, &qi);
    if (sc->sc_opmode == HAL_M_HOSTAP || sc->sc_opmode == HAL_M_STA)
    {
        /*
         * Always burst out beacon and CAB traffic.
         */
        qi.tqi_aifs = 1;
        qi.tqi_cwmin = 0;
        qi.tqi_cwmax = 0;
    }
    else
    {
        /*
         * Adhoc mode; important thing is to use 2x cwmin.
         */
        qi.tqi_aifs = sc->sc_beacon_qi.tqi_aifs;
        qi.tqi_cwmin = 2*sc->sc_beacon_qi.tqi_cwmin;
        qi.tqi_cwmax = sc->sc_beacon_qi.tqi_cwmax;
    }

    if (!ath_hal_settxqueueprops(ah, sc->sc_bhalq, &qi))
    {
        printk("%s: unable to update h/w beacon queue parameters\n",
               __func__);
        return 0;
    }
    else
    {
        ath_hal_resettxqueue(ah, sc->sc_bhalq); /* push to h/w */
        /* in DFS channel in case above function overwrites interframe spacing adjustment */
#if ATH_SUPPORT_DFS
        if((sc->sc_curchan.privFlags & CHANNEL_DFS) && (!sc->sc_scanning) &&(!sc->sc_nostabeacons)) {
            ath_adjust_difs(sc);
        }
#endif
        return 1;
    }
}

/******************************************************************************/
/*!
**  \brief Allocate and setup an initial beacon frame.
**
**  Allocate a beacon state variable for a specific VAP instance created on
**  the ATH interface.  This routine also calculates the beacon "slot" for
**  staggared beacons in the mBSSID case.
**
**  \param sc Pointer to ATH object ("this" pointer).
**  \param if_id Index of the specific VAP of interest.
**
**  \return -EINVAL if there is no function in the upper layer assigned to
**  \return         beacon transmission
**  \return -ENOMEM if no wbuf is available
**  \return   0     for success
*/

int
ath_beacon_alloc(struct ath_softc *sc, int if_id)
{
    struct ath_vap *avp;
    struct ath_buf *bf;
    wbuf_t wbuf;

    /*
    ** Code Begins
    */

    avp = sc->sc_vaps[if_id];
    ASSERT(avp);

    /* Allocate a beacon descriptor if we haven't done so. */
    if(!avp->av_bcbuf)
    {
        /*
         * Allocate beacon state for hostap/ibss.  We know
         * a buffer is available.
         */

        avp->av_bcbuf = TAILQ_FIRST(&sc->sc_bbuf);
        TAILQ_REMOVE(&sc->sc_bbuf, avp->av_bcbuf, bf_list);

        /* set up this buffer */
        ATH_TXBUF_RESET(avp->av_bcbuf, sc->sc_num_txmaps);

    }

    /*
     * release the previous beacon frame , if it already exists.
     */
    bf = avp->av_bcbuf;
    if (bf->bf_mpdu != NULL)
    {
        ieee80211_tx_status_t tx_status;

        wbuf = (wbuf_t)bf->bf_mpdu;
        wbuf_unmap_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                          OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
        tx_status.flags = 0;
        tx_status.retries = 0;
#ifdef  ATH_SUPPORT_TxBF
        tx_status.txbf_status = 0;
#endif
        sc->sc_ieee_ops->tx_complete(wbuf, &tx_status);
        bf->bf_mpdu = NULL;
    }

    /*
     * NB: the beacon data buffer must be 32-bit aligned;
    * we assume the wbuf routines will return us something
     * with this alignment (perhaps should assert).
     */
    if (!sc->sc_ieee_ops->get_beacon)
    {
        /* Protocol layer doesn't support beacon generation for host driver */
        return -EINVAL;
    }
    wbuf = sc->sc_ieee_ops->get_beacon(sc->sc_ieee, if_id, &avp->av_boff, &avp->av_btxctl);
    if (wbuf == NULL)
    {
        DPRINTF(sc, ATH_DEBUG_BEACON, "%s: cannot get wbuf\n",
                __func__);
        sc->sc_stats.ast_be_nobuf++;
        return -ENOMEM;
    }

    bf->bf_buf_addr[0] = wbuf_map_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                                      OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
    bf->bf_mpdu = wbuf;

    /*
     * if min rate is set then
     * get the corresponsding rate code and
     * save it in the vap.
     */
    if(avp->av_btxctl.min_rate) { 
        avp->av_beacon_rate_code = ath_rate_findrc((ath_dev_t) sc, avp->av_btxctl.min_rate); /* get the rate code */
    }

    return 0;
}

/******************************************************************************/
/*!
**  \brief Setup the beacon frame for transmit.
**
**  Associates the beacon frame buffer with a transmit descriptor.  Will set
**  up all required antenna switch parameters, rate codes, and channel flags.
**  Beacons are always sent out at the lowest rate, and are not retried.
**
**  \param sc Pointer to ATH object ("this" pointer)
**  \param avp Pointer to VAP object (ieee802.11 layer object)for the beacon
**  \param bf Pointer to ATH buffer containing the beacon frame.
**  \return N/A
*/

static void
ath_beacon_setup(struct ath_softc *sc, struct ath_vap *avp, struct ath_buf *bf)
{
    wbuf_t wbuf = (wbuf_t)bf->bf_mpdu;
    struct ath_hal *ah = sc->sc_ah;
    struct ath_desc *ds;
    int flags;
    const HAL_RATE_TABLE *rt;
    u_int8_t rix, rate;
    int ctsrate = 0;
    int ctsduration = 0;
    HAL_11N_RATE_SERIES  series[4];
    u_int32_t smartAntenna = 0;

    /*
    ** Code Begins
    */

    DPRINTF(sc, ATH_DEBUG_BEACON_PROC, "%s: m %p len %u\n",
            __func__, wbuf, wbuf_get_pktlen(wbuf));

    /* setup descriptors */
    ds = bf->bf_desc;

    flags = HAL_TXDESC_NOACK;
#ifdef ATH_SUPERG_DYNTURBO
    if (sc->sc_dturbo_switch)
        flags |= HAL_TXDESC_INTREQ;
#endif

    if (atomic_read(&sc->sc_has_tx_bcn_notify)) {
        /* Let the completion of this Beacon buffer Tx triggers an interrupt */
        /* NOTE: This per descriptor interrrupt is only available in Non-Osprey hw. */
        flags |= HAL_TXDESC_INTREQ;
    }

    if (sc->sc_opmode == HAL_M_IBSS && 
        sc->sc_hasveol &&
        !is_dfs_wait(sc)) 
    {
        ath_hal_setdesclink(ah, ds, bf->bf_daddr); /* self-linked */
        flags |= HAL_TXDESC_VEOL;
    }
    else
    {
        ath_hal_setdesclink(ah, ds, 0);
    }

    /*
     * Calculate rate code.
     */
    rix = sc->sc_minrateix;
    rt = sc->sc_currates;

    /*
     * if the vap requests a specific rate for beacon then
     * use it insted of minrateix. 
     */
    if (avp->av_beacon_rate_code) {
        rate = avp->av_beacon_rate_code;
    } else {
       rate = rt->info[rix].rateCode;
    }
    if (avp->av_btxctl.shortPreamble)
        rate |= rt->info[rix].shortPreamble;

#ifndef REMOVE_PKT_LOG
    bf->bf_vdata = wbuf_header(wbuf);
#endif

    /* NB: beacon's BufLen must be a multiple of 4 bytes */
    bf->bf_buf_len[0] = roundup(wbuf_get_pktlen(wbuf), 4);

    ath_hal_set11n_txdesc(ah, ds
                          , wbuf_get_pktlen(wbuf) + IEEE80211_CRC_LEN /* frame length */
                          , HAL_PKT_TYPE_BEACON                 /* Atheros packet type */
                          , avp->av_btxctl.txpower              /* txpower XXX */
                          , HAL_TXKEYIX_INVALID                 /* no encryption */
                          , HAL_KEY_TYPE_CLEAR                  /* no encryption */
                          , flags                               /* no ack, veol for beacons */
                          );

    ath_hal_filltxdesc(ah, ds
                       , bf->bf_buf_addr           /* buffer address */
                       , bf->bf_buf_len            /* buffer length */
                       , 0                                      /* descriptor id */
                       , sc->sc_bhalq                           /* QCU number */
                       , HAL_KEY_TYPE_CLEAR                     /* key type */
                       , AH_TRUE                                /* first segment */
                       , AH_TRUE                                /* last segment */
                       , ds                                     /* first descriptor */
                       );

    OS_MEMZERO(series, sizeof(HAL_11N_RATE_SERIES) * 4);
    series[0].Tries = 1;
    series[0].Rate = rate;
    series[0].RateIndex = rix;
    series[0].TxPowerCap = IS_CHAN_2GHZ(&sc->sc_curchan) ?
        sc->sc_config.txpowlimit2G : sc->sc_config.txpowlimit5G;
    series[0].ChSel = ath_txchainmask_reduction(sc,
            ath_green_ap_is_powersave_on(sc) ? ATH_CHAINMASK_ONE_CHAIN : sc->sc_tx_chainmask,
            rate);

    series[0].RateFlags = 0;

#if UMAC_SUPPORT_SMARTANTENNA
    if(sc->sc_smartant_enable)
    {
        /* same default antenna will be used for all rate series */
        smartAntenna = (sc->sc_defant) |(sc->sc_defant << 8)| (sc->sc_defant << 16) | (sc->sc_defant << 24); 
    }
    else    
    {
        smartAntenna = SMARTANT_INVALID; /* if smart antenna is not enabled */
    }
#else
    smartAntenna = SMARTANT_INVALID;
#endif    

    ath_hal_set11n_ratescenario(ah, ds, ds, 0, ctsrate, ctsduration, series, 4, 0, smartAntenna);

    /* NB: The desc swap function becomes void, 
     * if descriptor swapping is not enabled
     */
    ath_desc_swap(ds);
}

/******************************************************************************/
/*!
**  \brief Generate beacon frame and queue cab data for a vap.
**
**  Updates the contents of the beacon frame.  It is assumed that the buffer for
**  the beacon frame has been allocated in the ATH object, and simply needs to
**  be filled for this cycle.  Also, any CAB (crap after beacon?) traffic will
**  be added to the beacon frame at this point.
**
**  \param sc Pointer to ATH object ("this" pointer)
**  \param if_id Index to VAP object that is sending the beacon
**  \param needmark Pointer to integer.  Only used in dynturbo mode, and deprecated at that!
**  \return a pointer to an allocated ath buffer containing the beacon frame,
**  \return or NULL if not successful
*/
/*
 *
 */
static struct ath_buf *
ath_beacon_generate(struct ath_softc *sc, int if_id, int *needmark)
{
    struct ath_hal *ah = sc->sc_ah;
    struct ath_buf *bf;
    struct ath_vap *avp;
    wbuf_t wbuf;
    int ncabq;

#ifdef ATH_SUPERG_XR
    if (ieee80211vap_has_flag(vap, IEEE80211_F_XR))
    {
        vap->iv_xrbcnwait++;
        /* wait for XR_BEACON_FACTOR times before sending the beacon */
        if (vap->iv_xrbcnwait < IEEE80211_XR_BEACON_FACTOR)
            return NULL;
        vap->iv_xrbcnwait = 0;
    }
#endif

    avp = sc->sc_vaps[if_id];
    ASSERT(avp);

    atomic_inc(&avp->av_beacon_cabq_use_cnt);
    if (atomic_read(&avp->av_stop_beacon)) {
        atomic_dec(&avp->av_beacon_cabq_use_cnt);
        return NULL;
    }

#if ATH_SUPPORT_AP_WDS_COMBO
    if (avp->av_config.av_no_beacon) {
        atomic_dec(&avp->av_beacon_cabq_use_cnt);
        return NULL;
    }
#endif

    if (avp->av_bcbuf == NULL)
    {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: avp=%p av_bcbuf=%p\n",
                __func__, avp, avp->av_bcbuf);
        atomic_dec(&avp->av_beacon_cabq_use_cnt);
        return NULL;
    }
    bf = avp->av_bcbuf;
    wbuf = (wbuf_t)bf->bf_mpdu;

#ifdef ATH_SUPERG_DYNTURBO
    /*
     * If we are using dynamic turbo, update the
     * capability info and arrange for a mode change
     * if needed.
     */
    if (sc->sc_dturbo)
    {
        u_int8_t dtim;
        dtim = ((avp->av_boff.bo_tim[2] == 1) || (avp->av_boff.bo_tim[3] == 1));
#ifdef notyet
        ath_beacon_dturbo_update(vap, needmark, dtim);
#endif
    }
#endif
    /*
     * Update dynamic beacon contents.  If this returns
     * non-zero then we need to remap the memory because
     * the beacon frame changed size (probably because
     * of the TIM bitmap).
     */
    ncabq = avp->av_mcastq.axq_depth;

    if (!sc->sc_ieee_ops->update_beacon)
    {
        /*
         * XXX: if protocol layer doesn't support update beacon at run-time,
         * we have to free the old beacon and allocate a new one.
         */
        if (sc->sc_ieee_ops->get_beacon)
        {
            ieee80211_tx_status_t tx_status;

            wbuf_unmap_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
            tx_status.flags = 0;
            tx_status.retries = 0;
#ifdef  ATH_SUPPORT_TxBF
            tx_status.txbf_status = 0;
#endif
            sc->sc_ieee_ops->tx_complete(wbuf, &tx_status);

            wbuf = sc->sc_ieee_ops->get_beacon(sc->sc_ieee, if_id, &avp->av_boff, &avp->av_btxctl);
            if (wbuf == NULL) {
                atomic_dec(&avp->av_beacon_cabq_use_cnt);
                return NULL;
            }

            bf->bf_mpdu = wbuf;
            bf->bf_buf_addr[0] = wbuf_map_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
        }
    }
    else
    {
        int retVal = sc->sc_ieee_ops->update_beacon(sc->sc_ieee, if_id, &avp->av_boff, wbuf, ncabq);
        if (retVal == 1) {
            ath_beacon_seqno_set(avp->av_btxctl.an, wbuf);
            wbuf_unmap_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
            bf->bf_buf_addr[0] = wbuf_map_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
        } else if (retVal == 0) {
            ath_beacon_seqno_set(avp->av_btxctl.an, wbuf);
            OS_SYNC_SINGLE(sc->sc_osdev,
                       bf->bf_buf_addr[0], wbuf_get_pktlen(wbuf), BUS_DMA_TODEVICE,
                       OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
        } else if (retVal < 0) {
            /* ieee layer does not want to send beacon */
            atomic_dec(&avp->av_beacon_cabq_use_cnt);
            return NULL;
        }
    }

#if ATH_IBSS_DFS_CHANNEL_SUPPORT 
    if (is_dfs_wait(sc)) {
        atomic_dec(&avp->av_beacon_cabq_use_cnt);
        return NULL;        
    }
#endif

    /*
     * if the CABQ traffic from previous DTIM is pending and the current
     *  beacon is also a DTIM.
     *  1) if there is only one vap let the cab traffic continue.
     *  2) if there are more than one vap and we are using staggered
     *     beacons, then drain the cabq by dropping all the frames in
     *     the cabq so that the current vaps cab traffic can be scheduled.
     */
    if (ncabq && (avp->av_boff.bo_tim[4] & 1) && sc->sc_cabq->axq_depth)
    {
        if (sc->sc_nvaps > 1 && sc->sc_stagbeacons)
        {
            ath_tx_draintxq(sc, sc->sc_cabq, AH_FALSE);
            DPRINTF(sc, ATH_DEBUG_BEACON,
                    "%s: flush previous cabq traffic\n", __func__);
        }
    }

    /*
     * Construct tx descriptor.
     */
    ath_beacon_setup(sc, avp, bf);

    /*
     * Enable the CAB queue before the beacon queue to
     * insure cab frames are triggered by this beacon.
     */
    if (avp->av_boff.bo_tim[4] & 1)
    {   /* NB: only at DTIM */
        struct ath_txq *cabq = sc->sc_cabq;
        struct ath_buf *bfmcast;
        /*
         * Move everything from the vap's mcast queue
         * to the hardware cab queue.
         * XXX MORE_DATA bit?
         */
        ATH_TXQ_LOCK(&avp->av_mcastq);
        ATH_TXQ_LOCK(cabq);

        if (sc->sc_enhanceddmasupport)
        {
            /*
             * move all the buffers from av_mcastq to 
             * the CAB queue.
             */
            bfmcast = TAILQ_FIRST(&avp->av_mcastq.axq_q);
            if (bfmcast) 
            {
                if (cabq->axq_depth < HAL_TXFIFO_DEPTH) 
                {
                    if (sc->sc_nvaps > 1 && !sc->sc_stagbeacons) {
                        struct ath_txq *burstbcn_cabq = &sc->sc_burstbcn_cabq;
                        struct ath_buf *lbf;

#define DESC2PA(_sc, _va)       \
                ((caddr_t)(_va) - (caddr_t)((_sc)->sc_txdma.dd_desc) + \
                                (_sc)->sc_txdma.dd_desc_paddr)

                        if (!TAILQ_EMPTY(&burstbcn_cabq->axq_q)) {
                            if (!burstbcn_cabq->axq_link) {
                                printk("Oops! burstbcn_cabq not empty but axq_link is NULL?\n");
                                lbf = TAILQ_LAST(&burstbcn_cabq->axq_q, ath_bufhead_s);
                                burstbcn_cabq->axq_link = lbf->bf_desc;
                            }

                            ath_hal_setdesclink(ah, burstbcn_cabq->axq_link, bfmcast->bf_daddr);
                            OS_SYNC_SINGLE(sc->sc_osdev, (dma_addr_t)(DESC2PA(sc, burstbcn_cabq->axq_link)),
                                    sc->sc_txdesclen, BUS_DMA_TODEVICE, NULL);
                            DPRINTF(sc, ATH_DEBUG_BEACON_PROC, " %s: link(%p)=%llx (%p)\n", __func__, 
                                    burstbcn_cabq->axq_link, ito64(bfmcast->bf_daddr), bfmcast->bf_desc);
                        }

                        lbf = TAILQ_LAST(&avp->av_mcastq.axq_q, ath_bufhead_s);
                        ASSERT(avp->av_mcastq.axq_link == lbf->bf_desc);

                        sc->sc_burstbcn_cabq.axq_totalqueued += avp->av_mcastq.axq_totalqueued;
                        TAILQ_CONCAT(&burstbcn_cabq->axq_q, &avp->av_mcastq.axq_q, bf_list);
                        burstbcn_cabq->axq_link = avp->av_mcastq.axq_link;
                        avp->av_mcastq.axq_depth=0;
                        avp->av_mcastq.axq_totalqueued = 0;
                        avp->av_mcastq.axq_linkbuf = 0;
                        avp->av_mcastq.axq_link = NULL;
#undef DESC2PA
                    }
                    else {
                        ATH_EDMA_TXQ_MOVE_MCASTQ(&avp->av_mcastq, cabq);

                        /* Write the tx descriptor address into the hw fifo */
                        ASSERT(bfmcast->bf_daddr != 0);
                        ath_hal_puttxbuf(ah, cabq->axq_qnum, bfmcast->bf_daddr);
                    }
                }
            }
        }
        else if ((bfmcast = TAILQ_FIRST(&avp->av_mcastq.axq_q)) != NULL)
        {
            /* link the descriptors */
            if (cabq->axq_link == NULL)
            {
                ath_hal_puttxbuf(ah, cabq->axq_qnum, bfmcast->bf_daddr);
            }
            else
            {
#ifdef AH_NEED_DESC_SWAP
	            *cabq->axq_link = cpu_to_le32(bfmcast->bf_daddr);
#else
                *cabq->axq_link = bfmcast->bf_daddr;
#endif
            }
            /* append the private vap mcast list to  the cabq */
            ATH_TXQ_MOVE_MCASTQ(&avp->av_mcastq, cabq);
        }
#ifdef ATH_ADDITIONAL_STATS
        sc->sc_stats.ast_txq_packets[cabq->axq_qnum]++;
#endif

        /* NB: gated by beacon so safe to start here */
        if (!TAILQ_EMPTY(&(cabq->axq_q)))
        {
            ath_hal_txstart(ah, cabq->axq_qnum);
        }
        ATH_TXQ_UNLOCK(cabq);
        ATH_TXQ_UNLOCK(&avp->av_mcastq);
    }

    atomic_dec(&avp->av_beacon_cabq_use_cnt);
    return bf;
}

#if ATH_SUPPORT_IBSS
/******************************************************************************/
/*!
**  \brief Startup beacon transmission for adhoc mode
**
** Startup beacon transmission for adhoc mode when they are sent entirely
** by the hardware using the self-linked descriptor + veol trick.
**
**  \param sc Pointer to ATH object. "This" pointer.
**  \param if_id Integer containing index of VAP interface to start the beacon on.
**
**  \return N/A
*/

static void
ath_beacon_start_adhoc(struct ath_softc *sc, int if_id)
{
    struct ath_hal *ah = sc->sc_ah;
    struct ath_buf *bf;
    struct ath_vap *avp;

    /*
    ** Code Begins
    */

    avp = sc->sc_vaps[if_id];
    ASSERT(avp);
    
    if (avp->av_bcbuf == NULL)
    {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: avp=%p av_bcbuf=%p\n",
                __func__, avp, avp != NULL ? avp->av_bcbuf : NULL);
        return;
    }
    bf = avp->av_bcbuf;

    /* XXX: We don't do ATIM, so we don't need to update beacon contents */
#if 0
    wbuf_t wbuf;
    wbuf = (wbuf_t)bf->bf_mpdu;
    /*
     * Update dynamic beacon contents.  If this returns
     * non-zero then we need to remap the memory because
     * the beacon frame changed size (probably because
     * of the TIM bitmap).
     */
    /*
     * XXX: This function should be called from ath_beacon_tasklet,
     * which is in ISR context, thus use the locked version.
     * Better to have an assertion to verify that.
     */
    if (sc->sc_ieee_ops->update_beacon &&
        sc->sc_ieee_ops->update_beacon(ni, &avp->av_boff, wbuf, 0) == 0)
    {
        wbuf_unmap_single(sc->sc_osdev, wbuf,
                          BUS_DMA_TODEVICE,
                          OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
        bf->bf_buf_addr = wbuf_map_single(sc->sc_osdev, wbuf,
                                          BUS_DMA_TO_DEVICE,
                                          OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
    }
#endif

    /*
     * Construct tx descriptor.
     */
    ath_beacon_setup(sc, avp, bf);

    /* NB: caller is known to have already stopped tx dma */
    ath_hal_puttxbuf(ah, sc->sc_bhalq, bf->bf_daddr);
    ath_hal_txstart(ah, sc->sc_bhalq);
    DPRINTF(sc, ATH_DEBUG_BEACON_PROC, "%s: TXDP%u = %llx (%p)\n", __func__,
            sc->sc_bhalq, ito64(bf->bf_daddr), bf->bf_desc);
}
#endif

/******************************************************************************/
/*!
**  \brief Reclaim beacon resources and return buffer to the pool.
**
**  Checks the VAP to put the beacon frame buffer back to the ATH object
**  queue, and de-allocates any wbuf frames that were sent as CAB traffic.
**
**  \param sc Pointer to ATH object, "this" pointer.
**  \param avp Pointer to VAP object that sent the beacon
**
**  \return N/A
*/

void
ath_beacon_return(struct ath_softc *sc, struct ath_vap *avp)
{
    if (avp->av_bcbuf != NULL)
    {
        struct ath_buf *bf;

        bf = avp->av_bcbuf;
        if (bf->bf_mpdu != NULL)
        {
            wbuf_t wbuf = (wbuf_t)bf->bf_mpdu;
            ieee80211_tx_status_t tx_status;

            wbuf_unmap_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
            tx_status.flags = 0;
            tx_status.retries = 0;
#ifdef  ATH_SUPPORT_TxBF
            tx_status.txbf_status = 0;
#endif
            sc->sc_ieee_ops->tx_complete(wbuf, &tx_status);
            bf->bf_mpdu = NULL;
        }
        TAILQ_INSERT_TAIL(&sc->sc_bbuf, bf, bf_list);

        avp->av_bcbuf = NULL;
    }
}

/******************************************************************************/
/*!
**  \brief Reclaim beacon resources and return buffer to the pool.
**
**  This function will free any wbuf frames that are still attached to the
**  beacon buffers in the ATH object.  Note that this does not de-allocate
**  any wbuf objects that are in the transmit queue and have not yet returned
**  to the ATH object.
**
**  \param sc Pointer to ATH object, "this" pointer
**
**  \return N/A
*/

void
ath_beacon_free(struct ath_softc *sc)
{
    struct ath_buf *bf;

    TAILQ_FOREACH(bf, &sc->sc_bbuf, bf_list) {
        if (bf->bf_mpdu != NULL)
        {
            wbuf_t wbuf = (wbuf_t)bf->bf_mpdu;
            ieee80211_tx_status_t tx_status;

            wbuf_unmap_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
            tx_status.flags = 0;
            tx_status.retries = 0;
#ifdef  ATH_SUPPORT_TxBF
            tx_status.txbf_status = 0;
#endif
            sc->sc_ieee_ops->tx_complete(wbuf, &tx_status);
            bf->bf_mpdu = NULL;
        }
    }
}

#ifdef ATH_SUPPORT_DFS
static int
ath_handle_dfs_bb_hang(struct ath_softc *sc)
{
    if (sc->sc_dfs_hang.hang_war_activated) {
        sc->sc_bmisscount=0;
        printk("ALREADY ACTIVATED\n");
        return 0;
    }

    if (sc->sc_curchan.channelFlags & CHANNEL_HT20) {
        /*=========DFS HT20 HANG========*/
        /* moved marking PHY inactive to HAL reset */
        sc->sc_dfs_hang.hang_war_ht20count++;
        return 0;
    } else {
        /*=========DFS HT40 HANG========*/
        sc->sc_dfs_hang.hang_war_ht40count++;
        sc->sc_dfs_hang.hang_war_activated = 1;
        OS_CANCEL_TIMER(&sc->sc_dfs_hang.hang_war_timer);
                        sc->sc_dfs_hang.hang_war_timeout =
        get_dfs_hang_war_timeout(sc);
        OS_SET_TIMER(&sc->sc_dfs_hang.hang_war_timer,
                     sc->sc_dfs_hang.hang_war_timeout);
        /* Switch to static20 mode, clear 0x981C before
         * channel change
         */
        /* moved marking PHY inactive to HAL reset */
        sc->sc_ieee_ops->ath_net80211_switch_mode_static20(sc->sc_ieee);
        sc->sc_bmisscount=0;
        return 1;
    } /*End HT40 WAR*/
}
#endif

/*
 * Determines if the device currently has a BB or MAC hang.
 */
int
ath_hw_hang_check(struct ath_softc *sc)
{
    sc->sc_hang_check = AH_FALSE;

    /* Do we have a BB hang?
     */
    if (ATH_BB_HANG_WAR_REQ(sc)) {
        if (AH_TRUE == ath_hal_is_bb_hung(sc->sc_ah)) {
#ifdef ATH_SUPPORT_DFS
            /* Found a DFS related BB hang */
            if (ATH_DFS_BB_HANG_WAR_REQ(sc) && !ath_get_phy_err_rate(sc)) {
                if (ath_handle_dfs_bb_hang(sc))
                    return 0;
            }
#endif
            /* Found a generic BB hang */
            if (ATH_BB_GENERIC_HANG_WAR_REQ(sc)) {
                ATH_BB_GENERIC_HANG(sc);
                return 1;
            }
        }
    }

    /* Do we have a MAC hang?
     */
    if (ATH_MAC_HANG_WAR_REQ(sc)) {
        if (AH_TRUE == ath_hal_is_mac_hung(sc->sc_ah)) {
            ATH_MAC_GENERIC_HANG(sc);
            return 1;
        }
    }

    return 0; 
}

/******************************************************************************/
/*!
**  \brief Task for Sending Beacons
**
** Transmit one or more beacon frames at SWBA.  Dynamic updates to the frame
** contents are done as needed and the slot time is also adjusted based on
** current state.
**
** \warning This task is not scheduled, it's called in ISR context.
**
**  \param sc Pointer to ATH object ("this" pointer).
**  \param needmark Pointer to integer value indicating that the beacon miss
**                  threshold exceeded
**  \return Describe return value, or N/A for void
*/

void
ath_beacon_tasklet(struct ath_softc *sc, int *needmark)
{
    
    struct ath_hal *ah = sc->sc_ah;
    struct ath_buf *bf=NULL;
    int slot, if_id, pret;
    u_int32_t bfaddr = 0;
    u_int32_t rx_clear = 0, rx_frame = 0, tx_frame = 0;
    u_int32_t show_cycles = 0;
    u_int32_t bc = 0; /* beacon count */
    /* While scanning/in off channel do not send beacons */
    if (sc->sc_scanning) {
        return;
    }

#ifdef ATH_SUPPORT_DFS
    /* Make sure no beacons go out during the DFS wait period (CAC timer)*/
    if (sc->sc_dfs && sc->sc_dfs->sc_dfswait) {
       return;
    }
#endif

    /*
     * Check if the previous beacon has gone out.  If
     * not don't try to post another, skip this period
     * and wait for the next.  Missed beacons indicate
     * a problem and should not occur.  If we miss too
     * many consecutive beacons reset the device.
     */
    if (ath_hal_numtxpending(ah, sc->sc_bhalq) != 0)
    {
        /* Fix for EVID 108445 - Consecutive beacon stuck issue*/
        /* check NAV register, write 0 if it has invalid NAV */
        pret = ath_hal_update_navReg(ah);
        show_cycles = ath_hal_getMibCycleCountsPct(ah, 
                      &rx_clear, &rx_frame, &tx_frame);

        sc->sc_bmisscount++;
#ifdef ATH_SUPPORT_DFS         
        if((sc->sc_curchan.privFlags & CHANNEL_DFS) && (sc->sc_bmisscount == 1))
		sc->sc_3stream_sigflag1 = ath_hal_get3StreamSignature(ah);

	if((sc->sc_curchan.privFlags & CHANNEL_DFS) && (sc->sc_3stream_sigflag1) && (sc->sc_bmisscount == 2))
		sc->sc_3stream_sigflag2 = ath_hal_get3StreamSignature(ah);

	if((sc->sc_3stream_sigflag1) && (sc->sc_3stream_sigflag2)) {
		sc->sc_3stream_sigflag1 = 0;
		sc->sc_3stream_sigflag2 = 0;
		ath_hal_dmaRegDump(ah);
		ath_hal_dfs3streamfix(sc->sc_ah, 1);
		ath_hal_forcevcs(sc->sc_ah);
		sc->sc_3streamfix_engaged = 1;
                OS_SET_TIMER(&sc->sc_dfs->sc_dfs_war_timer, (10 * 60 * 1000));
		printk(KERN_DEBUG "%s engaing fix 0x9944 %x 0x9970 %x\n",__FUNCTION__,OS_REG_READ(sc->sc_ah,0x9944),
				OS_REG_READ(sc->sc_ah,0x9970));
	}
#endif
        /* XXX: doth needs the chanchange IE countdown decremented.
         *      We should consider adding a net80211 call to indicate
         *      a beacon miss so appropriate action could be taken
         *      (in that layer).
         */
        if (sc->sc_bmisscount < (BSTUCK_THRESH_PERVAP * sc->sc_nvaps)) {

            if (show_cycles && !tx_frame && (rx_clear >= 99)) {
                /* Meant to reduce PHY errors and potential false detects */
                if (sc->sc_toggle_immunity)
                    ath_hal_set_immunity(ah, AH_TRUE);

                sc->sc_noise++;
            }

            /* If the HW requires hang detection, notify caller
             * and detect in the tasklet context.
             */
            if (ATH_HANG_WAR_REQ(sc)) {
                sc->sc_hang_check = AH_TRUE;
                *needmark = 1;
                return;
            }

            if (sc->sc_noreset) {
                printk("%s: missed %u consecutive beacons\n",
                       __func__, sc->sc_bmisscount);
                if (show_cycles) {
                    /*
                     * Display cycle counter stats from HAL to
                     * aide in debug of stickiness.
                     */
                    printk("%s: busy times: rx_clear=%d, rx_frame=%d, tx_frame=%d\n", __func__, rx_clear, rx_frame, tx_frame);
                } else {
                    printk("%s: unable to obtain busy times\n", __func__);
                }
            } else {
                DPRINTF(sc, ATH_DEBUG_BEACON_PROC,
                        "%s: missed %u consecutive beacons\n",
                        __func__, sc->sc_bmisscount);
            }
        } else if (sc->sc_bmisscount >= (BSTUCK_THRESH_PERVAP * sc->sc_nvaps)) {
            if (sc->sc_noreset) {
                if (sc->sc_bmisscount == (BSTUCK_THRESH_PERVAP * sc->sc_nvaps)) {
                    printk("%s: beacon is officially stuck\n",
                           __func__);
                    ath_hal_dmaRegDump(ah);
                }
            } else {
                DPRINTF(sc, ATH_DEBUG_BEACON_PROC,
                        "%s: beacon is officially stuck\n",
                        __func__);

                if (show_cycles && rx_clear >= 99) {
                    sc->sc_hang_war |= HAL_BB_HANG_DETECTED;

                    if (rx_frame >= 99) {
                        printk("Busy environment detected\n");
                    } else {
                        printk("Interference detected\n");
                    }

                    printk("rx_clear=%d, rx_frame=%d, tx_frame=%d\n",
                           rx_clear, rx_frame, tx_frame);

                    sc->sc_noise = 0;
                }
                *needmark = 1;
            }
        }

        return;
    }

    if (sc->sc_toggle_immunity)
        ath_hal_set_immunity(ah, AH_FALSE);
    sc->sc_noise = 0;
    sc->sc_hang_check = AH_FALSE;

    /* resetting consecutive gtt count as beacon got 
    successfully transmitted. Eventhough this variable 
    is getting modified in two different contexts it is
    not protected as in worst case we loose reseting the 
    counter due to which we might have extra 
    check of mac tx hang which is not a problem */
    sc->sc_consecutive_gtt_count = 0;

    if (sc->sc_bmisscount != 0)
    {
        if (sc->sc_noreset) {
            printk("%s: resume beacon xmit after %u misses\n",
                   __func__, sc->sc_bmisscount);
        } else {
            DPRINTF(sc, ATH_DEBUG_BEACON_PROC,
                    "%s: resume beacon xmit after %u misses\n",
                    __func__, sc->sc_bmisscount);
        }
        sc->sc_bmisscount = 0;
    }

    /*
     * Generate beacon frames.  If we are sending frames
     * staggered then calculate the slot for this frame based
     * on the tsf to safeguard against missing an swba.
     * Otherwise we are bursting all frames together and need
     * to generate a frame for each vap that is up and running.
     */
    if (sc->sc_stagbeacons)
    {       /* staggered beacons */
        u_int64_t tsf;
        u_int32_t tsftu;
        u_int16_t intval;
        int nslots;

        nslots = sc->sc_nslots;
        if (sc->sc_ieee_ops->get_beacon_config)
        {
            ieee80211_beacon_config_t conf;
            sc->sc_ieee_ops->get_beacon_config(sc->sc_ieee, ATH_IF_ID_ANY, &conf);
            intval = conf.beacon_interval;
        }
        else
            intval = ATH_DEFAULT_BINTVAL;
        
        if (intval == 0 || nslots == 0) { 
            /*
             * This should not happen. We're seeing zero bintval sometimes 
             * in WDS testing but is not easily reproducible 
             */
            return;
        }

        tsf = ath_hal_gettsf64(ah);
        tsftu = TSF_TO_TU(tsf>>32, tsf);
        slot = ((tsftu % intval) * nslots) / intval;
        if_id = sc->sc_bslot[(slot + 1) % nslots];
        DPRINTF(sc, ATH_DEBUG_BEACON_PROC,
                "%s: slot %d [tsf %llu tsftu %u intval %u] if_id %d\n",
                __func__, slot, (unsigned long long)tsf, tsftu, intval, if_id);
        bfaddr = 0;
        if (if_id != ATH_IF_ID_ANY)
        {
            bf = ath_beacon_generate(sc, if_id, needmark);
#ifdef ATH_SUPPORT_DFS
            /* Make sure no beacons go out during the DFS wait period (CAC timer)*/
            if (sc->sc_dfs && sc->sc_dfs->sc_dfswait) {
                return;
            }
#endif

            if (bf != NULL) {
                bfaddr = bf->bf_daddr;
                bc = 1;
            }
        }
    }
    else
    {                        /* burst'd beacons */
#ifdef _WIN64
        u_int32_t __unaligned *bflink;
#else
        u_int32_t *bflink;
#endif
        struct ath_desc *prev_ds = NULL;
        struct ath_txq *cabq = sc->sc_cabq;
        struct ath_txq *burstbcn_cabq = &sc->sc_burstbcn_cabq;

            /* 
             * bursted beacons and edma: we shall move mcast traffic from 
             * all vaps to a temp q (burstbcn_cabq) and then move the tempq
             * to cabq. we could have just used the cabq instead of tempq
             * but that makes locking cabq unneccessarily complex.
             *
             * we don't need to lock burstbcn_cabq as it is accessed only in
             * tasklet context and linux makes sure that only one instance
             * of tasklet is running. what about other OSes?
             */

        if (sc->sc_enhanceddmasupport)
            TAILQ_INIT(&burstbcn_cabq->axq_q);

        bflink = &bfaddr;
        /* XXX rotate/randomize order? */
        for (slot = 0; slot < ATH_BCBUF; slot++)
        {
            if_id = sc->sc_bslot[slot];
            if (if_id != ATH_IF_ID_ANY)
            {
                bf = ath_beacon_generate(sc, if_id, needmark);
#ifdef ATH_SUPPORT_DFS
                /* Make sure no beacons go out during the DFS wait period (CAC timer)*/
                if (sc->sc_dfs && sc->sc_dfs->sc_dfswait) {
                    return;
                }
#endif
                if (bf != NULL)
                {
                    if (sc->sc_enhanceddmasupport) {
                        if (slot == 0)
                            bfaddr = bf->bf_daddr;
                        else 
                            ath_hal_setdesclink(ah, prev_ds, bf->bf_daddr);
                        prev_ds = bf->bf_desc;
                    } 
                    else {
#ifdef AH_NEED_DESC_SWAP
                        if(bflink != &bfaddr)
                            *bflink = cpu_to_le32(bf->bf_daddr);
                        else
                            *bflink = bf->bf_daddr;
#else
                        *bflink = bf->bf_daddr;
#endif
                        ath_hal_getdesclinkptr(ah, bf->bf_desc, &bflink);
                    }
                    bc ++;
                }
            }
        }
        /* this may not be needed as ath_becon_setup will zero it out */
        if (!sc->sc_enhanceddmasupport)
            *bflink = 0;    /* link of last frame */
        else {
            struct ath_buf *bfmcast;
            bfmcast = TAILQ_FIRST(&burstbcn_cabq->axq_q);
            if (bfmcast) {
                ATH_TXQ_LOCK(cabq);
                ATH_EDMA_TXQ_MOVE_MCASTQ(burstbcn_cabq, cabq);

                ASSERT(bfmcast->bf_daddr != 0);
                ath_hal_puttxbuf(ah, cabq->axq_qnum, bfmcast->bf_daddr);
                ath_hal_txstart(ah, cabq->axq_qnum);
                ATH_TXQ_UNLOCK(cabq);
            }
        }
    }

    /*
     * Handle slot time change when a non-ERP station joins/leaves
     * an 11g network.  The 802.11 layer notifies us via callback,
     * we mark updateslot, then wait one beacon before effecting
     * the change.  This gives associated stations at least one
     * beacon interval to note the state change.
     *
     * NB: The slot time change state machine is clocked according
     *     to whether we are bursting or staggering beacons.  We
     *     recognize the request to update and record the current
     *     slot then don't transition until that slot is reached
     *     again.  If we miss a beacon for that slot then we'll be
     *     slow to transition but we'll be sure at least one beacon
     *     interval has passed.  When bursting slot is always left
     *     set to ATH_BCBUF so this check is a noop.
     */
    /* XXX locking */
    if (sc->sc_updateslot == ATH_UPDATE)
    {
        sc->sc_updateslot = ATH_COMMIT; /* commit next beacon */
        sc->sc_slotupdate = slot;
    }
    else if (sc->sc_updateslot == ATH_COMMIT && sc->sc_slotupdate == slot)
        ath_setslottime(sc);        /* commit change to hardware */

    if ((!sc->sc_stagbeacons || slot == 0) && (!sc->sc_diversity))
    {
        int otherant;
        /*
         * Check recent per-antenna transmit statistics and flip
         * the default rx antenna if noticeably more frames went out
         * on the non-default antenna.  Only do this if rx diversity
         * is off.
         * XXX assumes 2 anntenae
         */
        otherant = sc->sc_defant & 1 ? 2 : 1;
        if (sc->sc_ant_tx[otherant] > sc->sc_ant_tx[sc->sc_defant] + ATH_ANTENNA_DIFF)
        {
            DPRINTF(sc, ATH_DEBUG_BEACON,
                    "%s: flip defant to %u, %u > %u\n",
                    __func__, otherant, sc->sc_ant_tx[otherant],
                    sc->sc_ant_tx[sc->sc_defant]);
            ath_setdefantenna(sc, otherant);
        }
        sc->sc_ant_tx[1] = sc->sc_ant_tx[2] = 0;
    }

    if (bfaddr != 0)
    {
        /*
         * WAR - the TXD to Q9 - TXDP fifo has a logic defect, so that after couple 
         * of times stopping/starting TXD[9], we'll see the beacon stuck.
         *
         * All the EDMA platform has similiar issue, per designer's suggestion to 
         * remove it temporarily.
         *
         * TBD: once HW fix this issue, we may revert this...
         */
        if (!sc->sc_enhanceddmasupport) {

           /*
            * Stop any current dma and put the new frame(s) on the queue.
            * This should never fail since we check above that no frames
            * are still pending on the queue.
            */
            if (!ath_hal_stoptxdma(ah, sc->sc_bhalq, 0))
            {
                DPRINTF(sc, ATH_DEBUG_ANY,
                        "%s: beacon queue %u did not stop?\n",
                        __func__, sc->sc_bhalq);
                /* NB: the HAL still stops DMA, so proceed */
            }
        }

#ifndef REMOVE_PKT_LOG

        {
            struct log_tx log_data;
            log_data.firstds = bf->bf_desc;
            log_data.bf = bf;
            ath_log_txctl(sc, &log_data, 0);
            log_data.lastds = bf->bf_desc;
            ath_log_txstatus(sc, &log_data, 0);
        }

#endif
        /* NB: cabq traffic should already be queued and primed */
        ath_hal_puttxbuf(ah, sc->sc_bhalq, bfaddr);
        ath_hal_txstart(ah, sc->sc_bhalq);

        sc->sc_stats.ast_be_xmit += bc;     /* XXX per-vap? */
#ifdef ATH_ADDITIONAL_STATS
        sc->sc_stats.ast_txq_packets[sc->sc_bhalq]++;
#endif
    }
}

/******************************************************************************/
/*!
**  \brief Task or Beacon Stuck processing
**
**  Processing for Beacon Stuck. 
**  Basically calls the ath_internal_reset function to reset the chip.
**
**  \param data Pointer to the ATH object (ath_softc).
**
**  \return N/A
*/

void
ath_bstuck_tasklet(struct ath_softc *sc)
{
#ifdef ATH_SUPPORT_DFS
    sc->sc_dfs_hang.total_stuck++;
#endif
    if (!ATH_HANG_DETECTED(sc)) {
        printk(KERN_DEBUG "%s: stuck beacon; resetting (bmiss count %u)\n",
               __func__, sc->sc_bmisscount);
    }
//	dump_stack();
    sc->sc_reset_type = ATH_RESET_NOLOSS;
    ath_internal_reset(sc);
 
    ath_radio_disable(sc);
    ath_radio_enable(sc);
    
    sc->sc_reset_type = ATH_RESET_DEFAULT;
    sc->sc_stats.ast_resetOnError++;

#if ATH_SUPPORT_SPECTRAL
   /* If CW interference is severe, then HW goes into a loop of continuous stuck beacons and resets. 
    * The check interference function checks if a single NF value exceeds bounds. The CHANNEL_CW_INT flag 
    * cannot be used becuase it is set only if the median of several NFs in a history buffer exceed bounds.
    * Continuous resets will not allow the history buffer to be populated.
    */
    if (ath_hal_interferenceIsPresent(sc->sc_ah)) {
       DPRINTF(sc, ATH_DEBUG_BEACON, "%s: Detected interference, sending eacs message\n", __func__);
       spectral_send_eacs_msg(sc);
       return;
    }
#endif

#ifdef ATH_SUPPORT_DFS
    if (sc->sc_3streamfix_engaged) {
	    ath_hal_dfs3streamfix(sc->sc_ah, 1);
	    ath_hal_forcevcs(sc->sc_ah);
	    printk(KERN_DEBUG "%s engaing fix again 0x9944 %x 0x9970 %x\n",__FUNCTION__,OS_REG_READ(sc->sc_ah,0x9944),
			    OS_REG_READ(sc->sc_ah,0x9970));
    }
#endif
}


/*
 * return information about all active vaps excluding the one passed.
 */
static int ath_get_all_active_vap_info(struct ath_softc *sc, ath_vap_info *av_info, int exclude_id)
{
    int i,j=0;
    struct ath_vap *avp;

    for (i=0; i < sc->sc_nvaps; i++) {
        avp = sc->sc_vaps[i];
        if (avp) {
            if(avp->av_up && i != exclude_id){
                av_info[j].if_id = i;
                ++j;
            }
        }
    }
    return j;
}

static void ath_get_beacon_config(struct ath_softc *sc, int if_id, ieee80211_beacon_config_t *conf)
{
    if (sc->sc_ieee_ops->get_beacon_config)
    {
        sc->sc_ieee_ops->get_beacon_config(sc->sc_ieee, if_id, conf);
    } else {
        /*
         * Protocol stack doesn't support dynamic beacon configuration,
         * use default configurations.
         */
        conf->beacon_interval = ATH_DEFAULT_BINTVAL;
        conf->listen_interval = 1;
        conf->dtim_period = conf->beacon_interval;
        conf->dtim_count = 1;
        conf->bmiss_timeout = ATH_DEFAULT_BMISS_LIMIT * conf->beacon_interval;
    }
}


#define MIN_TSF_TIMER_TIME  5000 /* 5 msec */
/*
 * @get the next tbtt in TUs.
 * @param curr_tsf64   : current tsf in usec.
 * @param bintval      : beacon interval in  TUs.
 *  @return next tbtt in TUs (lower 32 bits )
 */
static u_int32_t get_next_tbtt_tu_32(u_int64_t curr_tsf64, u_int32_t bintval)
{
   u_int64_t           nexttbtt_tsf64;

   bintval = bintval << 10; /* convert into usec */
    
   nexttbtt_tsf64 =  curr_tsf64 + bintval;

   nexttbtt_tsf64  = nexttbtt_tsf64 - OS_MOD64_TBTT_OFFSET(nexttbtt_tsf64, bintval);

   if ((nexttbtt_tsf64 - curr_tsf64) < MIN_TSF_TIMER_TIME ) {  /* if the immediate next tbtt is too close go to next one */
       nexttbtt_tsf64 += bintval;
   }

   return (u_int32_t) (nexttbtt_tsf64 >> 10);  /* convert to TUs */
}



static void ath_beacon_config_tsf_offset(struct ath_softc *sc, int if_id )
{
    struct ath_vap *avp;
    wbuf_t wbuf ;
    struct ieee80211_frame  *wh;

    KASSERT( (if_id != ATH_IF_ID_ANY), ("for vap up reason if_id can not be any" ));
    avp = sc->sc_vaps[if_id];

    if(avp->av_opmode == HAL_M_STA ) {
        return;
    }

    KASSERT( (avp->av_bcbuf != NULL), ("beacon buf can not be null" ));
    KASSERT( (avp->av_bcbuf->bf_mpdu != NULL), ("beacon wbuf can not be null" ));
   
    wbuf= (wbuf_t)avp->av_bcbuf->bf_mpdu;
    /*
     * Calculate a TSF adjustment factor required for
     * staggered beacons.  Note that we assume the format
     * of the beacon frame leaves the tstamp field immediately
     * following the header.
     */
    if (sc->sc_stagbeacons)
    {
        u_int64_t tsfadjust, tsfadjust_le;
        int intval;

        if (sc->sc_ieee_ops->get_beacon_config)
        {
            ieee80211_beacon_config_t conf;
            
            sc->sc_ieee_ops->get_beacon_config(sc->sc_ieee, if_id, &conf);
            intval = conf.beacon_interval;
        }
        else
            intval = ATH_DEFAULT_BINTVAL;
        
        /*
         * The beacon interval is in TU's; the TSF in usecs.
         * We figure out how many TU's to add to align the
         * timestamp then convert to TSF units and handle
         * byte swapping before writing it in the frame.
         * The hardware will then add this each time a beacon
         * frame is sent.  Note that we align vap's 1..N
         * and leave vap 0 untouched.  This means vap 0
         * has a timestamp in one beacon interval while the
         * others get a timestamp aligned to the next interval.
         */
        tsfadjust =

        tsfadjust_le = 0;
        if (avp->av_bslot) {
            tsfadjust = (intval * (sc->sc_nslots - avp->av_bslot)) / sc->sc_nslots;
            tsfadjust_le = cpu_to_le64(TU_TO_TSF(tsfadjust));      /* TU->TSF */
        }
        avp->av_tsfadjust = tsfadjust;
        DPRINTF(sc, ATH_DEBUG_BEACON,
                 "%s: %s beacons, bslot %d intval %u tsfadjust %llu tsfadjust_le %llu\n",
                 __func__, sc->sc_stagbeacons ? "stagger" : "burst",
                 avp->av_bslot, intval, (unsigned long long)tsfadjust, (unsigned long long)tsfadjust_le);

        wh = (struct ieee80211_frame *)wbuf_header(wbuf);
        OS_MEMCPY(&wh[1], &tsfadjust_le, sizeof(tsfadjust_le));

        ath_bslot_info_notify_change(avp, tsfadjust);
    }


}

static void ath_beacon_config_all_tsf_offset(struct ath_softc *sc)
{
    int id;
    for (id = 0; id < ATH_BCBUF; id++) {
        if (sc->sc_bslot[id] != ATH_IF_ID_ANY) {
            ath_beacon_config_tsf_offset(sc, sc->sc_bslot[id]);
        }
    }
}

      
/*
* allocate/free a beacon slot for AP vaps based on the reason and vap id.
* when a vap is up allocate beacon slot.
* when a vap is down free the beacon slot and to keep
* the allocated beacon slots contiguous move the last one to the free slot.
*/
static void ath_beacon_config_beacon_slot(struct ath_softc *sc, ath_beacon_slot_op config_op, int if_id )
{
    struct ath_vap *avp;
    bool config_tsf_offset = false;

    if (config_op == ATH_ALLOC_BEACON_SLOT) {     /* alloc slot */
        /*
         * Assign the vap to a beacon xmit slot.  As
         * above, this cannot fail to find one.
         */
        KASSERT( (if_id != ATH_IF_ID_ANY), ("for vap up reason if_id can not be any" ));
        avp = sc->sc_vaps[if_id];
        if (avp->av_bslot == -1) { /* coming up from down state */
            avp->av_bslot = sc->sc_nbcnvaps;
            sc->sc_bslot[avp->av_bslot] = if_id;
            sc->sc_nbcnvaps++;
            ath_set_num_slots(sc);
            config_tsf_offset = true;
        }
    }

    if (config_op == ATH_FREE_BEACON_SLOT) {     /* deallocate slot */
        KASSERT( (if_id != ATH_IF_ID_ANY), ("for vap down reason if_id can not be any" ));
        avp = sc->sc_vaps[if_id];
        if (avp->av_up == 1) { /* going down up from up state */
            KASSERT( (sc->sc_bslot[avp->av_bslot] == if_id), ("if_id does not match in the beacon slot" ));
            sc->sc_nbcnvaps--;
            ath_set_num_slots(sc);
            /*
             * move the last entry to 
             * to th entry corresponding to the vap that is down.
             */
            if (avp->av_bslot < sc->sc_nbcnvaps) {
                sc->sc_bslot[avp->av_bslot] = sc->sc_bslot[sc->sc_nbcnvaps];
                sc->sc_vaps[sc->sc_bslot[sc->sc_nbcnvaps]]->av_bslot = avp->av_bslot;
                sc->sc_bslot[sc->sc_nbcnvaps] = ATH_IF_ID_ANY;
                config_tsf_offset = true;
            } else {
                sc->sc_bslot[avp->av_bslot] = ATH_IF_ID_ANY;
            }
            avp->av_bslot = -1;
        }
    }

    if (config_op == ATH_ALLOC_FIRST_SLOT) {     /* alloc fisrt slot */
            bool insert_vap=false;
            if ( sc->sc_nbcnvaps > 0) {
            /* 
             * if it does not have the slot 0 then
             * push out the other vap from slot 0  and insert the vap on to slot 0.
             */      
                if ( sc->sc_bslot[0] != if_id) {
                    avp = sc->sc_vaps[sc->sc_bslot[0]];
                    sc->sc_bslot[sc->sc_nbcnvaps] = sc->sc_bslot[0];
                    avp->av_bslot = sc->sc_nbcnvaps;
                    insert_vap = true;
                    config_tsf_offset = true;
                    DPRINTF(sc, ATH_DEBUG_BEACON, "%s: move VAP %d from slot 0 \n", __func__, sc->sc_bslot[0]);
                }
            } else if (sc->sc_nbcnvaps == 0) {
                /* 
                 * if the STA vap is coming up and no other vaps are up 
                 */      
                insert_vap=true;
            }   
            if (insert_vap) {
                DPRINTF(sc, ATH_DEBUG_BEACON, "%s: insert STA Vap in slot 0 \n", __func__);
                /* insert STA vap at slot 0 */
                avp = sc->sc_vaps[if_id];
                avp->av_bslot = 0;
                sc->sc_bslot[0] = if_id;
                sc->sc_nbcnvaps++;
                ath_set_num_slots(sc);
            }
    }
    /* configure tsf offset for all beacon slots */
    if (config_tsf_offset) {
        ath_beacon_config_all_tsf_offset(sc);
    }
}

/*
 * find the smallest number that is power of 2 and is larger than the number of beacon vaps. 
 */
static void ath_set_num_slots(struct ath_softc *sc)
{
    int i, num_slots=0;
    KASSERT( (sc->sc_nbcnvaps <= ATH_BCBUF),  ("too many beacon vaps "));  
    if ( sc->sc_nbcnvaps == 0) {
        sc->sc_nslots = num_slots; 
        return;
    }
    for (i=0;i<5;++i) {
       num_slots = (1 << i);
       if ( sc->sc_nbcnvaps <= num_slots)
              break;
    }
    sc->sc_nslots = num_slots; 
}

static void ath_beacon_config_sta_mode(struct ath_softc *sc, ath_beacon_config_reason reason, int if_id )
{
    int num_slots,tbtt_timer_period;
    ieee80211_beacon_config_t conf;
    u_int32_t nexttbtt, intval;         /* units of TU   */
    int vap_index;
    struct ath_vap *avp;
    struct ath_vap *avpOther;
    u_int64_t tsf;
    u_int32_t tsftu;

    /*
     * assume only one sta vap.
     */
    switch (reason) {
    case ATH_BEACON_CONFIG_REASON_VAP_DOWN:     /* vap is down */
        /*
         *  vap is going down. adjust the slots.
         */
        avp = sc->sc_vaps[if_id]; /* a STA vap coming down may not have a beacon slot ,so check before freeing the slot */
        DPRINTF(sc, ATH_DEBUG_BEACON, "%s: reason %s if_id %d opmode %s \n",
            __func__, ath_beacon_config_reason_name(reason), if_id,
            ath_hal_opmode_name(sc->sc_opmode));
        /*
         * if a STA vap is going down and if another STA vap exists and is up,
         * then allocate the first slot to the existing STA vap.
         */ 
        if(avp->av_opmode == HAL_M_STA) {
            int i, j;
            /* if the sta had beacon slot allocated originally  then free it */
            if(avp->av_bslot != -1) {
                ath_beacon_config_beacon_slot(sc,ATH_FREE_BEACON_SLOT,if_id);
            }
            j = 0;
            for (i=0; ((i < ATH_BCBUF) && (j < sc->sc_nvaps)); i++) {
                avpOther = sc->sc_vaps[i];
                if (avpOther) {
                    j++;
                    if (i != if_id && avpOther->av_opmode == HAL_M_STA && avpOther->av_up && avpOther->av_bslot == -1) {
                        ath_beacon_config_beacon_slot(sc,ATH_ALLOC_FIRST_SLOT,i);
                        DPRINTF(sc, ATH_DEBUG_BEACON, "%s:allocating slot 0 to STA Vap with if_id %d\n",
                                __func__, i);
                    } 
                }
            }
        } else { /* Host AP mode */
           ath_beacon_config_beacon_slot(sc,ATH_FREE_BEACON_SLOT,if_id);
        }
        break;
    case ATH_BEACON_CONFIG_REASON_VAP_UP:     /* vap is up */
        if(sc->sc_vaps[if_id]->av_opmode == HAL_M_STA) {
            /* 
             * if a STA vap already exists on the slot 0 then 
             * do not allocate a slot for this seconds STA vap coming
             * up. (STA+STA ) case only one vap gets to have beacon slot
             * and the same vaps beacon information is programmed
             * into hardware. For the second STA vap the beacon offset
             * is maintained. 
             */
            if (sc->sc_nbcnvaps > 0 && sc->sc_bslot[0] != if_id &&
                sc->sc_vaps[if_id]->av_opmode == HAL_M_STA) {
                DPRINTF(sc, ATH_DEBUG_BEACON, "%s:(STA+STA case) primary STA vap with if_id %d exists \n",
                            __func__, sc->sc_bslot[0]);
                return;
            }
            ath_beacon_config_beacon_slot(sc,ATH_ALLOC_FIRST_SLOT,if_id);
        } else {
            ath_beacon_config_beacon_slot(sc,ATH_ALLOC_BEACON_SLOT,if_id);
        }
        break;
    case ATH_BEACON_CONFIG_REASON_OPMODE_SWITCH:
        {
            int j;
            /*
             * allocate beacon slot for the station vap.
             * protect vap list ?
             */ 
            j = 0;
            for(vap_index=0;((vap_index < ATH_BCBUF) && (j < sc->sc_nvaps)); vap_index++) {
                avp=sc->sc_vaps[vap_index];
                if (avp) {
                    j++;
                    if ((avp->av_opmode == HAL_M_STA) && avp->av_up) {
                        ath_beacon_config_beacon_slot(sc,ATH_ALLOC_BEACON_SLOT,vap_index);
                    }
                }
            }
        }
        break;
    case ATH_BEACON_CONFIG_REASON_RESET:
        break;
      
    }
    

    if (sc->sc_nbcnvaps == 0 ) {
        return;
    }
    /*
     * assert that the first beacon slot always points to a valid VAP.
     */
    KASSERT((sc->sc_bslot[0] != ATH_IF_ID_ANY),("the first slot does not have a valid vap id "));
    KASSERT((sc->sc_vaps[sc->sc_bslot[0]]),("the first slot does not point to a valid vap "));
#if 0
    KASSERT((sc->sc_vaps[sc->sc_bslot[0]]->av_opmode == HAL_M_STA),("the first slot does not point to a STA vap "));
#endif

    ath_get_beacon_config(sc,sc->sc_bslot[0],&conf); /* get the first VAPs beacon config */

    num_slots = sc->sc_nslots; 

    tbtt_timer_period = conf.beacon_interval & HAL_BEACON_PERIOD;
    if (sc->sc_stagbeacons)  { /* when  multiple vaps are present */
        tbtt_timer_period /= num_slots;    /* for staggered beacons */
    }
    nexttbtt = tbtt_timer_period;
    tsf = ath_hal_gettsf64(sc->sc_ah);
    /*
     * configure the sta beacon timers.
     */ 
    if (sc->sc_vaps[sc->sc_bslot[0]]->av_opmode == HAL_M_STA) {
        nexttbtt = ath_beacon_config_sta_timers(sc,&conf,tbtt_timer_period, tsf);
    }

    /*
     * configure the ap beacon timers.
     */ 
    if (num_slots > 1 || (sc->sc_vaps[sc->sc_bslot[0]]->av_opmode == HAL_M_HOSTAP)) {
        u_int32_t slot_duration ;
        u_int32_t sta_intval;      /* station beacon interval   */
        /* 
         * get the AP VAP beacon config
         * VAP at slot 1 is always AP VAP.
         */
        sta_intval = conf.beacon_interval & HAL_BEACON_PERIOD;
        ath_get_beacon_config(sc,sc->sc_bslot[1],&conf); 
        intval = conf.beacon_interval & HAL_BEACON_PERIOD;
        if (sc->sc_stagbeacons)  { /* when in WDS or multi AP vap mode */
            tsftu = TSF_TO_TU(tsf>>32, tsf);
            /*
             * the station  beacon tbtt is programmed . the nextbbtt value
             * is the nextbtt for the station vap. determine the immediate SWBA slot
             * for SWBA timer , the slot could be before the nextbtt.
             */
            /* meve the nextbtt back until it is below current tsf */ 
            while (nexttbtt > tsftu ) {
                nexttbtt -= sta_intval; 
            }
            /* nexttbtt now points the tbtt of the sta vap in the past */ 

            /*
             * increment the nextbtt by sta beacon interval until
             * the first slot is above current tsf.
             */
            slot_duration = (intval)/num_slots;
            nexttbtt += slot_duration;
            while ( nexttbtt < tsftu ) {
                nexttbtt += sta_intval; 
            }

             /*
              * special case if  num slots is 2 ( 1 STA VAP + 1 AP VAP) then
              * program SWBA timer to start at intval/2 from the tbtt of the STA
              * where intval is the beacon interval(usually 100TUs).
              */
            if (num_slots != 2) {
                intval /= num_slots;    /* for staggered beacons */
            }
        }
        ath_beacon_config_ap_timers(sc,nexttbtt,intval);
        DPRINTF(sc, ATH_DEBUG_BEACON, "%s: AP nexttbtt %d intval %d \n", __func__,nexttbtt,intval);
    }
}

static void ath_beacon_config_ap_mode(struct ath_softc *sc, ath_beacon_config_reason reason, int if_id )
{
    u_int32_t nexttbtt, intval;         /* units of TU   */
    int  num_slots;
    ieee80211_beacon_config_t conf;
    int slot=0;
    bool event_reset=0;

    switch (reason) {
    case ATH_BEACON_CONFIG_REASON_VAP_UP:
    case ATH_BEACON_CONFIG_REASON_VAP_DOWN:
        /*
         * When this is going to be the only active VAP
         * then do a TSF reset. All other cases do not
         * disturb the TSF
         */
        if(sc->sc_nbcnvaps != 0)  
            event_reset=1;
     
        if( sc->sc_vaps[if_id]->av_opmode == HAL_M_STA) {
            struct ath_vap *avp = sc->sc_vaps[if_id];
            /* 
             * STA vap is either going up (or) down 
             * we are in AP mode which means there wont be any network sleep support
             * we do not need to do any thing when a sta vap goes up/down.
             */  
            if( avp->av_bslot != -1) {
                /*
                 * if beacon slot was allocated to STA VAP. 
                 * this can happen if the STA VAP is brought down  with chip in STA mode and
                 * the chip is switched to AP mode while bringing down the VAP.
                 */ 
                ath_beacon_config_beacon_slot(sc,
                        reason==ATH_BEACON_CONFIG_REASON_VAP_UP?ATH_ALLOC_BEACON_SLOT:ATH_FREE_BEACON_SLOT,if_id);
 
            }
            return;
        }
        /* AP vap is coming up/down, allocate/deallocate beacon slots */
        ath_beacon_config_beacon_slot(sc,
             reason==ATH_BEACON_CONFIG_REASON_VAP_UP?ATH_ALLOC_BEACON_SLOT:ATH_FREE_BEACON_SLOT,if_id);
        break;
    case ATH_BEACON_CONFIG_REASON_RESET:
        event_reset=1;
        /* fall through */
    case ATH_BEACON_CONFIG_REASON_OPMODE_SWITCH:
        /*
         * deallocate beacon slot for station vap.
         */ 
        for(slot=0;slot < sc->sc_nbcnvaps; ++slot) {
            if( sc->sc_vaps[sc->sc_bslot[slot]]->av_opmode == HAL_M_STA) {
                ath_beacon_config_beacon_slot(sc,ATH_FREE_BEACON_SLOT,sc->sc_bslot[slot]);
            }
        }
        break;
    }

    if (sc->sc_nbcnvaps == 0 ) {
       return;
    }

    ath_get_beacon_config(sc,if_id,&conf); /* assume all the vaps have the same beacon config */
    num_slots = sc->sc_nslots; 
    intval = conf.beacon_interval & HAL_BEACON_PERIOD;
    if (sc->sc_stagbeacons)  { /* when in WDS or multi AP vap mode */
          intval /= num_slots;    /* for staggered beacons */
    }
    if (event_reset) {
        u_int64_t curr_tsf64;
        u_int32_t bcn_intval;


        bcn_intval = (conf.beacon_interval & HAL_BEACON_PERIOD);
 
        /* 
         * reset event, calculate the next tbtt based on current tsf and 
         * reprogram the SWBA based on the calculated tbtt, do not let the 
         * ath_beacon_config_ap_timers reset the tsf to 0.
         */ 
        curr_tsf64 = ath_hal_gettsf64(sc->sc_ah);

        nexttbtt =  get_next_tbtt_tu_32(curr_tsf64, bcn_intval);  /* convert to tus */

    } else {
       nexttbtt = intval;
    }

    /* configure the HAL for the IBSS mode */
    ath_beacon_config_ap_timers(sc,nexttbtt,intval);
}

#if ATH_SUPPORT_IBSS
static void ath_beacon_config_ibss_mode(struct ath_softc *sc, ath_beacon_config_reason reason, int if_id )
{
    ieee80211_beacon_config_t conf;
    u_int32_t nexttbtt, intval;         /* units of TU   */
    ath_vap_info av_info[ATH_BCBUF];
    u_int32_t num_active_vaps; 

    num_active_vaps =  ath_get_all_active_vap_info(sc,av_info,if_id);

    OS_MEMZERO(&conf, sizeof(ieee80211_beacon_config_t));

    switch (reason) {
    case ATH_BEACON_CONFIG_REASON_VAP_UP:
    case ATH_BEACON_CONFIG_REASON_VAP_DOWN:
        if (reason == ATH_BEACON_CONFIG_REASON_VAP_UP) {     /* vap is up */
            KASSERT( (num_active_vaps == 0), ("can not configure IBSS vap when other vaps are active" ));
        }
        ath_beacon_config_beacon_slot(sc,
             reason==ATH_BEACON_CONFIG_REASON_VAP_UP?ATH_ALLOC_BEACON_SLOT:ATH_FREE_BEACON_SLOT,if_id);
    default: 
        break;
    }

    ath_get_beacon_config(sc,if_id,&conf);
    
    /* extract tstamp from last beacon and convert to TU */
    nexttbtt = TSF_TO_TU(LE_READ_4(conf.u.last_tstamp + 4),
                         LE_READ_4(conf.u.last_tstamp));

    intval = conf.beacon_interval & HAL_BEACON_PERIOD;

    /* configure the HAL for the IBSS mode */
    ath_beacon_config_ibss_timers(sc,nexttbtt,intval);

}
#endif

/*
 * Configure the beacon and sleep timers.
 * supports combination of multiple vaps 
 * with multiple chip mode.
 *
 * When operating in station mode this sets up the beacon
 * timers according to the timestamp of the last received
 * beacon and the current TSF, configures PCF and DTIM
 * handling, programs the sleep registers so the hardware
 * will wakeup in time to receive beacons, and configures
 * the beacon miss handling so we'll receive a BMISS
 * interrupt when we stop seeing beacons from the AP
 * we've associated with.
 * 
 * also supports multiple combinations of VAPS.
 * a) multiple AP VAPS.
 * b) one AP VAP and one station VAP with the chip mode in AP (for WDS).
 * b) one AP VAP and one station VAP with the chip mode in STA(for P2P).
 */

void
ath_beacon_config(struct ath_softc *sc,ath_beacon_config_reason reason, int if_id)
{

    DPRINTF(sc, ATH_DEBUG_BEACON,
        "%s: reason %s if_id %d opmode %s  nslots %d\n",
        __func__, ath_beacon_config_reason_name(reason), if_id,
        ath_hal_opmode_name(sc->sc_opmode), sc->sc_nslots);

    ATH_BEACON_LOCK(sc);
    if (sc->sc_nbcnvaps ==0 && reason != ATH_BEACON_CONFIG_REASON_VAP_UP) { 
        /*
         * currently no active beacon vaps and none coming up.
         * ignore the call.
         */
         ATH_BEACON_UNLOCK(sc);
         DPRINTF(sc, ATH_DEBUG_BEACON, "%s: no beacon vaps ignore reason %d if_id %d \n", __func__, reason, if_id);
         return;
    }

    if (sc->sc_opmode  ==  HAL_M_HOSTAP) {
        ath_beacon_config_ap_mode(sc,reason,if_id);
    } else if (sc->sc_opmode  ==  HAL_M_STA) {
        ath_beacon_config_sta_mode(sc,reason,if_id);
    }
#if ATH_SUPPORT_IBSS
    else if (sc->sc_opmode  ==  HAL_M_IBSS) {
        ath_beacon_config_ibss_mode(sc,reason,if_id);
    }
#endif
    do {
        int i;
        DPRINTF(sc, ATH_DEBUG_BEACON, "%s: # beacon vaps %d nslots %d \n", __func__, sc->sc_nbcnvaps, sc->sc_nslots);
        for (i=0;i<sc->sc_nbcnvaps;++i) {
            DPRINTF(sc, ATH_DEBUG_BEACON, "slot %d vapid %d vap mode %s \n",
                i, sc->sc_bslot[i],
                ath_hal_opmode_name(sc->sc_vaps[sc->sc_bslot[i]]->av_opmode));
        }
    } while(0);
    ATH_BEACON_UNLOCK(sc);
}


static u_int32_t ath_beacon_config_sta_timers(struct ath_softc *sc, ieee80211_beacon_config_t *conf, int tbtt_timer_period, u_int64_t tsf)
{
    u_int32_t nexttbtt, intval;         /* units of TU   */
    HAL_BEACON_STATE bs;
    u_int32_t tsftu;
    int dtimperiod, dtimcount, sleepduration;
    int cfpperiod, cfpcount;
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t tsfdiff, tsfdiff_count, dtimdiff_left, cfpdiff_left;

    /* extract tstamp from last beacon and convert to TU */
    nexttbtt = TSF_TO_TU(LE_READ_4(conf->u.last_tstamp + 4),
                         LE_READ_4(conf->u.last_tstamp));
     
    intval = conf->beacon_interval & HAL_BEACON_PERIOD;
    /*
     * Setup dtim and cfp parameters according to
     * last beacon we received (which may be none).
     */
    dtimperiod = conf->dtim_period;
    if (dtimperiod <= 0)        /* NB: 0 if not known */
        dtimperiod = 1;
    dtimcount = conf->dtim_count;
    if (dtimcount >= dtimperiod)    /* NB: sanity check */
        dtimcount = 0;      /* XXX? */
    cfpperiod = 1;          /* NB: no PCF support yet */
    cfpcount = 0;

    sleepduration = conf->listen_interval * intval;
    if ((sleepduration <= 0)  || sc->sc_up_beacon_purge)
        sleepduration = intval;

#define FUDGE   2
    /*
     * Pull nexttbtt forward to reflect the current
     * TSF and calculate dtim+cfp state for the result.
     */
    tsftu = TSF_TO_TU(tsf>>32, tsf) + FUDGE;

    /* if nexttbtt is much less than tsftu, the original
     * loop will run for a long time, changed the algorithm
     * as below.
     */
    if (nexttbtt <= tsftu) {
        tsfdiff = tsftu - nexttbtt;
        tsfdiff_count = (tsfdiff + intval - 1)/intval;
        if (tsfdiff_count == 0)
            tsfdiff_count = 1;

        nexttbtt += tsfdiff_count * intval;

        dtimdiff_left = tsfdiff_count % dtimperiod;
        cfpdiff_left = tsfdiff_count % cfpperiod;

        while (dtimdiff_left) {
            if (--dtimcount < 0) {
                dtimcount = dtimperiod - 1;
                if (--cfpcount < 0)
                    cfpcount = cfpperiod - 1;
            }
            dtimdiff_left--;
        }
    }

#undef FUDGE
    OS_MEMZERO(&bs, sizeof(bs));
    bs.bs_intval = tbtt_timer_period;
    bs.bs_nexttbtt = nexttbtt;
    bs.bs_dtimperiod = dtimperiod*intval;
    bs.bs_nextdtim = bs.bs_nexttbtt + dtimcount*intval;
    bs.bs_cfpperiod = cfpperiod*bs.bs_dtimperiod;
    bs.bs_cfpnext = bs.bs_nextdtim + cfpcount*bs.bs_dtimperiod;
    bs.bs_cfpmaxduration = 0;
    /*
     * Calculate the number of consecutive beacons to miss
     * before taking a BMISS interrupt.  The configuration
     * is specified in TU so we only need calculate based
     * on the beacon interval.  Note that we clamp the
     * result to at most 15 beacons.
     */
    if (sleepduration > intval)
    {
        bs.bs_bmissthreshold = conf->listen_interval * ATH_DEFAULT_BMISS_LIMIT/2;
    }
    else
    {
        bs.bs_bmissthreshold = howmany(conf->bmiss_timeout, tbtt_timer_period);
        if (bs.bs_bmissthreshold > 15)
            bs.bs_bmissthreshold = 15;
        else if (bs.bs_bmissthreshold <= 0)
            bs.bs_bmissthreshold = 1;
    }

#ifdef ATH_BT_COEX
    if (sc->sc_btinfo.bt_on) {
        bs.bs_bmissthreshold = 50;
    }
#endif
    /*
     * Calculate sleep duration.  The configuration is
     * given in ms.  We insure a multiple of the beacon
     * period is used.  Also, if the sleep duration is
     * greater than the DTIM period then it makes senses
     * to make it a multiple of that.
     *
     * XXX fixed at 100ms
     */

    bs.bs_sleepduration =
        roundup(IEEE80211_MS_TO_TU(100), sleepduration);
    if (bs.bs_sleepduration > bs.bs_dtimperiod) {
       bs.bs_sleepduration =
        roundup(bs.bs_dtimperiod, sleepduration);
    }

    /* TSF out of range threshold fixed at 1 second */
    bs.bs_tsfoor_threshold = HAL_TSFOOR_THRESHOLD;
    DPRINTF(sc, ATH_DEBUG_BEACON, " beacon tsf stamp 0x%x:0x%x \n", 
                            LE_READ_4(conf->u.last_tstamp + 4), LE_READ_4(conf->u.last_tstamp));
    DPRINTF(sc, ATH_DEBUG_BEACON, 
            "%s: tsf %llu tsf:tu %u intval %u nexttbtt %u dtim %u nextdtim %u bmiss %u sleep %u cfp:period %u maxdur %u next %u timoffset %u\n"
            , __func__
            , (unsigned long long)tsf, tsftu
            , bs.bs_intval
            , bs.bs_nexttbtt
            , bs.bs_dtimperiod
            , bs.bs_nextdtim
            , bs.bs_bmissthreshold
            , bs.bs_sleepduration
            , bs.bs_cfpperiod
            , bs.bs_cfpmaxduration
            , bs.bs_cfpnext
            , bs.bs_timoffset
        );
        
    ATH_INTR_DISABLE(sc);

    ath_hal_beacontimers(ah, &bs);
    sc->sc_imask |= HAL_INT_BMISS;

    ATH_INTR_ENABLE(sc);


    return  nexttbtt;
}

static void ath_beacon_config_ap_timers(struct ath_softc *sc, u_int32_t nexttbtt, u_int32_t intval)
{
    u_int32_t nexttbtt_tu8, intval_tu8; /* units of TU/8 */
    struct ath_hal *ah = sc->sc_ah;

    ATH_INTR_DISABLE(sc);
        
    intval_tu8 = (intval & HAL_BEACON_PERIOD) << 3;
    if (nexttbtt == intval) {
        intval |= HAL_BEACON_RESET_TSF;
        intval_tu8 |= HAL_BEACON_RESET_TSF;
    } 


    /*
     * In AP mode we enable the beacon timers and
     * SWBA interrupts to prepare beacon frames.
     */
    intval |= HAL_BEACON_ENA;
    intval_tu8 |= HAL_BEACON_ENA;
    sc->sc_imask |= HAL_INT_SWBA;   /* beacon prepare */
    ath_beaconq_config(sc);

    /* ath_hal_beaconinit now requires units of TU/8 for nexttbtt */
    nexttbtt_tu8 = nexttbtt << 3;
    ath_hal_beaconinit(ah, nexttbtt_tu8, intval_tu8, HAL_M_HOSTAP);
    DPRINTF(sc, ATH_DEBUG_BEACON, 
            "%s: nexttbtt %u intval %u flags 0x%x \n",__func__,nexttbtt, intval & HAL_BEACON_PERIOD, intval & (~HAL_BEACON_PERIOD));
    sc->sc_bmisscount = 0;
    sc->sc_noise = 0;
        
    ATH_INTR_ENABLE(sc);
}

#if ATH_SUPPORT_IBSS
static void ath_beacon_config_ibss_timers(struct ath_softc *sc, u_int32_t nexttbtt, u_int32_t intval)
{

    u_int64_t tsf;
    u_int32_t tsftu;
    u_int32_t nexttbtt_tu8, intval_tu8; /* units of TU/8 */
    struct ath_hal *ah = sc->sc_ah;

    ATH_INTR_DISABLE(sc);

    if (nexttbtt == 0)      /* e.g. for ap mode */
        nexttbtt = intval;
    else if (intval)        /* NB: can be 0 for monitor mode */
        nexttbtt = roundup(nexttbtt, intval);

    intval_tu8 = (intval & HAL_BEACON_PERIOD) << 3;
    if (nexttbtt == intval) {
        intval |= HAL_BEACON_RESET_TSF;
        intval_tu8 |= HAL_BEACON_RESET_TSF;
    }
    /*
     * Pull nexttbtt forward to reflect the current
     * TSF .
     */
#define FUDGE   2
    if (!(intval & HAL_BEACON_RESET_TSF))
    {
        tsf = ath_hal_gettsf64(ah);
        tsftu = TSF_TO_TU((u_int32_t)(tsf>>32), (u_int32_t)tsf) + FUDGE;
        do
        {
            nexttbtt += intval;
        } while (nexttbtt < tsftu);
    }
#undef FUDGE
    DPRINTF(sc, ATH_DEBUG_BEACON, "%s:IBSS nexttbtt %u intval %u flags 0x%x \n",
                   __func__,nexttbtt, intval & HAL_BEACON_PERIOD, intval & (~HAL_BEACON_PERIOD));

    /*
     * In IBSS mode enable the beacon timers but only
     * enable SWBA interrupts if we need to manually
     * prepare beacon frames.  Otherwise we use a
     * self-linked tx descriptor and let the hardware
     * deal with things.
     */
    intval |= HAL_BEACON_ENA;
    intval_tu8 |= HAL_BEACON_ENA;
    if (!sc->sc_hasveol)
        sc->sc_imask |= HAL_INT_SWBA;
    ath_beaconq_config(sc);

    /* ath_hal_beaconinit now requires units of TU/8 for nexttbtt */
    nexttbtt_tu8 = nexttbtt << 3;
    ath_hal_beaconinit(ah, nexttbtt_tu8, intval_tu8,HAL_M_IBSS);
    sc->sc_bmisscount = 0;
    sc->sc_noise = 0;
    ATH_INTR_ENABLE(sc);
   /*
    * When using a self-linked beacon descriptor in
    * ibss mode load it once here.
    */
    if (sc->sc_hasveol)
        ath_beacon_start_adhoc(sc, 0);


}
#endif


/*
 * Function to collect beacon rssi data and resync beacon if necessary
 */
void
ath_beacon_sync(ath_dev_t dev, int if_id)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    HAL_OPMODE av_opmode;


    ATH_FUNC_ENTRY_VOID(sc);

    if ( if_id != ATH_IF_ID_ANY )
    {
        av_opmode = sc->sc_vaps[if_id]->av_opmode;
        if ((av_opmode == HAL_M_STA) &&
            (sc->sc_opmode == HAL_M_HOSTAP)) {
            /*
             * no need to program beacon timers for a STA vap
             * when the chip is operating in AP mode (WDS,Direct Connect).
             */
            DPRINTF(sc, ATH_DEBUG_BEACON,"%s\n", "chip op mode is AP, ignore beacon config request from sta vap");
            return;
        }
    }
    /*
     * Resync beacon timers using the tsf of the
     * beacon frame we just received.
     */
    ATH_PS_WAKEUP(sc);
    ath_beacon_config(sc,ATH_BEACON_CONFIG_REASON_RESET,ATH_IF_ID_ANY);
    ATH_PS_SLEEP(sc);
}

/******************************************************************************/
/*!
**  \brief Task for Beacon Miss Interrupt
**
**  This tasklet will notify the upper layer when a beacon miss interrupt
**  has occurred.  The upper layer will perform any required processing.  If
**  the upper layer does not supply a function for this purpose, no other action
**  is taken.
**
**  \param sc Pointer to ATH object, "this" pointer
**
**  \return N/A
*/

void
ath_bmiss_tasklet(struct ath_softc *sc)
{
    DPRINTF(sc, ATH_DEBUG_ANY, "%s\n", __func__);
    if (sc->sc_ieee_ops->notify_beacon_miss)
        sc->sc_ieee_ops->notify_beacon_miss(sc->sc_ieee);
}

void ath_beacon_attach(struct ath_softc *sc)
{
    ATH_BEACON_LOCK_INIT(sc);

    ath_notify_tx_beacon_attach(sc);
}

void ath_beacon_detach(struct ath_softc *sc)
{
    ath_notify_tx_beacon_detach(sc);

    ATH_BEACON_LOCK_DESTROY(sc);
}

#ifdef ATH_SUPPORT_DFS
/* Return PHY error rate per second */
static u_int32_t
ath_get_phy_err_rate(struct ath_softc *sc)
{
    u_int64_t cur_tstamp;
    u_int32_t tstamp_diff, phy_err_rate, total_phy_errors;

    cur_tstamp = ath_hal_gettsf64(sc->sc_ah);
    tstamp_diff = (cur_tstamp - sc->sc_dfs->ath_dfs_stats.last_reset_tstamp);

    total_phy_errors = sc->sc_dfs->ath_dfs_stats.total_phy_errors;

    if (total_phy_errors == 0) {
            return 0;
    }

    /* Clear PHY error stats*/
    dfs_clear_stats(sc);

    tstamp_diff /= 1000000;

    if (tstamp_diff) {
          phy_err_rate = (total_phy_errors / tstamp_diff);
    } else {
                /* not enough time difference*/
                phy_err_rate = 1;
    }
    return phy_err_rate;
}

int get_dfs_hang_war_timeout(struct ath_softc *sc)
{
    u_int64_t cur_tstamp = OS_GET_TIMESTAMP();     
    u_int64_t diff_tstamp = 0;     
    int timeout = 0;

    timeout = sc->sc_dfs_hang.hang_war_timeout;
    diff_tstamp = cur_tstamp - sc->sc_dfs_hang.last_dfs_hang_war_tstamp;

   /* Check if we should increase the WAR timeout, can only increase upto MAX value*/
    if (CONVERT_SYSTEM_TIME_TO_MS(diff_tstamp) <= ATH_DFS_HANG_FREQ_MS) {
       if ((timeout*2) < ATH_DFS_HANG_WAR_MAX_TIMEOUT_MS){
           /* double hang war timeout*/
           timeout = sc->sc_dfs_hang.hang_war_timeout * 2;
        } else {
           /* hang war timeout at MAX value*/
          timeout = ATH_DFS_HANG_WAR_MAX_TIMEOUT_MS;
        }
    } else {
    /* Check if we should decrease the WAR timeout, can only decrease upto MIN value*/
       if (CONVERT_SYSTEM_TIME_TO_MS(diff_tstamp) >= (ATH_DFS_HANG_FREQ_MS + 
                           (sc->sc_dfs_hang.hang_war_timeout ))) {
             /* decrease hang war timeout */
            timeout = sc->sc_dfs_hang.hang_war_timeout / 2;
            if (timeout < ATH_DFS_HANG_WAR_MIN_TIMEOUT_MS) {
                     timeout = ATH_DFS_HANG_WAR_MIN_TIMEOUT_MS;
            }
        } else { 
           /* Keep the WAR timeout the same */
           timeout = sc->sc_dfs_hang.hang_war_timeout;
        }
    }
    /* Store the current timestamp as the last time the WAR kicked in*/
    sc->sc_dfs_hang.last_dfs_hang_war_tstamp = cur_tstamp;
    return timeout;
}

#endif







