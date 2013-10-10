#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <asm/types.h>
#include <linux/if_arp.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>   /* The L2 protocols */

#undef ARP_DEBUG

int create_arp_socket(int ifindex)
{
	int sockfd;
	struct sockaddr_ll shaddr;

	if((sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ARP))) < 0) {
#ifdef ARP_DEBUG        
		perror("socket()");
#endif        
		return -1;
	}

	memset((void *)&shaddr, 0, sizeof(struct sockaddr_ll));
	shaddr.sll_family=AF_PACKET;
	shaddr.sll_protocol=htons(ETH_P_ARP);
	shaddr.sll_ifindex=ifindex;
	shaddr.sll_hatype=ARPHRD_ETHER;

	if(bind(sockfd,(struct sockaddr *)&shaddr,sizeof(struct sockaddr_ll)) < 0) {
#ifdef ARP_DEBUG        
		perror("bind()");
#endif        
		return -1;
	}

	return sockfd;
}



void close_arp_socket(int sockfd)
{
	close(sockfd);
}



int send_arp_request(int ifindex, int sockfd,
	char *eth_dst, char *eth_src,
	char *s_hwaddr, unsigned long *s_ipaddr,
	char *d_hwaddr, unsigned long *d_ipaddr )
{
	char frame[ sizeof(struct ethhdr) + sizeof(struct arphdr) + 8 + ETH_ALEN * 2];
	struct ethhdr *ehdr;
	struct arphdr *ahdr;
	struct sockaddr_ll dhaddr;

	bzero((void*)frame, sizeof(frame));

	/* Ethernet header */
	ehdr = (struct ethhdr*)frame;
	memcpy(ehdr->h_dest, eth_dst, ETH_ALEN);
	memcpy(ehdr->h_source, eth_src, ETH_ALEN);
	ehdr->h_proto = htons(ETH_P_ARP);

	/* Arp header */
	ahdr = (struct arphdr*)(frame + sizeof(struct ethhdr));
	ahdr->ar_hrd = htons(ARPHRD_ETHER);
	ahdr->ar_pro = htons(ETH_P_IP);
	ahdr->ar_hln = ETH_ALEN;
	ahdr->ar_pln = 4;
	ahdr->ar_op = htons(ARPOP_REQUEST);

	memcpy(ahdr + 1, s_hwaddr, ETH_ALEN);
	memcpy((char *)(ahdr + 1) + ETH_ALEN, (char *)s_ipaddr, 4);
	memcpy((char *)(ahdr + 1) + ETH_ALEN + 4, d_hwaddr, ETH_ALEN);
	memcpy((char *)(ahdr + 1) + ETH_ALEN + 4 + ETH_ALEN, (char *)d_ipaddr, 4);

/*	
 	memcpy(ahdr->ar_sha, s_hwaddr, ETH_ALEN);
	memcpy(ahdr->ar_sip, s_ipaddr, 4);
	memcpy(ahdr->ar_tha, d_hwaddr, ETH_ALEN);
	memcpy(ahdr->ar_tip, d_ipaddr, 4);
*/
	/* fill dhaaddr */
	memset((void *)&dhaddr, 0, (size_t)sizeof(struct sockaddr_ll));
	dhaddr.sll_family=AF_PACKET;
	dhaddr.sll_protocol=htons(ETH_P_ARP);
	dhaddr.sll_ifindex=ifindex;
	dhaddr.sll_hatype=ARPHRD_ETHER;

	if(sendto(sockfd, frame, sizeof(frame), 0, (struct sockaddr *)&dhaddr, (socklen_t)sizeof(dhaddr))<0) {
#ifdef ARP_DEBUG        
		perror("sendto");
#endif
		return -1;
	}
	return 0;
};

