#include "communication_serial.h"

static pthread_mutex_t mutex;                                  // 互斥量
static pthread_cond_t cond;                                    // 条件量
static struct s_data_list g_data_list;

/**
 * 发送挂机命令
 */
static int cmd_on_hook();

/**
 * 发送挂机命令
 */
static int cmd_call(char *phone);

/**
 * 摘机响应
 */
static int cmd_off_hook();

/**
 * 接收来电显示
 */
static int recv_display_msg(char *phone_num);

/**
 * 接收振铃
 */
static int refuse_to_answer(); // 拒接接听

static unsigned int spi_node_fd = 0; // spi节点描述符

/**
 * 初始化
 */
static int init();

/**
 * 释放
 */
static int release();

/**
 * 发送
 */
static int send_data(char *data, unsigned int data_len, struct timeval *tv);

/**
 * 接收
 */
static int recv_data(char *data, unsigned int data_len, struct timeval *tv);

/**
 * 继电器切换
 */
static int relay_change(unsigned char mode);

/**
 * 回环控制
 */
static int loop_manage(unsigned char mode);

/**
 * 电话线检测
 */
static int phone_state_monitor(unsigned char cmd);
    
struct class_communication_serial communication_serial = 
{   
    0x76,
    &mutex, &cond,
    
    cmd_on_hook, cmd_call, cmd_off_hook, recv_display_msg,refuse_to_answer,
    phone_state_monitor, send_data, recv_data,
    relay_change,
    loop_manage,
};

/**
 * 发送挂机命令
 */
