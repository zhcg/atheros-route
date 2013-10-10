/*
---------------------------------------------------------------------------
 tsp_client.c,v 1.34 2004/04/20 15:10:58 parent Exp
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

#define _USES_SYS_SOCKET_H_

#include "platform.h"

#include "tsp_client.h"

#include "net_udp.h"
#include "net_rudp.h"
#include "net_tcp.h"

#include "net.h"	/* net_tools_t */
#include "config.h"	/* tConf	*/
#include "tsp_cap.h"	/* tCapability */
#include "tsp_auth.h"	/* tspAuthenticate */
#include "tsp_net.h"	/* tPayload */
#include "xml_tun.h"	/* tTunnel, tspXMLParse() */
#include "xml_req.h"	/* build* */

#include "version.h"
#include "log.h"
#include "lib.h"		// tspGetErrorByCode()

char *TspProtocolVersionStrings[] = { "2.0.0", "1.0.1", 0 };

/*
   Call the XML parser. Data will be contained in the
   structure t (struct tTunnel)
*/

int
tspExtractPayload(char *Payload, tTunnel *t)
{
	char *p; /* First byte of payload. */
	int   rc;

	memset(t, 0, sizeof(tTunnel));

	Display(1, ELInfo, "tspExtractPayload", "Processing response from server");
	//Display(2, ELInfo, "tspExtractPayload", "Response is:\n");
	//Display(2, ELInfo, "tspExtractPayload", Payload);
	if((p = strchr(strchr(Payload, '\n'), '<')) == NULL)
		return 1;
	if((rc = tspXMLParse(p, t)) != 0)
		return 1;

/*  ShowInfo(&t); */

	return(0);
}

/* */

void tspClearPayload(tPayload *Payload)
{
	if (Payload->payload) {
		free(Payload->payload);
	}
	memset(Payload, 0, sizeof(tPayload));
}

/* */

int tspGetStatusCode(char *payload)
{
	if (payload)
		return atoi(payload);
	return(0);
}

/* */

char *tspGetStatusMsg(char *payload)
{
	static char Msg[1024], *ptr;

	if (!payload)
		return("");

	memset(Msg, 0, sizeof(Msg));

	if ((ptr = strchr(payload, '\n')) != NULL) {
		if (ptr - payload > sizeof Msg)
			ptr = payload + sizeof Msg;
		memcpy(Msg, payload, (int)(ptr-payload));
	} else {
		if ((ptr = strchr(payload, ' ')) != NULL) {
			snprintf(Msg, sizeof Msg, "%s", ptr+1);
		} else {
			snprintf(Msg, sizeof Msg, "%s", payload);
		}
	}

	return(Msg);
}

/* */

char
*tspAddPayloadString(tPayload *Payload, char *Addition)
{
	char *NewPayload;

	if(Addition) {
		if(Payload->PayloadCapacity == 0) {
			if((NewPayload = Payload->payload = (char *)malloc(PROTOCOLMAXPAYLOADCHUNK)) == NULL) {
				LogPrintf(ELError, "tspAddPayloadString", "Not able to allocate memory for payload");
				return NULL;
			}
			*Payload->payload = 0;
			Payload->PayloadCapacity = PROTOCOLMAXPAYLOADCHUNK;
		}
		
		if((Payload->size + strlen(Addition) + 1) > Payload->PayloadCapacity) {
			Payload->PayloadCapacity += PROTOCOLMAXPAYLOADCHUNK;
			if((NewPayload = (char *) malloc(Payload->PayloadCapacity)) == NULL) {
				LogPrintf(ELError, "tspAddPayloadString", "Not able to allocate memory for payload");
				return NULL;
			}

			memcpy(NewPayload, Payload->payload, Payload->size + 1);
			free(Payload->payload);
			Payload->payload = NewPayload;
		}

		strcat(Payload->payload, Addition);
		Payload->size += strlen(Addition);
	}

	return Payload->payload;
}



