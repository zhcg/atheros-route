/*
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2005 Atheros Communications, Inc.
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

#include "opt_ah.h"

#ifdef AH_SUPPORT_AR5416

#include "ah.h"
#include "ah_desc.h"
#include "ah_xr.h"
#include "ah_internal.h"

#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"
//#include "ar5416/ar5416desc.h"
#include "ar5416/ar5416phy.h"


/*
 * Update Tx FIFO trigger level.
 *
 * Set bIncTrigLevel to TRUE to increase the trigger level.
 * Set bIncTrigLevel to FALSE to decrease the trigger level.
 *
 * Returns TRUE if the trigger level was updated
 */
HAL_BOOL
ar5416UpdateTxTrigLevel(struct ath_hal *ah, HAL_BOOL bIncTrigLevel)
{
        struct ath_hal_5416 *ahp = AH5416(ah);
        u_int32_t txcfg, curLevel, newLevel;
        HAL_INT omask;
        u_int8_t txTrigLevelMax = AH_PRIVATE(ah)->ah_config.ath_hal_txTrigLevelMax;

        if (AH_PRIVATE(ah)->ah_txTrigLevel >= txTrigLevelMax)
            return AH_FALSE;

        /*
         * Disable interrupts while futzing with the fifo level.
         */
        omask = ar5416SetInterrupts(ah, ahp->ah_maskReg &~ HAL_INT_GLOBAL, 0);

        txcfg = OS_REG_READ(ah, AR_TXCFG);
        curLevel = MS(txcfg, AR_FTRIG);
        newLevel = curLevel;
        if (bIncTrigLevel)  {            /* increase the trigger level */
	            if (curLevel < txTrigLevelMax)
                        newLevel++;
        } else if (curLevel > MIN_TX_FIFO_THRESHOLD)
                newLevel--;
        if (newLevel != curLevel)
                /* Update the trigger level */
                OS_REG_WRITE(ah, AR_TXCFG,
                        (txcfg &~ AR_FTRIG) | SM(newLevel, AR_FTRIG));

        /* re-enable chip interrupts */
        ar5416SetInterrupts(ah, omask, 0);

        AH_PRIVATE(ah)->ah_txTrigLevel = newLevel;

        return (newLevel != curLevel);
}

/*
 * Returns the value of Tx Trigger Level
 */
u_int16_t 
ar5416GetTxTrigLevel(struct ath_hal *ah)
{
    return (AH_PRIVATE(ah)->ah_txTrigLevel);
}

/*
 * Set the properties of the tx queue with the parameters
 * from qInfo.
 */
HAL_BOOL
ar5416SetTxQueueProps(struct ath_hal *ah, int q, const HAL_TXQ_INFO *qInfo)
{
        struct ath_hal_5416 *ahp = AH5416(ah);
        HAL_CAPABILITIES *pCap = &AH_PRIVATE(ah)->ah_caps;

        if (q >= pCap->halTotalQueues) {
                HDPRINTF(ah, HAL_DBG_QUEUE, "%s: invalid queue num %u\n", __func__, q);
                return AH_FALSE;
        }
        return ath_hal_setTxQProps(ah, &ahp->ah_txq[q], qInfo);
}

/*
 * Return the properties for the specified tx queue.
 */
HAL_BOOL
ar5416GetTxQueueProps(struct ath_hal *ah, int q, HAL_TXQ_INFO *qInfo)
{
        struct ath_hal_5416 *ahp = AH5416(ah);
        HAL_CAPABILITIES *pCap = &AH_PRIVATE(ah)->ah_caps;


        if (q >= pCap->halTotalQueues) {
                HDPRINTF(ah, HAL_DBG_QUEUE, "%s: invalid queue num %u\n", __func__, q);
                return AH_FALSE;
        }
        return ath_hal_getTxQProps(ah, qInfo, &ahp->ah_txq[q]);
}

/*
 * Allocate and initialize a tx DCU/QCU combination.
 */
