/*************************************************************************
    > File Name: cacm_tools.c
    > Author: yangjilong
    > mail1: yangjilong65@163.com
    > mail2: yang.jilong@handaer.com
    > Created Time: 2013年10月30日 星期三 10时32分40秒
**************************************************************************/
#include "cacm_tools.h"


/**
 * 初始化注册
 */
static int init_register(struct s_cacm *cacm)
{
    osip_message_t * msg = NULL;
    int res = 0;
    char send_buf[2048] = {0};
    char length[5] = {0};
    
    eXosip_clear_authentication_info();
    /*
    if (eXosip_add_authentication_info(cacm->user_name, cacm->user_name, cacm->password, NULL, NULL) < 0)
    {
        PERROR("eXosip_add_authentication_info failed!\n");
        return ADD_AUTHENTICATION_INFO_ERR;
    }
    */
    
    PRINT("cacm->from = %s\n", cacm->from);
    PRINT("cacm->register_uri = %s\n", cacm->register_uri);
    PRINT("cacm->contact = %s\n", cacm->contact);
    PRINT("cacm->expires = %d\n", cacm->expires);
    eXosip_lock();
    if ((cacm->id = eXosip_register_build_initial_register(cacm->from, cacm->register_uri, cacm->contact, cacm->expires, &msg)) < 0)
    {
        eXosip_unlock();
        PERROR("eXosip_register_build_initial_register failed!\n");
        return INIT_REGISTER_ERR;
    }
    PRINT("after eXosip_register_build_initial_register! cacm->id = %d\n", cacm->id);
    
    snprintf (send_buf, sizeof(send_buf), 
        "v=0\r\n" 
        "o=%s\r\n" 
        "s=conversation\r\n" 
        "i=1\r\n"
        "c=%s\r\n"
        "t=0\r\n", cacm->base_sn, cacm->hint); 
    
    PRINT("strlen(send_buf) = %d\n", strlen(send_buf));
    snprintf(length, sizeof(length), "%d", strlen(send_buf) + 1);
    
    PRINT("length = %s\n", length);
    
    osip_message_set_content_type(msg, "application/sdp");
    PRINT("after osip_message_set_content_type\n");
    osip_message_set_body(msg, send_buf, strlen(send_buf));
    PRINT("after osip_message_set_body\n");
    
    eXosip_register_send_register(cacm->id, msg);
    PRINT("after eXosip_register_send_register!\n");
    eXosip_unlock();
    return 0;
}

/**
 * 初始化注销
 */
static int init_logout(struct s_cacm *cacm)
{
    int res = 0;
    char send_buf[2048] = {0};
    osip_message_t * msg = NULL;
    
    memset(cacm->hint, 0, sizeof(cacm->hint));
    snprintf(cacm->hint, sizeof(cacm->hint), "(%s) Off to the server (%s)!", cacm->base_sn, cacm->proxy_ip);
    cacm->expires = 0;
    
    PRINT("cacm->expires = %d\n", cacm->expires);
    eXosip_clear_authentication_info();
    
    eXosip_lock();
    if (eXosip_register_build_register(cacm->id, cacm->expires, &msg) < 0)
    {
        eXosip_unlock();
        PERROR("eXosip_register_build_register failed!\n");
        return INIT_LOGOUT_ERR;
    }
    PRINT("after eXosip_register_build_register! cacm->id = %d\n", cacm->id);
    
    /*
    snprintf (send_buf, sizeof(send_buf), 
        "v=0\r\n" 
        "o=%s\r\n" 
        "s=conversation\r\n" 
        "i=1\r\n"
        "c=%s\r\n"
        "t=0\r\n", cacm->base_sn, cacm->hint); 
        
    osip_message_set_body(msg, send_buf, strlen(send_buf));
    osip_message_set_content_type(msg, "application/sdp");
    */
    eXosip_register_send_register(cacm->id, msg);
    PRINT("after eXosip_register_send_register!\n");
    eXosip_unlock();
    return 0;
}

/**
 * 发送数据到平台服务器
 */
