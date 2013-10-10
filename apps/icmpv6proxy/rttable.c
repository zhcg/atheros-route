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
*   rttable.c 
*
*   Updates the routingtable according to 
*     recieved request.
*/
#include "defs.h"
#include <sys/syslog.h>
    
/**
*   Routing table structure definition. Double linked list...
*/
struct RouteTable {
    struct RouteTable   *nextroute;     // Pointer to the next group in line.
    struct RouteTable   *prevroute;     // Pointer to the previous group in line.
    struct in6_addr     group;          // The group to route
    struct in6_addr     originAddr;     // The origin adress (only set on activated routes)
    uint32              vifBits;        // Bits representing recieving VIFs.

    // Keeps the upstream membership state...
    short               upstrState;     // Upstream membership state.

    // These parameters contain aging details.
    uint32              ageVifBits;     // Bits representing aging VIFs.
    int                 ageValue;       // Downcounter for death.          
    int                 ageActivity;    // Records any acitivity that notes there are still listeners.
};

                 
// Keeper for the routing table...
static struct RouteTable   *routing_table;

// Prototypes
void atlogRouteTable(char *header);
int  internAgeRoute(struct RouteTable*  croute);

// Socket for sending join or leave requests.
int mcGroupSock = 0;


int internUpdateKernelRoute(struct RouteTable *route, int activate);
/**
*   Function for retrieving the Multicast Group socket.
*/
int getMcGroupSock() {
    if( ! mcGroupSock ) {
        mcGroupSock = openUdpSocket( INADDR_ANY, 0 );;
    }
    return mcGroupSock;
}
 
/**
*   Initializes the routing table.
*/
void initRouteTable() 
{
    unsigned Ix;
    struct IfDesc *Dp;

    // Clear routing table...
    routing_table = NULL;

    // Join the all routers group on downstream vifs...
    for ( Ix = 0; (Dp = getIfByIx( Ix )); Ix++ ) 
    {
        // If this is a downstream vif, we should join the All routers group...
        if( Dp->state == IF_STATE_DOWNSTREAM ) 
        {
            /*
            IF_DEBUG atlog(LOG_DEBUG, 0, "Joining all-routers group %s on vif %s",
                         inetFmt(allrouters_group,s1),inetFmt(Dp->InAdr.s_addr,s2));
            */
            //joinMcGroup( getMcGroupSock(), Dp, &allrouters_group );
        }
    }
}

/**
*   Internal function to send join or leave requests for
*   a specified route upstream...
*/

void sendJoinLeaveUpstream(struct RouteTable* route, int join) 
{  
    struct IfDesc*      upstrIf;
    // Get the upstream VIF...
    upstrIf = getIfByIx( upStreamVif );
    
    if(upstrIf == NULL) 
    {
        atlog(LOG_ERR, 0 ,"FATAL: Unable to get Upstream IF.");
    }

    // Send join or leave request...
    if(join) 
    {
        // Only join a group if there are listeners downstream...
        if(route->vifBits > 0) 
        {
            joinMcGroup( getMcGroupSock(), upstrIf, &route->group );
            route->upstrState = ROUTESTATE_JOINED;
        } 
    } 
    else 
    {
        // Only leave if group is not left already...
        if(route->upstrState != ROUTESTATE_NOTJOINED) 
        {
            leaveMcGroup( getMcGroupSock(), upstrIf, &route->group );
            route->upstrState = ROUTESTATE_NOTJOINED;
        }
    }
}

/**
*   Clear all routes from routing table, and alerts Leaves upstream.
*/
void clearAllRoutes() 
{
    struct RouteTable   *croute, *remainroute;

    // Loop through all routes...
    for(croute = routing_table; croute; croute = remainroute) 
    {
        remainroute = croute->nextroute;

        // Log the cleanup in debugmode...
        /*
        IF_DEBUG atlog(LOG_DEBUG, 0, "Removing route entry for %s",
                     inetFmt(croute->group, s1));
        */

        // Uninstall current route
        if(!internUpdateKernelRoute(croute, 0)) 
        {
            atlog(LOG_WARNING, 0, "The removal from Kernel failed.");
        }

        // Send Leave message upstream.
        sendJoinLeaveUpstream(croute, 0);

        // Clear memory, and set pointer to next route...
        free(croute);
    }
    routing_table = NULL;

    // Send a notice that the routing table is empty...
    atlog(LOG_NOTICE, 0, "All routes removed. Routing table is empty.");
}

           
/**
*   Private access function to find a route from a given 
*   Route Descriptor.
*/
struct RouteTable *findRoute(struct in6_addr *group, struct in6_addr *src) 
{
    struct RouteTable*  croute;
    int i;

