#include "spi_rt_interface.h"

// 客户端连接处理互斥锁 连接数量限制锁
static pthread_mutex_t client_link_manage_mutex;

//最大处理连接为MAX_LINK，link_count_limit_flag设为1
static unsigned char link_count_limit_flag = 0;

// 存放客户端套接字
static struct s_data_list link_data_list;

// 当接收MAX_LINK个连接时，accept_link 延时
static unsigned char push_index = 0, pop_index = 0;
static struct s_spi_rt_cmd spi_rt_cmd[MAX_LINK];

/**
 * 1、STUB程序分为三个线程：
 *    一个线程接收客户端连接，并记录连接数量，然后把客户端套接字添加到链表
 *    一个线程判断连接数量，接收命令，然后执行spi发送 
 *    最后一个线程循环接收spi数据，并把数据解包放到正确的关系内存区
 */

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
 * 监听客户端连接
 */
void * accept_link(void* para)
{
    int spi_client_fd_tmp = 0;
    unsigned char spi_client_fd = 0;
    fd_set fdset;
    struct timeval tv;
    struct timeval timeout;
    memset(&tv, 0, sizeof(struct timeval));
    memset(&timeout, 0, sizeof(struct timeval));
    
    #if LOCAL_SOCKET // 本地socket时
    struct sockaddr_un client; 
    #else
    struct sockaddr_in client; 
    #endif
    
    socklen_t len = sizeof(client); 
    
    // 存储客户端发送的命令包
    struct s_spi_rt_cmd spi_rt_cmd_tmp;
    memset(&spi_rt_cmd_tmp, 0, sizeof(struct s_spi_rt_cmd));
    
    while (1) 
    {
        pthread_mutex_lock(&client_link_manage_mutex);
        if (link_count_limit_flag == 1) 
        {
            sleep(1);
            PRINT("link_data_list.list_len = %d\n", link_data_list.list_len);
            pthread_mutex_unlock(&client_link_manage_mutex);
            continue;
        }
        pthread_mutex_unlock(&client_link_manage_mutex);
        
        FD_ZERO(&fdset);
        FD_SET(spi_rt_interface.spi_server_fd, &fdset);       

        tv.tv_sec = 10;
        tv.tv_usec = 0;
        switch(select(spi_rt_interface.spi_server_fd + 1, &fdset, NULL, NULL, &tv))
        {
            case -1:
            case 0:
            {              
                //PRINT("[spi_rt accept_link process is running ...]\n");
                usleep(DELAY);
                continue;
            }
            default:
            {
                if (FD_ISSET(spi_rt_interface.spi_server_fd, &fdset) > 0)
                {
                    if ((spi_client_fd_tmp = accept(spi_rt_interface.spi_server_fd, (struct sockaddr*)&client, &len)) < 0)
                	{	
                        PERROR("accept failed!\n");
                        continue;
                	}
                	spi_client_fd = spi_client_fd_tmp;
                	spi_client_fd_tmp = 0;
                    PRINT("spi_rt_interface.spi_server_fd = %d, spi_client_fd = %d\n", spi_rt_interface.spi_server_fd, spi_client_fd);
                    
                    // 接收 SPI发送命令
                    timeout.tv_sec = 1;
                    if (spi_rt_interface.recv_spi_rt_cmd_msg(spi_client_fd, &spi_rt_cmd_tmp, &timeout) < 0)
                    {	
                        PERROR("recv_spi_rt_cmd_msg failed!\n");
                        break;
                	}
                	
                	// 添加数据到链表
                	pthread_mutex_lock(&client_link_manage_mutex);
                	memcpy(&spi_rt_cmd[push_index], &spi_rt_cmd_tmp, sizeof(struct s_spi_rt_cmd));
                	memset(&spi_rt_cmd_tmp, 0, sizeof(struct s_spi_rt_cmd));
                	if (push_index == 29)
                    {
                        push_index = 0;
                    }
                    else
                    {
                        push_index++;
                    }
                    
                    PRINT("push_index = %d\n", push_index);
                	if (common_tools.list_tail_add_data(&link_data_list, spi_client_fd) < 0)
                	{
                	    PERROR("list_tail_add_data failed!\n");
                	    pthread_mutex_unlock(&client_link_manage_mutex);
                        break;
                	}
                	PRINT("after list_tail_add_data!\n");
                	
                	if (link_data_list.list_len >= 30)
                    {
                        link_count_limit_flag = 1;
                        PRINT("list_tail_add_data success! link_data_list.list_len = %d\n", link_data_list.list_len);
                    }
                    pthread_mutex_unlock(&client_link_manage_mutex);
                    usleep(10); // 把锁让给处理线程
                	continue;
                }
            }
        }
        
        // 当accept等出现中出现错误时，释放套接字
        if (spi_client_fd > 0)
        {
            close(spi_client_fd);
            spi_client_fd = 0;
        }
    }
    return NULL;
}

/**
 * 判断
 */
