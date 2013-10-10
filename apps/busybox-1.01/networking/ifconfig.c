/* ifconfig
 *
 * Similar to the standard Unix ifconfig, but with only the necessary
 * parts for AF_INET, and without any printing of if info (for now).
 *
 * Bjorn Wesen, Axis Communications AB
 *
 *
 * Authors of the original ifconfig was:
 *              Fred N. van Kempen, <waltje@uwalt.nl.mugnet.org>
 *
 * This program is free software; you can redistribute it
 * and/or  modify it under  the terms of  the GNU General
 * Public  License as  published  by  the  Free  Software
 * Foundation;  either  version 2 of the License, or  (at
 * your option) any later version.
 *
 * $Id: //depot/sw/releases/Aquila_9.2.0_U11/apps/busybox-1.01/networking/ifconfig.c#1 $
 *
 */

/*
 * Heavily modified by Manuel Novoa III       Mar 6, 2001
 *
 * From initial port to busybox, removed most of the redundancy by
 * converting to a table-driven approach.  Added several (optional)
 * args missing from initial port.
 *
 * Still missing:  media, tunnel.
 *
 * 2002-04-20
 * IPV6 support added by Bart Visscher <magick@linux-fan.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>		/* strcmp and friends */
#include <ctype.h>		/* isdigit and friends */
#include <stddef.h>		/* offsetof */
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#if __GLIBC__ >=2 && __GLIBC_MINOR >= 1
#include <netpacket/packet.h>
#include <net/ethernet.h>
#else
#include <sys/types.h>
#include <netinet/if_ether.h>
#endif
#include "inet_common.h"
#include "busybox.h"

#ifdef CONFIG_FEATURE_IFCONFIG_SLIP
# include <net/if_slip.h>
#endif

/* I don't know if this is needed for busybox or not.  Anyone? */
#define QUESTIONABLE_ALIAS_CASE


/* Defines for glibc2.0 users. */
#ifndef SIOCSIFTXQLEN
# define SIOCSIFTXQLEN      0x8943
# define SIOCGIFTXQLEN      0x8942
#endif

/* ifr_qlen is ifru_ivalue, but it isn't present in 2.0 kernel headers */
#ifndef ifr_qlen
# define ifr_qlen        ifr_ifru.ifru_mtu
#endif

#ifndef IFF_DYNAMIC
# define IFF_DYNAMIC     0x8000	/* dialup device with changing addresses */
#endif

#ifdef CONFIG_FEATURE_IPV6
struct in6_ifreq {
	struct in6_addr ifr6_addr;
	uint32_t ifr6_prefixlen;
	int ifr6_ifindex;
};
#endif

/*
 * Here are the bit masks for the "flags" member of struct options below.
 * N_ signifies no arg prefix; M_ signifies arg prefixed by '-'.
 * CLR clears the flag; SET sets the flag; ARG signifies (optional) arg.
 */
#define N_CLR            0x01
#define M_CLR            0x02
#define N_SET            0x04
#define M_SET            0x08
#define N_ARG            0x10
#define M_ARG            0x20

#define M_MASK           (M_CLR | M_SET | M_ARG)
#define N_MASK           (N_CLR | N_SET | N_ARG)
#define SET_MASK         (N_SET | M_SET)
#define CLR_MASK         (N_CLR | M_CLR)
#define SET_CLR_MASK     (SET_MASK | CLR_MASK)
#define ARG_MASK         (M_ARG | N_ARG)

/*
 * Here are the bit masks for the "arg_flags" member of struct options below.
 */

/*
 * cast type:
 *   00 int
 *   01 char *
 *   02 HOST_COPY in_ether
 *   03 HOST_COPY INET_resolve
 */
#define A_CAST_TYPE      0x03
/*
 * map type:
 *   00 not a map type (mem_start, io_addr, irq)
 *   04 memstart (unsigned long)
 *   08 io_addr  (unsigned short)
 *   0C irq      (unsigned char)
 */
#define A_MAP_TYPE       0x0C
#define A_ARG_REQ        0x10	/* Set if an arg is required. */
#define A_NETMASK        0x20	/* Set if netmask (check for multiple sets). */
#define A_SET_AFTER      0x40	/* Set a flag at the end. */
#define A_COLON_CHK      0x80	/* Is this needed?  See below. */
#ifdef CONFIG_FEATURE_IFCONFIG_BROADCAST_PLUS
#define A_HOSTNAME      0x100	/* Set if it is ip addr. */
#define A_BROADCAST     0x200	/* Set if it is broadcast addr. */
#else
#define A_HOSTNAME          0
#define A_BROADCAST         0
#endif

/*
 * These defines are for dealing with the A_CAST_TYPE field.
 */
#define A_CAST_CHAR_PTR  0x01
#define A_CAST_RESOLVE   0x01
#define A_CAST_HOST_COPY 0x02
#define A_CAST_HOST_COPY_IN_ETHER    A_CAST_HOST_COPY
#define A_CAST_HOST_COPY_RESOLVE     (A_CAST_HOST_COPY | A_CAST_RESOLVE)

