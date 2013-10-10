/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#include "sw.h"
#include "sw_ioctl.h"
#include "fal_ip.h"
#include "fal_uk_if.h"


sw_error_t
fal_ip_host_add(a_uint32_t dev_id, fal_host_entry_t * host_entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_HOST_ADD, dev_id, (a_uint32_t) host_entry);
    return rv;
}

sw_error_t
fal_ip_host_del(a_uint32_t dev_id, a_uint32_t del_mode, fal_host_entry_t * host_entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_HOST_DEL, dev_id, del_mode, (a_uint32_t) host_entry);
    return rv;
}

sw_error_t
fal_ip_host_get(a_uint32_t dev_id, a_uint32_t get_mode, fal_host_entry_t * host_entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_HOST_GET, dev_id, get_mode, (a_uint32_t) host_entry);
    return rv;
}

sw_error_t
fal_ip_host_next(a_uint32_t dev_id, a_uint32_t next_mode, fal_host_entry_t * host_entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_HOST_NEXT, dev_id, next_mode, (a_uint32_t) host_entry);
    return rv;
}

sw_error_t
fal_ip_host_counter_bind(a_uint32_t dev_id, a_uint32_t entry_id, a_uint32_t cnt_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_HOST_COUNTER_BIND, dev_id, entry_id, cnt_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_ip_host_pppoe_bind(a_uint32_t dev_id, a_uint32_t entry_id, a_uint32_t pppoe_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_HOST_PPPOE_BIND, dev_id, entry_id, pppoe_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_ip_pt_arp_learn_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t flags)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_PT_ARP_LEARN_SET, dev_id, port_id, flags);
    return rv;
}

sw_error_t
fal_ip_pt_arp_learn_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * flags)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_PT_ARP_LEARN_GET, dev_id, port_id, (a_uint32_t)flags);
    return rv;
}

sw_error_t
fal_ip_arp_learn_set(a_uint32_t dev_id, fal_arp_learn_mode_t mode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_ARP_LEARN_SET, dev_id, (a_uint32_t)mode);
    return rv;
}

sw_error_t
fal_ip_arp_learn_get(a_uint32_t dev_id, fal_arp_learn_mode_t * mode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_ARP_LEARN_GET, dev_id, (a_uint32_t)mode);
    return rv;
}

sw_error_t
fal_ip_source_guard_set(a_uint32_t dev_id, fal_port_t port_id, fal_source_guard_mode_t mode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_SOURCE_GUARD_SET, dev_id, port_id, (a_uint32_t)mode);
    return rv;
}

sw_error_t
fal_ip_source_guard_get(a_uint32_t dev_id, fal_port_t port_id, fal_source_guard_mode_t * mode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_SOURCE_GUARD_GET, dev_id, port_id, (a_uint32_t)mode);
    return rv;
}

sw_error_t
fal_ip_arp_guard_set(a_uint32_t dev_id, fal_port_t port_id, fal_source_guard_mode_t mode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_ARP_GUARD_SET, dev_id, port_id, (a_uint32_t)mode);
    return rv;
}

sw_error_t
fal_ip_arp_guard_get(a_uint32_t dev_id, fal_port_t port_id, fal_source_guard_mode_t * mode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_ARP_GUARD_GET, dev_id, port_id, (a_uint32_t)mode);
    return rv;
}

sw_error_t
fal_ip_route_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_ROUTE_STATUS_SET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_ip_route_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_ROUTE_STATUS_GET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_ip_intf_entry_add(a_uint32_t dev_id, fal_intf_mac_entry_t * entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_INTF_ENTRY_ADD, dev_id, (a_uint32_t) entry);
    return rv;    
}

sw_error_t
fal_ip_intf_entry_del(a_uint32_t dev_id, a_uint32_t del_mode, fal_intf_mac_entry_t * entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_INTF_ENTRY_DEL, dev_id, del_mode, (a_uint32_t) entry);
    return rv;
}

sw_error_t
fal_ip_intf_entry_next(a_uint32_t dev_id, a_uint32_t next_mode, fal_intf_mac_entry_t * entry)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_INTF_ENTRY_NEXT, dev_id, next_mode, (a_uint32_t) entry);
    return rv;
}

sw_error_t
fal_ip_unk_source_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_UNK_SOURCE_CMD_SET, dev_id, (a_uint32_t) cmd);
    return rv;
}

sw_error_t
fal_ip_unk_source_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_UNK_SOURCE_CMD_GET, dev_id, (a_uint32_t) cmd);
    return rv;
}

sw_error_t
fal_arp_unk_source_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_ARP_UNK_SOURCE_CMD_SET, dev_id, (a_uint32_t) cmd);
    return rv;
}

sw_error_t
fal_arp_unk_source_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_ARP_UNK_SOURCE_CMD_GET, dev_id, (a_uint32_t) cmd);
    return rv;
}

sw_error_t
fal_ip_age_time_set(a_uint32_t dev_id, a_uint32_t * time)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_AGE_TIME_SET, dev_id, (a_uint32_t) time);
    return rv;
}

sw_error_t
fal_ip_age_time_get(a_uint32_t dev_id, a_uint32_t * time)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_IP_AGE_TIME_GET, dev_id, (a_uint32_t) time);
    return rv;
}

sw_error_t
fal_ip_wcmp_hash_mode_set(a_uint32_t dev_id, a_uint32_t hash_mode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_WCMP_HASH_MODE_SET, dev_id, hash_mode);
    return rv;
}

sw_error_t
fal_ip_wcmp_hash_mode_get(a_uint32_t dev_id, a_uint32_t * hash_mode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_WCMP_HASH_MODE_GET, dev_id, (a_uint32_t) hash_mode);
    return rv;
}

