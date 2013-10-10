/*
---------------------------------------------------------------------------
 api.c,v 1.12 2004/09/09 18:26:06 dgregoire Exp
---------------------------------------------------------------------------
*
* This file is originally derived from the OpenVPN project 
* by James Yonan. 
*
* All source code which derives from the OpenVPN project is
* Copyright (C) James Yonan, 2003, and is released under the
* GPL version 2 (see below).
*
* All other source code is Copyright c) Hexago Inc. 2002-2004,
* and is released under the GPL version 2 (see below).
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


#include <ws2tcpip.h>

#include "config-win32.h"
#include "syshead.h"

#include "win32.h"

#include "options.h"
#include "socket.h"
#include "buffer.h"
#include "tun.h"
#include "net_ka.h"		// keepalive stuff
#include "log.h"

/* Handle signals */
static void print_signal (int);
static volatile int signal_received = 0;


const char *
TunGetTunName() {
	return guess_tuntap_dev(NULL, "tun", NULL);
}

int
TunMainLoop(SOCKET s, char *tun_dev, char *local_ipv4, int local_port, char *remote_ipv4, int remote_port,
			int debug_level, int keepalive,
			int keepalive_interval, char *local_ipv6, char *remote_keepalive_ipv6)
{
	struct link_socket_addr link_socket_addr;
	struct link_socket link_socket;
	struct sockaddr_in to_link_addr; /* IP address of remote */
	struct tuntap tuntap;
	struct options options;
	int dev = DEV_TYPE_UNDEF;
	/* MTU frame parameters */
	struct frame frame;
	/* description of signal */
	const char *signal_text = NULL;
	/* our global wait event */
	struct event_wait event_wait;

	/* init the GC */
	const int gc_level = gc_new_level ();

	/* init the keepalive */
	if (keepalive_interval != 0) {
		if (NetKeepaliveInit(local_ipv6, remote_keepalive_ipv6, keepalive_interval))
			return 1;
	}
	else keepalive = 0;

	/* declare various buffers */
	struct buffer to_tun = clear_buf ();
	struct buffer to_link = clear_buf ();
	struct buffer buf = clear_buf ();
	struct buffer nullbuf = clear_buf ();

	/* tells us to free to_link buffer after it has been written to TCP/UDP port */
	bool free_to_link = false;
	int has_read = 0;
	int has_wrote = 0;
	int ret = 0;

  /*
   * Buffers used to read from TUN device
   * and TCP/UDP port.
   */
	struct buffer read_link_buf = clear_buf ();
	struct buffer read_tun_buf = clear_buf ();

	error_reset ();                      /* initialize error.c */
	reset_check_status ();               /* initialize status check code in socket.c */
	init_win32 ();
	set_debug_level(debug_level);

	CLEAR (link_socket_addr);
	CLEAR (link_socket);
	CLEAR (tuntap);
	CLEAR (options);
	CLEAR (frame);

	options.local = local_ipv4;
	options.local_port = local_port;
	options.remote = remote_ipv4;
	options.remote_port = remote_port;
	options.proto = PROTO_UDPv4;
	options.bind_local = true;
	options.remote_float = false;
	options.inetd = false;
	options.ipchange = "NULL";
	options.resolve_retry_seconds = 4;
	options.mtu_discover_type = -1;
	options.link_mtu_defined = false;
	options.link_mtu = 0;
    options.tun_mtu_defined = true;
	options.tun_mtu = 1280;

	/* init the event thing */
	wait_init (&event_wait);

	/* initialize the frame */

	frame_finalize (&frame,
		  options.link_mtu_defined,
		  options.link_mtu,
		  options.tun_mtu_defined,
		  options.tun_mtu);

	frame_set_mtu_dynamic (
			       &frame,
				   tuntap.post_open_mtu,
			       SET_MTU_TUN | SET_MTU_UPPER_BOUND);

	 read_link_buf = alloc_buf (BUF_SIZE (&frame));
	 read_tun_buf = alloc_buf (BUF_SIZE (&frame));
	
	/* init the socket */
	link_socket_reset (&link_socket);
	link_socket_init_phase1 (&link_socket,
			   options.local, options.remote,
			   options.local_port, options.remote_port,
			   options.proto,
			   NULL,
			   options.bind_local,
			   options.remote_float,
			   options.inetd,
			   &link_socket_addr,
			   options.ipchange,
			   options.resolve_retry_seconds,
			   options.mtu_discover_type);

	options.dev_type = "tun";
	options.dev = tun_dev;
	tuntap.type = DEV_TYPE_TUN;


	/* init the tun device */
	init_tun (&tuntap,
	    options.dev,
	    options.dev_type,
	    "10.3.0.1",
	    "10.3.0.2",
	    addr_host (&link_socket.lsa->local),
	    addr_host (&link_socket.lsa->remote),
	    &frame,
	    0);


	/* and open the tun device */
	open_tun (options.dev, options.dev_type, options.dev_node,
		options.tun_ipv6, &tuntap);

	if (tuntap.post_open_mtu)
		frame_set_mtu_dynamic (
		&frame,
		tuntap.post_open_mtu,
		SET_MTU_TUN | SET_MTU_UPPER_BOUND);

	/* some info to the user */

	msg (D_TUNTAP_INFO, "local UDP port set to %i", local_port);
	
	/* finalize the TCP/UDP socket */
	link_socket_init_phase2 (&link_socket, &frame, &signal_received);

	if (signal_received) {
		signal_text = "socket";
		print_signal (signal_received);
		goto cleanup;
	}

	/* setup our main loop */
	
	SOCKET_SETMAXFD(link_socket);
	TUNTAP_SETMAXFD( &(tuntap) );

	
	while (true) {
		int stat = 0;
		struct timeval *tv = NULL;
		struct timeval timeval;
		
		signal_text = NULL;

		/* initialize select() timeout */
		//timeval.tv_sec = SHORT_TIMEOUT;
		//timeval.tv_usec = 0;
		timeval.tv_sec = 0;
		timeval.tv_usec = 100000;
		tv = &timeval;
		  
		/* do a quick garbage collect */
		gc_collect (gc_level);
		  
		/*
		* Set up for select call.
		*
		* Decide what kind of events we want to wait for.
		*/
		wait_reset (&event_wait);

		WAIT_SIGNAL(&event_wait);	// wait for keyboard events
        		
		if (to_link.len > 0) {
			msg(D_TUNTAP_INFO, "about to SOCKET_SET_WRITE\n");
			SOCKET_SET_WRITE (link_socket);
		}
		else {
			msg(D_TUNTAP_INFO, "about to TUNTAP_SET_READ\n");
			TUNTAP_SET_READ ( (&tuntap) );
		}

		if (to_tun.len > 0) {
			msg(D_TUNTAP_INFO, "about to TUNTAP_SET_WRITE\n");
			TUNTAP_SET_WRITE ( (&tuntap) );
		}
		else {
			msg(D_TUNTAP_INFO, "about to SOCKET_SET_READ\n");
			SOCKET_SET_READ (link_socket);
		}

		stat = 1; /* this will be our return "status" if select doesn't get called */
		
		if (!signal_received && !SOCKET_READ_RESIDUAL (link_socket)) {
			msg (D_SELECT, "SELECT %s|%s|%s|%s %d/%d",
				TUNTAP_READ_STAT ( (&tuntap) ),
				TUNTAP_WRITE_STAT ( (&tuntap) ),
				SOCKET_READ_STAT (link_socket),
				SOCKET_WRITE_STAT (link_socket),
				tv ? (int)tv->tv_sec : -1,
				tv ? (int)tv->tv_usec : -1
				);
			
			stat = SELECT ();
			check_status (stat, "select", NULL, NULL);
			
			/* keepalive stuff */
			/* if NetKeepaliveDo() returns 1 */
			/* we know we havent */

			if (keepalive) {
				if (NetKeepaliveDo() == 2) {
					ret = 4;
					goto cleanup;
				}
			}

		}
		
		/* set signal_received if a signal was received */
		SELECT_SIGNAL_RECEIVED ();

		if (signal_received) {
			Display(0, ELNotice, "main-loop", "Signal received: %i", signal_received);
			if (signal_received == SIGTERM)
				goto cleanup;
			signal_received = 0;
		}

		if (!stat)  /* timeout? */
			continue;

		if (stat > 0)
		{
			/* Incoming data on TCP/UDP port? */
			if (SOCKET_READ_RESIDUAL (link_socket) || SOCKET_ISSET (link_socket, reads))
			{
				/*
				* Set up for recvfrom call to read datagram
				* sent to our TCP/UDP port.
				*/
				
				struct sockaddr_in from;
				int status;

				ASSERT (!to_tun.len);
				buf = read_link_buf;
				ASSERT (buf_init (&buf, EXTRA_FRAME (&frame)));
				status = link_socket_read (&link_socket, &buf, MAX_RW_SIZE_LINK (&frame), &from);

				/* check recvfrom status */
				
				check_status (status, "read", &link_socket, NULL);
								
				if (buf.len > 0) {
					link_socket_incoming_addr (&buf, &link_socket, &from);
					to_tun = buf;
					has_read = 1;
					NetKeepaliveGotRead();
				}
				else {
					to_tun = nullbuf;
				}
			}
			
			/* Incoming data on TUN device ?*/
			else if (TUNTAP_ISSET ( (&tuntap), reads))
			{
				/*
				 * Setup for read() call on TUN/TAP device.
				  */
				
				ASSERT (!to_link.len);
				buf = read_tun_buf;
				
				read_tun_buffered ( (&tuntap), &buf, MAX_RW_SIZE_TUN (&frame));
				has_wrote = 1;
				NetKeepaliveGotWrite();
								
				/* Was TUN/TAP interface stopped? */
				if (tuntap_stop (buf.len))
				{
					signal_received = SIGTERM;
					signal_text = "tun-stop";
					msg (M_INFO, "TUN/TAP interface has been stopped, exiting");
					break;		  
				}
				
				/* Check the status return from read() */
				check_status (buf.len, "read from TUN/TAP", NULL, (&tuntap) );

				if (buf.len > 0) {
					link_socket_get_outgoing_addr (&buf, &link_socket,
				 &to_link_addr);
					to_link = buf;
				}
				else {
					to_link = nullbuf;
					free_to_link = false;
				}
			}
			
			/* TUN device ready to accept write */
			else if (TUNTAP_ISSET ( (&tuntap), writes))
			{
				/*
				* Set up for write() call to TUN/TAP
				* device.
				*/
				
				ASSERT (to_tun.len > 0);
				
				if (to_tun.len <= MAX_RW_SIZE_TUN(&frame))
				{
					/*
					 * Write to TUN/TAP device.
					 */
					
					int size;
					size = write_tun_buffered ( (&tuntap), &to_tun);
					to_tun = nullbuf;
				}
			}

			/* TCP/UDP port ready to accept write */
			
			else if (SOCKET_ISSET (link_socket, writes))
			{
				if (to_link.len > 0 && to_link.len <= MAX_RW_SIZE_LINK (&frame)) {
					/*
					 * Setup for call to send/sendto which will send 
					 * packet to remote over the TCP/UDP port.
					 */
					int size;
					ASSERT (addr_defined (&to_link_addr));

					/* Send packet */
					size = link_socket_write (&link_socket, &to_link, &to_link_addr);
					//free_buf (&to_link);	// caused crash
					to_link = nullbuf;
				}
				
			}
		}
		
	}

cleanup:

	/* pop our garbage collection level */

	free_buf (&to_link);
	free_buf (&to_tun);
	free_buf (&read_link_buf);
	free_buf (&read_tun_buf);

	link_socket_close (&link_socket);
	close_tun ( (&tuntap) );

	gc_free_level (gc_level);
	NetKeepaliveDestroy();
	return ret;	
}

static void
print_signal (int signum)
{
  switch (signum)
    {
    case SIGINT:
      msg (M_INFO, "SIGINT received, exiting");
      break;
    case SIGTERM:
      msg (M_INFO, "SIGTERM received, exiting");
      break;
    case SIGHUP:
      msg (M_INFO, "SIGHUP received, restarting");
      break;
    case SIGUSR1:
      msg (M_INFO, "SIGUSR1 received, restarting");
      break;
    default:
      msg (M_INFO, "Unknown signal %d received", signal_received);
      break;
    }
}

