
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include "our_md5.h"
#include  <limits.h>
#include <sys/statfs.h>
#include <sys/un.h>  
#include <unistd.h>  
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>

/************************************************************************
 * define our constant variables
 ***********************************************************************/
#define VERSIONS	"1.0V"
#define PORT		63000
//#define LENGTH		1024*1024
#define LENGTH		1050
//#define LENGTH		1024+50
#define WRITE_LOW_COMMUNICATE		"/tmp/WRITE_LOW_COMMUNICATE"
#define READ_LOW_COMMUNICATE		"/tmp/READ_LOW_COMMUNICATE"

#define STM32_FILE		"/tmp/STM32.zip"
#define AS532_FILE		"/tmp/AS532.zip"
#define AR9344_FILE		"/tmp/AR9344.zip"

static int Fd_WRITE;
static int Fd_READ;
typedef unsigned char  uchar;              //鏃犵鍙峰瓧绗
static int fileFd;
static int clientFd;
static char ack_buf[256];
static char head[3] = {0x4f,0x54,0x41};
static char version = 0x31;
static char version_num[256]; 
static char Serial_Number[256];
static int local_Fd,servFd;
static int runcond = 1; //循环条件
int send_len;

void printInfo()
{
	printf(
			"\n##############################################################\n");
	printf("\t\t\tAuthor \t:changyongjin\n");
	printf("\t\t\tVersion\t:%s\n", VERSIONS);
	printf("\t\t\tTime   \t:%s-%s", __DATE__, __TIME__);
	printf(
			"\n##############################################################\n");

}

void stophandler(int signum) //按下Ctrl+C的时候触发该函数
{	
	runcond = 0; //终止主循环，退出程序
	printf("exit the loop\n");
//	close(clientFd);
//	close(servFd);
}


int timing(int fd)
{
	fd_set rdfds;
	struct timeval tv;
	int ret;
	FD_ZERO(&rdfds);    
	FD_SET(fd, &rdfds);   
	tv.tv_sec = 1;
	tv.tv_usec = 0;      
	ret = select(fd+1, &rdfds, NULL, NULL, &tv);
	FD_ZERO(&rdfds);
	return ret;
}




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

char Array_Update(char *data, int length)
{
	char crc = 0x0 ;
	int i;
	for (i=0;i<length;i++)
	{
		crc = CRC8_TABLE[(crc ^ data[i])&0xFF];
	}
	return crc;
}	
	



/************************************************************************
 * describe 	: create Server over TCP
 * parameters	: port:used by TCP
 * return value	: socket FD
 ***********************************************************************/

int creatTCPServ(short port)
{
	int fd = -1, res;
	int yes = 1;
	struct sockaddr_in servaddr;
	socklen_t len = sizeof(servaddr);

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		return -1;
	}

	memset(&servaddr, 0, len);
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = INADDR_ANY;

	if (bind(fd, (struct sockaddr *) &servaddr, len) == -1)
	{
		perror("bind");
		return -1;
	}
	res = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	if (res < 0)
	{
		perror("Setsockopt");
		return -1;
	}

	
	return fd;
}

int Get_Version_Num()
{	
	FILE *fp;
	int len;
	fp = fopen("/tmp/.apcfg", "r");
//	fp = fopen("/etc/version", "r");
	char buf[100];
	if(fp != NULL)
	{
		
		while(fgets(buf, 100, fp) != NULL)
		{
			if(strstr(buf, "SOFT_VERSION") != 0)
			{
				sscanf(buf, "SOFT_VERSION=%s", version_num);
				printf("cyj  %s  %d  version_num = %s\n",__func__,__LINE__,version_num);
				len = strlen(version_num);
				fclose(fp);
				return len;
			}
		}
		fclose(fp);
	}
	return -1;
}

int Get_Serial_Number()
{

	FILE *fp;
//	fp = fopen("/etc/num", "r");
	fp = fopen("/proc/cmdline", "r");
	char buf[50];
	char buf1[100];
	char buf2[100];
	int len = 0;
	if(fp != NULL)
	{

			while(fgets(buf, 50, fp) != NULL)
			{
					if(strstr(buf, "SN") != 0)
					{
							sscanf(buf, "%s SN=%s %s", buf1, Serial_Number, buf2);
							printf("cyj  %s  %d  SN = %s\n",__func__,__LINE__,Serial_Number);
							len = strlen(Serial_Number);
							fclose(fp);
							return len;
					}
			}
			fclose(fp);
	}
	return -1;
	
}
	





//pack:[head,version,command,Device_ID,data_len,len_complement,pack_num,data_buf,check]

