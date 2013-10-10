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

#ifndef _ROUTER_
#define _ROUTER_

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <time.h>

#include "link.h"
#include "timers.h"

/* Route type (route->type) */
#define RT_STATIC	0
#define RT_DYNAMIC	1

/* Route source */
#define RS_SYSTEM	0
#define RS_CONFIG	1

/* Route flags (route->flags)*/
#define RF_UNREACH	0x00000001
#define RF_CHANGED	0x00000002

// ˜”≈ ¡ƒ“≈”¡ … Õ¡”À… »“¡Œ…Õ ◊ ËÔÛÙÔ˜ÔÌ ÔÚÒ‰ÎÂ ‚·ÍÙ
struct route {
	unsigned int dst;
	unsigned int dstmask;
	unsigned int nexthop;
	unsigned int metric;
	
	unsigned int source;
	unsigned int type;
	unsigned int flags;
	unsigned short rtag;
	unsigned short domain;
	
	time_t expire;
	time_t garbage;
	struct timer *timer;
	
	unsigned int timerset;
	char iface[16];
	struct link *source_link;
	struct route *next;
};

struct rtable {
	int num_routes;
	struct route *routes;
};

struct rproto;

struct rproto_operations {
	int (*learn_routes)(struct rproto*,struct rtable *);
	int (*unlearn_routes)(struct rproto*,struct rtable *);
	int (*input_packet)(struct rproto*,void *buf,unsigned int len,struct sockaddr *from, int fromlen);
	int (*output_packet)(struct rproto*,struct rtable *,struct link *link);
	void (*announce)(struct rproto*);
	void (*shutdown)(struct rproto*);
	void (*link_up)(struct rproto*,struct link *link);
	void (*link_down)(struct rproto*,struct link *link);
};

struct rproto {
	char *name;
	unsigned int family;
	unsigned int socket_type;
	unsigned char ipversion;
	unsigned int proto;
	unsigned int port;
	struct rproto_operations *ops;
	
	struct rproto *next;
};

extern time_t linkupdatetime;
extern struct rproto *routing_protocols;
extern struct rtable *main_rtable;

void ipaddr2str(char *buffer, unsigned int ipaddr);

int register_routing_protocol(struct rproto *rproto);
void unregister_routing_protocol(struct rproto *rproto);
void shutdown_routing_protocols();

struct rtable *new_rtable(void);
void free_rtable(struct rtable *rtable);
struct rtable *parse_rtable(char *str);
struct rtable *copy_rtable(struct rtable *,struct rtable *);
int discard_route(struct rtable *rtable, struct route *route);
int discard_rtable(struct rtable *rtable, struct rtable *what);
void dump_rtable(const struct rtable *rtable);

int add_route(struct rtable *rtable, struct route *route);
int remove_route(struct rtable *rtable, struct route *route);
int copy_route(struct rtable *,struct route *route);
struct route *dup_route(struct route *route);
void dump_route(struct route *route);

int learn_routes(struct rproto *,const struct rtable *);
int unlearn_routes(struct rproto *,const struct rtable *);
void announce_routes(struct rproto *rproto,struct rtable *);

void notify_link_up(struct link *link);
void notify_link_down(struct link *link);

int remove_dynamic_routes(struct rtable *rtable);

int set_expiring_timer(struct rtable *rtable);

#endif //_ROUTER_

