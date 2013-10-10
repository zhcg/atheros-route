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
#include <string.h>
#include <time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <time.h>

#include "ctlfile.h"
#include "router.h"
#include "socket.h"
#include "timers.h"
#include "link.h"
#include "util.h"

#include "rip.h"

struct rip2_header {
	unsigned char command;
	unsigned char version;
	unsigned short zero;
};

struct rip2_rte {
	unsigned short family;
	unsigned short rtag;
	unsigned int address;
	unsigned int netmask;
	unsigned int nexthop;
	unsigned int metric;
};

struct rip2_auth {
    u_int16_t xffff;
    u_int16_t type;
    char password[16];
};

extern struct rproto rip2_proto;

struct rtable *rip2_announce_table;
struct socket *rip2_socket;
int rip_version=2;
int rip2_auth=0;
int rip2_multicast=0;
int rip2_unicast=0;
int rip2_allowqueries=1;
int rip2_silent=0;
unsigned int rip2_nextpartupd=0;
unsigned int rip2_nextfullupd=0;
unsigned int rip2_lastrequest_time = 0;
unsigned int rip2_lastrequest_count = 0;
static struct sockaddr_in unicast_dst;
static int unicast_dst_valid = 0;

struct rtable *rip2_calculate_partial_rtable(struct rtable *announce_table) {
	struct rtable *rtable;
	struct route *p,*q;
	
	rtable = new_rtable();
	p = announce_table->routes;
	
	while(p) {
		if(p->flags & RF_CHANGED) {
			p->flags &= ~RF_CHANGED;
			q = dup_route(p);
			add_route(rtable,q);
		}
		p = p->next;
	}
	
	return rtable;
}

void rip2_announce(void *arg) {
	struct rproto *rproto = (struct rproto *)arg;
#ifdef PARTIAL_UPDATES	
	struct rtable *partial_rtable;
#endif
	
	if(rproto == NULL)
		return;	

	// Prevent partial update now (not completed for this version)	
	/* this is madness.. thousends of timers!
	rip2_nextpartupd = 0;
	rip2_nextfullupd = time(NULL) + RIP_SUPPLYTIME;
	set_timer(&rip2_announce,&rip2_proto,RIP_SUPPLYTIME);
	*/
	/* I loose 1 whole second somewhere, and because of that, the rip2_announce
	 * timer doesn't get set again.. But where??
	 * HACK: Add 1 second tolerance (thats that -1 for q;))
	 */
	if ((rip2_nextfullupd-1) <= time(NULL)) {
		/* this was not a triggerd update (but regular 30 sec), thou shall repeate it in time again */
		rip2_nextpartupd = 0;
		rip2_nextfullupd = time(NULL) + RIP_SUPPLYTIME;
		set_timer(&rip2_announce,&rip2_proto,RIP_SUPPLYTIME);
	}
	if(rip2_nextpartupd == 0) {
		announce_routes(rproto,rip2_announce_table);
		return;
	}
#ifdef PARTIAL_UPDATES
	partial_rtable = rip2_calculate_partial_rtable(rip2_announce_table);
	announce_routes(rproto,partial_rtable);
	free_rtable(partial_rtable);	
#endif
	rip2_nextpartupd = 0;
}

int rip2_init_config()
{
	rip2_multicast = get_param_b("rip2","multicast",0);
	rip2_silent = get_param_b("rip2","silent",0);	
	rip2_allowqueries = get_param_b("rip2","allowqueries",1);
	return 0;
}

int rip2_init_protocol(struct rproto *rproto) {
	if(rip2_silent)
		log_msg("Silent mode on");
	rip2_announce_table = new_rtable();
	return 0;
}

int rip2_init() {	
	struct sockaddr_in	sin;
	struct servent		*svp;
	struct socket		*socket;
	
	/*
	 *	Ищем нужный порт
	 */
	svp = getservbyname ("router", "udp");
	if (svp != (struct servent *) 0)
		rip2_proto.port = ntohs(svp->s_port);

	// Initializing configuration files
	if(rip2_init_protocol(&rip2_proto) < 0) {
		error("Unable to initialize RIP2");
		return -1;
	}
	
	// Регистрируем протокол
	if(register_routing_protocol(&rip2_proto) < 0) {
		error("Unable to register RIP2 routing protocol");
		return -1;
	}
	
	if((socket = socket_create(&rip2_proto)) == NULL) {
		error("Unable to create RIP2 socket");
		return -1;
	}
	
	if(socket_bind_to_local_port(socket,rip2_proto.port) < 0) {
		error("Unable to bind socket to local address");
		return -1;
	}
	
	if(rip2_multicast) {
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = htonl(RIP_MCAST);
		sin.sin_port = htons(rip2_proto.port);
	
		if(socket_bind_to_multicast(socket,&sin) < 0) {
			error("Unable to set multicast membership");
			return -1;
		}	
	}
	
	if(socketlist_add(socket) < 0) {
		error("Unable to register RIP2 socket");
		return -1;
	}
	
	// Ставим таймер на анонсы
	// Full update will be in 3 seconds
	rip2_nextfullupd = time(NULL) + 3;
	if(set_timer(&rip2_announce,&rip2_proto,3) < 0) {
		error("Unable to register RIP2 broadcasting timer");
		return -1;		
	}
	
	// Ну теперь держите меня семеро :-)
	rip2_socket = socket;
	
	return 0;
}

