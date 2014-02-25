#include "modules_server.h"
#include "as532.h"

int global_server_fd;
int global_uart_fd;
Passage passage_list[PASSAGE_NUM] = {
	{-1,(char)TYPE_USB_PASSAGE,USBPASSAGE},
	{-1,(char)TYPE_UART_PASSAGE,UARTPASSAGE},
	{-1,(char)TYPE_SPI_PASSAGE,SPIPASSAGE},
	{-1,(char)TYPE_PHONE_PASSAGE,PHONEPASSAGE},
};
//0--usb,1--uart,2--spi,3--phone

int global_sg_fd;
unsigned char uart_recv_buffer[BUFFER_SIZE_1K*4];
int uart_wp_pos = 0;
int uart_rd_pos = 0;
int stm32_ver_req = 0;
int stm32_ver_des_req = 0;
int stm32_ver_req_times = 0;
int stm32_ver_des_req_times = 0;
short global_port;
char global_ip[20];
int stm32_updating = 0;
int as532_updating = 0;
int factory_test_stm32 = 0;
struct UKey *global_pukey;
struct itimerval value;
const char hex_to_asc_table[16] = {0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
	0x38,0x39,0x41,0x42,0x43,0x44,0x45,0x46};
	
const char soft_version[4]={
	//phone_control
	0x00,0x00,0x00,0x01,
};

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
//输出一个数组的内容，以十六进制模式
void ComFunPrintfBuffer(unsigned char *pbuffer,unsigned char len)
{
	char *pstr;
	pstr = (char *)malloc(512);
	if(pstr == NULL)
		return;
	ComFunChangeHexBufferToAsc(pbuffer,len,pstr);

	printf("%s\r\n",pstr);

	free((void *)pstr);
}

void check_stm32_ver_req_func()
{
	if(stm32_ver_req == 1)
	{
		if(stm32_ver_req_times == 20)
		{
			PRINT("stm32_ver_req timeout\n");
			if(factory_test_stm32 == 1)
			{
				int ret = 0;
				char databuf[BUFFER_LEN] = {0};
				//char sendbuf[BUFFER_SIZE_4K] = {0};
				//09超时设备无响应
				ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_STM32_1,FACTORY_TEST_CMD_STM32_VER,FAIL,0x09,NULL,0);
				generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
			}
			stm32_ver_req = 0;
			stm32_ver_req_times = 0;
			factory_test_stm32 = 0;
			
		}
		stm32_ver_req_times ++;
	}
	else
	{
		stm32_ver_req_times = 0;
	}
}

void check_stm32_ver_des_req_func()
{
	if(stm32_ver_des_req == 1)
	{
		if(stm32_ver_des_req_times == 20)
		{
			PRINT("stm32_ver_des_req timeout\n");
			
			stm32_ver_des_req = 0;
			stm32_ver_des_req_times = 0;
		}
		stm32_ver_des_req_times ++;
	}
	else
	{
		stm32_ver_des_req_times = 0;
	}
}

//定时器处理函数
void timer_do(int signo)
{
	switch (signo)
	{
		case SIGALRM:
			check_stm32_ver_req_func();
			check_stm32_ver_des_req_func();
			signal(SIGALRM, timer_do);
			break;
	}

	return;
}

void init_env(void)
{
	int sockfd,on,i;
	
	struct sockaddr_in servaddr,cliaddr;

	sockfd = socket(AF_INET,SOCK_DGRAM,0);;
	if(sockfd<=0)
	{
		perror("create socket fail\n");
		exit(-1);
	}
	PRINT("socket success\n");

	on = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERVER_PORT);

	if(bind(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr)) == -1)
	{
		perror("bind error");
		exit(-1);
	}
	PRINT("bind success\n");
	
	global_server_fd = sockfd;
	
	global_pukey = (struct UKey *)alloc_UKey();
	global_pukey->p_hdr = (struct  sg_io_hdr *)alloc_io_hdr();
		
	global_sg_fd = user_open_usb(SGNAME);
	//PRINT("open sg file fd = %d\n" ,global_sg_fd);
	if (global_sg_fd < 0)
	{
		PRINT("failed to open sg file \n");
		//destroy_key(global_pukey);
		//exit(-1);
    }
    else
    {       
		global_pukey->fd = global_sg_fd;
	}
	
	global_uart_fd = open(UARTNAME,O_RDWR);
	if(global_uart_fd < 0)
	{
		PRINT("open uart error\n");
		//destroy_key(global_pukey);
		//exit(-1);
	}
	else
	{
		serialConfig(global_uart_fd,B115200);
		PRINT("open uart success\n");
	}
	
	for(i=0;i<PASSAGE_NUM;i++)
	{
		passage_list[i].fd = open(passage_list[i].passage_name,O_RDWR);
		if(passage_list[i].fd < 0)
		{
			PRINT("open %s error\n",passage_list[i].passage_name);
			//destroy_key(global_pukey);
			//exit(-1);
		}
		else
		{
			PRINT("open %s success\n",passage_list[i].passage_name);
		}
	}
	
	//启动定时器
	signal(SIGALRM, timer_do);
	value.it_value.tv_sec = 0;
	value.it_value.tv_usec = 50*1000;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = 50*1000;
	setitimer(ITIMER_REAL, &value, NULL);

}
int generate_stm32_down_msg(char *databuf,int databuf_len,int passage)
{
	//memset(sendbuf,0,BUFFER_SIZE_4K);
	int index = 0;
	if(databuf_len > 4000)
		databuf_len = 4000;
	char *sendbuf = malloc(databuf_len+16);
	if(sendbuf == NULL)
	{
		PRINT("malloc error\n");
		return -1;
	}
	PRINT("malloc size : %d\n",databuf_len+16);
	//int ret = 0;
	sendbuf[index++] = STM32_DOWN_HEAD_BYTE1_1;
	sendbuf[index++] = STM32_DOWN_HEAD_BYTE2_1;
	sendbuf[index++] = STM32_DOWN_HEAD_BYTE3_1;
	sendbuf[index++] = STM32_DOWN_HEAD_BYTE4_1;
	sendbuf[index++] = (((databuf_len+1) >> 8) & 0XFF);
	sendbuf[index++] = ((databuf_len+1) & 0XFF);
	sendbuf[index++] = (passage&0xFF); //通道
	//sendbuf[index++] = STM32_DOWN_HEAD_BYTE1_2;
	//sendbuf[index++] = STM32_DOWN_HEAD_BYTE2_2;
	//sendbuf[index++] = STM32_DOWN_HEAD_BYTE3_2;
	//sendbuf[index++] = STM32_DOWN_HEAD_BYTE4_2;
	//sendbuf[index++] = ((databuf_len >> 8) & 0XFF);
	//sendbuf[index++] = (databuf_len & 0XFF);
	memcpy(&sendbuf[index],databuf,databuf_len);
	index += databuf_len;
	//sendbuf[index++] = sumxor(&sendbuf[PACKET_STM32_HEAD_POS],6+databuf_len);
	sendbuf[index++] = sumxor(sendbuf,7+databuf_len);
	if(global_uart_fd < 0)
	{
		free(sendbuf);
		return -1;
	}
	else
	{
		if(write(global_uart_fd,sendbuf,index) != index)
		{
			free(sendbuf);
			return -1;
		}
		PRINT("send to stm32:\t");
		ComFunPrintfBuffer(sendbuf,index);
	}
	//if(passage == 0x20)
		//printf("USB:\t");
	//else if(passage == 0x30)
		//printf("UART:\t");
	//else if(passage == 0x40)
		//printf("SPI:\t");
	//ComFunPrintfBuffer(sendbuf,index);
	free(sendbuf);
	return 0;
}

