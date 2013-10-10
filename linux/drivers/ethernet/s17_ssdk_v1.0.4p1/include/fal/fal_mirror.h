/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup fal_mirror FAL_MIRROR
 * @{
 */ 
#ifndef _FAL_MIRROR_H_
#define _FAL_MIRROR_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "common/sw.h"
#include "fal/fal_type.h"



sw_error_t fal_mirr_analysis_port_set(a_uint32_t dev_id, fal_port_t port_id);



sw_error_t fal_mirr_analysis_port_get(a_uint32_t dev_id, fal_port_t * port_id);



sw_error_t
fal_mirr_port_in_set(a_uint32_t dev_id, fal_port_t port_id,
                     a_bool_t enable);



sw_error_t
fal_mirr_port_in_get(a_uint32_t dev_id, fal_port_t port_id,
                     a_bool_t * enable);



sw_error_t
fal_mirr_port_eg_set(a_uint32_t dev_id, fal_port_t port_id,
                     a_bool_t enable);



sw_error_t
fal_mirr_port_eg_get(a_uint32_t dev_id, fal_port_t port_id,
                         a_bool_t * enable);


#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _PORT_MIRROR_H_ */
/**
 * @}
 */