    for(croute = routing_table; croute; croute = croute->nextroute) 
    {
        if( IN6_ARE_ADDR_EQUAL( &croute->group, group) ) 
        {
            if( IN6_ARE_ADDR_EQUAL( &croute->originAddr, src) ) 
            {
                return croute;
            }
        }
    }
    return NULL;
}

/**
*   Private access function to find the next route of a certain group 
*/
struct RouteTable *findNextRoute(struct in6_addr *group, struct RouteTable *start) 
{
    struct RouteTable*  croute;
    int i;
    
    if (start == routing_table)
        return NULL;

    if (start == NULL)
        start = routing_table;

    for(croute = start; croute; croute = croute->nextroute) 
    {
        if( IN6_ARE_ADDR_EQUAL( &croute->group, group) ) 
            return croute;
    }
    return NULL;
}


/**
*   Private access function to find the next route of a certain group 
*/
struct RouteTable *showRoute() 
{
    struct RouteTable*  croute;
    int i;
    printf("****************************************************\n");
    for(croute = routing_table; croute; croute = croute->nextroute) 
    {
        printf("croute = %p, state = %d\n", croute, croute->upstrState);
        for (i = 0; i < 16; i ++)
            printf("%02x ", ((char*)(&(croute->group)))[i] & 255 );
        printf("\n");
        for (i = 0; i < 16; i ++)
            printf("%02x ", ((char*)(&(croute->originAddr)))[i] & 255 );
        printf("\n");        
    }
    printf("****************************************************\n");
    return;
}

