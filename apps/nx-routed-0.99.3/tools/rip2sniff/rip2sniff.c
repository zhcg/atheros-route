/*
 *
 *	NX-ROUTED
 *	RIP-2 Sniffing tool
 *
 *	Copyright (C) 2002 Andy Pershin
 *
 */
 
/*
 * $Id: //depot/sw/releases/Aquila_9.2.0_U11/apps/nx-routed-0.99.3/tools/rip2sniff/rip2sniff.c#1 $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include <time.h>

#define RIP_MCAST   0xE0000009    /* 224.0.0.9 */
#define RIP_PORT    520
#define RIPv2       2

#define USE_RAW

struct rte {
    u_int16_t af;
    u_int16_t tag;
    struct in_addr prefix;
    struct in_addr mask;
    struct in_addr nexthop;
    u_int32_t metric;
};

struct auth {
    u_int16_t xffff;
    u_int16_t type;
    char password[16];
};

struct rip {
    unsigned char command;
    unsigned char version;
    unsigned char pad1;
    unsigned char pad2;
};

time_t lastt=0;
time_t now=0;
    
static void show_packet(void *buf, int len) {

    int rtes = (len - sizeof(struct rip)) / 20; 
    int i;
    struct rip *pkt;
    struct rte *r;
    char *prefix, *mask, *nexthop;

    pkt = (struct rip*)buf;
    printf("Command: 0x%X\tVersion: 0x%X (delta t %d)\n", pkt->command, pkt->version, now-lastt);
    for (i=0;i<rtes;i++) {
        r = (struct rte *)(buf+sizeof(struct rip)+(sizeof(struct rte)*i));
        prefix = strdup(inet_ntoa(r->prefix));
        mask = strdup(inet_ntoa(r->mask));
        nexthop = strdup(inet_ntoa(r->nexthop));
        printf("  prefix: %s\tmask: %s\tnexthop: %s metric: %d\n",
                prefix, mask, nexthop, ntohl(r->metric));
        free(prefix);
        free(mask);
        free(nexthop);
    }
}

int main (int argc, char *argv[]) {

    int sock;
    struct sockaddr_in addr, from;
    struct ip_mreq m;
    int i, len;
    char buf[512];
    char *buf2=buf;
#ifdef USE_RAW
    struct ip *ip=(struct ip *)buf;
    struct udphdr *udp=(struct udphdr *)(buf+sizeof(struct ip));
    /* usefull data after all headers */
    buf2=buf+sizeof(struct ip)+sizeof(struct udphdr);
#endif

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;                                                    
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(RIP_PORT);

#ifndef USE_RAW
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket()");
        return 1;
    }
    i = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) < 0) {
        perror("setsockopt(SO_REUSEADDR)");
        return 1;
    }
    /* broadcast */
    i = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &i, sizeof(i)) < 0) {
        perror("setsockopt(SO_BROADCAST)");
        return 1;
    }
    /* multicast */
    memset (&m, 0, sizeof(m));
    m.imr_multiaddr.s_addr = htonl(RIP_MCAST);
    m.imr_interface = (struct in_addr)addr.sin_addr;
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &m, sizeof(m)) < 0) {
        perror("setsockopt(IP_ADD_MEMBERSHIP)");
    }

    if (bind(sock, (struct sockaddr *) &addr, sizeof (addr)) < 0) {
        perror("bind()");
        return 1;
    }
#else
    if ((sock = socket(PF_INET, SOCK_RAW, IPPROTO_UDP)) < 0) {
        perror("socket()");
        return 1;
    }
#endif
    now=time(NULL);
    while (1) {
        i = sizeof(from);
        len = recvfrom(sock, buf, sizeof(buf), 0,
                (struct sockaddr *) &from, &i);
#ifdef USE_RAW
	/* Process only the ones who got right dest port..
	 * Dont mind checking ip->saddr...
	 */  
	if (ntohs(udp->dest) != RIP_PORT)
	    continue;
	len=len-sizeof(struct ip)-sizeof(struct udphdr);
#endif
	lastt=now;
	time(&now);
	if (now-lastt)
	    printf("\n");
        printf("Received %d bytes from %s\n", len, inet_ntoa(from.sin_addr));
        if ((len - sizeof(struct rip)) % 20) {
            fprintf(stderr, "Wrong packet size: %d\n", len);
            continue;
        }
        show_packet(buf2, len);
	
    }
    return 0;
}

