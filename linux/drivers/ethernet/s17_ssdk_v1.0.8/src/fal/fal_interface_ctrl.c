/*
 * Copyright (c) 2007-2011 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup fal_interface_ctrl FAL_INTERFACE_CONTROL
 * @{
 */  
#include "sw.h"
#include "fal_interface_ctrl.h"
#include "hsl_api.h"

static sw_error_t
_fal_port_3az_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->port_3az_status_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->port_3az_status_set(dev_id, port_id, enable);
    return rv;
}

static sw_error_t
_fal_port_3az_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->port_3az_status_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->port_3az_status_get(dev_id, port_id, enable);
    return rv;
}

static sw_error_t
_fal_interface_mac_mode_set(a_uint32_t dev_id, fal_port_t port_id, fal_mac_config_t * config)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->interface_mac_mode_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->interface_mac_mode_set(dev_id, port_id, config);
    return rv;
}

static sw_error_t
_fal_interface_mac_mode_get(a_uint32_t dev_id, fal_port_t port_id, fal_mac_config_t * config)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->interface_mac_mode_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->interface_mac_mode_get(dev_id, port_id, config);
    return rv;
}

static sw_error_t
_fal_interface_phy_mode_set(a_uint32_t dev_id, a_uint32_t phy_id, fal_phy_config_t * config)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->interface_phy_mode_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->interface_phy_mode_set(dev_id, phy_id, config);
    return rv;
}

static sw_error_t
_fal_interface_phy_mode_get(a_uint32_t dev_id, a_uint32_t phy_id, fal_phy_config_t * config)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->interface_phy_mode_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->interface_phy_mode_get(dev_id, phy_id, config);
    return rv;
}

/**
  * @brief Set 802.3az status on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_port_3az_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_port_3az_status_set(dev_id, port_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
  * @brief Get 802.3az status on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_port_3az_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_port_3az_status_get(dev_id, port_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
  * @brief Set interface mode on a particular MAC device.
 * @param[in] dev_id device id
 * @param[in] mca_id MAC device ID
 * @param[in] config interface configuration
 * @return SW_OK or error code
 */
sw_error_t
fal_interface_mac_mode_set(a_uint32_t dev_id, fal_port_t port_id, fal_mac_config_t * config)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_interface_mac_mode_set(dev_id, port_id, config);
    FAL_API_UNLOCK;
    return rv;
}

/**
  * @brief Get interface mode on a particular MAC device.
 * @param[in] dev_id device id
 * @param[in] mca_id MAC device ID
 * @param[out] config interface configuration
 * @return SW_OK or error code
 */
sw_error_t
fal_interface_mac_mode_get(a_uint32_t dev_id, fal_port_t port_id, fal_mac_config_t * config)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_interface_mac_mode_get(dev_id, port_id, config);
    FAL_API_UNLOCK;
    return rv;
}

/**
  * @brief Set interface phy mode on a particular PHY device.
 * @param[in] dev_id device id
 * @param[in] phy_id PHY device ID
 * @param[in] config interface configuration
 * @return SW_OK or error code
 */
sw_error_t
fal_interface_phy_mode_set(a_uint32_t dev_id, a_uint32_t phy_id, fal_phy_config_t * config)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_interface_phy_mode_set(dev_id, phy_id, config);
    FAL_API_UNLOCK;
    return rv;
}

/**
  * @brief Get interface phy mode on a particular PHY device.
 * @param[in] dev_id device id
 * @param[in] phy_id PHY device ID
 * @param[out] config interface configuration
 * @return SW_OK or error code
 */
sw_error_t
fal_interface_phy_mode_get(a_uint32_t dev_id, a_uint32_t phy_id, fal_phy_config_t * config)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_interface_phy_mode_get(dev_id, phy_id, config);
    FAL_API_UNLOCK;
    return rv;
}


/**
 * @}
 */

