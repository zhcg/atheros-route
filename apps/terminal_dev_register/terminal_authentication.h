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

// 数据结构定义
struct class_terminal_authentication
{
    #if CTSI_SECURITY_SCHEME == 1
    int (* insert_token)();
    int (* delete_token)();
    int (* update_token)();
    #else
    char static_device_token[16];
    char static_position_token[16];
    char static_device_flag;
    char static_position_flag;
    int (* return_device_token)(char *device_token);
    int (* return_position_token)(char *position_token);
    int (* rebuild_device_token)(char *device_token);
    int (* rebuild_position_token)(char *position_token);
    int (* start_up_CACM)();
    #endif
};

extern struct class_terminal_authentication terminal_authentication;
#endif
