#include "common.h"
#include "modules_server.h"

int main(int argc,char **argv)
{
	pthread_t handle_pthread;
	pthread_t uart_pthread;
	pthread_t passage_pthread;
	
	init_env();
	
	pthread_create(&uart_pthread,NULL,(void*)uart_loop_recv,NULL);
	pthread_create(&handle_pthread,NULL,(void*)loop_recv,NULL);
	pthread_create(&passage_pthread,NULL,(void*)passage_thread_func,NULL);

	pthread_join(uart_pthread,NULL);
	pthread_join(handle_pthread,NULL);
	pthread_join(passage_pthread,NULL);
	
	return 0;
}