static int send_message(struct s_cacm *cacm, char *buf)
{
    int res = 0;
    osip_message_t *options = NULL;
    char length[5] = {0};
    PRINT("buf = %s", buf);
    
    PRINT("cacm->from = %s\n", cacm->from);
    PRINT("cacm->register_uri = %s\n", cacm->register_uri);
    
    eXosip_options_build_request(&options, cacm->from, cacm->from, cacm->register_uri);
    
    snprintf(length, sizeof(length), "%d", strlen(buf));
    
    PRINT("length = %s\n", length);
    
    osip_message_set_content_type(options, "application/sdp");
    osip_message_set_body(options, buf, strlen(buf));
    
    
    eXosip_lock();
    eXosip_options_send_request(options);
    eXosip_unlock();
    return 0;
}



#if SPI_RECV_PHONE_STATE == 1// spi中读取数据
/**
 * 接收电话线状态包
 */
static int recv_phone_state_msg(char *buf, unsigned short len, struct timeval *timeout)
{
    int res = 0;
    
    while (1)
    {
        if ((res = spi_rt_interface.recv_data(UART1, &buf[0], 1)) < 0)
        {
            PERROR("recv_data failed!\n");
            return res;
        }
        
        if ((unsigned char)buf[0] == 0xAA)
        {
            PRINT("recv head success!\n");
            break;
        }
    }
    
    if ((res = spi_rt_interface.recv_data(UART1, buf + 1, 3)) < 0)
    {
        PERROR("recv_data failed!\n");
        return res;
    }
    PRINT_BUF_BY_HEX(buf, NULL, 4, __FILE__, __FUNCTION__, __LINE__);
    return 0;
}

#else // 本地socket中读取数据
/**
 * 接收电话线状态包
 */
static int recv_phone_state_msg(int fd, char *buf, unsigned short len, struct timeval *timeout)
{
    int res = 0;
    buf[0] = 0x5A;
    buf[1] = 0xA5;
    
    if ((res = common_tools.recv_data_head(fd, buf, 2, timeout)) < 0)
    {
        PERROR("recv_data_head failed!\n");
        return res;
    }
    
    if ((res = common_tools.recv_data(fd, buf + 2, NULL, 4, timeout)) < 0)
    {
        PERROR("recv_data failed!\n");
        return res;
    }    
    
    return 0;
}
#endif

/**
 * 接收电话线状态包
 */
int phone_state_msg_unpack(struct s_cacm *cacm, char *buf, unsigned short buf_len)
{
    struct timeval tv;
    memset(&tv, 0, sizeof(struct timeval));    
    unsigned char phone_state_type = buf[4];
    unsigned char warn_type = 0;
    unsigned char warn_flag = 0;
    PRINT_BUF_BY_HEX(buf, NULL, 6, __FILE__, __FUNCTION__, __LINE__);
    
    // 判断电话线状态
    switch (phone_state_type)
    {
        case 0x51: // 电话线拔出
        {
            warn_type = 1;
            warn_flag = 1;
            PRINT("telephoneline extracting!\n");
            break;
        }
        case 0x52: // 电话线插入
        {
            warn_type = 2; // 插入
            warn_flag = 1;
            PRINT("telephoneline is inserting!\n");
            break;
        }        
        case 0x53: // 并机摘机
        {
            warn_type = 3;
            warn_flag = 1;
            PRINT("telephoneline is combining!\n");
            break;
        }
        case 0x54: // 正常摘机
        {
            PRINT("telephoneline is normal!\n");
            break;
        }
        case 0x55: // 正常挂机
        {
            PRINT("on-hook normal!\n");
            break;
        }
        case 0x56: // 振铃信号
        {
            PRINT("telephone is ringing!\n");
            break;
        }
        default:
        {
            PERROR("option is not mismatching!\n");
            return MISMATCH_ERR;
        }
    }
    
    if (warn_flag == 1)
    {
        gettimeofday(&tv, NULL);
        snprintf(buf, buf_len, "{deviceType:1,deviceId:%s,businessType:1,warnType:%d,warnTime:%d000,warnInfo:%d}", 
            cacm->base_sn, warn_type, tv.tv_sec, phone_state_type);
    }
    else 
    {
        PERROR("option is not mismatching!\n");
        return MISMATCH_ERR;
    }
    cacm->phone_state_type = phone_state_type;
    return 0;
}

#if SPI_RECV_PHONE_STATE == 1
/**
 * 监听电话线状态
 */
