/*
**  igmpproxy - IGMP proxy based multicast router 
**  Copyright (C) 2005 Johnny Egeland <johnny@rlo.org>
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**
**----------------------------------------------------------------------------
**
**  This software is derived work from the following software. The original
**  source code has been modified from it's original state by the author
**  of igmpproxy.
**
**  smcroute 0.92 - Copyright (C) 2001 Carsten Schill <carsten@cschill.de>
**  - Licensed under the GNU General Public License, version 2
**  
**  mrouted 3.9-beta3 - COPYRIGHT 1989 by The Board of Trustees of 
**  Leland Stanford Junior University.
**  - Original license can be found in the "doc/mrouted-LINCESE" file.
**
*/
/**
*   igmpproxy.c - The main file for the IGMP proxy application.
*
*   February 2005 - Johnny Egeland
*/

#include "defs.h"

static int igmpPState = 0;
    
// Local function Prototypes
int igmpProxyInit();
void igmpProxyCleanUp();

// The upstream VIF index
int upStreamVif = 0;

void 
setIgmpPState(int state)
{
    igmpPState = state;
}

int
getIgmpPState()
{
    return igmpPState;
}

/**
*   Handles the initial startup of IGMP PROXY.
*/
int
igmpProxyInit()
{
    //struct sigaction sa;

    // Initialize Routing table
    initRouteTable();
    initProxyif();
    
    // First thing we send a membership query in downstream VIF's...
    setStartCount();
    
    return 1;
}

/**
*   Clean up all on exit...
*/
void
igmpProxyCleanUp()
{

    atlog(LOG_DEBUG, 0, "clean handler called");

    upStreamVif = 0;
    clearProxyif();
    clearAllRoutes();           // Remove all routes.
}

