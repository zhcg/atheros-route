/******************************************************************************
*
* FILENAME:
*     testcom.c
*
* DESCRIPTION:
*     A utility to read/write serial port
*     Usage: testcom <device> <baud> <databits> <parity> <stopbits> <read_or_write>
*        databits: 5, 6, 7, 8
*        parity: 0(None), 1(Odd), 2(Even)
*        stopbits: 1, 2
*        read_or_write: 0(read), 1(write)
*     Example: testcom /dev/ttyS0 9600 8 0 1 0
*
* REVISION(MM/DD/YYYY):
*     03/12/2008  Shengkui Leng (shengkui.leng@advantech.com.cn)
*     - Initial version
*
******************************************************************************/
#include "common.h"
#include "stm32_update.h"
#include "file.h"
#include "modules_server.h"
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum __REQ_RSP_CODE
{
	REQ_RSP_OK = 0X00,
	REQ_RSP_CMD_ERR = 0X01,
	REQ_RSP_XOR_ERR = 0X02,
	REQ_RSP_ADD_ERR = 0X03,
	REQ_RSP_TOTAL_DATA_MORE_ERR = 0X04,
	REQ_RSP_PACKET_INDEX_ERR = 0X10,
	REQ_RSP_DATA_NOT_EQUAL_ERR = 0X11,	
	REQ_RSP_PACKET_DATA_MORE_ERR = 0X12,
	REQ_RSP_WRITE_FLASH_ERR = 0X13,
}REQ_RSP_CODE;


int fsfd;
unsigned int update_bin_file_size;
unsigned char stm_format_version[4] = {0x00,0x00,0x00,0x00};
unsigned char bin_format_version[4] = {0x00,0x00,0x00,0x00};
unsigned char bin_file_head[8] = {0x41,0x70,0x70,0x42,0x69,0x6E,0x20,0x20};
unsigned char packet_req_head[4] = {0xFE,0xEF,0xA5,0x5A};
unsigned char packet_rsp_head_1[4] = {0xEA,0xFF,0x5A,0xA5};
unsigned char packet_rsp_head_2[4] = {0xFF,0xEA,0x5A,0xA5};

unsigned char update_req_buffer[UPDATE_REQ_BUF_SIZE];
unsigned char update_rsp_buffer[UPDATE_RSP_BUF_SIZE];


int stm32_update(unsigned char* path)
{
    int fd;
    int baud = 115200;
    int len;
    int count;
    int i,ret;
    int databits = 8;
    int stopbits = 1;
    int parity = 0;
    int read_or_write;
	int update;

//	update_bin_file_size = get_file_size(STM32_BIN_NAME);
//	PRINT("get_file_size = %d \n", update_bin_file_size);

//	while(1){	
    //global_uart_fd = open(ttydev, O_RDWR, 0);
	//PRINT("global_uart_fd = %d \n", global_uart_fd);
    //if (global_uart_fd < 0) {
		//fprintf(stderr, "open <%s> error %s\n", ttydev, strerror(errno));
        //return -1;
    //}

	if (BinFileHeadCheck(path) == 0){
		update = 0;
		if((ret = CmdGetVersion()) != 0){
			update = 1;
			//return -2;
		}
		PRINT("bin_format_version:");
		for(i = 0; i < 4; i++)
			PRINT("%x ", bin_format_version[i]);
		PRINT("\n");

		PRINT("stm_format_version:");
		for(i = 0; i < 4; i++)
			PRINT("%x ", stm_format_version[i]);
		PRINT("\n");
		//比较STM32版本和BIN文件版本
		ret = memcmp((void *)bin_format_version, (void *)stm_format_version, sizeof(bin_format_version));
		if ((ret > 0) | (update == 1)){
			PRINT("Updata begin !!!\n");
			if((ret = CmdReBoot()) != 0)
				return -3;
			usleep(500 * 1000);
			if((ret = CmdStart()) != 0)
				return -4;
			if((ret = Update(STM32_BIN_NAME)) != 0)
				return -5;
			if((ret = CmdEnd()) != 0)
				return -6;
			PRINT("Update success !!!\n");
		}
		

	} else {
		PRINT("BinFileHeadCheck error !!!\n");
		//close(global_uart_fd);
		return -1;
	}

	//close(global_uart_fd);
	usleep(3000 * 1000);
//	}
	return 0;
}


