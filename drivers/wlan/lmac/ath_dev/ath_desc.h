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

#ifndef ATH_DESC_H
#define ATH_DESC_H

#include "ah.h"
#include "ah_desc.h"
#include "sys/queue.h"

struct ath_softc;
struct ath_txq;

#define ATH_BF_MORE_MPDU    1   /* if there's more fragment for this MSDU */

struct ath_buf_state {
    int                     bfs_nframes;        /* # frames in aggregate */
    u_int16_t               bfs_al;             /* length of aggregate */
    u_int16_t               bfs_frmlen;         /* length of frame */
    int                     bfs_seqno;          /* sequence number */
    int                     bfs_tidno;          /* tid of this frame */
    int                     bfs_retries;        /* current retries */
    struct ath_rc_series    bfs_rcs[4];         /* rate series */
    int                     bfs_useminrate: 1;  /* use minrate */
    int                     bfs_ismcast: 1;     /* is mcast packet */
    int                     bfs_isdata: 1;      /* is a data frame/aggregate */
    int                     bfs_isaggr: 1;      /* is an aggregate */
    int                     bfs_isampdu: 1;     /* is an a-mpdu, aggregate or not */
    int                     bfs_ht: 1;          /* is an HT frame */
    int                     bfs_isretried: 1;   /* is retried */
    int                     bfs_isxretried: 1;  /* is excessive retried */
    int                     bfs_shpreamble: 1;  /* is short preamble */
    int                     bfs_isbar: 1;       /* is a BAR */
    int                     bfs_ispspoll: 1;    /* is a PS-Poll */
    int                     bfs_aggrburst: 1;   /* is a aggr burst */
    int                     bfs_calcairtime:1;  /* requests airtime be calculated when set for tx frame */
#ifdef ATH_SUPPORT_UAPSD
    int                     bfs_qosnulleosp:1;  /* is QoS null EOSP frame */
#endif
    int                     bfs_ispaprd:1;      /* is PAPRD frame */
    int                     bfs_isswaborted: 1; /* is the frame marked as sw aborted*/
#if ATH_SUPPORT_CFEND
    int                     bfs_iscfend:1;  /* is QoS null EOSP frame */
#endif
#ifdef ATH_SWRETRY
    int                     bfs_isswretry: 1;   /* is the frame marked for swretry*/
    int                     bfs_swretries;      /* number of swretries made*/
    int                     bfs_totaltries;     /* total tries including hw retries*/
#endif
    int                     bfs_qnum;           /* h/w queue number */
    int                     bfs_rifsburst_elem; /* RIFS burst/bar */
    int                     bfs_nrifsubframes;  /* # of elements in burst */
    HAL_KEY_TYPE            bfs_keytype;        /* key type use to encrypt this frame */
    u_int8_t                bfs_txbfstatus;     /* for TxBF , txbf status from TXS*/
};

#define bf_nframes      bf_state.bfs_nframes
#define bf_al           bf_state.bfs_al
#define bf_frmlen       bf_state.bfs_frmlen
#define bf_retries      bf_state.bfs_retries
#define bf_seqno        bf_state.bfs_seqno
#define bf_tidno        bf_state.bfs_tidno
#define bf_rcs          bf_state.bfs_rcs
#define bf_isdata       bf_state.bfs_isdata
#define bf_ismcast      bf_state.bfs_ismcast
#define bf_useminrate   bf_state.bfs_useminrate
#define bf_isaggr       bf_state.bfs_isaggr
#define bf_isampdu      bf_state.bfs_isampdu
#define bf_ht           bf_state.bfs_ht
#define bf_isretried    bf_state.bfs_isretried
#define bf_isxretried   bf_state.bfs_isxretried
#define bf_shpreamble   bf_state.bfs_shpreamble
#define bf_rifsburst_elem  bf_state.bfs_rifsburst_elem
#define bf_nrifsubframes  bf_state.bfs_nrifsubframes
#define bf_keytype      bf_state.bfs_keytype
#define bf_isbar        bf_state.bfs_isbar
#define bf_ispspoll     bf_state.bfs_ispspoll
#define bf_aggrburst    bf_state.bfs_aggrburst
#define bf_calcairtime  bf_state.bfs_calcairtime
#ifdef ATH_SUPPORT_UAPSD
#define bf_qosnulleosp  bf_state.bfs_qosnulleosp
#endif
#ifdef ATH_SWRETRY
#define bf_isswretry    bf_state.bfs_isswretry
#define bf_swretries    bf_state.bfs_swretries
#define bf_totaltries   bf_state.bfs_totaltries
#endif
#define bf_isswaborted  bf_state.bfs_isswaborted
#define bf_qnum         bf_state.bfs_qnum
#define ATH_MAX_MAPS    4