/*
 * These defines are for dealing with the A_MAP_TYPE field.
 */
#define A_MAP_ULONG      0x04	/* memstart */
#define A_MAP_USHORT     0x08	/* io_addr */
#define A_MAP_UCHAR      0x0C	/* irq */

/*
 * Define the bit masks signifying which operations to perform for each arg.
 */
#ifdef CONFIG_FEATURE_IFCONFIG_VLAN_IGMP
#define ARG_PORT         (A_ARG_REQ | A_MAP_ULONG)

typedef struct {
    u_int8_t uc[6];
} mac_addr_t;

struct arl_struct {
	mac_addr_t mac_addr;
	int port_map;
	int sa_drop; 
};

#endif

#ifdef CONFIG_FEATURE_IFCONFIG_S26QOS
#define ETH_SOFT_CLASS   (SIOCDEVPRIVATE | 0x4)
#define ETH_PORT_QOS     (SIOCDEVPRIVATE | 0x5)
#define ETH_VLAN_QOS     (SIOCDEVPRIVATE | 0x6)
#define ETH_DA_QOS       (SIOCDEVPRIVATE | 0x7)
#define ETH_IP_QOS		 (SIOCDEVPRIVATE | 0x8)
#define ETH_PORT_ILIMIT  (SIOCDEVPRIVATE | 0x9)
#define ETH_PORT_ELIMIT  (SIOCDEVPRIVATE | 0xa)
#define ETH_PORT_EQLIMIT (SIOCDEVPRIVATE | 0xb)
struct qosoption {
	const char *name;
	const unsigned int selector;
	const unsigned int paranum;
};

static const struct qosoption qosOptArray[] = {
	{"enable", ETH_SOFT_CLASS, 1},
	{"disable", ETH_SOFT_CLASS, 1},
	{"portqos", ETH_PORT_QOS, 3},
	{"vlanqos", ETH_VLAN_QOS, 3},
	{"ipqos", ETH_IP_QOS, 1},
    {"macqos", ETH_DA_QOS, 4},
    {"portin", ETH_PORT_ILIMIT, 3},
    {"portout", ETH_PORT_ELIMIT, 3},
    {"portqout",ETH_PORT_EQLIMIT, 4},
	{NULL, 0}
};
#endif
struct eth_cfg_params {
    uint16_t cmd;
    char    ad_name[IFNAMSIZ];      /* if name, e.g. "eth0" */
	uint16_t vlanid;
    uint16_t portnum;           /* pack to fit, yech */
    uint32_t phy_reg;
	uint32_t tos;
    uint32_t val;
    uint8_t duplex;
    uint8_t  mac_addr[6];
};


#define ARG_METRIC       (A_ARG_REQ /*| A_CAST_INT*/)
#define ARG_MTU          (A_ARG_REQ /*| A_CAST_INT*/)
#define ARG_TXQUEUELEN   (A_ARG_REQ /*| A_CAST_INT*/)
#define ARG_MEM_START    (A_ARG_REQ | A_MAP_ULONG)
#define ARG_IO_ADDR      (A_ARG_REQ | A_MAP_ULONG)
#define ARG_IRQ          (A_ARG_REQ | A_MAP_UCHAR)
#define ARG_DSTADDR      (A_ARG_REQ | A_CAST_HOST_COPY_RESOLVE)
#define ARG_NETMASK      (A_ARG_REQ | A_CAST_HOST_COPY_RESOLVE | A_NETMASK)
#define ARG_BROADCAST    (A_ARG_REQ | A_CAST_HOST_COPY_RESOLVE | A_SET_AFTER | A_BROADCAST)
#define ARG_HW           (A_ARG_REQ | A_CAST_HOST_COPY_IN_ETHER)
#define ARG_POINTOPOINT  (A_ARG_REQ | A_CAST_HOST_COPY_RESOLVE | A_SET_AFTER)
#define ARG_KEEPALIVE    (A_ARG_REQ | A_CAST_CHAR_PTR)
#define ARG_OUTFILL      (A_ARG_REQ | A_CAST_CHAR_PTR)
#define ARG_HOSTNAME     (A_CAST_HOST_COPY_RESOLVE | A_SET_AFTER | A_COLON_CHK | A_HOSTNAME)
#define ARG_ADD_DEL      (A_CAST_HOST_COPY_RESOLVE | A_SET_AFTER)


/*
 * Set up the tables.  Warning!  They must have corresponding order!
 */

struct arg1opt {
	const char *name;
	int selector;
	unsigned short ifr_offset;
};

struct options {
	const char *name;
#ifdef CONFIG_FEATURE_IFCONFIG_BROADCAST_PLUS
	const unsigned int flags:6;
	const unsigned int arg_flags:10;
#else
	const unsigned char flags;
	const unsigned char arg_flags;
#endif
	const unsigned short selector;
};

