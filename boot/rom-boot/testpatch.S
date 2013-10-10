	.globl patchmain
	.text
patchmain:
	addiu	$sp,	-4	/* space for $ra */
	sw	$ra,	0($sp)

// Wasp 1.2
// _A_wasp_indirection_table_t is at 0xbd000000
// _A_wasp_indirection_table_t->nand->_nand_read is at 0xbd000138
#define wasp_indir_tbl		0xbd000000
#define nand_read_off		0x138
#define addr_of_udelay		0xbfc047a8
#define addr_of_nand_init	0xbfc058c4
#define MAX_STATUS_TRY		1000
#define MAX_IO_TRY		3

	li	$t0,	wasp_indir_tbl
	la	$t1,	ath_ecc_nand_read
	sw	$t1,	nand_read_off($t0)

// RST_WATCHDOG_TIMER_CONTROL_ADDRESS
	li	$t0,	0xb8060000

// Watchdog action none
	sw	$zero,	8($t0)

// 3 seconds for 40 MHz refclk and slightly longer on 25 MHz
	li	$t1,	3 * (40 * 1000 * 1000)
	sw	$t1,	12($t0)

// Watchdog action reset
	li	$t1,	0x3
	sw	$t1,	8($t0)

#define CPU_PLL_CONFIG_ADDRESS		0xb8050000
#define SWITCH_CLOCK_SPARE_OFFSET	0x0024

	li	$t0,	CPU_PLL_CONFIG_ADDRESS
	li	$t1,	0x530
	sw	$t1,	SWITCH_CLOCK_SPARE_OFFSET($t0)

#define RST_BASE			0xb8060000
#define RST_RESET_OFFSET		0x1c

#define RST_RESET_USB_PHY_ARESET_MASK	0x00000800
	li	$t0,	RST_BASE
	lw	$t1,	RST_RESET_OFFSET($t0)
	ori	$t1,	$t1,	RST_RESET_USB_PHY_ARESET_MASK
	sw	$t1,	RST_RESET_OFFSET($t0)

#define RST_RESET_USB_PHY_PLL_PWD_EXT_LSB	15
#define RST_RESET_USB_PHY_RESET_LSB		4
#define RST_RESET_USB_PHY_ARESET_LSB		11
#define RST_RESET_USB_HOST_RESET_LSB		5

	ins	$t1,	$zero,	RST_RESET_USB_PHY_PLL_PWD_EXT_LSB, 1
	sw	$t1,	RST_RESET_OFFSET($t0)

	ins	$t1,	$zero,	RST_RESET_USB_PHY_RESET_LSB, 1
	sw	$t1,	RST_RESET_OFFSET($t0)

	ins	$t1,	$zero,	RST_RESET_USB_PHY_ARESET_LSB, 1
	sw	$t1,	RST_RESET_OFFSET($t0)

	ins	$t1,	$zero,	RST_RESET_USB_HOST_RESET_LSB, 1
	sw	$t1,	RST_RESET_OFFSET($t0)

	lw	$ra,	0($sp)
	addiu	$sp,	4
	jr	$ra
2:
	j	2b


// int
// nand_read(unsigned nand_addr, unsigned count, unsigned buf)
	.globl	ath_ecc_nand_read
	.type	ath_ecc_nand_read, @function
	.text
ath_ecc_nand_read:
// {
#define space_num_regs(x)	(x * 4)	// 4 bytes per register
#define num_sav_regs		6	// no. of regs to save

// save regs
	addiu	$sp,	-space_num_regs(num_sav_regs)
	sw	$ra,	0($sp)
	sw	$a0,	4($sp)
	sw	$a1,	8($sp)
	li	$t1,	0x1fffffff	// virt to phys of buf
	and	$a2,	$a2,	$t1	// and save that to
	sw	$a2,	12($sp)		// stack
	sw	$s0,	16($sp)
	sw	$s1,	20($sp)

	ori	$s0,	$zero,	MAX_IO_TRY	// no. of retries
	li	$s1,	0xbb000000	// AR7240_NAND_FLASH_BASE

