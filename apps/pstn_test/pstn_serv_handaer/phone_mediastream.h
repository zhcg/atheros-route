#ifndef _PHONE_MEDIASTREAM_H_
#define _PHONE_MEDIASTREAM_H_

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include <math.h>


#if 0
#include "mediastream.h"
#include "msequalizer.h"
#include "msvolume.h"
#ifdef VIDEO_ENABLED
#include "msv4l.h"
#endif
#endif



#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msequalizer.h"
#include "mediastreamer2/msvolume.h"
#ifdef VIDEO_ENABLED
#include "mediastreamer2/msv4l.h"
#endif

#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#ifndef WIN32
#include <unistd.h>
#else
#include <malloc.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#ifdef __APPLE__
#include <CoreFoundation/CFRunLoop.h>
#endif
#if  defined(__ios) || defined (ANDROID)
#ifdef __ios
#import <UIKit/UIKit.h>
#endif
extern void ms_set_video_stream(VideoStream* video);
#ifdef HAVE_X264
extern void libmsx264_init();
#endif
#ifdef HAVE_SILK
extern void libmssilk_init();
#endif 
#endif

//extern void libmsbcg729_init();




#ifdef ANDROID
#include <android/log.h>
#include <jni.h>
#endif

#include <ortp/b64.h>




typedef struct _MediastreamDatas {
	int localport,remoteport,payload;
	char ip[64];
	char *fmtp;
	int jitter;
	int bitrate;
	MSVideoSize vs;
	bool_t ec;
	bool_t agc;
	bool_t eq;
	bool_t is_verbose;
	int device_rotation;

#ifdef VIDEO_ENABLED
	VideoStream *video;
#endif
	char * capture_card;
	char * playback_card;
	char * camera;
	char *infile,*outfile;
	float ng_threshold;
	bool_t use_ng;
	bool_t two_windows;
	bool_t el;
	bool_t use_rc;
	bool_t enable_srtp;
	bool_t pad[3];
	float el_speed;
	float el_thres;
	float el_force;
	int el_sustain;
	float el_transmit_thres;
	float ng_floorgain;
	char * zrtp_secrets;
	PayloadType *custom_pt;
	int video_window_id;
	int preview_window_id;
	/* starting values echo canceller */
	int ec_len_ms, ec_delay_ms, ec_framesize;
	char* srtp_local_master_key;
	char* srtp_remote_master_key;
	int netsim_bw;
	
	AudioStream *audio;	
	PayloadType *pt;
	RtpSession *session;
	OrtpEvQueue *q;
	RtpProfile *profile;

	pthread_t stat_tid;
	int stat_cond;
	int audio_running;
	
} MediastreamDatas;

#ifdef __cplusplus
	extern "C" {
#endif


extern MediastreamDatas* mediastream_init(int argc, char * argv[]);
extern int mediastream_uninit(MediastreamDatas* args);
extern int mediastream_begin(MediastreamDatas* args);
extern int mediastream_end(MediastreamDatas* args);

#ifdef __cplusplus
	}
#endif

#endif
