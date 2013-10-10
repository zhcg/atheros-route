/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#include "sw.h"
#include "sw_ioctl.h"
#include "fal_nat.h"
#include "fal_uk_if.h"

sw_error_t
fal_nat_add(a_uint32_t dev_id, fal_nat_entry_t * nat_entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_NAT_ADD, dev_id, (a_uint32_t) nat_entry);
    return rv;
}


sw_error_t
fal_nat_del(a_uint32_t dev_id, a_uint32_t del_mode, fal_nat_entry_t * nat_entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_NAT_DEL, dev_id, del_mode, (a_uint32_t) nat_entry);
    return rv;
}


sw_error_t
fal_nat_get(a_uint32_t dev_id, a_uint32_t get_mode, fal_nat_entry_t * nat_entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_NAT_GET, dev_id, get_mode, (a_uint32_t) nat_entry);
    return rv;
}

sw_error_t
fal_nat_next(a_uint32_t dev_id, a_uint32_t next_mode, fal_nat_entry_t * nat_entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_NAT_NEXT, dev_id, next_mode, (a_uint32_t) nat_entry);
    return rv;
}

sw_error_t
fal_nat_counter_bind(a_uint32_t dev_id, a_uint32_t entry_id, a_uint32_t cnt_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_NAT_COUNTER_BIND, dev_id, entry_id, cnt_id, (a_uint32_t) enable);
    return rv;
}


sw_error_t
fal_napt_add(a_uint32_t dev_id, fal_napt_entry_t * napt_entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_NAPT_ADD, dev_id, (a_uint32_t) napt_entry);
    return rv;
}

sw_error_t
fal_napt_del(a_uint32_t dev_id, a_uint32_t del_mode, fal_napt_entry_t * napt_entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_NAPT_DEL, dev_id, del_mode, (a_uint32_t) napt_entry);
    return rv;
}

sw_error_t
fal_napt_get(a_uint32_t dev_id, a_uint32_t get_mode, fal_napt_entry_t * napt_entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_NAPT_GET, dev_id, get_mode, (a_uint32_t) napt_entry);
    return rv;
}

sw_error_t
fal_napt_next(a_uint32_t dev_id, a_uint32_t next_mode, fal_napt_entry_t * napt_entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_NAPT_NEXT, dev_id, next_mode, (a_uint32_t) napt_entry);
    return rv;
}

sw_error_t
fal_napt_counter_bind(a_uint32_t dev_id, a_uint32_t entry_id, a_uint32_t cnt_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_NAPT_COUNTER_BIND, dev_id, entry_id, cnt_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_nat_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_NAT_STATUS_SET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_nat_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_NAT_STATUS_GET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_nat_hash_mode_set(a_uint32_t dev_id, a_uint32_t mode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_NAT_HASH_MODE_SET, dev_id, mode);
    return rv;
}

sw_error_t
fal_nat_hash_mode_get(a_uint32_t dev_id, a_uint32_t * mode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_NAT_HASH_MODE_GET, dev_id, (a_uint32_t) mode);
    return rv;
}

sw_error_t
fal_napt_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_NAPT_STATUS_SET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_napt_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_NAPT_STATUS_GET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_napt_mode_set(a_uint32_t dev_id, fal_napt_mode_t mode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_NAPT_MODE_SET, dev_id, (a_uint32_t) mode);
    return rv;
}

sw_error_t
fal_napt_mode_get(a_uint32_t dev_id, fal_napt_mode_t * mode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_NAPT_MODE_GET, dev_id, (a_uint32_t) mode);
    return rv;
}

sw_error_t
fal_nat_prv_base_addr_set(a_uint32_t dev_id, fal_ip4_addr_t addr)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PRV_BASE_ADDR_SET, dev_id, (a_uint32_t) addr);
    return rv;
}

sw_error_t
fal_nat_prv_base_addr_get(a_uint32_t dev_id, fal_ip4_addr_t * addr)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PRV_BASE_ADDR_GET, dev_id, (a_uint32_t) addr);
    return rv;
}

sw_error_t
fal_nat_prv_addr_mode_set(a_uint32_t dev_id, a_bool_t map_en)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PRV_ADDR_MODE_SET, dev_id, (a_uint32_t) map_en);
    return rv;
}

sw_error_t
fal_nat_prv_addr_mode_get(a_uint32_t dev_id, a_bool_t * map_en)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PRV_ADDR_MODE_GET, dev_id, (a_uint32_t) map_en);
    return rv;
}

sw_error_t
fal_nat_pub_addr_add(a_uint32_t dev_id, fal_nat_pub_addr_t * entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PUB_ADDR_ENTRY_ADD, dev_id, (a_uint32_t) entry);
    return rv;
}

sw_error_t
fal_nat_pub_addr_del(a_uint32_t dev_id, a_uint32_t del_mode, fal_nat_pub_addr_t * entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PUB_ADDR_ENTRY_DEL, dev_id, del_mode, (a_uint32_t) entry);
    return rv;
}

sw_error_t
fal_nat_pub_addr_next(a_uint32_t dev_id, a_uint32_t next_mode, fal_nat_pub_addr_t * entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PUB_ADDR_ENTRY_NEXT, dev_id, next_mode, (a_uint32_t) entry);
    return rv;
}

sw_error_t
fal_nat_unk_session_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_NAT_UNK_SESSION_CMD_SET, dev_id, (a_uint32_t) cmd);
    return rv;
}

sw_error_t
fal_nat_unk_session_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_NAT_UNK_SESSION_CMD_GET, dev_id, (a_uint32_t) cmd);
    return rv;
}