int
ar5416SetupTxQueue(struct ath_hal *ah, HAL_TX_QUEUE type,
        const HAL_TXQ_INFO *qInfo)
{
        struct ath_hal_5416 *ahp = AH5416(ah);
        HAL_TX_QUEUE_INFO *qi;
        HAL_CAPABILITIES *pCap = &AH_PRIVATE(ah)->ah_caps;
        int q;

        /* XXX move queue assignment to driver */
        switch (type) {
        case HAL_TX_QUEUE_BEACON:
                q = pCap->halTotalQueues-1;     /* highest priority */
                break;
        case HAL_TX_QUEUE_CAB:
                q = pCap->halTotalQueues-2;     /* next highest priority */
                break;
        case HAL_TX_QUEUE_PSPOLL:
                q = 1;
                break;
#if ATH_SUPPORT_CFEND
        case HAL_TX_QUEUE_CFEND:
                q = pCap->halTotalQueues - 3;
                break;
        case HAL_TX_QUEUE_UAPSD:
                q = pCap->halTotalQueues-4;
                break;
#else
        case HAL_TX_QUEUE_UAPSD:
                q = pCap->halTotalQueues-3;
                break;
#endif
        case HAL_TX_QUEUE_DATA:
                for (q = 0; q < pCap->halTotalQueues; q++)
                        if (ahp->ah_txq[q].tqi_type == HAL_TX_QUEUE_INACTIVE)
                                break;
                if (q == pCap->halTotalQueues) {
                        HDPRINTF(ah, HAL_DBG_QUEUE, "%s: no available tx queue\n", __func__);
                        return -1;
                }
                break;
        default:
                HDPRINTF(ah, HAL_DBG_QUEUE, "%s: bad tx queue type %u\n", __func__, type);
                return -1;
        }

        HDPRINTF(ah, HAL_DBG_QUEUE, "%s: queue %u\n", __func__, q);

        qi = &ahp->ah_txq[q];
        if (qi->tqi_type != HAL_TX_QUEUE_INACTIVE) {
                HDPRINTF(ah, HAL_DBG_QUEUE, "%s: tx queue %u already active\n", __func__, q);
                return -1;
        }
        OS_MEMZERO(qi, sizeof(HAL_TX_QUEUE_INFO));
        qi->tqi_type = type;
        if (qInfo == AH_NULL) {
                /* by default enable OK+ERR+DESC+URN interrupts */
                qi->tqi_qflags =
                          TXQ_FLAG_TXOKINT_ENABLE
                        | TXQ_FLAG_TXERRINT_ENABLE
                        | TXQ_FLAG_TXDESCINT_ENABLE
                        | TXQ_FLAG_TXURNINT_ENABLE
                        ;
                qi->tqi_aifs = INIT_AIFS;
                qi->tqi_cwmin = HAL_TXQ_USEDEFAULT;     /* NB: do at reset */
                qi->tqi_cwmax = INIT_CWMAX;
                qi->tqi_shretry = INIT_SH_RETRY;
                qi->tqi_lgretry = INIT_LG_RETRY;
                qi->tqi_physCompBuf = 0;
        } else {
                qi->tqi_physCompBuf = qInfo->tqi_compBuf;
                (void) ar5416SetTxQueueProps(ah, q, qInfo);
        }
        /* NB: must be followed by ar5416ResetTxQueue */
        return q;
}

/*
 * Update the h/w interrupt registers to reflect a tx q's configuration.
 */
static void
setTxQInterrupts(struct ath_hal *ah, HAL_TX_QUEUE_INFO *qi)
{
        struct ath_hal_5416 *ahp = AH5416(ah);

        HDPRINTF(ah, HAL_DBG_INTERRUPT, "%s: tx ok 0x%x err 0x%x desc 0x%x eol 0x%x urn 0x%x\n"
                , __func__
                , ahp->ah_txOkInterruptMask
                , ahp->ah_txErrInterruptMask
                , ahp->ah_txDescInterruptMask
                , ahp->ah_txEolInterruptMask
                , ahp->ah_txUrnInterruptMask
        );

		ENABLE_REG_WRITE_BUFFER

        OS_REG_WRITE(ah, AR_IMR_S0,
                  SM(ahp->ah_txOkInterruptMask, AR_IMR_S0_QCU_TXOK)
                | SM(ahp->ah_txDescInterruptMask, AR_IMR_S0_QCU_TXDESC)
        );
        OS_REG_WRITE(ah, AR_IMR_S1,
                  SM(ahp->ah_txErrInterruptMask, AR_IMR_S1_QCU_TXERR)
                | SM(ahp->ah_txEolInterruptMask, AR_IMR_S1_QCU_TXEOL)
        );
        OS_REG_WRITE_FLUSH(ah);

		DISABLE_REG_WRITE_BUFFER

        OS_REG_RMW_FIELD(ah, AR_IMR_S2,
                AR_IMR_S2_QCU_TXURN, ahp->ah_txUrnInterruptMask);
}

