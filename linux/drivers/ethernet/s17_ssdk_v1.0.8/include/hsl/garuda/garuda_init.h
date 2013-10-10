/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup garuda_init GARUDA_INIT
 * @{
 */ 
#ifndef _GARUDA_INIT_H_
#define _GARUDA_INIT_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "init/ssdk_init.h"

sw_error_t
garuda_init(a_uint32_t dev_id, ssdk_init_cfg *cfg);

sw_error_t
garuda_cleanup(a_uint32_t dev_id);

#ifdef HSL_STANDALONG

HSL_LOCAL sw_error_t
garuda_reset(a_uint32_t dev_id);

#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _GARUDA_INIT_H_ */

