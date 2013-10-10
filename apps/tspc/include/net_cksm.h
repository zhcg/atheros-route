/* in_cksum.h
 * Declaration of  Internet checksum routine.
 *
 * net_cksm.h,v 1.3 2004/02/11 04:52:41 blanchet Exp
 *
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
*/

#ifndef UCHAR
#define UCHAR unsigned char
#endif

#ifndef USHORT
#define USHORT unsigned short int
#endif

#ifndef ULONG
#define ULONG unsigned long int
#endif

typedef struct {
	UCHAR *ptr;
	int len;
} vec_t;

extern USHORT in_cksum(const vec_t *vec, int veclen);
