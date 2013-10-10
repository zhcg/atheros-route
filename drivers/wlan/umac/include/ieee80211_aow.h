/*
 * =====================================================================================
 *
 *       Filename:  ieee80211_aow.h
 *
 *    Description:  Header file for AOW Functions
 *
 *        Version:  1.0
 *        Created:  Friday 13 November 2009 04:47:44  IST
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *        Company:  
 *
 * =====================================================================================
 */

#ifndef IEEE80211_AOW_H
#define IEEE80211_AOW_H

#if  ATH_SUPPORT_AOW

#include "if_upperproto.h"
#include "ieee80211_aow_shared.h"
#include "if_athrate.h"
#include "ah_desc.h"
#include "ath_desc.h"
#include "ieee80211.h"

#define USE_NEW_AIR_FORMAT (1)
#define AOW_LINUX_VERSION  (KERNEL_VERSION(2,6,15))

/* data structures */

struct audio_pkt {
    u_int32_t signature;
    u_int32_t seqno;
    u_int64_t timestamp;
#if USE_NEW_AIR_FORMAT
    u_int32_t pkt_len;
    u_int32_t params;
#endif
    u_int32_t crc32;
}__packed;


/* L2 Packet Error Stats */
struct l2_pkt_err_stats {
    bool isInUse;
    bool wasAudStopped;
    u_int32_t srNo;
    u_int32_t fcs_ok;
    u_int32_t fcs_nok;
    systime_t start_time;
    systime_t end_time;
};

/* Packet Lost Rate Stats */
struct pkt_lost_rate_stats {
    bool isInUse;
    bool wasAudStopped;
    u_int32_t srNo;
    u_int32_t num_mpdus;      /* Orig stats: Number of MPDUs received
                                    w/o_fail */
    u_int32_t nok_framecount; /* Orig stats: Number of MPDUs that were
                                    delayed*/
    u_int32_t late_mpdu;      /* Orig stats: Number of MPDUs that came
                                    late */
    u_int32_t start_seqno;
    u_int16_t start_subseqno; 
    u_int32_t end_seqno;
    u_int16_t end_subseqno; 
    systime_t start_time;
    systime_t end_time;
};

/* MCS statistics */

/* No. of MCS we are interested in, from MCS 0 upwards.
   If this is ever changed, keep the code in aow_update_mcsinfo()
   in sync with the changed value */
#define ATH_AOW_NUM_MCS             (16)


/* TODO: Re-architect the code to place it in lmac. Till then,
   use custom defined values and structures.*/

/* Start of custom definitions to be removed */

#define ATH_AOW_TXMAXTRY            (13)

typedef enum {
/* MCS Rates */
    AOW_ATH_MCS0 = 0x80,
    AOW_ATH_MCS1,
    AOW_ATH_MCS2,
    AOW_ATH_MCS3,
    AOW_ATH_MCS4,
    AOW_ATH_MCS5,
    AOW_ATH_MCS6,
    AOW_ATH_MCS7,
    AOW_ATH_MCS8,
    AOW_ATH_MCS9,
    AOW_ATH_MCS10,
    AOW_ATH_MCS11,
    AOW_ATH_MCS12,
    AOW_ATH_MCS13,
    AOW_ATH_MCS14,
    AOW_ATH_MCS15,
} AOW_ATH_MCS_RATES;

#define AOW_WME_BA_BMP_SIZE     64
#define AOW_WME_MAX_BA          AOW_WME_BA_BMP_SIZE

/*
 * desc accessor macros
 */
#define ATH_AOW_DS_BA_SEQ(_ts)          (_ts)->ts_seqnum
#define ATH_AOW_DS_BA_BITMAP(_ts)       (&(_ts)->ba_low)
#define ATH_AOW_DS_TX_BA(_ts)           ((_ts)->ts_flags & HAL_TX_BA)
#define ATH_AOW_DS_TX_STATUS(_ts)       (_ts)->ts_status
#define ATH_AOW_DS_TX_FLAGS(_ts)        (_ts)->ts_flags

#define ATH_AOW_BA_INDEX(_st, _seq)     (((_seq) - (_st)) & (IEEE80211_SEQ_MAX - 1))
#define ATH_AOW_BA_ISSET(_bm, _n)       (((_n) < (AOW_WME_BA_BMP_SIZE)) &&          \
                                        ((_bm)[(_n) >> 5] & (1 << ((_n) & 31))))

/* End of custom definitions to be removed */

