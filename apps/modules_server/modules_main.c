#include "common.h"
#include "modules_server.h"

int main(int argc,char **argv)
{
	pthread_t uart_pthread;
	pthread_t passage_pthread;
#ifdef B6
	pthread_t handle_pthread;
	pthread_t factory_test_pthread;
	pthread_t ota_pthread;
#endif
	init_env();

#ifdef B6	
	pthread_create(&factory_test_pthread,NULL,(void*)factory_test_func,NULL);
#endif	
	pthread_create(&uart_pthread,NULL,(void*)uart_loop_recv,NULL);
#ifdef B6
	pthread_create(&handle_pthread,NULL,(void*)loop_recv,NULL);
#endif
	pthread_create(&passage_pthread,NULL,(void*)passage_thread_func,NULL);
#ifdef B6
	pthread_create(&ota_pthread,NULL,(void*)ota_thread_func,NULL);
	
	led_control();
#endif
	pthread_join(uart_pthread,NULL);
#ifdef B6
	pthread_join(handle_pthread,NULL);
#endif
	pthread_join(passage_pthread,NULL);
#ifdef B6
	pthread_join(ota_pthread,NULL);
	pthread_join(factory_test_pthread,NULL);
#endif
	return 0;
}
