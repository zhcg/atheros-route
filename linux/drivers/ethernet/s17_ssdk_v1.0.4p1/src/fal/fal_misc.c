/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

 /**
 * @defgroup fal_gen FAL_MISC
 * @{
 */ 
#include "sw.h"
#include "fal_misc.h"
#include "hsl_api.h"

static sw_error_t
_fal_arp_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->arp_status_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->arp_status_set(dev_id, enable);
    return rv;
}


static sw_error_t
_fal_arp_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->arp_status_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->arp_status_get(dev_id, enable);
    return rv;
}


static sw_error_t
_fal_frame_max_size_set(a_uint32_t dev_id, a_uint32_t size)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->frame_max_size_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->frame_max_size_set(dev_id, size);
    return rv;
}


static sw_error_t
_fal_frame_max_size_get(a_uint32_t dev_id, a_uint32_t * size)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->frame_max_size_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->frame_max_size_get(dev_id, size);
    return rv;
}


static sw_error_t
_fal_port_unk_sa_cmd_set(a_uint32_t dev_id, fal_port_t port_id,
                        fal_fwd_cmd_t cmd)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->port_unk_sa_cmd_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->port_unk_sa_cmd_set(dev_id, port_id, cmd);
    return rv;
}


static sw_error_t
_fal_port_unk_sa_cmd_get(a_uint32_t dev_id, fal_port_t port_id,
                        fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->port_unk_sa_cmd_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->port_unk_sa_cmd_get(dev_id, port_id, cmd);
    return rv;
}


static sw_error_t
_fal_port_unk_uc_filter_set(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->port_unk_uc_filter_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->port_unk_uc_filter_set(dev_id, port_id, enable);
    return rv;
}


static sw_error_t
_fal_port_unk_uc_filter_get(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t * enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->port_unk_uc_filter_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->port_unk_uc_filter_get(dev_id, port_id, enable);
    return rv;
}


static sw_error_t
_fal_port_unk_mc_filter_set(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->port_unk_mc_filter_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->port_unk_mc_filter_set(dev_id, port_id, enable);
    return rv;
}


static sw_error_t
_fal_port_unk_mc_filter_get(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t * enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->port_unk_mc_filter_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->port_unk_mc_filter_get(dev_id, port_id, enable);
    return rv;
}


static sw_error_t
_fal_port_bc_filter_set(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->port_bc_filter_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->port_bc_filter_set(dev_id, port_id, enable);
    return rv;
}


static sw_error_t
_fal_port_bc_filter_get(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t * enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->port_bc_filter_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->port_bc_filter_get(dev_id, port_id, enable);
    return rv;
}


static sw_error_t
_fal_cpu_port_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->cpu_port_status_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->cpu_port_status_set(dev_id, enable);
    return rv;
}


static sw_error_t
_fal_cpu_port_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->cpu_port_status_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->cpu_port_status_get(dev_id, enable);
    return rv;
}


static sw_error_t
_fal_bc_to_cpu_port_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->bc_to_cpu_port_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->bc_to_cpu_port_set(dev_id, enable);
    return rv;
}


static sw_error_t
_fal_bc_to_cpu_port_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->bc_to_cpu_port_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->bc_to_cpu_port_get(dev_id, enable);
    return rv;
}


static sw_error_t
_fal_pppoe_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->pppoe_cmd_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->pppoe_cmd_set(dev_id, cmd);
    return rv;
}


static sw_error_t
_fal_pppoe_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->pppoe_cmd_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->pppoe_cmd_get(dev_id, cmd);
    return rv;
}


static sw_error_t
_fal_pppoe_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->pppoe_status_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->pppoe_status_set(dev_id, enable);
    return rv;
}

static sw_error_t
_fal_pppoe_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->pppoe_status_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->pppoe_status_get(dev_id, enable);
    return rv;
}

static sw_error_t
_fal_port_dhcp_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->port_dhcp_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->port_dhcp_set(dev_id, port_id, enable);
    return rv;
}

static sw_error_t
_fal_port_dhcp_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->port_dhcp_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->port_dhcp_get(dev_id, port_id, enable);
    return rv;
}

static sw_error_t
_fal_arp_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->arp_cmd_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->arp_cmd_set(dev_id, cmd);
    return rv;
}

static sw_error_t
_fal_arp_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->arp_cmd_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->arp_cmd_get(dev_id, cmd);
    return rv;
}

static sw_error_t
_fal_eapol_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->eapol_cmd_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->eapol_cmd_set(dev_id, cmd);
    return rv;
}

static sw_error_t
_fal_eapol_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->eapol_cmd_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->eapol_cmd_get(dev_id, cmd);
    return rv;
}

