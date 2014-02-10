#include "spi_rt_interface.h"
#include <pthread.h>

struct s_spi_uart1
{
    pthread_mutex_t mutex;
    int fd;
    char phone_flag;
    char phone[25];
};

/**
 * 信号处理函数
 */
void signal_handle(int sig)
{
    PRINT("signal_handle entry!\n");
    
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
    return;
}

/** 
 * 初始化环境
 */
int init_env(struct s_spi_uart1 *spi_uart1)
{
    int res = 0;
    
    if ((res = communication_network.make_server_link(SPI_UART1_MUTEX_SOCKET_PORT)) < 0)
    {
        PERROR("make_server_link failed!\n");
        return res;
    }
    spi_uart1->fd = res;
    return 0;
}

/**
 * spi串口一接收
 */
void * pthread_spi_recv(void *para)
{
    struct s_spi_uart1 *spi_uart1 = (struct s_spi_uart1 *)para;
    int res = 0;
    int i = 0;
    char head[2] = {0};
    unsigned char cmd = 0;
    char phone_abnormal_buf[4] = {0};
    struct timeval tv = {1, 0};
    
    while (1)
	{
	    pthread_mutex_lock(&spi_uart1->mutex);
	    tv.tv_sec = 1;
	    
	    if (spi_uart1->phone_flag == 1) // 说明有来电要处理
	    {
	        // 处理来电
	        spi_uart1->phone_flag = 0;
	        PRINT("phone = %s\n", spi_uart1->phone);
	        memset(spi_uart1->phone, 0, sizeof(spi_uart1->phone));
	    }
	    
	    if ((res = spi_rt_interface.spi_select(UART1, &tv)) < 0)
        {
            PERROR("spi UART1 no data!\n");
            pthread_mutex_unlock(&spi_uart1->mutex);
            usleep(10);
            continue;
        }
            
        // 接收同步头
        for (i = 0; i < sizeof(head); i++)
        {
            if ((res = spi_rt_interface.recv_data2(UART1, &head[i], sizeof(head[i]), &tv)) != sizeof(head[i]))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                pthread_mutex_unlock(&spi_uart1->mutex);
                //continue;
                break;
            }
            PRINT("head[%d] = %02X\n", i, (unsigned char)head[i]);
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
        
        if (res < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
            continue;
        }
        
        if ((res = spi_rt_interface.recv_data2(UART1, &cmd, sizeof(cmd), &tv)) != sizeof(cmd))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            pthread_mutex_unlock(&spi_uart1->mutex);
            continue;
        }
        
        PRINT("cmd = %02X\n", cmd);
        if (cmd == 0x11) // 有电话线拔出 插入 并机 等情况
        {
            if ((res = spi_rt_interface.recv_data2(UART1, phone_abnormal_buf, sizeof(phone_abnormal_buf), &tv)) != sizeof(phone_abnormal_buf))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                pthread_mutex_unlock(&spi_uart1->mutex);
                continue;
            }
            PRINT_BUF_BY_HEX(phone_abnormal_buf, NULL, sizeof(phone_abnormal_buf), __FILE__, __FUNCTION__, __LINE__);
            if ((phone_abnormal_buf[0] == 0x02) && (((phone_abnormal_buf[1] == 0x51) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x51)) 
                || ((phone_abnormal_buf[1] == 0x52) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x52))
                || ((phone_abnormal_buf[1] == 0x53) && (phone_abnormal_buf[2] == 0x00) && (phone_abnormal_buf[3] == 0x53))))
            {
                if ((res = communication_serial.phone_state_monitor(phone_abnormal_buf[1])) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "phone_state_monitor failed!", res);
                }
            }
            pthread_mutex_unlock(&spi_uart1->mutex);
            continue;
        }
        pthread_mutex_unlock(&spi_uart1->mutex);
    }
    return (void *)0;
}

/**
 * 暂停接收 命令
 */
