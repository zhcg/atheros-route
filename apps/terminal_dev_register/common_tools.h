#ifndef _COMMON_TOOLS_H_
#define _COMMON_TOOLS_H_

//#define BOARDTYPE 6410
//#define BOARDTYPE 5350
#define BOARDTYPE 9344

// 头文件包含
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>

#include <termios.h>
#include <time.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>

#include <sys/mman.h>
#include <linux/vt.h>
#include <signal.h>
#if BOARDTYPE == 6410 || BOARDTYPE == 9344
#include <sqlite3.h>
#endif
#include <regex.h>
#include <arpa/inet.h>
// 宏定义

#define TERMIANL_SCHEME 1  // 终端初始化方案。0代表是旧UI方案（无取消）；1代表新UI方案

// 大小端的问题

#if BOARDTYPE == 9344
#define ENDIAN 1 // 0代表小端；1代表大端
#elif BOARDTYPE == 5350 || BOARDTYPE == 6410
#define ENDIAN 0 // 0代表小端；1代表大端
#endif
 
// 数据大小端互转 short 2字节
#define DATA_ENDIAN_CHANGE_SHORT(A)  ((((unsigned short)(A) & 0xff00) >> 8 ) | (((unsigned short)(A) & 0x00ff) << 8 ))
// 数据大小端互转 long 4字节
#define DATA_ENDIAN_CHANGE_LONG(A)  ((((unsigned long)(A) & 0xff000000) >> 24)  | (((unsigned long)(A) & 0x00ff0000) >> 8 ) | \
                                    (((unsigned long)(A) & 0x0000ff00) << 8 )  | (((unsigned long)(A) & 0x000000ff) << 24))
                                    
#if BOARDTYPE == 5350

#ifdef CONFIG_DUAL_IMAGE
#define NVRAM_RT5350 1
#define RT5350_FREE_SPACE 2
#define RT5350_BACKUP_SPACE 3
#else
#define NVRAM_RT5350 0
#define RT5350_FREE_SPACE 1
#define RT5350_BACKUP_SPACE 2
#endif

#endif

#define CACM_SOCKCT_PORT "8658"
#define SPI_UART1_MUTEX_SOCKET_PORT "5225"

#define MSGNAME "./phone_status"
#define MSGTYPE_SND 61
#define MSGTYPE_RCV 16

#define TERMINAL_AUTHOR "Yang Jilong"
#define TERMINAL_VERSION "F2A_V1.0.0"
#define TERMINAL_REMARKS "终端初始化demoV0.4"

#define TEL_UP 	 0
#define TEL_DOWN 1

#define DEBUG 0
#define PRINT_DATA 1
#define PRINT_DEBUG 1
#define LOG_DEBUG 1
#define RECORD_DEBUG 0

#define SSID3_MONITOR 0
#define SMART_RECOVERY 0 // 智能恢复路由器 Smart recovery 
#define CTSI_SECURITY_SCHEME 2 // CTSI安全方案 

#define CHECK_WAN_STATE 1 //0：不检测与平台服务器连接状态；1：检测

#if BOARDTYPE == 6410
#define AUTHENTICATION_CONFIG   "/terminal_init/config/base_init_config"
#define __STEP_LOG "/terminal_init/log/step_log/"
#define __SERIAL_DATA_LOG_FILE "/terminal_init/log/serial_data_log/"
#elif BOARDTYPE == 5350
#define AUTHENTICATION_CONFIG   "/var/terminal_init/config/base_init_config"
#define __STEP_LOG "/var/terminal_init/log/step_log/"
#define __SERIAL_DATA_LOG_FILE "/var/terminal_init/log/serial_data_log/"
#elif BOARDTYPE == 9344
#define AUTHENTICATION_CONFIG   "/var/terminal_dev_register/config"
#endif

#if DEBUG
#define PRINT_STEP(format, ...) printf("%s["__FILE__"][%s][%05d] "format"", common_tools.get_datetime_buf(), __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define PRINT_STEP(format, ...)
#endif

#if PRINT_DATA
#define PRINT_BUF_BY_HEX(buf, data_list, len, file_name, function_name, line_num) common_tools.print_buf_by_hex(buf, data_list, len, file_name, function_name, line_num)
#else
#define PRINT_BUF_BY_HEX(buf, data_list, len, file_name, function_name, line_num) 
#endif

