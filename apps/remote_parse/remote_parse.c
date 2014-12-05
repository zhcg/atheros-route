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
#include <locale.h>
#include <stdio.h>
#include <sys/stat.h>  //stat函数
#include <sys/wait.h>
#include "json.h"
#include "our_md5.h"

#define AUTHOR	"ZhangBo"
#define PRINT_INFO 1

#if PRINT_INFO==1
#define PRINT(format, ...) printf("[%s][%s][-%d-] "format"",__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#else
#define PRINT(format, ...)
#endif

#define MAX(A,B) ((A)>(B))?(A):(B)

#define BUF_LEN_64					64
#define BUF_LEN_128 				128
#define BUF_LEN_256					256
#define BUF_LEN_512  				512
#define BUFFER_SIZE_1K     			1024
#define BUFFER_SIZE_2K 				2048

#define HBD_REBOOT_CMD				"HBD_REBOOT_CMD"
#define HBD_REBOOT_CMD_ALL			"HBD_REBOOT_CMD_V1.0.0"
#define PROTOCOL_VER				"0101"
#define REQUEST_TYPE				0x01
#define RESPOND_TYPE				0x02
#define STATUS_TYPE					0x03
#define VER_NUM						"\"versionNum\":\"V"
#define VER_NUM_NOV						"\"versionNum\":\""
#define VER_DESC					"\"versionDesc\":\""
#define VER_PATH					"\"versionPath\":\""
#define CONF_MD5					"\"checkCode\":\""
#define WGET						"/bin/wget"
#define DOWNLOAD_UPDATE_TEMP_CONF_FILE		"/etc/532_tmp.conf"
#define DOWNLOAD_UPDATE_CONF_FILE			"/etc/532.conf"

char remote_ip_buf[128]={0};
char remote_port_buf[64]={0};
char ota_ip_buf[128]={0};
char ota_port_buf[64]={0};
char conf_ver[64]={0};
unsigned char as532_conf_version[5]={0};
unsigned char base_sn[34] = {0};
int remote_server_fd = -1;
char remote_cmd[128]={0};

int load_net_config()
{
	int i,ret = 0;
	char buf[512]={0};
	char tmp_buf[3]={0};
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
			return -1;
		}

		p += strlen("CONF_VER=\"");
		end = strstr(p,"\"");
		if(end == NULL)
		{
			printf("config file err.\nfile's first line must be CONF_VER=\"Vx.x.x\"\n");
			close(fd);
			return -1;
		}
		memcpy(conf_ver,p,end-p);
		//HBD_F2B_AS532_V4.0.1
		//				012345
		p = strstr(p,"V");
		if(p == NULL)
		{
			p = strstr(p,"v");
			if(p == NULL)
			{
				printf("config file err.\nfile's first line must be CONF_VER=\"Vx.x.x\"\n");
				close(fd);
				return -1;
			}
		}
		//V12.34.5678
		pp = p;
		i = 0;
		while(1)
		{
			if(*pp == '.')
			{
				i++;
				*pp = ' ';
			}
			if(i == 2)
				break;
			pp++;
		}
		PRINT("p = %s\n",p);
		char temp[5] = {0};
		unsigned long num = 0;
		sscanf(p+1,"%s %x %d",temp,&as532_conf_version[1],&minor);
		num = strtoul(temp,NULL,16);
		as532_conf_version[0] = (char)num;
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

int create_socket_client(int *fd,char *ip,char *port)
{
	struct sockaddr_in cliaddr;
    struct timeval timeo = {0};   
    timeo.tv_sec = 5;
	socklen_t len = sizeof(timeo);  
	*fd = socket(AF_INET, SOCK_STREAM, 0);
	if(*fd < 0)
	{
		PRINT("create client err.\n");
		return -1;
	}
	PRINT("fd = %d\n",*fd);
	setsockopt(*fd, SOL_SOCKET, SO_SNDTIMEO, &timeo, len);
	cliaddr.sin_family = AF_INET;
	inet_pton(AF_INET,ip,&cliaddr.sin_addr);//服务器ip
	cliaddr.sin_port = htons(atoi(port));//注意字节顺序
	if(connect(*fd, (struct sockaddr*)&cliaddr, sizeof(cliaddr))<0)
	{
		if (errno == EINPROGRESS) 
		{  
			PRINT("connect ip:%s port:%d timeout.\n",ip,atoi(port));
			close(*fd);
			*fd = -1;
			return -2;  
		}         
		PRINT("connect ip:%s port:%d err.\n",ip,atoi(port));
		close(*fd);
		*fd = -1;
		return -3;
	}
	PRINT("connected ip:%s port:%d.\n",ip,atoi(port));
	return 0;
}

