#include "unionpay_message.h"


/**
 * 对通过pstn发送的连接请求包解包
 */
static int PSTN_msg_display_unpack(struct s_data_list *a_data_list);

/**
 * 把通过pstn发送的连接应答包打包
 */
static int PSTN_msg_link_request_unpack(struct s_data_list *a_data_list);

/**
 * 对通过pstn发送的数据完成包解包
 */
static int PSTN_msg_link_respond_pack(struct s_data_list *a_data_list);

/**
 * 把通过pstn发送的数据完成包打包
 */
static int PSTN_msg_data_finish_unpack(struct s_data_list *a_data_list);

/**
 * 把通过pstn发送的数据完成包打包
 */
static int PSTN_msg_data_finish_pack(struct s_data_list *a_data_list);

/**
 * 对通过pstn发送的交易包解包
 */
static int PSTN_msg_deal_unpack(struct s_data_list *a_data_list, void *para);

/**
 * 把通过pstn发送的交易包打包
 */
static int PSTN_msg_deal_pack(struct s_data_list *a_data_list, void *para);

struct class_unionpay_message unionpay_message = 
{
    PSTN_msg_display_unpack, PSTN_msg_link_request_unpack, PSTN_msg_link_respond_pack,
    PSTN_msg_data_finish_unpack, PSTN_msg_data_finish_pack, PSTN_msg_deal_unpack, PSTN_msg_deal_pack
};

/**
 * 对通过pstn发送的连接请求包解包
 */
int PSTN_msg_display_unpack(struct s_data_list *a_data_list)
{
    PRINT_STEP("entry...\n");
    int index = 0;
    int res = 0;
    char buf_tmp = 0;
    struct s_phone_message phone_message;
    
    // 消息类型
	if ((res = common_tools.list_get_data(a_data_list, index, sizeof(phone_message.news_type), &phone_message.news_type)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res; 
	}
	   
	if (((unsigned char)phone_message.news_type != 0x80) && ((unsigned char)phone_message.news_type != 0x04))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "news_type error!", S_DATA_ERR);
    	return S_DATA_ERR;
    }
    index += sizeof(phone_message.news_type);
    
    // 报文长度
    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(phone_message.length), &phone_message.length)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res; 
	}
    index += sizeof(phone_message.length);
    
	while (index < phone_message.length)
    {
        if ((unsigned char)phone_message.news_type == 0x80)
        {
            if ((res = common_tools.list_get_data(a_data_list, index, sizeof(buf_tmp), &buf_tmp)) < 0)
        	{
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
                return res; 
        	}
        	
            switch (buf_tmp)
            {
                case 0x01 :
                {   
                    index++;
                    
                    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(phone_message.time_msg_len), &phone_message.time_msg_len)) < 0)
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
                        return res; 
                    }
                    index++;
                    
                    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(phone_message.time_msg_date), phone_message.time_msg_date)) < 0)
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
                        return res; 
                    }
                    index += sizeof(phone_message.time_msg_date);
                    
                    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(phone_message.time_msg_time), phone_message.time_msg_time)) < 0)
                    {
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
                        return res; 
                    }
                    index += sizeof(phone_message.time_msg_time);
                    break;
                }    
                case 0x02 :
                {
                    index++;
                    
                    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(phone_message.call_identify_msg_len), &phone_message.call_identify_msg_len)) < 0)
                    {                    
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res); 
                        return res; 
                    }
                    index++;
                    
                    if ((res = common_tools.list_get_data(a_data_list, index, phone_message.call_identify_msg_len, phone_message.call_identify_msg_phone_num)) < 0)
                    {                    
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
                        return res; 
                    }
                    index += phone_message.call_identify_msg_len;
                    
                    char print_buf[128] = {0};
                    sprintf(print_buf, "phone_message.call_identify_msg_phone_num = %s", phone_message.call_identify_msg_phone_num);
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                    
                    memset(print_buf, 0, sizeof(print_buf));
                    sprintf(print_buf, "common_tools.config->center_phone = %s", common_tools.config->center_phone);
                    OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                    PRINT("%s %x\n", phone_message.call_identify_msg_phone_num, phone_message.call_identify_msg_phone_num);
                    PRINT("common_tools.config->center_phone = %s %x\n", common_tools.config->center_phone, common_tools.config->center_phone);
                    if (memcmp(phone_message.call_identify_msg_phone_num, common_tools.config->center_phone, strlen(common_tools.config->center_phone)) != 0)
                    {                	
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "phone is error!", CENTER_PHONE_ERR);
                    	return CENTER_PHONE_ERR;
                    }
                    break;
                }    
                case 0x07 :
                {
                    index++;
                    
                    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(phone_message.name_msg_len), &phone_message.name_msg_len)) < 0)
                    {                    
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);  
                        return res; 
                    }
                    index++;
                    
                    if ((res = common_tools.list_get_data(a_data_list, index, phone_message.name_msg_len, phone_message.name_msg_name)) < 0)
                    {                    
                        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
                        return res; 
                    }
                    index += phone_message.name_msg_len;     
                    break; 
                }
                default:
                    break;
            }
        }
        else if (phone_message.news_type == 0x04)
        {
            if ((res = common_tools.list_get_data(a_data_list, index, sizeof(phone_message.time_msg_date), phone_message.time_msg_date)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
                return res; 
            }
            index += sizeof(phone_message.time_msg_date);
                    
            if ((res = common_tools.list_get_data(a_data_list, index, sizeof(phone_message.time_msg_time), phone_message.time_msg_time)) < 0)
            {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);                    
                return res; 
            }
            index += sizeof(phone_message.time_msg_time);
            
            if ((res = common_tools.list_get_data(a_data_list, index, (phone_message.length - 8), phone_message.call_identify_msg_phone_num)) < 0)
            {                    
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
                return res; 
            }
            index += (phone_message.length - 8);
                    
            char print_buf[128] = {0};
            sprintf(print_buf, "phone_message.call_identify_msg_phone_num = %s", phone_message.call_identify_msg_phone_num);
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
                    
            memset(print_buf, 0, sizeof(print_buf));
            sprintf(print_buf, "common_tools.config->center_phone = %s", common_tools.config->center_phone);
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, print_buf, 0);
            if (memcmp(phone_message.call_identify_msg_phone_num, common_tools.config->center_phone, strlen(common_tools.config->center_phone)) != 0)
            {                	
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "phone is error!", CENTER_PHONE_ERR);
                return CENTER_PHONE_ERR;
            }
            break;  
        }
    }
      
    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(phone_message.check_sum), &phone_message.check_sum)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res; 
    }
    
    // 计算来电显示报文的校验和
    
    res = common_tools.get_checkbit((char *)&phone_message, NULL, 0, sizeof(phone_message) - 1, SUM, 1);
    if ((res == MISMATCH_ERR) || (res == NULL_ERR))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "get_checkbit failed!", res);
        return res;
    }
    #if CHECK_DEBUG
    // 校验位对比
    if ((char)res != phone_message.check_sum)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "checksums are different!", res);
        return S_CHECK_DIFF_ERR; 
    }
    #endif
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 对通过pstn发送的连接请求包解包
 */
