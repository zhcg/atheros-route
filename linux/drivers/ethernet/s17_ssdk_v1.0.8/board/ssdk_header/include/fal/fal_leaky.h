/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup fal_leaky FAL_LEAKY
 * @{
 */ 
#ifndef _FAL_LEAKY_H_
#define _FAL_LEAKY_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "common/sw.h"
#include "fal/fal_type.h"

/**
@brief This enum defines the leaky control mode.
*/
typedef enum {
    FAL_LEAKY_PORT_CTRL = 0,		/**<   control leaky through port which packets received*/
    FAL_LEAKY_FDB_CTRL,				/**<   control leaky through fdb entry*/
    FAL_LEAKY_CTRL_MODE_BUTT
} fal_leaky_ctrl_mode_t;



sw_error_t
fal_uc_leaky_mode_set(a_uint32_t dev_id,
                      fal_leaky_ctrl_mode_t ctrl_mode);



sw_error_t
fal_uc_leaky_mode_get(a_uint32_t dev_id,
                      fal_leaky_ctrl_mode_t * ctrl_mode);



sw_error_t
fal_mc_leaky_mode_set(a_uint32_t dev_id,
                      fal_leaky_ctrl_mode_t ctrl_mode);



sw_error_t
fal_mc_leaky_mode_get(a_uint32_t dev_id,
                      fal_leaky_ctrl_mode_t * ctrl_mode);



sw_error_t
fal_port_arp_leaky_set(a_uint32_t dev_id, fal_port_t port_id,
                       a_bool_t enable);



sw_error_t
fal_port_arp_leaky_get(a_uint32_t dev_id, fal_port_t port_id,
                       a_bool_t * enable);



sw_error_t
fal_port_uc_leaky_set(a_uint32_t dev_id, fal_port_t port_id,
                      a_bool_t enable);



sw_error_t
fal_port_uc_leaky_get(a_uint32_t dev_id, fal_port_t port_id,
                      a_bool_t * enable);



sw_error_t
fal_port_mc_leaky_set(a_uint32_t dev_id, fal_port_t port_id,
                      a_bool_t enable);



sw_error_t
fal_port_mc_leaky_get(a_uint32_t dev_id, fal_port_t port_id,
                      a_bool_t * enable);


#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_LEAKY_H_ */
/**
 * @}
 */