void * link_manage(void* para)
{
    int res = 0;
    unsigned char cmd = 0;
    unsigned char uart = 0;
    
    struct timeval tv;
    memset(&tv, 0, sizeof(struct timeval));
    
    while (1) 
    {
        // 连接数量
        pthread_mutex_lock(&client_link_manage_mutex);
        if (link_data_list.list_len > 0)
        {
            PRINT("spi_rt_interface.spi_server_fd = %d, spi_client_fd = %d\n", spi_rt_interface.spi_server_fd, link_data_list.head->data);
            // 命令字判断
            if (spi_rt_cmd[pop_index].cmd == 'T') // 发送命令
            {
                cmd = 'T';
                uart = spi_rt_cmd[pop_index].uart;
                if ((res = spi_rt_interface.spi_send(&spi_rt_cmd[pop_index])) < 0)
                {
                    PERROR("spi_send failed!\n");
                }
            }
            else 
            {
                PRINT("Data does not match!\n");
                res = DATA_ERR;
            }
            
            spi_rt_cmd[pop_index].uart = uart;
            spi_rt_cmd[pop_index].cmd = cmd;
            // 发送或者接收回复
            if (res < 0)
            {
                spi_rt_cmd[pop_index].buf_len = 0xFFFF;
            }
            else 
            {
                spi_rt_cmd[pop_index].buf_len = res;    
            }
            
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            if ((res = spi_rt_interface.send_spi_rt_cmd_msg(link_data_list.head->data, &spi_rt_cmd[pop_index], &tv)) < 0)
            {	
                PERROR("send_spi_rt_cmd_msg failed!\n");
            }
            // 套接字关闭
            if (link_data_list.head->data > 0)
            {
                close(link_data_list.head->data);
            }
            
            // 链接数减一 链表数据
            memset(&spi_rt_cmd[pop_index], 0, sizeof(struct s_spi_rt_cmd));
            if (pop_index == 29)
            {
                pop_index = 0;
            }
            else
            {
                pop_index++;
            }
            PRINT("pop_index = %d, link_data_list.head->data = %d\n", pop_index, link_data_list.head->data);
            if ((res = common_tools.list_head_del_data(&link_data_list, link_data_list.head->data)) < 0)
            {
                PERROR("list_head_del_data failed!\n");
            }
            PRINT("after list_head_del_data!\n");
            if ((link_count_limit_flag == 1) && (link_data_list.list_len < 30))
            {
                link_count_limit_flag = 0;
            }
            
            cmd = uart = 0;
            res = 0;
        }
        else 
        {
            //PRINT("[spi_rt link_manage process is running ...]\n");
            usleep(DELAY);
        }
        pthread_mutex_unlock(&client_link_manage_mutex);
    }
    return NULL;
}

/**
 * 循环接收spi数据，并把数据解析后放到合理的缓冲区
 */
