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

#ifndef _UTIL_
#define _UTIL_

extern int debuglevel;
extern int printdebug;

void ipaddr2str(char *buffer, unsigned int ipaddr);
void prefix2mask(int prefix,unsigned int *addr);
int min(int a,int b);
int max(int a,int b);

void log_msg(const char *p, ... );
void error(const char *p, ... );
void debug(int level,const char *p, ... );

char *strNcpy(char *dest, char *src, int n);

#endif //_UTIL_
