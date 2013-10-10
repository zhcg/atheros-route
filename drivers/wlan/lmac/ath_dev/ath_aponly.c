/*
 * Copyright (c) 2005, Atheros Communications Inc.
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

/*! \file ath_aponly.c
**
** export lmac symbols for the aponly code
**
*/

#include "osdep.h"
#include "ath_internal.h"
#include "ath_hwtimer.h"
#include "if_athvar.h"

#if UMAC_SUPPORT_APONLY

OS_EXPORT_SYMBOL(ath_tx_complete_bar);
OS_EXPORT_SYMBOL(ath_tx_pause_tid);
OS_EXPORT_SYMBOL(ath_tx_resume_tid);
OS_EXPORT_SYMBOL(ath_tx_send_normal);
OS_EXPORT_SYMBOL(ath_tx_send_ampdu);
OS_EXPORT_SYMBOL(ath_txq_schedule);
OS_EXPORT_SYMBOL(ath_tx_num_badfrms);
OS_EXPORT_SYMBOL(ath_tx_complete_aggr_rifs);

OS_EXPORT_SYMBOL(ath_tx_complete_buf);
OS_EXPORT_SYMBOL(ath_tx_update_stats);
OS_EXPORT_SYMBOL(ath_txq_depth);
OS_EXPORT_SYMBOL(ath_buf_set_rate);
OS_EXPORT_SYMBOL(ath_txto_tasklet);
OS_EXPORT_SYMBOL(ath_tx_get_rts_retrylimit);

OS_EXPORT_SYMBOL(ath_tx_uapsd_complete);
OS_EXPORT_SYMBOL(ath_tx_queue_uapsd);

OS_EXPORT_SYMBOL(ath_pwrsave_set_state);
#if ATH_SUPPORT_FLOWMAC_MODULE
//ATH_SUPPORT_VOWEXT
OS_EXPORT_SYMBOL(ath_netif_stop_queue);
#endif
OS_EXPORT_SYMBOL(ath_edmaAllocRxbufsForFreeList);
OS_EXPORT_SYMBOL(ath_rx_edma_intr);
OS_EXPORT_SYMBOL(ath_hw_hang_check);
OS_EXPORT_SYMBOL(ath_beacon_tasklet);
OS_EXPORT_SYMBOL(ath_bstuck_tasklet);
OS_EXPORT_SYMBOL(ath_beacon_config);
OS_EXPORT_SYMBOL(ath_bmiss_tasklet);

OS_EXPORT_SYMBOL(ath_allocRxbufsForFreeList);
#if ATH_SUPPORT_DESCFAST
OS_EXPORT_SYMBOL(ath_rx_proc_descfast);
#endif

#ifdef ATH_GEN_TIMER
OS_EXPORT_SYMBOL(ath_gen_timer_isr);
OS_EXPORT_SYMBOL(ath_gen_timer_tsfoor_isr);
#endif

OS_EXPORT_SYMBOL(ath_handle_rx_intr);
OS_EXPORT_SYMBOL(__wbuf_unmap_sg);
OS_EXPORT_SYMBOL(bus_dma_sync_single);
OS_EXPORT_SYMBOL(bus_unmap_single);
OS_EXPORT_SYMBOL(bus_map_single);
#if ATH_SUPPORT_PAPRD
OS_EXPORT_SYMBOL(ath_tx_paprd_complete);
#endif
OS_EXPORT_SYMBOL(ath_tx_tasklet);

OS_EXPORT_SYMBOL(ath_rx_process_uapsd);
OS_EXPORT_SYMBOL(ath_rx_bf_handler);
#if ATH_SUPPORT_LED
OS_EXPORT_SYMBOL(ath_led_report_data_flow);
#endif
OS_EXPORT_SYMBOL(ath_rx_requeue);
OS_EXPORT_SYMBOL(ath_rx_bf_process);
OS_EXPORT_SYMBOL(ath_bar_rx);
OS_EXPORT_SYMBOL(ath_ampdu_input);
OS_EXPORT_SYMBOL(ath_set_timer_period);
OS_EXPORT_SYMBOL(ath_setdefantenna);
OS_EXPORT_SYMBOL(ath_txbf_chk_rpt_frm);
#endif
