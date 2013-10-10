/*
---------------------------------------------------------------------------
 Configuration file handling.
 config.c,v 1.21 2004/06/29 15:06:13 jpdionne Exp
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
#include <string.h>
#include <stdarg.h>

#include <sys/types.h>

#define _USES_NETDB_H_
#define _USES_SYS_IOCTL_H_
#define _USES_SYS_SOCKET_H_
#define _USES_NETINET_IN_H_
#define _USES_NET_IF_H_
#define _USES_ARPA_INET_H_

#include "platform.h"

#include "config.h"
#include "tsp_client.h"	// tspGetLocalAddress
#include "log.h"
#include "lib.h"
#include "cli.h"
#include "version.h"
#include "net.h"


/* Verify dns server for dns delegation */
static
int
VerifyDnsServer(char* server)
{
	char buffer[16];

	/* if its neither a valid v4 nor v6 address, never mind */
	
	if( INET_PTON(AF_INET, server, buffer) == 0 && INET_PTON(AF_INET6, server, buffer) == 0 )
		return 1; 

	return 0;
}



/* Add option to configuration struct */
static
int
AddOption(char *Option, char *Value, tConf *Conf)
{
  if (strcmp(Option, "tsp_dir") == 0) {
    Conf->tsp_dir = strdup(Value);
    return 0;
  }
  if (strcmp(Option, "tsp_version") == 0) {
	 /* do nothing but accept this as a valid option.
	    this allows us to remove this keyword
		from the config file while
		not breaking the client
		when used with older config files
		*/
	  return 0;
  }
  if (strcmp(Option, "server") == 0) {
    Conf->server = strdup(Value);
    return 0;
  }
  if (strcmp(Option, "client_v4") == 0) {
    Conf->client_v4 = strdup(Value);
    return 0;
  }
  if (strcmp(Option, "userid") == 0) {
    Conf->userid = strdup(Value);
    return 0;
  }
  if (strcmp(Option, "passwd") == 0) {
    Conf->passwd = strdup(Value);
    return 0;
  }
  if (strcmp(Option, "auth_method") == 0) {
    Conf->auth_method = strdup(Value);
    return 0;
  }
  if (strcmp(Option, "host_type") == 0) {
    Conf->host_type = strdup(Value);
    return 0;
  }
  if (strcmp(Option, "template") == 0) {
    Conf->template = strdup(Value);
    return 0;
  }
  if (strcmp(Option, "protocol") == 0) {
    Conf->protocol = strdup(Value);
    return 0;
  }
  if (strcmp(Option, "if_tunnel_v6v4") == 0) {
    Conf->if_tunnel_v6v4 = strdup(Value);
    return 0;
  }
  if (strcmp(Option, "if_tunnel_v6udpv4") == 0) {
    Conf->if_tunnel_v6udpv4 = strdup(Value);
    return 0;
  }
  if (strcmp(Option, "tunnel_mode") == 0) {
    if (strcmp(Value, STR_CONFIG_TUNNELMODE_V6ANYV4) == 0) {
      Conf->tunnel_mode = V6ANYV4;
    }
    else if (strcmp(Value, STR_CONFIG_TUNNELMODE_V6V4) == 0) {
      Conf->tunnel_mode = V6V4;
    }
    else if (strcmp(Value, STR_CONFIG_TUNNELMODE_V6UDPV4) == 0) {
      Conf->tunnel_mode = V6UDPV4;
    }
    else {
      printf("Invalid value for tunnel_mode.\n");
      return 1;
    }
    return 0;
  }
  if (strcmp(Option, "if_source") == 0) {
    Conf->if_source = strdup(Value);
    return 0;
  }
  if (strcmp(Option, "dns_server") == 0) {
    if(VerifyDnsServer(strdup(Value))) {
    	Conf->dns_server = strdup(Value);
	return 0;
    }
    printf("The DNS server must be FDQN address, not a numeric IP address.\n");
    return 1;
  }
  if (strcmp(Option, "routing_info") == 0) {
    Conf->routing_info = strdup(Value);
    return 0;
  }
  if (strcmp(Option, "if_prefix") == 0) {
    Conf->if_prefix = strdup(Value);
    return 0;
  }
  if (strcmp(Option, "prefixlen") == 0) {
    Conf->prefixlen = atoi(Value);
    return 0;
  }
  if (strcmp(Option, "retry_delay") == 0) {
     Conf->retry = atoi(Value);
     return 0;
  }
  if (strcmp(Option, "keepalive") == 0) {
    if (strcmp(Value, STR_CONFIG_BOOLEAN_TRUE) == 0) {
      Conf->keepalive = TRUE;
    }
    else if (strcmp(Value, STR_CONFIG_BOOLEAN_FALSE) == 0) {
      Conf->keepalive = FALSE;
    }
    else {
      printf("Invalid value for keepalive: %s\n", Value);
      return 1;
    }
    return 0;
  }
  if (strcmp(Option, "keepalive_interval") == 0) {
     Conf->keepalive_interval = atoi(Value);
     return 0;
  }
  if (strcmp(Option, "proxy_client") == 0 ) {
    if (strcmp(Value, STR_CONFIG_BOOLEAN_TRUE) == 0) {
      Conf->proxy_client = TRUE;
    }
    else if (strcmp(Value, STR_CONFIG_BOOLEAN_FALSE) == 0) {
      Conf->proxy_client = FALSE;
    }
    else {
      printf("Invalid value for proxy_client: %s\n", Value);
      return 1;
    }
    return 0;
  }
  if (strcmp(Option, "syslog_level") == 0 ) {
    /* TBD */
    return 0;
  }
  if (strcmp(Option, "syslog_facility") == 0) {
    /* TBD */
    return 0;
  }
  return 1;
}

