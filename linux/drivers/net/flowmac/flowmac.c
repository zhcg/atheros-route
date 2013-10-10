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
#include "flowmac.h"

#define MODULE_NAME "FLOWMAC"
MODULE_LICENSE("Dual BSD/GPL");

#ifdef SELF_TEST
void test_my_mod(void);
#endif 

/* global module locals here */
flow_dev_t *flowdev;
int flowmac_pause(void *handle, int pause, u_int32_t duration);
int flowmac_rate_limit(void *handle, flow_dir_t dir, void *rate);
/* timer function */
void
flowmac_resume_timer(unsigned long cnxt)
{
    flow_mac_t *tfmac = (flow_mac_t *)cnxt;

    if (!tfmac) return;

    /* resume the device without any regard */
    tfmac->flowdev->stats.forced_wakeup++;
    flowmac_pause(tfmac, 0, 0);
}

/* 
 * dump the statistics
 */
int
flowmac_dump_stats(char *buf, char **start, off_t offset, 
        int count, int *unused_i, void *unused_v)
{
    int len=0;


    len += sprintf(buf, "flowmac : init_done:%d\n", flowdev->init_done);
    len += sprintf(buf+len, "devices: %d\n",flowdev->n_registered);
    len += sprintf(buf+len, "stats:stall: %d resume %d forced_wakeup %d\n",
            flowdev->stats.stalls, flowdev->stats.resumes,
            flowdev->stats.forced_wakeup);
    return len;
}
/*
 * register the proc file names with kernel for stats reading
 */
int
flowmac_proc_register(flow_dev_t *tflow)
{
    struct proc_dir_entry *pe=NULL;

    tflow->flowmac_proc_tree = proc_mkdir (FLOWMAC_PROC_TREE, NULL);
    if (tflow->flowmac_proc_tree == NULL) {
        kfree (tflow);
        printk("Cannot Create root directory \n");
        return -ENOMEM;
    }

    pe = create_proc_entry(FLOWMAC_PROC_STATS, 0444, tflow->flowmac_proc_tree);

    if (!pe) {
        printk("cannot create the stats entry \n");
        remove_proc_entry (FLOWMAC_PROC_TREE, tflow->flowmac_proc_tree);
        return -ENOMEM;
    }
    pe->data = tflow;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 31)
    pe->owner = THIS_MODULE;
#endif
    pe->read_proc = flowmac_dump_stats;
    tflow->flowmac_stats_pe = pe;
    return 0;
}

/*
 * remove all the proc entries that are regsitered
 */
void
flowmac_proc_deregister(flow_dev_t *tflow)
{

    if (!tflow) return;

    /* de register stats first */
    if (tflow->flowmac_stats_pe) {
        remove_proc_entry (FLOWMAC_PROC_STATS, tflow->flowmac_proc_tree);
        tflow->flowmac_stats_pe = NULL;
    }
    if (tflow->flowmac_proc_tree) {
        remove_proc_entry(FLOWMAC_PROC_TREE, NULL);
        tflow->flowmac_proc_tree = NULL;
    }
}


/* module specific code here */
int flowmac_init(void)
{
    flow_dev_t *tflow;

    tflow = kmalloc(sizeof (flow_dev_t), GFP_ATOMIC);

    if (!tflow) return -ENOMEM;

    memset(tflow, 0, sizeof(*tflow));
    if (flowmac_proc_register(tflow)) {
        kfree(tflow);
        printk("failed to regsiter with proc \n");
        return -ENOMEM;
    }

    /* initialize the list of devices */
    INIT_LIST_HEAD(&tflow->devices.list);
    tflow->n_registered = 0;
    spin_lock_init(&tflow->flow_dev_lock);
    /* register the proc interface */
    tflow->init_done = 1;
    /* if every thing good assign the pointer to global */
    flowdev = tflow;
    return 0;
}

