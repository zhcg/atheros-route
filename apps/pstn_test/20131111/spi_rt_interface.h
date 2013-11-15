#ifndef _SPI_RT_INTERFACE_H_
#define _SPI_RT_INTERFACE_H_

#include "common_tools.h"
#include "internetwork_communication.h"
#include <sys/ipc.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define SPI_NAME	"/dev/spiS0"

#define SPI_SERVER_PORT "3738"
#define SPI_SERVER_IP "127.0.0.1" 

#define LOCAL_SOCKET 0
#define SPI_LOCAL_SOCKET_NAME "/var/terminal_init/log/.socket_name"

#define SPI_PARA_FILE    "/var/terminal_init/log/spi_para_file"

#define SHARED_MEM_SIZE 1024 * 4

// 因为协议中规定有效数据长度有1024，此处定义缓冲区长度为1024 * 2 + 6
//   包头        长度（两个字节）       有效数据          校验
//   0xA5 0x5A   有效数据 + 校验长度    10/20/30 + 数据   0x00 长度和有效数据的异或校验
//#define BUF_LEN 1024 * 2 + 6
#define BUF_LEN 1024 + 6

#define EXPAND 0
#define UART1 1 
#define UART2 2
#define UART3 3

#define BUF_DATA_FLAG 1  // 缓冲区对数据的存储标志：当把数据拷贝到缓冲区下标'0'处是BUF_DATA_FLAG为一

#define MAX_LINK 30 // STUB 可同时接收30路发送命令，当大于等于30时，暂停接收

#define DELAY 1000

struct s_spi_rt_cmd
{
    char head[2];
    unsigned char cmd;
    unsigned char uart;
    unsigned short buf_len; // 串口数据长度
    unsigned char checkbit;
};

// class_spi_rt_interface结构体定义
struct class_spi_rt_interface
{
    int (* init_spi)();
    int (* spi_select)(int uart, struct timeval *tv); 
    int (* send_data)(unsigned char uart, char *data, unsigned short len);
    int (* send_repeat_cmd)();
    int (* recv_data)(unsigned char uart, char *data, unsigned short len);
    int (* spi_send)(struct s_spi_rt_cmd *spi_rt_cmd);
    int (* send_spi_rt_cmd_msg)(int fd, struct s_spi_rt_cmd *spi_rt_cmd, struct timeval *tv);
    int (* recv_spi_rt_cmd_msg)(int fd, struct s_spi_rt_cmd *spi_rt_cmd, struct timeval *tv);
    int (* read_spi_rt_para)(int flag);
    int (* write_spi_rt_para)(int flag);
    
    int spi_uart_sem_key;
    int spi_uart_sem_id;
    
    key_t spi_uart1_recv_shared_mem_key;
    key_t spi_uart2_recv_shared_mem_key;
    key_t spi_uart3_recv_shared_mem_key;
    
    key_t spi_expand_recv_shared_mem_key;
    
    key_t spi_uart1_send_shared_mem_key;
    key_t spi_uart2_send_shared_mem_key;
    key_t spi_uart3_send_shared_mem_key;
    
    key_t spi_expand_send_shared_mem_key;
    
    int spi_uart1_recv_shared_mem_id;
    int spi_uart2_recv_shared_mem_id;
    int spi_uart3_recv_shared_mem_id;
    
    int spi_expand_recv_shared_mem_id;
    
    int spi_uart1_send_shared_mem_id;
    int spi_uart2_send_shared_mem_id;
    int spi_uart3_send_shared_mem_id;
    
    int spi_expand_send_shared_mem_id;
    
    // 共享内存
    void *spi_uart1_recv_shared_mem_buf;
    void *spi_uart2_recv_shared_mem_buf;
    void *spi_uart3_recv_shared_mem_buf;
    
    void *spi_expand_recv_shared_mem_buf;
        
    void *spi_uart1_send_shared_mem_buf;
    void *spi_uart2_send_shared_mem_buf;
    void *spi_uart3_send_shared_mem_buf;
    
    void *spi_expand_send_shared_mem_buf;
    
    // 接收共享内存区的写入和读出 索引值
    int spi_uart1_recv_write_index;
    int spi_uart2_recv_write_index;
    int spi_uart3_recv_write_index;
    
    int spi_expand_recv_write_index;
    
    int spi_uart1_recv_read_index;
    int spi_uart2_recv_read_index;
    int spi_uart3_recv_read_index;
    
    int spi_expand_recv_read_index;
    
    unsigned int uart1_recv_count;
    unsigned int uart2_recv_count;
    unsigned int uart3_recv_count;
    
    unsigned int expand_recv_count;
    
    unsigned int uart1_send_buf_len;
    unsigned int uart2_send_buf_len;
    unsigned int uart3_send_buf_len;
    
    unsigned int expand_send_buf_len;
    
    int spi_fd;
    int spi_server_fd;
};

extern struct class_spi_rt_interface spi_rt_interface;
#endif
