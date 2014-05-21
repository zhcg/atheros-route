#include "communication_network.h"

/**
 * 获得一个服务器fd 2013-01-08
 */
static int make_server_link(const char *port);

/**
 * 根据建立的服务器连接 获取来自客户端连接 2013-01-08
 */
static int get_socket_client_fd(const int sock_fd);

/**
 * 建立一个TCP客户端连接，获得socket 2013-01-08
 */
static int make_client_link(const char *ip, const char *port);

/**
 * 把通过tcp连接向平台发送的数据进行打包
 */
static int IP_msg_pack(struct s_data_list *a_data_list, struct s_dial_back_respond *a_dial_back_respond);

/**
 * 把通过tcp连接接收自平台的数据进行解包
 */
static int IP_msg_unpack(char *buf, unsigned short buf_len, struct s_dial_back_respond *a_dial_back_respond);

/**
 * 通过tcp发送链表中存放的数据
 */
static int IP_msg_send(int fd, struct s_data_list *a_data_list);

/**
 * 接收tcp连接发送的数据，并把数据发到链表中
 */
static int IP_msg_recv(int fd, char *buf, unsigned short buf_len);

/**
 * 初始化结构体
 */
struct class_communication_network communication_network = 
{
    make_server_link, get_socket_client_fd, 
    make_client_link,
    IP_msg_pack, IP_msg_unpack, IP_msg_send, IP_msg_recv
};

/**
 * 获得一个服务器fd 2013-01-08
 */
int make_server_link(const char *port)
{
    PRINT_STEP("entry...\n");
    int on = 1;
    int sock_fd = 0;
	struct sockaddr_in server;
	char buf[64] = {0};
	
	if (port == NULL)
	{
	    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "port is NULL!", NULL_ERR);
	    return NULL_ERR;
	}
	// 建立套接字		
	sock_fd = socket(PF_INET, SOCK_STREAM, 0);  		
	if (sock_fd < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "server socket failed!", SOCKET_ERR);
		return SOCKET_ERR;
	}
	PRINT("sock_fd = %d, port = %s\n", sock_fd, port);
	//设置套接字选项:SO_REUSEADDR,避免重复使用时，地址错误
	if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "server setsockopt failed!", SETSOCKOPT_ERR);
		close(sock_fd);
		return SETSOCKOPT_ERR;
	}
	
	// 使用命令行中指定的名字连接套接字
	server.sin_family = PF_INET;  
	//server.sin_addr.s_addr = htons(INADDR_ANY);		        //0.0.0.0 接收任意IP连接
	server.sin_addr.s_addr = INADDR_ANY;		        //0.0.0.0 接收任意IP连接
	server.sin_port = htons(atoi(port));	//指定连接的端口
	
	sprintf(buf, "port:%s; server bind start!", port);
	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, buf, 0);
	PRINT("sock_fd = %d, port = %s\n", sock_fd, port);
	//将struct sockaddr 绑定到套接字上
	if (bind(sock_fd, (struct sockaddr*)&server, sizeof(server)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "server bind failed!", BIND_ERR);
		close(sock_fd);
		return BIND_ERR;
	}
	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "server bind ok!", 0);
	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "server listen start!", 0);
	//（监听）只允许10 IP连接到当前的套接字上
	if (listen(sock_fd, 10) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "server listen failed!", LISTEN_ERR);
		close(sock_fd);
		return LISTEN_ERR;
	}
	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "server listen ok!", 0);
	PRINT_STEP("exit...\n");
	return sock_fd;  
}

/**
 * 根据建立的服务器连接 获取来自客户端连接 2013-01-08
 */
