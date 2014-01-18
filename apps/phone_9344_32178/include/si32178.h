#ifndef __SI32178_H__
#define __SI32178_H__

typedef enum {
    VDAA_BIT_SET = 1,
    VDAA_BIT_CLEAR = 0
} tVdaaBit;

/*
** Defines structure for daa current Status (ring detect/hook stat)
*/
typedef struct {
    tVdaaBit ringDetectedNeg;
    tVdaaBit ringDetectedPos;
    tVdaaBit ringDetected;
    tVdaaBit offhook;
    tVdaaBit onhookLineMonitor;
} vdaaRingDetectStatusType; 

int set_offhook();
int set_onhook();
int dial(int dialnum);
int si32178_init(int a,int b,int c,int d,int e,int f);
int checkRingStatus(vdaaRingDetectStatusType * pStatus);
int set_onhook_monitor();
#endif
