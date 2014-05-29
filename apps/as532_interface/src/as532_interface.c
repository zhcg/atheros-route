#include "json.h"
#include "common.h"
#include "our_md5.h"
#include "as532_interface.h"

int as532_fd = -1;
int remote_server_fd = -1;
const char hex_to_asc_table[16] = {0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
	0x38,0x39,0x41,0x42,0x43,0x44,0x45,0x46};
unsigned char respond_buffer[BUFFER_SIZE_1K] = {0};
unsigned char as532_version[4]={0};
unsigned char as532_conf_version[4]={0};
unsigned char as532_sn[BUF_LEN_128]={0};
unsigned char as532_version_description[BUF_LEN_128]={0};
unsigned char remote_as532_version[4]={0};
unsigned char remote_as532_path[BUF_LEN_256]={0};
unsigned char remote_as532_conf_md5[BUF_LEN_64]={0};
char remote_ip_buf[BUF_LEN_128]={0};
char remote_port_buf[BUF_LEN_64]={0};
char conf_ver[BUF_LEN_64]={0};

int ComFunChangeHexBufferToAsc(unsigned char *hexbuf,int hexlen,char *ascstr)
{
	unsigned char i;
	unsigned char ch,h,l;
	char *pdes = ascstr;
	for(i = 0;i < hexlen;i++)
	{
		ch = hexbuf[i];
		h = (ch >> 4) & 0x0F;
		l = ch & 0x0F;
		*(pdes++) = hex_to_asc_table[h];
		*(pdes++) = hex_to_asc_table[l];
	}
	*pdes = '\0';
	return hexlen * 2;
}
//输出一个数组的内容，以十六进制模式
void ComFunPrintfBuffer(unsigned char *pbuffer,unsigned char len)
{
	char *pstr;
	pstr = (char *)malloc(512);
	if(pstr == NULL)
		return;
	ComFunChangeHexBufferToAsc(pbuffer,len,pstr);

	printf("%s\r\n",pstr);

	free((void *)pstr);
}
unsigned char sumxor(const  unsigned char  *arr, int len)
{
	int i=0;
	unsigned char sum = 0;
	for(i=0; i<len; i++)
	{
		sum ^= arr[i];
	}

	return sum;
}

//test CPU is littleEnd or Bigend; 
//ret: 1 :Little Endian￡?0: BIG Endian
int testendian() 
{
	int x = 1;
	return *((char*)&x);
}

int serialConfig(int serial_fd, speed_t baudrate)
{
	struct termios options;
	tcgetattr(serial_fd, &options);
	options.c_cflag  |= ( CLOCAL | CREAD );//Ignore modem control lines ,Enable receiver
	options.c_cflag &= ~CSIZE; //Character size mask
	options.c_cflag &= ~CRTSCTS;//Enable  RTS/CTS  (hardware)  flow  control *
	options.c_cflag |= CS8;// 8bit *
	options.c_cflag &= ~CSTOPB;//Set two stop bits, rather than one  *
	options.c_iflag |= IGNPAR;//Ignore framing errors and parity errors  *
	options.c_iflag &= ~(ICRNL |IXON);//Enable XON/XOFF flow control on output
	//Translate  carriage  return to newline on input (unless IGNCR is set).

	options.c_oflag = 0;
	options.c_lflag = 0;

	cfsetispeed(&options, baudrate /*B9600*/); //*
	cfsetospeed(&options, baudrate /*B9600*/); //*
	tcsetattr(serial_fd,TCSANOW,&options);//the change occurs immediately

	PRINT("serial config finished\n");
	return 0;
}

int send_request(unsigned char cmd,unsigned char order,unsigned char *param,int len,unsigned char *data)
{
	int index = 0;
	unsigned char *buf = malloc(len+32);
	if(buf == NULL)
	{
		PRINT("malloc err\n");
		return -2;
	}
	buf[index++] = (unsigned char)AS532_HEAD1;
	buf[index++] = (unsigned char)AS532_HEAD2;
	buf[index++] = (CMD_HEAD>>0x08);
	buf[index++] = (CMD_HEAD&0xFF);
	buf[index++] = (cmd);
	buf[index++] = (order);
	if(param != NULL)
	{
		buf[index++] = (param[0]);
		buf[index++] = (param[1]);
	}
	else
	{
		buf[index++] = (0);
		buf[index++] = (0);
	}
	buf[index++] = (len>>0x08);
	buf[index++] = (len&0xFF);
	if(data != NULL)
	{
		memcpy(&buf[index],data,len);
		index+=len;
	}
	buf[index++] = sumxor(data,len);
	if(as532_fd < 0 || write(as532_fd,buf,index) != index)
	{
		PRINT("send request fail.\n");
		free(buf);
		return -1;
	}
	ComFunPrintfBuffer(buf,index);
	PRINT("send request success.%d\n",index);
	free(buf);
	return 0;
}

