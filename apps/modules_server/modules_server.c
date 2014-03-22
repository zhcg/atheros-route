#include "modules_server.h"
#include "as532.h"
#include "stm32_update.h"

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
int global_read_fifo_fd;
int global_write_fifo_fd;
unsigned char fifo_recv_buffer[BUFFER_SIZE_1K] = {0};
int fifo_wp_pos = 0;
int fifo_rp_pos = 0;
int ota_stm32_ver_flag = 0;
unsigned char uart_recv_buffer[BUFFER_SIZE_1K*4] = {0};
int uart_wp_pos = 0;
int uart_rd_pos = 0;
short global_port;
char global_ip[20];
int stm32_updating = 0;
int as532_updating = 0;
int start_factory_test = 0;
struct UKey *global_pukey;
char last_led_status[2]={0};

unsigned char factory_test_buffer[BUFFER_SIZE_1K] = {0};
int factory_test_wp = 0;
int factory_test_rp = 0;

int udp_get_stm32_ver = 0;
int udp_get_stm32_ver_des = 0;

const char hex_to_asc_table[16] = {0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
	0x38,0x39,0x41,0x42,0x43,0x44,0x45,0x46};
	
const char soft_version[4]={
	//phone_control
	0x00,0x00,0x00,0x01,
};
char offhook[] = {0xa5,0x5a,0x41,0x00,0x41};
char onhook[] = {0xa5,0x5a,0x5e,0x00,0x5e};
char dialup[] = {0xa5,0x5a,0x78,0x0C,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x00,0x0b,0x74};
char switch_cacm[] = {0xa5,0x5a,0x4a,0x01,0x94,0xdf};
char switch_32178[] = {0xa5,0x5a,0x4a,0x01,0x95,0xde};
char ht95r54_dis_loopback[7] = {0xa5,0x5a,0x70,0x02,0x09,0x00,0x7b};

static char CRC8_TABLE[] = {
		0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15, 0x38,
		0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d, 0x70, 0x77,
		0x7e, 0x79, 0x6c, 0x6b, 0x62, 0x65, 0x48, 0x4f, 0x46, 
		0x41, 0x54, 0x53, 0x5a, 0x5d, 0xe0, 0xe7, 0xee, 0xe9, 
		0xfc, 0xfb, 0xf2, 0xf5, 0xd8, 0xdf, 0xd6, 0xd1, 0xc4, 
		0xc3, 0xca, 0xcd, 0x90, 0x97, 0x9e, 0x99, 0x8c, 0x8b, 
		0x82, 0x85, 0xa8, 0xaf, 0xa6, 0xa1, 0xb4, 0xb3, 0xba, 
		0xbd, 0xc7, 0xc0, 0xc9, 0xce, 0xdb, 0xdc, 0xd5, 0xd2,
		0xff, 0xf8, 0xf1, 0xf6, 0xe3, 0xe4, 0xed, 0xea, 0xb7, 
		0xb0, 0xb9, 0xbe, 0xab, 0xac, 0xa5, 0xa2, 0x8f, 0x88, 
		0x81, 0x86, 0x93, 0x94, 0x9d, 0x9a, 0x27, 0x20, 0x29, 
		0x2e, 0x3b, 0x3c, 0x35, 0x32, 0x1f, 0x18, 0x11, 0x16, 
		0x03, 0x04, 0x0d, 0x0a, 0x57, 0x50, 0x59, 0x5e, 0x4b, 
		0x4c, 0x45, 0x42, 0x6f, 0x68, 0x61, 0x66, 0x73, 0x74, 
		0x7d, 0x7a, 0x89, 0x8e, 0x87, 0x80, 0x95, 0x92, 0x9b,
		0x9c, 0xb1, 0xb6, 0xbf, 0xb8, 0xad, 0xaa, 0xa3, 0xa4,
		0xf9, 0xfe, 0xf7, 0xf0, 0xe5, 0xe2, 0xeb, 0xec, 0xc1, 
		0xc6, 0xcf, 0xc8, 0xdd, 0xda, 0xd3, 0xd4, 0x69, 0x6e, 
		0x67, 0x60, 0x75, 0x72, 0x7b, 0x7c, 0x51, 0x56, 0x5f, 
		0x58, 0x4d, 0x4a, 0x43, 0x44, 0x19, 0x1e, 0x17, 0x10, 
		0x05, 0x02, 0x0b, 0x0c, 0x21, 0x26, 0x2f, 0x28, 0x3d, 
		0x3a, 0x33, 0x34, 0x4e, 0x49, 0x40, 0x47, 0x52, 0x55, 
		0x5c, 0x5b, 0x76, 0x71, 0x78, 0x7f, 0x6a, 0x6d, 0x64, 
		0x63, 0x3e, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2c, 0x2b, 
		0x06, 0x01, 0x08, 0x0f, 0x1a, 0x1d, 0x14, 0x13, 0xae, 
		0xa9, 0xa0, 0xa7, 0xb2, 0xb5, 0xbc, 0xbb, 0x96, 0x91,
		0x98, 0x9f, 0x8a, 0x8d, 0x84, 0x83, 0xde, 0xd9, 0xd0, 
		0xd7, 0xc2, 0xc5, 0xcc, 0xcb, 0xe6, 0xe1, 0xe8, 0xef,
		0xfa, 0xfd, 0xf4, 0xf3
};

char CRC8(char *data, int length)
{
	char crc = 0x0 ;
	int i;
	for (i=0;i<length;i++)
	{
		crc = CRC8_TABLE[(crc ^ data[i])&0xFF];
	}
	return crc;
}	