void rip2_trigger_update(struct rproto *rip2) {
	time_t now;
	unsigned int delay;
	
	time(&now);
	
	if(unicast_dst_valid)
	{
		rip2_nextpartupd = 0;
		rip2_announce(rip2);
		return;
	}
	// See, wether there was a partial update?
	if(rip2_nextpartupd != 0) {
		// Was. Should we do a partial update if
		// there is full update soon?
		if(rip2_nextpartupd + 2 >= rip2_nextfullupd) {
			rip2_nextpartupd = 0;
			return;
		}

		// Set timer on patial update
		if(set_timer(&rip2_announce,rip2,rip2_nextpartupd - now) < 0) {
			error("Unable register RIP2 broadcasting timer");
			return;
		}	
		return;
	}

	// Частичного анонса вообще не было
	// Смотрим, стоит ли его делать,
	// если скоро полный анонс
	if(now + 2 >= rip2_nextfullupd) {
		rip2_nextpartupd = 0;
		return;
	}
	
	// Рассчитаем время молчания после анонса
	delay = 1 + (rand() * 4 / RAND_MAX);
	rip2_nextpartupd = now + delay;
	
	// Теперь делаем анонс
	rip2_announce(rip2);
}

void rip2_shutdown(struct rproto *rip2) {
	free_rtable(rip2_announce_table);
}

int rip2_learn_routes(struct rproto *rip2,struct rtable *rtable) {
	if(debuglevel < 8) {
		log_msg("rip2_learn_routes called");
		dump_rtable(rtable);
	}
	
	copy_rtable(rip2_announce_table,rtable);
	rip2_trigger_update(rip2);
	
	if(debuglevel < 7) {
		log_msg("rip2_announce_table");
		dump_rtable(rip2_announce_table);
	}
	return 0;
}

int rip2_unlearn_routes(struct rproto *rip2,struct rtable *rtable) {
	if(debuglevel < 8) {
		log_msg("rip2_unlearn_routes called");
		dump_rtable(rtable);
	}
	
	discard_rtable(rip2_announce_table,rtable);
	rip2_trigger_update(rip2);
	
	if(debuglevel < 7) {
		log_msg("rip2_announce_table");
		dump_rtable(rip2_announce_table);
	}		
	return 0;
}