struct mcs_stats_element {
    u_int32_t ok_txcount[ATH_AOW_TXMAXTRY];
    u_int32_t nok_txcount[ATH_AOW_TXMAXTRY];
};

struct mcs_stats {
    struct mcs_stats_element mcs_info[ATH_AOW_NUM_MCS];    
};

#define ATH_AOW_ACCESS_APKT(wbuf)   ((struct audio_pkt *)\
                                         ((char*)(wbuf_header((wbuf))) +\
                                         ieee80211_anyhdrspace(ic,\
                                              wbuf_header((wbuf))) +\
                                         sizeof(struct llc)))

#define ATH_AOW_ACCESS_RECVR(wbuf)   ((struct ether_addr*)\
                                         (((struct ieee80211_frame *)\
                                                wbuf_header((wbuf)))->i_addr1))

#define ATH_AOW_ACCESS_WLANSEQ(wbuf) ((le16toh(*(u_int16_t *)\
                                            (((struct ieee80211_frame *)\
                                                   wbuf_header((wbuf)))->i_seq)))\
                                       >> IEEE80211_SEQ_SEQ_SHIFT)

/* defines */
#define ATH_AOW_SEQNO_LIMIT             4096

#define ATH_AOW_SIGNATURE               0xdeadbeef
#define ATH_AOW_RESET_COMMAND           0xa5a5a5a5
#define ATH_AOW_START_COMMAND           0xb5b5b5b5
#define ATH_AOW_SET_CHANNEL_COMMAND     0x00000001

#define ATH_AOW_SIG_SIZE                sizeof(u_int32_t)
#define ATH_AOW_TS_SIZE                 sizeof(u_int64_t)
#define ATH_QOS_FIELD_SIZE              sizeof(u_int16_t)
#define ATH_AOW_SEQNO_SIZE              sizeof(u_int32_t)
#define ATH_AOW_LENGTH_SIZE             sizeof(u_int16_t)
#define ATH_AOW_PARAMS_SIZE             sizeof(u_int16_t)

#define ATH_AOW_PARAMS_SUB_FRAME_NO_S       (0)
#define ATH_AOW_PARAMS_SUB_FRAME_NO_MASK    (0x3)
#define ATH_AOW_PARAMS_INTRPOL_ON_FLG_MASK  (0x1)
#define ATH_AOW_PARAMS_INTRPOL_ON_FLG_S     (2)
#define ATH_AOW_PARAMS_LOGSYNC_FLG_MASK     (0x1)
#define ATH_AOW_PARAMS_LOGSYNC_FLG_S        (3)

/* Sample size is 16 bits */
#define ATH_AOW_SAMPLE_SIZE (sizeof(u_int16_t))

/* 
 * This are the defines for the MPDU/AMPDU structure. 
 * There are 4 MPDUs, with 2 ms worth of 
 * data per MPDU. 
 */
#define ATH_AOW_NUM_MPDU_LOG2       (2)
#define ATH_AOW_NUM_MPDU            (1<<ATH_AOW_NUM_MPDU_LOG2)
#define ATH_AOW_NUM_SAMP_PER_AMPDU  (768) 
#define ATH_AOW_NUM_SAMP_PER_MPDU   (ATH_AOW_NUM_SAMP_PER_AMPDU >>ATH_AOW_NUM_MPDU_LOG2)
#define ATH_AOW_MPDU_MOD_MASK       ((1 << ATH_AOW_NUM_MPDU_LOG2)-1)

#define ATH_AOW_STATS_THRESHOLD     (10000)

#define ATH_AOW_PLAY_LOCAL_MASK 1

#define AUDIO_INFO_BUF_SIZE_LOG2    (5) 
#define AUDIO_INFO_BUF_SIZE         (1<<AUDIO_INFO_BUF_SIZE_LOG2)
#define AUDIO_INFO_BUF_SIZE_MASK    (AUDIO_INFO_BUF_SIZE-1)
#define AOW_ZERO_BUF_IDX            (AUDIO_INFO_BUF_SIZE)
#define AOW_MAX_MISSING_PKT         (20)

#define AOW_MAX_PLAYBACK_SIZE       (12)

#define AOW_CHECK_FOR_DATA_THRESHOLD    9000
#define AOW_LAST_PACKET_TIME_DELTA      8000
#define AOW_PACKET_INTERVAL             8000

/* print macros */
#define IEEE80211_AOW_DPRINTF   printk

