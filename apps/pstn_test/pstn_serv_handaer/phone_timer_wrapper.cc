/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "phone_timer_wrapper.h"



#ifdef _SET_I_TIMER_
#include "phone_timer.h"
#else
#include "phone_timer_posix.h"
#endif

#include <iostream>
using namespace std;
#include "phone_log.h"

namespace handaer {

PhoneTimerWrapper* PhoneTimerWrapper::CreatePhoneTimerWrapper() {  
#ifdef _SET_I_TIMER_
	cout << __FUNCTION__ << " in PhoneTimer::Create() " << __LINE__ << endl;
	return PhoneTimer::Create();
#else
	//cout << __FUNCTION__ << " in PhoneTimerPosix::Create() " << __LINE__ << endl;
	PLOG(LOG_INFO," in PhoneTimerPosix::Create() ");
	return PhoneTimerPosix::Create();
#endif  

}

} // namespace handaer