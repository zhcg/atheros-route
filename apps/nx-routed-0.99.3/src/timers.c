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

#include <stdlib.h>
#include <time.h>
#include <limits.h>

#include "util.h"

#include "timers.h"

struct timer timer_list = { &timer_list, &timer_list, 0, };

int set_timer(void (*handler)(void *arg),void *arg,time_t timeout) {
	struct timer *timer;
	
	if((timer = malloc(sizeof(struct timer))) == NULL)
		return -1;

	timer->timestamp = time(NULL) + timeout;
	timer->handler = handler;
	timer->arg = arg;
	timer->refcount = 0;
		
	if(timerlist_add_timer(timer) < 0) {
		free(timer);
		return -1;
	}
	
	return 0;
}

int get_next_timeout(time_t *next_timeout) {
	struct timer *p,*minp;
	time_t min = INT_MAX;

	if((next_timeout == NULL) || (timer_list.next == &timer_list))
		return -1;
		
	*next_timeout = 0;
	
	minp = NULL;
	p = timer_list.next;
	while(p != &timer_list) {
		if(p->timestamp < min) {
			min = p->timestamp;
			minp = p;
		}
		p = p->next;	
	}
	if(minp == NULL)
		return -1;
		
	if(min < time(NULL))
		return 0;

	*next_timeout = min - time(NULL);
	
	return 0;
}

void call_handler() {
	struct timer *timer,*p;
	time_t min=INT_MAX;
	
	timer = NULL;
	p = timer_list.next;
	while(p != &timer_list) {
		if(p->timestamp < min) {
			min = p->timestamp;
			timer = p;
		}
		p = p->next;
	}
	if(timer == NULL)
		return;
		
	/* This sould actualy never happen, but
	 * I think select isnt always acurate...
	 * Altho there could be a bug in some other part
	 * of the code q:) -Z
	 */
	if (timer->timestamp>time(NULL)) {
		error("Tried to call timer %x before it should be called", timer->handler);
		return;
	}
	timer_release(timer);	

	(timer->handler)(timer->arg);

	free(timer);
}

void timer_addref(struct timer *timer)
{
	if(timer == NULL) {
		error("timer_addref: timer == NULL");
		return;
	}
	timer->refcount++;
}

void timer_release(struct timer *timer)
{
	if(timer == NULL) {
		error("timer_release: timer == NULL");
		return;
	}
	timer->refcount--;
	if(timer->refcount <= 0) {
		timerlist_remove_timer(timer);
	}
}

int timerlist_add_timer(struct timer *timer)
{
	if((timer == NULL) || (timer == &timer_list))
		return -1;
	
	timer_list.prev->next = timer;
	timer->next = &timer_list;
	timer->prev = timer_list.prev;
	timer_list.prev = timer;
	
	return 0;
}

void timerlist_remove_timer(struct timer *timer)
{
	if((timer == NULL) || (timer == &timer_list))
		return;
		
	timer->next->prev = timer->prev;
	timer->prev->next = timer->next;
}

struct timer *
timerlist_find(
	void (*handler)(void *arg),
	void *arg,
	time_t timeout)
{
	struct timer *p;
	
	p = timer_list.next;
	
	while(p != &timer_list) {
		if((p->timestamp == timeout) &&
		   (p->handler == handler) &&
		   (p->arg == arg))
			return p;
		p = p->next;
	}
	return NULL;
}

void timerlist_shutdown() {
	struct timer *p,*next;
	
	p = timer_list.next;
	while(p != &timer_list) {
		next = p->next;
		free(p);
		p = next;
	}
}

void timerlist_dump() {
	int i=1;
	struct timer *p;
	
	p = timer_list.next;
	while(p != &timer_list) {
		log_msg("Timer %d -> T:%d(%d), H:%x",i,p->timestamp,p->timestamp-time(NULL),p->handler);
		p = p->next;
		i++;
	}
}
