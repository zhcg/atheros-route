#include "terminal_register.h"

static int respond_data_unpack(void *respond_buf, void *out_buf, unsigned int len);
static int request_data_pack(char **send_pack, char (*columns_value)[100]);
static int get_base_ip(char *base_ip);
static int set_function_id(char *function_id);

static struct s_respond_pack respond_pack;

/**
 * 获取sip服务器账号等信息
 */
static int get_sip_info(char *pad_sn, char *pad_mac);

/**
 * 注册：包括终端认证和获取sip服务器账号等信息
 */
static int user_register(char *pad_sn, char *pad_mac);

/**
 * 获取base板卡的序列号和mac（/proc/cmdline文件）
 */
static int get_base_sn_and_mac(char *base_sn, char *base_mac);


struct class_terminal_register terminal_register = 
{
    &respond_pack, get_sip_info, user_register, get_base_sn_and_mac
};

/**
 * 对响应的数据解包，判断数据是否正确
 */
static int respond_data_unpack(void *respond_buf, void *out_buf, unsigned int len)
{
    PRINT_STEP("entry...\n");
    char *buf_tmp = (char *)respond_buf;
    char *tmp = NULL;
    unsigned int len_tmp = 0;
    unsigned short index = 0;
    char msg_data_status_num[4] = {0};
    
    unsigned char keys[11][32] = {"msgDataStatus:", "sDataProcessTime:", "startTime:", "baseUserName:", "basePassword:",
        "padUserName:", "padPassword:", "port:", "sipIpAddress:", "heartbeatCycle:", "businessCycle:"};
    
    memcpy(&((struct s_respond_pack *)out_buf)->message_type, buf_tmp, sizeof(((struct s_respond_pack*)respond_buf)->message_type));
    buf_tmp += 2;
    
    #if ENDIAN == 0
    if (((struct s_respond_pack *)out_buf)->message_type != 2)
    #else 
    if (DATA_ENDIAN_CHANGE_SHORT(((struct s_respond_pack *)out_buf)->message_type) != 2)
    #endif    
    {   
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "the data is error!", S_DATA_ERR);
        return S_DATA_ERR;
    }    
    
    memcpy(((struct s_respond_pack *)out_buf)->version, buf_tmp, 4);
    
    int i = 0;
    int j = 0;
    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    {
        if ((tmp = strstr(buf_tmp, keys[i])) == NULL)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", S_DATA_ERR);
            return S_DATA_ERR;
        }
        switch (i)
        {
            case 0:
            {
                while ((*(tmp + j) != ',') && (*(tmp + j) != '\0') && (j < sizeof(((struct s_respond_pack *)out_buf)->msg_data_status)))
                {
                    //PRINT("*(tmp + j) = %c, j = %d\n", *(tmp + j), j);
                    ((struct s_respond_pack *)out_buf)->msg_data_status[j] = *(tmp + j);
                    j++;
                }
                
                memcpy(msg_data_status_num, ((struct s_respond_pack *)out_buf)->msg_data_status + strlen(keys[i]), 3);
                
                if (strcmp(msg_data_status_num, "200") != 0)
                {
                    j = 0;
                    i++;
                    if ((tmp = strstr(buf_tmp, keys[i])) == NULL)
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", S_DATA_ERR);
                        return S_DATA_ERR;
                    }
                    
                    while ((*(tmp + j) != ',') && (*(tmp + j) != '\0') && (j < sizeof(((struct s_respond_pack *)out_buf)->s_data_process_time)))
                    {
                        ((struct s_respond_pack *)out_buf)->s_data_process_time[j] = *(tmp + j);
                        j++;
                    }
                    
                    j = 0;
                    i++;
                    if ((tmp = strstr(buf_tmp, keys[i])) == NULL)
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", S_DATA_ERR);
                        return S_DATA_ERR;
                    }
                    
                    while ((*(tmp + j) != ',') && (*(tmp + j) != '\0') && (j < sizeof(((struct s_respond_pack *)out_buf)->start_time)))
                    {
                        ((struct s_respond_pack *)out_buf)->start_time[j] = *(tmp + j);
                        j++;
                    }
                    return 0;
                }
                break;
            }
            case 1:
            {
                while ((*(tmp + j) != ',') && (*(tmp + j) != '\0') && (j < sizeof(((struct s_respond_pack *)out_buf)->s_data_process_time)))
                {
                    ((struct s_respond_pack *)out_buf)->s_data_process_time[j] = *(tmp + j);
                    j++;
                }
                break;
            }
            case 2:
            {
                while ((*(tmp + j) != ',') && (*(tmp + j) != '\0') && (j < sizeof(((struct s_respond_pack *)out_buf)->start_time)))
                {
                    ((struct s_respond_pack *)out_buf)->start_time[j] = *(tmp + j);
                    j++;
                }
                break;
            }
            case 3:
            {
                while ((*(tmp + j) != ',') && (*(tmp + j) != '\0') && (j < sizeof(((struct s_respond_pack *)out_buf)->base_user_name)))
                {
                    ((struct s_respond_pack *)out_buf)->base_user_name[j] = *(tmp + j);
                    j++;
                }
                break;
            }
            case 4:
            {
                while ((*(tmp + j) != ',') && (*(tmp + j) != '\0') && (j < sizeof(((struct s_respond_pack *)out_buf)->base_password)))
                {
                    ((struct s_respond_pack *)out_buf)->base_password[j] = *(tmp + j);
                    j++;
                }
                break;
            }
            case 5:
            {
                while ((*(tmp + j) != ',') && (*(tmp + j) != '\0') && (j < sizeof(((struct s_respond_pack *)out_buf)->pad_user_name)))
                {
                    ((struct s_respond_pack *)out_buf)->pad_user_name[j] = *(tmp + j);
                    j++;
                }
                break;
            }
            case 6:
            {
                while ((*(tmp + j) != ',') && (*(tmp + j) != '\0') && (j < sizeof(((struct s_respond_pack *)out_buf)->pad_password)))
                {
                    ((struct s_respond_pack *)out_buf)->pad_password[j] = *(tmp + j);
                    j++;
                }
                break;
            }
            case 7:
            {
                while ((*(tmp + j) != ',') && (*(tmp + j) != '\0') && (j < sizeof(((struct s_respond_pack *)out_buf)->sip_port)))
                {
                    ((struct s_respond_pack *)out_buf)->sip_port[j] = *(tmp + j);
                    j++;
                }
                break;
            }
            case 8:
            {
                while ((*(tmp + j) != ',') && (*(tmp + j) != '\0') && (j < sizeof(((struct s_respond_pack *)out_buf)->sip_ip_address)))
                {
                    ((struct s_respond_pack *)out_buf)->sip_ip_address[j] = *(tmp + j);
                    j++;
                }
                break;
            }
            case 9:
            {
                while ((*(tmp + j) != ',') && (*(tmp + j) != '\0') && (j < sizeof(((struct s_respond_pack *)out_buf)->heart_beat_cycle)))
                {
                    ((struct s_respond_pack *)out_buf)->heart_beat_cycle[j] = *(tmp + j);
                    j++;
                }
                break;
            }
            case 10:
            {
                while ((*(tmp + j) != ',') && (*(tmp + j) != '\0') && (j < sizeof(((struct s_respond_pack *)out_buf)->business_cycle)))
                {
                    ((struct s_respond_pack *)out_buf)->business_cycle[j] = *(tmp + j);
                    j++;
                }
                break;
            }  
        }
        j = 0;
    }
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 对响应的数据解包，判断数据是否正确
 */
