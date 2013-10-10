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

/*
 * Public Interface for timer module
 */

/*
 * Definitions for the Atheros Timer module.
 */
#ifndef _DEV_ATH_TIMER_H
#define _DEV_ATH_TIMER_H

/* 
 * timer handler function. 
 *     Must return 0 if timer should be rearmed, or !0 otherwise.
 */

typedef int (*timer_handler_function) (void *context);

struct ath_timer {
    os_timer_t             os_timer;            // timer object
    u_int8_t               active_flag;         // indicates timer is running
    u_int32_t              timer_period;        // timer period
    void*                  context;             // execution context
    int (*timer_handler) (void*);               // handler function
    u_int32_t              signature;           // contains a signature indicating object has been initialized
};

/*
 * Specifies whether ath_cancel_timer can sleep while trying to cancel the timer.
 * Sleeping is allowed only if ath_cancel_timer is running at an IRQL that
 * supports changes in context and memory faults (i.e. Passive Level)
 */
enum timer_flags {
	CANCEL_SLEEP    = 0,
	CANCEL_NO_SLEEP = 1		
};

u_int8_t ath_initialize_timer_module (osdev_t sc_osdev);
u_int8_t ath_initialize_timer_int    (osdev_t                osdev,
                                      struct ath_timer*      timer_object, 
                                      u_int32_t              timer_period, 
                                      timer_handler_function timer_handler, 
                                      void*                  context);
void     ath_set_timer_period        (struct ath_timer* timer_object, u_int32_t timer_period);
u_int8_t ath_start_timer             (struct ath_timer* timer_object);
u_int8_t ath_cancel_timer            (struct ath_timer* timer_object, enum timer_flags flags);
u_int8_t ath_timer_is_active         (struct ath_timer* timer_object);
u_int8_t ath_timer_is_initialized    (struct ath_timer* timer_object);
void ath_free_timer_int(struct ath_timer* timer_object);

#if ATH_DEBUG_TIMERS
#define ath_initialize_timer   printk("%s [%d] creating a timer\n"\
                                    ,__func__,__LINE__), \
                                    ath_initialize_timer_int

#define ath_free_timer   printk("%s [%d] deleting a timer\n"\
                                ,__func__,__LINE__), \
                                ath_free_timer_int
#else
#define ath_initialize_timer    ath_initialize_timer_int
#define ath_free_timer          ath_free_timer_int
#endif

#endif /* _DEV_ATH_TIMER_H */