int ComFunChangeHexBufferToAsc(unsigned char *hexbuf,int hexlen,char *ascstr)
{
	int i;
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
void ComFunPrintfBuffer(unsigned char *pbuffer,int len)
{
	char *pstr;
	pstr = (char *)malloc(BUFFER_SIZE_1K);
	if(pstr == NULL)
		return;
	ComFunChangeHexBufferToAsc(pbuffer,len,pstr);

	printf("%s\r\n",pstr);

	free((void *)pstr);
}

int parse_r54_ver(unsigned char *buf,int *bytes)
{
	int i,ret = 0;
	char databuf[BUFFER_LEN] = {0};
	PRINT("parse_r54_ver :");
	ComFunPrintfBuffer(buf,*bytes);
	if(*bytes == 0)
	{
		PRINT("1\n");
		ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_STM32_1,FACTORY_TEST_CMD_STM32_R54,FAIL,0x01,NULL,0);
		generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
		memset(buf,BUFFER_LEN,0);
		*bytes = 0;
		return 0;
	}
	//00 A5 5A F1 B40200AEB40D02240048
	PRINT("buf[1] = 0x%X\n",buf[1]);
	PRINT("buf[2] = 0x%X\n",buf[2]);
	PRINT("buf[3] = 0x%X\n",buf[3]);
	for(i=0;i<(*bytes-2);i++)
	{
		if((buf[i] == (unsigned char)0xA5) && (buf[i+1] == (unsigned char)0x5A) && (buf[i+2] == (unsigned char)0xF1))
		{
			PRINT("ack : 0x%X\n",buf[buf[i+3]+i+4]);
			if(buf[buf[i+3]+i+4] == (unsigned char)0x00)
			{
				PRINT("2\n");
				ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_STM32_1,FACTORY_TEST_CMD_STM32_R54,SUCCESS,0x00,NULL,0);
				generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
				memset(buf,BUFFER_LEN,0);
				*bytes = 0;
				return 0;
			}
		}
	}
	ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_STM32_1,FACTORY_TEST_CMD_STM32_R54,FAIL,0x01,NULL,0);
	generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
	PRINT("3\n");
	*bytes = 0;
	return 0;
}

int init_as532()
{
	int ret = 0;
	int ret_err = 0;
	global_sg_fd = user_open_usb(SGNAME);
	char path_new[128]={0};
	//PRINT("open sg file fd = %d\n" ,global_sg_fd);
	if (global_sg_fd < 0)
	{
		PRINT("failed to open 532 usr_mode,try boot_mode\n");
	
		usb_fd = boot_open_usb(USBNAME);
		if(usb_fd < 0 )
		{
			PRINT("open usb boot error!\n");
			return -1;
		}
		PRINT("init as532 default\n");

		ret=check_file(NULL, DEFAULT_AS532_IMAGE,path_new);
		if(ret == 0)
		{
			PRINT("check file success\n");
		}
		else
		{
			PRINT("check file error\n");
			ret_err = -7; //文件校验错误
			goto INIT_532_ERR;
		}
	
		usleep(50000);
		
		ret = LoadDatFile(path_new);
		if(ret != 1)
		{
			PRINT("update as532 failed,cause load dat file err!\r\n");
			ret_err = -2; //读取文件失败
			goto INIT_532_ERR;
		}
		
		ret = BootAsDatFileCheck();
		if(ret != 1)
		{
			PRINT("update as532 failed,cause dat file err!\r\n");
			ret_err = -3; //文件校验失败
			goto INIT_532_ERR;
		}

		ret = BootAsSendIspStart();
		if(ret != 1)
		{
			PRINT("send isp start command,but not get response!\r\n");	
			ret_err = -4; //532应答失败
			goto INIT_532_ERR;
		}
		usleep(10000);
		ret = BootAsSendConfig();
		if(ret != 1)
		{
			PRINT("send config command,but not get response!\r\n");	
			ret_err = -4; //532应答失败
			goto INIT_532_ERR;
		}
		ret = BootAsSendData();
		if(ret != 1)
		{
			PRINT("update as532 failed,cause transefer data err!\r\n");
			ret_err = -5; //发送数据失败
			goto INIT_532_ERR;
		}
		ret = BootAsSendCheck();
		if(ret != 1)
		{
			PRINT("update as532 failed,cause check err!\r\n");
			ret_err = -6; //数据校验失败
			goto INIT_532_ERR;
		}
		PRINT("Default as532 Success!!!\r\n");
		boot_close_usb(&usb_fd);
		usleep(10000*1000);
		init_key(global_pukey);
		return 0 ;
INIT_532_ERR:
		boot_close_usb(&usb_fd);
		init_key(global_pukey);
		return ret_err;
    }
    else
    {       
		close(global_sg_fd);
		global_sg_fd = -1;
	}
	return 0;
}

int init_stm32()
{
	int ret = 0;
	if(global_uart_fd < 0)
		return -1;
	if(CmdGetVersion() == -1)
	{
		if(CmdGetVersion() == -1)
		{
			if (BinFileHeadCheck(DEFAULT_STM32_IMAGE) != 0)
				return -2;
			if((ret = CmdStart()) != 0)
				return -3;
			if((ret = Update(DEFAULT_STM32_IMAGE)) != 0)
				return -4;
			if((ret = CmdEnd()) != 0)
				return -5;
			PRINT("default Stm32 success !!!\n");
		}
	}
	PRINT("STM32 ---- Ok!\n");
	//close r54 loopback
	generate_stm32_down_msg(ht95r54_dis_loopback,7,TYPE_SPI_PASSAGE);
	return 0 ;
}

void init_env(void)
{
	int sockfd,on,i,ret;
	
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

	ret = init_as532();
	if(ret != 0)
		PRINT("As532 ---- Not Ok!%d\n",ret);
	else
		PRINT("AS532 ---- Ok!\n");
		
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
	
	global_uart_fd = open(UARTNAME,O_RDWR | O_NONBLOCK);
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
		ret = init_stm32();
		if(ret != 0)
			PRINT("Stm32 ---- Not Ok!%d\n",ret);
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
	
	mkfifo(READ_FIFO,0600);
	global_read_fifo_fd = open(READ_FIFO,O_RDWR);
	if(global_read_fifo_fd < 0)
	{
		PRINT("open %s error\n",READ_FIFO);
	}
	else
		PRINT("open %s success\n",READ_FIFO);
	mkfifo(WRITE_FIFO,0600);
	global_write_fifo_fd = open(WRITE_FIFO,O_RDWR);
	if(global_write_fifo_fd < 0)
	{
		PRINT("open %s error\n",WRITE_FIFO);
	}
	else
		PRINT("open %s success\n",WRITE_FIFO);
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
	if(global_uart_fd < 0 || stm32_updating == 1)
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
	udp_get_stm32_ver = 1;
	ret = generate_stm32_down_msg(&data,1,TYPE_STM32);
	return ret;
}