#define ifreq_offsetof(x)  offsetof(struct ifreq, x)

static const struct arg1opt Arg1Opt[] = {
	{"SIOCSIFMETRIC",  SIOCSIFMETRIC,  ifreq_offsetof(ifr_metric)},
	{"SIOCSIFMTU",     SIOCSIFMTU,     ifreq_offsetof(ifr_mtu)},
	{"SIOCSIFTXQLEN",  SIOCSIFTXQLEN,  ifreq_offsetof(ifr_qlen)},
	{"SIOCSIFDSTADDR", SIOCSIFDSTADDR, ifreq_offsetof(ifr_dstaddr)},
	{"SIOCSIFNETMASK", SIOCSIFNETMASK, ifreq_offsetof(ifr_netmask)},
	{"SIOCSIFBRDADDR", SIOCSIFBRDADDR, ifreq_offsetof(ifr_broadaddr)},
#ifdef CONFIG_FEATURE_IFCONFIG_HW
	{"SIOCSIFHWADDR",  SIOCSIFHWADDR,  ifreq_offsetof(ifr_hwaddr)},
#endif
	{"SIOCSIFDSTADDR", SIOCSIFDSTADDR, ifreq_offsetof(ifr_dstaddr)},
#ifdef SIOCSKEEPALIVE
	{"SIOCSKEEPALIVE", SIOCSKEEPALIVE, ifreq_offsetof(ifr_data)},
#endif
#ifdef SIOCSOUTFILL
	{"SIOCSOUTFILL",   SIOCSOUTFILL,   ifreq_offsetof(ifr_data)},
#endif
#ifdef CONFIG_FEATURE_IFCONFIG_MEMSTART_IOADDR_IRQ
	{"SIOCSIFMAP",     SIOCSIFMAP,     ifreq_offsetof(ifr_map.mem_start)},
	{"SIOCSIFMAP",     SIOCSIFMAP,     ifreq_offsetof(ifr_map.base_addr)},
	{"SIOCSIFMAP",     SIOCSIFMAP,     ifreq_offsetof(ifr_map.irq)},
#endif
	/* Last entry if for unmatched (possibly hostname) arg. */
#ifdef CONFIG_FEATURE_IPV6
	{"SIOCSIFADDR",    SIOCSIFADDR,    ifreq_offsetof(ifr_addr)}, /* IPv6 version ignores the offset */
	{"SIOCDIFADDR",    SIOCDIFADDR,    ifreq_offsetof(ifr_addr)}, /* IPv6 version ignores the offset */
#endif
#ifdef CONFIG_FEATURE_IFCONFIG_S26QOS
	{"S26_QOS_CTL",    (SIOCDEVPRIVATE|0x9),    ifreq_offsetof(ifr_data)},
#endif
#ifdef CONFIG_FEATURE_IFCONFIG_VLAN_IGMP
	{"S26_VLAN_ADDPORTS",    (SIOCDEVPRIVATE|0x4),    ifreq_offsetof(ifr_metric)},
	{"S26_VLAN_DELPORTS",    (SIOCDEVPRIVATE|0x5),    ifreq_offsetof(ifr_metric)},
	{"S26_VLAN_SETTAGMODE",    (SIOCDEVPRIVATE|0x6),    ifreq_offsetof(ifr_metric)},
	{"S26_VLAN_SETDEFAULTID",    (SIOCDEVPRIVATE|0x7),    ifreq_offsetof(ifr_metric)},
	{"S26_IGMP_ON_OFF",     (SIOCDEVPRIVATE|0x8),    ifreq_offsetof(ifr_data)},
	{"S26_LINK_GETSTAT",    (SIOCDEVPRIVATE|0xA),    ifreq_offsetof(ifr_data)},
	{"S26_VLAN_ENABLE",    (SIOCDEVPRIVATE|0xB),    ifreq_offsetof(ifr_data)},
	{"S26_VLAN_DISABLE",    (SIOCDEVPRIVATE|0xC),    ifreq_offsetof(ifr_data)},
	{"S26_ARL_ADD",    (SIOCDEVPRIVATE|0xD),    ifreq_offsetof(ifr_data)},
	{"S26_ARL_DEL",    (SIOCDEVPRIVATE|0xE),    ifreq_offsetof(ifr_data)},
	{"S26_MCAST_CLR",    (SIOCDEVPRIVATE|0xF),    ifreq_offsetof(ifr_data)},
	{"S26_PACKET_FLAG",    (SIOCDEVPRIVATE|0x0),    ifreq_offsetof(ifr_data)},
#endif
	{"SIOCSIFADDR",    SIOCSIFADDR,    ifreq_offsetof(ifr_addr)},

};

