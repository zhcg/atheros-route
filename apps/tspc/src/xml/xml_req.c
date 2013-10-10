/*
---------------------------------------------------------------------------
 xml_req.c,v 1.4 2004/02/11 04:52:54 blanchet Exp
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

#include "platform.h"

#include "xml_req.h"
#include "config.h"


/*
   Create XML request for tunnel
*/

char *tspBuildCreateRequest(tConf *conf)
{
  static char *Request[5000], Buffer[1024];

  /* XXX: This routine may overflow Request */
  memset(Request, 0, sizeof(Request));
  
  strcpy((char *)Request, "<tunnel action=\"create\" type=\"");
  
  if (conf->tunnel_mode == V6UDPV4)
     strcat((char *)Request, STR_XML_TUNNELMODE_V6UDPV4);
  else if (conf->tunnel_mode == V6V4)
    strcat((char *)Request, STR_XML_TUNNELMODE_V6V4);
  else
    strcat((char *)Request, STR_XML_TUNNELMODE_V6ANYV4);

  if (conf->proxy_client == TRUE)
    strcat((char *)Request, "\" proxy=\"yes\">\r\n");
  else
    strcat((char *)Request, "\" proxy=\"no\">\r\n");
  
  strcat((char *)Request, " <client>\r\n");

  snprintf(Buffer, sizeof Buffer,
           "  <address type=\"ipv4\">%s</address>\r\n", conf->client_v4);
  strcat((char *)Request, Buffer);

  /* ------------------------------------------------------- */
  /*                     KEEPALIVE                           */
  /* ------------------------------------------------------- */
  if (conf->keepalive == TRUE) {
    snprintf(Buffer, sizeof Buffer, "<keepalive interval=\"%d\"><address type=\"ipv6\">::</address></keepalive>",conf->keepalive_interval);
    strcat((char *) Request, Buffer);
  }
  
  /* ------------------------------------------------------- */
  /*                 ROUTER SECTION                          */
  /* ------------------------------------------------------- */
  if (strcmp(conf->host_type, "router") == 0) {

    strcat((char *)Request, "  <router");

    if (strcmp(conf->protocol, "default_route") != 0) {
      snprintf(Buffer, sizeof Buffer,
	      " protocol=\"%s\"",
	      conf->protocol);
      strcat((char *)Request, Buffer);
    }

    strcat((char *)Request, ">\r\n");

    if (conf->prefixlen==0) {
       conf->prefixlen = 48; /* default to 48*/
    }
    snprintf(Buffer, sizeof Buffer,
	      "   <prefix length=\"%d\"/>\r\n",
	      conf->prefixlen);
    strcat((char *)Request, Buffer);
    

    /* ------------------------------------------------------- */
    /*                 REVERSE DNS DELEGATION                  */
    /* ------------------------------------------------------- */
    if(strlen(conf->dns_server)) {
       char *Server;
       strcat((char *)Request, "   <dns_server>\r\n");
       for(Server = strtok(conf->dns_server, ":");Server; Server = strtok(NULL, ":")) {
          snprintf(Buffer, sizeof Buffer,
              "     <address type=\"dn\">%s</address>\r\n", Server);
          strcat((char *)Request, Buffer);
       }
       strcat((char *)Request, "   </dns_server>\r\n");
    }

    /* ------------------------------------------------------- */
    /*                 ROUTING PROTOCOL SECTION                */
    /* ------------------------------------------------------- */
    if (strcmp(conf->protocol, "default_route") == 0) {
      if (strlen(conf->routing_info) > 0) {
	snprintf(Buffer, sizeof Buffer,
		"    %s\r\n", 
		conf->routing_info);
	strcat((char *)Request, Buffer);
      }
    }

    strcat((char *)Request, "  </router>\r\n");
  }

  strcat((char *)Request, " </client>\r\n");
  strcat((char *)Request, "</tunnel>\r\n");

  return (char *)Request;
}

/*
  Create XML tunnel acknowledge
*/
char *tspBuildCreateAcknowledge()
{
  /*XXX Based on BuildCreateRequest - this is a reminder to fix memory usage of both functions.*/
  static char *Request[5000];

  memset(Request, 0, sizeof(Request));
  strcpy((char *)Request, "<tunnel action=\"accept\"></tunnel>\r\n");

  return (char *)Request;  
}
