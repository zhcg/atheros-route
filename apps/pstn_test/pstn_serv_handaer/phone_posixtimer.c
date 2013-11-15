/*
  The oRTP library is an RTP (Realtime Transport Protocol - rfc1889) stack.
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
#include <sys/select.h>

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

#include "phone_rtptimer.h"

#include "phone_log.h"

#define PHONETIMER_INTERVAL 10000


static struct timeval orig, cur;
static unsigned int phone_timer_time = 0;		/*in milisecond */

void phone_timer_init()
{
	phone_timer.state = RTP_TIMER_RUNNING;
	gettimeofday(&orig, NULL);
	phone_timer_time = 0;
}

void phone_timer_do()
{
	int diff, time;
	struct timeval tv;
	gettimeofday(&cur, NULL);
	time=((cur.tv_usec-orig.tv_usec)/1000 ) + ((cur.tv_sec-orig.tv_sec)*1000 );
	if ( (diff = time-phone_timer_time) > 50){
		printf("Must catchup %i miliseconds.\n", diff);
	}
	while((diff = phone_timer_time-time) > 0)
	{
		tv.tv_sec = diff/1000;
		tv.tv_usec = (diff%1000)*1000;

		select(0,NULL,NULL,NULL,&tv);

		gettimeofday(&cur,NULL);
		time=((cur.tv_usec-orig.tv_usec)/1000 ) + ((cur.tv_sec-orig.tv_sec)*1000 );
	}
	//phone_timer_time += PHONETIMER_INTERVAL/1000;
	phone_timer_time += (phone_timer.interval.tv_sec * 1000 
							+ phone_timer.interval.tv_usec/1000);
	//printf("phone_timer_time = %d\n", phone_timer_time);
	PDEBUG("phone_timer_time = %d", phone_timer_time/1000);
}

void phone_timer_uninit()
{
	phone_timer.state = RTP_TIMER_STOPPED;
}

RtpTimer phone_timer = {	0,
							phone_timer_init,
							phone_timer_do,
							phone_timer_uninit,
							{0,PHONETIMER_INTERVAL}};				
							

