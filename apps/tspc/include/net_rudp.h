/*
---------------------------------------------------------------------------
 net_rudp.h,v 1.4 2004/02/11 04:52:41 blanchet Exp
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

#ifndef _rudp_h_
#define _rudp_h_

#define RTTENGINE_G  (float)1/8
#define RTTENGINE_H  (float)1/4
#define RTTENGINE_TMIN	2
#define RTTENGINE_TMAX  30
#define RTTENGINE_MAXRTT 3
#define RTTENGINE_MAXRT  8

#include <sys/types.h>

extern SOCKET	NetRUDPConnect	(char *, int);
extern int	NetRUDPClose	(SOCKET);

extern int	NetRUDPReadWrite	(SOCKET, unsigned char *, size_t, unsigned char *, size_t);

extern int	NetRUDPWrite		(SOCKET, unsigned char *, size_t);
extern int	NetRUDPPrintf		(SOCKET, char *, size_t, char *, ...);

extern int	NetRUDPRead		(SOCKET, unsigned char *, size_t);


typedef struct rudp_message_struct {
	u_int32_t sequence;
	u_int32_t timestamp;
} rudp_msghdr_t;


typedef struct rttengine_statistics {
	/* connected udp host stats */

	struct sockaddr_in* sai;

	/* stat stats */
	
	float rtt;
	float srtt;
	float rttvar;
	float rto;

	/* timeline stats */
	u_int32_t sequence;
	int retries;
	int32_t last_recv_sequence;
	int32_t initial_timestamp;
	int apply_backoff;
	int has_peer;
	int initiated;
} rttengine_stat_t;

#endif
