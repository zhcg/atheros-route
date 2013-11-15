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
	int i = 0, j = 0, k = 0;
	int res = 0;
	char *recv_buf = NULL;
	char send_buf[64] = {0};
	char log_tmp[128] = {0};
	char *log_buf = NULL;
	char *tmp = NULL;
	unsigned short log_buf_len = 0;
	unsigned char len = 0;
	
	if (argc != 5)
	{
		PRINT("[cmd uart_num data count usleep!]\n");
		return -1;
	}
	len = strlen(argv[2]);
	PRINT("[argv[2]'s len: %d]\n", strlen(argv[2]));
	
	if ((atoi(argv[1]) == 1) || (atoi(argv[1]) == 2) || (atoi(argv[1]) == 3))
	{
	    if ((recv_buf = malloc(strlen(argv[2]) + 3)) == NULL)
    	{
    		PERROR("[malloc failed!]\n");
    		return -1;
    	}
    	memcpy(send_buf + 2, argv[2], strlen(argv[2]));
    	len += 2;
    	for (i = 0; i < atoi(argv[3]); i++)
    	{
    		#if 1
    		printf("******************************************************************************\n\n");
    		usleep(atoi(argv[4]) + 1);
    		memset(recv_buf, 0, strlen(argv[2]) + 1);
    		if ((res = spi_rt_interface.recv_data(atoi(argv[1]), recv_buf, len)) <= 0)
    		{
    		    PRINT("res = %d\n", res);
    			PERROR("recv error: %s %s %s %s\n", argv[0], argv[1], argv[2], argv[3]);
    			/*
    			free(recv_buf);
    			recv_buf = NULL;
    			return res;
    			*/
    			k++;
    			/*
    			memset(log_tmp, 0, sizeof(log_tmp));
    			sprintf(log_tmp, "uart:%s; send:%02X %02X %s;recv:(null)\n", argv[1], send_buf[0], send_buf[1], send_buf + 2);
    			//PRINT("log_tmp = %s", log_tmp);
    			log_buf_len += strlen(log_tmp);
    			if ((tmp = realloc(log_buf, log_buf_len + 1)) == NULL)
    			{
    			    PERROR("realloc failed!\n");
    			    
    			}
    			else
    			{
    			    log_buf = tmp;
    			    strncat(log_buf, log_tmp, strlen(log_tmp));
    			    PRINT("log_buf = %s\n", log_buf);
    			}
    			*/
    		}
    		else 
            {
                PRINT("res = %d\n", res);
                PRINT("[uart_num = %s, recv data: %s]\n", argv[1], recv_buf + 2);
        		print_buf_hex(recv_buf, res);
        		
        		for (j = 0; j < strlen(argv[2]); j++)
        	    {
        			//if (recv_buf[j] != (argv[2][j] + 1))
        			if (recv_buf[j] != send_buf[j])
        			{
        				k++;
        				/*
        				memset(log_tmp, 0, sizeof(log_tmp));
            			sprintf(log_tmp, "uart:%s; send:%02X %02X %s;recv:%02X %02X %s\n", argv[1], send_buf[0], send_buf[1], argv[2], recv_buf[0], recv_buf[1], recv_buf + 2);
            			PRINT("log_tmp = %s", log_tmp);
            			log_buf_len += strlen(log_tmp);
            			if ((tmp = realloc(log_buf, log_buf_len + 1)) == NULL)
            			{
            			    PRINT("realloc failed!\n");
            			}
            			else
            			{
            			    log_buf = tmp;
            			    strncat(log_buf, log_tmp, strlen(log_tmp));
            			    PRINT("log_buf = %s\n", log_buf);
            			}
            			*/
        				break;
        			}
        		}
            }
    		#endif
    		
    		printf("********************************************************\n\n");
    		PRINT("[uart_num = %s, send data: %s]\n", argv[1], argv[2]);
    		
    		send_buf[0] = (unsigned char)((i + 1) >> 8);
    		send_buf[1] = (unsigned char)(i + 1);
    		
    		print_buf_hex(send_buf, len);
    		if ((res = spi_rt_interface.send_data((char)atoi(argv[1]), send_buf, len)) < 0)
    		{
    			PERROR("send error: %s %s %s %s\n", argv[0], argv[1], argv[2], argv[3]);
    			k++;
    			PERROR("[i = %d; failed count: %d]\n", i, k);
    		    printf("********************************************************\n\n");
    			continue;
    		}
    		
    		PRINT("[i = %d; failed count: %d]\n", i, k);
    		printf("********************************************************\n\n");
    		usleep(atoi(argv[4]));
    	}
    }
    else if (atoi(argv[1]) == 0)
    {
        char buf = atoi(argv[2]);
        switch (buf)
        {
            case 0x02:
            {
                PRINT("打开IC卡指示灯！\n");
                break;
            }
            case 0x03:
            {
                PRINT("关闭IC卡指示灯！\n");
                break;
            }
            case 0x04:
            {
                PRINT("打开磁卡指示灯！\n");
                break;
            }
            case 0x05:
            {
                PRINT("关闭磁卡指示灯！\n");
                break;
            }
            case 0x06:
            {
                PRINT("打开未接来电指示灯！\n");
                break;
            }
            case 0x07:
            {
                PRINT("关闭未接来电指示灯！\n");
                break;
            }
            default:
            {
                PRINT("[data error!]\n");
                return -1;
            }
        }
        for (i = 0; i < atoi(argv[3]); i++)
    	{
    		printf("********************************************************\n\n");
    		PRINT("[uart_num = %s, send data: %c]\n", argv[1], argv[2][0]);
    		
    		if ((res = spi_rt_interface.send_data((char)atoi(argv[1]), &buf, sizeof(char))) < 0)
    		{
    			PRINT("send error: %s %s %s %s\n", argv[0], argv[1], argv[2], argv[3]);
    			return res;
    		}
    		printf("********************************************************\n\n");
    	}
    	close(spi_rt_interface.spi_fd);
    	return 0;
    }
    else 
    {
        PRINT("[uart_num error!]\n");
        return -1;
    }
	free(recv_buf);
	recv_buf = NULL;
	if (log_buf != NULL)
	{
	    free(log_buf);
	    log_buf = NULL;
	}
	return 0;
}
