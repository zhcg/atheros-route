/*
 * Copyright (c) 2008-2010, Atheros Communications Inc.
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

#ifdef AH_SUPPORT_AR9300

#include "ah.h"
#include "ah_desc.h"
#include "ah_internal.h"

#include "ar9300/ar9300.h"
#include "ar9300/ar9300reg.h"
#include "ar9300/ar9300desc.h"

/*
 * Get the RXDP.
 */
u_int32_t
ar9300GetRxDP(struct ath_hal *ath, HAL_RX_QUEUE qtype)
{
    if (qtype == HAL_RX_QUEUE_HP) {
        return OS_REG_READ(ath, AR_HP_RXDP);
    } else {
        return OS_REG_READ(ath, AR_LP_RXDP);
    }
}

/*
 * Set the RxDP.
 */
void
ar9300SetRxDP(struct ath_hal *ah, u_int32_t rxdp, HAL_RX_QUEUE qtype)
{
    HALASSERT((qtype == HAL_RX_QUEUE_HP) || (qtype == HAL_RX_QUEUE_LP));

    if (qtype == HAL_RX_QUEUE_HP) {
        OS_REG_WRITE(ah, AR_HP_RXDP, rxdp);
    } else {
        OS_REG_WRITE(ah, AR_LP_RXDP, rxdp);
    }
}

/*
 * Set Receive Enable bits.
 */
void
ar9300EnableReceive(struct ath_hal *ah)
{
    OS_REG_WRITE(ah, AR_CR, 0);
}

/*
 * Set the RX abort bit.
 */
HAL_BOOL
ar9300SetRxAbort(struct ath_hal *ah, HAL_BOOL set)
{ 
    if (set) {
        /* Set the ForceRXAbort bit */
        OS_REG_SET_BIT(ah, AR_DIAG_SW, (AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT));

        if ( AH_PRIVATE(ah)->ah_reset_reason == HAL_RESET_BBPANIC ){
            /* depending upon the BB panic status, rx state may not return to 0,
             * so skipping the wait for BB panic reset */
            OS_REG_CLR_BIT(ah, AR_DIAG_SW, (AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT));
            return AH_FALSE;    
        } else {
            HAL_BOOL okay;
            okay = ath_hal_wait(
                ah, AR_OBS_BUS_1, AR_OBS_BUS_1_RX_STATE, 0, AH_WAIT_TIMEOUT);
            /* Wait for Rx state to return to 0 */
            if (!okay) {
                u_int32_t    reg;

                /* abort: chip rx failed to go idle in 10 ms */
                OS_REG_CLR_BIT(ah, AR_DIAG_SW,
                    (AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT));

                reg = OS_REG_READ(ah, AR_OBS_BUS_1);
                HDPRINTF(ah, HAL_DBG_RX,
                    "%s: rx failed to go idle in 10 ms RXSM=0x%x\n",
                    __func__, reg);

                return AH_FALSE;    // Indicate failure
            }
        }
    }
    else {
        OS_REG_CLR_BIT(ah, AR_DIAG_SW, (AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT));
    }

    return AH_TRUE;    // Function completed successfully
}

/*
 * Stop Receive at the DMA engine
 */
HAL_BOOL
ar9300StopDmaReceive(struct ath_hal *ah, u_int timeout)
{
    int wait;
    int cnt = 0, fail_cnt = 0;
    HAL_BOOL status, okay;

#define AH_RX_STOP_DMA_TIMEOUT 10000   /* usec */
#define AH_TIME_QUANTUM        100     /* usec */

#ifdef AR9300_EMULATION
    timeout = 100000;
#else
    if (timeout == 0) {
        timeout = AH_RX_STOP_DMA_TIMEOUT;
    }
#endif

    OS_REG_WRITE(ah, AR_MACMISC, 
        ((AR_MACMISC_DMA_OBS_LINE_8 << AR_MACMISC_DMA_OBS_S) | 
         (AR_MACMISC_MISC_OBS_BUS_1 << AR_MACMISC_MISC_OBS_BUS_MSB_S)));

    while (cnt < 3 && fail_cnt < 5) {
        okay = ath_hal_wait(
            ah, AR_DMADBG_7, AR_DMADBG_RX_STATE, 0, AH_WAIT_TIMEOUT);
        // wait for Rx DMA state machine to become idle
        if (!okay) {
            HDPRINTF(ah, HAL_DBG_RX,
                "reg AR_DMADBG_7 is not 0, instead 0x%08x\n",
                OS_REG_READ(ah, AR_DMADBG_7));
            fail_cnt++;
            cnt = 0;
        }
        cnt++;
    }

    /* Set receive disable bit */
    OS_REG_WRITE(ah, AR_CR, AR_CR_RXD);

    /* Wait for rx enable bit to go low */
    for (wait = timeout / AH_TIME_QUANTUM; wait != 0; wait--) {
        if ((OS_REG_READ(ah, AR_CR) & AR_CR_RXE) == 0) {
            break;
        }
        OS_DELAY(AH_TIME_QUANTUM);
    }

    if (wait == 0) {
        HDPRINTF(ah, HAL_DBG_RX, "%s: dma failed to stop in %d ms\n"
                "AR_CR=0x%08x\nAR_DIAG_SW=0x%08x\n",
                __func__,
                timeout / 1000,
                OS_REG_READ(ah, AR_CR),
                OS_REG_READ(ah, AR_DIAG_SW));
        status = AH_FALSE;
    } else {
        status = AH_TRUE;
    }

    return status;
#undef AH_RX_STOP_DMA_TIMEOUT
#undef AH_TIME_QUANTUM
}