int PSTN_msg_link_request_unpack(struct s_data_list *a_data_list)
{
    PRINT_STEP("entry...\n");
    int index = 0;
    int res = 0;
    struct s_link_message link_message;
    
    // 获取消息类型
    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(link_message.news_type), &link_message.news_type)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res; 
	}
	index += sizeof(link_message.news_type);
	
	if ((unsigned char)link_message.news_type != 0x81)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "news_type is error!", res);
    	return S_DATA_ERR;
    }
    
    // 获取报文长度
    char buf_tmp[sizeof(link_message.length)] = {0};
    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(link_message.length), buf_tmp)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res; 
	}
	index += sizeof(link_message.length);
    
    // ENDIAN
    
    /*
    //原来为小端处理
	buf_tmp[0] = buf_tmp[0] + buf_tmp[1];
	buf_tmp[1] = buf_tmp[0] - buf_tmp[1];
	buf_tmp[0] = buf_tmp[0] - buf_tmp[1];
	memcpy(&(link_message.length), buf_tmp, sizeof(buf_tmp));
    */
    link_message.length = ((unsigned short)buf_tmp[0] << 8) & 0xFF00);
    link_message.length += buf_tmp[1];
    
	if (link_message.length != 5)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "length is error!", res);
    	return S_DATA_ERR;
    }
    
	// 获取随机数
    //需要和网关认证密钥进行按位异或运算，并返回中心，到连接结束前，一直用此值，并验证其一致性，不一致，就中断本次链接
    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(link_message.deal_sync_random_num), (char *)&link_message.deal_sync_random_num)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res; 
	}
	index += sizeof(link_message.deal_sync_random_num);
	common_tools.deal_attr->sync_random_num = link_message.deal_sync_random_num;
    
    // 同步序号
    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(link_message.message_sync_num), &link_message.message_sync_num)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res; 
	}
	index += sizeof(link_message.message_sync_num);
	common_tools.deal_attr->message_sync_num = link_message.message_sync_num;
    
	if (link_message.message_sync_num != 0x00)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "message_sync_num is error!", S_DATA_ERR);
    	return S_DATA_ERR;
    }
    
	if ((res = common_tools.list_get_data(a_data_list, index, sizeof(link_message.check_sum), &link_message.check_sum)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res; 
	}
	index += sizeof(link_message.check_sum);
	
	// 计算校验和
    res = common_tools.get_checkbit((char *)&link_message, NULL, 0, sizeof(link_message) - 1, SUM, 1);
    if ((res == MISMATCH_ERR) || (res == NULL_ERR))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "get_checkbit failed!", res);
        return res;
    }
    
	#if CHECK_DEBUG
	if ((char)res != link_message.check_sum)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "checksums are different!", S_CHECK_DIFF_ERR);
        return S_CHECK_DIFF_ERR; 
	}
	#endif
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 把通过pstn发送的连接应答包打包
 */
