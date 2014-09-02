#include "common.h"
#include "phone_control.h"
#include "phone_audio.h"


int main(int argc,char **argv)
{
	if(argc!=1)
	{
		exit(-1);
	}
#ifdef B6
	PRINT("Version:B6\n");
#elif defined(B6L)
	PRINT("Version:B6L\n");
#elif defined(S1)
	PRINT("Version:S1\n");
#elif defined(S1_F3A)
	PRINT("Version:S1_F3A\n");
#else
	PRINT("Version:NULL\n");
#endif

	PRINT("%s %s\n",__DATE__,__TIME__);
	PRINT("Author: %s\n",AUTHOR);
	
	pthread_t phone_control_pthread,check_ring_pthread;
	pthread_t phone_audio_pthread,check_tick_pthread;
	pthread_t link_manage_pthread,tcp_recv_pthread;
	pthread_t handle_down_msg_pthread;
	pthread_t handle_up_msg_pthread;
	pthread_t audiosend_pthread;
	pthread_t audiorecv_pthread;
	pthread_t audio_readwrite_pthread;
	pthread_t audio_echo_pthread;
	pthread_t audio_incoming_pthread;
#ifdef B6
	pthread_t passage_pthread;
#else
	pthread_t led_pthread;
#endif


	init_control();

	//base -> pad
	pthread_create(&handle_up_msg_pthread,NULL,(void*)handle_up_msg,NULL);

	//ring
	pthread_create(&check_ring_pthread,NULL,(void*)loop_check_ring,NULL);
	//tick
	pthread_create(&check_tick_pthread,NULL,(void*)phone_check_tick,NULL);
	//link manage
	pthread_create(&link_manage_pthread,NULL,(void*)phone_link_manage,NULL);

	//audio read write recv send
	pthread_create(&audio_readwrite_pthread,NULL,(void*)AudioReadWriteThreadCallBack,NULL);
	pthread_create(&audiosend_pthread,NULL,(void*)AudioSendThreadCallBack,NULL);
	pthread_create(&audiorecv_pthread,NULL,(void*)AudioRecvThreadCallBack,NULL);
	pthread_create(&audio_echo_pthread,NULL,(void*)AudioEchoThreadCallBack,NULL);
	
	//audio accept
	pthread_create(&phone_audio_pthread,NULL,(void*)audio_loop_accept,NULL);
	
	//pad -> base
	pthread_create(&handle_down_msg_pthread,NULL,(void*)handle_down_msg,NULL);

	//control recv
	pthread_create(&tcp_recv_pthread,NULL,(void*)tcp_loop_recv,NULL);

	//incoming
	pthread_create(&audio_incoming_pthread,NULL,(void*)AudioIncomingThreadCallBack,NULL);

	//control accept
	pthread_create(&phone_control_pthread,NULL,(void*)phone_control_loop_accept,NULL);

#ifdef B6
	//passage
	pthread_create(&passage_pthread,NULL,(void*)passage_pthread_func,NULL);
	
	pthread_join(passage_pthread,NULL);
#else
	//led
	pthread_create(&led_pthread,NULL,(void*)led_control_pthread_func,NULL);
	
	pthread_join(led_pthread,NULL);
#endif
	pthread_join(audio_incoming_pthread,NULL);
	pthread_join(handle_down_msg_pthread,NULL);
	pthread_join(handle_up_msg_pthread,NULL);
	pthread_join(phone_control_pthread,NULL);
	pthread_join(check_ring_pthread,NULL);
	pthread_join(phone_audio_pthread,NULL);
	pthread_join(check_tick_pthread,NULL);
	pthread_join(link_manage_pthread,NULL);
	pthread_join(tcp_recv_pthread,NULL);
	pthread_join(audiosend_pthread,NULL);
	pthread_join(audiorecv_pthread,NULL);
	pthread_join(audio_readwrite_pthread,NULL);
	pthread_join(audio_echo_pthread,NULL);

	return 0;
}