/**
*   Adds a specified route to the routingtable.
*   If the route already exists, the existing route 
*   is updated...
*/
int insertRoute(struct in6_addr *group, struct in6_addr *src, struct sockaddr_in6 *from) 
{
    struct Config *conf = getCommonConfig();
    struct RouteTable *croute, *tmpRoute;
    //int result = 1;
/*
    int i;
    printf("insertRoute called\n");

    for (i = 0; i < 16; i ++)
        printf("%02x ", ((char*)group)[i] & 255 );
    printf("\n");

    for (i = 0; i < 16; i ++)
        printf("%02x ", ((char*)src)[i] & 255 );
    printf("\n");
*/
    

    // Sanitycheck the group adress...
    if ( !IN6_IS_ADDR_MULTICAST(group) ) 
    {
        atlog( LOG_DEBUG, 0, "Not multicast group");    
        return 0;
    }

    // Santiycheck the VIF index...
    if(from->sin6_scope_id >= MAX_MC_VIFS) 
    {
        atlog(LOG_WARNING, 0, "The VIF Ix %d is out of range (0-%d). Table insert failed.",
              from->sin6_scope_id,MAX_MC_VIFS);
        return 0;
    }

    // Try to find an existing route for this group...
    croute = findRoute(group, src);
    
    if(croute==NULL)
    {
        struct RouteTable*  newroute;

        /*
        IF_DEBUG atlog(LOG_DEBUG, 0, "No existing route for %s. Create new.",
                     inetFmt(group, s1));
        */

        // Create and initialize the new route table entry..
        newroute = (struct RouteTable*)malloc(sizeof(struct RouteTable));
        // Insert the route desc and clear all pointers...
        newroute->group = *group;
        newroute->originAddr = *src;
        
        newroute->nextroute  = NULL;
        newroute->prevroute  = NULL;

        // The group is not joined initially.
        newroute->upstrState = ROUTESTATE_NOTJOINED;

        // The route is not active yet, so the age is unimportant.
        newroute->ageValue    = conf->robustnessValue;
        newroute->ageActivity = 0;
        
        BIT_ZERO(newroute->ageVifBits);     // Initially we assume no listeners.

        // Set the listener flag...
        BIT_ZERO(newroute->vifBits);    // Initially no listeners...
        if(from->sin6_scope_id >= 0) 
        {
            BIT_SET(newroute->vifBits, from->sin6_scope_id);
        }

        // Check if there is a table already....
        if(routing_table == NULL) 
        {
            // No location set, so insert in on the table top.
            routing_table = newroute;
            IF_DEBUG atlog(LOG_DEBUG, 0, "No routes in table. Insert at beginning.");
        }
        else 
        {
            IF_DEBUG atlog(LOG_DEBUG, 0, "Found existing routes. Find insert location.");

            // Check if the route could be inserted at the beginning...
            if( memcmp(&routing_table->group, group, sizeof(struct in6_addr)) > 0 ) 
            {
                //IF_DEBUG atlog(LOG_DEBUG, 0, "Inserting at beginning, before route %s",inetFmt(routing_table->group,s1));

                // Insert at beginning...
                newroute->nextroute = routing_table;
                newroute->prevroute = NULL;
                routing_table = newroute;

                // If the route has a next node, the previous pointer must be updated.
                if(newroute->nextroute != NULL) 
                {
                    newroute->nextroute->prevroute = newroute;
                }

            } 
            else 
            {
                // Find the location which is closest to the route.
                for( croute = routing_table; croute->nextroute != NULL; croute = croute->nextroute ) 
                {
                    // Find insert position.
                    if( memcmp( &(croute->nextroute->group), group, sizeof(struct in6_addr) ) > 0 ) 
                        break;
                }

                //IF_DEBUG atlog(LOG_DEBUG, 0, "Inserting after route %s",inetFmt(croute->group,s1));
                
                // Insert after current...
                newroute->nextroute = croute->nextroute;
                newroute->prevroute = croute;
                if(croute->nextroute != NULL) 
                {
                    croute->nextroute->prevroute = newroute; 
                }
                croute->nextroute = newroute;
            }
        }

        // Set the new route as the current...
        croute = newroute;

     
        if( !IN6_ARE_ADDR_EQUAL( &croute->originAddr, &allzero_addr) ) 
        {
            // Update route in kernel...
            if( !internUpdateKernelRoute(croute, 1) ) 
            {
                atlog(LOG_WARNING, 0, "The insertion into Kernel failed.");
                return 0;
            }
        }    
    } 
    else if(from->sin6_scope_id >= 0) 
    {
        if( !IN6_ARE_ADDR_EQUAL( &croute->originAddr, &allzero_addr) ) 
        {
            // The route exists already, so just update it.
            BIT_SET(croute->vifBits, from->sin6_scope_id);
            
            // Register the VIF activity for the aging routine
            BIT_SET(croute->ageVifBits, from->sin6_scope_id);
        }
        else
        {
            tmpRoute = findNextRoute(group, NULL);
            while (tmpRoute != NULL)
            {
                // The route exists already, so just update it.
                BIT_SET(tmpRoute->vifBits, from->sin6_scope_id);
                
                // Register the VIF activity for the aging routine
                BIT_SET(tmpRoute->ageVifBits, from->sin6_scope_id);
                if (tmpRoute->nextroute)
                    tmpRoute = findNextRoute(group, tmpRoute->nextroute);  
                else
                    break;
            }
        }
    }

    // Send join message upstream, if the route has no joined flag...
    if(croute->upstrState != ROUTESTATE_JOINED)
    {
        // Send Join request upstream
        sendJoinLeaveUpstream(croute, 1);
    }

    IF_DEBUG atlogRouteTable("Insert Route");
    return 1;
}

/**
*   Activates a passive group. If the group is already
*   activated, it's reinstalled in the kernel. If
*   the route is activated, no originAddr is needed.
*/
int activateRoute(struct in6_addr * group, struct in6_addr * originAddr)
{
    int result = 0;
    struct RouteTable*  croute;
    struct RouteTable*  newroute;

    // Find the requested route.
    croute = findRoute(group, originAddr);
    if(croute != NULL) 
        return 0;
    croute = findRoute(group, &allzero_addr);   
    if (croute)
    {        
        newroute = (struct RouteTable*)malloc(sizeof(struct RouteTable));
        *newroute = *croute;
        newroute->originAddr = *originAddr;

        newroute->nextroute = routing_table;
        newroute->prevroute = NULL;
        routing_table->prevroute = newroute;
        routing_table = newroute;

        if( !internUpdateKernelRoute(newroute, 1) ) 
        {
            atlog(LOG_WARNING, 0, "The insertion into Kernel failed.");
            return 0;
        }
    }
    
    return 1;
}


