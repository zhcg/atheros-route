#include "common.h"
#include "modules_server.h"
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define LONG_BYTES					4
#define INT_BYTES					4
#define SHORT_BYTES					2
#define CHAR_BYTES					1

#define	OK							0	 

#define MIN_RSP_PACKET_BYTES		4

#define MAX_PACKET_BYTES			2052
#define MAX_PACKET_DATA_BYTES		2048
#define MAX_RECV_PACKET_BYTES		128

#define FIX_HEAD_BYTE1				0XEF
#define FIX_HEAD_BYTE2				0X01

#define ISPSTART_CMD_CODE			4
#define FIX_CMD_CODE				2
	
#define PARAM_SET_CODE				1
#define PARAM_DOWNLOAD_CODE			2
#define PARAM_CHECK_CODE			8	

//#define PACKET_HEAD_BYTES			2
#define PACKET_CMD_BYTES			1
#define PACKET_PARAM_BYTES			2
#define PACKET_LC_BYTES				2

#define FIX_CHECK_LC_LSB			0X03
#define FIX_CHECK_LC_MSB			0X00

#define RUN_AFTER_UPDATE			1


#define APP_ENTRY_OFFSET					4
#define FIRST_SEGMENT_LEGNTH_OFFSET			9
#define FIRST_SEGMENT_START_OFFSET			13

#define FIX_CONFIG_CMD_PAD			0X20
#define FIX_REG_COUNTS				3
#define FIX_WAIT_LOOP				20
#define FIX_CONFIG_DATA_LENGTH		(0X20)


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int usb_fd;

unsigned char dat_file_buf[256 * 1024];

unsigned char *pdat_file;
unsigned int dat_file_size;
unsigned int lcodelength = 0;	

unsigned short data_sum = 0;
unsigned char send_buf[MAX_PACKET_BYTES];
unsigned char recv_buf[MAX_RECV_PACKET_BYTES];
unsigned int send_data_bytes;
int recv_data_bytes;

unsigned int app_start_add;
unsigned char reg_counts;

unsigned int segment_bytes;
unsigned int segment_start_add;
unsigned char *psegment_data;
unsigned int last_segment_start_add;

unsigned int app_entry_add;
unsigned int i_code_length;
const unsigned int register1_add = 0xfc00d01c;
const unsigned int register2_add = 0xfc00d01d;
const unsigned int register3_add = 0xfc00d020;
const unsigned int register1_val = 0x11115678;
const unsigned int register2_val = 0x44445678;
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int MyMemcpy(void *des,void *src,int len)
{
	unsigned char *pdes = (unsigned char *)(des);
	unsigned char *psrc = (unsigned char *)(src);
    int i = 0;
	while(i < len)
	{
		*(pdes++) = *(psrc++);	
        i++;
	}	
	return i;
}

