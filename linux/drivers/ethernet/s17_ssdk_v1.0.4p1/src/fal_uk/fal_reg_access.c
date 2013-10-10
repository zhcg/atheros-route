/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */  
#include "sw.h"
#include "sw_ioctl.h"
#include "fal_reg_access.h"
#include "fal_uk_if.h"

sw_error_t
fal_phy_get(a_uint32_t dev_id, a_uint32_t phy_addr,
                   a_uint32_t reg, a_uint16_t * value)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PHY_GET, dev_id, phy_addr, reg, value);
    return rv;
}

sw_error_t
fal_phy_set(a_uint32_t dev_id, a_uint32_t phy_addr,
                   a_uint32_t reg, a_uint16_t value)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PHY_SET, dev_id, phy_addr, reg, value);
    return rv;
}

sw_error_t
fal_reg_get(a_uint32_t dev_id, a_uint32_t reg_addr, a_uint8_t value[],
                      a_uint32_t value_len)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_REG_GET, dev_id, reg_addr, value, value_len);
    return rv;
}

sw_error_t
fal_reg_set(a_uint32_t dev_id, a_uint32_t reg_addr, a_uint8_t value[],
                      a_uint32_t value_len)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_REG_SET, dev_id, reg_addr, value, value_len);
    return rv;
}


sw_error_t
fal_reg_field_get(a_uint32_t dev_id, a_uint32_t reg_addr,
                     a_uint32_t bit_offset, a_uint32_t field_len,
                     a_uint8_t value[], a_uint32_t value_len)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_REG_FIELD_GET, dev_id, reg_addr, bit_offset, field_len, value, value_len);
    return rv;
}


sw_error_t
fal_reg_field_set(a_uint32_t dev_id, a_uint32_t reg_addr,
                     a_uint32_t bit_offset, a_uint32_t field_len,
                     const a_uint8_t value[], a_uint32_t value_len)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_REG_FIELD_SET, dev_id, reg_addr, bit_offset, field_len, value, value_len);
    return rv;
}

