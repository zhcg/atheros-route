/*
---------------------------------------------------------------------------
 net_ka.h,v 1.13 2004/07/13 17:53:01 dgregoire Exp
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
*/
/***********************************************************************
	 This implements a keepalive algorythm.

	 Should be able to get a ping socket for all the platforms,
	 generate the ICMP6 raw data and process the actual ping
	 based on internal values.
	 
 Algo:

		Start from the maximum value suggested by the server.

		Need:

		1. Major fuzz factor
			The major fuzz factor is applied to major increases or decreases 
			of the timeout value. +/- 0% - 40%, initial +25%

		2. Minor fuzz factor
			The minor fuzz factor is applied after we have reached the upper 
			timeout value zone.  +/- 0% - 5%, initial +3%

		3. Initial timeout
			Fixed to 5 seconds

		4. Maximal timeout
			Given by the server

		5. Upper timeout value zone
			( (Maximal timeout - 25%) - Maximal timeout ) (ie, 75% - 100% of 30 seconds). This
			is meant as a comfort zone in which we can apply small changes to
			the keep alive value - changes in the order of the minor fuzz factor.
			(from Teredo, section 6.7)


		a)	Apply +(Major fuzz factor) to the timeout value until we reach the upper
			timeout value zone, then throttle to +/-(Minor fuzz factor) with an adjustment to
			stay in the upper timeout value zone.

		b)	When we get no replies, apply -(Major fuzz factor) to the timeout value until 
			initial timeout is reached. Forfeit if we get too many timeouts. If reply, go to a)


		So.

		Each time NetKeepaliveDo() is called, we check next_event and see if we have
		to let out a keepalive packet. If so we do, recompute major_fuzz, minor_fuzz,
		apply either +(major_fuzz) or +/-(minor_fuzz) to next_event and exit.

		Apply +(major_fuzz) if outside the comfort zone, +/-(minor_fuzz) otherwise.

		If we have an event ready and we havent got a read from the socket,
		we send the keepalive and recompute major_fuzz, minor_fuzz, apply -(major_fuzz)
		to next_event and exit.


---------------------------------------------------------------------------
*/

#ifdef BROKEN_WINDOWS_RAWSOCK
#include <winioctl.h>
#define FSCTL_IPV6_BASE FILE_DEVICE_NETWORK
#define _IPV6_CTL_CODE(F, M, A) \
		CTL_CODE(FSCTL_IPV6_BASE, F, M, A)  
#define IOCTL_ICMPV6_ER \
            _IPV6_CTL_CODE(0, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define WIN_IPV6_BASE_DEVICE_NAME L"Ip6"
#define WIN_IPV6_DEVICE_NAME      L"\\\\.\\" WIN_IPV6_BASE_DEVICE_NAME

#pragma pack(1) 
typedef struct _ADDRESS_IP6 {
    unsigned short ip6_port;
    unsigned long  ip6_flowinfo;
    unsigned short ip6_addr[8];
    unsigned long  ip6_scope_id;
} ADDRESS_IP6, *PADDRESS_IP6;
#pragma pack() 

#pragma pack(1) 
typedef struct icmpv6_er {
    ADDRESS_IP6 dst; 
    ADDRESS_IP6 src; 
    unsigned int timeout;       
    unsigned char TTL;         
    unsigned int flags;
} ICMPV6_ER, *PICMPV6_ER;
#pragma pack() 

#pragma pack(1) 
typedef struct icmpv6_erep {
    ADDRESS_IP6 address;    // Replying address.
    unsigned long status;           // Reply IP_STATUS.
    unsigned int rtt; // RTT in milliseconds.
	char data[256];
} ICMPV6_EREP, *PICMPV6_EREP;
#pragma pack() 
#endif

/* forward functions declarations */

int
NetKeepaliveInit(char *src, char *dst, int maximal_keepalive); /* return values :
														0 - ok
														1 - error
														*/
void
NetKeepaliveDestroy();

int
NetKeepaliveV6V4Tunnel(char *src, char *dst, int maximal_keepalive);

int
NetKeepaliveDo();	/* return values :
										0 - everything is OK
										1 - warning - timeout
										2 - fatal - too many timeouts
										*/

void
NetKeepaliveGotRead();

void
NetKeepaliveGotWrite();

/* Data */

struct NetKeepAlive {

	int init;		/* is it initialized? */
	char host[512];		/* host to send ping to */

	SOCKET keepalive_fd;
	struct sockaddr_in6 sa6_local; /* our ipv6 */ /* needed for cksum */
	struct sockaddr_in6 sa6;	/* ping destination */
	int still_up;			// link is still up?
	int doit;				// we had no write out, ping out to keepalive
	int count;				// read counter in between cycles
	int got_read;			// read flag
	int got_write;			// write flag
	int pinged;				// pinged flag
	int consecutive_timeout;	/* number of consecutive timeouts */

	int maximal_keepalive;	/* the maximal wait factor */
	double current_keepalive;

	float minor_fuzz;		/* the minor fuzz factor */
	float major_fuzz;		/* the major fuzz factor */

#define KA_INITIAL_KEEPALIVE 5	// initial timeout value
#define KA_MAXIMAL_CYCLES	 5	// maximal number of consecutive keepalive
								// missed before declaring a timeout
	struct timeval next_event;		/* take action at OR after this time */
};


