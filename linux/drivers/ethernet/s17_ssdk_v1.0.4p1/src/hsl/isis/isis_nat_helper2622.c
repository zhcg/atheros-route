#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/icmp.h>

#include <linux/if_arp.h>   
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/types.h>
#include <linux/kmod.h>
#include <linux/proc_fs.h>
#include <linux/bitops.h>

#include <linux/netdevice.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_arp.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter/xt_multiport.h>
#include <linux/netfilter/xt_tcpudp.h>

#include <net/checksum.h>
#include <net/route.h>
#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_nat_core.h>
#include <net/netfilter/nf_nat_rule.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_expect.h>

static unsigned int napt_handle(unsigned   int   hooknum,
                             struct   sk_buff   *skb,   
                             const   struct   net_device   *in,   
                             const   struct   net_device   *out,   
                             int   (*okfn)(struct   sk_buff   *))
{
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;
	struct nf_conn_nat *nat = NULL;
	__u8 *saddr,*daddr,*ssaddr,*sdaddr,*tsaddr,*tdaddr;
	struct iphdr  *iph = ip_hdr(skb);
	struct gre_hdr_pptp *gh = (struct gre_hdr_pptp *)((__u8 *)iph + 20);
	struct tcphdr *th = tcp_hdr(skb);	
	struct udphdr *uh = udp_hdr(skb);
	struct nf_conn_help * help = NULL;
	//if(hooknum == NF_INET_PRE_ROUTING)
	th = (struct tcphdr *)(((__u8 *)th)+20);

	saddr = (__u8*)&iph->saddr;
   	daddr = (__u8*)&iph->daddr;
	if(!saddr[0]){//iphdr->version != 4 && iphdr->ihl != 5){
		printk("iptables\n");
	}
	if(saddr[0] != 127 && daddr[2] <=2 && daddr[0] == 10 ){//&& daddr[3] <10){
    	//ip_conntrack_destroyed(ct);
    	//return NF_ACCEPT;
    	ct = nf_ct_get(skb, &ctinfo);
	if(ct){
		//printk("%x\n",LINUX_VERSION_CODE);
		//printk("%x\n",KERNEL_VERSION(2,6,0));
		help = nfct_help(ct);
		nat = nfct_nat(ct);
		ssaddr = (__u8*)&ct->tuplehash[0].tuple.src.u3.ip;
		tsaddr = (__u8*)&ct->tuplehash[0].tuple.dst.u3.ip;
    		sdaddr = (__u8*)&ct->tuplehash[1].tuple.src.u3.ip;
    		tdaddr = (__u8*)&ct->tuplehash[1].tuple.dst.u3.ip;
		if(iph->protocol == 0x6){
			printk("TCP SRC %d.%d.%d.%d PORT %d DST %d.%d.%d.%d PORT %d \nIPH LEN %d TTL %x IP ID %x CHECKSUM %x\n",//TCP WINDOW %d\n", 
			saddr[0],saddr[1],saddr[2],saddr[3],ntohs(th->source),daddr[0],daddr[1],daddr[2],daddr[3],ntohs(th->dest),
    			iph->ihl*4,iph->ttl,iph->id,iph->check);//th->window);
		if(ct->status & IPS_NAT_MASK){
			printk("TCP %d.%d.%d.%d to %d.%d.%d.%d, %d.%d.%d.%d to %d.%d.%d.%d %x %x %x %x %d  \n",//%d %d %d
			ssaddr[0],ssaddr[1],ssaddr[2],ssaddr[3],tsaddr[0],tsaddr[1],tsaddr[2],tsaddr[3],
			sdaddr[0],sdaddr[1],sdaddr[2],sdaddr[3],tdaddr[0],tdaddr[1],tdaddr[2],tdaddr[3],
			iph->protocol,ctinfo,ct->status,ct->proto.tcp.state,ct->timeout.expires);
			if(ct->proto.tcp.state == TCP_CONNTRACK_SYN_RECV){
				;//write tcp napt
			}
			else if(ct->proto.tcp.state == TCP_CONNTRACK_LAST_ACK){
				;//delete tcp napt
			}
			if(help && !ctinfo && (ct->proto.tcp.state >= 3)){
				if(help->helper)
				if(!strcmp(help->helper->name,"pptp")){
					printk("PAC %d to %d,PNS %d to %d\n",ntohs(help->help.ct_pptp_info.pac_call_id),
					ntohs(nat->help.nat_pptp_info.pac_call_id),
					ntohs(help->help.ct_pptp_info.pns_call_id),ntohs(nat->help.nat_pptp_info.pns_call_id));
					if((help->help.ct_pptp_info.sstate == PPTP_SESSION_CONFIRMED) &&
					(help->help.ct_pptp_info.cstate == PPTP_CALL_OUT_REQ)){
						;//add gre nat->help.nat_pptp_info.
					}
					else if(help->help.ct_pptp_info.cstate == PPTP_CALL_CLEAR_REQ){
						;//delete gre
					}
					//printk("PPTP Session state %d, Call state %d, PAC Call ID %d, PNS Call ID %d\n",
					//help->help.ct_pptp_info.sstate,help->help.ct_pptp_info.cstate,
					//ntohs(help->help.ct_pptp_info.pac_call_id),ntohs(help->help.ct_pptp_info.pns_call_id));
				}
				//else if(!strcmp(help->helper->name,"ftp")){
					//expect = nf_ct_expect_find_get(ct->ct_net,&ct->tuplehash[1].tuple);
					//if(expect){
					//printk("FTP help!\n");
					//printk("src %x %d, dst %x %d\n",help->helper->tuple.src.u3.ip,ntohs(help->helper->tuple.src.u.tcp.port),
					//help->helper->tuple.dst.u3.ip,help->helper->tuple.dst.u.tcp.port);
					//}
				//}
			}
			/*else if(ct->master){
				help = nfct_help(ct->master);
				if(help)
				if(help->helper)
				if(!strcmp(help->helper->name,"ftp")){
					;//printk("FTP expect!\n");
				}
			}*/
			//printk("message end\n\n");
		}
		}
		//0x01 for icmp, 0x2f for gre, 0x11 for udp
		else if(iph->protocol == 0x11){
			printk("UDP SRC %d.%d.%d.%d PORT %d DST %d.%d.%d.%d PORT %d \nIPH LEN %d TTL %x IP ID %x CHECKSUM %x \n",
			saddr[0],saddr[1],saddr[2],saddr[3],ntohs(uh->source),daddr[0],daddr[1],daddr[2],daddr[3],ntohs(uh->dest),
    			iph->ihl*4,iph->ttl,iph->id,iph->check);
			printk("UDP %d.%d.%d.%d to %d.%d.%d.%d, %d.%d.%d.%d to %d.%d.%d.%d %x %x %x %d  \n",//%d %d %d
			ssaddr[0],ssaddr[1],ssaddr[2],ssaddr[3],tsaddr[0],tsaddr[1],tsaddr[2],tsaddr[3],
			sdaddr[0],sdaddr[1],sdaddr[2],sdaddr[3],tdaddr[0],tdaddr[1],tdaddr[2],tdaddr[3],
			iph->protocol,ctinfo,ct->status,ct->timeout.expires);
			printk("\n");
		}
		/*else if(iph->protocol == 0x2f){
			printk("GRE SRC %d.%d.%d.%d DST %d.%d.%d.%d CALL ID %d SEQ %d \nIPH LEN %d TTL %x IP ID %x CHECKSUM %x \n",
			saddr[0],saddr[1],saddr[2],saddr[3],daddr[0],daddr[1],daddr[2],daddr[3],ntohs(gh->call_id),ntohl(gh->seq),
    			iph->ihl*4,iph->ttl,iph->id,iph->check);
			printk("GRE %d.%d.%d.%d to %d.%d.%d.%d, %d.%d.%d.%d to %d.%d.%d.%d %x %x %x %x %x %d \n",//%d %d %d
			ssaddr[0],ssaddr[1],ssaddr[2],ssaddr[3],tsaddr[0],tsaddr[1],tsaddr[2],tsaddr[3],
			sdaddr[0],sdaddr[1],sdaddr[2],sdaddr[3],tdaddr[0],tdaddr[1],tdaddr[2],tdaddr[3],
			iph->protocol,ctinfo,ct->status,ct->proto.gre.timeout,ct->proto.gre.stream_timeout,ct->timeout.expires);
			printk("\n");
		}*/
	}
	}
	return NF_ACCEPT;
}

