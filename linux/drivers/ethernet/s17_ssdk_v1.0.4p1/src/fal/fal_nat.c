/*
 * Copyright (c) 2007-2009 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup fal_nat FAL_NAT
 * @{
 */  
#include "sw.h"
#include "fal_nat.h"
#include "hsl_api.h"

static sw_error_t
_fal_nat_add(a_uint32_t dev_id, fal_nat_entry_t * nat_entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->nat_add)
        return SW_NOT_SUPPORTED;

    rv = p_api->nat_add(dev_id, nat_entry);
    return rv;
}

static sw_error_t
_fal_nat_del(a_uint32_t dev_id, a_uint32_t del_mode,
             fal_nat_entry_t * nat_entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->nat_del)
        return SW_NOT_SUPPORTED;

    rv = p_api->nat_del(dev_id, del_mode, nat_entry);
    return rv;
}

static sw_error_t
_fal_nat_get(a_uint32_t dev_id, a_uint32_t get_mode,
             fal_nat_entry_t * nat_entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->nat_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->nat_get(dev_id, get_mode, nat_entry);
    return rv;
}

static sw_error_t
_fal_nat_next(a_uint32_t dev_id, a_uint32_t next_mode,
              fal_nat_entry_t * nat_entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->nat_next)
        return SW_NOT_SUPPORTED;

    rv = p_api->nat_next(dev_id, next_mode, nat_entry);
    return rv;
}

static sw_error_t
_fal_nat_counter_bind(a_uint32_t dev_id, a_uint32_t entry_id, a_uint32_t cnt_id,
                      a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->nat_counter_bind)
        return SW_NOT_SUPPORTED;

    rv = p_api->nat_counter_bind(dev_id, entry_id, cnt_id, enable);
    return rv;
}

static sw_error_t
_fal_napt_add(a_uint32_t dev_id, fal_napt_entry_t * napt_entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->napt_add)
        return SW_NOT_SUPPORTED;

    rv = p_api->napt_add(dev_id, napt_entry);
    return rv;
}

static sw_error_t
_fal_napt_del(a_uint32_t dev_id, a_uint32_t del_mode,
              fal_napt_entry_t * napt_entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->napt_del)
        return SW_NOT_SUPPORTED;

    rv = p_api->napt_del(dev_id, del_mode, napt_entry);
    return rv;
}

static sw_error_t
_fal_napt_get(a_uint32_t dev_id, a_uint32_t get_mode,
              fal_napt_entry_t * napt_entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->napt_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->napt_get(dev_id, get_mode, napt_entry);
    return rv;
}

static sw_error_t
_fal_napt_next(a_uint32_t dev_id, a_uint32_t next_mode,
               fal_napt_entry_t * napt_entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->napt_next)
        return SW_NOT_SUPPORTED;

    rv = p_api->napt_next(dev_id, next_mode, napt_entry);
    return rv;
}

static sw_error_t
_fal_napt_counter_bind(a_uint32_t dev_id, a_uint32_t entry_id,
                       a_uint32_t cnt_id, a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->napt_counter_bind)
        return SW_NOT_SUPPORTED;

    rv = p_api->napt_counter_bind(dev_id, entry_id, cnt_id, enable);
    return rv;
}

static sw_error_t
_fal_nat_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->nat_status_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->nat_status_set(dev_id, enable);
    return rv;
}

static sw_error_t
_fal_nat_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->nat_status_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->nat_status_get(dev_id, enable);
    return rv;
}

static sw_error_t
_fal_nat_hash_mode_set(a_uint32_t dev_id, a_uint32_t mode)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->nat_hash_mode_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->nat_hash_mode_set(dev_id, mode);
    return rv;
}

static sw_error_t
_fal_nat_hash_mode_get(a_uint32_t dev_id, a_uint32_t * mode)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->nat_hash_mode_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->nat_hash_mode_get(dev_id, mode);
    return rv;
}

static sw_error_t
_fal_napt_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->napt_status_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->napt_status_set(dev_id, enable);
    return rv;
}

static sw_error_t
_fal_napt_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->napt_status_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->napt_status_get(dev_id, enable);
    return rv;
}

static sw_error_t
_fal_napt_mode_set(a_uint32_t dev_id, fal_napt_mode_t mode)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->napt_mode_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->napt_mode_set(dev_id, mode);
    return rv;
}

static sw_error_t
_fal_napt_mode_get(a_uint32_t dev_id, fal_napt_mode_t * mode)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->napt_mode_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->napt_mode_get(dev_id, mode);
    return rv;
}

static sw_error_t
_fal_nat_prv_base_addr_set(a_uint32_t dev_id, fal_ip4_addr_t addr)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->nat_prv_base_addr_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->nat_prv_base_addr_set(dev_id, addr);
    return rv;
}

static sw_error_t
_fal_nat_prv_base_addr_get(a_uint32_t dev_id, fal_ip4_addr_t * addr)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->nat_prv_base_addr_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->nat_prv_base_addr_get(dev_id, addr);
    return rv;
}

static sw_error_t
_fal_nat_prv_addr_mode_set(a_uint32_t dev_id, a_bool_t map_en)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->nat_prv_addr_mode_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->nat_prv_addr_mode_set(dev_id, map_en);
    return rv;
}

static sw_error_t
_fal_nat_prv_addr_mode_get(a_uint32_t dev_id, a_bool_t * map_en)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->nat_prv_addr_mode_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->nat_prv_addr_mode_get(dev_id, map_en);
    return rv;
}

