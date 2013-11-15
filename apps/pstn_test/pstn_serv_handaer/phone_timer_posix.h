/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef _PHONE_TIMER_POSIX_H_
#define _PHONE_TIMER_POSIX_H_


#include <sys/time.h>


#include "phone_timer_wrapper.h"
#include "phone_rtptimer.h"
#include "phone_thread_wrapper.h"

using namespace webrtc;

namespace handaer{

class PhoneTimerPosix : public PhoneTimerWrapper {
	public:
		enum{
			TIMETICK_MS = (10 * 1000),
			INTERVAL_SEC = 1,
		};
		static PhoneTimerWrapper * Create();		
	 	   
		PhoneTimerPosix(int sec,int usec);
		~PhoneTimerPosix();
		
		
		virtual void setTimer(int sec,int usec);	
		virtual void setHandler(ProcessHandler handler);	
		virtual bool reset();
		virtual bool cancel();

	public:
	    virtual int32_t startPhoneTimer();
	    virtual int32_t stopPhoneTimer();
	    virtual bool phoneTimerisRunning() const;
	private:
		static bool phoneTimerThreadFunc(void*);		
		bool phoneTimerThreadProcess();
	private:
		
		ThreadWrapper* _ptrThreadPhoneTimer;
		uint32_t _phoneTimerThreadId;	
		bool _phoneTimerRunning;

	private:
		
		ProcessHandler phd_func;
		
		RtpTimer * ptimer;
		bool first;

};

}
        
#endif  // _PHONE_TIMER_POSIX_H_
