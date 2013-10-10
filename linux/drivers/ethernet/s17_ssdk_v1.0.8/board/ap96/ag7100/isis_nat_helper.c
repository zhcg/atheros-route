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
#include <linux/socket.h>
#include <asm-mips/addrspace.h>

#include <linux/netdevice.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_arp.h>
#include <linux/netfilter_ipv4/ip_conntrack.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_multiport.h>
#include <linux/netfilter_ipv4/ipt_iprange.h>

#include <net/checksum.h>
#include <net/route.h>
#include <linux/netfilter/nf_conntrack_common.h>
#include <linux/netfilter_ipv4/ip_nat.h>
#include <linux/netfilter_ipv4/ip_nat_core.h>
#include <linux/netfilter_ipv4/ip_nat_rule.h>
#include <linux/netfilter_ipv4/ip_conntrack_helper.h>
#include <linux/netfilter_ipv4/ip_conntrack_tftp.h>

#include "build_mode.h"
#include "ssdk_init.h"
#include "fal_nat.h"
#include "fal_ip.h"
#include "hsl.h"

//#define HEADER_CONFIG 1


#define P_ANY  0
#define P_ICMP 1
#define P_TCP  6
#define P_UDP  17

#ifdef AGE_MODE   
#define NAPT_AGE   0xe
#define ARP_AGE 6
#else
#define NAPT_AGE 0xf
#define ARP_AGE 7
#endif
sw_error_t isis_nat_add(a_uint32_t dev_id, fal_nat_entry_t * nat_entry);
sw_error_t isis_nat_del(a_uint32_t dev_id, a_uint32_t del_mode, fal_nat_entry_t * nat_entry);
sw_error_t isis_napt_add(a_uint32_t dev_id, fal_napt_entry_t * napt_entry);
sw_error_t isis_napt_del(a_uint32_t dev_id, a_uint32_t del_mode, fal_napt_entry_t * napt_entry);
sw_error_t isis_napt_get(a_uint32_t dev_id, a_uint32_t get_mode, fal_napt_entry_t * napt_entry);
sw_error_t isis_nat_pub_addr_add(a_uint32_t dev_id, fal_nat_pub_addr_t * entry);
sw_error_t isis_ip_intf_entry_add(a_uint32_t dev_id, fal_intf_mac_entry_t * entry);
sw_error_t isis_ip_host_add(a_uint32_t dev_id, fal_host_entry_t * host_entry);
static int if_mac_entry_add(void);
static int nat_set_ctl(struct sock *sk, int cmd, void __user * user, unsigned int len);
static int nat_get_ctl(struct sock *sk, int cmd, void __user * user, int *len);

void aos_header_read_enable (void);
void aos_header_read_disable (void);

static fal_nat_entry_t nat_entry;
static fal_intf_mac_entry_t if_mac_entry;
static uint8_t rt_int_mac[6] = {0x00, 0x03, 0x7f, 0x12, 0x2b, 0x44};
static uint8_t debug_counter = 0;

static int 
napt_hw_exist(fal_napt_entry_t *napt_entry)
{
#ifdef HEADER_CONFIG
	aos_header_read_enable();
#endif
	sw_error_t rv = isis_napt_get(0, FAL_NAT_ENTRY_ID_EN, napt_entry);
#ifdef HEADER_CONFIG
	aos_header_read_disable();
#endif
	
	printk("napt_hw_exist rv=%d id=0x%x\n", rv, napt_entry->entry_id);
	return (rv==SW_OK);
}

#define NAPT_AGE_POLL_SEC 10
#ifdef HEADER_CONFIG
#define CT_SAVE_MAX 10
static struct ip_conntrack *ct_save[CT_SAVE_MAX];
static  fal_napt_entry_t napt_save[CT_SAVE_MAX];
static pid_t thread_pid = 0;
static int thread_exit = 0;
static int
napt_death_thread(void *data)
{
	struct ip_conntrack *ct;

	while(1) {	
		if(thread_exit) return 0;
		
		int i= 0;
		for(i = 0; i< CT_SAVE_MAX; i++) {
			ct = ct_save[i];
			if(ct && !napt_hw_exist(&napt_save[i])){
				printk("NAPT conntrack del\n");
				ct_save[i] = 0;
				memset(&napt_save[i], 0, sizeof (fal_napt_entry_t));
				ct->timeout.function ((unsigned long)ct);
			}
		}
		schedule_timeout_interruptible(NAPT_AGE_POLL_SEC*HZ);
	}
}

static void 
napt_death_thread_create(void)
{
	if ((thread_pid = kernel_thread(napt_death_thread, 0, CLONE_KERNEL)) < 0){
		printk("thread can't be created\n");
		return;
	}
}

static void 
napt_death_thread_destory(void)
{
	thread_exit = 1;
}

static void
napt_death_add(struct ip_conntrack *ct, fal_napt_entry_t *napt_entry)
{
	int i= 0;
	for(i = 0; i< CT_SAVE_MAX; i++) {
		if(ct_save[i] == ct) {
			return;
		} else if(!ct_save[i]) {
			ct_save[i] = ct;
			memset(&napt_save[i], 0, sizeof (fal_napt_entry_t));
			memcpy(&napt_save[i], napt_entry, sizeof (fal_napt_entry_t));
			printk("napt_death_add %d\n", i);
			del_timer(&ct->timeout);
			return;
		}
	}

	if(i==CT_SAVE_MAX) {
		printk("ct save reach max\n");
	}
}

static int napt_pid_exit(pid_t parent_pid)
{
	struct task_struct *p;
	for_each_process(p) {
		if(parent_pid == p->pid)
	      return 0;
	}
    return 1;
}

#else