int recv_arp_echo(int sockfd, unsigned long *d_ipaddr) 
{
	char frame[100]; 
	struct arphdr *ahdr;
	struct sockaddr_ll shaddr;
	fd_set readfds;
	struct timeval timeout;

    int ret;

	FD_ZERO(&readfds);
	FD_SET(sockfd, &readfds);
	timeout.tv_sec = 0;
	timeout.tv_usec = 1000;
	
	if((ret = select(sockfd + 1, &readfds, NULL, NULL, &timeout)) < 0){
#ifdef ARP_DEBUG        
		perror("select()");
#endif
		return -1;
	}

	if(FD_ISSET(sockfd, &readfds)) {
		socklen_t len = sizeof(struct sockaddr);
		if(recvfrom(sockfd, frame, sizeof frame, 0, (struct sockaddr *)&shaddr, &len) < 0){
#ifdef ARP_DEBUG        
			perror("recvfrom()");
#endif            
			return -1;
		}

		ahdr = (struct arphdr*)(frame + sizeof(struct ethhdr));
	    if(ahdr->ar_op != ntohs(ARPOP_REPLY) || 
		    (memcmp((char *)(ahdr + 1) + ETH_ALEN, (char *)d_ipaddr, 4) != 0)) {
#ifdef ARP_DEBUG        
            printf("Wrong data.\n");
#endif
            return -1;
        }
		return 0;	
	}

	return -1;	
}

int get_ip_mac(const char *ifname, unsigned long *ip, unsigned char *mac)  
{  
    int sock, ret;  
    struct ifreq ifr;  
    struct sockaddr_in *sin_ptr;

    sock =  socket(AF_INET, SOCK_STREAM,   0);  
    if(sock < 0) {  
#ifdef ARP_DEBUG        
        perror("socket()");  
#endif
        return   -1;  
    }

    memset((void *)&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, (char *)ifname);  
                        
    /* get ifname's mac */
    ret = ioctl(sock, SIOCGIFHWADDR, &ifr, sizeof(ifr));  
    if(ret  == 0) {                                  
        memcpy((void *)mac, ifr.ifr_hwaddr.sa_data, ETH_ALEN);  
    } else {  
#ifdef ARP_DEBUG        
        perror("ioctl()");
#endif        
        goto out;
    }  

    /*get ifname's ip */
    ret = ioctl(sock, SIOCGIFADDR, &ifr);
    if(ret == 0) {
        sin_ptr = (struct sockaddr_in *)&ifr.ifr_addr;    
        *ip = (unsigned long)sin_ptr->sin_addr.s_addr;
    } 
#ifdef ARP_DEBUG        
    else {        
        perror("ioctl SIOCGIFADDR error");    
    }
#endif
out:
    close(sock );  
    return ret;  
}


int arp_query(char *ipaddr, char *iface)
{
	int i,sockfd;
	int ifindex = if_nametoindex(iface);
	unsigned char eth_dst[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	unsigned char eth_src[ETH_ALEN];
    unsigned long s_ipaddr;
    unsigned long d_ipaddr = inet_addr(ipaddr);

    if(get_ip_mac(iface, &s_ipaddr, eth_src) < 0) {
        goto out;
    }

	if((sockfd = create_arp_socket(ifindex)) < 0) {
        goto out;
	}

	for(i = 0; i < 3; i++) {
		if( send_arp_request(ifindex, sockfd,
			eth_dst, eth_src,
			eth_src, &s_ipaddr,
			eth_dst, &d_ipaddr ) < 0) {
			break;
		}

		if(recv_arp_echo(sockfd, &d_ipaddr) < 0) {
#ifdef ARP_DEBUG        
			printf("NO echo, continue.\n");
#endif
			continue;
		}
#ifdef ARP_DEBUG        
		printf("Recieve arp echo, stop.\n");
#endif
		break;	
	}

	close_arp_socket(sockfd);

    if(i == 3)
        goto out;
	return 0;
out:
    return -1;
}

/*
int main()
{
	unsigned long d_ipaddr = inet_addr("192.168.1.1");

    if(arp_query(d_ipaddr, "br100") < 0) {
        printf("arp_query failed.\n");
        return -1;
    }
    else {
        printf("arp_query success.\n");
    }

    return 0;

}
*/



