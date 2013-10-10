/*
---------------------------------------------------------------------------
 tsp_local.c,v 1.32 2004/07/07 19:25:47 jpdionne Exp
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

/* LINUX */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define _USES_SYS_TIME_H_
#define _USES_SYS_SOCKET_H_
#define _USES_NETINET_IN_H_
#define _USES_ARPA_INET_H_

#include "platform.h"

/* get data types needed here */

#include "config.h"	/* tConf */
#include "xml_tun.h" /* tTunnel */
#include "net.h"	/* net_tools_t */
#include "tsp_net.h"/* tspClose */
#include "tsp_client.h"/*tspmain*/
#include "net_ka.h"		/* NetKeepaliveV6V4Tunnel */

/* some globals and the logging */

#include "lib.h"
#include "log.h"
#include "errors.h"

#include "tsp_tun.h"   /* linux's tun */
#include "tsp_setup.h"

/* these globals are defined by US used by alot of things in  */

char *FileName  = "tspc.conf";
char *LogFile   = "tspc.log";
char *LogFileName = NULL;
char *ScriptInterpretor = "/bin/sh";
char *ScriptExtension = "sh";
char *ScriptDir = NULL;
char *TspHomeDir = "/usr/local/etc/tsp";
char DirSeparator = '/';

int Verbose = 0;
int RootUid = 0;

/* Verify for ipv6 support */
static
int tspTestIPv6Support() 
{
	struct stat buf;
	if(stat("/proc/net/if_inet6",&buf) == -1) {
		Display(0,ELError,"tspTestIPv6Support","No IPv6 support found");
		Display(0,ELError,"tspTestIPv6Support","Try \"modprobe ipv6\"");
		return INTERFACE_SETUP_FAILED;
	}
	Display(2,ELInfo,"tspTestIPv6Support","IPv6 support found");

	return NO_ERROR;	
}

/* linux specific to setup an env variable */

void
tspSetEnv(char *Variable, char *Value, int Flag)
{
    setenv(Variable, Value, Flag);
	Display(2, ELNotice, "tspSetEnv", "%s=%s", Variable, Value);
}

/* tspSetupTunnel() will callback here */

char *
tspGetLocalAddress(SOCKET socket) 
{
	struct sockaddr_in addr;
	int len;
	char *buf;

	len = sizeof addr;
	if (getsockname(socket, (struct sockaddr *)&addr, &len) < 0) {
		Display(1, ELError, "tspGetLocalAddress", "Error while trying to find source ip address.");
		return NULL;
	}
	
	buf = inet_ntoa(addr.sin_addr);
	return buf;
}


/* tspSetupTunnel() will callback here */

/* start locally, ie, setup interface and any daemons or anything needed */

int tspStartLocal(SOCKET socket, tConf *c, tTunnel *t, net_tools_t *nt) 
{
	  int status = NO_ERROR;
	  int keepalive_interval = 0;

	  /* Test for root privileges */
	if(geteuid() != 0) {
		Display(0, ELError, "tspStartLocal", "FATAL: You must be root to setup a tunnel");
		return INTERFACE_SETUP_FAILED;
	}

	/* Check Ipv6 support */
	Display(2, ELNotice, "tspStartLocal", "Checking for linux ipv6 support...");
	if(tspTestIPv6Support() == INTERFACE_SETUP_FAILED)
		return INTERFACE_SETUP_FAILED;	
	  
	/* start the tunneler service */
	if (t->keepalive_interval != NULL) {
		keepalive_interval = atoi(t->keepalive_interval);
		Display(3, ELInfo, "tspStartLocal", "keepalive interval: %s\n", t->keepalive_interval);
	 }
	{
		  int tunfd;

		  Display(0, ELInfo, "tspStartLocal", "Going daemon, check %s for tunnel creation status", LogFile);

		  if (daemon(0,0) == -1) {
			  Display(1, ELError, "tspStartLocal", "Unable to fork.");
			  return INTERFACE_SETUP_FAILED;
			  } 

		  if (strcasecmp(t->type, STR_CONFIG_TUNNELMODE_V6UDPV4) == 0 ) {
			  if ((tunfd = TunInit(c->if_tunnel_v6udpv4)) == -1 ) {
				  Display(1, ELError, "tspStartLocal", "Unable to initialize tun device.");
				  return(INTERFACE_SETUP_FAILED);
			  }
		  }
		
		/* now, run the config script without 
		   giving it our tunnel file descriptor.

		   This is important because otherwise
		   the tunnnel will stay open even if we get killed
		   */
			  {
				  int pid = fork();
				  if (pid < 0)
					  // fork() error
					  return INTERFACE_SETUP_FAILED;
				  
				  else if (pid == 0) {	// child
					  close(tunfd);
					  if (tspSetupInterface(c, t) != 0)
						  exit(INTERFACE_SETUP_FAILED);
					  exit(0);
				  }
				  
				  else {	//parent
					  int s = 0;
					  Display(1, ELInfo, "tspStartLocal", "Waiting for setup script to complete");
					  if (wait(&s) == pid) { // ok our child returned 
						  if ( !WIFEXITED(s) ) {
							  Display(0, ELError, "tspStartLocal", "Script failed to execute correctly");
							  return INTERFACE_SETUP_FAILED;
						  }
						  if ( WEXITSTATUS(s) != 0 ) {
							  Display(0, ELError, "tspStartLocal", "Script failed to execute correctly");
							  return INTERFACE_SETUP_FAILED;
						  }
						  // else everything is fine
					  }
					  else { // error occured we have no other child
						  Display(1, ELError, "tspStartLocal", "Error while waiting for script to complete");
						  return INTERFACE_SETUP_FAILED;
					  }
				  }
			  }

			  if (strcasecmp(t->type, STR_CONFIG_TUNNELMODE_V6UDPV4) == 0 ) {
				  status = TunMainLoop(tunfd, socket, c->keepalive,
					  atoi(t->keepalive_interval), t->client_address_ipv6,
					  t->keepalive_address_ipv6);
				  /* We got out of main loop = keepalive timeout || tunnel error */
				  close(tunfd);
				  tspClose(socket, nt);
				  return status;
			  }
			  
			  else if (strcasecmp(t->type, STR_CONFIG_TUNNELMODE_V6V4) == 0 ) {
				  if (keepalive_interval == 0)
					  return NO_ERROR; /* if there is no keepalive, we can exit safe at this point */
				  status = NetKeepaliveV6V4Tunnel(t->client_address_ipv6,
					  t->keepalive_address_ipv6, keepalive_interval);
				  return status;
			  }
		  }

	return INTERFACE_SETUP_FAILED;/* should never reach here */
}
	

  


int main(int argc, char *argv[]) 
{

	/* entry point */
	
	return tspMain(argc, argv);
	
}
	
	

