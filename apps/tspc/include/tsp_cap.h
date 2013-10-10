/*
---------------------------------------------------------------------------
 tsp_cap.h,v 1.7 2004/02/11 04:52:42 blanchet Exp
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

#ifndef _TSP_CAP_H_
#define _TSP_CAP_H_

#include "net.h"
#include "errors.h"

/*
   Capability bytes

   16      8      1
   |-------|------|
               `--' - TUNNEL TYPE 1-4
           `--' AUTH METHOD 5-8
   `------' RESERVED 9-16
*/

/* the tunnel modes values correspond to tTunnelMode defined in config.h */
#define TUNNEL_V6V4	0x0001
#define TUNNEL_V6UDPV4	0x0002
#define TUNNEL_ANY      0x0003

/* Authentication values */
#define AUTH_DIGEST_MD5	0x0040
#define AUTH_PLAIN	0x0020
#define AUTH_ANONYMOUS	0x0010
#define AUTH_ANY	0x00F0

typedef unsigned int tCapability;

tCapability tspSetCapability(char *, char *);
tErrors tspGetCapabilities(SOCKET, net_tools_t *, tCapability *, int);

#endif
