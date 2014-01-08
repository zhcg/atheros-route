/*************************************************************************
    > File Name: terminal_dev_register.c
    > Author: yangjilong
    > mail1: yangjilong65@163.com
    > mail2: yang.jilong@handaer.com
    > Created Time: 2013年11月08日 星期五 09时04分26秒
**************************************************************************/
#include "network_config.h"
#include "spi_rt_interface.h"
#include "terminal_register.h"

#if BOARDTYPE == 6410 || BOARDTYPE == 9344
#include "database_management.h"
#elif BOARDTYPE == 5350 
#include "nvram_interface.h"
#endif

#include "communication_network.h"
#include "terminal_authentication.h"

static pthread_mutex_t config_mutex;
static time_t start_time;
static int cmd_handle_fd = 0;
static int cmd_handle_cmd_count = 0;
static char cmd_handle_cmd_word = 0;

struct s_terminal_dev_register
{
    int fd;
    int cmd_count; // 接收命令的个数
    pthread_mutex_t mutex; // 线程创建锁
    pthread_cond_t cond;   // 线程创建条件量
    pthread_t pad_cmd_handle_id; // 配置线程id
    
    volatile char cmd_word; // 命令字
    volatile char config_now_flag; // 正在配置标准 1：代表正在配置
    volatile char transmission_mode; // 交易模式 0代表网口 1 代表串口
    volatile char mutex_lock_flag; // 加锁标志位   
};

/**
 * 命令行分析
 */
static int analyse_command_line(int argc, char ** argv)
{
    int i = 0;
    int res = 0;
    
    // 终端初始化参数
    char columns_name[31][30] = 
    { 
        "STEP_LOG", "SERIAL_DATA_LOG", "OLD_ROUTE_CONFIG", "PPPOE_STATE_FILE", "SPI_PARA_FILE", "WAN_STATE_FILE", 
        "SERIAL_STC", "SERIAL_PAD", "SERIAL_STC_BAUD", "SERIAL_PAD_BAUD", 
        "CENTERPHONE", "CENTERIP", "CENTERPORT", "BASE_IP",
        "PAD_IP", "PAD_SERVER_PORT", "PAD_SERVER_PORT2", "PAD_CLIENT_PORT", 
        "TOTALTIMEOUT", "ONE_BYTE_TIMEOUT_SEC",
        "ONE_BYTE_TIMEOUT_USEC", "ONE_BYTE_DELAY_USEC", 
        "ROUTE_REBOOT_TIME_SEC", "REPEAT", 
        "TERMINAL_SERVER_IP", "TERMINAL_SERVER_PORT", "LOCAL_SERVER_PORT", "WAN_CHECK_NAME", 
        "default_ssid", "default_ssid_password", "register_state"
    };
    
    if (memcmp(argv[1], "-v", strlen("-v")) == 0)
    {
        PRINT("author %s\n", TERMINAL_AUTHOR);
        PRINT("version %s\n", TERMINAL_VERSION);
        PRINT("%s %s\n", __DATE__, __TIME__);
        PRINT("%s\n", TERMINAL_REMARKS);
    }
    else if (memcmp(argv[1], "-c", strlen("-c")) == 0) // 情况数据
    {
        #if BOARDTYPE == 6410 || BOARDTYPE == 9344
        if ((res = database_management.clear()) < 0)
        {
            PRINT("sqlite3_clear_table failed!\n");
            return res;
        }
        #elif BOARDTYPE == 5350
        if ((res = nvram_interface.clear()) < 0)
        {
            PRINT("nvram_clear_all_data failed!\n");
            return res;
        }
        #endif
        PRINT("data clear success!\n");
    }
    #if BOARDTYPE == 5350
    else if (memcmp(argv[1], "-i", strlen("-i")) == 0) // 填充配置文件
    {
        unsigned short values_len[31] = {0};
        char columns_values[31][100] = 
        { 
            "/var/terminal_init/log/operation_steps/", 
            "/var/terminal_init/log/serial_date_log/", 
            "/var/terminal_init/log/old_route_config", 
            "/var/terminal_init/log/pppoe_state", 
            "/var/terminal_init/log/spi_para_file", "/dev/gpio", "/dev/ttyS1", "/dev/ttyS0",
            "9600", "9600", "62916698", "192.168.0.141", "15005", "10.10.10.101",
            "10.10.10.100", "7789", "7799", "7788", "10", "5",
            "0", "25", "40", "3", "192.168.0.120", "7000",
            "13435", "www.baidu.com", "handaer_wifi_register", "12345678", "251",
        };
        for (i = 0; i < sizeof(values_len) / sizeof(values_len[0]); i++)
        {
            values_len[i] = strlen(columns_values[i]);
        }
            
        if ((res = nvram_interface.insert(RT5350_FREE_SPACE, sizeof(values_len)/sizeof(unsigned short), columns_name, columns_values, values_len)) < 0)
        {
            PRINT("nvram_insert failed!\n");
            return res;
        }
        PRINT("make config file success!\n");
    }
    else if (memcmp(argv[1], "-u", strlen("-u")) == 0) // 修改特定的配置项
    {
        if (argc != 4)
        {
            printf("ex: cmd -u column_name column_value\n");
            return DATA_ERR;
        }
        
        for (i = 0; i < sizeof(columns_name) / sizeof(columns_name[0]); i++)
        {
            if (memcmp(argv[2], columns_name[i], strlen(columns_name[i])) == 0)
            {
                break;
            }
        }
        if (i == i < sizeof(columns_name) / sizeof(columns_name[0]))
        {
            PRINT("column_name error!\n");
            return DATA_ERR;
        }
        unsigned short value_len = strlen(argv[3]);
        if ((res = nvram_interface.update(RT5350_FREE_SPACE, 1, (char (*)[30])argv[2], (char (*)[100])argv[3], &value_len)) < 0)
        {
            PRINT("nvram_update failed!\n");
            return res;
        }
        PRINT("update success!\n");
    }
    else if (memcmp(argv[1], "-t", strlen("-t")) == 0) // pad串口测试
    {
        if (argc != 3)
        {
            printf("ex: cmd -t send count\n");
            return DATA_ERR;
        }
        
        int j = 0;
        int count = atoi(argv[2]);
        struct timeval tv = {1, 0};        
        char send_buf[128] = {0};
        char recv_buf[128] = {0};
        
        for (j = 0; j < 128; j++)
        {
            send_buf[j] = 0xF0;
        }
            
        for (i = 0; i < count; i++)
        {
            #if BOARDTYPE == 5350 || BOARDTYPE == 6410
            if ((res = common_tools.recv_data(*network_config.serial_pad_fd, recv_buf, NULL, sizeof(recv_buf), &tv)) < 0)
            #elif BOARDTYPE == 9344
            if ((res = common_tools.recv_data(usb_client_fd, recv_buf, NULL, sizeof(recv_buf), &tv)) < 0)
            #endif    
            {
                PRINT("recv_data failed!\n");
                return res;
            }
            
            tv.tv_sec = 1;
            #if BOARDTYPE == 5350 || BOARDTYPE == 6410
            if ((res = common_tools.send_data(*network_config.serial_pad_fd, send_buf, NULL, sizeof(send_buf), &tv)) < 0)
            #elif BOARDTYPE == 9344
            if ((res = common_tools.send_data(usb_client_fd, send_buf, NULL, sizeof(send_buf), &tv)) < 0)
            #endif
            {
                PRINT("send_data failed!\n");
                return res;
            }
            PRINT("success!\n");
        }
        return res;
    }
    #endif
    else if (memcmp(argv[1], "-h", strlen("-h")) == 0)
    {
        PRINT("terminal_init %s (%s %s)\n", TERMINAL_VERSION, __DATE__, __TIME__);
        PRINT("usage1: terminal_init [option]\n");
        PRINT("usage2: terminal_init [option] [column_name column_value]\n");
        PRINT("options:\n");
        PRINT("\t-c: 清空数据表\n");
        PRINT("\t-h: 帮助\n");
        PRINT("\t-i: 配置文件制作\n");
        //PRINT("\t-t: pad串口测试\n");
        PRINT("\t-u: 修改特定的配置项\n");
        PRINT("\t-v: 查看应用程序版本\n");
        return 0;
    }
    return 0;
}

/**
 * 信号处理函数
 */
static void signal_handle(int sig)
{
    PRINT("signal_handle entry!\n");
    int res = 0;
    switch (sig)
    {
        case 1:
        {
            PRINT("SIGHUP sig no:%d; sig info:挂起\n", sig);
            break;
        }
        case 2:
        {
            PRINT("SIGINT sig no:%d; sig info:中断(ctrl + c)\n", sig);
            break;
        }
        case 3:
        {
            PRINT("SIGQUIT sig no:%d; sig info:退出\n", sig);
            break;
        }
        case 6:
        {
            PRINT("SIGABRT sig no:%d; sig info:中止\n", sig);
            break;
        }
        case 10:
        {
            PRINT("SIGBUS sig no:%d; sig info:总线错误\n", sig);
            break;
        }
        case 13:
        {
            PRINT("SIGPIPE sig no:%d; sig info:管道破裂-写一个没有读端口的管道\n", sig);
            break;
        }
        case 14:
        {
            PRINT("SIGALRM sig no:%d; sig info:闹钟 当某进程希望在某时间后接收信号时发此信号\n", sig);
            break;
        }
        case 15:
        {
            PRINT("SIGTERM sig no:%d; sig info:软件终止\n", sig);
            break;
        }
        case 17:
        {
            pid_t pid;
            while ((pid = waitpid(-1, &res, WNOHANG)) > 0)
			{
				PRINT("pid = %d, state = %d\n", pid, res);
			}
            PRINT("SIGCHLD sig no:%d; sig info:子进程停止信号\n", sig);
            break;
        }
        case 27:
        case 29:
        {
            PRINT("SIGPROF sig no:%d; sig info:定时器到\n", sig);
            break;
        }
        case 26:
        case 28:
        {
            PRINT("SIGVTALRM sig no:%d; sig info:实际时间报警时钟信号\n", sig);
            break;
        }
        case 18:
        {
            pid_t pid;
            while ((pid = waitpid(-1, &res, WNOHANG)) > 0)
			{
				PRINT("pid = %d, state = %d\n", pid, res);
			}
            PRINT("SIGCLD sig no:%d; sig info:子进程结束信号\n", sig);
            break;
        }
        case 30:
        {
            PRINT("SIGPWR sig no:%d; sig info:电源故障\n", sig);
            break;
        }
        default:
        {
            PRINT("sig no:%d; sig info:其他\n", sig);
            break;
        }
    }
    
    PRINT("signal_handle exit!\n");
}

/**
 * 设置处理线程  
 */
