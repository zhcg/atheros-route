/*
 * Copyright (c) 2009, Atheros Communications Inc.
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

#ifndef _SPECTRAL_DEMO_H_
#define _SPECTRAL_DEMO_H_

#include <spec_msg_proto.h>
#include <spectral_data.h>

#define MAX_SAMP_SAVED 5
#define MAX_NUM_FREQ 2

typedef struct saved_samp_msg
{
  int freq;
  int count_saved;
  int cur_save_index;
  int cur_send_index;
  SPECTRAL_SAMP_MSG samp_msg[MAX_SAMP_SAVED];
}SAVED_SAMP_MSG;
//} __ATTRIB_PACK SAVED_SAMP_MSG;

SAVED_SAMP_MSG all_saved_samp[MAX_NUM_FREQ];

u_int8_t *build_single_samp_rsp (u_int16_t freq, int *nbytes,
				 u_int8_t * bin_pwr, int pwr_count,
				 u_int8_t rssi, int8_t lower_rssi,
				 int8_t upper_rssi, u_int8_t max_scale,
				 int16_t max_mag,
				 struct INTERF_SRC_RSP *interf);

u_int8_t *build_fake_samp_rsp (u_int16_t freq, int pwr_count,
			       u_int8_t fake_pwr_val, int *nbytes,
			       struct INTERF_SRC_RSP *interf);

#endif
