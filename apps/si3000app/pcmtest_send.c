#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>


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

unsigned short buf[BUFSIZE] = {0};

int main(void)
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
	while (1) {
	total++;
	if (total == 10000) {
//		break;
	}
	fd_w = open("/dev/pcm0", O_WRONLY);//|O_TRUNC);
	if (fd_w < 0) {
		perror("open pcm0 error\n");
		exit(1);
	}
	fd_r = open("./test.pcm", O_RDONLY);
	if (fd_r < 0) {
		perror("open test error\n");
		exit(1);
	}
	ioctl(fd_w, PCM_SET_PLAYBACK);
	while(1 /*cnt--*/){
		memset(buf,0,2*BUFSIZE);
		len = read(fd_r,(void*)buf, 2*(BUFSIZE-1));
		pos = buf;
		if(len < 0){
			perror("read the data err\n");
			exit(1);
		}
		if (len == 0) {
			break;
		}
		if (sum_r < 0/*len == 0*/){
			break;	
		}
		sum_r += len;
		j++;
		while(1){
			if (len > 0) {
				ret = write(fd_w,(void*)pos,len);
				sum_w += ret;
				len -= ret;
				pos += ret;
			}
			if(len == 0)
				break;
		}
	}
	ioctl(fd_w, PCM_SET_UNPLAYBACK);
	for (i=0;i<0x4fffff;i++);
	close(fd_r);
	close(fd_w);
	sleep(1);
	}
//	printf("sum_r = %d, sum_w = %d\n", sum_r, sum_w);
	return 0;
}