static const struct options OptArray[] = {
	{"metric",      N_ARG,         ARG_METRIC,      0},
	{"mtu",         N_ARG,         ARG_MTU,         0},
	{"txqueuelen",  N_ARG,         ARG_TXQUEUELEN,  0},
	{"dstaddr",     N_ARG,         ARG_DSTADDR,     0},
	{"netmask",     N_ARG,         ARG_NETMASK,     0},
	{"broadcast",   N_ARG | M_CLR, ARG_BROADCAST,   IFF_BROADCAST},
#ifdef CONFIG_FEATURE_IFCONFIG_HW
	{"hw",          N_ARG, ARG_HW,                  0},
#endif
	{"pointopoint", N_ARG | M_CLR, ARG_POINTOPOINT, IFF_POINTOPOINT},
#ifdef SIOCSKEEPALIVE
	{"keepalive",   N_ARG,         ARG_KEEPALIVE,   0},
#endif
#ifdef SIOCSOUTFILL
	{"outfill",     N_ARG,         ARG_OUTFILL,     0},
#endif
#ifdef CONFIG_FEATURE_IFCONFIG_MEMSTART_IOADDR_IRQ
	{"mem_start",   N_ARG,         ARG_MEM_START,   0},
	{"io_addr",     N_ARG,         ARG_IO_ADDR,     0},
	{"irq",         N_ARG,         ARG_IRQ,         0},
#endif
#ifdef CONFIG_FEATURE_IPV6
	{"add",         N_ARG,         ARG_ADD_DEL,     0},
	{"del",         N_ARG,         ARG_ADD_DEL,     0},
#endif
#ifdef CONFIG_FEATURE_IFCONFIG_S26QOS
	{"s26qos",     N_ARG,         0,            0},
#endif
#ifdef CONFIG_FEATURE_IFCONFIG_VLAN_IGMP
	{"addports",     N_ARG,         ARG_PORT,               0},
	{"delports",     N_ARG,         ARG_PORT,               0},
	{"setmode",     N_ARG,         ARG_PORT,               0},
	{"setid",     N_ARG,         ARG_PORT,               0},
	{"igmpon",      N_ARG,         0,           0},
	{"getstat",     N_ARG,         0,            0},
	{"startvlan",     N_ARG,         0,            0},
	{"stopvlan",     N_ARG,         0,            0},
	{"addarl",     N_ARG,         0,            0},
	{"delarl",     N_ARG,         0,            0},
	{"setcast",     N_ARG,         0,            0},
	{"setpacketflag", N_ARG,         0,            0},
#endif
	{"arp",         N_CLR | M_SET, 0,               IFF_NOARP},
	{"trailers",    N_CLR | M_SET, 0,               IFF_NOTRAILERS},
	{"promisc",     N_SET | M_CLR, 0,               IFF_PROMISC},
	{"multicast",   N_SET | M_CLR, 0,               IFF_MULTICAST},
	{"allmulti",    N_SET | M_CLR, 0,               IFF_ALLMULTI},
	{"dynamic",     N_SET | M_CLR, 0,               IFF_DYNAMIC},
	{"up",          N_SET,         0,               (IFF_UP | IFF_RUNNING)},
	{"down",        N_CLR,         0,               IFF_UP},
	{NULL,          0,             ARG_HOSTNAME,    (IFF_UP | IFF_RUNNING)}
};

/*
 * A couple of prototypes.
 */

#ifdef CONFIG_FEATURE_IFCONFIG_HW
static int in_ether(char *bufp, struct sockaddr *sap);
#endif

#ifdef CONFIG_FEATURE_IFCONFIG_STATUS
extern int interface_opt_a;
extern int display_interfaces(char *ifname);
#endif

/*
 * Our main function.
 */

