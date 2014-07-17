#include "phone_audio.h"
#include "common.h"
//#include "phone_action.h"
#include "fsk_external.h"

struct class_phone_audio phone_audio =
{
	.init_audio=init_audio, //音频socket初始化
	.phone_audio_sockfd=-1,//音频server句柄
	.phone_audio_pcmfd=-1,//音频设备
	.input_stream_wp=0,
	.input_stream_rp=0,
	.output_stream_wp=0,
	.output_stream_rp=0,
	.audio_reconnect_loops=0,
	.audio_reconnect_flag=0,
	.audio_send_thread_exit_flag=0,
	.audio_recv_thread_exit_flag=0,
	.audio_read_write_thread_exit_flag=0,
	.audio_send_thread_flag=0,
	.audio_recv_thread_flag=0,
	.audio_read_write_thread_flag=0,
	.audio_incoming_thread_flag=0,
	.audio_talkbacked_recv_send_thread_flag=0,
	.audio_talkback_recv_thread_flag=0,
	.audio_talkback_send_thread_flag=0,
	.start_recv=0,
	.get_code=0,
	.get_code_timeout=0,
	.get_code_timeout_times=0,
	.start_dtmf=0,
	.st = NULL,
	.dn = NULL,
	.den = NULL,
	.aecmInst = NULL,
};

unsigned char input_stream_buffer[AUDIO_STREAM_BUFFER_SIZE];
unsigned char output_stream_buffer[AUDIO_STREAM_BUFFER_SIZE];
unsigned char echo_stream_buffer[AUDIO_STREAM_BUFFER_SIZE];
unsigned char audio_sendbuf[BUFFER_SIZE_2K];
unsigned char audio_recvbuf[BUFFER_SIZE_2K*5];
unsigned char audio_tb_sendbuf[BUFFER_SIZE_2K];
unsigned char audio_tb_recvbuf[BUFFER_SIZE_2K*5];
int dtmf_ret = 0;
int total_dtmf_ret = 0;
unsigned char dtmf_buf[6000]={0};	
#ifdef SAVE_FILE
FILE* read_file;
char read_file_buf[1024*1024*2];
int global_read_buf_pos = 0;
int global_write_buf_pos = 0;
char write_file_buf[1024*1024*2];
#endif
#ifdef SAVE_OUT_DATE
char global_out_buf[1024*1024*2];
int global_out_buf_pos;
FILE* write_file;
FILE* out_file;
#endif
#ifdef SAVE_INCOMING_FILE
FILE* incoming_file;
#endif

int sampleRate = 8000;
const signed short kFilterCoefficients8kHz[5] =
    {3798, -7596, 3798, 7807, -3733};

const signed short kFilterCoefficients[5] =
    {4012, -8024, 4012, 8002, -3913};
    
int HighPassFilter_Initialize(HighPassFilterState* hpf, int sample_rate) {
	assert(hpf != NULL);

	if (sample_rate == 8000) {
		hpf->ba = kFilterCoefficients8kHz;
	} else {
		hpf->ba = kFilterCoefficients;
	}

	WebRtcSpl_MemSetW16(hpf->x, 0, 2);
	WebRtcSpl_MemSetW16(hpf->y, 0, 4);
	PRINT("HighPassFilter_Initialize OK\r\n");		

  return 0;
}

int HighPassFilter_Process(HighPassFilterState* hpf, signed short* data, int length) {
	assert(hpf != NULL);

	int i;
	signed int tmp_int32 = 0;
	signed short* y = hpf->y;
	signed short* x = hpf->x;
	const signed short* ba = hpf->ba;

	for (i = 0; i < length; i++) 
	{
		//  y[i] = b[0] * x[i] + b[1] * x[i-1] + b[2] * x[i-2]
		//         + -a[1] * y[i-1] + -a[2] * y[i-2];

		tmp_int32 = WEBRTC_SPL_MUL_16_16(y[1], ba[3]); // -a[1] * y[i-1] (low part)
		tmp_int32 += WEBRTC_SPL_MUL_16_16(y[3], ba[4]); // -a[2] * y[i-2] (low part)
		tmp_int32 = (tmp_int32 >> 15);
		tmp_int32 += WEBRTC_SPL_MUL_16_16(y[0], ba[3]); // -a[1] * y[i-1] (high part)
		tmp_int32 += WEBRTC_SPL_MUL_16_16(y[2], ba[4]); // -a[2] * y[i-2] (high part)
		tmp_int32 = (tmp_int32 << 1);

		tmp_int32 += WEBRTC_SPL_MUL_16_16(data[i], ba[0]); // b[0]*x[0]
		tmp_int32 += WEBRTC_SPL_MUL_16_16(x[0], ba[1]);    // b[1]*x[i-1]
		tmp_int32 += WEBRTC_SPL_MUL_16_16(x[1], ba[2]);    // b[2]*x[i-2]

		// Update state (input part)
		x[1] = x[0];
		x[0] = data[i];

		// Update state (filtered part)
		y[2] = y[0];
		y[3] = y[1];
		y[0] = (signed short)(tmp_int32 >> 13);
		y[1] = (signed short)((tmp_int32 - WEBRTC_SPL_LSHIFT_W32((signed int)(y[0]), 13)) << 2);

		// Rounding in Q12, i.e. add 2^11
		tmp_int32 += 2048;

		// Saturate (to 2^27) so that the HP filtered signal does not overflow
		tmp_int32 = WEBRTC_SPL_SAT((signed int)(134217727), tmp_int32, (signed int)(-134217728));

		// Convert back to Q0 and use rounding
		data[i] = (signed short)WEBRTC_SPL_RSHIFT_W32(tmp_int32, 12);

	}

	return 0;
}
int AudioAecmInit(void)
{
	int ret = 0;
	if (ret = WebRtcAecm_Create(&phone_audio.aecmInst) < 0) 
	{
		PRINT("WebRtcAecm_Create failed,return=%d\r\n",ret);
		return -1;
	}
	if (ret = WebRtcAecm_Init(phone_audio.aecmInst, sampleRate) < 0) 
	{
		PRINT("WebRtcAecm_Init failed,return=%d\r\n",ret);
		WebRtcAecm_Free(phone_audio.aecmInst);
		return -2;
	}	
	AecmConfig aecm_config;
	aecm_config.cngMode = 0;
	aecm_config.echoMode = 0;
	WebRtcAecm_set_config(phone_audio.aecmInst,aecm_config);
	PRINT("AudioAecmInit OK\r\n");		
	return 0;
}

int AudioDo(void *precord,int of1,void *pplayback,int of2,int bytes)
{
	signed short *pprecord,*ppplayback;
	int dots;
	int i = 0;
	int rtn  = 0;
	unsigned char  is_saturated = 0;
	unsigned char *pp;
	
	if(bytes <=0 || bytes%FRAME_BYTES != 0)
		return -7;
	dots = bytes / 2;
	pp = (unsigned char *)(precord);
	pp += of1;
	pprecord = (signed short *)(pp);
	
	pp = (unsigned char *)(pplayback);
	pp += of2;
	ppplayback = (signed short *)(pp);	
	do
	{
		if(phone_audio.audio_echo_thread_flag == 0)
			break;
		if (WebRtcAecm_BufferFarend(phone_audio.aecmInst, ppplayback, FRAME_DOTS)!=0)
		{
			printf("WebRtcAecm_BufferFarend() failed.\n");
			rtn = -3;
			break;
		}
		if (WebRtcAecm_Process(phone_audio.aecmInst, pprecord, NULL, 
					pprecord, FRAME_DOTS,0)!=0)
		{
			printf("WebRtcAecm_Process() failed.\n");
			rtn = -4;
			break;
		}
		i += FRAME_DOTS;
		pprecord += FRAME_DOTS;
		ppplayback += FRAME_DOTS;
		dots -= FRAME_DOTS;
	}while(dots > 0);  
	return rtn;                          
}

int AudioHpfInit(void)
{
	return HighPassFilter_Initialize(&phone_audio.hpf,sampleRate);
}

//开始检测来电
void start_read_incoming()
{
	phone_audio.audio_incoming_thread_flag = 1;
}

//结束检测来电
void stop_read_incoming()
{
	phone_audio.audio_incoming_thread_flag = 0;
}

