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

#define AR7100_SPI_CLOCK	0xbf000004

#define RVECENT(f,n) \
	b f; nop

#define XVECENT(f,bev)	\
	b	f;	\
	li	k0,	bev

	.set noreorder
#ifdef COMPRESSED_UBOOT
	.align 4
#endif

	.globl	_start
	.type	_start, @function
	.text
_start:
	RVECENT(reset,0)	/* U-boot entry point */
	RVECENT(reset,1)	/* software reboot */
	RVECENT(romReserved,2)
	RVECENT(romReserved,3)
	RVECENT(romReserved,4)
	RVECENT(romReserved,5)
	RVECENT(romReserved,6)
	RVECENT(romReserved,7)
	RVECENT(romReserved,8)
	RVECENT(romReserved,9)
	RVECENT(romReserved,10)
	RVECENT(romReserved,11)
	RVECENT(romReserved,12)
	RVECENT(romReserved,13)
	RVECENT(romReserved,14)
	RVECENT(romReserved,15)
	RVECENT(romReserved,16)
	RVECENT(romReserved,17)
	RVECENT(romReserved,18)
	RVECENT(romReserved,19)
	RVECENT(romReserved,20)
	RVECENT(romReserved,21)
	RVECENT(romReserved,22)
	RVECENT(romReserved,23)
	RVECENT(romReserved,24)
	RVECENT(romReserved,25)
	RVECENT(romReserved,26)
	RVECENT(romReserved,27)
	RVECENT(romReserved,28)
	RVECENT(romReserved,29)
	RVECENT(romReserved,30)
	RVECENT(romReserved,31)
	RVECENT(romReserved,32)
	RVECENT(romReserved,33)
	RVECENT(romReserved,34)
	RVECENT(romReserved,35)
	RVECENT(romReserved,36)
	RVECENT(romReserved,37)
	RVECENT(romReserved,38)
	RVECENT(romReserved,39)
	RVECENT(romReserved,40)
	RVECENT(romReserved,41)
	RVECENT(romReserved,42)
	RVECENT(romReserved,43)
	RVECENT(romReserved,44)
	RVECENT(romReserved,45)
	RVECENT(romReserved,46)
	RVECENT(romReserved,47)
	RVECENT(romReserved,48)
	RVECENT(romReserved,49)
	RVECENT(romReserved,50)
	RVECENT(romReserved,51)
	RVECENT(romReserved,52)
	RVECENT(romReserved,53)
	RVECENT(romReserved,54)
	RVECENT(romReserved,55)
	RVECENT(romReserved,56)
	RVECENT(romReserved,57)
	RVECENT(romReserved,58)
	RVECENT(romReserved,59)
	RVECENT(romReserved,60)
	RVECENT(romReserved,61)
	RVECENT(romReserved,62)
	RVECENT(romReserved,63)
	XVECENT(romExcHandle,0x200)	/* bfc00200: R4000 tlbmiss vector */
	RVECENT(romReserved,65)
	RVECENT(romReserved,66)
	RVECENT(romReserved,67)
	RVECENT(romReserved,68)
	RVECENT(romReserved,69)
	RVECENT(romReserved,70)
	RVECENT(romReserved,71)
	RVECENT(romReserved,72)
	RVECENT(romReserved,73)
	RVECENT(romReserved,74)
	RVECENT(romReserved,75)
	RVECENT(romReserved,76)
	RVECENT(romReserved,77)
	RVECENT(romReserved,78)
	RVECENT(romReserved,79)
	XVECENT(romExcHandle,0x280)	/* bfc00280: R4000 xtlbmiss vector */
	RVECENT(romReserved,81)
	RVECENT(romReserved,82)
	RVECENT(romReserved,83)
	RVECENT(romReserved,84)
	RVECENT(romReserved,85)
	RVECENT(romReserved,86)
	RVECENT(romReserved,87)
	RVECENT(romReserved,88)
	RVECENT(romReserved,89)
	RVECENT(romReserved,90)
	RVECENT(romReserved,91)
	RVECENT(romReserved,92)
	RVECENT(romReserved,93)
	RVECENT(romReserved,94)
	RVECENT(romReserved,95)
	XVECENT(romExcHandle,0x300)	/* bfc00300: R4000 cache vector */
	RVECENT(romReserved,97)
	RVECENT(romReserved,98)
	RVECENT(romReserved,99)
	RVECENT(romReserved,100)
	RVECENT(romReserved,101)
	RVECENT(romReserved,102)
	RVECENT(romReserved,103)
	RVECENT(romReserved,104)
	RVECENT(romReserved,105)
	RVECENT(romReserved,106)
	RVECENT(romReserved,107)
	RVECENT(romReserved,108)
	RVECENT(romReserved,109)
	RVECENT(romReserved,110)
	RVECENT(romReserved,111)
	XVECENT(romExcHandle,0x380)	/* bfc00380: R4000 general vector */
	RVECENT(romReserved,113)
	RVECENT(romReserved,114)
	RVECENT(romReserved,115)
	RVECENT(romReserved,116)
	RVECENT(romReserved,116)
	RVECENT(romReserved,118)
	RVECENT(romReserved,119)
	RVECENT(romReserved,120)
	RVECENT(romReserved,121)
	RVECENT(romReserved,122)
	RVECENT(romReserved,123)
	RVECENT(romReserved,124)
	RVECENT(romReserved,125)
	RVECENT(romReserved,126)
	RVECENT(romReserved,127)

	/* We hope there are no more reserved vectors!
	 * 128 * 8 == 1024 == 0x400
	 * so this is address R_VEC+0x400 == 0xbfc00400
	 */
	.align 4