int do_cmd_stm32_ver_des()
{
	char data = (char)CMD_STM32_VER_DES;
	int ret = 0;
	udp_get_stm32_ver_des = 1;
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

int do_cmd_stm32_update(unsigned char *path)
{
	int ret = 0;
	stm32_updating = 1;
	//PRINT("path = %s\n",path);
	ret = stm32_update(path);
	if(ret != 0 )
	{
		PRINT("stm32 update error! %d \n",ret);
	}
	else
	{
		PRINT("stm32 update success!\n");
	}
	stm32_updating = 0;
	return ret;
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

void update_test_thread_func(void* argv)
{
	int ret = as532_update(DEFAULT_AS532_IMAGE);
	do_cmd_stm32_update(DEFAULT_STM32_IMAGE);
	PRINT("update over %d\n",ret);	
	return;
}

int as532_update(unsigned char *path)
{
	int ret = 0;
	int ret_err = 1;
	int Versionlen_update_len =0;
    unsigned char VersionBuff_update[20] = {0};
	//int Versionlen_after_update =0;
    //unsigned char VersionBuff_after_update[20] = {0};
    unsigned char path_new[128]={0};

 	init_key(global_pukey);
	version(global_pukey,VersionBuff_update,&Versionlen_update_len);
	
    ret=check_file(VersionBuff_update, path,path_new);
	if(ret == 0)
	{
		PRINT("check file success\n");
	}
	else if(ret == -13)
	{
		PRINT("as532h is newest\n");
		//do_cmd_rep(AS532H_UPDATE,NULL,0,ret);
		ret_err = -8;
		goto AS532_UPDATE_ERR;
	}
	else
	{
		PRINT("check file error\n");
		//do_cmd_rep(AS532H_UPDATE,NULL,0,ret);
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
			PRINT("open usb error  532 booting!!\n");
			ret_err = -7;
			goto AS532_UPDATE_ERR;
		}
		goto AS532_UPDATE_ERR;
	}
	PRINT("open as532h fd = 0x%x======\r\n",usb_fd);
	
	usleep(50000);
	
	PRINT("%s\n",path_new);
	ret = LoadDatFile(path_new);
	if(ret != 1)
	{
		PRINT("update as532 failed,cause load dat file err!\r\n");
		boot_close_usb(&usb_fd);
		ret_err = -2; //读取文件失败
		goto AS532_UPDATE_ERR;
	}
	
	ret = BootAsDatFileCheck();
	if(ret != 1)
	{
		PRINT("update as532 failed,cause dat file err!\r\n");
		boot_close_usb(&usb_fd);
		ret_err = -3; //文件校验失败
		goto AS532_UPDATE_ERR;
	}

	ret = BootAsSendIspStart();
	if(ret != 1)
	{
		PRINT("send isp start command,but not get response!\r\n");	
		boot_close_usb(&usb_fd);
		ret_err = -4; //532应答失败
		goto AS532_UPDATE_ERR;
	}
	usleep(10000);
	ret = BootAsSendConfig();
	if(ret != 1)
	{
		PRINT("send config command,but not get response!\r\n");	
		boot_close_usb(&usb_fd);
		ret_err = -4; //532应答失败
		goto AS532_UPDATE_ERR;
	}
	ret = BootAsSendData();
	if(ret != 1)
	{
		PRINT("update as532 failed,cause transefer data err!\r\n");
		boot_close_usb(&usb_fd);
		ret_err = -5; //发送数据失败
		goto AS532_UPDATE_ERR;
	}
	ret = BootAsSendCheck();
	if(ret != 1)
	{
		PRINT("update as532 failed,cause check err!\r\n");
		boot_close_usb(&usb_fd);
		ret_err = -6; //数据校验失败
		goto AS532_UPDATE_ERR;
	}
	boot_close_usb(&usb_fd);
	PRINT("update as532 Success!!!\r\n");
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
	pthread_create(&as532_update_pthread,NULL,(void*)update_test_thread_func,NULL);
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
				do_cmd_rep(STM32_VER_DES,NULL,0,0x00);
			}
			break;
		case AS532H_UPDATE:
			PRINT("AS532H_UPDATE\n");
			do_cmd_532_update();
			break;
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

	PRINT("serial config finished\n");
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

	switch(pppacket[PACKET_STM32_REP_POS])
	{
		case 0x00: //成功 	//EA FF 5A A5 00 07 10 00 00 DATA XX
			data_len = pppacket[PACKET_STM32_DATA_LEN_POS_HEIGHT]*256 + pppacket[PACKET_STM32_DATA_LEN_POS_LOW] - 3;
			if(start_factory_test == 1)
			{
				ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_STM32_1,FACTORY_TEST_CMD_STM32_VER,SUCCESS,0x00,pppacket+PACKET_STM32_DATA_POS,data_len);
				//ComFunPrintfBuffer(pppacket+PACKET_STM32_DATA_POS,data_len);
				generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
			}
			if(udp_get_stm32_ver == 1)
				do_cmd_rep(STM32_VER,&pppacket[PACKET_STM32_DATA_POS],data_len,0x00);
			break;
		case 0x01: //命令错误
			data_len = pppacket[PACKET_STM32_DATA_LEN_POS_HEIGHT]*256 + pppacket[PACKET_STM32_DATA_LEN_POS_LOW] - 3;
			if(start_factory_test == 1)
			{
				ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_STM32_1,FACTORY_TEST_CMD_STM32_VER,FAIL,0x01,NULL,0);
				generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
			}
			if(udp_get_stm32_ver == 1)
				do_cmd_rep(STM32_VER,&pppacket[PACKET_STM32_DATA_POS],data_len,0x01);
			break;
		case 0x02: //校验错误
			data_len = pppacket[PACKET_STM32_DATA_LEN_POS_HEIGHT]*256 + pppacket[PACKET_STM32_DATA_LEN_POS_LOW] - 3;
			if(start_factory_test == 1)
			{
				ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_STM32_1,FACTORY_TEST_CMD_STM32_VER,FAIL,0x02,NULL,0);
				generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
			}
			if(udp_get_stm32_ver == 1)
				do_cmd_rep(STM32_VER,&pppacket[PACKET_STM32_DATA_POS],data_len,0x02);
			break;
		default:
			break;
	}
	udp_get_stm32_ver = 0;
	return 0;
}
int UartGetVerDes(unsigned char *pppacket,int bytes)
{
	PRINT("UartGetVerDes is called\r\n");
	int index = 0;
	int data_len = 0;

	switch(pppacket[PACKET_STM32_REP_POS])
	{
		//EA FF 5A A5 00 07 10 00 00 00 00 00 02 FF

		case 0x00: //成功 FE EF A5 5A 00 05 01 00 01 01 01 XX
			if(udp_get_stm32_ver_des == 1)
			{
				data_len = pppacket[PACKET_STM32_DATA_LEN_POS_HEIGHT]*256 + pppacket[PACKET_STM32_DATA_LEN_POS_LOW] - 3;
				do_cmd_rep(STM32_VER_DES,&pppacket[PACKET_STM32_DATA_POS],data_len,0x00);
			}
			break;
		case 0x01: //命令错误
			if(udp_get_stm32_ver_des == 1)
			{
				data_len = pppacket[PACKET_STM32_DATA_LEN_POS_HEIGHT]*256 + pppacket[PACKET_STM32_DATA_LEN_POS_LOW] - 3;
				do_cmd_rep(STM32_VER_DES,&pppacket[PACKET_STM32_DATA_POS],data_len,0x01);
			}
			break;
		case 0x02: //校验错误
			if(udp_get_stm32_ver_des == 1)
			{
				data_len = pppacket[PACKET_STM32_DATA_LEN_POS_HEIGHT]*256 + pppacket[PACKET_STM32_DATA_LEN_POS_LOW] - 3;
				do_cmd_rep(STM32_VER_DES,&pppacket[PACKET_STM32_DATA_POS],data_len,0x02);
			}
			break;
		default:
			break;
	}
	udp_get_stm32_ver_des = 0;
	return 0;
}
int UartCallBoot(unsigned char *pppacket,int bytes)
{
	PRINT("UartCallBoot is called\r\n");
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
		case CMD_STM32_TICK:
			PRINT("CMD_STM32_TICK\n");
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
	//PRINT("type = 0x%x\n",type);
	int i = 0;
	for(i=0;i<PASSAGE_NUM;i++)
	{
		//PRINT("passage_list[%d].type = 0x%x\n",i,passage_list[i].type);
		if(passage_list[i].type == type)
		{
			PRINT("%s\n",passage_list[i].passage_name);
			if((passage_list[i].fd < 0) || write(passage_list[i].fd,packet,bytes) != bytes)
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
	char databuf[BUFFER_LEN] = {0};
	int ret = 0;
	ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_CONTROL_1,FACTORY_TEST_CMD_CONTROL_START,SUCCESS,0x00,NULL,0);
	generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
	start_factory_test = 1;
	generate_stm32_down_msg(switch_32178,6,TYPE_SPI_PASSAGE);

	return 0;
}

int stop_test()
{
	start_factory_test = 0;
	generate_stm32_down_msg(switch_32178,6,TYPE_SPI_PASSAGE);
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

int factory_test_cmd_as532_test()
{
	unsigned char spiBuffer[100] = {0};
	int spiBufferLen =0;
	int ret = 0;
    unsigned char databuf[BUFFER_LEN] = {0};
	init_key(global_pukey);
	ret = testSpi(global_pukey, spiBuffer, &spiBufferLen );
	if (ret != 0) 
	{
		PRINT(" send test data error\n");
		ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_AS532_1,FACTORY_TEST_CMD_AS532_TEST,FAIL,/*(char)ret*/1,spiBuffer,0);
		generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
	}
	else
	{ 
		PRINT("send test success\n");  
		ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_AS532_1,FACTORY_TEST_CMD_AS532_TEST,SUCCESS,0,spiBuffer,spiBufferLen);
		generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
	}
}