#define MAC_LEN 6
#define IP_LEN 4
#define ARP_HEADER_LEN 8
#define MAC_BUFFER_LEN 10

__u8 local_addr[MAC_LEN];
__u8 local_addr_buffer[MAC_BUFFER_LEN][MAC_LEN];
__u8 bnum = 0;
__u8 bflag = 0;


static unsigned int arp_in(unsigned int hook,
				 struct sk_buff *skb,
				 const struct net_device *in,
				 const struct net_device *out,
				 int (*okfn)(struct sk_buff *))
{
	struct arphdr *arp = NULL;
	__u8 * sip;
	__u8 * dip;
	__u8 * smac;
	__u8 * dmac;
	__u8 i = 0;
	__u8 flag = 0;
	struct net_device *dev;
	//dev = dev_base;
	arp = arp_hdr(skb);
	smac = ((__u8 *)arp)+ARP_HEADER_LEN;
	if(ntohs(arp->ar_op) == ARPOP_REPLY && memcmp(local_addr,smac,MAC_LEN)){// && bflag){
		/*for(i= 0;i<bnum;i++){
			if(!memcmp(smac,local_addr_buffer[i],MAC_LEN))
				flag = 1;
		}
		if(!flag){
		memcpy(local_addr_buffer[bnum],smac,MAC_LEN);
		bnum++;
		if(bnum == MAC_BUFFER_LEN)
			bnum = 0;*/
		sip = smac + MAC_LEN;
		dmac = sip + IP_LEN;
		dip = dmac + MAC_LEN;
		//printk("ARP SMAC %x:%x:%x:%x:%x:%x SIP %d.%d.%d.%d DMAC %x:%x:%x:%x:%x:%x DIP %d.%d.%d.%d\n\n",
		//smac[0],smac[1],smac[2],smac[3],smac[4],smac[5],sip[0],sip[1],sip[2],sip[3],
		//dmac[0],dmac[1],dmac[2],dmac[3],dmac[4],dmac[5],dip[0],dip[1],dip[2],dip[3]);
		//}
		//write arp table
	}
	return NF_ACCEPT;
}

