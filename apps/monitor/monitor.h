#ifndef __MONITOR_H__
#define __MONITOR_H__

#pragma pack (1)
//#define SIP_PORT 5060
#define INIT_PORT 				9999
#define INIT_SERVER_IP 			"210.14.156.93"
//#define INIT_SERVER_IP		"218.241.217.218"
#define STATUS_DELAY 	 		1800
#define FAST_STATUS_DELAY 	 	120
#define OPTION_DELAY	 		60
#define CPU_WARING_THRESHOLD	70
#define MEM_WARING_THRESHOLD	80
#define HD_WARING_THRESHOLD		90
#define WARNING_TIMES			100
#define PRINT(format, ...)  	printf("[%s][%s][-%d-] "format"",__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
//#define PRINT(format, ...) printf("[%s][%s][-%d-] "format"",system_time(),__FUNCTION__,__LINE__,##__VA_ARGS__)
#define TOMCAT 					"/opt/jdk1.7.0_45/bin/java"
#define PHP						"/opt/httpd/bin/httpd"
#define SUCCESS 				0x1001
#define FAIL 					0x1002

enum ERR_NUM 
{ 
    INIT_REGISTER_ERR = -500,
    INIT_LOGOUT_ERR,
    BUILD_REGISTER_ERR,
    EXOSIP_INIT_ERR,
    EXOSIP_LISTEN_ERR,
    NO_EVENT_ERR,
    ADD_AUTHENTICATION_INFO_ERR,
};

/**
 * flash 信息结构体
 */
struct s_flash_info         
{
    unsigned char name[64];
    unsigned int size;
    unsigned int used;
    unsigned int available;
    unsigned int use;
    unsigned char mounted[16];
};

/**
 * cpu 信息结构体
 */
struct s_cpu_info         
{
    unsigned char name[10];
    unsigned long user;
    unsigned char user_key[8];
    unsigned long system;
    unsigned char system_key[8];
    unsigned long nice;
    unsigned char nice_key[8];
    unsigned long idle;
    unsigned char idle_key[8];
    unsigned long io;
    unsigned char io_key[8];
    unsigned long irq;
    unsigned char irq_key[8];
    unsigned long softirq;
    unsigned char softirq_key[8];
};

/**
 * mem 信息结构体
 */
struct s_mem_info 
{
	unsigned long total;
    unsigned char name[10];
    unsigned long used;
    unsigned char used_key[8];
    unsigned long free;
    unsigned char free_key[8];
    unsigned long shrd;
    unsigned char shrd_key[8];
    unsigned long buff;
    unsigned char buff_key[8];
    unsigned long cached;
    unsigned char cached_key[8];
};

/**
 * 进程 信息结构体
 */
struct s_process_info
{
    unsigned int pid;
    unsigned int ppid;
    unsigned char user[32];
    unsigned char stat[6];
    unsigned int vsz;
    unsigned int mem;
    unsigned int cpu;
    char name[64];
};

/**
 * flash 信息结构体
 */
struct s_port_info         
{
    unsigned char proto[16];
    unsigned int recv;
    unsigned int send;
    unsigned char l_addr[32];
    unsigned char f_addr[32];
    unsigned char state[16];
};


struct msg_t
{
    char base_sn[32];
    char base_a20_version[64];
    char base_9344_version[64];
    char base_532_version[64];
    char user_name[64];
    char password[64];
    char proxy_ip[64];
    int proxy_port;
    char localip[64];
    int local_port;
    
    char register_uri[128];
    char from[128];
    char to[128];
    char contact[128];
    
    char identity[64]; //身份
    int expires;
    int id;
        
    int fd;
    char register_flag;                 // 注册成功标志
    char hint[128];                     // 提示   
};

struct msg_head
{
	unsigned char ver;
	unsigned short cmd;
	unsigned short len;
};

struct init_request
{
	struct msg_head head;
	unsigned short type[2];
	unsigned char id[34];
	unsigned short reserve;
	unsigned char xor;
};

struct init_respond
{
	struct msg_head head;
	unsigned short type[2];
	unsigned char id[34];
	unsigned char msg[128];
	unsigned short result;
	unsigned char xor;
};

struct sip_info
{
    unsigned char user_name[64];
    unsigned char password[64];
    unsigned char proxy_ip[64];
    unsigned char proxy_port[16];
    int sip_status;
};

typedef struct _AS532_KEY_DATA_STR
{
    unsigned char appName[32];
    unsigned char vesselName[32];
    unsigned char vesselId;
    unsigned char appId;
    unsigned char p1; 
    unsigned char p2; 
    unsigned char pubKey[514];
}AS532_KEY_DATA, *pAS532_KEY_DATA;

typedef struct _F2B_INFO_DATA_STR
{
    unsigned char baseSn[20];
    unsigned char mac[20];
    unsigned char baseVer[64];
    unsigned char keySn[64];
    unsigned char keyCount;
    AS532_KEY_DATA keyData[4];
}F2B_INFO_DATA, *pF2B_INFO_DATA;

#endif
