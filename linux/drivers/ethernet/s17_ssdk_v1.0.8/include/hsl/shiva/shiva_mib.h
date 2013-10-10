/*
 * Copyright (c) 2007-2009 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup shiva_mib SHIVA_MIB
 * @{
 */ 
#ifndef _SHIVA_MIB_H_
#define _SHIVA_MIB_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "fal/fal_mib.h"

sw_error_t
shiva_mib_init(a_uint32_t dev_id);

#ifdef IN_MIB
    #define SHIVA_MIB_INIT(rv, dev_id) \
    { \
        rv = shiva_mib_init(dev_id); \
        SW_RTN_ON_ERROR(rv); \
    }
#else
    #define SHIVA_MIB_INIT(rv, dev_id)
#endif

#ifdef HSL_STANDALONG

HSL_LOCAL sw_error_t
shiva_get_mib_info(a_uint32_t dev_id, fal_port_t port_id, 
                    fal_mib_info_t * mib_info );


HSL_LOCAL sw_error_t
shiva_mib_status_set(a_uint32_t dev_id, a_bool_t enable);


HSL_LOCAL sw_error_t
shiva_mib_status_get(a_uint32_t dev_id, a_bool_t * enable);

#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SHIVA_MIB_H_ */
/**
 * @}
 */
