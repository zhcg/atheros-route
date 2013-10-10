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
_fal_ip_age_time_set(a_uint32_t dev_id, a_uint32_t * time)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_age_time_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_age_time_set(dev_id, time);
    return rv;
}

static sw_error_t
_fal_ip_age_time_get(a_uint32_t dev_id, a_uint32_t * time)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_age_time_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_age_time_get(dev_id, time);
    return rv;
}

static sw_error_t
_fal_ip_wcmp_hash_mode_set(a_uint32_t dev_id, a_uint32_t hash_mode)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_wcmp_hash_mode_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_wcmp_hash_mode_set(dev_id, hash_mode);
    return rv;
}

static sw_error_t
_fal_ip_wcmp_hash_mode_get(a_uint32_t dev_id, a_uint32_t * hash_mode)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ip_wcmp_hash_mode_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->ip_wcmp_hash_mode_get(dev_id, hash_mode);
    return rv;
}

/**
 * @brief Add one host entry to one particular device.
 *   @details Comments:
 *     For ISIS the intf_id parameter in host_entry means vlan id.
       Before host entry added related interface entry and ip6 base address
       must be set at first.
       Hardware entry id will be returned.
 * @param[in] dev_id device id
 * @param[in] host_entry host entry parameter
 * @return SW_OK or error code
 */
sw_error_t
fal_ip_host_add(a_uint32_t dev_id, fal_host_entry_t * host_entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_host_add(dev_id, host_entry);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Delete one host entry from one particular device.
 *   @details Comments:
 *     For ISIS the intf_id parameter in host_entry means vlan id.
       Before host entry deleted related interface entry and ip6 base address
       must be set atfirst.
       For del_mode please refer IP entry operation flags.
 * @param[in] dev_id device id
 * @param[in] del_mode delete operation mode
 * @param[in] host_entry host entry parameter
 * @return SW_OK or error code
 */
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

/**
 * @brief Get one host entry from one particular device.
 *   @details Comments:
 *     For ISIS the intf_id parameter in host_entry means vlan id.
       Before host entry deleted related interface entry and ip6 base address
       must be set atfirst.
       For get_mode please refer IP entry operation flags.
 * @param[in] dev_id device id
 * @param[in] get_mode get operation mode
 * @param[out] host_entry host entry parameter
 * @return SW_OK or error code
 */
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

/**
 * @brief Next one host entry from one particular device.
 *   @details Comments:
 *     For ISIS the intf_id parameter in host_entry means vlan id.
       Before host entry deleted related interface entry and ip6 base address
       must be set atfirst.
       For next_mode please refer IP entry operation flags.
       For get the first entry please set entry id as FAL_NEXT_ENTRY_FIRST_ID
 * @param[in] dev_id device id
 * @param[in] next_mode next operation mode
 * @param[out] host_entry host entry parameter
 * @return SW_OK or error code
 */
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

/**
 * @brief Bind one counter entry to one host entry on one particular device.
 * @param[in] dev_id device id
 * @param[in] entry_id host entry id
 * @param[in] cnt_id counter entry id
 * @param[in] enable A_TRUE means bind, A_FALSE means unbind 
 * @return SW_OK or error code
 */
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

/**
 * @brief Bind one pppoe session entry to one host entry on one particular device.
 * @param[in] dev_id device id
 * @param[in] entry_id host entry id
 * @param[in] pppoe_id pppoe session entry id
 * @param[in] enable A_TRUE means bind, A_FALSE means unbind 
 * @return SW_OK or error code
 */
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

/**
 * @brief Set arp packets type to learn on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] flags arp type FAL_ARP_LEARN_REQ and/or FAL_ARP_LEARN_ACK
 * @return SW_OK or error code
 */
sw_error_t
fal_ip_pt_arp_learn_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t flags)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_pt_arp_learn_set(dev_id, port_id, flags);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get arp packets type to learn on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] flags arp type FAL_ARP_LEARN_REQ and/or FAL_ARP_LEARN_ACK
 * @return SW_OK or error code
 */
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

/**
 * @brief Set arp packets type to learn on one particular device.
 * @param[in] dev_id device id
 * @param[in] mode learning mode
 * @return SW_OK or error code
 */
sw_error_t
fal_ip_arp_learn_set(a_uint32_t dev_id, fal_arp_learn_mode_t mode)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_arp_learn_set(dev_id, mode);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get arp packets type to learn on one particular device.
 * @param[in] dev_id device id
 * @param[out] mode learning mode
 * @return SW_OK or error code
 */
