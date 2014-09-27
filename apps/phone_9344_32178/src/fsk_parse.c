/***************************************************************************
** File Name:      fsk_parse.c                                             *
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
#include "common.h"
#include "fsk_internal.h"
#include "fsk_external.h"

static Fsk_Buffer_T g_fsk_buff = {0};
static Fsk_Message_T g_fsk_msg = {0};
static Fsk_Message_T g_fsk_bak_msg = {0};
static BOOLEAN g_finish_flag = FALSE;
static BOOLEAN zzl_g_finish_flag = FALSE;
static Fsk_Msg_Buffer_T g_fsk_msg_hex = {0};

static short int Pre_buff[4096];
static int Fsk_lim[2000];
int Fsk_CID[200];
int Fsk_CID_Len;
static int Fsk_n = 0;
static int Lim_ready = 0;
static int Lim_ready_buff = 0;
static int Lim_c = 0;
static int lim_fsk_ready = 0;
static int Pre_fsk_sign = 0;
static int Pre_buff_c = 0;

static int fix =0;
static int buff_fix =0;

extern FILE* fsk_file;
//unsigned char audioincoming_buf[2048] = {0};
/*****************************************************************************/
//  Description : initial parse FSK
//  Global resource dependence : g_fsk_buff
//  input: none
//  output: none
//  return: none
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
void Fsk_InitParse(void)
{
	memset(&g_fsk_buff, 0, sizeof(Fsk_Buffer_T));
	g_fsk_buff.start = FSK_BUFFER_START;
	g_fsk_buff.circlestart = FSK_BUFFER_START;
	g_fsk_buff.bitstart = 0;
	g_fsk_buff.count = 0;
	
	memset(&g_fsk_msg, 0, sizeof(Fsk_Message_T));
	g_fsk_msg.isgood = FALSE;
	memset(&g_fsk_bak_msg, 0, sizeof(Fsk_Message_T));
	memset(&g_fsk_msg_hex, 0, sizeof(Fsk_Msg_Buffer_T));
	g_fsk_msg_hex.step = FSK_MSG_STEP_START;
	g_fsk_msg_hex.parse_step = FSK_PARSE_STEP_START;
	g_fsk_msg_hex.noise_max = FSK_NOISE_DEFAULT_LEVEL;

	g_finish_flag = FALSE;
}

/*****************************************************************************/

/*****************************************************************************/
void	fsk_parse(int fsk_bool[512], int j)
{
	int i, n=0;
	int fsk_call[100];

	for(i = 0; i < j-9; i++)
	{
		if(fsk_bool[i] == 0 && fsk_bool[i+9] == 1)
		{
			fsk_call[n] = (fsk_bool[i+1]) + (fsk_bool[i+2]<<1) + (fsk_bool[i+3]<<2) + (fsk_bool[i+4]<<3);
			n++;

			fsk_call[n] = fsk_bool[i+5] + (fsk_bool[i+6]<<1) + (fsk_bool[i+7]<<2) + (fsk_bool[i+8]<<3);
			n++;

			i = i+9;
		}

		else if(fsk_bool[i] == 0 && fsk_bool[i+8] == 1)
		{
			fsk_call[n] = (fsk_bool[i]) + (fsk_bool[i+1]<<1) + (fsk_bool[i+2]<<2) + (fsk_bool[i+3]<<3);
			n++;

			fsk_call[n] = fsk_bool[i+4] + (fsk_bool[i+5]<<1) + (fsk_bool[i+6]<<2) + (fsk_bool[i+7]<<3);
			n++;

			i = i+8;
		}

		else if(fsk_bool[i] == 0 && n)
		{
			fsk_call[n] = (fsk_bool[i+1]) + (fsk_bool[i+2]<<1) + (fsk_bool[i+3]<<2) + (fsk_bool[i+4]<<3);
			n++;

			fsk_call[n] = fsk_bool[i+5] + (fsk_bool[i+6]<<1) + (fsk_bool[i+7]<<2) + (fsk_bool[i+8]<<3);

			if(3 == fsk_call[n] || 11 == fsk_call[n])
			{
				n++;
				i = i+9;
			}
			else 
				n--;
		}
	}
	printf("\nfsk_call data length = %d\n", n);

	if(fsk_call[0] == 4 || fsk_call[1] == 0)
	{
		if(fsk_call[3])
			 fsk_call[3] = 1;

		for(i=0; i < (fsk_call[2] + fsk_call[3]*16 - 8); i++)
		{
			Fsk_CID[i] = fsk_call[i*2 + 20];
			printf("%d ", Fsk_CID[i]);
		}
		Fsk_CID_Len = (fsk_call[2] + fsk_call[3]*16 - 8);

	}
	else if(fsk_call[1] == 8 &&  fsk_call[22] == 2)
	{
		for(i=0; i < fsk_call[24]; i++)
		{
			Fsk_CID[i] = fsk_call[i*2 +26];	
			printf("%d ", Fsk_CID[i]);
		}
		Fsk_CID_Len = fsk_call[24];
	}
	else if(fsk_call[1] == 8 &&  fsk_call[24] == 2)
	{
		for(i=0; i < fsk_call[26]; i++)
		{
			Fsk_CID[i] = fsk_call[i*2 +28];	
			printf("%d ", Fsk_CID[i]);
		}
		Fsk_CID_Len = fsk_call[26];
	}
	
	printf("\n================================================================================================= \n");

	if(n)
	{
		zzl_g_finish_flag = TRUE;
		for(i = 0; i < n; i++)
		{
			printf("%d ", fsk_call[i]);
		}
		printf("\n================================================================================================ \n");
	}

}

/*****************************************************************************/
//  Description : add data into buffer and start parse fsk
//  Global resource dependence : g_fsk_buff
//  input: buffer--data buffer pointer
//         count--data count    signed short  *    int
//  output: none
//  return: TRUE--success
//          FALSE--failure
//  Author: mengfanyi
//  date:2013-12-16
//  Note: count > 20
/*****************************************************************************/
BOOLEAN Fsk_AddData(short int *buff, int count)
{
	int  	i, pre=0, k=0, j=0, sum=0; 
	int	c20=0,  lock_num = 0;
	int 	fsk_bool[512];
	int 	fsk_count[2048];

	if ((NULL == buff) || FSK_BUFFER_MIN > count || count > (FSK_BUFFER_LENGTH - FSK_BUFFER_START))
	{
		return FALSE;
	}
	
	memset(g_fsk_buff.buffer + FSK_BUFFER_START, 0, count * sizeof(short int));
	memset(g_fsk_buff.bitbuf + FSK_BUFFER_START, 0, count * sizeof(char));
	g_fsk_buff.count = count;
	memcpy(g_fsk_buff.buffer + FSK_BUFFER_START, buff, count * sizeof(short int));

	Fsk_SetFSKZeroBuffer(&g_fsk_buff);

	Fsk_Process();
	
	if(Pre_fsk_sign == 0 && Lim_ready_buff == 0)							// finding preamble ...
	{
		for(i = 1; i < count-1; i++)
		{
			if( (buff[i]) > (buff[i-1]) && (buff[i]) > (buff[i+1]) )
			{		
				if(i-pre > 2 && i-pre < 5)
				{
					Lim_c++;
					if(Lim_c == 50)						// here it is!
					{
						Pre_fsk_sign = 1;
						lock_num = i;
						printf("\n---start connecting buffering -2-----------------------------------------\n");
					}
				}else Lim_c = 0;
				pre = i;
			}
			else if( (buff[i]) < (buff[i-1]) && (buff[i]) < (buff[i+1]) )
			{		
				if(i-pre > 2 && i-pre < 5)
				{
					Lim_c++;
					if(Lim_c == 50)						// here it is!
					{
						Pre_fsk_sign = 1;
						lock_num = i;
						printf("\n---start connecting buffering -1-----------------------------------------\n");
					}
				}else Lim_c = 0;
				pre = i;
			}
		}

		if(Pre_fsk_sign == 1)							// save this buff
		{
		//	memcpy(Pre_buff, 	buff, 	count);
			for(i=0; i < count-lock_num; i++)
			{
				Pre_buff[i] = buff[i+lock_num];
			}
			Pre_buff_c = count-lock_num;
		}
	
		return TRUE;

    }
    else if(Pre_fsk_sign == 1)							// connecting buffer...
    {
	//	memcpy(Pre_buff + Pre_buff_c, 	buff, 	count);
		for(i=0; i < count; i++)
		{
			Pre_buff[Pre_buff_c + i] = buff[i];
		}
		Pre_buff_c += count;
		printf("\n               Pre_buff_c                              -------------%d----------------------------\n", Pre_buff_c);

		if(Pre_buff_c > 4000 - count)
		{
			Lim_ready_buff = 1;							// connecting finished
		}
		else 
			return TRUE;
	}// end of  if (Pre_fsk_sign == 1 && Pre_buff_c < count*4)


	if(Lim_ready_buff && Pre_fsk_sign)
	{
		printf("\n             Lim_ready                             -----------------------------------------\n");
		Lim_ready_buff = 0;
		Pre_fsk_sign = 0;
		pre = 0, Lim_c = 0;
//part 0
		for(i = 1; i <Pre_buff_c - 1 ; i++)
		{
			if( (Pre_buff[i]) > (Pre_buff[i-1]) && (Pre_buff[i]) > (Pre_buff[i+1]) )
			{		
				fsk_count[k++] = i-pre;
				pre = i;
			}
			else if( (Pre_buff[i]) < (Pre_buff[i-1]) && (Pre_buff[i]) < (Pre_buff[i+1]) )
			{		
				fsk_count[k++] = i-pre;
				pre = i;
			}
		}

		Pre_buff_c = 0;

		for(i = 0; i <= k; i++)
		{
//part 1
			if((fsk_count[i] > 2 && fsk_count[i] < 5))
			{
				Lim_c++;
				if(Lim_c == 50)
				{
					Lim_ready = 1;
					printf("\n---start connecting data -------------111 111-----------------------------\n");
				}
			}
			else
			{		
				Lim_c=0;
			}
// part 2
			if(Lim_ready == 1)
			{
				if(Lim_c == 0)
				{
					Lim_ready = 2;
					Fsk_lim[Fsk_n++] = fsk_count[i];
					printf("\n---start connecting data -------------first point-----------------------------\n");
				}
			}
			else if(Lim_ready == 2)
			{
				Fsk_lim[Fsk_n++] = fsk_count[i];
		/*		printf("%d ", fsk_count[i]);
				if(Fsk_n > 850)
				{
					Lim_ready = 0;
					lim_fsk_ready = 1;
					printf("\n----------- connecting data beyond top -------------------\n");
				}
		*/	}
		}	//end of for ...

					Lim_ready = 0;
					lim_fsk_ready = 1;

	}	//end of Lim_ready

	if(lim_fsk_ready)
    {
		printf("\nk =  %d------------------------Fsk_n = %d--------------\n", k, Fsk_n);
		for(i = 0; i < Fsk_n; i++)
		{
//part 1
			if(Fsk_lim[i] > 4)
			{
				Fsk_lim[i] = 4;
				Fsk_lim[i+1] = 3;		
			}

			if(Fsk_lim[i]==3 && Fsk_lim[i-1]<3 && Fsk_lim[i+1]<3)
				Fsk_lim[i] = 2;		

			if(Fsk_lim[i]>=3 && Fsk_lim[i+1]==1)
				Fsk_lim[i] = 2;		

//part 2
			if(Fsk_lim[i] < 3)       				// 2 1 2 2 2 1 2 ... 
			{
				sum = sum+1;
				c20 = c20+Fsk_lim[i];
				if(c20 >=20)					//2 1 2 2 2 2 1 22 2 1 2
				{
					c20 = 0;
					i = i-1;
				}

				if((sum==3) && (fsk_bool[j-1]==1))		//pre  4 3 2 1 2 2 3
				{
					fsk_bool[j++] = 0;
					sum = 0;
				}
				else if((sum==3) && (Fsk_lim[i+1] > 2))		//pre  4 3 2 1 2 2 3
				{
					fsk_bool[j++] = 0;
					sum = 0;
				}
				else if(sum==4)					//2 2 1 2
				{
					fsk_bool[j++] = 0;		
					sum = 0;
				}
				else if((sum >= 2) && (i==k-1))			//4 3 2 2 >|
				{
					fsk_bool[j++] = 0;		
					c20 = 0;
					sum = 0;
				}
				else if(Fsk_lim[i] + Fsk_lim[i+1] > 5) 		//2 4
				{
					fsk_bool[j++] = 1;		
					i = i+1;				//jump 1 step
					sum = 0;
					c20 = 0;
				}
			}
			else if(Fsk_lim[i] + Fsk_lim[i+1] > 5)			//4 2 or 3 3
			{	
				fsk_bool[j++] = 1;		
				i = i+1;					//jump 1 step
				sum = 0;
				c20 = 0;
			}
			else							//3 2  
				sum = 0;
		} //end of for Fsk_n
//part 3	
		Fsk_n = 0;

		for(i=0; j > i; i++)
		{
			printf("%d", fsk_bool[i]);
		}
		printf("\n finished decoding \n");
		fsk_parse(fsk_bool, j);						// call fsk_parse

    }//end of lim_fsk_ready

	lim_fsk_ready = 0;


	return TRUE;
}

