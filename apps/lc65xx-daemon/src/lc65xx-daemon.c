/*************************************************************************
	> File Name: lc65xx-daemon.c
	> Author: 
	> Mail: @163.com 
	> Created Time: 2017年03月15日 星期三 13时53分59秒
 ************************************************************************/

#include "common.h"

extern int create_serial_thread(void);
extern int create_interface_thread(void);

int main(int argc, char* argv[])
{
	create_serial_thread();
	create_interface_thread();

	while(1)
	{
		sleep(100);
	}
	
	return 0;
}