/*
 * Free a tx DCU/QCU combination.
 */
HAL_BOOL
ar5416ReleaseTxQueue(struct ath_hal *ah, u_int q)
{
        struct ath_hal_5416 *ahp = AH5416(ah);
        HAL_CAPABILITIES *pCap = &AH_PRIVATE(ah)->ah_caps;
        HAL_TX_QUEUE_INFO *qi;

        if (q >= pCap->halTotalQueues) {
                HDPRINTF(ah, HAL_DBG_QUEUE, "%s: invalid queue num %u\n", __func__, q);
                return AH_FALSE;
        }
        qi = &ahp->ah_txq[q];
        if (qi->tqi_type == HAL_TX_QUEUE_INACTIVE) {
                HDPRINTF(ah, HAL_DBG_QUEUE, "%s: inactive queue %u\n", __func__, q);
                return AH_FALSE;
        }

        HDPRINTF(ah, HAL_DBG_QUEUE, "%s: release queue %u\n", __func__, q);

        qi->tqi_type = HAL_TX_QUEUE_INACTIVE;
        ahp->ah_txOkInterruptMask &= ~(1 << q);
        ahp->ah_txErrInterruptMask &= ~(1 << q);
        ahp->ah_txDescInterruptMask &= ~(1 << q);
        ahp->ah_txEolInterruptMask &= ~(1 << q);
        ahp->ah_txUrnInterruptMask &= ~(1 << q);
        setTxQInterrupts(ah, qi);

        return AH_TRUE;
}

/*
 * Set the retry, aifs, cwmin/max, readyTime regs for specified queue
 * Assumes:
 *  phwChannel has been set to point to the current channel
 */