int PSTN_msg_link_respond_pack(struct s_data_list *a_data_list)
{
    PRINT_STEP("entry...\n");
	int res = 0;
	int i = 0;
	struct s_link_message link_message;
	memset(&link_message, 0, sizeof(link_message));
	
	common_tools.list_free(a_data_list);
	
	link_message.news_type = 0x82;
	link_message.length = htons(5);
	
	/*
	ENDIAN
	unsigned char length_tmp[2] = {0};
	length_tmp[0] = (char)((link_message.length & 0xFF00) >> 8);
	length_tmp[1] = (char)(link_message.length & 0x00FF);
	memcpy(&(link_message.length), length_tmp, 2);
	*/
	
	link_message.deal_sync_random_num = common_tools.deal_attr->sync_random_num;
	link_message.message_sync_num = common_tools.deal_attr->message_sync_num;

	if ((link_message.check_sum = common_tools.get_checkbit((char *)&link_message, NULL, 0, sizeof(link_message) - 1, SUM, 1)) < 0)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_checkbit failed!", link_message.check_sum);
    	return link_message.check_sum;
    }
    
	// 把数据添加到链表中去
	for (i = 0; i < sizeof(link_message); i++)
	{
	    if ((res = common_tools.list_tail_add_data(a_data_list, ((char *)&link_message)[i])) < 0)
	    {	        
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_tail_add_data failed!", res);            
        	return res;
	    } 
	}
	PRINT_BUF_BY_HEX(NULL, a_data_list, a_data_list->list_len, __FILE__, __FUNCTION__, __LINE__);
	PRINT_STEP("exit...\n");
	return 0;
}

/**
 * 对通过pstn发送的数据完成包解包
 */
int PSTN_msg_data_finish_unpack(struct s_data_list *a_data_list)
{
    PRINT_STEP("entry...\n");
    int index = 0;
    int res = 0;
    struct s_data_finish_message data_finish_message;
    memset(&data_finish_message, 0, sizeof(data_finish_message));
    
    // 获取消息类型
    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(data_finish_message.news_type), &data_finish_message.news_type)) < 0)
	{    	
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res); 
        return res; 
	}
	index += sizeof(data_finish_message.news_type);
	
	if ((unsigned char)data_finish_message.news_type != 0x83)
    {	
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "news_type is error!", S_DATA_ERR);
    	return S_DATA_ERR;
    }
    
    // 获取报文长度
    char buf_tmp[sizeof(data_finish_message.length)] = {0};
    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(data_finish_message.length), buf_tmp)) < 0)
	{        
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);        
        return res; 
	}
	index += sizeof(data_finish_message.length);
    
    /*
    ENDIAN
	buf_tmp[0] = buf_tmp[0] + buf_tmp[1];
	buf_tmp[1] = buf_tmp[0] - buf_tmp[1];
	buf_tmp[0] = buf_tmp[0] - buf_tmp[1];
	memcpy(&(data_finish_message.length), buf_tmp, sizeof(buf_tmp));
    */
    data_finish_message.length = ((unsigned short)buf_tmp[0] << 8) & 0xFF00);
    data_finish_message.length += buf_tmp[1];
    
	if (data_finish_message.length != 5)
    {    	
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "length is error!", S_DATA_ERR);
    	return S_DATA_ERR;
    }
	// 获取随机数
    //需要和网关认证密钥进行按位异或运算，并返回中心，到连接结束前，一直用此值，并验证其一致性，不一致，就中断本次链接
    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(data_finish_message.deal_sync_random_num), (char *)&data_finish_message.deal_sync_random_num)) < 0)
	{    	
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", S_DATA_ERR); 
        return res; 
	}
	index += sizeof(data_finish_message.deal_sync_random_num);
	
	if (common_tools.deal_attr->sync_random_num != data_finish_message.deal_sync_random_num)
    {        
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "deal_sync_random_num is error!", S_DATA_ERR);
    	return S_DATA_ERR;
    }
    
    // 同步序号
    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(data_finish_message.message_sync_num), &data_finish_message.message_sync_num)) < 0)
	{        
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", S_DATA_ERR);
        return res;   
	}
	index += sizeof(data_finish_message.message_sync_num);
    
	if (data_finish_message.message_sync_num != common_tools.deal_attr->message_sync_num)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "message_sync_num is error!", S_DATA_ERR);  
    	return S_DATA_ERR;
    }
    
	if ((res = common_tools.list_get_data(a_data_list, index, sizeof(data_finish_message.check_sum), &data_finish_message.check_sum)) < 0)
	{        
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);  
        return res;  
	}
	index += sizeof(data_finish_message.check_sum);
	
	// 计算校验和
    res = common_tools.get_checkbit((char *)&data_finish_message, NULL, 0, sizeof(data_finish_message) - 1, SUM, 1);
    if ((res == MISMATCH_ERR) || (res == NULL_ERR))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "get_checkbit failed!", res);
        return res;
    }
	#if CHECK_DEBUG
	if ((char)res != data_finish_message.check_sum)
	{ 	
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "checksums are different!", S_CHECK_DIFF_ERR);  
        return S_CHECK_DIFF_ERR; 
	}
	#endif
    PRINT_STEP("exit...\n");
    return 0;
}

/**
 * 把通过pstn发送的数据完成包打包
 */