#define	bf_txbfstatus   bf_state.bfs_txbfstatus		/*for TxBF*/

/*
 * Abstraction of a contiguous buffer to transmit/receive.  There is only
 * a single hw descriptor encapsulated here.
 */
struct ath_buf {
    TAILQ_ENTRY(ath_buf)    bf_list;

    struct ath_buf         *bf_lastbf;   /* last buf of this unit (a frame or an aggregate) */
    struct ath_buf         *bf_lastfrm;  /* last buf of this frame */
    struct ath_buf         *bf_next;     /* next subframe in the aggregate */
    struct ath_buf         *bf_rifslast; /* last buf for RIFS burst */
    void                   *bf_mpdu;     /* enclosing frame structure */
    void                   *bf_vdata;    /* virtual addr of data buffer */
    void                   *bf_node;     /* pointer to the node */
    void                   *bf_desc;     /* virtual addr of desc */
    dma_addr_t              bf_daddr;    /* physical addr of desc */
    dma_addr_t              bf_buf_addr[ATH_MAX_MAPS]; /* physical addr of data buffer */
    u_int32_t               bf_buf_len[ATH_MAX_MAPS];  /* len of data */
    u_int32_t               bf_status;
    u_int32_t               bf_flags;    /* tx descriptor flags */
#if ATH_SUPPORT_IQUE && ATH_SUPPORT_IQUE_EXT
    u_int32_t               bf_txduration;/* Tx duration of this buf */
#endif
    u_int16_t               bf_avail_buf;
    u_int16_t               bf_reftxpower;  /* reference tx power */
    struct ath_buf_state    bf_state;    /* buffer state */

    OS_DMA_MEM_CONTEXT(bf_dmacontext)    /* OS Specific DMA context */
};

typedef TAILQ_HEAD(ath_bufhead_s, ath_buf) ath_bufhead;

#define ATH_TXBUF_RESET(_bf, _nmaps) do {       \
    (_bf)->bf_status = 0;                       \
    (_bf)->bf_lastbf = NULL;                    \
    (_bf)->bf_lastfrm = NULL;                   \
    (_bf)->bf_next = NULL;                      \
    (_bf)->bf_avail_buf = _nmaps;                \
    OS_MEMZERO(&((_bf)->bf_state),              \
               sizeof(struct ath_buf_state));   \
    OS_MEMZERO(&((_bf)->bf_buf_addr),           \
               sizeof(dma_addr_t)*ATH_MAX_MAPS);\
    OS_MEMZERO(&((_bf)->bf_buf_len),            \
               sizeof(u_int32_t)*ATH_MAX_MAPS); \
} while(0)

#ifdef ATH_SWRETRY
#define ATH_TXBUF_SWRETRY_RESET(_bf) do {       \
    (_bf)->bf_swretries = 0;                    \
    (_bf)->bf_isswretry = 0;                    \
    (_bf)->bf_totaltries = 0;                   \
    (_bf)->bf_status &= ~ATH_BUFSTATUS_MARKEDSWRETRY;    \
}while(0)
#endif

/*
 * reset the rx buffer.
 * any new fields added to the athbuf and require
 * reset need to be added to this macro.
 * currently bf_status is the only one requires that
 * requires reset.
 */
#define ATH_RXBUF_RESET(_bf)    (_bf)->bf_status=0

#define ATH_BUFSTATUS_DONE      0x00000001  /* hw processing complete, desc processed by hal */
#define ATH_BUFSTATUS_STALE     0x00000002  /* hw processing complete, desc hold for hw */
#define ATH_BUFSTATUS_FREE      0x00000004  /* Rx-only: OS is done with this packet and it's ok to queued it to hw */
#ifdef ATH_SWRETRY
#define ATH_BUFSTATUS_MARKEDSWRETRY 0x00000008 /*Marked for swretry, do not stale it*/
#endif
#define ATH_BUFSTATUS_WAR       0x00000010
#define ATH_BUFSTATUS_SYNCED       0x00000020    /* bf has been synced from the cache */