void switch_arp_local_mac_set(__u8 * addr){
	memcpy(local_addr,addr,MAC_LEN);//write local mac to this module
	bflag = 1;
}
EXPORT_SYMBOL_GPL(switch_arp_local_mac_set);

static int nat_set_ctl(struct sock *sk, int cmd, void __user *user, unsigned int len);
static int nat_get_ctl(struct sock *sk, int cmd, void __user *user, int *len);
static struct nf_sockopt_ops * ipt_sockopts = NULL;
static struct nf_sockopt_ops nat_sockopts = {
	.pf		= PF_INET,
	.set_optmin	= 31,
	.set_optmax	= 32,
#ifdef CONFIG_COMPAT
	.compat_set	= nat_set_ctl,
#endif
	.set		= nat_set_ctl,
	.get_optmin	= 31,
	.get_optmax	= 33,
#ifdef CONFIG_COMPAT
	.compat_get	= nat_get_ctl,
#endif
	.get		= nat_get_ctl,
	.owner		= THIS_MODULE,
};

static unsigned int change = 0;
static __u8 *gbuffer,*sbuffer;
static unsigned int glen,slen;
#define NAT_NUM 32
#define IPT_BUFFER_INIT_LEN 1000
struct snat{
	__u32 sip;
	__u16 min,max;
	__u32 tsip;
}nat_map[NAT_NUM];

