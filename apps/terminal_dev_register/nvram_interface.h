#ifndef _NVRAM_INTERFACE_H_
#define _NVRAM_INTERFACE_H_

// 头文件添加
#include "common_tools.h"

// 结构体定义
struct class_nvram_interface
{
    int (* insert)(unsigned char flash_block_index, unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100], unsigned short *columns_value_len);
    int (* del)(unsigned char flash_block_index, unsigned char columns_count, char (*columns_name)[30]);
    int (* update)(unsigned char flash_block_index, unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100], unsigned short *columns_value_len);
    int (* select)(unsigned char flash_block_index, unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100]);
    
    int (* clear)();
    
    key_t sem_key;
    pthread_mutex_t mutex;
};

extern struct class_nvram_interface nvram_interface;
#endif
