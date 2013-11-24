#include "network_config.h"


static int serial_pad_fd = 0, serial_5350_fd = 0, server_pad_fd = 0, server_base_fd = 0;
static char network_flag = 0; // 否已经注册成功
static char g_cmd = 0xFF;
static char pad_cmd = 0xFB;

#if BOARDTYPE == 5350 || BOARDTYPE == 9344
// 初始化cmd_list
static void init_cmd_list();
#endif

/**
 * 初始化运行环境
 */
static int init_env();

/**
 * 发送数据到PAD
 */
static int send_msg_to_pad(int fd, char cmd, char *data);

/**
 * 接收pad发送的报文包
 */
static int recv_msg_from_pad(int fd, struct s_pad_and_6410_msg *a_pad_and_6410_msg);

#if BOARDTYPE == 6410 
/**
 * 发送命令到5350
 */
static int send_msg_to_5350(int fd, char *cmd, unsigned short len);

/**
 * 接收pad发送的报文包
 */
static int recv_msg_from_5350(int fd, struct s_6410_and_5350_msg *a_6410_and_5350_msg);

/**
 * 获取PPPOE状态
 */
static int get_pppoe_state(int fd);

/**
 * 恢复路由器设置之前的状态
 */
static int recovery_route(int fd, char *buf, unsigned short buf_len);

/**
 * 设置路由器
 */
static int route_config(int fd, int index);
#elif BOARDTYPE == 5350 // 5350

/**
 * 获取路由wan口状态
 */
static int get_wan_state();

/**
 * 获取PPPOE状态
 */
static int get_pppoe_state();

/**
 * 使设置生效
 */
static int config_route_take_effect();

/**
 * 设置路由器
 */
static int route_config(int index);
static int route_config2(int index);

#elif BOARDTYPE == 9344 // 9344
/**
 * 获取路由wan口状态
 */
static int get_wan_state();

/**
 * 使设置生效
 */
static int config_route_take_effect();

/**
 * 设置路由器
 */
static int route_config(int index);
#endif

/**
 * 终端初始化
 */
static int network_settings(int fd, int cmd_count, char cmd_word);

// dhcpEnabled

#if BOARDTYPE == 5350
// 命令结构体数组
struct s_cmd cmd_list[] = 
{
    //word    bit   key                    set_cmd                        set_value set_cmd_and_value  get_cmd
    {0x14, 0x00, "wan_ipaddr",          "nvram_set 2860 wan_ipaddr ",           "", "", "nvram_get 2860 wan_ipaddr"},
    {0x15, 0x00, "wan_netmask",         "nvram_set 2860 wan_netmask ",          "", "", "nvram_get 2860 wan_netmask"},
    {0x16, 0x00, "wan_gateway",         "nvram_set 2860 wan_gateway ",          "", "", "nvram_get 2860 wan_gateway"},
    {0x17, 0x00, "wan_primary_dns",     "nvram_set 2860 wan_primary_dns ",      "", "", "nvram_get 2860 wan_primary_dns"},
    {0x18, 0x00, "wan_secondary_dns",   "nvram_set 2860 wan_secondary_dns ",    "", "", "nvram_get 2860 wan_secondary_dns"},
    {0x19, 0x00, "wan_dhcp_hn",         "nvram_set 2860 wan_dhcp_hn ",          "", "", "nvram_get 2860 wan_dhcp_hn"},
    {0x1A, 0x00, "wan_pppoe_user",      "nvram_set 2860 wan_pppoe_user ",       "", "", "nvram_get 2860 wan_pppoe_user"},
    {0x1B, 0x00, "wan_pppoe_pass",      "nvram_set 2860 wan_pppoe_pass ",       "", "", "nvram_get 2860 wan_pppoe_pass"},
    {0x1C, 0x00, "macCloneEnabled",     "nvram_set 2860 macCloneEnabled ",      "", "", "nvram_get 2860 macCloneEnabled"},
    {0x1D, 0x00, "macCloneMac",         "nvram_set 2860 macCloneMac ",          "", "", "nvram_get 2860 macCloneMac"},

    // 初始化设备
    {0x30, 0x00, "SSID2",               "nvram_set 2860 SSID2 ",                "", "", "nvram_get 2860 SSID2"},
    {0x31, 0x00, "WPAPSK2",             "nvram_set 2860 WPAPSK2 ",              "", "", "nvram_get 2860 WPAPSK2"},
    {0x32, 0x00, "dhcpStatic1",         "nvram_set 2860 dhcpStatic1 ",          "", "", "nvram_get 2860 dhcpStatic1"},
    #if BOARDTYPE == 6410 
    {0x33, 0x00, "dhcpStatic2",         "nvram_set 2860 dhcpStatic2 ",          "", "", "nvram_get 2860 dhcpStatic2"},
    #endif
    {0x34, 0x00, "HideSSID",            "nvram_set 2860 HideSSID ",             "", "", "nvram_get 2860 HideSSID"},
    {0x35, 0x00, "EncrypType",          "nvram_set 2860 EncrypType ",           "", "", "nvram_get 2860 EncrypType"},
    {0x36, 0x00, "RekeyInterval",       "nvram_set 2860 RekeyInterval ",        "", "", "nvram_get 2860 RekeyInterval"},
    {0x37, 0x00, "BssidNum",            "nvram_set 2860 BssidNum ",             "", "", "nvram_get 2860 BssidNum"},
    {0x38, 0x00, "RADIUS_Key1",         "nvram_set 2860 RADIUS_Key1 ",          "", "", "nvram_get 2860 RADIUS_Key1"},
    {0x39, 0x00, "RekeyMethod",         "nvram_set 2860 RekeyMethod ",          "", "", "nvram_get 2860 RekeyMethod"},
    {0x3A, 0x00, "WscConfigured",       "nvram_set 2860 WscConfigured ",        "", "", "nvram_get 2860 WscConfigured"},
    {0x3B, 0x00, "FixedTxMode",         "nvram_set 2860 FixedTxMode ",          "", "", "nvram_get 2860 FixedTxMode"},
    {0x3C, 0x00, "lan2_ipaddr",         "nvram_set 2860 lan2_ipaddr ",          "", "", "nvram_get 2860 lan2_ipaddr"},
    {0x3D, 0x00, "lan2_netmask",        "nvram_set 2860 lan2_netmask ",         "", "", "nvram_get 2860 lan2_netmask"},
    {0x3E, 0x00, "upnpEnabled",         "nvram_set 2860 upnpEnabled ",          "", "", "nvram_get 2860 upnpEnabled"},
    {0x3F, 0x00, "radvdEnabled",        "nvram_set 2860 radvdEnabled ",         "", "", "nvram_get 2860 radvdEnabled"},
    {0x40, 0x00, "pppoeREnabled",       "nvram_set 2860 pppoeREnabled ",        "", "", "nvram_get 2860 pppoeREnabled"},
    {0x41, 0x00, "dnsPEnabled",         "nvram_set 2860 dnsPEnabled ",          "", "", "nvram_get 2860 dnsPEnabled"},
    {0x42, 0x00, "Lan2Enabled",         "nvram_set 2860 Lan2Enabled ",          "", "", "nvram_get 2860 Lan2Enabled"},
    {0x43, 0x00, "SSID1",               "nvram_set 2860 SSID1 ",                "", "", "nvram_get 2860 SSID1"},
    {0x44, 0x00, "WPAPSK1",             "nvram_set 2860 WPAPSK1 ",              "", "", "nvram_get 2860 WPAPSK1"},
    {0x45, 0x00, "AuthMode",            "nvram_set 2860 AuthMode ",             "", "", "nvram_get 2860 AuthMode"},
    
    {0x46, 0x00, "SSID3",               "nvram_set 2860 SSID3 ",                "", "", "nvram_get 2860 SSID3"},
    {0x47, 0x00, "WPAPSK3",             "nvram_set 2860 WPAPSK3 ",              "", "", "nvram_get 2860 WPAPSK3"},
    // STATIC DHCP PPPOE
    {0x4A, 0x00, "wanConnectionMode",   "nvram_set 2860 wanConnectionMode ",    "", "", "nvram_get 2860 wanConnectionMode"},  
    {0x4B, 0x00, "wan_pppoe_opmode",    "nvram_set 2860 wan_pppoe_opmode ",     "", "", "nvram_get 2860 wan_pppoe_opmode"},    
    {0x4C, 0x00, "wan_pppoe_optime",    "nvram_set 2860 wan_pppoe_optime ",     "", "", "nvram_get 2860 wan_pppoe_optime"},
}; 

#elif BOARDTYPE == 9344
struct s_cmd cmd_list[] = 
{
    //word    bit   key                    set_cmd                        set_value set_cmd_and_value
    
    // STATIC DHCP PPPOE 模式
    {0x14, 0x00, "WAN_MODE",   "cfg -a WAN_MODE=",    "", ""},
    
    // 静态
    {0x15, 0x00, "WAN_IPADDR",          "cfg -a WAN_IPADDR=",        "", ""},
    {0x16, 0x00, "WAN_NETMASK",         "cfg -a WAN_NETMASK=",       "", ""},
    {0x17, 0x00, "IPGW",                "cfg -a IPGW=",              "", ""},
    {0x18, 0x00, "PRIDNS",              "cfg -a PRIDNS=",            "", ""},
    {0x19, 0x00, "SECDNS",              "cfg -a SECDNS=",            "", ""},
    
    // PPPOE
    {0x1A, 0x00, "PPPOE_USER",     "cfg -a PPPOE_USER=",       "", ""},
    {0x1B, 0x00, "PPPOE_PWD",      "cfg -a PPPOE_PWD=",        "", ""},
    
    // 初始化设备
    {0x1C, 0x00, "AP_STARTMODE",    "cfg -a 2860 AP_STARTMODE=",     "", ""},
    // SSID
    {0x1D, 0x00, "AP_SSID",         "cfg -a AP_SSID=",         "", ""},
    {0x1E, 0x00, "AP_SSID_2",       "cfg -a AP_SSID_2=",       "", ""},
    {0x1F, 0x00, "AP_SSID_3",       "cfg -a AP_SSID_3=",       "", ""},
    
    // 密码
    {0x30, 0x00, "PSK_KEY",         "cfg -a PSK_KEY=",         "", ""},
    {0x31, 0x00, "PSK_KEY_2",       "cfg -a AP_SSID_2=",       "", ""},
    {0x32, 0x00, "PSK_KEY_3",       "cfg -a PSK_KEY_3=",       "", ""},
    

    // 2.4G
    {0x33, 0x00, "AP_RADIO_ID",         "cfg -a AP_RADIO_ID=",            "", ""},
    {0x34, 0x00, "AP_RADIO_ID_2",       "cfg -a AP_RADIO_ID_2=",          "", ""},
    {0x35, 0x00, "AP_RADIO_ID_3",       "cfg -a AP_RADIO_ID_3=",          "", ""},
    
    // 模式
    {0x36, 0x00, "AP_MODE",         "cfg -a AP_MODE=",            "", ""},
    {0x37, 0x00, "AP_MODE_2",       "cfg -a AP_MODE_2=",          "", ""},
    {0x38, 0x00, "AP_MODE_3",       "cfg -a AP_MODE_3=",          "", ""},
    
