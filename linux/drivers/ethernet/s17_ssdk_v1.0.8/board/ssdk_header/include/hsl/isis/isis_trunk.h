/*
 * Copyright (c) 2007-2009 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
#ifndef _ISIS_TRUNK_H_
#define _ISIS_TRUNK_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "fal/fal_trunk.h"

sw_error_t isis_trunk_init(a_uint32_t dev_id);

#ifdef IN_TRUNK
#define ISIS_TRUNK_INIT(rv, dev_id) \
    { \
        rv = isis_trunk_init(dev_id); \
        SW_RTN_ON_ERROR(rv); \
    }
#else
#define ISIS_TRUNK_INIT(rv, dev_id)
#endif

#ifdef HSL_STANDALONG

HSL_LOCAL sw_error_t
isis_trunk_group_set(a_uint32_t dev_id, a_uint32_t trunk_id,
                     a_bool_t enable, fal_pbmp_t member);

HSL_LOCAL sw_error_t
isis_trunk_group_get(a_uint32_t dev_id, a_uint32_t trunk_id,
                     a_bool_t * enable, fal_pbmp_t * member);

HSL_LOCAL sw_error_t
isis_trunk_hash_mode_set(a_uint32_t dev_id, a_uint32_t hash_mode);

HSL_LOCAL sw_error_t
isis_trunk_hash_mode_get(a_uint32_t dev_id, a_uint32_t * hash_mode);

#endif

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _ISIS_TRUNK_H_ */