int DH_GetDiskfreeSpace()
{
	long long freespace = 0;
	int len;
	struct statfs disk_statfs;

	
	if( statfs("/tmp", &disk_statfs) >= 0 )
		freespace = (((long long)disk_statfs.f_bsize * (long long)disk_statfs.f_bfree)/((long long)1024*(long long)1024));
		len = (int)freespace;
		printf("freespace = %d\n",len);
	return len;
}


int  Compose_Pack(char command,short int pack_num,char *ack_data,int lenth, char device_id)
{
	int ack_len =  htonl(lenth);
	//int i;
	char check = 0x0;
	char pre_data[lenth+13];
	memcpy(ack_buf,head,3); 					
	memcpy(ack_buf+3,&version,1);
	memcpy(ack_buf+4,&command,1);
	memcpy(ack_buf+5,&device_id,1);
	memcpy(ack_buf+6,&ack_len,4);
	memcpy(ack_buf+10,&ack_len,4);
	memcpy(ack_buf+14,&pack_num,2);
	memcpy(ack_buf+16,ack_data,lenth);
	memcpy(pre_data,ack_buf+3,lenth+13);
	check = Array_Update(pre_data,lenth+13);
	memcpy(ack_buf+lenth+16,&check,1);
	return lenth+17;			
}

int Save_Img(char *recv_buf,int data_len)
{
	char dataBuf[LENGTH];
	int wLen;
	char ack_data;
	memcpy(dataBuf,recv_buf + 16,data_len);
	wLen = write(fileFd, dataBuf, data_len);
	if (data_len != wLen)
	{
		perror("write file data");
		close(fileFd);
		return -1;
	}
	printf("###########\n");

	return 0;
}


int Md5_Check(char *recv_buf )
{
	int ret;
	char rear_md5[33];
	char pre_md5[33];
	memcpy(pre_md5,recv_buf +16,32 );
	pre_md5[32] = '\0';
	switch(recv_buf[5])
	{
		case 0x11:
			CalcFileMD5(STM32_FILE,rear_md5);
			break;
		case 0x12:
			CalcFileMD5(AS532_FILE,rear_md5);
			break;
		case 0x13:
			CalcFileMD5(AR9344_FILE,rear_md5);
			break;
		default:
			break;
	}
	rear_md5[32] = '\0';
//	printf("pre_md5 = %s\n",pre_md5);
//	printf("rear_md5 = %s\n",rear_md5);
	ret = strncmp(rear_md5,pre_md5,32);
	if(ret == 0)
	{
		printf(" md5 check right!!!!!!\n");
		return 1;
	}
	else 
	{	
		printf(" md5  error correcting code\n");
/*		switch(recv_buf[5])
		{
			case 0x11:
				ret = unlink(STM32_FILE);
				break;
			case 0x12:
				ret = unlink(AS532_FILE);
				break;
			case 0x13:
				ret = unlink(AR9344_FILE);
				break;
			default:
				break;
		}
		if(ret != 0)
		{
			printf("unlink ERRO!!!!!\n");
		}	
*/
		return 0;
	}
	
}

int Local_Communicate(char *recv_buf, int data_len)
{
	int timing_return;
	int num;
/*	printf("send STM32=");
	for(num = 0; num < data_len; num++)
		printf("%x  ",recv_buf[num]);
		printf("%\n\n");
*/	write(Fd_WRITE, recv_buf, data_len);

	timing_return = timing(Fd_READ);
	if(timing_return == 0)
	{
		printf("Fd_READ timeout!!!\n");
		return -1;
	}
	else if(timing_return < 0)
	{
		printf("select error !!!!\n");
		return -1;
	}
	else
	{
		memset(ack_buf, 0, 256);
		send_len = read(Fd_READ, ack_buf, 256);
	}
	return 0;

}

void Detection_Version(char *recv_buf, int data_len)
{
	int len;
	switch(recv_buf[5])  //设备代码
	{
		case 0x11:	//STM32	
		case 0x12:	//AS532
			if(Local_Communicate(recv_buf, data_len) == 0)
			{
				send(clientFd, ack_buf, send_len, 0);
		/*		printf("send PAD= ");
				for(len = 0; len < send_len; len++)
					printf("%x  ", ack_buf[len]);
				printf("\n\n\n");
		*/	}
			else
			{
			
			}

			break;
		case 0x13:	//9344
			if((len = Get_Version_Num()) != -1)
			{
                memset(version_num, 0, 256);
	            memcpy(version_num,"HBD_F2A_B6_V8.2.12_20140814_A",len);
				send_len = Compose_Pack(0x02, 0x00, version_num, len, 0x13);
				send(clientFd, ack_buf, send_len, 0);
			}
			break;
		default:
			break;
	}	

}