int get_ota_info()
{
	int recv_ret = -1;
	int loops = 0;
	int i;
	char *request_msg = "0007\r\n\0";
	char recvbuf[BUFFER_SIZE_2K] = {0};
	if(create_socket_client(&remote_server_fd,remote_ip_buf,remote_port_buf)==0)
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

int check_cmd(char *cmd)
{
	char buf[64] = {0};
	int fd = open("/var/.cmd",O_RDWR);
	if(fd < 0)
		return -1;
	read(fd,buf,sizeof(buf));
	if(strcmp(cmd,buf) == 0)
	{
		PRINT("pass cmd\n");
		close(fd);
		return 0;
	}
	else
	{
		close(fd);
		return -1;
	}
}

int check_time(char *cmd)
{
	char year[5] = {0};
	char month[3] = {0};
	char day[3] = {0};
	char hour[3] = {0};
	char min[3] = {0};
	time_t now = time(NULL);
	time_t cmd_time = 0;
	struct tm tm_cmd;
	memset(&tm_cmd,0,sizeof(tm_cmd));
	//201412021200
	char *p = cmd+strlen(HBD_REBOOT_CMD)+1;
	memcpy(year,p,4);
	p += 4;
	memcpy(month,p,2);
	p += 2;
	memcpy(day,p,2);
	p += 2;
	memcpy(hour,p,2);
	p += 2;
	memcpy(min,p,2);
	tm_cmd.tm_min = atoi(min);
	tm_cmd.tm_hour = atoi(hour)-1;
	tm_cmd.tm_mday = atoi(day);
	tm_cmd.tm_mon = atoi(month)-1;
	tm_cmd.tm_year = atoi(year)-1900;
	cmd_time = mktime(&tm_cmd);
	PRINT("now = %d\n",now);
	PRINT("cmd_time = %d\n",cmd_time);
	struct tm *pp;
	pp=gmtime(&cmd_time); /*变量t的值转换为实际日期时间的表示格式*/
	printf("%d年%02d月%02d日",(1900+pp->tm_year), (1+pp->tm_mon),pp->tm_mday);
	printf("%02d:%02d:%02d\n", pp->tm_hour, pp->tm_min, pp->tm_sec);	
	if(now - cmd_time >= 86400 || now < cmd_time)
	{
		PRINT("time err\n");
		return -1;
	}
	else
		return 0;
}

int save_cmd(char *cmd)
{
	int fd = open("/var/.cmd",O_RDWR | O_CREAT | O_TRUNC);
	if(fd < 0)
		return -1;
	if(write(fd,cmd,strlen(cmd)) == strlen(cmd))
	{
		close(fd);
		return 0;
	}
	else
	{
		close(fd);
		return -1;
	}
}

int prase_cmd(char *cmd,char *local_version_head)
{
	if(cmd == NULL || local_version_head == NULL)
		return -1;
	PRINT("cmd = %s\n",cmd);
	PRINT("code = %s\n",local_version_head);
	if(strstr(cmd,local_version_head) == NULL)
		return -2;
	if(check_cmd(cmd) == 0)
		return 0;
	if(check_time(cmd) != 0)
		return -3;
	if(save_cmd(cmd) != 0)
		return -4;
	PRINT("start reboot\n");
	system("reboot");
	return 0;
}

int testendian() 
{
	int x = 1;
	return *((char*)&x);
}

int get_remote_file(char *local_path,unsigned char *remote_path)
{
	pid_t status;
	int ret = 0;
	// /bin/wget url
	char cmd[BUF_LEN_256]={0};
	sprintf(cmd,"%s%s%s%s%s",WGET," -c --read-timeout=30 -O ",local_path," ",remote_path);
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

int check_md5(char *local_md5,char *local_temp_file,char *local_file,char *remote_md5)
{
	char cmd[BUF_LEN_256]={0};
	if(testendian()==0)
		CalcFileMD5_Big(local_temp_file,local_md5);
	if(testendian()==1)
		CalcFileMD5_Little(local_temp_file,local_md5);	
	PRINT("local_md5 = %s\n",local_md5);
	PRINT("remote_md5 = %s\n",remote_md5);
	if(!strncmp(remote_md5,local_md5,32))
	{
		PRINT("file md5 ok\n");
		sprintf(cmd,"%s%s%s%s","mv ",local_temp_file," ",local_file);
		PRINT("cmd = %s\n",cmd);
		system(cmd);
		return 0;
	}
	else
	{
		PRINT("file md5 err\n");
		sprintf(cmd,"%s%s","rm -rf ",local_temp_file);
		//PRINT("cmd = %s\n",cmd);
		system(cmd);
		return -1;
	}
}

int prase_msg(char *msg,char *local_version_head)
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
	memset(remote_cmd,0,sizeof(remote_cmd));
	memcpy(remote_cmd,msgp,end-msgp);
	prase_cmd(remote_cmd,local_version_head);
	
	return 0;	
}

int get_remote_request(char *local_version_head)
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
				return prase_msg(recvbuf+16,local_version_head);
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

int get_remote_msg(char *local_version_all,char *local_version_code)
{
	char *json_str;
	char *send_str;
	int ret = -1;
	if(create_socket_client(&remote_server_fd,ota_ip_buf,ota_port_buf)==0)
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
		ret = get_remote_request(local_version_code);
		free(json_str);
		free(send_str);
		close(remote_server_fd);
		remote_server_fd = -1;
	}
	return ret;
}
int prase_ver_msg(char *msg,unsigned char *desc,unsigned int *remote_version,unsigned char *remote_path,unsigned char *remote_md5)
{
	char *msgp;
	char *end;
	char ver_buf[32]={0};
	char name[32]={0};
	char tmp_buf[3]={0};
	int minor = 0;
	int i;
	int nov = 0;
	
	for(i=0;i<strlen(msg);i++)
		printf("%c",msg[i]);
	printf("\n");
	msgp = strstr(desc,"_V");
	if(msgp == NULL)
	{
		msgp = strstr(desc,"_v");
		if(msgp == NULL)
		{
			printf("desc err\n");
			return -6;
		}
	}
	memcpy(name,desc,(unsigned char *)msgp-desc);
	PRINT("%s\n",name);
	
	msgp = strstr(msg,name);

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
		sscanf(ver_buf,"%x %x %d",&remote_version[0],&remote_version[1],&minor);
	else if(nov == 1)
	{
		sscanf(ver_buf,"%x %d",&remote_version[1],&minor);
		remote_version[0] = as532_conf_version[0];
	}
	PRINT("minor = %d\n",minor);
	//5678
	remote_version[2] = ((minor/1000)<<4)+(minor%1000)/100;
	remote_version[3] = (((minor%100)/10)<<4)+(minor%10);
	PRINT("remote ver = 0x%X.0x%X.0x%X.0x%X\n",remote_version[0],remote_version[1],remote_version[2],remote_version[3]);
	
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
	memcpy(remote_path,msgp,end-msgp);
	PRINT("remote_path = %s\n",remote_path);
		
	msgp = strstr(msg,VER_DESC);
	if(msgp == NULL)
	{
		PRINT("no ver desc\n");
		return -10;
	}
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
	memcpy(remote_md5,msgp,end-msgp);
	PRINT("remote_md5 = %s\n",remote_md5);
	
	return 0;	
}