static int request_data_pack(char **send_pack, char (*columns_value)[100])
{
    int count = 0;
    unsigned int buf_len = 0;
    struct s_request_pack request_pack;
    memset((void *)&request_pack, 0, sizeof(struct s_request_pack));
    
    #if ENDIAN == 0
    request_pack.message_type = 1;
    #else
    request_pack.message_type = DATA_ENDIAN_CHANGE_SHORT(1);
    #endif
    strcpy(request_pack.version, "0101");
    set_function_id(request_pack.func_id);
    common_tools.set_start_time(request_pack.start_time);
    
    common_tools.mac_add_horizontal(columns_value[1]);
    common_tools.mac_add_horizontal(columns_value[3]);
    sprintf(request_pack.pad_id, "padId:%s,", columns_value[0]);
    sprintf(request_pack.pad_mac, "padMac:%s,", columns_value[1]);
    sprintf(request_pack.base_id, "baseId:%s,", columns_value[2]);    
    sprintf(request_pack.base_mac, "baseMac:%s,", columns_value[3]);    
    sprintf(request_pack.token, "token:%s", columns_value[4]);
    
    request_pack.total_len = sizeof(request_pack.total_len) + sizeof(request_pack.message_type)
            + strlen(request_pack.version) + strlen(request_pack.func_id) + strlen(request_pack.start_time) 
            + strlen(request_pack.base_id) + strlen(request_pack.base_mac) 
            + strlen(request_pack.pad_id) + strlen(request_pack.pad_mac) + strlen(request_pack.token); 
    
    // 动态申请
    if ((*send_pack = (char *)malloc(request_pack.total_len + 1)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
        return MALLOC_ERR;
    }
    memset(*send_pack, 0, request_pack.total_len + 1);
    
    #if ENDIAN == 0
    memcpy(*send_pack, &request_pack.total_len, sizeof(unsigned int));
    #else
    buf_len = DATA_ENDIAN_CHANGE_LONG(request_pack.total_len);
    memcpy(*send_pack, &buf_len, sizeof(unsigned int));
    #endif
    count += sizeof(request_pack.total_len);
    
    (*send_pack)[4] = (char)request_pack.message_type;
    (*send_pack)[5] = (char)(request_pack.message_type >> 8);
    count += sizeof(request_pack.message_type);
    
    common_tools.memncat(*send_pack, request_pack.version, count, strlen(request_pack.version));
    count += strlen(request_pack.version);
    
    common_tools.memncat(*send_pack, request_pack.func_id, count, strlen(request_pack.func_id));
    count += strlen(request_pack.func_id);
    
    common_tools.memncat(*send_pack, request_pack.start_time, count, strlen(request_pack.start_time));
    count += strlen(request_pack.start_time);
    
    common_tools.memncat(*send_pack, request_pack.base_id, count, strlen(request_pack.base_id));
    count += strlen(request_pack.base_id);
    
    common_tools.memncat(*send_pack, request_pack.base_mac, count, strlen(request_pack.base_mac));
    count += strlen(request_pack.base_mac);
    
    common_tools.memncat(*send_pack, request_pack.pad_id, count, strlen(request_pack.pad_id));
    count += strlen(request_pack.pad_id);
    
    common_tools.memncat(*send_pack, request_pack.pad_mac, count, strlen(request_pack.pad_mac));
    count += strlen(request_pack.pad_mac);
    
    common_tools.memncat(*send_pack, request_pack.token, count, strlen(request_pack.token));
    count += strlen(request_pack.token);
    return count;
}

/**
 * 获取base板卡的序列号和mac（/proc/cmdline文件）
 */
static int get_base_sn_and_mac(char *base_sn, char *base_mac)
{
    #if 0
    strcpy(base_sn, "01A1010100100312122001B85AFEFFF00D");
    strcpy(base_mac, "B85AFEFFF00D");
    return 0;
    #endif
    
    PRINT_STEP("entry...\n");
    int res = 0;
    int i = 0;
    int j = 0;
    struct stat file_stat;
    memset(&file_stat, 0, sizeof(struct stat));
    char cmd_buf[256] = {0};
    char mac_tmp[18] = {0};
    char *tmp = NULL;;
    
    FILE *fp = NULL;
    if ((fp = fopen("/proc/cmdline", "r")) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "fopen failed!", OPEN_ERR);
        return OPEN_ERR;
    }
    
    if (fgets(cmd_buf, sizeof(cmd_buf), fp) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "fgets failed!", READ_ERR);
        fclose(fp);
        return READ_ERR;
    }
    fclose(fp);
    #if 0
    tmp = strstr(cmd_buf, "sn=");   
    if (tmp == NULL)
	{
	    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "base sn not find!", DATA_ERR);
        return DATA_ERR;
	}
	
    
	memcpy(base_sn, tmp + 3, 34);
    
    tmp = strstr(cmd_buf, "mac=");    
    if (tmp == NULL)
	{
	    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "base mac not find!", DATA_ERR);
        return DATA_ERR;
	}
	memcpy(mac_tmp, tmp + 4, 17);   
	
    for (i = 0; i < 17; i++)
    {
        if (mac_tmp[i] != ':')
        {
            base_mac[j++] = mac_tmp[i]; 
        }
    }
    PRINT("base_mac = %s\n", base_mac);
    #else
    
    tmp = strstr(cmd_buf, "SN=");   
    if (tmp == NULL)
	{
	    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "base sn not find!", DATA_ERR);
        return DATA_ERR;
	}
    memcpy(base_sn, tmp + 3, 22);
    
    if ((res = common_tools.get_wan_mac(mac_tmp)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_wan_mac failed!", res);
        return res;
    }
    
    common_tools.mac_del_colon(mac_tmp);
    
    strncat(base_sn, mac_tmp, strlen(mac_tmp));
    memcpy(base_mac, mac_tmp, strlen(mac_tmp));
    #endif
    
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 获取base板卡的ip
 */
static int get_base_ip(char *base_ip)
{
    PRINT_STEP("entry...\n");
	char base_ip_tmp[20] = {0};
    FILE *eth0_fp = fopen("/etc/eth0-setting", "r");
    if (eth0_fp == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "open failed!", OPEN_ERR);
        return OPEN_ERR;
	}
	fgets(base_ip_tmp, sizeof(base_ip_tmp), eth0_fp);
	fclose(eth0_fp);
	base_ip_tmp[strlen(base_ip_tmp) - 1] = '\0';
    sprintf(base_ip, "baseIP:%s", (base_ip_tmp + 3));
    PRINT_STEP("exit...\n");
	return 0;
}

