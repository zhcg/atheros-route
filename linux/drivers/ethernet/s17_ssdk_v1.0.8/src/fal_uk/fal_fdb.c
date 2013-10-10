/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#include "sw.h"
#include "sw_ioctl.h"
#include "fal_fdb.h"
#include "fal_uk_if.h"

sw_error_t
fal_fdb_add(a_uint32_t dev_id, const fal_fdb_entry_t * entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_ADD, dev_id, (a_uint32_t) entry);
    return rv;
}

sw_error_t
fal_fdb_del_all(a_uint32_t dev_id, a_uint32_t flag)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_DELALL, dev_id, flag);
    return rv;
}

sw_error_t
fal_fdb_del_by_port(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t flag)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_DELPORT, dev_id, port_id, flag);
    return rv;
}

sw_error_t
fal_fdb_del_by_mac(a_uint32_t dev_id, const fal_fdb_entry_t * entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_DELMAC, dev_id, (a_uint32_t) entry);
    return rv;
}

sw_error_t
fal_fdb_first(a_uint32_t dev_id, fal_fdb_entry_t * entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_FIRST, dev_id, (a_uint32_t) entry);
    return rv;
}

sw_error_t
fal_fdb_next(a_uint32_t dev_id, fal_fdb_entry_t * entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_NEXT, dev_id, (a_uint32_t) entry);
    return rv;
}

sw_error_t
fal_fdb_find(a_uint32_t dev_id, fal_fdb_entry_t * entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_FIND, dev_id, (a_uint32_t) entry);
    return rv;
}

sw_error_t
fal_fdb_port_learn_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_PT_LEARN_SET, dev_id, port_id,
                       (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_fdb_port_learn_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_PT_LEARN_GET, dev_id, port_id,
                       (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_fdb_age_ctrl_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_AGE_CTRL_SET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_fdb_age_ctrl_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_AGE_CTRL_GET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_fdb_age_time_set(a_uint32_t dev_id, a_uint32_t * time)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_AGE_TIME_SET, dev_id, (a_uint32_t) time);
    return rv;
}

sw_error_t
fal_fdb_age_time_get(a_uint32_t dev_id, a_uint32_t * time)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_AGE_TIME_GET, dev_id, (a_uint32_t) time);
    return rv;
}

sw_error_t
fal_fdb_iterate(a_uint32_t dev_id, a_uint32_t * iterator, fal_fdb_entry_t * entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_ITERATE, dev_id, (a_uint32_t) iterator, (a_uint32_t) entry);
    return rv;
}

sw_error_t
fal_fdb_extend_next(a_uint32_t dev_id, fal_fdb_op_t * option,
                     fal_fdb_entry_t * entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_EXTEND_NEXT, dev_id, (a_uint32_t)option, (a_uint32_t)entry);
    return rv;
}

sw_error_t
fal_fdb_extend_first(a_uint32_t dev_id, fal_fdb_op_t * option,
                     fal_fdb_entry_t * entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_EXTEND_FIRST, dev_id, (a_uint32_t)option, (a_uint32_t)entry);
    return rv;
}

sw_error_t
fal_fdb_transfer(a_uint32_t dev_id, fal_port_t old_port, fal_port_t new_port,
                                            a_uint32_t fid, fal_fdb_op_t * option)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_TRANSFER, dev_id, old_port, new_port, fid, (a_uint32_t)option);
    return rv;
}

sw_error_t
fal_port_fdb_learn_limit_set(a_uint32_t dev_id, fal_port_t port_id,
                              a_bool_t enable, a_uint32_t cnt)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_FDB_LEARN_LIMIT_SET, dev_id, port_id, enable, cnt);
    return rv;
}

sw_error_t
fal_port_fdb_learn_limit_get(a_uint32_t dev_id, fal_port_t port_id,
                              a_bool_t * enable, a_uint32_t * cnt)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_FDB_LEARN_LIMIT_GET, dev_id, port_id, (a_uint32_t)enable, (a_uint32_t)cnt);
    return rv;
}

sw_error_t
fal_port_fdb_learn_exceed_cmd_set(a_uint32_t dev_id, fal_port_t port_id,
                                   fal_fwd_cmd_t cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_FDB_LEARN_EXCEED_CMD_SET, dev_id, port_id, (a_uint32_t)cmd);
    return rv;
}

sw_error_t
fal_port_fdb_learn_exceed_cmd_get(a_uint32_t dev_id, fal_port_t port_id,
                                   fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_FDB_LEARN_EXCEED_CMD_GET, dev_id, port_id, (a_uint32_t)cmd);
    return rv;
}

sw_error_t
fal_fdb_learn_limit_set(a_uint32_t dev_id, a_bool_t enable, a_uint32_t cnt)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_LEARN_LIMIT_SET, dev_id, enable, cnt);
    return rv;
}

sw_error_t
fal_fdb_learn_limit_get(a_uint32_t dev_id, a_bool_t * enable, a_uint32_t * cnt)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_LEARN_LIMIT_GET, dev_id, (a_uint32_t)enable, (a_uint32_t)cnt);
    return rv;
}

sw_error_t
fal_fdb_learn_exceed_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_LEARN_EXCEED_CMD_SET, dev_id, (a_uint32_t)cmd);
    return rv;
}

sw_error_t
fal_fdb_learn_exceed_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_LEARN_EXCEED_CMD_GET, dev_id, (a_uint32_t)cmd);
    return rv;
}

sw_error_t
fal_fdb_resv_add(a_uint32_t dev_id, fal_fdb_entry_t * entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_RESV_ADD, dev_id, (a_uint32_t)entry);
    return rv;
}

sw_error_t
fal_fdb_resv_del(a_uint32_t dev_id, fal_fdb_entry_t * entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_RESV_DEL, dev_id, (a_uint32_t)entry);
    return rv;
}

sw_error_t
fal_fdb_resv_find(a_uint32_t dev_id, fal_fdb_entry_t * entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_RESV_FIND, dev_id, (a_uint32_t)entry);
    return rv;
}

sw_error_t
fal_fdb_resv_iterate(a_uint32_t dev_id, a_uint32_t * iterator, fal_fdb_entry_t * entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_RESV_ITERATE, dev_id, (a_uint32_t)iterator, (a_uint32_t)entry);
    return rv;
}

sw_error_t
fal_fdb_port_learn_static_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_PT_LEARN_STATIC_SET, dev_id, port_id, enable);
    return rv;
}

sw_error_t
fal_fdb_port_learn_static_get(a_uint32_t dev_id, fal_port_t port_id,
                        a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_PT_LEARN_STATIC_GET, dev_id, port_id, (a_uint32_t)enable);
    return rv;
}

sw_error_t
fal_fdb_port_add(a_uint32_t dev_id, a_uint32_t fid, fal_mac_addr_t * addr, fal_port_t port_id)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_PORT_ADD, dev_id, fid, addr, port_id);
    return rv;
}

sw_error_t
fal_fdb_port_del(a_uint32_t dev_id, a_uint32_t fid, fal_mac_addr_t * addr, fal_port_t port_id)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FDB_PORT_DEL, dev_id, fid, addr, port_id);
    return rv;
}

