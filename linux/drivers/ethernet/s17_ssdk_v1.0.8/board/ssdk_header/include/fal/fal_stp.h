/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup fal_stp FAL_STP
 * @{
 */  
#ifndef _FAL_STP_H_
#define _FAL_STP_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "common/sw.h"
#include "fal/fal_type.h"

#define FAL_SINGLE_STP_ID 0

/**
@brief This enum defines port state for spanning tree.
*/
typedef enum {
    FAL_STP_DISABLED = 0,	/**<   disable state*/
    FAL_STP_BLOKING,		/**<   blocking state*/
    FAL_STP_LISTENING,		/**<   listening state*/
    FAL_STP_LEARNING,		/**<    learning state*/
    FAL_STP_FARWARDING,		/**<   forwarding state*/
    FAL_STP_STATE_BUTT
} fal_stp_state_t;



sw_error_t
fal_stp_port_state_set(a_uint32_t dev_id, a_uint32_t st_id,
                          fal_port_t port_id, fal_stp_state_t state);



sw_error_t
fal_stp_port_state_get(a_uint32_t dev_id, a_uint32_t st_id,
                          fal_port_t port_id, fal_stp_state_t * state);


#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_STP_H_ */

/**
 * @}
 */
