/*
---------------------------------------------------------------------------
 tsp_tun.c,v 1.22 2004/03/24 18:43:57 dgregoire Exp
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

/* FreeBSD */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>

#define _USES_SYS_IOCTL_H_
#define _USES_SYS_TIME_H_
#define _USES_SYS_SOCKET_H_
#define _USES_NET_IF_H_
#define _USES_ARPA_INET_H_
#define _USES_NETINET_IN_H_
#define _USES_NETINET_IP6_H_
#define _USES_NETINET_ICMP6_H_

#include "platform.h"

#include <fcntl.h>
#include <net/if_tun.h>

#include "tsp_tun.h"
#include "net_ka.h"
#include "log.h"
#include "lib.h" /* Display */
#include "config.h" /* tBoolean define */
#include "errors.h"


/* Get the name of the tun device using file descriptor */
void TunName(int tunfd, char *name) {
  struct stat sbuf;
  char *internal_buffer;
  
  if (fstat(tunfd,&sbuf) != -1) {
    internal_buffer = devname(sbuf.st_rdev, S_IFCHR);
    strcpy(name,internal_buffer);
  }
}

/* Initialize tun interface */
int TunInit(char *TunDevice) {
  int tunfd;
  int ifmode = IFF_POINTOPOINT | IFF_MULTICAST;
  int tundebug = 0;
  int tunhead = 1;
  char iftun[128];

  strcpy(iftun,"/dev/");
  strcat(iftun,TunDevice);

  tunfd = open(iftun,O_RDWR);
  if (tunfd == -1) {
    Display(1, ELError, "TunInit", "Error opening tun device: %s", iftun);
    return(-1);
  }
  
  if ((ioctl(tunfd,TUNSDEBUG,&tundebug) == -1) ||
      (ioctl(tunfd,TUNSIFMODE,&ifmode) == -1) ||
      (ioctl(tunfd,TUNSIFHEAD,&tunhead) == -1)) {
    close(tunfd);
    Display(1, ELError, "TunInit", "Error configuring tun device: %s", iftun);
    return(-1);
  }
  return tunfd;
}

int TunMainLoop(int tunfd, SOCKET Socket, tBoolean keepalive, int keepalive_interval,
				char *local_address_ipv6, char *keepalive_address_ipv6) {
  fd_set rfds;
  int count, maxfd;
  char bufin[2048];
  char bufout[2048];
  struct timeval timeout;
  int ret = 0;

  if (keepalive_interval != 0) {
	  timeout.tv_sec = 0;
	  timeout.tv_usec = 100000;	// 100 ms
	  NetKeepaliveInit(local_address_ipv6, keepalive_address_ipv6, keepalive_interval);
  }
  else {
	  keepalive = FALSE;
	  timeout.tv_sec = 7 * 24 * 60 * 60 ; /* one week */
	  timeout.tv_usec = 0;
  }

  bufin[0] = 0;
  bufin[1] = 0;
  bufin[2] = 0;
  bufin[3] = 0x1c;

  while(1) {
    FD_ZERO(&rfds); 
    FD_SET(tunfd,&rfds);
	FD_SET(Socket,&rfds);

    maxfd = tunfd>Socket?tunfd:Socket;

	if(keepalive) 
		if (NetKeepaliveDo() == 2)
			return KEEPALIVE_TIMEOUT;
  
    
    ret = select(maxfd+1,&rfds,0,0,&timeout);

    if (ret > 0) {
		if( FD_ISSET(tunfd, &rfds) ) {
			/* data sent through udp tunnel */
			ioctl(tunfd, FIONREAD, &count);
			if (count > sizeof(bufout)) {
				Display(1, ELError, "TunMainLoop", "Error reading from tun device, buffer too small");
				return(TUNNEL_ERROR);
			}
			if (read(tunfd, bufout, count) != count) {
				Display(1, ELError, "TunMainLoop", "Error reading from tun device");
				return(TUNNEL_ERROR);
			}
			NetKeepaliveGotWrite();
			if (send(Socket, bufout+4, count-4, 0) != count-4) {
				Display(1, ELError, "TunMainLoop", "Error writing to socket");
				return(TUNNEL_ERROR);
			}
		}

		if(FD_ISSET(Socket,&rfds)) {
			/* data received through udp tunnel */
			count=recvfrom(Socket,bufin+4,2048 - 4,0,NULL,NULL);
			NetKeepaliveGotRead();
			if (write(tunfd, bufin, count + 4) != count + 4) {
				Display(1, ELError, "TunMainLoop", "Error writing to tun device");
				return(TUNNEL_ERROR);
			}
		}
	}//if (ret>0)
  }//while(1)
}

				
				


