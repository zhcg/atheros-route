 #include "common_tools.h"

static struct s_deal_attr g_deal_attr;
static struct s_config g_config;
static struct s_work_sum g_work_sum;
static struct s_timer g_timer;
static char __datetime_buf[128];
static unsigned char phone_status_flag;     // 电话线路状态 默认为0，0表示未摘机，1表示已摘机

/**
 * 获取比较大的数字
 */
static int get_large_number(int num1, int num2);

/**
 * 读取配置文件，并设置全局变量
 */
static int get_config();

/**
 * 设置GPIO4
 */
static int set_GPIO4(int cmd);

/**
 * 制作菜单
 */
static int make_menulist(time_t start_time, struct timeval start, struct timeval end);


/**
 * 获取电话线状态
 */
static int get_phone_stat();

/**
 * 查询网络状况
 */
static int get_network_state(char *ip, unsigned char ping_count, unsigned char repeat);

/**
 * 获取用户提示
 */
static int get_user_prompt(int error_num, char **out_buf);

/**
 * 获取错误码
 */
static int get_errno(char role, int error_num);

/**
 * 获取链表缓冲区的加密域
 */
static int get_list_MAC(struct s_data_list *a_data_list, unsigned short start_index, unsigned short end_index, char *MAC);

/**
 * 获取缓冲区的加密域
 */
static int get_MAC(int src, char *MAC);

/**
 * 获取WAN口的MAC地址
 */
static int get_wan_mac(char *mac);

/****************************************************************************************/
/**
 * 把BCD码转换成字符串
 */
static int BCD_to_string(char* p_src, int src_size, char* p_dst, int dst_size);

/**
 * long转换为str
 */
static int long_to_str(unsigned long src, char *dst, unsigned char dst_len);

/**
 * 在src找dest
 */
static int str_in_str(const void *src, unsigned short src_len, const char dest, unsigned short dest_len);

/**
 * 拷贝src的len个字节到dest的index后面 
 * dest:  目标指针 
 * src:   源指针
 * index：dest索引
 * len:   长度
 */ 
static void *memncat(void *dest, const void *src, const int index, const int len);

/**
 * 去除字符串中的空格
 */
static int trim(char *str); 

/**
 * 字符串匹配
 */
static int match_str(const char *rule, const char *str);

/**
 * 对mac地址加上横线
 */
void mac_add_horizontal(char *mac);

/**
 * 对mac地址加上冒号
 */
static void mac_add_colon(char *mac);

/**
 * 对mac地址去掉冒号
 */
static void mac_del_colon(char *mac);

/**
 * 转换成正常格式MAC地址 如 00:AA:11:BB:CC:DD
 */
static int mac_format_conversion(char *mac);

/**
 * 对数据加引号
 */
static int add_quotation_marks(char *data);

/**
 * 对数据去除引号
 */
static int del_quotation_marks(char *data);

/****************************************************************************************/
/**
 * 获取日期和时间字符串
 */
static char *get_datetime_buf();

/**
 * 获取服务启动的总时间
 */
static int get_service_use_time(time_t start_time, char *service_use_time);

/**
 * 获取本次交易所用的总时间
 */
static double get_use_time(struct timeval start, struct timeval end);

/**
 * 超时判断
 */
static int time_out(struct timeval *start, struct timeval *end);

/**
 * 获取开始时间
 */
static int set_start_time(char *start_time);

/****************************************************************************************/

/**
 * 接收一个字节数据
 */
static int recv_one_byte(int fd, char *data, struct timeval *tv);

/**
 * 接收一个buf
 */
static int recv_data(unsigned int fd, unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv);

/**
 * 数据头接收处理
 */
static int recv_data_head(int fd, char *head, unsigned char head_len, struct timeval *tv);

/**
 * 发送一个字节
 */
static int send_one_byte(int fd, char *data, struct timeval *tv);

/**
 * 发送一个buf
 */
static int send_data(unsigned int fd, unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv);

/****************************************************************************************/

/**
 * 用于在链表的尾部添加数据
 */
static int list_tail_add_data(struct s_data_list *a_data_list, char data);

/**
 * 用于在链表的头部删除数据
 */
static int list_head_del_data(struct s_data_list *a_data_list, char data);

/**
 * 用于在链表的尾部添加数据
 */
static int list_free(struct s_data_list *a_data_list);

/**
 * 用于在链表获得链表中的元素，下标从0开始
 */
static int list_get_data(struct s_data_list *a_data_list, unsigned short index, unsigned short count, char *list_buf);

/****************************************************************************************/

/**
 * 获得命令输出
 */
static int get_cmd_out(char *cmd, char *buf, unsigned short buf_len);

/**
 * 串口初始化设置
 */
static int serial_init(const int fd, int baud);

/**
 * 获取缓冲区的校验和
 */
static int get_checkbit(char *buf, struct s_data_list *data_list, unsigned short start_index, unsigned short len, unsigned char check_type, unsigned char bytes);

/**
 * 打印 
 */ 
static int print_buf_by_hex(const void *buf, struct s_data_list *data_list, const unsigned long len, const char *file_name, const char *function_name, const unsigned short line_num);

/**
 * 获取随机字符串
 */
static int get_rand_string(unsigned char english_char_len, unsigned char chinese_char_len, char *out, unsigned char chinese_char_type);

/**
 * 启动指定目录的应用
 */
static int start_up_application(const char* path, char * const argv[], unsigned char flag);
/****************************************************************************************/

// 初始化结构体
struct class_common_tools common_tools = 
{
    &g_deal_attr, 
    &g_config, 
    &g_work_sum,
    &g_timer,
    &phone_status_flag,
    {0},
    get_large_number,
    get_config, set_GPIO4, make_menulist,
    get_network_state, get_user_prompt, get_errno, get_list_MAC, get_MAC, get_wan_mac,
    BCD_to_string,
    long_to_str, str_in_str, memncat, trim, match_str, mac_add_horizontal, mac_add_colon, mac_del_colon, mac_format_conversion, add_quotation_marks,
    del_quotation_marks, get_datetime_buf, get_use_time, set_start_time,
    recv_one_byte, recv_data, recv_data_head, send_one_byte, send_data, list_tail_add_data, list_head_del_data,
    list_free, list_get_data, get_cmd_out, serial_init, get_checkbit, print_buf_by_hex, get_rand_string, start_up_application
};

/*******************************************************************************/
/************************      以上为声明，以下为实现       ********************/
/*******************************************************************************/

/**
 * 获取比较大的数字
 */
int get_large_number(int num1, int num2)
{
    return ((num1 > num2) ? num1 : num2);
}
/**
 * 读取配置文件，并设置全局变量
 */
