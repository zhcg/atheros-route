#ifndef __STM32_UPDATE__
#define __STM32_UPDATE__

#define BIN_FILE_HEAD_BYTES	18
#define UPDATE_DATA_MAX_BYTES	1024
#define UPDATE_APP_START_ADD	0X08008000

#define UPDATE_REQ_BUF_SIZE		2048
#define UPDATE_RSP_BUF_SIZE		2048

#define UPDATE_START_CMD	(0X00)
#define UPDATE_END_CMD		(0X02)
#define UPDATE_DATA_CMD		(0X01)
//发送到应用层的进入boot命令
#define UPDATE_GETVER_CMD		(0X80)
#define UPDATE_REBOOT_CMD		(0X82)

#define RSP_CMD_MIN_BYTES 9

int setup_port(int fd, int baud, int databits, int parity, int stopbits);
int reset_port(int fd);
int read_data(int fd, void *buf, int len);
int write_data(int fd, void *buf, int len);
void print_usage(char *program_name);
unsigned long get_file_size(const char *path);
static unsigned char PacketXorGenerate(const unsigned char *ppatas,int bytes);
int PacketRspGet(unsigned char *pbuf,int offset,int valid_bytes);
int UpdateRspRcv(unsigned char *prcv_buffer,int buf_size);
int OrgReBootPacket(unsigned char *psend_buf);
int CmdReBoot(void);
int OrgGetVerPacket(unsigned char *psend_buf);
int CmdGetVersion(void);
int OrgStartPacket(unsigned char *psend_buf,unsigned int app_start_add,unsigned int app_file_size);
int CmdStart(void);
int OrgDataTransPacket(unsigned char *psend_buf,unsigned char *pfile_buf,
					   unsigned short packet_index,unsigned int packet_data_size);
int CmdDataTrans(unsigned char *pfile_buf, unsigned short packet_index,unsigned int packet_data_size);
int Update(const char *path);
int OrgEndPacket(unsigned char *psend_buf);
int CmdEnd(void);
int BinFileHeadCheck(const char *path);

extern int global_uart_fd;

#endif
