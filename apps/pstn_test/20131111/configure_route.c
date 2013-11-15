#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <unistd.h>
#include <termios.h>
#include <math.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#define DEBUG 1
#define GPIO_DEV	"/dev/gpio"
#define PPPOE_STATE	"/var/pppoe_status.txt"

#define READONLY_ROOTFS 1       // 5350只读文件系统
#define ONE_BYTE_DELAY_SEC 10    // 接收或者发生一个字节延时 秒
#define ONE_BYTE_DELAY_USEC 0   // 接收或者发生一个字节延时 微秒

#if DEBUG
#define PRINT(format, ...) printf("%s["__FILE__"][%s][%05d] "format"", get_datetime_buf(), __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define PRINT(format, ...)
#endif

// 错误码
enum ERROR_NUM
{
    CHECK_DIFF_ERR = -100,
    FSTAT_ERR,
    GETATTR_ERR,
    IOCTL_ERR,
    LENGTH_ERR,
    MALLOC_ERR,
    NULL_ERR,
    OPEN_ERR,
    READ_ERR,
    SETATTR_ERR,
    SELECT_ERR,
    SELECT_NULL_ERR,
    SYSTEM_ERR,
    WRITE_ERR,
    WAN_STATE_ERR,
};

// 命令结构体
struct s_cmd
{
    char key[64];
    char value[64];
};

// 结构体数组 保存用户可改变的项 在恢复路由器设置的时候可用到
struct s_cmd cmd_list[] = 
{
    {"SSID1",              ""},
    {"WPAPSK1",            ""},
    {"wan_ipaddr",         ""},    
    {"wan_netmask",        ""},
    {"wan_gateway",        ""},
    {"wan_primary_dns",    ""},
    {"wan_secondary_dns",  ""},
    {"wan_dhcp_hn",        ""},
    {"wan_pppoe_user",     ""},
    {"wan_pppoe_pass",     ""},
    {"SSID3",              ""},
    {"WPAPSK3",            ""},
    {"AuthMode",           ""},
    {"macCloneEnabled",    ""},
    {"macCloneMac",        ""},
    {"wanConnectionMode",  ""}
};

char __datetime_buf[128] = {0};

// 获取日期和时间字符串
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

// 按照进程名称获取PID
int get_pid_by_process_name(char *process_name)
{
    if (process_name == NULL)
    {
        PRINT("process_name is NULL!\n");
        return NULL_ERR;
    }
    char *buf = NULL;
    int fd = 0;
    pid_t pid = 0;
    unsigned short buf_len = 0;
    
    buf_len = strlen("ps | grep ") + strlen(process_name) + strlen(" | sed '/grep/'d  > /var/process_info");
    if ((buf = (char *)malloc(buf_len + 1)) == NULL)
    {
        PRINT("malloc failed!\n");
        return MALLOC_ERR;
    }
    memset(buf, 0, buf_len + 1);
    
    sprintf(buf, "ps | grep %s | sed '/grep/'d > /var/process_info", process_name);
    
    if (system(buf) < 0)
    {
        PRINT("system failed!\n");
        free(buf);
        buf = NULL;
        return SYSTEM_ERR;
    }
    memset(buf, 0, buf_len + 1);
    
    if ((fd = open("/var/process_info", O_RDONLY, 0644)) < 0)
    {
        PRINT("open failed!\n");
        return OPEN_ERR;
    }
    
    if (read(fd, buf, 6) != 6)
    {
        PRINT("read failed!\n");
        free(buf);
        buf = NULL;
        close(fd);
        return READ_ERR;
    }
    
    PRINT("buf = %s\n", buf);
    pid = atoi(buf);
    
    free(buf);
    buf = NULL;
    close(fd);
    system("rm -rf /var/process_info");
    PRINT("pid = %d\n", pid);
    return pid;
}
// 使设置生效
int config_route_take_effect()
{
    int res = 0;
    char buf[20] = {0};
    if ((res = get_pid_by_process_name("nvram_daemon")) < 0)
    {
        PRINT("get_pid_by_process_name failed!\n");
        return res;
    }
    PRINT("pid = %d\n", (pid_t)res);
    sprintf(buf, "kill -9 %d", (pid_t)res);
    PRINT("%s\n", buf);
    if (system(buf) < 0)
    {
        PRINT("system failed!\n");
        return SYSTEM_ERR;
    }
    
    if ((res = get_pid_by_process_name("goahead")) < 0)
    {
        PRINT("get_pid_by_process_name failed!\n");
        return res;
    }
    memset(buf, 0, sizeof(buf));
    PRINT("pid = %d\n", (pid_t)res);
    sprintf(buf, "kill -9 %d", (pid_t)res);
    PRINT("%s\n", buf);
    if (system(buf) < 0)
    {
        PRINT("system failed!\n");
        return SYSTEM_ERR;
    }
    
    if (system("nvram_daemon &") < 0)
    {
        PRINT("system failed!\n");
        return SYSTEM_ERR;
    }
    
    if (system("goahead &") < 0)
    {
        PRINT("system failed!\n");
        return SYSTEM_ERR;
    }
    return 0;
}


