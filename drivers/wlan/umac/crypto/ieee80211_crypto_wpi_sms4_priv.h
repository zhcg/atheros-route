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

#if ATH_SUPPORT_WAPI
#ifndef __CRYPTO_WPI_SMS4__H
#define __CRYPTO_WPI_SMS4__H

#include "osdep.h"
#include "ieee80211_var.h"

#define SMS4_BLOCK_LEN		16

struct sms4_ctx {
    struct ieee80211vap *sms4c_vap;	/* for diagnostics + statistics */
    struct ieee80211com *sms4c_ic;
};

#endif /*__CRYPTO_WPI_SMS4__H*/
#endif /*ATH_SUPPORT_WAPI*/
