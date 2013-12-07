/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "mediastreamer2/mssndcard.h"
#include "mediastreamer2/msfilter.h"

#include <sys/soundcard.h>

#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>

#include <sys/types.h>
#include <signal.h>




#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif


#if defined(HAVE_CONFIG_H)
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#if 0
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>
#endif
#include "ortp/b64.h"

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#ifdef WIN32
#include <malloc.h> /* for alloca */
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>

#define PHONE_PATH_RINGING 1
#define PHONE_PATH_CALLING 2


#define PCM_IOC_MAGIC	'p'
#define PCM_SET_RECORD			_IO(PCM_IOC_MAGIC, 0)
#define PCM_SET_UNRECORD		_IO(PCM_IOC_MAGIC, 1)	
#define PCM_SET_PLAYBACK		_IO(PCM_IOC_MAGIC, 2)
#define PCM_SET_UNPLAYBACK		_IO(PCM_IOC_MAGIC, 3)
#define PCM_EXT_LOOPBACK_ON		_IO(PCM_IOC_MAGIC, 4)
#define PCM_EXT_LOOPBACK_OFF	_IO(PCM_IOC_MAGIC, 5)
#define	PCM_INT_LOOPBACK_ON		_IO(PCM_IOC_MAGIC, 6)
#define PCM_INT_LOOPBACK_OFF	_IO(PCM_IOC_MAGIC, 7)
#define	PCM_SI3000_REG			_IOW (PCM_IOC_MAGIC, 8, struct si3000_reg)




// /* control amplifier on/off */
// #define	PCM_AMPLIFIER_EN			_IO(PCM_IOC_MAGIC, 9)
// #define PCM_AMPLIFIER_DIS			_IO(PCM_IOC_MAGIC, 10)
// /* control switch to pad/base */
// #define	PCM_SWITCH_TO_PAD		_IO(PCM_IOC_MAGIC, 11)
// #define PCM_SWITCH_TO_BASE		_IO(PCM_IOC_MAGIC, 12)
// /* check pad insert status */
// #define CHECK_PAD_STATUS			_IO(PCM_IOC_MAGIC, 13)
// #define GET_SI3000_APP_PID_NUM		_IO(PCM_IOC_MAGIC, 14)




struct si3000_reg{
	unsigned int reg;// register number
	unsigned char val;// register value
};

static int pad_status = 0;

static int si3000_fd = -1;
static bool_t is_ring = FALSE;

/**
 *  si3000 gpio interrupt handler 
 *  SIGUSR1 - pad leave
 *  SIGUSR2 - pad insert
 *   pad insert(1)/leave(0)
 */
// static void si3000IrqHandler(int signum)
// {
// 	 if (signum == SIGUSR1) {
// 	 	 pad_status = 0;

// 		 if(is_ring == TRUE && si3000_fd > 0){
// 			 ioctl(si3000_fd, PCM_SWITCH_TO_BASE);		
// 			 ioctl(si3000_fd, PCM_AMPLIFIER_EN);
// 	 	 }
		 
// 		 ms_message("app pad leave base\n");
// 	 } else if (signum == SIGUSR2) {
// 	 	 pad_status = 1;
//  		 if(is_ring == TRUE && si3000_fd > 0){
// 			 ioctl(si3000_fd, PCM_SWITCH_TO_BASE);		
// 			 ioctl(si3000_fd, PCM_AMPLIFIER_EN);
// 	 	 }
// 		 ms_message("app pad insert base\n");
// 	 }
// }



MSFilter *ms_oss_read_new(MSSndCard *card);
MSFilter *ms_oss_write_new(MSSndCard *card);
#if 0
static int hs_configure_fd(int fd, int vol,int bits,int stereo, int rate, int *minsz)
{ 
	int min_size=0, blocksize=512;
	/* unset nonblocking mode */
	/* We wanted non blocking open but now put it back to normal ; thanks Xine !*/
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL)&~O_NONBLOCK);
	 
	blocksize = blocksize*(rate/8000); // 512
	 
	min_size = blocksize; 
#if 0	
   struct si3000_reg reg1 = {
		.reg = 1,
	//	.val = 0x04,
		.val = 0x0a,
	//	.val = 0x1a,//enable speaker
	};
    ioctl(fd, PCM_SI3000_REG, &reg1); 
