/*************************************************************************
    > File Name: request_cmd.c
    > Author: yangjilong
    > mail1: yangjilong65@163.com
    > mail2: yang.jilong@handaer.com
    > Created Time: 2014年01月18日 星期六 18时00分31秒
**************************************************************************/
#include "request_cmd.h"
static time_t start_time;
static unsigned short dev_count = 0;
static void *dev_info = NULL;

/**
 * 初始化请求命令
 */
static int init();

/**
 * 此线程处理网络设置和终端初始化
 */
static void * request_cmd_0x01_02_03_07_08_09_0A_pthread(void* para);

/**
 * 请求命令字分析 网络设置 和 注册
 */
static int request_cmd_0x01_02_03_07_08_09_0A(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x04 询问
 */
static int request_cmd_0x04(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x05: 5350 生成默认SSID; 9344 注册命令，PAD将随机生成的4字节串码发给base
 */
static int request_cmd_0x05(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x06 5350时 注销默认SSID；9344时 注册命令，智能设备将 设备名称、id、mac发送给BASE
 */
static int request_cmd_0x06(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x0B 
 * 5350时 网络设置询问
 * 9344时 设备名称修改命令，智能设备将 mac、设备名称发送给BASE
 */
static int request_cmd_0x0B(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x0C 恢复出厂
 */
static int request_cmd_0x0C(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x0D 查看Base版本信息
 */
static int request_cmd_0x0D(struct s_terminal_dev_register * terminal_dev_register);

#if CTSI_SECURITY_SCHEME == 2
/**
 * 请求命令字分析 0x0E 获取身份令牌
 */
static int request_cmd_0x0E(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x0F 获取位置令牌
 */
static int request_cmd_0x0F(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x50 重新生成身份令牌
 */
static int request_cmd_0x50(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x51 重新生成位置令牌
 */
static int request_cmd_0x51(struct s_terminal_dev_register * terminal_dev_register);
#endif // CTSI_SECURITY_SCHEME == 2

/**
 * 请求命令字分析 0x52 查看WAN口
 */
static int request_cmd_0x52(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x53 取消当前操作
 */
static int request_cmd_0x53(struct s_terminal_dev_register * terminal_dev_register);

#if BOARDTYPE == 9344
/**
 * 请求命令字分析 0x54 串码对比，智能设备将输入的串码发送到base，base进行对比
 */
static int request_cmd_0x54(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x55 PAD扫描“发送注册申请”的设备
 */
static int request_cmd_0x55(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x56 查询已经注册的设备
 */
static int request_cmd_0x56(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x57 注销命令，删除匹配序列号的行
 */
static int request_cmd_0x57(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x58 注销命令，智能设备，删除匹配MAC的行
 */
static int request_cmd_0x58(struct s_terminal_dev_register * terminal_dev_register);
#endif // BOARDTYPE == 9344

/**
 * 请求命令字分析
 */
static int request_cmd_analyse(struct s_terminal_dev_register * terminal_dev_register);

/*************************************************************************************************/
/*************************************************************************************************/
/*************************************************************************************************/

/**
 * 获取注册状态
 */
static int get_register_state();

/**
 * 初始化化表
 */
static int init_data_table(struct s_data_table *data_table);

#if BOARDTYPE == 9344
/**
 * 取消mac地址绑定
 */
static int cancel_mac_and_ip_bind();

/**
 * 获取BASE中存储的信息给PAD
 */
static int get_success_buf(char **buf);

/**
 * 打印设备列表
 */
static int print_dev_table(unsigned short dev_count, void *dev_info);

/**
 * 判断此设备是否在此表中
 * 0:不存在
 * 1:存在
 * < 0:错误
 */
static int is_in_dev_table(unsigned short dev_count, void *dev_info, struct s_dev_register *dev_register);

/**
 * 添加指定行 （内存）
 */
static int add_dev_info(unsigned short *dev_count, void **dev_info, struct s_dev_register *dev_register);

/**
 * 更新指定行串码和串码时间
 */
static int update_dev_info_code(unsigned short dev_count, void *dev_info, struct s_dev_register *dev_register);

/**
 * 删除指定行（内存）
 */
static int delete_dev_info(unsigned short *dev_count, void **dev_info, struct s_dev_register *dev_register);

/**
 * 查询内存表（内存）
 */
static int select_dev_info(unsigned short dev_count, void *dev_info, char *buf, unsigned short len);

/**
 * 串码对比
 */
static int dev_code_comparison(unsigned short dev_count, void *dev_info, struct s_dev_register *dev_register);

/**
 *  判断是否在数据库（数据库）
 */
static int is_in_dev_database(struct s_dev_register *dev_register);

/**
 * 添加指定行 （数据库）
 */
static int database_insert_dev_info(struct s_dev_register *dev_register);

/**
 * 删除指定行 （数据库）
 */
static int database_delete_dev_info(struct s_dev_register *dev_register);

/**
 * 查询注册成功的设备
 */
static int database_select_dev_info(char *buf, unsigned short len);

#endif // BOARDTYPE == 9344
/*************************************************************************************************/
/*************************************************************************************************/
/*************************************************************************************************/


struct class_request_cmd request_cmd = 
{
    .init = init,
    .request_cmd_analyse = request_cmd_analyse,
    .init_data_table = init_data_table,
    
    #if BOARDTYPE == 9344
    .cancel_mac_and_ip_bind = cancel_mac_and_ip_bind,
    #endif // BOARDTYPE == 9344
};

/**
 * 初始化请求命令
 */
int init()
{
    common_tools.work_sum->work_sum = 1;
    start_time = time(0);
    return 0;
}

/**
 * 此线程处理网络设置和终端初始化
 */
void * request_cmd_0x01_02_03_07_08_09_0A_pthread(void* para)
{
    PRINT("request_cmd_0x01_02_03_07_08_09_0A_pthread entry!\n");
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);          //允许退出线程
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,   NULL);   //设置立即取消

    int i = 0;
    int res = 0, ret = 0;
    struct s_terminal_dev_register * terminal_dev_register = (struct s_terminal_dev_register *)para;

    int fd = terminal_dev_register->fd;
    int len = 0;
    unsigned short insert_len = 0;
    char *buf = NULL;
    char port[10] = {0};
    char columns_name[3][30] = {0};
    char columns_value[3][100] = {0};
    
    PRINT("fd = %d\n", fd);
    // 相关设置 （包括设备认证和SIP信息获取）
    if ((res = network_config.network_settings(fd, terminal_dev_register->cmd_count, terminal_dev_register->cmd_word)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "network_config failed", res);
        network_config.pthread_recv_flag = 0;
        PERROR("network_settings failed!\n");
        pthread_mutex_unlock(&network_config.recv_mutex);
        PERROR("network_settings failed!\n");
        goto EXIT;
    }
    PRINT("after network_settings !\n");
    PRINT("*network_config.network_flag = %d\n", *network_config.network_flag);
    if (*network_config.network_flag == 1) // 说明此时是网络设置 7799
    {
        memcpy(port, common_tools.config->pad_server_port2, strlen(common_tools.config->pad_server_port2));
        PRINT("port = %s\n", port);
    }
    else // 7789
    {
        memcpy(port, common_tools.config->pad_server_port, strlen(common_tools.config->pad_server_port));
        PRINT("port = %s\n", port);
    }
    
    // 连接PAD端服务器
    for (i = 0; i < common_tools.config->repeat; i++)
    {
        if ((*network_config.server_pad_fd = communication_network.make_client_link(common_tools.config->pad_ip, port)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "make_client_link failed", *network_config.server_pad_fd);
            if (i != (common_tools.config->repeat - 1))
            {
                sleep(3);
                continue;
            }
        }
        break;
    }

    // 连接失败时
    if (*network_config.server_pad_fd < 0)
    {
        res = *network_config.server_pad_fd;
        ret = common_tools.get_errno('P', *network_config.server_pad_fd);
    }
    else
    {
        #if BOARDTYPE == 5350
        // 网口时关闭
        if (terminal_dev_register->communication_mode == 1)
        {
            PRINT("fd = %d\n", fd);
            close(fd);
        }
        terminal_dev_register->communication_mode = 0;
        
        #elif BOARDTYPE == 9344
        
        PRINT("before close! fd = %d terminal_dev_register->fd = %d\n", fd, terminal_dev_register->fd);
        #if USB_INTERFACE == 1
        shutdown(fd, SHUT_RDWR);
        PERROR("after shutdown\n");
        close(fd);
        #elif USB_INTERFACE == 2
        
        // 网口时关闭
        if (terminal_dev_register->communication_mode == 1)
        {
            PRINT("fd = %d\n", fd);
            close(fd);
        }
        terminal_dev_register->communication_mode = 0;
        
        #endif // USB_INTERFACE == 2
        
        #endif // BOARDTYPE == 9344
        fd = *network_config.server_pad_fd;
    }

    if ((res == 0) && (ret == 0) && (*network_config.network_flag == 1)) // 说明此时是网络设置
    {
        if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, NULL, 0)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
            goto EXIT;
        }
    }
    else if ((res == 0) && (ret == 0))
    {
        #if USER_REGISTER == 1
        len = strlen("base_sn:") + strlen("base_mac:") + strlen("base_ip:") +
            strlen(network_config.base_sn) + strlen(network_config.base_mac) + strlen(network_config.base_ip) +
            strlen(terminal_register.respond_pack->base_user_name) + strlen(terminal_register.respond_pack->base_password) +
            strlen(terminal_register.respond_pack->pad_user_name) + strlen(terminal_register.respond_pack->pad_password) +
            strlen(terminal_register.respond_pack->sip_ip_address) + strlen(terminal_register.respond_pack->sip_port) +
            strlen(terminal_register.respond_pack->heart_beat_cycle) + strlen(terminal_register.respond_pack->business_cycle) + 12;
        PRINT("len = %d\n", len);

        if ((buf = (char *)malloc(len)) == NULL)
        {
            res = MALLOC_ERR;
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed", res);
            goto EXIT;
        }

        memset(buf, 0, len);
        sprintf(buf, "base_sn:%s,base_mac:%s,base_ip:%s,%s,%s,%s,%s,%s,%s,%s,%s", network_config.base_sn, network_config.base_mac, network_config.base_ip,
            terminal_register.respond_pack->base_user_name, terminal_register.respond_pack->base_password,
            terminal_register.respond_pack->pad_user_name, terminal_register.respond_pack->pad_password,
            terminal_register.respond_pack->sip_ip_address, terminal_register.respond_pack->sip_port,
            terminal_register.respond_pack->heart_beat_cycle, terminal_register.respond_pack->business_cycle);

        #else // 不进行设备认证时

        memset(columns_name, 0, sizeof(columns_name));
        memset(columns_value, 0, sizeof(columns_value));
        memcpy(columns_name[0], "base_sn", strlen("base_sn"));
        memcpy(columns_name[1], "base_mac", strlen("base_mac"));
        memcpy(columns_name[2], "base_ip", strlen("base_ip"));

        #if BOARDTYPE == 5350
        if ((res = nvram_interface.select(RT5350_FREE_SPACE, 3, columns_name, columns_value)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);
            goto EXIT;
        }
        #elif BOARDTYPE == 9344
        if ((res = database_management.select(3, columns_name, columns_value)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);
            goto EXIT;
        }
        #endif
        len = strlen("base_sn:") + strlen("base_mac:") + strlen("base_ip:") +
        strlen(columns_value[0]) + strlen(columns_value[1]) + strlen(columns_value[2]) + 4;
        PRINT("len = %d\n", len);

        if ((buf = (char *)malloc(len)) == NULL)
        {
            res = MALLOC_ERR;
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed", res);
            goto EXIT;
        }

        memset(buf, 0, len);
        sprintf(buf, "base_sn:%s,base_mac:%s,base_ip:%s", columns_value[0], columns_value[1], columns_value[2]);
        #endif // #if USER_REGISTER == 1

        PRINT("%s\n", buf);

        // 正确信息发送到PAD
        if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, buf, strlen(buf))) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
            goto EXIT;
        }
        
        #if USER_REGISTER == 1
        
        #if 1 // 启动CACM
        if (terminal_authentication.start_up_CACM() < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "start_up_CACM failed!", -1);
        }
        else
        {
            PRINT("start_up_CACM success!\n");
        }
        #endif
        
        #endif // #if USER_REGISTER == 1
    }

EXIT:
    PRINT("res = %d\n", res);
    if (buf != NULL)
    {
        free(buf);
        buf = NULL;
    }

     // 当如上操作出现错误时，返回错误原因
    if (((res < 0) && (res != STOP_CMD)) || (ret < 0))
    {
        if (res == 0)
        {
            res = ret;
        }

        if (common_tools.get_user_prompt(res, &buf) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_user_prompt failed", res);
            if (buf != NULL)
            {
                free(buf);
                buf = NULL;
            }
        }
        else
        {
            PRINT("strlen(buf) = %d\n", strlen(buf));
            if ((res = network_config.send_msg_to_pad(fd, 0xFF, buf, strlen(buf))) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed!", res);
            }
            if (buf != NULL)
            {
                free(buf);
                buf = NULL;
            }
        }
    }
    else if (res == STOP_CMD)
    {
        if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, NULL, 0)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed!", res);
        }
    }

    if (((unsigned char)*network_config.cmd != 0xFF) && (*network_config.network_flag != 1)) // *network_config.cmd  等于如下值时 0x00 0xFC 0xFB 0xFD 0xFE，插入数据库
    {
        memset(columns_name[0], 0, sizeof(columns_name[0]));
        memset(columns_value[0], 0, sizeof(columns_value[0]));
        memcpy(columns_name[0], "register_state", strlen("register_state"));

        sprintf(columns_value[0], "%d", (unsigned char)*network_config.cmd);
        insert_len = strlen(columns_value[0]);
        PRINT("columns_value[0] = %s\n", columns_value[0]);

        for (i = 0; i < common_tools.config->repeat; i++)
        {
            #if BOARDTYPE == 6410 || BOARDTYPE == 9344
            if ((ret = database_management.update(1, columns_name, columns_value, &insert_len)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_update failed!", ret);
                continue;
            }
            #elif BOARDTYPE == 5350
            if ((ret = nvram_interface.insert(RT5350_FREE_SPACE, 1, columns_name, columns_value, &insert_len)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_insert failed!", ret);
                continue;
            }
            #endif
            break;
        }
        if (ret < 0)
        {
            res = ret;
            PRINT("insert failed!\n");
        }
    }
    PRINT("before close! fd = %d terminal_dev_register->fd = %d\n", fd, terminal_dev_register->fd);
    #if BOARDTYPE == 5350
    // 网口时关闭
    if (terminal_dev_register->communication_mode == 1)
    {
        close(fd);
    }
    terminal_dev_register->communication_mode = 0;
    
    #elif BOARDTYPE == 9344
    
    #if USB_INTERFACE == 1
    PRINT("before close! fd = %d terminal_dev_register->fd = %d\n", fd, terminal_dev_register->fd);
    shutdown(fd, SHUT_RDWR);
    PERROR("after shutdown\n");
    close(fd);
    PERROR("after close\n");
    #elif USB_INTERFACE == 2
    
    // 网口时关闭
    if (terminal_dev_register->communication_mode == 1)
    {
        close(fd);
    }
    terminal_dev_register->communication_mode = 0;
    
    #endif // USB_INTERFACE == 2
    
    #endif // BOARDTYPE == 9344
    *network_config.network_flag = 0;
    network_config.init_cmd_list();  // 初始化命令结构体
    request_cmd.init_data_table(&terminal_dev_register->data_table);
    terminal_dev_register->network_config_fd = 0;
    terminal_dev_register->config_now_flag = 0;
    return (void *)res;
}

/**
 * 请求命令字分析 网络设置 和 注册
 */
int request_cmd_0x01_02_03_07_08_09_0A(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
	int i = 0;
    int fd = terminal_dev_register->fd;
    unsigned char cmd = (unsigned char)terminal_dev_register->cmd_word;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
    
    PRINT("cmd = %02X, (unsigned char)terminal_dev_register->cmd_word = %02X\n", cmd, (unsigned char)terminal_dev_register->cmd_word);
    if ((cmd == 0x07) || (cmd == 0x08) || (cmd == 0x09) || (cmd == 0x0A)) // 无线网络设置和网络设置
    {
        if (register_state != SUCCESS_STATUS)
        {
            res = NO_INIT_ERR;
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
            return res;
        }
        *network_config.network_flag = 1;
        PRINT("*network_config.network_flag = %d\n", *network_config.network_flag);
    }
    
	for (i = 0; i < 3; i++)
	{
		PRINT("config_now_flag = %d\n", terminal_dev_register->config_now_flag);
		if (terminal_dev_register->config_now_flag == 1)
		{
			sleep(1);
			continue;
		}
		break;
	}
	if (i == 3)
	{
		PRINT("config now!");
		return CONFIG_NOW;
	}

    if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, NULL, 0)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
        return res;
    }
    
    // terminal_dev_register->config_now_flag == 0:说明此时没有在设置
	PRINT("config_now_flag = %d\n", terminal_dev_register->config_now_flag);
    if (terminal_dev_register->config_now_flag == 0)
    {
        if (pthread_create(&terminal_dev_register->request_cmd_0x01_02_03_07_08_09_0A_id, NULL, (void*)request_cmd_0x01_02_03_07_08_09_0A_pthread, (void *)terminal_dev_register) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "pthread_create failed!", PTHREAD_CREAT_ERR);
            return PTHREAD_CREAT_ERR;
        }
    }
    
    // 这将该子线程的状态设置为detached,则该线程运行结束后会自动释放所有资源（非阻塞，可立即返回）
    if (pthread_detach(terminal_dev_register->request_cmd_0x01_02_03_07_08_09_0A_id) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "pthread_detach failed!", PTHREAD_CREAT_ERR);
        
        #if BOARDTYPE == 5350
        pthread_mutex_lock(&nvram_interface.mutex);
        #endif
        
        pthread_cancel(terminal_dev_register->request_cmd_0x01_02_03_07_08_09_0A_id);
        PRINT("after pthread_cancel!\n");
        
        // 是否需要pthread_join
        pthread_join(terminal_dev_register->request_cmd_0x01_02_03_07_08_09_0A_id, NULL);
        
        #if BOARDTYPE == 5350
        pthread_mutex_unlock(&nvram_interface.mutex);
        #endif
        
        return PTHREAD_CREAT_ERR;
    }
    // 说明此时正在配置
    terminal_dev_register->config_now_flag = 1;
    terminal_dev_register->network_config_fd = fd;
    return res;
}

/**
 * 请求命令字分析 0x04 询问
 */
int request_cmd_0x04(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0, ret = 0;
    int fd = terminal_dev_register->fd;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;

    struct s_pad_and_6410_msg pad_and_6410_msg;
    memset(&pad_and_6410_msg, 0, sizeof(struct s_pad_and_6410_msg));
    pad_and_6410_msg.head[0] = 0x5A;
    pad_and_6410_msg.head[1] = 0xA5;

    int i = 0;
    char *buf = NULL;
    unsigned short len = 0;
    unsigned short buf_len = 0;
    
    char buf_tmp[256] = {0}; 
    char pad_mac[21] = {0};
    
    char columns_name[1][30] = {0};
    char columns_value[1][100] = {0};
    unsigned short insert_len = 0;
    unsigned char insert_flag = 0;
    
    // 判断wan口状态
    if ((res = network_config.get_wan_state()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_wan_state failed!", res);
        return res;
    }
            
    // 说明此时正在设置
    if (terminal_dev_register->config_now_flag == 1)
    {
        register_state = CONFIGURING_STATUS;
    }

    switch (register_state)
    {
        case CONFIGURING_STATUS: // 说明此时正在设置
        {
            pthread_mutex_lock(&network_config.recv_mutex);
            break;
        }
        case INITUAL_STATUS:
        {
             // 此时需查看pad_mac是否为空
            PRINT("terminal_dev_register->data_table.pad_mac = %s\n", terminal_dev_register->data_table.pad_mac);
            if (strlen(terminal_dev_register->data_table.pad_mac) == 12)
            {
                // 获取10.10.10.100的mac
                strcpy(buf_tmp, "arping -c 1 -w 1 -I br0 10.10.10.100 | grep '\\[' | awk '{print $5}'");
                if ((res = common_tools.get_cmd_out(buf_tmp, pad_mac, sizeof(pad_mac))) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_cmd_out failed!", res);
                    break;
                }
                memset(buf_tmp, 0, sizeof(buf_tmp));

                if (strlen(pad_mac) > 0)
                {
                    pad_mac[strlen(pad_mac) - 2] = '\0';
                    memcpy(buf_tmp, pad_mac + 1, strlen(pad_mac) - 1);
                    memset(pad_mac, 0, sizeof(pad_mac));
                    memcpy(pad_mac, buf_tmp, strlen(buf_tmp));
                    memset(buf_tmp, 0, sizeof(buf_tmp));
                    
                    PRINT("pad_mac = %s\n", pad_mac);
                    // 转换成正常格式MAC地址 00:AA:11:BB:CC:DD
                    if ((res = common_tools.mac_format_conversion(pad_mac)) < 0)
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "mac_format_conversion failed", res);
                        break;
                    }
                    common_tools.mac_del_colon(pad_mac);
                    PRINT("pad_mac = %s, terminal_dev_register->data_table.pad_mac = %s\n", pad_mac, terminal_dev_register->data_table.pad_mac);
                    if (memcmp(terminal_dev_register->data_table.pad_mac, pad_mac, strlen(terminal_dev_register->data_table.pad_mac)) == 0)
                    {
                        *network_config.pad_cmd = LAN_OK_STATUS;
                        *network_config.cmd = LAN_OK_STATUS;
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "Local area network normal!", 0);
                    }
                }
            }
            if ((unsigned char)*network_config.pad_cmd != LAN_OK_STATUS)
            {
                break;
            }
        }
        case LAN_OK_STATUS:
        {
            #if CHECK_WAN_STATE == 1
            
            #if 1
            //res = common_tools.get_network_state(common_tools.config->terminal_server_ip, 1, 1);
            //res = common_tools.get_network_state(common_tools.config->center_ip, 1, 1);
            res = common_tools.get_network_state(common_tools.config->center_ip, 1, 2);
            #else
            res = common_tools.get_network_state(common_tools.config->wan_check_name, 1, 1);
            #endif
            if (res == DATA_DIFF_ERR)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "network abnormal!", res);
                res = 0;
            }
            else if (res < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_network_state failed!", res);
            }
            else
            {
                *network_config.pad_cmd = WAN_OK_STATUS;
                *network_config.cmd = WAN_OK_STATUS;
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "Wide network normal!", 0);
            }

            #else // 不检测WAN口，直接置为WAN_OK_STATUS，代表注册没有进行
            *network_config.pad_cmd = WAN_OK_STATUS;
            *network_config.cmd = WAN_OK_STATUS;
            #endif // #if CHECK_WAN_STATE == 1

            if ((unsigned char)*network_config.pad_cmd != WAN_OK_STATUS)
            {
                break;
            }
        }
        case WAN_OK_STATUS:
        {
            #if USER_REGISTER == 1 // 1：做终端认证流程
            
            if ((res = terminal_register.user_register(terminal_dev_register->data_table.pad_sn, terminal_dev_register->data_table.pad_mac)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "user_register failed!", res);
                break;
            }
            #endif // #if USER_REGISTER == 1
            
            *network_config.pad_cmd = SUCCESS_STATUS;
            *network_config.cmd = SUCCESS_STATUS;
        }
        case SUCCESS_STATUS: // 当已经初始化成功时，PAD询问，BASE把SSID等信息发送到PAD
        {
            #if BOARDTYPE == 9344
            // 打包
            res = get_success_buf(&buf);
            PRINT("buf = %s\n", buf);
            if (res < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_success_buf failed", res);
            }
            else 
            {
                len = strlen(buf);
            }
            
            #endif
            
            break;
        }
        default:
        {
            res = DATA_ERR;
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "Data does not match", res);
            break;
        }
    }

    if (register_state == CONFIGURING_STATUS)
    {
        pthread_mutex_unlock(&network_config.recv_mutex);
    }

    if (res < 0)
    {
        goto EXIT;
    }
    
    
    for (i = 0; i < common_tools.config->repeat; i++)
    {
        if (pad_and_6410_msg.data != NULL)
        {
            free(pad_and_6410_msg.data);
            pad_and_6410_msg.data = NULL;
        }
        
        if (register_state == CONFIGURING_STATUS)
        {
            res = network_config.send_msg_to_pad(fd, register_state, NULL, len);
        }
        else
        {
            res = network_config.send_msg_to_pad(fd, *network_config.pad_cmd, buf, len);
        }
        
        // 数据发送到PAD
        if (res < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
            continue;
        }
        if ((res = network_config.recv_msg_from_pad(fd, &pad_and_6410_msg)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg_from_pad failed", res);
            continue;
        }
        PRINT("pad_and_6410_msg.cmd = %d\n", pad_and_6410_msg.cmd);
        if ((pad_and_6410_msg.data != NULL) || (pad_and_6410_msg.cmd != SUCCESS_STATUS))
        {
            res = WRITE_ERR;
            continue;
        }
        break;
    }

EXIT:
    if (pad_and_6410_msg.data != NULL)
    {
        free(pad_and_6410_msg.data);
        pad_and_6410_msg.data = NULL;
    }

    if (buf != NULL)
    {
        free(buf);
        buf = NULL;
    }

    if ((register_state != *network_config.pad_cmd) && (register_state != CONFIGURING_STATUS) && ((unsigned char)*network_config.cmd != 0xFF)) // 插入数据库
    {
        memset(columns_name[0], 0, sizeof(columns_name[0]));
        memset(columns_value[0], 0, sizeof(columns_value[0]));
        memcpy(columns_name[0], "register_state", strlen("register_state"));

        sprintf(columns_value[0], "%d", (unsigned char)*network_config.pad_cmd);
        insert_len = strlen(columns_value[0]);
        PRINT("columns_value[0] = %s\n", columns_value[0]);

        for (i = 0; i < common_tools.config->repeat; i++)
        {
            #if BOARDTYPE == 6410 || BOARDTYPE == 9344
            if ((ret = database_management.update(1, columns_name, columns_value, &insert_len)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_update failed!", ret);
                continue;
            }
            #elif BOARDTYPE == 5350
            if ((ret = nvram_interface.update(RT5350_FREE_SPACE, 1, columns_name, columns_value, &insert_len)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_update failed!", ret);
                continue;
            }
            #endif
            break;
        }
        if (ret < 0)
        {
            res = ret;
            PRINT("insert failed!\n");
        }
        terminal_dev_register->data_table.register_state = *network_config.pad_cmd;
        
        #if USER_REGISTER == 1
        
        if (*network_config.pad_cmd == 0)
        {
            #if 1 // 启动CACM
            if (terminal_authentication.start_up_CACM() < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "start_up_CACM failed!", -1);
            }
            else
            {
                PRINT("start_up_CACM success!\n");
            }
            #endif
        }
        
        #endif // #if USER_REGISTER == 1
    }
    return res;
}

/**
 * 请求命令字分析 0x05: 5350 生成默认SSID; 9344 注册命令，PAD将随机生成的4字节串码发给base
 */
int request_cmd_0x05(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    int len = terminal_dev_register->length;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
#if BOARDTYPE == 9344
	   struct s_dev_register dev_register;
	   memset(&dev_register, 0, sizeof(struct s_dev_register));
#endif
    int i = 0;
    int index = 0;
    if (register_state != SUCCESS_STATUS) // PAD没有注册成功
    {
        res = NO_INIT_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
        return res;
    }

    #if BOARDTYPE == 5350
    // 生成默认SSID
    if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, NULL, 0)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
        return res;
    }

    for (i = 0; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
    {
        // 初始化设备项
        switch (network_config.cmd_list[i].cmd_word)
        {
            case 0x46: // SSID3
            {
                index++;
                network_config.cmd_list[i].cmd_bit = index;
                sprintf(network_config.cmd_list[i].set_value, "%s", terminal_dev_register->data_table.default_ssid);
                sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                break;
            }
            case 0x47: // SSID3 密码
            {
                index++;
                network_config.cmd_list[i].cmd_bit = index;
                sprintf(network_config.cmd_list[i].set_value, "%s", terminal_dev_register->data_table.default_ssid_password);
                sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                break;
            }
            case 0x45:
            {
                index++;
                network_config.cmd_list[i].cmd_bit = index;
                memcpy(network_config.cmd_list[i].set_value, "WPAPSKWPA2PSK;WPAPSKWPA2PSK;WPAPSKWPA2PSK", strlen("WPAPSKWPA2PSK;WPAPSKWPA2PSK;WPAPSKWPA2PSK"));
                sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                break;
            }
            case 0x34:
            {
                index++;
                network_config.cmd_list[i].cmd_bit = index;
                memcpy(network_config.cmd_list[i].set_value, "1;0;0", strlen("1;0;0"));
                sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                break;
            }
            case 0x35:
            {
                index++;
                network_config.cmd_list[i].cmd_bit = index;
                memcpy(network_config.cmd_list[i].set_value, "AES;AES;AES", strlen("AES;AES;AES"));
                sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                break;
            }
            case 0x36:
            {
                index++;
                network_config.cmd_list[i].cmd_bit = index;
                memcpy(network_config.cmd_list[i].set_value, "3600;3600;3600", strlen("3600;3600;3600"));
                sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                break;
            }
            case 0x37:
            {
                index++;
                network_config.cmd_list[i].cmd_bit = index;
                memcpy(network_config.cmd_list[i].set_value, "3", strlen("3"));
                sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                break;
            }
            default:
            {
                break;
            }
        }
    }

    // 设置路由
    if ((res = network_config.route_config2(index)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "route_config failed!", res);
        return res;
    }

    sleep(1);
    system("reboot");

    #elif BOARDTYPE == 9344
    
    if (len != (SN_LEN + 12 + 4)) // 序列号 mac 串码
    {
        res = P_LENGTH_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "length error!", res);
        return res;
    }
    // 注册命令，PAD将随机生成的4字节串码发给base
    memset(&dev_register, 0, sizeof(struct s_dev_register));
    memcpy(&(dev_register.dev_mac), terminal_dev_register->data + SN_LEN, 12);
    memcpy(&(dev_register.dev_code), terminal_dev_register->data + SN_LEN + 12, 4);

    if ((res = update_dev_info_code(dev_count, dev_info, &dev_register)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "update_dev_info_code failed!", res);
        return res;
    }

    if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, NULL, 0)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
        return res;
    }

    PRINT("after send_msg_to_pad\n");
    #endif
    return res;
}

