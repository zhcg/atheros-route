#include "spi_rt_interface.h"

#define PORT "9738"

#define MS 0 // 磁条卡
#define IC 1 // IC卡
#define RF 2 // 射频卡

/*
    len: 2byte (card_type_len + cmd_len + data_len)
    
    card_type: 1byte
        01 MS
        02 IC
        03 RF
        
    cmd: 1byte
        01 read
        02 
        FF 错误
        
    data: 
    
    check: 1byte 
        xor
    */
    
    // head  len    type  cmd       data   xor 
    // A5 5A 02 00  01    01               00
    // A5 5A 02 00  02    01               00
    // A5 5A 02 00  03    01               00
    // A5 5A 02 00  03    02               00
    // A5 5A 02 00  01    FF               00
    
struct s_to_pad_msg
{
    char head[2];
    unsigned short len;
    unsigned char card_type;
    char cmd;
    char data[256];
    unsigned char checkbit;
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

#if 0
// 接收UART2的数据
int read_uart2_data(unsigned char uart, char *recv_buf)
{
    int res = 0;
    int count = 0;
    struct timeval tv = {0, 200000};
    struct timeval start, end; 
    memset(&start, 0, sizeof(struct timeval));
    memset(&end, 0, sizeof(struct timeval));
    
    if (recv_buf == NULL)
    {
        PRINT("buf is NULL!\n");
        return NULL_ERR;
    }
    gettimeofday(&start, NULL); // 得到当前时间 
    	
	while(1)
    {
        gettimeofday(&end, NULL); // 得到当前时间 
        
        if ((end.tv_sec - start.tv_sec) >= 30)
        {
            PRINT("timeout 30s\n");
            return TIMEOUT_ERR;
        }
        // 00ms没有数据，则退出
        tv.tv_usec = 200000;
        if ((res = spi_rt_interface.spi_select(uart, &tv)) < 0)
		{
		    if (count > 0)
		    {		        
		        return count;
		    }
		    PERROR("spi_select failed or no data. res = %d, tv.tv_sec = %d, tv.tv_usec = %d\n", res, tv.tv_sec, tv.tv_usec);
		    continue;
		}
		
		if ((res = spi_rt_interface.recv_data(uart, recv_buf + count, 1)) < 0)
		{
		    if (count > 0)
		    {
		        return count;
		    } 
		    PERROR("recv_data failed!\n");
		    return res;
		}
		PRINT("%02X\n", (unsigned char)recv_buf[count]);
		if ((count == 0) && ((unsigned char)recv_buf[0] != 0xA5))
		{
		    continue;
	    }
		count++;
	}
}
#else

// 接收UART2的数据
int read_uart2_data(unsigned char uart, char *recv_buf)
{
    int len = 0;
    int res = 0;
    int count = 0;
    struct timeval tv = {0, 200000};
    struct timeval start, end; 
    memset(&start, 0, sizeof(struct timeval));
    memset(&end, 0, sizeof(struct timeval));
    
    // A5 08 12 81 10 55555555555555555555555555555555 8B
    
    if (recv_buf == NULL)
    {
        PRINT("buf is NULL!\n");
        return NULL_ERR;
    }
    gettimeofday(&start, NULL); // 得到当前时间 
    
	for (count = 0; count < 3;)
    {
        gettimeofday(&end, NULL); // 得到当前时间 
        
        if ((end.tv_sec - start.tv_sec) >= 21)
        {
            PRINT("timeout 21s\n");
            return TIMEOUT_ERR;
        }
        // 00ms没有数据，则退出
        tv.tv_usec = 200000;
        if ((res = spi_rt_interface.spi_select(uart, &tv)) < 0)
		{
		    if (count > 0)
		    {		        
		        return count;
		    }
		    PERROR("spi_select failed or no data. res = %d, tv.tv_sec = %d, tv.tv_usec = %d\n", res, tv.tv_sec, tv.tv_usec);
		    continue;
		}
		
		if ((res = spi_rt_interface.recv_data(uart, recv_buf + count, 1)) < 0)
		{
		    if (count > 0)
		    {
		        return count;
		    } 
		    PERROR("recv_data failed!\n");
		    return res;
		}
		PRINT("%02X\n", (unsigned char)recv_buf[count]);
		if ((count == 0) && ((unsigned char)recv_buf[0] != 0xA5))
		{
		    continue;
	    }
		count++;
	}
	
	len = recv_buf[2] + 1;
	
	while (1)
	{
        gettimeofday(&end, NULL); // 得到当前时间 
        
        if ((end.tv_sec - start.tv_sec) >= 30)
        {
            PRINT("timeout 30s\n");
            return TIMEOUT_ERR;
        }
        // 200ms没有数据，则退出
        tv.tv_usec = 200000;
        if ((res = spi_rt_interface.spi_select(uart, &tv)) < 0)
		{
		    if (count > 0)
		    {		        
		        return count;
		    }
		    PERROR("spi_select failed or no data. res = %d, tv.tv_sec = %d, tv.tv_usec = %d\n", res, tv.tv_sec, tv.tv_usec);
		    continue;
		}
		
		if ((res = spi_rt_interface.recv_data(uart, recv_buf + count, len)) < 0)
		{
		    if (count > 0)
		    {
		        return count;
		    }
		    PERROR("recv_data failed!\n");
		    return res;
		}
		return (count + res);
	}
}

#endif

// 读取卡信息 磁条卡 magnetic stripe、IC卡、射频卡 rf
int read_card_info(unsigned char card_type, char *recv_buf)
{
    int res = 0;
    int i = 0;
    char recv_buf_tmp[256] = {0};
    unsigned char data_len = 0;
    if (recv_buf == NULL)
    {
        PRINT("buf is NULL!\n");
        return NULL_ERR;
    }
    
    switch (card_type)
    {
        case MS:
        {
            char send_ms_buf[7] = {0xA5, 0x01, 0x03, 0x15, 0x00, 0x00, 0x17}; // 读取1 2 3磁道信息
            
            unsigned char magnetic_track1_len = 0;
            unsigned char magnetic_track2_len = 0;
            unsigned char magnetic_track3_len = 0;
            
            char magnetic_track1[128] = {0};
            char magnetic_track2[128] = {0};
            char magnetic_track3[128] = {0};
            
            if ((res = spi_rt_interface.send_data(UART2, send_ms_buf, sizeof(send_ms_buf))) < 0)
            {
                PERROR("send_data failed!\n");
                return res;
            }
            #if 0
            if ((res = read_uart2_data(UART2, recv_buf_tmp)) < 0)
            {
                PERROR("read_uart2_data failed!\n");
                return res;
            }
            PRINT_BUF_BY_HEX(recv_buf_tmp, NULL, res, __FILE__, __FUNCTION__, __LINE__);
            
            // 解包
            if ((unsigned char)recv_buf_tmp[0] != 0xA5)
            {
                PRINT("data error!\n");
                return DATA_ERR;
            }
            
            if((unsigned char)recv_buf_tmp[1] == 0x01)
			{
			    magnetic_track1_len = recv_buf_tmp[4];
				if(magnetic_track1_len > 0)
				{
				    magnetic_track1_len -= 3;
					memcpy(magnetic_track1, recv_buf_tmp + 6, magnetic_track1_len);
				}
				
				magnetic_track2_len = recv_buf_tmp[magnetic_track1_len + 10];
				if(magnetic_track2_len > 0)
				{
				    magnetic_track2_len -= 3;
					memcpy(magnetic_track2, recv_buf_tmp + magnetic_track1_len + 10 + 2, magnetic_track2_len);
				}
				
				magnetic_track3_len = recv_buf_tmp[magnetic_track1_len + 10 + magnetic_track2_len + 6];
				if(magnetic_track3_len > 0)
				{
				    magnetic_track3_len -= 3;
					memcpy(magnetic_track3, recv_buf_tmp + magnetic_track1_len + 10 + magnetic_track2_len + 6 + 2,magnetic_track3_len);
				}
				
				sprintf(recv_buf, "%s;%s;%s", magnetic_track1, magnetic_track2, magnetic_track3);				
			}
			else
			{
			    PRINT("data error!\n");
                return DATA_ERR;
			}
			#else
			for (i = 0; i < 3; i++)
			{
			    if ((res = read_uart2_data(UART2, recv_buf_tmp)) < 0)
                {
                    PERROR("read_uart2_data failed!\n");
                    return res;
                }
                PRINT_BUF_BY_HEX(recv_buf_tmp, NULL, res, __FILE__, __FUNCTION__, __LINE__);
                
                // 解包
                if ((unsigned char)recv_buf_tmp[0] != 0xA5)
                {
                    PRINT("data error!\n");
                    return DATA_ERR;
                }
                
                if((unsigned char)recv_buf_tmp[1] == 0x01)
    			{
    			    if (recv_buf_tmp[3] == 0x11)
    			    {
    			        magnetic_track1_len = recv_buf_tmp[4];
    			        if(magnetic_track1_len > 0)
        				{
        				    // 如果磁条卡有信息 第一字节为‘;’，和最后两个字节 -- 第一磁道需要验证
        				    magnetic_track1_len -= 3;
        					memcpy(magnetic_track1, recv_buf_tmp + 6, magnetic_track1_len);
        				}
    			    }
    			    else if (recv_buf_tmp[3] == 0x12)
    			    {
    			        magnetic_track2_len = recv_buf_tmp[4];
    			        if(magnetic_track2_len > 0)
        				{
        				    magnetic_track2_len -= 3;
        					memcpy(magnetic_track2, recv_buf_tmp + 6, magnetic_track2_len);
        				}
    			    }
    				else if (recv_buf_tmp[3] == 0x13)
    			    {
    			        magnetic_track3_len = recv_buf_tmp[4];
    			        if(magnetic_track3_len > 0)
        				{
        				    magnetic_track3_len -= 3;
        					memcpy(magnetic_track3, recv_buf_tmp + 6, magnetic_track3_len);
        				}
    			    }
    			}
    			else if((unsigned char)recv_buf_tmp[1] == 0x80) // 非读响应
				{
					// 超时或者读卡异常时 : A5 80 03 15 01 04 93
    			    if (recv_buf_tmp[5] == 0x04)
    			    {
    			        PRINT("timeout!\n");
    			        return TIMEOUT_ERR;
    			    }
    			    else
        			{
        			    PRINT("data error!\n");
                        return DATA_ERR;
        			}
    			}
    			else
    			{
    			    PRINT("data error!\n");
                    return DATA_ERR;
    			}
			}
			sprintf(recv_buf, "%s;%s;%s", magnetic_track1, magnetic_track2, magnetic_track3);
			#endif
			PRINT("recv_buf = %s\n", recv_buf);
			res = magnetic_track1_len + magnetic_track2_len + magnetic_track3_len + 2;
            break;
        }
        case IC:
        {
            char send_ic_buf[7] = {0xA5, 0x03, 0x03, 0x31, 0x30, 0x0A, 0x0B}; // IC卡
            
            if ((res = spi_rt_interface.send_data(UART2, send_ic_buf, sizeof(send_ic_buf))) < 0)
            {
                PERROR("send_data failed!\n");
                return res;
            }
            
            if ((res = read_uart2_data(UART2, recv_buf_tmp)) < 0)
            {
                PERROR("read_uart2_data failed!\n");
                return res;
            }
            PRINT_BUF_BY_HEX(recv_buf_tmp, NULL, res, __FILE__, __FUNCTION__, __LINE__);
            
            // 解包            
            if ((unsigned char)recv_buf_tmp[0] != 0xA5)
            {
                PRINT("data error!\n");
                return DATA_ERR;
            }
            
            if((unsigned char)recv_buf_tmp[1] == 0x03)
			{
			    data_len = recv_buf_tmp[4];
				if(data_len > 0)
				{
					memcpy(recv_buf, recv_buf_tmp + 5, data_len);
			    }
			}
			else if((unsigned char)recv_buf_tmp[1] == 0x82) // 非读响应
    		{
    		    // IC卡上电异常 A5 82 033101 0B BA
    		    // 超时或者读卡异常时 : A5 82 03 31 01 04 B5
    		    switch (recv_buf_tmp[5])
    		    {
    		        case 0x04: // 读取异常 或者 超时
    		        {
    		            PRINT("timeout!\n");
    		            res = TIMEOUT_ERR;
    		            break;
    		        }
    		        case 0x0B: // 先按超时处理
    		        {
    		            PRINT("timeout!\n");
    		            res = TIMEOUT_ERR;
    		            break;
    		        }
    		        default: // 先按数据错误
    		        {
    		            PRINT("data error!\n");
                        res = DATA_ERR;
    		            break;
    		        }
    		    }
    		    return res;
    		}
			else
			{
			    PRINT("data error!\n");
                return DATA_ERR;
			}
			res = data_len;
            break;   
        } 
        case RF:
        {
            char send_rf_buf[7] = {0xA5, 0x08, 0x03, 0x81, 0x00, 0x00, 0x8A}; // 射频卡
            
            if ((res = spi_rt_interface.send_data(UART2, send_rf_buf, sizeof(send_rf_buf))) < 0)
            {
                PERROR("send_data failed!\n");
                return res;
            }
            
            if ((res = read_uart2_data(UART2, recv_buf_tmp)) < 0)
            {
                PERROR("read_uart2_data failed!\n");
                return res;
            }
            PRINT_BUF_BY_HEX(recv_buf_tmp, NULL, res, __FILE__, __FUNCTION__, __LINE__);
            
            // 解包
            if ((unsigned char)recv_buf_tmp[0] != 0xA5)
            {
                PRINT("data error!\n");
                return DATA_ERR;
            }
            
            if((unsigned char)recv_buf_tmp[1] == 0x08)
			{
			    data_len = recv_buf_tmp[4];
				if(data_len > 0)
				{
					memcpy(recv_buf, recv_buf_tmp + 5, data_len);
			    }
			}
			else if((unsigned char)recv_buf_tmp[1] == 0x87) // 非读响应
    		{
    		    // 超时或者读卡异常时 :A5 87 03 81 01 04 00
    		    if (recv_buf_tmp[5] == 0x04)
    		    {
    		        PRINT("timeout!\n");
    		        return TIMEOUT_ERR;
    		    }
    		    else
        		{
        		    PRINT("data error!\n");
                    return DATA_ERR;
        		}
    		}
			else
			{
			    PRINT("data error!\n");
                return DATA_ERR;
			}
			res = data_len;
            break;   
        }
        default:
        {
            PRINT("Data does not match!\n");
            return MISMATCH_ERR;
        }    
    }    
    return res;
}

/**
 * 和pad交互数据包打包
 */
int to_pad_msg_pack(struct s_to_pad_msg *to_pad_msg, char **send_buf)
{
    int res = 0;
    unsigned short send_buf_len = 0;
    
    if (to_pad_msg == NULL)
    {
        PRINT("to_pad_msg is NULL!\n");
        return NULL_ERR;
    }
    
    if ((*send_buf = malloc(to_pad_msg->len + 5)) == NULL)
    {
        PERROR("malloc failed!\n");
        return MALLOC_ERR;
    }
    memcpy(*send_buf, to_pad_msg->head, sizeof(to_pad_msg->head));
    send_buf_len += sizeof(to_pad_msg->head);
    
    memcpy(*send_buf + send_buf_len, &(to_pad_msg->len), sizeof(to_pad_msg->len));
    send_buf_len += sizeof(to_pad_msg->len);
    
    memcpy(*send_buf + send_buf_len, &(to_pad_msg->card_type), sizeof(to_pad_msg->card_type));
    send_buf_len += sizeof(to_pad_msg->card_type);
    
    memcpy(*send_buf + send_buf_len, &(to_pad_msg->cmd), sizeof(to_pad_msg->cmd));
    send_buf_len += sizeof(to_pad_msg->cmd);
    
    if (to_pad_msg->len > 2)
    {
        memcpy(*send_buf + send_buf_len, to_pad_msg->data, to_pad_msg->len - 2);
        send_buf_len += (to_pad_msg->len - 2);
    }

    res = common_tools.get_checkbit(*send_buf, NULL, 4, to_pad_msg->len, XOR, 1);
    if ((res == MISMATCH_ERR) || (res == NULL_ERR))
    {
        PERROR("get_checkbit failed!\n");
        free(send_buf);
        send_buf = NULL;
        return res;
    }
    to_pad_msg->checkbit = (unsigned char)res;
    memcpy(*send_buf + send_buf_len, &(to_pad_msg->checkbit), sizeof(to_pad_msg->checkbit));
    send_buf_len += sizeof(to_pad_msg->checkbit);
    
    PRINT_BUF_BY_HEX(*send_buf, NULL, send_buf_len, __FILE__, __FUNCTION__, __LINE__);
    return send_buf_len;
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
    
    struct s_to_pad_msg to_pad_msg;
    memset(&to_pad_msg, 0, sizeof(to_pad_msg));
    
    unsigned char checkbit = 0;
    
	char recv_buf[256] = {0};
	char *send_buf = NULL;
	
	unsigned char card_type = 0;
	
  	fd_set fdset;
  	struct timeval tv;
  	struct timeval timeout = {5, 0};
  	struct sockaddr_in client; 
    socklen_t len = sizeof(client); 
    
  	int sock_server_fd = 0, accept_fd = 0;
  	int res = 0;
  	
  	// 创建服务器
  	if ((sock_server_fd = internetwork_communication.make_server_link(PORT)) < 0)
  	{
  	    PERROR("make_server_link failed!\n");
  	    return sock_server_fd;
  	}
  	
  	while (1)
  	{
        FD_ZERO(&fdset);
        FD_SET(sock_server_fd, &fdset);       
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        switch(select(sock_server_fd + 1, &fdset, NULL, NULL, &tv))
        {
            case -1:
            case 0:
            {              
                PERROR("waiting swipe card cmd...\n");
                usleep(10000);
                continue;
            }
            default:
            {
                if (FD_ISSET(sock_server_fd, &fdset) > 0)
                {
                    if ((accept_fd = accept(sock_server_fd, (struct sockaddr*)&client, &len)) < 0)
                	{	
                        PERROR("accept failed!\n");
                        continue;
                	}
                    PRINT("sock_server_fd = %d, accept_fd = %d\n", sock_server_fd, accept_fd);    
                    
                    // 接收PAD命令
                    timeout.tv_sec = 5;
                    to_pad_msg.head[0] = 0xA5;
                    to_pad_msg.head[1] = 0x5A;
                    if ((res = common_tools.recv_data_head(accept_fd, to_pad_msg.head, sizeof(to_pad_msg.head), &timeout)) < 0)
                    {
                        PERROR("recv_data_head failed!\n");
                        break;
                    }
                    
                    if ((res = common_tools.recv_data(accept_fd, (char*)&(to_pad_msg.len), NULL, sizeof(to_pad_msg.len), &timeout)) < 0)
                    {
                        PERROR("recv_data failed!\n");
                        break;
                    }
                    
                    if ((res = common_tools.recv_one_byte(accept_fd, &to_pad_msg.card_type, &timeout)) < 0)
                    {
                        PERROR("recv_one_byte failed!\n");
                        break;
                    }
                    
                    if ((res = common_tools.recv_one_byte(accept_fd, &to_pad_msg.cmd, &timeout)) < 0)
                    {
                        PERROR("recv_one_byte failed!\n");
                        break;
                    }
                    
                    if (to_pad_msg.len > 2)
                    {
                        if (to_pad_msg.len > 256)
                        {
                            PRINT("len > 256!\n");
                            res = DATA_ERR;
                            break;
                        }
                        
                        if ((res = common_tools.recv_data(accept_fd, to_pad_msg.data, NULL, to_pad_msg.len - 2, &timeout)) < 0)
                        {
                            PERROR("recv_data failed!\n");
                            break;
                        }
                        
                        res = common_tools.get_checkbit(to_pad_msg.data, NULL, 0, to_pad_msg.len - 2, XOR, 1);
                        if ((res == MISMATCH_ERR) || (res == NULL_ERR))
                        {
                            PERROR("get_checkbit failed!\n");
                            break;
                        }
                        
                        checkbit = (unsigned char)res  ^ to_pad_msg.card_type ^ to_pad_msg.cmd;
                    }
                    else
                    {
                        checkbit = to_pad_msg.card_type ^ to_pad_msg.cmd;
                    }
                    
                    if ((res = common_tools.recv_one_byte(accept_fd, &to_pad_msg.checkbit, &timeout)) < 0)
                    {
                        PERROR("recv_one_byte failed!\n");
                        break;
                    }
                    
                    if (to_pad_msg.checkbit != checkbit)
                    {
                        PERROR("check is different!\n");
                        res = P_CHECK_DIFF_ERR;
                        break;
                    }
                    
                    // 根据命令读取UART2
                    switch (to_pad_msg.card_type)
                    {
                        case 0x01:
                        {
                            card_type = MS;
                            break;   
                        }
                        case 0x02:
                        {
                            card_type = IC;
                            break;   
                        }
                        case 0x03:
                        {
                            card_type = RF;
                            break;   
                        }
                        default:
                        {
                            PRINT("option is mismatchig!\n");
                            res = MISMATCH_ERR;
                            break;
                        }
                    }
                    if (res < 0)
                    {
                        break;
                    }
                    
                    switch (to_pad_msg.cmd)
                    {
                        case 0x01:
                        {
                            if ((res = read_card_info(card_type, recv_buf)) < 0)
                            {
                                PERROR("read_card_info failed!\n");
                            }
                            break;
                        }
                        default:
                        {
                            PRINT("option is mismatchig!\n");
                            res = MISMATCH_ERR;
                            break;
                        }
                    }
                    if (res < 0)
                    {
                        break;
                    }
                    PRINT("res = %d\n", res);
                    
                    // 打包
                    memcpy(to_pad_msg.data, recv_buf, res);
                    to_pad_msg.len = res + 2;
                    if ((res = to_pad_msg_pack(&to_pad_msg, &send_buf)) < 0)
                    {
                        PERROR("to_pad_msg_pack failed!\n");
                        break;
                    }
                    PRINT("res = %d\n", res);
                    
                    // 把数据发送到PAD
                    timeout.tv_sec = 5;
                    if ((res = common_tools.send_data(accept_fd, send_buf, NULL, res, &timeout)) < 0)
                    {
                        PERROR("send_data failed!\n");
                        free(send_buf);
                        send_buf = NULL;
                        break;
                    }
                }                
            }
        }
        
        if (res < 0)
        {
            memset(to_pad_msg.data, 0, sizeof(to_pad_msg.data));
            if (res == DATA_ERR)
            {
                strcpy(to_pad_msg.data, "data error!");
            }
            else if (res == TIMEOUT_ERR)
            {
                strcpy(to_pad_msg.data, "timeout error!");
            }
            else
            {
                sprintf(to_pad_msg.data, "failed. res = %d\n", res);
            }
            
            to_pad_msg.cmd = 0xFF;
            to_pad_msg.len = strlen(to_pad_msg.data) + 2;
            PRINT("to_pad_msg.data = %s\n", to_pad_msg.data);
            // 打包
            if ((res = to_pad_msg_pack(&to_pad_msg, &send_buf)) < 0)
            {
                PERROR("to_pad_msg_pack failed!\n");
            }
            
            if (res > 0)
            {
                if ((res = common_tools.send_data(accept_fd, send_buf, NULL, res, &timeout)) < 0)
                {
                    PERROR("send_data failed!\n");
                }
            }
        }
        
        // 资源释放
        if (accept_fd != 0)
        {
            close(accept_fd);
            accept_fd = 0;
        }
        res = 0;
        
        if (send_buf != NULL)
        {
            free(send_buf);
            send_buf = NULL;
        }
        memset(recv_buf, 0, sizeof(recv_buf));
        memset(&to_pad_msg, 0, sizeof(struct s_to_pad_msg));
  	}
  	
  	close(sock_server_fd);
	return 0; 
} 
