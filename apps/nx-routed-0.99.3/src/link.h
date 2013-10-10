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

#ifndef _LINK_
#define _LINK_

#define LF_UP			0x00000001
#define LF_VIRTUAL		0x00000002
#define LF_ANNOUNCETHIS		0x00000004
#define LF_ANNOUNCETO		0x00000008
#define LF_ANNOUNCEDEF		0x00000010
#define LF_UNICAST		0x00000020
#define LF_BROADCAST		0x00000040
#define LF_MULTICAST		0x00000080
#define LF_POINTOPOINT		0x00000100
#define LF_SPLITHORIZON		0x00000200
#define LF_POISONEDREVERSE	0x00000400

struct link {
	struct link *next,*prev;
	
	char iface[16];
	unsigned int address;
	unsigned int netmask;
	unsigned int unicast;
	unsigned int broadcast;
	unsigned int multicast;
	unsigned short cost;
	
	unsigned int flags;
};

extern struct link link_list;

struct link *link_create(const char *ifname,int virt);
int link_update(struct link *link);
void link_destroy(struct link *link);
int link_set_cost(struct link *link,unsigned short cost);
int link_set_up(struct link *link);
int link_set_down(struct link *link);
int link_set_flags(struct link *link,unsigned int flags,int up);
unsigned int link_get_flags(struct link *link);
int link_is_up(struct link *link);
void link_dump(struct link *link);

int linklist_add(struct link *link);
void linklist_remove(struct link *link);
int linklist_update();
void linklist_shutdown();
struct link *linklist_link_for_addr(unsigned int address);
struct link *linklist_link_for_iface(char *ifname);
int linklist_enable_multicast(int sockfd,struct sockaddr_in *multiaddr);
void linklist_disable_multicast(int sockfd,struct sockaddr_in *multiaddr);

#endif //_LINK_
