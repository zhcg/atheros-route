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
#include <unistd.h>
#include <memory.h>
#include <string.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/socket.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netinet/if_ether.h>
#include <linux/if.h>
#include <linux/sockios.h>

#include "router.h"
#include "util.h"

#include "link.h"

struct link link_list = { &link_list, &link_list, };

void netlink_fake_linkup(struct link *);
void netlink_fake_linkdown(struct link *);

struct link *link_create(const char *ifname, int virt) {
	struct link *link;
	
	if(ifname == NULL) {
		error("link_create: ifname == NULL");
		return NULL;
	}
	
	if((link = malloc(sizeof(struct link))) == NULL) {
		error("Unable to allocate memory for link");
		return NULL;
	}
	
	strncpy(link->iface, ifname, sizeof(link->iface));
	link->unicast = link->broadcast = link->multicast = 0;
	
	link->flags = 0;
	link->cost = 1;
	if(virt) link->flags = LF_VIRTUAL;
	
	if(link_update(link) < 0) {
		link_set_down(link);
	}
	
	return link;
}

int link_test(int sock, char *iface) {
	struct ifconf ifconf;
	struct ifreq *ifreq;
	int i;
	
	/* FIXME: It's not nice to limit interfaces to 20 */
	ifconf.ifc_len = 20 * sizeof(struct ifreq);
	ifconf.ifc_buf = malloc(ifconf.ifc_len);
	if(ioctl(sock, SIOCGIFCONF, &ifconf) < 0) {
		free(ifconf.ifc_buf);
		return -1;
	}
	ifreq = ifconf.ifc_req;
	for (i = 0;i<ifconf.ifc_len/sizeof(struct ifreq); i++) {
		if(!strncmp(iface, ifreq[i].ifr_name, 16)) {
			/* found it! */
			free(ifconf.ifc_buf);
			return 0;
		}
	}
	free(ifconf.ifc_buf);
	return -1;
}

int link_update(struct link *link) {
	int sock;
	unsigned int flags;
	struct ifreq ifreq;
	
	if(link->flags & LF_VIRTUAL) {
		link->flags |= LF_UP;
		return 0;
	}
	
	if((sock = socket(AF_INET,SOCK_DGRAM,0)) < 0) {
		error("Unable to open dummy socket");
		return -1;
	}
	
	/* Checking whether interface is up */
	if(link_test(sock, link->iface) < 0) {
		//Link not up...
		close(sock);
		return -1;
	}
	
	strcpy(ifreq.ifr_name, link->iface);
	if(ioctl(sock, SIOCGIFFLAGS, &ifreq) < 0) {
		//error("Unable to get interface flags: %s",strerror(errno));
		close(sock);
		return -1;
	}	
	flags = ifreq.ifr_flags;
	
	// Optaining interface address
	strcpy(ifreq.ifr_name, link->iface);
	if(ioctl(sock, SIOCGIFADDR, &ifreq) < 0) {
		error("Unable to get interface address: %s",strerror(errno));
		close(sock);
		return -1;
	}	
	link->address = ntohl(((struct sockaddr_in*)&ifreq.ifr_addr)->sin_addr.s_addr);

	// Optaining interface mask
	strcpy(ifreq.ifr_name, link->iface);
	if(ioctl(sock, SIOCGIFNETMASK, &ifreq) < 0) {
		error("Unable to get interface mask: %s",strerror(errno));
		close(sock);
		return -1;
	}	
	link->netmask = ntohl(((struct sockaddr_in*)&ifreq.ifr_netmask)->sin_addr.s_addr);
	
	if(flags & IFF_MULTICAST) {
		link->multicast = ntohl(inet_addr("224.0.0.9"));
		link->flags |= LF_MULTICAST;
	}
	
	if(flags & IFF_POINTOPOINT) {
		strcpy(ifreq.ifr_name, link->iface);
		if(ioctl(sock, SIOCGIFDSTADDR, &ifreq) < 0) {
			error("Unable to get interface destination address: %s",strerror(errno));
			close(sock);
			return -1;
		}	
		link->broadcast = ntohl(((struct sockaddr_in*)&ifreq.ifr_dstaddr)->sin_addr.s_addr);
		link->flags |= (LF_BROADCAST | LF_POINTOPOINT);
	}else{
		strcpy(ifreq.ifr_name, link->iface);
		if(ioctl(sock, SIOCGIFBRDADDR, &ifreq) < 0) {
			error("Unable to get interface broadcast address: %s",strerror(errno));
			close(sock);
			return -1;
		}	
		link->broadcast = ntohl(((struct sockaddr_in*)&ifreq.ifr_broadaddr)->sin_addr.s_addr);
		link->flags |= LF_BROADCAST;
	}
	
	if(flags & IFF_UP) {
		link_set_up(link);
	}else{
		link_set_down(link);
	}	
	
	close(sock);
	
	return 0;
}

void link_destroy(struct link *link) {
	if(link == NULL) {
		error("link_destroy: link == NULL");
		return;
	}
	free(link);
}

int link_set_cost(struct link *link,unsigned short cost) {
	if(link == NULL) {
		error("link_set_cost: link == NULL");
		return -1;
	}
	
	link->cost = cost;
	return 0;
}

