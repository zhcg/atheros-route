/*
 * Copyright (c) 2007-2009 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
#ifndef _ISIS_INTERFACE_CTRL_H_
#define _ISIS_INTERFACE_CTRL_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "fal/fal_interface_ctrl.h"

sw_error_t isis_interface_ctrl_init(a_uint32_t dev_id);

#ifdef IN_INTERFACECONTROL
    #define ISIS_INTERFACE_CTRL_INIT(rv, dev_id) \
    { \
        rv = isis_interface_ctrl_init(dev_id); \
        SW_RTN_ON_ERROR(rv); \
    }
#else
    #define ISIS_INTERFACE_CTRL_INIT(rv, dev_id)
#endif

#ifdef HSL_STANDALONG

HSL_LOCAL sw_error_t
isis_port_3az_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);

HSL_LOCAL sw_error_t
isis_port_3az_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable);

HSL_LOCAL sw_error_t
isis_interface_mac_mode_set(a_uint32_t dev_id, fal_port_t port_id, fal_mac_config_t * config);


HSL_LOCAL sw_error_t
isis_interface_mac_mode_get(a_uint32_t dev_id, fal_port_t port_id, fal_mac_config_t * config);

HSL_LOCAL sw_error_t
isis_interface_phy_mode_set(a_uint32_t dev_id, a_uint32_t phy_id, fal_phy_config_t * config);

HSL_LOCAL sw_error_t
isis_interface_phy_mode_get(a_uint32_t dev_id, a_uint32_t phy_id, fal_phy_config_t * config);

#endif

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _ISIS_INTERFACE_CTRL_H_ */

