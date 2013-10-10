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

#ifndef _IEEE80211_RESMGR_H_ 
#define _IEEE80211_RESMGR_H_

struct ieee80211_resmgr;
typedef struct ieee80211_resmgr *ieee80211_resmgr_t;

struct ieee80211_resmgr_vap_priv;
typedef struct ieee80211_resmgr_vap_priv *ieee80211_resmgr_vap_priv_t;

/* Resource manager modes */
typedef enum {
    IEEE80211_RESMGR_MODE_SINGLE_CHANNEL,
    IEEE80211_RESMGR_MODE_MULTI_CHANNEL,
} ieee80211_resmgr_mode;

/* Notifications from Resource Manager to registered modules */
typedef enum {
    IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE,
    IEEE80211_RESMGR_OFFCHAN_SWITCH_COMPLETE,
    IEEE80211_RESMGR_OFFCHAN_ABORTED,
    IEEE80211_RESMGR_VAP_START_COMPLETE,
    IEEE80211_RESMGR_CHAN_SWITCH_COMPLETE,
    IEEE80211_RESMGR_SCHEDULE_BSS_COMPLETE,

    IEEE80211_RESMGR_NOTIFICATION_TYPE_COUNT,
} ieee80211_resmgr_notification_type;

typedef enum {
IEEE80211_RESMGR_STATUS_SUCCESS,
IEEE80211_RESMGR_STATUS_NO_ACTIVE_VAP, /* no active vap exists */
IEEE80211_RESMGR_STATUS_NOT_SUPPORTED,  /* can not support */
} ieee80211_resmgr_notification_status;

typedef struct _ieee80211_resmgr_notification {
    ieee80211_resmgr_notification_type    type;
    u_int16_t                             req_id; /* ID of the request causing this event*/ 
    ieee80211_resmgr_notification_status  status;
    ieee80211_vap_t                       vap;    /* VAP associated with notification (if any) */
} ieee80211_resmgr_notification;

typedef void (*ieee80211_resmgr_notification_handler) (ieee80211_resmgr_t resmgr,
                                                       ieee80211_resmgr_notification *notif,
                                                       void *arg);
/*
 * device events for resource manager.
 */
#define IEEE80211_DEVICE_START 1
#define IEEE80211_DEVICE_STOP  2

#define IEEE80211_RESMGR_MAX_NOA_SCHEDULES  2

#include "ieee80211_resmgr_oc_scheduler.h"

typedef enum {
    IEEE80211_RESMGR_NOA_INVALID_PRIO = 0,
    IEEE80211_RESMGR_NOA_HIGH_PRIO,
    IEEE80211_RESMGR_NOA_MEDIUM_PRIO,
    IEEE80211_RESMGR_NOA_LOW_PRIO,
    IEEE80211_RESMGR_NOA_MAX_PRIO
} ieee80211_resmgr_noa_sched_priority_t;

typedef struct ieee80211_resmgr_noa_schedule {
    u_int8_t                                type_count; /* 1: one_shot, 255: periodic, 1 - 254 (number of intervals) */
    ieee80211_resmgr_noa_sched_priority_t   priority;
    u_int32_t                               start_tsf;  /* usec */
    u_int32_t                               duration;   /* usec */
    u_int32_t                               interval;   /* usec */
} *ieee80211_resmgr_noa_schedule_t;

typedef void (*ieee80211_resmgr_noa_event_handler) (void *arg,
                                                       ieee80211_resmgr_noa_schedule_t noa_schedules,
                                                       u_int8_t num_schedules);

#ifndef UMAC_SUPPORT_RESMGR
#define UMAC_SUPPORT_RESMGR 0
#endif

#if UMAC_SUPPORT_RESMGR

/*
 * API to create a Resource manager.
 */
ieee80211_resmgr_t ieee80211_resmgr_create(struct ieee80211com *ic, ieee80211_resmgr_mode mode);

/*
 * Portion of the Resource Manager initialization that requires that all other
 * objects be also initialized
 */
void ieee80211_resmgr_create_complete(ieee80211_resmgr_t);

/*
* API to delete Resource manager.
*/
void ieee80211_resmgr_delete(ieee80211_resmgr_t);

