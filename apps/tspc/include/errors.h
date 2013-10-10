/*
---------------------------------------------------------------------------
 errors.h,v 1.8 2004/04/20 15:10:54 parent Exp
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

#ifndef _ERRORS_H_
#define _ERRORS_H_

/* globals */

#ifdef NO_ERROR
#undef NO_ERROR
#endif

#ifdef TSP_ERROR
#undef TSP_ERROR
#endif

#ifdef SOCKET_ERROR
#undef SOCKET_ERROR
#endif

#ifdef INTERFACE_SETUP_FAILED
#undef INTERFACE_SETUP_FAILED
#endif

#ifdef KEEPALIVE_TIMEOUT
#undef KEEPALIVE_TIMEOUT
#endif

#ifdef TUNNEL_ERROR
#undef TUNNEL_ERROR
#endif



typedef enum {
  NO_ERROR = 0,
  TSP_ERROR,
  SOCKET_ERROR,
  INTERFACE_SETUP_FAILED,
  KEEPALIVE_TIMEOUT,
  TUNNEL_ERROR,
  TSP_VERSION_ERROR,
  AUTHENTIFICATION_ERROR
} tErrors;


#endif

