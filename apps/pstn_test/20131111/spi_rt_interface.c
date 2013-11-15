#include "spi_rt_interface.h"


/**
 * 初始化spi
 */
static int init_spi();

/**
 * spi轮训
 */
static int spi_select(int uart, struct timeval *tv); 

/**
 * 读取spi收发参数
 */
static int read_spi_rt_para(int flag);

/**
 * 写入spi收发参数
 */
static int write_spi_rt_para(int flag);

/**
 * spi 发送数据(应用程序接口)
 */
static int send_data(unsigned char uart, char *data, unsigned short len);

/**
 * spi 发送重发命令
 */
static int send_repeat_cmd();

/**
 * spi 接收数据(应用程序接口)
 */
static int recv_data(unsigned char uart, char *data, unsigned short len);

/**
 * spi 发送数据(STUB应用程序调用接口)
 */
static int spi_send(struct s_spi_rt_cmd *spi_rt_cmd);

/**
 * 发送spi收发命令包
 */
static int send_spi_rt_cmd_msg(int fd, struct s_spi_rt_cmd *spi_rt_cmd, struct timeval *tv);

/**
 * 接收spi收发命令包
 */
static int recv_spi_rt_cmd_msg(int fd, struct s_spi_rt_cmd *spi_rt_cmd, struct timeval *tv);


struct class_spi_rt_interface spi_rt_interface = 
{
    .init_spi = init_spi,
    .spi_select = spi_select,
    .send_data = send_data,
    .send_repeat_cmd = send_repeat_cmd,
    .recv_data = recv_data,
    .spi_send = spi_send,
    .send_spi_rt_cmd_msg = send_spi_rt_cmd_msg,
    .recv_spi_rt_cmd_msg = recv_spi_rt_cmd_msg,
    .read_spi_rt_para = read_spi_rt_para,
    .write_spi_rt_para = write_spi_rt_para,
    
    .spi_uart_sem_key = 0x55, 
    
    .spi_uart1_recv_shared_mem_key = 0x10,
    .spi_uart2_recv_shared_mem_key = 0x20,
    .spi_uart3_recv_shared_mem_key = 0x30,
    
    .spi_expand_recv_shared_mem_key = 0x40,
    
    .spi_uart1_send_shared_mem_key = 0x11,
    .spi_uart2_send_shared_mem_key = 0x21,
    .spi_uart3_send_shared_mem_key = 0x31,
    
    .spi_expand_send_shared_mem_key = 0x41,
    
    .spi_uart1_recv_shared_mem_id = 0,
    .spi_uart2_recv_shared_mem_id = 0,
    .spi_uart3_recv_shared_mem_id = 0,
    
    .spi_expand_recv_shared_mem_id = 0,
    
    .spi_uart1_send_shared_mem_id = 0,
    .spi_uart2_send_shared_mem_id = 0,
    .spi_uart3_send_shared_mem_id = 0,
    
    .spi_expand_send_shared_mem_id = 0,
    
    .spi_uart1_recv_write_index = 0,
    .spi_uart2_recv_write_index = 0,
    .spi_uart3_recv_write_index = 0,
    
    .spi_expand_recv_write_index = 0,
    
    .spi_uart1_recv_read_index = 0,
    .spi_uart2_recv_read_index = 0,
    .spi_uart3_recv_read_index = 0,
    
    .spi_expand_recv_read_index = 0,
    
    .spi_fd = 0,
    .spi_server_fd = 0,
};


/**
 * 初始化spi
 */
