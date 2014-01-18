/***************************************************************************
** File Name:      fsk_external.h                                          *
** Author:         mengfanyi                                               *
** Date:           2013-12-16                                              *
** Copyright:      2013 handaer, Inc. All Rights Reserved.                 *
** Description:    This file is used to describe the message               *
****************************************************************************
**                         Important Edit History                          *
** ------------------------------------------------------------------------*
** DATE           NAME             DESCRIPTION                             *
** 2013-12-16     mengfanyi        Create                                  *
****************************************************************************/
#ifndef  _FSK_EXTERNAL_H_
#define  _FSK_EXTERNAL_H_

#ifndef TRUE
#define TRUE	1
#endif
#ifndef FALSE
#define FALSE	0
#endif

typedef unsigned char		BOOLEAN;

typedef struct _Fsk_Message
{
	BOOLEAN isgood;			//true:解析正确，false:解析错误

	char month[3];			//带结束符月字符串，2位有效数字，无日期时为空字符串
	char day[3];			//带结束符日字符串，2位有效数字，无日期时为空字符串
	char hour[3];			//带结束符小时字符串，2位有效数字，无时间时为空字符串
	char minute[3];			//带结束符分钟字符串，2位有效数字，无时间时为空字符串

	BOOLEAN callnumvalid;		//true:电话号码有效，false:电话号码无效
	char num[50];			//电话号码。当callnumvalid为false时，不允许显示主叫号码时为'P',无法得到主叫号码时为'O'

	BOOLEAN cidvalid;			//true:姓名有效，false:姓名无效
	char name[42];			//姓名。当cidvalid为false时，不允许显示主叫姓名时为'P',无法得到主叫姓名时为'O'
}Fsk_Message_T;

/*****************************************************************************/
//  Description : initial parse FSK
//  Global resource dependence : none
//  input: none
//  output: none
//  return: none
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
extern void Fsk_InitParse(void);


/*****************************************************************************/
//  Description : add data into buffer and start parse fsk
//  Global resource dependence : none
//  input: buffer--data buffer pointer
//         count--data count
//  output: none
//  return: TRUE--success
//          FALSE--failure
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
extern BOOLEAN Fsk_AddData(short int *buff, int count);

/*****************************************************************************/
//  Description : get fsk data
//  Global resource dependence : g_fsk_buf_head
//  input: none
//  output: fskmsg--fsk messeage
//  return: TRUE--fsk data is valid
//          FALSE--fsk data is invalid
//  Author: mengfanyi
//  date:2013-12-16
//  Note: fskmsg空间由调用者分配
/*****************************************************************************/
extern BOOLEAN Fsk_GetFskMsg(Fsk_Message_T *fskmsg);

/*****************************************************************************/
//  Description : free all fsk buffer
//  Global resource dependence : g_fsk_buf_head
//  input: none
//  output: none
//  return: none
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
extern void Fsk_FinishFskData(void);

#endif