/**
 * 请求命令字分析 0x06 
 * 5350时 注销默认SSID；
 * 9344时 注册命令，智能设备将 设备名称、id、mac发送给BASE
 */
int request_cmd_0x06(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    int len = terminal_dev_register->length;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
#if BOARDTYPE == 9344
	struct s_dev_register dev_register;
	memset(&dev_register, 0, sizeof(struct s_dev_register));
#endif
    
    int i = 0;
    int index = 0;
    unsigned short length = 0;
    if (register_state != SUCCESS_STATUS) // PAD没有注册成功
    {
        res = NO_INIT_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
        return res;
    }

#if BOARDTYPE == 5350
    // 注销默认SSID
    if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, NULL, 0)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
        return res;
    }

    for (i = 0; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
    {
        // 初始化设备项
        switch (network_config.cmd_list[i].cmd_word)
        {
            case 0x46: // SSID3
            {
                index++;
                network_config.cmd_list[i].cmd_bit = index;
                strcpy(network_config.cmd_list[i].set_value, "\"\"");
                sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                break;
            }
            case 0x47: // SSID3 密码
            {
                index++;
                network_config.cmd_list[i].cmd_bit = index;
                strcpy(network_config.cmd_list[i].set_value, "\"\"");
                sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                break;
            }
            case 0x45:
            {
                index++;
                network_config.cmd_list[i].cmd_bit = index;
                memcpy(network_config.cmd_list[i].set_value, "WPAPSKWPA2PSK;WPAPSKWPA2PSK", strlen("WPAPSKWPA2PSK;WPAPSKWPA2PSK"));
                sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                break;
            }
            case 0x34:
            {
                index++;
                network_config.cmd_list[i].cmd_bit = index;
                memcpy(network_config.cmd_list[i].set_value, "1;0", strlen("1;0"));
                sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                break;
            }
            case 0x35:
            {
                index++;
                network_config.cmd_list[i].cmd_bit = index;
                memcpy(network_config.cmd_list[i].set_value, "AES;AES", strlen("AES;AES"));
                sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                break;
            }
            case 0x36:
            {
                index++;
                network_config.cmd_list[i].cmd_bit = index;
                memcpy(network_config.cmd_list[i].set_value, "3600;3600", strlen("3600;3600"));
                sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                break;
            }
            case 0x37:
            {
                index++;
                network_config.cmd_list[i].cmd_bit = index;
                memcpy(network_config.cmd_list[i].set_value, "2", strlen("2"));
                sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                break;
            }
            default:
            {
                break;
            }
        }
    }

    if ((res = network_config.route_config2(index)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "route_config failed!", res);
        return res;
    }
    sleep(1);
	system("reboot");
    
#elif BOARDTYPE == 9344
    
    // 注册命令，智能设备将 设备名称、id、mac发送给BASE
    memset(&dev_register, 0, sizeof(struct s_dev_register));
    length = terminal_dev_register->data[index++];
    length += terminal_dev_register->data[index++] << 8;
    PRINT("length = %d %02X\n", length, length);
    if (length > len)
    {
        res = P_DATA_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
        return res;
    }
    memcpy(dev_register.dev_name, terminal_dev_register->data + index, length);
    index += length;
    
    length = terminal_dev_register->data[index++];
    length += terminal_dev_register->data[index++] << 8;
    PRINT("length = %d %02X\n", length, length);
    if (length != 8)
    {
        res = P_DATA_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
        return res;
    }
    memcpy(dev_register.dev_id, terminal_dev_register->data + index, length);
    index += length;

    length = terminal_dev_register->data[index++];
    length += terminal_dev_register->data[index++] << 8;
    PRINT("length = %d %02X\n", length, length);
    if (length != 12)
    {
        res = P_DATA_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
        return res;
    }
    memcpy(dev_register.dev_mac, terminal_dev_register->data + index, length);
    
    PRINT("dev_register.dev_name = %s\n", dev_register.dev_name);
    PRINT("dev_register.dev_id = %s\n", dev_register.dev_id);
    PRINT("dev_register.dev_mac = %s\n", dev_register.dev_mac);
    
    if ((res = add_dev_info(&dev_count, &dev_info, &dev_register)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "add_dev_info failed!", res);
        return res;
    }
    
    PRINT("dev_count = %d\n", dev_count);
    print_dev_table(dev_count, dev_info);
    PRINT("after print_dev_table dev_info = %p\n", dev_info);
    
    if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, NULL, 0)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
        return res;
    }
    PRINT("exit 0x06\n");
