/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#ifndef _FAL_UK_IF_H_
#define _FAL_UK_IF_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "common/sw.h"
#include "fal/fal_type.h"
#include "init/ssdk_init.h"

sw_error_t
sw_uk_exec(a_uint32_t api_id, ...);

sw_error_t
ssdk_init(a_uint32_t dev_id, ssdk_init_cfg * cfg);

sw_error_t
ssdk_cleanup(void);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_UK_IF_H_ */


