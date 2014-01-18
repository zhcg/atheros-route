/***************************************************************************
** File Name:      fsk_internal.h                                          *
** Author:         mengfanyi                                               *
** Date:           2013-12-16                                              *
** Copyright:      2013 handaer, Inc. All Rights Reserved.                 *
** Description:    This file is used to describe the message               *
****************************************************************************
**                         Important Edit History                          *
** ------------------------------------------------------------------------*
** DATE           NAME             DESCRIPTION                             *
** 2013-12-16     mengfanyi        Create                                  *
** 2013-12-30     mengfanyi        link buffer queue charge to fixed buffer*
****************************************************************************/
#ifndef  _FSK_INTERNAL_H_
#define  _FSK_INTERNAL_H_

#include "fsk_external.h"

//#define FSK_DEBUG
//#define FSK_DEBUG_BIT
//#define FSK_DEBUG_MSG_WORD

#ifdef	FSK_DEBUG
#define FSK_PRINT(fmt,...) printf(fmt,__VA_ARGS__)
#else
#define FSK_PRINT(fmt,...)
#endif
#ifdef	FSK_DEBUG_BIT
#define FSK_PRINT_BIT(fmt,...) printf(fmt,__VA_ARGS__)
#else
#define FSK_PRINT_BIT(fmt,...)
#endif
#ifdef	FSK_DEBUG_MSG_WORD
#define FSK_PRINT_MSG_WORD(fmt,...) printf(fmt,__VA_ARGS__)
#else
#define FSK_PRINT_MSG_WORD(fmt,...)
#endif


#define FSK_START_BIT		0
#define FSK_END_BIT			1
#define FSK_MIN_VALID_MINVAL		1800
#define FSK_MIN_VALID_DECVAL		3500

#define FSK_ONE_COUNT		7
#define FSK_TWO_COUNT		6
#define FSK_THREE_COUNT		7
#define FSK_MIN_COUNT		6
#define FSK_CYCLE_GETDATA	20
#define FSK_MAX_CON_DOT		5
#define FSK_MSG_WORD_LENTH	11
#define FSK_CHANNAL_SIGNL_COUNT 120		//150 '0'-'1'
#define FSK_MARK_SIGNL_COUNT    150		//180 or 80 '1'

#define FSK_BUFFER_LENGTH			1024
#define FSK_RESERVE_LENGTH			FSK_CYCLE_GETDATA + FSK_MIN_COUNT + 4
#define FSK_BUFFER_START			FSK_RESERVE_LENGTH
#define FSK_BUFFER_MIN				20

#define FSK_MSG_TYPE_MAKECALL		0x80	//呼叫建立
#define FSK_MSG_TYPE_CID			0x04	//主叫号码传送信息

#define FSK_MSG_PARAM_TYPE_CALLTIME			0x01	//呼叫时间
#define FSK_MSG_PARAM_TYPE_CALLNUM			0x02	//主叫号码
#define FSK_MSG_PARAM_TYPE_NONUM			0x04	//无主叫号码
#define FSK_MSG_PARAM_TYPE_CALLNAME			0x07	//主叫姓名
#define FSK_MSG_PARAM_TYPE_NONAME			0x08	//无主叫姓名

#define FSK_MSG_PARAM_NODISP			'P'		//不允许显示主叫号码或姓名
#define FSK_MSG_PARAM_NOGET				'O'		//无法得到主叫号码或姓名

#define FSK_NOISE_SIGNAL_COUNT				100
#define FSK_NOISE_DEFAULT_LEVEL				600
#define FSK_NOISE_SIGNAL_RADIO				57/1000
//#define FSK_NOISE_SIGNAL_RADIO				1/5