#endif	


    struct si3000_reg reg9 = {
		.reg = 9,
	//	.val = 0x04,
		.val = 0x06, // 01-- lineout -6dB  10-- speaker -12dB
	//	.val = 0x00,
	};
    struct si3000_reg reg7 = {
		.reg = 7,
		.val = 0x58,
	};
	/*
	Register 7. DAC Volume Control
	7
	Reserved
	Read returns zero.
	6:2
	TXG
	TX PGA Gain Control.
	11111= 12 dB
	10111= 0 dB
	00000= 每34.5 dB
	LSB= 1.5 dB
	1
	SLM
	SPKR_L Mute.
	0 = Mute
	1 = Active
	0
	SRM
	SPKR_R Mute.
	0 = Mute
	1 = Active
	*/
	switch(vol)
	{
		case 0:
			reg7.val = 0x58;//SPK Mute
			break;
		case 1:
			reg7.val = 0x2b;//gain:01010
			break;
		case 2:
			reg7.val = 0x53;//gain:10100
			break;
		case 3:
			reg7.val = 0x58;//gain:10110
	//		reg7.val = 0x5b;//gain:10110  enable L and R speaker
	//		reg7.val = 0x5c;//gain:10111 digital gain for DAC to lineout
	//		reg7.val = 0x40;//gain:10000 digital gain for DAC to lineout
		//	reg7.val = 0x28;//gain:01010 digital gain for DAC to lineout
	//		reg7.val = 0x2c;//gain:01011 digital gain for DAC to lineout
		//	reg7.val = 0x7c;// 11111
			break;
		default:
			break;
	}
	 //Johnny force set
 //   reg7.val = 0x7b;//gain:10110
	
	*minsz = min_size;
	//playback
    ioctl(fd, PCM_SI3000_REG, &reg7);
	ioctl(fd, PCM_SI3000_REG, &reg9);
	//record
 
	/*
	Register 5. RX Gain Control 1
	Function
	7:6
	LIG
	Line in Gain.
	11 = 20 dB gain
	10 = 10 dB gain
	01 = 0 dB gain
	00 = Reserved
	5
	LIM
	Line in Mute.
	1 = Line input muted
	0 = Line input goes to mixer
	4:3
	MCG
	MIC Input Gain.
	11 = 30 dB gain
	10 = 20 dB gain
	01 = 10 dB gain
	00 = 0 dB gain
	2
	MCM
	MIC Input Mute.
	1 = Mute MIC input
	0 = MIC input goes into mixer.
	1
	HIM
	Handset Input Mute.
	1 = Mute handset input
	0 = Handset input goes into mixer.
	0
	IIR
	IIR Enable.
	1 = Enables IIR filter
	0 = Enables FIR filter
	Register 6. ADC Volume Control
	Function
	7
	Reserved
	Read returns zero.
	6:2
	RXG
	RX PGA Gain Control.
	11111= 12 dB
	10111= 0 dB
	00000= 每34.5 dB
	LSB= 1.5 dB
	1
	LOM
	Line Out Mute.
	0 = Mute
	1 = Active
	0
	HOM
	Handset Out Mute.
	0 = Mute
	1 = Active
	*/
    struct si3000_reg reg5 = {
		 .reg = 5,
		 .val = 0xdb,
	 };
	struct si3000_reg reg6 = {
		 .reg = 6,
		 .val = 0x63,
	  };
 
	ioctl(fd, PCM_SI3000_REG, &reg5);
	ioctl(fd, PCM_SI3000_REG, &reg6);

	
	ioctl(fd, PCM_SWITCH_TO_PAD);
	if (pad_status == 0)
		ioctl(fd, PCM_AMPLIFIER_DIS);
	else if (pad_status == 1)
		ioctl(fd, PCM_AMPLIFIER_EN);
	
	ioctl(fd, PCM_SET_PLAYBACK); 
	ioctl(fd, PCM_SET_RECORD);
	ms_message("/dev/pcm0 opened: reg5.val = %x,reg6.val = %x,reg7.val = %x,reg9.val = %x,vol= %d,rate=%i,bits=%i,stereo=%i blocksize=%i.",
	reg5.val,reg6.val,reg7.val,reg9.val,vol,rate,bits,stereo,min_size);
	return fd;
}
#else
static int hs_configure_fd(int fd, int vol,int bits,int stereo, int rate, int *minsz)
{ 
	// int min_size=0, blocksize=512;
	int min_size=0, blocksize=512;
	/* unset nonblocking mode */
	/* We wanted non blocking open but now put it back to normal ; thanks Xine !*/
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL)&~O_NONBLOCK);
	 
	blocksize = blocksize*(rate/8000); // 512 ---> 320
	 
	min_size = blocksize; 
