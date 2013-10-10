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
#include <stdarg.h>
#include <syslog.h>
#include <string.h>

int debuglevel = 10;
int printdebug = 0;

void ipaddr2str(char *buffer, unsigned int ipaddr)
{
	int	addr_byte[4];
	int	i;
	unsigned int	xbyte;

	for(i = 0;i < 4;i++) {
		xbyte = ipaddr >> (i*8);
		xbyte = xbyte & (unsigned int)0x000000FF;
		addr_byte[i] = xbyte;
	}
	sprintf(buffer, "%u.%u.%u.%u", addr_byte[3], addr_byte[2],
		addr_byte[1], addr_byte[0]);
}

void prefix2mask(int prefix,unsigned int *addr) {
	unsigned long n=0;
	int i;
	for(i=31;i>31-prefix;i--) {
		n |= (1<<i);
	}
	*addr = n;
}

int min(int a,int b) { return (a < b) ? a : b; }
int max(int a,int b) { return (a > b) ? a : b; }

void log_msg(const char *p, ... )
{
	va_list ap;
	
	va_start(ap,p);
	vsyslog(LOG_NOTICE,p,ap);
	if (printdebug) {
		printf("log:\t");
		vprintf(p,ap);
		printf("\n");
	}
	va_end(ap);
}

void error(const char *p, ... )
{
	va_list ap;
	
	va_start(ap,p);
	vsyslog(LOG_NOTICE,p,ap);
	if (printdebug) {
		printf("error:\t");
		vprintf(p,ap);
		printf("\n");
	}
	va_end(ap);
}

void debug(int level,const char *p, ... )
{
	va_list ap;

	if(level <= debuglevel) return;
	
	va_start(ap,p);
	vsyslog(LOG_NOTICE,p,ap);
	if (printdebug) {
		printf("debug:\t");
		vprintf(p,ap);
		printf("\n");
	}
	va_end(ap);
}

/*
 *	Like strncpy, but always adds \0
 */
char *strNcpy(char *dest, char *src, int n)
{
	if (n > 0)
		strncpy(dest, src, n);
	else
		n = 1;
	dest[n - 1] = 0;

	return dest;
}