int factory_test_cmd_stm32_ver()
{
	//char sendbuf[BUFFER_SIZE_4K]={0};
	char data = (char)CMD_STM32_VER;
	generate_stm32_down_msg(&data,1,TYPE_STM32);
	return 0;
}

int factory_test_cmd_stm32_pic()
{
	//A5 5A E1 02 E5 00 E5
	char data[] = {0XA5,0X5A,0XE1,0X02,0XE5,0X00,0XE5};
	generate_stm32_down_msg(data,7,TYPE_UART_PASSAGE);
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
	write(fd,"test_on",7);
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
	write(fd,"test_off",8);
	close(fd);
	//pstn led
	generate_stm32_down_msg(buf,2,TYPE_STM32);
	return 0;
}

int factory_test_cmd_9344_led3()
{
	char buf[2] = {0x83,0x00};
	int fd = open("/dev/wifiled",O_RDWR);
	if(fd < 0)
	{
		printf("open error\n");
		return -1;
	}
	printf("open success\n");
	write(fd,"test_off",8);
	close(fd);
	//pstn led
	generate_stm32_down_msg(buf,2,TYPE_STM32);
	return 0;
}


int factory_test_cmd_stm32_r54()
{
	char r54_ver_test[] = {0xa5,0x5a,0x71,0x00,0x71};
	generate_stm32_down_msg(r54_ver_test,5,TYPE_SPI_PASSAGE);
	return 0;
}

int factory_test_cmd_all_test()
{
	return 0;
}

void factory_test_cmd_9344_r54_call_func(void *argv)
{
	int ret = 0;
    unsigned char databuf[BUFFER_LEN] = {0};
	generate_stm32_down_msg(switch_cacm,6,TYPE_SPI_PASSAGE);
	usleep(500*1000);
	generate_stm32_down_msg(offhook,5,TYPE_SPI_PASSAGE);
	sleep(1);
	generate_stm32_down_msg(dialup,17,TYPE_SPI_PASSAGE);
	sleep(3);
	generate_stm32_down_msg(onhook,5,TYPE_SPI_PASSAGE);
	sleep(1);
	generate_stm32_down_msg(switch_32178,6,TYPE_SPI_PASSAGE);
	
	ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_9344_1,FACTORY_TEST_CMD_9344_R54_CALL,SUCCESS,0,NULL,0);
	generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
}

int factory_test_cmd_9344_r54_call()
{
	pthread_t r54_pthread;
	pthread_create(&r54_pthread,NULL,(void*)factory_test_cmd_9344_r54_call_func,NULL);

	return 0;
}

int factory_test_cmd_9344_r54_called()
{
	generate_stm32_down_msg(switch_cacm,6,TYPE_SPI_PASSAGE);
	
	return 0;
}

int factory_test_cmd_update_mac(unsigned char* data,int len)
{
	int ret = 0;
	int i = 0;
    unsigned char databuf[BUFFER_LEN] = {0};
    unsigned char macbuf[20] = {0};
    unsigned char cmdbuf[50] = {0};
    unsigned char* macp = macbuf;
	pid_t status;
	ComFunPrintfBuffer(data,len);
	if(len != 12)
	{
		ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_MAC_SN_1,FACTORY_TEST_CMD_UPDATE_MAC,FAIL,1,NULL,0);
	}
	else
	{
		for(i=0;i<len;i++)
		{
			sprintf(macp,"%c",data[i]);
			macp++;
			if(i == len-1)
				break;
			if(i % 2 != 0)
			{
				sprintf(macp,"%c",':');
				macp++;
			}
		}
		macp = '\0';
		sprintf(cmdbuf,"%s%s","set_macaddr ",macbuf);
		PRINT("cmdbuf = %s\n",cmdbuf);
		status = system(cmdbuf);
		PRINT("status = %d\n",status);
		if(status == -1)
		{
			PRINT("system fail!\n");
			ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_MAC_SN_1,FACTORY_TEST_CMD_UPDATE_MAC,FAIL,1,NULL,0);
		}
		else
		{
			ret = WEXITSTATUS(status);
			PRINT("ret = %d\n",ret);
			if(ret == 0)
			{
				PRINT("Update Mac Success!\n");
				ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_MAC_SN_1,FACTORY_TEST_CMD_UPDATE_MAC,SUCCESS,0,NULL,0);
			}
			else
			{
				PRINT("Update Mac Fail!\n");
				ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_MAC_SN_1,FACTORY_TEST_CMD_UPDATE_MAC,FAIL,1,NULL,0);
			}
		}
	}
	generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
	return 0;
}

