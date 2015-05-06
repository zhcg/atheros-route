
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>



char *Execute_cmd(char *cmd,char *buffer)
{
    FILE            *f;
    char            *retBuff = buffer;
    char            cmdLine[1024];

    /*
    ** Provide he command string to popen
    */

    f = popen(cmd, "r");

    if(f)
    {
        /*
        ** Read each line.
        */

        while(1)
        {
            *buffer = 0;
            fgets(buffer,120,f);
            if(strlen(buffer) == 0)
            {
                break;
            }
            //strcat(buffer,"<br>");
            buffer += strlen(buffer);
        }

        pclose(f);
    }

    return(retBuff);
}

char * my_strstr(char * ps,char *pd)  
{  
    char *pt = pd;  
    int c = 0;  
  
    while(*ps != '\0')  
    {  
        if(*ps == *pd)  
        {  
              
            while(*ps == *pd && *pd!='\0')  
            {  
                ps++;  
                pd++;  
                c++;  
            }  
        }  
  
        else  
        {  
            ps++;  
  
        }  
  
        if(*pd == '\0')  
        {  
            //sum++;  
            return (ps - c);  
        }  
            c = 0;  
            pd = pt;  
    }  
  
    return 0;  
}

int str_replace(char *p_result,char* p_source,char* p_seach,char *p_repstr)  
{  
  
    int c = 0;  
    int repstr_leng = 0;  
    int searchstr_leng = 0;  
  
    char *p1;  
    char *presult = p_result;  
    char *psource = p_source;  
    char *prep = p_repstr;  
    char *pseach = p_seach;  
    int nLen = 0;   
  
    repstr_leng = strlen(prep);  
    searchstr_leng = strlen(pseach);  
  
    do{   
        p1 = my_strstr(psource,p_seach);  
  
        if (p1 == 0)  
        {  
            strcpy(presult,psource);  
            return c;  
        }  
  
        c++;  //匹配子串计数加1;  
        //printf("p_result:%s\r\n",p_result);  
        //printf("p_source:%s\r\n",p_source);  
  
        // 拷贝上一个替换点和下一个替换点中间的字符串  
        nLen = p1 - psource;  
        memcpy(presult, psource, nLen);  
  
        // 拷贝需要替换的字符串  
        memcpy(presult + nLen,p_repstr,repstr_leng);  
  
        psource = p1 + searchstr_leng;  
        presult = presult + nLen + repstr_leng;  
  
    }while(p1);  
  
    return c;  
  
}  
int check_route(char *str_IP, char *str_mask, char *str_gw)
{
    unsigned long IP;
    unsigned long mask;
    unsigned long gw;

		//	printf("client eth0 ip  : %s\n", str_IP);
		//	printf("client mask  : %s\n", str_mask);
		//	printf("client ip  : %s\n", str_gw);
    IP = inet_addr(str_IP);
    mask = inet_addr(str_mask);
    gw = inet_addr(str_gw);

    if ((IP & mask) == (gw & mask)) 
    {    
        return 0;
    }    
    else 
    {    
        return 1;

    }    

}

void set_dnat(char *client_ip, char *iface)
{
        char cmd_buffer_r[512] = {0};
        char cmd_buffer_w[512] = {0};
        FILE * fd = fopen("/tmp/MultiScreen","a+");
        int i=0;
        char *ret=NULL;
	char port[10][10] =
        {
	"60001",
	"60003",
	"9527",
	"9529",
	"9992",
	"9991",
	"9990"
	};

	//printf("set_dnat start ip:%s\n",client_ip);
        memset(cmd_buffer_w,0,512);
        sprintf(cmd_buffer_w ,"#%s#%s\n",iface, client_ip);
        ret = fgets(cmd_buffer_r,500,fd);

	//printf("rrrr:%s\n", cmd_buffer_r);
//	printf("wwww:%s\n", cmd_buffer_w);
	fclose(fd);

        if(strcmp(cmd_buffer_w, cmd_buffer_r))
        {
	printf("set_dnat no route\n");
        	FILE *fd_w = fopen("/tmp/MultiScreen","w+");
              fprintf(fd_w,"%s",cmd_buffer_w);
		for(i=0;i<7;i++)
		{
			memset(cmd_buffer_w,0,512);
			sprintf(cmd_buffer_w ,"iptables -t nat -A PREROUTING_MULTISCR -i %s -p tcp -m tcp --dport %s -j DNAT --to-destination %s:%s\n", iface, port[i], client_ip, port[i]);
		        fprintf(fd_w,"%s",cmd_buffer_w);
		//	printf("%s\n", cmd_buffer_w);
		}
		fclose(fd_w);
		system("iptables -t nat -F PREROUTING_MULTISCR");
		system("sh /tmp/MultiScreen");
	}
	


}