// 异或校验计算
char get_xor_check(char *pack, unsigned short pack_len)
{
    unsigned short i = 0;;
    char check = 0;
    for (i = 0; i < pack_len; i++)
    {
        check ^= pack[i];
    }
    return check;
}

// 对数据加引号
int add_quotation_marks(char *data)
{
    char *data_tmp = NULL;
    if (data == NULL)   
    {
        PRINT("%s\n", "data is NULL!");
        return NULL_ERR;
    }
    
    if ((data_tmp = (char *)malloc(strlen(data) + 1)) == NULL)
    {
        PRINT("%s\n", "get_config failed!");        
        return MALLOC_ERR;
    }    
    memset(data_tmp, 0, strlen(data) + 1);    
    memcpy(data_tmp, data, strlen(data));
    memset(data, 0, strlen(data));
    
    if ((data_tmp[0] == '\"') && (data_tmp[strlen(data_tmp) - 1] == '\"'))
    {
        return 0;        
    }
    data[0] = '\"';
    memcpy(data + 1, data_tmp, strlen(data_tmp));
    data[strlen(data_tmp) + 1] = '\"';    
    return 0;
}

// 16进制形式打印一个buf
void print_buf(char *buf, unsigned short buf_len)
{
    PRINT("[buf_len = %d][", buf_len);
    int i = 0;
    for (i = 0; i < buf_len; i++)
    {
        printf("%02X ", (unsigned char)buf[i]);
    }
    printf("]\n");  
}

// 接收一个字节
int recv_one_byte(int fd, char *data, unsigned short wait_time_sec, unsigned short wait_time_usec)
{
    int i = 0;
    int res = 0;
    fd_set fdset;    
    struct timeval tv;    
    memset(&tv, 0, sizeof(struct timeval));
    
    // 不阻塞读
    fcntl(fd, F_SETFL, FNDELAY);
    tv.tv_sec = wait_time_sec;
    tv.tv_usec = wait_time_usec;
    FD_ZERO(&fdset);
    FD_SET(fd, &fdset); 
    
    switch (select(fd + 1, &fdset, NULL, NULL, &tv))
    {
        case -1:
        {
            PRINT("[%s]\n", "select failed!"); 
            return SELECT_ERR;
        }
        case 0:
        {
            PRINT("[%s]\n", "select no data recv!"); 
            return SELECT_NULL_ERR;
        }
        default:
        {
            if (FD_ISSET(fd, &fdset) > 0)
            {
                if (read(fd, data, sizeof(char)) != sizeof(char))
                {
                    PRINT("[%s]\n", "read failed!");           
                    return READ_ERR;
                }
                #if DEBUG
                printf("%02X ", (unsigned char)data[0]);
                #endif
            }
            else
            {
                PRINT("[%s]\n", "select no data recv!");
                return SELECT_NULL_ERR;
            }
        }        
    }
    return 0;
}