HAL_BOOL
ar5416ResetTxQueue(struct ath_hal *ah, u_int q)
{
        struct ath_hal_5416     *ahp  = AH5416(ah);
        struct ath_hal_private  *ap   = AH_PRIVATE(ah);
        HAL_CAPABILITIES        *pCap = &AH_PRIVATE(ah)->ah_caps;
        HAL_CHANNEL_INTERNAL    *chan = AH_PRIVATE(ah)->ah_curchan;
        HAL_TX_QUEUE_INFO       *qi;
        u_int32_t               cwMin, chanCwMin, value;

        if (q >= pCap->halTotalQueues) {
                HDPRINTF(ah, HAL_DBG_QUEUE, "%s: invalid queue num %u\n", __func__, q);
                return AH_FALSE;
        }
        qi = &ahp->ah_txq[q];
        if (qi->tqi_type == HAL_TX_QUEUE_INACTIVE) {
                HDPRINTF(ah, HAL_DBG_QUEUE, "%s: inactive queue %u\n", __func__, q);
                return AH_TRUE;         /* XXX??? */
        }

        HDPRINTF(ah, HAL_DBG_QUEUE, "%s: reset queue %u\n", __func__, q);

        if (qi->tqi_cwmin == HAL_TXQ_USEDEFAULT) {
                /*
                 * Select cwmin according to channel type.
                 * NB: chan can be NULL during attach
                 */
                if (chan && IS_CHAN_B(chan))
                        chanCwMin = INIT_CWMIN_11B;
                else
                        chanCwMin = INIT_CWMIN;
                /* make sure that the CWmin is of the form (2^n - 1) */
                for (cwMin = 1; cwMin < chanCwMin; cwMin = (cwMin << 1) | 1)
                        ;
        } else
                cwMin = qi->tqi_cwmin;

		ENABLE_REG_WRITE_BUFFER

        /* set cwMin/Max and AIFS values */
    if(q > 3 || (!ah->ah_fccaifs)) {
        OS_REG_WRITE(ah, AR_DLCL_IFS(q),
                  SM(cwMin, AR_D_LCL_IFS_CWMIN)
                | SM(qi->tqi_cwmax, AR_D_LCL_IFS_CWMAX)
               | SM(qi->tqi_aifs, AR_D_LCL_IFS_AIFS)); 
	}
        /* Set retry limit values */
        OS_REG_WRITE(ah, AR_DRETRY_LIMIT(q),
                   SM(INIT_SSH_RETRY, AR_D_RETRY_LIMIT_STA_SH)
                 | SM(INIT_SLG_RETRY, AR_D_RETRY_LIMIT_STA_LG)
                 | SM(qi->tqi_shretry, AR_D_RETRY_LIMIT_FR_SH)
        );

        /* enable early termination on the QCU */
        OS_REG_WRITE(ah, AR_QMISC(q), AR_Q_MISC_DCU_EARLY_TERM_REQ);

        /* enable DCU to wait for next fragment from QCU  */
        OS_REG_WRITE(ah, AR_DMISC(q), AR_D_MISC_CW_BKOFF_EN | AR_D_MISC_FRAG_WAIT_EN | 0x2);
        OS_REG_WRITE_FLUSH(ah);

        /* multiqueue support */
        if (qi->tqi_cbrPeriod) {
                OS_REG_WRITE(ah, AR_QCBRCFG(q),
                          SM(qi->tqi_cbrPeriod,AR_Q_CBRCFG_INTERVAL)
                        | SM(qi->tqi_cbrOverflowLimit, AR_Q_CBRCFG_OVF_THRESH));
                OS_REG_WRITE(ah, AR_QMISC(q),
                        OS_REG_READ(ah, AR_QMISC(q)) |
                        AR_Q_MISC_FSP_CBR |
                        (qi->tqi_cbrOverflowLimit ?
                                AR_Q_MISC_CBR_EXP_CNTR_LIMIT_EN : 0));
        }
        if (qi->tqi_readyTime && (qi->tqi_type != HAL_TX_QUEUE_CAB)) {
                OS_REG_WRITE(ah, AR_QRDYTIMECFG(q),
                        SM(qi->tqi_readyTime, AR_Q_RDYTIMECFG_DURATION) |
                        AR_Q_RDYTIMECFG_EN);
        }

        OS_REG_WRITE(ah, AR_DCHNTIME(q),
                        SM(qi->tqi_burstTime, AR_D_CHNTIME_DUR) |
                        (qi->tqi_burstTime ? AR_D_CHNTIME_EN : 0));
        OS_REG_WRITE_FLUSH(ah);
        if (qi->tqi_burstTime && (qi->tqi_qflags & TXQ_FLAG_RDYTIME_EXP_POLICY_ENABLE)) {
                        OS_REG_WRITE(ah, AR_QMISC(q),
                             OS_REG_READ(ah, AR_QMISC(q)) |
                             AR_Q_MISC_RDYTIME_EXP_POLICY);

        }

        if (qi->tqi_qflags & TXQ_FLAG_BACKOFF_DISABLE) {
                OS_REG_WRITE(ah, AR_DMISC(q),
                        OS_REG_READ(ah, AR_DMISC(q)) |
                        AR_D_MISC_POST_FR_BKOFF_DIS);
        }
        OS_REG_WRITE_FLUSH(ah);

		DISABLE_REG_WRITE_BUFFER

        if (qi->tqi_qflags & TXQ_FLAG_FRAG_BURST_BACKOFF_ENABLE) {
                OS_REG_WRITE(ah, AR_DMISC(q),
                        OS_REG_READ(ah, AR_DMISC(q)) |
                        AR_D_MISC_FRAG_BKOFF_EN);
        }
        switch (qi->tqi_type) {
        case HAL_TX_QUEUE_BEACON:               /* beacon frames */
			    ENABLE_REG_WRITE_BUFFER
                OS_REG_WRITE(ah, AR_QMISC(q),
                        OS_REG_READ(ah, AR_QMISC(q))
                        | AR_Q_MISC_FSP_DBA_GATED
                        | AR_Q_MISC_BEACON_USE
                        | AR_Q_MISC_CBR_INCR_DIS1);

                OS_REG_WRITE(ah, AR_DMISC(q),
                        OS_REG_READ(ah, AR_DMISC(q))
                        | (AR_D_MISC_ARB_LOCKOUT_CNTRL_GLOBAL <<
                                AR_D_MISC_ARB_LOCKOUT_CNTRL_S)
                        | AR_D_MISC_BEACON_USE
                        | AR_D_MISC_POST_FR_BKOFF_DIS);
                OS_REG_WRITE_FLUSH(ah);
				DISABLE_REG_WRITE_BUFFER
                break;
        case HAL_TX_QUEUE_CAB:                  /* CAB  frames */
                /*
                 * No longer Enable AR_Q_MISC_RDYTIME_EXP_POLICY,
                 * bug #6079.  There is an issue with the CAB Queue
                 * not properly refreshing the Tx descriptor if
                 * the TXE clear setting is used.
                 */
			    ENABLE_REG_WRITE_BUFFER
                OS_REG_WRITE(ah, AR_QMISC(q),
                        OS_REG_READ(ah, AR_QMISC(q))
                        | AR_Q_MISC_FSP_DBA_GATED
                        | AR_Q_MISC_CBR_INCR_DIS1
                        | AR_Q_MISC_CBR_INCR_DIS0);

                value = TU_TO_USEC(qi->tqi_readyTime)
                        - ap->ah_config.ath_hal_additional_swba_backoff
                        - ap->ah_config.ath_hal_sw_beacon_response_time
                        + ap->ah_config.ath_hal_dma_beacon_response_time;
                OS_REG_WRITE(ah, AR_QRDYTIMECFG(q), value | AR_Q_RDYTIMECFG_EN);

                OS_REG_WRITE(ah, AR_DMISC(q),
                        OS_REG_READ(ah, AR_DMISC(q))
                        | (AR_D_MISC_ARB_LOCKOUT_CNTRL_GLOBAL <<
                                AR_D_MISC_ARB_LOCKOUT_CNTRL_S));
                OS_REG_WRITE_FLUSH(ah);
				DISABLE_REG_WRITE_BUFFER
                break;
        case HAL_TX_QUEUE_PSPOLL:
                /*
                 * We may configure psPoll QCU to be TIM-gated in the
                 * future; TIM_GATED bit is not enabled currently because
                 * of a hardware problem in Oahu that overshoots the TIM
                 * bitmap in beacon and may find matching associd bit in
                 * non-TIM elements and send PS-poll PS poll processing
                 * will be done in software
                 */
                OS_REG_WRITE(ah, AR_QMISC(q),
                        OS_REG_READ(ah, AR_QMISC(q)) | AR_Q_MISC_CBR_INCR_DIS1);
                break;
        case HAL_TX_QUEUE_UAPSD:
                OS_REG_WRITE(ah, AR_DMISC(q),
                        OS_REG_READ(ah, AR_DMISC(q))
                        | AR_D_MISC_POST_FR_BKOFF_DIS);
                break;
        default:                        /* NB: silence compiler */
                break;
        }

#ifndef AH_DISABLE_WME
        /*
         * Yes, this is a hack and not the right way to do it, but
         * it does get the lockout bits and backoff set for the
         * high-pri WME queues for testing.  We need to either extend
         * the meaning of queueInfo->mode, or create something like
         * queueInfo->dcumode.
         */
        if (qi->tqi_intFlags & HAL_TXQ_USE_LOCKOUT_BKOFF_DIS) {
                OS_REG_WRITE(ah, AR_DMISC(q),
                         OS_REG_READ(ah, AR_DMISC(q)) |
                         SM(AR_D_MISC_ARB_LOCKOUT_CNTRL_GLOBAL,
                            AR_D_MISC_ARB_LOCKOUT_CNTRL)|
                         AR_D_MISC_POST_FR_BKOFF_DIS);
        }
#endif

        /*
         * Always update the secondary interrupt mask registers - this
         * could be a new queue getting enabled in a running system or
         * hw getting re-initialized during a reset!
         *
         * Since we don't differentiate between tx interrupts corresponding
         * to individual queues - secondary tx mask regs are always unmasked;
         * tx interrupts are enabled/disabled for all queues collectively
         * using the primary mask reg
         */
        if (qi->tqi_qflags & TXQ_FLAG_TXOKINT_ENABLE)
                ahp->ah_txOkInterruptMask |= 1 << q;
        else
                ahp->ah_txOkInterruptMask &= ~(1 << q);
        if (qi->tqi_qflags & TXQ_FLAG_TXERRINT_ENABLE)
                ahp->ah_txErrInterruptMask |= 1 << q;
        else
                ahp->ah_txErrInterruptMask &= ~(1 << q);
        if (qi->tqi_qflags & TXQ_FLAG_TXDESCINT_ENABLE)
                ahp->ah_txDescInterruptMask |= 1 << q;
        else
                ahp->ah_txDescInterruptMask &= ~(1 << q);
        if (qi->tqi_qflags & TXQ_FLAG_TXEOLINT_ENABLE)
                ahp->ah_txEolInterruptMask |= 1 << q;
        else
                ahp->ah_txEolInterruptMask &= ~(1 << q);
        if (qi->tqi_qflags & TXQ_FLAG_TXURNINT_ENABLE)
                ahp->ah_txUrnInterruptMask |= 1 << q;
        else
                ahp->ah_txUrnInterruptMask &= ~(1 << q);
        setTxQInterrupts(ah, qi);

        return AH_TRUE;
}

