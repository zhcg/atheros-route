#include "common_tools.h"
#include "database_management.h"
#include "communication_network.h"

#define DELAYS 5

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
				PRINT("pid = %d, res = %d\n", pid, res);
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
				PRINT("pid = %d, res = %d\n", pid, res);
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
 * 监听应用
 */
static int monitor_app()
{
    struct timeval tv;
    memset(&tv, 0, sizeof(struct timeval));
    int res = 0;
    unsigned char register_flag = 0;
    int client_fd = 0;
    char *spi_cmd = "ps | grep spi_rt_main | sed '/grep/'d | sed '/\\[spi_rt_main\\]/'d | sed '/sed/'d";
    char *cacm_cmd = "ps | grep cacm | sed '/grep/'d | sed '/\\[cacm\\]/'d | sed '/sed/'d";
    char *terminal_init_cmd = "ps | grep terminal_dev_register | sed '/grep/'d | sed '/\\[terminal_dev_register\\]/'d | sed '/sed/'d";
    char *swipe_card_cmd = "ps | grep swipe_card | sed '/grep/'d";
    
    char buf[256] = {0};
    unsigned char register_state = 5;
    
    char columns_name[1][30] = {0};
	char columns_value[1][100] = {0};
	
	unsigned char send_buf[40] = {0};
	unsigned char recv_buf[6] = {0};
	char pad_sn[SN_LEN + 1] = {0};
	unsigned int business_cycle = 0;
	int count = 0; // 记录延时的次数 count * DELAYS >= business_cycle
	
	strcpy(columns_name[0], "register_state");
    
	if ((res = database_management.select(1, columns_name, columns_value)) < 0)
	{
	    PERROR("sqlite_select failed!\n");
	    return res;
	}
    
    register_state = atoi(columns_value[0]);
    PRINT("register_state = %d, columns_value[0] = %s\n", register_state, columns_value[0]);
    // 初始化成功
	if (register_state ==  0)
	{
	    PRINT("register_state key is zero!(terminal initialized successful!)\n");
	    
	    memset(columns_name[0], 0, sizeof(columns_name[0]));
	    memset(columns_value[0], 0, sizeof(columns_value[0]));
	    
	    strcpy(columns_name[0], "business_cycle");
	    
    	if ((res = database_management.select(1, columns_name, columns_value)) < 0)
    	{
    	    PERROR("sqlite_select failed!\n");
    	    return res;
    	}
        register_flag = 1;
        business_cycle = atoi(columns_value[0]);
        PRINT("business_cycle = %s\n", columns_value[0]);
	}
    
    // 需要查看register_state和交易周期
    char * const cacm_app_argv[] = {"cacm", NULL};
    char * const terminal_init_app_argv[] = {"terminal_dev_register", NULL};
    
    while (1)
    {
        // 终端初始化
        if (common_tools.get_cmd_out(terminal_init_cmd, buf, sizeof(buf)) < 0)
        {
            PERROR("get_cmd_out failed!\n");
            continue;
        }
        else
        {
            if (strlen(buf) == 0)
            {
                PRINT("terminal_init stop!\n");
                if ((res = common_tools.start_up_application("/bin/terminal_dev_register", terminal_init_app_argv, 0)) < 0)
                {
                    PERROR("start_up_application failed!\n");
                    continue;
                }
                PRINT("terminal_init restart!\n");
            }
            //PRINT("terminal_init is running...\n");
        }
        //cacm
        if (register_state == 0)
        {
            // CACM
            if (common_tools.get_cmd_out(cacm_cmd, buf, sizeof(buf)) < 0)
            {
                PERROR("get_cmd_out failed!\n");
                continue;
            }
            else
            {
                if (strlen(buf) == 0)
                {
                    PRINT("cacm stop!\n");
                    if ((res = common_tools.start_up_application("/bin/cacm", cacm_app_argv, 0)) < 0)
                    {
                        PERROR("start_up_application failed!\n");
                    }
                    else 
                    {
                        PRINT("cacm restart!\n");
                    }
                }
            }
        }
        
        // base状态
        if ((register_flag == 1) && ((count == 0) || (++count * DELAYS >= business_cycle)))
        {
            count = 1; // 提前 DELAYS 生成
            // 生成base状态文件
            if (system("top -b -n 1 > /var/cacm/log/.base_state") < 0)
            {
                PERROR("system failed!\n");
                continue;
            }
            sleep(business_cycle);
        }
        else
        {
            sleep(DELAYS);
        }
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
    if ((res = common_tools.get_config()) < 0)
	{
		PERROR("get_config failed!\n");
		return res;
	}
    // STUB启动提示
    #if PRINT_DEBUG
    PRINT("[monitor_application (%s %s)]\n", __DATE__, __TIME__);
    PRINT("[monitor_application is running...]\n", __DATE__, __TIME__);
    #else
    printf("[%s]%s["__FILE__"][%s][%05d] [monitor_application (%s %s)]\n", common_tools.argv0, common_tools.get_datetime_buf(), __FUNCTION__, __LINE__, __DATE__, __TIME__);
    printf("[%s]%s["__FILE__"][%s][%05d] [monitor_application is running...]\n", common_tools.argv0, common_tools.get_datetime_buf(), __FUNCTION__, __LINE__);
    #endif
    
    monitor_app();
    return 0;
}
