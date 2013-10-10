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
*   mroute-api.c
*
*   This module contains the interface routines to the Linux mrouted API
*/


#define USE_LINUX_IN_H
#include "defs.h"
#include <unistd.h>


// MAX_MC_VIFS from mclab.h must have same value as MAXVIFS from mroute.h
#if MAX_MC_VIFS != MAXVIFS
//# error "constants don't match, correct mclab.h"
#endif
     
// need an IGMP socket as interface for the mrouted API
// - receives the IGMP messages
int         MRouterFD;        /* socket for all network I/O  */
char        *send_buf;           /* output packet buffer        */


// my internal virtual interfaces descriptor vector 
static struct MifDesc {
  struct IfDesc *IfDp;
} MifDescVc[ MAXMIFS ];


/*
** Initialises the mrouted API and locks it by this exclusively.
**     
** returns: - 0 if the functions succeeds     
**          - the errno value for non-fatal failure condition
*/
int enableMRouter()
{
    int Va = 1;

    struct sockaddr_in6 bindsock;

    bindsock.sin6_addr = downStreamIfDesc->linkAddr;
    bindsock.sin6_family = AF_INET6;
    bindsock.sin6_flowinfo = 0;
    bindsock.sin6_scope_id = downStreamIfDesc->index;
    bindsock.sin6_port = 0;


    if ( (MRouterFD  = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0 )
        return 0;

    if ( setsockopt( MRouterFD, IPPROTO_IPV6, MRT6_INIT, (void *)&Va, sizeof( Va ) ) < 0 ) 
        return 0;

    if ( setsockopt(MRouterFD,SOL_SOCKET,SO_REUSEADDR,&Va,sizeof(Va)) < 0 )
        return 0;

    if (bind(MRouterFD,(struct sockaddr *)(&bindsock), sizeof(bindsock)) < 0 )
        return 0;

    return 0;
}


/*
** Diables the mrouted API and relases by this the lock.
**          
*/
void disableMRouter()
{
    if ( setsockopt(MRouterFD, IPPROTO_IPV6, MRT6_DONE, 0, 0) || close( MRouterFD ) ) 
    {
        MRouterFD = 0;
        atlog( LOG_ERR, errno, "MRT_DONE/close" );
    }

    MRouterFD = 0;
}

/*
** Adds the interface '*IfDp' as virtual interface to the mrouted API
** 
*/
void addMIF( struct IfDesc *IfDp )
{
    struct mif6ctl MifCtl;
    struct MifDesc *MifDp;

    memset(&MifCtl, 0, sizeof(MifCtl));

    /* search free MifDesc
     */

    for( MifDp = MifDescVc; MifDp < VCEP( MifDescVc ); MifDp++ ) 
    {
        if( MifDp->IfDp == IfDp )   // Already Added
            return;
    }
    
    for( MifDp = MifDescVc; MifDp < VCEP( MifDescVc ); MifDp++ ) 
    {
        if( ! MifDp->IfDp )
            break;
    }
    
    /* no more space
     */
    if( MifDp >= VCEP( MifDescVc ) )
        atlog( LOG_ERR, errno, "addMIF, out of MIF space\n");

    MifDp->IfDp = IfDp;

    MifCtl.mif6c_mifi      = MifDp - MifDescVc; 
    MifCtl.mif6c_flags     = 0;                 /* no register */
    MifCtl.vifc_threshold  = IfDp->threshold;   /* Packet TTL must be at least 1 to pass them */
    MifCtl.mif6c_pifi      = IfDp->index;     /* physical interface index */
    MifCtl.vifc_rate_limit = IfDp->ratelimit;   /* hopefully no limit */

    atlog( LOG_DEBUG, 0, "Inside addMIF: setsockopt MRT6_ADD_MIF Mif-Ix %d PHY Ix %d Fl 0x%x %s\n", 
            MifCtl.mif6c_mifi, MifCtl.mif6c_pifi, MifCtl.mif6c_flags, MifDp->IfDp->Name );

    if( setsockopt( MRouterFD, IPPROTO_IPV6, MRT6_ADD_MIF, (char *)&MifCtl, sizeof( MifCtl ) ) )
    {
        atlog( LOG_ERR, errno, "\nMRT6_ADD_MIF failed %s\n\n", IfDp->Name );     
    }
    else
    {
        atlog( LOG_DEBUG, 0, "\nMRT6_ADD_MIF OK %s\n\n", IfDp->Name );             
    }
}

/*
** Adds the multicast routed '*Dp' to the kernel routes
**
** returns: - 0 if the function succeeds
**          - the errno value for non-fatal failure condition
*/
int addMRoute( struct MRouteDesc *Dp )
{
    int MifIx;
    struct mf6cctl CtlReq;
    int i;

    CtlReq.mf6cc_origin    = Dp->OriginAdr;
    CtlReq.mf6cc_mcastgrp  = Dp->McAdr;
    CtlReq.mf6cc_parent    = Dp->InVif;

    for ( MifIx = 0; MifIx < MAX_MC_MIFS; MifIx++ ) 
    {
        if ( Dp->TtlVc[ MifIx ] > 0 )
        {
            IF_SET( MifIx, &CtlReq.mf6cc_ifset );
        }
    }

    if ( setsockopt( MRouterFD, IPPROTO_IPV6, MRT6_ADD_MFC, (void *)&CtlReq, sizeof(CtlReq) ) )
    {
        atlog( LOG_ERR, errno, "\nMRT6_ADD_MFC set failed\n\n");        
    }
/*
    for (i = 0; i < sizeof(struct sockaddr_in6); i++)
        printf("%02x ", ((char *)(&CtlReq.mf6cc_origin))[i] & 255);
    printf("\n");
    for (i = 0; i < sizeof(struct sockaddr_in6); i++)
        printf("%02x ", ((char *)(&CtlReq.mf6cc_mcastgrp))[i] & 255);
    printf("\n");    
    printf ("CtlReq.mf6cc_parent = %d\n", CtlReq.mf6cc_parent);
    for (i = 0; i < sizeof(struct if_set); i++)
        printf("%02x ", ((char *)(&CtlReq.mf6cc_ifset))[i] & 255);
    printf("\n\n");  
*/    
    return 0;    
}

/*
** Removes the multicast routed '*Dp' from the kernel routes
**
** returns: - 0 if the function succeeds
**          - the errno value for non-fatal failure condition
*/
int delMRoute( struct MRouteDesc *Dp )
{
    int MifIx;
    struct mf6cctl CtlReq;

    int i;

    CtlReq.mf6cc_origin    = Dp->OriginAdr;
    CtlReq.mf6cc_mcastgrp  = Dp->McAdr;
    CtlReq.mf6cc_parent    = Dp->InVif;

    for ( MifIx = 0; MifIx < MAX_MC_MIFS; MifIx++ ) 
    {
        if ( Dp->TtlVc[ MifIx ] > 0 )
        {
            IF_SET( MifIx, &CtlReq.mf6cc_ifset );
        }
    }

    if ( setsockopt( MRouterFD, IPPROTO_IPV6, MRT6_DEL_MFC, (void *)&CtlReq, sizeof(CtlReq) ) )
    {
        atlog( LOG_ERR, errno, "MRT6_DEL_MFC set failed\n");        
    }
/*   
    for (i = 0; i < sizeof(struct sockaddr_in6); i++)
        printf("%02x ", ((char *)(&CtlReq.mf6cc_origin))[i] & 255);
    printf("\n");
    for (i = 0; i < sizeof(struct sockaddr_in6); i++)
        printf("%02x ", ((char *)(&CtlReq.mf6cc_mcastgrp))[i] & 255);
    printf("\n");    
    printf ("CtlReq.mf6cc_parent = %d\n", CtlReq.mf6cc_parent);
    for (i = 0; i < sizeof(struct if_set); i++)
        printf("%02x ", ((char *)(&CtlReq.mf6cc_ifset))[i] & 255);
    printf("\n\n");  
*/
    return 0;    
}

/*
** Returns for the virtual interface index for '*IfDp'
**
** returns: - the vitrual interface index if the interface is registered
**          - -1 if no virtual interface exists for the interface 
**          
*/
struct IfDesc * getIfDescFromMifByIx( int Ix )
{
    struct IfDesc *Dp;

    if ( MifDescVc[Ix].IfDp )
        return MifDescVc[Ix].IfDp;
    else
        return NULL;
}