static void nat_del(unsigned int newnum,unsigned int oldnum)
{
	int i,j;
	struct ipt_entry *gentry;
	struct ipt_entry *sentry;
	struct xt_entry_target *gtarget;
	struct xt_entry_target *starget;
	struct nf_nat_multi_range_compat *grange;
	struct nf_nat_multi_range_compat *srange;
	struct xt_multiport *xport = NULL;
	struct xt_tcp *xtcp = NULL;
	struct xt_udp *xudp = NULL;
	__u8 *gptr,*sptr;
	gptr = gbuffer;
	sptr = sbuffer;
	printk("delete one\n");
	for(i= oldnum;i>4;i--){
		gentry = gptr;
		sentry = sptr;
		gtarget = (__u8 *)gentry + gentry->target_offset;
		starget = (__u8 *)sentry + sentry->target_offset;
		grange = (__u8 *)gtarget + sizeof(*gtarget);
		srange = (__u8 *)starget + sizeof(*starget);
		if(strcmp(gtarget->u.user.name,starget->u.user.name)){
			if(!strcmp(gtarget->u.user.name,"DNAT")){
				if(gentry->ip.src.s_addr || !gentry->ip.dst.s_addr || grange->range[0].min.all)
					return;
				goto delete;
			}
			else if(!strcmp(gtarget->u.user.name,"SNAT")){
				if(!gentry->ip.src.s_addr || gentry->ip.dst.s_addr || grange->range[0].min.all)
					return;
				goto delete;
			}
			return;
		}
		else if(strcmp(gtarget->u.user.name,"DNAT")){
			if(memcmp(gentry,sentry,gentry->next_offset)){
				if(gentry->ip.src.s_addr || !gentry->ip.dst.s_addr || grange->range[0].min.all)
					return;
				goto delete;
			}
		}
		else if(strcmp(gtarget->u.user.name,"SNAT")){
			if(memcmp(gentry,sentry,gentry->next_offset)){
				if(!gentry->ip.src.s_addr || gentry->ip.dst.s_addr || grange->range[0].min.all)
					return;
				goto delete;
			}
		}
		gptr += gentry->next_offset;
		sptr += gentry->next_offset;
	}
	return;
delete:
	printk("delete one %s %s\n",gtarget->u.user.name,starget->u.user.name);
	if(gentry->next_offset == 164){
		;//port disable
	}
	else if(gentry->next_offset == 208){
		if(gentry->ip.proto == 6)
			xtcp = (__u8 *)gtarget - sizeof(*xtcp);
		else if(gentry->ip.proto == 17)
			xudp = (__u8 *)gtarget - sizeof(*xudp) - 2;
	}
	else if(gentry->next_offset == 244){
		xport = (__u8 *)gtarget - sizeof(*xport) - 16;
	}
}

struct ipt_replace old;

