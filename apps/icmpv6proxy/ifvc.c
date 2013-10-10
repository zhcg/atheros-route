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

#include "defs.h"
#include <linux/sockios.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>


struct IfDesc IfDescVc[ MAX_IF ];  //, *IfDescEp = IfDescVc;
unsigned int  IfDescVcNumber = 0;


struct IfDesc *findIfDescVcByName(char *name)
{
    int i;
    for (i = 0; i < IfDescVcNumber; i++)
        if ( strcmp(IfDescVc[i].Name, name) == 0 )
            return &IfDescVc[i];
    return NULL;
}

int addIfDescVcGlobalIp(struct IfDesc * ifPt ,struct in6_addr *ipAddr, int mask)
{
    struct globalIpList* p = ifPt->globalIpInfo;
    
    if (p == NULL)
    {
        ifPt->globalIpInfo = malloc(sizeof(struct globalIpList));
        ifPt->globalIpInfo->globalAddr = *ipAddr;
        ifPt->globalIpInfo->globalMask = mask;
        ifPt->globalIpInfo->next = NULL;
        return 0;
    }
    
    while (p->next)
        p = p->next;


    p->next = malloc(sizeof(struct globalIpList));
    if (p->next == NULL)
        return -1;
    
    p = p->next;
    p->globalAddr = *ipAddr;
    p->globalMask = mask;
    p->next = NULL;
    return 0;
}
/*
** Builds up a vector with the interface of the machine. Calls to the other functions of 
** the module will fail if they are called before the vector is build.
**          
*/
void buildIfVc() 
{
    FILE *fp = fopen("/proc/net/if_inet6", "r");
    char    ipSeg[8][5];
    int     index, mask, scope, flags;
    char    name[IFNAMSIZ];
    struct IfDesc * ifPt;
    char    ipString[64];
    struct in6_addr ipAddr;

    if (fp) 
    { 
        while (fscanf(fp, "%4s%4s%4s%4s%4s%4s%4s%4s %02x %02x %02x %02x %s\n",
                      ipSeg[0], ipSeg[1], ipSeg[2], ipSeg[3], ipSeg[4], ipSeg[5], 
                      ipSeg[6], ipSeg[7], &index, &mask, &scope, &flags, name) != EOF) 
        { 
            ifPt = findIfDescVcByName(name);
            if (ifPt == NULL)
            {
                ifPt = &IfDescVc[IfDescVcNumber];
                IfDescVcNumber++;                        
            }

            sprintf( ipString, "%s:%s:%s:%s:%s:%s:%s:%s", 
                     ipSeg[0], ipSeg[1], ipSeg[2], ipSeg[3], ipSeg[4], ipSeg[5], ipSeg[6], ipSeg[7]);
            inet_pton(AF_INET6, ipString, &ipAddr); 

            if (scope == 0) //Global ip address
            {
                if ( addIfDescVcGlobalIp(ifPt, &ipAddr, mask) == -1)
                    return;
            }
            else if (scope == 0x20) //Link Local ip address 
            {
                ifPt->linkAddr = ipAddr;
                ifPt->linkMask = mask;
            }
            else    // loop back or other
            {
                IfDescVcNumber--;
                continue;
            }

            strcpy(ifPt->Name, name);
            ifPt->Flags         = flags;
            ifPt->state         = IF_STATE_DISABLED;
            ifPt->robustness    = DEFAULT_ROBUSTNESS;
            ifPt->threshold     = DEFAULT_THRESHOLD;   /* ttl limit */
            ifPt->ratelimit     = DEFAULT_RATELIMIT;             
            ifPt->index         = index;  
        } 
    } 
}

void showIfVc()
{
    int i;
    char tempAddr[64];
    struct globalIpList* tmp;

    for (i = 0; i < IfDescVcNumber; i++)
    {
        atlog( LOG_DEBUG, 0, "\nName     = %s\n",IfDescVc[i].Name);
        atlog( LOG_DEBUG, 0, "index    = %d\n",IfDescVc[i].index);          
        inet_ntop(AF_INET6, &(IfDescVc[i].linkAddr), tempAddr, 64);
        atlog( LOG_DEBUG, 0, "linkAddr = %s\n",tempAddr);
        atlog( LOG_DEBUG, 0, "linkMask = %d\n",IfDescVc[i].linkMask);
        atlog( LOG_DEBUG, 0, "Flags    = %d\n",IfDescVc[i].Flags);
        atlog( LOG_DEBUG, 0, "state    = %d\n",IfDescVc[i].state);
        atlog( LOG_DEBUG, 0, "robustness = %d\n",IfDescVc[i].robustness);
        atlog( LOG_DEBUG, 0, "threshold  = %d\n",IfDescVc[i].threshold);
        atlog( LOG_DEBUG, 0, "ratelimit  = %d\n",IfDescVc[i].ratelimit);

        tmp = IfDescVc[i].globalIpInfo;
        while(tmp)
        {
            inet_ntop(AF_INET6, &(tmp->globalAddr), tempAddr, 64);
            atlog( LOG_DEBUG, 0, "globalAddr = %s\n",tempAddr);
            atlog( LOG_DEBUG, 0, "globalMask = %d\n",tmp->globalMask);
            tmp = tmp->next;
        }
    }
}

