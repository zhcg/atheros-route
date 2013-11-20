#include "communication_stc.h"

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
#else

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
#endif
/**
 * 发送
 */
static int send_data(unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv);
#endif

/**
 * 接收
 */
static int recv_data(unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv);
    
struct class_communication_stc communication_stc = 
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
    #else
    cmd_on_hook, cmd_call, cmd_off_hook, recv_display_msg,
    send_data
    #endif
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
                    if ((res = internetwork_communication.unpack_pack_send_data(&g_data_list, common_tools.deal_attr->pack_type, para)) < 0)
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
                    if ((res = internetwork_communication.unpack_pack_send_data(&g_data_list, common_tools.deal_attr->pack_type, para)) < 0)
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
                    if ((res = internetwork_communication.unpack_pack_send_data(&g_data_list, common_tools.deal_attr->pack_type, para)) < 0)
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
                    if ((res = internetwork_communication.unpack_pack_send_data(&g_data_list, common_tools.deal_attr->pack_type, para)) < 0)
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
                    if ((res = internetwork_communication.unpack_pack_send_data(&g_data_list, common_tools.deal_attr->pack_type, para)) < 0)
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

#else // 5350时

/*********************************************************************/
// 使用/dev/ttyS0 和单片机通信时
#if 0
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
    send_data[3] = 0x06;
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

int recv_display_msg()
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

#else 
// 使用STUB程序与单片机通信时

/**
 * 发送挂机命令
 */
int cmd_on_hook()
{
    PRINT_STEP("entry...\n"); 
    printf("\n");
    int i = 0;
    char head[2] = {0};
    char buf[5] = {0xA5, 0x5A, 0x5E, 0x00, 0x5E};
    int res = 0;
    
	if ((res = spi_rt_interface.send_data(UART1, buf, sizeof(buf)))< 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
    	return res; 
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
    
    memset(buf, 0, sizeof(buf));
    if ((res = spi_rt_interface.recv_data(UART1, buf, 4)) != 4)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
        return res;
    }
    
    if (((unsigned char)buf[0] != 0xDE) || (buf[1] != 0x00) || (buf[2] != 0x00) || ((unsigned char)buf[3] != 0xDE))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data err!", DATA_ERR);
        return DATA_ERR;
    }
    *(common_tools.phone_status_flag) = 0;
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
    char buf[6] = {0xA5, 0x5A, 0x73, 0x01, 0x01, 0x73};
    char *send_data = NULL;
    int res = 0;
    
    if ((res = spi_rt_interface.send_data(UART1, buf, sizeof(buf)))< 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
    	return res; 
    }
    if ((send_data = malloc(strlen(phone) + 7)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
    	return MALLOC_ERR; 
    }
    memset(send_data, 0, strlen(phone) + 7);
    send_data[0] = 0xA5;
    send_data[1] = 0x5A;
    send_data[2] = 0x42;
    send_data[3] = 0x06;
    send_data[4] = 0x01;
    send_data[5] = 0x01;
    memcpy(send_data + 6, phone, strlen(phone));
    send_data[strlen(phone) + 6] = common_tools.get_checkbit(send_data, NULL, 2, strlen(phone) + 4, XOR, 1);
    sleep(1);
    
    if ((res = spi_rt_interface.send_data(UART1, send_data, strlen(phone) + 7))< 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        free(send_data);
        send_data = NULL;
    	return res; 
    }
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
    int res = 0;
    
	if ((res = spi_rt_interface.send_data(UART1, buf, sizeof(buf)))< 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
    	return res; 
    }
    
    *(common_tools.phone_status_flag) = 1;
    PRINT_STEP("exit...\n");
    return 0;
}

#if CTSI_SECURITY_SCHEME == 1
int recv_display_msg()
#else 
int recv_display_msg(char *phone_num)
#endif
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
        if ((cmd != 0x05) && (cmd != 0x04))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data err!", DATA_ERR);
            return DATA_ERR;
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
        if (memcmp(phone_message.call_identify_msg_phone_num, phone_num, strlen(phone_num)) != 0)
        #endif
        {            	
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "phone is error!", CENTER_PHONE_ERR);
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
                
                memset(print_buf, 0, sizeof(print_buf));
                sprintf(print_buf, "common_tools.config->center_phone = %s", common_tools.config->center_phone);
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                PRINT("%s\n", phone_message.call_identify_msg_phone_num);
                #if CTSI_SECURITY_SCHEME == 1
                PRINT("common_tools.config->center_phone = %s\n", common_tools.config->center_phone);
                if (memcmp(phone_message.call_identify_msg_phone_num, common_tools.config->center_phone, strlen(common_tools.config->center_phone)) != 0)
                #else
                if (memcmp(phone_message.call_identify_msg_phone_num, phone_num, strlen(phone_num)) != 0)
                #endif
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

    return 0;
}
#endif