//开启音频流
int startaudio(dev_status_t* devp,int flag)
{
	PRINT("%s\n",__FUNCTION__);
	char *bufp = devp->dev_mac;
	char tmp[3]={0};

	if(devp->audio_client_fd < 0)
	{
		return -1;
	}
	int i,ret,count=0;
	for(i=0;i<CLIENT_NUM;i++)
	{
		if(devlist[i].dev_is_using == 1)
			count++;
	}
	if(count==1)
	{
		PRINT("PSTN....\n");	
		if(flag == 0)
		{
			//ioctl(phone_audio.phone_audio_pcmfd,SLIC_INIT,0);
			if(phone_audio.phone_audio_pcmfd != -1)
			{
				close(phone_audio.phone_audio_pcmfd);
				phone_audio.phone_audio_pcmfd = -1;
			}
			usleep(100*1000);
			if(phone_audio.phone_audio_pcmfd == -1)
			{
				int pcm_fd=open(PCMNAME,O_RDWR);
				if(pcm_fd<0)
				{
					perror("open pcm fail,again!\n");
					pcm_fd=open(PCMNAME,O_RDWR);
					if(pcm_fd<0)
						return -1;
				}
				PRINT("audio open_success\n");

				phone_audio.phone_audio_pcmfd = pcm_fd;
			}
			if(phone_audio.st != NULL)
			{
				speex_echo_state_destroy(phone_audio.st);
			}
			if(phone_audio.den != NULL)	
			{		
				speex_preprocess_state_destroy(phone_audio.den);
			}
			if(phone_audio.dn != NULL)	
			{		
				speex_preprocess_state_destroy(phone_audio.dn);
			}
			phone_audio.st = speex_echo_state_init(NN, TAIL);
			phone_audio.den = speex_preprocess_state_init(NN, sampleRate);			
			phone_audio.dn = speex_preprocess_state_init(NN, sampleRate);
			AudioHpfInit();
			if(AudioAecmInit()!=0)
				return -2;
			speex_echo_ctl(phone_audio.st, SPEEX_ECHO_SET_SAMPLING_RATE, &sampleRate);
			speex_preprocess_ctl(phone_audio.den, SPEEX_PREPROCESS_SET_ECHO_STATE, phone_audio.st);
			int noiseSuppress = -90;//70
			speex_preprocess_ctl(phone_audio.den, SPEEX_PREPROCESS_SET_ECHO_SUPPRESS, &noiseSuppress);			
			noiseSuppress = -40;//40
			speex_preprocess_ctl(phone_audio.den, SPEEX_PREPROCESS_SET_ECHO_SUPPRESS_ACTIVE, &noiseSuppress);			
			noiseSuppress = -90;//90
			speex_preprocess_ctl(phone_audio.den, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &noiseSuppress);			
			i = 1;
			speex_preprocess_ctl(phone_audio.dn, SPEEX_PREPROCESS_SET_DENOISE, &i);
#ifdef SAVE_FILE
			read_file = fopen("./record_file.pcm", "w"); //for read sound
			write_file = fopen("./play_file.pcm", "w"); //for read sound
#endif
#ifdef SAVE_OUT_DATE
			out_file = fopen("./out.pcm", "w"); //for read sound
#endif
			//usleep(200*1000);
		}
		else
			ioctl(phone_audio.phone_audio_pcmfd,SLIC_INIT,0);
			
	    if(devp->desencrypt_enable == 1)
	    {
			memset(phone_audio.key,0,8);
			for(i=0;i<6;i++)
			{
				memcpy(tmp,bufp,2);
				tmp[2]='\0';
				//printf("%s\n",tmp);
				phone_audio.key[i] = (char)strtoul(tmp,NULL,16);
				bufp += 2;
			}
			phone_audio.key[6] = phone_audio.key[7] = 0x20;
			//PRINT("Key: ");
			//ComFunPrintfBuffer(phone_audio.key,8);
		}
		phone_audio.audio_talkback_recv_thread_flag = 0;
		phone_audio.audio_talkbacked_recv_send_thread_flag = 0;
		phone_audio.audio_talkback_send_thread_flag = 0;
		phone_audio.audio_read_write_thread_flag = 0;
		phone_audio.audio_echo_thread_flag = 0;
		phone_audio.audio_send_thread_flag = 0;
		phone_audio.audio_recv_thread_flag = 0;
		memset(output_stream_buffer,0,AUDIO_STREAM_BUFFER_SIZE);
		memset(input_stream_buffer,0,AUDIO_STREAM_BUFFER_SIZE);
		usleep(180*1000);
		phone_audio.audio_send_thread_flag = 1;
		phone_audio.audio_recv_thread_flag = 1;
		phone_audio.audio_read_write_thread_flag = 1;
		phone_audio.audio_echo_thread_flag = 1;
		return 0;
	}
	if(devp->talkbacked)
	{
		PRINT("TALKBACK...\n");
		phone_audio.audio_talkback_recv_thread_flag = 1;
		phone_audio.audio_talkbacked_recv_send_thread_flag = 1;
		phone_audio.audio_talkback_send_thread_flag = 1;
	}
}
//停止音频流
void stopaudio(dev_status_t* devp,int flag,int reconnect_flag)
{
	int i,ret,count=0;
	for(i=0;i<CLIENT_NUM;i++)
	{
		if(devlist[i].dev_is_using == 1)
			count++;
	}
	//回收socket

	if(devp->audio_client_fd != -1)
	{
		close(devp->audio_client_fd);
		devp->audio_client_fd = -1;
	}
	if(devp->talkbacked == 1 || devp->talkbacking == 1)
	{
		for(i=0;i<CLIENT_NUM;i++)
		{
			if(devlist[i].client_fd == -1)
				continue;
			if(devlist[i].talkbacked == 1 || devlist[i].talkbacking == 1)
			{
				PRINT("%s\taudio_client_fd = %d\n",devlist[i].client_ip,devlist[i].audio_client_fd);
				if(devlist[i].audio_client_fd != -1)
				{
					close(devlist[i].audio_client_fd);
					devlist[i].audio_client_fd = -1;
				}
			}
		}

	}
	if(count<=1)
	{
		int loops = 0;
		phone_audio.audio_send_thread_exit_flag = 0;
		phone_audio.audio_recv_thread_exit_flag = 0;
		phone_audio.audio_read_write_thread_exit_flag = 0;
		if(flag == PSTN)
		{
			if(phone_audio.audio_send_thread_flag || phone_audio.audio_recv_thread_flag || phone_audio.audio_read_write_thread_flag || phone_audio.audio_echo_thread_flag)
			{
				phone_audio.audio_send_thread_flag = 0;
				phone_audio.audio_recv_thread_flag = 0;
				phone_audio.audio_read_write_thread_flag = 0;
				phone_audio.audio_echo_thread_flag = 0;
			}
		}
		if(flag == TALKBACK)
		{
			if(phone_audio.audio_talkback_recv_thread_flag || phone_audio.audio_talkback_send_thread_flag || phone_audio.audio_talkbacked_recv_send_thread_flag )
			{
				phone_audio.audio_talkback_recv_thread_flag = 0;
				phone_audio.audio_talkbacked_recv_send_thread_flag = 0;
				phone_audio.audio_talkback_send_thread_flag = 0;
			}
		}
		do
		{
			usleep(5* 1000);
			loops++;
			if(loops > 30)//150ms等待
				break;
		}while((phone_audio.audio_recv_thread_exit_flag != 1) || (phone_audio.audio_read_write_thread_exit_flag != 1) || (phone_audio.audio_send_thread_exit_flag != 1) || (phone_audio.audio_echo_thread_exit_flag != 1));
		if(loops == 100)//超时
		{
			 //有线程未响应，也许是之前就退出了也许是未关闭，不过不要紧
		}
		//关闭声卡
		if(reconnect_flag == 0 && flag == PSTN)
		{
			PRINT("speex release\n");
			usleep(100*1000);
			pthread_mutex_lock(&phone_audio.aec_mutex);
			if(phone_audio.st != NULL)
			{
				speex_echo_state_destroy(phone_audio.st);
				phone_audio.st = NULL;
			}
			if(phone_audio.den != NULL)			
			{
				speex_preprocess_state_destroy(phone_audio.den);
				phone_audio.den = NULL;
			}
			if(phone_audio.dn != NULL)			
			{
				speex_preprocess_state_destroy(phone_audio.dn);
				phone_audio.dn = NULL;
			}
			if(phone_audio.aecmInst != NULL)
			{
				WebRtcAecm_Free(phone_audio.aecmInst);
				phone_audio.aecmInst = NULL;
			}
			pthread_mutex_unlock(&phone_audio.aec_mutex);
		}
		ioctl(phone_audio.phone_audio_pcmfd,SLIC_RELEASE,0);
		total_dtmf_ret = 0;

	}
	PRINT("stop audio success\n");

}
//初始化音频通道
int init_audio(void)
{
	int sockfd,on;
	struct sockaddr_in servaddr,cliaddr;
	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd<=0)
	{
		perror("create socket fail\n");
		exit(-1);
	}
	PRINT("audio socket success\r\n");

	on = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	int input = 10240;
	int output= 10240;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &input, sizeof(input));
	setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &output, sizeof(output));

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(AUDIO_PORT);

	if(bind(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr)) == -1)
	{
		perror("bind error\n");
		exit(-1);
	}
	PRINT("audio bind success\n");

	if(listen(sockfd,10)<0)
	{
		perror("listen error\n");
		exit(-1);
	}
	PRINT("audio listen success\n");

	if(fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1) {
		perror("fcntl error");
		exit(-1);
	}

	PRINT("noblock success\n");

	phone_audio.phone_audio_sockfd = sockfd;

	if(phone_audio.phone_audio_pcmfd == -1 )
	{
		int pcm_fd=open(PCMNAME,O_RDWR);
		if(pcm_fd<0)
		{
			perror("open pcm fail,again!\n");
			pcm_fd=open(PCMNAME,O_RDWR);
			if(pcm_fd<0)
				return -1;
		}
		PRINT("audio open_success\n");

		phone_audio.phone_audio_pcmfd = pcm_fd;
	}
    pthread_mutex_init(&phone_audio.aec_mutex, NULL);
