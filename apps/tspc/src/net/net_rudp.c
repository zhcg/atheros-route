/*
---------------------------------------------------------------------------
 net_rudp.c,v 1.9 2004/02/11 04:52:51 blanchet Exp
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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <errno.h>

#define _USES_SYS_TIME_H_
#define _USES_SYS_SOCKET_H_
#define _USES_NETINET_IN_H_
#define _USES_ARPA_INET_H_
#define _USES_NETDB_H_

#include "platform.h"

#include "net_rudp.h"
#include "net.h"
#include "log.h"

// global variables

static rttengine_stat_t rttengine_stats;

// forward declarations

static int rttengine_init(rttengine_stat_t *);
static int rttengine_deinit(rttengine_stat_t *, void *, void *);

static void *internal_prepare_message(rudp_msghdr_t **, size_t);
static void internal_discard_message(void *);
static float rttengine_update(rttengine_stat_t *, uint32_t);
static uint32_t internal_get_timestamp(rttengine_stat_t *);
static float internal_get_adjusted_rto(float);
static ssize_t internal_send_recv(SOCKET, void *, size_t, void *, size_t);
static struct sockaddr_in *internal_get_sai(rttengine_stat_t *, char *, int);


/* Exported functions */

int
NetRUDPInit(void) 
{
	memset(&rttengine_stats, 0, sizeof(rttengine_stat_t));
	return rttengine_init(&rttengine_stats);
}

/* */

int
NetRUDPDestroy(void) 
{
	if ( rttengine_deinit(&rttengine_stats, NULL, NULL) == 0)
		return 1;
	return 0;
}

/* */

SOCKET
NetRUDPConnect(char *Host, int Port) 
{
	SOCKET sfd;
	struct sockaddr_in *sai;

	if (rttengine_stats.initiated == 0)
		NetRUDPInit();

	if ( (sai = internal_get_sai(&rttengine_stats, Host, Port)) == NULL) {
		return -1;
	}


	/* and get a socket */

	if ( (sfd = socket(PF_INET, SOCK_DGRAM, 0)) == -1 ) {
		return -1;
	}

	/* then connect it */

	if ( (connect(sfd,(struct sockaddr *) sai, sizeof(struct sockaddr_in))) == -1 ) {

		return -1;
	}
	
	return sfd;
}

/* */

int NetRUDPClose(SOCKET sock) 
{
	shutdown(sock, SHUT_RDWR);
	close(sock);
	return NetRUDPDestroy();
}

/* */

int NetRUDPReadWrite(SOCKET sock, unsigned char *in, size_t il, unsigned char *out, size_t ol)
{
	return internal_send_recv(sock, in, il, out, ol);
}

/* */

int
NetRUDPWrite(SOCKET sock, unsigned char *b, size_t l) 
{
	return NetRUDPReadWrite(sock, b, l, NULL, 0);
}

/* */

int
NetRUDPPrintf(SOCKET sock, char *out, size_t ol, char *Format, ...)
{
  va_list argp;
  int Length;
  char Data[1024];

  va_start(argp, Format);
  vsnprintf(Data, sizeof Data, Format, argp);
  va_end(argp);

  Length = strlen(Data);

  return NetRUDPReadWrite(sock, Data, strlen(Data), out, ol);
}

/* */

int
NetRUDPRead(SOCKET sock, unsigned char *b, size_t l) 
{
	return NetRUDPReadWrite(sock, NULL, 0, b, l);
}


/* Internal functions; not exported */