int get_socket_client_fd(const int sock_fd)
{
    PRINT_STEP("entry...\n");
    int res = 0;
    int accept_fd = 0;
    
    //用来保存来自于客户端的信息
    struct sockaddr_in client;  
    socklen_t len = sizeof(client);  
    
	fd_set fdset;
    FD_ZERO(&fdset);
	FD_SET(sock_fd, &fdset);
	struct timeval timeout = {common_tools.config->total_timeout, 0};
	
	//轮训集合，查看是否有事件发生，有就直接返回
	res = select(sock_fd + 1, &fdset, NULL, NULL, &timeout);
		
	if (res == -1)
	{
	    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "server select failed!", SELECT_ERR);
	  	return SELECT_ERR;
	}   
	else if (res == 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "socket no event!", SELECT_NULL_ERR);
        return SELECT_NULL_ERR;
	}
	
	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "server accept start!", 0);
    //接受连接请求
	if ((accept_fd = accept(sock_fd, (struct sockaddr*)&client, &len)) < 0)
	{	
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "accept failed!", ACCEPT_ERR);
		close(sock_fd);
		return ACCEPT_ERR;
	}
	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "accept ok!", 0);
	PRINT_STEP("exit...\n");
	return accept_fd;
}

/**
 * 建立一个TCP客户端连接，获得socket 2013-01-08
 */
int make_client_link(const char *ip, const char *port)
{
    PRINT_STEP("entry...\n");
    int res = 0;
    int len = 0;
    int sock_fd = 0;
	struct sockaddr_in client;
    char buf[128] = {0};
	// 1.建立套接字  	
	if ((sock_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) 
	{  		
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "client socket failed!", SOCKET_ERR);        
		return SOCKET_ERR;  
	}  
	
	// 2.使用命令行中指定的名字连接套接字
	client.sin_family = PF_INET;
	client.sin_addr.s_addr = inet_addr(ip);
	client.sin_port = htons(atoi(port));
	sprintf(buf, "ip = %s, port = %s; connect start!", ip, port);
	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, buf, 0);
	
	// 3.设置为非阻塞
	if (fcntl(sock_fd, F_SETFL, fcntl(sock_fd, F_GETFL, 0) | O_NONBLOCK) < 0)
	{
	    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "fcntl failed!", FCNTL_ERR);        
        close(sock_fd);
		return FCNTL_ERR; 
	}
	
	// 4.建立connect连接，此时socket设置为非阻塞，connect调用后，无论连接是否建立立即返回-1
	//   同时将errno设置为EINPROGRESS, 表示此时tcp三次握手仍旧进行
	res = connect(sock_fd, (struct sockaddr*)&client, sizeof(client));
	if (res == -1)
    {
        if (errno != EINPROGRESS) // errno不是EINPROGRESS，则说明连接错误，程序结束
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "connect failed!", CONNECT_ERR);        
            close(sock_fd);
            return CONNECT_ERR;
        }
        
        struct timeval tv = {5, 0};
        fd_set rfdset, wfdset; 
        FD_ZERO(&rfdset);
        FD_ZERO(&wfdset);
        FD_SET(sock_fd, &rfdset);
        FD_SET(sock_fd, &wfdset);
        
        switch (select(sock_fd + 1, &rfdset, &wfdset, NULL, &tv))
        {
            case -1:
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "select failed!", SELECT_ERR);
                close(sock_fd);
                return SELECT_ERR;
            }
            case 0:
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "select no data recv!", SELECT_NULL_ERR);
                close(sock_fd);
                return SELECT_NULL_ERR;
            }
            default:
            {
                if ((FD_ISSET(sock_fd, &rfdset) > 0) || (FD_ISSET(sock_fd, &wfdset) > 0)) 
                {
                    len = sizeof(res);
                    getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &res, (socklen_t *)&len);    
                    if (res == 0)
                    {
                        PRINT("connect ok!\n");
                    }
                    else 
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "connect failed!", CONNECT_ERR);
                        close(sock_fd);
                        return CONNECT_ERR;
                    }
                }
                else 
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "connect failed!", CONNECT_ERR);
                    close(sock_fd);
                    return CONNECT_ERR;
                }
                break;
            }      
        }
	}
	else if (res == 0) // 当客户端和服务器端在同一台主机上的时候，connect回马上结束，并返回0
    {
        PRINT("local server\n");
    }
	
    // 设置为阻塞
	fcntl(sock_fd, F_SETFL, fcntl(sock_fd, F_GETFL, 0) & ~O_NONBLOCK);
	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "connect ok!", 0);
	PRINT_STEP("exit...\n");
	return sock_fd;  
}

