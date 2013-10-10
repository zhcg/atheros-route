/*
 * Copyright (c) 2010, Atheros Communications Inc.
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

/*
 * Simple Atheros-specific tool to inspect and monitor network traffic
 * statistics.
 *  athstats [-i interface] 
 * (default interface is wifi0).  If interval is specified a rolling output
 * is displayed every interval seconds.
 */
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/wireless.h>

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>

#include "ah_desc.h"
#include "if_athioctl.h"

#define IS_RSSI_VALID(_rssi) ((_rssi) && (_rssi != -0x80))

static const struct {
    u_int       phyerr;
    const char* desc;
} phyerrdescriptions[] = {
    {HAL_PHYERR_UNDERRUN                , "phy tx underrun"         },
    {HAL_PHYERR_TIMING                  , "phy timing error"        },
    {HAL_PHYERR_PARITY                  , "Illegal Parity"          },
    {HAL_PHYERR_RATE                    , "Illegal Rate"            },
    {HAL_PHYERR_LENGTH                  , "Illegal Length"          },
    {HAL_PHYERR_RADAR                   , "Radar detect"            },
	{HAL_PHYERR_SERVICE                 , "Illegal service"         },
	{HAL_PHYERR_TOR                     , "Transmit override receive"},

    {HAL_PHYERR_OFDM_TIMING           ,  "phy ofdm timing"          },
    {HAL_PHYERR_OFDM_SIGNAL_PARITY    ,  "phy ofdm signal parity"   },
    {HAL_PHYERR_OFDM_RATE_ILLEGAL     ,  "phy ofdm rate illegal"    },
    {HAL_PHYERR_OFDM_LENGTH_ILLEGAL   ,  "phy ofdm length illegal"  },
    {HAL_PHYERR_OFDM_POWER_DROP       ,  "phy ofdm power drop"      },
    {HAL_PHYERR_OFDM_SERVICE          ,  "phy ofdm service"         },
    {HAL_PHYERR_OFDM_RESTART          ,  "phy ofdm restart"         },
    {HAL_PHYERR_CCK_TIMING            ,  "phy cck timing"           },
    {HAL_PHYERR_CCK_HEADER_CRC        ,  "phy cck header crc"       },
    {HAL_PHYERR_CCK_RATE_ILLEGAL      ,  "phy cck rate illegal"     },
    {HAL_PHYERR_CCK_SERVICE           ,  "phy cck service"          },
    {HAL_PHYERR_CCK_RESTART           ,  "phy cck restart"          },
    {HAL_PHYERR_CCK_LENGTH_ILLEGAL    ,  "phy cck length illegal"   },
    {HAL_PHYERR_CCK_POWER_DROP        ,  "phy cck power drop"       },
    {HAL_PHYERR_HT_CRC_ERROR          ,  "phy ht crc error"         },
    {HAL_PHYERR_HT_LENGTH_ILLEGAL     ,  "phy ht length illegal"    },
    {HAL_PHYERR_HT_RATE_ILLEGAL       ,  "phy ht rate illegal"      },
};

static char *qdesc[10] = { "BK","BE","VI","VO","","","","UAPSD","CAB","BEACON"};

