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
*   request.c 
*
*   Functions for recieveing and processing IGMP requests.
*
*/

#include "defs.h"
#include <sys/syslog.h>

static int startupQueryCount = 0;
// Prototypes...
void sendGroupSpecificMemberQuery(void *argument);  
    
typedef struct {
    struct in6_addr     group;
    struct in6_addr     src;    
    struct sockaddr_in6 from;
    short               started;
} GroupVifDesc;


int isReservedGroup(struct in6_addr *group)
{
    if(group && ((group->s6_addr[1] & 0xf) == 0x0))
    {
        /*scope is reserved*/
        return 1;
    }

    return 0;
}

/**
*   Handles incoming membership reports, and
*   appends them to the routing table.
*/
void acceptGroupReport(uint32 port, struct sockaddr_in6 *from, 
                       struct in6_addr *group, struct in6_addr *src) 
{
    //struct IfDesc  *sourceVif;
    int i;
    char *temp;
    struct globalIpList*    globalIpInfo;
/*

    printf("\n\nInside acceptGroupReport port = %d\n", port);
    printf("group = ");
    temp = (char *)group;
    for (i = 0; i < sizeof(struct in6_addr); i++)
        printf("%02x ", temp[i] & 255);
    printf("\n");

    printf("src = ");
    temp = (char *)(&from->sin6_addr);
    for (i = 0; i < sizeof(struct in6_addr); i++)
        printf("%02x ", temp[i] & 255);
    printf("\n");    
    for (i = 0; i < 60; i++)
        printf("%02x ", recv_buf[i] & 255);
    printf("\n");    
    printf("sin6_scope_id = %d\n\n", from->sin6_scope_id);
*/
    if ( upStreamVif == -1 )
        return;

    if(isReservedGroup(group))
    {
        return;
    }

    for (i = 0; i < IfDescVcNumber; i++)
    {
        if ( IN6_ARE_ADDR_EQUAL( &(from->sin6_addr), &(IfDescVc[i].linkAddr) ) )
            return;
        
        globalIpInfo = IfDescVc[i].globalIpInfo;

        while( globalIpInfo )
        {
            if ( IN6_ARE_ADDR_EQUAL( &from->sin6_addr, &globalIpInfo->globalAddr ) )
                return;
            globalIpInfo = globalIpInfo->next;
        }

        if ( IfDescVc[i].index == from->sin6_scope_id && IfDescVc[i].state != IF_STATE_DOWNSTREAM )
            return;
    }    
    
    /* Bug 755: in other mode, if the bridge ip address and the frame from downsteam 
     * is not in the same network, all the join request will be accepted.
     * We don't care the source ip address in the same network or not.  */
    if(commonConfig.mode & 0x01)
    {
        IF_DEBUG atlog(LOG_DEBUG, 0, "Should insert group X port %d to fdb table.", port);
        if (port > 0) 
            insertFdb(group, port);
    }

    // The membership report was OK... Insert it into the route table..
    if(commonConfig.mode&0x02)
    {
    	    /* IF_DEBUG atlog(LOG_DEBUG, 0,
                 "Should insert group %s (from: %s) to route table. Vif Ix : %d",
                 inetFmt(group, s1), inetFmt(src, s2), sourceVif->index); */
        insertRoute(group, src, from);
    }
}

/**
*   Recieves and handles a group leave message.
*/
void acceptLeaveMessage(struct sockaddr_in6 * from, 
                        struct in6_addr *group, struct in6_addr *src) 
{
    struct IfDesc   *sourceVif;

    
    int i;
    char *temp;
/*
    printf("\nacceptLeaveMessage called\n");
    printf("group = ");
    temp = (char *)group;
    for (i = 0; i < sizeof(struct in6_addr); i++)
        printf("%02x ", temp[i] & 255);
    printf("\n");

    printf("src = ");
    for (i = 0; i < sizeof(struct in6_addr); i++)
        printf("%02x ", ((char*)src)[i] & 255);
    printf("\n\n");    
*/

    // Find the interface on which the report was recieved.
    sourceVif = getIfByIfIdx( from->sin6_scope_id );
    if(sourceVif == NULL)
    {
        /*
        atlog(LOG_WARNING, 0, "No interfaces found for source %s",
            inetFmt(src,s1));
            */
        return;
    }