int do_cmd_532_show(char *sendbuf)
{
	init_key(global_pukey);
	int ret = 0;
	ret = lcd(global_pukey,&sendbuf[5]);
	return ret;
}

int do_cmd_532_ver(char *outbuf,int *len)
{
	int ret = 0;
	init_key(global_pukey);
	ret = version(global_pukey,outbuf,len);
	ComFunPrintfBuffer(outbuf,*len);
	return ret;
}

int do_cmd_532_ver_des(char *outbuf,int *len)
{
	int ret = 0;
	init_key(global_pukey);
	ret = version_detail(global_pukey,outbuf,len);
	ComFunPrintfBuffer(outbuf,*len);
	return ret;
}

int do_cmd_stm32_ver()
{
	char data = (char)CMD_STM32_VER;
	int ret = 0;
	ret = generate_stm32_down_msg(&data,1,TYPE_STM32);
	return ret;
}

int do_cmd_stm32_ver_des()
{
	char data = (char)CMD_STM32_VER_DES;
	int ret = 0;
	ret = generate_stm32_down_msg(&data,1,TYPE_STM32);
	return ret;
}

int do_cmd_stm32_boot()
{	
	char data = (char)CMD_STM32_BOOT;
	int ret = 0;
	ret = generate_stm32_down_msg(&data,1,TYPE_STM32);
	return ret;
}

int do_cmd_stm32_update()
{
	int ret = 0;
	stm32_updating = 1;
	ret = stm32_update();
	if(ret != 0 )
	{
		PRINT("stm32 update error! %d \n",ret);
	}
	else
	{
		PRINT("stm32 update success!\n");
	}
}

int boot_open_usb(char *dev)
{
	int fd = open(dev,O_RDWR);
	return fd;
}

void boot_close_usb(int *fd)
{
	close(*fd);
}

int user_open_usb(char *dev)
{
	int fd = open(dev,O_RDWR);
	return fd;
}

void user_close_usb(int *fd)
{
	close(*fd);
}

