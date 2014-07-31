#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <termios.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/time.h>
#include <signal.h>

#include <time.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <sys/mman.h>
#include <linux/vt.h>
#include <signal.h>
#include <regex.h>
#include <locale.h>
#include <stdio.h>
#include <sys/stat.h>  //stat函数
#include <sys/wait.h>

#include "eXosip2/eXosip.h"
#include "monitor.h"
#ifdef BASE_9344
#include "sqlite3.h"
#endif

struct msg_t sip_msg;
pthread_mutex_t send_mutex;
int init_server_fd = 0;
F2B_INFO_DATA fid;
struct sip_info sip_i;
int init_success = 0;
int new_sip_info = 0;
unsigned char base_sn[34] = {0};

static int set_buf(char *buf,int len)
{
	int i;
	char c = 0;
	for(i=0;i<len;i++)
	{
		c = buf[i] << 4;
		buf[i] = c + ((buf[i]>>4) & 0xf);
	}
	return 0;
}

#ifdef BASE_9344
//打印16进制
const char hex_to_asc_table[16] = {0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x41,0x42,0x43,0x44,0x45,0x46};
int ComFunChangeHexBufferToAsc(unsigned char *hexbuf,int hexlen,char *ascstr)
{
	unsigned char i;
	unsigned char ch,h,l;
	char *pdes = ascstr;
	for(i = 0;i < hexlen;i++)
	{
		ch = hexbuf[i];
		h = (ch >> 4) & 0x0F;
		l = ch & 0x0F;
		*(pdes++) = hex_to_asc_table[h];
		*(pdes++) = hex_to_asc_table[l];
	}
	*pdes = '\0';
	return hexlen * 2;
}

void ComFunPrintfBuffer(unsigned char *pbuffer,unsigned char len)
{
	char *pstr;
	pstr = (char *)malloc(512);
	if(pstr == NULL)
		return;
	ComFunChangeHexBufferToAsc(pbuffer,len,pstr);

	PRINT("%s\r\n",pstr);

	free((void *)pstr);
}

unsigned char sumxor(const  unsigned char  *arr, int len)
{
	int i=0;
	unsigned char sum = 0;
	for(i=0; i<len; i++)
	{
		sum ^= arr[i];
	}

	return sum;
}

int get_sn()
{
    system("fw_printenv SN > /tmp/.sn_tmp");
    char buf[128]={0};
    int fd = open("/tmp/.sn_tmp",O_RDWR);
    if(fd < 0)
    {   
        printf("open err\n");
        return -1; 
    }   
    if(read(fd,buf,sizeof(buf))>0)
    {   
        if(strstr(buf,"SN=")==NULL)
        {
            close(fd);
            return -1; 
        }
        printf("buf = %s\n",buf);
        memset(base_sn,0,sizeof(base_sn));
        sscanf(buf,"SN=%s",base_sn);
    }   
    close(fd);
    return 0;
}
#endif

static int get_flash_occupy_info(int *size)
{  
    FILE *fp = NULL;
    char buf_tmp[512] = {0};
    char process_info_buf[4096] = {0};
	struct s_flash_info	flash_info;
#ifdef BASE_9344
    if (system("df | grep \"/dev/mtdblock2\" 1> /var/log/.flash_state") < 0)
    {
        PRINT("system failed!\n");
        return -1;
    }
#elif defined(BASE_A20)
    if (system("df | grep \"/opt\" -w 1> /var/log/.flash_state") < 0)
    {
        PRINT("system failed!\n");
        return -1;
    }
#endif
    if ((fp = fopen("/var/log/.flash_state", "r")) == NULL)
    {
        PRINT("open failed!\n");
        return -2;
    }
        
    while(1)
    {
		if (fgets(buf_tmp, sizeof(buf_tmp), fp) == NULL)
		{
			PRINT("fgets failed!\n");
			break;
		}
		PRINT("strlen(buf_tmp) = %d buf_tmp = %s", strlen(buf_tmp), buf_tmp);
		memset(&flash_info,0,sizeof(flash_info));
		sscanf(buf_tmp, "%s %d %d %d %d%% %s", flash_info.name,&flash_info.size ,&flash_info.used,&flash_info.available,
				&flash_info.use,flash_info.mounted);
		memset(buf_tmp, 0, strlen(buf_tmp));
		*size = flash_info.use;
		fclose(fp);
		return 0;
	}
	fclose(fp);
    return -3;  
}

//端口信息
static int get_port_info(int *tomcat,int *php)
{  
    FILE *fp = NULL;
    char buf_tmp[512] = {0};
    char process_info_buf[4096] = {0};
    char *p = NULL;
	struct s_port_info	port_info;
    if (system("netstat -antl | grep \":80\" 1> /var/log/.port_state") < 0)
    {
        PRINT("system failed!\n");
        return -1;
    }
    if ((fp = fopen("/var/log/.port_state", "r")) == NULL)
    {
        PRINT("open failed!\n");
        return -2;
    }
        
    while(1)
    {
		if (fgets(buf_tmp, sizeof(buf_tmp), fp) == NULL)
		{
			PRINT("fgets failed!\n");
			break;
		}
		PRINT("strlen(buf_tmp) = %d buf_tmp = %s", strlen(buf_tmp), buf_tmp);
		memset(&port_info,0,sizeof(port_info));
		sscanf(buf_tmp, "%s %d %d %s %s %s", port_info.proto,&port_info.recv ,&port_info.send,port_info.l_addr,
				&port_info.f_addr,port_info.state);
		memset(buf_tmp, 0, strlen(buf_tmp));
		
		if(!strcmp(port_info.state,"LISTEN") && (p = strstr(port_info.l_addr,":80")) != NULL)
		{
			p += strlen(":80");
			if(*p == '\0')
			{
				PRINT("php!\n");
				*php += 1;
			}
		}
		if(!strcmp(port_info.state,"LISTEN") && (p = strstr(port_info.l_addr,":8081")) != NULL)
		{
			p += strlen(":8081");
			if(*p == '\0')
			{
				PRINT("tomcat!\n");
				*tomcat += 1;
			}
		}
	}
	fclose(fp);
    return 0;  
}

int cal_cpuinfo (struct s_cpu_info* cpu_info, struct s_cpu_info* cpu_info2)
{  
	unsigned long od, nd;   
	unsigned long id, sd;
	int cpu_use = 0;  

	od = (unsigned long) (cpu_info->user + cpu_info->nice + cpu_info->system +cpu_info->idle);
	nd = (unsigned long) (cpu_info2->user + cpu_info2->nice + cpu_info2->system +cpu_info2->idle);

	id = (unsigned long) (cpu_info2->user - cpu_info->user);
	sd = (unsigned long) (cpu_info2->system - cpu_info->system);
	PRINT("nd: %u\n",nd);
	PRINT("od: %u\n",od);
	PRINT("id: %u\n",id);
	PRINT("sd: %u\n",sd);
	if((nd-od) != 0)
	{
		cpu_use = (int)((sd+id)*100)/((nd-od));
		if(cpu_use > 100)
			cpu_use = 100;
	}
	else 
		cpu_use = 0;
	PRINT("cpu: %u\n",cpu_use);
	return cpu_use;
}
/**
 * 获取当前base的cpu等使用情况
 */