    if( !IN6_ARE_ADDR_EQUAL( src, &allzero_addr) ) 
        if ( findRoute(group, &allzero_addr) != NULL )
            return; //other client need all src of the group
 
    // We have a IF so check that it's an downstream IF.
    if(sourceVif->state == IF_STATE_DOWNSTREAM)
    {
        uint32 ifindex = k_get_ifindex(sourceVif->Name);
        uint32 old_mcast_ifindex = k_set_if(ifindex);
        GroupVifDesc   *gvDesc;
        gvDesc = (GroupVifDesc*) malloc(sizeof(GroupVifDesc));

        // Tell the route table that we are checking for remaining members...
        setRouteLastMemberMode(group, src);

        // Call the group spesific membership querier...
        gvDesc->group = *group;
        gvDesc->src = *src;        
        gvDesc->from = *from;
        gvDesc->started = 0;

        sendGroupSpecificMemberQuery(gvDesc);
        k_set_if(old_mcast_ifindex);
    } 
    else 
    {
        // just ignore the leave request...
        IF_DEBUG atlog(LOG_DEBUG, 0, "The found if for %s was not downstream. Ignoring leave request.");
    }
}

/**
*   Sends a group specific member report query until the 
*   group times out...
*/
void sendGroupSpecificMemberQuery(void *argument) 
{
    struct  Config  *conf = getCommonConfig();

    // Cast argument to correct type...
    GroupVifDesc   *gvDesc = (GroupVifDesc*) argument;

    if(gvDesc->started) 
    {
        lastMemberGroupAge(&gvDesc->group, &gvDesc->src);
        return;
    } 
    else 
    {
        gvDesc->started = 1;
    }

    // Send a group specific membership query...
    sendIcmpv6( &gvDesc->group, conf->lastMemberQueryInterval * 1000, &gvDesc->group);
    /*
    IF_DEBUG atlog(LOG_DEBUG, 0, "Sent membership query from %s to %s. Delay: %d",
        inetFmt(gvDesc->vifAddr,s1), inetFmt(gvDesc->group,s2),
        conf->lastMemberQueryInterval);
    */
    // Set timeout for next round...
    timer_setTimer(conf->lastMemberQueryInterval, sendGroupSpecificMemberQuery, gvDesc);
}


void setStartCount()
{
    struct Config *conf = getCommonConfig();
    startupQueryCount = conf->startupQueryCount;
}


/**
*   Sends a general membership query on downstream VIFs
*/

void sendGeneralMembershipQuery()
{
    struct  Config  *conf = getCommonConfig();
    int             Ix;
    struct IfDesc  *Dp;    

    // Loop through all downstream vifs...
    for (Ix = 0; (Dp = getIfByIx(Ix)); Ix++) 
    {
        // If this is a downstream vif, we should join the All routers group...
        if (Dp->state == IF_STATE_DOWNSTREAM)
        {
            uint32 ifindex = k_get_ifindex(Dp->Name);
            uint32 old_mcast_ifindex = k_set_if(ifindex);
            sendIcmpv6( &allhosts_group, conf->queryResponseInterval * 1000, &allzero_addr);    
            k_set_if(old_mcast_ifindex);
        }
    }
}


/**
*   Sends a general membership query on downstream VIFs
*/
void igmpTimerHandle()
{
     // Loads configuration for Physical interfaces...
    buildIfVc();    

    
    // Configures IF states and settings
    configureVifs();
   struct Config *conf = getCommonConfig();

    sendGeneralMembershipQuery();

    // Install timer for aging active routes.
    if (startupQueryCount == 0 || startupQueryCount == conf->startupQueryCount) 
    {
        if (commonConfig.mode&0x02)
            timer_setTimer(conf->queryResponseInterval, ageActiveRoutes, NULL);

        if (commonConfig.mode&0x01)
            timer_setTimer(conf->queryResponseInterval, ageActiveFdbs, NULL);   
    }
    
    // Install timer for next general query...
    if (startupQueryCount > 0) 
    {
        // Use quick timer...
        timer_setTimer(conf->startupQueryInterval, igmpTimerHandle, NULL);
        // Decrease startup counter...
        startupQueryCount--;
    }
    else 
    {   
        // Use slow timer...
        timer_setTimer(conf->queryInterval, igmpTimerHandle, NULL);
    }
}
 
