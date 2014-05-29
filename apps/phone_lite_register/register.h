#ifndef __REGISTER__H__
#define __REGISTER__H__
#include "common.h"

#define PACKET_HEAD_BYTE1	0X5A
#define PACKET_HEAD_BYTE2	0XA5

#define REGISTE_REQ			0X41
#define UNREGISTE_REQ		0X42
#define RENAME_REQ			0X43
#define PASSWORD_REQ		0X44
#define OK					0X80
#define ERR					0X81
#define ERR_CODE			0X01

#define DEFAULT_PASSWORD	"111111111111"

typedef struct __client_t
{
	//句柄，如没有连接，则设置为-1
	int client_fd;
	//ip
	char client_ip[16];

	//接收数据标志
	int wp;
	int rp;
	
	unsigned char msg[BUF_LEN_256];
}client_t;

void init();
void loop_handle(void *argv);
void tcp_loop_recv(void *argv);
void sock_loop_accept(void* argv);

#endif
