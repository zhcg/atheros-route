/*
---------------------------------------------------------------------------
 net_udp.h,v 1.4 2004/02/11 04:52:42 blanchet Exp
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

#ifndef _NET_UDP_H_
#define _NET_UDP_H_

extern SOCKET	NetUDPConnect	(char *, int);
extern int	NetUDPClose	(SOCKET);

extern int	NetUDPReadWrite	(SOCKET, unsigned char *, size_t, unsigned char *, size_t);

extern int	NetUDPWrite		(SOCKET, unsigned char *, size_t);
extern int	NetUDPPrintf		(SOCKET, char *, size_t, char *, ...);

extern int	NetUDPRead		(SOCKET, unsigned char *, size_t);

#endif
