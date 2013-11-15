#include "common_tools.h"
#include "nvram_interface.h"
#include "internetwork_communication.h"

static struct timeval tv_start;
static struct timeval tv_end;

#if SSID3_MONITOR
static pthread_mutex_t ssid3_mutex;
static unsigned char ssid3_live_flag = 0;

#define LOCAL_IP "127.0.0.1"
#define LOCAL_PORT "7788"

#endif


#if SMART_RECOVERY

/**
 * 智能恢复第二分区
 */
static int recovery_freespace()
{
    int res = 0;
    int i = 0;
    char column_name[1][30] = {"recovery_freespace"};
    char column_value[1][100] = {"1"};
    unsigned short column_value_len[1] = {1};
    
    char columns_name[50][30] = 
    { 
        "STEP_LOG", "SERIAL_DATA_LOG", "OLD_ROUTE_CONFIG", "PPPOE_STATE_FILE", "SPI_PARA_FILE", "WAN_STATE_FILE", 
        "SERIAL_STC", "SERIAL_PAD", "SERIAL_STC_BAUD", "SERIAL_PAD_BAUD", 
        "CENTERPHONE", "CENTERIP", "CENTERPORT", "BASE_IP",
        "PAD_IP", "PAD_SERVER_PORT", "PAD_SERVER_PORT2", "PAD_CLIENT_PORT", 
        "TOTALTIMEOUT", "ONE_BYTE_TIMEOUT_SEC",
        "ONE_BYTE_TIMEOUT_USEC", "ONE_BYTE_DELAY_USEC", 
        "ROUTE_REBOOT_TIME_SEC", "REPEAT", 
        "TERMINAL_SERVER_IP", "TERMINAL_SERVER_PORT", "LOCAL_SERVER_PORT", "WAN_CHECK_NAME", 
        "default_ssid", "default_ssid_password", "register_state",
        "pad_sn", "pad_mac", "pad_ip",
        "base_sn", "base_mac", "base_ip",
        "ssid_user_name", "ssid_password",
         
        "device_token", "position_token", 
        "ssid3_state", 
        "pad_user_name", "pad_password", "base_user_name", "base_password", "sip_ip", "sip_port", "heart_beat_cycle", "business_cycle",
    };
    
    char columns_value[50][100] = {0};
    unsigned short columns_value_len[50] = {0};
    
    // 读取备份分区的数据
    if ((res = nvram_interface.select(RT5350_BACKUP_SPACE, sizeof(columns_value) / sizeof(columns_value[0]), columns_name, columns_value)) < 0)
    {
        PERROR("nvram_insert failed!\n");
	    return res;
    }
    for (i = 0; i < sizeof(columns_value_len) / sizeof(short); i++)
    {
        columns_value_len[i] = strlen(columns_value[i]);
    }
    
    // 插入恢复标志
    if ((res = nvram_interface.insert(RT5350_BACKUP_SPACE, sizeof(column_value) / sizeof(column_value[0]), column_name, column_value, column_value_len)) < 0)
    {
        PERROR("nvram_insert failed!\n");
	    return res;
    }
    
    // 恢复第二分区
    if ((res = nvram_interface.insert(RT5350_FREE_SPACE, sizeof(columns_value) / sizeof(columns_value[0]), columns_name, columns_value, columns_value_len)) < 0)
    {
        PERROR("nvram_insert failed!\n");
	    return res;
    }
    
    column_value[0][0] = '0';
    // 插入恢复标志
    if ((res = nvram_interface.insert(RT5350_BACKUP_SPACE, sizeof(column_value) / sizeof(column_value[0]), column_name, column_value, column_value_len)) < 0)
    {
        PERROR("nvram_insert failed!\n");
	    return res;
    }
    system("reboot");
    return 0;    
}

/**
 * 智能恢复
 */