typedef struct {
    struct ip_conntrack *ct;
	fal_napt_entry_t *napt_entry;
	struct timer_list timer;
} napt_ct_t;

static void 
napt_death_function(unsigned long data)
{
	napt_ct_t *napt_ct = (napt_ct_t *)data;	

	struct ip_conntrack *ct = napt_ct->ct;
	fal_napt_entry_t *napt_entry = napt_ct->napt_entry;

	if(napt_hw_exist(napt_entry)){
		mod_timer(&napt_ct->timer, jiffies + NAPT_AGE_POLL_SEC*HZ);
		return;
	} else {
		del_timer(&napt_ct->timer);
		if(ct->timeout.function) ct->timeout.function((unsigned long)ct);
		if(napt_entry) kfree(napt_entry);
		if(napt_ct) kfree(napt_ct);
		printk("conntrack del done \n");
    }
}

static void
napt_death_replace(struct ip_conntrack *ct, fal_napt_entry_t *napt_entry)
{
	del_timer(&ct->timeout);

	printk("napt_death_replace\n");
	napt_ct_t *napt_ct = (napt_ct_t *) kmalloc(sizeof(napt_ct_t), GFP_ATOMIC);
	napt_ct->ct = ct;
	napt_ct->napt_entry = (fal_napt_entry_t *) kmalloc(sizeof(fal_napt_entry_t), GFP_ATOMIC);
	memcpy(napt_ct->napt_entry, napt_entry, sizeof(fal_napt_entry_t));
	
	init_timer(&napt_ct->timer);
	napt_ct->timer.data = (unsigned long)napt_ct;
    napt_ct->timer.function = napt_death_function;
	mod_timer(&napt_ct->timer, jiffies + NAPT_AGE_POLL_SEC*HZ);
}
#endif

static void
napt_death(struct ip_conntrack *ct, fal_napt_entry_t *napt_entry)
{
#ifdef AGE_MODE
#ifdef HEADER_CONFIG
	napt_death_add(ct, napt_entry);	
#else
	napt_death_replace(ct, napt_entry);	
#endif
#endif
}

static int
public_ip_add(uint32_t ip)
{
	static int pub_index = 0;
 	fal_nat_pub_addr_t ip_entry;
 	memset(&ip_entry,0,sizeof(ip_entry));
 	ip_entry.entry_id = pub_index++;
 	ip_entry.pub_addr = ip;
 	if(pub_index == 16)
 		pub_index = 0;
 	isis_nat_pub_addr_add(0,&ip_entry);
 	return ip_entry.entry_id;
 	//return 0xf;
}

