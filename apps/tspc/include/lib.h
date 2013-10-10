/*
---------------------------------------------------------------------------
 lib.h,v 1.11 2004/02/11 04:52:41 blanchet Exp
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

#ifndef _LIB_H_
#define _LIB_H_

#include "errors.h"

/* globals */

#define IPv4Addr		".0123456789"
#define IPv6Addr		"ABCDEFabcdef:0123456789"
#define Numeric			"0123456789"

/* exports */

int IsAll(char *, char *);
int IsPresent(char *);
int GetSizeOfNullTerminatedArray(char **);

char *tspGetErrorByCode(int code);


#endif

