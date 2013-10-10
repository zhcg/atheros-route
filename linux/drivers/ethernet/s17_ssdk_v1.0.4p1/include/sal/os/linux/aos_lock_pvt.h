#ifndef _AOS_LOCK_PVT_H
#define _AOS_LOCK_PVT_H


#include <linux/interrupt.h>
#include <linux/spinlock.h>


typedef spinlock_t aos_lock_pvt_t;


#define __aos_lock_init(lock)  spin_lock_init(lock)


#define __aos_lock(lock) spin_lock(lock)


#define __aos_unlock(lock) spin_unlock(lock)


#define __aos_irq_save(flags) local_irq_save(flags)


#define __aos_irq_restore(flags) local_irq_restore(flags)


#define __aos_default_unlock SPIN_LOCK_UNLOCKED


#endif /*_AOS_LOCK_PVT_H*/

