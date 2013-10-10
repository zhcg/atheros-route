/*
 * Copyright (c) 2007-2009 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

/**
 * @defgroup isis_trunk ISIS_TRUNK
 * @{
 */
#include "sw.h"
#include "hsl.h"
#include "hsl_dev.h"
#include "hsl_port_prop.h"
#include "isis_trunk.h"
#include "isis_reg.h"

#define ISIS_MAX_TRUNK_ID    3

static sw_error_t
_isis_trunk_group_set(a_uint32_t dev_id, a_uint32_t trunk_id,
                      a_bool_t enable, fal_pbmp_t member)
{
    sw_error_t rv;
    a_uint32_t i, reg, cnt = 0, data0 = 0, data1 = 0;

    if (ISIS_MAX_TRUNK_ID < trunk_id) {
        return SW_BAD_PARAM;
    }

    if (A_FALSE == hsl_mports_prop_check(dev_id, member, HSL_PP_INCL_CPU)) {
        return SW_BAD_PARAM;
    }

    if (A_TRUE == enable) {
        data0 = (0x1 << 7) | member;

        for (i = 0; i < 7; i++) {
            if (member & (0x1 << i)) {
                if (4 <= cnt) {
                    return SW_BAD_PARAM;
                }

                data1 |= (i << (cnt << 2));
                data1 |= (1 << (3 + (cnt << 2)));
                cnt++;
            }
        }
    } else if (A_FALSE == enable) {

    } else {
        return SW_BAD_PARAM;
    }

    /* set trunk port member bitmap info */
    HSL_REG_ENTRY_GET(rv, dev_id, GOL_TRUNK_CTL0, 0,
                      (a_uint8_t *) (&reg), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    reg &= (~(0xff << (trunk_id << 3)));
    reg |= (data0 << (trunk_id << 3));

    HSL_REG_ENTRY_SET(rv, dev_id, GOL_TRUNK_CTL0, 0,
                      (a_uint8_t *) (&reg), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    /* set trunk port member id info */
    HSL_REG_ENTRY_GET(rv, dev_id, GOL_TRUNK_CTL1, (trunk_id >> 1),
                      (a_uint8_t *) (&reg), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    reg &= (~(0xffff << ((trunk_id % 2) << 4)));
    reg |= (data1 << ((trunk_id % 2) << 4));

    HSL_REG_ENTRY_SET(rv, dev_id, GOL_TRUNK_CTL1, (trunk_id >> 1),
                      (a_uint8_t *) (&reg), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    return SW_OK;
}

static sw_error_t
_isis_trunk_group_get(a_uint32_t dev_id, a_uint32_t trunk_id,
                      a_bool_t * enable, fal_pbmp_t * member)
{
    sw_error_t rv;
    a_uint32_t data, reg;

    if (ISIS_MAX_TRUNK_ID < trunk_id) {
        return SW_BAD_PARAM;
    }

    HSL_REG_ENTRY_GET(rv, dev_id, GOL_TRUNK_CTL0, 0,
                      (a_uint8_t *) (&reg), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    data = (reg >> (trunk_id << 3)) & 0xff;
    if (0x80 & data) {
        *enable = A_TRUE;
        *member = data & 0x7f;
    } else {
        *enable = A_FALSE;
        *member = 0;
    }

    return SW_OK;
}

static sw_error_t
_isis_trunk_hash_mode_set(a_uint32_t dev_id, a_uint32_t hash_mode)
{
    sw_error_t rv;
    a_uint32_t data = 0;

    if (FAL_TRUNK_HASH_KEY_DA & hash_mode) {
        SW_SET_REG_BY_FIELD(TRUNK_HASH_MODE, DA_EN, 1, data);
    }

    if (FAL_TRUNK_HASH_KEY_SA & hash_mode) {
        SW_SET_REG_BY_FIELD(TRUNK_HASH_MODE, SA_EN, 1, data);
    }

    if (FAL_TRUNK_HASH_KEY_DIP & hash_mode) {
        SW_SET_REG_BY_FIELD(TRUNK_HASH_MODE, DIP_EN, 1, data);
    }

    if (FAL_TRUNK_HASH_KEY_SIP & hash_mode) {
        SW_SET_REG_BY_FIELD(TRUNK_HASH_MODE, SIP_EN, 1, data);
    }

    HSL_REG_ENTRY_SET(rv, dev_id, TRUNK_HASH_MODE, 0,
                      (a_uint8_t *) (&data), sizeof (a_uint32_t));
    return rv;
}

static sw_error_t
_isis_trunk_hash_mode_get(a_uint32_t dev_id, a_uint32_t * hash_mode)
{
    sw_error_t rv;
    a_uint32_t reg, data = 0;

    HSL_REG_ENTRY_GET(rv, dev_id, TRUNK_HASH_MODE, 0,
                      (a_uint8_t *) (&reg), sizeof (a_uint32_t));
    SW_RTN_ON_ERROR(rv);

    *hash_mode = 0;

    SW_GET_FIELD_BY_REG(TRUNK_HASH_MODE, DA_EN, data, reg);
    if (data) {
        *hash_mode |= FAL_TRUNK_HASH_KEY_DA;
    }

    SW_GET_FIELD_BY_REG(TRUNK_HASH_MODE, SA_EN, data, reg);
    if (data) {
        *hash_mode |= FAL_TRUNK_HASH_KEY_SA;
    }

    SW_GET_FIELD_BY_REG(TRUNK_HASH_MODE, DIP_EN, data, reg);
    if (data) {
        *hash_mode |= FAL_TRUNK_HASH_KEY_DIP;
    }

    SW_GET_FIELD_BY_REG(TRUNK_HASH_MODE, SIP_EN, data, reg);
    if (data) {
        *hash_mode |= FAL_TRUNK_HASH_KEY_SIP;
    }

    return SW_OK;
}

/**
 * @brief Set particular trunk group information on particular device.
 * @param[in] dev_id device id
 * @param[in] trunk_id trunk group id
 * @param[in] enable trunk group status, enable or disable
 * @param[in] member port member information
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_trunk_group_set(a_uint32_t dev_id, a_uint32_t trunk_id,
                     a_bool_t enable, fal_pbmp_t member)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_trunk_group_set(dev_id, trunk_id, enable, member);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get particular trunk group information on particular device.
 * @param[in] dev_id device id
 * @param[in] trunk_id trunk group id
 * @param[out] enable trunk group status, enable or disable
 * @param[out] member port member information
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_trunk_group_get(a_uint32_t dev_id, a_uint32_t trunk_id,
                     a_bool_t * enable, fal_pbmp_t * member)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_trunk_group_get(dev_id, trunk_id, enable, member);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set trunk hash mode on particular device.
 * @details   Comments:
    hash mode is listed below
    FAL_TRUNK_HASH_KEY_DA, FAL_TRUNK_HASH_KEY_SA, FAL_TRUNK_HASH_KEY_DIP and FAL_TRUNK_HASH_KEY_SIP    
 * @param[in] dev_id device id
 * @param[in] hash_mode trunk hash mode
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_trunk_hash_mode_set(a_uint32_t dev_id, a_uint32_t hash_mode)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_trunk_hash_mode_set(dev_id, hash_mode);
    HSL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get trunk hash mode on particular device.
 * @param[in] dev_id device id
 * @param[out] hash_mode trunk hash mode
 * @return SW_OK or error code
 */
HSL_LOCAL sw_error_t
isis_trunk_hash_mode_get(a_uint32_t dev_id, a_uint32_t * hash_mode)
{
    sw_error_t rv;

    HSL_API_LOCK;
    rv = _isis_trunk_hash_mode_get(dev_id, hash_mode);
    HSL_API_UNLOCK;
    return rv;
}

sw_error_t
isis_trunk_init(a_uint32_t dev_id)
{
    HSL_DEV_ID_CHECK(dev_id);

#ifndef HSL_STANDALONG
    {
        hsl_api_t *p_api;

        SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));
        p_api->trunk_group_set = isis_trunk_group_set;
        p_api->trunk_group_get = isis_trunk_group_get;
        p_api->trunk_hash_mode_set = isis_trunk_hash_mode_set;
        p_api->trunk_hash_mode_get = isis_trunk_hash_mode_get;        
    }
#endif

    return SW_OK;
}

/**
 * @}
 */

