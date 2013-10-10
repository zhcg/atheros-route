/*
 * Copyright (c) 2007-2011 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup fal_interface_ctrl FAL_INTERFACE_CONTROL
 * @{
 */  
#ifndef _FAL_INTERFACECTRL_H_
#define _FAL_INTERFACECTRL_H_

#ifdef __cplusplus
extern "c" {
#endif

#include "common/sw.h"
#include "fal/fal_type.h"

typedef enum {
    FAL_MAC_MODE_RGMII = 0,
    FAL_MAC_MODE_GMII,
    FAL_MAC_MODE_MII,
    FAL_MAC_MODE_SGMII,
    FAL_MAC_MODE_FIBER,
    FAL_MAC_MODE_DEFAULT
} fal_interface_mac_mode_t;

typedef enum {
    FAL_INTERFACE_CLOCK_MAC_MODE = 0,
    FAL_INTERFACE_CLOCK_PHY_MODE = 1,
} fal_interface_clock_mode_t;

typedef struct {
    a_bool_t   txclk_delay_cmd;
    a_bool_t   rxclk_delay_cmd;
    a_uint32_t txclk_delay_sel;
    a_uint32_t rxclk_delay_sel;
}fal_mac_rgmii_config_t;

typedef struct {
    fal_interface_clock_mode_t   clock_mode;
    a_uint32_t                   txclk_select;
    a_uint32_t                   rxclk_select;
}fal_mac_gmii_config_t;

typedef struct {
    fal_interface_clock_mode_t   clock_mode;
    a_uint32_t                   txclk_select;
    a_uint32_t                   rxclk_select;
}fal_mac_mii_config_t;

typedef struct {
    fal_interface_clock_mode_t  clock_mode;
    a_bool_t                    auto_neg;
}fal_mac_sgmii_config_t;

typedef struct {
    a_bool_t                    auto_neg;
}fal_mac_fiber_config_t;

typedef struct {
    fal_interface_mac_mode_t      mac_mode;
    union {
        fal_mac_rgmii_config_t    rgmii;
        fal_mac_gmii_config_t     gmii;
        fal_mac_mii_config_t      mii;
        fal_mac_sgmii_config_t    sgmii;
        fal_mac_fiber_config_t    fiber;
    } config;
}fal_mac_config_t;

typedef struct {
    fal_interface_mac_mode_t mac_mode;
    a_bool_t                 txclk_delay_cmd;
    a_bool_t                 rxclk_delay_cmd;
    a_uint32_t               txclk_delay_sel;
    a_uint32_t               rxclk_delay_sel;
}fal_phy_config_t;

sw_error_t
fal_port_3az_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);

sw_error_t
fal_port_3az_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable);

sw_error_t
fal_interface_mac_mode_set(a_uint32_t dev_id, fal_port_t port_id, fal_mac_config_t * config);

sw_error_t
fal_interface_mac_mode_get(a_uint32_t dev_id, fal_port_t port_id, fal_mac_config_t * config);

sw_error_t
fal_interface_phy_mode_set(a_uint32_t dev_id, a_uint32_t phy_id, fal_phy_config_t * config);

sw_error_t
fal_interface_phy_mode_get(a_uint32_t dev_id, a_uint32_t phy_id, fal_phy_config_t * config);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_INTERFACECTRL_H_ */
/**
 * @}
 */
