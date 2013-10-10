
#ifndef _AOS_TIMER_H
#define _AOS_TIMER_H

#include "sal/os/aos_types.h"
#ifdef KERNEL_MODULE
#include "sal/os/linux/aos_timer_pvt.h"
#else
#include "sal/os/linux_user/aos_timer_pvt.h"
#endif


typedef __aos_timer_t           aos_timer_t;


/*
 * Delay in microseconds
 */
static inline void
aos_udelay(int usecs)
{
    return __aos_udelay(usecs);
}

/*
 * Delay in milliseconds.
 */
static inline void
aos_mdelay(int msecs)
{
    return __aos_mdelay(msecs);
}
    

#endif