// 接收一个buf
int recv_data(int fd, char *data, unsigned int data_len, unsigned short wait_time_sec, unsigned short wait_time_usec)
{
    int i = 0;
    int res = 0;
    if (data == NULL)
    {
        PRINT("[%s]\n", "data is NULL!");
        return NULL_ERR;
    }
    if (data_len == 0)
    {
        PRINT("[%s]\n", "data_len is zero!");
        return LENGTH_ERR;
    }
    
    for (i = 0; i < data_len; i++)
    {        
        if ((res = recv_one_byte(fd, &data[i], wait_time_sec, wait_time_usec)) != 0)
        {
            PRINT("[%s]\n", "recv_one_byte failed!");
            return res;
        }
    }
    return 0;
}

 
int get_wan_state()
{
	int fd = 0, value = 0;
	
	if ((fd = open(GPIO_DEV, O_RDONLY)) < 0) 
	{
		PRINT("[%s]\n", "open failed!");
		return OPEN_ERR;
	}
	
	if (ioctl(fd, 0x62, &value) < 0) 
	{
		PRINT("[%s]\n", "ioctl failed!");
		close(fd);
		return IOCTL_ERR;
	}
	close(fd);
	if (value == 0)
	{
	    return WAN_STATE_ERR;
	}
	return 0;
}

// 获取PPPOE状态
int get_pppoe_state()
{
    int fd = 0, value = 0;
	char buf[5] = {0};
	if ((fd = open(PPPOE_STATE, O_RDONLY)) < 0) 
	{
		PRINT("[%s]\n", "open failed!");
		return OPEN_ERR;
	}
	if (read(fd, buf, 4) != 4)	    
	{
	    PRINT("[%s]\n", "open failed!");
	    close(fd);
		return READ_ERR;
	}
	close(fd);
	value = atoi(buf);
	PRINT("buf = %s, value = %d\n", buf, value);
	return value;
}
// 数据头接收处理
int recv_data_head(int fd, char *head, unsigned char head_len)
{
    int i = 0, j = 0;
    int res = 0;
    char *data_head = NULL;
    
    if ((head == NULL) || (head_len == 0))
    {
        PRINT("[%s]\n", "head is NULL or head_len is zero!");
        return NULL_ERR;
    }
    
    if ((data_head =(char *)malloc(head_len)) == NULL)
    {
        PRINT("[%s]\n", "malloc failed!");
        return MALLOC_ERR;
    }
    memset((void *)data_head, 0, head_len);
    
    for(i = 0; i < head_len;)
    {
        if ((res = recv_one_byte(fd, &data_head[i], ONE_BYTE_DELAY_SEC, ONE_BYTE_DELAY_USEC)) == 0)      
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
                    //PRINT("[%s]\n", "data head recv success!");
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
            PRINT("[%s]\n", "recv_one_byte failed!");
            return res;
        }
    }
    free(data_head);    
    data_head = NULL;
    return 0;
}

// 用于设置串口的属性
int serial_init(const int fd)
{
    int res = 0;
	struct termios old_opt, new_opt;
	memset(&old_opt, 0, sizeof(struct termios));
	memset(&new_opt, 0, sizeof(struct termios));

	if (tcgetattr(fd, &old_opt) < 0)
    {  
        PRINT("[%s]\n", "tcgetattr failed!");
        return GETATTR_ERR;
    }
    
    // 配置
	new_opt.c_iflag = IGNPAR;

	new_opt.c_cflag = B57600;
  	new_opt.c_cflag |= CS8;                //数据位为8  
  	new_opt.c_cflag |= (CREAD | CLOCAL);   //
  	new_opt.c_cflag &= ~PARENB;            //无校验   
  	new_opt.c_cflag &= ~CSTOPB;            //停止位是一位
  	new_opt.c_cc[VMIN] = 1;                // 当接收到一个字节数据就读取
  	new_opt.c_cc[VTIME] = 0;               // 不启用计时器  
    
    
    tcflush(fd, TCIOFLUSH);
  	if (tcsetattr(fd, TCSANOW, &new_opt) < 0)
    {
        PRINT("[%s]\n", "tcsetattr failed!");
        return SETATTR_ERR;
    }
    return 0;
}

