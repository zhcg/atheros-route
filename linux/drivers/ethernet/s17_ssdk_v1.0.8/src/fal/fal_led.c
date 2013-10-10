/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup fal_led FAL_LED
 * @{
 */ 

#include "sw.h"
#include "fal_led.h"
#include "hsl_api.h"



static sw_error_t
_fal_led_ctrl_pattern_set(a_uint32_t dev_id, led_pattern_group_t group,
                      led_pattern_id_t id, led_ctrl_pattern_t * pattern)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->led_ctrl_pattern_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->led_ctrl_pattern_set(dev_id, group, id, pattern);
    return rv;
}



static sw_error_t
_fal_led_ctrl_pattern_get(a_uint32_t dev_id, led_pattern_group_t group,
                      led_pattern_id_t id, led_ctrl_pattern_t * pattern)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->led_ctrl_pattern_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->led_ctrl_pattern_get(dev_id, group, id, pattern);
    return rv;
}


/**
* @brief Set led control pattern on a particular device.
* @param[in] dev_id device id
* @param[in] group  pattern group, lan or wan
* @param[in] id pattern id
* @param[in] pattern led control pattern
* @return SW_OK or error code
*/
sw_error_t
fal_led_ctrl_pattern_set(a_uint32_t dev_id, led_pattern_group_t group,
                      led_pattern_id_t id, led_ctrl_pattern_t * pattern)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_led_ctrl_pattern_set(dev_id, group, id, pattern);
    FAL_API_UNLOCK;
    return rv;
}

/**
* @brief Get led control pattern on a particular device.
* @param[in] dev_id device id
* @param[in] group  pattern group, lan or wan
* @param[in] id pattern id
* @param[out] pattern led control pattern
* @return SW_OK or error code
*/
sw_error_t
fal_led_ctrl_pattern_get(a_uint32_t dev_id, led_pattern_group_t group,
                      led_pattern_id_t id, led_ctrl_pattern_t * pattern)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_led_ctrl_pattern_get(dev_id, group, id, pattern);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @}
 */
