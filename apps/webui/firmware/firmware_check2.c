/*
** include files
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>

#define FIRST_OFFSET 16

#define FLAG_OFFSET 32772
#define MAX_SIZE  1024
#define STA_MAC "/configure_backup/.staMac"

struct staList
{
	int id;
	char macAddr[20];
	char staDesc[80];
	char status[10];  /*on-enable-1; off-disable-0 */
	struct staList *next;
};

void parse_staControl(FILE *fp)
{
	char buf[MAX_SIZE];
	int count;
	FILE *fpp, *fpp1;
	struct staList stalist;
	static FILE *errOut;
	errOut = fopen("/dev/ttyS0","w");

	
	
	while(fgets(buf, 17, fp))
	{
		//fprintf(errOut,"%s  %d buf is %s \n",__func__,__LINE__, buf);
		if(!strncmp(buf, "WCONON_OFF=", 11))
		{
			fpp1 = fopen("/etc/.staAcl", "w");
			//fprintf(errOut,"%s  %d &buf[11] is %s \n",__func__,__LINE__, &buf[11]);
			if(!strncmp(&buf[11], "on", 2))
				fwrite("enable", sizeof("enable"), 1, fpp1);
			else
				fwrite("disable", sizeof("disable"), 1, fpp1);
			
			fclose(fpp1);
		}
		
		if(!strncmp(buf, "staMac=", 7))
		{
			system("rm -rf /configure_backup/.staMac");
			fpp = fopen(STA_MAC, "at");
			while(fread(&stalist, sizeof stalist, 1, fp) == 1)
			{
				//fprintf(errOut,"%s  %d stalist.macAddr is %s \n",__func__,__LINE__, stalist.macAddr);
				if(stalist.id > 0 && stalist.id < 200)
					fwrite(&stalist, sizeof(stalist), 1, fpp);
			}
			fclose(fpp);
		}
	}
	
	
}

char flagbuf[]="ATH_countrycode=";
int main(int argc,char **argv)
{
    FILE  *f;
	int len;
	char buf[MAX_SIZE];
	int i=0;
	int ret = 0;
	static FILE *errOut;
	errOut = fopen("/dev/ttyS0","w");
	memset (buf , 0, sizeof(buf));
	
	if(  argc !=  2){
		printf("usage:fireware_check2  cal.bin\n");
		return 1;
	}
	f = fopen( argv[1] , "r" );
	if (!f)
	{
		printf("ERROR:  the fireware_check2 file is not found\n");
		exit(-1);
	}

	#if 0
	fseek(f, FIRST_OFFSET, SEEK_SET);
	len = fread (buf ,1,4,f );
	if( !len)
	{
		printf("ERROR:  the FIRST_OFFSET read operation is fail\n");
	}

	memset (buf , 0, sizeof(buf));

	
	fseek(f, FLAG_OFFSET, SEEK_SET);
	len = fread (buf ,1,16,f );
	#endif
	while(fgets(buf, 17, f))
	{
		//fprintf(errOut,"%s  %d the read is [%s]\n",__func__,__LINE__, buf);
		if (strncmp(buf,flagbuf,16) == 0)
		{
			//fprintf(errOut,"%s  %d write %s ok\n",__func__,__LINE__);
			ret = 0;
			parse_staControl(f);
			break;
		}
		else
			ret = 1;
	}
	//fprintf(errOut,"%s  %d the ret is %d\n",__func__,__LINE__, ret);
	if(ret == 1)
		return 1;
	/*
	if( !len)
	{
		printf("ERROR:  the read operation is fail\n");
	}


	if (strncmp(buf,flagbuf,16) != 0)
	{
		
		printf("ERROR:  the flag of cal.bin is wrong\n");
		
		return 1;
	}
	*/
	
	printf("It's OK\n");
		
	fclose(f);

	return 0;
}

