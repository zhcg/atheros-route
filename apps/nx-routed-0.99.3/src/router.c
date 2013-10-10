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
#include <string.h>

#include <time.h>

#include "router.h"
#include "socket.h"
#include "util.h"
#include "timers.h"

#ifdef LINUX
#include "sys_linux.h"
#endif

struct rproto *routing_protocols;
struct rtable *main_rtable;
int last_rtimeout = INT_MAX;

int register_routing_protocol(struct rproto *rproto) {
	struct rproto *p;
	
	p = routing_protocols;
	while(p) {
		if(!strcmp(p->name, rproto->name))
			return -1;
		p = p->next;
	}
	
	rproto->next = routing_protocols;
	routing_protocols = rproto;
	return 0;
}

void unregister_routing_protocol(struct rproto *rproto) {
	struct rproto *p,*next;
	
	if(routing_protocols != NULL) {	
		if(!strcmp(routing_protocols->name, rproto->name)) {
			routing_protocols = routing_protocols->next;
			return;
		}
	}	
	
	p = routing_protocols;
	while(p) {
		next = p->next;
		if(!strcmp(next->name, rproto->name)) {
			p->next = next->next;						
			return;
		}
		p = next;
	}
}

void shutdown_routing_protocols() {
	struct rproto *p,*next;
	
	p = routing_protocols;
	while(p) {
		next = p->next;
		if(p->ops->shutdown)
			p->ops->shutdown(p);
		p = next;
	}
}

//
// Routing table operations
//
struct rtable *new_rtable() {
	struct rtable *rtable;
	
	if((rtable = malloc(sizeof(struct rtable))) == NULL)
		return NULL;
	
	rtable->num_routes = 0;
	rtable->routes = NULL;
	return rtable;
}

void free_rtable(struct rtable *rtable) {
	struct route *p,*next;

	if(rtable == NULL)
		return;
		
	p = rtable->routes;
	while(p) {
		next = p->next;
		free(p);
		p = next;
	}

	free(rtable);
}

struct rtable *copy_rtable(struct rtable *tbl1,struct rtable *tbl2) {
	struct route *p,*n;

	if((tbl1 == NULL) || (tbl2 == NULL))
		return NULL;
		
	p = tbl2->routes;
	while(p) {
		if((n = malloc(sizeof(struct route))) == NULL) {
			error("Insufficient memory allocating route");
			return NULL;
		}
		
		memcpy(n,p,sizeof(struct route));
		add_route(tbl1,n);
		p = p->next;
	}
	return tbl1;
}

//
// Удалить все маршруты из таблицы, совпадающие с маршрутами
// из другой таблицы
//
int discard_rtable(struct rtable *rtable, struct rtable *what) {
	struct route *p;

	p = what->routes;
	while(p) {
		discard_route(rtable,p);	
		p = p->next;
	}
	return 0;
}

int add_route(struct rtable *rtable, struct route *route) {
	if((rtable == NULL) || (route == NULL))
		return -1;
	
	route->next = rtable->routes;
	rtable->routes = route;	
	rtable->num_routes++;
		
	return 0;
}

// Bogus function, should remove
int add_default_route(struct rtable *rtable, struct link *link) {
	struct route *route;

	if((rtable == NULL) || (link == NULL))
		return -1;

	if((route = malloc(sizeof(struct route))) == NULL) {
		error("Insufficient memory allocating route");
		return -1;
	}
	route->dst = 0; // Дефолт
	route->dstmask = 0;
	route->nexthop = 0; // Через нас
	route->metric = link->cost;
	route->type = RT_DYNAMIC;
	route->flags = 0;
	route->source = 0;
	route->source_link = link;
	route->rtag = 0;
	route->domain = 0;
	route->expire = 0;
	route->garbage = 0;	
	route->timerset = 0;
			
	route->next = rtable->routes;
	rtable->routes = route;	
	rtable->num_routes++;

	return 0;			
}

int copy_route(struct rtable *rtable, struct route *route) {
	struct route *n;

	if((n = malloc(sizeof(struct route))) == NULL) {
		error("Insufficient memory allocating route");
		return -1;
	}
	memcpy(n,route,sizeof(struct route));
	add_route(rtable,n);
	return 0;
}

struct route *dup_route(struct route *route) {
	struct route *n;

	if((n = malloc(sizeof(struct route))) == NULL) {
		error("Insufficient memory allocating route");
		return NULL;
	}
	memcpy(n,route,sizeof(struct route));
	return n;
}

