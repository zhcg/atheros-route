/*
 * Copyright (c) 2007-2009 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup fal_ip FAL_IP
 * @{
 */
#include "sw.h"
#include "fal_ip.h"
#include "hsl_api.h"

static sw_error_t
_fal_ip_host_add(a_uint32_t dev_id, fal_host_entry_t * host_entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_host_add)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_host_add(dev_id, host_entry);
    return rv;
}

static sw_error_t
_fal_ip_host_del(a_uint32_t dev_id, a_uint32_t del_mode,
                fal_host_entry_t * host_entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_host_del)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_host_del(dev_id, del_mode, host_entry);
    return rv;
}

static sw_error_t
_fal_ip_host_get(a_uint32_t dev_id, a_uint32_t get_mode,
                fal_host_entry_t * host_entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_host_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_host_get(dev_id, get_mode, host_entry);
    return rv;
}

static sw_error_t
_fal_ip_host_next(a_uint32_t dev_id, a_uint32_t next_mode,
                 fal_host_entry_t * host_entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_host_next)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_host_next(dev_id, next_mode, host_entry);
    return rv;
}

static sw_error_t
_fal_ip_host_counter_bind(a_uint32_t dev_id, a_uint32_t entry_id,
                         a_uint32_t cnt_id, a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_host_counter_bind)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_host_counter_bind(dev_id, entry_id, cnt_id, enable);
    return rv;
}

static sw_error_t
_fal_ip_host_pppoe_bind(a_uint32_t dev_id, a_uint32_t entry_id,
                       a_uint32_t pppoe_id, a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_host_pppoe_bind)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_host_pppoe_bind(dev_id, entry_id, pppoe_id, enable);
    return rv;
}

static sw_error_t
_fal_ip_pt_arp_learn_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t flags)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_pt_arp_learn_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_pt_arp_learn_set(dev_id, port_id, flags);
    return rv;
}

static sw_error_t
_fal_ip_pt_arp_learn_get(a_uint32_t dev_id, fal_port_t port_id,
                        a_uint32_t * flags)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_pt_arp_learn_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_pt_arp_learn_get(dev_id, port_id, flags);
    return rv;
}

static sw_error_t
_fal_ip_arp_learn_set(a_uint32_t dev_id, fal_arp_learn_mode_t mode)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_arp_learn_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_arp_learn_set(dev_id, mode);
    return rv;
}

static sw_error_t
_fal_ip_arp_learn_get(a_uint32_t dev_id, fal_arp_learn_mode_t * mode)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_arp_learn_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_arp_learn_get(dev_id, mode);
    return rv;
}

static sw_error_t
_fal_ip_source_guard_set(a_uint32_t dev_id, fal_port_t port_id,
                        fal_source_guard_mode_t mode)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_source_guard_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_source_guard_set(dev_id, port_id, mode);
    return rv;
}

static sw_error_t
_fal_ip_source_guard_get(a_uint32_t dev_id, fal_port_t port_id,
                        fal_source_guard_mode_t * mode)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_source_guard_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_source_guard_get(dev_id, port_id, mode);
    return rv;
}

static sw_error_t
_fal_ip_unk_source_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_unk_source_cmd_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_unk_source_cmd_set(dev_id, cmd);
    return rv;
}

static sw_error_t
_fal_ip_unk_source_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_unk_source_cmd_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_unk_source_cmd_get(dev_id, cmd);
    return rv;
}

static sw_error_t
_fal_ip_arp_guard_set(a_uint32_t dev_id, fal_port_t port_id,
                     fal_source_guard_mode_t mode)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_arp_guard_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_arp_guard_set(dev_id, port_id, mode);
    return rv;
}

static sw_error_t
_fal_ip_arp_guard_get(a_uint32_t dev_id, fal_port_t port_id,
                     fal_source_guard_mode_t * mode)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_arp_guard_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_arp_guard_get(dev_id, port_id, mode);
    return rv;
}

static sw_error_t
_fal_arp_unk_source_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->arp_unk_source_cmd_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->arp_unk_source_cmd_set(dev_id, cmd);
    return rv;
}

static sw_error_t
_fal_arp_unk_source_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->arp_unk_source_cmd_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->arp_unk_source_cmd_get(dev_id, cmd);
    return rv;
}

static sw_error_t
_fal_ip_route_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_route_status_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_route_status_set(dev_id, enable);
    return rv;
}

static sw_error_t
_fal_ip_route_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_route_status_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_route_status_get(dev_id, enable);
    return rv;
}

static sw_error_t
_fal_ip_intf_entry_add(a_uint32_t dev_id, fal_intf_mac_entry_t * entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_intf_entry_add)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_intf_entry_add(dev_id, entry);
    return rv;
}

