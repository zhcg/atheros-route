#include "communication_serial.h"

static pthread_mutex_t mutex;                                  // 互斥量
static pthread_cond_t cond;                                    // 条件量
static struct s_data_list g_data_list;

#if RECORD_DEBUG
static unsigned char stop_record_flag = 0;
#endif

#if CTSI_SECURITY_SCHEME == 1
/**
 * /dev/ttySAC2 数据接收线程
 */
static void * serial_recv_data(void * para);

/**
 * 对/dev/ttySAC2 发送数据进行分析
 */
static void * serial_data_analyse(void * para);
#endif

#if RECORD_DEBUG
/**
 * 录音线程
 */
static void * pthread_record(void * para);
#endif

#if BOARDTYPE == 6410
/**
 * 发送挂机命令
 */
static int cmd_on_hook();

/**
 * 摘机响应
 */
static int cmd_off_hook_respond(char cmd_data);

/**
 * 发送
 */
static int send_data(unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv);
#elif BOARDTYPE == 5350 || BOARDTYPE == 9344

/**
 * 发送挂机命令
 */
static int cmd_on_hook();

/**
 * 发送挂机命令
 */
static int cmd_call(char *phone);

/**
 * 摘机响应
 */
static int cmd_off_hook();

#if CTSI_SECURITY_SCHEME == 1
/**
 * 接收来电显示
 */
static int recv_display_msg();

#else 
/**
 * 接收来电显示
 */
static int recv_display_msg(char *phone_num);
#endif // CTSI_SECURITY_SCHEME == 1

/**
 * 接收振铃
 */
static int refuse_to_answer(); // 拒接接听


#if PHONE_CHANNEL_INTERFACE == 1

#elif PHONE_CHANNEL_INTERFACE == 2


#elif PHONE_CHANNEL_INTERFACE == 3

static unsigned int spi_node_fd = 0; // spi节点描述符

/**
 * 初始化
 */
static int init();

/**
 * 释放
 */
static int release();

/**
 * 发送
 */
//static int send_data(unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv);
static int send_data(char *data, unsigned int data_len, struct timeval *tv);

/**
 * 接收
 */
//static int recv_data(unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv);
static int recv_data(char *data, unsigned int data_len, struct timeval *tv);

/**
 * 继电器切换
 */
static int relay_change(unsigned char mode);

/**
 * 回环控制
 */
static int loop_manage(unsigned char mode);

#endif // PHONE_CHANNEL_INTERFACE == 3

/**
 * 电话线检测
 */
static int phone_state_monitor(unsigned char cmd);
#endif // BOARDTYPE == 5350 || BOARDTYPE == 9344
    
struct class_communication_serial communication_serial = 
{   
    0x76,
    &mutex, &cond,
    
    #if RECORD_DEBUG
    &stop_record_flag,
    #endif
    
    #if CTSI_SECURITY_SCHEME == 1
    serial_recv_data, serial_data_analyse, 
    #endif
    
    #if RECORD_DEBUG
    pthread_record,
    #endif
    
    #if BOARDTYPE == 6410
    cmd_on_hook, cmd_off_hook_respond, 
    send_data
    #elif BOARDTYPE == 5350 || BOARDTYPE == 9344
    
    cmd_on_hook, cmd_call, cmd_off_hook, recv_display_msg,refuse_to_answer,
    phone_state_monitor, send_data, recv_data,
    #if PHONE_CHANNEL_INTERFACE == 3
    //init, release,
    relay_change,
    loop_manage,
    #endif
    #endif // BOARDTYPE == 5350 || BOARDTYPE == 9344
};

// 6410时利用/dev/ttySAC2
#if 0 
/**
 * /dev/ttySAC2 数据接收线程
 */
void * serial_recv_data(void * para)
{ 
    PRINT_STEP("entry...\n");
    pthread_mutex_lock(&mutex);
    if (common_tools.deal_attr->deal_over_bit == 1)
    {
        common_tools.deal_attr->deal_over_bit = 1;
        PRINT_STEP("exit...\n");
        pthread_mutex_unlock(&mutex);          
        pthread_exit(NULL);
    }
    int fd = 0;                 // 文件描述符，用来标识打开的设备文件
	int res = 0;                // 返回值
	unsigned char buf_tmp = 0;  // 存放 0x81 0x84
	unsigned char buf = 0;      // 存放读取的数据    
    unsigned int buf_len = 0;   // 数据接收的长度
    int write_buf_fd = 0;       // 文件描述符，用来标识打开的串口数据文件
    char print_info[128] = {0}; // 日志打印信息
    struct timeval tv = {common_tools.config->one_byte_timeout_sec + 5, common_tools.config->one_byte_timeout_usec};
    
	if ((fd = open(common_tools.config->serial_stc, O_RDWR, 0644)) < 0)
    {
        common_tools.deal_attr->deal_over_bit = 1;
        
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "open failed!", fd);  	
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    	return (void *)OPEN_ERR;
    }
    //初始化串口
	if ((res = common_tools.serial_init(fd, 9600)) < 0)
    {
        close(fd);
    	common_tools.deal_attr->deal_over_bit = 1;
            	
    	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "serial_init failed", res);         
    	pthread_cond_signal(&cond);
    	pthread_mutex_unlock(&mutex);
    	return (void *)res;
    }
	fd_set fdset;
	FD_ZERO(&fdset);
	FD_SET(fd, &fdset);
    struct timeval timeout = {common_tools.config->total_timeout, 0};
    
    // read函数不阻塞
	fcntl(fd, F_SETFL, FNDELAY);
    
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "read start!", 0);
    pthread_mutex_unlock(&mutex);
    
	while (1)
    {
        if ((common_tools.deal_attr->analysed_data_len <= g_data_list.list_len) && (g_data_list.list_len >= 7))
        {
            usleep(1);           
        }
        pthread_mutex_lock(&mutex);
        
        // 交易结束标志位判断
        if (common_tools.deal_attr->deal_over_bit == 1)
        {   
            common_tools.list_free(&g_data_list);   
            common_tools.deal_attr->deal_over_bit = 1; 
            close(fd);
            if (write_buf_fd != 0)
            {
                close(write_buf_fd);
            }
            PRINT_STEP("exit...\n");            
            pthread_mutex_unlock(&mutex); 
            pthread_exit(NULL);
        }

        // 开始计时
        if ((common_tools.deal_attr->deal_state <= 5) && (common_tools.timer->timer_bit == 0))
        {
            common_tools.timer->timer_bit = 1;
            gettimeofday(&common_tools.timer->start, NULL);
        }
        if (common_tools.timer->timer_bit == 1)
        {
            gettimeofday(&common_tools.timer->end, NULL);
            if ((common_tools.timer->end.tv_sec - common_tools.timer->start.tv_sec) >= common_tools.config->total_timeout)
            {
                close(fd);
                if (write_buf_fd != 0)
                {
                    close(write_buf_fd);
                }
                 
                if (common_tools.deal_attr->deal_state != 0)   // 已经摘机
                {   
                    //挂机
                    if ((res = cmd_on_hook(&g_data_list)) < 0)
                    {
                        common_tools.list_free(&g_data_list);
                        memset(common_tools.deal_attr, 0, sizeof(struct s_deal_attr));
                        common_tools.deal_attr->deal_over_bit = 1; 
                                                   	
                    	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_on_hook failed!", res);
                        pthread_cond_signal(&cond);
                        pthread_mutex_unlock(&mutex);
                        return (void *)res;
                    }
                }
                
                common_tools.list_free(&g_data_list);  
                memset(common_tools.deal_attr, 0, sizeof(struct s_deal_attr));
                common_tools.deal_attr->deal_over_bit = 1; 
                
                sprintf(print_info, "timeout %2d second(s)!", common_tools.config->total_timeout);
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_info, 0);
         
                pthread_cond_signal(&cond);
                pthread_mutex_unlock(&mutex);                      
                pthread_exit((void *)TIMEOUT_ERR);
            }
        }
        #if 0
        FD_ZERO(&fdset);
    	FD_SET(fd, &fdset);       
    	res = select(fd + 1, &fdset, NULL, NULL, &timeout);
    	
    	if ((res = state_select(fd, &fdset, NULL, NULL, &timeout)) < 0)
    	{
            close(fd);
            if (write_buf_fd != 0)
            {
                close(write_buf_fd);
            }
            if (common_tools.deal_attr->deal_state != 0)    // 已经摘机
            {   
                //挂机
                if ((res = cmd_on_hook(&g_data_list)) < 0)
                {
                    common_tools.list_free(&g_data_list);
                    common_tools.deal_attr->deal_over_bit = 1; 
                                       	
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_on_hook failed!", res);  
                    pthread_cond_signal(&cond);
                    pthread_mutex_unlock(&mutex);
                    return (void *)res;
                }
            }
            
            common_tools.list_free(&g_data_list);   
            common_tools.deal_attr->deal_over_bit = 1;                     	
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "state_select failed!", 0); 
        	pthread_cond_signal(&cond);
        	pthread_mutex_unlock(&mutex);
        	return (void *)S_SELECT_NULL_ERR;
        }
        
        //判断读数据是否成功
    	if (read(fd, &buf, 1) == 1)
    	#else
    	if (common_tools.recv_one_byte(fd, &buf, &tv) == 0)
    	#endif
        { 
            buf_len++;
            write_buf_fd = SERIAL_DATA_LOG(&write_buf_fd, buf, buf_len);
            
            // buf等于0x55 并且 0x55标志位没有置位
            if ((common_tools.deal_attr->sync_data_bit == 0) && (buf == 0x55))
            {   
                common_tools.deal_attr->sync_data_count++;
                if (common_tools.deal_attr->sync_data_count >= 5)
                {
                    common_tools.deal_attr->sync_data_bit = 1;
                }
            }
            else
            {
                common_tools.deal_attr->sync_data_count = 0;
            }
            
            if ((common_tools.deal_attr->sync_data_bit == 1) && (common_tools.deal_attr->list_add_bit == 0))
            {
                switch (buf)
                {
                    #if BOARDTYPE == 6410
                    case 0x04:
                    case 0x80:
                    #endif
                    case 0x81:
                    case 0x82:
                    case 0x83:
                    case 0x84:
                    case 0x87:
                    {
                        common_tools.deal_attr->list_add_bit = 1;
                        break;
                    }
                    default:
                    {
                        break;
                    }  
                }
            }
            // 0x55标志位置位时，数据添加到链表中
            if (common_tools.deal_attr->list_add_bit == 1)
            {
                if ((res = common_tools.list_tail_add_data(&g_data_list, buf)) < 0)
                {
                    common_tools.list_free(&g_data_list);    
                    common_tools.deal_attr->deal_over_bit = 1; 
                    close(fd);
                    close(write_buf_fd);
                        	
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_tail_add_data failed!", res);
                    
                    pthread_cond_signal(&cond);
                    pthread_mutex_unlock(&mutex);
                    return (void *)res;
                }
                if (g_data_list.list_len == 1)
                {      	
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list length is one!", res); 
                }
            }
        }
        
        if ((common_tools.deal_attr->analysed_data_len <= g_data_list.list_len) && (g_data_list.list_len >= 7))
        {
            pthread_cond_signal(&cond);   	
            //OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "signal ok!", 0);
        }
        pthread_mutex_unlock(&mutex);
    }
	return 0;
}

#else // 利用STUB程序接收数据

#if CTSI_SECURITY_SCHEME == 1
/**
 * 数据接收线程
 */
