#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>

#define CONTROL_PORT	9998
#define UDHCPD_FILE  "/var/run/udhcpd.leases"
#define A20_NAME 	"HBD_F2B_A20"

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

int main(int argc,char **argv)
{
	struct sockaddr_in client;
	socklen_t len = sizeof(client);
	char *new_ip = NULL;
	struct in_addr ia;

	create_server();
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
		usleep(50*1000);
		do_cmd_get_s1_ip();
		usleep(100*1000);
		close(clientfd);
	}
	return 0;
}



