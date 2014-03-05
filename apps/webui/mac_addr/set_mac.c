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

#define WIFI0_OFFSET 4098
#define WIFI1_OFFSET 20482
#define ETH0_OFFSET 0
#define BR0_OFFSET 6

#define NAME  "/dev/caldata"

int main(int argc,char **argv)
{

    int mac_data[20] = {0};
	char  mac_buff[20];
	int i;
	FILE		*f;

	if(  argc !=  3){
		printf("usage:set_mac eth0/br0/wifi0/wifi1 00:11:22:33:44:55 \n");
		return 1;
	}
	if((!strncmp(argv[1],"br0",3))||(!strncmp(argv[1],"eth0",4))||(!strncmp(argv[1],"wifi0",5))||(!strncmp(argv[1],"wifi0",5))){

		memset(mac_buff,0,sizeof(mac_buff));
		strncpy(mac_buff,argv[2],17);
		sscanf(mac_buff,"%02x:%02x:%02x:%02x:%02x:%02x",&mac_data[0],&mac_data[1],&mac_data[2],&mac_data[3],&mac_data[4],&mac_data[5]);
		f = fopen(NAME,"w+");
		if(!f)
		{
			printf("open file error\n");
	        return (-1);
		}
		
	//	printf("open file success\n");
		if(!strncmp(argv[1],"br0",3))
		{
			fseek(f, BR0_OFFSET, SEEK_SET);
		}

		if(!strncmp(argv[1],"eth0",4))
		{
			fseek(f, ETH0_OFFSET, SEEK_SET);
		}
		
		if(!strncmp(argv[1],"wifi0",5))
		{
			fseek(f, WIFI0_OFFSET, SEEK_SET);
		}

		if(!strncmp(argv[1],"wifi1",5))
		{
			fseek(f, WIFI1_OFFSET, SEEK_SET);
		}
		
		sprintf(mac_buff,"%c%c%c%c%c%c",mac_data[0],mac_data[1],mac_data[2],mac_data[3],mac_data[4],mac_data[5]);

		int len=fwrite(mac_buff,6,1,f);
		if (len < 0 )
			printf("write mac addr error\n");

		fclose(f);
	}
	return 0;
}


