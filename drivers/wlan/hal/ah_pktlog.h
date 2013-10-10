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

#ifndef __AH_PKTLOG_H__
#define __AH_PKTLOG_H__

/* Parameter types for packet logging driver hooks */
struct log_ani {
    u_int8_t phyStatsDisable;
    u_int8_t noiseImmunLvl;
    u_int8_t spurImmunLvl;
    u_int8_t ofdmWeakDet;
    u_int8_t cckWeakThr;
    u_int16_t firLvl;
    u_int16_t listenTime;
    u_int32_t cycleCount;
    u_int32_t ofdmPhyErrCount;
    u_int32_t cckPhyErrCount;
    int8_t rssi;
    int32_t misc[8]; /* Can be used for HT specific or other misc info */
    /* TBD: Add other required log information */
};

#ifndef __ahdecl3
#ifdef __i386__
#define __ahdecl3 __attribute__((regparm(3)))
#else
#define __ahdecl3
#endif
#endif

#ifndef REMOVE_PKT_LOG
/*
 */
extern void ath_hal_log_ani_callback_register(
            void __ahdecl3 (*pktlog_ani)(void *, struct log_ani *, u_int16_t));
#else
#define ath_hal_log_ani_callback_register(func_ptr)
#endif /* REMOVE_PKT_LOG */

#endif /* __AH_PKTLOG_H__ */

