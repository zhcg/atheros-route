/*
 * Copyright (C) 2007 Delta Networks Inc.
 * This module is used by DNI's net-wall for logging packets according to NETGEAR spec.
 * And we are the last match in the netfilter rules to reduce the number of rules.
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/ip.h>
#include <net/icmp.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <net/route.h>

#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_log.h>

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

static int match(const struct sk_buff *skb,
		const struct net_device *in,
		const struct net_device *out,
		const void *matchinfo,
		int offset,
		int *hotdrop)
{
	if (net_ratelimit() == 0)
		return 1;

	struct iphdr *iph = skb->nh.iph;
	struct tcphdr *tcph = (void *)iph + iph->ihl*4;	/* Might be TCP, UDP */
	const struct ipt_logs_info *loginfo = matchinfo;
	int has_port = (iph->protocol == IPPROTO_TCP || iph->protocol == IPPROTO_UDP);
	
	if (has_port && (loginfo->flags & LOG_SPORT))
		printk("[%s] from source: %u.%u.%u.%u, port %u,\n", loginfo->prefix, 
				NIPQUAD(iph->saddr), ntohs(tcph->source));
	else if (has_port && (loginfo->flags & LOG_PORT))
		printk("[%s] from %u.%u.%u.%u:%u to %s:%u,\n", loginfo->prefix, NIPQUAD(iph->saddr), 
				ntohs(tcph->source), loginfo->destip, ntohs(tcph->dest));
	else
		printk("[%s] from source %u.%u.%u.%u,\n", loginfo->prefix, NIPQUAD(iph->saddr));
	
	return 1;
}

static int checkentry(const char *tablename,
		const struct ipt_ip *ip,
		void *matchinfo,
		unsigned int matchsize,
		unsigned int hook_mask)
{
	struct ipt_logs_info *loginfo = (struct ipt_logs_info *)matchinfo;

	if (matchsize != IPT_ALIGN(sizeof(struct ipt_logs_info))) {
		DEBUGP("log: matchsize %u != %u\n", matchsize, IPT_ALIGN(sizeof(struct ipt_logs_info)));
		return 0;
	}

	if (loginfo->prefix[sizeof(loginfo->prefix)-1] != '\0') {
		DEBUGP("log: prefix term %i\n", loginfo->prefix[sizeof(loginfo->prefix)-1]);
		return 0;
	}

	return 1;
}

static struct ipt_match log_match = {
	.name		= "log",
	.match		= &match,
	.checkentry	= &checkentry,
	.me		= THIS_MODULE,
};

static int __init init(void)
{
	if (ipt_register_match(&log_match))
		return -EINVAL;

	return 0;
}

static void __exit fini(void)
{
	ipt_unregister_match(&log_match);
}

module_init(init);
module_exit(fini);
