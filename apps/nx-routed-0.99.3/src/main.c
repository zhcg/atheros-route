/*
 *
 *	NX-ROUTED
 *	RIP-2 Routing Daemon
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *  
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *  
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *	Copyright (C) 2002 Valery Kholodkov
 *	Copyright (C) 2002 Andy Pershin
 *	Copyright (C) 2002 Antony Kholodkov
 *
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/resource.h>

#include <time.h>

#include <getopt.h>

#include "ctlfile.h"
#include "router.h"
#include "socket.h"
#include "link.h"
#include "rip.h"
#include "util.h"
#include "sys_linux.h"
#include "conf.h"
#include "version.h"
#include "timers.h"

extern char *optarg;
int dontfork=0;
time_t linkupdateinterval = 30;
time_t linkupdatetime;

int netlink_init();

//
// Returns 1 if it's ok to startup
// or 0 if another instance is running
//
int check_pid_file(const char *piddirname, const char *pidfilename) {
	FILE *f;
	int pid;	
	
/*	if(access(piddirname, R_OK|W_OK) < 0) {
		return 	
	}*/	
	
/*	if(access(pidfilename, R_OK|W_OK) < 0) {
		
	}*/
	
	if((f = fopen(pidfilename,"r")) == NULL)
		return 1;
	
	if(fscanf(f,"%d",&pid) != 1) {
		fclose(f);
		return 1;
	}
	
	fclose(f);
	
	// Got a pid
	// Try kill him with a safe signal
	if(kill(pid,SIGUSR2) < 0)
		if(errno == ESRCH)
			return 1;
			
	// Oops! My brother is still alive!
	return 0;
}

int create_pid_file(const char *piddirname, const char *pidfilename, int pid) {
	int fdesc;
	FILE *f;
	char templ[256];
	
	strcpy(templ,piddirname);
	strcat(templ,"/nx-routedXXXXXX");
	
	if((fdesc = mkstemp(templ)) < 0) {
		error("Unable to create pid file");
		return -1;
	}
	
	if((f = fdopen(fdesc,"w")) == NULL) {
		error("Unable to create pid file");
		return -1;
	}
	
	fprintf(f,"%d\n",pid);
	
	fclose(f);

	if(rename(templ,pidfilename) < 0) {
		unlink(templ);
		error("Unable to move temporary file");
		return -1;
	}
	
	return 0;	
}

int set_limits() {
        struct rlimit rlimit;

        rlimit.rlim_cur = RLIM_INFINITY; 
        rlimit.rlim_max = RLIM_INFINITY;

        if(setrlimit(RLIMIT_CORE, &rlimit) < 0) {
            error("Unable to set core file size limit");
            return -1;
        }
	
	return 0;	
}

int parse_command_line(int argc,char *argv[])
{
	char ch;
	int i;
	// FIXME: Don't forget to remove it nahren
	while ((ch=getopt(argc,argv,"dpl:i:p:u:")) != EOF)
	{
		switch(ch)
		{
			case 'd':
				dontfork=1;
				break;	
			case 'l':
				if((i = atoi(optarg)) != 0)
					debuglevel = i;
				break;	
			case 'u':
				if((i = atoi(optarg)) != 0)
					linkupdateinterval = i;
				break;	
			case 'p':
				printdebug=1;
				break;
			case '?': 
				return 0;
			default:
				printf("Invalid options: %c",ch);
				return 0;
		}
	}
	return 1;
}

void terminate_signal(int sig) {
#ifdef DEBUG
	log_msg("received non-fatal signal %s",strsignal(sig));
#endif
	linkupdatetime = -1;
	signal(sig,terminate_signal);
}

void abort_signal(int sig) {
	static int infatalsignal = 0;
	error("terminated by fatal signal %s",strsignal(sig));
	if(!infatalsignal) {
		infatalsignal = 1;
		remove_dynamic_routes(main_rtable);
	}
	abort();
}

/* 
 * Damjan "Zobo" <zobo@lana.krneki.org>
 * I can tell routed to imidetely rescan links by sending this signal.
 * Usefull when ppp comes up
 */
