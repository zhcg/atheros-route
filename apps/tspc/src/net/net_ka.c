/*
---------------------------------------------------------------------------
 net_ka.c,v 1.26 2004/07/14 16:42:23 dgregoire Exp
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
	 This implements a keepalive algorythm.

	 Should be able to get a ping socket for all the platforms,
	 generate the ICMP6 raw data and process the actual ping
	 based on internal values.
	 

---------------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define _USES_SYS_TIME_H_
#define _USES_SYS_SOCKET_H_
#define _USES_NETINET_IN_H_
#define _USES_NETINET_IP6_H_
#define _USES_ARPA_INET_H_

#include "platform.h"

#include "net_ka.h"
#include "net_cksm.h"
#include "log.h"	// log levels + display

static struct NetKeepAlive ka;

static void internal_adjust_major_fuzz(float *);
static void internal_adjust_minor_fuzz(float *);
static void internal_adjust_next_event(struct NetKeepAlive *, int got_reply, int pinged);
static float internal_trim(float, float, float);

static int internal_do_pingout(struct NetKeepAlive *);
static int internal_do_pingin(struct NetKeepAlive *);

#ifndef BROKEN_WINDOWS_RAWSOCK
#define KA_PING_OFFSET_SEQ 7	// offset of the sequence number in the ping packet
#define KA_PING_OFFSET_CKSUM_HI 3
#define KA_PING_OFFSET_CKSUM_LOW 2
#define KA_PING_OFFSET_HEADER 0

static char ping_packet[] = {
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
	0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,
	0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x61,
	0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69
}; /* ping packet */
#else	// DeviceIOControl ping for windows
HANDLE  Handle;
OVERLAPPED *ol = NULL;
PICMPV6_ER request = NULL;
ICMPV6_EREP reply;
void
TranslateSA6toIP6(ADDRESS_IP6 *dst, struct sockaddr_in6 *src)
{
    memcpy(dst, &src->sin6_port, sizeof *dst);
}
#endif


int
NetKeepaliveInit(char *src, char *dst, int maximal_keepalive) {
	struct timeval tv;
#ifdef BROKEN_WINDOWS_RAWSOCK
	Handle = CreateFileW(WIN_IPV6_DEVICE_NAME,
                         0,          // desired access
                         FILE_SHARE_READ | FILE_SHARE_WRITE, 
                         NULL,       // security attributes
                         OPEN_EXISTING,
                         FILE_FLAG_OVERLAPPED,          // flags & attributes
                         NULL);      // template file
    
	if (Handle == INVALID_HANDLE_VALUE) {
		Display(0, ELError, "NetKeepaliveInit", "Unable to contact IPv6 driver");
        return 1;
	}
#else
	// reset the sequence number
	ping_packet[KA_PING_OFFSET_SEQ] = 0;
#endif

	/* Load the structure with passed in AND
	 * initial values
	 */

	if ( strncpy( ka.host, dst, sizeof(ka.host)) == NULL )
		return 1;

	ka.maximal_keepalive = maximal_keepalive;
	ka.current_keepalive = KA_INITIAL_KEEPALIVE;
	ka.major_fuzz = (float) 0.60;
	ka.minor_fuzz = (float) 0.075;

	/* And initialize the next event */
	/* Initial timeout is set to five seconds */

	GETTIMEOFDAY(&tv, NULL);
	ka.next_event.tv_sec = tv.tv_sec + KA_INITIAL_KEEPALIVE;
	ka.next_event.tv_usec = tv.tv_usec;

	/* initialize the random source */

	SRANDOM(tv.tv_usec);

	/* get a ping socket and a monitor socket */

	INET_PTON(AF_INET6, dst, &ka.sa6.sin6_addr);
	ka.sa6.sin6_family = AF_INET6;

	INET_PTON(AF_INET6, src, &ka.sa6_local.sin6_addr);
	ka.sa6_local.sin6_family = AF_INET6;

	ka.keepalive_fd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	ka.consecutive_timeout = 0;
	ka.still_up = 0;
	ka.doit = 1;
	ka.count = 0;
	ka.got_read = 0;
	ka.got_write = 0;
	ka.pinged = 1;

	/* ok, we are inialized */

	ka.init = 1;
	Display(1, ELInfo, "NetKeepaliveInit", "keepalive initialized with %s as a peer, max KA value of %i\n", dst, maximal_keepalive);
	return 0;
}

