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

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "router.h"
#include "util.h"
#include "link.h"
#include "ctlfile.h"

int parse_link(const char *linkname) {
	char buf[256];
	char ifname[19];
	int cost;
	int announcethis;
	int announceto;
	int announcedef;
	unsigned int flags;
	struct link *link;
	
	get_param_a((char*)linkname,"protocol","",buf,sizeof(buf));
	if(strcmp(buf,"rip2")) {
		return 0;
	}
	
	get_param_a((char*)linkname,"interface","",buf,sizeof(buf));
	if(!strcmp(buf,"")) {
		error("Interface for link %s not specified",linkname);
		return -1;
	}

	strNcpy(ifname,buf,sizeof(ifname));
	
	cost = get_param_i((char*)linkname,"cost",1);
	
	announcethis = get_param_b((char*)linkname,"announcethis",0);
	
	announceto = get_param_b((char*)linkname,"announceto",0);
	
	announcedef = get_param_b((char*)linkname,"announcedef",0);
	
	flags = 0;
	if(announcethis) flags |= LF_ANNOUNCETHIS;
	if(announceto) flags |= LF_ANNOUNCETO;
	if(announcedef) flags |= LF_ANNOUNCEDEF;
	
	if((link = link_create(ifname,0)) == NULL) {
		return -1;
	}
	
	link_set_cost(link,(unsigned short)cost);
	link_set_flags(link,flags,1);
	
	linklist_add(link);
	link_dump(link);
	
	/* Must do like this, coz link_create already calls
	 * fake_linkup, but announce flags arnt set yet,
	 * so no effect...
	 */
	if (link->flags & LF_UP)
		notify_link_up(link);

	return 0;
}

int parse_destination(char *str, unsigned int *addr, unsigned int *mask) {
	char *p;
	char addr_buf[16];
	char mask_buf[16];
	int len;
	unsigned long a,b;

	if((p = strchr(str, '/')) == NULL)
		return -1;		
	
	len = strlen(str);
	memset(addr_buf,0,sizeof(addr_buf));
	memset(mask_buf,0,sizeof(mask_buf));
	strncpy(addr_buf,str,p - str);
	strncpy(mask_buf,p+1,len - (p - str) - 1);
	
	if((a = inet_addr(addr_buf)) == INADDR_NONE)
		return -1;
		
	*addr = ntohl(a);	

	if(strchr(mask_buf,'.')) {
		if((b = inet_addr(mask_buf)) == INADDR_NONE)
			return -1;
		*mask = ntohl(b);
	}else{	
		if(sscanf(mask_buf,"%lu",&b) != 1)
			return -1;
		prefix2mask(b,mask);
	}
		
	return 0;
}

int parse_virtual_link(const char *linkname) {
	char buf[256];
	int cost;
	unsigned int flags;
	struct link *link;
	
	get_param_a((char*)linkname,"protocol","",buf,sizeof(buf));
	if(strcmp(buf,"rip2")) {
		return 0;
	}
	
	get_param_a((char*)linkname,"network","",buf,sizeof(buf));
	if(!strcmp(buf,"")) {
		error("Network for virtual link %s not specified",linkname);
		return -1;
	}
	
	cost = get_param_i((char*)linkname,"cost",1);
	
	flags = LF_ANNOUNCETHIS | LF_VIRTUAL | LF_UP;
	
	if((link = link_create(linkname,1)) == NULL) {
		return -1;
	}

	if(parse_destination(buf,&link->address,&link->netmask) < 0) {
		error("Invalid destination (address/prefix) for link %s specified",linkname);
		return -1;	
	}
	
	link_set_cost(link,(unsigned short)cost);
	link_set_flags(link,flags,1);
	
	linklist_add(link);
	link_dump(link);
	return 0;
}

int parse_links() {
	char buf[256];
	char *linkname;
	
	get_param_a("general","links","",buf,sizeof(buf));
	if(!strcmp(buf,"")) {
		error("No links specified");
		return -1;
	}
	
	linkname = strtok(buf,", ");
	do{
		parse_link(linkname);
	}while((linkname = strtok(NULL,", ")) != NULL);

	// Now read virtual links
	get_param_a("general","virtuallinks","",buf,sizeof(buf));
	if(!strcmp(buf,"")) {
		return 0;
	}
	
	linkname = strtok(buf,", ");
	do{
		parse_virtual_link(linkname);
	}while((linkname = strtok(NULL,", ")) != NULL);
	
	return 0;
}
/*
int parse_configuration_line(const char *line) {
	if(!strncmp(line,"router ",7)) {
		if(!strncmp(line,"rip",3)) {
			if(rip2_init() < 0) {
				error("Unable to initialize RIP2");
				return -1;
			}
			command_target = rip2
			return 0;
		}
	}
	
	if(!strncmp(line,"no router ",10)) {
		if(!strncmp(line,"rip",3)) {
			if(rip2_init() < 0) {
				error("Unable to initialize RIP2");
				return -1;
			}
			command_target = rip2
			return 0;
		}
	}
}*/
