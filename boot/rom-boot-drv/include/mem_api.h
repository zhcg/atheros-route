#ifndef _MEM_API_H
#define _MEM_API_H

#include <asm/types.h>

#define HORNET_MEMORY_DDR     0
#define HORNET_MEMORY_DDR2    1
#define HORNET_MEMORY_SDRAM   2

struct memory_api {
    void (*_mem_init)(u8 type);
};

#endif

