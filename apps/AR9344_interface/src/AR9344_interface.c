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
int detection_type = -1;   // 1 只用于检测版本    2  检测版本并升级
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


//test CPU is littleEnd or Bigend; 
//ret: 1 :Little Endian￡?0: BIG Endian
int testendian() 
{
	int x = 1;
	return *((char*)&x);
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

	
	for(i=0;i<strlen(conf_ver);i++)
		printf("%c", conf_ver[i]);
	printf("\n");


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
			PRINT("recv: %s\n",recvbuf);
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


int get_remote_532_image()
{
	pid_t status;
	int ret = 0;
	// /bin/wget url
	char cmd[BUF_LEN_256]={0};
	sprintf(cmd,"%s%s%s%s%s",WGET," --read-timeout=30 -O ",DOWNLOAD_AS532_FILE," ",remote_as532_path);
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
		PRINT("json_str = %s\n",json_str);
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
			perror("write");
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
		CalcFileMD5_Big(DOWNLOAD_AS532_FILE,local_md5);
	if(testendian()==1)
		CalcFileMD5_Little(DOWNLOAD_AS532_FILE,local_md5);	
	PRINT("local_md5 = %s\n",local_md5);
	PRINT("remote_as532_conf_md5 = %s\n",remote_as532_conf_md5);
	if(!strncmp(remote_as532_conf_md5,local_md5,32))
	{
		PRINT("file md5 ok\n");
		return 0;
	}
	else
	{
		PRINT("file md5 err\n");
		return -1;
	}
}

int init_as532()
{
	char local_md5_buf[BUF_LEN_64]={0};
	int ret = 0;
	char cmd[BUF_LEN_256]={0};
	if(get_remote_532_conf_ver()==0)
	{
		if(detection_type == 1)
		{
			printf("NEW SYSTEM!!!\n");
			return 0;
		}
		else if(detection_type == 2)
		{
		//	PRINT("as532 needs update\n");
			if(get_remote_532_image()!=0)
				return -7;
			if(check_md5(local_md5_buf) == 0)
			{
				printf("UPDATE SYSTEM!!!\n");
	
			}
		}
	}
	else
	{
		printf("NO NEW SYSTEM!!!\n");
		return -1;
	}

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

	FILE *fp_apcfg;
	fp_apcfg = fopen("/tmp/.apcfg", "r");
	if(fp_apcfg < 0)
	{
		printf("/tmp/.apcfg is not found\n");
		exit(-1);
	}
	else	
	{
		while(fgets(buf, 100, fp_apcfg) != NULL)
                {
                        if((p = strstr(buf, "SOFT_VERSION")) != 0)
                        {
                               
                        	
		//		p += strlen("SOFT_VERSION=");
			//	end = p + strlen(buf);
		//		end = strstr(p,"A");
				//p += strlen("SOFT_VERSION=\"");
			//	end = strstr(p,"\"");
		/*		if(end == NULL)
				{
					printf("config file err.\nfile's first line must be CONF_VER=\"Vx.x.x\"\n");
			        	fclose(fp_apcfg);
					exit(-1);
				}
		*/		printf("p = %s\n", p);
				sscanf(buf, "SOFT_VERSION=%s", conf_ver);
				p = strstr(conf_ver,"V");
				if(p == NULL)
				{
					p = strstr(conf_ver,"v");
					if(p == NULL)
					{
						printf("config file err.\nfile's first line must be CONF_VER=\"Vx.x.x\"\n");
			        		fclose(fp_apcfg);
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
		
			 }
                }
     		fclose(fp_apcfg);
	}
	

	


	int fd = open("/etc/AR9344.conf",O_RDWR);
	if(fd < 0)
	{
		printf("AR9344.conf is not found\n");
		exit(-1);
	}
	ret = read(fd,buf,sizeof(buf));
	if(ret > 0)
	{
/*
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
*/	
		int num = 0;
		for(num; num < ret; num++)
		{
			printf("%c", buf[num]);

		}
		printf("\n");
		


		printf("ret = %d\n", ret);
	
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
				if(strstr(ver, "HBD_F2A_B6_V7") != 0)
				{
					fclose(fp);
					return 0;
				}
				else if(strstr(ver, "HBD_F2A_B6_V8") != 0)
				{
					printf("%s\nExit....\n",ver);
					system("/bin/dnat.sh");
					fclose(fp);
					exit(-1);
				}
				else if(strstr(ver, "HBD_F2A_B6_V9") != 0)
				{
					fclose(fp);
					return 0;
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
	
	if(*argv[1] == 49)
	{
		detection_type = 1;
	}
	else if(*argv[1] == 50)
	{
		detection_type = 2;
	}
	else
	{
		detection_type = -1;
	}
	
	if(testendian()==0)
	{
		printf("This is not A20\n");
//		is_b6l();
	}
	load_config();
	init_as532();
	
//	close(as532_fd);
	return 0;
}
