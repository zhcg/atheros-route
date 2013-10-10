/*
---------------------------------------------------------------------------
 net.h,v 1.7 2004/02/11 04:52:41 blanchet Exp
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

#ifndef _NET_H_
#define _NET_H_

#include <sys/types.h>

struct net_tools {
	SOCKET (*netopen)	(char *, int);
	int (*netclose)	(SOCKET);
	
	int (*netsendrecv)	(SOCKET, unsigned char *, size_t, unsigned char *, size_t);
	
	int (*netsend)		(SOCKET, unsigned char *, size_t);
	int (*netprintf)	(SOCKET, char *, size_t, char *, ...);
	
	int (*netrecv)		(SOCKET, unsigned char *, size_t);
	int (*netreadline)	(char *, size_t, unsigned char*, size_t);
};

typedef struct net_tools net_tools_t;

#define NET_TOOLS_T_SIZE 3

#define NET_TOOLS_T_RUDP 0
#define NET_TOOLS_T_UDP  1
#define NET_TOOLS_T_TCP  2

extern struct in_addr *NetText2Addr	(char *);
extern int NetReadLine			(unsigned char *, size_t, unsigned char *, size_t);

#endif
