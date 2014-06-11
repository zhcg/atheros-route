#include "common.h"
#include "update.h"

unsigned char bin_file_buffer[BIN_FILE_BUFFER_SIZE];

unsigned char packet_req_head[4] = {0xFE,0xEF,0xA5,0x5A};
unsigned char packet_rsp_head[4] = {0xFF,0xEA,0x5A,0xA5};

unsigned char update_req_buffer[UPDATE_REQ_BUF_SIZE];
unsigned char update_rsp_buffer[UPDATE_RSP_BUF_SIZE];

unsigned int update_bin_file_size = 0;
unsigned int file_buffer_data_size = 0;
unsigned char *pfile_buffer_data_offset;
unsigned short update_packet_index = 0;


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
unsigned short CRC16(unsigned char *puchMsg, unsigned int usDataLen)
{
	unsigned char uchCRCHi = 0xFF ; /* 高CRC字节初始化 */
	unsigned char uchCRCLo = 0xFF ; /* 低CRC 字节初始化 */
	unsigned uIndex ; 		/* CRC循环中的索引 */
	while (usDataLen--) 	/* 传输消息缓冲区 */
	{
		uIndex = uchCRCHi ^ *(puchMsg++); /* 计算CRC */
		uchCRCHi = uchCRCLo ^ auchCRCHi[uIndex] ;
		uchCRCLo = auchCRCLo[uIndex] ;
	}
	return (uchCRCHi << 8 | uchCRCLo) ;
}
void ChangeAscStrToHexBuf(unsigned char *pasc,int asc_counts,unsigned char *phex)
{
	unsigned char h,l;
	unsigned char *ppasc;
	unsigned char *pphex;
	int i,j,k;
	j = asc_counts / 2;
	ppasc = pasc;
	pphex = phex;
	k = 0;
	for(i = 0;i < j;i++)
	{
		h = pasc[k++];
		if((h >= '0') && (h <= '9'))
			h -= 0x30;
		else if((h >= 'A') && (h <= 'Z'))
			h -= (0x37);
		else if((h >= 'a') && (h <= 'z'))
			h -= (0x57);
		else
			h = 0;

		l = pasc[k++];
		if((l >= '0') && (l <= '9'))
			l -= 0x30;
		else if((l >= 'A') && (l <= 'Z'))
			l -= (0x37);
		else if((l >= 'a') && (l <= 'z'))
			l -= (0x57);
		else
			l = 0;
		phex[i] = (h << 4) | l;
	}
}
int SearchPacketFromFile(unsigned char *pfile_data,unsigned char *pout,int *if_continue)
{
	unsigned int rest_bytes;
	unsigned char *pend;
	unsigned int i;
	int start_index,end_index,valid_bytes;

	pend = pfile_buffer_data_offset + file_buffer_data_size;
	rest_bytes = pend - pfile_data;
	if(rest_bytes < 8)
	{
		if_continue = 0;
		return 0;
	}
	start_index = 0;
	end_index = 0;
	valid_bytes = 0;
	*if_continue = 0;
	//找EF 01开头的数据
	for(i = 0;i < rest_bytes - 3;i++)
	{
		if((pfile_data[i] == 'e') && (pfile_data[i + 1] == 'f') && (pfile_data[i + 2] == '0') && (pfile_data[i + 3] == '1'))
		{
			start_index = i;
			break;
		}
		if((pfile_data[i] == 'E') && (pfile_data[i + 1] == 'F') && (pfile_data[i + 2] == '0') && (pfile_data[i + 3] == '1'))
		{
			start_index = i;
			break;
		}
	}
	if(i == (rest_bytes - 1))
	{
		*if_continue = 0;
		return -2;
	}
	//找0x0D 0x0A开头的数据
	for(i = start_index + 4;i < rest_bytes - 1;i++)
	{
		if((pfile_data[i] == 0x0D) && (pfile_data[i + 1] == 0x0A))
		{
			end_index = i;
			*if_continue = 1;
			break;
		}
	}
	if(end_index == 0)
	{
		end_index = rest_bytes;
		*if_continue = 0;
	}
	valid_bytes = end_index - start_index;
	if(valid_bytes < 8)
	{
		*if_continue = 0;
		return -3;		
	}
	//将数据转换为十六进制
	ChangeAscStrToHexBuf((unsigned char *)(&pfile_data[start_index]),valid_bytes,(unsigned char *)(pout));
	valid_bytes /= 2;	
	return valid_bytes;
}

