/*************************************************************************
    > File Name: interface_test.c
    > Author: yangjilong
    > mail1: yangjilong65@163.com
    > mail2: yang.jilong@handaer.com
    > Created Time: 2014年02月10日 星期一 14时09分51秒
**************************************************************************/
#include "common_tools.h"
#include "terminal_authentication.h"
#include "terminal_register.h"
#include "network_config.h"
#include "communication_usb.h"
#include "communication_serial.h"

#define LOCAL_IP "127.0.0.1"

#define CMD_a "a"
#define CMD_b "b"
#define CMD_c "c"
#define CMD_d "d"
#define CMD_e "e"
#define CMD_f "f"
#define CMD_g "g"
#define CMD_h "h"
#define CMD_i "i"
#define CMD_j "j"
#define CMD_k "k"
#define CMD_l "l"
#define CMD_m "m"

#if PHONE_CHANNEL_INTERFACE == 2
#define CMD_n "n"
#define CMD_o "o"
#endif // PHONE_CHANNEL_INTERFACE == 2

#define CMD_p "p"
#define CMD_q "q"
#define CMD_r "r"
#define CMD_s "s"
#define CMD_t "t"
#define CMD_u "u"
#define CMD_v "v"
#define CMD_w "w"
#define CMD_x "x"
#define CMD_y "y"
#define CMD_z "z"

#define CMD_B "B"
#define CMD_C "C"
#define CMD_L "L"
#define CMD_R "R"


#define CMD_print "print" // 打印设置的环境变量
#define CMD_set "set" // 设置指定环境变量的值

#define CMD_help "help" // 打印帮助

#define CMD_quit "quit" // 退出测试程序
#define CMD_exit "exit"  // 退出测试程序


static unsigned short old_cmd_count = 0; // 上次命令的参数个数
static char old_cmd_buf[1024] = {0}; // 上次执行的命令

static int delay_us = 0; // 延时 微秒数
static int loop_count = 1; // 执行命令次数

static unsigned char hook_flag = 0; 

/**
 * 命令字 a 终端认证
 */
int cmd_a()
{
    int res = 0;
    char device_token[TOKENLEN] = {0};
    char position_token[TOKENLEN] = {0};
    
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
        return res;
    }
    #elif CTSI_SECURITY_SCHEME == 2
    if ((res = terminal_authentication.rebuild_device_token(device_token)) < 0)
    {
        PERROR("rebuild_device_token failed!\n");
        return res; 
    }
    if ((res = terminal_authentication.rebuild_position_token(position_token)) < 0)
    {
        PERROR("rebuild_position_token failed!\n");
        return res; 
    }
    #endif // CTSI_SECURITY_SCHEME == 2
    return res;
}
    
/**
 * 命令字 b 获取SIP服务器信息
 */