/*
 * Get the TXDP for the specified queue
 */
u_int32_t
ar5416GetTxDP(struct ath_hal *ah, u_int q)
{
        HALASSERT(q < AH_PRIVATE(ah)->ah_caps.halTotalQueues);
        return OS_REG_READ(ah, AR_QTXDP(q));
}

/*
 * Set the TxDP for the specified queue
 */
HAL_BOOL
ar5416SetTxDP(struct ath_hal *ah, u_int q, u_int32_t txdp)
{
        HALASSERT(q < AH_PRIVATE(ah)->ah_caps.halTotalQueues);
        HALASSERT(AH5416(ah)->ah_txq[q].tqi_type != HAL_TX_QUEUE_INACTIVE);

        /*
         * Make sure that TXE is deasserted before setting the TXDP.  If TXE
         * is still asserted, setting TXDP will have no effect.
         */
        HALASSERT((OS_REG_READ(ah, AR_Q_TXE) & (1 << q)) == 0);

        OS_REG_WRITE(ah, AR_QTXDP(q), txdp);

        return AH_TRUE;
}

/*
 * Set Transmit Enable bits for the specified queue
 */
HAL_BOOL
ar5416StartTxDma(struct ath_hal *ah, u_int q)
{
        HALASSERT(q < AH_PRIVATE(ah)->ah_caps.halTotalQueues);

        HALASSERT(AH5416(ah)->ah_txq[q].tqi_type != HAL_TX_QUEUE_INACTIVE);

        HDPRINTF(ah, HAL_DBG_QUEUE, "%s: queue %u\n", __func__, q);

        /* Check to be sure we're not enabling a q that has its TXD bit set. */
        HALASSERT((OS_REG_READ(ah, AR_Q_TXD) & (1 << q)) == 0);

        OS_REG_WRITE(ah, AR_Q_TXE, 1 << q);

        return AH_TRUE;
}