void * pad_cmd_handle(void* para) 
{
    PRINT("pad_cmd_handle entry!\n");
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);           //允许退出线程    
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,   NULL);   //设置立即取消 
    
    int i = 0;
    int res = 0, ret = 0;
    struct s_terminal_dev_register * terminal_dev_register = (struct s_terminal_dev_register *)para;
    
    int fd = terminal_dev_register->fd;
    int len = 0;
    unsigned short insert_len = 0;
    char *buf = NULL;
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
    // 连接PAD端服务器
    for (i = 0; i < common_tools.config->repeat; i++)
    {
        if ((*network_config.server_pad_fd = communication_network.make_client_link(common_tools.config->pad_ip, common_tools.config->pad_server_port)) < 0)
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
        ret = common_tools.get_errno('P', *network_config.server_pad_fd);
    }
    else 
    {
        #if BOARDTYPE == 5350
        // 网口时关闭
        if (terminal_dev_register->transmission_mode == 0)
        {
            PRINT("fd = %d\n", fd);
            close(fd);
        }
        #elif BOARDTYPE == 9344
        close(fd);
        #endif
        fd = *network_config.server_pad_fd;
    }
    
    if ((res == 0) && (ret == 0))
    {
        #if USER_REGISTER   
        len = strlen("network_config.base_sn:") + strlen("network_config.base_mac:") + strlen("network_config.base_ip:") + 
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
        sprintf(buf, "network_config.base_sn:%s,network_config.base_mac:%s,network_config.base_ip:%s,%s,%s,%s,%s,%s,%s,%s,%s,", network_config.base_sn, network_config.base_mac, network_config.base_ip, 
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
        sprintf(buf, "base_sn:%s,base_mac:%s,base_ip:%s,", columns_value[0], columns_value[1], columns_value[2]); 
        #endif

        PRINT("%s\n", buf);
        
        // 正确信息发送到PAD
        if ((res = network_config.send_msg_to_pad(fd, 0x00, buf, strlen(buf))) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
            goto EXIT;
        }
        
        #if 1 // 启动CACM
        if ((res = terminal_authentication.start_up_CACM()) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "start_up_CACM failed!", res);
            goto EXIT;
        }
        #endif
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
        if ((res = network_config.send_msg_to_pad(fd, 0x00, NULL, 0)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed!", res);  
        }
    }
    
    if ((unsigned char)*network_config.cmd != 0xFF) // *network_config.cmd  等于如下值时 0x00 0xFC 0xFB 0xFD 0xFE，插入数据库
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
            if ((ret = database_management.insert(1, columns_name, columns_value, &insert_len)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_insert failed!", ret);
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
    #if BOARDTYPE == 5350
    // 网口时关闭
    if (terminal_dev_register->transmission_mode == 0)
    {
        PRINT("fd = %d\n", fd);
        close(fd);
    }
    else // 串口时
    {
        terminal_dev_register->transmission_mode = 0;
    }
    #elif BOARDTYPE == 9344
    close(fd);
    #endif
    network_config.init_cmd_list();  // 初始化命令结构体
    return (void *)res;
}

/**
 * 管理线程  创建和终止线程
 */
void * pthread_manage(void* para)
{
    int res = 0;
    int i = 0;
    PRINT("pthread_manage entry!\n");
    struct s_terminal_dev_register * terminal_dev_register = (struct s_terminal_dev_register *)para;
    PRINT("after !\n");
    
    while (1)
    {
        PRINT("pthread_manage lock!\n");
        if (pthread_mutex_lock(&terminal_dev_register->mutex) < 0)
        {
            PERROR("pthread_mutex_lock failed!\n");
            res = PTHREAD_LOCK_ERR;
            continue;
        }
        PRINT("before pthread_cond_wait!\n");
        // 条件量接收
        pthread_cond_wait(&terminal_dev_register->cond, &terminal_dev_register->mutex);
        
        if (pthread_mutex_unlock(&terminal_dev_register->mutex) < 0)
        {
            PERROR("pthread_mutex_lock failed!\n");
            res = PTHREAD_UNLOCK_ERR;
            pthread_mutex_unlock(&terminal_dev_register->mutex);
        }
        
        PRINT("before pthread_mutex_lock\n");
        // 保证串口和网口监控线程先获取到锁
        while (terminal_dev_register->mutex_lock_flag == 0)
        {
            usleep(20);
        }
        terminal_dev_register->mutex_lock_flag = 0;
        pthread_mutex_lock(&terminal_dev_register->mutex);
        PRINT("after pthread_mutex_lock\n");
        
        // 设置线程创建
        for (i = 0; i < common_tools.config->repeat; i++)
        {
            if ((res = pthread_create(&terminal_dev_register->pad_cmd_handle_id, NULL, (void*)pad_cmd_handle, (void *)terminal_dev_register)) < 0)
            {
                PERROR("start_pthread_cmd_handle failed!\n");
                continue;
            }
            terminal_dev_register->config_now_flag = 1;
            break;
        }
        
        // 创建失败时
        if (res < 0)
        {
            // 发错误信号
            terminal_dev_register->pad_cmd_handle_id = 0;
            pthread_cond_signal(&terminal_dev_register->cond);
            pthread_mutex_unlock(&terminal_dev_register->mutex);
        }
        else // 创建成功时
        {
            PRINT("terminal_dev_register->pad_cmd_handle_id = %d\n", terminal_dev_register->pad_cmd_handle_id);
            PRINT("pthread_create (pad_cmd_handle) success!\n");
            // 发正确信号
            pthread_cond_signal(&terminal_dev_register->cond);
            usleep(500000);
            pthread_mutex_unlock(&terminal_dev_register->mutex);
            
            PRINT("before pthread_join\n");
            
            // 释放线程
            pthread_join(terminal_dev_register->pad_cmd_handle_id, NULL);
            terminal_dev_register->config_now_flag = 0;
            terminal_dev_register->pad_cmd_handle_id = 0;
            PRINT("after pthread_join\n");
        }
    }
    
    return 0;
}

#if BOARDTYPE == 5350
/**
 * pad串口监听  
 */
void * pad_serial_monitor(void* para) 

#elif BOARDTYPE == 9344

/**
 * pad串口监听  
 */
void * pad_usb_monitor(void* para) 
#endif
{
    pthread_mutex_lock(&config_mutex);
    
    int i = 0;
    int res = 0, ret = 0;
    
    fd_set fdset;
    struct timeval tv;
    struct timeval tpstart, tpend; 	
    memset(&tv, 0, sizeof(struct timeval));
    memset(&tpstart, 0, sizeof(struct timeval));
    memset(&tpend, 0, sizeof(struct timeval));
    
    struct s_pad_and_6410_msg pad_and_6410_msg;
    memset(&pad_and_6410_msg, 0, sizeof(struct s_pad_and_6410_msg));
    pad_and_6410_msg.head[0] = 0x5A;
    pad_and_6410_msg.head[1] = 0xA5;
    
    char *buf = NULL;
    char buf_tmp[128] = {0};
    char pad_mac[20] = {0};
    char columns_name[5][30] = {0};
    char columns_value[5][100] = {0};
    
    unsigned char cmd_count = 0;
    int len = 0;
    unsigned short buf_len = 0;
    unsigned short insert_len = 0;
    unsigned char one_byte_timeout_sec = 0;
    
    unsigned char cmd = 0;
    unsigned char write_falg = 1;  // 是否有必要更新注册状态
    struct s_terminal_dev_register * terminal_dev_register = (struct s_terminal_dev_register *)para;
    
    #if BOARDTYPE == 9344
    int usb_client_fd = 0;
    #endif
    pthread_mutex_unlock(&config_mutex);
    
    /*************************************************************************************/
    /************************   以上全局变量的定义 以下为主要功能实现   ******************/
    /*************************************************************************************/
    
    while(1) 
    {   
        PRINT("_______________\n");
        pthread_mutex_lock(&network_config.recv_mutex);
        // 接收信号
        if (network_config.pthread_recv_flag == 1)
        {
            pthread_mutex_unlock(&network_config.recv_mutex);
            #if BOARDTYPE == 5350
            PRINT("pad_serial waiing sleep 2...\n");
            #else 
            PRINT("pad_usb waiing sleep 2...\n");
            #endif
            sleep(2);
            continue;
        }
        pthread_mutex_unlock(&network_config.recv_mutex);
        
        PRINT("_______________\n");
        pthread_mutex_lock(&config_mutex);     
        FD_ZERO(&fdset);
        #if BOARDTYPE == 5350
        FD_SET(*network_config.serial_pad_fd, &fdset); 
        #elif BOARDTYPE == 9344
        FD_SET(network_config.usb_pad_fd, &fdset); 
        #endif
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        #if BOARDTYPE == 5350 || BOARDTYPE == 6410
        switch(select(*network_config.serial_pad_fd + 1, &fdset, NULL, NULL, &tv))
        #elif BOARDTYPE == 9344
        switch(select(network_config.usb_pad_fd + 1, &fdset, NULL, NULL, &tv))
        #endif
        {
            case -1:             
            case 0:
            {
                PRINT("pad_serial waiting to receive data...\n");
                pthread_mutex_unlock(&config_mutex);
                usleep(common_tools.config->one_byte_delay_usec);
                continue;
            }
            default:
            {             
                if (network_config.pthread_recv_flag == 1)
                {
                    PRINT("network_config.pthread_recv_flag == 1\n");
                    pthread_mutex_unlock(&config_mutex);
                    continue;
                }
                
                #if BOARDTYPE == 5350 || BOARDTYPE == 6410
                if (FD_ISSET(*network_config.serial_pad_fd, &fdset) > 0)
                #elif BOARDTYPE == 9344
                if (FD_ISSET(network_config.usb_pad_fd, &fdset) > 0)
                #endif
                {
                    memset(&tpstart, 0, sizeof(struct timeval));
                    gettimeofday(&tpstart, NULL); // 得到当前时间
                    //tcflush(*network_config.serial_pad_fd, TCIOFLUSH);
                    
                    #if BOARDTYPE == 9344
                    if ((res = communication_network.accept_client_connection(network_config.usb_pad_fd)) < 0)
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "accept_client_connection failed", res); 
                        pthread_mutex_unlock(&config_mutex);
                        continue;
                    }
                    usb_client_fd = res;
                    PRINT("usb_client_fd = %d\n", usb_client_fd);
                    #endif 
                    
                    // 接收pad发送的数据
                    #if BOARDTYPE == 5350 || BOARDTYPE == 6410
                    if ((res = network_config.recv_msg_from_pad(*network_config.serial_pad_fd, &pad_and_6410_msg)) < 0)
                    #elif BOARDTYPE == 9344
                    if ((res = network_config.recv_msg_from_pad(usb_client_fd, &pad_and_6410_msg)) < 0)
                    #endif    
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg_from_pad failed", res); 
                        break;
                    }
                    
                    switch (pad_and_6410_msg.cmd)
                    {
                        case 0x01:   //0x01: 动态IP + 注册
                        case 0x02:   //0x02: 静态IP + 注册
                        case 0x03:   //0x03: PPPOE + 注册
                        case 0x04:   //0x04: 询问当前设置状态
                        case 0x07:   //0x07: 无线网络设置
                        case 0x08:   //0x08: 动态IP
                        case 0x09:   //0x09: 静态IP
                        case 0x0A:   //0x0A: PPPOE
                        case 0x0C:   //0x0C: 恢复出厂（终端初始化前的状态）
                        case 0x0D:   //0x0D: 查看Base版本
                        case 0x52:   //0x52: 查看WAN口
                        case 0x53:   //0x53: 取消当前配置
                        {
                            cmd = pad_and_6410_msg.cmd;                            
                            break;
                        }
                        default:
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "Data does not match", res);
                            res = P_DATA_ERR;
                            break;
                        }    
                    }
                    if (res < 0)
                    {
                        break;
                    }
                    
                    // 查询数据库
                    memset(columns_name[0], 0, sizeof(columns_name[0]));
                    memset(columns_value[0], 0, sizeof(columns_value[0]));                  
                    memcpy(columns_name[0], "register_state", strlen("register_state")); 
                    #if BOARDTYPE == 6410 || BOARDTYPE == 9344
                    if ((res = database_management.select(1, columns_name, columns_value)) < 0)
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);                           
                        break;
                    }
                    *network_config.pad_cmd = atoi(columns_value[0]);
                    PRINT("columns_value[0] = %02X %02X %02X %02X, *network_config.pad_cmd = %02X\n", columns_value[0][0], columns_value[0][1], columns_value[0][2], columns_value[0][3], *network_config.pad_cmd);
                    #elif BOARDTYPE == 5350
                    if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);                           
                        break;
                    }
                    if ((strcmp("\"\"", columns_value[0]) == 0) || (strlen(columns_value[0]) == 0))
                    {
                        memset(columns_value[0], 0, sizeof(columns_value[0]));
                        sprintf(columns_value[0], "There is no (%s) record!", columns_name[0]);
                        res = NULL_ERR;
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[0], res); 
                        break;
                    }
                    *network_config.pad_cmd = atoi(columns_value[0]);
                    PRINT("columns_value[0] = %s, *network_config.pad_cmd = %02X\n", columns_value[0], (unsigned char)*network_config.pad_cmd);
                    
                    #endif
                    
                    
                    if ((unsigned char)(*network_config.pad_cmd) == 0xFC) //广域网设置成功，由于pad和base不是合法设备
                    {                            
                        memset(columns_name[0], 0, sizeof(columns_name[0]));
                        memset(columns_value[0], 0, sizeof(columns_value[0]));
                        memcpy(columns_name[0], "pad_sn", strlen("pad_sn"));
                        
                        #if BOARDTYPE == 6410 || BOARDTYPE == 9344
                        if ((res = database_management.select(1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);                            
                            break;
                        }
                        #elif BOARDTYPE == 5350
                        if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);
                            break;
                        }
                        if ((strcmp("\"\"", columns_value[0]) == 0) || (strlen(columns_value[0]) == 0))
                        {
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            sprintf(columns_value[0], "There is no (%s) record!", columns_name[0]);
                            res = NULL_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[0], res); 
                            break;
                        }
                        #endif
                        PRINT("columns_value[0] = %s\n", columns_value[0]);
                        if (pad_and_6410_msg.cmd == 0x04)
                        {
                            PRINT("pad_and_6410_msg.data = %s\n", pad_and_6410_msg.data);
                            // pad_sn不相同说明此时是一个新的PAD，把数据恢复原始状态
                            if (memcmp(columns_value[0], pad_and_6410_msg.data, strlen(columns_value[0])) != 0)
                            {
                                *network_config.cmd = 0xFB;
                                *network_config.pad_cmd = 0xFB;
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "new pad!", 0);
                            }
                            else
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "pad and base mismatching!", 0);
                            }
                        }
                        else
                        {
                            PRINT("pad_and_6410_msg.data + 1 = %s\n", pad_and_6410_msg.data + 1);
                            // pad_sn不相同说明此时是一个新的PAD，把数据恢复原始状态
                            if (memcmp(columns_value[0], pad_and_6410_msg.data + 1, strlen(columns_value[0])) != 0)
                            {
                                *network_config.cmd = 0xFB;
                                *network_config.pad_cmd = 0xFB;
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "new pad!", 0);
                            }
                            else
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "pad and base mismatching!", DATA_ERR);
                            }
                        }                            
                    }
                    
                    // 把接收的数据打包
                    if (pad_and_6410_msg.cmd == 0x04)
                    {
                        if (pad_and_6410_msg.len != 35)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        
                        pthread_mutex_lock(&network_config.recv_mutex);
                        PRINT("terminal_dev_register->config_now_flag = %d\n", terminal_dev_register->config_now_flag);
                        // 说明此时正在设置
                        if (terminal_dev_register->config_now_flag == 1)
                        {
                            write_falg = 0;
                            // 正在设置时出现，是否加入sn判断
                            for (i = 0; i < common_tools.config->repeat; i++)
                            {
                                if (pad_and_6410_msg.data != NULL)
                                {
                                    free(pad_and_6410_msg.data);
                                    pad_and_6410_msg.data = NULL;
                                }
                                // 数据发送到PAD
                                #if BOARDTYPE == 5350 || BOARDTYPE == 6410                      
                                if ((res = network_config.send_msg_to_pad(*network_config.serial_pad_fd, 0XFA, NULL, 0)) < 0)
                                #elif BOARDTYPE == 9344
                                if ((res = network_config.send_msg_to_pad(usb_client_fd, 0XFA, NULL, 0)) < 0)
                                #endif
                                {
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                                    continue;
                                }
                                
                                #if BOARDTYPE == 5350 || BOARDTYPE == 6410
                                if ((res = network_config.recv_msg_from_pad(*network_config.serial_pad_fd, &pad_and_6410_msg)) < 0)
                                #elif BOARDTYPE == 9344
                                if ((res = network_config.recv_msg_from_pad(usb_client_fd, &pad_and_6410_msg)) < 0)
                                #endif
                                {
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg_from_pad failed", res);
                                    continue;
                                }
                                PRINT("pad_and_6410_msg.cmd = %d\n", pad_and_6410_msg.cmd);
                                if ((pad_and_6410_msg.data != NULL) || (pad_and_6410_msg.cmd != 0x00))
                                {
                                    res = WRITE_ERR;
                                    continue;
                                }
                                break;
                            }
                            pthread_mutex_unlock(&network_config.recv_mutex);
                            break;
                        }
                        pthread_mutex_unlock(&network_config.recv_mutex);
                        
                        if ((unsigned char)*network_config.pad_cmd == 0xFB)
                        {
                            // 此时需查看pad_mac是否为空
                            memset(columns_name[0], 0, sizeof(columns_name[0]));
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            memcpy(columns_name[0], "pad_mac", strlen("pad_mac"));
                            
                            #if BOARDTYPE == 5350
                            if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res); 
                                break;
                            }
                            else 
                            {
                                PRINT("columns_value[0] = %s\n", columns_value[0]);
                            }
                            #elif BOARDTYPE == 9344 
                            if ((res = database_management.select(1, columns_name, columns_value)) < 0)
                            {
                                if (res == NO_RECORD_ERR)
                                {
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no record!", res); 
                                    res = 0;
                                }
                                else 
                                {
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res); 
                                    break;    
                                }
                            }
                            else 
                            {
                                PRINT("columns_value[0] = %s\n", columns_value[0]);
                            }
                            #endif
                            if ((strcmp("\"\"", columns_value[0]) != 0) && (strlen(columns_value[0]) == 12))
                            {
                                // 获取10.10.10.100的mac
                                memset(buf_tmp, 0, sizeof(buf_tmp));
                                strcpy(buf_tmp, "arping -c 1 -w 1 -I br0 10.10.10.100 | grep '\\[' | awk '{print $5}'");
                                if ((res = common_tools.get_cmd_out(buf_tmp, pad_mac, sizeof(pad_mac))) < 0)
                                {
                                    PRINT("res = %d\n", res);
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_cmd_out failed!", res);
                                    break;
                                }
                                PRINT("res = %d\n", res);
                                memset(buf_tmp, 0, sizeof(buf_tmp));
                                
                                if (strlen(pad_mac) > 0)
                                {
                                    memcpy(buf_tmp, pad_mac + 1, strlen(pad_mac) - 2);
                                    memset(pad_mac, 0, sizeof(pad_mac));
                                    memcpy(pad_mac, buf_tmp, strlen(buf_tmp));
                                    memset(buf_tmp, 0, sizeof(buf_tmp));
                                    //pad_mac[strlen(pad_mac) - 2] = '\0';
                                    PRINT("pad_mac = %s\n", pad_mac);
                                    // 转换成正常格式MAC地址 00:AA:11:BB:CC:DD
                                    if ((res = common_tools.mac_format_conversion(pad_mac)) < 0)
                                    {
                                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "mac_format_conversion failed", res);
                                        break;
                                    }
                                    common_tools.mac_del_colon(pad_mac);
                                    PRINT("pad_mac = %s, columns_value[0] = %s\n", pad_mac, columns_value[0]);
                                    if (memcmp(columns_value[0], pad_mac, strlen(pad_mac)) == 0)
                                    {
                                        *network_config.pad_cmd = 0xFD;
                                        *network_config.cmd = 0xFD;
                                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "Local area network normal!", 0);
                                    }
                                }
                            }
                        }
                        
                        if ((unsigned char)*network_config.pad_cmd == 0xFD)
                        {
                            #if CHECK_WAN_STATE == 1
                            
                            #if 1
                            if ((res = common_tools.get_network_state(common_tools.config->terminal_server_ip, 1, 1)) < 0)
                            #else
                            if ((res = common_tools.get_network_state(common_tools.config->wan_check_name, 1, 1)) < 0)
                            #endif
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_network_state failed!", res);
                            }
                            else
                            {
                                *network_config.pad_cmd = 0xFE;
                                *network_config.cmd = 0xFE;
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "Wide network normal!", 0);
                            }
                            
                            #else
                            *network_config.pad_cmd = 0xFE;
                            *network_config.cmd = 0xFE;
                            #endif
                        }
                        #if USER_REGISTER == 0
                        if ((unsigned char)*network_config.pad_cmd == 0xFE)
                        {
                            *network_config.pad_cmd = 0x00;
                            *network_config.cmd = 0x00;
                        }
                        #endif
                        
                        // 把当前的状态发送到PAD
                        for (i = 0; i < common_tools.config->repeat; i++)
                        {
                            if (pad_and_6410_msg.data != NULL)
                            {
                                free(pad_and_6410_msg.data);
                                pad_and_6410_msg.data = NULL;
                            }
                            // 数据发送到PAD
                            #if BOARDTYPE == 5350 || BOARDTYPE == 6410
                            if ((res = network_config.send_msg_to_pad(*network_config.serial_pad_fd, *network_config.pad_cmd, NULL, 0)) < 0)
                            #elif BOARDTYPE == 9344
                            if ((res = network_config.send_msg_to_pad(usb_client_fd, *network_config.pad_cmd, NULL, 0)) < 0)
                            #endif
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                                continue;
                            }
                            
                            #if BOARDTYPE == 5350 || BOARDTYPE == 6410
                            if ((res = network_config.recv_msg_from_pad(*network_config.serial_pad_fd, &pad_and_6410_msg)) < 0)
                            #elif BOARDTYPE == 9344
                            if ((res = network_config.recv_msg_from_pad(usb_client_fd, &pad_and_6410_msg)) < 0)
                            #endif
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg_from_pad failed", res);
                                continue;
                            }
                            //PRINT("pad_and_6410_msg.data = %s, pad_and_6410_msg.cmd = %d\n", pad_and_6410_msg.data, pad_and_6410_msg.cmd);
                            if ((pad_and_6410_msg.data != NULL) || (pad_and_6410_msg.cmd != 0x00))
                            {
                                res = WRITE_ERR;
                                continue;
                            }
                            break;
                        }
                        #if BOARDTYPE == 5350 || BOARDTYPE == 6410              
                        tcflush(*network_config.serial_pad_fd, TCIOFLUSH);
                        #elif BOARDTYPE == 9344
                        #endif
                        if (i == common_tools.config->repeat)
                        {
                            break;
                        }
                        
                        if (*network_config.pad_cmd == 0x00)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "The terminal has been registered successfully!", 0); 
                            break;
                        }                    
                        else if ((unsigned char)*network_config.pad_cmd == 0xFE) //广域网设置成功，注册失败
                        {
                            #if USER_REGISTER
                            memset(columns_name, 0, sizeof(columns_name));
                            memset(columns_value, 0, sizeof(columns_value));
                            memcpy(columns_name[0], "pad_sn", strlen("pad_sn"));
                            memcpy(columns_name[1], "pad_mac", strlen("pad_mac"));
                            memcpy(columns_name[2], "base_sn", strlen("base_sn"));
                            memcpy(columns_name[3], "base_mac", strlen("base_mac"));
                            memcpy(columns_name[4], "base_ip", strlen("base_ip"));
                            #if BOARDTYPE == 6410 || BOARDTYPE == 9344
                            if ((res = database_management.select(5, columns_name, columns_value)) < 0)
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);
                                break;
                            }
                            #elif BOARDTYPE == 5350
                            if ((res = nvram_interface.select(RT5350_FREE_SPACE, 5, columns_name, columns_value)) < 0)
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);
                                break;
                            }
                            for (i = 0; i < 5; i++)
                            {
                                if ((strcmp("\"\"", columns_value[i]) == 0) || (strlen(columns_value[i]) == 0))
                                {
                                    memset(columns_value[i], 0, sizeof(columns_value[i]));
                                    sprintf(columns_value[i], "There is no (%s) record!", columns_name[i]);
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[i], res); 
                                    break;
                                }
                            }
                            if (i < 5)
                            {
                                res = NULL_ERR;
                                break;
                            }
                            #endif
                            if ((res = terminal_register.user_register(columns_value[0], columns_value[1])) < 0)
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "user_register failed", res);
                                if (res == WRONGFUL_DEV_ERR)
                                {
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "pad and base mismatching!", res);
                                    *network_config.cmd = 0xFC;
                                }
                                break;
                            }
                            memset(columns_name[0], 0, sizeof(columns_name[0]));
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            memcpy(columns_name[0], "register_state", strlen("register_state")); 
                            
                            memcpy(columns_value[0], network_config.pad_cmd, sizeof(*network_config.pad_cmd)); 
                            buf_len = sizeof(*network_config.pad_cmd);
                            for (i = 0; i < common_tools.config->repeat; i++)
                            {
                                #if BOARDTYPE == 6410 || BOARDTYPE == 9344 
                                if ((res = database_management.insert(1, columns_name, columns_value, &buf_len)) < 0)
                                {
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_insert failed!", res);
                                    continue;
                                }
                                #elif BOARDTYPE == 5350
                                if ((res = nvram_interface.insert(RT5350_FREE_SPACE, 1, columns_name, columns_value, &buf_len)) < 0)
                                {
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_insert failed!", res); 
                                    continue;
                                }
                                #endif
                                break;
                            }
                            
                            if (res < 0)
                            {
                                break;
                            }
                            memset(network_config.base_sn, 0, sizeof(network_config.base_sn));
                            memset(network_config.base_mac, 0, sizeof(network_config.base_mac));
                            memset(network_config.base_ip, 0, sizeof(network_config.base_ip));
                            memcpy(network_config.base_sn, columns_name[2], strlen(columns_name[2]));
                            memcpy(network_config.base_mac, columns_name[3], strlen(columns_name[3]));
                            memcpy(network_config.base_ip, columns_name[4], strlen(columns_name[4])); 
                            #else
                            *network_config.cmd = 0;
                            #endif
                        }
                        else if ((unsigned char)*network_config.pad_cmd == 0xFB) //局域网设置失败
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "lan setup error!", 0);
                            break;
                        }
                        else if ((unsigned char)*network_config.pad_cmd == 0xFD) //局域网设置成功，广域网设置失败
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "wan setup error!", 0);
                            break;
                        }
                        else  if ((unsigned char)*network_config.pad_cmd == 0xFC) //广域网设置成功，由于pad和base不是合法设备
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "pad and base mismatching!", 0);
                            break;
                        }
                    }
                    else if ((pad_and_6410_msg.cmd == 0x07) || (pad_and_6410_msg.cmd == 0x08) || 
                        (pad_and_6410_msg.cmd == 0x09) || (pad_and_6410_msg.cmd == 0x0A)) // 无线网络设置和网络设置
                    {
                        
                        if (pad_and_6410_msg.len != 36)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        
                        if ((unsigned char)*network_config.pad_cmd != 0x00)
                        {
                            res = P_DATA_ERR;
                            break;
                        }
                        *network_config.network_flag = 1;
                        
                        #if BOARDTYPE == 5350 || BOARDTYPE == 6410
                        if ((res = network_config.send_msg_to_pad(*network_config.serial_pad_fd, 0x00, NULL, 0)) < 0)
                        #elif BOARDTYPE == 9344
                        if ((res = network_config.send_msg_to_pad(usb_client_fd, 0x00, NULL, 0)) < 0)
                        #endif
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                            break;
                        }
                        
                        #if BOARDTYPE == 5350 || BOARDTYPE == 6410         
                        if ((res = network_config.network_settings(*network_config.serial_pad_fd, pad_and_6410_msg.data[0], pad_and_6410_msg.cmd)) < 0)
                        #elif BOARDTYPE == 9344
                        if ((res = network_config.network_settings(usb_client_fd, pad_and_6410_msg.data[0], pad_and_6410_msg.cmd)) < 0)
                        #endif
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "network_config failed", res); 
                            write_falg = 0;
                            break;
                        }
                        
                        #if BOARDTYPE == 5350 || BOARDTYPE == 6410 
                        if ((res = network_config.send_msg_to_pad(*network_config.serial_pad_fd, 0x00, NULL, 0)) < 0)
                        #elif BOARDTYPE == 9344
                        if ((res = network_config.send_msg_to_pad(usb_client_fd, 0x00, NULL, 0)) < 0)
                        #endif 
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                        }
                        break;
                    }
                    else if (pad_and_6410_msg.cmd == 0x0C) //恢复出厂
                    {
                        if (pad_and_6410_msg.len != 35)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        
                        #if 0
                        // sn对比
                        memset(columns_name[0], 0, sizeof(columns_name[0]));
                        memset(columns_value[0], 0, sizeof(columns_value[0]));                  
                        memcpy(columns_name[0], "pad_sn", strlen("pad_sn")); 
                        
                        #if BOARDTYPE == 5350
                        if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);                           
                            break;
                        }
                        if ((strcmp("\"\"", columns_value[0]) == 0) || (strlen(columns_value[0]) == 0))
                        {
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            sprintf(columns_value[0], "There is no (%s) record!", columns_name[0], NULL_ERR);
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[0]); 
                            res = NULL_ERR;
                            break;
                        }
                        #elif BOARDTYPE == 9344
                        if ((res = database_management.select(1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);
                            break;
                        }
                        #endif
                        PRINT("columns_value[0] = %s, pad_and_6410_msg.data = %s\n", columns_value[0], pad_and_6410_msg.data);
                        if (memcmp(columns_value[0], pad_and_6410_msg.data, strlen(columns_value[0])) != 0)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res); 
                            break;
                        }
                        #endif
                        
                        #if BOARDTYPE == 5350
                        system("kill -9 `ps | grep cacm | sed '/grep/'d | awk '{print $1}'`");
                        system("nvram_set freespace register_state 251");
                        #if SMART_RECOVERY == 1// 路由智能恢复
                        system("nvram_set backupspace register_state 251");
                        #endif
                        system("ralink_init clear 2860");
                        #elif BOARDTYPE == 9344 
                        system("cfg -x");
                        #endif
                        
                        #if BOARDTYPE == 5350 || BOARDTYPE == 6410 
                        if ((res = network_config.send_msg_to_pad(*network_config.serial_pad_fd, 0x00, NULL, 0)) < 0)
                        #elif BOARDTYPE == 9344
                        if ((res = network_config.send_msg_to_pad(usb_client_fd, 0x00, NULL, 0)) < 0)
                        #endif
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                            break;
                        }
                        
                        #if BOARDTYPE == 5350
                        system("reboot");
                        #elif BOARDTYPE == 9344
                        system("/etc/ath/apdown");
                        system("/etc/ath/apup");
                        #endif
                        break;
                    }
                    else if (pad_and_6410_msg.cmd == 0x0D) //查看Base版本信息
                    {
                        if (pad_and_6410_msg.len != 35)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        
                        #if 0
                        // sn对比
                        memset(columns_name[0], 0, sizeof(columns_name[0]));
                        memset(columns_value[0], 0, sizeof(columns_value[0]));                  
                        memcpy(columns_name[0], "pad_sn", strlen("pad_sn")); 
                        
                        #if BOARDTYPE == 5350 || BOARDTYPE == 9344 
                        if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);
                            break;
                        }
                        if ((strcmp("\"\"", columns_value[0]) == 0) || (strlen(columns_value[0]) == 0))
                        {
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            sprintf(columns_value[0], "There is no (%s) record!", columns_name[0]);
                            res = NULL_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[0], res); 
                            break;
                        }
                        #endif
                        PRINT("columns_value[0] = %s, pad_and_6410_msg.data = %s\n", columns_value[0], pad_and_6410_msg.data);
                        if (memcmp(columns_value[0], pad_and_6410_msg.data, strlen(columns_value[0])) != 0)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res); 
                            break;
                        }
                        #endif
                        
                        if ((res = common_tools.get_cmd_out("cat /proc/version", buf_tmp, sizeof(buf_tmp))) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_cmd_out failed!", res);
                            break;
                        }
                        
                        #if BOARDTYPE == 5350 || BOARDTYPE == 6410 
                        if ((res = network_config.send_msg_to_pad(*network_config.serial_pad_fd, 0x00, buf_tmp, strlen(buf_tmp))) < 0)
                        #elif BOARDTYPE == 9344
                        if ((res = network_config.send_msg_to_pad(usb_client_fd, 0x00, buf_tmp, strlen(buf_tmp))) < 0)
                        #endif
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                            break;
                        }
                        sleep(3);
                        
                        #if BOARDTYPE == 5350 || BOARDTYPE == 6410 
                        if ((res = network_config.send_msg_to_pad(*network_config.serial_pad_fd, 0x00, buf_tmp, strlen(buf_tmp))) < 0)
                        #elif BOARDTYPE == 9344
                        if ((res = network_config.send_msg_to_pad(usb_client_fd, 0x00, buf_tmp, strlen(buf_tmp))) < 0)
                        #endif
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                            break;
                        }
                        break;
                    }
                    else if (pad_and_6410_msg.cmd == 0x52) //0x52: 查看WAN口
                    {
                        if (pad_and_6410_msg.len != 35)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        
                        if ((res = network_config.get_wan_state()) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_wan_state failed!", res);
                            break;
                        }
                        
                        break;
                    }
                    else if (pad_and_6410_msg.cmd == 0x53) //0x53: 取消当前操作
                    {
                        if (pad_and_6410_msg.len != 35)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        
                        // sn对比
                        memset(columns_name[0], 0, sizeof(columns_name[0]));
                        memset(columns_value[0], 0, sizeof(columns_value[0]));                  
                        memcpy(columns_name[0], "pad_sn", strlen("pad_sn")); 
                        
                        #if BOARDTYPE == 5350
                        if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);  
                            break;
                        }
                        #elif BOARDTYPE == 9344
                        if ((res = database_management.select(1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);  
                            break;
                        }
                        #endif
                        if ((strcmp("\"\"", columns_value[0]) == 0) || (strlen(columns_value[0]) == 0))
                        {
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            sprintf(columns_value[0], "There is no (%s) record!", columns_name[0]);
                            res = NULL_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[0], res);
                            break;
                        }
                        
                        PRINT("columns_value[0] = %s, pad_and_6410_msg.data = %s\n", columns_value[0], pad_and_6410_msg.data);
                        if (memcmp(columns_value[0], pad_and_6410_msg.data, strlen(columns_value[0])) != 0)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
                            break;
                        }
                        
                        if (terminal_dev_register->config_now_flag == 1)
                        {
                            #if BOARDTYPE == 5350
                            pthread_mutex_lock(&nvram_interface.mutex);
                            #endif
                            PRINT("_______________\n");
                            if (terminal_dev_register->pad_cmd_handle_id != 0)
                            {
                                if (pthread_cancel(terminal_dev_register->pad_cmd_handle_id) != 0)
                                {
                                    PERROR("pthread_cancel failed!\n");
                                    res = PTHREAD_CANCEL_ERR;
                                }
                                PRINT("after pthread_cancel!\n");
                            }
                            PRINT("_______________\n");
                            #if BOARDTYPE == 5350
                            pthread_mutex_unlock(&nvram_interface.mutex);
                            #endif
                            // 判断锁状态(需要跟踪)
                            if(pthread_mutex_trylock(&network_config.recv_mutex) == EBUSY)
                            {
                                PRINT("pad_cmd_handle pthread lock now!\n");
                                pthread_mutex_unlock(&network_config.recv_mutex);
                            }
                            else
                            {
                                PRINT("serial pthread lock now!\n");
                                pthread_mutex_unlock(&network_config.recv_mutex);
                            }
                        }
                        
                        /*
                        #if BOARDTYPE == 5350 || BOARDTYPE == 6410 
                        if ((res = network_config.send_msg_to_pad(*network_config.serial_pad_fd, 0x00, NULL, 0)) < 0)
                        #elif BOARDTYPE == 9344
                        if ((res = network_config.send_msg_to_pad(usb_client_fd, 0x00, NULL, 0)) < 0)
                        #endif
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                            break;
                        }
                        */
                        break;
                    }
                    else // 01 02 03 
                    {
                        if (pad_and_6410_msg.len != 36)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        
                        #if BOARDTYPE == 5350 || BOARDTYPE == 6410 
                        if ((res = network_config.send_msg_to_pad(*network_config.serial_pad_fd, 0x00, NULL, 0)) < 0)
                        #elif BOARDTYPE == 9344
                        if ((res = network_config.send_msg_to_pad(usb_client_fd, 0x00, NULL, 0)) < 0)
                        #endif
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                            break;
                        }
                        if (*network_config.pad_cmd == 0x00)
                        {
                            *network_config.network_flag = 1;
                        }
                        PRINT("_______________\n");
                        if (pthread_mutex_lock(&terminal_dev_register->mutex) < 0)
                        {
                            PERROR("pthread_mutex_lock failed!\n");
                            res = PTHREAD_LOCK_ERR;
                            break;
                        }
                        PRINT("_______________\n");
                        terminal_dev_register->transmission_mode = 1; // 串口传输
                        #if BOARDTYPE == 5350 || BOARDTYPE == 6410 
                        terminal_dev_register->fd = *network_config.serial_pad_fd;
                        #elif BOARDTYPE == 9344
                        terminal_dev_register->fd = usb_client_fd;
                        #endif
                        
                        terminal_dev_register->cmd_count = pad_and_6410_msg.data[0];
                        terminal_dev_register->cmd_word = pad_and_6410_msg.cmd;
                        
                        PRINT("_______________\n");
                        pthread_cond_signal(&terminal_dev_register->cond);
                        
                        if (pthread_mutex_unlock(&terminal_dev_register->mutex) < 0)
                        {
                            PERROR("pthread_mutex_unlock failed!\n");
                            res = PTHREAD_UNLOCK_ERR;
                            break;
                        }
                        
                        PRINT("_______________\n");
                        if (pthread_mutex_lock(&terminal_dev_register->mutex) < 0)
                        {
                            PERROR("pthread_mutex_lock failed!\n");
                            res = PTHREAD_LOCK_ERR;
                            break;
                        }
                        terminal_dev_register->mutex_lock_flag = 1;
                        pthread_cond_wait(&terminal_dev_register->cond, &terminal_dev_register->mutex);
                        PRINT("terminal_dev_register->pad_cmd_handle_id = %d\n", terminal_dev_register->pad_cmd_handle_id);
                        if (terminal_dev_register->pad_cmd_handle_id == 0)
                        {
                            // 线程创建失败
                            res = PTHREAD_CREAT_ERR;
                        }
                        if (pthread_mutex_unlock(&terminal_dev_register->mutex) < 0)
                        {
                            PERROR("pthread_mutex_unlock failed!\n");
                            res = PTHREAD_UNLOCK_ERR;
                            break;
                        }
                        PRINT("_______________\n");
                        break;
                    }
                }
            }
        }
        
        #if BOARDTYPE == 5350
        // 等于1时说明此时正在设置
        if ((terminal_dev_register->config_now_flag != 1) || (cmd == 0x53))
        {
            network_config.init_cmd_list();  // 初始化命令结构体
        }
        #endif
        
        PRINT("*network_config.cmd = %02X cmd = %02X\n", (unsigned char)*network_config.cmd, (unsigned char)cmd);
        if ((cmd == 0x04) && ((unsigned char)*network_config.cmd != 0xFF) && (write_falg == 1)) // 插入数据库
        {
            memset(columns_name[0], 0, sizeof(columns_name[0]));
            memset(columns_value[0], 0, sizeof(columns_value[0]));
            memcpy(columns_name[0], "register_state", strlen("register_state")); 
            #if BOARDTYPE == 6410 || BOARDTYPE == 9344 
            memcpy(columns_value[0], network_config.cmd, sizeof(*network_config.cmd));
            insert_len = sizeof(*network_config.cmd);
            #elif BOARDTYPE == 5350
            sprintf(columns_value[0], "%d", (unsigned char)*network_config.cmd);
            insert_len = strlen(columns_value[0]);
            PRINT("columns_value[0] = %s\n", columns_value[0]);
            #endif
            
            for (i = 0; i < common_tools.config->repeat; i++)
            {
                #if BOARDTYPE == 6410 || BOARDTYPE == 9344 
                if ((ret = database_management.insert(1, columns_name, columns_value, &insert_len)) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_insert failed!", ret);
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
        PRINT("res = %d\n", res);
        if (res < 0) // 当如上操作出现错误时，返回错误原因
        {
            if (buf != NULL)
            {
                free(buf);
                buf = NULL;
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
            #if 1
            PRINT("strlen(buf) = %d\n", strlen(buf));
            #if BOARDTYPE == 5350 || BOARDTYPE == 6410 
            if ((res = network_config.send_msg_to_pad(*network_config.serial_pad_fd, 0xFF, buf, strlen(buf))) < 0)
            #elif BOARDTYPE == 9344
            PRINT("usb_client_fd = %d\n", usb_client_fd);
            if ((res = network_config.send_msg_to_pad(usb_client_fd, 0xFF, buf, strlen(buf))) < 0)
            #endif
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed!", res);                
            }
            
            #if BOARDTYPE == 5350 || BOARDTYPE == 6410 
            sleep(3);
            PRINT("strlen(buf) = %d\n", strlen(buf));
            if ((res = network_config.send_msg_to_pad(*network_config.serial_pad_fd, 0xFF, buf, strlen(buf))) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed!", res);  
            }
            #endif
            
            #else
            while (1)
            {
                PRINT("strlen(buf) = %d\n", strlen(buf));
                #if BOARDTYPE == 5350 || BOARDTYPE == 6410 
                if ((res = network_config.send_msg_to_pad(*network_config.serial_pad_fd, 0xFF, buf)) < 0)
                #elif BOARDTYPE == 9344
                if ((res = network_config.send_msg_to_pad(usb_client_fd, 0xFF, buf)) < 0)
                #endif
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed!", res);    
                }
                //sleep(3);
            }
            #endif
            if (buf != NULL)
            {
                PRINT("buf = %s\n", buf);
                free(buf);
                buf = NULL;
            }
        }
        #if BOARDTYPE == 9344
        sleep(1);
        if ((usb_client_fd != 0) && (cmd != 0x01) && (cmd != 0x02) && (cmd != 0x03))
        {
            close(usb_client_fd);
            usb_client_fd = 0;
        }
        #endif
        
        if (buf != NULL)
        {
            free(buf);
            buf = NULL;
            PRINT("free ok!\n");
        }
        
        if (pad_and_6410_msg.data != NULL)
        {
            free(pad_and_6410_msg.data);
            pad_and_6410_msg.data = NULL;
        }
        cmd = 0;
        write_falg = 1;      
        memset(columns_name, 0, sizeof(columns_name));
        memset(columns_value, 0, sizeof(columns_value));
        buf_len = 0;
            
        gettimeofday(&tpend, NULL);                 
        common_tools.work_sum->work_success_sum++;
        common_tools.make_menulist(start_time, tpstart, tpend);
                
        common_tools.work_sum->work_sum++;
        #if BOARDTYPE == 5350 || BOARDTYPE == 6410 
        tcflush(*network_config.serial_pad_fd, TCIOFLUSH);
        #elif BOARDTYPE == 9344
        
        #endif
        
        network_config.pthread_flag = 0;
        pthread_mutex_unlock(&config_mutex);
        PRINT("_____________________\n");
    }
    return 0;
}