void * serial_recv_data(void * para)
{ 
    PRINT_STEP("entry...\n");
    pthread_mutex_lock(&mutex);
    if (common_tools.deal_attr->deal_over_bit == 1)
    {
        common_tools.deal_attr->deal_over_bit = 1;
        PRINT_STEP("exit...\n");
        pthread_mutex_unlock(&mutex);          
        pthread_exit(NULL);
    }
    
	int res = 0;                // 返回值
	unsigned char buf_tmp = 0;  // 存放 0x81 0x84
	unsigned char buf = 0;      // 存放读取的数据    
    unsigned int buf_len = 0;   // 数据接收的长度
    int write_buf_fd = 0;       // 文件描述符，用来标识打开的串口数据文件
    char print_info[128] = {0}; // 日志打印信息
      
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "read start!", res);
    pthread_mutex_unlock(&mutex);
    
	while (1)
    {
        if ((common_tools.deal_attr->analysed_data_len <= g_data_list.list_len) && (g_data_list.list_len >= 7))
        {
            usleep(1);           
        }
        pthread_mutex_lock(&mutex);
        
        // 交易结束标志位判断
        if (common_tools.deal_attr->deal_over_bit == 1)
        {   
            common_tools.list_free(&g_data_list);   
            common_tools.deal_attr->deal_over_bit = 1; 
            if (write_buf_fd != 0)
            {
                close(write_buf_fd);
            }
            PRINT_STEP("exit...\n");            
            pthread_mutex_unlock(&mutex); 
            pthread_exit(NULL);
        }

        // 开始计时
        if ((common_tools.deal_attr->deal_state <= 5) && (common_tools.timer->timer_bit == 0))
        {
            common_tools.timer->timer_bit = 1;
            gettimeofday(&common_tools.timer->start, NULL);
        }
        if (common_tools.timer->timer_bit == 1)
        {
            gettimeofday(&common_tools.timer->end, NULL);
            if ((common_tools.timer->end.tv_sec - common_tools.timer->start.tv_sec) >= common_tools.config->total_timeout)
            {
                if (write_buf_fd != 0)
                {
                    close(write_buf_fd);
                }
                 
                if (common_tools.deal_attr->deal_state != 0)   // 已经摘机
                {   
                    //挂机
                    if ((res = cmd_on_hook(&g_data_list)) < 0)
                    {
                        common_tools.list_free(&g_data_list);
                        memset(common_tools.deal_attr, 0, sizeof(struct s_deal_attr));
                        common_tools.deal_attr->deal_over_bit = 1; 
                                                   	
                    	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_on_hook failed!", res);
                        pthread_cond_signal(&cond);
                        pthread_mutex_unlock(&mutex);
                        return (void *)res;
                    }
                }
                
                common_tools.list_free(&g_data_list);  
                memset(common_tools.deal_attr, 0, sizeof(struct s_deal_attr));
                common_tools.deal_attr->deal_over_bit = 1; 
                
                sprintf(print_info, "timeout %2d second(s)!", common_tools.config->total_timeout);
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_info, 0);
         
                pthread_cond_signal(&cond);
                pthread_mutex_unlock(&mutex);                      
                pthread_exit((void *)TIMEOUT_ERR);
            }
        }

        //判断读数据是否成功
    	if (spi_rt_interface.recv_data(UART1, &buf, sizeof(buf)) == sizeof(buf))
        { 
            buf_len++;
            
            #if LOG_DEBUG
            write_buf_fd = common_tools.make_serial_data_file(&write_buf_fd, buf, buf_len);
            #endif
            
            // buf等于0x55 并且 0x55标志位没有置位
            if ((common_tools.deal_attr->sync_data_bit == 0) && (buf == 0x55))
            {   
                common_tools.deal_attr->sync_data_count++;
                if (common_tools.deal_attr->sync_data_count >= 5)
                {
                    common_tools.deal_attr->sync_data_bit = 1;
                }
            }
            else
            {
                common_tools.deal_attr->sync_data_count = 0;
            }
            
            if ((common_tools.deal_attr->sync_data_bit == 1) && (common_tools.deal_attr->list_add_bit == 0))
            {
                switch (buf)
                {
                    #if BOARDTYPE == 6410
                    case 0x04:
                    case 0x80:
                    #endif
                    case 0x81:
                    case 0x82:
                    case 0x83:
                    case 0x84:
                    case 0x87:
                    {
                        common_tools.deal_attr->list_add_bit = 1;
                        break;
                    }
                    default:
                    {
                        break;
                    }  
                }
            }
            // 0x55标志位置位时，数据添加到链表中
            if (common_tools.deal_attr->list_add_bit == 1)
            {
                if ((res = common_tools.list_tail_add_data(&g_data_list, buf)) < 0)
                {
                    common_tools.list_free(&g_data_list);    
                    common_tools.deal_attr->deal_over_bit = 1; 
                    close(write_buf_fd);
                        	
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_tail_add_data failed!", res);
                    
                    pthread_cond_signal(&cond);
                    pthread_mutex_unlock(&mutex);
                    return (void *)res;
                }
                if (g_data_list.list_len == 1)
                {      	
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list length is one!", 0); 
                }
            }
        }
        
        if ((common_tools.deal_attr->analysed_data_len <= g_data_list.list_len) && (g_data_list.list_len >= 7))
        {
            pthread_cond_signal(&cond);   	
            //OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "signal ok!", 0);
        }
        pthread_mutex_unlock(&mutex);
    }
	return 0;
}
#endif // 安全方案一

#endif

#if CTSI_SECURITY_SCHEME == 1
/**
 * 对/dev/ttySAC2 发送数据进行分析
 */
void * serial_data_analyse(void * para)
{
    PRINT_STEP("entry...\n");
    int i = 0, j = 0;
    int res = 0;
    unsigned short length = 0;
    unsigned short list_len = 0;
    char print_info[128] = {0};                     // 日志打印信息
    struct s_list_node *list_data, *list_data_tmp;    
    while (1)
    {   
        pthread_mutex_lock(&mutex);
        
        if (common_tools.deal_attr->deal_over_bit == 1)
        {
            common_tools.list_free(&g_data_list);
            common_tools.deal_attr->deal_over_bit = 1;
            PRINT_STEP("exit...\n");
            pthread_mutex_unlock(&mutex);
            pthread_exit(NULL);
            
        }
        if (g_data_list.list_len < 7)
        {
            //OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "wait start!", 0);
            
            pthread_cond_wait(&cond, &mutex);
            
            //OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "wait ok!", 0);
        }
        
        //FSK数据
        list_len = g_data_list.list_len;
        for (i = 0, list_data = g_data_list.head; i < list_len; i++, list_data = list_data_tmp)
        {
            list_data_tmp = list_data->next;
            #if 6410
            if (list_data->data == 0x80)
            {
                // 已经通话时候
                if (common_tools.deal_attr->deal_state != 0)
                {
                    common_tools.list_head_del_data(&g_data_list, list_data->data);
                    continue;
                }
                if ((g_data_list.list_len - i) < 4)
                {
                    break;
                }
                // 判断0x80之后的第二 和 第三个字节的值
                if ((list_data->next->next->data != 0x01) || (list_data->next->next->next->data != 0x08))
                {
                    common_tools.list_head_del_data(&g_data_list, list_data->data);
                    continue;
                }
                length = list_data->next->data;
                common_tools.deal_attr->analysed_data_len = length + 3;   
                if ((g_data_list.list_len - i - 3) >= length)
                {
                    
                    sprintf(print_info, "length = %d", length);
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  print_info, 0);
                    
                    common_tools.deal_attr->pack_type = 1;
                    common_tools.deal_attr->analysed_count++;
                    if ((res = communication_network.unpack_pack_send_data(&g_data_list, common_tools.deal_attr->pack_type, para)) < 0)
                    {
                        //挂机
                        if ((res = cmd_on_hook(&g_data_list)) < 0)
                        {
                            common_tools.list_free(&g_data_list);
                            common_tools.deal_attr->deal_over_bit = 1;
                             
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "cmd_on_hook failed!", res);
                            
                            pthread_cond_signal(&cond);
                            pthread_mutex_unlock(&mutex);
                            return (void *)res;
                        }
                        
                        common_tools.list_free(&g_data_list);
                        common_tools.deal_attr->deal_over_bit = 1;
                                            	
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "unpack_pack_send_data failed!", res);
                                            	
                    	pthread_cond_signal(&cond);                         
                        pthread_mutex_unlock(&mutex);
                        pthread_exit((void*)res); 
                    }
                    
                    common_tools.deal_attr->list_add_bit = 0;
                    common_tools.deal_attr->sync_data_bit = 0;
                    common_tools.deal_attr->deal_state = 1;
                    common_tools.timer->timer_bit = 0;
                    break;
                }
                else
                {
                    break;
                } 
            }
            else if (list_data->data == 0x04)
            {
                // 已经通话时候
                if (common_tools.deal_attr->deal_state != 0)
                {
                    common_tools.list_head_del_data(&g_data_list, list_data->data);
                    continue;
                }
                if ((g_data_list.list_len - i) < 2)
                {
                    break;
                }
                
                length = list_data->next->data;
                common_tools.deal_attr->analysed_data_len = length + 3;   
                if ((g_data_list.list_len - i - 3) >= length)
                {
                    sprintf(print_info, "length = %d", length);
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  print_info, 0);
                    
                    common_tools.deal_attr->pack_type = 1;
                    common_tools.deal_attr->analysed_count++;
                    if ((res = communication_network.unpack_pack_send_data(&g_data_list, common_tools.deal_attr->pack_type, para)) < 0)
                    {
                        //挂机
                        if ((res = cmd_on_hook(&g_data_list)) < 0)
                        {
                            common_tools.list_free(&g_data_list);
                            common_tools.deal_attr->deal_over_bit = 1;
                             
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "cmd_on_hook failed!", res);
                            
                            pthread_cond_signal(&cond);
                            pthread_mutex_unlock(&mutex);
                            return (void *)res;
                        }
                        
                        common_tools.list_free(&g_data_list);
                        common_tools.deal_attr->deal_over_bit = 1;
                                            	
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "unpack_pack_send_data failed!", res);
                                            	
                    	pthread_cond_signal(&cond);                         
                        pthread_mutex_unlock(&mutex);
                        pthread_exit((void*)res); 
                    }
                    
                    common_tools.deal_attr->list_add_bit = 0;
                    common_tools.deal_attr->sync_data_bit = 0;
                    common_tools.deal_attr->deal_state = 1;
                    common_tools.timer->timer_bit = 0;
                    break;
                }
                else
                {
                    break;
                } 
            }
            else if (list_data->data == 0x81)
            #else
            if (list_data->data == 0x81)
            #endif
            {
                // 中心呼叫没有来电 或者 （在接收0x83 0x84 或者 发送0x87包之后）
                if (common_tools.deal_attr->deal_state != 1)
                {
                    common_tools.list_head_del_data(&g_data_list, list_data->data);
                    continue;
                }
                if ((g_data_list.list_len - i) < 3)
                {
                    break;
                }
                // 判断0x81之后的第一个 和 第二个字节的值
                if ((list_data->next->data != 0x00) || (list_data->next->next->data != 0x05))
                {
                    common_tools.list_head_del_data(&g_data_list, list_data->data);
                    continue;
                }
                length = list_data->next->data * 256;
                length += list_data->next->next->data;
                common_tools.deal_attr->analysed_data_len = length + 4;
                // -4 四个字节的流程管理号码
                if ((g_data_list.list_len - i - 4) >= length)
                {
                    sprintf(print_info, "length = %d", length);
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  print_info, 0);
                    
                    common_tools.deal_attr->pack_type = 2;
                    common_tools.deal_attr->analysed_count++;
                    if ((res = communication_network.unpack_pack_send_data(&g_data_list, common_tools.deal_attr->pack_type, para)) < 0)
                    {
                        //挂机
                        if ((res = cmd_on_hook(common_tools.deal_attr)) < 0)
                        {   
                            common_tools.list_free(&g_data_list); 
                            common_tools.deal_attr->deal_over_bit = 1;
                             
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "cmd_on_hook failed!", res);
                            
                            pthread_cond_signal(&cond);
                            pthread_mutex_unlock(&mutex);
                            return (void *)res;
                        }
                        
                        common_tools.list_free(&g_data_list);
                        common_tools.deal_attr->deal_over_bit = 1;
                        
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "unpack_pack_send_data failed!", res);
                        
                    	pthread_cond_signal(&cond);   
                        pthread_mutex_unlock(&mutex);
                        pthread_exit((void *)res); 
                    }
                    
                    common_tools.deal_attr->list_add_bit = 0;
                    common_tools.deal_attr->sync_data_bit = 0;
                    common_tools.deal_attr->deal_state = 2;
                    common_tools.timer->timer_bit = 0;
                    break;
                }
                else
                {
                    break;
                }
            }
            else if (list_data->data == 0x83)
            {
                if (common_tools.deal_attr->deal_state != 4)
                {
                    common_tools.list_head_del_data(&g_data_list, list_data->data);
                    continue;
                }
                if ((g_data_list.list_len - i) < 3)
                {
                    break;
                }
                // 判断0x83之后的第一个 和 第二个字节的值
                if ((list_data->next->data != 0x00) || (list_data->next->next->data != 0x05))
                {
                    common_tools.list_head_del_data(&g_data_list, list_data->data);
                    continue;
                }
                length = list_data->next->data * 256;
                length += list_data->next->next->data;
                common_tools.deal_attr->analysed_data_len = length + 4;
                // -4 四个字节的流程管理号码
                if ((g_data_list.list_len - i - 4) >= length)
                {
                    sprintf(print_info, "length = %d", length);
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  print_info, 0);
                    
                    common_tools.deal_attr->pack_type = 3;
                    common_tools.deal_attr->analysed_count++;
                    if ((res = communication_network.unpack_pack_send_data(&g_data_list, common_tools.deal_attr->pack_type, para)) < 0)
                    {
                        //挂机
                        if ((res = cmd_on_hook(common_tools.deal_attr)) < 0)
                        {   
                            common_tools.list_free(&g_data_list);                 
                            common_tools.deal_attr->deal_over_bit = 1;
                             
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "cmd_on_hook failed!", res);
                    
                            pthread_cond_signal(&cond);
                            pthread_mutex_unlock(&mutex);
                            return (void *)res;
                        }
                        
                        common_tools.list_free(&g_data_list);
                        common_tools.deal_attr->deal_over_bit = 1;
                        
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "unpack_pack_send_data failed!", res);
                    	
                    	pthread_cond_signal(&cond);   
                        pthread_mutex_unlock(&mutex);
                        pthread_exit((void *)res); 
                    }
                    
                    common_tools.deal_attr->list_add_bit = 0;
                    common_tools.deal_attr->sync_data_bit = 0;
                    common_tools.deal_attr->deal_state = 3;
                    common_tools.timer->timer_bit = 0;
                    break;
                }
                else
                {
                    break;
                }
            }
            else if (list_data->data == 0x84)
            {
                // 建链之前
                if ((common_tools.deal_attr->deal_state != 2) && (common_tools.deal_attr->deal_state != 3))
                {
                    common_tools.list_head_del_data(&g_data_list, list_data->data);
                    continue;
                }
                
                if ((g_data_list.list_len - i) < 7)
                {
                    break;
                }
                
                unsigned int deal_sync_random_num = 0;
                int j = 0;
                struct s_list_node *list_data2;
                for (j = 0, list_data2 = list_data; j < 7; j++, list_data2 = list_data2->next);
                for (j = 7, list_data2 = list_data2->prev; j > 3; j--, list_data2 = list_data2->prev)
                {
                    deal_sync_random_num = (deal_sync_random_num << 8) + list_data2->data;
                }
            	if (common_tools.deal_attr->sync_random_num != deal_sync_random_num)
            	{
            	    common_tools.list_head_del_data(&g_data_list, list_data->data);
                    continue;
            	}
            	          
                length = list_data->next->data * 256;
                length += list_data->next->next->data;
                common_tools.deal_attr->analysed_data_len = length + 4;
                // -4 四个字节的流程管理号码
                if ((g_data_list.list_len - i - 4) >= length)
                { 
                    sprintf(print_info, "length = %d", length);
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  print_info, 0);
                    
                    common_tools.deal_attr->pack_type = 4;
                    common_tools.deal_attr->analysed_count++;
                    if ((res = communication_network.unpack_pack_send_data(&g_data_list, common_tools.deal_attr->pack_type, para)) < 0)
                    {
                        //挂机
                        if ((res = cmd_on_hook(common_tools.deal_attr)) < 0)
                        {    
                            common_tools.list_free(&g_data_list);                 
                            memset(common_tools.deal_attr, 0, sizeof(struct s_deal_attr));
                            common_tools.deal_attr->deal_over_bit = 1;
                             
                            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "cmd_on_hook failed!", res);

                            pthread_cond_signal(&cond);
                            pthread_mutex_unlock(&mutex);
                            return (void *)res;
                        }
                        
                        common_tools.list_free(&g_data_list);
                        memset(common_tools.deal_attr, 0, sizeof(struct s_deal_attr));
                        common_tools.deal_attr->deal_over_bit = 1;
                        
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "unpack_pack_send_data failed!", res);
                        
                    	pthread_cond_signal(&cond);   
                        pthread_mutex_unlock(&mutex);
                        pthread_exit((void *)res); 
                    }
                    
                    common_tools.deal_attr->list_add_bit = 0;
                    common_tools.deal_attr->sync_data_bit = 0;
                    common_tools.deal_attr->deal_state = 4;
                    common_tools.timer->timer_bit = 0;
                    break;
                }
                else
                {
                    break;
                }             
            }
            else //在链表的头部删除数据
            {
                /*
                if (g_data_list.list_len <= 6)
                {
                    break;
                }
                */
                //common_tools.list_head_del_data(&g_data_list, list_data->data);
            }
        }        
        pthread_mutex_unlock(&mutex);
    }
}
#endif

