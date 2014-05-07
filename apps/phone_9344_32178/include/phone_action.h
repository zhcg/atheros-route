#ifndef __PHONE_ACTION_H__
#define __PHONE_ACTION_H__
#include "phone_audio.h"

unsigned char sumxor(const  char  *arr, int len);
int senddtmf(char dtmf);
int dialup(char *num,int num_len);
int onhook();
int offhook();

extern struct class_phone_control phone_control;
extern unsigned char input_stream_buffer[AUDIO_STREAM_BUFFER_SIZE];
extern struct class_phone_audio phone_audio;
#endif
