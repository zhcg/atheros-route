#include "common.h"
#include "phone_control.h"
#include "phone_audio.h"
#include "phone_action.h"
#include "dtmf_extr.h"
#include "fsk_external.h"
#include "fsk_internal.h"
#ifdef REGISTER
#include "sqlite3.h"
#endif

struct class_phone_control phone_control =
{
	.phone_control_sockfd=0,//控制句柄
	.global_onhook_success=0, //挂机成功标志
	.global_offhook_success=0,
	.dial_flag=0,
	.global_incoming=0,
	.global_phone_is_using=0,
	.global_talkback=0,
	.start_dial=0,
	.ringon_flag=0,
	.send_dtmf=0,
	.handle_up_msg_wp_pos = 0,
	.handle_up_msg_rd_pos = 0,
	.check_ringon_count=0,
	.send_sw_ringon_count=0,
	.send_tb_ringon_count=0,
	.send_sw_ringon_limit=0,
	.send_tb_ringon_limit=0,
	.sw_dev = -1,
	.tb_dev = -1,
	.cli_req_buf_wp=0,
	.cli_req_buf_rp=0,
	.last_cli_length=0,
	.get_fsk = 0, //获取fsk号码
	.ring_count = 0, //来电标志后计数
	.ring_neg_count = 0,
	.ring_pos_count = 0,
	.passage_fd = -1,
	.called_test = 0,
	.vloop = 0,
};

dev_status_t devlist[CLIENT_NUM];//设备列表
unsigned char handle_up_msg_buffer[BUFFER_SIZE_1K];//上行报文接收buf
cli_request_t cli_req_buf[CLI_REQ_MAX_NUM];
int phone_control_fd[2];
char tb_sendbuf[SENDBUF]={0};
unsigned char nothing_to_do_buf[BUFFER_SIZE_1K];

static const char ringon[3]={0x03,0x00,0x03};
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

//socket发送函数
int netWrite(int fd,const void *buffer,int length)
{
	int bytes_left;
	int written_bytes;
	const char *ptr;
	ptr = (const char *)buffer;
	bytes_left = length;
	while(bytes_left > 0) {
		//begin to write
		written_bytes = write(fd,ptr,bytes_left);
		if(written_bytes <= 0) { //error
			if(errno == EINTR) //interrupt error, we continue writing
				written_bytes = 0;
			else             //other error, exit
				return (-1);
		}
		bytes_left -= written_bytes;
		ptr += written_bytes;     //from where left,  continue writing
	}
	return (0);
}

#ifdef REGISTER
/**
 * 数据查询
 */
int sqlite3_interface(char *tb_name,char *data_name, char *data_value,char *where_name,char *out_value)
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
    strncat(sql, data_value, strlen(data_value));
    strcat(sql, "\"");
    
    PRINT("sql:%s\n",sql);
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
#endif
//销毁连接
int destroy_client(dev_status_t *dev)
{
	PRINT("%s\n",__FUNCTION__);
	PRINT("ip = %s\n",dev->client_ip);
	char sendbuf[SENDBUF]={0};
	int i,j,k,count=0;
	dev->dying = 1;
	close(dev->client_fd);
	if(dev->dev_is_using == 1 && dev->client_fd == phone_control.who_is_online.client_fd)
	{
		PRINT("dev is using,but connection error\n");
		for(i=0;i<200;i++)
		{
			usleep(10*1000);
			for(j=0;j<CLIENT_NUM;j++)
			{
				if(!strcmp(phone_control.who_is_online.client_ip,devlist[j].client_ip) && devlist[j].control_reconnect==1)
				{
					PRINT("found reconnect client\n");
					dev->client_fd=devlist[j].client_fd;
					dev->dev_is_using = 1;
					phone_control.who_is_online.client_fd=devlist[j].client_fd;
					phone_control.who_is_online.dev_is_using = 1;

					memset(&devlist[j],0,sizeof(dev_status_t));
					devlist[j].client_fd = -1;
					devlist[j].audio_client_fd = -1;
					devlist[j].id = -1;
					devlist[j].registered = 0;
					memset(devlist[j].client_ip,0,sizeof(devlist[j].client_ip));
					for(k=0; k<CLIENT_NUM; k++)
						printf("%d:cli_fd=%d  , cli_ip=%s\n", k ,devlist[k].client_fd,devlist[k].client_ip);
					dev->dying = 0;
					return 0;
				}
			}
		}
	}

	if(dev->dev_is_using && phone_control.global_phone_is_using)
	{
		for(i=0;i<CLIENT_NUM;i++)
		{
			if(devlist[i].dev_is_using == 1)
				count++;
		}
		dev->dev_is_using = 0;
		if(count == 1)
		{
			phone_control.global_phone_is_using = 0;
			onhook();
			if(!set_onhook_monitor())
				PRINT("set_onhook_monitor success\n");
			//开始检测incoming
			start_read_incoming();
			dev->attach=0;
			dev->audio_reconnect=0;
			clean_who_is_online();
		}
		stopaudio(dev,PSTN);
	}
	if(dev->talkbacking == 1 || dev->talkbacked == 1)
	{
		do_cmd_talkbackonhook(dev,sendbuf);
		phone_control.global_talkback = 0;
	}
	if(dev->isswtiching)
	{
		dev->isswtiching = 0;
			onhook();
			if(!set_onhook_monitor())
				PRINT("set_onhook_monitor success\n");
			//开始检测incoming
			start_read_incoming();
	}
	close(dev->audio_client_fd);
	memset(dev, 0, sizeof(dev_status_t));
	dev->client_fd = -1;
	dev->audio_client_fd = -1;
	dev->id = -1;
	dev->registered = 0;
	dev->dying = 0;
	memset(dev->client_ip,0,sizeof(dev->client_ip));
	generate_up_msg();

	return 0;
}

//清除占用函数
void clean_who_is_online()
{
	memset(&phone_control.who_is_online,0,sizeof(dev_status_t));
	phone_control.who_is_online.client_fd = -1;
	phone_control.who_is_online.audio_client_fd = -1;
}

int do_cmd_offhook(dev_status_t *dev)
{
	int i,count=0;
	int offhook_limit = 0;//挂机尝试次数限制
	char sendbuf[SENDBUF]={0};

	if(phone_control.global_incoming == 1)
	{
		if(dev->incoming == 1)
		{
			phone_audio.start_recv = 0;
			goto OFFHOOK;
		}
		else
		{
			goto OFFHOOK_ERR;
		}
	}
OFFHOOK:
#ifdef REGISTER
	if(!dev->dev_is_using && dev->audio_client_fd>=0 && phone_control.global_phone_is_using==0 && dev->registered)
#else
	if(!dev->dev_is_using && dev->audio_client_fd>=0 && phone_control.global_phone_is_using==0)
#endif	
{
		for(i=0;i<CLIENT_NUM;i++)
		{
			if(devlist[i].client_fd == -1 || devlist[i].client_fd == dev->client_fd)
				continue;
			if(devlist[i].isswtiching == 1)
				count++;
		}
		//转子机的过程中禁止其他子机摘机
		if(count>0)
		{
			goto OFFHOOK_ERR;
		}

		stop_read_incoming();

		for(i=0;i<CLIENT_NUM;i++)
		{
			if(devlist[i].client_fd == -1)
				continue;
			if(devlist[i].talkbacking || devlist[i].talkbacked)
			{
				PRINT("stop talkbacking!\n");
				stopaudio(&devlist[i],TALKBACK);
				do_cmd_talkbackonhook(&devlist[i],sendbuf);
				usleep(300*1000);
				break;
			}
		}

		phone_control.global_phone_is_using = 1;
		if(dev->isswtiching == 0)
		{
			if(!phone_control.global_incoming)
			{
				onhook();
			}
		}
		dev->dev_is_using = 1;
		phone_control.start_dial = 1;

		startaudio(dev);

		if(dev->isswtiching == 0)
		{
START_OFFHOOK:
			offhook();
			phone_control.global_offhook_success = 1;

			for(i=0;i<75;i++)
			{
				if(phone_control.Status.offhook == 1)
				{
					goto OFFHOOK_SUCCESS;
				}
				usleep(2000);
			}
			if(offhook_limit == 1)
				goto OFFHOOK_SUCCESS;
			offhook_limit++;
			goto START_OFFHOOK;//挂机无响应，重复挂机

OFFHOOK_SUCCESS:
			offhook_limit = 0;
		}
		dev->attach=1;
		memcpy(&phone_control.who_is_online,dev,sizeof(dev_status_t));
		phone_control.global_incoming = 0;
		dev->incoming = 0;
		phone_control.ringon_flag = 0;
		if(phone_control.vloop > 3 || phone_control.vloop < -3)
			led_control(LED_OFFHOOK_IN);
		else
			led_control(LED_OFFHOOK_OUT);
	}
	else
	{
		goto OFFHOOK_ERR;
	}
	return 0;
OFFHOOK_ERR:
	memset(sendbuf,0,SENDBUF);
	snprintf(sendbuf, 23,"HEADR0011INUSING0014\r\n");
	netWrite(dev->client_fd, sendbuf, strlen(sendbuf));
	PRINT("INUSING\n");
	usleep(200*1000);
	if(dev->audio_client_fd > 0)
	{
		close(dev->audio_client_fd);
		dev->audio_client_fd = -1;
	}
	return -1;
}