int factory_test_cmd_update_sn(unsigned char* data,int len)
{
	int ret = 0;
	int i = 0;
    unsigned char databuf[BUFFER_LEN] = {0};
    unsigned char cmdbuf[50] = {0};
	pid_t status;
	ComFunPrintfBuffer(data,len);
	if(len != 18)
	{
		ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_MAC_SN_1,FACTORY_TEST_CMD_UPDATE_SN,FAIL,1,NULL,0);
	}
	else
	{
		snprintf(cmdbuf,13+len,"%s%s","fw_setenv SN ",data);
		PRINT("cmdbuf = %s\n",cmdbuf);
		status = system(cmdbuf);
		PRINT("status = %d\n",status);
		if(status == -1)
		{
			PRINT("system fail!\n");
			ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_MAC_SN_1,FACTORY_TEST_CMD_UPDATE_SN,FAIL,1,NULL,0);
		}
		else
		{
			ret = WEXITSTATUS(status);
			PRINT("ret = %d\n",ret);
			if(ret == 0)
			{
				PRINT("Update Sn Success!\n");
				ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_MAC_SN_1,FACTORY_TEST_CMD_UPDATE_SN,SUCCESS,0,NULL,0);
			}
			else
			{
				PRINT("Update Sn Fail!\n");
				ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_MAC_SN_1,FACTORY_TEST_CMD_UPDATE_SN,FAIL,1,NULL,0);
			}
		}
	}
	generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
	return 0;
}

int factory_test_down_control(char cmd)
{
	switch(cmd)
	{
		case FACTORY_TEST_CMD_CONTROL_START:
			PRINT("FACTORY_TEST_CMD_CONTROL_START\n");
			start_test();
			break;
		case FACTORY_TEST_CMD_CONTROL_STOP:
			stop_test();
			PRINT("FACTORY_TEST_CMD_CONTROL_STOP\n");
			break;
		default:
			break;
	}
	return 0;
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
			factory_test_cmd_stm32_pic();
			//do_cmd_stm32_update();
			break;
		case FACTORY_TEST_CMD_STM32_R54:
			factory_test_cmd_stm32_r54();
			PRINT("FACTORY_TEST_CMD_STM32_R54\n");
			break;
		default:
			break;
	}
	return 0;
}

int factory_test_down_as532(char cmd)
{
	switch(cmd)
	{
		case FACTORY_TEST_CMD_AS532_VER:
			PRINT("FACTORY_TEST_CMD_AS532_VER\n");
			factory_test_cmd_as532_ver();
			break;
		case FACTORY_TEST_CMD_AS532_TEST:
			PRINT("FACTORY_TEST_CMD_AS532_TEST\n");
			factory_test_cmd_as532_test();
			break;
		default:
			break;
	}
	return 0;
}

int factory_test_down_all(char cmd)
{
	switch(cmd)
	{
		case FACTORY_TEST_CMD_ALL_TEST:
			PRINT("FACTORY_TEST_CMD_ALL_TEST\n");
			factory_test_cmd_all_test();
			break;
		default:
			break;
	}
	return 0;
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
			factory_test_cmd_9344_led1();
			break;
		case FACTORY_TEST_CMD_9344_LED2:
			PRINT("FACTORY_TEST_CMD_9344_LED2\n");
			factory_test_cmd_9344_led2();
			break;
		case FACTORY_TEST_CMD_9344_LED3:
			PRINT("FACTORY_TEST_CMD_9344_LED3\n");
			factory_test_cmd_9344_led3();
			break;
		case FACTORY_TEST_CMD_9344_CALL:
		case FACTORY_TEST_CMD_9344_CALLED:		
			generate_stm32_down_msg(switch_32178,6,TYPE_SPI_PASSAGE);	
			sendbuf[index++]=0xA5;
			sendbuf[index++]=0X5A;
			sendbuf[index++]=FACTORY_TEST_CMD_DOWN_9344_1;
			sendbuf[index++]=cmd;
			write_passage((char)TYPE_PHONE_PASSAGE,sendbuf,index);
			break;
		case FACTORY_TEST_CMD_9344_R54_CALL:
			PRINT("FACTORY_TEST_CMD_9344_R54_CALL\n");
			factory_test_cmd_9344_r54_call();
			break;
		case FACTORY_TEST_CMD_9344_R54_CALLED:
			PRINT("FACTORY_TEST_CMD_9344_R54_CALLED\n");
			factory_test_cmd_9344_r54_called();
			break;	
		default:
			break;
	}
	return 0;
}

int factory_test_down_mac_sn(char cmd,unsigned char* data,int len)
{
	switch(cmd)
	{
		case FACTORY_TEST_CMD_UPDATE_MAC:
			PRINT("FACTORY_TEST_CMD_UPDATE_MAC\n");
			factory_test_cmd_update_mac(data,len);
			break;
		case FACTORY_TEST_CMD_UPDATE_SN:
			PRINT("FACTORY_TEST_CMD_UPDATE_SN\n");
			factory_test_cmd_update_sn(data,len);
			break;
		default:
			break;
	}
	return 0;
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
			if(start_factory_test != 1)
				return -1;
			factory_test_down_stm32(packet[FACTORY_TEST_CMD_2_POS]);
			break;
		case FACTORY_TEST_CMD_DOWN_AS532_1:
			if(start_factory_test != 1)
				return -1;
			factory_test_down_as532(packet[FACTORY_TEST_CMD_2_POS]);
			break;		
		case FACTORY_TEST_CMD_DOWN_9344_1:
			if(start_factory_test != 1)
				return -1;
			factory_test_down_9344(packet[FACTORY_TEST_CMD_2_POS]);
			break;
		case FACTORY_TEST_CMD_DOWN_ALL_1:
			if(start_factory_test != 1)
				return -1;
			factory_test_down_all(packet[FACTORY_TEST_CMD_2_POS]);
			break;
		case FACTORY_TEST_CMD_DOWN_MAC_SN_1:
			factory_test_down_mac_sn(packet[FACTORY_TEST_CMD_2_POS],&packet[FACTORY_TEST_CMD_DATA_POS],data_len-2);
			break;
		default:
			PRINT("cmd undef\n");
			break;
	}
	return 0;

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

