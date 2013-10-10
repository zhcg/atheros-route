/*
---------------------------------------------------------------------------
 tsp_tun.h,v 1.6 2004/02/11 04:52:46 blanchet Exp
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

#ifndef TUN_H
#define TUN_H

#include "config.h"		// tBoolean

extern void TunName(int tunfd, char *name);
extern int TunInit(char *TunDevice);
extern int TunMainLoop(int tun, int Socket, tBoolean keepalive,
                       int keepalive_interval, char *local_address_ipv6,
					   char *keepalive_address_ipv6);

#endif /* TUN_H */
