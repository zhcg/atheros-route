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

#ifndef __IEEE80211_EXTAP_H_
#define __IEEE80211_EXTAP_H_

#ifdef ATH_EXT_AP

#	define eamstr		"%02x:%02x:%02x:%02x:%02x:%02x"
#	define eamac(a)		(a)[0], (a)[1], (a)[2],\
				(a)[3], (a)[4], (a)[5]
#	define eaistr		"%u.%u.%u.%u"
#	define eaip(a)		((a)[0] & 0xff), ((a)[1] & 0xff),\
				((a)[2] & 0xff), ((a)[3] & 0xff)
#	define eastr6		"%02x%02x:%02x%02x:%02x%02x:"\
				"%02x%02x:%02x%02x:%02x%02x:"\
				"%02x%02x:%02x%02x"
#	define eaip6(a)		(a)[0], (a)[1], (a)[2], (a)[3],\
				(a)[4], (a)[5], (a)[6], (a)[7],\
				(a)[8], (a)[9], (a)[10], (a)[11],\
				(a)[12], (a)[13], (a)[14], (a)[15]

#ifdef EXTAP_DEBUG
#	define eadbg1(...)	IEEE80211_DPRINTF(NULL, 0, __VA_ARGS__)
#	define eadbg2(b, c)				\
	do {						\
		IEEE80211_DPRINTF(NULL, 0,		\
			"%s(%d): replacing " #b " "	\
			eamstr " with " eamstr "\n",	\
			__func__, __LINE__,		\
			eamac(b), eamac(c));		\
	} while (0)

#	define eadbg2i(b, c, i)				\
	do {						\
		IEEE80211_DPRINTF(NULL, 0,		\
			"%s(%d): replacing " #b " "	\
			eamstr " with " eamstr		\
			" for " eaistr "\n",		\
			__func__, __LINE__,		\
			eamac(b), eamac(c), eaip(i));	\
	} while (0)

#	define eadbg3(...)	IEEE80211_DPRINTF(NULL, 0, __VA_ARGS__)

#	define print_arp(a)		\
		print_arp_pkt(__func__, __LINE__, a)
#	define mi_add(a, b, c, d)	\
		mi_tbl_add(__func__, __LINE__, a, b, c, d)
#	define mi_lkup(a, b, c)		\
		mi_tbl_lkup(__func__, __LINE__, a, b, c)
#	define print_ipv6	print_ipv6_pkt


#else
#	define eadbg1(...)	/* */
#	define eadbg2(...)	/* */
#	define eadbg2i(...)	/* */
#	define eadbg3(...)	/* */
#	define print_arp(...)	/* */
#	define print_ipv6(...)	/* */
//#	define print_ipv6	print_ipv6_pkt
#	define mi_add		mi_tbl_add
#	define mi_lkup		mi_tbl_lkup
#endif

typedef struct {
	unsigned short	ar_hrd,	/* format of hardware address */
			ar_pro;	/* format of protocol address */
	unsigned char	ar_hln,	/* length of hardware address */
			ar_pln;	/* length of protocol address */
	unsigned short	ar_op;	/* ARP opcode (command) */
	unsigned char	ar_sha[ETH_ALEN],	/* sender hardware address */
			ar_sip[4],		/* sender IP address */
			ar_tha[ETH_ALEN],	/* target hardware address */
			ar_tip[4];		/* target IP address */
} eth_arphdr_t;

typedef struct {
	unsigned char	type,
			len,
			addr[ETH_ALEN];	/* hardware address */
} eth_icmp6_lladdr_t;

int ieee80211_extap_input(wlan_if_t, struct ether_header *);
int ieee80211_extap_output(wlan_if_t, struct ether_header *);

#endif


#endif /* __IEEE80211_EXTAP_H_ */