// 接收6410发送的数据
int recv_msg_from_6410(int fd, char **data)
{
    int i = 0;
    int res = 0;
    unsigned char data_head[2] = {0x5A, 0xA5};
    unsigned short data_len = 0;
    unsigned char len_tmp = 0;
    char check = 0;
    
    PRINT("___from 6410 start___\n");
    // 接收数据头
    if ((res = recv_data_head(fd, data_head, sizeof(data_head))) < 0)
    {
        PRINT("[%s]\n", "serial_init failed!");
        return res;
    }
    
    // 接收数据长度
    /*
    if ((res = recv_one_byte(fd, &len_tmp, ONE_BYTE_DELAY_SEC, ONE_BYTE_DELAY_USEC)) < 0)
    {
        PRINT("[%s]\n", "recv_one_byte failed!");
        return res;
    }
    data_len = len_tmp;
    if ((res = recv_one_byte(fd, &len_tmp, ONE_BYTE_DELAY_SEC, ONE_BYTE_DELAY_USEC)) < 0)
    {
        PRINT("[%s]\n", "recv_one_byte failed!");
        return res;
    }
    data_len += (len_tmp << 8);
    */
    if ((res = recv_data(fd, (char *)&data_len, sizeof(data_len), ONE_BYTE_DELAY_SEC, ONE_BYTE_DELAY_USEC)) < 0)
    {
        PRINT("[%s]\n", "recv_data failed!");
        return res;
    }
        
    // 动态申请内存
    if ((*data = (char *)malloc(data_len + 1)) == NULL)
    {
        PRINT("[%s]\n", "malloc failed!");
        return MALLOC_ERR;
    }
    memset(*data, 0, data_len + 1);
    
    // 接收有效数据
    if ((res = recv_data(fd, *data, data_len, ONE_BYTE_DELAY_SEC, ONE_BYTE_DELAY_USEC)) < 0)
    {
        PRINT("[%s]\n", "recv_data failed!");
        return res;
    }
    
    // 接收校验位
    if ((res = recv_one_byte(fd, &check, ONE_BYTE_DELAY_SEC, ONE_BYTE_DELAY_USEC)) < 0)
    {
        PRINT("[%s]\n", "recv_one_byte failed!");
        return res;
    }
    
    // 校验位对比
    if (check != get_xor_check(*data, data_len))
    {
        PRINT("[%s]\n", "check is different!");
        return CHECK_DIFF_ERR;
    }
    printf("\n");
    PRINT("___from 6410 end___\n");  
    PRINT("[%s]\n", *data);
    return 0;
}

// 发送数据到6410
int send_msg_to_6410(int fd, char *data, unsigned short data_len)
{
    char *send_buf = NULL;
    unsigned short buf_len = 0;
    int i = 0;
    PRINT("data_len = %d %04X\n", data_len, data_len);
    buf_len = 2 + 2 + data_len + 1;
    
    if ((send_buf = (char *)malloc(buf_len)) == NULL)
    {  
        PRINT("[%s]\n", "malloc failed!");      
        return MALLOC_ERR;
    }
    
    // 打包
    send_buf[0] = 0x5A;
    send_buf[1] = 0xA5;
    if (data_len == 1)
    {
        memcpy(send_buf + 2, &data_len, sizeof(data_len));
        send_buf[4] = data[0];
        send_buf[5] = data[0];
    }
    else
    {
        memcpy(send_buf + 2, &data_len, sizeof(data_len));
        memcpy(send_buf + 4, data, data_len);
        send_buf[buf_len - 1] = get_xor_check(data, data_len);
    }
    
    // 发送
    for (i = 0; i < buf_len; i++)
    {
        if (write(fd, &send_buf[i], sizeof(send_buf[i])) != sizeof(send_buf[i]))
        {
            PRINT("[%s]\n", "write failed!");                 
            return WRITE_ERR;
        }
        usleep(30);
    }
    
    print_buf(send_buf, buf_len);
    free(send_buf);
    send_buf = NULL;
    return 0;
}

