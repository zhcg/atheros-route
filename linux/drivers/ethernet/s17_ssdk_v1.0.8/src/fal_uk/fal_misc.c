/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#include "sw.h"
#include "sw_ioctl.h"
#include "fal_misc.h"
#include "fal_uk_if.h"

sw_error_t
fal_arp_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_ARP_STATUS_SET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_arp_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_ARP_STATUS_GET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_frame_max_size_set(a_uint32_t dev_id, a_uint32_t size)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FRAME_MAX_SIZE_SET, dev_id, size);
    return rv;
}

sw_error_t
fal_frame_max_size_get(a_uint32_t dev_id, a_uint32_t * size)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_FRAME_MAX_SIZE_GET, dev_id, (a_uint32_t) size);
    return rv;
}

sw_error_t
fal_port_unk_sa_cmd_set(a_uint32_t dev_id, fal_port_t port_id,
                        fal_fwd_cmd_t cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_UNK_SA_CMD_SET, dev_id, port_id,
                       (a_uint32_t) cmd);
    return rv;
}

sw_error_t
fal_port_unk_sa_cmd_get(a_uint32_t dev_id, fal_port_t port_id,
                        fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_UNK_SA_CMD_GET, dev_id, port_id,
                       (a_uint32_t) cmd);
    return rv;
}

sw_error_t
fal_port_unk_uc_filter_set(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_UNK_UC_FILTER_SET, dev_id, port_id,
                       (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_port_unk_uc_filter_get(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_UNK_UC_FILTER_GET, dev_id, port_id,
                       (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_port_unk_mc_filter_set(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_UNK_MC_FILTER_SET, dev_id, port_id,
                       (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_port_unk_mc_filter_get(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_UNK_MC_FILTER_GET, dev_id, port_id,
                       (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_port_bc_filter_set(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_BC_FILTER_SET, dev_id, port_id,
                       (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_port_bc_filter_get(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_BC_FILTER_GET, dev_id, port_id,
                       (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_cpu_port_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_CPU_PORT_STATUS_SET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_cpu_port_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_CPU_PORT_STATUS_GET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_bc_to_cpu_port_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_BC_TO_CPU_PORT_SET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_bc_to_cpu_port_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_BC_TO_CPU_PORT_GET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_pppoe_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
   sw_error_t rv;

    rv = sw_uk_exec(SW_API_PPPOE_CMD_SET, dev_id, (a_uint32_t) cmd);
    return rv;
}

sw_error_t
fal_pppoe_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PPPOE_CMD_GET, dev_id, (a_uint32_t) cmd);
    return rv;
}

sw_error_t
fal_pppoe_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PPPOE_STATUS_SET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_pppoe_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PPPOE_STATUS_GET, dev_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_port_dhcp_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_DHCP_SET, dev_id, (a_uint32_t)port_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_port_dhcp_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_DHCP_GET, dev_id, (a_uint32_t)port_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_arp_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_ARP_CMD_SET, dev_id, (a_uint32_t)cmd);
    return rv;
}

sw_error_t
fal_arp_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_ARP_CMD_GET, dev_id, (a_uint32_t)cmd);
    return rv;
}

sw_error_t
fal_eapol_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_EAPOL_CMD_SET, dev_id, (a_uint32_t)cmd);
    return rv;
}

sw_error_t
fal_eapol_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_EAPOL_CMD_GET, dev_id, (a_uint32_t)cmd);
    return rv;
}

sw_error_t
fal_pppoe_session_add(a_uint32_t dev_id, a_uint32_t session_id, a_bool_t strip_hdr)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PPPOE_SESSION_ADD, dev_id, session_id, (a_uint32_t)strip_hdr);
    return rv;
}

sw_error_t
fal_pppoe_session_del(a_uint32_t dev_id, a_uint32_t session_id)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PPPOE_SESSION_DEL, dev_id, session_id);
    return rv;
}

sw_error_t
fal_pppoe_session_get(a_uint32_t dev_id, a_uint32_t session_id, a_bool_t * strip_hdr)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PPPOE_SESSION_GET, dev_id, session_id, (a_uint32_t)strip_hdr);
    return rv;
}

