#include "common.h"
#include "phone_control.h"
#include "phone_action.h"
#include "phone_audio.h"
#include "dtmf_data.h"
#include "si32178.h"

int pcm_ret = 0;

//计算校验
unsigned char sumxor(const  char  *arr, int len)
{
	int i=0;
	unsigned char sum = 0;
	for(i=0; i<len; i++)
	{
		sum ^= arr[i];
	}

	return sum;
}

int senddtmf(char dtmf)
{
	//int tmp;
	phone_audio.start_dtmf = dtmf;
	/*
	dtmf_ret = GenerateCodePcmData(&dtmf,1,dtmf_buf,Big_Endian);
	//total_dtmf_ret += dtmf_ret;
	if(total_dtmf_ret <= 0)
		phone_audio.input_stream_wp = phone_audio.input_stream_rp = 0;
	total_dtmf_ret = dtmf_ret+32000;
	//if(total_dtmf_ret < 16000)
	//{
		//total_dtmf_ret = 16000-total_dtmf_ret+dtmf_ret;
	//}
	//else
		//total_dtmf_ret += dtmf_ret;
	tmp = AUDIO_STREAM_BUFFER_SIZE-phone_audio.input_stream_wp;
	if(tmp < dtmf_ret)
	{
		memcpy(&input_stream_buffer[phone_audio.input_stream_wp],dtmf_buf,tmp);
		phone_audio.input_stream_wp = 0;
		memcpy(&input_stream_buffer[phone_audio.input_stream_wp],dtmf_buf+tmp,dtmf_ret-tmp);
		phone_audio.input_stream_wp +=(dtmf_ret-tmp);
	}
	else
	{
		memcpy(&input_stream_buffer[phone_audio.input_stream_wp],dtmf_buf,dtmf_ret);
		phone_audio.input_stream_wp += dtmf_ret;
	}
	*/
}

int onhook()
{
	if(!set_onhook())
	{
		//此处在摘机时也会调用
		phone_audio.start_recv = 1;
		phone_audio.dialup = 0;
		PRINT("onhook success\n");
	}
}

int offhook()
{
	if(!set_offhook())
	{
		PRINT("offhook success\n");
	}
}

//拨号
int dialup(char *num,int num_len)
{
	if(num_len>40)
		num_len = 40;
	phone_audio.input_stream_rp = 0;
	phone_audio.input_stream_wp = 0;

	pcm_ret = GenerateCodePcmData(num,num_len,&input_stream_buffer[phone_audio.input_stream_wp],Big_Endian);
	PRINT("ret = %d\n",pcm_ret);
	phone_audio.input_stream_wp += pcm_ret;
	if(phone_audio.input_stream_wp >= AUDIO_STREAM_BUFFER_SIZE)
		phone_audio.input_stream_wp = 0;
	phone_audio.dialup = 1;
	return 0;
}