#if 0	
    struct si3000_reg reg1 = {
		.reg = 1,
	//    .val = 0x04,
		.val = 0x0a,
	// .val = 0x1a,//enable speaker
	};
    ioctl(fd, PCM_SI3000_REG, &reg1); 
#endif	
	/*
	3:2
	LOT
	Line Out Attenuation.
	11 = 每18 dB analog attenuation on Line Output.
	10 = 每12 dB analog attenuation on Line Output.
	01 = 每6 dB analog attenuation on Line Output.
	00 = 0 dB analog attenuation on Line Output.
	*/
	//01 <==
#if 0
   struct si3000_reg reg9 = {
		.reg = 9,
		//.val = 0x04,
	//	.val = 0x02, // 01-- lineout -6dB  10-- speaker -12dB
		.val = 0x06, // 01-- lineout -6dB  10-- speaker -12dB
	};
	/*
	6:2
	TXG
	TX PGA Gain Control.
	11111= 12 dB
	10111= 0 dB
	00000= 每34.5 dB
	LSB= 1.5 dB
	*/
	//11000<==
   struct si3000_reg reg7 = {
		.reg = 7,
		//.val = 0x5b,
		// reg7.val = 0x6f;//gain:10110
	//	reg7.val = 0x60,//gain:11000
	//	reg7.val = 0x58,//gain:10110
	//	reg7.val = 0x78,//gain:11110
		reg7.val = 0x60,//gain:11010
	};
#endif
	 
	*minsz = min_size;
#if 0
	//playback
    ioctl(fd, PCM_SI3000_REG, &reg7);
	usleep(20000);
	ioctl(fd, PCM_SI3000_REG, &reg9);
#endif
#if 0
	//record
	/*
	7:6
	LIG
	Line in Gain.
	11 = 20 dB gain   <==
	10 = 10 dB gain
	01 = 0 dB gain
	00 = Reserved
	*/
   struct si3000_reg reg5 = {
		 .reg = 5,
		// .val = 0x5b,
		//.val = 0x53,
	//	.val = 0xd3, //gain:11
		.val = 0xf3, //gain:11
	 };
	/*  
	6:2
	RXG
	RX PGA Gain Control.
	11111= 12 dB
	10111= 0 dB
	00000= 每34.5 dB
	LSB= 1.5 dB  
	*/
	//11000 <==
	struct si3000_reg reg6 = {
		 .reg = 6,
		// .val = 0x7f,  //gian:11011
		 .val = 0x7f,//gain:11111
		// .val = 0x6f,//gain:11010
	};
	 usleep(20000);
	ioctl(fd, PCM_SI3000_REG, &reg5);
	usleep(20000);
	ioctl(fd, PCM_SI3000_REG, &reg6);
#endif

#if 0
	ioctl(fd, PCM_SWITCH_TO_PAD);
	if (pad_status == 0)
		ioctl(fd, PCM_AMPLIFIER_DIS);
	else if (pad_status == 1)
		ioctl(fd, PCM_AMPLIFIER_EN);
#endif
	
	ioctl(fd, PCM_SET_PLAYBACK); 
	ioctl(fd, PCM_SET_RECORD);
	//ms_message("/dev/pcm0 opened: reg5.val = %x,reg6.val = %x,reg7.val = %x,reg9.val = %x,vol= %d,rate=%i,bits=%i,stereo=%i blocksize=%i. usleep->20000",
	//reg5.val,reg6.val,reg7.val,reg9.val,vol,rate,bits,stereo,min_size);
	return fd;
}



#endif

int phone_ring_volume = 0;