/*
 * Portion of the Resource Manager deletion that requires that all other
 * objects be still initialized
 */
void ieee80211_resmgr_delete_prepare(ieee80211_resmgr_t resmgr);

/**
 * @register a resmgr event handler.
 * ARGS :
 *  ieee80211_resmgr_event_handler : resmgr event handler
 *  arg                            : argument passed back via the evnt handler
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 * allows more than one event handler to be registered.
 */

int ieee80211_resmgr_register_notification_handler(ieee80211_resmgr_t resmgr, 
                                                   ieee80211_resmgr_notification_handler notificationhandler,
                                                   void  *arg);




/**
 * @unregister a resmgr event handler.
 * ARGS :
 *  ieee80211_resmgr_event_handler : resmgr event handler
 *  
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 * 
 */

int ieee80211_resmgr_unregister_notification_handler(ieee80211_resmgr_t resmgr, 
                                                     ieee80211_resmgr_notification_handler notificationhandler,
                                                     void  *arg);


/**
 * @ off channel request.
 * ARGS :
 *  resmgr  : handle to resource manager.
 *  chan    : off channel to switch to. 
 *  reqid   : id of the requestor.
 *  max_bss_chan   : timeout on the bss chan, if 0 the vaps can stay 
 *                   on the bss chan as long as they want(background scan). 
 *                   if non zero then the resource manager completes 
 *                   the off channel request in max_bss_chan by forcing the vaps to pause 
 *                   if the vaps are active at the end of max_bss_chan timeout.
 * RETURNS:
 *  returns EOK if the caller can change the channel synchronously and 
 *  returns EBUSY if the resource manager processes the request asynchronously and 
 *    at the end of processing the request the Resource Manager sends the 
 *    IEEE80211_RESMGR_OFFCHAN_SWITCH_COMPLETE notification to all the registred handlers.
 *  returns  EPERM if there is traffic .
 *  returns -ve for all other failure cases.
 */
int ieee80211_resmgr_request_offchan(ieee80211_resmgr_t resmgr, struct ieee80211_channel *chan, 
                                     u_int16_t reqid, u_int16_t max_bss_chan, u_int32_t max_dwell_time);

/**
 * @ bss channel request.
 * ARGS :
 *  resmgr  : handle to resource manager.
 *  reqid   : id of the requestor.
 * RETURNS:
 *  returns EOK if the caller can change the channel synchronously and 
 *  returns EBUSY if the resource manager processes the request asynchronously and 
 *     at the end of processing the request the Resource Manager posts the 
 *     IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE event to all the registred event handlers.
 */
int ieee80211_resmgr_request_bsschan(ieee80211_resmgr_t resmgr, u_int16_t reqid);

/**
 * @ channel siwth request.
 * ARGS :
 *  resmgr  : handle to resource manager.
 *  vap     : handle to the vap.
 *  chan    : off channel  to switch to. 
 * RETURNS:
 *  returns EOK if the caller can change the channel synchronously and 
 *  returns EBUSY if the resource manager processes the request asynchronously and 
 *     at the end of processing the request the Resource Manager posts the 
 *     IEEE80211_RESMGR_CHAN_SWITCH_COMPLETE event to all the registred event handlers.
 *  return EPERM if this request is from a non primary VAP and
 *     it does not support multiple vaps on different channels yet.
 *  This is primarily used by STA vap when it wants to change channel after it is associated
 *  (one of the reasons a STA vap could switch channel is CSA from its AP). 
 */
int ieee80211_resmgr_request_chanswitch(ieee80211_resmgr_t resmgr, ieee80211_vap_t vap, 
                                        struct ieee80211_channel *chan, u_int16_t reqid);

/**
 * @ vap start request.
 * ARGS :
 *  resmgr  : handle to resource manager.
 *  vap     : handle to the vap.
 *  chan    : off channel  to switch to. 
 *  reqid   : id of the requestor.
 *  max_start_time : time requested by the VAP to complete start operating
 *                    during start the resource manager/shceduler will try to 
 *                    to prevent the channel switch for max_start_time to allow
 *                    Vap to complete start.  
 * RETURNS:
 *  returns EOK if the caller can change the channel synchronously and 
 *  returns EBUSY if the resource manager processes the request asynchronously and 
 *     at the end of processing the request the Resource Manager posts the 
 *     IEEE80211_RESMGR_VAP_START_COMPLETE event to all the registred event handlers.
 *  return EPERM if this request is from a non primary VAP and
 *     it does not support multiple vaps on different channels yet.
 */
