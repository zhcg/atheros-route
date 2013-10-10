
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
#ifndef FLOWMAC_H
#ifdef CONFIG_FLOWMAC_MODULE
#define FLOWMAC_H 1
#include <linux/version.h>
#include <linux/stddef.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <asm/byteorder.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/bitops.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <net/sch_generic.h>
#include <linux/if_pppox.h>
#include <linux/ip.h>
#include <net/checksum.h>
#include <linux/proc_fs.h>
#include "flowmac_api.h"

/* All macro defines here */

/* OS specific stuff */
#define ASSERT(cond)                                     \
do {                                                     \
    if (unlikely(!(cond))) {                             \
        printk("assertion %s failed at %s:%i (%s)\n",    \
            #cond, __FILE__, __LINE__, __FUNCTION__);    \
    }                                                    \
} while (0)

#define spin_lock_destroy(__slock)

#define FLOW_VIDEO 0xa0
#define FLOW_AUDIO 0xef
#define FLOW_BE    0x00
#define FLOW_BK    0x08
#define FLOW_MAX_RATE_SET 4
typedef union _rate_element {
    struct ethernet_rate {
        int         dev_type;             /* which set of rates is configured */
        int         rate_code;     /* opaque rate code value, could be ip tos */
        u_int32_t   rate_value; /* opaque rate code, understood by the device */
    } eth_rate;
} rate_element_t;

/* statistics,
 * Number of times stall is called
 * Number of times resume is called 
 */
typedef struct _flowmac_stats {
    int stalls;
    int resumes;
    int forced_wakeup;
} flowmac_stats_t;
/*
 * Each of the following structure would represent one registered device
 * that can either be egress controlled or ingress controlled.
 *
 * ingress flow control: PAUSE on ingress into the AP on the current
 *                       device.
 * eg. If Ethernet is 1 Gig link and the wifi is 300 mbps link, the traffic
 * from Ethernet to wireless can be flow controlled. For this Ethernet
 * should be registered as ingress flow control capable. Probably wireless
 * interface could be egress controllable, like manual rate configuration.
 * One way the egress flow control is not existing. So that is kind of
 * leaving out a provision for future. 
 *
 * list             - Each registered device gets one such a pointer as
 *                    below, and points to next device.
 * dev              - A pointer to the device that is registered
 * is_flow_enable   - Is device registered to be flow controllable, it could
 *                    modify the status any time.
 * is_ingrate_limited  - Can device work with ingress rate limiting.
 *                    use case: In case of Ethernet it is possible, not in
 *                    wifi.
 * is_egrate_limited - Can device work with mentioned egress rates
 *                     use case: probably a PLC/Eth driver can ask wifi
 *                     driver to work at manual rate. This is not a
 *                     practical option, still leaving out a provision
 */
typedef struct _flow_mac  {
    struct list_head list;
    struct _flow_dev_t *flowdev;
    u_int8_t    is_flow_enabled;                   /* is flow control enabled */
	int			is_ing_flow_enabled;
	int			is_eg_flow_enabled;
    /* add all rate related fields here */
    u_int8_t    is_ingrate_limited;                /* is ingress rate limited */
    u_int8_t    is_egrate_limited;                  /* is egress rate limited */

	int			current_flow_state;
    void        *drv_handle;
    struct timer_list resume_timer;
    flow_ops_t  ops;      /* per device flow operations that can be performed */
} flow_mac_t;

typedef struct _flow_dev_t {
    u_int8_t    init_done;
    u_int8_t    n_registered;                 /* number of devices registered */
    u_int8_t    spare[2];
    flowmac_stats_t stats;
    spinlock_t  flow_dev_lock;
    flow_mac_t  devices;              /* list of devices that are registered */
#define FLOWMAC_PROC_TREE "flowmac"
#define FLOWMAC_PROC_STATS "stats"
    struct proc_dir_entry *flowmac_proc_tree;
    struct proc_dir_entry *flowmac_stats_pe;
} flow_dev_t;                   /* device pointer to register with net module */

/* all static globals here */
/* all non-static globals here */
/* all externs here */
/* all extern api's here */
/* all local function prototypes here */
int flowmac_init (void);
#else
#error "*** including without CONFIG_FLOWMAC is not wise ****"
#endif
#endif