/* */

int tspSetupTunnel(tConf *conf, net_tools_t *nt[], int version_index) 
{
	SOCKET socket;
	int status = 0, ret = 0;
	tPayload plin, plout;
	tCapability cap;
	tTunnel t;
	
	/* we have an index of the transport to use into the net_tools_t array */

	Display(2, ELInfo, "tspSetupTunnel", "Establishing connection with tunnel broker...");

	if ( (socket = tspConnect(conf->server, nt[conf->transport])) == -1 ) {
		Display(2, ELError, "tspSetupTunnel", "Unable to establish connection with %s", conf->server);
		tspClose(socket, nt[conf->transport]);
		return SOCKET_ERROR;
	}

	Display(1, ELInfo, "tspSetupTunnel", "Getting capabilities from server");
	ret = tspGetCapabilities(socket, nt[conf->transport], &cap, version_index);

	if (ret != NO_ERROR) {
		Display(0, ELError, "tspSetupTunnel", "tspGetCapabilities error %i: %s", ret, tspGetErrorByCode(ret));
		if (ret == SOCKET_ERROR)
			Display(0, ELError, "tspSetupTunnel", "if you are using udp, there is probably no udp listener at %s", conf->server);
		tspClose(socket, nt[conf->transport]);
		return ret;
	}		

	Display(2, ELInfo, "tspSetupTunnel", "Connection established");

	/* test if desired mode is offered by server */
	if ((conf->tunnel_mode & cap) == 0) {
		/* tunnel mode not supported on server */
		Display(0, ELError,"tspSetupTunnel", "Requested tunnel mode not supported on server %s", conf->server);
		tspClose(socket, nt[conf->transport]);
		return TSP_ERROR;
	}

	Display(1, ELInfo, "tspSetupTunnel", "Authenticating %s", conf->userid);
	if (tspAuthenticate(socket, conf->auth_method, conf->userid, conf->passwd, conf->server, cap, nt[conf->transport]) != 0 ) {
		Display(0, ELError,"tspSetupTunnel", "Authentification error");
		tspClose(socket, nt[conf->transport]);
		return AUTHENTIFICATION_ERROR;
	}

	if (strcmp(conf->client_v4, "auto") == 0) {
		conf->client_v4 = tspGetLocalAddress(socket);
	}

	Display(1, ELInfo, "tspSetupTunnel", "Authentication success");
	Display(1, ELInfo, "tspSetupTunnel", "Asking for a tunnel");

	/* prepare request */

	memset(&plin, 0, sizeof(plin));
	memset(&plout, 0, sizeof(plout));

	plin.payload = tspAddPayloadString(&plin, tspBuildCreateRequest(conf)); /* XXX completement debile */

	/* get some mem for the answer */

	plout.payload = (char *)malloc(PROTOCOLMAXPAYLOADCHUNK);
	plout.size = PROTOCOLMAXPAYLOADCHUNK;

        /* send it */
	memset(plout.payload, 0, plout.size);
	if ( tspSendRecv(socket, &plin, &plout, nt[conf->transport]) == -1) {
		Display(1, ELError, "tspTryServer", "Unable to send request to %s", conf->server);		
		tspClose(socket, nt[conf->transport]);
		return SOCKET_ERROR;
	}

	/* process it */

	if ( (status = tspGetStatusCode(plout.payload)) != 200 ) {
		Display(0, ELError, "tspTryServer", "Status error %i in tunnel negociation: %s", status, &plout.payload[1]);
		tspClose(socket, nt[conf->transport]);
		return status;
	}

	tspExtractPayload(plout.payload, &t);

	// free some memory

	free(plout.payload);
	plout.size = 0;

	free(plin.payload);
	plin.size=0;

	/* version 1.0.1 requires that we immediatly jump in tunnel mode */

	if (version_index == 1)
		goto start_show;
	
/* and acknowledge it */

	memset(&plin, 0, sizeof(plin));
	plin.payload = tspAddPayloadString(&plin, tspBuildCreateAcknowledge());

	if ( tspSend(socket, &plin, nt[conf->transport]) == -1 ) {
		Display(0, ELError, "tspTryServer", "Error in tunnel ackknowledge");
		tspClose(socket, nt[conf->transport]);
		return SOCKET_ERROR;
	}

	// free the last of the memory

	free(plin.payload);
	plin.size=0;

start_show:

	Display(0, ELInfo, "tspSetupTunnel", "Got tunnel parameters from server, setting up local tunnel");

	/* and start the show */
	ret = tspStartLocal(socket, conf, &t, nt[conf->transport]);

	/* now we are done */
	tspClose(socket, nt[conf->transport]);
	return ret;
}