int clean_tmp_file()
{
	system("rm -rf /tmp/tmp_as532_ver_file");
	return 0;
}

int update_tmp_file()
{
	system("echo UPDATING > /tmp/tmp_as532_ver_file");
	return 0;
}

int PacketHeadSearch(unsigned char *pbuf,int offset,int valid_bytes)
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
		if((ppcom[i] == (unsigned char)0xA5) && (ppcom[i + 1] == (unsigned char)0x5A))
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

	//A5 5A 02 00 00 00 04 04 00 00 01 05
	packet_len = (pbuf[offset+5] * 256) + pbuf[offset+6];
	packet_len += (7);
	if(valid_bytes <= packet_len){
		PRINT("Error: respond valid_bytes < packet_len\n");
		return -1;
	}
	check_byte = sumxor(pbuf+offset+7,packet_len-7);
	if(check_byte == pbuf[packet_len+offset])
	{
		packet_len++;
		return packet_len;
	}
	PRINT("Error: respond xor check error \n");
	return -1;
}

int get_respond(unsigned char *prcv_buffer,int buf_size)
{
	PRINT("%s\n",__FUNCTION__);
	int rcv_index;
	int i,j;
	int ret = 0;
	int loops = 0;
	rcv_index = 0;
	do
	{
		loops++;
		if(loops > 40){
			PRINT("Error: get respond timeout \n");
			return -1;
		}
		usleep(25 * 1000);
		ret = read(as532_fd, (prcv_buffer + rcv_index), (buf_size - rcv_index));
		if(ret > 0)
		{
			PRINT("read : ");
			ComFunPrintfBuffer(prcv_buffer + rcv_index,ret);
			rcv_index += ret;
			//PRINT("ret = %d\n",ret);
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
					else if(j == -1)
						//return j;
						continue;
				}
			}
		}
	}while(1);
}

int get_532_ver()
{
	int ret = 0;
	char tmp_buf[BUF_LEN_128]={0};
	char tail[4]={0x00,0xff,0x00,0x00};
	if(send_request(GETVER,0,NULL,sizeof(tail),tail)==0)
	{
		memset(respond_buffer,0,sizeof(respond_buffer));
		ret = get_respond(respond_buffer,sizeof(respond_buffer));
		//解析响应
		//a5 5a 02 00 00 00 04 33 31 30 35 xx
		if(ret >= RSP_CMD_MIN_BYTES)
		{
			//判断响应
			if(respond_buffer[2] != (char)GETVER)
			{
				PRINT("%s command respond error \n", __FUNCTION__);
				return -1;				
			}	
			switch(respond_buffer[4])
			{
				case REQ_RSP_OK:
					PRINT("%s command execute OK! \n", __FUNCTION__);					
					memset(as532_version,0,sizeof(as532_version));				
					memcpy((void *)(as532_version),(void *)(respond_buffer + 7),sizeof(as532_version));
					PRINT("major as532_version: %d\n", as532_version[0]);
					PRINT("minor as532_version: %d\n", as532_version[1]);
					PRINT("sub as532_version: %d\n", (as532_version[2] << 8) + as532_version[3]);					
					break;	
				default:
					PRINT("%s command execute error! \n", __FUNCTION__);
					return -1;
			}
			return 0;
		}
	}
	return -2;
}

int get_532_sn()
{
	int ret = 0;
	char tail[4]={0x00,0xff,0x00,0x00};
	if(send_request(GETSN,0,NULL,sizeof(tail),tail)==0)
	{
		memset(respond_buffer,0,sizeof(respond_buffer));
		ret = get_respond(respond_buffer,sizeof(respond_buffer));
		//解析响应
		//a5 5a 03 00 00 00 04 33 31 30 35 xx
		if(ret >= RSP_CMD_MIN_BYTES)
		{
			//判断响应
			if(respond_buffer[2] != (char)GETSN)
			{
				PRINT("%s command respond error \n", __FUNCTION__);
				return -1;				
			}	
			switch(respond_buffer[4])
			{
				case REQ_RSP_OK:
					PRINT("%s command execute OK! \n", __FUNCTION__);	
					memset(as532_sn,0,sizeof(as532_sn));				
					memcpy((void *)(as532_sn),(void *)(respond_buffer + 7),(respond_buffer[5]*256+respond_buffer[6]-2));
					PRINT("sn: ");
					ComFunPrintfBuffer(as532_sn,sizeof(as532_sn));
					break;	
				default:
					PRINT("%s command execute error! \n", __FUNCTION__);
					return -1;
			}
			return 0;
		}
	}
	return -2;
}