int rip2_validate_source(struct sockaddr_in *origin, struct link *link) {
	unsigned int address;	
	address = ntohl(origin->sin_addr.s_addr);
	
	if((address == 0) || (IN_BADCLASS(address))) {
		debug(5,"Discarded packet from martial source");
		return -1;
	}
		
	if(((address & IN_CLASSA_NET) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET) {
		debug(5,"Discarded packet from loopback network");	
		return -1;
	}
	
	if(address == link->address) {
		debug(1,"Discarded packet from us (or spooffed address?)");
		return -1;
	}
		
	return 0;	
}

int rip2_validate_header(struct rip2_header *header) {
	if(header == NULL) {
		error("rip2_validate_header: header == NULL");
		return -1;
	}

	if((header->command != RIP_REQUEST) && (header->command != RIP_REPLY)) {
		debug(5,"Discarded packet with invalid command");
		return -1;
	}
	
	if((header->version != RIP_V2) && (header->version != RIP_V1)) {
		debug(5,"Discarded packet with invalid version");
		return -1;
	}
	
	return 0;
}

int rip2_validate_rte(struct rip2_rte *rte) {
	if(rte == NULL) {
		error("rip2_validate_rte: rte == NULL");
		return -1;
	}
	
	if(ntohs(rte->family) != AF_INET) {
		debug(5,"Skipped RTE with invalid family");
		return -1;
	}
	
	if((ntohl(rte->metric) > RIP_INFINITE) || (ntohl(rte->metric) == 0)) {
		debug(5,"Skipped RTE with invalid metric");
		return -1;
	}

	return 0;
}

int rip2_authenticate(struct rip2_auth *auth, struct sockaddr_in *origin, struct link *link) {
	return 0;
}

int rip2_process_request(struct rproto *rip2,void *buf,unsigned int len, struct sockaddr_in *origin) {
	int num_entries;
	struct rip2_header *header;
	struct rip2_rte *rte;
	
	if(!rip2_allowqueries)
		return 0;
	
	num_entries = (len - sizeof(struct rip2_header)) / sizeof(struct rip2_rte);
	header = (struct rip2_header*)buf;
	rte = (struct rip2_rte*)(header+1);
	
	if((num_entries != 1) || (ntohs(rte->family) != 0) || (ntohl(rte->metric) != 16)) {
		// Пока поддерживаем только запросы всей таблицы
		return -1;
	}	
	
	// Проверим, не долбят ли нас запросами (козлы)
	// Хмм, неплохо бы это делать во всех случаях (см. выше)
	if(rip2_lastrequest_time + RIP_REQUEST_DELAY > time(NULL)) {
		if(rip2_lastrequest_count > RIP_REQUEST_LIMIT) {		
			error("Discarding annoying requests. Possible attack?");
			//error("Нас бомбят RIP-запросами!!!");
			return -1;
		}
	}else{
		rip2_lastrequest_count = 0;
		rip2_lastrequest_time = time(NULL);
	}
	
	rip2_lastrequest_count++;

	unicast_dst_valid = 1;
	memcpy(&unicast_dst, origin, sizeof(unicast_dst));
	debug(5,"set unicast dest, %lx:%u"
	        , ntohl(unicast_dst.sin_addr.s_addr), ntohs(unicast_dst.sin_port));

	rip2_trigger_update(rip2);
	return 0;
}

int rip2_input_packet(struct rproto *rip2,void *buf,unsigned int len, struct sockaddr *from, int fromlen) {
	int i,num_entries;
	int authenticated;
	struct rip2_header *header;
	struct rip2_rte *rte;
	struct link *link;
	struct rtable *learn_table,*unlearn_table;
	struct route *route;	
	struct sockaddr_in *origin = (struct sockaddr_in*)from;
#ifdef DEBUG2	
	char saddress[16];
	char snetmask[16];
	char snexthop[16];
#endif	

	if((link = linklist_link_for_addr(ntohl(origin->sin_addr.s_addr))) == NULL) {
		// This should reported carefuly
		// Beware of hackers :-)
		error("RIP2: Discarded packet from unknown link");
		return -1;
	}
	
	if(rip2_validate_source(origin,link) < 0)
		return -1;
		
	if(rip2_validate_header((struct rip2_header*)buf) < 0)
		return -1;
	
	num_entries = (len - sizeof(struct rip2_header)) / sizeof(struct rip2_rte);
	header = (struct rip2_header*)buf;
	rte = (struct rip2_rte*)(header+1);
	
#ifdef DEBUG2
	log_msg("RIP2: Received packet from link %s",link->iface);
#endif	
	if(header->command == RIP_REQUEST)
		return rip2_process_request(rip2,buf,len,origin);
	
	if(ntohs(origin->sin_port) != RIP_PORT) {
		error("RIP2: Ignored response from non-rip port %d",ntohs(origin->sin_port));
		return -1;
	}
	
#ifdef DEBUG2
	log_msg("RIP2: cmd=%d, ver=%d",header->command,header->version);
#endif	
	i = 0;
	authenticated = 0;
	if(ntohs(rte->family) == 0xFFFF) { // Authentication
		if(rip2_authenticate((struct rip2_auth*)rte,origin,link) == 0) {
			authenticated = 1;
		}
		i++,rte++;
	}
	if(rip2_auth && !authenticated) {
		error("RIP-2 authentication failed");
		return -1;
	}
	
	learn_table = new_rtable();
	unlearn_table = new_rtable();	

	for(;i<num_entries;i++,rte++) {
		if(rip2_validate_rte(rte) < 0)
			continue;
	
		if(rte->nexthop == 0) {	
			rte->nexthop = origin->sin_addr.s_addr;
		}
		
		if((route = malloc(sizeof(struct route))) == NULL) {
			error("RIP2: Insufficient memory allocating route");
			break;
		}
		
		strncpy(route->iface,link->iface,sizeof(route->iface));
		route->dst = ntohl(rte->address);
		route->dstmask = ntohl(rte->netmask);
		route->nexthop = ntohl(rte->nexthop);
		route->metric = min((int)ntohl(rte->metric) + (int)link->cost,RIP_INFINITE);
		route->source = ntohl(origin->sin_addr.s_addr);
		route->source_link = link;
		route->type = RT_DYNAMIC;
		route->flags = RF_CHANGED;
		route->domain = 0;
		route->timerset = 0;
		
		if(route->metric == RIP_INFINITE) {
			route->expire = 0;
			route->garbage = time(NULL) + RIP_GARBAGE_TIME;
			add_route(unlearn_table,route);
			route->flags = RF_UNREACH;
		}else{
			route->expire = time(NULL) + RIP_EXPIRE_TIME;
			route->garbage = 0;
			add_route(learn_table,route);
		}
#ifdef DEBUG2
		ipaddr2str(saddress,ntohl(rte->address));
		ipaddr2str(snetmask,ntohl(rte->netmask));
		ipaddr2str(snexthop,ntohl(rte->nexthop));
		
		log_msg("RIP2: addr=%s, mask=%s, nh=%s, metric=%d",
			saddress,snetmask,snexthop,ntohl(rte->metric));
#endif			
	}
	
	if(learn_table->num_routes != 0)
		learn_routes(rip2,learn_table);
	if(unlearn_table->num_routes != 0)
		unlearn_routes(rip2,unlearn_table);
	
	free_rtable(learn_table);
	free_rtable(unlearn_table);
	
	return 0;
}

int rip2_output_packet(struct rproto *rip2,struct rtable *rtable,struct link *link) {
	void *packet;
	int len;
	struct rip2_header *header;
	struct rip2_rte *rte;
	struct route *route;
	int num_routes,num_entries;
	struct sockaddr_in dest;
	
	packet = malloc(sizeof(struct rip2_header) + RIP_MAX_ENTRIES*sizeof(struct rip2_rte));
	if(packet == NULL)
		return -1;
	
	dest.sin_family = AF_INET;
	dest.sin_port = htons(RIP_PORT);
	
	route = rtable->routes;
	num_routes = rtable->num_routes;
	
	while(num_routes > 0) {
		num_entries = min(num_routes,RIP_MAX_ROUTES);
		num_routes -= num_entries;
		
		if(rip2_auth)
			num_entries++;
	
		header = (struct rip2_header *)packet;
		header->command = RIP_REPLY;
		header->version = RIP_V2;
		header->zero = 0;
		
		len = sizeof(struct rip2_header);
		
		rte = (struct rip2_rte*)(header+1);
		
		while(num_entries--) {
			rte->family = htons(AF_INET);
			rte->rtag = 0;
			rte->address = htonl(route->dst);
			rte->netmask = htonl(route->dstmask);
			rte->nexthop = htonl(0);
			rte->metric = htonl(route->flags & RF_UNREACH ? RIP_INFINITE : route->metric);
			//rte->metric = htonl(route->metric);
			
			len += sizeof(struct rip2_rte);
			route = route->next;
			rte++;
		}
		
		if(!rip2_silent) {
			if(unicast_dst_valid)
			{
				memcpy(&dest, &unicast_dst, sizeof(dest));
				unicast_dst_valid = 0;
			}
			else
				dest.sin_addr.s_addr = htonl(link->broadcast);

			socket_send_packet(rip2_socket,packet,len,&dest);
		}
	}
	
	free(packet);

	return 0;
}

//
// Send out a broadcast or multicast request to upcoming link
//
void rip2_send_request(struct rproto *rip2,struct link *link) {
	void *packet;
	int len;
	struct rip2_header *header;
	struct rip2_rte *rte;
	struct sockaddr_in dest;
	
	packet = malloc(sizeof(struct rip2_header) + sizeof(struct rip2_rte));
	if(packet == NULL)
		return;
	
	dest.sin_family = AF_INET;
	dest.sin_port = htons(RIP_PORT);
	
	header = (struct rip2_header *)packet;
	header->command = RIP_REQUEST;
	header->version = RIP_V2;
	header->zero = 0;
		
	len = sizeof(struct rip2_header);
		
	rte = (struct rip2_rte*)(header+1);

	// RFC says there should be family=0 and metric=16
	rte->family = 0;
	rte->rtag = 0;
	/*
	 * Imagine yourself a machine, in which decimal zero represents
	 * not as a sequance of binary zeroes... :)
	 * What is this? Paranoia?
	 */
	rte->address = htonl(0);
	rte->netmask = htonl(0);
	rte->nexthop = htonl(0);
	rte->metric = RIP_INFINITE;
		
	if(!rip2_silent) {
		dest.sin_addr.s_addr = htonl(link->broadcast);
		socket_send_packet(rip2_socket,packet,len,&dest);
	}
	
	free(packet);

	return;
}

//
// This ad hoc is first thing i can do after reengineering
// Somebody test this!!!
//
void rip2_link_up(struct rproto *rip2,struct link *link) {	
	rip2_send_request(rip2,link);
}

void rip2_link_down(struct rproto *rip2,struct link *link) {
}

struct rproto_operations rip2_ops = {
	rip2_learn_routes,
	rip2_unlearn_routes,
	rip2_input_packet,
	rip2_output_packet,
	NULL,
	rip2_shutdown,
	rip2_link_up,
	rip2_link_down
};

struct rproto rip2_proto = {
	"RIP-2",
	PF_INET,
	SOCK_DGRAM,
	4,
	IPPROTO_UDP,
	RIP_PORT,
	&rip2_ops,
	NULL
};
