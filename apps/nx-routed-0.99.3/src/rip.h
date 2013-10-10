/*
 * rip.h	Structures and definitions for simple RIP v1 and v2.
 *
 * Version:	@(#)ripd.h  0.01  05-May-1997  miquels@cistron.nl
 *
 * Version:     @(#)ripd.h  0.03  18-Mar-1999  pal@penza.com.ru
 * Changes:     Added RIPv2 support (not fully tested)
 * 
 * Version:     @(#)rip.h  0.04  25-Sep-2002  valery@penza.com.ru  
 */

#ifndef _RIP_
#define _RIP_

extern int rip_version;

#define FL_CHANGED	1
#define FL_SEEN		2

#define RIP_REQUEST	1
#define RIP_REPLY	2
#define RIP_INFINITE	16
#define RIP_V1		1
#define RIP_V2		2
#define RIP_GARBTIME	90
#define RIP_SUPPLYTIME	30
#define RIP_PORT	520
#define RIP_MCAST	0xE0000009

#define RIP_EXPIRE_TIME		180
#define RIP_GARBAGE_TIME	120

#define RIP_REQUEST_DELAY	5
#define RIP_REQUEST_LIMIT	10

#define RIP_MAX_ROUTES	24
#define RIP_MAX_ENTRIES	25

int rip2_init_config();
int rip2_init();

#endif //_RIP_
