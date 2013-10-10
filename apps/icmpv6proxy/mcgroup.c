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
*   mcgroup contains functions for joining and leaving multicast groups.
*
*/

#include "defs.h"
#include <sys/syslog.h>
       

/**
*   Common function for joining or leaving a MCast group.
*/
static int joinleave( char Cmd, int UdpSock, struct IfDesc *IfDp, struct in6_addr *mcastaddr ) 
{
    struct ipv6_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));
    int i;
    int result;

    mreq.ipv6mr_interface = IfDp->index;
    mreq.ipv6mr_multiaddr = *mcastaddr;

    if (Cmd =='j')
    {
        result = setsockopt(UdpSock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq));
/*
        atlog( LOG_DEBUG, 0, "IPV6_JOIN_GROUP\n");
        for (i = 0; i < sizeof(struct ipv6_mreq); i++)
            atlog( LOG_DEBUG, 0, "%02x ", ((char*)(&mreq))[i] & 255);
*/

        if (result == 0)
            return 1;
        else
            return 0;            
    }
    else
    {       
        result = setsockopt(UdpSock, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &mreq, sizeof(mreq));
/*
        atlog( LOG_DEBUG, 0, "IPV6_LEAVE_GROUP\n");
        for (i = 0; i < sizeof(struct ipv6_mreq); i++)
            atlog( LOG_DEBUG, 0, "%02x ", ((char*)(&mreq))[i] & 255);
        atlog( LOG_DEBUG, 0, "\n"); 
*/
        if (result == 0)
            return 1;
        else
            return 0;            
    }
    return 0;
}

/**
*   Joins the MC group with the address 'McAdr' on the interface 'IfName'. 
*   The join is bound to the UDP socket 'UdpSock', so if this socket is 
*   closed the membership is dropped.
*          
*   @return 0 if the function succeeds, 1 if parameters are wrong or the join fails
*/
int joinMcGroup( int UdpSock, struct IfDesc *IfDp, struct in6_addr *mcastaddr ) 
{
    return joinleave( 'j', UdpSock, IfDp, mcastaddr );
}

/**
*   Leaves the MC group with the address 'McAdr' on the interface 'IfName'. 
*          
*   @return 0 if the function succeeds, 1 if parameters are wrong or the join fails
*/
int leaveMcGroup( int UdpSock, struct IfDesc *IfDp, struct in6_addr * mcastaddr ) 
{
    return joinleave( 'l', UdpSock, IfDp, mcastaddr );
}