static sw_error_t
_fal_pppoe_session_add(a_uint32_t dev_id, a_uint32_t session_id, a_bool_t strip_hdr)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->pppoe_session_add)
        return SW_NOT_SUPPORTED;

    rv = p_api->pppoe_session_add(dev_id, session_id, strip_hdr);
    return rv;
}

static sw_error_t
_fal_pppoe_session_del(a_uint32_t dev_id, a_uint32_t session_id)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->pppoe_session_del)
        return SW_NOT_SUPPORTED;

    rv = p_api->pppoe_session_del(dev_id, session_id);
    return rv;
}

static sw_error_t
_fal_pppoe_session_get(a_uint32_t dev_id, a_uint32_t session_id, a_bool_t * strip_hdr)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->pppoe_session_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->pppoe_session_get(dev_id, session_id, strip_hdr);
    return rv;
}

static sw_error_t
_fal_eapol_status_set(a_uint32_t dev_id, a_uint32_t port_id, a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->eapol_status_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->eapol_status_set(dev_id, port_id, enable);
    return rv;
}

static sw_error_t
_fal_eapol_status_get(a_uint32_t dev_id, a_uint32_t port_id, a_bool_t * enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->eapol_status_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->eapol_status_get(dev_id, port_id, enable);
    return rv;
}

static sw_error_t
_fal_ripv1_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ripv1_status_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->ripv1_status_set(dev_id, enable);
    return rv;
}

static sw_error_t
_fal_ripv1_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->ripv1_status_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->ripv1_status_get(dev_id, enable);
    return rv;
}

static sw_error_t
_fal_pppoe_session_table_add(a_uint32_t dev_id,
                              fal_pppoe_session_t * session_tbl)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->pppoe_session_table_add)
        return SW_NOT_SUPPORTED;

    rv = p_api->pppoe_session_table_add(dev_id, session_tbl);
    return rv;
}


static sw_error_t
_fal_pppoe_session_table_del(a_uint32_t dev_id,
                              fal_pppoe_session_t * session_tbl)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->pppoe_session_table_del)
        return SW_NOT_SUPPORTED;

    rv = p_api->pppoe_session_table_del(dev_id, session_tbl);
    return rv;
}

static sw_error_t
_fal_pppoe_session_table_get(a_uint32_t dev_id,
                              fal_pppoe_session_t * session_tbl)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->pppoe_session_table_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->pppoe_session_table_get(dev_id, session_tbl);
    return rv;
}

static sw_error_t
_fal_port_arp_req_status_set(a_uint32_t dev_id, fal_port_t port_id,
                             a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->port_arp_req_status_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->port_arp_req_status_set(dev_id, port_id, enable);
    return rv;
}

static sw_error_t
_fal_port_arp_req_status_get(a_uint32_t dev_id, fal_port_t port_id,
                             a_bool_t * enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->port_arp_req_status_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->port_arp_req_status_get(dev_id, port_id, enable);
    return rv;
}

static sw_error_t
_fal_port_arp_ack_status_set(a_uint32_t dev_id, fal_port_t port_id,
                             a_bool_t enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->port_arp_ack_status_set)
        return SW_NOT_SUPPORTED;

    rv = p_api->port_arp_ack_status_set(dev_id, port_id, enable);
    return rv;
}

static sw_error_t
_fal_port_arp_ack_status_get(a_uint32_t dev_id, fal_port_t port_id,
                             a_bool_t * enable)
{
    sw_error_t rv;
    hsl_api_t *p_api;

    SW_RTN_ON_NULL(p_api = hsl_api_ptr_get(dev_id));

    if (NULL == p_api->port_arp_ack_status_get)
        return SW_NOT_SUPPORTED;

    rv = p_api->port_arp_ack_status_get(dev_id, port_id, enable);
    return rv;
}

/**
 * @brief Set arp packets hardware acknowledgement status on a particular device.
 * @param[in] dev_id device id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_arp_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_arp_status_set(dev_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get arp packets hardware acknowledgement status on a particular device.
 * @param[in] dev_id device id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_arp_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_arp_status_get(dev_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set max frame size which device can received on a particular device.
 * @details    Comments:
 * The granularity of packets size is byte. 
 * @param[in] dev_id device id
 * @param[in] size packet size
 * @return SW_OK or error code
 */
sw_error_t
fal_frame_max_size_set(a_uint32_t dev_id, a_uint32_t size)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_frame_max_size_set(dev_id, size);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get max frame size which device can received on a particular device.
 * @details   Comments:
 *    The unit of packets size is byte.
 * @param[in] dev_id device id
 * @param[out] size packet size
 * @return SW_OK or error code
 */
sw_error_t
fal_frame_max_size_get(a_uint32_t dev_id, a_uint32_t * size)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_frame_max_size_get(dev_id, size);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set forwarding command for packets which source address is unknown on a particular port.
 * @details Comments:
 *    Particular device may only support parts of forwarding commands.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] cmd forwarding command
 * @return SW_OK or error code
 */