int main()
{
    //setvbuf(stdout, NULL, _IONBF, 0); 
    //fflush(stdout); 
	// 绑定地址
	struct sockaddr_in addrto;
	bzero(&addrto, sizeof(struct sockaddr_in));
	addrto.sin_family = AF_INET;
	addrto.sin_addr.s_addr = htonl(INADDR_ANY);
	addrto.sin_port = htons(9535);
	
    //转发地址 wan
	struct sockaddr_in addrfw;
	bzero(&addrfw, sizeof(struct sockaddr_in));
	addrfw.sin_family=AF_INET;
	addrfw.sin_addr.s_addr=htonl(INADDR_BROADCAST);
	addrfw.sin_port=htons(9535);
	int nlen=sizeof(addrfw);
    //转发地址 lan
	struct sockaddr_in addrfw_lan;
	bzero(&addrfw_lan, sizeof(struct sockaddr_in));
	addrfw_lan.sin_family=AF_INET;
	char br0_bcast[20] = {0};
	unsigned char*pbr0_bcast;
	Execute_cmd("ifconfig br0|grep 'inet addr:'|awk -F ' ' '{print$3}'|awk -F ':' '{print$2}'", br0_bcast);
	pbr0_bcast=strtok(br0_bcast,"\n");
       // printf("pbr0_bcast: %s \n",pbr0_bcast);
	addrfw_lan.sin_addr.s_addr=inet_addr(pbr0_bcast);
	addrfw_lan.sin_port=htons(9535);
	int nlen_lan=sizeof(addrfw_lan);



	// 广播地址
	struct sockaddr_in from;

/*
	bzero(&from, sizeof(struct sockaddr_in));
	from.sin_family=AF_INET;
	from.sin_addr.s_addr=inet_addr("192.168.1.255");
	from.sin_port=htons(9535);
*/
	
	int sock = -1;
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) 
	{   
        //cout<<"socket error"<<endl;	
        printf("socket error\n");
		return -1;
	}   

	const int opt = 1;
	//设置该套接字为广播类型，
	int nb = 0;
	nb = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(opt));
	if(nb == -1)
	{
        //cout<<"set socket error..."<<endl;
        printf("set socket error\n");
		return -1;
	}

	if(bind(sock,(struct sockaddr *)&(addrto), sizeof(struct sockaddr_in)) == -1) 
	{   
        //cout<<"bind error..."<<endl;
        printf("bind error...\n");
		return -1;
	}



	int len = sizeof(struct sockaddr_in);
	char smsg[100] = {0};
	char smsg_new[100] = {0};
	char eth0_ip[100] = {0};
	char eth0_mask[100] = {0};
	char iface[10] = {0};
	char retBuf[1000] = {0};
	unsigned char*peth0_ip;
	unsigned char*peth0_mask;


	while(1)
	{
        printf("before receive\n");
	char br0_ip[20] = {0};
	unsigned char*pbr0_ip;
	Execute_cmd("ifconfig br0|grep 'inet addr:'|awk -F ' ' '{print$2}'|awk -F ':' '{print$2}'", br0_ip);
	pbr0_ip=strtok(br0_ip,"\n");
        //printf("br0 ip is: %s\n",pbr0_ip);
		//从广播地址接受消息
		int ret=recvfrom(sock, smsg, 100, 0, (struct sockaddr*)&from,(socklen_t*)&len);
	//	printf("ret is %d....\n",ret);
		if(ret<=0)
		{
            //cout<<"read error...."<<sock<<endl;
            printf("read error....\n");
		}
		else
		{	
			char *client_ip = inet_ntoa(from.sin_addr);
		//	printf("client ip  : %s\n", client_ip);
		//	printf("ret is %d....\n",ret);
		//	printf("%s\t \n", smsg);	
			Execute_cmd("ifconfig |grep eth0", retBuf);
			if(strstr(retBuf,"eth0")){
		//		printf("eth0 eth0 eth0 start\n");
				Execute_cmd("ifconfig eth0|grep 'inet addr:'|awk -F ' ' '{print$2}'|awk -F ':' '{print$2}'", eth0_ip);
				Execute_cmd("ifconfig eth0|grep 'inet addr:'|awk -F ' ' '{print$4}'|awk -F ':' '{print$2}'", eth0_mask);
				strcpy(iface, "eth0");
			}
			else
			{
				Execute_cmd("ifconfig ath4|grep 'inet addr:'|awk -F ' ' '{print$2}'|awk -F ':' '{print$2}'", eth0_ip);
				Execute_cmd("ifconfig ath4|grep 'inet addr:'|awk -F ' ' '{print$4}'|awk -F ':' '{print$2}'", eth0_mask);
				strcpy(iface, "ath4");
			}
			if(eth0_ip&&eth0_mask)
			{
				peth0_ip=strtok(eth0_ip,"\n");
				peth0_mask=strtok(eth0_mask,"\n");
				if(peth0_ip&&peth0_mask)
				{
			        	if(!(strcmp(client_ip,pbr0_ip)&&strcmp(client_ip,peth0_ip)))
					{
						printf("get own message!!!!!!!!!!!!!!!!!!!!!\n");
						continue;
					}
					if(check_route(peth0_ip,peth0_mask,client_ip))
					{
						printf("forward to wanKKKKKKKKKKKKKKKKKKKKK clientip: %s\n",client_ip);
		

						str_replace(smsg_new,smsg,client_ip,peth0_ip);
					//else
				//		str_replace(smsg_new,smsg,"192.168.1.101","192.168.8.111");
						int ret_fw=sendto(sock, smsg_new, strlen(smsg_new), 0, (struct sockaddr*)&addrfw, nlen);
						set_dnat(client_ip, iface);
						//printf("ret_fw is %d....\n",ret_fw);
					}
					else
					{
						//printf("smsg : %s\n",smsg);
						printf("forward to lan################### client_ip: %s\n",client_ip);
						int ret_fw=sendto(sock, smsg, strlen(smsg), 0, (struct sockaddr*)&addrfw_lan, nlen_lan);
					}
				}
			}
		}

		sleep(1);
	}

	printf("done\n");
	return 0;
}

