/*
 * =====================================================================================
 *
 *       Filename:  phone_timer_posix.cc
 *
 *    Description:  timer for keep heartbeating
 *
 *        Version:  1.0
 *        Created:  05/03/2013 04:07:40 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (chen.chunsheng badwtg1111@hotmail.com), 
 *   Organization:  handaer
 *
 * =====================================================================================
 */
#include <stdio.h>
#include <time.h>
#include <signal.h>

#include <iostream>
using namespace std;

#include "phone_timer_posix.h"
#include "phone_log.h"

using namespace webrtc;

 
namespace handaer {

PhoneTimerWrapper * PhoneTimerPosix::Create(){
	PhoneTimerPosix* ptr = new  PhoneTimerPosix(INTERVAL_SEC, 0);
	if (!ptr) {
	  return NULL;
	}	
	
	return ptr;
}		
	   
PhoneTimerPosix::PhoneTimerPosix(int sec, int usec)
	:	 _ptrThreadPhoneTimer(NULL),
		 _phoneTimerThreadId(0),	
		 _phoneTimerRunning(false),
	     phd_func(NULL),
		 ptimer(&phone_timer),
		 first(false) {				
	//cout << __FUNCTION__ << " " << __LINE__ << endl;	
	
	setTimer(INTERVAL_SEC, 0);	
}

PhoneTimerPosix::~PhoneTimerPosix(){
	//cout << __FUNCTION__ << " " << __LINE__ << endl;
	cancel();

	ptimer = NULL;
}



void PhoneTimerPosix::setTimer(int sec,int usec){				
	//cout << __FUNCTION__ << " " << __LINE__ << endl;

	struct timeval interval;
	interval.tv_sec = sec;
	interval.tv_usec = usec;
	
	phone_timer_set_interval(ptimer, &interval);

	return;
} 




void PhoneTimerPosix::setHandler(ProcessHandler handler){				
	//cout << __FUNCTION__ << " " << __LINE__ << endl;
	phd_func = handler;

	return;
}		 

bool PhoneTimerPosix::reset(){				 
	//cout << __FUNCTION__ << " " << __LINE__ << endl;
	first = true;
	ptimer->timer_init(); // start time count

	if(ptimer->state == RTP_TIMER_RUNNING)
		startPhoneTimer();
	
	return true;
}

bool PhoneTimerPosix::cancel(){
	//cout << __FUNCTION__ << " " << __LINE__ << endl;
	stopPhoneTimer();

	ptimer->timer_uninit();	
	
	return true;
}


int32_t PhoneTimerPosix::startPhoneTimer(){
    if (_phoneTimerRunning) {
        return 0;
    }

    _phoneTimerRunning = true;

	 
    const char* threadName = "handaer_phone_timer_posix_thread";
    _ptrThreadPhoneTimer = ThreadWrapper::CreateThread(phoneTimerThreadFunc,
                                                this,
                                                kRealtimePriority,
                                                threadName);
    if (_ptrThreadPhoneTimer == NULL) {
        cout << "failed to create the phone timer posix thread" << endl;
        _phoneTimerRunning = false;
        return -1;
    }

    unsigned int threadID(0);
    if (!_ptrThreadPhoneTimer->Start(threadID)) {
        cout << "failed to start the phone timer posix thread" << endl;
        _phoneTimerRunning = false;
        delete _ptrThreadPhoneTimer;
        _ptrThreadPhoneTimer = NULL;
        return -1;
    }
    _phoneTimerThreadId = threadID;


    return 0;	

}
int32_t PhoneTimerPosix::stopPhoneTimer(){
	_phoneTimerRunning = false;

    if ( _ptrThreadPhoneTimer
			&& !_ptrThreadPhoneTimer->Stop() ) {
        cout << "failed to stop the phone timer posix thread" << endl;
        return -1;
    } else {
        delete _ptrThreadPhoneTimer;
        _ptrThreadPhoneTimer = NULL;
    }
	
    return 0;

}
bool PhoneTimerPosix::phoneTimerisRunning() const{
	return (_phoneTimerRunning);

}

bool PhoneTimerPosix::phoneTimerThreadFunc(void* pThis){
	return (static_cast<PhoneTimerPosix*>(pThis)->phoneTimerThreadProcess());

}

bool PhoneTimerPosix::phoneTimerThreadProcess(){
	//cout << __FUNCTION__ << " " << __LINE__ << endl;

	if(first == true){
		first = false;
	} else {
		phd_func(SIGALRM);
	}
	ptimer->timer_do();
	
	return true;
}

}