int ieee80211_resmgr_vap_start(ieee80211_resmgr_t resmgr, ieee80211_vap_t vap, 
                                        struct ieee80211_channel *chan, u_int16_t reqid, u_int16_t max_start_time);
/*
 * @resmgr vap attach.
 * ARGS :
 *  resmgr  : handle to resource manager.
 *  vap     : handle to the vap.
 */
void ieee80211_resmgr_vattach(ieee80211_resmgr_t resmgr, ieee80211_vap_t vap);

/*
 * @notify vap delete.
 * ARGS :
 *  resmgr  : handle to resource manager.
 *  vap     : handle to the vap.
 */
void ieee80211_resmgr_vdetach(ieee80211_resmgr_t resmgr, ieee80211_vap_t vap);

/*
 * @return an ASCII string describing the notification type
 * ARGS :
 *  type    : notification type.
 */
const char *ieee80211_resmgr_get_notification_type_name(ieee80211_resmgr_notification_type type);

/*
 *  @Allocate a schedule request for submission to the Resource Manager off-channel scheduler. Upon success,
 *  a handle for the request is returned. Otherwise, NULL is returned to indicate a failure
 *  to allocate the request.
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *      vap             :   handle to the vap.
 *      start_tsf       :   Value of starting TSF for scheduling window.
 *      duration_msec   :   Air-time duration in milliseconds (msec).
 *      interval_msec   :   Air-time period or interval in milliseconds (msec).
 *      priority        :   Off-Channel scheduler priority, i.e. High or Low.
 *      chan            :   Channel information
 *      periodic        :   Is request a one-shot or reoccurring type request
 */
ieee80211_resmgr_oc_sched_req_handle_t ieee80211_resmgr_off_chan_sched_req_alloc(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap, u_int32_t duration_msec, u_int32_t interval_msec,
    ieee80211_resmgr_oc_sched_prio_t priority, bool periodic);

/*
 *  @Start or initiate the scheduling of air time for this schedule request handle.
 *  ARGS:
 *      resmgr          :   handle to resource manager
 *      req_handle      :   handle to off-channel scheduler request
 */
int ieee80211_resmgr_off_chan_sched_req_start(ieee80211_resmgr_t resmgr, 
        ieee80211_resmgr_oc_sched_req_handle_t req_handle);

/*
 *  @Stop or cease the scheduling of air time for this schedule request handle.
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *      req_handle      :   handle to off-channel scheduler request.
 */

int ieee80211_resmgr_off_chan_sched_req_stop(ieee80211_resmgr_t resmgr,
        ieee80211_resmgr_oc_sched_req_handle_t req_handle);

/*
 *  @Free a previously allocated schedule request handle.
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *      req_handle      :   handle to off-channel scheduler request.
 */
void ieee80211_resmgr_off_chan_sched_req_free(ieee80211_resmgr_t resmgr,
    ieee80211_resmgr_oc_sched_req_handle_t req_handle);

/*
 *  @Register a Notice Of Absence (NOA) handler
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *    vap           :    handle to VAP structure.
 *    handler           :       Function pointer for processing NOA schedules
 *      arg             :   Opaque parameter for handler
 *  
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 * 
 */
int ieee80211_resmgr_register_noa_event_handler(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap, ieee80211_resmgr_noa_event_handler handler, void *arg);

/*
 *  @Unregister a Notice Of Absence (NOA) handler
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *    vap           :    handle to VAP structure.
 *  
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 * 
 */
int ieee80211_resmgr_unregister_noa_event_handler(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap);

#define     ieee80211_resmgr_exists(ic) true 

/*
 *  @Get the air-time limit when using off-channel scheduling
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *    vap           :    handle to VAP structure.
 *  
 * RETURNS:
 *   returns current air-time limit.
 * 
 */