void *
flowmac_register(struct net_device *dev, flow_ops_t *ops,
        int is_ing_flow_enabled, int is_eg_flow_enabled,
        int ingrate, int egrate)
{
    /* create a flow_mac_t structure and insert into the list */
    flow_mac_t *tfmac;

    tfmac = (flow_mac_t*) kmalloc(sizeof(flow_mac_t), GFP_ATOMIC);
    if (!tfmac) return NULL;

    spin_lock(&flowdev->flow_dev_lock);

    tfmac->is_flow_enabled = 0;
    tfmac->is_ingrate_limited = ingrate;
    tfmac->is_egrate_limited = egrate;
    tfmac->is_ing_flow_enabled = is_ing_flow_enabled;
    tfmac->is_eg_flow_enabled = is_eg_flow_enabled;
    if (ops) {
        tfmac->ops.pause = ops->pause;
        tfmac->ops.rate_limit = ops->rate_limit;
        tfmac->ops.notify_event = ops->notify_event;
    }
    tfmac->drv_handle = (void*)dev;
    list_add (&(tfmac->list), &(flowdev->devices.list));

    /* initialize the timer */
    init_timer (&tfmac->resume_timer);
    tfmac->resume_timer.function = flowmac_resume_timer;
    tfmac->resume_timer.data = (unsigned long)tfmac;
    tfmac->resume_timer.expires = 0;
    /* keep the pointer to flowdev inside the registered device */
    tfmac->flowdev = flowdev;

    /* bump up the registered count */
    flowdev->n_registered++;

    printk("Device Added ............... %p\n", tfmac);
    spin_unlock(&flowdev->flow_dev_lock);
    return tfmac;
}

EXPORT_SYMBOL(flowmac_register);

int
flowmac_unregister(void *handle)
{
    flow_mac_t *tfmac = (flow_mac_t*)handle;

    /* unlink the element from the list first */
    printk("Device removed ............... %p\n", tfmac);
    /* stop and delete the running timer */
    spin_lock(&flowdev->flow_dev_lock);
    del_timer(&tfmac->resume_timer);
    list_del(&(tfmac->list));
    flowdev->n_registered--;
    spin_unlock(&flowdev->flow_dev_lock);
    kfree(tfmac);
    return 0;
}
EXPORT_SYMBOL(flowmac_unregister);
/*
 * Function: flowmac_pause
 *
 * Check if the device is in already paused state and call pause on
 * registered driver with the specified duration. 
 * 
 * Every time PAUSE == 1, and duration is set, timer for duration milli
 * seconds would be started. On expiry, the devices would resume.
 */
#define MAX_TIME_LIMIT 1000

#define check_pause_limits(__t) (((__t) >= 0) && ((__t) < MAX_TIME_LIMIT))
int
flowmac_pause(void *handle, int pause, u_int32_t duration)
{
    if (!handle) return -EINVAL;
    
    if (check_pause_limits(duration)) {
        /* walk through all the devices, other than the one requested 
         * and if ingress puase allowed, pause that
         */
        struct list_head *index = NULL;
        flow_mac_t *tfmac = NULL;

        list_for_each(index, &(flowdev->devices.list)) {
            tfmac = list_entry(index, flow_mac_t, list);
            if (tfmac && tfmac->ops.pause && (tfmac != handle) &&
                    tfmac->is_ing_flow_enabled) {

                if (timer_pending(&tfmac->resume_timer)) {
                    del_timer(&tfmac->resume_timer);
                }

                (void)tfmac->ops.pause (tfmac->drv_handle, pause, duration);

                tfmac->current_flow_state = pause;
                /* start the timer if going to PAUSE state */
                if (pause && duration) {
                    mod_timer (&tfmac->resume_timer,
                            jiffies + msecs_to_jiffies(duration));
                    flowdev->stats.stalls++;
                }

                if (!pause) flowdev->stats.resumes++;
            }
        }
    } else return -EINVAL;
    return 0;
}
EXPORT_SYMBOL(flowmac_pause);
int
flowmac_rate_limit(void *handle, flow_dir_t dir, void *rate)
{
    if (!handle) return -EINVAL;
    if (rate) {
        struct list_head *index = NULL;
        flow_mac_t *tfmac = NULL;
        
        list_for_each(index, &(flowdev->devices.list)) {
            tfmac = list_entry(index, flow_mac_t, list);
            if (tfmac && tfmac->ops.rate_limit && (tfmac != handle) &&
                    tfmac->is_ingrate_limited) {
                (void)tfmac->ops.rate_limit(tfmac->drv_handle, dir, rate);
            } 
        }
    } else return -EINVAL;
    return 0;
}
EXPORT_SYMBOL(flowmac_rate_limit);
/* Linux Module interfacing */
static int __init
flowmac_mod_init(void)
{
    int err;
    
    err = flowmac_init();

    printk("%s: Success: err %d\n", __func__, err);

    if (err) return err;
    
#ifdef SELF_TEST
    test_my_mod();
#endif
    return 0;
}

