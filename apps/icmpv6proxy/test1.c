#include <errno.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syslog.h>
#include <signal.h>

#include <sys/time.h>

#include <net/if.h>

struct in6_pktinfo  
{    
    struct in6_addr ipi6_addr;  /* src/dst IPv6 address */    
    unsigned int ipi6_ifindex;    /* send/recv interface index */
};


int main()
{
    struct msghdr mhdr;
    struct cmsghdr *cmsg;
    struct iovec iov;
    struct sockaddr_in6 dest, source;
    int sock;
	int err, val;   
    struct in6_pktinfo *pktinfo;
    char ctlbuf[100];
    const int opt_rta_len = 8;
    unsigned char *extbuf;
    struct in6_addr dst,src;    
    struct cmsghdr *cmsg2;    
    struct cmsghdr *chdr;    

    
    inet_pton(AF_INET6,"fe80::1",&src);    
    inet_pton(AF_INET6,"ff02::1",&dst);
    
    unsigned    char sendbuf[] =
    { 0x82, 0x00, 0xe0, 0xc6, 0x27, 0x10, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x02, 0x7d, 0x00, 0x00 };
    

    memcpy( &(dest.sin6_addr), &dst, sizeof(struct in6_addr));
    dest.sin6_family = AF_INET6;
    dest.sin6_port = 0;
    dest.sin6_flowinfo = 0;
    dest.sin6_scope_id = 0;

    memcpy( &(source.sin6_addr), &src, sizeof(struct in6_addr));
    source.sin6_family = AF_INET6;
    source.sin6_port = 0;
    source.sin6_flowinfo = 0;
    source.sin6_scope_id = 0;


    sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	if (sock < 0)
	{
            printf("can't create socket(AF_INET6): %s\n", strerror(errno));
	}
    printf("sock = %d\n",sock);
/*
	val = 2;
	err = setsockopt(sock, IPPROTO_RAW, IPV6_CHECKSUM, &val, sizeof(val));
	if (err < 0)
	{
		printf("setsockopt(IPV6_CHECKSUM): %s", strerror(errno));
	}

	val = 64;
	err = setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &val, sizeof(val));
	if (err < 0)
	{
		printf("setsockopt(IPV6_MULTICAST_HOPS): %s", strerror(errno));
		return (-1);
	}
*/


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

	pktinfo->ipi6_ifindex = 2;
	memcpy(  &(pktinfo->ipi6_addr), &(source.sin6_addr), sizeof(struct in6_addr) );  

	err = sendmsg(sock, &mhdr, 0);
		usleep(1000000);	printf("err = %s\n", strerror(errno));		
    close(sock);

}
