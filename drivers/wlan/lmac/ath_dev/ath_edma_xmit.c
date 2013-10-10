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

/*****************************************************************************/
/*! \file ath_edma_xmit.c
**  \brief ATH Enhanced DMA Transmit Processing
**
**  This file contains the functionality for management of descriptor queues
**  in the ATH object.
**
*/

#include "ath_internal.h"
#include "ath_edma.h"
#include "if_athrate.h"

#ifdef ATH_SUPPORT_TxBF
#include "ratectrl11n.h"
#endif

#ifndef REMOVE_PKT_LOG
#include "pktlog.h"
extern struct ath_pktlog_funcs *g_pktlog_funcs;
#endif

#if ATH_SUPPORT_EDMA

/******************************************************************************/
/*!
**  \brief Transmit Tasklet
**
**  Deferred processing of transmit interrupt.
**
**  \param dev Pointer to ATH_DEV object
**
*/

/*
 * Deferred processing of transmit interrupt.
 * Tx Interrupts need to be disabled before entering this.
 */
void
ath_tx_edma_tasklet(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int txok, nacked=0, qdepth = 0, nbad = 0;
    struct ath_txq *txq;
    struct ath_tx_status ts;
    HAL_STATUS status;
    ath_bufhead bf_head;
    struct ath_buf *bf;
    struct ath_node *an;
#ifndef REMOVE_PKT_LOG
    u_int32_t txs_desc[9];
#endif
#if ATH_SUPPORT_VOWEXT
    u_int8_t n_head_fail = 0;
    u_int8_t n_tail_fail = 0;
#endif
#if UMAC_SUPPORT_SMARTANTENNA
    struct ath_smartant_per pertable;
#endif    
#ifdef ATH_SUPPORT_TxBF
    struct atheros_node *oan;
#endif

    ath_vap_pause_txq_use_inc(sc);
    for (;;) {

        /* hold lock while accessing tx status ring */
        ATH_TXSTATUS_LOCK(sc);

#ifndef REMOVE_PKT_LOG
        ath_hal_getrawtxdesc(sc->sc_ah, txs_desc);
#endif

        /*
         * Process a completion event.
         */
        status = ath_hal_txprocdesc(sc->sc_ah, (void *)&ts);

        ATH_TXSTATUS_UNLOCK(sc);

        if (status == HAL_EINPROGRESS)
            break;

        if (status == HAL_EIO) {
            printk("ath_tx_edma_tasklet: error processing status\n");
            break;
        }

        /* Skip beacon completions */
        if (ts.queue_id == sc->sc_bhalq) {
            /* Do the Beacon completion callback (if enabled) */
            if (atomic_read(&sc->sc_has_tx_bcn_notify)) {
                /* Notify that a beacon has completed */
                ath_tx_bcn_notify(sc);
            }
            continue;
        }

        /* Make sure the event came from an active queue */
        ASSERT(ATH_TXQ_SETUP(sc, ts.queue_id));

        /* Get the txq for the completion event */
        txq = &sc->sc_txq[ts.queue_id];

        ATH_TXQ_LOCK(txq);

        txq->axq_intrcnt = 0; /* reset periodic desc intr count */
#if ATH_HW_TXQ_STUCK_WAR
        txq->tx_done_stuck_count = 0;
#endif
        bf = TAILQ_FIRST(&txq->axq_fifo[txq->axq_tailindex]);

        if (bf == NULL) {
            /*
             * Remove printk for UAPSD as bug in UAPSD when processing burst of
             * frames linked via LINK in TXBD.
             * Each frame has txstatus coresponding. However since we put only
             * 1 TXBD to the queue FIFO for all the frames, the code process the
             * first txstatus - the others txstatus will get here without a bf
             * as the first bf we pullout from
             * bf = TAILQ_FIRST(&txq->axq_fifo[txq->axq_tailindex])
             * is the parent of all the other frames.
             */
            if (txq != sc->sc_uapsdq) {
               printk("ath_tx_edma_tasklet: TXQ[%d] tailindex %d\n",
                        ts.queue_id, txq->axq_tailindex);
            }
            ATH_TXQ_UNLOCK(txq);
            ath_vap_pause_txq_use_dec(sc);
            return;
        }

        if (txq == sc->sc_cabq || txq == sc->sc_uapsdq) {
            ATH_EDMA_MCASTQ_MOVE_HEAD_UNTIL(txq, &bf_head, bf->bf_lastbf, bf_list);
        } else {
            ATH_EDMA_TXQ_MOVE_HEAD_UNTIL(txq, &bf_head, bf->bf_lastbf, bf_list);
        }

        if (bf->bf_isaggr) {
            txq->axq_aggr_depth--;
        }

#ifdef ATH_SWRETRY
        txok = (ts.ts_status == 0);

        /* 
         * Only enable the sw retry when the following conditions are met:
         * 1. transmission failure
         * 2. swretry is enabled at this time
         * 3. non-ampdu data frame (not under protection of BAW)
         * 4. not for frames from UAPSD queue
         * Otherwise, complete the TU
         */
        if (txok || (ath_check_swretry_req(sc, bf) == AH_FALSE) || bf->bf_isampdu || bf->bf_isaggr || !bf->bf_isdata || txq == sc->sc_uapsdq || bf->bf_ismcast) {
            /* Change the status of the frame and complete
             * this frame as normal frame
             */
            bf->bf_status &= ~ATH_BUFSTATUS_MARKEDSWRETRY;

        } else {
            /* This frame is going through SW retry mechanism
             */
            bf->bf_status |= ATH_BUFSTATUS_MARKEDSWRETRY;
            ath_hal_setdesclink(sc->sc_ah, bf->bf_desc, 0);
        }
#endif

        ATH_TXQ_UNLOCK(txq);

        an = bf->bf_node;
       
        if (an != NULL) {
            int noratectrl;
			
#ifdef ATH_SUPPORT_TxBF
            oan = ATH_NODE_ATHEROS(an);
            if (oan->txbf && (ts.ts_status == 0) && VALID_TXBF_RATE(ts.ts_ratecode, oan->usedNss)) {

                if (ts.ts_txbfstatus & ATH_TXBF_stream_missed) {
                    __11nstats(sc,bf_stream_miss);
                }
                if (ts.ts_txbfstatus & ATH_TxBF_BW_mismatched) {
                    __11nstats(sc,bf_bandwidth_miss);
                }
                if (ts.ts_txbfstatus & ATH_TXBF_Destination_missed ) {
                    __11nstats(sc,bf_destination_miss);
             }

            }
#endif
            noratectrl = an->an_flags & (ATH_NODE_CLEAN | ATH_NODE_PWRSAVE);

            OS_SYNC_SINGLE(sc->sc_osdev, bf->bf_daddr, sc->sc_txdesclen, BUS_DMA_FROMDEVICE, NULL);
            ath_hal_gettxratecode(sc->sc_ah, bf->bf_desc, (void *)&ts);

            ath_tx_update_stats(sc, bf, txq->axq_qnum, &ts);

            txok = (ts.ts_status == 0);

            /*
             * Hand the descriptor to the rate control algorithm
             * if the frame wasn't dropped for filtering or sent
             * w/o waiting for an ack.  In those cases the rssi
             * and retry counts will be meaningless.
             */
            if (!bf->bf_isampdu) {
                /*
                 * This frame is sent out as a single frame. Use hardware retry
                 * status for this frame.
                 */
                bf->bf_retries = ts.ts_longretry;
                if (ts.ts_status & HAL_TXERR_XRETRY) {
                    __11nstats(sc,tx_sf_hw_xretries);
                    bf->bf_isxretried = 1;
                }
                nbad = 0;
#if ATH_SUPPORT_VOWEXT
                if ( ATH_IS_VOWEXT_AGGRSIZE_ENABLED(sc) || ATH_IS_VOWEXT_RCA_ENABLED(sc)){
                    n_head_fail = n_tail_fail = 0;
                }
#endif
            } else {
                nbad = ath_tx_num_badfrms(sc, bf, &ts, txok);
#if ATH_SUPPORT_VOWEXT
                if ( ATH_IS_VOWEXT_AGGRSIZE_ENABLED(sc) || ATH_IS_VOWEXT_RCA_ENABLED(sc)) {
                    n_tail_fail = (nbad & 0xFF);
                    n_head_fail = ((nbad >> 8) & 0xFF);
                    nbad = ((nbad >> 16) & 0xFF);
                }
#endif
            }

            if ((ts.ts_status & HAL_TXERR_FILT) == 0 &&
                (bf->bf_flags & HAL_TXDESC_NOACK) == 0)
            {
                /*
                 * If frame was ack'd update the last rx time
                 * used to workaround phantom bmiss interrupts.
                 */
                if (ts.ts_status == 0)
                    nacked++;

                if (bf->bf_isdata && !noratectrl &&
                            likely(!bf->bf_useminrate)) {
#ifdef ATH_SUPPORT_VOWEXT
                    /* FIXME do not care Ospre related issues as on today, keep
                             this pending until we get to that
                    */
                    if(!wbuf_is_sa_train_packet(bf->bf_mpdu)) {
                        ath_rate_tx_complete_11n(sc,
                                                 an,
                                                 &ts,
                                                 bf->bf_rcs,
                                                 TID_TO_WME_AC(bf->bf_tidno),
                                                 bf->bf_nframes,
                                                 nbad, n_head_fail, n_tail_fail,
                                                 ath_tx_get_rts_retrylimit(sc, txq));
                    }
#else
                    if(!wbuf_is_sa_train_packet(bf->bf_mpdu)) {
                        ath_rate_tx_complete_11n(sc,
                                                 an,
                                                 &ts,
                                                 bf->bf_rcs,
                                                 TID_TO_WME_AC(bf->bf_tidno),
                                                 bf->bf_nframes,
                                                 nbad,
                                                 ath_tx_get_rts_retrylimit(sc, txq));
                    }
#endif

#if UMAC_SUPPORT_SMARTANTENNA                   
                    /*
                     *  Collecting PER values for a paricular antenna and ratecode
                     *  antenna value is stored in wbuf (skb->cb)
                     */
                    if(wbuf_is_sa_train_packet(bf->bf_mpdu)) {
                        /* call back to update permap for a particular node */
                        pertable.antenna = wbuf_sa_get_antenna(bf->bf_mpdu);
                        pertable.ratecode = ts.ts_ratecode;
                        pertable.nFrames = bf->bf_nframes;
                        pertable.nBad = nbad;
                        pertable.chain0_rssi = ts.ts_rssi_ctl0;
                        pertable.chain1_rssi = ts.ts_rssi_ctl1;
                        pertable.chain2_rssi = ts.ts_rssi_ctl2;
                        sc->sc_ieee_ops->update_smartant_pertable(an->an_node, (void *)&pertable);
                    }
#endif                 
                }
            }

#ifdef ATH_SWRETRY
            if ((sc->sc_debug & ATH_DEBUG_SWR) &&
                (ts.ts_status || bf->bf_isswretry) && (bf->bf_status & ATH_BUFSTATUS_MARKEDSWRETRY))
            {
                struct ieee80211_frame * wh;
                wh = (struct ieee80211_frame *)wbuf_header(bf->bf_mpdu);
                DPRINTF(sc, ATH_DEBUG_SWR,
                        "%s: SeqCtrl0x%02X%02X --> status %08X rate %02X, swretry %d retrycnt %d totaltries %d\n",
                        __func__, wh->i_seq[0], wh->i_seq[1], ts.ts_status, ts.ts_ratecode,
                        (bf->bf_isswretry)?1:0, bf->bf_swretries, bf->bf_totaltries +
                        ts.ts_longretry + ts.ts_shortretry);

                DPRINTF(sc, ATH_DEBUG_SWR, "%s, %s, type=0x%x, subtype=0x%x\n",
                        (ts.ts_status & HAL_TXERR_FILT) != 0?"IS FILT":"NOT FILT",
                        (ts.ts_status & HAL_TXERR_XRETRY) != 0?"IS XRETRY":"NOT XRETRY",
                        wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK,
                        wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
            }

            if (bf->bf_status & ATH_BUFSTATUS_MARKEDSWRETRY) {
#ifndef REMOVE_PKT_LOG
                /* do pktlog */
                {
                    struct log_tx log_data;
                    struct ath_buf *tbf;
    
                    TAILQ_FOREACH(tbf, &bf_head, bf_list) {
                        log_data.firstds = tbf->bf_desc;
                        log_data.bf = tbf;
                        ath_log_txctl(sc, &log_data, 0);
                    }
                }
            	{
                	struct log_tx log_data;
                	log_data.lastds = &txs_desc;
                	ath_log_txstatus(sc, &log_data, 0);
            	}
#endif
                /* Put the sw retry frame back to tid queue */
                ath_tx_mpdu_requeue(sc, txq, &bf_head, ts);

                /* We have completed the buf in resend in case of
                 * failure and hence not needed and will be fatal
                 * if we fall through this
                 */
               continue;
            }
#endif
            /*
             * Complete this transmit unit
             *
             * Node cannot be referenced past this point since it can be freed
             * here.
             */
            if (bf->bf_isampdu) {
                if (ts.ts_flags & HAL_TX_DESC_CFG_ERR)
                    __11nstats(sc, txaggr_desc_cfgerr);
                if (ts.ts_flags & HAL_TX_DATA_UNDERRUN) {
                    __11nstats(sc, txaggr_data_urun);
#ifdef ATH_C3_WAR
                    sc->sc_fifo_underrun = 1;
#endif
                }
                if (ts.ts_flags & HAL_TX_DELIM_UNDERRUN) {
                    __11nstats(sc, txaggr_delim_urun);
#ifdef ATH_C3_WAR
                    sc->sc_fifo_underrun = 1;
#endif
                }
                ath_tx_complete_aggr_rifs(sc, txq, bf, &bf_head, &ts, txok);
            } else {
#ifndef REMOVE_PKT_LOG
                /* do pktlog */
                {
                    struct log_tx log_data;
                    struct ath_buf *tbf;

                    TAILQ_FOREACH(tbf, &bf_head, bf_list) {
                        log_data.firstds = tbf->bf_desc;
                        log_data.bf = tbf;
                        ath_log_txctl(sc, &log_data, 0);
                    }
                }
#endif

#ifdef ATH_SUPPORT_UAPSD
                if (txq == sc->sc_uapsdq)
                {
#ifdef ATH_SUPPORT_TxBF
                    ath_tx_uapsd_complete(sc, an, bf, &bf_head, txok, ts.ts_txbfstatus, ts.ts_tstamp);
#else
                    ath_tx_uapsd_complete(sc, an, bf, &bf_head, txok);
#endif
                }
                else
#endif
                {
                    if (bf->bf_isbar)
                        ath_tx_complete_bar(sc, bf, &bf_head, txok);
                    else
#ifdef  ATH_SUPPORT_TxBF
                        ath_tx_complete_buf(sc, bf, &bf_head, txok, ts.ts_txbfstatus, ts.ts_tstamp);
#else
                        ath_tx_complete_buf(sc, bf, &bf_head, txok);
#endif
                }
            }

#ifndef REMOVE_PKT_LOG
            /* do pktlog */
            {
                struct log_tx log_data;
                log_data.lastds = &txs_desc;
                ath_log_txstatus(sc, &log_data, 0);
            }
#endif
        } else {
           /* PAPRD has NULL an */
            if (bf->bf_state.bfs_ispaprd) {
                ath_tx_paprd_complete(sc, bf, &bf_head);
                //printk("%s[%d]: ath_tx_paprd_complete called txok %d\n", __func__, __LINE__, txok);
                ath_vap_pause_txq_use_dec(sc);
                return;
            }
        }

        /*
         * schedule any pending packets if aggregation is enabled
         */

        ATH_TXQ_LOCK(txq);
        ath_txq_schedule(sc, txq);
        ATH_TXQ_UNLOCK(txq);

        qdepth += ath_txq_depth(sc, txq->axq_qnum);

#if defined(ATH_SWRETRY) && defined(ATH_SWRETRY_MODIFY_DSTMASK)
		ATH_TXQ_LOCK(txq);
        if (ath_txq_depth(sc, txq->axq_qnum) == 0)
            if (sc->sc_swRetryEnabled)
                txq->axq_destmask = AH_TRUE;
		ATH_TXQ_UNLOCK(txq);
#endif
    }

    ath_vap_pause_txq_use_dec(sc);
    if (sc->sc_ieee_ops->notify_txq_status)
        sc->sc_ieee_ops->notify_txq_status(sc->sc_ieee, qdepth);

    return;
}

int
ath_tx_edma_init(struct ath_softc *sc)
{
    int error = 0;

    if (sc->sc_enhanceddmasupport) {
        ATH_TXSTATUS_LOCK_INIT(sc);

        /* allocate tx status ring */
        error = ath_txstatus_setup(sc, &sc->sc_txsdma, "txs", ATH_TXS_RING_SIZE);
        if (error == 0) {
            ath_hal_setuptxstatusring(sc->sc_ah, sc->sc_txsdma.dd_desc,
                                  sc->sc_txsdma.dd_desc_paddr, ATH_TXS_RING_SIZE);
        }
    }

    return error;
}

void
ath_tx_edma_cleanup(struct ath_softc *sc)
{
    if (sc->sc_enhanceddmasupport) {
        /* cleanup tx status descriptors */
        if (sc->sc_txsdma.dd_desc_len != 0)
            ath_txstatus_cleanup(sc, &sc->sc_txsdma);

        ATH_TXSTATUS_LOCK_DESTROY(sc);
    }
}
#endif /* ATH_SUPPORT_EDMA */
