/***************log begin***********/
#include <stdio.h>  
 
#define LOG_DEBUG "PSH_DEBUG"  
#define LOG_TRACE "PSH_TRACE"  
#define LOG_ERROR "PSH_ERROR"  
#define LOG_INFO  "PSH_INFOR"  
#define LOG_CRIT  "PSH_CRTCL"  
 
#define PLOG(level, format, ...) \
	do { \
		fprintf(stderr, "[%s:%s@%s,%d] " format "\n", \
		   level, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__ ); \
    } while (0)  

  
#ifdef PHONE_DEBUG 
#define PDEBUG(format, ...)  PLOG(LOG_DEBUG, format, ##__VA_ARGS__)
#else 
#define PDEBUG(format, ...)  do {} while(0); 
#endif 




/************* log end ********/ 

