/*
---------------------------------------------------------------------------
 tsp_local.c,v 1.17 2004/06/14 20:45:24 dgregoire Exp
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

/* Windows */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/timeb.h>

#define _USES_SYS_TIME_H_
#define _USES_SYS_SOCKET_H_

#include "platform.h"

#include "config.h"	/* tConf */
#include "xml_tun.h" /* tTunnel */
#include "net.h"	/* net_tools_t */
#include "tsp_client.h"/* tspMain */
#include "log.h"	/* Display */
#include "lib.h"	/* some constants */

#include "api.h"	/* windows TAP-win32 interface */

char *FileName  = "tspc.conf";
char *LogFile   = "tspc.log";
char *LogFileName = NULL;
char *ScriptInterpretor = NULL;
char *ScriptExtension = "bat";
char *ScriptDir = NULL;
char *TspHomeDir = "";
char DirSeparator = '\\';

int Verbose = 0;
int RootUid = 0;

int
tspGetTimeOfDay(void *timev) {
	struct _timeb timeptr;

	struct timeval *tv = (struct timeval *) timev;
	memset(tv, 0, sizeof(struct timeval));
	memset(&timeptr, 0, sizeof(timeptr));

	_ftime(&timeptr);							// get the time, windows style

	tv->tv_sec = timeptr.time;
	tv->tv_usec = timeptr.millitm * 1000;		// from milisecond to microsecond
	return 0;
}


int
tspInetPton(int af, const char *src, void *dst) {
	unsigned long ret;
	char buf[1024];
	int sa_size = 1024;
	
	memset(buf, 0, sizeof(buf));

	if ( (ret = WSAStringToAddress( (LPTSTR)src, af, NULL, (LPSOCKADDR)buf, &sa_size)) != 0) {
		printf("WSAStringToAddress error: %i\n", WSAGetLastError());
		return 0;
	}
	
	if (af == AF_INET6) {
		struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *) buf;
		memcpy(dst, &sa6->sin6_addr, sizeof(struct in6_addr));
		return 1;
	}

	else if ( af == AF_INET) {
		struct sockaddr_in *sa4 = (struct sockaddr_in *) buf;
		memcpy(dst, &sa4->sin_addr, sizeof(struct in_addr));
		return 1;
	}

	return 0;
}

char *
tspGetLocalAddress(SOCKET socket) 
{
	struct sockaddr_in addr;
	int len;
	char *buf;

	len = sizeof addr;
	if (getsockname(socket, (struct sockaddr *)&addr, &len) < 0) {
		Display(1, ELError, "tspGetLocalAddress", "Error while trying to find source ip address");
		return NULL;
	}
	
	buf = inet_ntoa(addr.sin_addr);
	return buf;
}

static
int
tspGetLocalPort(SOCKET socket) {
	struct sockaddr_in addr;
	int len;

	len = sizeof addr;
	if (getsockname(socket, (struct sockaddr *)&addr, &len) < 0) {
		perror("getsockname");
		Display(1, ELError, "tspGetLocalPort", "Error while trying to find source ip port.");
		return -1;
	}
	
	return htons(addr.sin_port);
}

void
tspSetEnv(char *Variable, char *Value, int Flag) {
	char *buf;

    if(Value) {
		int size=(strlen(Variable) + strlen(Value) + 2);
		if((buf=malloc(size)) == NULL) {
			Display(1, ELError, "SetEnv", "Not enough memory!");
			exit(1);
		}
		
		snprintf(buf, size, "%s=%s", Variable, Value);
		putenv(buf);
		Display(2, ELNotice, "tspSetEnv", "%s", buf);
		free(buf);
	}
}


int
tspStartLocal(SOCKET s, tConf *conf, tTunnel *tunnel, net_tools_t *nt) {
	int status;
	int port;
	int keepalive_interval = 0;
	char *localhost;

	port = tspGetLocalPort(s);
	localhost = tspGetLocalAddress(s);

	if (tunnel->keepalive_interval != NULL) {
		keepalive_interval = atoi(tunnel->keepalive_interval);
		Display(3, ELInfo, "tspStartLocal", "keepalive interval: %s\n", tunnel->keepalive_interval);
	}

	shutdown(s,  SHUT_RDWR);

	conf->if_tunnel_v6udpv4 = TunGetTunName();
	status = tspSetupInterface(conf, tunnel);

	if (status != 0) { 
		Display(0, ELError, "tspStartLocal", "Script failed to execute correctly");
		return INTERFACE_SETUP_FAILED;
	}

	/* for a V6UDPV4 tunnel, we jump in api.c */

	if ( strcasecmp(tunnel->type, STR_CONFIG_TUNNELMODE_V6UDPV4) == 0 ) 
		status = TunMainLoop(s, conf->if_tunnel_v6udpv4, localhost, port, conf->server, WindowsUDPPort, 
			0, conf->keepalive, keepalive_interval, tunnel->client_address_ipv6,
			tunnel->keepalive_address_ipv6);

	/* For a V6V4 tunnel, we jump into net_ka.c into a simple loop
	   unless there is no keepalive */

	else if ( strcasecmp(tunnel->type, STR_CONFIG_TUNNELMODE_V6V4) == 0 ) {
		if (keepalive_interval == 0)
			return NO_ERROR; /* if there is no keepalive, we can exit safe at this point */
		status = NetKeepaliveV6V4Tunnel(tunnel->client_address_ipv6,
		tunnel->server_address_ipv6, keepalive_interval);
	}

	return status;
}

int main(int argc, char *argv[]) 
{
	/* Initialize windows sockets */

	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	
	wVersionRequested = MAKEWORD( 2, 2 );
	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 ) {
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */
		return -1;
	}

	/* entry point */
	return tspMain(argc, argv);
}