#ifdef SAVE_INCOMING_FILE
	incoming_file = fopen("./incoming_date.pcm", "w"); //for read sound
#endif
	return 0;
}

int UlawEncode(unsigned char *pout,int offset1,unsigned char *pin,int offset2,int mSampleCount)
{
	signed short *pcm;
	unsigned char *ppout;
	unsigned char *ppin;
	int i;
	int exponent = 7;
	int expMask;
	int mantissa;
	int sign;

	ppout = pout + offset1;
	ppin = pin + offset2;
	pcm = (signed short *)(ppin);
	for(i = 0;i < mSampleCount;i++)
	{
		sign = (pcm[i] & 0x8000) >> 8;
		if (sign != 0)
			pcm[i] = -pcm[i];
		if (pcm[i] > CODE_MAX)
			pcm[i] = CODE_MAX;
		exponent = 7;
		for (expMask = 0x4000; (pcm[i] & expMask) == 0   && exponent > 0; exponent--, expMask >>= 1)
		{
			;
		}
		mantissa = (pcm[i] >> ((exponent == 0) ? 4 : (exponent + 3))) & 0x0f;
		ppout[i] = (unsigned char)(sign | exponent << 4 | mantissa);
		ppout[i] ^= 0xD5;
	}
	return mSampleCount;
}

int UlawDecode(unsigned char *pout,int offset1,unsigned char *pin,int offset2,int inbytes)
{
	signed short *pcm;
	unsigned char *ppout;
	unsigned char *ppin;
	int sign;
	int exponent;
	int data;
	int i;
	ppout = pout +offset1;
	pcm = (signed short *)(ppout);
	ppin = pin + offset2;
	for(i = 0;i < inbytes;i++)
	{
		pin[i] ^= 0xD5;
		sign = pin[i] & 0x80;
		exponent = (pin[i] & 0x70) >> 4;
		data = pin[i] & 0x0f;
		data <<= 4;
		data += 8;
		if (exponent != 0)
			data += 0x100;
		if (exponent > 1)
			data <<= (exponent - 1);
		if(sign == 0)
			pcm[i] = data;
		else
			pcm[i] = -data;
	}
	return inbytes;
}

int Encrypt(unsigned char *data,int len)
{
	int i;
	unsigned char *pdata = data;
	unsigned char tmp_low = 0;
	unsigned char tmp_high = 0;
	for(i=0;i<len;i++)
	{
		pdata[i] = ~pdata[i];
		tmp_low = pdata[i] & 0x0f;
		tmp_high = pdata[i] & 0xf0;
		pdata[i] = (tmp_low << 4) + (tmp_high >> 4);
	}
}

int Decrypt(unsigned char *data,int len)
{
	int i;
	unsigned char *pdata = data;
	unsigned char tmp_low = 0;
	unsigned char tmp_high = 0;
	for(i=0;i<len;i++)
	{
		tmp_low = pdata[i] & 0x0f;
		tmp_high = pdata[i] & 0xf0;
		pdata[i] = (tmp_low << 4) + (tmp_high >> 4);
		pdata[i] = ~pdata[i];
	}
}

int UlawEncodeEncrypt(unsigned char *pout,int offset1,unsigned char *pin,int offset2,int mSampleCount)
{
	signed short *pcm;
	unsigned char *ppout;
	unsigned char *ppin;
	int i;
	int exponent = 7;
	int expMask;
	int mantissa;
	int sign;	
	
	ppout = pout + offset1;
	ppin = pin + offset2;
	pcm = (signed short *)(ppin);
	for(i = 0;i < mSampleCount;i++)
	{
		sign = (pcm[i] & 0x8000) >> 8;
		if (sign != 0)
			pcm[i] = -pcm[i];
		if (pcm[i] > CODE_MAX)
			pcm[i] = CODE_MAX;
		exponent = 7;
		for (expMask = 0x4000; (pcm[i] & expMask) == 0   && exponent > 0; exponent--, expMask >>= 1)
		{
			;
		}
		mantissa = (pcm[i] >> ((exponent == 0) ? 4 : (exponent + 3))) & 0x0f;
		ppout[i] = (unsigned char)(sign | exponent << 4 | mantissa);
		ppout[i] ^= 0xD5;
	}
	
	Encrypt(pout,mSampleCount*2);
	
	return mSampleCount;
}
int UlawEncodeDesEncrypt(unsigned char *pout,int offset1,unsigned char *pin,int offset2,int mSampleCount)
{
	signed short *pcm;
	unsigned char *ppout;
	unsigned char *ppin;
	int i;
	int exponent = 7;
	int expMask;
	int mantissa;
	int sign;	
	unsigned char pout_tmp[AUDIO_SEND_PACKET_SIZE]={0};
	
	ppout = pout_tmp + offset1;
	ppin = pin + offset2;
	pcm = (signed short *)(ppin);
	for(i = 0;i < mSampleCount;i++)
	{
		sign = (pcm[i] & 0x8000) >> 8;
		if (sign != 0)
			pcm[i] = -pcm[i];
		if (pcm[i] > CODE_MAX)
			pcm[i] = CODE_MAX;
		exponent = 7;
		for (expMask = 0x4000; (pcm[i] & expMask) == 0   && exponent > 0; exponent--, expMask >>= 1)
		{
			;
		}
		mantissa = (pcm[i] >> ((exponent == 0) ? 4 : (exponent + 3))) & 0x0f;
		ppout[i] = (unsigned char)(sign | exponent << 4 | mantissa);
		ppout[i] ^= 0xD5;
	}
	
	DesEcb(pout_tmp,mSampleCount,pout,phone_audio.key,0);
	
	return mSampleCount;
}

int UlawDecodeDesEncrypt(unsigned char *pout,int offset1,unsigned char *pin,int offset2,int inbytes)
{
	signed short *pcm;
	unsigned char *ppout;
	unsigned char *ppin;
	int sign;
	int exponent;
	int data;
	int i;
	unsigned char *pin_tmp=malloc(inbytes);
	
	DesEcb(pin,inbytes,pin_tmp,phone_audio.key,1);
	
	ppout = pout +offset1;
	pcm = (signed short *)(ppout);
	ppin = pin_tmp + offset2;
	for(i = 0;i < inbytes;i++)
	{
		pin_tmp[i] ^= 0xD5;
		sign = pin_tmp[i] & 0x80;
		exponent = (pin_tmp[i] & 0x70) >> 4;
		data = pin_tmp[i] & 0x0f;
		data <<= 4;
		data += 8;
		if (exponent != 0)
			data += 0x100;
		if (exponent > 1)
			data <<= (exponent - 1);
		if(sign == 0)
			pcm[i] = data;
		else
			pcm[i] = -data;
	}
	free(pin_tmp);
	return inbytes;
}

int UlawDecodeEncrypt(unsigned char *pout,int offset1,unsigned char *pin,int offset2,int inbytes)
{
	signed short *pcm;
	unsigned char *ppout;
	unsigned char *ppin;
	int sign;
	int exponent;
	int data;
	int i;
	
	Decrypt(pin,inbytes*2);
	
	ppout = pout +offset1;
	pcm = (signed short *)(ppout);
	ppin = pin + offset2;
	for(i = 0;i < inbytes;i++)
	{
		pin[i] ^= 0xD5;
		sign = pin[i] & 0x80;
		exponent = (pin[i] & 0x70) >> 4;
		data = pin[i] & 0x0f;
		data <<= 4;
		data += 8;
		if (exponent != 0)
			data += 0x100;
		if (exponent > 1)
			data <<= (exponent - 1);
		if(sign == 0)
			pcm[i] = data;
		else
			pcm[i] = -data;
	}
	return inbytes;
}

