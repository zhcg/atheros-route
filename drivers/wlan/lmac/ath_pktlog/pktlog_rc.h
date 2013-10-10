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

#ifndef _PKTLOG_RC_H_
#define _PKTLOG_RC_H_

#include "pktlog.h"

struct log_rcfind{
    struct TxRateCtrl_s *rc;
    u_int8_t rate;
    u_int8_t rateCode;
    int8_t rssiReduce;
    int8_t isProbing;
    int8_t primeInUse;
    int8_t currentPrimeState;
    u_int8_t ac;
    int32_t misc[8]; /* Can be used for HT specific or other misc info */
    /* TBD: Add other required log information */
};

struct log_rcupdate{
    struct TxRateCtrl_s *rc;
    int8_t currentBoostState;
    int8_t useTurboPrime;
    u_int8_t txRate;
    u_int8_t rateCode;
    u_int8_t Xretries;
    u_int8_t retries;
    int8_t   rssiAck;
    u_int8_t ac;
    int32_t misc[8]; /* Can be used for HT specific or other misc info */
    /* TBD: Add other required log information */
};

struct ath_pktlog_rcfuncs {
    void (*pktlog_rcfind)(struct ath_softc *, struct log_rcfind *, u_int16_t);
    void (*pktlog_rcupdate)(struct ath_softc *, struct log_rcupdate *, u_int16_t);
};

#define ath_log_rcfind(_sc, _log_data, _flags)                          \
    do {                                                                \
        if (g_pktlog_rcfuncs) {                                         \
            g_pktlog_rcfuncs->pktlog_rcfind(_sc, _log_data, _flags);    \
        }                                                               \
    } while(0)
                
#define ath_log_rcupdate(_sc, _log_data, _flags)                        \
    do {                                                                \
        if (g_pktlog_rcfuncs) {                                         \
            g_pktlog_rcfuncs->pktlog_rcupdate(_sc, _log_data, _flags);  \
        }                                                               \
    } while(0)

#endif /* _PKTLOG_RC_H_ */