int get_config()
{
    PRINT_STEP("entry...\n");
    FILE *fp = NULL;
    int res = 0;
    int i = 0, j = 0, k = 0;
    struct stat config_stat;
    char *tmp;
    char buf[64] = {0}; 
    char print_buf[64] = {0};
    
    char config_options[][25] = 
    {
        "STEP_LOG", 
        "DB", 
        
        "CENTERPHONE", "CENTERIP", "CENTERPORT", 
        "BASE_IP", "PAD_IP", "PAD_SERVER_PORT", "PAD_SERVER_PORT2", "PAD_CLIENT_PORT", 
        "TOTALTIMEOUT", "ONE_BYTE_TIMEOUT_SEC", "ONE_BYTE_TIMEOUT_USEC", 
        
        "REPEAT", "TERMINAL_SERVER_IP", "TERMINAL_SERVER_PORT", "LOCAL_SERVER_PORT", "WAN_CHECK_NAME",
    };
    
    if ((fp = fopen(AUTHENTICATION_CONFIG, "r")) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "fopen failed!", res);
        return OPEN_ERR;
    }
    
    
    while (k != sizeof(config_options)/sizeof(config_options[0]))
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
        if ((res = trim(buf)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "trim failed!", res);
            return res;
        }
        
        for (i = 0; i < sizeof(config_options)/sizeof(config_options[0]); i++)
        {
            if ((tmp = strstr(buf, config_options[i])) != NULL)
            {
                if (*(tmp + strlen(config_options[i])) != '=')
                {
                    continue;
                }    
                j = 0;
                switch (i)
                {
                    case STEP_LOG:
                    {
                        for (j = 0; j < sizeof(g_config.step_log); j++)
                        {
                            if (*(tmp + strlen(config_options[i]) + 1 + j) == '\n')
                            {
                                break;
                            }
                            g_config.step_log[j] = *(tmp + strlen(config_options[i]) + j + 1);
                        }
                        memset(print_buf, 0, sizeof(print_buf));
                        sprintf(print_buf, "step_log = %s", g_config.step_log);
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                        k++;
                        break;
                    }
                    case DB:
                    {
                        for (j = 0; j < sizeof(g_config.db); j++)
                        {
                            if (*(tmp + strlen(config_options[i]) + 1 + j) == '\n')
                            {
                                break;
                            }
                            g_config.db[j] = *(tmp + strlen(config_options[i]) + j + 1);
                        }
                        memset(print_buf, 0, sizeof(print_buf));
                        sprintf(print_buf, "db = %s", g_config.db);
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                        k++;
                        break;
                    }
                    case CENTERPHONE:
                    {
                        for (j = 0; j < sizeof(g_config.center_phone); j++)
                        {
                            if (*(tmp + strlen(config_options[i]) + 1 + j) == '\n')
                            {
                                break;
                            }
                            g_config.center_phone[j] = *(tmp + strlen(config_options[i]) + j + 1);
                        }
                        memset(print_buf, 0, sizeof(print_buf));
                        sprintf(print_buf, "center_phone = %s", g_config.center_phone);
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                        k++;
                        break;
                    }       
                    case CENTERIP:
                    {
                        for (j = 0; j < sizeof(g_config.center_ip); j++)
                        {
                            if (*(tmp + strlen(config_options[i]) + 1 + j) == '\n')
                            {
                                break;
                            }
                            g_config.center_ip[j] = *(tmp + strlen(config_options[i]) + j + 1);
                        }
                        memset(print_buf, 0, sizeof(print_buf));
                        sprintf(print_buf, "center_ip = %s", g_config.center_ip);
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                        k++;
                        break;
                    }    
                    case CENTERPORT:
                    {
                        for (j = 0; j < sizeof(g_config.center_port); j++)
                        {
                            if (*(tmp + strlen(config_options[i]) + 1 + j) == '\n')
                            {
                                break;
                            }
                            g_config.center_port[j] = *(tmp + strlen(config_options[i]) + j + 1);
                        }
                        memset(print_buf, 0, sizeof(print_buf));
                        sprintf(print_buf, "center_port = %s", g_config.center_port);
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                        k++;
                        break;
                    }
                    case BASE_IP:
                    {
                        for (j = 0; j < sizeof(g_config.base_ip); j++)
                        {
                            if (*(tmp + strlen(config_options[i]) + 1 + j) == '\n')
                            {
                                break;
                            }
                            g_config.base_ip[j] = *(tmp + strlen(config_options[i]) + j + 1);
                        }
                        memset(print_buf, 0, sizeof(print_buf));
                        sprintf(print_buf, "base_ip = %s", g_config.base_ip);
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                        k++;
                        break;
                    }
                    case PAD_IP:
                    {
                        for (j = 0; j < sizeof(g_config.pad_ip); j++)
                        {
                            if (*(tmp + strlen(config_options[i]) + 1 + j) == '\n')
                            {
                                break;
                            }
                            g_config.pad_ip[j] = *(tmp + strlen(config_options[i]) + j + 1);
                        }
                        memset(print_buf, 0, sizeof(print_buf));
                        sprintf(print_buf, "pad_ip = %s", g_config.pad_ip);
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                        k++;
                        break;
                    }
                    case PAD_SERVER_PORT:
                    {
                        for (j = 0; j < sizeof(g_config.pad_server_port); j++)
                        {
                            if (*(tmp + strlen(config_options[i]) + 1 + j) == '\n')
                            {
                                break;
                            }
                            g_config.pad_server_port[j] = *(tmp + strlen(config_options[i]) + j + 1);
                        }
                        memset(print_buf, 0, sizeof(print_buf));
                        sprintf(print_buf, "pad_server_port = %s", g_config.pad_server_port);
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                        k++;
                        break;
                    }
                    case PAD_SERVER_PORT2:
                    {
                        for (j = 0; j < sizeof(g_config.pad_server_port2); j++)
                        {
                            if (*(tmp + strlen(config_options[i]) + 1 + j) == '\n')
                            {
                                break;
                            }
                            g_config.pad_server_port2[j] = *(tmp + strlen(config_options[i]) + j + 1);
                        }
                        memset(print_buf, 0, sizeof(print_buf));
                        sprintf(print_buf, "pad_server_port2 = %s", g_config.pad_server_port2);
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                        k++;
                        break;
                    }
                    case PAD_CLIENT_PORT:
                    {
                        for (j = 0; j < sizeof(g_config.pad_client_port); j++)
                        {
                            if (*(tmp + strlen(config_options[i]) + 1 + j) == '\n')
                            {
                                break;
                            }
                            g_config.pad_client_port[j] = *(tmp + strlen(config_options[i]) + j + 1);
                        }
                        memset(print_buf, 0, sizeof(print_buf));
                        sprintf(print_buf, "pad_client_port = %s", g_config.pad_client_port);
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                        k++;
                        break;
                    }
                    case TOTAL_TIME_OUT:
                    {
                        char total_timeout[12] = {0};
                        for (j = 0; j < sizeof(total_timeout); j++)
                        {
                            if (*(tmp + strlen(config_options[i]) + 1 + j) == '\n')
                            {
                                break;
                            }
                            total_timeout[j] = *(tmp + strlen(config_options[i]) + j + 1);
                        }
                        g_config.total_timeout = atoi(total_timeout);
                        memset(print_buf, 0, sizeof(print_buf));
                        sprintf(print_buf, "total_timeout = %d", g_config.total_timeout);
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                        k++;
                        break;
                    }
                    case ONE_BYTE_TIMEOUT_SEC:
                    {
                        char one_byte_timeout_sec[12] = {0};
                        for (j = 0; j < sizeof(one_byte_timeout_sec); j++)
                        {
                            if (*(tmp + strlen(config_options[i]) + 1 + j) == '\n')
                            {
                                break;
                            }
                            one_byte_timeout_sec[j] = *(tmp + strlen(config_options[i]) + j + 1);
                        }
                        g_config.one_byte_timeout_sec = atoi(one_byte_timeout_sec);
                        memset(print_buf, 0, sizeof(print_buf));
                        sprintf(print_buf, "one_byte_timeout_sec = %d", g_config.one_byte_timeout_sec);
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                        k++;
                        break;
                    }
                    case ONE_BYTE_TIMEOUT_USEC:
                    {
                        char one_byte_timeout_usec[12] = {0};
                        for (j = 0; j < sizeof(one_byte_timeout_usec); j++)
                        {
                            if (*(tmp + strlen(config_options[i]) + 1 + j) == '\n')
                            {
                                break;
                            }
                            one_byte_timeout_usec[j] = *(tmp + strlen(config_options[i]) + j + 1);
                        }
                        g_config.one_byte_timeout_usec = atoi(one_byte_timeout_usec);
                        memset(print_buf, 0, sizeof(print_buf));
                        sprintf(print_buf, "one_byte_timeout_usec = %d", g_config.one_byte_timeout_usec);
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                        k++;
                        break;
                    }         
                    case REPEAT:
                    {
                        char repeat[12] = {0};
                        for (j = 0; j < sizeof(repeat); j++)
                        {
                            if (*(tmp + strlen(config_options[i]) + 1 + j) == '\n')
                            {
                                break;
                            }
                            repeat[j] = *(tmp + strlen(config_options[i]) + j + 1);
                        }
                        g_config.repeat = atoi(repeat);
                        memset(print_buf, 0, sizeof(print_buf));
                        sprintf(print_buf, "repeat = %d", g_config.repeat);
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                        k++;
                        break;
                    }                
                    case TERMINAL_SERVER_IP:
                    {
                        for (j = 0; j < sizeof(g_config.terminal_server_ip); j++)
                        {
                            if (*(tmp + strlen(config_options[i]) + 1 + j) == '\n')
                            {
                                break;
                            }
                            g_config.terminal_server_ip[j] = *(tmp + strlen(config_options[i]) + j + 1);
                        }
                        memset(print_buf, 0, sizeof(print_buf));
                        sprintf(print_buf, "terminal_server_ip = %s", g_config.terminal_server_ip);
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                        k++;
                        break;
                    }    
                    case TERMINAL_SERVER_PORT:
                    {
                        for (j = 0; j < sizeof(g_config.terminal_server_port); j++)
                        {
                            if (*(tmp + strlen(config_options[i]) + 1 + j) == '\n')
                            {
                                break;
                            }
                            g_config.terminal_server_port[j] = *(tmp + strlen(config_options[i]) + j + 1);
                        }
                        memset(print_buf, 0, sizeof(print_buf));
                        sprintf(print_buf, "terminal_server_port = %s", g_config.terminal_server_port);
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                        k++;
                        break;
                    }
                    case LOCAL_SERVER_PORT:
                    {
                        for (j = 0; j < sizeof(g_config.local_server_port); j++)
                        {
                            if (*(tmp + strlen(config_options[i]) + 1 + j) == '\n')
                            {
                                break;
                            }
                            g_config.local_server_port[j] = *(tmp + strlen(config_options[i]) + j + 1);
                        }
                        memset(print_buf, 0, sizeof(print_buf));
                        sprintf(print_buf, "local_server_port = %s", g_config.local_server_port);
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                        k++;
                        break;
                    }
                    case WAN_CHECK_NAME:
                    {
                        for (j = 0; j < sizeof(g_config.wan_check_name); j++)
                        {
                            if (*(tmp + strlen(config_options[i]) + 1 + j) == '\n')
                            {
                                break;
                            }
                            g_config.wan_check_name[j] = *(tmp + strlen(config_options[i]) + j + 1);
                        }
                        memset(print_buf, 0, sizeof(print_buf));
                        sprintf(print_buf, "wan_check_name = %s", g_config.wan_check_name);
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                        k++;
                        break;
                    }
                    default:
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "config_options error!", MISMATCH_ERR);
                        continue;
                    }
                }
                break; 
            }
        }
    }
    fclose(fp);
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 设置GPIO4
 */
int set_GPIO4(int cmd)
{
    PRINT_STEP("entry...\n");
    int fd = 0;
    int res = 0;
    
    if ((fd = open("/dev/2438", O_RDWR, 0644)) < 0)
    //if ((fd = open("/dev/2438", 0)) < 0)    
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "open failed!", OPEN_ERR);
        return OPEN_ERR;
    }
    
    if ((res = ioctl(fd, cmd)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "ioctl failed!", res);
        return IOCTL_ERR;
    }
    close(fd);
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 输出菜单项
 */
int make_menulist(time_t start_time, struct timeval start, struct timeval end)
{
    PRINT_STEP("entry...\n");
    char print_info[256] = {0}; // 日志打印信息
    double use_time = 0.0;
    static unsigned long test_failed_count_tmp = 0l;
    static unsigned long test_failed_last = 0l;
    static unsigned long test_failed_frist = 0l;
    
    //OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "*********************************************", 0);
    //OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "******** TERMINAL AUTHENTICATION TEST *******", 0);
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "*********************************************", 0);
    get_service_use_time(start_time, print_info);
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_info, 0);
    
    use_time = get_use_time(start, end);
    sprintf(print_info, "test  %ld running time: %3.6lf", g_work_sum.work_sum, use_time);
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_info, 0);

    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "*********************************************", 0);
	
	sprintf(print_info, "test number         %10ld ", g_work_sum.work_sum);
	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_info, 0);
    
	sprintf(print_info, "test failed number  %10ld ", g_work_sum.work_failed_sum);
	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_info, 0);
    
	sprintf(print_info, "test success number %10ld ", g_work_sum.work_success_sum);
	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_info, 0);
    
    // 错误计数不等于零时
    if (g_work_sum.work_failed_sum != 0)
    {
        // 第一次错误
        if (test_failed_count_tmp == 0)
        {
            test_failed_count_tmp = 1;
            test_failed_frist = g_work_sum.work_sum;
            //DEAL_FAILED_LOG();
        }
        
    	sprintf(print_info, "frist failed number %10ld ", test_failed_frist);
    	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_info, 0);
        
        //if ((g_work_sum.work_failed_sum - test_failed_count_tmp) == 1)
        {
            test_failed_count_tmp++;
            test_failed_last = g_work_sum.work_sum;
            //DEAL_FAILED_LOG();
        }
        
    	sprintf(print_info, "last failed number  %10ld ", test_failed_last);
    	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_info, 0);
    }
	//if ((g_work_sum.work_sum - 1)> 0)
    {    
    	sprintf(print_info, "failed ratio  = %%%6.2lf", ((double)g_work_sum.work_failed_sum / (double)(g_work_sum.work_sum)) * 100);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_info, 0);
        
    	sprintf(print_info, "success ratio = %%%6.2lf", ((double)g_work_sum.work_success_sum / (double)(g_work_sum.work_sum)) * 100);
    	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_info, 0);
    }
    
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "*********************************************", 0);
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 制作运行日志
 */