/* */

int tspMain(int argc, char *argv[]) 
{
	int status = 1, i, was_connected = 0;
	tConf c;
	net_tools_t **nt;

	int retries = 5;	// bug #1948, fixed to 5 for now...
	int version_index = 0;
	int TspProtocolVersionStringsSize =
		GetSizeOfNullTerminatedArray(TspProtocolVersionStrings);

	/* get some memory */
	/* and fill it up with NULLs */

	nt =  malloc(sizeof(net_tools_t *) * NET_TOOLS_T_SIZE );

	for (i = 0; i < NET_TOOLS_T_SIZE; i++) {
	  nt[i] = (net_tools_t *) malloc (sizeof (net_tools_t) );
	  memset(nt[i], 0, sizeof(net_tools_t));
	}

	i = 0;
	
	/* now, fill up our array */
	/* the correct index to use is in tConf */

	nt[NET_TOOLS_T_RUDP]->netopen = NetRUDPConnect;
	nt[NET_TOOLS_T_RUDP]->netclose = NetRUDPClose;
	nt[NET_TOOLS_T_RUDP]->netsendrecv = NetRUDPReadWrite;
	nt[NET_TOOLS_T_RUDP]->netsend = NetRUDPWrite;
	nt[NET_TOOLS_T_RUDP]->netprintf = NetRUDPPrintf;
	nt[NET_TOOLS_T_RUDP]->netrecv = NetRUDPRead;

	nt[NET_TOOLS_T_UDP]->netopen = NetUDPConnect;
	nt[NET_TOOLS_T_UDP]->netclose = NetUDPClose;
	nt[NET_TOOLS_T_UDP]->netsendrecv = NetUDPReadWrite;
    nt[NET_TOOLS_T_UDP]->netsend = NetUDPWrite;
    nt[NET_TOOLS_T_UDP]->netprintf = NetUDPPrintf;
    nt[NET_TOOLS_T_UDP]->netrecv = NetUDPRead;

    nt[NET_TOOLS_T_TCP]->netopen = NetTCPConnect;
	nt[NET_TOOLS_T_TCP]->netclose = NetTCPClose;
	nt[NET_TOOLS_T_TCP]->netsendrecv = NetTCPReadWrite;
	nt[NET_TOOLS_T_TCP]->netsend = NetTCPWrite;
	nt[NET_TOOLS_T_TCP]->netprintf = NetTCPPrintf;
	nt[NET_TOOLS_T_TCP]->netrecv = NetTCPRead;

	LogInit("tspc", LogFile);

	Display( 1, ELInfo, "tspMain", "----- TSP Client Version %s Initializing -------------------------", TSP_CLIENT_VERSION);
	Display( 0, ELInfo, "tspMain", IDENTIFICATION);
	Display( 0, ELInfo, "tspMain", "Initializing (use -h for help)\n");

	if((status = tspInitialize(argc, argv, &c)) != 0) {	// *** we have no uninitialize
		if (status == -1) {
			// error parsing command line
		}
		else if (status == 1) {
			// some other error with the config file
		}
      
		status = 0; /* dont print error message */
		i = 1;	/* dont print tsp session done */
		/* just exits clean with no fuss. if tspInitialize failed,
		   it might be we are done already (checking config file),
		   ... but if it fails we want to exit cleanly. */
		goto endtspc;
	}
	
	if ((status = tspVerifyConfig(&c)) != 0)
		goto endtspc;

	/* first try with RUDP */
	c.transport = NET_TOOLS_T_RUDP;
	Display( 0, ELInfo, "tspMain", "\nConnecting to server with reliable udp");

try_server:

	if (c.transport == NET_TOOLS_T_TCP)
		Display( 0, ELInfo, "tspMain", "\nConnecting to server with tcp");

	Display( 1, ELInfo, "tspMain", "Using TSP protocol version %s",
		TspProtocolVersionStrings[version_index]); 

	status = tspSetupTunnel(&c, nt, version_index);

	/* if session to server failed, fallback in TCP if tunnel mode requested is v6anyv4
	   or v6anyv4 */
	
	if (status == SOCKET_ERROR) {
		if (was_connected == 1)	/* if we WERE connected and got a keepalive timeout,
								   we have a fresh new 5 chances */
			goto retry;

		else if ( c.transport == NET_TOOLS_T_RUDP && c.tunnel_mode == V6ANYV4) {
			Display (2, ELInfo, "tspMain", "Reliable udp connection failed, fallback to tcp and V6V4");
			c.tunnel_mode = V6V4;
			c.transport = NET_TOOLS_T_TCP;
			tspVerifyConfig(&c);
			goto try_server;
		}
		/* However, if transport is RUDP and tunnel is V6V4 and there
		   is no UDP listener, dont get stuck
		   */
		else if (c.transport == NET_TOOLS_T_RUDP && c.tunnel_mode == V6V4) {
			Display (2, ELInfo, "tspMain", "Reliable udp connection failed, fallback to tcp");
			c.transport = NET_TOOLS_T_TCP;
			tspVerifyConfig(&c);
			goto try_server;
		}
		/* if a TCP session failed in V6V4 because of a socket error we can
		safely trust that TCP is strong enought to know when to quit */
		else if (c.transport == NET_TOOLS_T_TCP && c.tunnel_mode == V6V4) {
			Display(0, ELError, "tspMain", "\nAll transports failed, quitting");
			goto endtspc;
		}

		else // else we "simply" failed
			goto endtspc;
	}

	/* if session failed because of a protocol problem, fallback the protocol */

	else if (status == TSP_VERSION_ERROR && version_index != TspProtocolVersionStringsSize - 1 ) {
		Display( 0, ELError, "tspMain", "Protocol version %s failed, backing to %s", 
			TspProtocolVersionStrings[version_index],
			TspProtocolVersionStrings[version_index+1]);
		version_index++;
		goto try_server;
	}

	/* if setup interface failed, done */

	else if (status == INTERFACE_SETUP_FAILED)
		goto endtspc;

	/* keepalive timeout? */

	else if (status == KEEPALIVE_TIMEOUT) {
		was_connected = 1;
		retries = 5;
		goto retry;
	}

	/* if we are here with no error, we can assume we are done */

	else if (status == NO_ERROR)
		goto endtspc;

	/* unsure error. the end. i know, i know. need to test tho. */

	else
		goto endtspc;
		
retry:
	if (retries-- == 0)
		goto endtspc;
	Display (0, ELError, "tspMain", "\nDisconnected, retry in %i seconds\n", c.retry);
	SLEEP(c.retry);
	goto try_server;

endtspc:

	if(status) {
		Display(0, ELError, "tspMain", "\nError is %i: %s", status, tspGetErrorByCode(status));
	}

	if (i != 1)
		Display(0, ELInfo, "tspMain", "TSP session done");

	for (i = 0; i < NET_TOOLS_T_SIZE; i++) 
          free (nt[i]);

	free(nt);
	LogClose();

	return(status);
}