static int smart_recovery()
{
    int res = 0;
    int i = 0;
    int cmd_word = 0;
	unsigned char ssid3_state = 0;
	char cmd_set_ssid3[64] = {0};
	char cmd_set_ssid3_password[64] = {0};

    char cmd[23][100] = 
    {
        "nvram_set 2860 SSID1 ",
        "nvram_set 2860 WPAPSK1 ",
        
        "nvram_set 2860 SSID2 ",
        "nvram_set 2860 WPAPSK2 ",
        
        "nvram_set 2860 AuthMode \"WPAPSKWPA2PSK;",
        
        "nvram_set 2860 macCloneEnabled ",
        "nvram_set 2860 macCloneMac ",
        
        "nvram_set 2860 dhcpStatic1 \"",
        "nvram_set 2860 EncrypType \"AES;AES",
        "nvram_set 2860 HideSSID \"1;0",
        "nvram_set 2860 RekeyInterval \"3600;3600",
        "nvram_set 2860 BssidNum ",
        "nvram_set 2860 RADIUS_Key1 ralink",
        "nvram_set 2860 RekeyMethod TIME",
        "nvram_set 2860 WscConfigured 1",
        "nvram_set 2860 FixedTxMode HT",
        "nvram_set 2860 lan2_ipaddr \"\"",
        "nvram_set 2860 lan2_netmask \"\"",
        "nvram_set 2860 upnpEnabled 0",
        "nvram_set 2860 radvdEnabled 0",
        "nvram_set 2860 pppoeREnabled 0",
        "nvram_set 2860 dnsPEnabled 0",
        "nvram_set 2860 Lan2Enabled 0",
    };
    
    char cmd_static[6][100] = 
    {
        // STATIC
        "nvram_set 2860 wanConnectionMode STATIC",
        
        "nvram_set 2860 wan_ipaddr ",
        "nvram_set 2860 wan_netmask ",
        "nvram_set 2860 wan_gateway ",
        "nvram_set 2860 wan_primary_dns ",
        "nvram_set 2860 wan_secondary_dns ",
        
    };
    
    char cmd_dhcp[2][100] = 
    {
        // DHCP
        "nvram_set 2860 wanConnectionMode DHCP",
        
        "nvram_set 2860 wan_dhcp_hn ",
    };
    
    char cmd_pppoe[5][100] = 
    {   
        // PPPOE
        "nvram_set 2860 wanConnectionMode PPPOE",
        
        "nvram_set 2860 wan_pppoe_user ",
        "nvram_set 2860 wan_pppoe_pass ",
        
        "nvram_set 2860 wan_pppoe_opmode KeepAlive",
        "nvram_set 2860 wan_pppoe_optime 60",
    };
    
    
    char columns_name[20][30] = 
    {
        "cmd_word", 
        "wan_ipaddr", "wan_netmask", "wan_gateway", "wan_primary_dns", "wan_secondary_dns", 
        "wan_dhcp_hn", 
        "wan_pppoe_user", "wan_pppoe_pass", 
        "mac_clone_enabled", "mac_clone_mac", 
        "ssid2", "wpapsk2", "auth", 
        "ssid_user_name", "ssid_password", "pad_mac", "pad_ip", 
		"default_ssid", "default_ssid_password"
    };
    
	char columns_value[20][100] = {0};
	
	char column_name[2][30] = {"register_state", "ssid3_state"};
	char column_value[2][100] = {0};
	
    // 查询 register_state and ssid3_state
    if ((res = nvram_interface.select(RT5350_FREE_SPACE, 2, column_name, column_value)) < 0)
	{
	    PERROR("nvram_select failed!\n");
	    return res;
	}
	
	// 判断是否有此数据
	if ((strcmp("\"\"", column_value[0]) == 0) || (strlen(column_value[0]) == 0))
    {
        PERROR("The second partition data may be lost!\n");
        // 利用在第三分区的备份，恢复第二分区
        if ((res = recovery_freespace()) < 0)
        {
            PERROR("recovery_freespace failed!\n");
            return res;
        }
    }
                    
	ssid3_state = atoi(column_value[1]);

	// 初始化未成功
	if (column_value[0][0] !=  '0')
	{
	    PRINT("terminal is not init!\n");
	    return 0;
	}
	
	memset(column_name[0], 0, sizeof(column_name[0]));
	memset(column_value[0], 0, sizeof(column_value[0]));
	strcpy(column_name[0], "SSID1");
	
	// 查询 SSID1
    if ((res = nvram_interface.select(NVRAM_RT5350, 1, column_name, column_value)) < 0)
	{
	    PERROR("nvram_select failed!\n");
	    return res;
	}
	
	// SSID1 == Handaer_AP
	if (memcmp(column_value[0], "Handaer_AP", strlen("Handaer_AP")) != 0)
	{
	    PRINT("SSID1 is not \"Handaer_AP\"!\n");
	    return 0;
	}
	
	PERROR("The first partition data may be lost!\n");
	
	// 初始化显示成功，但是SSID1等于Handaer_AP时，启动恢复
    if ((res = nvram_interface.select(RT5350_BACKUP_SPACE, 20, columns_name, columns_value)) < 0)
	{
	    PERROR("nvram_select failed!\n");
	    return res;
	}
	
	cmd_word = atoi(columns_value[0]);
	
	strncat(cmd[0], columns_value[14], strlen(columns_value[14]));
	strncat(cmd[1], columns_value[15], strlen(columns_value[15]));
	
	if (ssid3_state == 0)
	{
		if ((strlen(columns_value[11]) == 0) || (memcmp(columns_value[11], "\"\"", 2) == 0))
		{
			strncat(cmd[2], columns_value[14] + 12, 12);
			strncat(cmd[3], "administrator", strlen("administrator"));
			strncat(cmd[4], "WPAPSKWPA2PSK", strlen("WPAPSKWPA2PSK"));
	        strcat(cmd[4], "\"");
		}
	    else 
		{
			strncat(cmd[2], columns_value[11], strlen(columns_value[11]));
			strncat(cmd[3], columns_value[12], strlen(columns_value[12]));
			strncat(cmd[4], columns_value[13], strlen(columns_value[13]));
			strcat(cmd[4], "\"");
		}	
    
		strncat(cmd[5], columns_value[9], strlen(columns_value[9]));
		strncat(cmd[6], columns_value[10], strlen(columns_value[10]));
		
		common_tools.mac_add_colon(columns_value[16]);
		strncat(cmd[7], columns_value[16], strlen(columns_value[16]));
		strcat(cmd[7], ";"); 
		strncat(cmd[7], columns_value[17], strlen(columns_value[17]));
		strcat(cmd[7], "\"");
		
		strcat(cmd[8], "\"");
		strcat(cmd[9], "\"");
		strcat(cmd[10], "\"");
		strcat(cmd[11], "2");

	}
	else if (ssid3_state == 1)
	{
		if ((strlen(columns_value[11]) == 0) || (memcmp(columns_value[11], "\"\"", 2) == 0))
		{
			strncat(cmd[2], columns_value[14] + 12, 12);
			strncat(cmd[3], "administrator", strlen("administrator"));
			strncat(cmd[4], "WPAPSKWPA2PSK;WPAPSKWPA2PSK", strlen("WPAPSKWPA2PSK;WPAPSKWPA2PSK"));
	        strcat(cmd[4], "\"");
		}
	    else 
		{
			strncat(cmd[2], columns_value[11], strlen(columns_value[11]));
			strncat(cmd[3], columns_value[12], strlen(columns_value[12]));
			strncat(cmd[4], columns_value[13], strlen(columns_value[13]));
			strcat(cmd[4], ";WPAPSKWPA2PSK\"");
		}	
    
		strncat(cmd[5], columns_value[9], strlen(columns_value[9]));
		strncat(cmd[6], columns_value[10], strlen(columns_value[10]));
    
        common_tools.mac_add_colon(columns_value[16]);
		strncat(cmd[7], columns_value[16], strlen(columns_value[16]));
		strcat(cmd[7], ";"); 
		strncat(cmd[7], columns_value[17], strlen(columns_value[17]));
		strcat(cmd[7], "\"");
		
		strcat(cmd[8], ";AES\"");
		strcat(cmd[9], ";0\"");
		strcat(cmd[10], ";3600\"");
		strcat(cmd[11], "3");

		sprintf(cmd_set_ssid3, "nvram_set 2860 SSID3 %s", columns_value[18]);
		sprintf(cmd_set_ssid3_password, "nvram_set 2860 WPAPSK3 %s", columns_value[19]);

		system(cmd_set_ssid3);
		system(cmd_set_ssid3_password);
	}
    
	switch (cmd_word)
    {
        case 0x01: // 动态IP
        case 0x08: // 动态IP
        {
            strncat(cmd_dhcp[1], columns_value[6], strlen(columns_value[6]));
            for (i = 0; i < sizeof(cmd_dhcp) / sizeof(cmd_dhcp[0]); i++)
            {
                system(cmd_dhcp[i]);
                PRINT("cmd_dhcp[%d] = %s\n", i, cmd_dhcp[i]);
            }
            break;
        }
        case 0x02: // 静态IP
        case 0x09: // 静态IP   
        {
            strncat(cmd_static[1], columns_value[1], strlen(columns_value[1]));
            strncat(cmd_static[2], columns_value[2], strlen(columns_value[2]));
            strncat(cmd_static[3], columns_value[3], strlen(columns_value[3]));
            strncat(cmd_static[4], columns_value[4], strlen(columns_value[4]));
            strncat(cmd_static[5], columns_value[5], strlen(columns_value[5]));
            
            for (i = 0; i < sizeof(cmd_static) / sizeof(cmd_static[0]); i++)
            {
                system(cmd_static[i]);
                PRINT("cmd_static[%d] = %s\n", i, cmd_static[i]);
            }
            break;
        }
        case 0x03: // PPPOE
        case 0x0A: // PPPOE      
        {
            strncat(cmd_pppoe[1], columns_value[7], strlen(columns_value[7]));
            strncat(cmd_pppoe[2], columns_value[8], strlen(columns_value[8]));
            for (i = 0; i < sizeof(cmd_pppoe) / sizeof(cmd_pppoe[0]); i++)
            {
                system(cmd_pppoe[i]);
                PRINT("cmd_pppoe[%d] = %s\n", i, cmd_pppoe[i]);
            }
            break;
        }
        default:
        {
            PERROR("option does not mismatching!\n");
            return MISMATCH_ERR;
        }
    }
    for (i = 0; i < sizeof(cmd) / sizeof(cmd[0]); i++)
    {
        system(cmd[i]);
        PRINT("cmd[%d] = %s\n", i, cmd[i]);
    }
    
    system("reboot");
    return 0;
}
#endif

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
#if SSID3_MONITOR
static void * monitor_app(void* para)
#else
static int monitor_app()
#endif
{
    struct timeval tv;
    memset(&tv, 0, sizeof(struct timeval));
    int res = 0;
    unsigned char register_flag = 0;
    int client_fd = 0;
    char *spi_cmd = "ps | grep spi_rt_main | sed '/grep/'d | sed '/\\[spi_rt_main\\]/'d | sed '/sed/'d";
    char *terminal_init_cmd = "ps | grep terminal_init | sed '/grep/'d | sed '/\\[terminal_init\\]/'d | sed '/sed/'d";
    char *swipe_card_cmd = "ps | grep swipe_card | sed '/grep/'d";
    char *terminal_init_para_check_cmd = "nvram_get freespace STEP_LOG";
    char *terminal_init_para_check_cmd2 = "nvram_get backupspace STEP_LOG";
    char buf[256] = {0};
    
    char columns_name[1][30] = {0};
	char columns_value[1][100] = {0};
	
	unsigned char send_buf[40] = {0};
	unsigned char recv_buf[6] = {0};
	char pad_sn[35] = {0};
	unsigned int business_cycle = 0;
	
	strcpy(columns_name[0], "register_state");
    
    #if BOARDTYPE == 5350
	// 查询终端初始化状态（register_state）
	if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
	{
	    PERROR("nvram_select failed!\n");
	    return res;
	}
	
	// 初始化成功
	if (columns_value[0][0] ==  '0')
	{
	    PRINT("register_state key is zero!(terminal initialized successful!)\n");
	    memset(columns_name[0], 0, sizeof(columns_name[0]));
	    memset(columns_value[0], 0, sizeof(columns_value[0]));
	    
	    strcpy(columns_name[0], "business_cycle");
	    if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
    	{
    	    PERROR("nvram_select failed!\n");
    	    return res;
    	}
    	
    	if ((strlen(columns_value[0]) == 0) || (memcmp(columns_value[0], "\"\"", 2) == 0))
        {
            PERROR("no business_cycle in flash!\n");
            register_flag = 0;
        }
        else 
        {
            register_flag = 1;
            business_cycle = atoi(columns_value[0]);
            PRINT("business_cycle = %s\n", columns_value[0]);
        }
	}
    #elif BOARDTYPE == 9344
    #endif
	char * const app_argv[] = {"terminal_init", "-i", NULL};
    while (1)
    {
        if (common_tools.get_cmd_out(terminal_init_para_check_cmd, buf, sizeof(buf)) < 0)
        {
            PERROR("get_cmd_out failed!\n");
            continue;
        }
        buf[strlen(buf) - 1] = '\0';
        PRINT("strlen(buf) = %d\n", strlen(buf));
        PRINT("buf = %s\n", buf);
        if ((strlen(buf) == 0) || (memcmp("/var/terminal_init/log/operation_steps/", buf, strlen("/var/terminal_init/log/operation_steps/")) != 0)) 
        {
            //system("terminal_init -i &");
            if ((res = common_tools.start_up_application("/bin/terminal_init", app_argv, 1)) < 0)
            {
                PERROR("start_up_application failed!\n");
                continue;
            }
            sleep(2);
            PRINT("terminal_init para check!\n");
        }
        else
        {
            PRINT("terminal_init para config ok...\n");
			#if SMART_RECOVERY == 1
            memset(buf, 0, sizeof(buf));
            if (common_tools.get_cmd_out(terminal_init_para_check_cmd2, buf, sizeof(buf)) < 0)
            {
                PERROR("get_cmd_out failed!\n");
                continue;
            }
            buf[strlen(buf) - 1] = '\0';
            PRINT("strlen(buf) = %d\n", strlen(buf));
            PRINT("buf = %s\n", buf);
            if ((strlen(buf) == 0) || (memcmp("/var/terminal_init/log/operation_steps/", buf, strlen("/var/terminal_init/log/operation_steps/")) != 0)) 
            {
                //system("terminal_init -i &");
                if ((res = common_tools.start_up_application("/bin/terminal_init", app_argv, 1)) < 0)
                {
                    PERROR("start_up_application failed!\n");
                    continue;
                }
                sleep(2);
                PRINT("terminal_init para check!\n");
            }
            #endif
            break;
        }  
    }   
    
    
    
    // 需要查看register_state和交易周期
    char * const stub_app_argv[] = {"spi_rt_main", NULL};
    char * const terminal_init_app_argv[] = {"terminal_init", NULL};
    char * const swipe_card_app_argv[] = {"swipe_card", NULL};
    while (1)
    {
        // STUB
        if (common_tools.get_cmd_out(spi_cmd, buf, sizeof(buf)) < 0)
        {
            PERROR("get_cmd_out failed!\n");
            continue;
        }
        else
        {
            if (strlen(buf) == 0)
            {
                PRINT("spi_rt_main stop!\n");
                //system("spi_rt_main &");
                if ((res = common_tools.start_up_application("/bin/spi_rt_main", stub_app_argv, 0)) < 0)
                {
                    PERROR("start_up_application failed!\n");
                    continue;
                }
                PRINT("spi_rt_main restart!\n");
            }
            //PRINT("spi_rt_main is running...\n");
        }
        
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
                //system("terminal_init &");
                if ((res = common_tools.start_up_application("/bin/terminal_init", terminal_init_app_argv, 0)) < 0)
                {
                    PERROR("start_up_application failed!\n");
                    continue;
                }
                PRINT("terminal_init restart!\n");
            }
            //PRINT("terminal_init is running...\n");
        }
        
        // 刷卡
        if (common_tools.get_cmd_out(swipe_card_cmd, buf, sizeof(buf)) < 0)
        {
            PERROR("get_cmd_out failed!\n");
            continue;
        }
        else
        {
            if (strlen(buf) == 0)
            {
                PRINT("swipe_card stop!\n");
                if ((res = common_tools.start_up_application("/bin/swipe_card", swipe_card_app_argv, 0)) < 0)
                {
                    PERROR("start_up_application failed!\n");
                    continue;
                }
                PRINT("swipe_card restart!\n");
            }
            //PRINT("swipe_card is running...\n");
        }
        
        #if SSID3_MONITOR
        // 监听SSID3，当每次生成命令命令时开始计时，当存活时间大于15分钟时，清除SSID3
        pthread_mutex_lock(&ssid3_mutex);
        PRINT("ssid3_live_flag = %d\n", ssid3_live_flag);
        PRINT("LOCAL_IP = %s, LOCAL_PORT = %s\n", LOCAL_IP, LOCAL_PORT);
        if (ssid3_live_flag == 1)
        {
            gettimeofday(&tv_end, NULL);
            PRINT("tv_end.tv_sec - tv_start.tv_sec = %d s\n", tv_end.tv_sec - tv_start.tv_sec);
            if ((tv_end.tv_sec - tv_start.tv_sec) >= 15 * 60)
            {
                // 清除SSID3
                if ((client_fd = internetwork_communication.make_client_link(LOCAL_IP, LOCAL_PORT)) < 0)
                {
                    PERROR("make_client_link failed!\n");
                }
                else
                {
                    send_buf[0] = 0x5A;
                    send_buf[1] = 0xA5;
                    send_buf[2] = 0x23;
                    send_buf[3] = 0x00;
                    
                    send_buf[4] = 0x06;
                    
                    memset(columns_name[0], 0, sizeof(columns_name[0]));
            	    memset(columns_value[0], 0, sizeof(columns_value[0]));
            	    
            	    strcpy(columns_name[0], "pad_sn");
            	    if ((res = nvram_interface.select(RT5350_FREE_SPACE, 1, columns_name, columns_value)) < 0)
                	{
                	    PERROR("nvram_select failed!\n");
                	    continue;
                	}
                    
                    memcpy(send_buf + 5, columns_value[0], 34);
                    send_buf[39] = common_tools.get_checkbit(send_buf + 4, NULL, 0, 35, XOR, 1);
                    
                    tv.tv_sec = 5;
                    if ((res = common_tools.send_data(client_fd, send_buf, NULL, sizeof(send_buf), &tv)) < 0)
                    {
                        PERROR("send_data failed!\n");
                    }
                    else
                    {
                        if ((res = common_tools.recv_data(client_fd, recv_buf, NULL, sizeof(recv_buf), &tv)) < 0)
                        {
                            PERROR("recv_data failed!\n");
                        }
                        else
                        {
                            // 正确信息
                            if ((recv_buf[0] == 0x5A) && (recv_buf[1] == 0xA5) && (recv_buf[2] == 0x01) &&
                	        (recv_buf[3] == 0x00) && (recv_buf[4] == 0x00) && (recv_buf[5] == 0x00))
                            {
                                ssid3_live_flag = 0;
                                memset(&tv_start, 0, sizeof(struct timeval));
                                memset(&tv_end, 0, sizeof(struct timeval));
                            }
                        }
                    }
                    memset(send_buf, 0, sizeof(send_buf));
                    memset(recv_buf, 0, sizeof(recv_buf));
                    close(client_fd);
                    client_fd = 0;
                }
            }
        }
        pthread_mutex_unlock(&ssid3_mutex);
        #endif
        
        #if SMART_RECOVERY == 1// 路由智能恢复
        if ((res = smart_recovery()) < 0)
        {
            PERROR("smart_recovery failed!\n");
        }
        #endif
        
        // base状态
        if (register_flag == 1)
        {
            // 生成base状态文件
            if (system("top -b -n 1 > /var/linphone/log/.base_state") < 0)
            {
                PERROR("system failed!\n");
                continue;
            }
            sleep(business_cycle);
        }
        else
        {
            sleep(5);
        }
    }
    #if SSID3_MONITOR
    return (void *)0;
    #else
    return 0;
    #endif
}