#if PRINT_DEBUG == 1
#define PRINT(format, ...) printf("[%s]%s["__FILE__"][%s][%05d] "format"", common_tools.argv0, common_tools.get_datetime_buf(), __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define PERROR(format, ...) printf("[%s]%s["__FILE__"][%s][%05d][errno = %d][%s] "format"", common_tools.argv0, common_tools.get_datetime_buf(), __FUNCTION__, __LINE__, errno, strerror(errno), ##__VA_ARGS__)
#else
#define PRINT(format, ...)
#define PERROR(format, ...)
#endif

#if LOG_DEBUG
#define LOG_IN_FILE_DEBUG 0 // 决定把LOG写入标准输出还是写入文件
#define OPERATION_LOG(file, func, line, info, flag) operation_steps_log(file, func, line, info, flag)
#else
#define OPERATION_LOG(file, func, line, info, flag) 
#endif

#define TERMINAL_LOCAL_SOCKET_NAME "/var/terminal_init/log/.socket_name"
#define TERMINAL_LOCAL_SERVER_PORT "53535"

#define SN_LEN 18 // 序列号长度 包括18字节SN
#define TOKENLEN 16 // 令牌长度16

/**
 * 枚举和结构体定义
 */
#pragma pack(1)
// 交易类型枚举 
enum enum_deal_type
{
    TERMINAL_AUTHENTICATION = 1,       // 终端认证
};

enum enum_chinese_type
{
    UTF8 = 1,       
    GB2312,
};

enum enum_check_type
{
    XOR = 1,       
    SUM,
};
//错误码 P_ 代表PAD，S_ 代表平台服务器
enum ERROR_NUM
{
    ACCEPT_ERR = -0xFF,    // 接收连接错误
    BIND_ERR,              // 绑定错误 
    
    BCD_TO_STR_ERR,        // BCD码转字符串错误
    
    CONNECT_ERR,           // 连接错误 
    P_CONNECT_ERR,         // 连接错误  
    S_CONNECT_ERR,         // 连接错误 -250
	CENTER_PHONE_ERR,      // 电话号码错误
    CHECK_DIFF_ERR,        // 校验不对错误
    P_CHECK_DIFF_ERR,      // 校验不对错误
    S_CHECK_DIFF_ERR,      // 校验不对错误
    DATA_ERR,              // 数据内容错误
    L_DATA_ERR,            // 本地数据内容错误 
    P_DATA_ERR,            // 数据内容错误
    S_DATA_ERR,            // 数据内容错误
    DATA_DIFF_ERR,         // 数据不相同错误
    P_DATA_DIFF_ERR,       // 数据不相同错误 -240
    S_DATA_DIFF_ERR,       // 数据不相同错误 
	DEAL_ERR_TYPE_ERR,     // 交易错误类型错误
	DEAL_TYPE_ERR,         // 交易类型错误 
	
	EXEC_ERR,              // 加载应用失败   
	FCNTL_ERR,             // fcntl设备
	FSTAT_ERR,             // 获取文件状态错误 
	FTOK_ERR,              // 获取IPC的ID值错误
	GETATTR_ERR,           // 属性设置错误
	GET_CHECK_ERR,         // 获取校验错误
	IOCTL_ERR,             // IO控制错误  -230
	LAN_ABNORMAL,          // 局域网不正常
	WAN_ABNORMAL,          // 广域网不正常 
	LENGTH_ERR,            // 数据长度错误
	P_LENGTH_ERR,          // 数据长度错误
	S_LENGTH_ERR,          // 数据长度错误
	LISTEN_ERR,            // 监听错误
	LIST_FREE_ERR,         // 链表释放错误
	LIST_NULL_ERR,         // 链表空错误  
	LIST_GET_ERR,          // 链表获得错误 
	LOGIN_FAILED,          // 登陆失败 -220
	
	MAC_ERR,               // mac地址错误 
	MALLOC_ERR,            // 内存分配错误 
	MISMATCH_ERR,          // 选项不匹配 
	P_MISMATCH_ERR,        // 选项不匹配
	S_MISMATCH_ERR,        // 选项不匹配
	MKDIR_ERR,             // 创建文件夹错误
	MSGGET_ERR,            // 消息队列获取错误
	MSGSND_ERR,            // 消息队列发送错误
	MSGRCV_ERR,            // 消息队列接收错误 
	