int get_532_ver_des()
{
	int ret = 0;
	char tmp_buf[BUF_LEN_128]={0};
	char tail[4]={0x00,0xff,0x00,0x00};
	if(send_request(GETVERDES,0,NULL,sizeof(tail),tail)==0)
	{
		memset(respond_buffer,0,sizeof(respond_buffer));
		ret = get_respond(respond_buffer,sizeof(respond_buffer));
		//解析响应
		//a5 5a 03 00 00 00 04 33 31 30 35 xx
		if(ret >= RSP_CMD_MIN_BYTES)
		{
			//判断响应
			if(respond_buffer[2] != (char)GETVERDES)
			{
				PRINT("%s command respond error \n", __FUNCTION__);
				return -1;				
			}	
			switch(respond_buffer[4])
			{
				case REQ_RSP_OK:
					PRINT("%s command execute OK! \n", __FUNCTION__);
					memset(as532_version_description,0,sizeof(as532_version_description));					
					memcpy((void *)(as532_version_description),(void *)(respond_buffer + 7),(respond_buffer[5]*256+respond_buffer[6]-2));
					PRINT("des: ");
					printf("%s\n",as532_version_description);
					
					sprintf(tmp_buf,"echo %s > /tmp/tmp_as532_ver_file",as532_version_description);
					system(tmp_buf);
					break;	
				default:
					PRINT("%s command execute error! \n", __FUNCTION__);
					return -1;
			}
			return 0;
		}
	}
	return -2;
}

