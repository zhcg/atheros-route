/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#include "sw.h"
#include "sw_ioctl.h"
#include "fal_led.h"
#include "fal_uk_if.h"


sw_error_t
fal_led_ctrl_pattern_set(a_uint32_t dev_id, led_pattern_group_t group,
                      led_pattern_id_t id, led_ctrl_pattern_t * pattern)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_LED_PATTERN_SET, dev_id, (a_uint32_t)group,
                        (a_uint32_t)id, (a_uint32_t)pattern);
    return rv;
}

sw_error_t
fal_led_ctrl_pattern_get(a_uint32_t dev_id, led_pattern_group_t group,
                      led_pattern_id_t id, led_ctrl_pattern_t * pattern)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_LED_PATTERN_GET, dev_id, (a_uint32_t)group,
                        (a_uint32_t)id, (a_uint32_t)pattern);
    return rv;
}
