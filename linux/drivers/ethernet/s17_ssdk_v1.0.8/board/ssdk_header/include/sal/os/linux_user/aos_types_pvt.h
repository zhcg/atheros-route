#ifndef _AOS_PVTTYPES_H
#define _AOS_PVTTYPES_H

#include <asm/types.h>
#ifndef GCCV4
#include <linux/compiler.h>
#endif
#include <stdio.h>
/*
 * Private definitions of general data types
 */

typedef void* __aos_device_t;
typedef int __aos_size_t;
typedef int __aos_iomem_t;

typedef __u8              __a_uint8_t;    
typedef __s8              __a_int8_t;     
typedef __u16             __a_uint16_t;   
typedef __s16             __a_int16_t;    
typedef __u32             __a_uint32_t;   
typedef __s32             __a_int32_t;    
typedef __u64             __a_uint64_t;   
typedef __s64             __a_int64_t;    


#define aos_printk        printf


#endif 
