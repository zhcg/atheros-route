/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#include "sw.h"
#include "sw_ioctl.h"
#include "fal_port_ctrl.h"
#include "fal_uk_if.h"

sw_error_t
fal_port_duplex_set(a_uint32_t dev_id, fal_port_t port_id,
                    fal_port_duplex_t duplex)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_DUPLEX_SET, dev_id, port_id,
                       (a_uint32_t) duplex);
    return rv;
}

sw_error_t
fal_port_duplex_get(a_uint32_t dev_id, fal_port_t port_id,
                    fal_port_duplex_t * pduplex)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_DUPLEX_GET, dev_id, port_id,
                       (a_uint32_t) pduplex);
    return rv;
}

sw_error_t
fal_port_speed_set(a_uint32_t dev_id, fal_port_t port_id,
                   fal_port_speed_t speed)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_SPEED_SET, dev_id, port_id,
                       (a_uint32_t) speed);
    return rv;
}

sw_error_t
fal_port_speed_get(a_uint32_t dev_id, fal_port_t port_id,
                   fal_port_speed_t * pspeed)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_SPEED_GET, dev_id, port_id,
                       (a_uint32_t) pspeed);
    return rv;
}

sw_error_t
fal_port_autoneg_status_get(a_uint32_t dev_id, fal_port_t port_id,
                            a_bool_t * status)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_AN_GET, dev_id, port_id, (a_uint32_t) status);
    return rv;
}

sw_error_t
fal_port_autoneg_enable(a_uint32_t dev_id, fal_port_t port_id)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_AN_ENABLE, dev_id, port_id);
    return rv;
}

sw_error_t
fal_port_autoneg_restart(a_uint32_t dev_id, fal_port_t port_id)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_AN_RESTART, dev_id, port_id);
    return rv;
}

sw_error_t
fal_port_autoneg_adv_set(a_uint32_t dev_id, fal_port_t port_id,
                         a_uint32_t autoadv)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_AN_ADV_SET, dev_id, port_id, autoadv);
    return rv;
}

sw_error_t
fal_port_autoneg_adv_get(a_uint32_t dev_id, fal_port_t port_id,
                         a_uint32_t * autoadv)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_AN_ADV_GET, dev_id, port_id,
                       (a_uint32_t) autoadv);
    return rv;
}

sw_error_t
fal_port_hdr_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_HDR_SET, dev_id, port_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_port_hdr_status_get(a_uint32_t dev_id, fal_port_t port_id,
                        a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_HDR_GET, dev_id, port_id, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_port_flowctrl_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_FLOWCTRL_SET, dev_id, port_id,
                       (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_port_flowctrl_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_FLOWCTRL_GET, dev_id, port_id,
                       (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_port_flowctrl_forcemode_set(a_uint32_t dev_id, fal_port_t port_id,
                                a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_FLOWCTRL_MODE_SET, dev_id, port_id,
                       (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_port_flowctrl_forcemode_get(a_uint32_t dev_id, fal_port_t port_id,
                                a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_FLOWCTRL_MODE_GET, dev_id, port_id,
                       (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_port_powersave_set(a_uint32_t dev_id, fal_port_t port_id,
                                a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_POWERSAVE_SET, dev_id, port_id,
                       (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_port_powersave_get(a_uint32_t dev_id, fal_port_t port_id,
                                a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_POWERSAVE_GET, dev_id, port_id,
                       (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_port_hibernate_set(a_uint32_t dev_id, fal_port_t port_id,
                                a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_HIBERNATE_SET, dev_id, port_id,
                       (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_port_hibernate_get(a_uint32_t dev_id, fal_port_t port_id,
                                a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_HIBERNATE_GET, dev_id, port_id,
                       (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_port_cdt(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t mdi_pair,
                a_uint32_t *cable_status, a_uint32_t *cable_len)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_CDT, dev_id, port_id, mdi_pair, 
                       (a_uint32_t) cable_status, (a_uint32_t)cable_len);
    return rv;
}

sw_error_t
fal_port_rxhdr_mode_set(a_uint32_t dev_id, fal_port_t port_id,
                           fal_port_header_mode_t mode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_RXHDR_SET, dev_id, port_id, (a_uint32_t)mode);
    return rv;
}

sw_error_t
fal_port_rxhdr_mode_get(a_uint32_t dev_id, fal_port_t port_id,
                           fal_port_header_mode_t * mode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_RXHDR_GET, dev_id, port_id, (a_uint32_t)mode);
    return rv;
}

sw_error_t
fal_port_txhdr_mode_set(a_uint32_t dev_id, fal_port_t port_id,
                           fal_port_header_mode_t mode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_TXHDR_SET, dev_id, port_id, (a_uint32_t)mode);
    return rv;
}

sw_error_t
fal_port_txhdr_mode_get(a_uint32_t dev_id, fal_port_t port_id,
                           fal_port_header_mode_t * mode)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_PT_TXHDR_GET, dev_id, port_id, (a_uint32_t)mode);
    return rv;
}

sw_error_t
fal_header_type_set(a_uint32_t dev_id, a_bool_t enable, a_uint32_t type)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_HEADER_TYPE_SET, dev_id, (a_uint32_t)enable, type);
    return rv;
}

sw_error_t
fal_header_type_get(a_uint32_t dev_id, a_bool_t * enable, a_uint32_t * type)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_HEADER_TYPE_GET, dev_id, (a_uint32_t)enable, (a_uint32_t)type);
    return rv;
}

