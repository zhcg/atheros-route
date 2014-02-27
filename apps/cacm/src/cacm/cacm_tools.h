/*************************************************************************
    > File Name: cacm_tools.h
    > Author: yangjilong
    > mail1: yangjilong65@163.com
    > mail2: yang.jilong@handaer.com
    > Created Time: 2013年10月30日 星期三 10时32分46秒
**************************************************************************/
#ifndef CACM_TOOLS_H_
#define CACM_TOOLS_H_

#include <sys/time.h>
#include <eXosip2/eXosip.h>

#include "common_tools.h"
#if BOARDTYPE == 5350
#include "nvram_interface.h"
#elif BOARDTYPE == 9344
#include "database_management.h"
#endif 
#include "communication_network.h"

#define CACM_STOP 0
#define PHONE_STATE_NODE "/dev/uartpassage"
#define PHONE_STATE_INTERFACE 3 // 电话线状态接口 1：spi interface接口读取； 2：接收本地socket；3：直接读取 节点uartpassage

enum CACM_ERR_NUM 
{ 
    INIT_REGISTER_ERR = -500,
    INIT_LOGOUT_ERR,
    BUILD_REGISTER_ERR,
    EXOSIP_INIT_ERR,
    EXOSIP_LISTEN_ERR,
    NO_EVENT_ERR,
    ADD_AUTHENTICATION_INFO_ERR,
};

/**
 * cpu 信息结构体
 */
struct s_cpu_info         
{
    unsigned char name[10];
    unsigned long user;
    unsigned char user_key[8];
    unsigned long system;
    unsigned char system_key[8];
    unsigned long nice;
    unsigned char nice_key[8];
    unsigned long idle;
    unsigned char idle_key[8];
    unsigned long io;
    unsigned char io_key[8];
    unsigned long irq;
    unsigned char irq_key[8];
    unsigned long softirq;
    unsigned char softirq_key[8];
};

/**
 * mem 信息结构体
 */
struct s_mem_info 
{
    unsigned char name[10];
    unsigned long used;
    unsigned char used_key[8];
    unsigned long free;
    unsigned char free_key[8];
    unsigned long shrd;
    unsigned char shrd_key[8];
    unsigned long buff;
    unsigned char buff_key[8];
    unsigned long cached;
    unsigned char cached_key[8];
};

/**
 * 进程 信息结构体
 */
struct s_process_info
{
    unsigned int pid;
    unsigned int ppid;
    unsigned char user[32];
    unsigned char stat[6];
    unsigned int vsz;
    unsigned int mem;
    unsigned int cpu;
    char name[64];
};

/**
 * flash 信息结构体
 */
struct s_flash_info
{
    unsigned long used;
};

struct s_cacm
{
    char base_sn[SN_LEN + 1];
    char user_name[64];
    char password[64];
    char proxy_ip[64];
    int proxy_port;
    char localip[64];
    int local_port;
    
    char register_uri[128];
    char from[128];
    char to[128];
    char contact[128];
    
    char identity[64]; //身份
    int expires;
    int id;
    
    unsigned char heartbeat_cycle;
    unsigned char business_cycle;
    
    int fd;
    unsigned char phone_state_type;
    char register_flag;                 // 注册成功标志
    char pthread_event_exit_flag;       // 事件监听线程退出标志
    char exit_flag;                     // 进程退出标志
    char hint[128];                     // 提示
    
    pthread_t pthread_event_manage_id;    // 事件处理线程ID
    pthread_t pthread_heartbeat_id;       // 心跳线程ID
    pthread_t pthread_base_state_id;      // base状态线程ID
    pthread_t pthread_real_time_event_id; // 实时事件线程ID
};

struct class_cacm_tools
{
    int (* init_register)(struct s_cacm *cacm);
    int (* init_logout)(struct s_cacm *cacm);
    int (* send_message)(struct s_cacm *cacm, char *buf);
    
    #if PHONE_STATE_INTERFACE == 1
    int (* recv_phone_state_msg)(char *buf, unsigned short len, struct timeval *timeout);
    #elif PHONE_STATE_INTERFACE == 2 
    int (* recv_phone_state_msg)(int fd, char *buf, unsigned short len, struct timeval *timeout);
    #elif PHONE_STATE_INTERFACE == 3 
    int (* recv_phone_state_msg)(int fd, char *buf, unsigned short len, struct timeval *timeout);
    #endif // PHONE_STATE_INTERFACE == 3 
    
    int (* phone_state_msg_unpack)(struct s_cacm *cacm, char *buf, unsigned short buf_len);
    int (* phone_state_monitor)(struct s_cacm *cacm, char *buf, unsigned short buf_len, struct timeval *timeout);
    int (* get_cpu_occupy_info)(struct s_cpu_info *cpu_info);
    int (* get_mem_occupy_info)(struct s_mem_info *mem_info);
    int (* get_process_resource_occupy_info)(char *process_info_buf, unsigned short buf_len);
    int (* get_flash_occupy_info)();
    int (* get_base_state)(struct s_cacm *cacm, char *buf, unsigned short buf_len);
};

extern struct class_cacm_tools cacm_tools;
#endif
