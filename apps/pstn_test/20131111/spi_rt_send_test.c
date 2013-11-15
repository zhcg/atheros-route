#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spi_rt_interface.h"


int print_buf_hex(char *data, unsigned short len)
{
	int i = 0; 
	PRINT("[len = %d; data = ", len);
	for (i = 0; i < len; i++)
	{
		printf("%02X ", (unsigned char)data[i]);
	}
	printf("]\n");
}

int main(int argc, char ** argv)
{
    strcpy(common_tools.argv0, argv[0]);
	int i = 0, k = 0;
	int res = 0;
	char *recv_buf = NULL;
	unsigned char len = 0;
	
	if (argc != 5)
	{
		PRINT("[cmd uart_num len count usleep!]\n");
		return -1;
	}
	len = strlen(argv[2]);
	PRINT("uart_num = %d, len: %d, recv count = %d, udelay = %d\n", atoi(argv[1]), len, atoi(argv[3]), atoi(argv[4]));
	
	if ((atoi(argv[1]) == 1) || (atoi(argv[1]) == 2) || (atoi(argv[1]) == 3))
	{
    	for (i = 0; i < atoi(argv[3]); i++)
    	{
    		if ((res = spi_rt_interface.send_data(atoi(argv[1]), argv[2], len)) <= 0)
    		{
    		    PRINT("res = %d\n", res);
    			PRINT("send error: %s %s %s %s\n", argv[0], argv[1], argv[2], argv[3]);
    			k++;
    		}
    		else 
            {
                PRINT("res = %d\n", res);
            }
    		PRINT("[i = %d; failed count: %d]\n", i, k);
    		printf("********************************************************\n\n");
    		usleep(atoi(argv[4]));
    	}
    }
    else 
    {
        PRINT("[uart_num error!]\n");
        return -1;
    }
    
	free(recv_buf);
	recv_buf = NULL;
	return 0;
}