static int configure_fd(int fd, int vol, int bits,int stereo, int rate, int *minsz)
{	
	int min_size=0, blocksize=512;
	
	/* unset nonblocking mode */
	/* We wanted non blocking open but now put it back to normal ; thanks Xine !*/
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL)&~O_NONBLOCK);

	blocksize = blocksize*(rate/8000); // 512

	min_size = blocksize;	

	ms_message("/dev/pcm0 opened: rate=%i,bits=%i,stereo=%i blocksize=%i.",
			rate,bits,stereo,min_size);
	
	*minsz = min_size;
	
#if 0 
    struct si3000_reg reg1 = {
		.reg = 1,			
		.val = 0x12,//enable speaker
	};
    ioctl(fd, PCM_SI3000_REG, &reg1); 
#endif

	struct si3000_reg reg7 = {
		.reg = 7,
		.val = 0x7b,
	};
	switch(phone_ring_volume){
		case 0:
			break;
		case 1:
			reg7.val = 0x6f;
			break;
		case 2:
			reg7.val = 0x5b;
			break;
		case 3:
			reg7.val = 0x2b;
			break;
		case 4:
			reg7.val = 0x00;
			break;
		default:
			break;
	}

	ioctl(fd, PCM_SI3000_REG, &reg7);

	// ioctl(fd, PCM_SWITCH_TO_BASE);
	// ioctl(fd, PCM_AMPLIFIER_EN);
	

	ioctl(fd, PCM_SET_PLAYBACK);	
	//ioctl(fd, PCM_SET_RECORD);
	
	return fd;
}


typedef struct OssData{
	char *pcmdev;
	char *mixdev;
	int pcmfd_read;
	int pcmfd_write;
	int rate;
	int bits;
	ms_thread_t thread;
	ms_mutex_t mutex;
	queue_t rq;
	MSBufferizer * bufferizer;
	int volume;//Johnny add 
	int phone_path;

	int reg_info;

//	bool_t pad_on_base;

	
	bool_t read_started;
	bool_t write_started;
	bool_t stereo;

} OssData;

static void oss_open(OssData* d, int *minsz){
	//int fd=open(d->pcmdev,O_WRONLY|O_NONBLOCK);
	//int fd=open(d->pcmdev,O_RDONLY|O_NONBLOCK);
	pid_t my_pid;
	int fd = open(d->pcmdev,O_RDWR|O_NONBLOCK);
	if (fd > 0) {

		// read pad insert/leave base infomation
		// if (ioctl(fd, CHECK_PAD_STATUS, &pad_status) < 0)
		// {
		// 	perror("CHECK_PAD_STATUS failed: ");
		// }
		// ms_message("pad insert(1)/leave(0) status:%d", pad_status);

		// my_pid = getpid();
		// ms_message("my pid is :%d", my_pid);

		//register my information
		// if (ioctl(fd, GET_SI3000_APP_PID_NUM, &my_pid) < 0)
		// {
		// 	perror("GET_SI3000_APP_PID_NUM failed: ");
		// }
		
		
		//issue a handler to handle SIGUSR1 and SIGUSR2
		// signal(SIGUSR1, si3000IrqHandler);
		// signal(SIGUSR2, si3000IrqHandler);


		
		
		if(d->phone_path == PHONE_PATH_CALLING){
			ms_message("%s : PHONE_PATH_CALLING ", __FUNCTION__);
			d->pcmfd_read=d->pcmfd_write=hs_configure_fd(fd, d->volume,d->bits, d->stereo, d->rate, minsz);
		} else if(d->phone_path == PHONE_PATH_RINGING){
			ms_message("%s : PHONE_PATH_RINGING ", __FUNCTION__);
			
			d->pcmfd_write=configure_fd(fd, d->volume,d->bits, d->stereo, d->rate, minsz);
			si3000_fd = d->pcmfd_write;
			is_ring = TRUE;

		}
		return ;
	}
	ms_warning ("Cannot open a single fd in rw mode for [%s] trying to open two",d->pcmdev);
	
	return ;
}

