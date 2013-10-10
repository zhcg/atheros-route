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
*   defs.h - Header file for common includes.
*/

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syslog.h>
#include <signal.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <unistd.h>
#include <net/if.h>

#include <netinet/in.h>
#include <linux/if_packet.h>
#include <sys/mman.h>


// The multicats API needs linux spesific headers !!!                  

//#include <linux/in.h>
//#include <linux/mroute.h>
//#include <linux/in6.h>
#include <linux/if_ether.h>
#include <linux/icmpv6.h>
#include <arpa/inet.h>

#include <linux/mroute6.h>


// The default path for the config file...
#define     IGMPPROXY_CONFIG_FILEPATH     "/etc/igmpproxy.conf"
#define     ENABLE_DEBUG    1

/*
 * Limit on length of route data
 */
#define MAX_IP_PACKET_LEN   576
#define MIN_IP_HEADER_LEN   20
#define MIN_IP6_HEADER_LEN  40
#define MAX_IP_HEADER_LEN   60

#define MAX_MC_VIFS    32     // !!! check this const in the specific includes
#define MAX_MC_MIFS    32     /* = to MAXMIFS from linux/mroute6.h */


// Useful macros..          
#define MIN( a, b ) ((a) < (b) ? (a) : (b))
#define MAX( a, b ) ((a) < (b) ? (b) : (a))
#define VCMC( Vc )  (sizeof( Vc ) / sizeof( (Vc)[ 0 ] ))
#define VCEP( Vc )  (&(Vc)[ VCMC( Vc ) ])

// Bit manipulation macros...
#define BIT_ZERO(X)      ((X) = 0)
#define BIT_SET(X,n)     ((X) |= 1 << (n))
#define BIT_CLR(X,n)     ((X) &= ~(1 << (n)))
#define BIT_TST(X,n)     ((X) & 1 << (n))
#define MAX_EAT_ONE_CYCLE 100


// Useful defs...
#define FALSE       0
#define TRUE        1

typedef void (*cfunc_t)   (void *);

typedef u_int8_t   uint8;
typedef u_int16_t  uint16;
typedef u_int32_t  uint32;

//#################################################################################
//  Globals
//#################################################################################

/*
 * External declarations for global variables and functions.
 */
#define RECV_BUF_SIZE 8192
extern char     *send_buf;

extern char     s1[];
extern char     s2[];
extern char     s3[];
extern char     s4[];
extern int      Ipv6Sock;  /* Ipv6 raw socket */
extern void     *Ipv6SockMap;
extern char     *Ipv6SockMapCurPtr;

extern int      tp_frame_size;
extern int      mmapedlen;

//#################################################################################
//  Lib function prototypes.
//#################################################################################

/* sysatlog.c
 */
extern int  Log2Stderr;           // Log threshold for stderr, LOG_WARNING .... LOG_DEBUG 
extern int  LogLastServerity;     // last atlogged serverity
extern int  LogLastErrno;         // last atlogged errno value
extern char LogLastMsg[ 128 ];    // last atlogged message

#define     IF_DEBUG    if(Log2Stderr & LOG_DEBUG)

void atlog( int Serverity, int Errno, const char *FmtSt, ... );

/* ifvc.c
 */
#define MAX_IF         40     // max. number of interfaces recognized 

// Interface states
#define IF_STATE_DISABLED      0   // Interface should be ignored.
#define IF_STATE_UPSTREAM      1   // Interface is the upstream interface
#define IF_STATE_DOWNSTREAM    2   // Interface is a downstream interface

// Multicast default values...
#define DEFAULT_ROBUSTNESS     2
#define DEFAULT_THRESHOLD      1
#define DEFAULT_RATELIMIT      0

// Define timer constants (in seconds...)
#define INTERVAL_QUERY          100//125
#define INTERVAL_QUERY_RESPONSE  10
//#define INTERVAL_QUERY_RESPONSE  10

#define ROUTESTATE_NOTJOINED            0   // The group corresponding to route is not joined
#define ROUTESTATE_JOINED               1   // The group corresponding to route is joined
#define ROUTESTATE_CHECK_LAST_MEMBER    2   // The router is checking for hosts



// Linked list of networks... 
struct globalIpList
{
    struct in6_addr         globalAddr;
    int                     globalMask;
    struct globalIpList*    next;
};

struct IfDesc 
{
    char                    Name[IFNAMSIZ];
    struct in6_addr         linkAddr;       /* Link local Address */
    int                     linkMask;       /* Link local Mask */
    short                   Flags;
    short                   state;          /* upstream or down stream */
    struct globalIpList*    globalIpInfo;
    unsigned int            robustness;
    unsigned char           threshold;   /* ttl limit */
    unsigned int            ratelimit; 
    unsigned int            index;
};

// Keeps common configuration settings 
struct Config {
    // 0:Undefined; 1:IGMP Snooping; 2:IGMP Proxy; 3: Snooping & Proxy;
    unsigned int        mode;
    unsigned int        robustnessValue;
    unsigned int        queryInterval;
    unsigned int        queryResponseInterval;
    // Used on startup..
    unsigned int        startupQueryInterval;
    unsigned int        startupQueryCount;
    // Last member probe...
    unsigned int        lastMemberQueryInterval;
    unsigned int        lastMemberQueryCount;
    // Set if upstream leave messages should be sent instantly..
    unsigned short      fastUpstreamLeave;
};

