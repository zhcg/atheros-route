#ifndef __DTMF_EXTR_H
#define __DTMF_EXTR_H

extern void DtmfInit(void);

extern int DtmfGetCode(char *pcode);

extern int DtmfDo(signed short *psample,int dots);

#endif