// 读取路由器当前项的值
int read_route_state_option(char **buf)
{
    int fd = 0;
    int i = 0, j = 0;
    struct stat file_stat;
    
    if ((fd = open("/var/route_state_option", O_RDONLY, 0644)) < 0)
    {
        PRINT("[%s]\n", "open failed!"); 
        return OPEN_ERR;
    }
    
    if (fstat(fd, &file_stat) < 0)
    {
        PRINT("[%s]\n", "fstat failed!"); 
        close(fd);
        return FSTAT_ERR;
    }
    PRINT("file_stat.st_size = %d\n", file_stat.st_size);
    if ((*buf = (char *)malloc(file_stat.st_size + 2)) == NULL)
    {
        PRINT("[%s]\n", "malloc failed!"); 
        close(fd);
        return MALLOC_ERR;
    }
    memset(*buf, 0, file_stat.st_size + 2);
    
    if ((read(fd, *buf + 1, file_stat.st_size)) != file_stat.st_size)
    {
        PRINT("[%s]\n", "malloc failed!");
        close(fd);
        return READ_ERR;
    }
    
    (*buf)[0] = '\"';
    (*buf)[file_stat.st_size] = '\"';
    (*buf)[file_stat.st_size + 1] = '\0';
    
    PRINT("[*buf = %s]\n", *buf);
    close(fd);
    system("rm -rf /var/route_state_option");
    return 0;
}
// 对路由器状态的数据打包
int current_route_state_msg_pack(struct s_cmd *cmd_list, int cmd_list_len, char **data)
{
    int i = 0;
    unsigned short data_len = 0;
    if (cmd_list == NULL)
    {
        PRINT("[%s]\n", "cmd_list is NULL!"); 
        return NULL_ERR;
    }
    if (cmd_list_len == 0)
    {
        PRINT("[%s]\n", "data_len is zero!");
        return LENGTH_ERR;
    }
    if ((*data = (char *)malloc(1)) == NULL)
    {
        PRINT("[%s]\n", "malloc failed!");
        return MALLOC_ERR;
    }
    memset(*data, 0, 1);
    data_len = 1;
    
    for (i = 0; i < cmd_list_len; i++)
    {        
        if ((*data = (char *)realloc(*data, data_len + strlen(cmd_list[i].value) + 3)) == NULL)
        {
            PRINT("[%s]\n", "malloc failed!");
            free(*data);
            *data = NULL;
            return MALLOC_ERR;
        }
        memset(*data + data_len, 0, strlen(cmd_list[i].value) + 3);
        add_quotation_marks(cmd_list[i].value);
        memcpy(*data + data_len - 1, cmd_list[i].value, strlen(cmd_list[i].value));                
        data_len += strlen(cmd_list[i].value);
        data_len += 1;
        (*data)[data_len - 2] = ',';
    }
    PRINT("[%s]\n", *data);
    return 0;
}

// 对路由器状态的数据打包
int route_state_msg_unpack(struct s_cmd *cmd_list, int cmd_list_len, char *data)
{
    int i = 0, j = 0;
    unsigned short data_len = 0;
    if ((cmd_list == NULL) || (data == NULL))
    {
        PRINT("[%s]\n", "cmd_list or data is NULL!"); 
        return NULL_ERR;
    }
    if (cmd_list_len == 0)
    {
        PRINT("[%s]\n", "data_len is zero!");
        return LENGTH_ERR;
    }
    
    for (i = 0; i < cmd_list_len; i++)
    {
        while ((cmd_list[i].value[j] = *(data + data_len)) != ',')
        {
            j++;
            data_len++;
        }
        cmd_list[i].value[j] = '\0';
        data_len++;
        j = 0;
    }
    PRINT("[data = %s]\n", data);
    return 0;
}

// 读取当前的路由状态
int read_current_route_state(struct s_cmd *cmd_list, int cmd_list_len)
{
    int fd = 0;
    int i = 0, j = 0;
    struct stat file_stat;
    char *buf = NULL;
    char *tmp = NULL;
    char key_tmp[64] = {0};
    
    if ((fd = open("/var/route_state", O_RDONLY, 0644)) < 0)
    {
        PRINT("[%s]\n", "open failed!"); 
        return OPEN_ERR;
    }
    
    if (fstat(fd, &file_stat) < 0)
    {
        PRINT("[%s]\n", "fstat failed!"); 
        close(fd);
        return FSTAT_ERR;
    }
    PRINT("file_stat.st_size = %d\n", file_stat.st_size);
    if ((buf = (char *)malloc(file_stat.st_size + 1)) == NULL)
    {
        PRINT("[%s]\n", "malloc failed!"); 
        close(fd);
        return MALLOC_ERR;
    }
    memset(buf, 0, file_stat.st_size + 1);
    
    if ((read(fd, buf, file_stat.st_size)) != file_stat.st_size)
    {
        PRINT("[%s]\n", "malloc failed!");
        free(buf);
        buf = NULL;
        close(fd);
        return READ_ERR;
    }
    close(fd);
    system("rm -rf /var/route_state");
    
    for(i = 0; i < cmd_list_len; i++)
    {
        memset(key_tmp, 0, sizeof(key_tmp));        
        sprintf(key_tmp, "%s=", cmd_list[i].key);
        if ((tmp = strstr(buf, key_tmp)) == NULL)
        {
            continue;
        }
        memset(cmd_list[i].value, 0, sizeof(cmd_list[i].value));
        for (j = 0; j < sizeof(cmd_list[i].value); j++)
        {
            if (*(tmp + strlen(cmd_list[i].key) + 1 + j) == '\n')
            {
                break;
            }
            cmd_list[i].value[j] = *(tmp + strlen(cmd_list[i].key) + 1 + j);
        }
        PRINT("%s %s\n", cmd_list[i].key, cmd_list[i].value);
    }
    free(buf);
    buf = NULL;
    return 0;
}