/*****************************************************************************/
//  Description : free buffer
//  Global resource dependence : g_fsk_buff
//  input: none
//  output: none
//  return: none
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
void Fsk_FreeBuffer(void)
{
	int count;

	if ((g_fsk_buff.circlestart + g_fsk_buff.bitstart + FSK_MIN_COUNT +1) < (g_fsk_buff.count + FSK_BUFFER_START))
	{
		return;
	}

	count = g_fsk_buff.count + FSK_BUFFER_START - g_fsk_buff.circlestart + FSK_MIN_COUNT;

	memcpy(g_fsk_buff.buffer + FSK_BUFFER_START - count, g_fsk_buff.buffer + g_fsk_buff.circlestart -FSK_MIN_COUNT, count *sizeof(short int));
	memcpy(g_fsk_buff.bitbuf + FSK_BUFFER_START - count, g_fsk_buff.bitbuf + g_fsk_buff.circlestart -FSK_MIN_COUNT, count *sizeof(char));
	g_fsk_buff.start = FSK_BUFFER_START - count;
	g_fsk_buff.circlestart = g_fsk_buff.start +FSK_MIN_COUNT;

	count = FSK_BUFFER_LENGTH - FSK_BUFFER_START;
	memset(g_fsk_buff.buffer + FSK_BUFFER_START, 0, count * sizeof(short int));
	memset(g_fsk_buff.bitbuf + FSK_BUFFER_START, 0, count * sizeof(char));
	g_fsk_buff.count = 0;

}

/*****************************************************************************/
//  Description : get max value
//  Global resource dependence : 
//  input: none
//  output: none
//  return: none
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
short int Fsk_GetMaxAbsValue(short int *pbuffer,int start, int count)
{
	int i;
	short int val_max = 0;
	short int abs_val;

	if (pbuffer == NULL)
	{
		return 0;
	}

	for (i=0; i< count; i++)
	{
		abs_val = FSK_ABS(pbuffer[start + i]);
		if (abs_val > val_max)
		{
			val_max = abs_val;
		}
	}
	return val_max;
}

/*****************************************************************************/
//  Description : get max value
//  Global resource dependence : 
//  input: none
//  output: none
//  return: none
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
int Fsk_GetOrdinalDotCount(short int *pbuffer,int start, int maxno)
{
	int count;
	int i;

	if (NULL == pbuffer || start >= maxno)
	{
		return 0;
	}

	count = 0;
	if (pbuffer[start] > pbuffer[start -1])
	{
		for (i = 0; (start + i +1) < maxno; i++)
		{
			if (pbuffer[start +i +1] < pbuffer[start +i])
			{
				break;;
			}
			count++;
		}
		for (i = 0; i > -4; i--)
		{
			if (pbuffer[start +i] < pbuffer[start +i -1])
			{
				break;
			}
			count++;
		}
	}
	else
	{
		for (i = 0; (start + i +1) < maxno; i++)
		{
			if (pbuffer[start +i +1] > pbuffer[start +i])
			{
				break;;
			}
			count++;
		}
		for (i = 0; i > -4; i--)
		{
			if (pbuffer[start +i] > pbuffer[start +i -1])
			{
				break;
			}
			count++;
		}
	}

	return count;
}

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
char Fsk_SetFSKOneZeroData(short int prev_val, short int val)
{
	char rtn = 0;

	if (prev_val == 0)
	{
		if (0 > val)
		{
			rtn = 1;
		}
		else if (0 < val)
		{
			rtn = -1;
		}
		else
		{
			rtn = 0;
		}
	}
	else if (prev_val > 0)
	{
		if (val > 0)
		{
			rtn = 0;
		}
		else
		{
			rtn = 1;
		}
	}
	else
	{
		if (val < 0)
		{
			rtn = 0;
		}
		else
		{
			rtn = -1;
		}
	}
	return rtn;
}

/*****************************************************************************/
//  Description : set zero dot
//  Global resource dependence : 
//  input: buffer--buffer pointer, previous two dot,progress from dot 2
//  output: bit_buf--convert to data, buffer count is equel to bit buffer count
//  return: TRUE--success
//          FALSE--failure
//  Author: mengfanyi
//  date:2013-12-16
//  Note: buf size equel to bit_flag size
/*****************************************************************************/
BOOLEAN Fsk_SetFSKZeroData(short int *pbuffer, char *pbit_buf, int count)
{
	int i;
	short int val_max =0;
	short int abs_val, pre_val, cur_val;

	if (NULL == pbuffer || NULL == pbit_buf || 1 > count)
	{
		return FALSE;
	}
	
	for (i=1; i< count;i++)
	{
		abs_val = FSK_ABS(pbuffer[i]);
		if (abs_val > val_max)
		{
			val_max = abs_val;
		}
	}
	abs_val = FSK_ABS(pbuffer[0]);
	if (abs_val < val_max / 3)
	{
		pre_val = 0;
		/*
		if (pbuffer[0] > 0)
		{
			pbit_buf[1] = -1;
		}
		else
		{
			pbit_buf[1] = 1;
		}
		*/
	}
	else
	{
		pre_val = pbuffer[0];
	}

	for (i=1; i< count; i++)
	{
		/*
		abs_val = FSK_ABS(pbuffer[i -1]);
		if (abs_val < val_max/4)
		{
			pre_val = 0;
			if (pbuffer[i -2] > 0)
			{
				pbit_buf[i -1] = -1;
			}
			else
			{
				pbit_buf[i -1] = 1;
			}
		}
		else
		{
			pre_val = pbuffer[i -1];
		}
		*/
		abs_val = FSK_ABS(pbuffer[i]);
		if (abs_val < val_max / 3)
		{
			cur_val = 0;
		}
		else
		{
			cur_val = pbuffer[i];
		}
		if (pre_val == 0)
		{
			if (0 != cur_val)
			{
				pbit_buf[i] = 0;
			}
			else
			{
				pbit_buf[i] = 1;
			}
		}
		else if (pre_val > 0)
		{
			if (cur_val > 0)
			{
				pbit_buf[i] = 0;
			}
			else
			{
				pbit_buf[i] = 1;
			}
		}
		else
		{
			if (cur_val < 0)
			{
				pbit_buf[i] = 0;
			}
			else
			{
				pbit_buf[i] = -1;
			}
		}
		pre_val = cur_val;
	}
	return TRUE;
}

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
BOOLEAN Fsk_SetFSKZeroBuffer(Fsk_Buffer_T *pbuf)
{
	int i;
	short int pre_val, value;
	short int noi_val = 0;

	if (NULL == pbuf)
	{
		return FALSE;
	}

	if (pbuf->count > (FSK_BUFFER_LENGTH - FSK_BUFFER_START))
	{
		pbuf->count = FSK_BUFFER_LENGTH - FSK_BUFFER_START;
	}

	value = FSK_ABS(pbuf->buffer[FSK_BUFFER_START]);
	if (g_fsk_msg_hex.value_max < value)
	{
		g_fsk_msg_hex.value_max = value;
		/*
		noi_val = value * FSK_NOISE_SIGNAL_RADIO;
		if (g_fsk_msg_hex.noise_max < noi_val)
		{
			g_fsk_msg_hex.noise_max = noi_val;
		}
		*/
		g_fsk_msg_hex.noise_max = FSK_NOISE_DEFAULT_LEVEL;
	}
	if (value < g_fsk_msg_hex.noise_max)
	{
		value = 0;
		//pbuf->buffer[FSK_BUFFER_START] = 0;
	}
	else
	{
		value = pbuf->buffer[FSK_BUFFER_START];
	}

	if (pbuf->start < FSK_BUFFER_START)
	{
		pre_val = FSK_ABS(pbuf->buffer[FSK_BUFFER_START -1]);
		if (pre_val < g_fsk_msg_hex.noise_max)
		{
			pre_val = 0;
		}
		else
		{
			pre_val = pbuf->buffer[FSK_BUFFER_START -1];
		}
		if (pre_val == 0)
		{
			if (0 == value)
			{
				pbuf->bitbuf[FSK_BUFFER_START] = 1;
			}
			else
			{
				pbuf->bitbuf[FSK_BUFFER_START] = 0;
			}
		}
		else if (pre_val > 0)
		{
			if ( value > 0)
			{
				pbuf->bitbuf[FSK_BUFFER_START] = 0;
			}
			else
			{
				pbuf->bitbuf[FSK_BUFFER_START] = 1;
			}
		}
		else
		{
			if  (value < 0)
			{
				pbuf->bitbuf[FSK_BUFFER_START] = 0;
			}
			else
			{
				pbuf->bitbuf[FSK_BUFFER_START] = -1;
			}
		}
	}
	else
	{
		if (0 == value)
		{
			pbuf->bitbuf[FSK_BUFFER_START] = 1;
		}
		else
		{
			pbuf->bitbuf[FSK_BUFFER_START] = 0;
		}
	}

	pre_val = value;
	for (i=FSK_BUFFER_START +1; i<pbuf->count + FSK_BUFFER_START; i++)
	{
		value = FSK_ABS(pbuf->buffer[i]);
		if (g_fsk_msg_hex.value_max < value)
		{
			g_fsk_msg_hex.value_max = value;
			/*
			noi_val = value * FSK_NOISE_SIGNAL_RADIO;
			if (g_fsk_msg_hex.noise_max < noi_val)
			{
				g_fsk_msg_hex.noise_max = noi_val;
			}
			*/
		}
		/*
		pre_val = FSK_ABS(pbuf->buffer[i -1]);
		if (pre_val < g_fsk_msg_hex.noise_max)
		{
			pre_val = 0;
		}
		else
		{
			pre_val = pbuf->buffer[i- 1];
		}
		*/
		if (value < g_fsk_msg_hex.noise_max)
		{
			value = 0;
			//pbuf->buffer[i] = 0;
		}
		else
		{
			value = pbuf->buffer[i];
		}

		if (pre_val == 0)
		{
			if (0 != value)
			{
				pbuf->bitbuf[i] = 0;
			}
			else
			{
				pbuf->bitbuf[i] = 1;
			}
		}
		else if (pre_val > 0)
		{
			if (value > 0)
			{
				pbuf->bitbuf[i] = 0;
			}
			else
			{
				pbuf->bitbuf[i] = 1;
			}
		}
		else
		{
			if (value < 0)
			{
				pbuf->bitbuf[i] = 0;
			}
			else
			{
				pbuf->bitbuf[i] = -1;
			}
		}
		pre_val = value;
	}
	return TRUE;
}

