/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup fal_mib FAL_MIB
 * @{
 */ 

#include "sw.h"
#include "fal_mib.h"
#include "hsl_api.h"


static sw_error_t
_fal_get_mib_info(a_uint32_t dev_id, fal_port_t port_id,
                 fal_mib_info_t * mib_Info)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->get_mib_info)
        return SW_NOT_SUPPORTED;

    rv = p_api->get_mib_info(dev_id, port_id, mib_Info);
    return rv;
}


static sw_error_t
_fal_mib_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->mib_status_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->mib_status_set(dev_id, enable);
    return rv;
}


static sw_error_t
_fal_mib_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->mib_status_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->mib_status_get(dev_id, enable);
    return rv;
}
/**
 * @brief Get mib infomation on particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] mib_info mib infomation
 * @return SW_OK or error code
 */
sw_error_t
fal_get_mib_info(a_uint32_t dev_id, fal_port_t port_id,
                 fal_mib_info_t * mib_Info)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_get_mib_info(dev_id, port_id, mib_Info);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set mib status on a particular device.
 * @param[in] dev_id device id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_mib_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_mib_status_set(dev_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get mib status on a particular device.
 * @param[in] dev_id device id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_mib_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_mib_status_get(dev_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @}
 */