#endif // BOARDTYPE == 9344
    return res;
}

/**
 * 请求命令字分析 0x0B 
 * 5350时 网络设置询问
 * 9344时 设备名称修改命令，智能设备将 mac、设备名称发送给BASE
 */
int request_cmd_0x0B(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    int len = terminal_dev_register->length;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
    
    char columns_name[1][30] = {0};
    char columns_value[1][100] = {0};
    
    int i = 0, j = 0;
    int index = 0;
    char buf[256] = {0};
    char send_buf[1024] = {0};
    char config_cmd[128] = {0};
    if (register_state != SUCCESS_STATUS) // PAD没有注册成功
    {
        res = NO_INIT_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
        return res;
    }

    
    #if BOARDTYPE == 5350
    memcpy(columns_name[0], "wanConnectionMode", strlen("wanConnectionMode"));
    if ((res = nvram_interface.select(NVRAM_RT5350, 1, columns_name, columns_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);
        return res;
    }
    PRINT("columns_value[0] = %s\n", columns_value[0]);
    if ((strcmp("\"\"", columns_value[0]) == 0) || (strlen(columns_value[0]) == 0))
    {
        memset(columns_value[0], 0, sizeof(columns_value[0]));
        sprintf(columns_value[0], "There is no (%s) record!", columns_name[0]);
        res = NULL_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[0], res);
        return res;
    }
    memcpy(buf, columns_value[0], strlen(columns_value[0]));
    #elif BOARDTYPE == 9344
    
    #if 0
    char mode[32] = {0};
    if ((res = common_tools.get_cmd_out("cfg -s | grep WAN_MODE:=", mode, sizeof(mode))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_cmd_out failed!", res);
        return res;
    }
    memcpy(buf, mode + strlen("WAN_MODE:="), strlen(mode) - strlen("WAN_MODE:=") - 1);
    #endif // #if 0
    
    
    struct s_dev_register dev_register;
	memset(&dev_register, 0, sizeof(struct s_dev_register));
	
    #endif


    PRINT("connect mode = %s\n", buf);
    memset(send_buf, 0, sizeof(send_buf));
    #if BOARDTYPE == 5350
    if (memcmp(buf, "STATIC", strlen("STATIC")) == 0)
    {
        strcpy(send_buf, "mode:STATIC,");
        for (i = 0; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
        {
            //PRINT("network_config.cmd_list[i].cmd_word = %02X, index = %d, i = %d\n", network_config.cmd_list[i].cmd_word, index, i);
            switch (network_config.cmd_list[i].cmd_word)
            {
                case 0x14:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x15:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x16:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x17:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x18:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x1D:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x30:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x31:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x45:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
            }
            if (index == 9)
            {
                break;
            }
        }
    }
    else if (memcmp(buf, "DHCP", strlen("DHCP")) == 0)
    {
        strcpy(send_buf, "mode:DHCP,");
        for (i = 0; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
        {
            switch (network_config.cmd_list[i].cmd_word)
            {
                case 0x19:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x1D:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x30:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x31:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x45:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
            }
            if (index == 5)
            {
                break;
            }
        }
    }
    else if (memcmp(buf, "PPPOE", strlen("PPPOE")) == 0)
    {
        strcpy(send_buf, "mode:PPPOE,");
        for (i = 0; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
        {
            switch (network_config.cmd_list[i].cmd_word)
            {
                case 0x1A:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x1B:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x30:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x31:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x45:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
            }
            if (index == 5)
            {
                break;
            }
        }
    }
    #elif BOARDTYPE == 9344
    
    #if 0
    if (memcmp(buf, "static", strlen("static")) == 0)
    {
        strcpy(send_buf, "mode:STATIC,");
        for (i = 0; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
        {
            //PRINT("network_config.cmd_list[i].cmd_word = %02X, index = %d, i = %d\n", network_config.cmd_list[i].cmd_word, index, i);
            switch (network_config.cmd_list[i].cmd_word)
            {
                case 0x15:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x16:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x17:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x18:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x1E: // SSID2
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x31: // 密码
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x3D: // 加密
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x40: // 加密
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
            }
            if (index == 8)
            {
                break;
            }
        }
    }
    else if (memcmp(buf, "dhcp", strlen("dhcp")) == 0)
    {
        strcpy(send_buf, "mode:DHCP,");
        for (i = 0; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
        {
            switch (network_config.cmd_list[i].cmd_word)
            {
                case 0x1E: // SSID2
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x31: // 密码
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x3D: // 加密
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x40: // 加密
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
            }
            if (index == 4)
            {
                break;
            }
        }
    }
    else if (memcmp(buf, "pppoe", strlen("pppoe")) == 0)
    {
        strcpy(send_buf, "mode:PPPOE,");
        for (i = 0; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
        {
            switch (network_config.cmd_list[i].cmd_word)
            {
                case 0x1A:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x1B:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x1E: // SSID2
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x31: // 密码
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x3D: // 加密
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
                case 0x40: // 加密
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    break;
                }
            }
            if (index == 6)
            {
                break;
            }
        }
    }
    #endif // #if 0
    
    // 9344时 设备名称修改命令，智能设备将 mac、设备名称发送给BASE
    memcpy(dev_register.dev_mac, terminal_dev_register->data, 12);
    
    if ((len - 12) > (sizeof(dev_register.dev_name) - 1))
    {
        res = P_DATA_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
        return res;
    }
    memcpy(dev_register.dev_name, terminal_dev_register->data + 12, len - 12);
    
    PRINT("dev_register.dev_name = %s\n", dev_register.dev_name);
    PRINT("dev_register.dev_mac = %s\n", dev_register.dev_mac);
    
    if ((res = database_update_dev_info(&dev_register)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "add_dev_info failed!", res);
        return res;
    }
    
    if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, NULL, 0)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
        return res;
    }
    
    #endif // BOARDTYPE == 9344
    #if 0
    else
    {
        res = DATA_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data err", res);
        return res;
    }
    
    char tmp[64] = {0};
    memset(buf, 0, sizeof(buf));
    for (i = 0, j = 1; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
    {
        if (j == network_config.cmd_list[i].cmd_bit)
        {
            PRINT("network_config.cmd_list[%d].cmd_bit = %02X, index = %d, j = %d\n", i, network_config.cmd_list[i].cmd_bit, index, j);
            memset(buf, 0, sizeof(buf));
            memset(tmp, 0, sizeof(tmp));
            
            #if BOARDTYPE == 5350
            memset(buf, 0, sizeof(buf));
            if ((res = nvram_interface.select(NVRAM_RT5350, 1, (char (*)[30])network_config.cmd_list[i].cmd_key, (char (*)[100])buf)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);
                break;
            }
            if (strlen(buf) == 0)
            {
                strcpy(buf, "\"\"");
            }
            PRINT("buf = %s\n", buf);
            
            #elif BOARDTYPE == 9344
            snprintf(config_cmd, sizeof(config_cmd), "cfg -s | grep %s:=", network_config.cmd_list[i].cmd_key);
            if ((res = common_tools.get_cmd_out(config_cmd, tmp, sizeof(tmp))) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_cmd_out failed!\n", res);
                break;
            }
            memcpy(buf, tmp + 2 + strlen(network_config.cmd_list[i].cmd_key), strlen(tmp) - 2 - strlen(network_config.cmd_list[i].cmd_key) - 1);
            #endif //BOARDTYPE == 9344
            
            PRINT("network_config.cmd_list[%d].cmd_key = %s, buf = %s\n", i, network_config.cmd_list[i].cmd_key, buf);
            switch (network_config.cmd_list[i].cmd_word)
            {
                case 0x15:
                {
                    strcat(send_buf, "ip:");
                    strncat(send_buf, buf, strlen(buf));
                    strcat(send_buf, ",");
                    j++;
                    break;
                }
                case 0x16:
                {
                    strcat(send_buf, "mask:");
                    strncat(send_buf, buf, strlen(buf));
                    strcat(send_buf, ",");
                    j++;
                    break;
                }
                case 0x17:
                {
                    strcat(send_buf, "gateway:");
                    strncat(send_buf, buf, strlen(buf));
                    strcat(send_buf, ",");
                    j++;
                    break;
                }
                case 0x18:
                {
                    strcat(send_buf, "dns1:");
                    strncat(send_buf, buf, strlen(buf));
                    strcat(send_buf, ",");
                    j++;
                    break;
                }
                case 0x1A:
                {
                    strcat(send_buf, "pppoe_user:");
                    strncat(send_buf, buf, strlen(buf));
                    strcat(send_buf, ",");
                    j++;
                    break;
                }
                case 0x1B:
                {
                    strcat(send_buf, "pppoe_pass:");
                    strncat(send_buf, buf, strlen(buf));
                    strcat(send_buf, ",");
                    j++;
                    break;
                }
                case 0x1E:
                {
                    strcat(send_buf, "ssid2:");
                    strncat(send_buf, buf, strlen(buf));
                    strcat(send_buf, ",");
                    j++;
                    break;
                }
                case 0x31:
                {
                    strcat(send_buf, "pass:");
                    strncat(send_buf, buf, strlen(buf));
                    strcat(send_buf, ",");
                    j++;
                    break;
                }
                case 0x3D:
                {
                    strcat(send_buf, "auth:");
                    strncat(send_buf, buf, strlen(buf));
                    j++;
                    break;
                }
                case 0x40:
                {
                    strncat(send_buf, buf, strlen(buf));
                    strcat(send_buf, ",");
                    j++;
                    break;
                }
            }
        }

        if (j > index)
        {
            break;
        }
        else if (i == (sizeof(network_config.cmd_list) / sizeof(struct s_cmd) - 1))
        {
            i = 0;
        }
    }
    PRINT("send_buf = %s\n", send_buf);
    if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, send_buf, strlen(send_buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
        return res;
    }
    #endif 
    return res;
}

/**
 * 请求命令字分析 0x0C 恢复出厂
 */
int request_cmd_0x0C(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;

    if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, NULL, 0)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
        return res;
    }
    sleep(1);

    #if BOARDTYPE == 5350
    system("kill -9 `ps | grep cacm | sed '/grep/'d | awk '{print $1}'`");
    system("nvram_set freespace register_state 251");
    #if SMART_RECOVERY == 1// 路由智能恢复
    system("nvram_set backupspace register_state 251");
    #endif
    system("ralink_init clear 2860");
    system("reboot");
    #elif BOARDTYPE == 9344
    // 1.情况数据库
    if ((res = database_management.clear()) < 0)
    {
        PRINT("sqlite3_clear_table failed!\n");
        return res;
    }
    // 2.取消绑定
    //cancel_mac_and_ip_bind();
    // 3.
    //system("cfg -x");
    system("cfg -b 5");
    //sleep(2);
    
    system("kill -9 `ps | grep monitor_application | sed '/grep/'d | awk '{print $1}'`");
    system("ps");
    PRINT("after kill monitor_application\n");
    
    PRINT("before exec reboot\n");
    system("reboot");
    PRINT("after exec reboot\n");
    
    // 
    exit(0);
    #endif
    
    return res;
}

/**
 * 请求命令字分析 0x0D 查看Base版本信息
 */
int request_cmd_0x0D(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    
    char buf[256] = {0};
    if ((res = common_tools.get_cmd_out("cat /proc/version", buf, sizeof(buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_cmd_out failed!", res);
        return res;
    }

    if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, buf, strlen(buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
        return res;
    }
    return res;
}

#if CTSI_SECURITY_SCHEME == 2
/**
 * 请求命令字分析 0x0E 获取身份令牌
 */
int request_cmd_0x0E(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
    char device_token[TOKENLEN] = {0};
    
    if (register_state != SUCCESS_STATUS) // PAD没有注册成功
    {
        res = NO_INIT_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
        return res;
    }

    if ((res = terminal_authentication.return_device_token(device_token)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "return_device_token failed!", res);
        return res;
    }

    if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, device_token, TOKENLEN)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
        return res;
    }
    return res;
}
/**
 * 请求命令字分析 0x0F 获取位置令牌
 */
int request_cmd_0x0F(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
    char position_token[16] = {0};
    
    if (register_state != SUCCESS_STATUS) // PAD没有注册成功
    {
        res = NO_INIT_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
        return res;
    }

    if ((res = terminal_authentication.return_position_token(position_token)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "return_position_token failed!", res);
        return res;
    }

    if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, position_token, 16)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
        return res;
    }
    return res;
}
/**
 * 请求命令字分析 0x50 重新生成身份令牌
 */
int request_cmd_0x50(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
    char device_token[16] = {0};
    
    if (register_state != SUCCESS_STATUS) // PAD没有注册成功
    {
        res = NO_INIT_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
        return res;
    }

    if ((res = terminal_authentication.rebuild_device_token(device_token)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "rebuild_device_token failed!", res);
        return res;
    }

    if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, device_token, 16)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
        return res;
    }
    return res;
}
/**
 * 请求命令字分析 0x51 重新生成位置令牌
 */
