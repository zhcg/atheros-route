#ifndef _NF_CONNTRACK_COMMON_H
#define _NF_CONNTRACK_COMMON_H
/* Connection state tracking for netfilter.  This is separated from,
   but required by, the NAT layer; it can also be used by an iptables
   extension. */
enum ip_conntrack_info
{
	/* Part of an established connection (either direction). */
	IP_CT_ESTABLISHED,

	/* Like NEW, but related to an existing connection, or ICMP error
	   (in either direction). */
	IP_CT_RELATED,

	/* Started a new connection to track (only
           IP_CT_DIR_ORIGINAL); may be a retransmission. */
	IP_CT_NEW,

	/* >= this indicates reply direction */
	IP_CT_IS_REPLY,

	/* Number of distinct IP_CT types (no NEW in reply dirn). */
	IP_CT_NUMBER = IP_CT_IS_REPLY * 2 - 1
};

/* Bitset representing status of connection. */
enum ip_conntrack_status {
	/* It's an expected connection: bit 0 set.  This bit never changed */
	IPS_EXPECTED_BIT = 0,
	IPS_EXPECTED = (1 << IPS_EXPECTED_BIT),

	/* We've seen packets both ways: bit 1 set.  Can be set, not unset. */
	IPS_SEEN_REPLY_BIT = 1,
	IPS_SEEN_REPLY = (1 << IPS_SEEN_REPLY_BIT),

	/* Conntrack should never be early-expired. */
	IPS_ASSURED_BIT = 2,
	IPS_ASSURED = (1 << IPS_ASSURED_BIT),

	/* Connection is confirmed: originating packet has left box */
	IPS_CONFIRMED_BIT = 3,
	IPS_CONFIRMED = (1 << IPS_CONFIRMED_BIT),

	/* Connection needs src nat in orig dir.  This bit never changed. */
	IPS_SRC_NAT_BIT = 4,
	IPS_SRC_NAT = (1 << IPS_SRC_NAT_BIT),

	/* Connection needs dst nat in orig dir.  This bit never changed. */
	IPS_DST_NAT_BIT = 5,
	IPS_DST_NAT = (1 << IPS_DST_NAT_BIT),

	/* Both together. */
	IPS_NAT_MASK = (IPS_DST_NAT | IPS_SRC_NAT),

	/* Connection needs TCP sequence adjusted. */
	IPS_SEQ_ADJUST_BIT = 6,
	IPS_SEQ_ADJUST = (1 << IPS_SEQ_ADJUST_BIT),

	/* NAT initialization bits. */
	IPS_SRC_NAT_DONE_BIT = 7,
	IPS_SRC_NAT_DONE = (1 << IPS_SRC_NAT_DONE_BIT),

	IPS_DST_NAT_DONE_BIT = 8,
	IPS_DST_NAT_DONE = (1 << IPS_DST_NAT_DONE_BIT),

	/* Both together */
	IPS_NAT_DONE_MASK = (IPS_DST_NAT_DONE | IPS_SRC_NAT_DONE),

	/* Connection is dying (removed from lists), can not be unset. */
	IPS_DYING_BIT = 9,
	IPS_DYING = (1 << IPS_DYING_BIT),

	/* Connection has fixed timeout. */
	IPS_FIXED_TIMEOUT_BIT = 10,
	IPS_FIXED_TIMEOUT = (1 << IPS_FIXED_TIMEOUT_BIT),

#ifdef CONFIG_ATHRS_HW_NAT

        /* Marked when a ct/nat help owns this pkt */
        IPS_NAT_ALG_PKT_BIT = 11,
        IPS_NAT_ALG_PKT = (1 << IPS_NAT_ALG_PKT_BIT),

        /* Marked when the tuple is added to the h/w nat */
        IPS_ATHR_HW_NAT_ADDED_BIT = 12,
        IPS_ATHR_HW_NAT_ADDED = (1 << IPS_ATHR_HW_NAT_ADDED_BIT),

        /* Marked when the tuple is added to the h/w nat for a UDP pkt*/
        IPS_ATHR_HW_NAT_IS_UDP_BIT = 13,
        IPS_ATHR_HW_NAT_IS_UDP = (1 << IPS_ATHR_HW_NAT_IS_UDP_BIT),

        /* Marked when the tuple is added to the h/w nat for a UDP pkt*/
        IPS_ATHR_HW_NAT_IS_ONLY_EGRESS_BIT = 14,
        IPS_ATHR_HW_NAT_IS_ONLY_EGRESS = (1 << IPS_ATHR_HW_NAT_IS_ONLY_EGRESS_BIT),

        /* Marked when the tuple is added to the h/w nat for a UDP pkt*/
        IPS_ATHR_SW_NAT_SKIPPED_BIT = 15,
        IPS_ATHR_SW_NAT_SKIPPED = (1 << IPS_ATHR_SW_NAT_SKIPPED_BIT),

        /*
         * Addded for nat frag table fast hash entry lookup
         */

	IPS_ATHR_HW_CT_INGRESS_BIT = 16,
	IPS_ATHR_HW_CT_INGRESS = (1 << IPS_ATHR_HW_CT_INGRESS_BIT),

	IPS_ATHR_HW_CT_EGRESS_BIT = 17,
	IPS_ATHR_HW_CT_EGRESS = (1 << IPS_ATHR_HW_CT_EGRESS_BIT),

#endif
};

#ifdef __KERNEL__
struct ip_conntrack_stat
{
	unsigned int searched;
	unsigned int found;
	unsigned int new;
	unsigned int invalid;
	unsigned int ignore;
	unsigned int delete;
	unsigned int delete_list;
	unsigned int insert;
	unsigned int insert_failed;
	unsigned int drop;
	unsigned int early_drop;
	unsigned int error;
	unsigned int expect_new;
	unsigned int expect_create;
	unsigned int expect_delete;
};

/* call to create an explicit dependency on nf_conntrack. */
extern void need_conntrack(void);

#endif /* __KERNEL__ */

#endif /* _NF_CONNTRACK_COMMON_H */
