/*
---------------------------------------------------------------------------
 platform.h,v 1.15 2004/07/14 16:42:06 dgregoire Exp
---------------------------------------------------------------------------
* This source code copyright (c) Hexago Inc. 2002-2004.
* 
* This program is free software; you can redistribute it and/or modify it 
* under the terms of the GNU General Public License (GPL) Version 2, 
* June 1991 as published by the Free  Software Foundation.
* 
* This program is distributed in the hope that it will be useful, 
* but WITHOUT ANY WARRANTY;  without even the implied warranty of 
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
* See the GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License 
* along with this program; see the file GPL_LICENSE.txt. If not, write 
* to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
* MA 02111-1307 USA
---------------------------------------------------------------------------
*/

/* Windows */

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <winsock2.h>

#ifdef _USES_SYS_TIME_H_
#include <sys/time.h>
/* tspGetTimeOfDay in windows/tsp_local.c */
int tspGetTimeOfDay(void *);
#define GETTIMEOFDAY(A, B) \
	tspGetTimeOfDay(A)
#endif

#ifdef _USES_NETDB_H_
#endif

#ifdef _USES_SYS_IOCTL_H_
#endif

#ifdef _USES_SYS_SOCKET_H_
#define SHUT_RDWR SD_BOTH	// windows
#include <ws2tcpip.h>
#endif

#ifdef _USES_NETINET_IN_H_
#define SHUT_RDWR SD_BOTH	// windows
#endif

#ifdef _USES_NETINET_IP6_H_
#include <ws2tcpip.h>
#endif

#ifdef _USES_NETINET_ICMP6_H_
#include <ws2tcpip.h>
#endif

#ifdef _USES_NET_IF_H_
#endif

#ifdef _USES_ARPA_INET_H_
/* tspInetPton in windows/tsp_local.c */
int tspInetPton(int af, const char *src, void *dst);
#define INET_PTON(A, B, C) \
	tspInetPton(A, B, C)
#endif

#ifdef _USES_SYSLOG_H_
#define LOG_EMERG       0       /* system is unusable */
#define LOG_ALERT       1       /* action must be taken immediately */
#define LOG_CRIT        2       /* critical conditions */
#define LOG_ERR         3       /* error conditions */
#define LOG_WARNING     4       /* warning conditions */
#define LOG_NOTICE      5       /* normal but significant condition */
#define LOG_INFO        6       /* informational */
#define LOG_DEBUG       7       /* debug-level messages */

#define SYSLOG(A,B)
#define OPENLOG(A,B,C)
#define CLOSELOG()
#endif

typedef unsigned long uint32_t;
typedef unsigned long u_int32_t;
typedef signed long int32_t;

#define __P(protos)     protos          /* full-blown ANSI C */
#define SLEEP(A) \
	Sleep(A*1000)

#define RANDOM \
	rand
#define SRANDOM \
	srand

#define SCRIPT_TMP_FILE "tmp.log"
#define _USING_WINDOWS_UDP_PORT_
int WindowsUDPPort;
#define BROKEN_WINDOWS_RAWSOCK

#endif










