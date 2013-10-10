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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <memory.h>

#include <sys/ioctl.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/route.h>
#include <linux/sockios.h>

#include <time.h>

#include "router.h"
#include "socket.h"
#include "util.h"
#include "link.h"

void hextosockaddr(const char *str,unsigned int *addr) {
	unsigned long n;
	
	sscanf(str,"%lx",&n);
	*addr = ntohl(n); // Здесь надо ntohl
}

struct route *strtoroute(const char *str) {
	char iface[8];
	char dest[9];
	char gw[9];
	int flags;
	int refcnt;
	int use;
	int metric;
	char mask[9];
	int mtu;
	int window;
	int irtt;
	struct route *route;

	if(sscanf(str,"%8s %8s %8s %d %d %d %d %8s %d %d %d",
		iface,dest,gw,&flags,&refcnt,&use,&metric,
		mask,&mtu,&window,&irtt) != 11)
	{
		error("Invalid procfs route table entry");
		return NULL;
	}
	
	if((flags & RTF_UP) == 0)
		return NULL;
		
	if((flags & RTF_GATEWAY) == 0)
		return NULL;
		
	if((route = malloc(sizeof(struct route))) == NULL) {
		error("Insufficient memory allocating route");
		return NULL;
	}
	
	// FIXME: modify flags according to system's flags
	hextosockaddr(dest,&route->dst);
	hextosockaddr(mask,&route->dstmask);
	hextosockaddr(gw,&route->nexthop);
	route->metric = metric;
	route->type = (flags & RTF_DYNAMIC) ? RT_DYNAMIC : RT_STATIC;
	route->flags = 0;
	route->source = 0;
	route->source_link = linklist_link_for_iface(iface);
	route->rtag = 0;
	route->domain = 0;
	route->expire = 0;
	route->garbage = 0;
	route->timerset = 0;
	return route;
}

struct rtable *system_read_rtable(void) {
	FILE *f;
	char string[1024];
	int len,firstline=1;	
	struct rtable *rtable;
	struct route *route;
	
	if((f = fopen("/proc/net/route","r")) == NULL)
		return NULL;
		
	if((rtable = new_rtable()) == NULL)
		return NULL;
		
	while(!feof(f)) {
		if(fgets(string,sizeof(string),f) == NULL)
			break;
		if(firstline) {
			firstline = 0;
			continue;
		}			
		len = strlen(string);
		if(string[len] == '\n')
			string[len] = '\0';
		
		if((route = strtoroute(string)) != NULL) {
			add_route(rtable,route);
		}
	}
	return rtable;
}

int system_add_routes(struct socket *socket,struct rtable *rtable) {
	struct route *route;
	struct sockaddr_in sin;
	struct rtentry rtentry;
	
	if(rtable->num_routes == 0)
		return 0;		
	
	if(rtable == NULL) {
		error("system_add_routes: NULL rtable");
		return -1;
	}
	
	if(socket == NULL) {
		error("system_add_routes: NULL socket");
		return -1;
	}
	
	sin.sin_family = AF_INET;
	for(route = rtable->routes;route;route = route->next) {	
		if(route->flags & RF_UNREACH)
			continue;
			
		/* Don't add direct links.. Kernel does that..
		 * Is there a better way to find out this is a local link route?
		 */
		if(route->metric == route->source_link->cost)
			continue;			
			
		sin.sin_addr.s_addr = htonl(route->dst);
		memcpy(&rtentry.rt_dst,&sin,sizeof(sin));
		
		sin.sin_addr.s_addr = htonl(route->nexthop);
		memcpy(&rtentry.rt_gateway,&sin,sizeof(sin));		
		
		sin.sin_addr.s_addr = htonl(route->dstmask);
		memcpy(&rtentry.rt_genmask,&sin,sizeof(sin));
		
		rtentry.rt_metric = route->metric;
		
		rtentry.rt_flags = RTF_UP | RTF_GATEWAY | RTF_DYNAMIC;
		rtentry.rt_dev = route->iface;
		
		if(ioctl(socket->fdesc,SIOCADDRT,&rtentry) < 0) {
			error("Unable to add route: %s",strerror(errno));
			dump_route(route);
			return -1;
		}
	}	
	return 0;
}

int system_remove_routes(struct socket *socket,struct rtable *rtable) {
	struct route *route;
	struct sockaddr_in sin;
	struct rtentry rtentry;
	
	if(rtable->num_routes == 0)
		return 0;	
	
	if(rtable == NULL) {
		error("system_remove_routes: NULL rtable");
		return -1;
	}
	
	if(socket == NULL) {
		error("system_remove_routes: NULL socket");
		return -1;
	}
	
	sin.sin_family = AF_INET;
	for(route = rtable->routes;route;route = route->next) {	
		if(route->flags & RF_UNREACH)
			continue;

		/* 
		 * Damjan "Zobo" <zobo@lana.krneki.org>
		 * Dont try to remove routes, those interface isn't up...
		 * Kernel removes all routes when intf goes down..
		 */
		if(!(route->source_link->flags & LF_UP))
			continue;			

		if(route->metric == route->source_link->cost)
			continue;
					
		sin.sin_addr.s_addr = htonl(route->dst);
		memcpy(&rtentry.rt_dst,&sin,sizeof(sin));
		
		sin.sin_addr.s_addr = htonl(route->nexthop);
		memcpy(&rtentry.rt_gateway,&sin,sizeof(sin));		
		
		sin.sin_addr.s_addr = htonl(route->dstmask);
		memcpy(&rtentry.rt_genmask,&sin,sizeof(sin));
		
		rtentry.rt_metric = route->metric;
		
		rtentry.rt_flags = RTF_UP | RTF_GATEWAY | RTF_DYNAMIC;
		rtentry.rt_dev = route->iface;
		
		if(ioctl(socket->fdesc,SIOCDELRT,&rtentry) < 0) {
			error("Unable to delete route: %s",strerror(errno));
			dump_route(route);
			// Сказать скажем, а тормозить не будем
			//return -1;
		}
	}	
	return 0;
}

