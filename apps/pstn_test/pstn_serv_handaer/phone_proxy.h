/*
 * =====================================================================================
 *
 *       Filename:  phone_proxy.h
 *
 *    Description:  phone proxy for PAD or Handset
 *
 *        Version:  1.0
 *        Created:  05/03/2013 04:07:36 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (chen.chunsheng badwtg1111@hotmail.com), 
 *   Organization:  handaer
 *
 * =====================================================================================
 */
#ifndef _PHONE_PROXY_H_
#define _PHONE_PROXY_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/epoll.h>

#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#include "phone_control_service.h"
#include "phone_timer_wrapper.h"
#if 1
#include "phone_mediastream.h"
#endif

#include "phone_thread_wrapper.h"

#include "phone_critical_section_wrapper.h"

#ifdef __cplusplus
extern "C" {
#endif

//#include "nvram.h"

#ifdef __cplusplus
}
#endif


using namespace webrtc;

namespace handaer{

extern ssize_t readline(int fd, void *vptr, size_t maxlen, int index);



class PhoneProxy{
	public:
		enum {
			USERNAME_LEN = 19,
			PASSWD_LEN = 16,
			CLIENT_NUM = 4,
			BACK_LOG = 10,
			SERVER_PORT = 63290,
			MAX_NUM_LEN_ = 64,
			MAX_LEN_PCLI = 512,
			MAX_EVENTS = 5,
			EPOLL_TIMEOUT_MS = 30,
			TIMER_SEC = 1,
			LOCAL_AUDIO_PORT = 44444,
			REMOTE_AUDIO_PORT = 55556,
			PAYLOAD_PCMU = 0,
			TIMEOUT_TRANS = 60,
			OPTION_INTERVAL = 5,
			TIMEOUT_INCOMING = 5,
			RING_INTERVAL = 2000,

		};
		
		enum PstnCommand {
			UNDEFINED = -1,
			DEFAULT,
			/*client heartbeat*/
			HEARTBEAT,
			
			/*Pstn control event*/
			DIALING,	
			OFFHOOK,			
			ONHOOK,		
			DTMF_SEND,
			FLASH,
			MUTE,

			
			//about registration
			QUERY,
			LOGIN,
			LOG_OUT,			
			LOGOUTALL,

			//switch function
			SWITCHTOSONMACHINE,
			SWITCHOFFHOOK,
			SWITCHONHOOK,
			SWITCHBACK,

			//talkback function
			TALKBACKDIALING,
			TALKBACKOFFHOOK,
			TALKBACKONHOOK,

			INCOMING_OK,
			
		};  
		typedef enum PstnCommand PstnCmdEnum;

		typedef enum{
			//Pstn incoming event
			DEFAULT_INCOMING,
			RINGON,
			RINGOFF,
			INCOMING_NUM,
			BUSYLINE,
			OTHER_OFFHOOK,

			
		}PstnCmdIncomingEnum;

		typedef int (*timer_process_cbfn_t)(void * ,void *);

		typedef struct _timer_info {
			
			int in_use;	 // on or off  
			int interval;
			int elapse;    // 0~interval  
			
			timer_process_cbfn_t  timer_func_cb;
			void * timer_func_cb_data;
			void * this_obj;

		}timer_info_t;
		
		typedef struct {
			int fd;
			char head[6]; //head
			char id; // client_id
			int length; //length
			PstnCmdEnum cmd; //cmd
			int arglen; //arg_len
			char arg[MAX_NUM_LEN_+1]; //arg
			char username[USERNAME_LEN];
			int user_name_len;
			char password[PASSWD_LEN];
			//short crc16;
		}cli_request_t; //client cmd message

		typedef enum timer_type{
			TIMER_HEARTBEAT = 0 ,
			TIMER_INCOMING,
		}TimerTypeEnum;

		typedef struct {
			bool registerd;
			char id;
			char username[USERNAME_LEN];
			char password[PASSWD_LEN];
			
			int client_fd; // PAD(0), Handset(1~3)
			char client_ip[PASSWD_LEN];

			timer_info_t * ti[2];

			int old_timer_counter;
			int timer_counter;
			int exit_threshold;

			bool timer_setting;
			MediastreamDatas* _ms_datas;

			volatile bool using_phone;

			//int timeout_incoming;
			//int timer_count_incoming;
			bool has_incom_ok;
			
		}cli_info_t;
		