int cmd_b(char *pad_sn, char *pad_mac)
{
    int res = 0;
    if ((res = terminal_register.get_sip_info(pad_sn, pad_mac)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_sip_info failed!", res);
        return res;
    }
    return res;       
}

/**
 * 命令字 c 挂机
 */
int cmd_c()
{
    int res = 0;
    if ((res = communication_serial.cmd_on_hook()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_on_hook failed!", res);
        return res;
    }
    return res;
}
/**
 * 命令字 d 呼叫 phone
 */
int cmd_d(char *phone)
{
    int res = 0;
    if ((res = communication_serial.cmd_call(phone)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_call failed!", res);
        return res;
    }
    return res;
}

/**
 * 命令字 e 发建链应答
 */
int cmd_e(char *phone)
{
    int res = 0;
    char fsk_82_send_buf[14] = {0xA5, 0x5A, 0x76, 0x09, 0x82, 0x00, 0x05, 0xCD, 0xD0, 0x53, 0x43, 0x00, 0x46, 0xB3};
    
    #if PHONE_CHANNEL_INTERFACE == 2
    if ((res = spi_rt_interface.send_data(UART1, fsk_82_send_buf, sizeof(fsk_82_send_buf)))< 0)
    #elif PHONE_CHANNEL_INTERFACE == 3
    struct timeval tv = {5, 0};
    if ((res = communication_serial.send_data(fsk_82_send_buf, sizeof(fsk_82_send_buf), &tv))< 0)
    #endif // PHONE_CHANNEL_INTERFACE == 3
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
    	return res;
    }
    
    return res;
}    

/**
 * 命令字 f 呼叫 发送建链请求 接收建链应答 挂机
 */
int cmd_f(char *phone)
{
    int res = 0;
    char fsk_82_recv_buf[9] = {0};
    char fsk_81_send_buf[14] = {0xA5, 0x5A, 0x76, 0x09, 0x81, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x85, 0x7E};
    
    // 呼叫
    if ((res = communication_serial.cmd_call(phone)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_call failed!", res);
        return res;
    }
    
    sleep(4);
    
    #if PHONE_CHANNEL_INTERFACE == 2
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
    
    #elif PHONE_CHANNEL_INTERFACE == 3
    
    struct timeval tv = {5, 0};
    // 发送建链请求
    if ((res = communication_serial.send_data(fsk_81_send_buf, sizeof(fsk_81_send_buf), &tv))< 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
    	return res; 
    }
    
    // 接收建链应答
    if ((res = communication_serial.recv_data(fsk_82_recv_buf, sizeof(fsk_82_recv_buf), &tv))< 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
    	return res; 
    }
    
    #endif // PHONE_CHANNEL_INTERFACE == 3
    
    PRINT_BUF_BY_HEX(fsk_82_recv_buf, NULL, sizeof(fsk_82_recv_buf), __FILE__, __FUNCTION__, __LINE__);
    
    sleep(2);
    if ((res = communication_serial.cmd_on_hook()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_on_hook failed!", res);
        return res;
    }
    return res;
}

/**
 * 命令字 g 获取base wan mac
 */
int cmd_g()
{
    int res = 0;
    char mac[18] = {0};
    if ((res = common_tools.get_wan_mac(mac)) < 0)
    {
        PRINT("get_wan_mac failed!\n");
        return res;
    }
    PRINT("mac = %s\n", mac);
    return res;
}

/**
 * 命令字 h 获取base sn and mac
 */
int cmd_h()
{
    int res = 0;
    char sn[SN_LEN + 1] = {0};
    char mac[18] = {0};
    if ((res = terminal_register.get_base_sn_and_mac(sn, mac)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_base_sn_and_mac failed!", res);
        return res;
    }
    PRINT("sn = %s, mac = %s\n", sn, mac);
    return res;
}

/**
 * 命令字 i 获取随机数
 */
int cmd_i()
{
    int res = 0;
    char rand_str[25] = {0};
    
    if ((res = common_tools.get_rand_string(12, 0, rand_str, UTF8)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_rand_string failed", res);
        return res;
    }
    PRINT("rand_str = %s\n", rand_str);
    return res;
}

/**
 * 命令字 j 转换成正常格式MAC地址 如 00:AA:11:BB:CC:DD
 */
int cmd_j()
{
    int res = 0;
    char mac[18] = "0:f1:AA:1:dc:ea";
                
    if ((res = common_tools.mac_format_conversion(mac)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "mac_format_conversion failed", res);
        return res;
    }
    PRINT("mac = %s\n", mac);
    return res;
}

/**
 * 命令字 k 发交易报文87
 */
int cmd_k()
{
    int res = 0;
    char fsk_87_send_buf[] = {0xA5, 0x5A, 0x76, 0x64, 0x87, 0x00, 0x60, 0x51, 0xDC, 0xB0, 0x74, 0x02, 0x00, 0x59, 0x02, 0x00, 0x20, 0x01, 0x20, 0x13, 0x01, 0x09, 0x31, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00, 0x20, 0x31, 0x35, 0x30, 0x03, 0x0C, 0x92, 0x0C, 0x28, 0x0C, 0x2B, 0x00, 0x39, 0x22, 0x30, 0x31, 0x41, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x31, 0x30, 0x30, 0x33, 0x31, 0x32, 0x31, 0x32, 0x32, 0x30, 0x30, 0x31, 0x42, 0x38, 0x35, 0x41, 0x46, 0x45, 0x46, 0x46, 0x46, 0x30, 0x30, 0x44, 0x04, 0x37, 0x45, 0xB1, 0x74, 0x08, 0x51, 0x9B, 0x50, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x52, 0x5E};
    
    #if PHONE_CHANNEL_INTERFACE == 2
    if ((res = spi_rt_interface.send_data(UART1, fsk_87_send_buf, sizeof(fsk_87_send_buf)))< 0)
    #elif PHONE_CHANNEL_INTERFACE == 3
    struct timeval tv = {5, 0};
    if ((res = communication_serial.send_data(fsk_87_send_buf, sizeof(fsk_87_send_buf), &tv))< 0)
    #endif // PHONE_CHANNEL_INTERFACE == 3
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
    	return res;
    }  
    return res;        
}

/**
 * 命令字 l 摘机
 */
int cmd_l()
{
    int res = 0;
    if ((res = communication_serial.cmd_off_hook()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_off_hook failed!", res);
        return res;
    }
    return res;
}


/**
 * 命令字 m 发送82 接收84
 */
int cmd_m()
{
    int res = 0;
    unsigned char tmp = 0;
    unsigned short len = 0;    
    unsigned char sync_data_count = 0;
    char fsk_len[2] = {0};
    char fsk_84_recv_buf[128] = {0};
    char fsk_82_send_buf[14] = {0xA5, 0x5A, 0x76, 0x09, 0x82, 0x00, 0x05, 0xCD, 0xD0, 0x53, 0x43, 0x00, 0x46, 0xB3};
    
    #if PHONE_CHANNEL_INTERFACE == 2
    if ((res = spi_rt_interface.send_data(UART1, fsk_82_send_buf, sizeof(fsk_82_send_buf)))< 0)
    #elif PHONE_CHANNEL_INTERFACE == 3
    struct timeval tv = {5, 0};
    if ((res = communication_serial.send_data(fsk_82_send_buf, sizeof(fsk_82_send_buf), &tv))< 0)
    #endif // PHONE_CHANNEL_INTERFACE == 3
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
    	return res;
    }
    else
    {
        while (1)
        {
            #if PHONE_CHANNEL_INTERFACE == 2
            if ((res = spi_rt_interface.recv_data(UART1, &tmp, sizeof(tmp))) == sizeof(tmp))
            #elif PHONE_CHANNEL_INTERFACE == 3
            tv.tv_sec = 5;
            if ((res = communication_serial.recv_data(&tmp, sizeof(tmp), &tv)) == sizeof(tmp))
            #endif // PHONE_CHANNEL_INTERFACE == 3
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
                return res;
            }
        }
        
        while (1)
        {
            #if PHONE_CHANNEL_INTERFACE == 2
            if ((res = spi_rt_interface.recv_data(UART1, &tmp, sizeof(tmp))) == sizeof(tmp))
            #elif PHONE_CHANNEL_INTERFACE == 3
            tv.tv_sec = 5;
            if ((res = communication_serial.recv_data(&tmp, sizeof(tmp), &tv)) == sizeof(tmp))
            #endif // PHONE_CHANNEL_INTERFACE == 3
            {
                if (tmp == 0x84)
                {   
                    break;
                }
            }
            else
            {
                return res;
            }
        }
        
        fsk_84_recv_buf[0] = tmp;
        
        #if PHONE_CHANNEL_INTERFACE == 2
        if ((res = spi_rt_interface.recv_data(UART1, fsk_len, sizeof(fsk_len))) != sizeof(fsk_len))
        #elif PHONE_CHANNEL_INTERFACE == 3
        tv.tv_sec = 5;
        if ((res = communication_serial.recv_data(fsk_len, sizeof(fsk_len), &tv)) != sizeof(fsk_len))
        #endif // PHONE_CHANNEL_INTERFACE == 3
        {
            PERROR("spi_rt_interface.recv_data failed!\n");
            return res;
        }
        fsk_84_recv_buf[1] = fsk_len[0];
        fsk_84_recv_buf[2] = fsk_len[1];
        len = fsk_len[1];
        len += (fsk_len[0] << 8);
        
        #if PHONE_CHANNEL_INTERFACE == 2
        if ((res = spi_rt_interface.recv_data(UART1, fsk_84_recv_buf + 3, len + 1)) != (len + 1))
        #elif PHONE_CHANNEL_INTERFACE == 3
        tv.tv_sec = 5;
        if ((res = communication_serial.recv_data(fsk_84_recv_buf + 3, len + 1, &tv)) != (len + 1))
        #endif // PHONE_CHANNEL_INTERFACE == 3
        {
            PERROR("spi_rt_interface.recv_data failed!\n");
            return res;
        }
        PRINT_BUF_BY_HEX(fsk_84_recv_buf, NULL, len + 4, __FILE__, __FUNCTION__, __LINE__);
    }
    
    // 资源释放
    sync_data_count = 0;
    memset(fsk_len, 0, sizeof(fsk_len));
    memset(fsk_84_recv_buf, 0, sizeof(fsk_84_recv_buf));
    len = 0;
    tmp = 0;
    return res;
}

#if PHONE_CHANNEL_INTERFACE == 2
/**
 * 命令字 n 重发命令
 */
int cmd_n()
{
    int res = 0;
    unsigned char tmp = 0x08;
    
    if ((res = spi_rt_interface.send_data(EXPAND, &tmp, sizeof(tmp)))< 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
    	return res;
    }
    
    return res;
}

/**
 * 命令字 o 发送读磁条卡命令
 */
int cmd_o()
{
    int res = 0; 
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
    	return res;
    }
    memset(buf, 0, sizeof(buf));
    return res;
}
#endif // PHONE_CHANNEL_INTERFACE == 2

/**
 * 命令字 p 接收81 发送87
 */
int cmd_p()
{
    int res = 0;
    unsigned char tmp = 0;
    unsigned short len = 0;
    unsigned char sync_data_count = 0;
    char fsk_len[2] = {0};
    char fsk_81_recv_buf[9] = {0};
    char fsk_87_send_buf[] = {0xA5, 0x5A, 0x76, 0x64, 0x87, 0x00, 0x60, 0x51, 0xDC, 0xB0, 0x74, 0x02, 0x00, 0x59, 0x02, 0x00, 0x20, 0x01, 0x20, 0x13, 0x01, 0x09, 0x31, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00, 0x20, 0x31, 0x35, 0x30, 0x03, 0x0C, 0x92, 0x0C, 0x28, 0x0C, 0x2B, 0x00, 0x39, 0x22, 0x30, 0x31, 0x41, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x31, 0x30, 0x30, 0x33, 0x31, 0x32, 0x31, 0x32, 0x32, 0x30, 0x30, 0x31, 0x42, 0x38, 0x35, 0x41, 0x46, 0x45, 0x46, 0x46, 0x46, 0x30, 0x30, 0x44, 0x04, 0x37, 0x45, 0xB1, 0x74, 0x08, 0x51, 0x9B, 0x50, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x52, 0x5E};
    struct timeval tv = {5, 0};
    
    while (1)
    {
        #if PHONE_CHANNEL_INTERFACE == 2
        if ((res = spi_rt_interface.recv_data(UART1, &tmp, sizeof(tmp))) == sizeof(tmp))
        #elif PHONE_CHANNEL_INTERFACE == 3
        tv.tv_sec = 5;
        if ((res = communication_serial.recv_data(&tmp, sizeof(tmp), &tv)) == sizeof(tmp))
        #endif // PHONE_CHANNEL_INTERFACE == 3
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
            return res;
        }
    }
    
    while (1)
    {
        #if PHONE_CHANNEL_INTERFACE == 2
        if ((res = spi_rt_interface.recv_data(UART1, &tmp, sizeof(tmp))) == sizeof(tmp))
        #elif PHONE_CHANNEL_INTERFACE == 3
        tv.tv_sec = 5;
        if ((res = communication_serial.recv_data(&tmp, sizeof(tmp), &tv)) == sizeof(tmp))
        #endif // PHONE_CHANNEL_INTERFACE == 3
        {
            if (tmp == 0x81)
            {   
                break;
            }
        }
        else
        {
            return res;
        }
    }
                 
    fsk_81_recv_buf[0] = tmp;
    #if PHONE_CHANNEL_INTERFACE == 2
    if ((res = spi_rt_interface.recv_data(UART1, fsk_len, sizeof(fsk_len))) != sizeof(fsk_len))
    #elif PHONE_CHANNEL_INTERFACE == 3
    tv.tv_sec = 5;
    if ((res = communication_serial.recv_data(fsk_len, sizeof(fsk_len), &tv)) != sizeof(fsk_len))
    #endif // PHONE_CHANNEL_INTERFACE == 3
    {
        PERROR("spi_rt_interface.recv_data failed!\n");
        return res;
    }
    fsk_81_recv_buf[1] = fsk_len[0];
    fsk_81_recv_buf[2] = fsk_len[1];
    len = fsk_len[1];
    len += (fsk_len[0] << 8);
    
    #if PHONE_CHANNEL_INTERFACE == 2
    if ((res = spi_rt_interface.recv_data(UART1, fsk_81_recv_buf + 3, len + 1)) != (len + 1))
    #elif PHONE_CHANNEL_INTERFACE == 3
    tv.tv_sec = 5;
    if ((res = communication_serial.recv_data(fsk_81_recv_buf + 3, len + 1, &tv)) != (len + 1))
    #endif // PHONE_CHANNEL_INTERFACE == 3
    {
        PERROR("spi_rt_interface.recv_data failed!\n");
        return res;
    }
    PRINT_BUF_BY_HEX(fsk_81_recv_buf, NULL, len + 4, __FILE__, __FUNCTION__, __LINE__);
    
    // 资源释放
    sync_data_count = 0;
    memset(fsk_len, 0, sizeof(fsk_len));
    memset(fsk_81_recv_buf, 0, sizeof(fsk_81_recv_buf));
    len = 0;
    tmp = 0;
    
    #if PHONE_CHANNEL_INTERFACE == 2
    if ((res = spi_rt_interface.send_data(UART1, fsk_87_send_buf, sizeof(fsk_87_send_buf)))< 0)
    #elif PHONE_CHANNEL_INTERFACE == 3
    tv.tv_sec = 5;
    if ((res = communication_serial.send_data(fsk_87_send_buf, sizeof(fsk_87_send_buf), &tv))< 0)
    #endif // PHONE_CHANNEL_INTERFACE == 3
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
    	return res; 
    }
    return res;
}

/**
 * 命令字 q 接收来电、接收建链、发交易报文87、挂机
 */
int cmd_q()
{
    int res = 0;
    unsigned char tmp = 0;
    unsigned short len = 0;
    unsigned char sync_data_count = 0;
    char fsk_len[2] = {0};
    
    char fsk_81_recv_buf[9] = {0};
    char fsk_87_send_buf[] = {0xA5, 0x5A, 0x76, 0x64, 0x87, 0x00, 0x60, 0x51, 0xDC, 0xB0, 0x74, 0x02, 0x00, 0x59, 0x02, 0x00, 0x20, 0x01, 0x20, 0x13, 0x01, 0x09, 0x31, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00, 0x20, 0x31, 0x35, 0x30, 0x03, 0x0C, 0x92, 0x0C, 0x28, 0x0C, 0x2B, 0x00, 0x39, 0x22, 0x30, 0x31, 0x41, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x31, 0x30, 0x30, 0x33, 0x31, 0x32, 0x31, 0x32, 0x32, 0x30, 0x30, 0x31, 0x42, 0x38, 0x35, 0x41, 0x46, 0x45, 0x46, 0x46, 0x46, 0x30, 0x30, 0x44, 0x04, 0x37, 0x45, 0xB1, 0x74, 0x08, 0x51, 0x9B, 0x50, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x52, 0x5E};
    
    #if CTSI_SECURITY_SCHEME == 1
    // 接收来电
    if ((res = communication_serial.recv_display_msg()) < 0)
    {
        PERROR("recv_display_msg failed!\n");
        return res;
    }
    #else
    // 接收来电
    if ((res = communication_serial.recv_display_msg(common_tools.config->center_phone)) < 0)
    {
        PERROR("recv_display_msg failed!\n");
        return res;
    }
    #endif
    // 摘机
    if ((res = communication_serial.cmd_off_hook()) < 0)
    {
        PERROR("cmd_off_hook failed!\n");
        return res;
    }
    
    struct timeval tv = {5, 0};
    // 接收建链
    while (1)
    {
        #if PHONE_CHANNEL_INTERFACE == 2
        if ((res = spi_rt_interface.recv_data(UART1, &tmp, sizeof(tmp))) == sizeof(tmp))
        #elif PHONE_CHANNEL_INTERFACE == 3
        tv.tv_sec = 5;
        if ((res = communication_serial.recv_data(&tmp, sizeof(tmp), &tv)) == sizeof(tmp))
        #endif // PHONE_CHANNEL_INTERFACE == 3
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
            return res;
        }
    }
    
    while (1)
    {
        #if PHONE_CHANNEL_INTERFACE == 2
        if ((res = spi_rt_interface.recv_data(UART1, &tmp, sizeof(tmp))) == sizeof(tmp))
        #elif PHONE_CHANNEL_INTERFACE == 3
        tv.tv_sec = 5;
        if ((res = communication_serial.recv_data(&tmp, sizeof(tmp), &tv)) == sizeof(tmp))
        #endif // PHONE_CHANNEL_INTERFACE == 3
        {
            if (tmp == 0x81)
            {   
                break;
            }
        }
        else
        {
            hook_flag = 1;
            return res;
        }
    }
                 
    fsk_81_recv_buf[0] = tmp;
    #if PHONE_CHANNEL_INTERFACE == 2
    if ((res = spi_rt_interface.recv_data(UART1, fsk_len, sizeof(fsk_len))) != sizeof(fsk_len))
    #elif PHONE_CHANNEL_INTERFACE == 3
    tv.tv_sec = 5;
    if ((res = communication_serial.recv_data(fsk_len, sizeof(fsk_len), &tv)) != sizeof(fsk_len))
    #endif // PHONE_CHANNEL_INTERFACE == 3
    {
        PERROR("spi_rt_interface.recv_data failed!\n");
        hook_flag = 1;
        return res;
    }
    fsk_81_recv_buf[1] = fsk_len[0];
    fsk_81_recv_buf[2] = fsk_len[1];
    len = fsk_len[1];
    len += (fsk_len[0] << 8);
    
    #if PHONE_CHANNEL_INTERFACE == 2
    if ((res = spi_rt_interface.recv_data(UART1, fsk_81_recv_buf + 3, len + 1)) != (len + 1))
    #elif PHONE_CHANNEL_INTERFACE == 3
    tv.tv_sec = 5;
    if ((res = communication_serial.recv_data(fsk_81_recv_buf + 3, len + 1, &tv)) != (len + 1))
    #endif // PHONE_CHANNEL_INTERFACE == 3
    {
        PERROR("spi_rt_interface.recv_data failed!\n");
        hook_flag = 1;
        return res;
    }
    PRINT_BUF_BY_HEX(fsk_81_recv_buf, NULL, len + 4, __FILE__, __FUNCTION__, __LINE__);
    
    // 资源释放
    sync_data_count = 0;
    memset(fsk_len, 0, sizeof(fsk_len));
    memset(fsk_81_recv_buf, 0, sizeof(fsk_81_recv_buf));
    len = 0;
    tmp = 0;
    
    // 发送87
    #if PHONE_CHANNEL_INTERFACE == 2
    if ((res = spi_rt_interface.send_data(UART1, fsk_87_send_buf, sizeof(fsk_87_send_buf)))< 0)
    #elif PHONE_CHANNEL_INTERFACE == 3
    tv.tv_sec = 5;
    if ((res = communication_serial.send_data(fsk_87_send_buf, sizeof(fsk_87_send_buf), &tv))< 0)
    #endif // PHONE_CHANNEL_INTERFACE == 3
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        hook_flag = 1;
    	return res; 
    }
    sleep(2);
    // 挂机
    if ((res = communication_serial.cmd_on_hook()) < 0)
    {
        PERROR("cmd_on_hook failed!\n");
        hook_flag = 1;
        return res;
    }
    return res;
}