void Detection_Serial_Number(char *recv_buf, int data_len)
{
	int len;
	switch(recv_buf[5])  //设备代码
	{
		case 0x11:	//STM32	
		case 0x12:	//AS532
			if(Local_Communicate(recv_buf, data_len) == 0)
			{
				send(clientFd,ack_buf,send_len,0);
		/*		printf("send PAD= ");
				for(len = 0; len < send_len; len++)
					printf("%x  ", ack_buf[len]);
				printf("\n\n\n");
		*/	}
			else
			{
			
			}
			break;
		case 0x13:	//9344
			if((len =Get_Serial_Number()) != -1)
			{
				send_len = Compose_Pack(0x22, 0x00, Serial_Number, len, 0x13);
				send(clientFd, ack_buf, send_len, 0);
			}
			break;
		default:
			break;
	}	


}

int Create_Upgrade_File(char *recv_buf)
{
	switch(recv_buf[5])  //设备代码
	{
		case 0x11:	//STM32	
			fileFd = open(STM32_FILE, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
			perror("open");
			if (fileFd == -1)
			{
				close(fileFd);
				return 0x02;
			}
			else
			{
				return 0x00;
			}
			break;
		case 0x12:	//AS532 
			fileFd = open(AS532_FILE, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
			if (fileFd == -1)
			{
				close(fileFd);
				return 0x02;
			}
			else
			{
				return 0x00;
			}
			break;
		case 0x13:	//9344
			fileFd = open(AR9344_FILE, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
			if (fileFd == -1)
			{
				close(fileFd);
				return 0x02;
			}
			else
			{
				return 0x00;
			}
			break;
		default:
			close(fileFd);
			return 0x02;
			break;
	}
}


int Notice_System_Updata(char *recv_buf,int data_len)
{
	int len;
	char updata_order[50];
	sprintf(updata_order, "sysupgrade  %s", AR9344_FILE);
	switch(recv_buf[5])  //设备代码
	{
		case 0x11:	//STM32	
		case 0x12:	//AS532
			printf(" System Updata!!!\n");
			if(Local_Communicate(recv_buf, data_len) == 0)
			{

			}
			else
			{
			
			}
			break;
		case 0x13:	//9344
			printf("9344 System Updata!!!\n");
			printf("updata_order = %s\n", updata_order);
	//		system(updata_order);
			system("sysupgrade  /tmp/AR9344.zip");
			break;
		default:
			break;
	}	
	 
}

void Parse_Pack(char *recv_buf,int valid_data_len, int data_len)
{	
	char ack_data;
	int pack_size = 0;
	int get_len;
	printf("order %x\n", recv_buf[4]);
	printf("recv_buf[5] =  %x\n", recv_buf[5]);
	switch(recv_buf[4])
	{	
		case 0x01://请求检查版本号
			Detection_Version(recv_buf, data_len);
			close(clientFd);
			clientFd = -1;
			break;
		case 0x21://请求设备序列号
			Detection_Serial_Number(recv_buf, data_len);
			close(clientFd);
			clientFd = -1;
			break;
		case 0x31://准备接收数据包
			ack_data = Create_Upgrade_File(recv_buf);
			Compose_Pack(0x32,0x00,&ack_data,1, recv_buf[5]);
			send(clientFd,ack_buf,18,0);
			break;
		case 0x33://数据包分发
			if(Save_Img(recv_buf, valid_data_len) != 0)
			{
				//写文件出问题
				ack_data = 0x01;
				send_len = Compose_Pack(0x34, 0x00, &ack_data, 1, recv_buf[5]);
				send(clientFd,ack_buf,send_len,0);
			}
			break;
		case 0x35://发送数据包结束标示
			close(fileFd);
			ack_data = 0x00;
			Compose_Pack(0x36,0x00,&ack_data,1, recv_buf[5]);
			send(clientFd,ack_buf,18,0);
			break;
		case 0x37:
			if( Md5_Check(recv_buf)>0)
			{
				ack_data = 0x01;
				Compose_Pack(0x38, 0x00, &ack_data, 1, recv_buf[5]);
				send(clientFd,ack_buf,18,0);
				printf("%s   %d   MD5  OK\n",__func__,__LINE__);
			}
			else
			{
				ack_data = 0x00;
				Compose_Pack(0x38, 0x00, &ack_data, 1, recv_buf[5]);
				send(clientFd,ack_buf,18,0);
				printf("%s   %d  no  MD5  OK\n",__func__,__LINE__);
			}
			break;
		case 0x41://通知升级
			close(clientFd);
			clientFd = -1;
			Notice_System_Updata(recv_buf, data_len);
			break;
		default:
			close(clientFd);
			clientFd = -1;
			printf("order	error!!!\n");
			break;
	}
}

int Check_Pack(char *recv_buf,int valid_data_len, int data_len)
{
	int check = 0x0;
	char ack_data;
	int get_len;
	if(data_len > (valid_data_len+17))
	{

		printf("long	error!!!\n");	
		ack_data = 0x00;
		Compose_Pack(0x34,0x00,&ack_data,1, recv_buf[5]);
		send(clientFd,ack_buf,18,0);
		return 0;
	}

	
	check = Array_Update(recv_buf+3,valid_data_len + 13);

//	printf("check = %x  recv_buf[valid_data_len + 16] = %x\n", check, recv_buf[valid_data_len + 16]);
	if(recv_buf[valid_data_len + 16] == check )
	{
		if(recv_buf[4] == 0x33)
		{
			ack_data = 0x01;
			send_len = Compose_Pack(0x34,0x00,&ack_data,1, recv_buf[5]);
			send(clientFd,ack_buf,send_len,0);
	//		for(get_len = 0; get_len < send_len; get_len++)
	//			printf("  %x   ", ack_buf[get_len]);
	//				printf("\n\n\n");
		}
		Parse_Pack(recv_buf, valid_data_len, data_len);	
	}
	else
	{	
		printf("check	error!!!\n");	
		ack_data = 0x00;
		Compose_Pack(0x34,0x00,&ack_data,1, recv_buf[5]);
		send(clientFd,ack_buf,18,0);
	}
	return 0;
}



void Low_Communicate_Init()
{
	int timing_return;
	if(access(WRITE_LOW_COMMUNICATE, F_OK) == -1)		//创建管道
	{
		mkfifo(WRITE_LOW_COMMUNICATE, 0666);
	}
	if(access(READ_LOW_COMMUNICATE, F_OK) == -1)		//创建管道
	{
		mkfifo(READ_LOW_COMMUNICATE, 0666);
	}
	Fd_WRITE = open(WRITE_LOW_COMMUNICATE, O_RDWR);
	if(Fd_WRITE < 0)
	{
		printf("WRITE_LOW_COMMUNICATE is not open!!!\n");
		exit(1);
	}
	Fd_READ = open(READ_LOW_COMMUNICATE, O_RDWR);
	if(Fd_READ < 0)
	{
		printf("READ_LOW_COMMUNICATE is not open!!!\n");
		exit(1);
	}
	timing_return = timing(Fd_READ);
	if(timing_return == 0)
	{
		printf("Fd_READ timeout!!!\n");
	}
	else if(timing_return < 0)
	{
		printf("select error !!!!\n");
	}
	else
	{
		memset(ack_buf, 0, 256);
		 read(Fd_READ, ack_buf, 256);
	}
}

void Dispose_Pthread()
{
	int recvLen,total;
	int valid_data_len;

	while(clientFd > 0)
	{
		total = 0;
		char recv_buf[LENGTH+17];
		memset(recv_buf, 0, LENGTH+17);
		do
		{
			recvLen = recv(clientFd, recv_buf + total, LENGTH + 17- total, 0);
			memcpy(&valid_data_len,recv_buf+6,4);
			total += recvLen;
			if(valid_data_len > LENGTH)
			{
			//	close(clientFd);
			//	clientFd = -1;
				printf("valid_data_len = %x\n", valid_data_len);
				total =  valid_data_len+20;
			
			}
			if(valid_data_len < 1)
			{
				
				printf("valid_data_len = %x\n", valid_data_len);
				total =  valid_data_len+20;
				close(clientFd);
				clientFd = -1;

			}
		}while(total < valid_data_len+17);
		Check_Pack(recv_buf, valid_data_len, total);
	}
}

int main(int argc, char **argv)
{
	
	int recvLen,total,ret;
	int valid_data_len;
	char recv_buf[LENGTH+17];
	pthread_t	pthread;
//	printInfo();


	Low_Communicate_Init();

//	signal(SIGINT, stophandler);
	servFd = creatTCPServ(PORT);
	if (listen(servFd, 10) == -1)
	{
		perror("listen");
		exit(1);
	}

	while(1)
	{
		clientFd = accept(servFd, NULL, NULL );
//		Dispose_Pthread();		
		pthread_create(&pthread, NULL, (void *)Dispose_Pthread, NULL);
	}
	pthread_join(pthread, NULL);
	return 0;
}