int UartPacketDis(unsigned char *ppacket,int bytes)
{
	int rtn = 0;
	//EA FF 5A A5 00 00 10 00 00 DATA XX
	//EA FF 5A A5 00 00 10
	PRINT("Uart data:: ");
	ComFunPrintfBuffer(ppacket,bytes);
	//PRINT("bytes = %d\n",bytes);
	if(ppacket[0]!=(unsigned char)STM32_UP_HEAD_BYTE1_1 || ppacket[1]!=(unsigned char)STM32_UP_HEAD_BYTE2_1 || ppacket[2]!=(unsigned char)STM32_UP_HEAD_BYTE3_1 || ppacket[3]!=(unsigned char)STM32_UP_HEAD_BYTE4_1) 
	{
		PRINT("wrong data\n");
		return -1;
	}
	int len_tmp=0;
	int xor=0;
	int t1 = 0;
	int t2 = 0;
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
		case TYPE_STM32:
			PRINT("UartStm32\r\n");
			rtn = UartStm32Do(ppacket,bytes);
			break;
		case TYPE_USB_PASSAGE:
			PRINT("UsbPassage\r\n");
			if(factory_test(ppacket+7,len_tmp-1) == -1)
			{
				rtn = UartPassageDo(TYPE_USB_PASSAGE,ppacket,bytes);
			}
			break;
		case TYPE_UART_PASSAGE:
			PRINT("UartPassage\r\n");
			if(start_factory_test == 1)
			{
				if(BUFFER_SIZE_1K - factory_test_wp < 128)//即将到缓冲尾部，则前移
				{
					t1 = factory_test_wp - factory_test_rp;
					if(t1 > 0)//有未处理的数据，则将该数据前移，并重置写入和读取位置
					{
						for(t2 = 0;t2 < t1;t2++)
							factory_test_buffer[t2] = factory_test_buffer[factory_test_rp + t2];
						factory_test_rp = 0;
						factory_test_wp = t1;
					}
				}
				memcpy(factory_test_buffer+factory_test_wp,ppacket+7,len_tmp-1);
				factory_test_wp = factory_test_wp+len_tmp-1;
			}
			else
			{
				rtn = UartPassageDo(TYPE_UART_PASSAGE,ppacket,bytes);
			}
			break;
		case TYPE_SPI_PASSAGE:
			PRINT("SpiPassage\r\n");
			if(start_factory_test == 1)
			{
				if(BUFFER_SIZE_1K - factory_test_wp < 128)//即将到缓冲尾部，则前移
				{
					t1 = factory_test_wp - factory_test_rp;
					if(t1 > 0)//有未处理的数据，则将该数据前移，并重置写入和读取位置
					{
						for(t2 = 0;t2 < t1;t2++)
							factory_test_buffer[t2] = factory_test_buffer[factory_test_rp + t2];
						factory_test_rp = 0;
						factory_test_wp = t1;
					}
				}
				memcpy(factory_test_buffer+factory_test_wp,ppacket+7,len_tmp-1);
				factory_test_wp = factory_test_wp+len_tmp-1;
			}
			else
			{
				rtn = UartPassageDo(TYPE_SPI_PASSAGE,ppacket,bytes);
			}
			break;
		//case TYPE_PHONE_PASSAGE:
			//PRINT("UartTest\r\n");
			//rtn = UartTestDo(ppacket,bytes);
			//break;
		default:
			rtn = -1;
			break;
	}

	return rtn;
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
		if(stm32_updating == 0 && ota_stm32_ver_flag == 0)
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
										last_led_status[0] = recvbuf[j];
										last_led_status[1] = recvbuf[j+1];
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

unsigned char *find_head(void)
{
	int valid_len;
	unsigned char *pp;
	int index = 0;
	int rtn,get_head;
	valid_len = factory_test_wp - factory_test_rp;

	if(valid_len < 5)
		return 0;
	pp = &factory_test_buffer[factory_test_rp];
	get_head = 0;
	for(index = 0;index < (valid_len - 1);index++)
	{
		if((pp[index] == (unsigned char)0xA5) && (pp[index + 1] == (unsigned char)0x5A))
		{
			get_head = 1;
			if(index != 0)
			{
				factory_test_rp += index;
			}
			return &pp[index];
		}
	}
	return 0;
}

int prase_test_msg(unsigned char *des_packet_buffer,int *packet_size)
{
	unsigned char *phead;
	unsigned char *pwp;
	int valid_len;
	int my_packet_bytes;
	phead = find_head();
	if(phead == 0)
		return 0;
	pwp = (unsigned char *)(&factory_test_buffer[factory_test_wp]);

	valid_len = pwp - phead;
	if(valid_len < 5)
	{
		return 0;
	}
	if(phead[2] == (unsigned char)0xf1)
	{
		my_packet_bytes = phead[3]+6;
	}
	else
	{
		my_packet_bytes = phead[3]+5;
	}

	if(valid_len >= my_packet_bytes)
	{
		memcpy((void *)(des_packet_buffer),(void *)(phead),my_packet_bytes);
		*packet_size = my_packet_bytes;
		factory_test_rp += my_packet_bytes;
		return 1;
	}
	return 0;
}

