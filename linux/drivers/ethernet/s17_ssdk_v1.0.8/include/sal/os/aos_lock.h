#ifndef _AOS_LOCK_H
#define _AOS_LOCK_H


#ifdef KERNEL_MODULE
#include "sal/os/linux/aos_lock_pvt.h"
#else
#include "sal/os/linux_user/aos_lock_pvt.h"
#endif


typedef aos_lock_pvt_t aos_lock_t;


#define aos_lock_init(lock)  __aos_lock_init(lock)


#define aos_lock(lock) __aos_lock(lock)


#define aos_unlock(lock) __aos_unlock(lock)


#define aos_irq_save(flags) __aos_irq_save(flags)


#define aos_irq_restore(flags) __aos_irq_restore(flags)


#define aos_default_unlock  __aos_default_unlock


#endif
