#ifndef _PHONE_SPI_TO_UART_H_
#define _PHONE_SPI_TO_UART_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>

#define SPI_NAME	"/dev/spiS0"

int fd;				/* open file fd */
#define NUMBER 256	/* buf size */

//	receive data buf
unsigned char out_data1[NUMBER];
unsigned char out_data2[NUMBER];
unsigned char out_data3[NUMBER];
//	three serial receive data count value
static int uart1_count;
static int uart2_count;
static int uart3_count;

#ifdef __cplusplus
extern "C" {
#endif




/************************************************
 **	argument:
 *		num:0 :	Broadcasting
 *			1 :	serial 1 
 *			2 : serial 2
 *			3 : serial 3
 *		data:
 *			send's data
 *		count:
 *			send's data count
 *
 * *********************************************/
int send_data(int num, unsigned char *data, int count)
{
	int i;
	int send_count = 0;
	unsigned char uart_no;
	unsigned char add_data[NUMBER];
	
	if(count > 128)
	{
		printf("Send data long, only send 128Byte data!\n");
		count = 128;
	}
	if((num >3) || (num < 0) )
	{
		printf("send_data first parameter error!\n");
		return -1;
	}

	if(num == 0)
		uart_no = 0x40;
	else if(num == 1)
		uart_no = 0x10;
	else if(num == 2)
		uart_no = 0x20;
	else if(num == 3)
		uart_no = 0x30;

	for(i = 0; i < (count * 2); i++ )
	{
		if(i%2 == 0)
		{
			add_data[i] = uart_no; 
		}
		else
		{
			add_data[i] = data[i/2];
		}
	}

	send_count = write(fd, &add_data, (count * 2));
	
	return send_count;
}

/***********************************************************
 *	read_data
 *	read data from stm32 3 uart port
 *	return :
 *		return read real data 
 * ********************************************************/
int read_data(void)
{
	int i;
	int count;
	unsigned char out_data[256];

	uart1_count = 0;
	uart2_count = 0;
	uart3_count = 0;

	count = read(fd, &out_data, 256);
	if( count % 2 != 0 )
	{
		printf("app read data error!\n");
	}
	else if(count == 0)
	{
		return 0;
	}
	else
	{
		for(i = 0; i < (count / 2); i++)
		{
			if(out_data[i*2] == 0x10)
			{
				out_data1[uart1_count++] = out_data[i * 2 +1];
			}
			else if(out_data[i*2] == 0x20)
			{
				out_data2[uart2_count++] = out_data[i * 2 +1];
			}
			else if(out_data[i*2] == 0x30)
			{
				out_data3[uart3_count++] = out_data[i * 2 +1];
			}
		}
	}
	count = uart1_count + uart2_count + uart3_count;

	return count;
}

void print_recv(void)
{
	int i;

	/* print serial 1 receive data*/
	if(uart1_count !=0)
	{
		printf("print serial 1 9600 receive data:\n");
		for(i = 0; i < uart1_count; i++)
		{
			printf("0x%x\t", out_data1[i]);
		}
		printf("\n");
	}

	/* print serial 2 receive data*/
	if(uart2_count !=0)
	{
		printf("print serial 2 4800 receive data:\n");
		for(i = 0; i < uart2_count; i++)
		{
			printf("0x%x\t", out_data2[i]);
		}
		printf("\n");
	}

	/* print serial 3 receive data*/
	if(uart3_count !=0)
	{
		printf("print serial 3 9600 receive data:\n");
		for(i = 0; i < uart3_count; i++)
		{
			printf("0x%x\t", out_data3[i]);
		}
		printf("\n");
	}
}

#ifdef __cplusplus
}
#endif




#ifdef __TEST_SPI_TO_UART__
int main(int argc, char* argv[])
{
	int command;
	int count ;
	/* send data  */
	unsigned char uart1_data[3]		= {0x11, 0x22, 0x33};
	//unsigned char uart2_data[3]		= {0x44, 0x55, 0x66};
	//unsigned char uart3_data[3]		= {0x77, 0x88, 0x99};
	//unsigned char broadcast_data[3]	= {0x12, 0x34, 0x56};
	
	fd = open( SPI_NAME, O_RDWR);
	if (fd < 0)
	{
		perror("failed to open /dev/spiS0\n");
		exit(1);
	}
	while(1)
	{
			printf("send data!\n");
			send_data(1, uart1_data, sizeof(uart1_data));
		//	send_data(2, uart2_data, sizeof(uart2_data));
		//	send_data(3, uart3_data, sizeof(uart3_data));
		//	send_data(0, broadcast_data, sizeof(broadcast_data));
			printf("read data!\n");
			count = read_data();
			if(count > 0){
				print_recv();
			}
			
	}

	close(fd);

	return 0;
}

#endif

#endif