#if RECORD_DEBUG
/**
 * 录音线程
 */
void * pthread_record(void * para)
{
    int res = 0;
	int sum_r = 0;
	int sum_w = 0;
	int fd_r = 0, fd_w = 0;
	char record_file_name[128] = {0};
	char buf[1024] = {0};
	struct timeval tv;
	memset(&tv, 0, sizeof(struct timeval));
	
	if ((fd_r = open(SI3000, O_RDWR)) < 0) 
	{
		PERROR("open /dev/pcm0 failed!\n");
		return (void *)OPEN_ERR;
	}
	sprintf(record_file_name, "/var/terminal_init/log/%s", common_tools.get_datetime_buf());
	if ((fd_w = open(record_file_name, O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0) 
	{
		PERROR("open record.raw failed!\n");
		close(fd_r);
		return (void *)OPEN_ERR;
	}
	
	ioctl(fd_r, PCM_SET_RECORD);
//	ioctl(fd_r, PCM_SET_PLAYBACK);
	while(stop_record_flag == 0)
	{
		if((res = read(fd_r, buf, sizeof(buf))) < 0)
		{
			PERROR("read failed!\n");
			continue;
		}
		sum_r += res;
		
		if ((res = write(fd_w, buf, res)) <0)
		{
			PERROR("write err1");
			continue;
		}
		sum_w += res;
	}
	
	ioctl(fd_r, PCM_SET_UNRECORD);
//	ioctl(fd_r, PCM_SET_UNPLAYBACK);

	close(fd_r);
	close(fd_w);
	PRINT("sum_r = %d, sum_w = %d\n", sum_r, sum_w);
	return 0;
}
#endif


#if BOARDTYPE == 6410
/**
 * 发送挂机命令
 */
int cmd_on_hook()
{
    PRINT_STEP("entry...\n");
	int res = 0;
	struct s_data_list data_list;
	memset((void *)&data_list, 0, sizeof(data_list));
	
    //挂机
	unsigned char tmp = 0x21;
	common_tools.deal_attr->serial_data_type[0] = 0xBA;
	common_tools.deal_attr->serial_data_type[1] = 0xFF;
	
	if ((res = common_tools.list_tail_add_data(&data_list, tmp)) < 0)
	{    
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_tail_add_data failed!", res);
	    return res;
	}
	if ((res = send_data(NULL, &data_list, data_list.list_len, NULL)) < 0)
    {     
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
    	return res;    
    }
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 摘机响应
 */
int cmd_off_hook_respond(char cmd_data)
{
    PRINT_STEP("entry...\n");
    int res = 0;
	struct s_data_list data_list;
	memset((void *)&data_list, 0, sizeof(data_list));    
    
    //摘机许可
	common_tools.deal_attr->serial_data_type[0] = 0xBA;
	common_tools.deal_attr->serial_data_type[1] = 0x00;
	if ((res = common_tools.list_tail_add_data(&data_list, cmd_data)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_tail_add_data failed!", res);
	    return res;
	}
	if ((res = send_data(NULL, &data_list, data_list.list_len, NULL)) < 0)
    {     
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
    	return res;    
    }
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 发送
 */
int send_data(unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv)
{
    PRINT_STEP("entry...\n");
    int res = 0;
	int fd = 0;
	int i = 0;
	
	// 打开串口
    if ((fd = open(common_tools.config->serial_stc, O_WRONLY, 0644)) < 0)
    {  	
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "open failed!", res);
    	return OPEN_ERR;
    }
    
    //初始化串口
	if ((res = common_tools.serial_init(fd, common_tools.config->serial_stc_baud)) < 0)
    {          
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "serial_read_init failed!", res);
        close(fd);
    	return res;
    }
        
    //产生校验和
	char check_sum = 0;
    if ((res = common_tools.get_checkbit(data, a_data_list, 0, data_len, XOR, 1)) < 0)
    {
        close(fd);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "get_checkbit failed!", res);
        return res;
    }
    check_sum = (char)res;
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "serial data send start!", 0);
    
    //1.串口的同步数据发送
    unsigned char uart_sync[4] = {0x55, 0xAA, 0xAA, 0x55};
    if ((res = common_tools.send_data(fd, uart_sync, NULL, sizeof(uart_sync), tv)) < 0)
    {
        close(fd);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        return res;
    }
    
    //2.类型
	if ((res = common_tools.send_data(fd, common_tools.deal_attr->serial_data_type, NULL, sizeof(common_tools.deal_attr->serial_data_type), tv)) < 0)
	{
	    close(fd);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data(fd failed!", res);
        return res;
	}
	
    //3.写数据长度   
	unsigned short list_len = htons(a_data_list->list_len);
	if ((res = common_tools.send_data(fd, (char *)&list_len, NULL, sizeof(list_len), tv)) < 0)
    {
        close(fd);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        return res;
    }

    //4.发送报文
    if ((res = common_tools.send_data(fd, NULL, a_data_list, a_data_list.list_len, tv)) < 0)
    {
        close(fd);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        return res;
    }
    
    //5.发送校验和	
	if ((res = common_tools.send_one_byte(fd, &check_sum, tv)) < 0)
	{
	    close(fd);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_one_byte(fd failed!", res);
        return res;
	}
	
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "serial data send end!", res);
               
    close(fd);
    PRINT_STEP("exit...\n");
    return 0;
}

#elif BOARDTYPE == 5350 || BOARDTYPE == 9344 // 5350时

/*********************************************************************/

#if PHONE_CHANNEL_INTERFACE == 1 // 使用/dev/ttyS0 和单片机通信时

/**
 * 发送挂机命令
 */
int cmd_on_hook()
{
    PRINT_STEP("entry...\n"); 
    printf("\n");
    char buf[5] = {0xA5, 0x5A, 0x5E, 0x00, 0x5E};
    int fd = 0;
    int res = 0;
    struct timeval tv = {common_tools.config->one_byte_timeout_sec, common_tools.config->one_byte_timeout_usec};
	if ((fd = open(common_tools.config->serial_stc, O_WRONLY, 0644)) < 0)
    {  	
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "open failed!", res);
    	return OPEN_ERR;
    }
    
    //初始化串口
	if ((res = common_tools.serial_init(fd, common_tools.config->serial_stc_baud)) < 0)
    {          
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "serial_read_init failed!", res);
        close(fd);
    	return res;
    }
    
	if ((res = common_tools.send_data(fd, buf, NULL, sizeof(buf), &tv))< 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        close(fd);
    	return res; 
    }
    close(fd);
    *(common_tools.phone_status_flag) = 0;
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 呼叫
 */
int cmd_call(char *phone)
{
    PRINT_STEP("entry...\n"); 
    printf("\n");
    char buf[6] = {0xA5, 0x5A, 0x73, 0x01, 0x01, 0x73};
    
    char *send_data = NULL;
    int fd = 0;
    int res = 0;
    struct timeval tv = {common_tools.config->one_byte_timeout_sec, common_tools.config->one_byte_timeout_usec};
	if ((fd = open(common_tools.config->serial_stc, O_WRONLY, 0644)) < 0)
    {  	
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "open failed!", res);
    	return OPEN_ERR;
    }
    
    //初始化串口
	if ((res = common_tools.serial_init(fd, common_tools.config->serial_stc_baud)) < 0)
    {          
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "serial_read_init failed!", res);
        close(fd);
    	return res;
    }
    
	if ((res = common_tools.send_data(fd, buf, NULL, sizeof(buf), &tv))< 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        close(fd);
    	return res; 
    }
    
    if ((send_data = malloc(strlen(phone) + 7)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", res);
        close(fd);
    	return MALLOC_ERR; 
    }
    memset(send_data, 0, strlen(phone) + 7);
    send_data[0] = 0xA5;
    send_data[1] = 0x5A;
    send_data[2] = 0x42;
    send_data[3] = 2 + strlen(phone);
    send_data[4] = 0x01;
    send_data[5] = 0x01;
    memcpy(send_data + 6, phone, strlen(phone));
    send_data[strlen(phone) + 6] = common_tools.get_checkbit(send_data, NULL, 2, strlen(phone) + 4, XOR, 1);
    sleep(1);
    
    if ((res = common_tools.send_data(fd, send_data, NULL, strlen(phone) + 7, &tv))< 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        free(send_data);
        send_data = NULL;
        close(fd);
    	return res; 
    }
    close(fd);
    free(send_data);
    send_data = NULL;
    *(common_tools.phone_status_flag) = 1;
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 摘机
 */
int cmd_off_hook()
{
    PRINT_STEP("entry...\n");
    char buf[6] = {0xA5, 0x5A, 0x73, 0x01, 0x01, 0x73};
    int fd = 0;
    int res = 0;
    struct timeval tv = {common_tools.config->one_byte_timeout_sec, common_tools.config->one_byte_timeout_usec};
	if ((fd = open(common_tools.config->serial_stc, O_WRONLY, 0644)) < 0)
    {  	
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "open failed!", fd);
    	return OPEN_ERR;
    }
    
    //初始化串口
	if ((res = common_tools.serial_init(fd, common_tools.config->serial_stc_baud)) < 0)
    {          
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "serial_read_init failed!", res);
        close(fd);
    	return res;
    }
    
	if ((res = common_tools.send_data(fd, buf, NULL, sizeof(buf), &tv))< 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        close(fd);
    	return res; 
    }
    *(common_tools.phone_status_flag) = 1;
    close(fd);
    PRINT_STEP("exit...\n");
    return 0;
}

