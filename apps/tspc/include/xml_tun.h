/*
---------------------------------------------------------------------------
 xml_tun.h,v 1.2 2004/02/11 04:52:42 blanchet Exp
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
#ifndef XML_TUN_H
#define XML_TUN_H

#ifdef XMLTUN
# define ACCESS
#else
# define ACCESS extern
#endif

typedef struct stLinkedList {
  char *Value;
  struct stLinkedList *next;
} tLinkedList;

typedef struct stTunnel {
  char
    *action,
    *type,
    *lifetime,
    *proxy,
    *mtu,
    *client_address_ipv4,
    *client_address_ipv6,
    *client_dns_name,
    *server_address_ipv4,
    *server_address_ipv6,
    *router_protocol,
    *prefix_length,
    *prefix,
    *client_as,
    *server_as,
    *keepalive_interval,
    *keepalive_address_ipv6;
  tLinkedList 
    *dns_server_address_ipv4,
    *dns_server_address_ipv6,
    *broker_address_ipv4;
} tTunnel;


ACCESS int tspXMLParse(char *Data, tTunnel *Tunnel);

#undef ACCESS
#endif

/*----- xmlparse.h --------------------------------------------------------------------*/










