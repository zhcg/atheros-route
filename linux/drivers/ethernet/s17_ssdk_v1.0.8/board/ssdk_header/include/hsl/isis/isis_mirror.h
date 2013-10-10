/*
 * Copyright (c) 2007-2009 Atheros Communications, Inc.
 * All rights reserved.
 *
 */ 
#ifndef _ISIS_MIRROR_H_
#define _ISIS_MIRROR_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "fal/fal_mirror.h"

sw_error_t isis_mirr_init(a_uint32_t dev_id);

#ifdef IN_MIRROR
    #define ISIS_MIRR_INIT(rv, dev_id) \
    { \
        rv = isis_mirr_init(dev_id); \
        SW_RTN_ON_ERROR(rv); \
    }
#else
    #define ISIS_MIRR_INIT(rv, dev_id)
#endif

#ifdef HSL_STANDALONG

HSL_LOCAL sw_error_t
isis_mirr_analysis_port_set(a_uint32_t dev_id, fal_port_t port_id);


HSL_LOCAL sw_error_t
isis_mirr_analysis_port_get(a_uint32_t dev_id, fal_port_t * port_id);


HSL_LOCAL sw_error_t
isis_mirr_port_in_set(a_uint32_t dev_id, fal_port_t port_id,
                        a_bool_t enable);


HSL_LOCAL sw_error_t
isis_mirr_port_in_get(a_uint32_t dev_id, fal_port_t port_id,
                        a_bool_t * enable);


HSL_LOCAL sw_error_t
isis_mirr_port_eg_set(a_uint32_t dev_id, fal_port_t port_id,
                       a_bool_t enable);


HSL_LOCAL sw_error_t
isis_mirr_port_eg_get(a_uint32_t dev_id, fal_port_t port_id,
                       a_bool_t * enable);

#endif

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _ISIS_MIRROR_H_ */

