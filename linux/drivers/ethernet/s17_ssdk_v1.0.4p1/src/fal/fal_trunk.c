/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup fal_trunk FAL_TRUNK
 * @{
 */  
#include "sw.h"
#include "fal_trunk.h"
#include "hsl_api.h"

static sw_error_t
_fal_trunk_group_set(a_uint32_t dev_id, a_uint32_t trunk_id,
                     a_bool_t enable, fal_pbmp_t member)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->trunk_group_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->trunk_group_set(dev_id, trunk_id, enable, member);
    return rv;
}

static sw_error_t
_fal_trunk_group_get(a_uint32_t dev_id, a_uint32_t trunk_id,
                     a_bool_t * enable, fal_pbmp_t * member)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->trunk_group_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->trunk_group_get(dev_id, trunk_id, enable, member);
    return rv;
}

static sw_error_t
_fal_trunk_hash_mode_set(a_uint32_t dev_id, a_uint32_t hash_mode)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->trunk_hash_mode_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->trunk_hash_mode_set(dev_id, hash_mode);
    return rv;
}

static sw_error_t
_fal_trunk_hash_mode_get(a_uint32_t dev_id, a_uint32_t * hash_mode)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->trunk_hash_mode_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->trunk_hash_mode_get(dev_id, hash_mode);
    return rv;
}

/**
 * @brief Set particular trunk group information on particular device.
 * @param[in] dev_id device id
 * @param[in] trunk_id trunk group id
 * @param[in] enable trunk group status, enable or disable
 * @param[in] member port member information
 * @return SW_OK or error code
 */
sw_error_t
fal_trunk_group_set(a_uint32_t dev_id, a_uint32_t trunk_id,
                     a_bool_t enable, fal_pbmp_t member)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_trunk_group_set(dev_id, trunk_id, enable, member);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get particular trunk group information on particular device.
 * @param[in] dev_id device id
 * @param[in] trunk_id trunk group id
 * @param[out] enable trunk group status, enable or disable
 * @param[out] member port member information
 * @return SW_OK or error code
 */
sw_error_t
fal_trunk_group_get(a_uint32_t dev_id, a_uint32_t trunk_id,
                     a_bool_t * enable, fal_pbmp_t * member)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_trunk_group_get(dev_id, trunk_id, enable, member);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set trunk hash mode on particular device.
 * @param[in] dev_id device id
 * @param[in] hash_mode trunk hash mode
 * @return SW_OK or error code
 */
sw_error_t
fal_trunk_hash_mode_set(a_uint32_t dev_id, a_uint32_t hash_mode)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_trunk_hash_mode_set(dev_id, hash_mode);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get trunk hash mode on particular device.
 * @param[in] dev_id device id
 * @param[out] hash_mode trunk hash mode
 * @return SW_OK or error code
 */
sw_error_t
fal_trunk_hash_mode_get(a_uint32_t dev_id, a_uint32_t * hash_mode)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_trunk_hash_mode_get(dev_id, hash_mode);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @}
 */
