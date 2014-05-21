#include "terminal_authentication.h"

/**
 * 获取本地存储的设备令牌并返回
 */
static int return_device_token(char *device_token);

/**
 * 获取本地存储的位置令牌并返回
 */
static int return_position_token(char *position_token);

/**
 * 从平台重获取设备令牌并返回
 */
static int rebuild_device_token(char *device_token);

/**
 * 从平台重获取位置令牌并返回
 */
static int rebuild_position_token(char *position_token);

/**
 * 启动CACM
 */
static int start_up_CACM();

/**
 * 初始化结构体
 */
struct class_terminal_authentication terminal_authentication = 
{
    .static_device_token = {0},
    .static_position_token = {0},
    .static_device_flag = 0,
    .static_position_flag = 0,
    .return_device_token = return_device_token, 
    .return_position_token = return_position_token, 
    .rebuild_device_token = rebuild_device_token, 
    .rebuild_position_token = rebuild_position_token, 
    .start_up_CACM = start_up_CACM,
};

/**
 * 获取本地存储的设备令牌并返回
 */
int return_device_token(char *device_token)
{
    int res = 0;
    
    if (device_token == NULL)
    {
        res = NULL_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "device_token is NULL!", res);
        return res;
    }
    
    if (terminal_authentication.static_device_flag == 0)
    {
        if ((res = rebuild_device_token(device_token)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "rebuild_device_token failed!", res);
            return res;
        }
        return 0;
    }
    memcpy(device_token, terminal_authentication.static_device_token, sizeof(terminal_authentication.static_device_token));
    
    return 0;
}

/**
 * 获取本地存储的位置令牌并返回
 */
int return_position_token(char *position_token)
{
    int res = 0;
    
    if (position_token == NULL)
    {
        res = NULL_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "position_token is NULL!", res);
        return res;
    }
    
    // 不等于一说明本地没有位置令牌
    if (terminal_authentication.static_position_flag == 0)
    {
        if ((res = rebuild_position_token(position_token)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "rebuild_device_token failed!", res);
            return res;
        }
        return 0;
    }
    memcpy(position_token, terminal_authentication.static_position_token, sizeof(terminal_authentication.static_position_token));
    return 0;
}

/**
 * 从平台重获取设备令牌并返回
 */
