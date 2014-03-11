#ifndef __MODULES_SERVER__
#define __MODULES_SERVER__
#include "common.h"

#define UARTNAME	"/dev/ttyS1"
#define PHONEPASSAGE "/dev/phonepassage"
#define USBPASSAGE	"/dev/usbpassage"
#define UARTPASSAGE	"/dev/uartpassage"
#define SPIPASSAGE	"/dev/spipassage"
#define SGNAME		"/dev/sg0"
#define USBNAME		"/dev/as532h"
#define DATNAME		"/root/ElfDate.dat"
#define DEFAULT_AS532_IMAGE "/var/default_image/default_ElfDate.dat"
#define DEFAULT_STM32_IMAGE "/var/default_image/default_stm32_app_packet.bin"
#define STM32_BIN_NAME "/root/stm32_app_packet.bin"
#define LOW_COMMUNICATE		"/tmp/LOW_COMMUNICATE"
#define SERVER_PORT	53232
#define BUFFER_LEN 512
#define BUFFER_SIZE_1K	1024
#define BUFFER_SIZE_4K	4096
#define PAD_HEAD_BYTE1       0x55
#define PAD_HEAD_BYTE2       0xaa
#define MIN_PACKET_BYTES	      8
#define STM32_UP_HEAD_BYTE1_1     0XEA
#define STM32_UP_HEAD_BYTE2_1     0XFF
#define STM32_UP_HEAD_BYTE3_1     0X5A
#define STM32_UP_HEAD_BYTE4_1     0XA5
#define STM32_DOWN_HEAD_BYTE1_1     0XEF
#define STM32_DOWN_HEAD_BYTE2_1     0XFE
#define STM32_DOWN_HEAD_BYTE3_1     0XA5
#define STM32_DOWN_HEAD_BYTE4_1     0X5A
#define PACKET_HEAD_BYTES		  4
#define PACKET_ADDITIONAL_BYTES	  7
#define PACKET_DIR_INDEX		  6
#define PACKET_CMD_TYPE			  7
#define PACKET_STM32_HEAD_POS	  7
#define PACKET_STM32_REP_POS	  8
#define PACKET_STM32_DATA_POS	  9
#define PACKET_STM32_DATA_LEN_POS_HEIGHT 4
#define PACKET_STM32_DATA_LEN_POS_LOW 5

//通道、命令
#define PASSAGE_NUM 4
#define TYPE_STM32	0X10
#define TYPE_USB_PASSAGE	0X20
#define TYPE_UART_PASSAGE	0X30
#define TYPE_SPI_PASSAGE	0X40
#define TYPE_PHONE_PASSAGE	0x50
#define CMD_STM32_VER		0X80
#define CMD_STM32_VER_DES	0X81
#define CMD_STM32_BOOT		0X82
#define CMD_STM32_LED		0X83

//产测
#define FACTORY_TEST_PROGRAM	"/bin/factory_test"
#define SUCCESS							0x00
#define FAIL							0x01
#define FACTORY_TEST_CMD_LENGTH			9
#define FACTORY_TEST_HEAD_BYTE1			0XFA
#define FACTORY_TEST_HEAD_BYTE2			0XEF
#define FACTORY_TEST_HEAD_BYTE3			0XAD
#define FACTORY_TEST_HEAD_BYTE4			0XEA

#define FACTORY_TEST_CMD_1_POS			0X06	
#define FACTORY_TEST_CMD_2_POS			0X07
#define FACTORY_TEST_CMD_DATA_POS		0X08
//第一个字节
//DOWN
#define FACTORY_TEST_CMD_DOWN_CONTROL_1	0X00
#define FACTORY_TEST_CMD_DOWN_STM32_1	0X01
#define FACTORY_TEST_CMD_DOWN_AS532_1	0X02
#define FACTORY_TEST_CMD_DOWN_9344_1	0X03
#define FACTORY_TEST_CMD_DOWN_ALL_1		0X04
#define FACTORY_TEST_CMD_DOWN_MAC_1		0X0A
//UP
#define FACTORY_TEST_CMD_UP_CONTROL_1	0X80
#define FACTORY_TEST_CMD_UP_STM32_1		0X81
#define FACTORY_TEST_CMD_UP_AS532_1		0X82
#define FACTORY_TEST_CMD_UP_9344_1		0X83
#define FACTORY_TEST_CMD_UP_ALL_1		0X84
#define FACTORY_TEST_CMD_UP_MAC_1		0X8A

//start、stop
#define FACTORY_TEST_CMD_CONTROL_START	0X0A
#define FACTORY_TEST_CMD_CONTROL_STOP	0X0F
//stm32
#define FACTORY_TEST_CMD_STM32_VER		0X00
#define FACTORY_TEST_CMD_STM32_PIC		0X01
#define FACTORY_TEST_CMD_STM32_R54		0X02
//532
#define FACTORY_TEST_CMD_AS532_VER		0X00
#define FACTORY_TEST_CMD_AS532_TEST		0X01
//9344
#define FACTORY_TEST_CMD_9344_VER		0X00
#define FACTORY_TEST_CMD_9344_LED1		0X01
#define FACTORY_TEST_CMD_9344_LED2		0X02
#define FACTORY_TEST_CMD_9344_LED3		0X05
#define FACTORY_TEST_CMD_9344_CALL		0X03
#define FACTORY_TEST_CMD_9344_CALLED	0X04
#define FACTORY_TEST_CMD_9344_R54_CALL	0X06
#define FACTORY_TEST_CMD_9344_R54_CALLED	0X07

