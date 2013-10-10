/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#include "sw.h"
#include "sw_ioctl.h"
#include "ssdk_init.h"
#include "fal_init.h"
#include "fal_uk_if.h"

sw_error_t
fal_reset(a_uint32_t dev_id)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_SWITCH_RESET, dev_id);
    return rv;
}

sw_error_t
fal_ssdk_cfg(a_uint32_t dev_id, ssdk_cfg_t *ssdk_cfg)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_SSDK_CFG, dev_id, ssdk_cfg);
    return rv;
}
