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

#ifndef _NET_IF_ATH_PROTO_H_
#define _NET_IF_ATH_PROTO_H_

/*
 * Atheros proprietary protocol info.
 */

/*
 * Atheros proprietary SuperG defines.
 */

#define ATH_ETH_TYPE  0x88bd
#define ATH_SNAP_ORGCODE_0 0x00
#define ATH_SNAP_ORGCODE_1 0x03
#define ATH_SNAP_ORGCODE_2 0x7f

struct athl2p_tunnel_hdr {
#if (AH_BYTE_ORDER == AH_LITTLE_ENDIAN)
	u_int32_t  offset	: 11,
	seqNum       		: 11,
	optHdrLen32  		: 2,
	frameType    		: 2,
	proto        		: 6;
#else /* big endian */
	u_int32_t  proto	: 6,
	frameType    		: 2,
	optHdrLen32  		: 2,
	seqNum       		: 11,
	offset       		: 11;
#endif
} __packed;

/*
 * The following defines control compiling Atheros-specific features
 * (see BuildCaps.inc):
 *
 *   ATH_SUPERG_FF 
 *      set to 1 for fast-frame
 */

#define ATH_L2TUNNEL_PROTO_FF 0
/* FF max payload: 
 * 802.2 + FFHDR + HPAD + 802.3 + 802.2 + 1500 + SPAD + 802.3 + 802.2 + 1500:
 *   8   +   4   +  4   +   14  +   8   + 1500 +  6   +   14  +   8   + 1500
 * = 3066
 */ 
#define ATH_FF_MAX_HDR_PAD	4
#define ATH_FF_MAX_SEP_PAD	6
#define ATH_FF_MAX_HDR		30
#define ATH_FF_MAX_PAYLOAD 	3066
#define ATH_FF_MAX_LEN (ATH_FF_MAX_PAYLOAD + IEEE80211_CRC_LEN + \
    (IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_CRCLEN))

/*
 * Store a magic number in skb->cb[] area to indicate FF within driver.
 * Offset of 8B into cb[] is used to preserve vlan tag info.
 */
#define ATH_FF_MAGIC_PUT(_skb) \
	(((struct ieee80211_cb *) (_skb)->cb)->flags |= M_FF)
#define ATH_FF_MAGIC_CLR(_skb) \
	(((struct ieee80211_cb *) (_skb)->cb)->flags &= ~M_FF)
#define ATH_FF_MAGIC_PRESENT(_skb) \
	((((struct ieee80211_cb *) (_skb)->cb)->flags & M_FF) != 0)
#define ATH_FF_NEXTSKB_PUT(_skb, _next) \
	(((struct ieee80211_cb *) (_skb)->cb)->next = _next)
#define ATH_FF_NEXTSKB_GET(_skb) \
	(((struct ieee80211_cb *) (_skb)->cb)->next)

/*
 * default value for the minimum txq depth required for an ath_buf to be
 * placed on the FF staging queue. this value should be >=3 due to interaction
 * with HW compression.
 */
#define ATH_FF_TXQMIN 2

/* 
 * default maximum age an ath_buf is allowed to remain on the staging queue.
 * When this number of ath_bufs have been queued on the txq, after an ath_buf
 * was placed on the staging queue, that ath_buf on the staging queue will be
 * flushed.
 */
#define ATH_FF_STAGEQAGEMAX 5

/*
 * Reserve enough buffer header length to avoid reallocation on fast-frame
 * rx and tx.
 */
#define USE_HEADERLEN_RESV 1

/*
 * Atheros proprietary AMSDU defines.
 */
#ifdef ATH_AMSDU
#define AMSDU_TIMEOUT 15
#define AMSDU_MAX_SUBFRM_LEN 100
#define AMSDU_BUFFER_HEADROOM 64
#ifdef ATH_SUPPORT_HTC
#define AMSDU_MAX_BUFFER_SIZE 1568
#else
// HACK !!! MacOS and NetBSD use a max size of 2048 bytes.
#define AMSDU_MAX_BUFFER_SIZE 2048 
#endif
// AMSDU_MAX_LEN of 1600 was arrived at after experiments in OTA and Azimuth testing.
// If this needs to be changed, please make sure that perf is not compromised.
#define AMSDU_MAX_LEN 1600
#endif

#endif /* _NET_IF_ATH_PROTO_H_ */