/**
*   This function loops through all routes, and updates the age 
*   of any active routes.
*/
void ageActiveRoutes() 
{
    struct RouteTable   *croute, *nroute;
    
    IF_DEBUG atlog(LOG_DEBUG, 0, "Aging routes in table.");

    // Scan all routes...
    for( croute = routing_table; croute != NULL; croute = nroute )
    {        
        // Keep the next route (since current route may be removed)...
        nroute = croute->nextroute;

        // Run the aging round algorithm.
        if(croute->upstrState != ROUTESTATE_CHECK_LAST_MEMBER)
        {
            // Only age routes if Last member probe is not active...
            internAgeRoute(croute);
        }
    }
    IF_DEBUG atlogRouteTable("Age active routes");
}

/**
*   Should be called when a leave message is recieved, to
*   mark a route for the last member probe state.
*/
void setRouteLastMemberMode(struct in6_addr * group, struct in6_addr * src) 
{
    struct Config       *conf = getCommonConfig();
    struct RouteTable   *croute, *tmpRoute;

    croute = findRoute(group, src);
    if(croute!=NULL) 
    {
        if( !IN6_ARE_ADDR_EQUAL( src, &allzero_addr) ) 
        {
            // Check for fast leave mode...
            /*
            if(croute->upstrState == ROUTESTATE_JOINED && conf->fastUpstreamLeave) 
            {
                // Send a leave message right away..
                sendJoinLeaveUpstream(croute, 0);
            }
            */
            // Set the routingstate to Last member check...
            croute->upstrState = ROUTESTATE_CHECK_LAST_MEMBER;
            // Set the count value for expiring... (-1 since first aging)
            croute->ageValue = conf->lastMemberQueryCount;
        }
        else
        {
            tmpRoute = findNextRoute(group, NULL);
            while(tmpRoute)
            {
                // Check for fast leave mode...
                /*
                if(tmpRoute->upstrState == ROUTESTATE_JOINED && conf->fastUpstreamLeave) 
                {
                    // Send a leave message right away..
                    sendJoinLeaveUpstream(tmpRoute, 0);
                }
                */
                // Set the routingstate to Last member check...
                tmpRoute->upstrState = ROUTESTATE_CHECK_LAST_MEMBER;
                // Set the count value for expiring... (-1 since first aging)
                tmpRoute->ageValue = conf->lastMemberQueryCount;  
                if (tmpRoute->nextroute)
                    tmpRoute = findNextRoute(group, tmpRoute->nextroute);            
                else
                    break;
            }
        }
    }
}


/**
*   Ages groups in the last member check state. If the
*   route is not found, or not in this state, 0 is returned.
*/
int lastMemberGroupAge(struct in6_addr * group, struct in6_addr * src) 
{
    struct Config       *conf = NULL;
    struct RouteTable   *croute, *nroute;
    int i;
    
    conf = getCommonConfig();
/*
    printf("\nlastMemberGroupAge called, src = \n");
    for (i=0; i <16; i++)
        printf("%02x ",((char*)group)[i] & 255);
    printf("\n");  
    for (i=0; i <16; i++)
        printf("%02x ",((char*)src)[i] & 255);
    printf("\n");      
*/
    if( !IN6_ARE_ADDR_EQUAL( src, &allzero_addr ) ) 
    {
        croute = findRoute(group, src);
        if(croute!=NULL)
        {
            if(croute->upstrState == ROUTESTATE_CHECK_LAST_MEMBER) 
            {
                internAgeRoute(croute);
            }
        }
    }
    else
    {
        //showRoute();

        for( croute = routing_table; croute != NULL; croute = nroute )
        {        
            // Keep the next route (since current route may be removed)...
            nroute = croute->nextroute;

            // Run the aging round algorithm.
            if( croute->upstrState = ROUTESTATE_CHECK_LAST_MEMBER && 
                IN6_ARE_ADDR_EQUAL( &croute->group, group ) 
              )
            {
                // Only age routes if Last member probe is not active...
                internAgeRoute(croute);
            }
        }
    }    
    return 0;
}