static void oss_set_level(MSSndCard *card, MSSndCardMixerElem e, int percent)
{
	OssData *d=(OssData*)card->data;
	int p,mix_fd;
	int osscmd;
	if (d->mixdev==NULL) return;
#if 0
	switch(e){
		case MS_SND_CARD_MASTER:
			osscmd=SOUND_MIXER_VOLUME;
		break;
		case MS_SND_CARD_CAPTURE:
			osscmd=SOUND_MIXER_IGAIN;
		break;
		case MS_SND_CARD_PLAYBACK:
			osscmd=SOUND_MIXER_PCM;
		break;
		default:
			ms_warning("oss_card_set_level: unsupported command.");
			return;
	}
	p=(((int)percent)<<8 | (int)percent);
	mix_fd = open(d->mixdev, O_WRONLY);
	ioctl(mix_fd,MIXER_WRITE(osscmd), &p);
	close(mix_fd);
#endif
}

static int oss_get_level(MSSndCard *card, MSSndCardMixerElem e)
{
	OssData *d=(OssData*)card->data;
	int p=0,mix_fd;
	int osscmd;
	if (d->mixdev==NULL) return -1;
#if 0
	switch(e){
		case MS_SND_CARD_MASTER:
			osscmd=SOUND_MIXER_VOLUME;
		break;
		case MS_SND_CARD_CAPTURE:
			osscmd=SOUND_MIXER_IGAIN;
		break;
		case MS_SND_CARD_PLAYBACK:
			osscmd=SOUND_MIXER_PCM;
		break;
		default:
			ms_warning("oss_card_get_level: unsupported command.");
			return -1;
	}
	mix_fd = open(d->mixdev, O_RDONLY);
	ioctl(mix_fd,MIXER_READ(osscmd), &p);
	close(mix_fd);
	return p>>8;
#endif
}

static void oss_set_source(MSSndCard *card, MSSndCardCapture source)
{
	OssData *d=(OssData*)card->data;
	int p=0;
	int mix_fd;
	if (d->mixdev==NULL) return;

#if 0
	switch(source){
		case MS_SND_CARD_MIC:
			p = 1 << SOUND_MIXER_MIC;
		break;
		case MS_SND_CARD_LINE:
			p = 1 << SOUND_MIXER_LINE;
		break;
	}
	
	mix_fd = open(d->mixdev, O_WRONLY);
	ioctl(mix_fd, SOUND_MIXER_WRITE_RECSRC, &p);
	close(mix_fd);
#endif
}

static void oss_init(MSSndCard *card){
	OssData *d=ms_new(OssData,1);
	d->pcmdev=NULL;
	d->mixdev=NULL;
	d->pcmfd_read=-1;
	d->pcmfd_write=-1;
	d->read_started=FALSE;
	d->write_started=FALSE;
	d->bits=16;
	d->rate=8000;
	d->stereo=FALSE;
	d->phone_path = PHONE_PATH_CALLING;
	d->reg_info = -1;
//	d->pad_on_base = FALSE;
	
//	d->volume = card->vol_level;//Johnny add 
//	   ms_message("card->vol_level = %d",card->vol_level);
	d->volume = 3;//Johnny add  
	//Johnny add 
    ms_message("card->vol_level = %d",3);

	qinit(&d->rq);
	d->bufferizer=ms_bufferizer_new();
	ms_mutex_init(&d->mutex,NULL);
	card->data=d;
}

static void oss_uninit(MSSndCard *card){
	OssData *d=(OssData*)card->data;
	if (d->pcmdev!=NULL) ms_free(d->pcmdev);
	if (d->mixdev!=NULL) ms_free(d->mixdev);
	ms_bufferizer_destroy(d->bufferizer);
	flushq(&d->rq,0);
	ms_mutex_destroy(&d->mutex);
	ms_free(d);
}

#define DSP_NAME "/dev/pcm0"
#define MIXER_NAME "/dev/pcm0"

static void oss_detect(MSSndCardManager *m);

MSSndCardDesc oss_card_desc={
	.driver_type="OSS",
	.detect=oss_detect,
	.init=oss_init,
	.set_level=oss_set_level,
	.get_level=oss_get_level,
	.set_capture=oss_set_source,
	.create_reader=ms_oss_read_new,
	.create_writer=ms_oss_write_new,
	.uninit=oss_uninit
};