/* 
 * Frequency at which errors are simulated. 
 * The subframes for one out of every AOW_EC_ERROR_SIMULATION_FREQUENCY 
 * frames are subjected to errors as given in fmap. 
 */ 
#define AOW_EC_ERROR_SIMULATION_FREQUENCY (10)

#define IS_ETHER_ADDR_NULL(macaddr) (((macaddr[0] == 0 ) &&  \
                                      (macaddr[1] == 0 ) &&  \
                                      (macaddr[2] == 0 ) &&  \
                                      (macaddr[3] == 0 ) &&  \
                                      (macaddr[4] == 0 ) &&  \
                                      (macaddr[5] == 0 ))?TRUE:FALSE)

typedef struct _audio_info {
    u_int32_t seqNo;
    u_int16_t WLANSeqNos[ATH_AOW_NUM_MPDU];
    bool      logSync[ATH_AOW_NUM_MPDU];
    u_int16_t foundBlocks;    
    u_int16_t audBuffer[ATH_AOW_NUM_MPDU][ATH_AOW_NUM_SAMP_PER_MPDU];
    u_int16_t inUse;
    u_int16_t params;
    u_int16_t datalen;
    u_int64_t startTime;
    u_int8_t  localSimCount;
#ifdef ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG
    u_int64_t latency[ATH_AOW_NUM_MPDU];
#endif 
}audio_info;

typedef struct _audio_control {
    u_int16_t audStopped;
    u_int32_t capture_data;
    u_int32_t capture_done;
    u_int32_t force_input;
} audio_ctrl;

#define AOW_DBG_WND_LEN (256)
typedef struct _audio_stats {
    u_int16_t rxFlg;
    u_int32_t num_ampdus;
    u_int32_t num_mpdus;
    u_int32_t num_missing_ampdu;
    u_int32_t num_missing_mpdu;
    u_int32_t num_missing_mpdu_hist[ATH_AOW_NUM_MPDU];
    u_int32_t missing_pos_cnt[ATH_AOW_NUM_MPDU];
    u_int32_t late_mpdu;
    u_int32_t mpdu_not_consumed;
    u_int32_t ampdu_size_short;
    u_int32_t too_late_ampdu;
    u_int32_t datalenCnt;
    u_int64_t lastPktTime;
    u_int64_t expectedPktTime;
    u_int32_t numPktMissing;
    u_int32_t datalenBuf[AOW_DBG_WND_LEN];
    u_int32_t datalenLen[AOW_DBG_WND_LEN];
    u_int32_t dbgCnt;
    u_int32_t frcDataCnt;
    u_int64_t prevTsf;
    u_int32_t sumTime;
    u_int32_t grtCnt;
    u_int32_t consume_aud_cnt;
    u_int32_t consume_aud_abort;
    int16_t before_deint[ATH_AOW_NUM_MPDU][ATH_AOW_NUM_SAMP_PER_MPDU];
    int16_t after_deint[ATH_AOW_NUM_SAMP_PER_AMPDU];
} audio_stats;

typedef struct _ext_stats
{
    /* Packet Latency stats */
    u_int32_t *aow_latency_stats;
    u_int32_t aow_latency_max_bin;
    u_int32_t aow_latency_stat_expired;

    /* L2 Packet Error Stats*/
    
    /* Array for L2 Packet Error Stats FIFO */
    struct l2_pkt_err_stats *aow_l2pe_stats;
    
    /* FIFO array position to write next element to. */
    u_int32_t aow_l2pe_stats_write; 
    
    /* FIFO array position to read next element from. */
    u_int32_t aow_l2pe_stats_read;
    
    /* Current L2 Packet Error Stats Group (to be written into
       for the current interval of ATH_AOW_L2PE_GROUP_INTERVAL seconds) */
    struct l2_pkt_err_stats l2pe_curr;
    
    /* Cumulative L2 Packet Error Stats Group (to be written into
       for the entire duration of measurement). */
    struct l2_pkt_err_stats l2pe_cumltv;

    /* Serial No. to use for the next L2 Packet Error Stats Group */
    u_int32_t l2pe_next_srNo;

    
    /* Packet Lost Rate Stats*/
    
    /* Array for Packet Lost Rate Stats FIFO */
    struct pkt_lost_rate_stats *aow_plr_stats;
    
    /* FIFO array position to write next element to. */
    u_int32_t aow_plr_stats_write; 
    
    /* FIFO array position to read next element from. */
    u_int32_t aow_plr_stats_read;
    
    /* Current Packet Lost Rate Stats Group (to be written into
       for the current interval of ATH_AOW_ES_GROUP_INTERVAL seconds) */
    struct pkt_lost_rate_stats plr_curr;
    
    /* Cumulative Packet Lost Rate Stats Group (to be written into
       for the entire duration of measurement). */
    struct pkt_lost_rate_stats plr_cumltv;

    /* Serial No. to use for the next Packet Lost Rate Stats Group */
    u_int32_t plr_next_srNo;

    /* MCS statistics */
    struct mcs_stats aow_mcs_stats;
    

} ext_stats;

