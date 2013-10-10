/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#include "sw.h"
#include "sw_ioctl.h"
#include "fal_sec.h"
#include "fal_uk_if.h"

sw_error_t
fal_sec_norm_item_set(a_uint32_t dev_id, fal_norm_item_t item, void * value)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_SEC_NORM_SET, dev_id, item, (a_uint32_t) value);
    return rv;
}

sw_error_t
fal_sec_norm_item_get(a_uint32_t dev_id, fal_norm_item_t item, void * value)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_SEC_NORM_GET, dev_id, item, (a_uint32_t) value);
    return rv;
}