#if SSID3_MONITOR 

/**
 * 监听客户端连接
 */
static void * accept_link(void* para)
{
    int res = 0;
    int i = 0;
    int client_fd = 0;
    int server_fd = 0;
    
    unsigned char recv_buf[6] = {0};
    unsigned char send_buf[6] = {0x5A, 0xA5, 0x01, 0x00, 0x00, 0x00};
    
    fd_set fdset;
    struct timeval tv;
    struct timeval timeout;
    memset(&tv, 0, sizeof(struct timeval));
    memset(&timeout, 0, sizeof(struct timeval));
    
    #if 0 // 本地socket时
    struct sockaddr_un client; 
    #else
    struct sockaddr_in client; 
    #endif
    
    socklen_t len = sizeof(client); 
    
    #if 0
    if ((server_fd = internetwork_communication.make_local_socket_server_link(TERMINAL_LOCAL_SOCKET_NAME)) < 0)
    {
        PERROR("make_local_socket_server_link failed!\n");         
        res = server_fd;
        return (void *)res;
    }
    PRINT("The server is established successfully! TERMINAL_LOCAL_SOCKET_NAME = %s\n", TERMINAL_LOCAL_SOCKET_NAME);    
    #else
    if ((server_fd = internetwork_communication.make_server_link(TERMINAL_LOCAL_SERVER_PORT)) < 0)
    {
        PERROR("make_server_link failed!\n");         
        res = server_fd;
        return (void *)res;
    }
    PRINT("The server is established successfully! TERMINAL_LOCAL_SERVER_PORT = %s\n", TERMINAL_LOCAL_SERVER_PORT);    
    #endif
    
    while (1) 
    {
        FD_ZERO(&fdset);
        FD_SET(server_fd, &fdset);       

        tv.tv_sec = 2;
        tv.tv_usec = 0;
        switch(select(server_fd + 1, &fdset, NULL, NULL, &tv))
        {
            case -1:
            case 0:
            {              
                PRINT("[accept_link process is running ...]\n");
                continue;
            }
            default:
            {
                if (FD_ISSET(server_fd, &fdset) > 0)
                {
                    if ((client_fd = accept(server_fd, (struct sockaddr*)&client, &len)) < 0)
                	{	
                        PERROR("accept failed!\n");
                        continue;
                	}
                    PRINT("server_fd = %d, client_fd = %d\n", server_fd, client_fd);
                    
                    timeout.tv_sec = 1;
                    for (i = 0; i < 3; i++)
                    {
                        if ((res = common_tools.recv_data(client_fd, recv_buf, NULL, sizeof(recv_buf), &timeout)) < 0)
                        {	
                            PERROR("recv_data failed!\n");
                            sleep(1);
                            continue;
                    	}
                	    
                	    if ((recv_buf[0] == 0x5A) && (recv_buf[1] == 0xA5) && (recv_buf[2] == 0x01) &&
                	        (recv_buf[3] == 0x00) && (recv_buf[4] == 0x00) && (recv_buf[5] == 0x00))
                	    {
                	        pthread_mutex_lock(&ssid3_mutex);
                        	ssid3_live_flag = 1;
                        	memset(&tv_start, 0, sizeof(struct timeval));
                            memset(&tv_end, 0, sizeof(struct timeval));
                            gettimeofday(&tv_start, NULL);
                            pthread_mutex_unlock(&ssid3_mutex);
                            
                            send_buf[4] = 0x00;
                            if (common_tools.send_data(client_fd, send_buf, NULL, sizeof(send_buf), &timeout) < 0)
                            {	
                                PERROR("send_data failed!\n");
                                continue;
                        	}
                	    }
                	    else
                	    {
                	        send_buf[4] = 0xFF;
                	        if (common_tools.send_data(client_fd, send_buf, NULL, sizeof(send_buf), &timeout) < 0)
                            {	
                                PERROR("recv_data failed!\n");
                                continue;
                        	}
                	        continue;
                	    }
                	    break;
                    }
                }
                
            }
        }
        memset(recv_buf, 0, sizeof(recv_buf));
        if (client_fd > 0)
        {
            close(client_fd);
            client_fd = 0;
        }
    }
    return NULL;
}