#ifdef BASE_A20
static int get_base_state(char *buf, int *buf_len)
{
    int i = 0;
    int res = 0;
    int fd = 0;
    int tomcat = 0;
    int php = 0;
	int ret = 0;
    struct stat file_stat;
    char * file_name = "/var/log/.base_state";
    FILE *fp = NULL;
    struct timeval tv;
    memset(&tv, 0, sizeof(struct timeval));
    memset(&file_stat, 0, sizeof(struct stat));
    
    struct s_cpu_info cpu_info;
    memset(&cpu_info, 0, sizeof(struct s_cpu_info));
    
    struct s_mem_info mem_info;
    memset(&mem_info, 0, sizeof(struct s_mem_info));
    
    struct s_process_info process_info;
    memset(&process_info, 0, sizeof(struct s_process_info));    
    
    char tmp_buf[15] = {0};
    char process_name[64] = {0};
    char buf_tmp[512] = {0};
    char process_info_buf[4096] = {0};
    if (system("top -b -n 1 > /var/log/.base_state") < 0)
    {
        PRINT("system failed!\n");
        return -1;
    }
    if ((fp = fopen("/var/log/.base_state", "r")) == NULL)
    {
        PRINT("open failed!\n");
        return -2;
    }
    
    // mem信息
    if(fgets(buf_tmp, sizeof(buf_tmp), fp) == NULL)
	{
		PRINT("fgets err\n");
		fclose(fp);
		return -3;
	}
    PRINT("strlen(buf_tmp) = %d buf_tmp = %s", strlen(buf_tmp), buf_tmp);
    sscanf(buf_tmp, "%s %uK %s %uK %s %uK %s %uK %s %uK %s", mem_info.name, &mem_info.used, mem_info.used_key, 
        &mem_info.free, mem_info.free_key, &mem_info.shrd, mem_info.shrd_key, &mem_info.buff, mem_info.buff_key,
        &mem_info.cached, mem_info.cached_key);
    memset(buf_tmp, 0, strlen(buf_tmp));
    PRINT("mem_info.used_key = %s\n",mem_info.used_key);
    // cpu信息
    if(fgets(buf_tmp, sizeof(buf_tmp), fp) == NULL)
	{
		PRINT("fgets err\n");
		fclose(fp);
		return -4;
	}
    PRINT("strlen(buf_tmp) = %d buf_tmp = %s", strlen(buf_tmp), buf_tmp);
    sscanf(buf_tmp, "%s %u%% %s %u%% %s %u%% %s %u%% %s %u%% %s %u%% %s %u%% %s", cpu_info.name, &cpu_info.user, cpu_info.user_key, 
        &cpu_info.system, cpu_info.system_key, &cpu_info.nice, cpu_info.nice_key, &cpu_info.idle, cpu_info.idle_key,
        &cpu_info.io, cpu_info.io_key, &cpu_info.irq, cpu_info.irq_key, &cpu_info.softirq, cpu_info.softirq_key);
    memset(buf_tmp, 0, strlen(buf_tmp));
    PRINT("idle = %d\n",cpu_info.idle);
    PRINT("user = %d\n",cpu_info.user);
    if((cpu_info.user+cpu_info.system) > WARING_THRESHOLD)
		ret = 1;
	else
		ret = 0;
    // load average 信息
    if(fgets(buf_tmp, sizeof(buf_tmp), fp) == NULL)
	{
		PRINT("fgets err\n");
		fclose(fp);
		return -5;
	}
    PRINT("strlen(buf_tmp) = %d buf_tmp = %s", strlen(buf_tmp), buf_tmp);
    memset(buf_tmp, 0, strlen(buf_tmp));
    //PRINT("_____________________\n");
    
    // 列名
    if(fgets(buf_tmp, sizeof(buf_tmp), fp) == NULL)
	{
		PRINT("fgets err\n");
		fclose(fp);
		return -6;
	}
    memset(buf_tmp, 0, strlen(buf_tmp));
    // 进程信息
    while (1)
    {
        if (fgets(buf_tmp, sizeof(buf_tmp), fp) == NULL)
        {
            PRINT("fgets failed!\n");
            break;
        }
        
        if (strlen(buf_tmp) < 6)
        {
            //PRINT("buf_tmp[0] = %02X, buf_tmp[1] = %02X, buf_tmp[2] = %02X\n", buf_tmp[0], buf_tmp[1], buf_tmp[2]);
            break;
        }
        buf_tmp[strlen(buf_tmp) - 1] = '\0';
        //PRINT("strlen(buf_tmp) = %d buf_tmp = %s\n", strlen(buf_tmp), buf_tmp);
        
        sscanf(buf_tmp, "%u %u %s %s %u %u %% %u", &process_info.pid, &process_info.ppid, process_info.user, 
            process_info.stat, &process_info.vsz, &process_info.mem, &process_info.cpu);
        
        for (i = strlen(buf_tmp) - 1; i > 1; i--)
        {
            if (buf_tmp[i] == '%')
            {
                memset(process_info.name, 0, sizeof(process_info.name));
                memcpy(process_info.name, buf_tmp + i + 2, strlen(buf_tmp + i + 2));
                while (process_info.name[strlen(process_info.name) - 1] == ' ')
                {
                    process_info.name[strlen(process_info.name) - 1] = '\0';
                }
                break;
            }
        }
		if(strstr(process_info.name,TOMCAT))
		{
			//found tomcat
			tomcat = 1;
		}
		if(!strcmp(process_info.name,PHP))
		{
			//found php
			php = 1;
		}
    }
    fclose(fp);
    
    if ((res = get_port_info(&tomcat,&php)) < 0)
    {
        PRINT("get_port_info failed!\n");
        tomcat = -1;
        php = -1;
    }
    int flash_size = 0;
    // flash 使用情况
    if ((res = get_flash_occupy_info(&flash_size)) < 0)
    {
        PRINT("get_flash_occupy_info failed!\n");
        flash_size = 0;
    }
    //PRINT("tomcat = %d\n",tomcat);
    //PRINT("php = %d\n",php);
    sprintf(buf,"{id:%s,dev_type:1,cpu:%d,memory:%d,hd:%d,tomcat:%d,php:%d}",
		sip_msg.base_sn,100-cpu_info.idle,100-(mem_info.free*100)/(mem_info.used+mem_info.free+mem_info.shrd+mem_info.buff+mem_info.cached),flash_size,(tomcat >= 2)?1:0,(php >= 2)?1:0);
    PRINT("strlen(buf) = %d\n", strlen(buf));
	*buf_len = strlen(buf);
    //buf[0] = 0x02;
    //buf[1] = 0x24;
    //buf[2] = strlen(buf) & 0xFF;
    //buf[3] = (strlen(buf) >> 8 ) & 0xFF;
    return ret;
}
#elif defined(BASE_9344)
int sqlite3_interface(char *tb_name,char *data_name, char *where_value,char *where_name,char *out_value)
{
    int i = 0;
    int index = 0;
    char **result_buf; //是 char ** 类型，两个*号
    int row_count = 0, column_count = 0;
    sqlite3 *db;
    char *err_msg;
    char sql[128] = {0};
        
    if ((data_name == NULL) || (out_value == NULL))
    {
        PRINT("para is NULL!\n");
        return -1;
    }
    if (sqlite3_open("/var/terminal_dev_register/db/terminal_base_db", &db) != 0)
    {
        PRINT("%s\n",sqlite3_errmsg(db));
        return -1;
    }
    //select age from data where age="15"
    strcpy(sql, "select ");
    strncat(sql, data_name, strlen(data_name));
    strcat(sql, " from ");
    strcat(sql, tb_name);
    strcat(sql, " where ");
    strncat(sql, where_name, strlen(where_name));
    strcat(sql, "=\"");
    strncat(sql, where_value, strlen(where_value));
    strcat(sql, "\" ");
    strcat(sql,"collate NOCASE;");
    //PRINT("sql:%s\n",sql);
    if(sqlite3_get_table(db, sql, &result_buf, &row_count, &column_count, &err_msg) != 0)
    {
        PRINT("%s\n", err_msg);
        sqlite3_free_table(result_buf);
        return -1;
    }
    
    index = column_count;
    for (i = 0; i < column_count; i++)
    {
        if (strcmp(result_buf[index], "") == 0)
        {
            PRINT("no data\n");
            return -1;    
        }
        memcpy(out_value, result_buf[index], strlen(result_buf[index]));
        ++index;
    }
    
    sqlite3_free_table(result_buf);
    sqlite3_close(db);
    return 0;
}

