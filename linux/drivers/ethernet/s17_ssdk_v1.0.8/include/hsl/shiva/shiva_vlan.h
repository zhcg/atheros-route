/*
 * Copyright (c) 2007-2009 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup shiva_vlan SHIVA_VLAN
 * @{
 */ 
#ifndef _SHIVA_VLAN_H_
#define _SHIVA_VLAN_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "fal/fal_vlan.h"

sw_error_t
shiva_vlan_init(a_uint32_t dev_id);

#ifdef IN_VLAN
    #define SHIVA_VLAN_INIT(rv, dev_id) \
    { \
        rv = shiva_vlan_init(dev_id); \
        SW_RTN_ON_ERROR(rv); \
    }
#else
    #define SHIVA_VLAN_INIT(rv, dev_id)
#endif

#ifdef HSL_STANDALONG


HSL_LOCAL sw_error_t
shiva_vlan_entry_append(a_uint32_t dev_id, const fal_vlan_t * vlan_entry);


HSL_LOCAL sw_error_t
shiva_vlan_create(a_uint32_t dev_id, a_uint32_t vlan_id);


HSL_LOCAL sw_error_t
shiva_vlan_next(a_uint32_t dev_id, a_uint32_t vlan_id, fal_vlan_t * p_vlan);


HSL_LOCAL sw_error_t
shiva_vlan_find(a_uint32_t dev_id, a_uint32_t vlan_id, fal_vlan_t * p_vlan);


HSL_LOCAL sw_error_t
shiva_vlan_member_update(a_uint32_t dev_id, a_uint32_t vlan_id, 
                          fal_pbmp_t member, fal_pbmp_t u_member);


HSL_LOCAL sw_error_t
shiva_vlan_delete(a_uint32_t dev_id, a_uint32_t vlan_id);


HSL_LOCAL sw_error_t
shiva_vlan_flush(a_uint32_t dev_id);


#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SHIVA_VLAN_H_ */
/**
 * @}
 */
