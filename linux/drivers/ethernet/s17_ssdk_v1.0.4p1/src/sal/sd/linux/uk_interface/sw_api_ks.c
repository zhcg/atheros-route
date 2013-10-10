/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#include "sw.h"
#include "sw_api.h"

#ifdef KVER26 /*Linux Kernel 2.6 */
#define __USER     __user
#else /*Linux Kernel 2.4 */
#include <asm/uaccess.h>
#define __USER
#define CLONE_KERNEL	(CLONE_FS | CLONE_FILES | CLONE_SIGHAND)
#define for_each_process(p) for_each_task(p)
#endif /*KVER26 */
#include <net/sock.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/version.h>
#include "api_access.h"
#include "sw_api_ks.h"

#if 0
#define dprintk(args...) printk(args)
#else
#define dprintk(args...)
#endif

/*configurable value for max creating request of kernel thread*/
#define PID_THREADS_MAX  32

#define PID_TAB_MAX         PID_THREADS_MAX
#define PID_TAB_NOT_FOUND   PID_TAB_MAX+1

static pid_t pid_parents[PID_TAB_MAX] = {0};    
static pid_t pid_childs[PID_TAB_MAX] = {0};
static wait_queue_head_t pid_child_wait[PID_TAB_MAX];
static struct semaphore	pid_tab_sem;

static a_uint32_t *cmd_buf = NULL;
static struct semaphore	api_sem;
static struct sock *ssdk_nl_sk = NULL;

static sw_error_t
input_parser(sw_api_param_t *p, a_uint32_t nr_param, a_uint32_t *args)
{
    a_uint32_t i = 0, buf_head = nr_param;

    for (i = 0; i < nr_param; i++) {
        if (p->param_type & SW_PARAM_PTR) {
            cmd_buf[i] = (a_uint32_t) & cmd_buf[buf_head];
            buf_head += (p->data_size + 3) / 4;

            if (buf_head > (SW_MAX_API_BUF / 4))
                return SW_NO_RESOURCE;

            if (p->param_type & SW_PARAM_IN) {
                if (copy_from_user((a_uint8_t*)(cmd_buf[i]), (void __USER *)args[i + 2], ((p->data_size + 3) >> 2) << 2))
                    return SW_NO_RESOURCE;
            }
        } else {
            cmd_buf[i] = args[i + 2];
        }
        p++;
    }
    return SW_OK;
}

static sw_error_t
output_parser(sw_api_param_t *p, a_uint32_t nr_param, a_uint32_t *args)
{
    a_uint32_t i =0;

    for (i = 0; i < nr_param; i++) {
        if (p->param_type & SW_PARAM_OUT) {
            if (copy_to_user
                ((void __USER *) args[i + 2], (a_uint32_t *) cmd_buf[i], ((p->data_size + 3) >> 2) << 2))
                return SW_NO_RESOURCE;
        }
        p++;
    }

    return SW_OK;
}

static sw_error_t
sw_api_cmd(a_uint32_t * args)
{
    a_uint32_t *p = cmd_buf, api_id = args[0], nr_param = 0;
    sw_error_t(*func) (a_uint32_t, ...);
    sw_api_param_t *pp;
    sw_api_func_t *fp;
    sw_error_t rv;
    sw_api_t sw_api;

    down(&api_sem);

    sw_api.api_id = api_id;
    rv = sw_api_get(&sw_api);
    SW_OUT_ON_ERROR(rv);

    fp = sw_api.api_fp;
    pp = sw_api.api_pp;
    nr_param = sw_api.api_nr;
    
    rv = input_parser(pp, nr_param, args);
    SW_OUT_ON_ERROR(rv);
    func = fp->func;

    switch (nr_param) {
    case 1:
        rv = (func) (p[0]);
        break;
    case 2:
        rv = (func) (p[0], p[1]);
        break;
    case 3:
        rv = (func) (p[0], p[1], p[2]);
        break;
    case 4:
        rv = (func) (p[0], p[1], p[2], p[3]);
        break;
    case 5:
        rv = (func) (p[0], p[1], p[2], p[3], p[4]);
        break;
    case 6:
        rv = (func) (p[0], p[1], p[2], p[3], p[4], p[5]);
        break;
    case 7:
        rv = (func) (p[0], p[1], p[2], p[3], p[4], p[5], p[6]);
        break;
    case 8:
        rv = (func) (p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
        break;
    case 9:
        rv = (func) (p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8]);
        break;
    case 10:
        rv = (func) (p[0], p[1], p[2], p[3], p[4], p[5],
                     p[6], p[7], p[8], p[9]);
        break;
    default:
        rv = SW_OUT_OF_RANGE;
    }

    SW_OUT_ON_ERROR(rv);
    rv = output_parser(pp, nr_param, args);

out:
    up(&api_sem);
    return rv;
}

static inline int pid_find(pid_t pid, pid_t pids[])
{
    a_uint32_t i, loc = PID_TAB_NOT_FOUND;
    
    for(i = 0; i< PID_TAB_MAX; i++) {
        if(pids[i] == pid) {
            loc = i;
            break;
        }
    }
    return loc;
}