#define FSK_ABS(x) (x < 0 ? -x : x)
//#define FSK_SAMESIGN(x,y)	(x < 0 ? (y > 0 ? FALSE : TRUE) : (y < 0 ? FALSE : TRUE))
//#define FSK_SAMESIGN(x,y)	(x*y >= 0 ? TRUE : FALSE)
//#define FSK_SAMESIGN(x,y)	((x^y) < 0 ? ((x & y) == 0 ? TRUE : FALSE) : TRUE)
#define FSK_SAMESIGN(x,y)	((x^y) > 0 || (x & y) == 0)

//typedef unsigned char		BOOLEAN;

typedef enum {
	FSK_MSG_STEP_START = 0x00,

	FSK_MSG_STEP_MSG_TYPE,
	FSK_MSG_STEP_MSG_LENTH,

	FSK_MSG_STEP_PARAM_TYPE,
	FSK_MSG_STEP_PARAM_LENTH,
	FSK_MSG_STEP_PARAM_WORD,

	FSK_MSG_STEP_MONTH,
	FSK_MSG_STEP_DAY,
	FSK_MSG_STEP_HOUR,
	FSK_MSG_STEP_MINUTE,
	FSK_MSG_STEP_NUMBER,
	FSK_MSG_STEP_NAME,

	FSK_MSG_STEP_CHECK,
	FSK_MSG_STEP_END
}Fsk_Msg_Step_E;

typedef enum {
	FSK_PARSE_STEP_START = 0x00,
	FSK_PARSE_STEP_HEAD,
	FSK_PARSE_STEP_CHANNEL,
	FSK_PARSE_STEP_MARK,
	FSK_PARSE_STEP_MSG,
	FSK_PARSE_STEP_END
}Fsk_Parse_Step_E;

typedef struct _Fsk_Buffer_
{
	int count;
	//int precount;
	int start;
	int circlestart;
	int bitstart;

	short int buffer[FSK_BUFFER_LENGTH + FSK_RESERVE_LENGTH];
	char bitbuf[FSK_BUFFER_LENGTH + FSK_RESERVE_LENGTH];
}Fsk_Buffer_T;

typedef struct _Fsk_Msg_Buffer_
{
	int size;
	int start;
	short int last_value;
	short int value_max;
	short int noise_max;

	Fsk_Parse_Step_E parse_step;

	int bit_count;
	BOOLEAN bitflag;
	int con1bit_count;
#ifdef FSK_DEBUG
	int allbitcount;
#endif
	unsigned char msgtype;
	unsigned char msglen;
	unsigned char msgcount;
	unsigned char checksum;
	unsigned char msgmaxlen;

	unsigned char paramtype;
	unsigned char paramlen;
	unsigned char paramno;

	Fsk_Msg_Step_E step;

	int bit_in_word;
	unsigned char data_word[FSK_MSG_WORD_LENTH+1];
#ifdef FSK_DEBUG
	char *buffer;
#endif
}Fsk_Msg_Buffer_T;


/*****************************************************************************/
//  Description : free buffer
//  Global resource dependence : g_fsk_buf_head
//  input: none
//  output: none
//  return: none
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
//void Fsk_FreeFirstBuffer(void);

/*****************************************************************************/
//  Description : free buffer
//  Global resource dependence : g_fsk_buf_head
//  input: none
//  output: none
//  return: none
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
//void Fsk_FreeAllBuffer(void);

/*****************************************************************************/
//  Description : set zero dot
//  Global resource dependence : 
//  input: none
//  output: none
//  return: none
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
char Fsk_SetFSKOneZeroData(short int prev_val, short int val);

/*****************************************************************************/
//  Description : set zero dot
//  Global resource dependence : 
//  input: buf--buffer pointer
//  output: bit_flag--convert to data
//  return: TRUE--success
//          FALSE--failure
//  Author: mengfanyi
//  date:2013-12-16
//  Note: buf size equel to bit_flag size
/*****************************************************************************/
BOOLEAN Fsk_SetFSKZeroData(short int *buf, char *bit_flag, int count);

