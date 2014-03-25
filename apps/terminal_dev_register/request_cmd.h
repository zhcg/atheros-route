/*************************************************************************
    > File Name: request_cmd.h
    > Author: yangjilong
    > mail1: yangjilong65@163.com
    > mail2: yang.jilong@handaer.com
    > Created Time: 2014年01月18日 星期六 18时00分46秒
**************************************************************************/
#ifndef _REQUEST_CMD_H_
#define _REQUEST_CMD_H_

// 头文件添加
#include "common_tools.h"
#include "network_config.h"
#include "database_management.h"

struct s_dev_register
{
    char flag;
    char dev_name[32];
    char dev_id[16];
    char dev_mac[16];
    char dev_code[5];

    time_t end_time;
};

struct s_data_table // 此结构对应数据库
{
    char base_sn[SN_LEN + 1];
    char base_mac[18];
    char pad_sn[SN_LEN + 1];
    char pad_mac[18];
    unsigned char register_state;

    #if BOARDTYPE == 5350
    char default_ssid[22];
    char default_ssid_password[9];
    #endif
};

struct s_terminal_dev_register
{
    int fd;
    int network_config_fd; // 备份设置时的fd
    int non_network_fd; // 备份非网络fd
    int cmd_count; // 接收命令的个数
    unsigned short length; // 数据包长度
    char data[256];
    
    pthread_t request_cmd_0x01_02_03_07_08_09_0A_id; // 配置线程id
    
    volatile char cmd_word; // 命令字
    volatile char config_now_flag; // 正在配置标准 1：代表正在配置
    volatile char communication_mode; // 交易模式 0代表网口 1 代表串口
    
    struct s_data_table data_table;
};

// 结构体定义
struct class_request_cmd
{
    int (* init)();
    int (* request_cmd_analyse)(struct s_terminal_dev_register * terminal_dev_register);
    int (* init_data_table)(struct s_data_table *data_table);
    
    #if BOARDTYPE == 9344
    int (* cancel_mac_and_ip_bind)();
    #endif // BOARDTYPE == 9344
};

extern struct class_request_cmd request_cmd;
#endif