/**
 * 把通过tcp连接向平台发送的数据进行打包
 */
int IP_msg_pack(struct s_data_list *a_data_list, struct s_dial_back_respond *a_dial_back_respond)
{
    PRINT_STEP("entry...\n");
    int res = 0;
    int len = 0;
    int i = 0;
    char dest[8] = {0};
    char reserve[2] = {0};
    
    struct s_IP_msg IP_msg;
    memset((void *)&IP_msg, 0, sizeof(struct s_IP_msg));
    common_tools.list_free(a_data_list);
    
    IP_msg.version = 0x02;
    IP_msg.cmd = htons(a_dial_back_respond->cmd);
    switch (a_dial_back_respond->cmd)
    {
        case 0x1000: // 请求
        {
            IP_msg.data_len += strlen(a_dial_back_respond->BASE_id);
            IP_msg.data_len += strlen(a_dial_back_respond->PAD_id);
            IP_msg.data_len += sizeof(dest);
            IP_msg.data_len += sizeof(reserve);
            
            if ((IP_msg.valid_data = malloc(IP_msg.data_len + 1)) == NULL)
            {                
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR); 
                return MALLOC_ERR;
            }
            sprintf(IP_msg.valid_data, "%s%s", a_dial_back_respond->BASE_id, a_dial_back_respond->PAD_id);
            
            memcpy(IP_msg.valid_data + strlen(a_dial_back_respond->BASE_id) + strlen(a_dial_back_respond->PAD_id), dest, sizeof(dest));
            
            // 预留
            memcpy(IP_msg.valid_data + strlen(a_dial_back_respond->BASE_id) + strlen(a_dial_back_respond->PAD_id) + sizeof(dest), reserve, sizeof(reserve));
            break;
        }
        case 0x1002: // 身份确认请求
        {
            IP_msg.data_len += sizeof(dest);
            IP_msg.data_len += sizeof(reserve);
            
            if ((IP_msg.valid_data = malloc(IP_msg.data_len + 1)) == NULL)
            {                
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);                
                return MALLOC_ERR;
            }
            
            memcpy(IP_msg.valid_data, dest, sizeof(dest));
            memcpy(IP_msg.valid_data + sizeof(dest), reserve, sizeof(reserve));
            break;
        }
        case 0x2000: // 请求
        {
            IP_msg.data_len += strlen(a_dial_back_respond->BASE_id);
            IP_msg.data_len += TOKENLEN;
            IP_msg.data_len += sizeof(dest);
            
            if ((IP_msg.valid_data = malloc(IP_msg.data_len + 1)) == NULL)
            {                
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);                
                return MALLOC_ERR;
            }
            memcpy(IP_msg.valid_data, a_dial_back_respond->BASE_id, strlen(a_dial_back_respond->BASE_id));
            
            // 设备令牌加密
            
            memcpy(IP_msg.valid_data + strlen(a_dial_back_respond->BASE_id), a_dial_back_respond->device_token, TOKENLEN);
            PRINT_BUF_BY_HEX(a_dial_back_respond->device_token, NULL, TOKENLEN, __FILE__, __FUNCTION__, __LINE__);
            
            memcpy(IP_msg.valid_data + strlen(a_dial_back_respond->BASE_id) + TOKENLEN, dest, sizeof(dest));
            break;
        }
        case 0x2003: // 接收呼叫请求
        {
            IP_msg.data_len++;
            IP_msg.data_len += sizeof(reserve[0]);
            if ((IP_msg.valid_data = malloc(IP_msg.data_len + 1)) == NULL)
            {                
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);                
                return MALLOC_ERR;
            }
            IP_msg.valid_data[0] = a_dial_back_respond->call_permit; // 0 可以呼叫 1 不同意呼叫
            IP_msg.valid_data[1] = reserve[0];
            break;
        }
        case 0x2004: // 呼叫结果请求
        {
            IP_msg.data_len += sizeof(reserve[0]);
            if ((IP_msg.valid_data = malloc(IP_msg.data_len + 1)) == NULL)
            {                
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);                
                return MALLOC_ERR;
            }
            IP_msg.valid_data[0] = a_dial_back_respond->call_result; 
            break;
        }
        default:
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "option does not mismatch!", S_MISMATCH_ERR); 
            return S_MISMATCH_ERR;
        }
    }
    
    #if ENDIAN == 1
    len = DATA_ENDIAN_CHANGE_SHORT(IP_msg.data_len);
    IP_msg.data_len = len;
    len = DATA_ENDIAN_CHANGE_SHORT(IP_msg.data_len);
    PRINT("len = %04X, IP_msg.data_len = 04X\n", len, IP_msg.data_len);
    #endif
    
    // 加入链表
    for (i = 6; i < 11; i++)
    {
        if ((res = common_tools.list_tail_add_data(a_data_list, *((char *)(&IP_msg) + i))) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_tail_add_data failed!", res);
            free(IP_msg.valid_data);
            IP_msg.valid_data = NULL;
            return res;
        }        
    }
    for (i = 0; i < len; i++)
    {
        if ((res = common_tools.list_tail_add_data(a_data_list, IP_msg.valid_data[i])) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_tail_add_data failed!", res);
            free(IP_msg.valid_data);
            IP_msg.valid_data = NULL;
            return res;
        }
    }
    free(IP_msg.valid_data);
    IP_msg.valid_data = NULL;
    
    res = common_tools.get_checkbit(NULL, a_data_list, 6, (sizeof(IP_msg) - sizeof(IP_msg.sync_head) - sizeof(IP_msg.sync_tail) - 5 + len), SUM, 1);
    if ((res == MISMATCH_ERR) || (res == NULL_ERR))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "get_checkbit failed!", res);
        return res;
    }
    IP_msg.check_sum = (char)res;
    
    if ((res = common_tools.list_tail_add_data(a_data_list, IP_msg.check_sum)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_tail_add_data failed!", res);
        return res;
    }
    PRINT_BUF_BY_HEX(NULL, a_data_list, a_data_list->list_len, __FILE__, __FUNCTION__, __LINE__);
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 把通过tcp连接接收自平台的数据进行解包
 */
int IP_msg_unpack(char *buf, unsigned short buf_len, struct s_dial_back_respond *a_dial_back_respond)
{
    PRINT_STEP("entry...\n");
    int res = 0;
    int index = 0;
    char answer[2] = {0};
    char src[8] = {0};
    char token[TOKENLEN] = {0};
    struct s_IP_msg IP_msg;
    
    IP_msg.version = buf[0];
    if (IP_msg.version != 0x02)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
        return DATA_ERR;
    }
    index++;
    
    memcpy((void *)&IP_msg.cmd, buf + index, sizeof(IP_msg.cmd));
    index += sizeof(IP_msg.cmd);
    
    memcpy((void *)&IP_msg.data_len, buf + index, sizeof(IP_msg.data_len));
    index += sizeof(IP_msg.data_len);
    
    #if ENDIAN == 1 // 大端
    IP_msg.data_len = DATA_ENDIAN_CHANGE_SHORT(IP_msg.data_len);
    #endif
    
    PRINT("IP_msg.data_len = %d, buf_len - 6 = %d\n", IP_msg.data_len, buf_len - 6);
    if (IP_msg.data_len != (buf_len - 6))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", res);
        return DATA_ERR;
    }
    
    switch (ntohs(IP_msg.cmd))
    {
        case 0x1001:
        {
            char base_random[8] = {0};
            char system_random[8] = {0};
            
            // 获取终端随机数（密文）
            memcpy(src, buf + index, sizeof(src));
            index += sizeof(src);
            
            //PRINT_BUF_BY_HEX(a_dial_back_respond->base_random, NULL, sizeof(a_dial_back_respond->base_random), __FILE__, __FUNCTION__, __LINE__);
            //PRINT_BUF_BY_HEX(base_random, NULL, sizeof(base_random), __FILE__, __FUNCTION__, __LINE__);
            // 终端随机数对比
            if (memcmp(base_random, a_dial_back_respond->base_random, sizeof(base_random)) != 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "the data is different!", S_DATA_ERR);
                return S_DATA_ERR;  
            }
            
            // 获取系统随机数（密文）
            memcpy(system_random, buf + index, sizeof(system_random));
            index += sizeof(system_random);
            
            // 获取离散因子（密文）
            memset(src, 0, sizeof(src));
            memcpy(src, buf + index, sizeof(src));
            index += sizeof(src);
            // 应答
            memcpy(answer, buf + index, sizeof(answer));
            PRINT("answer = %04X\n", answer[0], answer[1]);
            if ((answer[0] == 0x00) && (answer[1] == 0x00)) // 成功
            {
                // 成功
            }
            else if ((answer[0] == 0x00) && (answer[1] == 0x01)) // BASE_id错误
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "base sn error!", SN_BASE_ERR);
                return SN_BASE_ERR;
            }
            else if ((answer[0] == 0x00) && (answer[1] == 0x02)) // PAD_id 错误
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "pad sn error!", SN_PAD_ERR);
                return SN_PAD_ERR;
            }
            else if ((answer[0] == 0x00) && (answer[1] == 0x03)) // 不匹配错误
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "pad and base is not match!", WRONGFUL_DEV_ERR);
                return WRONGFUL_DEV_ERR;
            }
            else 
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", DATA_ERR);
                return DATA_ERR;
            }
            index += sizeof(answer);
            break;
        }
        case 0x1003: // 颁发设备令牌
        {
            // 获取设备令牌（密文）
            memcpy(token, buf + index, sizeof(token));
            index += sizeof(token);        
          
            // 获取mac（密文）
            memset(src, 0, sizeof(src));
            memcpy(src, buf + index, sizeof(src));
            index += sizeof(src);
            
            // 应答
            memcpy(answer, buf + index, sizeof(answer));
            if ((answer[0] == 0x00) && (answer[1] == 0x00)) // 成功
            {
                // 成功
            }
            else if ((answer[0] == 0x00) && (answer[1] == 0x01)) // 随机数错误
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "system random error!", DATA_ERR);
                return L_DATA_ERR;
            }
            else 
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", DATA_ERR);
                return DATA_ERR;
            }
            index += sizeof(answer);
            break;
        }
        case 0x2001:
        {
            memcpy(answer, buf + index, sizeof(answer[0]));
            switch (answer[0])
            {
                case 0x00:
                {
                    break;
                }
                case 0x01: // 令牌错误
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "device token error!", DATA_ERR);
                    return L_DATA_ERR;
                }
                default:
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "option does not mismatch!", S_MISMATCH_ERR); 
                    return S_MISMATCH_ERR;
                }  
            }
            index += sizeof(answer[0]);
            break;
        }
        case 0x2002: // 平台呼叫
        {
            char phone_num[16] = {0};
            // 获取电话号码
            memcpy(phone_num, buf + index, sizeof(phone_num));
            index += sizeof(phone_num);
            
            // 暂时处理电话号码
            #if 1
            {
                PRINT("phone_num = %s\n", phone_num);
                PRINT("a_dial_back_respond->phone_num = %s\n", a_dial_back_respond->phone_num);
                memset(phone_num, 0, sizeof(phone_num));
                int i = 0;
                for (i = 0; i < sizeof(a_dial_back_respond->phone_num); i++)
                {
                    if ((a_dial_back_respond->phone_num[i] >= '0') && (a_dial_back_respond->phone_num[i] <= '9'))
                    {
                        phone_num[i] = a_dial_back_respond->phone_num[i];
                    }
                    else
                    {
                        break;
                    }
                }
                PRINT("i = %d\n", i);
                PRINT("phone_num = %s strlen(phone_num) = %d\n", phone_num, strlen(phone_num));
                memset(a_dial_back_respond->phone_num, 0, sizeof(a_dial_back_respond->phone_num));
                memcpy(a_dial_back_respond->phone_num, phone_num, strlen(phone_num));
                if ((i == 0) || (i < 7))
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", DATA_ERR);
                    return DATA_ERR;
                }
            }
            #endif
            
            // 获取离散因子
            memcpy(src, buf + index, sizeof(src));
            index += sizeof(src);
            
            // 获取MAC
            memset(src, 0, sizeof(src));
            memcpy(src, buf + index, sizeof(src));
            index += sizeof(src);
            
            // 预留
            memcpy(answer, buf + index, sizeof(answer));
            index += sizeof(answer);
            break;
        }
        case 0x2005: // 颁发位置令牌
        {
            // 获取位置令牌（密文）
            memcpy(token, buf + index, sizeof(token));
            index += sizeof(token);
            
            
            // 获取MAC
            memset(src, 0, sizeof(src));
            memcpy(src, buf + index, sizeof(src));
            index += sizeof(src);
            
            
            // 应答
            memcpy(answer, buf + index, sizeof(answer));
            if ((answer[0] == 0x00) && (answer[0] == 0x00)) // 成功
            {
                // 成功
            }
            else // 失败
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "position token get failed!", POSITION_AUTH_ERR);
                return POSITION_AUTH_ERR;
            }
            index += sizeof(answer);
            break;
        }
        default:
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "option does not mismatch!", S_MISMATCH_ERR); 
            return S_MISMATCH_ERR;
        }
    }
    
    IP_msg.check_sum = buf[index];
    
    res = common_tools.get_checkbit(buf, NULL, 0, buf_len - 1, SUM, 1);
    if ((res == MISMATCH_ERR) || (res == NULL_ERR))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "get_checkbit failed!", res);
        return res;
    }
    
    #if 1
    if ((char)res != IP_msg.check_sum)
    {        
        char print_info[128] = {0};
        sprintf(print_info, "checksums are different! check_sum = %d IP_msg.check_sum = %d", (char)res, IP_msg.check_sum);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_info, S_CHECK_DIFF_ERR);
        return S_CHECK_DIFF_ERR;
    }
    #endif
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 通过tcp发送链表中存放的数据
 */
