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

#ifndef _ACL_
#define _ACL_

#define AF_ACCEPT		0x00000000
#define AF_REJECT		0x00000001

struct acl_entry {
	struct acl_entry *next,*prev;
	
	unsigned int src;
	unsigned int srcmask;
	unsigned int dst;
	unsigned int dstmask;
	unsigned int action;
};

extern struct acl_entry access_control_list;

struct acl_entry *acl_create(const char *spec);
void acl_destroy(struct acl_entry *acl_entry);
void acl_dump(struct acl_entry *acl_entry);

int acl_add(struct acl_entry *acl_entry);
void acl_remove(struct acl_entry *acl_entry);
void acl_shutdown();
unsigned int acl_check(unsigned int src,unsigned int dst);

#endif //_ACL_
