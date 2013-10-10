/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
 
#ifndef _HSL_ACL_H_
#define _HSL_ACL_H_

#ifdef __cplusplus
extern "c" {
#endif

sw_error_t
hsl_acl_pool_creat(a_uint32_t dev_id, a_uint32_t blk_nr, a_uint32_t rsc_nr);

sw_error_t
hsl_acl_pool_destroy(a_uint32_t dev_id);

sw_error_t
hsl_acl_blk_alloc(a_uint32_t dev_id, a_uint32_t pri, a_uint32_t size,
                  a_uint32_t info, a_uint32_t * addr);

sw_error_t
hsl_acl_blk_free(a_uint32_t dev_id, a_uint32_t addr);

sw_error_t
hsl_acl_blk_resize(a_uint32_t dev_id, a_uint32_t addr, a_uint32_t new_size);

sw_error_t
hsl_acl_free_rsc_get(a_uint32_t dev_id, a_uint32_t * free_rsc);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /*_HSL_ACL_H_ */
