/*
 * Copyright (c) 2008, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef CONFIG_FLOWMAC_MODULE
#ifndef FLOWMAC_API_H
#define FLOWMAC_API_H
#include <linux/netdevice.h>
typedef enum {
    DIR_INGRESS = 1,
    DIR_EGRESS = 2,
} flow_dir_t;

typedef enum {
    FLOWMAC_DOWN = 1
} flowmac_event_t;
/* keep this three times higher than time max video timeout */
#define FLOWMAC_PAUSE_TIMEOUT 120 
/*
 * pause: 
 *      Pauses the device through the net_device handle. Interface passes
 *      down a ioctl to the target device (from) with new ioctl. For this to
 *      happen device should be present in the registered list of devices
 * rate_limit
 *      Adjusts the rate of the device. The direction could be
 *      either DIR_INGRESS or DIR_EGRESS. The arguements to this pointer is
 *      a void pointer. This is a opaque pointer and every device interprets it
 *      differently. For wireless device, it may be changing from Auto to
 *      manual rate, and for ethernet, fixing the standard rate on one
 *      particular type of the class of service.
 * notify_event
 *      Function registered by the drivers, to get any notifications from
 *      the flow mac module.
 *          FLOWMAC_DOWN - if for some reason the module need to go down,
 *          when the devices are in registered state, inform the devices
 *          that the module is going down after un-pausing the stations.
 *
 */
typedef struct _flow_ops {
        int8_t (*pause)(struct net_device *dev, u_int8_t pause,
                u_int32_t duration);
        int8_t (*rate_limit)(struct net_device *dev, flow_dir_t dir,
                void *rate);
        int  (*notify_event) (struct net_device *dev, flowmac_event_t event,
                void *event_data);
} flow_ops_t;

extern void *flowmac_register(struct net_device *dev, flow_ops_t *ops,
        int ing_flow_ctl_enable, int is_eg_flow_cntrl_enabled,
        int ingrate, int egrate);
extern int   flowmac_unregister(void *handle);
extern int   flowmac_pause(void *handle, int pause, u_int32_t duration);
extern int   get_pause_state(void *handle);
extern int   flowmac_rate_limit(void *handle, flow_dir_t dir, void *rate);
#endif
#else
#error "**** including API file without CONFIG_FLOWMAC, is not wise ****"
#endif