int request_cmd_0x51(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
    char position_token[16] = {0};
    
    if (register_state != SUCCESS_STATUS) // PAD没有注册成功
    {
        res = NO_INIT_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
        return res;
    }

    if ((res = terminal_authentication.rebuild_position_token(position_token)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "rebuild_position_token failed!", res);
        return res;
    }

    if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, position_token, 16)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
        return res;
    }
    return res;
}
#endif // CTSI_SECURITY_SCHEME == 2

/**
 * 请求命令字分析 0x52 查看WAN口
 */
int request_cmd_0x52(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
    
    if ((res = network_config.get_wan_state()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_wan_state failed!", res);
        return res;
    }
}

/**
 * 请求命令字分析 0x53 取消当前操作
 */
int request_cmd_0x53(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->network_config_fd;
    
    if (terminal_dev_register->config_now_flag == 1)
    {
        #if BOARDTYPE == 5350
        pthread_mutex_lock(&nvram_interface.mutex);
        #endif
        
        pthread_cancel(terminal_dev_register->request_cmd_0x01_02_03_07_08_09_0A_id);
        PRINT("after pthread_cancel!\n");
        
        // 是否需要pthread_join
        pthread_join(terminal_dev_register->request_cmd_0x01_02_03_07_08_09_0A_id, NULL);
        
        #if BOARDTYPE == 5350
        pthread_mutex_unlock(&nvram_interface.mutex);
        #endif
        
        // 判断锁状态(需要跟踪)
        if(pthread_mutex_trylock(&network_config.recv_mutex) == EBUSY)
        {
            PRINT("pad_cmd_handle pthread lock now!\n");
        }
        else
        {
            PRINT("lock now!\n");
            pthread_mutex_unlock(&network_config.recv_mutex);
        }
        
        // 如果是通过网络接口注册，则需要关闭
        if (fd != terminal_dev_register->non_network_fd)
        {
            close(fd);
        }
    }
    
    if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, NULL, 0)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
        return res;;
    }
    
    terminal_dev_register->network_config_fd = 0;
    terminal_dev_register->config_now_flag = 0;
    request_cmd.init_data_table(&terminal_dev_register->data_table);
    return res;
}