static int get_base_state(char *buf, int *buf_len)
{
    int i = 0;
    int res = 0;
    int fd = 0;
	int ret = 0;
	int cpu_out = 0;
	char register_state[4]={0};
	char pad_sn[34]={0};
    struct stat file_stat;
    char * file_name = "/var/log/.base_state";
    FILE *fp = NULL;
    struct timeval tv;
    memset(&tv, 0, sizeof(struct timeval));
    memset(&file_stat, 0, sizeof(struct stat));
    
    struct s_cpu_info cpu_info;
    memset(&cpu_info, 0, sizeof(struct s_cpu_info));
    struct s_cpu_info cpu_info2;
    memset(&cpu_info2, 0, sizeof(struct s_cpu_info));
    
    struct s_mem_info mem_info;
    memset(&mem_info, 0, sizeof(struct s_mem_info));
    
    struct s_process_info process_info;
    memset(&process_info, 0, sizeof(struct s_process_info));    
    
    char tmp_buf[15] = {0};
    char process_name[64] = {0};
    char buf_tmp[512] = {0};
    char process_info_buf[4096] = {0};
    // cpu信息1
	if ((fp = fopen("/proc/stat", "r")) == NULL)
    {
        PRINT("open failed!\n");
        return -4;
    }
    if(fgets(buf_tmp, sizeof(buf_tmp), fp) == NULL)
	{
		PRINT("fgets err\n");
		fclose(fp);
		return -5;
	}
    PRINT("strlen(buf_tmp) = %d buf_tmp = %s", strlen(buf_tmp), buf_tmp);
    sscanf(buf_tmp, "%s %u %u %u %u %u %u", cpu_info.name, &cpu_info.user, &cpu_info.nice,
        &cpu_info.system, &cpu_info.idle,&cpu_info.irq, &cpu_info.softirq);
    memset(buf_tmp, 0, strlen(buf_tmp));
	fclose(fp);    
	
    // mem信息
	if ((fp = fopen("/proc/meminfo", "r")) == NULL)
    {
        PRINT("open failed!\n");
        return -1;
    }
    
    if(fgets(buf_tmp, sizeof(buf_tmp), fp) == NULL)
	{
		PRINT("fgets err\n");
		fclose(fp);
		return -2;
	}
    PRINT("strlen(buf_tmp) = %d buf_tmp = %s", strlen(buf_tmp), buf_tmp);
    sscanf(buf_tmp, "%s %u", mem_info.name, &mem_info.total);
    memset(buf_tmp, 0, strlen(buf_tmp));
    if(fgets(buf_tmp, sizeof(buf_tmp), fp) == NULL)
	{
		PRINT("fgets err\n");
		fclose(fp);
		return -3;
	}
    PRINT("strlen(buf_tmp) = %d buf_tmp = %s", strlen(buf_tmp), buf_tmp);
    sscanf(buf_tmp, "%s %u", mem_info.name, &mem_info.free);
    memset(buf_tmp, 0, strlen(buf_tmp));
	fclose(fp);
	

    sleep(2);
    // cpu信息2
	if ((fp = fopen("/proc/stat", "r")) == NULL)
    {
        PRINT("open failed!\n");
        return -6;
    }
    if(fgets(buf_tmp, sizeof(buf_tmp), fp) == NULL)
	{
		PRINT("fgets err\n");
		fclose(fp);
		return -7;
	}
    PRINT("strlen(buf_tmp) = %d buf_tmp = %s", strlen(buf_tmp), buf_tmp);
    sscanf(buf_tmp, "%s %u %u %u %u %u %u", cpu_info2.name, &cpu_info2.user, &cpu_info2.nice,
        &cpu_info2.system, &cpu_info2.idle,&cpu_info2.irq, &cpu_info2.softirq);
    memset(buf_tmp, 0, strlen(buf_tmp));
	fclose(fp);  
	  
	cpu_out = cal_cpuinfo(&cpu_info,&cpu_info2);
    if((cpu_out) > WARING_THRESHOLD)
		ret = 1;
	else
		ret = 0;
    int flash_size = 0;
    // flash 使用情况
    if ((res = get_flash_occupy_info(&flash_size)) < 0)
    {
        PRINT("get_flash_occupy_info failed!\n");
        flash_size = 0;
    }
	sqlite3_interface("terminal_base_tb","register_state","0","register_state",register_state);
	if(!strcmp(register_state,"0"))
	{
		sqlite3_interface("terminal_base_tb","pad_sn","0","register_state",pad_sn);
	}
	if(strlen(pad_sn) == 0)
	{
		memset(pad_sn,0,sizeof(pad_sn));
		memcpy(pad_sn,"0",strlen("0"));
	}
    sprintf(buf,"{id:%s,dev_type:1,cpu:%d,memory:%d,hd:%d,pad_sn:%s}",
		sip_msg.base_sn,cpu_out,100-(mem_info.free*100)/(mem_info.total),flash_size,pad_sn);
    PRINT("strlen(buf) = %d\n", strlen(buf));
	*buf_len = strlen(buf);
    //buf[0] = 0x02;
    //buf[1] = 0x24;
    //buf[2] = strlen(buf) & 0xFF;
    //buf[3] = (strlen(buf) >> 8 ) & 0xFF;
    return ret;
}
#endif

/**
 * 其他请求
 */
static int other_request(eXosip_event_t *ev)
{
    int res = 0;
    unsigned char state_type = 0;
    char *tmp = NULL; 
    char buf[2048] = {0};
    char *recv_buf = NULL;
    char send_buf[4096] = {0};
    char length[5] = {0};
    size_t recv_buf_len = 0;
    
    PRINT("other_request entry!\n");
    if (ev->request == NULL) 
    {
        return -1;
    }
    
    if (strcmp(ev->request->sip_method, "MESSAGE") == 0)
    {
        PRINT("Receiving MESSAGE request!\n");
        eXosip_message_send_answer(ev->tid, 200, NULL);
    }
    else if (strcmp(ev->request->sip_method, "OPTIONS") == 0)
    {   
        PRINT("Receiving OPTIONS request!\n");
        osip_message_t *options = NULL;
        osip_message_to_str(ev->request, &recv_buf, &recv_buf_len);
        
        if (recv_buf == NULL)
        {
            eXosip_options_send_answer(ev->tid, 730, NULL);
			return -1;
        }
        
        PRINT("recv_buf = %s\n", recv_buf);
        if ((tmp = strstr(recv_buf, "i=")) == NULL)
        {
            eXosip_options_send_answer(ev->tid, 730, NULL);
			return -1;
        }
        
        state_type = (unsigned char)(*(tmp + 2) - '0');
        PRINT("state_type = %d\n", state_type);
        
        
        switch (state_type)
        {
            case 1:
            {
                eXosip_options_send_answer(ev->tid, 200, NULL);
                return -1;
            }
            case 2:
            {
                eXosip_options_send_answer(ev->tid, 200, NULL);
                return -1;
            }
            case 3:
            {
                break;
            }
            default:
            {
                eXosip_options_send_answer(ev->tid, 200, NULL);
                return  -1;   
            }
        }        
        osip_free(recv_buf);
        
    }
    else if (strcmp(ev->request->sip_method, "WAKEUP") == 0) 
    {
        PRINT("Receiving WAKEUP request!\n");
        eXosip_message_send_answer(ev->tid, 200, NULL);
    }
    else 
    {
        osip_message_to_str(ev->request, &recv_buf, &recv_buf_len);
        if (recv_buf != NULL)
        {
            PRINT("Unsupported request received:%s\n", recv_buf);
            osip_free(recv_buf);
        }
        /*answer with a 501 Not implemented*/
        eXosip_message_send_answer(ev->tid, 501, NULL);
    }
    return 0;
}

