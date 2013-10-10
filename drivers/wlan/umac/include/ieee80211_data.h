/*
 *  Copyright (c) 2008 Atheros Communications Inc.
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

#ifndef _IEEE80211_DATA_H
#define _IEEE80211_DATA_H

#include <ieee80211_vap.h>


struct ieee80211_tx_status;

int ieee80211_send_wbuf(struct ieee80211vap *vap,struct ieee80211_node *ni, wbuf_t wbuf);
int ieee80211_input(struct ieee80211_node *ni, wbuf_t wbuf, struct ieee80211_rx_status *rs);
int ieee80211_input_all(struct ieee80211com *ic, wbuf_t wbuf, struct ieee80211_rx_status *rs);
int ieee80211_input_monitor(struct ieee80211com *ic, wbuf_t wbuf, struct ieee80211_rx_status *rs);
#if ATH_SUPPORT_IWSPY
int ieee80211_input_iwspy_update_rssi(struct ieee80211com *ic, u_int8_t *address, int8_t rssi);
#endif
wbuf_t ieee80211_encap(struct ieee80211_node   *ni, wbuf_t   wbuf );
wbuf_t __ieee80211_encap(struct ieee80211_node   *ni, wbuf_t   wbuf );

/* external to the umac */
void ieee80211_complete_wbuf(wbuf_t wbuf, struct ieee80211_tx_status *ts);
/* internal to the umac */
void ieee80211_update_stats(struct ieee80211vap *vap, wbuf_t wbuf, struct ieee80211_frame *wh, int type, int subtype, struct ieee80211_tx_status *ts);
void ieee80211_release_wbuf(struct ieee80211_node *ni, wbuf_t wbuf, struct ieee80211_tx_status *ts);
void ieee80211_notify_queue_status(struct ieee80211com *ic, u_int16_t qdepth);
void ieee80211_timeout_fragments(struct ieee80211com *ic, u_int32_t lifetime);
bool ieee80211_is_txq_empty(struct ieee80211com *ic);
int  ieee80211_classify(struct ieee80211_node *ni, wbuf_t wbuf);
int  ieee80211_amsdu_input(struct ieee80211_node *ni, wbuf_t wbuf, struct ieee80211_rx_status *rs,
                      int is_mcast, u_int8_t subtype);

#ifdef ENCAP_OFFLOAD
#define ieee80211_encap( _ni , _wbuf)   (_wbuf)
#define ieee80211_encap_force( _ni , _wbuf)   __ieee80211_encap( _ni, _wbuf)
#else
#define ieee80211_encap( _ni , _wbuf)   __ieee80211_encap( _ni, _wbuf)
#endif

#define IEEE80211_TX_COMPLETE_WITH_ERROR(_wbuf)   do {  \
    struct ieee80211_tx_status ts;                      \
    ts.ts_flags = IEEE80211_TX_ERROR;                   \
    ts.ts_retries = 0;                                  \
    ieee80211_complete_wbuf(_wbuf, &ts);                \
} while (0)

#define IEEE80211_TX_COMPLETE_STATUS_OK(_wbuf)    do {  \
    struct ieee80211_tx_status ts;                      \
    ts.ts_flags = 0;                                    \
    ts.ts_retries = 0;                                  \
    ieee80211_complete_wbuf(_wbuf, &ts);                \
} while (0)

/*
 * call back to be registered to receive a wbuf completion
 * notification.
 */
typedef wlan_action_frame_complete_handler ieee80211_vap_complete_buf_handler;

/*
 * API to register callback to receive a wbuf completion
 * notification.
 */
int ieee80211_vap_set_complete_buf_handler(wbuf_t wbuf, ieee80211_vap_complete_buf_handler handler, 
                               void *arg1); 

typedef enum {
    IEEE80211_VAP_INPUT_EVENT_UCAST=0x1,              /* received unicast packet */ 
    IEEE80211_VAP_INPUT_EVENT_LAST_MCAST=0x2,         /* received mcast frame with more bit not set  */ 
    IEEE80211_VAP_INPUT_EVENT_EOSP=0x4,               /* received frame with EOSP bit set(UAPSD)  */ 
    IEEE80211_VAP_OUTPUT_EVENT_DATA=0x10,             /* need to xmit data */ 
    IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_PS_NULL=0x20, /* completed PS null frame */ 
    IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_SMPS_ACT=0x40,/* completed action mgmt frame */ 
    IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_TX=0x40,      /* completed a non PS null frame */ 
    IEEE80211_VAP_OUTPUT_EVENT_TXQ_EMPTY=0x80,        /* txq is empty */ 
} ieee80211_vap_txrx_event_type;

typedef struct _ieee80211_vap_txrx_event {
    ieee80211_vap_txrx_event_type         type;
    struct ieee80211_frame *wh;
    union {
        int status;
        u_int32_t more_data:1;
    } u;
} ieee80211_vap_txrx_event;

typedef void (*ieee80211_vap_txrx_event_handler) (ieee80211_vap_t, ieee80211_vap_txrx_event *event, void *arg);

/**
 * @register a vap txrx event handler.
 * ARGS :
 *  ieee80211_vap_txrx_event_handler : vap txrx vent handler
 *  arg                               : argument passed back via the evnt handler
 *  event_filter                      : bitmask of interested events 
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 * allows more than one event handler to be registered.
 */
int ieee80211_vap_txrx_register_event_handler(ieee80211_vap_t,ieee80211_vap_txrx_event_handler evhandler, void *arg, u_int32_t event_filter);

/**
 * @unregister a vap txrx event handler.
 * ARGS :
 *  ieee80211_vap_txrx_event_handler : vap event handler
 *  arg                         : argument passed back via the evnt handler
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 */
int ieee80211_vap_txrx_unregister_event_handler(ieee80211_vap_t,ieee80211_vap_txrx_event_handler evhandler, void *arg);

#define IEEE80211_MAX_VAP_TXRX_EVENT_HANDLERS            4 

typedef struct _ieee80211_txrx_event_info {
    void*                             iv_txrx_event_handler_arg[IEEE80211_MAX_VAP_TXRX_EVENT_HANDLERS];
    ieee80211_vap_txrx_event_handler  iv_txrx_event_handler[IEEE80211_MAX_VAP_TXRX_EVENT_HANDLERS];
    u_int32_t                         iv_txrx_event_filters[IEEE80211_MAX_VAP_TXRX_EVENT_HANDLERS];
    u_int32_t                         iv_txrx_event_filter;
} ieee80211_txrx_event_info;



#endif /* _IEEE80211_DATA_H */
