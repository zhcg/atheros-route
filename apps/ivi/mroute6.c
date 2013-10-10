/* Adding special route for mapping (ipv6)
 * 2005-9-16	auron
 * Usage: mroute6 <destination addr> <mask length> 
 * No gateway & interface required
 * Netmask mask length = 128 will be automatically viewed as host route
 * 	   mask length = 0 will be automatically viewed as default route
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
//#include <linux/config.h>
#include <stdio.h>
//#include <net/if.h>
#include <linux/ipv6_route.h>

#if 0
#include <sys/ioctl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <linux/route.h>
#include <linux/autoconf.h>
#include <linux/netdevice.h>
#endif

int main(int argc, void *argv[]) {
	struct in6_rtmsg newrt;
	struct in6_addr myaddr;
	int sock_fd;
	int masklen, i;
	int temp, top;
	char mydevname[] = "eth0";

	if ((sock_fd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
	        printf("Socket Error!\n");
	        return 0;
	}
	
	memset(&newrt, 0, sizeof(newrt));


	newrt.rtmsg_type = RTMSG_NEWROUTE;

	newrt.rtmsg_flags = (RTF_UP | RTF_MAPPING);

	if (inet_pton(AF_INET6, argv[1], &myaddr) < 0) {
		printf("Wrong address!!\n");
		return(0);
	}

	memcpy(&newrt.rtmsg_dst, &myaddr, sizeof(myaddr));

	masklen = atoi(argv[2]);
	newrt.rtmsg_dst_len = masklen;

	if (masklen == 0) newrt.rtmsg_flags = (newrt.rtmsg_flags | RTF_DEFAULT);
	if (masklen == 128) newrt.rtmsg_flags = (newrt.rtmsg_flags | RTF_HOST);


	//memcpy(&newrt.rtmsg_gateway, &myaddr, sizeof(myaddr));
	newrt.rtmsg_ifindex = if_nametoindex(mydevname);
	newrt.rtmsg_metric = 1;


	if (ioctl(sock_fd, SIOCADDRT, &newrt) < 0) {
		printf("ioctl error!!!!!!!\n");
		printf("errno::%d\n", errno);
		return 0;
	}
	close(sock_fd);

	return 1;
}