static int baudflag_arr[] = {
    B921600, B460800, B230400, B115200, B57600, B38400,
    B19200,  B9600,   B4800,   B2400,   B1800,  B1200,
    B600,    B300,    B150,    B110,    B75,    B50
};
static int speed_arr[] = {
    921600,  460800,  230400,  115200,  57600,  38400,
    19200,   9600,    4800,    2400,    1800,   1200,
    600,     300,     150,     110,     75,     50
};


/******************************************************************************
* NAME:
*      speed_to_flag
*
* DESCRIPTION:
*      Translate baudrate into flag
*
* PARAMETERS:
*      speed - The baudrate to convert
*
* RETURN:
*      The flag
******************************************************************************/
int speed_to_flag(int speed)
{
    int i;

    for (i = 0;  i < sizeof(speed_arr)/sizeof(int);  i++) {
        if (speed == speed_arr[i]) {
            return baudflag_arr[i];
        }
    }

    fprintf(stderr, "Unsupported baudrate, use 9600 instead!\n");
    return B9600;
}


static struct termio oterm_attr;

/******************************************************************************
* NAME:
*      stup_port
*
* DESCRIPTION:
*      Set serial port (include baudrate, line control)
*
* PARAMETERS:
*      fd       - The fd of serial port to setup
*      baud     - Baudrate: 9600, ...
*      databits - Databits: 5, 6, 7, 8
*      parity   - Parity: 0(None), 1(Odd), 2(Even)
*      stopbits - Stopbits: 1, 2
*
* RETURN:
*      0 for OK; Others for ERROR
******************************************************************************/
int setup_port(int fd, int baud, int databits, int parity, int stopbits)
{
    struct termio term_attr;

    /* Get current setting */
    if (ioctl(fd, TCGETA, &term_attr) < 0) {
        return -1;
    }

    /* Backup old setting */
    memcpy(&oterm_attr, &term_attr, sizeof(struct termio));

    term_attr.c_iflag &= ~(INLCR | IGNCR | ICRNL | ISTRIP);
    term_attr.c_oflag &= ~(OPOST | ONLCR | OCRNL);
    term_attr.c_lflag &= ~(ISIG | ECHO | ICANON | NOFLSH);
    term_attr.c_cflag &= ~CBAUD;
    term_attr.c_cflag |= CREAD | speed_to_flag(baud);

    /* Set databits */
    term_attr.c_cflag &= ~(CSIZE);
    switch (databits) {
        case 5:
            term_attr.c_cflag |= CS5;
            break;

        case 6:
            term_attr.c_cflag |= CS6;
            break;

        case 7:
            term_attr.c_cflag |= CS7;
            break;

        case 8:
        default:
            term_attr.c_cflag |= CS8;
            break;
    }

    /* Set parity */
    switch (parity) {
        case 1:   /* Odd parity */
            term_attr.c_cflag |= (PARENB | PARODD);
            break;

        case 2:   /* Even parity */
            term_attr.c_cflag |= PARENB;
            term_attr.c_cflag &= ~(PARODD);
            break;

        case 0:   /* None parity */
        default:
            term_attr.c_cflag &= ~(PARENB);
            break;
    }


    /* Set stopbits */
    switch (stopbits) {
        case 2:   /* 2 stopbits */
            term_attr.c_cflag |= CSTOPB;
            break;

        case 1:   /* 1 stopbits */
        default:
            term_attr.c_cflag &= ~CSTOPB;
            break;
    }

    term_attr.c_cc[VMIN] = 1;
    term_attr.c_cc[VTIME] = 0;

    if (ioctl(fd, TCSETAW, &term_attr) < 0) {
        return -1;
    }

    if (ioctl(fd, TCFLSH, 2) < 0) {
        return -1;
    }

    return 0;
}


/******************************************************************************
* NAME:
*      read_data
*
* DESCRIPTION:
*      Read data from serial port
*
*
* PARAMETERS:
*      fd   - The fd of serial port to read
*      buf  - The buffer to keep readed data
*      len  - The max count to read
*
* RETURN:
*      Count of readed data
******************************************************************************/
int read_data(int fd, void *buf, int len)
{
    int count;
    int ret;

    ret = 0;
	count = 0;

    while (len > 0) {

    	ret = read(fd, (char*)(buf + count), len);
    	if (ret < 1) {
			fprintf(stderr, "Read error %s errno = %d\n", strerror(errno), errno);
        	continue;
		}
		
		count += ret;
		len = len - ret;
	}
    return count;
}