reset:
	/*
	 * From: Padmanabhan V Karthic
	 * Sent: Monday, February 15, 2010 5:03 PM
	 * To: Varadarajan Narayanan; Kavish Seth
	 * Cc: Venkatesh Ramakrishnan; Usha Krishnamurthy
	 * Subject: Need 4 bytes per ROM bank allocated for BIST "magic word"
	 *
	 * Hi,
	 *
	 * As part of chip testing the ROM contents are tested (BIST -
	 * built-in self-test) by computing a checksum of each word
	 * (32-bit for Wasp ROM banks) in the ROM and ensuring the
	 * result is as expected (called "target pattern"). [For RAM
	 * each location is written and read back and verified but
	 * this can't be done for ROMs.]
	 *
	 * We have computed the "target pattern" for a pattern of
	 * ffffffff's and 00000000's as the contents of each ROM. Now
	 * since the RTL is frozen the "target pattern" is frozen.
	 *
	 * When the actual ROM contents are ready, one word in each
	 * bank is intrapolated so as to make the checksum calculation
	 * arrive at the frozen "target pattern."
	 *
	 * For this we would use the last word of the second ROM bank
	 * -- word number 8191 or byte numbers 32764~32767 which is at
	 * memory address 0x1fc0_fffc.
	 *
	 * For the first ROM bank software has to leave aside an
	 * unused word and give Venky or me the location -- word
	 * number X (0 <= X <= 8191) or byte numbers 4X~4X+3 would be
	 * at memory address 0x1fc0_0000 + 4*X (0x1fc0_0000 <= memory
	 * address <= 0x1fc0_7ffc).
	 *
	 * Thanks,
	 * Paddu
	 */

	bal	1f
	nop
	.word	0x00000000	/* See the above e-mail */
	/*
	 * Clearing CP0 registers - This is generally required for the MIPS-24k
	 * core used by Atheros.
	 */
1:
	mtc0	zero,	$0
	mtc0	zero,	$1
	mtc0	zero,	$2
	mtc0	zero,	$3
	mtc0	zero,	$4
	mtc0	zero,	$5
	mtc0	zero,	$6
	mtc0	zero,	$7
	mtc0	zero,	$8
	mtc0	zero,	$9
	mtc0	zero,	$10
	mtc0	zero,	$11
	/*
	 * Set the location of exception vectors to Bootstrap (0xBFC00000 + Offset)
	 *	Offset 0x0	: Reset,NMI
	 *	Offset 0x300	: Cache error
	 *	Offset 0x380	: All others
	 */
	li	t0,	0x10400004
	//li	t0,	0x10000004
	mtc0	t0,	$12
	mtc0	zero,	$13
	mtc0	zero,	$14
	mtc0	zero,	$15
	mtc0	zero,	$16
	mtc0	zero,	$17
	mtc0	zero,	$18
	mtc0	zero,	$19
	mtc0	zero,	$20
	mtc0	zero,	$21
	mtc0	zero,	$22