static int nat_set_ctl(struct sock *sk, int cmd, void __user *user, unsigned int len)
{
	struct ipt_replace ireplace;
	struct ipt_entry *ientry;
	struct xt_entry_target *xtarget;
	struct nf_nat_multi_range_compat *nfmrange;
	struct xt_multiport *xport = NULL;
	struct xt_tcp *xtcp = NULL;
	struct xt_udp *xudp = NULL;
	__u8 *smac,*dmac,*tsmac,*tdmac;
	__u8 *tb;
	int i=0,j=0,k=0;
	printk("NAT set hook\n");
	if(cmd != IPT_SO_SET_REPLACE)
		goto normal;
	copy_from_user(&ireplace,user,sizeof(ireplace));
	if(slen<ireplace.size){
		kfree(sbuffer);
		slen = ireplace.size;
		sbuffer = kmalloc(slen,GFP_ATOMIC);
	}
	copy_from_user(sbuffer,user+sizeof(ireplace),ireplace.size);
	printk("%d %d %d %d %d %d %d %d %d:%d\n",sizeof(struct ipt_standard),sizeof(struct ipt_entry),sizeof(struct xt_counters),
	sizeof(struct xt_entry_target),sizeof(struct nf_nat_multi_range_compat),
	sizeof(struct xt_multiport),sizeof(struct xt_tcp),sizeof(struct xt_udp),sizeof(struct ipt_error_target),ireplace.size);
	if(strcmp(ireplace.name,"nat") || (ireplace.num_entries == ireplace.num_counters)){
		printk("none NAT or no new entry %d",ireplace.num_entries);
		goto normal;
	}
	if(ireplace.num_entries == 4){//new number is 4
		printk("flush all\n");
		//goto normal;
	}
	if(ireplace.num_entries < ireplace.num_counters){
		nat_del(ireplace.num_entries,ireplace.num_counters);
		goto normal;
	}
	k = ireplace.underflow[4] - old.underflow[4];
	if(k%164 == 0){
		j = 1;
	}
	else if(k%208 == 0){
		j = 2;
	}
	else if(k%244 == 0){
		j = 3;
	}
	else
		goto normal;
	if(ireplace.underflow[0] == old.underflow[0]){//snat(include masquerade)
		//ientry = sbuffer + ireplace.underflow[4] - k;
		ientry = sbuffer + old.underflow[4];
	}
	else{//dnat(include redirect)
		ientry = sbuffer + ireplace.hook_entry[4] - k - sizeof(struct ipt_standard);
	}
	printk("TYPE %d\n",j);
	xtarget = (__u8 *)ientry + ientry->target_offset;
	nfmrange = (__u8 *)xtarget + sizeof(*xtarget);
	smac = &(ientry->ip.src.s_addr);
	dmac = &(ientry->ip.dst.s_addr);
	if(j == 2){//gre need test
		if(ientry->ip.proto == 6)
		xtcp = (__u8 *)xtarget - sizeof(*xtcp);
		else if(ientry->ip.proto == 17)
		xudp = (__u8 *)xtarget - sizeof(*xudp) - 2;
	}
	if(j == 3)
		xport = (__u8 *)xtarget - sizeof(*xport) - 16;
	//temp->ip.proto,temp->ip.smsk.s_addr,temp->ip.dmsk.s_addr,
	//if(!ientry->ip.src.s_addr || ientry->ip.dst.s_addr || nfmrange->range[0].min.all)
	//	goto normal;
	if(strcmp(xtarget->u.user.name,"SNAT")){
		if(j == 1){
			//port disable only use sip
			;
		}
		else if(j == 2){
			//one port or port range
			if(ientry->ip.proto == 6)
				;
			else if(ientry->ip.proto == 17)
				;
		}
		else if(j == 3){
			//some discontinuous ports
			if(ientry->ip.proto == 6)
				;
			else if(ientry->ip.proto == 17)
				;
		}
	}
	else if(strcmp(xtarget->u.user.name,"DNAT")){
		;
	}
	else
		goto normal;
	tsmac = &nfmrange->range[0].min_ip;
	tdmac = &nfmrange->range[0].max_ip;
	printk("%s %x %d %d %d %x: %d %d %d %d %d : %d %d %d %d %d\n",
	ireplace.name,ireplace.valid_hooks,ireplace.num_entries,ireplace.size,ireplace.num_counters,ireplace.entries,
	ireplace.hook_entry[0],ireplace.hook_entry[1],ireplace.hook_entry[2],ireplace.hook_entry[3],ireplace.hook_entry[4],
	ireplace.underflow[0],ireplace.underflow[1],ireplace.underflow[2],ireplace.underflow[3],ireplace.underflow[4]);
	printk("%s proto(%d) src %d.%d.%d.%d dst %d.%d.%d.%d :%d %x %d.%d.%d.%d %d.%d.%d.%d %d %d:%d %d %x %x %x",
	xtarget->u.user.name,ientry->ip.proto,
	smac[0],smac[1],smac[2],smac[3],dmac[0],dmac[1],dmac[2],dmac[3],
	nfmrange->rangesize,nfmrange->range[0].flags,tsmac[0],tsmac[1],tsmac[2],tsmac[3],tdmac[0],tdmac[1],tdmac[2],tdmac[3],
	ntohs(nfmrange->range[0].min.all),ntohs(nfmrange->range[0].max.all),
	ientry->target_offset,ientry->next_offset,ientry->comefrom,ientry->nfcache,ientry->elems);
	if(j == 1)
		printk("\n");
	else if(j == 2){
		if(ientry->ip.proto == 6)
		printk(": %d %d %d %d\n",xtcp->spts[0],xtcp->spts[1],xtcp->dpts[0],xtcp->dpts[1]);
		else if(ientry->ip.proto == 17)
		printk(": %d %d %d %d\n",xudp->spts[0],xudp->spts[1],xudp->dpts[0],xudp->dpts[1]);
	}
	else if(j == 3){
		printk(":");
		for(i=0;i<xport->count;i++)
			printk(" %d",xport->ports[i]);
		printk("\n");
	}
	tb = (__u8 *)sbuffer;
	for(i=0,j=0;i<ireplace.size;i++){
		printk("%02x ",tb[i]);
		j++;
		if(j == 16)
			printk(" ");
		if(j == 32){
			j = 0;
			printk("\n");
		}
	}
	printk("\n");
	/*printk("%d %x %x %x %x %s %s %s %s\n",temp->ip.proto,temp->ip.flags,temp->ip.invflags,temp->ip.smsk.s_addr,temp->ip.dmsk.s_addr,
	temp->ip.iniface,temp->ip.outiface,temp->ip.iniface_mask,temp->ip.outiface_mask);*/
normal:
	old = ireplace;
	return nat_sockopts.set(sk,cmd,user,len);
}

