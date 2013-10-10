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

#ifndef _NET80211_IEEE80211_POWER_H_
#define _NET80211_IEEE80211_POWER_H_

#include "ieee80211_var.h"
#include <ieee80211_data.h>


#define IEEE80211_HAS_DYNAMIC_SMPS_CAP(ic)     (((ic)->ic_htcap & IEEE80211_HTCAP_C_SM_MASK) == IEEE80211_HTCAP_C_SMPOWERSAVE_DYNAMIC)

/*
 * station power save state.
 */
typedef enum {
    IEEE80211_PWRSAVE_INIT=0,
    IEEE80211_PWRSAVE_AWAKE,
    IEEE80211_PWRSAVE_FULL_SLEEP,
    IEEE80211_PWRSAVE_NETWORK_SLEEP
} IEEE80211_PWRSAVE_STATE;             

typedef struct _ieee80211_node_saveq_info {
    u_int16_t mgt_count;
    u_int16_t data_count;
    u_int16_t mgt_len;
    u_int16_t data_len;
    u_int16_t ps_frame_count; /* frames (null,pspoll) with ps bit set */
} ieee80211_node_saveq_info;

/* initialize functions */
void    ieee80211_power_attach(struct ieee80211com *);
void    ieee80211_power_detach(struct ieee80211com *);
void    ieee80211_power_vattach(struct ieee80211vap *, int fullsleep_enable, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t);
void    ieee80211_power_latevattach(struct ieee80211vap *);
void    ieee80211_power_vdetach(struct ieee80211vap *);
void    ieee80211_set_uapsd_flags(struct ieee80211vap *vap, u_int8_t flags);
u_int8_t ieee80211_get_uapsd_flags(struct ieee80211vap *vap);
void     ieee80211_set_wmm_power_save(struct ieee80211vap *vap, u_int8_t enable);
u_int8_t ieee80211_get_wmm_power_save(struct ieee80211vap *vap);
int ieee80211_power_enter_nwsleep(struct ieee80211vap *vap);
int ieee80211_power_exit_nwsleep(struct ieee80211vap *vap);

struct ieee80211_sta_power;
typedef struct ieee80211_sta_power *ieee80211_sta_power_t;

struct ieee80211_pwrsave_smps;
typedef struct ieee80211_pwrsave_smps *ieee80211_pwrsave_smps_t;

struct ieee80211_power;
typedef struct ieee80211_power *ieee80211_power_t;


typedef enum {
    IEEE80211_POWER_STA_PAUSE_COMPLETE,
    IEEE80211_POWER_STA_UNPAUSE_COMPLETE,
    IEEE80211_POWER_STA_SLEEP, /* station entered sleep */
    IEEE80211_POWER_STA_AWAKE, /* station is awake */
}ieee80211_sta_power_event_type; 

typedef enum {
    IEEE80211_POWER_STA_STATUS_SUCCESS,
    IEEE80211_POWER_STA_STATUS_TIMED_OUT,
    IEEE80211_POWER_STA_STATUS_NULL_FAILED,
    IEEE80211_POWER_STA_STATUS_DISCONNECT,
}ieee80211_sta_power_event_status; 

typedef struct _ieee80211_sta_power_event {
    ieee80211_sta_power_event_type type; 
    ieee80211_sta_power_event_status status; 
}  ieee80211_sta_power_event; 

typedef enum {
  IEEE80211_CONTINUE_PSPOLL_FOR_MORE_DATA,
  IEEE80211_WAKEUP_FOR_MORE_DATA,
} ieee80211_pspoll_moredata_handling;

typedef void (*ieee80211_sta_power_event_handler) (ieee80211_sta_power_t powersta, ieee80211_sta_power_event *event, void *arg);

#if UMAC_SUPPORT_STA_POWERSAVE 

/*
 * send keep alive frame .
 */
int ieee80211_sta_power_send_keepalive(ieee80211_sta_power_t handle);


/*
 * initiate pause (enter fake sleep) operation.
 */
int ieee80211_sta_power_pause(ieee80211_sta_power_t handle, u_int32_t timeout);

/*
 * initiate unpause (exit fake sleep) operation.
 */
int ieee80211_sta_power_unpause(ieee80211_sta_power_t handle);


/*
 * @register a power sta event handler.
 * ARGS :
 *  ieee80211_sta_power_event_handler : event handler
 *  arg                               : argument passed back via the evnt handler
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 * allows more than one event handler to be registered.
 */
int ieee80211_sta_power_register_event_handler(ieee80211_sta_power_t,ieee80211_sta_power_event_handler evhandler, void *arg);

/*
 * @unregister a power sta event handler.
 * ARGS :
 *  ieee80211_sta_power_event_handler : event handler
 *  arg                               : argument passed back via the evnt handler
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 * allows more than one event handler to be registered.
 */
int ieee80211_sta_power_unregister_event_handler(ieee80211_sta_power_t,ieee80211_sta_power_event_handler evhandler, void *arg);

void  ieee80211_sta_power_event_tim(ieee80211_sta_power_t handle);

void  ieee80211_sta_power_event_dtim(ieee80211_sta_power_t handle);


int ieee80211_pwrsave_uapsd_set_max_sp_length(wlan_if_t vaphandle,u_int8_t max_sp_val  );

/*
 * enable/disable pspoll mode of operation.
 */
int ieee80211_sta_power_set_pspoll(ieee80211_sta_power_t powersta, u_int32_t pspoll);