static void
printstats(FILE *fd, const struct ath_stats *stats)
{
#define N(a)    (sizeof(a) / sizeof(a[0]))

#define STAT(x,fmt) \
    if (stats->ast_##x) fprintf(fd, "%u " fmt "\n", stats->ast_##x)
#define STAT64(x,fmt) \
    if (stats->ast_##x) fprintf(fd, "%llu " fmt "\n", \
	(long long unsigned int) stats->ast_##x)
#define STAT_PEEK_11N(_x)   stats->ast_11n_stats._x
#define STAT_11N(x, descr) \
    fprintf(fd, "%10u " descr "\n", STAT_PEEK_11N(x))
#define STAT_11N_P(x, y, descr) \
    fprintf(fd, "%10.2f " descr "\n", \
            100*(float)STAT_PEEK_11N(x)/((float)(STAT_PEEK_11N(y))+1))
#define STAT_FMT(x,fmt) \
    if (stats->ast_##x) fprintf(fd, "%10u " fmt "\n", stats->ast_##x)

    int i, j;

#ifdef VOW_LOGLATENCY
    fprintf(fd, "Retry Delay Distribution\n");
    fprintf(fd, "========================\n");
    fprintf(fd, "RDD0 : ");
    for(i=0;i<45;i++) {
      fprintf(fd, "%u : ", stats->ast_retry_delay[0][i]);
    }
    fprintf(fd, "\n");
    fprintf(fd, "RDD1 : ");
    for(i=0;i<45;i++) {
      fprintf(fd, "%u : ", stats->ast_retry_delay[1][i]);
    }
    fprintf(fd, "\n");
    fprintf(fd, "RDD2 : ");
    for(i=0;i<45;i++) {
      fprintf(fd, "%u : ", stats->ast_retry_delay[2][i]);
    }
    fprintf(fd, "\n");
    fprintf(fd, "RDD3 : ");
    for(i=0;i<45;i++) {
      fprintf(fd, "%u : ", stats->ast_retry_delay[3][i]);
    }
    fprintf(fd, "\n");
    fprintf(fd, "RDD4 : ");
    for(i=0;i<45;i++) {
      fprintf(fd, "%u : ", stats->ast_retry_delay[4][i]);
    }
    fprintf(fd, "\n");

    fprintf(fd, "Queue Delay Distribution\n");
    fprintf(fd, "========================\n");
    fprintf(fd, "QDD0 : ");
    for(i=0;i<45;i++) {
      fprintf(fd, "%u : ", stats->ast_queue_delay[0][i]);
    }
    fprintf(fd, "\n");
    fprintf(fd, "QDD1 : ");
    for(i=0;i<45;i++) {
      fprintf(fd, "%u : ", stats->ast_queue_delay[1][i]);
    }
    fprintf(fd, "\n");
    fprintf(fd, "QDD2 : ");
    for(i=0;i<45;i++) {
      fprintf(fd, "%u : ", stats->ast_queue_delay[2][i]);
    }
    fprintf(fd, "\n");
    fprintf(fd, "QDD3 : ");
    for(i=0;i<45;i++) {
      fprintf(fd, "%u : ", stats->ast_queue_delay[3][i]);
    }
    fprintf(fd, "\n");
    fprintf(fd, "QDD4 : ");
    for(i=0;i<45;i++) {
      fprintf(fd, "%u : ", stats->ast_queue_delay[4][i]);
    }
    fprintf(fd, "\n");
#endif

    STAT(watchdog, "watchdog timeouts");
    STAT(hardware, "hardware error interrupts");
    STAT(bmiss, "beacon miss interrupts");
    STAT(rxorn, "recv overrun interrupts");
    STAT(rxeol, "recv eol interrupts");
    STAT(txurn, "txmit underrun interrupts");
    STAT(txto,  "global txmit timeout interrupts");
    STAT(cst,  "carrier sense timeout interrupts");
    STAT(mib,  "# mib interrupts");
    STAT(tx_packets, "# packets sent on the interface");
    STAT(tx_mgmt, "tx management frames");

    STAT(tx_discard, "tx frames discarded prior to association");
    STAT(tx_invalid, "tx frames discarded 'cuz device gone");
    STAT(tx_qstop, "tx queue stopped because full");
    STAT(tx_encap, "tx encapsulation failed");
    STAT(tx_nonode, "tx failed 'cuz no node");
    STAT(tx_nobuf, "tx failed 'cuz no tx buffer (data)");
    STAT(tx_stop, "Number of times netif_stop called");
    STAT(tx_resume, "Number of times netif_wake called");
    STAT(tx_nobufmgt, "tx failed 'cuz no tx buffer (mgt)");
    STAT(tx_xretries, "tx failed 'cuz too many retries");
    STAT(tx_fifoerr, "tx failed 'cuz FIFO underrun");
    STAT(tx_filtered, "tx failed 'cuz xmit filtered");
    STAT(tx_badrate, "tx failed 'cuz bogus xmit rate");
    STAT(tx_noack, "tx frames with no ack marked");


    STAT(tx_cts, "tx frames with cts enabled");
    STAT(tx_shortpre, "tx frames with short preamble");
    STAT(tx_altrate, "tx frames with an alternate rate");
    STAT(tx_protect, "tx frames with 11g protection");
    STAT(rx_orn, "rx failed 'cuz of desc overrun");
    STAT(rx_badcrypt, "rx failed 'cuz decryption");
    STAT(rx_badmic, "rx failed 'cuz MIC failure");
    STAT(rx_nobuf, "rx setup failed 'cuz no skbuff");

	STAT(tx_rssi, "tx rssi of last ack");
    STAT64(rx_bytes, "total number of bytes received");
    STAT64(tx_bytes, "total number of bytes transmitted");


/* PHY statistics */

    if (IS_RSSI_VALID(stats->ast_tx_rssi_ctl0))
        fprintf(fd, "rssi of last ack[ctl, ch0]: %u\n",
                stats->ast_tx_rssi_ctl0);

    if (IS_RSSI_VALID(stats->ast_tx_rssi_ctl1))
        fprintf(fd, "rssi of last ack[ctl, ch1]: %u\n",
                stats->ast_tx_rssi_ctl1);

    if (IS_RSSI_VALID(stats->ast_tx_rssi_ctl2))
        fprintf(fd, "rssi of last ack[ctl, ch2]: %u\n",
                stats->ast_tx_rssi_ctl2);
    if (IS_RSSI_VALID(stats->ast_tx_rssi_ext0))
        fprintf(fd, "rssi of last ack[ext, ch0]: %u\n",
                stats->ast_tx_rssi_ext0);
    if (IS_RSSI_VALID(stats->ast_tx_rssi_ext1))
        fprintf(fd, "rssi of last ack[ext, ch1]: %u\n",
                stats->ast_tx_rssi_ext1);
    if (IS_RSSI_VALID(stats->ast_tx_rssi_ext2))
        fprintf(fd, "rssi of last ack[ext, ch2]: %u\n",
                stats->ast_tx_rssi_ext2);


    STAT(rx_rssi, "rx rssi from histogram [combined]");

    if (IS_RSSI_VALID(stats->ast_rx_rssi_ctl0))
        fprintf(fd, "rssi of last rcv[ctl, ch0]: %u\n",
                stats->ast_rx_rssi_ctl0);

    if (IS_RSSI_VALID(stats->ast_rx_rssi_ctl1))
        fprintf(fd, "rssi of last rcv[ctl, ch1]: %u\n",
                stats->ast_rx_rssi_ctl1);

    if (IS_RSSI_VALID(stats->ast_rx_rssi_ctl2))
        fprintf(fd, "rssi of last rcv[ctl, ch2]: %u\n",
                stats->ast_rx_rssi_ctl2);

    if (IS_RSSI_VALID(stats->ast_rx_rssi_ext0))
        fprintf(fd, "rssi of last rcv[ext, ch0]: %u\n",
                stats->ast_rx_rssi_ext0);

    if (IS_RSSI_VALID(stats->ast_rx_rssi_ext1))
        fprintf(fd, "rssi of last rcv[ext, ch1]: %u\n",
                stats->ast_rx_rssi_ext1);

    if (IS_RSSI_VALID(stats->ast_rx_rssi_ext2))
        fprintf(fd, "rssi of last rcv[ext, ch2]: %u\n",
                stats->ast_rx_rssi_ext2);



    STAT(be_xmit, "beacons transmitted");
    STAT(be_nobuf, "no skbuff available for beacon");
    STAT(per_cal, "periodic calibrations");
    STAT(per_calfail, "periodic calibration failures");
    STAT(per_rfgain, "rfgain value change");
    STAT(rate_calls, "rate control checks");
    STAT(rate_raise, "rate control raised xmit rate");
    STAT(rate_drop, "rate control dropped xmit rate");

    fprintf(fd, "Antenna profile:\n");
    STAT(ant_defswitch, "switched default/rx antenna");
    STAT(ant_txswitch, "tx antenna switches");

    for (i = 0; i < 8; i++)
        if (stats->ast_ant_rx[i] || stats->ast_ant_tx[i])
            fprintf(fd, "[%u] tx %8u rx %8u\n", i,
                stats->ast_ant_tx[i], stats->ast_ant_rx[i]);


    STAT(bb_hang, "baseband hangs detected");
    STAT(mac_hang, "mac hangs detected");

#ifdef ATH_SUPPORT_UAPSD
    /*
     * UAPSD stats
     */
    if (stats->ast_uapsdqnul_pkts)
        fprintf(fd, "\nUAPSD stats\n");
    STAT_FMT(uapsdqnulbf_unavail,   "no qos null buffers available");
    STAT_FMT(uapsdqnul_pkts,        "qos null frames sent");
    STAT_FMT(uapsdqnulcomp,         "qos null frames completed");
    STAT_FMT(uapsdnodeinvalid,      "trigger for non-uapsd node");
    STAT_FMT(uapsddataqueued,       "qos data frames queued");
    STAT_FMT(uapsdeospdata,         "qos data with eosp sent");
    STAT_FMT(uapsddata_pkts,        "qos data frames sent");
    STAT_FMT(uapsddatacomp,         "qos data frames completed");
    STAT_FMT(uapsdedmafifofull,     "qos trigger failures edma fifo full");
#endif



    /*
     * 11n stats
     */
    fprintf(fd, "\n11n stats\n");
    STAT_11N(tx_pkts,            "total tx data packets");
    STAT_11N(tx_checks,          "tx drops in wrong state");
    STAT_11N(tx_drops,           "tx drops due to qdepth limit");
    STAT_11N(tx_minqdepth,       "tx when h/w queue depth is low");
    STAT_11N(tx_queue,           "tx pkts when h/w queue is busy");
    STAT_11N(tx_comps,           "tx completions");
    STAT_11N(tx_stopfiltered,    "tx pkts filtered for requeueing");
    STAT_11N(tx_qnull,           "txq empty occurences");
    STAT_11N(tx_noskbs,          "tx no skbs for encapsulations");
    STAT_11N(tx_nobufs,          "tx no descriptors");
    STAT_11N(tx_badsetups,       "tx key setup failures");
    STAT_11N(tx_normnobufs,      "tx no desc for legacy packets");
    STAT_11N(tx_schednone,       "tx schedule pkt queue empty");
    STAT_11N(tx_bars,            "tx bars sent");
    STAT_11N(txbar_compretries,  "tx bar retries sent");
    STAT_11N(txbar_errlast,      "tx bar last frame failed");
    STAT_11N(txbar_xretry,       "tx bar excessive retries");
    STAT_11N(tx_compunaggr,      "tx unaggregated frame completions");
    STAT_11N(txunaggr_xretry,    "tx unaggregated excessive retries");
    STAT_11N(txunaggr_compretries, "tx unaggregated unacked frames");
    STAT_11N(txunaggr_errlast,   "tx unaggregated last frame failed");
    STAT_11N(tx_compaggr,        "tx aggregated completions");
    STAT_11N(tx_bawadv,          "tx block ack window advanced");
    STAT_11N(tx_bawretries,      "tx block ack window retries");
    STAT_11N(tx_bawnorm,         "tx block ack window additions");
    STAT_11N(tx_bawupdates,      "tx block ack window updates");
    STAT_11N(tx_bawupdtadv,      "tx block ack window advances");
    STAT_11N(tx_retries,         "tx retries of sub frames");
    STAT_11N(tx_xretries,        "tx excessive retries of aggregates");
    STAT_11N(txaggr_noskbs,      "tx no skbs for aggr encapsualtion");
    STAT_11N(txaggr_nobufs,      "tx no desc for aggr");
    STAT_11N(txaggr_badkeys,     "tx enc key setup failures");
    STAT_11N(txaggr_schedwindow, "tx no frame scheduled: baw limited");
    STAT_11N(txaggr_single,      "tx frames not aggregated");
    STAT_11N(txaggr_mimo,        "tx frames aggregated for mimo");
    STAT_11N(txaggr_compgood,    "tx aggr good completions");
    STAT_11N(txaggr_compxretry,  "tx aggr excessive retries");
    STAT_11N(txaggr_compretries, "tx aggr unacked subframes");
    STAT_11N(txaggr_prepends,    "tx aggr old frames requeued");
    STAT_11N(txaggr_filtered,    "filtered aggr packet");
    STAT_11N(txaggr_fifo,        "fifo underrun of aggregate");
    STAT_11N(txaggr_xtxop,       "txop exceeded for an aggregate");
    STAT_11N(txaggr_desc_cfgerr, "aggregate descriptor config error");
    STAT_11N(txaggr_data_urun,   "data underrun for an aggregate");
    STAT_11N(txaggr_delim_urun,  "delimiter underrun for an aggregate");
    STAT_11N(txaggr_errlast,     "tx aggr: last sub-frame failed");
    STAT_11N(txaggr_longretries, "tx aggr: h/w long retries");
    STAT_11N(txaggr_shortretries,"tx aggr: h/w short retries");
    STAT_11N(txaggr_timer_exp,   "tx aggr: tx timer expired");
    STAT_11N(txaggr_babug,       "tx aggr: BA state is not updated");
    STAT_11N(txaggr_badtid,      "tx aggr: BA bad tid");
    STAT_11N(txrifs_single,      "tx frames not aggregated");
    STAT_11N(txrifs_babug,       "tx rifs: BA state is not updated");
    STAT_11N(txrifs_compretries, "tx rifs: unacked subframes");
    STAT_11N(txrifs_bar_alloc,   "tx rifs: bar frames allocated");
    STAT_11N(txrifs_bar_freed,   "tx rifs: bar frames freed");
    STAT_11N(txrifs_compgood,    "tx rifs: good completions");
    STAT_11N(tx_comprifs,        "tx rifs completions");
    STAT_11N(tx_compnorifs,      "tx non-rifs frame completions");
    STAT_11N(txrifs_prepends,    "tx rifs old frames requeued");
    STAT_11N(rx_pkts,            "rx pkts");
    STAT_11N(rx_aggr,            "rx aggregated packets");
    STAT_11N(rx_aggrbadver,      "rx pkts with bad version");
    STAT_11N(rx_bars,            "rx bars");
    STAT_11N(rx_nonqos,          "rx non qos-data frames");
    STAT_11N(rx_seqreset,        "rx sequence resets");
    STAT_11N(rx_oldseq,          "rx old packets");
    STAT_11N(rx_bareset,         "rx block ack window reset");
    STAT_11N(rx_baresetpkts,     "rx pts indicated due to baw resets");
    STAT_11N(rx_dup,             "rx duplicate pkts");
    STAT_11N(rx_baadvance,       "rx block ack window advanced");
    STAT_11N(rx_recvcomp,        "rx pkt completions");
    STAT_11N(rx_bardiscard,      "rx bar discarded");
    STAT_11N(rx_barcomps,        "rx pkts unblocked on bar reception");
    STAT_11N(rx_barrecvs,        "rx pkt completions on bar reception");
    STAT_11N(rx_skipped,         "rx pkt sequences skipped on timeout");
    STAT_11N(rx_comp_to,         "rx indications due to timeout");
    STAT_11N(wd_tx_active,       "watchdog: tx is active");
    STAT_11N(wd_tx_inactive,     "watchdog: tx is not active");
    STAT_11N(wd_tx_hung,         "watchdog: tx is hung");
    STAT_11N(wd_spurious,        "watchdog: spurious tx hang");
    STAT_11N(tx_requeue,         "filter & requeue on 20/40 transitions");
    STAT_11N(tx_drain_txq,       "draining tx queue on error");
    STAT_11N(tx_drain_tid,       "draining tid buf queue on error");
    STAT_11N(tx_cleanup_tid,     "draining tid buf queue on node cleanup");
    STAT_11N(tx_drain_bufs,      "buffers drained from pending tid queue");
    STAT_11N(tx_tidpaused,       "tid paused");
    STAT_11N(tx_tidresumed,      "tid resumed");
    STAT_11N(tx_unaggr_filtered, "unaggregated tx pkts filtered");
    STAT_11N(tx_aggr_filtered,   "aggregated tx pkts filtered");
    STAT_11N(tx_filtered,        "total sub-frames filtered");
    STAT_11N(rx_rb_on,           "rb on");
    STAT_11N(rx_rb_off,          "rb off");
    STAT_11N(rx_dsstat_err,      "rx descriptor status corrupted");

    /* Per Queue Statistics */
    for (i=0;i<10;++i) {
        if (stats->ast_txq_packets[i])
            fprintf(fd,"TXQ[%d]:%s tx %d xretry %d fifoerr %d filtered %d no buffs %d\n",
                    i,qdesc[i], stats->ast_txq_packets[i],stats->ast_txq_xretries[i],
                    stats->ast_txq_fifoerr[i],stats->ast_txq_filtered[i], stats->ast_txq_nobuf[i]);
    }

    /* Percentages */
    STAT_11N_P(txunaggr_xretry, tx_compunaggr,
               "tx unaggregated excessive retry percent");
    STAT_11N_P(txaggr_longretries, tx_compaggr,
               "tx aggregated long retry percent");
    STAT_11N_P(txaggr_compxretry, tx_compaggr,
               "tx aggregated excessive retry percent");
    STAT_11N_P(tx_retries, tx_bawadv,
               "tx aggregate subframe retry percent");
    STAT_11N_P(tx_xretries, tx_bawadv,
               "tx aggregate subframe excessive retry percent");
#ifdef ATH_SUPPORT_VOWEXT
    fprintf(fd, "VOW STATS: ul_tx_calls: %d %d %d %d ath_txq_calls: %d %d %d %d drops(be/bk): %d %d\n",
            stats->ast_vow_ul_tx_calls[0], stats->ast_vow_ul_tx_calls[1],
            stats->ast_vow_ul_tx_calls[2], stats->ast_vow_ul_tx_calls[3],
            stats->ast_vow_ath_txq_calls[0], stats->ast_vow_ath_txq_calls[1],
            stats->ast_vow_ath_txq_calls[2], stats->ast_vow_ath_txq_calls[3],
            stats->ast_vow_ath_be_drop, stats->ast_vow_ath_bk_drop);
#endif
#if UMAC_SUPPORT_VI_DBG
        fprintf(fd, "Timestamp  RSSIC0 RSSIC1 RSSIC2 RSSIE0 RSSIE1 RSSIE2  RSSI  EVM0 EVM1 EVM2 RXRATE TXFRAMECNT RXFRAMECNT RXCLRCOUNT RXEXCLRCNT CYCLECOUNT\n");
    for (i = 0; i < ATH_STATS_VI_LOG_LEN; i++) {
                fprintf(fd, "0x%08x %06d %06d %06d %06d %06d %06d %06d %4hhd %4hhd %4hhd 0x%04x %010d %010u %010u %010u %010u\n",
                             stats->vi_timestamp[i], stats->vi_rssi_ctl0[i], stats->vi_rssi_ctl1[i], stats->vi_rssi_ctl2[i],
                             stats->vi_rssi_ext0[i], stats->vi_rssi_ext1[i], stats->vi_rssi_ext2[i], stats->vi_rssi[i],
                             stats->vi_evm0[i], stats->vi_evm1[i], stats->vi_evm2[i], stats->vi_rs_rate[i],
                             stats->vi_tx_frame_cnt[i], stats->vi_rx_frame_cnt[i], stats->vi_rx_clr_cnt[i], stats->vi_rx_ext_clr_cnt[i],
                             stats->vi_cycle_cnt[i]);
                }
#endif

#ifdef ATH_SUPPORT_TxBF
    fprintf(fd,"\nHT Tx Rate STATS:\n");
    fprintf(fd," mcs 0- mcs 7 STATS:");
    for (i=0 ;i<8 ;i++)
        fprintf(fd,"%#6d,",stats->ast_mcs_count[i]);
    fprintf(fd,"\n mcs 8- mcs15 STATS:");
    for (i=0 ;i<8 ;i++)
        fprintf(fd,"%#6d,",stats->ast_mcs_count[i+8]);
    fprintf(fd,"\n mcs16- mcs23 STATS:");
    for (i=0 ;i<8 ;i++)
        fprintf(fd,"%#6d,",stats->ast_mcs_count[i+16]);
        
    fprintf(fd,"\nTxBF STATS:\n");
    fprintf(fd," Sounding sent %d\n",stats->ast_sounding_count);
    fprintf(fd," V/CV received %d\n",stats->ast_txbf_rpt_count);
#endif

    for (i = MAX_BB_PANICS - 1; i >= 0; i--) {
        if (!stats->ast_bb_panic[i].valid)
            continue;

        fprintf(fd, "\n==== BB update: BB status=0x%08x, tsf=0x%08x ====\n",
            stats->ast_bb_panic[i].status, stats->ast_bb_panic[i].tsf);
        fprintf(fd, "** BB state: wd=%u det=%u rdar=%u rOFDM=%d rCCK=%u tOFDM=%u tCCK=%u agc=%u src=%u **\n",
            stats->ast_bb_panic[i].wd, stats->ast_bb_panic[i].det, stats->ast_bb_panic[i].rdar,
            stats->ast_bb_panic[i].rODFM, stats->ast_bb_panic[i].rCCK, stats->ast_bb_panic[i].tODFM,
            stats->ast_bb_panic[i].tCCK, stats->ast_bb_panic[i].agc, stats->ast_bb_panic[i].src);
        fprintf(fd, "** BB WD cntl: cntl1=0x%08x cntl2=0x%08x **\n",
            stats->ast_bb_panic[i].phy_panic_wd_ctl1, stats->ast_bb_panic[i].phy_panic_wd_ctl2);
        fprintf(fd, "** BB mode: BB_gen_controls=0x%08x **\n", 
            stats->ast_bb_panic[i].phy_gen_ctrl);
        if (stats->ast_bb_panic[i].cycles) {
            fprintf(fd, "** BB busy times: rx_clear=%d%%, rx_frame=%d%%, tx_frame=%d%% **\n",
                stats->ast_bb_panic[i].rxc_pcnt, stats->ast_bb_panic[i].rxf_pcnt, stats->ast_bb_panic[i].txf_pcnt);
        }
        fprintf(fd, "==== BB update: done ====\n\n");
    }

#undef STAT_FMT
#undef STAT
#undef STAT64
#undef STAT_PEEK_11N
#undef STAT_11N_P
#undef STAT_11N
#undef N
}

static u_int
getifrate(int s, const char* ifname)
{
    struct iwreq wrq;

    (void) memset(&wrq, 0, sizeof(wrq));
    strncpy(wrq.ifr_name, ifname, IFNAMSIZ);
    wrq.ifr_name[IFNAMSIZ - 1] = '\0';
    if (ioctl(s, SIOCGIWRATE, &wrq) < 0)
        return 0;
    else
        return wrq.u.bitrate.value / 1000000;
}

static u_int
getrssi(int s, const char* ifname)
{
    struct iw_statistics stats;
    struct iwreq wrq;

    (void) memset(&wrq, 0, sizeof(wrq));
    wrq.u.data.pointer = (caddr_t) &stats;
    wrq.u.data.flags = 1;
    strncpy(wrq.ifr_name, ifname, IFNAMSIZ);
    wrq.ifr_name[IFNAMSIZ - 1] = '\0';
    if (ioctl(s, SIOCGIWSTATS, &wrq) < 0)
        return 0;
    else
        return stats.qual.qual;
}

static int
getifstats(const char *ifname, u_long *iframes, u_long *oframes)
{
    FILE * fd = fopen("/proc/net/dev", "r");
    if (fd != NULL) {
        char line[256];
        while (fgets(line, sizeof(line), fd)) {
            char *cp, *tp;

            for (cp = line; isspace(*cp); cp++)
                ;
            if (cp[0] != ifname[0])
                continue;
            for (tp = cp; *tp != ':' && *tp; tp++)
                ;
            if (*tp == ':') {
                *tp++ = '\0';
                if (strcmp(cp, ifname) != 0)
                    continue;
                sscanf(tp, "%*u %lu %*u %*u %*u %*u %*u %*u %*u %lu",
                    iframes, oframes);
                fclose(fd);
                return 1;
            }
        }
        fclose(fd);
    }
    return 0;
}

static int signalled;

static void
catchalarm(int signo)
{
    signalled = 1;
}

int
main(int argc, char *argv[])
{
#ifdef __linux__
    const char *ifname = "wifi0";
#else
    const char *ifname = "ath0";
#endif
    int s;
    struct ifreq ifr;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        err(1, "socket");
    if (argc > 1 && strcmp(argv[1], "-i") == 0) {
        if (argc < 2) {
            fprintf(stderr, "%s: missing interface name for -i\n",
                argv[0]);
            exit(-1);
        }
        ifname = argv[2];
        argc -= 2, argv += 2;
    }
    strncpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';

#if 0
    if (argc > 1) {
        u_long interval = strtoul(argv[1], NULL, 0);
        u_long off;
        int line, omask;
        u_int rate, rssi;
        struct ath_stats cur, total;
        u_long icur, ocur;
        u_long itot, otot;

        if (interval < 1)
            interval = 1;
        signal(SIGALRM, catchalarm);
        signalled = 0;
        alarm(interval);
    banner:
        printf("%8s %7s %7s %7s %6s %6s %6s %7s %4s %4s"
            , "output"
            , "altrate"
            , "short"
            , "long"
            , "xretry"
            , "crcerr"
            , "crypt"
            , "phyerr"
            , "rssi"
            , "rate"
        );
        putchar('\n');
        fflush(stdout);
        line = 0;
    loop:
        rate = getifrate(s, ifr.ifr_name);
        rssi = getrssi(s, ifr.ifr_name);
        if (line != 0) {
            ifr.ifr_data = (caddr_t) &cur;
            if (ioctl(s, SIOCGATHSTATS, &ifr) < 0)
                err(1, ifr.ifr_name);
            if (!getifstats(ifr.ifr_name, &icur, &ocur))
                err(1, ifr.ifr_name);
            printf("%8u %7u %7u %7u %6u %6u %6u %7u %4u %3uM\n"
                , ocur - otot
                , cur.ast_tx_altrate - total.ast_tx_altrate
                , cur.ast_tx_shortretry - total.ast_tx_shortretry
                , cur.ast_tx_longretry - total.ast_tx_longretry
                , cur.ast_tx_xretries - total.ast_tx_xretries
                , cur.ast_rx_crcerr - total.ast_rx_crcerr
                , cur.ast_rx_badcrypt - total.ast_rx_badcrypt
                , cur.ast_rx_phyerr - total.ast_rx_phyerr
                , rssi
                , rate
            );
            total = cur;
            itot = icur;
            otot = ocur;
        } else {
            ifr.ifr_data = (caddr_t) &total;
            if (ioctl(s, SIOCGATHSTATS, &ifr) < 0)
                err(1, ifr.ifr_name);
            if (!getifstats(ifr.ifr_name, &itot, &otot))
                err(1, ifr.ifr_name);
            printf("%8u %8u %7u %7u %7u %6u %6u %6u %7u %4u %3uM\n"
                , itot - total.ast_rx_mgt
                , otot
                , total.ast_tx_altrate
                , total.ast_tx_shortretry
                , total.ast_tx_longretry
                , total.ast_tx_xretries
                , total.ast_rx_crcerr
                , total.ast_rx_badcrypt
                , total.ast_rx_phyerr
                , rssi
                , rate
            );
        }
        fflush(stdout);
        omask = sigblock(sigmask(SIGALRM));
        if (!signalled)
            sigpause(0);
        sigsetmask(omask);
        signalled = 0;
        alarm(interval);
        line++;
        if (line == 21)     /* XXX tty line count */
            goto banner;
        else
            goto loop;
        /*NOTREACHED*/
    } else {
#endif
        struct ath_stats stats = { 0 };

        ifr.ifr_data = (caddr_t) &stats;
        if (ioctl(s, SIOCGATHSTATS, &ifr) < 0)
	    err(1, "%s", ifr.ifr_name);
        printstats(stdout,  &stats);
#if 0
    }
#endif
    return 0;
}