int as532_update()
{
	int ret = 0;
	int ret_err = 1;
	int Versionlen_update_len =0;
    unsigned char VersionBuff_update[20] = {0};
	//int Versionlen_after_update =0;
    //unsigned char VersionBuff_after_update[20] = {0};
    
 	init_key(global_pukey);
	version(global_pukey,VersionBuff_update,&Versionlen_update_len);
	
    ret=check_file(VersionBuff_update, DATNAME);
	if(ret == 0)
	{
		PRINT("check file success\n");
	}
	else if(ret == -13)
	{
		PRINT("your 532 is newest\n");
		do_cmd_rep(AS532H_UPDATE,NULL,0,ret);
		ret_err = -8;
		goto AS532_UPDATE_ERR;
	}
	else
	{
		PRINT("check file error\n");
		do_cmd_rep(AS532H_UPDATE,NULL,0,ret);
		ret_err = -9;
		goto AS532_UPDATE_ERR;
	}
	init_key(global_pukey);
	ret = reboot_enter_boot(global_pukey);
	if (ret != 0) 
		PRINT("send reboot_enter_boot data error\n");
	else 
		PRINT("send reboot_enter_boot success\n");  
	usleep(200*1000);
	
	//init_key(global_pukey);
	//ret = reboot(global_pukey);
	//if (ret != 0) 
		//PRINT("send reboot data error\n");
	//else 
		//PRINT("send reboot success\n");  
	user_close_usb(&(global_pukey->fd));
	usleep(1000*1000);
	
	usb_fd = boot_open_usb(USBNAME);
	if(usb_fd <= 0 )
	{
		PRINT("open usb error!\n");
		ret_err = -1; //打开usb失败
		init_key(global_pukey);
		global_pukey->fd = user_open_usb(SGNAME);
		if(global_pukey->fd < 0)
		{
			PRINT("open usb error after 532 updating!!\n");
			ret_err = -7;
			goto AS532_UPDATE_ERR;
		}
		goto AS532_UPDATE_ERR;
	}
	PRINT("open MyModule file fd = 0x%x======\r\n",usb_fd);
	
	usleep(50000);
	
	ret = LoadDatFile();
	if(ret != 1)
	{
		PRINT("update as532 failed,cause load dat file err!\r\n");
		ret_err = -2; //读取文件失败
		goto AS532_UPDATE_ERR;
	}
	
	ret = BootAsDatFileCheck();
	if(ret != 1)
	{
		PRINT("update as532 failed,cause dat file err!\r\n");
		ret_err = -3; //文件校验失败
		goto AS532_UPDATE_ERR;
	}

	ret = BootAsSendIspStart();
	if(ret != 1)
	{
		PRINT("send isp start command,but not get response!\r\n");	
		ret_err = -4; //532应答失败
		goto AS532_UPDATE_ERR;
	}
	usleep(10000);
	ret = BootAsSendConfig();
	if(ret != 1)
	{
		PRINT("send config command,but not get response!\r\n");	
		ret_err = -4; //532应答失败
		goto AS532_UPDATE_ERR;
	}
	ret = BootAsSendData();
	if(ret != 1)
	{
		PRINT("update as532 failed,cause transefer data err!\r\n");
		ret_err = -5; //发送数据失败
		goto AS532_UPDATE_ERR;
	}
	ret = BootAsSendCheck();
	if(ret != 1)
	{
		PRINT("update as532 failed,cause check err!\r\n");
		ret_err = -6; //数据校验失败
		goto AS532_UPDATE_ERR;
	}
	boot_close_usb(&usb_fd);
	PRINT("update as532 Over!!!\r\n");
	usleep(10000*1000);
	init_key(global_pukey);
	global_pukey->fd = user_open_usb(SGNAME);
	if(global_pukey->fd < 0)
	{
		PRINT("open usb error after 532 updating!!\n");
		ret_err = -7;
		goto AS532_UPDATE_ERR;
	}
	//version(global_pukey,VersionBuff_after_update,&Versionlen_after_update);
	//PRINT("before version:\t");
	//ComFunPrintfBuffer(VersionBuff_before_update,Versionlen_before_update);
	//PRINT("after version:\t");
	//ComFunPrintfBuffer(VersionBuff_after_update,Versionlen_after_update);
	//if(VersionBuff_after_update[0] != VersionBuff_before_update[0] || VersionBuff_after_update[1] != VersionBuff_before_update[1] || VersionBuff_after_update[2] <= VersionBuff_before_update[2])
	//{
		//PRINT("version is error,update error\n");
		//ret_err = -8;
		//goto AS532_UPDATE_ERR;
	//}
	as532_updating = 0;
	return as532_updating;
AS532_UPDATE_ERR:
	as532_updating = ret_err;
	return as532_updating;
}

void as532_update_thread_func(void* argv)
{
	int ret = as532_update();
	PRINT("update ret = %d\n",ret);
	
}

int do_cmd_rep(int type,char *data,int data_len,int result)
{
	//AA 55 0B 00 06 00 00 00 00 00 02
	char sendbuf[BUFFER_SIZE_4K]={0};
	int index = 0;
	struct sockaddr_in si;
	si.sin_family = AF_INET;
	inet_pton(AF_INET,global_ip,&si.sin_addr);
	si.sin_port = htons(global_port);

	memset(sendbuf,0,BUFFER_SIZE_4K);
	sendbuf[index++]=(char)PAD_HEAD_BYTE2;
	sendbuf[index++]=(char)PAD_HEAD_BYTE1;
	sendbuf[index++]=(char)type;
	sendbuf[index++]=(((data_len+1)>>8)&&0xff);
	sendbuf[index++]=data_len+1;
	sendbuf[index++]=(char)result;
	if(data != NULL)
	{
		memcpy(&sendbuf[index],data,data_len);
	}
	ComFunPrintfBuffer(sendbuf,data_len+index);
	PRINT("send_ret = %d\n",sendto(global_server_fd,sendbuf,data_len+index,0,(struct sockaddr*)&si,sizeof(si)));
	return 0;
}

int do_cmd_532_update()
{
	as532_updating = 1;
	pthread_t as532_update_pthread;
	pthread_create(&as532_update_pthread,NULL,(void*)as532_update_thread_func,NULL);
	return 0;
}