/*
 * set mode for handloing more data while in pspoll state.
 *  when set to IEEE80211_CONTINUE_PSPOLL_FOR_MORE_DATA, the SM will send pspolls (in response to TIM)
 * to receive  data frames (one pspoll at a time) until all the data
 * frames are received (data frame with more data bit cleared)
 * when set to IEEE80211_WAKEUP_FOR_MORE_DATA, it will only send one pspoll in response to TIM
 * to receive first data frame and if more frames are  queued on the AP
 * (more dat bit set on the received frame) the driver will wake up
 * by sending null frame with PS=0.
 */
int ieee80211_sta_power_set_pspoll_moredata_handling(ieee80211_sta_power_t powersta, ieee80211_pspoll_moredata_handling mode);


/*
* get state of pspoll.
*/
u_int32_t ieee80211_sta_power_get_pspoll(ieee80211_sta_power_t powersta);


/*
* get the state of pspoll_moredata.
*/
ieee80211_pspoll_moredata_handling ieee80211_sta_power_get_pspoll_moredata_handling(ieee80211_sta_power_t powersta);

/*
 * called when queueing tx packet  starts. to synchronize the SM with tx thread and make
 * sure that the SM will not change the chip power state while a tx queuing  is in progress.
 */
void ieee80211_sta_power_tx_start(ieee80211_sta_power_t powersta);


/*
 * called when tx packet queuing ends . 
 */
void ieee80211_sta_power_tx_end(ieee80211_sta_power_t powersta);

#else /* UMAC_SUPPORT_STA_POWERSAVE */

#define ieee80211_sta_power_send_keepalive(h)                             ENXIO 
#define ieee80211_sta_power_pause(h,t)                                    ENXIO 
#define ieee80211_sta_power_unpause(h)                                    ENXIO 
#define ieee80211_sta_power_register_event_handler(h,e,a)                 do{ void *dummy=(void *)e; dummy=NULL; } while(0)
#define ieee80211_sta_power_unregister_event_handler(h,e,a)               EOK
#define ieee80211_sta_power_event_tim(h) /**/
#define ieee80211_sta_power_event_dtim(h) /**/
#define ieee80211_pwrsave_uapsd_set_max_sp_length(vaphandle,max_sp_val)   ENXIO
#define ieee80211_sta_power_get_pspoll_moredata_handling(powersta)        IEEE80211_WAKEUP_FOR_MORE_DATA
#define ieee80211_sta_power_set_pspoll_moredata_handling(powersta,val)    ENXIO 
#define ieee80211_sta_power_get_pspoll(powersta)                           0
#define ieee80211_sta_power_set_pspoll(powersta,val)                      ENXIO 
#define ieee80211_sta_power_tx_start(powersta)                            /**/
#define ieee80211_sta_power_tx_end(powersta)                              /**/

#endif

/*
 * power save functions for node save queue.
 */
enum ieee80211_node_saveq_param {
    IEEE80211_NODE_SAVEQ_DATA_Q_LEN,
    IEEE80211_NODE_SAVEQ_MGMT_Q_LEN
};

#if UMAC_SUPPORT_POWERSAVE_QUEUE

struct node_powersave_queue {
    spinlock_t  nsq_lock;
    u_int32_t   nsq_len; /* number of queued frames */
    u_int32_t   nsq_bytes; /* number of bytes queued  */
    wbuf_t      nsq_whead;
    wbuf_t      nsq_wtail;
    u_int16_t   nsq_max_len;
    u_int16_t   nsq_num_ps_frames; /* number of frames with PS bit set */
}; 

#define  IEEE80211_NODE_POWERSAVE_QUEUE(_q)  struct node_powersave_queue _q;

void ieee80211_node_saveq_attach(struct ieee80211_node *ni);
void ieee80211_node_saveq_detach(struct ieee80211_node *ni);
int ieee80211_node_saveq_send(struct ieee80211_node *ni, int frame_type);
int ieee80211_node_saveq_drain(struct ieee80211_node *ni);
int ieee80211_node_saveq_age(struct ieee80211_node *ni);
void ieee80211_node_saveq_queue(struct ieee80211_node *ni, wbuf_t wbuf, u_int8_t frame_type);
void ieee80211_node_saveq_flush(struct ieee80211_node *ni);
void ieee80211_node_saveq_cleanup(struct ieee80211_node *ni);
void ieee80211_node_saveq_get_info(struct ieee80211_node *ni, ieee80211_node_saveq_info *info);
void ieee80211_node_saveq_set_param(struct ieee80211_node *ni, enum ieee80211_node_saveq_param param, u_int32_t val);

#else /* UMAC_SUPPORT_POWERSAVE_QUEUE */

#define IEEE80211_NODE_POWERSAVE_QUEUE(_q)                      /* */
#define ieee80211_node_saveq_attach(ni)                         /* */
#define ieee80211_node_saveq_detach(ni)                         /* */
#define ieee80211_node_saveq_send(ni,frame_type)                 0
#define ieee80211_node_saveq_drain(ni)                           0
#define ieee80211_node_saveq_age(ni)                            /* */
#define ieee80211_node_saveq_queue(ni, wbuf,  frame_type)       /* */
#define ieee80211_node_saveq_flush(ni)                          /* */
#define ieee80211_node_saveq_cleanup(ni)                        /* */
#define ieee80211_node_saveq_get_info(ni,info)                  /* */
#define ieee80211_node_saveq_set_param(ni,param,val)            /* */
#endif /* UMAC_SUPPORT_POWERSAVE_QUEUE */




#endif /* _NET80211_IEEE80211_POWER_H_ */