static inline a_bool_t  pid_exit(pid_t parent_pid)
{
	struct task_struct *p;
    a_bool_t rtn = A_TRUE;
  
	for_each_process(p) {
		if(parent_pid == p->pid){
          rtn = A_FALSE;
          break;
		}
	}

    return rtn;
}

static inline void pid_free(a_uint32_t loc)
{
	if (down_interruptible(&pid_tab_sem))
        return;

    pid_childs[loc] = 0;
    pid_parents[loc] = 0;

    up(&pid_tab_sem);
}

static inline a_bool_t pid_full(void)
{
    return (pid_find(0, pid_parents) == PID_TAB_NOT_FOUND)?A_TRUE:A_FALSE;
}

static a_uint32_t pid_find_save (pid_t parent_pid, pid_t child_pid)
{
    a_uint32_t loc = PID_TAB_NOT_FOUND;
    
    if(!parent_pid && !child_pid) {
        aos_printk("child and father can't both zero\n");
        return loc;
    }    

	if (down_interruptible(&pid_tab_sem))
        return loc;
    
    if(!parent_pid) { 
        /*find locate by child_pid*/
        loc = pid_find(child_pid, pid_childs);

    } else {
        /*find locate by parent_pid*/
        loc = pid_find(parent_pid, pid_parents);
        
        if(child_pid)  { 
            loc = pid_find(0, pid_parents);

            if(loc != PID_TAB_NOT_FOUND) {
                pid_childs[loc] = child_pid;                
                pid_parents[loc] = parent_pid;
            }
        }
    } 

    up(&pid_tab_sem);
    return loc;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22) /* Linux > 2.6.22, jhung */
static void
sw_api_exec(struct sk_buff *skb, pid_t pid, int do_cmd)
{
    sw_error_t rv = SW_NO_RESOURCE;
    a_uint32_t args[SW_MAX_API_PARAM], rtn, skblen, nlmsglen;
    struct nlmsghdr *nlh = NULL;
    struct sk_buff *rep; 

    skblen = skb->len; 
    if (skb->len < sizeof(nlh))
    {
        aos_printk("skb len error - (%d)\n", skb->len);
        return;
    }
    
    nlh = (struct nlmsghdr *)skb->data;
    if (!nlh) {
        aos_printk("pid error: nlh = null\n");
        return;
    }
    
    nlmsglen = nlh->nlmsg_len; 
    if (nlmsglen < sizeof(*nlh) || skblen < nlmsglen)
    {
        aos_printk("nlmsglen error - (%d)\n", nlmsglen);
        return;
    }
    
    if(nlh->nlmsg_pid != pid)	/* not our skb packet */
        return;

    aos_mem_copy(args, NLMSG_DATA(nlh), SW_MAX_PAYLOAD);
    rep = skb_clone(skb, in_interrupt() ? GFP_ATOMIC : GFP_KERNEL);
    
    if (!rep) {
        aos_printk("reply socket buffer allocation error... \n");
        return;
    }
    
    if(do_cmd)
        rv = sw_api_cmd(args);
    
    /* return API result to user */
    rtn = (a_uint32_t) rv;
    if (copy_to_user
        ((void __USER *) args[1], (a_uint32_t *) & rtn, sizeof (a_uint32_t)))
    {
        rv = SW_NO_RESOURCE;
    }
    
    NETLINK_CB(rep).pid = 0;
    NETLINK_CB(rep).dst_group = 0;
    netlink_unicast(ssdk_nl_sk, rep, nlh->nlmsg_pid, MSG_DONTWAIT);
}
#else /* LINUX < 2.6.22 */
static void
sw_api_exec(struct sock *sk, pid_t pid, int do_cmd)
{
    sw_error_t rv = SW_NO_RESOURCE;
    a_uint32_t args[SW_MAX_API_PARAM], rtn;
    struct sk_buff *skb, *skb_first = NULL;
    struct nlmsghdr *nlh = NULL;

    /* debug, jhung */
    aos_printk("into sw_api_exec... \n"); 
    while(1) {   
#ifdef KVER26
        skb = skb_dequeue(&sk->sk_receive_queue);
#else
        skb = skb_dequeue(&sk->receive_queue);
#endif
        if (!skb) {
            dprintk("pid error: skb = null\n");
            return;
        }

        nlh = (struct nlmsghdr *)skb->data;
        if (!nlh) {
            dprintk("pid error: nlh = null\n");
            return;
        }

        if(nlh->nlmsg_pid == pid)
            break;

        if(!skb_first) {
            skb_first = skb;
        } else if (skb_first == skb) {
            aos_printk("can't found my skb???\n");
            return;
        }

#ifdef KVER26
        skb_queue_tail(&sk->sk_receive_queue, skb);
#else
        skb_queue_tail(&sk->receive_queue, skb);
#endif
    }   
    
    aos_mem_copy(args, NLMSG_DATA(nlh), SW_MAX_PAYLOAD);
    
    /* debug, jhung */
    aos_printk("before sw_api_cmd... \n");  
    if(do_cmd)
        rv = sw_api_cmd(args);
    
    /* debug, jhung */
    aos_printk("after sw_api_cmd... \n");  
    /* return API result to user */
    rtn = (a_uint32_t) rv;
    if (copy_to_user
        ((void __USER *) args[1], (a_uint32_t *) & rtn, sizeof (a_uint32_t)))
    {
        rv = SW_NO_RESOURCE;
    }
    
    /* debug, jhung */
    aos_printk("after copy_to_user... \n");  
    
    NETLINK_CB(skb).pid = 0;
    NETLINK_CB(skb).dst_pid = nlh->nlmsg_pid;
#ifdef KVER26        
    NETLINK_CB(skb).dst_group = 0;
#else
    NETLINK_CB(skb).dst_groups = 0;
#endif
    netlink_unicast(sk, skb, nlh->nlmsg_pid, MSG_DONTWAIT);
}

