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
 *	Copyright (C) 2002 Antony Kholodkov
 *
 */

#ifndef _CTLFILE_H_
#define _CTLFILE_H_

#include <sys/types.h>

int parse_ctl_file(char *filename);

void cleanup_ctl_file();

int get_param_i(char *section, char *key, int def_value);

int get_param_b(char *section, char *key, int def_value);

char *get_param_a(char *section, char *key, char *def_value, char *buffer, size_t max_size);

void for_each_key(char *section, char *key, void(*proc)(char *value));

void for_each_section(void(*proc)(char *sectionname));


#endif // _CTLFILE_H_