void *pthread_stop_recv_cmd(void *para)
{
    struct s_spi_uart1 *spi_uart1 = (struct s_spi_uart1 *)para;
    int res = 0;
    int i = 0;
    int client_fd = 0;
    fd_set fdset;
    struct timeval tv = {5, 0};
    struct sockaddr_un client; 
    socklen_t len = sizeof(client);
    
    char success_buf[5] = {0x5A, 0xA5, 0x01, 0x00, 0x00};
    char failed_buf[5] = {0x5A, 0xA5, 0x01, 0xFF, 0xFF};
    
    char head[2] = {0x5A, 0xA5};
    unsigned char buf_len = 0;
    unsigned char cmd = 0;
    char *data = NULL;
    unsigned char checkbit = 0;
    
    char lock_flag = 0;
    while (1) 
    {
        FD_ZERO(&fdset);
        FD_SET(spi_uart1->fd, &fdset);       
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        switch (select(spi_uart1->fd + 1, &fdset, NULL, NULL, &tv))
        {
            case 0:
            case -1:
            {
                PRINT("pthread_stop_recv_cmd waiting recv cmd! spi_uart1->fd = %d\n", spi_uart1->fd);
                continue;    
            }
            default:
            {
                if (FD_ISSET(spi_uart1->fd, &fdset) > 0)
                {
                    if ((client_fd = accept(spi_uart1->fd, (struct sockaddr*)&client, &len)) < 0)
                	{	
                        PERROR("accept failed!\n");
                        continue;
                	}
                	PRINT("client_fd = %d\n", client_fd);
                	// 接收数据头
                	if ((res = common_tools.recv_data_head(client_fd, head, sizeof(head), &tv)) < 0)
                    {
                        PERROR("recv_data_head failed!\n");         
                        continue;
                    }
                	
                	// 接收数据长度
                	if ((res = common_tools.recv_one_byte(client_fd, &buf_len, &tv)) < 0)
                    {
                        PERROR("recv_one_byte failed!\n");         
                        continue;
                    }
                	PRINT("buf_len = %02X\n", buf_len);
                	
                	// 接收命令字
                    if ((res = common_tools.recv_one_byte(client_fd, &cmd, &tv)) < 0)
                    {
                        PERROR("recv_one_byte failed!\n");      
                        continue;
                    }
                    PRINT("cmd = %02X\n", cmd);
                    
                    // 接收有效数据
                    if (buf_len > 1)
                    {
                        if ((data = malloc(buf_len)) == NULL)
                        {
                            PERROR("malloc failed!\n");
                            continue;
                        }
                        memset(data, 0, buf_len);
                        
                        if ((res = common_tools.recv_data(client_fd, data, NULL, buf_len - 1, &tv)) < 0)
                        {
                            PERROR("recv_one_byte failed!\n");
                            break;
                        }
                        PRINT_BUF_BY_HEX(data, NULL, buf_len - 1, __FILE__, __FUNCTION__, __LINE__);
                    }
                    
                    // 接收校验
                    if ((res = common_tools.recv_one_byte(client_fd, &checkbit, &tv)) < 0)
                    {
                        PERROR("recv_one_byte failed!\n");         
                        continue;
                    }
                    PRINT("checkbit = %02X\n", checkbit);
                    // 计算校验
                    if (buf_len > 1)
                    {
                        if ((res = common_tools.get_checkbit(data, NULL, 0, buf_len - 1, XOR, sizeof(checkbit))) < 0)
                        {
                            PERROR("get_checkbit failed!\n"); 
                            break;
                        }
                    }
                    else if (buf_len == 1) 
                    {
                        res = cmd;
                    }
                    else 
                    {
                        PERROR("buf_len error!\n");
                        res = LENGTH_ERR;
                        break;
                    }
                    
                    // 校验判断
                    if (res != checkbit)
                    {
                        PERROR("checkbit is different!\n");
                        res = CHECK_DIFF_ERR;
                        break;
                    }
                    
                    switch (cmd)
                    {
                        case 0x01: // 暂停spi接收
                        {
                            PRINT("lock_flag = %d\n", lock_flag);
                            if (lock_flag == 0) // 本线程没有获得锁的情况
                            {
                                for (i = 0; i < 3; i++)
                                {
                                    if (pthread_mutex_lock(&spi_uart1->mutex) != 0)
                                    {
                                        PERROR("pthread_mutex_lock failed!\n");
                                        res = PTHREAD_LOCK_ERR;
                                        sleep(1);
                                        continue;
                                    }
                                    break;
                                }
                                
                                PRINT("i = %d\n", i);
                                if (i < 3) //成功
                                {
                                    lock_flag = 1;
                                    PRINT("get lock success!\n");
                                    if ((res = common_tools.send_data(client_fd, success_buf, NULL, sizeof(success_buf), NULL)) < 0)
                                    {
                                        PERROR("send_data failed!");
                                        pthread_mutex_unlock(&spi_uart1->mutex);
                                        lock_flag = 0;
                                    }
                                    
                                }
                                else 
                                {
                                    PRINT("get lock failed!\n");
                                    if ((res = common_tools.send_data(client_fd, failed_buf, NULL, sizeof(failed_buf), NULL)) < 0)
                                    {
                                        PERROR("send_data failed!");
                                    }
                                }
                            }
                            else 
                            {
                                if ((res = common_tools.send_data(client_fd, success_buf, NULL, sizeof(success_buf), NULL)) < 0)
                                {
                                    PERROR("send_data failed!");
                                    pthread_mutex_unlock(&spi_uart1->mutex);
                                    lock_flag = 0;
                                }
                            }
                            break;
                        }
                        case 0x02: // 开始spi接收
                        {
                            PRINT("lock_flag = %d\n", lock_flag);
                            if (lock_flag == 1) // 本线程获得锁的情况
                            {
                                pthread_mutex_unlock(&spi_uart1->mutex);
                                lock_flag = 0;
                            }
                            break;
                        }
                        case 0x03: // 开始接收 并有来电需要响应
                        {
                            if (lock_flag == 1) // 本线程获得锁的情况
                            {
                                pthread_mutex_unlock(&spi_uart1->mutex);
                                lock_flag = 0;
                            }
                            if ((buf_len - 1) >= sizeof(spi_uart1->phone))
                            {
                                memcpy(spi_uart1->phone, data, sizeof(spi_uart1->phone));
                            }
                            else
                            {
                                memcpy(spi_uart1->phone, data, buf_len - 1);
                            }
                            spi_uart1->phone_flag = 1;
                            break;
                        }
                        default:
                        {
                            PERROR("option is mismatching!\n");
                            res = MISMATCH_ERR;
                            break;
                        }  
                    }
                }
            }
                
        }
        
        // 资源释放
        if (data != NULL)
        {
            free(data);
            data = NULL;
        }
    }
    return (void *)0;
}

int main(int argc, char ** argv)
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
    
    strcpy(common_tools.argv0, argv[0]);
	
	int res = 0;
	struct s_spi_uart1 spi_uart1;
	memset(&spi_uart1, 0, sizeof(struct s_spi_uart1));
	
	pthread_t pthread_spi_recv_t, pthread_stop_recv_cmd_t;
	
	if ((res = init_env(&spi_uart1)) < 0)
	{
	    PERROR("init_env failed!\n");
	    return res;
	}
	
	pthread_mutex_init(&spi_uart1.mutex, NULL);
	
	// 创建两个线程 1个接收spi uart1的数据 1个接收暂停接收命令
	pthread_create(&pthread_spi_recv_t, NULL, pthread_spi_recv, &spi_uart1);
	pthread_create(&pthread_stop_recv_cmd_t, NULL, pthread_stop_recv_cmd, &spi_uart1);
	
	pthread_join(pthread_spi_recv_t, NULL);
	pthread_join(pthread_stop_recv_cmd_t, NULL);
	pthread_mutex_destroy(&spi_uart1.mutex);
	return 0;
}
