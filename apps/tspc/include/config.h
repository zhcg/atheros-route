/*
---------------------------------------------------------------------------
 config.h,v 1.9 2004/04/02 16:46:50 dgregoire Exp
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

#ifndef CONFIG_H
#define CONFIG_H

typedef enum {
  V6V4=1,
  V6UDPV4=2,
  V6ANYV4=3
} tTunnelMode;

/* these globals are defined by US used by alot of things in  */

#define STR_CONFIG_TUNNELMODE_V6ANYV4 	"v6anyv4"
#define STR_CONFIG_TUNNELMODE_V6V4 	"v6v4"
#define STR_CONFIG_TUNNELMODE_V6UDPV4 	"v6udpv4"

#define STR_XML_TUNNELMODE_V6ANYV4 	"v6anyv4"
#define STR_XML_TUNNELMODE_V6V4 	"v6v4"
#define STR_XML_TUNNELMODE_V6UDPV4 	"v6udpv4"

#ifdef FALSE
#undef FALSE
#endif

#ifdef TRUE
#undef TRUE
#endif

typedef enum {
  FALSE=0,
  TRUE
} tBoolean;

#define STR_CONFIG_BOOLEAN_FALSE	"no"
#define STR_CONFIG_BOOLEAN_TRUE		"yes"

typedef struct stConf {
  char 	
    *tsp_dir,
    *server,
    *userid,
    *passwd,
    *auth_method,
    *client_v4,
    *protocol,
    *if_tunnel_v6v4,
    *if_tunnel_v6udpv4,
    *if_source,
    *dns_server,
    *routing_info,
    *if_prefix,
    *template,
    *host_type;
  int keepalive_interval;
  int prefixlen;
  int retry;
  int syslog_level;
  int syslog_facility;
  int transport;
  tBoolean keepalive;
  tBoolean syslog;
  tBoolean proxy_client;
  tTunnelMode tunnel_mode;
} tConf;


/* conf checking */

#define Filename	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_."
#define ServerString	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-.:"
#define DNSLegal	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-."


/* imports defined in the platform dependant file */


extern char *FileName;
extern char *LogFile;
extern char *LogFileName;
extern char *ScriptInterpretor;
extern char *ScriptExtension;
extern char *ScriptDir;
extern char *TspHomeDir;
extern char DirSeparator;

extern int RootUid;

/* function exported */

int tspReadConfigFile(char *, tConf *);
int tspInitialize(int, char *[], tConf *);
int tspVerifyConfig(tConf *);

#endif



