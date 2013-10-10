/*
---------------------------------------------------------------------------
 platform.h,v 1.10 2004/06/29 15:06:12 jpdionne Exp
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

/* Linux */

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#ifdef _USES_SYS_TIME_H_
#include <sys/time.h>
#define GETTIMEOFDAY(A, B) \
	gettimeofday(A, B)
#endif

#ifdef _USES_NETDB_H_
#include <netdb.h>
#endif

#ifdef _USES_SYS_IOCTL_H_
#include <sys/ioctl.h>
#endif

#ifdef _USES_SYS_SOCKET_H_
#include <sys/socket.h>
#define SOCKET int
#endif

#ifdef _USES_NETINET_IN_H_
#include <netinet/in.h>
#endif

#ifdef _USES_NETINET_IP6_H_
#include <netinet/ip6.h>
#endif

#ifdef _USES_NETINET_ICMP6_H_
#include <netinet/icmp6.h>
#endif

#ifdef _USES_NET_IF_H_
#include <net/if.h>
#endif

#ifdef _USES_ARPA_INET_H_
#include <arpa/inet.h>
#define INET_PTON(A, B, C)\
	inet_pton(A, B, C)
#endif

#ifdef _USES_SYSLOG_H_
#include <syslog.h>
#define SYSLOG(A,B)\
	syslog(A, B)
#define OPENLOG(A,B,C)\
	openlog(A, B, C)
#define CLOSELOG\
	closelog
#endif

#define SLEEP(A)\
	sleep(A)

#define RANDOM \
	rand
#define SRANDOM \
	srand


#define SCRIPT_TMP_FILE "/tmp/tspc-tmp.log"

#endif

