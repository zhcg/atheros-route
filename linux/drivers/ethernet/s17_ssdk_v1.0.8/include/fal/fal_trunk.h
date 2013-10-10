/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup fal_trunk FAL_TRUNK
 * @{
 */  
#ifndef _FAL_TRUNK_H_
#define _FAL_TRUNK_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "common/sw.h"
#include "fal/fal_type.h"


#define FAL_TRUNK_HASH_KEY_DA              0x1
#define FAL_TRUNK_HASH_KEY_SA              0x2
#define FAL_TRUNK_HASH_KEY_DIP             0x4
#define FAL_TRUNK_HASH_KEY_SIP             0x8


sw_error_t
fal_trunk_group_set(a_uint32_t dev_id, a_uint32_t trunk_id,
                          a_bool_t enable, fal_pbmp_t member);


sw_error_t
fal_trunk_group_get(a_uint32_t dev_id, a_uint32_t trunk_id,
                          a_bool_t * enable, fal_pbmp_t * member);


sw_error_t
fal_trunk_hash_mode_set(a_uint32_t dev_id, a_uint32_t hash_mode);


sw_error_t
fal_trunk_hash_mode_get(a_uint32_t dev_id, a_uint32_t * hash_mode);


#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_TRUNK_H_ */

/**
 * @}
 */
