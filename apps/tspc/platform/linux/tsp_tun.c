/*
---------------------------------------------------------------------------
 tsp_tun.c,v 1.33 2004/06/30 20:59:00 jpdionne Exp
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>

#define _USES_SYS_IOCTL_H_
#define _USES_SYS_TIME_H_
#define _USES_SYS_SOCKET_H_
#define _USES_ARPA_INET_H_
#define _USES_NETINET_IN_H_
#define _USES_NETINET_IP6_H_
#define _USES_NETINET_ICMP6_H_

#include "platform.h"

#include <linux/if.h>
#include <linux/if_tun.h>

#include "tsp_tun.h"
#include "net_ka.h"
#include "log.h"
#include "lib.h" /* Display */
#include "config.h" /* tBoolean define */
#include "errors.h"

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>


/* Initialize tun interface */
int TunInit(char *TunDevice) {
  int tunfd;
  struct ifreq ifr;
  char iftun[128];
  unsigned long ioctl_nochecksum = 1;
  
  /* for linux, force the use of "tun" */
  strcpy(iftun,"/dev/net/tun");

  tunfd = open(iftun,O_RDWR);
  if (tunfd == -1) {
    Display(0, ELError, "TunInit", "Error opening device: %s", iftun);
    Display(0, ELError, "TunInit", "Try \"modprobe tun\"");
    return INTERFACE_SETUP_FAILED;
  }
 
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TUN;
  strncpy(ifr.ifr_name, TunDevice, IFNAMSIZ);

  
  /* Enable debugging on tunnel device */
  /*
  Display(1, ELError, "TunInit" , "Enable tun device debugging");
  if(ioctl(tunfd, TUNSETDEBUG, (void *) &ioctl_nochecksum) == -1) {
	  Display(1, ELError,"TunInit","ioctl failed");
  }*/
  
  if((ioctl(tunfd, TUNSETIFF, (void *) &ifr) == -1) || 
     (ioctl(tunfd, TUNSETNOCSUM, (void *) ioctl_nochecksum) == -1)) { 	
    Display(1, ELError, "TunInit", "Error configuring tun device %s : %s", iftun,strerror(errno));
    close(tunfd);	

    return(-1);
  }
  
  return tunfd;
}

int TunMainLoop(int tunfd, SOCKET Socket, tBoolean keepalive, int keepalive_interval,
				char *local_address_ipv6, char *keepalive_address_ipv6)
{
	
  fd_set rfds;
  int count, maxfd;
  unsigned char bufin[2048];
  unsigned char bufout[2048];
  struct timeval timeout;
  int ret;

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

  while(1) {
    FD_ZERO(&rfds); 
    FD_SET(tunfd,&rfds);
	FD_SET(Socket,&rfds);
    maxfd = tunfd>Socket?tunfd:Socket;

    bufin[0]=0;
    bufin[1]=0;
    bufin[2]=0x86;
    bufin[3]=0xdd;

    if(keepalive) {
	    // Reinit timeout variable, in linux select modify it
	    timeout.tv_sec = 0;
	    timeout.tv_usec = 100000;	// 100 ms
		if (NetKeepaliveDo() == 2) 
			return KEEPALIVE_TIMEOUT;
	}
	
	ret = select(maxfd+1,&rfds,0,0,&timeout);
	if (ret>0) {
		if(FD_ISSET(tunfd,&rfds)) {
			/* data sent through udp tunnel */
			if ( (count = read(tunfd,bufout,sizeof(bufout) )) == -1 ) {
				Display(1, ELError, "TunMainLoop","Error reading from tun device");
				return(TUNNEL_ERROR);
			}
			NetKeepaliveGotWrite();
			if (send(Socket,bufout+4,count-4,0) != count-4) {
				Display(1, ELError, "TunMainLoop","Error writing to socket");
				return(TUNNEL_ERROR);
			}
		} 
		if(FD_ISSET(Socket,&rfds)) {
			/* data received through udp tunnel */
			count=recvfrom(Socket,bufin+4,2048-4,0,NULL,NULL);
			NetKeepaliveGotRead();
			if (write(tunfd,bufin,count+4) != count+4) {
				Display(1, ELError, "TunMainLoop", "Error writing to tun device");
				return(TUNNEL_ERROR);
			}
		}
	}
  }
}

				