int do_cmd_onhook(dev_status_t *dev)
{
	int onhook_limit = 0;//挂机尝试次数限制
	dev->incoming = 0;
#ifdef REGISTER
	if(dev->dev_is_using || phone_control.global_incoming || dev->isswtiching || dev->registered == 0)
#else
	if(dev->dev_is_using || phone_control.global_incoming || dev->isswtiching)
#endif
	{
		int i,count=0;
		for(i=0;i<CLIENT_NUM;i++)
		{
			if(devlist[i].client_fd<0)
				continue;
			if(devlist[i].dev_is_using == 1)
				count++;
		}
		if(count>1)
		{
			dev->dev_is_using = 0;
			return -1;
		}
		if(phone_control.global_incoming)
		{
			for(i=0;i<CLIENT_NUM;i++)
			{
				if(devlist[i].client_fd<0)
					continue;
				if(devlist[i].audio_client_fd > 0)
				{
					return 0;
				}
			}
			offhook();
			for(i=0;i<CLIENT_NUM;i++)
			{
				if(devlist[i].client_fd<0)
					continue;
				if(devlist[i].incoming > 0)
				{
					devlist[i].incoming = 0;
				}
			}
			phone_control.global_incoming = 0;
			usleep(400*1000);
		}
		phone_control.global_phone_is_using = 0;
		dev->dev_is_using = 0;
		dev->isswtiching = 0;
		clean_who_is_online();
		stopaudio(dev,PSTN);
START_ONHOOK:
		onhook();
		if(!set_onhook_monitor())
			PRINT("set_onhook_monitor success\n");
		phone_control.global_onhook_success = 1;

		for(i=0;i<75;i++)
		{
			if(phone_control.Status.offhook == 0)
			{
				goto ONHOOK_SUCCESS;
			}
			usleep(2000);
		}
		if(onhook_limit == 1)
			goto ONHOOK_SUCCESS;
		onhook_limit++;
		goto START_ONHOOK;//挂机无响应，重复挂机

ONHOOK_SUCCESS:
		//usleep(50*1000);
		phone_audio.audio_reconnect_flag = 0;
		onhook_limit = 0;
		dev->attach=0;
		dev->audio_reconnect=0;

		start_read_incoming();
		if(phone_control.vloop > 3 || phone_control.vloop < -3)
			led_control(LED_LINE_IN);
		else
			led_control(LED_LINE_OUT);

		//sleep(1);
		return 0;
	}
	if(dev->audio_client_fd >= 0)
	{
		close(dev->audio_client_fd);
		dev->audio_client_fd = -1;
	}
	return 0;
}

int do_cmd_talkback(dev_status_t* dev,char *sendbuf)
{
	/*			HEADR0011INUSING0012
	 * 			1	成功
	 *          2   设备信息错误
	 * 			3   不在线
	 * 			4   线路忙
	 * */

	int i;
#ifdef REGISTER
	if(dev->dev_is_using == 1 || phone_control.global_incoming == 1 ||phone_control.global_phone_is_using == 1 || phone_control.global_talkback == 1 || dev->registered == 0)
#else
	if(dev->dev_is_using == 1 || phone_control.global_incoming == 1 ||phone_control.global_phone_is_using == 1 || phone_control.global_talkback == 1)
#endif
	{
			memset(sendbuf,0,SENDBUF);
			snprintf(sendbuf, 23,"HEADR0011INUSING0014\r\n");
			goto TALKBACK_ERR;
	}
	for(i =0;i<CLIENT_NUM;i++)
	{
		if(devlist[i].client_fd == -1)
			continue;
		if(devlist[i].isswtiching == 1)
		{
			memset(sendbuf,0,SENDBUF);
			snprintf(sendbuf, 23,"HEADR0011INUSING0014\r\n");
			goto TALKBACK_ERR;
		}
	}
	//HEADS 0025 INTERPH 013 1 10.10.10.101
	int id;
	char client_ip[16]={0};
	char flag;
	//解析转子机命令得到id和ip
	id = (int)cli_req_buf[phone_control.cli_req_buf_rp].arg[0]-48;
	memcpy(client_ip,&cli_req_buf[phone_control.cli_req_buf_rp].arg[1],cli_req_buf[phone_control.cli_req_buf_rp].arglen-1);//10.10.10.103 //172.16.0.1
	client_ip[cli_req_buf[phone_control.cli_req_buf_rp].arglen]='\0';
	PRINT("talkback id = %d\n",id);
	PRINT("talkback ip = %s\n",client_ip);

	if(id<0 || id>=CLIENT_NUM)
	{
		PRINT("ID error\n");
		memset(sendbuf,0,SENDBUF);
		snprintf(sendbuf, 23,"HEADR0011INUSING0012\r\n");
		goto TALKBACK_ERR;
	}
	if(devlist[id].client_fd <0)
	{
	    memset(sendbuf,0,SENDBUF);
		snprintf(sendbuf, 23,"HEADR0011INUSING0012\r\n");
		PRINT("dev is not online\n");
		goto TALKBACK_ERR;
	}
	if(strcmp(devlist[id].client_ip,client_ip))
	{
	    memset(sendbuf,0,SENDBUF);
		snprintf(sendbuf, 23,"HEADR0011INUSING0012\r\n");
		PRINT("ip error\n");
		goto TALKBACK_ERR;
	}
	memset(sendbuf,0,SENDBUF);
	snprintf(sendbuf, strlen(client_ip)+38,
	"HEADR0%03dTALKBAC%03d%d%s%s\r\n",strlen(dev->client_ip)+27,strlen(dev->client_ip)+17,dev->id,dev->client_ip,dev->dev_name);
	PRINT("%s\n",sendbuf);
	netWrite(devlist[id].client_fd, sendbuf, strlen(sendbuf));
	devlist[id].talkbacked = 1;
	dev->talkbacking = 1;
	phone_control.global_talkback = 1;
             memset(sendbuf,0,SENDBUF);
	snprintf(sendbuf, 23,"HEADR0011INUSING0011\r\n");
	netWrite(dev->client_fd, sendbuf, strlen(sendbuf));
	return 0;

TALKBACK_ERR:
	netWrite(dev->client_fd, sendbuf, strlen(sendbuf));
	PRINT("INUSING\n");
	if(dev->audio_client_fd > 0)
	{
			close(dev->audio_client_fd);
			dev->audio_client_fd = -1;
	}
	return -1;
}

int do_cmd_talkbackoffhook(dev_status_t* dev,char *sendbuf)
{
	int i,flag = 0;
	for(i=0;i<CLIENT_NUM;i++)
	{
		if(devlist[i].client_fd == -1)
			continue;
		if(devlist[i].talkbacking == 1)
		{	
			flag ++;
		}
		if(devlist[i].talkbacked == 1)
		{	
			flag ++;
		}
	}
	if(flag != 2)
	{
		goto TALKBACKOFFHOOK_ERR;
	}
	memset(sendbuf,0,SENDBUF);
	snprintf(sendbuf, 22,"HEADR0010STARTTB000\r\n");
	netWrite(dev->client_fd, sendbuf, strlen(sendbuf));
	for(i=0;i<CLIENT_NUM;i++)
	{
		if(devlist[i].client_fd == -1)
			continue;
		if(devlist[i].talkbacking == 1)
		{
			netWrite(devlist[i].client_fd, sendbuf, strlen(sendbuf));
		}
	}
	startaudio(dev);
	dev->talkback_answer = 1;
	return 0;
TALKBACKOFFHOOK_ERR:
	memset(sendbuf,0,SENDBUF);
	snprintf(sendbuf, 23,"HEADR0011INUSING0014\r\n");
	netWrite(dev->client_fd, sendbuf, strlen(sendbuf));
	PRINT("INUSING\n");
	if(dev->audio_client_fd > 0)
	{
		close(dev->audio_client_fd);
		dev->audio_client_fd = -1;
	}
	return -1;
}

int do_cmd_talkbackonhook(dev_status_t* dev,char *sendbuf)
{
	int flag = 0;
	int i;

	for(i=0;i<CLIENT_NUM;i++)
	{
		if(devlist[i].client_fd == -1)
			continue;
		if(devlist[i].dev_is_using == 1)
		flag = 1;
	}
	if(flag == 0)
	{
		stopaudio(dev,TALKBACK);
	}
	memset(sendbuf,0,SENDBUF);
	snprintf(sendbuf, 22,"HEADR0010STOPTB_000\r\n");
	netWrite(dev->client_fd, sendbuf, strlen(sendbuf));
	if(dev->talkbacked || dev->talkbacking)
	{
		for(i=0;i<CLIENT_NUM;i++)
		{
			if(devlist[i].client_fd == -1)
				continue;
			if(devlist[i].talkbacking == 1)
			{
				devlist[i].talkbacking = 0;
				memset(sendbuf,0,SENDBUF);
				snprintf(sendbuf, 22,"HEADR0010STOPTB_000\r\n");
				netWrite(devlist[i].client_fd, sendbuf, strlen(sendbuf));
			}
			if(devlist[i].talkbacked == 1)
			{
				devlist[i].talkbacked = 0;
				devlist[i].talkback_answer = 0;
				memset(sendbuf,0,SENDBUF);
				snprintf(sendbuf, 22,"HEADR0010STOPTB_000\r\n");
				netWrite(devlist[i].client_fd, sendbuf, strlen(sendbuf));
			}
		}
		phone_control.global_talkback = 0;
	}
	return 0;
}

#ifdef REGISTER
int do_cmd_heartbeat(dev_status_t *dev,char *buf)
{
	int i,j,count =0;

	if(dev->tick_time == 0)
	{
		char dev_mac[20]={0};
		char register_state[4]={0};
		char pad_mac[20]={0};
		char insidebuf[10]={0};
		char device_name[30]={0};
		for(i=0;i<strlen(buf);i++)
		{
			if(buf[i]==' ')
				buf[i] = '\0';
		}
		PRINT("first heartbeat!\n");
		sqlite3_interface("terminal_register_tb","device_mac",buf,"device_mac",dev_mac);
		PRINT("dev_mac=%s\n",dev_mac);
		PRINT("buf=%s\n",buf);
		if(!strncmp(dev_mac,buf,strlen(buf)))
		{
			PRINT("dev is registered!\n");
			sqlite3_interface("terminal_register_tb","device_name",buf,"device_mac",device_name);
			memset(dev->dev_name,' ',16);
			memcpy(dev->dev_name,device_name,16);
			for(i=0;i<16;i++)
			{
				if(dev->dev_name[i] == '\0')
				{
					dev->dev_name[i]=' ';
				}
			}
			goto REGISTERED;
		}
		sqlite3_interface("terminal_base_tb","register_state","0","register_state",register_state);
		//PRINT("register_state = %s\n",register_state);
		sqlite3_interface("terminal_base_tb","pad_mac",buf,"pad_mac",pad_mac);
		//PRINT("buf=%s\n",buf);
		//PRINT("pad_mac = %s\n",pad_mac);
		//PRINT("%d\n",strcmp(pad_mac,buf));
		//PRINT("%d\n",strcmp(register_state,"0"));
		if(!strncmp(pad_mac,buf,strlen(buf)) && !strcmp(register_state,"0"))
		{
			PRINT("pad is registered!\n");
			memset(dev->dev_name,' ',16);
			memcpy(dev->dev_name,"主机",6);
			for(i=0;i<16;i++)
			{
				if(dev->dev_name[i] == '\0')
				{
					dev->dev_name[i]=' ';
				}
			}
			goto REGISTERED;
		}
		PRINT("dev is unregistered\n");
		for(i=0;i<CLIENT_NUM;i++)
		{
			if(devlist[i].client_fd == -1)
				continue;
			if(dev == &devlist[i])
				break;
		}
		memset(insidebuf,0,10);
		sprintf(insidebuf,"%s%d","INSIDE",i);
		netWrite(phone_control_fd[0],insidebuf, strlen(insidebuf));
		return -1;
	}
REGISTERED:
	dev->tick_time++;
	dev->registered = 1;
	return 0;
}
#else