int
initSnoopBr()
{
    unsigned Ix;
    struct IfDesc *Dp;
    
    // Join the all routers group on downstream vifs...
    for (Ix = 0; (Dp = getIfByIx(Ix)); Ix++) {
        // If this is a downstream vif, we should join the All routers group...
        if (Dp->state == IF_STATE_DOWNSTREAM)
            addMIF(Dp); 
    }

    return 0;
}

int
clearSnoopBr()
{
    unsigned Ix;
    struct IfDesc *Dp;
    
    // Join the all routers group on downstream vifs...
    for (Ix = 0; (Dp = getIfByIx(Ix)); Ix++) 
    {
        // If this is a downstream vif, we should join the All routers group...
        if (Dp->state == IF_STATE_DOWNSTREAM)
        {
            //delVIF(Dp); 
            Dp->state = IF_STATE_DISABLED;
        }   
    }

    return 0;
}

/*
** Returns a pointer to the IfDesc of the interface 'IfName'
**
** returns: - pointer to the IfDesc of the requested interface
**          - NULL if no interface 'IfName' exists
**          
*/
struct IfDesc *getIfByName( const char *IfName ) 
{
    int i;
    for ( i = 0; i < IfDescVcNumber; i++ )
        if ( ! strcmp( IfName, IfDescVc[i].Name ) )
            return &(IfDescVc[i]);

    return NULL;
}

/*
** Returns a pointer to the IfDesc of the interface 'Ix'
**
** returns: - pointer to the IfDesc of the requested interface
**          - NULL if no interface 'Ix' exists
**          
*/
struct IfDesc *getIfByIx( unsigned Ix ) 
{
    return Ix < IfDescVcNumber ? &IfDescVc[Ix] : NULL;
}

/**
*   Returns a pointer to the IfDesc whose subnet matches
*   the supplied IP adress. The IP must match a interfaces
*   subnet, or any configured allowed subnet on a interface.
*/
struct IfDesc *getIfByAddress( struct in6_addr *ipaddr ) 
{
    struct globalIpList*    globalIpInfo;
    int i;

    for (i = 0; i < IfDescVcNumber; i++)
    {
        globalIpInfo = IfDescVc[i].globalIpInfo;

        while (globalIpInfo)
        {
            if ( IN6_ARE_ADDR_EQUAL( &(IfDescVc[i].globalIpInfo->globalAddr), ipaddr) )
                return &(IfDescVc[i]);
            globalIpInfo = globalIpInfo->next;
        }
    }
    return NULL;
}

struct IfDesc *getIfByIfIdx( int index ) 
{
    int i;

    for (i = 0; i < IfDescVcNumber; i++)
        if (IfDescVc[i].index == index)
            return &(IfDescVc[i]);

    return NULL;
}


/**
*   Returns a pointer to the IfDesc whose subnet matches
*   the supplied IP adress. The IP must match a interfaces
*   subnet, or any configured allowed subnet on a interface.
*/
struct IfDesc *getIfByVifIndex( unsigned vifindex ) 
{
    int i;
    for ( i = 0; i < IfDescVcNumber; i++ )
        if(IfDescVc[i].index == vifindex) 
            return &(IfDescVc[i]);
            
    return NULL;
}


/**
*   Function that checks if a given ipaddress is a valid
*   address for the supplied VIF.
*/
int isAdressValidForIf( struct IfDesc* intrface, uint32 ipaddr ) 
{
#if 0    
    struct SubnetList   *currsubnet;
    
    if(intrface == NULL) {
        return 0;
    }
    // Loop through all registered allowed nets of the VIF...
    for(currsubnet = intrface->allowednets; currsubnet != NULL; currsubnet = currsubnet->next) {

        IF_DEBUG atlog(LOG_DEBUG, 0, "Testing %s for subnet %s, mask %s: Result net: %s",
            inetFmt(ipaddr, s1),
            inetFmt(currsubnet->subnet_addr, s2),
            inetFmt(currsubnet->subnet_mask, s3),
            inetFmt((ipaddr & currsubnet->subnet_mask), s4)
            );

        // Check if the ip falls in under the subnet....
        if((ipaddr & currsubnet->subnet_mask) == currsubnet->subnet_addr) {
            return 1;
        }
    }
#endif    
    return 0;
}


