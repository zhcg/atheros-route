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
	.get_fsk_mfy = 0, //获取fsk号码
	.get_fsk_zzl = 0, //获取fsk号码
	.ring_count = 0, //来电标志后计数
	.ring_neg_count = 0,
	.ring_pos_count = 0,
#ifdef B6
	.passage_fd = -1,
#else
	.led_control_fd = -1,
#endif
	.vloop = 0,
	.offhook_kill_talkback = 0,
	.phone_busy = 0,
};

dev_status_t devlist[CLIENT_NUM];//设备列表
unsigned char handle_up_msg_buffer[BUFFER_SIZE_1K];//上行报文接收buf
cli_request_t cli_req_buf[CLI_REQ_MAX_NUM];
int phone_control_fd[2];
char tb_sendbuf[SENDBUF]={0};
unsigned char nothing_to_do_buf[BUFFER_SIZE_1K];

struct timeval print_tv;	
char print_time_buf_tmp[64] = {0};
char print_time_buf[256] = {0};	

static const char ringon[3]={0x03,0x00,0x03};

char *system_time()
{
	memset(print_time_buf, 0, sizeof(print_time_buf));
	memset(&print_tv, 0, sizeof(print_tv));
	
	gettimeofday(&print_tv, NULL);
	strftime(print_time_buf_tmp, sizeof(print_time_buf_tmp), "%T:", localtime(&print_tv.tv_sec));    
	sprintf(print_time_buf, "%s%03d", print_time_buf_tmp, print_tv.tv_usec/1000);
	return print_time_buf;
}