int IP_msg_send(int fd, struct s_data_list *a_data_list)
{
    PRINT_STEP("entry...\n");
    int i = 0;
    int res = 0;
    struct s_list_node *list_node;
    fd_set fdset;
    FD_ZERO(&fdset);
	FD_SET(fd, &fdset);
	struct timeval timeout = {common_tools.config->total_timeout, 0};
	
    char *list_buf = malloc(a_data_list->list_len);
    if (list_buf == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);  
        return MALLOC_ERR;
    }
    for (i = 0, list_node = a_data_list->head; i < a_data_list->list_len; list_node = list_node->next, i++)
    {
        list_buf[i] = list_node->data;       
    }
    
    if (write(fd, list_buf, a_data_list->list_len) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", S_WRITE_ERR);
        free(list_buf);
        list_buf = NULL;
        return S_WRITE_ERR;
    }
    free(list_buf);
    list_buf = NULL;
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 接收tcp连接发送的数据，并把数据发到链表中
 */
int IP_msg_recv(int fd, char *buf, unsigned short buf_len)
{
    PRINT_STEP("entry...\n");
    int res = 0;
    struct timeval tv = {15, 0};
    fd_set fdset; 
    FD_ZERO(&fdset);
    FD_SET(fd, &fdset);
    
	switch (select(fd + 1, &fdset, NULL, NULL, &tv))
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
                if ((res = read(fd, buf, buf_len)) <= 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "read failed!", READ_ERR);
                    return READ_ERR;
                }
            }
            else
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "select no data recv!", SELECT_NULL_ERR);
                return SELECT_NULL_ERR;
            }
        }       
    }
    PRINT_BUF_BY_HEX(buf, NULL, res, __FILE__, __FUNCTION__, __LINE__);
    PRINT_STEP("exit...\n");
    return res;
}
