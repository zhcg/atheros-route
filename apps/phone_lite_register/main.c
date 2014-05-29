#include "common.h"
#include "register.h"

int main(int argc,char **argv)
{
	if(argc!=1)
	{
		exit(-1);
	}
	PRINT("%s %s\n",__DATE__,__TIME__);
	PRINT("Author: %s\n",AUTHOR);
	
	pthread_t socket_pthread;
	pthread_t accept_pthread;
	pthread_t handle_pthread;
	
	init();
	
	pthread_create(&socket_pthread,NULL,(void*)loop_handle,NULL);
	pthread_create(&socket_pthread,NULL,(void*)tcp_loop_recv,NULL);
	pthread_create(&accept_pthread,NULL,(void*)sock_loop_accept,NULL);

	pthread_join(handle_pthread,NULL);
	pthread_join(accept_pthread,NULL);
	pthread_join(socket_pthread,NULL);

	return 0;
}
