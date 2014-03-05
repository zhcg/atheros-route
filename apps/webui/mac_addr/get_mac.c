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

#define ETH0_OFFSET 0
#define BR0_OFFSET 6
#define WIFI0_OFFSET 4098
#define WIFI1_OFFSET 20482


#define NAME  "/dev/caldata"

int main(int argc,char **argv)
{

    unsigned char buff[20] = {0};
	int i;
	FILE		*f;

	if(  argc !=  2){
		printf("usage:get_mac eth0/br0/wifi0/wifi1 \n");
		return 1;
	}

	if((!strncmp(argv[1],"br0",3))||(!strncmp(argv[1],"eth0",4))||(!strncmp(argv[1],"wifi0",5))||(!strncmp(argv[1],"wifi1",5))){
		memset(buff,0,sizeof(buff));
		
		f = fopen(NAME,"r");
		if(!f)
		{
			printf("open file error\n");
	        return (-1);
		}
	//	printf("open file success\n");
		if(!strncmp(argv[1],"eth0",4))
		{
			fseek(f, ETH0_OFFSET, SEEK_SET);
		}
		if(!strncmp(argv[1],"br0",3))
		{
			fseek(f, BR0_OFFSET, SEEK_SET);
		}		
		if(!strncmp(argv[1],"wifi0",5))
		{
			fseek(f, WIFI0_OFFSET, SEEK_SET);
		}
		
		if(!strncmp(argv[1],"wifi1",5))
		{
			fseek(f, WIFI1_OFFSET, SEEK_SET);
		}
		fread(buff,6,1,f);

		for(i=0;i<6;i++)
		{		
			if(i<5)			
				printf("%02x:",buff[i]);
			else			
				printf("%02x",buff[i]);
		}
		fclose(f);
	}
	return 0;
}

