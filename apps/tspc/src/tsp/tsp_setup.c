/*
---------------------------------------------------------------------------
 tsp_setup.c,v 1.16 2004/06/11 02:14:28 jpicard Exp
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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "platform.h"

#include "tsp_setup.h"

#include "config.h"	// tConf
#include "xml_tun.h"	// tTunnel
#include "log.h"	// Display()
#include "lib.h"	// IsAll()


/*  Should be defined in platform.h  */
#ifndef SCRIPT_TMP_FILE
#define SCRIPT_TMP_FILE "/tmp/tspc-tmp.log"
#endif

/* to remove a warning. This is not right,
   it is already declared in tsp_client.h.
   */

extern void	tspSetEnv(char *, char *, int);

/* Execute cmd and send output to log subsystem */
static
int execScript(const char *cmd) 
{
	char buf[2048];
	
	char *ptr,*ptr2;
	int retVal, fd, n,i , used;

	memset(buf, 0, sizeof(buf));
	snprintf(buf,sizeof(buf),"%s > %s", cmd, SCRIPT_TMP_FILE);
	retVal = system(buf);

	if((fd = open(SCRIPT_TMP_FILE,O_RDONLY)) == -1) {
		Display(0,ELError,"tspSetupInterface","Can't open tmp file" SCRIPT_TMP_FILE);
		return -1;
	}


	ptr2 = buf;
	used = 0;
	while( (n = read(fd, ptr2, sizeof(buf) - used ) ) > 0) {
		ptr=ptr2;
		for(i=0;i<n;i++) {
			if(ptr2[i] == '\n' ) {
				ptr2[i] = 0;
				Display(2,ELInfo,"Script","%s",ptr);
				ptr=ptr2+i+1;
			}
		}
		ptr2=ptr;
		used = buf-ptr2;
	}

	close(fd);
	unlink(SCRIPT_TMP_FILE);
	
	return retVal;
}


int
tspSetupInterface(tConf *c, tTunnel *t) {

	char buf[1024];
	int Err = 0;
	int ret;

	Display(1, ELInfo, "tspSetupInterface", "tspSetupInterface beginning"); 
 
    /* setup locally */
	
	tspSetEnv("TSP_TUNNEL_MODE",         t->type, 1);
	tspSetEnv("TSP_HOST_TYPE",           c->host_type, 1);
	
	if (strcasecmp(t->type, STR_XML_TUNNELMODE_V6V4) == 0 )
		tspSetEnv("TSP_TUNNEL_INTERFACE",    c->if_tunnel_v6v4, 1);
	else
		tspSetEnv("TSP_TUNNEL_INTERFACE",    c->if_tunnel_v6udpv4, 1);
	
	tspSetEnv("TSP_HOME_INTERFACE",      c->if_prefix, 1);
	
	if(IsAll(IPv4Addr, t->client_address_ipv4))
		tspSetEnv("TSP_CLIENT_ADDRESS_IPV4", t->client_address_ipv4, 1);
	else {
		Display(1, ELError, "tspSetupInterface", "Bad value received from server for client_address_ipv4.\n");
		Err++;
	}
	
	if(IsAll(IPv6Addr, t->client_address_ipv6))
		tspSetEnv("TSP_CLIENT_ADDRESS_IPV6", t->client_address_ipv6, 1);
	else {
		Display(1, ELError, "tspSetupInterface", "Bad value received from server for client_address_ipv6.\n");
		Err++;
	}
	
	if(IsAll(IPv4Addr, t->server_address_ipv4))
		tspSetEnv("TSP_SERVER_ADDRESS_IPV4", t->server_address_ipv4, 1);
	else {
		Display(1, ELError, "tspSetupInterface", "Bad value received from server for server_address_ipv4.\n");
		Err++;
	}
	
	if(IsAll(IPv6Addr, t->server_address_ipv6))
		tspSetEnv("TSP_SERVER_ADDRESS_IPV6", t->server_address_ipv6, 1);
	else {
		Display(1, ELError, "tspSetupInterface", "Bad value received from server for server_address_ipv6.\n");
		Err++;
	}
	
	tspSetEnv("TSP_TUNNEL_PREFIXLEN",    "128", 1);

	if(t->prefix) {
		if(IsAll(IPv6Addr, t->prefix)) {
			char chPrefix[128];
			int len, sep;
			
			len = (atoi(t->prefix_length) % 16) ? (atoi(t->prefix_length) / 16 + 1) * 4 : atoi(t->prefix_length) / 16 * 4;
			sep = (atoi(t->prefix_length) % 16) ? (atoi(t->prefix_length) / 16) : (atoi(t->prefix_length) / 16) -1;
            
			memset(chPrefix, 0, 128);
			memcpy(chPrefix, t->prefix, len+sep);
			
			tspSetEnv("TSP_PREFIX",              chPrefix, 1);
		}
		else {
			Display(1, ELError, "tspSetupInterface", "Bad value received from server for prefix.\n");
			Err++;
		}
		
		if(IsAll(Numeric, t->prefix_length))
			tspSetEnv("TSP_PREFIXLEN", t->prefix_length, 1);
		else {
			Display(1, ELError, "tspSetupInterface", "Bad value received from server for prefix_length.\n");
			Err++;
		}
	}

	if(Err) {
		Display(1,ELError, "tspSetupInterface", "Errors in server response");
		return 1;
	} 
	
	snprintf(buf, sizeof buf, "%d", Verbose);
	
	tspSetEnv("TSP_VERBOSE",           buf, 1);
	tspSetEnv("TSP_HOME_DIR",          TspHomeDir, 1);
	if (ScriptInterpretor != NULL)
		snprintf(buf, sizeof buf, "%s \"%s%c%s.%s\"", ScriptInterpretor, ScriptDir, DirSeparator, c->template, ScriptExtension);
	else
		snprintf(buf, sizeof buf, "\"%s%c%s.%s\"", ScriptDir, DirSeparator, c->template, ScriptExtension);

	Display(2, ELInfo, "tspSetupInterface", "Executing configuration script: %s", buf);

	ret =  execScript(buf);	
	if (ret == 0) {
		Display(1, ELInfo, "tspSetupInterface", "Script completed sucessfully");
		Display(0, ELInfo, "tspSetupInterface", "Your IPv6 address is %s", t->client_address_ipv6);
	}

	return ret;
}