#if BOARDTYPE == 9344
/**
 * 请求命令字分析 0x54 串码对比，智能设备将输入的串码发送到base，base进行对比
 */
int request_cmd_0x54(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
    
    int index = 0;
    unsigned short length = 0;
    
#if BOARDTYPE == 9344
    struct s_dev_register dev_register;
    memset(&dev_register, 0, sizeof(struct s_dev_register));
#endif

    if (register_state != SUCCESS_STATUS) // PAD没有注册成功
    {
        res = NO_INIT_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
        return res;
    }
    
    memset(&dev_register, 0, sizeof(struct s_dev_register));
    length = terminal_dev_register->data[index++];
    length += terminal_dev_register->data[index++] << 8;
    PRINT("length = %d %02X\n", length, length);
    if (length != 12)
    {
        res = P_DATA_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
        return res;
    }
    memcpy(dev_register.dev_mac, terminal_dev_register->data + index, length);
    index += length;
    
    length = terminal_dev_register->data[index++];
    length += terminal_dev_register->data[index++] << 8;
    PRINT("length = %d %02X\n", length, length);
    if (length != 8)
    {
        res = P_DATA_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
        return res;
    }
    memcpy(dev_register.dev_id, terminal_dev_register->data + index, length);
    index += length;

    length = terminal_dev_register->data[index++];
    length += terminal_dev_register->data[index++] << 8;
    PRINT("length = %d %02X\n", length, length);
    if (length != 4)
    {
        res = P_DATA_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
        return res;
    }
    memcpy(dev_register.dev_code, terminal_dev_register->data + index, length);
    
    PRINT("dev_register.dev_mac = %s\n", dev_register.dev_mac);
    PRINT("dev_register.dev_id = %s\n", dev_register.dev_id);
    PRINT("dev_register.dev_code = %s\n", dev_register.dev_code);
    
    if ((res = dev_code_comparison(dev_count, dev_info, &dev_register)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "dev_code_comparison failed!", res);
        return res;
    }

    if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, NULL, 0)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
        return res;
    }
    
    // 从内存中删除
    if ((res = delete_dev_info(&dev_count, &dev_info, &dev_register)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "delete_dev_info failed", res);
        return res;
    }
    PRINT("after delete_dev_info \n");
    
    // 插入数据库
    if ((res = database_insert_dev_info(&dev_register)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "database_insert_dev_info failed", res);
        return res;
    }
    PRINT("after database_insert_dev_info \n");
    return res;
}
/**
 * 请求命令字分析 0x55 PAD扫描“发送注册申请”的设备
 */
int request_cmd_0x55(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    int length = terminal_dev_register->length;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
    
    unsigned short len = 0;
    char buf[1024] = {0};

    if (register_state != SUCCESS_STATUS) // PAD没有注册成功
    {
        res = NO_INIT_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
        return res;
    }
    
    if (length != SN_LEN) // 序列号
    {
        res = P_LENGTH_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "length error!", res);
        return res;
    }
    
    if ((res = select_dev_info(dev_count, dev_info, buf, sizeof(buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "select_dev_info failed!", res);
        return res;
    }
    len = strlen(buf);
    PRINT("buf = %s\n", buf);
    if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, buf, len)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
        return res;
    }
    return res;
}
/**
 * 请求命令字分析 0x56 查询已经注册的设备
 */
int request_cmd_0x56(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
    
    unsigned short len = 0;
    char buf[1024] = {0};

    if (register_state != SUCCESS_STATUS) // PAD没有注册成功
    {
        res = NO_INIT_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
        return res;
    }
     
    if ((res = database_select_dev_info(buf, sizeof(buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "database_select_dev_info failed!", res);
        return res;
    }
    len = strlen(buf);
    
    if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, buf, len)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
        return res;
    }
    return res;
}

/**
 * 请求命令字分析 0x57 注销命令，删除匹配序列号的行
 */
int request_cmd_0x57(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
    
    char buf[1024] = {0};
#if BOARDTYPE == 9344
    struct s_dev_register dev_register;
    memset(&dev_register, 0, sizeof(struct s_dev_register));
#endif
    
    if (register_state != SUCCESS_STATUS) // PAD没有注册成功
    {
        res = NO_INIT_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
        return res;
    }
    
    memcpy(dev_register.dev_mac, terminal_dev_register->data + SN_LEN, 12);
    if ((res = database_delete_dev_info(&dev_register)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "database_delete_dev_info failed!", res);
        return res;
    }

    if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, NULL, 0)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
        return res;
    }
    return res;
}

/**
 * 请求命令字分析 0x58 注销命令，删除匹配MAC的行
 */
int request_cmd_0x58(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    int len = terminal_dev_register->length;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
    
#if BOARDTYPE == 9344
    struct s_dev_register dev_register;
    memset(&dev_register, 0, sizeof(struct s_dev_register));
#endif
    
    if (register_state != SUCCESS_STATUS) // PAD没有注册成功
    {
        res = NO_INIT_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
        return res;
    }
    
    PRINT("len = %d %02X\n", len, len);
    if (len != 12)
    {
        res = P_DATA_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
        return res;
    }
    memcpy(dev_register.dev_mac, terminal_dev_register->data, len);
    
    if ((res = database_delete_dev_info(&dev_register)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "database_delete_dev_info failed!", res);
        return res;
    }

    if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, NULL, 0)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
        return res;
    }
    return res;
}

/**
 * 请求命令字分析 0x60 设备绑定 即生成隐藏SSID
 */
int request_cmd_0x60(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    int len = terminal_dev_register->length;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
    
#if BOARDTYPE == 9344
    struct s_dev_register dev_register;
    memset(&dev_register, 0, sizeof(struct s_dev_register));
#endif
    
    unsigned char redraw_flag = 0;
    char buf[256] = {0};
    char cmd_buf[256] = {0};
    
    char pad_sn[SN_LEN + 1] = {0};
    char pad_mac[18] = {0};
    
    char base_sn[SN_LEN + 1] = {0};
    char base_mac[18] = {0};
    char ssid1[32] = {0};
    char wpapsk1[32] = {0};
    
    PRINT("len = %d %02X\n", len, len);
    if (len != (SN_LEN + 12))
    {
        res = P_DATA_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
        return res;
    }
    
    memcpy(pad_sn, terminal_dev_register->data, SN_LEN);
    memcpy(pad_mac, terminal_dev_register->data + SN_LEN, 12);
    
    // 读取数据库，判断SSID是否存在
    char columns_name[9][30] = {"base_sn", "base_mac", "base_ip", "ssid_user_name", "ssid_password", "pad_sn", "pad_mac", "pad_ip", "register_state"};
    char columns_value[9][100] = {0};
    unsigned short values_len[9] = {0};
    
    res = database_management.select(5, columns_name, columns_value);
    
    if ((res < 0) && (res != NO_RECORD_ERR))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);
        return res;
    }
    else if (res == 0) /*SSID存在*/
    {
        // 直接返回
        redraw_flag = 0;
        common_tools.mac_add_colon(columns_value[1]);
        
        memcpy(base_sn, columns_value[0], strlen(columns_value[0]));
        memcpy(base_mac, columns_value[1], strlen(columns_value[1]));
        memcpy(ssid1, columns_value[3], strlen(columns_value[3]));
        memcpy(wpapsk1, columns_value[4], strlen(columns_value[4]));
    }
    else if (res == NO_RECORD_ERR) // 重新生成SSID
    {
        redraw_flag = 1;
        res = 0;
        
        // 获取base_sn mac
        if ((res = terminal_register.get_base_sn_and_mac(base_sn, base_mac)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_base_sn_and_mac failed", res);
            return res;
        }
        PRINT("get_base_sn_and_mac end!\n");
        
        if ((res = common_tools.get_rand_string(12, 0, ssid1, UTF8)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_rand_string failed", res);
            return res;
        }
        strncat(ssid1, base_mac, strlen(base_mac));
        PRINT("ssid1 = %s base_mac = %s\n", ssid1, base_mac);
        common_tools.mac_add_colon(base_mac);
        
        if ((res = common_tools.get_rand_string(12, 0, wpapsk1, UTF8)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_rand_string failed", res);
            return res;
        }
        PRINT("wpapsk1 = %s\n", wpapsk1);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "SSID and password randomly generated successfully!", 0);
        
        // 生成
        snprintf(cmd_buf, sizeof(cmd_buf), "cfg -b 3 %s_2G %s_5G %s %s &", ssid1, ssid1, wpapsk1, wpapsk1);
        system(cmd_buf);
    }
    
    snprintf(buf, sizeof(buf), "base_sn:%s,base_mac:%s,base_ip:10.10.10.254,ssid_user_name:%s,ssid_password:%s", base_sn, base_mac, ssid1, wpapsk1);
    common_tools.mac_del_colon(base_mac);
    
    int i = 0;
    struct s_pad_and_6410_msg pad_and_6410_msg;
    memset(&pad_and_6410_msg, 0, sizeof(struct s_pad_and_6410_msg));
    pad_and_6410_msg.head[0] = 0x5A;
    pad_and_6410_msg.head[1] = 0xA5;
    
    for (i = 0; i < 3; i++)
    {
        // 发送SSID
        if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, buf, strlen(buf))) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
            continue;
        }
        
        // 接收正确回复
        if ((res = network_config.recv_msg_from_pad(fd, &pad_and_6410_msg)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg_from_pad failed", res);
            continue;
        }
        
        if (pad_and_6410_msg.cmd == SUCCESS_STATUS)
        {
            // 修改终端初始化状态并插入数据库
            memcpy(columns_value[0], base_sn, strlen(base_sn));
            memcpy(columns_value[1], base_mac, strlen(base_mac));
            memcpy(columns_value[2], "10.10.10.254", strlen("10.10.10.254"));
            memcpy(columns_value[3], ssid1,strlen(ssid1));
            memcpy(columns_value[4], wpapsk1,strlen(wpapsk1));
            memcpy(columns_value[5], pad_sn, strlen(pad_sn));
            memcpy(columns_value[6], pad_mac, strlen(pad_mac));
            memcpy(columns_value[7], "10.10.10.100", strlen("10.10.10.100"));
            memcpy(columns_value[8], "0", strlen("0"));
            
            for (i = 0; i < 9; i++)
            {
                values_len[i] = strlen(columns_value[i]);
            }
            
            if ((res = database_management.update(9, columns_name, columns_value, values_len)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_update failed", res);
                return res;
            }
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "The database update success!", 0); 
            
            break;
        }
        else if (pad_and_6410_msg.cmd == ERROR_STATUS)
        {
            res = P_DATA_ERR;
            continue;
        }
    }
    return res;
}
#endif // BOARDTYPE == 9344