int main(int argc, char ** argv)
{    
    // 信号捕获和处理 
    signal(SIGCHLD, signal_handle);
    signal(SIGINT, signal_handle);
    signal(SIGTERM, signal_handle);
    
    strcpy(common_tools.argv0, argv[0]);
    
    pthread_t pthread_accept_link_id, pthread_monitor_app_id; // 线程ID
    
    // STUB启动提示
    #if PRINT_DEBUG
    PRINT("[monitor_application (%s %s)]\n", __DATE__, __TIME__);
    PRINT("[monitor_application is running...]\n", __DATE__, __TIME__);
    #else
    printf("[%s]%s["__FILE__"][%s][%05d] [monitor_application (%s %s)]\n", common_tools.argv0, common_tools.get_datetime_buf(), __FUNCTION__, __LINE__, __DATE__, __TIME__);
    printf("[%s]%s["__FILE__"][%s][%05d] [monitor_application is running...]\n", common_tools.argv0, common_tools.get_datetime_buf(), __FUNCTION__, __LINE__);
    #endif
    
    // 初始化互斥锁、创建线程
    pthread_mutex_init(&ssid3_mutex, NULL);
    pthread_create(&pthread_accept_link_id, NULL, (void*)accept_link, NULL); 
    pthread_create(&pthread_monitor_app_id, NULL, (void*)monitor_app, NULL); 
    
    pthread_join(pthread_accept_link_id, NULL);
    pthread_join(pthread_monitor_app_id, NULL);
    pthread_mutex_destroy(&ssid3_mutex);
    return 0;
}

#else

int main(int argc, char ** argv)
{    
    // 信号捕获和处理 
    signal(SIGCHLD, signal_handle);
    signal(SIGINT, signal_handle);
    signal(SIGTERM, signal_handle);
    
    strcpy(common_tools.argv0, argv[0]);
    
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
#endif