static sw_error_t
_fal_nat_pub_addr_add(a_uint32_t dev_id, fal_nat_pub_addr_t * entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->nat_pub_addr_add)
        return SW_NOT_SUPPORTED;

    rv = p_api->nat_pub_addr_add(dev_id, entry);
    return rv;
}

static sw_error_t
_fal_nat_pub_addr_del(a_uint32_t dev_id, a_uint32_t del_mode,
                      fal_nat_pub_addr_t * entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->nat_pub_addr_del)
        return SW_NOT_SUPPORTED;

    rv = p_api->nat_pub_addr_del(dev_id, del_mode, entry);
    return rv;
}

static sw_error_t
_fal_nat_pub_addr_next(a_uint32_t dev_id, a_uint32_t next_mode,
                       fal_nat_pub_addr_t * entry)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->nat_pub_addr_next)
        return SW_NOT_SUPPORTED;

    rv = p_api->nat_pub_addr_next(dev_id, next_mode, entry);
    return rv;
}

static sw_error_t
_fal_nat_unk_session_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->nat_unk_session_cmd_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->nat_unk_session_cmd_set(dev_id, cmd);
    return rv;
}

static sw_error_t
_fal_nat_unk_session_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->nat_unk_session_cmd_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->nat_unk_session_cmd_get(dev_id, cmd);
    return rv;
}

sw_error_t
fal_nat_add(a_uint32_t dev_id, fal_nat_entry_t * nat_entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_nat_add(dev_id, nat_entry);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_nat_del(a_uint32_t dev_id, a_uint32_t del_mode,
             fal_nat_entry_t * nat_entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_nat_del(dev_id, del_mode, nat_entry);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_nat_get(a_uint32_t dev_id, a_uint32_t get_mode,
             fal_nat_entry_t * nat_entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_nat_get(dev_id, get_mode, nat_entry);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_nat_next(a_uint32_t dev_id, a_uint32_t next_mode,
              fal_nat_entry_t * nat_entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_nat_next(dev_id, next_mode, nat_entry);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_nat_counter_bind(a_uint32_t dev_id, a_uint32_t entry_id, a_uint32_t cnt_id,
                      a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_nat_counter_bind(dev_id, entry_id, cnt_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_napt_add(a_uint32_t dev_id, fal_napt_entry_t * napt_entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_napt_add(dev_id, napt_entry);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_napt_del(a_uint32_t dev_id, a_uint32_t del_mode,
              fal_napt_entry_t * napt_entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_napt_del(dev_id, del_mode, napt_entry);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_napt_get(a_uint32_t dev_id, a_uint32_t get_mode,
              fal_napt_entry_t * napt_entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_napt_get(dev_id, get_mode, napt_entry);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_napt_next(a_uint32_t dev_id, a_uint32_t next_mode,
               fal_napt_entry_t * napt_entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_napt_next(dev_id, next_mode, napt_entry);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_napt_counter_bind(a_uint32_t dev_id, a_uint32_t entry_id,
                       a_uint32_t cnt_id, a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_napt_counter_bind(dev_id, entry_id, cnt_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_nat_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_nat_status_set(dev_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_nat_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_nat_status_get(dev_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_nat_hash_mode_set(a_uint32_t dev_id, a_uint32_t mode)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_nat_hash_mode_set(dev_id, mode);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_nat_hash_mode_get(a_uint32_t dev_id, a_uint32_t * mode)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_nat_hash_mode_get(dev_id, mode);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_napt_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_napt_status_set(dev_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_napt_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_napt_status_get(dev_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_napt_mode_set(a_uint32_t dev_id, fal_napt_mode_t mode)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_napt_mode_set(dev_id, mode);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_napt_mode_get(a_uint32_t dev_id, fal_napt_mode_t * mode)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_napt_mode_get(dev_id, mode);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_nat_prv_base_addr_set(a_uint32_t dev_id, fal_ip4_addr_t addr)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_nat_prv_base_addr_set(dev_id, addr);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_nat_prv_base_addr_get(a_uint32_t dev_id, fal_ip4_addr_t * addr)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_nat_prv_base_addr_get(dev_id, addr);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_nat_prv_addr_mode_set(a_uint32_t dev_id, a_bool_t map_en)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_nat_prv_addr_mode_set(dev_id, map_en);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_nat_prv_addr_mode_get(a_uint32_t dev_id, a_bool_t * map_en)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_nat_prv_addr_mode_get(dev_id, map_en);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_nat_pub_addr_add(a_uint32_t dev_id, fal_nat_pub_addr_t * entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_nat_pub_addr_add(dev_id, entry);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_nat_pub_addr_del(a_uint32_t dev_id, a_uint32_t del_mode,
                      fal_nat_pub_addr_t * entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_nat_pub_addr_del(dev_id, del_mode, entry);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_nat_pub_addr_next(a_uint32_t dev_id, a_uint32_t next_mode,
                       fal_nat_pub_addr_t * entry)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_nat_pub_addr_next(dev_id, next_mode, entry);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_nat_unk_session_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_nat_unk_session_cmd_set(dev_id, cmd);
    FAL_API_UNLOCK;
    return rv;
}

sw_error_t
fal_nat_unk_session_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_nat_unk_session_cmd_get(dev_id, cmd);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @}
 */