    // 1表示隐藏
    {0x39, 0x00, "AP_HIDESSID",         "cfg -a AP_HIDESSID=",            "", ""},
    {0x3A, 0x00, "AP_HIDESSID_2",       "cfg -a AP_HIDESSID_2=",          "", ""},
    {0x3B, 0x00, "AP_HIDESSID_3",       "cfg -a AP_HIDESSID_3=",          "", ""},
    
    // 加密方式
    {0x3C, 0x00, "AP_SECMODE",         "cfg -a AP_SECMODE=",            "", ""},
    {0x3D, 0x00, "AP_SECMODE_2",       "cfg -a AP_SECMODE_2=",          "", ""},
    {0x3E, 0x00, "AP_SECMODE_3",       "cfg -a AP_SECMODE_3=",          "", ""},
    {0x3F, 0x00, "AP_WPA",         "cfg -a AP_WPA=",            "", ""},
    {0x40, 0x00, "AP_WPA_2",       "cfg -a AP_WPA_2=",          "", ""},
    {0x41, 0x00, "AP_WPA_3",       "cfg -a AP_WPA_3=",          "", ""},
    {0x42, 0x00, "AP_SECFILE",         "cfg -a AP_SECFILE=",            "", ""},
    {0x43, 0x00, "AP_SECFILE_2",       "cfg -a AP_SECFILE_2=",          "", ""},
    {0x44, 0x00, "AP_SECFILE_3",       "cfg -a AP_SECFILE_3=",          "", ""},
    
    // 静态绑定
    {0x45, 0x00, "MAC_BIND_1",         "cfg -a MAC_BIND_1=",         "", ""},
    {0x46, 0x00, "IP_BIND_1",         "cfg -a IP_BIND_1=",          "", ""},
};
#endif

struct class_network_config network_config = 
{
    .serial_pad_fd = &serial_pad_fd,
    .serial_5350_fd = &serial_5350_fd,
    .server_pad_fd = &server_pad_fd,
    .server_base_fd = &server_base_fd,
    .usb_pad_fd = 0,
    .network_flag = &network_flag,
    .cmd = &g_cmd,
    .pad_cmd = &pad_cmd,
    .pthread_flag = 0,
    .pthread_recv_flag = 0,
    
    #if BOARDTYPE == 5350 || BOARDTYPE == 9344
    .init_cmd_list = init_cmd_list,
    #endif
    .init_env = init_env, 
    .send_msg_to_pad = send_msg_to_pad,
    .recv_msg_from_pad = recv_msg_from_pad,
    .network_settings = network_settings,
    #if BOARDTYPE == 6410 
    .send_msg_to_5350 = send_msg_to_5350,
    .recv_msg_from_5350 = recv_msg_from_5350,
    .get_pppoe_state = get_pppoe_state,
    .route_config = route_config,
    #elif BOARDTYPE == 5350
    .get_wan_state = get_wan_state,
    .get_pppoe_state = get_pppoe_state,
    .config_route_take_effect = config_route_take_effect,
    .route_config = route_config,
    .route_config2 = route_config2,
    #elif BOARDTYPE == 9344
    .get_wan_state = get_wan_state,
    .config_route_take_effect = config_route_take_effect,
    .route_config = route_config,
    #endif
};

#if BOARDTYPE == 5350 || BOARDTYPE == 9344
// 初始化cmd_list
void init_cmd_list()
{
    int i = 0;
    for (i = 0; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
    {
        network_config.cmd_list[i].cmd_bit = 0x00;
        memset(network_config.cmd_list[i].set_value, 0, sizeof(network_config.cmd_list[i].set_value));
        memset(network_config.cmd_list[i].set_cmd_and_value, 0, sizeof(network_config.cmd_list[i].set_cmd_and_value));
    }
}
#endif

#if BOARDTYPE == 9344
int init_usb()
{
    int res = 0;
	/*
    char buf[128] = {0};
    // 打开串口并且初始化
    if ((network_config.usb_pad_fd = open(USB_NODE, O_RDWR, 0644)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "open failed", OPEN_ERR); 
        return OPEN_ERR;
    }
    
    sprintf(buf, "open %s (pad usb) and init success", USB_NODE);
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, buf, 0);
	*/
    return 0;
}
#endif
/**
 * 初始化运行环境
 */
int init_env()
{
    PRINT_STEP("entry...\n");
    int res = 0;
    char buf[64] = {0};
    
    if ((res = common_tools.get_config()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_config failed!", res);
        return res;
    }
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "The configuration file is read successfully!", 0);
    #if BOARDTYPE == 5350
    // 打开串口并且初始化
    if ((serial_pad_fd = open(common_tools.config->serial_pad, O_RDWR, 0644)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "open failed", OPEN_ERR); 
        return OPEN_ERR;
    }
    
    if ((res = common_tools.serial_init(serial_pad_fd, common_tools.config->serial_pad_baud)) < 0)
    {
        close(serial_pad_fd);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "serial_init failed", res);         
        return res;
    }
    
    sprintf(buf, "open %s (pad serial) and init success, baud:%d", common_tools.config->serial_pad, common_tools.config->serial_pad_baud);
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, buf, 0);
    #elif BOARDTYPE == 9344
    if ((res = init_usb()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "init_usb failed", res); 
        return res;
    }
    #endif
    
    memset(buf, 0, sizeof(buf));
    
    #if BOARDTYPE == 6410
    // 打开串口并且初始化
    if ((serial_5350_fd = open(common_tools.config->serial_5350, O_RDWR, 0644)) < 0)
    {
        close(serial_pad_fd);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "open failed", OPEN_ERR); 
        return OPEN_ERR;
    }
    
    if ((res = common_tools.serial_init(serial_5350_fd, common_tools.config->serial_5350_baud)) < 0)
    {
        close(serial_pad_fd);
        close(serial_5350_fd);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "serial_init failed", res);   
        return res;
    }
    sprintf(buf, "open %s (5350 serial) and init success, baud:%d", common_tools.config->serial_5350, common_tools.config->serial_5350_baud);
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, buf, 0);
    memset(buf, 0, sizeof(buf));
    #endif
    
    // 建立服务器
    if ((server_base_fd = internetwork_communication.make_server_link(common_tools.config->pad_client_port)) < 0)
    {
        close(serial_pad_fd);
        close(serial_5350_fd);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "make_server_link failed", server_base_fd); 
        return server_base_fd;
    }
    sprintf(buf, "The server is established successfully! port = %s", common_tools.config->pad_client_port);    
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, buf, 0);
    
    #if BOARDTYPE == 5350
    memcpy(network_config.cmd_list, cmd_list, sizeof(cmd_list));
    #endif
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 发送数据到PAD
 */
