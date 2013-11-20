#include "terminal_authentication.h"

#if CTSI_SECURITY_SCHEME == 1
/**
 * 插入令牌
 */
static int insert_token();

/**
 * 删除令牌
 */
static int delete_token();

/**
 * 更新令牌
 */
static int update_token();

/**
 * 初始化结构体
 */
struct class_terminal_authentication terminal_authentication = 
{
    insert_token, delete_token, update_token
};

/**
 * 初始化终端认证数据
 */
static int init_authentication_data(struct s_dial_back_respond *a_dial_back_respond)
{   
    PRINT_STEP("entry...\n");
    int res = 0;
    memset(a_dial_back_respond, 0, sizeof(struct s_dial_back_respond));	
    char columns_name[2][30] = {"base_sn", "pad_sn"};
    char columns_value[2][100] = {0};
    
    #if BOARDTYPE == 6410 || BOARDTYPE == 9344
    if ((res = database_management.select(2, columns_name, columns_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);
        return res;
    }
    #elif BOARDTYPE == 5350
    
    if ((res = nvram_interface.select(RT5350_FREE_SPACE, 2, columns_name, columns_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_seletc failed!", res);
        return res;
    }
    int i = 0;
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
    #endif
	memcpy(a_dial_back_respond->BASE_id, columns_value[0], strlen(columns_value[0]));
	memcpy(a_dial_back_respond->PAD_id, columns_value[1], strlen(columns_value[1]));
	
	srand((unsigned int)time(NULL));
	a_dial_back_respond->base_random = rand();
	PRINT("a_dial_back_respond->base_random = %08X\n", a_dial_back_respond->base_random);
	a_dial_back_respond->cmd = 0x2000;
	PRINT_STEP("exit...\n");
	return 0;
}

/**
 * 终端认证
 */
static int authentication(struct s_dial_back_respond *a_dial_back_respond)
{
    PRINT_STEP("entry...\n");
    int res = 0;
    int i = 0;
    int fd_client = 0;
    int serial_data_analyse_res = 0;
    int receive_data_res = 0;    
    struct s_data_list data_list;
    memset((void *)&data_list, 0, sizeof(struct s_data_list));
    memset((void *)(common_tools.deal_attr), 0, sizeof(struct s_deal_attr));
    memset((void *)(common_tools.timer), 0, sizeof(struct s_timer));
    common_tools.deal_attr->deal_type = 1;
    //线程id
    pthread_t pthread_read_data_id, pthread_analyse_data_id;
    #if 1
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "make_client_link start!", 0);
    #if 0
    for (i = 0; i < common_tools.config->repeat; i++)
    {
        if ((fd_client = internetwork_communication.make_client_link(common_tools.config->center_ip, common_tools.config->center_port)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "make_client_link failed!", fd_client);
            if (i != (common_tools.config->repeat - 1))
            {
                sleep(3);
                continue;    
            } 
        }
        break;
    }
    if (fd_client < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "make_client_link failed!", fd_client);
        return common_tools.get_errno('S', fd_client);
    }
    #else
    if ((fd_client = internetwork_communication.make_client_link(common_tools.config->center_ip, common_tools.config->center_port)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "make_client_link failed!", fd_client);
        return common_tools.get_errno('S', fd_client);     
    }
    #endif
    
    if ((res = internetwork_communication.msg_pack(&data_list, a_dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_pack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_pack ok!", 0);
    if ((res = internetwork_communication.msg_send(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_send failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_send ok!", 0);
    #if BOARDTYPE == 6410
    char buf[128] = {0};
    char out[128] = {0};
    sprintf(buf, "netstat -lan | grep %s:%s | grep CLOSE_WAIT", common_tools.config->center_ip, common_tools.config->center_port);
    PRINT("buf = %s\n", buf);
    if ((res = common_tools.get_cmd_out(buf, out, sizeof(out))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_cmd_out failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    PRINT("out = %s\n", out);
    if (strlen(out) != 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "link close wait!", WRONGFUL_DEV_ERR);  
        return WRONGFUL_DEV_ERR;
    } 
    #endif
    if ((res = internetwork_communication.msg_recv(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_recv failed!", res);        
        close(fd_client);
        common_tools.list_free(&data_list);
        #if BOARDTYPE == 5350
        if (res == S_READ_ERR)
        {
            return WRONGFUL_DEV_ERR;
        }
        #endif
        return res;
    }
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_recv ok!", 0);
    if ((res = internetwork_communication.msg_unpack(&data_list, a_dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_unpack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        if (a_dial_back_respond->failed_reason != NULL)
        {
            free(a_dial_back_respond->failed_reason);
            a_dial_back_respond->failed_reason = NULL;
        }
        return common_tools.get_errno('S', res);;
    }
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_unpack ok!", 0);
    close(fd_client);
    #endif
    #if BOARDTYPE == 5350
    // 告知PSTN停止接受串口1数据
    // 接受PSTN回复
    if ((res = communication_stc.recv_display_msg()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_display_msg failed!", res);
        // 如果因为来电错误时，把来电发送给PSTN
        // 如果不是因为来电错误时，// 发送开启接受串口1数据命令到PSTN
        // 接受PSTN回复
        return res;
    }
    common_tools.deal_attr->deal_state = 1;
    //摘机
    if ((res = communication_stc.cmd_off_hook()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_off_hook failed!", res);
        return res;
    }
    #endif
    #if 0
    if ((res = cmd_call(&data_list, "8089", "")) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_call failed!", res);
        common_tools.list_free(&data_list);
        return res;       
    }
    #endif
    //初始化互斥锁
    pthread_mutex_init(communication_stc.mutex, NULL);
    
    //初始化条件量 
    pthread_cond_init(communication_stc.cond, NULL);
    
    pthread_mutex_lock(communication_stc.mutex);

    //创建接收数据线程和分析数据线程
    if (pthread_create(&pthread_read_data_id, NULL, (void *)communication_stc.recv, NULL) != 0)
    {   
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "pthread_read_data_id pthread_create failed", PTHREAD_CREAT_ERR);
        res = PTHREAD_CREAT_ERR;
        pthread_mutex_unlock(communication_stc.mutex);
        goto destroy_receive_data;
    }
    
    if (pthread_create(&pthread_analyse_data_id, NULL, (void *)communication_stc.analyse, (void *)a_dial_back_respond) != 0)
    {
        common_tools.deal_attr->deal_over_bit = 1;     
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "pthread_analyse_data_id pthread_create failed", PTHREAD_CREAT_ERR);
        
        res = PTHREAD_CREAT_ERR;
        pthread_mutex_unlock(communication_stc.mutex);
        goto destroy_serial_data_analyse;
    }
    
    pthread_mutex_unlock(communication_stc.mutex);
    
    pthread_join(pthread_analyse_data_id, (void *)&serial_data_analyse_res);
destroy_serial_data_analyse:    
    pthread_join(pthread_read_data_id, (void *)&receive_data_res);
    
destroy_receive_data:
    usleep(10);
    // 互斥锁和条件量的释放
    pthread_mutex_destroy(communication_stc.mutex);
    pthread_cond_destroy(communication_stc.cond);
    
    if (((serial_data_analyse_res + receive_data_res) != 0) && (*(common_tools.phone_status_flag) == 1))  
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "analyse or recv failed", serial_data_analyse_res + receive_data_res);  
        if (communication_stc.cmd_on_hook() < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_on_hook failed", -1); 
        }
    }
    
    common_tools.list_free(&data_list);
    // 发送开启接受串口1数据命令到PSTN
    // 接受PSTN回复
    PRINT_STEP("exit...\n");
    return res + serial_data_analyse_res + receive_data_res; 
}

/**
 * 插入令牌
 */
int insert_token()
{
    PRINT_STEP("entry...\n");
    int res = 0;
	int result = 0;
	struct s_dial_back_respond dial_back_respond;    
    
	char print_info[128] = {0}; // 日志打印信息
	
    if ((res = init_authentication_data(&dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "init_authentication_data failed!", res);
        return res;
    }    
    #if BOARDTYPE == 6410
    if ((result = common_tools.set_GPIO4(TEL_DOWN)) < 0)  
    {  
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "set_GPIO4 failed!", result);
        return result;
    }
    sprintf(print_info, "set GPIO4 DOWN success! TEL_DOWN = %d\n", TEL_DOWN); 
 	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_info, 0);          
 	#endif	
 	
    // 创建录音线程
    #if RECORD_DEBUG
    pthread_t pthread_record_id;
    int record_res = 0;
    if (pthread_create(&pthread_record_id, NULL, (void *)communication_stc.record, NULL) != 0)
    {   
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "pthread_record pthread_create failed", PTHREAD_CREAT_ERR);
        return PTHREAD_CREAT_ERR;
    }
    #endif
    
    res = authentication(&dial_back_respond);
    
    #if RECORD_DEBUG
    // 停止录音
    *communication_stc.stop_record_flag = 1;
    pthread_join(pthread_record_id, (void *)&record_res);
    PRINT("record_res = %d\n", record_res);
    *communication_stc.stop_record_flag = 0;    
    #endif
    
    #if BOARDTYPE == 6410
    if ((result = common_tools.set_GPIO4(TEL_UP)) < 0)  
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "set_GPIO4 failed!", result);
        return result;
    }
    memset(print_info, 0, sizeof(print_info));
    sprintf(print_info, "set GPIO4 UP success! TEL_UP = %d\n", TEL_UP); 
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_info, 0);         
    #endif
    
    if (res == 0)
    {   
        memset(print_info, 0, sizeof(print_info));
        sprintf(print_info, "authentication success! BASE_id = %s; PAD_id = %s;  terminal_token = %s", dial_back_respond.BASE_id, dial_back_respond.PAD_id, dial_back_respond.terminal_token); 
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_info, 0);
        char column_name[1][30] = {"token"};
        
		char column_value[1][100] = {0};
		memcpy(column_value, dial_back_respond.terminal_token, strlen(dial_back_respond.terminal_token));
        unsigned short value_len = dial_back_respond.terminal_token_len;
        PRINT("dial_back_respond.terminal_token = %02X\n", dial_back_respond.terminal_token);
        PRINT("dial_back_respond.terminal_token_len = %02X\n", &(dial_back_respond.terminal_token_len));
        PRINT("column_name = %02X\n", column_name);
        
        #if BOARDTYPE == 6410  || BOARDTYPE == 9344
        if ((res = database_management.insert(1, column_name, (char (*)[100])&(dial_back_respond.terminal_token), (unsigned short *)&dial_back_respond.terminal_token_len)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_insert failed!", res);
            return res;
        }
        #elif BOARDTYPE == 5350
        usleep(500000);
        //if ((res = nvram_interface.insert(RT5350_FREE_SPACE, 1, column_name, (char (*)[100])&(dial_back_respond.terminal_token), (unsigned short *)&dial_back_respond.terminal_token_len)) < 0)
        if ((res = nvram_interface.insert(RT5350_FREE_SPACE, 1, column_name, column_value, &value_len)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_insert failed!", res);
            return res;
        }
        
        //PRINT("dial_back_respond.terminal_token = %s\n", dial_back_respond.terminal_token);
        
        #endif
    }
    else if (res < 0)
    {
        memset(print_info, 0, sizeof(print_info));
        if (res == LOGIN_FAILED)
        {        
            sprintf(print_info, "authentication failed! res = %d failed_reason = %s", res, dial_back_respond.failed_reason);
        }
        else
        {
            sprintf(print_info, "authentication failed! res = %d", res);
        }
                    
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_info, 0);
        return res;
    }
    PRINT_STEP("exit...\n");
    PRINT("________________\n");
    return 0;
}


/**
 * 删除令牌
 */
int delete_token()
{
    PRINT_STEP("entry...\n");
    int res = 0;
    char *column_name = "token";
    #if BOARDTYPE == 6410  || BOARDTYPE == 9344
    if ((res = database_management.del(1, (char(*)[30])&column_name)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_clear failed!", res);
        return res;
    }
    #elif BOARDTYPE == 5350
    if ((res = nvram_interface.del(RT5350_FREE_SPACE, 1, (char(*)[30])&column_name)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_del failed!", res);
        return res;
    }
    
    #endif
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 更新令牌
 */
int update_token()
{
    PRINT_STEP("entry...\n");
    int res = 0;
    if ((res = delete_token()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "delete_token failed!", res);
        return res;
    }
    
    if ((res = insert_token()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "insert_token failed!", res);
        return res;
    }
    PRINT_STEP("exit...\n");
    return 0;
}
#else
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
    return_device_token, return_position_token, rebuild_device_token, rebuild_position_token, start_up_CACM
};


/**
 * 获取本地存储的设备令牌并返回
 */
int return_device_token(char *device_token)
{
    int res = 0;
    char column_name[1][30] = {"device_token"};
    char column_value[1][100] = {0};
    
    if (device_token == NULL)
    {
        res = NULL_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "device_token is NULL!", res);
        return res;
    }
    #if BOARDTYPE == 5350
    if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, column_name, column_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);
        return res;
    }
    #elif BOARDTYPE == 9344
    if ((res = database_management.select(1, column_name, column_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);
        return res;
    }
    #endif
    if ((strcmp("\"\"", column_value[0]) == 0) || (strlen(column_value[0]) == 0))
    {
        memset(column_value[0], 0, sizeof(column_value[0]));
        sprintf(column_value[0], "There is no (%s) record!", column_name[0]);
        res = NULL_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, column_value[0], res); 
        return res;
    }
    memcpy(device_token, column_value[0], strlen(column_value[0]));
    return 0;
}

/**
 * 获取本地存储的位置令牌并返回
 */
int return_position_token(char *position_token)
{
    int res = 0;
    char column_name[1][30] = {"position_token"};
    char column_value[1][100] = {0};
    if (position_token == NULL)
    {
        res = NULL_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "position_token is NULL!", res);
        return res;
    }
    
    #if BOARDTYPE == 5350
    if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, column_name, column_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);
        return res;
    }
    #elif BOARDTYPE == 9344
    if ((res = database_management.select(1, column_name, column_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);
        return res;
    }
    #endif
    if ((strcmp("\"\"", column_value[0]) == 0) || (strlen(column_value[0]) == 0))
    {
        memset(column_value[0], 0, sizeof(column_value[0]));
        if ((res = rebuild_position_token(column_value[0])) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "rebuild_position_token failed!", res);
            return res;
        }
    }
    
    memcpy(position_token, column_value[0], strlen(column_value[0]));
    return 0;
}
#if 0
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
    
    if (device_token == NULL)
    {
        res = NULL_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "device_token is NULL!", res);
        return res;
    }
    
    // 连接服务器
    if ((fd_client = internetwork_communication.make_client_link(common_tools.config->center_ip, common_tools.config->center_port)) < 0)
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
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_seletc failed!", res);
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
    if ((res = internetwork_communication.msg_pack(&data_list, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_pack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 0x1000请求发送
    if ((res = internetwork_communication.msg_send(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_send failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 0x1001应答
    if ((res = internetwork_communication.msg_recv(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_recv failed!", res);        
        close(fd_client);
        common_tools.list_free(&data_list);
        if (res == S_READ_ERR)
        {
            return WRONGFUL_DEV_ERR;
        }
        return res;
    }
    
    // 0x1001解包
    if ((res = internetwork_communication.msg_unpack(&data_list, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_unpack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return common_tools.get_errno('S', res);;
    }
    
    // 0x1002请求打包（确认身份）
    dial_back_respond.cmd = 0x1002;
    if ((res = internetwork_communication.msg_pack(&data_list, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_pack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 0x1002请求发送
    if ((res = internetwork_communication.msg_send(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_send failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 0x1003应答（颁发设备令牌）
    if ((res = internetwork_communication.msg_recv(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_recv failed!", res);        
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 0x1003解包
    if ((res = internetwork_communication.msg_unpack(&data_list, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_unpack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return common_tools.get_errno('S', res);;
    }
    common_tools.list_free(&data_list);
    
    memcpy(device_token, dial_back_respond.device_token, strlen(dial_back_respond.device_token));
    close(fd_client);
    
    // 把获取的设备令牌保存到本地
    memset(columns_name[0], 0, sizeof(columns_name[0]));
    memset(columns_value[0], 0, sizeof(columns_value[0]));
    
    memcpy(columns_name[0], "device_token", strlen("device_token"));
    memcpy(columns_value[0], dial_back_respond.device_token, strlen(dial_back_respond.device_token));
    unsigned short value_len = strlen(dial_back_respond.device_token);
    
    PRINT("dial_back_respond.device_token = %s\n", dial_back_respond.device_token);
    
    #if BOARDTYPE == 5350
    if ((res = nvram_interface.insert(RT5350_FREE_SPACE, 1, columns_name, columns_value, &value_len)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_insert failed!", res);
        return res;
    }
    #elif BOARDTYPE == 9344
    if ((res = database_management.insert(1, columns_name, columns_value, &value_len)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_insert failed!", res);
        return res;
    }
    #endif
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
    unsigned long base_random = 0;
    struct s_data_list data_list;
    struct s_dial_back_respond dial_back_respond;
    memset((void *)&data_list, 0, sizeof(struct s_data_list));
    memset((void *)&dial_back_respond, 0, sizeof(struct s_dial_back_respond));
    
    if (position_token == NULL)
    {
        res = NULL_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "position_token is NULL!", res);
        return res;
    }
    
    // 连接服务器
    if ((fd_client = internetwork_communication.make_client_link(common_tools.config->center_ip, common_tools.config->center_port)) < 0)
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
	
	srand((unsigned int)time(NULL));
	base_random = rand();
	memcpy(dial_back_respond.base_random, (void *)&base_random, sizeof(base_random));
	base_random = rand();
	memcpy(dial_back_respond.base_random + sizeof(base_random), (void *)&base_random, sizeof(base_random));
	
	
    // 0x2000请求打包
    dial_back_respond.cmd = 0x2000;
    if ((res = internetwork_communication.msg_pack(&data_list, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_pack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 0x2000请求发送
    if ((res = internetwork_communication.msg_send(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_send failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 0x2001终端呼叫应答
    if ((res = internetwork_communication.msg_recv(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_recv failed!", res);        
        close(fd_client);
        common_tools.list_free(&data_list);
        #if BOARDTYPE == 5350
        if (res == S_READ_ERR)
        {
            return WRONGFUL_DEV_ERR;
        }
        #endif
        return res;
    }
    
    // 0x2001解包
    if ((res = internetwork_communication.msg_unpack(&data_list, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_unpack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return common_tools.get_errno('S', res);;
    }
    
    // PSTN呼叫
    if ((res = communication_stc.cmd_call(common_tools.config->center_phone)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_call failed!", res);
        return res;
    }
    
    // 0x2002平台呼叫应答
    if ((res = internetwork_communication.msg_recv(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_recv failed!", res);        
        close(fd_client);
        common_tools.list_free(&data_list);
        #if BOARDTYPE == 5350
        if (res == S_READ_ERR)
        {
            return WRONGFUL_DEV_ERR;
        }
        #endif
        return res;
    }
    
    // 挂机（停止呼叫）
    if ((res = communication_stc.cmd_on_hook()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_on_hook failed!", res);
        return res;
    }
    
    // 0x2002解包
    if ((res = internetwork_communication.msg_unpack(&data_list, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_unpack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return common_tools.get_errno('S', res);;
    }
    
    // 0x2003接收呼叫请求打包
    dial_back_respond.cmd = 0x2003;
    if ((res = internetwork_communication.msg_pack(&data_list, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_pack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 0x2003请求发送
    if ((res = internetwork_communication.msg_send(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_send failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 接收来电
    if ((res = communication_stc.recv_display_msg(dial_back_respond.phone_num)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_display_msg failed!", res);
        return res;
    }
    
    // 0x2004呼叫结果请求打包
    dial_back_respond.cmd = 0x2004;
    if ((res = internetwork_communication.msg_pack(&data_list, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_pack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 0x2004请求发送
    if ((res = internetwork_communication.msg_send(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_send failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 0x2005颁发位置令牌应答
    if ((res = internetwork_communication.msg_recv(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_recv failed!", res);        
        close(fd_client);
        common_tools.list_free(&data_list);
        #if BOARDTYPE == 5350
        if (res == S_READ_ERR)
        {
            return WRONGFUL_DEV_ERR;
        }
        #endif
        return res;
    }
    
    // 0x2005解包
    if ((res = internetwork_communication.msg_unpack(&data_list, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_unpack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return common_tools.get_errno('S', res);;
    }
    common_tools.list_free(&data_list);
    memcpy(position_token, dial_back_respond.position_token, strlen(dial_back_respond.position_token));
    close(fd_client);
    
    // 把获取的位置令牌保存到本地
    memset(columns_name[0], 0, sizeof(columns_name[0]));
    memset(columns_value[0], 0, sizeof(columns_value[0]));
    
    memcpy(columns_name, "position_token", strlen("position_token"));
    memcpy(columns_value[0], dial_back_respond.device_token, strlen(dial_back_respond.device_token));
    unsigned short value_len = strlen(dial_back_respond.device_token);
    
    PRINT("dial_back_respond.device_token = %s\n", dial_back_respond.device_token);
    
    #if BOARDTYPE == 5350
    if ((res = nvram_interface.insert(RT5350_FREE_SPACE, 1, columns_name, columns_value, &value_len)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_insert failed!", res);
        return res;
    }
    #elif BOARDTYPE == 9344
    if ((res = database_management.insert(1, columns_name, columns_value, &value_len)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_insert failed!", res);
        return res;
    }
    #endif
    
    #if 0 // 由于方案修改，启动CACM在开机之后，由监控程序检测是否有必要启动
    // 启动CACM
    if ((res = start_up_CACM()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "start_up_CACM failed!", res);
        return res;
    }
    #endif
    return 0;
}
#else

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
    if ((fd_client = internetwork_communication.make_client_link(common_tools.config->center_ip, common_tools.config->center_port)) < 0)
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
	
	srand((unsigned int)time(NULL));
	base_random = rand();
	memcpy(dial_back_respond.base_random, (void *)&base_random, sizeof(base_random));
	base_random = rand();
	memcpy(dial_back_respond.base_random + sizeof(base_random), (void *)&base_random, sizeof(base_random));
	
	
    // 0x1000请求打包
    dial_back_respond.cmd = 0x1000;
    if ((res = internetwork_communication.msg_pack2(&data_list, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_pack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 0x1000请求发送
    if ((res = internetwork_communication.msg_send(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_send failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 0x1001应答
    if ((res = internetwork_communication.msg_recv2(fd_client, buf, sizeof(buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_recv failed!", res);        
        close(fd_client);
        if (res == S_READ_ERR)
        {
            return WRONGFUL_DEV_ERR;
        }
        return res;
    }
    
    // 0x1001解包
    if ((res = internetwork_communication.msg_unpack2(buf, res, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_unpack failed!", res);
        close(fd_client);
        return common_tools.get_errno('S', res);;
    }
    PRINT("XXXXXXXXXXXXXXXXX\n");
    // 0x1002请求打包（确认身份）
    dial_back_respond.cmd = 0x1002;
    if ((res = internetwork_communication.msg_pack2(&data_list, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_pack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    PRINT("XXXXXXXXXXXXXXXXX\n");
    // 0x1002请求发送
    if ((res = internetwork_communication.msg_send(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_send failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    memset(buf, 0, sizeof(buf));
    // 0x1003应答（颁发设备令牌）
    if ((res = internetwork_communication.msg_recv2(fd_client, buf, sizeof(buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_recv failed!", res);        
        close(fd_client);
        return res;
    }
    PRINT("XXXXXXXXXXXXXXXXX\n");
    // 0x1003解包
    if ((res = internetwork_communication.msg_unpack2(buf, res, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_unpack failed!", res);
        close(fd_client);
        return common_tools.get_errno('S', res);;
    }
    
    memcpy(device_token, dial_back_respond.device_token, strlen(dial_back_respond.device_token));
    close(fd_client);
    
    // 把获取的设备令牌保存到本地
    memset(columns_name[0], 0, sizeof(columns_name[0]));
    memset(columns_value[0], 0, sizeof(columns_value[0]));
    
    memcpy(columns_name[0], "device_token", strlen("device_token"));
    memcpy(columns_value[0], dial_back_respond.device_token, strlen(dial_back_respond.device_token));
    unsigned short value_len = strlen(dial_back_respond.device_token);
    
    PRINT("dial_back_respond.device_token = %s\n", dial_back_respond.device_token);
    
    #if BOARDTYPE == 5350
    if ((res = nvram_interface.insert(RT5350_FREE_SPACE, 1, columns_name, columns_value, &value_len)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_insert failed!", res);
        return res;
    }
    #elif BOARDTYPE == 9344
    if ((res = database_management.insert(1, columns_name, columns_value, &value_len)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_insert failed!", res);
        return res;
    }
    #endif
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
    unsigned long base_random = 0;
    struct s_data_list data_list;
    struct s_dial_back_respond dial_back_respond;
    memset((void *)&data_list, 0, sizeof(struct s_data_list));
    memset((void *)&dial_back_respond, 0, sizeof(struct s_dial_back_respond));
    char buf[128] = {0};
    
    if (position_token == NULL)
    {
        res = NULL_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "position_token is NULL!", res);
        return res;
    }
    
    // 连接服务器
    if ((fd_client = internetwork_communication.make_client_link(common_tools.config->center_ip, common_tools.config->center_port)) < 0)
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
	
	srand((unsigned int)time(NULL));
	base_random = rand();
	memcpy(dial_back_respond.base_random, (void *)&base_random, sizeof(base_random));
	base_random = rand();
	memcpy(dial_back_respond.base_random + sizeof(base_random), (void *)&base_random, sizeof(base_random));
	
	
    // 0x2000请求打包
    dial_back_respond.cmd = 0x2000;
    if ((res = internetwork_communication.msg_pack2(&data_list, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_pack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 0x2000请求发送
    if ((res = internetwork_communication.msg_send(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_send failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 0x2001终端呼叫应答
    if ((res = internetwork_communication.msg_recv2(fd_client, buf, sizeof(buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_recv failed!", res);        
        close(fd_client);
        common_tools.list_free(&data_list);
        #if BOARDTYPE == 5350
        if (res == S_READ_ERR)
        {
            return WRONGFUL_DEV_ERR;
        }
        #endif
        return res;
    }
    
    // 0x2001解包
    if ((res = internetwork_communication.msg_unpack2(buf, res, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_unpack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return common_tools.get_errno('S', res);;
    }
    
    // PSTN呼叫
    if ((res = communication_stc.cmd_call(common_tools.config->center_phone)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_call failed!", res);
        return res;
    }
    
    memset(buf, 0, sizeof(buf));
    // 0x2002平台呼叫应答
    if ((res = internetwork_communication.msg_recv2(fd_client, buf, sizeof(buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_recv failed!", res);        
        close(fd_client);
        common_tools.list_free(&data_list);
        #if BOARDTYPE == 5350
        if (res == S_READ_ERR)
        {
            return WRONGFUL_DEV_ERR;
        }
        #endif
        return res;
    }
    
    // 挂机（停止呼叫）
    if ((res = communication_stc.cmd_on_hook()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_on_hook failed!", res);
        return res;
    }
    
    // 0x2002解包
    if ((res = internetwork_communication.msg_unpack2(buf, res, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_unpack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return common_tools.get_errno('S', res);;
    }
    
    // 0x2003接收呼叫请求打包
    dial_back_respond.cmd = 0x2003;
    if ((res = internetwork_communication.msg_pack2(&data_list, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_pack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 0x2003请求发送
    if ((res = internetwork_communication.msg_send(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_send failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 接收来电
    if ((res = communication_stc.recv_display_msg(dial_back_respond.phone_num)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_display_msg failed!", res);
        return res;
    }
    
    // 0x2004呼叫结果请求打包
    dial_back_respond.cmd = 0x2004;
    if ((res = internetwork_communication.msg_pack2(&data_list, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_pack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    // 0x2004请求发送
    if ((res = internetwork_communication.msg_send(fd_client, &data_list)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_send failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return res;
    }
    
    memset(buf, 0, sizeof(buf));
    // 0x2005颁发位置令牌应答
    if ((res = internetwork_communication.msg_recv2(fd_client, buf, sizeof(buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_recv failed!", res);        
        close(fd_client);
        common_tools.list_free(&data_list);
        #if BOARDTYPE == 5350
        if (res == S_READ_ERR)
        {
            return WRONGFUL_DEV_ERR;
        }
        #endif
        return res;
    }
    
    // 0x2005解包
    if ((res = internetwork_communication.msg_unpack2(buf, res, &dial_back_respond)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "msg_unpack failed!", res);
        close(fd_client);
        common_tools.list_free(&data_list);
        return common_tools.get_errno('S', res);;
    }
    common_tools.list_free(&data_list);
    memcpy(position_token, dial_back_respond.position_token, strlen(dial_back_respond.position_token));
    close(fd_client);
    
    // 把获取的位置令牌保存到本地
    memset(columns_name[0], 0, sizeof(columns_name[0]));
    memset(columns_value[0], 0, sizeof(columns_value[0]));
    
    memcpy(columns_name, "position_token", strlen("position_token"));
    memcpy(columns_value[0], dial_back_respond.device_token, strlen(dial_back_respond.device_token));
    unsigned short value_len = strlen(dial_back_respond.device_token);
    
    PRINT("dial_back_respond.device_token = %s\n", dial_back_respond.device_token);
    
    #if BOARDTYPE == 5350
    if ((res = nvram_interface.insert(RT5350_FREE_SPACE, 1, columns_name, columns_value, &value_len)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_insert failed!", res);
        return res;
    }
    #elif BOARDTYPE == 9344
    if ((res = database_management.insert(1, columns_name, columns_value, &value_len)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_insert failed!", res);
        return res;
    }
    #endif
    
    #if 0 // 由于方案修改，启动CACM在开机之后，由监控程序检测是否有必要启动
    // 启动CACM
    if ((res = start_up_CACM()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "start_up_CACM failed!", res);
        return res;
    }
    #endif
    return 0;
}
#endif


/**
 * 启动CACM
 */
int start_up_CACM()
{
    int res = 0;
    char buf[256] = {0};
    char *linphone_cmd = "ps | grep linphone | sed '/grep/'d";
    char * const app_argv[] = {"linphone", NULL};
    
    if ((res = common_tools.get_cmd_out(linphone_cmd, buf, sizeof(buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_cmd_out failed!", res);
        return res;
    }
    
    buf[strlen(buf) - 1] = '\0';
    
    if (strlen(buf) != 0) 
    {
        system("kill -9 `ps | grep linphonec | sed \'/grep/\'d | awk \'{print $1}\'`");
	}
    
    if ((res = common_tools.start_up_application("/bin/linphone", app_argv, 1)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "start_up_application failed!", res);
        return res;
    }
    return 0;
}
#endif