	NVRAM_SET_ERR,         // 数据插入失败 -210
	NVRAM_COMMIT_ERR,      // 数据提交失败 
	NO_INIT_ERR,           // 没有初始化设备 
	NO_FILE_ERR,           // 文件未找到错误 
	NULL_ERR,              // 数据为空错误 
	NO_RECORD_ERR,         // 没有此记录
	
	OFF_HOOK_ERR,          // 中心号码错误 
	OPEN_ERR,              // 文件打开错误
	OVER_LEN_ERR,          // 自定义数据超长错误
	PIPE_ERR,              // 创建管道失败 
	PTHREAD_CREAT_ERR,     // 线程创建错误 -200
	PTHREAD_CANCEL_ERR,    // 线程终止错误 
	PTHREAD_DETACH_ERR,    // 子线程的状态设置为detached错误
	
	PTHREAD_LOCK_ERR,      // 获取锁错误 
	PTHREAD_UNLOCK_ERR,    // 解锁失败错误
	PPPOE_INVALID_INFO,    // 无效的账号和密码 
	PPPOE_CALL_FAILED,     // 呼叫失败 
	PPPOE_LINK_FAILED,     // 连接失败 
	PPPOE_NO_DNS_SERVER_INFO, // 不存在DNS服务器  
	PHONE_LINE_ERR,        // 电话线状态异常错误 
	POSITION_AUTH_ERR,     // 位置认证失败 -190
	
	REGCOMP_ERR,           // 编译正则表达式 
	REGEXEC_ERR,           // 匹配字符串 
	READ_ERR,              // 文件读取错误 
	P_READ_ERR,            // 文件读取错误 
	S_READ_ERR,            // 文件读取错误  
	
	SHMGET_ERR,            // 创建共享内存失败   
	SHMAT_ERR,             // 添加共享内存到进程
	SHMDT_ERR,             // 把共享内存脱离进程 
	SHMCTL_ERR,            // 释放共享内存 
	
	SEMGET_ERR,            // 信号量获取失败 -180 
	SEMCTL_ERR,            // 信号量设置失败 
	SEMOP_ERR,             // 信号量操作失败 
	SEM_OPEN_ERR,          // 创建命名信号量  
	SEM_WAIT_ERR,          // 信号量减一 
	SEM_POST_ERR,          // 信号量加一   
	
	STOP_CMD,              // 终止命令       
	SERVER_ERR,            // 服务器端错误 
	SQLITE_OPEN_ERR,       // 数据库打开错误
	SQLITE_EXEC_ERR,       // sql语句执行错误 
	SQLITE_GET_TABLE_ERR,  // 数据库查询错误 -170 
	SELECT_ERR,            // 轮询等待错误 
	SELECT_NULL_ERR,       // 轮询为空错误 
	P_SELECT_NULL_ERR,     // 轮询为空错误 
	S_SELECT_NULL_ERR,     // 轮询为空错误 
	SOCKET_ERR,            // 获取套接字错误 
	SETSOCKOPT_ERR,        // 设置套接字属性错误
	SETATTR_ERR,           // 属性设置错误 
	SYSTEM_ERR,            // 执行shell命令失败
	STRSTR_ERR,            // strstr错误 
	SN_BASE_ERR,           // base序列号错误 -160
	SN_PAD_ERR,            // PAD序列号错误 
	TIMEOUT_ERR,           // 时间超时错误 
	WRITE_ERR,             // 文件写入错误 
	WRONGFUL_PAD_ERR,      // PAD不合法 
	WRONGFUL_DEV_ERR,      // PAD和base不合法 
	WAN_STATE_ERR,         // wan口没有插入  
	P_WRITE_ERR,           // 文件写入错误
	S_WRITE_ERR,           // 文件写入错误
	
	NO_REQUEST_ERR,        // 无请求错误
	IDENTIFYING_CODE_ERR,  // 验证码错误  -150
	OVERDUE_ERR,           // 请求数据过期
	NON_EXISTENT_ERR,      // 请求数据不存在
	REPEAT_ERR,            // 重复操作
};

//错误码
enum DEAL_ERROR_TYPE
{
    BEFORE_CALL_ID_BUSY_TONE = 1,  // 错误类型一   80包接收之前 出现忙音
    CALL_ID_DATA_ERR,              // 错误类型二   80包数据错误 包不正确
    CALL_ID_DATA_TIMEOUT,          // 错误类型三   80包接收超时 包不存在 
    