int run_test_cmd(unsigned char *ppacket,int bytes)
{
	int ret = 0;
	unsigned char databuf[BUFFER_LEN] = {0};
	// a5 5a 74 01 01 xx
	PRINT("test recv data:: ");
	ComFunPrintfBuffer(ppacket,bytes);
	//PRINT("bytes = %d\n",bytes);
	if(ppacket[0]!=(unsigned char)0XA5 || ppacket[1]!=(unsigned char)0X5A ) 
	{
		PRINT("wrong data\n");
		return -1;
	}
	int len_tmp=0;
	int xor=0;
	int t1 = 0;
	int t2 = 0;
	unsigned char *bufp = ppacket;
	len_tmp = ppacket[3];
	PRINT("len_tmp=%d\n",len_tmp);
	if(ppacket[2] == (unsigned char)0x11)
		xor=sumxor(ppacket+4,len_tmp);
	else
		xor=sumxor(ppacket+2,len_tmp+2);
	PRINT("xor=0x%x\n",xor);
	if(ppacket[bytes-1]!=(unsigned char)xor)
	{
		PRINT("xor error\n");
		return -1;
	}

	switch(ppacket[2])
	{
		case 0x04:
			PRINT("R54 CALLED TEST DONE!\n");
			ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_9344_1,FACTORY_TEST_CMD_9344_R54_CALLED,SUCCESS,0,&ppacket[4],ppacket[3]);
			generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
			break;
		case 0x05:
			PRINT("R54 CALLED TEST DONE!\n");
			ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_9344_1,FACTORY_TEST_CMD_9344_R54_CALLED,SUCCESS,0,&ppacket[8],ppacket[7]);
			generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
			break;
		case 0xf1:
			PRINT("R54 TEST DONE!\n");
			if(ppacket[bytes-2] == (unsigned char)0x00)
			{ 
				ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_STM32_1,FACTORY_TEST_CMD_STM32_R54,SUCCESS,0,NULL,0);
			}
			else
			{
				ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_STM32_1,FACTORY_TEST_CMD_STM32_R54,FAIL,1,NULL,0);
			}
			generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
			break;
		case 0x11:
			PRINT("PIC TEST DONE!\n");
			if(ppacket[bytes-2] == (unsigned char )0x00 && ppacket[bytes-3] == (unsigned char )0xE5)
			{
				ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_STM32_1,FACTORY_TEST_CMD_STM32_PIC,SUCCESS,0,NULL,0);
			}
			else
			{
				ret = generate_test_up_msg(databuf,FACTORY_TEST_CMD_UP_STM32_1,FACTORY_TEST_CMD_STM32_PIC,FAIL,1,NULL,0);
			}
			generate_stm32_down_msg(databuf,ret,TYPE_USB_PASSAGE);
			break;
		default:
			break;
	}
}

void factory_test_func(void *argv)
{
	unsigned char packet_buffer[256]={0};
	int packet_bytes;
	int rtn = 0;
	PRINT("%s is running......\n",__FUNCTION__);
	while(1)
	{
		if(start_factory_test == 1)
		{
			if(factory_test_rp == factory_test_wp)
			{
				factory_test_rp = factory_test_wp = 0;
				usleep(100*1000);
				continue;
			}
			do
			{
				//接收完整包
				rtn = prase_test_msg(packet_buffer,&packet_bytes);
				if(rtn == 1)
				{
					//对完整包内的命令进行解析执行
					rtn = run_test_cmd(packet_buffer,packet_bytes);
					PRINT("factory_test_rp = %d\n",factory_test_rp);
					PRINT("factory_test_wp = %d\n",factory_test_wp);
				}
				else
					break;
			}while(1);
		}
		else
		{
			usleep(50*1000);
		}
	}
}

int generate_ota_up_msg(unsigned char cmd,unsigned char id,unsigned char *data,int data_len)
{
	char sendbuf[128]={0};
	int index = 0;
	sendbuf[index++] = (unsigned char)OTA_HEAD1;
	sendbuf[index++] = (unsigned char)OTA_HEAD2;
	sendbuf[index++] = (unsigned char)OTA_HEAD3;
	sendbuf[index++] = (unsigned char)OTA_VER;
	sendbuf[index++] = (unsigned char)cmd;
	sendbuf[index++] = (unsigned char)id;
	sendbuf[index++] = (unsigned char)((data_len)>>24);
	sendbuf[index++] = (unsigned char)((data_len)>>16);
	sendbuf[index++] = (unsigned char)((data_len)>>8);
	sendbuf[index++] = (unsigned char)((data_len)>>0);
	sendbuf[index++] = (unsigned char)((data_len)>>24);
	sendbuf[index++] = (unsigned char)((data_len)>>16);
	sendbuf[index++] = (unsigned char)((data_len)>>8);
	sendbuf[index++] = (unsigned char)((data_len)>>0);
	sendbuf[index++] = (unsigned char)0x00;
	sendbuf[index++] = (unsigned char)0x00;
	memcpy(&sendbuf[index],data,data_len);
	index += data_len;
	sendbuf[index++] = CRC8(sendbuf+3,index-3);
	if(global_write_fifo_fd > 0)
	{
		if(write(global_write_fifo_fd,sendbuf,index) == index)
		{
			PRINT("send fifo success:");
			ComFunPrintfBuffer(sendbuf,index);
		}
	}
	return 0;
}

int ota_as532h_ver()
{
	int ret = 0;
	int len = 0;
	unsigned char outbuf[128]={0};

	init_key(global_pukey);
	ret = version(global_pukey,outbuf,&len);
	if(ret == 0)
	{
		generate_ota_up_msg(OTA_CMD_VER_RET,OTA_AS532H_ID,outbuf,len);
	}
	return ret;
}

int ota_stm32_ver()
{
	ota_stm32_ver_flag = 1;
	if(CmdGetVersion() == 0)
	{
		generate_ota_up_msg(OTA_CMD_VER_RET,OTA_STM32_ID,stm_format_version,4);
	}
	ota_stm32_ver_flag = 0;
	return 0;
}

int ota_as532h_sn()
{
	generate_ota_up_msg(OTA_CMD_SN_RET,OTA_AS532H_ID,"12345",5);
	return 0;
}

int ota_stm32_sn()
{
	generate_ota_up_msg(OTA_CMD_SN_RET,OTA_STM32_ID,"12345",5);
	return 0;
}

int ota_as532h_update(unsigned char* path)
{
	char result = 0;
	int ret = as532_update(path);
	if(ret == 0)
	{
		result = 1;
	}
	else
	{
		result = 0;
	}
	system("rm -rf /tmp/AS532.zip");
	generate_ota_up_msg(OTA_CMD_UPDATE_RET,OTA_AS532H_ID,&result,1);
	return 0;
}

int ota_stm32_update(unsigned char* path)
{
	char result = 0;
	int ret = do_cmd_stm32_update(path);
	if(ret == 0)
	{
		result = 1;
		generate_stm32_down_msg(last_led_status,2,TYPE_STM32);
	}
	else
	{
		result = 0;
	}
	system("rm -rf /tmp/STM32.zip");
	generate_ota_up_msg(OTA_CMD_UPDATE_RET,OTA_STM32_ID,&result,1);
	return 0;
}

