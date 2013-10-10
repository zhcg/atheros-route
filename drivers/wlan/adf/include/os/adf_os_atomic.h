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

/** 
 * @ingroup adf_os_public
 * @file adf_os_atomic.h
 * This file abstracts an atomic counter.
 */
 
#ifndef _ADF_OS_ATOMIC_H
#define _ADF_OS_ATOMIC_H

#include <adf_os_atomic_pvt.h>
/**
 * @brief Atomic type of variable.
 * Use this when you want a simple resource counter etc. which is atomic
 * across multiple CPU's. These maybe slower than usual counters on some
 * platforms/OS'es, so use them with caution.
 */
typedef __adf_os_atomic_t    adf_os_atomic_t;

/** 
 * @brief Initialize an atomic type variable
 * @param[in] v a pointer to an opaque atomic variable
 */
static inline void
adf_os_atomic_init(adf_os_atomic_t *v)
{
    __adf_os_atomic_init(v);
}

/**
 * @brief Read the value of an atomic variable.
 * @param[in] v a pointer to an opaque atomic variable
 *
 * @return the current value of the variable
 */
static inline a_uint32_t
adf_os_atomic_read(adf_os_atomic_t *v)
{
    return (__adf_os_atomic_read(v));
}

/**
 * @brief Increment the value of an atomic variable.
 * @param[in] v a pointer to an opaque atomic variable
 */
static inline void
adf_os_atomic_inc(adf_os_atomic_t *v)
{
    __adf_os_atomic_inc(v);
}

/**
 * @brief Decrement the value of an atomic variable.
 * @param v a pointer to an opaque atomic variable
 */
static inline void
adf_os_atomic_dec(adf_os_atomic_t *v)
{
    __adf_os_atomic_dec(v);
}
#endif
