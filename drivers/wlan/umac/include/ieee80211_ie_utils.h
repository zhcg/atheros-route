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
#include "ieee80211_options.h"

#ifndef _IEEE80211_IE_UTILS_H
#define _IEEE80211_IE_UTILS_H

#if UMAC_SUPPORT_IE_UTILS
/**
 * find an IE with given tag in the frame and return pointer
 * to begining of the found ie.
 * @param frm  : pointer where to begin the search.
 * @param efrm : pointer where to end the search.
 * @param tag  : tag of the ie to find.
 * @return pointer to the matching ie (points to the begining of ie including type).
*/
u_int8_t *ieee80211_find_ie(u_int8_t *frm, u_int8_t *efrm,u_int8_t tag);


/**
 * for a given management frame type, return pointer pointing to the begining of ie data 
 * in the frame  
 * @param wbuf    : wbuf containing the frame.
 * @param subtype : subtye of the management frame.
 * @return pointer to the begining of ie data.
*/
u_int8_t *ieee80211_mgmt_iedata(wbuf_t wbuf, int subtype);


#else
static INLINE u_int8_t *ieee80211_find_ie(u_int8_t *frm, u_int8_t *efrm,u_int8_t tag)
{
 KASSERT( (0),("ie_utils module is compiled out, define UMAC_SUPPORT_IE_UTILS to 1 to compile in"));
 return NULL;
}

static INLINE u_int8_t *ieee80211_mgmt_iedata(wbuf_t wbuf, int subtype)
{
 KASSERT( (0),("ie_utils module is compiled out, define UMAC_SUPPORT_IE_UTILS to 1 to compile in"));
 return NULL;
}

#endif

#endif
