#include "common_tools.h"
#include "terminal_authentication.h"
#include "terminal_register.h"
#include "network_config.h"

#define LOCAL_IP "127.0.0.1"
// 终端初始化 测试
int main(int argc, char **argv)
{
    //signal(SIGBUS, SIG_IGN);
    int res = 0;
    strcpy(common_tools.argv0, argv[0]);
    PRINT("terminal_init_test (%s %s)\n", __DATE__, __TIME__);
    
    if ((argc == 2) && (memcmp("-help", argv[1], strlen("-help")) == 0))
    {
        PRINT("cmd option test_num delay(usleep)\n");
        #if CTSI_SECURITY_SCHEME == 1
        PRINT("a 终端认证\n");
        #endif
        PRINT("b 获取SIP服务器信息\n");
        PRINT("c 发送挂机命令\n");
        PRINT("d 呼叫指定号码 例：terminal_init_test d 8109 1\n");
        PRINT("e 发建链应答\n");
        PRINT("f 呼叫 发送建链请求 接收建链应答 挂机\n");
        PRINT("g 获取base wan mac\n");
        PRINT("h 获取base sn and mac\n");
        PRINT("i 获取随机数\n");
        PRINT("j 转换成正常格式MAC地址 如 00:AA:11:BB:CC:DD\n");
        PRINT("k 发交易报文87\n");
        PRINT("l 发送摘机命令\n");
        PRINT("m 发送82 接收84\n");
        PRINT("n 发送重发命令\n");
        PRINT("o 发送读磁条卡命令\n");
        PRINT("p 接收建链、发交易报文87\n");
        PRINT("q 接收来电、接收建链、发交易报文87、挂机\n");
        PRINT("r 启动一个进程\n");
        PRINT("s 发送(SSID3计时)命令\n");
        #if CTSI_SECURITY_SCHEME == 2
        PRINT("t 获取本地存储的设备令牌并返回\n");
        PRINT("u 获取本地存储的位置令牌并返回\n");
        PRINT("v 从平台重获取设备令牌并返回\n");
        PRINT("w 从平台重获取位置令牌并返回\n");
        #endif
        PRINT("x 检测WAN口状态\n");
        PRINT("y 连接服务器\n");
        return 0;
    }
    if (argc != 4)
    {
        PRINT("cmd option test_num delay(usleep)\n");
        return -1;
    }
    if ((res = common_tools.get_config()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_config failed!", res);
        return res;
    }
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "The configuration file is read successfully!", 0);
    
    char fsk_81_send_buf[14] = {0xA5, 0x5A, 0x76, 0x09, 0x81, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x85, 0x7E};
    char fsk_82_send_buf[14] = {0xA5, 0x5A, 0x76, 0x09, 0x82, 0x00, 0x05, 0xCD, 0xD0, 0x53, 0x43, 0x00, 0x46, 0xB3};
    char fsk_87_send_buf[] = {0xA5, 0x5A, 0x76, 0x64, 0x87, 0x00, 0x60, 0x51, 0xDC, 0xB0, 0x74, 0x02, 0x00, 0x59, 0x02, 0x00, 0x20, 0x01, 0x20, 0x13, 0x01, 0x09, 0x31, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00, 0x20, 0x31, 0x35, 0x30, 0x03, 0x0C, 0x92, 0x0C, 0x28, 0x0C, 0x2B, 0x00, 0x39, 0x22, 0x30, 0x31, 0x41, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x31, 0x30, 0x30, 0x33, 0x31, 0x32, 0x31, 0x32, 0x32, 0x30, 0x30, 0x31, 0x42, 0x38, 0x35, 0x41, 0x46, 0x45, 0x46, 0x46, 0x46, 0x30, 0x30, 0x44, 0x04, 0x37, 0x45, 0xB1, 0x74, 0x08, 0x51, 0x9B, 0x50, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x52, 0x5E};
    int i = 0;    
    char fsk_84_recv_buf[128] = {0};
    char fsk_81_recv_buf[9] = {0};
    char fsk_82_recv_buf[9] = {0};
    
    unsigned char hook_flag = 0;
    char log_bug[4096] = {0};
    char fsk_len[2] = {0};
    unsigned char sync_data_count = 0;
    unsigned short len = 0;
    unsigned char tmp = 0;
    
    unsigned int count = 0;
    unsigned int success_count = 0;
    unsigned int failed_count = 0;
    
    char device_token[100] = {0};
    char position_token[100] = {0};
    
    strcpy(log_bug, "failed log:");
    for (i = 0; i < atoi(argv[2]); i++)
    {
        if (strcmp(argv[1], "a") == 0) // 终端认证
        {
            #if 0
            if (common_tools.get_phone_stat() < 0)
            {
                PRINT("get_phone_stat failed!\n");
                return res;
            }
            #endif
            #if CTSI_SECURITY_SCHEME == 1
            if ((res = terminal_authentication.insert_token()) < 0)
            {
                PRINT("insert_token failed!\n");
                goto EXIT;
            }
            #endif
        }
        else if (strcmp(argv[1], "b") == 0) // 获取SIP服务器信息
        {
            if ((res = terminal_register.get_sip_info("01A102010033331212200158124318D4D7", "58124318D4D7")) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_sip_info failed!", res);
                return res;
            }
        }
        else if (strcmp(argv[1], "c") == 0) // 挂机
        {
            if ((res = communication_stc.cmd_on_hook()) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_on_hook failed!", res);
                return res;
            }
            return 0;
        }
        else if (strcmp(argv[1], "d") == 0) // 呼叫
        {
            if ((res = communication_stc.cmd_call(argv[2])) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_call failed!", res);
                return res;
            }
            sleep(50);
            return 0;
        }
        else if (strcmp(argv[1], "e") == 0) // 发建链应答
        {
        	if ((res = spi_rt_interface.send_data(UART1, fsk_82_send_buf, sizeof(fsk_82_send_buf)))< 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
            	goto EXIT; 
            }
        }
        else if (strcmp(argv[1], "f") == 0) // 呼叫 发送建链请求 接收建链应答 挂机
        {
            // 呼叫
            if ((res = communication_stc.cmd_call(argv[2])) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_call failed!", res);
                return res;
            }
            
            sleep(4);
            
            // 发送建链请求
        	if ((res = spi_rt_interface.send_data(UART1, fsk_81_send_buf, sizeof(fsk_81_send_buf)))< 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
            	return res; 
            }
            
            // 接收建链应答
            if ((res = spi_rt_interface.recv_data(UART1, fsk_82_recv_buf, sizeof(fsk_82_recv_buf)))< 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
            	return res; 
            }
            PRINT_BUF_BY_HEX(fsk_82_recv_buf, NULL, sizeof(fsk_82_recv_buf), __FILE__, __FUNCTION__, __LINE__);
            
            sleep(2);
            if ((res = communication_stc.cmd_on_hook()) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_on_hook failed!", res);
                return res;
            }
            return 0;
        }
        else if (strcmp(argv[1], "g") == 0) // 获取base wan mac
        {
            char mac[18] = {0};
            if ((res = common_tools.get_wan_mac(mac)) < 0)
            {
                PRINT("get_wan_mac failed!\n");
                return res;
            }
            PRINT("mac = %s\n", mac);
        }
        else if (strcmp(argv[1], "h") == 0) // 获取base sn and mac
        {
            char sn[35] = {0};
            char mac[18] = {0};
            if ((res = terminal_register.get_base_sn_and_mac(sn, mac)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_base_sn_and_mac failed!", res);
                return res;
            }
            PRINT("sn = %s, mac = %s\n", sn, mac);
        }
        else if (strcmp(argv[1], "i") == 0) // 获取随机数
        {
            char rand_str[25] = {0};
            
            if ((res = common_tools.get_rand_string(12, 0, rand_str, UTF8)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_rand_string failed", res);
                return res;
            }
            PRINT("rand_str = %s\n", rand_str);
        }
        else if (strcmp(argv[1], "j") == 0) // 转换成正常格式MAC地址 如 00:AA:11:BB:CC:DD
        {
            char mac[18] = "0:f1:AA:1:dc:ea";
                        
            if ((res = common_tools.mac_format_conversion(mac)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "mac_format_conversion failed", res);
                return res;
            }
            PRINT("mac = %s\n", mac);
            return 0;
        }
        else if (strcmp(argv[1], "k") == 0) // 发交易报文87
        {
        	if ((res = spi_rt_interface.send_data(UART1, fsk_87_send_buf, sizeof(fsk_87_send_buf)))< 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
            	goto EXIT; 
            }          
        }
        else if (strcmp(argv[1], "l") == 0) // 摘机
        {
            if ((res = communication_stc.cmd_off_hook()) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_off_hook failed!", res);
                return res;
            }
            return 0;
        }
        else if (strcmp(argv[1], "m") == 0) // 发送82 接收84
        {
        	if ((res = spi_rt_interface.send_data(UART1, fsk_82_send_buf, sizeof(fsk_82_send_buf)))< 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
            	goto EXIT;
            }
            else
            {
                while (1)
                {
                    if ((res = spi_rt_interface.recv_data(UART1, &tmp, sizeof(tmp))) == sizeof(tmp))
                    {
                        if (tmp == 0x55)
                        {   
                            sync_data_count++;
                            if (sync_data_count >= 5)
                            {
                                break;
                            }
                        }
                        else
                        {
                            sync_data_count = 0;
                        }
                    }
                    else
                    {
                        goto EXIT;
                    }
                }
                
                while (1)
                {
                    if ((res = spi_rt_interface.recv_data(UART1, &tmp, sizeof(tmp))) == sizeof(tmp))
                    {
                        if (tmp == 0x84)
                        {   
                            break;
                        }
                    }
                    else
                    {
                        goto EXIT;
                    }
                }
                
                fsk_84_recv_buf[0] = tmp;
                if ((res = spi_rt_interface.recv_data(UART1, fsk_len, sizeof(fsk_len))) != sizeof(fsk_len))
                {
                    PERROR("spi_rt_interface.recv_data failed!\n");
                    goto EXIT;
                }
                fsk_84_recv_buf[1] = fsk_len[0];
                fsk_84_recv_buf[2] = fsk_len[1];
                len = fsk_len[1];
                len += (fsk_len[0] << 8);
                
                if ((res = spi_rt_interface.recv_data(UART1, fsk_84_recv_buf + 3, len + 1)) != (len + 1))
                {
                    PERROR("spi_rt_interface.recv_data failed!\n");
                    goto EXIT;
                }
                PRINT_BUF_BY_HEX(fsk_84_recv_buf, NULL, len + 4, __FILE__, __FUNCTION__, __LINE__);
            }
            
            // 资源释放
            sync_data_count = 0;
            memset(fsk_len, 0, sizeof(fsk_len));
            memset(fsk_84_recv_buf, 0, sizeof(fsk_84_recv_buf));
            len = 0;
            tmp = 0;
        }
        else if (strcmp(argv[1], "n") == 0) // 重发命令
        {
            tmp = 0x08;
            
        	if ((res = spi_rt_interface.send_data(EXPAND, &tmp, sizeof(tmp)))< 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
            	goto EXIT;
            }
            
            tmp = 0;
        }
        else if (strcmp(argv[1], "o") == 0) // 发送读磁条卡命令
        {
            char buf[7] = {0};
            buf[0] = 0xA5;
            buf[1] = 0x01;
            buf[2] = 0x03;
            buf[3] = 0x15;
            buf[4] = 0x00;
            buf[5] = 0x00;
            buf[6] = 0x17;
            
            if ((res = spi_rt_interface.send_data(UART2, buf, 7))< 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
            	goto EXIT;
            }
            memset(buf, 0, sizeof(buf));
        }
        else if (strcmp(argv[1], "p") == 0) // 接收81 发送87
        {
            while (1)
            {
                if ((res = spi_rt_interface.recv_data(UART1, &tmp, sizeof(tmp))) == sizeof(tmp))
                {
                    if (tmp == 0x55)
                    {   
                        sync_data_count++;
                        if (sync_data_count >= 5)
                        {
                            break;                            
                        }
                    }
                    else
                    {
                        sync_data_count = 0;
                    }
                }
                else
                {
                    goto EXIT;
                }
            }
            
            while (1)
            {
                if ((res = spi_rt_interface.recv_data(UART1, &tmp, sizeof(tmp))) == sizeof(tmp))
                {
                    if (tmp == 0x81)
                    {   
                        break;
                    }
                }
                else
                {
                    goto EXIT;
                }
            }
                         
            fsk_81_recv_buf[0] = tmp;
            if ((res = spi_rt_interface.recv_data(UART1, fsk_len, sizeof(fsk_len))) != sizeof(fsk_len))
            {
                PERROR("spi_rt_interface.recv_data failed!\n");
                goto EXIT;
            }
            fsk_81_recv_buf[1] = fsk_len[0];
            fsk_81_recv_buf[2] = fsk_len[1];
            len = fsk_len[1];
            len += (fsk_len[0] << 8);
             
            if ((res = spi_rt_interface.recv_data(UART1, fsk_81_recv_buf + 3, len + 1)) != (len + 1))
            {
                PERROR("spi_rt_interface.recv_data failed!\n");
                goto EXIT;
            }
            PRINT_BUF_BY_HEX(fsk_81_recv_buf, NULL, len + 4, __FILE__, __FUNCTION__, __LINE__);
            
            // 资源释放
            sync_data_count = 0;
            memset(fsk_len, 0, sizeof(fsk_len));
            memset(fsk_81_recv_buf, 0, sizeof(fsk_81_recv_buf));
            len = 0;
            tmp = 0;
            
        	if ((res = spi_rt_interface.send_data(UART1, fsk_87_send_buf, sizeof(fsk_87_send_buf)))< 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
            	goto EXIT; 
            }
        }
        else if (strcmp(argv[1], "q") == 0) // 接收来电、接收建链、发交易报文87、挂机
        {
            #if CTSI_SECURITY_SCHEME == 1
            // 接收来电
            if ((res = communication_stc.recv_display_msg()) < 0)
            {
                PERROR("recv_display_msg failed!\n");
                goto EXIT;
            }
            #else
            // 接收来电
            if ((res = communication_stc.recv_display_msg(common_tools.config->center_phone)) < 0)
            {
                PERROR("recv_display_msg failed!\n");
                goto EXIT;
            }
            #endif
            // 摘机
            if ((res = communication_stc.cmd_off_hook()) < 0)
            {
                PERROR("cmd_off_hook failed!\n");
                goto EXIT;
            }
            
            // 接收建链
            while (1)
            {
                if ((res = spi_rt_interface.recv_data(UART1, &tmp, sizeof(tmp))) == sizeof(tmp))
                {
                    if (tmp == 0x55)
                    {   
                        sync_data_count++;
                        if (sync_data_count >= 5)
                        {
                            break;                            
                        }
                    }
                    else
                    {
                        sync_data_count = 0;
                    }
                }
                else
                {
                    hook_flag = 1;
                    goto EXIT;
                }
            }
            
            while (1)
            {
                if ((res = spi_rt_interface.recv_data(UART1, &tmp, sizeof(tmp))) == sizeof(tmp))
                {
                    if (tmp == 0x81)
                    {   
                        break;
                    }
                }
                else
                {
                    hook_flag = 1;
                    goto EXIT;
                }
            }
                         
            fsk_81_recv_buf[0] = tmp;
            if ((res = spi_rt_interface.recv_data(UART1, fsk_len, sizeof(fsk_len))) != sizeof(fsk_len))
            {
                PERROR("spi_rt_interface.recv_data failed!\n");
                hook_flag = 1;
                goto EXIT;
            }
            fsk_81_recv_buf[1] = fsk_len[0];
            fsk_81_recv_buf[2] = fsk_len[1];
            len = fsk_len[1];
            len += (fsk_len[0] << 8);
             
            if ((res = spi_rt_interface.recv_data(UART1, fsk_81_recv_buf + 3, len + 1)) != (len + 1))
            {
                PERROR("spi_rt_interface.recv_data failed!\n");
                hook_flag = 1;
                goto EXIT;
            }
            PRINT_BUF_BY_HEX(fsk_81_recv_buf, NULL, len + 4, __FILE__, __FUNCTION__, __LINE__);
            
            // 资源释放
            sync_data_count = 0;
            memset(fsk_len, 0, sizeof(fsk_len));
            memset(fsk_81_recv_buf, 0, sizeof(fsk_81_recv_buf));
            len = 0;
            tmp = 0;
            
            // 发送87
        	if ((res = spi_rt_interface.send_data(UART1, fsk_87_send_buf, sizeof(fsk_87_send_buf)))< 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
                hook_flag = 1;
            	goto EXIT; 
            }
            sleep(2);
            // 挂机
            if ((res = communication_stc.cmd_on_hook()) < 0)
            {
                PERROR("cmd_on_hook failed!\n");
                hook_flag = 1;
                goto EXIT;
            }
        }
        else if (strcmp(argv[1], "r") == 0) // 启动一个进程
        {
            char * const app_argv[] = {"top", "-b", "-n", "1", NULL};
            if ((res = common_tools.start_up_application("/usr/bin/top", app_argv, 0)) < 0)
            {
                PERROR("start_up_application failed!\n");
                return res;
            }
            return res;
        }
        else if (strcmp(argv[1], "s") == 0) // 发送(SSID3计时)命令
        {
            int i = 0;
            int client_fd = 0;
            struct timeval timeout;
            memset(&timeout, 0, sizeof(struct timeval));
            unsigned char recv_buf[6] = {0};
            unsigned char send_buf[6] = {0x5A, 0xA5, 0x01, 0x00, 0x00, 0x00};
            
            #if 0
            // 创建客户端连接
            if ((client_fd = internetwork_communication.make_local_socket_client_link(TERMINAL_LOCAL_SOCKET_NAME)) < 0)
            {
                PERROR("make_client_link failed!\n");
                res = client_fd;
                client_fd = 0;
                return res;
            }
            #else
            // 创建客户端连接
            PRINT("LOCAL_IP = %s, TERMINAL_LOCAL_SERVER_PORT = %s\n", LOCAL_IP, TERMINAL_LOCAL_SERVER_PORT);
            if ((client_fd = internetwork_communication.make_client_link(LOCAL_IP, TERMINAL_LOCAL_SERVER_PORT)) < 0)
            {
                PERROR("make_client_link failed!\n");
                res = client_fd;
                client_fd = 0;
                return res;
            }
            #endif
            PRINT("client_fd = %d\n", client_fd);
            
            for (i = 0; i < 3; i++)
            {
                if (common_tools.send_data(client_fd, send_buf, NULL, sizeof(send_buf), &timeout) < 0)
                {	
                    PERROR("send_data failed!\n");
                    continue;
                }
                PRINT("after send_data\n");
                if ((res = common_tools.recv_data(client_fd, recv_buf, NULL, sizeof(recv_buf), &timeout)) < 0)
                {	
                    PERROR("recv_data failed!\n");
                    sleep(1);
                    continue;
                }
                	    
                if ((recv_buf[0] == 0x5A) && (recv_buf[1] == 0xA5) && (recv_buf[2] == 0x01) &&
                	(recv_buf[3] == 0x00) && (recv_buf[4] == 0x00) && (recv_buf[5] == 0x00))
                {
                    PRINT("________________________________________\n");
                    close(client_fd);
                    client_fd = 0;
                    return 0;
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
                return res;
            }
        }
        #if CTSI_SECURITY_SCHEME == 2
        else if (strcmp(argv[1], "t") == 0) // 获取本地存储的设备令牌并返回
        {
            if ((res = terminal_authentication.return_device_token(device_token)) < 0)
            {
                PERROR("return_device_token failed!\n");
                return res;
            }
            PRINT("device_token = %s\n", device_token);
            PRINT_BUF_BY_HEX(device_token, NULL, sizeof(device_token), __FILE__, __FUNCTION__, __LINE__);
            return 0;
        }
        else if (strcmp(argv[1], "u") == 0) // 获取本地存储的位置令牌并返回
        {
            if ((res = terminal_authentication.return_position_token(position_token)) < 0)
            {
                PERROR("return_position_token failed!\n");
                return res;
            }
            PRINT("position_token = %s\n", position_token);
            return 0;
        }
        else if (strcmp(argv[1], "v") == 0) // 从平台重获取设备令牌并返回
        {
            if ((res = terminal_authentication.rebuild_device_token(device_token)) < 0)
            {
                PERROR("rebuild_device_token failed!\n");
                return res;
            }
            PRINT("device_token = %s\n", device_token);
        }
        else if (strcmp(argv[1], "w") == 0) // 从平台重获取位置令牌并返回
        {
            if ((res = terminal_authentication.rebuild_position_token(position_token)) < 0)
            {
                PERROR("rebuild_position_token failed!\n");
                return res;
            }
            PRINT("position_token = %s\n", position_token);
            return 0;
        }
        #endif
        else if (strcmp(argv[1], "x") == 0) // 检测WAN口
        {
            if ((res = network_config.get_wan_state()) < 0)
            {
                PERROR("get_wan_state failed!\n");
                return res;
            }
            return 0;
        }
        else if (strcmp(argv[1], "y") == 0) // 检测WAN口
        {
            PRINT("ip = %s port = %s\n", argv[2], argv[3]);
            if ((res = internetwork_communication.make_client_link(argv[2], argv[3])) < 0)
            {
                PERROR("make_client_link failed!\n");
                return res;
            }
            PRINT("link success!\n");
            close(res);
            return 0;
        }
        else
        {  
            if ((res = terminal_register.user_register("01A102010033331212200158124318D4D7", "58124318D4D7")) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "user_register failed!", res);
                return res;
            }
        }

EXIT:
        count++;
        if (res < 0)
        {
            if (hook_flag == 1)
            {
                if ((res = communication_stc.cmd_on_hook()) < 0)
                {
                    PERROR("cmd_on_hook failed!\n");
                    failed_count++;
                    sprintf(log_bug, "%s %d", log_bug, count);
                    PRINT("count = %d, failed_count = %d, success_count = %d, success_count / count = %%%3.6f\n", count, failed_count, success_count, 100.0 * ((float)success_count / (float)count));
                    break;
                }
            }
            hook_flag = 0;
            failed_count++;
            sprintf(log_bug, "%s %d", log_bug, count);
        }
        else if (res >= 0)
        {
            success_count++;
        }
        PRINT("count = %d, failed_count = %d, success_count = %d, success_count / count = %%%3.6f\n", count, failed_count, success_count, 100.0 * ((float)success_count / (float)count));
        usleep(atoi(argv[3]));
    }
    
    PRINT("%s\n", log_bug);
    return 0;
}
