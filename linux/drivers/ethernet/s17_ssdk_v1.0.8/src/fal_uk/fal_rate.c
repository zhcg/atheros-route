/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#include "sw.h"
#include "sw_ioctl.h"
#include "fal_rate.h"
#include "fal_uk_if.h"

sw_error_t
fal_rate_queue_egrl_set(a_uint32_t dev_id, fal_port_t port_id,
                        fal_queue_t queue_id, a_uint32_t * speed,
                        a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_RATE_QU_EGRL_SET, dev_id, port_id, queue_id,
                       (a_uint32_t) speed, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_rate_queue_egrl_get(a_uint32_t dev_id, fal_port_t port_id,
                        fal_queue_t queue_id, a_uint32_t * speed,
                        a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_RATE_QU_EGRL_GET, dev_id, port_id, queue_id,
                       (a_uint32_t) speed, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_rate_port_egrl_set(a_uint32_t dev_id, fal_port_t port_id,
                       a_uint32_t * speed, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_RATE_PT_EGRL_SET, dev_id, port_id,
                       (a_uint32_t) speed, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_rate_port_egrl_get(a_uint32_t dev_id, fal_port_t port_id,
                       a_uint32_t * speed, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_RATE_PT_EGRL_GET, dev_id, port_id,
                       (a_uint32_t) speed, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_rate_port_inrl_set(a_uint32_t dev_id, fal_port_t port_id,
                       a_uint32_t * speed, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_RATE_PT_INRL_SET, dev_id, port_id,
                       (a_uint32_t) speed, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_rate_port_inrl_get(a_uint32_t dev_id, fal_port_t port_id,
                       a_uint32_t * speed, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_RATE_PT_INRL_GET, dev_id, port_id,
                       (a_uint32_t) speed, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_storm_ctrl_frame_set(a_uint32_t dev_id, fal_port_t port_id,
                         fal_storm_type_t frame_type, a_bool_t enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_STORM_CTRL_FRAME_SET, dev_id, port_id,
                       (a_uint32_t) frame_type, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_storm_ctrl_frame_get(a_uint32_t dev_id, fal_port_t port_id,
                         fal_storm_type_t frame_type, a_bool_t * enable)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_STORM_CTRL_FRAME_GET, dev_id, port_id,
                       (a_uint32_t) frame_type, (a_uint32_t) enable);
    return rv;
}

sw_error_t
fal_storm_ctrl_rate_set(a_uint32_t dev_id, fal_port_t port_id,
                        a_uint32_t * rate)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_STORM_CTRL_RATE_SET, dev_id, port_id,
                       (a_uint32_t) rate);
    return rv;
}

sw_error_t
fal_storm_ctrl_rate_get(a_uint32_t dev_id, fal_port_t port_id,
                        a_uint32_t * rate)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_STORM_CTRL_RATE_GET, dev_id, port_id,
                       (a_uint32_t) rate);
    return rv;
}

sw_error_t
fal_rate_port_policer_set(a_uint32_t dev_id, fal_port_t port_id,
                           fal_port_policer_t * policer)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_RATE_PORT_POLICER_SET, dev_id, port_id,
                       (a_uint32_t) policer);
    return rv;
}

sw_error_t
fal_rate_port_policer_get(a_uint32_t dev_id, fal_port_t port_id,
                           fal_port_policer_t * policer)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_RATE_PORT_POLICER_GET, dev_id, port_id,
                       (a_uint32_t) policer);
    return rv;
}

sw_error_t
fal_rate_port_shaper_set(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t enable, fal_egress_shaper_t * shaper)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_RATE_PORT_SHAPER_SET, dev_id, port_id,
                       (a_uint32_t) enable, (a_uint32_t) shaper);
    return rv;
}

sw_error_t
fal_rate_port_shaper_get(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t * enable, fal_egress_shaper_t * shaper)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_RATE_PORT_SHAPER_GET, dev_id, port_id,
                       (a_uint32_t) enable, (a_uint32_t) shaper);
    return rv;
}

sw_error_t
fal_rate_queue_shaper_set(a_uint32_t dev_id, fal_port_t port_id,
                           fal_queue_t queue_id, a_bool_t enable,
                           fal_egress_shaper_t * shaper)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_RATE_QUEUE_SHAPER_SET, dev_id, port_id, queue_id,
                       (a_uint32_t) enable, (a_uint32_t) shaper);
    return rv;
}

sw_error_t
fal_rate_queue_shaper_get(a_uint32_t dev_id, fal_port_t port_id,
                           fal_queue_t queue_id, a_bool_t * enable,
                           fal_egress_shaper_t * shaper)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_RATE_QUEUE_SHAPER_GET, dev_id, port_id, queue_id,
                       (a_uint32_t) enable, (a_uint32_t) shaper);
    return rv;
}

sw_error_t
fal_rate_acl_policer_set(a_uint32_t dev_id, a_uint32_t policer_id,
                           fal_acl_policer_t * policer)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_RATE_ACL_POLICER_SET, dev_id, policer_id, (a_uint32_t) policer);
    return rv;
}

sw_error_t
fal_rate_acl_policer_get(a_uint32_t dev_id, a_uint32_t policer_id,
                           fal_acl_policer_t * policer)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_RATE_ACL_POLICER_GET, dev_id, policer_id, (a_uint32_t) policer);
    return rv;
}

