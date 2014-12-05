#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
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
#include <arpa/inet.h>
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
#include <locale.h>
#include <stdio.h>
#include <sys/stat.h>  //stat函数
#include <sys/wait.h>

#define AUTHOR	"ZhangBo"
#define PRINT_INFO 1

#if PRINT_INFO==1
#define PRINT(format, ...) printf("[%s][%s][-%d-] "format"",__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#else
#define PRINT(format, ...)
#endif

#define MAX(A,B) ((A)>(B))?(A):(B)

#define BUF_LEN_64					64
#define BUF_LEN_128 				128
#define BUF_LEN_256					256
#define BUF_LEN_512  				512
#define BUFFER_SIZE_1K     			1024
#define BUFFER_SIZE_2K 				2048
#define REGISTER_PORT 				7788

#endif

