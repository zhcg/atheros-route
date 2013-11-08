#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>


#define BUFSIZE	161
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

unsigned short buf[BUFSIZE] = {0};

#if 1 
int main(void)
{
	int fd_r, fd_w;
	int i=0;
	fd_w = open("./test.pcm", O_WRONLY|O_CREAT);//|O_TRUNC);
	if (fd_w < 0) {
		perror("open test.pcm error\n");
		exit(1);
	}
	fd_r = open("/dev/pcm0", O_RDONLY);
	//fd_r = open("/mnt/pcm.ko", O_RDONLY);
	if (fd_r < 0) {
		perror("open pcm0 error\n");
		exit(1);
	}
		for (i=0;i<0x4ffff;i++);
#if 1
	int ret = 0;
	int cnt = 10;
	int len = 0;
	int sum_r = 0;
	int sum_w = 0;
	unsigned short *pos = buf;
	ioctl(fd_r, PCM_SET_RECORD);
	while(1 /*cnt--*/){
		pos = buf;
		len = read(fd_r,(void*)buf, 2*(BUFSIZE-1));
		if(len < 0){
			perror("read err");
			exit(1);
		}
		if (sum_r < 0/*len == 0*/){
			break;	
		}
		sum_r += len;
		while(1){
			ret = write(fd_w,(void*)pos,len);
			sum_w += ret;
			len -= ret;
			pos += ret;
			if(len == 0)
				break;
		}
	}
	ioctl(fd_r, PCM_SET_UNRECORD);
#endif
	printf("read the data over\n");
	close(fd_r);
	close(fd_w);
	return 0;
}
#endif