int
tspReadConfigFile(char *File, tConf *Conf)
{
  FILE *fp;
  int Line = 0, err = 0;
  char *Option, *Value, Buffer[1024], Buffer2[1024], LineBuf[1024];

  if ((fp = fopen(File, "r")) == NULL) {
     return 1;
  }

  while (fgets(Buffer, sizeof(Buffer), fp)) {
    Line++;
    if (*Buffer == '#' || *Buffer == '\r' || *Buffer == '\n')
      continue;

    if (strlen(Buffer) && (Buffer[strlen(Buffer)-1] == '\n' || Buffer[strlen(Buffer)-1] == '\r'))
      Buffer[strlen(Buffer)-1] = '\0';

    if (strlen(Buffer) && (Buffer[strlen(Buffer)-1] == '\n' || Buffer[strlen(Buffer)-1] == '\r'))
      Buffer[strlen(Buffer)-1] = '\0';
     
    if((Option = strtok(Buffer, "=")) != NULL)  {
       if ((Value = strtok(NULL, "=")) == NULL) Value = "";
       if (memcmp(Value, "<<", 2)==0) {
         memset(Buffer2, 0, sizeof(Buffer));
         while (fgets(LineBuf, sizeof(LineBuf), fp)) {
            if (*LineBuf == '.') break;
            /* Check for string overflow */
            if (strlen(Buffer2) + strlen(LineBuf) + 1 > sizeof(Buffer2))
               err++;
            else
               strcat(Buffer2, LineBuf);
         }
         Value = Buffer2;
       }
       if (AddOption(Option, Value, Conf)==0)
         continue;
    }

    printf("Error in configuration file line %i\n", Line);
    err++;
  }

  if (!feof(fp)) {
    err++;
  }

  fclose(fp);

  if (err) {
    return(1);
  }

  return(0);
}