static void __exit
flowmac_mod_cleanup(void)
{
    flow_mac_t *tfmac = NULL;
    struct list_head *index = NULL;

    /* remove the spinlock firt */
    if (flowdev) {
        spin_lock(&flowdev->flow_dev_lock);
        if (flowdev->n_registered) {
            /* for all the devices */
            list_for_each(index, &(flowdev->devices.list)) {
                tfmac = list_entry(index, flow_mac_t, list);
                if (tfmac) {
                    /* unpause all of them, stop running timer */
                    flowmac_pause(tfmac, 0, 0);
                    /* notify the device, no return value, this is just
                     * notification, every module should stop using
                     * the flow control calls
                     */
                    tfmac->ops.notify_event(tfmac->drv_handle, FLOWMAC_DOWN,
                            NULL);
                    /* de allocate the list element, on behalf of module */
                    flowmac_unregister(tfmac);
                    tfmac = NULL;
                } 
            } /* list_for_each ( device list ) */
            ASSERT(flowdev->n_registered == 0);
        }
        spin_unlock (&flowdev->flow_dev_lock);
        spin_lock_destroy(&flowdev->flow_dev_lock);
        kfree(flowdev);
        flowdev = NULL;
    }
    printk(MODULE_NAME ": cleanup done\n");
}

module_init(flowmac_mod_init);
module_exit(flowmac_mod_cleanup);


/************************************************************************
 * unit test functions below 
 ************************************************************************
 */
u_int8_t
my_pause (struct net_device *dev, u_int8_t pause, u_int32_t duration)
{
    printk("%s called dev %d, pause %d, duration %d \n", 
            __func__, *((int*)dev), pause, duration);
    return 0;
}
u_int8_t
my_rate_limit(struct net_device *dev, flow_dir_t dir, void *rate)
{
    printk("%s called dev %d direction %d, rate %d \n", 
            __func__, *((int*)dev), dir, *((int*)rate));
    return 0;
}

void
test_my_mod(void)
{
#define N_DEV 10
    void *my_handles[N_DEV];
    int  my_devices[N_DEV];
    flow_ops_t my_ops[N_DEV];

    int i=0;

    for (i=0; i < N_DEV; i++) {
        my_devices[i] = i;
        my_ops[i].pause = my_pause;
        my_ops[i].rate_limit = my_rate_limit;
        my_ops[i].notify_event = NULL;
    }
    for (i=0; i < N_DEV; i++) {
        /* register the device */
        my_handles[i] = flowmac_register((struct net_device*)&my_devices[i],
                                &my_ops[i], 1, 1, 1, 1);
        
    }

    /* call pause on all functions */
    {
        flow_mac_t *tfmac = NULL;
        struct list_head *index;
        
        list_for_each(index, &(flowdev->devices.list)) {
            tfmac = list_entry(index, flow_mac_t, list);
            if (tfmac && tfmac->ops.pause) {
                (void)tfmac->ops.pause ( tfmac->drv_handle, 1, 1024);
                (void)tfmac->ops.rate_limit ( tfmac->drv_handle, 1,
                        tfmac->drv_handle);
            } else {
                printk("%s tfmac is NULL %p\n", __func__, tfmac);
            }
        }
    }

    for (i=0; i < N_DEV; i++) {
        /* register the device */
        flowmac_unregister(my_handles[i]);
    }
}