/*****************************************************************************/
//  Description : 
//  Global resource dependence : g_fsk_buff
//  input: pbuf--buffer pointer
//         start--start position
//  output: 
//  return: >
//  Author: mengfanyi
//  date:2013-12-16
//  Note:  
/*****************************************************************************/
BOOLEAN Fsk_AjustZeroDot(Fsk_Buffer_T *pbuf, int start, int count)
{
	int j;
	short int abs_val, abs_val1, abs_val2;
	short int pre_val, cur_val;
	short int val_max = 0;

	if (NULL == pbuf)
	{
		return FALSE;
	}

	for (j=0; j<count; j++)
	{
		if (pbuf->bitbuf[start +j] != 0 && pbuf->bitbuf[start +j +1] == 0)
		{
			break;
		}
	}
	if (j >= count)
	{
		return FALSE;
	}

	val_max = Fsk_GetMaxAbsValue(pbuf->buffer,start -1, count + 1);
	abs_val1 = FSK_ABS(pbuf->buffer[start +j]);
	abs_val2 = FSK_ABS(pbuf->buffer[start +j +1]);
	if (abs_val1 > (val_max /2) && abs_val2 > (val_max /2))
	{
		abs_val = FSK_ABS(pbuf->buffer[start +j -1]);
		if (abs_val < val_max /3)
		{
			abs_val = FSK_ABS(pbuf->buffer[start +j -2]);
			if (abs_val < g_fsk_msg_hex.noise_max)
			{
				pre_val = 0;
			}
			else
			{
				pre_val = pbuf->buffer[start +j -2];
			}
			if (pre_val < 0)
			{
				pbuf->bitbuf[start +j -1] = -1;
			}
			else
			{
				pbuf->bitbuf[start +j -1] = 1;
			}

			pbuf->bitbuf[start +j] = 0;
		}
		if ((start +j +2) < (pbuf->count + FSK_RESERVE_LENGTH))
		{
			abs_val = FSK_ABS(pbuf->buffer[start +j +2]);
			if (abs_val < val_max /3)
			{
				abs_val = FSK_ABS(pbuf->buffer[start +j +1]);
				if (abs_val < g_fsk_msg_hex.noise_max)
				{
					pre_val = 0;
				}
				else
				{
					pre_val = pbuf->buffer[start +j +1];
				}
				if (pre_val < 0)
				{
					pbuf->bitbuf[start +j +2] = -1;
				}
				else
				{
					pbuf->bitbuf[start +j +2] = 1;
				}

				if ((start +j +3) < (pbuf->count + FSK_RESERVE_LENGTH))
				{
					abs_val = FSK_ABS(pbuf->buffer[start +j +3]);
					if (abs_val < g_fsk_msg_hex.noise_max)
					{
						pre_val = 0;
					}
					else
					{
						pre_val = pbuf->buffer[start +j +3];
					}
					if (pre_val == 0)
					{
						pbuf->bitbuf[start +j +3] = 1;
					}
					else
					{
						pbuf->bitbuf[start +j +3] = 0;
					}
				}
			}
		}
	}
	return TRUE;
}

/*****************************************************************************/
//  Description : 
//  Global resource dependence : g_fsk_buff
//  input: pbuf--buffer pointer
//         start--start position
//  output: 
//  return: >
//  Author: mengfanyi
//  date:2013-12-16
//  Note:  
/*****************************************************************************/
BOOLEAN Fsk_UncondAjustZeroDot(Fsk_Buffer_T *pbuf, int start, int count)
{
	int j;
	short int abs_val, abs_val1, abs_val2;
	short int pre_val, cur_val;
	short int val_max = 0;

	if (NULL == pbuf)
	{
		return FALSE;
	}

	val_max = Fsk_GetMaxAbsValue(pbuf->buffer,start, count);
	for (j=0; j< count-1; j++)
	{
		abs_val = FSK_ABS(pbuf->buffer[start +j]);
		if (abs_val < val_max /4)
		{
			abs_val = FSK_ABS(pbuf->buffer[start +j -1]);
			if (abs_val < g_fsk_msg_hex.noise_max)
			{
				cur_val = 0;
			}
			else
			{
				cur_val = pbuf->buffer[start +j -1];
			}
			if (cur_val < 0)
			{
				pbuf->bitbuf[start +j] = -1;
			}
			else
			{
				pbuf->bitbuf[start +j] = 1;
			}
			abs_val = FSK_ABS(pbuf->buffer[start +j +1]);
			if (abs_val < g_fsk_msg_hex.noise_max)
			{
				cur_val = 0;
			}
			else
			{
				cur_val = pbuf->buffer[start +j +1];
			}
			if (cur_val != 0)
			{
				pbuf->bitbuf[start +j +1] = 0;
			}
			else
			{
				pbuf->bitbuf[start +j +1] = 1;
			}

		}
	}
	return TRUE;
}

