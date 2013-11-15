#ifndef _SERIAL_COMMUNICATION_H_
#define _SERIAL_COMMUNICATION_H_

#include "common_tools.h"

// 结构体定义
struct class_serial_communication
{
    int (* send)(unsigned int fd, unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv);
    int (* recv)(unsigned int fd, unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv);
};

extern struct class_serial_communication serial_communication;
#endif
