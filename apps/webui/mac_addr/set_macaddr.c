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

    unsigned mac_data[20] = {0};
	char  mac_buff[20];
	int i;
	FILE		*f;
	int  len;

	if(  argc !=  2){
		printf("usage:set_macaddr 00:11:22:33:44:55 \n");
		return 1;
	}

	memset(mac_buff,0,sizeof(mac_buff));
	
//	printf("argv[1]  is %d \n",strlen(argv[1]));
	if ( strlen(argv[1]) != 17 ){
		printf("mac address is not right format\n");
        return (-1);
	}
	strncpy(mac_buff,argv[1],17);
	sscanf(mac_buff,"%02x:%02x:%02x:%02x:%02x:%02x",&mac_data[0],&mac_data[1],&mac_data[2],&mac_data[3],&mac_data[4],&mac_data[5]);
	f = fopen(NAME,"w+");
	if(!f)
	{
		printf("open file error\n");
        return (-1);
	}
	
//	printf("open file success\n");
	if ( mac_data[0] & 0x01 ){
		fclose(f);
		printf("mac address error\n");
		return -1;
	}
	if ((( mac_data[5] == 0xfc ) || ( mac_data[5] == 0xfd ) ||( mac_data[5] == 0xfe )||( mac_data[5] == 0xff )) &&
		(( mac_data[4] == 0xff ) && ( mac_data[3] == 0xff )&&( mac_data[2] == 0xff )&&( mac_data[1] == 0xff ))){
		fclose(f);
		printf("mac address don't fit\n");
		return -1;
	}
	
		fseek(f, ETH0_OFFSET, SEEK_SET);
		sprintf(mac_buff,"%c%c%c%c%c%c",mac_data[0],mac_data[1],mac_data[2],mac_data[3],mac_data[4],mac_data[5]);
		
		len=fwrite(mac_buff,6,1,f);
		if (len < 0 )
			printf("write mac addr error\n");

		fseek(f, BR0_OFFSET, SEEK_SET);
		if ( mac_data[5] == 0xff ){
			mac_data[5] = 0x00;
			if ( mac_data[4] == 0xff ){
				mac_data[4] = 0x00;
				if ( mac_data[3] == 0xff ){
					mac_data[3] = 0x00;
					if ( mac_data[2] == 0xff ){
						mac_data[2] = 0x00;
						mac_data[1] ++;
					}
					else
						mac_data[2] ++;
				}
				else
					mac_data[3] ++;
			}
			else
				mac_data[4] ++;
		}
		else
			mac_data[5] ++;
		
		sprintf(mac_buff,"%c%c%c%c%c%c",mac_data[0],mac_data[1],mac_data[2],mac_data[3],mac_data[4],mac_data[5]);
		
		len=fwrite(mac_buff,6,1,f);
		if (len < 0 )
			printf("write mac addr error\n");

		fseek(f, WIFI0_OFFSET, SEEK_SET);
		if ( mac_data[5] == 0xff ){
			mac_data[5] = 0x00;
			if ( mac_data[4] == 0xff ){
				mac_data[4] = 0x00;
				if ( mac_data[3] == 0xff ){
					mac_data[3] = 0x00;
					if ( mac_data[2] == 0xff ){
						mac_data[2] = 0x00;
						mac_data[1] ++;
					}
					else
						mac_data[2] ++;
				}
				else
					mac_data[3] ++;
			}
			else
				mac_data[4] ++;
		}
		else
			mac_data[5] ++;
		
		sprintf(mac_buff,"%c%c%c%c%c%c",mac_data[0],mac_data[1],mac_data[2],mac_data[3],mac_data[4],mac_data[5]);
		
		len=fwrite(mac_buff,6,1,f);
		if (len < 0 )
			printf("write mac addr error\n");

		fseek(f, WIFI1_OFFSET, SEEK_SET);
		if (( mac_data[5] == 0xff ) || ( mac_data[5] == 0xfe )){
			if( mac_data[5] == 0xfe )
				mac_data[5] = 0x00;
			else
				mac_data[5] = 0x01;
			if ( mac_data[4] == 0xff ){
				mac_data[4] = 0x00;
				if ( mac_data[3] == 0xff ){
					mac_data[3] = 0x00;
					if ( mac_data[2] == 0xff ){
						mac_data[2] = 0x00;
						mac_data[1] ++;
					}
					else
						mac_data[2] ++;
				}
				else
					mac_data[3] ++;
			}
			else
				mac_data[4] ++;
		}
		else
			mac_data[5] = mac_data[5] + 2;
		
		sprintf(mac_buff,"%c%c%c%c%c%c",mac_data[0],mac_data[1],mac_data[2],mac_data[3],mac_data[4],mac_data[5]);
		
		len=fwrite(mac_buff,6,1,f);
		if (len < 0 )
			printf("write mac addr error\n");
	
	fclose(f);
	return 0;
}