int CheckPacketFile(unsigned char *current_vision)
{
	int i;
	unsigned char *pfile_d;
	unsigned char packet_vision[4];
	const unsigned char pakcet_file_head[8] = {'A','p','p','B','i','n',' ',' '};
	unsigned int t1,t2,t;
	unsigned short crc_cal,crc_packet;
	pfile_d = (unsigned char *)(bin_file_buffer);
	//比较头
	for(i = 0;i < 8;i++)
	{
		if(pfile_d[i] != pakcet_file_head[i])//头不正确
			return -1;
	}	
	if(current_vision != NULL)
	{
		//比较版本
		packet_vision[0] = pfile_d[8];
		packet_vision[1] = pfile_d[9];
		packet_vision[2] = pfile_d[10];
		packet_vision[3] = pfile_d[11];
		if((current_vision[0] != packet_vision[0]) || (current_vision[1] != packet_vision[1]))
			return -2;
		t = (current_vision[2] >> 4) & 0x0F;
		t1 = t * 1000;
		t = (current_vision[2] & 0x0F);
		t1 += t * 100;
		t = (current_vision[3] >> 4) & 0x0F;
		t1 += t * 10;
		t = (current_vision[3] & 0x0F);
		t1 += t;

		t = (packet_vision[2] >> 4) & 0x0F;
		t2 = t * 1000;
		t = (packet_vision[2] & 0x0F);
		t2 += t * 100;
		t = (packet_vision[3] >> 4) & 0x0F;
		t2 += t * 10;
		t = (packet_vision[3] & 0x0F);
		t2 += t;
		if((t1 >= t2))
			return -3;
	}
	//计算CRC
	crc_packet = (pfile_d[12] << 8) | pfile_d[13];
	crc_cal = CRC16((unsigned char *)(&pfile_d[14]),update_bin_file_size - PACKET_FILE_LENGTH_OFFSET);
	if(crc_packet != crc_cal)
		return -4;

	//判断总长度
	t = (pfile_d[14] << 24) | (pfile_d[15] << 16) | (pfile_d[16] << 8) | (pfile_d[17]) ;
	if(t != (update_bin_file_size - PACKET_FILE_HEAD_BYTES))
		return -5;

	pfile_buffer_data_offset = bin_file_buffer + PACKET_FILE_HEAD_BYTES;
	file_buffer_data_size = update_bin_file_size - PACKET_FILE_HEAD_BYTES;
	return 0;
}