/* TODO : This needs to move to common header
 * file between app and driver module
 */
#define ATH_AOW_MAX_BUFFER_SIZE     786
#define ATH_AOW_MAX_BUFFER_ELEMENTS 16
#define AOW_I2S_DATA_BLOCK_SIZE     24

#define I2S_START_CMD   0x1
#define I2S_STOP_CMD    0x2
#define I2S_PAUSE_CMD   0x4
#define I2S_DMA_START   0x8

#define SET_I2S_START_FLAG(flag)    ((flag) = (flag) | (I2S_START_CMD))
#define CLR_I2S_START_FLAG(flag)    ((flag) = (flag) & ~(I2S_START_CMD))
#define SET_I2S_STOP_FLAG(flag)     ((flag) = (flag) | (I2S_STOP_CMD))
#define CLR_I2S_STOP_FLAG(flag)     ((flag) = (flag) & ~(I2S_STOP_CMD))
#define SET_I2S_PAUSE_FLAG(flag)    ((flag) = (flag) | (I2S_PAUSE_CMD))
#define CLR_I2S_PAUSE_FLAG(flag)    ((flag) = (flag) & ~(I2S_PAUSE_CMD))
#define SET_I2S_DMA_START(flag)     ((flag) = (flag) | (I2S_DMA_START))
#define CLR_I2S_DMA_START(flag)     ((flag) = (flag) & ~(I2S_DMA_START))

#define IS_I2S_START_DONE(flag)     (((flag) & I2S_START_CMD)?1:0)
#define IS_I2S_STOP_DONE(flag)      (((flag) & I2S_STOP_CMD)?1:0)
#define IS_I2S_NEED_DMA_START(flag) (((flag) & I2S_DMA_START)?1:0)



/* function declarations */

extern u_int32_t ieee80211_get_aow_seqno(struct ieee80211com *ic);

extern void ieee80211_aow_attach(struct ieee80211com *ic);
extern void ieee80211_audio_print_capture_data(struct ieee80211com *ic);
extern void ieee80211_set_audio_data_capture(struct ieee80211com *ic);
extern void ieee80211_set_force_aow_data(struct ieee80211com *ic, int32_t flg);
extern void ieee80211_audio_stat_clear(struct ieee80211com *ic);
extern void ieee80211_audio_stat_print(struct ieee80211com *ic);
extern void ar7240_i2sound_dma_pause(int mode);

extern int ieee80211_aow_get_stats(struct ieee80211com *ic);
extern int ieee80211_audio_receive(struct ieee80211vap *vap, wbuf_t wbuf, struct ieee80211_rx_status *rs);
extern int ieee80211_aow_detach(struct ieee80211com *);
extern int ieee80211_aow_cmd_handler(struct ieee80211com* ic, int command);
extern int aow_i2s_init(struct ieee80211com* ic);
extern int aow_i2s_deinit(struct ieee80211com* ic);
extern int ieee80211_consume_audio_data( struct ieee80211com *ic, u_int64_t curTime );
extern int ar7242_i2s_close(void);
extern int ar7242_i2s_open(void);
extern int ar7240_i2s_clk(unsigned long, unsigned long);
extern int ar7242_i2s_write(size_t, const char *);
extern int ar7240_i2sound_dma_start(int);
extern int ar7240_i2sound_dma_resume(int);
extern int ar7242_i2s_fifo_reset(void);
extern int ar7242_i2s_desc_busy(int);
extern int i2s_audio_data_stop(void);
extern int ar7240_i2s_fill_dma(void);

/******************************************************************************
    Functions relating to AoW PoC 2
    Interface functions between USB and Wlan

******************************************************************************/    

extern int aow_get_macaddr(int channel, int index, struct ether_addr* addr);
extern int wlan_aow_tx(char* data, int datalen, int channel, u_int64_t tsf);
extern int ieee80211_send_aow_data_ipformat(struct ieee80211_node *ni, void *data, 
                        int len, u_int32_t seqno, u_int64_t tsf, bool setlogSync);

