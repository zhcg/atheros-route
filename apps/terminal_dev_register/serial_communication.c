#include "serial_communication.c"

// 初始化结构体
struct class_serial_communication serial_communication = 
{
    common_tools.send_data, common_tools.recv_data
};
