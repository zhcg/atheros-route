/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#ifndef _ATHENA_FDB_H
#define _ATHENA_FDB_H

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "fal/fal_fdb.h"

sw_error_t athena_fdb_init(a_uint32_t dev_id);

#ifdef IN_FDB
    #define ATHENA_FDB_INIT(rv, dev_id) \
    { \
        rv = athena_fdb_init(dev_id); \
        SW_RTN_ON_ERROR(rv); \
    }
#else
    #define ATHENA_FDB_INIT(rv, dev_id)
#endif

#ifdef HSL_STANDALONG


HSL_LOCAL sw_error_t
athena_fdb_add(a_uint32_t dev_id, const fal_fdb_entry_t * entry);



HSL_LOCAL sw_error_t
athena_fdb_del_all(a_uint32_t dev_id, a_uint32_t flag);



HSL_LOCAL sw_error_t
athena_fdb_del_by_port(a_uint32_t dev_id, a_uint32_t port_id,
                       a_uint32_t flag);



HSL_LOCAL sw_error_t
athena_fdb_del_by_mac(a_uint32_t dev_id, const fal_fdb_entry_t * addr);



HSL_LOCAL sw_error_t
athena_fdb_first(a_uint32_t dev_id, fal_fdb_entry_t * entry);



HSL_LOCAL sw_error_t
athena_fdb_next(a_uint32_t dev_id, fal_fdb_entry_t * entry);

#endif

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _ATHENA_FDB_H */

