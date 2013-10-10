/*
---------------------------------------------------------------------------
 tsp_client.h,v 1.9 2004/02/11 04:52:42 blanchet Exp
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

#ifndef _TSP_CLIENT_H_
#define _TSP_CLIENT_H_

#include "config.h"	/* tConf */
#include "xml_tun.h"	/* tTunnel , tspXMLParse() */
#include "tsp_net.h"	/* tPayload */
#include "net.h"	/* net_tools_t */

extern int tspExtractPayload(char *, tTunnel *);
extern void tspClearPayload(tPayload *);
extern int tspGetStatusCode(char *);
extern char *tspGetStatusMsg(char *);
extern char *tspAddPayloadString(tPayload *, char *);

extern int tspSetupTunnel(tConf *, net_tools_t *[], int version_index);
extern int tspMain(int, char *[]);

/* IMPORTS, should be defined in platform/tsp_local.c
 * this will be ran when the nego is done.
 */

extern char	*tspGetLocalAddress(SOCKET);
extern int		tspStartLocal(SOCKET, tConf *, tTunnel *, net_tools_t *);
extern void	tspSetEnv(char *, char *, int);

#endif