int rebuild_device_token(char *device_token)
{
    int fd_client = 0;
    int i = 0;
    int res = 0;
    unsigned long base_random = 0;
    struct s_data_list data_list;
    struct s_dial_back_respond dial_back_respond;
    memset((void *)&data_list, 0, sizeof(struct s_data_list));
    memset((void *)&dial_back_respond, 0, sizeof(struct s_dial_back_respond));
    char buf[128] = {0};
    if (device_token == NULL)
    {
        res = NULL_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "device_token is NULL!", res);
        return res;
    }
    PRINT("common_tools.config->center_ip = %s common_tools.config->center_port = %s\n", common_tools.config->center_ip, common_tools.config->center_port);
    // 连接服务器
    if ((fd_client = communication_network.make_client_link(common_tools.config->center_ip, common_tools.config->center_port)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "make_client_link failed!", fd_client);
        return common_tools.get_errno('S', fd_client);     
    }
    PRINT("link server success!\n");
    
    char columns_name[2][30] = {"base_sn", "pad_sn"};
    char columns_value[2][100] = {0};
    
    #if BOARDTYPE == 5350
    if ((res = nvram_interface.select(RT5350_FREE_SPACE, 2, columns_name, columns_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_seletc failed!", res);
        return res;
    }
    #elif BOARDTYPE == 9344
    if ((res = database_management.select(2, columns_name, columns_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);
        return res;
    }
    #endif
    for (i = 0; i < 2; i++)
    {
        if ((strcmp("\"\"", columns_value[i]) == 0) || (strlen(columns_value[i]) == 0))
        {
            memset(columns_value[i], 0, sizeof(columns_value[i]));
            sprintf(columns_value[i], "There is no (%s) record!", columns_name[i]);
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[i], NULL_ERR); 
            return NULL_ERR;
        }
    }
    
	memcpy(dial_back_respond.BASE_id, columns_value[0], strlen(columns_value[0]));
	memcpy(dial_back_respond.PAD_id, columns_value[1], strlen(columns_value[1]));
	
	srand((unsigned int)time(NULL));
	base_random = rand();
	memcpy(dial_back_respond.base_random, (void *)&base_random, sizeof(base_random));
	base_random = rand();
	memcpy(dial_back_respond.base_random + sizeof(base_random), (void *)&base_random, sizeof(base_random));
	
	
    // 0x1000请求打包
    dial_back_respond.cmd = 0x1000;
    if ((res = communication_network.msg_pack(&data_list, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_pack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 0x1000请求发送
    if ((res = communication_network.msg_send(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_send failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 0x1001应答
    if ((res = communication_network.msg_recv(fd_client, buf, sizeof(buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_recv failed!", res);        
        close(fd_client);
        return common_tools.get_errno('S', res);
    }
    
    // 0x1001解包
    if ((res = communication_network.msg_unpack(buf, res, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_unpack failed!", res);
        close(fd_client);
        return common_tools.get_errno('S', res);;
    }
    PRINT("XXXXXXXXXXXXXXXXX\n");
    // 0x1002请求打包（确认身份）
    dial_back_respond.cmd = 0x1002;
    if ((res = communication_network.msg_pack(&data_list, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_pack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    PRINT("XXXXXXXXXXXXXXXXX\n");
    // 0x1002请求发送
    if ((res = communication_network.msg_send(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_send failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    memset(buf, 0, sizeof(buf));
    // 0x1003应答（颁发设备令牌）
    if ((res = communication_network.msg_recv(fd_client, buf, sizeof(buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_recv failed!", res);        
        close(fd_client);
        return common_tools.get_errno('S', res);
    }
    PRINT("XXXXXXXXXXXXXXXXX\n");
    // 0x1003解包
    if ((res = communication_network.msg_unpack(buf, res, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_unpack failed!", res);
        close(fd_client);
        return common_tools.get_errno('S', res);
    }
    
    memcpy(device_token, dial_back_respond.device_token, TOKENLEN);
    memcpy(terminal_authentication.static_device_token, dial_back_respond.device_token, TOKENLEN);
    close(fd_client);
    
    terminal_authentication.static_device_flag = 1;
    PRINT_BUF_BY_HEX(dial_back_respond.device_token, NULL, TOKENLEN, __FILE__, __FUNCTION__, __LINE__);
    PRINT_BUF_BY_HEX(terminal_authentication.static_device_token, NULL, TOKENLEN, __FILE__, __FUNCTION__, __LINE__);
    return 0;
}

/**
 * 从平台重获取位置令牌并返回
 */
int rebuild_position_token(char *position_token)
{
    int fd_client = 0;
    int i = 0;
    int res = 0;
    int ret = 0;
    unsigned long base_random = 0;
    struct s_data_list data_list;
    struct s_dial_back_respond dial_back_respond;
    memset((void *)&data_list, 0, sizeof(struct s_data_list));
    memset((void *)&dial_back_respond, 0, sizeof(struct s_dial_back_respond));
    
    struct timeval tv = {5, 1};
    
    char buf[128] = {0};
    int client_fd = 0; // 和PSTN交互
    
    if (position_token == NULL)
    {
        res = NULL_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "position_token is NULL!", res);
        return res;
    }
    
    // 连接服务器
    if ((fd_client = communication_network.make_client_link(common_tools.config->center_ip, common_tools.config->center_port)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "make_client_link failed!", fd_client);
        return common_tools.get_errno('S', fd_client);     
    }
    
    char columns_name[2][30] = {"base_sn", "pad_sn"};
    char columns_value[2][100] = {0};
    
    #if BOARDTYPE == 5350
    if ((res = nvram_interface.select(RT5350_FREE_SPACE, 2, columns_name, columns_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_seletc failed!", res);
        return res;
    }
    #elif BOARDTYPE == 9344
    if ((res = database_management.select(2, columns_name, columns_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);
        return res;
    }
    #endif
    for (i = 0; i < 2; i++)
    {
        if ((strcmp("\"\"", columns_value[i]) == 0) || (strlen(columns_value[i]) == 0))
        {
            memset(columns_value[i], 0, sizeof(columns_value[i]));
            sprintf(columns_value[i], "There is no (%s) record!", columns_name[i]);
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[i], NULL_ERR); 
            return NULL_ERR;
        }
    }
    
	memcpy(dial_back_respond.BASE_id, columns_value[0], strlen(columns_value[0]));
	memcpy(dial_back_respond.PAD_id, columns_value[1], strlen(columns_value[1]));
	
	
	if (terminal_authentication.static_device_flag == 0)
    {
        if ((res = rebuild_device_token(dial_back_respond.device_token)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "rebuild_device_token failed!", res);
            return res;
        }
    }
    else
    {
        memcpy(dial_back_respond.device_token, terminal_authentication.static_device_token, sizeof(terminal_authentication.static_device_token));
    }
	PRINT_BUF_BY_HEX(terminal_authentication.static_device_token, NULL, TOKENLEN, __FILE__, __FUNCTION__, __LINE__);
	PRINT_BUF_BY_HEX(dial_back_respond.device_token, NULL, TOKENLEN, __FILE__, __FUNCTION__, __LINE__);
	
	srand((unsigned int)time(NULL));
	base_random = rand();
	memcpy(dial_back_respond.base_random, (void *)&base_random, sizeof(base_random));
	base_random = rand();
	memcpy(dial_back_respond.base_random + sizeof(base_random), (void *)&base_random, sizeof(base_random));
	
    // 0x2000请求打包
    dial_back_respond.cmd = 0x2000;
    if ((res = communication_network.msg_pack(&data_list, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_pack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 0x2000请求发送
    if ((res = communication_network.msg_send(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_send failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    PRINT("________________0x2000\n");
    
    // 0x2001终端呼叫应答
    if ((res = communication_network.msg_recv(fd_client, buf, sizeof(buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_recv failed!", res);        
        close(fd_client);
        common_tools.list_free(&data_list);
        return common_tools.get_errno('S', res);
    }
    PRINT("________________0x2001\n");
    // 0x2001解包
    if ((res = communication_network.msg_unpack(buf, res, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_unpack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return common_tools.get_errno('S', res);
    }
    
    PRINT("________________0x2001\n");
    
    #if RELAY_CHANGE == 0
    PRINT("SPI_UART1_MUTEX_SOCKET_PORT = %s\n", SPI_UART1_MUTEX_SOCKET_PORT);
    // 发送暂停接收命令到PSTN
    if ((res = communication_network.make_client_link("127.0.0.1", SPI_UART1_MUTEX_SOCKET_PORT)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "make_client_link failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    client_fd = res;
    
    // 打包
    memset(buf, 0, sizeof(buf));
    buf[0] = 0x5A;
    buf[1] = 0xA5;
    buf[2] = 0x01;
    buf[3] = 0x01;
    buf[4] = 0x01;
    
    // 发送
    if ((res = common_tools.send_data(client_fd, buf, NULL, 5, &tv)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        close(fd_client);
        close(client_fd);
        common_tools.list_free(&data_list);
        return res;
    }
    if ((res = common_tools.recv_data(client_fd, buf, NULL, 5, &tv)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_data failed!", res);
        close(fd_client);
        close(client_fd);
        common_tools.list_free(&data_list);
        return res;
    }
    PRINT("buf[3] = %02X\n", (unsigned char)buf[3]);
    if (buf[3] != 0x00) // 没有
    {
        res = DATA_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        close(fd_client);
        close(client_fd);
        common_tools.list_free(&data_list);
        return res;
    }
    close(client_fd);
    
    PRINT("________________send stop cmd!\n");
    #else // 继电器  从SI32919切换到95R54
    if ((res = communication_serial.relay_change(0)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "relay_change failed!", res);
        close(fd_client);
        close(client_fd);
        common_tools.list_free(&data_list);
        return res;
    }
    #endif  // RELAY_CHANGE == 0
    
    // PSTN呼叫
    if ((res = communication_serial.cmd_call(common_tools.config->center_phone)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_call failed!", res);
        // 挂机（停止呼叫）
        if ((ret = communication_serial.cmd_on_hook()) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_on_hook failed!", ret);
        }
        goto EXIT;
    }
    PRINT("________________call\n");
    
    memset(buf, 0, sizeof(buf));
    // 0x2002平台呼叫应答
    if ((res = communication_network.msg_recv(fd_client, buf, sizeof(buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_recv failed!", res);
        common_tools.list_free(&data_list);
        // 挂机（停止呼叫）
        if ((ret = communication_serial.cmd_on_hook()) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_on_hook failed!", ret);
        }
        res = common_tools.get_errno('S', res);
        PRINT("________________hook\n");
        goto EXIT;
    }
    
    PRINT("________________0x2002\n");
    
    if ((ret = communication_serial.cmd_on_hook()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_on_hook failed!", ret);
        res = ret;
        goto EXIT;
    }
    // 由于挂机后，95R54会自动把继电器切到SI32919
    #if RELAY_CHANGE == 1 // 继电器  从SI32919切换到95R54
    if ((ret = communication_serial.relay_change(0)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "relay_change failed!", ret);
        res = ret;
        goto EXIT;
    }
    #endif  // RELAY_CHANGE == 1
    
    PRINT("________________hook\n");
    
    // 0x2002解包
    if ((res = communication_network.msg_unpack(buf, res, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_unpack failed!", res);
        common_tools.list_free(&data_list);
        res = common_tools.get_errno('S', res);
        goto EXIT;
    }
    
    // 0x2003接收呼叫请求打包
    dial_back_respond.cmd = 0x2003;
    if ((res = communication_network.msg_pack(&data_list, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_pack failed!", res);
        common_tools.list_free(&data_list);
        goto EXIT;
    }
    
    // 0x2003请求发送
    if ((res = communication_network.msg_send(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_send failed!", res);
        common_tools.list_free(&data_list);
        goto EXIT;
    }
    PRINT("________________0x2003\n");
    
    #if 1
    
    dial_back_respond.call_result = 0; // 呼叫状态
    PRINT("dial_back_respond.phone_num = %s\n", dial_back_respond.phone_num);
    // 接收来电
    if ((res = communication_serial.recv_display_msg(dial_back_respond.phone_num)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_display_msg failed!", res);
        dial_back_respond.call_result = 1; // 错误
        ret = res;
        //goto EXIT;
    }
    else 
    {
        PRINT("recv_display_msg ok\n");    
    }
    
    // 拒绝接听
    if ((res = communication_serial.refuse_to_answer()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "refuse_to_answer failed!", res);
        dial_back_respond.call_result = 1; // 错误
        // goto EXIT;
    }
    else
    {
        PRINT("refuse_to_answer ok\n");
    }
    
    // 由于挂机后，95R54会自动把继电器切到SI32919，所以不用手动切换到SI32919
    #endif
    
    // 0x2004呼叫结果请求打包
    dial_back_respond.cmd = 0x2004;
    if ((res = communication_network.msg_pack(&data_list, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_pack failed!", res);
        common_tools.list_free(&data_list);
        goto EXIT;
    }
    
    // 0x2004请求发送
    if ((res = communication_network.msg_send(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_send failed!", res);
        common_tools.list_free(&data_list);
        goto EXIT;
    }
    PRINT("________________0x2004\n");
    if (ret < 0)
    {
        res = ret;
        goto EXIT;
    }
    memset(buf, 0, sizeof(buf));
    // 0x2005颁发位置令牌应答
    if ((res = communication_network.msg_recv(fd_client, buf, sizeof(buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_recv failed!", res);        
        common_tools.list_free(&data_list);
        res = common_tools.get_errno('S', res);
        goto EXIT;
    }
    
    // 0x2005解包
    if ((res = communication_network.msg_unpack(buf, res, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_unpack failed!", res);
        common_tools.list_free(&data_list);
        res = common_tools.get_errno('S', res);
        goto EXIT;
    }
    common_tools.list_free(&data_list);
    memcpy(position_token, dial_back_respond.position_token, TOKENLEN);
    memcpy(terminal_authentication.static_position_token, dial_back_respond.position_token, TOKENLEN);
    PRINT("________________0x2005\n");
    
    PRINT_BUF_BY_HEX(dial_back_respond.position_token, NULL, TOKENLEN, __FILE__, __FUNCTION__, __LINE__);
    
    terminal_authentication.static_position_flag = 1;
    #if 0 // 由于方案修改，启动CACM在开机之后，由监控程序检测是否有必要启动
    // 启动CACM
    if ((res = start_up_CACM()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "start_up_CACM failed!", res);
        return res;
    }
    #endif

EXIT:
    #if 0 // 接收到电话号码
    if (res == )
    {
        
    }
    #endif
    
    if (fd_client != 0)
    {
        close(fd_client);
    }
    
    // 是否切换继电器
    #if RELAY_CHANGE == 0
    if ((ret = communication_network.make_client_link("127.0.0.1", SPI_UART1_MUTEX_SOCKET_PORT)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "make_client_link failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return ret;
    }
    client_fd = ret;
    
    memset(buf, 0, sizeof(buf));
    buf[0] = 0x5A;
    buf[1] = 0xA5;
    buf[2] = 0x01;
    buf[3] = 0x02;
    buf[4] = 0x02;
    
    // 发送
    if (common_tools.send_data(client_fd, buf, NULL, 5, NULL) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
    }
    sleep(1);
    close(client_fd);
    
    #else // 1 从95R54切换到SI32919
    if ((res = communication_serial.relay_change(1)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "relay_change failed!", res);
    }
    #endif // RELAY_CHANGE == 0
    return res;
}


/**
 * 启动CACM
 */
int start_up_CACM()
{
    int res = 0;
    char buf[256] = {0};
    char *cacm_cmd = "ps | grep cacm | sed '/grep/'d";
    char * const app_argv[] = {"cacm", NULL};
    
    if ((res = common_tools.get_cmd_out(cacm_cmd, buf, sizeof(buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_cmd_out failed!", res);
        return res;
    }
    
    buf[strlen(buf) - 1] = '\0';
    
    if (strlen(buf) != 0) 
    {
        system("kill -9 `ps | grep cacm | sed \'/grep/\'d | awk \'{print $1}\'`");
	}
    
    if ((res = common_tools.start_up_application("/bin/cacm", app_argv, 0)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "start_up_application failed!", res);
        return res;
    }
    return 0;
}