/******************************************************************************
* NAME:
*      write_data
*
* DESCRIPTION:
*      Write data to serial port
*
*
* PARAMETERS:
*      fd   - The fd of serial port to write
*      buf  - The buffer to keep data
*      len  - The count of data
*
* RETURN:
*      Count of data wrote
******************************************************************************/
int write_data(int fd, void *buf, int len)
{
    int count;
    int ret;

    ret = 0;
    count = 0;

    while (len > 0) {

        ret = write(fd, (char*)(buf + count), len);
        if (ret < 1) {
            fprintf(stderr, "Write error %s\n", strerror(errno));
            break;
        }

        count += ret;
        len = len - ret;
    }

    return count;
}


/******************************************************************************
* NAME:
*      print_usage
*
* DESCRIPTION:
*      print usage info
*
* PARAMETERS:
*      program_name - The name of the program
*
* RETURN:
*      None
******************************************************************************/
void print_usage(char *program_name)
{
    fprintf(stderr,
            "*************************************\n"
            "  A Simple Serial Port Test Utility\n"
            "*************************************\n\n"
            "Usage:\n  %s <device> <baud> <databits> <parity> <stopbits> <read_or_write>\n"
            "       databits: 5, 6, 7, 8\n"
            "       parity: 0(None), 1(Odd), 2(Even)\n"
            "       stopbits: 1, 2\n"
            "       read_or_write: 0(read), 1(write)\n"
            "Example:\n  %s /dev/ttyS0 9600 8 0 1 0\n\n",
            program_name, program_name
           );
}


/******************************************************************************
* NAME:
*      reset_port
*
* DESCRIPTION:
*      Restore original setting of serial port
*
* PARAMETERS:
*      fd - The fd of the serial port
*
* RETURN:
*      0 for OK; Others for ERROR
******************************************************************************/
int reset_port(int fd)
{
    if (ioctl(fd, TCSETAW, &oterm_attr) < 0) {
        return -1;
    }

    return 0;
}
///////////////////////////////////////////////////////////////////////////////
//获取文件大小
///////////////////////////////////////////////////////////////////////////////

unsigned long get_file_size(const char *path)  
{  
    unsigned long filesize = -1;      
    struct stat statbuff;  
    if(stat(path, &statbuff) < 0){  
        return filesize;  
    }else{  
        filesize = statbuff.st_size;  
    }  
    return filesize;  
}  

///////////////////////////////////////////////////////////////////////////////
//CRC16算法：
///////////////////////////////////////////////////////////////////////////////

/* CRC 高位字节值表 */
static const unsigned char auchCRCHi[] = {
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
	0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
	0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
	0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
	0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
	0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
	0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
	0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
	0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
	0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
} ;
/* CRC低位字节值表*/
static const unsigned char auchCRCLo[] = 
{
	0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06,
	0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD,
	0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
	0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A,
	0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4,
	0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
	0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3,
	0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,
	0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
	0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29,
	0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED,
	0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
	0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60,
	0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67,
	0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
	0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,
	0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E,
	0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
	0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71,
	0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92,
	0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
	0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B,
	0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B,
	0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
	0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42,
	0x43, 0x83, 0x41, 0x81, 0x80, 0x40
} ;

//unsigned short CRC16(unsigned char *puchMsg, unsigned short usDataLen)
//
//{
	//unsigned char uchCRCHi = 0xFF ; /* 高CRC字节初始化 */
	//unsigned char uchCRCLo = 0xFF ; /* 低CRC 字节初始化 */
	//unsigned uIndex ; 		/* CRC循环中的索引 */
	//while (usDataLen--) 	/* 传输消息缓冲区 */
	//{
		//uIndex = uchCRCHi ^ *(puchMsg++); /* 计算CRC */
		//uchCRCHi = uchCRCLo ^ auchCRCHi[uIndex] ;
		//uchCRCLo = auchCRCLo[uIndex] ;
	//}
	//return (uchCRCHi << 8 | uchCRCLo) ;
//}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static unsigned char PacketXorGenerate(const unsigned char *ppatas,int bytes)
{
	unsigned char result = 0;
	int i;
	for(i = 0; i < bytes; i++)
	{
		result ^= ppatas[i];
	}
	return result;
}

