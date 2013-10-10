/*
**  igmpproxy - IGMP proxy based multicast router 
**  Copyright (C) 2005 Johnny Egeland <johnny@rlo.org>
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**
**----------------------------------------------------------------------------
**
**  This software is derived work from the following software. The original
**  source code has been modified from it's original state by the author
**  of igmpproxy.
**
**  smcroute 0.92 - Copyright (C) 2001 Carsten Schill <carsten@cschill.de>
**  - Licensed under the GNU General Public License, version 2
**  
**  mrouted 3.9-beta3 - COPYRIGHT 1989 by The Board of Trustees of 
**  Leland Stanford Junior University.
**  - Original license can be found in the "doc/mrouted-LINCESE" file.
**
*/
/**
*   igmp.h - Recieves IGMP requests, and handle them 
*            appropriately...
*/
#include "defs.h"

extern int isAdressValidForIf( struct IfDesc* intrface, uint32 ipaddr );
extern int inetChksum(u_short *addr, u_int len);

struct mldQuery
{
    uint8_t     icmp6_type;   /* type field */
    uint8_t     icmp6_code;   /* code field */
    uint16_t    icmp6_cksum;  /* checksum field */
    uint16_t    max_resp_delay; /* Max response delay */
    struct in6_addr addr;
    uint8_t     sFlagAndRob;
    uint8_t     qqi;
};

 
// Globals                  
struct in6_addr     allhosts_group;            /* All hosts addr in net order */
struct in6_addr     allrouters_group;          /* All hosts addr in net order */
struct in6_addr     mld_report_group;        
struct in6_addr     allzero_addr;

              
extern int MRouterFD;
int         Ipv6Sock;  /* Ipv6 raw socket */
void        *Ipv6SockMap;
char        *Ipv6SockMapCurPtr;
int         tp_frame_size = 2048;
int         tp_block_size = 256 * 1024;
int         mmapedlen = 256*1024;

void createIpv6RawSocket()
{
    struct tpacket_req req;

    req.tp_frame_size = tp_frame_size;
    req.tp_block_size = tp_block_size;
    req.tp_block_nr = mmapedlen / req.tp_block_size;
    req.tp_frame_nr = mmapedlen / req.tp_frame_size;

    
    Ipv6Sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IPV6));
    if (Ipv6Sock < 0)
        atlog( LOG_ERR, errno, "can't create socket(AF_INET6): %s\n", strerror(errno));
    else
        atlog( LOG_DEBUG, 0, "Ipv6Sock created as %d\n", strerror(errno));

    if (setsockopt(Ipv6Sock, SOL_PACKET, PACKET_RX_RING, (void *)&req, sizeof(req)) == 0) 
    {
        Ipv6SockMap = mmap(0, mmapedlen, PROT_READ|PROT_WRITE, MAP_SHARED, Ipv6Sock, 0);
        if ( Ipv6SockMap == MAP_FAILED ) 
        {
            atlog( LOG_ERR, errno, "Ipv6SockMap map failed\n", Ipv6Sock);
            Ipv6SockMap = NULL;
            Ipv6SockMapCurPtr = NULL;
            return;
        } 
        Ipv6SockMapCurPtr = Ipv6SockMap;
    }
    else
        atlog( LOG_ERR, errno, "PACKET_RX_RING failed: %s\n", strerror(errno));
}

void closeIpv6RawSocket()
{
    struct tpacket_req req;
    if (Ipv6SockMap) 
    {
        munmap(Ipv6SockMap, mmapedlen);
        memset(&req, 0, sizeof(req));
        setsockopt(Ipv6Sock, SOL_PACKET, PACKET_RX_RING, &req, sizeof(req));
    }    
    close(Ipv6Sock);
}

/*
 * Open and initialize the igmp socket, and fill in the non-changing
 * IP header fields in the output packet buffer.
 */
void initIcmpv6() 
{
    send_buf = malloc(RECV_BUF_SIZE);   

    inet_pton(AF_INET6, "ff02::1"	, &allhosts_group); 
    inet_pton(AF_INET6, "ff02::2"	, &allrouters_group); 
    inet_pton(AF_INET6, "ff02::16"	, &mld_report_group);     
    inet_pton(AF_INET6, "::", &allzero_addr); 

    k_set_rcvbuf(256*1024,48*1024); /* lots of input buffering        */
    k_set_ttl(1);           /* restrict multicasts to one hop */
    k_set_loop(FALSE);      /* disable multicast loopback     */

    createIpv6RawSocket();

    joinReportGroup();

    //force the mld version to 2
    FILE *fp = fopen("/proc/sys/net/ipv6/conf/all/force_mld_version", "w");
    if(fp)
        fprintf(fp, "2");
    fclose(fp);    
}

