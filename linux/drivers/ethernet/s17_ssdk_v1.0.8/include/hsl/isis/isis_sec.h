/*
 * Copyright (c) 2007-2009 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
#ifndef _ISIS_SEC_H_
#define _ISIS_SEC_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "fal/fal_sec.h"

sw_error_t isis_sec_init(a_uint32_t dev_id);

#ifdef IN_SEC
#define ISIS_SEC_INIT(rv, dev_id) \
    { \
        rv = isis_sec_init(dev_id); \
        SW_RTN_ON_ERROR(rv); \
    }
#else
#define ISIS_SEC_INIT(rv, dev_id)
#endif

#ifdef HSL_STANDALONG

HSL_LOCAL sw_error_t
isis_sec_norm_item_set(a_uint32_t dev_id, fal_norm_item_t item,
                       void *value);

HSL_LOCAL sw_error_t
isis_sec_norm_item_get(a_uint32_t dev_id, fal_norm_item_t item,
                       void *value);

#endif

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _ISIS_SEC_H_ */