static unsigned int
napt_handle(unsigned int hooknum,
            struct sk_buff **pskb,
            const struct net_device *in,
            const struct net_device *out, int (*okfn) (struct sk_buff *))
{
    struct ip_conntrack *ct;
    enum ip_conntrack_info ctinfo;
    uint8_t *saddr, *daddr, *ssaddr, *sdaddr, *tsaddr, *tdaddr;
    uint32_t usaddr, udaddr, usport, udport, tusaddr, tusport;
    struct iphdr *iph = (*pskb)->nh.iph;
    struct udphdr *uh = ((struct udphdr *) (*pskb)->h.uh + 20);
 
    struct tftphdr *tftph = ((struct tftphdr *) (*pskb)->h.uh + 28);
	fal_napt_entry_t napt_entry;
	
    saddr = (uint8_t *) & iph->saddr;
    daddr = (uint8_t *) & iph->daddr;
	
    if (daddr[0] == 255)
        return NF_ACCEPT;
    ct = ip_conntrack_get(*pskb, &ctinfo);
    if (!ct)
        return NF_ACCEPT;
    if (((ct->status & IPS_NAT_MASK) != IPS_SRC_NAT)
        &&((ct->status & IPS_NAT_MASK) != IPS_DST_NAT))
            return NF_ACCEPT;

    ssaddr = (uint8_t *) (&ct->tuplehash[0].tuple.src.ip);
    tsaddr = (uint8_t *) (&ct->tuplehash[0].tuple.dst.ip);
    sdaddr = (uint8_t *) (&ct->tuplehash[1].tuple.src.ip);
    tdaddr = (uint8_t *) (&ct->tuplehash[1].tuple.dst.ip);
    if ((ct->status & IPS_NAT_MASK) == IPS_SRC_NAT) {   //snat
        usaddr = ct->tuplehash[0].tuple.src.ip;
        usport = ct->tuplehash[0].tuple.src.u.all;
        udaddr = ct->tuplehash[0].tuple.dst.ip;
        udport = ct->tuplehash[0].tuple.dst.u.all;
        tusaddr = ct->tuplehash[1].tuple.dst.ip;
        tusport = ct->tuplehash[1].tuple.dst.u.all;
    } else {                    //dnat
        usaddr = ct->tuplehash[1].tuple.src.ip;
        usport = ct->tuplehash[1].tuple.src.u.all;
        udaddr = ct->tuplehash[1].tuple.dst.ip;
        udport = ct->tuplehash[1].tuple.dst.u.all;
        tusaddr = ct->tuplehash[0].tuple.dst.ip;
        tusport = ct->tuplehash[0].tuple.dst.u.all;
    }

    if (iph->protocol == P_TCP) {
        //IPS_SRC_NAT IPS_DST_NAT
        if ((ct->proto.tcp.state == TCP_CONNTRACK_SYN_RECV)
            || (ct->proto.tcp.state == TCP_CONNTRACK_CLOSE)) {
            memset((void *) &napt_entry, 0, sizeof (fal_napt_entry_t));

            napt_entry.counter_en = 1;
            napt_entry.counter_id = debug_counter;
            if(++debug_counter == 8) debug_counter = 0;
            
            napt_entry.flags = FAL_NAT_ENTRY_PROTOCOL_TCP | FAL_NAT_ENTRY_TRANS_IPADDR_INDEX;
            napt_entry.src_addr = usaddr;
            napt_entry.src_port = usport;
            napt_entry.dst_addr = udaddr;
            napt_entry.dst_port = udport;
            napt_entry.trans_addr = public_ip_add(tusaddr);
            napt_entry.trans_port = tusport;
            napt_entry.status = NAPT_AGE;
        }
        if (!ct->helper) {
            if (ct->proto.tcp.state == TCP_CONNTRACK_SYN_RECV) {
                printk("isis_napt_add####(pub:%x)####\n", napt_entry.trans_addr);
                isis_napt_add(0, &napt_entry);
				napt_death(ct, &napt_entry);
            } else if (ct->proto.tcp.state == TCP_CONNTRACK_CLOSE) {
                
                printk("isis_napt_del########\n");
				/*if hw nat work, we recv the close any more*/
                isis_napt_del(0, FAL_NAT_ENTRY_ID_EN, &napt_entry);
            }
        } else if (ct->proto.tcp.state == TCP_CONNTRACK_ESTABLISHED) {
        	/*need kernel support ip_conntrack_pptp.ko and ip_nat_pptp.ko*/
            if (!strcmp(ct->helper->name, "pptp")) {
                printk
                    ("PPTP Session state %d, Call state %d, PAC Call ID %d to %d, PNS Call ID %d to %d\n",
                     ct->help.ct_pptp_info.sstate,
                     ct->help.ct_pptp_info.cstate,
                     ntohs(ct->help.ct_pptp_info.pac_call_id),
                     ntohs(ct->nat.help.nat_pptp_info.pac_call_id),
                     ntohs(ct->help.ct_pptp_info.pns_call_id),
                     ntohs(ct->nat.help.nat_pptp_info.pns_call_id));
                if (((ct->help.ct_pptp_info.sstate == PPTP_SESSION_CONFIRMED)
                     && (ct->help.ct_pptp_info.cstate == PPTP_CALL_OUT_REQ))
                    || (ct->help.ct_pptp_info.sstate == PPTP_SESSION_STOPREQ)) {
                    memset((void *) &napt_entry, 0, sizeof (fal_napt_entry_t));

                    napt_entry.counter_en = 1;
                    napt_entry.counter_id = debug_counter;
                    if(++debug_counter == 8) debug_counter = 0;

                    napt_entry.flags =
                        FAL_NAT_ENTRY_PROTOCOL_PPTP |
                        FAL_NAT_ENTRY_TRANS_IPADDR_INDEX;
                    napt_entry.src_addr = usaddr;
                    napt_entry.src_port = ct->help.ct_pptp_info.pns_call_id;
                    napt_entry.dst_addr = udaddr;
                    napt_entry.dst_port = 0;
                    napt_entry.trans_addr = 15;
                    napt_entry.trans_port =
                        ct->nat.help.nat_pptp_info.pns_call_id;
                    napt_entry.status = NAPT_AGE;
                }
                if ((ct->help.ct_pptp_info.sstate == PPTP_SESSION_CONFIRMED)
                    && (ct->help.ct_pptp_info.cstate == PPTP_CALL_OUT_REQ)) {
                    printk("pptp_isis_napt_add########\n");
                    isis_napt_add(0, &napt_entry);
					napt_death(ct, &napt_entry);
                } else if (ct->help.ct_pptp_info.sstate == PPTP_SESSION_STOPREQ) {
                    printk("pptp_isis_napt_del########\n");
					/*if hw nat work, we recv the close any more*/
                    isis_napt_del(0, FAL_NAT_ENTRY_ID_EN, &napt_entry);
                }
            }

        }
    }                          
    else if (iph->protocol == P_UDP) {
		/*need kernel support ip_conntrack_tftp.ko and ip_nat_tftp.ko*/
        if (ct->master && ct->master->helper
            && !strcmp(ct->master->helper->name, "tftp")) {
            if ((ct->status == 0x191 || ct->status == 0x1a1) || 
				((ntohs(uh->len) - 12 < 512) && (ntohs(tftph->opcode) == TFTP_OPCODE_DATA))) {
                memset((void *) &napt_entry, 0, sizeof (fal_napt_entry_t));

                napt_entry.counter_en = 1;
                napt_entry.counter_id = debug_counter;
                if(++debug_counter == 8) debug_counter = 0;

                napt_entry.flags =
                    FAL_NAT_ENTRY_PROTOCOL_UDP |
                    FAL_NAT_ENTRY_TRANS_IPADDR_INDEX;
                napt_entry.src_addr = usaddr;
                napt_entry.src_port = usport;
                napt_entry.dst_addr = udaddr;
                napt_entry.dst_port = udport;
                napt_entry.trans_addr = 15;
                napt_entry.trans_port = tusport;
                napt_entry.status = NAPT_AGE;
            }
            if (ct->status == 0x191 || ct->status == 0x1a1) {  //add
                printk("tftp_isis_napt_add########\n");
                isis_napt_add(0, &napt_entry);
				napt_death(ct, &napt_entry);
				
            } else if ((ntohs(uh->len) - 12 < 512) && (ntohs(tftph->opcode) == TFTP_OPCODE_DATA)) {    //del
                printk("tftp_isis_napt_del########\n");
				/*if hw nat work, we recv the close any more*/
                isis_napt_del(0, FAL_NAT_ENTRY_ID_EN, &napt_entry);
            }
        }
    }
    return NF_ACCEPT;
}

#define MAC_LEN 6
#define IP_LEN 4
#define ARP_HEADER_LEN 8
#define MAC_BUFFER_LEN 10
#define AOS_HEADER_TYPE_ARP 0x8

