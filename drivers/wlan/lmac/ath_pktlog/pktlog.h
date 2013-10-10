/*
 * Copyright (c) 2000-2003, Atheros Communications Inc.
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

#ifndef _PKTLOG_H_
#define _PKTLOG_H_

#ifndef PKTLOG_TOOLS

#define PKTLOG_INC 1
#include "ath_internal.h"
#include "ieee80211.h"
#undef PKTLOG_INC

#include "ratectrl.h"
#endif /* PKTLOG_TOOLS */
#include "pktlog_fmt.h"
#ifndef PKTLOG_TOOLS
#include "pktlog_rc.h"
#endif /* PKTLOG_TOOLS */

#define PKTLOG_TAG "ATH_PKTLOG"

#ifdef USE_CHATTY_DEBUG
#define PKTLOG_CHATTY_DEBUG(_fmt, ...) printk(_fmt, __VA_ARGS__)
#else
#define PKTLOG_CHATTY_DEBUG(_fmt, ...)
#endif


#ifndef REMOVE_PKT_LOG
/*
 * Version info:
 * 10001 - Logs (rssiLast, rssiLastPrev)
 * 10002 - Logs (rssiLast, rssiLastPrev, rssiLastPrev2)
 */

/*
 * NOTE: the Perl script that processes the packet log data
 *       structures has hardcoded structure offsets.
 *
 * When adding new fields it is important to:
 * - assign a newer verison number (CURRENT_VER_PKT_LOG)
 * - Add fields to the end of the structure.
 * - Observe proper memory alignment of fields. This
 *     eases the process of adding the structure offsets
 *     to the Perl script.
 * - note the new size of the pkt log data structure
 * - Update the Perl script to reflect the new structure and version/size changes
 */

/* Packet log state information */
struct ath_pktlog_info {
    struct ath_pktlog_buf *buf;
    u_int32_t log_state;
    u_int32_t saved_state;
    u_int32_t options;
    int32_t buf_size;           /* Size of buffer in bytes */
    spinlock_t log_lock;
    struct ath_softc *pl_sc;    /*Needed to call 'AirPort_AthrFusion__allocatePktLog' or similar functions */
    int sack_thr;               /* Threshold of TCP SACK packets for triggered stop */
    int tail_length;            /* # of tail packets to log after triggered stop */
    u_int32_t thruput_thresh;           /* throuput threshold in bytes for triggered stop */
	u_int32_t pktlen;          /* (aggregated or single) packet size in bytes */ 
	                           /* a temporary variable for counting TX throughput only */
    u_int32_t per_thresh;               /* PER threshold for triggered stop, 10 for 10%, range [1, 99] */
    u_int32_t phyerr_thresh;          /* Phyerr threshold for triggered stop */
    u_int32_t trigger_interval;       /* time period for counting trigger parameters, in milisecond */
    u_int32_t start_time_thruput;
    u_int32_t start_time_per;
};

struct ath_pktlog_arg {
    struct ath_pktlog_info *pl_info;
    u_int16_t log_type;
    size_t log_size;
    u_int32_t flags;
    char *buf;
};

/* Locking interface for pktlog */
#define PKTLOG_LOCK_INIT(_pl_info)    spin_lock_init(&(_pl_info)->log_lock)
#define	PKTLOG_LOCK_DESTROY(_pl_info)
#define PKTLOG_LOCK(_pl_info)         spin_lock(&(_pl_info)->log_lock)
#define PKTLOG_UNLOCK(_pl_info)       spin_unlock(&(_pl_info)->log_lock)

/* Parameter types for packet logging driver hooks */
struct log_tx {
    void *firstds;
    void *lastds;
    struct ath_buf *bf;
	int32_t nbad;               /* # of bad frames (by looking at block ACK) in an aggregate */
    int32_t misc[8];            /* Can be used for HT specific or other misc info */
    /* TBD: Add other required information */
};

struct log_rx {
    struct ath_desc *ds;
    struct ath_rx_status *status;
    struct ath_buf *bf;
    int32_t misc[8];            /* Can be used for HT specific or other misc info */
    /* TBD: Add other required information */
};

#define PKTLOG_MODE_SYSTEM   1
#define PKTLOG_MODE_ADAPTER  2