sw_error_t
fal_port_unk_sa_cmd_set(a_uint32_t dev_id, fal_port_t port_id,
                        fal_fwd_cmd_t cmd)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_port_unk_sa_cmd_set(dev_id, port_id, cmd);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get forwarding command for packets which source address is unknown on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] cmd forwarding command
 * @return SW_OK or error code
 */
sw_error_t
fal_port_unk_sa_cmd_get(a_uint32_t dev_id, fal_port_t port_id,
                        fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_port_unk_sa_cmd_get(dev_id, port_id, cmd);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set flooding status of unknown unicast packets on a particular port.
 * @details  Comments:
 *   If enable unknown unicast packets filter on one port then unknown
 *   unicast packets can't flood out from this port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_port_unk_uc_filter_set(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_port_unk_uc_filter_set(dev_id, port_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get flooding status of unknown unicast packets on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_port_unk_uc_filter_get(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t * enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_port_unk_uc_filter_get(dev_id, port_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set flooding status of unknown multicast packets on a particular port.
 * @details  Comments:
 *   If enable unknown multicast packets filter on one port then unknown
 *   multicast packets can't flood out from this port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_port_unk_mc_filter_set(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_port_unk_mc_filter_set(dev_id, port_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get flooding status of unknown multicast packets on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_port_unk_mc_filter_get(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t * enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_port_unk_mc_filter_get(dev_id, port_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set flooding status of broadcast packets on a particular port.
 * @details  Comments:
 *   If enable unknown multicast packets filter on one port then unknown
 *   multicast packets can't flood out from this port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_port_bc_filter_set(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_port_bc_filter_set(dev_id, port_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get flooding status of broadcast packets on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_port_bc_filter_get(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t * enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_port_bc_filter_get(dev_id, port_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set cpu port status on a particular device.
 * @param[in] dev_id device id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code 
 */
sw_error_t
fal_cpu_port_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_cpu_port_status_set(dev_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get cpu port status on a particular device.
 * @param[in] dev_id device id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code 
 */
sw_error_t
fal_cpu_port_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_cpu_port_status_get(dev_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set status of braodcast packets broadcasting to cpu on a particular device.
 * @param[in] dev_id device id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code 
 */
sw_error_t
fal_bc_to_cpu_port_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_bc_to_cpu_port_set(dev_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get status of braodcast packets broadcasting to cpu on a particular device.
 * @param[in] dev_id device id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code 
 */
sw_error_t
fal_bc_to_cpu_port_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_bc_to_cpu_port_get(dev_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set pppoe packets forwarding command on a particular device.
 * @details     comments:
 *   Particular device may only support parts of forwarding commands.
 *   Ihis operation will take effect only after enabling pppoe packets
 *   hardware acknowledgement
 * @param[in] dev_id device id
 * @param[in] cmd forwarding command
 * @return SW_OK or error code
 */
sw_error_t
fal_pppoe_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_pppoe_cmd_set(dev_id, cmd);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get pppoe packets forwarding command on a particular device.
 * @param[in] dev_id device id
 * @param[out] cmd forwarding command
 * @return SW_OK or error code 
 */
sw_error_t
fal_pppoe_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_pppoe_cmd_get(dev_id, cmd);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set pppoe packets hardware acknowledgement status on particular device.
 * @details     comments:
 *   Particular device may only support parts of pppoe packets.
 * @param[in] dev_id device id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code 
 */
sw_error_t
fal_pppoe_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_pppoe_status_set(dev_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get pppoe packets hardware acknowledgement status on a particular device.
 * @param[in] dev_id device id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_pppoe_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_pppoe_status_get(dev_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set dhcp packets hardware acknowledgement status on a particular device.
 * @param[in] dev_id device id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_port_dhcp_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_port_dhcp_set(dev_id, port_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get dhcp packets hardware acknowledgement status on a particular device.
 * @param[in] dev_id device id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_port_dhcp_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_port_dhcp_get(dev_id, port_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set arp packets forwarding command on a particular device.
 * @details    Comments:
 *    Particular device may only support parts of forwarding commands.
 *    This operation will take effect only after enabling arp
 * @param[in] dev_id device id
 * @param[in] cmd forwarding command
 * @return SW_OK or error code
 */
sw_error_t
fal_arp_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_arp_cmd_set(dev_id, cmd);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get arp packets forwarding command on a particular device.
 * @param[in] dev_id device id
 * @param[out] cmd forwarding command
 * @return SW_OK or error code
 */
sw_error_t
fal_arp_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_arp_cmd_get(dev_id, cmd);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set eapol packets forwarding command on a particular device.
 * @details    Comments:
 *    Particular device may only support parts of forwarding commands.
 *    This operation will take effect only after enabling eapol
 * @param[in] dev_id device id
 * @param[in] cmd forwarding command
 * @return SW_OK or error code
 */
sw_error_t
fal_eapol_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_eapol_cmd_set(dev_id, cmd);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get eapol packets forwarding command on a particular device.
 * @param[in] dev_id device id
 * @param[out] cmd forwarding command
 * @return SW_OK or error code
 */
sw_error_t
fal_eapol_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_eapol_cmd_get(dev_id, cmd);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Add a pppoe session entry to a particular device.
 * @param[in] dev_id device id
 * @param[in] session_id pppoe session id
 * @param[in] strip_hdr strip or not strip pppoe header
 * @return SW_OK or error code
 */
sw_error_t
fal_pppoe_session_add(a_uint32_t dev_id, a_uint32_t session_id, a_bool_t strip_hdr)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_pppoe_session_add(dev_id, session_id, strip_hdr);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Delete a pppoe session entry from a particular device.
 * @param[in] dev_id device id
 * @param[in] session_id pppoe session id
 * @return SW_OK or error code
 */
sw_error_t
fal_pppoe_session_del(a_uint32_t dev_id, a_uint32_t session_id)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_pppoe_session_del(dev_id, session_id);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get a pppoe session entry from a particular device.
 * @param[in] dev_id device id
 * @param[in] session_id pppoe session id
 * @param[out] strip_hdr strip or not strip pppoe header
 * @return SW_OK or error code
 */
sw_error_t
fal_pppoe_session_get(a_uint32_t dev_id, a_uint32_t session_id, a_bool_t * strip_hdr)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_pppoe_session_get(dev_id, session_id, strip_hdr);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set eapol packets hardware acknowledgement on a particular port.
 * @details    Comments:
 *    Particular device may only support parts of forwarding commands.
 *    This operation will take effect only after enabling eapol
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_eapol_status_set(a_uint32_t dev_id, a_uint32_t port_id, a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_eapol_status_set(dev_id, port_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get eapol packets hardware acknowledgement on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_eapol_status_get(a_uint32_t dev_id, a_uint32_t port_id, a_bool_t * enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_eapol_status_get(dev_id, port_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set ripv1 packets hardware acknowledgement on a particular port.
 * @details    Comments:
 *    Particular device may only support parts of forwarding commands.
 *    This operation will take effect only after enabling eapol
 * @param[in] dev_id device id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_ripv1_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ripv1_status_set(dev_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get ripv1 packets hardware acknowledgement on a particular port.
 * @param[in] dev_id device id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_ripv1_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_ripv1_status_get(dev_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Add a pppoe session entry to a particular device.
 * @param[in] dev_id device id
 * @param[in] session_tbl pppoe session table
 * @return SW_OK or error code
 */
sw_error_t
fal_pppoe_session_table_add(a_uint32_t dev_id,
                              fal_pppoe_session_t * session_tbl)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_pppoe_session_table_add(dev_id, session_tbl);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Delete a pppoe session entry from a particular device.
 * @param[in] dev_id device id
 * @param[in] session_tbl pppoe session table
 * @return SW_OK or error code
 */
sw_error_t
fal_pppoe_session_table_del(a_uint32_t dev_id,
                              fal_pppoe_session_t * session_tbl)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_pppoe_session_table_del(dev_id, session_tbl);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get a pppoe session entry from a particular device.
 * @param[in] dev_id device id
 * @param[out] session_tbl pppoe session table
 * @return SW_OK or error code
 */
sw_error_t
fal_pppoe_session_table_get(a_uint32_t dev_id,
                              fal_pppoe_session_t * session_tbl)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_pppoe_session_table_get(dev_id, session_tbl);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set arp req packets hardware acknowledgement status on a particular device.
 * @param[in] dev_id device id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_port_arp_req_status_set(a_uint32_t dev_id, fal_port_t port_id,
                             a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_port_arp_req_status_set(dev_id, port_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get arp req packets hardware acknowledgement status on a particular device.
 * @param[in] dev_id device id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_port_arp_req_status_get(a_uint32_t dev_id, fal_port_t port_id,
                             a_bool_t * enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_port_arp_req_status_get(dev_id, port_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Set arp ack packets hardware acknowledgement status on a particular device.
 * @param[in] dev_id device id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_port_arp_ack_status_set(a_uint32_t dev_id, fal_port_t port_id,
                             a_bool_t enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_port_arp_ack_status_set(dev_id, port_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @brief Get arp ack packets hardware acknowledgement status on a particular device.
 * @param[in] dev_id device id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
fal_port_arp_ack_status_get(a_uint32_t dev_id, fal_port_t port_id,
                             a_bool_t * enable)
{
    sw_error_t rv;

    FAL_API_LOCK;
    rv = _fal_port_arp_ack_status_get(dev_id, port_id, enable);
    FAL_API_UNLOCK;
    return rv;
}

/**
 * @}
 */