#if CTSI_SECURITY_SCHEME == 1
int recv_display_msg()
#else 
int recv_display_msg(char *phone_num)
#endif // CTSI_SECURITY_SCHEME == 1
{
    int res = 0;
    int i = 0;
    int j = 0;
    unsigned char pack_count = 1;
    int fd = 0;
    struct timeval tv = {common_tools.config->one_byte_timeout_sec + 15, common_tools.config->one_byte_timeout_usec};
    char head[2] = {0xA5, 0x5A};
    char cmd = 0;
    unsigned char index = 0;
    unsigned char buf_len = 0;
    char buf[256] = {0};
    char buf_tmp[128] = {0};
    char checkbit = 0;
    char recv_checkbit = 0;
    struct s_phone_message phone_message;
    memset(&phone_message, 0, sizeof(struct s_phone_message));
    if ((fd = open(common_tools.config->serial_stc, O_RDWR, 0644)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "open failed!", fd);  	
    	return OPEN_ERR;
    }
    
    //初始化串口
	if ((res = common_tools.serial_init(fd, 9600)) < 0)
    {
        close(fd); 	
    	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "serial_init failed", res);         
    	return res;
    }
    for (j = 0; j < pack_count; j++)
    {
        if ((res = common_tools.recv_data_head(fd, head, sizeof(head), &tv)) < 0)
        {
            close(fd);
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
            return res;
        }
        
        if ((res = common_tools.recv_one_byte(fd, &cmd, &tv)) < 0)
        {
            close(fd);
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_one_byte failed!", res);
            return res;
        }
        if (cmd != 0x05)
        {
            close(fd);
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data err!", DATA_ERR);
            return DATA_ERR;
        }
        
        if ((res = common_tools.recv_one_byte(fd, &buf_len, &tv)) < 0)
        {
            close(fd);
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_one_byte failed!", res);
            return res;
        }
        buf_tmp[0] = cmd;
        buf_tmp[1] = buf_len;
        for (i = 0; i < buf_len; i++)
        {
            if ((res = common_tools.recv_one_byte(fd, &buf_tmp[i + 2], &tv)) < 0)
            {
                close(fd);
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_one_byte failed!", res);
                return res;
            }
        }
        
        if ((res = common_tools.recv_one_byte(fd, &checkbit, &tv)) < 0)
        {
            close(fd);
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_one_byte failed!", res);
            return res;
        }
        
        printf("\n");
        res = common_tools.get_checkbit(buf_tmp, NULL, 0, (unsigned short)buf_len + 2, XOR, 1);
        if ((res == MISMATCH_ERR) || (res == NULL_ERR))
        {
            close(fd);
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "get_checkbit failed!", res);
            return res;
        }
        
        //PRINT("checkbit = %02X, res = %02X\n", checkbit, res);
        if (checkbit != (char)res)
        {
            close(fd);
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "check failed!", res);
            return CHECK_DIFF_ERR;
        }
        memcpy(buf + index, buf_tmp + 4, buf_len - 2);
        index += (buf_len - 2);
        pack_count = buf_tmp[2];
        memset(buf_tmp, 0, sizeof(buf_tmp));
    }
    close(fd);
    for (i = 0; i < index; i++)
    {
        switch (buf[i])
        {
            case 0x01 :
            {   
                phone_message.time_msg_len = buf[++i];
                i++;
                memcpy(phone_message.time_msg_date, buf + i, sizeof(phone_message.time_msg_date));
                i +=  sizeof(phone_message.time_msg_date);
                memcpy(phone_message.time_msg_time, buf + i, sizeof(phone_message.time_msg_time));  
                i +=  sizeof(phone_message.time_msg_time);
                break;
            }    
            case 0x02 :
            {
                phone_message.call_identify_msg_len = buf[++i];
                i++;
                memcpy(phone_message.call_identify_msg_phone_num, buf + i, phone_message.call_identify_msg_len);
                i +=  phone_message.call_identify_msg_len;
                char print_buf[128] = {0};
                sprintf(print_buf, "phone_message.call_identify_msg_phone_num = %s", phone_message.call_identify_msg_phone_num);
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                
                memset(print_buf, 0, sizeof(print_buf));
                sprintf(print_buf, "common_tools.config->center_phone = %s", common_tools.config->center_phone);
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                PRINT("%s\n", phone_message.call_identify_msg_phone_num);
                PRINT("common_tools.config->center_phone = %s\n", common_tools.config->center_phone);
                if (memcmp(phone_message.call_identify_msg_phone_num, common_tools.config->center_phone, strlen(common_tools.config->center_phone)) != 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "phone is error!", CENTER_PHONE_ERR);
                	return CENTER_PHONE_ERR;
                }
                break;
            }    
            case 0x07 :
            {
                phone_message.name_msg_len = buf[++i]; 
                i++; 
                memcpy(phone_message.name_msg_name, buf + i, phone_message.name_msg_len);
                i +=  phone_message.name_msg_len;
                break; 
            }
            default:
                break;
        }
    }
    #if 0
        else if (phone_message.news_type == 0x04)
        {
            if ((res = list_get_data(a_data_list, index, sizeof(phone_message.time_msg_date), phone_message.time_msg_date)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
                return res; 
            }
            index += sizeof(phone_message.time_msg_date);
                    
            if ((res = list_get_data(a_data_list, index, sizeof(phone_message.time_msg_time), phone_message.time_msg_time)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);                    
                return res; 
            }
            index += sizeof(phone_message.time_msg_time);
            
            if ((res = list_get_data(a_data_list, index, (phone_message.length - 8), phone_message.call_identify_msg_phone_num)) < 0)
            {                    
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);                    
                return res; 
            }
            index += (phone_message.length - 8);
                    
            char print_buf[128] = {0};
            sprintf(print_buf, "phone_message.call_identify_msg_phone_num = %s", phone_message.call_identify_msg_phone_num);
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                    
            memset(print_buf, 0, sizeof(print_buf));
            sprintf(print_buf, "g_config.center_phone = %s", g_config.center_phone);
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
            if (memcmp(phone_message.call_identify_msg_phone_num, g_config.center_phone, strlen(g_config.center_phone)) != 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "phone is error!", CENTER_PHONE_ERR);                    
                return CENTER_PHONE_ERR;
            }
            break;  
        }
    #endif
    return 0;
}

/*
 * 拒绝接听
 */
int refuse_to_answer()
{
}

#elif PHONE_CHANNEL_INTERFACE == 2  // 使用STUB程序与单片机通信时

/**
 * 发送挂机命令
 */
int cmd_on_hook()
{
    PRINT_STEP("entry...\n"); 
    printf("\n");
    int i = 0, j = 0;
    char head[2] = {0};
    unsigned char cmd = 0;
    char buf[5] = {0xA5, 0x5A, 0x5E, 0x00, 0x5E};
    char recv_buf[3] = {0};
    char phone_abnormal_buf[4] = {0};
    
    int res = 0;
    
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        if ((res = spi_rt_interface.send_data(UART1, buf, sizeof(buf)))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        	continue; 
        }
        
        for (i = 0; i < sizeof(head); i++)
        {
            if ((res = spi_rt_interface.recv_data(UART1, &head[i], sizeof(head[i]))) != sizeof(head[i]))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                return res;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        if ((res = spi_rt_interface.recv_data(UART1, &cmd, sizeof(cmd))) != sizeof(cmd))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        
        PRINT("cmd = %02X\n", cmd);
        // cmd == 0x11 为其他情况
        if (cmd == 0x11)
        {
            if ((res = spi_rt_interface.recv_data(UART1, phone_abnormal_buf, sizeof(phone_abnormal_buf))) != sizeof(phone_abnormal_buf))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                return res;
            }
            
            if ((phone_abnormal_buf[0] == 0x02) && (((phone_abnormal_buf[1] == 0x51) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x51)) 
                || ((phone_abnormal_buf[1] == 0x52) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x52)) 
                || ((phone_abnormal_buf[1] == 0x53) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x53))))
            {
                if ((res = phone_state_monitor(phone_abnormal_buf[1])) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "phone_state_monitor failed!", res);
                }
                return PHONE_LINE_ERR;
            }
            continue;
        }   
        if (cmd != 0xDE)
        {
            continue;
        }
        
        if ((res = spi_rt_interface.recv_data(UART1, recv_buf, sizeof(recv_buf))) != sizeof(recv_buf))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            continue;
        }
        if ((recv_buf[0] != 0x00) || (recv_buf[1] != 0x00) || ((unsigned char)recv_buf[2] != 0xDE))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data err!", DATA_ERR);
            continue;
        }
        break;
    }
    
    *(common_tools.phone_status_flag) = 0;
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 摘机
 */
int cmd_off_hook()
{
    PRINT_STEP("entry...\n");
    char head[2] = {0};
    unsigned char cmd = 0;
    char buf[6] = {0xA5, 0x5A, 0x73, 0x01, 0x01, 0x73};
    
    char recv_buf[5] = {0};
    char phone_abnormal_buf[4] = {0};
    int res = 0;
    int i = 0, j = 0;
    
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        if ((res = spi_rt_interface.send_data(UART1, buf, sizeof(buf)))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        	continue; 
        }
        
        *(common_tools.phone_status_flag) = 1;
        
        // A55A C1 02 02 00 00 C1
        for (i = 0; i < sizeof(head); i++)
        {
            if ((res = spi_rt_interface.recv_data(UART1, &head[i], sizeof(head[i]))) != sizeof(head[i]))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                return res;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        if ((res = spi_rt_interface.recv_data(UART1, &cmd, sizeof(cmd))) != sizeof(cmd))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        
        PRINT("cmd = %02X\n", cmd);
        // cmd == 0x11 为其他情况
        if (cmd == 0x11)
        {
            if ((res = spi_rt_interface.recv_data(UART1, phone_abnormal_buf, sizeof(phone_abnormal_buf))) != sizeof(phone_abnormal_buf))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                return res;
            }
            
            if ((phone_abnormal_buf[0] == 0x02) && (((phone_abnormal_buf[1] == 0x51) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x51)) 
                || ((phone_abnormal_buf[1] == 0x52) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x52)) 
                || ((phone_abnormal_buf[1] == 0x53) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x53))))
            {
                if ((res = phone_state_monitor(phone_abnormal_buf[1])) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "phone_state_monitor failed!", res);
                }
                return PHONE_LINE_ERR;
            }
            continue;
        }
        if (cmd != 0xC1)
        {
            continue;
        }
        
        if ((res = spi_rt_interface.recv_data(UART1, recv_buf, sizeof(recv_buf))) != sizeof(recv_buf))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        
        if ((recv_buf[0] != 0x02) || (recv_buf[1] != 0x02) || (recv_buf[2] != 0x00) || (recv_buf[3] != 0x00) || ((unsigned char)recv_buf[4] != 0xC1))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data err!", DATA_ERR);
            return DATA_ERR;
        }
        break;
    }
	if (res < 0)
	{
	    return res;
	}
    
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 发送呼叫命令
 */
