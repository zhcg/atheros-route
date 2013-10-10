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

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "ctlfile.h"

#define BUF_SIZE_CTL 200
#define SECTION_HEAD '['
#define SECTION_TRAILER	']'

typedef struct node {
    struct node *next;
    char *name;
    char *value;
} node_t;

typedef struct section {
    struct section* next;
    node_t* node;
    char* name;
} section_t;

section_t *root_node = NULL;

char *strdup2(char *str) 
{
    if (str) 
    { 
	return (char*)strdup(str);
    } 
    else 
    {
	return (char*)strdup("");
    }	
};

node_t *seek_node(char *section, char *node) 
{
    section_t* cur_section = NULL;
    node_t* cur_node = NULL ;
    
    if (root_node == NULL) 
    { 
	return NULL;
    }	
    
    for(cur_section = root_node; cur_section != NULL; cur_section = cur_section->next) 
    {
	if (strcmp(cur_section->name,section) == 0) 
	{
	    for(cur_node = cur_section->node; cur_node != NULL; cur_node = (node_t*)cur_node->next) 
	    {
		if (strcmp(cur_node->name,node) == 0) 
		{
		    return cur_node;
		}
	    }
	}
    }
    
    return NULL;
}

int parse_ctl_file(char *filename) 
{
    FILE *f;
    char buffer[BUF_SIZE_CTL];
    section_t *cur_section = NULL;
    section_t *new_section = NULL;
    node_t* cur_node = NULL ;
    node_t* new_node = NULL ;
    char* sp;
    
    if (root_node == NULL) 
    {
	cur_section = malloc(sizeof(section_t));
	cur_section->next = NULL;
	cur_section->node = NULL;
	cur_section->name = strdup2("root");
	root_node = cur_section;
    }
    
    if ((f = fopen(filename,"r")) == NULL) 
    {
	return 0;	
    }
        
    while (!feof(f)) 
    {
	fgets(buffer, BUF_SIZE_CTL, f);
	
	sp = buffer;
	sp += strspn(buffer, " \t\n");

	if (strlen(sp) == 0) continue;
	
	if (*sp == '#') continue;
	
	if (*sp == SECTION_HEAD) 
	{
	    sp += strspn(sp,"[");
	    sp = (char*)strtok(sp,"]\r\n");
	    new_section = malloc(sizeof(section_t));
	    new_section->next = NULL;
	    new_section->node = NULL;
	    new_section->name = strdup2(sp);
	    cur_section->next = new_section;
	    cur_section = new_section;
	    cur_node = NULL;
	} 
	else 
	{
	    sp = (char*)strtok(sp,"=");
	    new_node = malloc(sizeof(node_t));
	    new_node->next = NULL;
	    new_node->name = strdup2(sp);
	    sp = (char*)strtok(NULL,"\r\n");
	    new_node->value = strdup2(sp);
	    if (cur_node == NULL) 
	    {
		cur_section->node = new_node;
	    } 
	    else 
	    {
		cur_node->next = new_node;
	    }
	    cur_node = new_node;	
	}    
    }
    fclose(f);
    return 1;
};

int get_param_b(char* section, char* key, int def_value) 
{
    node_t *cur_node;
    
    cur_node = seek_node(section,key);
    if (cur_node == NULL) return def_value;
    
    if (strlen(cur_node->value)) 
    {
	switch (cur_node->value[0]) 
	{
	    case 'y':
	    case 'Y':
	    case '1': 
		return 1;
	}
	return 0;
    } 
    else 
    {
	return 0;
    }
}

int get_param_i(char* section, char* key, int def_value) {
    node_t *cur_node;
    
    cur_node = seek_node(section,key);
    
    if (cur_node == NULL) 
	return def_value;
    
    return atoi(cur_node->value);
};

char *get_param_a(char* section, char* key, char* def_value, char* buffer, size_t max_size) 
{
    node_t *cur_node;
    
    cur_node = seek_node(section,key);
    if (cur_node) 
    {
	strncpy(buffer,cur_node->value,max_size);
    } else 
    {
	strncpy(buffer,def_value,max_size);
    };
    
    return buffer;
};

void for_each_key(char *section, char *key, void(*proc)(char *value)) 
{
    node_t *cur_node = seek_node(section,key);
    
    if (cur_node == NULL) 
	return;
    
    do 
    {
	if (strcmp(cur_node->name,key) == 0)
	    proc(cur_node->value);
    } while ((cur_node = cur_node->next) != NULL);
}

void for_each_section(void(*proc)(char *sectionname)) 
{
    section_t *cur_section;
    
    for(cur_section = root_node; cur_section != NULL; cur_section = cur_section->next) 
    {
	proc(cur_section->name);
    }
}