int FifoPacketDis(unsigned char *ppacket,int bytes)
{
	int rtn = 0;
	int len_tmp=0;
	int crc=0;
	unsigned char *bufp = ppacket;
	PRINT("Fifo data:: ");
	ComFunPrintfBuffer(ppacket,bytes);
	//4F 54 41 31 21 11 00 00 00 01 00 00 00 01 00 00 00 92
	PRINT("bytes = %d\n",bytes);
	if(ppacket[0]!=(unsigned char)OTA_HEAD1 || ppacket[1]!=(unsigned char)OTA_HEAD2 || ppacket[2]!=(unsigned char)OTA_HEAD3) 
	{
		PRINT("wrong data\n");
		return -1;
	}
	if(ppacket[3] != (unsigned char)OTA_VER)
	{
		PRINT("ota ver error\n");
		return -2;
	}
	
	len_tmp =  (ppacket[6] << 24);
	len_tmp += (ppacket[7] << 16);
	len_tmp += (ppacket[8] << 8);
	len_tmp += (ppacket[9] << 0);
	PRINT("len_tmp=%d\n",len_tmp);
	crc=CRC8(ppacket+3,bytes-4);
	PRINT("crc=0x%x\n",(unsigned char)crc);
	if(ppacket[bytes-1]!=(unsigned char)crc)
	{
		PRINT("crc error\n");
		return -3;
	}

	switch(ppacket[4])
	{
		case OTA_CMD_VER_REQ:
			if(ppacket[5] == (unsigned char)OTA_AS532H_ID)
			{
				PRINT("OTA_AS532H_VER_REQ\n");
				ota_as532h_ver();
			}
			else if(ppacket[5] == (unsigned char)OTA_STM32_ID)
			{
				PRINT("OTA_STM32_VER_REQ\n");
				ota_stm32_ver();
			}
			break;
		case OTA_CMD_SN_REQ:
			if(ppacket[5] == (unsigned char)OTA_AS532H_ID)
			{
				PRINT("OTA_AS532H_SN_REQ\n");
				ota_as532h_sn();
			}
			else if(ppacket[5] == (unsigned char)OTA_STM32_ID)
			{
				PRINT("OTA_STM32_SN_REQ\n");
				ota_stm32_sn();
			}
			break;
		case OTA_CMD_UPDATE_REQ:
			if(ppacket[5] == (unsigned char)OTA_AS532H_ID)
			{
				PRINT("OTA_AS532H_UPDATE_REQ\n");
				ppacket[16+len_tmp] = '\0';
				//ota_as532h_update(&ppacket[16]);
				ota_as532h_update("/tmp/AS532.zip");
			}
			else if(ppacket[5] == (unsigned char)OTA_STM32_ID)
			{
				PRINT("OTA_STM32_UPDATE_REQ\n");
				ppacket[16+len_tmp] = '\0';
				//ota_stm32_update(&ppacket[16]);
				ota_stm32_update("/tmp/STM32.zip");
			}
			break;
		default:
			rtn = -1;
			break;
	}
	//usleep(50*1000);
	return rtn;
}

unsigned char *FifoSearchHead(void)
{
	int valid_len;
	unsigned char *pp;
	int index = 0;
	int rtn,get_head;
	valid_len = fifo_wp_pos - fifo_rp_pos;
	if(valid_len < 16)
		return 0;
	pp = &fifo_recv_buffer[fifo_rp_pos];
	get_head = 0;
	for(index = 0;index < (valid_len - 2);index++)
	{
		if((pp[index] == (unsigned char)OTA_HEAD1) && (pp[index + 1] == (unsigned char)OTA_HEAD2) && (pp[index + 2] == (unsigned char)OTA_HEAD3))
		{
			get_head = 1;
			if(index != 0)
			{
				fifo_rp_pos += index;
			}
			return &pp[index];
		}
	}
	return 0;
}


int FifoPacketRcv(unsigned char *des_packet_buffer,int *packet_size)
{
	int valid_len;
	unsigned char *phead;
	unsigned char *pwp;
	int index = 0;
	unsigned int my_packet_bytes;

	phead = FifoSearchHead();
	if(phead == 0)
		return 0;
	pwp = (unsigned char *)(&fifo_recv_buffer[fifo_wp_pos]);

	valid_len = pwp - phead;
	
	if(valid_len < 16)
	{
		return 0;
	}
	//PRINT("ppacket[6] = %d\n",phead[6]<<24);
	//PRINT("ppacket[7] = %d\n",phead[7]<<16);
	//PRINT("ppacket[8] = %d\n",phead[8]<<8);
	//PRINT("ppacket[9] = %d\n",phead[9]);
	//4F 54 41 31 21 11 00 00 00 01 00 00 00 01 00 00 00 92

	my_packet_bytes = ((phead[6]<<24) + (phead[7]<<16) + (phead[8]<<8) + phead[9]);
	my_packet_bytes += 17;
	PRINT("valid_len = %d\n",valid_len);
	PRINT("my_packet_bytes = %d\n",my_packet_bytes);
	
	if(valid_len >= my_packet_bytes)
	{
		memcpy((void *)(des_packet_buffer),(void *)(phead),my_packet_bytes);
		*packet_size = my_packet_bytes;
		fifo_rp_pos += my_packet_bytes;
		return 1;
	}
	return 0;
}


void ota_thread_func(void * argv)
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
		if(fifo_wp_pos == fifo_rp_pos)
		{
			fifo_wp_pos = 0;
			fifo_rp_pos = 0;
		}
		if(sizeof(fifo_recv_buffer) - fifo_wp_pos < 128)//即将到缓冲尾部，则前移
		{
			t1 = fifo_wp_pos - fifo_rp_pos;
			if(t1 > 0)//有未处理的数据，则将该数据前移，并重置写入和读取位置

			{
				for(t2 = 0;t2 < t1;t2++)
					fifo_recv_buffer[t2] = fifo_recv_buffer[fifo_rp_pos + t2];
				fifo_rp_pos = 0;
				fifo_wp_pos = t1;
			}
		}
		if(global_read_fifo_fd >= 0)
		{
			read_ret = read(global_read_fifo_fd,(unsigned char*)(&fifo_recv_buffer[fifo_wp_pos]),BUFFER_SIZE_1K);
		}
		if(read_ret > 0)//有数据读取到
		{
			PRINT("from Fifo\n");
			ComFunPrintfBuffer(&fifo_recv_buffer[fifo_wp_pos],read_ret);
			fifo_wp_pos += read_ret;
		}
		else//错误
		{
			//   PRINT("Read Fifo error\n");
			usleep(20*1000);
			continue;
		}
		do
		{
			//接收完整包
			rtn = FifoPacketRcv(packet_buffer,&packet_bytes);
			if(rtn == 1)
			{
				//对完整包内的命令进行解析执行
				rtn = FifoPacketDis(packet_buffer,packet_bytes);
			}
			else
				break;
		}while(1);
		usleep(20*1000);
	}
}