/*
   Initialize with default values, read configuration file and override
   defaults with config file values.
*/
int tspInitialize(int argc, char *argv[], tConf *Conf)
{
  tConf CmdLine;
  char *Templ = "template";

  Conf->tsp_dir      = NULL;
  Conf->server       = "";
  Conf->userid       = "anonymous";
  Conf->passwd       = "";
  Conf->client_v4    = "auto";
  Conf->template     = "";
  Conf->dns_server   = "";
  Conf->auth_method  = "any";
  Conf->protocol     = "default_route";
  Conf->tunnel_mode  = V6ANYV4;
  Conf->if_tunnel_v6v4 = "";
  Conf->if_tunnel_v6udpv4 = "";
  Conf->if_source    = "";
  Conf->routing_info = "";
  Conf->if_prefix    = "";
  Conf->host_type    = "host";
  Conf->prefixlen    = 0;
  Conf->retry	     = 1;
  Conf->syslog       = FALSE;
  Conf->keepalive    = TRUE;
  Conf->keepalive_interval = 30;
  Conf->proxy_client = FALSE;

  Conf->transport = NET_TOOLS_T_RUDP;  /* RUDP is default value */

  memset(&CmdLine, 0, sizeof(CmdLine));

  if(argc>1) {
	  if ( ParseArguments(argc, argv, &CmdLine) == -1) 
		  return -1;
  }

  if(tspReadConfigFile(FileName, Conf) != 0) {
	  Display(0, ELNotice, "tspInitialize", "Config file %s not found...", FileName);
	  return 1;
  }

  if(CmdLine.if_tunnel_v6v4) Conf->if_tunnel_v6v4 = CmdLine.if_tunnel_v6v4;
  if(CmdLine.if_tunnel_v6udpv4) Conf->if_tunnel_v6udpv4 = CmdLine.if_tunnel_v6udpv4;
  if(CmdLine.if_source) Conf->if_source = CmdLine.if_source;

  if(Conf->tsp_dir) {
     TspHomeDir = Conf->tsp_dir;
     if((ScriptDir = (char *)malloc((size_t)(strlen(Conf->tsp_dir)+strlen(Templ)+2)))==NULL) {
        Display(1, ELError, "Initialise", "Not enough memory!");
        return 1;
     }
     sprintf(ScriptDir, "%s%c%s", Conf->tsp_dir, DirSeparator, Templ);
     if((LogFileName = (char *)malloc((size_t)(strlen(Conf->tsp_dir)+strlen(LogFile)+2)))==NULL) {
        Display(1, ELError, "Initialise", "Not enough memory!");
        return 1;
     }
     sprintf(LogFileName, "%s%c%s", Conf->tsp_dir, DirSeparator, LogFile);
  } else {
     if((ScriptDir = (char *)malloc((size_t)(strlen(TspHomeDir)+strlen(Templ)+2)))==NULL) {
        Display(1, ELError, "Initialise", "Not enough memory!");
        return 1;
     }
     sprintf(ScriptDir, "%s%c%s", TspHomeDir, DirSeparator, Templ);
     LogFileName = LogFile;
  }
     
  return 0;
}

/*
   Validate all members of tConf.
*/