int cmd_call(char *phone)
{
    PRINT_STEP("entry...\n"); 
    printf("\n");
    char head[2] = {0};
    unsigned char cmd = 0;
    char *send_data = NULL;
    unsigned char *recv_data = NULL;
    int res = 0, ret = 0;
    int i = 0, j = 0, k = 0;
    char open_reply_buf[7] = {0xA5, 0x5A, 0x70, 0x02, 0x08, 0x01, 0x7B};
    char close_reply_buf[7] = {0xA5, 0x5A, 0x70, 0x02, 0x08, 0x00, 0x7A};
    char reply_recv_buf[5] = {0};
    char phone_abnormal_buf[4] = {0};
    // 摘机
    if ((res = cmd_off_hook()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_off_hook failed!", res);
    	return res; 
    }
    
    if ((send_data = malloc(strlen(phone) + 7)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
    	return MALLOC_ERR; 
    }
    memset(send_data, 0, strlen(phone) + 7);
    
    if ((recv_data = malloc(strlen(phone) + 5)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
        free(send_data);
        send_data = NULL;
    	return MALLOC_ERR; 
    }
    memset(recv_data, 0, strlen(phone) + 5);
    
    /*
    0x42:拨号命令字
    一次性发送的呼叫号码有限制，可以设置长度（单片机）
    */
    send_data[0] = 0xA5;
    send_data[1] = 0x5A;
    send_data[2] = 0x42;
    send_data[3] = 2 + strlen(phone); // 包长度
    send_data[4] = 0x01; // 包个数： 为一，说明号码没有分包
    send_data[5] = 0x01; // 包编号
    memcpy(send_data + 6, phone, strlen(phone));
    send_data[strlen(phone) + 6] = common_tools.get_checkbit(send_data, NULL, 2, strlen(phone) + 4, XOR, 1);
    sleep(1);
    
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        // 打开参数回复
        if ((res = spi_rt_interface.send_data(UART1, open_reply_buf, sizeof(open_reply_buf)))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
         	return res; 
        }
        
        // 接收同步头
        for (i = 0; i < sizeof(head); i++)
        {
            if ((res = spi_rt_interface.recv_data(UART1, &head[i], sizeof(head[i]))) != sizeof(head[i]))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                return res;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        if ((res = spi_rt_interface.recv_data(UART1, &cmd, sizeof(cmd))) != sizeof(cmd))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            continue;
        }
        PRINT("cmd = %02X\n", cmd);
        // cmd == 0x11 为其他情况
        if (cmd == 0x11)
        {
            if ((res = spi_rt_interface.recv_data(UART1, phone_abnormal_buf, sizeof(phone_abnormal_buf))) != sizeof(phone_abnormal_buf))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                return res;
            }
            
            if ((phone_abnormal_buf[0] == 0x02) && (((phone_abnormal_buf[1] == 0x51) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x51)) 
                || ((phone_abnormal_buf[1] == 0x52) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x52)) 
                || ((phone_abnormal_buf[1] == 0x53) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x53))))
            {
                if ((res = phone_state_monitor(phone_abnormal_buf[1])) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "phone_state_monitor failed!", res);
                }
                res = PHONE_LINE_ERR;
                goto EXIT;
            }
            continue;
        }
        if (cmd != 0xF0)
        {
            res = DATA_ERR;
            continue;
        }
        
        // A5 5A F0 02 F7 FE 00 FB
        if ((res = spi_rt_interface.recv_data(UART1, reply_recv_buf, sizeof(reply_recv_buf))) != sizeof(reply_recv_buf))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
            continue;
        }
        
        if ((reply_recv_buf[0] != 0x02) || ((unsigned char)reply_recv_buf[1] != 0xF7) || ((unsigned char)reply_recv_buf[2] != 0xFE) || 
            (reply_recv_buf[3] != 0x00) || ((unsigned char)reply_recv_buf[4] != 0xFB))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
            res = DATA_ERR;
            goto EXIT;
        }
        break;
    }
    if (res < 0)
    {
        goto EXIT;
    }
    
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        // 发送号码
        if ((res = spi_rt_interface.send_data(UART1, send_data, strlen(phone) + 7))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        	continue; 
        }
        
        // 接收同步头
        for (i = 0; i < sizeof(head); i++)
        {
            if ((res = spi_rt_interface.recv_data(UART1, &head[i], sizeof(head[i]))) != sizeof(head[i]))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                return res;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        // 接收包根据号码的不同而不同 C9 + 36 = FF
        // A55A C2 0A FEFE C9CDC6CEC9C9C6C7 00 C5 接收包
        // A55A 42 0A 0101 3632393136363938    45 发送包
        if ((res = spi_rt_interface.recv_data(UART1, &cmd, sizeof(cmd))) != sizeof(cmd))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            goto EXIT;
        }
        PRINT("cmd = %02X\n", cmd);
        // cmd == 0x11 为其他情况
        if (cmd == 0x11)
        {
            if ((res = spi_rt_interface.recv_data(UART1, phone_abnormal_buf, sizeof(phone_abnormal_buf))) != sizeof(phone_abnormal_buf))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                return res;
            }
            
            if ((phone_abnormal_buf[0] == 0x02) && (((phone_abnormal_buf[1] == 0x51) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x51)) 
                || ((phone_abnormal_buf[1] == 0x52) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x52)) 
                || ((phone_abnormal_buf[1] == 0x53) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x53))))
            {
                if ((res = phone_state_monitor(phone_abnormal_buf[1])) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "phone_state_monitor failed!", res);
                }
                res = PHONE_LINE_ERR;
                goto EXIT;
            }
            continue;
        }
        if (cmd != 0xC2)
        {
            continue;
        }
        
        if ((res = spi_rt_interface.recv_data(UART1, recv_data, strlen(phone) + 5)) != (strlen(phone) + 5))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            goto EXIT;
        }
        // 不判断长度、呼叫成功标志位、校验位
        for (k = 0; k < strlen(phone) + 5 - 3; k++)
        {
            if (send_data[k + 4] + recv_data[k + 1] != 0xFF)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data err!", DATA_ERR);
                res = DATA_ERR;
                goto EXIT;
            }
        }
        
        // 不等于0x00说明呼叫失败
        PRINT("recv_data[k + 1] = %02X\n", recv_data[k + 1]);
        if (recv_data[k + 1] != 0x00)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data err!", DATA_ERR);
            res = DATA_ERR;
            goto EXIT;
        }
        break;
    }

EXIT: 
    if (send_data != NULL)
    {    
        free(send_data);
        send_data = NULL;
    }
    if (recv_data != NULL)
    {
        free(recv_data);
        recv_data = NULL;    
    }
    
    *(common_tools.phone_status_flag) = 1;
    
    // 关闭参数回复
    if ((ret = spi_rt_interface.send_data(UART1, close_reply_buf, sizeof(close_reply_buf))) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
     	return ret; 
    }
    PRINT("res = %d\n", res);
    PRINT_STEP("exit...\n");
    
    return (res < 0) ? res : 0;
}

#if CTSI_SECURITY_SCHEME == 1
int recv_display_msg()
#else 
int recv_display_msg(char *phone_num)
#endif // CTSI_SECURITY_SCHEME == 1
{
    int res = 0;
    int i = 0;
    int j = 0;
    unsigned char pack_count = 1;
    int fd = 0;
    struct timeval tv = {common_tools.config->one_byte_timeout_sec + 15, common_tools.config->one_byte_timeout_usec};
    char head[2] = {0};
    char cmd = 0;
    unsigned char index = 0;
    unsigned char buf_len = 0;
    char buf[256] = {0};
    char buf_tmp[128] = {0};
    char checkbit = 0;
    char recv_checkbit = 0;
    struct s_phone_message phone_message;
    memset(&phone_message, 0, sizeof(struct s_phone_message));
    
    unsigned char ring_count = 0;
    char ring_buf[2] = {0};
    #if CTSI_SECURITY_SCHEME == 1
    #else
    if (phone_num == NULL) 
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "phone_num is NULL!", res);
        return res;
    }
    #endif
    for (j = 0; j < pack_count; j++)
    {
        for (i = 0; i < sizeof(head); i++)
        {
            if ((res = spi_rt_interface.recv_data(UART1, &head[i], sizeof(head[i]))) != sizeof(head[i]))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                return res;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        if ((res = spi_rt_interface.recv_data(UART1, &cmd, sizeof(cmd))) != sizeof(cmd))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        PRINT("cmd = %02X\n", (unsigned char)cmd);
        // FSK 来电 、DTMF 来电 、振铃
        if ((cmd != 0x05) && (cmd != 0x04) && (cmd != 0x03))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data err!", DATA_ERR);
            return DATA_ERR;
        }
        
        if (cmd == 0x03)
        {
            if ((res = spi_rt_interface.recv_data(UART1, ring_buf, sizeof(ring_buf))) != sizeof(ring_buf))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                return res;
            }
            
            if ((ring_buf[0] != 0x00) || (ring_buf[1] != 0x03))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
                return DATA_ERR;
            }
            ring_count++;
            if (ring_count >= 5)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
                refuse_to_answer(); // 拒接来电 以后和PSTN协商
                return DATA_ERR;
            }
            memset(ring_buf, 0, sizeof(ring_buf));
            j = -1;
            continue;
        }
        
        if ((res = spi_rt_interface.recv_data(UART1, &buf_len, sizeof(buf_len))) != sizeof(buf_len))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        PRINT("buf_len = %02X\n", buf_len);
        
        buf_tmp[0] = cmd;
        buf_tmp[1] = buf_len;
        
        if ((res = spi_rt_interface.recv_data(UART1, buf_tmp + 2, buf_len)) != buf_len)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        PRINT("checkbit = %02X\n", checkbit);
        if ((res = spi_rt_interface.recv_data(UART1, &checkbit, sizeof(checkbit))) != sizeof(checkbit))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        
        printf("\n");
        res = common_tools.get_checkbit(buf_tmp, NULL, 0, (unsigned short)buf_len + 2, XOR, 1);
        if ((res == MISMATCH_ERR) || (res == NULL_ERR))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "get_checkbit failed!", res);
            return res;
        }
        
        //PRINT("checkbit = %02X, res = %02X\n", checkbit, res);
        if (checkbit != (char)res)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "check failed!", CHECK_DIFF_ERR);
            return CHECK_DIFF_ERR;
        }
        
        if (cmd == 0x05)
        {
            memcpy(buf + index, buf_tmp + 4, buf_len - 2);
            index += (buf_len - 2);
            pack_count = buf_tmp[2];
            memset(buf_tmp, 0, sizeof(buf_tmp));
        }
        else if (cmd == 0x04)
        {
            memcpy(buf, buf_tmp + 2, buf_len);
            memset(buf_tmp, 0, sizeof(buf_tmp));
            break;
        }
    }
    // DTMF
    if (cmd == 0x04)
    {
        memcpy(phone_message.call_identify_msg_phone_num, buf, buf_len);
        char print_buf[128] = {0};
        sprintf(print_buf, "phone_message.call_identify_msg_phone_num = %s", phone_message.call_identify_msg_phone_num);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                
        memset(print_buf, 0, sizeof(print_buf));
        sprintf(print_buf, "common_tools.config->center_phone = %s", common_tools.config->center_phone);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
        PRINT("%s\n", phone_message.call_identify_msg_phone_num);
        
        #if CTSI_SECURITY_SCHEME == 1
        PRINT("common_tools.config->center_phone = %s\n", common_tools.config->center_phone);
        if (memcmp(phone_message.call_identify_msg_phone_num, common_tools.config->center_phone, strlen(common_tools.config->center_phone)) != 0)
        #else
        PRINT("phone_num = %s\n", phone_num);
        if (memcmp(phone_message.call_identify_msg_phone_num, phone_num, strlen(phone_num)) != 0)
        #endif
        {            	
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "phone is error!", CENTER_PHONE_ERR);
            refuse_to_answer(); // 拒接 以后不做处理
          	return CENTER_PHONE_ERR;
        }
        return 0;
    }
    
    // FSK
    for (i = 0; i < index; i++)
    {
        switch (buf[i])
        {
            case 0x01 :
            {   
                phone_message.time_msg_len = buf[++i];
                i++;
                memcpy(phone_message.time_msg_date, buf + i, sizeof(phone_message.time_msg_date));
                i +=  sizeof(phone_message.time_msg_date);
                memcpy(phone_message.time_msg_time, buf + i, sizeof(phone_message.time_msg_time));  
                i +=  sizeof(phone_message.time_msg_time);
                break;
            }    
            case 0x02 :
            {
                phone_message.call_identify_msg_len = buf[++i];
                i++;
                memcpy(phone_message.call_identify_msg_phone_num, buf + i, phone_message.call_identify_msg_len);
                i +=  phone_message.call_identify_msg_len;
                char print_buf[128] = {0};
                sprintf(print_buf, "phone_message.call_identify_msg_phone_num = %s", phone_message.call_identify_msg_phone_num);
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                
                PRINT("%s\n", phone_message.call_identify_msg_phone_num);
                #if CTSI_SECURITY_SCHEME == 1
                memset(print_buf, 0, sizeof(print_buf));
                sprintf(print_buf, "common_tools.config->center_phone = %s", common_tools.config->center_phone);
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);                
                if (memcmp(phone_message.call_identify_msg_phone_num, common_tools.config->center_phone, strlen(common_tools.config->center_phone)) != 0)
                #else
                PRINT("phone_num = %s\n", phone_num);
                if (memcmp(phone_message.call_identify_msg_phone_num, phone_num, strlen(phone_num)) != 0)
                #endif
                {            	
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "phone is error!", CENTER_PHONE_ERR);
                    refuse_to_answer(); // 拒接 以后不做处理
                	return CENTER_PHONE_ERR;
                }
                break;
            }    
            case 0x07 :
            {
                phone_message.name_msg_len = buf[++i]; 
                i++; 
                memcpy(phone_message.name_msg_name, buf + i, phone_message.name_msg_len);
                i +=  phone_message.name_msg_len;
                break; 
            }
            default:
                break;
        }
    }

    return 0;
}

