/*
---------------------------------------------------------------------------
 cli.c,v 1.9 2004/04/05 21:44:31 dgregoire Exp
---------------------------------------------------------------------------
* This source code copyright (c) Hexago Inc. 2002-2004.
* 
* This program is free software; you can redistribute it and/or modify it 
* under the terms of the GNU General Public License (GPL) Version 2, 
* June 1991 as published by the Free  Software Foundation.
* 
* This program is distributed in the hope that it will be useful, 
* but WITHOUT ANY WARRANTY;  without even the implied warranty of 
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
* See the GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License 
* along with this program; see the file GPL_LICENSE.txt. If not, write 
* to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
* MA 02111-1307 USA
---------------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#include "platform.h"

#include "cli.h"
#include "log.h"	// Verbose
#include "lib.h"
#include "config.h"
#include "cnfchk.h"		// tspFixConfig

void PrintUsage(char *Message, ...) {

	if(Message) {
		va_list     argp;
		va_start(argp, Message);
		vprintf(Message, argp);
		va_end(argp);
	}

   printf("usage: tspc [options] [-f config_file] [-r seconds]\n");
   printf("  where options are :\n");
   printf("    -v    set verbose level (-v,-vv or -vvv)\n");
   printf("    -i    gif interface to use for tunnel_v6v4\n");
   printf("    -u    interface to use for tunnel_v6udpv4\n");
   printf("    -s    interface to query to get IPv4 source address\n");
   printf("    -f    Read this config file instead of %s \n", FileName);
   printf("    -r    Retry after n seconds until success\n");
   printf("    -c    Verify and fix the config file (to migrate template names)\n");
   printf("    -h    help\n");
   printf("    -?    help\n");
   printf("\n");
   return;
}


int ParseArguments(int argc, char *argv[], tConf *Conf) {
    int ch;
   
    while ((ch = getopt(argc, argv, "h?cvf:r:i:u:s:")) != -1) {
       switch (ch) {
       case 'v':
	 Verbose++;
         break;
       case 's':
         Conf->if_source = optarg;
         break;
       case 'i':
         Conf->if_tunnel_v6v4 = optarg;
         break;
       case 'u':
	 Conf->if_tunnel_v6udpv4 = optarg;
	 break;
       case 'f':
         FileName = optarg;
         break;
       case 'r':
	 Conf->retry = atoi(optarg);
	 break;
	   case 'c':
     return tspFixConfig();
       case '?':
       case 'h':
         PrintUsage(NULL);
	 return -1;
       default:
         PrintUsage("Error while parsing command line arguments");
	 return -1;
	     }
       
    }
    return 0;
}