static MSSndCard *oss_card_new(const char *pcmdev, const char *mixdev){
	MSSndCard *card=ms_snd_card_new(&oss_card_desc);
	OssData *d=(OssData*)card->data;
	d->pcmdev=ms_strdup(pcmdev);   // "/dev/pcm0"
	d->mixdev=ms_strdup(mixdev);   // "/dev/pcm0"
	card->name=ms_strdup(pcmdev);  //"/dev/pcm0"
	return card;
}

static void oss_detect(MSSndCardManager *m){
	int i;
	char pcmdev[sizeof(DSP_NAME)+3];
	char mixdev[sizeof(MIXER_NAME)+3];
	if (access(DSP_NAME,F_OK)==0){
		MSSndCard *card=oss_card_new(DSP_NAME, MIXER_NAME);
		ms_snd_card_manager_add_card(m,card);
	}
	for(i=0;i<10;i++){
		snprintf(pcmdev,sizeof(pcmdev),"%s%i",DSP_NAME,i);
		snprintf(mixdev,sizeof(mixdev),"%s%i",MIXER_NAME,i);
		if (access(pcmdev,F_OK)==0){
			MSSndCard *card=oss_card_new(pcmdev,mixdev);
			ms_snd_card_manager_add_card(m,card);
		}
	}
}

// sound card init
//////////////////////////////////////////////////
// sound read and write

//#define __WRITETOFILE__


#ifdef __WRITETOFILE__
static FILE* infile;
static FILE* outfile;

#endif

static void * oss_thread(void *p){
	MSSndCard *card=(MSSndCard*)p;
	OssData *d=(OssData*)card->data;
	int bsize=0;
	uint8_t *rtmpbuff=NULL;
	uint8_t *wtmpbuff=NULL;
	int err;
	mblk_t *rm=NULL;
	bool_t did_read=FALSE;
	int r_count;

	oss_open(d,&bsize);

	ms_error("bsize for soundcard -- %d -- \n",bsize);

	if (d->pcmfd_read>=0){
		rtmpbuff=(uint8_t*)alloca(bsize);
	}
	if (d->pcmfd_write>=0){
		wtmpbuff=(uint8_t*)alloca(bsize);
	}
	
#ifdef __WRITETOFILE__

	infile = fopen("/mnt/record_send.wav", "w"); //for read sound
	outfile = fopen("/mnt/recv_play.wav", "w");  //for write sound
#endif
	
	while(d->read_started || d->write_started){
		did_read=FALSE;

		if (d->pcmfd_read>=0){
			if (d->read_started){
				if (rm==NULL) rm=allocb(bsize,0);
				err=read(d->pcmfd_read,rm->b_wptr,bsize);
				// err=read(d->pcmfd_read,rm->b_wptr,318);
				if (err<0) {
					ms_warning("Fail to read %i bytes from soundcard: %s",
					bsize,strerror(errno));
				}else{
#if 0
#ifdef __WRITETOFILE__

					fwrite(rm->b_wptr, 1, err, infile);
#endif
#endif
					did_read=TRUE;
					rm->b_wptr+=err;
					ms_mutex_lock(&d->mutex);
					putq(&d->rq,rm);
					ms_mutex_unlock(&d->mutex);
					rm=NULL;

					
				}
			}else {
				/* case where we have no reader filtern the read is performed for synchronisation */
				int sz = read(d->pcmfd_read,rtmpbuff,bsize);
				if( sz==-1) ms_warning("sound device read error %s ",strerror(errno));
				else did_read=TRUE;
			}
		}

		
		if (d->pcmfd_write>=0){
#if 0
			if(d->phone_path == PHONE_PATH_RINGING){
				if(pad_status == 1)
					ioctl(d->pcmfd_write, PCM_SWITCH_TO_BASE);
				if(pad_status == 0)
					ioctl(d->pcmfd_write, PCM_AMPLIFIER_EN);
				
			}
#endif
			if (d->write_started){
				//wtmpbuff--data to write
				// ms_mutex_lock(&d->mutex);//add by chen.chunsheng
				err=ms_bufferizer_read(d->bufferizer,wtmpbuff,bsize); //bsize = 512 bytes --> 320
				// ms_mutex_unlock(&d->mutex);//add by chen.chunsheng

				//printf("f %s:l %d--bsize=%d\n", __FUNCTION__, __LINE__, bsize);
				
				if (err==bsize){

#ifdef __WRITETOFILE__

					fwrite(wtmpbuff, 1, err, outfile);
#endif
					
					err=write(d->pcmfd_write,wtmpbuff,bsize);
					if (err<0){
						ms_warning("Fail to write %i bytes from soundcard: %s",
						bsize,strerror(errno));
					}
				} else {
					memset(wtmpbuff, 0x01 , bsize);
					err=write(d->pcmfd_write,wtmpbuff,bsize);
					if (err<0){
						ms_error("Fail to write %i bytes ---0--- from soundcard: %s",
						bsize,strerror(errno));
					}

				}


			}else {
				int sz;
				memset(wtmpbuff,0,bsize);
				sz = write(d->pcmfd_write,wtmpbuff,bsize);
				if( sz!=bsize) ms_warning("sound device write returned %i !",sz);
			}
		}
		
		if (!did_read) usleep(20000); /*avoid 100%cpu loop for nothing*/

	}
#ifdef __WRITETOFILE__

	fclose(infile);
	fclose(outfile);
#endif	
	if (d->pcmfd_read==d->pcmfd_write && d->pcmfd_read>=0 ) {
		ioctl(d->pcmfd_read, PCM_SET_UNRECORD);	
		ioctl(d->pcmfd_read, PCM_SET_UNPLAYBACK);

		// ioctl(d->pcmfd_write, PCM_SWITCH_TO_PAD);
		// if(pad_status == 0)
			// ioctl(d->pcmfd_write, PCM_AMPLIFIER_DIS);
		// else if(pad_status == 1)
			// ioctl(d->pcmfd_write, PCM_AMPLIFIER_EN);

		
		close(d->pcmfd_read);
		d->pcmfd_read = d->pcmfd_write =-1;
	} else {
		if (d->pcmfd_read>=0) {
			ioctl(d->pcmfd_read, PCM_SET_UNRECORD);	
			close(d->pcmfd_read);
			d->pcmfd_read=-1;
		}
		if (d->pcmfd_write>=0) {// only ringing
			ioctl(d->pcmfd_write, PCM_SET_UNPLAYBACK);

			// ioctl(d->pcmfd_write, PCM_SWITCH_TO_PAD);
			// if(pad_status == 0)
				// ioctl(d->pcmfd_write, PCM_AMPLIFIER_DIS);
			// else if(pad_status == 1)
				// ioctl(d->pcmfd_write, PCM_AMPLIFIER_EN);
			
			
			close(d->pcmfd_write);
			d->pcmfd_write=-1;
			si3000_fd = -1;
			is_ring = FALSE;
		}
	}	

	return NULL;
}