int parse_msg(char *buf,char *ip,short port)
{
	if(buf[0]!= (char)PAD_HEAD_BYTE1 || buf[1]!= (char)PAD_HEAD_BYTE2)
	{
		PRINT("head error\n");
		return -1;
	}
	int ret = 0;
	char sendbuf[BUFFER_SIZE_4K]={0};
	int len = 0;
	switch(buf[2])
	{
		case AS532H_SHOW:
			PRINT("AS532H_SHOW\n");
			ret = do_cmd_532_show(buf);
			if (ret != 0) 
			{
				PRINT("lcd send data error\n");
				do_cmd_rep(AS532H_SHOW,NULL,0,0x01);
			}
			else 
			{
				PRINT("send lcd success\n");
				do_cmd_rep(AS532H_SHOW,NULL,0,0x00);
			}
			break;
		case AS532H_VER:
			PRINT("AS532H_VER\n");
			ret = do_cmd_532_ver(sendbuf,&len);
			if (ret != 0) 
			{
				PRINT(" send version data error\n");
				do_cmd_rep(AS532H_VER,sendbuf,len,0x01);
			}
			else
			{ 
				PRINT("send version success\n");  
				do_cmd_rep(AS532H_VER,sendbuf,len,0x00);
			}
			break;
		case AS532H_VER_DES:
			PRINT("AS532H_VER_DES\n");
			ret = do_cmd_532_ver_des(sendbuf,&len);
			if (ret != 0) 
			{
				PRINT(" send version data error\n");
				do_cmd_rep(AS532H_VER_DES,sendbuf,len,0x01);
			}
			else
			{ 
				PRINT("send version success\n");  
				do_cmd_rep(AS532H_VER_DES,sendbuf,len,0x00);
			}
			break;		
		case STM32_VER:
			PRINT("STM32_VER\n");
			ret = do_cmd_stm32_ver();
			if(ret != 0)
			{
				do_cmd_rep(STM32_VER,NULL,0,0x01);
			}
			else
			{
				stm32_ver_req = 1;
				do_cmd_rep(STM32_VER,NULL,0,0x00);
			}
			break;
		case STM32_VER_DES:
			PRINT("STM32_VER_DES\n");
			ret = do_cmd_stm32_ver_des();
			if(ret != 0)
			{
				do_cmd_rep(STM32_VER_DES,NULL,0,0x01);
			}
			else
			{
				stm32_ver_des_req = 1;
				do_cmd_rep(STM32_VER_DES,NULL,0,0x00);
			}
			break;
		case AS532H_UPDATE:
			PRINT("AS532H_UPDATE\n");
			do_cmd_532_update();
		default:
			break;
	}
}

void loop_recv(void *argv)
{
	PRINT("loop_recv is  running.......\n");
	char recv_buf[BUFFER_LEN]={0};
	int ret=0;
	int i;
	struct sockaddr_in si;
	int len = 0;
	fd_set socket_fdset;
	struct timeval tv;
	memset(&tv, 0, sizeof(struct timeval));

	while(1)
	{
		int maxfd = -1;
		int tmp_ret = 0;
		FD_ZERO(&socket_fdset);
		
		FD_SET(global_server_fd, &socket_fdset);

		tv.tv_sec =  1;
		tv.tv_usec = 0;

		switch(select(global_server_fd+ 1, &socket_fdset, NULL, NULL, &tv))
		{
			case -1:
			case 0:
				break;
			default:
			{
				if ( FD_ISSET(global_server_fd, &socket_fdset) )
				{
					PRINT("from pad\n");
					memset(recv_buf,0,BUFFER_LEN);
					len = sizeof(si);
					ret = recvfrom(global_server_fd,recv_buf,sizeof(recv_buf),MSG_DONTWAIT,(struct sockaddr*)&si,&len);
					if(ret<=0)
					{
						PRINT("socket recv error");
						break;
					}
					if(as532_updating == 1)
					{
						PRINT("532 is updating\n");
						break;
					}
					PRINT("ret = %d\n",ret);
					inet_ntop(AF_INET,&si.sin_addr,global_ip,sizeof(global_ip));
					global_port = ntohs(si.sin_port);
					PRINT("global_port = %d\n",global_port);
					PRINT("global_ip = %s\n",global_ip);
					ComFunPrintfBuffer(recv_buf,ret);
					parse_msg(recv_buf,global_ip,global_port);
				}
				break;
			}
		}
	}
}

int serialConfig(int serial_fd, speed_t baudrate)
{
	struct termios options;
	tcgetattr(serial_fd, &options);
	options.c_cflag  |= ( CLOCAL | CREAD );//Ignore modem control lines ,Enable receiver
	options.c_cflag &= ~CSIZE; //Character size mask
	options.c_cflag &= ~CRTSCTS;//Enable  RTS/CTS  (hardware)  flow  control *
	options.c_cflag |= CS8;// 8bit *
	options.c_cflag &= ~CSTOPB;//Set two stop bits, rather than one  *
	options.c_iflag |= IGNPAR;//Ignore framing errors and parity errors  *
	options.c_iflag &= ~(ICRNL |IXON);//Enable XON/XOFF flow control on output
	//Translate  carriage  return to newline on input (unless IGNCR is set).

	options.c_oflag = 0;
	options.c_lflag = 0;

	cfsetispeed(&options, baudrate /*B9600*/); //*
	cfsetospeed(&options, baudrate /*B9600*/); //*
	tcsetattr(serial_fd,TCSANOW,&options);//the change occurs immediately

	printf("serial config finished\n");
	return 0;
}

//计算校验
unsigned char sumxor(const  char  *arr, int len)
{
	int i=0;
	unsigned char sum = 0;
	for(i=0; i<len; i++)
	{
		sum ^= arr[i];
	}

	return sum;
}