/**
*   Remove a specified route. Returns 1 on success,
*   and 0 if route was not found.
*/
int removeRoute(struct RouteTable*  croute) 
{
    int result = 1;    
    struct RouteTable*  tmpRoute;
    struct Config       *conf = getCommonConfig();
    
    // If croute is null, no routes was found.
    if(croute==NULL) 
        return 0;

    //printf("removeRoute called\n");
    
   
    //BIT_ZERO(croute->vifBits);

    // Uninstall current route from kernel
    if(!internUpdateKernelRoute(croute, 0)) 
    {
        //printf("The removal from Kernel failed.\n");
        atlog(LOG_WARNING, 0, "The removal from Kernel failed.");
        result = 0;
    }

    // Send Leave request upstream if group is joined
    if(croute->upstrState == ROUTESTATE_JOINED || 
       (croute->upstrState == ROUTESTATE_CHECK_LAST_MEMBER && !conf->fastUpstreamLeave)) 
    {
        tmpRoute = findNextRoute( &(croute->group), NULL);
        if (tmpRoute && tmpRoute->nextroute)
            tmpRoute = findNextRoute( &(croute->group), tmpRoute->nextroute);

        // the group is not used by others
        if (tmpRoute == NULL)
            sendJoinLeaveUpstream(croute, 0);
    }

    // Update pointers...
    if(croute->prevroute == NULL)
    {
        // Topmost node...
        if(croute->nextroute != NULL) 
        {
            croute->nextroute->prevroute = NULL;
        }
        routing_table = croute->nextroute;

    } 
    else
    {
        croute->prevroute->nextroute = croute->nextroute;
        if(croute->nextroute != NULL) 
        {
            croute->nextroute->prevroute = croute->prevroute;
        }
    }
    // Free the memory, and set the route to NULL...
    free(croute);
    croute = NULL;

    IF_DEBUG atlogRouteTable("Remove route");
    return result;
}


/**
*   Ages a specific route
*/
int internAgeRoute(struct RouteTable*  croute) 
{
    int result = 0;
    struct Config *conf = getCommonConfig();
/*
    int i;

    printf("\n\n\n\n\n\n Inside internAgeRoute\n");

    for (i=0; i <16; i++)
        printf("%02x ",((char*)(&croute->group))[i] & 255);
    printf("\n");
    for (i=0; i <16; i++)
        printf("%02x ",((char*)(&croute->originAddr))[i] & 255);
    printf("\n");    

    printf("croute->ageValue = %d \n",croute->ageValue);
    printf("croute->ageVifBits = %d \n",croute->ageVifBits);
    printf("croute->ageActivity = %d \n",croute->ageActivity);
    printf("croute->vifBits = %d \n\n\n\n\n",croute->vifBits);
*/   

    // Drop age by 1.
    croute->ageValue--;

    // Check if there has been any activity...
    if( croute->ageVifBits > 0 && croute->ageActivity == 0 ) 
    {
        // There was some activity, check if all registered vifs responded.
        if(croute->vifBits == croute->ageVifBits) 
        {
            // Everything is in perfect order, so we just update the route age.
            croute->ageValue = conf->robustnessValue;
            //croute->ageActivity = 0;
        }
        else 
        {
            // One or more VIF has not gotten any response.
            croute->ageActivity++;

            // Update the actual bits for the route...
            croute->vifBits = croute->ageVifBits;
        }
    } 
    // Check if there have been activity in aging process...
    else if( croute->ageActivity > 0 )
    {
        // If the bits are different in this round, we must
        if(croute->vifBits != croute->ageVifBits) 
        {
            // Or the bits together to insure we don't lose any listeners.
            croute->vifBits |= croute->ageVifBits;

            // Register changes in this round as well..
            croute->ageActivity++;
        }
    }

    // If the aging counter has reached zero, its time for updating...
    if(croute->ageValue == 0)
    {
        // Check for activity in the aging process,
        if(croute->ageActivity>0)
        {
            // Just update the routing settings in kernel...
            internUpdateKernelRoute(croute, 1);
    
            // We append the activity counter to the age, and continue...
            croute->ageValue = croute->ageActivity;
            croute->ageActivity = 0;
        } 
        else 
        {
            /*
            IF_DEBUG atlog(LOG_DEBUG, 0, "Removing group %s. Died of old age.",
                         inetFmt(croute->group,s1));
            */
            // No activity was registered within the timelimit, so remove the route.
            removeRoute(croute);
            result = 1;        
        }
        // Tell that the route was updated...
           }

    // The aging vif bits must be reset for each round...
    BIT_ZERO(croute->ageVifBits);
    return result;
}

