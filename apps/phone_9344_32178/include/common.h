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
#include <sys/wait.h>
#include <assert.h>

//#define REGISTER

#define Big_Endian	1
#define Little_Endian	0

#ifdef B6L
#define AS532_NUM	"get532ver"
#define AS532_VER	"HBD_B6L_AS532_V"
#define TMP_AS532_VER_FILE "/tmp/tmp_as532_ver_file"
#endif

#define PCMNAME "/dev/slic"
#define TICK_INTERVAL 100//50

#ifdef B6
#define PASSAGE_NAME "/dev/phonepassage"
#else
#define LED_NAME "/dev/wifiled"
#endif

#ifdef S1
#define UDHCPD_FILE  "/var/run/udhcpd.leases"
#define A20_NAME 	"HBD_F2B_A20"
#endif
#ifdef S1_F3A
#define UDHCPD_FILE  "/var/run/udhcpd.leases"
#define A20_NAME 	"HBD_F3A_A20"
#endif

#define AUTHOR	"ZhangBo"
#define PRINT_INFO 1

#if PRINT_INFO==1
#define PRINT(format, ...) printf("[%s][-%d-] "format"",__FUNCTION__,__LINE__,##__VA_ARGS__)
#else
#define PRINT(format, ...)
#endif

/*
#if PRINT_INFO==1
#define PRINT(format, ...) printf("[%s][%s][-%d-] "format"",system_time(),__FUNCTION__,__LINE__,##__VA_ARGS__)
#else
#define PRINT(format, ...)
#endif
*/

#define MAX(A,B) ((A)>(B))?(A):(B)
#ifdef B6
#define CLIENT_NUM 21	//实际设备数目限制为20
#else
#define CLIENT_NUM 21	//实际设备数目限制为20
#endif
#define SENDBUF 					128
#define BUF_LEN_256					256
#define BUF_LEN   					512
#define BUFFER_SIZE_1K     			1024
#define BUFFER_SIZE_2K 				2048
#define AUDIO_PORT 					8888
#define CONTROL_PORT				9999
#define DELAY						20000
#define CODE_MAX					32635	
#define AUDIO_SEND_PACKET_SIZE		1600
#define AUDIO_READ_BYTE_SIZE		1280//512//448
#define AUDIO_WRITE_BYTE_SIZE		AUDIO_READ_BYTE_SIZE
#define AUDIO_STREAM_BUFFER_SIZE	AUDIO_READ_BYTE_SIZE*300
#define AUDIO_PACKET_LIMIT			5//14
//#define AUDIO_PACKET_LIMIT			25

//speex
#define NN 							(AUDIO_READ_BYTE_SIZE / 2)
#define TAIL						1600
//aecm
#define FRAME_DOTS 80
#define FRAME_BYTES (FRAME_DOTS*2)

#endif