/**
 * 命令字 r 启动一个进程
 */
int cmd_r()
{
    int res = 0;
    char * const app_argv[] = {"top", "-b", "-n", "1", NULL};
    if ((res = common_tools.start_up_application("/usr/bin/top", app_argv, 0)) < 0)
    {
        PERROR("start_up_application failed!\n");
        return res;
    }
    return res;
}

/**
 * 命令字 s 发送(SSID3计时)命令
 */
int cmd_s()
{
    int res = 0;
    int i = 0;
    int client_fd = 0;
    struct timeval timeout;
    memset(&timeout, 0, sizeof(struct timeval));
    unsigned char recv_buf[6] = {0};
    unsigned char send_buf[6] = {0x5A, 0xA5, 0x01, 0x00, 0x00, 0x00};
    
    #if 0
    // 创建客户端连接
    if ((client_fd = communication_network.make_local_socket_client_link(TERMINAL_LOCAL_SOCKET_NAME)) < 0)
    {
        PERROR("make_client_link failed!\n");
        res = client_fd;
        client_fd = 0;
        return res;
    }
    #else
    // 创建客户端连接
    PRINT("LOCAL_IP = %s, TERMINAL_LOCAL_SERVER_PORT = %s\n", LOCAL_IP, TERMINAL_LOCAL_SERVER_PORT);
    if ((client_fd = communication_network.make_client_link(LOCAL_IP, TERMINAL_LOCAL_SERVER_PORT)) < 0)
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
/**
 * 命令字 t 获取本地存储的设备令牌并返回
 */
int cmd_t()
{
    int res = 0;
    char device_token[TOKENLEN] = {0};
    
    if ((res = terminal_authentication.return_device_token(device_token)) < 0)
    {
        PERROR("return_device_token failed!\n");
        return res;
    }
    PRINT_BUF_BY_HEX(device_token, NULL, sizeof(device_token), __FILE__, __FUNCTION__, __LINE__);
    return res;
}

/**
 * 命令字 u 获取本地存储的位置令牌并返回
 */
int cmd_u()
{
    int res = 0;
    char position_token[TOKENLEN] = {0};
    
    if ((res = terminal_authentication.return_position_token(position_token)) < 0)
    {
        PERROR("return_position_token failed!\n");
        return res;
    }
    return res;
}

/**
 * 命令字 v 从平台重获取设备令牌并返回
 */
int cmd_v()
{
    int res = 0;
    char device_token[TOKENLEN] = {0};
    
    if ((res = terminal_authentication.rebuild_device_token(device_token)) < 0)
    {
        PERROR("rebuild_device_token failed!\n");
        return res;
    }
    return res;
}

/**
 * 命令字 w 从平台重获取位置令牌并返回
 */
int cmd_w()
{
    int res = 0;
    char position_token[TOKENLEN] = {0};
    if ((res = terminal_authentication.rebuild_position_token(position_token)) < 0)
    {
        PERROR("rebuild_position_token failed!\n");
        return res;
    }
    return res;
}
#endif // CTSI_SECURITY_SCHEME == 2


#if 1

/**
 * 命令字 x 检测WAN口
 */
int cmd_x()
{
    int res = 0;
    if ((res = network_config.get_wan_state()) < 0)
    {
        PERROR("get_wan_state failed!\n");
        return res;
    }
    return res;
}

/**
 * 命令字 y 连接服务器
 */
int cmd_y(char *ip, char *port)
{
    int res = 0;
    PRINT("ip = %s port = %s\n", ip, port);
    if ((res = communication_network.make_client_link(ip, port)) < 0)
    {
        PERROR("make_client_link failed!\n");
        return res;
    }
    PRINT("link success!\n");
    close(res);
    return res;
}
#endif

/**
 * 命令字 z USB通路测试
 */
int cmd_z()
{
    int res = 0;
    int fd = 0;
    int client_fd = 0;
    fd_set fdset;
    unsigned long num = 0;
    unsigned long num_tmp = 0;
    struct timeval tv = {5, 0};
    if ((res = communication_network.make_server_link(USB_SOCKET_PORT)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "make_server_link failed", res); 
        return res;
    }
    fd = res;
    while (1)
    {
        FD_ZERO(&fdset);
        FD_SET(fd, &fdset);       
        tv.tv_sec = 5;
        switch(select(fd + 1, &fdset, NULL, NULL, &tv))
        {
            case -1:             
            case 0:
            {
                PRINT("waiting to receive data...\n");
                continue;
            }
            default:
            {
                if (FD_ISSET(fd, &fdset) > 0)
                {
                    if ((res = communication_network.accept_client_connection(fd)) < 0)
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "accept_client_connection failed", res); 
                        continue;
                    }
                    client_fd = res;
                    PRINT("client_fd = %d\n", client_fd);
                    while (1)
                    {
                        if ((res = common_tools.recv_data(client_fd, (char *)&num_tmp, NULL, sizeof(num_tmp), &tv)) < 0)
                        {
                            PRINT("recv_data failed!\n");
                            break;
                        }
                        /*
                        if (num + 1 != num_tmp)
                        {
                            PRINT("data err!\n");
                            res = DATA_ERR;
                            break;
                        }
                        */
                        PRINT("num = %08X %d\n", num, num);
                        PRINT("num_tmp = %08X %d\n", num_tmp, num_tmp);
                        num = num_tmp;
                        num_tmp++;
                        if ((res = common_tools.send_data(client_fd, (char *)&num_tmp, NULL, sizeof(num_tmp), &tv)) < 0)
                        {
                            PRINT("send_data failed!\n");
                            break; 
                        }
                    }
                    if (res < 0)
                    {
                        break;
                    }
                }
            }
        }
        /*
        if (fd != 0)
        {
            close(fd);
            fd = 0;
        }
        */
        if (client_fd != 0)
        {
            close(client_fd);
            client_fd = 0;
        }
        /*
        if (res < 0)
        {
            break;
        }
        */
    }

}