rd_cmd_retry:
	sw	$a0,	0x21c($s1)	// AR7240_NF_ADDR0_0
	sw	$zero,	0x224($s1)	// AR7240_NF_ADDR0_1
	sw	$a2,	0x264($s1)	// AR7240_NF_DMA_ADDR
	sw	$a1,	0x268($s1)	// AR7240_NF_DMA_COUNT
	sw	$a1,	0x284($s1)	// AR7240_NF_PG_SIZE

	li	$t1,	0xcc
	sw	$t1,	0x26c($s1)	// AR7240_NF_DMA_CTRL

	lw	$t1,	0x204($s1)	// AR7240_NF_CTRL
	ori	$t1,	(1 <<  5)	// Enable ECC
	sw	$t1,	0x204($s1)	// AR7240_NF_CTRL

	li	$t1,	(4 << 8) | (1 << 5)	// 4 - bit ECC
	sw	$t1,	0x214($s1)	// ATH_NF_ECC_CTRL

	li	$t1,	2048 + 20
	sw	$t1,	0x218($s1)	// ATH_NF_ECC_OFFSET

	sw	$zero,	0x210($s1)	// AR7240_NF_INT_STATUS

	li	$t1,	(1 << 1)	// Enable cmd end intr
	sw	$t1,	0x20c($s1)	// AR7240_NF_INT_MASK

	li	$t1,	0x30006a	// Read Page command
	sw	$t1,	0x200($s1)	// AR7240_NF_CMD

rd_cmd_end_intr_loop:	// while (ath_nand_get_cmd_end_status() == 0);
	lw	$t1,	0x210($s1)	// AR7240_NF_INT_STATUS
	andi	$t1,	$t1,	(1 << 1)	// check for cmd end intr
	beq	$zero,	$t1,	rd_cmd_end_intr_loop

	li	$a0,	100
	li	$t1,	addr_of_udelay
	jalr	$t1

	la	$t1,	ath_ecc_nand_status
	jalr	$t1

	li	$t1,	0xc0		// READ_STATUS_OK
	or	$t0,	$v0,	$zero
	andi	$v0,	$v0,	0xc7	// READ_STATUS_MASK
	beq	$t1,	$v0,	rd_out	// success, so exit

	lw	$a0,	4($sp)		// restore our arg values
	lw	$a1,	8($sp)
	lw	$a2,	12($sp)

	// udelay() and nand_status() would have saved
	// and restored $s0 and $s1

	addi	$s0,	$s0,	-1

	bne	$s0,	$zero,	rd_cmd_retry

rd_out:
	or	$v0,	$t0,	$zero
// restore regs (a0, a1, a2 can be ignored)
	lw	$ra,	0($sp)
	lw	$s0,	16($sp)
	lw	$s1,	20($sp)
	addiu	$sp,	space_num_regs(num_sav_regs)

// return
	jr	$ra
// }

	.globl	ath_ecc_nand_status
	.type	ath_ecc_nand_status, @function
	.text
ath_ecc_nand_status:
	addiu	$sp,	-4
	sw	$ra,	0($sp)

	li	$t3,	MAX_STATUS_TRY
	li	$t4,	MAX_STATUS_TRY

	li	$t2,	0xff
nf_status_loop:
	lw	$t0,	0x208($s1)	// AR7240_NF_STATUS
	beq	$t0,	$t2,	dma_ctrl_loop

	li	$a0,	25
	li	$t1,	addr_of_udelay
	jalr	$t1

	addi	$t3,	$t3,	-1
	bne	$zero,	$t3,	nf_status_loop

dma_ctrl_loop:
	lw	$t0,	0x26c($s1)	// AR7240_NF_DMA_CTRL
	andi	$t0,	$t0,	1
	bne	$t0,	$zero,	check_status

	li	$a0,	25
	li	$t1,	addr_of_udelay
	jalr	$t1

	addi	$t4,	$t4,	-1
	bne	$zero,	$t4,	dma_ctrl_loop

check_status:
	mul	$t3,	$t3,	$t4
	// $t3 will be zero if either $t3 or $t4 is zero
	// then we have to re-init nand...
	bne	$zero,	$t3,	sts_out1

	li	$t1,	addr_of_nand_init
	jalr	$t1

	li	$v0,	-1
	b	sts_out2		// return -1
sts_out1:
	sw	$zero,	0x210($s1)	// AR7240_NF_INT_STATUS

	li	$t1,	0x7024		// Read Status
	sw	$t1,	0x200($s1)	// AR7240_NF_CMD

sts_cmd_end_intr_loop:	// while (ath_nand_get_cmd_end_status() == 0);
	lw	$t1,	0x210($s1)	// AR7240_NF_INT_STATUS
	andi	$t1,	$t1,	(1 << 1)	// check for cmd end intr
	beq	$zero,	$t1,	sts_cmd_end_intr_loop

	lw	$v0,	0x288($s1)	// AR7240_NF_RD_STATUS
sts_out2:
	lw	$ra,	0($sp)

	addiu	$sp,	4
	jr	$ra