int get_remote_ver_request(char *version,int *fd,unsigned int *remote_version,unsigned char *remote_path,unsigned char *remote_md5)
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
		recv_ret = recv(*fd,recvbuf,BUFFER_SIZE_2K,MSG_DONTWAIT);
		if(recv_ret > 0 )
		{
			PRINT("recv_ret: %d\n",recv_ret);			
			//ComFunPrintfBuffer(recvbuf,recv_ret);
			//PRINT("recv: %s\n",recvbuf);
			memcpy(&length,recvbuf,4);
			PRINT("length = %d\n",length);
			if(recv_ret > 16)
				return prase_ver_msg(recvbuf+16,version,remote_version,remote_path,remote_md5);
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

int get_remote_ver(char *version,int *fd,unsigned int *remote_version,unsigned char *remote_path,unsigned char *remote_md5)
{
	char *json_str;
	char *send_str;
	int ret = -1;
	if(create_socket_client(fd,ota_ip_buf,ota_port_buf)==0)
	{
		if(get_sn()!=0)
		{
			json_str = generate_get_remote_msg(version,NULL);
		}
		else
		{
			json_str = generate_get_remote_msg(version,base_sn);
		}
		send_str = malloc(strlen(json_str)+32);
		PRINT("json_str = %d\n",strlen(json_str));
		if(send_str == NULL)
		{
			close(*fd);
			*fd = -1;
			free(json_str);
			return -1;
		}
		memset(send_str,0,strlen(json_str)+32);
		generate_msg_head(send_str,strlen(json_str),REQUEST_TYPE,0);
		memcpy(send_str+16,json_str,strlen(json_str));
		//ComFunPrintfBuffer(send_str,strlen(json_str)+16);
		write(*fd,send_str,strlen(json_str)+16);
		ret = get_remote_ver_request(version,fd,remote_version,remote_path,remote_md5);
		free(json_str);
		free(send_str);
		close(*fd);
		*fd = -1;
	}
	return ret;
}

int get_commamd(char *version_all,char *version_code)
{
	char local_md5_buf[BUF_LEN_64]={0};
	char cmd[BUF_LEN_128]={0};
	int ret = 0;
	unsigned int remote_version[5] = {0};
	unsigned char remote_path[BUF_LEN_256] = {0};
	unsigned char remote_md5[BUF_LEN_64] = {0};
	int remote_fd = 0;
	
	if(get_ota_info()!=0)
		return -5;
	if(get_remote_ver(conf_ver,&remote_fd,remote_version,remote_path,remote_md5)==0)
	{
		if(get_remote_file(DOWNLOAD_UPDATE_TEMP_CONF_FILE,remote_path)!=0)
			goto START;
		if(check_md5(local_md5_buf,DOWNLOAD_UPDATE_TEMP_CONF_FILE,DOWNLOAD_UPDATE_CONF_FILE,remote_md5)!=0)
			goto START;
		ret = load_net_config();
		if(ret < 0)
		{
			usleep(10*1000);
			load_net_config();
		}
		if(get_ota_info()!=0)
			return -5;
	}
START:
	if(get_remote_msg(version_all,version_code)!=0)
		return -1;
	return 0;
}

int start_loop(char *version_all,char *version_code,int loop_time)
{
	int times = loop_time;
	
	if(version_all != NULL)
	{
		PRINT("VER:%s\n",version_all);
	}
	while(1)
	{
		if(times >= loop_time)
		{
			times = 0;
			if(load_net_config() != 0)
				continue;
			get_commamd(version_all,version_code);
		}
		times++;
		usleep(1000*1000);
	}
	return 0;
}

int main(int argc,char **argv)
{
	time_t now = time(NULL);
	struct tm *p;
	p=gmtime(&now); /*变量t的值转换为实际日期时间的表示格式*/
	printf("%d年%02d月%02d日",(1900+p->tm_year), (1+p->tm_mon),p->tm_mday);
	printf("%02d:%02d:%02d\n", p->tm_hour, p->tm_min, p->tm_sec);	
	start_loop(HBD_REBOOT_CMD_ALL,HBD_REBOOT_CMD,30*60);
	return 0;
}