void* AudioIncomingThreadCallBack(void* argv)
{
	unsigned char audioincoming_buf[AUDIO_READ_BYTE_SIZE*2] = {0};
	int read_ret;

	phone_audio.audio_read_write_thread_flag = 0;
	while(1)
	{
		while(phone_audio.audio_incoming_thread_flag)
		{
			read_ret = read(phone_audio.phone_audio_pcmfd,audioincoming_buf,AUDIO_READ_BYTE_SIZE);
			if(read_ret < 0)//或者小于0。则表示声卡异常
			{
				PRINT("error,when read from sound card,error code is %d\r\n",read_ret);
				close(phone_audio.phone_audio_pcmfd);
				usleep(50*1000);
				int pcm_fd=open(PCMNAME,O_RDWR);
				if(pcm_fd<0)
				{
					perror("open pcm fail,again!\n");
					pcm_fd=open(PCMNAME,O_RDWR);
				}
				PRINT("audio open_success\n");

				phone_audio.phone_audio_pcmfd = pcm_fd;
				break;
			}
			else if(read_ret > 0)
			//else
			{
#ifdef SAVE_INCOMING_FILE
				fwrite(audioincoming_buf, 1, read_ret , incoming_file);
#endif
				//PRINT("read_ret = %d\n",read_ret);
				if(Fsk_AddData((signed short *)audioincoming_buf,read_ret/2) == 0)
					PRINT("Fsk_AddData err\n");

				if(phone_control.get_fsk == 0)
				{
					if(DtmfDo((signed short *)audioincoming_buf,read_ret/2) == 2)
					{
						//PRINT("get dtmf num...\n");
						phone_audio.get_code = 1;
						//if(phone_control.ring_count == 0)  //没有响铃，则认为号码无效
						//{
							//PRINT("no ring,pass...\n");
							//DtmfGetCode(audioincoming_buf); //数据扔了不要
							//phone_audio.get_code = 0;
						//}
						phone_audio.get_code_timeout = 1;
					}
				}
			}
			else
			{
				usleep(10*1000);
				continue;
			}
#ifdef SAVE_INCOMING_FILE
			usleep(30*1000);
#else
			usleep(50*1000);
#endif
		}
		PRINT("audio incoming thread entry idle...\n");
		while(phone_audio.audio_incoming_thread_flag == 0)
		{
			usleep(50* 1000);
		}
		PRINT("audio incoming thread entry busy...\n");
		DtmfInit();
	}
	PRINT("AudioIncomingThreadCallBack exit....\n");
}

