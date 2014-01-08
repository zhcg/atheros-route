/*************************************************************************
    > File Name: usb_stub_main.c
    > Author: yangjilong
    > mail1: yangjilong65@163.com
    > mail2: yang.jilong@handaer.com
    > Created Time: 2013年12月11日 星期三 17时07分37秒
**************************************************************************/
#include "communication_usb.h"

static volatile unsigned char link_flag = 1; // 
static volatile int client_fd = 0;

/** 
 * 接收 返回错误或者接收的字节数
 */
int recv_data(int fd, char *data, unsigned int data_len, struct timeval *tv)
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
    #if 0
    for (i = 0; i < data_len; i++)
    {        
        if ((res = common_tools.recv_one_byte(fd, &data[i], &time)) != 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_one_byte failed!", res);
            return i;
        }
    }
    #if PRINT_DATA
    printf("\n");
    #endif
    
    #else
    if ((res = read(fd, data, data_len)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "read failed!", res);
        return READ_ERR;
    }
    #endif
    
    return res;
}
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
            link_flag = 1; // 说明此链接已经关闭
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
 * 接收服务器发送的socket数据，并通过usb发送到PAD
 */
void * pthread_socket_recv(void *para)
{
    int res = 0;
    int i = 0;
    unsigned short buf_len = 0;
    fd_set fdset;
    struct timeval tv = {1, 0};
    char buf[SECTORS] = {0};
    
    struct s_usb_stub *usb = (struct s_usb_stub *)para;
    while (1) 
    {
        pthread_mutex_lock(&usb->mutex);
        if (link_flag == 1)
        {
            // 接收信号
            pthread_cond_wait(&usb->cond, &usb->mutex);
        }
        
        FD_ZERO(&fdset);
        FD_SET(client_fd, &fdset);
        tv.tv_sec = 1;
        switch (select(client_fd + 1, &fdset, NULL, NULL, &tv))
        {
            case 0:
            case -1:
            {
                PRINT("pthread_socket_recv waiting recv!\n");
                pthread_mutex_unlock(&usb->mutex);
                continue;
            }
            default :
            {
                if (FD_ISSET(client_fd, &fdset) > 0)
                {
                    if (link_flag == 1)
                    {
                        client_fd = 0;
                        link_flag = 0;
                        break;
                    }
                    PRINT("client_fd = %d\n", client_fd);
                    
                    // 读取服务器发送的数据
                    //tv.tv_usec = 300000;
                    if ((res = recv_data(client_fd, buf + 2, sizeof(buf) - 2, &tv)) <= 0)
                    {
                        PERROR("recv_data failed or no data!\n");
                        break;
                    }
                    buf_len = res;
                    PRINT("buf_len = %d %04X\n", buf_len, buf_len);
                    
                    // 大小端的问题
                    memcpy(buf, &buf_len, sizeof(buf_len));
                    #if 0
                    for (i = 0; i < 3; i++)
                    {
                        if ((res = communication_usb.usb_send(communication_usb.fd, buf, buf_len + 2)) < 0)
                        {
                            PERROR("usb_send failed!\n");
                            continue;
                        }
                        PRINT_BUF_BY_HEX(buf, NULL, buf_len + 2, __FILE__, __FUNCTION__, __LINE__);
                    }
                    
                    if (res < 0)
                    {
                        break;
                    }
                    #else
                    if ((res = communication_usb.usb_send(communication_usb.fd, buf, buf_len + 2)) < 0)
                    {
                        PERROR("usb_send failed!\n");
                        break;
                    }
                    PRINT_BUF_BY_HEX(buf, NULL, buf_len + 2, __FILE__, __FUNCTION__, __LINE__);
                    #endif
                    PRINT("usb_send success!\n");
                }
            } 
        }
        
        // 释放资源
        memset(buf, 0, sizeof(buf));
        buf_len = 0;
        
        pthread_mutex_unlock(&usb->mutex);
    }
    return (void *)res;
}


/**
 * 接收PAD通过usb发送的数据，并通过socket发送到服务器
 */