int remove_route(struct rtable *rtable, struct route *route) {
	struct route *p,*prev;
	
	if((rtable == NULL) || (route == NULL))
		return -1;

	if(rtable->routes == route) {
		p = rtable->routes;
		rtable->routes = rtable->routes->next;
		free(p);
		rtable->num_routes--;
		return 0;
	}
	
	p = rtable->routes->next;
	prev = rtable->routes;
	while(p) {		
		if(p == route) {
			prev->next = p->next;
			free(p);
			rtable->num_routes--;
			return 0;
		}
		prev = p;
		p = p->next;
	}
	
	return -1;
}

//
// Remove routes from table matched with specified
//
//
int discard_route(struct rtable *rtable, struct route *route) {
	struct route *p;

	p = rtable->routes;
	while(p) {
		/* if((p->dst == route->dst) && (p->dstmask == route->dstmask)) { */
		if((p->dst & p->dstmask) == (route->dst & route->dstmask)) {
			remove_route(rtable,p);
			return 0;
		}
		p = p->next;
	}
	return -1;
}

void dump_rtable(const struct rtable *rtable) {
	char dst[20];
	char dstmask[20];
	char nexthop[20];
	struct route *p;

	if(rtable == NULL)
		return;
		
	p = rtable->routes;
	while(p) {
		ipaddr2str(dst,p->dst);
		ipaddr2str(dstmask,p->dstmask);
		ipaddr2str(nexthop,p->nexthop);
		
		if(p->type == RT_STATIC) {
			log_msg("%s/%s via %s s",dst,dstmask,nexthop);
		}else{
			if(p->flags & RF_UNREACH) {
				log_msg("%s/%s via %s unr",dst,dstmask,nexthop);
			}else{
				log_msg("%s/%s via %s (%d)",dst,dstmask,nexthop,p->metric);
			}
		}
		p = p->next;
	}
}

void dump_route(struct route *route) {
	char dst[20];
	char dstmask[20];
	char nexthop[20];

	ipaddr2str(dst,route->dst);
	ipaddr2str(dstmask,route->dstmask);
	ipaddr2str(nexthop,route->nexthop);
		
	//FIXME: Update according to dump_rtable
	log_msg("%s/%s via %s (%d)",dst,dstmask,nexthop,route->metric);
}

struct rtable *parse_rtable(char *str) {
	struct rtable *rtable;
	struct route *route;
	char buf[1024];
	char *dst,*mask;
	int prefix;
	struct in_addr in_addr;

	if(str == NULL)
		return NULL;
		
	if((rtable = new_rtable()) == NULL) {
		error("Insufficient memory allocating route table");
		return NULL;
	}
			
	strncpy(buf,str,sizeof(buf));
		
	dst = strtok(buf,"/");
	mask = strtok(NULL,", ");
	
	if((route = malloc(sizeof(struct route))) == NULL) {
		error("Insufficient memory allocating route");
		return NULL;
	}
	
	if(!inet_aton(dst,&in_addr))
		return NULL;
		
	route->dst = ntohl(in_addr.s_addr);

	if(sscanf(mask,"%d",&prefix) != 1) {	
		if(!inet_aton(mask,&in_addr))
			return NULL;
		route->dstmask = ntohl(in_addr.s_addr);
	}else{
		prefix2mask(prefix,&route->dstmask);
	}
	
	route->metric = 0;
	route->type = RT_STATIC;
	route->flags = 0;
	route->source = RS_CONFIG;
	route->source_link = linklist_link_for_addr(route->dst);
	route->domain = 0;
	route->expire = 0;
	route->garbage = 0;
	route->timerset = 0;
	
	add_route(rtable,route);

	return rtable;
}

/*
 * learn_routes
 *
 * Routing protocol reports a set of routes to be added
 */