    BEFORE_LINK_BUSY_TONE,         // 错误类型四   81包接收之前 出现忙音
    LINK_DATA_ERR,                 // 错误类型五   81包数据错误 包不正确
    LINK_DATA_TIMEOUT,             // 错误类型六   81包接收超时 包不存在 
    
    BEFORE_DEAL_BUSY_TONE,         // 错误类型七   84包接收之前 出现忙音
    DEAL_DATA_ERR,                 // 错误类型八   84包数据错误 包不正确
    DEAL_DATA_TIMEOUT,             // 错误类型九   84包接收超时 包不存在 
    
    BEFORE_DATA_FINISH_BUSY_TONE,  // 错误类型十   83包接收之前 出现忙音
    DATA_FINISH_DATA_ERR,          // 错误类型十一 83包数据错误 包不正确
    DATA_FINISH_DATA_TIMEOUT,      // 错误类型十二 83包接收超时 包不存在
    
    DEAL_OVER_BUSY_TONE,           // 错误类型十三 84包接收成功 出现忙音
    
};

enum enum_options
{
    STEP_LOG,                  // 运行步骤日志 
    
    #if BOARDTYPE == 5350 || BOARDTYPE == 6410
    SERIAL_DATA_LOG,           // 串口数据日志
    #endif 
    #if BOARDTYPE == 6410 || BOARDTYPE == 9344
    DB,                        // 数据库目录 
    #endif
    
    OLD_ROUTE_CONFIG,          // 设置前的路由配置 
    PPPOE_STATE_FILE,          // 路由器PPPOE设置后，保存路由设置状态
    SPI_PARA_FILE,             // spi参数文件
    WAN_STATE_FILE,            // wan口插入状态文件 
    
    #if BOARDTYPE == 6410 || BOARDTYPE == 5350
    SERIAL_STC,    
    SERIAL_PAD,
    #endif
    
    #if BOARDTYPE == 6410
    SERIAL_5350,
    #endif
    
    #if BOARDTYPE == 6410 || BOARDTYPE == 5350
    SERIAL_STC_BAUD,
    SERIAL_PAD_BAUD,
    #endif
    
    #if BOARDTYPE == 6410
    SERIAL_5350_BAUD,
    #endif
    
    CENTERPHONE,
    CENTERIP,
    CENTERPORT,
    
    BASE_IP,
    PAD_IP,
    PAD_SERVER_PORT,
    PAD_SERVER_PORT2,
    PAD_CLIENT_PORT,
    
    TOTAL_TIME_OUT,            // 总超时时间
    ONE_BYTE_TIMEOUT_SEC,      // 接收或发送一个字节数据超时时间 s
    ONE_BYTE_TIMEOUT_USEC,     // 接收或发送一个字节数据超时时间 us
    #if BOARDTYPE == 6410 || BOARDTYPE == 5350
    ONE_BYTE_DELAY_USEC,       // 接收或发送一个字节数据延时时间 us
    #endif
    ROUTE_REBOOT_TIME_SEC,     // 路由器重启时间 
    REPEAT,                    // 错误重发次数 
    
    TERMINAL_SERVER_IP,
    TERMINAL_SERVER_PORT,
    LOCAL_SERVER_PORT,
    WAN_CHECK_NAME,
};

/**
 * 配置文件结构
 */
 struct s_config
{
    char step_log[64];
    char serial_data_log[64];
    char db[64];
    char old_route_config[64];
    char pppoe_state[64];
    char spi_para_file[64];
    char wan_state[64];
    
    char serial_stc[50];
    char serial_pad[50];
    char serial_5350[50];
    
    unsigned int serial_stc_baud;
    unsigned int serial_pad_baud;
    unsigned int serial_5350_baud;
    
    char center_ip[30];
    char center_phone[20];
    char center_port[15];
    
    char base_ip[30];
    char pad_ip[30];
    char pad_server_port[15];
    char pad_server_port2[15];   
    char pad_client_port[15];
    
    char total_timeout;
    char one_byte_timeout_sec;
    unsigned short one_byte_timeout_usec;
    unsigned short one_byte_delay_usec;
    unsigned char route_reboot_time_sec; 
    char repeat;

    char terminal_server_ip[30];
    char terminal_server_port[15];
    char local_server_port[15]; 
    char wan_check_name[30];
    