int UartGetVer(unsigned char *pppacket,int bytes)
{
	//EA FF 5A A5 00 00 10 00 00 DATA XX

	PRINT("UartGetVer is called\r\n");	
	unsigned char databuf[BUFFER_LEN] = {0};
    //unsigned char sendbuf[BUFFER_SIZE_4K] = {0};

	int index = 0;
	int data_len = 0;
	int ret = 0;
	if(stm32_ver_req == 0)
		return 0;
	stm32_ver_req = 0;

	switch(pppacket[PACKET_STM32_REP_POS])
	{
		case 0x00: //成功 	//EA FF 5A A5 00 07 10 00 00 DATA XX
			data_len = pppacket[PACKET_STM32_DATA_LEN_POS_HEIGHT]*256 + pppacket[PACKET_STM32_DATA_LEN_POS_LOW] - 3;
			if(factory_test_stm32 == 1)
			{
				ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_STM32_1,FACTORY_TEST_CMD_STM32_VER,SUCCESS,0x00,pppacket+PACKET_STM32_DATA_POS,data_len);
				//ComFunPrintfBuffer(pppacket+PACKET_STM32_DATA_POS,data_len);
				generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
			}
			else
				do_cmd_rep(STM32_VER,&pppacket[PACKET_STM32_DATA_POS],data_len,0x00);
			break;
		case 0x01: //命令错误
			data_len = pppacket[PACKET_STM32_DATA_LEN_POS_HEIGHT]*256 + pppacket[PACKET_STM32_DATA_LEN_POS_LOW] - 3;
			if(factory_test_stm32 == 1)
			{
				ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_STM32_1,FACTORY_TEST_CMD_STM32_VER,FAIL,0x01,NULL,0);
				generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
			}
			else
				do_cmd_rep(STM32_VER,&pppacket[PACKET_STM32_DATA_POS],data_len,0x01);
			break;
		case 0x02: //校验错误
			data_len = pppacket[PACKET_STM32_DATA_LEN_POS_HEIGHT]*256 + pppacket[PACKET_STM32_DATA_LEN_POS_LOW] - 3;
			if(factory_test_stm32 == 1)
			{
				ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_STM32_1,FACTORY_TEST_CMD_STM32_VER,FAIL,0x02,NULL,0);
				generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
			}
			else
				do_cmd_rep(STM32_VER,&pppacket[PACKET_STM32_DATA_POS],data_len,0x02);
			break;
		default:
			break;
	}
	stm32_ver_req = 0;
	factory_test_stm32 = 0;
	return 0;
}
int UartGetVerDes(unsigned char *pppacket,int bytes)
{
	PRINT("UartGetVerDes is called\r\n");
	int index = 0;
	int data_len = 0;
	if(stm32_ver_des_req == 0)
		return 0;
	stm32_ver_des_req = 0;

	switch(pppacket[PACKET_STM32_REP_POS])
	{
		//EA FF 5A A5 00 07 10 00 00 00 00 00 02 FF

		case 0x00: //成功 FE EF A5 5A 00 05 01 00 01 01 01 XX
			data_len = pppacket[PACKET_STM32_DATA_LEN_POS_HEIGHT]*256 + pppacket[PACKET_STM32_DATA_LEN_POS_LOW] - 3;
			do_cmd_rep(STM32_VER_DES,&pppacket[PACKET_STM32_DATA_POS],data_len,0x00);
			break;
		case 0x01: //命令错误
			data_len = pppacket[PACKET_STM32_DATA_LEN_POS_HEIGHT]*256 + pppacket[PACKET_STM32_DATA_LEN_POS_LOW] - 3;
			do_cmd_rep(STM32_VER_DES,&pppacket[PACKET_STM32_DATA_POS],data_len,0x01);
			break;
		case 0x02: //校验错误
			data_len = pppacket[PACKET_STM32_DATA_LEN_POS_HEIGHT]*256 + pppacket[PACKET_STM32_DATA_LEN_POS_LOW] - 3;
			do_cmd_rep(STM32_VER_DES,&pppacket[PACKET_STM32_DATA_POS],data_len,0x02);
			break;
		default:
			break;
	}
	stm32_ver_des_req = 0;
	return 0;
}
int UartCallBoot(unsigned char *pppacket,int bytes)
{
	PRINT("UartGetVerDes is called\r\n");
	return 0;
}

int UartStm32Do(unsigned char *ppacket,int bytes)
{
	//EA FF 5A A5 00 00 10 00 00 DATA XX
	//unsigned char *pppacket = &ppacket[PACKET_STM32_HEAD_POS];
	//int data_bytes = bytes - 8; 
	switch(ppacket[PACKET_CMD_TYPE])
	{
		case CMD_STM32_VER:
			UartGetVer(ppacket,bytes);
			break;
		case CMD_STM32_VER_DES:
			UartGetVerDes(ppacket,bytes);
			break;
		case CMD_STM32_BOOT:
			UartCallBoot(ppacket,bytes);
			break;
		default:
			break;
	}
	return 0;
}
int UartTestDo(unsigned char *ppacket,int bytes)
{
	return 0;
}
int write_passage(char type,unsigned char *packet,int bytes)
{
	PRINT("%s\n",__FUNCTION__);
	int i = 0;
	for(i=0;i<PASSAGE_NUM;i++)
	{
		if(passage_list[i].type == type)
		{
			PRINT("%s\n",passage_list[i].passage_name);
			if(write(passage_list[i].fd,packet,bytes) != bytes)
			{
				PRINT("%s error\n",__FUNCTION__);
			}
		}
	}
	return 0;
}

int UartPassageDo(char type,unsigned char *ppacket,int bytes)
{
	unsigned char *pppacket = &ppacket[PACKET_STM32_HEAD_POS];
	int data_bytes = bytes - 8; 
	write_passage(type,pppacket,data_bytes);
	return 0;
}

int generate_test_up_msg(char *sendbuf,char cmd1,char cmd2,char result_type,char err_code,char *result,int result_len)
{
	memset(sendbuf,0,BUFFER_LEN);
	int index = 0;
	//int ret = 0;
	sendbuf[index++] = FACTORY_TEST_HEAD_BYTE1;
	sendbuf[index++] = FACTORY_TEST_HEAD_BYTE2;
	sendbuf[index++] = FACTORY_TEST_HEAD_BYTE3;
	sendbuf[index++] = FACTORY_TEST_HEAD_BYTE4;
	if(result_type == FAIL )
	{
		sendbuf[index++] = (((result_len+4) >> 8) & 0XFF);
		sendbuf[index++] = ((result_len+4) & 0XFF);
	}
	else if(result_type == SUCCESS)
	{
		sendbuf[index++] = (((result_len+3) >> 8) & 0XFF);
		sendbuf[index++] = ((result_len+3) & 0XFF);
	}
	sendbuf[index++] = cmd1;
	sendbuf[index++] = cmd2;
	sendbuf[index++] = result_type;
	if(result_type == FAIL )
	{
		sendbuf[index++] = err_code;
	}
	if(result != NULL)
	{
		memcpy(&sendbuf[index],result,result_len);
		index += result_len;
	}
	if(result_type == FAIL )
	{
		sendbuf[index++] = sumxor(sendbuf+4,6+result_len);
	}
	else if(result_type == SUCCESS)
	{
		sendbuf[index++] = sumxor(sendbuf+4,5+result_len);
	}
	return index;
}