static int set_function_id(char *func_id)
{
    strcpy(func_id, "functionId:0001_0001_0001,");
    return 0;
}

/**
 * 获取sip服务器账号等信息
 */
static int get_sip_info(char *pad_sn, char *pad_mac)
{
    PRINT_STEP("entry...\n");
    int res = 0;
    int fd = 0; 
    char *buf = NULL;
    struct timeval tv;
    unsigned int buf_len = 0;
    char columns_name[5][30] = {"pad_sn", "pad_mac", "base_sn", "base_mac", "device_token"};
    char columns_value[5][100] = {0};
    memset(&respond_pack, 0, sizeof(struct s_respond_pack));
    
    if ((pad_sn == NULL) && (pad_mac == NULL))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "pad_sn or pad_mac is NULL!", NULL_ERR);
        return NULL_ERR;
    }       
    
    #if BOARDTYPE == 6410 || BOARDTYPE == 9344
    // 数据库查询数据
    if ((res = database_management.select(5, columns_name, columns_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_select failed!", res);
        return res;
    }
    #elif BOARDTYPE == 5350
    
    // 数据库查询数据
    if ((res = nvram_interface.select(RT5350_FREE_SPACE, 5, columns_name, columns_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram__select failed!", res);
        return res;
    }
    int i = 0;
    for (i = 0; i < 5; i++)
    {
        if ((strcmp("\"\"", columns_value[i]) == 0) || (strlen(columns_value[i]) == 0))
        {
            memset(columns_value[i], 0, sizeof(columns_value[i]));
            sprintf(columns_value[i], "There is no (%s) record!", columns_name[i]);
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, columns_value[i], NULL_ERR); 
            return NULL_ERR;
        }
    }
    #endif
    PRINT("sqlite pad_sn = %s\n", columns_value[0]);
    PRINT("sqlite pad_mac = %s\n", columns_value[1]);
    PRINT("pad_sn = %s\n", pad_sn);
    PRINT("pad_mac = %s\n", pad_mac);
      
    // 对比数据库中的数据和形参
    if ((strcmp(columns_value[0], pad_sn) != 0) || (strcmp(columns_value[1], pad_mac) != 0))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "the data is different!", P_DATA_ERR);
        return P_DATA_ERR;
    }
    
    // 数据打包
    if ((buf_len = request_data_pack(&buf, columns_value)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "request_data_pack failed!", buf_len);
        if (buf != NULL)
        {
            free(buf);
            buf = NULL;
        }
        return buf_len;
    }

    PRINT_BUF_BY_HEX(buf, NULL, buf_len, __FILE__, __FUNCTION__, __LINE__);
    PRINT("[%s]\n", buf + 6);
    
    // 链接服务器
	PRINT("common_tools.config->terminal_server_ip = %s, common_tools.config->terminal_server_port = %s\n", common_tools.config->terminal_server_ip, common_tools.config->terminal_server_port);
    if ((fd =  internetwork_communication.make_client_link(common_tools.config->terminal_server_ip, common_tools.config->terminal_server_port)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "tcp_client_link failed!", fd);
        free(buf);
        buf = NULL;
        return common_tools.get_errno('S', fd);
    }
    tv.tv_sec = common_tools.config->total_timeout;
    // 发送数据
    if ((res = common_tools.send_data(fd, buf, NULL, buf_len, &tv)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        close(fd);
        free(buf);
        buf = NULL;
        return common_tools.get_errno('S', res);
    }
    free(buf);
    buf = NULL;
    PRINT("recv data start!\n");
    
    // 接收数据 长度
    if ((res = common_tools.recv_data(fd, (char *)&buf_len, NULL, sizeof(buf_len), &tv)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_data failed!", res);
        close(fd);
        return common_tools.get_errno('S', res);
    }
    
    #if ENDIAN == 1
    buf_len = DATA_ENDIAN_CHANGE_SHORT(buf_len);
    #endif
    
    if ((buf = (char *)malloc(buf_len - 3)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", res);
        close(fd);	
        return MALLOC_ERR;
    }
    memset(buf, 0, buf_len - 3);
    
    // 接收数据 
    if ((res = common_tools.recv_data(fd, buf, NULL, buf_len - 4, &tv)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_data failed!", res);
        close(fd);
        free(buf);
        buf = NULL;
    	return common_tools.get_errno('S', res);
    }
    close(fd);
    printf("\n");
    PRINT("recv data end!\n");
    PRINT("[%s]\n", buf + 2);
    // 数据解包
    if ((res = respond_data_unpack((void *)buf, (void *)&respond_pack, buf_len - 4)) < 0)
    {
    	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "respond_data_unpack failed!", res);
    	free(buf);
        buf = NULL;
    	return res;
    }
    
    PRINT("respond_data_unpack success!\n");
    PRINT("[%s]\n", buf + 2);
    free(buf);
    buf = NULL;
    
    PRINT("respond_pack.total_len = %d\n", buf_len);
    PRINT("respond_pack.message_type = %d\n", respond_pack.message_type);
    PRINT("respond_pack.version = %s\n", respond_pack.version); 
        
    PRINT("respond_pack.msg_data_status = %s\n", respond_pack.msg_data_status);
    PRINT("respond_pack.s_data_process_time = %s\n", respond_pack.s_data_process_time);
    PRINT("respond_pack.start_time = %s\n", respond_pack.start_time);
              
    unsigned short index = 0;
    index = common_tools.str_in_str((void *)respond_pack.msg_data_status, strlen(respond_pack.msg_data_status), ':', 1);
    char msg_data_status_num[4] = {0};
    memcpy(msg_data_status_num, respond_pack.msg_data_status + index + 1, 3);
    
    // 200 成功           
    if (strcmp(msg_data_status_num, "200") == 0)
    {
        PRINT("respond_pack.base_user_name = %s\n", respond_pack.base_user_name);
        PRINT("respond_pack.base_password = %s\n", respond_pack.base_password);
        PRINT("respond_pack.pad_user_name = %s\n", respond_pack.pad_user_name);
        PRINT("respond_pack.pad_password = %s\n", respond_pack.pad_password);
        PRINT("respond_pack.sip_ip_address = %s\n", respond_pack.sip_ip_address);
        PRINT("respond_pack.sip_port = %s\n", respond_pack.sip_port);
        PRINT("respond_pack.heart_beat_cycle = %s\n", respond_pack.heart_beat_cycle);
        PRINT("respond_pack.business_cycle = %s\n", respond_pack.business_cycle);
            
        char columns_name[8][30] = {"pad_user_name", "pad_password", "base_user_name", "base_password", "sip_ip", "sip_port", "heart_beat_cycle", "business_cycle"};
        char columns_value[8][100] = {0};
        unsigned short columns_value_len[8] = {0};
        memcpy(columns_value[0], (respond_pack.pad_user_name + strlen("padUserName:")), (strlen(respond_pack.pad_user_name) - strlen("padUserName:")));
        columns_value_len[0] = strlen(respond_pack.pad_user_name) - strlen("padUserName:");
            
        memcpy(columns_value[1], (respond_pack.pad_password + strlen("padPassword:")), (strlen(respond_pack.pad_password) - strlen("padPassword:")));
        columns_value_len[1] = strlen(respond_pack.pad_password) - strlen("padPassword:");
            
        memcpy(columns_value[2], (respond_pack.base_user_name + strlen("baseUserName:")), (strlen(respond_pack.base_user_name) - strlen("baseUserName:")));
        columns_value_len[2] = strlen(respond_pack.base_user_name) - strlen("baseUserName:");
        
        memcpy(columns_value[3], (respond_pack.base_password + strlen("basePassword:")), (strlen(respond_pack.base_password) - strlen("basePassword:")));
        columns_value_len[3] = strlen(respond_pack.base_password) - strlen("basePassword:");
        
        memcpy(columns_value[4], (respond_pack.sip_ip_address + strlen("sipIpAddress:")), (strlen(respond_pack.sip_ip_address) - strlen("sipIpAddress:")));
        columns_value_len[4] = strlen(respond_pack.sip_ip_address) - strlen("sipIpAddress:");
        
        memcpy(columns_value[5], (respond_pack.sip_port + strlen("port:")), (strlen(respond_pack.sip_port) - strlen("port:")));
        columns_value_len[5] = strlen(respond_pack.sip_port) - strlen("port:");
            
        memcpy(columns_value[6], (respond_pack.heart_beat_cycle + strlen("heartbeatCycle:")), (strlen(respond_pack.heart_beat_cycle) - strlen("heartbeatCycle:")));
        columns_value_len[6] = strlen(respond_pack.heart_beat_cycle) - strlen("heartbeatCycle:");
            
        memcpy(columns_value[7], (respond_pack.business_cycle + strlen("businessCycle:")), (strlen(respond_pack.business_cycle) - strlen("businessCycle:")));
        columns_value_len[7] = strlen(respond_pack.business_cycle) - strlen("businessCycle:");
        
        #if BOARDTYPE == 6410 || BOARDTYPE == 9344
        if ((res = database_management.insert(8, columns_name, columns_value, columns_value_len)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "sqlite3_insert failed!", res);
            return res;
        }
        #elif BOARDTYPE == 5350
        PRINT("before insert!\n");
        if ((res = nvram_interface.insert(RT5350_FREE_SPACE, 8, columns_name, columns_value, columns_value_len)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "nvram_insert failed!", res);
            return res;
        }
        PRINT("after insert!\n");
        #endif
    }
    else if (strcmp(msg_data_status_num, "401") == 0) // functionId找不到
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "functionId not find!", SERVER_ERR);
        return SERVER_ERR;
    }
    else if (strcmp(msg_data_status_num, "404") == 0) // 注册失败
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "register failed!", SERVER_ERR);
        return SERVER_ERR;
    }
    else if (strcmp(msg_data_status_num, "405") == 0) // 没有发现响应终端信息
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "respond info not find!", SERVER_ERR);
        return SERVER_ERR;
    }
    else if (strcmp(msg_data_status_num, "406") == 0) // 令牌无效
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "token is error!", SERVER_ERR);
        return SERVER_ERR;
    }
    else if (strcmp(msg_data_status_num, "500") == 0) // 服务端内部错误
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "service error!", SERVER_ERR);
        return SERVER_ERR;
    }
    else // 未知的错误
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "unknown error!", SERVER_ERR);
        return (0 - atoi(msg_data_status_num));
    }
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 注册：包括终端认证和获取sip服务器账号等信息
 */
static int user_register(char *pad_sn, char *pad_mac)
{
    PRINT_STEP("entry...\n");
    int res = 0;
    #if 0
    if ((res = common_tools.get_phone_stat()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_phone_stat failed!", res);
        return res;
    }
    #endif
    #if CTSI_SECURITY_SCHEME == 1
    if ((res = terminal_authentication.insert_token()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "insert_token failed!", res);
        return res;
    }
    
    if ((res = get_sip_info(pad_sn, pad_mac)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_sip_info failed!", res);
        return res;
    }
    
    #else
    char device_token[100] = {0};
    //char position_token[100] = {0};
    if ((res = terminal_authentication.rebuild_device_token(device_token)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "rebuild_device_token failed!", res);
        return res;
    }
    
    if ((res = get_sip_info(pad_sn, pad_mac)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_sip_info failed!", res);
        return res;
    }
    
	/*
    if ((res = terminal_authentication.rebuild_position_token(position_token)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "rebuild_position_token failed!", res);
        return res;
    }
	*/
    #endif
    
    PRINT_STEP("exit...\n");
    return 0;
}
