
/**************************************************************************
        2007-1-5 11:42 establish by lzh.A cgi program.
        get a file from user's explorer.
***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cgiMain.h"
#include "filelist.h"

static FILE *errOut;

#define DEAL_BUF_LEN  1024
#define SIGN_CODE_LEN  100
#define FILE_NAME_LEN 64
#define FILE_SAVE_DIR "/tmp/"

enum
{
    STATE_START,
    STATE_GET_SIGN_CODE,
    STATE_GET_FILE_NAME,
    STATE_GET_FILE_START,
    STATE_GET_FILE_CONTENT,
    STATE_CHECK_END,
    STATE_END
};
/***************************************************************************
ShowErrorInfo
****************************************************************************/
static void ShowErrorInfo(char * error)
{

    printf("Content-Type:text/html;charset=UTF-8\n\n");
    printf("<center><font color='red'>%s</font></center>" , error );
}
static void restore_file(char *filename)
{
    FILE *p = NULL;
    char cmd[255]={0};
    char buff[255]={0};
    int addr = 64*1024;
    int len = 0;
    int i = 0;

    while(strlen(backup_file_list[i]) !=0)
    {
        sprintf(cmd,"dd if=%s bs=1 count=8 skip=%d 2> /dev/null",filename, addr);

        p = popen(cmd ,"r");
        fgets(buff, 9, p);

        addr +=8;
        len=atoi(buff);
        memset(cmd,0,255);
        memset(buff,0,255);
        sprintf(cmd,"dd if=%s of=%s bs=1 count=%d skip=%d >/dev/null 2>&1",filename, backup_file_list[i], len, addr);
        system(cmd);
        addr +=len;
        fclose(p);
        i++;
    }

}
static void  Reboot_tiaozhuan(char* res,char * gopage)
{
    char temp[256]={0};
    printf("HTTP/1.0 200 OK\r\n");
    printf("Content-type: text/html\r\n");
    printf("Connection: close\r\n");
    printf("\r\n");
    printf("\r\n");
    printf("<HTML><HEAD>\r\n");
    printf("</head><body>");

    sprintf(temp,"<script language=javascript>window.location.href=\"reboot?RESULT=%s?PAGE=%s\";</script>",res,gopage);
    printf(temp);
    printf("</body></html>");
}

/* 主体从这里开始 */