int PSTN_msg_data_finish_pack(struct s_data_list *a_data_list)
{
    PRINT_STEP("entry...\n");
	int res = 0;
	int i = 0;
	char print_info[512] = {0};                     // 日志打印信息
	struct s_data_finish_message data_finish_message;
	memset(&data_finish_message, 0, sizeof(data_finish_message));
	common_tools.list_free(a_data_list);
	
	data_finish_message.news_type = 0x83;
	data_finish_message.length = htons(5);
	
	/*
	ENDIAN
	unsigned char length_tmp[2] = {0};
	length_tmp[0] = (char)((data_finish_message.length & 0xFF00) >> 8);
	length_tmp[1] = (char)(data_finish_message.length & 0x00FF);
	memcpy(&(data_finish_message.length), length_tmp, 2);
	*/
	data_finish_message.deal_sync_random_num = common_tools.deal_attr->sync_random_num;
	data_finish_message.message_sync_num = common_tools.deal_attr->message_sync_num;
	
	res = common_tools.get_checkbit((char *)&data_finish_message, NULL, 0, sizeof(data_finish_message) - 1, SUM, 1);
    if ((res == MISMATCH_ERR) || (res == NULL_ERR))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "get_checkbit failed!", res);
        return res;
    }
    data_finish_message.check_sum = (char)res;
    
	// 把数据添加到链表中去
	for (i = 0; i < sizeof(data_finish_message); i++)
	{
	    if ((res = common_tools.list_tail_add_data(a_data_list, ((char *)&data_finish_message)[i])) < 0)
	    {	        
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_tail_add_data failed!", res); 
            return res;
	    } 
	}
	PRINT_BUF_BY_HEX(NULL, a_data_list, a_data_list->list_len, __FILE__, __FUNCTION__, __LINE__);
	PRINT_STEP("exit...\n");
	return 0;
}

/**
 * 对通过pstn发送的交易包解包
 */