void
NetKeepaliveDestroy() {
	shutdown(ka.keepalive_fd, SHUT_RDWR);
	close(ka.keepalive_fd);
	memset(&ka, 0, sizeof(ka));
#ifdef BROKEN_WINDOWS_RAWSOCK
	CloseHandle(Handle);
	if (ol != NULL) {
		free(ol);
		ol = NULL;
	}
#endif
}

void
NetKeepaliveGotRead() {
	if (ka.init == 1) {
		ka.got_read = 1;
		Display(4, ELNotice, "NetKeepaliveGotRead", "Incoming data from TUNNEL");
	}
}

void
NetKeepaliveGotWrite() {
	if (ka.init == 1) {
		ka.got_write = 1;
		Display(4, ELNotice, "NetKeepaliveGotWrite", "Outgoing data to TUNNEL");
	}
}


int
NetKeepaliveV6V4Tunnel(char *src, char *dst, int maximal_keepalive) {
	if (NetKeepaliveInit(src, dst, maximal_keepalive))
		return 1;
	for (;;) {
		SLEEP(1);
		if ( NetKeepaliveDo() == 2) {
			NetKeepaliveDestroy();
			return 4;		// 4 is keepalive timeout. very sketchy. 
		}
	}
	NetKeepaliveDestroy();
	return 0;
}


/* Should be called with a good resolution 
   to allow for good precision. 

   Every 50 to 100 ms.

   return values :
	0 - everything is OK
	1 - warning - timeout
	2 - fatal - too many timeouts
	3 - not initialised
	*/

int
NetKeepaliveDo() {

	struct timeval tv;
	int doit = 0;		// local evaluation of if we should try to ping.
						// will be flagged only if we hit the next event time
	
	if (ka.init != 1)
		return 3;
	
	if (internal_do_pingin(&ka) != 0)	// read echo replies
		ka.got_read = 1;

	if (ka.got_read == 1) {			// if we did get a read in the last cycle,
		ka.consecutive_timeout = 0;	// we can reset the consecutive timeouts counter.
		ka.count++;					// increment the packet counter
		ka.got_read = 0;			// and reset the read flag until next time
	}

	if (ka.got_write == 1) {	// we did get a write in the last cycle?
		ka.doit = 0;			// then no need to ping out
		ka.got_write = 0;		// and also reset the write flag until next time
	}


	/* do we need to ping out ? */

	GETTIMEOFDAY(&tv, NULL);
	
	/* If a ping is needed,
	   then also take a look
	   at the number of consecutive
	   missreads we have.
	   */

	if (tv.tv_sec == ka.next_event.tv_sec) {
		if (tv.tv_sec >= ka.next_event.tv_usec)
			doit = 1; 
	}

	else if (tv.tv_sec > ka.next_event.tv_sec)
		doit = 1;
	
	
	if (doit) {
	
		if (ka.doit == 0 && ka.count > 0)
			ka.still_up = 1;	/* so if we had both read and writes, we are up? */

		if (ka.consecutive_timeout != 0)	/* if we had timeouts, ping */
			ka.still_up = 0;

		if (ka.count == 2 || ka.count == 1)	/* a count of 2 or 1 probably means */
											/* only the keepalive packet was sent */
			ka.still_up = 0;

		if (ka.doit == 1)					/* no write, do it */
			ka.still_up = 0;

		Display(4, ELNotice, "NetKeepaliveDo", "A: ka.still_up: %i ka.ct: %i ka.doit: %i ka.count: %i", ka.still_up, ka.consecutive_timeout, ka.doit, ka.count);
		
		if ( ka.still_up == 0 )  { 
								// if we havent got any
								// traffic for the last cycle,
								// generate a ping (and thus
								// a reply, and thus - traffic)
								// or none if we are down.
								// a count of 2 will mean only our ping got thru since
								// last time and it means we need to continue
								// pinging
			
			if (internal_do_pingout(&ka) > 0) {
				ka.got_write = 1;	// if it worked, flag the write flag
			}
			ka.consecutive_timeout++;		// we had a timeout of sorts
											// since we had to ping
		}

		ka.doit = 1;		// re-ping unless noted otherwise
		ka.count = 0;		// reset packet counter in between pings
		
		Display(4, ELNotice, "NetKeepaliveDo", "B: ka.still_up: %i ka.ct: %i ka.doit: %i ka.count: %i", ka.still_up, ka.consecutive_timeout, ka.doit, ka.count);

		/* adjust internal values */
		/* set the next event time */

		internal_adjust_next_event(&ka, ka.still_up, ka.consecutive_timeout);

		/* adjust the major fuzz factor.
	   it needs to vary +/- 10 - 40% and stay
	   in the .40 - .80 range
	   */
		internal_adjust_major_fuzz(&ka.major_fuzz);

		/* then adjust the minor fuzz factor.
		this one needs to vary +/- 1 - 5%
		staying in the .03 - .07 range
		*/
		internal_adjust_minor_fuzz(&ka.minor_fuzz);

		if (ka.consecutive_timeout == KA_MAXIMAL_CYCLES)
			return 2;

		ka.still_up = 0; // until next time, we are down unless noted otherwise
	}


	/* and return */
	return 0;
}