int main(void)
{
    FILE *fp; /* 文件指针，保存我们要获得的文件 */
    int getState = STATE_START;
    int contentLength;/*标准输入内容长度*/
    int nowReadLen;
    int signCodeLen;
    int tmpLen;
    char *nowReadP;
    char *nowWriteP;
    char dealBuf[DEAL_BUF_LEN];
    char signCode[SIGN_CODE_LEN]; /*存储本次的特征码*/
    char tmpSignCode[SIGN_CODE_LEN];
    char fileName[FILE_NAME_LEN];
    memset(dealBuf,0,DEAL_BUF_LEN);
    memset(signCode,0,SIGN_CODE_LEN);
    memset(fileName,0,FILE_NAME_LEN);
    nowReadLen = 0;
    char error=0;

    errOut = fopen("/dev/ttyS0","w");
    write_systemLog("cfgupload setting begin"); 
    
    if((char *)getenv("CONTENT_LENGTH")!=NULL)
    {
        contentLength = atoi((char *)getenv("CONTENT_LENGTH"));
    }
    else
    {
        //ShowErrorInfo("没有恢复数据!");
       // exit(1);
        error=1;
        goto error;

    }
 
    while(contentLength > 0)
    {
        if(contentLength >= DEAL_BUF_LEN)
        {
            nowReadLen = DEAL_BUF_LEN;
        }
        else
        {
            nowReadLen = contentLength;
        }
        contentLength -= nowReadLen;
        if(fread(dealBuf,sizeof(char),nowReadLen,stdin) != nowReadLen)
        {
            //ShowErrorInfo("读取恢复数据失败，请重试！");
            //exit(1);
            error=1;
            goto error;
        }
        nowReadP = dealBuf;
        while(nowReadLen > 0)
        {
            switch (getState)
            {
                case STATE_START:
                    nowWriteP = signCode;
                    getState = STATE_GET_SIGN_CODE;
                case STATE_GET_SIGN_CODE:
                    if(strncmp(nowReadP,"\r\n",2) == 0)
                    {
                        signCodeLen = nowWriteP - signCode;
                        nowReadP++;
                        nowReadLen--;
                        *nowWriteP = 0;
                        getState = STATE_GET_FILE_NAME;
                    }
                    else
                    {
                        *nowWriteP = *nowReadP;
                        nowWriteP++;
                    }
                    break;
                case STATE_GET_FILE_NAME:
                    if(strncmp(nowReadP,"filename=",strlen("filename=")) == 0)
                    {
                        nowReadP += strlen("filename=");
                        nowReadLen -= strlen("filename=");
                        nowWriteP = fileName + strlen(FILE_SAVE_DIR);
                        while(*nowReadP != '\r')
                        {
                            if(*nowReadP == '\\' || *nowReadP == '/')
                            {
                                nowWriteP = fileName + strlen(FILE_SAVE_DIR);
                            }
                            else if(*nowReadP != '\"')
                            {
                                *nowWriteP = *nowReadP;
                                nowWriteP++;
                            }
                            nowReadP++;
                            nowReadLen--;
                        }
                        *nowWriteP = 0;
                        nowReadP++;
                        nowReadLen--;
                        getState = STATE_GET_FILE_START;
                        memcpy(fileName,FILE_SAVE_DIR,strlen(FILE_SAVE_DIR));
                        sprintf(fileName,"%scal.bin",FILE_SAVE_DIR);
                        if((fp=fopen(fileName,"w"))==NULL)
                        {
                            //fprintf(stderr,"open file error\n");
                            //exit(1);
                            error=1;
                            goto error;
                        }
                    }
                    break;
                case STATE_GET_FILE_START:
                    if(strncmp(nowReadP,"\r\n\r\n",4) == 0)
                    {
                        nowReadP += 3;
                        nowReadLen -= 3;
                        getState = STATE_GET_FILE_CONTENT;
                    }
                    break;
                case STATE_GET_FILE_CONTENT:
                    if(*nowReadP != '\r')
                    {
                        fputc(*nowReadP,fp);
                    }
                    else
                    {
                        if(nowReadLen >= (signCodeLen + 2))
                        {
                            if(strncmp(nowReadP + 2,signCode,signCodeLen) == 0)
                            {
                                getState = STATE_END;
                                nowReadLen = 1;
                                //ShowErrorInfo("数据上传成功");
                             }
                            else
                            {
                                fputc(*nowReadP,fp);
                            }
                        }
                        else
                        {
                            getState = STATE_CHECK_END;
                            nowWriteP = tmpSignCode;
                            *nowWriteP = *nowReadP;
                            nowWriteP++;
                            tmpLen = 1;
                        }
                    }
                    break;
                case STATE_CHECK_END:
                    if(*nowReadP != '\r')
                    {
                        if(tmpLen < signCodeLen + 2)
                        {
                            *nowWriteP = *nowReadP;
                            nowWriteP++;
                            tmpLen++;
                            if(tmpLen == signCodeLen + 2)
                            {
                                *nowWriteP = 0;
                                if((tmpSignCode[1] == '\n')&&(strncmp(tmpSignCode + 2,signCode,signCodeLen) == 0))
                                {
                                    getState = STATE_END;
                                    nowReadLen = 1;
                                    //ShowErrorInfo("数据上传成功");
                                }
                                else
                                {
                                    fwrite(tmpSignCode,sizeof(char),tmpLen,fp);
                                    getState = STATE_GET_FILE_CONTENT;
                                }
                            }
                        }
                    }
                    else
                    {
                        *nowWriteP = 0;
                        fwrite(tmpSignCode,sizeof(char),tmpLen,fp);
                        nowWriteP = tmpSignCode;
                        *nowWriteP = *nowReadP;
                        nowWriteP++;
                        tmpLen = 1;
                    }
                    break;
                case STATE_END:
                    nowReadLen = 1;
                    break;
                    default:break;
            }
            nowReadLen--;
            nowReadP++;
        }
    }

    error:
        if (fp != NULL)
        {
            fclose(fp);
        }
        if(error==1)
        {
            printf("Content-Type:text/html\n\n");
            printf("<HTML><HEAD>\r\n");
            printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
            printf("<script type=\"text/javascript\" src=\"/lang/b28n.js\"></script>");
            printf("</head><body>");
            printf("<script type='text/javascript' language='javascript'>Butterlate.setTextDomain(\"man\");window.parent.DialogHide();alert(_(\"err cfg upload\"));window.location.href=\"ad_man_cfgsave?ad_man_cfgsave=yes\";</script>");
            printf("</body></html>");
        }
        else 
        {		
            fprintf(errOut,"%s  %d file upload complete !fileName:%s\n",__func__,__LINE__,fileName);
            char cmdd[256];
            int firmware_flag=0;
            sprintf(cmdd,"firmware_check2 %s > /tmp/firmware.log",fileName);
            system(cmdd);
            FILE *fileBuf2=NULL;
            if ((fileBuf2= fopen("/tmp/firmware.log", "r")) == NULL)
            {
                fprintf(errOut,"%s  %d File open error.Make sure you have the permission.\n",__func__,__LINE__);
                firmware_flag=1;
            }
            else
            {
                fgets(cmdd,sizeof(cmdd),fileBuf2);
                if(strstr(cmdd,"OK")!=NULL)
                {
                    firmware_flag=2;
                }
                else
                {
                    firmware_flag=1;
                }
                fclose(fileBuf2);
            }

            if(firmware_flag!=1)
            {
                Reboot_tiaozhuan("cfgback","index.html");
                //reconfig
                int i;

                restore_file(fileName);

                sprintf(cmdd,"dd if=%s of=/dev/nvram bs=1024 count=64  > /dev/null 2>&1",fileName);
                i = 5;
                while(i--)
                {
                    usleep(10);
                    system(cmdd);
                }
                system("reboot"); 
                //Todo reboot	
            }
            else //error firmware file
            {	
                printf("Content-Type:text/html\n\n");
                printf("<HTML><HEAD>\r\n");
                printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
                printf("<script type=\"text/javascript\" src=\"/lang/b28n.js\"></script>");
                printf("</head><body>");
                printf("<script type='text/javascript' language='javascript'>Butterlate.setTextDomain(\"man\");window.parent.DialogHide();alert(_(\"err cfg format\"));window.location.href=\"ad_man_cfgsave?ad_man_cfgsave=yes\";</script>");
                printf("</body></html>");
            }
        }
	
        write_systemLog("cfgupload setting end"); 
        return 0;
}

