#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/stat.h>
#include<unistd.h>
#include "filelist.h"
#define MAX_FILE_LEN  (1024*30)
#define DOWNLOAD_FILE_PATH	"/var/log/"
#define DOWNLOAD_FILE_NAME	"lc65xx.log"


static FILE *errOut;
    
int main()
{
	errOut = fopen("/dev/ttyS0","w");
	FILE *fp, *fpp;
	char filebuf[MAX_FILE_LEN];
	char cmd[512];
	struct stat sb;
	char outbuff[65536];
	sprintf(cmd, "%s%s", DOWNLOAD_FILE_PATH, DOWNLOAD_FILE_NAME);
	stat(cmd, &sb); //取待下载文件的大小
	//输出HTTP头信息，输出附加下载文件、文件长度以及内容类型
	//printf("Content-Disposition:attachment;filename=%s", DOWNLOAD_FILE_NAME);
	printf("\r\n"); 
	//printf("Content-Length:%d", sb.st_size);
	printf("\r\n");
	printf("Content-Type:text/plain\r\n");
	printf("\r\n");
	sprintf(cmd, "%s%s", DOWNLOAD_FILE_PATH, DOWNLOAD_FILE_NAME);
	if(fp=fopen(cmd, "r+b")){  
	//成功打开文件，读取文件内容
		do{
			int rs = fread(filebuf, 1, sizeof(filebuf), fp);
			
			fwrite(filebuf, rs, 1, stdout);
		}while(!feof(fp));
		fclose(fp);	
	}
	//fanhui(outbuff,filebuf);
	return 1;
}
int fanhui(char *outbuff,char *buf)
{
	outbuff += sprintf(outbuff,"<html>\n");
	outbuff += sprintf(outbuff,"<head>\n");
	outbuff += sprintf(outbuff,"<title></title>\n");
	outbuff += sprintf(outbuff,"<LINK REL=\"stylesheet\" HREF=\"/style/handaer.css\" TYPE=\"text/css\" media=\"all\">\n");
	outbuff += sprintf(outbuff,"<link rel=\"stylesheet\" href=\"/style/normal_ws.css\" type=\"text/css\">\n");
	outbuff += sprintf(outbuff,"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n");
	outbuff += sprintf(outbuff,"<META http-equiv=Pragma content=no-cache>\n");
	outbuff += sprintf(outbuff,"<META HTTP-EQUIV=\"CACHE-CONTROL\" CONTENT=\"NO-CACHE\">\n");
	outbuff += sprintf(outbuff,"<script language=\"JavaScript\" type=\"text/javascript\" src=\"/lang/b28n.js\"></script>\n");
	outbuff += sprintf(outbuff,"<script language=\"JavaScript\" type=\"text/javascript\" src=\"/style/basic.js\"></script>\n");
	outbuff += sprintf(outbuff,"<script language=\"JavaScript\" type=\"text/javascript\" src=\"/style/load.js\"></script>\n");
	outbuff += sprintf(outbuff,"<script language=\"JavaScript\" type=\"text/javascript\" src=\"/style/upload.js\"></script>\n");
	outbuff += sprintf(outbuff,"<script language=\"JavaScript\" type=\"text/javascript\" src=\"/style/localinfo.js\"></script>\n");
	outbuff += sprintf(outbuff,"<script type=\"text/javascript\">\n");
	outbuff += sprintf(outbuff,"if(getCookie()){}\n");
	outbuff += sprintf(outbuff,"else{window.top.location.href=\"../login.html\";}\n");
	outbuff += sprintf(outbuff,"</script>\n");
	outbuff += sprintf(outbuff,"</head>\n");
	outbuff += sprintf(outbuff,"<body onload=\"init();\" oncontextmenu=\"window.event.returnValue=false\" >\n");
	outbuff += sprintf(outbuff,"<div class=\"handaer_main\">\n");
	outbuff += sprintf(outbuff,"<table  border=\"0\" cellpadding=\"0\" cellspacing=\"0\" id=\"tt1\"><tr><td>\n");
	outbuff += sprintf(outbuff,"<h1 id=\"paramsetting\">调试Log</h1>\n");
	outbuff += sprintf(outbuff,"<table id=\"uploadff\" width=\"100%\" cellpadding=\"0\" cellspacing=\"0\"   class=\"handaer_text\">\n");
	outbuff += sprintf(outbuff,"<tr><td class=\"title\" colspan=\"4\">调试窗口</td></tr>\n");
	outbuff += sprintf(outbuff,"<tr><td class=\"head\" id=\"arg1\">操作</td><td>\n");
	outbuff += sprintf(outbuff,"<form method=\"post\" name=\"ExportSettings\" action=\"/cgi-bin/log_show.cgi\">\n");
	outbuff += sprintf(outbuff,"<input id=\"Upload\" class=\"handaer_input\" type=\"submit\" name=\"Upload\" value=\"显示日志\"></form></td>\n");
	outbuff += sprintf(outbuff,"<td><form method=\"post\" name=\"ExportSettings\" action=\"/cgi-bin/log_download.cgi\">\n");
	outbuff += sprintf(outbuff,"<input id=\"Download\" class=\"handaer_input\" type=\"submit\" name=\"Download\" value=\"下载日志\"></form>\n");
	outbuff += sprintf(outbuff,"</td><td></td></tr><tr><td class=\"head\" id=\"arg2\">日志显示</td>\n");
	outbuff += sprintf(outbuff,"<td colspan=\"3\"><textarea id=\"log\" name=\"log\" rows=\"30\" cols=\"100\">%s</textarea></td></tr>\n");
	outbuff += sprintf(outbuff,"<tr><form name=\"ExportSettings\" action=\"/cgi-bin/log_download.cgi\" method=\"post\">\n");
	outbuff += sprintf(outbuff,"<td class=\"head\" id=\"arg3\">执行命令</td><td colspan=\"2\"><input type=\"text\"  class=\"handaer_text_content\" name=\"cmd\" size=\"40\">\n");
	outbuff += sprintf(outbuff,"</textarea></td><td><input type=\"submit\" class=\"handaer_input\" name=\"UPGRADE\" id=\"UPGRADE\" value=\"执行\">\n");
	outbuff += sprintf(outbuff,"</td></form></tr></table></td></tr></table></div>\n");
	outbuff += sprintf(outbuff, "<input type=\"text\" style=\"display:none\" name=\"IPA\" id=\"IPA\" size=\"20\" maxlength=\"32\" value=\"10.10.10.254\">\n");
	outbuff += sprintf(outbuff, "</body>\n");
	outbuff += sprintf(outbuff, "</html>\n");
	
	
	printf("%s", outbuff);

	return 0;
}