int learn_routes(struct rproto *origin,const struct rtable *rtable) {
	struct rtable *add_table,*del_table;
	struct route *p,*q;
	
	if((origin == NULL) || (rtable == NULL)) {
		error("learn_route: origin || rtable == NULL");
		return -1;
	}
	
	if(rtable->num_routes == 0) {
		error("No routes from protocol %s",origin->name);	
		return 0;
	}

	if(debuglevel < 6) {
		log_msg("Learned routes from protocol %s",origin->name);
		dump_rtable(rtable);
	}
	
	add_table = new_rtable();
	del_table = new_rtable();
	
	// Вычисляем разницу между главной таблицей и полученными маршрутами
	for(p = rtable->routes;p;p = p->next) {
		for(q = main_rtable->routes;q;q = q->next) {
			if((p->dst & p->dstmask) == (q->dst & q->dstmask)) {
				// Такой маршрут есть
				// Выясним, что это за маршрут
				if(q->type == RT_STATIC)
					break; // Статический, ничего не меняем
					
				if(p->nexthop == q->nexthop) {
					// Обновим время жизни
					q->expire = p->expire;
					q->timerset = 0;
				}
				
				if((q->flags & RF_UNREACH) && 
                                  !(p->flags & RF_UNREACH)) {
					copy_route(del_table,q);
					copy_route(add_table,p);
					p->flags |= RF_CHANGED;
					break;
				}
				
				// Новый маршрут лучше
				if(q->metric > p->metric) {
					copy_route(del_table,q);
					copy_route(add_table,p);
					// Ставим флаг обновления
					p->flags |= RF_CHANGED;
					break;
				}
				// Новый маршрут хуже
				// Подождем пока старый удалиться
				break;
			}
		}
		if(q == NULL) {
			// There is no such route
			// Такого маршрута нет
			//
			// Проверим, не получен ли маршрут к назначениям,
			// к которым есть маршрут с более короткой маской
			//			
/*			for(q = main_rtable->routes;q;q = q->next) {
				// К дефолтам это не относится
				// Fuck this shit up!				
				if(q->dst == 0)
					continue;
			
				if((p->dst & q->dstmask) == (q->dst & q->dstmask)) {
					log_msg("Announced route");
					dump_route(p);
					log_msg("longer than");
					dump_route(q);
					break;
				}			
			}*/
			
			if(q == NULL) {
				copy_route(add_table,p);
				p->flags |= RF_CHANGED;
			}
		}
	}

	/* Propogate routes to routing protocols if needed */
	{
		struct rproto *p,*next;
		
		if(del_table->num_routes > 0) {
			p = routing_protocols;
			while(p) {
				next = p->next;
				if(p->ops->unlearn_routes)
					p->ops->unlearn_routes(p,del_table);
				p = next;
			}
		}
			
		if(add_table->num_routes > 0) {
			if(debuglevel < 9) {
				log_msg("Destination(s) becomes reachable");
				dump_rtable(add_table);
			}
			p = routing_protocols;
			while(p) {
				next = p->next;
				if(p->ops->learn_routes)
					p->ops->learn_routes(p,add_table);
				p = next;
			}
		}
	}

	discard_rtable(main_rtable,del_table);
	copy_rtable(main_rtable,add_table);
	free_rtable(add_table);
	free_rtable(del_table);
	
	return 0;
}

/*
 * unlearn_routes
 *
 * Routing protocol reports a set of routes to be deleted
 */
int unlearn_routes(struct rproto *origin,const struct rtable *rtable) {
	struct rtable *del_table,*add_table;
	struct route *p,*q;
	
	if(debuglevel < 6) {
		log_msg("Discarded routes by protocol %s",origin->name);
		dump_rtable(rtable);
	}
	
	del_table = new_rtable();
	add_table = new_rtable();
	
	for(p = rtable->routes;p;p = p->next) {
		for(q = main_rtable->routes;q;q = q->next) {
			if((p->dst & p->dstmask) == (q->dst & q->dstmask)) {
				// Route does exist
				// Let's find out what is it
				if(q->type == RT_STATIC)
					break; // Static one, don't change anything
					
				if(p->nexthop == q->nexthop) {
					if(q->flags & RF_UNREACH) {
						/* Route already unreachable */
						/* Update expire time */
						q->expire = p->expire;
						q->timerset = 0;
					}else{
						/* Otherwise remove */
						copy_route(del_table,q);
						copy_route(add_table,p);
						// Set update flag
						p->flags |= RF_CHANGED;
					}
				}
				break;
			}
		}
		if(q == NULL) {
			// There is no such route
			// Starting deletion process
/*			copy_route(add_table,p);
			p->flags |= RF_CHANGED;*/
		}
	}
	
	{
		struct rproto *p,*next;
		
		if(del_table->num_routes > 0) {
			if(debuglevel < 9) {
				log_msg("Destination(s) becomes unreachable");
				dump_rtable(del_table);
			}
			p = routing_protocols;
			while(p) {
				next = p->next;
				if(p->ops->unlearn_routes)
					p->ops->unlearn_routes(p,del_table);
				p = next;
			}
		}
		
		if(add_table->num_routes > 0) {
			p = routing_protocols;
			while(p) {
				next = p->next;
				if(p->ops->learn_routes)
					p->ops->learn_routes(p,add_table);
				p = next;
			}
		}		
	}
	
	discard_rtable(main_rtable,del_table);
	copy_rtable(main_rtable,add_table);
	free_rtable(add_table);
	free_rtable(del_table);
	return 0;
}