/* 
 * XXX : This is the interface between wlan and i2s
 *       When adding info, here please make sure that
 *       it is reflected in the i2s side also
 */
typedef struct i2s_stats {
    u_int32_t write_fail;
    u_int32_t rx_underflow;
    u_int32_t aow_sync_enabled;
    u_int32_t sync_buf_full;
    u_int32_t sync_buf_empty;
    u_int32_t tasklet_count;
    u_int32_t repeat_count;
} i2s_stats_t;    

extern void ieee80211_aow_i2s_stats(struct ieee80211com* ic);
extern void ieee80211_aow_ec_stats(struct ieee80211com* ic);
extern void ieee80211_aow_clear_i2s_stats(struct ieee80211com* ic);
extern void i2s_get_stats(struct i2s_stats *p);
extern void i2s_clear_stats(void);

#define IEEE80211_ENAB_AOW(ic)  (1)

/* Error Recovery related */
#define AOW_ER_ENAB(ic)     (ic->ic_get_aow_er(ic))
#define AOW_EC_ENAB(ic)     (ic->ic_get_aow_ec(ic))
#define AOW_ER_DATA_VALID   0x1
#define AOW_ER_MAX_RETRIES  3

#define AOW_SUB_FRAME_IDX_0 0
#define AOW_SUB_FRAME_IDX_1 1
#define AOW_SUB_FRAME_IDX_2 2
#define AOW_SUB_FRAME_IDX_3 3

#define AOW_ER_DATA_OK  (1)
#define AOW_ER_DATA_NOK (0)

enum { ER_IDLE, ER_IN, ER_OUT };

typedef struct aow_er {
    int cstate;             /* current er state */
    int expected_seqno;     /* next expected sequence number */
    int flag;               /* control flags */
    int datalen;            /* data length */
    int count;              /* error packet, occurance count */
    
    /* stats */
    int bad_fcs_count;      /* indicates number of bad fcs frame received */
    int recovered_frame_count;  /* indicates number of recovered frames */
    int aow_er_crc_valid;       /* valid aow crc, in bad fcs frame */
    int aow_er_crc_invalid;     /* invalid aow crc, in bad fcs frame */

    /* lock related */
    spinlock_t  lock;     /* AoW ER lock */
    unsigned long lock_flags;

    u_int8_t data[ATH_AOW_NUM_SAMP_PER_AMPDU];    /* pointer to data */
}aow_er_t;    

typedef struct aow_ec {

    u_int32_t index0_bad;
    u_int32_t index1_bad;
    u_int32_t index2_bad;
    u_int32_t index3_bad;

    u_int32_t index0_fixed;
    u_int32_t index1_fixed;
    u_int32_t index2_fixed;
    u_int32_t index3_fixed;

    /* place holder */
    u_int16_t prev_data[ATH_AOW_NUM_SAMP_PER_MPDU];
    u_int16_t predicted_data[ATH_AOW_NUM_SAMP_PER_MPDU];
}aow_ec_t;


typedef struct ieee80211_aow {

    /* stats related */
    u_int16_t gpio11Flg;
    u_int32_t ok_framecount;
    u_int32_t nok_framecount;  
    u_int32_t tx_framecount;
    u_int32_t macaddr_not_found;
    u_int32_t node_not_found;
    u_int32_t channel_set_flag; /* holds the set channel mask, for fast tx check */
    u_int32_t i2s_open_count;
    u_int32_t i2s_close_count;
    u_int32_t i2s_dma_start;

    /* control flags */
    u_int32_t i2sOpen;    

    /* bit 0 - START 
     * bit 1 - RESET
     * bit 2 - PAUSE
     * bit 3 - DMA START NEEDED
     */
    u_int32_t i2s_flags;
    u_int32_t i2s_dmastart;     /* This flag indicates the i2s DMA need to started */

    spinlock_t   lock;
    
    spinlock_t aow_lh_lock;          /* Lock control for Latency Histogram */
    spinlock_t aow_essc_lock;        /* Lock control for ESS Counts */
    spinlock_t aow_l2pe_lock;        /* Lock control for L2 Packet Error
                                        Measurement */
    spinlock_t aow_plr_lock;         /* Lock control for Packet Lost Rate
                                        Measurement */
    
    audio_info info[AUDIO_INFO_BUF_SIZE];
    audio_ctrl ctrl;

    u_int16_t *audio_data[AUDIO_INFO_BUF_SIZE+1];

    audio_stats stats;      /* AoW Stats */

    ext_stats estats;       /* AoW Extended Stats */

    /* Extended statistics synchronization - (remaining) no. of audio frames
       for which synchronized logging is to be carried out. */
    u_int32_t ess_count[AOW_MAX_AUDIO_CHANNELS];

    /* Node type, whether transmitter or receiver. */
    u_int16_t node_type;

    /* Netlink related data structure. */
    void  *aow_nl_sock;

    /* Feature Flags */
    u_int16_t   interleave;     /* Interleave/Deinterleave feature */
    aow_er_t    er;                 /* Error Recovery */
    aow_ec_t    ec;                 /* Error Concealment */
    
}ieee80211_aow_t;


