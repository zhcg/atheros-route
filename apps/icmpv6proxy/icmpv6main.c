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

#include "version.h"
#if 0   /* original */
#include "build.h"
#else   /* simplify! */
#define BUILD "XXXXXX"
#endif

#include <sys/syslog.h>
#include <unistd.h>
#include <fcntl.h>




// Constants
static const char Version[] = 
"igmpproxy, Version " VERSION ", Build" BUILD "\n"
"Copyright 2005 by Johnny Egeland <johnny@rlo.org>\n"
"Modified by Tos Xu for IGMP snooping in April, 2009.\n"
"Distributed under the GNU GENERAL PUBLIC LICENSE, Version 2 - check GPL.txt\n"
"\n";

static const char Usage[] = 
"usage: igmpproxy [-h] [-d] [-c <configfile>]\n"
"\n" 
"   -h   Display this help screen\n"
"   -c   Specify a location for the config file (default is '/etc/igmpproxy.conf').\n"
"   -d   Run in debug mode. Does not fork deamon, and output all atlogmessages on stderr.\n"
"\n"
;

// Local function Prototypes
static void signalHandler(int);
int     icmpv6ProxyInit();
void    igmpProxyCleanUp();
void    igmpMainRun();

// Global vars...
static int sighandled = 0;
#define GOT_SIGINT  0x01
#define GOT_SIGHUP  0x02
#define GOT_SIGUSR1 0x04
#define GOT_SIGUSR2 0x08

// The upstream VIF index
int         upStreamVif;   

/**
*   Program main method. Is invoked when the program is started
*   on commandline. The number of commandline arguments, and a
*   pointer to the arguments are recieved on the line...
*/    
int main( int ArgCn, const char *ArgVc[] ) 
{
    int debugMode = 0;

    // Set the default config Filepath...
    char* configFilePath = IGMPPROXY_CONFIG_FILEPATH;

    // Display version 
    fputs( Version, stderr );

    // Parse the commandline options and setup basic settings..
    int i = 1;
    while (i < ArgCn) 
    {
        if ( strlen(ArgVc[i]) > 1 && ArgVc[i][0] == '-') 
        {
            switch ( ArgVc[i][1] ) 
            {
                case 'h':
                    fputs( Usage, stderr );
                    exit( 0 );

                case 'd':
                    Log2Stderr = LOG_DEBUG;
                    /*
                case 'v':
                    // Enable debug mode...
                    if (Log2Stderr < LOG_INFO) 
                    {
                        Log2Stderr = LOG_INFO;
                    }
                    */
                    debugMode = 1;
                    break;

                case 'c':
                    // Get new filepath...
                    if (i + 1 < ArgCn && ArgVc[i+1][0] != '-') 
                    {
                        configFilePath = (char *)ArgVc[i+1];
                        i++;
                    } 
                    else 
                    {
                        atlog(LOG_ERR, 0, "Missing config file path after -c option.");
                    }
                    break;
            }
        }
        i++;
    }

    // Chech that we are root
    if (geteuid() != 0) 
    {
        fprintf(stderr, "igmpproxy: must be root\n");
        exit(1);
    }

    // Write debug notice with file path...
    IF_DEBUG atlog(LOG_DEBUG, 0, "Searching for config file at '%s'" , configFilePath);

    // Loads the config file...
    if( ! loadConfig( configFilePath ) ) 
    {
        atlog(LOG_ERR, 0, "Unable to load config file...");
        goto exit_all;
    }       

    if ( !icmpv6ProxyInit() ) 
    {
        atlog(LOG_ERR, 0, "Unable to initialize IGMP Proxy and Snooping.");
        goto exit_all;;
    }
        // Initializes the proxy.
    if(commonConfig.mode&0x02)
    {

    }
        //Initializes the snooping

    if(commonConfig.mode&0x01)
        if (!igmpSnoopInit()) 
        {
            atlog(LOG_ERR, 0, "Unable to initialize Snooping.");
            goto exit_all;
        }

    // If not in debug mode, fork and detatch from terminal.
    if ( ! debugMode ) 
    {

        IF_DEBUG atlog( LOG_DEBUG, 0, "Starting daemon mode.");

        // Only daemon goes past this line...
        if (fork()) 
            exit(0);
    }

    // Detach deamon from terminal
    if ( close( 0 ) < 0 || close( 1 ) < 0 || close( 2 ) < 0 
         || open( "/dev/null", 0 ) != 0 || dup2( 0, 1 ) < 0 || dup2( 0, 2 ) < 0
         || setpgrp() < 0 ) 
    {
        atlog( LOG_ERR, errno, "failed to detach deamon" );
    }

    // Go to the main loop.
    igmpMainRun();

    // Clean up
    igmpProxyCleanUp();

    // cleanup the proxy.
    if(commonConfig.mode&0x02)
    {

    }

    //cleanup the snooping
    if(commonConfig.mode&0x01)
    {
        igmpSnoopCleanUp();
    }

exit_all:
    // Inform that we are exiting.
    atlog(LOG_INFO, 0, "Shutdown complete....");
    while(1)
        sleep(10);
    exit(0);
}

