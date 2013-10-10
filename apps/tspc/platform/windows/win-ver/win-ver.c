/*
---------------------------------------------------------------------------
 win-ver.c,v 1.9 2004/09/08 17:47:43 dgregoire Exp
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
/*

   UNKNOWN							= 0
   WINDOWS 2000 = Microsoft Windows 2000 			= 3
   WINDOWS XP   = Microsoft Windows XP 				= 2 
   WINDOWS XPSP1= Microsoft Windows XP Service Pack 1		= 1

*/

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

BYTE *version_string[] = {  "Microsoft Windows XP Service Pack 1",
							"Microsoft Windows XP Service Pack 2",
							"Microsoft Windows XP",
							"Microsoft Windows 2000",
							0 };


void
print_usage() {
	printf("win-ver version 1.0 by Hexago\n\n");
	printf("-v get windows version\n");
	printf("\t\tMicrosoft Windows XP Service Pack 1 = 1\n");
	printf("\t\tMicrosoft Windows XP Service Pack 2 = 2\n");
	printf("\t\tMicrosoft Windows XP = 3\n");
	printf("\t\tMicrosoft Windows 2000 = 4\n");
	printf("-i get the tunnel adapter index (not implemented)\n\n");
	printf("%%errorlevel%% contains the answer\n");

}

int
get_windows_version() {
	HKEY key;
	BYTE data[2048];
	DWORD l = 2048;
	SHORT s;
	BYTE **p;
	int i = 1;

	RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &key);

	RegQueryValueEx(key, "ProductName", NULL, NULL, data, &l);
	s = strlen(data);
	l = 2048 - s;

	RegQueryValueEx(key, "CSDVersion", NULL, NULL, data + s + 1, &l);
	data[s] = ' ';

	p = version_string;
	
	while (*p != 0) {
		if (strstr(data, *p) != NULL)
			return i;
	p++, i++;
	}

	return 0;
}

int
get_adapter_index(char *name) {

	/* this should call GetAdaptersAddresses and
	   get the index for a named adapter */

	return 0;
}





int
main(int argc, char *argv[]) {

	if (argc == 1)
		return get_windows_version();

	else if (argc > 2) {
		print_usage();
		return -1;
	}

	else if (strcmp(argv[1], "-v") == 0) 
		return get_windows_version();
	else if (strcmp(argv[1], "-i") == 0)
        return get_adapter_index(NULL);
    
	print_usage();
	return -1;
}


 