static void oss_start_r(MSSndCard *card){
	OssData *d=(OssData*)card->data;
	if (d->read_started==FALSE && d->write_started==FALSE){
		d->read_started=TRUE;
		ms_thread_create(&d->thread,NULL,oss_thread,card);
	}else d->read_started=TRUE;
}

static void oss_stop_r(MSSndCard *card){
	OssData *d=(OssData*)card->data;
	d->read_started=FALSE;
	if (d->write_started==FALSE){
		ms_thread_join(d->thread,NULL);
	}
}

static void oss_start_w(MSSndCard *card){
	OssData *d=(OssData*)card->data;
	if (d->read_started==FALSE && d->write_started==FALSE){
		d->write_started=TRUE;
		ms_thread_create(&d->thread,NULL,oss_thread,card);
	}else{
		d->write_started=TRUE;
	}
}

static void oss_stop_w(MSSndCard *card){
	OssData *d=(OssData*)card->data;
	d->write_started=FALSE;
	if (d->read_started==FALSE){
		ms_thread_join(d->thread,NULL);
	}
}

static mblk_t *oss_get(MSSndCard *card){
	OssData *d=(OssData*)card->data;
	mblk_t *m;
	ms_mutex_lock(&d->mutex);
	m=getq(&d->rq);
	ms_mutex_unlock(&d->mutex);
	return m;
}

static void oss_put(MSSndCard *card, mblk_t *m){
	OssData *d=(OssData*)card->data;
	ms_mutex_lock(&d->mutex);
	ms_bufferizer_put(d->bufferizer,m);
	ms_mutex_unlock(&d->mutex);
}


