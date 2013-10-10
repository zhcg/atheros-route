/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#include "sw.h"
#include "sw_ioctl.h"
#include "fal_cosmap.h"
#include "fal_uk_if.h"


sw_error_t
fal_cosmap_dscp_to_pri_set(a_uint32_t dev_id, a_uint32_t dscp, a_uint32_t pri)
{
    sw_error_t rv;

    rv= sw_uk_exec(SW_API_COSMAP_DSCP_TO_PRI_SET, dev_id, dscp, pri);
    return rv;
}

sw_error_t
fal_cosmap_dscp_to_pri_get(a_uint32_t dev_id, a_uint32_t dscp, a_uint32_t * pri)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_COSMAP_DSCP_TO_PRI_GET, dev_id, dscp, (a_uint32_t)pri);
    return rv;
}

sw_error_t
fal_cosmap_dscp_to_dp_set(a_uint32_t dev_id, a_uint32_t dscp, a_uint32_t dp)
{
    sw_error_t rv;

    rv= sw_uk_exec(SW_API_COSMAP_DSCP_TO_DP_SET, dev_id, dscp, dp);
    return rv;
}

sw_error_t
fal_cosmap_dscp_to_dp_get(a_uint32_t dev_id, a_uint32_t dscp, a_uint32_t * dp)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_COSMAP_DSCP_TO_DP_GET, dev_id, dscp, (a_uint32_t)dp);
    return rv;
}

sw_error_t
fal_cosmap_up_to_pri_set(a_uint32_t dev_id, a_uint32_t up, a_uint32_t pri)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_COSMAP_UP_TO_PRI_SET, dev_id, up, pri);
    return rv;
}

sw_error_t
fal_cosmap_up_to_pri_get(a_uint32_t dev_id, a_uint32_t up, a_uint32_t * pri)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_COSMAP_UP_TO_PRI_GET, dev_id, up, (a_uint32_t)pri);
    return rv;
}

sw_error_t
fal_cosmap_up_to_dp_set(a_uint32_t dev_id, a_uint32_t up, a_uint32_t dp)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_COSMAP_UP_TO_DP_SET, dev_id, up, dp);
    return rv;
}

sw_error_t
fal_cosmap_up_to_dp_get(a_uint32_t dev_id, a_uint32_t up, a_uint32_t * dp)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_COSMAP_UP_TO_DP_GET, dev_id, up, (a_uint32_t)dp);
    return rv;
}

sw_error_t
fal_cosmap_pri_to_queue_set(a_uint32_t dev_id, a_uint32_t pri,
                             a_uint32_t queue)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_COSMAP_PRI_TO_QU_SET, dev_id, pri, queue);
    return rv;
}

sw_error_t
fal_cosmap_pri_to_queue_get(a_uint32_t dev_id, a_uint32_t pri,
                             a_uint32_t * queue)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_COSMAP_PRI_TO_QU_GET, dev_id, pri, (a_uint32_t)queue);
    return rv;
}

sw_error_t
fal_cosmap_pri_to_ehqueue_set(a_uint32_t dev_id, a_uint32_t pri,
                               a_uint32_t queue)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_COSMAP_PRI_TO_EHQU_SET, dev_id, pri, queue);
    return rv;
}

sw_error_t
fal_cosmap_pri_to_ehqueue_get(a_uint32_t dev_id, a_uint32_t pri,
                               a_uint32_t * queue)
{
    sw_error_t rv;

    rv = sw_uk_exec(SW_API_COSMAP_PRI_TO_EHQU_GET, dev_id, pri, (a_uint32_t)queue);
    return rv;
}

