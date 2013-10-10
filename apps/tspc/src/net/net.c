/*
---------------------------------------------------------------------------
 net.c,v 1.5 2004/02/11 04:52:51 blanchet Exp
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

#include <stdlib.h>
#include <sys/types.h>

#define _USES_SYS_SOCKET_H_
#define _USES_NETINET_IN_H_
#define _USES_ARPA_INET_H_
#define _USES_NETDB_H_

#include "platform.h"

#include "net.h"

struct in_addr *NetText2Addr(char *Address)
{
  struct hostent *host;
  static struct in_addr saddr;

  /* First try it as aaa.bbb.ccc.ddd. */

  saddr.s_addr = inet_addr(Address);
  if (saddr.s_addr != -1) {
    return &saddr;
  }

  host = gethostbyname(Address);

  if (host != NULL) {
    return (struct in_addr *) *host->h_addr_list;
  }

  return NULL;
}

int NetReadLine(unsigned char *in, size_t il, unsigned char *line, size_t ll)
{
	int i;

	if (in == NULL || line == NULL)
		return -1;
	
	for (i = 0; i < il; i++) {
		if (i == ll) {
			line[i] = 0;
			return i;		/* end of input buffer reached */
		}
		
		*(line + i) = *(in + i);
		if (ll == i || *line == '\n')
			break;
	}
	
	line[i] = 0;
	return i;
}