#if !CONFIG_WASP_EMU
	mtc0	zero,	$23
#endif
	mtc0	zero,	$24
	mtc0	zero,	$25
	mtc0	zero,	$26
	mtc0	zero,	$27
	mtc0	zero,	$28

	/*
	 * Clear watch registers.
	 */

	mtc0	zero,	CP0_WATCHLO
	mtc0	zero,	CP0_WATCHHI

	/* STATUS register */
	mfc0	k0,	CP0_STATUS
	li	k1,	~ST0_IE
	and	k0,	k1
	mtc0	zero,	CP0_CAUSE
	mtc0	k0,	CP0_STATUS

	/* CAUSE register */
	mtc0	zero,	CP0_CAUSE

	/* Init Timer */
	mtc0	zero,	CP0_COUNT
	mtc0	zero,	CP0_COMPARE

	/* CONFIG0 register */
	li	t0,	CONF_CM_UNCACHED
	mtc0	t0,	CP0_CONFIG

#if 0	// RAY: Not required since rom-boot is non-PIC now
	/* Initialize GOT pointer.*/
	bal	1f
	nop
	.word	_GLOBAL_OFFSET_TABLE_
	1:
	move	gp,	ra
	lw	t1,	0(ra)
	move	gp,	t1
#endif

	DV_DBG_RECORD_LOCATION_ASM(START_S) // Location Pointer
	/* Initialize any external memory.
	 */
	la	t9,	lowlevel_init
	jalr	t9
	nop

	la	t0,	rel_start
	j	t0
	nop

rel_start:
	/* Initialize caches...
	 */
	la	t9,	simple_mips_cache_reset
	jalr	t9
	nop

	/* ... and enable them.
	 */
	li	t0,	CONF_CM_CACHABLE_NONCOHERENT
	mtc0	t0,	CP0_CONFIG


	/* Jump to clear BSS
	 */
	la	t0,	clear_bss
	j	t0
	nop

	.word	boot_end_data
	.word	boot_end

clear_bss:

	/* Clear BSS.
	 */
	DV_DBG_RECORD_LOCATION_ASM(START_S) // Location Pointer
	lw	t1,	-8(t0)	/* t1 <-- boot_end_data	*/
	lw	t2,	-4(t0)	/* t2 <-- boot_end */
	//add	t1,	t6	/* adjust pointers */
	//add	t2,	t6

	sub	t1,	4
1:	addi	t1,	4
	bltl	t1,	t2,	1b
	sw	zero,	0(t1)	/* delay slot */

#if defined(ROM_BUILD) || defined(EMUROM_BUILD)
	/* Copy the data segment from ROM to SRAM */
	la	t0, copy_data
	j	t0
	nop

	.word	_rom_end
	.word	_boot_data_start
	.word	_boot_data_end

copy_data:
	/*
	 * Copy the data to SRAM
	 */
	DV_DBG_RECORD_LOCATION_ASM(START_S) // Location Pointer
	lw	t1,	-12(t0)	/* t1 <-- _rom_end */
	lw	t2,	-8(t0)	/* t2 <-- _boot_data_start */
	lw	t3,	-4(t0)	/* t3 <-- _boot_data_end */

copy_loop:
	lw	t4,	0(t1)
	sw	t4,	0(t2)
	addi	t1,	4
	addi	t2,	4
	ble	t2,	t3,	copy_loop
	nop
#endif


// @TODO:
//#define CFG_SRAM_BASE 0xbd000000 (non-cached SRAM) in ar7240.h
//#define CFG_INIT_SP_OFFSET 0x7FF0 (32KB) in waspemu.h

	DV_DBG_RECORD_LOCATION_ASM(START_S) // Location Pointer
	li	t0,	CFG_SRAM_BASE + CFG_INIT_SP_OFFSET
	la	sp,	0(t0)

	la	t9,	main
	j	t9
	nop

