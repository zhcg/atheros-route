#ifndef _DATABASE_MANAGEMENT_H_
#define _DATABASE_MANAGEMENT_H_

#include "common_tools.h"

#define TB "terminal_base_tb"
#define REGISTERTB "terminal_register_tb"

// 结构体定义
struct class_database_management
{
    int (* clear)();
    int (* insert)(unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100], unsigned short *columns_value_len);
    int (* delete_row)(unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100]);
    int (* del)(unsigned char columns_count, char (*columns_name)[30]);
    int (* update)(unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100], unsigned short *columns_value_len);
    int (* select)(unsigned char columns_count, char (*columns_name)[30], char (*columns_value)[100]);
    int (* select_table)(unsigned char columns_count, char (*columns_name)[30], char *buf, unsigned short buf_len);
};

extern struct class_database_management database_management;
#endif