int PSTN_msg_deal_unpack(struct s_data_list *a_data_list, void *para)
{
    PRINT_STEP("entry...\n");
    int res = 0;
	int index = 0;
    struct s_deal_message_84 deal_message_84;
    memset((void *)&deal_message_84, 0, sizeof(deal_message_84));
        
    // 消息类型
    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(deal_message_84.news_type), &deal_message_84.news_type)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res;  
	}
	index += sizeof(deal_message_84.news_type);
	
	if ((unsigned char)deal_message_84.news_type != 0x84)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "news_type is error!", S_DATA_ERR);
    	return S_DATA_ERR;
    }
        
    // 报文长度
    char buf_tmp[sizeof(deal_message_84.length)] = {0};
    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(deal_message_84.length), buf_tmp)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res;  
	}
	index += sizeof(deal_message_84.length);
	/*
	ENDIAN
	buf_tmp[0] = buf_tmp[0] + buf_tmp[1];
	buf_tmp[1] = buf_tmp[0] - buf_tmp[1];
	buf_tmp[0] = buf_tmp[0] - buf_tmp[1];
	memcpy(&(deal_message_84.length), buf_tmp, sizeof(buf_tmp));
    */
    deal_message_84.length = ((unsigned short)buf_tmp[0] << 8) & 0xFF00);
    deal_message_84.length += buf_tmp[1];
    
    // 同步随机数
    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(deal_message_84.deal_sync_random_num), (char *)&(deal_message_84.deal_sync_random_num))) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res;  
	}
	index += sizeof(deal_message_84.deal_sync_random_num);
	
	if (common_tools.deal_attr->sync_random_num != deal_message_84.deal_sync_random_num)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "deal_sync_random_num is error!", S_DATA_ERR);
    	return S_DATA_ERR;
    }    
    
    // 同步序号
    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(deal_message_84.message_sync_num), &(deal_message_84.message_sync_num))) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res;   
	}
	index += sizeof(deal_message_84.message_sync_num);
	
	if (deal_message_84.message_sync_num - common_tools.deal_attr->message_sync_num == 1)
    {
    	common_tools.deal_attr->message_sync_num = deal_message_84.message_sync_num;    
    }
	else
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "message_sync_num is error!", S_DATA_ERR);
    	return S_DATA_ERR;
    }
    
    // 消息内容长度
    memset(buf_tmp, 0, sizeof(buf_tmp));
	if ((res = common_tools.list_get_data(a_data_list, index, sizeof(buf_tmp), buf_tmp)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res;  
	}
	index += sizeof(buf_tmp);
	
	/*
	ENDIAN
	buf_tmp[0] = buf_tmp[0] + buf_tmp[1];
	buf_tmp[1] = buf_tmp[0] - buf_tmp[1];
	buf_tmp[0] = buf_tmp[0] - buf_tmp[1];
	memcpy(&(deal_message_84.news_content_len), buf_tmp, sizeof(buf_tmp));
	*/
    deal_message_84.news_content_len = ((unsigned short)buf_tmp[0] << 8) & 0xFF00);
    deal_message_84.news_content_len += buf_tmp[1];
    
    // 消息报文类型
    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(deal_message_84.news_content_84.message_type), &deal_message_84.news_content_84.message_type)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res;  
	}
	index += sizeof(deal_message_84.news_content_84.message_type);
    
	if (deal_message_84.news_content_84.message_type != 0x02)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "message_type is error!", S_DATA_ERR);
    	return S_DATA_ERR;
    }
    
    // 结束标志
    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(deal_message_84.news_content_84.end_flag), &deal_message_84.news_content_84.end_flag)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res;  
	}
	index += sizeof(deal_message_84.news_content_84.end_flag);
    
    // 密码键盘序列号
    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(deal_message_84.news_content_84.password_keyboard_num), deal_message_84.news_content_84.password_keyboard_num)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res;  
	}
	index += sizeof(deal_message_84.news_content_84.password_keyboard_num);
	
	// 系统日期
	if ((res = common_tools.list_get_data(a_data_list, index, sizeof(deal_message_84.news_content_84.system_date), deal_message_84.news_content_84.system_date)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res;  
	}
	index += sizeof(deal_message_84.news_content_84.system_date);
	
	// 系统时间
	if ((res = common_tools.list_get_data(a_data_list, index, sizeof(deal_message_84.news_content_84.system_time), deal_message_84.news_content_84.system_time)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res;  
	}
	index += sizeof(deal_message_84.news_content_84.system_time);
	
	// 交易号
	if ((res = common_tools.list_get_data(a_data_list, index, sizeof(deal_message_84.news_content_84.deal_num), deal_message_84.news_content_84.deal_num)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res;  
	}
	index += sizeof(deal_message_84.news_content_84.deal_num);
	
	// 交易代码
	if ((res = common_tools.list_get_data(a_data_list, index, sizeof(deal_message_84.news_content_84.deal_code), deal_message_84.news_content_84.deal_code)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res;  
	}
	index += sizeof(deal_message_84.news_content_84.deal_code);
    
    //********************************流程代码 start
    //1.读取一个字节知道操作码的总数
	char tmp;
    char flowsheet_count = 0;
    char flowsheet_code[50] = {0};
    if ((res = common_tools.list_get_data(a_data_list, index, sizeof(flowsheet_count), &flowsheet_count)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        return res;   
	}
	index += sizeof(flowsheet_count);
	
    
	int j = 0, k = 0;
	for (k = 0; k < flowsheet_count; k++)
    {
        //2.读取一个字节与0xC0相与得到操作码类型,如果操作码类型为0，到第三步；类型为0x80 到第四步；类型为0Xc0,到第五步
    	if ((res = common_tools.list_get_data(a_data_list, index, sizeof(tmp), &tmp)) < 0)
    	{
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
            return res;  
    	}
    	index += sizeof(tmp);
    	flowsheet_code[j] = tmp;
    	
    	if ((tmp & 0xc0) == 0) //3.是双字节操作码，继续读一字节
        {
            if ((res = common_tools.list_get_data(a_data_list, index, sizeof(tmp), &tmp)) < 0)
        	{
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
                return res;  
        	}
        	index += sizeof(tmp);
        	
        	j++;
        	flowsheet_code[j] = tmp;
        	j++;
        }
    	else if ((tmp & 0xc0) == 0x80) //4.为单字节操作码，得到操作码，继续第二部，直到等于操作码总数
        {
        	j++;
        }
    	else if ((tmp & 0xc0) == 0xc0) //5.为三字节操作码，第三字节表示加密（p17）
        {
        	if ((res = common_tools.list_get_data(a_data_list, index, sizeof(tmp), &tmp)) < 0)
        	{
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
                return res;    
        	}
        	index += sizeof(tmp);
        	j++;
        	flowsheet_code[j] = tmp;
        	j++;
        	
        	if ((res = common_tools.list_get_data(a_data_list, index, sizeof(tmp), &tmp)) < 0)
        	{
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
                return res;  
        	}
        	index += sizeof(tmp);
        	flowsheet_code[j] = tmp;
        	j++;
        }
    }
	flowsheet_count = j;
    //********************************流程代码 end
    if ((deal_message_84.news_content_84.flowsheet_code = malloc(j + 1)) == NULL)
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR);
        return MALLOC_ERR;  
    }
    memcpy(deal_message_84.news_content_84.flowsheet_code, flowsheet_code, j);
    
	if ((res = common_tools.list_get_data(a_data_list, index, sizeof(deal_message_84.news_content_84.valid_data_length), (char *)&deal_message_84.news_content_84.valid_data_length)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        goto EXIT;
	}
	deal_message_84.news_content_84.valid_data_length = ntohs(deal_message_84.news_content_84.valid_data_length);
	index += sizeof(deal_message_84.news_content_84.valid_data_length);
	
	if ((deal_message_84.news_content_84.valid_data_zone = malloc(deal_message_84.news_content_84.valid_data_length + 1 - 8)) == NULL)
    {
        res = MALLOC_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", res);
        goto EXIT;
    }
	if ((res = common_tools.list_get_data(a_data_list, index, deal_message_84.news_content_84.valid_data_length-8, deal_message_84.news_content_84.valid_data_zone)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        goto EXIT;
	}
	
	if ((res = common_tools.list_get_data(a_data_list, index, sizeof(deal_message_84.news_content_84.MAC), deal_message_84.news_content_84.MAC)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        goto EXIT;
	}
	index += deal_message_84.news_content_84.valid_data_length;
    	
	if ((res = common_tools.list_get_data(a_data_list, index, sizeof(deal_message_84.check_sum), &deal_message_84.check_sum)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_get_data failed!", res);
        goto EXIT;
	}
	
	char check_sum;
	if ((res = common_tools.get_checkbit(NULL, a_data_list, 0, index, SUM, 1)) < 0)
    {    	
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "common_tools.list_get_data failed!", res);    
        goto EXIT;
    }
    check_sum = res;
    index += sizeof(deal_message_84.check_sum);
	#if CHECK_DEBUG
	if (check_sum != deal_message_84.check_sum)
	{    	
	    res = CHECK_DIFF_ERR;
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "checksums is different!", res); 
        goto EXIT;
	}
	#endif
	
	char userdefine_length = 0;
	j = 0;
	for (k = 0; k < flowsheet_count; k++)
    {        
        if ((unsigned char)deal_message_84.news_content_84.flowsheet_code[k] == 0XA6)
        {
            continue;
        }
    	if ((deal_message_84.news_content_84.flowsheet_code[k] == 0x0C) && (deal_message_84.news_content_84.flowsheet_code[k + 1] == 0x2C))
        {
        	userdefine_length = deal_message_84.news_content_84.valid_data_zone[j];
            
        	if (userdefine_length > sizeof(((struct s_dial_back_respond *)para)->terminal_token))
            {
                res = OVER_LEN_ERR; 
            	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "userdefine_length over length!", res); 
            	goto EXIT;
            }
            j++;
        	memcpy(((struct s_dial_back_respond *)para)->terminal_token, deal_message_84.news_content_84.valid_data_zone + j, userdefine_length);
        	((struct s_dial_back_respond *)para)->terminal_token_len = userdefine_length;
        	j += userdefine_length;
        	k++;
        	continue;
        }
    	if ((deal_message_84.news_content_84.flowsheet_code[k] == 0x0C) && (deal_message_84.news_content_84.flowsheet_code[k + 1] == 0x2A))
        {
        	userdefine_length = deal_message_84.news_content_84.valid_data_zone[j];
            
        	if (userdefine_length > sizeof(((struct s_dial_back_respond *)para)->terrace_random))
            {
                res = OVER_LEN_ERR;     
            	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "userdefine_length over length!", res); 
            	goto EXIT;
            }
            j++;
        	memcpy(&((struct s_dial_back_respond *)para)->terrace_random, deal_message_84.news_content_84.valid_data_zone + j, userdefine_length);
        	j += userdefine_length;
        	k++;
        	continue;
        }
    	if ((deal_message_84.news_content_84.flowsheet_code[k] == 0x0C) && (deal_message_84.news_content_84.flowsheet_code[k + 1] == 0x29))
        {
        	userdefine_length = deal_message_84.news_content_84.valid_data_zone[j];
            
        	if (userdefine_length > sizeof(((struct s_dial_back_respond *)para)->base_random_mac))
            {
                res = OVER_LEN_ERR;    
            	OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "userdefine_length over length!", res); 
            	goto EXIT;
            }
            j++;
        	memcpy(((struct s_dial_back_respond *)para)->base_random_mac, deal_message_84.news_content_84.valid_data_zone + j, userdefine_length);
        	j += userdefine_length;
        	k++;
        	continue;
        }
    }

EXIT:
    if (deal_message_84.news_content_84.flowsheet_code != NULL)
    {
        free(deal_message_84.news_content_84.flowsheet_code);
        deal_message_84.news_content_84.flowsheet_code = NULL;
    }
    if (deal_message_84.news_content_84.valid_data_zone != NULL)
    {
        free(deal_message_84.news_content_84.valid_data_zone);
        deal_message_84.news_content_84.valid_data_zone = NULL;
    }
    PRINT_STEP("exit...\n");
    return res;
}

/**
 * 把通过pstn发送的交易包打包
 */
int PSTN_msg_deal_pack(struct s_data_list *a_data_list, void *para)
{
    PRINT_STEP("entry...\n");
    int i = 0;
    int res = 0;
    int flowsheet_len = 0;
    unsigned short list_len = 0;
    struct s_deal_message_87 deal_message_87;
    memset(&deal_message_87, 0, sizeof(deal_message_87));
	common_tools.list_free(a_data_list);
	
	deal_message_87.news_type = 0x87;

	deal_message_87.deal_sync_random_num = common_tools.deal_attr->sync_random_num;
	deal_message_87.length += sizeof(deal_message_87.deal_sync_random_num);
	
	common_tools.deal_attr->message_sync_num++;
	deal_message_87.message_sync_num = common_tools.deal_attr->message_sync_num;
	deal_message_87.length += sizeof(deal_message_87.message_sync_num);
	
	deal_message_87.length += sizeof(deal_message_87.news_content_len);
	
	deal_message_87.news_content_87.message_type = 0x02;
	deal_message_87.length += sizeof(deal_message_87.news_content_87.message_type);
	deal_message_87.news_content_len += sizeof(deal_message_87.news_content_87.message_type);
	
	deal_message_87.news_content_87.end_flag = 0x00;
	deal_message_87.length += sizeof(deal_message_87.news_content_87.end_flag);
	deal_message_87.news_content_len += sizeof(deal_message_87.news_content_87.end_flag);
	
	deal_message_87.news_content_87.program_version_num[0] = 0x20;
	deal_message_87.news_content_87.program_version_num[1] = 0x01;
	deal_message_87.length += sizeof(deal_message_87.news_content_87.program_version_num);
	deal_message_87.news_content_len += sizeof(deal_message_87.news_content_87.program_version_num);
	
	deal_message_87.news_content_87.application_version_num[0] = 0x20;
	deal_message_87.news_content_87.application_version_num[1] = 0x13;
	deal_message_87.news_content_87.application_version_num[2] = 0x01;
	deal_message_87.news_content_87.application_version_num[3] = 0x09;
	deal_message_87.length += sizeof(deal_message_87.news_content_87.application_version_num);
	deal_message_87.news_content_len += sizeof(deal_message_87.news_content_87.application_version_num);
	
	deal_message_87.news_content_87.display_flag = '1';
	deal_message_87.length += sizeof(deal_message_87.news_content_87.display_flag);
	deal_message_87.news_content_len += sizeof(deal_message_87.news_content_87.display_flag);
	
	deal_message_87.news_content_87.password_keyboard_num[0] = 0x00;
	deal_message_87.news_content_87.password_keyboard_num[1] = 0x00;
	deal_message_87.news_content_87.password_keyboard_num[2] = 0x00;
	deal_message_87.news_content_87.password_keyboard_num[3] = 0x00;
	deal_message_87.news_content_87.password_keyboard_num[4] = 0x11;
	deal_message_87.news_content_87.password_keyboard_num[5] = 0x11;
	deal_message_87.news_content_87.password_keyboard_num[6] = 0x11;
	deal_message_87.news_content_87.password_keyboard_num[7] = 0x11;
	deal_message_87.length += sizeof(deal_message_87.news_content_87.password_keyboard_num);
	deal_message_87.news_content_len += sizeof(deal_message_87.news_content_87.password_keyboard_num);
	
	deal_message_87.news_content_87.deal_num[0] = 0x00;
	deal_message_87.news_content_87.deal_num[1] = 0x00;
	deal_message_87.news_content_87.deal_num[2] = 0x20;
	deal_message_87.length += sizeof(deal_message_87.news_content_87.deal_num);
	deal_message_87.news_content_len += sizeof(deal_message_87.news_content_87.deal_num);
	
	switch (1)
	{
	    //case AUTHENTICATION:
	    case 1:
	    {
	        memcpy(deal_message_87.news_content_87.deal_code, "150", strlen("150"));
	        deal_message_87.length += sizeof(deal_message_87.news_content_87.deal_code);
	        deal_message_87.news_content_len += sizeof(deal_message_87.news_content_87.deal_code);
	        
	        flowsheet_len = 7;
	        if ((deal_message_87.news_content_87.flowsheet_code = malloc(6 + 1)) == NULL)
	        {
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", MALLOC_ERR); 
                return MALLOC_ERR;
	        }
	        deal_message_87.news_content_87.flowsheet_code[0] = 0x03;
	        deal_message_87.news_content_87.flowsheet_code[1] = 0X0C;
	        deal_message_87.news_content_87.flowsheet_code[2] = 0X92;
	        deal_message_87.news_content_87.flowsheet_code[3] = 0X0C;
	        deal_message_87.news_content_87.flowsheet_code[4] = 0X28;
	        deal_message_87.news_content_87.flowsheet_code[5] = 0X0C;
	        deal_message_87.news_content_87.flowsheet_code[6] = 0X2B;
	        deal_message_87.length += 7;
	        deal_message_87.news_content_len += 7;
	        
	        deal_message_87.news_content_87.valid_data_length = 46 + 3 + 8;
	        deal_message_87.length += sizeof(deal_message_87.news_content_87.valid_data_length);
	        deal_message_87.news_content_len += sizeof(deal_message_87.news_content_87.valid_data_length);
	        
	        if ((deal_message_87.news_content_87.valid_data_zone = malloc(deal_message_87.news_content_87.valid_data_length + 1)) == NULL)
	        {
	            res = MALLOC_ERR;
                OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "malloc failed!", res); 
                goto EXIT;
	        }
	        
	        i = 0;
	        deal_message_87.news_content_87.valid_data_zone[i] = 34;
	        i++;
	        memcpy((deal_message_87.news_content_87.valid_data_zone + i), ((struct s_dial_back_respond *)para)->BASE_id, 34);	       
	        i += 34;
	        
	        deal_message_87.news_content_87.valid_data_zone[i] = sizeof(((struct s_dial_back_respond *)para)->base_random);
	        i++;
	        memcpy((deal_message_87.news_content_87.valid_data_zone + i), (char *)&((struct s_dial_back_respond *)para)->base_random, sizeof(((struct s_dial_back_respond *)para)->base_random));
	        PRINT("*(long*)(deal_message_87.news_content_87.valid_data_zone + i) = %08X\n", *(long*)(deal_message_87.news_content_87.valid_data_zone + i));
	        PRINT("((struct s_dial_back_respond *)para)->base_random = %08X\n", ((struct s_dial_back_respond *)para)->base_random);
	        i += sizeof(((struct s_dial_back_respond *)para)->base_random);
   
	        deal_message_87.news_content_87.valid_data_zone[i] = sizeof(((struct s_dial_back_respond *)para)->terrace_random_mac);
	        i++;	       
	        common_tools.get_MAC(((struct s_dial_back_respond *)para)->terrace_random, ((struct s_dial_back_respond *)para)->terrace_random_mac);
	        memcpy((deal_message_87.news_content_87.valid_data_zone + i), ((struct s_dial_back_respond *)para)->terrace_random_mac, sizeof(((struct s_dial_back_respond *)para)->terrace_random_mac));	       
	        i += sizeof(((struct s_dial_back_respond *)para)->terrace_random_mac);
	        
	        deal_message_87.length += deal_message_87.news_content_87.valid_data_length;
	        deal_message_87.news_content_len += deal_message_87.news_content_87.valid_data_length;        
	        break;  
	    }
	    default:
	    {
	        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "deal type error!", DEAL_TYPE_ERR);
	        return DEAL_TYPE_ERR;
	    }
	}
    
    list_len = deal_message_87.length + 4;
    //ENDIAN
    deal_message_87.length = htons(deal_message_87.length);
    deal_message_87.news_content_len= htons(deal_message_87.news_content_len);
    // 把结构体数据添加到链表中去
	for (i = 0; i < (list_len - flowsheet_len - deal_message_87.news_content_87.valid_data_length - 3); i++)
	{
	    if ((res = common_tools.list_tail_add_data(a_data_list, ((char *)&deal_message_87)[i])) < 0)
	    {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_tail_add_data failed!", res);
	        goto EXIT;
	    } 
	}
	
	// 把流程代码数据添加到链表中去
	for (i = 0; i < flowsheet_len; i++)
	{
	    if ((res = common_tools.list_tail_add_data(a_data_list, deal_message_87.news_content_87.flowsheet_code[i])) < 0)
	    {      
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_tail_add_data failed!", res);
	        goto EXIT;
	    }
	}
	
	// 有效数据长度
    common_tools.list_tail_add_data(a_data_list, (char)((deal_message_87.news_content_87.valid_data_length & 0xFF00) >> 8));
    common_tools.list_tail_add_data(a_data_list, (char)(deal_message_87.news_content_87.valid_data_length & 0x00FF));
	
	// 把有效数据添加到链表中去
	for (i = 0; i < deal_message_87.news_content_87.valid_data_length - 8; i++)
	{
	    if ((res = common_tools.list_tail_add_data(a_data_list, deal_message_87.news_content_87.valid_data_zone[i])) < 0)
	    {       
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_tail_add_data failed!", res);
	        goto EXIT;
	    }
	}
	if ((res = common_tools.get_list_MAC(a_data_list, (unsigned long)&deal_message_87.news_content_87 - (unsigned long)&deal_message_87, ntohs(deal_message_87.news_content_len) - 8, deal_message_87.news_content_87.MAC)) < 0)
	{    
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "get_list_MAC failed!", res);  
        goto EXIT;
	}
	// 把MAC添加到链表中去
	for (i = 0; i < 8; i++)
	{
	    if ((res = common_tools.list_tail_add_data(a_data_list, deal_message_87.news_content_87.MAC[i])) < 0)
	    {
            OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_tail_add_data failed!", res);
	        goto EXIT;
	    }
	}
	
    //产生校验和
    res = common_tools.get_checkbit(NULL, a_data_list, 0, a_data_list->list_len, SUM, 1);
    if ((res == MISMATCH_ERR) || (res == NULL_ERR))
    {
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__,  "get_checkbit failed!", res);
        goto EXIT;
    }
    deal_message_87.check_sum = (char)res;
    
    if ((res = common_tools.list_tail_add_data(a_data_list, deal_message_87.check_sum)) < 0)
	{
        OPERATION_LOG(__FILE__, __FUNCTION__, __LINE__, "list_tail_add_data failed!", res);
	    goto EXIT;
	}
    PRINT_BUF_BY_HEX(NULL, a_data_list, a_data_list->list_len, __FILE__, __FUNCTION__, __LINE__);

EXIT:
    if (deal_message_87.news_content_87.flowsheet_code != NULL)
    {
	    free(deal_message_87.news_content_87.flowsheet_code);
	    deal_message_87.news_content_87.flowsheet_code = NULL;
	}
	if (deal_message_87.news_content_87.valid_data_zone != NULL)
	{
	    free(deal_message_87.news_content_87.valid_data_zone);
	    deal_message_87.news_content_87.valid_data_zone = NULL;
	}
	PRINT_STEP("exit...\n");
	return 0;	
}