/**
 * pad客户端连接监听
 */
void * pad_socket_monitor(void* para) 
{
    pthread_mutex_lock(&config_mutex);
     
    int i = 0, j = 0;
    int res = 0, ret = 0;
    char port[15] = {0};
    
    fd_set fdset;
    struct timeval tv;
    struct timeval tpstart, tpend; 
    memset(&tv, 0, sizeof(struct timeval));
    memset(&tpstart, 0, sizeof(struct timeval));
    memset(&tpend, 0, sizeof(struct timeval));
    
    struct sockaddr_in client; 
    socklen_t len = sizeof(client); 
    
    int client_fd = 0; // 和monitor_application通信
    
    int fd = 0;
    int accept_pad_fd = 0;
    
    struct s_pad_and_6410_msg pad_and_6410_msg;
    memset(&pad_and_6410_msg, 0, sizeof(struct s_pad_and_6410_msg));
    struct s_pad_and_6410_msg monitor_and_base_msg;
    memset(&monitor_and_base_msg, 0, sizeof(struct s_pad_and_6410_msg));
    struct s_6410_and_5350_msg _6410_and_5350_msg;
    memset(&_6410_and_5350_msg, 0, sizeof(struct s_6410_and_5350_msg));
    
    monitor_and_base_msg.head[0] = 0x5A;
    monitor_and_base_msg.head[1] = 0xA5;
    pad_and_6410_msg.head[0] = 0x5A;
    pad_and_6410_msg.head[1] = 0xA5;
    _6410_and_5350_msg.head[0] = 0x5A;
    _6410_and_5350_msg.head[1] = 0xA5;
    
    char *buf = NULL;
    char send_buf[256] = {0};
    char buf_tmp[128] = {0};
    char pad_mac[20] = {0};
    char columns_name[5][30] = {0};
    char columns_value[5][100] = {0};
    
    char device_token[100];                   // 设备令牌
	char position_token[100];                 // 位置令牌
	
    unsigned char cmd_count = 0;
    unsigned short buf_len = 0;
    unsigned short insert_len = 0;
    unsigned char index = 0;
    
    char config_cmd[64] = {0};
    unsigned char cmd = 0;
    unsigned char write_falg = 1;  // 是否有必要更新注册状态
    struct s_terminal_dev_register * terminal_dev_register = (struct s_terminal_dev_register *)para;
    pthread_mutex_unlock(&config_mutex);
    
    /*************************************************************************************/
    /************************   以上全局变量的定义 以下为主要功能实现   ******************/
    /*************************************************************************************/
    
    while (1)
    {       
        PRINT("_______________\n");
        pthread_mutex_lock(&network_config.recv_mutex);
        // 接收信号
        if (network_config.pthread_recv_flag == 1)
        {
            pthread_mutex_unlock(&network_config.recv_mutex);
            PRINT("pad_socket waiting sleep 2...\n");
            sleep(2);
            continue;
        }
        pthread_mutex_unlock(&network_config.recv_mutex);
        
        PRINT("_______________\n");
        pthread_mutex_lock(&config_mutex);       
        FD_ZERO(&fdset);
        FD_SET(*network_config.server_base_fd, &fdset);       
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        switch(select(*network_config.server_base_fd + 1, &fdset, NULL, NULL, &tv))
        {
            case -1:
            case 0:
            {              
                PRINT("pad_socket waiting to receive data...\n");
                pthread_mutex_unlock(&config_mutex);
                usleep(common_tools.config->one_byte_delay_usec);
                continue;
            }
            default:
            {
                if (network_config.pthread_recv_flag == 1)
                {
                    PRINT("network_config.pthread_recv_flag == 1\n");
                    pthread_mutex_unlock(&config_mutex);
                    continue;
                }
                if (FD_ISSET(*network_config.server_base_fd, &fdset) > 0)
                {                    
                    memset(&tpstart, 0, sizeof(struct timeval));
                    gettimeofday(&tpstart, NULL); // 得到当前时间 
                    
                    if ((accept_pad_fd = accept(*network_config.server_base_fd, (struct sockaddr*)&client, &len)) < 0)
                	{	
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "accept failed!", accept_pad_fd);
                        pthread_mutex_unlock(&config_mutex);
                        continue;
                	}             
                    PRINT("server_base_fd = %d, accept_pad_fd = %d\n", *network_config.server_base_fd, accept_pad_fd);
                    // 接收pad发送的数据
                    if ((res = network_config.recv_msg_from_pad(accept_pad_fd, &pad_and_6410_msg)) < 0)
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg_from_pad failed", res);
                        break;
                    }
                    PRINT("pad_and_6410_msg.cmd = %02X\n", pad_and_6410_msg.cmd);
                    switch (pad_and_6410_msg.cmd)
                    {
                        case 0x01:   //0x01: 动态IP + 注册
                        case 0x02:   //0x02: 静态IP + 注册
                        case 0x03:   //0x03: PPPOE + 注册
                        case 0x04:   //0x04: 询问当前设置状态
                        case 0x05:   //0x05: 生成默认ssid，用于外围设备的接入
                        case 0x06:   //0x06: 注销默认ssid
                        case 0x07:   //0x07: 无线网络设置
                        case 0x08:   //0x08: 动态IP
                        case 0x09:   //0x09: 静态IP
                        case 0x0A:   //0x0A: PPPOE
                        case 0x0B:   //0x0B: 网络设置询问和无线设置询问
                        case 0x0C:   //0x0C: 恢复出厂（终端初始化前的状态）
                        case 0x0D:   //0x0D: 查看Base版本
                        #if CTSI_SECURITY_SCHEME == 2
                        case 0x0E:   //0x0E: 获取身份令牌
                        case 0x0F:   //0x0F: 获取位置令牌
                        case 0x50:   //0x50: 重新生成身份令牌
                        case 0x51:   //0x51: 重新生成身份令牌
                        #endif
                        case 0x52:   //0x52: 查看WAN口
                        case 0x53:   //0x53: 取消当前配置
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
                    if (res < 0)
                    {
                        break;
                    }
                    
                    // 查询数据库
                    memset(columns_name[0], 0, sizeof(columns_name[0]));
                    memset(columns_value[0], 0, sizeof(columns_value[0]));                        
                    memcpy(columns_name[0], "register_state", strlen("register_state")); 
                    
                    #if BOARDTYPE == 6410 || BOARDTYPE == 9344 
                    if ((res = database_management.select(1, columns_name, columns_value)) < 0)
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);
                        break;
                    }
                    *network_config.pad_cmd = atoi(columns_value[0]);
                    PRINT("columns_value[0] = %02X %02X %02X %02X, *network_config.pad_cmd = %02X\n", columns_value[0][0], columns_value[0][1], columns_value[0][2], columns_value[0][3], (unsigned char)*network_config.pad_cmd);
                    #elif BOARDTYPE == 5350 
                    if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);
                        break;
                    }
                    if ((strcmp("\"\"", columns_value[0]) == 0) || (strlen(columns_value[0]) == 0))
                    {
                        memset(columns_value[0], 0, sizeof(columns_value[0]));
                        sprintf(columns_value[0], "There is no (%s) record!", columns_name[0]);
                        res = NULL_ERR;
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[0], res); 
                        break;
                    }
                    *network_config.pad_cmd = atoi(columns_value[0]);
                    PRINT("columns_value[0] = %s, *network_config.pad_cmd = %02X\n", columns_value[0], (unsigned char)*network_config.pad_cmd);
                    #endif
                    
                    if ((unsigned char)*network_config.pad_cmd == 0xFC) //广域网设置成功，由于pad和base不是合法设备
                    {                            
                        memset(columns_name[0], 0, sizeof(columns_name[0]));
                        memset(columns_value[0], 0, sizeof(columns_value[0]));
                        memcpy(columns_name[0], "pad_sn", strlen("pad_sn"));
                        #if BOARDTYPE == 6410 || BOARDTYPE == 9344 
                        if ((res = database_management.select(1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);  
                            break;
                        }
                        #elif BOARDTYPE == 5350
                        if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);  
                            break;
                        }
                        #endif
                        if ((strcmp("\"\"", columns_value[0]) == 0) || (strlen(columns_value[0]) == 0))
                        {
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            sprintf(columns_value[0], "There is no (%s) record!", columns_name[0]);
                            res = NULL_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[0], res); 
                            break;
                        }
                        
                        PRINT("columns_value[0] = %s\n", columns_value[0]);
                        if (pad_and_6410_msg.cmd == 0x04)
                        {
                            PRINT("pad_and_6410_msg.data = %s\n", pad_and_6410_msg.data);
                            // pad_sn不相同说明此时是一个新的PAD，把数据恢复原始状态
                            if (memcmp(columns_value[0], pad_and_6410_msg.data, strlen(columns_value[0])) != 0)
                            {
                                *network_config.cmd = 0xFB;
                                *network_config.pad_cmd = 0xFB;
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "new pad!", 0);
                            }
                            else
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "pad and base mismatching!", DATA_ERR);
                            }
                        }
                        else
                        {
                            PRINT("pad_and_6410_msg.data + 1 = %s\n", pad_and_6410_msg.data + 1);
                            // pad_sn不相同说明此时是一个新的PAD，把数据恢复原始状态
                            if (memcmp(columns_value[0], pad_and_6410_msg.data + 1, strlen(columns_value[0])) != 0)
                            {
                                *network_config.cmd = 0xFB;
                                *network_config.pad_cmd = 0xFB;
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "new pad!", 0);
                            }
                            else
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "pad and base mismatching!", DATA_ERR);
                            }
                        }                            
                    }
                    // 判断命令字
                    if (pad_and_6410_msg.cmd == 0x04)
                    {
                        if (pad_and_6410_msg.len != 35)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        
                        // sn对比
                        memset(columns_name[0], 0, sizeof(columns_name[0]));
                        memset(columns_value[0], 0, sizeof(columns_value[0]));                  
                        memcpy(columns_name[0], "pad_sn", strlen("pad_sn")); 
                        
                        #if BOARDTYPE == 5350 
                        if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);  
                            break;
                        }
                        #elif BOARDTYPE == 9344
                        if ((res = database_management.select(1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);  
                            break;
                        }
                        #endif
                        if ((strcmp("\"\"", columns_value[0]) == 0) || (strlen(columns_value[0]) == 0))
                        {
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            sprintf(columns_value[0], "There is no (%s) record!", columns_name[0]);
                            res = NULL_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[0], res);
                            break;
                        }
                        
                        PRINT("columns_value[0] = %s, pad_and_6410_msg.data = %s\n", columns_value[0], pad_and_6410_msg.data);
                        if (memcmp(columns_value[0], pad_and_6410_msg.data, strlen(columns_value[0])) != 0)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
                            break;
                        }
                        
                        pthread_mutex_lock(&network_config.recv_mutex);
                        // 说明此时正在设置
                        if (terminal_dev_register->config_now_flag == 1)
                        {
                            write_falg = 0;
                            // 正在设置时出现，是否加入sn判断
                            for (i = 0; i < common_tools.config->repeat; i++)
                            {
                                if (pad_and_6410_msg.data != NULL)
                                {
                                    free(pad_and_6410_msg.data);
                                    pad_and_6410_msg.data = NULL;
                                }
                                // 数据发送到PAD                            
                                if ((res = network_config.send_msg_to_pad(accept_pad_fd, 0XFA, NULL, 0)) < 0)
                                {
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                                    continue;
                                }
                                if ((res = network_config.recv_msg_from_pad(accept_pad_fd, &pad_and_6410_msg)) < 0)
                                {
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg_from_pad failed", res);
                                    continue;
                                }
                                PRINT("pad_and_6410_msg.cmd = %d\n", pad_and_6410_msg.cmd);
                                if ((pad_and_6410_msg.data != NULL) || (pad_and_6410_msg.cmd != 0x00))
                                {
                                    res = WRITE_ERR;
                                    continue;
                                }
                                break;
                            }
                            
                            pthread_mutex_unlock(&network_config.recv_mutex);
                            break;
                        }
                        pthread_mutex_unlock(&network_config.recv_mutex);
                        
                        // 当数据库中存储的是FB状态时，
                        if ((unsigned char)*network_config.pad_cmd == 0xFB)
                        {
                            // 此时需查看pad_mac是否为空
                            memset(columns_name[0], 0, sizeof(columns_name[0]));
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            memcpy(columns_name[0], "pad_mac", strlen("pad_mac"));
                            
                            #if BOARDTYPE == 5350 
                            if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);  
                                break;
                            }
                            #elif BOARDTYPE == 9344 
                            if ((res = database_management.select(1, columns_name, columns_value)) < 0)
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);  
                                break;
                            }
                            #endif
                            if ((strcmp("\"\"", columns_value[0]) != 0) && (strlen(columns_value[0]) == 12))
                            {
                                // 获取10.10.10.100的mac
                                memset(buf_tmp, 0, sizeof(buf_tmp));
                                strcpy(buf_tmp, "arping -c 1 -w 1 -I br0 10.10.10.100 | grep '\\[' | awk '{print $5}'");
                                if ((res = common_tools.get_cmd_out(buf_tmp, pad_mac, sizeof(pad_mac))) < 0)
                                {
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_cmd_out failed!", res);
                                    break;
                                }
                                memset(buf_tmp, 0, sizeof(buf_tmp));
                                
                                if (strlen(pad_mac) > 0)
                                {
                                    memcpy(buf_tmp, pad_mac + 1, strlen(pad_mac) - 2);
                                    memset(pad_mac, 0, sizeof(pad_mac));
                                    memcpy(pad_mac, buf_tmp, strlen(buf_tmp));
                                    memset(buf_tmp, 0, sizeof(buf_tmp));
                                    //pad_mac[strlen(pad_mac) - 2] = '\0';
                                    PRINT("pad_mac = %s\n", pad_mac);
                                    // 转换成正常格式MAC地址 00:AA:11:BB:CC:DD
                                    if ((res = common_tools.mac_format_conversion(pad_mac)) < 0)
                                    {
                                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "mac_format_conversion failed", res);
                                        break;
                                    }
                                    common_tools.mac_del_colon(pad_mac);
                                    PRINT("pad_mac = %s, columns_value[0] = %s\n", pad_mac, columns_value[0]);
                                    if (memcmp(columns_value[0], pad_mac, strlen(pad_mac)) == 0)
                                    {
                                        *network_config.pad_cmd = 0xFD;
                                        *network_config.cmd = 0xFD;
                                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "Local area network normal!", 0);
                                    }
                                }
                            }
                        }
                        if ((unsigned char)*network_config.pad_cmd == 0xFD)
                        {
                            #if CHECK_WAN_STATE == 1
                            
                            #if 1
                            if ((res = common_tools.get_network_state(common_tools.config->terminal_server_ip, 1, 1)) < 0)
                            #else
                            if ((res = common_tools.get_network_state(common_tools.config->wan_check_name, 1, 1)) < 0)
                            #endif
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_network_state failed!", res);
                            }
                            else
                            {
                                *network_config.pad_cmd = 0xFE;
                                *network_config.cmd = 0xFE;
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "Wide network normal!", 0);
                            }
                            
                            #else // 不检测WAN口，直接置为0xFE，代表注册没有进行
                            *network_config.pad_cmd = 0xFE;
                            *network_config.cmd = 0xFE;
                            #endif
                        }
                        
                        #if USER_REGISTER == 0
                        if ((unsigned char)*network_config.pad_cmd == 0xFE)
                        {
                            *network_config.pad_cmd = 0x00;
                            *network_config.cmd = 0x00;
                        }
                        #endif
                        for (i = 0; i < common_tools.config->repeat; i++)
                        {
                            // 数据发送到PAD
                            if ((res = network_config.send_msg_to_pad(accept_pad_fd, *network_config.pad_cmd, NULL, 0)) < 0)
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                                continue;
                            }
                            if ((res = network_config.recv_msg_from_pad(accept_pad_fd, &pad_and_6410_msg)) < 0)
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg_from_pad failed", res);
                                continue;
                            }
                            if (pad_and_6410_msg.cmd == 0xFF)
                            {
                                res = WRITE_ERR;
                                continue;
                            }
                            break;
                        }
                        if (i == common_tools.config->repeat)
                        {
                            break;
                        }
                        
                        if (*network_config.pad_cmd == 0x00)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "The terminal has been registered successfully!", 0); 
                            break;
                        }                       
                        else if ((unsigned char)*network_config.pad_cmd == 0xFE) //广域网设置成功，注册失败
                        {
                            #if USER_REGISTER
                            memset(columns_name[0], 0, sizeof(columns_name[0]));
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            memcpy(columns_name[0], "pad_sn", strlen("pad_sn"));
                            memcpy(columns_name[1], "pad_mac", strlen("pad_mac"));
                            memcpy(columns_name[2], "base_sn", strlen("base_sn"));
                            memcpy(columns_name[3], "base_mac", strlen("base_mac"));
                            memcpy(columns_name[4], "base_ip", strlen("base_ip"));
                            #if BOARDTYPE == 6410 || BOARDTYPE == 9344 
                            if ((res = database_management.select(5, columns_name, columns_value)) < 0)
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);
                                break;
                            }
                            #elif BOARDTYPE == 5350 
                            if ((res = nvram_interface.select(RT5350_FREE_SPACE, 5, columns_name, columns_value)) < 0)
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res); 
                                break;
                            }
                            for (i = 0; i < 5; i++)
                            { 
                                if ((strcmp("\"\"", columns_value[i]) == 0) || (strlen(columns_value[i]) == 0))
                                {
                                    memset(columns_value[i], 0, sizeof(columns_value[i]));
                                    sprintf(columns_value[i], "There is no (%s) record!", columns_name[i]);
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[i], NULL_ERR);
                                    break;
                                }
                            }
                            if (i < 5)
                            {
                                res = NULL_ERR;
                                break;
                            }
                            #endif
                            if ((res = terminal_register.user_register(columns_value[0], columns_value[1])) < 0)
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "user_register failed", res);
                                if (res == WRONGFUL_DEV_ERR)
                                {
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "pad and base mismatching!", res);
                                    *network_config.cmd = 0xFC;
                                }
                                break;
                            }
                            memset(columns_name[0], 0, sizeof(columns_name[0]));
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            memcpy(columns_name[0], "register_state", strlen("register_state")); 
                            
                            memcpy(columns_value[0], network_config.pad_cmd, sizeof(*network_config.pad_cmd)); 
                            buf_len = sizeof(*network_config.pad_cmd);
                            for (i = 0; i < common_tools.config->repeat; i++)
                            {
                                #if BOARDTYPE == 6410 || BOARDTYPE == 9344 
                                if ((res = database_management.insert(1, columns_name, columns_value, &buf_len)) < 0)
                                {
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_insert failed!", res);
                                    continue;
                                }
                                #elif BOARDTYPE == 5350
                                if ((res = nvram_interface.insert(RT5350_FREE_SPACE, 1, columns_name, columns_value, &buf_len)) < 0)
                                {
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_insert failed!", res); 
                                    continue;
                                }
                                #endif
                                break;
                            }
                            
                            if (res < 0)
                            {
                                break;
                            }
                            memset(network_config.base_sn, 0, sizeof(network_config.base_sn));
                            memset(network_config.base_mac, 0, sizeof(network_config.base_mac));
                            memset(network_config.base_ip, 0, sizeof(network_config.base_ip));
                            memcpy(network_config.base_sn, columns_name[2], strlen(columns_name[2]));
                            memcpy(network_config.base_mac, columns_name[3], strlen(columns_name[3]));
                            memcpy(network_config.base_ip, columns_name[4], strlen(columns_name[4]));
                            
                            len = strlen("network_config.base_sn:") + strlen("network_config.base_mac:") + strlen("network_config.base_ip:") + 
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
                                //close(*network_config.server_pad_fd);
                                break;
                            }
                            
                            memset(buf, 0, len);
                            sprintf(buf, "network_config.base_sn:%s,network_config.base_mac:%s,network_config.base_ip:%s,%s,%s,%s,%s,%s,%s,%s,%s,", network_config.base_sn, network_config.base_mac, network_config.base_ip, 
                                terminal_register.respond_pack->base_user_name, terminal_register.respond_pack->base_password, 
                                terminal_register.respond_pack->pad_user_name, terminal_register.respond_pack->pad_password, 
                                terminal_register.respond_pack->sip_ip_address, terminal_register.respond_pack->sip_port, 
                                terminal_register.respond_pack->heart_beat_cycle, terminal_register.respond_pack->business_cycle);
                                
                    	    PRINT("%s\n", buf);
                            if ((res = network_config.send_msg_to_pad(accept_pad_fd, 0x00, buf, strlen(buf))) < 0)
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                                break;
                            }
                            #else
                            *network_config.cmd = 0;
                            #endif
                        }
                        else if ((unsigned char)*network_config.pad_cmd == 0xFB) //局域网设置失败
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "lan setup error!", 0);
                            break;
                        }
                        else if ((unsigned char)*network_config.pad_cmd == 0xFD) ////局域网设置成功，广域网设置失败
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "wan setup error!", 0);
                            break;
                        }
                        else if ((unsigned char)*network_config.pad_cmd == 0xFC) //pad和base不是合法设备
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "pad and base mismatching!", 0);
                            break;
                        }
                        
                    }
                    else if (pad_and_6410_msg.cmd == 0x05) // 生成默认SSID
                    {
                        if (pad_and_6410_msg.len != 35)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        if ((unsigned char)*network_config.pad_cmd != 0x00)  // PAD没有注册成功
                        {
                            res = NO_INIT_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
                            break;
                        }
                        
                        memset(columns_name, 0, sizeof(columns_name));
                        memset(columns_value, 0, sizeof(columns_value));
                        memcpy(columns_name[0], "pad_sn", strlen("pad_sn"));
                        memcpy(columns_name[1], "default_ssid", strlen("default_ssid"));
                        memcpy(columns_name[2], "default_ssid_password", strlen("default_ssid_password"));
                        
                        // 查询数据库，从中取得默认ssid和密码
                        #if BOARDTYPE == 6410 || BOARDTYPE == 9344
                        if ((res = database_management.select(3, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res); 
                            break;
                        }
                        #elif BOARDTYPE == 5350
                        if ((res = nvram_interface.select(RT5350_FREE_SPACE, 3, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);
                            break;
                        }
                        for (i = 0; i < 3; i++)
                        { 
                            if ((strcmp("\"\"", columns_value[i]) == 0) || (strlen(columns_value[i]) == 0))
                            {
                                memset(columns_value[i], 0, sizeof(columns_value[i]));
                                sprintf(columns_value[i], "There is no (%s) record!", columns_name[i]);
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[i], res); 
                                break;
                            }
                        }
                        if (i < 3)
                        {
                            res = NULL_ERR;
                            break;
                        }
                        #endif
                        PRINT("columns_value[0] = %s\n", columns_value[0]);
                        PRINT("pad_and_6410_msg.data = %s\n", pad_and_6410_msg.data);
                        
                        if (memcmp(columns_value[0], pad_and_6410_msg.data, strlen(columns_value[0])) != 0)
                        {
                            res = WRONGFUL_PAD_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "pad and base mismatching!", res);
                            break;
                        }
                                               
                        if ((res = network_config.send_msg_to_pad(accept_pad_fd, 0x00, NULL, 0)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                            break;
                        }
                        
                        #if BOARDTYPE == 5350
                        for (i = 0; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
                        {
                            // 初始化设备项
                            switch (network_config.cmd_list[i].cmd_word)
                            {
                                case 0x46: // SSID3
                                {
                                    index++;
                                    network_config.cmd_list[i].cmd_bit = index;
                                    sprintf(network_config.cmd_list[i].set_value, "%s", columns_value[1]);
                                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                                    break;
                                }
                                case 0x47: // SSID3 密码
                                {
                                    index++;
                                    network_config.cmd_list[i].cmd_bit = index;
                                    sprintf(network_config.cmd_list[i].set_value, "%s", columns_value[2]);
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
                        #elif BOARDTYPE == 9344
                        for (i = 0; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
                        {
                            // 初始化设备项
                            switch (network_config.cmd_list[i].cmd_word)
                            {
                                case 0x1F: // SSID3
                                {
                                    index++;
                                    network_config.cmd_list[i].cmd_bit = index;
                                    sprintf(network_config.cmd_list[i].set_value, "%s", columns_value[1]);
                                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                                    break;
                                }
                                case 0x22: // AP_CYPHER
                                {
                                    index++;
                                    network_config.cmd_list[i].cmd_bit = index;
                                    memcpy(network_config.cmd_list[i].set_value, "TKIP CCMP", strlen("TKIP CCMP"));
                                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s\"%s\"", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                                    break;
                                }
                                case 0x32: // SSID3 密码
                                {
                                    index++;
                                    network_config.cmd_list[i].cmd_bit = index;
                                    sprintf(network_config.cmd_list[i].set_value, "%s", columns_value[2]);
                                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                                    break;
                                }
                                case 0x35: // 2.4G
                                {
                                    index++;
                                    network_config.cmd_list[i].cmd_bit = index;
                                    memcpy(network_config.cmd_list[i].set_value, "0", strlen("0"));
                                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                                    break;
                                }
                                case 0x38: // 模式
                                {
                                    index++;
                                    network_config.cmd_list[i].cmd_bit = index;
                                    memcpy(network_config.cmd_list[i].set_value, "ap", strlen("ap"));
                                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                                    break;
                                }
                                case 0x3B: // 是否隐藏
                                {
                                    index++;
                                    network_config.cmd_list[i].cmd_bit = index;
                                    memcpy(network_config.cmd_list[i].set_value, "0", strlen("0"));
                                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                                    break;
                                }
                                case 0x3E: // 加密
                                {
                                    index++;
                                    network_config.cmd_list[i].cmd_bit = index;
                                    memcpy(network_config.cmd_list[i].set_value, "WPA", strlen("WPA"));
                                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                                    break;
                                }
                                case 0x41: // 加密
                                {
                                    index++;
                                    network_config.cmd_list[i].cmd_bit = index;
                                    memcpy(network_config.cmd_list[i].set_value, "3", strlen("3"));
                                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                                    break;
                                }
                                case 0x44: // 加密
                                {
                                    index++;
                                    network_config.cmd_list[i].cmd_bit = index;
                                    memcpy(network_config.cmd_list[i].set_value, "PSK", strlen("PSK"));
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
                        #endif
                        
                        #if BOARDTYPE == 6410
                        // 设置路由
                        if ((res = network_config.route_config(*network_config.serial_5350_fd, index)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "route_config failed!", res);
                            break;
                        }
                         // 发送重启命令
                        if ((res = network_config.send_msg_to_5350(*network_config.serial_5350_fd, "reboot", strlen("reboot"))) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_5350 failed", res);
                            break;
                        }
                        #elif BOARDTYPE == 5350 
                        
                        // 设置路由
                        if ((res = network_config.route_config2(index)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "route_config failed!", res);
                            break;
                        }
                        
                        sleep(1);
						/*
                        // 是设置生效
                        if ((res = network_config.config_route_take_effect()) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "config_route_take_effect failed", res);
                            break;
                        }
                        
                        // add ssid3_state
                        memset(columns_name[0], 0, sizeof(columns_name[0]));
                        memset(columns_value[0], 0, sizeof(columns_value[0]));
                        memcpy(columns_name[0], "ssid3_state", strlen("ssid3_state"));
						columns_value[0][0] = '1';
						insert_len = 1;
                        if ((res = nvram_interface.insert(RT5350_FREE_SPACE, 1, columns_name, columns_value, &insert_len)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_insert failed!", res);
                            break;
                        }
						*/
						
						system("reboot");
                        /*****************************************************************
                         *************** 发送到监控程序（monitor_application）   *********
                         *****************************************************************/
                        #if SSID3_MONITOR 
                        
                        #if LOCAL_SOCKET
                        // 创建客户端连接
                        if ((client_fd = communication_network.make_local_socket_client_link(TERMINAL_LOCAL_SOCKET_NAME)) < 0)
                        {
                            PERROR("make_client_link failed!\n");
                            res = client_fd;
                            client_fd = 0;
                            break;
                        }
                        #else
                        // 创建客户端连接
                        if ((client_fd = communication_network.make_client_link("127.0.0.1", TERMINAL_LOCAL_SERVER_PORT)) < 0)
                        {
                            PERROR("make_client_link failed!\n");
                            res = client_fd;
                            client_fd = 0;
                            break;
                        }
                        #endif
                        PRINT("client_fd = %d\n", client_fd);
                        for (i = 0; i < 3; i++)
                        {
                            if ((res = network_config.send_msg_to_pad(client_fd, 0x00, NULL, 0)) < 0)
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                                continue;
                            }
                            PRINT("after send_msg_to_pad\n");
                            if ((res = network_config.recv_msg_from_pad(client_fd, &monitor_and_base_msg)) < 0)
                            {
                                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg_from_pad failed", res);
                                sleep(1);
                                continue;
                            }
                            PRINT("after recv_msg_from_pad\n");
                            PRINT("monitor_and_base_msg.cmd = %d\n");
                            if (monitor_and_base_msg.cmd == 0x00)
                            {
                                close(client_fd);
                                client_fd = 0;
                            }
                            else 
                            {
                                continue;
                            }
                            
                        }
                        if ((res < 0) || (i == 3))
                        {
                            close(client_fd);
                            client_fd = 0;
                            res = 0;
                        }
                        
                        #endif
                        /*****************************************************************
                         *************** 发送到监控程序（monitor_application）   *********
                         *****************************************************************/
                        #elif BOARDTYPE == 9344
                        // 设置路由
                        if ((res = network_config.route_config(index)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "route_config failed!", res);
                            break;
                        }
                        
                        sleep(1);
                        system("/etc/ath/apdown");
                        system("/etc/ath/apup");
                        #endif
                        break;
                    }
                    else if (pad_and_6410_msg.cmd == 0x06) // 注销默认SSID
                    {
                        PRINT("pad_and_6410_msg.len = %d\n", pad_and_6410_msg.len);
                        if (pad_and_6410_msg.len != 35)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        if ((unsigned char)*network_config.pad_cmd != 0x00)  // PAD没有注册成功
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
                            res = NO_INIT_ERR;
                            break;
                        }
                        
                        memset(columns_name[0], 0, sizeof(columns_name[0]));
                        memset(columns_value[0], 0, sizeof(columns_value[0]));
                        memcpy(columns_name[0], "pad_sn", strlen("pad_sn"));
                        
                        #if BOARDTYPE == 6410 || BOARDTYPE == 9344
                        if ((res = database_management.select(1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);   
                            break;
                        }
                        #elif BOARDTYPE == 5350 
                        if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res); 
                            break;
                        }
                        if ((strcmp("\"\"", columns_value[0]) == 0) || (strlen(columns_value[0]) == 0))
                        {
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            sprintf(columns_value[0], "There is no (%s) record!", columns_name[0]);
                            res = NULL_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[0], res);
                            break;
                        }
                        #endif
                        PRINT("columns_value[0] = %s\n", columns_value[0]);
                        PRINT("pad_and_6410_msg.data = %s\n", pad_and_6410_msg.data);
                        
                        if (memcmp(columns_value[0], pad_and_6410_msg.data, strlen(columns_value[0])) != 0)
                        {
                            res = WRONGFUL_PAD_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "pad and base mismatching!", res);
                            break;
                        }
                        //#if BOARDTYPE == 5350
                        if ((res = network_config.send_msg_to_pad(accept_pad_fd, 0x00, NULL, 0)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                            break;
                        }
                        //#endif
                        
                        #if BOARDTYPE == 5350
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
                        #elif BOARDTYPE == 9344
                        system("cfg -r AP_SSID_3");
                        system("cfg -r PSK_KEY_3");
                        system("cfg -r AP_RADIO_ID_3");
                        system("cfg -r AP_MODE_3");
                        system("cfg -r AP_HIDESSID_3");
                        system("cfg -r AP_SECMODE_3");
                        system("cfg -r AP_WPA_3");
                        system("cfg -r AP_SECFILE_3");
                        system("cfg -c");
                        #endif
                        
                        #if BOARDTYPE == 6410
                        // 设置路由
                        if ((res = network_config.route_config(*network_config.serial_5350_fd, index)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "route_config failed!", res);
                            break;
                        }
                         // 发送重启命令
                        if ((res = network_config.send_msg_to_5350(*network_config.serial_5350_fd, "reboot", strlen("reboot"))) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_5350 failed", res);
                            break;
                        }
                        
                        #elif BOARDTYPE == 5350
                        /*
                        // 设置路由
                        if ((res = network_config.route_config(index)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "route_config failed!", res);
                            break;
                        }
                        */
                        
                        if ((res = network_config.route_config2(index)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "route_config failed!", res);
                            break;
                        }
                        sleep(1);

						/*
                        // 是设置生效
                        if ((res = network_config.config_route_take_effect()) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "config_route_take_effect failed", res);
                            break;
                        }
						*/
						system("reboot");
						#elif BOARDTYPE == 9344
                        system("/etc/ath/apdown");
                        system("/etc/ath/apup");
                        #endif
                        
						/*
                        // add ssid3_state
                        memset(columns_name[0], 0, sizeof(columns_name[0]));
                        memset(columns_value[0], 0, sizeof(columns_value[0]));
                        memcpy(columns_name[0], "ssid3_state", strlen("ssid3_state"));
						columns_value[0][0] = '0';
						insert_len = 1;
                        if ((res = nvram_interface.insert(RT5350_FREE_SPACE, 1, columns_name, columns_value, &insert_len)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_insert failed!", res);
                            break;
						}
						*/
						
                        break;
                    }
                    else if ((pad_and_6410_msg.cmd == 0x07) || (pad_and_6410_msg.cmd == 0x08) || 
                        (pad_and_6410_msg.cmd == 0x09) || (pad_and_6410_msg.cmd == 0x0A)) // 无线网络设置和网络设置
                    {
                        write_falg = 0;
                        if (pad_and_6410_msg.len != 36)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        
                        if ((unsigned char)*network_config.pad_cmd != 0x00)
                        {
                            res = NO_INIT_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
                            break;
                        }
                        *network_config.network_flag = 1;
                        
                        if ((res = network_config.send_msg_to_pad(accept_pad_fd, 0x00, NULL, 0)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                            break;
                        }
                        
                        
                        if ((res = network_config.network_settings(accept_pad_fd, pad_and_6410_msg.data[0], pad_and_6410_msg.cmd)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "network_config failed", res); 
                        }
                        
                        break;
                    }
                    else if (pad_and_6410_msg.cmd == 0x0B) // 网络设置询问
                    {
                        if (pad_and_6410_msg.len != 35)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        if ((unsigned char)*network_config.pad_cmd != 0x00)  // PAD没有注册成功
                        {
                            res = NO_INIT_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
                            break;
                        }
                        
                        memset(columns_name[0], 0, sizeof(columns_name[0]));
                        memset(columns_value[0], 0, sizeof(columns_value[0]));
                        memcpy(columns_name[0], "pad_sn", strlen("pad_sn"));
                        #if BOARDTYPE == 6410 || BOARDTYPE == 9344 
                        if ((res = database_management.select(1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);   
                            break;
                        }
                        #elif BOARDTYPE == 5350
                        if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);
                            break;
                        }
                        if ((strcmp("\"\"", columns_value[0]) == 0) || (strlen(columns_value[0]) == 0))
                        {
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            sprintf(columns_value[0], "There is no (%s) record!", columns_name[0]);
                            res = NULL_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[0], res); 
                            break;
                        }
                        #endif
                        PRINT("columns_value[0] = %s\n", columns_value[0]);
                        PRINT("pad_and_6410_msg.data = %s\n", pad_and_6410_msg.data);
                        
                        if (memcmp(columns_value[0], pad_and_6410_msg.data, strlen(columns_value[0])) != 0)
                        {
                            res = WRONGFUL_PAD_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "pad and base mismatching!", res);
                            break;
                        }
                        
                        #if BOARDTYPE == 6410
                        if ((res = network_config.send_msg_to_5350(*network_config.serial_5350_fd, "nvram_get 2860 wanConnectionMode", strlen("nvram_get 2860 wanConnectionMode"))) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_5350 failed", res); 
                            break;
                        }
                        
                        if ((res = network_config.recv_msg_from_5350(*network_config.serial_5350_fd, &_6410_and_5350_msg)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg_from_5350 failed", res); 
                            break;
                        }
                        if (_6410_and_5350_msg.data[0] == 0xFF)
                        {
                            res = DATA_ERR;
                            break;
                        }
                        memcpy(buf_tmp, _6410_and_5350_msg.data, strlen(_6410_and_5350_msg.data));
                        free(_6410_and_5350_msg.data);
                        _6410_and_5350_msg.data = NULL;
                        common_tools.del_quotation_marks(buf_tmp);
                        #elif BOARDTYPE == 5350 
                        memset(columns_name[0], 0, sizeof(columns_name[0]));
                        memset(columns_value[0], 0, sizeof(columns_value[0]));
                        memset(buf_tmp, 0, sizeof(buf_tmp));
                        memcpy(columns_name[0], "wanConnectionMode", strlen("wanConnectionMode"));
                        if ((res = nvram_interface.select(NVRAM_RT5350, 1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);  
                            break;
                        }
                        PRINT("columns_value[0] = %s\n", columns_value[0]);
                        if ((strcmp("\"\"", columns_value[0]) == 0) || (strlen(columns_value[0]) == 0))
                        {
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            sprintf(columns_value[0], "There is no (%s) record!", columns_name[0]);
                            res = NULL_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[0], res); 
                            break;
                        }
                        memcpy(buf_tmp, columns_value[0], strlen(columns_value[0]));
                        #elif BOARDTYPE == 9344 
                        if ((res = common_tools.get_cmd_out("cfg -e WAN_MODE", buf_tmp, sizeof(buf_tmp))) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_cmd_out failed!", res); 
                            break;
                        }
                        #endif
                        
                        
                        PRINT("connect mode = %s\n", buf_tmp);
                        memset(send_buf, 0, sizeof(send_buf));
                        #if BOARDTYPE == 5350
                        if (memcmp(buf_tmp, "STATIC", strlen("STATIC")) == 0)
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
                        else if (memcmp(buf_tmp, "DHCP", strlen("DHCP")) == 0)
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
                        else if (memcmp(buf_tmp, "PPPOE", strlen("PPPOE")) == 0)
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
                        if (memcmp(buf_tmp, "static", strlen("static")) == 0)
                        {   
                            strcpy(send_buf, "mode:stat,");
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
                        else if (memcmp(buf_tmp, "dhcp", strlen("dhcp")) == 0)
                        {
                            strcpy(send_buf, "mode:dhcp,");
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
                        else if (memcmp(buf_tmp, "pppoe", strlen("pppoe")) == 0)
                        {
                            strcpy(send_buf, "mode:pppoe,");
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
                        #endif
                        else
                        {
                            res = DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data err", res);
                            break;
                        }
                        memset(buf_tmp, 0, sizeof(buf_tmp));
                        
                        for (i = 0, j = 1; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
                        {
                            if (j == network_config.cmd_list[i].cmd_bit)
                            {
                                PRINT("network_config.cmd_list[%d].cmd_bit = %02X, index = %d, j = %d\n", i, network_config.cmd_list[i].cmd_bit, index, j);
                                memset(buf_tmp, 0, sizeof(buf_tmp));
                                #if BOARDTYPE == 6410
                                if (_6410_and_5350_msg.data != NULL)
                                {
                                    free(_6410_and_5350_msg.data);
                                    _6410_and_5350_msg.data = NULL;
                                }
                                // 发送命令到5350
                                if ((res = network_config.send_msg_to_5350(*network_config.serial_5350_fd, network_config.cmd_list[i].get_cmd, strlen(network_config.cmd_list[i].get_cmd))) < 0)
                                {
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_5350 failed", res);
                                    break;
                                }
                                
                                // 接收5350发送的数据
                                if ((res = network_config.recv_msg_from_5350(*network_config.serial_5350_fd, &_6410_and_5350_msg)) < 0)
                                {
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg_from_5350 failed", res);
                                    break;
                                }
                                
                                if (_6410_and_5350_msg.data[0] == 0xFF)
                                {
                                    res = DATA_ERR;
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data err", res);
                                    break;
                                }
                                
                                memcpy(buf_tmp, _6410_and_5350_msg.data, strlen(_6410_and_5350_msg.data));
                                if (strlen(buf_tmp) > 2)
                                {
                                    common_tools.del_quotation_marks(buf_tmp);
                                }
                                #elif BOARDTYPE == 5350 
                                memset(buf_tmp, 0, sizeof(buf_tmp));
                                
                                if ((res = nvram_interface.select(NVRAM_RT5350, 1, (char (*)[30])network_config.cmd_list[i].cmd_key, (char (*)[100])buf_tmp)) < 0)
                                {
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);  
                                    break;
                                }
                                if (strlen(buf_tmp) == 0)
                                {
                                    strcpy(buf_tmp, "\"\"");
                                }
                                PRINT("buf_tmp = %s\n", buf_tmp);
                                #elif BOARDTYPE == 9344 
                                snprintf(config_cmd, sizeof(config_cmd), "cfg -e %s", network_config.cmd_list[i].cmd_key);
                                if ((res = common_tools.get_cmd_out("config_cmd", buf_tmp, sizeof(buf_tmp))) < 0)
                                {
                                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_cmd_out failed!\n", res); 
                                    break;
                                }
                                #endif
                                PRINT("network_config.cmd_list[%d].cmd_key = %s, buf_tmp = %s\n", i, network_config.cmd_list[i].cmd_key, buf_tmp);
                                switch (network_config.cmd_list[i].cmd_word)
                                {
                                    case 0x15:
                                    {
                                        strcat(send_buf, "ip:");
                                        strncat(send_buf, buf_tmp, strlen(buf_tmp));
                                        strcat(send_buf, ",");
                                        j++;
                                        break;
                                    }
                                    case 0x16:
                                    {
                                        strcat(send_buf, "mask:");
                                        strncat(send_buf, buf_tmp, strlen(buf_tmp));
                                        strcat(send_buf, ",");
                                        j++;
                                        break;
                                    }
                                    case 0x17:
                                    {
                                        strcat(send_buf, "gateway:");
                                        strncat(send_buf, buf_tmp, strlen(buf_tmp));
                                        strcat(send_buf, ",");
                                        j++;
                                        break;
                                    }
                                    case 0x18:
                                    {
                                        strcat(send_buf, "dns1:");
                                        strncat(send_buf, buf_tmp, strlen(buf_tmp));
                                        strcat(send_buf, ",");
                                        j++;
                                        break;
                                    }
                                    case 0x1A:
                                    {
                                        strcat(send_buf, "pppoe_user:");
                                        strncat(send_buf, buf_tmp, strlen(buf_tmp));
                                        strcat(send_buf, ",");
                                        j++;
                                        break;
                                    }
                                    case 0x1B:
                                    {
                                        strcat(send_buf, "pppoe_pass:");
                                        strncat(send_buf, buf_tmp, strlen(buf_tmp));
                                        strcat(send_buf, ",");
                                        j++;
                                        break;
                                    }
                                    case 0x1E:
                                    {
                                        strcat(send_buf, "ssid2:");
                                        strncat(send_buf, buf_tmp, strlen(buf_tmp));
                                        strcat(send_buf, ",");
                                        j++;
                                        break;
                                    }
                                    case 0x31:
                                    {
                                        strcat(send_buf, "pass:");
                                        strncat(send_buf, buf_tmp, strlen(buf_tmp));
                                        strcat(send_buf, ",");
                                        j++;
                                        break;
                                    }
                                    case 0x3D:
                                    {
                                        strcat(send_buf, "auth:");
                                        strncat(send_buf, buf_tmp, strlen(buf_tmp));
                                        j++;
                                        break;
                                    }
                                    case 0x40:
                                    {
                                        strncat(send_buf, buf_tmp, strlen(buf_tmp));
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
                        if ((res = network_config.send_msg_to_pad(accept_pad_fd, 0x00, send_buf, strlen(send_buf))) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                        }
                        break;
                    }
                    else if (pad_and_6410_msg.cmd == 0x0C) //恢复出厂
                    {
                        if (pad_and_6410_msg.len != 35)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        
                        #if 0
                        // sn对比
                        memset(columns_name[0], 0, sizeof(columns_name[0]));
                        memset(columns_value[0], 0, sizeof(columns_value[0]));                  
                        memcpy(columns_name[0], "pad_sn", strlen("pad_sn")); 
                        
                        #if BOARDTYPE == 5350
                        if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res); 
                            break;
                        }
                        #elif BOARDTYPE == 9344
                        if ((res = database_management.select(1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res); 
                            break;
                        }
                        #endif
                        if ((strcmp("\"\"", columns_value[0]) == 0) || (strlen(columns_value[0]) == 0))
                        {
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            sprintf(columns_value[0], "There is no (%s) record!", columns_name[0]);
                            res = NULL_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[0], res); 
                            break;
                        }
                        
                        PRINT("columns_value[0] = %s, pad_and_6410_msg.data = %s\n", columns_value[0], pad_and_6410_msg.data);
                        if (memcmp(columns_value[0], pad_and_6410_msg.data, strlen(columns_value[0])) != 0)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res); 
                            break;
                        }
                        #endif
                        
                        if ((res = network_config.send_msg_to_pad(accept_pad_fd, 0x00, NULL, 0)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                            break;
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
                        system("cfg -x");
                        system("/etc/ath/apdown");
                        system("/etc/ath/apup");
                        #endif
                        
                        break;
                    }
                    else if (pad_and_6410_msg.cmd == 0x0D) //查看Base版本信息
                    {
                        if (pad_and_6410_msg.len != 35)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        
                        #if 0
                        // sn对比
                        memset(columns_name[0], 0, sizeof(columns_name[0]));
                        memset(columns_value[0], 0, sizeof(columns_value[0]));                  
                        memcpy(columns_name[0], "pad_sn", strlen("pad_sn")); 
                        
                        #if BOARDTYPE == 5350
                        if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);  
                            break;
                        }
                        #elif BOARDTYPE == 9344 
                        if ((res = database_management.select(1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);  
                            break;
                        }
                        #endif
                        if ((strcmp("\"\"", columns_value[0]) == 0) || (strlen(columns_value[0]) == 0))
                        {
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            sprintf(columns_value[0], "There is no (%s) record!", columns_name[0]);
                            res = NULL_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[0], res);
                            break;
                        }
                        
                        PRINT("columns_value[0] = %s, pad_and_6410_msg.data = %s\n", columns_value[0], pad_and_6410_msg.data);
                        if (memcmp(columns_value[0], pad_and_6410_msg.data, strlen(columns_value[0])) != 0)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
                            break;
                        }
                        #endif
                        
                        if ((res = common_tools.get_cmd_out("cat /proc/version", buf_tmp, sizeof(buf_tmp))) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_cmd_out failed!", res);
                            break;
                        }
                        
                        if ((res = network_config.send_msg_to_pad(accept_pad_fd, 0x00, buf_tmp, strlen(buf_tmp))) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                            break;
                        }
                        break;
                    }
                    #if CTSI_SECURITY_SCHEME == 2
                    else if (pad_and_6410_msg.cmd == 0x0E) // 获取身份令牌
                    {
                        if (pad_and_6410_msg.len != 35)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        
                        if ((unsigned char)*network_config.pad_cmd != 0x00)  // PAD没有注册成功
                        {
                            res = NO_INIT_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
                            break;
                        }
                        
                        // sn对比
                        memset(columns_name[0], 0, sizeof(columns_name[0]));
                        memset(columns_value[0], 0, sizeof(columns_value[0]));                  
                        memcpy(columns_name[0], "pad_sn", strlen("pad_sn")); 
                        
                        #if BOARDTYPE == 5350
                        if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);  
                            break;
                        }
                        #elif BOARDTYPE == 9344 
                        if ((res = database_management.select(1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);  
                            break;
                        }
                        #endif
                        if ((strcmp("\"\"", columns_value[0]) == 0) || (strlen(columns_value[0]) == 0))
                        {
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            sprintf(columns_value[0], "There is no (%s) record!", columns_name[0]);
                            res = NULL_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[0], res);
                            break;
                        }
                        
                        PRINT("columns_value[0] = %s, pad_and_6410_msg.data = %s\n", columns_value[0], pad_and_6410_msg.data);
                        if (memcmp(columns_value[0], pad_and_6410_msg.data, strlen(columns_value[0])) != 0)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
                            break;
                        }
                        
                        if ((res = terminal_authentication.return_device_token(device_token)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "return_device_token failed!", res);
                            break;
                        }
                        
                        if ((res = network_config.send_msg_to_pad(accept_pad_fd, 0x00, device_token, 16)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                            break;
                        }
                        memset(device_token, 0, sizeof(device_token));
                        break;
                    }
                    else if (pad_and_6410_msg.cmd == 0x0F) // 获取位置令牌
                    {
                        if (pad_and_6410_msg.len != 35)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        
                        if ((unsigned char)*network_config.pad_cmd != 0x00)  // PAD没有注册成功
                        {
                            res = NO_INIT_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
                            break;
                        }
                        
                        // sn对比
                        memset(columns_name[0], 0, sizeof(columns_name[0]));
                        memset(columns_value[0], 0, sizeof(columns_value[0]));                  
                        memcpy(columns_name[0], "pad_sn", strlen("pad_sn")); 
                        
                        #if BOARDTYPE == 5350
                        if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);  
                            break;
                        }
                        #elif BOARDTYPE == 9344 
                        if ((res = database_management.select(1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);  
                            break;
                        }
                        #endif
                        if ((strcmp("\"\"", columns_value[0]) == 0) || (strlen(columns_value[0]) == 0))
                        {
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            sprintf(columns_value[0], "There is no (%s) record!", columns_name[0]);
                            res = NULL_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[0], res);
                            break;
                        }
                        
                        PRINT("columns_value[0] = %s, pad_and_6410_msg.data = %s\n", columns_value[0], pad_and_6410_msg.data);
                        if (memcmp(columns_value[0], pad_and_6410_msg.data, strlen(columns_value[0])) != 0)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
                            break;
                        }
                        
                        if ((res = terminal_authentication.return_position_token(position_token)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "return_position_token failed!", res);
                            break;
                        }
                        
                        if ((res = network_config.send_msg_to_pad(accept_pad_fd, 0x00, position_token, 16)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                            break;
                        }
                        memset(position_token, 0, sizeof(position_token));
                        break;
                    }
                    else if (pad_and_6410_msg.cmd == 0x50) // 重新生成身份令牌
                    {
                        if (pad_and_6410_msg.len != 35)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        
                        if ((unsigned char)*network_config.pad_cmd != 0x00)
                        {
                            res = NO_INIT_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
                            break;
                        }
                        
                        // sn对比
                        memset(columns_name[0], 0, sizeof(columns_name[0]));
                        memset(columns_value[0], 0, sizeof(columns_value[0]));                  
                        memcpy(columns_name[0], "pad_sn", strlen("pad_sn")); 
                        
                        #if BOARDTYPE == 5350
                        if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);  
                            break;
                        }
                        #elif BOARDTYPE == 9344 
                        if ((res = database_management.select(1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);  
                            break;
                        }
                        #endif
                        if ((strcmp("\"\"", columns_value[0]) == 0) || (strlen(columns_value[0]) == 0))
                        {
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            sprintf(columns_value[0], "There is no (%s) record!", columns_name[0]);
                            res = NULL_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[0], res);
                            break;
                        }
                        
                        PRINT("columns_value[0] = %s, pad_and_6410_msg.data = %s\n", columns_value[0], pad_and_6410_msg.data);
                        if (memcmp(columns_value[0], pad_and_6410_msg.data, strlen(columns_value[0])) != 0)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
                            break;
                        }
                        
                        if ((res = terminal_authentication.rebuild_device_token(device_token)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "rebuild_device_token failed!", res);
                            break;
                        }
                        
                        if ((res = network_config.send_msg_to_pad(accept_pad_fd, 0x00, device_token, 16)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                            break;
                        }
                        memset(device_token, 0, sizeof(device_token));
                        break;
                    }
                    else if (pad_and_6410_msg.cmd == 0x51) // 重新生成位置令牌
                    {
                        if (pad_and_6410_msg.len != 35)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        
                        if ((unsigned char)*network_config.pad_cmd != 0x00)  // PAD没有注册成功
                        {
                            res = NO_INIT_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "no init!", res);
                            break;
                        }
                        
                        // sn对比
                        memset(columns_name[0], 0, sizeof(columns_name[0]));
                        memset(columns_value[0], 0, sizeof(columns_value[0]));                  
                        memcpy(columns_name[0], "pad_sn", strlen("pad_sn")); 
                        
                        #if BOARDTYPE == 5350
                        if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);  
                            break;
                        }
                        #elif BOARDTYPE == 9344
                        if ((res = database_management.select(1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);  
                            break;
                        }
                        #endif
                        if ((strcmp("\"\"", columns_value[0]) == 0) || (strlen(columns_value[0]) == 0))
                        {
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            sprintf(columns_value[0], "There is no (%s) record!", columns_name[0]);
                            res = NULL_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[0], res);
                            break;
                        }
                        
                        PRINT("columns_value[0] = %s, pad_and_6410_msg.data = %s\n", columns_value[0], pad_and_6410_msg.data);
                        if (memcmp(columns_value[0], pad_and_6410_msg.data, strlen(columns_value[0])) != 0)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
                            break;
                        }
                        
                        if ((res = terminal_authentication.rebuild_position_token(position_token)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "rebuild_position_token failed!", res);
                            break;
                        }
                        
                        if ((res = network_config.send_msg_to_pad(accept_pad_fd, 0x00, position_token, 16)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                            break;
                        }
                        memset(position_token, 0, sizeof(position_token));
                        break;
                    }
                    #endif
                    else if (pad_and_6410_msg.cmd == 0x52) //0x52: 查看WAN口
                    {
                        if (pad_and_6410_msg.len != 35)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        
                        // sn对比
                        memset(columns_name[0], 0, sizeof(columns_name[0]));
                        memset(columns_value[0], 0, sizeof(columns_value[0]));                  
                        memcpy(columns_name[0], "pad_sn", strlen("pad_sn")); 
                        
                        #if BOARDTYPE == 5350
                        if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);  
                            break;
                        }
                        #elif BOARDTYPE == 9344 
                        if ((res = database_management.select(1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);  
                            break;
                        }
                        #endif
                        if ((strcmp("\"\"", columns_value[0]) == 0) || (strlen(columns_value[0]) == 0))
                        {
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            sprintf(columns_value[0], "There is no (%s) record!", columns_name[0]);
                            res = NULL_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[0], res);
                            break;
                        }
                        
                        PRINT("columns_value[0] = %s, pad_and_6410_msg.data = %s\n", columns_value[0], pad_and_6410_msg.data);
                        if (memcmp(columns_value[0], pad_and_6410_msg.data, strlen(columns_value[0])) != 0)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
                            break;
                        }
                        
                        if ((res = network_config.get_wan_state()) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_wan_state failed!", res);
                            break;
                        }
                        break;
                    }
                    else if (pad_and_6410_msg.cmd == 0x53) //0x53: 取消当前操作
                    {
                        if (pad_and_6410_msg.len != 35)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        
                        // sn对比
                        memset(columns_name[0], 0, sizeof(columns_name[0]));
                        memset(columns_value[0], 0, sizeof(columns_value[0]));                  
                        memcpy(columns_name[0], "pad_sn", strlen("pad_sn")); 
                        
                        #if BOARDTYPE == 5350
                        if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);  
                            break;
                        }
                        #elif BOARDTYPE == 9344 
                        if ((res = database_management.select(1, columns_name, columns_value)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);  
                            break;
                        }
                        #endif
                        if ((strcmp("\"\"", columns_value[0]) == 0) || (strlen(columns_value[0]) == 0))
                        {
                            memset(columns_value[0], 0, sizeof(columns_value[0]));
                            sprintf(columns_value[0], "There is no (%s) record!", columns_name[0]);
                            res = NULL_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[0], res);
                            break;
                        }
                        
                        PRINT("columns_value[0] = %s, pad_and_6410_msg.data = %s\n", columns_value[0], pad_and_6410_msg.data);
                        if (memcmp(columns_value[0], pad_and_6410_msg.data, strlen(columns_value[0])) != 0)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
                            break;
                        }
                        
                        if (terminal_dev_register->config_now_flag == 1)
                        {
                            #if BOARDTYPE == 5350
                            pthread_mutex_lock(&nvram_interface.mutex);
                            #endif
                            pthread_cancel(terminal_dev_register->pad_cmd_handle_id);
                            PRINT("after pthread_cancel!\n");
                            
                            #if BOARDTYPE == 5350
                            pthread_mutex_unlock(&nvram_interface.mutex);
                            #endif
                            // 判断锁状态(需要跟踪)
                            if(pthread_mutex_trylock(&network_config.recv_mutex) == EBUSY)
                            {
                                PRINT("pad_cmd_handle pthread lock now!\n");
                                pthread_mutex_unlock(&network_config.recv_mutex);
                            }
                            else
                            {
                                PRINT("serial pthread lock now!\n");
                                pthread_mutex_unlock(&network_config.recv_mutex);
                            }
                        }
                        
                        /*
                        if ((res = network_config.send_msg_to_pad(accept_pad_fd, 0x00, NULL, 0)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                            break;
                        }
                        */
                        break;
                    }
                    else // 01 02 03 
                    {
                        if (pad_and_6410_msg.len != 36)
                        {
                            res = P_DATA_ERR;
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf len err!", res);
                            break;
                        }
                        
                        if ((res = network_config.send_msg_to_pad(accept_pad_fd, 0x00, NULL, 0)) < 0)
                        {
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                            break;
                        }
                        
                        pthread_mutex_lock(&terminal_dev_register->mutex);
                        
                        terminal_dev_register->fd = accept_pad_fd;
                        terminal_dev_register->cmd_count = pad_and_6410_msg.data[0];
                        terminal_dev_register->cmd_word = pad_and_6410_msg.cmd;
                        
                        pthread_cond_signal(&terminal_dev_register->cond);
                        
                        pthread_mutex_unlock(&terminal_dev_register->mutex);
                        
                        
                        pthread_mutex_lock(&terminal_dev_register->mutex);
                        terminal_dev_register->mutex_lock_flag = 1;
                        pthread_cond_wait(&terminal_dev_register->cond, &terminal_dev_register->mutex);
                        
                        if (terminal_dev_register->pad_cmd_handle_id == 0)
                        {
                            // 线程创建失败
                            res = PTHREAD_CREAT_ERR;
                        }
                        pthread_mutex_unlock(&terminal_dev_register->mutex);
                        
                        break;
                    } 
                }
            }
        }
        #if BOARDTYPE == 5350
        // 等于1时说明此时正在设置
        if ((terminal_dev_register->config_now_flag != 1) || (cmd == 0x53))
        {
            network_config.init_cmd_list();  // 初始化命令结构体
        }
        #endif
        
        PRINT("network_flag = %d *network_config.server_pad_fd = %d\n", *network_config.network_flag, *network_config.server_pad_fd);
        PRINT("*network_config.network_flag = %d cmd = %d\n", *network_config.network_flag, cmd);
        if ((cmd == 0x07) || (cmd == 0x08) || (cmd == 0x09) || (cmd == 0x0A))
        {
            memcpy(port, common_tools.config->pad_server_port2, strlen(common_tools.config->pad_server_port2));
            // 建立链接
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
            if (*network_config.server_pad_fd < 0)
            {
                res = common_tools.get_errno('P', *network_config.server_pad_fd);
            }
            else
            {
                if (res == 0)
                {
                    // 发送
                    if ((res = network_config.send_msg_to_pad(*network_config.server_pad_fd, 0x00, NULL, 0)) < 0)
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);  
                    }
                    else
                    {
                        PRINT("send success!\n");
                    }
                }
            }
        }
        PRINT("network_flag = %d server_pad_fd = %d\n", *network_config.network_flag, *network_config.server_pad_fd);
        PRINT("*network_config.cmd = %02X write_falg = %d\n", (unsigned char)(*network_config.cmd), write_falg);
        if ((cmd == 0x04) && ((unsigned char)*network_config.cmd != 0xFF) && (write_falg == 1)) // 插入数据库 // *network_config.cmd  等于如下值时 0x00 0xFC 0xFB 0xFD 0xFE，插入数据库
        {
            memset(columns_name[0], 0, sizeof(columns_name[0]));
            memset(columns_value[0], 0, sizeof(columns_value[0]));
            memcpy(columns_name[0], "register_state", strlen("register_state")); 
            #if BOARDTYPE == 6410 || BOARDTYPE == 9344
            memcpy(columns_value[0], network_config.cmd, sizeof(*network_config.cmd));
            insert_len = sizeof(*network_config.cmd);
            #elif BOARDTYPE == 5350
            sprintf(columns_value[0], "%d", (unsigned char)*network_config.cmd);
            insert_len = strlen(columns_value[0]);
            PRINT("columns_value[0] = %s\n", columns_value[0]);
            #endif
            
            for (i = 0; i < common_tools.config->repeat; i++)
            {
                #if BOARDTYPE == 6410 || BOARDTYPE == 9344
                if ((ret = database_management.insert(1, columns_name, columns_value, &insert_len)) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_insert failed!", ret);
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
        PRINT("res = %d\n", res);
        if (res < 0)
        {
            if (buf != NULL)
            {
                free(buf);
                buf = NULL;
            }
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
            //if ((*network_config.network_flag == 1) && (*network_config.server_pad_fd > 0))
            if (*network_config.server_pad_fd > 0)
            {
                fd = *network_config.server_pad_fd;
                PRINT("pad is server\n");
            }
            else
            {
                PRINT("pad is client\n");
                fd = accept_pad_fd;
            }
            if ((res = network_config.send_msg_to_pad(fd, 0xFF, buf, strlen(buf))) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed!", res);
                PRINT("send failed! %d %d %s\n", res, errno, strerror(errno));  
            }
            else
            {
                PRINT("send success\n");
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send success!", 0);
            }
        }
        close(accept_pad_fd);
        if (*network_config.server_pad_fd > 0)
        {
            close(*network_config.server_pad_fd);
            *network_config.server_pad_fd = 0;
        }
        *network_config.network_flag = 0;
        
        memset(buf_tmp, 0, sizeof(buf_tmp));
        memset(columns_name, 0, sizeof(columns_name));
        memset(columns_value, 0, sizeof(columns_value));
        memset(send_buf, 0, sizeof(send_buf));
        memset(port, 0, sizeof(port));
        
        // 资源释放
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
        cmd = 0;
        write_falg = 1;
        index = 0;
        gettimeofday(&tpend, NULL);                 
        common_tools.work_sum->work_failed_sum++;
        common_tools.make_menulist(start_time, tpstart, tpend);
                                    
        common_tools.work_sum->work_sum++;
       
        network_config.pthread_flag = 0;
        pthread_mutex_unlock(&config_mutex);
    }
    return (void *)0;
}

int main(int argc, char ** argv)
{    
    // 信号捕获和处理
    signal(SIGHUP, signal_handle);
    signal(SIGINT, signal_handle);
    signal(SIGQUIT, signal_handle);
    
    signal(SIGABRT, signal_handle);
    //signal(SIGBUS, signal_handle);
    
    signal(SIGPIPE, signal_handle);
    signal(SIGALRM, signal_handle);
    signal(SIGTERM, signal_handle);
    
    signal(SIGPROF, signal_handle);
    signal(SIGVTALRM, signal_handle);
    signal(SIGCLD, signal_handle);
    signal(SIGPWR, signal_handle);
    signal(SIGCHLD, signal_handle);
    
    strcpy(common_tools.argv0, argv[0]);
    
    int res = 0;
    
    pthread_t pthread_pad_socket_monitor_id, pthread_manage_id,
    #if BOARDTYPE == 5350
    pthread_pad_serial_monitor_id;
    #elif BOARDTYPE == 9344
    pthread_pad_usb_monitor_id;
    #endif
    
    common_tools.work_sum->work_sum = 1;
	start_time = time(0);
	network_config.init_cmd_list();
	
	// 命令行分析
    if (argc != 1)
    {
        if ((res = analyse_command_line(argc, argv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "analyse_command_line failed", res); 
            return res;
        }
        return 0;
    }
    
    // 初始化 “终端初始化” 运行环境
    if ((res = network_config.init_env()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "init_env failed", res); 
        return res;
    }
    
    // 终端初始化启动提示
    #if PRINT_DEBUG
    PRINT("terminal_init %s (%s %s)\n", TERMINAL_VERSION, __DATE__, __TIME__);
    PRINT("[terminal_init init env success!]\n");
    #else
    printf("[%s]%s["__FILE__"][%s][%05d] [terminal_init %s (%s %s)]\n", common_tools.argv0, common_tools.get_datetime_buf(), __FUNCTION__, __LINE__, TERMINAL_VERSION, __DATE__, __TIME__);
    printf("[%s]%s["__FILE__"][%s][%05d] [terminal_init init env success!]\n", common_tools.argv0, common_tools.get_datetime_buf(), __FUNCTION__, __LINE__);
    printf("[%s]%s["__FILE__"][%s][%05d] [terminal_init is running...]\n", common_tools.argv0, common_tools.get_datetime_buf(), __FUNCTION__, __LINE__);
    #endif
    
    #if CTSI_SECURITY_SCHEME == 2
     // 用户登录：当base上电时，检测是否终端初始化成功，当成功时，主动向平台请求设备令牌，保存在本地
    char device_token[128] = {0};
    char column_name[1][30] = {"register_state"};
    char column_value[1][100] = {0};
    int i = 0;
    
    // 查询状态
    for (i = 0; i < common_tools.config->repeat; i++)
    {
        #if BOARDTYPE == 6410 || BOARDTYPE == 9344
        if ((res = database_management.select(1, column_name, column_value)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);           
            continue;
        }
        PRINT("column_value[0] = %02X %02X %02X %02X, *network_config.pad_cmd = %02X\n", column_value[0][0], column_value[0][1], column_value[0][2], column_value[0][3], (unsigned char)*network_config.pad_cmd);
        #elif BOARDTYPE == 5350
        if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, column_name, column_value)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);           
            continue;
        }
        if ((strcmp("\"\"", column_value[0]) == 0) || (strlen(column_value[0]) == 0))
        {
            memset(column_value[0], 0, sizeof(column_value[0]));
            sprintf(column_value[0], "There is no (%s) record!", column_name[0]);
            res = NULL_ERR;
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, column_value[0], res); 
            break;
        }
        PRINT("columns_value[0] = %s\n", column_value[0]);
        #endif
        break;
    }
    
    if (res == 0)
    {
        if (memcmp("0", column_value[0], strlen(column_value[0])) == 0)
        {
            for (i = 0; i < common_tools.config->repeat; i++)
            {
                if ((res = terminal_authentication.rebuild_device_token(device_token)) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "rebuild_device_token failed", res); 
                    continue;
                }
                break;
            }
        }
    }
    #endif
    
    struct s_terminal_dev_register terminal_dev_register;
    memset(&terminal_dev_register, 0, sizeof(struct s_terminal_dev_register));
    
    pthread_mutex_init(&network_config.recv_mutex, NULL);
    pthread_mutex_init(&terminal_dev_register.mutex, NULL);
    #if BOARDTYPE == 5350
    pthread_mutex_init(&nvram_interface.mutex, NULL);
    #endif
    pthread_cond_init(&terminal_dev_register.cond, NULL);
    
    // 初始化互斥锁、创建线程
    pthread_mutex_init(&config_mutex, NULL);
    
    pthread_create(&pthread_pad_socket_monitor_id, NULL, (void*)pad_socket_monitor, (void *)&terminal_dev_register); 
    PRINT("pthread_create (pad_socket_monitor) success!\n");
    #if BOARDTYPE == 5350
    pthread_create(&pthread_pad_serial_monitor_id, NULL, (void*)pad_serial_monitor, (void *)&terminal_dev_register);
    PRINT("pthread_create (pad_serial_monitor) success!\n");
    #elif BOARDTYPE == 9344
    pthread_create(&pthread_pad_usb_monitor_id, NULL, (void*)pad_usb_monitor, (void *)&terminal_dev_register);
    PRINT("pthread_create (pad_usb_monitor) success!\n");
    #endif
    pthread_create(&pthread_manage_id, NULL, (void*)pthread_manage, (void *)&terminal_dev_register);
    PRINT("pthread_create (pthread_manage) success!\n");
    
    pthread_join(pthread_pad_socket_monitor_id, NULL);
    #if BOARDTYPE == 5350
    pthread_join(pthread_pad_serial_monitor_id, NULL);
    #elif BOARDTYPE == 9344
    pthread_join(pthread_pad_usb_monitor_id, NULL);
    #endif
    pthread_join(pthread_manage_id, NULL);
    pthread_mutex_destroy(&config_mutex);
    pthread_mutex_destroy(&terminal_dev_register.mutex);
    #if BOARDTYPE == 5350
    pthread_mutex_destroy(&nvram_interface.mutex);
    #endif
    pthread_cond_destroy(&terminal_dev_register.cond);
    return 0;
}


