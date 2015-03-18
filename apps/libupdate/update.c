#include "json.h"
#include "common.h"
#include "our_md5.h"
#include "update.h"
#include "errno.h"

int as532_fd = -1;
int remote_server_fd = -1;
const char hex_to_asc_table[16] = {0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
	0x38,0x39,0x41,0x42,0x43,0x44,0x45,0x46};
unsigned char respond_buffer[BUFFER_SIZE_1K] = {0};
unsigned char local_version[5]={0};
unsigned char as532_sn[BUF_LEN_128]={0};
unsigned char as532_version_description[BUF_LEN_128]={0};
unsigned int remote_version[5]={0};
unsigned int as532_conf_version[5]={0};
unsigned int old_remote_version[5]={0};
unsigned char old_remote_conf_version[5]={0};
unsigned char remote_path[BUF_LEN_256]={0};
unsigned char remote_md5[BUF_LEN_64]={0};
char remote_ip_buf[BUF_LEN_128]={0};
char remote_port_buf[BUF_LEN_64]={0};
char ota_ip_buf[BUF_LEN_128]={0};
char ota_port_buf[BUF_LEN_64]={0};
char conf_ver[BUF_LEN_64]={0};
char tmp_app_name[BUF_LEN_64] = {0};
char global_app_name[BUF_LEN_64]={0};
unsigned char base_sn[34];
F2B_INFO_DATA fid;

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

int create_socket_client(char *ip,char *port)
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
	PRINT("remote_server_fd = %d\n",remote_server_fd);
	setsockopt(remote_server_fd, SOL_SOCKET, SO_SNDTIMEO, &timeo, len);
	cliaddr.sin_family = AF_INET;
	inet_pton(AF_INET,ip,&cliaddr.sin_addr);//服务器ip
	cliaddr.sin_port = htons(atoi(port));//注意字节顺序
	if(connect(remote_server_fd, (struct sockaddr*)&cliaddr, sizeof(cliaddr))<0)
	{
		if (errno == EINPROGRESS) 
		{  
			PRINT("connect ip:%s port:%d timeout.\n",ip,atoi(port));
			close(remote_server_fd);
			remote_server_fd = -1;
			return -2;  
		}         
		PRINT("connect ip:%s port:%d err.\n",ip,atoi(port));
		close(remote_server_fd);
		remote_server_fd = -1;
		return -3;
	}
	PRINT("connected ip:%s port:%d.\n",ip,atoi(port));
	return 0;
}

int get_sn()
{
    system("fw_printenv SN > /tmp/.sn_tmp");
    char buf[128]={0};
    int fd = open("/tmp/.sn_tmp",O_RDWR);
    if(fd < 0)
    {   
        printf("open err\n");
        return -1; 
    }   
    if(read(fd,buf,sizeof(buf))>0)
    {   
        if(strstr(buf,"SN=")==NULL)
        {
            close(fd);
            return -1; 
        }
        printf("buf = %s\n",buf);
        memset(base_sn,0,sizeof(base_sn));
        sscanf(buf,"SN=%s",base_sn);
    }   
    close(fd);
    return 0;
}

char *generate_get_remote_msg(char *remote_type,char *base_id)
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
	if(base_id == NULL)
		value = json_new_string("0000000000000000000000000000001010");
	else
		value = json_new_string(base_id);
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