/**
 * 注册成功
 */
static int register_success(eXosip_event_t *ev)
{
    char *ru = NULL;
    osip_header_t *h = NULL;
    osip_uri_t *requri = osip_message_get_uri(ev->request);
    
    int status_code = osip_message_get_status_code(ev->response);
    
    osip_uri_to_str(requri,&ru);
    if (sip_msg.expires == 0)
    {
        PRINT("Success from the server (%s) logout.(status_code = %d)\n", ru, status_code);
    }
    else
    {
        PRINT("Registration on %s successful.(status_code = %d)\n", ru, status_code);
    }
    osip_free(ru);
    sip_msg.register_flag = 1;
    return 0;
}

/**
 * 注册失败分析
 */
static int register_failed(eXosip_event_t *ev)
{
    int status_code = 0;
    char *reason = NULL;
    char *ru = NULL;
    
    osip_uri_t *requri = osip_message_get_uri(ev->request);
    osip_uri_to_str(requri, &ru);

    if (ev->response)
    {
        status_code = osip_message_get_status_code(ev->response);
        reason = osip_message_get_reason_phrase(ev->response);
    }
    
    PRINT("registration_faillure! status_code = %d\n", status_code);
    switch(status_code)
    {
        case 401: // 注册鉴权
        case 407: // 呼叫请求鉴权
        {
            PRINT("401 Unauthorized!\n");
 			
            if (eXosip_add_authentication_info(sip_msg.user_name, sip_msg.user_name, sip_msg.password, NULL, NULL) < 0)
            {
                PRINT("eXosip_add_authentication_info failed!\n");
                return ADD_AUTHENTICATION_INFO_ERR;
            }
            
            eXosip_lock();
            if (eXosip_default_action(ev) < 0)
            {
                PRINT("eXosip_default_action failed!\n");
            }
            eXosip_unlock();
            
            break;
        }
        case 403:
        {
            break;
        }
        case 400:
        {
            break;
        }
        case 431: // 无法识别
        {
            PRINT("431 DevID Not Match Error!\n");
            break;
        }
        default:
        {
            break;
        }
    }
    osip_free(ru);
    
    return 0;
}

static void *pthread_get_respond(void *para)
{
	int status_code = 0;
	eXosip_event_t *ev = NULL;
    //while (sip_msg.register_flag == 0)
	while (1)
	{
		if(init_success)
		{
			//PRINT("before eXosip_event_wait\n");
			ev = eXosip_event_wait(1, 0);
			//if(ev == NULL)
				//PRINT("NULLNULL\n");
			eXosip_lock();
			eXosip_automatic_action(); 
			eXosip_unlock();
			//PRINT("after eXosip_event_wait\n");

			if (ev != NULL)
			{
				if(ev->response == NULL)
					goto FREE_EVENT;
				status_code = osip_message_get_status_code(ev->response);
				PRINT("status_code = %d\n", status_code);
				switch(ev->type)
				{
					case EXOSIP_REGISTRATION_NEW: // 一个新的注册（可能是作为服务器时使用）
					{
						PRINT("announce new registration.\n");
						break;
					}
					case EXOSIP_REGISTRATION_SUCCESS:  // 用户已经成功注册
					{
						PRINT("eXosip registeration success!\n");
						register_success(ev);                                                                                   
						break;
					}
					case EXOSIP_REGISTRATION_FAILURE: // 用户没有注册
					{
						PRINT("eXosip registeration failed!\n");
						register_failed(ev);                                                                                                         
						break;
					}
					case EXOSIP_REGISTRATION_REFRESHED: // 注册已被更新
					{
						PRINT("registration has been refreshed!\n");
						break;
					}
					case EXOSIP_REGISTRATION_TERMINATED: // UA终止注册（server）
					{
						PRINT("UA is not registred any more!\n");
						break;
					}
					/******************************************************/
					case EXOSIP_CALL_INVITE: 
					{
						PRINT("announce a new call.\n");
						break;
					}
					case EXOSIP_CALL_REINVITE: 
					{
						PRINT("announce a new INVITE within call\n");
						break;
					}
					case EXOSIP_CALL_TIMEOUT:
					case EXOSIP_CALL_NOANSWER:
					{    
						PRINT("announce no answer within the timeout!\n");
						break;
					}
					case EXOSIP_CALL_PROCEEDING: // 通过远程应用程序发布处理
					{
						PRINT("announce processing by a remote app!\n");                                                                                              
						break;
					}
					case EXOSIP_CALL_RINGING: // 回铃
					{
						PRINT("announce ringback\n");                                                                           
						break;
					}
					case EXOSIP_CALL_ANSWERED: // 表明开始呼叫
					{
						PRINT("announce start of call.\n");                                                                                       
						break;
					}
					case EXOSIP_CALL_REDIRECTED: // 表明是一个重定向
					{
						PRINT("announce a redirection.\n");
						break;
					}
					case EXOSIP_CALL_REQUESTFAILURE: // 请求失败
					case EXOSIP_CALL_SERVERFAILURE:  // 服务器故障
					case EXOSIP_CALL_GLOBALFAILURE:  // 表明全程失败
					{    
						PRINT("announce a request failure or announce a server failure or announce a global failure\n");
						break;
					}
					case EXOSIP_CALL_ACK:
					{    
						PRINT("ACK received for 200ok to INVITE.\n");
						break;
					}
					case EXOSIP_CALL_CANCELLED: // 取消呼叫
					{
						PRINT("announce that call has been cancelled.\n");                                                                                      
						break;
					}
					case EXOSIP_CALL_MESSAGE_NEW: // 表明一个新的呼入请求
					{
						PRINT("announce new incoming request.\n");                                                                                            
						break;
					}
					case EXOSIP_CALL_MESSAGE_PROCEEDING: 
					{
						PRINT("announce a 1xx for request.\n");                                                                                            
						break;
					}
					case EXOSIP_CALL_MESSAGE_ANSWERED: 
					{
						PRINT("announce a 200ok.\n");                                                                                            
						break;
					}
					case EXOSIP_CALL_MESSAGE_REDIRECTED: 
					case EXOSIP_CALL_MESSAGE_SERVERFAILURE:
					case EXOSIP_CALL_MESSAGE_GLOBALFAILURE:
					{
						PRINT("announce a failure.\n");                                                                                            
						break;
					}
					case EXOSIP_CALL_MESSAGE_REQUESTFAILURE:
					{
						PRINT("EXOSIP_CALL_MESSAGE_REQUESTFAILURE\n");
						if (ev->did < 0 && ev->response && (ev->response->status_code == 407 || ev->response->status_code == 401))
						{
							 eXosip_default_action(ev);
						}
						break;
					}
					case EXOSIP_CALL_CLOSED: // 在此呼叫中收到挂机
					{
						PRINT("a BYE was received for this call.\n");
						break;
					}
					case EXOSIP_CALL_RELEASED: // 释放
					{
						PRINT("call context is cleared.\n");                                                                                        
						break;
					}
					/******************************************************/
					case EXOSIP_MESSAGE_NEW: // 新的请求
					{
						PRINT("announce new incoming request.\n");
						other_request(ev);
						break;
					}
					case EXOSIP_MESSAGE_PROCEEDING:
					{
						PRINT("announce a 1xx for request.\n");                                                                                            
						break;
					}
					case EXOSIP_MESSAGE_ANSWERED: 
					{
						PRINT("announce a 200ok.\n");                                                                                     
						break;
					}
					case EXOSIP_MESSAGE_REQUESTFAILURE:
					{
						PRINT("EXOSIP_MESSAGE_REQUESTFAILURE!\n");
						if (ev->response && (ev->response->status_code == 407 || ev->response->status_code == 401)){
							/*the user is expected to have registered to the proxy, thus password is known*/
							eXosip_default_action(ev);
						}
						break;
					}
					case EXOSIP_MESSAGE_REDIRECTED: 
					case EXOSIP_MESSAGE_SERVERFAILURE:
					case EXOSIP_MESSAGE_GLOBALFAILURE:
					{
						PRINT("announce a failure.\n");                                                                                            
						break;
					}
					/******************************************************/
					case EXOSIP_SUBSCRIPTION_UPDATE:
					{
						PRINT("announce incoming SUBSCRIBE.\n");
						break;
					}
					case EXOSIP_SUBSCRIPTION_CLOSED:
					{
						   PRINT("announce end of subscription.\n");
						break;
					}
					case EXOSIP_SUBSCRIPTION_NOANSWER:
					{
						   PRINT("announce no answer.\n");
						break;
					}
					case EXOSIP_SUBSCRIPTION_PROCEEDING:
					{
						PRINT("announce a 1xx.\n");
						break;
					}
					case EXOSIP_SUBSCRIPTION_ANSWERED:
					{
						PRINT("announce a 200ok.\n");
						break;
					}
					case EXOSIP_SUBSCRIPTION_REDIRECTED:
					{
						PRINT("announce a redirection.\n");
						break;
					}
					case EXOSIP_SUBSCRIPTION_REQUESTFAILURE: 
					case EXOSIP_SUBSCRIPTION_SERVERFAILURE: 
					case EXOSIP_SUBSCRIPTION_GLOBALFAILURE: 
					{
						PRINT("announce a request failure or announce a server failure or announce a global failure.\n");
						break;
					}
					case EXOSIP_SUBSCRIPTION_NOTIFY:
					{
						   PRINT("announce new NOTIFY request.\n");
						break;
					}
					case EXOSIP_SUBSCRIPTION_RELEASED:
					{
						PRINT("call context is cleared.\n");
						break;
					}     
					case EXOSIP_IN_SUBSCRIPTION_NEW:
					{
						PRINT("announce new incoming SUBSCRIBE.\n");
						break;
					}
					case EXOSIP_IN_SUBSCRIPTION_RELEASED:
					{
						PRINT("announce end of subscription.\n");
						break;
					}
					case EXOSIP_NOTIFICATION_NOANSWER:
					{
						PRINT("announce no answer.\n");
						break;
					}
					case EXOSIP_NOTIFICATION_PROCEEDING:
					{
						PRINT("announce a 1xx.\n");
						break;
					}
					case EXOSIP_NOTIFICATION_ANSWERED:
					{
						PRINT("announce a 200ok.\n");
						break;
					}
					case EXOSIP_NOTIFICATION_REDIRECTED:
					{
						PRINT("announce a redirection.\n");
						break;
					}
					case EXOSIP_NOTIFICATION_REQUESTFAILURE:
					case EXOSIP_NOTIFICATION_SERVERFAILURE:
					case EXOSIP_NOTIFICATION_GLOBALFAILURE:
					{
						PRINT("announce a request failure or announce a server failure or announce a global failure.\n");
						break;
					}
					case EXOSIP_EVENT_COUNT:
					{
						PRINT("MAX number of events.\n");
						break;
					}
					/******************************************************/
					default:
					{
						PRINT("Unhandled exosip event!\n");
						break;
					}
				}
FREE_EVENT:
				eXosip_event_free(ev);
			}
		}
		else
			usleep(20*1000);
    }
    return NULL;
}

