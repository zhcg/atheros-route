/*
 * Copyright (c) 2007-2009 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup horus_misc HORUS_MISC
 * @{
 */ 
#ifndef _HORUS_MISC_H_
#define _HORUS_MISC_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "fal/fal_misc.h"

sw_error_t horus_misc_init(a_uint32_t dev_id);

#ifdef IN_MISC
    #define HORUS_MISC_INIT(rv, dev_id) \
    { \
        rv = horus_misc_init(dev_id); \
        SW_RTN_ON_ERROR(rv); \
    }
#else
    #define HORUS_MISC_INIT(rv, dev_id)
#endif

#ifdef HSL_STANDALONG

HSL_LOCAL sw_error_t
horus_arp_status_set(a_uint32_t dev_id, a_bool_t enable);


HSL_LOCAL sw_error_t
horus_arp_status_get(a_uint32_t dev_id, a_bool_t * enable);


HSL_LOCAL sw_error_t
horus_frame_max_size_set(a_uint32_t dev_id, a_uint32_t size);


HSL_LOCAL sw_error_t
horus_frame_max_size_get(a_uint32_t dev_id, a_uint32_t * size);


HSL_LOCAL sw_error_t
horus_port_unk_sa_cmd_set(a_uint32_t dev_id, fal_port_t port_id,
                           fal_fwd_cmd_t cmd);


HSL_LOCAL sw_error_t
horus_port_unk_sa_cmd_get(a_uint32_t dev_id, fal_port_t port_id,
                           fal_fwd_cmd_t * cmd);


HSL_LOCAL sw_error_t
horus_port_unk_uc_filter_set(a_uint32_t dev_id, fal_port_t port_id,
                              a_bool_t enable);


HSL_LOCAL sw_error_t
horus_port_unk_uc_filter_get(a_uint32_t dev_id, fal_port_t port_id,
                              a_bool_t * enable);


HSL_LOCAL sw_error_t
horus_port_unk_mc_filter_set(a_uint32_t dev_id, fal_port_t port_id,
                              a_bool_t enable);


HSL_LOCAL sw_error_t
horus_port_unk_mc_filter_get(a_uint32_t dev_id, fal_port_t port_id,
                              a_bool_t * enable);


HSL_LOCAL sw_error_t
horus_port_bc_filter_set(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t enable);


HSL_LOCAL sw_error_t
horus_port_bc_filter_get(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t * enable);


HSL_LOCAL sw_error_t
horus_cpu_port_status_set(a_uint32_t dev_id, a_bool_t enable);


HSL_LOCAL sw_error_t
horus_cpu_port_status_get(a_uint32_t dev_id, a_bool_t * enable);


HSL_LOCAL sw_error_t
horus_pppoe_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd);


HSL_LOCAL sw_error_t
horus_pppoe_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd);


HSL_LOCAL sw_error_t
horus_pppoe_status_set(a_uint32_t dev_id, a_bool_t enable);


HSL_LOCAL sw_error_t
horus_pppoe_status_get(a_uint32_t dev_id, a_bool_t * enable);


HSL_LOCAL sw_error_t
horus_port_dhcp_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);


HSL_LOCAL sw_error_t
horus_port_dhcp_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable);


HSL_LOCAL sw_error_t
horus_arp_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd);


HSL_LOCAL sw_error_t
horus_arp_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd);


HSL_LOCAL sw_error_t
horus_eapol_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd);


HSL_LOCAL sw_error_t
horus_eapol_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd);

HSL_LOCAL sw_error_t
horus_eapol_status_set(a_uint32_t dev_id, a_uint32_t port_id, a_bool_t enable);

HSL_LOCAL sw_error_t
horus_eapol_status_get(a_uint32_t dev_id, a_uint32_t port_id, a_bool_t * enable);

HSL_LOCAL sw_error_t
horus_ripv1_status_set(a_uint32_t dev_id, a_bool_t enable);

HSL_LOCAL sw_error_t
horus_ripv1_status_get(a_uint32_t dev_id, a_bool_t * enable);


#endif

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _HORUS_GEN_H_ */
/**
 * @}
 */
