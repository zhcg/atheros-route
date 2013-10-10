/*
---------------------------------------------------------------------------
 xmlparse.h,v 1.2 2004/02/11 04:52:42 blanchet Exp
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
---------------------------------------------------------------------------
*/
#ifndef XMLPARSE_H
#define XMLPARSE_H

#ifdef XMLPARSE
# define ACCESS
#else
# define ACCESS extern
#endif

/*
 * In the node structure, the following identify how many attributes a node can
 * have max. It can be adjusted at compile time.
 */

#define MAX_ATTRIBUTES 5

/*
 * The following identify the format of a node processing function that must be
 * supplied in the node structure to take care of the information supplied in the
 * structure.
 */

struct stNode;

typedef int processNode(struct stNode *n, char *content);

/*
 * The node structure contains the relevant information of each node to be
 * parsed by each invocation of the parser.
 */

typedef struct stAttribute {
  char *name;
  char *value;
} tAttribute;

typedef struct stNode {
  char *name;
  processNode *p;
  tAttribute attributes[MAX_ATTRIBUTES + 1];
} tNode;

/*
 * The XMLParser function is responsible of parsing the contain of a string,
 * calling the appropriate processNode function when a node token is recognized.
 *
 * Parameters:
 *
 * 
 * Return value:
 *
 *    0 - Success
 *   -1 - Memory allocation error
 *    n - Parsing error at position n in the string
 */

ACCESS int XMLParse(char *str, tNode nodes[]);

/*
 * The following macros are usefull to declare the node structures
 *
 * They assume that the event functions will be named the same as the
 * XML tags, prefixed with p_
 *
 */
 
#define NODE(n) { #n, p_##n, {
#define ENDNODE { "", 0 } } },
#define ATTR(a) { #a, 0 },
#define ENDLIST { "", 0, { { "", 0 } } } };
#define STARTLIST {

#define PROC(pr) int p_##pr (tNode *n, char *content)

#undef ACCESS

#endif
