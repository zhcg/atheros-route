/*
---------------------------------------------------------------------------
 lib.c,v 1.9 2004/02/11 04:52:49 blanchet Exp
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
#include <string.h>

#include "platform.h"

#include "lib.h"

// these must be kept in sync with errors.h

char *TspErrorCodesArray[] = { "NO_ERROR",
							"TSP_ERROR",
							"SOCKET_ERROR",
							"INTERFACE_SETUP_FAILED",
							"KEEPALIVE_TIMEOUT",
							"TUNNEL_ERROR",
							"TSP_VERSION_ERROR",
							"AUTHENTIFICATION_ERROR",
							NULL
};

/*
   Check if all characters in Value are within AllowedChars list.
*/

int IsAll(char *AllowedChars, char *Value)
{
	if(Value) {
		for(;*Value; Value++) {
			if(strchr(AllowedChars, *Value) == NULL)
				return 0;
		}
	} else {
		return 0;
	}
	return 1;
}

/*
   Check to see if there is a value in the char *
   If not, then the value was not supplied.
*/

int IsPresent(char *Value)
{
	if(Value)
		if(strlen(Value))
			return 1;
	return 0;
}

/* This next function is very dangerous.
   If you can call be certain the array
   finished by NULL or it will do bad things.
   */

int GetSizeOfNullTerminatedArray(char **a) {
	int i;
	for (i = 0;;i++) {
		if (a[i] == NULL)
			return i;
	}
	return -1;
}


char *tspGetErrorByCode(int code) {
	static char buf[1024];
	int i;
	
	i = GetSizeOfNullTerminatedArray(TspErrorCodesArray);
	if (code < i && code > -1)
		return TspErrorCodesArray[code];
	else
		snprintf(buf, sizeof(buf), "%i is not defined as a client error, might be a TSP error?", code);
	return buf;
}




