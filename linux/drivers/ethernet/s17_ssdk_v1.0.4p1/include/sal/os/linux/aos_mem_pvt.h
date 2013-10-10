#ifndef _AOS_MEM_PVT_H
#define _AOS_MEM_PVT_H

#include <linux/slab.h>

static inline void *__aos_mem_alloc(aos_size_t size)
{
    return (kmalloc(size, GFP_KERNEL));
}

static inline void __aos_mem_free(void *buf)
{
    kfree(buf);
}

/* move a memory buffer */
static inline void
__aos_mem_copy(void *dst, void *src, aos_size_t size)
{
    memcpy(dst, src, size);
}

/* set a memory buffer */
static inline void
__aos_mem_set(void *buf, a_uint8_t b, aos_size_t size)
{
    memset(buf, b, size);
}

/* zero a memory buffer */
static inline void
__aos_mem_zero(void *buf, aos_size_t size)
{
    memset(buf, 0, size);
}

/* compare two memory buffers */
static inline int
__aos_mem_cmp(void *buf1, void *buf2, aos_size_t size)
{
    return (memcmp(buf1, buf2, size) == 0) ? 0 : 1;
}



#endif /*_AOS_MEM_PVT_H*/