int operation_steps_log(const char *file_name, const char *function_name, const unsigned short line_num, const char *log_info, short flag)
{
    PRINT_STEP("entry...\n");
    
    if ((file_name == NULL) || (file_name == NULL) || (log_info == NULL))
    {
        PERROR("buf is NULL!\n");
        return NULL_ERR;
    }
    // 
    #if LOG_IN_FILE_DEBUG
    struct s_record_info record_info;                 //创建日志时使用
    memset(&record_info, 0, sizeof(record_info));
	struct timeval tv;
	char record_name_time[20] = {0};
	char record_pack[5120] = {0};
	char dest[6] = {0};
    
	gettimeofday(&tv, NULL);
	strftime(record_info.date_and_time, sizeof(record_info.date_and_time), "[%Y-%m-%d][%T: ", localtime(&tv.tv_sec));    
	
    strcpy(record_info.file_name, file_name);
    strcpy(record_info.function_name, function_name);
    record_info.line_num = line_num;
    strcpy(record_info.print_info, log_info);
    
	make_data_middle(record_info.file_name, strlen(record_info.file_name), sizeof(record_info.file_name) - 1);
	make_data_middle(record_info.function_name, strlen(record_info.function_name), sizeof(record_info.function_name) - 1);
    //转换成字符串，再拼接
	char line_num_tmp[11] = {0};
	sprintf(line_num_tmp, "%d", record_info.line_num);
	make_data_middle(line_num_tmp, strlen(line_num_tmp), sizeof(line_num_tmp) - 1);
	
	sprintf(record_pack, "%s%6d][%s][%s][%s][%s]\n", record_info.date_and_time, tv.tv_usec, record_info.file_name, 
	        record_info.function_name, line_num_tmp, record_info.print_info);
	
    // 文件名
    sprintf(record_info.record_name, "%s%ld", __STEP_LOG, g_work_sum.work_sum);   
    int record_fd = 0;
	if (access(record_info.record_name, F_OK | W_OK) == -1)  // 文件不存在
    {
        if (record_fd > 0)
        {            
            close(record_fd);
        }
        char cmd[64] = {0}; 
        sprintf(cmd, "mkdir -p %s", __STEP_LOG);
        system(cmd);
    	if ((record_fd = open(record_info.record_name, O_WRONLY | O_APPEND | O_CREAT, 0644)) < 0)
        {
            PERROR("open failed!\n");
        	return OPEN_ERR;
        }
        
    	char record_head[158] = {0};
        
    	memset(record_head, '*', sizeof(record_head) - 1);
    	record_head[sizeof(record_head) - 1] = '\n';
    	write(record_fd, record_head, sizeof(record_head));
        
    	memset(record_head, 0, sizeof(record_head));
        
    	char record_head_date[11] = "DATE";
    	make_data_middle(record_head_date, strlen(record_head_date), sizeof(record_head_date) - 1);
    	char record_head_time[17] = "TIME";
    	make_data_middle(record_head_time, strlen(record_head_time), sizeof(record_head_time) - 1);
    	char record_head_file[33] = "FILE";
    	make_data_middle(record_head_file, strlen(record_head_file), sizeof(record_head_file) - 1);
    	char record_head_func[33] = "FUNCTION";
    	make_data_middle(record_head_func, strlen(record_head_func), sizeof(record_head_func) - 1);
    	char record_head_line[11] = " LINE_NUM ";
        
    	char record_head_print_info[46] = "PRINT_INFO";
    	make_data_middle(record_head_print_info, strlen(record_head_print_info), sizeof(record_head_print_info) - 1);

        sprintf(record_head, "*%s**%s**%s**%s**%s**%s*\n", record_head_date, record_head_time, record_head_file, 
                record_head_func, record_head_line, record_head_print_info);
    	write(record_fd, record_head, sizeof(record_head));
            
    	memset(record_head, '*', sizeof(record_head) - 1);
    	record_head[sizeof(record_head) - 1] = '\n';
    	write(record_fd, record_head, sizeof(record_head));
    }
    else
    {
        if ((record_fd = open(record_info.record_name, O_WRONLY | O_APPEND, 0644)) < 0)
        {
            PERROR("open failed!\n");
        	return OPEN_ERR;
        } 
    }
	if (write(record_fd, record_pack, strlen(record_pack)) < 0)
    {
        PERROR("write failed!\n");
        return WRITE_ERR;
    }
    close(record_fd);
    #else // 标准输出
    if (flag == 0)
    {
        printf("[%s]%s[%s][%s][%05d] %s\n", common_tools.argv0, common_tools.get_datetime_buf(), file_name, function_name, line_num, log_info);
    }
    else
    {
        printf("[%s]%s[%s][%s][%05d][errno = %d][%s] [%s]\n", common_tools.argv0, common_tools.get_datetime_buf(), file_name, function_name, line_num, errno, strerror(errno), log_info);
    }
    #endif
    
    PRINT_STEP("exit...\n");    
    return 0;
}

/**
 * 查询网络状况
 */
int get_network_state(char *ip, unsigned char ping_count, unsigned char repeat)
{
    int res = 0;
    int i = 0;
    char state[2] = {0};
    char *cmd_buf = NULL;
    
    if (ip == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "ip is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    if ((cmd_buf = (char *)malloc(strlen(ip) + 50)) == NULL)
	{
	    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
		return MALLOC_ERR;
	}
	memset(cmd_buf, 0, strlen(ip) + 50);
	//sprintf(cmd_buf, "ping -c %d -W 1 %s 1>/dev/null 2>/dev/null;echo $?", ping_count, ip);
	sprintf(cmd_buf, "ping %s test 1>/dev/null 2>/dev/null;echo $?", ip);
	PRINT("cmd_buf = %s\n", cmd_buf);
    for (i = 0; i < repeat; i++)
    {
        memset(state, 0, sizeof(state));
        if ((res = get_cmd_out(cmd_buf, state, sizeof(state))) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_cmd_out failed!", res);
            continue;
        }
        PRINT("state = %s\n", state);
        if (memcmp(state, "0", strlen("0")) != 0)
        {
            res = DATA_DIFF_ERR;
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data is different!", res);
            if (i != (repeat - 1))
            {
                sleep(3);
                continue;
            } 
        }
        break;      
    }
    // free
    free(cmd_buf);
    cmd_buf = NULL;
    
    if (res < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "network abnormal!", res);
        return res;
    }
    return 0;
}

/**
 * 获取用户提示
 */