int jump_to_boot()
{
	int ret = 0;
	memset(respond_buffer,0,sizeof(respond_buffer));
	char tail[4]={0x00,0xff,0x00,0x00};
	if(send_request(JUMPBOOT,0,NULL,sizeof(tail),tail)==0)
	{
		ret = get_respond(respond_buffer,sizeof(respond_buffer));
		//解析响应
		//a5 5a 03 00 00 00 04 33 31 30 35 xx
		if(ret >= RSP_CMD_MIN_BYTES)
		{
			//判断响应
			if(respond_buffer[2] != (char)JUMPBOOT)
			{
				PRINT("%s command respond error \n", __FUNCTION__);
				return -1;				
			}	
			switch(respond_buffer[4])
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
	return -2;
}

int create_socket_client()
{
	struct sockaddr_in cliaddr;
    struct timeval timeo = {0};   
    timeo.tv_sec = 5;
	socklen_t len = sizeof(timeo);  
	remote_server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(remote_server_fd < 0)
	{
		PRINT("create client err.\n");
		return -1;
	}
	setsockopt(remote_server_fd, SOL_SOCKET, SO_SNDTIMEO, &timeo, len);
	cliaddr.sin_family = AF_INET;
	inet_pton(AF_INET,remote_ip_buf,&cliaddr.sin_addr);//服务器ip
	cliaddr.sin_port = htons(atoi(remote_port_buf));//注意字节顺序
	if(connect(remote_server_fd, (struct sockaddr*)&cliaddr, sizeof(cliaddr))<0)
	{
		if (errno == EINPROGRESS) 
		{  
			PRINT("connect ip:%s port:%d timeout.\n",remote_ip_buf,atoi(remote_port_buf));
			return -2;  
		}         
		PRINT("connect ip:%s port:%d err.\n",remote_ip_buf,atoi(remote_port_buf));
		return -3;
	}
	PRINT("connected ip:%s port:%d.\n",remote_ip_buf,atoi(remote_port_buf));
	return 0;
}

char *generate_get_remote_msg(char *remote_type)
{
    char *text;
	json_t *entry, *label, *value, *array;
	setlocale (LC_ALL, "");//设为系统默认地域信息

	// create an entry node
	entry = json_new_object();

//    第一部分，打印结果：
//    {"entry":{"name":"Andew","phone":"555 123 456"}}
	array = json_new_array();
	label = json_new_string("sysSoftwareList");
	value = json_new_string(remote_type);
 	//value = json_new_string("HBD_B6L_AS532_V4.0.1_140326_R");
    json_insert_child(array,value);
	json_insert_child(label, array);
	json_insert_child(entry, label);

	// insert the first label-value pair
	label = json_new_string("functionId");
	value = json_new_string("0007_0008_0001");
	json_insert_child(label, value);
	json_insert_child(entry, label);

	// insert the second label-value pair
	label = json_new_string("baseId");
	value = json_new_string("0000000000000000000000000000001010");
	json_insert_child(label, value);
	json_insert_child(entry, label);

	label = json_new_string("userType");
	value = json_new_string("1");
	json_insert_child(label, value);
	json_insert_child(entry, label);

	label = json_new_string("startTime");
	value = json_new_string("12000");
	json_insert_child(label, value);
	json_insert_child(entry, label);


	// print the result
	json_tree_to_string(entry, &text);
	printf("%s\n",text);

    // clean up
    json_free_value(&entry);
    return text;
}

int prase_532_ver_msg(char *msg)
{
	char *msgp;
	char *end;
	char ver_buf[32]={0};
	char tmp_buf[3]={0};
	int minor = 0;
	int i;
	for(i=0;i<strlen(msg);i++)
		printf("%c",msg[i]);
	printf("\n");
	if(testendian()==0)
	{
		PRINT("%s\n",B6L_AS532_PACKET_NAME);
		msgp = strstr(msg,B6L_AS532_PACKET_NAME);
	}
	if(testendian()==1)
	{
		PRINT("%s\n",A20_AS532_PACKET_NAME);
		msgp = strstr(msg,A20_AS532_PACKET_NAME);
	}
	if(msgp == NULL)
	{
		PRINT("remote msg err\n");
		return -1;
	}
	msgp = strstr(msg,AS532_VER_NUM);
	if(msgp == NULL)
	{
		PRINT("no ver\n");
		return -2;
	}
	msgp = msgp + strlen(AS532_VER_NUM);
	end = strstr(msgp,"\"");
	if(end == NULL)
	{
		PRINT("ver err\n");
		return -3;
	}
	memcpy(ver_buf,msgp,end-msgp);
	
	remote_as532_version[0] = ver_buf[0]-48;
	remote_as532_version[1] = ver_buf[2]-48;
	if(strlen(ver_buf) == 5)
	{
		remote_as532_version[2] = 0x0;
		remote_as532_version[3] = ver_buf[4]-48;
		minor = remote_as532_version[3];
	}
	else if(strlen(ver_buf) == 6)
	{
		tmp_buf[0] = ver_buf[4];
		tmp_buf[1] = ver_buf[5];
		tmp_buf[2] = '\0';
		minor = atoi(tmp_buf);
		remote_as532_version[2] = minor >> 8;
		remote_as532_version[3] = minor&0xff;
	}
	PRINT("remote_as532 ver = %d.%d.%d\n",remote_as532_version[0],remote_as532_version[1],minor);
	msgp = strstr(msg,AS532_VER_PATH);
	if(msgp == NULL)
	{
		PRINT("no path\n");
		return -4;
	}
	msgp = msgp + strlen(AS532_VER_PATH);
	//PRINT("msgp = %s\n",msgp);
	end = strstr(msgp,"\"");
	if(end == NULL)
	{
		PRINT("path err\n");
		return -5;
	}
	memset(remote_as532_path,0,sizeof(remote_as532_path));
	memcpy(remote_as532_path,msgp,end-msgp);
	PRINT("remote_as532_path = %s\n",remote_as532_path);
	return 0;	
}

int prase_532_conf_msg(char *msg)
{
	char *msgp;
	char *end;
	char ver_buf[32]={0};
	char tmp_buf[3]={0};
	char name[16]={0};
	int minor = 0;
	int i;
	for(i=0;i<strlen(msg);i++)
		printf("%c",msg[i]);
	printf("\n");
	msgp = strstr(conf_ver,"_V");
	if(msgp == NULL)
	{
		msgp = strstr(conf_ver,"_v");
		if(msgp == NULL)
		{
			printf("config file err.\nfile's first line must be CONF_VER=\"Vx.x.x\"\n");
			exit(-1);
		}
	}
	memcpy(name,conf_ver,msgp-conf_ver);
	PRINT("%s\n",name);
	msgp = strstr(msg,name);
	if(msgp == NULL)
	{
		PRINT("remote msg err\n");
		return -1;
	}
	msgp = strstr(msg,AS532_VER_NUM);
	if(msgp == NULL)
	{
		PRINT("no ver\n");
		return -2;
	}
	msgp = msgp + strlen(AS532_VER_NUM);
	end = strstr(msgp,"\"");
	if(end == NULL)
	{
		PRINT("ver err\n");
		return -3;
	}
	memcpy(ver_buf,msgp,end-msgp);
	
	remote_as532_version[0] = ver_buf[0]-48;
	remote_as532_version[1] = ver_buf[2]-48;
	if(strlen(ver_buf) == 5)
	{
		remote_as532_version[2] = 0x0;
		remote_as532_version[3] = ver_buf[4]-48;
		minor = remote_as532_version[3];
	}
	else if(strlen(ver_buf) == 6)
	{
		tmp_buf[0] = ver_buf[4];
		tmp_buf[1] = ver_buf[5];
		tmp_buf[2] = '\0';
		minor = atoi(tmp_buf);
		remote_as532_version[2] = minor >> 8;
		remote_as532_version[3] = minor&0xff;
	}
	PRINT("remote_as532 ver = %d.%d.%d\n",remote_as532_version[0],remote_as532_version[1],minor);
	msgp = strstr(msg,AS532_VER_PATH);
	if(msgp == NULL)
	{
		PRINT("no path\n");
		return -4;
	}
	msgp = msgp + strlen(AS532_VER_PATH);
	//PRINT("msgp = %s\n",msgp);
	end = strstr(msgp,"\"");
	if(end == NULL)
	{
		PRINT("path err\n");
		return -5;
	}
	memset(remote_as532_path,0,sizeof(remote_as532_path));
	memcpy(remote_as532_path,msgp,end-msgp);
	PRINT("remote_as532_path = %s\n",remote_as532_path);
	msgp = strstr(msg,AS532_CONF_MD5);
	if(msgp == NULL)
	{
		PRINT("no md5\n");
		return -4;
	}
	msgp = msgp + strlen(AS532_CONF_MD5);
	//PRINT("msgp = %s\n",msgp);
	end = strstr(msgp,"\"");
	if(end == NULL)
	{
		PRINT("md5 err\n");
		return -5;
	}
	memset(remote_as532_conf_md5,0,sizeof(remote_as532_conf_md5));
	memcpy(remote_as532_conf_md5,msgp,end-msgp);
	PRINT("remote_as532_conf_md5 = %s\n",remote_as532_conf_md5);
	return 0;	
}

int get_remote_532_ver_request()
{
	int loops = 0;
	char recvbuf[BUFFER_SIZE_2K]={0};
	int recv_ret = 0;
	int i;
	int length = 0;
	do
	{
		loops++;
		usleep(50*1000);
		if(loops >= 100)
		{
			PRINT("get remote 532 ver request timeout\n");
			return -1;
		}
		recv_ret = recv(remote_server_fd,recvbuf,BUFFER_SIZE_2K,MSG_DONTWAIT);
		if(recv_ret > 0 )
		{
			PRINT("recv_ret: %d\n",recv_ret);			
			//ComFunPrintfBuffer(recvbuf,recv_ret);
			//PRINT("recv: %s\n",recvbuf);
			memcpy(&length,recvbuf,4);
			PRINT("length = %d\n",length);
			if(recv_ret > 16)
				return prase_532_ver_msg(recvbuf+16);
		}
	}while(1);
	return -4;
}

int get_remote_532_conf_request()
{
	int loops = 0;
	char recvbuf[BUFFER_SIZE_2K]={0};
	int recv_ret = 0;
	int i;
	int length = 0;
	do
	{
		loops++;
		usleep(50*1000);
		if(loops >= 100)
		{
			PRINT("get remote 532 conf request timeout\n");
			return -1;
		}
		recv_ret = recv(remote_server_fd,recvbuf,BUFFER_SIZE_2K,MSG_DONTWAIT);
		if(recv_ret > 0 )
		{
			PRINT("recv_ret: %d\n",recv_ret);			
			//ComFunPrintfBuffer(recvbuf,recv_ret);
			//PRINT("recv: %s\n",recvbuf);
			memcpy(&length,recvbuf,4);
			PRINT("length = %d\n",length);
			if(recv_ret > 16)
				return prase_532_conf_msg(recvbuf+16);
		}
	}while(1);
	return -4;
}

int generate_msg_head(char *buf,int len,short type,short flag)
{
	int index = 0;
	//int len_t = len+16;
	buf[index++]= (len+12) >> 24;
	buf[index++]= (len+12) >> 16;
	buf[index++]= (len+12) >> 8;
	buf[index++]= (len+12)&0xff;
	buf[index++]= type >> 8;
	buf[index++]= type&0xff;
	memcpy(&buf[index],PROTOCOL_VER,4);
	index += 4;
	buf[index++]= flag >> 8;
	buf[index++]= flag&0xff;
	memcpy(&buf[index],"1234",4);
	index += 4;
	return 0;
}

int get_remote_532_ver()
{
	char *json_str;
	char *send_str;
	int ret = -1;
	if(create_socket_client()==0)
	{
		json_str = generate_get_remote_msg(as532_version_description);
		send_str = malloc(strlen(json_str)+32);
		PRINT("json_str = %d\n",strlen(json_str));
		if(send_str == NULL)
		{
			close(remote_server_fd);
			remote_server_fd = -1;
			free(json_str);
			return -1;
		}
		memset(send_str,0,strlen(json_str)+32);
		generate_msg_head(send_str,strlen(json_str),REQUEST_TYPE,0);
		memcpy(send_str+16,json_str,strlen(json_str));
		//ComFunPrintfBuffer(send_str,strlen(json_str)+16);
		write(remote_server_fd,send_str,strlen(json_str)+16);
		ret = get_remote_532_ver_request();
		free(json_str);
		free(send_str);
		close(remote_server_fd);
		remote_server_fd = -1;
	}
	return ret;
}

int set_default()
{
	char *p;
	update_tmp_file();
	jump_to_boot();
	if(testendian()==0)
		p = B6L_DEFAULT_AS532_IMAGE;
	if(testendian()==1)
		p = A20_DEFAULT_AS532_IMAGE;
	if(As532PacketCheck((const char *)p,NULL)==0)
	{
		if(As532Update((const char *)p,NULL)==0)
		{
			sleep(3);
			clean_tmp_file();
			return 0;
		}
	}
	clean_tmp_file();
	return -1;
}

int check_ver()
{
	int ret = memcmp((void *)remote_as532_version, (void *)as532_version, sizeof(as532_version)/2);
	if(ret != 0)
	{
		PRINT("ver err\n");
		return -1;
	}
	ret = memcmp((void *)remote_as532_version, (void *)as532_version, sizeof(as532_version));
	if(ret <= 0)
	{
		PRINT("as532 is newest\n");
		return -2;
	}
	return 0;
}

int check_conf_ver()
{
	int ret = memcmp((void *)remote_as532_version, (void *)as532_version, sizeof(as532_version)/2);
	if(ret != 0)
	{
		PRINT("ver err\n");
		return -1;
	}
	ret = memcmp((void *)remote_as532_version, (void *)as532_version, sizeof(as532_version));
	if(ret <= 0)
	{
		PRINT("as532conf is newest\n");
		return -2;
	}
	return 0;
}

int get_remote_532_image()
{
	pid_t status;
	int ret = 0;
	// /bin/wget url
	char cmd[BUF_LEN_256]={0};
	sprintf(cmd,"%s%s%s%s%s",WGET," -O ",DOWNLOAD_AS532_FILE," ",remote_as532_path);
	PRINT("cmd = %s\n",cmd);
	status = system(cmd);
	PRINT("status = %d\n",status);
	if(status == -1)
	{
		PRINT("system fail!\n");
		return -1;
	}
	else
	{
		ret = WEXITSTATUS(status);
		if(ret == 0)
		{
			PRINT("download success\n");
			return 0;
		}
		PRINT("download fail!\n");
		return -2;
	}
}

int get_remote_532_conf()
{
	pid_t status;
	int ret = 0;
	// /bin/wget url
	char cmd[BUF_LEN_256]={0};
	sprintf(cmd,"%s%s%s%s%s",WGET," -O ",DOWNLOAD_AS532_TEMP_CONF_FILE," ",remote_as532_path);
	PRINT("cmd = %s\n",cmd);
	status = system(cmd);
	PRINT("status = %d\n",status);
	if(status == -1)
	{
		PRINT("system fail!\n");
		return -1;
	}
	else
	{
		ret = WEXITSTATUS(status);
		if(ret == 0)
		{
			PRINT("download success\n");
			return 0;
		}
		PRINT("download fail!\n");
		return -2;
	}
}

int get_remote_532_conf_ver()
{
	char *json_str;
	char *send_str;
	int ret = -1;
	if(create_socket_client()==0)
	{
		json_str = generate_get_remote_msg(conf_ver);;
		send_str = malloc(strlen(json_str)+32);
		PRINT("json_str = %d\n",strlen(json_str));
		if(send_str == NULL)
		{
			close(remote_server_fd);
			remote_server_fd = -1;
			free(json_str);
			return -1;
		}
		memset(send_str,0,strlen(json_str)+32);
		generate_msg_head(send_str,strlen(json_str),REQUEST_TYPE,0);
		memcpy(send_str+16,json_str,strlen(json_str));
		//ComFunPrintfBuffer(send_str,strlen(json_str)+16);
		write(remote_server_fd,send_str,strlen(json_str)+16);
		ret = get_remote_532_conf_request();
		free(json_str);
		free(send_str);
		close(remote_server_fd);
		remote_server_fd = -1;
	}
	return ret;
}

int check_md5(char *local_md5)
{
	char cmd[BUF_LEN_256]={0};
	if(testendian()==0)
		CalcFileMD5_Big(DOWNLOAD_AS532_TEMP_CONF_FILE,local_md5);
	if(testendian()==1)
		CalcFileMD5_Little(DOWNLOAD_AS532_TEMP_CONF_FILE,local_md5);	
	PRINT("local_md5 = %s\n",local_md5);
	PRINT("remote_as532_conf_md5 = %s\n",remote_as532_conf_md5);
	if(!strncmp(remote_as532_conf_md5,local_md5,32))
	{
		PRINT("file md5 ok\n");
		sprintf(cmd,"%s%s%s%s","mv ",DOWNLOAD_AS532_TEMP_CONF_FILE," ",DOWNLOAD_AS532_CONF_FILE);
		PRINT("cmd = %s\n",cmd);
		system(cmd);
		return 0;
	}
	else
	{
		PRINT("file md5 err\n");
		sprintf(cmd,"%s%s","rm -rf ",DOWNLOAD_AS532_TEMP_CONF_FILE);
		//PRINT("cmd = %s\n",cmd);
		system(cmd);
		return -1;
	}
}

int init_as532()
{
	char local_md5_buf[BUF_LEN_64]={0};
	int ret = 0;
	if(testendian()==0)
	{
		as532_fd = open(B6L_UART_NAME,O_RDWR | O_NONBLOCK);
	}
	if(testendian()==1)
	{
		as532_fd = open(A20_UART_NAME,O_RDWR | O_NONBLOCK);
	}
	if(as532_fd < 0)
	{
		PRINT("open uart err\n");
		exit(-1);
	}
	PRINT("as532_fd = %d\n",as532_fd);
	serialConfig(as532_fd,B115200);
	PRINT("open uart success\n");
	
	if(get_532_ver()!=0)
	{
		if(get_532_ver()!=0)
		{
			if(set_default()!=0)
			{
				return -2;			
			}
		}
	}
	if(get_532_ver()!=0)
		return -3;
	if(get_532_ver_des()!=0)
		return -4;
	if(testendian()==1)
	{
		printf("This is A20\n");
		usleep(90*1000*1000);
	}
	//if(get_532_sn()!=0)
		//return -5;		
	if(get_remote_532_conf_ver()==0)
	{
		ret = check_conf_ver();
		if(ret == -1)
			goto START;
		if(ret == 0)
		{
			if(get_remote_532_conf()!=0)
				goto START;
			if(check_md5(local_md5_buf)!=0)
				goto START;
			load_config();
		}
	}
START:
	if(get_remote_532_ver()!=0)
		return -5;
	if(check_ver()!=0)
		return -6;
	PRINT("as532 needs update\n");
	if(get_remote_532_image()!=0)
		return -7;
	if(As532PacketCheck((const char *)DOWNLOAD_AS532_FILE,as532_version)==0)
	{
		update_tmp_file();
		jump_to_boot();
		usleep(500*1000);
		if(As532Update((const char *)DOWNLOAD_AS532_FILE,as532_version)==0)
		{
			clean_tmp_file();
			usleep(1000*1000);
			system("rm -rf /tmp/AS532.bin");
			PRINT("update success\n");
			usleep(2000*1000);
			if(get_532_ver_des()!=0)
				return -8;
			return 0;
		}
		clean_tmp_file();
	}
	system("rm -rf /tmp/AS532.bin");
	return -9;
}

int load_config()
{
	int ret = 0;
	char buf[BUF_LEN_512]={0};
	char tmp_buf[3]={0};
	int minor = 0;
	char *p;
	char *end;
	int fd = open("/etc/532.conf",O_RDWR);
	if(fd < 0)
	{
		printf("532.conf is not found\n");
		exit(-1);
	}
	ret = read(fd,buf,sizeof(buf));
	if(ret > 0)
	{
		p = strstr(buf,"CONF_VER=\"");
		if(p == NULL)
		{
			printf("config file err.\nfile's first line must be CONF_VER=\"Vx.x.x\"\n");
			close(fd);
			exit(-1);
		}

		p += strlen("CONF_VER=\"");
		end = strstr(p,"\"");
		if(end == NULL)
		{
			printf("config file err.\nfile's first line must be CONF_VER=\"Vx.x.x\"\n");
			close(fd);
			exit(-1);
		}
		memcpy(conf_ver,p,end-p);
		//HBD_F2B_AS532_V4.0.1
		//				012345
		p = strstr(conf_ver,"V");
		if(p == NULL)
		{
			p = strstr(conf_ver,"v");
			if(p == NULL)
			{
				printf("config file err.\nfile's first line must be CONF_VER=\"Vx.x.x\"\n");
				close(fd);
				exit(-1);
			}
		}
		as532_conf_version[0] = p[1]-48;
		as532_conf_version[1] = p[3]-48;
		//PRINT("strlen(p) = %d\n",strlen(p));
		if(strlen(p) == 6)
		{
			as532_conf_version[2] = 0x0;
			as532_conf_version[3] = p[5]-48;
			minor = as532_conf_version[3];
		}
		else if(strlen(p) == 7)
		{
			tmp_buf[0] = p[5];
			tmp_buf[1] = p[6];
			tmp_buf[2] = '\0';
			minor = atoi(tmp_buf);
			as532_conf_version[2] = minor >> 8;
			as532_conf_version[3] = minor&0xff;
		}
		PRINT("local as532 conf = %d.%d.%d\n",as532_conf_version[0],as532_conf_version[1],minor);
		
		p = strstr(buf,"REMOTE_SERVER_IP=\"");
		if(p == NULL)
		{
			printf("config file err.\nfile's second line must be REMOTE_SERVER_IP=\"xxx.xxx.xxx.xxx\"\n");
			close(fd);
			exit(-1);
		}
		p += strlen("REMOTE_SERVER_IP=\"");
		end = strstr(p,"\"");
		if(end == NULL)
		{
			printf("config file err.\nfile's second line must be REMOTE_SERVER_IP=\"xxx.xxx.xxx.xxx\"\n");
			close(fd);
			exit(-1);
		}
		memcpy(remote_ip_buf,p,end-p);
		
		p = strstr(buf,"REMOTE_SERVER_PORT=\"");
		if(p == NULL)
		{
			printf("config file err.\nfile's third line must be REMOTE_SERVER_PORT=\"xxx\"\n");
			close(fd);
			exit(-1);
		}
		p += strlen("REMOTE_SERVER_PORT=\"");
		end = strstr(p,"\"");
		if(end == NULL)
		{
			printf("config file err.\nfile's third line must be REMOTE_SERVER_PORT=\"xxx\"\n");
			close(fd);
			exit(-1);
		}
		memcpy(remote_port_buf,p,end-p);
		close(fd);
		printf("conf=%s\n",conf_ver);
		printf("port=%s\n",remote_port_buf);
		printf("ip=%s\n",remote_ip_buf);
		return 0;
	}
	close(fd);
	PRINT("config file err.\nfile's first line must be CONF_VER=\"xxx\"\n");
	PRINT("config file err.\nfile's second line must be REMOTE_SERVER_IP=\"xxx.xxx.xxx.xxx\"\n");
	PRINT("config file err.\nfile's third line must be REMOTE_SERVER_PORT=\"xxx\"\n");
	exit(-1);	
}

int is_b6l()
{
	FILE *fp;
	int len;
	fp = fopen("/tmp/.apcfg", "r");
	char buf[100]={0};
	char ver[64]={0};
	if(fp != NULL)
	{	
		while(fgets(buf, 100, fp) != NULL)
		{
			if(strstr(buf, "SOFT_VERSION") != 0)
			{
				sscanf(buf, "SOFT_VERSION=%s", ver);
				if(strstr(ver, "HBD_B6L") != 0)
				{
					fclose(fp);
					return 0;
				}
				else if(strstr(ver, "HBD_F2B") != 0)
				{
					printf("%s\nExit....\n",ver);
					system("/bin/dnat.sh");
					fclose(fp);
					exit(-1);
				}
				else
				{
					fclose(fp);
					exit(-1);
				}
			}
		}
		fclose(fp);
	}
	return -1;
}

int main(int argc,char **argv)
{
	if(testendian()==0)
	{
		printf("This is not A20\n");
		is_b6l();
	}
	clean_tmp_file();
	load_config();
	init_as532();
	
	close(as532_fd);
	return 0;
}
