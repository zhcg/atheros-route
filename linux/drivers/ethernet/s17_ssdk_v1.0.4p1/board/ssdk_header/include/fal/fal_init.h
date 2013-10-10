/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup fal_init FAL_INIT
 * @{
 */
#ifndef _FAL_INIT_H_
#define _FAL_INIT_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "init/ssdk_init.h"



sw_error_t fal_init(a_uint32_t dev_id, ssdk_init_cfg * cfg);



sw_error_t
fal_reduced_init(a_uint32_t dev_id, hsl_init_mode cpu_mode,
                 hsl_access_mode reg_mode);


sw_error_t fal_reset(a_uint32_t dev_id);
sw_error_t fal_ssdk_cfg(a_uint32_t dev_id, ssdk_cfg_t *ssdk_cfg);
sw_error_t fal_cleanup(void);


#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_INIT_H_ */
/**
 * @}
 */