/**
 * 请求命令字分析
 */
static int request_cmd_analyse(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    char *buf = NULL;
    
    struct timeval tpstart, tpend;
    memset(&tpstart, 0, sizeof(struct timeval));
    memset(&tpend, 0, sizeof(struct timeval));

    int fd = terminal_dev_register->fd;
    int mode = terminal_dev_register->communication_mode;
    
    struct s_pad_and_6410_msg pad_and_6410_msg;
    memset(&pad_and_6410_msg, 0, sizeof(struct s_pad_and_6410_msg));
    pad_and_6410_msg.head[0] = 0x5A;
    pad_and_6410_msg.head[1] = 0xA5;
    
    int index = 0;
    unsigned char cmd = 0;
    
    gettimeofday(&tpstart, NULL); // 得到当前时间
    
    // 接收pad发送的数据
    if ((res = network_config.recv_msg_from_pad(fd, &pad_and_6410_msg)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg_from_pad failed", res);
        goto EXIT;
    }
    
    PRINT("pad_and_6410_msg.cmd = %02X, mode = %02X\n", pad_and_6410_msg.cmd, mode);
    switch (mode)
    {
        case 0: // 非网络
        {
            switch (pad_and_6410_msg.cmd)
            {
                case STOP_CONFIG:   //0x53: 取消当前配置
                case INIT_LAN: // 设备绑定 即生成隐藏SSID
                {
                    cmd = pad_and_6410_msg.cmd;
                    break;
                }
                default:
                {
                    res = P_DATA_ERR;
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "Data does not match", res);
                    break;
                }
            }
            break;
        }
        case 1: // 网络
        {
            switch (pad_and_6410_msg.cmd)
            {
                #if BOARDTYPE == 9344
                case PAD_MAKE_IDENTIFYING_CODE:   //0x05: 注册命令，PAD将随机生成的4字节串码发给base
                case INTELLIGENT_DEV_REGISTER_REQUEST:   //0x06: 注册命令，智能设备将 设备名称、id、mac发送给BASE
                case INTELLIGENT_DEV_CHANGE_NAME:   //0x0B: 设备名称修改命令，智能设备将 mac、设备名称发送给BASE
                #endif // BOARDTYPE == 9344
                case RESTORE_FACTORY:   //0x0C: 恢复出厂（终端初始化前的状态）
                case BASE_VERSION:   //0x0D: 查看Base版本
                #if CTSI_SECURITY_SCHEME == 2
                case GET_DEV_TOKEN:   //0x0E: 获取身份令牌
                case GET_POSITION_TOKEN:   //0x0F: 获取位置令牌
                case REDRAW_DEV_TOKEN:   //0x50: 重新生成身份令牌
                case REDRAW_POSITION_TOKEN:   //0x51: 重新生成位置令牌
                #endif // CTSI_SECURITY_SCHEME == 2
                case GET_WAN_STATE:   //0x52: 查看WAN口
                case STOP_CONFIG:   //0x53: 取消当前配置
                #if BOARDTYPE == 9344
                case INTELLIGENT_DEV_IDENTIFYING_CODE_COMPARE:   //0x54：串码对比，智能设备将输入的串码发送到base，base进行对比
                case PAD_GET_REQUEST_REGISTER_DEV:   //0x55：PAD扫描“发送注册申请”的设备
                case PAD_GET_REGISTER_SUCCESS_DEV:   //0x56：查询已经注册的设备
                case PAD_LOGOUT_DEV_BY_SN:   //0x57：注销命令，删除匹配序列号的行
                case INTELLIGENT_DEV_LOGOUT:   //0x58：注销命令，智能设备将mac地址发送到base
                #endif // BOARDTYPE == 9344
                {
                    cmd = pad_and_6410_msg.cmd;
                    break;
                }
                default:
                {
                    res = P_DATA_ERR;
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "Data does not match", res);
                    break;
                }
            }
            break;
        }
        default:
        {
            res = P_DATA_ERR;
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "Data does not match", res);
            break;
        }
    }

    if (res < 0)
    {
        goto EXIT;
    }
    
    if ((res = get_register_state()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_register_state failed!", res);
        return res;
    }
    
    terminal_dev_register->data_table.register_state = (unsigned char)res;
    *network_config.pad_cmd = terminal_dev_register->data_table.register_state;
    PRINT("register_state = %02X, *network_config.pad_cmd = %02X\n", terminal_dev_register->data_table.register_state, (unsigned char)*network_config.pad_cmd);

    // 不是智能设备都要验证 序列号 // 所有都要验证 序列号 通过无线
    #if BOARDTYPE == 9344
    if ((pad_and_6410_msg.cmd != INTELLIGENT_DEV_REGISTER_REQUEST) && (pad_and_6410_msg.cmd != INTELLIGENT_DEV_CHANGE_NAME) && (pad_and_6410_msg.cmd != INTELLIGENT_DEV_IDENTIFYING_CODE_COMPARE) && (pad_and_6410_msg.cmd != INTELLIGENT_DEV_LOGOUT) && (mode == 1)) // 网络通道
    #endif
    {
        switch (pad_and_6410_msg.cmd)
        {
            default:
            {
                index = 0;
                break;
            }
        }

        if (strlen(terminal_dev_register->data_table.pad_sn) != SN_LEN)
        {
            res = NO_RECORD_ERR;
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "There is no (pad_sn) record!", res);
            goto EXIT;
        }

        PRINT("pad_and_6410_msg.data = %s, terminal_dev_register->data_table.pad_sn = %s\n", pad_and_6410_msg.data + index, terminal_dev_register->data_table.pad_sn);

        if (memcmp(terminal_dev_register->data_table.pad_sn, pad_and_6410_msg.data + index, strlen(terminal_dev_register->data_table.pad_sn)) != 0)
        {
            // 要记录一个状态，用于确定是否要恢复出厂
            if ((unsigned char)*network_config.pad_cmd == WRONGFUL_DEV_STATUS) //广域网设置成功，由于pad和base不是合法设备
            {
                *network_config.cmd = INITUAL_STATUS;
                *network_config.pad_cmd = INITUAL_STATUS;
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "new pad!", 0);
            }
            else
            {
                res = P_DATA_ERR;
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
                goto EXIT;
            }
        }
    }
    PRINT("pad_and_6410_msg.len = %d\n", pad_and_6410_msg.len);
    terminal_dev_register->length = pad_and_6410_msg.len - 1;
    memset(terminal_dev_register->data, 0, sizeof(terminal_dev_register->data));
    memcpy(terminal_dev_register->data, pad_and_6410_msg.data, (pad_and_6410_msg.len - 1));
    
    // 判断命令字
    switch (cmd)
    {
        case DYNAMIC_CONFIG_AND_REGISTER:
        case STATIC_CONFIG_AND_REGISTER:
        case PPPOE_CONFIG_AND_REGISTER:
        #if BOARDTYPE == 5350
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0A:
        #endif // BOARDTYPE == 5350
        {
            terminal_dev_register->cmd_word = cmd;
            terminal_dev_register->cmd_count = pad_and_6410_msg.data[0];
            res = request_cmd_0x01_02_03_07_08_09_0A(terminal_dev_register);
            break;
        }
        case ASK_BASE:
        {
            res = request_cmd_0x04(terminal_dev_register);
            break;
        }
        case PAD_MAKE_IDENTIFYING_CODE:
        {
            res = request_cmd_0x05(terminal_dev_register);
            break;
        }
        case INTELLIGENT_DEV_REGISTER_REQUEST:
        {
            res = request_cmd_0x06(terminal_dev_register);
            PRINT("after 0x06\n");
            break;
        }
        case INTELLIGENT_DEV_CHANGE_NAME:
        {
            res = request_cmd_0x0B(terminal_dev_register);
            break;
        }
        case RESTORE_FACTORY:
        {
            res = request_cmd_0x0C(terminal_dev_register);
            break;
        }
        case BASE_VERSION:
        {
            res = request_cmd_0x0D(terminal_dev_register);
            break;
        }
        case GET_DEV_TOKEN:
        {
            res = request_cmd_0x0E(terminal_dev_register);
            break;
        }
        case GET_POSITION_TOKEN:
        {
            res = request_cmd_0x0F(terminal_dev_register);
            break;
        }
        case REDRAW_DEV_TOKEN:
        {
            res = request_cmd_0x50(terminal_dev_register);
            break;
        }
        case REDRAW_POSITION_TOKEN:
        {
            res = request_cmd_0x51(terminal_dev_register);
            break;
        }
        case GET_WAN_STATE:
        {
            res = request_cmd_0x52(terminal_dev_register);
            break;
        }
        case STOP_CONFIG:
        {
            res = request_cmd_0x53(terminal_dev_register);
            break;
        }
        #if BOARDTYPE == 9344
        case INTELLIGENT_DEV_IDENTIFYING_CODE_COMPARE:
        {
            res = request_cmd_0x54(terminal_dev_register);
            break;
        }
        case PAD_GET_REQUEST_REGISTER_DEV:
        {
            res = request_cmd_0x55(terminal_dev_register);
            break;
        }
        case PAD_GET_REGISTER_SUCCESS_DEV:
        {
            res = request_cmd_0x56(terminal_dev_register);
            break;
        }
        case PAD_LOGOUT_DEV_BY_SN:
        {
            res = request_cmd_0x57(terminal_dev_register);
            break;
        }
        case INTELLIGENT_DEV_LOGOUT:
        {
            res = request_cmd_0x58(terminal_dev_register);
            break;
        }
        case INIT_LAN: // 设备绑定 即生成隐藏SSID
        {
            res = request_cmd_0x60(terminal_dev_register);
            break;
        }
        #endif // BOARDTYPE == 9344
        default:
        {
            res = P_DATA_ERR;
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "Data does not match", res);
            break;
        }
    }

