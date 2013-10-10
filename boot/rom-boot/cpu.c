#include "asm/types.h"
#include "ar7240_soc.h"

extern void dcache_flush_range(__u32 start, __u32 end);

void flush_cache (unsigned long start_addr, unsigned long size)
{
    __u32 end, a;

    a = start_addr & ~(CFG_CACHELINE_SIZE - 1);
    size = (size + CFG_CACHELINE_SIZE - 1) & ~(CFG_CACHELINE_SIZE - 1);
    end = a + size;

    dcache_flush_range(a, end);
}

