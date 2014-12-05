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

#define WIFIFLAG_OFFSET 4104
#define WIFI0_OFFSET 4098
#define WIFI1_OFFSET 20482
#define ETH0_OFFSET 0
#define BR0_OFFSET 6

#define NAME  "/dev/caldata"

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

int main(int argc,char **argv)
{

    unsigned int mac_data[20] = {0};
	char  mac_buff[20];
	int i;
	FILE		*f;
	int  len;
	char valBuf[30];

	if(  argc !=  3){
		printf("usage:set_macaddr -a 00:11:22:33:44:55 \n");
		printf("usage:set_macaddr -b 00:11:22:33:44:55 \n");
		return 1;
	}

	memset(mac_buff,0,sizeof(mac_buff));
	
	if(!strncmp(argv[1],"-a",2))
		{
		//	printf("argv[1]  is %d \n",strlen(argv[1]));
		if ( strlen(argv[2]) != 17 ){
			printf("mac address is not right format\n");
		    return (-1);
		}
		strncpy(mac_buff,argv[2],17);
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
		if ( mac_data[5] % 0x2 == 0 ){
		if ((( mac_data[5] == 0xfc ) || ( mac_data[5] == 0xfd ) ||( mac_data[5] == 0xfe )||( mac_data[5] == 0xff )) &&
			(( mac_data[4] == 0xff ) && ( mac_data[3] == 0xff )&&( mac_data[2] == 0xff )&&( mac_data[1] == 0xff ))){
			fclose(f);
			printf("mac address don't fit\n");
			return -1;
		}

		fseek(f, WIFIFLAG_OFFSET, SEEK_SET);

		len=fwrite(mac_buff,17,1,f);
		if (len < 0 )
			printf("write mac addr error\n");


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

		//	Execute_cmd("grep -c 168c /proc/bus/pci/devices", valBuf);
		//	printf("\n grep -c 168c /proc/bus/pci/devices value is %s  \n",valBuf);
		//	if (strncmp(valBuf,"1",1) == 0){
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
		if (len < 0 ){
			printf("write mac addr error\n");
			fclose(f);
			return -1;
		}
		//	}
		//	system("cfg -a ETH0_MAC=`/usr/sbin/get_mac eth0`");
		//	system("cfg -a ETH0_DFMAC=`/usr/sbin/get_mac eth0`");
		//	system("cfg -a BR0_MAC=`/usr/sbin/get_mac br0`");

		fclose(f);
		}
		else
			{
				if ((( mac_data[5] == 0xfc ) || ( mac_data[5] == 0xfd ) ||( mac_data[5] == 0xfe )||( mac_data[5] == 0xff )) &&
						(( mac_data[4] == 0xff ) && ( mac_data[3] == 0xff )&&( mac_data[2] == 0xff )&&( mac_data[1] == 0xff ))){
						fclose(f);
						printf("mac address don't fit\n");
						return -1;
					}
			
					fseek(f, WIFIFLAG_OFFSET, SEEK_SET);
			
					len=fwrite(mac_buff,17,1,f);
					if (len < 0 )
						printf("write mac addr error\n");
			
			
					fseek(f, ETH0_OFFSET, SEEK_SET);
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
			
					//	Execute_cmd("grep -c 168c /proc/bus/pci/devices", valBuf);
					//	printf("\n grep -c 168c /proc/bus/pci/devices value is %s  \n",valBuf);
					//	if (strncmp(valBuf,"1",1) == 0){
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
					if (len < 0 ){
						printf("write mac addr error\n");
						fclose(f);
						return -1;
					}
					
					fseek(f, BR0_OFFSET, SEEK_SET);
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
					}
		}
		if(!strncmp(argv[1],"-b",2))
		{
		//	printf("argv[1]  is %d \n",strlen(argv[1]));
		if ( strlen(argv[2]) != 17 ){
			printf("mac address is not right format\n");
		    return (-1);
		}
		strncpy(mac_buff,argv[2],17);
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
		if ( mac_data[5] % 0x2 == 0 ){		
			if ((( mac_data[5] == 0xfc ) || ( mac_data[5] == 0xfd ) ||( mac_data[5] == 0xfe )||( mac_data[5] == 0xff )) &&
				(( mac_data[4] == 0xff ) && ( mac_data[3] == 0xff )&&( mac_data[2] == 0xff )&&( mac_data[1] == 0xff ))){
				fclose(f);
				printf("mac address don't fit\n");
				return -1;
			}

			fseek(f, WIFIFLAG_OFFSET, SEEK_SET);

			len=fwrite(mac_buff,17,1,f);
			if (len < 0 )
				printf("write mac addr error\n");


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

			//	Execute_cmd("grep -c 168c /proc/bus/pci/devices", valBuf);
			//	printf("\n grep -c 168c /proc/bus/pci/devices value is %s  \n",valBuf);
			#if 0
				if (strncmp(valBuf,"1",1) == 0){
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
			if (len < 0 ){
				printf("write mac addr error\n");
				fclose(f);
				return -1;
			}
				}
				#endif
			//	system("cfg -a ETH0_MAC=`/usr/sbin/get_mac eth0`");
			//	system("cfg -a ETH0_DFMAC=`/usr/sbin/get_mac eth0`");
			//	system("cfg -a BR0_MAC=`/usr/sbin/get_mac br0`");
				}
		else
		{		
			if ((( mac_data[5] == 0xfc ) || ( mac_data[5] == 0xfd ) ||( mac_data[5] == 0xfe )||( mac_data[5] == 0xff )) &&
				(( mac_data[4] == 0xff ) && ( mac_data[3] == 0xff )&&( mac_data[2] == 0xff )&&( mac_data[1] == 0xff ))){
				fclose(f);
				printf("mac address don't fit\n");
				return -1;
			}

			fseek(f, WIFIFLAG_OFFSET, SEEK_SET);

			len=fwrite(mac_buff,17,1,f);
			if (len < 0 )
				printf("write mac addr error\n");


			fseek(f, ETH0_OFFSET, SEEK_SET);
			sprintf(mac_buff,"%c%c%c%c%c%c",mac_data[0],mac_data[1],mac_data[2],mac_data[3],mac_data[4],mac_data[5]);

			len=fwrite(mac_buff,6,1,f);
			if (len < 0 )
				printf("write mac addr error\n");

			fseek(f, BR0_OFFSET, SEEK_SET);
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

			//	Execute_cmd("grep -c 168c /proc/bus/pci/devices", valBuf);
			//	printf("\n grep -c 168c /proc/bus/pci/devices value is %s  \n",valBuf);
			#if 0
				if (strncmp(valBuf,"1",1) == 0){
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
			if (len < 0 ){
				printf("write mac addr error\n");
				fclose(f);
				return -1;
			}
				}
				#endif
			//	system("cfg -a ETH0_MAC=`/usr/sbin/get_mac eth0`");
			//	system("cfg -a ETH0_DFMAC=`/usr/sbin/get_mac eth0`");
			//	system("cfg -a BR0_MAC=`/usr/sbin/get_mac br0`");
				}
		fclose(f);
		}
	system("cfg -x");
	system("cfg -r AP_SSID_2");	
	system("cfg -r AP_SSID_4");	
	system("cfg -c");
		
	return 0;
}