int start_test()
{
	//int f = fork();
	//if(f > 0)
	//{
		//PRINT("%s\n",__FUNCTION__);
	//}
	//else if(f == 0)
	//{
		//if(system(FACTORY_TEST_PROGRAM)!= -1)
		//{
			//PRINT("start test success\n");
		//}
	//}
}

int factory_test_cmd_as532_ver()
{
	int Versionlen =0;
	int ret = 0;
    unsigned char VersionBuff[20] = {0};
    unsigned char databuf[BUFFER_LEN] = {0};
    //unsigned char sendbuf[BUFFER_SIZE_4K] = {0};
	init_key(global_pukey);
	ret = version(global_pukey,VersionBuff,&Versionlen);
	ComFunPrintfBuffer(VersionBuff,Versionlen);
	if (ret != 0) 
	{
		PRINT(" send version data error\n");
		ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_AS532_1,FACTORY_TEST_CMD_AS532_VER,FAIL,/*(char)ret*/1,VersionBuff,0);
		generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
	}
	else
	{ 
		PRINT("send version success\n");  
		ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_AS532_1,FACTORY_TEST_CMD_AS532_VER,SUCCESS,0,VersionBuff,Versionlen);
		generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
	}
	return 0;
}

int factory_test_cmd_stm32_ver()
{
	//char sendbuf[BUFFER_SIZE_4K]={0};
	char data = (char)CMD_STM32_VER;
	generate_stm32_down_msg(&data,1,TYPE_STM32);
	factory_test_stm32 = 1;
	stm32_ver_req = 1;
	return 0;
}

int factory_test_cmd_9344_ver()
{
	int ret = 0;
    unsigned char databuf[BUFFER_LEN] = {0};
    //unsigned char sendbuf[BUFFER_SIZE_4K] = {0};
	ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_9344_1,FACTORY_TEST_CMD_9344_VER,SUCCESS,0,soft_version,4);
	generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
	return 0;
}

int factory_test_cmd_9344_led1()
{
	char buf[2] = {0x83,0x01};
	int fd = open("/dev/wifiled",O_RDWR);
	if(fd < 0)
	{
		printf("open error\n");
		return -1;
	}
	printf("open success\n");
	write(fd,"enable",6);
	close(fd);
	//pstn led
	generate_stm32_down_msg(buf,2,TYPE_STM32);
	return 0;
}

int factory_test_cmd_9344_led2()
{
	char buf[2] = {0x83,0x10};
	int fd = open("/dev/wifiled",O_RDWR);
	if(fd < 0)
	{
		printf("open error\n");
		return -1;
	}
	printf("open success\n");
	write(fd,"disable",7);
	close(fd);
	//pstn led
	generate_stm32_down_msg(buf,2,TYPE_STM32);
	return 0;
}

int factory_test_down_control(char cmd)
{
	switch(cmd)
	{
		case FACTORY_TEST_CMD_CONTROL_START:
			PRINT("FACTORY_TEST_CMD_CONTROL_START\n");
			//start_test();
			break;
		case FACTORY_TEST_CMD_CONTROL_STOP:
			PRINT("FACTORY_TEST_CMD_CONTROL_STOP\n");
			break;
		default:
			break;
	}
}

int factory_test_down_stm32(char cmd)
{
	//char sendbuf[2]={0,0};
	int index = 0;
	switch(cmd)
	{
		case FACTORY_TEST_CMD_STM32_VER:
			factory_test_cmd_stm32_ver();
			PRINT("FACTORY_TEST_CMD_STM32_VER\n");
			break;
		case FACTORY_TEST_CMD_STM32_PIC:
			PRINT("FACTORY_TEST_CMD_STM32_PIC\n");
			break;
		case FACTORY_TEST_CMD_STM32_R54:
			PRINT("FACTORY_TEST_CMD_STM32_R54\n");
			break;
		default:
			break;
	}
}

int factory_test_down_as532(char cmd)
{
	//char sendbuf[2]={0,0};
	int index = 0;
	switch(cmd)
	{
		case FACTORY_TEST_CMD_AS532_VER:
			PRINT("FACTORY_TEST_CMD_AS532_VER\n");
			factory_test_cmd_as532_ver();
			break;
		case FACTORY_TEST_CMD_AS532_TEST:
			PRINT("FACTORY_TEST_CMD_AS532_TEST\n");
			break;
		default:
			break;
	}
}

int factory_test_down_9344(char cmd)
{
	char sendbuf[2]={0,0};
	int index = 0;
	switch(cmd)
	{
		case FACTORY_TEST_CMD_9344_VER:
			PRINT("FACTORY_TEST_CMD_9344_VER\n");
			factory_test_cmd_9344_ver();
			break;
		case FACTORY_TEST_CMD_9344_LED1:
			PRINT("FACTORY_TEST_CMD_9344_LED1\n");
			//factory_test_cmd_9344_led1();
			break;
		case FACTORY_TEST_CMD_9344_LED2:
			PRINT("FACTORY_TEST_CMD_9344_LED2\n");
			//factory_test_cmd_9344_led2();
			break;
		case FACTORY_TEST_CMD_9344_CALL:
		case FACTORY_TEST_CMD_9344_CALLED:			
			sendbuf[index++]=0xA5;
			sendbuf[index++]=0X5A;
			sendbuf[index++]=FACTORY_TEST_CMD_DOWN_9344_1;
			sendbuf[index++]=cmd;
			write_passage((char)TYPE_PHONE_PASSAGE,sendbuf,index);
			break;
		default:
			break;
	}
}

