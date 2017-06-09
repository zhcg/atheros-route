#include <stdio.h>  
#include <stdlib.h>  
#include <sys/types.h>  
#include <sys/stat.h>  
#include <fcntl.h>  
#include <string.h>  
#include <unistd.h>  
#include <errno.h> 

#define pipein "/tmp/USER_DETECTION_STATUS"
#define pipeout "/tmp/VERSION_STATUS"

#define READMODE (O_RDONLY)  
#define WRITEMODE (O_WRONLY)  

char* getcgidata(FILE* fp, char* requestmethod);
static FILE *errOut;

int timing(int fd ,int secs)
{
        fd_set rdfds;
        struct timeval tv;
        int ret;
        FD_ZERO(&rdfds);
        FD_SET(fd, &rdfds);
        tv.tv_sec = secs;
        tv.tv_usec = 0;
        ret = select(fd+1, &rdfds, NULL, NULL, &tv);
        FD_ZERO(&rdfds);
        return ret;

}
int pipi_set(int type)
{  
    char buf[1024];  
    int fd;  
    int readnum;  
    int nwrite;
    int seconds=0;
    if ((fd = open(pipein, WRITEMODE)) < 0) 
    {  
            fprintf(errOut,"open pipein fail!\n");  
            exit(1);  
    }  
    /*向管道文件中写入数据，在这里要用strlen，如果用sizeof，则只是4 个字节的指针长度*/  
    if(type == 1)
    {
        if (nwrite = write(fd,"1",2 ) < 0) 
        {  
            if (errno == EAGAIN) 
            {  
                  fprintf(errOut,"1The FIFO has not been read yet.Please try later\n");  
            }  
        } 
        seconds = 5;
    }
    else
    {
        if (nwrite = write(fd,"2",2 ) < 0) 
        {  
            if (errno == EAGAIN) 
            {  
                  fprintf(errOut,"2The FIFO has not been read yet.Please try later\n");  
            }  
        }
        seconds = 60;
    }
 
    close(fd);
	
//	fprintf(errOut,"--------pipi_set(%d)!\n",type);  

	sleep(1);
    /*打开有名管道，并设置非阻塞标志*/  
    if ((fd = open(pipeout, READMODE)) < 0) 
    {  
            fprintf(errOut,"open pipeout fail!\n");  
            exit(1);  
    }  

    {
        int timing_return = timing(fd ,seconds);
        if(timing_return == 0)
        {
            fprintf(errOut,"timeout!\n");  
        }
        else if(timing_return < 0)
        {
             fprintf(errOut,"select error !!!!\n");
        }
        else
        {
                bzero(buf, sizeof(buf));  
                readnum = read(fd, buf, sizeof(buf));
    /*如果读到数据则打印出来，如果没有数据，则忽略*/  
                if (readnum != 0) 
                {  
                    buf[readnum] = '\0';  
                    printf("%s", buf); 
                    fprintf(errOut,"output %s \n",buf);  
                }
        }
    }
    close(fd);  
    return 0;  
}  

int main()
   {
    char *input;
    char *req_method;
    char name[64];
    char pass[64];
    int i = 0;
    int j = 0;
    char rspBuff1[65536];
    char cmdd[256];
    
    req_method = getenv("REQUEST_METHOD");
    input = getcgidata(stdin, req_method);
    //errOut = fopen("/dev/ttyS0","w");
    errOut = fopen("/etc/lc65xx.conf","w");
    //fprintf(errOut,"%s <br> \n", input);

    // 我们获取的input字符串可能像如下的形式
    // Username="admin"&Password="aaaaa"
    // 其中"Username="和"&Password="都是固定的
    // 而"admin"和"aaaaa"都是变化的，也是我们要获取的
    // 前面9个字符是UserName=
    // 在"UserName="和"&"之间的是我们要取出来的用户名
    for ( i = 0, j = 0; i < (int)strlen(input); i++ )
    {
        if ( input[i] != '&' && input[i] != '\n')
        {
			name[j++] = input[i];
        } 
		else
		{
            name[j] = '\0';
			fprintf(errOut,"%s\n",name);
			j = 0;
		}
    }
	fprintf(errOut,"\n");

	char temp[256]={0};
			
	printf("HTTP/1.0 200 OK\r\n");
	printf("Content-type: text/html\r\n");
	printf("Connection: close\r\n");
	printf("\r\n");
	printf("\r\n");
	printf("<HTML><HEAD>\r\n");
	printf("</head><body>");
	sprintf(temp,"<script language=javascript>window.location.href=\"ad_man_localinfo\";</script>");
	printf(temp);
	printf("</body></html>");

    return 1;
}

char* getcgidata(FILE* fp, char* requestmethod)
{
    char* input;
    int len;
    int size = 1024;
    int i = 0;
    if (!strcmp(requestmethod, "GET"))
    {
        input = getenv("QUERY_STRING");
        return input;
    }
    else if (!strcmp(requestmethod, "POST"))
    {
        len = atoi(getenv("CONTENT_LENGTH"));
         input = (char*)malloc(sizeof(char)*(size + 1));
        if (len == 0)
        {
            input[0] = '\0';
            return input;
        }
        while(1)
        {
            input[i] = (char)fgetc(fp);
            if (i == size)
            {
                input[i+1] = '\0';
                return input;
            }
            --len;
            if (feof(fp) || (!(len)))
            {
                i++;
                input[i] = '\0';
                return input;
            }
            i++;
         }
    }
    return NULL;
}



