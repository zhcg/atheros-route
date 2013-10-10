/*
 * arch/mips/boot/compressed/head.S
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994, 1995 Waldorf Electronics
 * Written by Ralf Baechle and Andreas Busse
 * Copyright (C) 1995 - 1999 Ralf Baechle
 * Copyright (C) 1996 Paul M. Antoine
 * Modified for DECStation and hence R3000 support by Paul M. Antoine
 * Further modifications by David S. Miller and Harald Koerfgen
 * Copyright (C) 1999 Silicon Graphics, Inc.
 *
 * Head.S contains the MIPS exception handler and startup code.
 *
 **************************************************************************
 *  9 Nov, 2000.
 *  Added Cache Error exception handler and SBDDP EJTAG debug exception.
 *
 *  Kevin Kissell, kevink@mips.com and Carsten Langgaard, carstenl@mips.com
 *  Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 **************************************************************************
 * 08/2002 modified for zImage boot from ROM
 * Johannes Stezenbach <js@convergence.de>
 */



#include <asm/asm.h>
#include <asm/regdef.h>

#define IndexInvalidate_I       0x00

	.set noreorder
	.cprestore
	LEAF(zstartup)
zstartup:

        la      sp, .stack
	move	s0, a0
	move	s1, a1
	move	s2, a2
	move	s3, a3

	/* Clear BSS */
	/* Note: when zImage is in ROM, _edata and _bss point to
	 * ROM space even when using -Tbss on the linker command line;
	 * maybe ld.script needs to be corrected.
	 */
	la	a0, .stack
	la	a2, _end
1:	sw	zero, 0(a0)
	bne	a2, a0, 1b
	addu	a0, 4

	/* flush the I-Cache */
	li	k0, 0x80000000  # start address
	li	k1, 0x80002000  # end address (8KB I-Cache)
	subu	k1, 128

2:
	.set mips3
	cache	IndexInvalidate_I, 0(k0)
	cache	IndexInvalidate_I, 32(k0)
	cache	IndexInvalidate_I, 64(k0)
	cache	IndexInvalidate_I, 96(k0)
	.set mips0

	bne	k0, k1, 2b
	addu	k0, k0, 128
	/* done */

	la	ra, 3f
	la	k0, decompress_kernel
	jr	k0
	nop
3:

	move	a0, s0
	move	a1, s1
	move	a2, s2
	move	a3, s3
	li	k0, KERNEL_ENTRY
	jr	k0
	nop
4:
	b 4b
	END(zstartup)

	.bss
	.fill 0x2000
	EXPORT(.stack)