/*
 * 拒绝接听
 */
int refuse_to_answer()
{
    int res = 0, ret = 0;
    int i = 0, j = 0;
    char head[2] = {0};
    unsigned char cmd = 0;
    char open_channel_buf[6] = {0xA5, 0x5A, 0x4A, 0x01, 0x95, 0xDE};
    char close_channel_buf[6] = {0xA5, 0x5A, 0x4A, 0x01, 0x94, 0xDF};
    char open_reply_buf[7] = {0xA5, 0x5A, 0x70, 0x02, 0x08, 0x01, 0x7B};
    char close_reply_buf[7] = {0xA5, 0x5A, 0x70, 0x02, 0x08, 0x00, 0x7A};
    char reply_recv_buf[5] = {0};
    char ring_buf[2] = {0};
    char buf[4] = {0};
    char phone_abnormal_buf[4] = {0};
    
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        // 打开参数回复
        if ((res = spi_rt_interface.send_data(UART1, open_reply_buf, sizeof(open_reply_buf)))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
         	return res; 
        }
        
        // 接收同步头
        for (i = 0; i < sizeof(head); i++)
        {
            if ((res = spi_rt_interface.recv_data(UART1, &head[i], sizeof(head[i]))) != sizeof(head[i]))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                return res;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        if ((res = spi_rt_interface.recv_data(UART1, &cmd, sizeof(cmd))) != sizeof(cmd))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            continue;
        }
        PRINT("cmd = %02X\n", cmd);
        // cmd == 0x11 为其他情况
        if (cmd == 0x11)
        {
            if ((res = spi_rt_interface.recv_data(UART1, phone_abnormal_buf, sizeof(phone_abnormal_buf))) != sizeof(phone_abnormal_buf))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                return res;
            }
            
            if ((phone_abnormal_buf[0] == 0x02) && (((phone_abnormal_buf[1] == 0x51) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x51)) 
                || ((phone_abnormal_buf[1] == 0x52) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x52)) 
                || ((phone_abnormal_buf[1] == 0x53) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x53))))
            {
                if ((res = phone_state_monitor(phone_abnormal_buf[1])) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "phone_state_monitor failed!", res);
                }
                res = PHONE_LINE_ERR;
                goto EXIT;
            }
            continue;
        }
        if (cmd != 0xF0)
        {
            res = DATA_ERR;
            continue;
        }
        
        // A5 5A F0 02 F7 FE 00 FB
        if ((res = spi_rt_interface.recv_data(UART1, reply_recv_buf, sizeof(reply_recv_buf))) != sizeof(reply_recv_buf))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
            continue;
        }
        
        if ((reply_recv_buf[0] != 0x02) || ((unsigned char)reply_recv_buf[1] != 0xF7) || ((unsigned char)reply_recv_buf[2] != 0xFE) || 
            (reply_recv_buf[3] != 0x00) || ((unsigned char)reply_recv_buf[4] != 0xFB))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
            res = DATA_ERR;
            goto EXIT;
        }
        break;
    }
    if (res < 0)
    {
        goto EXIT;
    }
    
    // 第一步：打开话路 A5 5A 4A 01 95 DE
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        // 发送命令
        if ((res = spi_rt_interface.send_data(UART1, open_channel_buf, sizeof(open_channel_buf)))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        	continue; 
        }
        
        // 接收回复 A5 5A CA 01 6A 00 A1
        for (i = 0; i < sizeof(head); i++)
        {
            if ((res = spi_rt_interface.recv_data(UART1, &head[i], sizeof(head[i]))) != sizeof(head[i]))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                continue;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        if ((res = spi_rt_interface.recv_data(UART1, &cmd, sizeof(cmd))) != sizeof(cmd))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            continue;
        }
        PRINT("cmd = %02X\n", cmd);
        if (cmd == 0xCA)
        {
            if ((res = spi_rt_interface.recv_data(UART1, buf, sizeof(buf))) != sizeof(buf))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                continue;
            }
            
            if ((buf[0] != 0x01) || ((unsigned char)buf[1] != 0x6A) || (buf[2] != 0x00) || ((unsigned char)buf[3] != 0xA1))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
                res = DATA_ERR;
                continue;
            }
            
            // 成功时跳出
            break;
        }
        else if (cmd == 0x03) // 振铃
        {
            if ((res = spi_rt_interface.recv_data(UART1, ring_buf, sizeof(ring_buf))) != sizeof(ring_buf))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                goto EXIT;
            }
            
            if ((ring_buf[0] != 0x00) || (ring_buf[1] != 0x03))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
                res = DATA_ERR;
                goto EXIT;
            }
            continue;
        }
        else if (cmd == 0x11) // cmd == 0x11 为其他情况
        {
            if ((res = spi_rt_interface.recv_data(UART1, phone_abnormal_buf, sizeof(phone_abnormal_buf))) != sizeof(phone_abnormal_buf))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                return res;
            }
            
            if ((phone_abnormal_buf[0] == 0x02) && (((phone_abnormal_buf[1] == 0x51) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x51)) 
                || ((phone_abnormal_buf[1] == 0x52) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x52)) 
                || ((phone_abnormal_buf[1] == 0x53) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x53))))
            {
                if ((res = phone_state_monitor(phone_abnormal_buf[1])) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "phone_state_monitor failed!", res);
                }
                res = PHONE_LINE_ERR;
                goto EXIT;
            }
            continue;
        }
        else // 其他命令字时，重新接收
        {
            res = DATA_ERR;
            continue;
        } 
    }
    if (i == common_tools.config->repeat)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "open channel failed!", res);
        goto EXIT;
    }
    
    // 第二步：延时150ms
    usleep(150000);
    
    // 第三步：关闭话路 A5 5A 4A 01 94 DF
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        // 发送命令
        if ((res = spi_rt_interface.send_data(UART1, close_channel_buf, sizeof(close_channel_buf)))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        	continue; 
        }
        
        // 接收回复 A5 5A CA 01 6B 00 A0
        for (i = 0; i < sizeof(head); i++)
        {
            if ((res = spi_rt_interface.recv_data(UART1, &head[i], sizeof(head[i]))) != sizeof(head[i]))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                continue;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        if ((res = spi_rt_interface.recv_data(UART1, &cmd, sizeof(cmd))) != sizeof(cmd))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            continue;
        }
        
        if (cmd == 0xCA)
        {
            if ((res = spi_rt_interface.recv_data(UART1, buf, sizeof(buf))) != sizeof(buf))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                continue;
            }
            
            if ((buf[0] != 0x01) || ((unsigned char)buf[1] != 0x6B) || (buf[2] != 0x00) || ((unsigned char)buf[3] != 0xA0))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
                res = DATA_ERR;
                continue;
            }
            
            // 成功时跳出
            break;
        }
        else if (cmd == 0x03) // 振铃
        {
            if ((res = spi_rt_interface.recv_data(UART1, ring_buf, sizeof(ring_buf))) != sizeof(ring_buf))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
               goto EXIT;
            }
            
            if ((ring_buf[0] != 0x00) || (ring_buf[1] != 0x03))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
                res = DATA_ERR;
                goto EXIT;
            }
            continue;
        }
        else if (cmd == 0x11) // cmd == 0x11 为其他情况
        {
            if ((res = spi_rt_interface.recv_data(UART1, phone_abnormal_buf, sizeof(phone_abnormal_buf))) != sizeof(phone_abnormal_buf))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                return res;
            }
            
            if ((phone_abnormal_buf[0] == 0x02) && (((phone_abnormal_buf[1] == 0x51) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x51)) 
                || ((phone_abnormal_buf[1] == 0x52) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x52)) 
                || ((phone_abnormal_buf[1] == 0x53) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x53))))
            {
                if ((res = phone_state_monitor(phone_abnormal_buf[1])) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "phone_state_monitor failed!", res);
                }
                res = PHONE_LINE_ERR;
                goto EXIT;
            }
            continue;
        }
        else // 其他命令字时，重新接收
        {
            res = DATA_ERR;
            continue;
        }  
    }
    if (i == common_tools.config->repeat)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "open channel failed!", res);
        goto EXIT;
    }
 
EXIT:   
    // 关闭参数回复
    if ((ret = spi_rt_interface.send_data(UART1, close_reply_buf, sizeof(close_reply_buf)))< 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
     	return ret; 
    }
    return res;
}

int send_data(unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv)
{
    PRINT_STEP("entry...\n");
    int res = 0;
	int i = 0;
	char *send_buf = NULL;
    
    // 
    if ((send_buf = malloc(a_data_list->list_len + 5 + 1)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "malloc failed!", NULL_ERR);
        return NULL_ERR;
    }
    send_buf[0] = 0xA5;
    send_buf[1] = 0x5A;
	send_buf[2] = communication_serial.cmd;
	send_buf[3] = (char)(a_data_list->list_len);
    
    struct s_list_node *list_node;
    for (i = 4, list_node = a_data_list->head; i < (data_len + 4); list_node = list_node->next, i++)
    {
        send_buf[i] = list_node->data;
    }
    
    
    //产生校验和
	res = common_tools.get_checkbit(send_buf, NULL, 2, data_len + 2, XOR, 1);
    if ((res == MISMATCH_ERR) || (res == NULL_ERR))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "get_checkbit failed!", res);
        free(send_buf);
        send_buf = NULL;
        return res;
    }
    send_buf[data_len + 4] = (char)res;

	if ((res = spi_rt_interface.send_data(UART1, send_buf, data_len + 5)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        free(send_buf);
        send_buf = NULL;
        return res;
    }
    PRINT_BUF_BY_HEX(send_buf, NULL, data_len + 5, __FILE__, __FUNCTION__, __LINE__);
    
    free(send_buf);
    send_buf = NULL;
    PRINT_STEP("exit...\n");
    return 0;
}

#elif PHONE_CHANNEL_INTERFACE == 3  // 使用SPI节点

/**
 * 发送挂机命令
 */