/**
 * 命令字 B 测试PIC单片机运行
 */
int cmd_B()
{
    int res = 0;
    int fd = 0;
    struct timeval tv = {5, 0}; 
    char buf[7] = {0xA5, 0x5A, 0xE1, 0x02, 0xE5, 0x00, 0xE5};
    
    if ((res = open("/dev/uartpassage", O_RDWR, 0644)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "open failed!", OPEN_ERR);
        return OPEN_ERR;
    }
    fd = res;
    if ((res = common_tools.send_data(fd, buf, NULL, sizeof(buf), &tv)) < 0)
    {
        PRINT("send_data failed!\n");
        close(fd);
        return res;
    }
    
    memset(buf, 0, sizeof(buf));
    if ((res = common_tools.recv_data(fd, buf, NULL, sizeof(buf), &tv)) < 0)
    {
        PRINT("send_data failed!\n");
        close(fd);
        return res;
    }
    close(fd);
    return res;
}

/**
 * 命令字 C 切换控制电话和CACM通路的继电器
 */
int cmd_C(int mode)
{
    int res = 0;
    if ((res = communication_serial.relay_change(mode)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "relay_change failed!", res);
        return res;
    }
    return res;
}

/**
 * 命令字 L 关闭和打开回环
 */
int cmd_L(unsigned char mode)
{
    int res = 0;
    if ((res = communication_serial.loop_manage(mode)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "loop_manage failed!", res);
        return res;
    }
    return res;
}

