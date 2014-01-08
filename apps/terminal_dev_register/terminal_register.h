#ifndef _TERMINAL_REGISTER_H_
#define _TERMINAL_REGISTER_H_

#include "common_tools.h"
#include "terminal_authentication.h"
#include "database_management.h"
#include "communication_network.h"

// 请求结构
struct s_request_pack
{
    unsigned int total_len;           // 消息总长度 = 包头长度 + 包体长度
    unsigned short message_type;      // 消息类型：1 请求消息；2 响应消息
    char version[5];                  // 用于表示协议的版本号  
    
    char func_id[27];                 // 功能编号，字符串：数字和下划线组成
    char start_time[26];              // 请求开始时间， 数字，单位为毫秒 
    
    char base_id[43];                 // BASE：“01A1010100100312122001”+ “DM9000 MAC” 等两段组成，
                                      // 如某个Base网卡DM9000的MAC是：11:33:55:77:99:0A，
                                      // 则该Base的SN是：01A101010010031212200111335577990A 。34字节
                                      // 6 + 1 + 34 + 1 = 42
    char base_mac[27];                // 7 + 1 + 17 + 1 = 26
    char pad_id[43];                  // 34字节 来自于PAD
    char pad_mac[27];                 // 6 + 1 + 17 = 25
    char token[128];                  // BASE_ID+PAD_ID+电话号码+8字节随机数 
};

// 响应的结构
struct s_respond_pack
{
    unsigned int total_len;           // 消息总长度 = 包头长度 + 包体长度
    unsigned short message_type;      // 消息类型：1 请求消息；2 响应消息
    char version[5];                  // 用于表示协议的版本号  
     
    char msg_data_status[19];          // 返回数据的状态 字符串 200：代表成功
    char s_data_process_time[25];     // 是指服务端对应答数据处理的时间  字符串:数字，单位为毫秒
    char start_time[26];              // 请求开始时间， 字符串:数字，单位为毫秒
    
    char base_user_name[60];          // Base 用户名 字符串
    char base_password[30];           // Base 密码 字符串
    char pad_user_name[60];           // Pad 用户名 字符串
    char pad_password[30];            // Pad 密码 字符串
    char sip_ip_address[30];          // SIP服务器地址
    char sip_port[10];                // SIP端口
    char heart_beat_cycle[25];        // 心跳周期（整形字符串 单位秒）
    char business_cycle[25];          // 业务监控周期 字符串
};

struct class_terminal_register
{
    struct s_respond_pack *respond_pack;
    int (* get_sip_info)(char *pad_sn, char *pad_mac);
    int (* user_register)(char *pad_sn, char *pad_mac);
    int (* get_base_sn_and_mac)(char *base_sn, char *base_mac);
};

extern struct class_terminal_register terminal_register;
#endif
