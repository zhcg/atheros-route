#include <stdio.h>
#include<stdlib.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include<string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static int val = -1;

int main()
{
	int sock, addr_len, len, xss_len, ret;
	struct sockaddr_in s_addr, c_addr;
	unsigned int buff[30];
	char buff_char[10];
	char cmd[40] = {0};
	int bReuseaddr = 1; 
	char xss_recv_ok[3]="ok";

	if((sock = socket(AF_INET,SOCK_DGRAM,0)) == -1)   //udp 创建socket对象
	{
		perror("socket");   ////创建失败
		close(sock);
		return -1;
		//exit(EXIT_FAILURE);
	}
	else
	{
		//MSG(" create channel control  socket \n");
		printf(" create channel control  socket \n");
	}
	bzero(&s_addr, sizeof(s_addr));
	s_addr.sin_family = AF_INET;
	s_addr.sin_port = htons(59999);
	s_addr.sin_addr.s_addr = INADDR_ANY;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,(const char*)&bReuseaddr, sizeof(int));

	if((bind(sock, (struct sockaddr *)&s_addr, sizeof(s_addr))) == -1)
	{
		perror("bind");
		close(sock);
		return -1;
		//exit(EXIT_FAILURE);
	}
	else
	{
		printf("bind address of socket \n");
	}
	addr_len = sizeof(c_addr);

	while (1)
	{
		bzero(&buff, 30 * sizeof(unsigned int));
		len = recvfrom(sock, buff, 30 * sizeof(unsigned int), 0, (struct sockaddr *)&c_addr, &addr_len);
		
	
		if( len < 0)
		{
			perror("recvfrom error:");
			//exit(1);
			close(sock);
			return -1;
		}
		
		int i;
		for( i = 0;i<30;i++)
		{
			printf("buff[%d] = 0x%x\n",i,buff[i]);
		}
		if(buff[0] == 0x55556666)
		{
			 
			//printf("buff[0] = 0x%x\n", buff[0]);
			if(buff[1] == 0x34120000)
			{
				printf("buff[1] = 0x%x\n", buff[1]);
				printf("buff[4] = 0x%x\n", buff[4]);
					
				unsigned int *val_ip = buff + 4;
				unsigned char *val_cp = val_ip;
				val = *(val_cp);
					
				printf("val_ip = %x\n",*val_ip);
				printf("val = %d\n",val);
				len = 0;
				if(0 < val < 14)
				{	
					xss_len = sendto(sock, xss_recv_ok, strlen(xss_recv_ok), 0, (struct sockaddr *)&c_addr, addr_len);
					printf("send to length:%d\n", xss_len);
					if(xss_len < 0)
					{
						perror("sendto error:");
						close(sock);
						return -1;
						//exit(1);
					}
				}
				continue;
					
			}
			else if(buff[1] == 0x35120000)
			{
				sprintf(cmd, "iwconfig ath0 freq %d", val);
				if (system(cmd) < 0)
				{
					printf("try it angin\n");
					sleep(1);
					system(cmd);
				}
				val = 0;
				memset(cmd, 0, sizeof(cmd));
			}
					
		}
	}				
	close(sock);
	return 0;
}