/**
 * 发送数据到平台服务器
 */
static int send_message(char *buf,int buf_len)
{
    int res = 0;
    osip_message_t *options = NULL;
    //PRINT("buf = %s", buf);
    
    PRINT("sip_msg->from = %s\n", sip_msg.from);
    PRINT("sip_msg->register_uri = %s\n", sip_msg.register_uri);
    
    eXosip_options_build_request(&options, sip_msg.from, sip_msg.from, sip_msg.register_uri);
        
    PRINT("buf_len = %d\n", buf_len);

    osip_message_set_content_type(options, "application/sdp");
    osip_message_set_body(options, buf, buf_len);
      
    eXosip_lock();
    eXosip_options_send_request(options);
    eXosip_unlock();
    return 0;
}

static void * base_heartbeat_msg_manage(void* para)
{
	int res = 0;    
    //struct timeval tv;
    //memset(&tv, 0, sizeof(struct timeval));
    
    char heartbeat_buf[256] = {0};
    char send_buf[4096] = {0};
    
    while (1)
    {
        // 心跳
        //gettimeofday(&tv, NULL);
        
        sprintf(heartbeat_buf,"{id:%s,dev_type:1,ip:%s}", sip_msg.base_sn,sip_msg.localip);            
        
        snprintf(send_buf, sizeof(send_buf),
            "v=0\r\n"
            "o=%s\r\n"
            "s=conversation\r\n"
            "i=2\r\n"
            "c=%s\r\n",sip_msg.base_sn, heartbeat_buf);
            
        pthread_mutex_lock(&send_mutex);
        // 发送
        if ((res = send_message(send_buf,strlen(send_buf))) < 0)
        {
            PRINT("send_message failed!\n");
        }
        else
        {
            PRINT("send heartbeat msg successful!\n");    
        }
        pthread_mutex_unlock(&send_mutex);
        
        memset(heartbeat_buf, 0, sizeof(heartbeat_buf));
        //memset(&tv, 0, sizeof(struct timeval));
        memset(send_buf, 0, sizeof(send_buf));
        
        sleep(OPTION_DELAY);
    }
    return (void *)res;

}

static void * base_state_msg_manage(void* para)
{
    int i,res = 0;
    char base_state_buf[4096] = {0};
    char send_buf[4096] = {0};
	int msg_len = 0;
	int tmp_len = 0;
	int counts = 0;
    while (1)
    {
#ifdef BASE_9344
		if(counts%1 == 0)
#elif defined(BASE_A20)
		if(counts%3 == 0)
#endif
		{
			memset(base_state_buf, 0, sizeof(base_state_buf));
			// base状态监控信息
			if ((res = get_base_state(base_state_buf, &msg_len)) < 0)
			{
				PRINT("get_base_state failed!\n");
				goto NEXT;
			}
			if(res == 1)
			{
				PRINT("CPU is busy\n");
				snprintf(send_buf, sizeof(send_buf),
					"v=0\r\n"
					"o=%s\r\n"
					"s=conversation\r\n"
					"i=1\r\n"
					"c=%s", sip_msg.base_sn,base_state_buf);

				pthread_mutex_lock(&send_mutex);
				// 发送
				if ((res = send_message(send_buf,strlen(send_buf))) < 0)
				{
					PRINT("send_message failed!\n");
				}
				else
				{
					PRINT("send waring base_state msg successful!\n");    
				}
				pthread_mutex_unlock(&send_mutex);
			}
		}
		if(counts%(STATUS_DELAY) == 0)
		{
			snprintf(send_buf, sizeof(send_buf),
				"v=0\r\n"
				"o=%s\r\n"
				"s=conversation\r\n"
				"i=3\r\n"
				"c=%s", sip_msg.base_sn,base_state_buf);

			pthread_mutex_lock(&send_mutex);
			// 发送
			if ((res = send_message(send_buf,strlen(send_buf))) < 0)
			{
				PRINT("send_message failed!\n");
			}
			else
			{
				PRINT("send base_state msg successful!\n");    
			}
			pthread_mutex_unlock(&send_mutex);
		}
        memset(send_buf, 0, sizeof(send_buf));
NEXT:    
#ifdef BASE_9344
		counts+=3;
#elif defined(BASE_A20)
		counts++;
#endif
		sleep(1);
    }    
    return (void *)res;
}

