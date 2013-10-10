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


#include <config.h>
#include <dv_dbg.h>
//#include <version.h>
#include <asm/regdef.h>
#include <asm/mipsregs.h>

	.set noreorder
	.align 4

	.globl	_start
	.type	_start, @function
	.text
_start:
	la	t0,	clear_bss
	j	t0
	nop

	.word	boot_end_data
	.word	boot_end

clear_bss:
	lw	t1,	-8(t0)	/* t1 <-- boot_end_data	*/
	lw	t2,	-4(t0)	/* t2 <-- boot_end */

	sub	t1,	4
1:	addi	t1,	4
	bltl	t1,	t2,	1b
	sw	zero,	0(t1)	/* delay slot */

	li	t0,	CFG_SRAM_BASE + CFG_INIT_SP_OFFSET
	la	sp,	0(t0)

	la	t9,	main
	j	t9
	nop