static int phone_state_monitor(struct s_cacm *cacm, char *buf, unsigned short buf_len, struct timeval *timeout)
{
    int res = 0;      
    
    if ((res = recv_phone_state_msg(buf, buf_len, timeout)) < 0)
    {    
        PERROR("recv_phone_state_msg failed!\n");
        return res;
    }
    PRINT_BUF_BY_HEX(buf, NULL, 4, __FILE__, __FUNCTION__, __LINE__);
    if ((res = phone_state_msg_unpack(cacm, buf, buf_len)) < 0)
    {
        PERROR("phone_state_msg_unpack failed!\n");
        return res;
    }
    
    return 0;
}
#else

/**
 * 监听电话线状态
 */
static int phone_state_monitor(struct s_cacm *cacm, char *buf, unsigned short buf_len, struct timeval *timeout)
{
    int res = 0;
    int client_fd = 0;
    fd_set fdset;
    
    struct timeval tv;
    struct sockaddr_un client; 
    memset(&tv, 0, sizeof(struct timeval));
    
    socklen_t sock_len = sizeof(client); 
    
    FD_ZERO(&fdset);
    FD_SET(cacm->fd, &fdset);       
    
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    switch(select(cacm->fd + 1, &fdset, NULL, NULL, &tv))
    {
        case -1:
        {
            PERROR("select null!\n");
            return SELECT_ERR;
        }
        case 0:
        {
            PERROR("select null!\n");
            return SELECT_NULL_ERR;
        }
        default:
        {
            if (FD_ISSET(cacm->fd, &fdset) > 0)
            {
                if ((client_fd = accept(cacm->fd, (struct sockaddr*)&client, &sock_len)) < 0)
                {    
                    PERROR("accept failed!\n");
                    return ACCEPT_ERR;
                }             
                          
                if ((res = recv_phone_state_msg(client_fd, buf, buf_len, timeout)) < 0)
                {    
                    PERROR("recv_phone_state_msg failed!\n");
                    return res;
                }
                
                if ((res = phone_state_msg_unpack(cacm, buf, buf_len)) < 0)
                {
                    PERROR("phone_state_msg_unpack failed!\n");
                    return res;
                }
            }
        }
    }
    return 0;
}

#endif

/**
 * 获取当前cpu使用情况
 */
static int get_cpu_occupy_info(struct s_cpu_info *cpu_info)
{
    int res = 0;
    char buf[64] = {0};
    char *cmd = "top -n 1 | grep CPU: | sed '/grep/'d | awk '{print $2,$4,$6,$8}'";
    
    if ((res = common_tools.get_cmd_out(cmd, buf, sizeof(buf))) < 0)
    {
        PERROR("get_cmd_out failed!\n");
        return res;
    }
    
    sscanf(buf, "%u %u %u %u", &cpu_info->user, &cpu_info->nice, &cpu_info->system, &cpu_info->idle);
    return 0;
}

/**
 * 获取当前内存使用情况
 */
static int get_mem_occupy_info(struct s_mem_info *mem_info)
{
    int res = 0;
    char buf[64] = {0};
    char *cmd = "top -n 1 | grep Mem: | sed '/grep/'d | awk '{print $2}'";
    
    if ((res = common_tools.get_cmd_out(cmd, buf, sizeof(buf))) < 0)
    {
        PERROR("get_cmd_out failed!\n");
        return res;
    }
    buf[strlen(buf) - 1] = '\0';
    
    mem_info->used = atoi(buf);    
    return 0;
}

/**
 * 获取当前各进程cpu和内存使用情况
 */
static int get_process_resource_occupy_info(char *process_info_buf, unsigned short buf_len)
{
    PRINT("get_process_resource_occupy_info entry!\n");
    int res = 0;
    int i = 0;
    char *tmp = NULL;
    char buf[2048] = {0};
    char process_info_tmp[256] = {0};
    struct s_process_info process_info;
    memset(&process_info, 0, sizeof(struct s_process_info));    
    char *cmd = "top -b -n 1 | sed '1,4d' | awk '{print $8,$6,$7}' | awk '{if ($0 != line) print;line = $0}'";
    if ((res = common_tools.get_cmd_out(cmd, buf, sizeof(buf))) < 0)
    {
        PERROR("get_cmd_out failed!\n");
        return res;
    }
    PRINT("after get_cmd_out!\n");
    //buf[strlen(buf) - 1] = '\0';
    while (1)
    {
        if ((tmp = strstr(buf + i, "\n")) == NULL)
        {            
            break;
        }
        *tmp = '\0';
        tmp++;
        
        sscanf(buf + i, "%s %u %u", process_info.name, &process_info.mem, &process_info.cpu);
        
        if (i != 0)
        {
            strncat(process_info_buf, ",", 1);
        }
        
        i += strlen(buf + i);
        i++;
        
        /*
        PRINT("process_info.name = %s\n", process_info.name);
        PRINT("process_info.mem = %d\n", process_info.mem);
        PRINT("process_info.cpu = %d\n", process_info.cpu);
        */
        sprintf(process_info_tmp, "{baseBusinessName:%s,businessRamUsageRates:%.3f%%,businessCpuUsageRates:%.3f%%}", process_info.name, (double)process_info.mem, (double)process_info.cpu);
        strncat(process_info_buf, process_info_tmp, strlen(process_info_tmp));
        memset(process_info_tmp, 0, sizeof(process_info_tmp));
    }
    //PRINT("process_info_buf = %s\n", process_info_buf);
    return 0;
}

