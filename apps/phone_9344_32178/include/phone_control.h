#ifndef __PHONE_CONTROL_H__
#define __PHONE_CONTROL_H__
#include "common.h"
#include "si32178.h"

#define MIN_PACKET_BYTES	        7

#define RING_LIMIT					9

#define PACKET_HEAD_BYTE1       0XA5
#define PACKET_HEAD_BYTE2       0X5A

#define PACKET_HEAD_BYTES       2
#define PACKET_LEN_BYTES	        2
#define PACKET_CHECK_BYTES     1
#define PACKET_ADDITIONAL_BYTES	4

#define PACKET_TYPE_INDEX	5
#define PACKET_TYPE_FROM_PIC	0xAA
#define PACKET_TYPE_FROM_254	0xA5
#define PACKET_CMD_INDEX_TYPE_FROM_254	7
#define PACKET_CMD_INDEX_TYPE_FROM_PIC	7
#define PACKET_DIR_INDEX			4
#define CLI_REQ_MAX_NUM 100
#define FSK 0
#define DTMF 1


enum PstnCommand {
	UNDEFINED = -1,
	DEFAULT,
	/*client heartbeat*/
	HEARTBEAT,

	/*Pstn control event*/
	DIALING,
	OFFHOOK,
	ONHOOK,
	DTMF_SEND,

	//about registration
	LOGIN,

	//switch function
	SWITCHTOSONMACHINE,

	//talkback function
	TALK,
	TALKBACKOFFHOOK,
	TALKBACKONHOOK,
	REQ_SWITCH,
	REQ_TALK,
	RET_PHONETOBASE,
};
typedef enum PstnCommand PstnCmdEnum;

typedef struct dev_status
{
	int registered;//注册
	int id;//ID

	int client_fd;//控制句柄
	char client_ip[16];//子机ip
	int control_reconnect;

	int audio_client_fd;//音频句柄
	int attach;//音频控制匹配
	int audio_reconnect;//重连

	int isswtiching;//正在转子机

	int talkbacked; //¶Ô½²±»½Ð
	int talkbacking;//¶Ô½²Ö÷½Ð
	int talkback_answer;

	int incoming;
	int tick_time;
	int old_tick_time;
	int destroy_count;
	int dying;

	char dev_name[16];//设备名字

	int dev_is_using;//设备是否正在使用

}dev_status_t ;

typedef struct {
	int fd;
	char head[6]; //head
	char id; // client_id
	int length; //length
	PstnCmdEnum cmd; //cmd
	int arglen; //arg_len
	char arg[SENDBUF+1]; //arg
	dev_status_t *dev;
}cli_request_t; //client cmd message

struct class_phone_control
{
	int phone_control_sockfd;
	char telephone_num[64];
	int telephone_num_len;
	char dtmf;
	int global_onhook_success; //挂机成功标志
	int global_offhook_success; //挂机成功标志
	int dial_flag;
	int global_incoming;//来电
	int global_phone_is_using;//电话占用
	int global_talkback;
	int start_dial;
	int ringon_flag;//用于判断RINGON
	int send_dtmf;
	dev_status_t who_is_online;//当前占用线路的设备
	int handle_up_msg_wp_pos;
	int handle_up_msg_rd_pos;
	vdaaRingDetectStatusType Status;
	struct itimerval value;//定时器
	int check_ringon_count;//检测来电
	int send_sw_ringon_count;//发送响铃
	int send_tb_ringon_count;//发送响铃
	int send_sw_ringon_limit;//发送响铃次数限制
	int send_tb_ringon_limit;//发送响铃次数限制
	int sw_dev;
	int tb_dev;
	int cli_req_buf_wp;
	int cli_req_buf_rp;
	int last_cli_length;
	int get_fsk; //获取fsk号码
	int ring_neg_count;
	int ring_pos_count;
	int ring_count;
};

void Ringon(unsigned char *ppacket);
void Incomingcall(unsigned char *ppacket,int bytes,int flag);
void OnhookRes(unsigned char *ppacket,int bytes);
void OffhookRes(unsigned char *ppacket,int bytes);
void Parallel();
int SpiUart1CmdDo(unsigned char *ppacket,int bytes);
int SpiUart2CmdDo(unsigned char *ppacket,int bytes);
int UpPacketDis(unsigned char *ppacket,int bytes);
unsigned char *PacketSearchHead(void);
int UpPacketRcv(unsigned char *des_packet_buffer,int *packet_size);
int netWrite(int fd,const void *buffer,int length);
int destroy_client(dev_status_t *dev);
int match_cmd(char *buf,int len);
int getoutcmd(char *buf,int ret,dev_status_t* dev);
void clean_who_is_online();
int do_cmd_heartbeat(dev_status_t *dev);
int do_cmd_offhook(dev_status_t *dev);
int do_cmd_onhook(dev_status_t *dev);
int do_cmd_dialup(dev_status_t *dev);
int do_cmd_dtmf(dev_status_t *dev);
int do_cmd_switch(dev_status_t* dev,char *sendbuf);
int do_cmd_talkback(dev_status_t* dev,char *sendbuf);
int do_cmd_talkbackoffhook(dev_status_t* dev,char *sendbuf);
int do_cmd_talkbackonhook(dev_status_t* dev,char *sendbuf);
int do_cmd_req_switch(dev_status_t* dev,char *sendbuf);
int do_cmd_req_talk(dev_status_t* dev,char *sendbuf);
int do_cmd_ret_ptb(dev_status_t* dev,char *sendbuf);
int parse_msg(cli_request_t *cli);
void *phone_control_loop_accept(void *argv);
int ComFunChangeHexBufferToAsc(unsigned char *hexbuf,int hexlen,char *ascstr);
void ComFunPrintfBuffer(unsigned char *pbuffer,unsigned char len);
int init_control(void);
void *handle_up_msg(void * argv);
void *loop_check_ring(void* argv);
int generate_up_msg(void);
void *phone_check_tick(void* argv);
void *phone_link_manage(void* argv);
void *tcp_loop_recv(void* argv);
void *handle_down_msg(void* argv);
int getCmdtypeFromString(char *cmd_str);
void check_ringon_func();
void get_code_timeout_func();
void send_sw_ringon();
void send_tb_ringon();
void check_dev_tick(int signo);
int generate_incoming_msg(unsigned char *buf,const unsigned char *num,int num_len);

extern struct class_phone_control phone_control;
extern struct class_phone_audio phone_audio;
extern dev_status_t devlist[4];
extern unsigned char output_stream_buffer[AUDIO_STREAM_BUFFER_SIZE];
#endif