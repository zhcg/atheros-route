#include "cacm.h"

pthread_mutex_t send_mutex;
unsigned char register_flag = 0;
int server_fd = 0;

/*****************************************************************************************
* 线程一：完成状态的上报，即把心跳包发送到平台服务器
* 线程二：完成状态的上报，即把base的状态信息包发送到平台服务器
* 线程三：完成状态的上报，即把电话线状态包等异常异常信息发送到平台服务器
* 线程四：事件处理线程，监控事件
******************************************************************************************/
/**
 * 事件处理线程
 */
static void *pthread_event_manage(void *para)
{
    eXosip_event_t *event = NULL;
    while (((struct s_cacm *)para)->pthread_event_exit_flag == 0)
    {
        event = eXosip_event_wait(1, 0);
        
        eXosip_lock();
        // 刷新注册和注册期满前延迟
        eXosip_automatic_action(); 
        eXosip_unlock();
        
        if (event != NULL)
        {
            exevents.cacm_parse_event((struct s_cacm *)para, event);
        }
    }
	return NULL;
}

/**
 * 心跳包管理
 */
static void * heartbeat_msg_manage(void* para)
{
    int res = 0;
    int heartbeat_cycle = ((struct s_cacm *)para)->heartbeat_cycle;
    PRINT("heartbeat_cycle = %d\n", heartbeat_cycle);
    
	struct timeval tv;
	memset(&tv, 0, sizeof(struct timeval));
	
	char heartbeat_buf[256] = {0};
	char send_buf[4096] = {0};
	
	while (1)
	{
	    // 心跳
	    gettimeofday(&tv, NULL);
	    
		sprintf(heartbeat_buf,"{deviceType:1,deviceId:%s,time:%d000}", ((struct s_cacm *)para)->base_sn, tv.tv_sec);			
		
		snprintf(send_buf, sizeof(send_buf),
			"v=0\r\n"
			"o=%s\r\n"
			"s=conversation\r\n"
			"i=3\r\n"
			"c=%s\r\n",((struct s_cacm *)para)->base_sn, heartbeat_buf);
			
		pthread_mutex_lock(&send_mutex);
		// 发送
		if ((res = cacm_tools.send_message((struct s_cacm *)para, send_buf)) < 0)
    	{
    	    PERROR("send_message failed!\n");
    	}
    	else
    	{
    	    PRINT("send heartbeat msg successful!\n");    
    	}
    	pthread_mutex_unlock(&send_mutex);
    	
    	memset(heartbeat_buf, 0, sizeof(heartbeat_buf));
		memset(&tv, 0, sizeof(struct timeval));
		memset(send_buf, 0, sizeof(send_buf));
		
		sleep(heartbeat_cycle);
	}
	return (void *)res;
}

/**
 * 监控信息包 管理
 */
static void * base_state_msg_manage(void* para)
{
    int res = 0;
    unsigned int business_cycle = ((struct s_cacm *)para)->business_cycle;
    PRINT("business_cycle = %d\n", business_cycle);
	
	char base_state_buf[4096] = {0};
	char send_buf[4096] = {0};
	
	while (1)
	{
	    // base状态监控信息
		if ((res = cacm_tools.get_base_state((struct s_cacm *)para, base_state_buf, sizeof(base_state_buf))) < 0)
		{
		    PERROR("get_base_state failed!\n");
		    continue;
		}
		snprintf(send_buf, sizeof(send_buf),
		    "v=0\r\n"
		    "o=%s\r\n"
		    "s=conversation\r\n"
		    "i=2\r\n"
		    "c=%s\r\n", ((struct s_cacm *)para)->base_sn, base_state_buf);
		
		pthread_mutex_lock(&send_mutex);
		// 发送
		if ((res = cacm_tools.send_message((struct s_cacm *)para, send_buf)) < 0)
    	{
    	    PERROR("send_message failed!\n");
    	}
    	else
    	{
    	    PRINT("send base_state msg successful!\n");    
    	}
    	pthread_mutex_unlock(&send_mutex);
    	
    	memset(base_state_buf, 0, sizeof(base_state_buf));
		memset(send_buf, 0, sizeof(send_buf));
		
		sleep(business_cycle);
	}
	return (void *)res;
}