int ifconfig_main(int argc, char **argv)
{
	struct ifreq ifr;
	struct sockaddr_in sai;
#ifdef CONFIG_FEATURE_IPV6
	struct sockaddr_in6 sai6;
#endif
#ifdef CONFIG_FEATURE_IFCONFIG_HW
	struct sockaddr sa;
#endif
	const struct arg1opt *a1op;
	const struct options *op;
	int sockfd;			/* socket fd we use to manipulate stuff with */
	int goterr;
	int selector;
#ifdef CONFIG_FEATURE_IFCONFIG_BROADCAST_PLUS
	unsigned int mask;
	unsigned int did_flags;
	unsigned int sai_hostname, sai_netmask;
#else
	unsigned char mask;
	unsigned char did_flags;
#endif
	char *p;
	char host[128];

	goterr = 0;
	did_flags = 0;
#ifdef CONFIG_FEATURE_IFCONFIG_BROADCAST_PLUS
	sai_hostname = 0;
	sai_netmask = 0;
#endif

	/* skip argv[0] */
	++argv;
	--argc;

#ifdef CONFIG_FEATURE_IFCONFIG_STATUS
	if ((argc > 0) && (((*argv)[0] == '-') && ((*argv)[1] == 'a') && !(*argv)[2])) {
		interface_opt_a = 1;
		--argc;
		++argv;
	}
#endif

	if (argc <= 1) {
#ifdef CONFIG_FEATURE_IFCONFIG_STATUS
		return display_interfaces(argc ? *argv : NULL);
#else
		bb_error_msg_and_die
			("ifconfig was not compiled with interface status display support.");
#endif
	}

	/* Create a channel to the NET kernel. */
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		bb_perror_msg_and_die("socket");
	}

	/* get interface name */
	safe_strncpy(ifr.ifr_name, *argv, IFNAMSIZ);

	/* Process the remaining arguments. */
	while (*++argv != (char *) NULL) {
		p = *argv;
		mask = N_MASK;
		if (*p == '-') {	/* If the arg starts with '-'... */
			++p;		/*    advance past it and */
			mask = M_MASK;	/*    set the appropriate mask. */
		}
		for (op = OptArray; op->name; op++) {	/* Find table entry. */
			if (strcmp(p, op->name) == 0) {	/* If name matches... */
				if ((mask &= op->flags)) {	/* set the mask and go. */
					goto FOUND_ARG;;
				}
				/* If we get here, there was a valid arg with an */
				/* invalid '-' prefix. */
				++goterr;
				goto LOOP;
			}
		}

		/* We fell through, so treat as possible hostname. */
		a1op = Arg1Opt + (sizeof(Arg1Opt) / sizeof(Arg1Opt[0])) - 1;
		mask = op->arg_flags;
		goto HOSTNAME;

	  FOUND_ARG:
		if (mask & ARG_MASK) 
			{
			mask = op->arg_flags;
			a1op = Arg1Opt + (op - OptArray);
			if (mask & A_NETMASK & did_flags) {
				bb_show_usage();
			}
#ifdef CONFIG_FEATURE_IFCONFIG_VLAN_IGMP
			struct arl_struct arl;
			// Note: the argv has been added by 2; the argc has been substrated by one.
			if(!strcmp("igmpon",op->name)||!strcmp("igmpoff",op->name))
			{
				if(argc!=4) goto VLAN_ERR;
				unsigned int i = strtoul(argv[1], NULL, 0);
				unsigned int j = strtoul(argv[2], NULL, 0);
				ifr.ifr_ifru.ifru_ivalue = i&0x1f;
				if(j != 0)
					j = 1;
				ifr.ifr_ifru.ifru_ivalue |= j << 7;

				//printf("[%s] vlanid %x.\n",op->name,ifr.ifr_ifru.ifru_ivalue);
				if (ioctl(sockfd, a1op->selector, &ifr) < 0) {
					perror(a1op->name);
					++goterr;
					return goterr;
				}
				return 0;

			}

			// bit0-->port0,bit1-->port1,so it can be added or deleted many ports at the same time.
			if(!strcmp("addports",op->name)||!strcmp("delports",op->name)||!strcmp("setid",op->name))
			{
				if(argc!=4) goto VLAN_ERR;
				unsigned int data1 = strtoul(argv[1], NULL, 0);
				unsigned int data2 = strtoul(argv[2], NULL, 0);

				data1 &=0xfff; //vlan-id
				data2 &=0x1f;  //ports

				ifr.ifr_ifru.ifru_ivalue = (data1 << 16) | (data2);

				//printf("[%s] vlanid-ports %x.\n",op->name,ifr.ifr_ifru.ifru_ivalue);
				if (ioctl(sockfd, a1op->selector, &ifr) < 0) {
					perror(a1op->name);
					++goterr;
					return goterr;
				}
				return 0;

			}

			if(!strcmp("startvlan",op->name)||!strcmp("stopvlan",op->name))
			{
				if(argc!=2) goto VLAN_ERR;

				//No arguments needed.
				if (ioctl(sockfd, a1op->selector, &ifr) < 0) {
					perror(a1op->name);
					++goterr;
					return goterr;
				}
				return 0;

			}

			if(!strcmp("setmode",op->name))
			{

				if(argc!=4) goto VLAN_ERR;
				unsigned int data1 = strtoul(argv[1], NULL, 0);
				unsigned int data2 = strtoul(argv[2], NULL, 0);

				data1 &=0xf;   //mode:0-2 (0->unmodified,1->untagged,2->tagged)
				data2 &=0x7;   //port:0-5 (cpu:port0)

				ifr.ifr_ifru.ifru_ivalue = (data1 << 16) | (data2);

				//printf("[%s] vlanid-ports %x.\n",op->name,ifr.ifr_ifru.ifru_ivalue);
				if (ioctl(sockfd, a1op->selector, &ifr) < 0) {
					perror(a1op->name);
					++goterr;
					return goterr;
				}
				return 0;

			}

			if(!strcmp("getstat",op->name))
			{
				if(argc!=3) goto VLAN_ERR;
				unsigned int data1 = strtoul(argv[1], NULL, 0);

				data1 &=0x7;  //port: 1-5
				
				ifr.ifr_ifru.ifru_ivalue = data1;

				//printf("[%s] port: %x.\n",op->name,ifr.ifr_ifru.ifru_ivalue);
				if (ioctl(sockfd, a1op->selector, &ifr) < 0) {
					perror(a1op->name);
					++goterr;
					return goterr;
				}
				printf("phy port:%d--status %d.\n",data1,ifr.ifr_ifru.ifru_ivalue);//1--->up,0---->down
				return 0;

			}

			if(!strcmp("setcast",op->name))
			{
				if(argc!=3) goto VLAN_ERR;
				unsigned int data1 = strtoul(argv[1], NULL, 0);

				data1 &=0x7;  //port: 1-5
				
				ifr.ifr_ifru.ifru_ivalue = data1;

				if (ioctl(sockfd, a1op->selector, &ifr) < 0) {
					perror(a1op->name);
					++goterr;
					return goterr;
				}

				//1--->enable,0---->disable
				printf("Enable or disable unknow multicast packets over vlans %d.\n",data1);
				return 0;

			}

			if(!strcmp("addarl",op->name))
			{

				if(argc!=5) goto VLAN_ERR;

				safe_strncpy(host, argv[1], (sizeof host));

				if (in_ether(host, &sa)) {
					bb_error_msg("invalid hw-addr %s", host);
					++goterr;
					continue;
				}

				memcpy(&arl.mac_addr,&sa.sa_data,6);
				arl.port_map = strtoul(argv[2], NULL, 0);
				arl.sa_drop = strtoul(argv[3], NULL, 0);


				memcpy(&ifr.ifr_ifru.ifru_mtu,&arl,sizeof(arl));

				//printf("[%s] vlanid-ports %x.\n",op->name,ifr.ifr_ifru.ifru_ivalue);
				if (ioctl(sockfd, a1op->selector, &ifr) < 0) {
					perror(a1op->name);
					++goterr;
					return goterr;
				}
				return 0;

			}

			if(!strcmp("delarl",op->name))
			{

				if(argc!=3) goto VLAN_ERR;

				safe_strncpy(host, argv[1], (sizeof host));

				if (in_ether(host, &sa)) {
					bb_error_msg("invalid hw-addr %s", host);
					++goterr;
					continue;
				}

				memcpy(&arl.mac_addr,&sa.sa_data,6);
				memcpy(&ifr.ifr_ifru.ifru_mtu,&arl,sizeof(arl));

				//printf("[%s] vlanid-ports %x.\n",op->name,ifr.ifr_ifru.ifru_ivalue);
				if (ioctl(sockfd, a1op->selector, &ifr) < 0) {
					perror(a1op->name);
					++goterr;
					return goterr;
				}
				return 0;

			}
#endif
#ifdef CONFIG_FEATURE_IFCONFIG_S26QOS
			if(!strcmp("s26qos",op->name))
			{
				if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
					printf("ioctl sockfd error.\n");
					return 0;
				}
				if((ifr.ifr_flags&IFF_UP) == 0){
					printf("Please up the %s first.\n", ifr.ifr_name);
					return 0;
				}
				--argc;
				if(argc < 2) goto QOS_ERR;
				const struct qosoption *qosop;
				unsigned int data0, data1;
				struct eth_cfg_params qos_para;
				++argv;
				--argc;
		        for (qosop = qosOptArray; qosop->name; qosop++) {   /* Find table entry. */
        		    if (strcmp(*argv, qosop->name) == 0) { /* If name matches... */
           				break; 
					}
       			 }
				if((qosop->name == NULL) || ((argc != qosop->paranum)&&(qosop->selector != ETH_IP_QOS)))  
					goto QOS_ERR;
				memset((unsigned char *)&qos_para, 0x0, sizeof(struct eth_cfg_params));
				switch(qosop->selector){
					case ETH_SOFT_CLASS:
						if (strcmp("enable", qosop->name) == 0)
							qos_para.val = 1;
						else
							qos_para.val = 0;
						break;
					case ETH_PORT_QOS:
						qos_para.portnum = strtoul(argv[1], NULL, 0);
						qos_para.val = strtoul(argv[2], NULL, 0);
						break;
					case ETH_VLAN_QOS:
						qos_para.vlanid = strtoul(argv[1], NULL, 0);
						qos_para.val = strtoul(argv[2], NULL, 0);
						break;
					case ETH_IP_QOS:
						if(argc == 3){
							qos_para.tos = strtoul(argv[1], NULL, 16);
							qos_para.val = strtoul(argv[2], NULL, 0);
						}
						break;
					case ETH_DA_QOS:
						{
							int i,j,k;
							char tmp[3];
							tmp[2] = '\0';
							k = 0;
							for(i=0; i<6; i++){
								for(j=0; j<2; j++)
									tmp[j]=argv[1][k++];
								qos_para.mac_addr[i] = strtoul(tmp, NULL, 16);
								k++;
							}
				//			printf("%x:%x:%x:%x:%x:%x\n", qos_para.mac_addr[0],qos_para.mac_addr[1],
				//				qos_para.mac_addr[2],qos_para.mac_addr[3],qos_para.mac_addr[4],qos_para.mac_addr[5]);
							qos_para.val = strtoul(argv[2], NULL, 0);
							qos_para.portnum=strtoul(argv[3],NULL,0);
						}
						break;
                    case ETH_PORT_ILIMIT:
                    case ETH_PORT_ELIMIT:
                        {
                            qos_para.portnum = strtoul(argv[1], NULL, 0);
                            qos_para.val = strtoul(argv[2], NULL, 16);
                        }
                        break;
                    case ETH_PORT_EQLIMIT:
                        {
                            qos_para.portnum = strtoul(argv[1], NULL, 0);
                            qos_para.phy_reg = strtoul(argv[2], NULL, 0);
                            qos_para.val = strtoul(argv[3], NULL, 16);
                        }
                        break;
				}

				strcpy(qos_para.ad_name, ifr.ifr_name);
				qos_para.cmd = qosop->selector;
				
				ifr.ifr_ifru.ifru_data = &qos_para;
				if (ioctl(sockfd, a1op->selector, &ifr) < 0) {
					perror(a1op->name);
					++goterr;
					return goterr;
				}
				return 0;

			}
#endif
#ifdef CONFIG_FEATURE_IFCONFIG_VLAN_IGMP
			if(!strcmp("setpacketflag",op->name))
			{
				if(argc!=3) goto VLAN_ERR;
				unsigned int data1 = strtoul(argv[1], NULL, 0);

				ifr.ifr_ifru.ifru_ivalue = data1;

				//printf("[%s] port: %x.\n",op->name,ifr.ifr_ifru.ifru_ivalue);
				if (ioctl(sockfd, a1op->selector, &ifr) < 0) {
					perror(a1op->name);
					++goterr;
					return goterr;
				}
				printf("Set  packet ignoring flag in eth1 to %d.\n",data1);//1--->up,0---->down
				return 0;

			}

#endif
			if (*++argv == NULL) {
				if (mask & A_ARG_REQ) {
					bb_show_usage();
				} else {
					--argv;
					mask &= A_SET_AFTER;	/* just for broadcast */
				}
			} else {	/* got an arg so process it */
			  HOSTNAME:
				did_flags |= (mask & (A_NETMASK|A_HOSTNAME));
				if (mask & A_CAST_HOST_COPY) {
#ifdef CONFIG_FEATURE_IFCONFIG_HW
					if (mask & A_CAST_RESOLVE) {
#endif
#ifdef CONFIG_FEATURE_IPV6
						char *prefix;
						int prefix_len = 0;
#endif

						safe_strncpy(host, *argv, (sizeof host));
#ifdef CONFIG_FEATURE_IPV6
						if ((prefix = strchr(host, '/'))) {
							if (safe_strtoi(prefix + 1, &prefix_len) ||
								(prefix_len < 0) || (prefix_len > 128))
							{
								++goterr;
								goto LOOP;
							}
							*prefix = 0;
						}
#endif

						sai.sin_family = AF_INET;
						sai.sin_port = 0;
						if (!strcmp(host, bb_INET_default)) {
							/* Default is special, meaning 0.0.0.0. */
							sai.sin_addr.s_addr = INADDR_ANY;
#ifdef CONFIG_FEATURE_IFCONFIG_BROADCAST_PLUS
						} else if (((host[0] == '+') && !host[1]) && (mask & A_BROADCAST) &&
								   (did_flags & (A_NETMASK|A_HOSTNAME)) == (A_NETMASK|A_HOSTNAME)) {
							/* + is special, meaning broadcast is derived. */
							sai.sin_addr.s_addr = (~sai_netmask) | (sai_hostname & sai_netmask);
#endif
#ifdef CONFIG_FEATURE_IPV6
						} else if (inet_pton(AF_INET6, host, &sai6.sin6_addr) > 0) {
							int sockfd6;
							struct in6_ifreq ifr6;

							memcpy((char *) &ifr6.ifr6_addr,
								   (char *) &sai6.sin6_addr,
								   sizeof(struct in6_addr));

							/* Create a channel to the NET kernel. */
							if ((sockfd6 = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
								bb_perror_msg_and_die("socket6");
							}
							if (ioctl(sockfd6, SIOGIFINDEX, &ifr) < 0) {
								perror("SIOGIFINDEX");
								++goterr;
								continue;
							}
							ifr6.ifr6_ifindex = ifr.ifr_ifindex;
							ifr6.ifr6_prefixlen = prefix_len;
							if (ioctl(sockfd6, a1op->selector, &ifr6) < 0) {
								perror(a1op->name);
								++goterr;
							}
							continue;
#endif
						} else if (inet_aton(host, &sai.sin_addr) == 0) {
							/* It's not a dotted quad. */
							struct hostent *hp;
							if ((hp = gethostbyname(host)) == (struct hostent *)NULL) {
								++goterr;
								continue;
							}
							memcpy((char *) &sai.sin_addr, (char *) hp->h_addr_list[0],
							sizeof(struct in_addr));
						}
#ifdef CONFIG_FEATURE_IFCONFIG_BROADCAST_PLUS
						if (mask & A_HOSTNAME) {
							sai_hostname = sai.sin_addr.s_addr;
						}
						if (mask & A_NETMASK) {
							sai_netmask = sai.sin_addr.s_addr;
						}
#endif
						p = (char *) &sai;
#ifdef CONFIG_FEATURE_IFCONFIG_HW
					} else {	/* A_CAST_HOST_COPY_IN_ETHER */
						/* This is the "hw" arg case. */
						if (strcmp("ether", *argv) || (*++argv == NULL)) {
							bb_show_usage();
						}
						safe_strncpy(host, *argv, (sizeof host));
						if (in_ether(host, &sa)) {
							bb_error_msg("invalid hw-addr %s", host);
							++goterr;
							continue;
						}
						p = (char *) &sa;
					}
#endif
					memcpy((((char *) (&ifr)) + a1op->ifr_offset),
						   p, sizeof(struct sockaddr));
				} else {
					unsigned int i = strtoul(*argv, NULL, 0);

					p = ((char *) (&ifr)) + a1op->ifr_offset;
#ifdef CONFIG_FEATURE_IFCONFIG_MEMSTART_IOADDR_IRQ
					if (mask & A_MAP_TYPE) {
						if (ioctl(sockfd, SIOCGIFMAP, &ifr) < 0) {
							++goterr;
							continue;
						}
						if ((mask & A_MAP_UCHAR) == A_MAP_UCHAR) {
							*((unsigned char *) p) = i;
						} else if (mask & A_MAP_USHORT) {
							*((unsigned short *) p) = i;
						} else {
							*((unsigned long *) p) = i;
						}
					} else
#endif
					if (mask & A_CAST_CHAR_PTR) {
						*((caddr_t *) p) = (caddr_t) i;
					} else {	/* A_CAST_INT */
						*((int *) p) = i;
					}
				}

				if (ioctl(sockfd, a1op->selector, &ifr) < 0) {
					perror(a1op->name);
					++goterr;
					continue;
				}
#ifdef QUESTIONABLE_ALIAS_CASE
				if (mask & A_COLON_CHK) {
					/*
					 * Don't do the set_flag() if the address is an alias with
					 * a - at the end, since it's deleted already! - Roman
					 *
					 * Should really use regex.h here, not sure though how well
					 * it'll go with the cross-platform support etc.
					 */
					char *ptr;
					short int found_colon = 0;

					for (ptr = ifr.ifr_name; *ptr; ptr++) {
						if (*ptr == ':') {
							found_colon++;
						}
					}

					if (found_colon && *(ptr - 1) == '-') {
						continue;
					}
				}
#endif
			}
			if (!(mask & A_SET_AFTER)) {
				continue;
			}
			mask = N_SET;
		}
		if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
			perror("SIOCGIFFLAGS");
			++goterr;
		} else {
			selector = op->selector;
			if (mask & SET_MASK) {
				ifr.ifr_flags |= selector;
			} else {
				ifr.ifr_flags &= ~selector;
			}
			if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
				perror("SIOCSIFFLAGS");
				++goterr;
			}
		}
	  LOOP:
		continue;
	}					/* end of while-loop */

	return goterr;
