/*
 * Copyright (c) 2007-2009 Atheros Communications, Inc.
 * All rights reserved.
 *
 */ 
#ifndef _SHIVA_LED_H_
#define _SHIVA_LED_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "fal/fal_led.h"

sw_error_t
shiva_led_init(a_uint32_t dev_id);

#ifdef IN_LED
    #define SHIVA_LED_INIT(rv, dev_id) \
    { \
        rv = shiva_led_init(dev_id); \
        SW_RTN_ON_ERROR(rv); \
    }
#else
    #define SHIVA_LED_INIT(rv, dev_id)
#endif

#ifdef HSL_STANDALONG

HSL_LOCAL sw_error_t
shiva_led_ctrl_pattern_set(a_uint32_t dev_id, led_pattern_group_t group,
                      led_pattern_id_t id, led_ctrl_pattern_t * pattern);


HSL_LOCAL sw_error_t
shiva_led_ctrl_pattern_get(a_uint32_t dev_id, led_pattern_group_t group,
                      led_pattern_id_t id, led_ctrl_pattern_t * pattern);

#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SHIVA_LED_H_ */