int prase_msg(char *msg,char *local_version_head,char *app_version,int *busy_status)
{
	char *msgp;
	char *end;
	char ver_buf[32]={0};
	char tmp_buf[8]={0};
	int minor = 0;
	int minor_nov = 0;
	int nov = 0;
	int i;
	for(i=0;i<strlen(msg);i++)
		printf("%c",msg[i]);
	printf("\n");

	msgp = strstr(msg,local_version_head);

	if(msgp == NULL)
	{
		PRINT("remote msg err\n");
		return -1;
	}
	msgp = strstr(msg,VER_NUM);
	if(msgp == NULL)
	{
		msgp = strstr(msg,VER_NUM_NOV);
		if(msgp == NULL)
		{
			PRINT("no ver\n");
			return -2;
		}
		msgp = msgp + strlen(VER_NUM_NOV);
		nov = 1;
	}
	else
	{
		msgp = msgp + strlen(VER_NUM);
	}
	end = strstr(msgp,"\"");
	if(end == NULL)
	{
		PRINT("ver err\n");
		return -3;
	}
	memcpy(ver_buf,msgp,end-msgp);
	
	for(i=0;i<sizeof(ver_buf);i++)
	{
		if(ver_buf[i] == '.')
			ver_buf[i] = ' ';
	}
	if(nov == 0)
	{
		sscanf(ver_buf,"%x %x %d",&remote_version[0],&remote_version[1],&minor);
	}
	else if(nov == 1)
	{
		sscanf(ver_buf,"%x %d",&remote_version[1],&minor);
		msgp = strstr(app_version,".");
		if(msgp == NULL)
		{
			PRINT("app_version err\n");
			return -2;
		}
		memcpy(tmp_buf,app_version,(msgp-app_version)>7?7:(msgp-app_version));
		minor_nov = atoi(tmp_buf);
		remote_version[0] = ((minor_nov/10)<<4)+(minor_nov%10);
	}
	PRINT("minor = %d\n",minor);
	//5678
	remote_version[2] = ((minor/1000)<<4)+(minor%1000)/100;
	remote_version[3] = (((minor%100)/10)<<4)+(minor%10);
	PRINT("remote ver = 0x%X.0x%X.0x%X.0x%X\n",remote_version[0],remote_version[1],remote_version[2],remote_version[3]);

	msgp = strstr(msg,VER_DESC);
	if(msgp == NULL)
	{
		PRINT("no ver desc\n");
		return -2;
	}
	msgp = msgp + strlen(VER_DESC);
	end = strstr(msgp,"\"");
	if(end == NULL)
	{
		PRINT("ver desc err \n");
		return -3;
	}
	
	//if(!strcmp(local_version_head,LOCAL_VERSION_HEAD))
	//{
		if(memcmp(old_remote_version,remote_version,sizeof(remote_version)) == 0)
		{
			PRINT("App has been downloaded\n");
			if(busy_status != NULL && (*busy_status == 0))
				new_bin();		
			return -8;
		}
	//}

	msgp = strstr(msg,VER_PATH);
	if(msgp == NULL)
	{
		PRINT("no path\n");
		return -4;
	}
	msgp = msgp + strlen(VER_PATH);
	//PRINT("msgp = %s\n",msgp);
	end = strstr(msgp,"\"");
	if(end == NULL)
	{
		PRINT("path err\n");
		return -5;
	}
	memset(remote_path,0,sizeof(remote_path));
	memcpy(remote_path,msgp,end-msgp);
	PRINT("remote_path = %s\n",remote_path);

	msgp = strstr(msg,CONF_MD5);
	if(msgp == NULL)
	{
		PRINT("no md5\n");
		return -6;
	}
	msgp = msgp + strlen(CONF_MD5);
	//PRINT("msgp = %s\n",msgp);
	end = strstr(msgp,"\"");
	if(end == NULL)
	{
		PRINT("md5 err\n");
		return -7;
	}
	memset(remote_md5,0,sizeof(remote_md5));
	memcpy(remote_md5,msgp,end-msgp);
	PRINT("remote_md5 = %s\n",remote_md5);
	return 0;	
}

