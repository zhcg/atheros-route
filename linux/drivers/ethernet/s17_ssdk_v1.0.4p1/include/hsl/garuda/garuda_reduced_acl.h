/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#ifndef _GARUDA_REDUCED_ACL_H_
#define _GARUDA_REDUCED_ACL_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "common/sw.h"

sw_error_t
garuda_acl_rule_write(a_uint32_t dev_id, a_uint32_t rule_idx, a_uint32_t vlu[8],
                        a_uint32_t msk[8]);

sw_error_t
garuda_acl_action_write(a_uint32_t dev_id, a_uint32_t act_idx,
                        a_uint32_t act);

sw_error_t
garuda_acl_slct_write(a_uint32_t dev_id, a_uint32_t slct_idx,
                      a_uint32_t slct[8]);

sw_error_t
garuda_acl_rule_read(a_uint32_t dev_id, a_uint32_t rule_idx, a_uint32_t vlu[8],
                        a_uint32_t msk[8]);

sw_error_t
garuda_acl_action_read(a_uint32_t dev_id, a_uint32_t act_idx,
                        a_uint32_t * act);

sw_error_t
garuda_acl_slct_read(a_uint32_t dev_id, a_uint32_t slct_idx,
                      a_uint32_t slct[8]);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _GARUDA_REDUCED_ACL_H_ */

