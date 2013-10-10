/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup garuda_rate GARUDA_RATE
 * @{
 */ 
#ifndef _GARUDA_RATE_H_
#define _GARUDA_RATE_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "fal/fal_rate.h"

sw_error_t garuda_rate_init(a_uint32_t dev_id);

#ifdef IN_RATE
    #define GARUDA_RATE_INIT(rv, dev_id) \
    { \
        rv = garuda_rate_init(dev_id); \
        SW_RTN_ON_ERROR(rv); \
    }
#else
    #define GARUDA_RATE_INIT(rv, dev_id)
#endif    

#ifdef HSL_STANDALONG


HSL_LOCAL sw_error_t
garuda_rate_queue_egrl_set(a_uint32_t dev_id, fal_port_t port_id,
                          fal_queue_t queue_id, a_uint32_t * speed,
                          a_bool_t enable);



HSL_LOCAL sw_error_t
garuda_rate_queue_egrl_get(a_uint32_t dev_id, fal_port_t port_id,
                          fal_queue_t queue_id, a_uint32_t * speed,
                          a_bool_t * enable);



HSL_LOCAL sw_error_t
garuda_rate_port_egrl_set(a_uint32_t dev_id, fal_port_t port_id,
                         a_uint32_t * speed, a_bool_t enable);



HSL_LOCAL sw_error_t
garuda_rate_port_egrl_get(a_uint32_t dev_id, fal_port_t port_id,
                         a_uint32_t * speed, a_bool_t * enable);



HSL_LOCAL sw_error_t
garuda_rate_port_inrl_set(a_uint32_t dev_id, fal_port_t port_id,
                         a_uint32_t * speed, a_bool_t enable);



HSL_LOCAL sw_error_t
garuda_rate_port_inrl_get(a_uint32_t dev_id, fal_port_t port_id,
                         a_uint32_t * speed, a_bool_t * enable);



HSL_LOCAL sw_error_t
garuda_storm_ctrl_frame_set(a_uint32_t dev_id, fal_port_t port_id,
                            fal_storm_type_t storm_type, a_bool_t enable);



HSL_LOCAL sw_error_t
garuda_storm_ctrl_frame_get(a_uint32_t dev_id, fal_port_t port_id,
                            fal_storm_type_t storm_type, a_bool_t * enable);



HSL_LOCAL sw_error_t
garuda_storm_ctrl_rate_set(a_uint32_t dev_id, fal_port_t port_id,
                           a_uint32_t * rate_in_pps);



HSL_LOCAL sw_error_t
garuda_storm_ctrl_rate_get(a_uint32_t dev_id, fal_port_t port_id,
                           a_uint32_t * rate_in_pps);
#endif

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _GARUDA_RATE_H_ */
/**
 * @}
 */