static sw_error_t
_fal_ip_intf_entry_del(a_uint32_t dev_id, a_uint32_t del_mode,
                      fal_intf_mac_entry_t * entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_intf_entry_del)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_intf_entry_del(dev_id, del_mode, entry);
    return rv;
}

static sw_error_t
_fal_ip_intf_entry_next(a_uint32_t dev_id, a_uint32_t next_mode,
                       fal_intf_mac_entry_t * entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_intf_entry_next)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_intf_entry_next(dev_id, next_mode, entry);
    return rv;
}

static sw_error_t
_fal_ip6_base_addr_set(a_uint32_t dev_id, fal_ip6_addr_t * addr)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip6_base_addr_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip6_base_addr_set(dev_id, addr);
    return rv;
}

static sw_error_t
_fal_ip6_base_addr_get(a_uint32_t dev_id, fal_ip6_addr_t * addr)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip6_base_addr_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip6_base_addr_get(dev_id, addr);
    return rv;
}

sw_error_t
fal_ip_host_add(a_uint32_t dev_id, fal_host_entry_t * host_entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_host_add(dev_id, host_entry);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip_host_del(a_uint32_t dev_id, a_uint32_t del_mode,
                fal_host_entry_t * host_entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_host_del(dev_id, del_mode, host_entry);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip_host_get(a_uint32_t dev_id, a_uint32_t get_mode,
                fal_host_entry_t * host_entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_host_get(dev_id, get_mode, host_entry);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip_host_next(a_uint32_t dev_id, a_uint32_t next_mode,
                 fal_host_entry_t * host_entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_host_next(dev_id, next_mode, host_entry);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip_host_counter_bind(a_uint32_t dev_id, a_uint32_t entry_id,
                         a_uint32_t cnt_id, a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_host_counter_bind(dev_id, entry_id, cnt_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip_host_pppoe_bind(a_uint32_t dev_id, a_uint32_t entry_id,
                       a_uint32_t pppoe_id, a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_host_pppoe_bind(dev_id, entry_id, pppoe_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip_pt_arp_learn_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t flags)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_pt_arp_learn_set(dev_id, port_id, flags);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip_pt_arp_learn_get(a_uint32_t dev_id, fal_port_t port_id,
                        a_uint32_t * flags)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_pt_arp_learn_get(dev_id, port_id, flags);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip_arp_learn_set(a_uint32_t dev_id, fal_arp_learn_mode_t mode)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_arp_learn_set(dev_id, mode);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip_arp_learn_get(a_uint32_t dev_id, fal_arp_learn_mode_t * mode)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_arp_learn_get(dev_id, mode);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip_source_guard_set(a_uint32_t dev_id, fal_port_t port_id,
                        fal_source_guard_mode_t mode)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_source_guard_set(dev_id, port_id, mode);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip_source_guard_get(a_uint32_t dev_id, fal_port_t port_id,
                        fal_source_guard_mode_t * mode)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_source_guard_get(dev_id, port_id, mode);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip_unk_source_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_unk_source_cmd_set(dev_id, cmd);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip_unk_source_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_unk_source_cmd_get(dev_id, cmd);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip_arp_guard_set(a_uint32_t dev_id, fal_port_t port_id,
                     fal_source_guard_mode_t mode)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_arp_guard_set(dev_id, port_id, mode);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip_arp_guard_get(a_uint32_t dev_id, fal_port_t port_id,
                     fal_source_guard_mode_t * mode)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_arp_guard_get(dev_id, port_id, mode);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_arp_unk_source_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_arp_unk_source_cmd_set(dev_id, cmd);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_arp_unk_source_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_arp_unk_source_cmd_get(dev_id, cmd);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip_route_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_route_status_set(dev_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip_route_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_route_status_get(dev_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip_intf_entry_add(a_uint32_t dev_id, fal_intf_mac_entry_t * entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_intf_entry_add(dev_id, entry);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip_intf_entry_del(a_uint32_t dev_id, a_uint32_t del_mode,
                      fal_intf_mac_entry_t * entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_intf_entry_del(dev_id, del_mode, entry);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip_intf_entry_next(a_uint32_t dev_id, a_uint32_t next_mode,
                       fal_intf_mac_entry_t * entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_intf_entry_next(dev_id, next_mode, entry);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip6_base_addr_set(a_uint32_t dev_id, fal_ip6_addr_t * addr)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip6_base_addr_set(dev_id, addr);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_ip6_base_addr_get(a_uint32_t dev_id, fal_ip6_addr_t * addr)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip6_base_addr_get(dev_id, addr);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @}
 */