int send_msg_to_pad(int fd, char cmd, char *data)
{
    PRINT_STEP("entry...\n");
    int res = 0;
    char *send_buf = NULL;
    struct timeval tv = {common_tools.config->one_byte_timeout_sec, common_tools.config->one_byte_timeout_usec};
    unsigned short buf_len = 0;
    unsigned short msg_len = 0;
    int i = 0;
    if (data == NULL)
    {
        buf_len = 2 + 2 + 1 + 1;   
    }
    else
    {
        buf_len = 2 + 2 + 1 + strlen(data) + 1;
    }
    
    if ((send_buf = (char *)malloc(buf_len)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed", MALLOC_ERR);         
        return MALLOC_ERR;
    }
    
    // 打包
    send_buf[0] = 0x5A;
    send_buf[1] = 0xA5;
    if (data == NULL)
    {
        send_buf[2] = 1;
        send_buf[3] = 0;
        send_buf[4] = cmd;
        send_buf[5] = cmd;
    }
    else
    {
        msg_len = strlen(data) + 1;
        #if ENDIAN == 0
        memcpy(send_buf + 2, &msg_len, sizeof(unsigned short));
        #else
        msg_len = DATA_ENDIAN_CHANGE_SHORT(msg_len);
        memcpy(send_buf + 2, &msg_len, sizeof(unsigned short));        
        #endif
        
        send_buf[4] = cmd;
        memcpy(send_buf + 5, data, strlen(data));
        
        send_buf[buf_len - 1] = (cmd ^ common_tools.get_checkbit(data, NULL, 0, strlen(data), XOR, 1));
    }
    
    // 发送
	PRINT("buf_len = %d\n", buf_len);
    if ((res = common_tools.send_data(fd, send_buf, NULL, buf_len, &tv)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed", res);
        free(send_buf); 
        send_buf = NULL;
        return common_tools.get_errno('P', res);
    }
    
    PRINT_BUF_BY_HEX(send_buf, NULL, buf_len, __FILE__, __FUNCTION__, __LINE__);
    free(send_buf);
    send_buf = NULL;
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 接收pad发送的报文包
 */
int recv_msg_from_pad(int fd, struct s_pad_and_6410_msg *a_pad_and_6410_msg)
{
    PRINT_STEP("entry...\n");
    int res = 0;
    char tmp = 0;
    struct timeval tv = {common_tools.config->one_byte_timeout_sec, common_tools.config->one_byte_timeout_usec};
    
    if (a_pad_and_6410_msg != NULL)
    {
        PRINT("___from pad start___\n");
        // 接收数据头
        if ((res = common_tools.recv_data_head(fd, a_pad_and_6410_msg->head, sizeof(a_pad_and_6410_msg->head), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_data_head failed", res);
            return common_tools.get_errno('P', res);
        }
        
        // 接收数据长度
        if ((res = common_tools.recv_one_byte(fd, &tmp, &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_one_byte failed", res);
            return common_tools.get_errno('P', res);;
        }
        a_pad_and_6410_msg->len = tmp;
        
        if ((res = common_tools.recv_one_byte(fd, &tmp, &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_one_byte failed", res);
            return common_tools.get_errno('P', res);;
        }
        a_pad_and_6410_msg->len += (tmp << 8);
        // 接收命令字
        if ((res = common_tools.recv_one_byte(fd, &a_pad_and_6410_msg->cmd, &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_one_byte failed", res); 
            return common_tools.get_errno('P', res);;
        }        
        if ((a_pad_and_6410_msg->len - 1) > 0)          
        {
            // 动态申请
            if ((a_pad_and_6410_msg->data = (char *)malloc(a_pad_and_6410_msg->len)) == NULL)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed", MALLOC_ERR);
                return MALLOC_ERR;
            }
            memset((void *)a_pad_and_6410_msg->data, 0, a_pad_and_6410_msg->len);
            
            // 接收有效数据
            if ((res = common_tools.recv_data(fd, a_pad_and_6410_msg->data, NULL, a_pad_and_6410_msg->len - 1, &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_data failed", res);
                return common_tools.get_errno('P', res);;
            }
        }
        // 接收校验位
        if ((res = common_tools.recv_one_byte(fd, &a_pad_and_6410_msg->check, &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_one_byte failed", res);
            return common_tools.get_errno('P', res);;
        }
        printf("\n"); 
        PRINT("___from pad end___\n");
        if ((a_pad_and_6410_msg->len - 1) > 0) 
        { 
            // 校验位对比       
            if ((a_pad_and_6410_msg->cmd ^ common_tools.get_checkbit(a_pad_and_6410_msg->data, NULL, 0, a_pad_and_6410_msg->len, XOR, 1)) != a_pad_and_6410_msg->check)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "check is different!", P_CHECK_DIFF_ERR);
                return P_CHECK_DIFF_ERR;         
            }
        }
        else
        {
            if (a_pad_and_6410_msg->check != a_pad_and_6410_msg->cmd)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "check is different!", P_CHECK_DIFF_ERR);
                return P_CHECK_DIFF_ERR;         
            }
        }
    }
    else
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data buf recv success!", 0);
    PRINT_STEP("exit...\n");
    return 0;
}

#if BOARDTYPE == 6410 

/**
 * 发送命令到5350
 */
int send_msg_to_5350(int fd, char *cmd, unsigned short len)
{
    PRINT_STEP("entry...\n");
    char *send_buf = NULL;
    unsigned short buf_len = 0;
    int i = 0;
    if ((cmd[0] == 0x00) || (cmd[0] == 0xFF))
    {
        buf_len = 6;
    }
    else
    {
        buf_len = 2 + 2 + len + 1;
    }
    
    if ((send_buf = (char *)malloc(buf_len)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed", MALLOC_ERR);         
        return MALLOC_ERR;
    }
    
    // 打包
    send_buf[0] = 0x5A;
    send_buf[1] = 0xA5;
    if ((cmd[0] == 0x00) || (cmd[0] == 0xFF))
    {
        memcpy(send_buf + 2, &len, sizeof(len));
        send_buf[4] = cmd[0];
        send_buf[5] = cmd[0]; 
    }
    else
    {
        memcpy(send_buf + 2, &len, sizeof(len));
        memcpy(send_buf + 4, cmd, len);
        send_buf[buf_len - 1] = common_tools.get_checkbit(cmd, NULL, 0, len, XOR, 1);
        PRINT("[%s]\n", cmd);
    }
    
    // 发送
    for (i = 0; i < buf_len; i++)
    {
        if (write(fd, &send_buf[i], sizeof(send_buf[i])) != sizeof(send_buf[i]))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "write failed", WRITE_ERR); 
            free(send_buf);
            send_buf = NULL;    
            return WRITE_ERR;
        }
        usleep(common_tools.config->one_byte_delay_usec);
    }
    PRINT_BUF_BY_HEX(send_buf, NULL, buf_len, __FILE__, __FUNCTION__, __LINE__);
    free(send_buf);
    send_buf = NULL;
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 接收pad发送的报文包
 */
int recv_msg_from_5350(int fd, struct s_6410_and_5350_msg *a_6410_and_5350_msg)
{
    PRINT_STEP("entry...\n");
    int res = 0;
    struct timeval tv = {common_tools.config->one_byte_timeout_sec, common_tools.config->one_byte_timeout_usec};
    unsigned char tmp = 0;
    if (a_6410_and_5350_msg != NULL)
    {
        PRINT("___from 5350 start___\n");
        // 接收数据头
        if ((res = common_tools.recv_data_head(fd, a_6410_and_5350_msg->head, sizeof(a_6410_and_5350_msg->head), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_data_head failed", res);         
            return res;
        }
        
        // 接收数据长度
        if ((res = common_tools.recv_one_byte(fd, &tmp, &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_one_byte failed", res); 
            return res;
        }
        a_6410_and_5350_msg->len = tmp;
        
        if ((res = common_tools.recv_one_byte(fd, &tmp, &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_one_byte failed", res);
            return res;
        }
        a_6410_and_5350_msg->len += (tmp << 8);
        
        // 动态申请
        if ((a_6410_and_5350_msg->data = (char *)malloc(a_6410_and_5350_msg->len + 1)) == NULL)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed", MALLOC_ERR);
            return MALLOC_ERR;
        }
        memset(a_6410_and_5350_msg->data, 0, a_6410_and_5350_msg->len + 1);
        
        // 接收有效数据
        if ((res = common_tools.recv_data(fd, a_6410_and_5350_msg->data, NULL, a_6410_and_5350_msg->len, &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_data failed", res);
            return res;
        }
        
        // 接收校验位
        if ((res = common_tools.recv_one_byte(fd, &a_6410_and_5350_msg->check, &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_one_byte failed");         
            return res;   
        }
        
        // 校验位对比
        if (a_6410_and_5350_msg->check != common_tools.get_checkbit(a_6410_and_5350_msg->data, NULL, 0, a_6410_and_5350_msg->len, XOR, 1))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "check is different!");
            return CHECK_DIFF_ERR;
        }
        printf("\n");       
        PRINT("___from 5350 end___\n");
    }
    else
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "mismatching!");
        return NULL_ERR;
    }
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 获取PPPOE状态
 */
int get_pppoe_state(int fd)
{
    int res = 0;
    struct s_6410_and_5350_msg _6410_and_5350_msg;
    memset(&_6410_and_5350_msg, 0, sizeof(struct s_6410_and_5350_msg));
    _6410_and_5350_msg.head[0] = 0x5A;
    _6410_and_5350_msg.head[1] = 0xA5;
    
    if ((res = send_msg_to_5350(fd, "PPPOE state?", strlen("PPPOE state?"))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_5350 failed"); 
        return res;
    }
    
    if ((res = recv_msg_from_5350(fd, &_6410_and_5350_msg)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg_from_5350 failed"); 
        if (_6410_and_5350_msg.data != NULL)
        {
            free(_6410_and_5350_msg.data);
            _6410_and_5350_msg.data = NULL;
        }
        return res;
    }
    
    if (_6410_and_5350_msg.data[0] == 0xFF)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data err"); 
        free(_6410_and_5350_msg.data);
        _6410_and_5350_msg.data = NULL;
        return DATA_ERR;
    }
    res = _6410_and_5350_msg.data[1];
    free(_6410_and_5350_msg.data);
    _6410_and_5350_msg.data = NULL;
    return res;
}

/**
 * 恢复路由器设置之前的状态
 */
int recovery_route(int fd, char *buf, unsigned short buf_len)
{
    int res = 0;
    struct s_6410_and_5350_msg _6410_and_5350_msg;
    memset(&_6410_and_5350_msg, 0, sizeof(struct s_6410_and_5350_msg));
    _6410_and_5350_msg.head[0] = 0x5A;
    _6410_and_5350_msg.head[1] = 0xA5;
    
    if (buf == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf is NULL"); 
        return NULL_ERR;
    }
    
    if ((res = send_msg_to_5350(fd, "recovery of router state!", strlen("recovery of router state!"))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_5350 failed"); 
        return res;
    }
    
    if ((res = recv_msg_from_5350(fd, &_6410_and_5350_msg)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg_from_5350 failed"); 
        if (_6410_and_5350_msg.data != NULL)
        {
            free(_6410_and_5350_msg.data);
            _6410_and_5350_msg.data = NULL;
        }
        return res;
    }
    
    if (_6410_and_5350_msg.data[0] == 0xFF)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data err"); 
        free(_6410_and_5350_msg.data);
        _6410_and_5350_msg.data = NULL;
        return DATA_ERR;
    }
    free(_6410_and_5350_msg.data);
    _6410_and_5350_msg.data = NULL;
    #if READONLY_ROOTFS
    PRINT("buf = %s\n", buf);
    if ((res = send_msg_to_5350(fd, buf, buf_len)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_5350 failed"); 
        return res;
    }
    #endif
    return 0;
}

/**
 * 保存当前路由状态 
 */
int save_current_route_state(int fd, char *route_state_buf, unsigned short *route_state_buf_len)
{
    int res = 0;
    struct s_6410_and_5350_msg _6410_and_5350_msg;
    memset(&_6410_and_5350_msg, 0, sizeof(struct s_6410_and_5350_msg));
    _6410_and_5350_msg.head[0] = 0x5A;
    _6410_and_5350_msg.head[1] = 0xA5;
    
    // 读取5350当前状态
    if ((res = send_msg_to_5350(fd, "current route state?", strlen("current route state?"))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_5350 failed");
        return res;
    }
    
    if ((res = recv_msg_from_5350(fd, &_6410_and_5350_msg)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg failed");
        if (_6410_and_5350_msg.data != NULL)
        {
            free(_6410_and_5350_msg.data);
            _6410_and_5350_msg.data = NULL;
        }
        return res;
    }
    
    if (_6410_and_5350_msg.data[0] == 0xFF)
    {
        free(_6410_and_5350_msg.data);
        _6410_and_5350_msg.data = NULL;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data err");
        return DATA_ERR;
    }
    
    #if READONLY_ROOTFS // 只读文件系统时
    char tmp = 0x00;
    if ((route_state_buf = (char *)malloc(_6410_and_5350_msg.len + 1)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed");
        free(_6410_and_5350_msg.data);
        _6410_and_5350_msg.data = NULL;
        return MALLOC_ERR;
    }
    
    memset(route_state_buf, 0, _6410_and_5350_msg.len + 1);
    memcpy(route_state_buf, _6410_and_5350_msg.data, _6410_and_5350_msg.len);
    *route_state_buf_len = _6410_and_5350_msg.len + 1;
    
    PRINT("route_state_buf = %s\n", route_state_buf);
    
    if ((res = send_msg_to_5350(fd, &tmp, sizeof(tmp))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_5350 failed");
        free(_6410_and_5350_msg.data);
        _6410_and_5350_msg.data = NULL;
        return res;
    }
    #endif
    free(_6410_and_5350_msg.data);
    _6410_and_5350_msg.data = NULL;
    return 0;
}

/**
 * 检查路由设置是否成功
 */
int check_route_config(int fd, char *route_state_buf, unsigned short route_state_buf_len, char index)
{
    int i = 0, j = 0;
    int res = 0;
    struct s_6410_and_5350_msg _6410_and_5350_msg;
    memset(&_6410_and_5350_msg, 0, sizeof(struct s_6410_and_5350_msg));
    _6410_and_5350_msg.head[0] = 0x5A;
    _6410_and_5350_msg.head[1] = 0xA5;
    
    // 检查设置是否成功，如果失败 恢复5350状态
    for (i = 0, j = 1; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
    {
        if (j == network_config.cmd_list[i].cmd_bit)
        {
            // 发送命令到5350
            if ((res = send_msg_to_5350(fd, network_config.cmd_list[i].get_cmd, strlen(network_config.cmd_list[i].get_cmd))) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_5350 failed");
                return res;
            }
            
            // 接收5350发送的数据
            
            if ((res = recv_msg_from_5350(fd, &_6410_and_5350_msg)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg failed");
                if (_6410_and_5350_msg.data != NULL)
                {
                    free(_6410_and_5350_msg.data);
                    _6410_and_5350_msg.data = NULL;
                }
                return res;
            }
            j++;
            PRINT("_6410_and_5350_msg.data = %s, network_config.cmd_list[i].set_value = %s\n", _6410_and_5350_msg.data, network_config.cmd_list[i].set_value);
            if (strcmp(_6410_and_5350_msg.data, network_config.cmd_list[i].set_value) != 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "the route configuration failed");
                free(_6410_and_5350_msg.data);
                _6410_and_5350_msg.data = NULL;
                // 恢复路由器设置之前的状态
                if ((res = recovery_route(fd, route_state_buf, route_state_buf_len)) < 0)
                {
                    free(route_state_buf);
                    route_state_buf = NULL;
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recovery_route failed");
                    return res;
                }
                
                free(route_state_buf);
                route_state_buf = NULL;         
                return DATA_ERR;
            }
            free(_6410_and_5350_msg.data);
            _6410_and_5350_msg.data = NULL;
        }
        
        if (j == index)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "the route configuration success");
            break;
        }
        else if ((i == (sizeof(network_config.cmd_list) / sizeof(struct s_cmd) - 1)))
        {
            i = 0;
        }
    }
    return 0;
}

/**
 * 设置路由器
 */
int route_config(int fd, int index)
{
    int res = 0;
    int i= 0, j = 0;
    struct s_6410_and_5350_msg _6410_and_5350_msg;
    memset(&_6410_and_5350_msg, 0, sizeof(struct s_6410_and_5350_msg));
    _6410_and_5350_msg.head[0] = 0x5A;
    _6410_and_5350_msg.head[1] = 0xA5;
    // 发送命令
    for (i = 0, j = 1; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
    {
        if (j == network_config.cmd_list[i].cmd_bit)
        {
            usleep(1000);
            PRINT("%s\n", network_config.cmd_list[i].set_cmd_and_value);
            // 发送命令到5350
            if ((res = send_msg_to_5350(fd, network_config.cmd_list[i].set_cmd_and_value, strlen(network_config.cmd_list[i].set_cmd_and_value))) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_5350 failed");
                return res;
            }
            
            // 接收5350发送的数据
            if ((res = recv_msg_from_5350(fd, &_6410_and_5350_msg)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg_from_5350 failed");
                if (_6410_and_5350_msg.data == NULL)
                {
                    free(_6410_and_5350_msg.data);
                    _6410_and_5350_msg.data = NULL;
                }
                return res;
            }
            
            if (_6410_and_5350_msg.data[0] == 0xFF)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data err");
                free(_6410_and_5350_msg.data);
                _6410_and_5350_msg.data = NULL;
                return DATA_ERR;
            }
            free(_6410_and_5350_msg.data);
            _6410_and_5350_msg.data = NULL;
            j++;
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
    return 0;
}
#elif BOARDTYPE == 5350

/**
 * 获取路由wan口状态
 */
int get_wan_state()
{
	int fd = 0, value = 0;
	
	if ((fd = open(common_tools.config->wan_state, O_RDONLY)) < 0) 
	{
	    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "open failed", OPEN_ERR);
		return OPEN_ERR;
	}
	
	if (ioctl(fd, 0x62, &value) < 0) 
	{
		PRINT("[%s]\n", "ioctl failed!");
		close(fd);
		return IOCTL_ERR;
	}
	close(fd);
	if (value == 0)
	{
	    return WAN_STATE_ERR;
	}
	return 0;
}
/**
 * 获取PPPOE状态
 */
int get_pppoe_state()
{
    int fd = 0, value = 0;
	char buf[5] = {0};
	if ((fd = open(common_tools.config->pppoe_state, O_RDONLY)) < 0) 
	{
		OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "open failed", OPEN_ERR);
		return OPEN_ERR;
	}
	if (read(fd, buf, 4) != 4)	    
	{
	    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "read failed", READ_ERR);
	    close(fd);
		return READ_ERR;
	}
	close(fd);
	value = atoi(buf);
	PRINT("buf = %s, value = %d\n", buf, value);
	return value;
}

/**
 * 保存当前路由状态 
 */
int save_current_route_state()
{
    char buf[128] = {0};
    sprintf(buf, "ralink_init show 2860 > %s", common_tools.config->old_route_config);
    if (system(buf) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "system failed", SYSTEM_ERR);
		return SYSTEM_ERR;
    }
    return 0;
}

/**
 * 恢复路由器设置之前的状态
 */
int recovery_route(unsigned char cmd_count)
{
    PRINT("____________________________\n");
    FILE *fp = NULL;
    int res = 0;
    int i = 0, j = 0, k = 0; 
    char buf[128] = {0};
    char *tmp = NULL;
    
    char *columns_name = NULL; 
    char *columns_value = NULL; 
    unsigned short *columns_value_len = NULL;
    
    if ((columns_name = malloc(cmd_count * 30)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
        return MALLOC_ERR;
    }
    
    if ((columns_value = malloc(cmd_count * 100)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
        free(columns_name);
        columns_name = NULL;
        return MALLOC_ERR;
    }
    
    if ((columns_value_len = malloc(cmd_count * sizeof(unsigned short))) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
        free(columns_name);
        columns_name = NULL;
        free(columns_value);
        columns_value = NULL;
        return MALLOC_ERR;
    }
        
    if ((fp = fopen(common_tools.config->old_route_config, "r")) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "fopen failed!", OPEN_ERR);
        free(columns_name);
        columns_name = NULL;
        free(columns_value);
        columns_value = NULL;
        free(columns_value_len);
        columns_value_len = NULL;
        return OPEN_ERR;
    }
    
    while (k != cmd_count)
    {
        memset(buf, 0, sizeof(buf));
        if (fgets(buf, sizeof(buf), fp) == NULL)
        {
            break;
        }
        if ((buf[0] == '#') || (buf[0] == '\n')) 
        {
            continue;
        }
        if ((res = common_tools.trim(buf)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "trim failed!", res);
            fclose(fp);
            
            free(columns_name);
            columns_name = NULL;
            free(columns_value);
            columns_value = NULL;
            free(columns_value_len);
            columns_value_len = NULL;
            return res;
        }
        for (i = 0; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
        {
            PRINT("____________________________\n");
            if ((tmp = strstr(buf, network_config.cmd_list[i].cmd_key)) != NULL)
            {
                if (*(tmp + strlen(network_config.cmd_list[i].cmd_key)) != '=')
                {
                    continue;
                }
                memset(network_config.cmd_list[i].set_value, 0, sizeof(network_config.cmd_list[i].set_value));
                for (j = 0; j < sizeof(network_config.cmd_list[i].set_value); j++)
                {
                    if (*(tmp + strlen(network_config.cmd_list[i].cmd_key) + 1 + j) == '\n')
                    {
                        break;
                    }
                    network_config.cmd_list[i].set_value[j] = *(tmp + strlen(network_config.cmd_list[i].cmd_key) + j + 1);
                }

                memset(network_config.cmd_list[i].set_cmd_and_value, 0, sizeof(network_config.cmd_list[i].set_cmd_and_value));
                sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                PRINT("network_config.cmd_list[i].set_value = %s, network_config.cmd_list[i].set_cmd_and_value = %s\n", network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                
                memcpy(columns_name + k * 30, network_config.cmd_list[i].cmd_key, strlen(network_config.cmd_list[i].cmd_key)); 
                memcpy(columns_value + k * 100, network_config.cmd_list[i].set_value, strlen(network_config.cmd_list[i].set_value)); 
                columns_value_len[k * sizeof(unsigned short)] = strlen(network_config.cmd_list[i].set_value); 
                PRINT("columns_name[k] = %s, columns_value[k] = %s, columns_value_len[k] = %d\n", columns_name + k * 30, columns_value + k * 100, columns_value_len + k * sizeof(unsigned short));            
                
                k++;
                break;
            }
        }
    }
    fclose(fp);
    if ((res = nvram_interface.update(NVRAM_RT5350, cmd_count, (char(*)[30])columns_name, (char(*)[100])columns_value, (unsigned short *)columns_value_len)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_update failed!", res);
        free(columns_name);
        columns_name = NULL;
        free(columns_value);
        columns_value = NULL;
        free(columns_value_len);
        columns_value_len = NULL;
        return res;
    }
    free(columns_name);
    columns_name = NULL;
    free(columns_value);
    columns_value = NULL;
    free(columns_value_len);
    columns_value_len = NULL;
    if ((res = config_route_take_effect()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "config_route_take_effect failed!", res);
        return res;
    }
    PRINT("____________________________\n");
    return 0;
}

/**
 * 检查路由设置是否成功
 */
int check_route_config(unsigned char cmd_count)
{
    int res = 0;
    int i = 0, j = 0;
    
    char out_buf[100] = {0};
    // 检查设置是否成功，如果失败 恢复5350状态
    for (i = 0, j = 1; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
    {
        if (j == network_config.cmd_list[i].cmd_bit)
        {
            #if 0
            if ((res = common_tools.get_cmd_out(network_config.cmd_list[i].get_cmd, out_buf, sizeof(out_buf))) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_cmd_out failed", res);
                return res;
            }
            #else
            PRINT("network_config.cmd_list[i].cmd_key = %s\n", network_config.cmd_list[i].cmd_key);
            if ((res = nvram_interface.select(NVRAM_RT5350, 1, (char (*)[30])network_config.cmd_list[i].cmd_key,  (char (*)[100])out_buf)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed", res);
                return res;
            }
            #endif
            if (strlen(out_buf) == 0) 
            {
                strcpy(out_buf, "\"\"");
            }
            j++;
            PRINT("out_buf = %s, network_config.cmd_list[i].set_value = %s\n", out_buf, network_config.cmd_list[i].set_value);
            if (memcmp(out_buf, network_config.cmd_list[i].set_value, strlen(network_config.cmd_list[i].set_value)) != 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "the route configuration failed", res);
                // 恢复路由器设置之前的状态
                if ((res = recovery_route(cmd_count)) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recovery_route failed", res);
                    return res;
                } 
                PRINT("____________________________\n");    
                return DATA_ERR;
            }
        }
        
        if (j == cmd_count)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "the route configuration success", res);
            break;
        }
        else if ((i == (sizeof(network_config.cmd_list) / sizeof(struct s_cmd) - 1)))
        {
            i = 0;
        }
    }
    return 0;
}

/**
 * 使设置生效
 */
int config_route_take_effect()
{
    int res = 0;
    if (system("kill -9 `ps | grep nvram_daemon | sed '/grep/'d | awk '{print $1}'`") < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "system failed", SYSTEM_ERR);
        return SYSTEM_ERR;
    }
    
    if (system("kill -9 `ps | grep goahead | sed '/grep/'d | awk '{print $1}'`") < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "system failed", SYSTEM_ERR);
        return SYSTEM_ERR;
    }
    
    char * const nvram_daemon_app_argv[] = {"nvram_daemon", NULL};
    char * const goahead_app_argv[] = {"goahead", NULL};
    
    if ((res = common_tools.start_up_application("/bin/nvram_daemon", nvram_daemon_app_argv, 0)) < 0)
    {
        PERROR("start_up_application failed!\n");
        return res;
    }
    
    if ((res = common_tools.start_up_application("/bin/goahead", goahead_app_argv, 0)) < 0)
    {
        PERROR("start_up_application failed!\n");
        return res;
    }
    return 0;
}

/**
 * 设置路由器
 */
int route_config(int index)
{
    int res = 0;
    int i= 0, j = 0;
    char *columns_name = NULL; 
    char *columns_value = NULL; 
    unsigned short *columns_value_len = NULL;
    PRINT("index = %d\n", index);
    if ((columns_name = malloc(index * 30)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
        PRINT("errno = %d, strerror(errno) = %s\n", errno, strerror(errno));
        return MALLOC_ERR;
    }
    
    if ((columns_value = malloc(index * 100)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
        free(columns_name);
        return MALLOC_ERR;
    }
    
    if ((columns_value_len = malloc(index * sizeof(unsigned short))) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
        free(columns_name);
        free(columns_value);
        return MALLOC_ERR;
    }
    memset(columns_name, 0, index * 30);
    memset(columns_value, 0, index * 100);
    memset(columns_value_len, 0, index * sizeof(unsigned short));
    PRINT("%p %p %p \n", columns_name, columns_value, columns_value_len);
    // 设置5350
    for (i = 0, j = 1; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
    {
        //PRINT("network_config.cmd_list[i].cmd_bit = %02X, index = %d, j = %d\n", network_config.cmd_list[i].cmd_bit, index, j);
        if (j == network_config.cmd_list[i].cmd_bit)
        {
            memcpy(columns_name + (j - 1) * 30, network_config.cmd_list[i].cmd_key, strlen(network_config.cmd_list[i].cmd_key)); 
            memset(columns_value + (j - 1) * 100, 0, 100);
            memset(columns_value_len + (j - 1) * sizeof(unsigned short), 0, sizeof(unsigned short));
            if (memcmp("\"\"", network_config.cmd_list[i].set_value, strlen(network_config.cmd_list[i].set_value)) != 0)
            {   
                memcpy(columns_value + (j - 1) * 100, network_config.cmd_list[i].set_value, strlen(network_config.cmd_list[i].set_value)); 
                columns_value_len[(j - 1) * sizeof(unsigned short)] = strlen(network_config.cmd_list[i].set_value); 
            }
            
            PRINT("columns_name[j] = %s, columns_value[j] = %s, columns_value_len[j] = %d\n", columns_name + (j - 1) * 30, 
                columns_value + (j - 1) * 100, *(columns_value_len + (j - 1) * sizeof(unsigned short)));
            j++;
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
    
    PRINT("____________________________\n");
    PRINT("%s %p %p %p index = %d\n", columns_name, columns_name, columns_value, columns_value_len, index);
    if ((res = nvram_interface.update(NVRAM_RT5350, index, (char(*)[30])columns_name, (char(*)[100])columns_value, (unsigned short *)columns_value_len)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_update failed!", res);
        free(columns_name);
        free(columns_value);
        free(columns_value_len);
        return res;
    }
    PRINT("____________________________\n");
    free(columns_name);
    free(columns_value);
    free(columns_value_len);
    return 0;
}

int route_config2(int index)
{
    PRINT("route_config2 entry!\n");
    int res = 0;
    int i= 0, j = 0;
    for (i = 0, j = 1; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
    {
        //PRINT("network_config.cmd_list[i].cmd_bit = %02X, index = %d, j = %d\n", network_config.cmd_list[i].cmd_bit, index, j);
        if (j == network_config.cmd_list[i].cmd_bit)
        {
            if (memcmp("\"\"", network_config.cmd_list[i].set_value, strlen(network_config.cmd_list[i].set_value)) != 0)
            {
                PRINT("network_config.cmd_list[i].set_cmd_and_value = %s\n", network_config.cmd_list[i].set_cmd_and_value);
                memset(network_config.cmd_list[i].set_cmd_and_value, 0, sizeof(network_config.cmd_list[i].set_cmd_and_value));
                sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s \"%s\"", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                PRINT("network_config.cmd_list[i].set_cmd_and_value = %s\n", network_config.cmd_list[i].set_cmd_and_value);    
            }
            
            if (system(network_config.cmd_list[i].set_cmd_and_value) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "system failed!", SYSTEM_ERR);
                return SYSTEM_ERR;
            } 
            j++;
            if (memcmp("\"\"", network_config.cmd_list[i].set_value, strlen(network_config.cmd_list[i].set_value)) != 0)
            {
                memset(network_config.cmd_list[i].set_cmd_and_value, 0, sizeof(network_config.cmd_list[i].set_cmd_and_value));
                sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s %s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                PRINT("network_config.cmd_list[i].set_cmd_and_value = %s\n", network_config.cmd_list[i].set_cmd_and_value);
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
    PRINT("route_config2 exit!\n");
    return 0;
}

#elif BOARDTYPE == 9344

/**
 * 获取路由wan口状态
 */
int get_wan_state()
{
    return 0;
}
/**
 * 使设置生效
 */
int config_route_take_effect()
{
    system("/etc/ath/apdown");
    system("/etc/ath/apup");
    return 0;
}

/**
 * 检查路由设置是否成功
 */
int check_route_config(unsigned char cmd_count)
{
    return 0;
}

/**
 * 设置路由器
 */
int route_config(int index)
{
    int res = 0;
    int i= 0, j = 0;
    for (i = 0, j = 1; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
    {
        //PRINT("network_config.cmd_list[i].cmd_bit = %02X, index = %d, j = %d\n", network_config.cmd_list[i].cmd_bit, index, j);
        if (j == network_config.cmd_list[i].cmd_bit)
        {
            if (memcmp("\"\"", network_config.cmd_list[i].set_value, strlen(network_config.cmd_list[i].set_value)) != 0)
            {
                PRINT("network_config.cmd_list[i].set_cmd_and_value = %s\n", network_config.cmd_list[i].set_cmd_and_value);
                memset(network_config.cmd_list[i].set_cmd_and_value, 0, sizeof(network_config.cmd_list[i].set_cmd_and_value));
                sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s \"%s\"", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                PRINT("network_config.cmd_list[i].set_cmd_and_value = %s\n", network_config.cmd_list[i].set_cmd_and_value);    
            }
            
            if (system(network_config.cmd_list[i].set_cmd_and_value) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "system failed!", SYSTEM_ERR);
                return SYSTEM_ERR;
            } 
            j++;
            if (memcmp("\"\"", network_config.cmd_list[i].set_value, strlen(network_config.cmd_list[i].set_value)) != 0)
            {
                memset(network_config.cmd_list[i].set_cmd_and_value, 0, sizeof(network_config.cmd_list[i].set_cmd_and_value));
                sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s %s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                PRINT("network_config.cmd_list[i].set_cmd_and_value = %s\n", network_config.cmd_list[i].set_cmd_and_value);
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
    system("cfg -c");
    return 0;
}

#endif

/**
 * 终端初始化 01 02 03 07
 */
int network_settings(int fd, int cmd_count, char cmd_word)
{
    int res = 0;
    int i = 0, j = 0;
    char buf[100] = {0};
    unsigned char index = 0;
    char *route_state_buf = NULL;
    unsigned short route_state_buf_len = 0;
    
    const char *ip_rule = "^[1-9]([0-9]{0,2}).([1-9]([0-9]{0,2})|0).([1-9]([0-9]{0,2})|0).([1-9]([0-9]{0,2})|0)$";
    const char *mask__rule = "^((255.255.255.(0|128|192|224|240|248|252))|(255.255.(0|128|192|224|240|248|252|254|255).0)|(255.(0|128|192|224|240|248|252|254|255).0.0)|((128|192|224|240|248|252|254|255).0.0.0))$";
    const char *mac_rule = "^[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}$";
    
    char pad_sn[37] = {0};
    char pad_mac[20] = {0};
    char pad_ip[16] = {0};
    
    char ssid1[64] = {0};
    char wpapsk1[64] = {0};
    char ssid2[64] = {0};
    char wpapsk2[64] = {0};
    char auth[64] = {0};
    
    char wan_ipaddr[18] = {0};
    char wan_netmask[18] = {0};
    char wan_gateway[18] = {0};
    char wan_primary_dns[18] = {0};
    char wan_secondary_dns[18] = {0};
    
    char mac_clone_enabled[4] = {0};
    char mac_clone_mac[20] = {0};
    
    char wan_dhcp_hn[64] = {0};
    char wan_pppoe_user[64] = {0};
    char wan_pppoe_pass[64] = {0};
    
    struct s_6410_and_5350_msg _6410_and_5350_msg;
    struct s_pad_and_6410_msg pad_and_6410_msg;
    memset(&_6410_and_5350_msg, 0, sizeof(struct s_6410_and_5350_msg));
    memset(&pad_and_6410_msg, 0, sizeof(struct s_pad_and_6410_msg));
    memset(network_config.base_sn, 0, sizeof(network_config.base_sn));
    memset(network_config.base_mac, 0, sizeof(network_config.base_mac));
    memset(network_config.base_ip, 0, sizeof(network_config.base_ip));

    pad_and_6410_msg.head[0] = 0x5A;
    pad_and_6410_msg.head[1] = 0xA5;
    _6410_and_5350_msg.head[0] = 0x5A;
    _6410_and_5350_msg.head[1] = 0xA5;
    
    // 智能恢复
    #if SMART_RECOVERY
    char values[14][100] = {0};
    unsigned short values_len[14] = {0};
    char column_name[14][30] = {0};
    unsigned char count = 0;
    #endif
    
    PRINT("cmd_word = %02X cmd_count = %02X\n", cmd_word, cmd_count);
    #if 0
    if (((cmd_word == 0x01) && (cmd_count != 7)) || ((cmd_word == 0x02) && (cmd_count != 11)) || 
        ((cmd_word == 0x03) && (cmd_count != 6)) || ((cmd_word == 0x07) && (cmd_count != 3)) || 
        ((cmd_word == 0x08) && (cmd_count != 4)) || ((cmd_word == 0x09) && (cmd_count != 8)) || 
        ((cmd_word == 0x0A) && (cmd_count != 4)))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", P_DATA_ERR);
        return P_DATA_ERR;
    }
    #else // 20131111 新需求去掉mac地址复制
    if (((cmd_word == 0x01) && (cmd_count != 4)) || ((cmd_word == 0x02) && (cmd_count != 7)) || 
        ((cmd_word == 0x03) && (cmd_count != 5)) || ((cmd_word == 0x07) && (cmd_count != 3)) || 
        ((cmd_word == 0x08) && (cmd_count != 2)) || ((cmd_word == 0x09) && (cmd_count != 5)) || 
        ((cmd_word == 0x0A) && (cmd_count != 3)))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", P_DATA_ERR);
        return P_DATA_ERR;
    }
    #endif
    
    #if SMART_RECOVERY
    if (cmd_word != 0x07)
    {
        sprintf(values[count], "%d", cmd_word);
        values_len[count] = strlen(values[count]);
        strcpy(column_name[count], "cmd_word");
        count++;
    }
    #endif
    
    pthread_mutex_lock(&network_config.recv_mutex);
    network_config.pthread_recv_flag = 1;
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv msg from pad start!", 0);
    // 数据接收
    for (i = 0; i < cmd_count; i++)
    {
        if ((res = recv_msg_from_pad(fd, &pad_and_6410_msg)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg_from_pad failed", res);
            if (pad_and_6410_msg.data != NULL)
            {
                free(pad_and_6410_msg.data);
                pad_and_6410_msg.data = NULL;
            }
            return res;
        }
        switch (pad_and_6410_msg.cmd)
        {
            case 0x10: // wan口状态查询
            {
                #if BOARDTYPE == 6410
                if ((res = send_msg_to_5350(serial_5350_fd, "wan state?", strlen("wan state?"))) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_5350 failed", res); 
                    return res;
                }
                
                if ((res = recv_msg_from_5350(serial_5350_fd, &_6410_and_5350_msg)) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg_from_5350 failed", res); 
                    if (_6410_and_5350_msg.data != NULL)
                    {
                        free(_6410_and_5350_msg.data);
                        _6410_and_5350_msg.data = NULL;
                    }
                    return res;
                }
                
                if (_6410_and_5350_msg.data[0] == 0xFF)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data err", DATA_ERR); 
                    free(_6410_and_5350_msg.data);
                    _6410_and_5350_msg.data = NULL;
                    return DATA_ERR;
                }
                free(_6410_and_5350_msg.data);
                _6410_and_5350_msg.data = NULL;
                #elif BOARDTYPE == 5350 ||  BOARDTYPE == 9344
                /*
                if ((res = get_wan_state()) < 0)
                {
                    return res;
                }
                */
                #endif
                PRINT("wan normal!\n");
                break;
            }
            #if 0
            case 0x11: // 电话线状态查询
            {
                if ((res = common_tools.get_phone_stat()) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_phone_stat failed!", res);
                    return res;
                }
                PRINT("phone line normal!\n");
                break;
            }
            #endif
            case 0x12: // pad 序列号
            {
                memcpy(pad_sn, pad_and_6410_msg.data, sizeof(pad_sn) - 1);
                PRINT("pad_sn = %s\n", pad_sn);
                break;
            }
            case 0x13: // pad mac
            {
                memcpy(pad_mac, pad_and_6410_msg.data, sizeof(pad_mac) - 1); 
                PRINT("pad_mac = %s\n", pad_mac);
                break;
            }
            case 0x14: // IP 地址
            {
                memcpy(wan_ipaddr, pad_and_6410_msg.data, strlen(pad_and_6410_msg.data));
                if ((res = common_tools.match_str(ip_rule, wan_ipaddr)) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "match_str failed!", res);
                    return res;                    
                }
                
                #if SMART_RECOVERY
                memcpy(values[count], wan_ipaddr, strlen(wan_ipaddr));
                values_len[count] = strlen(values[count]);
                strcpy(column_name[count], "wan_ipaddr");
                count++;
                #endif
                break;
            }
            case 0x15: // 子网掩码
            {
                memcpy(wan_netmask, pad_and_6410_msg.data, strlen(pad_and_6410_msg.data));
                if ((res = common_tools.match_str(mask__rule, wan_netmask)) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "match_str failed!", res);
                    return res;                    
                }
                
                #if SMART_RECOVERY
                memcpy(values[count], wan_netmask, strlen(wan_netmask));
                values_len[count] = strlen(values[count]);
                strcpy(column_name[count], "wan_netmask");
                count++;
                #endif
                break;
            }
            case 0x16: // 预设网关
            {
                memcpy(wan_gateway, pad_and_6410_msg.data, strlen(pad_and_6410_msg.data));
                if ((res = common_tools.match_str(ip_rule, wan_gateway)) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "match_str failed!", res);
                    return res;                    
                }
                
                #if SMART_RECOVERY
                memcpy(values[count], wan_gateway, strlen(wan_gateway));
                values_len[count] = strlen(values[count]);
                strcpy(column_name[count], "wan_gateway");
                count++;
                #endif
                break;
            }
            case 0x17: // 惯用DNS服务器
            {
                memcpy(wan_primary_dns, pad_and_6410_msg.data, strlen(pad_and_6410_msg.data));
                if ((res = common_tools.match_str(ip_rule, wan_primary_dns)) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "match_str failed!", res);
                    return res;                    
                }
                
                #if SMART_RECOVERY
                memcpy(values[count], wan_primary_dns, strlen(wan_primary_dns));
                values_len[count] = strlen(values[count]);
                strcpy(column_name[count], "wan_primary_dns");
                count++;
                #endif
                break;
            }
            #if 0
            case 0x18: // 其他DNS服务器
            {
                memcpy(wan_secondary_dns, pad_and_6410_msg.data, strlen(pad_and_6410_msg.data));
                if (((res = common_tools.match_str(ip_rule, wan_secondary_dns)) < 0) && (memcmp(wan_secondary_dns, "\"\"", strlen("\"\"")) != 0))
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data err!", res); 
                    return P_DATA_ERR;                   
                }
                
                #if SMART_RECOVERY
                memcpy(values[count], wan_secondary_dns, strlen(wan_secondary_dns));
                values_len[count] = strlen(values[count]);
                strcpy(column_name[count], "wan_secondary_dns");
                count++;
                #endif
                break;
            }
            #endif
            case 0x19: // 网络名称
            {
                memcpy(wan_dhcp_hn, pad_and_6410_msg.data, strlen(pad_and_6410_msg.data));
                
                #if SMART_RECOVERY
                memcpy(values[count], wan_dhcp_hn, strlen(wan_dhcp_hn));
                values_len[count] = strlen(values[count]);
                strcpy(column_name[count], "wan_dhcp_hn");
                count++;
                #endif
                break;
            }
            case 0x1A: // 使用者姓名
            {
                memcpy(wan_pppoe_user, pad_and_6410_msg.data, strlen(pad_and_6410_msg.data));
                
                #if SMART_RECOVERY
                memcpy(values[count], wan_pppoe_user, strlen(wan_pppoe_user));
                values_len[count] = strlen(values[count]);
                strcpy(column_name[count], "wan_pppoe_user");
                count++;
                #endif
                break;
            }
            case 0x1B: // 口令
            {
                memcpy(wan_pppoe_pass, pad_and_6410_msg.data, strlen(pad_and_6410_msg.data));
                if (strlen(wan_pppoe_pass) < 8)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data err", P_DATA_ERR);
                    return P_DATA_ERR;                    
                }
                
                #if SMART_RECOVERY
                memcpy(values[count], wan_pppoe_pass, strlen(wan_pppoe_pass));
                values_len[count] = strlen(values[count]);
                strcpy(column_name[count], "wan_pppoe_pass");
                count++;
                #endif
                break;
            }
            #if 0
            case 0x1C: // 使能MAC复制
            {
                memcpy(mac_clone_enabled, pad_and_6410_msg.data, strlen(pad_and_6410_msg.data));
                
                #if SMART_RECOVERY
                memcpy(values[count], mac_clone_enabled, strlen(mac_clone_enabled));
                values_len[count] = strlen(values[count]);
                strcpy(column_name[count], "mac_clone_enabled");
                count++;
                #endif
                break;
            }
            case 0x1D: // 填充MAC地址
            {
                memcpy(mac_clone_mac, pad_and_6410_msg.data, strlen(pad_and_6410_msg.data));
                if (memcmp(mac_clone_enabled, "1", strlen("1")) == 0)
                {
                    if ((res = common_tools.match_str(mac_rule, mac_clone_mac)) < 0)
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "match_str failed!", res);
                        return res;                    
                    }
                }
                else
                {
                    if (memcmp(mac_clone_mac, "\"\"", strlen("\"\"")) != 0)
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data err", P_DATA_ERR);
                        return P_DATA_ERR;                    
                    }
                }
                
                #if SMART_RECOVERY
                memcpy(values[count], mac_clone_mac, strlen(mac_clone_mac));
                values_len[count] = strlen(values[count]);
                strcpy(column_name[count], "mac_clone_mac");
                count++;
                #endif
                break;
            }
            #endif
            case 0x1E: // SSID2
            {
                memcpy(ssid2, pad_and_6410_msg.data, strlen(pad_and_6410_msg.data));
                
                #if SMART_RECOVERY
                memcpy(values[count], ssid2, strlen(ssid2));
                values_len[count] = strlen(values[count]);
                strcpy(column_name[count], "ssid2");
                count++;
                #endif
                break;
            }
            case 0x1F: // SSID2密码
            {
                memcpy(wpapsk2, pad_and_6410_msg.data, strlen(pad_and_6410_msg.data));
                
                #if SMART_RECOVERY
                memcpy(values[count], wpapsk2, strlen(wpapsk2));
                values_len[count] = strlen(values[count]);
                strcpy(column_name[count], "wpapsk2");
                count++;
                #endif
                break;
            }
            case 0x20: // 加密方式
            {
                memcpy(auth, pad_and_6410_msg.data, strlen(pad_and_6410_msg.data));
                
                #if SMART_RECOVERY
                memcpy(values[count], auth, strlen(auth));
                values_len[count] = strlen(values[count]);
                strcpy(column_name[count], "auth");
                count++;
                #endif
                break;
            }
            case 0x53: // 终止命令
            {
                PRINT("stop config!\n");
                return STOP_CMD;
            }
            case 0x04: // 询问
            {
                PRINT("cmd = %02X\n", pad_and_6410_msg.cmd);
                for (j = 0; j < common_tools.config->repeat; j++)
                {
                    if (pad_and_6410_msg.data != NULL)
                    {
                        free(pad_and_6410_msg.data);
                        pad_and_6410_msg.data = NULL;
                    }
                    // 数据发送到PAD                            
                    if ((res = send_msg_to_pad(fd, 0XFA, NULL)) < 0)
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                        continue;
                    }
                    if ((res = recv_msg_from_pad(fd, &pad_and_6410_msg)) < 0)
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
                if (res < 0)
                {
                    if (pad_and_6410_msg.data != NULL)
                    {
                        free(pad_and_6410_msg.data);
                        pad_and_6410_msg.data = NULL;
                    }
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "write error!", res);
                    return res;
                }
            }
            default:
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "option does not mismatching!", P_MISMATCH_ERR);
                return P_MISMATCH_ERR;
            }  
        }
        if (pad_and_6410_msg.data != NULL)
        {
            free(pad_and_6410_msg.data);
            pad_and_6410_msg.data = NULL;
        }
        
        if (i < (cmd_count - 1))
        {
            if ((res = send_msg_to_pad(fd, 0x00, NULL)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                return res;
            }
        }
    }
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv msg from pad end!", 0);
    network_config.pthread_recv_flag = 0;
    network_config.pthread_flag = 1;
    pthread_mutex_unlock(&network_config.recv_mutex);
    
    #if SMART_RECOVERY
    if ((res = nvram_interface.insert(RT5350_BACKUP_SPACE, count, column_name, values, values_len)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_insert failed!", res);
        return res;
    }
    #endif
    
    if ((unsigned char)pad_cmd == 0xFB) // 局域网络没有设置时
    {
        // 获取base_sn mac
        if ((res = terminal_register.get_base_sn_and_mac(network_config.base_sn, network_config.base_mac)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_base_sn_and_mac failed", res);
            return res;
        }
        memcpy(pad_ip, common_tools.config->pad_ip, strlen(common_tools.config->pad_ip));
        memcpy(network_config.base_ip, common_tools.config->base_ip, strlen(common_tools.config->base_ip));
        PRINT("get_base_sn_and_mac end!\n");
        
        if ((res = common_tools.get_rand_string(12, 0, ssid1, UTF8)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_rand_string failed", res);
            return res;
        }
        strncat(ssid1, network_config.base_mac, strlen(network_config.base_mac));
        PRINT("ssid1 = %s\n", ssid1);
        if ((res = common_tools.get_rand_string(12, 0, wpapsk1, UTF8)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_rand_string failed", res);
            return res;
        }
        PRINT("wpapsk1 = %s\n", wpapsk1);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "SSID and password randomly generated successfully!", 0);
        
        #if SMART_RECOVERY
        memset(values, 0, sizeof(values));
        memset(values_len, 0, sizeof(values_len));
        memset(column_name, 0, sizeof(column_name));
        strcpy(column_name[0], "pad_sn");
        strcpy(column_name[1], "pad_mac");
        strcpy(column_name[2], "pad_ip");
        strcpy(column_name[3], "base_sn");
        strcpy(column_name[4], "base_mac");
        strcpy(column_name[5], "base_ip");
        strcpy(column_name[6], "ssid_user_name");
        strcpy(column_name[7], "ssid_password");
        
        #else
        
        // 把数据插入数据库
        char values[8][100] = {0};
        unsigned short values_len[8] = {0};
        char column_name[8][30] = { 
            "pad_sn",
            "pad_mac",
            "pad_ip",
            "base_sn",
            "base_mac",
            "base_ip",
            "ssid_user_name",
            "ssid_password"
        };
        #endif
        
        memcpy(values[0], pad_sn, strlen(pad_sn));
        memcpy(values[1], pad_mac,strlen(pad_mac));
        memcpy(values[2], pad_ip, strlen(pad_ip));
        memcpy(values[3], network_config.base_sn, strlen(network_config.base_sn));
        memcpy(values[4], network_config.base_mac, strlen(network_config.base_mac));
        memcpy(values[5], network_config.base_ip, strlen(network_config.base_ip));
        memcpy(values[6], ssid1,strlen(ssid1));
        memcpy(values[7], wpapsk1,strlen(wpapsk1));
        
        for (i = 0; i < 8; i++)
        {
            values_len[i] = strlen(values[i]);
        }
        PRINT("_________________________________\n");
        #if BOARDTYPE == 6410 || BOARDTYPE == 9344
        if ((res = database_management.insert(8, column_name, values, values_len)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_insert failed", res);
            return res;
        }
        #elif BOARDTYPE == 5350
        PRINT("_________________________________\n");
        if ((res = nvram_interface.insert(RT5350_FREE_SPACE, 8, column_name, values, values_len)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_insert failed!", res);
            return res;
        }
        PRINT("_________________________________\n");
        #endif
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "The database insert success!", 0);    
    }
    
    common_tools.mac_add_colon(pad_mac);
    common_tools.mac_add_colon(network_config.base_mac);
    #if BOARDTYPE == 5350
    for (i = 0; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
    {   
        if ((unsigned char)pad_cmd == 0xFB) // 局域网络没有设置时
        {
            // 初始化设备项
            switch (network_config.cmd_list[i].cmd_word)
            {
                case 0x43: // SSID1
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    sprintf(network_config.cmd_list[i].set_value, "%s", ssid1);
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x44: // SSID1 密码
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    sprintf(network_config.cmd_list[i].set_value, "%s", wpapsk1);
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x45: // 加密
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "WPAPSKWPA2PSK;WPAPSKWPA2PSK", strlen("WPAPSKWPA2PSK;WPAPSKWPA2PSK"));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x30: // SSID2
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    // 正式生产时，打开开关
                    #if 0
                    memcpy(network_config.cmd_list[i].set_value, "Hander_ap", strlen("Hander_ap"));
                    #else
                    common_tools.mac_del_colon(network_config.base_mac);
                    memcpy(network_config.cmd_list[i].set_value, network_config.base_mac,strlen(network_config.base_mac));
                    common_tools.mac_add_colon(network_config.base_mac);
                    #endif
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x31: // SSID2 密码
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    sprintf(network_config.cmd_list[i].set_value, "%s", "administrator");
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x32: // 静态指定1
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    sprintf(network_config.cmd_list[i].set_value, "%s;%s", pad_mac, pad_ip);
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                #if BOARDTYPE == 6410 
                case 0x33: // 静态指定2
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    sprintf(network_config.cmd_list[i].set_value, "%s;%s", network_config.base_mac, network_config.base_ip);
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                #endif
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
                case 0x38:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "ralink", strlen("ralink"));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x39:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "TIME", strlen("TIME"));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x3A:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "1", strlen("1"));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x3B:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "HT", strlen("HT"));                
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x3C:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "\"\"", strlen("\"\""));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x3D:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "\"\"", strlen("\"\""));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x3E:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "0", strlen("0"));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x3F:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "0", strlen("0"));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x40:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "0", strlen("0"));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x41:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "0", strlen("0"));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x42:
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "0", strlen("0"));
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
        
        // 网络设置项
        switch (cmd_word)
        {
            case 0x01: // 动态IP
            case 0x08: // 动态IP
            {
                switch(network_config.cmd_list[i].cmd_word)
                {
                    case 0x19:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", wan_dhcp_hn);
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x1C:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", mac_clone_enabled);
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x1D:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", mac_clone_mac);
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x4A:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        memcpy(network_config.cmd_list[i].set_value, "DHCP", strlen("DHCP"));
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
                break;
            }
            case 0x02: // 静态IP
            case 0x09: // 静态IP
            {
                switch(network_config.cmd_list[i].cmd_word)
                {
                    case 0x14:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", wan_ipaddr);        
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x15:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", wan_netmask); 
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x16:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", wan_gateway); 
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x17:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", wan_primary_dns);
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x18:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", wan_secondary_dns);
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x1C:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", mac_clone_enabled);
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x1D:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", mac_clone_mac);
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x4A:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        memcpy(network_config.cmd_list[i].set_value, "STATIC", strlen("STATIC"));
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    default:
                    {
                        break;
                    }
                } 
                break;              
            }
            case 0x03: // PPPOE
            case 0x0A: // PPPOE
            {
                switch(network_config.cmd_list[i].cmd_word)
                {
                    case 0x1A:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", wan_pppoe_user);
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x1B:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", wan_pppoe_pass);
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);             
                        break;
                    }                    
                    case 0x4A:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        memcpy(network_config.cmd_list[i].set_value, "PPPOE", strlen("PPPOE"));
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x4B:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        memcpy(network_config.cmd_list[i].set_value, "KeepAlive", strlen("KeepAlive"));                        
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x4C:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        memcpy(network_config.cmd_list[i].set_value, "60", strlen("60"));                        
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value);
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
                break;
            }
            case 0x07: // SSID2设置
            {
                #if BOARDTYPE == 5350
                int ssid_count = 0;
                char *column_name = "BssidNum";
                if ((res = nvram_interface.select(NVRAM_RT5350, 1, (char (*)[30])column_name, (char (*) [100])buf)) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);
                    return res;
                }
                if ((strlen(buf) == 0) || (memcmp("\"\"", buf, strlen(buf)) == 0))
                {
                    memset(buf, 0, sizeof(buf));
                    sprintf(buf, "There is no (%s) record!", column_name);
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, buf, 0); 
                    return NULL_ERR;
                }
                #else 
                //strcpy(buf, "2");
                #endif
                switch(network_config.cmd_list[i].cmd_word)
                {
                    case 0x30: // SSID2
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", ssid2);
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x31: // SSID2 密码
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", wpapsk2);
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x34:
                    {   
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        
                        if (buf[0] == '3')
                        {
                            memcpy(network_config.cmd_list[i].set_value, "1;0;0", strlen("1;0;0"));
                        }
                        else
                        {
                            memcpy(network_config.cmd_list[i].set_value, "1;0", strlen("1;0"));
                        }
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x35:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        if (buf[0] == '3')
                        {
                            memcpy(network_config.cmd_list[i].set_value, "AES;AES;AES", strlen("AES;AES;AES"));
                        }
                        else
                        {
                            memcpy(network_config.cmd_list[i].set_value, "AES;AES", strlen("AES;AES"));
                        }
                        
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x36:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        
                        if (buf[0] == '3')
                        {
                            memcpy(network_config.cmd_list[i].set_value, "3600;3600;3600", strlen("3600;3600;3600"));
                        }
                        else
                        {
                            memcpy(network_config.cmd_list[i].set_value, "3600;3600", strlen("3600;3600"));
                        }
                        
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x37:
                    {   
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        if (buf[0] == '3')
                        {
                            memcpy(network_config.cmd_list[i].set_value, "3", strlen("3"));
                        }
                        else
                        {
                            memcpy(network_config.cmd_list[i].set_value, "2", strlen("2"));
                        } 
                                
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x45: // 加密
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        memcpy(network_config.cmd_list[i].set_value, auth, strlen(auth));
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
                break;
            }
            default:
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "option does not mismatching!", P_MISMATCH_ERR);
                return P_MISMATCH_ERR;
            }
        }
    }
    #elif BOARDTYPE == 9344
    for (i = 0; i < sizeof(network_config.cmd_list) / sizeof(struct s_cmd); i++)
    {   
        if ((unsigned char)pad_cmd == 0xFB) // 局域网络没有设置时
        {
            // 初始化设备项
            switch (network_config.cmd_list[i].cmd_word)
            {
                case 0x1D: // SSID1
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    sprintf(network_config.cmd_list[i].set_value, "%s", ssid1);
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x30: // SSID1 密码
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    sprintf(network_config.cmd_list[i].set_value, "%s", wpapsk1);
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x33: // 2.4
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "0", strlen("0"));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x36: // 模式
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "ap", strlen("ap"));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x39: // 隐藏
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "1", strlen("1"));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x3C: // 加密
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "WPA", strlen("WPA"));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x3F: // 加密
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "2", strlen("2"));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x42: // 加密
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "PSK", strlen("PSK"));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x1E: // SSID2
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    // 正式生产时，打开开关
                    #if 0
                    memcpy(network_config.cmd_list[i].set_value, "Hander_ap", strlen("Hander_ap"));
                    #else
                    common_tools.mac_del_colon(network_config.base_mac);
                    memcpy(network_config.cmd_list[i].set_value, network_config.base_mac,strlen(network_config.base_mac));
                    common_tools.mac_add_colon(network_config.base_mac);
                    #endif
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x31: // SSID2 密码
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    sprintf(network_config.cmd_list[i].set_value, "%s", "administrator");
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x34: // 2.4
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "0", strlen("0"));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x37: // 模式
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "ap", strlen("ap"));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x3A: // 隐藏
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "0", strlen("0"));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x3D: // 加密
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "WPA", strlen("WPA"));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x40: // 加密
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "2", strlen("2"));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x43: // 加密
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    memcpy(network_config.cmd_list[i].set_value, "PSK", strlen("PSK"));
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x45: // 静态指定1 mac
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    sprintf(network_config.cmd_list[i].set_value, "%s", pad_mac);
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                case 0x46: // 静态指定1 IP
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    sprintf(network_config.cmd_list[i].set_value, "%s", pad_ip);
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                #if BOARDTYPE == 6410 
                case 0x33: // 静态指定2
                {
                    index++;
                    network_config.cmd_list[i].cmd_bit = index;
                    sprintf(network_config.cmd_list[i].set_value, "%s;%s", network_config.base_mac, network_config.base_ip);
                    sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                    PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                    break;
                }
                #endif
                default:
                {
                    break;
                }
            }
        }
        
        // 网络设置项
        switch (cmd_word)
        {
            case 0x01: // 动态IP
            case 0x08: // 动态IP
            {
                switch(network_config.cmd_list[i].cmd_word)
                {
                    case 0x14:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        memcpy(network_config.cmd_list[i].set_value, "dhcp", strlen("dhcp"));
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
                break;
            }
            case 0x02: // 静态IP
            case 0x09: // 静态IP
            {
                switch(network_config.cmd_list[i].cmd_word)
                {
                    case 0x14: // 模式
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        memcpy(network_config.cmd_list[i].set_value, "static", strlen("static"));
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x15:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", wan_ipaddr);        
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x16:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", wan_netmask); 
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x17:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", wan_gateway); 
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x18:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", wan_primary_dns);
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    default:
                    {
                        break;
                    }
                } 
                break;              
            }
            case 0x03: // PPPOE
            case 0x0A: // PPPOE
            {
                switch(network_config.cmd_list[i].cmd_word)
                {
                    case 0x14: // 模式
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        memcpy(network_config.cmd_list[i].set_value, "pppoe", strlen("pppoe"));
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x1A:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", wan_pppoe_user);
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x1B:
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", wan_pppoe_pass);
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);             
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
                break;
            }
            case 0x07: // SSID2设置
            {
                #if BOARDTYPE == 5350
                int ssid_count = 0;
                char *column_name = "BssidNum";
                if ((res = nvram_interface.select(NVRAM_RT5350, 1, (char (*)[30])column_name, (char (*) [100])buf)) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_select failed!", res);
                    return res;
                }
                if ((strlen(buf) == 0) || (memcmp("\"\"", buf, strlen(buf)) == 0))
                {
                    memset(buf, 0, sizeof(buf));
                    sprintf(buf, "There is no (%s) record!", column_name);
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, buf, 0); 
                    return NULL_ERR;
                }
                #endif
                
                switch(network_config.cmd_list[i].cmd_word)
                {
                    case 0x1E: // SSID2
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", ssid2);
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x31: // SSID2 密码
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        sprintf(network_config.cmd_list[i].set_value, "%s", wpapsk2);
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x34: // 2.4
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        memcpy(network_config.cmd_list[i].set_value, "0", strlen("0"));
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x37: // 模式
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        memcpy(network_config.cmd_list[i].set_value, "ap", strlen("ap"));
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x3A: // 隐藏
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        memcpy(network_config.cmd_list[i].set_value, "0", strlen("0"));
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x3D: // 加密
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        // auth 需要改
                        memcpy(network_config.cmd_list[i].set_value, "WPA", strlen("WPA"));
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x40: // 加密
                    {
                        index++;
                        network_config.cmd_list[i].cmd_bit = index;
                        memcpy(network_config.cmd_list[i].set_value, "2", strlen("2"));
                        sprintf(network_config.cmd_list[i].set_cmd_and_value, "%s%s", network_config.cmd_list[i].set_cmd, network_config.cmd_list[i].set_value); 
                        PRINT("%2d %s %s\n", network_config.cmd_list[i].cmd_bit, network_config.cmd_list[i].set_value, network_config.cmd_list[i].set_cmd_and_value);
                        break;
                    }
                    case 0x43: // 加密
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
                break;
            }
            default:
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "option does not mismatching!", P_MISMATCH_ERR);
                return P_MISMATCH_ERR;
            }
        }
    }
    #endif
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "make cmd success!", 0);
    
    #if BOARDTYPE == 6410
    // 保存路由状态    
    if ((res = save_current_route_state(serial_5350_fd, route_state_buf, &route_state_buf_len)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "save_current_route_state failed", res);
        if (route_state_buf != NULL)
        {
            free(route_state_buf);
            route_state_buf = NULL;
        }
        return res;
    }
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "save current route state success!", 0);
    
    
    // 发送命令
    if ((res = route_config(serial_5350_fd, index)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "route_config failed", res);
        return res;
    }
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send cmd success!", 0);
    // 发送重启命令
    if ((res = send_msg_to_5350(serial_5350_fd, "reboot", strlen("reboot"))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_5350 failed", res);
        return res;
    }
    
    if (pad_cmd == 0xFB)
    {
        // 延时
        sleep(common_tools.config->route_reboot_time_sec);
    }
    else
    {
        sleep(common_tools.config->route_reboot_time_sec / 2);
    }
    
    // 发送重启完毕询问
    for (i = 0; i < (common_tools.config->repeat + 5); i++)
    {
        if ((res = send_msg_to_5350(serial_5350_fd, "reboot success?", strlen("reboot success?"))) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_5350 failed", res);
            continue;
        }
    
        // 接收重启状态回复
        if ((res = recv_msg_from_5350(serial_5350_fd, &_6410_and_5350_msg)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg_from_5350 failed", res);
            if (_6410_and_5350_msg.data == NULL)
            {
                free(_6410_and_5350_msg.data);
                _6410_and_5350_msg.data = NULL;
            }
            if (i != (common_tools.config->repeat + 4))
            {
                sleep(3);
                continue;    
            }            
        }        
        break;
    }
    if (_6410_and_5350_msg.data == NULL)
    {
        free(_6410_and_5350_msg.data);
        _6410_and_5350_msg.data = NULL;
    }
    if (res < 0)
    {
        return res;
    }
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "route reboot success!", 0);
    #elif BOARDTYPE == 5350 // 5350
    /*
    // 保存路由状态    
    if ((res = save_current_route_state()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "save_current_route_state failed", res);
        return res;
    }
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "save current route state success!", 0);
    */
    // 设置5350
    #if 1
    PRINT("before route_config2\n");
    if ((res = route_config2(index)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "route_config failed", res);
        PERROR("route_config2 failed!\n");
        return res;
    }
    PRINT("after route_config2\n");
    #else
    if ((res = route_config(index)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "route_config failed", res);
        return res;
    }
    #endif
    PRINT("before config_route_take_effect!\n");
    // 使路由设置成功
    if ((res = config_route_take_effect()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "config_route_take_effect failed", res);
        return res;
    }
    PRINT("after config_route_take_effect!\n");
    // 是否应该加延时
    sleep(common_tools.config->route_reboot_time_sec);
    
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, " route configuration success!", 0);
    #elif BOARDTYPE == 9344
    if ((res = route_config(index)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "route_config failed", res);
        PERROR("route_config2 failed!\n");
        return res;
    }
    
    // 使路由设置成功
    if ((res = config_route_take_effect()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "config_route_take_effect failed", res);
        return res;
    }
    PRINT("after config_route_take_effect!\n");
    
    // 是否应该加延时
    sleep(common_tools.config->route_reboot_time_sec);
    
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, " route configuration success!", 0);
    #endif
    
    if ((unsigned char)pad_cmd == 0xFB)
    {
        pthread_mutex_lock(&network_config.recv_mutex);
        network_config.pthread_recv_flag = 1;
        network_config.pthread_flag = 0;
        // 发送隐藏通道ssid和密码
        memset(buf, 0, sizeof(buf));
        sprintf(buf, "ssid1:%s,wpapsk1:%s,", ssid1, wpapsk1);
        PRINT("buf = %s\n", buf);
        for (i = 0; i < common_tools.config->repeat; i++)
        {
            if ((res = send_msg_to_pad(fd, 0x00, buf)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                if (i != (common_tools.config->repeat - 1))
                {
                    sleep(5);
                    continue;    
                } 
            }
            PRINT("send SSID1 success!\n");
            if ((res = recv_msg_from_pad(fd, &pad_and_6410_msg)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_msg_from_pad failed", res);
                if (i != (common_tools.config->repeat - 1))
                {
                    sleep(5);
                    continue;    
                } 
            }
            PRINT("recv_msg_from_pad success!\n");
            if (pad_and_6410_msg.cmd == 0xFF) // 发送信息有误
            {
                continue;
            }
            else if (pad_and_6410_msg.cmd == 0x04) // PAD发起询问
            {
                // 可以办到SN正确性
                
                for (j = 0; j < common_tools.config->repeat; j++)
                {
                    if (pad_and_6410_msg.data != NULL)
                    {
                        free(pad_and_6410_msg.data);
                        pad_and_6410_msg.data = NULL;
                    }
                    // 数据发送到PAD                            
                    if ((res = send_msg_to_pad(fd, 0XFA, NULL)) < 0)
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_msg_to_pad failed", res);
                        continue;
                    }
                    if ((res = recv_msg_from_pad(fd, &pad_and_6410_msg)) < 0)
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
                if (res < 0)
                {
                    if (pad_and_6410_msg.data != NULL)
                    {
                        free(pad_and_6410_msg.data);
                        pad_and_6410_msg.data = NULL;
                    }
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "write error!", res);
                    return res;
                }
                i = 0; // 此时认为PAD没有收到SSID，重新发送SSID
            }  
            break;
        }
        network_config.pthread_recv_flag = 0;
        pthread_mutex_unlock(&network_config.recv_mutex);
        
        if (res < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send hide SSID failed", res);
            return res;
        }
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "Pad save SSID success!", 0);  
    }
    if ((res = common_tools.get_network_state(common_tools.config->pad_ip, 4, 6)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_network_state failed!", res);
        return (res == DATA_DIFF_ERR) ? LAN_ABNORMAL : res;
    }
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "Local area network normal!", 0);  
    g_cmd = 0xFD; // 网络设置成功
    if (cmd_word != 0x07)
    {
        #if CHECK_WAN_STATE == 1
        
        #if 1
        if ((res = common_tools.get_network_state(common_tools.config->terminal_server_ip, 3, 3)) < 0)
        #else
        if ((res = common_tools.get_network_state(common_tools.config->wan_check_name, 3, 3)) < 0)
        #endif
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_network_state failed!", res);
            if (cmd_word == 0x03)
            {
                int ret = 0;
                if ((ret = get_pppoe_state(serial_5350_fd)) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_pppoe_state failed!", ret);
                    return (res == DATA_DIFF_ERR) ? WAN_ABNORMAL : res;
                }
                switch (ret)
                {
                    case 0x01: // 网络不通或者服务器没有打开
                    {
                        res = PPPOE_LINK_FAILED;
                        break;
                    }
                    case 0x02: // 连接PPPOE服务器成功
                    {
                        break;
                    }
                    case 0x03: // 表示用户名和密码无效
                    {
                        res = PPPOE_INVALID_INFO;
                        break;
                    }
                    case 0x04: // 表示DNS服务器不存在
                    {
                        res = PPPOE_NO_DNS_SERVER_INFO;
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            } 
            return (res == DATA_DIFF_ERR) ? WAN_ABNORMAL : res;
        }
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "Wide network normal!", 0);
        
        #endif
    }
    
    #if BOARDTYPE == 6410
    // 检查路由设置是否成功
    if ((res = check_route_config(serial_5350_fd, route_state_buf, route_state_buf_len, index)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "check_route_config failed", res);
        return res;
    }
    #elif BOARDTYPE == 5350 // 5350
    if ((res = check_route_config(index)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "check_route_config failed", res);
        return res;
    }
    #elif BOARDTYPE == 9344
    
    #endif
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "Complete routing setup check!", 0);
    
    if (network_flag == 0)
    {
        g_cmd = 0xFE; // 网络设置成功 
        #if USER_REGISTER
        common_tools.del_quotation_marks(pad_sn);
        common_tools.del_quotation_marks(pad_mac);
        common_tools.mac_del_colon(pad_mac);    
        if ((res = terminal_register.user_register(pad_sn, pad_mac)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "user_register failed", res);
            //if (res == LOGIN_FAILED) // 无效的设备
            if (res == WRONGFUL_DEV_ERR) // 无效的设备
            {
                g_cmd = 0xFC;
            }
            return res;
        }
        #endif
        g_cmd = 0x00;  // 注册成功 
    }
    
    return 0;
}