// Defines the Index of the upstream VIF...
extern int upStreamVif;

/* ifvc.c
 */
void buildIfVc( void );
void showIfVc( void );

struct IfDesc *getIfByName( const char *IfName );
struct IfDesc *getIfByIx( unsigned Ix );
struct IfDesc *getIfByAddress( struct in6_addr *ipAddr );
struct IfDesc *getIfByIfIdx( int index );



int initSnoopBr();
int clearSnoopBr();

/* mroute-api.c
 */
struct MRouteDesc {
    struct sockaddr_in6 OriginAdr;
    struct sockaddr_in6 McAdr;
    short               InVif;
    uint8               TtlVc[ MAX_MC_VIFS ];
};

int icmpv6ProxyInit();
void igmpProxyCleanUp();
int getIgmpPState();
void setIgmpPState(int);

int igmpSnoopInit();
void igmpSnoopCleanUp();
int getIgmpSState();
void setIgmpSState(int);
void delVIF( struct IfDesc *IfDp );

// IGMP socket as interface for the mrouted API
// - receives the IGMP messages
extern int MRouterFD;
extern struct Config commonConfig; 

int enableMRouter( void );
void disableMRouter( void );
void addMIF( struct IfDesc *Dp );
int addMRoute( struct MRouteDesc * Dp );
int delMRoute( struct MRouteDesc * Dp );
struct IfDesc * getIfDescFromMifByIx( int Ix );


/* config.c
 */
int loadConfig(char *configFile);
void configureVifs();
struct Config *getCommonConfig();

/* igmp.c
*/
extern struct in6_addr allhosts_group;
extern struct in6_addr allrouters_group;
extern struct in6_addr mld_report_group;
extern struct in6_addr allzero_addr;

extern struct IfDesc IfDescVc[ MAX_IF ];
extern unsigned int  IfDescVcNumber;
extern struct IfDesc * downStreamIfDesc;


void initIcmpv6(void);
void acceptIcmpv6(char * msg, struct sockaddr_in6 *from);
void sendIcmpv6(struct in6_addr *dst, int delay, struct in6_addr *group);
/* lib.c
 */
/*char   *fmtInAdr( char *St, struct in_addr InAdr );*/
char   *inetFmt(uint32 addr, char *s);
char   *inetFmts(uint32 addr, uint32 mask, char *s);
int     inetCksum(u_short *addr, u_int len);

/* kern.c
 */
void k_set_rcvbuf(int bufsize, int minsize);
void k_set_ttl(int t);
void k_set_loop(int l);
uint32 k_get_ifindex(char *ifname);
uint32 k_set_if(uint32 new_mcast_ifindex);
void joinReportGroup();

/*
void k_join(uint32 grp, uint32 ifa);
void k_leave(uint32 grp, uint32 ifa);
*/

/* udpsock.c
 */
int openUdpSocket( uint32 PeerInAdr, uint16 PeerPort );

/* mcgroup.c
 */
int joinMcGroup( int UdpSock, struct IfDesc *IfDp, struct in6_addr * mcastaddr );
int leaveMcGroup( int UdpSock, struct IfDesc *IfDp, struct in6_addr * mcastaddr );


/* rttable.c
 */
void initRouteTable();
void clearAllRoutes();
int insertRoute(struct in6_addr *group, struct in6_addr *src, struct sockaddr_in6 *from);
int activateRoute(struct in6_addr *group, struct in6_addr *originAddr);
void ageActiveRoutes();
void setRouteLastMemberMode(struct in6_addr * group, struct in6_addr *src);
int lastMemberGroupAge(struct in6_addr * group, struct in6_addr * src);

/* request.c
 */

void acceptGroupReport(uint32 port, struct sockaddr_in6 *from, 
                       struct in6_addr *group, struct in6_addr *src); 
void acceptLeaveMessage(struct sockaddr_in6 *from, 
                        struct in6_addr *group, struct in6_addr *src);

void sendGeneralMembershipQuery();
void igmpTimerHandle();
void setStartCount();

/* callout.c 
*/
void callout_init();
void free_all_callouts();
void age_callout_queue(int);
int timer_nextTimer();
int timer_setTimer(int, cfunc_t, void *);
int timer_clearTimer(int);
int timer_leftTimer(int);

/* fdbtable.c 
 */
int insertFdb(struct in6_addr * group, uint32 port);
void initFdbTable();

/* fdb.c
 */
void clearAllFdbs();
void ageActiveFdbs();
int internUpdateFdb(struct in6_addr *group, uint32 ifBits, int activate);
void logFdbTable(char *);

/* confread.c
 */
#define MAX_TOKEN_LENGTH    30

int openConfigFile(char *filename);
void closeConfigFile();
char* nextConfigToken();
char* getCurrentConfigToken();

/* switch-api.c
 */
int addMFdb(struct in6_addr *group, uint32 port);
int addMFdbByMac(unsigned char * macAddr, uint32 port);
int delMFdb(struct in6_addr * group);
int setMultiOnVlan(int enable);
int enableHwIcmpv6();
int disableHwIGMPS();
