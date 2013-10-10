/*
 *
 *	NX-ROUTED
 *	RIP-2 Routing Daemon
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *  
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *  
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *	Copyright (C) 2002 Valery Kholodkov
 *	Copyright (C) 2002 Andy Pershin
 *	Copyright (C) 2002 Antony Kholodkov
 *
 */

#include <protocols/routed.h>

#include "router.h"

int rip_socket;

int rip_init(char *ifname) {
	struct	servent		*svp;
	struct sockaddr_in	salocal,*sin;
	struct sockaddr		broadcast;
	struct sockaddr		netmask;
	unsigned short		port;
	
	/*
	 *	Open routing socket.
	 */
	svp = getservbyname ("router", "udp");
	if (svp != (struct servent *) 0)
		port = ntohs(svp->s_port);
	else
		port = 520;
		
	if( (rip2_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		return -1;
	
	if (ioctl(sd, SIOCGIFBRDADDR, &ifr) < 0) {
		close(rip2_socket);
		return -1;
	}	
	memcpy(&broadcast,&ifr.ifr_broadaddr,sizeof(broadcast));

	if (ioctl(sd, SIOCGIFNETMASK, &ifr) < 0) {
		close(rip2_socket);
		return -1;
	}	
	memcpy(&netmask,&ifr.ifr_netmask,sizeof(netmask));
	
	sin = (struct sockaddr_in *) &salocal;
        memset ((char *) sin, '\0', sizeof (salocal));
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = INADDR_ANY;
	sin->sin_port = htons(port);
	
	result = bind (sockfd, & salocal, sizeof (*sin));
	if (result < 0)
		return -1;		
}

int rip_learn_routes(struct rproto *rip,struct rtable *rtable) {
	// Do nothing
	return 0;
}

int rip_unlearn_routes(struct rproto *rip,struct rtable *rtable) {
	// Do nothing
	return 0;
}

int rip_input_packet(struct rproto *rip,struct queue *queue) {
	char buffer[1024];
	struct rip *header;
	struct netinfo *newroute;
	struct route route;	
	struct rtable *rtable;
	
	struct sockaddr_in sender;
	
	if(queue_get_packet(queue,buffer,sizeof(buffer)) < 0)
		return -1;
	
	header = (struct rip *)buffer;
	newroute = &header->ripun.ru_nets[0];
	
	if(header->rip_cmd != RIPCMD_RESPONSE) {
		return -1;
	}
	
	memcpy(&route.dst,&newroute->rip_dst,sizeof(route.dst));
	memcpy(&route.dstmask,"\xFF\xFF\xFF\xFF",sizeof(route.dstmask));
	memcpy(&route.nexthop,&sender,sizeof(route.nexthop));
	route.metric = newroute->rip_metric;
	route.type = RT_DYNAMIC;
	route.source = RIP;
	route.domain = 0;
	route.ttl = 600;
	
	if((rtable = new_rtable()) == NULL)
		return -1;
		
	add_route(rtable,&route);
	
	learn_routes(rip,rtable);
	return 0;
}

int rip_output_packet(struct rproto *rip,struct queue *queue) {
}