///////////////////////////////////////////////////////////////////////////////
static int PacketHeadSearch(unsigned char *pbuf,int offset,int valid_bytes)
{
	int bytes;
	unsigned char *ppcom;
	int i,j;
	ppcom = pbuf;
	bytes = valid_bytes - offset;
	if(bytes < RSP_CMD_MIN_BYTES){
//		PRINT("Error: respond length <　RSP_CMD_MIN_BYTES \n");					
		return -1;
	}
	j = valid_bytes - RSP_CMD_MIN_BYTES;
	for(i = offset;i <= j;i++)
	{		
		if((ppcom[i] == packet_rsp_head_1[0]) && (ppcom[i + 1] == packet_rsp_head_1[1]) && 
			(ppcom[i + 2] == packet_rsp_head_1[2]) && (ppcom[i + 3] == packet_rsp_head_1[3]))
		{
			return i;	
		}
		if((ppcom[i] == packet_rsp_head_2[0]) && (ppcom[i + 1] == packet_rsp_head_2[1]) && 
			(ppcom[i + 2] == packet_rsp_head_2[2]) && (ppcom[i + 3] == packet_rsp_head_2[3]))
		{
			return i;	
		}
	}
	PRINT("Error: respond packet head not found \n");	
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
int PacketRspGet(unsigned char *pbuf,int offset,int valid_bytes)
{
	unsigned short packet_len;
	unsigned char check_byte;


	packet_len = (pbuf[4] * 256) + pbuf[5];
	packet_len += (4 + 2);
	if(valid_bytes <= packet_len){
		PRINT("Error: respond valid_bytes < packet_len\n");
		return -1;
	}
	check_byte = PacketXorGenerate(pbuf,packet_len);
	if(check_byte == pbuf[packet_len])
	{
		packet_len++;
		return packet_len;
	}
	PRINT("Error: respond xor check error \n");
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
int UpdateRspRcv(unsigned char *prcv_buffer,int buf_size)
{
	int rcv_index;
	int i,j;
	int ret = 0;
	int loops = 0;
	rcv_index = 0;
	do
	{
		loops++;
		if(loops > 20){
			PRINT("Error: get respond timeout \n");
			return -1;
		}
//		PRINT("get respond .... \n");

		usleep(50 * 1000);
//		ret = read_data(global_uart_fd, prcv_buffer, (buf_size - rcv_index));
		ret = read(global_uart_fd, (prcv_buffer + rcv_index), (buf_size - rcv_index));
		//PRINT("ret = %d\n",ret);
//		for(i=0; i<ret; i++)
//			   PRINT("read_data ret = %x \n", prcv_buffer[i]);
		if(ret > 0)
		{
			//ComFunPrintfBuffer(prcv_buffer + rcv_index,ret);
			rcv_index += ret;
		}

		if(rcv_index > 0)
		{
			i = PacketHeadSearch(prcv_buffer,0,rcv_index);
			if(i > -1)
			{
				j = PacketRspGet(prcv_buffer,i,rcv_index);	
				if(j > 1)
				{
					for(ret = 0;ret < j;ret++)
					{
						prcv_buffer[ret] = prcv_buffer[i + ret];
					}
					return j;
				}
				else if(j = -1)
					return j;
			}
		}
	}while(1);
}
/******************************************************************************
* NAME:
*      CmdReBoot
*
* DESCRIPTION:
*      smt32复位进入boot
*
* PARAMETERS:
*      fd - The fd of the serial port
*
* RETURN:
*      0 for OK; Others for ERROR
******************************************************************************/
///////////////////////////////////////////////////////////////////////////////
int OrgReBootPacket(unsigned char *psend_buf)
{
	int index = 0;
	unsigned char check_byte;
//第一级包头
	psend_buf[index++] = 0xEF;
	psend_buf[index++] = 0xFE;
	psend_buf[index++] = 0xA5;
	psend_buf[index++] = 0x5A;
//第一级数据长度，高位在前		
	psend_buf[index++] = 0x00;
	psend_buf[index++] = 0x02;
//数据通道为STM32自身
	psend_buf[index++] = 0x10;
//第二级包头
	//psend_buf[index++] = 0xFE;
	//psend_buf[index++] = 0xEF;
	//psend_buf[index++] = 0xA5;
	//psend_buf[index++] = 0x5A;
//第二级数据长度，高位在前		
	//psend_buf[index++] = 0x00;	
	//psend_buf[index++] = 0x01;	
//第二级命令
	psend_buf[index++] = UPDATE_REBOOT_CMD;
//第二级校验字节	
	//check_byte = PacketXorGenerate((psend_buf+7),(index-7));	
	//psend_buf[index++] = check_byte;
//第一级校验字节
	check_byte = PacketXorGenerate(psend_buf,index);	
	psend_buf[index++] = check_byte;

	return index;	
}

int CmdReBoot(void)
{
	int ret;
	int packet_bytes;
	int i;
	PRINT("Enter %s ......\n", __FUNCTION__);
	packet_bytes = OrgReBootPacket(update_req_buffer);
//	PRINT("update_req_buffer:  \n");
//	for(i = 0; i < packet_bytes; i++)
//		PRINT("%x ", update_req_buffer[i]);
//	PRINT("\n");
	//发送命令请求
	ret = write_data(global_uart_fd, update_req_buffer, packet_bytes);
	if (ret != packet_bytes){
		fprintf(stderr, "%s request error %s\n", __FUNCTION__, strerror(errno));
	}
	//获取STM32的响应
	ret = UpdateRspRcv(update_rsp_buffer,sizeof(update_rsp_buffer));
//	ret = UpdateRspRcv(update_rsp_buffer,8);
//	PRINT("update_rsp_buffer:  \n");
//	for(i = 0; i < ret; i++)
//		PRINT("%x ", update_rsp_buffer[i]);
//	PRINT("\n");

	//解析响应
	if(ret >= RSP_CMD_MIN_BYTES)
	{
		//判断响应
		if((update_rsp_buffer[6] != (char)0X10) || (update_rsp_buffer[7] != UPDATE_REBOOT_CMD))
		{
			PRINT("%s command respond error \n", __FUNCTION__);
			return -1;				
		}	
		{
			switch(update_rsp_buffer[8])
			{
				case REQ_RSP_OK:
					PRINT("%s command execute OK! \n", __FUNCTION__);
					PRINT("jump to boot mode ...... \n");
					break;	
				default:
					PRINT("%s command execute error! \n", __FUNCTION__);
					return -1;
			}
			return 0;
		}		
	}
	return -1;	
}

/******************************************************************************
* NAME:
*      CmdGetVersion
*
* DESCRIPTION:
*      获得smt32应用的版本号
*
* PARAMETERS:
*      fd - The fd of the serial port
*
* RETURN:
*      0 for OK; Others for ERROR
******************************************************************************/
///////////////////////////////////////////////////////////////////////////////
int OrgGetVerPacket(unsigned char *psend_buf)
{
	int index = 0;
	unsigned char check_byte;

//第一级包头
	psend_buf[index++] = 0xEF;
	psend_buf[index++] = 0xFE;
	psend_buf[index++] = 0xA5;
	psend_buf[index++] = 0x5A;
//第一级数据长度，高位在前		
	psend_buf[index++] = 0x00;
	psend_buf[index++] = 0x02;
//数据通道为STM32自身
	psend_buf[index++] = 0x10;

	psend_buf[index++] = 0x80;

//第一级校验字节
	check_byte = PacketXorGenerate(psend_buf,index);	
	psend_buf[index++] = check_byte;
	return index;	
}

int CmdGetVersion(void)
{
	int ret;
	int packet_bytes;
	int i;
	PRINT("Enter %s ......\n", __FUNCTION__);
	packet_bytes = OrgGetVerPacket(update_req_buffer);
//	PRINT("update_req_buffer:  \n");
//	for(i = 0; i < packet_bytes; i++)
//		PRINT("%x ", update_req_buffer[i]);
//	PRINT("\n");
	//发送命令请求
	ret = write_data(global_uart_fd, update_req_buffer, packet_bytes);
	if (ret != packet_bytes){
		fprintf(stderr, "%s request error %s\n", __FUNCTION__, strerror(errno));
	}
	//获取STM32的响应
	ret = UpdateRspRcv(update_rsp_buffer,sizeof(update_rsp_buffer));
//	PRINT("update_rsp_buffer:  \n");
//	for(i = 0; i < ret; i++)
//		PRINT("%x ", update_rsp_buffer[i]);
//	PRINT("\n");

	//解析响应
	if(ret >= RSP_CMD_MIN_BYTES)
	{
		//判断响应
		if((update_rsp_buffer[6] != (char)0X10) || (update_rsp_buffer[7] != UPDATE_GETVER_CMD))
		{
			PRINT("%s command respond error \n", __FUNCTION__);
			return -1;				
		}	
		{
			switch(update_rsp_buffer[8])
			{
				case REQ_RSP_OK:
					PRINT("%s command execute OK! \n", __FUNCTION__);					
					memcpy((void *)(stm_format_version),(void *)(update_rsp_buffer + 9),sizeof(stm_format_version));
					PRINT("major version: %d\n", stm_format_version[0]);
					PRINT("minor version: %d\n", stm_format_version[1]);
					PRINT("sub version: %d\n", (stm_format_version[2] << 8) + stm_format_version[3]);					
					break;	
				default:
					PRINT("%s command execute error! \n", __FUNCTION__);
					return -1;
			}
			return 0;
		}		
	}
	return -1;	
}
/******************************************************************************
* NAME:
*      CmdStart
*
* DESCRIPTION:
*      升级开始命令
*
* PARAMETERS:
*      fd - The fd of the serial port
*
* RETURN:
*      0 for OK; Others for ERROR
******************************************************************************/
///////////////////////////////////////////////////////////////////////////////
int OrgStartPacket(unsigned char *psend_buf,unsigned int app_start_add,unsigned int app_file_size)
{
	int index = 4;
	unsigned char check_byte;

	memcpy((void *)(psend_buf),(void *)(packet_req_head),sizeof(packet_req_head));
	psend_buf[index++] = 0;
	psend_buf[index++] = 9;

	psend_buf[index++] = UPDATE_START_CMD;

	psend_buf[index++] = (app_start_add >> 24)& 0xff;
	psend_buf[index++] = (app_start_add >> 16)& 0xff;
	psend_buf[index++] = (app_start_add >> 8)& 0xff;
	psend_buf[index++] = app_start_add & 0xff;

	psend_buf[index++] = (app_file_size >> 24)& 0xff;
	psend_buf[index++] = (app_file_size >> 16)& 0xff;
	psend_buf[index++] = (app_file_size >> 8)& 0xff;
	psend_buf[index++] = app_file_size & 0xff;
	check_byte = PacketXorGenerate(psend_buf,index);
	psend_buf[index++] = check_byte;
	return index;
}

int CmdStart(void)
{
	int ret;
	int packet_bytes;
	int i;
	int msg_head_type = 0;
	PRINT("Enter %s ......\n", __FUNCTION__);
	packet_bytes = OrgStartPacket(update_req_buffer, UPDATE_APP_START_ADD, update_bin_file_size);
//	PRINT("update_req_buffer:  \n");
//	for(i = 0; i < packet_bytes; i++)
//		PRINT("%x ", update_req_buffer[i]);
//	PRINT("\n");
	//发送命令请求
	ret = write_data(global_uart_fd, update_req_buffer, packet_bytes);
	//ComFunPrintfBuffer(update_req_buffer,packet_bytes);
	if (ret != packet_bytes){
		fprintf(stderr, "%s request error %s\n", __FUNCTION__, strerror(errno));
	}
	//获取STM32的响应
	ret = UpdateRspRcv(update_rsp_buffer,sizeof(update_rsp_buffer));
//	PRINT("update_rsp_buffer:  \n");
//	for(i = 0; i < ret; i++)
//		PRINT("%x ", update_rsp_buffer[i]);
//	PRINT("\n");

	//解析响应
	if(ret >= RSP_CMD_MIN_BYTES)
	{
		//判断响应
		if((update_rsp_buffer[6] != UPDATE_START_CMD))
		{
			PRINT("%s command respond error \n", __FUNCTION__);
			return -1;				
		}	
		{
			switch(update_rsp_buffer[7])
			{
				case REQ_RSP_OK:
					PRINT("%s command execute OK! \n", __FUNCTION__);					
					break;	
				default:
					PRINT("%s command execute error! \n", __FUNCTION__);
					return -1;
			}
			return 0;
		}		
	}
	return -1;	
}
/******************************************************************************
* NAME:
*      CmdDataTrans
*
* DESCRIPTION:
*      传输设计数据
*
* PARAMETERS:
*      fd - The fd of the serial port
*
* RETURN:
*      0 for OK; Others for ERROR
******************************************************************************/
///////////////////////////////////////////////////////////////////////////////
int OrgDataTransPacket(unsigned char *psend_buf,unsigned char *pfile_buf,
					   unsigned short packet_index,unsigned int packet_data_size)
{
	int index = 4;
//	unsigned int packet_data_size;
	unsigned short packet_len;
	unsigned char check_byte;

//	packet_data_size = file_size - *file_send_size;
//	if(packet_data_size > UPDATE_DATA_MAX_BYTES)
//		packet_data_size = UPDATE_DATA_MAX_BYTES;
	
	memcpy((void *)(psend_buf),(void *)(packet_req_head),sizeof(packet_req_head));
	packet_len = packet_data_size + 5; 
	psend_buf[index++] = (packet_len >> 8) & 0xff;
	psend_buf[index++] = packet_len & 0xff;

	psend_buf[index++] = UPDATE_DATA_CMD;

//	packet_index++;
	psend_buf[index++] = (packet_index >> 8) & 0xff;
	psend_buf[index++] = packet_index & 0xff;

	psend_buf[index++] = (packet_data_size >> 8) & 0xff;
	psend_buf[index++] = packet_data_size & 0xff;

	memcpy((void *)(psend_buf + index),(void *)pfile_buf,packet_data_size);

	index += packet_data_size;
//	*file_send_size += packet_data_size;
	check_byte = PacketXorGenerate(psend_buf,index);
	psend_buf[index++] = check_byte;
	return index;
}

int CmdDataTrans(unsigned char *pfile_buf, unsigned short packet_index,unsigned int packet_data_size)
{
	int ret;
	int packet_bytes;
	int i;
	PRINT("Enter %s ......\n", __FUNCTION__);
	packet_bytes = OrgDataTransPacket(update_req_buffer, pfile_buf, packet_index, packet_data_size);
//	PRINT("update_req_buffer: %d \n", packet_bytes);
//	for(i = 0; i < packet_bytes; i++)
//		PRINT("%x ", update_req_buffer[i]);
//	PRINT("\n");
	//发送命令请求
	ret = write_data(global_uart_fd, update_req_buffer, packet_bytes);
	if (ret != packet_bytes){
		fprintf(stderr, "%s request error %s\n", __FUNCTION__, strerror(errno));
	}
	//获取STM32的响应
	ret = UpdateRspRcv(update_rsp_buffer,sizeof(update_rsp_buffer));
//	PRINT("update_rsp_buffer:  \n");
//	for(i = 0; i < ret; i++)
//		PRINT("%x ", update_rsp_buffer[i]);
//	PRINT("\n");

	//解析响应
	if(ret >= RSP_CMD_MIN_BYTES)
	{
		//判断响应
		if((update_rsp_buffer[6] != UPDATE_DATA_CMD))
		{
			PRINT("%s command respond error \n", __FUNCTION__);
			return -1;				
		}	
		{
			switch(update_rsp_buffer[7])
			{
				case REQ_RSP_OK:
					PRINT("%s command execute OK! \n", __FUNCTION__);					
					break;	
				default:
					PRINT("%s command execute error! \n", __FUNCTION__);
					return -1;
			}
			return 0;
		}		
	}
	return -1;	
}
///////////////////////////////////////////////////////////////////////////////////////
int Update(const char *path)
{
	int i,ret,fd;
	int timeout = 0;
	unsigned int update_packet_num,last_packet_len;
	unsigned char bin_file_buffer[UPDATE_DATA_MAX_BYTES];
	fd = open(STM32_BIN_NAME, O_RDONLY, 0);
//	PRINT("fsfd = %d \n", fd);
	if (fd < 0) {
		fprintf(stderr, "open <%s> error %s\n", STM32_BIN_NAME, strerror(errno));
		return -1;
	}
	update_packet_num = (update_bin_file_size / UPDATE_DATA_MAX_BYTES);
	last_packet_len = (update_bin_file_size % UPDATE_DATA_MAX_BYTES);
	lseek(fd, BIN_FILE_HEAD_BYTES, SEEK_SET);
	//PRINT("update_packet_num::: %d\n",update_packet_num);
	for(i = 1; i <= update_packet_num; i++){
		timeout = 0;
		ret = read_data(fd, bin_file_buffer, UPDATE_DATA_MAX_BYTES);
		do{
			timeout++;
			if(timeout >= 3){
				close(fd);
				return -1;
			}
			ret = CmdDataTrans(bin_file_buffer, i, UPDATE_DATA_MAX_BYTES);
		}while(ret != 0);

	}
	timeout = 0;
	ret = read_data(fd, bin_file_buffer, last_packet_len);
	do{
		timeout++;
		if(timeout >= 3){
			close(fd);
			return -1;
		}
		ret = CmdDataTrans(bin_file_buffer, i, last_packet_len);
	}while(ret != 0);

	close(fd);

	return 0;
}
/******************************************************************************
* NAME:
*      CmdEnd
*
* DESCRIPTION:
*      升级结束
*
* PARAMETERS:
*      fd - The fd of the serial port
*
* RETURN:
*      0 for OK; Others for ERROR
******************************************************************************/
///////////////////////////////////////////////////////////////////////////////
int OrgEndPacket(unsigned char *psend_buf)
{
	int index = 4;
	unsigned char check_byte;

	memcpy((void *)(psend_buf),(void *)(packet_req_head),sizeof(packet_req_head));
	psend_buf[index++] = 0;
	psend_buf[index++] = 1;
	psend_buf[index++] = UPDATE_END_CMD;
	check_byte = PacketXorGenerate(psend_buf,index);
	psend_buf[index++] = check_byte;
	return index;	
}

int CmdEnd(void)
{
	int ret;
	int packet_bytes;
	int i;
	PRINT("Enter %s ......\n", __FUNCTION__);
	packet_bytes = OrgEndPacket(update_req_buffer);
//	PRINT("update_req_buffer:  \n");
//	for(i = 0; i < packet_bytes; i++)
//		PRINT("%x ", update_req_buffer[i]);
//	PRINT("\n");
	//发送命令请求
	ret = write_data(global_uart_fd, update_req_buffer, packet_bytes);
	if (ret != packet_bytes){
		fprintf(stderr, "%s request error %s\n", __FUNCTION__, strerror(errno));
	}
	//获取STM32的响应
	ret = UpdateRspRcv(update_rsp_buffer,sizeof(update_rsp_buffer));
//	PRINT("update_rsp_buffer:  \n");
//	for(i = 0; i < ret; i++)
//		PRINT("%x ", update_rsp_buffer[i]);
//	PRINT("\n");

	//解析响应
	if(ret >= RSP_CMD_MIN_BYTES)
	{
		//判断响应
		if((update_rsp_buffer[6] != UPDATE_END_CMD))
		{
			PRINT("%s command respond error \n", __FUNCTION__);
			return -1;				
		}	
		{
			switch(update_rsp_buffer[7])
			{
				case REQ_RSP_OK:
					PRINT("%s command execute OK! \n", __FUNCTION__);					
					break;	
				default:
					PRINT("%s command execute error! \n", __FUNCTION__);
					return -1;
			}
			return 0;
		}		
	}
	return -1;	
}



int BinFileHeadCheck(const char *path)
{
	int i,ret,fd;
	unsigned short crc_check,crc_ret;
	unsigned char bin_file_buffer[BIN_FILE_HEAD_BYTES];
	fd = open(STM32_BIN_NAME, O_RDONLY, 0);
	//PRINT("fsfd = %d \n", fd);
	if (fd < 0) {
		fprintf(stderr, "open <%s> error %s\n", path, strerror(errno));
		return -1;
	}
	ret = read_data(fd, bin_file_buffer, BIN_FILE_HEAD_BYTES);
//	PRINT("read_data = %d \n", ret);
//	for(i = 0; i < ret; i++)
//		PRINT("%x ", bin_file_buffer[i]);
//	PRINT("\n");
	//比较文件头
	if(memcmp((void *)bin_file_buffer, (void *)bin_file_head, sizeof(bin_file_head))){
		PRINT("Update bin file head error!!!\n");
		return -1;
	} else {
		//获得版本号
		memcpy((void *)bin_format_version, (void *)(bin_file_buffer + 8),
					sizeof(bin_format_version));
		//获得CRC校验结果
		crc_check = (bin_file_buffer[12] << 8) + bin_file_buffer[13];
		//获得bin文件长度
		update_bin_file_size = (bin_file_buffer[14] << 24) + 
							   (bin_file_buffer[15] << 16) +
							   (bin_file_buffer[16] << 8) +
							   (bin_file_buffer[17]);
	}

	//读取bin文件
	unsigned char *bin_file = malloc(update_bin_file_size + 4);
//	unsigned char bin_file[20 * 1024];
	lseek(fd, (BIN_FILE_HEAD_BYTES - 4), SEEK_SET);
	ret = read_data(fd, bin_file, (update_bin_file_size + 4));
//	PRINT("read_data = %d \n", ret);
	crc_ret = CRC16(bin_file, (update_bin_file_size + 4));
//	PRINT("read_data = %x \n", crc_ret);
	free(bin_file);
	close(fd);
	//比较CRC校验值
	if(crc_ret == crc_check){
		return 0;
	} else {
		PRINT("Update bin file CRC error!!!\n");
		return -1;
	}
	
}