/**
 * 初始化线程的运行环境
 */
static int init_thread_run_env()
{
    int res = 0;
    char * socket_name = "/var/linphone/log/.linphone_phone_state";
    
    // 创建本地socket，接收电话线状态的上报
    if ((server_fd = internetwork_communication.make_local_socket_server_link(socket_name)) < 0)
    {
        PERROR("make_local_socket_server_link failed!\n");
        return server_fd;
    }
    
    return 0;
}
/**
 * 紧急事件上报 电话线插拔等（实时）
 */
static void * real_time_event(void* para)
{
    int res = 0;
    char buf[256] = {0};
    char send_buf[1024] = {0};
    
    struct timeval timeout;
    memset(&timeout, 0, sizeof(struct timeval));
    
    #if SPI_RECV_PHONE_STATE
    
    #else
    // 初始化运行环境
    if ((res = init_thread_run_env()) < 0)
    {
        PERROR("init_thread_run_env failed!\n");
        return (void *)res;
    }
    #endif
    
    while (1)
    {
        if ((res = cacm_tools.phone_state_monitor((struct s_cacm *)para, buf, sizeof(buf), &timeout)) < 0)
        {
            PERROR("phone_state_monitor failed!\n");
            continue;
        }
        
        snprintf(send_buf, sizeof(send_buf),	
			"v=0\r\n"
			"o=%s\r\n"
			"s=conversation\r\n"
		    "i=1\r\n"
			"c=%s\r\n", ((struct s_cacm *)para)->base_sn, buf);
			
		pthread_mutex_lock(&send_mutex);
		if ((res = cacm_tools.send_message((struct s_cacm *)para, send_buf)) < 0)
		{
		    PERROR("send_message failed!\n");
		}
		else
		{
		    PRINT("send real_time_event msg successful!\n");
		}
		pthread_mutex_unlock(&send_mutex);
		
		memset(send_buf, 0, sizeof(send_buf));
		memset(buf, 0, sizeof(buf));
		
		usleep(100000);
    }
    return 0;
}

/**
 * 启动三个线程
 */
