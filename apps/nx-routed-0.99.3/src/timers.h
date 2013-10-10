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

#ifndef _TIMERS_
#define _TIMERS_

#include <time.h>

struct timer {
	/* Forward and back links */
	struct timer *next;
	struct timer *prev;

	/* A primary key */
	time_t timestamp;
	void (*handler)(void *arg);
	void *arg;
	
	int refcount;
};

extern struct timer *timers;

int set_timer(void (*handler)(void *arg),void *arg,time_t timeout);
int get_next_timeout(time_t *next_timeout);
void call_handler();

void timer_addref(struct timer *timer);
void timer_release(struct timer *timer);

struct timer *
timerlist_find(
	void (*handler)(void *arg),
	void *arg,
	time_t timeout);
int timerlist_add_timer(struct timer *timer);
void timerlist_remove_timer(struct timer *timer);
void timerlist_shutdown();
void timerlist_dump();

#endif //_TIMERS_