void *AudioReadWriteThreadCallBack(void *argv)
{
	int optval;
	int optlen = sizeof(int);
	int valid_bytes;
	int write_bytes;
	int send_ret;
	unsigned int write_ret;
	unsigned int total_write_bytes = 0;
	unsigned int total_send_bytes = 0;
	unsigned int total_read_bytes = 0;
	unsigned int total_recv_bytes = 0;
	int free_bytes=0;
	int read_ret,read_bytes,recv_ret;
	int print_loop = 0;
	int i = 0;
	unsigned int read_times = 0;
	int first_recv = 0;
	int real_data = 0;
	dev_status_t* devp;
	unsigned char read_buf[AUDIO_WRITE_BYTE_SIZE*2] = {0};
	unsigned char out_buf[AUDIO_WRITE_BYTE_SIZE] = {0};
	short *readp ;
	short *outp ;
	phone_audio.audio_read_write_thread_flag = 0;
	phone_audio.audio_talkbacked_recv_send_thread_flag = 0;
	while(1)
	{
		phone_audio.output_stream_wp = 0;
		phone_audio.output_stream_rp = 0;
		free_bytes=0;
		print_loop = 0;
		total_read_bytes = 0;
		total_recv_bytes = 0;
		phone_audio.audio_read_write_thread_exit_flag = 0;
		read_times = 0;
		phone_audio.input_stream_rp = 0;
		phone_audio.input_stream_wp = 0;
		total_write_bytes = 0;
		total_send_bytes = 0;
		first_recv = 0;
		real_data = 0;
		read_ret = 0;
		for(i=0;i<CLIENT_NUM;i++)
		{
			if(devlist[i].client_fd == -1)
				continue;
			if(devlist[i].dev_is_using == 1 || devlist[i].talkbacked == 1)
			{
				devp=&devlist[i];
			}
		}
		while(phone_audio.audio_read_write_thread_flag)
		{
START_WRITE:
			if(phone_audio.phone_audio_pcmfd == -1)
			{
				stopaudio(devp,PSTN,0);
				continue;
			}
			if(devp->audio_client_fd < 0)
				continue;
			//write!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			//获取input buffer的有效数据字节数
			if(phone_audio.input_stream_rp == AUDIO_STREAM_BUFFER_SIZE)
				phone_audio.input_stream_rp = 0;
			if(phone_audio.input_stream_wp >= phone_audio.input_stream_rp)
				valid_bytes	= phone_audio.input_stream_wp - phone_audio.input_stream_rp;
			else
			{
				valid_bytes	= AUDIO_STREAM_BUFFER_SIZE - phone_audio.input_stream_rp;
				PRINT("%d,end,back!!\n",valid_bytes);
			}

			//if((phone_control.start_dial == 1 || total_dtmf_ret > 0) && valid_bytes < AUDIO_WRITE_BYTE_SIZE && valid_bytes != 0)
			//{
				//PRINT("valid_bytes = %d\n",valid_bytes);
				//real_data = valid_bytes; //数据不够填充，保存真实数据长度
				//valid_bytes = AUDIO_WRITE_BYTE_SIZE;
			//}
			if(phone_control.start_dial == 1 || total_dtmf_ret > 0 ||  (valid_bytes >= AUDIO_WRITE_BYTE_SIZE) || phone_audio.input_stream_rp > phone_audio.input_stream_wp)//要求AUDIO_WRITE_BYTE_SIZE必须能被input_stream_buffer大小整除
			{
				if(phone_control.start_dial == 0 && total_dtmf_ret <= 0) //除了拨号和dtmf时，防止延迟
				{
					if(valid_bytes > (AUDIO_PACKET_LIMIT * AUDIO_WRITE_BYTE_SIZE))
					{
						//打印信息
						PRINT("%d;received data is more,so discard some one\r\n",valid_bytes);
						phone_audio.input_stream_rp += (valid_bytes / AUDIO_WRITE_BYTE_SIZE - 1) * AUDIO_WRITE_BYTE_SIZE;
						if(phone_audio.input_stream_rp > AUDIO_STREAM_BUFFER_SIZE)
							phone_audio.input_stream_rp = 0;
						total_write_bytes += (valid_bytes / AUDIO_WRITE_BYTE_SIZE - 1) * AUDIO_WRITE_BYTE_SIZE;;
					}
					else if(valid_bytes > ((AUDIO_PACKET_LIMIT-3) * AUDIO_WRITE_BYTE_SIZE))//2*1280
					{
						PRINT("LIMIT:%d\n",valid_bytes);
						phone_audio.input_stream_rp += (AUDIO_WRITE_BYTE_SIZE/10);
						if(phone_audio.input_stream_rp > AUDIO_STREAM_BUFFER_SIZE)
							phone_audio.input_stream_rp = 0;
						phone_audio.input_stream_wp -= (AUDIO_WRITE_BYTE_SIZE/10);
						if(phone_audio.input_stream_wp < 0)
							phone_audio.input_stream_wp = 0;
						total_write_bytes += (AUDIO_WRITE_BYTE_SIZE/10);
					}
				}
				if(valid_bytes > AUDIO_WRITE_BYTE_SIZE)
					valid_bytes = AUDIO_WRITE_BYTE_SIZE;

				write_ret = write(phone_audio.phone_audio_pcmfd,&input_stream_buffer[phone_audio.input_stream_rp],valid_bytes);
				if(write_ret < 0)
				{
					//错误处理
					PRINT("error,when write data to sound card,error code is %d\n",write_ret);
					break;
				}

				//PRINT("write_ret = %d\n",write_ret);
				phone_audio.input_stream_rp += write_ret;
				total_write_bytes += write_ret;
				if(phone_audio.input_stream_rp >= pcm_ret && phone_audio.input_stream_rp > 0 && phone_control.start_dial == 1)
				{
					PRINT("dialup over!!!!!\n");
					phone_control.start_dial = 0;
					phone_audio.input_stream_rp = 0;
					phone_audio.input_stream_wp = 0;
					pcm_ret = 0;
				}
				if(phone_audio.input_stream_rp >= AUDIO_STREAM_BUFFER_SIZE)
					phone_audio.input_stream_rp = 0;
			}
			
			//read!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!			
			read_ret = read(phone_audio.phone_audio_pcmfd,read_buf,AUDIO_READ_BYTE_SIZE);
			//PRINT("read_ret = %d\n",read_ret);
			if(read_ret < 0)//或者小于0。则表示声卡异常
			{
				PRINT("error,when read from sound card,error code is %d\r\n",read_ret);
				phone_audio.audio_read_write_thread_flag = 0;
				break;
			}
			else if(read_ret == AUDIO_READ_BYTE_SIZE)		
			{
				readp = (short *)(read_buf+AUDIO_READ_BYTE_SIZE);
				for(i=0;i<read_ret/2;i++)
				{
					*readp++ /= 3;
				}
#ifdef SAVE_FILE	
					memcpy(read_file_buf+global_read_buf_pos,read_buf,AUDIO_READ_BYTE_SIZE);
					global_read_buf_pos += AUDIO_READ_BYTE_SIZE;
					memcpy(write_file_buf+global_write_buf_pos,read_buf+AUDIO_READ_BYTE_SIZE,AUDIO_READ_BYTE_SIZE);
					global_write_buf_pos += AUDIO_READ_BYTE_SIZE;
					
#endif
				//获取output buffer的剩余空间
				free_bytes = AUDIO_STREAM_BUFFER_SIZE - phone_audio.echo_stream_wp;
				if(free_bytes < AUDIO_WRITE_BYTE_SIZE*2)
				{
					memcpy((void *)(&echo_stream_buffer[phone_audio.echo_stream_wp]),(void *)(read_buf),free_bytes);
					memcpy((void *)(echo_stream_buffer),(void *)(&read_buf[free_bytes]),AUDIO_WRITE_BYTE_SIZE*2 - free_bytes);
					phone_audio.echo_stream_wp = (AUDIO_WRITE_BYTE_SIZE*2- free_bytes);
				}
				else
				{
					memcpy((void *)(&echo_stream_buffer[phone_audio.echo_stream_wp]),(void *)(read_buf),AUDIO_WRITE_BYTE_SIZE*2);					
					phone_audio.echo_stream_wp += AUDIO_WRITE_BYTE_SIZE*2;
				}
				total_read_bytes += AUDIO_WRITE_BYTE_SIZE;
				if(phone_audio.echo_stream_wp >= AUDIO_STREAM_BUFFER_SIZE)
					phone_audio.echo_stream_wp = 0;
			}
			else if(read_ret == 0)
			{
				usleep(10* 1000);
				continue;
			}
			print_loop++;
			if(print_loop > 1500)//5秒打印一次
			{
				//打印信息。。。。
				PRINT("total_write_bytes is %d\r\n",total_write_bytes);
				PRINT("total_read_bytes is %d\r\n",total_read_bytes);
				print_loop = 0;
			}
#ifdef SAVE_FILE
			usleep(5* 1000);
#else
			usleep(60* 1000);
#endif
		}

		while(phone_audio.audio_talkbacked_recv_send_thread_flag == 1)
		{
			//recv!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			//获取input buffer的剩余空间
			if(phone_audio.output_stream_wp >= AUDIO_STREAM_BUFFER_SIZE)
				phone_audio.output_stream_wp = 0;
			free_bytes = AUDIO_STREAM_BUFFER_SIZE - phone_audio.output_stream_wp;
			if(devp->audio_client_fd < 0)
			{
				goto TBED_RECV_ERROR;
			}
			recv_ret = recv(devp->audio_client_fd,&output_stream_buffer[phone_audio.output_stream_wp],free_bytes,MSG_DONTWAIT);
			if(recv_ret < 1)//或者小于0。则表示socket异常，此处需要同时处理其他线程的错误
			{
				getsockopt(devp->audio_client_fd,SOL_SOCKET,SO_ERROR,&optval, &optlen);
				if(errno == EAGAIN && optval == 0)
				{
					//usleep(10*1000);
					goto START_SEND;
				}
TBED_RECV_ERROR:
				PRINT("error,when receive from socket,error code is %d\r\n",recv_ret);
				phone_audio.audio_talkback_recv_thread_flag = 0;
				phone_audio.audio_talkbacked_recv_send_thread_flag = 0;
				phone_audio.audio_talkback_send_thread_flag = 0;

				close(devp->audio_client_fd);
				devp->audio_client_fd = -1;

				break;

			}
			else
			{
				if(first_recv == 1)
				{
					phone_audio.output_stream_wp += recv_ret;
					total_recv_bytes += recv_ret;
					if(phone_audio.output_stream_wp >= AUDIO_STREAM_BUFFER_SIZE)
						phone_audio.output_stream_wp = 0;
				}
				first_recv = 1;
			}
START_SEND:
			//send!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			//读取位置已经到缓冲尾部，则从头开始
			if(phone_audio.input_stream_rp == AUDIO_STREAM_BUFFER_SIZE)
				phone_audio.input_stream_rp = 0;
			//获取缓冲中有效数据字节数
			if(phone_audio.input_stream_wp >= phone_audio.input_stream_rp)
				valid_bytes = phone_audio.input_stream_wp - phone_audio.input_stream_rp;
			else
				valid_bytes = AUDIO_STREAM_BUFFER_SIZE - phone_audio.input_stream_rp;
			//每次只允许发送最多AUDIO_SEND_PACKET_SIZE个字节
			if(valid_bytes > AUDIO_SEND_PACKET_SIZE)
				valid_bytes = AUDIO_SEND_PACKET_SIZE;
			if((valid_bytes >= AUDIO_SEND_PACKET_SIZE) || (phone_audio.input_stream_wp < phone_audio.input_stream_rp))
			{
				if(devp->audio_client_fd < 0)
				{
					goto TBED_SEND_ERROR;
				}
				send_ret = send(devp->audio_client_fd,&input_stream_buffer[phone_audio.input_stream_rp],valid_bytes,MSG_DONTWAIT);
				if(send_ret < 1)//或者小于0。则表示socket异常。发送线程中有错不处理其他，只需要处理自身就行
				{
					getsockopt(devp->audio_client_fd,SOL_SOCKET,SO_ERROR,&optval, &optlen);
					if(errno == EAGAIN && optval == 0)
					{
						usleep(15*1000);
						continue;
					}
TBED_SEND_ERROR:
					phone_audio.audio_talkbacked_recv_send_thread_flag = 0;
					PRINT("error,when send to socket,error code is %d",send_ret);
					break;
				}
				else
				{
					phone_audio.input_stream_rp += send_ret;
					total_send_bytes += send_ret;
					if(phone_audio.input_stream_rp >= AUDIO_STREAM_BUFFER_SIZE)
						phone_audio.input_stream_rp = 0;
				}
			}
			print_loop++;
			if(print_loop > 300)//5秒打印一次
			{
				//打印信息。。。。
				PRINT("total_recv_talkbacked_bytes is %d\r\n",total_recv_bytes);
				PRINT("total_send_talkbacked_bytes is %d\r\n",total_send_bytes);
				print_loop = 0;
			}
			usleep(15* 1000);

		}
		phone_audio.audio_read_write_thread_exit_flag = 1;
		PRINT("audio read write thread entry idle...\n");
		while(phone_audio.audio_read_write_thread_flag == 0 && phone_audio.audio_talkbacked_recv_send_thread_flag == 0)
		{
			usleep(30* 1000);
		}
		PRINT("audio read write thread entry busy...\n");
	}
	PRINT("AudioReadWriteThreadCallBack exit....\n");
}

