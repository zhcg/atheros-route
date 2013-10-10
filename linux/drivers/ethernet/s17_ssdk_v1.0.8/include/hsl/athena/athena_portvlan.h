/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#ifndef _ATHENA_PORTVLAN_H
#define _ATHENA_PORTVLAN_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "fal/fal_portvlan.h"

sw_error_t
athena_portvlan_init(a_uint32_t dev_id);

#ifdef IN_PORTVLAN
    #define ATHENA_PORTVLAN_INIT(rv, dev_id) \
    { \
        rv = athena_portvlan_init(dev_id); \
        SW_RTN_ON_ERROR(rv); \
    }
#else
    #define ATHENA_PORTVLAN_INIT(rv, dev_id)
#endif

#ifdef HSL_STANDALONG


HSL_LOCAL sw_error_t
athena_port_1qmode_set(a_uint32_t dev_id, fal_port_t port_id,
                       fal_pt_1qmode_t port_1qmode);



HSL_LOCAL sw_error_t
athena_port_1qmode_get(a_uint32_t dev_id, fal_port_t port_id,
                       fal_pt_1qmode_t * pport_1qmode);



HSL_LOCAL sw_error_t
athena_port_egvlanmode_set(a_uint32_t dev_id, fal_port_t port_id,
                           fal_pt_1q_egmode_t port_egvlanmode);



HSL_LOCAL sw_error_t
athena_port_egvlanmode_get(a_uint32_t dev_id, fal_port_t port_id,
                           fal_pt_1q_egmode_t * pport_egvlanmode);



HSL_LOCAL sw_error_t
athena_portvlan_member_add(a_uint32_t dev_id, fal_port_t port_id,
                           a_uint32_t mem_port_id);



HSL_LOCAL sw_error_t
athena_portvlan_member_del(a_uint32_t dev_id, fal_port_t port_id,
                           a_uint32_t mem_port_id);



HSL_LOCAL sw_error_t
athena_portvlan_member_update(a_uint32_t dev_id, fal_port_t port_id,
                              fal_pbmp_t mem_port_map);



HSL_LOCAL sw_error_t
athena_portvlan_member_get(a_uint32_t dev_id, fal_port_t port_id,
                           fal_pbmp_t *mem_port_map);

#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _ATHENA_PORTVLAN_H */