int remove_dynamic_routes(struct rtable *rtable) {
	struct route *route,*next;
	struct rtable *remove_table;
	
	remove_table = new_rtable();
	
	for(route = rtable->routes;route;route = next) {
		next = route->next;
		if(route->type == RT_DYNAMIC) {
			copy_route(remove_table,route);
			remove_route(rtable,route);
		}
	}

#ifdef DEBUG	
	log_msg("Removing dynamic routes from system routing table");
	dump_rtable(remove_table);
#endif	
	/*
	 * This is a big hack: socket_list.next could not be a
	 * a good socket for managing routing table. Need to
	 * find aout how to fix this...
	 */
	system_remove_routes(socket_list.next,remove_table);
	
	free_rtable(remove_table);
	
	return 0;
}

void announce_routes(struct rproto *rproto, struct rtable *rtable) {
	struct link *link;
	struct rtable *tmp;
	struct route *route;
	
	if((rproto == NULL) || (rtable == NULL)) {
		error("announce_routes: rproto || rtable == NULL");
		return;
	}
	
	for(link = link_list.next;link != &link_list;link = link->next) {
		/* Don't announce to virtual links
		   since they are virtual */
		if(!(link->flags & LF_UP) || (link->flags & LF_VIRTUAL))
			continue;
			
		if(!(link->flags & LF_ANNOUNCETO))
			continue;

		tmp = new_rtable();		
		
		if(link->flags & LF_ANNOUNCEDEF) {
			add_default_route(tmp,link);
		}
		
		/*
		 * Split Horizon		
		 * Don't send routes to networks
		 * from which they were originated
		 */
		for(route=rtable->routes;route;route=route->next) {
			if(route->source_link == link) {
				continue;
			}
			copy_route(tmp,route);
		}

#ifdef DEBUG	
		log_msg("Announcing routes to link %s",link->iface);
		dump_rtable(tmp);
#endif
		rproto->ops->output_packet(rproto,tmp,link);
		
		free_rtable(tmp);
	}
}

void expire_routes(void *arg) {
	struct rtable *rtable;
	struct rtable *add_table,*del_table;
	struct route *p,*q;
	
	rtable = (struct rtable*)(arg);
	
	if(rtable == NULL) {
		error("expire_routes: rtable == NULL");
		return;
	}
	
	if(rtable->num_routes == 0) {
		error("No routes to expire");	
		return;
	}

	add_table = new_rtable();
	del_table = new_rtable();
	
	for(p = rtable->routes;p;p = p->next) {
		if(p->type == RT_STATIC)
			continue;
			
		p->timerset = 0;
			
		if((p->expire != 0) && (p->expire <= time(NULL))) {
			q = dup_route(p);
			q->flags = RF_UNREACH | RF_CHANGED;
			q->expire = 0;
			/* 
			 * This is hack, should fix it later
			 * FIXME: introduce a field in struct rproto
			 * for default timeouts, and make a link to
			 * struct rproto from struct route
			 * Call it source_rproto
			 */			
			q->garbage = time(NULL) + 120;
			q->timerset = 0;
			copy_route(del_table,p);
			add_route(add_table,q);
			continue;
		}		
		if((p->garbage != 0) && (p->garbage <= time(NULL))) {
			copy_route(del_table,p);
		}		
	}
	
	if((debuglevel < 9) && (del_table->num_routes > 0)) {
		log_msg("Expired routes");
		dump_rtable(del_table);
	}

	if(rtable == main_rtable) {
#ifdef DEBUG
		log_msg("Deleting routes from routing table");
		dump_rtable(del_table);
		log_msg("Inserting routes into routing table");
		dump_rtable(add_table);
#endif
		/* propagate to all protocols
		 * FIXME: This is a hack, need to do
		 * a function or sth (call leatn with rproto
		 * &system_rproto? Anyway, all protocols need
		 * to know witch routes expired
		 */
		{
		struct rproto *p,*next;

		if(del_table->num_routes > 0) {
			p = routing_protocols;
			while(p) {
				next = p->next;
				if(p->ops->unlearn_routes)
					p->ops->unlearn_routes(p,del_table);
				p = next;
			}
		}

		if(add_table->num_routes > 0) {
			p = routing_protocols;
			while(p) {
				next = p->next;
				if(p->ops->learn_routes)
					p->ops->learn_routes(p,add_table);
				p = next;
			}
		}
		}
	}	
	discard_rtable(rtable,del_table);
	copy_rtable(rtable,add_table);

	free_rtable(add_table);
	free_rtable(del_table);
	
	set_expiring_timer(rtable);
	
	return;
}