sw_error_t
fal_ip_arp_learn_get(a_uint32_t dev_id, fal_arp_learn_mode_t * mode)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_arp_learn_get(dev_id, mode);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set ip packets source guarding mode on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] mode source guarding mode
 * @return SW_OK or error code
 */
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

/**
 * @brief Get ip packets source guarding mode on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] mode source guarding mode
 * @return SW_OK or error code
 */
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

/**
 * @brief Set unkonw source ip packets forwarding command on one particular device.
 *   @details Comments:
 *     This settin is no meaning when ip source guard not enable
 * @param[in] dev_id device id
 * @param[in] cmd forwarding command
 * @return SW_OK or error code
 */
sw_error_t
fal_ip_unk_source_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_unk_source_cmd_set(dev_id, cmd);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get unkonw source ip packets forwarding command on one particular device.
 * @param[in] dev_id device id
 * @param[out] cmd forwarding command
 * @return SW_OK or error code
 */
sw_error_t
fal_ip_unk_source_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_unk_source_cmd_get(dev_id, cmd);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set arp packets source guarding mode on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] mode source guarding mode
 * @return SW_OK or error code
 */
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

/**
 * @brief Get arp packets source guarding mode on one particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] mode source guarding mode
 * @return SW_OK or error code
 */
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

/**
 * @brief Set unkonw source arp packets forwarding command on one particular device.
 *   @details Comments:
 *     This settin is no meaning when arp source guard not enable
 * @param[in] dev_id device id
 * @param[in] cmd forwarding command
 * @return SW_OK or error code
 */
sw_error_t
fal_arp_unk_source_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_arp_unk_source_cmd_set(dev_id, cmd);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get unkonw source arp packets forwarding command on one particular device.
 * @param[in] dev_id device id
 * @param[out] cmd forwarding command
 * @return SW_OK or error code
 */
sw_error_t
fal_arp_unk_source_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_arp_unk_source_cmd_get(dev_id, cmd);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set IP unicast routing status on one particular device.
 * @param[in] dev_id device id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_ip_route_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_route_status_set(dev_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get IP unicast routing status on one particular device.
 * @param[in] dev_id device id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_ip_route_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_route_status_get(dev_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Add one interface entry to one particular device.
 * @param[in] dev_id device id
 * @param[in] entry interface entry
 * @return SW_OK or error code
 */
sw_error_t
fal_ip_intf_entry_add(a_uint32_t dev_id, fal_intf_mac_entry_t * entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_intf_entry_add(dev_id, entry);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Delete one interface entry from one particular device.
 * @param[in] dev_id device id
 * @param[in] del_mode delete operation mode
 * @param[in] entry interface entry
 * @return SW_OK or error code
 */
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

/**
 * @brief Next one interface entry from one particular device.
 * @param[in] dev_id device id
 * @param[in] next_mode next operation mode
 * @param[out] entry interface entry
 * @return SW_OK or error code
 */
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

/**
 * @brief Set IP host entry aging time on one particular device.
 * @details   Comments:
 *       This operation will set dynamic entry aging time on a particular device.
 *       The unit of time is second. Because different device has differnet
 *       hardware granularity function will return actual time in hardware.
 * @param[in] dev_id device id
 * @param[in] time aging time
 * @param[out] time actual aging time
 * @return SW_OK or error code
 */
sw_error_t
fal_ip_age_time_set(a_uint32_t dev_id, a_uint32_t * time)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_age_time_set(dev_id, time);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get IP host entry aging time on one particular device.
 * @param[in] dev_id device id
 * @param[out] time aging time
 * @return SW_OK or error code
 */
sw_error_t
fal_ip_age_time_get(a_uint32_t dev_id, a_uint32_t * time)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_age_time_get(dev_id, time);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set IP WCMP hash key mode.
 * @param[in] dev_id device id
 * @param[in] hash_mode hash mode
 * @return SW_OK or error code
 */
sw_error_t
fal_ip_wcmp_hash_mode_set(a_uint32_t dev_id, a_uint32_t hash_mode)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_wcmp_hash_mode_set(dev_id, hash_mode);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get IP WCMP hash key mode.
 * @param[in] dev_id device id
 * @param[out] hash_mode hash mode
 * @return SW_OK or error code
 */
sw_error_t
fal_ip_wcmp_hash_mode_get(a_uint32_t dev_id, a_uint32_t * hash_mode)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ip_wcmp_hash_mode_get(dev_id, hash_mode);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @}
 */