/*
 * Return the number of pending frames or 0 if the specified
 * queue is stopped.
 */
u_int32_t
ar5416NumTxPending(struct ath_hal *ah, u_int q)
{
        u_int32_t npend;

        HALASSERT(q < AH_PRIVATE(ah)->ah_caps.halTotalQueues);

        npend = OS_REG_READ(ah, AR_QSTS(q)) & AR_Q_STS_PEND_FR_CNT;
        if (npend == 0) {
                /*
                 * Pending frame count (PFC) can momentarily go to zero
                 * while TXE remains asserted.  In other words a PFC of
                 * zero is not sufficient to say that the queue has stopped.
                 */
                if (OS_REG_READ(ah, AR_Q_TXE) & (1 << q))
                        npend = 1;              /* arbitrarily return 1 */
        }
#ifdef DEBUG
        if (npend && (AH5416(ah)->ah_txq[q].tqi_type == HAL_TX_QUEUE_CAB)) {
                if (OS_REG_READ(ah, AR_Q_RDYTIMESHDN) & (1 << q)) {
                        HDPRINTF(ah, HAL_DBG_QUEUE, "RTSD on CAB queue\n");
                        /* Clear the ReadyTime shutdown status bits */
                        OS_REG_WRITE(ah, AR_Q_RDYTIMESHDN, 1 << q);
                }
        }
#endif
        HALASSERT((npend == 0) || (AH5416(ah)->ah_txq[q].tqi_type != HAL_TX_QUEUE_INACTIVE));
        
        return npend;
}

