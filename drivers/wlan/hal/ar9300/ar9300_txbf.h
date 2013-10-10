/*
 * Copyright (c) 2008-2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _ATH_AR9000_TxBF_CAL_H_
#define _ATH_AR9300_TxBF_CAL_H_
#ifdef  ATH_SUPPORT_TxBF
#include "ah.h"

#define MAXMCSRATE  0x17    /*MCS 23 is max supported MCS rate*/
#define HT_RATE     0x80    /* HT rate indicator*/

#define debugKa     0
#define debugRC     0
#define debugWinRC  0

#define Tone_40M	114
#define Tone_20M	56
#define NUM_ITER	8

#define Nb_phin		(10-1)
#define Nb_coridc	(12-1)
#define Nb_sin		(8-1)
#define	Nb_ph		5
#define	Nb_psi		4
#define	NUM_ITER_V	6

#define Nb_MHINV	13
#define EVM_TH		10
#define rc_max		6000
#define rc_min		100

#define BW_40M      1
#define BW_20M_low  2
#define BW_20M_up   3


/* MIMO control field related bit definiton */

#define AR_Nc_idx       0x0003  /* Nc Index*/
#define AR_Nc_idx_S     0
#define AR_Nr_idx       0x000c  /* Nr Index*/
#define AR_Nr_idx_S     2
#define AR_bandwith     0x0010  /* MIMO Control Channel Width*/
#define AR_bandwith_S   4
#define AR_Ng           0x0060  /* Grouping*/
#define AR_Ng_S         5
#define AR_Nb           0x0180  /* Coeffiecient Size*/
#define AR_Nb_S         7
#define AR_CI           0x0600  /* Codebook Information*/
#define AR_CI_S         9
#define CAL_GAIN        0
#define Smooth          1

/* constant for TXBF IE */
#define ALL_GROUP       3
#define No_CALIBRATION  0
#define INIT_RESP_CAL   3

/* constant for rate code */
#define MIN_THREE_STREAM_RATE   0x90
#define MIN_TWO_STREAM_RATE     0x88
#define MIN_HT_RATE             0x80

typedef struct 
{
	int32_t real;
	int32_t imag;
}COMPLEX;

HAL_BOOL Ka_Caculation(struct ath_hal *ah,int8_t Ntx_A,int8_t Nrx_A,int8_t Ntx_B,int8_t Nrx_B,
                    COMPLEX (*Hba_eff)[3][Tone_40M],COMPLEX (*H_eff_quan)[3][Tone_40M],u_int8_t *M_H
                    ,COMPLEX (*Ka)[Tone_40M],u_int8_t NESSA,u_int8_t NESSB,int8_t BW);

void
compress_v(COMPLEX (*V)[3],u_int8_t Nr,u_int8_t Nc,u_int8_t *phi,u_int8_t *psi);

void
H_to_CSI(struct ath_hal *ah,u_int8_t Ntx,u_int8_t Nrx,COMPLEX (*H)[3][Tone_40M],u_int8_t *M_H,u_int8_t Ntone);
void
ar9300Get_local_h(struct ath_hal *ah, u_int8_t *local_h,int local_h_length,int Ntone,
                COMPLEX (*output_h) [3][Tone_40M]);
#endif
#endif
