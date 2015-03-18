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
 * 请求命令字分析 0x05: 9344 注册命令，PAD将随机生成的4字节串码发给base
 */
static int request_cmd_pad_make_identifying_code(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x06 9344时 注册命令，智能设备将 设备名称、id、mac发送给BASE
 */
static int request_cmd_intelligent_dev_register_request(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x0B 
 * 9344时 设备名称修改命令，智能设备将 mac、设备名称发送给BASE
 */
static int request_cmd_intelligent_dev_change_name(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x0C 恢复出厂
 */
static int request_cmd_restore_factory(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x0D 查看Base版本信息
 */
static int request_cmd_get_base_version(struct s_terminal_dev_register * terminal_dev_register);

#if CTSI_SECURITY_SCHEME == 2
/**
 * 请求命令字分析 0x0E 获取身份令牌
 */
static int request_cmd_get_dev_token(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x0F 获取位置令牌
 */
static int request_cmd_get_position_token(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x50 重新生成身份令牌
 */
static int request_cmd_redraw_dev_token(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x51 重新生成位置令牌
 */
static int request_cmd_redraw_position_token(struct s_terminal_dev_register * terminal_dev_register);
#endif // CTSI_SECURITY_SCHEME == 2

/**
 * 请求命令字分析 0x53 取消当前操作
 */
static int request_cmd_stop_config(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x54 串码对比，智能设备将输入的串码发送到base，base进行对比
 */
static int request_cmd_intelligent_dev_identifying_code_compare(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x55 PAD扫描“发送注册申请”的设备
 */
static int request_cmd_pad_get_request_register_dev(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x56 查询已经注册的设备
 */
static int request_cmd_pad_get_register_success_dev(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x57 注销命令，删除匹配序列号的行
 */
static int request_cmd_pad_logout_dev_by_sn(struct s_terminal_dev_register * terminal_dev_register);

/**
 * 请求命令字分析 0x58 注销命令，智能设备，删除匹配MAC的行
 */
static int request_cmd_intelligent_dev_logout(struct s_terminal_dev_register * terminal_dev_register);

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

/*************************************************************************************************/
/*************************************************************************************************/
/*************************************************************************************************/


struct class_request_cmd request_cmd = 
{
    .init = init,
    .request_cmd_analyse = request_cmd_analyse,
    .init_data_table = init_data_table,
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
 * 请求命令字分析 0x05: 9344 注册命令，PAD将随机生成的4字节串码发给base
 */
int request_cmd_pad_make_identifying_code(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    int len = terminal_dev_register->length;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
	struct s_dev_register dev_register;
	memset(&dev_register, 0, sizeof(struct s_dev_register));
	
    int i = 0;
    int index = 0;
    if (register_state != SUCCESS_STATUS) // PAD没有注册成功
    {
        res = NO_INIT_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
        return res;
    }
    
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
    return res;
}

/**
 * 请求命令字分析 0x06 
 * 9344时 注册命令，智能设备将 设备名称、id、mac发送给BASE
 */
int request_cmd_intelligent_dev_register_request(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    int len = terminal_dev_register->length;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
	struct s_dev_register dev_register;
	memset(&dev_register, 0, sizeof(struct s_dev_register));
    
    int i = 0;
    int index = 0;
    unsigned short length = 0;
    if (register_state != SUCCESS_STATUS) // PAD没有注册成功
    {
        res = NO_INIT_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
        return res;
    }
    
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
    return res;
}

/**
 * 请求命令字分析 0x0B 
 * 9344时 设备名称修改命令，智能设备将 mac、设备名称发送给BASE
 */
int request_cmd_intelligent_dev_change_name(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    int len = terminal_dev_register->length;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
    
    if (register_state != SUCCESS_STATUS) // PAD没有注册成功
    {
        res = NO_INIT_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
        return res;
    }
    
    struct s_dev_register dev_register;
	memset(&dev_register, 0, sizeof(struct s_dev_register));
	
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
    
    return res;
}

/**
 * 请求命令字分析 0x0C 恢复出厂
 */
int request_cmd_restore_factory(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;

    if ((res = network_config.send_msg_to_pad(fd, SUCCESS_STATUS, NULL, 0)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
        return res;
    }
    sleep(1);
    
    // 1.情况数据库
    if ((res = database_management.clear()) < 0)
    {
        PRINT("sqlite3_clear_table failed!\n");
        return res;
    }
    
    // 2.删除隐藏SSID
    system("cfg -b 5");
    
    system("kill -9 `ps | grep monitor_application | sed '/grep/'d | awk '{print $1}'`");
    system("ps");
    PRINT("after kill monitor_application\n");
    
    PRINT("before exec reboot\n");
    system("reboot");
    PRINT("after exec reboot\n");
    
    // 
    exit(0);
}

/**
 * 请求命令字分析 0x0D 查看Base版本信息
 */
int request_cmd_get_base_version(struct s_terminal_dev_register * terminal_dev_register)
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
int request_cmd_get_dev_token(struct s_terminal_dev_register * terminal_dev_register)
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
int request_cmd_get_position_token(struct s_terminal_dev_register * terminal_dev_register)
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
int request_cmd_redraw_dev_token(struct s_terminal_dev_register * terminal_dev_register)
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
int request_cmd_redraw_position_token(struct s_terminal_dev_register * terminal_dev_register)
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
 * 请求命令字分析 0x53 取消当前操作
 */
int request_cmd_stop_config(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->network_config_fd;
    
    if (terminal_dev_register->config_now_flag == 1)
    {
        //pthread_cancel(terminal_dev_register->request_cmd_0x01_02_03_07_08_09_0A_id);
        PRINT("after pthread_cancel!\n");
        
        // 是否需要pthread_join
        //pthread_join(terminal_dev_register->request_cmd_0x01_02_03_07_08_09_0A_id, NULL);
        
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
/**
 * 请求命令字分析 0x54 串码对比，智能设备将输入的串码发送到base，base进行对比
 */
int request_cmd_intelligent_dev_identifying_code_compare(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
    
    int index = 0;
    unsigned short length = 0;
    
    struct s_dev_register dev_register;
    memset(&dev_register, 0, sizeof(struct s_dev_register));

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
int request_cmd_pad_get_request_register_dev(struct s_terminal_dev_register * terminal_dev_register)
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
int request_cmd_pad_get_register_success_dev(struct s_terminal_dev_register * terminal_dev_register)
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
int request_cmd_pad_logout_dev_by_sn(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
    
    char buf[1024] = {0};
    struct s_dev_register dev_register;
    memset(&dev_register, 0, sizeof(struct s_dev_register));
    
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
int request_cmd_intelligent_dev_logout(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    int len = terminal_dev_register->length;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
    
    struct s_dev_register dev_register;
    memset(&dev_register, 0, sizeof(struct s_dev_register));
    
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
int request_cmd_init_lan(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    int fd = terminal_dev_register->fd;
    int len = terminal_dev_register->length;
    unsigned char register_state = (unsigned char)terminal_dev_register->data_table.register_state;
    
    struct s_dev_register dev_register;
    memset(&dev_register, 0, sizeof(struct s_dev_register));
    
    unsigned char redraw_flag = 0;
    char buf[256] = {0};
    char cmd_buf[256] = {0};
    
    char pad_sn[SN_LEN + 1] = {0};
    char pad_mac[18] = {0};
    
    char base_sn[SN_LEN + 1] = {0};
    char base_mac[18] = {0};
    char ssid1[32] = {0};
    char wpapsk1[32] = {0};
    
    unsigned char columns_count = 0;
    
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
    char columns_name[10][30] = {"base_sn", "base_mac", "base_ip", "ssid_user_name", "ssid_password", "pad_sn", "pad_mac", "pad_ip", "register_state", "authentication_state"};
    char columns_value[10][100] = {0};
    unsigned short values_len[10] = {0};
    
    res = database_management.select(6, columns_name, columns_value);
    PRINT("res = %d\n", res);
    
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
	
	//fixed by suxiaozhi 20140611
	snprintf(cmd_buf, sizeof(cmd_buf), "cfg -b 3 %s_2G %s_5G %s %s &", ssid1, ssid1, wpapsk1, wpapsk1);
	system(cmd_buf);
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
            
            // 如果是一个新的PAD序列号，则认证状态恢复到原始状态
            if (memcmp(pad_sn, columns_value[5], SN_LEN) != 0)
            {
                memcpy(columns_value[9], "240", strlen("240")); // NO_AUTHENTICATION_STATUS
                columns_count = 10;
            }
            else
            {
                columns_count = 9;
            }
            
            memcpy(columns_value[5], pad_sn, strlen(pad_sn));
            memcpy(columns_value[6], pad_mac, strlen(pad_mac));
            memcpy(columns_value[7], "10.10.10.100", strlen("10.10.10.100"));
            memcpy(columns_value[8], "0", strlen("0"));
            
            for (i = 0; i < columns_count; i++)
            {
                values_len[i] = strlen(columns_value[i]);
            }
            
            if ((res = database_management.update(columns_count, columns_name, columns_value, values_len)) < 0)
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
    request_cmd.init_data_table(&terminal_dev_register->data_table);
    
    return res;
}


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
                case PAD_MAKE_IDENTIFYING_CODE:   //0x05: 注册命令，PAD将随机生成的4字节串码发给base
                case INTELLIGENT_DEV_REGISTER_REQUEST:   //0x06: 注册命令，智能设备将 设备名称、id、mac发送给BASE
                case INTELLIGENT_DEV_CHANGE_NAME:   //0x0B: 设备名称修改命令，智能设备将 mac、设备名称发送给BASE
                case RESTORE_FACTORY:   //0x0C: 恢复出厂（终端初始化前的状态）
                case BASE_VERSION:   //0x0D: 查看Base版本
                #if CTSI_SECURITY_SCHEME == 2
                case GET_DEV_TOKEN:   //0x0E: 获取身份令牌
                case GET_POSITION_TOKEN:   //0x0F: 获取位置令牌
                case REDRAW_DEV_TOKEN:   //0x50: 重新生成身份令牌
                case REDRAW_POSITION_TOKEN:   //0x51: 重新生成位置令牌
                #endif // CTSI_SECURITY_SCHEME == 2
                case STOP_CONFIG:   //0x53: 取消当前配置
                case INTELLIGENT_DEV_IDENTIFYING_CODE_COMPARE:   //0x54：串码对比，智能设备将输入的串码发送到base，base进行对比
                case PAD_GET_REQUEST_REGISTER_DEV:   //0x55：PAD扫描“发送注册申请”的设备
                case PAD_GET_REGISTER_SUCCESS_DEV:   //0x56：查询已经注册的设备
                case PAD_LOGOUT_DEV_BY_SN:   //0x57：注销命令，删除匹配序列号的行
                case INTELLIGENT_DEV_LOGOUT:   //0x58：注销命令，智能设备将mac地址发送到base
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
    if ((pad_and_6410_msg.cmd != INTELLIGENT_DEV_REGISTER_REQUEST) && (pad_and_6410_msg.cmd != INTELLIGENT_DEV_CHANGE_NAME) && (pad_and_6410_msg.cmd != INTELLIGENT_DEV_IDENTIFYING_CODE_COMPARE) && (pad_and_6410_msg.cmd != INTELLIGENT_DEV_LOGOUT) && (mode == 1)) // 网络通道
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
        case PAD_MAKE_IDENTIFYING_CODE:
        {
            res = request_cmd_pad_make_identifying_code(terminal_dev_register);
            break;
        }
        case INTELLIGENT_DEV_REGISTER_REQUEST:
        {
            res = request_cmd_intelligent_dev_register_request(terminal_dev_register);
            PRINT("after 0x06\n");
            break;
        }
        case INTELLIGENT_DEV_CHANGE_NAME:
        {
            res = request_cmd_intelligent_dev_change_name(terminal_dev_register);
            break;
        }
        case RESTORE_FACTORY:
        {
            res = request_cmd_restore_factory(terminal_dev_register);
            break;
        }
        case BASE_VERSION:
        {
            res = request_cmd_get_base_version(terminal_dev_register);
            break;
        }
        case GET_DEV_TOKEN:
        {
            res = request_cmd_get_dev_token(terminal_dev_register);
            break;
        }
        case GET_POSITION_TOKEN:
        {
            res = request_cmd_get_position_token(terminal_dev_register);
            break;
        }
        case REDRAW_DEV_TOKEN:
        {
            res = request_cmd_redraw_dev_token(terminal_dev_register);
            break;
        }
        case REDRAW_POSITION_TOKEN:
        {
            res = request_cmd_redraw_position_token(terminal_dev_register);
            break;
        }
        case STOP_CONFIG:
        {
            res = request_cmd_stop_config(terminal_dev_register);
            break;
        }
        case INTELLIGENT_DEV_IDENTIFYING_CODE_COMPARE:
        {
            res = request_cmd_intelligent_dev_identifying_code_compare(terminal_dev_register);
            break;
        }
        case PAD_GET_REQUEST_REGISTER_DEV:
        {
            res = request_cmd_pad_get_request_register_dev(terminal_dev_register);
            break;
        }
        case PAD_GET_REGISTER_SUCCESS_DEV:
        {
            res = request_cmd_pad_get_register_success_dev(terminal_dev_register);
            break;
        }
        case PAD_LOGOUT_DEV_BY_SN:
        {
            res = request_cmd_pad_logout_dev_by_sn(terminal_dev_register);
            break;
        }
        case INTELLIGENT_DEV_LOGOUT:
        {
            res = request_cmd_intelligent_dev_logout(terminal_dev_register);
            break;
        }
        case INIT_LAN: // 设备绑定 即生成隐藏SSID
        {
            res = request_cmd_init_lan(terminal_dev_register);
            break;
        }
        default:
        {
            res = P_DATA_ERR;
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "Data does not match", res);
            break;
        }
    }

EXIT:
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
    int register_state = -1;
    int authentication_state = 0;
    int res = 0;
    char column_name[2][30] = {"register_state","authentication_state"};
    char column_value[2][100] = {0};
    
    if ((res = database_management.select(2, column_name, column_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);
        return res;
    }
    
    register_state = atoi(column_value[0]);
    PRINT("register_state = %02X\n", (unsigned char)register_state);
    authentication_state = atoi(column_value[1]);
    PRINT("authentication_state = %02X\n", (unsigned char)authentication_state);
    if(authentication_state != 240)
    {
		PRINT("delete db\n");
		system("rm /configure_backup/terminal_dev_register/db/terminal_base_db");

		if ((res = database_management.select(2, column_name, column_value)) < 0)
		{
			OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);
			return res;
		}
		register_state = 251;
	}
    return (unsigned char)register_state;
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
    char columns_name[5][30] = {"base_sn", "base_mac", "pad_sn", "pad_mac", "authentication_state"};
    char columns_value[5][100] = {0};

    if ((res = database_management.select(5, columns_name, columns_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);
        return res;
    }

    memcpy(data_table->base_sn, columns_value[0], sizeof(data_table->base_sn) - 1);
    memcpy(data_table->base_mac, columns_value[1], sizeof(data_table->base_mac) - 1);
    memcpy(data_table->pad_sn, columns_value[2], sizeof(data_table->pad_sn) - 1);
    memcpy(data_table->pad_mac, columns_value[3], sizeof(data_table->pad_mac) - 1);
    data_table->authentication_state = atoi(columns_value[4]);
    
    if ((res = get_register_state()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_register_state failed!", res);
        return res;
    }
    data_table->register_state = (unsigned char)res;
    
    PRINT("data_table->base_sn = %s\n", data_table->base_sn);
    PRINT("data_table->base_mac = %s\n", data_table->base_mac);
    PRINT("data_table->pad_sn = %s\n", data_table->pad_sn);
    PRINT("data_table->pad_mac = %s\n", data_table->pad_mac);
    PRINT("data_table->authentication_state = %02X\n", data_table->authentication_state);
    PRINT("data_table->register_state = %02X\n", data_table->register_state);
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
    PRINT("*dev_info = %p\n", *dev_info);
    for (i = 0; i < *dev_count; i++)
    {
        dev_tmp = (struct s_dev_register *)((*dev_info) + i * sizeof(struct s_dev_register));
        PRINT("dev_tmp->dev_mac = %s\n", dev_tmp->dev_mac);
        if (memcmp(dev_tmp->dev_mac, dev_register->dev_mac, strlen(dev_register->dev_mac)) == 0)
        {
            memset(dev_register->dev_name, 0, sizeof(dev_register->dev_name));
            memcpy(dev_register->dev_name, dev_tmp->dev_name, strlen(dev_tmp->dev_name));
            
            memset(dev_tmp, 0, sizeof(struct s_dev_register));
            memcpy(*dev_info + i * sizeof(struct s_dev_register), *dev_info + (i + 1) * sizeof(struct s_dev_register), ((*dev_count) - i - 1) * sizeof(struct s_dev_register));
            
            PRINT("((*dev_count) - i - 1) * sizeof(struct s_dev_register) = %d\n", ((*dev_count) - i - 1) * sizeof(struct s_dev_register));
            PRINT("name = %s mac = %s dev_code = %s\n", ((struct s_dev_register *)(*dev_info + (i + 1) *  sizeof(struct s_dev_register)))->dev_name, ((struct s_dev_register *)(*dev_info + (i + 1) *  sizeof(struct s_dev_register)))->dev_mac, ((struct s_dev_register *)(*dev_info + (i + 1) * sizeof(struct s_dev_register)))->dev_code);
            (*dev_count)--;
            
            PRINT("*dev_count = %d i = %d\n", *dev_count, i);
            PRINT("name = %s mac = %s dev_code = %s\n", ((struct s_dev_register *)(*dev_info))->dev_name, ((struct s_dev_register *)(*dev_info))->dev_mac, ((struct s_dev_register *)(*dev_info))->dev_code);
            PRINT("*dev_info = %p\n", *dev_info);
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
                PRINT("name = %s mac = %s dev_code = %s\n", ((struct s_dev_register *)(*dev_info))->dev_name, ((struct s_dev_register *)(*dev_info))->dev_mac, ((struct s_dev_register *)(*dev_info))->dev_code);
                PRINT("*dev_info = %p\n", *dev_info); 
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
        buf_len += (strlen(dev_tmp->dev_name) + strlen(dev_tmp->dev_id) + strlen(dev_tmp->dev_mac) + 3);
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