void * spi_recv_data(void* para)
{
    int i = 0, j = 0, k = 0;
    int res = 0, ret = 0;
    int recv_count = 0;            // 接收数据长度
    char data_buf[BUF_LEN] = {0};  // 接收数据缓冲区
    unsigned short len_tmp = 0;
    unsigned char uart = 0;
    
    fd_set fdset;
    struct timeval tv;
    memset(&tv, 0, sizeof(struct timeval));
    
    struct sembuf semopbuf;           // 信号量结构
    memset(&semopbuf, 0, sizeof(struct sembuf));
    
    int uart1_recv_sem_flag = 0;
    int uart2_recv_sem_flag = 0;
    int uart3_recv_sem_flag = 0;
    int expand_recv_sem_flag = 0;
    
	struct s_spi_rt_cmd spi_rt_cmd;
    
    unsigned char repeat_count = 0;
	
    // 获取8个信号
    if ((spi_rt_interface.spi_uart_sem_id = semget(spi_rt_interface.spi_uart_sem_key, 8, IPC_CREAT | 0666)) < 0)
    {
        PERROR("semget failed!\n");
        return (void *)SEMGET_ERR;
    }
    
    while (1)
	{
	    FD_ZERO(&fdset);
        FD_SET(spi_rt_interface.spi_fd, &fdset); 
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        
        switch(select(spi_rt_interface.spi_fd + 1, &fdset, NULL, NULL, &tv))
        {
            case -1:
            case 0:
            {
                PRINT("spi no data or select failed!\n");
                usleep(DELAY);
                continue;
            }
            default:
            {                
                if (FD_ISSET(spi_rt_interface.spi_fd, &fdset) > 0)
                {
                    memset(data_buf, 0, sizeof(data_buf));
                    res = read(spi_rt_interface.spi_fd, data_buf, BUF_LEN);
                    //PRINT("res = %d\n", res);
                    if (res < 0)
                	{
                	    PERROR("read failed!\n");
                        res = READ_ERR;
                        goto EXIT;
                	}
                	else if (res == 0) // 当此次接收长度为0时返回
                    {
                        res = recv_count;
                        goto EXIT;
                    }
                    PRINT("before print_buf_by_hex!\n");
                    PRINT_BUF_BY_HEX(data_buf, NULL, res, __FILE__, __FUNCTION__, __LINE__);
                    PRINT("after print_buf_by_hex!\n");
                    // 缓冲区调节
                    #if BUF_DATA_FLAG
                    do
                    {
                        if ((data_buf[j] != (char)0xA5) || (data_buf[j + 1] != 0x5A))
                        {
                            PRINT("data error!\n");
                            res = DATA_ERR;
                            goto EXIT;
                        }
                        len_tmp = data_buf[j + 2] * 255;
                        len_tmp += (unsigned char)data_buf[j + 3];
                        uart = data_buf[j + 4];
                        
                        if ((data_buf[j + 3 + len_tmp] != common_tools.get_checkbit(data_buf, NULL, j + 2, len_tmp + 1, XOR, 1)))
                        {
                            PRINT("checkbit is different!\n");
                            res = CHECK_DIFF_ERR;
                            goto EXIT;
                        }
                        
                        if(uart == 0x10)
                        {
                			// 信号量没有获取时，
                			if (uart1_recv_sem_flag == 0)
                			{
                			    semopbuf.sem_op = -1;
                                semopbuf.sem_flg = SEM_UNDO;
                                    
                                // 信号量减一
                                semopbuf.sem_num = 4;
                                PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
                                if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                                {
                                    PERROR("semop failed!\n");
                                    res = SEMOP_ERR;
                                    goto EXIT;
                                }
                                uart1_recv_sem_flag = 1;
                                    
                                if ((ret = spi_rt_interface.read_spi_rt_para(semopbuf.sem_num)) < 0)
                                {
                                    PERROR("read_spi_rt_para failed!\n");
                                    res = ret;
                                    goto EXIT;
                                }
                			}
                			if ((spi_rt_interface.uart1_recv_count + len_tmp - 2) > SHARED_MEM_SIZE)
                			{
                			    memset(spi_rt_interface.spi_uart1_recv_shared_mem_buf, 0, SHARED_MEM_SIZE);
                			    spi_rt_interface.uart1_recv_count = 0;
                		    } 
                			// 拷贝到备份缓冲区
                			//PRINT("spi_rt_interface.uart1_recv_count = %d\n", spi_rt_interface.uart1_recv_count);
                		    memcpy(spi_rt_interface.spi_uart1_recv_shared_mem_buf + spi_rt_interface.uart1_recv_count, data_buf + j + 5, len_tmp - 2);
                			spi_rt_interface.uart1_recv_count += (len_tmp - 2);
                			PRINT_BUF_BY_HEX(spi_rt_interface.spi_uart1_recv_shared_mem_buf, NULL, spi_rt_interface.uart1_recv_count, __FILE__, __FUNCTION__, __LINE__);
                		}
                		else if(uart == 0x20)
                		{
                			// 信号量没有获取时
                		    if (uart2_recv_sem_flag == 0)
                		    {
                		        semopbuf.sem_op = -1;
                                semopbuf.sem_flg = SEM_UNDO;
                                // 信号量减一
                                semopbuf.sem_num = 5;
                                PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
                                if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                                {
                                    PERROR("semop failed!\n");
                                    res = SEMOP_ERR;
                                    goto EXIT;
                                }
                                uart2_recv_sem_flag = 1;
                                 
                                if ((ret = spi_rt_interface.read_spi_rt_para(semopbuf.sem_num)) < 0)
                                {
                                    PERROR("read_spi_rt_para failed!\n");
                                    res = ret;
                                    goto EXIT;
                                }
                            }
                            if ((spi_rt_interface.uart2_recv_count + len_tmp - 2) > SHARED_MEM_SIZE)
                			{
                			    memset(spi_rt_interface.spi_uart2_recv_shared_mem_buf, 0, SHARED_MEM_SIZE);
                			    spi_rt_interface.uart2_recv_count = 0;
                		    } 
                			// 拷贝到备份缓冲区
                			// PRINT("spi_rt_interface.uart2_recv_count = %d\n", spi_rt_interface.uart2_recv_count);
                		    memcpy(spi_rt_interface.spi_uart2_recv_shared_mem_buf + spi_rt_interface.uart2_recv_count, data_buf + j + 5, len_tmp - 2);
                			spi_rt_interface.uart2_recv_count += (len_tmp - 2);
                			PRINT_BUF_BY_HEX(spi_rt_interface.spi_uart2_recv_shared_mem_buf, NULL, spi_rt_interface.uart2_recv_count, __FILE__, __FUNCTION__, __LINE__);
                	    }
                	    else if(uart == 0x30)
                		{
                		    // 信号量没有获取时
                		    if (uart3_recv_sem_flag == 0)
                		    {
                		        semopbuf.sem_op = -1;
                                semopbuf.sem_flg = SEM_UNDO;
                                   
                                // 信号量减一
                                semopbuf.sem_num = 6;
                                PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
                                if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                                {
                                    PERROR("semop failed!\n");
                                    res = SEMOP_ERR;
                                    goto EXIT;
                                }
                                uart3_recv_sem_flag = 1;
                                
                                if ((ret = spi_rt_interface.read_spi_rt_para(semopbuf.sem_num)) < 0)
                                {
                                    PERROR("read_spi_rt_para failed!\n");
                                    res = ret;
                                    goto EXIT;
                                }
                			}
                			if ((spi_rt_interface.uart3_recv_count + len_tmp - 2) > SHARED_MEM_SIZE)
                			{
                			    memset(spi_rt_interface.spi_uart3_recv_shared_mem_buf, 0, SHARED_MEM_SIZE);
                			    spi_rt_interface.uart3_recv_count = 0;
                		    } 
                			// 拷贝到备份缓冲区
                			//PRINT("spi_rt_interface.spi_uart3_recv_shared_mem_buf = %X\n", spi_rt_interface.spi_uart3_recv_shared_mem_buf);
                			memcpy(spi_rt_interface.spi_uart3_recv_shared_mem_buf + spi_rt_interface.uart3_recv_count, data_buf + j + 5, len_tmp - 2);
                			spi_rt_interface.uart3_recv_count += (len_tmp - 2);
                			PRINT_BUF_BY_HEX(spi_rt_interface.spi_uart3_recv_shared_mem_buf, NULL, spi_rt_interface.uart3_recv_count, __FILE__, __FUNCTION__, __LINE__);
                        }
                        else if(uart == 0x40)
                		{
                		    // 信号量没有获取时
                		    if (expand_recv_sem_flag == 0)
                		    {
                		        semopbuf.sem_op = -1;
                                semopbuf.sem_flg = SEM_UNDO;
                                   
                                // 信号量减一
                                semopbuf.sem_num = 7;
                                PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
                                if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                                {
                                    PERROR("semop failed!\n");
                                    res = SEMOP_ERR;
                                    goto EXIT;
                                }
                                expand_recv_sem_flag = 1;
                                
                                if ((ret = spi_rt_interface.read_spi_rt_para(semopbuf.sem_num)) < 0)
                                {
                                    PERROR("read_spi_rt_para failed!\n");
                                    res = ret;
                                    goto EXIT;
                                }
                			}
                			
                			if ((spi_rt_interface.expand_recv_count + len_tmp - 2) > SHARED_MEM_SIZE)
                			{
                			    memset(spi_rt_interface.spi_expand_recv_shared_mem_buf, 0, SHARED_MEM_SIZE);
                			    spi_rt_interface.expand_recv_count = 0;
                		    } 
                			// 拷贝到备份缓冲区
                			//PRINT("spi_rt_interface.spi_expand_recv_shared_mem_buf = %X\n", spi_rt_interface.spi_expand_recv_shared_mem_buf);
                			memcpy(spi_rt_interface.spi_expand_recv_shared_mem_buf + spi_rt_interface.expand_recv_count, data_buf + j + 5, len_tmp - 2);
                			spi_rt_interface.expand_recv_count += (len_tmp - 2);
                			PRINT_BUF_BY_HEX(spi_rt_interface.spi_expand_recv_shared_mem_buf, NULL, spi_rt_interface.expand_recv_count, __FILE__, __FUNCTION__, __LINE__);
                        }
                        else
                        {
                            PRINT("data error!\n");
                            res = DATA_ERR;
                            goto EXIT;
                        } 
                        j += (len_tmp + 4);
                        k++;
                        PRINT("j = %d, res = %d, k = %d\n", j, res, k);
                    }
                    while(j < res);
                    recv_count += (res - (6 * k));
                    
                    #else
                    
                    do
                    {
                        if ((data_buf[j] != (char)0xA5) || (data_buf[j + 1] != 0x5A))
                        {
                            PRINT("data error!\n");
                            res = DATA_ERR;
                            goto EXIT;
                        }
                        len_tmp = data_buf[j + 2] * 255;
                        len_tmp += (unsigned char)data_buf[j + 3];
                        uart = data_buf[j + 4];
                        
                        if ((data_buf[j + 3 + len_tmp] != common_tools.get_checkbit(data_buf, NULL, j + 2, len_tmp + 1, XOR, 1)))
                        {
                            PRINT("checkbit is different!\n");
                            res = CHECK_DIFF_ERR;
                            goto EXIT;
                        }
                        
                        if(uart == 0x10)
                        {
                			// 信号量没有获取时，
                			if (uart1_recv_sem_flag == 0)
                			{
                			    semopbuf.sem_op = -1;
                                semopbuf.sem_flg = SEM_UNDO;
                                    
                                // 信号量减一
                                semopbuf.sem_num = 4;
                                PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
                                if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                                {
                                    PERROR("semop failed!\n");
                                    res = SEMOP_ERR;
                                    goto EXIT;
                                }
                                uart1_recv_sem_flag = 1;
                                    
                                if ((ret = spi_rt_interface.read_spi_rt_para(semopbuf.sem_num)) < 0)
                                {
                                    PERROR("read_spi_rt_para failed!\n");
                                    res = ret;
                                    goto EXIT;
                                }
                			}
                			
                			// 拷贝到备份缓冲区
                		    PRINT("spi_rt_interface.uart1_recv_count = %d, spi_rt_interface.spi_uart1_recv_read_index = %d, spi_rt_interface.spi_uart1_recv_write_index = %d, len_tmp = %d\n", 
                                    spi_rt_interface.uart1_recv_count, spi_rt_interface.spi_uart1_recv_read_index, spi_rt_interface.spi_uart1_recv_write_index, len_tmp);
                		    if ((spi_rt_interface.spi_uart1_recv_write_index + len_tmp - 2) <= SHARED_MEM_SIZE)
                		    {
                		        memcpy(spi_rt_interface.spi_uart1_recv_shared_mem_buf + spi_rt_interface.spi_uart1_recv_write_index, data_buf + j + 5, len_tmp - 2);
                		        spi_rt_interface.uart1_recv_count += (len_tmp - 2);
                		        spi_rt_interface.spi_uart1_recv_write_index += (len_tmp - 2);
                		    }
                		    else if ((spi_rt_interface.spi_uart1_recv_write_index + len_tmp - 2) > SHARED_MEM_SIZE)
                		    {
                		        if (spi_rt_interface.spi_uart1_recv_write_index == 0)
                		        {
                		            memcpy(spi_rt_interface.spi_uart1_recv_shared_mem_buf, data_buf + j + 5, SHARED_MEM_SIZE - spi_rt_interface.uart1_recv_count);
                		            spi_rt_interface.spi_uart1_recv_write_index = SHARED_MEM_SIZE - spi_rt_interface.uart1_recv_count;
                		            spi_rt_interface.uart1_recv_count = SHARED_MEM_SIZE;
                		        }
                		        else 
                		        {
                		            memcpy(spi_rt_interface.spi_uart1_recv_shared_mem_buf, spi_rt_interface.spi_uart1_recv_shared_mem_buf + spi_rt_interface.spi_uart1_recv_read_index, spi_rt_interface.uart1_recv_count);
                		            if (spi_rt_interface.spi_uart1_recv_read_index >= spi_rt_interface.uart1_recv_count)
                		            {
                		                memset(spi_rt_interface.spi_uart1_recv_shared_mem_buf + spi_rt_interface.spi_uart1_recv_read_index, 0, spi_rt_interface.uart1_recv_count);
                		            }
                		            else
                		            {
                		                memset(spi_rt_interface.spi_uart1_recv_shared_mem_buf + spi_rt_interface.uart1_recv_count, 0, spi_rt_interface.spi_uart1_recv_read_index);
                		            }
                		            
                		            spi_rt_interface.spi_uart1_recv_read_index = 0;
                		            memcpy(spi_rt_interface.spi_uart1_recv_shared_mem_buf + spi_rt_interface.uart1_recv_count, data_buf + j + 5, SHARED_MEM_SIZE - spi_rt_interface.uart1_recv_count);
                		            spi_rt_interface.uart1_recv_count = SHARED_MEM_SIZE;
                		            spi_rt_interface.spi_uart1_recv_write_index = SHARED_MEM_SIZE;
                		        }
                		    }
                		    
                			PRINT_BUF_BY_HEX(spi_rt_interface.spi_uart1_recv_shared_mem_buf + spi_rt_interface.spi_uart1_recv_read_index, NULL, spi_rt_interface.uart1_recv_count, __FILE__, __FUNCTION__, __LINE__);
                		}
                		else if(uart == 0x20)
                		{
                			// 信号量没有获取时
                		    if (uart2_recv_sem_flag == 0)
                		    {
                		        semopbuf.sem_op = -1;
                                semopbuf.sem_flg = SEM_UNDO;
                                // 信号量减一
                                semopbuf.sem_num = 5;
                                PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
                                if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                                {
                                    PERROR("semop failed!\n");
                                    res = SEMOP_ERR;
                                    goto EXIT;
                                }
                                uart2_recv_sem_flag = 1;
                                 
                                if ((ret = spi_rt_interface.read_spi_rt_para(semopbuf.sem_num)) < 0)
                                {
                                    PERROR("read_spi_rt_para failed!\n");
                                    res = ret;
                                    goto EXIT;
                                }
                            }
                            
                			// 拷贝到备份缓冲区
                			PRINT("spi_rt_interface.uart2_recv_count = %d, spi_rt_interface.spi_uart2_recv_read_index = %d, spi_rt_interface.spi_uart2_recv_write_index = %d, len_tmp = %d\n", 
                                    spi_rt_interface.uart2_recv_count, spi_rt_interface.spi_uart2_recv_read_index, spi_rt_interface.spi_uart2_recv_write_index, len_tmp);
                		    if ((spi_rt_interface.spi_uart2_recv_write_index + len_tmp - 2) <= SHARED_MEM_SIZE)
                		    {
                		        memcpy(spi_rt_interface.spi_uart2_recv_shared_mem_buf + spi_rt_interface.spi_uart2_recv_write_index, data_buf + j + 5, len_tmp - 2);
                		        spi_rt_interface.uart2_recv_count += (len_tmp - 2);
                		        spi_rt_interface.spi_uart2_recv_write_index += (len_tmp - 2);
                		    }
                		    else if ((spi_rt_interface.spi_uart2_recv_write_index + len_tmp - 2) > SHARED_MEM_SIZE)
                		    {
                		        if (spi_rt_interface.spi_uart2_recv_write_index == 0)
                		        {
                		            memcpy(spi_rt_interface.spi_uart2_recv_shared_mem_buf, data_buf + j + 5, SHARED_MEM_SIZE - spi_rt_interface.uart2_recv_count);
                		            spi_rt_interface.spi_uart2_recv_write_index = SHARED_MEM_SIZE - spi_rt_interface.uart2_recv_count;
                		            spi_rt_interface.uart2_recv_count = SHARED_MEM_SIZE;
                		        }
                		        else 
                		        {
                		            memcpy(spi_rt_interface.spi_uart2_recv_shared_mem_buf, spi_rt_interface.spi_uart2_recv_shared_mem_buf + spi_rt_interface.spi_uart2_recv_read_index, spi_rt_interface.uart2_recv_count);
                		            if (spi_rt_interface.spi_uart2_recv_read_index >= spi_rt_interface.uart2_recv_count)
                		            {
                		                memset(spi_rt_interface.spi_uart2_recv_shared_mem_buf + spi_rt_interface.spi_uart2_recv_read_index, 0, spi_rt_interface.uart2_recv_count);
                		            }
                		            else
                		            {
                		                memset(spi_rt_interface.spi_uart2_recv_shared_mem_buf + spi_rt_interface.uart2_recv_count, 0, spi_rt_interface.spi_uart2_recv_read_index);
                		            }
                		            
                		            spi_rt_interface.spi_uart2_recv_read_index = 0;
                		            memcpy(spi_rt_interface.spi_uart2_recv_shared_mem_buf + spi_rt_interface.uart2_recv_count, data_buf + j + 5, SHARED_MEM_SIZE - spi_rt_interface.uart2_recv_count);
                		            spi_rt_interface.uart2_recv_count = SHARED_MEM_SIZE;
                		            spi_rt_interface.spi_uart2_recv_write_index = SHARED_MEM_SIZE;
                		        }
                		    }
                			PRINT_BUF_BY_HEX(spi_rt_interface.spi_uart2_recv_shared_mem_buf + spi_rt_interface.spi_uart2_recv_read_index, NULL, spi_rt_interface.uart2_recv_count, __FILE__, __FUNCTION__, __LINE__);
                	    }
                	    else if(uart == 0x30)
                		{
                		    // 信号量没有获取时
                		    if (uart3_recv_sem_flag == 0)
                		    {
                		        semopbuf.sem_op = -1;
                                semopbuf.sem_flg = SEM_UNDO;
                                   
                                // 信号量减一
                                semopbuf.sem_num = 6;
                                PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
                                if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                                {
                                    PERROR("semop failed!\n");
                                    res = SEMOP_ERR;
                                    goto EXIT;
                                }
                                uart3_recv_sem_flag = 1;
                                
                                if ((ret = spi_rt_interface.read_spi_rt_para(semopbuf.sem_num)) < 0)
                                {
                                    PERROR("read_spi_rt_para failed!\n");
                                    res = ret;
                                    goto EXIT;
                                }
                			}
                			
                			// 拷贝到备份缓冲区
                			PRINT("spi_rt_interface.uart3_recv_count = %d, spi_rt_interface.spi_uart3_recv_read_index = %d, spi_rt_interface.spi_uart3_recv_write_index = %d, len_tmp = %d\n", 
                                    spi_rt_interface.uart3_recv_count, spi_rt_interface.spi_uart3_recv_read_index, spi_rt_interface.spi_uart3_recv_write_index, len_tmp);
                		    if ((spi_rt_interface.spi_uart3_recv_write_index + len_tmp - 2) <= SHARED_MEM_SIZE)
                		    {
                		        memcpy(spi_rt_interface.spi_uart3_recv_shared_mem_buf + spi_rt_interface.spi_uart3_recv_write_index, data_buf + j + 5, len_tmp - 2);
                		        spi_rt_interface.uart3_recv_count += (len_tmp - 2);
                		        spi_rt_interface.spi_uart3_recv_write_index += (len_tmp - 2);
                		    }
                		    else if ((spi_rt_interface.spi_uart3_recv_write_index + len_tmp - 2) > SHARED_MEM_SIZE)
                		    {
                		        if (spi_rt_interface.spi_uart3_recv_write_index == 0)
                		        {
                		            memcpy(spi_rt_interface.spi_uart3_recv_shared_mem_buf, data_buf + j + 5, SHARED_MEM_SIZE - spi_rt_interface.uart3_recv_count);
                		            spi_rt_interface.spi_uart3_recv_write_index = SHARED_MEM_SIZE - spi_rt_interface.uart3_recv_count;
                		            spi_rt_interface.uart3_recv_count = SHARED_MEM_SIZE;
                		        }
                		        else 
                		        {
                		            memcpy(spi_rt_interface.spi_uart3_recv_shared_mem_buf, spi_rt_interface.spi_uart3_recv_shared_mem_buf + spi_rt_interface.spi_uart3_recv_read_index, spi_rt_interface.uart3_recv_count);
                		            if (spi_rt_interface.spi_uart3_recv_read_index >= spi_rt_interface.uart3_recv_count)
                		            {
                		                memset(spi_rt_interface.spi_uart3_recv_shared_mem_buf + spi_rt_interface.spi_uart3_recv_read_index, 0, spi_rt_interface.uart3_recv_count);
                		            }
                		            else
                		            {
                		                memset(spi_rt_interface.spi_uart3_recv_shared_mem_buf + spi_rt_interface.uart3_recv_count, 0, spi_rt_interface.spi_uart3_recv_read_index);
                		            }
                		            
                		            spi_rt_interface.spi_uart3_recv_read_index = 0;
                		            memcpy(spi_rt_interface.spi_uart3_recv_shared_mem_buf + spi_rt_interface.uart3_recv_count, data_buf + j + 5, SHARED_MEM_SIZE - spi_rt_interface.uart3_recv_count);
                		            spi_rt_interface.uart3_recv_count = SHARED_MEM_SIZE;
                		            spi_rt_interface.spi_uart3_recv_write_index = SHARED_MEM_SIZE;
                		        }
                		    }
                			PRINT_BUF_BY_HEX(spi_rt_interface.spi_uart3_recv_shared_mem_buf + spi_rt_interface.spi_uart3_recv_read_index, NULL, spi_rt_interface.uart3_recv_count, __FILE__, __FUNCTION__, __LINE__);
                        }
                        else if(uart == 0x40)
                		{
                		    // 信号量没有获取时
                		    if (expand_recv_sem_flag == 0)
                		    {
                		        semopbuf.sem_op = -1;
                                semopbuf.sem_flg = SEM_UNDO;
                                   
                                // 信号量减一
                                semopbuf.sem_num = 6;
                                PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
                                if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                                {
                                    PERROR("semop failed!\n");
                                    res = SEMOP_ERR;
                                    goto EXIT;
                                }
                                expand_recv_sem_flag = 1;
                                
                                if ((ret = spi_rt_interface.read_spi_rt_para(semopbuf.sem_num)) < 0)
                                {
                                    PERROR("read_spi_rt_para failed!\n");
                                    res = ret;
                                    goto EXIT;
                                }
                			}
                			
                			// 拷贝到备份缓冲区
                			PRINT("spi_rt_interface.expand_recv_count = %d, spi_rt_interface.spi_expand_recv_read_index = %d, spi_rt_interface.spi_expand_recv_write_index = %d, len_tmp = %d\n", 
                                    spi_rt_interface.expand_recv_count, spi_rt_interface.spi_expand_recv_read_index, spi_rt_interface.spi_expand_recv_write_index, len_tmp);
                		    if ((spi_rt_interface.spi_expand_recv_write_index + len_tmp - 2) <= SHARED_MEM_SIZE)
                		    {
                		        memcpy(spi_rt_interface.spi_expand_recv_shared_mem_buf + spi_rt_interface.spi_expand_recv_write_index, data_buf + j + 5, len_tmp - 2);
                		        spi_rt_interface.expand_recv_count += (len_tmp - 2);
                		        spi_rt_interface.spi_expand_recv_write_index += (len_tmp - 2);
                		    }
                		    else if ((spi_rt_interface.spi_expand_recv_write_index + len_tmp - 2) > SHARED_MEM_SIZE)
                		    {
                		        if (spi_rt_interface.spi_expand_recv_write_index == 0)
                		        {
                		            memcpy(spi_rt_interface.spi_expand_recv_shared_mem_buf, data_buf + j + 5, SHARED_MEM_SIZE - spi_rt_interface.expand_recv_count);
                		            spi_rt_interface.spi_expand_recv_write_index = SHARED_MEM_SIZE - spi_rt_interface.expand_recv_count;
                		            spi_rt_interface.expand_recv_count = SHARED_MEM_SIZE;
                		        }
                		        else 
                		        {
                		            memcpy(spi_rt_interface.spi_expand_recv_shared_mem_buf, spi_rt_interface.spi_expand_recv_shared_mem_buf + spi_rt_interface.spi_expand_recv_read_index, spi_rt_interface.expand_recv_count);
                		            if (spi_rt_interface.spi_expand_recv_read_index >= spi_rt_interface.expand_recv_count)
                		            {
                		                memset(spi_rt_interface.spi_expand_recv_shared_mem_buf + spi_rt_interface.spi_expand_recv_read_index, 0, spi_rt_interface.expand_recv_count);
                		            }
                		            else
                		            {
                		                memset(spi_rt_interface.spi_expand_recv_shared_mem_buf + spi_rt_interface.expand_recv_count, 0, spi_rt_interface.spi_expand_recv_read_index);
                		            }
                		            
                		            spi_rt_interface.spi_expand_recv_read_index = 0;
                		            memcpy(spi_rt_interface.spi_expand_recv_shared_mem_buf + spi_rt_interface.expand_recv_count, data_buf + j + 5, SHARED_MEM_SIZE - spi_rt_interface.expand_recv_count);
                		            spi_rt_interface.expand_recv_count = SHARED_MEM_SIZE;
                		            spi_rt_interface.spi_expand_recv_write_index = SHARED_MEM_SIZE;
                		        }
                		    }
                			PRINT_BUF_BY_HEX(spi_rt_interface.spi_expand_recv_shared_mem_buf + spi_rt_interface.spi_expand_recv_read_index, NULL, spi_rt_interface.expand_recv_count, __FILE__, __FUNCTION__, __LINE__);
                        }
                        else
                        {
                            PRINT("data error!\n");
                            res = DATA_ERR;
                            goto EXIT;
                        }    
                        j += (len_tmp + 4);
                        k++;
                        PRINT("j = %d, res = %d, k = %d\n", j, res, k);
                    }
                    while(j < res);
                    recv_count += (res - (6 * k));
                    #endif
                    
                    PRINT("recv_count = %d\n", recv_count);
                }
                else
                {
                    PRINT("spi no data!\n");
                    usleep(DELAY);
                    continue;
                }
            }
        }
            
EXIT:
        // 资源的释放
        semopbuf.sem_op = 1;
        if (expand_recv_sem_flag == 1)
        {
            if ((ret = spi_rt_interface.write_spi_rt_para(7)) < 0)
            {
                PERROR("write_spi_rt_para failed!\n");
                res = ret;
            }
            
            expand_recv_sem_flag = 0;
            semopbuf.sem_num = 7;
            PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            while (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
            }
        }
        if (uart3_recv_sem_flag == 1)
        {
            if ((ret = spi_rt_interface.write_spi_rt_para(6)) < 0)
            {
                PERROR("write_spi_rt_para failed!\n");
                res = ret;
            }
            
            uart3_recv_sem_flag = 0;
            semopbuf.sem_num = 6;
            PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            while (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
            }
        }
        if (uart2_recv_sem_flag == 1)
        {
            if ((ret = spi_rt_interface.write_spi_rt_para(5)) < 0)
            {
                PERROR("write_spi_rt_para failed!\n");
                res = ret;
            }
            
            uart2_recv_sem_flag = 0;
            semopbuf.sem_num = 5;
            PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            while (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
            }
        }
        if (uart1_recv_sem_flag == 1)
        {
            if ((ret = spi_rt_interface.write_spi_rt_para(4)) < 0)
            {
                PERROR("write_spi_rt_para failed!\n");
                res = ret;
            }
            
            uart1_recv_sem_flag = 0;
            semopbuf.sem_num = 4;
            PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            while (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
            }
        }
        
        if (res <= 0)
        {
			// 重发命令 
            if ((res == CHECK_DIFF_ERR) || (res == DATA_ERR))
            {
                if (repeat_count >= 3)
                {
                    repeat_count = 0;
                }
                else 
                {
                    PRINT("send repeat cmd to STM32!\n");
                    if ((res = spi_rt_interface.send_repeat_cmd()) < 0)
                    {
                        PERROR("send_repeat_cmd failed!\n");
                    } 
                }
            }
			
            usleep(DELAY);
		}
		else
        {
            repeat_count = 0;
        }  
        res = 0;
        ret = 0;
        i = j = k = 0;
        recv_count = 0;
    }
    return NULL;
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
    pthread_t pthread_accept_link_id, pthread_link_manage_id, pthread_spi_recv_data_id; // 线程ID
    
    // 清空结构体
    memset(&link_data_list, 0, sizeof(struct s_data_list));
    memset(&spi_rt_cmd, 0, sizeof(struct s_spi_rt_cmd) * MAX_LINK);
    
    // 初始化STUB
    if ((res = spi_rt_interface.init_spi()) < 0)
    {
        PERROR("init_spi falied\n");
        return res;
    }
    
    // STUB启动提示
    #if PRINT_DEBUG
    PRINT("STUB (%s %s)\n", __DATE__, __TIME__);
    PRINT("[init_spi success!]\n");
    #else
    printf("[%s]%s["__FILE__"][%s][%05d] [STUB (%s %s)]\n", common_tools.argv0, common_tools.get_datetime_buf(), __FUNCTION__, __LINE__, __DATE__, __TIME__);
    printf("[%s]%s["__FILE__"][%s][%05d] [init_spi success!]\n", common_tools.argv0, common_tools.get_datetime_buf(), __FUNCTION__, __LINE__);
    printf("[%s]%s["__FILE__"][%s][%05d] [STUB is running...]\n", common_tools.argv0, common_tools.get_datetime_buf(), __FUNCTION__, __LINE__);
    #endif
    
    // 初始化互斥锁、创建线程
    pthread_mutex_init(&client_link_manage_mutex, NULL);
    pthread_create(&pthread_accept_link_id, NULL, (void*)accept_link, NULL); 
    pthread_create(&pthread_link_manage_id, NULL, (void*)link_manage, NULL);
    pthread_create(&pthread_spi_recv_data_id, NULL, (void*)spi_recv_data, NULL);
    
    pthread_join(pthread_accept_link_id, NULL);
    pthread_join(pthread_link_manage_id, NULL);
    pthread_join(pthread_spi_recv_data_id, NULL);
    pthread_mutex_destroy(&client_link_manage_mutex);
    PRINT("STUB over...");
    return 0;
}