typedef struct {
	uint16_t ver:2;
	uint16_t pri:3;
	uint16_t type:5;
	uint16_t rev:2;
	uint16_t with_tag:1;
	uint16_t sport:3;
	uint16_t vid;
} aos_header_t;


static unsigned int
arp_in(unsigned int hook,
        struct sk_buff **pskb,
        const struct net_device *in,
        const struct net_device *out, int (*okfn) (struct sk_buff *)) {
    struct arphdr *arp = NULL;
    uint8_t *sip;
    uint8_t *dip;
    uint8_t *smac;
    uint8_t *dmac;

    arp = (*pskb)->nh.arph;
    smac = ((uint8_t *) arp) + ARP_HEADER_LEN;
    //if (ntohs(arp->ar_op) == ARPOP_REQUEST) {
    if (ntohs(arp->ar_op) == ARPOP_REPLY) {
		aos_header_t athr_header;
		memcpy( &athr_header, (*pskb)->head, sizeof(aos_header_t));

		//if(athr_header.type != AOS_HEADER_TYPE_ARP) return NF_ACCEPT;
		//printk("header type:%x port:%d vid:%x\n", athr_header.type, athr_header.sport, athr_header.vid);

        fal_host_entry_t arp_entry;
		memset(&arp_entry,0,sizeof(arp_entry));
		
        sip = smac + MAC_LEN;
        dmac = sip + IP_LEN;
        dip = dmac + MAC_LEN;

		//if_mac_entry_add();//debug
		uint32_t mac_index = if_mac_entry.entry_id;
		uint32_t vid_low = if_mac_entry.vid_low;
		uint32_t vid_offset = athr_header.vid - vid_low;
		uint32_t intf_id = (vid_offset<<3)| (mac_index&0x7);
		
        memcpy(&arp_entry.ip4_addr, sip, 4);
        memcpy(arp_entry.mac_addr.uc, smac, 6);
		arp_entry.port_id = athr_header.sport;
		arp_entry.intf_id = intf_id;
        arp_entry.flags = FAL_IP_IP4_ADDR;
        arp_entry.status = ARP_AGE;
        
        arp_entry.counter_en = 1;
        arp_entry.counter_id = debug_counter;
        if(++debug_counter == 8) debug_counter = 0;
        
        int rv = isis_ip_host_add(0,&arp_entry);
        if(rv==0)
		    printk("##(port:%d)##isis_ip_host_add #%d##\n", athr_header.sport, rv);
    }
    return NF_ACCEPT;
}


static struct nf_sockopt_ops *ipt_sockopts = NULL;
static struct nf_sockopt_ops nat_sockopts = {
	.pf = PF_INET,
	.set_optmin = 31,
	.set_optmax = 32,
	.set = nat_set_ctl,
	.get_optmin = 31,
	.get_optmax = 33,
	.get = nat_get_ctl
};

static uint8_t *gbuffer, *sbuffer;
static unsigned int glen, slen;
#define NAT_NUM 32
#define IPT_BUFFER_INIT_LEN 1000
static struct ipt_replace old;

struct snat {
    uint32_t sip;
    uint16_t min, max;
    uint32_t tsip;
    uint8_t proto;
};

static struct snat s17_nat_map[NAT_NUM];

