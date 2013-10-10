/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#include "sw.h"
#include "hsl_api.h"

static hsl_api_t hsl_api_table[SW_MAX_NR_DEV];

hsl_api_t *
hsl_api_ptr_get(a_uint32_t dev_id)
{
    if (dev_id >= SW_MAX_NR_DEV)
        return NULL;

    return &(hsl_api_table[dev_id]);
}

sw_error_t
hsl_api_init(a_uint32_t dev_id)
{
    if (SW_MAX_NR_DEV <= dev_id) {
        return SW_BAD_PARAM;
    }

    aos_mem_set(&hsl_api_table[dev_id], 0, sizeof (hsl_api_t));
    return SW_OK;
}

