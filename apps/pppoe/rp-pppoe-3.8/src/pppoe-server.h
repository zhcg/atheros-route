/**********************************************************************
*
* pppoe-server.h
*
* Definitions for PPPoE server
*
* Copyright (C) 2001-2006 Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
* LIC: GPL
*
* $Id: //depot/sw/releases/Aquila_9.2.0_U11/apps/pppoe/rp-pppoe-3.8/src/pppoe-server.h#1 $
*
***********************************************************************/

#include "pppoe.h"
#include "event.h"

#ifdef HAVE_L2TP
#include "l2tp/l2tp.h"
#endif

#define MAX_USERNAME_LEN 31
/* An Ethernet interface */
typedef struct {
    char name[IFNAMSIZ+1];	/* Interface name */
    int sock;			/* Socket for discovery frames */
    unsigned char mac[ETH_ALEN]; /* MAC address */
    EventHandler *eh;		/* Event handler for this interface */

    /* Next fields are used only if we're an L2TP LAC */
#ifdef HAVE_L2TP
    int session_sock;		/* Session socket */
    EventHandler *lac_eh;	/* LAC's event-handler */
#endif
} Interface;

#define FLAG_RECVD_PADT      1
#define FLAG_USER_SET        2
#define FLAG_IP_SET          4
#define FLAG_SENT_PADT       8

/* Only used if we are an L2TP LAC or LNS */
#define FLAG_ACT_AS_LAC      256
#define FLAG_ACT_AS_LNS      512
#define HAVE_LINUX_KERNEL_PPPOE

/* Forward declaration */
struct ClientSessionStruct;

/* Dispatch table for session-related functions.  We call different
   functions for L2TP-terminated sessions than for locally-terminated
   sessions. */
typedef struct PppoeSessionFunctionTable_t {
    /* Stop the session */
    void (*stop)(struct ClientSessionStruct *ses, char const *reason);

    /* Return 1 if session is active, 0 otherwise */
    int (*isActive)(struct ClientSessionStruct *ses);

    /* Describe a session in human-readable form */
    char const * (*describe)(struct ClientSessionStruct *ses);
} PppoeSessionFunctionTable;

extern PppoeSessionFunctionTable DefaultSessionFunctionTable;

/* A client session */
typedef struct ClientSessionStruct {
    struct ClientSessionStruct *next; /* In list of free or active sessions */
    PppoeSessionFunctionTable *funcs; /* Function table */
    pid_t pid;			/* PID of child handling session */
    Interface *ethif;		/* Ethernet interface */
    unsigned char myip[IPV4ALEN]; /* Local IP address */
    unsigned char peerip[IPV4ALEN]; /* Desired IP address of peer */
    UINT16_t sess;		/* Session number */
    unsigned char eth[ETH_ALEN]; /* Peer's Ethernet address */
    unsigned int flags;		/* Various flags */
    time_t startTime;		/* When session started */
    char const *serviceName;	/* Service name */
#ifdef HAVE_LICENSE
    char user[MAX_USERNAME_LEN+1]; /* Authenticated user-name */
    char realm[MAX_USERNAME_LEN+1]; /* Realm */
    unsigned char realpeerip[IPV4ALEN];	/* Actual IP address -- may be assigned
					   by RADIUS server */
    int maxSessionsPerUser;	/* Max sessions for this user */
#endif
#ifdef HAVE_L2TP
    l2tp_session *l2tp_ses;	/* L2TP session */
    struct sockaddr_in tunnel_endpoint;	/* L2TP endpoint */
#endif
} ClientSession;

/* Hack for daemonizing */
#define CLOSEFD 64

/* Max. number of interfaces to listen on */
#define MAX_INTERFACES 64

/* Max. 64 sessions by default */
#define DEFAULT_MAX_SESSIONS 64

/* An array of client sessions */
extern ClientSession *Sessions;

/* Interfaces we're listening on */
extern Interface interfaces[MAX_INTERFACES];
extern int NumInterfaces;