    #if CTSI_SECURITY_SCHEME == 2
    char private_key[8];
    char public_key[8];
    #endif
};
struct s_work_sum
{
    unsigned long work_sum;         // 记录总数
    unsigned long work_failed_sum;  // 记录失败总数
    unsigned long work_success_sum; // 记录正确总数
};
/**
 * 日志信息结构
 */
struct s_record_info
{
	char record_name[32];           // 日志文件名称
      
	char date_and_time[32];         // 当前日期和时间
	char file_name[33];             // 当前所在文件名称
	char function_name[33];         // 当前所在函数名称    
	unsigned short line_num;        // 当前的行号
	char print_info[5120];           // 需要打印的信息 
};

struct s_deal_fail_file
{
    unsigned char deal_error_type;                      // 交易错误类型
    
    unsigned short before_call_id_busy_tone_count;      // 错误类型一计数
    unsigned short call_id_data_err_count;              // 错误类型二计数
    unsigned short call_id_data_timeout_count;          // 错误类型三计数
    
    unsigned short before_link_busy_tone_count;         // 错误类型四计数
    unsigned short link_data_err_count;                 // 错误类型五计数
    unsigned short link_data_timeout_count;             // 错误类型六计数
    
    unsigned short before_deal_busy_tone_count;         // 错误类型七计数
    unsigned short deal_data_err_count;                 // 错误类型八计数
    unsigned short deal_data_timeout_count;             // 错误类型九计数
    
    unsigned short before_data_finish_busy_tone_count;  // 错误类型十计数
    unsigned short data_finish_data_err_count;          // 错误类型十一计数
    unsigned short data_finish_data_timeout_count;      // 错误类型十二计数
    
    unsigned short deal_over_busy_tone_count;           // 错误类型十三计数 
};
/**
 * 交易属性
 */
struct s_deal_attr
{
    unsigned char deal_type : 7;            // 交易的类型 例如回拨为交易1，验证码交易2  
    unsigned char deal_over_bit : 1;        // 交易结束标志; 1为结束
    unsigned char deal_state : 3;           // 1:已经通话; 2:已经建链; 3:已收到数据完成报文; 4：已经交易回复；5：可发交易请求
    unsigned char pack_type : 5;            // 数据包内容的类型
    unsigned char sync_data_count : 5;      // 同步数据的个数：即0x55的个数
    unsigned char list_add_bit : 1;         // 默认为0，数据加入链表标志位
    unsigned char sync_data_bit : 1;        // 同步数据接收标志     
    //unsigned char news_type_bit : 1;        // 默认为0，消息类型标志位 0x04 0x80 0x81 0x82 0x83 0x84 0x87

    unsigned char analysed_count;           // 分析的次数
    unsigned short analysed_data_len;       // 数据分析后的长度
    
    long sync_random_num;                   //保存同步随机数
    char message_sync_num;                  //保存报文同步序号
    char serial_data_type[2];               //串口通信类型
};

struct s_timer
{
	long sec;                   // 秒
	long usec;                  // 微秒
	struct timeval start;       // 开始时间
	struct timeval end;         // 结束时间
	unsigned char timer_bit;    // 定时器标志位 0为设置起始时间 1为设置结束时间 默认为0
};

struct s_cmds
{
    char cmd_bits[10];
    char cmd_keys[10][10];
    char cmd_values[14][128];
};

/**
 * 存放接收的数据 双向循环链表
 */
struct s_list_node
{
    unsigned char data;          // 链表数据数据
    struct s_list_node *next;    // 指向下一个数据的地址
    struct s_list_node *prev;    // 指向上一个数据的地址
};

/**
 * 存放接收的数据 双向循环链表
 */
struct s_data_list
{
    unsigned short list_len;                // 链表长度
    struct s_list_node *head;               // 链表头
    struct s_list_node *tail;               // 链表尾
};
 
// IP接口报文 结构
struct s_IP_msg
{
    char sync_head[6];          // 同步头 hex
    char version;               // 接口版本 hex
    unsigned short cmd;         // 操作命令 hex
    unsigned short data_len;    // 有效数据长度（低位在前） hex
    char *valid_data;           // 有效数据 
    char check_sum;             // 校验和 hex
    char sync_tail[6];          // 同步尾 hex
};

/**
 * 电话号码报文 结构
 */
struct s_phone_message
{
	char news_type;                           // 消息类型 hex
	char length;                              // 报文长度 hex
    
