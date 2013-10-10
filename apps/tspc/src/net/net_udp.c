/*
---------------------------------------------------------------------------
 net_udp.c,v 1.7 2004/02/11 04:52:51 blanchet Exp
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

#define _USES_SYS_SOCKET_H_
#define _USES_NETINET_IN_H_
#define _USES_ARPA_INET_H_
#define _USES_NETDB_H_

#include "platform.h"

#include "net_udp.h"
#include "net.h"

SOCKET
NetUDPConnect(char *Host, int Port)
{
  SOCKET              sockfd;
  struct sockaddr_in  serv_addr;
  struct in_addr      *addr;

  if((addr = NetText2Addr(Host)) == NULL) {
    return -1;
  }

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family      = AF_INET;
  serv_addr.sin_port        = htons(Port);
  serv_addr.sin_addr.s_addr = addr->s_addr;

  /*
   * Open a UDP socket.
   */

  if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    return -1;
  }

  /*
   * Connect to the server.
   */

  if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    return -1;
  }

  return sockfd;
}


int
NetUDPClose(SOCKET Socket)
{
  shutdown(Socket, SHUT_RDWR);
  return close(Socket);
}


int
NetUDPReadWrite(SOCKET sock, unsigned char *bi, size_t li, unsigned char *bo, size_t lo) 
{
	if ( NetUDPWrite(sock, bi, li) != li)
		return -1;
	
	return NetUDPRead(sock, bo, lo);
}
	


int
NetUDPWrite(SOCKET sock, unsigned char *b, size_t l) 
{
	size_t nleft, nwritten;
	unsigned char	*ptr;

	ptr = b;	/* can't do pointer arithmetic on void* */
	nleft = l;
	while (nleft > 0) {
		if ((nwritten = send(sock, ptr, nleft, 0)) <= 0) {
			return nwritten;		/* error */
		}

		nleft -= nwritten;
		ptr   += nwritten;
	}
	return l;
}


int
NetUDPPrintf (SOCKET sock, char *out, size_t ol, char *Format, ...) 
{
	va_list argp;
	int Length;
	char Data[1024];

	va_start(argp, Format);
	vsnprintf(Data, sizeof Data, Format, argp);
	va_end(argp);

	Length = strlen(Data);

	/* XXX error checking */
	
	return NetUDPReadWrite(sock, Data, strlen(Data), out, ol);
}

int
NetUDPRead (SOCKET sock, unsigned char *b, size_t l) 
{
	return(recvfrom(sock, b, l, 0, NULL, NULL));

}


