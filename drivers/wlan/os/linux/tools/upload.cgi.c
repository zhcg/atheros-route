#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "dirent.h"
#include <assert.h>

static FILE *errOut;

static int atoii (char *zzzz)
{
  int i = 0;
  int num=0;
for(i=0;i<20;i++)
{
  if(zzzz[i] >= '0' && zzzz[i] <= '9')
  	{
	  num = num * 10 + (zzzz[i] - '0');
 	}else
 	{
 			break;
 	}
}
  return num;
}


char* getCgiData(FILE* fp, char* requestmethod)
{

       char* input;
       int len;
	   char *pppp;
       int size = 1024;
       int i = 0;
     if(!strcmp(requestmethod, "GET"))
       {

              input = getenv("QUERY_STRING");
              return input;

       }
       else if (!strcmp(requestmethod, "POST"))
       {
   		    pppp=getenv("CONTENT_LENGTH");
             len = atoii(pppp);
              input = (char*)malloc(sizeof(char)*(size + 1));    

              if (len == 0)
              {
                     input[0] = '\0';

                     return input;
              }
        
		fgets(input, len+1, stdin);
			input[len]='\0';
		 return input;
       }

       return NULL;
}

static unsigned int tmppp=0;

char *getFileName(unsigned char *req)
{
	int i;
	int leng;
	tmppp=0;
	char *psz1; char *psz2;
	unsigned char *cur_post,*buf;

	// get filename keyword
	if ((psz1=strstr(req, "filename=")) == NULL)
	{
	return (char *)&tmppp;
	}

	// get pointer to actual filename (it's in quotes)
	psz1+=strlen("filename=");
	if ((psz1 = strtok(psz1, "\"")) == NULL)
	{
	return (char *)&tmppp;
	}
	// remove leading path for both PC and UNIX systems
	if ((psz2 = strrchr(psz1,'\\')) != NULL)
	{
	psz1 = psz2+1;
	}
	if ((psz2 = strrchr(psz1,'/')) != NULL)
	{
	psz1 = psz2+1;
	}
	return psz1;
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

main()
{
	char *reqMethod;
	char *wp;
	char *var=NULL;
	int len;
	long total,i,count;
	char *fileName,*ps1,*ps2;
	char *fileN;
	char Boundary[256];
	char error=0;
	char tmpBuf[512];
	char filePath[256]="/tmp/";//directory of uploaded file
	FILE *fileBuf=NULL;
	reqMethod=getenv("REQUEST_METHOD");
	len=atoii(getenv("CONTENT_LENGTH"));

    
        errOut = fopen("/dev/ttyS0","w");

        
	//printf("Content-type:text/html \r\n\r\n");
	//printf("<html><head><meta http-equiv=\"Content-Type\" content=\"text/html;charset=UTF-8\"><script 	type=\"text/javascript\" src=\"/lang/b28n.js\"></script></head><body>");

	Boundary[0] = '\r';
	Boundary[1] = '\n';
	Boundary[2] = '\0';

	if (fgets(&Boundary[2], sizeof(Boundary)-2, stdin) == NULL)
	{
                error=1;
                fprintf(errOut,"%s  %d Get boundary failed !\n",__func__,__LINE__);
		goto error;
	}

	//strip terminating CR / LF
	if ((ps1=strchr(&Boundary[2],'\r')) != NULL)
	{
		*ps1 = '\0';
	}

	if ((ps1=strchr(&Boundary[2],'\n')) != NULL)
	{
		*ps1 = '\0';
	}
	fprintf(errOut,"%s  %d  Boundry=%s\n",__func__,__LINE__,Boundary);
	fprintf(errOut,"%s  %d  content-length=%d\n",__func__,__LINE__,len);

	fgets(tmpBuf,512,stdin);
       fprintf(errOut,"%s  %d  All=%s\n",__func__,__LINE__,tmpBuf);

	fileName=getFileName(tmpBuf);

	if(fileName)
	{
            fprintf(errOut,"%s  %d  fileName=%s\n",__func__,__LINE__,fileName);
	}
	strcat(filePath,fileName);	

    fprintf(errOut,"%s  %d  filepath===%s\n",__func__,__LINE__,filePath);

	memset(tmpBuf,512,0x00);
	fgets(tmpBuf,512,stdin);
	//printf("%s<br>",tmpBuf);//content-type
    fprintf(errOut,"%s  %d  tmpBuf===%s\n",__func__,__LINE__,tmpBuf);
	memset(tmpBuf,512,0x00);
	fgets(tmpBuf,512,stdin);	
	//printf("%s<br>",tmpBuf);// \r\n
    fprintf(errOut,"%s  %d  tmpBuf2===%s\n",__func__,__LINE__,tmpBuf);

	if(fopen(filePath,"rb"))
	{
	    char cmdd[256];
        fprintf(errOut,"%s  %d File already exist but delete it.\n",__func__,__LINE__);
        sprintf(cmdd,"rm %s",filePath);
	 	system(cmdd);
	}

	if ((fileBuf = fopen(filePath, "wb+")) == NULL)
	{
        fprintf(errOut,"%s  %d File open error.Make sure you have the permission.\n",__func__,__LINE__);
        error=1;
		goto error;
	}
	// copy the file
	while ((count=fread(tmpBuf, 1, 512, stdin)) != 0)
	{
		if ((fwrite(tmpBuf, 1, count, fileBuf)) != count)
		{
                    fprintf(errOut,"%s  %d Write file error.\n",__func__,__LINE__);
                    error=1;
			goto error;
		}
	}
	// re read last 128 bytes of file, handling files < 128 bytes
	if ((count = ftell(fileBuf)) == -1)
	{
		error=1;
              goto error;
	}

	if (count > 128)
	{
            count = 128;
	}

	if (fseek(fileBuf, 0-count, SEEK_END) != 0)
	{
            error=1;
            goto error;
	}
	// get the new position
	if ((total = ftell(fileBuf)) == -1)
	{
	    error=1;
            goto error;
	}

	// and read the data from fileBuf
	count = fread(tmpBuf, 1, sizeof(tmpBuf), fileBuf);
	tmpBuf[count] = '\0';
    fprintf(errOut,"%s  %d count==%ld.\n",__func__,__LINE__,count);
    // determine offset of terminating boundary line
	for (i=0; i<=(count); i++)//-(long)strlen(Boundary)
	{
              fprintf(errOut,"%c",tmpBuf[i]);
		//printf("%c",tmpBuf[i]);
		if (tmpBuf[i] == Boundary[0])
		{
                     fprintf(errOut,"found /r");
			if(strncmp(Boundary, &tmpBuf[i], strlen(Boundary)) == 0)
			{
			    total+=i;
                        fprintf(errOut,"find boudary.");
			    break;
			}
		}
	}
       fprintf(errOut,"%s  %d i==%ld.\n",__func__,__LINE__,i);
       fprintf(errOut,"%s  %d total==%ld.\n",__func__,__LINE__,total);
	if (fseek(fileBuf,total, SEEK_SET) != 0)
	{
            error=1;
            goto error;
	}

	if ((total = ftell(fileBuf)) == -1)
	{
	    error=1;
	    goto error;
	}
    fprintf(errOut,"%s  %d total2==%ld.\n",__func__,__LINE__,total);
	// truncate the terminating boundary line .
	int fd=fileno(fileBuf);
	ftruncate(fd,total);

	fflush(fileBuf);
error:
	if (fileBuf != NULL)
	{
		fclose(fileBuf);
	}
	if(error==1)
	{
	     printf("Content-Type:text/html\n\n");
         printf("<HTML><HEAD>\r\n");
         printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
         printf("<script type=\"text/javascript\" src=\"/lang/b28n.js\"></script>");
         printf("</head><body>");
 	     printf("<script type='text/javascript' language='javascript'>Butterlate.setTextDomain(\"admin\");alert(_(\"err file upload\"));window.location.href=\"ad_man_upgrade\";</script>");
         printf("</body></html>");
    }
	//打印信息到网页的隐藏的iframe中
	else 
	{		
            fprintf(errOut,"%s  %d file upload complete !\n",__func__,__LINE__);
			char cmdd[256];
			int firmware_flag=0;
			sprintf(cmdd,"firmware_check %s > /tmp/firmware.log",filePath);
			system(cmdd);
			FILE *fileBuf2=NULL;
			if ((fileBuf2= fopen("/tmp/firmware.log", "r")) == NULL)
			{
				fprintf(errOut,"%s  %d File open error.Make sure you have the permission.\n",__func__,__LINE__);
				firmware_flag=1;
			}else
			{
				fgets(cmdd,sizeof(cmdd),fileBuf2);
				if(strstr(cmdd,"OK")!=NULL)
				{
					firmware_flag=2;
				}else
				{
					firmware_flag=1;
				}
				fclose(fileBuf2);
			}
					
			if(firmware_flag!=1)
			{
			#if 0
				printf("Content-Type:text/html\n\n");
				printf("<HTML><HEAD>\r\n");
				printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
				printf("</head><body>");
				printf("<div style=\"font-size: 14pt; font-weight: bold; margin-left: 10px; font-family: 微软雅黑, Arial, Helvetica, sans-serif; color: #848484;border-bottom:1px dotted #d0d0d0; padding-bottom:10px; margin-bottom:10px;height:30px; line-height:30px; padding:5px;\">固件升级</div>\n");	
				printf("<p align=\"center\" style=\"font-size: 9pt; margin-left: 10px; font-family: 微软雅黑, Arial, Helvetica, sans-serif; color: #848484\">升级完成,正在重启BASE..........</p><br>\n");	
				printf("<p align=\"center\" style=\"font-size: 9pt; margin-left: 10px; font-family: 微软雅黑, Arial, Helvetica, sans-serif; color: #848484\">The upgrade was completed, restartting BASE..........</p><br>\n");	
				printf("<script  language=javascript>setTimeout(function(){window.location.href=\"crupload\";},140000);</script>");
				printf("</body></html>");	
                #endif
                            Reboot_tiaozhuan("upload","index.html");
				//[TODO]factory default
				system("cfg -x");
				sleep(1);
		
				sprintf(cmdd,"sleep 1 && sysupgrade %s &",filePath);
				system(cmdd);
			}else //error firmware file
			{	
				 printf("Content-Type:text/html\n\n");
				 printf("<HTML><HEAD>\r\n");
				 printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
				 printf("<script type=\"text/javascript\" src=\"/lang/b28n.js\"></script>");
				 printf("</head><body>");
				 printf("<script type='text/javascript' language='javascript'>Butterlate.setTextDomain(\"admin\");alert(_(\"err file format\"));window.location.href=\"ad_man_upgrade\";</script>");
				 printf("</body></html>");
			}
	}
	return;
	
}

