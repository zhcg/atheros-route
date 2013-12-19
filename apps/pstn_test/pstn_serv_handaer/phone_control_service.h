/*
 * =====================================================================================
 *
 *       Filename:  phone_control_service.h
 *
 *    Description:  in charge of phone control process 
 *
 *        Version:  1.0
 *        Created:  05/03/2013 04:07:31 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (chen.chunsheng badwtg1111@hotmail.com), 
 *   Organization:  handaer
 *
 * =====================================================================================
 */
#ifndef _PHONE_CONTROL_SERVICE_H_
#define _PHONE_CONTROL_SERVICE_H_

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "phone_thread_wrapper.h"


#include "phone_control_base.h"

using namespace webrtc;
//class ThreadWrapper;



namespace handaer{
extern ssize_t readline(int fd, void *vptr, size_t maxlen, int index);



class PhoneControlBase;

class PhoneControlService: public PhoneControlBase{
	public:
		enum { 
			MAX_NUM_LEN = 73,
			SERIAL_BUF_LEN = 128,
			//TIMEOUT_US = (500 * 1000), //30 ms
			TIMEOUT_US = (30 * 1000), //30 ms
		};
		typedef enum PhoneControlType{
					DEFAULT,
						
					DIALING,
					OFFHOOK,
					ONHOOK,
					DTMFSEND,
					FLASH,
					MUTE,
					CLEARINCOMING,
					
					RINGON,
					RINGOFF,
					INCOMING_NUM,
					BUSYLINE,
					OTHER_OFFHOOK,
				}PhoneControlTypeEnum;

		//before you:  new  PhoneControlService() ;
		//you must: bool setPhoneProxyFd(int fd);
		PhoneControlService();
		~PhoneControlService();

		bool phoneControlServiceInit();
		bool phoneControlServiceExit();
		
		int dialUp(const  char *num, int num_len);
		int offHook(void);
		int onHook(void);
		int sendDtmf(const  char dtmf);
		int flash(void);
		int mute(bool yes_no);

		int connectIntoIdle(void);
		ssize_t netReadLine(int fd,void *buffer,size_t maxlen, int i);

		int parsePhoneProxyEvent(char* msg, int msg_len_wanted);
		int handlePhoneProxyEvent(void *msg, int msg_len);		

		int parseSerialEvent(char* msg, int msg_len_wanted);
		int handleSerialEvent(void *msg, int msg_len);
		
		int returnOtherOffhook();		
		int returnRingOn(void *msg, int msg_len);
		int returnRingOff(void *msg, int msg_len);
		int returnIncomingNumber(void *tel_num, int tel_num_len);
		int returnBusyLine(void *msg, int msg_len);
		
		static bool setPhoneProxyFd(int fd);
		bool getIncomingNumber(char *num, int *num_len);
		bool setGoingNumber(char *num, int num_len);
		bool setDtmfNumber(unsigned char num);
		bool setMute(bool yes_no);

	    virtual int32_t StartPhoneControlService();
	    virtual int32_t StopPhoneControlService();
	    virtual bool PhoneControlServiceRunning() const;

		bool resetFdsetAndTimeout();
		
	private:
		static bool PhoneControlServiceThreadFunc(void*);		
		bool PhoneControlServiceThreadProcess();

		int max(int a, int b) { return (a > b ? a : b); }
		
	
	private:
		
		ThreadWrapper* _ptrThreadPhoneControlService;
		uint32_t ThreadPhoneControlServiceId;
	
		bool phoneControlServiceRunning;

		fd_set fdR;	
		struct timeval tv;

		
		
	private:
		static int phone_proxy_fd;
		PhoneControlTypeEnum event_type_serial;
		PhoneControlTypeEnum event_type_client;

		char incoming_number[MAX_NUM_LEN];
	public:
		char going_number[MAX_NUM_LEN];
	
		char dtmf;
		bool mute_ok;
		
	private:
		
		
		char incoming_son_number;
		char going_son_number;

	private:
	    virtual int32_t StartPhoneIncoming();
	    virtual int32_t StopPhoneIncoming();
	    virtual bool PhoneIncomingRunning() const;
		static bool PhoneIncomingThreadFunc(void*);		
		bool PhoneIncomingThreadProcess();
		
		ThreadWrapper* _ptrThreadPhoneIncoming;
		uint32_t ThreadPhoneIncomingId;
		bool phoneIncomingRunning;


};

}

#endif
