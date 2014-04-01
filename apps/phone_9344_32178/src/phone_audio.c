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
};

unsigned char input_stream_buffer[AUDIO_STREAM_BUFFER_SIZE];
unsigned char output_stream_buffer[AUDIO_STREAM_BUFFER_SIZE];
unsigned char audio_sendbuf[BUFFER_SIZE_2K];
unsigned char audio_recvbuf[BUFFER_SIZE_2K*5];
unsigned char audio_tb_sendbuf[BUFFER_SIZE_2K];
unsigned char audio_tb_recvbuf[BUFFER_SIZE_2K*5];

static FILE* dtmffile;

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
		}
		phone_audio.audio_talkback_recv_thread_flag = 0;
		phone_audio.audio_talkbacked_recv_send_thread_flag = 0;
		phone_audio.audio_talkback_send_thread_flag = 0;
		phone_audio.audio_read_write_thread_flag = 0;
		phone_audio.audio_send_thread_flag = 0;
		phone_audio.audio_recv_thread_flag = 0;
		memset(output_stream_buffer,0,AUDIO_STREAM_BUFFER_SIZE);
		memset(input_stream_buffer,0,AUDIO_STREAM_BUFFER_SIZE);
		usleep(180*1000);
		phone_audio.audio_send_thread_flag = 1;
		phone_audio.audio_recv_thread_flag = 1;
		phone_audio.audio_read_write_thread_flag = 1;
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
void stopaudio(dev_status_t* devp,int flag)
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
		if(flag == 1)
		{
			if(phone_audio.audio_send_thread_flag || phone_audio.audio_recv_thread_flag || phone_audio.audio_read_write_thread_flag )
			{
				phone_audio.audio_send_thread_flag = 0;
				phone_audio.audio_recv_thread_flag = 0;
				phone_audio.audio_read_write_thread_flag = 0;
			}
		}
		if(flag == 0)
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
			if(loops > 20)//100ms等待
				break;
		}while((phone_audio.audio_recv_thread_exit_flag != 1) || (phone_audio.audio_read_write_thread_exit_flag != 1) || (phone_audio.audio_send_thread_exit_flag != 1));
		if(loops == 100)//超时
		{
			 //有线程未响应，也许是之前就退出了也许是未关闭，不过不要紧
		}
		//关闭声卡

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
	//dtmffile = fopen("./read_from_pcm.wav", "w"); //for read sound

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

