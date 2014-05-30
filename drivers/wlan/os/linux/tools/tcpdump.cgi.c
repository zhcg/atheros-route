#include <stdio.h>
#include <stdlib.h>
#include <string.h>


char* getcgidata(FILE* fp, char* requestmethod);
static FILE *errOut;


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
            strcat(buffer,"<br>");
            buffer += strlen(buffer);
        }

        pclose(f);
    }

    return(retBuff);
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

    req_method = getenv("REQUEST_METHOD");
    input = getcgidata(stdin, req_method);
    errOut = fopen("/dev/ttyS0","w");

    // 我们获取的input字符串可能像如下的形式
    // Username="admin"&Password="aaaaa"
    // 其中"Username="和"&Password="都是固定的
    // 而"admin"和"aaaaa"都是变化的，也是我们要获取的
    // 前面9个字符是UserName=
    // 在"UserName="和"&"之间的是我们要取出来的用户名
    for ( i = 2; i < (int)strlen(input); i++ )
    {
        if ( input[i] == '&' )
        {
            name[j] = '\0';
            break;
        } 
        name[j++] = input[i];
    }

    // 前面9个字符 + "&Password="10个字符 + Username的字符数
    // 是我们不要的，故省略掉，不拷贝
    for ( i = 7 + strlen(name), j = 0; i < (int)strlen(input); i++ )
    {
        pass[j++] = input[i];
    }
    pass[j] = '\0';

    fprintf(errOut,"Your Username is %s<br>Your Password is %s<br> \n", name, pass);
    //printf("Your Username is %s<br>Your Password is %s<br> \n", name, pass);
    if(strcmp(name,"begin")==0)
    {
        //Execute_cmd("net_check", rspBuff1);
        system("tcpdumps > /tmp/tcpdump.log &");sleep(2);//bash  avoid let syntax error
       // printf("%s",rspBuff1);
       // fprintf(errOut,"WAN:%s!!!!!  %d \n",rspBuff1,strlen(rspBuff1));
    }
    else if(strcmp(name,"end")==0)
    {
       // Execute_cmd("ping -c 3 www.baidu.com", rspBuff1);
       Execute_cmd("killall tcpdumps", rspBuff1);
       // printf("%s",rspBuff1);
       // fprintf(errOut,"BD:%s!!!!!  %d \n",rspBuff1,strlen(rspBuff1));
    } 

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