static 
void
internal_adjust_major_fuzz(float *major_fuzz) {
	int i;
	float f;

	/* here we want a randomness of .40 - .80 */

	i = RANDOM()%40;
    f = (float) i / 100;		/* 0 - 100% */
	f += .40;
	
	*major_fuzz = internal_trim (f, 0.40, 0.80);
}

static
void 
internal_adjust_minor_fuzz(float *minor_fuzz) {
	int i;
	float f;

	/* here we want a randomness of .03 - .07 */

	i = RANDOM()%4;
    f = (float) i / 100;
	f += .03;

	*minor_fuzz = internal_trim (f, 0.03, 0.07);
}

static
void
internal_adjust_next_event(struct NetKeepAlive *nka, int got_reply, int ct) {
	/* if we are in the comfort zone, 
	   apply minor_fuzz.
	   Otherwise, apply major_fuzz

	   If we are in the comfort zone and got no
	   reply, force us out of it.
	*/

	int i;
	struct timeval tv;
	float minor_minimum, major_minimum;

	i = RANDOM()&0x01;      /* positive or negative shift? */

	minor_minimum = KA_INITIAL_KEEPALIVE;
	major_minimum = (float) nka->maximal_keepalive * 0.75;

	/* next_event = current_event * +/- factor */

	if (nka->current_keepalive >= major_minimum) {
			// we are in the comfort zone
			// did we get any reply?
			// if not, bump down outside the zone
			// also ensure that if we
		    // are stuck at either ends of the
		    // spectrum, we get out fast
		if (got_reply == 0 && ct > 1) 
				nka->current_keepalive = major_minimum - 1;
		else {
			float fuzz_factor;
			if (nka->current_keepalive == nka->maximal_keepalive)
				fuzz_factor = 1 - nka->minor_fuzz;
			else if (nka->current_keepalive == major_minimum)
				    fuzz_factor = 1 + nka->minor_fuzz; 
			else fuzz_factor = i ? (1 - nka->minor_fuzz) : (1 + nka->minor_fuzz);
			
			nka->current_keepalive = internal_trim(nka->current_keepalive * fuzz_factor,
				major_minimum, (float) nka->maximal_keepalive);
		}
	}

	/* else we are NOT in the comfort zone */
	if (nka->current_keepalive < major_minimum) {
		/* if we got_reply we go up, otherwise
		   we go down */
		float fuzz_factor;

		fuzz_factor = (ct < 2) ? (1 + nka->major_fuzz) : ( 1 - nka->major_fuzz);
		nka->current_keepalive = internal_trim(nka->current_keepalive * fuzz_factor,
		minor_minimum, (float) nka->maximal_keepalive);
	}

	/* then put the next event in the timeval */

	GETTIMEOFDAY(&tv, NULL);

	nka->next_event.tv_sec  = tv.tv_sec + (int) nka->current_keepalive;
	nka->next_event.tv_usec = ( nka->current_keepalive - (int) nka->current_keepalive ) * 1000000;

	Display(4, ELNotice, "internal_adjust_next_event", "next KA scheduled in %f\n", nka->current_keepalive);

	return;
}

static
float
internal_trim(float value, float lower, float higher) {
	if ( value <= lower)
		return lower;
	if (value >= higher)
		return higher;
	return value;
}

/* if this returns 0, no data was written. Otherwise, data was written
*/