static int start_threads(struct s_cacm *cacm)
{
    int res = 0;
	
	char columns_name[2][30] = {"heart_beat_cycle", "business_cycle"};
	char columns_value[2][100] = {0};
#if BOARDTYPE == 5350
	if ((res = nvram_interface.select(RT5350_FREE_SPACE, 2, columns_name, columns_value)) < 0)
	{
	    PERROR("nvram_select failed!\n");
	    return res;
	}
#elif BOARDTYPE == 9344
	if ((res = database_management.select(2, columns_name, columns_value)) < 0)
	{
	    PERROR("sqlite_select failed!\n");
	    return res;
	}
#endif
    if ((strlen(columns_value[0]) == 0) || (memcmp(columns_value[0], "\"\"", 2) == 0) ||
        (strlen(columns_value[1]) == 0) || (memcmp(columns_value[1], "\"\"", 2) == 0))
    {
        PERROR("no heart_beat_cycle or no business_cycle in flash!\n");
	    return res;
    }
    
    PRINT("heart_beat_cycle = %s\n", columns_value[0]);
    PRINT("business_cycle = %s\n", columns_value[1]);
    
	cacm->heartbeat_cycle = atoi(columns_value[0]);
	cacm->business_cycle = atoi(columns_value[1]);
	
    pthread_mutex_init(&send_mutex, NULL);
    
	// 心跳包发送线程
	pthread_create(&cacm->pthread_heartbeat_id, NULL,&heartbeat_msg_manage, (void *)cacm);
	PRINT("pthread_create success!(heartbeat_msg_manage)\n");
	
	// 监控信息包 发送线程
	pthread_create(&cacm->pthread_base_state_id, NULL,&base_state_msg_manage, (void *)cacm);
	PRINT("pthread_create success!(base_state_msg_manage)\n");
	
	// 实时事件发送线程 电话线插拔等
	pthread_create(&cacm->pthread_real_time_event_id, NULL, &real_time_event, (void *)cacm);
	PRINT("pthread_create success!(real_time_event)\n");
	
	pthread_join(cacm->pthread_heartbeat_id, NULL);
	PRINT("pthread_join heartbeat_msg_manage exit!\n");
	pthread_join(cacm->pthread_base_state_id, NULL);
	PRINT("pthread_join base_state_msg_manage exit!\n");
	pthread_join(cacm->pthread_real_time_event_id, NULL);
	PRINT("pthread_join real_time_event  exit!\n");
	
	pthread_mutex_destroy(&send_mutex);
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
 * 注册到服务器
 */
static int register_to_sip_server(struct s_cacm *cacm)
{
    int res = 0;
    int i = 0;
    eXosip_event_t *event = NULL;
	
	memset(cacm->hint, 0, sizeof(cacm->hint));
    snprintf(cacm->hint, sizeof(cacm->hint), "(%s) Registered to the server (%s)!", cacm->base_sn, cacm->proxy_ip);
    
	if ((res = cacm_tools.init_register(cacm)) < 0)
	{
	    PERROR("init_register failed!\n");
	    return res;
	}
	PRINT("after init_register!\n");
	return 0;
}

/**
 * 初始化cacm
 */
static int cacm_init(struct s_cacm *cacm)
{
	int res = 0;
	
	char columns_name[5][30] = {"base_user_name", "base_password", "sip_ip", "sip_port", "base_sn"};
	char columns_value[5][100] = {0};
	
	if (eXosip_init() < 0)
	{
	    PERROR("eXosip_init failed!\n");
	    return EXOSIP_INIT_ERR;
	}
	PRINT("after eXosip_init!\n");
	if (eXosip_listen_addr(IPPROTO_UDP, NULL, CACM_PORT, AF_INET, 0) < 0)
	{
	    eXosip_quit();
	    PERROR("eXosip_listen_addr failed!\n");
	    return EXOSIP_LISTEN_ERR;
	}
	PRINT("after eXosip_listen_addr!\n");


#if BOARDTYPE == 5350
	if ((res = nvram_interface.select(RT5350_FREE_SPACE, 5, columns_name, columns_value)) < 0)
	{
	    eXosip_quit();
	    PERROR("nvram_select failed!\n");
	    return res;
	}
#elif BOARDTYPE == 9344

	if ((res = database_management.select(5, columns_name, columns_value)) < 0)
	{
	    eXosip_quit();
	    PERROR("sqlite_select failed!\n");
	    return res;
	}
#endif 

	if ((strlen(columns_value[0]) == 0) || (memcmp(columns_value[0], "\"\"", 2) == 0) || 
        (strlen(columns_value[1]) == 0) || (memcmp(columns_value[1], "\"\"", 2) == 0) ||
        (strlen(columns_value[2]) == 0) || (memcmp(columns_value[2], "\"\"", 2) == 0) ||
        (strlen(columns_value[3]) == 0) || (memcmp(columns_value[3], "\"\"", 2) == 0) ||
        (strlen(columns_value[4]) == 0) || (memcmp(columns_value[4], "\"\"", 2) == 0))
    {
        PERROR("no base_user_name or no base_password no sip_ip no sip_port in flash!\n");
	    return res;
    }
    
    memcpy(cacm->user_name, columns_value[0], strlen(columns_value[0])); // 用户名
	memcpy(cacm->password, columns_value[1], strlen(columns_value[1]));  // 密码 
	memcpy(cacm->proxy_ip, columns_value[2], strlen(columns_value[2]));  // 代理服务器IP
	cacm->proxy_port = atoi(columns_value[3]);                           // 代理服务器端口
	cacm->local_port = CACM_PORT;                                        // 本地端口   
	memcpy(cacm->base_sn, columns_value[4], strlen(columns_value[4]));   // 设备序列号
	
	eXosip_guess_localip(AF_INET, cacm->localip, 64);                    // 本地IP
	
	#if CACM_STOP == 0
	cacm->expires = 120;
	#else 
    cacm->expires = 3600; // 一小时
	#endif
	
	sprintf(cacm->from, "sip:%s@%s:%d", cacm->user_name, cacm->proxy_ip, cacm->proxy_port);
	sprintf(cacm->register_uri, "sip:%s:%d", cacm->proxy_ip, cacm->proxy_port); // 代理服务器地址
	sprintf(cacm->contact, "sip:%s@%s:%d", cacm->user_name, cacm->localip, cacm->local_port);
	
	
	PRINT("base_user_name = %s, base_password = %s\n", cacm->user_name, cacm->password);
	PRINT("sip_ip = %s, sip_port = %d\n", cacm->proxy_ip, cacm->proxy_port);
	PRINT("cacm->localip = %s, local_port = %d\n", cacm->localip, cacm->local_port);
	PRINT("base_sn = %s\n", cacm->base_sn);
	
	PRINT("cacm->from = %s\n", cacm->from);
	PRINT("cacm->register_uri = %s\n", cacm->register_uri);
	PRINT("cacm->contact = %s\n", cacm->contact);
	
	return 0;
}

/**
 * 释放cacm
 */
int cacm_exit()
{
    eXosip_quit();
    return 0;
}

int main (int argc, char ** argv)
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
    
    signal(SIGPROF, signal_handle);
    signal(SIGVTALRM, signal_handle);
    signal(SIGCLD, signal_handle);
    signal(SIGPWR, signal_handle);
    signal(SIGCHLD, signal_handle);
    
    strcpy(common_tools.argv0, argv[0]);
    
    int res = 0;
    int i = 0;
    struct s_cacm cacm;
    memset((void *)&cacm, 0, sizeof(struct s_cacm));
    
    // 清空位置令牌
    {
        char column_name[1][30] = {"position_token"};
	    char column_value[1][100] = {""};
	    unsigned short len = strlen("");
#if BOARDTYPE == 5350
        if ((res = nvram_interface.update(RT5350_FREE_SPACE, 1, column_name, column_value, &len)) < 0)
    	{
    	    PERROR("nvram_update failed!\n");
    	    return res;
    	}
#elif BOARDTYPE == 9344
        if ((res = database_management.update(1, column_name, column_value, &len)) < 0)
    	{
    	    PERROR("sqlite_update failed!\n");
    	    return res;
    	}
#endif
    }
    
    // 初始化
	if ((res = cacm_init(&cacm)) < 0) 
	{
	    PERROR("cacm_init failed!\n");
    	    return res;
	}
	PRINT("after cacm_init!\n");
	
	// 事件处理线程
	pthread_create(&cacm.pthread_event_manage_id, NULL, &pthread_event_manage, (void *)&cacm);
	PRINT("pthread_create success!(pthread_event_manage) pthread_event_manage_id = %d\n", cacm.pthread_event_manage_id);
	
	//sprintf(buf, "register sip:%s@%s:%s sip:%s %s", columns_value[0], columns_value[2], columns_value[3], columns_value[2], columns_value[1]);
	// 注册到SIP服务器
	for (i = 0; i < 10; i++)
	{
    	if ((res = register_to_sip_server(&cacm)) < 0)
    	{
    	    PRINT("register_to_sip_server failed!\n");
    	    continue;
        }
        sleep(3);
        
        if (cacm.exit_flag == 1)
        {
            break;
        }
        if (cacm.register_flag == 1)
        {
            PRINT("register_to_sip_server ok!\n");
            break;
        }
    }
    
    if (i == 10)
    {
        cacm_exit();
        PRINT("register_to_sip_server failed!\n");
        return res;
    }
    
    // 启动3个线程
    if (cacm.exit_flag == 0)
    {
        if ((res = start_threads(&cacm)) < 0)
        {
            PRINT("start_threads failed!\n");
        }
        
        PRINT("after start_threads!\n");
    }
    
    #if CACM_STOP
    // 注销
    if (cacm.register_flag == 1)
    {
        if ((res = cacm_tools.init_logout(&cacm)) < 0)
    	{
    	    PERROR("init_logout failed!\n");
        }
        PRINT("after init_logout!\n");
        sleep(2);
    }
	
    cacm.pthread_event_exit_flag = 1;
    PRINT("cacm.pthread_event_exit_flag = %d\n", cacm.pthread_event_exit_flag);
    #endif
    pthread_join(cacm.pthread_event_manage_id, NULL);
    PRINT("pthread_join pthread_event_manage exit!\n");
	
    cacm_exit();
    return res;
}