static ssize_t
internal_send_recv(SOCKET fd, void *in, size_t il, void *out, size_t ol)	/* needs a connected UDP socket or else all hell will break loose */ 
{
	fd_set fs;
	int ret, ls;	/* return code, length sent */
	rudp_msghdr_t *omh = NULL; /* outoing message header */
	rudp_msghdr_t *imh = NULL; /* incoming message header */
	void *om = NULL;
	void *im = NULL;       /* outgoing and incoming messages raw data */
	struct timeval tv_sel, tv_beg;
	

	if ( rttengine_stats.initiated == 0 )
		return -1;

	om = internal_prepare_message(&omh,il);
	im = internal_prepare_message(&imh,ol);

	memset(om, 0, il);
	memset(im, 0, ol);
	
	if (om == NULL || im == NULL) { /* something in the memory allocation failed */
		/* cleanup */
		rttengine_deinit(&rttengine_stats, om, im);
		return -1;
	}

	memcpy(om+sizeof(rudp_msghdr_t), in, il);

	/* stamp in the sequence number */

	omh->sequence = rttengine_stats.sequence++ | 0xf0000000;
		

 sendloop: /* if we have no peer yet - that means retries = MAXRTT with no replies, quit it.
	    * if we do have a peer, then now is a good time to apply exponential backoff
	    */

	if (rttengine_stats.retries == RTTENGINE_MAXRTT) {
		if (rttengine_stats.has_peer == 0) {
			rttengine_deinit(&rttengine_stats, om, im);
			return -1;
		} else rttengine_stats.apply_backoff = 1;
	}
	
	
	if (rttengine_stats.retries == RTTENGINE_MAXRT) {
		/* cleanup */
		rttengine_deinit(&rttengine_stats, om, im);		
		return -1;
	}


	/* update the timestamp of the message */
	
	omh->timestamp = internal_get_timestamp(&rttengine_stats);

	Display(4, ELNotice, "internal_send_recv", "rudp packet %i, RTO %f, sequence 0x%x timestamp %i",rttengine_stats.retries, rttengine_stats.rto, omh->sequence, omh->timestamp);

        /* send the message */

	if ( ( ls = send(fd, om, il+sizeof(rudp_msghdr_t), 0)) == -1) {
		/* cleanup */
		rttengine_deinit(&rttengine_stats, om, im);
		return -1; /* if the send fails, quit it */ /* XXX check for a ICMP port unreachable here */
	}

	tv_sel.tv_sec=(int)rttengine_stats.rto; /* get the RTO in a format select can understand */
	tv_sel.tv_usec=(int)( (rttengine_stats.rto - tv_sel.tv_sec) * 1000 ); /* ie, 3.314 - 3 * 1000 = 314 milliseconds */

	GETTIMEOFDAY(&tv_beg, NULL);
	
 selectloop:
/* and wait for an answer */

	FD_ZERO(&fs);
	FD_SET(fd, &fs);

	ret = select(fd+1, &fs, NULL, NULL, &tv_sel);

	switch (ret) {

	case 0: {
		/* select timed out with nothing to read */
		/* so we might need to step back a little on the timeout, and do a resend */
		Display(3, ELNotice, "internal_send_recv", "No RUDP reply");
		if (rttengine_stats.apply_backoff == 1)
			rttengine_stats.rto = internal_get_adjusted_rto(rttengine_stats.rto *= 2);
		rttengine_stats.retries++;
		goto sendloop;
	}

	case 1: { /* We did get a reply, is it what we were waiting for?
		   * lets read everything the server is sending and see
		   */
		
		ret = recv(fd, im, sizeof(rudp_msghdr_t)+ol, 0);

		Display(4, ELNotice, "internal_send_recv", "reply: rudp packet %i, RTO %f, sequence 0x%x timestamp %i",rttengine_stats.retries, rttengine_stats.rto, imh->sequence, imh->timestamp);
		
		if (ret == -1) { /* fatal read error */
			/* cleanup */
			rttengine_deinit(&rttengine_stats, om, im);
			return -1;
		}
		
		if ( imh->sequence == omh->sequence ) {
			ret = ret - sizeof(rudp_msghdr_t);	/* we keep the lenght received minus the headers */
			break; /* yes it is what we are waiting for */
		} else {
			/* readjust tv to the remaining time */
			/* tv_sel = time_now - time_beginning */

			struct timeval tv_now;
			GETTIMEOFDAY(&tv_now, NULL);

			tv_sel.tv_sec = tv_now.tv_sec - tv_beg.tv_sec;
			/* XXX substract the usec as well */

			goto selectloop;

		}
	}
		
	default: { /* error of unknown origin, ret contains the ERRNO compatible error released by select() */
		/* cleanup */
		rttengine_deinit(&rttengine_stats, om, im);
		return -1;
	}

	}//switch

	/* update our stat engine, the RTT and compute the new RTO */

	rttengine_update(&rttengine_stats, internal_get_timestamp(&rttengine_stats) - imh->timestamp);

	/* get the reply in a safe place */

	memcpy(out, im+sizeof(rudp_msghdr_t), ret);
	
	/* free the memory */
	
	internal_discard_message(om);
	internal_discard_message(im);

	/* and *goodbye* */

	rttengine_stats.has_peer = 1;	/* we have a peer it seems */
	rttengine_stats.retries = 0;	/* next packet can retry like it wishes to */
	

	return ret;
}

