/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#ifndef _HSL_DEV_H
#define _HSL_DEV_H

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "hsl_api.h"
#include "ssdk_init.h"

#define HSL_DEV_ID_CHECK(dev_id) \
do { \
    if (dev_id >= SW_MAX_NR_DEV) \
        return SW_OUT_OF_RANGE; \
} while (0)

typedef struct {
    a_uint32_t dev_id;
    a_uint8_t cpu_port_nr;
    a_uint8_t nr_ports;
    a_uint8_t nr_phy;
    a_uint8_t nr_queue;
    a_uint16_t nr_vlans;
    a_bool_t hw_vlan_query;
    hsl_acl_func_t acl_func;
    hsl_init_mode  cpu_mode;
} hsl_dev_t;

hsl_dev_t *hsl_dev_ptr_get(a_uint32_t dev_id);
hsl_acl_func_t *hsl_acl_ptr_get(a_uint32_t dev_id);

sw_error_t
hsl_dev_init(a_uint32_t dev_id, ssdk_init_cfg * cfg);

sw_error_t
hsl_dev_reduced_init(a_uint32_t dev_id, hsl_init_mode cpu_mode,
                                    hsl_access_mode reg_mode);

sw_error_t
hsl_ssdk_cfg(a_uint32_t dev_id, ssdk_cfg_t *ssdk_cfg);

sw_error_t
hsl_dev_cleanup(void);

sw_error_t
hsl_access_mode_set(a_uint32_t dev_id, hsl_access_mode reg_mode);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _HSL_DEV_H */
