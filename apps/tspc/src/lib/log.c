/*
---------------------------------------------------------------------------
 log.c,v 1.10 2004/03/29 22:12:22 jpdionne Exp
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
#include <stdio.h>
#include <stdarg.h>

#include <time.h>
#include <string.h>

#define _USES_SYSLOG_H_

#include "platform.h"

#define LOG_IT
#define TEST 0

#include "log.h"

static FILE *Logfp;
static char *Ident;

static int prio[] = {
  LOG_EMERG,
  LOG_ALERT,
  LOG_CRIT,
  LOG_ERR,
  LOG_WARNING,
  LOG_NOTICE,
  LOG_INFO,
  LOG_DEBUG
};


void Display(int VerboseLevel, enum tErrorLevel el, char *func, char *format, ...) {

        va_list     argp;
        int i, j;
        char fmt[5000];
        char clean[5000];

        va_start(argp, format);
        vsnprintf(fmt, sizeof fmt, format, argp);
        va_end(argp);

        /* Change CRLF to LF for log output */

        for (i = 0, j = 0; i < sizeof fmt; i++) {
                if (fmt[i] == '\r' && fmt[i + 1] == '\n')
                        continue;
                clean[j++] = fmt[i];
                if (fmt[i] == '\0')
                        break;
        }

  /*
     output if verbose on
  */
        if(Verbose >= VerboseLevel) {
                printf("%s\n", clean);
				LogPrintf(el, func, clean);
        }
        
}


int LogInit (char *Identity, char *Filename)
{
  if ((Filename != NULL) && (strlen(Filename) > 0)) {

    if((Logfp = fopen(Filename, "a")) == NULL) {
      fprintf(stderr, "%s: LogInit: Cannot open log file %s\n", Identity, Filename);
      return 1;
    }
  } else
	  Logfp = NULL;

  Ident = Identity;
  OPENLOG(Identity, 0, 0);
  return 0;
}

void LogClose(void)
{
  if (Logfp != NULL) {
    fclose(Logfp);
    Logfp = NULL;
  }

  CLOSELOG();
}

int LogFPrintf(enum tErrorLevel ErrorLevel, char *FunctionName, char *Format, ...)
{
  va_list     argp;
  time_t      t;
  struct  tm *tm;

  va_start(argp, Format);

  if (Logfp != NULL) {

    t = time(NULL);
    tm = localtime(&t);

    fprintf(Logfp, 
	    "%04d/%02d/%02d %02d:%02d:%02d %s: %s: ",
	    (tm->tm_year+1900), 
	    (tm->tm_mon+1), 
	    tm->tm_mday,
	    tm->tm_hour, 
	    tm->tm_min, 
	    tm->tm_sec,
            Ident == NULL ? "" : Ident,
	    FunctionName == NULL ? "" : FunctionName);

    vfprintf(Logfp, Format, argp);
    fputs("\n", Logfp);
    fflush(Logfp);
  }

  va_end(argp);
  return 0;
}

int LogPrintf(enum tErrorLevel ErrorLevel, char *FunctionName, char *Format, ...)
{
  va_list     argp;
  time_t      t;
  struct  tm *tm;
  char *s1, *s2;
  int i, j;

  static char str[5000];
  static char fmt[5000];

  va_start(argp, Format);
  vsnprintf(fmt, sizeof fmt, Format, argp);
  va_end(argp);

  if (Logfp != NULL) {

    t = time(NULL);
    tm = localtime(&t);

    fprintf(Logfp, 
	    "%04d/%02d/%02d %02d:%02d:%02d %s: %s: ",
	    (tm->tm_year+1900), 
	    (tm->tm_mon+1), 
	    tm->tm_mday,
	    tm->tm_hour, 
	    tm->tm_min, 
	    tm->tm_sec,
        Ident == NULL ? "" : Ident,
	    FunctionName == NULL ? "" : FunctionName);

	// remove the \r and \n from the logging

	i = strlen(fmt);
	s1 = s2 = malloc(i + 1);
	
	if (s1 == NULL)
		return -1;

	for (j = 0;j < i; j++)
		if (fmt[j] != '\r' && fmt[j] != '\n')
			*s1++ = fmt[j];

	*s1++ = '\0';
    fprintf(Logfp, s2);
	free(s2);

    fputs("\n", Logfp);
    fflush(Logfp);
  }

  snprintf(str, sizeof str,  " %s: %s", FunctionName, fmt);  

  SYSLOG(prio[ErrorLevel], str);
  return 0;
}

int vLogPrintf(enum tErrorLevel ErrorLevel, char *FunctionName, char *Format, va_list argp)
{
  time_t      t;
  struct  tm *tm;

  static char str[5000];
  static char fmt[5000];

  vsnprintf(fmt, sizeof fmt, Format, argp);

  if (Logfp != NULL) {

    t = time(NULL);
    tm = localtime(&t);

    fprintf(Logfp, 
	    "%04d/%02d/%02d %02d:%02d:%02d %s: %s: ",
	    (tm->tm_year+1900), 
	    (tm->tm_mon+1), 
	    tm->tm_mday,
	    tm->tm_hour, 
	    tm->tm_min, 
	    tm->tm_sec,
            Ident == NULL ? "" : Ident,
	    FunctionName == NULL ? "" : FunctionName);

    fprintf(Logfp, fmt);

    fputs("\n", Logfp);
    fflush(Logfp);
  }

  snprintf(str, sizeof str, "%s : %s", FunctionName, Format);  

  SYSLOG(prio[ErrorLevel], str);
  return 0;
}

/*---- log.c ----------------------------------------------------------------*/
