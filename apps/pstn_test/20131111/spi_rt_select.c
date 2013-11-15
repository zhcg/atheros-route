#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "spi_rt_interface.h"

int main(int argc, char ** argv)
{
    strcpy(common_tools.argv0, argv[0]);
	
	if ((spi_rt_interface.spi_uart1_recv_shared_mem_id = shmget(spi_rt_interface.spi_uart1_recv_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
    {
        PRINT("shmget failed!\n");
        return SHMGET_ERR;
    }
    
    if ((spi_rt_interface.spi_uart1_recv_shared_mem_buf = shmat(spi_rt_interface.spi_uart1_recv_shared_mem_id, 0, 0)) < 0)
    {
        PRINT("shmat failed!\n");
        return SHMAT_ERR;
    }
    
    PRINT("spi_rt_interface.spi_uart1_recv_shared_mem_buf = %X\n", spi_rt_interface.spi_uart1_recv_shared_mem_buf);
    
    if ((spi_rt_interface.spi_uart1_recv_shared_mem_id = shmget(spi_rt_interface.spi_uart1_recv_shared_mem_key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) < 0)
    {
        PRINT("shmget failed!\n");
        return SHMGET_ERR;
    }
    
    if ((spi_rt_interface.spi_uart1_recv_shared_mem_buf = shmat(spi_rt_interface.spi_uart1_recv_shared_mem_id, 0, 0)) < 0)
    {
        PRINT("shmat failed!\n");
        return SHMAT_ERR;
    }
    PRINT("spi_rt_interface.spi_uart1_recv_shared_mem_buf = %X\n", spi_rt_interface.spi_uart1_recv_shared_mem_buf);
    return 0;
}