int cmd_on_hook()
{
    PRINT_STEP("entry...\n"); 
    printf("\n");
    int i = 0, j = 0;
    char head[2] = {0};
    unsigned char cmd = 0;
    char buf[5] = {0xA5, 0x5A, 0x5E, 0x00, 0x5E};
    char recv_buf[3] = {0};
    char phone_abnormal_buf[4] = {0};
    
    struct timeval tv = {5, 0};
    int res = 0;
    
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        tv.tv_sec = 5;
        if ((res = send_data(buf, sizeof(buf), &tv))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        	continue; 
        }
        
        for (i = 0; i < sizeof(head); i++)
        {
            tv.tv_sec = 5;
            if ((res = recv_data(&head[i], sizeof(head[i]), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                return res;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        tv.tv_sec = 5;
        if ((res = recv_data(&cmd, sizeof(cmd), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        
        PRINT("cmd = %02X\n", cmd); 
        if (cmd != 0xDE)
        {
            continue;
        }
        
        tv.tv_sec = 5;
        if ((res = recv_data(recv_buf, sizeof(recv_buf), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            continue;
        }
        if ((recv_buf[0] != 0x00) || (recv_buf[1] != 0x00) || ((unsigned char)recv_buf[2] != 0xDE))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data err!", DATA_ERR);
            continue;
        }
        break;
    }
    
    *(common_tools.phone_status_flag) = 0;
    release();
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 摘机
 */
int cmd_off_hook()
{
    PRINT_STEP("entry...\n");
    char head[2] = {0};
    unsigned char cmd = 0;
    char buf[6] = {0xA5, 0x5A, 0x73, 0x01, 0x01, 0x73};
    
    char recv_buf[5] = {0};
    char phone_abnormal_buf[4] = {0};
    
    struct timeval tv = {5, 0};
    int res = 0;
    int i = 0, j = 0;
    
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        tv.tv_sec = 5;
        if ((res = send_data(buf, sizeof(buf), &tv))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        	continue; 
        }
        
        *(common_tools.phone_status_flag) = 1;
        
        // A55A C1 02 02 00 00 C1
        for (i = 0; i < sizeof(head); i++)
        {
            tv.tv_sec = 5;
            if ((res = recv_data(&head[i], sizeof(head[i]), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                return res;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        tv.tv_sec = 5;
        if ((res = recv_data(&cmd, sizeof(cmd), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        
        PRINT("cmd = %02X\n", cmd);
        if (cmd != 0xC1)
        {
            continue;
        }
        
        tv.tv_sec = 5;
        if ((res = recv_data(recv_buf, sizeof(recv_buf), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        
        if ((recv_buf[0] != 0x02) || (recv_buf[1] != 0x02) || (recv_buf[2] != 0x00) || (recv_buf[3] != 0x00) || ((unsigned char)recv_buf[4] != 0xC1))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data err!", DATA_ERR);
            return DATA_ERR;
        }
        break;
    }
	if (res < 0)
	{
	    return res;
	}
    
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 发送呼叫命令
 */
int cmd_call(char *phone)
{
    PRINT_STEP("entry...\n"); 
    printf("\n");
    
    struct timeval tv = {5, 0};
    char head[2] = {0};
    unsigned char cmd = 0;
    char *send_buf = NULL;
    unsigned char *recv_buf = NULL;
    int res = 0, ret = 0;
    int i = 0, j = 0, k = 0;
    char open_reply_buf[7] = {0xA5, 0x5A, 0x70, 0x02, 0x08, 0x01, 0x7B};
    char close_reply_buf[7] = {0xA5, 0x5A, 0x70, 0x02, 0x08, 0x00, 0x7A};
    char reply_recv_buf[5] = {0};
    char phone_abnormal_buf[4] = {0};
    // 摘机
    if ((res = cmd_off_hook()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_off_hook failed!", res);
    	return res; 
    }
    PRINT("res = %d\n", res);
    PRINT("phone = %s\n", phone);
    if ((send_buf = malloc(strlen(phone) + 7)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
    	return MALLOC_ERR; 
    }
    memset(send_buf, 0, strlen(phone) + 7);
    
    if ((recv_buf = malloc(strlen(phone) + 5)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
        free(send_buf);
        send_buf = NULL;
    	return MALLOC_ERR; 
    }
    memset(recv_buf, 0, strlen(phone) + 5);
    
    /*
    0x78:DTMF拨号命令字
    */
    send_buf[0] = 0xA5;
    send_buf[1] = 0x5A;
    send_buf[2] = 0x78;
    send_buf[3] = strlen(phone); // 包长度
    for (i = 0; i < strlen(phone); i++)
    {
        send_buf[i + 4] = phone[i] - '0';
    }
    // memcpy(send_buf + 4, phone, strlen(phone));
    send_buf[strlen(phone) + 4] = common_tools.get_checkbit(send_buf, NULL, 2, strlen(phone) + 2, XOR, 1);
    sleep(1);
    
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        tv.tv_sec = 5;
        // 打开参数回复
        if ((res = send_data(open_reply_buf, sizeof(open_reply_buf), &tv))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
         	return res; 
        }
        
        // 接收同步头
        for (i = 0; i < sizeof(head); i++)
        {
            tv.tv_sec = 5;
            if ((res = recv_data(&head[i], sizeof(head[i]), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                return res;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        tv.tv_sec = 5;
        if ((res = recv_data(&cmd, sizeof(cmd), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            continue;
        }
        PRINT("cmd = %02X\n", cmd);
        if (cmd != 0xF0)
        {
            res = DATA_ERR;
            continue;
        }
        
        tv.tv_sec = 5;
        // A5 5A F0 02 F7 FE 00 FB
        if ((res = recv_data(reply_recv_buf, sizeof(reply_recv_buf), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
            continue;
        }
        
        if ((reply_recv_buf[0] != 0x02) || ((unsigned char)reply_recv_buf[1] != 0xF7) || ((unsigned char)reply_recv_buf[2] != 0xFE) || 
            (reply_recv_buf[3] != 0x00) || ((unsigned char)reply_recv_buf[4] != 0xFB))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
            res = DATA_ERR;
            goto EXIT;
        }
        break;
    }
    if (res < 0)
    {
        goto EXIT;
    }
    
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        tv.tv_sec = 5;
        // 发送号码
        if ((res = send_data(send_buf, strlen(phone) + 5, &tv))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        	continue; 
        }
        
        // 接收同步头
        for (i = 0; i < sizeof(head); i++)
        {
            tv.tv_sec = 5;
            if ((res = recv_data(&head[i], sizeof(head[i]), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                return res;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        tv.tv_sec = 5;
        // 接收包根据号码的不同而不同 C9 + 36 = FF
        // A55A C2 0A FEFE C9CDC6CEC9C9C6C7 00 C5 接收包
        // A55A 42 0A 0101 3632393136363938    45 发送包
        if ((res = recv_data(&cmd, sizeof(cmd), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            goto EXIT;
        }
        PRINT("cmd = %02X\n", cmd);
        if (cmd != 0xC2)
        {
            continue;
        }
        
        tv.tv_sec = 5;
        if ((res = recv_data(recv_buf, strlen(phone) + 5, &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            goto EXIT;
        }
        // 不判断长度、呼叫成功标志位、校验位
        for (k = 0; k < strlen(phone) + 5 - 3; k++)
        {
            if (send_buf[k + 4] + recv_buf[k + 1] != 0xFF)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data err!", DATA_ERR);
                res = DATA_ERR;
                goto EXIT;
            }
        }
        
        // 不等于0x00说明呼叫失败
        PRINT("recv_buf[k + 1] = %02X\n", recv_buf[k + 1]);
        if (recv_buf[k + 1] != 0x00)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data err!", DATA_ERR);
            res = DATA_ERR;
            goto EXIT;
        }
        break;
    }

EXIT: 
    if (send_buf != NULL)
    {    
        free(send_buf);
        send_buf = NULL;
    }
    if (recv_buf != NULL)
    {
        free(recv_buf);
        recv_buf = NULL;    
    }
    
    *(common_tools.phone_status_flag) = 1;
    
    // 关闭参数回复
    tv.tv_sec = 5;
    if ((ret = send_data(close_reply_buf, sizeof(close_reply_buf), &tv)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
     	return ret; 
    }
    PRINT("res = %d\n", res);
    PRINT_STEP("exit...\n");
    
    return (res < 0) ? res : 0;
}

#if CTSI_SECURITY_SCHEME == 1
int recv_display_msg()
#else 
int recv_display_msg(char *phone_num)
#endif // CTSI_SECURITY_SCHEME == 1
{
    int res = 0;
    int i = 0;
    int j = 0;
    unsigned char pack_count = 1;
    int fd = 0;
    struct timeval tv = {common_tools.config->one_byte_timeout_sec + 15, common_tools.config->one_byte_timeout_usec};
    char head[2] = {0};
    char cmd = 0;
    unsigned char index = 0;
    unsigned char buf_len = 0;
    char buf[256] = {0};
    char buf_tmp[128] = {0};
    char checkbit = 0;
    char recv_checkbit = 0;
    struct s_phone_message phone_message;
    memset(&phone_message, 0, sizeof(struct s_phone_message));
    
    unsigned char ring_count = 0;
    char ring_buf[2] = {0};
    #if CTSI_SECURITY_SCHEME == 1
    #else
    if (phone_num == NULL) 
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "phone_num is NULL!", res);
        return res;
    }
    #endif
    for (j = 0; j < pack_count; j++)
    {
        for (i = 0; i < sizeof(head); i++)
        {
            if ((res = recv_data(&head[i], sizeof(head[i]), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                return res;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        if ((res = recv_data(&cmd, sizeof(cmd), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        PRINT("cmd = %02X\n", (unsigned char)cmd);
        // FSK 来电 、DTMF 来电 、振铃
        if ((cmd != 0x05) && (cmd != 0x04) && (cmd != 0x03))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data err!", DATA_ERR);
            return DATA_ERR;
        }
        
        if (cmd == 0x03)
        {
            if ((res = recv_data(ring_buf, sizeof(ring_buf), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                return res;
            }
            
            if ((ring_buf[0] != 0x00) || (ring_buf[1] != 0x03))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
                return DATA_ERR;
            }
            ring_count++;
            if (ring_count >= 5)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
                refuse_to_answer(); // 拒接来电 以后和PSTN协商
                return DATA_ERR;
            }
            memset(ring_buf, 0, sizeof(ring_buf));
            j = -1;
            continue;
        }
        
        if ((res = recv_data(&buf_len, sizeof(buf_len), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        PRINT("buf_len = %02X\n", buf_len);
        
        buf_tmp[0] = cmd;
        buf_tmp[1] = buf_len;
        
        if ((res = recv_data(buf_tmp + 2, buf_len, &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        
        if ((res = recv_data(&checkbit, sizeof(checkbit), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        
        printf("\n");
        res = common_tools.get_checkbit(buf_tmp, NULL, 0, (unsigned short)buf_len + 2, XOR, 1);
        if ((res == MISMATCH_ERR) || (res == NULL_ERR))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "get_checkbit failed!", res);
            return res;
        }
        
        PRINT("checkbit = %02X, res = %02X\n", checkbit, res);
        if (checkbit != (char)res)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "check failed!", CHECK_DIFF_ERR);
            return CHECK_DIFF_ERR;
        }
        
        if (cmd == 0x05)
        {
            memcpy(buf + index, buf_tmp + 4, buf_len - 2);
            index += (buf_len - 2);
            pack_count = buf_tmp[2];
            memset(buf_tmp, 0, sizeof(buf_tmp));
        }
        else if (cmd == 0x04)
        {
            memcpy(buf, buf_tmp + 2, buf_len);
            memset(buf_tmp, 0, sizeof(buf_tmp));
            break;
        }
    }
    // DTMF
    if (cmd == 0x04)
    {
        memcpy(phone_message.call_identify_msg_phone_num, buf, buf_len);
        char print_buf[128] = {0};
        sprintf(print_buf, "phone_message.call_identify_msg_phone_num = %s", phone_message.call_identify_msg_phone_num);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                
        memset(print_buf, 0, sizeof(print_buf));
        sprintf(print_buf, "common_tools.config->center_phone = %s", common_tools.config->center_phone);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
        PRINT("%s\n", phone_message.call_identify_msg_phone_num);
        
        #if CTSI_SECURITY_SCHEME == 1
        PRINT("common_tools.config->center_phone = %s\n", common_tools.config->center_phone);
        if (memcmp(phone_message.call_identify_msg_phone_num, common_tools.config->center_phone, strlen(common_tools.config->center_phone)) != 0)
        #else
        PRINT("phone_num = %s\n", phone_num);
        if (memcmp(phone_message.call_identify_msg_phone_num, phone_num, strlen(phone_num)) != 0)
        #endif
        {            	
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "phone is error!", CENTER_PHONE_ERR);
            refuse_to_answer(); // 拒接 以后不做处理
          	return CENTER_PHONE_ERR;
        }
        return 0;
    }
    
    // FSK
    for (i = 0; i < index; i++)
    {
        switch (buf[i])
        {
            case 0x01 :
            {   
                phone_message.time_msg_len = buf[++i];
                i++;
                memcpy(phone_message.time_msg_date, buf + i, sizeof(phone_message.time_msg_date));
                i +=  sizeof(phone_message.time_msg_date);
                memcpy(phone_message.time_msg_time, buf + i, sizeof(phone_message.time_msg_time));  
                i +=  sizeof(phone_message.time_msg_time);
                break;
            }    
            case 0x02 :
            {
                phone_message.call_identify_msg_len = buf[++i];
                i++;
                memcpy(phone_message.call_identify_msg_phone_num, buf + i, phone_message.call_identify_msg_len);
                i +=  phone_message.call_identify_msg_len;
                char print_buf[128] = {0};
                sprintf(print_buf, "phone_message.call_identify_msg_phone_num = %s", phone_message.call_identify_msg_phone_num);
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                
                PRINT("%s\n", phone_message.call_identify_msg_phone_num);
                #if CTSI_SECURITY_SCHEME == 1
                memset(print_buf, 0, sizeof(print_buf));
                sprintf(print_buf, "common_tools.config->center_phone = %s", common_tools.config->center_phone);
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);                
                if (memcmp(phone_message.call_identify_msg_phone_num, common_tools.config->center_phone, strlen(common_tools.config->center_phone)) != 0)
                #else
                PRINT("phone_num = %s\n", phone_num);
                if (memcmp(phone_message.call_identify_msg_phone_num, phone_num, strlen(phone_num)) != 0)
                #endif
                {            	
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "phone is error!", CENTER_PHONE_ERR);
                    refuse_to_answer(); // 拒接 以后不做处理
                	return CENTER_PHONE_ERR;
                }
                break;
            }    
            case 0x07 :
            {
                phone_message.name_msg_len = buf[++i]; 
                i++; 
                memcpy(phone_message.name_msg_name, buf + i, phone_message.name_msg_len);
                i +=  phone_message.name_msg_len;
                break; 
            }
            default:
                break;
        }
    }

    return 0;
}

/*
 * 拒绝接听
 */
int refuse_to_answer()
{
    int res = 0, ret = 0;
    int i = 0, j = 0;
    char head[2] = {0};
    unsigned char cmd = 0;
    char open_channel_buf[6] = {0xA5, 0x5A, 0x4A, 0x01, 0x95, 0xDE};
    char close_channel_buf[6] = {0xA5, 0x5A, 0x4A, 0x01, 0x94, 0xDF};
    char open_reply_buf[7] = {0xA5, 0x5A, 0x70, 0x02, 0x08, 0x01, 0x7B};
    char close_reply_buf[7] = {0xA5, 0x5A, 0x70, 0x02, 0x08, 0x00, 0x7A};
    char reply_recv_buf[5] = {0};
    char ring_buf[2] = {0};
    char buf[4] = {0};
    char phone_abnormal_buf[4] = {0};
    struct timeval tv = {5, 0};
    
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        tv.tv_sec = 5;
        // 打开参数回复
        if ((res = send_data(open_reply_buf, sizeof(open_reply_buf), &tv))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
         	return res; 
        }
        
        // 接收同步头
        for (i = 0; i < sizeof(head); i++)
        {
            tv.tv_sec = 5;
            if ((res = recv_data(&head[i], sizeof(head[i]), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                return res;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        tv.tv_sec = 5;
        if ((res = recv_data(&cmd, sizeof(cmd), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            continue;
        }
        PRINT("cmd = %02X\n", cmd);
        if (cmd != 0xF0)
        {
            res = DATA_ERR;
            continue;
        }
        
        tv.tv_sec = 5;
        // A5 5A F0 02 F7 FE 00 FB
        if ((res = recv_data(reply_recv_buf, sizeof(reply_recv_buf), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
            continue;
        }
        
        if ((reply_recv_buf[0] != 0x02) || ((unsigned char)reply_recv_buf[1] != 0xF7) || ((unsigned char)reply_recv_buf[2] != 0xFE) || 
            (reply_recv_buf[3] != 0x00) || ((unsigned char)reply_recv_buf[4] != 0xFB))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
            res = DATA_ERR;
            goto EXIT;
        }
        break;
    }
    if (res < 0)
    {
        goto EXIT;
    }
    
    // 第一步：打开话路 A5 5A 4A 01 95 DE
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        tv.tv_sec = 5;
        // 发送命令
        if ((res = send_data(open_channel_buf, sizeof(open_channel_buf), &tv))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        	continue; 
        }
        
        // 接收回复 A5 5A CA 01 6A 00 A1
        for (i = 0; i < sizeof(head); i++)
        {
            tv.tv_sec = 5;
            if ((res = recv_data(&head[i], sizeof(head[i]), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                continue;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        tv.tv_sec = 5;
        if ((res = recv_data(&cmd, sizeof(cmd), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            continue;
        }
        PRINT("cmd = %02X\n", cmd);
        if (cmd == 0xCA)
        {
            tv.tv_sec = 5;
            if ((res = recv_data(buf, sizeof(buf), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                continue;
            }
            
            if ((buf[0] != 0x01) || ((unsigned char)buf[1] != 0x6A) || (buf[2] != 0x00) || ((unsigned char)buf[3] != 0xA1))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
                res = DATA_ERR;
                continue;
            }
            
            // 成功时跳出
            break;
        }
        else if (cmd == 0x03) // 振铃
        {
            tv.tv_sec = 5;
            if ((res = recv_data(ring_buf, sizeof(ring_buf), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                goto EXIT;
            }
            
            if ((ring_buf[0] != 0x00) || (ring_buf[1] != 0x03))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
                res = DATA_ERR;
                goto EXIT;
            }
            continue;
        }
        else // 其他命令字时，重新接收
        {
            res = DATA_ERR;
            continue;
        } 
    }
    if (i == common_tools.config->repeat)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "open channel failed!", res);
        goto EXIT;
    }
    
    // 第二步：延时150ms
    usleep(150000);
    
    // 第三步：关闭话路 A5 5A 4A 01 94 DF
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        tv.tv_sec = 5;
        // 发送命令
        if ((res = send_data(close_channel_buf, sizeof(close_channel_buf), &tv))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        	continue; 
        }
        
        // 接收回复 A5 5A CA 01 6B 00 A0
        for (i = 0; i < sizeof(head); i++)
        {
            tv.tv_sec = 5;
            if ((res = recv_data(&head[i], sizeof(head[i]), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                continue;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        tv.tv_sec = 5;
        if ((res = recv_data(&cmd, sizeof(cmd), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            continue;
        }
        
        if (cmd == 0xCA)
        {
            tv.tv_sec = 5;
            if ((res = recv_data(buf, sizeof(buf), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                continue;
            }
            
            if ((buf[0] != 0x01) || ((unsigned char)buf[1] != 0x6B) || (buf[2] != 0x00) || ((unsigned char)buf[3] != 0xA0))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
                res = DATA_ERR;
                continue;
            }
            
            // 成功时跳出
            break;
        }
        else if (cmd == 0x03) // 振铃
        {
            tv.tv_sec = 5;
            if ((res = recv_data(ring_buf, sizeof(ring_buf), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                goto EXIT;
            }
            
            if ((ring_buf[0] != 0x00) || (ring_buf[1] != 0x03))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
                res = DATA_ERR;
                goto EXIT;
            }
            continue;
        }
        else // 其他命令字时，重新接收
        {
            res = DATA_ERR;
            continue;
        }  
    }
    if (i == common_tools.config->repeat)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "open channel failed!", res);
        goto EXIT;
    }
 
EXIT:   
    // 关闭参数回复
    if ((ret = send_data(close_reply_buf, sizeof(close_reply_buf), &tv))< 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
     	return ret; 
    }
    return res;
}


/**
 * 初始化
 */
int init()
{
    int res = 0;
    if (spi_node_fd != 0)
    {
        close(spi_node_fd);
        spi_node_fd = 0;
    }
    
    if ((res = open(SPI_NODE, O_RDWR, 0644)) < 0)
    {  	
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "open failed!", OPEN_ERR);
    	return OPEN_ERR;
    }
    spi_node_fd = res;
    PRINT("spi_node_fd = %d\n", spi_node_fd);
    return 0;
}

/**
 * 释放
 */
int release()
{
    if (spi_node_fd != 0)
    {
        close(spi_node_fd);
        spi_node_fd = 0;
    }
    return 0;
}

int send_data(char *data, unsigned int data_len, struct timeval *tv)
{
    PRINT_STEP("entry...\n");
    int res = 0;
    
    if (data == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    if (spi_node_fd == 0)
    {
        if ((res = init()) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "init failed!", res);
            return res;
        }
    }
	if ((res = common_tools.send_data(spi_node_fd, data, NULL, data_len, tv)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        release();
        return res;
    }
    PRINT_STEP("exit...\n");
    return 0;
}


/**
 * 接收
 */
int recv_data(char *data, unsigned int data_len, struct timeval *tv)
{
    int res = 0;
    
    if (data == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    if (spi_node_fd == 0)
    {
        if ((res = init()) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "init failed!", res);
            return res;
        }
    }
    
    if ((res = common_tools.recv_data(spi_node_fd, data, NULL, data_len, tv)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_data failed!", res);
        release();
    	return res;
    }
    return res;
}


/**
 * 切换继电器
 */
int relay_change(unsigned char mode)
{
    // A5 5A 4A 01 94 DF  切到CACM
    // A5 5A 4A 01 95 DE  切到打电话 
    int res = 0;
    char send_cacm_buf[6] = {0xA5, 0x5A, 0x4A, 0x01, 0x94, 0xDF};
    char send_phone_buf[6] = {0xA5, 0x5A, 0x4A, 0x01, 0x95, 0xDE};
    struct timeval tv = {5, 0};
    
    if (mode == 1) // 从95R54切换到SI32919
    {
        if ((res = send_data(send_phone_buf, sizeof(send_phone_buf), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
            return res;
        }
    }
    else if (mode == 0) // 从SI32919切换到95R54
    {
        if ((res = send_data(send_cacm_buf, sizeof(send_cacm_buf), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
            return res;
        }
    }
    else
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", DATA_ERR);
        return DATA_ERR;
    }
    return 0;
}

/**
 * 回环控制
 */
int loop_manage(unsigned char mode)
{
    // A5 5A 70 02 09 00 7B 关闭回环
    // A5 5A 70 02 09 01 7A 打开回环 
    int res = 0;
    char loop_close_buf[7] = {0xA5, 0x5A, 0x70, 0x02, 0x09, 0x00, 0x7B};
    char loop_open_buf[7] = {0xA5, 0x5A, 0x70, 0x02, 0x09, 0x01, 0x7A};
    struct timeval tv = {5, 0};
    
    if (mode == 1) // 打开回环 
    {
        if ((res = send_data(loop_open_buf, sizeof(loop_open_buf), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
            return res;
        }
    }
    else if (mode == 0) // 关闭回环
    {
        if ((res = send_data(loop_close_buf, sizeof(loop_close_buf), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
            return res;
        }
    }
    else
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", DATA_ERR);
        return DATA_ERR;
    }
    return 0;
}
#endif // PHONE_CHANNEL_INTERFACE == 3

/**
 * 电话线检测
 */
int phone_state_monitor(unsigned char cmd)
{
    int res = 0;
    int fd = 0;
    struct timeval tv = {3, 0};
    char send_buf[6] = {0x5A, 0xA5, 0x02, 0x11};
    send_buf[4] = cmd;
    send_buf[5] = cmd ^ 0x11;
    
     // 创建本地socket，发送电话线状态到CACM
    if ((fd = communication_network.make_client_link("127.0.0.1", CACM_SOCKCT_PORT)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "make_client_link failed!", fd);
        return fd;
    }
    
    if ((res = common_tools.send_data(fd, send_buf, NULL, sizeof(send_buf), &tv)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        close(fd);
    	return res;
    }
    usleep(10);
    close(fd);
    return 0;
}

#endif //  BOARDTYPE == 5350 || BOARDTYPE == 9344