static void
nat_del(unsigned int newnum, unsigned int oldnum) {
    int i, j;
    struct ipt_entry *gentry = NULL;
    struct ipt_entry *sentry = NULL;
    struct ipt_entry_target *gtarget = NULL;
    struct ipt_entry_target *starget = NULL;
    struct ip_nat_multi_range_compat *grange = NULL;
    struct ip_nat_multi_range_compat *srange = NULL;
    struct ipt_multiport *xport = NULL;
    struct ipt_tcp *xtcp = NULL;
    struct ipt_udp *xudp = NULL;
    uint32_t iip = 0, pip = 0;     //internal publicd
    uint16_t port1 = 0, port2 = 0;
    uint8_t *gptr, *sptr;
    gptr = gbuffer;
    sptr = sbuffer;
    printk("delete one\n");
    for (i = oldnum; i >= 4; i--) {
        gentry = (struct ipt_entry *)gptr;
        sentry = (struct ipt_entry *)sptr;
        gtarget = (struct ipt_entry_target *)((uint8_t *) gentry + gentry->target_offset);
        starget = (struct ipt_entry_target *)((uint8_t *) sentry + sentry->target_offset);
        grange = (struct ip_nat_multi_range_compat *)((uint8_t *) gtarget + sizeof (*gtarget));
        srange = (struct ip_nat_multi_range_compat *)((uint8_t *) starget + sizeof (*starget));
        printk("(0)isis_nat_del name %s:%s#####(%x:%x %x)###\n",
               gtarget->u.user.name, starget->u.user.name,
               gentry->ip.src.s_addr, gentry->ip.dst.s_addr,
               grange->range[0].min.all);
        if (strcmp(gtarget->u.user.name, starget->u.user.name)) {
            if (!strcmp(gtarget->u.user.name, "DNAT")) {
                if (gentry->ip.src.s_addr || !gentry->ip.dst.s_addr
                    || grange->range[0].min.all)
                    return;
                goto delete;
            } else if (!strcmp(gtarget->u.user.name, "SNAT")) {
                if (!gentry->ip.src.s_addr || gentry->ip.dst.s_addr
                    || grange->range[0].min.all)
                    return;
                goto delete;
            }
            return;
        } else if (!strcmp(gtarget->u.user.name, "DNAT")) {
            if (memcmp(gentry, sentry, gentry->next_offset)) {
                if (gentry->ip.src.s_addr || !gentry->ip.dst.s_addr
                    || grange->range[0].min.all)
                    return;
                goto delete;
            }
        } else if (!strcmp(gtarget->u.user.name, "SNAT")) {
            if (memcmp(gentry, sentry, gentry->next_offset)) {
                if (!gentry->ip.src.s_addr || gentry->ip.dst.s_addr
                    || grange->range[0].min.all)
                    return;
                goto delete;
            }
        }
        gptr += gentry->next_offset;
        sptr += gentry->next_offset;
    }
    return;
  delete:
    printk("isis_nat_del OFFSET %d########\n", gentry->next_offset);
#if 0
    if (gentry->next_offset == 168) {
        //reg_write(0xe48,gentry->ip.src.s_addr);
        printk("isis_nat_del(1)########\n");
        fal_nat_entry_t nat_entry;
        memset((void *) &nat_entry, 0, sizeof (fal_nat_entry_t));
        nat_entry.entry_id = 0;
        isis_nat_del(0, 1, &nat_entry);
    } else
#endif
    if (gentry->next_offset == 216) {
        if (gentry->ip.proto == P_TCP)
            xtcp = (struct ipt_tcp *)((uint8_t *) gtarget - sizeof (*xtcp));
        else if (gentry->ip.proto == P_UDP)
            xudp = (struct ipt_udp *)((uint8_t *) gtarget - sizeof (*xudp) - 2);
    } else if (gentry->next_offset == 248) {
        xport = (struct ipt_multiport *)((uint8_t *) gtarget - sizeof (*xport) - 16);
        if (!xport->count || (xport->count > IPT_MULTI_PORTS))
            return;
    }
    if (grange->range[0].min.all)
        return;
    if (!strcmp(gtarget->u.user.name, "SNAT")) {
        if (!gentry->ip.src.s_addr || gentry->ip.dst.s_addr)
            return;
        if (gentry->next_offset == 216) {
            if (gentry->ip.proto == P_TCP) {
                if (xtcp->dpts[0] || !xtcp->spts[0])
                    return;
                port1 = xtcp->spts[0];
                port2 = xtcp->spts[1];
            } else if (gentry->ip.proto == P_UDP) {
                if (xudp->dpts[0] || !xudp->spts[0])
                    return;
                port1 = xudp->spts[0];
                port2 = xudp->spts[1];
            }
        }
        iip = gentry->ip.src.s_addr;
        pip = grange->range[0].min_ip;
    } else if (!strcmp(gtarget->u.user.name, "DNAT")) {
        if (gentry->ip.src.s_addr || !gentry->ip.dst.s_addr)
            return;
        if (gentry->next_offset == 216) {
            if (gentry->ip.proto == P_TCP) {
                printk("---------------[2]------%x-%x--------\n", xtcp->dpts[0],
                       xtcp->spts[0]);
                if (!xtcp->dpts[0] || xtcp->spts[0])
                    return;
                port1 = xtcp->dpts[0];
                port2 = xtcp->dpts[1];
            } else if (gentry->ip.proto == P_UDP) {
                if (!xudp->dpts[0] || xudp->spts[0])
                    return;
                port1 = xudp->dpts[0];
                port2 = xudp->dpts[1];
            }
        }
        iip = grange->range[0].min_ip;
        pip = gentry->ip.dst.s_addr;
    }
    memset((void *) &nat_entry, 0, sizeof (fal_nat_entry_t));
    
    nat_entry.src_addr = iip;
    nat_entry.trans_addr = pip;

    if (gentry->ip.proto == P_TCP) {
        nat_entry.flags = FAL_NAT_ENTRY_PROTOCOL_TCP;
    } else if (gentry->ip.proto == P_UDP) {
        nat_entry.flags = FAL_NAT_ENTRY_PROTOCOL_UDP;
    } else if (gentry->ip.proto == P_ANY) {
        nat_entry.flags = FAL_NAT_ENTRY_PROTOCOL_ANY;
    } else
        return;
    //port disable only use sip
    if (gentry->next_offset == 168) {
        for (j = 0; j < NAT_NUM; j++) {
            printk("isis_nat_del[%d] sip(%x:%x) tsip(%x:%x)##(%x %x)######\n",
                   j, iip, s17_nat_map[j].sip, pip, s17_nat_map[j].tsip,
                   s17_nat_map[j].min, s17_nat_map[j].max);
            if ((iip == s17_nat_map[j].sip) && (pip == s17_nat_map[j].tsip)
                && (0 == s17_nat_map[j].min) && (0 == s17_nat_map[j].max)
                && (nat_entry.flags == s17_nat_map[j].proto))
                break;
        }
        if (j == NAT_NUM)
            return;
        nat_entry.entry_id = j;
        printk("isis_nat_del(1) J=%d########\n", j);
        isis_nat_del(0, FAL_NAT_ENTRY_ID_EN, &nat_entry);
    } else if (gentry->next_offset == 216) {
        nat_entry.flags |= FAL_NAT_ENTRY_PORT_CHECK;
        //one port or port range
        nat_entry.port_num = port1;
        nat_entry.port_range = (port2 - port1 + 1);

        for (j = 0; j < NAT_NUM; j++) {
            if ((iip == s17_nat_map[j].sip) && (pip == s17_nat_map[j].tsip)
                && (port1 == s17_nat_map[j].min)
                && (port2 == s17_nat_map[j].max)
                && (nat_entry.flags == s17_nat_map[j].proto))
                break;
        }
        if (j == NAT_NUM)
            return;
        nat_entry.entry_id = j;
        printk("isis_nat_del(2) J=%d########\n", j);
        isis_nat_del(0, FAL_NAT_ENTRY_ID_EN, &nat_entry);
    } else if (gentry->next_offset == 248) {
        nat_entry.flags |= FAL_NAT_ENTRY_PORT_CHECK;
        //some discontinuous ports
        for (i = 0; i < xport->count; i++) {
            nat_entry.port_num = xport->ports[i];
            nat_entry.port_range = 1;

            for (j = 0; j < NAT_NUM; j++) {
                if ((iip == s17_nat_map[j].sip) && (pip == s17_nat_map[j].tsip)
                    && (xport->ports[i] == s17_nat_map[j].min)
                    && (xport->ports[i] == s17_nat_map[j].max)
                    && (nat_entry.flags == s17_nat_map[j].proto))
                    break;
            }

            if (j == NAT_NUM)
                return;
            nat_entry.entry_id = j;
            printk("isis_nat_del(3) J=%d########\n", j);
            isis_nat_del(0, FAL_NAT_ENTRY_ID_EN, &nat_entry);
        }
    }
}