int factory_test(unsigned char *packet,int bytes)
{
	//FA EF AD EA 00 02 00 0A XX
	if(bytes < FACTORY_TEST_CMD_LENGTH)
		return -1;
	if(packet[0] != FACTORY_TEST_HEAD_BYTE1 ||  packet[1] != FACTORY_TEST_HEAD_BYTE2 || packet[2] != FACTORY_TEST_HEAD_BYTE3 || packet[3] != FACTORY_TEST_HEAD_BYTE4)
	{
		return -1;
	}
	int data_len = packet[4]*256+packet[5];
	PRINT("data_len = %d\n",data_len);
	//FA EF AD EA 00 02 02 00 00
	if(packet[data_len+6] != sumxor(packet+4,2+data_len))
	{
		PRINT("xor err\n");
		return -1;
	}
	switch(packet[FACTORY_TEST_CMD_1_POS])
	{
		case FACTORY_TEST_CMD_DOWN_CONTROL_1:
			factory_test_down_control(packet[FACTORY_TEST_CMD_2_POS]);
			break;
		case FACTORY_TEST_CMD_DOWN_STM32_1:
			factory_test_down_stm32(packet[FACTORY_TEST_CMD_2_POS]);
			break;
		case FACTORY_TEST_CMD_DOWN_AS532_1:
			factory_test_down_as532(packet[FACTORY_TEST_CMD_2_POS]);
			break;		
		case FACTORY_TEST_CMD_DOWN_9344_1:
			factory_test_down_9344(packet[FACTORY_TEST_CMD_2_POS]);
			break;
		default:
			PRINT("cmd undef\n");
			break;
	}
	return 0;
}