int do_cmd_heartbeat(dev_status_t *dev)
{
	int i,j,count =0;

	dev->tick_time++;
	//memset(dev->dev_name,' ',16);
	//for(i=0;i<CLIENT_NUM;i++)
	//{
		//if(devlist[i].client_fd == -1)
			//continue;
		//if(dev == &devlist[i])
		//{
			//sprintf(dev->dev_name,"%s%d","子机",i+1);
		//}
	//}
	//for(i=0;i<16;i++)
	//{
		//if(dev->dev_name[i] == '\0')
		//{
			//dev->dev_name[i]=' ';
		//}
	//}
	return 0;
}
#endif
int do_cmd_dialup(dev_status_t* dev)
{
	int i,j,count=0;
#ifdef REGISTER
	if(dev->dev_is_using && dev->registered)
#else
	if(dev->dev_is_using)
#endif
	{
		for(i=0;i<CLIENT_NUM;i++)
		{
			if(devlist[i].dev_is_using == 1)
				count++;
		}
		if(count==1)
		{
			memset(phone_control.telephone_num,0,64);
			memcpy(phone_control.telephone_num, cli_req_buf[phone_control.cli_req_buf_rp].arg,cli_req_buf[phone_control.cli_req_buf_rp].arglen);
			phone_control.telephone_num[cli_req_buf[phone_control.cli_req_buf_rp].arglen]='\0';
			PRINT("phone_num:%s\n",phone_control.telephone_num);
			phone_control.telephone_num_len = cli_req_buf[phone_control.cli_req_buf_rp].arglen;
			dialup(phone_control.telephone_num, cli_req_buf[phone_control.cli_req_buf_rp].arglen);
		}
	}
	return 0;
}

int do_cmd_dtmf(dev_status_t* dev)
{
#ifdef REGISTER
	if(dev->dev_is_using && dev->registered)
#else
	if(dev->dev_is_using)
#endif
	{
		phone_control.dtmf=cli_req_buf[phone_control.cli_req_buf_rp].arg[0];
		PRINT("dtmf=%c\n",phone_control.dtmf);
		senddtmf(phone_control.dtmf);
	}
	return 0;
}

int do_cmd_switch(dev_status_t* dev,char *sendbuf)
{
	/*			HEADR0011SON_RET0012
	 * 			1	成功
	 *          2   设备信息错误
	 * 			3   不在线
	 * */
#ifdef REGISTER
	if(dev->dev_is_using && dev->registered)
#else
	if(dev->dev_is_using)
#endif
	{
		//HEADSR XXX 转子机 XXX XXX IP x xxx NUM
		int id;
		char client_ip[16]={0};
		char flag;
		//解析转子机命令得到id和ip
		id = (int)cli_req_buf[phone_control.cli_req_buf_rp].arg[0]-48;
		memcpy(client_ip,&cli_req_buf[phone_control.cli_req_buf_rp].arg[1],cli_req_buf[phone_control.cli_req_buf_rp].arglen-1);//10.10.10.103 //172.16.0.1
		client_ip[cli_req_buf[phone_control.cli_req_buf_rp].arglen]='\0';
		PRINT("switch id = %d\n",id);
		PRINT("switch ip = %s\n",client_ip);

		if(id<0 || id>=CLIENT_NUM)
		{
			PRINT("ID error\n");
			memset(sendbuf,0,SENDBUF);
			snprintf(sendbuf, 23,"HEADR0011SON_RET0012\r\n");
			goto SWITCH_ERR;
		}
		if(devlist[id].client_fd <0)
		{
			PRINT("dev is not online\n");
			memset(sendbuf,0,SENDBUF);
			snprintf(sendbuf, 23,"HEADR0011SON_RET0012\r\n");
			goto SWITCH_ERR;
		}
		if(strcmp(devlist[id].client_ip,client_ip))
		{
			PRINT("ip error\n");
			memset(sendbuf,0,SENDBUF);
			snprintf(sendbuf, 23,"HEADR0011SON_RET0012\r\n");
			goto SWITCH_ERR;
		}
		memset(sendbuf,0,SENDBUF);
		//主叫转子机得到号码
		snprintf(sendbuf, 22+phone_control.telephone_num_len, "HEADR0%03dINCOMIN%03d%s\r\n",10+phone_control.telephone_num_len, phone_control.telephone_num_len, phone_control.telephone_num);
		PRINT("%s\n",sendbuf);
	              netWrite(devlist[id].client_fd, sendbuf, strlen(sendbuf));
		devlist[id].isswtiching = 1;
		phone_control.global_phone_is_using = 0;
		dev->dev_is_using = 0;
		dev->attach=0;
		dev->audio_reconnect=0;
		clean_who_is_online();
		stopaudio(dev,PSTN);
		memset(sendbuf,0,SENDBUF);
		snprintf(sendbuf, 23,"HEADR0011SON_RET0011\r\n");
		netWrite(dev->client_fd, sendbuf, strlen(sendbuf));
	}
	return 0;
SWITCH_ERR:
	netWrite(dev->client_fd, sendbuf, strlen(sendbuf));
	PRINT("INUSING\n");
	return -1;
}

int do_cmd_req_switch(dev_status_t * dev, char * sendbuf)
{
#ifdef REGISTER
        if(dev->dev_is_using && dev->registered)
#else
        if(dev->dev_is_using )
#endif
        {
                int i,id;
                char client_ip[16]={0};
                char flag;
                id = (int)cli_req_buf[phone_control.cli_req_buf_rp].arg[0]-48;
                memcpy(client_ip,&cli_req_buf[phone_control.cli_req_buf_rp].arg[1],cli_req_buf[phone_control.cli_req_buf_rp].arglen-1);//10.10.10.103 //172.16.0.1
                client_ip[cli_req_buf[phone_control.cli_req_buf_rp].arglen]='\0';
                PRINT("req switch id = %d\n",id);
                PRINT("req switch ip = %s\n",client_ip);

                if(id<0 || id>=CLIENT_NUM)
                {
                	PRINT("ID error\n");
                	memset(sendbuf,0,SENDBUF);
                	snprintf(sendbuf, 23,"HEADR0011RET_BTP0012\r\n");
                	goto REQ_SWITCH_ERR;
                }
                if(devlist[id].client_fd <0)
                {
                	PRINT("dev is not online\n");
                	memset(sendbuf,0,SENDBUF);
                	snprintf(sendbuf, 23,"HEADR0011RET_BTP0012\r\n");
                	goto REQ_SWITCH_ERR;
                }
                if(strcmp(devlist[id].client_ip,client_ip))
                {
                	PRINT("ip error\n");
                	memset(sendbuf,0,SENDBUF);
                	snprintf(sendbuf, 23,"HEADR0011RET_BTP0012\r\n");
                	goto REQ_SWITCH_ERR;
                }
                for(i=0;i<CLIENT_NUM;i++)
                {
                        if(devlist[i].client_fd == -1)
                                continue;
                        if(&devlist[i] == dev)
                        {
                               // id = i ;
                                break;
                        }
                }
                memset(sendbuf,0,SENDBUF);
                snprintf(sendbuf,strlen(dev->client_ip)+23
                ,"HEADR0%03dREQ_BTP%03d%d%s\r\n",strlen(dev->client_ip)+11,strlen(dev->client_ip)+1,i,dev->client_ip);
                PRINT("%s\n",sendbuf);
                netWrite(devlist[id].client_fd, sendbuf, strlen(sendbuf));
        }
        return 0;
REQ_SWITCH_ERR:
        netWrite(dev->client_fd, sendbuf, strlen(sendbuf));
        return -1;
}

int do_cmd_req_talk(dev_status_t * dev, char * sendbuf)
{
#ifdef REGISTER
        if(dev->dev_is_using == 1 || phone_control.global_incoming == 1 ||phone_control.global_phone_is_using == 1 || phone_control.global_talkback == 1 || dev->registered == 0)
#else
        if(dev->dev_is_using == 1 || phone_control.global_incoming == 1 ||phone_control.global_phone_is_using == 1 || phone_control.global_talkback == 1)
#endif
        {
                memset(sendbuf,0,SENDBUF);
                snprintf(sendbuf, 23,"HEADR0011INUSING0014\r\n");
                goto REQ_TALK_ERR;
        }
        int i,id;
        char client_ip[16]={0};
        char flag;
        id = (int)cli_req_buf[phone_control.cli_req_buf_rp].arg[0]-48;
        memcpy(client_ip,&cli_req_buf[phone_control.cli_req_buf_rp].arg[1],cli_req_buf[phone_control.cli_req_buf_rp].arglen-1);//10.10.10.103 //172.16.0.1
        client_ip[cli_req_buf[phone_control.cli_req_buf_rp].arglen]='\0';
        PRINT("req talk id = %d\n",id);
        PRINT("req talk ip = %s\n",client_ip);

        if(id<0 || id>=CLIENT_NUM)
        {
                PRINT("ID error\n");
                memset(sendbuf,0,SENDBUF);
                snprintf(sendbuf, 23,"HEADR0011RET_BTP0012\r\n");
                goto REQ_TALK_ERR;
        }
        if(devlist[id].client_fd <0)
        {
                PRINT("dev is not online\n");
                memset(sendbuf,0,SENDBUF);
                snprintf(sendbuf, 23,"HEADR0011RET_BTP0012\r\n");
                goto REQ_TALK_ERR;
        }
        if(strcmp(devlist[id].client_ip,client_ip))
        {
                PRINT("ip error\n");
                memset(sendbuf,0,SENDBUF);
                snprintf(sendbuf, 23,"HEADR0011RET_BTP0012\r\n");
                goto REQ_TALK_ERR;
        }
        for(i=0;i<CLIENT_NUM;i++)
        {
                if(devlist[i].client_fd == -1)
                        continue;
                if(&devlist[i] == dev)
                {
                        //id = i ;
                        break;
                }
        }
        memset(sendbuf,0,SENDBUF);
        snprintf(sendbuf,strlen(dev->client_ip)+23
        ,"HEADR0%03dREQ_BTP%03d%d%s\r\n",strlen(dev->client_ip)+11,strlen(dev->client_ip)+1,i,dev->client_ip);
        PRINT("%s\n",sendbuf);
        netWrite(devlist[id].client_fd, sendbuf, strlen(sendbuf));
        return 0;
REQ_TALK_ERR:
        netWrite(dev->client_fd, sendbuf, strlen(sendbuf));
        return -1;
}

