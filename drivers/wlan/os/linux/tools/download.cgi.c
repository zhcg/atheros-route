	#include<stdio.h>
	#include<stdlib.h>
	#include<string.h>
	#include<sys/stat.h>
	#include<unistd.h>
    #include "filelist.h"

	#define MAX_FILE_LEN  (1024*30)
	#define DOWNLOAD_FILE_PATH	"/tmp/"
	#define DOWNLOAD_FILE_NAME	"cal.bin"


static FILE *errOut;
	const char *staFile = "/etc/.staMac";
	struct staList
	{
		int id;
		char macAddr[20];
		char staDesc[80];
		struct staList *next;
	};
void backup_file(void)
{
    int i = 0;
    int len=0;
    char cmd[255]={0};
    char rspBuff[255]={0};
    int addr = 64*1024;

    struct stat sb;

    while(strlen(backup_file_list[i]) !=0)
    {
        memset(&sb,0,sizeof(struct stat));
		stat(backup_file_list[i], &sb); //取文件的大小
        memset(cmd,0,255);
        sprintf(cmd,"echo \"%8d\" | dd of=/tmp/cal.bin bs=1 count=8 seek=%d > /dev/null 2>&1", sb.st_size, addr);
        printf("%s\n", cmd);
        system(cmd);
        addr += 8;

        memset(cmd,0,255);
        sprintf(cmd,"cat %s >> /tmp/cal.bin 2> /dev/null", backup_file_list[i]);
        addr += sb.st_size;
        //printf("%s\n", cmd);
        system(cmd);

        i++;
    }

}

    
	int main(){
		errOut = fopen("/dev/ttyS0","w");
		FILE *fp, *fpp;
		char filebuf[MAX_FILE_LEN];
		char cmd[512];
		struct stat sb;
		struct staList stalist;
		
		system("dd if=/dev/caldata of=/tmp/cal.bin  > /dev/null 2>&1");

        backup_file();

		if ((fp = fopen("/tmp/cal.bin", "ab")) != NULL) 
		{
			fwrite("\nstaMac=\n", 9, 1, fp);
			if ((fpp = fopen(staFile, "r")) != NULL)
			{	
				while(fread(&stalist, sizeof stalist, 1, fpp) == 1)
				{
					fwrite(&stalist, sizeof(struct staList), 1, fp);
				}
				fclose(fpp);
			}
		}
		//fprintf(errOut,"\n%s  %d  zhaozhanwei444\n",__func__,__LINE__);
		fclose(fp);
		
		
		sprintf(cmd, "%s%s", DOWNLOAD_FILE_PATH, DOWNLOAD_FILE_NAME);
		stat(cmd, &sb); //取待下载文件的大小
		
		//输出HTTP头信息，输出附加下载文件、文件长度以及内容类型
		printf("Content-Disposition:attachment;filename=%s", DOWNLOAD_FILE_NAME);
		printf("\r\n"); 
		printf("Content-Length:%d", sb.st_size);
		printf("\r\n");
	//	printf("Content-Type:application/octet-stream %c%c", 13,10); (ascii:"13--\r", "10--\n") 与下一行等同
		printf("Content-Type:application/octet-stream\r\n");
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

		return 1;
	}