int init_spi()
{
    int res = 0;
    int release_flag = 0;
    
    PRINT("spi_rt_interface.recv_data = %X\n", spi_rt_interface.recv_data);
    PRINT("spi_rt_interface.send_data = %X\n", spi_rt_interface.send_data);
    
    // 打开 /dev/spiS0
    if ((spi_rt_interface.spi_fd = open(SPI_NAME, O_RDWR)) < 0)
    {
        PERROR("open /dev/spiS0 failed!\n");
        return OPEN_ERR;
    }
    PRINT("spi_rt_interface.spi_fd = %d\n", spi_rt_interface.spi_fd);
    
    // 创建共享内存 （8块）
    // 串口一接收缓冲区
    if ((spi_rt_interface.spi_uart1_recv_shared_mem_id = shmget(spi_rt_interface.spi_uart1_recv_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
    {
        PERROR("shmget failed!\n");
        close(spi_rt_interface.spi_fd);
        return SHMGET_ERR;
    }
    
    if ((spi_rt_interface.spi_uart1_recv_shared_mem_buf = shmat(spi_rt_interface.spi_uart1_recv_shared_mem_id, 0, 0)) < 0)
    {
        PERROR("shmat failed!\n");
        close(spi_rt_interface.spi_fd);
        return SHMAT_ERR;
    }
    PRINT("spi_rt_interface.spi_uart1_recv_shared_mem_buf = %X\n", spi_rt_interface.spi_uart1_recv_shared_mem_buf);
    release_flag = 1;
    
    // 串口二接收缓冲区
    if ((spi_rt_interface.spi_uart2_recv_shared_mem_id = shmget(spi_rt_interface.spi_uart2_recv_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
    {
        PERROR("shmget failed!\n");
        res = SHMGET_ERR;
        goto EXIT;
    }
    
    if ((spi_rt_interface.spi_uart2_recv_shared_mem_buf = shmat(spi_rt_interface.spi_uart2_recv_shared_mem_id, 0, 0)) < 0)
    {
        PERROR("shmat failed!\n");
        res = SHMAT_ERR;
        goto EXIT;
    }
    release_flag = 2;
    PRINT("spi_rt_interface.spi_uart2_recv_shared_mem_buf = %X\n", spi_rt_interface.spi_uart2_recv_shared_mem_buf);
    
    // 串口三接收缓冲区
    if ((spi_rt_interface.spi_uart3_recv_shared_mem_id = shmget(spi_rt_interface.spi_uart3_recv_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
    {
        PERROR("shmget failed!\n");
        res = SHMGET_ERR;
        goto EXIT;
    }
    
    if ((spi_rt_interface.spi_uart3_recv_shared_mem_buf = shmat(spi_rt_interface.spi_uart3_recv_shared_mem_id, 0, 0)) < 0)
    {
        PERROR("shmat failed!\n");
        res = SHMAT_ERR;
        goto EXIT;
    }
    release_flag = 3;
    PRINT("spi_rt_interface.spi_uart3_recv_shared_mem_buf = %X\n", spi_rt_interface.spi_uart3_recv_shared_mem_buf);
    
    // 扩展功能接收缓冲区
    if ((spi_rt_interface.spi_expand_recv_shared_mem_id = shmget(spi_rt_interface.spi_expand_recv_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
    {
        PERROR("shmget failed!\n");
        res = SHMGET_ERR;
        goto EXIT;
    }
    
    if ((spi_rt_interface.spi_expand_recv_shared_mem_buf = shmat(spi_rt_interface.spi_expand_recv_shared_mem_id, 0, 0)) < 0)
    {
        PERROR("shmat failed!\n");
        res = SHMAT_ERR;
        goto EXIT;
    }
    release_flag = 4;
    PRINT("spi_rt_interface.spi_expand_recv_shared_mem_buf = %X\n", spi_rt_interface.spi_expand_recv_shared_mem_buf);
    
    // 串口一发送缓冲区
    if ((spi_rt_interface.spi_uart1_send_shared_mem_id = shmget(spi_rt_interface.spi_uart1_send_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
    {
        PERROR("shmget failed!\n");
        close(spi_rt_interface.spi_fd);
        return SHMGET_ERR;
    }
    
    if ((spi_rt_interface.spi_uart1_send_shared_mem_buf = shmat(spi_rt_interface.spi_uart1_send_shared_mem_id, 0, 0)) < 0)
    {
        PERROR("shmat failed!\n");
        close(spi_rt_interface.spi_fd);
        return SHMAT_ERR;
    }
    PRINT("spi_rt_interface.spi_uart1_send_shared_mem_buf = %X\n", spi_rt_interface.spi_uart1_send_shared_mem_buf);
    release_flag = 5;
    
    // 串口二发送缓冲区
    if ((spi_rt_interface.spi_uart2_send_shared_mem_id = shmget(spi_rt_interface.spi_uart2_send_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
    {
        PERROR("shmget failed!\n");
        res = SHMGET_ERR;
        goto EXIT;
    }
    
    if ((spi_rt_interface.spi_uart2_send_shared_mem_buf = shmat(spi_rt_interface.spi_uart2_send_shared_mem_id, 0, 0)) < 0)
    {
        PERROR("shmat failed!\n");
        res = SHMAT_ERR;
        goto EXIT;
    }
    release_flag = 6;
    PRINT("spi_rt_interface.spi_uart2_send_shared_mem_buf = %X\n", spi_rt_interface.spi_uart2_send_shared_mem_buf);
    
    // 串口三发送缓冲区
    if ((spi_rt_interface.spi_uart3_send_shared_mem_id = shmget(spi_rt_interface.spi_uart3_send_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
    {
        PERROR("shmget failed!\n");
        res = SHMGET_ERR;
        goto EXIT;
    }
    
    if ((spi_rt_interface.spi_uart3_send_shared_mem_buf = shmat(spi_rt_interface.spi_uart3_send_shared_mem_id, 0, 0)) < 0)
    {
        PERROR("shmat failed!\n");
        res = SHMAT_ERR;
        goto EXIT;
    }
    release_flag = 7;
    PRINT("spi_rt_interface.spi_uart3_send_shared_mem_buf = %X\n", spi_rt_interface.spi_uart3_send_shared_mem_buf);
    
    // 扩展功能发送缓冲区
    if ((spi_rt_interface.spi_expand_send_shared_mem_id = shmget(spi_rt_interface.spi_expand_send_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
    {
        PERROR("shmget failed!\n");
        res = SHMGET_ERR;
        goto EXIT;
    }
    
    if ((spi_rt_interface.spi_expand_send_shared_mem_buf = shmat(spi_rt_interface.spi_expand_send_shared_mem_id, 0, 0)) < 0)
    {
        PERROR("shmat failed!\n");
        res = SHMAT_ERR;
        goto EXIT;
    }
    release_flag = 8;
    PRINT("spi_rt_interface.spi_expand_send_shared_mem_buf = %X\n", spi_rt_interface.spi_expand_send_shared_mem_buf);
    
    // 创建信号量集，8个信号量，信号量的值为一
    // （0,1,2,3）四个为发送信号量，（4,5,6,7）四个位接收信号量
    if ((spi_rt_interface.spi_uart_sem_id = semget(spi_rt_interface.spi_uart_sem_key, 8, IPC_CREAT | 0666)) < 0)
    {
        PERROR("semget failed!\n");
        res = SEMGET_ERR;
        goto EXIT;
    }
    
    // 对8个信号量进行初始化，值为1
    if (semctl(spi_rt_interface.spi_uart_sem_id, 0, SETVAL, 1) < 0)
    {
        PERROR("semctl failed!\n");
        res = SEMCTL_ERR;   
        goto EXIT;
    }
    
    if (semctl(spi_rt_interface.spi_uart_sem_id, 1, SETVAL, 1) < 0)
    {
        PERROR("semctl failed!\n");
        res = SEMCTL_ERR;   
        goto EXIT;
    }
    
    if (semctl(spi_rt_interface.spi_uart_sem_id, 2, SETVAL, 1) < 0)
    {
        PERROR("semctl failed!\n");
        res = SEMCTL_ERR;   
        goto EXIT;
    }
    
    if (semctl(spi_rt_interface.spi_uart_sem_id, 3, SETVAL, 1) < 0)
    {
        PERROR("semctl failed!\n");
        res = SEMCTL_ERR;   
        goto EXIT;
    }
    
    if (semctl(spi_rt_interface.spi_uart_sem_id, 4, SETVAL, 1) < 0)
    {
        PERROR("semctl failed!\n");
        res = SEMCTL_ERR;   
        goto EXIT;
    }
    
    if (semctl(spi_rt_interface.spi_uart_sem_id, 5, SETVAL, 1) < 0)
    {
        PERROR("semctl failed!\n");
        res = SEMCTL_ERR;   
        goto EXIT;
    }
    
    if (semctl(spi_rt_interface.spi_uart_sem_id, 6, SETVAL, 1) < 0)
    {
        PERROR("semctl failed!\n");
        res = SEMCTL_ERR;   
        goto EXIT;
    }
    
    if (semctl(spi_rt_interface.spi_uart_sem_id, 7, SETVAL, 1) < 0)
    {
        PERROR("semctl failed!\n");
        res = SEMCTL_ERR;   
        goto EXIT;
    }
    
    release_flag = 9;
    
    // 初始化收发缓冲区长度
    if ((res = write_spi_rt_para(0)) < 0)
    {
        PERROR("write_spi_rt_para failed!\n");
        goto EXIT;
    }
    if ((res = write_spi_rt_para(1)) < 0)
    {
        PERROR("write_spi_rt_para failed!\n");
        goto EXIT;
    }
    if ((res = write_spi_rt_para(2)) < 0)
    {
        PERROR("write_spi_rt_para failed!\n");
        goto EXIT;
    }
    if ((res = write_spi_rt_para(3)) < 0)
    {
        PERROR("write_spi_rt_para failed!\n");
        goto EXIT;
    }
    if ((res = write_spi_rt_para(4)) < 0)
    {
        PERROR("write_spi_rt_para failed!\n");
        goto EXIT;
    }
    if ((res = write_spi_rt_para(5)) < 0)
    {
        PERROR("write_spi_rt_para failed!\n");
        goto EXIT;
    }
    if ((res = write_spi_rt_para(6)) < 0)
    {
        PERROR("write_spi_rt_para failed!\n");
        goto EXIT;
    }
    if ((res = write_spi_rt_para(7)) < 0)
    {
        PERROR("write_spi_rt_para failed!\n");
        goto EXIT;
    }
    
    // 建立本地服务器
    #if LOCAL_SOCKET
    if ((spi_rt_interface.spi_server_fd = internetwork_communication.make_local_socket_server_link(SPI_LOCAL_SOCKET_NAME)) < 0)
    {
        PERROR("make_local_socket_server_link failed!\n");         
        res = spi_rt_interface.spi_server_fd;
        goto EXIT;
    }
    PRINT("The server is established successfully! SPI_LOCAL_SOCKET_NAME = %s\n", SPI_LOCAL_SOCKET_NAME);    
    #else
    if ((spi_rt_interface.spi_server_fd = internetwork_communication.make_server_link(SPI_SERVER_PORT)) < 0)
    {
        PERROR("make_server_link failed!\n");         
        res = spi_rt_interface.spi_server_fd;
        goto EXIT;
    }
    PRINT("The server is established successfully! SPI_SERVER_PORT = %s\n", SPI_SERVER_PORT);    
    #endif
    
    return 0;
    
EXIT:
    // 资源释放
    if (spi_rt_interface.spi_fd != 0)
    {
        close(spi_rt_interface.spi_fd);
        spi_rt_interface.spi_fd = 0;
    }
    
    switch (release_flag)
    {
        case 9:
        {
            if (semctl(spi_rt_interface.spi_uart_sem_id, IPC_RMID, 0) < 0)
            {
                PERROR("semop failed!\n");
                res = SEMCTL_ERR;
            }
        }
        case 8:
        {
            if (shmctl(spi_rt_interface.spi_expand_send_shared_mem_id, IPC_RMID, NULL) < 0)
            {
                PERROR("shmctl failed!\n");
                res = SHMCTL_ERR;
            }
        }
        case 7:
        {
            if (shmctl(spi_rt_interface.spi_uart3_send_shared_mem_id, IPC_RMID, NULL) < 0)
            {
                PERROR("shmctl failed!\n");
                res = SHMCTL_ERR;
            }
        }
        case 6:
        {
            if (shmctl(spi_rt_interface.spi_uart2_send_shared_mem_id, IPC_RMID, NULL) < 0)
            {
                PERROR("shmctl failed!\n");
                res = SHMCTL_ERR;
            }
        }
        case 5:
        {
            if (shmctl(spi_rt_interface.spi_uart1_send_shared_mem_id, IPC_RMID, NULL) < 0)
            {
                PERROR("shmctl failed!\n");
                res = SHMCTL_ERR;
            }
        }
        case 4:
        {
            if (shmctl(spi_rt_interface.spi_expand_recv_shared_mem_id, IPC_RMID, NULL) < 0)
            {
                PERROR("shmctl failed!\n");
                res = SHMCTL_ERR;
            }
        }
        case 3:
        {
            if (shmctl(spi_rt_interface.spi_uart3_recv_shared_mem_id, IPC_RMID, NULL) < 0)
            {
                PERROR("shmctl failed!\n");
                res = SHMCTL_ERR;
            }
        }
        case 2:
        {
            if (shmctl(spi_rt_interface.spi_uart2_recv_shared_mem_id, IPC_RMID, NULL) < 0)
            {
                PERROR("shmctl failed!\n");
                res = SHMCTL_ERR;
            }
        }
        case 1:
        {
            if (shmctl(spi_rt_interface.spi_uart1_recv_shared_mem_id, IPC_RMID, NULL) < 0)
            {
                PERROR("shmctl failed!\n");
                res = SHMCTL_ERR;
            }
        }
    }
    return res;
}

/**
 * spi轮训
 */
int spi_select(int uart, struct timeval *tv)
{
    int i = 0;
    int j = 0;
    int res = 0;
    struct sembuf semopbuf;      // 信号量结构
    unsigned char sem_num = 0;   // 信号量序号
    
    // 获取8个信号
    if ((spi_rt_interface.spi_uart_sem_id = semget(spi_rt_interface.spi_uart_sem_key, 8, IPC_CREAT | 0666)) < 0)
    {
        PERROR("semget failed!\n");
        return SEMGET_ERR;
    }
    
    // 查询的次数
    j = tv->tv_sec * 250 + tv->tv_usec / 2000;
    
    semopbuf.sem_flg = SEM_UNDO;
    switch (uart)
    {
        case UART1:
        {
            semopbuf.sem_num = 4;
            break;
        }
        case UART2:
        {
            semopbuf.sem_num = 5;
            break;
        }
        case UART3:
        {
            semopbuf.sem_num = 6;
            break;
        }
        case EXPAND:
        {
            semopbuf.sem_num = 7;
            break;
        }
        default:
        {
            PERROR("option does not mismatch!\n");
            return MISMATCH_ERR;
        }
    }
        
    do
    {
        semopbuf.sem_op = -1;
        if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
        {
            PERROR("semop failed!\n");
            return SEMOP_ERR;
        }
        
        if ((res = read_spi_rt_para(semopbuf.sem_num)) < 0)
        {
            PERROR("read_spi_rt_para failed!\n");
        }
        
        semopbuf.sem_op = 1;
        if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
        {
            PERROR("semop failed!\n");
            return SEMOP_ERR;
        }
        if (res < 0)
        {
            return res;
        }
        
        switch (uart)
        {
            case UART1:
            {                
                if (spi_rt_interface.uart1_recv_count > 0)
                {
                    //PRINT("spi_rt_interface.uart1_recv_count = %d\n", spi_rt_interface.uart1_recv_count);
                    return 0;
                }
                break;
            }
            case UART2:
            {
                if (spi_rt_interface.uart2_recv_count > 0)
                {
                    PRINT("spi_rt_interface.uart2_recv_count = %d\n", spi_rt_interface.uart2_recv_count);
                    return 0;
                }
                break;
            }
            case UART3:
            {
                if (spi_rt_interface.uart3_recv_count > 0)
                {
                    PRINT("spi_rt_interface.uart3_recv_count = %d\n", spi_rt_interface.uart3_recv_count);
                    return 0;
                }
                break;
            }
            case EXPAND:
            {
                if (spi_rt_interface.expand_recv_count > 0)
                {
                    PRINT("spi_rt_interface.expand_recv_count = %d\n", spi_rt_interface.expand_recv_count);
                    return 0;
                }
                break;
            }
        }
        i++;
        usleep(2000);
    }
    while (i < j);
    return SELECT_NULL_ERR;
}

#if BUF_DATA_FLAG 
/**
 * 读取spi收发参数
 */
int read_spi_rt_para(int flag)
{
    int i = 0;
    int res = 0;
    char buf[64] = {0};
    char file_name[64] = {0};
    char *tmp = NULL;
    
    sprintf(file_name, "%s%d", SPI_PARA_FILE, flag);
    //PRINT("file_name = %s\n", file_name);
    FILE *fp = fopen(file_name, "r");
    
    if (fp == NULL)
    {
        PRINT("fopen failed!\n");
        return OPEN_ERR;
    }
    
    fgets(buf, sizeof(buf), fp);
    buf[strlen(buf) - 1] = '\0';
    switch (flag)
    {
        case 4:
        {
            if ((tmp = strstr(buf, "uart1_recv_count=")) != NULL)
            {
                spi_rt_interface.uart1_recv_count = atoi(tmp + strlen("uart1_recv_count="));
                break;
            }
            res = DATA_ERR;
            break;
        }
        case 5:
        {
            if ((tmp = strstr(buf, "uart2_recv_count=")) != NULL)
            {
                spi_rt_interface.uart2_recv_count = atoi(tmp + strlen("uart2_recv_count="));
                break;
            }
            res = DATA_ERR;
            break;
        }
        case 6:
        {
            if ((tmp = strstr(buf, "uart3_recv_count=")) != NULL)
            {
                spi_rt_interface.uart3_recv_count = atoi(tmp + strlen("uart3_recv_count="));
                break;
            }
            res = DATA_ERR;
            break;
        }
        case 7:
        {
            if ((tmp = strstr(buf, "expand_recv_count=")) != NULL)
            {
                spi_rt_interface.expand_recv_count = atoi(tmp + strlen("expand_recv_count="));
                break;
            }
            res = DATA_ERR;
            break;
        }
        case 0:
        {
            if ((tmp = strstr(buf, "uart1_send_buf_len=")) != NULL)
            {
                spi_rt_interface.uart1_send_buf_len = atoi(tmp + strlen("uart1_send_buf_len="));
                break;
            }
            res = DATA_ERR;
            break;
        }
        case 1:
        {
            if ((tmp = strstr(buf, "uart2_send_buf_len=")) != NULL)
            {
                spi_rt_interface.uart2_send_buf_len = atoi(tmp + strlen("uart2_send_buf_len="));
                break;
            }
            res = DATA_ERR;
            break;
        }
        case 2:
        {
            if ((tmp = strstr(buf, "uart3_send_buf_len=")) != NULL)
            {
                spi_rt_interface.uart3_send_buf_len = atoi(tmp + strlen("uart3_send_buf_len="));
                break;
            }
            res = DATA_ERR;
            break;
        }
        case 3:
        {
            if ((tmp = strstr(buf, "expand_send_buf_len=")) != NULL)
            {
                spi_rt_interface.expand_send_buf_len = atoi(tmp + strlen("expand_send_buf_len="));
                break;
            }
            res = DATA_ERR;
            break;
        }
        default:
        {
            PRINT("option does not mismatch!\n");
            res = MISMATCH_ERR;
            break;
        }   
    }
    fclose(fp);
    return res;
}

/**
 * 写入spi收发参数
 */
int write_spi_rt_para(int flag)
{
    // 写入文件
    int fd = 0;
    int res = 0;
    char buf[64] = {0};
    char file_name[64] = {0};
    
    sprintf(file_name, "%s%d", SPI_PARA_FILE, flag);
    
    if ((fd = open(file_name, O_CREAT | O_TRUNC | O_WRONLY, 0666)) < 0)
    {
        PERROR("open failed!\n");
        return OPEN_ERR;
    }
    switch (flag)
    {
        case 4:
        {
            sprintf(buf, "uart1_recv_count=%d\n", spi_rt_interface.uart1_recv_count);
            break;
        }
        case 5:
        {
            sprintf(buf, "uart2_recv_count=%d\n", spi_rt_interface.uart2_recv_count);
            break;
        }
        case 6:
        {
            sprintf(buf, "uart3_recv_count=%d\n", spi_rt_interface.uart3_recv_count);
            break;
        }
        case 7:
        {
            sprintf(buf, "expand_recv_count=%d\n", spi_rt_interface.expand_recv_count);
            break;
        }
        case 0:
        {
            sprintf(buf, "uart1_send_buf_len=%d\n", spi_rt_interface.uart1_send_buf_len);
            break;
        }
        case 1:
        {
            sprintf(buf, "uart2_send_buf_len=%d\n", spi_rt_interface.uart2_send_buf_len);
            break;
        }
        case 2:
        {
            sprintf(buf, "uart3_send_buf_len=%d\n", spi_rt_interface.uart3_send_buf_len);
            break;
        }
        case 3:
        {
            sprintf(buf, "expand_send_buf_len=%d\n", spi_rt_interface.expand_send_buf_len);
            break;
        }
        default:
        {
            PRINT("option does not mismatch!\n");
            res = MISMATCH_ERR;
            break;
        }   
    }
    if (write(fd, buf, strlen(buf)) < 0)
    {
        close(fd);
        PERROR("write failed!\n");
        return WRITE_ERR;
    }
    close(fd);
    return 0;
}

#else
/**
 * 读取spi收发参数
 */
int read_spi_rt_para(int flag)
{
    int i = 0;
    int res = 0;
    char buf[64] = {0};
    char file_name[64] = {0};
    char *tmp = NULL;
    
    sprintf(file_name, "%s%d", SPI_PARA_FILE, flag);
    FILE *fp = fopen(file_name, "r");
    
    if (fp == NULL)
    {
        PRINT("fopen failed!\n");
        return OPEN_ERR;
    }
    
    fgets(buf, sizeof(buf), fp);
    buf[strlen(buf) - 1] = '\0';
    switch (flag)
    {
        case 4:
        {
            // 接收数据长度
            if ((tmp = strstr(buf, "uart1_recv_count=")) != NULL)
            {
                spi_rt_interface.uart1_recv_count = atoi(tmp + strlen("uart1_recv_count="));
            }
            else
            {
                res = DATA_ERR;
                break;
            }
            
            // 写索引
            memset(buf, 0, sizeof(buf));
            fgets(buf, sizeof(buf), fp);
            buf[strlen(buf) - 1] = '\0';
            if ((tmp = strstr(buf, "spi_uart1_recv_write_index=")) != NULL)
            {
                spi_rt_interface.spi_uart1_recv_write_index = atoi(tmp + strlen("spi_uart1_recv_write_index="));
            }
            else
            {
                res = DATA_ERR;
                break;
            }
            
            // 读索引
            memset(buf, 0, sizeof(buf));
            fgets(buf, sizeof(buf), fp);
            buf[strlen(buf) - 1] = '\0';
            if ((tmp = strstr(buf, "spi_uart1_recv_read_index=")) != NULL)
            {
                spi_rt_interface.spi_uart1_recv_read_index = atoi(tmp + strlen("spi_uart1_recv_read_index="));
            }
            else
            {
                res = DATA_ERR;
                break;
            }
            break;
        }
        case 5:
        {
            // 接收数据长度
            if ((tmp = strstr(buf, "uart2_recv_count=")) != NULL)
            {
                spi_rt_interface.uart2_recv_count = atoi(tmp + strlen("uart2_recv_count="));
            }
            else
            {
                res = DATA_ERR;
                break;
            }
            
            // 写索引
            memset(buf, 0, sizeof(buf));
            fgets(buf, sizeof(buf), fp);
            buf[strlen(buf) - 1] = '\0';
            if ((tmp = strstr(buf, "spi_uart2_recv_write_index=")) != NULL)
            {
                spi_rt_interface.spi_uart2_recv_write_index = atoi(tmp + strlen("spi_uart2_recv_write_index="));
            }
            else
            {
                res = DATA_ERR;
                break;
            }
            
            // 读索引
            memset(buf, 0, sizeof(buf));
            fgets(buf, sizeof(buf), fp);
            buf[strlen(buf) - 1] = '\0';
            if ((tmp = strstr(buf, "spi_uart2_recv_read_index=")) != NULL)
            {
                spi_rt_interface.spi_uart2_recv_read_index = atoi(tmp + strlen("spi_uart2_recv_read_index="));
            }
            else
            {
                res = DATA_ERR;
                break;
            }
            break;
        }
        case 6:
        {
            // 接收数据长度
            if ((tmp = strstr(buf, "uart3_recv_count=")) != NULL)
            {
                spi_rt_interface.uart3_recv_count = atoi(tmp + strlen("uart3_recv_count="));
            }
            else
            {
                res = DATA_ERR;
                break;
            }
            
            // 写索引
            memset(buf, 0, sizeof(buf));
            fgets(buf, sizeof(buf), fp);
            buf[strlen(buf) - 1] = '\0';
            if ((tmp = strstr(buf, "spi_uart3_recv_write_index=")) != NULL)
            {
                spi_rt_interface.spi_uart3_recv_write_index = atoi(tmp + strlen("spi_uart3_recv_write_index="));
            }
            else
            {
                res = DATA_ERR;
                break;
            }
            
            // 读索引
            memset(buf, 0, sizeof(buf));
            fgets(buf, sizeof(buf), fp);
            buf[strlen(buf) - 1] = '\0';
            if ((tmp = strstr(buf, "spi_uart3_recv_read_index=")) != NULL)
            {
                spi_rt_interface.spi_uart3_recv_read_index = atoi(tmp + strlen("spi_uart3_recv_read_index="));
            }
            else
            {
                res = DATA_ERR;
                break;
            }
            break;
        }
        case 7:
        {
            // 接收数据长度
            if ((tmp = strstr(buf, "expand_recv_count=")) != NULL)
            {
                spi_rt_interface.expand_recv_count = atoi(tmp + strlen("expand_recv_count="));
            }
            else
            {
                res = DATA_ERR;
                break;
            }
            
            // 写索引
            memset(buf, 0, sizeof(buf));
            fgets(buf, sizeof(buf), fp);
            buf[strlen(buf) - 1] = '\0';
            if ((tmp = strstr(buf, "spi_expand_recv_write_index=")) != NULL)
            {
                spi_rt_interface.spi_expand_recv_write_index = atoi(tmp + strlen("spi_expand_recv_write_index="));
            }
            else
            {
                res = DATA_ERR;
                break;
            }
            
            // 读索引
            memset(buf, 0, sizeof(buf));
            fgets(buf, sizeof(buf), fp);
            buf[strlen(buf) - 1] = '\0';
            if ((tmp = strstr(buf, "spi_expand_recv_read_index=")) != NULL)
            {
                spi_rt_interface.spi_expand_recv_read_index = atoi(tmp + strlen("spi_expand_recv_read_index="));
            }
            else
            {
                res = DATA_ERR;
                break;
            }
            break;
        }
        case 0:
        {
            if ((tmp = strstr(buf, "uart1_send_buf_len=")) != NULL)
            {
                spi_rt_interface.uart1_send_buf_len = atoi(tmp + strlen("uart1_send_buf_len="));
                break;
            }
            res = DATA_ERR;
            break;
        }
        case 1:
        {
            if ((tmp = strstr(buf, "uart2_send_buf_len=")) != NULL)
            {
                spi_rt_interface.uart2_send_buf_len = atoi(tmp + strlen("uart2_send_buf_len="));
                break;
            }
            res = DATA_ERR;
            break;
        }
        case 2:
        {
            if ((tmp = strstr(buf, "uart3_send_buf_len=")) != NULL)
            {
                spi_rt_interface.uart3_send_buf_len = atoi(tmp + strlen("uart3_send_buf_len="));
                break;
            }
            res = DATA_ERR;
            break;
        }
        case 3:
        {
            if ((tmp = strstr(buf, "expand_send_buf_len=")) != NULL)
            {
                spi_rt_interface.expand_send_buf_len = atoi(tmp + strlen("expand_send_buf_len="));
                break;
            }
            res = DATA_ERR;
            break;
        }
        default:
        {
            PRINT("option does not mismatch!\n");
            res = MISMATCH_ERR;
            break;
        }   
    }
    fclose(fp);
    return res;
}

/**
 * 写入spi收发参数
 */
int write_spi_rt_para(int flag)
{
    // 写入文件
    int fd = 0;
    int res = 0;
    char buf[64] = {0};
    char file_name[64] = {0};
    
    sprintf(file_name, "%s%d", SPI_PARA_FILE, flag);
    
    if ((fd = open(file_name, O_CREAT | O_TRUNC | O_WRONLY, 0666)) < 0)
    {
        PERROR("open failed!\n");
        return OPEN_ERR;
    }
    switch (flag)
    {
        case 4:
        {
            sprintf(buf, "uart1_recv_count=%d\nspi_uart1_recv_write_index=%d\nspi_uart1_recv_read_index=%d\n", 
                spi_rt_interface.uart1_recv_count, spi_rt_interface.spi_uart1_recv_write_index, spi_rt_interface.spi_uart1_recv_read_index);
            break;
        }
        case 5:
        {
            sprintf(buf, "uart2_recv_count=%d\nspi_uart2_recv_write_index=%d\nspi_uart2_recv_read_index=%d\n", 
                spi_rt_interface.uart2_recv_count, spi_rt_interface.spi_uart2_recv_write_index, spi_rt_interface.spi_uart2_recv_read_index);
            PRINT("buf = %s", buf);
            break;
        }
        case 6:
        {
            sprintf(buf, "uart3_recv_count=%d\nspi_uart3_recv_write_index=%d\nspi_uart3_recv_read_index=%d\n", 
                spi_rt_interface.uart3_recv_count, spi_rt_interface.spi_uart3_recv_write_index, spi_rt_interface.spi_uart3_recv_read_index);
            break;
        }
        case 7:
        {
            sprintf(buf, "expand_recv_count=%d\nspi_expand_recv_write_index=%d\nspi_expand_recv_read_index=%d\n", 
                spi_rt_interface.expand_recv_count, spi_rt_interface.spi_expand_recv_write_index, spi_rt_interface.spi_expand_recv_read_index);
            break;
        }
        case 0:
        {
            sprintf(buf, "uart1_send_buf_len=%d\n", spi_rt_interface.uart1_send_buf_len);
            break;
        }
        case 1:
        {
            sprintf(buf, "uart2_send_buf_len=%d\n", spi_rt_interface.uart2_send_buf_len);
            break;
        }
        case 2:
        {
            sprintf(buf, "uart3_send_buf_len=%d\n", spi_rt_interface.uart3_send_buf_len);
            break;
        }
        case 3:
        {
            sprintf(buf, "expand_send_buf_len=%d\n", spi_rt_interface.expand_send_buf_len);
            break;
        }
        default:
        {
            PRINT("option does not mismatch!\n");
            res = MISMATCH_ERR;
            break;
        }   
    }
    if (write(fd, buf, strlen(buf)) < 0)
    {
        close(fd);
        PERROR("write failed!\n");
        return WRITE_ERR;
    }
    close(fd);
    return 0;
}
#endif

#if 0
/**
 * spi 发送数据
 */
int send_data(unsigned char uart, char *data, unsigned short len)
{
    //PRINT("[uart = %d, len = %d]\n", uart, len);
    int i = 0, j = 0;
    int res = 0, ret = 0;
    int spi_client_fd = 0;       // 客户端套接字
	char uart_no = 0;            // 串口数据标识
	int release_flag = 0;        // 资源释放标识
	unsigned char sem_num = 0;   // 信号量序号
    struct sembuf semopbuf;      // 信号量结构
    
    struct timeval tv = {60, 0};
    struct s_spi_rt_cmd spi_rt_cmd; // spi接收发送命令结构
    memset(&spi_rt_cmd, 0, sizeof(struct s_spi_rt_cmd));
    
	if (data == NULL)
	{
	    PRINT("data is NULL!\n");
        return NULL_ERR;
	}
	if (len == 0)
	{
	    PRINT("len is zero!\n");
        return DATA_ERR;
	}
	PRINT_BUF_BY_HEX(data, NULL, len, __FILE__, __FUNCTION__, __LINE__);
	
	// 获取8个信号
    if ((spi_rt_interface.spi_uart_sem_id = semget(spi_rt_interface.spi_uart_sem_key, 8, IPC_CREAT | 0666)) < 0)
    {
        PERROR("semget failed!\n");
        return SEMGET_ERR;
    }

    semopbuf.sem_op = -1;
    semopbuf.sem_flg = SEM_UNDO;
    
    // 设置发送到STUB服务器的命令
    spi_rt_cmd.cmd = 'T';
    spi_rt_cmd.uart = uart;
    
	switch (uart)
    {
        case UART1:
        {
            // 信号量减一
            semopbuf.sem_num = 0;
            sem_num = 0;
            PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }

            // 把串口1发送缓冲区（共享内存区）添加到调用进程
            if ((spi_rt_interface.spi_uart1_send_shared_mem_id = shmget(spi_rt_interface.spi_uart1_send_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
            {
                PERROR("shmget failed!\n");
                res = SHMGET_ERR;
                break;
            }
            if ((spi_rt_interface.spi_uart1_send_shared_mem_buf = shmat(spi_rt_interface.spi_uart1_send_shared_mem_id, NULL, 0)) < 0)
            {
                PERROR("shmat failed!\n");
                res = SHMAT_ERR;
                break;
            }
            release_flag = 1;
            
            while (1)
            {
                if ((res = read_spi_rt_para(sem_num)) < 0)
                {
                    PERROR("read_spi_rt_para failed!\n");
                    break;
                }
                //PRINT("spi_rt_interface.uart1_send_buf_len = %d\n", spi_rt_interface.uart1_send_buf_len);
                if ((spi_rt_interface.uart1_send_buf_len == SHARED_MEM_SIZE) || ((spi_rt_interface.uart1_send_buf_len + len) > SHARED_MEM_SIZE))
                {
                    // 释放信号量
                    semopbuf.sem_op = 1;
                    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                    {
                        PERROR("semop failed!\n");
                        res = SEMOP_ERR;
                        break;
                    }
                    
                    usleep(DELAY);
                    
                    // 获取信号量
                    semopbuf.sem_op = -1;
                    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                    {
                        PERROR("semop failed!\n");
                        res = SEMOP_ERR;
                        break;
                    }
                    continue;
                }
                break;
            }
            
            if (res < 0)
            {
                break;
            }
            
            spi_rt_cmd.buf_len = len;
            // 把应用程序要发送的数据拷贝到共享内存区
            memcpy(spi_rt_interface.spi_uart1_send_shared_mem_buf + spi_rt_interface.uart1_send_buf_len, data, len);
            spi_rt_interface.uart1_send_buf_len += len;
            //PRINT("spi_rt_interface.uart1_send_buf_len = %d\n", spi_rt_interface.uart1_send_buf_len);
            break;
        }
        case UART2:
        {
            // 信号量减一
            semopbuf.sem_num = 1;
            sem_num = 1;
            //PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }
            
            if ((spi_rt_interface.spi_uart2_send_shared_mem_id = shmget(spi_rt_interface.spi_uart2_send_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
            {
                PERROR("shmget failed!\n");
                res = SHMGET_ERR;
                break;
            }
            
            if ((spi_rt_interface.spi_uart2_send_shared_mem_buf = shmat(spi_rt_interface.spi_uart2_send_shared_mem_id, NULL, 0)) < 0)
            {
                PERROR("shmat failed!\n");
                res = SHMAT_ERR;
                break;
            }
            release_flag = 2;
            
            while (1)
            {
                if ((res = read_spi_rt_para(sem_num)) < 0)
                {
                    PERROR("read_spi_rt_para failed!\n");
                    break;
                }
                //PRINT("spi_rt_interface.uart2_send_buf_len = %d\n", spi_rt_interface.uart2_send_buf_len);
                if ((spi_rt_interface.uart2_send_buf_len == SHARED_MEM_SIZE) || ((spi_rt_interface.uart2_send_buf_len + len) > SHARED_MEM_SIZE))
                {
                    // 释放信号量
                    semopbuf.sem_op = 1;
                    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                    {
                        PERROR("semop failed!\n");
                        res = SEMOP_ERR;
                        break;
                    }
                    
                    usleep(DELAY);
                    
                    // 获取信号量
                    semopbuf.sem_op = -1;
                    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                    {
                        PERROR("semop failed!\n");
                        res = SEMOP_ERR;
                        break;
                    }
                    continue;
                }
                break;
            }
            
            if (res < 0)
            {
                break;
            }
            
            spi_rt_cmd.buf_len = len;
            memcpy(spi_rt_interface.spi_uart2_send_shared_mem_buf + spi_rt_interface.uart2_send_buf_len, data, len);
            spi_rt_interface.uart2_send_buf_len += len;
            //PRINT("spi_rt_interface.uart2_send_buf_len = %d\n", spi_rt_interface.uart2_send_buf_len);
            break;
        }
        case UART3:
        {
            // 信号量减一
            semopbuf.sem_num = 2;
            sem_num = 2;
            //PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }
            
            if ((spi_rt_interface.spi_uart3_send_shared_mem_id = shmget(spi_rt_interface.spi_uart3_send_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
            {
                PERROR("shmget failed!");
                res = SHMGET_ERR;
                break;
            }
            
            if ((spi_rt_interface.spi_uart3_send_shared_mem_buf = shmat(spi_rt_interface.spi_uart3_send_shared_mem_id, NULL, 0)) < 0)
            {
                PERROR("shmat failed!\n");
                res = SHMAT_ERR;
                break;
            }
            release_flag = 3;
            
            while (1)
            {
                if ((res = read_spi_rt_para(sem_num)) < 0)
                {
                    PERROR("read_spi_rt_para failed!\n");
                    break;
                }
                //PRINT("spi_rt_interface.uart3_send_buf_len = %d\n", spi_rt_interface.uart3_send_buf_len);
                if ((spi_rt_interface.uart3_send_buf_len == SHARED_MEM_SIZE) || ((spi_rt_interface.uart3_send_buf_len + len) > SHARED_MEM_SIZE))
                {
                    // 释放信号量
                    semopbuf.sem_op = 1;
                    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                    {
                        PERROR("semop failed!\n");
                        res = SEMOP_ERR;
                        break;
                    }
                    
                    usleep(DELAY);
                    
                    // 获取信号量
                    semopbuf.sem_op = -1;
                    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                    {
                        PERROR("semop failed!\n");
                        res = SEMOP_ERR;
                        break;
                    }
                    continue;
                }
                break;
            }
            if (res < 0)
            {
                break;
            }
            
            spi_rt_cmd.buf_len = len;
            memcpy(spi_rt_interface.spi_uart3_recv_shared_mem_buf + spi_rt_interface.uart3_send_buf_len, data, len);
            spi_rt_interface.uart3_send_buf_len += len;
            //PRINT("spi_rt_interface.uart3_send_buf_len = %d\n", spi_rt_interface.uart3_send_buf_len);
            break;
        }
        case EXPAND:
        {
            // 信号量减一
            semopbuf.sem_num = 3;
            sem_num = 3;
            PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }

            // 把扩展功能发送缓冲区（共享内存区）添加到调用进程
            if ((spi_rt_interface.spi_expand_send_shared_mem_id = shmget(spi_rt_interface.spi_expand_send_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
            {
                PERROR("shmget failed!\n");
                res = SHMGET_ERR;
                break;
            }
            
            if ((spi_rt_interface.spi_expand_send_shared_mem_buf = shmat(spi_rt_interface.spi_expand_send_shared_mem_id, NULL, 0)) < 0)
            {
                PERROR("shmat failed!\n");
                res = SHMAT_ERR;
                break;
            }
            release_flag = 4;
            
            while (1)
            {
                if ((res = read_spi_rt_para(sem_num)) < 0)
                {
                    PERROR("read_spi_rt_para failed!\n");
                    break;
                }
                //PRINT("spi_rt_interface.expand_send_buf_len = %d\n", spi_rt_interface.expand_send_buf_len);
                if ((spi_rt_interface.expand_send_buf_len == SHARED_MEM_SIZE) || ((spi_rt_interface.expand_send_buf_len + len) > SHARED_MEM_SIZE))
                {
                    // 释放信号量
                    semopbuf.sem_op = 1;
                    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                    {
                        PERROR("semop failed!\n");
                        res = SEMOP_ERR;
                        break;
                    }
                    
                    usleep(DELAY);
                    
                    // 获取信号量
                    semopbuf.sem_op = -1;
                    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                    {
                        PERROR("semop failed!\n");
                        res = SEMOP_ERR;
                        break;
                    }
                    continue;
                }
                break;
            }
            
            if (res < 0)
            {
                break;
            }
            
            spi_rt_cmd.buf_len = len;
            // 把应用程序要发送的数据拷贝到共享内存区
            memcpy(spi_rt_interface.spi_expand_send_shared_mem_buf + spi_rt_interface.expand_send_buf_len, data, len);
            spi_rt_interface.expand_send_buf_len += len;
            //PRINT("spi_rt_interface.expand_send_buf_len = %d\n", spi_rt_interface.expand_send_buf_len);
            break;
        }
        default:
        {
            PRINT("option does not mismatch!\n");
            return MISMATCH_ERR;
        }
    }
    
    if (res == 0)
    {
        if ((res = write_spi_rt_para(sem_num)) < 0)
        {
            PERROR("write_spi_rt_para failed!\n");    
        }
    }
    
    // 把共享内存脱离出当前进程
    switch (release_flag)
    {
        case 4:
        {
            if (shmdt(spi_rt_interface.spi_expand_send_shared_mem_buf) < 0)
            {
                PERROR("shmdt failed!\n");
                res = SHMDT_ERR;
            }
            break;
        }
        case 3:
        {
            if (shmdt(spi_rt_interface.spi_uart3_send_shared_mem_buf) < 0)
            {
                PERROR("shmdt failed!\n");
                res = SHMDT_ERR;
            }
            break;
        }
        case 2:
        {
            if (shmdt(spi_rt_interface.spi_uart2_send_shared_mem_buf) < 0)
            {
                PERROR("shmdt failed!\n");
                res = SHMDT_ERR;
            }
            break;
        }
        case 1:
        {
            if (shmdt(spi_rt_interface.spi_uart1_send_shared_mem_buf) < 0)
            {
                PERROR("shmdt failed!\n");
                res = SHMDT_ERR;
            }
            break;
        }
        default:
            break;
    }
    
    semopbuf.sem_op = 1;
    semopbuf.sem_num = sem_num;
    
    //PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
    {
        PERROR("semop failed!\n");
        return SEMOP_ERR;
    }
    //PRINT("res = %d\n", res);
    if (res < 0)
    {
        PRINT("send failed!\n");
        return res;
    }
    
    #if LOCAL_SOCKET
    // 创建客户端连接
    if ((spi_client_fd = internetwork_communication.make_local_socket_client_link(SPI_LOCAL_SOCKET_NAME)) < 0)
    {
        PERROR("make_client_link failed!\n");
        return spi_client_fd;
    }
    #else
    // 创建客户端连接
    if ((spi_client_fd = internetwork_communication.make_client_link(SPI_SERVER_IP, SPI_SERVER_PORT)) < 0)
    {
        PERROR("make_client_link failed!\n");
        return spi_client_fd;
    }
    #endif
    //PRINT("spi_client_fd = %d\n", spi_client_fd);
    // 发送命令
    if ((res = spi_rt_interface.send_spi_rt_cmd_msg(spi_client_fd, &spi_rt_cmd, &tv)) < 0)
    {
        PERROR("send_spi_rt_cmd_msg failed!\n");
        // 关闭客户端连接
        close(spi_client_fd);
        return res;
    }
    PRINT("tv.tv_sec = %d\n", tv.tv_sec);
    // 接收发送情况
    tv.tv_sec = 60;
    if ((res = spi_rt_interface.recv_spi_rt_cmd_msg(spi_client_fd, &spi_rt_cmd, &tv)) < 0)
    {
        PERROR("recv_spi_rt_cmd_msg failed!\n");
        // 关闭客户端连接
        close(spi_client_fd);
        return res;
    }
    // 关闭客户端连接
    close(spi_client_fd);
    
    // 当内容（即buf_len）为0xFFFF时，表示STUB服务器在发送的某个过程中出现错误
    if (spi_rt_cmd.buf_len == 0xFFFF)
    {
        PRINT("write failed!\n");
        return WRITE_ERR;
    }
    
    // 当不为0xFFFF时，把接收长度负值给res，然后释放资源
    PRINT("send success!\n");
    
    return spi_rt_cmd.buf_len;

EXIT:
    // 失败后清空缓冲区
}

#else

/**
 * spi 发送数据
 */
int send_data(unsigned char uart, char *data, unsigned short len)
{
    //PRINT("[uart = %d, len = %d]\n", uart, len);
    int i = 0, j = 0;
    int res = 0, ret = 0;
    int spi_client_fd = 0;       // 客户端套接字
	char uart_no = 0;            // 串口数据标识
	int release_flag = 0;        // 资源释放标识
	unsigned char sem_num = 0;   // 信号量序号
    struct sembuf semopbuf;      // 信号量结构
    
    struct timeval tv = {60, 0};
    struct s_spi_rt_cmd spi_rt_cmd; // spi接收发送命令结构
    memset(&spi_rt_cmd, 0, sizeof(struct s_spi_rt_cmd));
    
	if (data == NULL)
	{
	    PRINT("data is NULL!\n");
        return NULL_ERR;
	}
	if (len == 0)
	{
	    PRINT("len is zero!\n");
        return DATA_ERR;
	}
	PRINT_BUF_BY_HEX(data, NULL, len, __FILE__, __FUNCTION__, __LINE__);
	
	// 获取8个信号
    if ((spi_rt_interface.spi_uart_sem_id = semget(spi_rt_interface.spi_uart_sem_key, 8, IPC_CREAT | 0666)) < 0)
    {
        PERROR("semget failed!\n");
        return SEMGET_ERR;
    }

    semopbuf.sem_op = -1;
    semopbuf.sem_flg = SEM_UNDO;
    
    // 设置发送到STUB服务器的命令
    spi_rt_cmd.cmd = 'T';
    spi_rt_cmd.uart = uart;
    
	switch (uart)
    {
        case UART1:
        {
            // 信号量减一
            semopbuf.sem_num = 0;
            sem_num = 0;
            PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }

            // 把串口1发送缓冲区（共享内存区）添加到调用进程
            if ((spi_rt_interface.spi_uart1_send_shared_mem_id = shmget(spi_rt_interface.spi_uart1_send_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
            {
                PERROR("shmget failed!\n");
                res = SHMGET_ERR;
                break;
            }
            if ((spi_rt_interface.spi_uart1_send_shared_mem_buf = shmat(spi_rt_interface.spi_uart1_send_shared_mem_id, NULL, 0)) < 0)
            {
                PERROR("shmat failed!\n");
                res = SHMAT_ERR;
                break;
            }
            release_flag = 1;
            
            while (1)
            {
                if ((res = read_spi_rt_para(sem_num)) < 0)
                {
                    PERROR("read_spi_rt_para failed!\n");
                    break;
                }
                //PRINT("spi_rt_interface.uart1_send_buf_len = %d\n", spi_rt_interface.uart1_send_buf_len);
                if ((spi_rt_interface.uart1_send_buf_len == SHARED_MEM_SIZE) || ((spi_rt_interface.uart1_send_buf_len + len) > SHARED_MEM_SIZE))
                {
                    // 释放信号量
                    semopbuf.sem_op = 1;
                    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                    {
                        PERROR("semop failed!\n");
                        res = SEMOP_ERR;
                        break;
                    }
                    
                    usleep(DELAY);
                    
                    // 获取信号量
                    semopbuf.sem_op = -1;
                    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                    {
                        PERROR("semop failed!\n");
                        res = SEMOP_ERR;
                        break;
                    }
                    continue;
                }
                break;
            }
            
            if (res < 0)
            {
                break;
            }
            
            spi_rt_cmd.buf_len = len;
            // 把应用程序要发送的数据拷贝到共享内存区
            memcpy(spi_rt_interface.spi_uart1_send_shared_mem_buf + spi_rt_interface.uart1_send_buf_len, data, len);
            spi_rt_interface.uart1_send_buf_len += len;
            //PRINT("spi_rt_interface.uart1_send_buf_len = %d\n", spi_rt_interface.uart1_send_buf_len);
            break;
        }
        case UART2:
        {
            // 信号量减一
            semopbuf.sem_num = 1;
            sem_num = 1;
            //PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }
            
            if ((spi_rt_interface.spi_uart2_send_shared_mem_id = shmget(spi_rt_interface.spi_uart2_send_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
            {
                PERROR("shmget failed!\n");
                res = SHMGET_ERR;
                break;
            }
            
            if ((spi_rt_interface.spi_uart2_send_shared_mem_buf = shmat(spi_rt_interface.spi_uart2_send_shared_mem_id, NULL, 0)) < 0)
            {
                PERROR("shmat failed!\n");
                res = SHMAT_ERR;
                break;
            }
            release_flag = 2;
            
            while (1)
            {
                if ((res = read_spi_rt_para(sem_num)) < 0)
                {
                    PERROR("read_spi_rt_para failed!\n");
                    break;
                }
                //PRINT("spi_rt_interface.uart2_send_buf_len = %d\n", spi_rt_interface.uart2_send_buf_len);
                if ((spi_rt_interface.uart2_send_buf_len == SHARED_MEM_SIZE) || ((spi_rt_interface.uart2_send_buf_len + len) > SHARED_MEM_SIZE))
                {
                    // 释放信号量
                    semopbuf.sem_op = 1;
                    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                    {
                        PERROR("semop failed!\n");
                        res = SEMOP_ERR;
                        break;
                    }
                    
                    usleep(DELAY);
                    
                    // 获取信号量
                    semopbuf.sem_op = -1;
                    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                    {
                        PERROR("semop failed!\n");
                        res = SEMOP_ERR;
                        break;
                    }
                    continue;
                }
                break;
            }
            
            if (res < 0)
            {
                break;
            }
            
            spi_rt_cmd.buf_len = len;
            memcpy(spi_rt_interface.spi_uart2_send_shared_mem_buf + spi_rt_interface.uart2_send_buf_len, data, len);
            spi_rt_interface.uart2_send_buf_len += len;
            //PRINT("spi_rt_interface.uart2_send_buf_len = %d\n", spi_rt_interface.uart2_send_buf_len);
            break;
        }
        case UART3:
        {
            // 信号量减一
            semopbuf.sem_num = 2;
            sem_num = 2;
            //PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }
            
            if ((spi_rt_interface.spi_uart3_send_shared_mem_id = shmget(spi_rt_interface.spi_uart3_send_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
            {
                PERROR("shmget failed!");
                res = SHMGET_ERR;
                break;
            }
            
            if ((spi_rt_interface.spi_uart3_send_shared_mem_buf = shmat(spi_rt_interface.spi_uart3_send_shared_mem_id, NULL, 0)) < 0)
            {
                PERROR("shmat failed!\n");
                res = SHMAT_ERR;
                break;
            }
            release_flag = 3;
            
            while (1)
            {
                if ((res = read_spi_rt_para(sem_num)) < 0)
                {
                    PERROR("read_spi_rt_para failed!\n");
                    break;
                }
                //PRINT("spi_rt_interface.uart3_send_buf_len = %d\n", spi_rt_interface.uart3_send_buf_len);
                if ((spi_rt_interface.uart3_send_buf_len == SHARED_MEM_SIZE) || ((spi_rt_interface.uart3_send_buf_len + len) > SHARED_MEM_SIZE))
                {
                    // 释放信号量
                    semopbuf.sem_op = 1;
                    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                    {
                        PERROR("semop failed!\n");
                        res = SEMOP_ERR;
                        break;
                    }
                    
                    usleep(DELAY);
                    
                    // 获取信号量
                    semopbuf.sem_op = -1;
                    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                    {
                        PERROR("semop failed!\n");
                        res = SEMOP_ERR;
                        break;
                    }
                    continue;
                }
                break;
            }
            if (res < 0)
            {
                break;
            }
            
            spi_rt_cmd.buf_len = len;
            memcpy(spi_rt_interface.spi_uart3_recv_shared_mem_buf + spi_rt_interface.uart3_send_buf_len, data, len);
            spi_rt_interface.uart3_send_buf_len += len;
            //PRINT("spi_rt_interface.uart3_send_buf_len = %d\n", spi_rt_interface.uart3_send_buf_len);
            break;
        }
        case EXPAND:
        {
            // 信号量减一
            semopbuf.sem_num = 3;
            sem_num = 3;
            PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }

            // 把扩展功能发送缓冲区（共享内存区）添加到调用进程
            if ((spi_rt_interface.spi_expand_send_shared_mem_id = shmget(spi_rt_interface.spi_expand_send_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
            {
                PERROR("shmget failed!\n");
                res = SHMGET_ERR;
                break;
            }
            
            if ((spi_rt_interface.spi_expand_send_shared_mem_buf = shmat(spi_rt_interface.spi_expand_send_shared_mem_id, NULL, 0)) < 0)
            {
                PERROR("shmat failed!\n");
                res = SHMAT_ERR;
                break;
            }
            release_flag = 4;
            
            while (1)
            {
                if ((res = read_spi_rt_para(sem_num)) < 0)
                {
                    PERROR("read_spi_rt_para failed!\n");
                    break;
                }
                //PRINT("spi_rt_interface.expand_send_buf_len = %d\n", spi_rt_interface.expand_send_buf_len);
                if ((spi_rt_interface.expand_send_buf_len == SHARED_MEM_SIZE) || ((spi_rt_interface.expand_send_buf_len + len) > SHARED_MEM_SIZE))
                {
                    // 释放信号量
                    semopbuf.sem_op = 1;
                    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                    {
                        PERROR("semop failed!\n");
                        res = SEMOP_ERR;
                        break;
                    }
                    
                    usleep(DELAY);
                    
                    // 获取信号量
                    semopbuf.sem_op = -1;
                    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
                    {
                        PERROR("semop failed!\n");
                        res = SEMOP_ERR;
                        break;
                    }
                    continue;
                }
                break;
            }
            
            if (res < 0)
            {
                break;
            }
            
            spi_rt_cmd.buf_len = len;
            // 把应用程序要发送的数据拷贝到共享内存区
            memcpy(spi_rt_interface.spi_expand_send_shared_mem_buf + spi_rt_interface.expand_send_buf_len, data, len);
            spi_rt_interface.expand_send_buf_len += len;
            //PRINT("spi_rt_interface.expand_send_buf_len = %d\n", spi_rt_interface.expand_send_buf_len);
            break;
        }
        default:
        {
            PRINT("option does not mismatch!\n");
            return MISMATCH_ERR;
        }
    }
    
    if (res == 0)
    {
        if ((res = write_spi_rt_para(sem_num)) < 0)
        {
            PERROR("write_spi_rt_para failed!\n");    
        }
    }
    
    // 把共享内存脱离出当前进程
    switch (release_flag)
    {
        case 4:
        {
            if (shmdt(spi_rt_interface.spi_expand_send_shared_mem_buf) < 0)
            {
                PERROR("shmdt failed!\n");
                res = SHMDT_ERR;
            }
            break;
        }
        case 3:
        {
            if (shmdt(spi_rt_interface.spi_uart3_send_shared_mem_buf) < 0)
            {
                PERROR("shmdt failed!\n");
                res = SHMDT_ERR;
            }
            break;
        }
        case 2:
        {
            if (shmdt(spi_rt_interface.spi_uart2_send_shared_mem_buf) < 0)
            {
                PERROR("shmdt failed!\n");
                res = SHMDT_ERR;
            }
            break;
        }
        case 1:
        {
            if (shmdt(spi_rt_interface.spi_uart1_send_shared_mem_buf) < 0)
            {
                PERROR("shmdt failed!\n");
                res = SHMDT_ERR;
            }
            break;
        }
        default:
            break;
    }
    
    semopbuf.sem_op = 1;
    semopbuf.sem_num = sem_num;
    
    //PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
    {
        PERROR("semop failed!\n");
        return SEMOP_ERR;
    }
    //PRINT("res = %d\n", res);
    if (res < 0)
    {
        PRINT("send failed!\n");
        return res;
    }
    
    #if LOCAL_SOCKET
    // 创建客户端连接
    if ((spi_client_fd = internetwork_communication.make_local_socket_client_link(SPI_LOCAL_SOCKET_NAME)) < 0)
    {
        PERROR("make_client_link failed!\n");
        res = spi_client_fd;
        goto EXIT;
    }
    #else
    // 创建客户端连接
    if ((spi_client_fd = internetwork_communication.make_client_link(SPI_SERVER_IP, SPI_SERVER_PORT)) < 0)
    {
        PERROR("make_client_link failed!\n");
        res = spi_client_fd;
        goto EXIT;
    }
    #endif
    //PRINT("spi_client_fd = %d\n", spi_client_fd);
    // 发送命令
    if ((res = spi_rt_interface.send_spi_rt_cmd_msg(spi_client_fd, &spi_rt_cmd, &tv)) < 0)
    {
        PERROR("send_spi_rt_cmd_msg failed!\n");
        // 关闭客户端连接
        close(spi_client_fd);
        goto EXIT;
    }
    PRINT("tv.tv_sec = %d\n", tv.tv_sec);
    // 接收发送情况
    tv.tv_sec = 60;
    if ((res = spi_rt_interface.recv_spi_rt_cmd_msg(spi_client_fd, &spi_rt_cmd, &tv)) < 0)
    {
        PERROR("recv_spi_rt_cmd_msg failed!\n");
        // 关闭客户端连接
        close(spi_client_fd);
        goto EXIT;
    }
    // 关闭客户端连接
    close(spi_client_fd);
    
    // 当内容（即buf_len）为0xFFFF时，表示STUB服务器在发送的某个过程中出现错误
    if (spi_rt_cmd.buf_len == 0xFFFF)
    {
        PRINT("write failed!\n");
        return WRITE_ERR;
    }
    
    // 当不为0xFFFF时，把接收长度负值给res，然后释放资源
    PRINT("send success!\n");
    
    return spi_rt_cmd.buf_len;

EXIT:
    // 失败后清空缓冲区
    PRINT("send failed! res = %d\n", res);
    
    semopbuf.sem_op = -1;
    semopbuf.sem_flg = SEM_UNDO;
    switch (uart)
    {
        case UART1:
        {
            // 信号量减一
            semopbuf.sem_num = 0;
            sem_num = 0;
            PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }
            
            spi_rt_interface.uart1_send_buf_len -= len;
            break;
        }
        case UART2:
        {
            // 信号量减一
            semopbuf.sem_num = 1;
            sem_num = 1;
            PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }
            
            spi_rt_interface.uart2_send_buf_len -= len;
            break;
        }
        case UART3:
        {
            // 信号量减一
            semopbuf.sem_num = 2;
            sem_num = 2;
            PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }
            
            spi_rt_interface.uart3_send_buf_len -= len;
            break;
        }
        case EXPAND:
        {
            // 信号量减一
            semopbuf.sem_num = 3;
            sem_num = 3;
            PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }
            
            spi_rt_interface.expand_send_buf_len -= len;
            break;
        }
        default:
        {
            PRINT("option does not mismatch!\n");
            return MISMATCH_ERR;
        }
    }
    
    if (ret == 0)
    {
        if ((ret = write_spi_rt_para(sem_num)) < 0)
        {
            PERROR("write_spi_rt_para failed!\n");    
        }
    }
    // 释放信号量
    semopbuf.sem_op = 1;
    semopbuf.sem_num = sem_num;
    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
    {
        PERROR("semop failed!\n");
        return SEMOP_ERR;
    }
        
    return res;
}
#endif
/**
 * spi 发送重发命令
 */
int send_repeat_cmd()
{
    int res = 0;
    struct s_spi_rt_cmd spi_rt_cmd;
    struct sembuf semopbuf;      // 信号量结构
    
    spi_rt_cmd.cmd = 'T'; 
    spi_rt_cmd.uart = EXPAND;
    spi_rt_cmd.buf_len = 1;
    char data = 0x08;
    
    // 获取8个信号
    if ((spi_rt_interface.spi_uart_sem_id = semget(spi_rt_interface.spi_uart_sem_key, 8, IPC_CREAT | 0666)) < 0)
    {
        PERROR("semget failed!\n");
        return SEMGET_ERR;
    }

    semopbuf.sem_op = -1;
    semopbuf.sem_flg = SEM_UNDO;    
    semopbuf.sem_num = 3;
    
    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
    {
        PERROR("semop failed!\n");
        return SEMOP_ERR;
    }
    
    do
    {
        if ((res = read_spi_rt_para(semopbuf.sem_num)) < 0)
        {
            PERROR("read_spi_rt_para failed!\n");
            break;
        }
        usleep(DELAY); 
    }
    while ((spi_rt_interface.expand_send_buf_len == SHARED_MEM_SIZE) || ((spi_rt_interface.expand_send_buf_len + 1) > SHARED_MEM_SIZE));
    
    if (res < 0)
    {
        semopbuf.sem_op = 1;
        if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
        {
            PERROR("semop failed!\n");
            return SEMOP_ERR;
        }
    }
        
    // 把应用程序要发送的数据拷贝到共享内存区
    memcpy(spi_rt_interface.spi_expand_send_shared_mem_buf + spi_rt_interface.expand_send_buf_len, &data, sizeof(data));
    spi_rt_interface.expand_send_buf_len++;
            
    
    res = write_spi_rt_para(semopbuf.sem_num);
    
    semopbuf.sem_op = 1;
    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
    {
        PERROR("semop failed!\n");
        return SEMOP_ERR;
    }
    
    if (res < 0)
    {
        PERROR("write_spi_rt_para failed!\n"); 
        return res;
    }
    
    if ((res = spi_send(&spi_rt_cmd)) < 0)
    {
        PERROR("spi_send failed!\n"); 
        return res;
    }
    return 0;
}

#if BUF_DATA_FLAG
/**
 * spi 接收数据
 */
int recv_data(unsigned char uart, char *data, unsigned short len)
{
    //PRINT("[uart = %d, len = %d]\n", uart, len);
    int i = 0;
    int res = 0, ret = 0;
    char release_flag = 0;            // 资源释放标识
    unsigned char sem_num = 0;        // 信号量序号
    
    int spi_client_fd = 0;            // 客户端套接字
    struct sembuf semopbuf;           // 信号量结构
    
    struct s_spi_rt_cmd spi_rt_cmd;   // spi接收发送命令结构
    memset(&spi_rt_cmd, 0, sizeof(struct s_spi_rt_cmd));
    
    fd_set fdset;
    struct timeval tv = {15, 0};
    
	if (data == NULL)
	{
	    PRINT("data is NULL!\n");
        return NULL_ERR;
	}
	
	if (len == 0)
	{
	    PRINT("len is zero!\n");
        return DATA_ERR;
	}
	
    // 获取8个信号
    if ((spi_rt_interface.spi_uart_sem_id = semget(spi_rt_interface.spi_uart_sem_key, 8, IPC_CREAT | 0666)) < 0)
    {
        PERROR("semget failed!\n");
        return SEMGET_ERR;
    }
    
    memset(&semopbuf, 0, sizeof(struct sembuf));
    semopbuf.sem_op = -1;
    semopbuf.sem_flg = SEM_UNDO;
    
    /* 根据不同的串口（uart）解析共享内存区，当内存区数据长度大于等于需要读取的长度时，把共享区的数据
     * 拷贝到buf，然后清空共享内存区，释放资源返回，读取的长度
     */
	switch (uart)
    {
        case UART1:
        {
            semopbuf.sem_num = 4;
            sem_num = 4;
            //PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            //PRINT("before spi_select\n");
            // 判断接收缓冲区是否有数据可读
            if ((res = spi_select(uart, &tv)) < 0)
            {
                PERROR("spi_select failed!\n");
                return res;
            }
            //PRINT("after spi_select\n");
            // 信号量减一
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }
            
            if ((res = read_spi_rt_para(sem_num)) < 0)
            {
                PERROR("read_spi_rt_para failed!\n");
                break;
            }
            //PRINT("spi_rt_interface.uart1_recv_count = %d\n", spi_rt_interface.uart1_recv_count);
            
            if (spi_rt_interface.uart1_recv_count > 0)
            {
                if ((spi_rt_interface.spi_uart1_recv_shared_mem_id = shmget(spi_rt_interface.spi_uart1_recv_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "shmget failed!", res);
                    res = SHMGET_ERR;
                    break;
                }
                if ((spi_rt_interface.spi_uart1_recv_shared_mem_buf = shmat(spi_rt_interface.spi_uart1_recv_shared_mem_id, NULL, 0)) < 0)
                {
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "shmget failed!", res);
                    res = SHMGET_ERR;
                    break;
                }
                release_flag = 1;
                //PRINT_BUF_BY_HEX(spi_rt_interface.spi_uart1_recv_shared_mem_buf, NULL, spi_rt_interface.uart1_recv_count, __FILE__, __FUNCTION__, __LINE__);
                //PRINT("len = %d spi_rt_interface.uart1_recv_count = %d\n", len, spi_rt_interface.uart1_recv_count);
                if (len == spi_rt_interface.uart1_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_uart1_recv_shared_mem_buf, len);
                    memset(spi_rt_interface.spi_uart1_recv_shared_mem_buf, 0, spi_rt_interface.uart1_recv_count);
                    spi_rt_interface.uart1_recv_count = 0;
                    res = len;
                    break;
                }
                else if (len < spi_rt_interface.uart1_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_uart1_recv_shared_mem_buf, len);
                    spi_rt_interface.uart1_recv_count -= len;
                    memcpy(spi_rt_interface.spi_uart1_recv_shared_mem_buf, spi_rt_interface.spi_uart1_recv_shared_mem_buf + len, spi_rt_interface.uart1_recv_count);
                    memset(spi_rt_interface.spi_uart1_recv_shared_mem_buf + spi_rt_interface.uart1_recv_count, 0, len);
                    res = len;
                    break;
                }
                else if (len > spi_rt_interface.uart1_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_uart1_recv_shared_mem_buf, spi_rt_interface.uart1_recv_count);
                    res = spi_rt_interface.uart1_recv_count;
                    memset(spi_rt_interface.spi_uart1_recv_shared_mem_buf, 0, spi_rt_interface.uart1_recv_count);
                    len -= spi_rt_interface.uart1_recv_count;
                    spi_rt_interface.uart1_recv_count = 0;
                    break;
                }
            }
            break;
        }
        case UART2:
        {
            semopbuf.sem_num = 5;
            sem_num = 5;
            PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            
            // 判断接收缓冲区是否有数据可读
            if ((res = spi_select(uart, &tv)) < 0)
            {
                PERROR("spi_select failed!\n");
                return res;
            }
            
            // 信号量减一
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }
            
            if ((res = read_spi_rt_para(sem_num)) < 0)
            {
                PERROR("read_spi_rt_para failed!\n");
                break;
            }
            PRINT("spi_rt_interface.uart2_recv_count = %d\n", spi_rt_interface.uart2_recv_count);
            
            if (spi_rt_interface.uart2_recv_count > 0)
            {
                if ((spi_rt_interface.spi_uart2_recv_shared_mem_id = shmget(spi_rt_interface.spi_uart2_recv_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
                {
                    res = SHMGET_ERR;
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "shmget failed!", res);
                    break;
                }
                
                if ((spi_rt_interface.spi_uart2_recv_shared_mem_buf = shmat(spi_rt_interface.spi_uart2_recv_shared_mem_id, NULL, 0)) < 0)
                {
                    res = SHMGET_ERR;
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "shmget failed!", res);
                    break;
                }
                release_flag = 2;
                
                if (len == spi_rt_interface.uart2_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_uart2_recv_shared_mem_buf, len);
                    memset(spi_rt_interface.spi_uart2_recv_shared_mem_buf, 0, spi_rt_interface.uart2_recv_count);
                    spi_rt_interface.uart2_recv_count = 0;
                    res = len;
                    break;
                }
                else if (len < spi_rt_interface.uart2_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_uart2_recv_shared_mem_buf, len);
                    spi_rt_interface.uart2_recv_count -= len;
                    memcpy(spi_rt_interface.spi_uart2_recv_shared_mem_buf, spi_rt_interface.spi_uart2_recv_shared_mem_buf + len, spi_rt_interface.uart2_recv_count);
                    memset(spi_rt_interface.spi_uart2_recv_shared_mem_buf + spi_rt_interface.uart2_recv_count, 0, len);
                    res = len;
                    break;
                }
                else if (len > spi_rt_interface.uart2_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_uart2_recv_shared_mem_buf, spi_rt_interface.uart2_recv_count);
                    res = spi_rt_interface.uart2_recv_count;
                    memset(spi_rt_interface.spi_uart2_recv_shared_mem_buf, 0, spi_rt_interface.uart2_recv_count);
                    len -= spi_rt_interface.uart2_recv_count;
                    spi_rt_interface.uart2_recv_count = 0;
                    break;
                }
            }
            break;
        }   
        case UART3:
        {
            semopbuf.sem_num = 6;
            sem_num = 6;
            PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            
            // 判断接收缓冲区是否有数据可读
            if ((res = spi_select(uart, &tv)) < 0)
            {
                PERROR("spi_select failed!\n");
                return res;
            }
            
            // 信号量减一
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }
            
            if ((res = read_spi_rt_para(sem_num)) < 0)
            {
                PERROR("read_spi_rt_para failed!\n");
                break;
            }
            PRINT("spi_rt_interface.uart3_recv_count = %d\n", spi_rt_interface.uart3_recv_count);
            
            if (spi_rt_interface.uart3_recv_count > 0)
            {
                if ((spi_rt_interface.spi_uart3_recv_shared_mem_id = shmget(spi_rt_interface.spi_uart3_recv_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
                {
                    PERROR("shmget failed!");
                    res = SHMGET_ERR;
                    break;
                }
                
                if ((spi_rt_interface.spi_uart3_recv_shared_mem_buf = shmat(spi_rt_interface.spi_uart3_recv_shared_mem_id, NULL, 0)) < 0)
                {
                    PERROR("shmget failed!");
                    res = SHMGET_ERR;
                    break;
                }
                release_flag = 3;
    
                if (len == spi_rt_interface.uart3_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_uart3_recv_shared_mem_buf, len);
                    memset(spi_rt_interface.spi_uart3_recv_shared_mem_buf, 0, spi_rt_interface.uart3_recv_count);
                    spi_rt_interface.uart3_recv_count = 0;
                    res = len;
                    break;
                }
                else if (len < spi_rt_interface.uart3_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_uart3_recv_shared_mem_buf, len);
                    spi_rt_interface.uart3_recv_count -= len;
                    memcpy(spi_rt_interface.spi_uart3_recv_shared_mem_buf, spi_rt_interface.spi_uart3_recv_shared_mem_buf + len, spi_rt_interface.uart3_recv_count);
                    memset(spi_rt_interface.spi_uart3_recv_shared_mem_buf + spi_rt_interface.uart3_recv_count, 0, len);
                    res = len;
                    break;
                }
                else if (len > spi_rt_interface.uart3_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_uart3_recv_shared_mem_buf, spi_rt_interface.uart3_recv_count);
                    res = spi_rt_interface.uart3_recv_count;
                    memset(spi_rt_interface.spi_uart3_recv_shared_mem_buf, 0, spi_rt_interface.uart3_recv_count);
                    len -= spi_rt_interface.uart3_recv_count;
                    spi_rt_interface.uart3_recv_count = 0;
                    break;
                }
            }
            break;
        }
        case EXPAND:
        {
            semopbuf.sem_num = 7;
            sem_num = 7;
            //PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            
            // 判断接收缓冲区是否有数据可读
            if ((res = spi_select(uart, &tv)) < 0)
            {
                PERROR("spi_select failed!\n");
                return res;
            }
            // 信号量减一
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }
            
            if ((res = read_spi_rt_para(sem_num)) < 0)
            {
                PERROR("read_spi_rt_para failed!\n");
                break;
            }
            //PRINT("spi_rt_interface.uart1_recv_count = %d\n", spi_rt_interface.uart1_recv_count);
            
            if (spi_rt_interface.expand_recv_count > 0)
            {
                if ((spi_rt_interface.spi_expand_recv_shared_mem_id = shmget(spi_rt_interface.spi_expand_recv_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
                {
                    res = SHMGET_ERR;
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "shmget failed!", res);
                    break;
                }
                if ((spi_rt_interface.spi_expand_recv_shared_mem_buf = shmat(spi_rt_interface.spi_expand_recv_shared_mem_id, NULL, 0)) < 0)
                {
                    res = SHMGET_ERR;
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "shmget failed!", res);
                    break;
                }
                release_flag = 4;
                //PRINT_BUF_BY_HEX(spi_rt_interface.spi_expand_recv_shared_mem_buf, NULL, spi_rt_interface.expand_recv_count, __FILE__, __FUNCTION__, __LINE__);
                //PRINT("len = %d spi_rt_interface.expand_recv_count = %d\n", len, spi_rt_interface.expand_recv_count);
                if (len == spi_rt_interface.expand_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_expand_recv_shared_mem_buf, len);
                    memset(spi_rt_interface.spi_expand_recv_shared_mem_buf, 0, spi_rt_interface.expand_recv_count);
                    spi_rt_interface.expand_recv_count = 0;
                    res = len;
                    break;
                }
                else if (len < spi_rt_interface.expand_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_expand_recv_shared_mem_buf, len);
                    spi_rt_interface.expand_recv_count -= len;
                    memcpy(spi_rt_interface.spi_expand_recv_shared_mem_buf, spi_rt_interface.spi_expand_recv_shared_mem_buf + len, spi_rt_interface.expand_recv_count);
                    memset(spi_rt_interface.spi_expand_recv_shared_mem_buf + spi_rt_interface.expand_recv_count, 0, len);
                    res = len;
                    break;
                }
                else if (len > spi_rt_interface.expand_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_expand_recv_shared_mem_buf, spi_rt_interface.expand_recv_count);
                    res = spi_rt_interface.expand_recv_count;
                    memset(spi_rt_interface.spi_expand_recv_shared_mem_buf, 0, spi_rt_interface.expand_recv_count);
                    len -= spi_rt_interface.expand_recv_count;
                    spi_rt_interface.expand_recv_count = 0;
                    break;
                }
            }
            break;
        }
        default:
        {
            PRINT("option does not mismatch!");
            res = MISMATCH_ERR;
            break;
        }
    }
    
    if (res >= 0)
    {
        if ((ret = write_spi_rt_para(sem_num)) < 0)
        {
            PERROR("write_spi_rt_para failed!\n");
            res = ret;
        }
    }
    
    //PRINT("release_flag = %d\n", release_flag);
    switch (release_flag)
    {
        case 4:
        {
            if (shmdt(spi_rt_interface.spi_expand_recv_shared_mem_buf) < 0)
            {
                PERROR("shmdt failed!\n");
                res = SHMDT_ERR;
            }
            break;
        }
        case 3:
        {
            if (shmdt(spi_rt_interface.spi_uart3_recv_shared_mem_buf) < 0)
            {
                PERROR("shmdt failed!\n");
                res = SHMDT_ERR;
            }
            break;
        }
        case 2:
        {
            if (shmdt(spi_rt_interface.spi_uart2_recv_shared_mem_buf) < 0)
            {
                PERROR("shmdt failed!\n");
                res = SHMDT_ERR;
            }
            break;
        }
        case 1:
        {
            if (shmdt(spi_rt_interface.spi_uart1_recv_shared_mem_buf) < 0)
            {
                PERROR("shmdt failed!\n");
                res = SHMDT_ERR;
            }
            break;
        }
        default:
            break;
    }
    
    semopbuf.sem_op = 1;
    semopbuf.sem_num = sem_num;
    //PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
    {
        PERROR("semop failed!\n");
        return SEMOP_ERR;
    }
     
    // 当res小于0时，说明以上处理出现错误，大于0时，此时res为要返回的数据长度
    //PRINT("res = %d\n", res);
    return res;
}

#else 
/**
 * spi 接收数据(缓冲区循环读取)
 */
int recv_data(unsigned char uart, char *data, unsigned short len)
{
    //PRINT("[uart = %d, len = %d]\n", uart, len);
    int i = 0;
    int res = 0, ret = 0;
    char release_flag = 0;            // 资源释放标识
    unsigned char sem_num = 0;        // 信号量序号
    
    int spi_client_fd = 0;            // 客户端套接字
    
    struct sembuf semopbuf;           // 信号量结构
    
    struct s_spi_rt_cmd spi_rt_cmd;   // spi接收发送命令结构
    memset(&spi_rt_cmd, 0, sizeof(struct s_spi_rt_cmd));
    
    fd_set fdset;
    struct timeval tv = {15, 0};
    
    unsigned char recv_count_name[30] = {0};
    unsigned char recv_count_value[100] = {0};
    unsigned short recv_count_len = 0;
	if (data == NULL)
	{
	    PRINT("data is NULL!\n");
        return NULL_ERR;
	}
	
	if (len == 0)
	{
	    PRINT("len is zero!\n");
        return DATA_ERR;
	}
	
    // 获取8个信号
    if ((spi_rt_interface.spi_uart_sem_id = semget(spi_rt_interface.spi_uart_sem_key, 8, IPC_CREAT | 0666)) < 0)
    {
        PERROR("semget failed!\n");
        return SEMGET_ERR;
    }
    
    memset(&semopbuf, 0, sizeof(struct sembuf));
    semopbuf.sem_op = -1;
    semopbuf.sem_flg = SEM_UNDO;
    
    /* 根据不同的串口（uart）解析共享内存区，当内存区数据长度大于等于需要读取的长度时，把共享区的数据
     * 拷贝到buf，然后清空共享内存区，释放资源返回，读取的长度
     */
	switch (uart)
    {
        case UART1:
        {
            semopbuf.sem_num = 4;
            sem_num = 4;
            //PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            
            // 判断接收缓冲区是否有数据可读
            if ((res = spi_select(uart, &tv)) < 0)
            {
                PERROR("spi_select failed!\n");
                return res;
            }
            // 信号量减一
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }
            
            if ((res = read_spi_rt_para(sem_num)) < 0)
            {
                PERROR("read_spi_rt_para failed!\n");
                break;
            }
            //PRINT("spi_rt_interface.uart1_recv_count = %d\n", spi_rt_interface.uart1_recv_count);
            
            if (spi_rt_interface.uart1_recv_count > 0)
            {
                if ((spi_rt_interface.spi_uart1_recv_shared_mem_id = shmget(spi_rt_interface.spi_uart1_recv_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
                {
                    res = SHMGET_ERR;
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "shmget failed!", res);
                    break;
                }
                if ((spi_rt_interface.spi_uart1_recv_shared_mem_buf = shmat(spi_rt_interface.spi_uart1_recv_shared_mem_id, NULL, 0)) < 0)
                {
                    res = SHMGET_ERR;
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "shmget failed!", res);
                    break;
                }
                release_flag = 1;
                //PRINT_BUF_BY_HEX(spi_rt_interface.spi_uart1_recv_shared_mem_buf, NULL, spi_rt_interface.uart1_recv_count, __FILE__, __FUNCTION__, __LINE__);
                //PRINT("len = %d spi_rt_interface.uart1_recv_count = %d\n", len, spi_rt_interface.uart1_recv_count);
                if (len == spi_rt_interface.uart1_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_uart1_recv_shared_mem_buf + spi_rt_interface.spi_uart1_recv_read_index, len);
                    memset(spi_rt_interface.spi_uart1_recv_shared_mem_buf + spi_rt_interface.spi_uart1_recv_read_index, 0, spi_rt_interface.uart1_recv_count);
                    spi_rt_interface.uart1_recv_count = 0;
                    spi_rt_interface.spi_uart1_recv_read_index = 0;
                    spi_rt_interface.spi_uart1_recv_write_index = 0;
                    res = len;
                    break;
                }
                else if (len < spi_rt_interface.uart1_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_uart1_recv_shared_mem_buf + spi_rt_interface.spi_uart1_recv_read_index, len);
                    spi_rt_interface.uart1_recv_count -= len;
                    memset(spi_rt_interface.spi_uart1_recv_shared_mem_buf + spi_rt_interface.spi_uart1_recv_read_index, 0, len);
                    spi_rt_interface.spi_uart1_recv_read_index += len;
                    res = len;
                    break;
                }
                else if (len > spi_rt_interface.uart1_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_uart1_recv_shared_mem_buf + spi_rt_interface.spi_uart1_recv_read_index, spi_rt_interface.uart1_recv_count);
                    res = spi_rt_interface.uart1_recv_count;
                    memset(spi_rt_interface.spi_uart1_recv_shared_mem_buf + spi_rt_interface.spi_uart1_recv_read_index, 0, spi_rt_interface.uart1_recv_count);
                    spi_rt_interface.uart1_recv_count = 0;
                    spi_rt_interface.spi_uart1_recv_read_index = 0;
                    spi_rt_interface.spi_uart1_recv_write_index = 0;
                    break;
                }
            }
            break;
        }
        case UART2:
        {
            semopbuf.sem_num = 5;
            sem_num = 5;
            PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            
            // 判断接收缓冲区是否有数据可读
            if ((res = spi_select(uart, &tv)) < 0)
            {
                PERROR("spi_select failed!\n");
                return res;
            }
            
            // 信号量减一
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }
            
            if ((res = read_spi_rt_para(sem_num)) < 0)
            {
                PERROR("read_spi_rt_para failed!\n");
                break;
            }
            PRINT("spi_rt_interface.uart2_recv_count = %d, spi_rt_interface.spi_uart2_recv_read_index = %d, spi_rt_interface.spi_uart2_recv_write_index = %d\n", 
                spi_rt_interface.uart2_recv_count, spi_rt_interface.spi_uart2_recv_read_index, spi_rt_interface.spi_uart2_recv_write_index);
            if (spi_rt_interface.uart2_recv_count > 0)
            {
                if ((spi_rt_interface.spi_uart2_recv_shared_mem_id = shmget(spi_rt_interface.spi_uart2_recv_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
                {
                    res = SHMGET_ERR;
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "shmget failed!", res);
                    break;
                }
                
                if ((spi_rt_interface.spi_uart2_recv_shared_mem_buf = shmat(spi_rt_interface.spi_uart2_recv_shared_mem_id, NULL, 0)) < 0)
                {
                    res = SHMGET_ERR;
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "shmget failed!", res);
                    break;
                }
                release_flag = 2;
                
                if (len == spi_rt_interface.uart2_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_uart2_recv_shared_mem_buf + spi_rt_interface.spi_uart2_recv_read_index, len);
                    memset(spi_rt_interface.spi_uart2_recv_shared_mem_buf + spi_rt_interface.spi_uart2_recv_read_index, 0, spi_rt_interface.uart2_recv_count);
                    spi_rt_interface.uart2_recv_count = 0;
                    spi_rt_interface.spi_uart2_recv_read_index = 0;
                    spi_rt_interface.spi_uart2_recv_write_index = 0;
                    res = len;
                    
                    PRINT("spi_rt_interface.uart2_recv_count = %d, spi_rt_interface.spi_uart2_recv_read_index = %d, spi_rt_interface.spi_uart2_recv_write_index = %d\n", 
                        spi_rt_interface.uart2_recv_count, spi_rt_interface.spi_uart2_recv_read_index, spi_rt_interface.spi_uart2_recv_write_index);
                    break;
                }
                else if (len < spi_rt_interface.uart2_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_uart2_recv_shared_mem_buf + spi_rt_interface.spi_uart2_recv_read_index, len);
                    spi_rt_interface.uart2_recv_count -= len;
                    memset(spi_rt_interface.spi_uart2_recv_shared_mem_buf + spi_rt_interface.spi_uart2_recv_read_index, 0, len);
                    spi_rt_interface.spi_uart2_recv_read_index += len;
                    res = len;
                    break;
                }
                else if (len > spi_rt_interface.uart2_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_uart2_recv_shared_mem_buf + spi_rt_interface.spi_uart2_recv_read_index, spi_rt_interface.uart2_recv_count);
                    res = spi_rt_interface.uart2_recv_count;
                    memset(spi_rt_interface.spi_uart2_recv_shared_mem_buf + spi_rt_interface.spi_uart2_recv_read_index, 0, spi_rt_interface.uart2_recv_count);
                    spi_rt_interface.uart2_recv_count = 0;
                    spi_rt_interface.spi_uart2_recv_read_index = 0;
                    spi_rt_interface.spi_uart2_recv_write_index = 0;
                    break;
                }
            }
            break;
        }   
        case UART3:
        {
            semopbuf.sem_num = 6;
            sem_num = 6;
            PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            
            // 判断接收缓冲区是否有数据可读
            if ((res = spi_select(uart, &tv)) < 0)
            {
                PERROR("spi_select failed!\n");
                return res;
            }
            
            // 信号量减一
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }
            
            if ((res = read_spi_rt_para(sem_num)) < 0)
            {
                PERROR("read_spi_rt_para failed!\n");
                break;
            }
            PRINT("spi_rt_interface.uart3_recv_count = %d\n", spi_rt_interface.uart3_recv_count);
            
            if (spi_rt_interface.uart3_recv_count > 0)
            {
                if ((spi_rt_interface.spi_uart3_recv_shared_mem_id = shmget(spi_rt_interface.spi_uart3_recv_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
                {
                    PERROR("shmget failed!");
                    res = SHMGET_ERR;
                    break;
                }
                
                if ((spi_rt_interface.spi_uart3_recv_shared_mem_buf = shmat(spi_rt_interface.spi_uart3_recv_shared_mem_id, NULL, 0)) < 0)
                {
                    PERROR("shmget failed!");
                    res = SHMGET_ERR;
                    break;
                }
                release_flag = 3;
    
                 if (len == spi_rt_interface.uart3_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_uart3_recv_shared_mem_buf + spi_rt_interface.spi_uart3_recv_read_index, len);
                    memset(spi_rt_interface.spi_uart3_recv_shared_mem_buf + spi_rt_interface.spi_uart3_recv_read_index, 0, spi_rt_interface.uart3_recv_count);
                    spi_rt_interface.uart3_recv_count = 0;
                    spi_rt_interface.spi_uart3_recv_read_index = 0;
                    spi_rt_interface.spi_uart3_recv_write_index = 0;
                    res = len;
                    break;
                }
                else if (len < spi_rt_interface.uart3_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_uart3_recv_shared_mem_buf + spi_rt_interface.spi_uart3_recv_read_index, len);
                    spi_rt_interface.uart3_recv_count -= len;
                    memset(spi_rt_interface.spi_uart3_recv_shared_mem_buf + spi_rt_interface.spi_uart3_recv_read_index, 0, len);
                    spi_rt_interface.spi_uart3_recv_read_index += len;
                    res = len;
                    break;
                }
                else if (len > spi_rt_interface.uart3_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_uart3_recv_shared_mem_buf + spi_rt_interface.spi_uart3_recv_read_index, spi_rt_interface.uart3_recv_count);
                    res = spi_rt_interface.uart3_recv_count;
                    memset(spi_rt_interface.spi_uart3_recv_shared_mem_buf + spi_rt_interface.spi_uart3_recv_read_index, 0, spi_rt_interface.uart3_recv_count);
                    spi_rt_interface.uart3_recv_count = 0;
                    spi_rt_interface.spi_uart3_recv_read_index = 0;
                    spi_rt_interface.spi_uart3_recv_write_index = 0;
                    break;
                }
            }
            break;
        }
        case EXPAND:
        {
            semopbuf.sem_num = 7;
            sem_num = 7;
            //PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            
            // 判断接收缓冲区是否有数据可读
            if ((res = spi_select(uart, &tv)) < 0)
            {
                PERROR("spi_select failed!\n");
                return res;
            }
            // 信号量减一
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }
            
            if ((res = read_spi_rt_para(sem_num)) < 0)
            {
                PERROR("read_spi_rt_para failed!\n");
                break;
            }
            //PRINT("spi_rt_interface.uart1_recv_count = %d\n", spi_rt_interface.uart1_recv_count);
            
            if (spi_rt_interface.expand_recv_count > 0)
            {
                if ((spi_rt_interface.spi_expand_recv_shared_mem_id = shmget(spi_rt_interface.spi_expand_recv_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
                {
                    res = SHMGET_ERR;
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "shmget failed!", res);
                    break;
                }
                if ((spi_rt_interface.spi_expand_recv_shared_mem_buf = shmat(spi_rt_interface.spi_expand_recv_shared_mem_id, NULL, 0)) < 0)
                {
                    res = SHMGET_ERR;
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "shmget failed!", res);
                    break;
                }
                release_flag = 4;
                //PRINT_BUF_BY_HEX(spi_rt_interface.spi_expand_recv_shared_mem_buf, NULL, spi_rt_interface.expand_recv_count, __FILE__, __FUNCTION__, __LINE__);
                //PRINT("len = %d spi_rt_interface.expand_recv_count = %d\n", len, spi_rt_interface.expand_recv_count);
                if (len == spi_rt_interface.expand_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_expand_recv_shared_mem_buf + spi_rt_interface.spi_expand_recv_read_index, len);
                    memset(spi_rt_interface.spi_expand_recv_shared_mem_buf + spi_rt_interface.spi_expand_recv_read_index, 0, spi_rt_interface.expand_recv_count);
                    spi_rt_interface.expand_recv_count = 0;
                    spi_rt_interface.spi_expand_recv_read_index = 0;
                    spi_rt_interface.spi_expand_recv_write_index = 0;
                    res = len;
                    break;
                }
                else if (len < spi_rt_interface.expand_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_expand_recv_shared_mem_buf + spi_rt_interface.spi_expand_recv_read_index, len);
                    spi_rt_interface.expand_recv_count -= len;
                    memset(spi_rt_interface.spi_expand_recv_shared_mem_buf + spi_rt_interface.spi_expand_recv_read_index, 0, len);
                    spi_rt_interface.spi_expand_recv_read_index += len;
                    res = len;
                    break;
                }
                else if (len > spi_rt_interface.expand_recv_count)
                {
                    memcpy(data, spi_rt_interface.spi_expand_recv_shared_mem_buf + spi_rt_interface.spi_expand_recv_read_index, spi_rt_interface.expand_recv_count);
                    res = spi_rt_interface.expand_recv_count;
                    memset(spi_rt_interface.spi_expand_recv_shared_mem_buf + spi_rt_interface.spi_expand_recv_read_index, 0, spi_rt_interface.expand_recv_count);
                    spi_rt_interface.expand_recv_count = 0;
                    spi_rt_interface.spi_expand_recv_read_index = 0;
                    spi_rt_interface.spi_expand_recv_write_index = 0;
                    break;
                }
            }
            break;
        }
        default:
        {
            PRINT("option does not mismatch!");
            res = MISMATCH_ERR;
            break;
        }
    }
    
    if (res >= 0)
    {
        if ((ret = write_spi_rt_para(sem_num)) < 0)
        {
            PERROR("write_spi_rt_para failed!\n");
            res = ret;
        }
    }
    
    switch (release_flag)
    {
        case 4:
        {
            if (shmdt(spi_rt_interface.spi_expand_recv_shared_mem_buf) < 0)
            {
                PERROR("shmdt failed!\n");
                res = SHMDT_ERR;
            }
            break;
        }
        case 3:
        {
            if (shmdt(spi_rt_interface.spi_uart3_recv_shared_mem_buf) < 0)
            {
                PERROR("shmdt failed!\n");
                res = SHMDT_ERR;
            }
            break;
        }
        case 2:
        {
            if (shmdt(spi_rt_interface.spi_uart2_recv_shared_mem_buf) < 0)
            {
                PERROR("shmdt failed!\n");
                res = SHMDT_ERR;
            }
            break;
        }
        case 1:
        {
            if (shmdt(spi_rt_interface.spi_uart1_recv_shared_mem_buf) < 0)
            {
                PERROR("shmdt failed!\n");
                res = SHMDT_ERR;
            }
            break;
        }
        default:
            break;
    }
    
    semopbuf.sem_op = 1;
    semopbuf.sem_num = sem_num;
    //PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
    {
        PERROR("semop failed!\n");
        return SEMOP_ERR;
    }
     
    // 当res小于0时，说明以上处理出现错误，大于0时，此时res为要返回的数据长度
    //PRINT("res = %d\n", res);
    return res;
}
#endif

/**
 * spi 发送数据(STUB应用程序调用接口)
 */
int spi_send(struct s_spi_rt_cmd *spi_rt_cmd)
{
    int res = 0, ret = 0;
    int i = 0, j = 0;
    int send_count = 0;           // 实际串口数据发送长度
    char *send_buf = NULL;        // 串口数据发送buf
    char *data = NULL; 
    char uart_no = 0;             // 串口标识
    int release_flag = 0;        // 资源释放标识
    struct sembuf semopbuf;      // 信号量结构
    unsigned char sem_num = 0;   // 信号量序号
    
    fd_set fdset;
    struct timeval tv;
    memset(&tv, 0, sizeof(struct timeval));
    
    if (spi_rt_cmd == NULL)
    {
        PRINT("spi_rt_cmd is NULL!\n");
        return NULL_ERR;
    }
    if (spi_rt_cmd->buf_len == 0)
    {
        PRINT("send len is zero!\n");
        return 0;
    }
    else if (spi_rt_cmd->buf_len > SHARED_MEM_SIZE)
    {
        spi_rt_cmd->buf_len = SHARED_MEM_SIZE;
    }
    
    memset(&semopbuf, 0, sizeof(struct sembuf));
    semopbuf.sem_op = -1;
    semopbuf.sem_flg = SEM_UNDO;
    
    // 根据 spi_rt_cmd->uart，赋值串口标识，把共享内存（即发送缓冲区）地址赋值给data
    switch (spi_rt_cmd->uart)
    {
        case UART1:
        {
            semopbuf.sem_num = 0;
            sem_num = 0;
            PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }
            
            if ((res = read_spi_rt_para(sem_num)) < 0)
            {
                PERROR("read_spi_rt_para failed!\n");
                break;
            }
            PRINT("spi_rt_interface.uart1_send_buf_len = %d\n", spi_rt_interface.uart1_send_buf_len);
            
            if (spi_rt_interface.uart1_send_buf_len <= 0)
            {
                res = DATA_ERR;
                goto EXIT;
            }
            
            uart_no = 0x10;
            data = spi_rt_interface.spi_uart1_send_shared_mem_buf;
            break;
        }
        case UART2:
        {
            semopbuf.sem_num = 1;
            sem_num = 1;
            PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }
            
            if ((res = read_spi_rt_para(sem_num)) < 0)
            {
                PERROR("read_spi_rt_para failed!\n");
                break;
            }
            PRINT("spi_rt_interface.uart2_send_buf_len = %d\n", spi_rt_interface.uart2_send_buf_len);
                         
            if (spi_rt_interface.uart2_send_buf_len <= 0)
            {
                res = DATA_ERR;
                goto EXIT;
            }
            
            uart_no = 0x20;
            data = spi_rt_interface.spi_uart2_send_shared_mem_buf;
            break;
        } 
        case UART3:
        {
            semopbuf.sem_num = 2;
            sem_num = 2;
            PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }
            
            if ((res = read_spi_rt_para(sem_num)) < 0)
            {
                PERROR("read_spi_rt_para failed!\n");
                break;
            }
            PRINT("spi_rt_interface.uart3_send_buf_len = %d\n", spi_rt_interface.uart3_send_buf_len);
                                  
            if (spi_rt_interface.uart3_send_buf_len <= 0)
            {
                res = DATA_ERR;
                goto EXIT;
            }
            
            uart_no = 0x30;
            data = spi_rt_interface.spi_uart3_send_shared_mem_buf;
            break;
        }
        case EXPAND:
        {
            semopbuf.sem_num = 3;
            sem_num = 3;
            PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
            if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
            {
                PERROR("semop failed!\n");
                return SEMOP_ERR;
            }
            
            if ((res = read_spi_rt_para(sem_num)) < 0)
            {
                PERROR("read_spi_rt_para failed!\n");
                break;
            }
            PRINT("spi_rt_interface.expand_send_buf_len = %d\n", spi_rt_interface.expand_send_buf_len);
            
            if (spi_rt_interface.expand_send_buf_len <= 0)
            {
                res = DATA_ERR;
                goto EXIT;
            }
            
            uart_no = 0x40;
            data = spi_rt_interface.spi_expand_send_shared_mem_buf;
            PRINT("spi_rt_cmd->buf_len = %d\n", spi_rt_cmd->buf_len);
            PRINT("data = %X, spi_rt_interface.spi_expand_send_shared_mem_buf = %X\n", data, spi_rt_interface.spi_expand_send_shared_mem_buf);
            break;
        }
        default:
        {
            PRINT("option does not mismatch!\n");
            return MISMATCH_ERR;
        }
    }
    
    // 申请需要发送buf
    if ((send_buf = malloc(spi_rt_cmd->buf_len + 6 + 1)) == NULL) 
    {
        PERROR("malloc failed!\n");
        return MALLOC_ERR;
    }
    memset(send_buf, 0, spi_rt_cmd->buf_len + 6 + 1);
    // 发送缓冲区初始化
    send_buf[0] = 0xA5;
    send_buf[1] = 0x5A;
    send_buf[2] = (unsigned char)((spi_rt_cmd->buf_len + 2) >> 8);
    send_buf[3] = (unsigned char)(spi_rt_cmd->buf_len + 2);
    send_buf[4] = uart_no;
    
    memcpy(send_buf + 5, data, spi_rt_cmd->buf_len);
    
    send_buf[spi_rt_cmd->buf_len + 5] = common_tools.get_checkbit(send_buf, NULL, 2, spi_rt_cmd->buf_len + 3, XOR, 1);
    
    // 发送数据
    PRINT("spi_rt_interface.spi_fd = %d\n", spi_rt_interface.spi_fd);
    PRINT("spi_rt_cmd->buf_len + 6 = %d\n", spi_rt_cmd->buf_len + 6);
    if ((send_count = write(spi_rt_interface.spi_fd, send_buf, spi_rt_cmd->buf_len + 6)) < 0)
    {
        PERROR("write failed!\n");
        res = WRITE_ERR;
        goto EXIT;
    }
    res = (send_count - 6);
    PRINT_BUF_BY_HEX(send_buf, NULL, spi_rt_cmd->buf_len + 6, __FILE__, __FUNCTION__, __LINE__);
    PRINT("send_count = %d spi_rt_cmd->uart = %d\n", send_count, spi_rt_cmd->uart);
    
    switch (spi_rt_cmd->uart)
    {
        case UART1:
        {
            spi_rt_interface.uart1_send_buf_len -= spi_rt_cmd->buf_len;
            PRINT("spi_rt_interface.uart1_send_buf_len = %d spi_rt_cmd->buf_len = %d\n", spi_rt_interface.uart1_send_buf_len, spi_rt_cmd->buf_len);
            break;
        }
        case UART2:
        {
            spi_rt_interface.uart2_send_buf_len -= spi_rt_cmd->buf_len;
            PRINT("spi_rt_interface.uart2_send_buf_len = %d spi_rt_cmd->buf_len = %d\n", spi_rt_interface.uart2_send_buf_len, spi_rt_cmd->buf_len);
            break;
        }
        case UART3:
        {
            spi_rt_interface.uart3_send_buf_len -= spi_rt_cmd->buf_len;
            PRINT("spi_rt_interface.uart3_send_buf_len = %d spi_rt_cmd->buf_len = %d\n", spi_rt_interface.uart3_send_buf_len, spi_rt_cmd->buf_len);
            break;
        }
        case EXPAND:
        {
            spi_rt_interface.expand_send_buf_len -= spi_rt_cmd->buf_len;
            PRINT("spi_rt_interface.expand_send_buf_len = %d spi_rt_cmd->buf_len = %d\n", spi_rt_interface.expand_send_buf_len, spi_rt_cmd->buf_len);
            break;
        }
    }

EXIT:
    PRINT("spi_rt_cmd->uart = %d res = %d\n", spi_rt_cmd->uart, res);
    if (res > 0)
    {
        if ((ret = write_spi_rt_para(sem_num)) < 0)
        {
            PERROR("write_spi_rt_para failed!\n");
            res = ret;
        }
    }
    
    if (send_buf != NULL)
    {
        free(send_buf);
        send_buf = NULL;
    }
    
    semopbuf.sem_num = sem_num;
    semopbuf.sem_op = 1;
    PRINT("semopbuf.sem_num = %d semopbuf.sem_op = %d\n", semopbuf.sem_num, semopbuf.sem_op);
    if (semop(spi_rt_interface.spi_uart_sem_id, &semopbuf, 1) < 0)
    {
        PERROR("semop failed!\n");
        return SEMOP_ERR;
    }
    
    PRINT("res = %d\n", res);
    return res;
}

/**
 * 发送spi收发命令包
 */
int send_spi_rt_cmd_msg(int fd, struct s_spi_rt_cmd *spi_rt_cmd, struct timeval *tv)
{
    PRINT("send_spi_rt_cmd_msg entry!\n");
    int res = 0;
    unsigned short len = 0;
    char send_buf[8] = {0};
    
    if (spi_rt_cmd == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    send_buf[0] = 0x5A;
    send_buf[1] = 0xA5;
    send_buf[2] = spi_rt_cmd->cmd;
    send_buf[3] = spi_rt_cmd->uart;
    send_buf[4] = (unsigned char)spi_rt_cmd->buf_len;
    send_buf[5] = (unsigned char)(spi_rt_cmd->buf_len >> 8);
    
    send_buf[6] = common_tools.get_checkbit(send_buf, NULL, 2, 4, XOR, 1);
    
    len = 7;
    PRINT("before send_data!\n");
    // 发送
    if ((res = common_tools.send_data(fd, send_buf, NULL, len, tv)) < 0)
    {
        PERROR("send_data failed!\n");   
        return res;
    }
    PRINT_BUF_BY_HEX(send_buf, NULL, len, __FILE__, __FUNCTION__, __LINE__);
    PRINT("________________\n");
    return 0;
}

/**
 * 接收spi收发命令包
 */
int recv_spi_rt_cmd_msg(int fd, struct s_spi_rt_cmd *spi_rt_cmd, struct timeval *tv)
{
    PRINT("recv_spi_rt_cmd_msg entry!\n");
    int res = 0;
    char checkbit = 0;
    unsigned char buf_len[2] = {0};
    
    if (spi_rt_cmd != NULL)
    {
        spi_rt_cmd->head[0] = 0x5A;
        spi_rt_cmd->head[1] = 0xA5; 
        // 接收数据头
        if ((res = common_tools.recv_data_head(fd, spi_rt_cmd->head, sizeof(spi_rt_cmd->head), tv)) < 0)
        {
            PERROR("recv_data_head failed!\n");         
            return res;
        }
        
        // 接收命令字
        if ((res = common_tools.recv_one_byte(fd, &spi_rt_cmd->cmd, tv)) < 0)
        {
            PERROR("recv_one_byte failed!\n");      
            return res;
        }
        
        // 接收串口编号
        if ((res = common_tools.recv_one_byte(fd, &spi_rt_cmd->uart, tv)) < 0)
        {
            PERROR("recv_one_byte failed!\n");         
            return res;
        }   
        
        // 接收串口数据长度
        if ((res = common_tools.recv_data(fd, buf_len, NULL, sizeof(buf_len), tv)) < 0)
        {
            PERROR("recv_data failed!\n");         
            return res;
        }
        
        // 接收校验位
        if ((res = common_tools.recv_one_byte(fd, &spi_rt_cmd->checkbit, tv)) < 0)
        {
            PERROR("recv_one_byte failed!\n");         
            return res;
        }
        #if PRINT_DATA
        printf("\n"); 
        #endif
        
        spi_rt_cmd->buf_len = buf_len[0];
        spi_rt_cmd->buf_len += (buf_len[1] << 8);
        
        if ((spi_rt_cmd->cmd ^ spi_rt_cmd->uart ^ buf_len[0] ^ buf_len[1]) != spi_rt_cmd->checkbit)
        {
            PERROR("check is different!\n");
            return CHECK_DIFF_ERR;         
        }
    }
    else
    {
        PRINT("data is NULL!\n");
        return NULL_ERR;
    }
    PRINT("data buf recv success!\n");
    return 0;
}
