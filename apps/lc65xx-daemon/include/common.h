/*************************************************************************
	> File Name: common.h
	> Author: 
	> Mail: @163.com 
	> Created Time: 2017年03月15日 星期三 13时55分48秒
 ************************************************************************/
#ifndef _COMMON_H
#define _COMMON_H

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
//#include <android/log.h>
#include <pthread.h>
#include <fcntl.h>
#include <linux/types.h>
#include "serial.h"

#if 1
#define PRINT(format, ...) printf("[%s][%s][-%d-] "format"",__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#else
#define PRINT(format, ...)
#endif

#endif