int tspVerifyConfig(tConf *Conf)
{
  int status;
# define eq(a,b) (strcmp(a,b) == 0)

  status = 0;

  if(IsPresent(Conf->client_v4)) {
     if(strcmp(Conf->client_v4, "auto")) { /* If not auto then check if ip address is ok. */
        struct in_addr address;
        in_addr_t net;

        net = inet_addr(Conf->client_v4);
		memcpy(&address, &net, sizeof(net));

        if(strcmp(Conf->client_v4, inet_ntoa(address))) {
           Display(1, ELError, "tspVerifyConfig", "invalid ipv4 address in client_v4 ->[%s] vs inet_toa(%s)", Conf->client_v4,  inet_ntoa(address));
           status = 1;
        }
     } else {
        if(IsPresent(Conf->if_source)) {
           int fd;
		   fd = socket(AF_INET, SOCK_DGRAM, 0);
		   
		   Display(1, ELError, "tspVerifyConfig", "source interface name : %s", Conf->if_source);
		   if ( (Conf->client_v4 = tspGetLocalAddress(fd)) != NULL)
			   Display(1, ELInfo, "tspVerifyConfig", "Using [%s] as source IPv4 address taken from %s.", Conf->client_v4, Conf->if_source);
		}
	 }
  } else {
	  Display(1, ELError, "tspVerifyConfig", "client_v4 must be specified!");
	  status = 1;
  }

  if(IsPresent(Conf->server)) {
     if(IsAll(ServerString, Conf->server) == 0) {
        Display(1, ELError, "tspVerifyConfig", "invalid server name.");
        status = 1;
     }
  } else {
    Display(1, ELError, "tspVerifyConfig", "server must be specified!");
    status = 1;
  }
  if(IsPresent(Conf->userid)) {
     if(strlen(Conf->userid) > 63) {
        Display(1, ELError, "tspVerifyConfig", "userid too long: must be less than 63 bytes");
        status = 1;
     } else if(IsAll(DNSLegal, Conf->userid) == 0) {
        Display(1, ELError, "tspVerifyConfig", "illegal chars in userid.");
        status = 1;
     }
     if(strcmp(Conf->userid, "anonymous")) {
        if(IsPresent(Conf->passwd)==0) {
           Display(1, ELError, "tspVerifyConfig", "passwd must be specified!");
           status = 1;
        }
     }
  } else {
    Display(1, ELError, "tspVerifyConfig", "userid must be specified!");
    status = 1;
  }


  if(IsPresent(Conf->if_tunnel_v6udpv4)) {
     if(IsAll(DNSLegal, Conf->if_tunnel_v6udpv4) == 0) {
        Display(1, ELError, "tspVerifyConfig", "illegal chars in if_tunnel_v6udpv4.");
        status = 1;
     }
  } else {
    if (Conf->tunnel_mode == V6UDPV4) {
      Display(1, ELError, "tspVerifyConfig", "if_tunnel_v6udpv4 must be specified!");
      status = 1;
    }
    else if (Conf->tunnel_mode == V6ANYV4) {
      Display(1, ELError, "tspVerifyConfig", "if_tunnel_v6udpv4 not specified, forcing tunnel_mode to v6v4.");
      Conf->tunnel_mode = V6V4;
    }
  }
				  
  if(IsPresent(Conf->if_tunnel_v6v4)) {
     if(IsAll(DNSLegal, Conf->if_tunnel_v6v4) == 0) {
        Display(1, ELError, "tspVerifyConfig", "illegal chars in if_tunnel_v6v4.");
        status = 1;
     }
  } else {
    if (Conf->tunnel_mode == V6V4) {
      Display(1, ELError, "tspVerifyConfig", "if_tunnel_v6v4 must be specified!");
      status = 1;
    }
    else if (Conf->tunnel_mode == V6ANYV4) {
      Display(1, ELError, "tspVerifyConfig", "if_tunnel_v6v4 not specified, forcing tunnel_mode to v6udpv4.");
      Conf->tunnel_mode = V6UDPV4;
    }
  }
  
  if ((Conf->proxy_client == TRUE) && (Conf->tunnel_mode == V6UDPV4)) {
      Display(1, ELError, "tspVerifyConfig", "tunnel_mode incompatible with proxy_client option.");
      status = 1;
  }
  
  if(IsPresent(Conf->template)) {
     if(IsAll(Filename, Conf->template) == 0) {
        Display(1, ELError, "tspVerifyConfig", "invalid template file name.");
        status = 1;
     }
  } else {
    Display(1, ELError, "tspVerifyConfig", "template must be specified!");
    status = 1;
  }

  if(IsPresent(Conf->host_type)) {
     if (!(eq(Conf->host_type, "host") ||  eq(Conf->host_type, "router"))) {
       Display(1, ELError, "tspVerifyConfig", "Bad host_type, expecting 'host' or 'router', read '%s'", Conf->host_type);
       status = 1;
     }
  }

  if (Conf->prefixlen < 0 || Conf->prefixlen > 64) {
	  Display(1, ELError, "tspVerifyConfig", "Bad prefix length, expecting between 0 and 64, read '%d'", Conf->prefixlen);
	  status = 1;
  }


  if(IsPresent(Conf->host_type)) {
     if(!(eq(Conf->protocol, "default_route") ||
   	eq(Conf->protocol, "bgp"))) {
       Display(1,ELError, "tspVerifyConfig", "Bad routing protocol, expecting 'default_route' or 'bgp'");
       status = 1;
     }
  }
  if(IsPresent(Conf->dns_server)) {
     char *Server;
     char *dns = strdup(Conf->dns_server);
     if (eq(Conf->host_type, "host")) {
       Display(1,ELError, "tspVerifyConfig", "DNS delegation is not supported for host_type=host");
       status = 1;
     }
     for(Server = strtok(dns, ":");Server; Server = strtok(NULL, ":")) {
        if(gethostbyname(Server) == NULL) {
           Display(1,ELError, "tspVerifyConfig", "DNS server name %s is not resolving.", Server);
           status = 1;
        }
     }
     free(dns);
  }

  if (Conf->keepalive == TRUE && Conf->keepalive_interval == 0) {
	  Display(1, ELError, "tspVerifyConfig", "Warning: Keepalive is TRUE with interval 0, forcing FALSE");
	  Conf->keepalive = FALSE;
  }

  return status;  
}