sw_error_t
fal_eapol_status_set(a_uint32_t dev_id, a_uint32_t port_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_EAPOL_STATUS_SET, dev_id, port_id, (a_uint32_t)enable);
    return rv;
}

sw_error_t
fal_eapol_status_get(a_uint32_t dev_id, a_uint32_t port_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_EAPOL_STATUS_GET, dev_id, port_id, (a_uint32_t)enable);
    return rv;
}

sw_error_t
fal_ripv1_status_set(a_uint32_t dev_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_RIPV1_STATUS_SET, dev_id, (a_uint32_t)enable);
    return rv;
}

sw_error_t
fal_ripv1_status_get(a_uint32_t dev_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_RIPV1_STATUS_GET, dev_id, (a_uint32_t)enable);
    return rv;
}


sw_error_t
fal_port_arp_req_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_ARP_REQ_STATUS_SET, dev_id, port_id, (a_uint32_t)enable);
    return rv;
}


sw_error_t
fal_port_arp_req_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_ARP_REQ_STATUS_GET, dev_id, port_id, (a_uint32_t)enable);
    return rv;
}


sw_error_t
fal_port_arp_ack_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_ARP_ACK_STATUS_SET, dev_id, port_id, (a_uint32_t)enable);
    return rv;
}

sw_error_t
fal_port_arp_ack_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_ARP_ACK_STATUS_GET, dev_id, port_id, (a_uint32_t)enable);
    return rv;
}

sw_error_t
fal_pppoe_session_table_add(a_uint32_t dev_id, fal_pppoe_session_t * session_tbl)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PPPOE_SESSION_TABLE_ADD, dev_id, (a_uint32_t)session_tbl);
    return rv;
}

sw_error_t
fal_pppoe_session_table_del(a_uint32_t dev_id, fal_pppoe_session_t * session_tbl)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PPPOE_SESSION_TABLE_DEL, dev_id, (a_uint32_t)session_tbl);
    return rv;
}

sw_error_t
fal_pppoe_session_table_get(a_uint32_t dev_id, fal_pppoe_session_t * session_tbl)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PPPOE_SESSION_TABLE_GET, dev_id, (a_uint32_t)session_tbl);
    return rv;
}

sw_error_t
fal_pppoe_session_id_set(a_uint32_t dev_id, a_uint32_t index,
                              a_uint32_t id)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PPPOE_SESSION_ID_SET, dev_id, index, id);
    return rv;
}

sw_error_t
fal_pppoe_session_id_get(a_uint32_t dev_id, a_uint32_t index,
                              a_uint32_t * id)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PPPOE_SESSION_ID_GET, dev_id, index, (a_uint32_t)id);
    return rv;
}

sw_error_t
fal_intr_mask_set(a_uint32_t dev_id, a_uint32_t intr_mask)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_INTR_MASK_SET, dev_id, intr_mask);
    return rv;
}

sw_error_t
fal_intr_mask_get(a_uint32_t dev_id, a_uint32_t * intr_mask)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_INTR_MASK_GET, dev_id, (a_uint32_t)intr_mask);
    return rv;
}

sw_error_t
fal_intr_status_get(a_uint32_t dev_id, a_uint32_t * intr_status)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_INTR_STATUS_GET, dev_id, (a_uint32_t)intr_status);
    return rv;
}

sw_error_t
fal_intr_status_clear(a_uint32_t dev_id, a_uint32_t intr_status)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_INTR_STATUS_CLEAR, dev_id, intr_status);
    return rv;
}

sw_error_t
fal_intr_port_link_mask_set(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t intr_mask)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_INTR_PORT_LINK_MASK_SET, dev_id, port_id, intr_mask);
    return rv;
}

sw_error_t
fal_intr_port_link_mask_get(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t * intr_mask)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_INTR_PORT_LINK_MASK_GET, dev_id, port_id, (a_uint32_t)intr_mask);
    return rv;
}

sw_error_t
fal_intr_port_link_status_get(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t * intr_mask)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_INTR_PORT_LINK_STATUS_GET, dev_id, port_id, (a_uint32_t)intr_mask);
    return rv;
}

