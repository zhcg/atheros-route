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

#ifndef __ADF_OS_WAITQ_H
#define __ADF_OS_WAITQ_H

#include <adf_os_waitq_pvt.h>

typedef __adf_os_waitq_t      adf_os_waitq_t;

/**
 * @brief Initialize the waitq
 * 
 * @param wq
 */
static inline void
adf_os_init_waitq(adf_os_waitq_t *wq)
{
    __adf_os_init_waitq(wq);
}
/**
 * @brief Sleep on the waitq, the thread is woken up if somebody
 *        wakes it or the timeout occurs, whichever is earlier
 * @Note  Locks should be taken by the driver
 * 
 * @param wq
 * @param timeout
 * 
 * @return a_status_t
 */
static inline a_status_t
adf_os_sleep_waitq(adf_os_waitq_t *wq, a_uint32_t timeout)
{
    return __adf_os_sleep_waitq(wq, timeout);
}
/**
 * @brief Wake the first guy sleeping in the queue
 * 
 * @param wq
 * 
 * @return a_status_t
 */
static inline a_status_t
adf_os_wake_waitq(adf_os_waitq_t *wq)
{
    return __adf_os_wake_waitq(wq);
}


#endif