		PhoneProxy();
		~PhoneProxy();

		void setNoiseGateThreshold(const char *_ng_thres);

		bool startAudio(cli_info_t *p_ci, int payload);
		bool stopAudio(cli_info_t *p_ci, int payload);

		int listenOnServerSocket(int port);
		int accepting_loop(bool yes_no);
		int my_accept(struct sockaddr *addr, socklen_t *addrlen);

		PstnCmdEnum getCmdtypeFromString(char *cmd_str);
		int netWrite(int fd,const void *buffer,int length);
		int netRead(int fd,void *buffer,int length);

		ssize_t netReadLine(int fd,void *buffer,size_t maxlen, int i);

		int parseClient(cli_info_t *ci_ , int i);
		int handleClient(cli_info_t *ci_);

		int dispatchClientId(cli_info_t *ci);
		int queryOnlineInfo(char *h1);

		int parsePhoneControlServiceEvent(); 		
		int handlePhoneControlServiceEvent();


	
		
	public:
	    virtual int32_t startPhoneProxy();
	    virtual int32_t stopPhoneProxy();
	    virtual bool phoneProxyisRunning() const;

		virtual int32_t startHeartBeating();
	    virtual int32_t stopHeartBeating();
	    virtual bool heartBeatingisRunning() const;
		

		int setClientTimer(cli_info_t* cli_info, int interval, 
				 timer_process_cbfn_t timer_proc, void *arg, void* pThis, 
				 TimerTypeEnum _timer_type);
		int deleteClientTimer(cli_info_t* cli_info, TimerTypeEnum _timer_type);

		bool phoneProxyInit(int port);
		bool phoneProxyExit();
		
	private:
		static bool phoneProxyThreadFunc(void*);		
		bool phoneProxyThreadProcess();

		bool resetFdsetAndTimeout();


		static bool heartBeatingThreadFunc(void*);		
		bool heartBeatingThreadProcess();


		int max(int a, int b) { return (a > b ? a : b); }

		static void get_current_format_time(char * tstr);

		static void notify( int signum );
		static int recycleClient(void *arg, void *pThis);
		static int resendIncoming(void *arg, void *pThis);
		static bool resetTimerInfo();
		int destroyClient(cli_info_t *pci);

		
	private:
		
		ThreadWrapper* _ptrThreadPhoneProxy;
		uint32_t _phoneProxyThreadId;	
		bool _phoneProxyRunning;
		
		ThreadWrapper* _ptrThreadHeartBeating;
		uint32_t _heartBeatingThreadId;	
		bool _heartBeatingisRunning;

		static timer_info_t timer_info_g[CLIENT_NUM*2];
	private:
		PhoneControlService *pcs;
		PhoneTimerWrapper *timer;
#if 0
		MediastreamDatas* _ms_datas;
#endif
		int server_port;
		int payload_type;
		int serv_listen_sockfd;
		
		int phone_proxy_fd[2]; // 0 -- self, 1 -- PhoneControlService

	public:
		cli_info_t ci[CLIENT_NUM];	
	private:

		cli_request_t _cli_req;

		PstnCmdIncomingEnum _phone_control_service_event;
		
		
		int epollfd;
		
		fd_set fdR;	
		fd_set fdRall;

		struct timeval tv;
		int maxfd;

		volatile bool phone_in_use;

		char client_ip_using[PASSWD_LEN];

		
	private:
		// void phoneLock() { _phone_critSect.Enter(); };
		// void phoneUnLock() { _phone_critSect.Leave(); };
		// CriticalSectionWrapper& _phone_critSect;

		
		char phone_name_id[CLIENT_NUM][USERNAME_LEN] ;
		char username_in_flash[CLIENT_NUM][USERNAME_LEN];
		bool username_is_null[CLIENT_NUM];
		char passwd_in_flash[PASSWD_LEN];

		bool incoming_call;

		cli_info_t * ci_transout;
		cli_info_t * ci_transinto;

		cli_info_t * ci_talkback_out;
		cli_info_t * ci_talkback_into;
	public:
		char incoming_response[MAX_LEN_PCLI];
	private:
		RingStream *rs;
		bool dnd;//do not disturb
		bool ringing;
	public:
		void phoneRingStart();
		void phoneRingStop();
		void setDND(bool yes_no);

};

}

#endif
