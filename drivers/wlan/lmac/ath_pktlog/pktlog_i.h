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

#ifndef _PKTLOG_INTERNAL_H
#define _PKTLOG_INTERNAL_H

#ifndef REMOVE_PKT_LOG

#include "pktlog.h"
#include "pktlog_rc.h"
#include "ah_pktlog.h"
#include "if_llc.h"

#define PKTLOG_DEFAULT_BUFSIZE (1024 * 1024)
#define PKTLOG_DEFAULT_SACK_THR 3
#define PKTLOG_DEFAULT_TAIL_LENGTH 100
#define PKTLOG_DEFAULT_THRUPUT_THRESH   (64 * 1024)
#define PKTLOG_DEFAULT_PER_THRESH   30
#define PKTLOG_DEFAULT_PHYERR_THRESH   300
#define PKTLOG_DEFAULT_TRIGGER_INTERVAL 500

extern struct ath_pktlog_funcs *g_pktlog_funcs;
extern struct ath_pktlog_rcfuncs *g_pktlog_rcfuncs;

/*
 * internal pktlog API's (common to all OS'es)
 */
void pktlog_init(struct ath_pktlog_info *pl_info);
void pktlog_cleanup(struct ath_pktlog_info *pl_info);
int pktlog_enable(struct ath_softc *sc, int32_t log_state);
int pktlog_setsize(struct ath_softc *sc, int32_t size);
void pktlog_txctl(struct ath_softc *sc, struct log_tx *log_data, u_int16_t flags);
void pktlog_txstatus(struct ath_softc *sc, struct log_tx *log_data, u_int16_t flags);
void pktlog_rx(struct ath_softc *sc, struct log_rx *log_data, u_int16_t flags);
void __ahdecl3 pktlog_ani(HAL_SOFTC hal_sc, struct log_ani *log_data, u_int16_t flags);
void pktlog_rcfind(struct ath_softc *sc, struct log_rcfind *log_data, u_int16_t flags);
void pktlog_rcupdate(struct ath_softc *sc, struct log_rcupdate *log_data, u_int16_t flags);
int pktlog_text(struct ath_softc *sc, char *tbuf, u_int32_t flags);
int pktlog_tcpip(struct ath_softc *sc, wbuf_t wbuf, struct llc *llc, u_int32_t *proto_log, int *proto_len, HAL_BOOL *isSack);
int pktlog_start(struct ath_softc *, int log_state);
int pktlog_read_hdr(struct ath_softc *, void *buf, u_int32_t buf_len,
                    u_int32_t *required_len,
                    u_int32_t *actual_len);
int pktlog_read_buf(struct ath_softc *, void *buf, u_int32_t buf_len,
                    u_int32_t *required_len,
                    u_int32_t *actual_len);


#define get_pktlog_state(_sc)  ((_sc)?(_sc)->pl_info->log_state: \
                                   g_pktlog_info->log_state)

#define get_pktlog_bufsize(_sc)  ((_sc)?(_sc)->pl_info->buf_size: \
                                     g_pktlog_info->buf_size)

/*
 * helper functions (OS dependent)
 */
void pktlog_disable_adapter_logging(void);
int pktlog_alloc_buf(struct ath_softc *sc, struct ath_pktlog_info *pl_info);
void pktlog_release_buf(struct ath_pktlog_info *pl_info);

#endif /* ifndef REMOVE_PKT_LOG */
#endif