#define SET_I2S_OPEN_STATE(_ic, val)    ((_ic)->ic_aow.i2sOpen = val)
#define IS_I2S_OPEN(_ic)                (((_ic)->ic_aow.i2sOpen)?1:0)
#define IS_AOW_AUDIO_STOPPED(_ic)       ((_ic)->ic_aow.ctrl.audStopped)
#define IS_AOW_AUDIO_CAPTURE_DONE(_ic)  ((_ic)->ic_aow.ctrl.capture_done)
#define IS_AOW_FORCE_INPUT(_ic)         ((_ic)->ic_aow.ctrl.force_input)
#define IS_AOW_CAPTURE_DATA(_ic)        ((_ic)->ic_aow.ctrl.capture_data)
#define AOW_INTERLEAVE_ENAB(_ic)        (((_ic)->ic_aow.interleave)?1:0)

#define AOW_LOCK_INIT(_ic)      spin_lock_init(&(_ic)->ic_aow.lock)
#define AOW_LOCK(_ic)           spin_lock(&(_ic)->ic_aow.lock)    
#define AOW_UNLOCK(_ic)         spin_unlock(&(_ic)->ic_aow.lock)
#define AOW_LOCK_DESTROY(_ic)   spin_lock_destroy(&(_ic)->ic_aow.lock)

#define AOW_LH_LOCK_INIT(_ic)             spin_lock_init(\
                                            &(_ic)->ic_aow.aow_lh_lock)
#define AOW_LH_LOCK(_ic)                  spin_lock(\
                                            &(_ic)->ic_aow.aow_lh_lock)
#define AOW_LH_UNLOCK(_ic)                spin_unlock(\
                                            &(_ic)->ic_aow.aow_lh_lock)
#define AOW_LH_LOCK_DESTROY(_ic)          spin_lock_destroy(\
                                            &(_ic)->ic_aow.aow_lh_lock)

#define AOW_ESSC_LOCK_INIT(_ic)           spin_lock_init(\
                                                &(_ic)->ic_aow.aow_essc_lock)
#define AOW_ESSC_LOCK(_ic)                spin_lock(\
                                                &(_ic)->ic_aow.aow_essc_lock)
#define AOW_ESSC_UNLOCK(_ic)              spin_unlock(\
                                                &(_ic)->ic_aow.aow_essc_lock)
#define AOW_ESSC_LOCK_DESTROY(_ic)        spin_lock_destroy(\
                                                &(_ic)->ic_aow.aow_essc_lock)

#define AOW_L2PE_LOCK_INIT(_ic)           spin_lock_init(\
                                                &(_ic)->ic_aow.aow_l2pe_lock)
#define AOW_L2PE_LOCK(_ic)                spin_lock(\
                                                &(_ic)->ic_aow.aow_l2pe_lock)
#define AOW_L2PE_UNLOCK(_ic)              spin_unlock(\
                                                &(_ic)->ic_aow.aow_l2pe_lock)
#define AOW_L2PE_LOCK_DESTROY(_ic)        spin_lock_destroy(\
                                                &(_ic)->ic_aow.aow_l2pe_lock)

#define AOW_PLR_LOCK_INIT(_ic)            spin_lock_init(\
                                                 &(_ic)->ic_aow.aow_plr_lock)
#define AOW_PLR_LOCK(_ic)                 spin_lock(\
                                                 &(_ic)->ic_aow.aow_plr_lock)
#define AOW_PLR_UNLOCK(_ic)               spin_unlock(\
                                                 &(_ic)->ic_aow.aow_plr_lock)
#define AOW_PLR_LOCK_DESTROY(_ic)         spin_lock_destroy(\
                                                &(_ic)->ic_aow.aow_plr_lock)