void* AudioIncomingThreadCallBack(void* argv)
{
	unsigned char audioincoming_buf[BUFFER_SIZE_2K] = {0};
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
			else
			{
				Fsk_AddData((signed short *)audioincoming_buf,read_ret/2);

				if(phone_control.get_fsk == 0)
				{
					//fwrite(audioincoming_buf, 1, read_ret , dtmffile);
					if(DtmfDo((signed short *)audioincoming_buf,read_ret/2) == 2)
					{
						PRINT("get dtmf num...\n");
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
			usleep(40*1000);
		}
		PRINT("audio incoming thread entry idle...\n");
		while(phone_audio.audio_incoming_thread_flag == 0)
		{
			usleep(20* 1000);
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
	dev_status_t* devp;

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
			//read!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			if(phone_audio.phone_audio_pcmfd == -1)
			{
				stopaudio(devp,PSTN);
				continue;
			}
			if(devp->audio_client_fd < 0)
				continue;
			//写入位置已经到缓冲尾部，则从头开始
			if(phone_audio.output_stream_wp >= AUDIO_STREAM_BUFFER_SIZE)
				phone_audio.output_stream_wp = 0;
			//获取output buffer的剩余空间
			free_bytes = AUDIO_STREAM_BUFFER_SIZE - phone_audio.output_stream_wp;
			read_bytes = free_bytes;
			//每次最多从声卡读取AUDIO_READ_BYTE_SIZE字节数据
			if(free_bytes > AUDIO_READ_BYTE_SIZE)
				read_bytes = AUDIO_READ_BYTE_SIZE;
			//从声卡读取数据
			read_times++;
			read_ret = read(phone_audio.phone_audio_pcmfd,&output_stream_buffer[phone_audio.output_stream_wp],read_bytes);
			if(read_ret < 0)//或者小于0。则表示声卡异常
			{
				if(read_times == 1)
					goto START_WRITE;
				PRINT("error,when read from sound card,error code is %d\r\n",read_ret);
				phone_audio.audio_read_write_thread_flag = 0;
				break;
			}
			else
			{
				if(read_times > 5) //丢前5包，屏蔽拨号开始的杂音
				{
					if(phone_audio.dtmf_over == 10) //双音多频后延迟一会打开侧音处理
					{
						if(total_dtmf_ret <= 0)
						{
							//延迟后，检查如果待播放数据大于0（原因：在此延迟间再次二次拨号）则继续关闭侧音消除
							ioctl(phone_audio.phone_audio_pcmfd,SET_WRITE_TYPE,1);
						}
						phone_audio.dtmf_over = 0;
					}
					if(phone_audio.dtmf_over  != 0)
					{
						phone_audio.dtmf_over++;
					}
					phone_audio.output_stream_wp += read_ret;
					total_read_bytes += read_ret;
					if(phone_audio.output_stream_wp >= AUDIO_STREAM_BUFFER_SIZE)
						phone_audio.output_stream_wp = 0;
				}
			}
START_WRITE:
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
			if(phone_control.start_dial == 1 && valid_bytes < AUDIO_WRITE_BYTE_SIZE && valid_bytes != 0)
			{
				valid_bytes = AUDIO_WRITE_BYTE_SIZE;
			}
			if((valid_bytes >= AUDIO_WRITE_BYTE_SIZE) || (phone_audio.input_stream_wp < phone_audio.input_stream_rp))
			{
				if(phone_control.start_dial == 0 && total_dtmf_ret <= 0) //除了拨号和dtmf时，防止延迟
				{
					if(valid_bytes > (5 * AUDIO_WRITE_BYTE_SIZE))
					{
						//打印信息
						PRINT("%d;received data is more,so discard some one\r\n",valid_bytes);
						phone_audio.input_stream_rp += (valid_bytes / AUDIO_WRITE_BYTE_SIZE - 1) * AUDIO_WRITE_BYTE_SIZE;
						total_write_bytes += (valid_bytes / AUDIO_WRITE_BYTE_SIZE - 1) * AUDIO_WRITE_BYTE_SIZE;;
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
				//if(phone_audio.input_stream_wp < phone_audio.input_stream_rp)
					//PRINT("write_ret = %d\n",write_ret);
				//if(phone_audio.input_stream_rp < pcm_ret)
				//{
					//PRINT("phone_audio.input_stream_rp = %d\n",phone_audio.input_stream_rp);
					//PRINT("phone_audio.input_stream_wp = %d\n",phone_audio.input_stream_wp);
				//}
				phone_audio.input_stream_rp += write_ret;
				if(phone_audio.input_stream_rp >= pcm_ret && phone_control.start_dial == 1)
				{
					PRINT("dialup over!!!!!\n");
					phone_control.start_dial = 0;
					//PRINT("phone_audio.input_stream_rp = %d\n",phone_audio.input_stream_rp);
					//PRINT("phone_audio.input_stream_wp = %d\n",phone_audio.input_stream_wp);
					phone_audio.input_stream_rp = 0;
					phone_audio.input_stream_wp = 0;
					//total_write_bytes = 0;
					pcm_ret = 0;
				}
				total_write_bytes += write_ret;
				if(phone_audio.input_stream_rp >= AUDIO_STREAM_BUFFER_SIZE)
					phone_audio.input_stream_rp = 0;
			}
			print_loop++;
			if(print_loop > 600)//5秒打印一次
			{
				//打印信息。。。。
				PRINT("total_write_bytes is %d\r\n",total_write_bytes);
				PRINT("total_read_bytes is %d\r\n",total_read_bytes);
				print_loop = 0;
			}
			usleep(15* 1000);
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
			if(print_loop > 600)//5秒打印一次
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
			usleep(50* 1000);
		}
		PRINT("audio read write thread entry busy...\n");
	}
	PRINT("AudioReadWriteThreadCallBack exit....\n");
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
			//每次只允许发送最多AUDIO_SEND_PACKET_SIZE个字节
			if(valid_bytes > AUDIO_SEND_PACKET_SIZE)
				valid_bytes = AUDIO_SEND_PACKET_SIZE;
			if((valid_bytes >= AUDIO_SEND_PACKET_SIZE) || (phone_audio.output_stream_wp < phone_audio.output_stream_rp))
			{
				//编码
				UlawEncode(audio_sendbuf,0,&output_stream_buffer[phone_audio.output_stream_rp],0,valid_bytes/2);

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
						usleep(15*1000);
						continue;
					}
SEND_ERR:
					phone_audio.audio_send_thread_flag = 0;
					PRINT("error,when send to socket,error code is %d",send_ret);
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
			if(print_loop > 600)//5秒打印一次
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
			if(print_loop > 600)//5秒打印一次
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
			usleep(50* 1000);
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
	int dtmf_recv_times = 0;
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
		dtmf_recv_times = 0;
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
			if(total_dtmf_ret > 0) //双音多频丢数据
			{
				//PRINT("dtmfing....\n");
				recv_ret = recv(devp->audio_client_fd,tmp_buf,1600,MSG_DONTWAIT);
				if(recv_ret < 1)//或者小于0。则表示socket异常，此处需要同时处理其他线程的错误
				{
					getsockopt(devp->audio_client_fd,SOL_SOCKET,SO_ERROR,&optval, &optlen);
					if(errno == EAGAIN && optval == 0)
					{
						usleep(15*1000);
						continue;
					}
					total_dtmf_ret = 0;
					dtmf_recv_times = 0;
					goto RECV_ERROR;
				}
				usleep(10*1000);
				total_dtmf_ret -= (recv_ret*2);
				dtmf_recv_times ++;
				if(dtmf_recv_times == 2)
				{
					ioctl(phone_audio.phone_audio_pcmfd,SET_WRITE_TYPE,0);
				}
				if(total_dtmf_ret <= 0)
				{
					total_dtmf_ret = 0;
					PRINT("dtmf over!!!!\n");
					phone_audio.dtmf_over = 1;
					dtmf_recv_times = 0;
					continue;
				}
				PRINT("total_dtmf_ret = %d\n",total_dtmf_ret);
				continue;
			}
			while(phone_audio.start_recv)
			{
				recv_ret = recv(devp->audio_client_fd,tmp_buf,1600,MSG_DONTWAIT);
				if(recv_ret < 1)//或者小于0。则表示socket异常，此处需要同时处理其他线程的错误
				{
					getsockopt(devp->audio_client_fd,SOL_SOCKET,SO_ERROR,&optval, &optlen);
					if(errno == EAGAIN && optval == 0)
					{
						usleep(15*1000);
						continue;
					}
					goto RECV_ERROR;
				}
				usleep(10*1000);
				if(!phone_control.start_dial)
				{
					for(i=0;i<(32);i++)
					{
						usleep(10*1000);
						if(phone_audio.audio_recv_thread_flag ==0)
							break;
					}
					PRINT("start recv.....\n");
					break;
				}
			}
			if(recv_flag == 0)
			{
				ioctl(phone_audio.phone_audio_pcmfd,SET_WRITE_TYPE,1);
				recv_flag = 1;
			}
			phone_audio.start_recv = 0;
			//获取input buffer的剩余空间
			if(phone_audio.input_stream_wp >= AUDIO_STREAM_BUFFER_SIZE)
				phone_audio.input_stream_wp	= 0;
			free_bytes = AUDIO_STREAM_BUFFER_SIZE - phone_audio.input_stream_wp;
			if(devp->audio_client_fd < 0)
			{
				goto RECV_ERROR;
			}
			if(free_bytes < (BUFFER_SIZE_2K*4))
			{
				recv_ret = recv(devp->audio_client_fd,audio_recvbuf,free_bytes/2,MSG_DONTWAIT);
			}
			else
			{
				recv_ret = recv(devp->audio_client_fd,audio_recvbuf,BUFFER_SIZE_2K*2,MSG_DONTWAIT);
			}
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
				//解码
				UlawDecode(&input_stream_buffer[phone_audio.input_stream_wp],0,audio_recvbuf,0,recv_ret);

				phone_audio.input_stream_wp += recv_ret*2;
				total_recv_bytes += recv_ret*2;
				if(phone_audio.input_stream_wp >= AUDIO_STREAM_BUFFER_SIZE)
					phone_audio.input_stream_wp = 0;
			}
			print_loop++;
			if(print_loop > 600)//5秒打印一次
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
				stopaudio(devp,PSTN);
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
			if(print_loop > 600)//5秒打印一次
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
			usleep(50* 1000);
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
										stopaudio(&devlist[i],PSTN);
										
								}
								devlist[i].audio_client_fd = clientfd;
								if(!strcmp(phone_control.who_is_online.client_ip,devlist[i].client_ip) && phone_control.who_is_online.attach == 1)
								{
									PRINT("audio reconnect\n");
									startaudio(&devlist[i],1);
									phone_audio.audio_reconnect_flag=0;

								}
							}
						}
						for(i=0;i<CLIENT_NUM;i++)
						{
							printf("%d  client_fd = %d  , audio_client_fd = %d\n",i,devlist[i].client_fd,devlist[i].audio_client_fd);
						}
					}
				}
		}
		//usleep(100*1000);
	}
}