static
int
internal_do_pingout(struct NetKeepAlive *nka) {
#ifdef BROKEN_WINDOWS_RAWSOCK
	unsigned long int ret = 0;
	char *SendBuffer;
	unsigned int SendSize = 16;
	int errorCode, i;
#else
	int ret = 0;
#endif

#ifdef NET_KA_COMPUTE_CHECKSUM
	unsigned long pseudo_hdr[2];
	vec_t cksum_vec[4];
	unsigned short int cksum;

	/* get the checksum */

	cksum_vec[0].ptr = (unsigned char *) &nka->sa6_local.sin6_addr;
	cksum_vec[0].len = sizeof(nka->sa6_local.sin6_addr);
	cksum_vec[1].ptr = (unsigned char *) &nka->sa6.sin6_addr;
	cksum_vec[1].len = sizeof(nka->sa6.sin6_addr);

	pseudo_hdr[0] = htonl(40);
	pseudo_hdr[1] = htonl(0x3a);
	cksum_vec[2].ptr = (unsigned char *) &pseudo_hdr;
	cksum_vec[2].len = sizeof (pseudo_hdr);

	cksum_vec[3].ptr = (unsigned char *) ping_packet + KA_PING_OFFSET_HEADER;
	cksum_vec[3].len = sizeof (ping_packet) - KA_PING_OFFSET_HEADER;

	ping_packet[KA_PING_OFFSET_CKSUM_LOW] = 0;
	ping_packet[KA_PING_OFFSET_CKSUM_HI] = 0;

	cksum = in_cksum(cksum_vec, 4);

	ping_packet[KA_PING_OFFSET_CKSUM_LOW] = cksum&0xff;
	ping_packet[KA_PING_OFFSET_CKSUM_HI] = cksum>>8;
#endif	

	/* and send out the data*/

#ifndef BROKEN_WINDOWS_RAWSOCK
	/* increase the seq number */
	ping_packet[KA_PING_OFFSET_SEQ] = (++ping_packet[KA_PING_OFFSET_SEQ]) % 0xff;
	ret = sendto(nka->keepalive_fd, ping_packet, sizeof(ping_packet), 0, (struct sockaddr *) &(nka->sa6),
		sizeof(nka->sa6));
#else

	ol = malloc(sizeof(OVERLAPPED));
	memset(ol, 0, sizeof(OVERLAPPED));

	if ( (ol->hEvent = CreateEvent(NULL, FALSE, FALSE, "Ping6Event")) == NULL ) {
		Display(0, ELError, "NetKeepaliveInit", "Unable to create an event object");
        return 0;
	}

	request = malloc(sizeof(ICMPV6_ER) + SendSize * sizeof(char) );
	memset(request, 0, sizeof(ICMPV6_ER) + SendSize * sizeof(char) );
	SendBuffer = (char *)(request + 1);

	TranslateSA6toIP6(&request->src, &ka.sa6_local);
	TranslateSA6toIP6(&request->dst, &ka.sa6);
	request->timeout = 10000;	// timeout value in MS

	for (i = 0; i < SendSize; i++)
        SendBuffer[i] = 'a' + (i % 26);

	if ( DeviceIoControl(Handle, IOCTL_ICMPV6_ER,
		  request, sizeof(ICMPV6_ER) + SendSize * sizeof(char),
		  &reply, sizeof(reply), &ret, ol) == FALSE) {
			if ((errorCode = GetLastError()) == ERROR_IO_PENDING)
				ret = SendSize;
			else
				Display(4, ELNotice, "internal_do_pingout", "PING: transmit failed, error code %u", errorCode);
		}
#endif
	
	if (ret > 0)
		Display(4, ELNotice, "internal_do_pingout", "Data written on the ICMP6 socket: %i bytes", ret);
	return ret;
}
 
/* if this next one returns zero, no ping reply.
   if it returns > 0, ping reply.
   */

static
int
internal_do_pingin(struct NetKeepAlive *nka) {
	unsigned char buffer[2048];

#ifndef BROKEN_WINDOWS_RAWSOCK
	int ret = 0;
	fd_set fs;
	struct timeval tv_sel;

	memset(buffer, 0, sizeof(buffer));

	FD_ZERO(&fs);
	FD_SET(nka->keepalive_fd, &fs);
	memset(&tv_sel, 0, sizeof(tv_sel));	// set to zero - imitate polling

	ret = select(nka->keepalive_fd + 1, &fs, NULL, NULL, &tv_sel);	
	if (ret > 0)
		ret = recv(nka->keepalive_fd, buffer, sizeof(buffer), 0);
#else
	unsigned long int ret = 0;
	memset(buffer, 0, sizeof(buffer));
	if ( ol != NULL && HasOverlappedIoCompleted(ol)) {
		GetOverlappedResult(Handle, ol, &ret, FALSE);
		CloseHandle(ol->hEvent);
		free(ol);
		ol = NULL;
		free(request);
		request = NULL;

		if (ret > 0 && reply.status != 0)
			ret = 0;
	}
#endif
	if (ret > 0) 
		Display(4, ELNotice, "internal_do_pingin", "ICMP echo reply on the ICMP6 socket, size: %i\n", ret);
	return ret;
}



