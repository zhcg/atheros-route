/*
---------------------------------------------------------------------------
 log.h,v 1.3 2004/02/11 04:52:41 blanchet Exp
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

#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

#ifdef LOG_IT
# define ACCESS
#else
# define ACCESS extern
#endif

enum tErrorLevel {
  ELEmergency,
  ELAlert,
  ELCritical,
  ELError,
  ELWarning,
  ELNotice,
  ELInfo,
  ELDebug 
};

extern int Verbose;	// This should be here, not in config.h

ACCESS void Display(int, enum tErrorLevel, char *, char *, ...);
ACCESS int  LogInit(char *, char *);
ACCESS void LogClose(void);
ACCESS int  LogPrintf(enum tErrorLevel ErrorLevel, char *FunctionName, char *Format, ...);
ACCESS int vLogPrintf(enum tErrorLevel ErrorLevel, char *FunctionName, char *Format, va_list argp);

#undef ACCESS
#endif

/*----- log.h ----------------------------------------------------------------------------------*/
