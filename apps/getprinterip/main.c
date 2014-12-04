#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#define CONTROL_PORT	9998
#define UDHCPD_FILE  "/var/run/udhcpd.leases"
#ifdef S1
#define A20_NAME 	"HBD_F2B_A20"
#elif defined(S1_F3A)
#define A20_NAME 	"HBD_F3A_A20"
#endif

int sockfd = 0;
int clientfd = 0;

int create_server()
{
	int on,i;
	struct sockaddr_in servaddr,cliaddr;

	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd<=0)
	{
		perror("create control socket fail\n");
		exit(-1);
	}
	printf("control socket success\n");

	on = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(CONTROL_PORT);

	if(bind(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr)) == -1)
	{
		perror("control bind error");
		exit(-1);
	}
	printf("control bind success\n");

	if(listen(sockfd,5)<0)
	{
		perror("control listen error\n");
		exit(-1);
	}
	printf("control listen success\n");

	return 0;
}

//socket发送函数
int netWrite(int fd,const void *buffer,int length)
{
	int bytes_left;
	int written_bytes;
	const char *ptr;
	ptr = (const char *)buffer;
	bytes_left = length;
	while(bytes_left > 0) {
		//begin to write
		written_bytes = write(fd,ptr,bytes_left);
		if(written_bytes <= 0) { //error
			if(errno == EINTR) //interrupt error, we continue writing
				written_bytes = 0;
			else             //other error, exit
				return (-1);
		}
		bytes_left -= written_bytes;
		ptr += written_bytes;     //from where left,  continue writing
	}
	return (0);
}

int do_cmd_get_9344_sn()
{
	system("fw_printenv SN > /tmp/.sn_tmp");
	char buf[128]={0};
	char sn[32]={0};
	char sendbuf[128]={0};
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
		sscanf(buf,"SN=%s",sn);
		printf("sn = %s\n",sn);
		sprintf(sendbuf,"GET_SN%03d%s",strlen(sn),sn);
		netWrite(clientfd,sendbuf,strlen(sendbuf));
	}
	else
		netWrite(clientfd,"GET_SN000",strlen("GET_SN000"));
	close(fd);
	return 0;
}

int get_mac(char *out_mac,char *dev)
{
	int i;
	char cmd[32]={0};
	char buf[128]={0};
	sprintf(cmd,"get_mac %s  > /tmp/.mac_tmp",dev);
	system(cmd);
	int fd = open("/tmp/.mac_tmp",O_RDWR);
	if(fd < 0)
	{
		printf("open err\n");
		return -1;
	}
	if(read(fd,buf,sizeof(buf))>0)
	{
		memcpy(out_mac,buf,17);
		for(i=0;i<17;i++)
		{
			if(out_mac[i] == 0)
				out_mac[i] = ' ';
		}
	}
	else
	{
		for(i=0;i<17;i++)
		{
			if(out_mac[i] == 0)
				out_mac[i] = ' ';
		}
	}
	close(fd);
	return 0;
}

int do_cmd_get_9344_mac()
{
	char sendbuf[256]={0};
	char sendbuf2[256]={0};
	char mac[20]={0};
	get_mac(mac,"eth0");
	//printf("eth0 = %s\n",mac);
	//sprintf(sendbuf,"%s%s","eth0",mac);
	//memset(mac,0,sizeof(mac));
	//get_mac(mac,"br0");
	//printf("br0 = %s\n",mac);
	//strcat(sendbuf,"br0");
	//strcat(sendbuf,mac);
	//memset(mac,0,sizeof(mac));
	//get_mac(mac,"wifi0");
	//printf("wifi0 = %s\n",mac);
	//strcat(sendbuf,"wifi0");
	//strcat(sendbuf,mac);
	//memset(mac,0,sizeof(mac));
	//get_mac(mac,"wifi1");
	//printf("wifi1 = %s\n",mac);
	//strcat(sendbuf,"wifi1");
	//strcat(sendbuf,mac);
	sprintf(sendbuf2,"%s%03d%s","GETMAC",strlen(mac),mac);
	netWrite(clientfd,sendbuf2,strlen(sendbuf2));
	
	return 0;
}

