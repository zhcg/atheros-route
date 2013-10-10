/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup fal_cosmap FAL_COSMAP
 * @{
 */
#ifndef _FAL_COSMAP_H_
#define _FAL_COSMAP_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "common/sw.h"
#include "fal/fal_type.h"

typedef struct {
    a_bool_t remark_dscp;
    a_bool_t remark_up;
    a_uint8_t g_dscp;
    a_uint8_t y_dscp;
    a_uint8_t g_up;
    a_uint8_t y_up;
} fal_egress_remark_table_t;

sw_error_t
fal_cosmap_dscp_to_pri_set(a_uint32_t dev_id, a_uint32_t dscp,
                           a_uint32_t pri);

sw_error_t
fal_cosmap_dscp_to_pri_get(a_uint32_t dev_id, a_uint32_t dscp,
                           a_uint32_t * pri);

sw_error_t
fal_cosmap_dscp_to_dp_set(a_uint32_t dev_id, a_uint32_t dscp,
                          a_uint32_t dp);

sw_error_t
fal_cosmap_dscp_to_dp_get(a_uint32_t dev_id, a_uint32_t dscp,
                          a_uint32_t * dp);

sw_error_t
fal_cosmap_up_to_pri_set(a_uint32_t dev_id, a_uint32_t up,
                         a_uint32_t pri);

sw_error_t
fal_cosmap_up_to_pri_get(a_uint32_t dev_id, a_uint32_t up,
                         a_uint32_t * pri);

sw_error_t
fal_cosmap_up_to_dp_set(a_uint32_t dev_id, a_uint32_t up,
                        a_uint32_t dp);

sw_error_t
fal_cosmap_up_to_dp_get(a_uint32_t dev_id, a_uint32_t up,
                        a_uint32_t * dp);

sw_error_t
fal_cosmap_pri_to_queue_set(a_uint32_t dev_id, a_uint32_t pri,
                            a_uint32_t queue);

sw_error_t
fal_cosmap_pri_to_queue_get(a_uint32_t dev_id, a_uint32_t pri,
                            a_uint32_t * queue);

sw_error_t
fal_cosmap_pri_to_ehqueue_set(a_uint32_t dev_id, a_uint32_t pri,
                              a_uint32_t queue);

sw_error_t
fal_cosmap_pri_to_ehqueue_get(a_uint32_t dev_id, a_uint32_t pri,
                              a_uint32_t * queue);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_COSMAP_H_ */

/**
 * @}
 */