/*
 * Stop transmit on the specified queue
 */
HAL_BOOL
ar5416StopTxDma(struct ath_hal *ah, u_int q, u_int timeout)
{
#define AH_TX_STOP_DMA_TIMEOUT 4000    /* usec */  
#define AH_TIME_QUANTUM        100     /* usec */
        u_int wait;

        HALASSERT(q < AH_PRIVATE(ah)->ah_caps.halTotalQueues);

        HALASSERT(AH5416(ah)->ah_txq[q].tqi_type != HAL_TX_QUEUE_INACTIVE);

        if (timeout == 0) {
            timeout = AH_TX_STOP_DMA_TIMEOUT;
        }

        OS_REG_WRITE(ah, AR_Q_TXD, 1 << q);

        for (wait = timeout / AH_TIME_QUANTUM; wait != 0; wait--) {
                if (ar5416NumTxPending(ah, q) == 0)
                        break;
                OS_DELAY(AH_TIME_QUANTUM);        /* XXX get actual value */
        }

#ifdef AH_DEBUG
        if (wait == 0) {
                HDPRINTF(ah, HAL_DBG_QUEUE, "%s: queue %u DMA did not stop in 100 msec\n",
                        __func__, q);
                HDPRINTF(ah, HAL_DBG_QUEUE, "%s: QSTS 0x%x Q_TXE 0x%x Q_TXD 0x%x Q_CBR 0x%x\n"
                        , __func__
                        , OS_REG_READ(ah, AR_QSTS(q))
                        , OS_REG_READ(ah, AR_Q_TXE)
                        , OS_REG_READ(ah, AR_Q_TXD)
                        , OS_REG_READ(ah, AR_QCBRCFG(q))
                );
                HDPRINTF(ah, HAL_DBG_QUEUE, "%s: Q_MISC 0x%x Q_RDYTIMECFG 0x%x Q_RDYTIMESHDN 0x%x\n"
                        , __func__
                        , OS_REG_READ(ah, AR_QMISC(q))
                        , OS_REG_READ(ah, AR_QRDYTIMECFG(q))
                        , OS_REG_READ(ah, AR_Q_RDYTIMESHDN)
                );
        }
#endif /* AH_DEBUG */

	    /* 2413+ and up can kill packets at the PCU level */
        if (ar5416NumTxPending(ah, q)) {
            u_int32_t tsfLow, j;

            HDPRINTF(ah, HAL_DBG_QUEUE, "%s: Num of pending TX Frames %d on Q %d\n",
                 __func__, ar5416NumTxPending(ah, q), q);

            /* Kill last PCU Tx Frame */
            /* TODO - save off and restore current values of Q1/Q2? */
            for (j = 0; j < 2; j++) {
    	        tsfLow = OS_REG_READ(ah, AR_TSF_L32);
                OS_REG_WRITE(ah, AR_QUIET2, SM(10, AR_QUIET2_QUIET_DUR));
                OS_REG_WRITE(ah, AR_QUIET_PERIOD, 100);
                OS_REG_WRITE(ah, AR_NEXT_QUIET_TIMER, tsfLow >> 10);
                OS_REG_SET_BIT(ah, AR_TIMER_MODE, AR_QUIET_TIMER_EN);

                if ((OS_REG_READ(ah, AR_TSF_L32) >> 10) == (tsfLow >> 10)) {
                    break;
                }
                HDPRINTF(ah, HAL_DBG_QUEUE, "%s: TSF have moved while trying to set quiet time TSF: 0x%08x\n", __func__, tsfLow);
                HALASSERT(j < 1); /* TSF shouldn't count twice or reg access is taking forever */
            }
	        
            OS_REG_SET_BIT(ah, AR_DIAG_SW, AR_DIAG_FORCE_CH_IDLE_HIGH);
	        
            /* Allow the quiet mechanism to do its work */
            OS_DELAY(200);
            OS_REG_CLR_BIT(ah, AR_TIMER_MODE, AR_QUIET_TIMER_EN);
	        
            /* Verify all transmit is dead */
            wait = timeout / AH_TIME_QUANTUM;
            while (ar5416NumTxPending(ah, q)) {
                if ((--wait) == 0) {
                    HDPRINTF(ah, HAL_DBG_TX, "%s: Failed to stop Tx DMA in %d msec after killing last frame\n",
                         __func__, timeout / 1000);
                    break;
                }
                OS_DELAY(AH_TIME_QUANTUM);
            }

            OS_REG_CLR_BIT(ah, AR_DIAG_SW, AR_DIAG_FORCE_CH_IDLE_HIGH);
        }

        if ((wait == 0) && ar5416NumTxPending(ah, q) > 0) {
            AH5416(ah)->ah_dma_stuck = AH_TRUE;
        }

        OS_REG_WRITE(ah, AR_Q_TXD, 0);
        return (wait != 0);

#undef AH_TX_STOP_DMA_TIMEOUT
#undef AH_TIME_QUANTUM
}

