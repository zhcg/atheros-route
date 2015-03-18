/*************************************************************************
    > File Name: terminal_dev_register.c
    > Author: yangjilong
    > mail1: yangjilong65@163.com
    > mail2: yang.jilong@handaer.com
    > Created Time: 2013年11月08日 星期五 09时04分26秒
**************************************************************************/
#include "network_config.h"
#include "request_cmd.h"
#include "terminal_register.h"

#include "database_management.h"
#include "communication_network.h"
#include "terminal_authentication.h"

/**
 * 命令行分析
 */
static int analyse_command_line(int argc, char ** argv)
{
    int i = 0;
    int res = 0;
    
    if (memcmp(argv[1], "-v", strlen("-v")) == 0)
    {
        PRINT("author %s\n", TERMINAL_AUTHOR);
        PRINT("version %s\n", TERMINAL_VERSION);
        PRINT("%s %s\n", __DATE__, __TIME__);
        PRINT("%s\n", TERMINAL_REMARKS);
    }
    else if (memcmp(argv[1], "-c", strlen("-c")) == 0) // 情况数据
    {
        if ((res = database_management.clear()) < 0)
        {
            PRINT("sqlite3_clear_table failed!\n");
            return res;
        }
        PRINT("data clear success!\n");
    }
    else if (memcmp(argv[1], "-d", strlen("-d")) == 0) // 恢复出厂设置
    {
        // 1.情况数据库
        if ((res = database_management.clear()) < 0)
        {
            PRINT("sqlite3_clear_table failed!\n");
            return res;
        }
        
        // 2.删除隐藏SSID
		system("cfg -b 5"); // default config
        system("reboot");
    }
    else if (memcmp(argv[1], "-h", strlen("-h")) == 0)
    {
        PRINT("terminal_init %s (%s %s)\n", TERMINAL_VERSION, __DATE__, __TIME__);
        PRINT("usage1: terminal_dev_register [option]\n");
        PRINT("options:\n");
        PRINT("\t-c: 清空数据表\n");
        PRINT("\t-d: 恢复出厂设置\n");
        PRINT("\t-h: 帮助\n");
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

    fd_way2 = network_config.usb_pad_fd;
    terminal_dev_register->non_network_fd = fd_way2;
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
                    // if (terminal_dev_register->config_now_flag == 0) // 说明此时不在设置
                    if (network_config.pthread_recv_flag == 0) // 说明此时没有接收数据
                    #endif // USB_INTERFACE == 2
                    {
                        fd_tmp = fd_way2;
                        communication_mode = 0;
                        PRINT("network communication! fd_tmp = %d fd_way2 = %d\n", fd_tmp, fd_way2);
                    }
                    else
                    {
                        PRINT("network_config.pthread_recv_flag = %d\n", network_config.pthread_recv_flag);
                        continue;
                    }
                }
                if ((fd_tmp == fd_way1) || (fd_tmp == fd_way2))
                {
                    #if USB_INTERFACE == 1
                    if (1)
                    #elif USB_INTERFACE == 2
                    if (fd_way1 == fd_tmp)
                    #endif // USB_INTERFACE == 2
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
    //signal(SIGHUP, signal_handle);
    //signal(SIGINT, signal_handle);
    //signal(SIGQUIT, signal_handle);

    //signal(SIGABRT, signal_handle);
    //signal(SIGBUS, signal_handle);

    signal(SIGPIPE, signal_handle);
    //signal(SIGALRM, signal_handle);
    //signal(SIGTERM, signal_handle);
    
    //signal(SIGCLD, signal_handle);
    //signal(SIGCHLD, signal_handle);
    //signal(SIGSTOP, signal_handle);
    
    //signal(SIGPROF, signal_handle);
    //signal(SIGVTALRM, signal_handle);
    //signal(SIGPWR, signal_handle);

    strcpy(common_tools.argv0, argv[0]);

    int res = 0;
    
    request_cmd.init();
    
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
    
    pthread_mutex_init(&network_config.recv_mutex, NULL);
    
    if ((res = monitor_request(&terminal_dev_register)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "monitor_request failed", res);
        return res;
    }
    
    pthread_mutex_destroy(&network_config.recv_mutex);
    return 0;
}


