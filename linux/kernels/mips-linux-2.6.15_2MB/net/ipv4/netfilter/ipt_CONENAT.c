/* 
 * Copyright (C) 2007 Delta Networks Inc.
 * Copyright (C) 2007 Stone liu <stone.liu@deltaww.com.cn>
 *
 * Description:
 *   This is kernel module for udp cone (full or restrict)nat .
 *
 *   The module follows the Netfilter framework, called extended packet 
 *   matching modules. 
 */


#include <linux/types.h>
#include <linux/ip.h>
#include <linux/timer.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netdevice.h>
#include <linux/if.h>
#include <linux/inetdevice.h>
#include <net/protocol.h>
#include <net/checksum.h>

#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ip_conntrack.h>
#include <linux/netfilter_ipv4/ip_conntrack_core.h>
#include <linux/netfilter_ipv4/ip_conntrack_tuple.h>
#include <linux/netfilter_ipv4/ip_nat_rule.h>

#define ASSERT_READ_LOCK(x)
#define ASSERT_WRITE_LOCK(x)

#include <linux/netfilter_ipv4/listhelp.h>

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

#if 0
#define MORE_DEBUGP printk
#else
#define MORE_DEBUGP(format, args...)
#endif

enum ipt_conenat_step
{
	IPT_CONENAT_DNAT = 1,
	IPT_CONENAT_IN = 2,
};
enum ipt_conenat_type
{
	IPT_CONENAT_FULL = 1,
	IPT_CONENAT_RESTRICT = 2,
};

struct ipt_conenat_info {
	enum ipt_conenat_step step;
	enum ipt_conenat_type type;
};

static inline int
conntrack_tuple_cone_port_cmp(const struct ip_conntrack_tuple_hash *i,
		const u_int16_t portmap, 
		const u_int32_t srcip, 
		const void *targinfo)
{
	const struct ipt_conenat_info *info = targinfo;
	ASSERT_READ_LOCK(&ip_conntrack_lock);
	
	if (DIRECTION(i) != IP_CT_DIR_REPLY 
	    || i->tuple.dst.protonum != IPPROTO_UDP)
		return 0;
	
	if ( info->type == IPT_CONENAT_FULL )
		return ( i->tuple.dst.u.all == portmap);
	else if (info->type == IPT_CONENAT_RESTRICT )
		return ( i->tuple.dst.u.all == portmap && i->tuple.src.ip == srcip );
}

struct ip_conntrack_tuple_hash *
ip_cone_port_used(const u_int16_t portmap, 
		const u_int32_t srcip, 
		const void *targinfo)
{
	int i = 0;
	struct ip_conntrack_tuple_hash *h = NULL;

	read_lock_bh(&ip_conntrack_lock);
	
	for (i = 0; i < ip_conntrack_htable_size; i++)
	{
		h = LIST_FIND(&ip_conntrack_hash[i],
			conntrack_tuple_cone_port_cmp,
			struct ip_conntrack_tuple_hash *,
			portmap,srcip,targinfo);

		if(h != NULL)
			 break;
	}

	read_unlock_bh(&ip_conntrack_lock);
	return h;
}

static unsigned int
conenat_in(struct sk_buff **pskb,
		unsigned int hooknum,
		const struct net_device *in,
		const struct net_device *out,
		const void *targinfo,
		void *userinfo)
{
	struct ip_conntrack *ct;
	enum ip_conntrack_info ctinfo;
	ct = ip_conntrack_get(*pskb, &ctinfo);

	if (!ct)
		return IPT_CONTINUE;
	else if (ct->status & IPS_CONENAT)
			return NF_ACCEPT;	    

	return IPT_CONTINUE;	/* Our job is the interception. */
}

static unsigned int
conenat_dnat(struct sk_buff **pskb,
		unsigned int hooknum,
		const struct net_device *in,
		const struct net_device *out,
		const void *targinfo,
		void *userinfo)
{
	enum ip_conntrack_info ctinfo;
	struct ip_nat_range newrange;
	struct ip_conntrack *ct;	
	struct ip_conntrack_tuple_hash *h;

	IP_NF_ASSERT( hooknum == NF_IP_PRE_ROUTING );

	MORE_DEBUGP("\nnow in conenat dnat\n");
	ct = ip_conntrack_get(*pskb, &ctinfo);
	IP_NF_ASSERT(ct && (ctinfo == IP_CT_NEW));
	
	h = ip_cone_port_used(ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.u.all,
		ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.ip, targinfo);	
	if (!h )
		return IPT_CONTINUE;	/* We don't block any packet. */

	MORE_DEBUGP("conenat_dnat : reply tuplehash src ip : 
			destport %u.%u.%u.%u : %d \n",
			h->tuple.src.ip, h->tuple.dst.u.all);	

	DUMP_TUPLE(&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);

	struct ip_conntrack *temp = tuplehash_to_ctrack(h);
	/* Alter the destination of imcoming packet. */
	newrange = ((struct ip_nat_range)
			{ IP_NAT_RANGE_MAP_IPS | IP_NAT_RANGE_PROTO_SPECIFIED,
			  temp->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.ip, 
			  temp->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.ip,
			  { temp->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u.all }, 
			  { temp->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u.all }});

	ct->status |= IPS_CONENAT;

	/* Hand modified range to generic setup. */
	return ip_nat_setup_info(ct, &newrange, hooknum);
}

static unsigned int
conenat_target(struct sk_buff **pskb,
		const struct net_device *in,
		const struct net_device *out,
		unsigned int hooknum,
		const void *targinfo,
		void *userinfo)
{
	const struct ipt_conenat_info *info = targinfo;
	const struct iphdr *iph = (*pskb)->nh.iph;

	MORE_DEBUGP("%s: step = %s\n", __FUNCTION__, 
		(info->step == IPT_CONENAT_DNAT) ? "dnat" :
		(info->step == IPT_CONENAT_IN) ? "in" : "out");

	/* The Port-trigger only supports TCP and UDP. */
	if (iph->protocol != IPPROTO_UDP)
		return IPT_CONTINUE;

	if (info->step == IPT_CONENAT_IN)
		return conenat_in(pskb, hooknum, in, out, targinfo, userinfo);
	else if (info->step == IPT_CONENAT_DNAT)
		return conenat_dnat(pskb, hooknum, in, out, targinfo, userinfo);

	return IPT_CONTINUE;
}

static int
conenat_check(const char *tablename,
	       const struct ipt_entry *e,
	       void *targinfo,
	       unsigned int targinfosize,
	       unsigned int hook_mask)
{
	if ((strcmp(tablename, "mangle") == 0)) {
		DEBUGP("conenat_check: bad table `%s'.\n", tablename);
		return 0;
	}
	if (hook_mask & ~((1 << NF_IP_PRE_ROUTING) | (1 << NF_IP_FORWARD))) {
		DEBUGP("conenat_check: bad hooks %x.\n", hook_mask);
		return 0;
	}

	return 1;
}

static struct ipt_target conenat_reg = {
	.name		= "CONENAT",
	.target		= conenat_target,
	.checkentry	= conenat_check,
	.me		= THIS_MODULE,
};

static int __init init(void)
{
	return ipt_register_target(&conenat_reg);
}

static void __exit fini(void)
{
	ipt_unregister_target(&conenat_reg);
}

module_init(init);
module_exit(fini);