#ifdef CONFIG_FEATURE_IFCONFIG_VLAN_IGMP
VLAN_ERR:
	bb_show_usage();
#endif
#ifdef CONFIG_FEATURE_IFCONFIG_S26QOS
QOS_ERR:
	printf("Here we need to show the usage of QosCtl\n");
#endif
	return -1;

}

#ifdef CONFIG_FEATURE_IFCONFIG_HW
/* Input an Ethernet address and convert to binary. */
static int in_ether(char *bufp, struct sockaddr *sap)
{
	unsigned char *ptr;
	int i, j;
	unsigned char val;
	unsigned char c;

	sap->sa_family = ARPHRD_ETHER;
	ptr = sap->sa_data;

	i = 0;
	do {
		j = val = 0;

		/* We might get a semicolon here - not required. */
		if (i && (*bufp == ':')) {
			bufp++;
		}

		do {
			c = *bufp;
			if (((unsigned char)(c - '0')) <= 9) {
				c -= '0';
			} else if (((unsigned char)((c|0x20) - 'a')) <= 5) {
				c = (c|0x20) - ('a'-10);
			} else if (j && (c == ':' || c == 0)) {
				break;
			} else {
				return -1;
			}
			++bufp;
			val <<= 4;
			val += c;
		} while (++j < 2);
		*ptr++ = val;
	} while (++i < ETH_ALEN);

	return (int) (*bufp);	/* Error if we don't end at end of string. */
}
#endif
