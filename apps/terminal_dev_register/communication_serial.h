#ifndef _communication_serial_H_
#define _communication_serial_H_

#include "common_tools.h"
#include "communication_network.h"

#define SPI_NODE "/dev/spipassage" // spi数据节点

// 结构体定义
struct class_communication_serial
{
    unsigned char cmd;                // 命令字
    pthread_mutex_t *mutex;                                  // 互斥量
    pthread_cond_t *cond;                                    // 条件量
    
    
    int (* cmd_on_hook)();
    int (* cmd_call)(char *phone);
    int (* cmd_off_hook)();
    
    int (* recv_display_msg)(char *phone_num);
    int (* refuse_to_answer)(); // 拒接接听
    
    int (* phone_state_monitor)(unsigned char cmd);
    
    int (* send_data)(char *data, unsigned int data_len, struct timeval *tv);
    int (* recv_data)(char *data, unsigned int data_len, struct timeval *tv);
    
    int (* relay_change)(unsigned char mode);
    int (* loop_manage)(unsigned char mode);
};

extern struct class_communication_serial communication_serial;
#endif