int UsbWrite(unsigned char *pbuf,int write_bytes)
{
	int ret;
	int printf_bytes;
	printf_bytes = write_bytes;
	if(printf_bytes > 16)
		printf_bytes = 16;
printf("%s->%d->%s::UsbWrite write_bytes::%08X,first 16 bytes::\r\n",__FILE__,__LINE__,__FUNCTION__,write_bytes);
for(ret = 0;ret < printf_bytes;ret++)
	printf("%02X\r\n",pbuf[ret]);
printf("\r\n");
printf("%s->%d->%s::UsbWrite last 4 bytes::%02X%02X%02X%02X\r\n",__FILE__,__LINE__,__FUNCTION__,pbuf[write_bytes - 4],pbuf[write_bytes - 3],pbuf[write_bytes - 2],pbuf[write_bytes - 1]);

	ret = write(usb_fd,pbuf,write_bytes);
	printf("%s->%d->%s::write usb data bytes::%d\r\n",__FILE__,__LINE__,__FUNCTION__,ret);
	if(ret != write_bytes)
		return -1;
	return write_bytes;
}
int UsbRead(unsigned char *pbuf,int buf_size)
{
	int rev = 0;
	rev = read(usb_fd,pbuf,buf_size);
	return rev;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
unsigned int BootAsGetSegment(unsigned int pfile_offset)
{
	unsigned char *p;
	unsigned int new_offset;
	int i;
	unsigned int t_segment_bytes,t_segment_start_add;
printf("%s->%d->%s::pfile_offset::%08X\r\n",__FILE__,__LINE__,__FUNCTION__,pfile_offset);
	p = (pdat_file + pfile_offset);
//printf("%s->%d->%s::first 16 byte::\r\n",__FILE__,__LINE__,__FUNCTION__);
//for(i = 0;i < 16;i++)
//printf("%02X\r\n",p[i]);
//printf("\r\n");
	
	((unsigned char *)(&t_segment_bytes))[0] = p[3];
	((unsigned char *)(&t_segment_bytes))[1] = p[2];
	((unsigned char *)(&t_segment_bytes))[2] = p[1];
	((unsigned char *)(&t_segment_bytes))[3] = p[0];

	p += INT_BYTES;
	((unsigned char *)(&t_segment_start_add))[0] = p[3];
	((unsigned char *)(&t_segment_start_add))[1] = p[2];
	((unsigned char *)(&t_segment_start_add))[2] = p[1];
	((unsigned char *)(&t_segment_start_add))[3] = p[0];
	
printf("%s->%d->%s::t_segment_bytes::%08X\r\n",__FILE__,__LINE__,__FUNCTION__,t_segment_bytes);
printf("%s->%d->%s::t_segment_start_add::%08X\r\n",__FILE__,__LINE__,__FUNCTION__,t_segment_start_add);
	if(t_segment_bytes == 0)
		return 0;
		
	new_offset = pfile_offset + t_segment_bytes + INT_BYTES + INT_BYTES;
printf("%s->%d->%s::new_offset::%08X\r\n",__FILE__,__LINE__,__FUNCTION__,new_offset);	
	if(new_offset > dat_file_size)
	{
printf("%s->%d->%s::new_offset::%08X is more than file size\r\n",__FILE__,__LINE__,__FUNCTION__,new_offset);		
		return 0;
	}
	segment_bytes = t_segment_bytes;
	segment_start_add = t_segment_start_add;
	psegment_data = (p + INT_BYTES);	
printf("%s->%d->%s::segment_bytes::%08X\r\n",__FILE__,__LINE__,__FUNCTION__,t_segment_bytes);
printf("%s->%d->%s::segment_start_add::%08X\r\n",__FILE__,__LINE__,__FUNCTION__,t_segment_start_add);
	return new_offset;
}
int BootAsDatFileCheck(void)
{
	if(dat_file_size < FIRST_SEGMENT_LEGNTH_OFFSET)
		return 0;
	return 1;
}

void BootAsCalSum(unsigned char *pd,int bytes)
{
	int i;
	for(i = 0;i < bytes;i++)
		data_sum += pd[i];
}
int BootAsTransPacket(unsigned char *pda,unsigned int add,int bytes)
{
	int ret = 0;
	int index = 0;
	unsigned short len;
	send_buf[index++] = FIX_HEAD_BYTE1;
	send_buf[index++] = FIX_HEAD_BYTE2;
	send_buf[index++] = FIX_CMD_CODE;
	send_buf[index++] = PARAM_DOWNLOAD_CODE;
	len = bytes + INT_BYTES;
	send_buf[index++] = ((unsigned char *)(&len))[1];
	send_buf[index++] = ((unsigned char *)(&len))[0];
	//发送命令
	ret = UsbWrite(send_buf,index);
	if(ret < 0)
		return -101;
//	usleep(50* 1000);
	index = 0;
	send_buf[index++] = ((unsigned char *)(&add))[3];
	send_buf[index++] = ((unsigned char *)(&add))[2];
	send_buf[index++] = ((unsigned char *)(&add))[1];
	send_buf[index++] = ((unsigned char *)(&add))[0];	
	MyMemcpy((void *)(&send_buf[index]),(void *)(pda),bytes);
	index += bytes;
	//计算校验和
	BootAsCalSum(send_buf,index);
	//发送数据
	ret = UsbWrite(send_buf,index);
	if(ret < 0)
		return -101;
	return 1;
}
int BootAsTransWaitRsp(void)
{
	int loops = 0;
	int recv_bytes;
	int data_bytes;
	recv_data_bytes = 0;
	do
	{
		usleep(10000);
		loops++;
		if(loops > 50)
		{
printf("%s->%d->%s::waiting time out!\r\n",__FILE__,__LINE__,__FUNCTION__);
			return -1;
		}
		recv_bytes = UsbRead((unsigned char *)(&recv_buf[recv_data_bytes]),sizeof(recv_buf) - recv_data_bytes);
printf("%s->%d->%s::Usbread return::%08X,data is::\r\n",__FILE__,__LINE__,__FUNCTION__,recv_bytes);	
		if(recv_bytes > 0)
		{
for(data_bytes = 0; data_bytes < recv_bytes;data_bytes++)
{
printf("%02X",recv_buf[recv_data_bytes + data_bytes]);		
}	
printf("\r\n");	
			recv_data_bytes += recv_bytes;
		}
		if(MIN_RSP_PACKET_BYTES <= recv_data_bytes)
		{
			if((recv_buf[0] != FIX_CMD_CODE) && (recv_buf[0] != ISPSTART_CMD_CODE))
				return -2;
			if(recv_buf[1] != OK)
				return -3;
			data_bytes = recv_buf[2] + recv_buf[3] * 256;
			if(recv_data_bytes >= (data_bytes + MIN_RSP_PACKET_BYTES))
				return 1;
		}
	}while(1);

	return 1;
}

int BootAsSendConfig(void)
{
	int ret = 0;
	int index = 0;
	unsigned int file_offset,new_offset;
	send_buf[index++] = FIX_HEAD_BYTE1;
	send_buf[index++] = FIX_HEAD_BYTE2;
	send_buf[index++] = FIX_CMD_CODE;
	send_buf[index++] = PARAM_SET_CODE;
	send_buf[index++] = FIX_CONFIG_CMD_PAD;
	send_buf[index++] = 0X00;
	//发送命令
	ret = UsbWrite(send_buf,index);
	if(ret < 0)
		return -101;
//	usleep(50 * 1000);
	//获取应用程序入口地址
	((unsigned char *)(&app_entry_add))[3] = pdat_file[APP_ENTRY_OFFSET];
	((unsigned char *)(&app_entry_add))[2] = pdat_file[APP_ENTRY_OFFSET + 1];
	((unsigned char *)(&app_entry_add))[1] = pdat_file[APP_ENTRY_OFFSET + 2];
	((unsigned char *)(&app_entry_add))[0] = pdat_file[APP_ENTRY_OFFSET + 3];
	//解析获取最后一段地址和长度
	file_offset = FIRST_SEGMENT_LEGNTH_OFFSET;
	while(file_offset < dat_file_size)
	{
		//解析segment的length以及数据长度
		new_offset = BootAsGetSegment(file_offset);
		if(new_offset == 0)
		{		
			break;
		}
		file_offset = new_offset;
	}	
	i_code_length = segment_start_add + segment_bytes;	
printf("%s->%d->%s::i_code_length::%08X,app_entry_add::%08X\r\n",__FILE__,__LINE__,__FUNCTION__,i_code_length,app_entry_add);	
	index = 0;
	send_buf[index++] = ((unsigned char *)(&app_entry_add))[3];
	send_buf[index++] = ((unsigned char *)(&app_entry_add))[2];
	send_buf[index++] = ((unsigned char *)(&app_entry_add))[1];
	send_buf[index++] = ((unsigned char *)(&app_entry_add))[0];
	send_buf[index++] = FIX_REG_COUNTS;
	send_buf[index++] = ((unsigned char *)(&register1_add))[3];
	send_buf[index++] = ((unsigned char *)(&register1_add))[2];
	send_buf[index++] = ((unsigned char *)(&register1_add))[1];
	send_buf[index++] = ((unsigned char *)(&register1_add))[0];
	send_buf[index++] = ((unsigned char *)(&register1_val))[3];
	send_buf[index++] = ((unsigned char *)(&register1_val))[2];
	send_buf[index++] = ((unsigned char *)(&register1_val))[1];
	send_buf[index++] = ((unsigned char *)(&register1_val))[0];
	send_buf[index++] = FIX_CONFIG_CMD_PAD;	
	send_buf[index++] = ((unsigned char *)(&register2_add))[3];
	send_buf[index++] = ((unsigned char *)(&register2_add))[2];
	send_buf[index++] = ((unsigned char *)(&register2_add))[1];
	send_buf[index++] = ((unsigned char *)(&register2_add))[0];
	send_buf[index++] = ((unsigned char *)(&register2_val))[3];
	send_buf[index++] = ((unsigned char *)(&register2_val))[2];
	send_buf[index++] = ((unsigned char *)(&register2_val))[1];
	send_buf[index++] = ((unsigned char *)(&register2_val))[0];
	send_buf[index++] = FIX_CONFIG_CMD_PAD;	
	send_buf[index++] = ((unsigned char *)(&register3_add))[3];
	send_buf[index++] = ((unsigned char *)(&register3_add))[2];
	send_buf[index++] = ((unsigned char *)(&register3_add))[1];
	send_buf[index++] = ((unsigned char *)(&register3_add))[0];
	send_buf[index++] = ((unsigned char *)(&i_code_length))[3];
	send_buf[index++] = ((unsigned char *)(&i_code_length))[2];
	send_buf[index++] = ((unsigned char *)(&i_code_length))[1];
	send_buf[index++] = ((unsigned char *)(&i_code_length))[0];
	send_buf[index++] = FIX_CONFIG_CMD_PAD;	
	//计算校验
	BootAsCalSum(send_buf,index);
	//发送数据
	ret = UsbWrite(send_buf,index);
	if(ret < 0)
		return -101;
	//等待响应
	ret= BootAsTransWaitRsp();
	if(ret != 1)
		return -102;
	return 1;				
}

int BootAsSegmentUpdate(void)
{
	int ret;
	unsigned int send_bytes = 0;
	unsigned int rest_bytes;
	do
	{
		rest_bytes = segment_bytes - send_bytes;
		if(rest_bytes > MAX_PACKET_DATA_BYTES)
			rest_bytes = MAX_PACKET_DATA_BYTES;
		//打包发送
		ret = BootAsTransPacket(psegment_data,segment_start_add,rest_bytes);
		if(ret != 1)
			return ret;		
		//接收等待
		ret = BootAsTransWaitRsp();
		if(ret != 1)
			return -102;
		segment_start_add += rest_bytes;
		send_bytes += rest_bytes;
		psegment_data += rest_bytes;
	}while(send_bytes < segment_bytes);
	last_segment_start_add = segment_start_add;
	return 1;
}
int BootAsSendData(void)
{
	int ret;
	unsigned int file_offset,new_offset;

	file_offset = FIRST_SEGMENT_LEGNTH_OFFSET;
	
	while(file_offset < dat_file_size)
	{
		usleep(10000);
		//解析segment的length以及数据长度
		new_offset = BootAsGetSegment(file_offset);
		if(new_offset == 0)
		{
printf("%s->%d->%s::BootAsSendData not get new segment!so data tranfser is over!\r\n",__FILE__,__LINE__,__FUNCTION__);			
			
			break;
		}
printf("%s->%d->%s::BootAsSendData get one new segment!,new offset::%08X;old_offset::%08X\r\n",__FILE__,__LINE__,__FUNCTION__,new_offset,file_offset);				
		ret = BootAsSegmentUpdate();
		if(ret != 1)
			return 0;
		file_offset = new_offset;
	}
	//判断后续是否还剩余有数据,如果有，则接着最后一个segment的地址一次全部写入
	if(file_offset < dat_file_size)
	{
		new_offset = dat_file_size - file_offset;
		//打包发送
printf("%s->%d->%s::last packet pure data bytes is::%08X\r\n",__FILE__,__LINE__,__FUNCTION__,new_offset);
printf("%s->%d->%s::will send last packet\r\n",__FILE__,__LINE__,__FUNCTION__,send_data_bytes);
		ret = BootAsTransPacket((unsigned char *)(&pdat_file[file_offset]),last_segment_start_add,new_offset);
		if(ret != 1)
			return ret;
		//接收等待
		ret = BootAsTransWaitRsp();
		if(ret != 1)
			return -102;
	}
	return 1;
}
int BootAsSendCheck(void)
{
	int index = 0;
	int ret;
	send_buf[index++] = FIX_HEAD_BYTE1;
	send_buf[index++] = FIX_HEAD_BYTE2;
	send_buf[index++] = FIX_CMD_CODE;
	send_buf[index++] = PARAM_CHECK_CODE;
	send_buf[index++] = FIX_CHECK_LC_LSB;
	send_buf[index++] = FIX_CHECK_LC_MSB;
	//发送命令
	ret = UsbWrite(send_buf,index);
	if(ret < 0)
		return -101;
//	usleep(50* 1000);
	index = 0;
	send_buf[index++] = ((unsigned char *)(&data_sum))[1];
	send_buf[index++] = ((unsigned char *)(&data_sum))[0];
	send_buf[index++] = RUN_AFTER_UPDATE;
	//发送数据
	ret = UsbWrite(send_buf,index);
	if(ret < 0)
		return -101;
	//等待响应
	ret= BootAsTransWaitRsp();
	if(ret != 1)
		return -102;
	return 1;
}
int BootAsSendIspStart(void)
{
	int index = 0;
	int ret;
	send_buf[index++] = FIX_HEAD_BYTE1;
	send_buf[index++] = FIX_HEAD_BYTE2;
	send_buf[index++] = ISPSTART_CMD_CODE;
	send_buf[index++] = 0X00;
	send_buf[index++] = 0X00;
	send_buf[index++] = 0X00;
	//发送命令
	ret = UsbWrite(send_buf,index);
	if(ret < 0)
		return -101;	
	//等待响应
	ret= BootAsTransWaitRsp();
	if(ret != 1)
		return -102;		
	data_sum = 0;
	return 1;	
}

int LoadDatFile(void)
{
	FILE *pfile;
	int ret,read_bytes;

	pfile = fopen(DATNAME,"rb");
	if(pfile == 0)
	{
		printf("%s->%d->%s::Load dat file failed!\r\n",__FILE__,__LINE__,__FUNCTION__);
		return 0;
	}
	read_bytes = 0;
	do
	{
		ret = fread((void *)(&dat_file_buf[read_bytes]),1,1024,pfile);
		if(ret < 1)
			break;
		read_bytes += ret;
	}while(1);
	pdat_file = dat_file_buf;
	dat_file_size = read_bytes;
	printf("%s->%d->%s::load dat file ok,file size::%d\r\n",__FILE__,__LINE__,__FUNCTION__,dat_file_size);
	
	return 1;
}

/*
int main(int argc, char* argv[])
{
	int ret;
	
	usb_fd = open("/dev/MyModule", O_RDWR);
	if(usb_fd <= 0 )
	{
		printf("%s->%d->%s::error  111111111\n");
		return -1;
	}
	printf("%s->%d->%s::======open MyModule file usb_fd = 0x%x======\r\n",__FILE__,__LINE__,__FUNCTION__,usb_fd);
	
	usleep(50000);
	
	ret = LoadDatFile();
	if(ret != 1)
	{
		printf("%s->%d->%s::update as532 failed,cause load dat file err!\r\n",__FILE__,__LINE__,__FUNCTION__);
		getchar();
		return -1;
	}
	
	ret = BootAsDatFileCheck();
	if(ret != 1)
	{
		printf("%s->%d->%s::update as532 failed,cause dat file err!\r\n",__FILE__,__LINE__,__FUNCTION__);
		getchar();
		return -1;
	}

	ret = BootAsSendIspStart();
	if(ret != 1)
	{
		printf("%s->%d->%s::send isp start command,but not get response!\r\n",__FILE__,__LINE__,__FUNCTION__);	
		getchar();
		return -6;			
	}
	usleep(10000);
	ret = BootAsSendConfig();
	if(ret != 1)
	{
		printf("%s->%d->%s::send config command,but not get response!\r\n",__FILE__,__LINE__,__FUNCTION__);	
		getchar();
		return -6;				
	}
	ret = BootAsSendData();
	if(ret != 1)
	{
		printf("%s->%d->%s::update as532 failed,cause transefer data err!\r\n",__FILE__,__LINE__,__FUNCTION__);
		getchar();
		return -2;
	}
	ret = BootAsSendCheck();
	if(ret != 1)
	{
		printf("%s->%d->%s::update as532 failed,cause check err!\r\n",__FILE__,__LINE__,__FUNCTION__);
		getchar();
		return -3;
	}
	getchar();
	close(usb_fd);
	printf("%s->%d->%s::update as532 Over!!!\r\n",__FILE__,__LINE__,__FUNCTION__);
	printf("%s->%d->%s::update as532 Over!!!\r\n",__FILE__,__LINE__,__FUNCTION__);
	printf("%s->%d->%s::update as532 Over!!!\r\n",__FILE__,__LINE__,__FUNCTION__);
	
	
	return 0;
}
*/