int UartPacketDis(unsigned char *ppacket,int bytes)
{
	int rtn = 0;
	//EA FF 5A A5 00 00 10 00 00 DATA XX
	//EA FF 5A A5 00 00 10
	PRINT("Command Packet data::\r\n");
	ComFunPrintfBuffer(ppacket,bytes);
	if(ppacket[0]!=(unsigned char)STM32_UP_HEAD_BYTE1_1 || ppacket[1]!=(unsigned char)STM32_UP_HEAD_BYTE2_1 || ppacket[2]!=(unsigned char)STM32_UP_HEAD_BYTE3_1 || ppacket[3]!=(unsigned char)STM32_UP_HEAD_BYTE4_1) 
	{
		PRINT("wrong data\n");
		return -1;
	}
	int uart,len_tmp=0;
	int xor=0;
	unsigned char *bufp = ppacket;
	len_tmp = ((ppacket[4]) << 8);
	len_tmp += ppacket[5];
	PRINT("len_tmp=%d\n",len_tmp);
	xor=sumxor(ppacket,bytes-1);
	//PRINT("xor=0x%x\n",xor);
	if(ppacket[len_tmp+6]!=(unsigned char)xor)
	{
		PRINT("xor error\n");
		return -1;
	}

	switch(ppacket[PACKET_DIR_INDEX])
	{
		case 0x10:
			PRINT("UartStm32\r\n");
			rtn = UartStm32Do(ppacket,bytes);
			break;
		case 0x20:
			PRINT("UsbPassage\r\n");
			if(factory_test(ppacket+7,len_tmp-1) == -1)
			{
				rtn = UartPassageDo(TYPE_UART_PASSAGE,ppacket,bytes);
			}
			break;
		case 0x30:
			PRINT("UartPassage\r\n");
			rtn = UartPassageDo(TYPE_UART_PASSAGE,ppacket,bytes);
			break;
		case 0x40:
			PRINT("SpiPassage\r\n");
			rtn = UartPassageDo(TYPE_SPI_PASSAGE,ppacket,bytes);
			break;
		case 0x50:
			PRINT("UartTest\r\n");
			rtn = UartTestDo(ppacket,bytes);
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
	valid_len = uart_wp_pos - uart_rd_pos;

	if(valid_len < MIN_PACKET_BYTES)
		return 0;
	pp = &uart_recv_buffer[uart_rd_pos];
	get_head = 0;
	for(index = 0;index < (valid_len - 3);index++)
	{
		if((pp[index] == STM32_UP_HEAD_BYTE1_1) && (pp[index + 1] == STM32_UP_HEAD_BYTE2_1) && (pp[index + 2] == STM32_UP_HEAD_BYTE3_1) && (pp[index + 3] == STM32_UP_HEAD_BYTE4_1))
		{
			get_head = 1;
			if(index != 0)
			{
				uart_rd_pos += index;
			}
			return &pp[index];
		}
	}
	return 0;
}

int UartPacketRcv(unsigned char *des_packet_buffer,int *packet_size)
{
	int valid_len;
	unsigned char *phead;
	unsigned char *pwp;
	int index = 0;
	int my_packet_bytes;

	phead = PacketSearchHead();
	if(phead == 0)
		return 0;
	pwp = (unsigned char *)(&uart_recv_buffer[uart_wp_pos]);

	valid_len = pwp - phead;
	if(valid_len < MIN_PACKET_BYTES)
	{
		return 0;
	}
	my_packet_bytes = phead[PACKET_HEAD_BYTES] * 256 + phead[PACKET_HEAD_BYTES +
		1];
	my_packet_bytes += PACKET_ADDITIONAL_BYTES;

	if(valid_len >= my_packet_bytes)
	{
		memcpy((void *)(des_packet_buffer),(void *)(phead),my_packet_bytes);
		*packet_size = my_packet_bytes;
		uart_rd_pos += my_packet_bytes;
		return 1;
	}
	return 0;
}

void uart_loop_recv(void * argv)
{
	PRINT("%s running.......\n",__FUNCTION__);
	int read_ret;
	int t1,t2;
	int rtn;
	unsigned char packet_buffer[BUFFER_SIZE_1K*4] = {0};
	int packet_bytes;

	while(1)
	{
		read_ret = 0;
		//维护读写位置
		if(uart_wp_pos == uart_rd_pos)
		{
			uart_wp_pos = 0;
			uart_rd_pos = 0;
		}
		if(sizeof(uart_recv_buffer) - uart_wp_pos < 512)//即将到缓冲尾部，则前移

		{
			t1 = uart_wp_pos - uart_rd_pos;
			if(t1 > 0)//有未处理的数据，则将该数据前移，并重置写入和读取位置

			{
				for(t2 = 0;t2 < t1;t2++)
					uart_recv_buffer[t2] = uart_recv_buffer[uart_rd_pos + t2];
				uart_rd_pos = 0;
				uart_wp_pos = t1;
			}
		}
		//stm32正在升级
		if(stm32_updating == 0)
		{
			if(global_uart_fd >= 0)
			{
				read_ret = read(global_uart_fd,(unsigned char*)(&uart_recv_buffer[uart_wp_pos]),BUFFER_SIZE_1K*4);
			}
		}
		if(read_ret > 0)//有数据读取到
		{
			//PRINT("from Uart\n");
			uart_wp_pos += read_ret;
		}
		else//错误
		{
			//   PRINT("Read Uart error\n");
			usleep(20*1000);
			continue;
		}
		do
		{
			//接收完整包
			rtn = UartPacketRcv(packet_buffer,&packet_bytes);
			if(rtn == 1)
			{
				//对完整包内的命令进行解析执行
				rtn = UartPacketDis(packet_buffer,packet_bytes);
			}
			else
				break;
		}while(1);
		usleep(20*1000);
	}
}

void passage_thread_func(void *argv)
{
	PRINT("%s running.......\n",__FUNCTION__);
	fd_set fdset;
	int i,j,maxfd = -1;
	int ret = 0;
	struct timeval tv;
	memset(&tv, 0, sizeof(struct timeval));
	char sendbuf[BUFFER_SIZE_4K]={0};
	char recvbuf[BUFFER_SIZE_4K]={0};
	char tel_num[64] = {0};
	int tel_len = 0;
	while(1)
	{
		maxfd = -1;
		FD_ZERO(&fdset);
		for(i=0; i<PASSAGE_NUM; i++)
		{
			if(passage_list[i].fd < 0)
				continue;
			FD_SET(passage_list[i].fd, &fdset);
			maxfd = MAX(maxfd, passage_list[i].fd);
		}

		tv.tv_sec =  1;
		tv.tv_usec = 0;

		switch(select(maxfd+ 1, &fdset, NULL, NULL, &tv))
		{
			case -1:
			case 0:
				break;
			default:
				for(i=0; i<PASSAGE_NUM; i++)
				{
					if( FD_ISSET(passage_list[i].fd,&fdset) )
					{
						PRINT("i = %d\n",i);
						//memset(sendbuf,0,BUFFER_SIZE_4K);
						memset(recvbuf,0,BUFFER_SIZE_4K);
						ret = read(passage_list[i].fd,recvbuf,BUFFER_SIZE_4K);
						if(ret > 0)
						{
							PRINT("Read Passage : ");
							ComFunPrintfBuffer(recvbuf,ret);
							//来自电话通道
							if(!strcmp(passage_list[i].passage_name,PHONEPASSAGE))
							{
								for(j = 0;j<ret;j++)
								{
									//led -- stm32
									if(recvbuf[j] == (char)0x5A && recvbuf[j+1] == (char)0XA5)
									{
										recvbuf[j] = (char)CMD_STM32_LED;
										recvbuf[j+1] = recvbuf[j+2];
										generate_stm32_down_msg(&recvbuf[j],2,TYPE_STM32);
									}
									//test -- usb 5A A5 02 A5 5A 83 04 00 00 04 38 31 36 31
									if(recvbuf[j] == (char)0xA5 && recvbuf[j+1] == (char)0X5A)
									{
										if(recvbuf[j+3] == FACTORY_TEST_CMD_9344_CALL)
										{
											int send_ret = generate_test_up_msg(sendbuf,FACTORY_TEST_CMD_UP_9344_1,recvbuf[j+3],recvbuf[j+4],recvbuf[j+5],NULL,0);
											generate_stm32_down_msg(sendbuf,send_ret,TYPE_USB_PASSAGE);
										}
										else if(recvbuf[j+3] == FACTORY_TEST_CMD_9344_CALLED)
										{
											int send_ret = generate_test_up_msg(sendbuf,FACTORY_TEST_CMD_UP_9344_1,recvbuf[j+3],recvbuf[j+4],recvbuf[j+5],&recvbuf[j+7],recvbuf[j+6]);
											generate_stm32_down_msg(sendbuf,send_ret,TYPE_USB_PASSAGE);
										}
									}
								}
							}
							//其他通道
							else
								generate_stm32_down_msg(recvbuf,ret,passage_list[i].type);
						}
					}
				}
				break;
		}
		usleep(50*1000);
	}
}
