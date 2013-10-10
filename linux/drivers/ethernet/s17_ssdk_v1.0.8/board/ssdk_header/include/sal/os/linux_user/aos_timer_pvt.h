#ifndef _AOS_TIMER_PVT_H
#define _AOS_TIMER_PVT_H

#include <unistd.h>

typedef int __aos_timer_t;

static inline void
__aos_udelay(int usecs)
{
    usleep(usecs);
    return;
}

static inline void
__aos_mdelay(int msecs)
{
    usleep(1000*msecs);
    return;
}

#endif /*_AOS_TIMER_PVT_H*/
