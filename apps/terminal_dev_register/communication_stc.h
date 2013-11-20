#ifndef _COMMUNICATION_STC_H_
#define _COMMUNICATION_STC_H_

#include "common_tools.h"
#include "spi_rt_interface.h"
#include "internetwork_communication.h"
#include "serial_communication.h"

// 录音时使用
#if RECORD_DEBUG 
/* driver i/o control command */
#define PCM_IOC_MAGIC	        'p'
#define PCM_SET_RECORD			_IO(PCM_IOC_MAGIC, 0)
#define PCM_SET_UNRECORD		_IO(PCM_IOC_MAGIC, 1)	
#define PCM_SET_PLAYBACK		_IO(PCM_IOC_MAGIC, 2)
#define PCM_SET_UNPLAYBACK		_IO(PCM_IOC_MAGIC, 3)
#define PCM_EXT_LOOPBACK_ON		_IO(PCM_IOC_MAGIC, 4)
#define PCM_EXT_LOOPBACK_OFF	_IO(PCM_IOC_MAGIC, 5)

#define SI3000 "/dev/pcm0"
#endif

// 结构体定义
struct class_communication_stc
{
    unsigned char cmd;                // 命令字
    pthread_mutex_t *mutex;                                  // 互斥量
    pthread_cond_t *cond;                                    // 条件量
    #if RECORD_DEBUG
    unsigned char *stop_record_flag;
    #endif
    
    #if CTSI_SECURITY_SCHEME == 1
    void* (* recv)(void * para);
    void* (* analyse)(void * para);
    #endif
    
    #if RECORD_DEBUG
    void* (* record)(void * para);
    #endif
    #if BOARDTYPE == 6410
    int (* cmd_on_hook)();
    int (* cmd_off_hook_respond)(char cmd_data);
    int (* send_data)(unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv);
    #else
    int (* cmd_on_hook)();
    int (* cmd_call)(char *phone);
    int (* cmd_off_hook)();
    #if CTSI_SECURITY_SCHEME == 1
    int (* recv_display_msg)();
    #else
    int (* recv_display_msg)(char *phone_num);
    #endif
    int (* send_data)(unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv);
    #endif
    
};

extern struct class_communication_stc communication_stc;
#endif