int do_cmd_ret_ptb(dev_status_t * dev, char * sendbuf)
{
        int i,id;
        char client_ip[16]={0};
        char flag;
        id = (int)cli_req_buf[phone_control.cli_req_buf_rp].arg[0]-48;
        memcpy(client_ip,&cli_req_buf[phone_control.cli_req_buf_rp].arg[1],cli_req_buf[phone_control.cli_req_buf_rp].arglen-1);//10.10.10.103 //172.16.0.1
        client_ip[cli_req_buf[phone_control.cli_req_buf_rp].arglen]='\0';
        PRINT("ret id = %d\n",id);
        PRINT("ret ip = %s\n",client_ip);

        if(id<0 || id>=CLIENT_NUM)
        {
                PRINT("ID error\n");
                goto RET_ERR;
        }
        if(devlist[id].client_fd <0)
        {
                PRINT("dev is not online\n");
                goto RET_ERR;
        }
        if(strcmp(devlist[id].client_ip,client_ip))
        {
                PRINT("ip error\n");
                goto RET_ERR;
        }
        memset(sendbuf,0,SENDBUF);
        snprintf(sendbuf, 23,"HEADR0011RET_BTP0011\r\n");
        netWrite(devlist[id].client_fd, sendbuf, strlen(sendbuf));
        return 0;
RET_ERR:
        return -1;
}

//消息处理
int parse_msg(cli_request_t* cli)
{
	dev_status_t* devp =cli->dev;
	char sendbuf[SENDBUF]={0};
	int i;
	switch(cli->cmd)
	{
		case HEARTBEAT:
		{
			PRINT("HEARTBEAT from %s\n",cli->dev->client_ip);
#ifdef REGISTER
			do_cmd_heartbeat(cli->dev,cli->arg);
#else
			do_cmd_heartbeat(cli->dev);
#endif
			break;
		}
		case DIALING:
		{
			PRINT("DIALING from %s\n",cli->dev->client_ip);
			do_cmd_dialup(cli->dev);
			break;
		}
		case OFFHOOK:
		{
			PRINT("OFFHOOK from %s\n",cli->dev->client_ip);
			do_cmd_offhook(cli->dev);
			break;
		}
		case ONHOOK:
		{
			PRINT("ONHOOK from %s\n",cli->dev->client_ip);
			do_cmd_onhook(cli->dev);
			break;
		}
		case DTMF_SEND:
		{
			PRINT("DTMF_SEND from %s\n",cli->dev->client_ip);
			do_cmd_dtmf(cli->dev);
			break;
		}
		//转子机
		case SWITCHTOSONMACHINE:
		{
			PRINT("SWITCHTOSONMACHINE from %s\n",cli->dev->client_ip);
			do_cmd_switch(cli->dev,sendbuf);
			break;
		}
		case TALK:
		{
			PRINT("TALKBACK from %s\n",cli->dev->client_ip);
			do_cmd_talkback(cli->dev,sendbuf);
			break;
		}
		case TALKBACKOFFHOOK:
		{
			PRINT("TALKBACKOFFHOOK from %s\n",cli->dev->client_ip);
			do_cmd_talkbackoffhook(cli->dev,sendbuf);
			break;
		}
		case TALKBACKONHOOK:
		{
			PRINT("TALKBACKONHOOK from %s\n",cli->dev->client_ip);
			do_cmd_talkbackonhook(cli->dev,sendbuf);
			break;
		}
		case REQ_SWITCH:
	    {
			   PRINT("REQ_SWITCH from %s\n",cli->dev->client_ip);
			   do_cmd_req_switch(cli->dev,sendbuf);
			   break;
		}
		 case REQ_TALK:
		{
				PRINT("REQ_TALK from %s\n",cli->dev->client_ip);
				do_cmd_req_talk(cli->dev,sendbuf);
				break;
		}
		case RET_PHONETOBASE:
		{
				PRINT("RET_PHONETOBASE from %s\n",cli->dev->client_ip);
				do_cmd_ret_ptb(cli->dev,sendbuf);
				break;
		}
		case DEFAULT:
		{
			PRINT("other cmd\n");
			break;
		}
	}
}


int getCmdtypeFromString(char *cmd_str)
{
	PstnCmdEnum cmdtype;

	if (strncmp(cmd_str, "OPTION_", 7) == 0)
	{
		cmdtype = HEARTBEAT;
	}
	else if (strncmp(cmd_str, "DIALING", 7) == 0)
	{
		cmdtype = DIALING;
	}
	 else if (strncmp(cmd_str, "OFFHOOK", 7) == 0)
	{
		cmdtype = OFFHOOK;
	}
	else if (strncmp(cmd_str, "ONHOOK_", 7) == 0)
	{
		cmdtype = ONHOOK;
	}
	else if (strncmp(cmd_str, "DTMFSND", 7) == 0)
	{
		cmdtype = DTMF_SEND;
	}
	else if (strncmp(cmd_str, "LOGIN__", 7) == 0)
	{
		cmdtype = LOGIN;
	}
	else if (strncmp(cmd_str, "SWTOSON", 7) == 0)
	{
		cmdtype = SWITCHTOSONMACHINE;
	}
	else if (strncmp(cmd_str, "INTERPH", 7) == 0)
	{
		cmdtype = TALK;
	}
	else if (strncmp(cmd_str, "TALKOFF", 7) == 0)
	{
		cmdtype = TALKBACKOFFHOOK;
	}
	else if (strncmp(cmd_str, "TALKON_", 7) == 0)
	{
		cmdtype = TALKBACKONHOOK;
	}
	else if (strncmp(cmd_str, "REQ_SWI", 7) == 0)
	{
		cmdtype = REQ_SWITCH;
	}
	else if (strncmp(cmd_str, "REQTALK", 7) == 0)
	{
		cmdtype = REQ_TALK;
	}
	else if (strncmp(cmd_str, "RET_PTB", 7) == 0)
	{
		cmdtype = RET_PHONETOBASE;
	}
	else
	{
		PRINT("command undefined\n");
		cmdtype = UNDEFINED;
	}

	return cmdtype;
}

int init_control()
{
	int sockfd,on,i;
	struct sockaddr_in servaddr,cliaddr;

	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd<=0)
	{
		perror("create control socket fail\n");
		exit(-1);
	}
	PRINT("control socket success\n");

	on = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(CONTROL_PORT);

	if(bind(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr)) == -1)
	{
		perror("control bind error");
		exit(-1);
	}
	PRINT("control bind success\n");

	if(listen(sockfd,10)<0)
	{
		perror("control listen error\n");
		exit(-1);
	}
	PRINT("control listen success\n");

	phone_control.phone_control_sockfd=sockfd;

	phone_audio.init_audio();

	if(si32178_init(0,0,5,5,10,1,1,0)==-1)
	{
		PRINT("si32178 init fail\n");
		exit(-1);
	}
	PRINT("si32178 init success\n");

	Fsk_InitParse();
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, phone_control_fd) < 0) {
		perror("socketpair: ");
		exit (-1);
	}
	PRINT("socketpair success\n");

	onhook();
	if(!set_onhook_monitor())
		PRINT("set_onhook_monitor success\n");

	phone_control.global_phone_is_using = 0;
	phone_control.global_incoming = 0;

	for(i=0; i<CLIENT_NUM; i++)
	{
		devlist[i].client_fd = -1;
		devlist[i].audio_client_fd = -1;
	}
	//启动定时器
	signal(SIGALRM, check_dev_tick);
	phone_control.value.it_value.tv_sec = 0;
	phone_control.value.it_value.tv_usec = 200*1000;
	phone_control.value.it_interval.tv_sec = 0;
	phone_control.value.it_interval.tv_usec = 200*1000;
	setitimer(ITIMER_REAL, &phone_control.value, NULL);

	//开始检测incoming
	start_read_incoming();

	phone_control.passage_fd = open(PASSAGE_NAME,O_RDWR);
	if(phone_control.passage_fd < 0)
	{
		PRINT("open %s error\n",PASSAGE_NAME);
		//exit(-1);
	}
	else
	{
		PRINT("open %s success\n",PASSAGE_NAME);
	}

	return 0;
}

//来电响铃
void Ringon(unsigned char *ppacket)
{
	int i,j=0;
	if(!strncmp(ppacket+7,ringon,3))
	{
		PRINT("RINGON!\n");
		for(i=0; i<CLIENT_NUM; i++){
#ifdef REGISTER
			if(devlist[i].client_fd != -1 && devlist[i].incoming == 1 && devlist[i].registered == 1)
#else
			if(devlist[i].client_fd != -1 && devlist[i].incoming == 1 )
#endif
			{
				netWrite(devlist[i].client_fd, "HEADR0010RINGON_000\r\n",22);
				j++;
			}
		}
		PRINT("%d dev recv RINGON\n",j);
		phone_control.ringon_flag=1;
	}
}