int WaitingRsp(void)
{
	int rcv_index;
	int ret,i;
	int loops = 0;
	rcv_index = 0;
	unsigned char recv_temp_buf[64];
	do
	{
		loops++;
		if(loops > UPDATE_RECV_TIME_OUT_TICKS)
			return -2;

		usleep(UPDATE_RECV_TICKS_MS*1000);
		//接收数据	
//		ret = UartRead(recv_temp_buf,sizeof(recv_temp_buf));
		ret = read(as532_fd,recv_temp_buf,sizeof(recv_temp_buf));
		//PRINT("ret = %d\n",ret);
		if(ret < 0)
		{
			continue;
		}
		if(ret > 0)
		{
			if(rcv_index == 0)//消除干扰数据
			{
				for(i = 0;i < ret;i++)
				{
					if(recv_temp_buf[i] == UPDATE_PACKET_HEAD1)
						break;
				}
				if(i == ret)
					continue;
				for(;i < ret;i++)
				{
					update_rsp_buffer[rcv_index++] = recv_temp_buf[i];
				}
			}
			else
			{
				memcpy((void *)(&update_rsp_buffer[rcv_index]),(void *)(recv_temp_buf),ret);
				rcv_index += ret;
			}
		}
		if(rcv_index >= 8)
		{
			if((update_rsp_buffer[0] != UPDATE_PACKET_HEAD1) || (update_rsp_buffer[1] != UPDATE_PACKET_HEAD2))
			{
				return 1;
			}
			if((update_rsp_buffer[2] != UPDATE_PACKET_CMD) && (update_rsp_buffer[2] != UPDATE_SETBR_CMD) && (update_rsp_buffer[2] != UPDATE_WRITEREG_CMD))
			{
				return 2;
			}
			if((update_rsp_buffer[3] ==0x05))
			{
				return 5;
			}
			if((update_rsp_buffer[3] ==0x06))
			{
				return 6;
			}
			if((update_rsp_buffer[3] ==0x00))
			{
				return 0;
			}
			return 0;
		}
	}while(1);
	return 0;
}
int LoadPacketFile(const char *ppacket_file_path)
{
	FILE *pfile;
	int read_ret;

	update_bin_file_size = 0;
	pfile = fopen(ppacket_file_path,"rb");
	if(pfile == 0)
	{
		return -1;
	}
	read_ret = fread(bin_file_buffer,1,sizeof(bin_file_buffer),pfile);
	if(read_ret > 0)
	{
		update_bin_file_size = read_ret;
	}
	else
	{
		fclose(pfile);
		return -2;
	}
	fclose(pfile);
	return 0;
}
int As532PacketCheck(const char *packet_file_path,unsigned char *pcurrent_vision)
{
	int rtn;
	if(pcurrent_vision != NULL)
		PRINT("packet_file_path is %s,pcurrent_vision is::%02X%02X%02X%02X\n",packet_file_path,pcurrent_vision[0],pcurrent_vision[1],pcurrent_vision[2],pcurrent_vision[3]);	
	//打开文件读取文件
	rtn = LoadPacketFile((const char *)packet_file_path);
	if(rtn != 0)
	{
		PRINT("check err!\r\n");
		switch(rtn)
		{
			case -1:
				PRINT("open packet err!\r\n");
				return -1;
			case -2:
				PRINT("read packet err!\r\n");
				return -2;
		}
		return -3;
	}
	PRINT("file::%s!\r\n",(const char *)(packet_file_path));	
	PRINT("file size::%d\r\n",update_bin_file_size);	
	PRINT("=================================================\r\n");

	//检查升级包的正确性
	rtn = CheckPacketFile(pcurrent_vision);
	if(rtn < 0)
	{
		PRINT("check err!\r\n");
		switch(rtn)
		{
			case -1:
				PRINT("head err!\r\n");
				return -4;
			case -2:
				PRINT("major err!\r\n");
				return -5;
			case -3:
				PRINT("minor err!\r\n");
				return -6;
			case -4:
				PRINT("check err!\r\n");
				return -7;
			case -5:
				PRINT("leng err!\r\n");
				return -8;
		}
		return -9;
	}		
	return 0;
}
int As532Update(const char *packet_file_path,unsigned char *pcurrent_vision) 
{
	unsigned short packet_index = 0;
	int packet_bytes;
	int rtn,i;
	unsigned int sent_bytes;
	unsigned char *psend_pos;
	int continue_flag;
	if(pcurrent_vision != NULL)
		PRINT("packet_file_path is %s,pcurrent_vision is::%02X%02X%02X%02X\n",packet_file_path,pcurrent_vision[0],pcurrent_vision[1],pcurrent_vision[2],pcurrent_vision[3]);	

	//打开文件读取文件
	rtn = LoadPacketFile((const char *)packet_file_path);
	if(rtn != 0)
	{
		PRINT("update stop!\r\n");
		switch(rtn)
		{
			case -1:
				PRINT("open packet err!\r\n");
				break;
			case -2:
				PRINT("read packet err!\r\n");
				break;
		}
		return -1;
	}
	PRINT("file::%s!\r\n",(const char *)(packet_file_path));	
	PRINT("file size::%d\r\n",update_bin_file_size);	
	PRINT("=================================================\r\n");

	//检查升级包的正确性
	rtn = CheckPacketFile(pcurrent_vision);
	if(rtn < 0)
	{
		PRINT("update stop!\r\n");
		switch(rtn)
		{
			case -1:
				PRINT("head err!\r\n");
				return -2;
			case -2:
				PRINT("major err!\r\n");
				return -3;
			case -3:
				PRINT("minor err!\r\n");
				return -4;
			case -4:
				PRINT("check err!\r\n");
				return -5;
			case -5:
				PRINT("leng err!\r\n");
				return -6;
		}
		return -7;
	}
	psend_pos = pfile_buffer_data_offset;
	
	PRINT("*************************************************\r\n");
	PRINT("start!\r\n");
	PRINT("DELAY::%d\r\n",UPDATE_PACKET_DELAY_MS);

	packet_index = 0;
	sent_bytes = 0;
	update_packet_index = 0;
	do
	{	
		usleep(UPDATE_PACKET_DELAY_MS*1000);
		update_packet_index++;
		//从文件中找对应一行数据
		packet_bytes = SearchPacketFromFile(psend_pos,update_req_buffer,&continue_flag);
		if(packet_bytes > 1)
		{
			PRINT("%s,%d\n",__FUNCTION__,__LINE__);
			psend_pos += (packet_bytes * 2); 
			if(continue_flag == 1)
			{
				psend_pos += 2;
			}
		}
		else 
		{
			PRINT("%s,%d\n",__FUNCTION__,__LINE__);
			break;
		}
		PRINT("*************************************************\r\n");
		PRINT("send::%d bytes.\r\n",sent_bytes );
		PRINT("packet num::%d.\r\n",update_packet_index );
		PRINT("size::%d bytes.\r\n",packet_bytes );
		PRINT("head::%02X%02X%02X%02X.\r\n",update_req_buffer[0],update_req_buffer[1],update_req_buffer[2],update_req_buffer[3]);
		PRINT("tail::%02X%02X%02X%02X.\r\n",update_req_buffer[packet_bytes - 4],update_req_buffer[packet_bytes - 3],update_req_buffer[packet_bytes - 2],update_req_buffer[packet_bytes - 1] );

		//发送报文
//		rtn = UartWrite(update_req_buffer,packet_bytes);
		rtn = write(as532_fd,update_req_buffer,packet_bytes);
		PRINT("rtn  = %d\n",rtn);
		if(rtn != packet_bytes)
		{
			PRINT("send err!\r\n");
			return -8;
		}
		//等待响应
		
		rtn = WaitingRsp();
		if(rtn > -1)
		{
			PRINT("respond ::");
			for(i = 0;i < 8;i++)
				printf("%02X",update_rsp_buffer[i]);
			printf("\n");
			switch(rtn)
			{
				case 1:
					PRINT("AS532 respond head err!\r\n");
					return -9;
				case 2:
					PRINT("AS532 respond cmd err!\r\n");
					return -10;
				case 5:
					PRINT("AS532 respond check err!\r\n");
					return -11;
				case 6:
					PRINT("AS532 write err!\r\n");
					return -12;
				case 0:
					PRINT("AS532 write ok!\r\n");
					break;
			}
		}
		else
		{
			PRINT("AS532 no respond!\r\n");
			return -13;
		}
		sent_bytes += packet_bytes;
	}while(continue_flag == 1);
	PRINT("AS532 update success!\r\n");
	return 0;
}


//for test
//int main(void)
//{
	//unsigned char path[] = "dfdfdfdfdf";
	//unsigned char vision[4] = {0x01,0x02,0x03,0x04};
	//As532PacketCheck((const char *)path,vision);
	//As532Update((const char *)path,vision);
	//
	//return 1;		
//}
