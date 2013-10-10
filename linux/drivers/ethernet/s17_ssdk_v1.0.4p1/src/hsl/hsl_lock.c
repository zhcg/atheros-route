/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#include "sw.h"

aos_lock_t sw_hsl_api_lock = aos_default_unlock;

sw_error_t
hsl_api_lock_init(void) {
    aos_lock_init(&sw_hsl_api_lock);
    return SW_OK;
}