//来电号码
void Incomingcall(unsigned char *ppacket,int bytes,int flag)
{
	char sendbuf[SENDBUF]={0};
	int i;
	int index = 0;
	PRINT("INCOMMINGCALL!\n");
	int num_len=0;
	led_control(LED_INCOMING);
	if(flag == FSK)
	{
		num_len=(int)ppacket[12];
	}
	else if(flag == DTMF)
	{
		num_len=(int)ppacket[8];
	}
	phone_control.global_incoming = 1;

	//如果正在对讲，干掉！！
	for(i=0;i<CLIENT_NUM;i++)
	{
		if(devlist[i].client_fd == -1)
			continue;
		if(devlist[i].talkbacking || devlist[i].talkbacked)
		{
			PRINT("stop talkbacking!\n");
			stopaudio(&devlist[i],TALKBACK);
			do_cmd_talkbackonhook(&devlist[i],sendbuf);
			usleep(300*1000);
			break;
		}
	}
	for(i=0;i<CLIENT_NUM;i++)
	{
		if(devlist[i].client_fd == -1)
			continue;
		if(devlist[i].audio_client_fd > 0)
		{
			close(devlist[i].audio_client_fd);
			devlist[i].audio_client_fd = -1;
		}
	}
	phone_control.telephone_num_len = num_len;
	memset(phone_control.telephone_num,0,64);
	if(flag == FSK)
	{
		memcpy(phone_control.telephone_num,&ppacket[13],num_len);
	}
	else if(flag == DTMF)
	{
		memcpy(phone_control.telephone_num,&ppacket[9],num_len);
	}
	phone_control.telephone_num[num_len]='\0';
	PRINT("incoming num : %s\n",phone_control.telephone_num);
	if(phone_control.called_test == 1)
	{
		//生产测试被叫
		sendbuf[index++] = 0xA5;
		sendbuf[index++] = 0x5A;
		sendbuf[index++] = 0x83;
		sendbuf[index++] = 0X04;
		sendbuf[index++] = 0X00;
		sendbuf[index++] = 0X00;
		sendbuf[index++] = (char)num_len;
		memcpy(&sendbuf[index],phone_control.telephone_num,num_len);
		index += num_len;
		write(phone_control.passage_fd,sendbuf,index);
		phone_control.called_test = 0;
	}

	snprintf(sendbuf, 22+num_len, "HEADR0%03dINCOMIN%03d%s\r\n",10+num_len, num_len, phone_control.telephone_num);
	//snprintf(sendbuf, 22, "HEADR0%03dINCOMIN%03d\r\n",10,0);
	for(i=0; i<CLIENT_NUM; i++)
	{
#ifdef REGISTER
		if(devlist[i].client_fd != -1 && devlist[i].registered)
#else
		if(devlist[i].client_fd != -1 )
#endif		
		{
			netWrite(devlist[i].client_fd, sendbuf,strlen(sendbuf)+1);
			devlist[i].incoming = 1;
		}
	}
	phone_control.ringon_flag=1;
}
//挂机确认成功
void OnhookRes(unsigned char *ppacket,int bytes)
{
	//A5 5A DE 00 05 DB
	if(ppacket[9] == 0x00)
	{
		PRINT("ONHOOKRES\n");
		phone_control.global_onhook_success = 0;
	}
}

//摘机确认成功
void OffhookRes(unsigned char *ppacket,int bytes)
{
	//A5 5A C1 02 02 00 00 C1
	if(ppacket[9] == 0x02 && ppacket[10] == 0x00 && ppacket[11] == 0x00)
	{
		PRINT("OFFHOOKRES\n");
		phone_control.global_offhook_success = 0;
	}
}

//~ //并机检测
//~ void Parallel()
//~ {
	//~ PRINT("Parallel\n");
	//~ int i;
	//~ char sendbuf[BUF_LEN]="HEADR0010PARALLE000\r\n";
//~
	//~ for(i=0;i<CLIENT_NUM;i++)
	//~ {
		//~ if(devlist[i].client_fd == -1)
			//~ continue;
		//~ if(devlist[i].dev_is_using == 1 || devlist[i].isswtiching == 1)
		//~ {
			//~ netWrite(devlist[i].client_fd, sendbuf,strlen(sendbuf)+1);
		//~ }
	//~ }
//~ }
//~

int SpiUart1CmdDo(unsigned char *ppacket,int bytes)
{
	int rtn = 0;
	PRINT("SpiUart1CmdDo is called\r\n");

	//~ //?4?PIC??????m
	//~ if(ppacket[PACKET_TYPE_INDEX] == PACKET_TYPE_FROM_PIC)
	//~ {
		//~ switch(ppacket[PACKET_CMD_INDEX_TYPE_FROM_PIC])
		//~ {
			//~ case 0x50:
//~
				//~ break;
			//~ case 0x51:
//~
				//~ break;
			//~ case 0x52:
//~
				//~ break;
			//~ case 0x53:
				//~ Parallel();
				//~ break;
			//~ case 0x54:
//~
				//~ break;
//~
		//~ }
		//~ return rtn;
	//~ }

	//?4?254??????m
	switch(ppacket[PACKET_CMD_INDEX_TYPE_FROM_254])
	{
		case 0x03:
			Ringon(ppacket);
			break;
		case 0x04:
			Incomingcall(ppacket,bytes,DTMF);
			break;
		case 0x05:
			Incomingcall(ppacket,bytes,FSK);
			break;
		case 0xDE:
			OnhookRes(ppacket,bytes);
			break;
		case 0xC1:
			OffhookRes(ppacket,bytes);
			break;
	}

	return rtn;
}
int SpiUart2CmdDo(unsigned char *ppacket,int bytes)
{
	int rtn = 0;
	PRINT("SpiUart2CmdDo is called\r\n");
	return rtn;
}

int UpPacketDis(unsigned char *ppacket,int bytes)
{
	int rtn = 0;
	PRINT("Command Packet data::\r\n");
	ComFunPrintfBuffer(ppacket,bytes);
	if(ppacket[0]!=(unsigned char)0xA5 || ppacket[1]!=(unsigned char)0x5A)
	{
		PRINT("wrong data\n");
		return -1;
	}
	int uart,len_tmp=0;
	int xor=0;
	unsigned char *bufp = ppacket;
	len_tmp = (((unsigned short)ppacket[2]) << 8);
	len_tmp += ppacket[3];
	//PRINT("len_tmp=%d\n",len_tmp);
	xor=sumxor(ppacket+2,len_tmp+1);
	//PRINT("xor=0x%x\n",xor);
	if(ppacket[len_tmp+3]!=(unsigned char)xor)
	{
		PRINT("xor error\n");
		return -1;
	}

	switch(ppacket[PACKET_DIR_INDEX])
	{
		case 0x10:
			rtn = SpiUart1CmdDo(ppacket,bytes);
			break;
		case 0x20:
			rtn = SpiUart2CmdDo(ppacket,bytes);
			break;
		case 0x30:
			break;
		default:
			rtn = -1;
			break;
	}

	return rtn;
}


unsigned char *PacketSearchHead(void)
{
	int valid_len;
	unsigned char *pp;
	int index = 0;
	int rtn,get_head;
	valid_len = phone_control.handle_up_msg_wp_pos - phone_control.handle_up_msg_rd_pos;

	if(valid_len < MIN_PACKET_BYTES)//???????
		return 0;
	pp = &handle_up_msg_buffer[phone_control.handle_up_msg_rd_pos];
	get_head = 0;
	for(index = 0;index < (valid_len - 1);index++)
	{
		if((pp[index] == PACKET_HEAD_BYTE1) && (pp[index + 1] == PACKET_HEAD_BYTE2))
		{
			get_head = 1;
			if(index != 0)//?????,???ì???
			{
				phone_control.handle_up_msg_rd_pos += index;
			}
			return &pp[index];
		}
	}
	return 0;
}


//SPI?????
int UpPacketRcv(unsigned char *des_packet_buffer,int *packet_size)
{
	int valid_len;
	unsigned char *phead;
	unsigned char *pwp;
	int index = 0;
	int my_packet_bytes;

	phead = PacketSearchHead();
	if(phead == 0)
		return 0;
	pwp = (unsigned char *)(&handle_up_msg_buffer[phone_control.handle_up_msg_wp_pos]);
	//?????????β??Ч?????
	valid_len = pwp - phead;
	if(valid_len < MIN_PACKET_BYTES)//???????
	{
		return 0;
	}
	my_packet_bytes = phead[PACKET_HEAD_BYTES] * 256 + phead[PACKET_HEAD_BYTES +1];
	my_packet_bytes += PACKET_ADDITIONAL_BYTES;

	if(valid_len >= my_packet_bytes)
	{
		//?????τ??????
		memcpy((void *)(des_packet_buffer),(void *)(phead),my_packet_bytes);
		*packet_size = my_packet_bytes;
		phone_control.handle_up_msg_rd_pos += my_packet_bytes;
		//???????Р???х?
		return 1;
	}
	return 0;
}

void *handle_up_msg(void * argv)
{
	PRINT("%s thread start.......\n",__FUNCTION__);
	int read_ret;
	int t1,t2;
	int rtn;
	unsigned char packet_buffer[288];
	int packet_bytes;
	while(1)
	{
		//维护读写位置
		if(phone_control.handle_up_msg_wp_pos == phone_control.handle_up_msg_rd_pos)
		{
			phone_control.handle_up_msg_wp_pos = 0;
			phone_control.handle_up_msg_rd_pos = 0;
		}
		if(sizeof(handle_up_msg_buffer) - phone_control.handle_up_msg_wp_pos < 64)//即将到缓冲尾部，则前移

		{
			t1 = phone_control.handle_up_msg_wp_pos - phone_control.handle_up_msg_rd_pos;
			if(t1 > 0)//有未处理的数据，则将该数据前移，并重置写入和读取位置

			{
				for(t2 = 0;t2 < t1;t2++)
					handle_up_msg_buffer[t2] = handle_up_msg_buffer[phone_control.handle_up_msg_rd_pos + t2];
				phone_control.handle_up_msg_rd_pos = 0;
				phone_control.handle_up_msg_wp_pos = t1;
			}
		}
		do
		{
			//接收完整包
			rtn = UpPacketRcv(packet_buffer,&packet_bytes);
			if(rtn == 1)
			{
				//对完整包内的命令进行解析执行
				rtn = UpPacketDis(packet_buffer,packet_bytes);
			}
			else
				break;
		}while(1);
		usleep(20*1000);
	}
}