/*
 * Start Transmit at the PCU engine (unpause receive)
 */
void
ar9300StartPcuReceive(struct ath_hal *ah, HAL_BOOL is_scanning)
{

    ar9300EnableMIBCounters(ah);

    ar9300AniReset(ah, is_scanning);

    /* Clear RX_DIS and RX_ABORT after enabling phy errors in aniReset */
    OS_REG_CLR_BIT(ah, AR_DIAG_SW, (AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT));

}

/*
 * Stop Transmit at the PCU engine (pause receive)
 */
void
ar9300StopPcuReceive(struct ath_hal *ah)
{
    OS_REG_SET_BIT(ah, AR_DIAG_SW, AR_DIAG_RX_DIS);

    ar9300DisableMIBCounters(ah);
}

/*
 * Abort current ingress frame at the PCU
 */
void
ar9300AbortPcuReceive(struct ath_hal *ah)
{
    OS_REG_SET_BIT(ah, AR_DIAG_SW, AR_DIAG_RX_ABORT | AR_DIAG_RX_DIS);

    ar9300DisableMIBCounters(ah);
}

/*
 * Set multicast filter 0 (lower 32-bits)
 *               filter 1 (upper 32-bits)
 */
void
ar9300SetMulticastFilter(
    struct ath_hal *ah,
    u_int32_t filter0,
    u_int32_t filter1)
{
    OS_REG_WRITE(ah, AR_MCAST_FIL0, filter0);
    OS_REG_WRITE(ah, AR_MCAST_FIL1, filter1);
}

/*
 * Clear multicast filter by index
 */
HAL_BOOL
ar9300ClrMulticastFilterIndex(struct ath_hal *ah, u_int32_t ix)
{
    u_int32_t val;

    if (ix >= 64) {
        return AH_FALSE;
    }
    if (ix >= 32) {
        val = OS_REG_READ(ah, AR_MCAST_FIL1);
        OS_REG_WRITE(ah, AR_MCAST_FIL1, (val &~ (1 << (ix - 32))));
    } else {
        val = OS_REG_READ(ah, AR_MCAST_FIL0);
        OS_REG_WRITE(ah, AR_MCAST_FIL0, (val &~ (1 << ix)));
    }
    return AH_TRUE;
}

/*
 * Set multicast filter by index
 */
HAL_BOOL
ar9300SetMulticastFilterIndex(struct ath_hal *ah, u_int32_t ix)
{
    u_int32_t val;

    if (ix >= 64) {
        return AH_FALSE;
    }
    if (ix >= 32) {
        val = OS_REG_READ(ah, AR_MCAST_FIL1);
        OS_REG_WRITE(ah, AR_MCAST_FIL1, (val | (1 << (ix - 32))));
    } else {
        val = OS_REG_READ(ah, AR_MCAST_FIL0);
        OS_REG_WRITE(ah, AR_MCAST_FIL0, (val | (1 << ix)));
    }
    return AH_TRUE;
}

/*
 * Get the receive filter.
 */
u_int32_t
ar9300GetRxFilter(struct ath_hal *ah)
{
    u_int32_t bits = OS_REG_READ(ah, AR_RX_FILTER);
    u_int32_t phybits = OS_REG_READ(ah, AR_PHY_ERR);
    if (phybits & AR_PHY_ERR_RADAR) {
        bits |= HAL_RX_FILTER_PHYRADAR;
    }
    if (phybits & (AR_PHY_ERR_OFDM_TIMING | AR_PHY_ERR_CCK_TIMING)) {
        bits |= HAL_RX_FILTER_PHYERR;
    }
    return bits;
}

/*
 * Set the receive filter.
 */