static void oss_read_preprocess(MSFilter *f){
	MSSndCard *card=(MSSndCard*)f->data;
	oss_start_r(card);
}

static void oss_read_postprocess(MSFilter *f){
	MSSndCard *card=(MSSndCard*)f->data;
	oss_stop_r(card);
}

static void oss_read_process(MSFilter *f){
	MSSndCard *card=(MSSndCard*)f->data;
	mblk_t *m;
	while((m=oss_get(card))!=NULL){
		ms_queue_put(f->outputs[0],m);
	}
}

static void oss_write_preprocess(MSFilter *f){
	MSSndCard *card=(MSSndCard*)f->data;
	oss_start_w(card);
}

static void oss_write_postprocess(MSFilter *f){
	MSSndCard *card=(MSSndCard*)f->data;
	oss_stop_w(card);
}

static void oss_write_process(MSFilter *f){
	MSSndCard *card=(MSSndCard*)f->data;
	mblk_t *m;
	while((m=ms_queue_get(f->inputs[0]))!=NULL){
		oss_put(card,m);
	}
}

static int set_rate(MSFilter *f, void *arg){
	MSSndCard *card=(MSSndCard*)f->data;
	OssData *d=(OssData*)card->data;
	d->rate=*((int*)arg);
	return 0;
}

static int get_rate(MSFilter *f, void *arg){
	MSSndCard *card=(MSSndCard*)f->data;
	OssData *d=(OssData*)card->data;
	*((int*)arg)=d->rate;
	return 0;
}

static int set_nchannels(MSFilter *f, void *arg){
	MSSndCard *card=(MSSndCard*)f->data;
	OssData *d=(OssData*)card->data;
	d->stereo=(*((int*)arg)==2);
	return 0;
}

static int set_speech_channel(MSFilter *f, void *arg){
	MSSndCard *card=(MSSndCard*)f->data;
	OssData *d=(OssData*)card->data;
	d->phone_path = *((int*)arg);
	ms_message("%s : %s ", __FUNCTION__, 
		  ((d->phone_path == PHONE_PATH_RINGING)
		  ?"PHONE_PATH_RINGING"
		  :"PHONE_PATH_CALLING"));
	return 0;
}

static int set_si3000_reg(MSFilter *f, void *arg){
	MSSndCard *card=(MSSndCard*)f->data;
	OssData *d=(OssData*)card->data;
	d->reg_info = *((int*)arg);

	
	ms_message("%s : %s ", __FUNCTION__, 
		  " ");
	return 0;
}



static MSFilterMethod oss_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE	, set_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE	, get_rate },
	{	MS_FILTER_SET_NCHANNELS		, set_nchannels	},
	{	MS_FILTER_SET_SPEECH_CHANNEL	, set_speech_channel	},
	{	MS_FILTER_SET_SI3000_REG	, set_si3000_reg	},
	{	0				, NULL		}
};

MSFilterDesc oss_read_desc={
	.id=MS_OSS_READ_ID,
	.name="MSOssRead",
	.text="Sound capture filter for OSS drivers",
	.category=MS_FILTER_OTHER,
	.ninputs=0,
	.noutputs=1,
	.preprocess=oss_read_preprocess,
	.process=oss_read_process,
	.postprocess=oss_read_postprocess,
	.methods=oss_methods
};


MSFilterDesc oss_write_desc={
	.id=MS_OSS_WRITE_ID,
	.name="MSOssWrite",
	.text="Sound playback filter for OSS drivers",
	.category=MS_FILTER_OTHER,
	.ninputs=1,
	.noutputs=0,
	.preprocess=oss_write_preprocess,
	.process=oss_write_process,
	.postprocess=oss_write_postprocess,
	.methods=oss_methods
};

MSFilter *ms_oss_read_new(MSSndCard *card){
	MSFilter *f=ms_filter_new_from_desc(&oss_read_desc);
	f->data=card;
	return f;
}


MSFilter *ms_oss_write_new(MSSndCard *card){
	MSFilter *f=ms_filter_new_from_desc(&oss_write_desc);
	f->data=card;
	return f;
}

MS_FILTER_DESC_EXPORT(oss_read_desc)
MS_FILTER_DESC_EXPORT(oss_write_desc)