void link_update_signal(int sig) {
	log_msg("Recived signal %s(%d) - updating links", strsignal(sig), sig);
	linkupdatetime = 0;
	signal(sig,terminate_signal);
}

char **myargv;
#define CONFPATH "/tmp/routed.conf"
int main(int argc,char *argv[])
{
//	int t;
	pid_t pid;
	
	myargv = argv;
	
	if(!parse_command_line(argc,argv))
		return 2;
		
	// Figure out where the config is
	if(access("./routed.conf",R_OK) < 0) {
		if(access(CONFPATH, R_OK) < 0) {
			error("Missing configuration files");
			return 3;
		}else{
			if(!parse_ctl_file(CONFPATH)) {
				error("Invalid configuration file %s",CONFPATH);
				return 3;			
			}
		}
		
	}else{
		if(!parse_ctl_file("./routed.conf")) {
			error("Invalid configuration file ./routed.conf");
			return 3;
		}
	}
	
	log_msg(ROUTED_BANNER " version " ROUTED_VERSION);
	
	if(rip2_init_config() < 0) {
		fprintf(stderr,"Error while parsing RIP-2 config");
		return 1;
	}
	
	main_rtable = new_rtable();

	/*
	 * I changed the order of initialization coz we
	 * need good socket at socket_list.next. If you know
	 * how to fix this hack, email me!
	 */
	if(rip2_init() < 0) {
		fprintf(stderr,"Unable to initialize RIP2");
		return 1;
	}
	
	if(netlink_init() < 0) {
		fprintf(stderr,"Unable to initialize Netlink");
		return 1;
	}

	if(parse_links() < 0) {
		fprintf(stderr,"Error while parsing links");
		return 1;
	}	

	// That was and old stuff, should clean it later
	//main_rtable = system_read_rtable();
#ifdef DEBUG	
	log_msg("System routing table");
	dump_rtable(main_rtable);
#endif

	if(!dontfork) {			
		// Prevent stopping by SIGTTOU
		printdebug = 0;	
		switch(pid = fork()) {
			case 0: break;
			case -1:
				error("Cannot move itself to background, fork failed");
				return 4;			
			default:
				return 0; // Tell the parent we are ok :)
		}
	}

	signal(SIGINT,terminate_signal);
	signal(SIGHUP,terminate_signal);
	signal(SIGTERM,terminate_signal);
	signal(SIGUSR1,link_update_signal);
	
	// Should catch up some cruel signals
	// ????? ?????? ???? ???????????? ???????? ??????? ?????????????
#ifndef DEBUG	
	signal(SIGSEGV,abort_signal);
	signal(SIGQUIT,abort_signal);
#endif	

	if(!check_pid_file("/var/run","/var/run/nx-routed.pid")) {
		error("Another instance of nx-routed is already running");
		exit(5);
	}
	
	if(!dontfork) {
		setsid();

		/*
		 *	Close unused file descriptors.
		 */
		/* FIXME: In the setup process we open some sockets.
		 * Dont want to close them before they even start doing
		 * anything usefull
		 * 
		for (t = 32; t >= 3; t--)
				close(t);
		 */
		 /* valery: Avoiding SIGTTOU is a good reason to do this */
	}

        // FIXME: Race condition here	
	
	if(!create_pid_file("/var/run","/var/run/nx-routed.pid",getpid()) < 0) {
		error("Cannot create pid file");
		exit(6);
	}

        if(set_limits()) {
		error("Unable to set limits");
		exit(7);
        }
	
	// Ok, we are daemon now
	
	linkupdatetime = 0;
	socketlist_build_fdsets();
	while(socketlist_wait_event() == 0) {
		if(linkupdatetime <= time(NULL)) {
			debug(1,"Updating link's state");
			linklist_update();
			linkupdatetime = time(NULL) + linkupdateinterval;
		}
		if(set_expiring_timer(main_rtable) < 0) {
			error("Unable to set expiration timer");
		}
	}
	
	remove_dynamic_routes(main_rtable);
	free_rtable(main_rtable);
	socketlist_shutdown();
	linklist_shutdown();
	shutdown_routing_protocols();
	timerlist_shutdown();
	
	exit(0);
}