#endif /* LINUX > 2.6.22 */

static int sw_api_thread(void *sk)
{
    a_uint32_t loc, i;
    pid_t parent_pid = 0, child_pid = current->pid;

    while ((loc = pid_find_save(parent_pid, child_pid)) == PID_TAB_NOT_FOUND)
        schedule_timeout(1*HZ);

    parent_pid = pid_parents[loc];
    dprintk("thread child [%d] find parent [%d] at %d \n", child_pid, parent_pid, loc);
    
    for(i=0; ; i++) {
        if(i && !sleep_on_timeout(&pid_child_wait[loc], (5*HZ))) {
            if(pid_exit(parent_pid) == A_FALSE)
                continue;
            
            pid_free(loc);
            dprintk("thread child[%d] exit!\n", child_pid);
            return 0;
        }
        
        sw_api_exec(sk, parent_pid, 1);
    }

    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22) /* Linux > 2.6.22, jhung */
static void
sw_api_netlink(struct sk_buff *skb)
#else	/* Linux < 2.6.22, jhung */
static void
sw_api_netlink(struct sock *sk, int len)
#endif
{
    pid_t parent_pid = current->pid, child_pid = 0;
    a_uint32_t loc = pid_find_save (parent_pid, child_pid);
    
    if(loc == PID_TAB_NOT_FOUND) {
        if(pid_full()){
            aos_printk("###threads exceed the max [%d] for pid [%d]!###\n", PID_TAB_MAX, parent_pid);
            #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
            sw_api_exec(skb, parent_pid, 0);
            #else
            sw_api_exec(sk, parent_pid, 0);
            #endif
            return;
        }
        
        #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
        if ((child_pid = kernel_thread(sw_api_thread, skb, CLONE_KERNEL)) < 0){
        #else
        if ((child_pid = kernel_thread(sw_api_thread, ssdk_nl_sk, CLONE_KERNEL)) < 0){
        #endif
            aos_printk("thread can't be created for netlink\n");
            return;
        }
        dprintk("[%d] create child [%d] at %d\n", parent_pid, child_pid, loc);
        pid_find_save(parent_pid, child_pid);

    } else {
        dprintk("[%d] wake up child [%d] at %d\n", parent_pid, pid_childs[loc], loc);
        wake_up(&pid_child_wait[loc]);
    }

    return;
}

sw_error_t
sw_uk_init(a_uint32_t nl_prot)
{
    int i;
    
    if (!cmd_buf) {
        if((cmd_buf = (a_uint32_t *) aos_mem_alloc(SW_MAX_API_BUF)) == NULL)
            return SW_OUT_OF_MEM;  
    }

    if (!ssdk_nl_sk) {
        #ifdef KVER26
        ssdk_nl_sk = netlink_kernel_create(
        #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
        &init_net,
        #endif
        nl_prot, 
        0,  
        sw_api_netlink, 
        #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
        NULL,
        #endif
        THIS_MODULE);
        #else
        ssdk_nl_sk = netlink_kernel_create(nl_prot, sw_api_netlink);
        #endif
        if (!ssdk_nl_sk) {
            aos_printk("netlink_kernel_create fail at nl_prot:[%d]\n", nl_prot);
            return SW_NO_RESOURCE;
        }else { /* debug, jhung */
            #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
            aos_printk("netlink_kernel_create succeeded at nl_prot: [%d] (>2.6.22)\n", nl_prot);
            #else
            aos_printk("netlink_kernel_create succeeded at nl_prot: [%d] (<2.6.22)\n", nl_prot);
            #endif
        }
    }
    
    init_MUTEX(&pid_tab_sem);
    init_MUTEX(&api_sem);
    
    for(i=0; i< PID_TAB_MAX; i++) {
        init_waitqueue_head(&pid_child_wait[i]);
    }

    return SW_OK;
}

sw_error_t
sw_uk_cleanup(void)
{
    if (cmd_buf) {
        aos_mem_free(cmd_buf);
        cmd_buf = NULL;
    }

    if (ssdk_nl_sk) {
        #ifdef KVER26
        sock_release(ssdk_nl_sk->sk_socket);
        #else
        sock_release(ssdk_nl_sk->socket);
        #endif
        ssdk_nl_sk = NULL;
    }

    return SW_OK;
}