//
// Set timer for earliest expiring routes
//
int set_expiring_timer(struct rtable *rtable) {
	struct route *p;
	time_t mintime = INT_MAX;
	struct timer *rtimer;
	
	if(rtable == NULL) {
		error("set_expiring_timer: rtable == NULL");
		return -1;
	}
	
	debug(5,"Entring, nafig, set_expiring_timer");

	for(p = rtable->routes;p;p = p->next) {
	
		if(p->type == RT_STATIC) {
#ifdef DEBUG		
			debug(5,"Skipped static route");
			dump_route(p);
#endif
			continue;
		}
			
		if(p->timerset) {
#ifdef DEBUG
			debug(5,"Timer is already set for route");
			dump_route(p);
#endif
			return 0;
		}

#ifdef DEBUG		
		debug(5,"p->expire == %d",p->expire);	
#endif
		if((p->expire != 0) && (p->expire <= mintime)) {
			mintime = p->expire;
			p->timerset = 1;
		}
#ifdef DEBUG
		debug(5,"p->garbage == %d",p->garbage);
#endif
		if((p->garbage != 0) && (p->garbage <= mintime)) {
			mintime = p->garbage;
			p->timerset = 1;
		}
	}
	
	if(mintime == INT_MAX) {
#ifdef DEBUG	
		debug(5,"No routes found to expire");
#endif		
		return 0;
	}
	
	if(mintime < 1000000) {
#ifdef DEBUG
		debug(5,"Min time is lower the limit. Rejecting timer");
#endif		
		return 0;
	}
	
	debug(5,"Route expiration timeout is set to %d",mintime);

#ifdef DEBUG	
	timerlist_dump();
#endif	
	rtimer=timerlist_find(expire_routes,rtable,last_rtimeout);

	/* No timer set.. yet */
	if (!rtimer) {
		debug(5, "No existing timer for expire_routes");
		last_rtimeout = mintime;
		return set_timer(expire_routes,rtable,mintime - time(NULL));
	}
	

	debug(5, "Changing timeout for expire_routes");
	last_rtimeout = mintime;
	rtimer->timestamp = mintime;
	return 0;
}

//
// Initalize timer for route
//
int initalize_timer(struct route *route) {
	void (*handler)(void *arg);
	void *arg;
	time_t timeout;
	struct timer *timer;
	
	if(route == NULL) {
		error("initalize_timer: route == NULL");
		return -1;
	}
	
	if(route->type == RT_STATIC) {
		error("Cannot initalize timer for static route");
		return -1;
	}
	
	if(route->expire != 0) {
		timeout = route->expire;
	}else if(route->garbage != 0) {
		timeout = route->garbage;
	}else return 0;
	
	handler = expire_routes;
	arg = main_rtable;
	
	if((timer = timerlist_find(handler,arg,timeout)) != NULL) {
		timer_addref(timer);
		route->timer = timer;
		return 0;
	}
	
        if(set_timer(handler,arg,timeout) < 0) {
		error("Unable to initalize timer for route");
                dump_route(route);
		return -1;
	}

	if((timer = timerlist_find(handler,arg,timeout)) != NULL) {
		timer_addref(timer);
		route->timer = timer;
		return 0;
	}
        
	error("Timer for route initialized, but not found!!!");
	
	return -1;
}

void notify_link_up(struct link *link) {
	struct rproto *p,*next;

	p = routing_protocols;
	while(p) {
		next = p->next;
		if(p->ops->link_up)
			p->ops->link_up(p,link);
		p = next;
	}
}

void notify_link_down(struct link *link) {
	struct rproto *p,*next;

	p = routing_protocols;
	while(p) {
		next = p->next;
		if(p->ops->link_down)
			p->ops->link_down(p,link);
		p = next;
	}
}
