#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "spi_rt_interface.h"

unsigned char uart = 0;
pthread_mutex_t rt_mutex;

void* pthreat_recv_data(void* para)
{
	int i = 0;
	int res = 0;
	unsigned long recv_count = 0;
	char recv_buf[254] = {0};
	
	if ((uart == 1) || (uart == 2) || (uart == 3))
	{
    	while (1)
    	{
    	    pthread_mutex_lock(&rt_mutex);
    		
    		if ((res = spi_rt_interface.recv_data(uart, recv_buf, sizeof(recv_buf))) <= 0)
    		{
    		    #if PRINT_DATA
    			PRINT("recv error! res = %d\n", res);
    			#else
    			printf("recv error! res = %d\n", res);
    			#endif
    		}
    		else 
            {
                #if PRINT_DATA
                PRINT("recv_data res = %d\n", res);
                #else
                printf("recv_data res = %d\n", res);
                #endif
            }
    		recv_count++;
    		#if PRINT_DATA
            PRINT("recv_count = %d\n", recv_count);
            #else
            printf("recv_count = %d\n", recv_count);
            #endif
            
    	    pthread_mutex_unlock(&rt_mutex);
    	    usleep(10000);
    	}
    }
    else 
    {
        PRINT("[uart_num error!]\n");
        return (void *)-1;
    }
    return (void *)0;
}

void* pthreat_send_data(void* para)
{
	int i = 0;
	int res = 0;
	unsigned long send_count = 0;
	unsigned char hook_flag = 0;
	char on_hook[5] = {0xA5, 0x5A, 0x5E, 0x00, 0x5E};
	char of_hook[6] = {0xA5, 0x5A, 0x73, 0x01, 0x01, 0x73};
	
	if ((uart == 1) || (uart == 2) || (uart == 3))
	{
    	while (1)
    	{
            pthread_mutex_lock(&rt_mutex);
    		if (hook_flag == 0)
    		{
    		    if ((res = spi_rt_interface.send_data(uart, of_hook, sizeof(of_hook))) <= 0)
        		{
        		    #if PRINT_DATA
        			PRINT("send of_hook error! res = %d\n", res);
        			#else
        			printf("send of_hook error! res = %d\n", res);
        			#endif
        		}
        		else 
                {
                    #if PRINT_DATA
                    PRINT("send of_hook success! res = %d\n", res);
                    #else
                    printf("send of_hook success! res = %d\n", res);
                    #endif
                }
                hook_flag = 1;
    		}
    		else 
            {
                if ((res = spi_rt_interface.send_data(uart, on_hook, sizeof(on_hook))) <= 0)
        		{
        		    #if PRINT_DATA
        			PRINT("send on_hook error! res = %d\n", res);
        			#else
        			printf("send on_hook error! res = %d\n", res);
        			#endif
        		}
        		else 
                {
                    #if PRINT_DATA
                    PRINT("send on_hook success! res = %d\n", res);
                    #else
                    printf("send on_hook success! res = %d\n", res);
                    #endif
                }
                hook_flag = 0;
            }
    		
            send_count++;
            #if PRINT_DATA
            PRINT("send_count = %d\n", send_count);
            #else
            printf("send_count = %d\n", send_count);
            #endif
    	    pthread_mutex_unlock(&rt_mutex);
    	    //usleep(10000);
    	    sleep(1);
    	}
    }
    else 
    {
        PRINT("[uart_num error!]\n");
        return (void *)-1;
    }
    return (void *)0;
}

int main(int argc, char ** argv)
{
    strcpy(common_tools.argv0, argv[0]);
    signal(SIGSEGV, SIG_IGN);
    fflush(stdout);
	if (argc != 2)
	{
		PRINT("[cmd uart_num!]\n");
		return -1;
	}
	
	uart = atoi(argv[1]);
	
	pthread_t pthread_send_id, pthread_recv_id;
	pthread_create(&pthread_send_id, NULL, (void*)pthreat_send_data, NULL); 
    pthread_create(&pthread_recv_id, NULL, (void*)pthreat_recv_data, NULL);
    
    pthread_join(pthread_send_id, NULL);
    pthread_join(pthread_recv_id, NULL);
    return 0;
}