//all
#define FACTORY_TEST_CMD_ALL_TEST		0X03

//mac
#define FACTORY_TEST_CMD_UPDATE_MAC		0X0C

//ota
#define OTA_VER 			0x31
#define OTA_HEAD1 			0x4f
#define OTA_HEAD2	 		0x54
#define OTA_HEAD3 			0x41
#define OTA_CMD_VER_REQ		0x01
#define OTA_CMD_VER_RET	    0x02
#define OTA_CMD_SN_REQ		0x21
#define OTA_CMD_SN_RET		0x22
#define OTA_CMD_UPDATE_REQ	0x41
#define OTA_CMD_UPDATE_RET	0x42

#define OTA_STM32_ID		0x11
#define OTA_AS532H_ID		0x12
			
enum cmd_type {
	DEFAULT = 0,
	UKEY_1,//1
	UKEY_2,//2
	UKEY_3,//3
	UKEY_4,//4
	UKEY_5,//5
	AS532H_SHOW,//6
	KEY_EVENT,//7
	AS532H_VER,//8
	AS532H_VER_DES,//9
	STM32_1,//a
	STM32_VER,//b
	STM32_VER_DES,//c
	AS532H_UPDATE,//d
};
typedef struct __passage
{
	int fd;
	char type; //通道号
	char passage_name[20];
}Passage;

char CRC8(char *data, int length);
int parse_r54_ver(unsigned char *buf,int *bytes);
void check_r54_test_called_func();
void check_stm32_ver_req_func();
void check_stm32_ver_des_req_func();
void check_r54_test_ver_func();
void timer_do(int signo);
int init_as532();
int init_stm32();
void boot_close_usb(int *fd);
void init_env(void);
int ComFunChangeHexBufferToAsc(unsigned char *hexbuf,int hexlen,char *ascstr);
void ComFunPrintfBuffer(unsigned char *pbuffer,int len);
void loop_recv(void *argv);
void uart_loop_recv(void *argv);
int do_cmd_532_show(char *sendbuf);
int do_cmd_532_ver(char *outbuf,int *len);
int do_cmd_532_ver_des(char *outbuf,int *len);
int do_cmd_stm32_ver();
int do_cmd_stm32_ver_des();
int do_cmd_stm32_boot();
int do_cmd_532_update();
int do_cmd_stm32_update(unsigned char *path);
int do_cmd_rep(int type,char *data,int data_len,int result);
int as532_update(unsigned char *path);
int stm32_update(unsigned char* path);
int parse_msg(char *buf,char *ip,short port);
int serialConfig(int serial_fd, speed_t baudrate);
unsigned char sumxor(const  char  *arr, int len);
int UartGetVer(unsigned char *ppacket,int bytes);
int UartGetVerDes(unsigned char *ppacket,int bytes);
int UartCallBoot(unsigned char *ppacket,int bytes);
int UartPacketDis(unsigned char *ppacket,int bytes);
unsigned char *PacketSearchHead(void);
int UartPacketRcv(unsigned char *des_packet_buffer,int *packet_size);
void passage_thread_func(void *argv);

int start_test();
int stop_test();

void factory_test_func(void *argv);
int factory_test(unsigned char *packet,int bytes);
int factory_test_cmd_9344_r54_call();
int factory_test_cmd_9344_led1();
int factory_test_cmd_9344_led2();
int factory_test_cmd_9344_led3();
int factory_test_cmd_9344_ver();
int factory_test_cmd_as532_ver();
int factory_test_cmd_stm32_ver();
int factory_test_cmd_update_mac(unsigned char* data,int len);
int factory_test_down_9344(char cmd);
int factory_test_down_all(char cmd);
int factory_test_down_as532(char cmd);
int factory_test_down_control(char cmd);
int factory_test_down_stm32(char cmd);
int factory_test_down_mac(char cmd,unsigned char* data,int len);
int factory_test_cmd_stm32_r54();
int factory_test_r54_ver(unsigned char *packet,int bytes);
int factory_test_r54_called_func(unsigned char *packet,int bytes);
int generate_test_up_msg(char *sendbuf,char cmd1,char cmd2,char result_type,char err_code,char *result,int result_len);
int generate_stm32_down_msg(char *databuf,int databuf_len,int passage);
unsigned char *find_head(void);
int prase_test_msg();
int factory_test_cmd_stm32_pic();

unsigned char *FifoSearchHead(void);
int FifoPacketRcv(unsigned char *des_packet_buffer,int *packet_size);
int FifoPacketDis(unsigned char *ppacket,int bytes);
void ota_thread_func(void *argv);
int ota_as532h_ver();
int ota_stm32_ver();
int ota_as532h_sn();
int ota_stm32_sn();
int ota_as532h_update(unsigned char* path);
int ota_stm32_update(unsigned char* path);
int generate_ota_up_msg(unsigned char cmd,unsigned char id,unsigned char *data,int data_len);

extern int usb_fd;
extern unsigned char stm_format_version[4];
#endif