#define AOW_ER_SYNC_LOCK_INIT(_ic)      spin_lock_init(&(_ic)->ic_aow.er.lock)
#define AOW_ER_SYNC_LOCK_DESTORY(_ic)   spin_lock_destroy(&(_ic)->ic_aow.er.lock)
#define AOW_ER_SYNC_IRQ_LOCK(_ic)       spin_lock_irqsave(&(_ic)->ic_aow.er.lock, (_ic)->ic_aow.er.lock_flags)
#define AOW_ER_SYNC_IRQ_UNLOCK(_ic)     spin_unlock_irqrestore(&(_ic)->ic_aow.er.lock, (_ic)->ic_aow.er.lock_flags)
#define AOW_ER_SYNC_LOCK(_ic)           spin_lock(&(_ic)->ic_aow.er.lock)
#define AOW_ER_SYNC_UNLOCK(_ic)         spin_unlock(&(_ic)->ic_aow.er.lock)


#define AOW_EC_PKT_INDEX_0  0
#define AOW_EC_PKT_INDEX_1  1
#define AOW_EC_PKT_INDEX_2  2
#define AOW_EC_PKT_INDEX_3  3
#define AOW_EC_PKT_INDEX_MASK   0x3
#define AOW_EC_SUB_FRAME_MASK   0xf

#define AOW_INDEX_0 0
#define AOW_INDEX_1 1
#define AOW_INDEX_2 2
#define AOW_INDEX_3 3


extern u_int32_t chksum_crc32 (u_int8_t *block, u_int32_t length);
extern u_int32_t cumultv_chksum_crc32(u_int32_t crc_prev,
                                      u_int8_t *block,
                                      u_int32_t length);
extern void chksum_crc32gentab (void);
extern int aow_er_handler(struct ieee80211vap *vap, wbuf_t wbuf, struct ieee80211_rx_status *rs);
extern int aow_ec_handler(struct ieee80211com *ic, audio_info *info);

/* Extended stats related */
#define AOW_ES_ENAB(ic)         (ic->ic_get_aow_es(ic))
#define AOW_ESS_ENAB(ic)        (ic->ic_get_aow_ess(ic))
#define AOW_ESS_SYNC_SET(param) (((param) >> ATH_AOW_PARAMS_LOGSYNC_FLG_S)\
                                  & ATH_AOW_PARAMS_LOGSYNC_FLG_MASK)

/* Size of each latency range in microseconds */
#define ATH_AOW_LATENCY_RANGE_SIZE  (200)

/* Maximum value of latency in milliseconds which will be covered
   by the Latency Histogram */
#define ATH_AOW_LH_MAX_LATENCY      (300U)

/* Maximum value of latency in microseconds which will be covered
   by the Latency Histogram.
   Make sure this can never exceed max value representable by 
   unsigned int */
#define ATH_AOW_LH_MAX_LATENCY_US   (ATH_AOW_LH_MAX_LATENCY * 1000U)

/* Max number of ES/ESS Statistics groups to be maintained.
 * These groups are inserted into a fixed size FIFO */
#define ATH_AOW_MAX_ES_GROUPS       (8)

/* Time interval for which each ES/ESS Statistics group is
   measured (in seconds). */
#define ATH_AOW_ES_GROUP_INTERVAL   (10)

/* Time interval for which each ES/ESS Statistics group is
   measured (in milliseconds). */
#define ATH_AOW_ES_GROUP_INTERVAL_MS  (ATH_AOW_ES_GROUP_INTERVAL *\
                                        NUM_MILLISEC_PER_SEC)

/* Number of digits after the decimal point, required for the ratios
   reported for extended stats. */
#define ATH_AOW_ES_PRECISION        (4)  

/* 10 to the power ATH_AOW_ES_PRECISION. Used for determining fractional
   component of ratio */
#define ATH_AOW_ES_PRECISION_MULT   (10000)  

#define ATH_AOW_PLR_TYPE_NUM_MPDUS  (1)
#define ATH_AOW_PLR_TYPE_NOK_FRMCNT (2)
#define ATH_AOW_PLR_TYPE_LATE_MPDU  (3)


extern int aow_es_base_init(struct ieee80211com *ic);
extern void aow_es_base_deinit(struct ieee80211com *ic);

extern void aow_clear_ext_stats(struct ieee80211com *ic);
extern void aow_print_ext_stats(struct ieee80211com * ic);

