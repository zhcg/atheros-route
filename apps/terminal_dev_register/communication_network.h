#ifndef _communication_network_H_
#define _communication_network_H_

#include "common_tools.h"
#include "communication_serial.h"

// 结构体定义
struct class_communication_network
{
    int (* make_server_link)(const char *port);
    int (* accept_client_connection)(const int server_fd);
    int (* make_client_link)(const char *ip, const char *port); 
    int (* msg_pack)(struct s_data_list *a_data_list, struct s_dial_back_respond *a_dial_back_respond);
    int (* msg_unpack)(char *buf, unsigned short buf_len, struct s_dial_back_respond *a_dial_back_respond);
    int (* msg_send)(int fd, struct s_data_list *a_data_list);
    int (* msg_recv)(int fd, char *buf, unsigned short buf_len);
};
extern struct class_communication_network communication_network;
#endif