/**
 * 命令字 R 终端认证和获取sip服务器账号等信息
 */
int cmd_R(char *pad_sn, char *pad_mac)
{
    int res = 0;
    if ((res = terminal_register.user_register(pad_sn, pad_mac)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "user_register failed!", res);
        return res;
    }
    return res;
}

/**
 * 命令字 print
 */
int cmd_print()
{
    PRINT("old_cmd_count=%d\n", old_cmd_count);
    PRINT("old_cmd_buf=%s\n", old_cmd_buf);    
    PRINT("delay_us=%d\n", delay_us);
    PRINT("loop_count=%d\n", loop_count);
    return 0;
}

/**
 * 命令字 set_delay
 */
int cmd_set_delay(unsigned int us)
{
    delay_us = us;
    return 0;
}

/**
 * 命令字 set_loop
 */
int cmd_set_loop_count(unsigned int count)
{
    loop_count = count;
    return 0;
}

/**
 * 命令字 help
 */
int cmd_help()
{
    PRINT("cmd option test_num delay(usleep)\n");
    PRINT("a 终端认证\n");
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
    PRINT("z USB通路测试\n");
    
    PRINT("B 测试PIC单片机运行\n");
    PRINT("C 切换控制电话和CACM通路的继电器\n");
    PRINT("L 关闭和打开回环\n");
    PRINT("R 终端认证和获取sip服务器账号\n");
    return 0;
}