void print_devlist()
{
	int i,offline = 0;
	PRINT("****************************************************************\n");
	PRINT("*                            devlist                           *\n");
	for(i=0;i<CLIENT_NUM;i++)
	{
		if(devlist[i].client_fd == -1)
		{
			offline ++;
			continue;
		}
		PRINT("* %02d, cfd:%02d , afd:%02d , ip:%s ,name:%s *\n",i,devlist[i].client_fd,devlist[i].audio_client_fd,devlist[i].client_ip,devlist[i].dev_name);
	}
	PRINT("*                   max:%02d,online:%02d,last:%02d                   *\n",CLIENT_NUM-1,CLIENT_NUM-offline,offline-1);
	PRINT("****************************************************************\n");
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
    if (sqlite3_open("/configure_backup/terminal_dev_register/db/terminal_base_db", &db) != 0)
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
    strcat(sql, "\" ");
    strcat(sql,"collate NOCASE;");
    //PRINT("sql:%s\n",sql);
    if(sqlite3_get_table(db, sql, &result_buf, &row_count, &column_count, &err_msg) != 0)
    {
        PRINT("%s\n", err_msg);
        sqlite3_free_table(result_buf);
		sqlite3_close(db);
        return -1;
    }
    
    index = column_count;
    for (i = 0; i < column_count; i++)
    {
        if (strcmp(result_buf[index], "") == 0)
        {
            PRINT("no data\n");
			sqlite3_free_table(result_buf);
			sqlite3_close(db);
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
int destroy_client(dev_status_t *dev,int broadcast_flag)
{
	PRINT("%s\n",__FUNCTION__);
	if(dev->client_fd == -1)
		return 0;
	PRINT("ip = %s\n",dev->client_ip);
	char sendbuf[SENDBUF]={0};
	int i,j,k,count=0;
	//dev->dying = 1;
	close(dev->client_fd);
	if(dev->dev_is_using == 1 && dev->client_fd == phone_control.who_is_online.client_fd)
	{
		PRINT("dev is using,but connection error\n");
		//PRINT("phone_control.who_is_online.client_fd = %d\n",phone_control.who_is_online.client_fd);
		//PRINT("dev->client_fd = %d\n",dev->client_fd);
		//PRINT("dev = %p\n",dev);
		
		for(i=0;i<200;i++)
		{
			if(phone_control.global_phone_is_using != 1)
				break;
			usleep(10*1000);
			for(j=0;j<CLIENT_NUM;j++)
			{
				if(dev == &devlist[j] || strlen(devlist[j].client_ip) == 0)
				{
					continue;
				}
				if(!strcmp(phone_control.who_is_online.client_ip,devlist[j].client_ip) /*&& devlist[j].control_reconnect==1*/)
				{
					PRINT("found reconnect client\n");
					dev->client_fd=devlist[j].client_fd;
					dev->dev_is_using = 1;
					dev->tick_time = 1;
					dev->destroy_count = 0;
					phone_control.who_is_online.client_fd=devlist[j].client_fd;
					phone_control.who_is_online.dev_is_using = 1;

					memset(&devlist[j],0,sizeof(dev_status_t));
					devlist[j].client_fd = -1;
					devlist[j].audio_client_fd = -1;
					devlist[j].id = -1;
					devlist[j].registered = 0;
					memset(devlist[j].client_ip,0,sizeof(devlist[j].client_ip));
					print_devlist();
					dev->dying = 0;
#ifdef B6
					netWrite(dev->client_fd,"HEADR0012REG_SUC002B6\r\n",23);
#elif defined(B6L)	
					netWrite(dev->client_fd,"HEADR0013REG_SUC003B6L\r\n",24);
#elif defined(S1) || defined(S1_F3A)
					netWrite(dev->client_fd,"HEADR0012REG_SUC002S1\r\n",23);
#else
					netWrite(dev->client_fd,"HEADR0010REG_SUC000\r\n",21);
#endif
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
			if(phone_control.vloop > 3 || phone_control.vloop < -3)
			{
				led_control(LED_LINE_IN);
#ifndef B6
				led_control(LED2_ON);
#endif
			}
			else
			{
				led_control(LED_LINE_OUT);
#ifndef B6
				led_control(LED2_OFF);
#endif
			}
		}
		stopaudio(dev,PSTN,0);
	}
	if(dev->talkbacking == 1 || dev->talkbacked == 1)
	{
		do_cmd_talkbackonhook(dev,sendbuf);
		phone_control.global_talkback = 0;
	}
	if(dev->isswtiching)
	{
		do_cmd_onhook(dev);
		//dev->isswtiching = 0;
		//onhook();
		//if(!set_onhook_monitor())
			//PRINT("set_onhook_monitor success\n");
		//开始检测incoming
		//start_read_incoming();
	}
	close(dev->audio_client_fd);
	memset(dev, 0, sizeof(dev_status_t));
	dev->client_fd = -1;
	dev->audio_client_fd = -1;
	dev->id = -1;
	dev->registered = 0;
	dev->dying = 0;
	memset(dev->client_ip,0,sizeof(dev->client_ip));
	if(broadcast_flag == 1)
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

int do_cmd_offhook(dev_status_t *dev,char *buf)
{
	int i,count=0;
	int offhook_limit = 0;//挂机尝试次数限制
	char sendbuf[SENDBUF]={0};
	
	if(phone_control.vloop < 3 && phone_control.vloop > -3)
	{
		memset(sendbuf,0,SENDBUF);
		snprintf(sendbuf, 23,"HEADR0011INUSING0015\r\n");
		goto OFFHOOK_ERR;
	}
	if(dev->audio_client_fd < 0)
	{
		memset(sendbuf,0,SENDBUF);
		snprintf(sendbuf, 23,"HEADR0011INUSING0016\r\n");
		goto OFFHOOK_ERR;
	}
	PRINT("buf[0] = %c \n",buf[0]);
	if(phone_control.global_incoming == 1)
	{
		if(dev->incoming == 1 && buf[0] != '0')
		{
			phone_audio.start_recv = 0;
			goto OFFHOOK;
		}
		else
		{
			memset(sendbuf,0,SENDBUF);
			snprintf(sendbuf, 23,"HEADR0011INUSING0017\r\n");
			goto OFFHOOK_ERR;
		}
	}
	if(dev->isswtiching == 1)
	{
		if(buf[0] != '0')
		{
			phone_audio.start_recv = 0;
			goto OFFHOOK;
		}
		else
		{
			memset(sendbuf,0,SENDBUF);
			snprintf(sendbuf, 23,"HEADR0011INUSING0017\r\n");
			goto OFFHOOK_ERR;
		}
	}
	if(buf[0] == '1')
	{
		memset(sendbuf,0,SENDBUF);
		snprintf(sendbuf, 23,"HEADR0011INUSING0017\r\n");
		goto OFFHOOK_ERR;
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
			memset(sendbuf,0,SENDBUF);
			snprintf(sendbuf, 23,"HEADR0011INUSING0014\r\n");
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
				do_cmd_talkbackonhook(&devlist[i],sendbuf);
				phone_control.offhook_kill_talkback = 1;
				usleep(100*1000);
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
		phone_control.dial_over = 0;
		
		if(dev->isswtiching == 0)
			if(phone_control.global_incoming == 1)
				startaudio(dev,PCM_RESET,CALLED);
			else
				startaudio(dev,PCM_RESET,CALL);
		else
			startaudio(dev,PCM_NO_RESET,SWITCH);
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
		//PRINT("phone_control.who_is_online.attach = %d\n",phone_control.who_is_online.attach);
		//PRINT("phone_control.who_is_online.client_fd = %d\n",phone_control.who_is_online.client_fd);
		phone_control.global_incoming = 0;
		dev->incoming = 0;
		phone_control.ringon_flag = 0;
#ifdef B6
		if(phone_control.vloop > 3 || phone_control.vloop < -3)
			led_control(LED_OFFHOOK_IN);
		else
			led_control(LED_OFFHOOK_OUT);
#endif
		netWrite(dev->client_fd, "HEADR0011INUSING0010\r\n", 23);	
	}
	else
	{
		memset(sendbuf,0,SENDBUF);
		snprintf(sendbuf, 23,"HEADR0011INUSING0014\r\n");
		goto OFFHOOK_ERR;
	}
	return 0;
OFFHOOK_ERR:
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
					netWrite(devlist[i].client_fd,"HEADR0010RINGEND000",strlen("HEADR0010RINGEND000"));
				}
			}
			phone_control.global_incoming = 0;
			//usleep(400*1000);
			usleep(1000*1000);
		}
		else
			stopaudio(dev,PSTN,0);		
		phone_control.global_phone_is_using = 0;
		dev->dev_is_using = 0;
		dev->isswtiching = 0;
		clean_who_is_online();
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
#ifdef SAVE_OUT_DATE
		fwrite(global_out_buf, 1, global_out_buf_pos , out_file);
		global_out_buf_pos = 0;
		fclose(out_file);
		//PRINT("save out file success\n");
#endif
#ifdef SAVE_FILE
		fwrite(read_file_buf, 1, global_read_buf_pos , read_file);
		fwrite(write_file_buf, 1, global_write_buf_pos , write_file);
		global_read_buf_pos = 0;
		global_write_buf_pos = 0;
		fclose(read_file);
		fclose(write_file);
		PRINT("save file success\n");
#endif

		start_read_incoming();
		if(phone_control.vloop > 3 || phone_control.vloop < -3)
		{
			led_control(LED_LINE_IN);
#ifndef B6
			led_control(LED2_ON);
#endif
		}
		else
		{
			led_control(LED_LINE_OUT);
#ifndef B6
			led_control(LED2_OFF);
#endif
		}
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
	char id_buf[4]={0};
	memcpy(id_buf,&cli_req_buf[phone_control.cli_req_buf_rp].arg[0],3);
	//解析转子机命令得到id和ip
	ComFunPrintfBuffer(id_buf,3);
	id = atoi(id_buf);
	memcpy(client_ip,&cli_req_buf[phone_control.cli_req_buf_rp].arg[3],cli_req_buf[phone_control.cli_req_buf_rp].arglen-3);//10.10.10.103 //172.16.0.1
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
	startaudio(dev,PCM_RESET,OTHER);
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
	PRINT("%s\n",__FUNCTION__);
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
		stopaudio(dev,TALKBACK,0);
	}
	//memset(sendbuf,0,SENDBUF);
	//snprintf(sendbuf, 22,"HEADR0010STOPTB_000\r\n");
	//netWrite(dev->client_fd, sendbuf, strlen(sendbuf));
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

int strtobig(unsigned char *data,unsigned int len)
{
	int i = 0;
	unsigned char *datap = data;
	for(i=0;i<len;i++)
	{
		if((*datap >= 'a') && (*datap <= 'z'))
		{
			*datap -= 32;
		}
		datap++;
	}
	return 0;
}

int strtosmall(unsigned char *data,unsigned int len)
{
	int i = 0;
	unsigned char *datap = data;
	for(i=0;i<len;i++)
	{
		if((*datap >= 'A') && (*datap <= 'Z'))
		{
			(*datap) += 32;
		}
		datap++;
	}
	return 0;
}

#ifdef B6L
int get_532_ver(dev_status_t *dev)
{
	int ret = 0;
	char buf[SENDBUF] = {0};
	char sendbuf[SENDBUF] = {0};	
	int fd = open(TMP_AS532_VER_FILE,O_RDWR);
	if(fd < 0)
	{
		PRINT("open 532 file err\n");
		goto GET_532VER_ERR;
	}
	ret = read(fd,buf,SENDBUF);
	//HBD_B6L_AS532_xxxxxxxx
	if(ret > strlen(AS532_VER))
	{
		if(!strncmp(buf,AS532_VER,strlen(AS532_VER)))
		{
			PRINT("get ver\n");
			//HEADR0011532_VER0xxHBD_B6L_AS532_xxxxxxxx
			sprintf(sendbuf,"HEADR0%03d532_VER%03d%s",strlen(buf)+10-1,strlen(buf)-1,buf);
			PRINT("send_buf = %s\n",sendbuf);
			netWrite(dev->client_fd,sendbuf,strlen(sendbuf));
			close(fd);
			return 0;
		}
		close(fd);
		goto GET_532VER_ERR;
	}
GET_532VER_ERR:
	netWrite(dev->client_fd,"HEADR1011532_VER000",strlen("HEADR1011532_VER000"));
	return -1;
}
#endif

int generate_incoming(dev_status_t *dev)
{
	PRINT("%s\n",__FUNCTION__);
	char sendbuf[128] = {0};
	if(phone_control.global_incoming == 0 || dev->incoming == 1)
	{
		PRINT("no incoming\n");
		return -1;
	}
	snprintf(sendbuf, 22+phone_control.telephone_num_len, "HEADR0%03dINCOMIN%03d%s\r\n",10+phone_control.telephone_num_len, phone_control.telephone_num_len, phone_control.telephone_num);
	netWrite(dev->client_fd, sendbuf,strlen(sendbuf)+1);
	dev->incoming = 1;
	PRINT("send incoming\n");
	return 0;
}

#ifdef REGISTER
int do_cmd_heartbeat(dev_status_t *dev,char *buf)
{
	int i,j,count =0;

	if(dev->tick_time == 0)
	{
		char dev_mac[20]={0};
		char comming_mac[20]={0};
		char register_state[4]={0};
		char pad_mac[20]={0};
		char insidebuf[10]={0};
		char device_name[30]={0};
		memcpy(comming_mac,buf,16);
		//去除mac后的空格
		for(i=0;i<strlen(comming_mac);i++)
		{
			if(comming_mac[i]==' ')
				comming_mac[i] = '\0';
		}
		PRINT("first heartbeat!\n");
		
		if(strlen(comming_mac) != 12)
			goto UNREGISTERED;
		
		//转换mac为大写
		strtobig(comming_mac,strlen(comming_mac));
		strcpy(dev->dev_mac,comming_mac);
		sqlite3_interface("terminal_register_tb","device_mac",comming_mac,"device_mac",dev_mac);
		PRINT("comming_mac=%s\n",comming_mac);
		PRINT("dev_mac=%s\n",dev_mac);
		strtobig(dev_mac,strlen(dev_mac));
		if(!strncmp(dev_mac,comming_mac,strlen(comming_mac)))
		{
			PRINT("dev is registered!\n");
			sqlite3_interface("terminal_register_tb","device_name",comming_mac,"device_mac",device_name);
			memset(dev->dev_name,' ',16);
			memcpy(dev->dev_name,device_name,16);
			for(i=0;i<16;i++)
			{
				if(dev->dev_name[i] == '\0')
				{
					dev->dev_name[i]=' ';
				}
			}
			PRINT("name = %s\n",dev->dev_name);
#ifdef B6
			netWrite(dev->client_fd,"HEADR0012REG_SUC002B6\r\n",23);
#elif defined(B6L)	
			netWrite(dev->client_fd,"HEADR0013REG_SUC003B6L\r\n",24);
#elif defined(S1) || defined(S1_F3A)
			netWrite(dev->client_fd,"HEADR0012REG_SUC002S1\r\n",23);
#else
			netWrite(dev->client_fd,"HEADR0010REG_SUC000\r\n",21);
#endif
			goto REGISTERED;
		}
#ifndef B6L		
		sqlite3_interface("terminal_base_tb","register_state","0","register_state",register_state);
		//PRINT("register_state = %s\n",register_state);
		sqlite3_interface("terminal_base_tb","pad_mac",comming_mac,"pad_mac",pad_mac);
		strtobig(pad_mac,strlen(pad_mac));
//		PRINT("comming_mac=%s\n",comming_mac);
		PRINT("pad_mac = %s\n",pad_mac);
		//PRINT("%d\n",strcmp(pad_mac,comming_mac));
		//PRINT("%d\n",strcmp(register_state,"0"));
		if(!strncmp(pad_mac,comming_mac,strlen(comming_mac)) && !strcmp(register_state,"0"))
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
			PRINT("name = %s\n",dev->dev_name);
#ifdef B6
			netWrite(dev->client_fd,"HEADR0012REG_SUC002B6\r\n",23);
#elif defined(B6L)	
			netWrite(dev->client_fd,"HEADR0013REG_SUC003B6L\r\n",24);
#elif defined(S1) || defined(S1_F3A)
			netWrite(dev->client_fd,"HEADR0012REG_SUC002S1\r\n",23);
#else
			netWrite(dev->client_fd,"HEADR0010REG_SUC000\r\n",21);
#endif
			goto REGISTERED;
		}
#endif
UNREGISTERED:		
		PRINT("dev is unregistered\n");
		for(i=0;i<CLIENT_NUM;i++)
		{
			if(devlist[i].client_fd == -1)
				continue;
			if(dev == &devlist[i])
				break;
		}
#ifdef B6
		netWrite(dev->client_fd,"HEADR0012REGFAIL002B6\r\n",23);
#elif defined(B6L)	
		netWrite(dev->client_fd,"HEADR0013REGFAIL003B6L\r\n",24);
#elif defined(S1) || defined(S1_F3A)
		netWrite(dev->client_fd,"HEADR0012REGFAIL002S1\r\n",23);
#else
		netWrite(dev->client_fd,"HEADR0010REGFAIL000\r\n",21);
#endif
		memset(insidebuf,0,10);
		sprintf(insidebuf,"%s","inside");
		insidebuf[6]=i+1;
		netWrite(phone_control_fd[0],insidebuf, 7);
		return -1;
	}
REGISTERED:
	//dev->encrypt_enable = 1;
	dev->tick_time++;
	dev->registered = 1;
	if(dev->tick_time == 1)
	{
		generate_up_msg();
		generate_incoming(dev);
	}
	return 0;
}
#else

int do_cmd_heartbeat(dev_status_t *dev,char *buf)
{
	int i,j,count =0;

	dev->tick_time++;
	if(dev->tick_time == 1)
	{
#ifdef B6
		netWrite(dev->client_fd,"HEADR0012REG_SUC002B6\r\n",23);
#elif defined(B6L)	
		netWrite(dev->client_fd,"HEADR0013REG_SUC003B6L\r\n",24);
#elif defined(S1) || defined(S1_F3A)
		netWrite(dev->client_fd,"HEADR0012REG_SUC002S1\r\n",23);
#else
		netWrite(dev->client_fd,"HEADR0010REG_SUC000\r\n",21);
#endif
		generate_up_msg();
		generate_incoming(dev);
	}
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
		if(phone_control.offhook_kill_talkback == 1)
		{
			PRINT("delay dialup\n");
			usleep(500*1000);
			phone_control.offhook_kill_talkback = 0;
		}
		usleep(1000*1000);//等待音频线程初始化结束
		for(i=0;i<CLIENT_NUM;i++)
		{
			if(devlist[i].dev_is_using == 1)
				count++;
		}
		if(count==1)
		{
			memset(phone_control.telephone_num,0,64);
			if(cli_req_buf[phone_control.cli_req_buf_rp].arglen > 64)
				cli_req_buf[phone_control.cli_req_buf_rp].arglen = 64;
			memcpy(phone_control.telephone_num, cli_req_buf[phone_control.cli_req_buf_rp].arg,cli_req_buf[phone_control.cli_req_buf_rp].arglen);
			phone_control.telephone_num[cli_req_buf[phone_control.cli_req_buf_rp].arglen]='\0';
			PRINT("phone_num:%s\n",phone_control.telephone_num);
			phone_control.telephone_num_len = cli_req_buf[phone_control.cli_req_buf_rp].arglen;
			dialup(phone_control.telephone_num, cli_req_buf[phone_control.cli_req_buf_rp].arglen);
		}
#ifdef B6L
	}
	else
	{
		memset(phone_control.telephone_num,0,64);
		if(cli_req_buf[phone_control.cli_req_buf_rp].arglen > 64)
			cli_req_buf[phone_control.cli_req_buf_rp].arglen = 64;
		memcpy(phone_control.telephone_num, cli_req_buf[phone_control.cli_req_buf_rp].arg,cli_req_buf[phone_control.cli_req_buf_rp].arglen);
		phone_control.telephone_num[cli_req_buf[phone_control.cli_req_buf_rp].arglen]='\0';
		PRINT("phone_num:%s\n",phone_control.telephone_num);
		phone_control.telephone_num_len = cli_req_buf[phone_control.cli_req_buf_rp].arglen;
		if(!strncmp(phone_control.telephone_num,AS532_NUM,strlen(AS532_NUM)))
		{
			get_532_ver(dev);
			return 0;
		}
		netWrite(dev->client_fd,"HEADR1011532_VER000",strlen("HEADR1011532_VER000"));
#endif
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
		char id_buf[4]={0};
		memcpy(id_buf,&cli_req_buf[phone_control.cli_req_buf_rp].arg[0],3);
		//解析转子机命令得到id和ip
		ComFunPrintfBuffer(id_buf,3);
		id = atoi(id_buf);
		memcpy(client_ip,&cli_req_buf[phone_control.cli_req_buf_rp].arg[3],cli_req_buf[phone_control.cli_req_buf_rp].arglen-3);//10.10.10.103 //172.16.0.1
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
		memset(phone_control.incoming_num,0,sizeof(phone_control.incoming_num));
		memcpy(phone_control.incoming_num,phone_control.telephone_num,phone_control.telephone_num_len);
		PRINT("%s\n",sendbuf);
	    netWrite(devlist[id].client_fd, sendbuf, strlen(sendbuf));
		devlist[id].isswtiching = 1;
		phone_control.global_phone_is_using = 0;
		phone_audio.dialup = 1;
		dev->dev_is_using = 0;
		dev->attach=0;
		dev->audio_reconnect=0;
		clean_who_is_online();
		stopaudio(dev,PSTN,1);
		phone_audio.audio_reconnect_flag = 0;
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
			char id_buf[4]={0};
			memcpy(id_buf,&cli_req_buf[phone_control.cli_req_buf_rp].arg[0],3);
			ComFunPrintfBuffer(id_buf,3);
			id = atoi(id_buf);
			memcpy(client_ip,&cli_req_buf[phone_control.cli_req_buf_rp].arg[3],cli_req_buf[phone_control.cli_req_buf_rp].arglen-3);//10.10.10.103 //172.16.0.1
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
			,"HEADR0%03dREQ_BTP%03d%03d%s\r\n",strlen(dev->client_ip)+13,strlen(dev->client_ip)+3,i,dev->client_ip);
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
		char id_buf[4]={0};
		memcpy(id_buf,&cli_req_buf[phone_control.cli_req_buf_rp].arg[0],3);
		ComFunPrintfBuffer(id_buf,3);
		id = atoi(id_buf);
        memcpy(client_ip,&cli_req_buf[phone_control.cli_req_buf_rp].arg[3],cli_req_buf[phone_control.cli_req_buf_rp].arglen-3);//10.10.10.103 //172.16.0.1
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
        ,"HEADR0%03dREQ_BTP%03d%03d%s\r\n",strlen(dev->client_ip)+13,strlen(dev->client_ip)+3,i,dev->client_ip);
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
	char id_buf[4]={0};
	memcpy(id_buf,&cli_req_buf[phone_control.cli_req_buf_rp].arg[0],3);
	ComFunPrintfBuffer(id_buf,3);
	id = atoi(id_buf);
	memcpy(client_ip,&cli_req_buf[phone_control.cli_req_buf_rp].arg[3],cli_req_buf[phone_control.cli_req_buf_rp].arglen-3);//10.10.10.103 //172.16.0.1
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

#ifndef B6
int do_cmd_test_on(dev_status_t * dev, char * sendbuf)
{
	dev->registered = 1;
	dev->tick_time = 1;
	dev->test_on = 1;
	return 0;
}

int do_cmd_set_mac(dev_status_t * dev, char * sendbuf)
{
	int i,ret = 0;
	char mac_buf[20]={0};
	char mac_buf_out[30]={0};
	char cmdbuf[50]={0};
	char *macp=mac_buf_out;
	pid_t status;
	if(dev->registered == 1 && dev->test_on == 1)
	{
		memcpy(mac_buf, cli_req_buf[phone_control.cli_req_buf_rp].arg,(cli_req_buf[phone_control.cli_req_buf_rp].arglen > 20)?20:(cli_req_buf[phone_control.cli_req_buf_rp].arglen));
		for(i=0;i<12;i++)
		{
			sprintf(macp,"%c",mac_buf[i]);
			macp++;
			if(i == 11)
				break;
			if(i % 2 != 0)
			{
				sprintf(macp,"%c",':');
				macp++;
			}
		}
		macp = '\0';
#if defined(S1_F3A)
		sprintf(cmdbuf,"%s%s","set_macaddr -a ",mac_buf_out);
#elif defined(S1)
		sprintf(cmdbuf,"%s%s","set_macaddr -b ",mac_buf_out);
#endif
		PRINT("cmdbuf = %s\n",cmdbuf);
		status = system(cmdbuf);
		PRINT("status = %d\n",status);
		if(status == -1)
		{
			PRINT("system fail!\n");
			netWrite(dev->client_fd,"HEADR0011SET_MAC0011\r\n",22);
			return -1;
		}
		else
		{
			ret = WEXITSTATUS(status);
			PRINT("ret = %d\n",ret);
			if(ret == 0)
			{
				PRINT("Update Mac Success!\n");
				netWrite(dev->client_fd,"HEADR0011SET_MAC0010\r\n",22);
				return 0;
			}
			else
			{
				PRINT("Update Mac Fail!\n");
				netWrite(dev->client_fd,"HEADR0011SET_MAC0011\r\n",22);
				return -1;
			}
		}
	}
	netWrite(dev->client_fd,"HEADR0011SET_MAC0011\r\n",22);
	return -1;
}

int do_cmd_set_sn(dev_status_t * dev, char * sendbuf)
{
	int ret = 0;
	int i = 0;
    unsigned char databuf[20] = {0};
    unsigned char cmdbuf[50] = {0};
	pid_t status;
	if(dev->registered == 1 && dev->test_on == 1)
	{
		memcpy(databuf, cli_req_buf[phone_control.cli_req_buf_rp].arg,(cli_req_buf[phone_control.cli_req_buf_rp].arglen > 20)?20:(cli_req_buf[phone_control.cli_req_buf_rp].arglen));
		snprintf(cmdbuf,32,"%s%s","fw_setenv SN ",databuf);
		PRINT("cmdbuf = %s\n",cmdbuf);
		status = system(cmdbuf);
		PRINT("status = %d\n",status);
		if(status == -1)
		{
			PRINT("system fail!\n");
			netWrite(dev->client_fd,"HEADR0011SET__SN0011\r\n",22);
			return -1;
		}
		else
		{
			ret = WEXITSTATUS(status);
			PRINT("ret = %d\n",ret);
			if(ret == 0)
			{
				PRINT("Update Sn Success!\n");
				netWrite(dev->client_fd,"HEADR0011SET__SN0010\r\n",22);
				return 0;
			}
			else
			{
				PRINT("Update Sn Fail!\n");
				netWrite(dev->client_fd,"HEADR0011SET__SN0011\r\n",22);
				return -1;
			}
		}
	}
	netWrite(dev->client_fd,"HEADR0011SET__SN0011\r\n",22);
	return -1;
}
#endif

#if defined(S1_F3A)
int do_cmd_usb_test(dev_status_t * dev, char * sendbuf)
{
	struct in_addr addr;
	int ret;
    int status;
    struct   stat   buf;
	char mac_buf[20]={0};
	status = system("grep sd* /proc/partitions > /tmp/partitions");
		PRINT("1111111%d !!!\n \n", status);
    stat("/tmp/partitions", &buf);
		PRINT("2222222%d !!!\n \n", buf.st_size);
	if (buf.st_size > 30)
	{
		PRINT("usb test success !!!\n \n");
		netWrite(dev->client_fd,"HEADR0010USBTEST000",strlen("HEADR0010USBTEST000"));
		ret = 0;
	} else {
	    PRINT("usb test fail !!!\n");
	    netWrite(dev->client_fd,"HEADR1010USBTEST000",strlen("HEADR1010USBTEST000"));
	    ret = -1;
    }
    system("rm /tmp/partitions");
    return ret;
}
#endif
#if defined(S1) || defined(S1_F3A)
int do_cmd_get_s1_ip(dev_status_t * dev, char * sendbuf)
{
	struct in_addr addr;
	int i,j;
	char mac_buf[20]={0};
	struct dhcpOfferedAddr 
	{
		unsigned char hostname[16];
		unsigned char mac[16];
		unsigned long ip;
		unsigned long expires;
	} lease;
	system("killall -q -USR1 udhcpd");
	FILE *fp = fopen(UDHCPD_FILE, "r");  /*  /var/run/udhcpd.leases   */
	if (NULL == fp)
	{
		printf("open udhcpd error\n \n");
		netWrite(dev->client_fd,"HEADR1010GETS1IP000",strlen("HEADR1010GETS1IP000"));
		return -1;
	}

	while(1) 
	{
		memset(&lease,0, sizeof(lease));
		if(fread(&lease, sizeof(lease), 1, fp) != 1)
			break;
		addr.s_addr = lease.ip;
		//for(i = 0, j = 0 ; i < 6; i++, j+=3)
		//{
			//sprintf(&mac_buf[j], "%02x:", lease.mac[i]);
		//}

		PRINT("hostname = %s\n",lease.hostname);
		PRINT("strlen(hostname) = %d\n",strlen(lease.hostname));
		//printf("mac = %s\n",mac_buf);
		PRINT("ip = %s\n",inet_ntoa(addr));
		//if(lease.hostname == NULL)
			//strcpy(lease.hostname,A20_NAME);
		if(!strcmp(lease.hostname,A20_NAME))
		{
			//netWrite(dev->client_fd,"HEADR0011GETS1IP000",strlen("HEADR1011GETS1IP000"));
			sprintf(sendbuf,"HEADR0%03dGETS1IP%03d%s",strlen(inet_ntoa(addr))+10,strlen(inet_ntoa(addr)),inet_ntoa(addr));
			PRINT("send_buf = %s\n",sendbuf);
			netWrite(dev->client_fd,sendbuf,strlen(sendbuf));
			fclose(fp);
			return 0;
		}
		//printf("expires = %ld\n",lease.expires);
		//printf("******************\n");
	
	}
	PRINT("not found a20\n");
	netWrite(dev->client_fd,"HEADR1010GETS1IP000",strlen("HEADR1010GETS1IP000"));
	fclose(fp);
	return -1;
}
#endif

int do_cmd_req_enc(dev_status_t * dev, char * sendbuf)
{
	dev->encrypt_enable = 1;
	netWrite(dev->client_fd,"HEADR0010SUC_ENC000\r\n",22);
	return 0;
}

int do_cmd_get_ver(dev_status_t * dev, char * sendbuf)
{
	sprintf(sendbuf,"HEADR0%03dGET_VER%03d%s",strlen(phone_control.version)+10,strlen(phone_control.version),phone_control.version);
	netWrite(dev->client_fd,sendbuf,strlen(sendbuf));
	return 0;
}

int do_cmd_def_name(dev_status_t *dev, char *buf)
{
	if(buf == NULL)
		return -1;
	int len = strlen(buf);
	memset(dev->dev_name,0,sizeof(dev->dev_name));
	memcpy(dev->dev_name,buf,((len > (sizeof(dev->dev_name)-1))?(sizeof(dev->dev_name)-1):len));
	if(phone_control.vloop < 3 && phone_control.vloop > -3)
	{
		netWrite(dev->client_fd,"HEADR0011LINESTA0011",strlen("HEADR0011LINESTA0011"));
	}
	else
	{
		netWrite(dev->client_fd,"HEADR0011LINESTA0010",strlen("HEADR0011LINESTA0010"));
	}
	if(dev->tick_time > 1)
	{
		generate_up_msg();
	}
	return 0;
}

int do_cmd_req_dial(dev_status_t *dev, char *buf)
{
	char sendbuf[BUF_LEN_256] = {0};
	if((phone_control.global_incoming == 1 || dev->isswtiching == 1) && strcmp(buf,phone_control.incoming_num) == 0)
	{
		PRINT("req dial ok\n");
		sprintf(sendbuf,"HEADR0011DIALSTA0011",strlen("HEADR0011DIALSTA0011"));
		netWrite(dev->client_fd,sendbuf,strlen(sendbuf));
		return 0;	
	}
	PRINT("req dial not ok\n");
	sprintf(sendbuf,"HEADR0011DIALSTA0010",strlen("HEADR0011DIALSTA0010"));
	netWrite(dev->client_fd,sendbuf,strlen(sendbuf));
	return 0;
}

//消息处理
int parse_msg(cli_request_t* cli,char *sendbuf)
{
	int i;
	if(cli->dev->client_fd < 0)
		return 0;
	switch(cli->cmd)
	{
		case HEARTBEAT:
		{
			PRINT("HEARTBEAT from %s\n",cli->dev->client_ip);
#ifdef REGISTER
			do_cmd_heartbeat(cli->dev,cli->arg);
#else
			do_cmd_heartbeat(cli->dev,cli->arg);
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
			do_cmd_offhook(cli->dev,cli->arg);
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
#ifndef B6
		case TEST_ON:
		{
			PRINT("TEST_ON from %s\n",cli->dev->client_ip);
			do_cmd_test_on(cli->dev,sendbuf);
			break;
		}
		case SET_MAC:
		{
			PRINT("SET_MAC from %s\n",cli->dev->client_ip);
			do_cmd_set_mac(cli->dev,sendbuf);
			break;
		}
		case SET_SN:
		{
			PRINT("SET_SN from %s\n",cli->dev->client_ip);
			do_cmd_set_sn(cli->dev,sendbuf);
			break;
		}
#endif
#if defined(S1) || defined(S1_F3A)
		case GET_S1_IP:
		{
			PRINT("GET_S1_IP from %s\n",cli->dev->client_ip);
			do_cmd_get_s1_ip(cli->dev,sendbuf);
			break;
		}
#if defined(S1_F3A)
		case USB_TEST:
		{
			PRINT("USB_TEST from %s\n",cli->dev->client_ip);
			do_cmd_usb_test(cli->dev,sendbuf);
			break;
		}
#endif
#endif
		case REQ_ENC:
		{
			PRINT("REQ_ENC from %s\n",cli->dev->client_ip);
			do_cmd_req_enc(cli->dev,sendbuf);
			break;
		}
		case GET_VER:
		{
			PRINT("GET_VER from %s\n",cli->dev->client_ip);
			do_cmd_get_ver(cli->dev,sendbuf);
			break;
		}
		case DEF_NAME:
		{
			PRINT("DEF_NAME from %s\n",cli->dev->client_ip);
			do_cmd_def_name(cli->dev,cli->arg);
			break;
		}
		case REQ_DIAL:
		{
			PRINT("REQ_DIAL from %s\n",cli->dev->client_ip);
			do_cmd_req_dial(cli->dev,cli->arg);
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
#ifndef B6
	else if (strncmp(cmd_str, "TEST_ON", 7) == 0)
	{
		cmdtype = TEST_ON;
	}
	else if (strncmp(cmd_str, "SET_MAC", 7) == 0)
	{
		cmdtype = SET_MAC;
	}
	else if (strncmp(cmd_str, "SET__SN", 7) == 0)
	{
		cmdtype = SET_SN;
	}
#endif
#if defined(S1) || defined(S1_F3A)
	else if (strncmp(cmd_str, "GETS1IP", 7) == 0)
	{
		cmdtype = GET_S1_IP;
	}
#if defined(S1_F3A)
	else if (strncmp(cmd_str, "USBTEST", 7) == 0)
	{
		cmdtype = USB_TEST;
	}
#endif
#endif
	else if (strncmp(cmd_str, "REQ_ENC", 7) == 0)
	{
		cmdtype = REQ_ENC;
	}
	else if (strncmp(cmd_str, "GET_VER", 7) == 0)
	{
		cmdtype = GET_VER;
	}
	else if (strncmp(cmd_str, "DEFNAME", 7) == 0)
	{
		cmdtype = DEF_NAME;
	}
	else if (strncmp(cmd_str, "REQDIAL", 7) == 0)
	{
		cmdtype = REQ_DIAL;
	}
	else
	{
		PRINT("command undefined\n");
		cmdtype = UNDEFINED;
	}

	return cmdtype;
}

int get_version()
{
	char buf[BUFFER_SIZE_1K]={0};
	char *p = NULL;
	int i,ret = 0;
	system("cfg -e | grep \"SOFT_VERSION=\" > /tmp/phone_control_version");
	int fd = open("/tmp/phone_control_version",O_RDONLY);
	if(fd < 0)
	{
		PRINT("get version err\n");
		return -1;
	}
	ret = read(fd,buf,BUFFER_SIZE_1K);
	if(ret > 0)
	{
		memset(phone_control.version,0,sizeof(phone_control.version));
		p = strstr(buf, "SOFT_VERSION");
		if(p != NULL)
		{
			sscanf(p,"SOFT_VERSION=\"%s",phone_control.version);
			for(i=0;i<strlen(phone_control.version);i++)
			{
				if(phone_control.version[i] == '"')
					phone_control.version[i] = '\0';
			}
			PRINT("version:%s\n",phone_control.version);
		}
	}
	close(fd);
	system("rm -rf /tmp/phone_control_version");
	return 0;
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

	if(listen(sockfd,CLIENT_NUM)<0)
	{
		perror("control listen error\n");
		exit(-1);
	}
	PRINT("control listen success\n");

	phone_control.phone_control_sockfd=sockfd;

	phone_audio.init_audio();

	if(si32178_init(0,0,0,0,10,1,1,0)==-1)
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

#ifdef B6
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
#else
	phone_control.led_control_fd = open(LED_NAME,O_RDWR);
	if(phone_control.led_control_fd < 0)
	{
		PRINT("open %s error\n",LED_NAME);
		//exit(-1);
	}
	else
	{
		PRINT("open %s success\n",LED_NAME);
	}	
#endif

	get_version();
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
			if(devlist[i].client_fd != -1 && devlist[i].incoming == 1 && devlist[i].registered == 1 && devlist[i].dying == 0)
#else
			if(devlist[i].client_fd != -1 && devlist[i].incoming == 1  && devlist[i].dying == 0)
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
#ifdef B6
	led_control(LED_INCOMING);
#endif
	if(flag == FSK)
	{
		num_len=(int)ppacket[12];
	}
	else if(flag == DTMF)
	{
		num_len=(int)ppacket[8];
	}

	//如果正在对讲，干掉！！
	for(i=0;i<CLIENT_NUM;i++)
	{
		if(devlist[i].client_fd == -1)
			continue;
		if(devlist[i].talkbacking || devlist[i].talkbacked)
		{
			PRINT("stop talkbacking!\n");
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
	phone_control.global_incoming = 1;
	if(num_len >= 64)
		num_len = 63;
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
	memset(phone_control.incoming_num,0,sizeof(phone_control.incoming_num));
	memcpy(phone_control.incoming_num,phone_control.telephone_num,num_len);
#ifdef B6
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
#endif

	snprintf(sendbuf, 22+num_len, "HEADR0%03dINCOMIN%03d%s\r\n",10+num_len, num_len, phone_control.telephone_num);
	//snprintf(sendbuf, 22, "HEADR0%03dINCOMIN%03d\r\n",10,0);
	for(i=0; i<CLIENT_NUM; i++)
	{
#ifdef REGISTER
		if(devlist[i].client_fd != -1 && devlist[i].registered == 1  && devlist[i].dying == 0)
#else
		if(devlist[i].client_fd != -1  && devlist[i].dying == 0)
#endif		
		{
			netWrite(devlist[i].client_fd, sendbuf,strlen(sendbuf)+1);
			devlist[i].incoming = 1;
		}
	}
	phone_control.ringon_flag=1;
	phone_audio.dialup = 1;
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

#ifdef B6
void led_control(char type)
{
	char led[3] = {0x5A,0XA5};
	led[2] = type;
	write(phone_control.passage_fd,led,3);
}
#endif

int check_incoming_num(char *incoming_num,int len)
{
	int i;
	if(len == 0)
		return 0;
	for(i=0;i<len;i++)
	{
		if(incoming_num[i] != '#' && incoming_num[i] != '*' && incoming_num[i] != '+' && (incoming_num[i] > '9' || incoming_num[i] < '0'))
			return -1;
	}
	return 0;
}

int get_incoming_num(char *incoming_num)
{
	/*
	unsigned char* datep = NULL;
	struct __fsk_num f_num;
	unsigned char date[64] = {0};
	int i,j;
	int len = 0;
	memset(&f_num,0,sizeof(f_num));
	for(i=0,j=0;i<Fsk_CID_Len;i+=2,j++)
	{
		date[j] = (unsigned char)(Fsk_CID[i] & 0xff)+ (unsigned char)((Fsk_CID[i+1] & 0xff) << 4);
		printf("0x%02X ", date[j]);
	}
	printf("\n");
	if(date[0] == (unsigned char)DOUBLE)
	{
		f_num.type = (unsigned char)DOUBLE;
	}
	else if(date[0] == (unsigned char)SINGLE)
	{
		f_num.type = (unsigned char)SINGLE;
	}
	else
		f_num.type = (unsigned char)OTHER_CODE;
	switch(f_num.type)
	{
		case DOUBLE:
			datep = date+date[3]+4;
			if(*datep != (unsigned char)0x02)
				return -1;
			len = *(datep+1);
			printf("len = %d\n",len);
			memcpy(incoming_num,datep+2,len);
			break;
		case SINGLE:
			datep = date;
			len = (*(datep+1)-8);
			printf("len = %d\n",len);
			memcpy(incoming_num,datep+10,len);
			break;
		default:
			break;
	}
	* */
	int i;
	if(Fsk_CID_Len >= SENDBUF)
		Fsk_CID_Len = SENDBUF-1;
	for(i=0;i<Fsk_CID_Len;i++)
	{
		incoming_num[i] = (unsigned char)(Fsk_CID[i] & 0xff);
		printf("0x%02X ", incoming_num[i]);
		incoming_num[i] += '0';
	}
	printf("\n");
	printf("incoming_num1 ?= %s\n",incoming_num);
	return 0;
}

void *loop_check_ring(void * argv)
{
	PRINT("%s thread start.......\n",__FUNCTION__);
	unsigned char dtmf_buf[BUFFER_SIZE_1K*10]={0};
	unsigned char incoming_num[SENDBUF]={0};
	unsigned char incoming_num1[SENDBUF]={0};
	unsigned char incoming_num2[SENDBUF]={0};
	int ret,read_ret = 0;
	int i,loop_count = 0;
	unsigned char packet_buffer[40]={0xA5,0x5A,0x00,0x00,0x10,0xA5,0x5A,0x04};
	unsigned char ringon_buffer[12]={0xA5,0x5A,0x00,0x07,0x10,0xA5,0x5A,0x03,0x00,0x03,0xE8};
	Fsk_Message_T fskmsg;
	short iloop = 0;;
	unsigned char fdt = 0;
	int line = 0;
	int line_in = 0;
	int line_out = 0;
	memset(&fskmsg,0,sizeof(fskmsg));
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
#ifndef B6
					led_control(LED2_ON);
#endif
					for(i=0;i<CLIENT_NUM;i++)
					{
						if(devlist[i].client_fd == -1)
							continue;
						netWrite(devlist[i].client_fd,"HEADR0011LINESTA0010",strlen("HEADR0011LINESTA0010"));
					}
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
#ifndef B6
					led_control(LED2_OFF);
#endif
					for(i=0;i<CLIENT_NUM;i++)
					{
						if(devlist[i].client_fd == -1)
							continue;
						netWrite(devlist[i].client_fd,"HEADR0011LINESTA0011",strlen("HEADR0011LINESTA0011"));
					}
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
					if(phone_control.ring_neg_count == 1)
						phone_control.ring_count = 0;
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
				ret = Fsk_GetFskMsg(&fskmsg);
				if(ret == 1)
				{
					PRINT("Get Fsk mfy...\n");
					phone_control.get_fsk_mfy = 1;
					phone_control.ring_count = 110;
				}
				if(ret == 2)
				{
					PRINT("Get Fsk zzl...\n");
					phone_control.get_fsk_zzl = 1;
					phone_control.ring_count = 110;
				}
			}
			else if(phone_control.Status.ringDetected == 0)
			{
				if(phone_control.ring_count > 0 && phone_control.ring_neg_count > 0)
				{
					PRINT("ringDectected = %d\n",phone_control.Status.ringDetected);
					//结束检测ring
					phone_control.check_ringon_count = RING_LIMIT;
					for(i=0;i<CLIENT_NUM;i++)
					{
						if(devlist[i].client_fd == -1)
							continue;
						if(devlist[i].incoming == 1 && devlist[i].dev_is_using != 1)
							netWrite(devlist[i].client_fd,"HEADR0010RINGEND000",strlen("HEADR0010RINGEND000"));
					}
				}
				phone_control.ring_count = 0;
				phone_control.ring_neg_count = 0;
			}
			if(((phone_audio.get_code == 1) && (phone_control.ring_neg_count > 1)) || ((phone_control.ring_count == 120) && (phone_control.ring_neg_count > 5)) || 
			(phone_control.get_fsk_mfy == 1 && phone_control.get_fsk_zzl == 1 && phone_control.ring_neg_count > 1) || ((phone_control.ring_count == 120) && (phone_control.get_fsk_zzl == 1) && (phone_control.ring_neg_count > 1))
			|| ((phone_control.ring_count == 120) && (phone_control.get_fsk_mfy == 1) && (phone_control.ring_neg_count > 1)))
			{
				PRINT("phone_control.ring_count = %d\n",phone_control.ring_count);
				//产生Incoming
				phone_control.ring_count += 120;
				memset(incoming_num,0,SENDBUF);
				memset(incoming_num1,0,SENDBUF);
				memset(incoming_num2,0,SENDBUF);
				if(phone_audio.get_code == 1)
				{
					DtmfGetCode(incoming_num);
				}
				else
				{
					if(phone_control.get_fsk_zzl == 1)
						get_incoming_num(incoming_num1);
					//PRINT("num_len = %d\n",strlen(fskmsg.num));
					//zb 14-06-27
					//if(fskmsg.isgood == TRUE)
					//{
					if(phone_control.get_fsk_mfy == 1)
						memcpy(incoming_num2,fskmsg.num,strlen(fskmsg.num));
					//}
					memset(&fskmsg,0,sizeof(fskmsg));
					Fsk_FinishFskData();
					PRINT("num_zzl: %s\n",incoming_num1);
					PRINT("num_mfy: %s\n",incoming_num2);
					if(strlen(incoming_num1) != 1 || strlen(incoming_num2) != 1)
					{
						if(check_incoming_num(incoming_num1,strlen(incoming_num1)) != 0)
							memset(incoming_num1,0,SENDBUF);
						if(check_incoming_num(incoming_num2,strlen(incoming_num2)) != 0)
							memset(incoming_num2,0,SENDBUF);
						if(strlen(incoming_num1) == 0)
							memcpy(incoming_num,incoming_num2,strlen(incoming_num2));
						else if(strlen(incoming_num2) == 0)
							memcpy(incoming_num,incoming_num1,strlen(incoming_num1));
						else
							memcpy(incoming_num,incoming_num2,strlen(incoming_num2));
					}
				}
				phone_audio.get_code = 0;
				phone_control.get_fsk_mfy = 0;
				phone_control.get_fsk_zzl = 0;
				if(strlen(incoming_num) != 1)
				{
					PRINT("Incoming.....\n");
					if(check_incoming_num(incoming_num,strlen(incoming_num)) != 0)
						memset(incoming_num,0,SENDBUF);
					generate_incoming_msg(packet_buffer,incoming_num,strlen(incoming_num));
					//stop_read_incoming();
					memcpy((unsigned char*)(&handle_up_msg_buffer[phone_control.handle_up_msg_wp_pos]),packet_buffer,strlen(incoming_num)+12);
					phone_control.handle_up_msg_wp_pos += (strlen(incoming_num)+12);
				}
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
	// PRINT("phone_control.cli_req_buf_wp = %d\n",phone_control.cli_req_buf_wp);
	// PRINT("phone_control.cli_req_buf_rp = %d\n",phone_control.cli_req_buf_rp);

	memset(&cli_req_buf[phone_control.cli_req_buf_wp], 0, sizeof(cli_request_t));
	memcpy(cli_req_buf[phone_control.cli_req_buf_wp].head,pbuf,5);
	if(strcmp(cli_req_buf[phone_control.cli_req_buf_wp].head,"HEADS"))
	{
		PRINT("msg error\n");
		memset(&cli_req_buf[phone_control.cli_req_buf_wp],0,sizeof(cli_request_t));
		return -1;
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

	if(cli_req_buf[phone_control.cli_req_buf_wp].arglen > sizeof(cli_req_buf[phone_control.cli_req_buf_wp].arg))
	{	
		PRINT("msg length err\n");
		memset(&cli_req_buf[phone_control.cli_req_buf_wp],0,sizeof(cli_request_t));
		return -1;
	}
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
	//char length_more[4]={0};
	int optval;
	int optlen = sizeof(int);

	fd_set socket_fdset;
	struct timeval tv;
	memset(&tv, 0, sizeof(struct timeval));


	while(1)
	{
		int maxfd = -1;
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
						getsockopt(devlist[i].client_fd,SOL_SOCKET,SO_ERROR,&optval, &optlen);
						//PRINT("optval = %d\n",optval);
						if(optval == 0)
						{
							ret=recv(devlist[i].client_fd,msg,BUFFER_SIZE_2K,0);
							//PRINT("ret = %d\n",ret);						
						}
						else
						{
							ret = -1;
						}
						if(ret<0)
						{
							PRINT("read failed\n");
							devlist[i].dying = 1;
							memset(sendbuf,0,SENDBUF);
							sprintf(sendbuf,"%s","INSIDE");
							sendbuf[6]=i+1;
							netWrite(phone_control_fd[0],sendbuf, 7);
							continue;
						}
						else if(ret==0)
						{
							//PRINT("dying = %d\n",devlist[i].dying);
							//PRINT("%s\n",devlist[i].client_ip);
							PRINT("socket return 0\n");
							devlist[i].dying = 1;
							memset(sendbuf,0,SENDBUF);
							sprintf(sendbuf,"%s","INSIDE");
							sendbuf[6]=i+1;
							netWrite(phone_control_fd[0],sendbuf, 7);
							continue;
						}
						PRINT("%s\n",msg);
						pbuf=msg;
again:
						if(getoutcmd(pbuf,ret,&devlist[i])==-1)
						{
							continue;
						}

						//parse_msg(&devlist[i]);
						int tmp_len=phone_control.last_cli_length;
						if(!strncmp(pbuf+9+tmp_len,"HEADS",5))
						{
							//PRINT("recv more msg\n");
							//memcpy(length_more, pbuf+tmp_len+15, 3);
							//int len_more = atoi(length_more);
							pbuf = pbuf+9+tmp_len;
							ret = ret-tmp_len-9;
							//PRINT("ret=%d\n",ret);
							if(ret<19)
							{
								continue;
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
					if(online_dev_num == CLIENT_NUM-1)
					{
						PRINT("client limit\n");
						netWrite(clientfd, "HEADR0010LIMITED000\r\n",22);
						usleep(10*1000);
						close(clientfd);
						break;
					}
					if(clientfd >= 0)
					{
						for(i=0; i<CLIENT_NUM; i++)
						{
							if(devlist[i].client_fd == -1)
							{
								memset(devlist[i].dev_name,' ',16);
								memcpy(devlist[i].dev_name,new_ip,strlen(new_ip));
								PRINT("default name\n");
								//sprintf(devlist[i].dev_name,"%s%d","子机",i+1);
								for(j=0;j<16;j++)
								{
									if(devlist[i].dev_name[j] == '\0')
									{
										devlist[i].dev_name[j]=' ';
									}
								}							
								devlist[i].client_fd = clientfd;
								devlist[i].id = i;
								memset(devlist[i].client_ip, 0, sizeof(devlist[i].client_ip));
								memcpy(devlist[i].client_ip, new_ip, strlen(new_ip)+1);
								for(j=0; j<CLIENT_NUM; j++)
								{
									if(devlist[j].client_fd == -1 || j == i)
										continue;
									if(!strcmp(new_ip,devlist[j].client_ip))
									{
										if(devlist[j].dev_is_using == 1 && devlist[j].dying == 1)
										{
											//PRINT("i = %d\nj = %d\n",i,j);
											devlist[i].control_reconnect=1;
											PRINT("reconnect\n");
											break;
										}
										devlist[j].dying = 1;
										memset(sendbuf,0,SENDBUF);
										sprintf(sendbuf,"%s","INSIDE");
										sendbuf[6]=j+1;
										netWrite(phone_control_fd[0],sendbuf, 7);
										//usleep(50*1000);									
										break;
									}
								}
								//generate_up_msg();
								break;
							}
						}
					}
					print_devlist();
				}
		}
		//	usleep(100*1000);
	}
}

int parse_msg_inside(char *msg)
{
	PRINT("%s\n",__FUNCTION__);
	char num = msg[6];
	//PRINT("num=%c\n",num);
	//if(num>='0' && num <='4')
	//{
		//PRINT("dev %d will be destroyed\n",(int)num-48);
		//destroy_client(&devlist[(int)num-48]);
	//}
	PRINT("name=%s\n",devlist[num-1].dev_name);
	if(num>= 1 && num <=CLIENT_NUM)
	{
		PRINT("dev %d will be destroyed\n",num-1);
		if(msg[0] == 'I')
			destroy_client(&devlist[num-1],1);
		else if(msg[0] == 'i')
			destroy_client(&devlist[num-1],0);
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
				if(phone_control.vloop > 3 || phone_control.vloop < -3)
				{
					led_control(LED_LINE_IN);
#ifndef B6
					led_control(LED2_ON);
#endif
				}
				else
				{
					led_control(LED_LINE_OUT);
#ifndef B6
					led_control(LED2_OFF);
#endif
				}
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
	if(phone_control.send_sw_ringon_count >= 5)
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
				//PRINT("send_sw_ringon_limit = %d\n",phone_control.send_sw_ringon_limit);
				if(phone_control.send_sw_ringon_limit == 40 && devlist[i].dev_is_using == 0)
				{
					if(devlist[i].isswtiching == 1)
					{
						do_cmd_onhook(&devlist[i]);
						netWrite(devlist[i].client_fd,"HEADR0010RINGEND000",strlen("HEADR0010RINGEND000"));
						//onhook();
						//if(!set_onhook_monitor())
							//PRINT("set_onhook_monitor success\n");
						//开始检测incoming
						//start_read_incoming();

					}
					//响铃超时
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
				//onhook();
				//if(!set_onhook_monitor())
					//PRINT("set_onhook_monitor success\n");
				//start_read_incoming();
			}
		}
		phone_control.send_sw_ringon_count = 0;
	}
}

void send_tb_ringon()
{
	phone_control.send_tb_ringon_count++;
	if(phone_control.send_tb_ringon_count >= 5)
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
				//PRINT("send_tb_ringon_limit = %d\n",phone_control.send_tb_ringon_limit);
				
				phone_control.send_tb_ringon_limit++;
				if(phone_control.send_tb_ringon_limit == 60 && devlist[i].talkback_answer == 0)
				{
					//响铃超时
					do_cmd_talkbackonhook(&devlist[i],tb_sendbuf);
					netWrite(devlist[i].client_fd,"HEADR0010TBR_END000",strlen("HEADR0010TBR_END000"));
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
				//stopaudio(&devlist[phone_control.tb_dev],TALKBACK);
				//do_cmd_talkbackonhook(&devlist[phone_control.tb_dev],tb_sendbuf);
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
		if(phone_audio.get_code_timeout_times >= 11)
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
		if(phone_audio.audio_reconnect_loops >= 16)//超过3秒没有重连，则需要挂机
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
					//if(devlist[i].client_fd == phone_control.who_is_online.client_fd)
					if(!strcmp(phone_control.who_is_online.client_ip,devlist[i].client_ip))
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

void get_phone_status()
{
	int i = 0;
	int isswtiching = 0;
	for(i=0;i<CLIENT_NUM;i++)
	{
		if(devlist[i].client_fd == -1)
			continue;
		if(devlist[i].isswtiching == 1)
			isswtiching++;
	}
	if(phone_control.global_incoming == 0 && phone_control.global_phone_is_using == 0 && phone_control.global_talkback == 0 
	&& phone_control.ring_neg_count == 0 && isswtiching == 0)
	{
		phone_control.phone_busy = 0;
		//PRINT("idle!!\n");
	}
	else
	{
		phone_control.phone_busy = 1;
		//PRINT("busy!!\n");
	}
	return 0;
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
			get_phone_status();
			signal(SIGALRM, check_dev_tick);
			break;
	}

	return;
}

void* handle_down_msg(void* argv)
{
	PRINT("%s thread start.......\n",__FUNCTION__);
	char sendbuf[SENDBUF]={0};
	while(1)
	{
		if(phone_control.cli_req_buf_rp == phone_control.cli_req_buf_wp)
		{
			usleep(20*1000);
			continue;
		}
		memset(sendbuf,0,sizeof(sendbuf));
		parse_msg(&cli_req_buf[phone_control.cli_req_buf_rp],sendbuf);
		phone_control.cli_req_buf_rp++;
		if(phone_control.cli_req_buf_rp>=CLI_REQ_MAX_NUM)
			phone_control.cli_req_buf_rp = 0;
		// if(phone_control.cli_req_buf_wp>=CLI_REQ_MAX_NUM)
			// phone_control.cli_req_buf_wp = 0;
		usleep(20*1000);
	}
}

int generate_up_msg()
{
	PRINT("%s\n",__FUNCTION__);
	char sendbuf[BUFFER_SIZE_2K]={0};
	char tmpbuf1[BUFFER_SIZE_2K]={0};
	char tmpbuf2[BUFFER_SIZE_2K]={0};
	char lenbuf[3]={0};
	int count=0;
	int ip_len = 0;
	int i,j = 0;
	//组合消息
	memset(sendbuf,0,BUFFER_SIZE_2K);
	for(i=0;i<CLIENT_NUM;i++)
	{
#ifdef REGISTER
		if(devlist[i].client_fd == -1 || devlist[i].dying == 1 || devlist[i].registered == 0 /*|| devlist[i].tick_time == 0*/)
#else
		if(devlist[i].client_fd == -1 || devlist[i].dying == 1  /*|| devlist[i].tick_time == 0*/)
#endif
				continue;
		ip_len = strlen(devlist[i].client_ip);
		memset(tmpbuf1,0,BUFFER_SIZE_2K);
		memcpy(tmpbuf1,"#DEV#IP",7);
		sprintf(&tmpbuf1[7],"%03d",i);
		memcpy(tmpbuf1+10,devlist[i].client_ip,ip_len);
		memcpy(tmpbuf1+10+ip_len,"#NAME",5);
		memcpy(tmpbuf1+ip_len+15,devlist[i].dev_name,16);
		memcpy(tmpbuf2+count+3,tmpbuf1,31+ip_len);
		//PRINT("tmpbuf1 = %s\n",tmpbuf1);
		//PRINT("tmpbuf2 = %s\n",tmpbuf2+3);
		count+=31+ip_len; //7+5+16+3+ip_len
		j++;
	}
	sprintf(lenbuf,"%03d",j);
	memcpy(tmpbuf2,lenbuf,3);
	j = 0;
	memcpy(tmpbuf2+count+3,"\r\n\0",3);
	if(count!=0)
	{
		sprintf(sendbuf,"%s%03d%s","HEADR0",count+10,"OPTI_BS");
		memcpy(sendbuf+16,tmpbuf2,count+3);
		//PRINT("sendbuf = %s\n",sendbuf);
	}
	memset(tmpbuf2,0,BUFFER_SIZE_2K);
	for(i=0;i<CLIENT_NUM;i++)
	{
#ifdef REGISTER
		if(devlist[i].client_fd == -1 || devlist[i].dying == 1 || devlist[i].registered == 0 )
#else
		if(devlist[i].client_fd == -1 || devlist[i].dying == 1 )
#endif
			continue;
//		PRINT("%s:%d\n",devlist[i].client_ip,netWrite(devlist[i].client_fd, sendbuf, count+19));
		netWrite(devlist[i].client_fd, sendbuf, count+22);
	}
	return 0;
}

#ifdef REGISTER
int check_register_status()
{
	int j,i = 0;
	char dev_mac[20]={0};
	char register_state[4]={0};
	char pad_mac[20]={0};
	char insidebuf[10]={0};
	char device_name[30]={0};
	for(i=0;i<CLIENT_NUM;i++)
	{		
		if(devlist[i].client_fd == -1 || devlist[i].registered == 0)
			continue;		
		memset(dev_mac,0,20);
		memset(register_state,0,4);
		memset(pad_mac,0,20);
		memset(insidebuf,0,10);
		memset(device_name,0,30);
		//转换mac为大写
		strtobig(devlist[i].dev_mac,strlen(devlist[i].dev_mac));
		if(sqlite3_interface("terminal_register_tb","device_mac",devlist[i].dev_mac,"device_mac",dev_mac) != 0)
			continue;
		//PRINT("dev_mac=%s\n",dev_mac);
		strtobig(dev_mac,strlen(dev_mac));
		if(!strncmp(dev_mac,devlist[i].dev_mac,strlen(devlist[i].dev_mac)))
		{
			//PRINT("dev is registered!\n");
			if(sqlite3_interface("terminal_register_tb","device_name",devlist[i].dev_mac,"device_mac",device_name) != 0)
				continue;
			if(strcmp(device_name,devlist[i].dev_name))
			{
				memset(devlist[i].dev_name,' ',16);
				memcpy(devlist[i].dev_name,device_name,16);
				for(j=0;j<16;j++)
				{
					if(devlist[i].dev_name[j] == '\0')
					{
						devlist[i].dev_name[j]=' ';
					}
				}
				//PRINT("dev_name = %s\n",devlist[i].dev_name);
				//PRINT("device_name = %s\n",device_name);
				//netWrite(dev->client_fd,"HEADR0010REG_SUC000\r\n",21);
			}
			continue;
		}

#ifndef B6L		
		if(sqlite3_interface("terminal_base_tb","register_state","0","register_state",register_state) != 0)
  			continue;
		//PRINT("register_state = %s\n",register_state);
		if(sqlite3_interface("terminal_base_tb","pad_mac",devlist[i].dev_mac,"pad_mac",pad_mac) != 0)
		 	continue;
		strtobig(pad_mac,strlen(pad_mac));
	//		PRINT("buf=%s\n",buf);
	//	PRINT("pad_mac = %s\n",pad_mac);
		//PRINT("%d\n",strcmp(pad_mac,buf));
		//PRINT("%d\n",strcmp(register_state,"0"));
		if(!strncmp(pad_mac,devlist[i].dev_mac,strlen(devlist[i].dev_mac)) && !strcmp(register_state,"0"))
		{
			//PRINT("pad is registered!\n");
			continue;
		}
#endif
		PRINT("dev is unregistered\n");
#ifdef B6
		netWrite(devlist[i].client_fd,"HEADR0012REGFAIL002B6\r\n",23);
#elif defined(B6L)	
		netWrite(devlist[i].client_fd,"HEADR0013REGFAIL003B6L\r\n",24);
#elif defined(S1) || defined(S1_F3A)
		netWrite(devlist[i].client_fd,"HEADR0012REGFAIL002S1\r\n",23);
#else
		netWrite(devlist[i].client_fd,"HEADR0010REGFAIL000\r\n",21);
#endif	
		memset(insidebuf,0,10);
		sprintf(insidebuf,"%s","INSIDE");
		insidebuf[6]=i+1;
		netWrite(phone_control_fd[0],insidebuf, 7);
	}
	return 0;
}
#endif

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
		//zb
		if(tick_count == TICK_INTERVAL)
		{
			PRINT("up tick\n");
			tick_count = 0;
            generate_up_msg();
		}
		if(tick_count%2 == 0)
		{
#ifdef REGISTER
			check_register_status();
#endif
		}
		tick_count ++;
		for(i=0;i<CLIENT_NUM;i++)
		{
			if(devlist[i].client_fd == -1)
				continue;
			if(devlist[i].old_tick_time == devlist[i].tick_time)
			{
				if(devlist[i].destroy_count >= TICK_INTERVAL)
				{
					PRINT("dev%d.destroy_count = %d\n",i,devlist[i].destroy_count);
				}
				devlist[i].destroy_count ++;
			}
			else
			{
				devlist[i].destroy_count = 0;
				devlist[i].old_tick_time = devlist[i].tick_time;
			}
			if(devlist[i].destroy_count >= (TICK_INTERVAL+30))
			{
				PRINT("no tick!!!\n");
				devlist[i].dying = 1;
				memset(insidebuf,0,SENDBUF);
				sprintf(sendbuf,"%s","INSIDE");
				sendbuf[6]=i+1;
				netWrite(phone_control_fd[0],sendbuf, 7);
				devlist[i].destroy_count = 0;
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
		usleep(1000*1000);
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
						//INSIDE表示注册状态，inside表示未注册状态
						if(strncmp(msg+7*i,"INSIDE",6) && strncmp(msg+7*i,"inside",6))
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
	//ioctl(phone_audio.phone_audio_pcmfd,SLIC_RELEASE,0);
	//ioctl(phone_audio.phone_audio_pcmfd,SLIC_INIT,0);
	if(phone_audio.phone_audio_pcmfd != -1)
	{
		close(phone_audio.phone_audio_pcmfd);
		phone_audio.phone_audio_pcmfd = -1;
	}
	usleep(100*1000);
	if(phone_audio.phone_audio_pcmfd == -1)
	{
		int pcm_fd=open(PCMNAME,O_RDWR);
		if(pcm_fd<0)
		{
			perror("open pcm fail,again!\n");
			pcm_fd=open(PCMNAME,O_RDWR);
			if(pcm_fd<0)
				return -1;
		}
		PRINT("audio open_success\n");

		phone_audio.phone_audio_pcmfd = pcm_fd;
	}
	usleep(200*1000);

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
	ioctl(phone_audio.phone_audio_pcmfd,SLIC_RELEASE,0);
	start_read_incoming();
	return ret;
}

#ifdef B6
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
			//a5 5a 03 03  主叫
			//a5 5a 03 04  led状态
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
					PRINT("CALL_TEST!!\n");
					if(call_test()==0)
						write(phone_control.passage_fd,successbuf,6);
					else
						write(phone_control.passage_fd,failbuf,6);
				}
				//获取灯状态
				if(recvbuf[2] == (char)0x03 && recvbuf[3] == (char)0x04)
				{
					PRINT("GET LED STATUS\n");
					if(phone_control.vloop > 3 || phone_control.vloop < -3)
					{
						if(phone_control.global_phone_is_using == 1)
							led_control(LED_OFFHOOK_IN);
						else if(phone_control.global_incoming == 1)
							led_control(LED_INCOMING);
						else
							led_control(LED_LINE_IN);
					}
					else
						led_control(LED_LINE_OUT);
				}
			}
		}
		usleep(100*1000);
	}
}
#else
void led_control(char type)
{
	switch(type)
	{
		case LED2_ON:
			write(phone_control.led_control_fd,"led2_on",7);
			break;
		case LED2_OFF:
			write(phone_control.led_control_fd,"led2_off",8);
			break;
		case LED_LINE_IN:
			write(phone_control.led_control_fd,"led1_off",8);
			break;
		case LED_LINE_OUT:
			write(phone_control.led_control_fd,"led1_on",7);
			break;		
	}
}

void led_control_pthread_func(void *argv)
{
	int incoming_on_times = 0;
	int offhook_times = 1;
	int led1_on = 0;
	while(1)
	{
		if(phone_control.global_incoming == 1)
		{
			incoming_on_times++;
		}
		else
		{
			incoming_on_times = 1;
		}
		if(incoming_on_times%3 == 0)
		{
			if(led1_on == 1)
			{
				led_control(LED2_OFF);
				led1_on = 0;
			}
			else
			{
				led_control(LED2_ON);
				led1_on = 1;
			}
		}
		if(phone_control.global_phone_is_using == 1)
		{
			offhook_times++;
		}
		else
		{
			offhook_times = 1;
		}
		if(offhook_times%7 == 0)
		{
			if(led1_on == 1)
			{
				led_control(LED2_OFF);
				led1_on = 0;
			}
			else
			{
				led_control(LED2_ON);
				led1_on = 1;
			}
		}
		usleep(200*1000);
	}
}
#endif

void *pthread_update(void *para)
{
	update(PHONE_APP_NAME,PHONE_APP_DES,PHONE_APP_CODE,PHONE_APP_VERSION,60*60,&phone_control.phone_busy);
}