#ifdef ATH_TX_BUF_FLOW_CNTL
#define ATH_NUM_BUF_IN_Q(num_buf, head) do {            \
     struct ath_buf *bf_traverse;                       \
     TAILQ_FOREACH(bf_traverse, (head), bf_list) {      \
         (*(num_buf))++;                                \
     }                                                  \
} while(0)
#endif

/* Below macro is used while allocating buffers for the descriptors */
#define MAX_BUF_MEM_ALLOC_SIZE  0x20000 /* 128KB - Max kmalloc size in linux */

/*
 * DMA state for tx/rx descriptors.
 */
struct ath_descdma {
    const char*         dd_name;
    void               *dd_desc;        // descriptors
    dma_addr_t          dd_desc_paddr;  // physical addr of dd_desc
    u_int32_t           dd_desc_len;    // size of dd_desc
    struct ath_buf     **dd_bufptr;      // associated buffers

    OS_DMA_MEM_CONTEXT(dd_dmacontext)   // OS Specific DMA context
};

/*
 * Abstraction of a received RX MPDU/MMPDU, or a RX fragment
 */
struct ath_rx_context {
    struct ath_buf          *ctx_rxbuf;     /* associated ath_buf for rx */
};

#define ATH_RX_CONTEXT(_wbuf)           ((struct ath_rx_context *)wbuf_get_context(_wbuf))
#define ATH_SET_RX_CONTEXT(_wbuf, ctx)           (wbuf_set_context(_wbuf, ctx))

/* MPDU/Descriptor API's */
int
ath_descdma_setup(
    struct ath_softc *sc,
    struct ath_descdma *dd, ath_bufhead *head,
    const char *name, int nbuf, int ndesc, int is_tx, int frag_per_msdu);

int
ath_txstatus_setup(
    struct ath_softc *sc,
    struct ath_descdma *dd,
    const char *name, int ndesc);


int ath_desc_alloc(struct ath_softc *sc);
void ath_desc_free(struct ath_softc *sc);

/*
 * Athero's extension to TAILQ macros
 */

#define    TAILQ_REMOVE_HEAD_UNTIL(head1, head2, elm, field) do {              \
    TAILQ_FIRST(head2) = TAILQ_FIRST(head1);                                \
    TAILQ_FIRST(head1)->field.tqe_prev = &TAILQ_FIRST(head2);               \
    if ((TAILQ_FIRST(head1) = TAILQ_NEXT((elm), field)) == NULL)            \
        (head1)->tqh_last = &TAILQ_FIRST(head1);                            \
    else                                                                    \
        TAILQ_NEXT((elm), field)->field.tqe_prev = &TAILQ_FIRST(head1);     \
    TAILQ_NEXT((elm), field) = NULL;                                        \
    (head2)->tqh_last = &TAILQ_NEXT((elm), field);                          \
} while (0)

#define TAILQ_REMOVE_HEAD_BEFORE(head1, head2, elm, headname, field) do {   \
    if (TAILQ_PREV((elm), headname, field) == NULL) {                       \
        TAILQ_INIT(head2);                                                  \
    } else {                                                                \
        TAILQ_FIRST(head2) = TAILQ_FIRST(head1);                            \
        (head2)->tqh_last = (elm)->field.tqe_prev;                          \
        *((head2)->tqh_last) = NULL;                                        \
        TAILQ_FIRST(head1)->field.tqe_prev = &TAILQ_FIRST(head2);           \
        TAILQ_FIRST(head1) = (elm);                                         \
        (elm)->field.tqe_prev = &TAILQ_FIRST(head1);                        \
    }                                                                       \
} while (0)

#define TAILQ_INSERTQ_HEAD(head, tq, field) do {                            \
    if ((head)->tqh_first) {                                                \
        *(tq)->tqh_last = (head)->tqh_first;                                \
        (head)->tqh_first->field.tqe_prev = (tq)->tqh_last;                 \
    } else {                                                                \
        (head)->tqh_last = (tq)->tqh_last;                                  \
    }                                                                       \
    (head)->tqh_first = (tq)->tqh_first;                                    \
    (tq)->tqh_first->field.tqe_prev = &(head)->tqh_first;                   \
} while (0)

#endif /* end of ATH_DESC_H */