static int
nat_set_ctl(struct sock *sk, int cmd, void __user * user, unsigned int len) {
    struct ipt_replace ireplace;
    struct ipt_entry *ientry = NULL;
    struct ipt_entry_target *xtarget = NULL;
    struct ip_nat_multi_range_compat *nfmrange = NULL;
    struct ipt_multiport *xport = NULL;
    struct ipt_tcp *xtcp = NULL;
    struct ipt_udp *xudp = NULL;
    uint8_t *smac, *dmac, *tsmac, *tdmac;
    
    uint32_t iip = 0, pip = 0;     //internal publicd
    uint16_t port1 = 0, port2 = 0;
    int i = 0, j = 0, k = 0;
    printk("NAT set hook\n");
    if (cmd != IPT_SO_SET_REPLACE)
        goto normal;
    copy_from_user(&ireplace, user, sizeof (ireplace));
    if (slen < ireplace.size) {
        kfree(sbuffer);
        slen = ireplace.size;
        sbuffer = kmalloc(slen, GFP_ATOMIC);
    }
    copy_from_user(sbuffer, user + sizeof (ireplace), ireplace.size);
    printk("ireplace:%s %x %d %d %d %x: %d %d %d %d %d : %d %d %d %d %d\n",
           ireplace.name, ireplace.valid_hooks, ireplace.num_entries,
           ireplace.size, ireplace.num_counters, ireplace.entries,
           ireplace.hook_entry[0], ireplace.hook_entry[1],
           ireplace.hook_entry[2], ireplace.hook_entry[3],
           ireplace.hook_entry[4], ireplace.underflow[0], ireplace.underflow[1],
           ireplace.underflow[2], ireplace.underflow[3], ireplace.underflow[4]);
    if (strcmp(ireplace.name, "nat")
        || (ireplace.num_entries == ireplace.num_counters)) {
        printk("none NAT or no new entry %d", ireplace.num_entries);
        goto normal;
    }
    if (ireplace.num_entries == 4) {    //new number is 4

        sw_error_t rv;
        uint32_t reg = 0x80000001;
		isis_reg_set(0, 0xe58, (uint8_t *) (&reg), 4);
        //HSL_REG_ENTRY_GEN_SET(rv, 0, 0xe58, 4, (uint8_t *) (&reg), 4);
        printk("---------------(flush napt done)--------------\n");

        reg = 0x80000021;
		isis_reg_set(0, 0xe58, (uint8_t *) (&reg), 4);
        //HSL_REG_ENTRY_GEN_SET(rv, 0, 0xe58, 4, (uint8_t *) (&reg), 4);
        printk("---------------(flush nat done)--------------\n");
        memset(s17_nat_map, 0, sizeof (struct snat) * NAT_NUM);
        goto normal;
    }
    if (ireplace.num_entries < ireplace.num_counters) {
        nat_del(ireplace.num_entries, ireplace.num_counters);
        goto normal;
    }
    k = ireplace.underflow[4] - old.underflow[4];
    if (old.underflow[0] == ireplace.underflow[0]) {        //snat
        ientry = (struct ipt_entry *)(sbuffer + old.underflow[4]);
        printk("---------------(add snat)--------------\n");
    } else {                    //dnat
        ientry = (struct ipt_entry *)
            (sbuffer + ireplace.hook_entry[4] - k - sizeof (struct ipt_standard));
        printk("---------------(add dnat)--------------\n");
    }
    if (k % 168 == 0) {
        j = 1;
    } else if (k % 216 == 0) {
        j = 2;
    } else if (k % 248 == 0) {
        j = 3;
    } else
        goto normal;
    printk("---------------protocol %d--------------\n", ientry->ip.proto);
    if ((ientry->ip.proto != P_TCP) && (ientry->ip.proto != P_UDP)
        && (ientry->ip.proto != P_ANY))
        goto normal;

    xtarget = (struct ipt_entry_target *)((uint8_t *) ientry + ientry->target_offset);
    nfmrange = (struct ip_nat_multi_range_compat *)((uint8_t *) xtarget + sizeof (*xtarget));
    smac = (uint8_t *)(&(ientry->ip.src.s_addr));
    dmac = (uint8_t *)(&(ientry->ip.dst.s_addr));
    if (j == 2) {               //gre need test
        if (ientry->ip.proto == P_TCP)
            xtcp = (struct ipt_tcp *)((uint8_t *) xtarget - sizeof (*xtcp));
        else if (ientry->ip.proto == P_UDP)
            xudp = (struct ipt_udp *)((uint8_t *) xtarget - sizeof (*xudp) - 2);
    }
    if (j == 3) {
        xport = (struct ipt_multiport *)((uint8_t *) xtarget - sizeof (*xport) - 16);
        if (!xport->count || (xport->count > IPT_MULTI_PORTS))
            goto normal;
    }
    if (nfmrange->range[0].min.all)
        goto normal;
    printk("---------------s_addr:%x-d_addr%x---------\n", ientry->ip.src.s_addr, ientry->ip.dst.s_addr);
    if (!strcmp(xtarget->u.user.name, "SNAT")) {
        if (!ientry->ip.src.s_addr || ientry->ip.dst.s_addr)
            goto normal;
        if (j == 2) {
            if (ientry->ip.proto == P_TCP) {
                if (xtcp->dpts[0] || !xtcp->spts[0])
                    goto normal;
                port1 = xtcp->spts[0];
                port2 = xtcp->spts[1];
            } else if (ientry->ip.proto == P_UDP) {
                if (xudp->dpts[0] || !xudp->spts[0])
                    goto normal;
                port1 = xudp->spts[0];
                port2 = xudp->spts[1];
            }
        }
        iip = ientry->ip.src.s_addr;
        pip = nfmrange->range[0].min_ip;
    } else if (!strcmp(xtarget->u.user.name, "DNAT")) {
        if (ientry->ip.src.s_addr || !ientry->ip.dst.s_addr)
            goto normal;
        if (j == 2) {
            if (ientry->ip.proto == P_TCP) {
                if (!xtcp->dpts[0] || xtcp->spts[0])
                    goto normal;
                port1 = xtcp->dpts[0];
                port2 = xtcp->dpts[1];
            } else if (ientry->ip.proto == P_UDP) {
                if (!xudp->dpts[0] || xudp->spts[0])
                    goto normal;
                port1 = xudp->dpts[0];
                port2 = xudp->dpts[1];
            }
        }
        iip = nfmrange->range[0].min_ip;
        pip = ientry->ip.dst.s_addr;
    }

    if (strcmp(xtarget->u.user.name, "SNAT")
        && strcmp(xtarget->u.user.name, "DNAT"))
        goto normal;
    memset((void *) &nat_entry, 0, sizeof (fal_nat_entry_t));
    
    nat_entry.counter_en = 1;
    nat_entry.counter_id = debug_counter;
    if(++debug_counter == 8) debug_counter = 0;
    
    nat_entry.src_addr = iip;
    nat_entry.trans_addr = pip;

    if (ientry->ip.proto == 6) {
        nat_entry.flags = FAL_NAT_ENTRY_PROTOCOL_TCP;
    } else if (ientry->ip.proto == 17) {
        nat_entry.flags = FAL_NAT_ENTRY_PROTOCOL_UDP;
    } else if (ientry->ip.proto == 0) {
        nat_entry.flags = FAL_NAT_ENTRY_PROTOCOL_ANY;
    }
    //port disable only use sip
    if (j == 1) {
        isis_nat_add(0, &nat_entry);
        printk("isis_nat_add(1) id:%x########\n", nat_entry.entry_id);
    } else if (j == 2) {
        nat_entry.flags |= FAL_NAT_ENTRY_PORT_CHECK;
        //one port or port range
        nat_entry.port_num = port1;
        nat_entry.port_range = (port2 - port1 + 1);
        isis_nat_add(0, &nat_entry);
        printk("isis_nat_add(2) id:%x########\n", nat_entry.entry_id);
        s17_nat_map[nat_entry.entry_id].min = port1;
        s17_nat_map[nat_entry.entry_id].max = port2;
    } else if (j == 3) {
        nat_entry.flags |= FAL_NAT_ENTRY_PORT_CHECK;
        //some discontinuous ports
        for (i = 0; i < xport->count; i++) {
            nat_entry.port_num = xport->ports[i];
            nat_entry.port_range = 1;
            isis_nat_add(0, &nat_entry);
            printk("isis_nat_add(3) id:%x########\n", nat_entry.entry_id);
            s17_nat_map[nat_entry.entry_id].sip = iip;
            s17_nat_map[nat_entry.entry_id].tsip = pip;
			s17_nat_map[nat_entry.entry_id].proto = nat_entry.flags;
            s17_nat_map[nat_entry.entry_id].min = xport->ports[i];
            s17_nat_map[nat_entry.entry_id].max = xport->ports[i];
        }
    }
	if(j != 3){
		s17_nat_map[nat_entry.entry_id].sip = iip;
    	s17_nat_map[nat_entry.entry_id].tsip = pip;
    	s17_nat_map[nat_entry.entry_id].proto = nat_entry.flags;
	}

    tsmac = (uint8_t *)(&nfmrange->range[0].min_ip);
    tdmac = (uint8_t *)(&nfmrange->range[0].max_ip);
    printk
        ("%s proto(%d) src %d.%d.%d.%d dst %d.%d.%d.%d :%d %x %d.%d.%d.%d %d.%d.%d.%d %d %d:%d %d %x %x %x",
         xtarget->u.user.name, ientry->ip.proto, smac[0], smac[1], smac[2],
         smac[3], dmac[0], dmac[1], dmac[2], dmac[3], nfmrange->rangesize,
         nfmrange->range[0].flags, tsmac[0], tsmac[1], tsmac[2], tsmac[3],
         tdmac[0], tdmac[1], tdmac[2], tdmac[3],
         ntohs(nfmrange->range[0].min.all), ntohs(nfmrange->range[0].max.all),
         ientry->target_offset, ientry->next_offset, ientry->comefrom,
         ientry->nfcache, ientry->elems);
    if (j == 1)
        printk("\n");
    else if (j == 2) {
        if (ientry->ip.proto == P_TCP)
            printk(": %d %d %d %d\n", xtcp->spts[0], xtcp->spts[1],
                   xtcp->dpts[0], xtcp->dpts[1]);
        else if (ientry->ip.proto == P_UDP)
            printk(": %d %d %d %d\n", xudp->spts[0], xudp->spts[1],
                   xudp->dpts[0], xudp->dpts[1]);
    } else if (j == 3) {
        printk(":");
        for (i = 0; i < xport->count; i++)
            printk(" %d", xport->ports[i]);
        printk("\n");
    }
#if 0
	uint8_t *tb;
    tb = (uint8_t *) sbuffer;
    for (i = 0, j = 0; i < ireplace.size; i++) {
        printk("%02x ", tb[i]);
        j++;
        if (j == 16)
            printk(" ");
        if (j == 32) {
            j = 0;
            printk("\n");
        }
    }
    printk("\n");
#endif
  normal:
    old = ireplace;
    return nat_sockopts.set(sk, cmd, user, len);
}