int link_set_up(struct link *link) {
	if(link == NULL) {
		error("link_set_up: link == NULL");
		return -1;
	}
	
	if(!(link->flags & LF_UP)) {
		link->flags |= LF_UP;
		notify_link_up(link);
	}
	
	return 0;
}

int link_set_down(struct link *link) {
	if(link == NULL) {
		error("link_set_down: link == NULL");
		return -1;
	}
	
	if(link->flags & LF_UP) {
		link->flags &= ~LF_UP;
		notify_link_down(link);
	}
	return 0;
}

int link_set_flags(struct link *link,unsigned int flags,int up) {
	if(link == NULL) {
		error("link_set_flags: link == NULL");
		return 0;
	}
	
	if(up) {
		link->flags |= flags;
	}else{
		link->flags &= ~flags;
	}
	return 0;
}

unsigned int link_get_flags(struct link *link) {
	if(link == NULL) {
		error("link_get_flags: link == NULL");
		return 0;
	}
	return link->flags;
}

int link_is_up(struct link *link) {
	if(link == NULL) {
		error("link_is_up: link == NULL");
		return 0;
	}
	
	return (link->flags & LF_UP);
}

void link_dump(struct link *link) {
	char address[20];
	char netmask[20];
	char broadcast[20];

	ipaddr2str(address,link->address);
	ipaddr2str(netmask,link->netmask);
	ipaddr2str(broadcast,link->broadcast);
	if(!(link->flags & LF_UP)) {
		log_msg("Link %s: down",link->iface);
	}else if(link->flags & LF_VIRTUAL) {
		log_msg("Link %s: %s/%s virtual up",
			link->iface,
			address,netmask);
	}else if(link->flags & LF_POINTOPOINT){
		log_msg("Link %s: %s/%s ptp %s up",
			link->iface,
			address,netmask,broadcast);
	}else{
		log_msg("Link %s: %s/%s brd %s up",
			link->iface,
			address,netmask,broadcast);
	}
}

int linklist_add(struct link *link) {
	if(link == NULL) {
		error("linklist_add: link == NULL");
		return -1;
	}
	
	link_list.prev->next = link;
	link->next = &link_list;
	link->prev = link_list.prev;
	link_list.prev = link;
	
	return 0;
}

void linklist_remove(struct link *link) {
	if(link == NULL) {
		error("linklist_remove: link == NULL");
		return;
	}

	link->next->prev = link->prev;
	link->prev->next = link->next;
}

int linklist_update() {
	struct link *p;
	int updated=0;
	
	p = link_list.next;
	while(p != &link_list) {
		// Virtual links are always up and running
		if(p->flags & LF_VIRTUAL) {
			p = p->next;
			continue;
		}
		
		if(link_update(p) < 0) {
			link_set_down(p);
		}
		p = p->next;
	}
	return updated;
}

void linklist_shutdown() {
	struct link *p;
	
	while(link_list.next != &link_list) {
		p = link_list.next;
		linklist_remove(link_list.next);
		link_destroy(p);
	}
}

struct link *linklist_link_for_addr(unsigned int address) {
	struct link *p;
	
	for(p=link_list.next;p != &link_list;p = p->next) {
		if(!(p->flags & LF_UP))
			continue;
		if(p->flags & LF_POINTOPOINT) {		
			if((address == p->address) || (address == p->broadcast))
				return p;
		}else{
			if((address & p->netmask) == (p->address & p->netmask))
				return p;
		}
	}	
	return NULL;
}

struct link *linklist_link_for_iface(char *ifname) {
	struct link *p;
	
	for(p=link_list.next;p != &link_list;p = p->next) {
		if(!strcmp(p->iface,ifname))
			return p;
	}	
	return NULL;
}

/*
 * Add multicast membership for given group on all links
 *
 */
int linklist_enable_multicast(int sockfd,struct sockaddr_in *multiaddr)
{
	struct link *p;
	struct ip_mreq mreq;
	
	for(p=link_list.next;p != &link_list;p = p->next) {
		if(!(p->flags & (LF_MULTICAST|LF_UP)))
			continue;
	
#ifdef DEBUG	
		log_msg("Adding multicast membership on %s",p->iface);
#endif

		/* set multicast */
		memset(&mreq, 0, sizeof(mreq));
		mreq.imr_multiaddr.s_addr = multiaddr->sin_addr.s_addr;
		mreq.imr_interface.s_addr = htonl(p->address);
		if(setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
			error("Unable to add multicast membership on %s: %s",p->iface,strerror(errno));
			return -1;
		}
	}	
	
	return 0;
}

/*
 * Drop multicast membership for given group on all links
 *
 * In fact, i don't like function that return values
 */
void linklist_disable_multicast(int sockfd,struct sockaddr_in *multiaddr)
{
	struct link *p;
	struct ip_mreq mreq;
	
	for(p=link_list.next;p != &link_list;p = p->next) {
		if(!(p->flags & LF_MULTICAST))
			continue;
	
#ifdef DEBUG	
		log_msg("Dropping multicast membership on %s",p->iface);
#endif

		/* drop multicast */
		memset(&mreq, 0, sizeof(mreq));
		mreq.imr_multiaddr.s_addr = multiaddr->sin_addr.s_addr;
		mreq.imr_interface.s_addr = htonl(p->address);
		if(setsockopt(sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
			error("Unable to drop multicast membership on %s: %s",p->iface,strerror(errno));
		}
	}		
	return;
}
