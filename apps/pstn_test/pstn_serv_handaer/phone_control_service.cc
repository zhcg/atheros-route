/*
 * =====================================================================================
 *
 *       Filename:  phone_control_service.cc
 *
 *    Description:  in charge of phone control process 
 *
 *        Version:  1.0
 *        Created:  05/03/2013 04:07:34 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (chen.chunsheng badwtg1111@hotmail.com), 
 *   Organization:  handaer
 *
 * =====================================================================================
 */
#include "phone_thread_wrapper.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <iostream>
#include <sys/time.h>
#include "phone_control_service.h"

#include "phone_log.h"

using namespace std;
using namespace webrtc;


bool not_send_ring = false;


namespace handaer{
int PhoneControlService::phone_proxy_fd = -1;

PhoneControlService::PhoneControlService()
	:PhoneControlBase(),	 
	 _ptrThreadPhoneControlService(NULL),
	 ThreadPhoneControlServiceId(0),	
	 phoneControlServiceRunning(false),
	 
	 event_type_serial(DEFAULT),
	 event_type_client(DEFAULT),
	 dtmf(0),
	 mute_ok(false),
	 incoming_son_number(-1),
	 going_son_number(-1) ,
	 _ptrThreadPhoneIncoming(NULL),
	 ThreadPhoneIncomingId(0),
	 phoneIncomingRunning(false)
{

	FD_ZERO(&fdR);
	tv.tv_sec = 0;
	tv.tv_usec = TIMEOUT_US;
	 
	memset(incoming_number, 0, MAX_NUM_LEN);
	memset(going_number, 0, MAX_NUM_LEN);



	phoneControlServiceInit();

	StartPhoneControlService();
#if 1
#ifdef SPI_FOR_PHONE

	StartPhoneIncoming(); 
#endif
#endif
}

PhoneControlService::~PhoneControlService(){
#if 1
#ifdef SPI_FOR_PHONE

	StopPhoneIncoming();
#endif
#endif
	StopPhoneControlService();
	
	phoneControlServiceExit();
}
//#define __COMPILE_FOR_6410__
bool PhoneControlService::phoneControlServiceInit(){
#ifndef SPI_FOR_PHONE

#ifdef __COMPILE_FOR_6410__
	setDeviceName("/dev/ttySAC3");
#else	
	setDeviceName("/dev/ttyS0");
#endif
	openDevice();
	serialConfig(B9600);
#ifdef __COMPILE_FOR_6410__
	connectIntoIdle();
#endif	

#endif
	return true;
}

bool PhoneControlService::phoneControlServiceExit(){
#ifndef SPI_FOR_PHONE	
	closeDevice();
#endif	
	return true;
}


int PhoneControlService::dialUp(const  char *num, int num_len){
#ifdef __COMPILE_FOR_6410__ 		
	char dial[MAX_NUM_LEN] = {0};
	memcpy(dial, "AT%TPDN=", 9);
	strncat(dial, num, num_len);
	writeDevice(dial,num_len+8);

	char buff[SERIAL_BUF_LEN] = {0};
	readAll(buff, SERIAL_BUF_LEN);
#else
	//sleep(2);
	char dial[MAX_NUM_LEN] = {0};
	if(num_len < 24){
		dial[0] = 0x42;
		dial[1] = num_len+2;
		dial[2] = 0x01;
		dial[3] = 0x01;

		memcpy(&dial[4], num, num_len);

		writeDeviceXor(dial, num_len+4);
	} else if(num_len >= 24) {
		dial[0] = 0x42;
		dial[1] = 0x19;
		dial[2] = 0x02;
		dial[3] = 0x01;

		memcpy(&dial[4], num, 23);
		writeDeviceXor(dial, 27);

		dial[0] = 0x42;
		dial[1] = num_len - 23 + 2;
		dial[2] = 0x02;
		dial[3] = 0x02;

		memcpy(&dial[4], num+23, num_len-23);
		writeDeviceXor(dial, num_len-23+4);
		
	}
#endif
	
	return 0;
}
int PhoneControlService::offHook(void){
	memset(incoming_number, 0 ,sizeof(incoming_number));
#ifdef __COMPILE_FOR_6410__	
	writeDevice("AT%TPCA=1,1,1", 13);
	writeDevice("AT%TPSM=1", 9);

	char buff[SERIAL_BUF_LEN] = {0};
	readAll(buff, SERIAL_BUF_LEN);
#else
	//cout << __FUNCTION__ << " " << __LINE__ << endl;
	char buff[3] = {0};

	buff[0] = 0x41;
	buff[1] = 0x00;
	//buff[2] = 0x01;


	writeDeviceXor(buff, 2);

#endif
	return 0;
}
int PhoneControlService::onHook(void){
	// memset(incoming_number, 0 ,sizeof(incoming_number));
#ifdef __COMPILE_FOR_6410__
	writeDevice("AT%TPCA=0,0,0", 13);
	
	char buff[SERIAL_BUF_LEN] = {0};
	readAll(buff, SERIAL_BUF_LEN);
#else
	char buff[3] = {0};
	buff[0] = 0x5e;
	buff[1] = 0x00;

	writeDeviceXor(buff, 2);
#endif
	return 0;
}

int PhoneControlService::sendDtmf(const  char dtmf){
#ifdef __COMPILE_FOR_6410__
	char key[16] = {0};
	memcpy(key, "AT%TPKB=", 9);
	strncat(key, &dtmf, 1);
	writeDevice(key, 9);

	char buff[SERIAL_BUF_LEN] = {0};
	readAll(buff, SERIAL_BUF_LEN);
#else
	char dial[6] = {0};

	dial[0] = 0x42;
	dial[1] = 0x03;
	dial[2] = 0x01;
	dial[3] = 0x01;
	dial[4] = (char)dtmf;
	

	writeDeviceXor(dial, 5);
#endif
	return 0;
}
int PhoneControlService::flash(void){
#ifdef __COMPILE_FOR_6410__ 
	writeDevice("AT%TPLF", 7);

	char buff[SERIAL_BUF_LEN] = {0};
	readAll(buff, SERIAL_BUF_LEN);
#else
	char dial[5] = {0};

	dial[0] = 0x55;
	dial[1] = 0x02;
	dial[2] = 0x0c;
	dial[3] = 0x18;	

	writeDeviceXor(dial, 4);
#endif
	return 0;
}
int PhoneControlService::mute(bool yes_no){
#ifdef __COMPILE_FOR_6410__ 
	if(yes_no)
		writeDevice("AT%TPCA=1,1,0", 13); //chl,spk,mic  mute
	else
		writeDevice("AT%TPCA=1,1,1", 13); //unmute

	char buff[SERIAL_BUF_LEN] = {0};
	readAll(buff, SERIAL_BUF_LEN);
#else
	char dial[3] = {0};
	if ( yes_no ) {
		dial[0] = 0x45;
	} else {
		dial[0] = 0x46;
	}
	dial[1] = 0x00;
	
	writeDeviceXor(dial, 2);
#endif
	return 0;
}

int PhoneControlService::connectIntoIdle(void){
	
	writeDevice("AT%TPQV=?", 9); 
	writeDevice("AT%TPLM=1", 9);

	char buff[SERIAL_BUF_LEN] = {0};
	readAll(buff, SERIAL_BUF_LEN);
	

	return 0;
}

ssize_t PhoneControlService::netReadLine(int fd,void *buffer,size_t maxlen, int i){
	ssize_t 	n;
	if ( (n = readline(fd, buffer, maxlen, i)) < 0){
		perror(" PhoneControlService::netReadLine readline error: ");
		PLOG(LOG_INFO, "readline error");
	}
	return(n);	
}


int PhoneControlService::parsePhoneProxyEvent(char* msg, int msg_len_wanted){
	ssize_t nread;
	char will_read[64] = {0};
	nread = netReadLine(phone_proxy_fd, will_read, 64, 5);
	PLOG(LOG_INFO,"%s",will_read);
	if(strcmp(will_read, "DIALING\n") == 0){
		event_type_client = DIALING;
	}
	if(strcmp(will_read, "OFFHOOK\n") == 0){
		event_type_client = OFFHOOK;
	}
	if(strcmp(will_read, "ONHOOK\n") == 0){
		event_type_client = ONHOOK;
	}		
	if(strcmp(will_read, "DTMFSEND\n") == 0){
		event_type_client = DTMFSEND;
	}
	if(strcmp(will_read, "FLASH\n") == 0){
		event_type_client = FLASH;
	}		
	if(strcmp(will_read, "MUTE\n") == 0){
		event_type_client = MUTE;
	}
	if(strcmp(will_read, "RINGOFF\n") == 0){
		event_type_client = CLEARINCOMING;
	}
	
	return 0;
}
int PhoneControlService::handlePhoneProxyEvent(void *msg, int msg_len){

	switch(event_type_client){
		case DIALING:
			dialUp(going_number, strlen(going_number));
			break;
		case OFFHOOK:
			offHook();
			break;
		case ONHOOK:
			onHook();
			break;
		case DTMFSEND:
			sendDtmf(dtmf);
			break;
		case FLASH:
			flash();
			break;
		case MUTE:
			mute(mute_ok);
			break;
		case CLEARINCOMING:
			memset(incoming_number, 0 ,sizeof(incoming_number));
			break;
		default:
			break;
	}
	
	return 0;
}

static const char ring_msg[6] = { 0xa5, 0x5a, 0x03, 0x00, 0x03, 0x00};
static const char dtmf_incoming[4] = {0xa5, 0x5a, 0x04, 0x00};
static const char fsk_incoming[4] = {0xa5, 0x5a, 0x05, 0x00};

static const char other_offhook[4] = {0xaa, 0x03, 0x54, 0x54};

int PhoneControlService::parseSerialEvent(char* msg, int msg_len_wanted){

	char recv_buf[SERIAL_BUF_LEN+1];
	ssize_t nread;	 
	
	memset(recv_buf, 0, SERIAL_BUF_LEN+1);	
	nread = readLine(recv_buf, msg_len_wanted);
	if(nread <= 0){
		event_type_serial = DEFAULT;
		//cout << "readLine error" << endl;
		return (-1);
	}

	if ( strncmp((const char *)(recv_buf+1), 
								"AT%PTBU", 7) == 0 ){
		event_type_serial = BUSYLINE;
		return 0;
	}

	if ( strncmp((const char *)(recv_buf), 
								other_offhook, 4) == 0 ){
		event_type_serial = OTHER_OFFHOOK;
		return 0;
	}



	
	if ( (strncmp((const char *)(recv_buf+1), 
				"AT%PTRS=1", 9) == 0) 
		|| (strncmp((const char *)recv_buf, ring_msg, 5) == 0) 
		|| (strncmp((const char *)recv_buf+4, ring_msg, 5) == 0)

		) {
		event_type_serial = RINGON;
		return 0;
	}
	if ( strncmp((const char *)(recv_buf+1), 
				"AT%PTRS=0", 9) == 0 ) {
		event_type_serial = RINGOFF;
		return 0;
	}	
	if ( (strncmp((const char *)(recv_buf+1), 
				"AT%PTCI=", 8) == 0 ) ) {

		event_type_serial = INCOMING_NUM;
		
		if(recv_buf[9] == 'E'){
			cout << "Incoming E" << endl;
			return 0;
		}
 		
		char *p = recv_buf + 9;
		int i=0;
		while(*p != ','){
			msg[i] = *p;
			p++;
			i++;
		}
		
		msg[i] = '\0';
		memset(incoming_number, 0, MAX_NUM_LEN);
		memcpy(incoming_number, msg, strlen(msg)+1);
		cout << "The incoming number is " << msg
			 << " or " << incoming_number
			 << "  tele_no_len = " << i << endl;
		return i;		
	}

	if ((strncmp((const char *)recv_buf+6, dtmf_incoming, 3) == 0 )  
		|| (strncmp((const char *)recv_buf+6, fsk_incoming, 3) == 0 ) ) {
		strcpy(recv_buf, recv_buf+6);
		PLOG(LOG_INFO, "dtmf_incoming:%s", recv_buf);
	}

	if ((strncmp((const char *)recv_buf+4, dtmf_incoming, 3) == 0 ) ) {
		strcpy(recv_buf, recv_buf+4);
		PLOG(LOG_INFO, "dtmf_incoming:%s", recv_buf);
	}

	if ((strncmp((const char *)recv_buf, dtmf_incoming, 3) == 0 ) ) {
		event_type_serial = INCOMING_NUM;
		int num_len = recv_buf[3];
		memset(incoming_number, 0, MAX_NUM_LEN);
		memcpy(incoming_number, recv_buf+4, num_len);
		return num_len;
	}

	if ((strncmp((const char *)recv_buf+4, fsk_incoming, 3) == 0 ) ) {
		strcpy(recv_buf, recv_buf+4);
		PLOG(LOG_INFO, "fsk_incoming:%s", recv_buf);
	}

	if ((strncmp((const char *)recv_buf, fsk_incoming, 3) == 0 ) ) {
		
		if(recv_buf[4] == 0x01){
			event_type_serial = INCOMING_NUM;
			//int total_len = recv_buf[3];
			int num_len = recv_buf[7];
			memset(incoming_number, 0, MAX_NUM_LEN);
			memcpy(incoming_number, recv_buf+8, num_len);
			return num_len;	
		} else {			
			static char *p_incom_num = incoming_number;
			static int num_len = 0;
			static int total_ret = 0;
			memset(p_incom_num, 0, MAX_NUM_LEN - num_len);
			num_len = recv_buf[7];
			total_ret += recv_buf[7];
			memcpy(p_incom_num, recv_buf+8, num_len);
			p_incom_num += num_len;
			if(recv_buf[4] == recv_buf[5]){
				event_type_serial = INCOMING_NUM;
				p_incom_num = incoming_number;
				int ret = total_ret;
				total_ret = 0;
				return ret;
			} else {
				event_type_serial = DEFAULT;
				return num_len;
			}
		}
	}
	
	return 0;
}



int PhoneControlService::handleSerialEvent(void *msg, int msg_len){
	switch(event_type_serial){
		case RINGON:
			if(!not_send_ring){
				returnRingOn(msg, msg_len);
			} else {
				not_send_ring = false;
			}
			break;
		case RINGOFF:
			returnRingOff(msg, msg_len);
			break;
		case BUSYLINE:
			returnBusyLine(msg, msg_len);
			break;
		case INCOMING_NUM:
			cout << __FUNCTION__ << " " << __LINE__ 
				 << " INCOMING_NUM " << endl;
			returnIncomingNumber(msg, msg_len);
			break;
		case OTHER_OFFHOOK:
			returnOtherOffhook();
			break;
		case DEFAULT:
			break;
		default:
			break;
	}

	event_type_serial = DEFAULT;
	
	return 0;
}

int PhoneControlService::returnOtherOffhook(){
	write(phone_proxy_fd, "OTHER_OFFHOOK\n", 14);

	return 0;
}


int PhoneControlService::returnRingOn(void *msg, int msg_len){
	write(phone_proxy_fd, "RINGON\n", 7);
	return 0;
}
int PhoneControlService::returnRingOff(void *msg, int msg_len){
	write(phone_proxy_fd, "RINGOFF\n", 8);
	return 0;
}
int PhoneControlService::returnIncomingNumber(void *tel_num, int tel_num_len){
	write(phone_proxy_fd, "INCOMING\n", 9);
	return 0;
}
int PhoneControlService::returnBusyLine(void *msg, int msg_len){
	write(phone_proxy_fd, "BUSYLINE\n", 9);
	return 0;
}


bool PhoneControlService::setPhoneProxyFd(int fd){
	phone_proxy_fd = fd;

	return true;
}

bool PhoneControlService::getIncomingNumber(char *num, int *num_len){
	*num_len = strlen(incoming_number);
	memcpy(num, incoming_number, strlen(incoming_number)+1);
	return true;
}
bool PhoneControlService::setGoingNumber(char *num, int num_len){
	memcpy(going_number, num, num_len+1);
	return true;	
}
bool PhoneControlService::setDtmfNumber(unsigned char num){
	dtmf = num;
	return true;
}

bool PhoneControlService::setMute(bool yes_no){
	mute_ok = yes_no;
	return true;
}





int32_t PhoneControlService::StartPhoneControlService(){
    if (phoneControlServiceRunning) {
        return 0;
    }

    phoneControlServiceRunning = true;

	// PhoneControlServiceRunning
    const char* threadName = "handaer_phone_control_service_thread";
    _ptrThreadPhoneControlService = ThreadWrapper::CreateThread(PhoneControlServiceThreadFunc,
                                                this,
                                                kRealtimePriority,
                                                threadName);
    if (_ptrThreadPhoneControlService == NULL) {
        cout << "failed to create the phone control service thread" << endl;
        phoneControlServiceRunning = false;
        return -1;
    }

    unsigned int threadID(0);
    if (!_ptrThreadPhoneControlService->Start(threadID)) {
        cout << "failed to start the phone control service thread" << endl;
        phoneControlServiceRunning = false;
        delete _ptrThreadPhoneControlService;
        _ptrThreadPhoneControlService = NULL;
        return -1;
    }
    ThreadPhoneControlServiceId = threadID;


    return 0;	

}

int32_t PhoneControlService::StopPhoneControlService(){

	phoneControlServiceRunning = false;

    if ( _ptrThreadPhoneControlService 
			&& !_ptrThreadPhoneControlService->Stop() ) {
        cout << "failed to stop the phone control service thread" << endl;
        return -1;
    } else {
        delete _ptrThreadPhoneControlService;
        _ptrThreadPhoneControlService = NULL;
    }
	
    return 0;
}

bool PhoneControlService::PhoneControlServiceRunning() const{
	return (phoneControlServiceRunning);
}


bool PhoneControlService::PhoneControlServiceThreadFunc(void* pThis){
	
	return (static_cast<PhoneControlService*>(pThis)->PhoneControlServiceThreadProcess());
}

bool PhoneControlService::resetFdsetAndTimeout(){
	FD_ZERO(&fdR);
#ifndef SPI_FOR_PHONE

	FD_SET(serial_fd, &fdR);
#endif
	FD_SET(phone_proxy_fd, &fdR);
	
	tv.tv_sec = 0;
	tv.tv_usec = TIMEOUT_US;

	return true;
}


bool PhoneControlService::PhoneControlServiceThreadProcess(){
	resetFdsetAndTimeout();	
	
	switch( select(max(serial_fd, phone_proxy_fd)+1, &fdR, NULL, NULL, &tv) ) {
	case -1:
		if (errno == EINTR) {
			//cout << __FUNCTION__ << " : " << endl;
			perror("select in phoneControlService EINTR Error: ");
			break;
		} else {
			perror("select in phoneControlService error: ");			
			exit (-1);
		}		
	case 0:	//timeout			
		break;
	default:

#ifndef SPI_FOR_PHONE

		if ( FD_ISSET(serial_fd, &fdR) ) {
			cout << "serial event happened" << endl;			
			char mmsg[MAX_NUM_LEN];
			
			parseSerialEvent(mmsg, SERIAL_BUF_LEN);
			handleSerialEvent(NULL, 0);
		}
#endif		
		if ( FD_ISSET(phone_proxy_fd, &fdR) ) {
			//cout << "client request event happened" << endl;
			PDEBUG("client request event happened");
			
			parsePhoneProxyEvent(NULL, 0);
			handlePhoneProxyEvent(NULL, 0);			
		}
		break;
	}//end switch (select)
			
	return true;	
}


int32_t PhoneControlService::StartPhoneIncoming(){
    if (phoneIncomingRunning) {
        return 0;
    }

    phoneIncomingRunning = true;

	// PhoneControlServiceRunning
    const char* threadName = "handaer_phone_incoming_thread";
    _ptrThreadPhoneIncoming = ThreadWrapper::CreateThread(PhoneIncomingThreadFunc,
                                                this,
                                                kRealtimePriority,
                                                threadName);
    if (_ptrThreadPhoneIncoming == NULL) {
        cout << "failed to create the phone incoming thread" << endl;
        phoneIncomingRunning = false;
        return -1;
    }

    unsigned int threadID(0);
    if (!_ptrThreadPhoneIncoming->Start(threadID)) {
        cout << "failed to start the phone incoming thread" << endl;
        phoneIncomingRunning = false;
        delete _ptrThreadPhoneIncoming;
        _ptrThreadPhoneIncoming = NULL;
        return -1;
    }
    ThreadPhoneIncomingId = threadID;


    return 0;



}
int32_t PhoneControlService::StopPhoneIncoming(){

	phoneIncomingRunning = false;

    if ( _ptrThreadPhoneIncoming 
			&& !_ptrThreadPhoneIncoming->Stop() ) {
        cout << "failed to stop the phone incoming thread" << endl;
        return -1;
    } else {
        delete _ptrThreadPhoneIncoming;
        _ptrThreadPhoneIncoming = NULL;
    }
	
    return 0;

}
bool PhoneControlService::PhoneIncomingRunning() const{
	return (phoneIncomingRunning);

}
bool PhoneControlService::PhoneIncomingThreadFunc(void* pThis){
	return (static_cast<PhoneControlService*>(pThis)->PhoneIncomingThreadProcess());


} 	
bool PhoneControlService::PhoneIncomingThreadProcess(){
	//cout << "serial event happened" << endl;			
	char mmsg[SERIAL_BUF_LEN];

	//sleep(1);
#if 1
	struct timeval tv = {0, 30000};
	int ret = spi_rt_interface.spi_select(UART1, &tv);
	
	if(ret < 0){
		usleep(30000);
		return true;
	}
#endif
	
	ret = parseSerialEvent(mmsg, SERIAL_BUF_LEN);
	if(ret >= 0)
		handleSerialEvent(NULL, 0);

	return true;
}





}