/*****************************************************************************/
//  Description : parse data to bit
//  Global resource dependence : g_fsk_buff
//  input: pbuf--buffer pointer
//         start--start position
//  output: bit_ch--bit value pointer
//  return: >0--success,process dot count
//          -1--buffer overflow
//          -2--error
//  Author: mengfanyi
//  date:2013-12-16
//  Note:  
/*****************************************************************************/
int Fsk_ParseOneBit(Fsk_Buffer_T *pbuf, int start, char *bit_ch, int *adjcount)
{
#ifdef FSK_DEBUG
	static int line_count = 0;
#endif
	int i, j;
	int rtn = -1;
	int adjust_count = 0;
	int zero_count,nzero_count,connzero_count, nzero_max;
	//short int tmp_bitbuf[FSK_MIN_COUNT+1];
	short int abs_val, abs_val1, abs_val2, val_max = 0;
	short int pre_val, cur_val;


	if (NULL == pbuf)
	{
		return -2;
	}

	zero_count = 0;
	nzero_count = 0;
	connzero_count = 0;
	nzero_max = 0;

	for (i=0;i<FSK_MIN_COUNT;i++)
	{
		if ((start+i) >= (pbuf->count + FSK_BUFFER_START))
		{
			return -1;
		}
		if (pbuf->bitbuf[start +i] != 0)
		{
			if (nzero_count > nzero_max && zero_count > 0)
			{
				nzero_max = nzero_count;
			}
			if (nzero_count > connzero_count)
			{
				connzero_count = nzero_count;
			}
			nzero_count = 0;

			zero_count++;
		}
		else
		{
			nzero_count++;
		}
	}

	//ajust
	if (zero_count < 2)
	{
		if ((start+i) >= (pbuf->count + FSK_BUFFER_START))
		{
			return -1;
		}

		if (pbuf->bitbuf[start + i] != 0 && pbuf->bitbuf[start + i -1] == 0  && pbuf->bitbuf[start + i -2] == 0)
		{
			if (nzero_count > nzero_max && zero_count > 0)
			{
				nzero_max = nzero_count;
			}
			if (nzero_count > connzero_count)
			{
				connzero_count = nzero_count;
			}
			nzero_count = 0;

			nzero_max = 2;
			zero_count++;
			i++;
		}
		else if ((pbuf->bitbuf[start -1] != 0) && pbuf->bitbuf[start] == 0 && pbuf->bitbuf[start +1] == 0)
		{
			zero_count++;
			nzero_max =2;
		}
	}
#if 0
	else if (zero_count == 3 && nzero_max == 1 && g_fsk_msg_hex.con1bit_count > 2)
	{
		for (j=0; j<FSK_MIN_COUNT; j++)
		{
			if (pbuf->bitbuf[start +j] != 0 && pbuf->bitbuf[start +j +1] == 0)
			{
				break;
			}
		}
		val_max = Fsk_GetMaxAbsValue(pbuf->buffer,start -1, FSK_MIN_COUNT + 1);
		abs_val1 = FSK_ABS(pbuf->buffer[start +j]);
		abs_val2 = FSK_ABS(pbuf->buffer[start +j +1]);
		if (abs_val1 > (val_max *2 /3) && abs_val2 > (val_max *2 /3))
		{
			abs_val = FSK_ABS(pbuf->buffer[start +j -1]);
			if (abs_val < val_max /3)
			{
				/*
				abs_val = FSK_ABS(pbuf->buffer[start +j -2]);
				if (abs_val < g_fsk_msg_hex.noise_max)
				{
					pre_val = 0;
				}
				else
				{
					pre_val = pbuf->buffer[start +j -2];
				}
				*/
				pre_val = pbuf->buffer[start +j -2];
				if (pre_val < 0)
				{
					pbuf->bitbuf[start +j -1] = -1;
				}
				else
				{
					pbuf->bitbuf[start +j -1] = 1;
				}

				pbuf->bitbuf[start +j] = 0;
				
				if (j > 0)
				{
					nzero_max++;
				}
				else
				{
					zero_count = 0;
					nzero_count = 0;
					connzero_count = 0;
					nzero_max = 0;
					for (i=0;i<FSK_MIN_COUNT;i++)
					{
						if (pbuf->bitbuf[start +i] != 0)
						{
							if (nzero_count > nzero_max && zero_count > 0)
							{
								nzero_max = nzero_count;
							}
							if (nzero_count > connzero_count)
							{
								connzero_count = nzero_count;
							}
							nzero_count = 0;

							zero_count++;
						}
						else
						{
							nzero_count++;
						}
					}
				}
			}
		}
	}
#endif

	if (nzero_count > connzero_count)
	{
		connzero_count = nzero_count;
	}

	//ajust
	if (zero_count == 2 && nzero_max < 2 && connzero_count < 3 && i<(FSK_MIN_COUNT+1))
	{
		val_max = Fsk_GetMaxAbsValue(pbuf->buffer,start -1, FSK_MIN_COUNT + 1);
		for (j=0; j<FSK_MIN_COUNT -2; j++)
		{
			if (pbuf->bitbuf[start +j] != 0 && pbuf->bitbuf[start +j +1] == 0  && pbuf->bitbuf[start +j +2] != 0)
			{
				abs_val1 = FSK_ABS(pbuf->buffer[start +j]);
				abs_val2 = FSK_ABS(pbuf->buffer[start +j +1]);
				if (abs_val1 > (val_max *3 /5) && abs_val2 > (val_max *3 /5))
				{
					abs_val1 = FSK_ABS(pbuf->buffer[start +j -1]);
					abs_val2 = FSK_ABS(pbuf->buffer[start +j +2]);
					if (abs_val1 < (val_max *2 /5) && abs_val2 < (val_max *2 /5))
					{
						abs_val1 = FSK_ABS(pbuf->buffer[start +j] - pbuf->buffer[start +j -1]);
						abs_val2 = FSK_ABS(pbuf->buffer[start +j +1] - pbuf->buffer[start +j +2]);
						if (abs_val1 < (val_max *5 /4) && abs_val2 < (val_max *5 /4))
						{
							abs_val1 = FSK_ABS(pbuf->buffer[start +j -2]);
							if (abs_val1 < g_fsk_msg_hex.noise_max)
							{
								pre_val = 0;
							}
							else
							{
								pre_val = pbuf->buffer[start +j -2];
							}
							if (pre_val < 0)
							{
								pbuf->bitbuf[start +j -1] = -1;
							}
							else
							{
								pbuf->bitbuf[start +j -1] = 1;
							}
							pbuf->bitbuf[start +j] = 0;

							abs_val1 = FSK_ABS(pbuf->buffer[start +j +1]);
							if (abs_val1 < g_fsk_msg_hex.noise_max)
							{
								cur_val = 0;
							}
							else
							{
								cur_val = pbuf->buffer[start +j +1];
							}
							if (cur_val < 0)
							{
								pbuf->bitbuf[start +j +2] = -1;
							}
							else
							{
								pbuf->bitbuf[start +j +2] = 1;
							}
							nzero_max++;
						}
					}
				}
			}
		}	//for (j)
		if (nzero_max < 2)
		{
			if ((start+i) >= (pbuf->count + FSK_BUFFER_START))
			{
				return -1;
			}
			if (pbuf->bitbuf[start +i] == 0)
			{
				nzero_count=2;
				i++;
				if (nzero_count > nzero_max )
				{
					nzero_max = nzero_count;
				}
			}
			else
			{
				if (nzero_count > nzero_max )
				{
					nzero_max = nzero_count;
				}
				if (nzero_max < 2 && connzero_count < 2)
				{
					zero_count++;
					i++;
				}
				else if (nzero_max < 2 && pbuf->bitbuf[start -1] != 0)
				{
					val_max = Fsk_GetMaxAbsValue(pbuf->buffer,start, FSK_MIN_COUNT);
					if (FSK_ABS(pbuf->buffer[start]) < val_max /4)
					{
						nzero_max = 2;
					}
					else
					{
						zero_count++;
						i++;
					}
				}
				else
				{
					nzero_max = 2;
					i++;
				}
			}
		}
	}


	//sync
	adjust_count = 0;
	if (zero_count > 2 && ((nzero_max >= 1) || (connzero_count > 1) || (pbuf->bitbuf[start] == 0 && pbuf->bitbuf[start -1] == 0 && pbuf->bitbuf[start -2] == 0)) && g_fsk_msg_hex.con1bit_count >= 2)
	{
#if 1
		if (nzero_max == 1 && zero_count == 3)
		{
			for (j=0; j<FSK_MIN_COUNT -2; j++)
			{
				if (pbuf->bitbuf[start +j] != 0 && pbuf->bitbuf[start +j +1] == 0  && pbuf->bitbuf[start +j +2] != 0)
				{
					break;
				}
			}
			val_max = Fsk_GetMaxAbsValue(pbuf->buffer,start -1, FSK_MIN_COUNT + 1);
#if 0
			abs_val1 = FSK_ABS(pbuf->buffer[start +j] - pbuf->buffer[start +j -1]);
			abs_val2 = FSK_ABS(pbuf->buffer[start +j +1] - pbuf->buffer[start +j +2]);
			if (abs_val1 < (val_max *5 /4) && abs_val2 < (val_max *5 /4))
			{
				adjust_count = j +2;
				FSK_PRINT_BIT("\nstart=%d, adjust count=%d\n",start, adjust_count);
			}
#endif
#if 1
			abs_val1 = FSK_ABS(pbuf->buffer[start +j]);
			abs_val2 = FSK_ABS(pbuf->buffer[start +j +1]);
			if (abs_val1 > (val_max *2 /3) && abs_val2 > (val_max *2 /3))
			{
				abs_val = FSK_ABS(pbuf->buffer[start +j -1]);
				if (abs_val < val_max /3)
				{
					abs_val = FSK_ABS(pbuf->buffer[start +j -2]);
					if (abs_val < g_fsk_msg_hex.noise_max)
					{
						pre_val = 0;
					}
					else
					{
						pre_val = pbuf->buffer[start +j -2];
					}
					if (pre_val < 0)
					{
						pbuf->bitbuf[start +j -1] = -1;
					}
					else
					{
						pbuf->bitbuf[start +j -1] = 1;
					}

					pbuf->bitbuf[start +j] = 0;
				
					if (j > 0)
					{
						nzero_max++;
					}
					else
					{
						zero_count = 0;
						nzero_count = 0;
						connzero_count = 0;
						nzero_max = 0;
						for (i=0;i<FSK_MIN_COUNT;i++)
						{
							if (pbuf->bitbuf[start +i] != 0)
							{
								if (nzero_count > nzero_max && zero_count > 0)
								{
									nzero_max = nzero_count;
								}
								if (nzero_count > connzero_count)
								{
									connzero_count = nzero_count;
								}
								nzero_count = 0;

								zero_count++;
							}
							else
							{
								nzero_count++;
							}
						}
					}
				}
			}
			//Fsk_SetFSKZeroData(pbuf->buffer + start -2, pbuf->bitbuf + start -2, FSK_MIN_COUNT + 2);

			for (j= 0; j< FSK_MIN_COUNT; j++)
			{
				if ((pbuf->bitbuf[start +j] == 0) && (pbuf->bitbuf[start +j -1] == 0))
				{
					break;
				}
			}
			if (j >= FSK_MIN_COUNT)
			{
				//Fsk_AjustOneBit(pbuf, start -4, FSK_MIN_COUNT -1);
				for (j=0; j<FSK_MIN_COUNT; j++)
				{
					if (pbuf->bitbuf[start +j] != 0)
					{
						break;
					}
				}
				for (; j > -FSK_MIN_COUNT +1; j--)
				{
					if (pbuf->bitbuf[start +j -1] != 0 && pbuf->bitbuf[start +j] == 0 && pbuf->bitbuf[start +j +1] != 0)
					{
						if (Fsk_GetOrdinalDotCount(pbuf->buffer, start +j -1, start + i) >= 3)
						{
							Fsk_AjustZeroDot(pbuf, start +j -1, FSK_MIN_COUNT -2);
						}
					}
				}
				//if (j > -FSK_MIN_COUNT +1)
				//{
				//	Fsk_AjustZeroDot(pbuf, start +j -1, FSK_MIN_COUNT -2);
				//}
				
				for (j= 0; j> -FSK_MIN_COUNT; j--)
				{
					if ((pbuf->bitbuf[start +j] == 0) && (pbuf->bitbuf[start +j -1] == 0))
					{
						if ((FSK_ABS(pbuf->buffer[start +j -2]) >= val_max /3) || (Fsk_GetOrdinalDotCount(pbuf->buffer, start +j -1, start + i) >= 3))
						{
							break;
						}
					}
				}
			}
			while (pbuf->bitbuf[start +j] == 0 && j <FSK_MIN_COUNT)
			{
				j++;
			}
			
			if ((j != 0) && (j < FSK_MIN_COUNT) && (j > -FSK_MIN_COUNT))
			{
				if (Fsk_GetOrdinalDotCount(pbuf->buffer, start +j, start + i) > 2)
				{
					adjust_count = j +1;
				}
				else
				{
					adjust_count = j;
				}
				//adjust_count = j+1;
				//adjcount = nn;
				FSK_PRINT_BIT("\nstart=%d, adjust count=%d\n",start, adjust_count);
#if 0
				if ((j < FSK_MIN_COUNT) && (j > -FSK_MIN_COUNT) && (pbuf->bitbuf[start +j] != 0))
				{
					val_max = Fsk_GetMaxAbsValue(pbuf->buffer,start -1, FSK_MIN_COUNT + 1);
					if (FSK_ABS(pbuf->buffer[start +j]) < val_max /3 && FSK_SAMESIGN(pbuf->buffer[start + j -1], pbuf->buffer[start +j]))
					{
						adjust_count = j +1;
					}
					else
					{
						adjust_count = j;
					}
					//adjust_count = j+1;
					//adjcount = nn;
					FSK_PRINT_BIT("\nstart=%d, adjust count=%d\n",start, adjust_count);
				}
#endif
			}
#endif
		}
		else if (nzero_max == 1 && zero_count == 4  && (g_fsk_msg_hex.con1bit_count >= 2))
		{
			val_max = Fsk_GetMaxAbsValue(pbuf->buffer,start -1, FSK_MIN_COUNT + 1);
			for (j= -1; j > -FSK_MIN_COUNT; j--)
			{
				if (FSK_ABS(pbuf->buffer[start +j]) < val_max /4)
				{
					abs_val = FSK_ABS(pbuf->buffer[start +j -1]);
					if (abs_val < g_fsk_msg_hex.noise_max)
					{
						pre_val = 0;
					}
					else
					{
						pre_val = pbuf->buffer[start +j -1];
					}
					if (pbuf->buffer[start +j -1] == 0)
					{
						pbuf->bitbuf[start +j] = 1;
					}
					else
					{
						pbuf->bitbuf[start +j] = 0;
					}

					abs_val = FSK_ABS(pbuf->buffer[start +j +1]);
					if (abs_val < g_fsk_msg_hex.noise_max)
					{
						cur_val = 0;
					}
					else
					{
						cur_val = pbuf->buffer[start +j +1];
					}
					if (cur_val > 0)
					{
						pbuf->bitbuf[start +j +1] = -1;
					}
					else
					{
						pbuf->bitbuf[start +j +1] = 1;
					}
				}
			}
			for (j= 0; j > -FSK_MIN_COUNT +1; j--)
			{
				if (pbuf->bitbuf[start +j] == 0 && pbuf->bitbuf[start +j -1] == 0)
				{
					abs_val = FSK_ABS(pbuf->buffer[start +j -2]);
					if (abs_val >= val_max /4)
					{
						break;
					}
					else
					{
						if (pbuf->buffer[start +j -1] > 0)
						{
							pbuf->buffer[start +j -1] = -1;
						}
						else
						{
							pbuf->buffer[start +j -1] = 1;
						}
						pbuf->bitbuf[start +j -2] = 0; 
					}
				}
			}
			if (j > -FSK_MIN_COUNT +1)
			{
				while (pbuf->bitbuf[start +j] == 0 && j < 0)
				{
					j++;
				}
				adjust_count = j;
				FSK_PRINT_BIT("\nstart=%d, adjust count=%d\n",start, adjust_count);
			}
		}
		else 
#endif
#if 1
		if (connzero_count < 2 && (pbuf->bitbuf[start -1] == 0) && (pbuf->bitbuf[start] == 0))
		{
			j=0;
			while ((pbuf->bitbuf[start +j] == 0) && (j < FSK_MIN_COUNT))
			{
				j++;
			}
			adjust_count = j;
			//adjust_count = j+1;
			//adjcount = j;
			FSK_PRINT_BIT("start=%d, adjust count=%d \n", start, adjust_count);
		}
		else if (connzero_count > 1)
		{
			j=0;
			if (pbuf->bitbuf[start] != 0)
			{
				j++;
			}
		
			while (j < FSK_MIN_COUNT && (start +j + 1) < (g_fsk_buff.count + FSK_RESERVE_LENGTH))
			{
				if ((pbuf->bitbuf[start +j] == 0) && (pbuf->bitbuf[start +j +1] == 0))
				{
					break;
				}
				j++;
			}
			val_max = Fsk_GetMaxAbsValue(pbuf->buffer,start +j -1, FSK_MIN_COUNT);
			if (FSK_ABS(pbuf->buffer[start +j -1]) > val_max /4)
			{
				while (pbuf->bitbuf[start +j] == 0 && j < FSK_MIN_COUNT)
				{
					j++;
				}

				if ((j < 4) && ((pbuf->bitbuf[start] == 0) || (j > 2 && pbuf->bitbuf[start] != 0 && pbuf->bitbuf[start +i] == 0))
					 || (g_fsk_msg_hex.con1bit_count >= FSK_MARK_SIGNL_COUNT && j<FSK_MIN_COUNT))
				{
					adjust_count = j;
					//adjust_count = j+1;
					//adjcount = nn;
					FSK_PRINT_BIT("\nstart=%d, adjust count=%d\n",start, adjust_count);
				}
			}
			/*
			else if ((j >= FSK_MIN_COUNT) && (pbuf->bitbuf[start] != 0))
			{
				if ((pbuf->bitbuf[start -1] != 0)) || (pbuf->bitbuf[start -2] != 0))
				{
					adjust_count = -1;
					FSK_PRINT_MSG_WORD("start=%d, adjust count=%d\n",start, adjust_count);
				}
			}
			*/
		}
#endif
	}
	else if ((zero_count > 2) && (connzero_count < 2 || nzero_max < 2) && (g_fsk_msg_hex.con1bit_count >= FSK_MARK_SIGNL_COUNT))
	{
#if 0
		for (j=0; j< FSK_MIN_COUNT;j++)
		{
			abs_val = FSK_ABS(pbuf->buffer[start + j]);
			if (abs_val > val_max)
			{
				val_max = abs_val;
			}
		}
		abs_val = FSK_ABS(pbuf->buffer[start -1]);
		if (abs_val < val_max / 3)
		{
			pre_val = 0;
			if (pbuf->buffer[start -2] > 0)
			{
				pbuf->bitbuf[start -1] = -1;
			}
			else
			{
				pbuf->bitbuf[start -1] = 1;
			}
		}
		else
		{
			pre_val = pbuf->buffer[start -1];
		}

		for (j=0; j< FSK_MIN_COUNT; j++)
		{
			/*
			abs_val = FSK_ABS(pbuf->buffer[start + j -1]);
			if (abs_val < val_max/4)
			{
				pre_val = 0;
				if (pbuf->buffer[start + j -2] > 0)
				{
					pbuf->bitbuf[start + j -1] = -1;
				}
				else
				{
					pbuf->bitbuf[start + j -1] = 1;
				}
			}
			else
			{
				pre_val = pbuf->buffer[start + j -1];
			}
			*/
			abs_val = FSK_ABS(pbuf->buffer[start + j]);
			if (abs_val < val_max / 3)
			{
				cur_val = 0;
			}
			else
			{
				cur_val = pbuf->buffer[start + j];
			}
			if (pre_val == 0)
			{
				if (0 != cur_val)
				{
					pbuf->bitbuf[start + j] = 0;
				}
				else
				{
					pbuf->bitbuf[start + j] = 1;
				}
			}
			else if (pre_val > 0)
			{
				if (cur_val > 0)
				{
					pbuf->bitbuf[start + j] = 0;
				}
				else
				{
					pbuf->bitbuf[start + j] = 1;
				}
			}
			else
			{
				if (cur_val < 0)
				{
					pbuf->bitbuf[start + j] = 0;
				}
				else
				{
					pbuf->bitbuf[start + j] = -1;
				}
			}
			pre_val = cur_val;
		}
#endif
		Fsk_SetFSKZeroData(pbuf->buffer + start -2, pbuf->bitbuf + start -2, FSK_MIN_COUNT + 2);

		for (j= 0; j< FSK_MIN_COUNT; j++)
		{
			if ((pbuf->bitbuf[start +j] == 0) && (pbuf->bitbuf[start +j -1] == 0))
			{
				break;
			}
		}
		if (j >= FSK_MIN_COUNT)
		{
			for (j= 0; j> -FSK_MIN_COUNT; j--)
			{
				if ((pbuf->bitbuf[start +j] == 0) && (pbuf->bitbuf[start +j -1] == 0))
				{
					break;
				}
			}
		}
		while (pbuf->bitbuf[start +j] == 0 && j <FSK_MIN_COUNT)
		{
			j++;
		}

		if ((j < FSK_MIN_COUNT) && (j > -FSK_MIN_COUNT) && (pbuf->bitbuf[start +j] != 0))
		{
			adjust_count = j;
			//adjust_count = j+1;
			//adjcount = nn;
			FSK_PRINT_BIT("\nstart=%d, adjust count=%d\n",start, adjust_count);
		}
	}
	else if ((zero_count == 2) && (connzero_count >1 && nzero_max < 2) && (g_fsk_msg_hex.con1bit_count >= FSK_MARK_SIGNL_COUNT))
	{
#if 0
		val_max = Fsk_GetMaxAbsValue(pbuf->buffer,start -1, FSK_MIN_COUNT + 1);
		for (j=0; j<FSK_MIN_COUNT -2; j++)
		{
			if (pbuf->bitbuf[start +j] != 0 && pbuf->bitbuf[start +j +1] == 0  && pbuf->bitbuf[start +j +2] != 0)
			{
				abs_val1 = FSK_ABS(pbuf->buffer[start +j]);
				abs_val2 = FSK_ABS(pbuf->buffer[start +j +1]);
				if (abs_val1 > (val_max *3 /5) && abs_val2 > (val_max *3 /5))
				{
					abs_val1 = FSK_ABS(pbuf->buffer[start +j -1]);
					abs_val2 = FSK_ABS(pbuf->buffer[start +j +2]);
					if (abs_val1 < (val_max *2 /5) && abs_val2 < (val_max *2 /5))
					{
						abs_val1 = FSK_ABS(pbuf->buffer[start +j] - pbuf->buffer[start +j -1]);
						abs_val2 = FSK_ABS(pbuf->buffer[start +j +1] - pbuf->buffer[start +j +2]);
						if (abs_val1 < (val_max *5 /4) && abs_val2 < (val_max *5 /4))
						{
							abs_val1 = FSK_ABS(pbuf->buffer[start +j -2]);
							if (abs_val1 < g_fsk_msg_hex.noise_max)
							{
								pre_val = 0;
							}
							else
							{
								pre_val = pbuf->buffer[start +j -2];
							}
							if (pre_val < 0)
							{
								pbuf->bitbuf[start +j -1] = -1;
							}
							else
							{
								pbuf->bitbuf[start +j -1] = 1;
							}
							pbuf->bitbuf[start +j] = 0;

							abs_val1 = FSK_ABS(pbuf->buffer[start +j +1]);
							if (abs_val1 < g_fsk_msg_hex.noise_max)
							{
								cur_val = 0;
							}
							else
							{
								cur_val = pbuf->buffer[start +j +1];
							}
							if (cur_val < 0)
							{
								pbuf->bitbuf[start +j +2] = -1;
							}
							else
							{
								pbuf->bitbuf[start +j +2] = 1;
							}

						}
					}
				}
			}
		}	//for (j)
#endif
		for (j= 0; j< FSK_MIN_COUNT; j++)
		{
			if (pbuf->bitbuf[start +j] != 0)
			{
				break;
			}
		}
		if ((j < FSK_MIN_COUNT) && (j > 1) && (pbuf->bitbuf[start +j] != 0))
		{
			adjust_count = j;
			//adjust_count = j+1;
			//adjcount = nn;
			FSK_PRINT_BIT("\nstart=%d, adjust count=%d\n",start, adjust_count);
		}
	}
	else if ((zero_count == 2) && (nzero_max == 2) && (g_fsk_msg_hex.con1bit_count >= FSK_MARK_SIGNL_COUNT))
	{
		if (pbuf->bitbuf[start] == 0 && pbuf->bitbuf[start +i -1] == 0 && pbuf->bitbuf[start +i] != 0)
		{
			adjust_count = 4;
			//adjust_count = j+1;
			//adjcount = nn;
			FSK_PRINT_BIT("\nstart=%d, adjust count=%d\n",start, adjust_count);
		}
	}
	*adjcount = adjust_count;
	if (adjust_count != 0)
	{
		g_fsk_msg_hex.con1bit_count = 0;
		//return adjust_count;
		return 1;
	}



	if (zero_count == 3)
	{
		if ((start+i) >= (pbuf->count + FSK_BUFFER_START))
		{
			return -1;
		}

		if (nzero_max > 1)
		{
			if (pbuf->bitbuf[start +i] != 0)
			{
				zero_count++;
				i++;
			}
			else if (pbuf->bitbuf[start -1] != 0)
			{
				zero_count++;
			}
			else
			{
				if (pbuf->bitbuf[start] != 0)
				{
					zero_count--;
				}
				else if (pbuf->bitbuf[start +i +1] == 0)
				{
					zero_count--;
					i++;
				}
				else if (pbuf->bitbuf[start -2] == 0)
				{
					zero_count--;
				}
				else
				{
					zero_count++;
					i++;
				}
			}
		}
		else
		{
			if ((pbuf->bitbuf[start] == 0) && (pbuf->bitbuf[start +i] == 0))
			{
				i++;
			}
			else if (nzero_max == 1)
			{
				val_max = Fsk_GetMaxAbsValue(pbuf->buffer,start -1, FSK_MIN_COUNT + 1);
				for (j=0; j<FSK_MIN_COUNT -2; j++)
				{
					if (pbuf->bitbuf[start +j] != 0 && pbuf->bitbuf[start +j +1] == 0  && pbuf->bitbuf[start +j +2] != 0)
					{
						abs_val1 = FSK_ABS(pbuf->buffer[start +j]);
						abs_val2 = FSK_ABS(pbuf->buffer[start +j +1]);
						if (abs_val1 > (val_max *3 /5) && abs_val2 > (val_max *3 /5))
						{
							abs_val1 = FSK_ABS(pbuf->buffer[start +j -1]);
							abs_val2 = FSK_ABS(pbuf->buffer[start +j +2]);
							if (abs_val1 < (val_max *2 /5) && abs_val2 < (val_max *2 /5))
							{
								abs_val1 = FSK_ABS(pbuf->buffer[start +j] - pbuf->buffer[start +j -1]);
								abs_val2 = FSK_ABS(pbuf->buffer[start +j +1] - pbuf->buffer[start +j +2]);
								if (abs_val1 < (val_max *5 /4) && abs_val2 < (val_max *5 /4))
								{
									zero_count--;
									nzero_max++;
								}
							}
						}
					}
				}	//for (j)
			}
		}
	}
		

	rtn = i;
	if (rtn > 7)
	{
		rtn =7;
	}
	if ((zero_count == 4) || ((zero_count ==3) && (nzero_max == 0 || nzero_max == 1)))
	{
		*bit_ch = 0;
		g_fsk_msg_hex.con1bit_count = 0;
#ifdef FSK_DEBUG
		g_fsk_msg_hex.allbitcount++;
#endif
	}
	else if ((zero_count ==2) && (nzero_max == 2 || nzero_max == 3 || connzero_count >= 3))
	{
		*bit_ch = 1;
		g_fsk_msg_hex.con1bit_count++;
#ifdef FSK_DEBUG
		g_fsk_msg_hex.allbitcount++;
#endif
	}
	else
	{
		rtn = -2;
		g_fsk_msg_hex.con1bit_count = 0;
#ifdef FSK_DEBUG
		g_fsk_msg_hex.allbitcount = 0;
#endif
	}

#ifdef FSK_DEBUG
	//FSK_PRINT_BIT("no:%d cournts=%d,nzero_count=%d,connzero_count=%d,zero_count=%d,nzero_max=%d\n", line_count, i, nzero_count, connzero_count,zero_count,nzero_max);
	line_count++;
	
	if ((line_count % 10) == 0)
	{
		//FSK_PRINT_BIT("\n%02d: ", g_fsk_msg_hex.allbitcount/10);
		FSK_PRINT_BIT("\n%02d: ", line_count/10);
	}
	if (rtn != -2)
	{
		FSK_PRINT_BIT("%d", *bit_ch);
	}
	else
	{
		FSK_PRINT_BIT("2");
	}
	
#endif

	return rtn;
}