/**
 * 命令字分析
 */
int analyse_cmd(unsigned short cmd_count, char *cmd_buf)
{
    int res = 0;
    int i = 0;
    char *tmp = NULL;
    char *index = cmd_buf;
    unsigned char word = 0;
    unsigned char word_1 = 0;
    
    char pad_sn[SN_LEN + 1] = {0};
    char pad_mac[13] = {0};
    char phone[13] = {0};
    char ip[16] = {0};
    char port[8] = {0};
    unsigned char mode = 0;
    
    if (cmd_count == 0)
    {
        PERROR("para error!\n");
        return DATA_ERR;
    }
    /*************************************************************************/
    PRINT("before analyse cmd!\n");
    for (i = 0; i < cmd_count; i++)
    {
        if ((tmp = strstr(index, " ")) == NULL)
        {
            PERROR("para error\n");
            return DATA_ERR;
        }
        *tmp = '\0';
        
        
        if (strcmp(index, CMD_print) == 0)
        {
            word = 1;
        }
        else if (strcmp(index, CMD_set) == 0)
        {
            word = 2;
        }
        else if (strcmp(index, CMD_help) == 0)
        {
            word = 3;
        }
        else if ((strcmp(index, CMD_quit) == 0) || (strcmp(index, CMD_exit) == 0))
        {
            PRINT("Goodbye!\n");
            exit(0); // 退出测试
        }
        else if (strcmp(index, CMD_a) == 0)
        {
            word = 'a';
        }
        else if (strcmp(index, CMD_b) == 0)
        {
            word = 'b';
        }
        else if (strcmp(index, CMD_c) == 0)
        {
            word = 'c';
        }
        else if (strcmp(index, CMD_d) == 0)
        {
            word = 'd';
        }
        else if (strcmp(index, CMD_e) == 0)
        {
            word = 'e';
        }
        else if (strcmp(index, CMD_f) == 0)
        {
            word = 'f';
        }
        else if (strcmp(index, CMD_g) == 0)
        {
            word = 'g';
        }
        else if (strcmp(index, CMD_h) == 0)
        {
            word = 'h';
        }
        else if (strcmp(index, CMD_i) == 0)
        {
            word = 'i';
        }
        else if (strcmp(index, CMD_j) == 0)
        {
            word = 'j';
        }
        else if (strcmp(index, CMD_k) == 0)
        {
            word = 'k';
        }
        else if (strcmp(index, CMD_l) == 0)
        {
            word = 'l';
        }
        else if (strcmp(index, CMD_m) == 0)
        {
            word = 'm';
        }
        #if PHONE_CHANNEL_INTERFACE == 2
        else if (strcmp(index, CMD_n) == 0)
        {
            word = 'n';
        }
        else if (strcmp(index, CMD_o) == 0)
        {
            word = 'o';
        }
        #endif // PHONE_CHANNEL_INTERFACE == 2
        else if (strcmp(index, CMD_p) == 0)
        {
            word = 'p';
        }
        else if (strcmp(index, CMD_q) == 0)
        {
            word = 'q';
        }
        else if (strcmp(index, CMD_r) == 0)
        {
            word = 'r';
        }
        else if (strcmp(index, CMD_s) == 0)
        {
            word = 's';
        }
        #if CTSI_SECURITY_SCHEME == 2
        else if (strcmp(index, CMD_t) == 0)
        {
            word = 't';
        }
        else if (strcmp(index, CMD_u) == 0)
        {
            word = 'u';
        }
        else if (strcmp(index, CMD_v) == 0)
        {
            word = 'v';
        }
        else if (strcmp(index, CMD_w) == 0)
        {
            word = 'w';
        }
        #endif // CTSI_SECURITY_SCHEME == 2
        else if (strcmp(index, CMD_x) == 0)
        {
            word = 'x';
        }
        else if (strcmp(index, CMD_y) == 0)
        {
            word = 'y';
        }
        else if (strcmp(index, CMD_z) == 0)
        {
            word = 'z';
        }
        else if (strcmp(index, CMD_B) == 0)
        {
            word = 'B';
        }
        else if (strcmp(index, CMD_C) == 0)
        {
            word = 'C';
        }
        else if (strcmp(index, CMD_L) == 0)
        {
            word = 'L';
        }
        else if (strcmp(index, CMD_R) == 0)
        {
            word = 'R';
        }
        else if (word == 2) // 当命令字大于一时，判断第二个参数
        {
            PRINT("index = %s\n", index);
            if (strcmp(index, "delay_us") == 0)
            {
                word_1 = 1;
            }
            else if (strcmp(index, "loop_count") == 0)
            {
                word_1 = 2;
            }
            else
            {
                if (word_1 == 1) // 设置延时
                {
                    PRINT("index = %s %d\n", index, atoi(index));
                    cmd_set_delay(atoi(index));
                    return 0;
                }
                else if (word_1 == 2) // 设置循环次数
                {
                    PRINT("index = %s %d\n", index, atoi(index));
                    cmd_set_loop_count(atoi(index));
                    return 0;
                }
                else
                {
                    PRINT("Option does not match!\n");
                    return MISMATCH_ERR;
                }
            }
        }
        else if (word == 'y')
        {
            if (word_1 == 0)
            {
                snprintf(ip, sizeof(ip), "%s", index);
                word_1 = 1;
            }
            else
            {
                snprintf(port, sizeof(port), "%s", index);
            }
        }
        else if ((word == 'b') || (word == 'R'))
        {
            if (word_1 == 0)
            {
                snprintf(pad_sn, sizeof(pad_sn), "%s", index);
                word_1 = 1;
            }
            else
            {
                snprintf(pad_mac, sizeof(pad_mac), "%s", index);
            }
        }
        else if ((word == 'd') || (word == 'e') || (word == 'f'))
        {
            snprintf(phone, sizeof(phone), "%s", index);
        }
        else if (word == 'C')
        {
            mode = (unsigned char)atoi(index);
            PRINT("mode = %d, index = %s\n", mode, index);
        }
        else if (word == 'L')
        {
            mode = (unsigned char)atoi(index);
            PRINT("mode = %d, index = %s\n", mode, index);
        }
        else
        {
            PRINT("Option does not match!\n");
            return MISMATCH_ERR;
        }
        index = tmp + 1;
    }
    
    /*************************************************************************/
    char log_bug[4096] = {0};
    unsigned int count = 0;
    unsigned int success_count = 0;
    unsigned int failed_count = 0;
    
    struct timeval tpstart, tpend;
    memset(&tpstart, 0, sizeof(struct timeval));
    memset(&tpend, 0, sizeof(struct timeval));
    
    strcpy(log_bug, "failed log:");
    
    PRINT("before exec cmd!\n");
    
    // 执行命令
    for (i = 0; i < loop_count; i++)
    {
        gettimeofday(&tpstart, NULL); // 得到当前时间
        switch (word)
        {
            case 1: // 环境变量打印
            {
                res = cmd_print();
                //break;
                return 0;
            }
            case 3: // 打印帮助
            {
                res = cmd_help();
                //break;
                return 0;
            }
            case 'a': 
            {
                res = cmd_a();
                break;
            }
            case 'b':
            {
                res = cmd_b(pad_sn, pad_mac);
                break;
            }
            case 'c':
            {
                res = cmd_c();
                break;
            }
            case 'd':
            {
                res = cmd_d(phone);
                break;
            }
            case 'e':
            {
                res = cmd_e(phone);
                break;
            }
            case 'f':
            {
                res = cmd_f(phone);
                break;
            }
            case 'g':
            {
                res = cmd_g();
                break;
            }
            case 'h':
            {
                res = cmd_h();
                break;
            }
            case 'i':
            {
                res = cmd_i();
                break;
            }
            case 'j':
            {
                res = cmd_j();
                break;
            }
            case 'k':
            {
                res = cmd_k();
                break;
            }
            case 'l':
            {
                res = cmd_l();
                break;
            }
            case 'm':
            {
                res = cmd_m();
                break;
            }
            #if PHONE_CHANNEL_INTERFACE == 2
            case 'n':
            {
                res = cmd_n();
                break;
            }
            case 'o':
            {
                res = cmd_o();
                break;
            }
            #endif // #if PHONE_CHANNEL_INTERFACE == 2
            case 'p':
            {
                res = cmd_p();
                break;
            }
            case 'q':
            {
                res = cmd_q();
                break;
            }
            case 'r':
            {
                res = cmd_r();
                break;
            }
            case 's':
            {
                res = cmd_s();
                break;
            }
            #if CTSI_SECURITY_SCHEME == 2
            case 't':
            {
                res = cmd_t();
                break;
            }
            case 'u':
            {
                res = cmd_u();
                break;
            }
            case 'v':
            {
                res = cmd_v();
                break;
            }
            case 'w':
            {
                res = cmd_w();
                break;
            }
            #endif // CTSI_SECURITY_SCHEME == 2
            case 'x':
            {
                res = cmd_x();
                break;
            }
            case 'y':
            {
                res = cmd_y(ip, port);
                break;
            }
            case 'z':
            {
                res = cmd_z();
                break;
            }
            case 'B':
            {
                res = cmd_B();
                break;
            }
            case 'C':
            {
                res = cmd_C(mode);
                break;
            }
            case 'L':
            {
                res = cmd_L(mode);
                break;
            }
            case 'R':
            {
                res = cmd_R(pad_sn, pad_mac);
                break;
            }
            default:
            {
                PRINT("Option does not match!\n");
                res = MISMATCH_ERR;
            }
        }
        
        count++;
        if (res < 0)
        {
            if (hook_flag == 1)
            {
                if ((res = communication_serial.cmd_on_hook()) < 0)
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
            
            #if 1 // 错误时，直接退出 
            gettimeofday(&tpend, NULL);      
            PRINT("****************************************\n");
            PRINT("use time：%3.6lf\n", common_tools.get_use_time(tpstart, tpend));
            PRINT("count = %d, failed_count = %d, success_count = %d\n", count, failed_count, success_count);
            PRINT("success_count / count = %%%3.6f\n", 100.0 * ((float)success_count / (float)count));
            PRINT("****************************************\n");
            return res;
            #endif
        }
        else if (res >= 0)
        {
            success_count++;
        }
        
        gettimeofday(&tpend, NULL);      
        PRINT("****************************************\n");
        PRINT("use time：%3.6lf\n", common_tools.get_use_time(tpstart, tpend));
        PRINT("count = %d, failed_count = %d, success_count = %d\n", count, failed_count, success_count);
        PRINT("success_count / count = %%%3.6f\n", 100.0 * ((float)success_count / (float)count));
        PRINT("****************************************\n");
        
        usleep(delay_us); // 延时
    }
    PRINT("\n%s\n", log_bug);
    return res;
}

/**
 * 读取命令参数
 */
int read_cmd(unsigned short *cmd_count, char *cmd_buf)
{
    int i = 0;
    char tmp = 0;
    
    for (i = 0; i < 1024; i++)
    {
        scanf("%c", &tmp);
        cmd_buf[i] = tmp;
        
        if (tmp == '\n')
        {
            if (i == 0) // 执行上条命令
            {
                if (strlen(old_cmd_buf) != 0)
                {
                    (*cmd_count) = old_cmd_count;
                    memcpy(cmd_buf, old_cmd_buf, strlen(old_cmd_buf));
                }
                else
                {
                    return DATA_ERR;
                }
            }
            else 
            {
                cmd_buf[i] = ' ';
                (*cmd_count)++;
            }
            // 其他情况说明输入完毕
            break;
        }
        else if ((tmp == ' ') || (tmp == '\t')) // 是否为空白符
        {
            (*cmd_count)++;
        }
    }
    // PRINT("*cmd_count = %d, cmd_buf = %s\n", *cmd_count, cmd_buf);
    return 0;
}

int main(int argc, char **argv)
{
    strcpy(common_tools.argv0, argv[0]);
    
    int res = 0;
    unsigned short cmd_count = 0;
    char cmd_buf[1024] = {0};
    
    if ((res = common_tools.get_config()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_config failed!", res);
        return res;
    }
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "The configuration file is read successfully!", 0);
    
    while (1) 
    {
        fflush(stdin);
        fflush(stdout);
        printf("test> ");
        if (read_cmd(&cmd_count, cmd_buf) < 0)
        {
            // PERROR("read_cmd failed!\n");
            continue;
        }
        else 
        {
            PRINT("after read_cmd!\n");
            PRINT("cmd_buf = %s cmd_count = %d\n", cmd_buf, cmd_count);
            
            old_cmd_count = cmd_count;
            memset(old_cmd_buf, 0, sizeof(old_cmd_buf));
            memcpy(old_cmd_buf, cmd_buf, strlen(cmd_buf));
            
            PRINT("old_cmd_buf = %s old_cmd_count = %d\n", old_cmd_buf, old_cmd_count);            
            if (analyse_cmd(cmd_count, cmd_buf) < 0)
            {
                PERROR("analyse_cmd failed!\n");
                // old_cmd_count = 0;
                // memset(old_cmd_buf, 0, sizeof(old_cmd_buf));
            }
            PRINT("after analyse_cmd!\n");
        }
        printf("\n");
         
        // 资源释放
        memset(cmd_buf, 0, sizeof(cmd_buf));
        cmd_count = 0;
    }
    return 0;
}