int generate_incoming_msg(unsigned char *buf,const unsigned char *num,int num_len)
{
	int i;
	buf[2] = (num_len+7)>>8;
	buf[3] = (num_len+7)&(0xff);

	buf[8] = num_len;
	for(i=0;i<num_len;i++)
	{
		buf[9+i] = num[i];
	}
	buf[num_len+9] = sumxor(&buf[7],num_len+2);
	buf[num_len+10] = sumxor(&buf[2],num_len+8);

	//ComFunPrintfBuffer(buf,num_len+11);
}

void led_control(char type)
{
	char led[3] = {0x5A,0XA5};
	led[2] = type;
	write(phone_control.passage_fd,led,3);
}


void *loop_check_ring(void * argv)
{
	PRINT("%s thread start.......\n",__FUNCTION__);
	unsigned char dtmf_buf[BUFFER_SIZE_1K*10]={0};
	unsigned char incoming_num[SENDBUF]={0};
	int read_ret = 0;
	int i,loop_count = 0;
	unsigned char packet_buffer[30]={0xA5,0x5A,0x00,0x00,0x10,0xA5,0x5A,0x04};
	unsigned char ringon_buffer[12]={0xA5,0x5A,0x00,0x07,0x10,0xA5,0x5A,0x03,0x00,0x03,0xE8};
	Fsk_Message_T fskmsg;
	short iloop = 0;;
	unsigned char fdt = 0;
	int line = 0;
	int line_in = 0;
	int line_out = 0;
	while(1)
	{
		getFXOstatus(&(phone_control.vloop),&iloop,&fdt);
		if((phone_control.vloop < -3 || phone_control.vloop > 3))
		{
			line_out = 0;
			if(line == -1 || line == 0)
			{
				//PRINT("phone_control.vloop = %d\n",phone_control.vloop);
				line_in ++;
				if(line_in == 3)
				{
					PRINT("line in!\n");
					line = 1;
					led_control(LED_LINE_IN);
				}
			}
		}
		if((phone_control.vloop > -3 && phone_control.vloop < 3))
		{
			line_in = 0;
			if(line == 1 || line == 0)
			{
				//PRINT("phone_control.vloop = %d\n",phone_control.vloop);
				line_out ++;
				if(line_out == 3)
				{
					PRINT("line out!\n");
					line = -1;
					led_control(LED_LINE_OUT);
				}
			}
		}
		checkRingStatus(&phone_control.Status);
		if(phone_control.Status.offhook == 0)
		{
			if(phone_control.Status.ringDetected == 1)
			{
				if(phone_control.ring_count == 0)
				{
					Fsk_InitParse();
				}
				if(phone_control.Status.ringDetectedNeg == 1)
				{
					phone_control.ring_neg_count ++;
					PRINT("phone_control.ring_neg_count = %d\n",phone_control.ring_neg_count);
				}
				
				//if(phone_control.Status.ringDetectedPos == 1)
				//{
					//phone_control.ring_pos_count ++;
					//PRINT("phone_control.ring_pos_count = %d\n",phone_control.ring_pos_count);
				//}
				phone_control.ring_count ++;
				if(phone_control.ring_count > 0 && phone_control.ring_count <= 120)
				{
					if(((phone_control.ring_count % 10)==0) && phone_control.ring_neg_count > 0)
					{
						PRINT("phone_control.ring_count = %d\n",phone_control.ring_count);
					}
				}
				memset(&fskmsg,0,sizeof(fskmsg));
				if(Fsk_GetFskMsg(&fskmsg)==TRUE)
				{
					PRINT("Get Fsk Num...\n");
					phone_control.get_fsk = 1;
				}

			}
			else if(phone_control.Status.ringDetected == 0)
			{
				if(phone_control.ring_count > 0 && phone_control.ring_neg_count > 0)
				{
					PRINT("ringDectected = %d\n",phone_control.Status.ringDetected);
					//结束检测ring
					phone_control.check_ringon_count = RING_LIMIT;
				}
				phone_control.ring_count = 0;
				phone_control.ring_neg_count = 0;
			}
			if(((phone_audio.get_code == 1) && (phone_control.ring_neg_count > 1)) || ((phone_control.ring_count == 120) && (phone_control.ring_neg_count > 5)) || phone_control.get_fsk == 1)
			{
				PRINT("phone_control.ring_count = %d\n",phone_control.ring_count);
				//产生Incoming
				PRINT("Incoming.....\n");
				phone_control.ring_count += 120;
				memset(incoming_num,0,SENDBUF);
				if(phone_audio.get_code == 1)
				{
					DtmfGetCode(incoming_num);
				}
				if(phone_control.get_fsk == 1)
				{
					PRINT("num_len = %d\n",strlen(fskmsg.num));
					if(fskmsg.isgood == TRUE)
					{
						memcpy(incoming_num,fskmsg.num,strlen(fskmsg.num));
					}
					//Fsk_FinishFskData();
				}
				phone_audio.get_code = 0;
				phone_control.get_fsk = 0;
				//PRINT("num: %s\n",incoming_num);
				generate_incoming_msg(packet_buffer,incoming_num,strlen(incoming_num));
				//stop_read_incoming();
				memcpy((unsigned char*)(&handle_up_msg_buffer[phone_control.handle_up_msg_wp_pos]),packet_buffer,strlen(incoming_num)+12);
				phone_control.handle_up_msg_wp_pos += (strlen(incoming_num)+12);
			}
			if( (phone_control.ring_count > 120) && ((phone_control.ring_count % 20) == 0) && (phone_control.ring_neg_count > 0))
			{
				PRINT("Ringon.....\n");
				//产生Ringon
				memcpy((unsigned char*)(&handle_up_msg_buffer[phone_control.handle_up_msg_wp_pos]),ringon_buffer,11);
				phone_control.handle_up_msg_wp_pos += 11;
			}
		}
		else
		{
			phone_control.ring_count = 0;
			phone_control.ring_neg_count = 0;
		}
		usleep(50*1000);
	}
}

int getoutcmd(char *buf,int ret,dev_status_t* dev)
{
	char *pbuf=buf;
	//~ PRINT("phone_control.cli_req_buf_wp = %d\n",phone_control.cli_req_buf_wp);
	//~ PRINT("phone_control.cli_req_buf_rp = %d\n",phone_control.cli_req_buf_rp);

	memset(&cli_req_buf[phone_control.cli_req_buf_wp], 0, sizeof(cli_request_t));
	memcpy(cli_req_buf[phone_control.cli_req_buf_wp].head,pbuf,5);
	if(strcmp(cli_req_buf[phone_control.cli_req_buf_wp].head,"HEADS"))
	{
		PRINT("msg error\n");
		memset(&cli_req_buf[phone_control.cli_req_buf_wp],0,sizeof(cli_request_t));
		return -1;;
	}

	//devlist[i].id= pbuf[5];

	char length[4] = {0};
	char arg_len[4] = {0};

	memcpy(length, pbuf+6, 3);
	memcpy(arg_len, pbuf+16, 3);

	cli_req_buf[phone_control.cli_req_buf_wp].length = atoi(length);
	phone_control.last_cli_length = cli_req_buf[phone_control.cli_req_buf_wp].length;
	cli_req_buf[phone_control.cli_req_buf_wp].arglen = atoi(arg_len);

	char cmd[8] = {0};
	memcpy(cmd, pbuf+9, 7);

	cli_req_buf[phone_control.cli_req_buf_wp].cmd = getCmdtypeFromString(cmd);

	memcpy(cli_req_buf[phone_control.cli_req_buf_wp].arg, pbuf+19, cli_req_buf[phone_control.cli_req_buf_wp].arglen);
	cli_req_buf[phone_control.cli_req_buf_wp].dev = dev;
	phone_control.cli_req_buf_wp++;
	if(phone_control.cli_req_buf_wp >= CLI_REQ_MAX_NUM)
		phone_control.cli_req_buf_wp = 0;
	return 0;
}

void* tcp_loop_recv(void* argv)
{
	PRINT("%s thread start.......\n",__FUNCTION__);
	int ret=0;
	int i;
	char msg[BUFFER_SIZE_2K]={0};
	char *pbuf=msg;
	char sendbuf[SENDBUF]={0};
	char length_more[3]={0};

	fd_set socket_fdset;
	struct timeval tv;
	memset(&tv, 0, sizeof(struct timeval));


	while(1)
	{
		int maxfd = -1;
		int tmp_ret = 0;
		FD_ZERO(&socket_fdset);
		for(i=0; i<CLIENT_NUM ; i++)
		{
			if(devlist[i].client_fd == -1 || devlist[i].dying == 1)
				continue;
			FD_SET(devlist[i].client_fd, &socket_fdset);
			maxfd = MAX(maxfd, devlist[i].client_fd);
		}

		tv.tv_sec =  1;
		tv.tv_usec = 0;

		switch(select(maxfd+ 1, &socket_fdset, NULL, NULL, &tv))
		{
			case -1:
			case 0:
				break;
			default:
				for(i=0; i<CLIENT_NUM ; i++)
				{
					if(devlist[i].client_fd == -1 || devlist[i].dying == 1)
						continue;
					if ( FD_ISSET(devlist[i].client_fd, &socket_fdset) )
					{
						memset(msg,0,BUFFER_SIZE_2K);
						ret=recv(devlist[i].client_fd,msg+tmp_ret,BUFFER_SIZE_2K,0);
						if(ret<0)
						{
							PRINT("read failed\n");
							memset(sendbuf,0,SENDBUF);
							sprintf(sendbuf,"%s%d","INSIDE",i);
							netWrite(phone_control_fd[0],sendbuf, strlen(sendbuf));
							break;
						}
						if(ret==0)
						{
							PRINT("socket return 0\n");
							memset(sendbuf,0,SENDBUF);
							sprintf(sendbuf,"%s%d","INSIDE",i);
							netWrite(phone_control_fd[0],sendbuf, strlen(sendbuf));
							break;
						}
						//PRINT("%s\n",msg);
						pbuf=msg;
again:
						if(getoutcmd(pbuf,ret,&devlist[i])==-1)
						{
							break;
						}

						//parse_msg(&devlist[i]);
						int tmp_len=phone_control.last_cli_length;
						if(!strncmp(pbuf+9+tmp_len,"HEADS",5))
						{
							//PRINT("recv more msg\n");
							memcpy(length_more, pbuf+tmp_len+15, 3);
							int len_more = atoi(length_more);
							pbuf = pbuf+9+tmp_len;
							ret = ret-tmp_len-9;
							//PRINT("ret=%d\n",ret);
							if(ret<19)
							{
								//PRINT("no msg\n");
								tmp_ret=ret;
								memcpy(msg,pbuf,ret);
							}
							goto again;
						}
					}
				}
		}
		usleep(50*1000);
	}
}

