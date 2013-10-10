/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup fal_reg_access FAL_REG_ACCESS
 * @{
 */  
#ifndef _FAL_REG_ACCESS_H_
#define _FAL_REG_ACCESS_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "common/sw.h"
#include "fal/fal_type.h"

sw_error_t
fal_phy_get(a_uint32_t dev_id, a_uint32_t phy_addr,
                   a_uint32_t reg, a_uint16_t * value);

sw_error_t
fal_phy_set(a_uint32_t dev_id, a_uint32_t phy_addr,
                   a_uint32_t reg, a_uint16_t value);

sw_error_t
fal_reg_get(a_uint32_t dev_id, a_uint32_t reg_addr, a_uint8_t value[],
                      a_uint32_t value_len);

sw_error_t
fal_reg_set(a_uint32_t dev_id, a_uint32_t reg_addr, a_uint8_t value[],
                      a_uint32_t value_len);

sw_error_t
fal_reg_field_get(a_uint32_t dev_id, a_uint32_t reg_addr,
                     a_uint32_t bit_offset, a_uint32_t field_len,
                     a_uint8_t value[], a_uint32_t value_len);

sw_error_t
fal_reg_field_set(a_uint32_t dev_id, a_uint32_t reg_addr,
                     a_uint32_t bit_offset, a_uint32_t field_len,
                     const a_uint8_t value[], a_uint32_t value_len);


#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_REG_ACCESS_H_ */

/**
 * @}
 */