/**
*   Updates the Kernel routing table. If activate is 1, the route
*   is (re-)activated. If activate is false, the route is removed.
*/
int internUpdateKernelRoute(struct RouteTable *route, int activate)
{
    struct   MRouteDesc     mrDesc;
    struct   IfDesc         *Dp;
    unsigned                Ix;
    struct IfDesc*      upstrIf;
    // Get the upstream VIF...
    upstrIf = getIfByIx( upStreamVif );
    
    if(upstrIf == NULL) 
        atlog(LOG_ERR, 0 ,"FATAL: Unable to get Upstream IF.");

    if( IN6_ARE_ADDR_EQUAL( &(route->group), &allzero_addr ) ) 
        return 1;

    if( IN6_ARE_ADDR_EQUAL( &(route->originAddr), &allzero_addr ) ) 
        return 1;
     
    // Build route descriptor from table entry...
    // Set the source address and group address...
    mrDesc.McAdr.sin6_addr = route->group;
    mrDesc.McAdr.sin6_family = AF_INET6;
    mrDesc.McAdr.sin6_flowinfo = 0;
    mrDesc.McAdr.sin6_port = 0;
    mrDesc.McAdr.sin6_scope_id = upstrIf->index;
    mrDesc.OriginAdr.sin6_addr = route->originAddr;

    // clear output interfaces 
    memset( mrDesc.TtlVc, 0, sizeof( mrDesc.TtlVc ) );

    IF_DEBUG atlog(LOG_DEBUG, 0, "Vif bits : 0x%08x", route->vifBits);

    for( Ix = 0 ; Ix < MAXMIFS; Ix++ ) 
    {
        Dp = getIfDescFromMifByIx(Ix);
        if (Dp == NULL)
            break;
        if ( Dp->state == IF_STATE_UPSTREAM ) 
            mrDesc.InVif = Ix;
        else if(BIT_TST(route->vifBits, Dp->index)) 
        {
            mrDesc.TtlVc[ Ix ] = Dp->threshold;
        }
    }

    // Do the actual Kernel route update...
    if(activate)
    {
        // Add route in kernel...
        addMRoute( &mrDesc );

    }
    else 
    {
        // Delete the route from Kernel...
        delMRoute( &mrDesc );
    }

    return 1;
}

/**
*   Debug function that writes the routing table entries
*   to the atlog.
*/
void atlogRouteTable(char *header) 
{
#if 0
    IF_DEBUG  {
        struct RouteTable*  croute = routing_table;
        unsigned            rcount = 0;
    
        atlog(LOG_DEBUG, 0, "\nCurrent routing table (%s);\n-----------------------------------------------------\n", header);
        if(croute==NULL) {
            atlog(LOG_DEBUG, 0, "No routes in table...");
        } else {
            do {
                /*
                atlog(LOG_DEBUG, 0, "#%d: Src: %s, Dst: %s, Age:%d, St: %s, Prev: 0x%08x, T: 0x%08x, Next: 0x%08x",
                    rcount, inetFmt(croute->originAddr, s1), inetFmt(croute->group, s2),
                    croute->ageValue,(croute->originAddr>0?"A":"I"),
                    croute->prevroute, croute, croute->nextroute);
                atlog(LOG_DEBUG, 0, "#%d: Src: %s, Dst: %s, Age:%d, St: %s, OutVifs: 0x%08x",
                    rcount, inetFmt(croute->originAddr, s1), inetFmt(croute->group, s2),
                    croute->ageValue,(croute->originAddr>0?"A":"I"),
                    croute->vifBits);
                */

                croute = croute->nextroute; 
        
                rcount++;
            } while ( croute != NULL );
        }
    
        atlog(LOG_DEBUG, 0, "\n-----------------------------------------------------\n");
    }
#endif
}