int cmd_on_hook()
{
    PRINT_STEP("entry...\n"); 
    printf("\n");
    int i = 0, j = 0;
    char head[2] = {0};
    unsigned char cmd = 0;
    char buf[5] = {0xA5, 0x5A, 0x5E, 0x00, 0x5E};
    char recv_buf[3] = {0};
    char phone_abnormal_buf[4] = {0};
    
    struct timeval tv = {5, 0};
    int res = 0;
    
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        tv.tv_sec = 5;
        if ((res = send_data(buf, sizeof(buf), &tv))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        	continue; 
        }
        
        for (i = 0; i < sizeof(head); i++)
        {
            tv.tv_sec = 5;
            if ((res = recv_data(&head[i], sizeof(head[i]), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                return res;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        tv.tv_sec = 5;
        if ((res = recv_data(&cmd, sizeof(cmd), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        
        PRINT("cmd = %02X\n", cmd); 
        if (cmd != 0xDE)
        {
            continue;
        }
        
        tv.tv_sec = 5;
        if ((res = recv_data(recv_buf, sizeof(recv_buf), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            continue;
        }
        if ((recv_buf[0] != 0x00) || (recv_buf[1] != 0x00) || ((unsigned char)recv_buf[2] != 0xDE))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data err!", DATA_ERR);
            continue;
        }
        break;
    }
    
    *(common_tools.phone_status_flag) = 0;
    release();
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 摘机
 */
int cmd_off_hook()
{
    PRINT_STEP("entry...\n");
    char head[2] = {0};
    unsigned char cmd = 0;
    char buf[6] = {0xA5, 0x5A, 0x73, 0x01, 0x01, 0x73};
    
    char recv_buf[5] = {0};
    char phone_abnormal_buf[4] = {0};
    
    struct timeval tv = {5, 0};
    int res = 0;
    int i = 0, j = 0;
    
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        tv.tv_sec = 5;
        if ((res = send_data(buf, sizeof(buf), &tv))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        	continue; 
        }
        
        *(common_tools.phone_status_flag) = 1;
        
        // A55A C1 02 02 00 00 C1
        for (i = 0; i < sizeof(head); i++)
        {
            tv.tv_sec = 5;
            if ((res = recv_data(&head[i], sizeof(head[i]), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                return res;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        tv.tv_sec = 5;
        if ((res = recv_data(&cmd, sizeof(cmd), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        
        PRINT("cmd = %02X\n", cmd);
        if (cmd != 0xC1)
        {
            continue;
        }
        
        tv.tv_sec = 5;
        if ((res = recv_data(recv_buf, sizeof(recv_buf), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        
        if ((recv_buf[0] != 0x02) || (recv_buf[1] != 0x02) || (recv_buf[2] != 0x00) || (recv_buf[3] != 0x00) || ((unsigned char)recv_buf[4] != 0xC1))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data err!", DATA_ERR);
            return DATA_ERR;
        }
        break;
    }
	if (res < 0)
	{
	    return res;
	}
    
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 发送呼叫命令
 */
int cmd_call(char *phone)
{
    PRINT_STEP("entry...\n"); 
    printf("\n");
    
    struct timeval tv = {5, 0};
    char head[2] = {0};
    unsigned char cmd = 0;
    char *send_buf = NULL;
    unsigned char *recv_buf = NULL;
    int res = 0, ret = 0;
    int i = 0, j = 0, k = 0;
    char open_reply_buf[7] = {0xA5, 0x5A, 0x70, 0x02, 0x08, 0x01, 0x7B};
    char close_reply_buf[7] = {0xA5, 0x5A, 0x70, 0x02, 0x08, 0x00, 0x7A};
    char reply_recv_buf[5] = {0};
    char phone_abnormal_buf[4] = {0};
    // 摘机
    if ((res = cmd_off_hook()) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "cmd_off_hook failed!", res);
    	return res; 
    }
    PRINT("res = %d\n", res);
    PRINT("phone = %s\n", phone);
    if ((send_buf = malloc(strlen(phone) + 7)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
    	return MALLOC_ERR; 
    }
    memset(send_buf, 0, strlen(phone) + 7);
    
    if ((recv_buf = malloc(strlen(phone) + 5)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
        free(send_buf);
        send_buf = NULL;
    	return MALLOC_ERR; 
    }
    memset(recv_buf, 0, strlen(phone) + 5);
    
    /*
    0x78:DTMF拨号命令字
    */
    send_buf[0] = 0xA5;
    send_buf[1] = 0x5A;
    send_buf[2] = 0x78;
    send_buf[3] = strlen(phone); // 包长度
    for (i = 0; i < strlen(phone); i++)
    {
        send_buf[i + 4] = phone[i] - '0';
    }
    // memcpy(send_buf + 4, phone, strlen(phone));
    send_buf[strlen(phone) + 4] = common_tools.get_checkbit(send_buf, NULL, 2, strlen(phone) + 2, XOR, 1);
    sleep(1);
    
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        tv.tv_sec = 5;
        // 打开参数回复
        if ((res = send_data(open_reply_buf, sizeof(open_reply_buf), &tv))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
         	return res; 
        }
        
        // 接收同步头
        for (i = 0; i < sizeof(head); i++)
        {
            tv.tv_sec = 5;
            if ((res = recv_data(&head[i], sizeof(head[i]), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                return res;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        tv.tv_sec = 5;
        if ((res = recv_data(&cmd, sizeof(cmd), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            continue;
        }
        PRINT("cmd = %02X\n", cmd);
        if (cmd != 0xF0)
        {
            res = DATA_ERR;
            continue;
        }
        
        tv.tv_sec = 5;
        // A5 5A F0 02 F7 FE 00 FB
        if ((res = recv_data(reply_recv_buf, sizeof(reply_recv_buf), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
            continue;
        }
        
        if ((reply_recv_buf[0] != 0x02) || ((unsigned char)reply_recv_buf[1] != 0xF7) || ((unsigned char)reply_recv_buf[2] != 0xFE) || 
            (reply_recv_buf[3] != 0x00) || ((unsigned char)reply_recv_buf[4] != 0xFB))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
            res = DATA_ERR;
            goto EXIT;
        }
        break;
    }
    if (res < 0)
    {
        goto EXIT;
    }
    
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        tv.tv_sec = 5;
        // 发送号码
        if ((res = send_data(send_buf, strlen(phone) + 5, &tv))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        	continue; 
        }
        
        // 接收同步头
        for (i = 0; i < sizeof(head); i++)
        {
            tv.tv_sec = 5;
            if ((res = recv_data(&head[i], sizeof(head[i]), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                return res;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        tv.tv_sec = 5;
        // 接收包根据号码的不同而不同 C9 + 36 = FF
        // A55A C2 0A FEFE C9CDC6CEC9C9C6C7 00 C5 接收包
        // A55A 42 0A 0101 3632393136363938    45 发送包
        if ((res = recv_data(&cmd, sizeof(cmd), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            goto EXIT;
        }
        PRINT("cmd = %02X\n", cmd);
        if (cmd != 0xC2)
        {
            continue;
        }
        
        tv.tv_sec = 5;
        if ((res = recv_data(recv_buf, strlen(phone) + 5, &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            goto EXIT;
        }
        // 不判断长度、呼叫成功标志位、校验位
        for (k = 0; k < strlen(phone) + 5 - 3; k++)
        {
            if (send_buf[k + 4] + recv_buf[k + 1] != 0xFF)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data err!", DATA_ERR);
                res = DATA_ERR;
                goto EXIT;
            }
        }
        
        // 不等于0x00说明呼叫失败
        PRINT("recv_buf[k + 1] = %02X\n", recv_buf[k + 1]);
        if (recv_buf[k + 1] != 0x00)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data err!", DATA_ERR);
            res = DATA_ERR;
            goto EXIT;
        }
        break;
    }

EXIT: 
    if (send_buf != NULL)
    {    
        free(send_buf);
        send_buf = NULL;
    }
    if (recv_buf != NULL)
    {
        free(recv_buf);
        recv_buf = NULL;    
    }
    
    *(common_tools.phone_status_flag) = 1;
    
    // 关闭参数回复
    tv.tv_sec = 5;
    if ((ret = send_data(close_reply_buf, sizeof(close_reply_buf), &tv)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
     	return ret; 
    }
    PRINT("res = %d\n", res);
    PRINT_STEP("exit...\n");
    
    return (res < 0) ? res : 0;
}

int recv_display_msg(char *phone_num)
{
    int res = 0;
    int i = 0;
    int j = 0;
    unsigned char pack_count = 1;
    int fd = 0;
    struct timeval tv = {common_tools.config->one_byte_timeout_sec + 15, common_tools.config->one_byte_timeout_usec};
    char head[2] = {0};
    char cmd = 0;
    unsigned char index = 0;
    unsigned char buf_len = 0;
    char buf[256] = {0};
    char buf_tmp[128] = {0};
    char checkbit = 0;
    char recv_checkbit = 0;
    struct s_phone_message phone_message;
    memset(&phone_message, 0, sizeof(struct s_phone_message));
    
    unsigned char ring_count = 0;
    char ring_buf[2] = {0};
    if (phone_num == NULL) 
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "phone_num is NULL!", res);
        return res;
    }
    for (j = 0; j < pack_count; j++)
    {
        for (i = 0; i < sizeof(head); i++)
        {
            if ((res = recv_data(&head[i], sizeof(head[i]), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                return res;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        if ((res = recv_data(&cmd, sizeof(cmd), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        PRINT("cmd = %02X\n", (unsigned char)cmd);
        // FSK 来电 、DTMF 来电 、振铃
        if ((cmd != 0x05) && (cmd != 0x04) && (cmd != 0x03))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data err!", DATA_ERR);
            return DATA_ERR;
        }
        
        if (cmd == 0x03)
        {
            if ((res = recv_data(ring_buf, sizeof(ring_buf), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                return res;
            }
            
            if ((ring_buf[0] != 0x00) || (ring_buf[1] != 0x03))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
                return DATA_ERR;
            }
            ring_count++;
            if (ring_count >= 5)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
                refuse_to_answer(); // 拒接来电 以后和PSTN协商
                return DATA_ERR;
            }
            memset(ring_buf, 0, sizeof(ring_buf));
            j = -1;
            continue;
        }
        
        if ((res = recv_data(&buf_len, sizeof(buf_len), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        PRINT("buf_len = %02X\n", buf_len);
        
        buf_tmp[0] = cmd;
        buf_tmp[1] = buf_len;
        
        if ((res = recv_data(buf_tmp + 2, buf_len, &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        
        if ((res = recv_data(&checkbit, sizeof(checkbit), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            return res;
        }
        
        printf("\n");
        res = common_tools.get_checkbit(buf_tmp, NULL, 0, (unsigned short)buf_len + 2, XOR, 1);
        if ((res == MISMATCH_ERR) || (res == NULL_ERR))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "get_checkbit failed!", res);
            return res;
        }
        
        PRINT("checkbit = %02X, res = %02X\n", checkbit, res);
        if (checkbit != (char)res)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "check failed!", CHECK_DIFF_ERR);
            return CHECK_DIFF_ERR;
        }
        
        if (cmd == 0x05)
        {
            memcpy(buf + index, buf_tmp + 4, buf_len - 2);
            index += (buf_len - 2);
            pack_count = buf_tmp[2];
            memset(buf_tmp, 0, sizeof(buf_tmp));
        }
        else if (cmd == 0x04)
        {
            memcpy(buf, buf_tmp + 2, buf_len);
            memset(buf_tmp, 0, sizeof(buf_tmp));
            break;
        }
    }
    // DTMF
    if (cmd == 0x04)
    {
        memcpy(phone_message.call_identify_msg_phone_num, buf, buf_len);
        char print_buf[128] = {0};
        sprintf(print_buf, "phone_message.call_identify_msg_phone_num = %s", phone_message.call_identify_msg_phone_num);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                
        memset(print_buf, 0, sizeof(print_buf));
        sprintf(print_buf, "common_tools.config->center_phone = %s", common_tools.config->center_phone);
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
        PRINT("%s\n", phone_message.call_identify_msg_phone_num);
        
        PRINT("phone_num = %s\n", phone_num);
        if (memcmp(phone_message.call_identify_msg_phone_num, phone_num, strlen(phone_num)) != 0)
        {            	
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "phone is error!", CENTER_PHONE_ERR);
            refuse_to_answer(); // 拒接 以后不做处理
          	return CENTER_PHONE_ERR;
        }
        return 0;
    }
    
    // FSK
    for (i = 0; i < index; i++)
    {
        switch (buf[i])
        {
            case 0x01 :
            {   
                phone_message.time_msg_len = buf[++i];
                i++;
                memcpy(phone_message.time_msg_date, buf + i, sizeof(phone_message.time_msg_date));
                i +=  sizeof(phone_message.time_msg_date);
                memcpy(phone_message.time_msg_time, buf + i, sizeof(phone_message.time_msg_time));  
                i +=  sizeof(phone_message.time_msg_time);
                break;
            }    
            case 0x02 :
            {
                phone_message.call_identify_msg_len = buf[++i];
                i++;
                memcpy(phone_message.call_identify_msg_phone_num, buf + i, phone_message.call_identify_msg_len);
                i +=  phone_message.call_identify_msg_len;
                char print_buf[128] = {0};
                sprintf(print_buf, "phone_message.call_identify_msg_phone_num = %s", phone_message.call_identify_msg_phone_num);
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                
                PRINT("%s\n", phone_message.call_identify_msg_phone_num);
                PRINT("phone_num = %s\n", phone_num);
                if (memcmp(phone_message.call_identify_msg_phone_num, phone_num, strlen(phone_num)) != 0)
                {            	
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "phone is error!", CENTER_PHONE_ERR);
                    refuse_to_answer(); // 拒接 以后不做处理
                	return CENTER_PHONE_ERR;
                }
                break;
            }    
            case 0x07 :
            {
                phone_message.name_msg_len = buf[++i]; 
                i++; 
                memcpy(phone_message.name_msg_name, buf + i, phone_message.name_msg_len);
                i +=  phone_message.name_msg_len;
                break; 
            }
            default:
                break;
        }
    }

    return 0;
}

/*
 * 拒绝接听
 */
int refuse_to_answer()
{
    int res = 0, ret = 0;
    int i = 0, j = 0;
    char head[2] = {0};
    unsigned char cmd = 0;
    char open_channel_buf[6] = {0xA5, 0x5A, 0x4A, 0x01, 0x95, 0xDE};
    char close_channel_buf[6] = {0xA5, 0x5A, 0x4A, 0x01, 0x94, 0xDF};
    char open_reply_buf[7] = {0xA5, 0x5A, 0x70, 0x02, 0x08, 0x01, 0x7B};
    char close_reply_buf[7] = {0xA5, 0x5A, 0x70, 0x02, 0x08, 0x00, 0x7A};
    char reply_recv_buf[5] = {0};
    char ring_buf[2] = {0};
    char buf[4] = {0};
    char phone_abnormal_buf[4] = {0};
    struct timeval tv = {5, 0};
    
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        tv.tv_sec = 5;
        // 打开参数回复
        if ((res = send_data(open_reply_buf, sizeof(open_reply_buf), &tv))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
         	return res; 
        }
        
        // 接收同步头
        for (i = 0; i < sizeof(head); i++)
        {
            tv.tv_sec = 5;
            if ((res = recv_data(&head[i], sizeof(head[i]), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                return res;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        tv.tv_sec = 5;
        if ((res = recv_data(&cmd, sizeof(cmd), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            continue;
        }
        PRINT("cmd = %02X\n", cmd);
        if (cmd != 0xF0)
        {
            res = DATA_ERR;
            continue;
        }
        
        tv.tv_sec = 5;
        // A5 5A F0 02 F7 FE 00 FB
        if ((res = recv_data(reply_recv_buf, sizeof(reply_recv_buf), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
            continue;
        }
        
        if ((reply_recv_buf[0] != 0x02) || ((unsigned char)reply_recv_buf[1] != 0xF7) || ((unsigned char)reply_recv_buf[2] != 0xFE) || 
            (reply_recv_buf[3] != 0x00) || ((unsigned char)reply_recv_buf[4] != 0xFB))
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
            res = DATA_ERR;
            goto EXIT;
        }
        break;
    }
    if (res < 0)
    {
        goto EXIT;
    }
    
    // 第一步：打开话路 A5 5A 4A 01 95 DE
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        tv.tv_sec = 5;
        // 发送命令
        if ((res = send_data(open_channel_buf, sizeof(open_channel_buf), &tv))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        	continue; 
        }
        
        // 接收回复 A5 5A CA 01 6A 00 A1
        for (i = 0; i < sizeof(head); i++)
        {
            tv.tv_sec = 5;
            if ((res = recv_data(&head[i], sizeof(head[i]), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                continue;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        tv.tv_sec = 5;
        if ((res = recv_data(&cmd, sizeof(cmd), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            continue;
        }
        PRINT("cmd = %02X\n", cmd);
        if (cmd == 0xCA)
        {
            tv.tv_sec = 5;
            if ((res = recv_data(buf, sizeof(buf), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                continue;
            }
            
            if ((buf[0] != 0x01) || ((unsigned char)buf[1] != 0x6A) || (buf[2] != 0x00) || ((unsigned char)buf[3] != 0xA1))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
                res = DATA_ERR;
                continue;
            }
            
            // 成功时跳出
            break;
        }
        else if (cmd == 0x03) // 振铃
        {
            tv.tv_sec = 5;
            if ((res = recv_data(ring_buf, sizeof(ring_buf), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                goto EXIT;
            }
            
            if ((ring_buf[0] != 0x00) || (ring_buf[1] != 0x03))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
                res = DATA_ERR;
                goto EXIT;
            }
            continue;
        }
        else // 其他命令字时，重新接收
        {
            res = DATA_ERR;
            continue;
        } 
    }
    if (i == common_tools.config->repeat)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "open channel failed!", res);
        goto EXIT;
    }
    
    // 第二步：延时150ms
    usleep(150000);
    
    // 第三步：关闭话路 A5 5A 4A 01 94 DF
    for (j = 0; j < common_tools.config->repeat; j++)
    {
        tv.tv_sec = 5;
        // 发送命令
        if ((res = send_data(close_channel_buf, sizeof(close_channel_buf), &tv))< 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        	continue; 
        }
        
        // 接收回复 A5 5A CA 01 6B 00 A0
        for (i = 0; i < sizeof(head); i++)
        {
            tv.tv_sec = 5;
            if ((res = recv_data(&head[i], sizeof(head[i]), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data_head failed!", res);
                continue;
            }
            if (i == (sizeof(head) - 1)) 
            {
                if (((unsigned char)head[0] == 0xA5) && (head[1] == 0x5A))
                {
                    break;
                }
                else 
                {
                    head[0] = head[1];
                    head[1] = 0;
                    i = 0;
                }
            }
        }
        
        tv.tv_sec = 5;
        if ((res = recv_data(&cmd, sizeof(cmd), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
            continue;
        }
        
        if (cmd == 0xCA)
        {
            tv.tv_sec = 5;
            if ((res = recv_data(buf, sizeof(buf), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                continue;
            }
            
            if ((buf[0] != 0x01) || ((unsigned char)buf[1] != 0x6B) || (buf[2] != 0x00) || ((unsigned char)buf[3] != 0xA0))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
                res = DATA_ERR;
                continue;
            }
            
            // 成功时跳出
            break;
        }
        else if (cmd == 0x03) // 振铃
        {
            tv.tv_sec = 5;
            if ((res = recv_data(ring_buf, sizeof(ring_buf), &tv)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "recv_data failed!", res);
                goto EXIT;
            }
            
            if ((ring_buf[0] != 0x00) || (ring_buf[1] != 0x03))
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "data error!", DATA_ERR);
                res = DATA_ERR;
                goto EXIT;
            }
            continue;
        }
        else // 其他命令字时，重新接收
        {
            res = DATA_ERR;
            continue;
        }  
    }
    if (i == common_tools.config->repeat)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "open channel failed!", res);
        goto EXIT;
    }
 
EXIT:   
    // 关闭参数回复
    if ((ret = send_data(close_reply_buf, sizeof(close_reply_buf), &tv))< 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
     	return ret; 
    }
    return res;
}


/**
 * 初始化
 */
int init()
{
    int res = 0;
    if (spi_node_fd != 0)
    {
        close(spi_node_fd);
        spi_node_fd = 0;
    }
    
    if ((res = open(SPI_NODE, O_RDWR, 0644)) < 0)
    {  	
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "open failed!", OPEN_ERR);
    	return OPEN_ERR;
    }
    spi_node_fd = res;
    PRINT("spi_node_fd = %d\n", spi_node_fd);
    return 0;
}

/**
 * 释放
 */
int release()
{
    if (spi_node_fd != 0)
    {
        close(spi_node_fd);
        spi_node_fd = 0;
    }
    return 0;
}

int send_data(char *data, unsigned int data_len, struct timeval *tv)
{
    PRINT_STEP("entry...\n");
    int res = 0;
    
    if (data == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    if (spi_node_fd == 0)
    {
        if ((res = init()) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "init failed!", res);
            return res;
        }
    }
	if ((res = common_tools.send_data(spi_node_fd, data, NULL, data_len, tv)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        release();
        return res;
    }
    PRINT_STEP("exit...\n");
    return 0;
}


/**
 * 接收
 */
int recv_data(char *data, unsigned int data_len, struct timeval *tv)
{
    int res = 0;
    
    if (data == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data is NULL!", NULL_ERR);
        return NULL_ERR;
    }
    
    if (spi_node_fd == 0)
    {
        if ((res = init()) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "init failed!", res);
            return res;
        }
    }
    
    if ((res = common_tools.recv_data(spi_node_fd, data, NULL, data_len, tv)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "recv_data failed!", res);
        release();
    	return res;
    }
    return res;
}


/**
 * 切换继电器
 */
int relay_change(unsigned char mode)
{
    // A5 5A 4A 01 94 DF  切到CACM
    // A5 5A 4A 01 95 DE  切到打电话 
    int res = 0;
    char send_cacm_buf[6] = {0xA5, 0x5A, 0x4A, 0x01, 0x94, 0xDF};
    char send_phone_buf[6] = {0xA5, 0x5A, 0x4A, 0x01, 0x95, 0xDE};
    struct timeval tv = {5, 0};
    
    if (mode == 1) // 从95R54切换到SI32919
    {
        if ((res = send_data(send_phone_buf, sizeof(send_phone_buf), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
            return res;
        }
    }
    else if (mode == 0) // 从SI32919切换到95R54
    {
        if ((res = send_data(send_cacm_buf, sizeof(send_cacm_buf), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
            return res;
        }
    }
    else
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", DATA_ERR);
        return DATA_ERR;
    }
    return 0;
}

/**
 * 回环控制
 */
int loop_manage(unsigned char mode)
{
    // A5 5A 70 02 09 00 7B 关闭回环
    // A5 5A 70 02 09 01 7A 打开回环 
    int res = 0;
    char loop_close_buf[7] = {0xA5, 0x5A, 0x70, 0x02, 0x09, 0x00, 0x7B};
    char loop_open_buf[7] = {0xA5, 0x5A, 0x70, 0x02, 0x09, 0x01, 0x7A};
    struct timeval tv = {5, 0};
    
    if (mode == 1) // 打开回环 
    {
        if ((res = send_data(loop_open_buf, sizeof(loop_open_buf), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
            return res;
        }
    }
    else if (mode == 0) // 关闭回环
    {
        if ((res = send_data(loop_close_buf, sizeof(loop_close_buf), &tv)) < 0)
        {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
            return res;
        }
    }
    else
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "data error!", DATA_ERR);
        return DATA_ERR;
    }
    return 0;
}

/**
 * 电话线检测
 */
int phone_state_monitor(unsigned char cmd)
{
    int res = 0;
    int fd = 0;
    struct timeval tv = {3, 0};
    char send_buf[6] = {0x5A, 0xA5, 0x02, 0x11};
    send_buf[4] = cmd;
    send_buf[5] = cmd ^ 0x11;
    
     // 创建本地socket，发送电话线状态到CACM
    if ((fd = communication_network.make_client_link("127.0.0.1", CACM_SOCKCT_PORT)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "make_client_link failed!", fd);
        return fd;
    }
    
    if ((res = common_tools.send_data(fd, send_buf, NULL, sizeof(send_buf), &tv)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "send_data failed!", res);
        close(fd);
    	return res;
    }
    usleep(10);
    close(fd);
    return 0;
}