void createVif()
{
    /* create VIFs for all IP, non-loop interfaces
     */
    unsigned int    Ix;
    struct IfDesc   *Dp;
    int             vifcount = 0;
    upStreamVif = -1;

    for ( Ix = 0; (Dp = getIfByIx( Ix )); Ix++ ) 
    {
        if(Dp->state == IF_STATE_UPSTREAM) 
        {
            if(upStreamVif == -1) 
            {
                upStreamVif = Ix;
            } 
            else 
            {
                atlog(LOG_ERR, 0, "Vif #%d was already upstream. Cannot set VIF #%d as upstream as well.",
                    upStreamVif, Ix);
            }
        }
        // if Snooping only, we just need downstreams.
        if((commonConfig.mode==1)&&(Dp->state == IF_STATE_UPSTREAM)) 
            continue;

        if ( Dp->state != IF_STATE_UPSTREAM && Dp->state != IF_STATE_DOWNSTREAM)
            continue;

        //printf("addMIF: Dp->index = %d\n", Dp->index);            

        addMIF( Dp );
        vifcount++;
    }

    // If there is only one VIF, or no defined upstream VIF, we send an error.	
    if((commonConfig.mode&0x2)&&(upStreamVif<0))
    {
        atlog(LOG_ERR, 0, "There must be one upstream in the proxy mode.");
    }

    if(vifcount >0 ) 
    {
        atlog(LOG_INFO, 0, "We have created %d total streams including Upstream Interface %d",vifcount,upStreamVif);
    }
}

/**
*   Handles the initial startup of the daemon.
*/
int icmpv6ProxyInit()
{
    struct sigaction sa;
    int Err;

    sa.sa_handler = signalHandler;
    sa.sa_flags = 0;    /* Interrupt system calls */
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);

    // Loads configuration for Physical interfaces...
    buildIfVc();    

    
    // Configures IF states and settings
    configureVifs();

    switch ( Err = enableMRouter() )
    {
	    case 0: break;
	    case EADDRINUSE: atlog( LOG_ERR, EADDRINUSE, "MC-Router API already in use" ); break;
	    default: atlog( LOG_ERR, Err, "MRT_INIT failed" );
    }

    //showIfVc();

    createVif();
    
    // Initialize IGMP
    initIcmpv6();
    
    // Initialize Routing table
    initRouteTable();    
    
    // Initialize timer
    callout_init();
    
    setStartCount();

    /* Removed igmpTimerHandle(), the timer and snooping is not initilizated now.
     * We just call it in igmpMainRun() before enter the dead loop.
     */
    //igmpTimerHandle();

    return 1;
}

/**
*   Clean up all on exit...
*/
void igmpProxyCleanUp()
{
    atlog( LOG_DEBUG, 0, "clean handler called" );
    
    free_all_callouts();    // No more timeouts.
    clearAllRoutes();       // Remove all routes.
    disableMRouter();       // Disable the multirout API
    closeIpv6RawSocket();
}

void handle_ipv6(int ifindex, char *msg, int len)
{
//    int i;
    struct sockaddr_in6 from;
    struct in6_addr *src = &(msg[8]);
    struct in6_addr *dst = &(msg[24]);

    if ( (msg[24] & 255) != 0xff )  //not multicast addr 
        return;

/*
    printf("ifindex = %d\n", ifindex);
    for (i = 0; i < 70; i++)
        printf("%02x ", msg[i] & 255);
    printf("\n\n");
*/   

    if ( (msg[40] == 0x3a) && 
         ( (msg[48] & 255) == 143 ) || ( (msg[48] & 255) == 131 ) || ( (msg[48] & 255) == 132 )
       )
    {       
        if ( ifindex != downStreamIfDesc->index)
            return;
        
        from.sin6_family = AF_INET6;
        from.sin6_port = 0;
        from.sin6_flowinfo = 0;
        from.sin6_addr = *dst;
        from.sin6_scope_id = ifindex;
     
        acceptIcmpv6(msg + 48, &from);
        return;
    }
    else
    {       
        activateRoute(&(msg[24]), &(msg[8]));        
    }                   
    
}

