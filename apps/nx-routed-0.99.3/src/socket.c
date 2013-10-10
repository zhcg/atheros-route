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
#include <errno.h>
#include <memory.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <netinet/if_ether.h>
#include <linux/if.h>
#include <linux/sockios.h>

#ifdef LINUX
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#endif

#include "timers.h"
#include "router.h"
#include "util.h"
#include "link.h"

#include "socket.h"

#define READ_BUFFER_SIZE	1024

fd_set readfds,writefds,exceptfds;
int maxfd;

struct socket socket_list = { &socket_list, &socket_list, };

//
// socket_create
//
// Create socket with specified routing protocol 
// and allocate control structure
//
struct socket *socket_create(struct rproto *rproto) {
	struct socket dummy;
	struct socket *s;

        /* Ensure we got sane info */
	if((rproto == NULL) || (rproto->ops == NULL))
		return NULL;
	
	/* Ensure all fields are zero (use later in socket_close) */	
	memset(&dummy,0,sizeof(dummy));	
	
	if((dummy.fdesc = socket(rproto->family,rproto->socket_type,rproto->proto)) < 0) {
		error("Unable to create socket: %s",strerror(errno));
		return NULL;
	}
	
	dummy.rproto = rproto;

        /* Allocate control structure */
	if((s = malloc(sizeof(struct socket))) == NULL) {	
		error("Unable to allocate memory for socket");
		close(dummy.fdesc);
		return NULL;
	}
	
	memcpy(s,&dummy,sizeof(struct socket));
			
	return s;
}

//
// socket_create
//
// Close socket
//
void socket_close(struct socket *socket) {
	if(socket == NULL) {
		error("socket_close: socket == NULL");
		return;
	}
	
	/* drop multicast membership implicitly */
	if(socket->multicast.sin_family != 0) {
		linklist_disable_multicast(socket->fdesc,&socket->multicast);
	}

	close(socket->fdesc);
	socket->fdesc = -1;
}

//
// socket_destroy
//
// Deallocate control structure
// 
void socket_destroy(struct socket *socket) {
	if(socket == NULL) {
		error("socket_destroy: socket == NULL");
		return;
	}
	free(socket);
}


// 
// socket_bind_to_local_port
// 
// Bind socket to local UDP port
// 
int socket_bind_to_local_port(struct socket *socket,unsigned short port) {
	int result;
	struct sockaddr_in sin;
	unsigned long val;
#ifdef DEBUG	
	char buffer[20];
#endif
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);

#ifdef DEBUG	
	ipaddr2str(buffer,ntohl(sin.sin_addr.s_addr));
	log("Binding socket to %s:%d",buffer,port);
#endif

	val = 1;
	if(setsockopt(socket->fdesc, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
		error("Unable to set SO_REUSEADDR: %s",strerror(errno));
		return -1;
	}

	/* set broadcast */
	val = 1;
	if (setsockopt(socket->fdesc, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)) < 0) {
		error("Unable to set SO_BROADCAST: %s",strerror(errno));
		return -1;
	}
	
	result = bind(socket->fdesc,(struct sockaddr*)&sin,sizeof(sin));
	if (result < 0) {	
		error("Unable to bind socket: %s",strerror(errno));
		return -1;
	}
		
	return 0;
}

// 
// socket_bind_to_multicast
// 
// Enable multicasting on socket
// 
int socket_bind_to_multicast(struct socket *socket,struct sockaddr_in *sin) {
	int result;
	unsigned long val;	
	
	if(socket->multicast.sin_family != 0) 
	{
		if((socket->multicast.sin_family != sin->sin_family) &&
		   (socket->multicast.sin_addr.s_addr != sin->sin_addr.s_addr))
		{
			error("Trying add multicast membership on socket while already have!");
			return -1;
		}
	}
	
	/* disable multicast loop */
	val = 0;
	if (setsockopt(socket->fdesc, IPPROTO_IP, IP_MULTICAST_LOOP, &val, sizeof(val)) < 0) {
		error("Unable to disable multicast loop: %s",strerror(errno));
	}	

	/* set multicast ttl to 1 */
	val = 1;
	if (setsockopt(socket->fdesc, IPPROTO_IP, IP_MULTICAST_TTL, &val, sizeof(val)) < 0) {
		error("Unable to set multicast ttl to 1: %s",strerror(errno));
	}

	if((result = linklist_enable_multicast(socket->fdesc,sin)) == 0) {
		socket->multicast = *sin;
	}
	
	return result;
}

// 
// socket_bind_to_netlink
// 
// Bind socket to netlink family to listen for network status update 
// 
#ifdef LINUX
int socket_bind_to_netlink(struct socket *socket) {
	int result;
	struct sockaddr_nl addr;
	
	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_pad = 0;
	addr.nl_pid = getpid();
	addr.nl_groups = 0xFFFFFFFF;

	result = bind(socket->fdesc,(struct sockaddr*)&addr,sizeof(addr));
	if (result < 0) {	
		error("Unable to bind socket: %s",strerror(errno));
		return -1;
	}
		
	return 0;
}
#endif

