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

#include "util.h"

#include "acl.h"

struct acl_entry access_control_list = { &access_control_list, &access_control_list, };

struct acl_entry *acl_create(const char *spec)
{
	struct acl_entry *acl_entry;
	
	if(spec == NULL) {
		error("link_create: ifname == NULL");
		return NULL;
	}
	
	if((acl_entry = malloc(sizeof(struct acl_entry))) == NULL) {
		error("Unable to allocate memory for acl");
		return NULL;
	}
	
	return acl_entry;
}

void acl_destroy(struct acl_entry *acl_entry)
{
	if(acl_entry == NULL) {
		error("acl_destroy: acl_entry == NULL");
		return;
	}
	free(acl_entry);
}

void acl_dump(struct acl_entry *acl_entry)
{
	if(acl_entry == NULL) {
		error("acl_destroy: acl_entry == NULL");
		return;
	}
}

int acl_add(struct acl_entry *acl_entry)
{
	if(acl_entry == NULL) {
		error("acl_remove: acl_entry == NULL");
		return -1;
	}
	
	access_control_list.prev->next = acl_entry;
	acl_entry->next = &access_control_list;
	acl_entry->prev = access_control_list.prev;
	access_control_list.prev = acl_entry;
	
	return 0;
	
}

void acl_remove(struct acl_entry *acl_entry)
{
	if(acl_entry == NULL) {
		error("acl_remove_remove: acl_entry == NULL");
		return;
	}

	acl_entry->next->prev = acl_entry->prev;
	acl_entry->prev->next = acl_entry->next;	
}

void acl_shutdown()
{
	struct acl_entry *p;
	
	while(access_control_list.next != &access_control_list) {
		p = access_control_list.next;
		acl_remove(access_control_list.next);
		acl_destroy(p);
	}
}

unsigned int acl_check(unsigned int src,unsigned int dst)
{
	struct acl_entry *p;

	for(p=access_control_list.next;p != &access_control_list;p = p->next) {
		if( ((src & p->srcmask) == (p->src & p->srcmask)) &&
		    ((dst & p->dstmask) == (p->dst & p->dstmask)))
			return p->action;
	}	
	return AF_REJECT;
}