u_int32_t ieee80211_resmgr_off_chan_sched_get_air_time_limit(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap
    );

/*
 *  @Set the air-time limit when using off-channel scheduling
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *    vap           :    handle to VAP structure.
 *      scheduled_air_time_limit    :   unsigned integer percentage with a range of 1 to 100
 *  
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 * 
 */
int ieee80211_resmgr_off_chan_sched_set_air_time_limit(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap,
    u_int32_t scheduled_air_time_limit
    );

#if UMAC_SUPPORT_RESMGR_SM
/*
*  set resource manager mode.
*  @param    resmgr          :   handle to resource manager.
*  @param    mode            :   operating mode of the resource manager.
*  @ return EOK if succesfull. EPERM if the operation is not permitted.
     EINVAL if it is not supported
*/
int ieee80211_resmgr_setmode(ieee80211_resmgr_t resmgr, ieee80211_resmgr_mode mode);

/*
*  get resource manager mode.
*  @param    resmgr          :   handle to resource manager.
*  @ return operating mode of the resource manager.
*/
ieee80211_resmgr_mode ieee80211_resmgr_getmode(ieee80211_resmgr_t resmgr);


#define     ieee80211_resmgr_active(ic) true 

#else

#define     ieee80211_resmgr_active(ic) false
#define     ieee80211_resmgr_setmode(_resmgr,_mode)  EINVAL
#define     ieee80211_resmgr_getmode(_resmgr) IEEE80211_RESMGR_MODE_SINGLE_CHANNEL

#endif

#else

#define     ieee80211_resmgr_setmode(_resmgr,_mode)  EINVAL
#define     ieee80211_resmgr_getmode(_resmgr) IEEE80211_RESMGR_MODE_SINGLE_CHANNEL
#define     ieee80211_resmgr_create(ic, mode)  (NULL)
#define     ieee80211_resmgr_create_complete(_resmgr) /* */ 
#define     ieee80211_resmgr_delete(ic)  /* */ 
#define     ieee80211_resmgr_delete_prepare(_resmgr) /* */ 
#define     ieee80211_resmgr_vattach(_resmgr,_ap)  /**/
#define     ieee80211_resmgr_vdetach(_resmgr, _vap) /**/
/* To keep the compiler happy add a dummy access to _handler */
#define     ieee80211_resmgr_unregister_notification_handler(_resmgr,_handler,_arg) EINVAL
#define     ieee80211_resmgr_vap_start(_resmgr, _vap,_chan, reqid, starttime) EOK 
#define     ieee80211_resmgr_request_offchan(resmgr,chan,reqid,max_bss_chan,max_dwell_time) EOK
#define     ieee80211_resmgr_request_bsschan(resmgr,reqid) EOK
#define     ieee80211_resmgr_request_chanswitch(resmgr,vap,chan,reqid) EOK
#define     ieee80211_resmgr_exists(ic) false 
#define     ieee80211_resmgr_active(ic) false
#define     ieee80211_resmgr_get_notification_type_name(type) "unknown"
#define     ieee80211_resmgr_off_chan_sched_req_alloc(resmgr, vap, duration_msec, interval_msec, priority, periodic) (NULL)
#define     ieee80211_resmgr_off_chan_sched_req_start(resmgr, req_handle) (EOK)
#define     ieee80211_resmgr_off_chan_sched_req_stop(resmgr, req_handle)  (EOK)
#define     ieee80211_resmgr_off_chan_sched_req_free(resmgr, req_handle)  /**/
#define     ieee80211_resmgr_register_noa_event_handler(resmgr, vap, handler, arg) (EOK)
#define     ieee80211_resmgr_unregister_noa_event_handler(resmgr, vap)  (EOK)
#define     ieee80211_resmgr_off_chan_sched_get_air_time_limit(resmgr,vap) 100
#define     ieee80211_resmgr_off_chan_sched_set_air_time_limit(resmgr,vap,val) EINVAL 

static INLINE int ieee80211_resmgr_register_notification_handler(ieee80211_resmgr_t resmgr, ieee80211_resmgr_notification_handler notificationhandler, void  *arg)
{
    return EINVAL;
}
#endif
#endif


