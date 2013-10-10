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
 *
 *  Implementation of receive path in atheros OS-independent layer.
 */

/*
 * Definitions for the ATH TxBF layer. private header file
 */
#ifndef ATH_TXBF_H
#define ATH_TXBF_H


#ifdef ATH_SUPPORT_TxBF

#define LOWEST_RATE_RESET_PERIOD (10*60*1000) /* ten minutes in ms*/
#define ONE_MS    1

int  ath_get_txbfcaps(ath_dev_t dev, ieee80211_txbf_caps_t **txbf_caps);
#ifdef TXBF_TODO
void ath_rx_get_pos2_data(ath_dev_t dev,u_int8_t **p_data, u_int16_t* p_len,void **rx_status);
bool ath_rx_txbfrcupdate(ath_dev_t dev,void *rx_status,u_int8_t *local_h,u_int8_t *CSIFrame,u_int8_t NESSA,u_int8_t NESSB, int BW); 
void ath_ap_save_join_mac(ath_dev_t dev, u_int8_t *join_macaddr);
void ath_start_imbf_cal(ath_dev_t dev);
#endif
HAL_BOOL ath_txbf_alloc_key(ath_dev_t dev, u_int8_t *mac, u_int16_t *keyix);
void ath_txbf_set_key (ath_dev_t dev,u_int16_t keyidx,u_int8_t rx_staggered_sounding,u_int8_t channel_estimation_cap
                ,u_int8_t MMSS);
void ath_txbf_set_hw_cvtimeout(ath_dev_t dev, HAL_BOOL opt);
void ath_txbf_print_cv_cache(ath_dev_t dev);

void ath_txbf_set_rpt_received(ath_node_t node);
bool ath_txbf_chk_rpt_frm(struct ieee80211_frame *wh);
#endif /* ATH_SUPPORT_TxBF */

#endif /* ATH_TXBF_H */
