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
 * @file adf_os_util.h
 * This file defines utility functions.
 */

#ifndef _ADF_OS_UTIL_H
#define _ADF_OS_UTIL_H

#include <adf_os_util_pvt.h>

/**
 * @brief Compiler-dependent macro denoting code likely to execute.
 */
#define adf_os_unlikely(_expr)     __adf_os_unlikely(_expr)

/**
 * @brief Compiler-dependent macro denoting code unlikely to execute.
 */
#define adf_os_likely(_expr)       __adf_os_likely(_expr)

/**
 * @brief read memory barrier. 
 */
#define adf_os_wmb()                __adf_os_wmb()

/**
 * @brief write memory barrier. 
 */
#define adf_os_rmb()                __adf_os_rmb()

/**
 * @brief read + write memory barrier. 
 */
#define adf_os_mb()                 __adf_os_mb()

/**
 * @brief return the lesser of a, b
 */ 
#define adf_os_min(_a, _b)          __adf_os_min(_a, _b)

/**
 * @brief return the larger of a, b
 */ 
#define adf_os_max(_a, _b)          __adf_os_max(_a, _b)

/**
 * @brief assert "expr" evaluates to false.
 */ 
#define adf_os_assert(expr)         __adf_os_assert(expr)
/**
 * @brief warn & dump backtrace if expr evaluates true
 */
#define adf_os_warn(expr)           __adf_os_warn(expr)
/**
 * @brief supply pseudo-random numbers
 */
static inline void adf_os_get_rand(adf_os_handle_t  hdl, 
                                   a_uint8_t       *ptr, 
                                   a_uint32_t       len)
{
    __adf_os_get_rand(hdl, ptr, len);
}

#endif /*_ADF_OS_UTIL_H*/