/*****************************************************************************/
//  Description : sync start bit
//  Global resource dependence : g_fsk_buff
//  input: pbuf--buffer pointer
//         dotcn--dot count
//  output: none
//  return: TRUE--sucess
//          FALSE --failure
//  Author: mengfanyi
//  date:2013-12-16
//  Note: start>=0, dotcn>=0
/*****************************************************************************/
BOOLEAN Fsk_SyncStartBit(Fsk_Buffer_T *pbuf, int start,int dotcn)
{
	return TRUE;
}

/*****************************************************************************/
//  Description : sync fsk signal
//  Global resource dependence : g_fsk_buff
//  input: pbuf--buffer pointer
//         start--start position
//  output: none
//  return: >=0--move dot count
//          -1 --failure
//  Author: mengfanyi
//  date:2013-12-16
//  Note: don't modify g_fsk_buff and pbuf
/*****************************************************************************/
int Fsk_GetSyncSignal(Fsk_Buffer_T *pbuf, int start)
{
	return 0;
}

/*****************************************************************************/
//  Description : locate FSK head,start dot
//  Global resource dependence : g_fsk_buff
//  input: none
//  output: none
//  return: FALSE--failure
//          TRUE--success
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
BOOLEAN Fsk_LocateFSKHead(void)
{
	int i, count;
	int curpos;

	g_fsk_buff.circlestart = g_fsk_buff.circlestart + g_fsk_buff.bitstart;
	g_fsk_buff.bitstart = 0;
	curpos = g_fsk_buff.circlestart;

	while (curpos < g_fsk_buff.count + FSK_BUFFER_START)
	{
		if (((g_fsk_buff.buffer[curpos] < -FSK_MIN_VALID_DECVAL) && ((g_fsk_buff.buffer[curpos] - g_fsk_buff.buffer[curpos -1]) < -FSK_MIN_VALID_MINVAL))
			|| ((g_fsk_buff.buffer[curpos] < 0) && ((g_fsk_buff.buffer[curpos] - g_fsk_buff.buffer[curpos -1]) < -FSK_MIN_VALID_DECVAL)))
		{
			count = 0;
			for (i=0; i < FSK_MAX_CON_DOT && (curpos +i) < (g_fsk_buff.count + FSK_RESERVE_LENGTH) ; i++)
			{
				if (g_fsk_buff.buffer[curpos + i] >= 0)
				{
					break;
				}
				count++;
			}	//for(i)


			if (FSK_MAX_CON_DOT > count)
			{
				if ((curpos +i) < (g_fsk_buff.count + FSK_RESERVE_LENGTH))
				{
					g_fsk_buff.circlestart = curpos;
					g_fsk_buff.bitstart = 0;
					return TRUE;
				}
				else
				{
					break;
				}
			}
			else
			{
				curpos++;
			}
		}	//if (cur_val)
		else
		{
			curpos++;
		}
	}	//while (g_fsk_buff)
	g_fsk_buff.circlestart = curpos;

	//Fsk_FreeBuffer();
	return FALSE;
}

