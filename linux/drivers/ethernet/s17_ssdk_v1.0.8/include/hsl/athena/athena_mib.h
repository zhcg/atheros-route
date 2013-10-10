/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#ifndef _ATHENA_MIB_H
#define _ATHENA_MIB_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "fal/fal_mib.h"

sw_error_t
athena_mib_init(a_uint32_t dev_id);

#ifdef IN_MIB
    #define ATHENA_MIB_INIT(rv, dev_id) \
    { \
        rv = athena_mib_init(dev_id); \
        SW_RTN_ON_ERROR(rv); \
    }
#else
    #define ATHENA_MIB_INIT(rv, dev_id)
#endif

#ifdef HSL_STANDALONG


HSL_LOCAL sw_error_t
athena_get_mib_info(a_uint32_t dev_id, fal_port_t port_id, 
                    fal_mib_info_t * mib_info );
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _ATHENA_MIB_H */
