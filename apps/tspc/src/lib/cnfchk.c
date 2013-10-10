/*
---------------------------------------------------------------------------
 cnfchk.c,v 1.3 2004/07/14 16:41:10 dgregoire Exp
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
#include <unistd.h>
#include <memory.h>

#include "platform.h"

#include "cnfchk.h"
#include "config.h"
#include "log.h"

char *template_mappings[] = {	"freebsd44", "freebsd", "freebsd4", "freebsd", "windows2000", "windows",
								"windows2003", "windows", "windowsXP", "windows",
								"solaris8", "solaris", NULL };

static int readfixwrite(char *, char *, char *, char *);

/* This next function has to return -1 in any case - it exits the client and 
   does nothing else. 
   */

int 
tspFixConfig(void) {
	tConf t;
	char **tm = template_mappings;
	int worked = 0;

	Display(0, ELNotice, "tspFixConfig", "Verifying and fixing the config file...\n");

	memset(&t, 0, sizeof(tConf));
	if (tspReadConfigFile(FileName, &t) != 0) {
		Display(0, ELError, "tspFixConfig", "Unable to read the config file: %s\n", FileName);
		return -1;
	}

	if (t.template == NULL) {
		Display(0, ELError, "tspFixConfig", "Unable to read the template name from the config file!\nIs it specified? Do you have %s in the current directory?", FileName);
		return -1;
	}

	/* fix template names from older version */

	while (*tm != NULL) {
		//Display(0, ELInfo, "tspFixConfig", "Checkout out %s, config is %s", *tm, t.template);

		if (strcmp(t.template, *tm) == 0) {
			Display(0, ELInfo, "tspFixConfig", "Wrong template name in %s : updating %s -> %s", FileName, *tm, *(tm+1));
			/* needs a replacement in the config file right here.
			   rename the config file to .old and recreate one
			   with the mapped template name.
			   */
			readfixwrite(FileName, "tspc.conf.new", "template=", *(tm+1));
			worked = 1;
		}
		tm+=2;
	}

	if (worked == 0) 
		Display(0, ELInfo, "tspFixConfig", "Nothing was done! Your config file appears to be valid");

	return -1;
}

static
int readfixwrite(char *in, char *out, char *pname, char *pvalue) {

	FILE *f_in, *f_out;
	char *line;

	line = (char *)malloc(256);

	if ( (f_in = fopen(in, "r")) == NULL)
		return 1;
	if ( (f_out = fopen(out, "w")) == NULL)
		return 1;

	while (fgets(line, 256, f_in) != NULL) {
		char buf[256];
		strncpy(buf, line, strlen(pname));
		if (strcmp(buf, pname) == 0) {
			memset(buf, 0, sizeof(buf));
			strcat(buf, pname);
			strcat(buf, pvalue);
			strcat(buf, "\n");
			fputs(buf, f_out);
		}
		else fputs(line, f_out);
	}

	fclose(f_in);
	fclose(f_out);

	unlink(in);
	rename(out, in);

	return 0;
}