int get_user_prompt(int error_num, char **out_buf)
{
    unsigned short state_num = 0;
    char prompt[64] = {0};
    unsigned char len = 0;
       
    switch (error_num)
    {
        case P_DATA_ERR:            // 数据内容错误
        case P_CHECK_DIFF_ERR:      // 校验不对错误
        case P_DATA_DIFF_ERR:       // 数据不相同错误
        case P_LENGTH_ERR:          // 数据长度错误
        case P_MISMATCH_ERR:        // 选项不匹配
        case REGEXEC_ERR:           // 匹配字符串
        {            
            state_num = 1;  //状态码
            memcpy(prompt, "请求方数据异常！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);    
            break;
        }
        case WRONGFUL_DEV_ERR:      // 不合法设备
        case LOGIN_FAILED:          // 登陆失败
        {            
            state_num = 2;  //状态码
            memcpy(prompt, "终端PAD和底座不匹配！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);    
            break;
        }
        case S_CONNECT_ERR:         // 连接错误 
        {            
            state_num = 3;  //状态码
            memcpy(prompt, "连接服务器失败！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case S_READ_ERR:            // 文件读取错误	
	    case S_SELECT_NULL_ERR:     // 轮询为空错误
        {            
            state_num = 4;  //状态码
            memcpy(prompt, "服务器无响应！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case LAN_ABNORMAL:          // 局域网不正常 
        {            
            state_num = 5;  //状态码
            memcpy(prompt, "局域网络不正常！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case WAN_ABNORMAL:          // 局域网不正常
        {            
            state_num = 6;  //状态码
            memcpy(prompt, "不正常的广域网络，请检查WAN口及网络参数！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case PPPOE_INVALID_INFO:
        {            
            state_num = 7;  //状态码
            memcpy(prompt, "PPPOE账号和密码无效！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case PPPOE_CALL_FAILED:
        {            
            state_num = 8;  //状态码
            memcpy(prompt, "PPPOE呼叫失败！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case S_MISMATCH_ERR:        // 选项不匹配
	    case CENTER_PHONE_ERR:      // 电话号码错误 	
	    case OVER_LEN_ERR:          // 自定义数据超长错误
	    case S_CHECK_DIFF_ERR:      // 校验不对错误
        case S_DATA_ERR:            // 数据内容错误
        case S_DATA_DIFF_ERR:       // 数据不相同错误
    	case DEAL_ERR_TYPE_ERR:     // 交易错误类型错误
    	case DEAL_TYPE_ERR:         // 交易类型错误
    	case S_LENGTH_ERR:          // 数据长度错误
        {            
            state_num = 9;  //状态码
            memcpy(prompt, "服务器数据异常！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
    	case SERVER_ERR:             // 服务器内部错误
        {            
            state_num = 10;  //状态码
            memcpy(prompt, "服务器内部错误！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case P_CONNECT_ERR:         // 连接错误
        {            
            state_num = 11;  //状态码
            memcpy(prompt, "终端PAD和底座连接建立失败！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case P_READ_ERR:            // 文件读取错误
        case P_SELECT_NULL_ERR:     // 轮询为空错误
        {            
            state_num = 12;  //状态码
            memcpy(prompt, "终端PAD无响应底座！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }    
        case ACCEPT_ERR:            // 接收连接错误
        case BIND_ERR:              // 绑定错误 
        case BCD_TO_STR_ERR:        // BCD码转字符串错误
        case CONNECT_ERR:           // 连接错误
        case L_DATA_ERR:            // 本地数据内容错误 
        case DATA_ERR:              // 数据内容错误
        case DATA_DIFF_ERR:         // 数据不相同错误
        case EXEC_ERR:              // 加载应用失败
        case FSTAT_ERR:             // 获取文件状态错误 -91
    	case FTOK_ERR:              // 获取IPC的ID值错误
    	case GETATTR_ERR:           // 属性设置错误
    	case GET_CHECK_ERR:         // 获取校验错误
    	case IOCTL_ERR:             // IO控制错误	
    	case LENGTH_ERR:            // 数据长度错误
    	case LISTEN_ERR:            // 监听错误
    	case LIST_FREE_ERR:         // 链表释放错误
    	case LIST_NULL_ERR:         // 链表空错误  
    	case LIST_GET_ERR:          // 链表获得错误  -80
    	case MALLOC_ERR:            // 内存分配错误
    	case MKDIR_ERR:             // 创建文件夹错误
    	case MSGGET_ERR:            // 消息队列获取错误
    	case MSGSND_ERR:            // 消息队列发送错误
    	case MSGRCV_ERR:            // 消息队列接收错误 	
    	case NO_FILE_ERR:           // 文件未找到错误
    	case NULL_ERR:              // 数据为空错误  -70 
    	case OPEN_ERR:              // 文件打开错误
    	case PIPE_ERR:              // 创建管道失败 
    	case PTHREAD_CREAT_ERR:     // 线程创建错误	
    	case PTHREAD_CANCEL_ERR:    // 线程终止错误 
    	case PTHREAD_DETACH_ERR:    // 子线程的状态设置为detached错误
    	case READ_ERR:              // 文件读取错误
    	case SQLITE_OPEN_ERR:       // 数据库打开错误
    	case SQLITE_EXEC_ERR:       // sql语句执行错误
    	case SQLITE_GET_TABLE_ERR:  // 数据库查询错误
    	case SELECT_ERR:            // 轮询等待错误 -60
    	case SELECT_NULL_ERR:       // 轮询为空错误
    	case SOCKET_ERR:            // 获取套接字错误 
    	case SETSOCKOPT_ERR:        // 设置套接字属性错误
    	case SETATTR_ERR:           // 属性设置错误
    	case SYSTEM_ERR:            // 执行shell命令失败
    	case WRITE_ERR:             // 文件写入错误
    	case S_WRITE_ERR:           // 文件写入错误
    	case P_WRITE_ERR:           // 文件写入错误
    	case FCNTL_ERR:             // 
        case REGCOMP_ERR:           // 编译正则表达式
        case NVRAM_SET_ERR:         // 数据插入失败
	    case NVRAM_COMMIT_ERR:      // 数据提交失败
	    case SHMGET_ERR:            // 创建共享内存失败   
	    case SHMAT_ERR:             // 添加共享内存到进程  
	    case SHMDT_ERR:             // 把共享内存脱离进程
	    case SHMCTL_ERR:            // 释放共享内存
	    case SEM_OPEN_ERR:          // 创建命名信号量
	    case SEM_WAIT_ERR:          // 信号量减一 
	    case SEM_POST_ERR:          // 信号量加一  
	    case SEMGET_ERR:            // 信号量获取失败 
	    case SEMCTL_ERR:            // 信号量设置失败  
	    case SEMOP_ERR:             // 信号量操作失败 
	    case STRSTR_ERR:            // strstr错误
        {            
            state_num = 13;  //状态码
            memcpy(prompt, "终端底座内部错误！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case TIMEOUT_ERR:           // 时间超时错误
        {            
            state_num = 14;  //状态码
            memcpy(prompt, "注册超时！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case PPPOE_LINK_FAILED:    // 连接PPPOE服务器失败
        {
            state_num = 15;  //状态码
            memcpy(prompt, "连接PPPOE服务器失败！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case WAN_STATE_ERR:        // wan口没有插入有效网线
        {
            state_num = 16;  //状态码
            memcpy(prompt, "请检查WAN口线是否已经插入！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case NO_INIT_ERR:          // 此设备还没有初始化
        {
            state_num = 17;  //状态码
            memcpy(prompt, "此设备还没有初始化！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case WRONGFUL_PAD_ERR:     // PAD不合法
        {
            state_num = 18;  //状态码
            memcpy(prompt, "此PAD不合法！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case PPPOE_NO_DNS_SERVER_INFO: // 不存在DNS服务器
        {
            state_num = 19;  //状态码
            memcpy(prompt, "PPPOE设置时，缺少DNS服务器信息！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case MAC_ERR: // mac 地址错误
        {
            state_num = 20;  //状态码
            memcpy(prompt, "mac 地址错误！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case NO_RECORD_ERR: // 没有此记录
        {
            state_num = 21;  //状态码
            memcpy(prompt, "没有此记录！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case PHONE_LINE_ERR: // 电话线状态异常错误
        {
            state_num = 22;  //状态码
            memcpy(prompt, "电话线状态异常，并机或电话线拔出！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case SN_PAD_ERR: // PAD序列号错误
        {
            state_num = 23;  //状态码
            memcpy(prompt, "数据库中找不到此PAD序列号！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case POSITION_AUTH_ERR: // 位置认证失败
        {
            state_num = 24;  //状态码
            memcpy(prompt, "位置认证失败！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
	    case NO_REQUEST_ERR:        // 无请求错误
	    {
            state_num = 25;  //状态码
            memcpy(prompt, "无请求！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
	    case IDENTIFYING_CODE_ERR:  // 验证码错误
	    {
            state_num = 26;  //状态码
            memcpy(prompt, "验证码错误！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
	    case OVERDUE_ERR:           // 请求数据过期
	    {
            state_num = 27;  //状态码
            memcpy(prompt, "请求数据过期！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
	    case NON_EXISTENT_ERR:      // 请求数据不存在
	    {
            state_num = 28;  //状态码
            memcpy(prompt, "请求数据不存在！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case REPEAT_ERR:            // 重复操作
        {
            state_num = 29;  //状态码
            memcpy(prompt, "重复操作！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case SN_BASE_ERR:           // base序列号错误
        {
            state_num = 30;  //状态码
            memcpy(prompt, "BASE序列号错误！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        }
        case BASE_NO_SN_ERR:        // base没有序列号
        {
            state_num = 31;  //状态码
            memcpy(prompt, "此BASE没有序列号！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        } 
        case CONFIG_NOW:        // base没有序列号
        {
            state_num = 32;  //状态码
            memcpy(prompt, "此BASE没有序列号！", sizeof(prompt) - 1);  // 提示信息
            len = strlen(prompt);
            break;
        } 
        default:
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "option does not mismatch!", MISMATCH_ERR);
            return MISMATCH_ERR;
        }
    }
    
    if ((*out_buf = (char *)malloc(len + 8)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
        return MALLOC_ERR;
    }
    memset(*out_buf, 0, len + 8);
    sprintf(*out_buf, "[%05d]%s", state_num, prompt);
    //PRINT("*out_buf = %s\n", *out_buf);
    return 0;
}

/**
 * 获取错误码
 */
int get_errno(char role, int error_num)
{
    switch (role)
    {
        case 'P': // PAD端
        {
            switch (error_num)
            {
                case CONNECT_ERR:           // 连接错误 
                {
                    return P_CONNECT_ERR;
                }
                case CHECK_DIFF_ERR:        // 校验不对错误
                {
                    return P_CHECK_DIFF_ERR;
                }
                case DATA_ERR:              // 数据内容错误
                {
                    return P_DATA_ERR;
                }
                case DATA_DIFF_ERR:         // 数据不相同错误
                {
                    return P_DATA_DIFF_ERR;
                }
                case LENGTH_ERR:            // 数据长度错误
                {
                    return P_LENGTH_ERR;
                }
                case MISMATCH_ERR:          // 选项不匹配
                {
                    return P_MISMATCH_ERR;
                }
                case READ_ERR:              // 文件读取错误
                {
                    return P_READ_ERR;
                }
                case SELECT_NULL_ERR:       // 轮询为空错误
                {
                    return P_SELECT_NULL_ERR;
                }
                case WRITE_ERR:             // 文件写入错误
                {
                    return P_WRITE_ERR;
                }
                default:
                {
                    return error_num;
                }
            }
        }
        case 'S': // 平台服务器端
        {
            switch (error_num)
            {
                case CONNECT_ERR:           // 连接错误 
                {
                    return S_CONNECT_ERR;
                }
                case CHECK_DIFF_ERR:        // 校验不对错误
                {
                    return S_CHECK_DIFF_ERR;
                }
                case DATA_ERR:              // 数据内容错误
                {
                    return S_DATA_ERR;
                }
                case DATA_DIFF_ERR:         // 数据不相同错误
                {
                    return S_DATA_DIFF_ERR;
                }
                case LENGTH_ERR:            // 数据长度错误
                {
                    return S_LENGTH_ERR;
                }
                case MISMATCH_ERR:          // 选项不匹配
                {
                    return S_MISMATCH_ERR;
                }
                case READ_ERR:              // 文件读取错误
                {
                    return S_READ_ERR;
                }
                case SELECT_NULL_ERR:       // 轮询为空错误
                {
                    return S_SELECT_NULL_ERR;
                }
                case WRITE_ERR:             // 文件写入错误
                {
                    return S_WRITE_ERR;
                }
                default:
                {
                    return error_num;
                }
            }
        }
        default:
        {
            return error_num;
        }
    }
}

/**
 * 获取链表缓冲区的加密域
 */
int get_list_MAC(struct s_data_list *data_list, unsigned short start_index, unsigned short end_index, char *MAC)
{
    PRINT_STEP("entry...\n");
    int res = 0;
    memcpy(MAC, &res, sizeof(res));
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 获取缓冲区的加密域
 */
int get_MAC(int src, char *MAC)
{
    PRINT_STEP("entry...\n");
    memcpy(MAC, &src, sizeof(src));
    src = 0;
    memcpy(MAC + sizeof(src), &src, sizeof(src));
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 获取WAN口的MAC地址
 */
int get_wan_mac(char *mac)
{
    int res = 0;
    
    // 获取ath00的mac
    char *cmd = "ifconfig ath0 | grep ath0 | awk '{print $5}'";
    char buf[18] = {0};
    
    if (mac == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "mac is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    if ((res = common_tools.get_cmd_out(cmd, buf, sizeof(buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_cmd_out failed!", res);
        return res;
    }
    memcpy(mac, buf, sizeof(buf));
    PRINT("mac = %s buf = %s\n", mac, buf);
    return 0;
}
/*******************************************************************************/
/************************        以下为项目无关代码         ********************/
/*******************************************************************************/

/*******************************************************************************/
/************************      对字符串操作的相关代码       ********************/
/*******************************************************************************/
/**
 * 把一个0-15的数字转换成十六制字符
 */
static char hex_to_char(const char in)
{
    char res = in;
	switch(in)
    {
    	case 0: res = '0'; break;
    	case 1: res = '1'; break;
    	case 2: res = '2'; break;
    	case 3: res = '3'; break;
    	case 4: res = '4'; break;
    	case 5: res = '5'; break;
    	case 6: res = '6'; break;
    	case 7: res = '7'; break;
    	case 8: res = '8'; break;
    	case 9: res = '9'; break;
    	case 10:res = 'A'; break;
    	case 11:res = 'B'; break;
    	case 12:res = 'C'; break;
    	case 13:res = 'D'; break;
    	case 14:res = 'E'; break;
    	case 15:res = 'F'; break;
    	default:
    	    break;
    }    
	return res;
}

/**
 * long转换为str
 */
int long_to_str(unsigned long src, char *dst, unsigned char dst_len)
{
    int res = 0;
    int i = 0, j = 0;
    unsigned char value = 0;
    char dst_tmp[12] = {0};
    if (dst == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf is null!", NULL_ERR);
        return NULL_ERR;
    }
    value = src;
    for (i = 0; i < dst_len - 1; i++)
    {
        if ((value / 10) == 0)
        {
            dst_tmp[i] = (value % 10) + '0';
            break;
        }
        else
        {
            dst_tmp[i] = (value % 10) + '0';
            value /= 10;
        }
    }
    for (i = 0; i < dst_len - 1; i++)
    {
        if ((dst[i] = dst_tmp[strlen(dst_tmp) - i - 1]) == '\0')
        {
            break;
        }
    }
    return 0;
}

/**
 * 把BCD码转换成字符串
 */
int BCD_to_string(char *src, int src_size, char *dst, int dst_size)
{
    //PRINT_STEP("entry...\n");
    int i = 0;
	if((src == NULL) || (src_size <= 0) || (dst == NULL) || (dst_size <= 0))
    {     
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "para is errno!", BCD_TO_STR_ERR);    
    	return BCD_TO_STR_ERR;
    }
    
	if(src_size > dst_size * 2)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "para is errno!", BCD_TO_STR_ERR);
    	return BCD_TO_STR_ERR;
    }
    
	char high4_bit = 0; //低4位
  	char low4_bit = 0;    //高4位
  	
    //清空输出数据
  	memset(dst, 0, dst_size);
          
    //遍历输入数据
  	for(i = 0; i < src_size; i++)
    {                    
      	low4_bit = src[i] & 0x0F;
        high4_bit = (src[i] & 0xF0) >> 4;
                            
        dst[i * 2] = hex_to_char(high4_bit);
        dst[i * 2 + 1] = hex_to_char(low4_bit);
    }
    //PRINT_STEP("exit...\n");
	return 0;
}

/**
 * 把字符串中可显示字符放到缓冲区的中间
 */
int make_data_middle(char *data, int data_len, int array_len)
{
    PRINT_STEP("entry...\n");
    if (data_len >= array_len)
    {
    	return 0;
    }
    
	char *data_tmp = malloc(array_len + 1);
	if (data_tmp == NULL)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
        return MALLOC_ERR;
	}
	memset(data_tmp, ' ', array_len);
    
	int i = 0, j = 0;
	for (i = (array_len - data_len) / 2, j = 0; ((i < array_len) && (j < data_len)); i++, j++)
    {
    	data_tmp[i] = data[j];    
    }
    
	memcpy(data, data_tmp, array_len);
	// free
	free(data_tmp);
	data_tmp = NULL;
    PRINT_STEP("exit...\n");
	return 0;
}

/**
 * 在src找dest
 */
int str_in_str(const void *src, unsigned short src_len, const char dest, unsigned short dest_len)
{
    int i = 0, j = 0;
    char *src_tmp = (char *)src;
    for (i = 0; i < src_len; i++)
    {
        if (*((char *)src_tmp + i) == dest)
        {
            return i;
        }
    }
    return 0;
}

/**
 * 拷贝src的len个字节到dest的index后面 
 * dest:  目标指针 
 * src:   源指针
 * index：dest索引
 * len:   长度
 */
void *memncat(void *dest, const void *src, const int index, const int len)
{
    PRINT_STEP("entry...\n");
    if ((dest == NULL) || (src == NULL))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "para is NULL!", NULL_ERR);
        return (void *)NULL_ERR;
    }
    int i = 0;
    int j = index;
    while (i < len)
    {
        *((char*)dest + index + i) = *((char*)src + i);
        i++;
    }
    PRINT_STEP("exit...\n");
    return NULL;
}

/**
 * 去除字符串中的空格
 */
int trim(char *str)
{
    int i = 0, j = 0;
    char *buf = NULL;
    if (str == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf is null!", NULL_ERR);
        return NULL_ERR;
    }
    
    if ((buf = malloc(strlen(str) + 1)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
        return MALLOC_ERR;
    }
    memcpy(buf, str, strlen(str));
    memset(str, 0, strlen(str));
    for (i = 0; i < strlen(buf); i++)
    {
        if (buf[i] != ' ')
        {
            str[j++] = buf[i];
        }
    }
    // free
    free(buf);
    buf = NULL;
    return 0;
}

/**
 * 字符串匹配
 */
int match_str(const char *rule, const char *str)
{
    int res = 0;
    int status = 0, i = 0;
    int cflags = REG_EXTENDED;
    regmatch_t pmatch[1];
    char err[128] = {0};
    const size_t nmatch = 1;
    regex_t reg;
    
    PRINT("%s\n", rule);
    PRINT("%s\n", str);
    if ((res = regcomp(&reg, rule, cflags)) != 0)
    {
        regerror(res, &reg, err, sizeof(err));
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "regcomp failed!", res);
        PRINT("res = %d errno = %d %s \n%s\n", res, errno, strerror(errno), err);
        return REGCOMP_ERR;
    }
    
    if ((status = regexec(&reg, str, nmatch, pmatch, 0)) != 0)        
    {
        regerror(res, &reg, err, sizeof(err));
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "regexec failed!", status);
        PRINT("res = %d errno = %d %s \n%s\n", res, errno, strerror(errno), err);
        return REGEXEC_ERR;
    }
    
    regfree(&reg);
    return 0;
}

/**
 * 对mac地址加上横线
 */
void mac_add_horizontal(char *mac)
{
    int i = 0, j = 0;
    char mac_tmp[13] = {0};
    if (strlen(mac) != 12)
    {
        return;
    }
    memcpy(mac_tmp, mac, sizeof(mac_tmp) - 1);
    memset(mac, 0, 18);
    
    for (i = 0; i < 17; i++)
    {
        if (((i + 1) % 3) == 0)
        {
            mac[i] = '-';
            continue;
        }
        
        mac[i] = mac_tmp[j++]; 
    }
    PRINT("mac = %s\n", mac);
}

/**
 * 对mac地址加上冒号
 */
void mac_add_colon(char *mac)
{
    int i = 0, j = 0;
    char mac_tmp[13] = {0};
    if (strlen(mac) != 12)
    {
        return;
    }
    memcpy(mac_tmp, mac, sizeof(mac_tmp) - 1);
    memset(mac, 0, 18);
    
    for (i = 0; i < 17; i++)
    {
        if (((i + 1) % 3) == 0)
        {
            mac[i] = ':';
            continue;
        }
        
        mac[i] = mac_tmp[j++]; 
    }
    PRINT("mac = %s\n", mac);
}

/**
 * 对mac地址去掉冒号
 */
void mac_del_colon(char *mac)
{
    int i = 0, j = 0;
    char mac_tmp[18] = {0};
    if (strlen(mac) != 17)
    {
        return;
    }
    memcpy(mac_tmp, mac, sizeof(mac_tmp) - 1);
    memset(mac, 0, 18);
    
    for (i = 0; i < 17; i++)
    {      
        if (mac_tmp[i] != ':')
        {  
            mac[j++] = mac_tmp[i]; 
        }
    }
    PRINT("mac = %s\n", mac);
}

/**
 * 转换成正常格式MAC地址 如 00:AA:11:BB:CC:DD
 */
int mac_format_conversion(char *mac)
{
    if (mac == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "mac is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    int i = 0, j = 0;
    char mac_tmp[19] = {0};
    char *tmp = NULL;
    
    memcpy(mac_tmp, mac, strlen(mac));
    memset(mac, 0, strlen(mac));
    mac_tmp[strlen(mac_tmp)] = ':';
    
    for (j = 0; j < 6; j++)
    {
        if ((tmp = strstr(mac_tmp, ":")) == NULL)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data err!", DATA_ERR);
            return DATA_ERR;
        }
        
        tmp[0] = '\0';
        
        if ((strlen(mac_tmp) == 1) && (mac_tmp[0] >= '0') && (mac_tmp[0] <= '9'))
        {
            mac[i++] = '0';
            mac[i++] = mac_tmp[0];
        }    
        else if ((mac_tmp[0] >= 'a') && (mac_tmp[0] <= 'f'))
        {
            if (strlen(mac_tmp) == 1)
            {
                mac[i++] = '0';
                mac[i++] = mac_tmp[0] - ('a' - 'A');
            }
            else 
            {
                mac[i++] = mac_tmp[0] - ('a' - 'A');
                if ((mac_tmp[1] >= 'a') && (mac_tmp[1] <= 'f'))
                {
                    mac[i++] = mac_tmp[1] - ('a' - 'A');
                }
                else if ((mac_tmp[1] >= '0') && (mac_tmp[1] <= '9'))
                {
                    mac[i++] = mac_tmp[1];
                }
                else
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data err!", DATA_ERR);
                    return DATA_ERR;          
                }
            }
        }
        else if ((mac_tmp[1] >= 'a') && (mac_tmp[1] <= 'f'))
        {
            if ((mac_tmp[0] >= '0') && (mac_tmp[0] <= '9'))
            {
                mac[i++] = mac_tmp[0];
            }
            else
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data err!", DATA_ERR);
                return DATA_ERR;          
            }
            mac[i++] = mac_tmp[1] - ('a' - 'A');
        }
        //else if (((((mac_tmp[0] >= '0') && (mac_tmp[0] <= '9')) || ((mac_tmp[0] >= 'a') && (mac_tmp[0] <= 'f')) || ((mac_tmp[0] >= 'A') && (mac_tmp[0] <= 'A'))) || 
            //(((mac_tmp[1] >= '0') && (mac_tmp[1] <= '9')) || ((mac_tmp[1] >= 'a') && (mac_tmp[1] <= 'f')) || ((mac_tmp[1] >= 'A') && (mac_tmp[1] <= 'A')))))
        else if ((strlen(mac_tmp) == 2) && (((mac_tmp[0] >= '0') && (mac_tmp[0] <= '9')) || ((mac_tmp[0] >= 'A') && (mac_tmp[0] <= 'F'))) && 
            (((mac_tmp[1] >= '0') && (mac_tmp[1] <= '9')) || ((mac_tmp[1] >= 'A') && (mac_tmp[1] <= 'F'))))
        {
            mac[i++] = mac_tmp[0];
            mac[i++] = mac_tmp[1];
        }
        else
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data err!", DATA_ERR);
            return DATA_ERR;            
        }
        
        if (i != 5)
        {
            tmp++;
            memcpy(mac_tmp, tmp, strlen(tmp));
            mac_tmp[strlen(tmp)] = '\0';
            tmp = NULL;
        }
    }
    
    mac_add_colon(mac);
    return 0;
}

/**
 * 对数据加引号
 */
int add_quotation_marks(char *data)
{
    char *data_tmp = NULL;
    if (data == NULL)   
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    if ((data_tmp = (char *)malloc(strlen(data) + 1)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_config failed!", MALLOC_ERR);
        return MALLOC_ERR;
    }    
    memset(data_tmp, 0, strlen(data) + 1);    
    memcpy(data_tmp, data, strlen(data));
    memset(data, 0, strlen(data));
    
    if ((data_tmp[0] == '\"') && (data_tmp[strlen(data_tmp) - 1] == '\"'))
    {
        free(data_tmp);
        data_tmp = NULL;
        return 0;        
    }
    data[0] = '\"';
    memcpy(data + 1, data_tmp, strlen(data_tmp));
    data[strlen(data_tmp) + 1] = '\"';
    
    // free
    free(data_tmp);
    data_tmp = NULL;
    return 0;
}

/**
 * 对数据去除引号
 */
int del_quotation_marks(char *data)
{
    char *data_tmp = NULL;
    if (data == NULL)   
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    if ((data[0] != '\"') || (data[strlen(data) - 1] != '\"'))
    {
        return 0;
    }
    
    if ((data_tmp = (char *)malloc(strlen(data) + 1)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_config failed!", MALLOC_ERR);
        return MALLOC_ERR;
    }    
    memset(data_tmp, 0, strlen(data) + 1);    
    memcpy(data_tmp, data, strlen(data));
    memset(data, 0, strlen(data));
    
    memcpy(data, data_tmp + 1, strlen(data_tmp) - 2);
    
    free(data_tmp);
    data_tmp = NULL;
    PRINT("%s\n", data);
    return 0;
}

/*******************************************************************************/
/************************         时间操作相关代码          ********************/
/*******************************************************************************/

/**
 * 获取日期和时间字符串
 */
char *get_datetime_buf()
{
    struct timeval tv;	
	char buf_tmp[64] = {0};	
	memset(__datetime_buf, 0, sizeof(__datetime_buf));
	memset(&tv, 0, sizeof(struct timeval));
	
	gettimeofday(&tv, NULL);
	strftime(buf_tmp, sizeof(buf_tmp), "[%Y-%m-%d][%T:", localtime(&tv.tv_sec));    
	sprintf(__datetime_buf, "%s%06d]", buf_tmp, tv.tv_usec);
	return __datetime_buf;
}

/**
 * 计算应用程序自开机到现在的运行时间
 */ 
static int get_service_use_time(time_t start_time, char *service_use_time)
{
    PRINT_STEP("entry...\n");
    if (service_use_time == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "service_use_time is NULL!", NULL_ERR);
        return NULL_ERR;
    }
	time_t current_time = time(0);
    
	unsigned long long total_time = current_time - start_time;
    
	unsigned short year = total_time / (365 * 24 * 3600);
	total_time -= year * (365 * 24 * 3600);
    
	unsigned short month = total_time / (30 * 24 * 3600);
	total_time -= month * (30 * 24 * 3600);
    
	unsigned short day = total_time / (24 * 3600);
	total_time -= day * (24 * 3600);
    
	unsigned short hour = total_time / 3600;
	total_time -= hour * 3600;
    
	unsigned short minute = total_time / 60;
	total_time -= minute * 60;
    
	unsigned short second = total_time;
                        
	sprintf(service_use_time, "Service run time: %4dYear(s)%2dMonth(s)%2dDay(s) %2dHour(s)%2dMinute(s)%2dSecond(s) ", year, month, day, hour, minute, second);
	PRINT_STEP("exit...\n");
	return 0;
}

/**
 * 计算使用时间
 */
static double get_use_time(struct timeval start, struct timeval end)
{
    PRINT_STEP("entry...\n");
	double use_time = 0.0;	
	use_time = (1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec)) / 1000000.0; 
	PRINT_STEP("exit...\n");
	return use_time;
}

/**
 * 超时判断
 */
static int time_out(struct timeval *start, struct timeval *end)
{    
    if (((*end).tv_sec - (*start).tv_sec) >= g_config.total_timeout)
    {
        char buf[30] = {0};
        sprintf(buf, "time out %d (s)!", g_config.total_timeout);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, buf, TIMEOUT_ERR);
        return TIMEOUT_ERR;
    }
    return 0;
}

/**
 * 获取开始时间
 */
int set_start_time(char *start_time)
{
    if (strftime == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    struct timeval tv_start_time;
    gettimeofday(&tv_start_time);
    sprintf(start_time, "startTime:%.0lf,", (tv_start_time.tv_sec * 1000.0 + tv_start_time.tv_usec / 1000.0));
    return 0;
}

/*******************************************************************************/
/************************         链表操作相关代码          ********************/
/*******************************************************************************/
/**
 * 用于在链表的尾部添加数据
 */
int list_tail_add_data(struct s_data_list *a_data_list, char data)
{
    PRINT_STEP("entry...\n");
    struct s_list_node *list_data = malloc(sizeof(struct s_list_node));
    if (list_data == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);     
    	return MALLOC_ERR;    
    }
    list_data->data = data;
    
    // 如果链表头为NULL
    if (a_data_list->head == NULL)
    {
        a_data_list->head = list_data;
        a_data_list->tail = list_data;
        
        a_data_list->head->next = a_data_list->tail;
        a_data_list->head->prev = a_data_list->tail;
        
        a_data_list->tail->next = a_data_list->head;
        a_data_list->tail->prev = a_data_list->head;
    }
    else //其他情况
    {
        a_data_list->tail->next = list_data;
        a_data_list->head->prev = list_data;
        
        list_data->prev = a_data_list->tail;
        list_data->next = a_data_list->head;
        a_data_list->tail = list_data;
    }
    a_data_list->list_len++;
    
    // free 不能打开 这句话不需要（list_free的时候释放）
    //free(list_data);
    //list_data = NULL;
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 用于在链表的头部删除数据
 */
int list_head_del_data(struct s_data_list *a_data_list, char data)
{
    PRINT_STEP("entry...\n");
    int i = 0;
    if (a_data_list->head == NULL)
    {
        return 0;
    }
    
    struct s_list_node *list_data = NULL;
    if ((a_data_list->list_len == 1) && (a_data_list->head->data == data))
    {
        a_data_list->head->data = 0;
        a_data_list->tail->data = 0;
                  
        free(a_data_list->head);
        a_data_list->head = NULL;
        a_data_list->tail = NULL;
        a_data_list->list_len--;
        return 0;
    }
    
    if ((a_data_list->list_len == 2) && (a_data_list->head->data == data))
    {
        a_data_list->head->data = 0;
        free(a_data_list->head);
        a_data_list->head = NULL;
        
        a_data_list->head = a_data_list->tail;
        
        a_data_list->head->next = a_data_list->tail;
        a_data_list->head->prev = a_data_list->tail;
        
        a_data_list->tail->next = a_data_list->head;
        a_data_list->tail->prev = a_data_list->head;
        a_data_list->list_len--;
        return 0;
    }
        
    for (list_data = a_data_list->head; list_data->next != a_data_list->head; list_data = list_data->next)
    {
        if (list_data->data == data)
        {
            if (list_data == a_data_list->head)
            {
                a_data_list->tail->next = a_data_list->head->next;
                a_data_list->head->next->prev = a_data_list->head->prev;
                a_data_list->head = a_data_list->head->next;   
            }
            else if (list_data == a_data_list->tail)
            {
                a_data_list->head->prev = a_data_list->tail->prev;
                a_data_list->tail->prev->next = a_data_list->tail->next;
                a_data_list->tail = a_data_list->tail->prev;
            }
            else
            {
                list_data->prev->next = list_data->next;
                list_data->next->prev = list_data->prev;  
            }
            list_data->data = 0;            
            free(list_data);
            list_data = NULL;
            a_data_list->list_len--;
            return 0;
        }
    }
    
    if (a_data_list->tail->data == data)
    {
        list_data = a_data_list->tail;
        a_data_list->head->prev = a_data_list->tail->prev;
        a_data_list->tail->prev->next = a_data_list->tail->next;
        a_data_list->tail = a_data_list->tail->prev;
        
        list_data->data = 0;            
        free(list_data);
        list_data = NULL;
        a_data_list->list_len--;
    }
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 用于在链表的尾部添加数据
 */
int list_free(struct s_data_list *a_data_list)
{
    PRINT_STEP("entry...\n");
    if ((a_data_list->list_len == 0) || (a_data_list->head == NULL))
    {
        return 0;
    }
    
    struct s_list_node *list_data = NULL;
    struct s_list_node *list_data_tmp = NULL;
    for (list_data = a_data_list->head; list_data != a_data_list->tail; list_data = list_data_tmp)
    {
        list_data_tmp = list_data->next;
        free(list_data);
        list_data = NULL;
    }
    
    free(a_data_list->tail);
    a_data_list->tail = NULL;
    a_data_list->head = NULL;
    memset(a_data_list, 0, sizeof(struct s_data_list));
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 用于在链表获得链表中的元素，下标从0开始
 */
int list_get_data(struct s_data_list *a_data_list, unsigned short index, unsigned short count, char *list_buf)
{
    PRINT_STEP("entry...\n");
    unsigned short i = 0, j = 0;
    char bit = 0;
    if (a_data_list->list_len == 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_len is zero!", LIST_NULL_ERR);
        return LIST_NULL_ERR;
    }
    
    if ((index == a_data_list->list_len - 1) && (count == 1))
    {
        list_buf[0] = a_data_list->tail->data;
        return 0;
    }
    
    struct s_list_node *list_data = NULL;
    struct s_list_node *list_data_tmp = NULL;
    for (i = 0, list_data = a_data_list->head; ((i < a_data_list->list_len) && (list_data != a_data_list->tail)); i++, list_data = list_data->next)
    {
        if (i == index)
        {
            bit = 1;
        }
        if (bit == 1)
        {
            if (count > j)
            {
                list_buf[j++] = list_data->data;
            }
            else
            {
                return 0;
            }
        }
    }
    
    if (count > j)
    {
        list_buf[j++] = list_data->data;
        if (j != count)
        {
            
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list data get failed!", LIST_GET_ERR);
            return LIST_GET_ERR;
        }  
    }
    
    if (bit == 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list data get failed!", LIST_GET_ERR);
        return LIST_GET_ERR;
    }    
    PRINT_STEP("exit...\n");
    return 0;
}

/*******************************************************************************/
/************************      数据接收与发送相关代码       ********************/
/*******************************************************************************/

/**
 * 接收一个字节
 */
int recv_one_byte(int fd, char *data, struct timeval *tv)
{
    int i = 0;
    int res = 0;
    fd_set fdset; 
    FD_ZERO(&fdset);
    FD_SET(fd, &fdset);
    
    fcntl(fd, F_SETFL, FNDELAY);
    
    switch (select(fd + 1, &fdset, NULL, NULL, tv))
    {
        case -1:
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "select failed!", SELECT_ERR);
            return SELECT_ERR;
        }
        case 0:
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "select no data recv!", SELECT_NULL_ERR);
            return SELECT_NULL_ERR;
        }
        default:
        {
            if (FD_ISSET(fd, &fdset) > 0)
            {
                if ((res = read(fd, data, sizeof(char))) != sizeof(char))
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "read failed!", READ_ERR);
                    return READ_ERR;
                }
                #if PRINT_DATA
                printf("%02X ", (unsigned char)data[0]);
                fflush(stdout);
                #endif
            }
            else
            {
                #if PRINT_DATA
                printf("\n");
                #endif
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "select no data recv!", SELECT_NULL_ERR);
                return SELECT_NULL_ERR;
            }
        }        
    }
    return 0;
}

/**
 * 接收一个buf
 */
int recv_data(unsigned int fd, unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv)
{
    int i = 0;
    int res = 0;
    struct timeval time;
    
    if (data == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    if (data_len == 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data_len is zero!", LENGTH_ERR);
        return LENGTH_ERR;
    }
    
    if (tv == NULL)
    {
        time.tv_sec = 0;
        time.tv_usec = 0;
    }
    else
    {
        memcpy(&time, tv, sizeof(struct timeval));
    }
    
    for (i = 0; i < data_len; i++)
    {        
        if ((res = recv_one_byte(fd, &data[i], &time)) != 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_one_byte failed!", res);
            return res;
        }
    }
    #if PRINT_DATA
    printf("\n");
    #endif
    return 0;
}

/**
 * 数据头接收处理
 */
int recv_data_head(int fd, char *head, unsigned char head_len, struct timeval *tv)
{
    int i = 0, j = 0;
    int res = 0;
    char *data_head = NULL;
    
    if ((head == NULL) || (head_len == 0))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "head is NULL or head_len is zero!", NULL_ERR);
        return NULL_ERR;
    }
    
    if ((data_head =(char *)malloc(head_len)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
        return MALLOC_ERR;
    }
    memset((void *)data_head, 0, head_len);
    
    for(i = 0; i < head_len;)
    {
        if ((res = recv_one_byte(fd, &data_head[i], tv)) == 0)      
        {
            i++;
            if (i == head_len)
            {
                for (j = 0; j < head_len; j++)
                {
                    if (data_head[j] != head[j])
                    {
                        break;
                    }
                }
                
                if (j == head_len)
                {
                    PRINT("data head recv success!\n");
                    break;
                }
                
                for (j = 0; j < head_len - 1; j++)
                {
                    data_head[j] = data_head[j + 1];                    
                }
                i--;
            }
        }
        else
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_one_byte failed!", res);
            free(data_head);    
            data_head = NULL;
            return res;
        }
    }
    free(data_head);    
    data_head = NULL;
    return 0;
}

/**
 * 发送一个字节
 */
int send_one_byte(int fd, char *data, struct timeval *tv)
{
    int i = 0;
    int res = 0;
    fd_set fdset;        
    struct timeval time;
    
    if (tv == NULL)
    {
        time.tv_sec = 0;
        time.tv_usec = 0;
    }
    else
    {
        memcpy(&time, tv, sizeof(struct timeval));
    }
    
    FD_ZERO(&fdset);
    FD_SET(fd, &fdset);
    
    switch (select(fd + 1, NULL, &fdset, NULL, &time))
    {
        case -1:
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "select failed!", SELECT_ERR);
            return SELECT_ERR;
        }
        case 0:
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "select no write env!", SELECT_NULL_ERR);
            return SELECT_NULL_ERR;
        }
        default:
        {
            if (FD_ISSET(fd, &fdset) > 0)
            {
                if (write(fd, data, sizeof(char)) != sizeof(char))
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "write failed!", WRITE_ERR);
                    return WRITE_ERR;
                }
            }
            else
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "select no write env!", SELECT_NULL_ERR);
                return SELECT_NULL_ERR;
            }
        }        
    }
    return 0;
}

/**
 * 发送一个buf
 */
int send_data(unsigned int fd, unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv)
{
    int i = 0;
    int res = 0;
    struct timeval time;
    if (tv == NULL)
    {
        time.tv_sec = 0;
        time.tv_usec = 0;
    }
    else
    {
        memcpy(&time, tv, sizeof(struct timeval));
    }
    PRINT_BUF_BY_HEX(data, a_data_list, data_len, __FILE__, __FUNCTION__, __LINE__);
    PRINT("data_len = %d\n", data_len);
    
    if ((data != NULL) && (data_len != 0))
    {
        #if 0
        for (i = 0; i < data_len; i++)
        {        
            if ((res = send_one_byte(fd, &data[i], &time)) != 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_one_byte failed!", res);
                return res;
            }
        }
        #else
        PRINT("data_len = %d\n", data_len);
        if (write(fd, data, data_len) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "write failed!", WRITE_ERR);
            return WRITE_ERR;
        }
        PRINT("after write\n");
        #endif
    }
    else if (a_data_list != NULL)
    {
        struct s_list_node *list_node;
        for (i = 0, list_node = a_data_list->head; i < data_len; list_node = list_node->next, i++)
        {
            if ((res = send_one_byte(fd, &(list_node->data), &time)) != 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_one_byte failed!", res);
                return res;
            }
        }
    }
    else
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data err!", DATA_ERR);
        return DATA_ERR;
    }
    PRINT("exit send_data\n");
    return 0;
}


/*******************************************************************************/
/************************     随机字符串相关代码代码        ********************/
/*******************************************************************************/

/**
 * 获取一个随机数
 */
long get_rand_num(unsigned long start, unsigned long end, unsigned short type_len)
{
    int fd = 0;
    long value = 0;
    if ((fd = open("/dev/urandom", O_RDONLY)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "open failed!", OPEN_ERR);
        return OPEN_ERR;
    }
    if (read(fd, &value, type_len) != type_len)
    {
        close(fd);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "read failed!", READ_ERR);
        return READ_ERR;
    }
    
    value = start + value % (end - start + 1);
    close(fd);
    PRINT("value = %08X %02X\n", value, (char)value);
    return value;
}

/**
 * unicode码转换成utf-8
 */
int unicode_to_utf_8(char *unicode, unsigned char unicode_len, char *utf_8)
{
    char tmp = 0;
    if ((unicode == NULL) || (utf_8 == NULL))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    switch (unicode_len)
    {
        case 2:
        {
            utf_8[2] = (0x80 | (unicode[0] & 0x3F));
            
            utf_8[1] = (0x80 | ((unicode[0] & 0xC0) >> 6));
            utf_8[1] |= ((unicode[1] & 0x0F) << 2);
            
            utf_8[0] = (0xE0 | ((unicode[1] & 0xF0) >> 4));
            PRINT("%02X %02X %02X\n", utf_8[0], utf_8[1], utf_8[2]);
            break;
        }
        default:
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "option does not mismatch!", MISMATCH_ERR);
            return MISMATCH_ERR;
        }
    }
    PRINT("utf_8 = %s\n", utf_8);
    return 0;    
}

/**
 * 随机获取一个中文字符
 */
int get_chinese_char(unsigned char *buf, char *code)
{
    int res = 0;
    if ((buf == NULL) || (code == NULL))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    if (memcmp(code, "gb2312", strlen("gb2312")) == 0)
    {
        if ((res = get_rand_num(16, 55, sizeof(char))) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_rand_num failed!", res);
            return res;
        }
        buf[0] = (res & 0x000000FF) + 0xA0;
        PRINT("buf[0] = %02X\n", buf[0]);
        
        if ((res = get_rand_num(1, 94, sizeof(char))) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_rand_num failed!", res);
            return res;
        }
        buf[1] = (res & 0x000000FF) + 0xA0;
        PRINT("buf[1] = %02X\n", buf[1]);
    }
    else if (memcmp(code, "utf-8", strlen("utf-8")) == 0)
    {
        #if 1
        char tmp[2] = {0};
        if ((res = get_rand_num(0x4E00, 0x9FA5, sizeof(short))) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_rand_num failed!", res);
            return res;
        }
        
        tmp[0] = (res & 0x0000FFFF);
        tmp[1] = (res & 0x0000FF00) >> 8;
        PRINT("tmp[0] = %02X\n", tmp[0]);
        PRINT("tmp[1] = %02X\n", tmp[1]);
        
        if ((res = unicode_to_utf_8(tmp, 2, buf)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "unicode_to_utf_8 failed!", res);
            return res;
        }
        #else
        if ((res = get_rand_num(0, sizeof(utf8_char) / 3, sizeof(short))) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_rand_num failed!", res);
            return res;
        }
        buf[0] = utf8_char[res][0];
        buf[1] = utf8_char[res][1];
        buf[2] = utf8_char[res][2];
        #endif
        PRINT("buf = %s\n", buf);
    }
    else
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "option does not mismatch!", MISMATCH_ERR);
        return MISMATCH_ERR;
    } 
    
    return 0;
}

/**
 * 随机获取一个中文字符
 */
char get_english_char()
{
    return (char)get_rand_num('A', 'Z', sizeof(char));
}

/**
 * 获取随机字符串
 */
int get_rand_string(unsigned char english_char_len, unsigned char chinese_char_len, char *out, unsigned char chinese_char_type)
{
    int i = 0, j = 0;
    char tmp = 0;
    int res = 0;
    char *buf = NULL;
    char *rand_num = NULL;
    char flag = 0;
    char char_len = 0;
    
    char chinese_char[4] = {0};
    if ((chinese_char_type == UTF8) && (chinese_char_len != 0))
    {
        char_len = 3;
        
        if (english_char_len % 3 == 1)
        {
            english_char_len += 2;
        }
        else if (english_char_len % 3 == 2)
        {
            english_char_len++;
        }
    }
    else if ((chinese_char_type == GB2312) && (chinese_char_len != 0))
    {
        char_len = 2;
        if (english_char_len % 2 != 0)
        {
            english_char_len++;
        }
    }
    
    if ((buf = (char *)malloc(english_char_len + chinese_char_len * char_len + 1)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
        return MALLOC_ERR;
    }
    memset(buf, 0, english_char_len + chinese_char_len * char_len + 1);
    
    if (chinese_char_len != 0)
    {
        if ((rand_num = (char *)malloc(english_char_len / char_len + chinese_char_len + 1)) == NULL)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
            free(buf);
            buf = NULL;
            return MALLOC_ERR;
        }
        memset(rand_num, 0, english_char_len / char_len + chinese_char_len + 1);
    }
    else 
    {
        if ((rand_num = (char *)malloc(english_char_len + 1)) == NULL)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
            free(buf);
            buf = NULL;
            return MALLOC_ERR;
        }
        memset(rand_num, 0, english_char_len + 1);
    }
    
    for (i = 0; i < english_char_len; i++)
    {
        if ((tmp = get_english_char()) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_english_char failed!", tmp);
            free(buf);
            buf = NULL;
            free(rand_num);
            rand_num = NULL;
            return tmp;
        }
        buf[i] = tmp;
    }
    PRINT("buf = %s\n", buf);
    
    for (i = 0; i < chinese_char_len; i++)
    {
        if ((res = get_chinese_char(chinese_char, "utf-8")) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_chinese_char failed!", res);
            free(buf);
            buf = NULL;
            free(rand_num);
            rand_num = NULL;
            return res;
        }
        PRINT("chinese_char = %s\n", chinese_char);
        memcpy(buf + strlen(buf), chinese_char, 3);
    }
    PRINT("buf = %s\n", buf);
    
    for (i = 0; (i < (english_char_len + chinese_char_len * char_len)) && (chinese_char_len != 0); i += char_len)
    {
        if ((res = get_rand_num(0, (unsigned char)(english_char_len / char_len) + chinese_char_len - 1, 1)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_rand_num failed!", res);
            free(buf);
            buf = NULL;
            free(rand_num);
            rand_num = NULL;
            return res;
        }
        for (j = 0; j < (i / char_len); j++)
        {
            if (rand_num[j] == (res & 0x000000FF)) 
            {
                flag = 1;
                break;
            }         
        }
        if (flag == 1)
        {
            i -= char_len;
            flag = 0;
            continue;
        }
        rand_num[i / char_len] = (res & 0x000000FF);     
    }
    
    if (chinese_char_len != 0)
    {
        for (i = 0; i < (english_char_len + chinese_char_len * char_len); i += char_len)
        {
            out[i] = buf[rand_num[i / char_len] * char_len];
            out[i + 1] = buf[rand_num[i / char_len] * char_len + 1];
            out[i + 2] = buf[rand_num[i / char_len] * char_len + 2];
        }
    }
    else
    {
        memcpy(out, buf, strlen(buf));
    }
    free(buf);
    buf = NULL;
    free(rand_num);
    rand_num = NULL;
    PRINT("out = %s\n", out);
    return 0;
}


/*******************************************************************************/
/************************           其他代码                ********************/
/*******************************************************************************/
#if 0
/**
 * 获得命令输出
 */
int get_cmd_out(char *cmd, char *buf, unsigned short buf_len)
{
    int pipe_fd[2] = {0};
	pid_t child_pid = 0;
	struct timeval tv = {5, 1};
	
	int res = 0;
	if ((cmd == NULL) || (buf == NULL))
	{
	    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "para is NULL!", NULL_ERR);
	    return NULL_ERR;
	}
	memset(buf, 0, buf_len);
	if (pipe(pipe_fd) < 0)
	{
	    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "pipe failed!", PIPE_ERR);
	    return PIPE_ERR;
	}
	
	if ((child_pid = fork()) < 0)
	{
	    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "fork failed!", FTOK_ERR);
		return FTOK_ERR;
	}
	PRINT("child_pid = %d\n", child_pid);
	if (child_pid == 0) // subprocess write
	{
	    PRINT("______________________________\n");
		close(pipe_fd[0]);
		char *cmd_buf = NULL;
		
		if ((cmd_buf = (char *)malloc(strlen(cmd) + 10)) == NULL)
    	{
    	    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
    		return MALLOC_ERR;
    	}
    	memset(cmd_buf, 0, strlen(cmd) + 10);
	    
		PRINT("pipe_fd[1] = %d\n", pipe_fd[1]);		
		sprintf(cmd_buf, "%s 1>&%d", cmd, pipe_fd[1]);
		PRINT("cmd_buf = %s\n", cmd_buf);
		PRINT("______________________________\n");
		if (system(cmd_buf) < 0)
		{
		    res = SYSTEM_ERR;
		}
		close(pipe_fd[1]);
		free(cmd_buf);
		cmd_buf = NULL;
		PRINT("_____________________________get_cmd_out res = %d\n", res);
	    exit(res); //此处用exit，结束当前进程
	    //return res;
	}
	else // parent process read
	{
		close(pipe_fd[1]);
		memset(buf, 0, buf_len);
		PRINT("pipe_fd[0] = %d\n", pipe_fd[0]);
        #if 0
		if (read(pipe_fd[0], buf, buf_len - 1) < 0)
		{
		    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "read failed!", READ_ERR);
		    close(pipe_fd[0]);
		    //wait(&res);
		    waitpid(child_pid, &res, 0);
		    return READ_ERR;
		}
        #else
        PRINT("______________________________\n");
		recv_data(pipe_fd[0], buf, NULL, buf_len - 1, &tv);
		if (strlen(buf) == 0)
		{
		    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "read failed!", READ_ERR);
		    PRINT("______________________________\n");
		    close(pipe_fd[0]);
		    waitpid(child_pid, &res, 0);
		    return READ_ERR;
		}
        #endif
		close(pipe_fd[0]);
		PRINT("______________________________\n");
		waitpid(child_pid, &res, 0);
		PRINT("_____________________________get_cmd_out res = %d\n", res);
		if (res != 0)
		{
		    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "system failed!", SYSTEM_ERR);
		    res = SYSTEM_ERR;
		}
		return res;
	}
}
#else

/**
 * 获得命令输出
 */
int get_cmd_out(char *cmd, char *buf, unsigned short buf_len)
{
    int res = 0;
    int fd = 0;
	char file_name[64] = {0};
	char rm_buf[64] = {};
	char *cmd_buf = NULL;	
	struct timeval tv;
	memset(&tv, 0, sizeof(struct timeval));
	struct stat file_stat;
	memset(&file_stat, 0, sizeof(struct stat));
	
	if ((cmd == NULL) || (buf == NULL))
	{
	    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "para is NULL!", NULL_ERR);
	    return NULL_ERR;
	}
	
	gettimeofday(&tv, NULL);	
	sprintf(file_name, "/var/terminal_dev_register/log/.%d%d", (int)tv.tv_sec, (int)tv.tv_usec);
	sprintf(rm_buf, "rm -rf %s", file_name);
	
	if ((cmd_buf = (char *)malloc(strlen(cmd) + 64)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
        system(rm_buf);
    	return MALLOC_ERR;
    }
    memset(cmd_buf, 0, strlen(cmd) + 10);
	
	sprintf(cmd_buf, "%s >%s", cmd, file_name);
	PRINT("cmd_buf = %s\n", cmd_buf);
	
	if (system(cmd_buf) < 0)
	{
	    res = SYSTEM_ERR;
	    free(cmd_buf);
	    cmd_buf = NULL;
	    system(rm_buf);
	    return res;
	}
	PRINT("after system!\n");
	free(cmd_buf);
	cmd_buf = NULL;
	PRINT("before open!\n");
	if ((fd = open(file_name, O_RDONLY, 0644)) < 0)
	{
	    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "open failed!", OPEN_ERR);
	    system(rm_buf);
    	return OPEN_ERR;
	}
	if (fstat(fd, &file_stat) < 0)
	{
	    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "fstat failed!", FSTAT_ERR);
	    system(rm_buf);
	    close(fd);
    	return FSTAT_ERR;
	}
	PRINT("file_stat.st_size = %d\n", (int)file_stat.st_size);
	if (file_stat.st_size == 0)
	{
	    memset(buf, 0, buf_len);
	    system(rm_buf);
	    close(fd);
    	return 0;
	}
	memset(buf, 0, buf_len);
	#if 0
	if (read(fd, buf, buf_len - 1) < 0)
	{
	    PRINT("______________________________\n");
	    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "read failed!", READ_ERR);
		close(fd);
		system(rm_buf);
		return READ_ERR;
    }
    #else
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    
    PRINT("fd = %d\n", fd);
    recv_data(fd, buf, NULL, buf_len - 1, &tv);
	if (strlen(buf) == 0)
	{
	    PRINT("______________________________\n");
	    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "read failed!", READ_ERR);
	    close(fd);
	    system(rm_buf);
		return READ_ERR;
	}
	#endif
	close(fd);
	system(rm_buf);
	return 0;
}
#endif

#if 0
/**
 * 用于设置串口的属性
 */
int serial_init(const int fd, int baud)
{
    PRINT_STEP("entry...\n");
    int res = 0;
	struct termios old_opt, new_opt;
	memset(&old_opt, 0, sizeof(struct termios));
	memset(&new_opt, 0, sizeof(struct termios));

	if (tcgetattr(fd, &old_opt) < 0)
    {  
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "tcgetattr failed!", GETATTR_ERR);
        return GETATTR_ERR;
    }

    // 配置
	new_opt.c_iflag = IGNPAR;
    new_opt.c_iflag &= ~(ICRNL |IXON);
    
    switch (baud)
    {
        case 2400:
        {
            new_opt.c_cflag = B2400;
            break;
        }
        case 4800:
        {
            new_opt.c_cflag = B4800;            
            break;
        }
        case 9600:
        {
            new_opt.c_cflag = B9600;
            break;
        }
        case 57600:
        {
            new_opt.c_cflag = B57600;
            break;
        }
        case 115200:
        {
            new_opt.c_cflag = B115200;
            break;
        }
        default :
        {
            new_opt.c_cflag = B115200;
            break;
        }
    }
	
  	new_opt.c_cflag |= CS8;                //数据位为8
  	new_opt.c_cflag &= ~CRTSCTS;
  	new_opt.c_cflag |= (CREAD | CLOCAL);   //
  	new_opt.c_cflag &= ~CSIZE;
  	new_opt.c_cflag &= ~PARENB;            //无校验   
  	new_opt.c_cflag &= ~CSTOPB;            //停止位是一位
	
	new_opt.c_oflag = 0;
	new_opt.c_lflag = 0; 
	
  	new_opt.c_cc[VMIN] = 1;                // 当接收到一个字节数据就读取
  	new_opt.c_cc[VTIME] = 0;               // 不启用计时器  
        
  	if (tcsetattr(fd, TCSANOW, &new_opt) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "tcsetattr failed!", SETATTR_ERR);
        return SETATTR_ERR;
    }
    tcflush(fd, TCIOFLUSH);
    PRINT_STEP("exit...\n");
    return 0;
}
#else
/**
 * 用于设置串口的属性
 */
int serial_init(const int fd, int baud)
{
    PRINT_STEP("entry...\n");
    int res = 0;
	struct termios old_opt, new_opt;
	memset(&old_opt, 0, sizeof(struct termios));
	memset(&new_opt, 0, sizeof(struct termios));

	if (tcgetattr(fd, &old_opt) < 0)
    {  
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "tcgetattr failed!", GETATTR_ERR);
        return GETATTR_ERR;
    }
    
    // 配置
	new_opt.c_iflag = IGNPAR;
    
    switch (baud)
    {
        case 2400:
        {
            new_opt.c_cflag = B2400;
            break;
        }
        case 4800:
        {
            new_opt.c_cflag = B4800;            
            break;
        }
        case 9600:
        {
            new_opt.c_cflag = B9600;
            break;
        }
        case 57600:
        {
            new_opt.c_cflag = B57600;
            break;
        }
        case 115200:
        {
            new_opt.c_cflag = B115200;
            break;
        }
        default :
        {
            new_opt.c_cflag = B115200;
            break;
        }
    }
    
  	new_opt.c_cflag |= CS8;                //数据位为8  
  	new_opt.c_cflag |= (CREAD | CLOCAL);   //
  	new_opt.c_cflag &= ~PARENB;            //无校验   
  	new_opt.c_cflag &= ~CSTOPB;            //停止位是一位
  	new_opt.c_cc[VMIN] = 1;                // 当接收到一个字节数据就读取
  	new_opt.c_cc[VTIME] = 0;               // 不启用计时器  
    
        
  	if (tcsetattr(fd, TCSANOW, &new_opt) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "tcsetattr failed!", SETATTR_ERR);
        return SETATTR_ERR;
    }
    tcflush(fd, TCIOFLUSH);
    PRINT_STEP("exit...\n");
    return 0;
}
#endif

/**
 * 校验位计算
 */
int get_checkbit(char *buf, struct s_data_list *data_list, unsigned short start_index, unsigned short len, unsigned char check_type, unsigned char bytes)
{
    unsigned short i = 0;
    char check = 0;
    unsigned short check_sum = 0;
    if (buf != NULL)
    {
        switch (check_type)
        {
            case XOR: // 异或校验计算
            {
                for (i = start_index; i < start_index + len; i++)
                {
                    check ^= buf[i];
                }
                break;
            }
            case SUM: // 校验和
            {
                //1.将每个字节内容相加；
            	for (i = start_index; i < start_index + len; i++)
                {
                	check_sum += buf[i];
                }
                
                //2.相加结果取模256；
            	check = check_sum % 256;
                
                //3.求补码（正数等于源码， 负数等于取反加1）。    
            	if ((check & 0x80) == 0x80)
                {    
                    check = (~check) + 1;
                }
                break;
            }
            default:
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "option is mistmatching!", MISMATCH_ERR);
                return MISMATCH_ERR;
            }
        }
    }
    else if (data_list != NULL)
    {
        struct s_list_node *list_data;
        switch (check_type)
        {
            case XOR:
            {
                for (i = 0, list_data = data_list->head; i < start_index + len; i++, list_data = list_data->next)
                {
                    if (i >= start_index)
                    {
                        check ^= list_data->data;
                    }
                }
                break;
            }
            case SUM:
            {
                //1.将每个字节内容相加;
                for (i = 0, list_data = data_list->head; i < start_index + len; i++, list_data = list_data->next)
                {
                    if (i >= start_index)
                    {
                        check_sum += list_data->data;
                    }
                }
                
                //2.相加结果取模256；
            	check = check_sum % 256;
                
                //3.求补码（正数等于源码， 负数等于取反加1）。
                
            	if ((check & 0x80) == 0x80)
                {    
                    check = (~check) + 1;
                }
                break;
            }
            default:
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "option is mistmatching!", MISMATCH_ERR);
                return MISMATCH_ERR;
            }
        }
    }
    else
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    return check;
}

/**
 * 以16进制形式打印
 */
int print_buf_by_hex(const void *buf, struct s_data_list *data_list, const unsigned long len, const char *file_name, const char *function_name, const unsigned short line_num)
{
    int i = 0;
    int res = 0;
    unsigned short len_tmp = 0;
    char *log_buf = NULL;
    char tmp_buf[2] = {0};
    
    if (buf != NULL)
    {
        if (len == 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "len is zero!", DATA_ERR);
            return DATA_ERR;
        }
        
        if ((log_buf = malloc(len * 2 + 1 + 25)) == NULL)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
            return MALLOC_ERR;
        }
        PRINT("log_buf = %08X log_buf = %p\n", *log_buf, log_buf);
        
        memset(log_buf, 0, len * 2 + 1 + 25);
        
        sprintf(log_buf, "[len = %5ld][data = ", len);
        len_tmp = strlen(log_buf);
        
        if ((res = BCD_to_string((char *)buf, (int)len, log_buf + len_tmp, (int)len * 2)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "BCD_to_string failed!", res);
            free(log_buf);
            log_buf = NULL;
            return res;
        }
        log_buf[strlen(log_buf)] = ']';
    }
    else if (data_list != NULL)
    {
        struct s_list_node *list_data = NULL;
        if (data_list->list_len == 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "len is zero!", DATA_ERR);
            return DATA_ERR;
        }
        
        if ((log_buf = malloc(data_list->list_len * 2 + 1 + 25)) == NULL)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
            return MALLOC_ERR;
        }
        memset(log_buf, 0, data_list->list_len * 2 + 1 + 25);
        
        sprintf(log_buf, "[len = %5d][data = ", data_list->list_len);
        len_tmp = strlen(log_buf);
        
        for (i = len_tmp, list_data = data_list->head; i < data_list->list_len * 2 + len_tmp; i += 2, list_data = list_data->next)
        {
            BCD_to_string(&list_data->data, 1, tmp_buf, 2);
            log_buf[i] = tmp_buf[0];
            log_buf[i + 1] = tmp_buf[1];
        }
        log_buf[strlen(log_buf)] = ']';
    }    
    else
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "buf is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    PRINT("before printf\n");
    printf("[%s]%s[%s][%s][%05d]%s\n", common_tools.argv0, common_tools.get_datetime_buf(), file_name, function_name, line_num, log_buf);    
    //OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, log_buf, 0); 
    PRINT("after printf\n");
    if (log_buf != NULL)
    {
        PRINT("before free\n");
        PRINT("log_buf = %08X log_buf = %p\n", *log_buf, log_buf);
        free(log_buf);
        PRINT("after free\n");
        log_buf = NULL;  
    }
    PRINT("exit print_buf_by_hex\n");
    return 0;
}

/**
 * 启动指定目录的应用
 */
int start_up_application(const char *path, char *const argv[], unsigned char flag)
{
    int res = 0;
	pid_t child_pid = 0;
	
	if (path == NULL)
	{
	    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "path is NULL!", NULL_ERR);
	    return NULL_ERR;
	}
	
	if ((child_pid = fork()) < 0)
	{
	    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "fork failed!", FTOK_ERR);
		return FTOK_ERR;
	}
	PRINT("child_pid = %d\n", child_pid);
	
	if (child_pid == 0) // subprocess write
	{
	    if (execv(path, argv) < 0)
	    {
	        PERROR("exec failed!\n");
	        res = EXEC_ERR;
	        exit(res); //此处用exit，结束当前进程
	    }
	}
	else // parent process read
	{
	    if (flag == 1)
	    {
	        PRINT("flag = %d\n", flag);
	        waitpid(child_pid, &res, 0);
	    }
		else 
        {
            PRINT("flag = %d\n", flag);
            waitpid(child_pid, &res, WNOHANG);
        }
		return res;
	}
}