int do_cmd_get_9344_ver()
{
	FILE *fp;
	fp = fopen("/tmp/.apcfg", "r");
	char buffer[100];
	char buf[64] = {0};
	char sendbuf[64]={0};
	if(fp != NULL)
	{	
		while(fgets(buffer, 100, fp) != NULL)
		{
			if(strstr(buffer, "SOFT_VERSION") != 0)
			{
				sscanf(buffer, "SOFT_VERSION=%s", buf);
				printf("buf = %s\n",buf);
				sprintf(sendbuf,"GETVER%03d%s",strlen(buf),buf);
				netWrite(clientfd,sendbuf,strlen(sendbuf));
				fclose(fp);
				return 0;
			}
		}
		fclose(fp);
	}
	netWrite(clientfd,"GETVER000",strlen("GETVER000"));
	return -1;
}

int do_cmd_get_s1_ip()
{
	struct in_addr addr;
	int i,j;
	char mac_buf[20]={0};
	char sendbuf[64]={0};
	struct dhcpOfferedAddr 
	{
		unsigned char hostname[16];
		unsigned char mac[16];
		unsigned long ip;
		unsigned long expires;
	} lease;
	system("killall -q -USR1 udhcpd");
	FILE *fp = fopen(UDHCPD_FILE, "r");  /*  /var/run/udhcpd.leases   */
	if (NULL == fp)
	{
		printf("open udhcpd error\n \n");
		netWrite(clientfd,"PRINTERIP000",strlen("PRINTERIP000"));
		return -1;
	}

	while(1) 
	{
		memset(&lease,0, sizeof(lease));
		if(fread(&lease, sizeof(lease), 1, fp) != 1)
			break;
		addr.s_addr = lease.ip;

		printf("hostname = %s\n",lease.hostname);
		printf("strlen(hostname) = %d\n",strlen(lease.hostname));
		printf("ip = %s\n",inet_ntoa(addr));
		if(!strcmp(lease.hostname,A20_NAME))
		{
			sprintf(sendbuf,"PRINTERIP%03d%s",strlen(inet_ntoa(addr)),inet_ntoa(addr));
			printf("send_buf = %s\n",sendbuf);
			netWrite(clientfd,sendbuf,strlen(sendbuf));
			fclose(fp);
			return 0;
		}	
	}
	printf("not found printer\n");
	netWrite(clientfd,"PRINTERIP000",strlen("PRINTERIP000"));
	fclose(fp);
	return -1;
}

int do_cmd_reboot()
{
	printf("reboot\n");
	netWrite(clientfd,"REBOOT000",strlen("REBOOT000"));
	sleep(2);
	system("reboot");
	return 0;
}

int prase_msg(char *msg)
{
	if(!strncmp(msg,"GET_IP",6))
	{
		printf("GET_IP\n");
		do_cmd_get_s1_ip();
	}
	else if(!strncmp(msg,"GET_SN",6))
	{
		printf("GET_SN\n");
		do_cmd_get_9344_sn();
	}
	else if(!strncmp(msg,"GETMAC",6))
	{
		printf("GETMAC\n");
		do_cmd_get_9344_mac();
	}	
	else if(!strncmp(msg,"GETVER",6))
	{
		printf("GETVER\n");
		do_cmd_get_9344_ver();
	}	
	else if(!strncmp(msg,"REBOOT",6))
	{
		printf("REBOOT\n");
		do_cmd_reboot();
	}	
	else
		printf("undefine\n");
	return 0;
}

int main(int argc,char **argv)
{
	struct sockaddr_in client;
	socklen_t len = sizeof(client);
	char *new_ip = NULL;
	struct in_addr ia;
	char recvbuf[512]={0};
	create_server();
	fd_set socket_fdset;
	struct timeval tv;
	int i,ret = 0;
	memset(&tv, 0, sizeof(struct timeval));
	for(;;)
	{
		clientfd = accept(sockfd, (struct sockaddr*)&client, &len);
		if (clientfd < 0)
		{
			printf("accept failed!\n");
			continue;
		}
		printf("control accept success\n");
		printf("clientfd=%d\n",clientfd);
		ia = client.sin_addr;
		new_ip = inet_ntoa(ia);
		printf("client_ip=%s\n",new_ip);
		memset(recvbuf,0,sizeof(recvbuf));
		FD_ZERO(&socket_fdset);
		FD_SET(clientfd, &socket_fdset);
		tv.tv_sec =  5;
		tv.tv_usec = 0;
		switch(select(clientfd+ 1, &socket_fdset, NULL, NULL, &tv))
		{
			case -1:
			case 0:
				printf("time out\n");
				break;
			default:
				ret = recv(clientfd,recvbuf,sizeof(recvbuf),0);
				if(recv > 0)
				{
					printf("ret = %d\n",ret);
					for(i=0;i<ret/6;i++)
						prase_msg(recvbuf+i*6);
				}
				break;
		}
		close(clientfd);
	}
	return 0;
}



