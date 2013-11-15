/*
 * =====================================================================================
 *
 *       Filename:  phone_timer.cc
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

#include "phone_timer.h"
 
namespace handaer{
		
	   
PhoneTimer::PhoneTimer(int sec,int usec){				
	_tim_ticks.it_value.tv_sec = sec;				 
	_tim_ticks.it_value.tv_usec = usec;
	
	_tim_ticks.it_interval.tv_sec = sec;				
	_tim_ticks.it_interval.tv_usec = usec;		  
}

PhoneTimer::~PhoneTimer(){

}

PhoneTimerWrapper * PhoneTimer::Create(){
	PhoneTimer* ptr = new  PhoneTimer(INTERVAL_SEC, 0);
	if (!ptr) {
	  return NULL;
	}	
	
	return ptr;
}

void PhoneTimer::setTimer(int sec,int usec){				
	_tim_ticks.it_value.tv_sec = sec;				 
	_tim_ticks.it_value.tv_usec = usec;
	
	_tim_ticks.it_interval.tv_sec = sec;				
	_tim_ticks.it_interval.tv_usec = usec;

	return;
} 

void PhoneTimer::setHandler(ProcessHandler handler){				
	sigemptyset( &_act.sa_mask );				 
	_act.sa_flags = 0;				  
	_act.sa_handler = handler;

	return;
}		 

bool PhoneTimer::reset(){				 
	int res = sigaction( SIGALRM, &_act, &_oldact );
	if ( res ) { 
		perror( "Fail to install handle: " );
		return false;	
	} 
	
	res = setitimer( ITIMER_REAL, &_tim_ticks, 0 ); 
	//alarm(2);    
	if(res){	
		perror( "Fail to set timer: " );   
		sigaction( SIGALRM, &_oldact, 0 );	  
		return false;			   
	} 
	// for ( ; ; ){		 
	wait(TIMETICK_MS); 
	// }	 
	return true;
}

bool PhoneTimer::cancel(){
	setTimer(0,0);
	
	int res = setitimer( ITIMER_REAL, &_tim_ticks, 0 ); 
	//alarm(2);    
	if(res){	
		perror( "Fail to set timer: " );   
		sigaction( SIGALRM, &_oldact, 0 );	  
		return false;			   
	}

	return true;
}


void PhoneTimer::wait( long timeout_ms ){				 
	struct timespec spec;				 
	spec.tv_sec  = timeout_ms / 1000;				 
	spec.tv_nsec = (timeout_ms % 1000) * 1000000;				 
	nanosleep(&spec,NULL);

	return;
}

	



}

