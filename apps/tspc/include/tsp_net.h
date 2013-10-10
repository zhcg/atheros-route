/*
---------------------------------------------------------------------------
 tsp_net.h,v 1.5 2004/02/11 04:52:42 blanchet Exp
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

#ifndef _TSP_NET_H_
#define _TSP_NET_H_


#include "net.h"

/* definitions and constants */

#define MAXNAME 1024
#define SERVER_PORT "3653"
#define PROTOCOLMAXPAYLOADCHUNK	2048
#define PROTOCOLFRAMESIZE	4096
#define PROTOCOLMAXHEADER	70

enum { PROTOCOL_OK, PROTOCOL_ERROR, PROTOCOL_EMEM, PROTOCOL_ESYNTAX, PROTOCOL_ESIZE, PROTOCOL_EREAD };

/* structures */

typedef struct stPayload {
  long	size, PayloadCapacity;
  char *payload;
} tPayload;


/* exports */

extern SOCKET tspConnect(char *, net_tools_t *);
extern int tspClose(SOCKET, net_tools_t *);

extern int tspSendRecv(SOCKET socket, tPayload *, tPayload *, net_tools_t *) ;
extern int tspSend(SOCKET, tPayload *, net_tools_t *);
extern int tspReceive(SOCKET, tPayload *, net_tools_t *);



#endif 