/*****************************************************************************/
//  Description : locate fsk channel signal
//  Global resource dependence : g_fsk_buff
//  input: none
//  output: none
//  return: >=0--process dot count
//          -1--channel signal end
//          -2--buffer overflow
//          -3--bit parse error
//  Author: mengfanyi
//  date:2013-12-16
//  Note:  150 '01'
/*****************************************************************************/
int Fsk_LocateFSKChannelSignal(void)
{
	int i, j;
	//BOOLEAN bitflag = FALSE;
	int fail_flag;
	char bit_val = -1;
	int adjcount = 0;
	int curpos;
	int count, dot_count,alldot_count;
	int proc_count;

	count = 0;
	//bit_count = 0;
	alldot_count = 0;
	fail_flag = 0;
	curpos = g_fsk_buff.circlestart + g_fsk_buff.bitstart;

	while (curpos < g_fsk_buff.count + FSK_BUFFER_START)
	{
		fail_flag = 0;
		while (g_fsk_buff.bitstart <FSK_CYCLE_GETDATA)
		{
			adjcount = 0;
			dot_count = Fsk_ParseOneBit(&g_fsk_buff, g_fsk_buff.circlestart + g_fsk_buff.bitstart, &bit_val, &adjcount);
			if (dot_count < 0)
			{
				if (dot_count == -2)
				{
					fail_flag = -3;
				}
				else
				{
					fail_flag  = -2;
				}
				break;
			}

			if (adjcount != 0)
			{
				//Fsk_SyncStartBit(pbuf, pbuf->bitcurl +i +j,adjcount);
				//pbuf->bitcurl += g_fsk_msg_hex.bit_startno + adjcount;
				break;
			}

			if ((FALSE == g_fsk_msg_hex.bitflag) && (0 ==bit_val))
			{
				g_fsk_msg_hex.bitflag = TRUE;
				g_fsk_msg_hex.bit_count++;
			}
			else if ((TRUE == g_fsk_msg_hex.bitflag) && (1 ==bit_val))
			{
				g_fsk_msg_hex.bitflag = FALSE;
				g_fsk_msg_hex.bit_count++;
			}
			else
			{
				fail_flag = -1;
				/*
				g_fsk_buff.bitstart += FSK_MIN_COUNT + (dot_count>FSK_MIN_COUNT? 1 : 0);
				if (g_fsk_buff.bitstart >= FSK_CYCLE_GETDATA)
				{
					g_fsk_buff.circlestart += FSK_CYCLE_GETDATA;
					g_fsk_buff.bitstart = 0;
				}
				*/
				//g_fsk_msg_hex.bit_count = 0;
				break;
			}
			if (dot_count > 0 && dot_count < 7)
			{
				dot_count++;
			}
	
			g_fsk_buff.bitstart += FSK_MIN_COUNT + (dot_count>FSK_MIN_COUNT? 1 : 0);

		}	//while (g_fsk_buff.bitstart)

		if (0 > fail_flag)
		{
			break;
		}

		if (adjcount != 0)
		{
			//i += j+adjcount;
			proc_count = g_fsk_buff.circlestart + g_fsk_buff.bitstart + adjcount;
			//g_fsk_msg_hex.bit_startno = 0;
			FSK_PRINT("\nadjcount=%d\n", adjcount);
		}
		else
		{
			//i += FSK_CYCLE_GETDATA;
			proc_count = g_fsk_buff.circlestart + FSK_CYCLE_GETDATA;
			//g_fsk_msg_hex.bit_startno = 0;
		}

		if (proc_count < g_fsk_buff.count + FSK_BUFFER_START)
		{
			g_fsk_buff.circlestart = proc_count;
			g_fsk_buff.bitstart = 0;
		}
		else
		{
			fail_flag = -2;
			break;
		}

		curpos = g_fsk_buff.circlestart + g_fsk_buff.bitstart;

	}	//while (curpos)

	/*
	if (bit_count < FSK_CHANNAL_SIGNL_COUNT)
	{
		g_fsk_buff.prevalue = pbuf->buffer[pbuf->curl];
		pbuf->curl++;
		pbuf->bitcurl = pbuf->curl;
		return -1;
	}
	else
	{
		//skip signal
		pbuf = g_fsk_buff.pcurl;
		while (NULL != pbuf && alldot_count > 0)
		{
			if ((alldot_count + pbuf->bitcurl) < pbuf->size)
			{
				pbuf->bitcurl += alldot_count;
				pbuf->curl = pbuf->bitcurl;
				alldot_count = 0;
			}
			else
			{
				pnext = pbuf->next;
				alldot_count -= pbuf->size - pbuf->bitcurl;
				pbuf = pnext;
				g_fsk_buff.pcurl = pnext;
			}
		}
		return bit_count;
	}
	*/
	if (fail_flag < 0)
	{
		return fail_flag;
	}
	else
	{
		return g_fsk_msg_hex.bit_count;
	}
}

/*****************************************************************************/
//  Description : process buffer
//  Global resource dependence : g_fsk_buff
//  input: none
//  output: none
//  return: >=0--process dot count
//          -1--channel signal end
//          -2--buffer overflow
//          -3--bit parse error
//  Author: mengfanyi
//  date:2013-12-16
//  Note: 180 '1' or 80 '1'
/*****************************************************************************/
int Fsk_LocateFSKMarkSignal(void)
{
	int i, j;
	//BOOLEAN bitflag = FALSE;
	int fail_flag;
	char bit_val = -1;
	int adjcount = 0;
	int curpos;
	int count, dot_count,alldot_count;
	int proc_count;

	count = 0;
	//bit_count = 0;
	alldot_count = 0;
	fail_flag = 0;
	curpos = g_fsk_buff.circlestart + g_fsk_buff.bitstart;


	while (curpos < g_fsk_buff.count + FSK_BUFFER_START)
	{
		fail_flag = 0;
		while (g_fsk_buff.bitstart < FSK_CYCLE_GETDATA)	//don't add pbuf->bitcurl
		{
			adjcount = 0;
			dot_count = Fsk_ParseOneBit(&g_fsk_buff, g_fsk_buff.circlestart + g_fsk_buff.bitstart, &bit_val, &adjcount);
			if (dot_count < 0)
			{
				if (dot_count == -2)
				{
					fail_flag  = -3;
				}
				else
				{
					fail_flag  = -2;
				}
				break;
			}

			if (adjcount != 0)
			{
				//Fsk_SyncStartBit(pbuf, pbuf->bitcurl +i +j,adjcount);
				//pbuf->bitcurl += g_fsk_msg_hex.bit_startno + adjcount;
				break;
			}

			if (1 ==bit_val)
			{
				//bitflag = FALSE;
				g_fsk_msg_hex.bit_count++;
			}
			else
			{
				FSK_PRINT_BIT("\nmark signal end! bit count=%d\n", g_fsk_msg_hex.bit_count);

				fail_flag = -1;
				/*
				g_fsk_buff.bitstart += FSK_MIN_COUNT + (dot_count>FSK_MIN_COUNT? 1 : 0);
				if (g_fsk_buff.bitstart >= FSK_CYCLE_GETDATA)
				{
					g_fsk_buff.circlestart += FSK_CYCLE_GETDATA;
					g_fsk_buff.bitstart = 0;
				}
				*/
				//g_fsk_msg_hex.bit_count = 0;
				break;
			}
			if (dot_count > 0 && dot_count < 7)
			{
				dot_count++;
			}
	
			g_fsk_buff.bitstart += FSK_MIN_COUNT + (dot_count>FSK_MIN_COUNT? 1 : 0);

		}	//for (j)

		if (0 > fail_flag)
		{
			break;
		}

		if (adjcount != 0)
		{
			//i += j+adjcount;
			proc_count = g_fsk_buff.circlestart + g_fsk_buff.bitstart + adjcount;
			//g_fsk_msg_hex.bit_startno = 0;
			FSK_PRINT("\nadjcount=%d\n", adjcount);
		}
		else
		{
			//i += FSK_CYCLE_GETDATA;
			proc_count = g_fsk_buff.circlestart + FSK_CYCLE_GETDATA;
			//g_fsk_msg_hex.bit_startno = 0;
		}

		if (proc_count < g_fsk_buff.count + FSK_BUFFER_START)
		{
			g_fsk_buff.circlestart = proc_count;
			g_fsk_buff.bitstart = 0;
		}
		else
		{
			fail_flag = -2;
			break;
		}
	}	//while (i)

	/*
	if (bit_count < FSK_CHANNAL_SIGNL_COUNT)
	{
		g_fsk_buff.prevalue = pbuf->buffer[pbuf->curl];
		pbuf->curl++;
		pbuf->bitcurl = pbuf->curl;
		return -1;
	}
	else
	{
		//skip signal
		pbuf = g_fsk_buff.pcurl;
		while (NULL != pbuf && alldot_count > 0)
		{
			if ((alldot_count + pbuf->bitcurl) < pbuf->size)
			{
				pbuf->bitcurl += alldot_count;
				pbuf->curl = pbuf->bitcurl;
				alldot_count = 0;
			}
			else
			{
				pnext = pbuf->next;
				alldot_count -= pbuf->size - pbuf->bitcurl;
				pbuf = pnext;
				g_fsk_buff.pcurl = pnext;
			}
		}
		return bit_count;
	}
	*/
	if (fail_flag < 0)
	{
		return fail_flag;
	}
	else
	{
		return g_fsk_msg_hex.bit_count;
	}
}