EXIT:
    // 等于1时说明此时正在设置
    if ((terminal_dev_register->config_now_flag != 1) || (cmd == STOP_CONFIG))
    {
        network_config.init_cmd_list();  // 初始化命令结构体
    }
    
    PRINT("res = %d\n", res);
    if (res < 0)
    {
        common_tools.work_sum->work_failed_sum++; // 记录测试失败次数
        // 获取用户提示
        if (common_tools.get_user_prompt(res, &buf) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_user_prompt failed", res);
            if (buf != NULL)
            {
                free(buf);
                buf = NULL;
            }
        }
        if (buf != NULL)
        {
            PRINT("buf = %s\n", buf);
        }

        if (network_config.send_msg_to_pad(fd, ERROR_STATUS, buf, strlen(buf)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed!", res);
        }
        else
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send success!", 0);
        }
        
        if (mode == 1) // 网络通信
        {
            close(fd);
            terminal_dev_register->fd = 0;
        }
    }
    else 
    {
        common_tools.work_sum->work_success_sum++; // 记录测试正确次数
        if ((terminal_dev_register->config_now_flag != 1) && (mode == 1))
        {
            close(fd);
            terminal_dev_register->fd = 0;
        }
    }
    
    if (buf != NULL)
    {
        free(buf);
        buf = NULL;
    }
    if (pad_and_6410_msg.data != NULL)
    {
        free(pad_and_6410_msg.data);
        pad_and_6410_msg.data = NULL;
    }
    
    // 统计测试情况
    gettimeofday(&tpend, NULL);
    common_tools.make_menulist(start_time, tpstart, tpend);
    common_tools.work_sum->work_sum++;

    network_config.pthread_flag = 0;
    return 0;
}


/**
 * 获取注册状态
 */
int get_register_state()
{
    int res = 0;
    char column_name[1][30] = {"register_state"};
    char column_value[1][100] = {0};
    
    #if BOARDTYPE == 9344
    if ((res = database_management.select(1, column_name, column_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);
        return res;
    }
    #elif BOARDTYPE == 5350
    if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, column_name, column_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);
        return res;
    }
    #endif
    
    res = atoi(column_value[0]);
    PRINT("register_state = %02X\n", (unsigned char)res);
    return (unsigned char)res;
}
 
/**
 * 初始化化表
 */
int init_data_table(struct s_data_table *data_table)
{
    if (data_table == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data_table is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    memset(data_table, 0, sizeof(struct s_data_table));
    
    int res = 0;
    char columns_name[4][30] = {"base_sn", "base_mac", "pad_sn", "pad_mac"};
    char columns_value[4][100] = {0};

    #if BOARDTYPE == 9344
    if ((res = database_management.select(4, columns_name, columns_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);
        return res;
    }
    #elif BOARDTYPE == 5350
    if ((res = nvram_interface.select(RT5350_FREE_SPACE, 4, columns_name, columns_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);
        return res;
    }
    #endif

    memcpy(data_table->base_sn, columns_value[0], sizeof(data_table->base_sn) - 1);
    memcpy(data_table->base_mac, columns_value[1], sizeof(data_table->base_mac) - 1);
    memcpy(data_table->pad_sn, columns_value[2], sizeof(data_table->pad_sn) - 1);
    memcpy(data_table->pad_mac, columns_value[3], sizeof(data_table->pad_mac) - 1);
    if ((res = get_register_state()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_register_state failed!", res);
        return res;
    }
    data_table->register_state = (unsigned char)res;
    
    #if BOARDTYPE == 5350
    strcpy(data_table->default_ssid, "handaer_wifi_register");
    strcpy(data_table->default_ssid_password, "12345678");
    #endif
    PRINT("data_table->base_sn = %s\n", data_table->base_sn);
    PRINT("data_table->base_mac = %s\n", data_table->base_mac);
    PRINT("data_table->pad_sn = %s\n", data_table->pad_sn);
    PRINT("data_table->pad_mac = %s\n", data_table->pad_mac);
    PRINT("data_table->register_state = %02X\n", data_table->register_state);
    return 0;
}

#if BOARDTYPE == 9344

/**
 * 取消mac地址绑定
 */
int cancel_mac_and_ip_bind()
{
    int res = 0;
    char *cmd = "cfg -s | grep \"ADD_MAC:=\"";
    char mac[64] = {0};
    char buf[128] = {0};

    if ((res = common_tools.get_cmd_out(cmd, mac, sizeof(mac))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_cmd_out failed!", res);
        return res;
    }
    mac[strlen(mac) - 1] = '\0';
    PRINT("mac = %s\n", mac);
    
    if (strlen(mac) == 0)
    {
        PRINT("no bind!\n");
        return 0;
    }
    
    sprintf(buf, "cfg -a DELXXX=\"%s\"", mac + strlen("ADD_MAC:="));
    PRINT("buf = %s\n", buf);
    system(buf);
    system("/usr/sbin/del_addr"); // 取消静态绑定
    return 0;
}

/**
 * 获取BASE中存储的信息给PAD
 */
int get_success_buf(char **buf)
{
    int res = 0;
    int len = 0;
    #if USER_REGISTER == 1
    // 需要发送ssid 和 密码
    char columns_name[13][30] = {"base_sn", "base_mac", "base_ip", "base_user_name", "base_password",
        "pad_user_name", "pad_password", "sip_ip", "sip_port", "heart_beat_cycle", "business_cycle",
        "ssid_user_name", "ssid_password"};
        
    char columns_value[13][100] = {0};

    if ((res = database_management.select(13, columns_name, columns_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);
        return res;
    }
    common_tools.mac_add_colon(columns_value[1]);
    
    len = strlen("base_sn:") + strlen("base_mac:") + strlen("base_ip:") +
        strlen("baseUserName:") + strlen("basePassword:") + strlen("padUserName:") +
        strlen("padPassword:") + strlen("sipIpAddress:") + strlen("port:") +
        strlen("heartbeatCycle:") + strlen("businessCycle:") + strlen("ssid_user_name:") + strlen("ssid_password:") + 
        strlen(columns_value[0]) + strlen(columns_value[1]) + strlen(columns_value[2]) +
        strlen(columns_value[3]) + strlen(columns_value[4]) + strlen(columns_value[5]) +
        strlen(columns_value[6]) + strlen(columns_value[7]) + strlen(columns_value[8]) +
        strlen(columns_value[9]) + strlen(columns_value[10]) + strlen(columns_value[11]) + 
        strlen(columns_value[12]) + 14 + 5;

    PRINT("len = %d\n", len);
    if ((*buf = (char *)malloc(len)) == NULL)
    {
        res = MALLOC_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed", res);
        return res;
    }

    memset(*buf, 0, len);
    sprintf(*buf, "base_sn:%s,base_mac:%s,base_ip:%s,baseUserName:%s,basePassword:%s,padUserName:%s,padPassword:%s,sipIpAddress:%s,port:%s,heartbeatCycle:%s,businessCycle:%s,ssid_user_name:%s,ssid_password:%s",
        columns_value[0], columns_value[1], columns_value[2], columns_value[3], columns_value[4], columns_value[5],
        columns_value[6], columns_value[7], columns_value[8], columns_value[9], columns_value[10], columns_value[11], columns_value[12]);
    
    #else // 没有进行设备认证

    char columns_name[5][30] = {"base_sn", "base_mac", "base_ip", "ssid_user_name", "ssid_password"};
    char columns_value[5][100] = {0};

    if ((res = database_management.select(5, columns_name, columns_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);
        return res;
    }
    common_tools.mac_add_colon(columns_value[1]);
    
    len = strlen("base_sn:") + strlen("base_mac:") + strlen("base_ip:") + strlen("ssid_user_name:") + strlen("ssid_password:") +
        strlen(columns_value[0]) + strlen(columns_value[1]) + strlen(columns_value[2]) + strlen(columns_value[3]) + strlen(columns_value[4]) + 6;

    PRINT("len = %d\n", len);
    if ((*buf = (char *)malloc(len)) == NULL)
    {
        res = MALLOC_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed", res);
        return res;
    }

    memset(*buf, 0, len);
    sprintf(*buf, "base_sn:%s,base_mac:%s,base_ip:%s,ssid_user_name:%s,ssid_password:%s", columns_value[0], columns_value[1], columns_value[2], columns_value[3], columns_value[4]);
    #endif // #if USER_REGISTER == 1
    PRINT("%s\n", *buf);
    return 0;
}

/**
 * 打印设备列表
 */
int print_dev_table(unsigned short dev_count, void *dev_info)
{
    if (dev_info == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    int i = 0;
    struct s_dev_register *dev_tmp;
    PRINT("dev_count = %d\n", dev_count);
    for (i = 0; i < dev_count; i++)
    {
        dev_tmp = (struct s_dev_register *)(dev_info + i * sizeof(struct s_dev_register));
        PRINT("%d %s %s %s %s\n", i, dev_tmp->dev_name, dev_tmp->dev_id, dev_tmp->dev_mac, dev_tmp->dev_code);
    }
    return 0;
}

/**
 * 判断此设备是否在此表中
 * 0:不存在
 * 1:存在
 * < 0:错误
 */
int is_in_dev_table(unsigned short dev_count, void *dev_info, struct s_dev_register *dev_register)
{
    if (dev_info == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    if (dev_register == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    int i = 0;
    struct s_dev_register *dev_tmp;

    for (i = 0; i < dev_count; i++)
    {
        dev_tmp = (struct s_dev_register *)(dev_info + i * sizeof(struct s_dev_register));
        if (memcmp(dev_tmp->dev_mac, dev_register->dev_mac, strlen(dev_register->dev_mac)) == 0)
        {
            PRINT("This dev in table!\n");
            return 1;
        }
    }
    return 0;
}

/**
 * 添加指定行 （内存）
 */
int add_dev_info(unsigned short *dev_count, void **dev_info, struct s_dev_register *dev_register)
{
    int res = 0;
    int malloc_len = sizeof(struct s_dev_register);

    if (dev_register == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data is NULL!", NULL_ERR);
        return NULL_ERR;
    }

    if (*dev_info == NULL)
    {
        if ((*dev_info = malloc(malloc_len)) == NULL)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed", MALLOC_ERR);
            return MALLOC_ERR;
        }
    }
    else
    {
        res = is_in_dev_table(*dev_count, *dev_info, dev_register);
        PRINT("res = %d\n", res);
        switch (res)
        {
            case 0: // 不存在
            {
                if ((*dev_info = realloc(*dev_info, malloc_len * (*dev_count + 1))) == NULL)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "realloc failed", MALLOC_ERR);
                    return MALLOC_ERR;
                }
                break;
            }
            case 1: // 已存在
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "This record already exists!", 0);
                // return REPEAT_ERR;
				return 0;
            }
            default:
            {
                return res;
            }
        }
    }
    memcpy((*dev_info) + (*dev_count) * malloc_len, dev_register, malloc_len);
    (*dev_count)++;
    PRINT("*dev_count = %d\n", *dev_count);
    print_dev_table(*dev_count, *dev_info);
    PRINT("after print_dev_table *dev_info = %p\n", *dev_info);
    return 0;
}

/**
 * 更新指定行串码和串码时间
 */
int update_dev_info_code(unsigned short dev_count, void *dev_info, struct s_dev_register *dev_register)
{
    if (is_in_dev_table(dev_count, dev_info, dev_register) != 1)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", DATA_ERR);
        return DATA_ERR;
    }

    int i = 0;
    struct s_dev_register *dev_tmp;
    for (i = 0; i < dev_count; i++)
    {
        dev_tmp = (struct s_dev_register *)(dev_info + i * sizeof(struct s_dev_register));
        if (memcmp(dev_tmp->dev_mac, dev_register->dev_mac, strlen(dev_register->dev_mac)) == 0)
        {
            PRINT("dev_register->dev_code = %s\n", dev_register->dev_code);
            memcpy(dev_tmp->dev_code, dev_register->dev_code, 4);
            dev_tmp->end_time = time(NULL) + 1800;  // 有效期30分钟
            return 0;
        }
    }
    return DATA_ERR;
}

/**
 * 删除指定行（内存）
 */
int delete_dev_info(unsigned short *dev_count, void **dev_info, struct s_dev_register *dev_register)
{
    int res = 0;
    res = is_in_dev_table(*dev_count, *dev_info, dev_register);
    PRINT("res = %d\n", res);
    switch (res)
    {
        case 0:
        {
            return 0;
        }
        case 1:
        {
            break;
        }
        default:
        {
            return res;
        }
    }

    int i = 0;
    void *dev_info_tmp = NULL;
    struct s_dev_register *dev_tmp;
    for (i = 0; i < *dev_count; i++)
    {
        dev_tmp = (struct s_dev_register *)((*dev_info) + i * sizeof(struct s_dev_register));
        PRINT("dev_tmp->dev_mac = %s\n", dev_tmp->dev_mac);
        if (memcmp(dev_tmp->dev_mac, dev_register->dev_mac, strlen(dev_register->dev_mac)) == 0)
        {
            memset(dev_register->dev_name, 0, sizeof(dev_register->dev_name));
            memcpy(dev_register->dev_name, dev_tmp->dev_name, strlen(dev_tmp->dev_name));
            
            memset(dev_tmp, 0, sizeof(struct s_dev_register));
            memcpy(dev_tmp, dev_tmp + sizeof(struct s_dev_register), ((*dev_count) - i - 1) * sizeof(struct s_dev_register));
            (*dev_count)--;
            
            if (*dev_count == 0)
            {
                free(*dev_info);
                *dev_info = NULL;
            }
            else 
            {
                if ((dev_info_tmp = realloc(*dev_info, (*dev_count) * sizeof(struct s_dev_register))) == NULL)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "realloc failed", MALLOC_ERR);
                    return MALLOC_ERR;
                }
                *dev_info = dev_info_tmp;
            }
            return 0;
        }
    }
    return DATA_ERR;
}

/**
 * 查询内存表（内存）
 */
int select_dev_info(unsigned short dev_count, void *dev_info, char *buf, unsigned short len)
{
    if (dev_info == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data is NULL!", NULL_ERR);
        return NO_REQUEST_ERR;
    }
    if (buf == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    if (dev_count == 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "dev table is NULL!", NULL_ERR);
        return NO_REQUEST_ERR;
    }
    
    int i = 0;
    unsigned short buf_len = 0;
    struct s_dev_register *dev_tmp;
    for (i = 0; i < dev_count; i++)
    {
        dev_tmp = (struct s_dev_register *)(dev_info + i * sizeof(struct s_dev_register));

        snprintf(buf + buf_len, len - buf_len, "%s,%s,%s;", dev_tmp->dev_name, dev_tmp->dev_id, dev_tmp->dev_mac);
        buf_len = strlen(dev_tmp->dev_name) + strlen(dev_tmp->dev_id) + strlen(dev_tmp->dev_mac) + 3;
    }
    PRINT("buf = %s\n", buf);
    return 0;
}

/**
 * 串码对比
 */
int dev_code_comparison(unsigned short dev_count, void *dev_info, struct s_dev_register *dev_register)
{
    if (dev_info == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    if (dev_register == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data is NULL!", NULL_ERR);
        return NULL_ERR;
    }

    int i = 0;
    char dev_existent = 0;

    struct s_dev_register *dev_tmp;
    for (i = 0; i < dev_count; i++)
    {
        dev_tmp = (struct s_dev_register *)(dev_info + i * sizeof(struct s_dev_register));
        if (memcmp(dev_tmp->dev_mac, dev_register->dev_mac, strlen(dev_register->dev_mac)) == 0)
        {
            if (dev_tmp->end_time == 0) // 说明PAD端还没有发送串码
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "don't have dev_code!", NON_EXISTENT_ERR);
                return NON_EXISTENT_ERR;
            }
            if ((dev_tmp->end_time - time(NULL)) < 0) // 说明此时串码过期
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "dev_code timeout!", OVERDUE_ERR);
                dev_tmp->end_time = 0;
                memset(dev_tmp->dev_code, 0, sizeof(dev_tmp->dev_code));
                return OVERDUE_ERR;
            }
            if (memcmp(dev_tmp->dev_code, dev_register->dev_code, 4) != 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "dev_code error!", IDENTIFYING_CODE_ERR);
                return IDENTIFYING_CODE_ERR;
            }
            dev_existent = 1;
            break;
        }
    }
    if (dev_existent == 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "non existent error!", NON_EXISTENT_ERR);
        return NON_EXISTENT_ERR;
    }
    return 0;
}