void* AudioEchoThreadCallBack(void* argv)
{
	short * out_buf[AUDIO_WRITE_BYTE_SIZE*2]= {0};
	short *readp;
	short *out_bufp;
	int valid_bytes = 0;
	int free_bytes = 0;
	int ret,i = 0;
	int echo_cancellation = 0;
	while(1)
	{
		phone_audio.echo_stream_wp = 0;
		phone_audio.echo_stream_rp = 0;
		phone_audio.audio_echo_thread_exit_flag = 0;
		valid_bytes = 0;
		free_bytes = 0;
		echo_cancellation = 0;
		while(phone_audio.audio_echo_thread_flag == 1)
		{
			//读取位置已经到缓冲尾部，则从头开始
			if(phone_audio.echo_stream_rp == AUDIO_STREAM_BUFFER_SIZE)
				phone_audio.echo_stream_rp = 0;
			//获取缓冲中有效数据字节数
			if(phone_audio.echo_stream_wp >= phone_audio.echo_stream_rp)
				valid_bytes = phone_audio.echo_stream_wp - phone_audio.echo_stream_rp;
			else
			{
				valid_bytes = AUDIO_STREAM_BUFFER_SIZE - phone_audio.echo_stream_rp;
				//PRINT("***%d\n",phone_audio.echo_stream_rp-phone_audio.echo_stream_wp);
				PRINT("valid_bytes:%d\n",valid_bytes);			
			}
			//每次只允许发送最多AUDIO_SEND_PACKET_SIZE个字节
			if(valid_bytes > AUDIO_WRITE_BYTE_SIZE*2)
				valid_bytes = AUDIO_WRITE_BYTE_SIZE*2;
			if((valid_bytes >= AUDIO_WRITE_BYTE_SIZE*2) || (phone_audio.echo_stream_wp < phone_audio.echo_stream_rp))
			{
				//if(phone_control.dial_over == 1)
				//{
					//readp = (short*)&echo_stream_buffer[phone_audio.echo_stream_rp];
					//for(i=0;i<valid_bytes/4;i++)
					//{
						//*readp++ <<= 1;
					//}
				//}
				if(phone_audio.st == NULL || phone_audio.den == NULL || phone_audio.dn == NULL)
				{
					PRINT("speex init err\n");
					phone_audio.audio_echo_thread_flag = 0;
					break;
				}
				
				//if(phone_control.dial_over == 1)
				//{
					//if(echo_cancellation <= 5)
						//echo_cancellation ++;
				//}
				//if(echo_cancellation >= 0 && total_dtmf_ret <= 16000 )
				if(phone_control.dial_over == 1 && total_dtmf_ret <= 16000)
				{
					//if(echo_cancellation == 0)
						//PRINT("start aec\n");
					pthread_mutex_lock(&phone_audio.aec_mutex);
					HighPassFilter_Process(&phone_audio.hpf,(signed short *)out_buf,valid_bytes/4);
					speex_echo_cancellation(phone_audio.st,(spx_int16_t*)&echo_stream_buffer[phone_audio.echo_stream_rp], (spx_int16_t*)(&echo_stream_buffer[phone_audio.echo_stream_rp+AUDIO_WRITE_BYTE_SIZE]), (spx_int16_t*)out_buf);
					speex_preprocess_run(phone_audio.den, (spx_int16_t*)out_buf);
					//speex_preprocess_run(phone_audio.dn, (spx_int16_t*)out_buf);
					AudioDo(out_buf,0,&echo_stream_buffer[phone_audio.echo_stream_rp+AUDIO_WRITE_BYTE_SIZE],0,valid_bytes/2);
					pthread_mutex_unlock(&phone_audio.aec_mutex);
				}
				else
					memcpy(out_buf,&echo_stream_buffer[phone_audio.echo_stream_rp],AUDIO_WRITE_BYTE_SIZE);
				//readp = (short*)&echo_stream_buffer[phone_audio.echo_stream_rp+AUDIO_WRITE_BYTE_SIZE];
				//for(i=0;i<valid_bytes/4;i++)
				//{
					//*readp++ >>= 1;
				//}			
			
				out_bufp = (short*)out_buf;
#ifdef SAVE_OUT_DATE
				memcpy(global_out_buf+global_out_buf_pos,out_buf,AUDIO_WRITE_BYTE_SIZE);
				global_out_buf_pos += AUDIO_WRITE_BYTE_SIZE;
#endif
				//获取output buffer的剩余空间
				free_bytes = AUDIO_STREAM_BUFFER_SIZE - phone_audio.output_stream_wp;
				if(free_bytes < AUDIO_WRITE_BYTE_SIZE)
				{
					memcpy((void *)(&output_stream_buffer[phone_audio.output_stream_wp]),(void *)(out_bufp),free_bytes);
					memcpy((void *)(output_stream_buffer),(void *)(&out_bufp[free_bytes]),AUDIO_WRITE_BYTE_SIZE - free_bytes);
					phone_audio.echo_stream_rp = (AUDIO_WRITE_BYTE_SIZE - free_bytes);					
				}
				else
				{
					memcpy((void *)(&output_stream_buffer[phone_audio.output_stream_wp]),(void *)(out_bufp),AUDIO_WRITE_BYTE_SIZE);					
					phone_audio.echo_stream_rp += valid_bytes;					
				}
				phone_audio.output_stream_wp += AUDIO_WRITE_BYTE_SIZE;
				if(phone_audio.output_stream_wp >= AUDIO_STREAM_BUFFER_SIZE)
					phone_audio.output_stream_wp = 0;
				if(phone_audio.echo_stream_rp >= AUDIO_STREAM_BUFFER_SIZE)
					phone_audio.echo_stream_rp = 0;

			}
			else
			{
				usleep(5*1000);
				continue;
			}
			usleep(5*1000);
		}
		PRINT("audio echo thread entry idle...\n");
		phone_audio.audio_echo_thread_exit_flag = 1;
		while(phone_audio.audio_echo_thread_flag == 0)
		{
			usleep(30* 1000);
		}
		PRINT("audio echo thread entry busy...\n");
	}
	//打印线程退出信息
	PRINT("AudioEchoThreadCallBack exit....\n");
}