    //01H 日期和时间
	char time_msg_head;                       // 时间日期头 hex 
	char time_msg_len;                        // 时间日期长度 hex
	char time_msg_date[4];                    // 时间日期之日期，没有年份；如10月23日 31 30 32 33 hex
	char time_msg_time[4];                    // 时间日期之时间 hex
    
    //02H 电话号码
	char call_identify_msg_head;              // 电话号码头 hex
	char call_identify_msg_len;               // 电话号码长度 hex
	char call_identify_msg_phone_num[20];     // 电话号码 hex
    
    //07H
	char name_msg_head; 
	char name_msg_len;
	char name_msg_name[20];
    
	char check_sum;                           // 校验位
};

/**
 * 建链报文 结构
 */
struct s_link_message
{
	char news_type;                           // 消息类型 hex
	unsigned short length;                    // 报文长度 hex
	long deal_sync_random_num;                // 同步随机数 hex
	char message_sync_num;                    // 同步序号 hex
	char check_sum;                           // 校验位 hex
};

/**
 * 数据完成报文 结构
 */
struct s_data_finish_message
{
	char news_type;                           // 消息类型
	unsigned short length;                    // 报文长度
	long deal_sync_random_num;                // 同步随机数
	char message_sync_num;                    // 同步序号
	char check_sum;                           // 校验位
};

/**
 * 发往中心的报文 结构
 */
struct s_deal_message_87
{
	char news_type;                           // 消息类型
	unsigned short length;                    // 报文长度
	long deal_sync_random_num;                // 同步随机数
	char message_sync_num;                    // 同步序号
	unsigned short news_content_len;          // 消息内容长度
	
	struct s_news_content_87
	{
    	char message_type;                    // 报文类型
    	char end_flag;                        // 结束标志
		char program_version_num[2];          // 程序版本号 BCD
		char application_version_num[4];      // 应用版本号 BCD
		char display_flag;                    // 来电显示 ASC		
		char password_keyboard_num[8];        // 密码键盘序列号 BCD
		char deal_num[3];                     // 交易流水号 BCD
		char deal_code[3];                    // 交易代码 BCD
		char *flowsheet_code;                 // 流程代码
		unsigned short valid_data_length;     // 有效数据长度
		char *valid_data_zone;                // 有效数据
		char MAC[8];                          // MAC BCD
	}news_content_87;
	char check_sum;                           // 校验位
};

/**
 * 来自中心的报文 结构
 */
struct s_deal_message_84
{
	char news_type;                           // 消息类型
	unsigned short length;                    // 报文长度
	long deal_sync_random_num;                // 同步随机数
	char message_sync_num;                    // 同步序号
	unsigned short news_content_len;          // 消息内容长度
    
	struct s_news_content_84
    {
    	char message_type;                    // 报文类型
    	char end_flag;                        // 结束标志
    	char password_keyboard_num[8];        // 密码键盘序列号
    	char system_date[4];                  // 系统日期
    	char system_time[3];                  // 系统时间
    	char deal_num[3];                     // 交易流水号
        char deal_code[3];                    // 交易代码
        char *flowsheet_code;                 // 流程代码
		unsigned short valid_data_length;     // 有效数据长度
		char *valid_data_zone;                // 有效数据
		char MAC[8];                          // MAC BCD
    }news_content_84;
    char check_sum;                           // 校验位
};

/**
 * s_dial_back_respond 终端认证 结构
 */
struct s_dial_back_respond
{
	char BASE_id[SN_LEN + 1];                 // BASE_id SN_LEN
	char PAD_id[SN_LEN + 1];                  // PAD_id  SN_LEN
	
	#if CTSI_SECURITY_SCHEME == 1
	long base_random;                         // 终端随机数
	#else
	char base_random[8];                         // 终端随机数
	#endif
	
	char base_random_mac[8];                  // 终端随机数MAC
	
	#if CTSI_SECURITY_SCHEME == 1
	long terrace_random;                      // 平台随机数
	#else
	char system_random[8];                      // 系统随机数
	#endif
	char terrace_random_mac[8];               // 平台随机数 MAC   
		
	char *failed_reason;                      // 失败原因 成功的时候没有
	unsigned char terminal_token_len;         // 终端令牌的长度
	char result_num;                          // 结果 0为成功 1为失败
	