void *phone_control_loop_accept(void* arg)
{
	PRINT("%s thread start.......\n",__FUNCTION__);
	int clientfd;
	int i,j;
	fd_set fdset;
	struct timeval tv;
	struct timeval timeout;
	memset(&tv, 0, sizeof(struct timeval));
	memset(&timeout, 0, sizeof(struct timeval));
	struct sockaddr_in client;
	socklen_t len = sizeof(client);
	char sendbuf[SENDBUF]={0};
	char *new_ip = NULL;
	struct in_addr ia;
	int control_reconnect=0;
	int online_dev_num = 0;

	while (1)
	{

		FD_ZERO(&fdset);
		FD_SET(phone_control.phone_control_sockfd, &fdset);

		tv.tv_sec =   1;
		tv.tv_usec = 0;
		switch(select(phone_control.phone_control_sockfd + 1, &fdset, NULL, NULL,&tv))
		{
			case -1:
			case 0:
				break;
			default:
				if (FD_ISSET(phone_control.phone_control_sockfd, &fdset) > 0)
				{

					clientfd = accept(phone_control.phone_control_sockfd, (struct sockaddr*)&client, &len);
					if (clientfd < 0)
					{
						PRINT("accept failed!\n");
						break;
					}
					PRINT("control accept success\n");
					PRINT("clientfd=%d\n",clientfd);
					ia = client.sin_addr;
					control_reconnect=0;
					new_ip = inet_ntoa(ia);
					PRINT("client_ip=%s\n",new_ip);
					online_dev_num = 0;
					for(i=0; i<CLIENT_NUM; i++)
					{
						if(devlist[i].client_fd == -1)
							continue;
						if(devlist[i].client_fd >= 0 && strcmp(new_ip,devlist[i].client_ip))
							online_dev_num ++;
					}
					if(online_dev_num == 4)
					{
						PRINT("client limit\n");
						close(clientfd);
						break;
					}
					for(i=0; i<CLIENT_NUM; i++)
					{
						if(devlist[i].client_fd == -1)
							continue;
						if(!strcmp(new_ip,devlist[i].client_ip))
						{
							if(devlist[i].dev_is_using == 1 && devlist[i].dying == 1)
							{
								PRINT("control client reconnect\n");
								control_reconnect=1;
							}
							else
							{
								PRINT("reconnect\n");
								memset(sendbuf,0,SENDBUF);
								sprintf(sendbuf,"%s%d","INSIDE",i);
								netWrite(phone_control_fd[0],sendbuf, strlen(sendbuf));
								usleep(100*1000);
							}
						}
					}
					if(clientfd >= 0)
					{
						for(i=0; i<CLIENT_NUM; i++)
						{
							if(devlist[i].client_fd == -1)
							{
								devlist[i].client_fd = clientfd;
								devlist[i].control_reconnect=control_reconnect;
								devlist[i].id = i;
								memset(devlist[i].client_ip, 0, sizeof(devlist[i].client_ip));
								memcpy(devlist[i].client_ip, new_ip, strlen(new_ip)+1);
								memset(devlist[i].dev_name,' ',16);
								sprintf(devlist[i].dev_name,"%s%d","子机",i+1);
								for(j=0;j<16;j++)
								{
									if(devlist[i].dev_name[j] == '\0')
									{
										devlist[i].dev_name[j]=' ';
									}
								}							
								generate_up_msg();
								break;
							}
						}
					}
					for(i=0; i<CLIENT_NUM; i++)
						printf("%d:cli_fd=%d  , cli_ip=%s\n", i ,devlist[i].client_fd,devlist[i].client_ip);
				}
		}
		//	usleep(100*1000);
	}
}

int parse_msg_inside(char *msg)
{
	PRINT("%s\n",__FUNCTION__);
	char num = msg[6];
	PRINT("num=%c\n",num);
	if(num>='0' && num <='3')
	{
		PRINT("dev %d will be destroyed\n",(int)num-48);
		destroy_client(&devlist[(int)num-48]);
	}
	return 0;
}

//检测来电持续
void check_ringon_func()
{
	int i=0;
	if(phone_control.global_incoming == 1)
	{
		PRINT("check ringon running\n");
		if(phone_control.ringon_flag == 0)
		{
			phone_control.check_ringon_count++;
			//PRINT("phone_control.check_ringon_count = %d\n",phone_control.check_ringon_count);
			if(phone_control.check_ringon_count >= RING_LIMIT)
			{
				phone_control.global_incoming = 0;
				PRINT("no RINGON !\n");
				for(i=0;i<CLIENT_NUM;i++)
				{
					if(devlist[i].incoming == 1)
					{
						devlist[i].incoming = 0;
					}
				}
				led_control(LED_LINE_IN);
				phone_control.check_ringon_count = 0;
			}
		}
		else
		{
			phone_control.check_ringon_count = 0;
		}
		phone_control.ringon_flag=0;
	}
	else
	{
		phone_control.check_ringon_count = 0;
	}
}

//转子机发送RINGON
void send_sw_ringon()
{
	phone_control.send_sw_ringon_count++;
	if(phone_control.send_sw_ringon_count == 5)
	{
		int i,j;
		for(i=0;i<CLIENT_NUM;i++)
		{
			if(devlist[i].client_fd == -1)
				continue;
			if(devlist[i].isswtiching == 1)
			{
				PRINT("send ringon running\n");
				phone_control.sw_dev = i;
				netWrite(devlist[i].client_fd, "HEADR0010RINGON_000\r\n",22);
				phone_control.send_sw_ringon_limit++;
				if(phone_control.send_sw_ringon_limit == 40 && devlist[i].dev_is_using == 0)
				{
					if(devlist[i].isswtiching == 1)
					{
						onhook();
						if(!set_onhook_monitor())
							PRINT("set_onhook_monitor success\n");
						//开始检测incoming
						start_read_incoming();

					}
					//响铃超时或者接听
					devlist[i].isswtiching = 0;
					phone_control.send_sw_ringon_limit = 0;
				}
				if(devlist[i].dev_is_using == 1)
				{
					devlist[i].isswtiching = 0;
					phone_control.send_sw_ringon_limit = 0;
				}
				break;
			}
			if(phone_control.send_sw_ringon_limit > 0 && phone_control.sw_dev != -1 && devlist[phone_control.sw_dev].isswtiching == 0)
			{
				phone_control.send_sw_ringon_limit = 0;
				phone_control.sw_dev = -1;
				onhook();
				if(!set_onhook_monitor())
					PRINT("set_onhook_monitor success\n");
				//开始检测incoming
				start_read_incoming();
			}
		}
		phone_control.send_sw_ringon_count = 0;
	}
}

void send_tb_ringon()
{
	phone_control.send_tb_ringon_count++;
	if(phone_control.send_tb_ringon_count == 5)
	{
		int i,j,k,flag;
		for(i=0;i<CLIENT_NUM;i++)
		{
			if(devlist[i].client_fd == -1)
				continue;
			flag = 0;
			if(devlist[i].talkbacked == 1 && devlist[i].talkback_answer == 0)
			{
				for(k=0;k<CLIENT_NUM;k++)
				{
					if(devlist[k].client_fd == -1)
						continue;
					if(devlist[k].talkbacking == 1)
						flag = 1;
				}
				if(flag == 0)
				{
					phone_control.send_tb_ringon_limit = 0;
					phone_control.tb_dev = -1;
					break;
				}
				PRINT("send talkback ringon running\n");
				phone_control.tb_dev = i;
				netWrite(devlist[i].client_fd, "HEADR0010TBRING_000\r\n",22);
				phone_control.send_tb_ringon_limit++;
				if(phone_control.send_tb_ringon_limit == 60 && devlist[i].talkback_answer == 0)
				{
					stopaudio(&devlist[i],TALKBACK);
					//响铃超时
					do_cmd_talkbackonhook(&devlist[i],tb_sendbuf);
					//devlist[i].talkbacked = 0;
					phone_control.send_tb_ringon_limit = 0;
				}
				if(devlist[i].talkback_answer == 1)
				{
					phone_control.send_tb_ringon_limit = 0;
				}
				break;
			}
			if(phone_control.send_tb_ringon_limit > 0 && phone_control.tb_dev != -1 && devlist[phone_control.tb_dev].talkbacked == 0)
			{
				stopaudio(&devlist[phone_control.tb_dev],TALKBACK);
				do_cmd_talkbackonhook(&devlist[phone_control.tb_dev],tb_sendbuf);
				phone_control.send_tb_ringon_limit = 0;
				phone_control.tb_dev = -1;
			}
		}
		phone_control.send_tb_ringon_count = 0;
	}
}

void get_code_timeout_func()
{
	if(phone_audio.get_code_timeout)
	{
		phone_audio.get_code_timeout_times++;
		if(phone_audio.get_code_timeout_times == 11)
		{
			if(phone_control.ring_neg_count == 0)
			{
				PRINT("no ring,pass...\n");
				DtmfGetCode(nothing_to_do_buf); //数据扔了不要
				phone_audio.get_code = 0;				
			}
			phone_audio.get_code_timeout_times = 0;
			phone_audio.get_code_timeout = 0;
		}
	}
	else
	{
		phone_audio.get_code_timeout_times = 0;
	}
}