// 
// socket_send_packet
// 
// Send data to socket (really should enqueue)
// 
int socket_send_packet(struct socket *socket,char *buf,unsigned int len, struct sockaddr_in *dest) {

	if(sendto(socket->fdesc,buf,len,0,(struct sockaddr*)dest,sizeof(struct sockaddr_in)) < 0) {
		error("Unable to send packet: %s",strerror(errno));
		return -1;
	}
		
	return 0;
}

int socket_get_packet(struct socket *socket,char *buf,unsigned int len) {
	return 0;
}

// 
// socket_read
// 
// Read packet from socket and notify routing protocol
// 
int socket_read(struct socket *socket) {
	int res;
	char dummy[READ_BUFFER_SIZE];
	struct sockaddr from;
	int fromlen;

	fromlen = sizeof(from);
	if((res = recvfrom(socket->fdesc,dummy,sizeof(dummy),0,
	    (struct sockaddr*)&from,&fromlen)) <= 0) {
		error("Read error: %s",strerror(errno));
		return -1;
	}
	
	if(socket->rproto->ops->input_packet)
		(socket->rproto->ops->input_packet)(socket->rproto,dummy,res,&from, fromlen);
	return 0;
}


// 
// socketlist_add
// 
// Add new socket to socket list 
// 
int socketlist_add(struct socket *socket) {
	if(socket == NULL)
		return -1;
	
	socket_list.prev->next = socket;
	socket->next = &socket_list;
	socket->prev = socket_list.prev;
	socket_list.prev = socket;
	
	return 0;
}

// 
// socketlist_remove
// 
// Remove socket from socket list
// 
void socketlist_remove(struct socket *socket) {
	if(socket == NULL)
		return;
		
	socket->next->prev = socket->prev;
	socket->prev->next = socket->next;
}

// 
// socketlist_shutdown
// 
// Clear socket list removing all sockets
// 
void socketlist_shutdown() {
	struct socket *socket,*next;

	socket = socket_list.next;
	while(socket != &socket_list) {
		next = socket->next;
		socketlist_remove(socket);
		if(socket->fdesc >= 0) {
			socket_close(socket);
		}
		socket_destroy(socket);
		socket = next;
	}			
}

// 
// socketlist_build_fdsets
// 
// Build fd sets from list of sockets
// 
void socketlist_build_fdsets(void)
{
	struct socket *socket;

	maxfd = -1;
	
	// Clear fd sets
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&exceptfds);
	
	// Fill in fd sets for each socket
	socket = socket_list.next;
	
	while(socket != &socket_list)
	{
		if(socket->fdesc >= 0) {
			if(socket->fdesc > maxfd)
				maxfd = socket->fdesc;
			FD_SET(socket->fdesc,&readfds);
			FD_SET(socket->fdesc,&exceptfds);
		}
		socket = socket->next;
	}
}

// 
// socketlist_wait_event
// 
// Wait for and event to occur on  a list of sockets
// 
int socketlist_wait_event(void)
{
	int retval;
	fd_set rfds,wfds,efds;
	struct timeval tv;
	struct socket *socket,*next;
	
	for(;;)
	{
		// Copy fdsets for multiple use
		//rfds = readfds;
		memcpy(&rfds,&readfds,sizeof(rfds));
		memcpy(&wfds,&writefds,sizeof(wfds));
		memcpy(&efds,&exceptfds,sizeof(efds));
		
		if(get_next_timeout(&tv.tv_sec) < 0)
			tv.tv_sec = 30;
		tv.tv_usec = 0;

		if((tv.tv_sec == 0) && (tv.tv_usec == 0)) {
			// FIXME: This is possible infinite loop
			call_handler();
			return 0;
		}
		
		retval = select(maxfd+1,&rfds,&wfds,&efds,&tv);
		
		if(retval < 0) {
			if(errno == EINTR) {
				if (linkupdatetime)
					return -1;
				else
					return 0;
			}		
			error("Select has returned an error %s",strerror(errno));
			return -1;
		}		
		
		if(retval)
		{
			socket = socket_list.next;
			while(socket != &socket_list)
			{
				if(socket->fdesc < 0) {
					socket = socket->next;
					continue;
				}
				
				if(FD_ISSET(socket->fdesc,&rfds))
				{
					socket_read(socket);
				}
				
				if(FD_ISSET(socket->fdesc,&efds))
				{
				}
				socket = socket->next;
			}
			
			// Remove closed sockets
			socket = socket_list.next;
			while(socket != &socket_list) {
				next = socket->next;				
				if(socket->fdesc < 0) {
					socketlist_remove(socket);
					socket_destroy(socket);
				}
				socket = next;
			}			
		}
	
                // Select has returned timeout
                // call timers now
		if(!retval) {
			call_handler();
			return 0;
		}
	}
}
