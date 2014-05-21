#include "network_config.h"


static int serial_pad_fd = 0, serial_5350_fd = 0, server_pad_fd = 0, server_base_fd = 0;
static char network_flag = 0; // 否已经注册成功
static char g_cmd = 0xFF;
static char pad_cmd = INITUAL_STATUS;

/**
 * 初始化运行环境
 */
static int init_env();

/**
 * 发送数据到PAD
 */
static int send_msg_to_pad(int fd, char cmd, char *data, unsigned short data_len);

/**
 * 接收pad发送的报文包
 */
static int recv_msg_from_pad(int fd, struct s_pad_and_6410_msg *a_pad_and_6410_msg);



struct class_network_config network_config = 
{
    .serial_pad_fd = &serial_pad_fd,
    .serial_5350_fd = &serial_5350_fd,
    .server_pad_fd = &server_pad_fd,
    .server_base_fd = &server_base_fd,
    .usb_pad_fd = 0,
    .network_flag = &network_flag,
    .cmd = &g_cmd,
    .pad_cmd = &pad_cmd,
    .pthread_flag = 0,
    .pthread_recv_flag = 0,
    
    .init_env = init_env, 
    .send_msg_to_pad = send_msg_to_pad,
    .recv_msg_from_pad = recv_msg_from_pad,
};

/**
 * 初始化运行环境
 */
int init_env()
{
    PRINT_STEP("entry...\n");
    int res = 0;
    char buf[64] = {0};
    
    PRINT("USB_NODE = %s\n", USB_NODE);
    if ((res = open(USB_NODE, O_RDWR, 0644)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "open failed", OPEN_ERR); 
        return OPEN_ERR;
    }
    network_config.usb_pad_fd = res;
    
    memset(buf, 0, sizeof(buf));
    
    // 建立服务器
    if ((server_base_fd = communication_network.make_server_link(common_tools.config->pad_client_port)) < 0)
    {
        if (serial_pad_fd != 0)
        {
            close(serial_pad_fd);
            serial_pad_fd = 0;
        }
        if (serial_5350_fd != 0)
        {
            close(serial_5350_fd);
            serial_5350_fd = 0;
        }
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "make_server_link failed", server_base_fd); 
        return server_base_fd;
    }
    sprintf(buf, "The server is established successfully! port = %s", common_tools.config->pad_client_port);    
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, buf, 0);
    
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 发送数据到PAD
 */
int send_msg_to_pad(int fd, char cmd, char *data, unsigned short data_len)
{
    PRINT_STEP("entry...\n");
    int res = 0;
    char *send_buf = NULL;
    struct timeval tv = {common_tools.config->one_byte_timeout_sec, common_tools.config->one_byte_timeout_usec};
    unsigned short buf_len = 0;
    unsigned short msg_len = 0;
    int i = 0;
    if (data == NULL)
    {
        buf_len = 2 + 2 + 1 + 1;   
    }
    else
    {
        buf_len = 2 + 2 + 1 + data_len + 1;
    }
    
    if ((send_buf = (char *)malloc(buf_len)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed", MALLOC_ERR);         
        return MALLOC_ERR;
    }
    
    // 打包
    send_buf[0] = 0x5A;
    send_buf[1] = 0xA5;
    if (data == NULL)
    {
        send_buf[2] = 1;
        send_buf[3] = 0;
        send_buf[4] = cmd;
        send_buf[5] = cmd;
    }
    else
    {
        msg_len = data_len + 1;
        #if ENDIAN == 0
        memcpy(send_buf + 2, &msg_len, sizeof(unsigned short));
        #else
        msg_len = DATA_ENDIAN_CHANGE_SHORT(msg_len);
        memcpy(send_buf + 2, &msg_len, sizeof(unsigned short));        
        #endif
        
        send_buf[4] = cmd;
        memcpy(send_buf + 5, data, data_len);
        
        send_buf[buf_len - 1] = (cmd ^ common_tools.get_checkbit(data, NULL, 0, data_len, XOR, 1));
    }
    
    // 发送
	PRINT("buf_len = %d\n", buf_len);
    if ((res = common_tools.send_data(fd, send_buf, NULL, buf_len, &tv)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed", res);
        free(send_buf); 
        send_buf = NULL;
        return common_tools.get_errno('P', res);
    }
    
    PRINT_BUF_BY_HEX(send_buf, NULL, buf_len, __FILE__, __FUNCTION__, __LINE__);
    free(send_buf);
    send_buf = NULL;
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 接收pad发送的报文包
 */
int recv_msg_from_pad(int fd, struct s_pad_and_6410_msg *a_pad_and_6410_msg)
{
    PRINT_STEP("entry...\n");
    int res = 0;
    char tmp = 0;
    struct timeval tv = {common_tools.config->one_byte_timeout_sec, common_tools.config->one_byte_timeout_usec};
    
    if (a_pad_and_6410_msg != NULL)
    {
        PRINT("___from pad start___\n");
        // 接收数据头
        if ((res = common_tools.recv_data_head(fd, a_pad_and_6410_msg->head, sizeof(a_pad_and_6410_msg->head), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_data_head failed", res);
            return common_tools.get_errno('P', res);
        }
        
        // 接收数据长度
        if ((res = common_tools.recv_one_byte(fd, &tmp, &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_one_byte failed", res);
            return common_tools.get_errno('P', res);;
        }
        a_pad_and_6410_msg->len = tmp;
        
        if ((res = common_tools.recv_one_byte(fd, &tmp, &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_one_byte failed", res);
            return common_tools.get_errno('P', res);
        }
        a_pad_and_6410_msg->len += (tmp << 8);
        // 接收命令字
        if ((res = common_tools.recv_one_byte(fd, &a_pad_and_6410_msg->cmd, &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_one_byte failed", res); 
            return common_tools.get_errno('P', res);
        }        
        if ((a_pad_and_6410_msg->len - 1) > 0)          
        {
            // 动态申请
            if ((a_pad_and_6410_msg->data = (char *)malloc(a_pad_and_6410_msg->len)) == NULL)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed", MALLOC_ERR);
                return MALLOC_ERR;
            }
            memset((void *)a_pad_and_6410_msg->data, 0, a_pad_and_6410_msg->len);
            
            // 接收有效数据
            if ((res = common_tools.recv_data(fd, a_pad_and_6410_msg->data, NULL, a_pad_and_6410_msg->len - 1, &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_data failed", res);
                return common_tools.get_errno('P', res);;
            }
        }
        // 接收校验位
        if ((res = common_tools.recv_one_byte(fd, &a_pad_and_6410_msg->check, &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_one_byte failed", res);
            return common_tools.get_errno('P', res);
        }
        
        printf("\n"); 
        PRINT("___from pad end___\n");
        if ((a_pad_and_6410_msg->len - 1) > 0) 
        { 
            // 校验位对比       
            if ((a_pad_and_6410_msg->cmd ^ common_tools.get_checkbit(a_pad_and_6410_msg->data, NULL, 0, a_pad_and_6410_msg->len, XOR, 1)) != a_pad_and_6410_msg->check)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "check is different!", P_CHECK_DIFF_ERR);
                return P_CHECK_DIFF_ERR;         
            }
        }
        else
        {
            if (a_pad_and_6410_msg->check != a_pad_and_6410_msg->cmd)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "check is different!", P_CHECK_DIFF_ERR);
                return P_CHECK_DIFF_ERR;         
            }
        }
    }
    else
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data buf recv success!", 0);
    PRINT_STEP("exit...\n");
    return 0;
}