int get_remote_request(char *local_version_head,char *app_version,int *busy_status)
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
			PRINT("get remote ver request timeout\n");
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
				return prase_msg(recvbuf+16,local_version_head,app_version,busy_status);
		}
		else
		{
			if(errno == EAGAIN)
			{
				continue;
			}
			break;
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

int get_remote_msg(char *local_version_all,char *local_version_head,char *app_version,int *busy_status)
{
	char *json_str;
	char *send_str;
	int ret = -1;
	if(create_socket_client(ota_ip_buf,ota_port_buf)==0)
	{
		if(get_sn()!=0)
		{
			json_str = generate_get_remote_msg(local_version_all,NULL);
		}
		else
		{
			json_str = generate_get_remote_msg(local_version_all,base_sn);
		}
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
		ret = get_remote_request(local_version_head,app_version,busy_status);
		free(json_str);
		free(send_str);
		close(remote_server_fd);
		remote_server_fd = -1;
	}
	return ret;
}

int check_ver(char *app_version)
{
	char ver_tmp[32]={0};
	char tmp_buf[3]={0};
	int minor,i=0;
	memset(local_version,0,sizeof(local_version));
	memcpy(ver_tmp,app_version,strlen(app_version));//1.2.03
	for(i=0;i<sizeof(ver_tmp);i++)
	{
		if(ver_tmp[i] == '.')
			ver_tmp[i] = ' ';
	}
	sscanf(ver_tmp,"%x %x %d",&local_version[0],&local_version[1],&minor);
	PRINT("minor = %d\n",minor);
	//5678
	local_version[2] = (minor/1000)<<4+(minor%1000)/100;
	local_version[3] = ((minor%100)/10)<<4+(minor%10);
	PRINT("local_version ver = 0x%X.0x%X.0x%X.0x%X\n",local_version[0],local_version[1],local_version[2],local_version[3]);

	//int ret = memcmp((void *)remote_version, (void *)local_version, 2);
	//if(ret != 0)
	//{
		//PRINT("ver err\n");
		//return -1;
	//}
	int ret = memcmp((void *)remote_version, (void *)local_version, sizeof(remote_version));
	if(ret <= 0)
	{
		PRINT("newest\n");
		return -2;
	}
	return 0;
}

int get_remote_image(char *name)
{
	pid_t status;
	int ret = 0;
	// /bin/wget url
	char cmd[BUF_LEN_256]={0};
	memset(tmp_app_name,0,sizeof(tmp_app_name));
	sprintf(tmp_app_name,"%s%s%s","/tmp/",name,"_tmp");
	sprintf(cmd,"%s%s%s%s%s",WGET," --read-timeout=30 -O ",tmp_app_name," ",remote_path);
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

int check_md5(char *local_md5)
{
	char cmd[BUF_LEN_256]={0};
	if(testendian()==0)
		CalcFileMD5_Big(tmp_app_name,local_md5);
	if(testendian()==1)
		CalcFileMD5_Little(tmp_app_name,local_md5);	
	PRINT("local_md5 = %s\n",local_md5);
	PRINT("remote_md5 = %s\n",remote_md5);
	if(!strncmp(remote_md5,local_md5,32))
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

int get_ota_info()
{
	int recv_ret = -1;
	int loops = 0;
	int i;
	char *request_msg = "0007\r\n\0";
	char recvbuf[BUFFER_SIZE_2K] = {0};
	if(create_socket_client(remote_ip_buf,remote_port_buf)==0)
	{
		write(remote_server_fd,request_msg,strlen(request_msg));
		do
		{
			loops++;
			usleep(50*1000);
			if(loops >= 100)
			{
				close(remote_server_fd);
				PRINT("get remote ver request timeout\n");
				return -1;
			}
			recv_ret = recv(remote_server_fd,recvbuf,BUFFER_SIZE_2K,MSG_DONTWAIT);
			if(recv_ret > 0 )
			{
				PRINT("recv_ret: %d\n",recv_ret);	
				PRINT("recv: %s \n",recvbuf);		
				if(recv_ret < 6)
				{
					close(remote_server_fd);
					PRINT("msg length err\n");
					return -2;
				}
				for(i = 0;i<recv_ret;i++)
				{
					if(recvbuf[i] == ':')
						recvbuf[i] = ' ';
				}
				sscanf(recvbuf,"%s %s",ota_ip_buf,ota_port_buf);
				close(remote_server_fd);
				return 0;
			}
			else
			{
				if(errno == EAGAIN)
				{
					continue;
				}
				break;
			}
		}while(1);
		close(remote_server_fd);
		remote_server_fd = -1;
	}
	return -1;
}

int stop_here()
{
	char cmd[128] = {0};
	printf("killed %s\n",global_app_name);
	//if(restart_flag == 1)
	//{
		//sprintf(cmd,"touch /etc/app_status/%s",app_name);
		//system(cmd);
	//}
	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"killall -9 %s",global_app_name);
	system(cmd);
	return 0;
}

int new_bin()
{
	char cmd[BUF_LEN_128]={0};
	//memset(cmd,0,sizeof(cmd));	
	sprintf(cmd,"%s%s","rm -rf /bin/",global_app_name);
	PRINT("cmd=%s\n",cmd);
	system(cmd);		
	//memset(cmd,0,sizeof(cmd));	
	sprintf(cmd,"%s%s%s%s","mv -f ",tmp_app_name," /bin/",global_app_name);
	PRINT("cmd=%s\n",cmd);
	system(cmd);
	//memset(cmd,0,sizeof(cmd));	
	sprintf(cmd,"%s%s%s","chmod +x ","/bin/",global_app_name);
	PRINT("cmd=%s\n",cmd);
	system(cmd);		
	stop_here();
	return 0;
}

int init_start(char *app_version_all,char *app_version_head,char *app_version,int *busy_status)
{
	char local_md5_buf[BUF_LEN_64]={0};
	char cmd[BUF_LEN_128]={0};
	int ret = 0;

	if(get_ota_info()!=0)
		return -5;
	if(get_remote_msg(app_version_all,app_version_head,app_version,busy_status)!=0)
		return -1;
	//if(check_ver(app_version)!=0)
		//return -2;
	PRINT("needs update\n");
	if(get_remote_image(global_app_name)!=0)
		return -3;
	if(check_md5(local_md5_buf)==0)
	{
		memcpy(old_remote_version,remote_version,sizeof(remote_version));	
		if(busy_status != NULL && (*busy_status == 0))
			new_bin();		
		return 0;
	}
	else
	{
		sprintf(cmd,"%s%s","rm -rf ",tmp_app_name);
		system(cmd);
	}
	return -4;
}

int load_net_config()
{
	int i,ret = 0;
	char buf[512]={0};
	char tmp_buf[3]={0};
	char tempbuf[64]={0};
	int minor = 0;
	char *p;
	char *pp;
	char *end;
	memset(conf_ver,0,sizeof(conf_ver));
	memset(as532_conf_version,0,sizeof(as532_conf_version));
	memset(remote_ip_buf,0,sizeof(remote_ip_buf));
	memset(remote_port_buf,0,sizeof(remote_port_buf));
	int fd = open("/etc/532.conf",O_RDWR);
	if(fd < 0)
	{
		printf("532.conf is not found\n");
		return -1;
	}
	ret = read(fd,buf,sizeof(buf));
	if(ret > 0)
	{
		p = strstr(buf,"CONF_VER=\"");
		if(p == NULL)
		{
			printf("config file err.\nfile's first line must be CONF_VER=\"Vx.x.x\"\n");
			close(fd);
			return -2;
		}

		p += strlen("CONF_VER=\"");
		end = strstr(p,"\"");
		if(end == NULL)
		{
			printf("config file err.\nfile's first line must be CONF_VER=\"Vx.x.x\"\n");
			close(fd);
			return -2;
		}
		memcpy(conf_ver,p,end-p);
		conf_ver[end-p] = '\0';
		//HBD_F2B_AS532_V4.0.1
		//				012345
		p = strstr(conf_ver,"_V");
		if(p == NULL)
		{
			p = strstr(conf_ver,"_v");
			if(p == NULL)
			{
				printf("config file err.\nfile's first line must be CONF_VER=\"Vx.x.x\"\n");
				close(fd);
				return -2;
			}
		}
		p += 2;
		strcpy(tempbuf, p);
		
		for(i=0; i<strlen(tempbuf); i++)
		{
			if(tempbuf[i] == '.')
				tempbuf[i] = ' ';
		}

		sscanf(tempbuf,"%x %x %d",&as532_conf_version[0],&as532_conf_version[1],&minor);
		PRINT("tempbuf = %s\n",tempbuf);
		PRINT("minor = %d\n",minor);
		//5678
		as532_conf_version[2] = ((minor/1000)<<4)+(minor%1000)/100;
		as532_conf_version[3] = (((minor%100)/10)<<4)+(minor%10);
		PRINT("conf ver = 0x%X.0x%X.0x%X.0x%X\n",as532_conf_version[0],as532_conf_version[1],as532_conf_version[2],as532_conf_version[3]);
		
		p = strstr(buf,"REMOTE_SERVER_IP=\"");
		if(p == NULL)
		{
			printf("config file err.\nfile's second line must be REMOTE_SERVER_IP=\"xxx.xxx.xxx.xxx\"\n");
			close(fd);
			return -1;
		}
		p += strlen("REMOTE_SERVER_IP=\"");
		end = strstr(p,"\"");
		if(end == NULL)
		{
			printf("config file err.\nfile's second line must be REMOTE_SERVER_IP=\"xxx.xxx.xxx.xxx\"\n");
			close(fd);
			return -1;
		}
		memcpy(remote_ip_buf,p,end-p);
		
		p = strstr(buf,"REMOTE_SERVER_PORT=\"");
		if(p == NULL)
		{
			printf("config file err.\nfile's third line must be REMOTE_SERVER_PORT=\"xxx\"\n");
			close(fd);
			return -1;
		}
		p += strlen("REMOTE_SERVER_PORT=\"");
		end = strstr(p,"\"");
		if(end == NULL)
		{
			printf("config file err.\nfile's third line must be REMOTE_SERVER_PORT=\"xxx\"\n");
			close(fd);
			return -1;
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
	return -1;
}

int update(char *app_name,char *app_version_all,char *app_version_head,char *app_version,int loop_time,int *busy_status)
{
	int times = loop_time;
	
	memset(global_app_name,0,sizeof(global_app_name));
	memcpy(global_app_name,app_name,strlen(app_name));
	if(app_version_all != NULL)
	{
		PRINT("VER:%s\n",app_version_all);
	}
	while(1)
	{
		if(times >= loop_time)
		{
			times = 0;
			if(load_net_config() != 0)
				continue;
			init_start(app_version_all,app_version_head,app_version,busy_status);
		}
		times++;
		usleep(1000*1000);
	}
	return 0;
}