/**
*   Finds the textual name of the supplied IGMP request.
*/
char *igmpPacketKind(u_int type, u_int code) 
{    
    static char unknown[20];
#if 0
    switch (type) 
    {
        case IGMP_MEMBERSHIP_QUERY:     return  "Membership query  ";
        case IGMP_V1_MEMBERSHIP_REPORT:  return "V1 member report  ";
        case IGMP_V2_MEMBERSHIP_REPORT:  return "V2 member report  ";
        case IGMP_V2_LEAVE_GROUP:        return "Leave message     ";
        
        default:
            sprintf(unknown, "unk: 0x%02x/0x%02x    ", type, code);
    }
#endif
    return unknown;
}


/**
 * Process a newly received IGMP packet that is sitting in the input
 * packet buffer.
 */
void acceptIcmpv6(char *msg, struct sockaddr_in6 *from) 
{
    int     i, j;
    int     recNum, srcNum, auxLen;
    char    *ptr = &(msg[8]);
    char    type;
    struct in6_addr *group;
    struct in6_addr *src;
    int     port = 0;
/*
    printf("acceptIcmpv6 called\n");
    for (i = 0; i < 40; i++)
        printf("%02x ", msg[i] & 255);
    printf("\n");
*/
    if ( ( (msg[2] & 0xff) == 0x7d) && ( (msg[3] & 0xf0) == 0x50 ) )    //checksum msg[2] and msg[3]
        port = msg[3] & 0xf;

    if ( (msg[0] & 255) == 143)  //version 2
    {
        recNum = msg[6] & 255;
        recNum <<= 8;
        recNum += msg[7] & 255;

        atlog( LOG_DEBUG, 0, "\n\n\nInside acceptIcmpv6 v2, port = %d, checksum = %02x %02x\n", port, msg[2] & 255, msg[3] & 255);   

        for (i = 0; i < recNum; i++)
        {           
            type = *ptr;
            ptr++;

            auxLen = *ptr;
            ptr++;

            srcNum = *ptr & 255;
            srcNum <<= 8;
            ptr++;
            srcNum += *ptr;
            ptr++;

            group = (struct in6_addr *)ptr;
            ptr += sizeof(struct in6_addr);    

            //printf("type = %d, srcNum = %d, auxLen = %d\n", type, srcNum, auxLen);

            if (srcNum > 0 )
            {
                for ( j = 0; j < srcNum; j++ )
                {
                    src = (struct in6_addr *)ptr;
                    ptr += sizeof(struct in6_addr);       

                    if (type == 4 || type == 2)
                        acceptGroupReport( port, from, group, src );
                    else if (type == 3 || type == 1)
                        acceptLeaveMessage( from, group, src );
                    else
                        atlog( LOG_DEBUG, 0, "icmpv6 include/exclude error type = %d\n", type);
                }
            }
            else
            {
                if (type == 4 || type == 2)
                    acceptGroupReport( port, from, group, &allzero_addr );
                else if (type == 3 || type == 1)
                    acceptLeaveMessage( from, group, &allzero_addr );
                else
                    atlog( LOG_DEBUG, 0, "icmpv6 include/exclude error type = %d\n", type);
            }
            ptr += auxLen;            
        }
    }
    else if( (msg[0] & 255) == 131) //version 1 add
    {
        atlog( LOG_DEBUG, 0, "\n\n\naccept v1, port = %d, checksum = %02x %02x\n", port, msg[2] & 255, msg[3] & 255);   
        group = (struct in6_addr *)ptr;
        acceptGroupReport( port, from, group, &allzero_addr );
    }
    else if( (msg[0] & 255) == 132) //version 1 del
    {
        atlog( LOG_DEBUG, 0, "\n\n\nleave v1, port = %d, checksum = %02x %02x\n", port, msg[2] & 255, msg[3] & 255);   
        group = (struct in6_addr *)ptr;
        acceptLeaveMessage( from, group, &allzero_addr );
    }    
}

unsigned short inchksum(const void *data, int length)
{
	long sum = 0;
	const unsigned short *wrd = (const unsigned short *)(data);
	int slen = length;

	while (slen > 1) 
     {
		sum += *wrd++;
		slen -= 2;
	}

	if (slen > 0)
		sum += *((unsigned char *)(wrd));

	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return (unsigned short)(sum);
}

unsigned short ipv6_checksum(unsigned char protocol, struct in6_addr *src, 
                             struct in6_addr *dst, const void *data, unsigned short len) 
{
	struct {
		struct in6_addr src;
		struct in6_addr dst;
		unsigned short length;
		unsigned short zero1;
		unsigned char zero2;
		unsigned char next;
	} pseudo;
	unsigned long chksum = 0;

	pseudo.src = *src;
	pseudo.dst = *dst;
	pseudo.length = htons(len);
	pseudo.zero1 = 0;
	pseudo.zero2 = 0;
	pseudo.next = protocol;

	chksum = inchksum(&pseudo, sizeof(pseudo));
	chksum += inchksum(data, len);

	chksum = (chksum >> 16) + (chksum & 0xffff);
	chksum += (chksum >> 16);

	chksum = (unsigned short)(~chksum);
	if (chksum == 0)
		chksum = 0xffff;

	return chksum;
}