int sip_register()
{
    osip_message_t * msg = NULL;
    int res = 0;
    char send_buf[2048] = {0};
    char length[5] = {0};
	//sip_msg.proxy_port = SIP_PORT;
	//sip_msg.local_port = SIP_PORT;
	//memset(sip_msg.proxy_ip,0,sizeof(sip_msg.proxy_ip));
	//memcpy(sip_msg.proxy_ip,SIP_SERVER_IP,strlen(SIP_SERVER_IP));
    
    if(eXosip_guess_localip(AF_INET, sip_msg.localip, 64) < 0)
    {
		PRINT("net err\n");
		return -1;
	}                    // 本地IP
	sip_msg.expires = 3600; //1h
    //memcpy(sip_msg.localip,"192.168.4.86",strlen("192.168.4.86"));
	eXosip_clear_authentication_info();
    //sprintf(sip_msg.from, "sip:%s@%s:%d", sip_msg.user_name, sip_msg.proxy_ip, sip_msg.proxy_port);
    //sprintf(sip_msg.register_uri, "sip:%s:%d", sip_msg.proxy_ip, sip_msg.proxy_port); // 代理服务器地址
    //sprintf(sip_msg.contact, "sip:%s@%s:%d", sip_msg.user_name, sip_msg.localip, sip_msg.local_port);
    sprintf(sip_msg.from, "sip:%s@%s:%d", sip_msg.user_name, sip_msg.proxy_ip, sip_msg.proxy_port);
    sprintf(sip_msg.register_uri, "sip:%s:%d", sip_msg.proxy_ip, sip_msg.proxy_port); // 代理服务器地址
    sprintf(sip_msg.contact, "sip:%s@%s:%d", sip_msg.user_name, sip_msg.localip, sip_msg.local_port);
    eXosip_add_authentication_info(sip_msg.user_name, sip_msg.user_name, sip_msg.password, NULL, NULL);
 
    PRINT("sip_msg.from = %s\n", sip_msg.from);
    PRINT("sip_msg.register_uri = %s\n", sip_msg.register_uri);
    PRINT("sip_msg.contact = %s\n", sip_msg.contact);
    PRINT("sip_msg.expires = %d\n", sip_msg.expires);
    eXosip_lock();
    if ((sip_msg.id = eXosip_register_build_initial_register(sip_msg.from, sip_msg.register_uri, sip_msg.contact, sip_msg.expires, &msg)) < 0)
    {   
        eXosip_unlock();
        PRINT("eXosip_register_build_initial_register failed!\n");
        return INIT_REGISTER_ERR;
    }   
    PRINT("sip_msg.id = %d\n", sip_msg.id);

    snprintf (send_buf, sizeof(send_buf),
        "v=0\r\n"
        "o=%s\r\n"
        "s=conversation\r\n"
        "i=1\r\n"
        "c=%s\r\n"
        "t=0\r\n", sip_msg.base_sn, sip_msg.hint);

    //PRINT("strlen(send_buf) = %d\n", strlen(send_buf));
    snprintf(length, sizeof(length), "%d", strlen(send_buf) + 1);

    //PRINT("length = %s\n", length);

    osip_message_set_content_type(msg, "application/sdp");
    osip_message_set_body(msg, send_buf, strlen(send_buf));

    PRINT("eXosip_register_send_register = %d\n",eXosip_register_send_register(sip_msg.id, msg));
    eXosip_unlock();
    return 0;

}

int monitor_init()
{	
	if (eXosip_init() < 0)
    {
        PRINT("eXosip_init failed!\n");
        return EXOSIP_INIT_ERR;
    }
    if (eXosip_listen_addr(IPPROTO_UDP, NULL, sip_msg.proxy_port, AF_INET, 0) < 0)
    {
        eXosip_quit();
        PRINT("eXosip_listen_addr failed!\n");
        return EXOSIP_LISTEN_ERR;
    }
    PRINT("monitor_init success !\n");
	return 0;
}

int net_config()
{
	int ret = 0;
	char buf[512]={0};
	char tmp_buf[3]={0};
	int minor = 0;
	char as532_conf_version[4]={0};
	char conf_ver[32]={0};
	char *p;
	char *end;
	int fd = open("/etc/532.conf",O_RDWR);
	if(fd < 0)
	{
		printf("532.conf is not found\n");
		exit(-1);
	}
	ret = read(fd,buf,sizeof(buf));
	if(ret > 0)
	{
		p = strstr(buf,"CONF_VER=\"");
		if(p == NULL)
		{
			printf("config file err.\nfile's first line must be CONF_VER=\"Vx.x.x\"\n");
			close(fd);
			exit(-1);
		}

		p += strlen("CONF_VER=\"");
		end = strstr(p,"\"");
		if(end == NULL)
		{
			printf("config file err.\nfile's first line must be CONF_VER=\"Vx.x.x\"\n");
			close(fd);
			exit(-1);
		}
		memset(conf_ver,0,sizeof(conf_ver));
		memcpy(conf_ver,p,end-p);
		//HBD_F2B_AS532_V4.0.1
		//				012345
		p = strstr(conf_ver,"V");
		if(p == NULL)
		{
			p = strstr(conf_ver,"v");
			if(p == NULL)
			{
				printf("config file err.\nfile's first line must be CONF_VER=\"Vx.x.x\"\n");
				close(fd);
				exit(-1);
			}
		}
		as532_conf_version[0] = p[1]-48;
		as532_conf_version[1] = p[3]-48;
		//PRINT("strlen(p) = %d\n",strlen(p));
		if(strlen(p) == 6)
		{
			as532_conf_version[2] = 0x0;
			as532_conf_version[3] = p[5]-48;
			minor = as532_conf_version[3];
		}
		else if(strlen(p) == 7)
		{
			tmp_buf[0] = p[5];
			tmp_buf[1] = p[6];
			tmp_buf[2] = '\0';
			minor = atoi(tmp_buf);
			as532_conf_version[2] = minor >> 8;
			as532_conf_version[3] = minor&0xff;
		}
		PRINT("local as532 conf = %d.%d.%d\n",as532_conf_version[0],as532_conf_version[1],minor);
		
		p = strstr(buf,"SIP_SERVER_IP=\"");
		if(p == NULL)
		{
			printf("config file err.\nfile's Fourth line must be SIP_SERVER_IP=\"xxx.xxx.xxx.xxx\"\n");
			close(fd);
			exit(-1);
		}
		p += strlen("SIP_SERVER_IP=\"");
		end = strstr(p,"\"");
		if(end == NULL)
		{
			printf("config file err.\nfile's Fourth line must be SIP_SERVER_IP=\"xxx.xxx.xxx.xxx\"\n");
			close(fd);
			exit(-1);
		}

		memset(sip_msg.proxy_ip,0,sizeof(sip_msg.proxy_ip));
		memcpy(sip_msg.proxy_ip,p,end-p);
				
		close(fd);
		//printf("conf=%s\n",conf_ver);
		return 0;
	}
	close(fd);
	PRINT("config file err.\nfile's first line must be CONF_VER=\"xxx\"\n");
	PRINT("config file err.\nfile's second line must be REMOTE_SERVER_IP=\"xxx.xxx.xxx.xxx\"\n");
	PRINT("config file err.\nfile's third line must be REMOTE_SERVER_PORT=\"xxx\"\n");
	PRINT("config file err.\nfile's Fourth line must be SIP_SERVER_IP=\"xxx.xxx.xxx.xxx\"\n");
	exit(-1);	
}
/*
int get_sn()
{
	int fd = open("/opt/.f2b_info.conf",O_RDONLY);
	if(fd < 0)
	{
		PRINT("open f2b_info err\n");
		return -1;
	}
	if(read(fd,&fid,sizeof(fid))==sizeof(fid))
	{
		memset(sip_msg.base_sn,0,sizeof(sip_msg.base_sn));
		memcpy(sip_msg.base_sn,fid.sn,sizeof(fid.sn));
		close(fd);
		return 0;
	}
	close(fd);
	return -1;
}*/
/*
int get_config()
{
	char buf[512]={0};
	F2B_INFO_DATA fid;
	char *p = NULL;
	char *end = NULL;
#ifdef BASE_A20
	int fd = open("/opt/.sip_info.conf",O_RDONLY);
#elif defined(BASE_9344)
	int fd = open("/var/.sip_info.conf",O_RDONLY);
#endif
	if(fd < 0)
	{
		PRINT("open sip_config err\n");
		return -1;
	}
	memset(sip_msg.user_name,0,sizeof(sip_msg.user_name));
	memset(sip_msg.password,0,sizeof(sip_msg.password));
		
	if(read(fd,buf,sizeof(buf))>0)
	{
		//sscanf(buf,"SIP_ACCOUNT=%s\"",sip_msg.user_name);
		//sscanf(buf,"SIP_PASSWORD=%s\"",sip_msg.password);
		p = strstr(buf,"SIP_ACCOUNT=\"");
		if(p == NULL)
		{
			close(fd);
			return -1;
		}

		p += strlen("SIP_ACCOUNT=\"");
		end = strstr(p,"\"");
		if(end == NULL)
		{
			close(fd);
			return -1;
		}
		memcpy(sip_msg.user_name,p,end-p);
		
		p = strstr(buf,"SIP_PASSWORD=\"");
		if(p == NULL)
		{
			close(fd);
			return -1;
		}

		p += strlen("SIP_PASSWORD=\"");
		end = strstr(p,"\"");
		if(end == NULL)
		{
			close(fd);
			return -1;
		}
		memcpy(sip_msg.password,p,end-p);

	}
	PRINT("SIP_ACCOUNT = %s\n",sip_msg.user_name);	
	PRINT("SIP_PASSWORD = %s\n",sip_msg.password);
	
	close(fd);
	//net_config();
	return 0;
}
*/