int main(int argc,char **argv)  
{
    int fd = 0;
    int i = 0; 
    int res = 0;
    struct timeval tv;
    fd_set fdset;
    char *data = NULL;
    char *cmd_buf = NULL; 
    unsigned char cmd_len = 80;
    unsigned char cmd_count = 0;
    char data_tmp = 0;
    char buf_tmp[80] = {0};
    unsigned short buf_len = 0;
    
    while((fd = open("/dev/ttyS0", O_RDWR, 0644)) < 0)
    {
        perror("Can't Open /dev/ttyS0");
        sleep(1);
    }
    PRINT("[%s]\n", "Open /dev/ttyS0 success!");
    
    while (serial_init(fd) < 0)
    {
        perror("serial_init failed!");
        sleep(1);
    }
    PRINT("[%s]\n", "serial_init success!");
    
    while(1)
    {
        FD_ZERO(&fdset);
        FD_SET(fd, &fdset);
        tv.tv_sec = 1;
        
        switch(select(fd + 1, &fdset, NULL, NULL, &tv))
        {
            case -1:
            case 0:
            {
                PRINT("[%s]\n", "select error or no data!");
                continue;
            }
            default:
            {
                if (FD_ISSET(fd, &fdset) > 0)
                {
                    if ((res = recv_msg_from_6410(fd, &data)) < 0)
                    {
                        PRINT("[%s]\n", "recv_msg_from_6410 failed!");
                        break;
                    }
                    PRINT("[%s]\n", data);
                    
                    if (strcmp(data, "reboot success?") == 0) // 询问是否重启完毕
                    {
                        PRINT("________________________________________\n"); 
                        free(data);
                        data = NULL;
                        data_tmp = 0x00;
                        if ((res = send_msg_to_6410(fd, &data_tmp, 1)) < 0)
                        {
                            PRINT("[%s]\n", "send_msg_to_6410 failed!");
                        }
                        break;
                    }
                    else if (strcmp(data, "current route state?") == 0) // 询问当前路由器状态
                    {
                        PRINT("________________________________________\n");                    
                        free(data);
                        data = NULL;
                        if ((res = system("ralink_init show 2860 > /var/route_state")) < 0)
                        {
                            PRINT("[%s]\n", "system failed!");
                            break;
                        }
                        
                        // 只读文件系统时，需加入如下操作
                        #if READONLY_ROOTFS
                        if ((res = read_current_route_state(cmd_list, sizeof(cmd_list) / sizeof(struct s_cmd))) < 0)
                        {
                            PRINT("[%s]\n", "read_current_route_state failed!");
                            break;
                        }
                        
                        if ((res = current_route_state_msg_pack(cmd_list, sizeof(cmd_list) / sizeof(struct s_cmd), &data)) < 0)
                        {
                            PRINT("[%s]\n", "current_route_state_msg_pack failed!");
                            break;
                        }
                        PRINT("[%s]\n", data);
                        if ((res = send_msg_to_6410(fd, data, strlen(data))) < 0)
                        {
                            PRINT("[%s]\n", "send_msg_to_6410 failed!");
                            break;
                        }
                        data = NULL;
                        free(data);
                        if ((res = recv_msg_from_6410(fd, &data)) < 0)
                        {
                            PRINT("[%s]\n", "recv_msg_from_6410 failed!");                            
                            break;
                        }
                        free(data);
                        data = NULL;
                        #else
                        data_tmp = 0x00;
                        if ((res = send_msg_to_6410(fd, &data_tmp, 1)) < 0)
                        {
                            PRINT("[%s]\n", "send_msg_to_6410 failed!");
                            break;
                        }
                        #endif
                        PRINT("[%s]\n", "save the current route state success!");
                        break;
                    }
                    else if (strcmp(data, "recovery of router state!") == 0) // 恢复路由器状态
                    {
                        PRINT("________________________________________\n");
                        free(data);
                        data = NULL;
                        // 只读文件系统时，需加入如下操作
                        #if READONLY_ROOTFS
                        data_tmp = 0x00;
                        if ((res = send_msg_to_6410(fd, &data_tmp, 1)) < 0)
                        {
                            PRINT("[%s]\n", "send_msg_to_6410 failed!");
                            break;
                        }
                        
                        if ((res = recv_msg_from_6410(fd, &data)) < 0)
                        {
                            PRINT("[%s]\n", "recv_msg_from_6410 failed!");               
                            break;
                        }
                        
                        if ((res = route_state_msg_unpack(cmd_list, sizeof(cmd_list) / sizeof(struct s_cmd), data)) < 0)
                        {
                            PRINT("[%s]\n", "route_state_msg_unpack failed!");                
                            break;
                        }
                        free(data);                 
                        data = NULL;
                        
                        unsigned short cmd_buf_len = 0;
                        
                        for (i = 0; i < sizeof(cmd_list) / sizeof(struct s_cmd); i++)
                        {
                            cmd_buf_len = strlen("nvram_set 2860 ") + strlen(cmd_list[i].key) + strlen(cmd_list[i].value) + 2;
                            if ((cmd_buf = malloc(cmd_buf_len)) == NULL)
                            {
                                PRINT("[%s]\n", "send_msg_to_6410 failed!");
                                break;
                            }
                            memset(cmd_buf, 0, sizeof(cmd_buf));
                            
                            sprintf(cmd_buf, "nvram_set 2860 %s %s", cmd_list[i].key, cmd_list[i].value);
                            if ((res = system(cmd_buf)) < 0)
                            {
                                PRINT("[%s]\n", "system failed!");
                                break;
                            }
                            free(cmd_buf);
                            cmd_buf = NULL;
                        }
                        if (i != sizeof(cmd_list) / sizeof(struct s_cmd))
                        {
                            PRINT("[%s]\n", "recovery of router state failed!");  
                            break;
                        }
                        data_tmp = 0x00;
                        if ((res = send_msg_to_6410(fd, &data_tmp, 1)) < 0)
                        {
                            PRINT("[%s]\n", "send_msg_to_6410 failed!");
                            break;
                        }
                        #else // 可写文件系统时
                        if ((res = read_current_route_state(cmd_list, sizeof(cmd_list) / sizeof(struct s_cmd))) < 0)
                        {
                            PRINT("[%s]\n", "read_current_route_state failed!");
                            break;
                        }
                        
                        char send_buf[128] = {0};                        
                        for (i = 0; i < sizeof(cmd_list); i++)
                        {
                            sprintf(send_buf, "nvram_set 2860 %s %s", cmd_list[i].key, cmd_list[i].value);
                            if ((res = system(send_buf)) < 0)
                            {
                                PRINT("[%s]\n", "system failed!");
                                break;
                            }
                            memset(send_buf, 0, sizeof(send_buf));
                        }
                        if (res < 0)
                        {
                            PRINT("[%s]\n", "recovery of router state failed!");
                            break;
                        }
                        data_tmp = 0x00;
                        if ((res = send_msg_to_6410(fd, &data_tmp, 1)) < 0)
                        {
                            PRINT("[%s]\n", "send_msg_to_6410 failed!");
                            break;
                        }
                        #endif
                        PRINT("[%s]\n", "recovery of router state success!");
                        break;
                    }
                    else if (strcmp(data, "reboot") == 0) // 重启命令
                    {
                        PRINT("________________________________________\n");                
                        // 根据buf中的数据设置5350
                        PRINT("cmd_count = %d\n", cmd_count);
                        PRINT("cmd_len = %d\n", cmd_len);                        
                        for (i = 0; i < cmd_count; i++)
                        {
                            memset(buf_tmp, 0, sizeof(buf_tmp));
                            sprintf(buf_tmp, "%s", cmd_buf + i * cmd_len);
                            PRINT("%s\n", buf_tmp);
                            if ((res = system(buf_tmp)) < 0)
                            {
                                PRINT("system failed!");
                                break;
                            }
                        }
                        
                        free(cmd_buf);
                        cmd_buf = NULL;
                        
                        if (res < 0)
                        {
                            break;
                        }
                                                
                        // 执行重启命令
                        if ((res = config_route_take_effect()) < 0)
                        {
                            PRINT("[%s]\n", "config_route_take_effect failed!");
                            break;
                        }
                        PRINT("config_route_take_effect success!\n");
                        break;
                    }
                    else if (strcmp(data, "wan state?") == 0) // wan口状态
                    {
                        PRINT("________________________________________\n");
                        data_tmp = 0x00;
                        if ((res = get_wan_state()) < 0)
                        {
                            PRINT("[%s]\n", "get_wan_state failed!");
                            data_tmp = 0xFF;
                        }
                                              
                        if ((res = send_msg_to_6410(fd, &data_tmp, 1)) < 0)
                        {
                            PRINT("[%s]\n", "send_msg_to_6410 failed!");
                        }
                        break;
                    }
                    else if (strcmp(data, "PPPOE state?") == 0) // wan口状态
                    {
                        PRINT("________________________________________\n");
                        memset(buf_tmp, 0, sizeof(buf_tmp));
                        buf_len = 2;
                        if ((buf_tmp[1] = get_pppoe_state()) < 0)
                        {
                            PRINT("[%s]\n", "get_wan_state failed!");
                            buf_tmp[0] = 0xFF;
                            buf_len = 1;
                        }
                                             
                        if ((res = send_msg_to_6410(fd, buf_tmp, buf_len)) < 0)
                        {
                            PRINT("[%s]\n", "send_msg_to_6410 failed!");
                        }
                        break;
                    }
                    else //5350 设置命令
                    {
                        PRINT("________________________________________\n");
                        memset(buf_tmp, 0, sizeof(buf_tmp));                        
                        memcpy(buf_tmp, data, 14);
                        if (strcmp(buf_tmp, "nvram_get 2860") == 0)
                        {
                            memset(buf_tmp, 0, sizeof(buf_tmp));
                            sprintf(buf_tmp, "%s > /var/route_state_option", data);
                            free(data);
                            data = NULL;
                            // 执行查询 导入文件
                            if ((res = system(buf_tmp)) < 0)
                            {
                                PRINT("[%s]\n", "system failed!");
                                break;
                            }
                            // 读取文件
                            if ((res = read_route_state_option(&data)) < 0)
                            {
                                PRINT("[%s]\n", "read_route_state_option failed!");
                                break;
                            }                            
                            PRINT("data = %s\n", data);
                            // 发送到6410
                            if ((res = send_msg_to_6410(fd, data, strlen(data))) < 0)
                            {
                                PRINT("[%s]\n", "send_msg_to_6410 failed!");
                            }
                            break;
                        }
                        memset(buf_tmp, 0, sizeof(buf_tmp));
                        
                        // 添加命令到cmd_buf
                        cmd_count++;
                        if ((cmd_buf = (char *)realloc(cmd_buf, cmd_count * cmd_len)) == NULL)
                        {
                            PRINT("realloc failed!");
                            break;
                        }
                        memset(cmd_buf + (cmd_count - 1) * cmd_len, 0, cmd_len);
                        memcpy(cmd_buf + (cmd_count - 1) * cmd_len, data, strlen(data));
                        free(data);
                        data = NULL;                       
                        
                        data_tmp = 0x00;
                        if ((res = send_msg_to_6410(fd, &data_tmp, 1)) < 0)
                        {
                            PRINT("[%s]\n", "send_msg_to_6410 failed!");
                            break;
                        }
                        continue;
                    }                    
                }
            }
        }
        
        PRINT("resource release start!\n");
        
        if (data != NULL)
        {
            free(data);
            data = NULL;
        }
        if (cmd_buf != NULL)
        {
            free(cmd_buf);
            data = cmd_buf;
        }
        cmd_count = 0;
        
        if (res < 0)
        {
            data_tmp = 0xFF;
            if ((res = send_msg_to_6410(fd, &data_tmp, 1)) < 0)
            {
                PRINT("[%s]\n", "send_msg_to_6410 failed!");
            }  
        }
        
        PRINT("resource release end!\n");
	}
	return 0;

}
