#ifndef _TERMINAL_AUTHENTICATION_H_
#define _TERMINAL_AUTHENTICATION_H_

#include "common_tools.h"
#include "communication_serial.h"

#if BOARDTYPE == 6410 || BOARDTYPE == 9344
#include "database_management.h"
#elif BOARDTYPE == 5350
#include "nvram_interface.h"
#endif

#include "communication_network.h"
#include "communication_serial.h"

#define RELAY_CHANGE 1 // 是否切换继电器 1 切换

// 数据结构定义
struct class_terminal_authentication
{
    char static_device_token[TOKENLEN];
    char static_position_token[TOKENLEN];
    char static_device_flag;
    char static_position_flag;
    int (* return_device_token)(char *device_token);
    int (* return_position_token)(char *position_token);
    int (* rebuild_device_token)(char *device_token);
    int (* rebuild_position_token)(char *position_token);
    int (* start_up_CACM)();
};

extern struct class_terminal_authentication terminal_authentication;
#endif