int create_init_client()
{
	struct sockaddr_in cliaddr;
    struct timeval timeo = {0};   
    timeo.tv_sec = 5;
	socklen_t len = sizeof(timeo);  
	init_server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(init_server_fd < 0)
	{
		PRINT("create client err.\n");
		return -1;
	}
	setsockopt(init_server_fd, SOL_SOCKET, SO_SNDTIMEO, &timeo, len);
	cliaddr.sin_family = AF_INET;
	inet_pton(AF_INET,INIT_SERVER_IP,&cliaddr.sin_addr);//服务器ip
	cliaddr.sin_port = htons(INIT_PORT);//注意字节顺序
	if(connect(init_server_fd, (struct sockaddr*)&cliaddr, sizeof(cliaddr))<0)
	{
		if (errno == EINPROGRESS) 
		{  
			PRINT("connect ip:%s port:%d timeout.\n",INIT_SERVER_IP,INIT_PORT);
			return -2;  
		}         
		PRINT("connect ip:%s port:%d err.\n",INIT_SERVER_IP,INIT_PORT);
		return -3;
	}
	PRINT("connected ip:%s port:%d.\n",INIT_SERVER_IP,INIT_PORT);
	return 0;
}

int generate_init_msg()
{
	struct init_request i_req;
	i_req.head.ver = 0x01;
#ifdef BASE_A20
	i_req.head.cmd = htons(0x1000);
	i_req.head.len = htons(sizeof(i_req));
#elif defined(BASE_9344)
	i_req.head.cmd = (0x1000);
	i_req.head.len = (sizeof(i_req));
#endif
	memset(i_req.type,0,sizeof(i_req.type));	
#ifdef BASE_A20
	i_req.type[0] = htons(0x0002);
#elif defined(BASE_9344)
	i_req.type[0] = (0x0002);
#endif
	memset(i_req.id,0,sizeof(i_req.id));
	memcpy(i_req.id,sip_msg.base_sn,strlen(sip_msg.base_sn));
#ifdef BASE_A20
	i_req.reserve = htons(0x00);
#elif defined(BASE_9344)
	i_req.reserve = (0x00);
#endif
	i_req.xor = sumxor((unsigned char *)&i_req,sizeof(i_req)-1);

	if(write(init_server_fd,&i_req,sizeof(i_req)) == sizeof(i_req))
		return 0;
	else
		return -1;
}

int prase_init_msg(char *msg)
{
	char sip_buf[129]={0};
	char port[16] = {0};
	int i;
	struct init_respond i_res;
	unsigned char xor = 0;
	memset(&i_res,0,sizeof(i_res));
	memcpy(&i_res,msg,sizeof(i_res));
#ifdef BASE_A20
	i_res.head.cmd = ntohs(i_res.head.cmd);
	i_res.head.len = ntohs(i_res.head.len);
	i_res.type[0] = ntohs(i_res.type[0]);
	i_res.result = ntohs(i_res.result);
#elif defined(BASE_9344)
	i_res.head.cmd = (i_res.head.cmd);
	i_res.head.len = (i_res.head.len);
	i_res.type[0] = (i_res.type[0]);
	i_res.result = (i_res.result);
#endif
	//printf("sizeof(i_res) = %d\n",sizeof(i_res));
	//printf("i_res.head.ver = 0x%04x\n",i_res.head.ver);
	//printf("i_res.head.cmd = 0x%04x\n",i_res.head.cmd);
	//printf("i_res.head.len = 0x%04x\n",i_res.head.len);
	//printf("i_res.type[0] = 0x%04x\n",i_res.type[0]);
	xor = sumxor(msg,sizeof(i_res)-1);
	PRINT("xor = %d\n",xor);
	PRINT("i_res.xor = %d\n",i_res.xor);
	if(xor != i_res.xor)
	{
		PRINT("xor err\n");
		return -1;
	}
	if(i_res.head.ver != 0x01)
	{
		PRINT("ver err\n");
		return -2;
	}
	if(i_res.head.cmd != 0x1001)
	{
		PRINT("cmd err\n");
		return -3;
	}
	if(i_res.type[0] != 0x0002)
	{
		PRINT("type err\n");
		return -4;
	}
	if(memcmp(i_res.id,sip_msg.base_sn,strlen(sip_msg.base_sn)))
	{
		PRINT("id err\n");
		return -5;
	}
	if(i_res.result != 0x00)
	{
		PRINT("result err\n");
		return -6;
	}
	memcpy(sip_buf,i_res.msg,sizeof(i_res)-46);
	//PRINT("%s\n",sip_buf);
	for(i=0;i<strlen(sip_buf);i++)
	{
		if(sip_buf[i]==',')
			sip_buf[i] = ' ';
	}
	memset(sip_msg.user_name,0,sizeof(sip_msg.user_name));
	memset(sip_msg.password,0,sizeof(sip_msg.password));
	memset(sip_msg.proxy_ip,0,sizeof(sip_msg.proxy_ip));
	sscanf(sip_buf,"username:%s password:%s serverip:%s serverport:%s",sip_msg.user_name,sip_msg.password,sip_msg.proxy_ip,port);
	if(strlen(sip_msg.user_name) == 0 || strlen(sip_msg.password) == 0 || strlen(sip_msg.proxy_ip) == 0 || strlen(port) == 0)
	{
		PRINT("return user info err\n");
		sip_i.sip_status = FAIL;
		return -7;
	}
	sip_msg.proxy_port = atoi(port);
	sip_msg.local_port = atoi(port);
	sscanf(sip_buf,"%s %s %s %s",sip_i.user_name,sip_i.password,sip_i.proxy_ip,sip_i.proxy_port);
	sip_i.sip_status = SUCCESS;
	new_sip_info = 1;
	return 0;
}