static int nat_get_ctl(struct sock *sk, int cmd, void __user *user, int *len)
{
	
	struct ipt_getinfo info;
	struct ipt_get_entries entries;
	__u8 *buffer,*tb;
	int i,j,k;
	/*printk("NAT get hook %d %d\n",cmd,*len);
	if(cmd == IPT_SO_GET_INFO){
		copy_from_user(&info,user,*len);
		printk("Before %s %d %d %d : %d %d %d %d %d: %d %d %d %d %d\n",info.name,info.valid_hooks,info.num_entries,info.size,
		info.hook_entry[0],info.hook_entry[1],info.hook_entry[2],info.hook_entry[3],info.hook_entry[4],
		info.underflow[0],info.underflow[1],info.underflow[2],info.underflow[3],info.underflow[4]);
	}
	if(cmd == IPT_SO_GET_ENTRIES){
		copy_from_user(&entries,user,sizeof(entries));
		printk("Before %s %d\n",entries.name,entries.size);
	}*/
	k = nat_sockopts.get(sk,cmd,user,len);
	if(cmd == IPT_SO_GET_INFO){
		copy_from_user(&info,user,*len);
		printk("After %s %d %d %d : %d %d %d %d %d: %d %d %d %d %d\n",info.name,info.valid_hooks,info.num_entries,info.size,
		info.hook_entry[0],info.hook_entry[1],info.hook_entry[2],info.hook_entry[3],info.hook_entry[4],
		info.underflow[0],info.underflow[1],info.underflow[2],info.underflow[3],info.underflow[4]);
		tb = (__u8 *)&info;
		for(i=0,j=0;i<*len;i++){
			printk("%02x ",tb[i]);
			j++;
			if(j == 16)
				printk(" ");
			if(j == 32){
				j = 0;
				printk("\n");
			}
		}
		printk("\n\n");
	}
	if(cmd == IPT_SO_GET_ENTRIES){
		if(glen < (*len-sizeof(entries))){
			kfree(gbuffer);
			glen = *len-sizeof(entries);
			gbuffer = kmalloc(glen,GFP_ATOMIC);
		}
		copy_from_user(&entries,user,sizeof(entries));
		copy_from_user(gbuffer,user+sizeof(entries),*len-sizeof(entries));
		printk("%s %d\n",entries.name,entries.size);
		tb = (__u8 *)gbuffer;
		for(i=0,j=0;i<*len-sizeof(entries);i++){
			printk("%02x ",tb[i]);
			j++;
			if(j == 16)
				printk(" ");
			if(j == 32){
				j = 0;
				printk("\n");
			}
		}
		printk("\n\n");
	}
	return k;
}