typedef enum {
    PKTLOG_PROTO_RX_DESC,
    PKTLOG_PROTO_TX_DESC,
} pktlog_proto_desc_t;

struct log_rcfind;
struct log_rcupdate;

struct ath_pktlog_funcs {
    int (*pktlog_attach) (struct ath_softc *);
    void (*pktlog_detach) (struct ath_softc *);
    void (*pktlog_txctl) (struct ath_softc *, struct log_tx *, u_int16_t);
    void (*pktlog_txstatus) (struct ath_softc *, struct log_tx *, u_int16_t);
    void (*pktlog_rx) (struct ath_softc *, struct log_rx *, u_int16_t);
    int (*pktlog_text) (struct ath_softc *sc, char *text, u_int32_t flags);
    int (*pktlog_start)(struct ath_softc *sc, int log_state);
    int (*pktlog_readhdr)(struct ath_softc *sc, void *buf, u_int32_t,
                          u_int32_t *, u_int32_t *);
    int (*pktlog_readbuf)(struct ath_softc *sc, void *buf, u_int32_t,
                          u_int32_t *, u_int32_t *);
    void (*pktlog_rcfind)(struct ath_softc *, struct log_rcfind *, u_int16_t);
    void (*pktlog_rcupdate)(struct ath_softc *, struct log_rcupdate *, u_int16_t);
};

#define ath_log_txctl(_sc, _log_data, _flags)                           \
        do {                                                            \
            if (g_pktlog_funcs) {                                       \
                g_pktlog_funcs->pktlog_txctl(_sc, _log_data, _flags);   \
            }                                                           \
        } while(0)

#define ath_log_txstatus(_sc, _log_data, _flags)                        \
        do {                                                            \
            if (g_pktlog_funcs) {                                       \
                g_pktlog_funcs->pktlog_txstatus(_sc, _log_data, _flags);\
            }                                                           \
        } while(0)

#define ath_log_rx(_sc, _log_data, _flags)                          \
        do {                                                        \
            if (g_pktlog_funcs) {                                   \
                g_pktlog_funcs->pktlog_rx(_sc, _log_data, _flags);  \
            }                                                       \
        } while(0)

#define ath_log_text(_sc, _log_text, _flags)                        \
        do {                                                        \
            if (g_pktlog_funcs) {                                   \
                g_pktlog_funcs->pktlog_text(_sc, _log_text, _flags);\
            }                                                       \
        } while(0)

#define ath_pktlog_attach(_sc)                                      \
        do {                                                        \
            if (g_pktlog_funcs) {                                   \
                g_pktlog_funcs->pktlog_attach(_sc);                 \
            }                                                       \
        } while(0)

#define ath_pktlog_detach(_sc)                                      \
        do {                                                        \
            if (g_pktlog_funcs) {                                   \
                g_pktlog_funcs->pktlog_detach(_sc);                 \
            }                                                       \
        } while(0)

#define ath_pktlog_start(_sc, _log, _err)                           \
        do {                                                        \
            if (g_pktlog_funcs) {                                   \
                _err = g_pktlog_funcs->pktlog_start(_sc, _log);     \
            } else {                                                \
                _err = 0;                                           \
            }                                                       \
        } while (0)

#define ath_pktlog_read_hdr(_sc, _buf, _len, _rlen, _alen, _err)    \
        do {                                                        \
            if (g_pktlog_funcs) {                                   \
                _err = g_pktlog_funcs->pktlog_readhdr(_sc, _buf,    \
                                       _len, _rlen, _alen);         \
            } else {                                                \
                _err = 0;                                           \
            }                                                       \
        } while (0)

#define ath_pktlog_read_buf(_sc, _buf, _len, _rlen, _alen, _err)    \
        do {                                                        \
            if (g_pktlog_funcs) {                                   \
                _err = g_pktlog_funcs->pktlog_readbuf(_sc, _buf,    \
                                       _len, _rlen, _alen);         \
            } else {                                                \
                _err = 0;                                           \
            }                                                       \
        } while (0)

#endif /* #ifndef REMOVE_PKT_LOG */

#endif /* _PKTLOG_H_ */


