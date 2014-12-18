/* vi: set sw=4 ts=4: */
/*
 * Mini reboot implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <stdio.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>  //stat函数
#include <sys/types.h>
#include <sys/wait.h>
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

#define UARTNAME	"/dev/ttyS1"
#define STM32_DOWN_HEAD_BYTE1_1     0XEF
#define STM32_DOWN_HEAD_BYTE2_1     0XFE
#define STM32_DOWN_HEAD_BYTE3_1     0XA5
#define STM32_DOWN_HEAD_BYTE4_1     0X5A
#define TYPE_REBOOT_PASSAGE			0x60
#define RSP_CMD_MIN_BYTES 9

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

int generate_stm32_down_msg(int *global_uart_fd,char *databuf,int databuf_len,int passage)
{
	int index = 0;
	char *sendbuf = malloc(databuf_len+16);
	if(sendbuf == NULL)
	{
		printf("malloc error\n");
		return -1;
	}
	printf("malloc size : %d\n",databuf_len+16);
	//int ret = 0;
	sendbuf[index++] = STM32_DOWN_HEAD_BYTE1_1;
	sendbuf[index++] = STM32_DOWN_HEAD_BYTE2_1;
	sendbuf[index++] = STM32_DOWN_HEAD_BYTE3_1;
	sendbuf[index++] = STM32_DOWN_HEAD_BYTE4_1;
	sendbuf[index++] = (((databuf_len+1) >> 8) & 0XFF);
	sendbuf[index++] = ((databuf_len+1) & 0XFF);
	sendbuf[index++] = (passage&0xFF); //通道
	memcpy(&sendbuf[index],databuf,databuf_len);
	index += databuf_len;
	sendbuf[index++] = sumxor(sendbuf,7+databuf_len);
	if(*global_uart_fd < 0)
	{
		free(sendbuf);
		return -1;
	}
	else
	{
		if(write(*global_uart_fd,sendbuf,index) != index)
		{
			free(sendbuf);
			return -1;
		}
		printf("ok\n");
	}
	free(sendbuf);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static int PacketHeadSearch(unsigned char *pbuf,int offset,int valid_bytes)
{
	int bytes;
	unsigned char *ppcom;
	int i,j;
	unsigned char packet_rsp_head_1[4] = {0xEA,0xFF,0x5A,0xA5};
	unsigned char packet_rsp_head_2[4] = {0xFF,0xEA,0x5A,0xA5};
	ppcom = pbuf;
	bytes = valid_bytes - offset;
	if(bytes < RSP_CMD_MIN_BYTES){
//		printf("Error: respond length <　RSP_CMD_MIN_BYTES \n");					
		return -1;
	}
	j = valid_bytes - RSP_CMD_MIN_BYTES;
	for(i = offset;i <= j;i++)
	{		
		if((ppcom[i] == packet_rsp_head_1[0]) && (ppcom[i + 1] == packet_rsp_head_1[1]) && 
			(ppcom[i + 2] == packet_rsp_head_1[2]) && (ppcom[i + 3] == packet_rsp_head_1[3]))
		{
			return i;	
		}
		if((ppcom[i] == packet_rsp_head_2[0]) && (ppcom[i + 1] == packet_rsp_head_2[1]) && 
			(ppcom[i + 2] == packet_rsp_head_2[2]) && (ppcom[i + 3] == packet_rsp_head_2[3])
			&& ((ppcom[i + 6]) != (unsigned char)0x84))
		{
			return i;	
		}
	}
	printf("Error: respond packet head not found \n");	
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
int PacketRspGet(unsigned char *pbuf,int offset,int valid_bytes)
{
	unsigned short packet_len;
	unsigned char check_byte;


	packet_len = (pbuf[offset+4] * 256) + pbuf[offset+5];
	packet_len += (4 + 2);
	if(valid_bytes <= packet_len){
		printf("Error: respond valid_bytes < packet_len\n");
		return -1;
	}
	check_byte = sumxor(pbuf+offset,packet_len);
	if(check_byte == pbuf[packet_len+offset])
	{
		packet_len++;
		return packet_len;
	}
	printf("Error: respond xor check error \n");
	return -1;
}

int UpdateRspRcv(int *global_uart_fd,unsigned char *prcv_buffer,int buf_size)
{
	int rcv_index;
	int i,j;
	int ret = 0;
	int loops = 0;
	rcv_index = 0;
	do
	{
		loops++;
		if(loops > 20){
			printf("Error: get respond timeout \n");
			return -1;
		}
//		printf("get respond .... \n");

		usleep(50 * 1000);
//		ret = read_data(global_uart_fd, prcv_buffer, (buf_size - rcv_index));
		ret = read(*global_uart_fd, (prcv_buffer + rcv_index), (buf_size - rcv_index));
		printf("ret = %d\n",ret);
//		for(i=0; i<ret; i++)
//			   printf("read_data ret = %x \n", prcv_buffer[i]);
		if(ret > 0)
		{
			rcv_index += ret;
		}

		if(rcv_index > 0)
		{
			i = PacketHeadSearch(prcv_buffer,0,rcv_index);
			if(i > -1)
			{
				j = PacketRspGet(prcv_buffer,i,rcv_index);	
				if(j > 1)
				{
					for(ret = 0;ret < j;ret++)
					{
						prcv_buffer[ret] = prcv_buffer[i + ret];
					}
					return j;
				}
				else if(j == -1)
					//return j;
					continue;
			}
		}
	}while(1);
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

extern int power_down_a20()
{
	int ret,global_uart_fd;
	char data;
	unsigned char rsp_buffer[128] = {0};
	system("killall check_phone_status");
	system("killall module_server");
	usleep(100*1000);
	global_uart_fd = open(UARTNAME,O_RDWR | O_NONBLOCK);
	if(global_uart_fd < 0)
	{
		printf("open uart error\n");
		return -1;
	}
	else
	{
		serialConfig(global_uart_fd,B115200);
		//printf("open uart success\n");
		data = 1;
		ret = generate_stm32_down_msg(&global_uart_fd,&data,1,TYPE_REBOOT_PASSAGE);
		/*
		if(ret == 0)
		{
			ret = UpdateRspRcv(&global_uart_fd,rsp_buffer,sizeof(rsp_buffer));
			//解析响应
			if(ret >= RSP_CMD_MIN_BYTES)
			{
				// xx xx xx xx 11 11 60 01 xx
				//判断响应
				if((rsp_buffer[6] != (char)TYPE_REBOOT_PASSAGE) || (rsp_buffer[7] != 0x01))
				{
					printf("%s command respond error \n", __FUNCTION__);
					close(global_uart_fd);
					return -1;				
				}
				printf("success\n");	
				close(global_uart_fd);
				return 0;
			}
			close(global_uart_fd);
			return -1;
		}
		* */
		close(global_uart_fd);
		return -1;
	}
}


int main(int argc, char **argv)
{
#if defined(B6) || defined(B6L)
	printf("b6 or b6l ,exit..\n");
	return 0;
#endif
	power_down_a20();
	sleep(1);
	power_down_a20();
	return 0;
}