int get_init_request()
{
	int loops = 0;
	char recvbuf[2048]={0};
	int recv_ret = 0;
	int i;
	int length = 0;
	do
	{
		loops++;
		usleep(50*1000);
		if(loops >= 100)
		{
			PRINT("get remote ver request timeout\n");
			return -8;
		}
		recv_ret = recv(init_server_fd,recvbuf,2048,MSG_DONTWAIT);
		if(recv_ret > 0 )
		{
			PRINT("recv_ret: %d\n",recv_ret);			
			ComFunPrintfBuffer(recvbuf,recv_ret);
			//PRINT("recv: %s\n",recvbuf);
			if(recv_ret > 5)
				return prase_init_msg(recvbuf);
		}
		else
		{
			if(errno == EAGAIN)
			{
				continue;
			}
			break;
		}
	}while(1);
	return -9;
}

int get_sip_info()
{
#ifdef BASE_A20
	int fd = open("/opt/.sip_info.conf",O_RDONLY);
#elif defined(BASE_9344)
	int fd = open("/var/.sip_info.conf",O_RDONLY);
#endif
	if(fd < 0)
	{
		printf("open sip_info err\n");
		return -1;
	}
	memset(&sip_i,0,sizeof(sip_i));
	if(read(fd,&sip_i,sizeof(sip_i))==sizeof(sip_i))
	{
		set_buf((char *)&sip_i,sizeof(sip_i));
		PRINT("sip_i.sip_status = 0x%0x\n",sip_i.sip_status);
		if(sip_i.sip_status == SUCCESS)
		{
			//PRINT("sip_i.user_name = %s\n",sip_i.user_name);
			//PRINT("sip_i.password = %s\n",sip_i.password);
			//PRINT("sip_i.serverip = %s\n",sip_i.proxy_ip);
			//PRINT("sip_i.serverport = %s\n",sip_i.proxy_port);
			if(strncmp(sip_i.user_name,"username:",strlen("username:")) 
			|| strncmp(sip_i.password,"password:",strlen("password:"))
			|| strncmp(sip_i.proxy_ip,"serverip:",strlen("serverip:"))
			|| strncmp(sip_i.proxy_port,"serverport:",strlen("serverport:")))
			{
				PRINT("config err\n");
				close(fd);
				return -3;
			}
			memcpy(sip_msg.user_name,sip_i.user_name+strlen("username:"),sizeof(sip_msg.user_name));
			memcpy(sip_msg.password,sip_i.password+strlen("password:"),sizeof(sip_msg.password));
			memcpy(sip_msg.proxy_ip,sip_i.proxy_ip+strlen("serverip:"),sizeof(sip_msg.proxy_ip));
			sip_msg.proxy_port = atoi(sip_i.proxy_port+strlen("serverport:"));
			sip_msg.local_port = sip_msg.proxy_port;
			close(fd);
			return 0;
		}
	}
	close(fd);
	return -2;
}

int system_init()
{
	int ret = 0;
	
	if(get_sn() == 0)
	{
		memcpy(sip_msg.base_sn,base_sn,sizeof(sip_msg.base_sn));
		if(get_sip_info() != 0)
		{
			if(create_init_client()==0)
			{
				if(generate_init_msg()==0)
				{
					ret = get_init_request();
					if(ret == 0)
					{
						close(init_server_fd);
						return 0;
					}
					PRINT("ret = %d\n",ret);
				}
				close(init_server_fd);
				return -3;
			}
			return -2;
		}
		return 0;
	}
	return -1;
}

#ifdef BASE_A20
static void *pthread_update(void *para)
{
	update("monitor","HBD_F2B_MONITOR_V1.0.1_20140618","HBD_F2B_MONITOR","1.0.1",60*60);
}
#endif

int main(int argc,char **argv)
{
	pthread_t pthread_get_respond_id;
	pthread_t pthread_base_state_id;
	pthread_t pthread_heartbeat_id;
#ifdef BASE_A20
	pthread_t pthread_update_id;
#endif
	int fd,init_flag = 0;
#ifdef BASE_A20
	pthread_create(&pthread_update_id, NULL, &pthread_update,NULL);
#endif
	pthread_create(&pthread_get_respond_id, NULL, &pthread_get_respond,NULL);
    pthread_mutex_init(&send_mutex, NULL);
	while(1)
	{
		if(system_init() == 0)
		{
			PRINT("system init success\n");
			if(monitor_init() == 0)
			{
				init_success = 1;
			}
			else
				goto MAIN_NEXT;
		}
		else
		{
			PRINT("system init err\n");
			goto MAIN_NEXT;
		}
		if(sip_register()<0)
		{
			eXosip_quit();
			PRINT("sip_register failed\n");
			goto MAIN_NEXT;
		}
		sleep(5);
		if(sip_msg.register_flag == 1)
		{		
			PRINT("register success\n");
			if(new_sip_info == 1)
			{
#ifdef BASE_A20
				fd = open("/opt/.sip_info.conf",O_RDWR|O_CREAT|O_TRUNC,0600);
#elif defined(BASE_9344)
				fd = open("/var/.sip_info.conf",O_RDWR|O_CREAT|O_TRUNC,0600);
#endif
				if(fd > 0)
				{
					set_buf((char *)&sip_i,sizeof(sip_i));
					write(fd,&sip_i,sizeof(sip_i));
					close(fd);
					PRINT("new sip info\n");
					break;
				}
				new_sip_info = 0;
			}
			break;
		}
		else
		{
#ifdef BASE_A20
			fd = open("/opt/.sip_info.conf",O_RDWR|O_CREAT|O_TRUNC,0600);
#elif defined(BASE_9344)
			fd = open("/var/.sip_info.conf",O_RDWR|O_CREAT|O_TRUNC,0600);
#endif
			if(fd > 0)
			{
				sip_i.sip_status = FAIL;
				set_buf((char *)&sip_i,sizeof(sip_i));
				write(fd,&sip_i,sizeof(sip_i));
				close(fd);
			}
			eXosip_quit();
			PRINT("register_flag failed\n");
			goto MAIN_NEXT;
		}
MAIN_NEXT:
		init_success = 0;
		sleep(STATUS_DELAY);
	}
	if(sip_msg.register_flag == 1)
	{
		pthread_create(&pthread_base_state_id, NULL,&base_state_msg_manage, NULL);
		pthread_create(&pthread_heartbeat_id, NULL,&base_heartbeat_msg_manage, NULL);
		pthread_join(pthread_heartbeat_id, NULL);
		pthread_join(pthread_get_respond_id, NULL);
		pthread_join(pthread_base_state_id, NULL);
	}
#ifdef BASE_A20
 	pthread_join(pthread_update_id, NULL);
#endif
    pthread_mutex_destroy(&send_mutex);
	return 0;
}