/*
 * Cannot trust the first stage f/w. Don't know what registers
 * it saves or clobbers. Save them and call the f/w and restore
 * them on return. PCIe-EP and USB didn't have any problem.
 * However, when called from nand boot-rom, there were issues,
 * hence the paranoia
 *
 * Refer: http://math-atlas.sourceforge.net/devel/assembly/mipsabi32.pdf
 *
 * Only registers $16..$23 and registers $28.$30 are preserved across
 * a function call. Register $28 is not preserved, however, when
 * calling position independent code.
 *
 *
 * void call_fw(arg, func)
 */
#define	reg_space	(13 * 4)	/* 11 regs + 1 signature */

	.globl	call_fw
	.text

call_fw:
	addiu	sp,	(reg_space * -1)	/* space for registers */
	sw	s0,	4(sp)			/* Save them */
	sw	s1,	8(sp)
	sw	s2,	12(sp)
	sw	s3,	16(sp)
	sw	s4,	20(sp)
	sw	s5,	24(sp)
	sw	s6,	28(sp)
	sw	s7,	32(sp)
	sw	s8,	36(sp)
	sw	gp,	40(sp)
	sw	ra,	44(sp)

#define detect_stack_corruption
#ifdef detect_stack_corruption
	lui	t0,	0x7761			/* wasp == 0x77617370 */
	ori	t0,	0x7370

	sw	t0,	0(sp)			/* sign the top of stack */
#endif /* detect_stack_corruption */

	or	t0,	a1,	zero

	and	a0,	a0,	zero
	and	a1,	a0,	zero
	and	a2,	a0,	zero
	and	a3,	a0,	zero

	jalr	t0				/* call the function */
	nop					/* delay slot */

#ifdef detect_stack_corruption
	lw	t1,	0(sp)			/* check the signature */

	lui	t0,	0x7761
	ori	t0,	0x7370

stack_corrupted:
	bne	t1,	t0,	stack_corrupted
	nop					/* delay slot */
#endif /* detect_stack_corruption */

	lw	s0,	4(sp)			/* Restore them */
	lw	s1,	8(sp)
	lw	s2,	12(sp)
	lw	s3,	16(sp)
	lw	s4,	20(sp)
	lw	s5,	24(sp)
	lw	s6,	28(sp)
	lw	s7,	32(sp)
	lw	s8,	36(sp)
	lw	gp,	40(sp)
	lw	ra,	44(sp)

	addiu	sp,	reg_space		/* Unwind stack */

	j	ra				/* return */
	nop					/* delay slot */

	/* Exception handlers.
	 */
romReserved:
	b romReserved

romExcHandle:
#if 1 /* Save 4 CP0 registers and 32 general purpose registers from the lowest address of stack (stack size is 1024 bytes, the last 144 bytes are not likely to be used) */
	li		k1, CFG_SRAM_BASE + CFG_INIT_SP_OFFSET - CFG_STACK_SIZE
	mfc0	k0,	CP0_STATUS
	sw	k0,	0(k1)
	mfc0	k0,	CP0_CAUSE
	sw	k0,	4(k1)
	mfc0	k0,	CP0_EPC
	sw	k0,	8(k1)
	mfc0	k0,	CP0_BADVADDR
	sw	k0,	12(k1)

	sw	$0,	16(k1)
	.set noat
	sw	$1,	20(k1)
	.set at
	sw	$2,	24(k1)
	sw	$3,	28(k1)
	sw	$4,	32(k1)
	sw	$5,	36(k1)
	sw	$6,	40(k1)
	sw	$7,	44(k1)
	sw	$8,	48(k1)
	sw	$9,	52(k1)
	sw	$10,	56(k1)
	sw	$11,	60(k1)
	sw	$12,	64(k1)
	sw	$13,	68(k1)
	sw	$14,	72(k1)
	sw	$15,	76(k1)
	sw	$16,	80(k1)
	sw	$17,	84(k1)
	sw	$18,	88(k1)
	sw	$19,	92(k1)
	sw	$20,	96(k1)
	sw	$21,	100(k1)
	sw	$22,	104(k1)
	sw	$23,	108(k1)
	sw	$24,	112(k1)
	sw	$25,	116(k1)
	sw	$26,	120(k1)
	sw	$27,	124(k1)
	sw	$28,	128(k1)
	sw	$29,	132(k1)
	sw	$30,	136(k1)
	sw	$31,	140(k1)

	la	t9,	exception_handler
	j	t9
#else
	b romExcHandle
#endif

