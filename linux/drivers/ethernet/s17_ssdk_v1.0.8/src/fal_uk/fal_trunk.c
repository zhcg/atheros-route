/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#include "sw.h"
#include "sw_ioctl.h"
#include "fal_trunk.h"
#include "fal_uk_if.h"


sw_error_t
fal_trunk_group_set(a_uint32_t dev_id, a_uint32_t trunk_id,
                          a_bool_t enable, fal_pbmp_t member)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_TRUNK_GROUP_SET, dev_id, trunk_id, enable,
                       (a_uint32_t) member);
    return rv;
}

sw_error_t
fal_trunk_group_get(a_uint32_t dev_id, a_uint32_t trunk_id,
                          a_bool_t * enable, fal_pbmp_t * member)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_TRUNK_GROUP_GET, dev_id, trunk_id, enable,
                       (a_uint32_t) member);
    return rv;
}

sw_error_t
fal_trunk_hash_mode_set(a_uint32_t dev_id, a_uint32_t hash_mode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_TRUNK_HASH_SET, dev_id, hash_mode);
    return rv;
}

sw_error_t
fal_trunk_hash_mode_get(a_uint32_t dev_id, a_uint32_t * hash_mode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_TRUNK_HASH_GET, dev_id, hash_mode);
    return rv;
}
