#ifndef _CGIMAIN_H_
#define _CGIMAIN_H_ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/reboot.h>

#include <sys/file.h>

#define SYS_LOG

void __write_systemLog(char *message)
{
	FILE *f;
	int i;
	int fd;

	fd = open("/tmp/br0", O_RDWR);
	flock(fd, LOCK_EX);

	//fprintf(errOut,"%s  %d\n",__func__,__LINE__);
	f = fopen("/tmp/systemLog", "a" );

	system("echo ----------------  >>  /tmp/systemLog");
	system("date  >>  /tmp/systemLog");
	system("dmesg -c  >>  /tmp/systemLog");
	fseek(f, 0, SEEK_END);
	fwrite(message,strlen(message),1,f);

	fclose(f);
	system("echo   >>  /tmp/systemLog");

	flock(fd, LOCK_UN);
	close(fd);

}
void write_systemLog(char *message)
{
	#ifdef SYS_LOG
	__write_systemLog(message);
	#endif
}

#endif