/*
 * Abort transmit on all queues
 */
#define AR5416_ABORT_LOOPS     1000
#define AR5416_ABORT_WAIT      5
HAL_BOOL
ar5416AbortTxDma(struct ath_hal *ah)
{
    int i, q;

    /*
     * set txd on all queues
     */
    OS_REG_WRITE(ah, AR_Q_TXD, AR_Q_TXD_M);

    /*
     * set tx abort bits
     */
    OS_REG_SET_BIT(ah, AR_PCU_MISC, (AR_PCU_FORCE_QUIET_COLL | AR_PCU_CLEAR_VMF));
    OS_REG_SET_BIT(ah, AR_DIAG_SW, AR_DIAG_FORCE_CH_IDLE_HIGH);
    OS_REG_SET_BIT(ah, AR_D_GBL_IFS_MISC, AR_D_GBL_IFS_MISC_IGNORE_BACKOFF);

    /*
     * wait on all tx queues
     */
    for (q = 0; q < AR_NUM_QCU; q++) {
        for (i = 0; i < AR5416_ABORT_LOOPS; i++) {
            if (!ar5416NumTxPending(ah, q))
                break;

            OS_DELAY(AR5416_ABORT_WAIT);
        }
        if (i == AR5416_ABORT_LOOPS) {
            return AH_FALSE;
        }
    }

    /*
     * clear tx abort bits
     */
    OS_REG_CLR_BIT(ah, AR_PCU_MISC, (AR_PCU_FORCE_QUIET_COLL | AR_PCU_CLEAR_VMF));
    OS_REG_CLR_BIT(ah, AR_DIAG_SW, AR_DIAG_FORCE_CH_IDLE_HIGH);
    OS_REG_CLR_BIT(ah, AR_D_GBL_IFS_MISC, AR_D_GBL_IFS_MISC_IGNORE_BACKOFF);

    /*
     * clear txd
     */
    OS_REG_WRITE(ah, AR_Q_TXD, 0);

    return AH_TRUE;
}

/*
 * Determine which tx queues need interrupt servicing.
 */
void
ar5416GetTxIntrQueue(struct ath_hal *ah, u_int32_t *txqs)
{
        struct ath_hal_5416 *ahp = AH5416(ah);
        *txqs &= ahp->ah_intrTxqs;
        ahp->ah_intrTxqs &= ~(*txqs);
}

HAL_BOOL
ar5416SetGlobalTxTimeout(struct ath_hal *ah, u_int tu)
{
        struct ath_hal_5416 *ahp = AH5416(ah);

	if (tu > 0xFFFF) {
                HDPRINTF(ah, HAL_DBG_TX, "%s: bad global tx timeout %u\n", __func__, tu);
                ahp->ah_globaltxtimeout = (u_int) -1; /* restore default handling */
                return AH_FALSE;
        } else {
                OS_REG_RMW_FIELD(ah, AR_GTXTO,
                        AR_GTXTO_TIMEOUT_LIMIT, tu);
                ahp->ah_globaltxtimeout = tu;
                return AH_TRUE;
        }
}

u_int
ar5416GetGlobalTxTimeout(struct ath_hal *ah)
{
        return MS(OS_REG_READ(ah, AR_GTXTO), AR_GTXTO_TIMEOUT_LIMIT);
}

#endif /* AH_SUPPORT_AR5416 */
