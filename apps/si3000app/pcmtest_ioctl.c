#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>


struct si3000_ioctl_regs{
	int reg;
	char val;
};


#define BUFSIZE	 161
//#define PCM_SET_RECORD			0
//#define PCM_SET_UNRECORD		1	
//#define PCM_SET_PLAYBACK		2
//#define PCM_SET_UNPLAYBACK		3
//#define PCM_EXT_LOOPBACK_ON		4
//#define PCM_EXT_LOOPBACK_OFF		5
/* driver i/o control command */
#define PCM_IOC_MAGIC	'p'
//#define _IO(type,nr)
#define PCM_SET_RECORD			_IO(PCM_IOC_MAGIC, 0)
#define PCM_SET_UNRECORD		_IO(PCM_IOC_MAGIC, 1)	
#define PCM_SET_PLAYBACK		_IO(PCM_IOC_MAGIC, 2)
#define PCM_SET_UNPLAYBACK		_IO(PCM_IOC_MAGIC, 3)
#define PCM_EXT_LOOPBACK_ON		_IO(PCM_IOC_MAGIC, 4)
#define PCM_EXT_LOOPBACK_OFF		_IO(PCM_IOC_MAGIC, 5)
#define PCM_SI3000_REG                  _IOW(PCM_IOC_MAGIC, 8, struct si3000_ioctl_regs)

unsigned short buf[BUFSIZE] = {0};

int main(int argc, const char *argv[])
{
	int fd_r, fd_w;
	int i = 0,j = 0,k=0;
	int ret = 0;
	int cnt = 10;
	int len = 0;
	int sum_r = 0;
	int sum_w = 0;
	unsigned short *pos = buf;
	static total=0;
	int reg = 0;
	
	struct si3000_ioctl_regs ioctl_reg;

	sscanf(argv[1],"%x",&reg);

	printf("reg: %x\n", reg);

	ioctl_reg.reg = (reg>>8)&0xff;
	ioctl_reg.val = reg&0xff;

	
	

	fd_w = open("/dev/pcm0", O_WRONLY);//|O_TRUNC);
	if (fd_w < 0) {
		perror("open pcm0 error\n");
		exit(1);
	}
	ioctl(fd_w, PCM_SI3000_REG, &ioctl_reg);
	//ioctl(fd_w, PCM_SET_UNPLAYBACK);
	for (i=0;i<0x4fffff;i++);
	close(fd_w);
	sleep(1);
//	printf("sum_r = %d, sum_w = %d\n", sum_r, sum_w);
	return 0;
}