extern int aow_lh_init(struct ieee80211com *ic);
extern int aow_lh_reset(struct ieee80211com *ic);
extern int aow_lh_add(struct ieee80211com *ic, const u_int64_t latency);
extern int aow_lh_add_expired(struct ieee80211com *ic);
extern int aow_lh_print(struct ieee80211com *ic);
extern void aow_lh_deinit(struct ieee80211com *ic);

extern void aow_update_mcsinfo(struct ieee80211com *ic,
                               struct ath_buf *bf,
                               struct ath_tx_status *ts,
                               bool is_success,
                               int *num_attempts);
extern void aow_update_txstatus(struct ieee80211com *ic,
                                struct ath_buf *bf,
                                struct ath_tx_status *ts);

extern void aow_get_node_type(struct ieee80211com *ic, u_int16_t *node_type);

#if LINUX_VERSION_CODE == AOW_LINUX_VERSION
/* Netlink related declarations */
extern int aow_am_init(struct ieee80211com *ic);
extern void aow_am_deinit(struct ieee80211com *ic);
extern int aow_am_send(struct ieee80211com *ic, char *data, int size);
#else
#define aow_am_init(a)          -1
#define aow_am_deinit(b)        do{}while(0)
#define aow_am_send(a,b,c)      do{}while(0)
#endif /* LINUX_VERSION_CODE == AOW_LINUX_VERSION */

extern int aow_nl_send_rxpl(struct ieee80211com *ic,
                            u_int32_t seqno,
                            u_int8_t subfrme_seqno,
                            u_int16_t wlan_seqno,
#if ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG
                            u_int32_t latency,
#endif
                            u_int8_t rxstatus);

extern int aow_nl_send_txpl(struct ieee80211com *ic,
                            struct ether_addr recvr_addr,
                            u_int32_t seqno,
                            u_int8_t subfrme_seqno,
                            u_int16_t wlan_seqno,
                            u_int8_t txstatus,
#if ATH_SUPPORT_AOW_ES_ADVNCD_TXDEBUG
                            struct aow_advncd_txinfo *atxinfo,
#endif
                            u_int8_t numretries);

extern void aow_essc_init(struct ieee80211com *ic);
extern void aow_essc_setall(struct ieee80211com *ic, u_int32_t val);
extern bool aow_essc_testdecr(struct ieee80211com *ic, int channel);
extern void aow_essc_deinit(struct ieee80211com *ic);

extern int aow_l2pe_init(struct ieee80211com *ic);
extern void aow_l2pe_reset(struct ieee80211com *ic);
extern void aow_l2pe_deinit(struct ieee80211com *ic);
extern int aow_l2pe_record(struct ieee80211com *ic, bool is_success);
extern void aow_l2pe_audstopped(struct ieee80211com *ic);
extern int aow_l2pe_print(struct ieee80211com *ic);

extern int aow_plr_init(struct ieee80211com *ic);
extern void aow_plr_reset(struct ieee80211com *ic);
extern void aow_plr_deinit(struct ieee80211com *ic);
extern int aow_plr_record(struct ieee80211com *ic,
                          u_int32_t seqno,
                          u_int16_t subseqno,
                          u_int8_t type);
extern void aow_plr_audstopped(struct ieee80211com *ic);
extern int aow_plr_print(struct ieee80211com *ic);
extern void aow_mcss_print(struct ieee80211com *ic);


/* few of the external functions, that the AoW module needs */

extern void ieee80211_send_setup( struct ieee80211vap *vap, 
                           struct ieee80211_node *ni,
                           struct ieee80211_frame *wh, 
                           u_int8_t type, 
                           const u_int8_t *sa, 
                           const u_int8_t *da,
                           const u_int8_t *bssid);

//extern struct sk_buff* ieee80211_get_aow_frame(u_int8_t **frm, u_int32_t pktlen);
extern int ath_tx_send(wbuf_t wbuf);

#else   /* ATH_SUPPORT_AOW */

#define IEEE80211_ENAB_AOW(ic)  (0)
#define AOW_ES_ENAB(ic)         (0)
#define AOW_ESS_ENAB(ic)        (0)
#define AOW_ESS_SYNC_SET(param) (0)

#define ieee80211_aow_attach(a)             do{}while(0)
#define ieee80211_audio_receive(a,b,c)      do{}while(0)
#define ieee80211_aow_detach(a)             do{}while(0)
#define IEEE80211_AOW_DPRINTF(...)

#endif  /* ATH_SUPPORT_AOW */
#endif  /* IEEE80211_AOW_H */