void * usb_recv_and_socket_send(void *para)
{
    int res = 0;
    short buf_len = 0;
    fd_set fdset;
    struct timeval tv = {5, 0};
    char buf[SECTORS] = {0};
    struct s_usb_stub *usb = (struct s_usb_stub *)para;
    char cmd_out[256] = {0};
    unsigned char signal_flag = 0;
    
    while (1) 
    {
        pthread_mutex_lock(&usb->mutex);
        
        FD_ZERO(&fdset);
        FD_SET(communication_usb.fd, &fdset);
        tv.tv_sec = 1;
        switch (select(communication_usb.fd + 1, &fdset, NULL, NULL, &tv))
        {
            case 0:
            case -1:
            {
                PRINT("pthread_stop_recv_cmd waiting recv cmd!\n");
                pthread_mutex_unlock(&usb->mutex);
                continue;
            }
            default :
            {
                if (FD_ISSET(communication_usb.fd, &fdset) > 0)
                {
                    buf_len = communication_usb.usb_recv(communication_usb.fd, buf, sizeof(buf));
                    if (buf_len < 0)
                    {
                        PERROR("usb_recv failed!\n");
                        res = buf_len;
                        break;
                    }
                    else if (buf_len == 0) 
                    {
                        PERROR("usb no data!\n");
                        res = SELECT_NULL_ERR;
                        break;
                    }
                    // 响应PAD
                    if ((res = communication_usb.usb_send_ack(communication_usb.fd)) < 0)
                    {
                        PERROR("usb_send_ack failed!\n");
                        break;
                    }
                    PRINT("send ack success!\n");
        
                    PRINT("This is a data packet!\n");
                    PRINT_BUF_BY_HEX(buf, NULL, buf_len, __FILE__, __FUNCTION__, __LINE__);
                    
                    // 判断是否已经建立连接 
                    if (client_fd == 0)
                    {
                        // 需要连接服务器
                        if ((res = communication_network.make_client_link("127.0.0.1", USB_SOCKET_PORT)) < 0)
                        {
                            PERROR("make_client_link failed!\n");
                            break;
                        }
                        // 
                        client_fd = res;
                        PRINT("client_fd = %d\n", client_fd);
                        link_flag = 0;
                        signal_flag = 1;
                    }
                    else 
                    {
                        if ((res = common_tools.get_cmd_out("netstat -atn | grep 127.0.0.1:9685 | grep ESTABLISHED", cmd_out, sizeof(cmd_out))) < 0)
                        {
                            PERROR("get_cmd_out failed!\n");
                            break;
                        }
                        
                        if (strlen(cmd_out) == 0)
                        {
                            if ((res = communication_network.make_client_link("127.0.0.1", USB_SOCKET_PORT)) < 0)
                            {
                                PERROR("make_client_link failed!\n");
                                break;
                            }
                            // 
                            client_fd = res;
                            PRINT("client_fd = %d\n", client_fd);
                            link_flag = 0;
                            signal_flag = 1;
                        }
                    }
                    
                    // 发送数据到服务器
                    if ((res = write(client_fd, buf, buf_len)) < 0)
                    {
                        PERROR("write failed!\n");
                        // 判断errno 看是否向一个已经关闭的连接中发送数据 信号捕捉
                        close(client_fd);
                        client_fd = 0;
                        link_flag = 1;
                        signal_flag = 0;
                        res = WRITE_ERR;
                        break;
                    }
                    PRINT("send to server success!\n"); 
                }
            } 
        }
        // link_flag == 0:说明建立连接，此时判断连接是否断开
        if (link_flag == 0)
        {
            memset(cmd_out, 0, sizeof(cmd_out));
            if (common_tools.get_cmd_out("netstat -atn | grep 127.0.0.1:9685 | grep ESTABLISHED", cmd_out, sizeof(cmd_out)) < 0)
            {
                PERROR("get_cmd_out failed!\n");
            }
            else 
            {
                if (strlen(cmd_out) == 0)
                {
                    PERROR("link close!\n");
                    if (client_fd != 0)
                    {
                        close(client_fd);
                        client_fd = 0;
                    }
                    link_flag = 1;
                }
            }
        }
        // 释放资源
        memset(buf, 0, sizeof(buf));
        buf_len = 0;
        memset(cmd_out, 0, sizeof(cmd_out));
        
        if (signal_flag == 1)
        {
            // 发信号
            pthread_cond_signal(&usb->cond);
            signal_flag = 0;
        }
        pthread_mutex_unlock(&usb->mutex);
        if (res < 0)
        {
            usleep(200000);
        }
    }
    return (void *)res;
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
    struct s_usb_stub usb;
    memset(&usb, 0, sizeof(struct s_usb_stub));
    
    if ((res = communication_usb.usb_init()) < 0)
    {
        PERROR("usb_init failed!\n");
        return res;
    }
    
    pthread_mutex_init(&usb.mutex, NULL);
    pthread_cond_init(&usb.cond, NULL);
    
    pthread_t pthread_socket_recv_t, pthread_usb_recv_and_socket_send_t;
    
    pthread_create(&pthread_socket_recv_t, NULL, (void *)pthread_socket_recv, (void *)&usb);
    pthread_create(&pthread_usb_recv_and_socket_send_t, NULL, (void *)usb_recv_and_socket_send, (void *)&usb);
    
    pthread_join(pthread_socket_recv_t, NULL);
    pthread_join(pthread_usb_recv_and_socket_send_t, NULL);
    
    pthread_mutex_destroy(&usb.mutex);
    pthread_cond_destroy(&usb.cond);
    return 0;
}

