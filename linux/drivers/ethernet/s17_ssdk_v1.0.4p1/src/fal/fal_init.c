/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup fal_init FAL_INIT
 * @{
 */
#include "sw.h"
#include "fal_vlan.h"
#include "hsl.h"
#include "hsl_dev.h"
#include "hsl_api.h"


/**
 * @brief Init fal layer.
 * @details Comments:
 *   This operation will init fal layer and hsl layer
 * @param[in] dev_id device id
 * @param[in] cfg configuration for initialization
 * @return SW_OK or error code
 */
sw_error_t
fal_init(a_uint32_t dev_id, ssdk_init_cfg *cfg)
{
    sw_error_t rv;
    HSL_DEV_ID_CHECK(dev_id);

    rv = hsl_api_init(dev_id);
    SW_RTN_ON_ERROR(rv);    

    rv = hsl_dev_init(dev_id, cfg);
    SW_RTN_ON_ERROR(rv);

#ifdef IN_VLAN
    rv = fal_vlan_init(dev_id);
    SW_RTN_ON_ERROR(rv);
#endif    

    return rv;
}


/**
 * @brief Init fal layer.
 * @details Comments:
 *   This operation will init fal layer and hsl layer
 * @param[in] dev_id device id
 * @param[in] cpu_mode cpu port connecting mode
 * @param[in] reg_mode register access mode
 * @return SW_OK or error code
 */
sw_error_t
fal_reduced_init(a_uint32_t dev_id, hsl_init_mode cpu_mode, hsl_access_mode reg_mode)
{
    sw_error_t rv;
    HSL_DEV_ID_CHECK(dev_id);

    rv = hsl_api_init(dev_id);
    SW_RTN_ON_ERROR(rv);

    rv = hsl_dev_reduced_init(dev_id, cpu_mode, reg_mode);
    SW_RTN_ON_ERROR(rv);

#ifdef IN_VLAN
    rv = fal_vlan_init(dev_id);
    SW_RTN_ON_ERROR(rv);
#endif    

    return rv;
}


static sw_error_t
_fal_reset(a_uint32_t dev_id)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->dev_reset)
        return SW_NOT_SUPPORTED;

#ifdef IN_VLAN
    rv = fal_vlan_reset(dev_id);
    SW_RTN_ON_ERROR(rv);
#endif    

    rv = p_api->dev_reset(dev_id);
    return rv;
}

static sw_error_t
_fal_ssdk_cfg(a_uint32_t dev_id, ssdk_cfg_t *ssdk_cfg)
{    
    sw_error_t rv;
    HSL_DEV_ID_CHECK(dev_id);

    rv = hsl_ssdk_cfg(dev_id, ssdk_cfg);

    return rv;
}

sw_error_t
fal_cleanup(void)
{
    sw_error_t rv;

    rv = hsl_dev_cleanup();
    SW_RTN_ON_ERROR(rv);

#ifdef IN_VLAN
    rv = fal_vlan_cleanup();
    SW_RTN_ON_ERROR(rv);
#endif    

    return SW_OK;
}

/**
 * @brief Reset fal layer.
 * @details Comments:
 *   This operation will reset fal layer and hsl layer
 * @param[in] dev_id device id
 * @return SW_OK or error code
 */
sw_error_t
fal_reset(a_uint32_t dev_id)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_reset(dev_id);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get SSDK config infomation.
 * @param[in] dev_id device id
 * @param[out] ssdk_cfg SSDK config infomation
 * @return SW_OK or error code
 */
sw_error_t
fal_ssdk_cfg(a_uint32_t dev_id, ssdk_cfg_t *ssdk_cfg)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ssdk_cfg(dev_id, ssdk_cfg);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @}
 */