/**
 * 判断是否在数据库（数据库）
 * 0:不存在
 * 1:存在
 * < 0:错误
 */
int is_in_dev_database(struct s_dev_register *dev_register)
{
    int res = 0;
    int i = 0;
    unsigned short len = 0;
    char buf[1024] = {0};
    char *tmp = NULL;
    char *index = NULL;
    char *row_index = NULL;
    
    char columns_name[3][30] = {"device_name", "device_id", "device_mac"};
    if ((res = database_management.select_table(3, columns_name, buf, sizeof(buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select_table failed!", res);
        return res;
    }
    
    index = buf;
    len = strlen(buf);
    
    while (1) 
    {
        if ((index != NULL) && ((index - buf) >= len))
        {
            return 0;
        }
        
        if ((tmp = strstr(index, ";")) == NULL)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "To match the string was not found in the buffer!", STRSTR_ERR);
            return STRSTR_ERR;
        }
        *tmp = '\0';
        row_index = index;
        index = tmp + 1;
        
        // device_mac
        if ((tmp = strrchr(row_index, ',')) == NULL)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "To match the string was not found in the buffer!", STRSTR_ERR);
            return STRSTR_ERR;
        }
        PRINT("tmp = %s\n", tmp + 1);
        PRINT("dev_register->dev_mac = %s\n", dev_register->dev_mac);
        
        if (memcmp(tmp + 1, dev_register->dev_mac, 12) == 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select_table failed!", res);
            return 1;
        }
    }
}

/**
 * 添加指定行 （数据库）
 */
int database_insert_dev_info(struct s_dev_register *dev_register)
{
    int res = 0;
    char columns_name[4][30] = {"device_name", "device_id", "device_mac", "device_code"};
    char columns_value[4][100] = {0};
    unsigned short columns_value_len[4] = {0};
    
    res = is_in_dev_database(dev_register);
    switch (res)
    {
        case 0: // 不存在
        {
            break;
        }
        case 1: // 数据库中已经存在
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "This record already exists!", REPEAT_ERR);
            return REPEAT_ERR;
        }
        default:
        {
            return res;
        }    
    }
    
    memcpy(columns_value[0], dev_register->dev_name, strlen(dev_register->dev_name));
    memcpy(columns_value[1], dev_register->dev_id, strlen(dev_register->dev_id));
    memcpy(columns_value[2], dev_register->dev_mac, strlen(dev_register->dev_mac));
    memcpy(columns_value[3], dev_register->dev_code, strlen(dev_register->dev_code));
    
    columns_value_len[0] = strlen(columns_value[0]);
    columns_value_len[1] = strlen(columns_value[1]);
    columns_value_len[2] = strlen(columns_value[2]);
    columns_value_len[3] = strlen(columns_value[3]);
    
    PRINT("%s %s %s %s\n", dev_register->dev_name, dev_register->dev_id, dev_register->dev_mac, dev_register->dev_code);
    PRINT("%s %s %s %s\n", columns_value[0], columns_value[1], columns_value[2], columns_value[3]);
    
    if ((res = database_management.insert(4, columns_name, columns_value, columns_value_len)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_insert failed!", res);
        return res;
    }
    return 0;
}

/**
 * 删除指定行 （数据库）
 */
int database_delete_dev_info(struct s_dev_register *dev_register)
{
    int res = 0;
    char columns_name[1][30] = {"device_mac"};
    char columns_value[1][100] = {0};
    
    res = is_in_dev_database(dev_register);
    switch (res)
    {
        case 0: // 不存在
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "There is no this record!", NO_RECORD_ERR);
            return NO_RECORD_ERR;
        }
        case 1: // 数据库中已经存在
        {
            break;
        }
        default:
        {
            return res;
        }    
    }
    
    memcpy(columns_value[0], dev_register->dev_mac, strlen(dev_register->dev_mac));
    if ((res = database_management.delete_row(1, columns_name, columns_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_delete_row failed!", res);
        return res;
    }
    return 0;
}

/**
 * 修改指定行 （数据库）
 */
int database_update_dev_info(struct s_dev_register *dev_register)
{
    int res = 0;
    char *tmp = NULL;
    char *index = NULL;
    
    // 查询此行
    char sql[256] = {0};
    char buf[sizeof(struct s_dev_register) + 4] = {0};
    
    snprintf(sql, sizeof(sql), "select * from %s where device_mac =\'%s\'", 
        REGISTERTB, dev_register->dev_mac);
    if ((res = database_management.exec_select_cmd(sql, common_tools.config->db, buf, sizeof(buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_exec_cmd failed!", res);
        return res;
    }
    
    index = buf;
    
    if ((index == NULL) || (strcmp(index, "") == 0))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "There is no this record!", NON_EXISTENT_ERR);
        return NON_EXISTENT_ERR;
    }
    
    // device_name
    if ((tmp = strchr(index, ',')) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "To match the string was not found in the buffer!", STRSTR_ERR);
        return STRSTR_ERR;
    }
    
    *tmp = '\0';
    index = tmp + 1;
    
    // device_id
    if ((tmp = strchr(index, ',')) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "To match the string was not found in the buffer!", STRSTR_ERR);
        return STRSTR_ERR;
    }
    
    *tmp = '\0';
    memset(dev_register->dev_id, 0, sizeof(dev_register->dev_id));
    memcpy(dev_register->dev_id, index, strlen(index));
    
    index = tmp + 1;
    
    // device_mac
    if ((tmp = strchr(index, ',')) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "To match the string was not found in the buffer!", STRSTR_ERR);
        return STRSTR_ERR;
    }
    
    *tmp = '\0';
    index = tmp + 1;
    
    // device_code
    if ((tmp = strchr(index, ';')) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "To match the string was not found in the buffer!", STRSTR_ERR);
        return STRSTR_ERR;
    }
    
    *tmp = '\0';
    memset(dev_register->dev_code, 0, sizeof(dev_register->dev_code));
    memcpy(dev_register->dev_code, index, strlen(index));
    
    // 更新指定的行
    memset(sql, 0, sizeof(sql));
    PRINT("%s %s %s %s\n", dev_register->dev_name, dev_register->dev_id, dev_register->dev_mac, dev_register->dev_code);
    snprintf(sql, sizeof(sql), "replace into %s(device_mac,device_name,device_id,device_code) values(\'%s\',\'%s\',\'%s\',\'%s\')", 
        REGISTERTB, dev_register->dev_mac, dev_register->dev_name, dev_register->dev_id, dev_register->dev_code);
    
    
    if ((res = database_management.exec_cmd(sql, common_tools.config->db)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_exec_cmd failed!", res);
        return res;
    }
    return 0;
}

/**
 * 查询注册成功的设备
 */
int database_select_dev_info(char *buf, unsigned short len)
{
    int res = 0;
    char columns_name[3][30] = {"device_name", "device_id", "device_mac"};
    if ((res = database_management.select_table(3, columns_name, buf, len)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select_table failed!", res);
        return res;
    }
    if (strlen(buf) == 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "There is no this record!", NON_EXISTENT_ERR);
        return NON_EXISTENT_ERR;
    }
    return 0;
}

#endif // BOARDTYPE == 9344
