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
        //request_cmd.cancel_mac_and_ip_bind();

        // 3.
        system("cfg -x");
        sleep(2);
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
        case SIGHUP:
        {
            PRINT("SIGHUP sig no:%d; sig info:挂起\n", sig);
            break;
        }
        case SIGINT:
        {
            PRINT("SIGINT sig no:%d; sig info:中断(ctrl + c)\n", sig);
            break;
        }
        case SIGQUIT:
        {
            PRINT("SIGQUIT sig no:%d; sig info:退出\n", sig);
            break;
        }
        case SIGABRT:
        {
            PRINT("SIGABRT sig no:%d; sig info:中止\n", sig);
            break;
        }
        case SIGBUS:
        {
            PRINT("SIGBUS sig no:%d; sig info:总线错误\n", sig);
            break;
        }
        case SIGPIPE:
        {
            PRINT("SIGPIPE sig no:%d; sig info:管道破裂-写一个没有读端口的管道\n", sig);
            break;
        }
        case SIGALRM:
        {
            PRINT("SIGALRM sig no:%d; sig info:闹钟 当某进程希望在某时间后接收信号时发此信号\n", sig);
            break;
        }
        case SIGTERM:
        {
            PRINT("SIGTERM sig no:%d; sig info:软件终止\n", sig);
            break;
        }
        #if 0
        case SIGCHLD:
        {
            pid_t pid;
            while ((pid = waitpid(-1, &res, WNOHANG)) > 0)
			{
				PRINT("pid = %d, state = %d\n", pid, res);
			}
            PRINT("SIGCHLD sig no:%d; sig info:子进程结束信号\n", sig);
            break;
        }
        #endif // BOARDTYPE == 9344
        case SIGCLD:
        {
            pid_t pid;
            while ((pid = waitpid(-1, &res, WNOHANG)) > 0)
			{
				PRINT("pid = %d, state = %d\n", pid, res);
			}
            PRINT("SIGCLD sig no:%d; sig info:子进程结束信号\n", sig);
            break;
        }
        case SIGSTOP:
        {
            PRINT("SIGSTOP sig no:%d; sig info:停止进程的执行\n", sig);
            break;
        }
        case SIGPROF:
        {
            PRINT("SIGPROF sig no:%d; sig info:定时器到\n", sig);
            break;
        }
        case SIGVTALRM:
        {
            PRINT("SIGVTALRM sig no:%d; sig info:实际时间报警时钟信号\n", sig);
            break;
        }
        case SIGPWR:
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
 * 监听请求时间
 */
int monitor_request(struct s_terminal_dev_register * terminal_dev_register)
{
    int res = 0;
    unsigned char communication_mode = 0;
    
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
        if (network_config.pthread_recv_flag == 1) // 说明正在网络设置和初始化设置
        {
            PRINT("before pthread_mutex_lock\n");
            pthread_mutex_lock(&network_config.recv_mutex);
            pthread_mutex_unlock(&network_config.recv_mutex);
            PRINT("before pthread_mutex_unlock\n");
        }
        
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
                    communication_mode = 1;
                    PRINT("network communication! fd_tmp = %d fd_way1 = %d\n", fd_tmp, fd_way1);
                }
                if (FD_ISSET(fd_way2, &fdset) > 0) //
                {
                    #if USB_INTERFACE == 1 // 
                    if (1)
                    #elif USB_INTERFACE == 2 // 从usb节点读取
                    if (terminal_dev_register->config_now_flag == 0) // 说明此时不在设置
                    #endif // USB_INTERFACE == 2
                    {
                        fd_tmp = fd_way2;
                        communication_mode = 0;
                        PRINT("network communication! fd_tmp = %d fd_way2 = %d\n", fd_tmp, fd_way2);
                    }
                    else
                    {
                        PRINT("terminal_dev_register->config_now_flag = %d\n", terminal_dev_register->config_now_flag);
                        continue;
                    }
                }
                if ((fd_tmp == fd_way1) || (fd_tmp == fd_way2))
                {
                    #if BOARDTYPE == 5350
                    if (fd_way1 == fd_tmp) // 网络通道
                    #elif BOARDTYPE == 9344
                    
                    #if USB_INTERFACE == 1
                    if (1)
                    #elif USB_INTERFACE == 2
                    if (fd_way1 == fd_tmp)
                    #endif // USB_INTERFACE == 2
                    
                    #endif // BOARDTYPE == 9344
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
                    
                    PRINT("fd_tmp = %d, fd = %d\n", fd_tmp, fd);
                    terminal_dev_register->fd = fd;
                    terminal_dev_register->communication_mode = communication_mode;
                    if ((res = request_cmd.request_cmd_analyse(terminal_dev_register)) < 0)
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "request_cmd_analyse failed!", res);
                        break;
                    }
                }
            }
        }

        communication_mode = 0;
        fd = 0;
        fd_tmp = 0;
    }
    return 0;
}

int main(int argc, char ** argv)
{
    // 信号捕获和处理
    signal(SIGHUP, signal_handle);
    signal(SIGINT, signal_handle);
    signal(SIGQUIT, signal_handle);

    signal(SIGABRT, signal_handle);
    signal(SIGBUS, signal_handle);

    signal(SIGPIPE, signal_handle);
    signal(SIGALRM, signal_handle);
    signal(SIGTERM, signal_handle);
    
    signal(SIGCLD, signal_handle);
    signal(SIGCHLD, signal_handle);
    signal(SIGSTOP, signal_handle);
    
    signal(SIGPROF, signal_handle);
    signal(SIGVTALRM, signal_handle);
    signal(SIGPWR, signal_handle);

    strcpy(common_tools.argv0, argv[0]);

    int res = 0;
    
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
    char device_token[TOKENLEN] = {0};
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
    
    #if BOARDTYPE == 5350
    pthread_mutex_init(&nvram_interface.mutex, NULL);
    #endif
    
    if ((res = monitor_request(&terminal_dev_register)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "monitor_request failed", res);
        return res;
    }
    
    #if BOARDTYPE == 5350
    pthread_mutex_destroy(&nvram_interface.mutex);
    #endif
    pthread_mutex_destroy(&network_config.recv_mutex);
    return 0;
}