/*****************************************************************************/
//  Description : set zero dot
//  Global resource dependence : 
//  input: none
//  output: none
//  return: none
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
BOOLEAN Fsk_SetFSKZeroBuffer(Fsk_Buffer_T *pbuf);

/*****************************************************************************/
//  Description : parse data to bit
//  Global resource dependence : g_fsk_buf_head
//  input: pbuf--buffer pointer
//         start--start position
//  output: bit_ch--bit value pointer
//  return: >0--success,process dot count
//          -1--farelure
//  Author: mengfanyi
//  date:2013-12-16
//  Note:  
/*****************************************************************************/
int Fsk_ParseOneBit(Fsk_Buffer_T *pbuf, int start, char *bit_ch, int *adjcount);

#if 0
/*****************************************************************************/
//  Description : sync start bit
//  Global resource dependence : g_fsk_buf_head
//  input: pbuf--buffer pointer
//         dotcn--dot count
//  output: none
//  return: >=0--move dot count
//          -1 --failure
//  Author: mengfanyi
//  date:2013-12-16
//  Note: modify g_fsk_buf_head
/*****************************************************************************/
int Fsk_SyncStartBit(int dotcn);
#endif
/*****************************************************************************/
//  Description : sync start bit
//  Global resource dependence : g_fsk_buf_head
//  input: pbuf--buffer pointer
//         dotcn--dot count
//  output: none
//  return: TRUE--sucess
//          FALSE --failure
//  Author: mengfanyi
//  date:2013-12-16
//  Note: start>=0, dotcn>=0
/*****************************************************************************/
BOOLEAN Fsk_SyncStartBit(Fsk_Buffer_T *pbuf, int start,int dotcn);

/*****************************************************************************/
//  Description : sync fsk signal
//  Global resource dependence : g_fsk_buf_head
//  input: pbuf--buffer pointer
//         start--start position
//  output: none
//  return: >=0--move dot count
//          -1 --failure
//  Author: mengfanyi
//  date:2013-12-16
//  Note: don't modify g_fsk_buf_head and pbuf
/*****************************************************************************/
int Fsk_GetSyncSignal(Fsk_Buffer_T *pbuf, int start);

/*****************************************************************************/
//  Description : locate FSK head,start dot
//  Global resource dependence : g_fsk_buf_head
//  input: none
//  output: none
//  return: FALSE--failure
//          TRUE--success
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
BOOLEAN Fsk_LocateFSKHead(void);

/*****************************************************************************/
//  Description : locate fsk channel signal
//  Global resource dependence : g_fsk_buf_head
//  input: none
//  output: none
//  return: >=0--process dot count
//          -1--failure
//  Author: mengfanyi
//  date:2013-12-16
//  Note:  150 '01'
/*****************************************************************************/
int Fsk_LocateFSKChannelSignal(void);

/*****************************************************************************/
//  Description : process buffer
//  Global resource dependence : g_fsk_buf_head
//  input: none
//  output: none
//  return: none
//  Author: mengfanyi
//  date:2013-12-16
//  Note: 180 '1' or 80 '1'
/*****************************************************************************/
int Fsk_LocateFSKMarkSignal(void);

/*****************************************************************************/
//  Description : parse fsk message data word
//  Global resource dependence : g_fsk_buf_head,g_fsk_msg_hex
//  input: word--fsk message word
//  output: none
//  return: TRUE--sucess
//          FALSE--failure
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
BOOLEAN Fsk_ParseFSKMsgWord(unsigned char *word);

/*****************************************************************************/
//  Description : parse fsk message
//  Global resource dependence : g_fsk_buf_head,g_fsk_msg_hex
//  input: none
//  output: none
//  return: none
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
int Fsk_ParseFSKMsg(void);

/*****************************************************************************/
//  Description : process buffer
//  Global resource dependence : g_fsk_buf_head
//  input: none
//  output: none
//  return: none
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
void Fsk_Process(void);

#endif