//发送数据到socket线程
void* AudioSendThreadCallBack(void* argv)
{
	PRINT("AudioSendThreadCallBack is running....\n");
	int optval;
	int optlen = sizeof(int);
	int valid_bytes;
	int send_ret;
	int print_loop = 0;
	unsigned int total_send_bytes = 0;
	int i = 0;
	dev_status_t* devp;
	int count = 0;
	
	phone_audio.audio_send_thread_flag = 0;
	phone_audio.audio_talkback_send_thread_flag = 0;
	while(1)
	{
		phone_audio.output_stream_wp = 0;
		phone_audio.output_stream_rp = 0;
		print_loop = 0;
		total_send_bytes = 0;
		phone_audio.audio_send_thread_exit_flag = 0;
		//打印线程创建信息
		for(i=0;i<CLIENT_NUM;i++)
		{
			if(devlist[i].client_fd == -1)
				continue;
			if(devlist[i].dev_is_using == 1 || devlist[i].talkbacking == 1)
			{
				devp=&devlist[i];
			}
		}
		while(phone_audio.audio_send_thread_flag == 1)
		{
			//读取位置已经到缓冲尾部，则从头开始
			if(phone_audio.output_stream_rp == AUDIO_STREAM_BUFFER_SIZE)
				phone_audio.output_stream_rp = 0;
			//获取缓冲中有效数据字节数
			if(phone_audio.output_stream_wp >= phone_audio.output_stream_rp)
				valid_bytes = phone_audio.output_stream_wp - phone_audio.output_stream_rp;
			else
				valid_bytes = AUDIO_STREAM_BUFFER_SIZE - phone_audio.output_stream_rp;
			//if(valid_bytes > 4*AUDIO_SEND_PACKET_SIZE)
			//{
				//PRINT("send data is more,so discard some one\r\n");
				//phone_audio.output_stream_wp = phone_audio.output_stream_rp;
				//valid_bytes = 0;
			//}
			//每次只允许发送最多AUDIO_SEND_PACKET_SIZE个字节
			if(valid_bytes > AUDIO_SEND_PACKET_SIZE)
				valid_bytes = AUDIO_SEND_PACKET_SIZE;
			if((valid_bytes >= AUDIO_SEND_PACKET_SIZE) || (phone_audio.output_stream_wp < phone_audio.output_stream_rp))
			{
				if(devp->encrypt_enable == 1)
					//编码
					UlawEncodeEncrypt(audio_sendbuf,0,&output_stream_buffer[phone_audio.output_stream_rp],0,valid_bytes/2);
				else if(devp->desencrypt_enable == 1)
				{
					if(valid_bytes < AUDIO_SEND_PACKET_SIZE)
						valid_bytes = valid_bytes - (valid_bytes%8);				
					//PRINT("valid_bytes = %d\n",valid_bytes);
					//编码
					UlawEncodeDesEncrypt(audio_sendbuf,0,&output_stream_buffer[phone_audio.output_stream_rp],0,valid_bytes/2);
				}
				else
				{
					//编码
					UlawEncode(audio_sendbuf,0,&output_stream_buffer[phone_audio.output_stream_rp],0,valid_bytes/2);
				}
				if(devp->audio_client_fd < 0)
				{
					goto SEND_ERR;
				}
				send_ret = send(devp->audio_client_fd,audio_sendbuf,valid_bytes/2,MSG_DONTWAIT);
				if(send_ret < 1)//或者小于0。则表示socket异常。发送线程中有错不处理其他，只需要处理自身就行
				{
					getsockopt(devp->audio_client_fd,SOL_SOCKET,SO_ERROR,&optval, &optlen);
					if(errno == EAGAIN && optval == 0)
					{
						phone_audio.output_stream_rp += valid_bytes;
						if(phone_audio.output_stream_rp >= AUDIO_STREAM_BUFFER_SIZE)
							phone_audio.output_stream_rp = 0;
						usleep(15*1000);
						continue;
					}
SEND_ERR:
					phone_audio.audio_send_thread_flag = 0;
					PRINT("error,when send to socket,error code is %d\n",send_ret);
					break;
				}
				else
				{
					//PRINT("send_ret = %d\n",send_ret);
					//if(send_ret != 400)
						//count++;
					//PRINT("count = %d\n",count);
					phone_audio.output_stream_rp += send_ret*2;
					total_send_bytes += send_ret*2;
					if(phone_audio.output_stream_rp >= AUDIO_STREAM_BUFFER_SIZE)
						phone_audio.output_stream_rp = 0;
				}
			}
			print_loop++;
			if(print_loop > 300)//5秒打印一次
			{
				//打印信息。。。。
				PRINT("total_send_bytes is %d\r\n",total_send_bytes);
				print_loop = 0;
			}
			usleep(15* 1000);
		}
		//向对讲主叫发送数据
		while(phone_audio.audio_talkback_send_thread_flag == 1)
		{
			//读取位置已经到缓冲尾部，则从头开始
			if(phone_audio.output_stream_rp == AUDIO_STREAM_BUFFER_SIZE)
				phone_audio.output_stream_rp = 0;
			//获取缓冲中有效数据字节数
			if(phone_audio.output_stream_wp >= phone_audio.output_stream_rp)
				valid_bytes = phone_audio.output_stream_wp - phone_audio.output_stream_rp;
			else
				valid_bytes = AUDIO_STREAM_BUFFER_SIZE - phone_audio.output_stream_rp;
			//每次只允许发送最多AUDIO_SEND_PACKET_SIZE个字节
			if(valid_bytes > AUDIO_SEND_PACKET_SIZE)
				valid_bytes = AUDIO_SEND_PACKET_SIZE;
			if((valid_bytes >= AUDIO_SEND_PACKET_SIZE) || (phone_audio.output_stream_wp < phone_audio.output_stream_rp))
			{
				if(devp->audio_client_fd < 0)
				{
					goto TB_SEND_ERR;
				}
				send_ret = send(devp->audio_client_fd,&output_stream_buffer[phone_audio.output_stream_rp],valid_bytes,MSG_DONTWAIT);
				if(send_ret < 1)//或者小于0。则表示socket异常。发送线程中有错不处理其他，只需要处理自身就行
				{
					getsockopt(devp->audio_client_fd,SOL_SOCKET,SO_ERROR,&optval, &optlen);
					if(errno == EAGAIN && optval == 0)
					{
						usleep(15*1000);
						continue;
					}
TB_SEND_ERR:
					phone_audio.audio_talkback_send_thread_flag = 0;
					PRINT("error,when send to socket,error code is %d",send_ret);
					break;
				}
				else
				{
					phone_audio.output_stream_rp += send_ret;
					total_send_bytes += send_ret;
					if(phone_audio.output_stream_rp >= AUDIO_STREAM_BUFFER_SIZE)
						phone_audio.output_stream_rp = 0;
				}
			}
			print_loop++;
			if(print_loop > 300)//5秒打印一次
			{
				//打印信息。。。。
				PRINT("total_send_talkbacking_bytes is %d\r\n",total_send_bytes);
				print_loop = 0;
			}
			usleep(15* 1000);
		}
		PRINT("audio send thread entry idle...\n");
		phone_audio.audio_send_thread_exit_flag = 1;
		while(phone_audio.audio_send_thread_flag == 0 && phone_audio.audio_talkback_send_thread_flag == 0)
		{
			usleep(30* 1000);
		}
		PRINT("audio send thread entry busy...\n");
	}
	//打印线程退出信息
	PRINT("AudioSendThreadCallBack exit....\n");
}

