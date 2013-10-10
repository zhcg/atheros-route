/* Adding special route for mapping
 * 2005-9-16	auron
 * Usage: mroute <destination addr> <netmask> 
 * No gateway & interface required
 * Netmask 255.255.255.255 will be automatically viewed as host route
 * 	   0.0.0.0 will be automatically viewed as default route
 */
#include <sys/ioctl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <linux/route.h>
#include <linux/config.h>
#include <linux/netdevice.h>
#include <stdio.h>

int main(int argc, void *argv[]) {
	struct rtentry newrt;
	struct sockaddr_in myaddr;
	struct in6_addr src_prefix, dst_prefix;	/* Indicating the backbone prefix */
	int sock_fd;

	if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("Socket Error!\n");
		return 0;
	}

	newrt.rt_flags = (RTF_UP | RTF_GATEWAY | RTF_MAPPING);

	myaddr.sin_family = AF_INET;
	inet_aton(argv[1], &myaddr.sin_addr);
	memcpy(&newrt.rt_dst, &myaddr, sizeof(myaddr));

	inet_aton(argv[2], &myaddr.sin_addr);
	memcpy(&newrt.rt_genmask, &myaddr, sizeof(myaddr));

	//if (myaddr.sin_addr.s_addr == 0) newrt.rt_flags = (newrt.rt_flags | RTF_DEFAULT);
	if (myaddr.sin_addr.s_addr == 0xffffffff) newrt.rt_flags = (newrt.rt_flags | RTF_HOST);
	inet_aton(argv[3], &myaddr.sin_addr);
	memcpy(&newrt.rt_gateway, &myaddr, sizeof(myaddr));

	newrt.rt_dev = (char *)malloc(sizeof(char) * 16);
	memset(newrt.rt_dev, 0, 16 * sizeof(char));
	
	memcpy(newrt.rt_dev, argv[4], strlen(argv[4]) );
	newrt.rt_metric = 0;

	if (inet_pton(AF_INET6, argv[5], &src_prefix) < 0 || inet_pton(AF_INET6, argv[6], &dst_prefix) < 0) {
		printf("wrong IPv6 prefix!\n");
		return 0;
	}
	
	newrt.src_prefix = src_prefix.s6_addr32[0];
	newrt.dst_prefix = dst_prefix.s6_addr32[0];
	
	if (ioctl(sock_fd, SIOCADDRT, &newrt) < 0) {
		printf("ioctl error!\n");
		return 0;
	}

	return 1;
}