/*****************************************************************************/
//  Description : parse fsk message data word
//  Global resource dependence : g_fsk_buff,g_fsk_msg_hex
//  input: word--fsk message word
//  output: none
//  return: TRUE--sucess
//          FALSE--failure
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
BOOLEAN Fsk_ParseFSKMsgWord(unsigned char *word)
{
	unsigned char ch;
	unsigned char fsk_checkbit;
	int i;

	i = 0;
	ch = 0;
	fsk_checkbit = 0;
	if (0 != word[0])
	{
		FSK_PRINT_BIT("\nfirst bit isn't 0 in data word\n");
		return FALSE;
	}
	i++;
	while (i < g_fsk_msg_hex.msgmaxlen -2)
	{
		fsk_checkbit +=	word[i];
		ch >>=1;
		if (word[i] == 1)
		{
			ch |= 0x80;
		}
		i++;
	}
	if (g_fsk_msg_hex.msgmaxlen < FSK_MSG_WORD_LENTH)
	{
		ch >>=1;
		if (word[i] == 1 &&g_fsk_msg_hex.step == FSK_MSG_STEP_CHECK)
		{
			ch |= 0x80;
		}
	}
			
	/*
	fsk_checkbit %=2;
	if (fsk_checkbit == word[i])
	{
		FSK_PRINT_MSG_WORD("\nbit check flag is ok, byte num = %d!!!!", i/11);
	}
	else
	{
		FSK_PRINT_MSG_WORD("\nbit check flag is error, byte num = %d!!!!", i/11);
	}
	*/

	if (1 != word[g_fsk_msg_hex.msgmaxlen -1] && g_fsk_msg_hex.step != FSK_MSG_STEP_CHECK)
	{
		FSK_PRINT_BIT("\nend bit isn't 1 in data word\n");
		return FALSE;
	}

	//g_fsk_msg_hex.checksum += ch;
	if (0 == g_fsk_msg_hex.msglen || g_fsk_msg_hex.msgcount < (g_fsk_msg_hex.msglen +1))
	{
		switch (g_fsk_msg_hex.step)
		{
		case FSK_MSG_STEP_START:
			//g_fsk_msg_hex.step = FSK_MSG_STEP_MSG_TYPE;
			break;
		case FSK_MSG_STEP_MSG_TYPE:
			g_fsk_msg_hex.checksum += ch;
			if (FSK_MSG_TYPE_MAKECALL == ch || FSK_MSG_TYPE_CID == ch)
			{
				g_fsk_msg_hex.msgtype = ch;
				g_fsk_msg_hex.step = FSK_MSG_STEP_MSG_LENTH;
			}
			break;
		case FSK_MSG_STEP_MSG_LENTH:
			g_fsk_msg_hex.checksum += ch;
			if (ch > 0)
			{
				g_fsk_msg_hex.msglen = ch;
			}
			else
			{
				g_fsk_msg_hex.msglen = 0;
			}
			g_fsk_msg_hex.msgcount = 0;

#ifdef FSK_DEBUG
			if (NULL != g_fsk_msg_hex.buffer)
			{
				free(g_fsk_msg_hex.buffer);
				g_fsk_msg_hex.buffer = NULL;
			}
			g_fsk_msg_hex.buffer = (char *)malloc(g_fsk_msg_hex.msglen * sizeof(char));
			if (NULL == g_fsk_msg_hex.buffer)
			{
				return FALSE;
			}
#endif
			if (FSK_MSG_TYPE_MAKECALL == g_fsk_msg_hex.msgtype)
			{
				g_fsk_msg_hex.paramtype = 0;
				g_fsk_msg_hex.paramlen = 0;
				g_fsk_msg_hex.step = FSK_MSG_STEP_PARAM_TYPE;
			}
			else
			{
				g_fsk_msg_hex.step = FSK_MSG_STEP_MONTH;
			}		
			g_fsk_msg_hex.paramno = 0;
			break;
		case FSK_MSG_STEP_PARAM_TYPE:
			g_fsk_msg_hex.msgcount++;
			g_fsk_msg_hex.paramtype = ch;
			g_fsk_msg_hex.checksum += ch;
			g_fsk_msg_hex.step = FSK_MSG_STEP_PARAM_LENTH;
			break;
		case FSK_MSG_STEP_PARAM_LENTH:
			g_fsk_msg_hex.msgcount++;
			g_fsk_msg_hex.paramlen = ch;
			g_fsk_msg_hex.checksum += ch;
			g_fsk_msg_hex.paramno = 0;
			if (FSK_MSG_TYPE_MAKECALL == g_fsk_msg_hex.msgtype)
			{
				switch(g_fsk_msg_hex.paramtype)
				{
				case FSK_MSG_PARAM_TYPE_CALLTIME:
					g_fsk_msg_hex.step = FSK_MSG_STEP_MONTH;
					break;
				case FSK_MSG_PARAM_TYPE_CALLNUM:
					g_fsk_msg_hex.step = FSK_MSG_STEP_NUMBER;
					break;
				case FSK_MSG_PARAM_TYPE_NONUM:
					g_fsk_msg_hex.step = FSK_MSG_STEP_NUMBER;
					break;
				case FSK_MSG_PARAM_TYPE_CALLNAME:
					g_fsk_msg_hex.step = FSK_MSG_STEP_NAME;
					break;
				case FSK_MSG_PARAM_TYPE_NONAME:
					g_fsk_msg_hex.step = FSK_MSG_STEP_NAME;
					break;
				}
			}
			else if (FSK_MSG_TYPE_CID == g_fsk_msg_hex.msgtype)
			{
				g_fsk_msg_hex.step = FSK_MSG_STEP_MONTH;
			}
			else
			{
				g_fsk_msg_hex.step = FSK_MSG_STEP_END;
			}
			break;
		case FSK_MSG_STEP_MONTH:
			g_fsk_msg_hex.msgcount++;
			g_fsk_msg_hex.checksum += ch;
			if (g_fsk_msg_hex.paramno < 2)
			{
				g_fsk_msg.month[g_fsk_msg_hex.paramno] = ch;
			}
			g_fsk_msg_hex.paramno++;
			if (g_fsk_msg_hex.paramno == 2)
			{
				g_fsk_msg_hex.step = FSK_MSG_STEP_DAY;
				g_fsk_msg_hex.paramno = 0;
			}
			break;
		case FSK_MSG_STEP_DAY:
			g_fsk_msg_hex.msgcount++;
			g_fsk_msg_hex.checksum += ch;
			if (g_fsk_msg_hex.paramno < 2)
			{
				g_fsk_msg.day[g_fsk_msg_hex.paramno] = ch;
			}
			g_fsk_msg_hex.paramno++;
			if (g_fsk_msg_hex.paramno == 2)
			{
				g_fsk_msg_hex.step = FSK_MSG_STEP_HOUR;
				g_fsk_msg_hex.paramno = 0;
			}
			break;
		case FSK_MSG_STEP_HOUR:
			g_fsk_msg_hex.msgcount++;
			g_fsk_msg_hex.checksum += ch;
			if (g_fsk_msg_hex.paramno < 2)
			{
				g_fsk_msg.hour[g_fsk_msg_hex.paramno] = ch;
			}
			g_fsk_msg_hex.paramno++;
			if (g_fsk_msg_hex.paramno == 2)
			{
				g_fsk_msg_hex.step = FSK_MSG_STEP_MINUTE;
				g_fsk_msg_hex.paramno = 0;
			}
			break;
		case FSK_MSG_STEP_MINUTE:
			g_fsk_msg_hex.msgcount++;
			g_fsk_msg_hex.checksum += ch;
			if (g_fsk_msg_hex.paramno < 2)
			{
				g_fsk_msg.minute[g_fsk_msg_hex.paramno] = ch;
			}
			g_fsk_msg_hex.paramno++;
			if (g_fsk_msg_hex.paramno == 2)
			{
				if (FSK_MSG_TYPE_MAKECALL == g_fsk_msg_hex.msgtype)
				{
					g_fsk_msg_hex.step = FSK_MSG_STEP_PARAM_TYPE;
				}
				else
				{
					g_fsk_msg_hex.step = FSK_MSG_STEP_NUMBER;
				}
				g_fsk_msg_hex.paramno = 0;
			}
			break;
		case FSK_MSG_STEP_NUMBER:
			g_fsk_msg_hex.msgcount++;
			g_fsk_msg_hex.checksum += ch;
			if (FALSE == g_fsk_msg.callnumvalid)
			{
				g_fsk_msg.callnumvalid = TRUE;
			}

			if (g_fsk_msg_hex.paramno < 40)
			{
				g_fsk_msg.num[g_fsk_msg_hex.paramno] = ch;
			}
			g_fsk_msg_hex.paramno++;

			if (FSK_MSG_TYPE_MAKECALL == g_fsk_msg_hex.msgtype)
			{
				if (g_fsk_msg_hex.paramlen == g_fsk_msg_hex.paramno)
				{
					if (g_fsk_msg_hex.msgcount == g_fsk_msg_hex.msglen)
					{
						g_fsk_msg_hex.step = FSK_MSG_STEP_CHECK;
					}
					else
					{
						g_fsk_msg_hex.step = FSK_MSG_STEP_PARAM_TYPE;
					}
				}
			}
			else
			{
				if (g_fsk_msg_hex.msgcount == g_fsk_msg_hex.msglen)
				{
					g_fsk_msg_hex.step = FSK_MSG_STEP_CHECK;
				}
			}
			break;
		case FSK_MSG_STEP_NAME:
			g_fsk_msg_hex.msgcount++;
			g_fsk_msg_hex.checksum += ch;
			if (FALSE == g_fsk_msg.cidvalid)
			{
				g_fsk_msg.cidvalid = TRUE;
			}

			if (g_fsk_msg_hex.paramno < 40)
			{
				g_fsk_msg.num[g_fsk_msg_hex.paramno] = ch;
			}
			if (g_fsk_msg_hex.paramlen == g_fsk_msg_hex.paramno)
			{
				if (g_fsk_msg_hex.msgcount == g_fsk_msg_hex.msglen)
				{
					g_fsk_msg_hex.step = FSK_MSG_STEP_CHECK;
				}
				else
				{
					g_fsk_msg_hex.step = FSK_MSG_STEP_PARAM_TYPE;
				}
			}
			break;
		case FSK_MSG_STEP_CHECK:
			g_fsk_msg_hex.checksum = ~(g_fsk_msg_hex.checksum) + 1;

			if (g_fsk_msg_hex.checksum == ch)
			{
				g_fsk_msg.isgood = TRUE;
				FSK_PRINT_BIT("\ncheck sum is ok\n");
				//g_fsk_msg_hex.step = FSK_MSG_STEP_END;
			}
			else
			{
				g_fsk_msg.isgood = FALSE;
				g_fsk_msg.callnumvalid = FALSE;
				g_fsk_msg.cidvalid = FALSE;
				FSK_PRINT_BIT("\ncheck sum is error\n");
			}
			memcpy(&g_fsk_bak_msg, &g_fsk_msg, sizeof(Fsk_Message_T));

			if (FALSE == g_finish_flag)
			{
				g_finish_flag = TRUE;
			}
			g_fsk_msg_hex.step = FSK_MSG_STEP_END;
			break;
		case FSK_MSG_STEP_END:
			break;
		}
	}
	/*
	else
	{
		g_fsk_msg_hex.checksum = ~(g_fsk_msg_hex.checksum) + 1;

		if (g_fsk_msg_hex.checksum == ch)
		{
			if (FALSE == g_finish_flag)
			{
				g_finish_flag = TRUE;
			}
			memcpy(&g_fsk_bak_msg, &g_fsk_msg, sizeof(Fsk_Message_T));
			FSK_PRINT_MSG_WORD("check sum is ok\n");
			g_fsk_msg_hex.step = FSK_MSG_STEP_END;
		}
		else
		{
			FSK_PRINT_MSG_WORD("check sum is error\n");
		}

	}
	*/
	return TRUE;
}

