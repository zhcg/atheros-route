#ifndef _NETWORK_CONFIG_H_
#define _NETWORK_CONFIG_H_

#include "common_tools.h"
#include "communication_network.h"
#include "terminal_register.h"
#if BOARDTYPE == 9344
#include "database_management.h"
#include "communication_usb.h"
#endif

#define READONLY_ROOTFS 1
#define USER_REGISTER 1 // 1：做终端认证流程

#define USB_NODE "/dev/usbpassage" // usb 节点

#ifndef USB_NODE // 如果没有定义USB_NODE宏
#define USB_INTERFACE 1 // 1：通过读取socket转发数据；2：读取USB节点
#else 
#define USB_INTERFACE 2 // 1：通过读取socket转发数据；2：读取USB节点
#endif // ifndef USB_NODE

/**
 * 初始化状态
 */
enum enum_init_state
{
    CONFIGURING_STATUS = 0xFA, // 正在配置状态
    INITUAL_STATUS = 0xFB, // 初始状态
    WRONGFUL_DEV_STATUS = 0xFC, // 不合法设备状态
    LAN_OK_STATUS = 0xFD, // 局域网正常状态
    WAN_OK_STATUS = 0xFE, // 广域网正常状态
    SUCCESS_STATUS = 0x00, // 成功状态
};

/**
 * 请求命令
 */
enum enum_request_cmd
{
    DYNAMIC_CONFIG_AND_REGISTER = 0x01, // 动态IP + 注册
    STATIC_CONFIG_AND_REGISTER = 0x02, // 静态IP + 注册
    PPPOE_CONFIG_AND_REGISTER = 0x03, // PPPOE + 注册
    
    ASK_BASE = 0x04, // 询问BASE
    
    PAD_MAKE_IDENTIFYING_CODE = 0x05, // 注册命令，PAD将随机生成的4字节串码发给base
    INTELLIGENT_DEV_REGISTER_REQUEST = 0x06, // 注册命令，智能设备将 设备名称、id、mac发送给BASE
    INTELLIGENT_DEV_CHANGE_NAME = 0x0B, // 设备名称修改命令，智能设备将 mac、设备名称发送给BASE
    
    RESTORE_FACTORY = 0x0C, // 恢复出厂（终端初始化前的状态）
    BASE_VERSION = 0x0D, // 查看Base版本
    
    GET_DEV_TOKEN = 0x0E, // 获取身份令牌
    GET_POSITION_TOKEN = 0x0F, // 获取位置令牌
    REDRAW_DEV_TOKEN = 0x50, // 重新生成身份令牌
    REDRAW_POSITION_TOKEN = 0x51, // 重新生成位置令牌
    
    GET_WAN_STATE = 0x52, // 查看WAN口
    STOP_CONFIG = 0x53, // 取消当前配置
    
    INTELLIGENT_DEV_IDENTIFYING_CODE_COMPARE = 0x54, // 串码对比，智能设备将输入的串码发送到base，base进行对比
    PAD_GET_REQUEST_REGISTER_DEV = 0x55, // PAD扫描“发送注册申请”的设备
    PAD_GET_REGISTER_SUCCESS_DEV = 0x56, // 查询已经注册的设备
    PAD_LOGOUT_DEV_BY_SN = 0x57, // 注销命令，删除匹配序列号的行
    INTELLIGENT_DEV_LOGOUT = 0x58, // 注销命令，智能设备将mac地址发送到base
    
    INIT_LAN = 0x60,
    ERROR_STATUS = 0xFF, // 错误状态
};

// pad和6410交互包结构
struct s_pad_and_6410_msg
{
    char head[2];                  // 数据头
    unsigned short len;            // 数据长度 = 命令字长度 + 有效数据长度
    unsigned char cmd;             // 命令字
    char *data;                    // 有效数据 
    char check;                    // 校验位
};

// 6410和5350交互包结构
struct s_6410_and_5350_msg
{
    char head[2];                  // 数据头
    unsigned short len;            // 数据长度 = 有效数据长度（两个字节低位在前）
    char *data;                    // 有效数据 
    char check;                    // 校验位
};

// 命令结构体
struct s_cmd
{
    unsigned char cmd_word;        // 命令字
    unsigned char cmd_bit;         // 命令更改标志位
    char cmd_key[32];              // 命令关键字
    char set_cmd[64];              // 设置命令
    char set_value[64];            // 设置命令值
    char set_cmd_and_value[128];   // 设置命令 + 设置命令值
    #if BOARDTYPE == 5350
    char get_cmd[64];              // 查询命令
    #endif
};

#pragma pack(4)
struct class_network_config
{
    int *serial_pad_fd;
    int *serial_5350_fd;
    int *server_pad_fd;
    int *server_base_fd; 
    int usb_pad_fd;
    char *network_flag; // 是否已经注册成功
    char *cmd;
    char *pad_cmd;
    volatile char pthread_flag;
    volatile char pthread_recv_flag; // 线程接收标志位
    pthread_mutex_t recv_mutex;  // 线程接收互斥锁
    
    #if BOARDTYPE == 5350 || BOARDTYPE == 9344
    void (* init_cmd_list)();
    #endif
    int (* init_env)();
    int (* send_msg_to_pad)(int fd, char cmd, char *data, unsigned short data_len);
    int (* recv_msg_from_pad)(int fd, struct s_pad_and_6410_msg *a_pad_and_6410_msg);
    int (* network_settings)(int fd, int cmd_count, char cmd_word);
    
    #if BOARDTYPE == 6410 
    int (* send_msg_to_5350)(int fd, char *cmd, unsigned short len);
    int (* recv_msg_from_5350)(int fd, struct s_6410_and_5350_msg *a_6410_and_5350_msg);
    int (* get_pppoe_state)(int fd);
    int (* route_config)(int fd, int index);
    
    #elif BOARDTYPE == 5350
    int (* get_wan_state)();
    int (* get_pppoe_state)();
    int (* config_route_take_effect)();
    int (* route_config)(int index);
    int (* route_config2)(int index);
    #elif BOARDTYPE == 9344
    int (* get_wan_state)();
    int (* config_route_take_effect)();
    int (* route_config)(int index);
    #endif
    
    char base_sn[SN_LEN + 1];
    char base_mac[18];
    char base_ip[16];
    struct s_cmd cmd_list[40];
};
#pragma pop()
extern struct class_network_config network_config;
#endif