struct IfDesc * downStreamIfDesc = NULL;
#if 0
void sendIcmpv6(struct in6_addr *dst, int delay, struct in6_addr *group)
{
    struct sockaddr_in6 dest;
    int result;
    unsigned short checksum;

    int i;
    printf("\n\n\n sendIcmpv6 dst:");
    for (i = 0; i < 16; i++)
        printf("%02x ", ((char*)dst)[i] & 255);
    printf("\n");

    printf(" sendIcmpv6 group:");
    for (i = 0; i < 16; i++)
        printf("%02x ", ((char*)group)[i] & 255);
    printf("\n\n\n");

    unsigned    char sendbuf[] =
    { 0x82, 0x00, 0xe0, 0xc6, 0x27, 0x10, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x02, 0x7d, 0x00, 0x00 };
    
    checksum = ipv6_checksum(IPPROTO_ICMPV6, &(downStreamIfDesc->linkAddr), 
                             dst, &sendbuf, 40);
    printf("checksum = %x\n", checksum);
    
    dest.sin6_addr = *dst;
    dest.sin6_family = AF_INET6;
    dest.sin6_flowinfo = 0;
    dest.sin6_scope_id = (unsigned int)(downStreamIfDesc->index);
    dest.sin6_port = 0;

    sendbuf[4] = delay >> 8;
    sendbuf[6] = delay & 255;

    memcpy( &(sendbuf[8]), group, sizeof(struct in6_addr) );

    result = sendto(MRouterFD, sendbuf, sizeof(sendbuf), 0, (struct sockaddr*)&dest, sizeof(dest));

    printf("sendto result = %d, %s \n", result, strerror(errno));
    if(result == -1)
    {
        atlog( LOG_DEBUG, 0, "sendto error %s\n", strerror(errno));
    }
}
#endif

void sendIcmpv6(struct in6_addr *dst, int delay, struct in6_addr *group)
{
    struct msghdr mhdr;
    struct iovec iov;
    struct sockaddr_in6 dest, source;
    int sock;
	int err;   
    struct in6_pktinfo *pktinfo;
    char ctlbuf[100];
    const int opt_rta_len = 8;
    unsigned char *extbuf;
    struct cmsghdr *cmsg2;    
    struct cmsghdr *chdr;    

        
    unsigned    char sendbuf[] =
    { 0x82, 0x00, 0xe0, 0xc6, 0x27, 0x10, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x02, 0x7d, 0x00, 0x00 };
    
    dest.sin6_addr = *dst;
    dest.sin6_family = AF_INET6;
    dest.sin6_flowinfo = 0;
    dest.sin6_scope_id = (unsigned int)(downStreamIfDesc->index);
    dest.sin6_port = 0;

    source.sin6_addr = downStreamIfDesc->linkAddr;
    source.sin6_family = AF_INET6;
    source.sin6_port = 0;
    source.sin6_flowinfo = 0;
    source.sin6_scope_id = (unsigned int)(downStreamIfDesc->index);


	cmsg2 = (struct cmsghdr *)(ctlbuf + CMSG_SPACE(sizeof(struct in6_pktinfo)));
	if (cmsg2 == NULL)
		printf("cmsg2 = NULL");
    
	cmsg2->cmsg_len = CMSG_SPACE(opt_rta_len);
	cmsg2->cmsg_level = IPPROTO_IPV6;
	cmsg2->cmsg_type = IPV6_HOPOPTS;

	extbuf = (unsigned char *)CMSG_DATA(cmsg2);

	extbuf[0] = 0x00; /* next header */
	extbuf[1] = 0x00; /* length (8 bytes) */
	extbuf[2] = 0x05;
	extbuf[3] = 0x02; /* RTA length (2 bytes) */
	*(unsigned short *)(extbuf + 4) = 0;
	extbuf[6] = 0x01;
	extbuf[7] = 0x00;

    
	iov.iov_len  = sizeof(sendbuf);
	iov.iov_base = (caddr_t) sendbuf;

	memset(&mhdr, 0, sizeof(mhdr));
	mhdr.msg_name = (void *)(&dest);
	mhdr.msg_namelen = sizeof(struct sockaddr_in6);
	mhdr.msg_iov = &iov;
	mhdr.msg_iovlen = 1;
	mhdr.msg_control = ctlbuf;
	mhdr.msg_controllen = CMSG_SPACE(sizeof(struct in6_pktinfo)) + CMSG_SPACE(opt_rta_len);
	mhdr.msg_flags = 0;

	chdr = (struct cmsghdr *)CMSG_FIRSTHDR(&mhdr);
	chdr->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
	chdr->cmsg_level = IPPROTO_IPV6;
	chdr->cmsg_type = IPV6_PKTINFO;

	pktinfo = (struct in6_pktinfo *)CMSG_DATA(chdr);

	pktinfo->ipi6_ifindex = downStreamIfDesc->index;
	memcpy(  &(pktinfo->ipi6_addr), &(source.sin6_addr), sizeof(struct in6_addr) );  

	err = sendmsg(MRouterFD, &mhdr, 0);

}