static struct nf_hook_ops outhook =
{
	.hook		=   napt_handle,
	.owner		=   THIS_MODULE,
	.pf		=   PF_INET,
 	.hooknum	=   NF_INET_POST_ROUTING,
 	.priority	=   NF_IP_PRI_LAST,
};

static struct nf_hook_ops inhook = 
{
	.hook		=   napt_handle,
	.owner		=   THIS_MODULE,
	.pf		=   PF_INET,
 	.hooknum	=   NF_INET_PRE_ROUTING,
 	.priority	=   NF_IP_PRI_CONNTRACK,
};

static struct nf_hook_ops arpinhook = 
{
	.hook		=   arp_in,
 	.hooknum	=   NF_ARP_IN,
	.owner		=   THIS_MODULE,
	.pf		=   NFPROTO_ARP,
 	.priority	=   NF_IP_PRI_FILTER,
};

static void nat_init(){
	//struct nf_sockopt_ops *ops;
	nf_register_sockopt(&nat_sockopts);
	list_for_each_entry(ipt_sockopts,nat_sockopts.list.next,list){
		if(ipt_sockopts->set_optmin == IPT_BASE_CTL)
			break;
	}
	//ipt_sockopts = ops;
	nf_unregister_sockopt(&nat_sockopts);
	nat_sockopts = *ipt_sockopts;
	ipt_sockopts->set = nat_set_ctl;
	ipt_sockopts->get = nat_get_ctl;
	memset(&old,0,sizeof(old));
	old.underflow[4] = sizeof(struct ipt_standard);
	memset(nat_map,0,sizeof(struct snat)*NAT_NUM);
	glen = IPT_BUFFER_INIT_LEN;
	slen = IPT_BUFFER_INIT_LEN;
	gbuffer = kmalloc(glen,GFP_ATOMIC);
	sbuffer = kmalloc(slen,GFP_ATOMIC);
}

static void nat_exit(){
	ipt_sockopts->set = nat_sockopts.set;
	ipt_sockopts->get = nat_sockopts.get;
	if(gbuffer)
		kfree(gbuffer);
	if(sbuffer)
		kfree(sbuffer);
}

static int __init s17_init(void)// Module entry function specified by module_init()
{
	printk("Hello,S17!\n");
	nf_register_hook(&outhook);
	nf_register_hook(&inhook);
	//memset(local_addr,0,MAC_LEN);//may use switch mac to replace this init
	//memset(local_addr_buffer,0,MAC_BUFFER_LEN*MAC_LEN);
	local_addr[0] = 0;
	local_addr[0] = 0x24;
	local_addr[0] = 0x7e;
	local_addr[0] = 0xda;
	local_addr[0] = 0x97;
	local_addr[0] = 0x05;
	nf_register_hook(&arpinhook);
	nat_init();
	return 0;
}

static void __exit s17_exit(void)//Module exit function specified by module_exit()
{
	printk("Goodbye,S17 world!\n");
	nf_unregister_hook(&outhook);
	nf_unregister_hook(&inhook);
	nf_unregister_hook(&arpinhook);
	nat_exit();
	memset(local_addr,0,MAC_LEN);//clear arp local mac
}

module_init(s17_init);
module_exit(s17_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("YWP");
MODULE_DESCRIPTION("NAT NAPT ARP ACL(FILTER/FIREWALL) MODULE");