/* The number of session slots */
extern size_t NumSessionSlots;

/* The number of active sessions */
extern size_t NumActiveSessions;

/* Offset of first session */
extern size_t SessOffset;

/* Access concentrator name */
extern char *ACName;

extern unsigned char LocalIP[IPV4ALEN];
extern unsigned char RemoteIP[IPV4ALEN];

/* Do not create new sessions if free RAM < 10MB (on Linux only!) */
#define MIN_FREE_MEMORY 10000

/* Do we increment local IP for each connection? */
extern int IncrLocalIP;

/* Free sessions */
extern ClientSession *FreeSessions;

/* When a session is freed, it is added to the end of the free list */
extern ClientSession *LastFreeSession;

/* Busy sessions */
extern ClientSession *BusySessions;

extern EventSelector *event_selector;
extern int GotAlarm;

extern void setAlarm(unsigned int secs);
extern void killAllSessions(void);
extern void serverProcessPacket(Interface *i);
extern void processPADT(Interface *ethif, PPPoEPacket *packet, int len);
extern void processPADR(Interface *ethif, PPPoEPacket *packet, int len);
extern void processPADI(Interface *ethif, PPPoEPacket *packet, int len);
extern void usage(char const *msg);
extern ClientSession *pppoe_alloc_session(void);
extern int pppoe_free_session(ClientSession *ses);
extern void sendHURLorMOTM(PPPoEConnection *conn, char const *url, UINT16_t tag);
#define pppoe_proxy
#ifdef pppoe_proxy
#include<stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#define MAX_CLIENT_SESSION 8
#define MAX_PROXY_SESSION 4
#define RV_TRUE 1
#define RV_FALSE 0

typedef enum
{
    e_null,
    e_alloc,
    e_waiting_auth,
    e_auth_suc,
    e_auth_fail,
}e_cliSesState;

typedef struct s_clientSession
{
    struct s_clientSession*next;  
    int state;
    pid_t childPid;
    int sock;
    void*proxySession;
}clientSession;

typedef enum
{
   e_state_inactive,
   e_state_active,   
}stateDes;
typedef struct  s_proxySession
{
        struct  s_proxySession*next;
        int state;
	 pid_t pid;
        char* username;
        char* pwd;
	 int sock;
	 int actCliNum;
	 clientSession* clientSesList;
        clientSession* freeCliSes; 	 
}proxySession;

typedef struct  
{
       proxySession* FirstProxySes;
       proxySession* freeProxySes; 	 
}proxySessionList;


#define STARTING_COMM_PORT 7000

typedef enum
{
     e_com_null,
     e_com_reqAuth,
     e_com_rspAuth,
}commMsgType;
typedef enum
{
    pppAuthNull,
    pppAuthSucc,
    pppAuthfail,
}pppAuthRes;
typedef struct
{
     char user[64];
     char pwd[32];
     unsigned int dnsaddr[2];	/* Primary and secondary MS DNS entries */
     unsigned int winsaddr[2];	/* Primary and secondary MS WINS entries */
}authMsg;

typedef struct
{
    commMsgType msgtype;
    pid_t pid;
    pppAuthRes result;
    char data[256];
}pppdCommMsg;

void initProxy();
void initProxySesList();
proxySession* allocProxySes(char*username,char*pwd);
int freeProxySes(proxySession*session);
proxySession* checkProxySes(char*username,char*pwd);
clientSession* addClientSes(proxySession*proxySes);
int delClientSes(clientSession*clientSes);
clientSession* checkClientSes(pid_t pid);
int termClientSes(pid_t pid);
void killAllProxy();
void processReqAuth(pppdCommMsg*message,int sock);
void processRspAuth(pppdCommMsg*message,int sock);
void responseAuthSucc(pppdCommMsg*message,int sock);
void startPppdProxy(proxySession* pProxySes);
void responseAuthFail(pppdCommMsg*message,int sock);
#endif 
#ifdef HAVE_LICENSE
extern int getFreeMem(void);
#endif
