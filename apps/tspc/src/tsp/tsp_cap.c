/*
---------------------------------------------------------------------------
 tsp_cap.c,v 1.13 2004/02/11 04:52:53 blanchet Exp
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

#include "tsp_cap.h"
#include "tsp_client.h"	// tspGetStatusCode()
#include "net.h"
#include "log.h"
#include "version.h"

/* static functions */

/* Convert a CAPABILITY string in corresponding bit in capability flag */

static
tCapability
tspExtractCapability(char *String) {
	tCapability flags;
	char *s, *e, Token[strlen(String)+1], Value[strlen(String)+1];
	int len;

	flags = 0;
	*Token = *Value = 0;

	for(s=e=String+11; *e; e++) {
		if(*e== ' ' || *e == '\r' || *e == '\n' || *e == 0) {
			if(s!=e) {
				if(*Token && (*Value == 0)) {
					len = (int)((char *)e-(char *)s);
					memcpy(Value, s, len);
					Value[len] = 0;
				}
				if(*Token && *Value) {
					flags |= tspSetCapability(Token,Value);
					*Value = *Token = 0;
				}
			}
			s = ++e;
		}

		if((*e=='=' || *e== ' ' || *e == '\r' || *e == '\n' || *e == 0) && (e != s)) {
			len = (int)((char *)e-(char *)s);
			memcpy(Token, s, len);
			Token[len] = 0;
			s = ++e;
		}
	}
	return flags;
}

/* exports */

/* Return the capability flag corrsponding to the token */
tCapability
tspSetCapability(char *Token, char *Value) {
	if(strcmp("TUNNEL", Token)==0) {
		if(strcmp("V6V4", Value)==0)
			return TUNNEL_V6V4;
		if(strcmp("V6UDPV4", Value)==0)
			return TUNNEL_V6UDPV4;
	}
	if(strcmp("AUTH", Token)==0) {
		if(strcmp("DIGEST-MD5", Value)==0)
			return AUTH_DIGEST_MD5;
		if(strcmp("ANONYMOUS", Value)==0)
			return AUTH_ANONYMOUS;
		if(strcmp("PLAIN", Value)==0)
			return AUTH_PLAIN;
	}
	return 0;
}

tErrors
tspGetCapabilities(SOCKET socket, net_tools_t *nt, tCapability *capability, int version_index) {
	char dataout[256];
	char datain[2048];

	snprintf(dataout, sizeof(dataout), "VERSION=%s", TspProtocolVersionStrings[version_index]);
	memset(datain, -0, sizeof(datain));

	if (nt->netsendrecv(socket, dataout, strlen(dataout), datain, sizeof(datain)) == -1) 
		return SOCKET_ERROR;

	if(memcmp("CAPABILITY ", datain, 11) == 0)
		*capability = tspExtractCapability(datain);
	else {
		if ( tspGetStatusCode(datain) == 302 )
			return TSP_VERSION_ERROR;
		Display(1, ELInfo, "Capability", "Error received from server: %s\n", datain);
		return TSP_ERROR;
	}

	return NO_ERROR;
}

