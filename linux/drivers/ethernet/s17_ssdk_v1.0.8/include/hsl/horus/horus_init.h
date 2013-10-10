/*
 * Copyright (c) 2007-2009 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup horus_init HORUS_INIT
 * @{
 */ 
#ifndef _HORUS_INIT_H_
#define _HORUS_INIT_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "init/ssdk_init.h"


sw_error_t
horus_init(a_uint32_t dev_id, ssdk_init_cfg *cfg);


sw_error_t
horus_reset(a_uint32_t dev_id);


sw_error_t
horus_cleanup(a_uint32_t dev_id);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _HORUS_INIT_H_ */
/**
 * @}
 */
