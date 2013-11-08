#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>

#define SPI_NAME	"/dev/spiS0"


int main(int argc, char* argv[])
{
	int read_count ;
	int write_count = 0;
	int fd;
	int i;
	
	unsigned char read_data[256];
	/* send data  */
	unsigned char test_stm32[7]	= {0xa5,0x5a,0x00,0x03,0x40,0x00,0x43};
	unsigned char send_cmd_ht95r54[11]={0xA5,0x5A,0x00,0x07,0x10,0xA5,0x5A,0x5E,0x00,0x5E,0xE8};
	
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
			write_count = write(fd, &argv[2], sizeof(argv[2]));
			break;
		case 4:
			memset(read_data,0,sizeof(read_data));
			read_count = read(fd, &read_data, 256);
			for(i=0;i<sizeof(read_data);i++)
			{
				printf("0x%x\n",read_data[i]);
			}
			break;
	}

	close(fd);

	return 0;
}
