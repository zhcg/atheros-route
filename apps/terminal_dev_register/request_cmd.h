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
    char dev_name[32];
    char dev_id[16];
    char dev_mac[16];
    int dev_code;

    time_t end_time;
};

struct s_data_table // 此结构对应数据库
{
    char base_sn[35];
    char base_mac[18];
    char pad_sn[35];
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
    int cmd_count; // 接收命令的个数
    unsigned short length; // 数据包长度
    char data[256];
    
    pthread_mutex_t mutex; // 线程创建锁
    pthread_cond_t cond;   // 线程创建条件量
    pthread_t pad_cmd_handle_id; // 配置线程id
    
    volatile char cmd_word; // 命令字
    volatile char config_now_flag; // 正在配置标准 1：代表正在配置
    volatile char transmission_mode; // 交易模式 0代表网口 1 代表串口
    volatile char mutex_lock_flag; // 加锁标志位
    volatile char mode; // 请求方式

    struct s_data_table data_table;
};

// 结构体定义
struct class_request_cmd
{
    int (* init)();
    int (* request_cmd_0x01_02_03_07_08_09_0A)(struct s_terminal_dev_register * terminal_dev_register);
    
    int (* request_cmd_0x04)(struct s_terminal_dev_register * terminal_dev_register);
    int (* request_cmd_0x05)(struct s_terminal_dev_register * terminal_dev_register);
    int (* request_cmd_0x06)(struct s_terminal_dev_register * terminal_dev_register);
    int (* request_cmd_0x0B)(struct s_terminal_dev_register * terminal_dev_register);
    int (* request_cmd_0x0C)(struct s_terminal_dev_register * terminal_dev_register);
    int (* request_cmd_0x0D)(struct s_terminal_dev_register * terminal_dev_register);
    
    #if CTSI_SECURITY_SCHEME == 2
    int (* request_cmd_0x0E)(struct s_terminal_dev_register * terminal_dev_register);
    int (* request_cmd_0x0F)(struct s_terminal_dev_register * terminal_dev_register);
    int (* request_cmd_0x50)(struct s_terminal_dev_register * terminal_dev_register);
    int (* request_cmd_0x51)(struct s_terminal_dev_register * terminal_dev_register);
    #endif // CTSI_SECURITY_SCHEME == 2
    
    int (* request_cmd_0x52)(struct s_terminal_dev_register * terminal_dev_register);
    int (* request_cmd_0x53)(struct s_terminal_dev_register * terminal_dev_register);
    
    #if BOARDTYPE == 9344
    int (* request_cmd_0x54)(struct s_terminal_dev_register * terminal_dev_register);
    int (* request_cmd_0x55)(struct s_terminal_dev_register * terminal_dev_register);
    int (* request_cmd_0x56)(struct s_terminal_dev_register * terminal_dev_register);
    int (* request_cmd_0x57)(struct s_terminal_dev_register * terminal_dev_register);
    #endif // BOARDTYPE == 9344
    int (* request_cmd_analyse)(struct s_terminal_dev_register * terminal_dev_register);
    
    int (* init_data_table)(struct s_data_table *data_table);
    
    #if BOARDTYPE == 9344
    int (* cancel_mac_and_ip_bind)();
    #endif // BOARDTYPE == 9344
};

extern struct class_request_cmd request_cmd;
#endif

