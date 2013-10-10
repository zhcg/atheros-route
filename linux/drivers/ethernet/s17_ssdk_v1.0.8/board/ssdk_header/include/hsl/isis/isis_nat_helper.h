/*
 * Copyright (c) 2007-2009 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
#ifndef _ISIS_NAT_HELPER_H_
#define _ISIS_NAT_HELPER_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "fal/fal_nat.h"

sw_error_t nat_helper_init(a_uint32_t dev_id);

sw_error_t nat_helper_cleanup(a_uint32_t dev_id);

#ifdef IN_NAT_HELPER
#define ISIS_NAT_HELPER_INIT(rv, dev_id) \
    { \
        rv = nat_helper_init(dev_id); \
        SW_RTN_ON_ERROR(rv); \
    }

#define ISIS_NAT_HELPER_CLEANUP(rv, dev_id) \
    { \
        rv = nat_helper_cleanup(dev_id); \
        SW_RTN_ON_ERROR(rv); \
    }
#else
#define ISIS_NAT_HELPER_INIT(rv, dev_id)
#define ISIS_NAT_HELPER_CLEANUP(rv, dev_id)
#endif


#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _ISIS_NAT_HELPER_H_ */