/*****************************************************************************/
//  Description : parse fsk message
//  Global resource dependence : g_fsk_buff,g_fsk_msg_hex
//  input: none
//  output: none
//  return: >=0--process dot count
//          -1--channel signal end
//          -2--buffer overflow
//          -3--bit parse error
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
int Fsk_ParseFSKMsg(void)
{
	int i, j;
	//BOOLEAN bitflag = FALSE;
	int fail_flag;
	char bit_val = -1;
	int adjcount = 0;
	int curpos;
	int count, dot_count,alldot_count;
	int proc_count;

	count = 0;
	//bit_count = 0;
	alldot_count = 0;
	fail_flag = 0;
	curpos = g_fsk_buff.circlestart + g_fsk_buff.bitstart;

	if (g_fsk_msg_hex.step == FSK_MSG_STEP_START || g_fsk_msg_hex.step == FSK_MSG_STEP_END)
	{
		memset(&g_fsk_msg, 0, sizeof(Fsk_Message_T));
		g_fsk_msg_hex.bit_in_word = 0;
		//memset(g_fsk_msg_hex.data_word, 0, g_fsk_msg_hex.msgmaxlen * sizeof(unsigned char));
		//memset(&g_fsk_msg_hex, 0, sizeof(Fsk_Msg_Buffer_T));
		g_fsk_msg_hex.step = FSK_MSG_STEP_MSG_TYPE;
		g_fsk_msg_hex.msgtype = 0;
		g_fsk_msg_hex.msglen = 0;
		g_fsk_msg_hex.checksum = 0;
		g_fsk_msg_hex.msgmaxlen = FSK_MSG_WORD_LENTH;
		g_fsk_msg_hex.paramtype = 0;
		g_fsk_msg_hex.paramlen = 0;
		g_fsk_msg_hex.paramno = 0;
		g_fsk_msg_hex.bit_in_word = 0;
		memset(g_fsk_msg_hex.data_word, 0, (FSK_MSG_WORD_LENTH+1)*sizeof(unsigned char));
#ifdef FSK_DEBUG
		if (NULL !=g_fsk_msg_hex.buffer)
		{
			free(g_fsk_msg_hex.buffer);
			g_fsk_msg_hex.buffer = NULL;
		}
#endif
	}

	while ((curpos < g_fsk_buff.count + FSK_BUFFER_START) && g_fsk_msg_hex.step != FSK_MSG_STEP_END)
	{
		fail_flag = 0;
		while (g_fsk_buff.bitstart <FSK_CYCLE_GETDATA && g_fsk_msg_hex.step != FSK_MSG_STEP_END)	//don't add pbuf->bitcurl
		{
			adjcount = 0;
			dot_count = Fsk_ParseOneBit(&g_fsk_buff, g_fsk_buff.circlestart + g_fsk_buff.bitstart, &bit_val, &adjcount);
			if (dot_count < 0)
			{
				if (dot_count == -2)
				{
					if (g_fsk_msg_hex.step != FSK_MSG_STEP_CHECK && g_fsk_msg_hex.bit_in_word == g_fsk_msg_hex.msgmaxlen -1)
					{
						fail_flag  = -3;
						break;
					}
				}
				else
				{
					fail_flag  = -2;
					break;
				}
			}

			if (adjcount != 0)
			{
				//Fsk_SyncStartBit(pbuf, pbuf->bitcurl +i +j,adjcount);
				//pbuf->bitcurl += g_fsk_msg_hex.bit_startno + adjcount;
				break;
			}

			if (0 == bit_val)
			{
				if (g_fsk_msg_hex.bit_in_word < g_fsk_msg_hex.msgmaxlen)
				{
					g_fsk_msg_hex.data_word[g_fsk_msg_hex.bit_in_word] = 0;
					g_fsk_msg_hex.bit_in_word++;
				}
				if (g_fsk_msg_hex.bit_in_word >= g_fsk_msg_hex.msgmaxlen)
				{
					int k;

					for (k=0;k<g_fsk_msg_hex.msgmaxlen;k++)
					{
						FSK_PRINT_MSG_WORD("%d", g_fsk_msg_hex.data_word[k]);
					}
					FSK_PRINT_MSG_WORD("\n");

					Fsk_ParseFSKMsgWord(g_fsk_msg_hex.data_word);
					g_fsk_msg_hex.bit_in_word = 0;
					memset(g_fsk_msg_hex.data_word, 0, FSK_MSG_WORD_LENTH * sizeof(unsigned char));
				}
			}
			else if (0 < g_fsk_msg_hex.bit_in_word)
			{
				if (3 == g_fsk_msg_hex.bit_in_word && g_fsk_msg_hex.step == FSK_MSG_STEP_MSG_TYPE)
				{
					g_fsk_msg_hex.msgmaxlen = FSK_MSG_WORD_LENTH -1;
				}
				if (g_fsk_msg_hex.bit_in_word < g_fsk_msg_hex.msgmaxlen)
				{
					g_fsk_msg_hex.data_word[g_fsk_msg_hex.bit_in_word] = 1;
					g_fsk_msg_hex.bit_in_word++;
				}
				if (g_fsk_msg_hex.bit_in_word >= g_fsk_msg_hex.msgmaxlen)
				{
					int k;

					for (k=0;k<g_fsk_msg_hex.msgmaxlen;k++)
					{
						FSK_PRINT_MSG_WORD("%d", g_fsk_msg_hex.data_word[k]);
					}
					FSK_PRINT_MSG_WORD("\n");

					Fsk_ParseFSKMsgWord(g_fsk_msg_hex.data_word);
					g_fsk_msg_hex.bit_in_word = 0;
					memset(g_fsk_msg_hex.data_word, 0, FSK_MSG_WORD_LENTH * sizeof(unsigned char));
				}
			}
			if (g_fsk_msg_hex.step == FSK_MSG_STEP_END)
			{
				fail_flag  = -1;
				/*
				g_fsk_buff.bitstart += FSK_MIN_COUNT + (dot_count>FSK_MIN_COUNT? 1 : 0);
				if (g_fsk_buff.bitstart >= FSK_CYCLE_GETDATA)
				{
					g_fsk_buff.circlestart += FSK_CYCLE_GETDATA;
					g_fsk_buff.bitstart = 0;
				}
				*/
				break;
	
			}
			if (dot_count > 0 && dot_count < 7)
			{
				dot_count++;
			}
	
			g_fsk_buff.bitstart += FSK_MIN_COUNT + (dot_count>FSK_MIN_COUNT? 1 : 0);
			if (0 > fail_flag)
			{
				break;
			}

		}	//for (j)

		if (0 > fail_flag)
		{
			break;
		}

		if (adjcount != 0)
		{
			//i += j+adjcount;
			proc_count = g_fsk_buff.circlestart + g_fsk_buff.bitstart + adjcount;
			//g_fsk_msg_hex.bit_startno = 0;
			FSK_PRINT_MSG_WORD("\nadjcount=%d\n", adjcount);
		}
		else
		{
			//i += FSK_CYCLE_GETDATA;
			proc_count = g_fsk_buff.circlestart + FSK_CYCLE_GETDATA;
			//g_fsk_msg_hex.bit_startno = 0;
		}

		if (proc_count < g_fsk_buff.count + FSK_BUFFER_START)
		{
			g_fsk_buff.circlestart = proc_count;
			g_fsk_buff.bitstart = 0;
		}
		else
		{
			fail_flag = -2;
			break;
		}

		curpos = g_fsk_buff.circlestart + g_fsk_buff.bitstart;

	}	//while (pbuf)

	/*
	if (bit_count < FSK_CHANNAL_SIGNL_COUNT)
	{
		g_fsk_buff.prevalue = pbuf->buffer[pbuf->curl];
		pbuf->curl++;
		pbuf->bitcurl = pbuf->curl;
		return -1;
	}
	else
	{
		//skip signal
		pbuf = g_fsk_buff.pcurl;
		while (NULL != pbuf && alldot_count > 0)
		{
			if ((alldot_count + pbuf->bitcurl) < pbuf->size)
			{
				pbuf->bitcurl += alldot_count;
				pbuf->curl = pbuf->bitcurl;
				alldot_count = 0;
			}
			else
			{
				pnext = pbuf->next;
				alldot_count -= pbuf->size - pbuf->bitcurl;
				pbuf = pnext;
				g_fsk_buff.pcurl = pnext;
			}
		}
		return bit_count;
	}
	*/
	if (fail_flag < 0)
	{
		return fail_flag;
	}
	else
	{
		return g_fsk_msg_hex.bit_count;
	}
}

/*****************************************************************************/
//  Description : process buffer
//  Global resource dependence : g_fsk_buff
//  input: none
//  output: none
//  return: none
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
void Fsk_Process(void)
{
	int i, count;
	int curpos;
	BOOLEAN rtn = TRUE;

	curpos = g_fsk_buff.circlestart + g_fsk_buff.bitstart;

	while (curpos < g_fsk_buff.count + FSK_BUFFER_START)
	{
		count = -1;
		switch(g_fsk_msg_hex.parse_step)
		{
		case FSK_PARSE_STEP_START:
			g_fsk_msg_hex.parse_step = FSK_PARSE_STEP_HEAD;
			g_fsk_msg_hex.bit_count = 0;
			g_fsk_msg_hex.bitflag = FALSE;
			//g_fsk_msg_hex.bit_startno = 0;
			break;
		case FSK_PARSE_STEP_HEAD:
			rtn = Fsk_LocateFSKHead();
			if (TRUE == rtn)
			{
				g_fsk_msg_hex.parse_step = FSK_PARSE_STEP_CHANNEL;
				g_fsk_msg_hex.bit_count = 0;
				//g_fsk_msg_hex.bit_startno = 0;
				g_fsk_msg_hex.bitflag = FALSE;
			}
			break;
		case FSK_PARSE_STEP_CHANNEL:
			count = Fsk_LocateFSKChannelSignal();
			if (count == -1)
			{
				if (g_fsk_msg_hex.bit_count < FSK_CHANNAL_SIGNL_COUNT)
				{
					//g_fsk_buff.circlestart++;
					//g_fsk_msg_hex.parse_step = FSK_PARSE_STEP_HEAD;
					g_fsk_msg_hex.parse_step = FSK_PARSE_STEP_MARK;
					g_fsk_msg_hex.bit_count = 0;
					//pbuf->curl = pbuf->bitcurl;
					g_fsk_msg_hex.bitflag = FALSE;
				}
				else
				{
					g_fsk_msg_hex.parse_step = FSK_PARSE_STEP_MARK;
					g_fsk_msg_hex.bit_count = 0;
					g_fsk_msg_hex.bitflag = FALSE;
				}
			}
			else if (count == -3)
			{
				g_fsk_buff.circlestart++;
				g_fsk_msg_hex.parse_step = FSK_PARSE_STEP_HEAD;
				g_fsk_msg_hex.bit_count = 0;
				g_fsk_msg_hex.bitflag = FALSE;
			}
			break;
		case FSK_PARSE_STEP_MARK:
			count = Fsk_LocateFSKMarkSignal();
			if (count == -1)
			{
				if (g_fsk_msg_hex.bit_count < FSK_MARK_SIGNL_COUNT)
				{
					g_fsk_msg_hex.parse_step = FSK_PARSE_STEP_HEAD;
					g_fsk_msg_hex.bit_count = 0;
					g_fsk_buff.circlestart++;
					g_fsk_msg_hex.bitflag = FALSE;
				}
				else
				{
					g_fsk_msg_hex.parse_step = FSK_PARSE_STEP_MSG;
					g_fsk_msg_hex.bit_count = 0;
					memset(&g_fsk_msg, 0, sizeof(Fsk_Message_T));
					g_fsk_msg_hex.step = FSK_MSG_STEP_START;
					g_fsk_msg_hex.msgmaxlen = FSK_MSG_WORD_LENTH;
					g_fsk_msg_hex.bitflag = FALSE;
				}
			}
			else if (count == -3)
			{
				g_fsk_buff.circlestart++;
				g_fsk_msg_hex.parse_step = FSK_PARSE_STEP_HEAD;
				g_fsk_msg_hex.bit_count = 0;
				g_fsk_msg_hex.bitflag = FALSE;
			}
			break;
		case FSK_PARSE_STEP_MSG:
			count = Fsk_ParseFSKMsg();
			if (count == -1)
			{
				g_fsk_msg_hex.parse_step = FSK_PARSE_STEP_HEAD;
				g_fsk_msg_hex.bit_count = 0;
				g_fsk_buff.circlestart++;
				g_fsk_msg_hex.step = FSK_MSG_STEP_START;
				g_fsk_msg_hex.bit_in_word = 0;
				g_fsk_msg_hex.bitflag = FALSE;
			}
			else if (count == -3)
			{
				g_fsk_msg_hex.parse_step = FSK_PARSE_STEP_HEAD;
				g_fsk_msg_hex.bit_count = 0;
				g_fsk_buff.circlestart++;
				g_fsk_msg_hex.step = FSK_MSG_STEP_START;
				g_fsk_msg_hex.bit_in_word = 0;
				g_fsk_msg_hex.bitflag = FALSE;
			}
			break;
		case FSK_PARSE_STEP_END:
			g_fsk_msg_hex.parse_step = FSK_PARSE_STEP_START;
			break;
		}
		//if (count == -2 || count >= 0)
		if (count == -2 || rtn == FALSE)
		{
			break;
		}
		curpos = g_fsk_buff.circlestart + g_fsk_buff.bitstart;
	}
	Fsk_FreeBuffer();
}

/*****************************************************************************/
//  Description : get fsk data
//  Global resource dependence : g_fsk_buff
//  input: fskmsg--fsk message pointer
//  output: fskmsg--fsk messeage
//  return: TRUE--fsk data is valid
//          FALSE--fsk data is invalid
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
BOOLEAN Fsk_GetFskMsg(Fsk_Message_T *fskmsg)
{
	if (g_finish_flag == FALSE && zzl_g_finish_flag == FALSE)
	{
		return FALSE;
	}
	
	if (g_finish_flag == TRUE)
	{
		//PRINT("mengfanyi\n");
		memcpy(fskmsg, &g_fsk_bak_msg, sizeof(Fsk_Message_T));
		memset(&g_fsk_bak_msg, 0, sizeof(Fsk_Message_T));
		g_finish_flag = FALSE;
		return 1;
	}
	if (zzl_g_finish_flag == TRUE)
	{
		//PRINT("zhangzhiliang\n");
		zzl_g_finish_flag = FALSE;
		return 2;
	}
	//return TRUE;
}

/*****************************************************************************/
//  Description : free all fsk buffer
//  Global resource dependence : g_fsk_buff
//  input: none
//  output: none
//  return: none
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
void Fsk_FinishFskData(void)
{
	//Fsk_FreeAllBuffer();
	Fsk_FreeBuffer();
}

