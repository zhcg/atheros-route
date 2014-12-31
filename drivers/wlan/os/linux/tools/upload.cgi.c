
/**************************************************************************
        2007-1-5 11:42 establish by lzh.A cgi program.
        get a file from user's explorer.
***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cgiMain.h"

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

/* ?????¡°???¨¨??¨¦????€?¡ì? */

int main(void)
{
    FILE *fp; /* ?¨C???????¨¦¡¯????????-???¡®???¨¨|?¨¨?¡¤??¡ª????¨C???? */
    int getState = STATE_START;
    int contentLength;/*??????¨¨?¡°?¡­£¤??¡­??1¨¦???o|*/
    int nowReadLen;
    int signCodeLen;
    int tmpLen;
    char *nowReadP;
    char *nowWriteP;
    char dealBuf[DEAL_BUF_LEN];
    char signCode[SIGN_CODE_LEN]; /*?-???¡§??????????¡ë1??????*/
    char tmpSignCode[SIGN_CODE_LEN];
    char fileName[FILE_NAME_LEN];
    memset(dealBuf,0,DEAL_BUF_LEN);
    memset(signCode,0,SIGN_CODE_LEN);
    memset(fileName,0,FILE_NAME_LEN);
    nowReadLen = 0;
    char error=0;

    errOut = fopen("/dev/ttyS0","w");
    write_systemLog("upload setting begin"); 

    
    if((char *)getenv("CONTENT_LENGTH")!=NULL)
    {
        contentLength = atoi((char *)getenv("CONTENT_LENGTH"));
    }
    else
    {
        //ShowErrorInfo("?2???¡ë??¡é?¡è???¡ã???!");
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
            //ShowErrorInfo("¨¨¡¥???¨C??¡é?¡è???¡ã????¡è¡À¨¨¡ä£¤???¨¨¡¥¡¤¨¦??¨¨¡¥????");
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
                        sprintf(fileName,"%sota.bin",FILE_SAVE_DIR);
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
                                //ShowErrorInfo("??¡ã???????????????");
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
                                    //ShowErrorInfo("??¡ã???????????????");
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
            printf("<script type='text/javascript' language='javascript'>Butterlate.setTextDomain(\"admin\");window.parent.DialogHide();window.parent.clearttzhuan();alert(_(\"err file upload\"));window.location.href=\"ad_man_upgrade?ad_man_upgrade=yes\";</script>");
            printf("</body></html>");
        }
	//¡ä¨°¨®?D??¡é¦Ì?¨ª?¨°3¦Ì?¨°t2?¦Ì?iframe?D
	else 
	{		
            fprintf(errOut,"%s  %d file upload complete fileName:%s!\n",__func__,__LINE__,fileName);
            char cmdd[256];
            int firmware_flag=0;
            sprintf(cmdd,"firmware_check %s > /tmp/firmware.log",fileName);
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
                    /*deal /etc/backup to mtdblock4(umount /etc)  zzw*/
                    system("upgrade_backup_reboot > /dev/null 2>&1");
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
                printf("Content-Type:text/html\n\n");
                printf("<HTML><HEAD>\r\n");
                printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
                printf("<script type=\"text/javascript\"  src=\"/style/upload.js\"></script>");
                printf("</head><body>");
                printf("<script type='text/javascript' language='javascript'>CheckUser(\"%s\");</script>",fileName);
                printf("</body></html>");
            }
            else
            {	
                printf("Content-Type:text/html\n\n");
                printf("<HTML><HEAD>\r\n");
                printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
                printf("<script type=\"text/javascript\" src=\"/lang/b28n.js\"></script>");
                printf("</head><body>");
                printf("<script type='text/javascript' language='javascript'>Butterlate.setTextDomain(\"admin\");window.parent.DialogHide();window.parent.clearttzhuan();alert(_(\"err file format\"));window.location.href=\"ad_man_upgrade?ad_man_upgrade=yes\";</script>");
                printf("</body></html>");
            }
	}
	
	write_systemLog("upload setting  end"); 
        return 0;
}