/**
 * 获取当前flash使用情况
 */
static int get_flash_occupy_info()
{    
    return 4;  
}


/**
 * 获取当前base的cpu等使用情况
 */
static int get_base_state(struct s_cacm *cacm, char *buf, unsigned short buf_len)
{
    int i = 0;
    int res = 0;
    int fd = 0;
    struct stat file_stat;
    char * file_name = "/var/linphone/log/.base_state";
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
    PRINT("_____________________\n");
    if (system("top -b -n 1 > /var/linphone/log/.base_state") < 0)
    {
        PERROR("system failed!\n");
        return SYSTEM_ERR;
    }
    PRINT("_____________________\n");
    if ((fp = fopen("/var/linphone/log/.base_state", "r")) == NULL)
    {
        PERROR("open failed!\n");
        return OPEN_ERR;
    }
    
    // mem信息
    fgets(buf_tmp, sizeof(buf_tmp), fp);
    PRINT("strlen(buf_tmp) = %d buf_tmp = %s", strlen(buf_tmp), buf_tmp);
    sscanf(buf_tmp, "%s %u %s %u %s %u %s %u %s %u %s", mem_info.name, &mem_info.used, mem_info.used_key, 
        &mem_info.free, mem_info.free_key, &mem_info.shrd, mem_info.shrd_key, &mem_info.buff, mem_info.buff_key,
        &mem_info.cached, mem_info.cached_key);
    memset(buf_tmp, 0, strlen(buf_tmp));
    
    // cpu信息
    fgets(buf_tmp, sizeof(buf_tmp), fp);
    PRINT("strlen(buf_tmp) = %d buf_tmp = %s", strlen(buf_tmp), buf_tmp);
    sscanf(buf_tmp, "%s %u %s %u %s %u %s %u %s %u %s %u %s %u %s", cpu_info.name, &cpu_info.user, cpu_info.user_key, 
        &cpu_info.system, cpu_info.system_key, &cpu_info.nice, cpu_info.nice_key, &cpu_info.idle, cpu_info.idle_key,
        &cpu_info.io, cpu_info.io_key, &cpu_info.irq, cpu_info.irq_key, &cpu_info.softirq, cpu_info.softirq_key);
    memset(buf_tmp, 0, strlen(buf_tmp));
    
    // load average 信息
    fgets(buf_tmp, sizeof(buf_tmp), fp);
    PRINT("strlen(buf_tmp) = %d buf_tmp = %s", strlen(buf_tmp), buf_tmp);
    memset(buf_tmp, 0, strlen(buf_tmp));
    PRINT("_____________________\n");
    
    // 列名
    fgets(buf_tmp, sizeof(buf_tmp), fp);
    memset(buf_tmp, 0, strlen(buf_tmp));
    
    // 进程信息
    char flag = 0;
    while (1)
    {
        if (fgets(buf_tmp, sizeof(buf_tmp), fp) == NULL)
        {
            PERROR("fgets failed!\n");
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
        //PRINT("%u %u %s %s %u %u %u %s\n", process_info.pid, process_info.ppid, process_info.user, process_info.stat, process_info.vsz, process_info.mem, process_info.cpu, process_info.name);
        if (flag == 1)
        {
            //PRINT("process_name = %s process_info.name = %s\n", process_name, process_info.name);
            if ((memcmp(process_info.name, process_name, strlen(process_name)) == 0) 
                && (memcmp(process_name, process_info.name, strlen(process_info.name)) == 0))
            {
                memset(buf_tmp, 0, strlen(buf_tmp));
                continue;
            }
            memset(process_name, 0, strlen(process_name));
            strncat(process_info_buf, ",", 1);
        }
        memcpy(process_name, process_info.name, strlen(process_info.name));
        
        sprintf(buf_tmp, "{baseBusinessName:%s,businessRamUsageRates:%.3f%%,businessCpuUsageRates:%.3f%%}", process_info.name, (double)process_info.mem, (double)process_info.cpu);
        strncat(process_info_buf, buf_tmp, strlen(buf_tmp));
        memset(buf_tmp, 0, strlen(buf_tmp));
        memset(&process_info, 0, sizeof(struct s_process_info));
        if (strlen(process_info_buf) > 700)
        {
            break;
        }
        flag = 1;
    }
    fclose(fp);
    
    // flash 使用情况
    if ((res = get_flash_occupy_info()) < 0)
    {
        PERROR("get_flash_occupy_info failed!\n");
        return res;
    }
    gettimeofday(&tv, NULL);
    sprintf(buf, "{deviceType:1,businessInfo: {baseId:%s,cpuUsageRates:%.3f,ramUsageRates:%.3f,diskLeftSpace:%dMB,time:%d000,baseBusinessList:[%s]}}",
        cacm->base_sn, (double)cpu_info.user, (double)mem_info.used, res, tv.tv_sec, process_info_buf);
    PRINT("strlen(buf) = %d\n", strlen(buf));
    return 0;
}

#if 0 

/**
 * 获取当前base的cpu等使用情况
 */
static int get_base_state(struct s_cacm *cacm, char *buf, unsigned short buf_len)
{
    int res = 0;
    struct timeval tv;
    memset(&tv, 0, sizeof(struct timeval));
    
    struct s_cpu_info cpu_info;
    memset(&cpu_info, 0, sizeof(struct s_cpu_info));
    
    struct s_mem_info mem_info;
    memset(&mem_info, 0, sizeof(struct s_mem_info));
    char process_info_buf[4096] = {0};
    
    if ((res = get_cpu_occupy_info(&cpu_info)) < 0)
    {
        PERROR("get_cpu_occupy_info failed!\n");
        return res;
    }
    
    if ((res = get_mem_occupy_info(&mem_info)) < 0)
    {
        PERROR("get_mem_occupy_info failed!\n");
        return res;
    }
    
    if ((res = get_process_resource_occupy_info(process_info_buf, sizeof(process_info_buf))) < 0)
    {
        PERROR("get_process_resource_occupy_info failed!\n");
        return res;
    }
    
    if ((res = get_flash_occupy_info()) < 0)
    {
        PERROR("get_flash_occupy_info failed!\n");
        return res;
    }
    gettimeofday(&tv, NULL);
    sprintf(buf, "{deviceType:1,businessInfo: {baseId:%s,cpuUsageRates:%.3f,ramUsageRates:%.3f,diskLeftSpace:%dMB,time:%d000,baseBusinessList:[%s]}}",
        cacm->base_sn, (double)cpu_info.user, (double)mem_info.used, res, tv.tv_sec, process_info_buf);
    return 0;
}

/**
 * 获取当前base的cpu等使用情况
 */
static int get_base_state(struct s_cacm *cacm, char *buf, unsigned short buf_len)
{
    int res = 0;
    int fd = 0;
    struct stat file_stat;
    char * file_name = "/var/linphone/log/.base_state";
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
    
    char buf_tmp[256] = {0};
    char process_info_buf[4096] = {0};
    PRINT("_____________________\n");
    if (system("top -b -n 1 > /var/linphone/log/.base_state") < 0)
    {
        PERROR("system failed!\n");
        return SYSTEM_ERR;
    }
    PRINT("_____________________\n");
    if (system("cat /var/linphone/log/.base_state | grep CPU: | sed '/grep/'d | awk '{print $2,$4,$6,$8}' > /var/linphone/log/.cpu_info") < 0)
    {
        PERROR("system failed!\n");
        return SYSTEM_ERR;
    }
    PRINT("_____________________\n");
    if (system("cat /var/linphone/log/.base_state | grep Mem: | sed '/grep/'d | awk '{print $2}' > /var/linphone/log/.mem_info") < 0)
    {
        PERROR("system failed!\n");
        return SYSTEM_ERR;
    }
    PRINT("_____________________\n");
    if (system("cat /var/linphone/log/.base_state | sed '1,4d' | awk '{print $8,$6,$7}' | awk '{if ($0 != line) print;line = $0}' > /var/linphone/log/.process_info") < 0)
    {
        PERROR("system failed!\n");
        return SYSTEM_ERR;
    }
    PRINT("_____________________\n");
    // cpu 信息
    if ((fp = fopen("/var/linphone/log/.cpu_info", "r")) == NULL)
    {
        PERROR("open failed!\n");
        return OPEN_ERR;
    }
    fgets(buf_tmp, sizeof(buf_tmp), fp);
    fclose(fp);
    
    sscanf(buf_tmp, "%u %u %u %u", &cpu_info.user, &cpu_info.nice, &cpu_info.system, &cpu_info.idle);
    memset(buf_tmp, 0, strlen(buf_tmp));
    
    // mem 信息
    if ((fp = fopen("/var/linphone/log/.mem_info", "r")) == NULL)
    {
        PERROR("open failed!\n");
        return OPEN_ERR;
    }
    fgets(buf_tmp, sizeof(buf_tmp), fp);
    fclose(fp);
    buf[strlen(buf_tmp) - 1] = '\0';
    mem_info.used = atoi(buf_tmp);
    memset(buf_tmp, 0, strlen(buf_tmp));
    
    // 各个进程 信息
    if ((fp = fopen("/var/linphone/log/.process_info", "r")) == NULL)
    {
        PERROR("open failed!\n");
        return OPEN_ERR;
    }
    
    char flag = 0;
    while (1)
    {
        if (fgets(buf_tmp, sizeof(buf_tmp), fp) == NULL)
        {
            PERROR("fgets failed!\n");
            break;
        }
        if (strlen(buf_tmp) < 6)
        {
            //PRINT("buf_tmp[0] = %02X, buf_tmp[1] = %02X, buf_tmp[2] = %02X\n", buf_tmp[0], buf_tmp[1], buf_tmp[2]);
            break;
        }
        //PRINT("strlen(buf_tmp) = %d buf_tmp = %s", strlen(buf_tmp), buf_tmp);
        buf_tmp[strlen(buf_tmp) - 1] = '\0';
        
        sscanf(buf_tmp, "%s %u %u", process_info.name, &process_info.mem, &process_info.cpu);
        memset(buf_tmp, 0, strlen(buf_tmp));
        if (flag == 1)
        {
            strncat(process_info_buf, ",", 1);
        }
        
        sprintf(buf_tmp, "{baseBusinessName:%s,businessRamUsageRates:%.3f%%,businessCpuUsageRates:%.3f%%}", process_info.name, (double)process_info.mem, (double)process_info.cpu);
        strncat(process_info_buf, buf_tmp, strlen(buf_tmp));
        memset(buf_tmp, 0, strlen(buf_tmp));
        flag = 1;
    }
    fclose(fp);
    PRINT("_____________________\n");
    // flash 使用情况
    if ((res = get_flash_occupy_info()) < 0)
    {
        PERROR("get_flash_occupy_info failed!\n");
        return res;
    }
    gettimeofday(&tv, NULL);
    sprintf(buf, "{deviceType:1,businessInfo: {baseId:%s,cpuUsageRates:%.3f,ramUsageRates:%.3f,diskLeftSpace:%dMB,time:%d000,baseBusinessList:[%s]}}",
        cacm->base_sn, (double)cpu_info.user, (double)mem_info.used, res, tv.tv_sec, process_info_buf);
    return 0;
}

/**
 * 获取当前base的cpu等使用情况
 */
int get_base_state(struct s_cacm *cacm, char *buf, unsigned short buf_len)
{
    int i = 0;
    int res = 0;
    int fd = 0;
    struct stat file_stat;
    char * file_name = "/var/linphone/log/.base_state";
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
    
    PRINT("_____________________\n");
    while (1)
    {
        if ((fp = fopen("/var/linphone/log/.base_state", "r")) == NULL)
        {
            PERROR("open failed!\n");
            return OPEN_ERR;
        }
        
        if ((fstat(fd, &file_stat)) < 0)
        {
            PERROR("fstat failed!\n");
            return FSTAT_ERR;
        }
        
        if (file_stat.st_size == 0)
        {
            usleep(100000);
            close(fd);
            memset(&file_stat, 0, sizeof(struct stat));
            continue;
        }
        break;
    }
    
    // mem信息
    fgets(buf_tmp, sizeof(buf_tmp), fp);
    PRINT("strlen(buf_tmp) = %d buf_tmp = %s", strlen(buf_tmp), buf_tmp);
    sscanf(buf_tmp, "%s %u %s %u %s %u %s %u %s %u %s", mem_info.name, &mem_info.used, mem_info.used_key, 
        &mem_info.free, mem_info.free_key, &mem_info.shrd, mem_info.shrd_key, &mem_info.buff, mem_info.buff_key,
        &mem_info.cached, mem_info.cached_key);
    memset(buf_tmp, 0, strlen(buf_tmp));
    
    // cpu信息
    fgets(buf_tmp, sizeof(buf_tmp), fp);
    PRINT("strlen(buf_tmp) = %d buf_tmp = %s", strlen(buf_tmp), buf_tmp);
    sscanf(buf_tmp, "%s %u %s %u %s %u %s %u %s %u %s %u %s %u %s", cpu_info.name, &cpu_info.user, cpu_info.user_key, 
        &cpu_info.system, cpu_info.system_key, &cpu_info.nice, cpu_info.nice_key, &cpu_info.idle, cpu_info.idle_key,
        &cpu_info.io, cpu_info.io_key, &cpu_info.irq, cpu_info.irq_key, &cpu_info.softirq, cpu_info.softirq_key);
    memset(buf_tmp, 0, strlen(buf_tmp));
    
    // load average 信息
    fgets(buf_tmp, sizeof(buf_tmp), fp);
    PRINT("strlen(buf_tmp) = %d buf_tmp = %s", strlen(buf_tmp), buf_tmp);
    memset(buf_tmp, 0, strlen(buf_tmp));
    PRINT("_____________________\n");
    
    // 列名
    fgets(buf_tmp, sizeof(buf_tmp), fp);
    memset(buf_tmp, 0, strlen(buf_tmp));
    
    // 进程信息
    char flag = 0;
    while (1)
    {
        if (fgets(buf_tmp, sizeof(buf_tmp), fp) == NULL)
        {
            PERROR("fgets failed!\n");
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
        //PRINT("%u %u %s %s %u %u %u %s\n", process_info.pid, process_info.ppid, process_info.user, process_info.stat, process_info.vsz, process_info.mem, process_info.cpu, process_info.name);
        if (flag == 1)
        {
            //PRINT("process_name = %s process_info.name = %s\n", process_name, process_info.name);
            if ((memcmp(process_info.name, process_name, strlen(process_name)) == 0) 
                && (memcmp(process_name, process_info.name, strlen(process_info.name)) == 0))
            {
                memset(buf_tmp, 0, strlen(buf_tmp));
                continue;
            }
            memset(process_name, 0, strlen(process_name));
            strncat(process_info_buf, ",", 1);
        }
        memcpy(process_name, process_info.name, strlen(process_info.name));
        
        sprintf(buf_tmp, "{baseBusinessName:%s,businessRamUsageRates:%.3f%%,businessCpuUsageRates:%.3f%%}", process_info.name, (double)process_info.mem, (double)process_info.cpu);
        strncat(process_info_buf, buf_tmp, strlen(buf_tmp));
        memset(buf_tmp, 0, strlen(buf_tmp));
        memset(&process_info, 0, sizeof(struct s_process_info));
        if (strlen(process_info_buf) > 700)
        {
            break;
        }
        flag = 1;
    }
    fclose(fp);
    
    // flash 使用情况
    if ((res = get_flash_occupy_info()) < 0)
    {
        PERROR("get_flash_occupy_info failed!\n");
        return res;
    }
    gettimeofday(&tv, NULL);
    sprintf(buf, "{deviceType:1,businessInfo: {baseId:%s,cpuUsageRates:%.3f,ramUsageRates:%.3f,diskLeftSpace:%dMB,time:%d000,baseBusinessList:[%s]}}",
        cacm->base_sn, (double)cpu_info.user, (double)mem_info.used, res, tv.tv_sec, process_info_buf);
    PRINT("strlen(buf) = %d\n", strlen(buf));
    return 0;
}

#endif

struct class_cacm_tools cacm_tools = 
{
    init_register, init_logout, send_message, 
    recv_phone_state_msg, phone_state_msg_unpack, 
    phone_state_monitor, get_cpu_occupy_info, get_mem_occupy_info,
    get_process_resource_occupy_info, get_flash_occupy_info, get_base_state
};
