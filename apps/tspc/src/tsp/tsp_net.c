/*
---------------------------------------------------------------------------
 tsp_net.c,v 1.11 2004/06/14 20:45:24 dgregoire Exp
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#define _USES_SYS_SOCKET_H_

#include "platform.h"

#include "tsp_net.h"
#include "net.h"
#include "log.h"


SOCKET tspConnect(char *server, net_tools_t *nt) 
{
	 SOCKET socket;
	 char Buffer[MAXNAME], *srvname, *srvport;

	 snprintf(Buffer, sizeof Buffer, "%s", server);
	 srvname = strtok(Buffer, ":");
	 if((srvport = strtok(NULL, ":"))==NULL) {
		 srvport = SERVER_PORT;
	 }

#ifdef _USING_WINDOWS_UDP_PORT_
	WindowsUDPPort = atoi(srvport);
#endif

	 if(atoi(srvport) <= 0) {
		 LogPrintf(ELError, "tspConnect", "Service port not valid: %s", srvport);
		 return -1;
	 }

	 if((socket = nt->netopen(srvname, atoi(srvport))) < 0) {
		 LogPrintf(ELError, "tspConnect", "Not able to connect to service port %s", srvport);
		 return -1;
	 }

	 return socket;
}


int tspClose(SOCKET socket, net_tools_t *nt)
{
	return nt->netclose(socket);
}

/* XXX add all the error checking done in send / receive */ 

int tspSendRecv(SOCKET socket, tPayload *plin, tPayload *plout, net_tools_t *nt) {

	char string[] = "Content-length: %ld\r\n";
	char buffer[PROTOCOLFRAMESIZE];
	char *ptr_b, *ptr_c;
	int size, ret;

	/* add in content-length */

	snprintf(buffer, PROTOCOLFRAMESIZE, string, plin->size);
	size = strlen(buffer);
	memcpy(buffer + size, plin->payload, plin->size);

	buffer[size + plin->size] = 0;
	Display(3,ELInfo,"tspSendRecv","sent: %s",buffer);
	
	ret =  nt->netsendrecv(socket, buffer, size + plin->size, plout->payload, plout->size);

	if (ret == -1)
		return -1;

	/* but strip it from the returned string */
	/* XXX but it shouldn't.. it shloud validate the content length */


	ptr_c = strchr(plout->payload, '\n');
	size = strlen(ptr_c);
		
	ptr_b = (char *)malloc(++size); // need space for a little 0 at the end */
	memset(ptr_b, 0, size);
	memcpy(ptr_b, ptr_c, --size);   /* but need not to overwrite that little 0 */

	free(plout->payload);	
	plout->payload = ptr_b; 

	Display(3,ELInfo,"tspSendRecv","recv: %s",plout->payload);

	return ret;
}

int tspSend(SOCKET socket, tPayload *pl, net_tools_t *nt) 
{
	char Buffer[PROTOCOLFRAMESIZE];
	int ClSize;
	int ret = -1;

	snprintf(Buffer, PROTOCOLFRAMESIZE, "Content-length: %ld\r\n", pl->size);
	ClSize = strlen(Buffer);

	if (ClSize + pl->size > PROTOCOLFRAMESIZE) {
		LogPrintf(ELError, "tspSend", "Payload size is bigger than PROTOCOLFRAMESIZE");
		return -1;
	}

	memcpy(Buffer + ClSize,pl->payload, pl->size);

	Buffer[ClSize + pl->size] = 0;
	Display(3,ELInfo,"tspSend","sent: %s",Buffer);
	
	if ( (ret = nt->netsend(socket, Buffer, ClSize + pl->size)) == -1) {
		LogPrintf(ELError, "tspSend", "Not able to write tsp request to server socket");
		return ret;
	}
  
  return ret;
}

int tspReceive(SOCKET socket, tPayload *pl, net_tools_t *nt) 
{
	int BytesTotal = 0, BytesRead = 0, BytesLeft = 0;
	char Buffer[PROTOCOLFRAMESIZE+1];
	char *StartOfPayload;

	memset(Buffer,0,sizeof(Buffer));

	if (pl->payload) free(pl->payload);
	if ((BytesRead = nt->netrecv(socket, Buffer, sizeof(Buffer))) <= 0) {
		LogPrintf(ELError, "tspReceive", "Not able to read from server socket");
		return PROTOCOL_EREAD;
	}

	Display(3,ELInfo,"tspReceive","recv: %s",Buffer);

	if (memcmp(Buffer, "Content-length:", 15)) {
		LogPrintf(ELError, "tspReceive", "Was expecting 'Content-length:', received %s", Buffer);
		return PROTOCOL_ERROR;
	}

/* Start of payload is after Content-length: XX\r\n*/

	if ((StartOfPayload = strchr(Buffer,'\n')) == NULL) {
		LogPrintf(ELError, "tspReceive", "Invalid response received");
		return PROTOCOL_ERROR;
	}

	StartOfPayload++;
	BytesTotal = strlen(StartOfPayload);

	if (((pl->size = atol(Buffer + 15)) <= 0L) || BytesTotal > pl->size) {
		LogPrintf(ELError, "tspReceive", "Invalid payload size");
		return PROTOCOL_ERROR;
	}

	BytesLeft = pl->size - BytesTotal;

	while(BytesLeft) {
		if((BytesRead = nt->netrecv(socket, (StartOfPayload + BytesTotal), BytesLeft)) <= 0) {
			LogPrintf(ELError, "tspReceive", "Not able to read from server socket");
			return(PROTOCOL_EREAD);
		}
		
		BytesTotal += BytesRead;
		BytesLeft -= BytesRead;
	}

	if((pl->payload = (char *)malloc((pl->size) + 1)) == NULL) {
		LogPrintf(ELError, "tspReceive", "Memory allocation error for payload");
		return(PROTOCOL_EMEM);
	}
	
	memset(pl->payload, 0, sizeof(pl->payload));
	strcpy(pl->payload, StartOfPayload);

	return PROTOCOL_OK;
}