//检测音频重连
void check_audio_reconnect()
{
	if(phone_audio.audio_reconnect_flag)
	{
		phone_audio.audio_reconnect_loops++;
		if(phone_audio.audio_reconnect_loops ==11)//超过2秒没有重连，则需要挂机
		{
			if(phone_control.who_is_online.attach == 1)
			{
				//超时未重连处理
				PRINT("audio reconnect timeout...\n");
				int i;
				for(i=0;i<CLIENT_NUM;i++)
				{
					if(devlist[i].client_fd == -1)
						continue;
					if(devlist[i].client_fd == phone_control.who_is_online.client_fd)
					{
						do_cmd_onhook(&devlist[i]);
						break;
					}
				}
			}
			//重置重连标识
			phone_audio.audio_reconnect_loops = 0;
			phone_audio.audio_reconnect_flag = 0;
		}
	}
	else
	{
		phone_audio.audio_reconnect_loops = 0;
	}
}

//定时器处理函数
void check_dev_tick(int signo)
{
	switch (signo)
	{
		case SIGALRM:
			check_ringon_func();
			send_sw_ringon();
			send_tb_ringon();
			check_audio_reconnect();
			get_code_timeout_func();
			signal(SIGALRM, check_dev_tick);
			break;
	}

	return;
}

void* handle_down_msg(void* argv)
{
	PRINT("%s thread start.......\n",__FUNCTION__);
	while(1)
	{
		if(phone_control.cli_req_buf_rp == phone_control.cli_req_buf_wp)
		{
			usleep(20*1000);
			continue;
		}
		parse_msg(&cli_req_buf[phone_control.cli_req_buf_rp]);
		phone_control.cli_req_buf_rp++;
		if(phone_control.cli_req_buf_rp>=CLI_REQ_MAX_NUM)
			phone_control.cli_req_buf_rp = 0;
		//~ if(phone_control.cli_req_buf_wp>=CLI_REQ_MAX_NUM)
			//~ phone_control.cli_req_buf_wp = 0;
		usleep(20*1000);
	}
}

int generate_up_msg()
{
	char sendbuf[BUF_LEN]={0};
	char tmpbuf1[BUF_LEN]={0};
	char tmpbuf2[BUF_LEN]={0};
	int count=0;
	int ip_len = 0;
	int i,j = 0;
	//组合消息
	memset(sendbuf,0,BUF_LEN);
	for(i=0;i<CLIENT_NUM;i++)
	{
#ifdef REGISTER
		if(devlist[i].client_fd == -1 || devlist[i].dying == 1 || devlist[i].registered == 0 /*|| devlist[i].tick_time == 0*/)
#else
		if(devlist[i].client_fd == -1 || devlist[i].dying == 1  /*|| devlist[i].tick_time == 0*/)
#endif
				continue;
		ip_len = strlen(devlist[i].client_ip);
		memset(tmpbuf1,0,BUF_LEN);
		memcpy(tmpbuf1,"#DEV#IP",7);
		tmpbuf1[7]= (char)(i+48);
		memcpy(tmpbuf1+8,devlist[i].client_ip,ip_len);
		memcpy(tmpbuf1+8+ip_len,"#NAME",5);
		memcpy(tmpbuf1+ip_len+13,devlist[i].dev_name,16);
		memcpy(tmpbuf2+count+1,tmpbuf1,29+ip_len);
		count+=29+ip_len; //7+5+16+1+ip_len
		j++;
	}
	tmpbuf2[0] = (char)(j+48);
	j = 0;
	memcpy(tmpbuf2+count+1,"\r\n\0",3);
	if(count!=0)
	{
		sprintf(sendbuf,"%s%03d%s","HEADR0",count+8,"OPTI_BS");
		memcpy(sendbuf+16,tmpbuf2,count+3);
		//PRINT("%s\n",sendbuf);
	}
	memset(tmpbuf2,0,BUF_LEN);
	for(i=0;i<CLIENT_NUM;i++)
	{
		if(devlist[i].client_fd == -1)
			continue;
		PRINT("%s:%d\n",devlist[i].client_ip,netWrite(devlist[i].client_fd, sendbuf, count+19));
	}
	return 0;
}


//心跳
void* phone_check_tick(void* argv)
{
	PRINT("%s thread start.......\n",__FUNCTION__);
	int i,j=0;
	int print_count = 0;
	int tick_count = 0;
	int online_count = 0;
	char sendbuf[BUF_LEN]={0};
	char insidebuf[SENDBUF]={0};
	//HEADR0172OPTI_BS4#DEV#IP010.10.10.105#NAMEA31s-Tablet
	while(1)
	{
		if(tick_count == 20)
		{
			tick_count = 0;
            generate_up_msg();
		}
		tick_count ++;
		for(i=0;i<CLIENT_NUM;i++)
		{
			if(devlist[i].client_fd == -1)
				continue;
			if(devlist[i].old_tick_time == devlist[i].tick_time)
			{
				if(devlist[i].destroy_count >= 20)
				{
					PRINT("devlist[%d].destroy_count = %d\n",i,devlist[i].destroy_count);
				}
				devlist[i].destroy_count ++;
			}
			else
			{
				devlist[i].destroy_count = 0;
				devlist[i].old_tick_time = devlist[i].tick_time;
			}
			if(devlist[i].destroy_count == 25)
			{
				PRINT("no tick!!!\n");
				//destroy_client(&devlist[i]);
				memset(insidebuf,0,SENDBUF);
				sprintf(insidebuf,"%s%d","INSIDE",i);
				netWrite(phone_control_fd[0],insidebuf, strlen(insidebuf));
			}
		}
		print_count ++;
		if(print_count%10 == 0)
		{
			printf("[%d]   ",print_count);
			for(i=0;i<CLIENT_NUM;i++)
			{
				if(devlist[i].client_fd == -1)
					continue;
				online_count ++;
				printf("%s",devlist[i].dev_name);
			}
			if(online_count > 0)
				printf("online\n");
			else
				printf("no devs online\n");
			online_count = 0;
		}
		sleep(1);

	}
}


//连接管理
void* phone_link_manage(void* argv)
{
	PRINT("%s thread start.......\n",__FUNCTION__);
	fd_set fdset;
	int ret;
	struct timeval tv;
	memset(&tv, 0, sizeof(struct timeval));
	char msg[BUF_LEN]={0};
	int i=0;
	int len=0;

	while(1)
	{
		FD_ZERO(&fdset);
		FD_SET(phone_control_fd[1], &fdset);
		tv.tv_sec  = 0;
		tv.tv_usec = 20*1000;
		switch( select(phone_control_fd[1]+1, &fdset, NULL, NULL, &tv) )
		{
			case -1:
				perror("select phone_control_fd error\n");
				break;
			case 0:	//timeout
				break;
			default:
				if ( FD_ISSET(phone_control_fd[1], &fdset) )
				{
					memset(msg,0,BUF_LEN);
					ret = recv(phone_control_fd[1],msg,BUF_LEN,0);
					if(ret<=0)
					{
						PRINT("recv phone_control_fd error\n");
						break;
					}
					PRINT("inside msg=%s\n",msg);
					len=strlen(msg);
					//PRINT("inside msg len=%d\n",len);
					for(i=0;i<len/7;i++)
					{
						if(strncmp(msg+7*i,"INSIDE",6))
						{
							PRINT("inside msg error\n");
							break;
						}
						parse_msg_inside(msg+7*i);
					}
				}
		}
		usleep(20*1000);
	}
}

int dialup_test()
{
	char dial_buf[BUFFER_SIZE_1K*60]={0};
	int pcm_ret = 0;
	int w_ret = 0;
	pcm_ret = GenerateCodePcmData("123456789*0#",12,dial_buf,Big_Endian);
	//pcm_ret = GenerateCodePcmData("8161",4,dial_buf,Big_Endian);
	char *dial_p = dial_buf;
	PRINT("pcm_ret = %d\n",pcm_ret);
	while(1)
	{
		w_ret = write(phone_audio.phone_audio_pcmfd,dial_p,BUFFER_SIZE_1K);
		if(w_ret > 0)
		{
			PRINT("w_ret = %d\n",w_ret);
			pcm_ret -= w_ret;
			dial_p += w_ret;
		}
		else
		{
			return 1;
		}
		if(pcm_ret <= 0)
			break;
		usleep(15*1000);
	}
	return 0;
}

//test 123456789*0#
int call_test()
{
	int ret = 0;
	stop_read_incoming();
	offhook();
	sleep(1);
	if(dialup_test() == 1)
	{
		ret = 1;
	}
	sleep(1);
	onhook();
	set_onhook_monitor();
	start_read_incoming();
	return ret;
}

void passage_pthread_func(void *argv)
{
	int ret = 0;
	char recvbuf[BUF_LEN_256]={0};
	char failbuf[]={0xA5,0X5A,0X83,0X03,0X01,0X01};
	char successbuf[]={0xA5,0X5A,0X83,0X03,0X00,0X00};
	while(1)
	{
		if(phone_control.passage_fd > 0)
		{
			memset(recvbuf,0,BUF_LEN_256);
			//a5 5a 03 03
			//a5 5a 03 04 
			ret = read(phone_control.passage_fd,recvbuf,4);
			if(ret > 0)
			{
				ComFunPrintfBuffer(recvbuf,ret);
				if(ret < 4)
					continue;
				if(recvbuf[0] != (char)0xA5 || recvbuf[1] != (char)0x5A)
					continue;
				if(recvbuf[2] == (char)0x03 && recvbuf[3] == (char)0x03)
				{
					PRINT("CALL!!\n");
					if(call_test()==0)
						write(phone_control.passage_fd,successbuf,6);
					else
						write(phone_control.passage_fd,failbuf,6);
				}
				if(recvbuf[2] == (char)0x03 && recvbuf[3] == (char)0x04)
					phone_control.called_test = 1;
			}
		}
		usleep(100*1000);
	}
}
