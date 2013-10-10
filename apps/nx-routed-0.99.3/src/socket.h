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

#ifndef _SOCKET_
#define _SOCKET_

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//
// A control structure for socket
//
struct socket {
        // Forward and backward links
	struct socket *prev;
	struct socket *next;

        // File descriptor
	int			fdesc;
	unsigned short		port;
	struct sockaddr_in	unicast;
	struct sockaddr_in	broadcast;
	struct sockaddr_in	multicast;

        // A link to routing protocol it uses
	struct rproto		*rproto;
};

// A base for list of socket
extern struct socket socket_list;

struct socket *socket_create(struct rproto *);
void socket_destroy(struct socket *);

int socket_bind_to_local_port(struct socket *socket,unsigned short port);
int socket_bind_to_multicast(struct socket *socket,struct sockaddr_in *sin);
#ifdef LINUX
int socket_bind_to_netlink(struct socket *socket);
#endif

int socket_send_packet(struct socket *,char *,unsigned int,struct sockaddr_in *);

int socketlist_add(struct socket *socket);
void socketlist_remove(struct socket *socket);
void socketlist_shutdown();
void socketlist_build_fdsets(void);
int socketlist_wait_event(void);

#endif //_SOCKET_