static int
nat_get_ctl(struct sock *sk, int cmd, void __user * user, int *len) {

    struct ipt_getinfo info;
    struct ipt_get_entries entries;

    int k = nat_sockopts.get(sk, cmd, user, len);
    if (cmd == IPT_SO_GET_INFO) {
        copy_from_user(&info, user, *len);
        printk("After %s %d %d %d : %d %d %d %d %d: %d %d %d %d %d\n",
               info.name, info.valid_hooks, info.num_entries, info.size,
               info.hook_entry[0], info.hook_entry[1], info.hook_entry[2],
               info.hook_entry[3], info.hook_entry[4], info.underflow[0],
               info.underflow[1], info.underflow[2], info.underflow[3],
               info.underflow[4]);
    }

    if (cmd == IPT_SO_GET_ENTRIES) {
        if (glen < (*len - sizeof (entries))) {
            kfree(gbuffer);
            glen = *len - sizeof (entries);
            gbuffer = kmalloc(glen, GFP_ATOMIC);
        }
        copy_from_user(&entries, user, sizeof (entries));
        copy_from_user(gbuffer, user + sizeof (entries),
                       *len - sizeof (entries));
        printk("IPT_SO_GET_ENTRIES: %s %d\n", entries.name, entries.size);
    }
    return k;
}

