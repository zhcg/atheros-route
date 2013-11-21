#ifndef _UNIONPAY_MESSAGE_H_
#define _UNIONPAY_MESSAGE_H_

#include "common_tools.h"

// 结构体定义
struct class_unionpay_message
{
    int (* display_msg_unpack)(struct s_data_list *a_data_list);
    int (* link_request_msg_unpack)(struct s_data_list *a_data_list);
    int (* link_respond_msg_pack)(struct s_data_list *a_data_list);
    int (* data_finish_msg_unpack)(struct s_data_list *a_data_list);
    int (* data_finish_msg_pack)(struct s_data_list *a_data_list);
    int (* deal_msg_unpack)(struct s_data_list *a_data_list, void *para);
    int (* deal_pack)(struct s_data_list *a_data_list, void *para);
};

extern struct class_unionpay_message unionpay_message;
#endif