#if 0
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
    
    //产生校验和
	char check_sum = 0;
	res = common_tools.get_checkbit(data, a_data_list, 0, data_len, XOR, 1);
    if ((res == MISMATCH_ERR) || (res == NULL_ERR))
    {
        close(fd);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "get_checkbit failed!", res);
        return res;
    }
    
    check_sum = (char)res;
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "serial data send start!", 0);
    
    //1.串口的同步数据发送
    unsigned char uart_sync[2] = {0xA5, 0x5A};
    if ((res = common_tools.send_data(fd, uart_sync, NULL, sizeof(uart_sync), tv)) < 0)
    {
        close(fd);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        return res;
    }
    
    //2.类型
    PRINT("[communication_stc.cmd = %02X]\n", communication_stc.cmd);
	if ((res = common_tools.send_one_byte(fd, &(communication_stc.cmd), tv)) < 0)
	{
	    close(fd);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_one_byte failed!", res);
        return res;
	}
	#if 0
    //3.写数据长度   
	unsigned short list_len = htons(a_data_list->list_len);
	if ((res = common_tools.send_data(fd, (char *)&list_len, NULL, sizeof(list_len), tv)) < 0)
    {
        close(fd);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        return res;
    }
    #else
    unsigned char list_len = (char)(a_data_list->list_len);
    PRINT("[list_len = %02X]\n", list_len);
	if ((res = common_tools.send_one_byte(fd, &list_len, tv)) < 0)
    {
        close(fd);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_one_byte failed!", res);
        return res;
    }
    #endif
    
    //4.发送报文
    if ((res = common_tools.send_data(fd, NULL, a_data_list, a_data_list->list_len, tv)) < 0)
    {
        close(fd);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        return res;
    }
    
    //5.发送校验和
    check_sum ^= communication_stc.cmd;
    check_sum ^= list_len;
    PRINT("[check_sum = %02X]\n", (unsigned char)check_sum);
	if ((res = common_tools.send_one_byte(fd, &check_sum, tv)) < 0)
	{
	    close(fd);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_one_byte(fd failed!", res);
        return res;
	}
	
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "serial data send end!", 0);          
    close(fd);
    PRINT_STEP("exit...\n");
    return 0;
}
int send_data(unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv)
{
    PRINT_STEP("entry...\n");
    int res = 0;
	int fd = 0;
	int i = 0;
	struct s_data_list data_list;
	memset((void *)&data_list, 0, sizeof(struct s_data_list));
	
	// 打开串口
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
    
    //产生校验和
	char check_sum = 0;
	res = common_tools.get_checkbit(data, a_data_list, 0, data_len, XOR, 1);
    if ((res == MISMATCH_ERR) || (res == NULL_ERR))
    {
        close(fd);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "get_checkbit failed!", res);
        return res;
    }
    
    check_sum = (char)res;
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "serial data send start!", 0);
    
    //1.串口的同步数据发送
    if ((res = common_tools.list_tail_add_data(&data_list, 0xA5)) < 0)
	{
	    close(fd);
	    common_tools.list_free(&data_list); 
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_tail_add_data failed!", res);
	    return res;
	}
	if ((res = common_tools.list_tail_add_data(&data_list, 0x5A)) < 0)
	{
	    close(fd);
	    common_tools.list_free(&data_list); 
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_tail_add_data failed!", res);
	    return res;
	}
	
    //2.类型
	if ((res = common_tools.list_tail_add_data(&data_list, communication_stc.cmd)) < 0)
	{
	    close(fd);
	    common_tools.list_free(&data_list); 
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_tail_add_data failed!", res);
	    return res;
	}
	
    //3.写数据长度 
    unsigned char list_len = (char)(a_data_list->list_len);
    if ((res = common_tools.list_tail_add_data(&data_list, list_len)) < 0)
	{
	    close(fd);
	    common_tools.list_free(&data_list); 
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_tail_add_data failed!", res);
	    return res;
	}
    
    //4.发送报文
    struct s_list_node *list_node;
    for (i = 0, list_node = a_data_list->head; i < data_len; list_node = list_node->next, i++)
    {
        if ((res = common_tools.list_tail_add_data(&data_list, list_node->data)) < 0)
    	{
    	    close(fd);
    	    common_tools.list_free(&data_list); 
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_tail_add_data failed!", res);
    	    return res;
    	}
    }
    
    //5.发送校验和
    check_sum ^= communication_stc.cmd;
    check_sum ^= list_len;
    if ((res = common_tools.list_tail_add_data(&data_list, check_sum)) < 0)
	{
	    close(fd);
	    common_tools.list_free(&data_list); 
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_tail_add_data failed!", res);
	    return res;
	}

	if ((res = common_tools.send_data(fd, NULL, &data_list, data_list.list_len, tv)) < 0)
    {
        close(fd);
        common_tools.list_free(&data_list); 
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        return res;
    }
    
    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "serial data send end!", 0);   
    common_tools.list_free(&data_list);       
    close(fd);
    PRINT_STEP("exit...\n");
    return 0;
}
#else
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
	send_buf[2] = communication_stc.cmd;
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
#endif

#endif

/**
 * 接收
 */
int recv_data(unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv)
{
    int fd = 0;
    int res = 0;
    if ((fd = open(common_tools.config->serial_stc, O_RDWR, 0644)) < 0)
    {  	
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "open failed!", fd);
    	return OPEN_ERR;
    }
    if ((res = common_tools.recv_data(fd, data, a_data_list, data_len, tv)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_data failed!", res);
    	return res;
    }
    close(fd);
    return 0;
}
