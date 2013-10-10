/*
 * Copyright (c) 2008-2010, Atheros Communications Inc.
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

#include "opt_ah.h"

#include "ah.h"
#include "ah_internal.h"
#include "ah_pktlog.h"

#ifndef REMOVE_PKT_LOG

static void __ahdecl3
(*ath_hal_pktlog_ani) (HAL_SOFTC, struct log_ani *, u_int16_t) = AH_NULL;

void __ahdecl ath_hal_log_ani_callback_register(
    void __ahdecl3 (*pktlog_ani)(HAL_SOFTC, struct log_ani *, u_int16_t)
    )
{
    ath_hal_pktlog_ani = pktlog_ani;
}

void
ath_hal_log_ani(HAL_SOFTC sc, struct log_ani *log_data, u_int16_t iflags)
{
    if ( ath_hal_pktlog_ani ) {
        ath_hal_pktlog_ani(sc, log_data, iflags);
    }
}

#endif /* REMOVE_PKT_LOG */
