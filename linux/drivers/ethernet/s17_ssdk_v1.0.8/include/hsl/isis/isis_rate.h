/*
 * Copyright (c) 2007-2009 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
#ifndef _ISIS_RATE_H_
#define _ISIS_RATE_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "fal/fal_rate.h"

sw_error_t isis_rate_init(a_uint32_t dev_id);

#ifdef IN_RATE
#define ISIS_RATE_INIT(rv, dev_id) \
    { \
        rv = isis_rate_init(dev_id); \
        SW_RTN_ON_ERROR(rv); \
    }
#else
#define ISIS_RATE_INIT(rv, dev_id)
#endif

#ifdef HSL_STANDALONG

HSL_LOCAL sw_error_t
isis_rate_port_policer_set(a_uint32_t dev_id, fal_port_t port_id,
                           fal_port_policer_t * policer);

HSL_LOCAL sw_error_t
isis_rate_port_policer_get(a_uint32_t dev_id, fal_port_t port_id,
                           fal_port_policer_t * policer);

HSL_LOCAL sw_error_t
isis_rate_port_shaper_set(a_uint32_t dev_id, fal_port_t port_id,
                          a_bool_t enable,
                          fal_egress_shaper_t * shaper);

HSL_LOCAL sw_error_t
isis_rate_port_shaper_get(a_uint32_t dev_id, fal_port_t port_id,
                          a_bool_t * enable,
                          fal_egress_shaper_t * shaper);

HSL_LOCAL sw_error_t
isis_rate_queue_shaper_set(a_uint32_t dev_id, fal_port_t port_id,
                           fal_queue_t queue_id, a_bool_t enable,
                           fal_egress_shaper_t * shaper);

HSL_LOCAL sw_error_t
isis_rate_queue_shaper_get(a_uint32_t dev_id, fal_port_t port_id,
                           fal_queue_t queue_id, a_bool_t * enable,
                           fal_egress_shaper_t * shaper);

HSL_LOCAL sw_error_t
isis_rate_acl_policer_set(a_uint32_t dev_id, a_uint32_t policer_id,
                          fal_acl_policer_t * policer);

HSL_LOCAL sw_error_t
isis_rate_acl_policer_get(a_uint32_t dev_id, a_uint32_t policer_id,
                          fal_acl_policer_t * policer);

#endif

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _ISIS_RATE_H_ */