/**
*   Main daemon loop.
*/
void igmpMainRun() 
{
    // Get the config.
    //struct Config *config = getCommonConfig();
    // Set some needed values.
    register int recvlen;
    int     MaxFD, Rt, secs;
    fd_set  ReadFDS;
    struct  timeval  curtime, lasttime, difftime, tv; 
    // The timeout is a pointer in order to set it to NULL if nessecary.
    struct  timeval  *timeout = &tv;
    int loop;
    struct sockaddr_in6 from;
    unsigned int socklen = sizeof(struct sockaddr_in6);
    char isFromMyself;
    struct tpacket_hdr *hdr;
    struct sockaddr_ll *sa;
    char *bp;

    // Initialize timer vars
    difftime.tv_usec = 0;
    gettimeofday(&curtime, NULL);
    lasttime = curtime;

    // First thing we send a membership query in downstream VIF's...
    igmpTimerHandle();
    //sendGeneralMembershipQuery();

    // Loop until the end...
    for (;;) 
    {
        // Process signaling...
        if (sighandled) 
        {
            if (sighandled & GOT_SIGINT) 
            {
                sighandled &= ~GOT_SIGINT;
                atlog(LOG_NOTICE, 0, "Got a interupt signal. Exiting.");
                break;
            }
        }

        // Prepare timeout...
        secs = timer_nextTimer();
        if(secs == -1) 
        {
            timeout = NULL;
        } 
        else 
        {
            timeout->tv_usec = 0;
            timeout->tv_sec = secs;
        }

        // Prepare for select.
        MaxFD = Ipv6Sock;
        
        FD_ZERO( &ReadFDS );
        FD_SET( Ipv6Sock, &ReadFDS );


        // wait for input
        Rt = select( MaxFD +1, &ReadFDS, NULL, NULL, timeout );

        // atlog and ignore failures
        if( Rt < 0 ) 
        {
            atlog( LOG_WARNING, errno, "select() failure" );
            continue;
        }
        else if( Rt > 0 && Ipv6SockMapCurPtr) 
        {
            // Read IGMP request, and handle it...
            if( FD_ISSET( Ipv6Sock, &ReadFDS ) ) 
            {
                loop = 0;


                while (loop < MAX_EAT_ONE_CYCLE && *(unsigned long *)Ipv6SockMapCurPtr) 
                {
                    hdr = (struct tpacket_hdr *)Ipv6SockMapCurPtr;
                    sa = (struct sockaddr_ll *)( ((unsigned char *)hdr) + TPACKET_ALIGN(sizeof(*hdr)) );
                    bp = ((unsigned char *)hdr) + hdr->tp_mac;

                    if ( sa->sll_protocol == htons(ETH_P_IPV6) && sa->sll_pkttype != PACKET_OUTGOING && 
                         bp[0] == 0x33 && bp[1] == 0x33 )
                        handle_ipv6(sa->sll_ifindex, bp + 14, hdr->tp_len);
                    
                    hdr->tp_status = TP_STATUS_KERNEL;
                    Ipv6SockMapCurPtr += tp_frame_size;
                    if (Ipv6SockMapCurPtr >= (((char *)Ipv6SockMap) + mmapedlen))
                        Ipv6SockMapCurPtr = (char *)Ipv6SockMap;
                    loop++;
                }
                
            }
        }

        // At this point, we can handle timeouts...
        do 
        {
            /*
             * If the select timed out, then there's no other
             * activity to account for and we don't need to
             * call gettimeofday.
             */
            if (Rt == 0) 
            {
                curtime.tv_sec = lasttime.tv_sec + secs;
                curtime.tv_usec = lasttime.tv_usec;
                Rt = -1; /* don't do this next time through the loop */
            } 
            else 
            {
                gettimeofday(&curtime, NULL);
            }
            difftime.tv_sec = curtime.tv_sec - lasttime.tv_sec;
            difftime.tv_usec += curtime.tv_usec - lasttime.tv_usec;
            while (difftime.tv_usec > 1000000) 
            {
                difftime.tv_sec++;
                difftime.tv_usec -= 1000000;
            }
            if (difftime.tv_usec < 0) 
            {
                difftime.tv_sec--;
                difftime.tv_usec += 1000000;
            }
            lasttime = curtime;
            
            if (secs == 0 || difftime.tv_sec > 0) 
            {
                /* Fix bug 793, when the ntp client ajust the local timezone, 
                 * the curtime and lasttime need be adjusted too.
                 */
                if(difftime.tv_sec > INTERVAL_QUERY + 1) 
                {
                    gettimeofday(&curtime, NULL);
                    lasttime = curtime;
                    break;
                }
                age_callout_queue(difftime.tv_sec);
            }
            secs = -1;
        } while (difftime.tv_sec > 0);
    }
}

/*
 * Signal handler.  Take note of the fact that the signal arrived
 * so that the main loop can take care of it.
 */
static void signalHandler(int sig) {
    switch (sig) {
    case SIGUSR1:
    case SIGTERM:
        sighandled |= GOT_SIGINT;
        break;
        /* XXX: Not in use.
        case SIGHUP:
            sighandled |= GOT_SIGHUP;
            break;
    
        case SIGUSR1:
            sighandled |= GOT_SIGUSR1;
            break;
    
        case SIGUSR2:
            sighandled |= GOT_SIGUSR2;
            break;
        */
    }
}