static struct nf_hook_ops outhook = {
	.hook = napt_handle,
	.owner = THIS_MODULE,
	.pf = PF_INET,.hooknum = NF_IP_POST_ROUTING,
	.priority = NF_IP_PRI_LAST,
};

static struct nf_hook_ops inhook = {
	.hook = napt_handle,
	.owner = THIS_MODULE,
	.pf = PF_INET,
	.hooknum = NF_IP_PRE_ROUTING,
	.priority = NF_IP_PRI_LAST,
};

static struct nf_hook_ops arpinhook = {
	.hook = arp_in,
	.hooknum = NF_ARP_IN,
	.owner = THIS_MODULE,
	.pf = NF_ARP,
	.priority = NF_IP_PRI_FILTER,
};

static void
nat_init(void) {
    //struct nf_sockopt_ops *ops;
    nf_register_sockopt(&nat_sockopts);
    list_for_each_entry(ipt_sockopts, nat_sockopts.list.next, list) {
        if (ipt_sockopts->set_optmin == IPT_BASE_CTL)
            break;
    }
    //ipt_sockopts = ops;
    nf_unregister_sockopt(&nat_sockopts);
    nat_sockopts = *ipt_sockopts;
    ipt_sockopts->set = nat_set_ctl;
    ipt_sockopts->get = nat_get_ctl;
    memset(&old, 0, sizeof (old));
    old.underflow[4] = sizeof (struct ipt_standard);
    memset(s17_nat_map, 0, sizeof (struct snat) * NAT_NUM);
    glen = IPT_BUFFER_INIT_LEN;
    slen = IPT_BUFFER_INIT_LEN;
    gbuffer = kmalloc(glen, GFP_ATOMIC);
    sbuffer = kmalloc(slen, GFP_ATOMIC);
    memset(gbuffer, 0, glen);
    memset(sbuffer, 0, slen);
}

static void
nat_exit(void) {
    ipt_sockopts->set = nat_sockopts.set;
    ipt_sockopts->get = nat_sockopts.get;
    if (gbuffer)
        kfree(gbuffer);
    if (sbuffer)
        kfree(sbuffer);
}

static int 
if_mac_entry_add(void)
{
	memset(&if_mac_entry, 0, sizeof(fal_intf_mac_entry_t));
	memcpy(if_mac_entry.mac_addr.uc, rt_int_mac, 6);

	if_mac_entry.entry_id = 0;
	if_mac_entry.ip4_route = 1;
    if_mac_entry.vid_low = 0;
    if_mac_entry.vid_high = 511;   
	isis_ip_intf_entry_add(0, &if_mac_entry);
	
	printk("isis_ip_intf_entry_add id[%d]\n", if_mac_entry.entry_id);
}

int
nat_ipt_init(a_uint32_t dev_id) 
{
    printk("Hello, nat module!\n");
	
	nf_register_hook(&outhook);
    nf_register_hook(&inhook);
    nf_register_hook(&arpinhook);
    nat_init();
#ifdef HEADER_CONFIG	
	napt_death_thread_create();
#endif
    if_mac_entry_add();
    return 0;
}

int
nat_ipt_cleanup(a_uint32_t dev_id)
{
    printk("Goodbye, nat module!\n");
#ifdef HEADER_CONFIG	
	napt_death_thread_destory();

	while(thread_pid) {
		if(napt_pid_exit(thread_pid))
			break;
		schedule_timeout_interruptible(HZ);
	}
#endif

    nf_unregister_hook(&outhook);
    nf_unregister_hook(&inhook);
    nf_unregister_hook(&arpinhook);

    nat_exit();
    return 0;
}