static int
rttengine_init(rttengine_stat_t *s) 
{
	struct timeval tv;
	
	if (s == NULL)
		return 0;

	if (s->initiated == 1)
		return s->initiated;

	if ( GETTIMEOFDAY(&tv, NULL) == -1 )
		memset(&tv, 0, sizeof(struct timeval));

	s->rtt = 0;
	s->srtt = 0;
	s->rttvar = 0.50;
	s->rto = RTTENGINE_TMIN;
	
	s->sequence = 0;
	s->retries = 0;
	s->last_recv_sequence = 0xBAD;
	s->initial_timestamp = tv.tv_sec;

	s->has_peer = 0;
	s->apply_backoff = 0;
	s->initiated = 1;

	return s->initiated;
}

static int
rttengine_deinit(rttengine_stat_t *s, void *im, void *om) 
{
	if (s->sai != NULL) {
		free(s->sai);
		s->sai = NULL;
	}

	if (im != NULL) {
		internal_discard_message(im);
		im = NULL;
	}

	if (om != NULL) {
		internal_discard_message(om);
		om = NULL;
	}
	
	s->initiated = 0;
	return s->initiated;
}


static float
rttengine_update(rttengine_stat_t *s, uint32_t rtt) 
{
	float delta;

	if (s == NULL)
		return 0; 

	s->rtt = rtt / 1000;	/* bring this back to seconds, the rest of the code uses milliseconds, including the timestamp that is in the payload */
	delta = s->rtt - s->srtt;
	s->srtt = s->srtt + RTTENGINE_G * delta;
	s->rttvar = s->rttvar + RTTENGINE_H * ( delta<0?-delta:delta );
	s->rto = s->srtt + 4 * s->rttvar;
	s->rto = internal_get_adjusted_rto(s->rto);

	s->retries = 0; /* update is called when a packet was acked OK, so we need to reset this here */

	return s->rto;
}

static void *
internal_prepare_message(rudp_msghdr_t **hdr, size_t msglen) 
{
	void *buf;

	if (msglen == 0)
		buf = (rudp_msghdr_t *) malloc(sizeof(rudp_msghdr_t));
	else buf = (void *) malloc(msglen+sizeof(rudp_msghdr_t));

	if (buf!=NULL)
		*hdr = buf;
	return buf;
	
}

static void
internal_discard_message(void *m) 
{
	if (m != NULL)
		free(m);
	return;
}

static uint32_t
internal_get_timestamp(rttengine_stat_t *s)
{
	struct timeval tv;

	if (GETTIMEOFDAY(&tv, NULL) == -1)
		return 0;

	return ( ((tv.tv_sec - s->initial_timestamp) * 1000 ) + (tv.tv_usec / 1000) );
}

static float
internal_get_adjusted_rto(float rto) 
{
	if (rto<RTTENGINE_TMIN)
		return RTTENGINE_TMIN;
	if (rto>RTTENGINE_TMAX)
		return RTTENGINE_TMAX;

	return rto;
}


static struct sockaddr_in *
internal_get_sai(rttengine_stat_t *s, char *Host, int Port) 
{
	/* we need to be reinitialised for each new connection,
	 * so we can check if we already have something
	 * cached and assume it is fit for the
	 * current situation
	 */

	struct sockaddr_in *sai;
	struct in_addr *addr;

	/* so, is it cached? */
	
	if (s->sai != NULL)
		return s->sai;

	/* its not */
	
	/* get the IP address from the hostname */

	if( (addr = NetText2Addr(Host)) == NULL )
			return NULL;

	/* get memory for our patente */
	if ( (sai = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in))) == NULL)
		return NULL;
	
	/* clear out our sockaddr_in entry, fill it and cache it */

	memset(sai, 0, sizeof(struct sockaddr_in));
	sai->sin_family = PF_INET;
	sai->sin_port = htons(Port);
	sai->sin_addr.s_addr = addr->s_addr;
	s->sai = sai;

	return sai;
}


