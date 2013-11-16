#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>

#define SPI_NAME	"/dev/spiS0"


int charToInt(char a)
{
	int tmp;			
	if((a>='A')&&(a<='F') )
	{
		tmp=(int)a-55;
	}else if ((a>='a')&&(a<='f'))
	{
		tmp=(int)a-87;	
	}else if((a>='0')&&(a<='9'))
	{
		tmp=(int)a-48;
	}	
	return tmp;	
}


int main(int argc, char* argv[])
{
	int read_count ;
	int write_count = 0;
	int fd;
	int i;
	
	unsigned char data[256];
	/* send data  */
	unsigned char test_stm32[7]	= {0xa5,0x5a,0x00,0x03,0x40,0x00,0x43};
	unsigned char send_cmd_ht95r54[11]={0xA5,0x5A,0x00,0x07,0x10,0xA5,0x5A,0x5E,0x00,0x5E,0xE8};
	unsigned char send_cmd_stc[12]={0xA5,0x5A,0x00,0x08,0x01,0xA5,0x06,0x03,0x65,0x00,0x00,0x60};
	
	fd = open( SPI_NAME, O_RDWR);
	if (fd < 0)
	{
		perror("failed to open /dev/spiS0\n");
		exit(1);
	}
	
	switch(atoi(argv[1]))
	{
		case 1:
			write_count = write(fd, &test_stm32, sizeof(test_stm32));
			break;
		case 2:
			write_count = write(fd, &send_cmd_ht95r54, sizeof(send_cmd_ht95r54));
			break;
		case 3:
			write_count = write(fd, &send_cmd_stc, sizeof(send_cmd_stc));
			break;
		case 4:
			for(i=0;i<argc-2;i++)
			{
				data[i]=((charToInt(argv[i+2][0])&0x0f)<<4)|(charToInt(argv[i+2][1])&0x0f);
			}
			data[argc-2]='\0';	
			write_count = write(fd, &data, argc-2);
			break;
		case 5:
			while(1)
			{
				write_count = write(fd, &send_cmd_ht95r54, sizeof(send_cmd_ht95r54));
				usleep(100000);
			}
			break;
	}

	close(fd);

	return 0;
}