//从socket接收数据线程
void* AudioRecvThreadCallBack(void* argv)
{
	PRINT("AudioRecvThreadCallBack is running....\n");
	int optval;
	int optlen = sizeof(int);
	int free_bytes;
	int i,j;
	int recv_ret,recv_bytes;
	int print_loop = 0;
	unsigned int total_recv_bytes = 0;
	int first_recv_tb = 0;
	int recv_flag = 0;
	int tmp_len = 0;
	int tmp = 0;
	dev_status_t* devp;
	unsigned char tmp_buf[2000];
	phone_audio.audio_recv_thread_flag = 0;
	phone_audio.audio_talkback_recv_thread_flag = 0;
	while(1)
	{
		phone_audio.input_stream_wp = 0;
		phone_audio.input_stream_rp = 0;
		print_loop = 0;
		total_recv_bytes = 0;
		phone_audio.audio_recv_thread_exit_flag = 0;
		first_recv_tb = 0;
		recv_flag = 0;
		tmp_len = 0;
		tmp = 0;
		for(i=0;i<CLIENT_NUM;i++)
		{
			if(devlist[i].client_fd == -1)
				continue;
			if(devlist[i].dev_is_using == 1 || devlist[i].talkbacking == 1)
			{
				devp=&devlist[i];
			}
		}
		while(phone_audio.audio_recv_thread_flag == 1)
		{
			if(phone_audio.start_dtmf != 0)
			{
				PRINT("dtmf = %c\n",phone_audio.start_dtmf);
				dtmf_ret = GenerateCodePcmData(&phone_audio.start_dtmf,1,dtmf_buf,Big_Endian);
				//total_dtmf_ret += dtmf_ret;
				if(total_dtmf_ret <= dtmf_ret)
					phone_audio.input_stream_wp = phone_audio.input_stream_rp = 0;
				//total_dtmf_ret = dtmf_ret+32000;
				if(total_dtmf_ret < 32000)
				{
					total_dtmf_ret = 32000+dtmf_ret;
				}
				else
				{
					total_dtmf_ret += dtmf_ret;
					total_dtmf_ret = total_dtmf_ret - (total_dtmf_ret % 64);
				}
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
				phone_audio.start_dtmf = 0;
			}
			if(total_dtmf_ret > 0) //双音多频丢数据
			{
				//PRINT("dtmfing....\n");
				recv_ret = recv(devp->audio_client_fd,tmp_buf,AUDIO_SEND_PACKET_SIZE,MSG_DONTWAIT);
				if(recv_ret < 1)//或者小于0。则表示socket异常，此处需要同时处理其他线程的错误
				{
					getsockopt(devp->audio_client_fd,SOL_SOCKET,SO_ERROR,&optval, &optlen);
					if(errno == EAGAIN && optval == 0)
					{
						usleep(10*1000);
						continue;
					}
					total_dtmf_ret = 0;
					goto RECV_ERROR;
				}
				usleep(10*1000);
				//PRINT("total_dtmf_ret = %d\n",total_dtmf_ret);
				total_dtmf_ret -= (recv_ret*2);
				if(total_dtmf_ret <= 0)
				{
					total_dtmf_ret = 0;
					PRINT("dtmf over!!!!\n");
					phone_audio.dtmf_over = 1;
					//phone_audio.input_stream_wp = phone_audio.input_stream_rp = 0;
					//ioctl(phone_audio.phone_audio_pcmfd,SLIC_RELEASE,0);
					//ioctl(phone_audio.phone_audio_pcmfd,SLIC_INIT,0);
					//continue;
				}
				else
					continue;
			}
			while(phone_audio.start_recv)
			{
				recv_ret = recv(devp->audio_client_fd,tmp_buf,AUDIO_SEND_PACKET_SIZE,MSG_DONTWAIT);
				if(recv_ret < 1)//或者小于0。则表示socket异常，此处需要同时处理其他线程的错误
				{
					getsockopt(devp->audio_client_fd,SOL_SOCKET,SO_ERROR,&optval, &optlen);
					if(errno == EAGAIN && optval == 0)
					{
						goto RECV_CONTINUE;
					}
					goto RECV_ERROR;
				}
RECV_CONTINUE:
				usleep(10*1000);
				if(!phone_control.start_dial)
				{
					for(i=0;i<(200);i++)
					{
						if(phone_audio.audio_recv_thread_flag ==0)
							break;
						recv_ret = recv(devp->audio_client_fd,tmp_buf,AUDIO_SEND_PACKET_SIZE,MSG_DONTWAIT);
						if(recv_ret < 1)//或者小于0。则表示socket异常，此处需要同时处理其他线程的错误
						{
							getsockopt(devp->audio_client_fd,SOL_SOCKET,SO_ERROR,&optval, &optlen);
							if(errno == EAGAIN && optval == 0)
							{
								usleep(10*1000);
								continue;
							}
							goto RECV_ERROR;
						}
						usleep(10*1000);
					}
					PRINT("start recv.....\n");
					recv_ret = 0;
					break;
				}
			}
			if(recv_flag == 0)
			{
				//ioctl(phone_audio.phone_audio_pcmfd,SET_WRITE_TYPE,1);
				recv_flag = 1;
			}
			phone_control.dial_over = 1;
			phone_audio.start_recv = 0;
			//获取input buffer的剩余空间
			if(phone_audio.input_stream_wp >= AUDIO_STREAM_BUFFER_SIZE)
				phone_audio.input_stream_wp	= 0;
			free_bytes = AUDIO_STREAM_BUFFER_SIZE - phone_audio.input_stream_wp;
			if(devp->audio_client_fd < 0)
			{
				goto RECV_ERROR;
			}
			//PRINT("tmp_len = %d\n",tmp_len);
			if(devp->desencrypt_enable == 1)
			{
				if(recv_ret > 0 && tmp_len > 0)
				{
					PRINT("tmp_len = %d\n",tmp_len);
					PRINT("recv_ret = %d\n",recv_ret);
					memcpy(audio_recvbuf,audio_recvbuf+recv_ret,tmp_len);
				}
			}
			if(free_bytes > 2400)
				free_bytes = 2400;
			if(devp->desencrypt_enable == 1)
				recv_ret = recv(devp->audio_client_fd,audio_recvbuf+tmp_len,free_bytes/2,MSG_DONTWAIT);
			else
				recv_ret = recv(devp->audio_client_fd,audio_recvbuf,free_bytes/2,MSG_DONTWAIT);
			if(recv_ret < 1)//或者小于0。则表示socket异常，此处需要同时处理其他线程的错误
			{
				getsockopt(devp->audio_client_fd,SOL_SOCKET,SO_ERROR,&optval, &optlen);
				//PRINT("optval = %d\n",optval);
				if(errno == EAGAIN && optval == 0)
				{
					usleep(15*1000);
					continue;
				}
RECV_ERROR:
				PRINT("error,when receive from socket,error code is %d\r\n",recv_ret);
				phone_audio.audio_echo_thread_flag = 0;
				phone_audio.audio_recv_thread_flag = 0;
				phone_audio.audio_send_thread_flag = 0;
				phone_audio.audio_read_write_thread_flag = 0;
				close(devp->audio_client_fd);
				devp->audio_client_fd = -1;

				phone_audio.audio_reconnect_flag = 1;

				break;

			}
			else
			{
				if(devp->encrypt_enable == 1)
					//解码
					UlawDecodeEncrypt(&input_stream_buffer[phone_audio.input_stream_wp],0,audio_recvbuf,0,recv_ret);
				else if(devp->desencrypt_enable == 1)
				{
					tmp_len = (recv_ret%8);
					recv_ret = recv_ret -tmp_len ;
					//解码
					UlawDecodeDesEncrypt(&input_stream_buffer[phone_audio.input_stream_wp],0,audio_recvbuf,0,recv_ret);
				}
				else
					//解码
					UlawDecode(&input_stream_buffer[phone_audio.input_stream_wp],0,audio_recvbuf,0,recv_ret);
				phone_audio.input_stream_wp += recv_ret*2;
				total_recv_bytes += recv_ret*2;
				if(phone_audio.input_stream_wp >= AUDIO_STREAM_BUFFER_SIZE)
					phone_audio.input_stream_wp = 0;
			}
			print_loop++;
			if(print_loop > 300)//5秒打印一次
			{
				//打印信息。。。。
				PRINT("total_recv_bytes is %d\r\n",total_recv_bytes);
				print_loop = 0;
			}
			usleep(15* 1000);
		}
		//对讲接收主叫数据
		while(phone_audio.audio_talkback_recv_thread_flag == 1)
		{
			//获取input buffer的剩余空间
			if(phone_audio.input_stream_wp >= AUDIO_STREAM_BUFFER_SIZE)
				phone_audio.input_stream_wp = 0;
			free_bytes = AUDIO_STREAM_BUFFER_SIZE - phone_audio.input_stream_wp;
			if(devp->audio_client_fd < 0)
			{
				goto TB_RECV_ERROR;
			}
			recv_ret = recv(devp->audio_client_fd,&input_stream_buffer[phone_audio.input_stream_wp],free_bytes,MSG_DONTWAIT);
			if(recv_ret < 1)//或者小于0。则表示socket异常，此处需要同时处理其他线程的错误
			{
				getsockopt(devp->audio_client_fd,SOL_SOCKET,SO_ERROR,&optval, &optlen);
				if(errno == EAGAIN && optval == 0)
				{
					usleep(15*1000);
					continue;
				}
TB_RECV_ERROR:
				PRINT("error,when receive from socket,error code is %d\r\n",recv_ret);
				//phone_audio.audio_talkback_recv_thread_flag = 0;
				//phone_audio.audio_talkback_send_thread_flag = 0;
				//phone_audio.audio_talkbacked_recv_send_thread_flag = 0;
//
				//close(devp->audio_client_fd);
				//devp->audio_client_fd = -1;
				stopaudio(devp,TALKBACK,0);
				break;

			}
			else
			{
				if(first_recv_tb == 1)
				{
					phone_audio.input_stream_wp += recv_ret;
					total_recv_bytes += recv_ret;
					if(phone_audio.input_stream_wp >= AUDIO_STREAM_BUFFER_SIZE)
						phone_audio.input_stream_wp = 0;
				}
				first_recv_tb = 1;
			}
			print_loop++;
			if(print_loop > 300)//5秒打印一次
			{
				//打印信息。。。。
				PRINT("total_recv_talkbacking_bytes is %d\r\n",total_recv_bytes);
				print_loop = 0;
			}
			usleep(15* 1000);
		}
		PRINT("audio recv thread entry idle...\n");
		phone_audio.audio_recv_thread_exit_flag = 1;
		while(phone_audio.audio_recv_thread_flag == 0 &&
				phone_audio.audio_talkback_recv_thread_flag == 0)
		{
			usleep(30* 1000);
		}
		PRINT("audio recv thread entry busy...\n");
	}
	//打印线程退出信息
}

//音频通道检测线程
void* audio_loop_accept(void* argv)
{
	int clientfd = 0;
	int i;
	fd_set fdset;
	struct timeval tv;
	struct timeval timeout;
	memset(&tv, 0, sizeof(struct timeval));
	memset(&timeout, 0, sizeof(struct timeval));
	struct sockaddr_in client;
	socklen_t len = sizeof(client);

	while (1)
	{
		FD_ZERO(&fdset);
		FD_SET(phone_audio.phone_audio_sockfd, &fdset);

		tv.tv_sec =   1;
		tv.tv_usec = 0;
		switch(select(phone_audio.phone_audio_sockfd + 1, &fdset, NULL, NULL,
					&tv))
		{
			case -1:
			case 0:
				break;
			default:
				{
					if (FD_ISSET(phone_audio.phone_audio_sockfd, &fdset) > 0)
					{
						if ((clientfd = accept(phone_audio.phone_audio_sockfd
										,(struct sockaddr*)&client, &len)) < 0)
						{
							PRINT("accept failed!\n");
							break;
						}
						char *new_ip = NULL;
						struct in_addr ia = client.sin_addr;
						new_ip = inet_ntoa(ia);
						PRINT("new_ip=%s\n",new_ip);

						for(i=0;i<CLIENT_NUM;i++)
						{
							if(devlist[i].client_fd == -1)
								continue;
							if(!strcmp(new_ip,devlist[i].client_ip))
							{
								PRINT("audio client found\n");
								if(devlist[i].audio_client_fd > 0 )
								{
									PRINT("audio client has been created\n");
									//break;
									if(devlist[i].dev_is_using == 1)
										stopaudio(&devlist[i],PSTN,1);
										
								}
								devlist[i].audio_client_fd = clientfd;
								if(!strcmp(phone_control.who_is_online.client_ip,devlist[i].client_ip) && phone_control.who_is_online.attach == 1)
								{
									PRINT("audio reconnect\n");
									startaudio(&devlist[i],1);
									phone_audio.audio_reconnect_flag=0;
									if(phone_control.start_dial == 1)
									{
										//音频重连时，拨号没有结束，重新拨号
										onhook();
										usleep(150*1000);
										offhook();
										usleep(150*1000);
										dialup(phone_control.telephone_num, phone_control.telephone_num_len);
									}
								}
							}
						}
						print_devlist();
					}
				}
		}
		//usleep(100*1000);
	}
}

