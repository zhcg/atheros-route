/*
  The oRTP library is an RTP (Realtime Transport Protocol - rfc3550) stack.
  Copyright (C) 2001  Simon MORLAT simon.morlat@linphone.org

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _PHONE_RTPTIMER_H_
#define _PHONE_RTPTIMER_H_

#include <sys/time.h>

typedef void (*RtpTimerFunc)(void);
	
struct _RtpTimer
{
	int state;
#define RTP_TIMER_RUNNING 1
#define RTP_TIMER_STOPPED 0
	RtpTimerFunc timer_init;
	RtpTimerFunc timer_do;
	RtpTimerFunc timer_uninit;
	struct timeval interval;
};

typedef struct _RtpTimer RtpTimer;

#ifdef __cplusplus
extern "C" {
#endif


void phone_timer_set_interval(RtpTimer *timer, struct timeval *interval);

extern RtpTimer phone_timer;

#ifdef __cplusplus
}
#endif


#endif