	#if CTSI_SECURITY_SCHEME == 1
	char terminal_token[100];                 // 终端令牌
	#else
	char device_token[100];                   // 设备令牌
	char position_token[100];                 // 位置令牌
	char discrete_factor[8];                  // 离散因子
	char phone_num[16];
	char call_permit;                         // 呼叫许可
	char call_result;                         // 接收语音中心结果 // 0 呼叫成功并且拒接等步骤成功 1 中间错误 
	#endif
	unsigned short cmd;                       // 命令字
};

struct s_msgbuf
{
    long mtype;
    char mtext[256];
};

// common_tools结构体定义
struct class_common_tools
{
    struct s_deal_attr *deal_attr;
    struct s_config *config;
    struct s_work_sum *work_sum;  
    struct s_timer *timer;
    unsigned char *phone_status_flag;
    char argv0[32];
    
    // 项目相关
    int (* get_large_number)(int num1, int num2);
    int (* get_config)();
    int (* set_GPIO4)(int cmd);
    int (* make_menulist)(time_t start_time, struct timeval start, struct timeval end);
    #if BOARDTYPE == 6410 || BOARDTYPE == 5350
    int (* make_serial_data_file)(int *write_buf_fd, char buf, int buf_len);
    #endif
    int (* get_phone_stat)();
    int (* get_network_state)(char *ip, unsigned char ping_count, unsigned char repeat);
    int (* get_user_prompt)(int error_num, char **out_buf);
    int (* get_errno)(char role, int error_num);
    int (* get_list_MAC)(struct s_data_list *a_data_list, unsigned short start_index, unsigned short end_index, char *MAC);
    int (* get_MAC)(int src, char *MAC);
    int (* get_wan_mac)(char *mac);
    // 项目无关
    // 字符串操作
    int (* BCD_to_string)(char* p_src, int src_size, char* p_dst, int dst_size);
//  int (* make_data_middle)(char *data, int data_len, int array_len);
    int (* long_to_str)(unsigned long src, char *dst, unsigned char dst_len);
    int (* str_in_str)(const void *src, unsigned short src_len, const char dest, unsigned short dest_len);
    void* (* memncat)(void *dest, const void *src, const int index, const int len);
    int (* trim)(char *str);
    int (* match_str)(const char *rule, const char *str);
    void (* mac_add_horizontal)(char *mac);
    void (* mac_add_colon)(char *mac);
    void (* mac_del_colon)(char *mac);
    int (* mac_format_conversion)(char *mac);
    int (* add_quotation_marks)(char *data);
    int (* del_quotation_marks)(char *data);
    
    // 时间操作
    char* (* get_datetime_buf)();
//  int (* get_service_use_time)(time_t start_time, char *service_use_time);
    double (* get_use_time)(struct timeval start, struct timeval end);
//  int (* time_out)(struct timeval *start, struct timeval *end);
    int (* set_start_time)(char *start_time);
    
    // 数据发送与接收
    int (* recv_one_byte)(int fd, char *data, struct timeval *tv);
    int (* recv_data)(unsigned int fd, unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv);
    int (* recv_data_head)(int fd, char *head, unsigned char head_len, struct timeval *tv);
    int (* send_one_byte)(int fd, char *data, struct timeval *tv); 
    int (* send_data)(unsigned int fd, unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv);
    
    // 链表操作
    int (* list_tail_add_data)(struct s_data_list *a_data_list, char data);
    int (* list_head_del_data)(struct s_data_list *a_data_list, char data);
    int (* list_free)(struct s_data_list *a_data_list);
    int (* list_get_data)(struct s_data_list *a_data_list, unsigned short index, unsigned short count, char *list_buf);
    
    // 其他
    int (* get_cmd_out)(char *cmd, char *buf, unsigned short buf_len);
    int (* serial_init)(const int fd, int baud);
    int (* get_checkbit)(char *buf, struct s_data_list *data_list, unsigned short start_index, unsigned short len, unsigned char check_type, unsigned char bytes);
    int (* print_buf_by_hex)(const void *buf, struct s_data_list *data_list, const unsigned long len, const char *file_name, const char *function_name, const unsigned short line_num);
    int (* get_rand_string)(unsigned char english_char_len, unsigned char chinese_char_len, char *out, unsigned char chinese_char_type);
    int (*start_up_application)(const char* path, char * const argv[], unsigned char flag);
};

extern struct class_common_tools common_tools;

/**
 * 服务运行步骤日志
 */
extern int operation_steps_log(const char *file_name, const char *function_name, const unsigned short line_num, const char *log_info, short flag);
#pragma pop()

#endif
