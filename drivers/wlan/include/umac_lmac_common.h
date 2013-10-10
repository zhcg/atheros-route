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

/*
 * This module define the constant and enum that are used between the upper
 * net80211 layer and lower device ath layer.
 * NOTE: Please do not add any functional prototype in here and also do not
 * include any other header files.
 * Only constants, enum, and structure definitions.
 */

#ifndef UMAC_LMAC_COMMON_H
#define UMAC_LMAC_COMMON_H

/* The following define the various VAP Information Types to register for callbacks */
typedef u_int32_t   ath_vap_infotype;
#define ATH_VAP_INFOTYPE_SLOT_OFFSET    (1<<0)


#endif  //UMAC_LMAC_COMMON_H
