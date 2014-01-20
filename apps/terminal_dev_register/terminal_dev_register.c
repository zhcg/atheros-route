/*************************************************************************
    > File Name: terminal_dev_register.c
    > Author: yangjilong
    > mail1: yangjilong65@163.com
    > mail2: yang.jilong@handaer.com
    > Created Time: 2013年11月08日 星期五 09时04分26秒
**************************************************************************/
#include "network_config.h"
#include "request_cmd.h"
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
    #if BOARDTYPE == 9344
    else if (memcmp(argv[1], "-d", strlen("-d")) == 0) // 恢复出厂设置
    {
        // 1.情况数据库
        if ((res = database_management.clear()) < 0)
        {
            PRINT("sqlite3_clear_table failed!\n");
            return res;
        }
        
        // 2.取消绑定
        request_cmd.cancel_mac_and_ip_bind();

        // 3.
        system("cfg -x");
        sleep(1);
        system("reboot");
    }
    #endif
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
        PRINT("usage1: terminal_dev_register [option]\n");
        PRINT("usage2: terminal_dev_register [option] [column_name column_value]\n");
        PRINT("options:\n");
        PRINT("\t-c: 清空数据表\n");
        PRINT("\t-d: 恢复出厂设置\n");
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
        case 19:
        {
            PRINT("SIGSTOP sig no:%d; sig info:停止进程的执行\n", sig);
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
        if (terminal_dev_register->transmission_mode == 0)
        {
            PRINT("fd = %d\n", fd);
            close(fd);
        }
        #elif BOARDTYPE == 9344
        PRINT("before close! fd = %d terminal_dev_register->fd = %d\n", fd, terminal_dev_register->fd);
        shutdown(fd, SHUT_RDWR);
        PERROR("after shutdown\n");
        close(fd);
        #endif
        fd = *network_config.server_pad_fd;
    }

    if ((res == 0) && (ret == 0) && (*network_config.network_flag == 1)) // 说明此时是网络设置
    {
        if ((res = network_config.send_msg_to_pad(fd, 0x00, NULL, 0)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
            goto EXIT;
        }
    }
    else if ((res == 0) && (ret == 0))
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

        #if 0 // 启动CACM
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
    if (terminal_dev_register->transmission_mode == 0)
    {
        close(fd);
    }
    else // 串口时
    {
        terminal_dev_register->transmission_mode = 0;
    }
    #elif BOARDTYPE == 9344
    PRINT("before close! fd = %d terminal_dev_register->fd = %d\n", fd, terminal_dev_register->fd);
    shutdown(fd, SHUT_RDWR);
    PERROR("after shutdown\n");
    close(fd);
    PERROR("after close\n");
    #endif
    *network_config.network_flag = 0;
    network_config.init_cmd_list();  // 初始化命令结构体
    request_cmd.init_data_table(&terminal_dev_register->data_table);
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

/**
 * 监听请求时间
 */
void * monitor_request(void* para)
{
    struct s_terminal_dev_register * terminal_dev_register = (struct s_terminal_dev_register *)para;

    int res = 0;
    unsigned char mode = 0;
    
    fd_set fdset;
    struct timeval tv;
    memset(&tv, 0, sizeof(struct timeval));

    struct sockaddr_in client;
    socklen_t len = sizeof(client);

    int fd = 0;
    int fd_tmp = 0;
    int fd_way1 = *network_config.server_base_fd; // 监听的通道一，此通道为网络通道
    int fd_way2 = 0; // 监听的通道二

    #if BOARDTYPE == 5350
    fd_way2 = *network_config.serial_pad_fd;
    #elif BOARDTYPE == 9344
    fd_way2 = network_config.usb_pad_fd;
    #endif

    /*************************************************************************************/
    /************************   以上全局变量的定义 以下为主要功能实现   ******************/
    /*************************************************************************************/

    while (1)
    {
        // 加入集合
        FD_ZERO(&fdset);
        FD_SET(fd_way1, &fdset);
        FD_SET(fd_way2, &fdset);

        // 超时2s
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        switch(select(common_tools.get_large_number(fd_way1, fd_way2) + 1, &fdset, NULL, NULL, &tv))
        {
            case -1:
            case 0:
            {
                PRINT("waiting monitor request...\n");
                continue;
            }
            default:
            {
                if (FD_ISSET(fd_way1, &fdset) > 0) // 网络通道
                {
                    fd_tmp = fd_way1;
                    mode = 1;
                    PRINT("network communication! fd_tmp = %d fd_way1 = %d\n", fd_tmp, fd_way1);
                }
                if (FD_ISSET(fd_way2, &fdset) > 0) //
                {
                    fd_tmp = fd_way2;
                    mode = 0;
                    PRINT("network communication! fd_tmp = %d fd_way2 = %d\n", fd_tmp, fd_way2);
                }
                if ((fd_tmp == fd_way1) || (fd_tmp == fd_way2))
                {
                    #if BOARDTYPE == 5350
                    if (fd_way1 == fd_tmp) // 网络通道
                    #elif BOARDTYPE == 9344
                    if (1)
                    #endif
                    {
                        if ((fd = accept(fd_tmp, (struct sockaddr*)&client, &len)) < 0)
                    	{
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "accept failed!", fd);
                            break;
                    	}
                    }
                    else if (fd_way2 == fd_tmp)
                    {
                        fd = fd_tmp;
                    }
                    
                    PRINT("fd_tmp = %d, fd = %d\n", fd_tmp, fd_tmp);
                    terminal_dev_register->fd = fd;
                    terminal_dev_register->mode = mode;
                    if ((res = request_cmd.request_cmd_analyse(terminal_dev_register)) < 0)
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "request_cmd_analyse failed!", res);
                        break;
                    }
                }
            }
        }

        mode = 0;
        fd = 0;
        fd_tmp = 0;
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
    signal(SIGSTOP, signal_handle);


    strcpy(common_tools.argv0, argv[0]);

    int res = 0;

    pthread_t pthread_monitor_request_id, pthread_manage_id;
    
	   request_cmd.init();
	   network_config.init_cmd_list();

	   if ((res = common_tools.get_config()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_config failed!", res);
        return res;
    }
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "The configuration file is read successfully!", 0);

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

    struct s_terminal_dev_register terminal_dev_register;
    memset(&terminal_dev_register, 0, sizeof(struct s_terminal_dev_register));

    if ((res = request_cmd.init_data_table(&terminal_dev_register.data_table)) < 0)
    {
         OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "init_data_table failed!", res);
    }

    #if CTSI_SECURITY_SCHEME == 2
     // 用户登录：当base上电时，检测是否终端初始化成功，当成功时，主动向平台请求设备令牌，保存在本地
    char device_token[128] = {0};
    int i = 0;

    // 查询状态
    if (terminal_dev_register.data_table.register_state == 0)
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
    #endif

    pthread_mutex_init(&network_config.recv_mutex, NULL);
    pthread_mutex_init(&terminal_dev_register.mutex, NULL);
    #if BOARDTYPE == 5350
    pthread_mutex_init(&nvram_interface.mutex, NULL);
    #endif
    pthread_cond_init(&terminal_dev_register.cond, NULL);

    // 初始化互斥锁、创建线程
    pthread_mutex_init(&config_mutex, NULL);

    pthread_create(&pthread_monitor_request_id, NULL, (void*)monitor_request, (void *)&terminal_dev_register);
    PRINT("pthread_create (monitor_request) success!\n");
   
    pthread_create(&pthread_manage_id, NULL, (void*)pthread_manage, (void *)&terminal_dev_register);
    PRINT("pthread_create (pthread_manage) success!\n");
    
    pthread_join(pthread_monitor_request_id, NULL);
    pthread_join(pthread_manage_id, NULL);
    
    pthread_mutex_destroy(&config_mutex);
    pthread_mutex_destroy(&terminal_dev_register.mutex);
    
    #if BOARDTYPE == 5350
    pthread_mutex_destroy(&nvram_interface.mutex);
    #endif
    pthread_cond_destroy(&terminal_dev_register.cond);
    return 0;
}


