/*************************************************************************/
/*  Copyright (c) 2008 Atheros Communications, Inc., All Rights Reserved */
/*                                                                       */
/*  Module Name : clock.c                                                */
/*                                                                       */
/*  Abstract                                                             */
/*      This file contains clock code.                                   */
/*                                                                       */
/*  NOTES                                                                */
/*      None                                                             */
/*                                                                       */
/*************************************************************************/


#include <dv_dbg.h>
#include <asm/addrspace.h>
#include <asm/types.h>
#include <config.h>
#include <ar7240_soc.h>
#include "wasp_api.h"
#include "clock_api.h"

u32 MipsTimerCountPerSecond;

/* dummy for module init
 *
 */
LOCAL void
clock_init(void)
{
	u32 bs_reg = ar7240_reg_rd(WASP_BOOTSTRAP_REG);

	if ((bs_reg & WASP_REF_CLK_25) == 0) {
		MipsTimerCountPerSecond = 25 * 1000 * 1000 / 2;
	} else {
		MipsTimerCountPerSecond = 40 * 1000 * 1000 / 2;
	}
}


void clock_module_install(struct clock_api *api)
{
	api->_clock_init = clock_init;
}
