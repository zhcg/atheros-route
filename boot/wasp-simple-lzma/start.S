/*
 *  Startup Code for MIPS32 CPU-core
 *
 *  Copyright (c) 2003	Wolfgang Denk <wd@denx.de>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */


#include <asm/regdef.h>
#include <asm/mipsregs.h>

	.set noreorder
	.align 4
	.globl _start

	.text
_start:
	.align 4

    /* init initial stack */
    la      a0,__init_stack
    move    sp,a0
    nop

    /* clear BSS section */
    la      a0,__bss_start          # start of bss
    la      a1,__bss_end            # end of bss
1:  sb      zero,0(a0)              # zero memory
    addiu   a0,a0,1                 # next addr
    bne     a0,a1,1b                # to next store
    nop

	/* main routine */
	la      t9, main
	j       t9
	nop

    .rept 1024
    .long 0
    .endr
__init_stack:
    
