#include <config.h>
#include <dv_dbg.h>
#include <version.h>
#include <asm/regdef.h>
#include <asm/mipsregs.h>
#include <asm/addrspace.h>
#include <ar7240_soc.h>
#include <apb_map.h>

.globl lowlevel_init
	.type   lowlevel_init, @function

lowlevel_init:

	/*
	 * The code below is for the real chip. Wont work on FPGA
	 */
	/* jr ra  */
	DV_DBG_RECORD_LOCATION_ASM(LOWLEVEL_INIT_S) // Location Pointer

#ifdef ROM_BUILD

	li	t0,	KSEG1ADDR(0x18116c40)
	li	t1,	0x633c8176
	sw	t1,	0(t0)

	li	t0,	KSEG1ADDR(0x18116c44)
	li	t1,	0x10380000
	sw	t1,	0(t0)

	li	t5,	KSEG1ADDR(WASP_BOOTSTRAP_REG)
	lw	t3,	0(t5)
	andi	t3,	t3,	WASP_REF_CLK_25

	/* Nothing to do for 40MHz ref clk */
	bne	t3,	zero,	load_40_val
	nop
load_25_val:
	li	t4,	DDR_PLL_25_INIT_VAL
	li	t5,	SWITCH_CLOCK_SPARE_25_INIT_VAL
	j	1f
	nop

load_40_val:
	li	t4,	DDR_PLL_40_INIT_VAL
	li	t5,	SWITCH_CLOCK_SPARE_40_INIT_VAL

1:
	/* Setup DDR PLL */
	DV_DBG_RECORD_LOCATION_ASM(LOWLEVEL_INIT_S) // Location Pointer

	li	t0,	KSEG1ADDR(DDR_PLL_CONFIG_ADDRESS)
	lw	t1,	0(t0)
	li	t2,	DDR_PLL_INIT_MASK
	and	t1,	t1,	t2
	/* t4 set with values above */
	or	t1,	t1,	t4
	sw	t1,	0(t0)

ddr_pll_updating:
	lw	t1,	0(t0)
	li	t2,	DDR_PLL_CONFIG_UPDATING_MASK
	and	t2,	t2,	t1
	bne	t2,	zero,	ddr_pll_updating
	nop

	DV_DBG_RECORD_LOCATION_ASM(LOWLEVEL_INIT_S) // Location Pointer
cpu_ddr_clock_control:
	li	t0,	KSEG1ADDR(CPU_DDR_CLOCK_CONTROL_ADDRESS)
	lw	t1,	0(t0)
	li	t2,	CPU_DDR_CLOCK_CONTROL_INIT_MASK
	and	t1,	t1,	t2
	li	t2,	CPU_DDR_CLOCK_CONTROL_INIT_VAL
	or	t1,	t1,	t2
	sw	t1,	0(t0)

set_switch_clock:
	li	t0,	KSEG1ADDR(SWITCH_CLOCK_SPARE_ADDRESS)
	lw	t1,	0(t0)
	li	t2,	SWITCH_CLOCK_SPARE_INIT_MASK
	and	t1,	t1,	t2
	/* t5 set with values above */
	or	t1,	t1,	t5
	sw	t1,	0(t0)
#endif
	j	ra
	nop