void
ar9300SetRxFilter(struct ath_hal *ah, u_int32_t bits)
{
    u_int32_t phybits;

    OS_REG_WRITE(ah, AR_RX_FILTER,
        (bits & 0xffff) | AR_RX_UNCOM_BA_BAR | AR_RX_COMPR_BAR);
    phybits = 0;
    if (bits & HAL_RX_FILTER_PHYRADAR) {
        phybits |= AR_PHY_ERR_RADAR;
    }
    if (bits & HAL_RX_FILTER_PHYERR) {
        phybits |= AR_PHY_ERR_OFDM_TIMING | AR_PHY_ERR_CCK_TIMING;
    }
    OS_REG_WRITE(ah, AR_PHY_ERR, phybits);
    if (phybits) {
        OS_REG_WRITE(ah, AR_RXCFG,
            OS_REG_READ(ah, AR_RXCFG) | AR_RXCFG_ZLFDMA);
    } else {
        OS_REG_WRITE(ah, AR_RXCFG,
            OS_REG_READ(ah, AR_RXCFG) &~ AR_RXCFG_ZLFDMA);
    }
}

/*
 * Select to pass PLCP headr or EVM data.
 */
HAL_BOOL
ar9300SetRxSelEvm(struct ath_hal *ah, HAL_BOOL selEvm, HAL_BOOL justQuery)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    HAL_BOOL old_value = ahp->ah_getPlcpHdr == 0;

    if (justQuery) {
        return old_value;
    }
    if (selEvm) {
        OS_REG_SET_BIT(ah, AR_PCU_MISC, AR_PCU_SEL_EVM);
    } else {
        OS_REG_CLR_BIT(ah, AR_PCU_MISC, AR_PCU_SEL_EVM);
    }

    ahp->ah_getPlcpHdr = !selEvm;

    return old_value;
}

void ar9300PromiscMode(struct ath_hal *ah, HAL_BOOL enable)
{
    u_int32_t regVal = 0;
    regVal =  OS_REG_READ(ah, AR_RX_FILTER);
    if (enable){
        regVal |= AR_RX_PROM;
    } else{ /*Disable promisc mode */
        regVal &= ~AR_RX_PROM;
    }    
    OS_REG_WRITE(ah, AR_RX_FILTER, regVal);
}
void 
ar9300ReadPktlogReg(
    struct ath_hal *ah,
    u_int32_t *rxfilterVal,
    u_int32_t *rxcfgVal,
    u_int32_t *phyErrMaskVal,
    u_int32_t *macPcuPhyErrRegval)
{

    *rxfilterVal = OS_REG_READ(ah, AR_RX_FILTER);
    *rxcfgVal    = OS_REG_READ(ah, AR_RXCFG);
    *phyErrMaskVal = OS_REG_READ(ah, AR_PHY_ERR);
    *macPcuPhyErrRegval = OS_REG_READ(ah, 0x8338);
    HDPRINTF(ah, HAL_DBG_UNMASKABLE,
        "%s[%d] rxfilterVal 0x%08x , rxcfgVal 0x%08x, phyErrMaskVal 0x%08x "
        "macPcuPhyErrRegval 0x%08x\n",
        __func__, __LINE__,
        *rxfilterVal, *rxcfgVal, *phyErrMaskVal, *macPcuPhyErrRegval);
}

void
ar9300WritePktlogReg(
    struct ath_hal *ah,
    HAL_BOOL enable,
    u_int32_t rxfilterVal,
    u_int32_t rxcfgVal,
    u_int32_t phyErrMaskVal,
    u_int32_t macPcuPhyErrRegVal)
{
    if (enable) { /* Enable pktlog phyerr setting */
        OS_REG_WRITE(ah, AR_RX_FILTER, 0xffff | AR_RX_COMPR_BAR | rxfilterVal);
        OS_REG_WRITE(ah, AR_PHY_ERR, 0xFFFFFFFF);
        OS_REG_WRITE(ah, AR_RXCFG, rxcfgVal | AR_RXCFG_ZLFDMA);
        OS_REG_WRITE(ah, AR_PHY_ERR_MASK_REG, macPcuPhyErrRegVal | 0xFF);    
    } else { /* Disable phyerr and Restore regs */
        OS_REG_WRITE(ah, AR_RX_FILTER, rxfilterVal);
        OS_REG_WRITE(ah, AR_PHY_ERR, phyErrMaskVal);
        OS_REG_WRITE(ah, AR_RXCFG, rxcfgVal);
        OS_REG_WRITE(ah, AR_PHY_ERR_MASK_REG, macPcuPhyErrRegVal);
    }
    HDPRINTF(ah, HAL_DBG_UNMASKABLE,
        "%s[%d] ena %d rxfilterVal 0x%08x , rxcfgVal 0x%08x, "
        "phyErrMaskVal 0x%08x macPcuPhyErrRegval 0x%08x\n",
        __func__, __LINE__,
        enable, rxfilterVal, rxcfgVal, phyErrMaskVal, macPcuPhyErrRegVal);
}

#endif /* AH_SUPPORT_AR9300 */
