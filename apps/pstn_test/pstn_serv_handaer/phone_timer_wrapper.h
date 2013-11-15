/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef _PHONE_TIMER_WRAPPER_H_
#define _PHONE_TIMER_WRAPPER_H_

namespace handaer {
typedef void(*ProcessHandler)(int);

class PhoneTimerWrapper {
public:  

	virtual ~PhoneTimerWrapper() {};
	static PhoneTimerWrapper* CreatePhoneTimerWrapper();

	virtual void setTimer(int sec,int usec) = 0;
	virtual void setHandler(ProcessHandler handler) = 0; 
	virtual bool reset() = 0;
	virtual bool cancel() = 0;	

};

} // namespace handaer

#endif  // _PHONE_TIMER_WRAPPER_H_