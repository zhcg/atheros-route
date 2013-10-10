/*
 *  Copyright (c) 2008 Atheros Communications Inc.  All rights reserved.
 */

/*
 * IEEE80211 AoW Handler.
 */

#include <ieee80211_var.h>
#include <ieee80211_aow.h>
#include <osdep.h>
#include "if_athvar.h"
#include "ah.h"

#if  ATH_SUPPORT_AOW



#if ATH_SUPPORT_AOW_DEBUG

#define AOW_MAX_TSF_COUNT 256
#define AOW_DBG_PRINT_LIMIT 300

typedef struct aow_dbg_stats {
    u_int64_t tsf[AOW_MAX_TSF_COUNT];
    u_int64_t sum;
    u_int64_t avg;
    u_int64_t min;
    u_int64_t max;
    u_int64_t pretsf;
    u_int64_t ref_p_tsf;
    u_int32_t prnt_count;
    u_int32_t index;
    u_int32_t wait;
} aow_dbg_stats_t;

struct aow_dbg_stats dbgstats;

static int aow_dbg_init(struct aow_dbg_stats* p);
static int aow_update_dbg_stats(struct aow_dbg_stats* p, u_int64_t val);

#endif  /* ATH_SUPPORT_AOW_DEBUG */

/* static function declarations */
static void aow_update_ec_stats(struct ieee80211com* ic, int fmap);

static wbuf_t 
ieee80211_get_aow_frame(struct ieee80211_node* ni, 
                        u_int8_t **frm, 
                        u_int32_t pktlen);

#if ATH_SUPPORT_AOW_ES_ADVNCD_TXDEBUG
static void aow_populate_advncd_txinfo(struct aow_advncd_txinfo *atxinfo,
                                       struct ath_tx_status *ts,
                                       struct ath_buf *bf,
                                       int loopcount);
#endif

static bool aow_istxsuccess(struct ieee80211com *ic,
                            struct ath_buf *bf,
                            struct ath_tx_status *ts,
                            bool is_aggr);

static inline int aow_l2pe_print_inst(struct ieee80211com *ic,
                                      struct l2_pkt_err_stats *l2pe_inst);

static int aow_l2pe_fifo_init(struct ieee80211com *ic);
static int aow_l2pe_fifo_reset(struct ieee80211com *ic);
static int aow_l2pe_fifo_enqueue(struct ieee80211com *ic,
                                 struct l2_pkt_err_stats *l2pe_instnce);
static int aow_l2pe_fifo_copy(struct ieee80211com *ic,
                              struct l2_pkt_err_stats *aow_l2pe_stats,
                              int *count);
static void aow_l2pe_fifo_deinit(struct ieee80211com *ic);

static inline void aow_l2peinst_init(struct l2_pkt_err_stats *l2pe_inst,
                                     u_int32_t srNo,
                                     systime_t start_time);

static inline int aow_plr_print_inst(struct ieee80211com *ic,
                                     struct pkt_lost_rate_stats *plr_inst);
static int aow_plr_fifo_init(struct ieee80211com *ic);
static int aow_plr_fifo_reset(struct ieee80211com *ic);
static int aow_plr_fifo_enqueue(struct ieee80211com *ic,
                                struct pkt_lost_rate_stats *plr_instnce);
static int aow_plr_fifo_copy(struct ieee80211com *ic,
                             struct pkt_lost_rate_stats *aow_plr_stats,
                             int *count);
static void aow_plr_fifo_deinit(struct ieee80211com *ic);
static inline void aow_plrinst_init(struct pkt_lost_rate_stats *plr_inst,
                                    u_int32_t srNo,
                                    u_int32_t seqno,
                                    u_int16_t subseqno,
                                    systime_t start_time);

static inline int uint_floatpt_division(const u_int32_t dividend,
                                        const u_int32_t divisor,
                                        u_int32_t *integer,
                                        u_int32_t *fraction,
                                        const u_int32_t precision_mult);

static inline long aow_compute_nummpdus(u_int32_t start_seqno,
                                        u_int8_t start_subseqno,
                                        u_int32_t end_seqno,
                                        u_int8_t end_subseqno);


/* place holder for ic and sc pointers */
typedef struct aow_info {
    struct ieee80211com* ic;
    struct ath_softc* sc;
}aow_info_t;

/* global aowinfo */
aow_info_t aowinfo;

/******************** ERROR RECOVERY FEATURE RELATED *************************/

/* crc_tab[] -- this crcTable is being build by chksum_crc32GenTab().
 *		so make sure, you call it before using the other
 *		functions!
 */
u_int32_t crc_tab[256];

/* fuction prototypes */

/* chksum_crc() -- to a given block, this one calculates the
 *				crc32-checksum until the length is
 *				reached. the crc32-checksum will be
 *				the result.
 */
u_int32_t chksum_crc32 (u_int8_t *block, u_int32_t length)
{
   register unsigned long crc;
   unsigned long i;

   crc = 0xFFFFFFFF;
   for (i = 0; i < length; i++)
   {
      crc = ((crc >> 8) & 0x00FFFFFF) ^ crc_tab[(crc ^ *block++) & 0xFF];
   }
   return (crc ^ 0xFFFFFFFF);
}

/* cumultv_chksum_crc() -- Continues checksum from a previous operation of
 * chksum_crc32()
 */
u_int32_t cumultv_chksum_crc32(u_int32_t crc_prev,
                               u_int8_t *block,
                               u_int32_t length)
{
   register unsigned long crc;
   unsigned long i;

   crc = crc_prev ^ 0xFFFFFFFF;
   for (i = 0; i < length; i++)
   {
      crc = ((crc >> 8) & 0x00FFFFFF) ^ crc_tab[(crc ^ *block++) & 0xFF];
   }
   return (crc ^ 0xFFFFFFFF);
}

/* chksum_crc32gentab() --      to a global crc_tab[256], this one will
 *				calculate the crcTable for crc32-checksums.
 *				it is generated to the polynom [..]
 */

void chksum_crc32gentab (void)
{
   unsigned long crc, poly;
   int i, j;

   poly = 0xEDB88320L;
   for (i = 0; i < 256; i++)
   {
      crc = i;
      for (j = 8; j > 0; j--)
      {
         if (crc & 1)
         {
            crc = (crc >> 1) ^ poly;
         }
         else
         {
            crc >>= 1;
         }
      }
      crc_tab[i] = crc;
   }
}


/******************** ERROR RECOVERY FEATURE END  *****************************/


/*
 * function : aow_i2s_init
 * --------------------------
 * Initializes the i2s device
 */

int aow_i2s_init(struct ieee80211com* ic)
{
    int ret = FALSE;

    if (!IS_I2S_OPEN(ic)) {
        if (!ar7242_i2s_open()) {
            ar7240_i2s_clk(63565868, 9091);
            SET_I2S_OPEN_STATE(ic, TRUE);
            CLR_I2S_STOP_FLAG(ic->ic_aow.i2s_flags);
            CLR_I2S_START_FLAG(ic->ic_aow.i2s_flags);
            CLR_I2S_PAUSE_FLAG(ic->ic_aow.i2s_flags);
            SET_I2S_DMA_START(ic->ic_aow.i2s_flags);
            ic->ic_aow.i2s_open_count++;
            ret = TRUE;
        }
    }
    return ret;        
}    

/*
 * function : aow_i2s_deinit
 * -------------------------
 * Resets the I2S device 
 */

int aow_i2s_deinit(struct ieee80211com* ic)
{
    
    if (IS_I2S_OPEN(ic)) {
        
        ar7242_i2s_desc_busy(0);
        if (!ar7242_i2s_close()) {
            IEEE80211_AOW_DPRINTF("I2S device close failed\n");
        }            
        CLR_I2S_STOP_FLAG(ic->ic_aow.i2s_flags);
        CLR_I2S_START_FLAG(ic->ic_aow.i2s_flags);
        CLR_I2S_DMA_START(ic->ic_aow.i2s_flags);
        SET_I2S_OPEN_STATE(ic, FALSE);
        ic->ic_aow.i2s_close_count++;
    }        

    return TRUE;
}

/* All the times below are in micro seconds */
#define AOW_PKT_RX_PROC_THRESHOLD   (8500)
#define AOW_PKT_CONSUME_THRESHOLD   (AOW_PKT_RX_PROC_THRESHOLD - 500)
#define AOW_PKT_I2S_WRITE_THRESHOLD (AOW_PKT_CONSUME_THRESHOLD - 6000)


int ieee80211_process_audio_data(struct ieee80211vap *vap,  
                                 struct audio_pkt *apkt, 
                                 u_int16_t rxseq,
                                 u_int64_t curTime, 
                                 char *data)
{
    struct ieee80211com *ic = vap->iv_ic;
    u_int64_t aow_latency   = ic->ic_get_aow_latency_us(ic);    /* get latency */
    audio_info *cur_info    = &ic->ic_aow.info[apkt->seqno & AUDIO_INFO_BUF_SIZE_MASK];
    audio_stats *cur_stats  = &ic->ic_aow.stats;
    static u_int8_t simcount = 0;

    /* This is an Rx, set the Rx pointer to 1 */
    cur_stats->rxFlg = 1;

    AOW_LOCK(ic);

    /* 
     * Check if sequence number is already stored
     * Check if the packet came on time 
     */

    curTime = ic->ic_get_aow_tsf_64(ic);

    if (AOW_ES_ENAB(ic) ||
        (AOW_ESS_ENAB(ic) &&  AOW_ESS_SYNC_SET(apkt->params))) {
        aow_lh_add(ic, curTime - apkt->timestamp);
    } 

    if ((curTime - apkt->timestamp) < (aow_latency - AOW_PKT_RX_PROC_THRESHOLD) ) {

        if (cur_info->inUse ) {

            /* Already in use, check if the sequency number matches */
            if (cur_info->seqNo == apkt->seqno ) {

#if  AOW_NO_EC_SIMULATE 
                /* add the information */
                cur_info->foundBlocks |= 1<<(apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK);

                /* Copy the data */
                memcpy(cur_info->audBuffer[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK], 
                       data, sizeof( u_int16_t) * (apkt->pkt_len >> ATH_AOW_NUM_MPDU_LOG2));
                cur_info->WLANSeqNos[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK] = rxseq;
                cur_info->logSync[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK] = 
                        AOW_ESS_SYNC_SET(apkt->params);
#if ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG
                cur_info->latency[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK] =
                        curTime - apkt->timestamp;
#endif /* ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG */
#else

                {
                    int fmap = ic->ic_get_aow_ec_fmap(ic);

                    if ((fmap & (1 << (apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK)))) {
                        if (cur_info->localSimCount == (AOW_EC_ERROR_SIMULATION_FREQUENCY - 1)) {

                            /* Skip the packets */
                            /* update the stats */
                            aow_update_ec_stats(ic, fmap);

                        } else {
                          cur_info->foundBlocks |= 1<<(apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK);
                          memcpy(cur_info->audBuffer[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK], 
                                 data, sizeof( u_int16_t) * (apkt->pkt_len >> ATH_AOW_NUM_MPDU_LOG2));
                          cur_info->WLANSeqNos[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK] = rxseq;
                          cur_info->logSync[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK] = 
                                 AOW_ESS_SYNC_SET(apkt->params);
#if ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG
                          cur_info->latency[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK] =
                                 curTime - apkt->timestamp;
#endif /* ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG */

                        }
                        
                    } else {
                        cur_info->foundBlocks |= 1<<(apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK);
                        memcpy(cur_info->audBuffer[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK], 
                               data, sizeof( u_int16_t) * (apkt->pkt_len >> ATH_AOW_NUM_MPDU_LOG2));
                        cur_info->WLANSeqNos[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK] = rxseq;
                        cur_info->logSync[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK] = 
                                 AOW_ESS_SYNC_SET(apkt->params);
#if ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG
                        cur_info->latency[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK] =
                                 curTime - apkt->timestamp;
#endif /* ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG */

                    }

                }
#endif  /* AOW_NO_EC_SIMULATE */

            } else {

                /* The packet has not been cleared on time. Log this */
                cur_stats->mpdu_not_consumed++;
            }

        } else {

            cur_info->inUse = 1;
            cur_info->startTime = apkt->timestamp;
            cur_info->seqNo = apkt->seqno;
            cur_info->params = apkt->params;
            cur_info->datalen = apkt->pkt_len; 
            cur_info->localSimCount = simcount;
            simcount = (simcount + 1) % AOW_EC_ERROR_SIMULATION_FREQUENCY;  

#if  AOW_NO_EC_SIMULATE 
            /* add the information */
            cur_info->foundBlocks |= 1<<(apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK);

            /* Copy the data */
            memcpy(cur_info->audBuffer[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK], 
                   data, sizeof( u_int16_t) * (apkt->pkt_len >> ATH_AOW_NUM_MPDU_LOG2));
            cur_info->WLANSeqNos[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK] = rxseq;
            cur_info->logSync[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK] = 
                   AOW_ESS_SYNC_SET(apkt->params);
#if ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG
            cur_info->latency[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK] =
                   curTime - apkt->timestamp;
#endif /* ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG */
                
#else
            {
                int fmap = ic->ic_get_aow_ec_fmap(ic);

                if ((fmap & (1 << (apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK)))) {
                    if (cur_info->localSimCount == (AOW_EC_ERROR_SIMULATION_FREQUENCY - 1)) {

                        /* Skip the packets */
                        /* update the stats */
                        aow_update_ec_stats(ic, fmap); 

                    } else {
                      cur_info->foundBlocks |= 1<<(apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK);
                      memcpy(cur_info->audBuffer[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK], 
                             data, sizeof( u_int16_t) * (apkt->pkt_len >> ATH_AOW_NUM_MPDU_LOG2));
                      cur_info->WLANSeqNos[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK] = rxseq;
                      cur_info->logSync[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK] = 
                             AOW_ESS_SYNC_SET(apkt->params);
#if ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG
                      cur_info->latency[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK] =
                             curTime - apkt->timestamp;
#endif /* ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG */
                    }
                    
                } else {
                    cur_info->foundBlocks |= 1<<(apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK);
                    memcpy(cur_info->audBuffer[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK], 
                           data, sizeof( u_int16_t) * (apkt->pkt_len >> ATH_AOW_NUM_MPDU_LOG2));
                    cur_info->WLANSeqNos[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK] = rxseq;
                    cur_info->logSync[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK] = 
                           AOW_ESS_SYNC_SET(apkt->params);
#if ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG
                    cur_info->latency[apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK] =
                           curTime - apkt->timestamp;
#endif /* ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG */
                }
            }
#endif  /* AOW_NO_EC_SIMULATE */
        } 
    } else {
        cur_stats->late_mpdu++;

        if (AOW_ES_ENAB(ic)
            || (AOW_ESS_ENAB(ic) &&  AOW_ESS_SYNC_SET(apkt->params))) {
            aow_nl_send_rxpl(ic,
                             apkt->seqno,
                             apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK,
                             rxseq,
#if ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG
                             curTime - apkt->timestamp,                            
#endif /* ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG */
                             ATH_AOW_PL_INFO_DELAYED);

            aow_plr_record(ic,
                           apkt->seqno,
                           apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK,
                           ATH_AOW_PLR_TYPE_LATE_MPDU);
        }                         
    }

    AOW_UNLOCK(ic);
    return 0;
}

       
#define __bswap16(_x) ( (u_int16_t)( (((const u_int8_t *)(&_x))[0] ) |\
                         ( ( (const u_int8_t *)( &_x ) )[1]<< 8) ) )

int ieee80211_consume_audio_data(struct ieee80211com *ic, u_int64_t curTime )
{
    u_int16_t cnt, fcnt, miss_cnt=0, bufCnt;
    u_int64_t maxTime = 0xffffffffffffffffULL; /* Some large number */
    u_int16_t maxTimeIdx = 0;
    u_int16_t fndPktToPly  = 0;
    audio_info *cur_info;
    audio_stats *cur_stats = &ic->ic_aow.stats;
    u_int64_t aow_latency = ic->ic_get_aow_latency_us(ic);    /* get latency */

    /* Restart the timer */
    if (ic->ic_aow.ctrl.audStopped ) {

        ic->ic_aow_proc_timer_stop(ic);
        ieee80211_aow_cmd_handler(ic, ATH_AOW_RESET_COMMAND );
        i2s_audio_data_stop();
        return 0;    

    } else {

        /* Audio has stopped, just return */
        ic->ic_aow_proc_timer_start(ic);

    }

    AOW_LOCK(ic);
    cur_stats->consume_aud_cnt++;
    curTime = ic->ic_get_aow_tsf_64(ic);                        

    /* loop through and figure out which is the earliest buffer */
    for ( cnt = 0; cnt < AUDIO_INFO_BUF_SIZE; cnt++ ) {

        cur_info = &ic->ic_aow.info[cnt];

        if (cur_info->inUse ) {
            /* If used, find it it is one with the oldest time stamp */
            if (cur_info->startTime <  maxTime ) {
               maxTime = cur_info->startTime;
               maxTimeIdx = cnt;
            }
        }
    }


    /* loop through and figure out which is about to play */
    for ( bufCnt = 0; bufCnt < AUDIO_INFO_BUF_SIZE ; bufCnt++ ) {

        cnt = maxTimeIdx;
        maxTimeIdx++;
        maxTimeIdx &= AUDIO_INFO_BUF_SIZE_MASK;
        cur_info = &ic->ic_aow.info[cnt];
        curTime = ic->ic_get_aow_tsf_64(ic);                        

        if (cur_info->inUse ) {
            cur_stats->lastPktTime = cur_info->startTime;
            cur_stats->numPktMissing = 0;
            fndPktToPly = 1;               
            miss_cnt=0;

            if ((curTime - cur_info->startTime) > (aow_latency - AOW_PKT_CONSUME_THRESHOLD)) {
                /* Time to clear this buffer and send it to I2S */
                /* received at least on MPDU, mark it as such */
                cur_stats->num_ampdus++;

                for( fcnt = 0; fcnt < ATH_AOW_NUM_MPDU; fcnt++ ) {
                    if (!(cur_info->foundBlocks & (1<<fcnt))) {   
                        miss_cnt++;
                        cur_stats->missing_pos_cnt[fcnt]++;
                    } else {
                        cur_stats->num_mpdus++;
                        if (AOW_ES_ENAB(ic) ||
                                (AOW_ESS_ENAB(ic) && cur_info->logSync[fcnt])) {
                            aow_nl_send_rxpl(ic,
                                             cur_info->seqNo,
                                             fcnt,
                                             cur_info->WLANSeqNos[fcnt],
#if ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG
                                             cur_info->latency[fcnt],                            
#endif /* ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG */
                                             ATH_AOW_PL_INFO_INTIME);
                            
                            aow_plr_record(ic,
                                           cur_info->seqNo,
                                           fcnt,
                                           ATH_AOW_PLR_TYPE_NUM_MPDUS);
                        }                         
                    }
                }
                
                cur_stats->num_missing_mpdu_hist[miss_cnt]++;
                cur_stats->num_missing_mpdu += miss_cnt;
                cur_stats->datalenCnt++;

                if (cur_stats->dbgCnt < AOW_DBG_WND_LEN ) {
                    cur_stats->datalenLen[cur_stats->dbgCnt++] = cur_info->seqNo;    
                }

                /* TODO Send the buffer to the I2S */
                /* Check if the data is too late to be played, if so discard */

                if ((curTime - cur_info->startTime) > (aow_latency - AOW_PKT_I2S_WRITE_THRESHOLD) ) {
                    /* Delayed processing. Discard it */
                    cur_stats->too_late_ampdu++;
                } else {
                    u_int32_t samp_cnt;
                    u_int16_t *dataPtr = ic->ic_aow.audio_data[cnt];


                    /* pass through the error concealment feature */
                    if (AOW_EC_ENAB(ic)) {
                        aow_ec_handler(ic, cur_info);
                    }
                        
                    /* Some debug stuff. Check if all the sub-frames have been 
                     * received and the block size is 768 and the capture flag 
                     * is set 
                     */
                    if (ic->ic_aow.ctrl.capture_data && (cur_info->foundBlocks == 0xf) && 
                        (cur_info->datalen == ATH_AOW_NUM_SAMP_PER_AMPDU )) {

                        /* do check with ATH_AOW_NUM_SAMP_PER_AMPDU / 4 */
                        for( fcnt = 0; fcnt < ATH_AOW_NUM_SAMP_PER_MPDU; fcnt++ ) {
                            cur_stats->before_deint[0][fcnt] = cur_info->audBuffer[0][fcnt];       
                            cur_stats->before_deint[1][fcnt] = cur_info->audBuffer[1][fcnt];       
                            cur_stats->before_deint[2][fcnt] = cur_info->audBuffer[2][fcnt];       
                            cur_stats->before_deint[3][fcnt] = cur_info->audBuffer[3][fcnt];       
                        }
                    }

                    /* Check if deinterleave has to be done */
                    if ((cur_info->params >> ATH_AOW_PARAMS_INTRPOL_ON_FLG_S) & ATH_AOW_PARAMS_INTRPOL_ON_FLG_MASK ) {

                        /* Interleaving is on, deinterleave the packets */
                        for ( fcnt = 0; fcnt < cur_info->datalen; fcnt += 2 ) {
                            samp_cnt = fcnt >> 1; /* This is for the left and the right sample */
                            dataPtr[fcnt] = cur_info->audBuffer[samp_cnt & ATH_AOW_MPDU_MOD_MASK][ ((samp_cnt >> ATH_AOW_NUM_MPDU_LOG2) << 1)];
                            dataPtr[fcnt+1] = cur_info->audBuffer[samp_cnt & ATH_AOW_MPDU_MOD_MASK][ ((samp_cnt >> ATH_AOW_NUM_MPDU_LOG2) << 1) + 1];
                        }

                    } else { /* No interleaving case, just copy the buffers */
                        u_int32_t dataPerMpdu = cur_info->datalen >> 2;
                        for( fcnt = 0; fcnt < ATH_AOW_NUM_MPDU; fcnt++ ) {
                            memcpy(&dataPtr[dataPerMpdu*fcnt], cur_info->audBuffer[fcnt], 
                                    sizeof(u_int16_t)*(dataPerMpdu));
                        }
                    }

                    /* Some debug stuff. Check if all the sub-frames have been 
                     * received and the block size is 768 and the capture flag 
                     * is set 
                     */
                    if (ic->ic_aow.ctrl.capture_data && 
                        (cur_info->foundBlocks == 0xf) && 
                        (cur_info->datalen == ATH_AOW_NUM_SAMP_PER_AMPDU)) {

                        for( fcnt = 0; fcnt < ATH_AOW_NUM_SAMP_PER_AMPDU; fcnt++ ) {
                            cur_stats->after_deint[fcnt] = dataPtr[fcnt];
                        }
                        ic->ic_aow.ctrl.capture_data = 0;
                        ic->ic_aow.ctrl.capture_done = 1;
                    }

                    /* Write the data to I2S */
                    {
                        /* The length is in char, the maximum number being 768. So, split the buffer so that they are in sequence. Keep it as a 
                         * multiple of 4 so that both right and left stereo fall in the same band. Also, keep headroom to add samples in case of 
                         * clock synchronization
                         */
                        u_int16_t  pb_left = cur_info->datalen;
                        u_int16_t pb_size;
                        cur_stats->ampdu_size_short++;     

                        while (pb_left ) {
                            pb_size = ( pb_left > AOW_MAX_PLAYBACK_SIZE) ? 
                                    AOW_MAX_PLAYBACK_SIZE:pb_left;
                            if (IS_I2S_NEED_DMA_START(ic->ic_aow.i2s_flags)) {
                                ic->ic_aow.i2s_dmastart = TRUE;
                            }

                            if (!AOW_ES_ENAB(ic) && !AOW_ESS_ENAB(ic)) {
                                ar7242_i2s_write(pb_size * ATH_AOW_SAMPLE_SIZE, (char *)dataPtr);
                            }
                            pb_left -= pb_size;
                            dataPtr += pb_size; 
                        
                        }
                    }
                }

                /* Clear up the info */
                memset( cur_info, 0, sizeof( audio_info ));
                /* Found a packet. Record it as the current packet time and the expected next packet */
            }
        }
    }

    /* Check if we found something to play. If not, check if we have missed the last packet */
    if (!fndPktToPly ) {

        if (curTime > (cur_stats->lastPktTime + AOW_CHECK_FOR_DATA_THRESHOLD)) {

            /* Increment the lastPktTime */
            cur_stats->lastPktTime += AOW_LAST_PACKET_TIME_DELTA;
            cur_stats->numPktMissing++;

            if ( cur_stats->numPktMissing > AOW_MAX_MISSING_PKT ) {
                /* Stop the timer and I2S */    
                ic->ic_aow.ctrl.audStopped = 1;

                if (AOW_ES_ENAB(ic) || AOW_ESS_ENAB(ic)) {
                    aow_l2pe_audstopped(ic);
                    aow_plr_audstopped(ic);
                }

                /*reset the counter for the next time */
                cur_stats->numPktMissing = 0;
                IEEE80211_AOW_DPRINTF("Detected stop of packets\n");
            } else {

                /* Send dummy packets, till the audio stop is detected */
                u_int16_t  pb_left = ATH_AOW_NUM_SAMP_PER_AMPDU;
                u_int16_t pb_size;
                u_int16_t *dataPtr = ic->ic_aow.audio_data[AOW_ZERO_BUF_IDX];
                while (pb_left ) {
                    pb_size = ( pb_left > AOW_MAX_PLAYBACK_SIZE) ? 
                            AOW_MAX_PLAYBACK_SIZE:pb_left;
                    if (!AOW_ES_ENAB(ic) && !AOW_ESS_ENAB(ic)) {
                        ar7242_i2s_write(pb_size * ATH_AOW_SAMPLE_SIZE, (char *)dataPtr);
                    }
                    pb_left -= pb_size;
                    dataPtr += pb_size; 
                }                    
            }
        }
    }
    AOW_UNLOCK(ic);
    return 0;
}

void ieee80211_set_audio_data_capture(struct ieee80211com *ic)
{
    ic->ic_aow.ctrl.capture_data = 1;
    ic->ic_aow.stats.dbgCnt = 0;
    ic->ic_aow.stats.prevTsf = 0;        
}

void ieee80211_set_force_aow_data(struct ieee80211com *ic, int32_t flg)
{
    ic->ic_aow.ctrl.force_input = !!flg;        
}


void ieee80211_audio_print_capture_data(struct ieee80211com *ic)
{
    audio_stats *cur_stats = &ic->ic_aow.stats;
    u_int32_t cnt,sub_frm_cnt;

    AOW_LOCK(ic);

    if (ic->ic_aow.ctrl.capture_done ) {
        if ( cur_stats->rxFlg ) {
            IEEE80211_AOW_DPRINTF("Before De-Interleave\n");
            for( sub_frm_cnt = 0; sub_frm_cnt < ATH_AOW_NUM_MPDU; sub_frm_cnt++ ) {
                IEEE80211_AOW_DPRINTF("\n***Subframe %d data\n", sub_frm_cnt);
                for ( cnt = 0; cnt < ATH_AOW_NUM_SAMP_PER_MPDU/8; cnt++ ) {
                    IEEE80211_AOW_DPRINTF("%5d   %5d        %5d   %4d        %5d   %5d        %5d   %5d\n",
                        cur_stats->before_deint[sub_frm_cnt][cnt*8],                
                        cur_stats->before_deint[sub_frm_cnt][cnt*8+1],                
                        cur_stats->before_deint[sub_frm_cnt][cnt*8+2],                
                        cur_stats->before_deint[sub_frm_cnt][cnt*8+3],                
                        cur_stats->before_deint[sub_frm_cnt][cnt*8+4],                
                        cur_stats->before_deint[sub_frm_cnt][cnt*8+5],                
                        cur_stats->before_deint[sub_frm_cnt][cnt*8+6],                
                        cur_stats->before_deint[sub_frm_cnt][cnt*8+7]);                
                }
            }
            IEEE80211_AOW_DPRINTF("\n\n***After de-interleave, samples 0(L/R) 1(L/R) 2(L/R) 3(L/R)\n                             4(L/R) 5(L/R) 6(L/R) 7(L/R)....\n");
            for ( cnt = 0; cnt < ATH_AOW_NUM_SAMP_PER_AMPDU/8; cnt++ ) {
                IEEE80211_AOW_DPRINTF(" %5d    %5d     %5d    %5d     %5d    %5d     %5d    %5d\n",
                        cur_stats->after_deint[cnt*8],
                        cur_stats->after_deint[cnt*8+1],
                        cur_stats->after_deint[cnt*8+2],
                        cur_stats->after_deint[cnt*8+3],
                        cur_stats->after_deint[cnt*8+4],
                        cur_stats->after_deint[cnt*8+5],
                        cur_stats->after_deint[cnt*8+6],
                        cur_stats->after_deint[cnt*8+7]);
            }
        } else {
            IEEE80211_AOW_DPRINTF("Last few time stamps starting %d\n", cur_stats->dbgCnt); 
            for (cnt = 0; cnt < AOW_DBG_WND_LEN/4; cnt++)
                IEEE80211_AOW_DPRINTF(" tsfDiff[%d] = %5d tsfDiff[%d] = %5d tsfDiff[%d] = %5d tsfDiff[%d] = %5d\n", 
                       cnt * 4, cur_stats->datalenBuf[cnt*4],
                       cnt * 4+1, cur_stats->datalenBuf[cnt*4+1],
                       cnt * 4+2, cur_stats->datalenBuf[cnt*4+2],
                       cnt * 4+3, cur_stats->datalenBuf[cnt*4+3]
                      );    
            
        }
        ic->ic_aow.ctrl.capture_done = 0;
    } else {
        IEEE80211_AOW_DPRINTF("Data capture not done\n");    
    }

    AOW_UNLOCK(ic);
}


void ieee80211_audio_stat_print(struct ieee80211com *ic)
{
    u_int16_t cnt;
    audio_stats *cur_stats = &ic->ic_aow.stats;

    AOW_LOCK(ic);

    if (cur_stats->rxFlg ) {
        /* Print the receive side data */
        IEEE80211_AOW_DPRINTF("Number of times stats function called %d and aborted %d\n",
               cur_stats->consume_aud_cnt ,cur_stats->consume_aud_abort ); 
        IEEE80211_AOW_DPRINTF("Number of AMPDUs received             %d\n", cur_stats->num_ampdus);
        IEEE80211_AOW_DPRINTF("Number of MPDUs received without fail %d\n", cur_stats->num_mpdus);
        IEEE80211_AOW_DPRINTF("Number of Missing/corrupted MPDUs     %d\n", cur_stats->num_missing_mpdu);
        
        IEEE80211_AOW_DPRINTF("Number of MPDUs missing per received AMPDU\n");
        for( cnt = 0; cnt < ATH_AOW_NUM_MPDU; cnt++ ) {
            IEEE80211_AOW_DPRINTF(" %d  MPDUs missing  %d times\n", cnt, cur_stats->num_missing_mpdu_hist[cnt]); 
        }
        IEEE80211_AOW_DPRINTF("Position of MPDUs missing              \n");
        for( cnt = 0; cnt < ATH_AOW_NUM_MPDU; cnt++ ) {
            IEEE80211_AOW_DPRINTF(" %d  pos  MPDUs missing %d times\n", cnt+1, cur_stats->missing_pos_cnt[cnt]); 
        }
        IEEE80211_AOW_DPRINTF("Number of MPDUs that came late       %d\n", cur_stats->late_mpdu);
        IEEE80211_AOW_DPRINTF("MPDU not cleared on time             %d\n", cur_stats->mpdu_not_consumed);
        IEEE80211_AOW_DPRINTF("Call to I2S DMA write                %d\n", cur_stats->ampdu_size_short);
        IEEE80211_AOW_DPRINTF("Too late for DMA write               %d\n", cur_stats->too_late_ampdu);
        
        IEEE80211_AOW_DPRINTF("Number of packets missing            %d\n", cur_stats->numPktMissing);
        
    } else {
        /* Print the transmit side data */
    }

    AOW_UNLOCK(ic);
}

void ieee80211_audio_stat_clear(struct ieee80211com *ic)
{
    ieee80211_aow_t* paow = &ic->ic_aow;

    AOW_LOCK(ic);
    memset(&paow->stats, 0, sizeof( audio_stats ));      

    paow->ok_framecount = 0;
    paow->nok_framecount = 0;
    paow->tx_framecount = 0;
    paow->macaddr_not_found = 0;
    paow->node_not_found = 0;
    paow->i2s_open_count = 0;
    paow->i2s_close_count = 0;
    paow->i2s_dma_start = 0;
    paow->er.bad_fcs_count = 0;
    paow->er.recovered_frame_count = 0;
    paow->er.aow_er_crc_invalid = 0;
    paow->er.aow_er_crc_valid = 0;
    paow->ec.index0_bad = 0;
    paow->ec.index1_bad = 0;
    paow->ec.index2_bad = 0;
    paow->ec.index3_bad = 0;
    paow->ec.index0_fixed = 0;
    paow->ec.index1_fixed = 0;
    paow->ec.index2_fixed = 0;
    paow->ec.index3_fixed = 0;

#if ATH_SUPPORT_AOW_DEBUG
    aow_dbg_init(&dbgstats);
#endif  /* ATH_SUPPORT_AOW_DEBUG */

    AOW_UNLOCK(ic);
}      
        

/*
 * function : ieee80211_aow_attach
 * -----------------------------
 * AoW Init Routine
 *
 */

void ieee80211_aow_attach(struct ieee80211com *ic)
{
    int i = 0;

    /* Init the buffers */
    for( i = 0; i < AUDIO_INFO_BUF_SIZE ; i++ ) {
        memset(&ic->ic_aow.info[i], 0, sizeof( audio_info ));
        ic->ic_aow.audio_data[i] = (u_int16_t *)OS_MALLOC(ic->ic_osdev, (ATH_AOW_SAMPLE_SIZE * ATH_AOW_NUM_SAMP_PER_AMPDU), GFP_KERNEL);
    }

    /* The last buffer in audio data is used for the zero buffer. Init it seperate and set it to zero */
    ic->ic_aow.audio_data[AOW_ZERO_BUF_IDX] = (u_int16_t *)OS_MALLOC(ic->ic_osdev, (ATH_AOW_SAMPLE_SIZE * ATH_AOW_NUM_SAMP_PER_AMPDU), GFP_KERNEL);
    memset(&ic->ic_aow.stats, 0, sizeof(audio_stats));

    /* Clear the zero buffer (the last one) */
    memset(ic->ic_aow.audio_data[AOW_ZERO_BUF_IDX], 0, sizeof(u_int16_t)*ATH_AOW_NUM_SAMP_PER_AMPDU);    
    
    ic->ic_aow.ctrl.audStopped=1;

    /* Set the interlever to default */
    ic->ic_aow.interleave = 1;

    /* Clear extended stats */
    memset(&ic->ic_aow.estats, 0, sizeof(ic->ic_aow.estats));
    /* To guard against a platform on which NULL is not zero */ 
    ic->ic_aow.estats.aow_latency_stats = NULL;
    ic->ic_aow.estats.aow_l2pe_stats = NULL;
    ic->ic_aow.estats.aow_plr_stats = NULL;
    
    /* Initlize the spinlock */
    AOW_LOCK_INIT(ic);
    AOW_ER_SYNC_LOCK_INIT(ic);
    AOW_LH_LOCK_INIT(ic);
    AOW_ESSC_LOCK_INIT(ic); 
    AOW_L2PE_LOCK_INIT(ic); 
    AOW_PLR_LOCK_INIT(ic); 
        
    /* generate the CRC32 table */
    chksum_crc32gentab();

    /* init the globals */
    aowinfo.ic = ic;

#if ATH_SUPPORT_AOW_DEBUG
    aow_dbg_init(&dbgstats);
#endif  /* ATH_SUPPORT_AOW_DEBUG */

    return;
}
        
/*
 * function : ieee80211_audio_receive
 * -----------------------------------------------
 * IEEE80211 AoW Receive handler, called from the
 * data delivery function.
 *
 */

int ieee80211_audio_receive(struct ieee80211vap *vap, 
                            wbuf_t wbuf, 
                            struct ieee80211_rx_status *rs)
{
    int32_t time_to_play = 0;
    int32_t air_delay = 0;

    u_int64_t aow_latency = 0;
    u_int64_t cur_tsf = 0;

    char *data;

    struct ieee80211com *ic = vap->iv_ic;
    struct audio_pkt *apkt;

    u_int32_t datalen = 0;
    apkt = (struct audio_pkt *)((char*)(wbuf_raw_data(wbuf)) + sizeof(struct ether_header));

    /* check for valid AOW packet */
    if (apkt->signature != ATH_AOW_SIGNATURE)
        return 0;

    /* Record ES/ESS statistics for AoW L2 Packet Error Stats */
    /* The FCS fail case would have already been handled by now. We
       only handle the FCS pass case.
       But we check the CRC again just in case ER too is enabled.*/
    if ((AOW_ES_ENAB(ic) || (AOW_ESS_ENAB(ic) && AOW_ESS_SYNC_SET(apkt->params)))
            && !(rs->rs_flags & IEEE80211_RX_FCS_ERROR)) {
            u_int32_t crc32;

            crc32 = chksum_crc32((u_int8_t*)wbuf_raw_data(wbuf),
                                 sizeof(struct ether_header) +
                                 sizeof(struct audio_pkt) -
                                 sizeof(u_int32_t));

            if (crc32 == apkt->crc32) {
                aow_l2pe_record(ic, true);
            }
    }

    /* Handle the Error Recovery logic, if enabled */
    if (AOW_ER_ENAB(ic) && (aow_er_handler(vap, wbuf, rs) == AOW_ER_DATA_NOK))
        return 0;

    /* extract the audio packet pointers */
    data = (char *)(wbuf_raw_data(wbuf)) + sizeof(struct ether_header) + sizeof(struct audio_pkt) + ATH_QOS_FIELD_SIZE;

    aow_latency = ic->ic_get_aow_latency_us(ic);    /* get latency */
    cur_tsf     = ic->ic_get_aow_tsf_64(ic);        /* get tsf */
    air_delay   = (cur_tsf - apkt->timestamp);      /* get air delay */


    /* get the data length for the received audio packet */
    datalen = wbuf_get_pktlen(wbuf) - sizeof(struct ether_header) - sizeof(struct audio_pkt) - ATH_QOS_FIELD_SIZE;


    /*
     * If Error Recovery is enabled and frame is FCS bad, then
     * get the data length from the validated AoW header
     */

    if (AOW_ER_ENAB(ic) && (rs->rs_flags & IEEE80211_RX_FCS_ERROR))
        datalen = apkt->pkt_len;


#if 0    


    AoW command support is deprecated at the transmit node.
    Enable this section, we continue to support audio play
    via a mounted USB flash drive.

    /* Handle AoW control commands here 
     * All the commands are unique ENUM values currently
     */
    if (datalen == sizeof(int)) {

        int command = 0;
        memcpy(&command, data, sizeof(int));
        if (ieee80211_aow_cmd_handler(ic, command) == AH_TRUE)
            return 0;
    }
#endif    

    air_delay = cur_tsf - apkt->timestamp;

    if (air_delay < aow_latency) {
        time_to_play = aow_latency - air_delay;
    } else {
        time_to_play = 0;
        ic->ic_aow.nok_framecount++;
        if (AOW_ES_ENAB(ic)
                || (AOW_ESS_ENAB(ic) &&  AOW_ESS_SYNC_SET(apkt->params))) {
            aow_nl_send_rxpl(ic,
                             apkt->seqno,
                             apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK,
                             rs->rs_rxseq,
#if ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG
                             air_delay,                            
#endif /* ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG */
                             ATH_AOW_PL_INFO_DELAYED);

            aow_lh_add_expired(ic);

            aow_plr_record(ic,
                           apkt->seqno,
                           apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK,
                           ATH_AOW_PLR_TYPE_NOK_FRMCNT);
        }  
    }

    /* get the future time to play */
    if (time_to_play ) {

        /* make sure the audio is started */
        ic->ic_start_aow_inter(ic, (u_int32_t)(apkt->timestamp + aow_latency), AOW_LAST_PACKET_TIME_DELTA);

        if (ic->ic_aow.ctrl.audStopped ) {
            IEEE80211_AOW_DPRINTF("Audio receive called with aud stopped. Time2Play %u\n", (u_int32_t)(apkt->timestamp + aow_latency));
            ic->ic_aow.ctrl.audStopped = 0;
            ieee80211_aow_cmd_handler(ic, ATH_AOW_START_COMMAND );
        }

        ic->ic_aow.ok_framecount++;

        /* start the timer */
        ic->ic_aow_proc_timer_start(ic);
        
        ieee80211_process_audio_data(vap, apkt, rs->rs_rxseq, cur_tsf, data);
    }

    return 0;

}EXPORT_SYMBOL(ieee80211_audio_receive);


/*
 * function : ieee80211_aow_cmd_handler
 * -------------------------------------------------------------
 * Handles the AoW commands, now deprecated at the transmit node
 * The AoW receive logic, calls this function depending on the
 * current receive state
 *
 */
int ieee80211_aow_cmd_handler(struct ieee80211com* ic, int command)
{ 
    int is_command = FALSE;

    switch (command) {
        case ATH_AOW_RESET_COMMAND:
        {

            /* 
             * Testing : Fix the i2s hang, issue.
             * Sleep for some time and allow the i2s to
             * drain the audio packets, this function
             * can be called from ISR context, we cannot
             * use OS_SLEEP()
             *
             */

             {
                int i = 0xffff;
                while(i--);
             }

            ic->ic_stop_aow_inter(ic);
            aow_i2s_deinit(ic);
            SET_I2S_STOP_FLAG(ic->ic_aow.i2s_flags);
            CLR_I2S_START_FLAG(ic->ic_aow.i2s_flags);
            is_command = TRUE;
            IEEE80211_AOW_DPRINTF("AoW : Received RESET command\n");
         }
         break;

        case ATH_AOW_START_COMMAND:
        {

            aow_i2s_init(ic);
            SET_I2S_START_FLAG(ic->ic_aow.i2s_flags);
            CLR_I2S_STOP_FLAG(ic->ic_aow.i2s_flags);
            is_command = TRUE;
            IEEE80211_AOW_DPRINTF("AoW : Received START command\n");
                
        }
        break;
        
        default:
            IEEE80211_AOW_DPRINTF("Aow : Unknown command\n");
            break;

    }

    return is_command;
}    



/*
 * function : ieee80211_aow_detach
 * ----------------------------------------
 * Cleanup handler for IEEE80211 AoW module
 *
 */
int ieee80211_aow_detach(struct ieee80211com *ic)
{
    u_int32_t cnt;

    if (IS_I2S_OPEN(ic)) {
        ar7242_i2s_close();
        SET_I2S_OPEN_STATE(ic, FALSE);
    }        

    AOW_LOCK(ic);

    for( cnt = 0; cnt < AUDIO_INFO_BUF_SIZE; cnt++ ) {
        OS_FREE(ic->ic_aow.audio_data[cnt]);
    }

    /* Free the last buffer that is used as zero index */
    OS_FREE(ic->ic_aow.audio_data[AOW_ZERO_BUF_IDX]); 

    // Just in case the deinit has not been called.
    aow_lh_deinit(ic);
    aow_am_deinit(ic);
    aow_l2pe_deinit(ic);
    aow_plr_deinit(ic);
    
    AOW_UNLOCK(ic);
    AOW_LOCK_DESTROY(ic);
    AOW_LH_LOCK_DESTROY(ic);
    AOW_ESSC_LOCK_DESTROY(ic);
    AOW_L2PE_LOCK_DESTROY(ic);
    AOW_PLR_LOCK_DESTROY(ic);

    return 0;
}


/******************************************************************************

    Functions relating to AoW PoC 2
    Interface fu:nctions between USB and Wlan

******************************************************************************/    


int aow_get_macaddr(int channel, int index, struct ether_addr *macaddr)
{
    struct ieee80211com *ic = aowinfo.ic;
    return ic->ic_get_aow_macaddr(ic, channel, index, macaddr);
}

int aow_get_num_dst(int channel)
{
    struct ieee80211com *ic = aowinfo.ic;
    return ic->ic_get_num_mapped_dst(ic, channel);
}

void wlan_get_tsf(u_int64_t* tsf)
{
    struct ieee80211com* ic = aowinfo.ic;
    *tsf = ic->ic_get_aow_tsf_64(ic);
#if ATH_SUPPORT_AOW_DEBUG
    /* update the aow stats */
    aow_update_dbg_stats(&dbgstats, (*tsf - dbgstats.ref_p_tsf));
    /* store the complete tsf for future use */
    dbgstats.ref_p_tsf = *tsf;
#endif  /* ATH_SUPPORT_AOW_DEBUG */

}EXPORT_SYMBOL(wlan_get_tsf);


/*
 * function : wlan_aow_tx
 * --------------------------------------
 * transmit handler for AoW audio packets 
 *
 */

int wlan_aow_tx(char *data, int datalen, int channel, u_int64_t tsf)
{
    struct ether_addr macaddr;
    struct ieee80211_node *ni = NULL;
    struct ieee80211com *ic = aowinfo.ic;

    int dst_cnt = 0;
    int seqno = 0;
    int playval;
    int play_local;
    int play_channel;
    bool setlogSync = FALSE;
    int done = FALSE;
    int get_seqno = TRUE;
    int j = 0;



    /* get the number of destination, to stream */
    dst_cnt = aow_get_num_dst(channel);

    for (j = 0; j < dst_cnt; j++) {

        /* check for the valid channel */
        if (!(ic->ic_aow.channel_set_flag & (1 << channel))) {
            ic->ic_aow.macaddr_not_found++;
            return FALSE;
        }

        /* get destination */
        if (!aow_get_macaddr(channel, j, &macaddr)) {
            ic->ic_aow.macaddr_not_found++;
            return FALSE;
        }        

        /* get the destination */
        ni = ieee80211_find_node(&ic->ic_sta, macaddr.octet);

        /* valid destination */
        if (!ni) {
            ic->ic_aow.node_not_found++;
            return FALSE;
        }        

        /* check if play local is enabled */
        playval = ic->ic_get_aow_playlocal(ic);
        
        if ((playval & ATH_AOW_PLAY_LOCAL_MASK) && !done) {

            play_local   = playval & ATH_AOW_PLAY_LOCAL_MASK;
            play_channel = playval >> ATH_AOW_PLAY_LOCAL_MASK;

            if ( play_local && (channel == play_channel)) {

                u_int64_t aow_latency = ic->ic_get_aow_latency_us(ic);

                int playcount = datalen / AOW_I2S_DATA_BLOCK_SIZE;
                int left      = datalen % AOW_I2S_DATA_BLOCK_SIZE;

                char *pdata   = data;

                while (playcount--) {
                    ar7242_i2s_write(AOW_I2S_DATA_BLOCK_SIZE, pdata);
                    pdata += AOW_I2S_DATA_BLOCK_SIZE;
                }                

                if (left) {
                    ar7242_i2s_write(left, pdata);
                }                
                if (IS_I2S_NEED_DMA_START(ic->ic_aow.i2s_flags)) {
                    ic->ic_aow.i2s_dmastart = TRUE;
                }

                ic->ic_start_aow_inter(ic, (u_int32_t)(tsf + aow_latency), AOW_PACKET_INTERVAL);
            }
            
            done = TRUE;
        }

        /* up the transmit count */
        ic->ic_aow.tx_framecount++;

        /* update the seqno and setlogSync flag */
        if (get_seqno == TRUE) {
            ic->ic_get_aow_chan_seqno(ic, channel, &seqno);
            
            if (AOW_ESS_ENAB(ic)) {
                setlogSync = aow_essc_testdecr(ic, channel);
            } else {
                setlogSync = 0;
            }
            
            get_seqno = FALSE;
        }            
       
        /* now send the data */
        ieee80211_send_aow_data_ipformat(ni, data, datalen, seqno, tsf, setlogSync);
    }
    return TRUE;

}EXPORT_SYMBOL(wlan_aow_tx);

void ieee80211_aow_clear_i2s_stats(struct ieee80211com *ic)
{
    i2s_clear_stats();
}    

void ieee80211_aow_i2s_stats(struct ieee80211com* ic)
{
    i2s_stats_t stats;
    i2s_get_stats(&stats);

    IEEE80211_AOW_DPRINTF("I2S Write fail   = %d\n", stats.write_fail);
    IEEE80211_AOW_DPRINTF("I2S Rx underflow = %d\n", stats.rx_underflow);
    IEEE80211_AOW_DPRINTF("Tasklet Count    = %d\n", stats.tasklet_count);
    IEEE80211_AOW_DPRINTF("Pkt repeat Count = %d\n", stats.repeat_count);

    if (stats.aow_sync_enabled) {
        IEEE80211_AOW_DPRINTF("SYNC overflow  = %d\n", stats.sync_buf_full);
        IEEE80211_AOW_DPRINTF("SYNC underflow = %d\n", stats.sync_buf_empty);
    }
}

void ieee80211_aow_ec_stats(struct ieee80211com* ic)
{
    if (!AOW_EC_ENAB(ic))
        return;

    IEEE80211_AOW_DPRINTF("Index0 Fixed = %d\n", ic->ic_aow.ec.index0_fixed);
    IEEE80211_AOW_DPRINTF("Index1 Fixed = %d\n", ic->ic_aow.ec.index1_fixed);
    IEEE80211_AOW_DPRINTF("Index2 Fixed = %d\n", ic->ic_aow.ec.index2_fixed);
    IEEE80211_AOW_DPRINTF("Index3 Fixed = %d\n", ic->ic_aow.ec.index3_fixed);

}

void ieee80211_aow_clear_stats(struct ieee80211com* ic)
{


}

/*
 * Error concealment feature
 *
 */
int aow_ec_attach(struct ieee80211com *ic)
{
    memset(ic->ic_aow.ec.prev_data, 0, sizeof(ic->ic_aow.ec.prev_data));
    return 0;
}

int aow_ec_deattach(struct ieee80211com *ic)
{
    return 0;
}

/* 
 * Error recovery feature
 *
 */

/* initalize the error recovery state and variables */
int aow_er_attach(struct ieee80211com *ic)
{
    ic->ic_aow.er.cstate = ER_IDLE;
    ic->ic_aow.er.expected_seqno = 0;

    if (!ic->ic_aow.er.data)
        return FALSE;

    return TRUE;
}

/* deinitalize the error recovery state and variables */
int aow_er_deattach(struct ieee80211com *ic)
{
    if (ic->ic_aow.er.data) {
        OS_FREE(ic->ic_aow.er.data);
    }        
    return FALSE;
}

/* handler for aow error recovery feature */
int aow_er_handler(struct ieee80211vap *vap, 
                   wbuf_t wbuf, 
                   struct ieee80211_rx_status *rs)
{
    int crc32;
    struct audio_pkt *apkt;
    struct ieee80211com *ic = vap->iv_ic;
    char *data;
    int ret = AOW_ER_DATA_NOK;
    int i = 0;
    int datalen;

    KASSERT((ic != NULL), ("ic Null pointer"));

    AOW_ER_SYNC_IRQ_LOCK(ic);

    apkt = (struct audio_pkt *)((char*)(wbuf_raw_data(wbuf)) + 
           sizeof(struct ether_header));

    data = (char *)(wbuf_raw_data(wbuf)) + 
           sizeof(struct ether_header) + 
           sizeof(struct audio_pkt) + 
           ATH_QOS_FIELD_SIZE;

    datalen = wbuf_get_pktlen(wbuf) - 
              sizeof(struct ether_header) - 
              sizeof(struct audio_pkt) - 
              ATH_QOS_FIELD_SIZE;

    KASSERT(apkt != NULL, ("Audio Null Pointer"));              
    KASSERT(data != NULL, ("Data Null Pointer"));

    if (!(rs->rs_flags & IEEE80211_RX_FCS_ERROR)) {

        /* Handle the FCS ok case */

        if (ic->ic_aow.er.expected_seqno == apkt->seqno) {
            /* Received the expected frame,
             * reset the cached data buffer
             * clear er related state
             */
            
            ic->ic_aow.er.cstate = ER_IDLE;
            ic->ic_aow.er.flag &= ~AOW_ER_DATA_VALID;
        }            
        
        /* Clear the AoW ER state machine, as the packets come in
         * order, this is ok 
         */
        ic->ic_aow.er.expected_seqno = apkt->seqno++;
        ic->ic_aow.er.cstate = ER_IDLE;
        ic->ic_aow.er.count = 0;
        ic->ic_aow.er.flag &= ~AOW_ER_DATA_VALID;
        ret = AOW_ER_DATA_OK;

    } else {

        /* Handle the FCS nok case */

        //TODO: Cover sender & receiver addresses in the CRC

        crc32 = chksum_crc32((u_int8_t*)apkt, (sizeof(struct audio_pkt) - sizeof(int)));

        if (crc32 == apkt->crc32) {

            IEEE80211_AOW_DPRINTF("Bad FCS frame, AoW CRC pass ES = %d PS = %d\n", ic->ic_aow.er.expected_seqno, apkt->seqno);

            ic->ic_aow.er.aow_er_crc_valid++;

            /* AoW header is valid */
            if (!(ic->ic_aow.er.flag & AOW_ER_DATA_VALID)) {

               /* first instance, store the data */
               KASSERT((apkt->pkt_len <= ATH_AOW_NUM_SAMP_PER_AMPDU), ("Invalid data length"));
               memcpy(ic->ic_aow.er.data, data, apkt->pkt_len);

               /* set the er state info */
               ic->ic_aow.er.flag |= AOW_ER_DATA_VALID;
               ic->ic_aow.er.datalen = apkt->pkt_len;
               ic->ic_aow.er.count++;

            } else {

               /* consecutive data, recover data from them */
               /* xor the incoming data to stored buffer */ 
               KASSERT((ic->ic_aow.er.datalen <= ATH_AOW_NUM_SAMP_PER_AMPDU), ("Invalid data length"));
               for (i = 0; i < ic->ic_aow.er.datalen; i++) {
                   ic->ic_aow.er.data[i] |= data[i];
               }                   
                
               /* update the er state */
               ic->ic_aow.er.count++;
            }

            /* check the err packet occurance had exceeded the limit */
            if (ic->ic_aow.er.count > AOW_ER_MAX_RETRIES) {

                IEEE80211_AOW_DPRINTF("Aow ER : Passing the recovered data\n");

                /* pass the data for audio processing */
                KASSERT(data != NULL, ("Data Null Pointer"));
                KASSERT(ic->ic_aow.er.datalen <= ATH_AOW_NUM_SAMP_PER_AMPDU, ("Invalid datalength"));
                memcpy(data, ic->ic_aow.er.data, ic->ic_aow.er.datalen);

                /* reset the er state */
                ic->ic_aow.er.count = 0;
                ic->ic_aow.er.flag &= ~AOW_ER_DATA_VALID;
                ic->ic_aow.er.datalen = 0;
                ic->ic_aow.er.recovered_frame_count++;

                ret = AOW_ER_DATA_OK;
            }

        } else {
            ic->ic_aow.er.aow_er_crc_invalid++;
            /* AoW header is invalid */
        }
    }

    AOW_ER_SYNC_IRQ_UNLOCK(ic);

    return ret;
    
}

/*
 * This function conceals error for single 
 * packet losses
 *
 * X01  X11 X21 X31  |
 * X02  X12 X22 X32  |
 * X03  X13 X23 X33  |
 *
 *
 * The formula to recover the data is given
 * below
 *
 * If the failed frame index is 0 or 3
 * -----------------------------------
 * R = Row, Failed index
 * C = Coloum, the data elements
 *
 * For Sub frame index 0
 * ---------------------
 * Off set of two is required for Left and Right channels
 * The Left and Right channels alternate
 * X[R][C] = (X[3][C - 2] + X[1][C])/2
 *
 * For Sub frame index 3
 * ---------------------
 * Off set of two is required for Left and Right channels
 * The Left and Right channels alternate
 * X[R][C] = (X[2][C] + X[0][C + 2])/2
 * 
 * If the failed frame index is 1 or 2
 * -----------------------------------
 * R = Row, Failed index
 * C = Coloum, the data elements
 * 
 * X[R][C] = (X[R -1][C] + X[(R+1)][C]) >> 1
 *
 * Example : (X12 = X02 + X22) / 2
 * 
 */

int aow_ec(struct ieee80211com *ic, audio_info* info, int index)
{
    u_int32_t row, col;
    u_int32_t pre_row;
    u_int32_t post_row;
    u_int16_t result;

    row = index;

    switch(index) {
        case AOW_SUB_FRAME_IDX_0:
           pre_row = AOW_SUB_FRAME_IDX_3;
           post_row = AOW_SUB_FRAME_IDX_1;

           /* TODO : Update with actual diff value 
            * Currently we map the next sample in the buffer
            * for Left and Right channels
            */
           info->audBuffer[row][0] = info->audBuffer[AOW_SUB_FRAME_IDX_1][0];
           info->audBuffer[row][1] = info->audBuffer[AOW_SUB_FRAME_IDX_1][1];

           for (col = 2; col < ATH_AOW_NUM_SAMP_PER_MPDU; col++) {
               result = ((((int16_t)__bswap16(info->audBuffer[pre_row][col - 2])) >> 1) +
                     (((int16_t)__bswap16(info->audBuffer[post_row][col])) >> 1));
               info->audBuffer[row][col] = __bswap16(result);
           }                    
           break;

        case AOW_SUB_FRAME_IDX_3:                
            pre_row  = AOW_SUB_FRAME_IDX_2;
            post_row = AOW_SUB_FRAME_IDX_0;

            for (col = 0; col < ATH_AOW_NUM_SAMP_PER_MPDU - 2; col++) {
                result = ((((int16_t)__bswap16(info->audBuffer[pre_row][col])) >> 1) +
                      (((int16_t)__bswap16(info->audBuffer[post_row][col + 2])) >> 1));
        
                info->audBuffer[row][col] = __bswap16(result);
            }                    

            /* TODO : Update with actual diff value 
             * Currently we map the previous sample in the buffer
             * for Left and Right channels
             */
            info->audBuffer[row][ATH_AOW_NUM_SAMP_PER_MPDU - 2] = info->audBuffer[AOW_SUB_FRAME_IDX_2][ATH_AOW_NUM_SAMP_PER_MPDU - 2];
            info->audBuffer[row][ATH_AOW_NUM_SAMP_PER_MPDU - 1] = info->audBuffer[AOW_SUB_FRAME_IDX_2][ATH_AOW_NUM_SAMP_PER_MPDU - 1];
            break;                    

        case AOW_SUB_FRAME_IDX_1:
        case AOW_SUB_FRAME_IDX_2:

            /* get the pre and post row index */
            pre_row = row - 1;
            post_row = row + 1; 

            for (col = 0; col < ATH_AOW_NUM_SAMP_PER_MPDU; col++) {
                result = ((((int16_t)__bswap16(info->audBuffer[pre_row][col])) >> 1) +
                          (((int16_t)__bswap16(info->audBuffer[post_row][col])) >> 1));
                
                info->audBuffer[row][col] = __bswap16(result);
            }
            break;
    }

    return 0;
}


/*
 * Function to recover multiple error 
 *
 */

int aow_ecm(struct ieee80211com *ic, audio_info* info, int failmap)
{
    if (failmap & 0x1) {
        aow_ec(ic, info, 0);
        ic->ic_aow.ec.index0_fixed++;
    }                

    if (failmap & 0x2) {
        aow_ec(ic, info, 1);
        ic->ic_aow.ec.index1_fixed++;
    }        

    if (failmap & 0x4) {
        aow_ec(ic, info, 2);
        ic->ic_aow.ec.index2_fixed++;
    }        

    if (failmap & 0x8) {
        aow_ec(ic, info, 3);
        ic->ic_aow.ec.index3_fixed++;
    }        

    return 0;        
}

/*
 * Find the number of bits set in integer
 *
 */
int num_of_bits_set(int i)
{
    i = i - ((i >> 1) & 0x55555555);
    i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
    return (((i + (i >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
}

/*
 * AoW Error Concealment function handler 
 *
 */

int aow_ec_handler(struct ieee80211com *ic, audio_info *info)
{

    /* find the number of failed frame */
    int failmap = (~(info->foundBlocks) & AOW_EC_SUB_FRAME_MASK);
    int failed_frame_count = num_of_bits_set((~(info->foundBlocks) & AOW_EC_SUB_FRAME_MASK));

    if (failed_frame_count == 0)
        return 0;

    aow_ecm(ic, info, failmap);
    return 0;
}

static void aow_update_ec_stats(struct ieee80211com* ic, int fmap)
{
    if (fmap & 0x1)
        ic->ic_aow.ec.index0_bad++;
    if (fmap & 0x2)
        ic->ic_aow.ec.index1_bad++;
    if (fmap & 0x4)
        ic->ic_aow.ec.index2_bad++;
    if (fmap & 0x8)
        ic->ic_aow.ec.index3_bad++;
}

/*
 * function : ieee80211_aow_get_stats
 * ----------------------------------
 * Print the AoW Related stats to console
 *
 */

int ieee80211_aow_get_stats(struct ieee80211com*ic)
{
    if (ic->ic_aow.ctrl.audStopped) {

        /* print AoW Aggregation stats */
        ieee80211_audio_stat_print(ic);

        /* Disable the GPIO Interrupts */
        ic->ic_stop_aow_inter(ic);

        /* Simple implementation */
        IEEE80211_AOW_DPRINTF("\nTransmit Stats\n");
        IEEE80211_AOW_DPRINTF("--------------------------------------------\n");
        IEEE80211_AOW_DPRINTF("Tx frames      = %d\n", ic->ic_aow.tx_framecount);
        IEEE80211_AOW_DPRINTF("Addr not found = %d\n", ic->ic_aow.macaddr_not_found);
        IEEE80211_AOW_DPRINTF("Node not found = %d\n", ic->ic_aow.node_not_found);

        IEEE80211_AOW_DPRINTF("\nReceive Stats\n");
        IEEE80211_AOW_DPRINTF("--------------------------------------------\n");
        IEEE80211_AOW_DPRINTF("Intime frames  = %d\n", ic->ic_aow.ok_framecount);
        IEEE80211_AOW_DPRINTF("Delayed frames = %d\n", ic->ic_aow.nok_framecount);
        IEEE80211_AOW_DPRINTF("Latency        = %d\n", ic->ic_get_aow_latency(ic));

        IEEE80211_AOW_DPRINTF("\nI2S Stats\n");
        IEEE80211_AOW_DPRINTF("--------------------------------------------\n");
        IEEE80211_AOW_DPRINTF("I2S Open       = %d\n", ic->ic_aow.i2s_open_count);
        IEEE80211_AOW_DPRINTF("I2S Close      = %d\n", ic->ic_aow.i2s_close_count);
        IEEE80211_AOW_DPRINTF("I2S DMA start  = %d\n", ic->ic_aow.i2s_dma_start);

        ieee80211_aow_i2s_stats(ic);

        if (AOW_EC_ENAB(ic)) {
            IEEE80211_AOW_DPRINTF("\nError Concealment Stats\n");
            IEEE80211_AOW_DPRINTF("--------------------------------------------\n");
            ieee80211_aow_ec_stats(ic);
        }        

        IEEE80211_AOW_DPRINTF("\n");
    } else {
        IEEE80211_AOW_DPRINTF("Device busy\n");
    }

    return 0;
}

/*
 * function : ieee80211_send_aow_data_ipformat
 * --------------------------------------------------------------
 * This function sends the AoW data, in the ipformat. 
 * The IP header is appended to the AoW data and the normal
 * transmit path is invoked, to make sure that it has aggregation
 * and other features enabled on its way
 *
 */
int ieee80211_send_aow_data_ipformat(struct ieee80211_node* ni, 
                                     void *pkt,
                                     int len, 
                                     u_int32_t seqno, 
                                     u_int64_t tsf,
                                     bool setlogSync)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    wbuf_t wbuf[ATH_AOW_NUM_MPDU];
    struct ether_header *eh;

    int total_len = 0;
    int retVal    = 0;

    int16_t cnt, sampCnt;

    u_int8_t *frame;
    
    u_int16_t tmpBuf[ATH_AOW_NUM_MPDU][ATH_AOW_NUM_SAMP_PER_MPDU];
    u_int16_t mpdu_len;
    u_int16_t num_samp = len >> 1; /* assuming 16 bit audio samples */
    u_int16_t *dataPtr = (u_int16_t *)pkt;



    /* 
     * Packet Format
     *
     * |---------|-----|-------|------------|----------|------------|-------------|--------|
     * | Header  | Sig | Seqno | Timestamp  |  length  | Parameters | Audio data  | CRC    |
     * |---------|-----|-------|------------|----------|------------|-------------|--------|
     *
     * The legth we get the ioctl interface is gives the audio data length
     * The frame length is a sum of:
     *      - 802.11 mac header, assumes QoS
     *      - AOW signature
     *      - AOW sequence number
     *      - AOW Timestamp
     *      - AOW Length (Of all the packets in the AMPDU)
     *      - Parameters (2 bytes )
     *      - CRC 
     *
     * 
     */


     /* 
      * Some assumptions about the data size here, assume it is 8ms = 48*2*8 samples = 768 16 bit words
      * MPDU length is always = 192 words
      */

    /* Debug feature to force the input to known value */
    if (ic->ic_aow.ctrl.force_input )
    {
        for( sampCnt = 0; sampCnt < num_samp; sampCnt++ ) {
            dataPtr[sampCnt] = sampCnt;
        }
    }

    /* Interleave and copy the data into tmp buffers */
    if (AOW_INTERLEAVE_ENAB(ic)) {

        for ( cnt = 0; cnt < num_samp; cnt += 2) {
            sampCnt = cnt >> 1; /* This is for the left and the right sample */
   
            tmpBuf[sampCnt & ATH_AOW_MPDU_MOD_MASK][ ((sampCnt >> ATH_AOW_NUM_MPDU_LOG2) << 1)] = dataPtr[cnt];
            tmpBuf[sampCnt & ATH_AOW_MPDU_MOD_MASK][ ((sampCnt >> ATH_AOW_NUM_MPDU_LOG2) << 1) + 1] = dataPtr[cnt+1];
        }

    } else {

        u_int32_t sub_frame_len = (num_samp + ATH_AOW_MPDU_MOD_MASK) >> ATH_AOW_NUM_MPDU_LOG2;

        for( cnt = 0; cnt < ATH_AOW_NUM_MPDU; cnt++ ) {
            memcpy( tmpBuf[cnt], dataPtr + cnt*sub_frame_len, sizeof(u_int16_t)*sub_frame_len);        
        }
    }

    /* get the total length of the AOW packet */
    mpdu_len  = (num_samp + ATH_AOW_PARAMS_SUB_FRAME_NO_MASK) >> ATH_AOW_NUM_MPDU_LOG2;
    total_len = mpdu_len * ATH_AOW_SAMPLE_SIZE + sizeof(struct audio_pkt) + ATH_QOS_FIELD_SIZE;
    
    /* allocate the wbuf for the wbuf */
    for ( cnt = 0; cnt < ATH_AOW_NUM_MPDU; cnt++ ) {

        wbuf[cnt] = ieee80211_get_aow_frame(ni, &frame, total_len);

        /* Check if there is enough memory. If not, free all the buffers and exit */
        if (wbuf[cnt] == NULL) {

            for(; cnt >= 0; cnt-- ) {
                wbuf_free(wbuf[cnt]);
            }   

            vap->iv_stats.is_tx_nobuf++;
            ieee80211_free_node(ni);

            return -ENOMEM;
        }        
    }
    
    
    for ( cnt = 0; cnt < ATH_AOW_NUM_MPDU; cnt++ ) {

        /* take the size of QOS, SIG, TS into account */
        int offset = ATH_QOS_FIELD_SIZE + sizeof(struct audio_pkt);

        /* prepare the ethernet header */
        eh = (struct ether_header*)wbuf_push(wbuf[cnt], sizeof(struct ether_header));
        eh = (struct ether_header*)wbuf_raw_data(wbuf[cnt]);
        eh = (struct ether_header*)wbuf_header(wbuf[cnt]);
    
        /* prepare the ethernet header */
        IEEE80211_ADDR_COPY(eh->ether_dhost, ni->ni_macaddr);
        IEEE80211_ADDR_COPY(eh->ether_shost, vap->iv_myaddr);
        eh->ether_type = ETHERTYPE_IP;
    
        /* copy the user data to the skb */
        {
            char* pdata = (char*)&eh[1];
            struct audio_pkt *apkt = (struct audio_pkt*)pdata;
    
            apkt->signature = ATH_AOW_SIGNATURE;
            apkt->seqno = seqno;
            apkt->timestamp = tsf;
            /* Set the length field */
            apkt->pkt_len = num_samp;
            /* Set the parameter field */
            apkt->params = cnt;     
            apkt->params |= ((AOW_INTERLEAVE_ENAB(ic)) & ATH_AOW_PARAMS_INTRPOL_ON_FLG_MASK)<<ATH_AOW_PARAMS_INTRPOL_ON_FLG_S;
            /* If setlogSync is true, it automatically signifies
               that ESS is enabled. We optimize on processing */
            apkt->params |= (setlogSync & ATH_AOW_PARAMS_LOGSYNC_FLG_MASK)<< ATH_AOW_PARAMS_LOGSYNC_FLG_S; 

            if (AOW_ER_ENAB(ic) || AOW_ES_ENAB(ic) || setlogSync) {
                apkt->crc32 = chksum_crc32((u_int8_t*)eh,
                                           (sizeof(struct ether_header) +
                                            sizeof(struct audio_pkt) -
                                            sizeof(int)));
            }

            /* copy the user data to skb */
            memcpy((pdata + offset), tmpBuf[cnt], mpdu_len * ATH_AOW_SAMPLE_SIZE);
            if (!(cnt)) {
                if (ic->ic_aow.ctrl.capture_data ) {
                    if (ic->ic_aow.stats.prevTsf ) { 
                        ic->ic_aow.stats.datalenBuf[ic->ic_aow.stats.dbgCnt] = (u_int32_t)(apkt->timestamp - ic->ic_aow.stats.prevTsf);
                        ic->ic_aow.stats.sumTime += ic->ic_aow.stats.datalenBuf[ic->ic_aow.stats.dbgCnt];
                        if (ic->ic_aow.stats.datalenBuf[ic->ic_aow.stats.dbgCnt] > ATH_AOW_STATS_THRESHOLD ) ic->ic_aow.stats.grtCnt++;
                        
                        ic->ic_aow.stats.datalenLen[ic->ic_aow.stats.dbgCnt++] = apkt->pkt_len;    
                        if (ic->ic_aow.stats.dbgCnt == AOW_DBG_WND_LEN) {
                            ic->ic_aow.ctrl.capture_data = 0;
                            ic->ic_aow.ctrl.capture_done = 1;    
                        }
                    }
                    ic->ic_aow.stats.prevTsf = apkt->timestamp;
                }
            }
        }
    
        /* invoke the normal transmit path */
        retVal |= ath_tx_send((wbuf_t)wbuf[cnt]);
    }

    ieee80211_free_node(ni);
    return retVal;


}

/*
 * function : ieee80211_get_aow_frame
 * --------------------------------------------------------
 * Allocates the frame for the given length and returns the
 * pointer to SKB
 *
 */
static wbuf_t 
ieee80211_get_aow_frame(struct ieee80211_node* ni, 
                        u_int8_t **frm, 
                        u_int32_t pktlen)
{
    struct ieee80211com *ic = ni->ni_ic;
    wbuf_t wbuf;

    wbuf = wbuf_alloc(ic->ic_osdev, WBUF_TX_DATA, pktlen);
    if (wbuf == NULL)
        return NULL;

    wbuf_set_node(wbuf, ieee80211_ref_node(ni));
    wbuf_set_priority(wbuf, WME_AC_VO);
    wbuf_set_tid(wbuf, WME_AC_TO_TID(WME_AC_VO));
    wbuf_set_pktlen(wbuf, pktlen);

    return wbuf;
}

/* Init and DeInit operations for Extended Statistics */

/*
 * function : aow_es_base_init
 * ----------------------------------
 * Extended stats: Base Initialization (common to
 * synchronized and unsynchronized modes).
 *
 */
int aow_es_base_init(struct ieee80211com *ic)
{
    int retval;
    
    if ((retval = aow_lh_init(ic)) < 0) {
        return retval;
    }

    if ((retval = aow_am_init(ic)) < 0) {
        aow_lh_deinit(ic);
        return retval;
    }
    
    if ((retval =  aow_l2pe_init(ic)) < 0) {
        aow_lh_deinit(ic);
        aow_am_deinit(ic);
        return retval;
    }
    
    if ((retval =  aow_plr_init(ic)) < 0) {
        aow_lh_deinit(ic);
        aow_am_deinit(ic);
        aow_l2pe_deinit(ic);
        return retval;
    }

    OS_MEMSET(&ic->ic_aow.estats.aow_mcs_stats,
              0,
              sizeof(ic->ic_aow.estats.aow_mcs_stats));

    return 0;
}

/*
 * function : aow_es_base_deinit
 * ----------------------------------
 * Extended stats: Base De-Initialization (common to
 * synchronized and unsynchronized modes).
 */

void aow_es_base_deinit(struct ieee80211com *ic)
{
    aow_lh_deinit(ic);
    aow_am_deinit(ic);
    aow_l2pe_deinit(ic);
    aow_plr_deinit(ic);

    OS_MEMSET(&ic->ic_aow.estats.aow_mcs_stats,
              0,
              sizeof(ic->ic_aow.estats.aow_mcs_stats));
}

/*
 * function : aow_clear_ext_stats
 * ----------------------------------
 * Extended stats: Clear extended stats
 *
 */
void aow_clear_ext_stats(struct ieee80211com *ic)
{
    if (AOW_ES_ENAB(ic) || AOW_ESS_ENAB(ic)) {
        aow_lh_reset(ic);
        aow_l2pe_reset(ic);
        aow_plr_reset(ic);

        OS_MEMSET(&ic->ic_aow.estats.aow_mcs_stats,
                  0,
                  sizeof(ic->ic_aow.estats.aow_mcs_stats));
    }
}

/*
 * function : aow_print_ext_stats
 * ----------------------------------
 * Extended stats: Print extended stats
 *
 */
void aow_print_ext_stats(struct ieee80211com * ic)
{
    if (!AOW_ES_ENAB(ic) && !AOW_ESS_ENAB(ic)) {
        return;
    }

    if (ic->ic_aow.ctrl.audStopped) {
        if (ic->ic_aow.node_type == ATH_AOW_NODE_TYPE_RX) {
            aow_lh_print(ic);
            aow_l2pe_print(ic);
            aow_plr_print(ic);
        } else if (ic->ic_aow.node_type == ATH_AOW_NODE_TYPE_TX) {
            aow_mcss_print(ic);
        }
    } else {
        IEEE80211_AOW_DPRINTF("Device busy\n");
    }
}

/* Operations on Latency Histogram (LH) records */

/*
 * function : aow_lh_init
 * ----------------------------------
 * Extended stats: Init latency histogram recording
 *
 */
int aow_lh_init(struct ieee80211com *ic)
{
   u_int64_t aow_latency;
   u_int32_t aow_latency_max_bin; 

   aow_latency = ic->ic_get_aow_latency_us(ic);

   if (aow_latency > ATH_AOW_LH_MAX_LATENCY_US)
   {
        IEEE80211_AOW_DPRINTF("Max aow_latency covered by "
                              "Latency Histogram: %u microseconds\n",
                              ATH_AOW_LH_MAX_LATENCY_US);
        return -EINVAL;
   }

   AOW_LH_LOCK(ic);    
   
   ic->ic_aow.estats.aow_latency_max_bin =
       (u_int32_t)aow_latency/ATH_AOW_LATENCY_RANGE_SIZE;

   aow_latency_max_bin = ic->ic_aow.estats.aow_latency_max_bin;

   ic->ic_aow.estats.aow_latency_stats =
       (u_int32_t *)OS_MALLOC(ic->ic_osdev,
                                 aow_latency_max_bin * sizeof(u_int32_t),
                                 GFP_KERNEL);
    
   if (NULL == ic->ic_aow.estats.aow_latency_stats)
   {
        IEEE80211_AOW_DPRINTF("%s: Could not allocate memory\n", __func__);
        AOW_LH_UNLOCK(ic);
        return -ENOMEM;
   }

   memset(ic->ic_aow.estats.aow_latency_stats,
          0,
          aow_latency_max_bin * sizeof(u_int32_t));

   ic->ic_aow.estats.aow_latency_stat_expired = 0;

   AOW_LH_UNLOCK(ic);

   return 0; 
}

/*
 * function : aow_lh_reset
 * ----------------------------------
 * Extended stats: Reset latency histogram recording
 *
 */
int aow_lh_reset(struct ieee80211com *ic)
{
   aow_lh_deinit(ic);
   return aow_lh_init(ic);
}

/*
 * function : aow_record_latency
 * ----------------------------------
 * Extended stats: Increment count in latency histogram bin for MPDUs received
 * before expiry of max AoW latency.
 *
 */
int aow_lh_add(struct ieee80211com *ic, const u_int64_t latency)
{
   u_int64_t aow_latency;

   aow_latency = ic->ic_get_aow_latency_us(ic);

   if (latency >= aow_latency) {
       // This function should not have been called, but we guard against it.
       return aow_lh_add_expired(ic);
   }
   
   AOW_LH_LOCK(ic);

   if (NULL == ic->ic_aow.estats.aow_latency_stats)
   {
       AOW_LH_UNLOCK(ic); 
       return -1; 
   } 

   // TODO: If profiling results show a slowdown, we can explore
   // the possibility of some optimized logic instead of division.
   ic->ic_aow.estats.aow_latency_stats[\
       (u_int32_t)latency/ATH_AOW_LATENCY_RANGE_SIZE]++;
    
   AOW_LH_UNLOCK(ic); 
    
   return 0;   
}

/*
 * function : aow_lh_add_expired
 * ----------------------------------
 * Extended stats: Increment count in latency histogram bin for MPDUs received
 * after expiry of max AoW latency.
 *
 */
int aow_lh_add_expired(struct ieee80211com *ic)
{
   AOW_LH_LOCK(ic);    
  
   /* This serves as a check to ensure LH is active */ 
   if (NULL == ic->ic_aow.estats.aow_latency_stats)
   {
       AOW_LH_UNLOCK(ic); 
       return -1; 
   } 

   ic->ic_aow.estats.aow_latency_stat_expired++; 

   AOW_LH_UNLOCK(ic);

   return 0;
}

/*
 * function : aow_lh_print
 * ----------------------------------
 * Extended stats: Print contents of latency histogram
 *
 */
int aow_lh_print(struct ieee80211com *ic)
{
    int i;

    AOW_LH_LOCK(ic);    
   
    if (NULL == ic->ic_aow.estats.aow_latency_stats) {
        IEEE80211_AOW_DPRINTF("-------------------------------\n");
        IEEE80211_AOW_DPRINTF("Latency Histogram not available\n");
        IEEE80211_AOW_DPRINTF("-------------------------------\n");
        return -1;
    }

    IEEE80211_AOW_DPRINTF("--------------------------------------------\n");
    IEEE80211_AOW_DPRINTF("\nLatency Histogram\n");
    IEEE80211_AOW_DPRINTF("--------------------------------------------\n");
        
    for (i = 0; i < ic->ic_aow.estats.aow_latency_max_bin; i++)
    {
        IEEE80211_AOW_DPRINTF("%-5u to %-5u usec : %u\n",
                              ATH_AOW_LATENCY_RANGE_SIZE * i,
                              ATH_AOW_LATENCY_RANGE_SIZE * (i + 1) - 1,
                              ic->ic_aow.estats.aow_latency_stats[i]);
    }
     
    IEEE80211_AOW_DPRINTF("%-5u usec and above: %u\n",
                          ATH_AOW_LATENCY_RANGE_SIZE * i,
                          ic->ic_aow.estats.aow_latency_stat_expired);

    AOW_LH_UNLOCK(ic);

    return 0;
}

/*
 * function : aow_lh_deinit
 * ----------------------------------
 * Extended stats: De-init latency histogram recording
 *
 */
void aow_lh_deinit(struct ieee80211com *ic)
{
    AOW_LH_LOCK(ic);

    if (ic->ic_aow.estats.aow_latency_stats != NULL)
    {    
        OS_FREE(ic->ic_aow.estats.aow_latency_stats);
        ic->ic_aow.estats.aow_latency_stats = NULL;
        ic->ic_aow.estats.aow_latency_stat_expired = 0;
    }

    AOW_LH_UNLOCK(ic);    
}

#if ATH_SUPPORT_AOW_ES_ADVNCD_TXDEBUG
static void aow_populate_advncd_txinfo(struct aow_advncd_txinfo *atxinfo,
                                       struct ath_tx_status *ts,
                                       struct ath_buf *bf,
                                       int loopcount)
{
    atxinfo->ts_tstamp = ts->ts_tstamp;
    atxinfo->ts_seqnum = ts->ts_seqnum;
    atxinfo->ts_status = ts->ts_status;
    atxinfo->ts_ratecode = ts->ts_ratecode;
    atxinfo->ts_rateindex = ts->ts_rateindex;
    atxinfo->ts_rssi = ts->ts_rssi;
    atxinfo->ts_shortretry = ts->ts_shortretry;
    atxinfo->ts_longretry = ts->ts_longretry;
    atxinfo->ts_virtcol = ts->ts_virtcol;
    atxinfo->ts_antenna = ts->ts_antenna;
    atxinfo->ts_flags = ts->ts_flags;
    atxinfo->ts_rssi_ctl0 = ts->ts_rssi_ctl0;
    atxinfo->ts_rssi_ctl1 = ts->ts_rssi_ctl1;
    atxinfo->ts_rssi_ctl2 = ts->ts_rssi_ctl2;
    atxinfo->ts_rssi_ext0 = ts->ts_rssi_ext0;
    atxinfo->ts_rssi_ext1 = ts->ts_rssi_ext1;
    atxinfo->ts_rssi_ext2 = ts->ts_rssi_ext2;
    atxinfo->queue_id = ts->queue_id;
    atxinfo->desc_id = ts->desc_id;
    atxinfo->ba_low = ts->ba_low;
    atxinfo->ba_high = ts->ba_high;
    atxinfo->evm0 = ts->evm0;
    atxinfo->evm1 = ts->evm1;
    atxinfo->evm2 = ts->evm2;
    atxinfo->ts_txbfstatus = ts->ts_txbfstatus;
    atxinfo->tid = ts->tid;

    atxinfo->bf_avail_buf = bf->bf_avail_buf;
    atxinfo->bf_status = bf->bf_status;
    atxinfo->bf_flags = bf->bf_flags;
    atxinfo->bf_reftxpower = bf->bf_reftxpower;
#if ATH_SUPPORT_IQUE && ATH_SUPPORT_IQUE_EXT
    atxinfo->bf_txduration = bf->bf_txduration;
#endif

    atxinfo->bfs_nframes = bf->bf_state.bfs_nframes;
    atxinfo->bfs_al = bf->bf_state.bfs_al;
    atxinfo->bfs_frmlen = bf->bf_state.bfs_frmlen;
    atxinfo->bfs_seqno = bf->bf_state.bfs_seqno;
    atxinfo->bfs_tidno = bf->bf_state.bfs_tidno;
    atxinfo->bfs_retries = bf->bf_state.bfs_retries;
    atxinfo->bfs_useminrate = bf->bf_state.bfs_useminrate;
    atxinfo->bfs_ismcast = bf->bf_state.bfs_ismcast;
    atxinfo->bfs_isdata = bf->bf_state.bfs_isdata;
    atxinfo->bfs_isaggr = bf->bf_state.bfs_isaggr;
    atxinfo->bfs_isampdu = bf->bf_state.bfs_isampdu;
    atxinfo->bfs_ht = bf->bf_state.bfs_ht;
    atxinfo->bfs_isretried = bf->bf_state.bfs_isretried;
    atxinfo->bfs_isxretried = bf->bf_state.bfs_isxretried;
    atxinfo->bfs_shpreamble = bf->bf_state.bfs_shpreamble;
    atxinfo->bfs_isbar = bf->bf_state.bfs_isbar;
    atxinfo->bfs_ispspoll = bf->bf_state.bfs_ispspoll;
    atxinfo->bfs_aggrburst = bf->bf_state.bfs_aggrburst;
    atxinfo->bfs_calcairtime = bf->bf_state.bfs_calcairtime;
#ifdef ATH_SUPPORT_UAPSD
    atxinfo->bfs_qosnulleosp = bf->bf_state.bfs_qosnulleosp;
#endif
    atxinfo->bfs_ispaprd = bf->bf_state.bfs_ispaprd;
    atxinfo->bfs_isswaborted = bf->bf_state.bfs_isswaborted;
#if ATH_SUPPORT_CFEND
    atxinfo->bfs_iscfend = bf->bf_state.bfs_iscfend;
#endif
#ifdef ATH_SWRETRY
    atxinfo->bfs_isswretry = bf->bf_state.bfs_isswretry;
    atxinfo->bfs_swretries = bf->bf_state.bfs_swretries;
    atxinfo->bfs_totaltries = bf->bf_state.bfs_totaltries;
#endif
    atxinfo->bfs_qnum = bf->bf_state.bfs_qnum;
    atxinfo->bfs_rifsburst_elem = bf->bf_state.bfs_rifsburst_elem;
    atxinfo->bfs_nrifsubframes = bf->bf_state.bfs_nrifsubframes;
    atxinfo->bfs_txbfstatus = bf->bf_state.bfs_txbfstatus;
    
    atxinfo->loopcount = loopcount;

}
#endif /* ATH_SUPPORT_AOW_ES_ADVNCD_TXDEBUG */

/* Operations to update Tx completion information */

/*
 * function : aow_istxsuccess
 * ----------------------------------
 * Find if transmission of MPDU was successful
 *
 */
static bool aow_istxsuccess(struct ieee80211com *ic,
                            struct ath_buf *bf,
                            struct ath_tx_status *ts,
                            bool is_aggr)
{
    u_int32_t ba_bitmap[AOW_WME_BA_BMP_SIZE >> 5];
    
    if (!is_aggr) {
        if (ts->ts_status == 0) {
            return true;
        } else {
            return false;
        }
    }
  
    /*TODO: Optimize the code to carry out the 8 byte bitmap creation once
            at a single point for an aggregate */

    if (ts->ts_status == 0 ) {
        if (ATH_AOW_DS_TX_BA(ts)) {
            OS_MEMCPY(ba_bitmap, ATH_AOW_DS_BA_BITMAP(ts), AOW_WME_BA_BMP_SIZE >> 3);
        } else {
            OS_MEMZERO(ba_bitmap, AOW_WME_BA_BMP_SIZE >> 3);
        }
    } else {
        OS_MEMZERO(ba_bitmap, AOW_WME_BA_BMP_SIZE >> 3);
    }

    if (ATH_AOW_BA_ISSET(ba_bitmap,
                         ATH_AOW_BA_INDEX(ATH_AOW_DS_BA_SEQ(ts),
                                          bf->bf_seqno))) {
        return true;
    } else {
        return false;
    }
}

/*
 * function : aow_update_mcsinfo
 * ----------------------------------
 * Extended stats: Update MCS information for
 * a given MPDU.
 *
 */
void aow_update_mcsinfo(struct ieee80211com *ic,
                        struct ath_buf *bf,
                        struct ath_tx_status *ts,
                        bool is_success,
                        int *num_attempts)
{

    int i = 0, j = 0;
    u_int8_t    rix;
    u_int8_t    tries;
    int         max_prefinal_index;
    int         lnum_attempts = 0;
    bool        is_recprefinal_enab = true;
    bool        is_recfinal_enab = true;
    /* Index into our AoW specific mcs_info array */
    u_int8_t    rateIndex;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    const HAL_RATE_TABLE *rt = sc->sc_rates[sc->sc_curmode];
    
    struct mcs_stats *mstats = &ic->ic_aow.estats.aow_mcs_stats;
 
    *num_attempts = 0; 

    max_prefinal_index = ts->ts_rateindex - 1;

    for (i = 0; i <= max_prefinal_index; i++)
    {
        rix = bf->bf_rcs[i].rix;
        tries =  bf->bf_rcs[i].tries;
        
        rateIndex = rt->info[rix].rateCode - AOW_ATH_MCS0;

        if (rateIndex > (ATH_AOW_NUM_MCS - 1) || tries > ATH_AOW_TXMAXTRY) {
            is_recprefinal_enab = false;
        }
        
        for (j = 0; j < tries; j++)
        {
            if (is_recprefinal_enab) {
                mstats->mcs_info[rateIndex].nok_txcount[j]++;
            }
            lnum_attempts++;
        }
    }
    
    rix = bf->bf_rcs[ts->ts_rateindex].rix;
    tries =  bf->bf_rcs[ts->ts_rateindex].tries;
        
    rateIndex = rt->info[rix].rateCode - AOW_ATH_MCS0;

    if (rateIndex > (ATH_AOW_NUM_MCS - 1) || tries > ATH_AOW_TXMAXTRY) {
        is_recfinal_enab = false;        
        return;
    }
        
    for (j = 0; j < ts->ts_longretry; j++)
    {
        if (is_recfinal_enab) {
            mstats->mcs_info[rateIndex].nok_txcount[j]++;
        }
        lnum_attempts++;
    }
    
    if (!is_success) {
        if (ts->ts_status == 0 && bf->bf_isaggr) {
            /* The MPDU was part of an aggregate transmission which succeeded
               Increment failure count for this unlucky MPDU alone. */
             if (is_recfinal_enab) {
                mstats->mcs_info[rateIndex].nok_txcount[j]++;
            }
            lnum_attempts++;
        }
        
        *num_attempts = lnum_attempts;

        return;
    }

    if (is_recfinal_enab) {
        mstats->mcs_info[rateIndex].ok_txcount[j]++;
    }
    lnum_attempts++;
    *num_attempts = lnum_attempts;
}

/*
 * function : aow_update_txstatus
 * ----------------------------------
 * Extended stats: Update Tx status
 *
 */
void aow_update_txstatus(struct ieee80211com *ic, struct ath_buf *bf, struct ath_tx_status *ts)
{
    u_int16_t txstatus;
    wbuf_t wbuf;
    struct audio_pkt *apkt;
    struct ether_addr recvr_addr;
    struct ath_buf *bfl;
    bool is_success = false;
    bool is_aggr = false;
    int num_attempts = 0;
#if ATH_SUPPORT_AOW_ES_ADVNCD_TXDEBUG
    struct aow_advncd_txinfo atxinfo;
    int loopcount = 0;
#endif

    if (ic != aowinfo.ic ||
        (!AOW_ES_ENAB(ic) && !AOW_ESS_ENAB(ic))) {
        return;
    }

    is_aggr = bf->bf_isaggr;
   
    bfl = bf;

    while (bfl != NULL)
    {
        wbuf = (wbuf_t)bfl->bf_mpdu;

        apkt = ATH_AOW_ACCESS_APKT(wbuf);

        if (apkt->signature != ATH_AOW_SIGNATURE) {
            return;
        }
        
#if ATH_SUPPORT_AOW_ES_ADVNCD_TXDEBUG
        loopcount++;
#endif
        if (!AOW_ES_ENAB(ic) && AOW_ESS_ENAB(ic)
            &&  !AOW_ESS_SYNC_SET(apkt->params)) {
            bfl =  bfl->bf_next;
            continue;
        }

        is_success = aow_istxsuccess(ic, bfl, ts, is_aggr);

        /* We pass bf instead of bfl because there is a chance
           that the rate series array for bfl would not be valid. */
        aow_update_mcsinfo(ic, bf, ts, is_success, &num_attempts);

        memcpy(&recvr_addr,
               ATH_AOW_ACCESS_RECVR(wbuf),
               sizeof(struct ether_addr)); 

        txstatus =
            is_success ? ATH_AOW_PL_INFO_TX_SUCCESS : ATH_AOW_PL_INFO_TX_FAIL;

#if ATH_SUPPORT_AOW_ES_ADVNCD_TXDEBUG
           aow_populate_advncd_txinfo(&atxinfo, ts, bfl, loopcount);
#endif

           aow_nl_send_txpl(ic,
                         recvr_addr,
                         apkt->seqno,
                         apkt->params & ATH_AOW_PARAMS_SUB_FRAME_NO_MASK,
                         ATH_AOW_ACCESS_WLANSEQ(wbuf),
                         txstatus,
#if ATH_SUPPORT_AOW_ES_ADVNCD_TXDEBUG
                         &atxinfo,
#endif
                         num_attempts);

        bfl =  bfl->bf_next;
    }
}

#if LINUX_VERSION_CODE == AOW_LINUX_VERSION

/* Application Messaging (AM) Services. Netlink is used. */

/*
 * function : aow_am_init
 * ----------------------------------
 * Application Messaging: Initialize Netlink operation.
 *
 */
int aow_am_init(struct ieee80211com *ic)
{
    if (ic->ic_aow.aow_nl_sock == NULL) {
        ic->ic_aow.aow_nl_sock = 
            (struct sock *) OS_NETLINK_CREATE(NETLINK_ATHEROS,
                                              1,
                                              NULL,
                                              THIS_MODULE);

        if (ic->ic_aow.aow_nl_sock == NULL) {
            IEEE80211_AOW_DPRINTF("OS_NETLINK_CREATE() failed\n");
            return -ENODEV;
        }
    }
    return 0;
}

/*
 * function : aow_am_deinit
 * ----------------------------------
 * Application Messaging: De-Initialize Netlink operation.
 *
 */
void aow_am_deinit(struct ieee80211com *ic)
{
    if (ic->ic_aow.aow_nl_sock) {
            OS_SOCKET_RELEASE(ic->ic_aow.aow_nl_sock);
            ic->ic_aow.aow_nl_sock = NULL;
    }
}

/*
 * function : aow_am_send
 * ----------------------------------
 * Application Messaging: Send data using netlink.
 *
 */
int aow_am_send(struct ieee80211com *ic, char *data, int size)
{
    wbuf_t  nlwbuf;
    void    *nlh;
    
    if (ic->ic_aow.aow_nl_sock == NULL) {
        return -1;
    }
   
    nlwbuf = wbuf_alloc(ic->ic_osdev,
                        WBUF_MAX_TYPE,
                        OS_NLMSG_SPACE(AOW_NL_DATA_MAX_SIZE));

    if (nlwbuf != NULL) {
        wbuf_append(nlwbuf, OS_NLMSG_SPACE(AOW_NL_DATA_MAX_SIZE));
    } else {
        IEEE80211_AOW_DPRINTF("%s: wbuf_alloc() failed\n", __func__);
        return -1;
    }
    
    nlh = wbuf_raw_data(nlwbuf);
    OS_SET_NETLINK_HEADER(nlh, NLMSG_LENGTH(size), 0, 0, 0, 0);

    /* sender is in group 1<<0 */
    wbuf_set_netlink_pid(nlwbuf, 0);      /* from kernel */
    wbuf_set_netlink_dst_pid(nlwbuf, 0);  /* multicast */ 
    /* to mcast group 1<<0 */
    wbuf_set_netlink_dst_group(nlwbuf, 1);
    
    OS_MEMCPY(OS_NLMSG_DATA(nlh), data, OS_NLMSG_SPACE(size));

    OS_NETLINK_BCAST(ic->ic_aow.aow_nl_sock, nlwbuf, 0, 1, GFP_ATOMIC);

    return 0;
}

#endif /* LINUX_VERSION_CODE == AOW_LINUX_VERSION */

/*
 * function : aow_nl_send_rxpl
 * ----------------------------------
 * Extended stats: Send a pl_element entry (Rx) to the application
 * layer via Netlink socket.
 *
 */
int aow_nl_send_rxpl(struct ieee80211com *ic,
                     u_int32_t seqno,
                     u_int8_t subfrme_seqno,
                     u_int16_t wlan_seqno,
#if ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG
                     u_int32_t latency,
#endif
                     u_int8_t rxstatus)
{
    struct aow_nl_packet nlpkt;

    if (!AOW_ES_ENAB(ic) && !AOW_ESS_ENAB(ic)) {
        return -EINVAL;
    }

    ic->ic_aow.node_type = ATH_AOW_NODE_TYPE_RX;
    
    nlpkt.header = ATH_AOW_NODE_TYPE_RX & ATH_AOW_NL_TYPE_MASK;
    
    nlpkt.body.rxdata.elmnt.seqno = seqno;
    
    nlpkt.body.rxdata.elmnt.subfrme_wlan_seqnos = 
        subfrme_seqno & ATH_AOW_PL_SUBFRME_SEQ_MASK; 
    
    nlpkt.body.rxdata.elmnt.subfrme_wlan_seqnos |=
        ((wlan_seqno & ATH_AOW_PL_WLAN_SEQ_MASK) << ATH_AOW_PL_WLAN_SEQ_S);
    
#if ATH_SUPPORT_AOW_ES_ADVNCD_RXDEBUG
    nlpkt.body.rxdata.elmnt.arxinfo.latency = latency;
#endif

    nlpkt.body.rxdata.elmnt.info = rxstatus;
    
    aow_am_send(ic,
                (char*)&nlpkt,
                AOW_NL_PACKET_RX_SIZE);
    
    return 0;
}

/*
 * function : aow_nl_send_txpl
 * ----------------------------------
 * Extended stats: Send a pl_element entry (Tx) to the application
 * layer via Netlink socket.
 *
 */
int aow_nl_send_txpl(struct ieee80211com *ic,
                     struct ether_addr recvr_addr,
                     u_int32_t seqno,
                     u_int8_t subfrme_seqno,
                     u_int16_t wlan_seqno,
                     u_int8_t txstatus,
#if ATH_SUPPORT_AOW_ES_ADVNCD_TXDEBUG
                     struct aow_advncd_txinfo *atxinfo,
#endif
                     u_int8_t numretries)
{
    struct aow_nl_packet nlpkt;

    if (!AOW_ES_ENAB(ic) && !AOW_ESS_ENAB(ic)) {
        return -EINVAL;
    }

    ic->ic_aow.node_type = ATH_AOW_NODE_TYPE_TX;

    nlpkt.header = ATH_AOW_NODE_TYPE_TX & ATH_AOW_NL_TYPE_MASK;
    
    nlpkt.body.txdata.elmnt.seqno               = seqno;
    
    nlpkt.body.txdata.elmnt.subfrme_wlan_seqnos =
        subfrme_seqno & ATH_AOW_PL_SUBFRME_SEQ_MASK; 
    
    nlpkt.body.txdata.elmnt.subfrme_wlan_seqnos |=
        ((wlan_seqno & ATH_AOW_PL_WLAN_SEQ_MASK) << ATH_AOW_PL_WLAN_SEQ_S);

    nlpkt.body.txdata.elmnt.info                =
        numretries & ATH_AOW_PL_NUM_ATTMPTS_MASK;

    nlpkt.body.txdata.elmnt.info                |=
        (txstatus & ATH_AOW_PL_TX_STATUS_MASK) << ATH_AOW_PL_TX_STATUS_S;

#if ATH_SUPPORT_AOW_ES_ADVNCD_TXDEBUG
    OS_MEMCPY(&nlpkt.body.txdata.elmnt.atxinfo,
              atxinfo,
              sizeof(struct aow_advncd_txinfo));
#endif

    OS_MEMCPY(&nlpkt.body.txdata.recvr, &recvr_addr, sizeof(recvr_addr));

    aow_am_send(ic,
                (char*)&nlpkt,
                AOW_NL_PACKET_TX_SIZE);
    
    return 0;
}

/* Operations on ESS Counts */

/*
 * function : aow_essc_init
 * ----------------------------------
 * Extended stats (Synchronized): Initialize ESS Count Maintenance
 *
 */
void aow_essc_init(struct ieee80211com *ic)
{
   AOW_ESSC_LOCK(ic);
   
   memset(ic->ic_aow.ess_count, 0, sizeof(ic->ic_aow.ess_count)); 

   AOW_ESSC_UNLOCK(ic);
}

/*
 * function : aow_essc_setall
 * ----------------------------------
 * Extended stats (Synchronized): Set ESS Count for all channels
 *
 */

void aow_essc_setall(struct ieee80211com *ic, u_int32_t val)
{
    int i;

    if (!AOW_ESS_ENAB(ic)) {
        return;
    }

    AOW_ESSC_LOCK(ic);
    
    for (i = 0; i < AOW_MAX_AUDIO_CHANNELS; i++)
    {
        ic->ic_aow.ess_count[i] = val;
    }


    AOW_ESSC_UNLOCK(ic);
}

/*
 * function : aow_essc_testdecr
 * ----------------------------------
 * Extended stats (Synchronized): Test and decrement ESS Count for a given
 * channel.
 * - If the count is non-zero, then it is decremented and true is returned.
 * - If the count is zero or ESS is not enabled, then false is returned.
 */
bool aow_essc_testdecr(struct ieee80211com *ic, int channel)
{
    if (!AOW_ESS_ENAB(ic)) {
        return false;
    }

    AOW_ESSC_LOCK(ic);
   
    if (ic->ic_aow.ess_count[channel] == 0) {
        return false;
        AOW_ESSC_UNLOCK(ic);
    } else {
        ic->ic_aow.ess_count[channel]--;
        AOW_ESSC_UNLOCK(ic);
        return true;
    }

    AOW_ESSC_UNLOCK(ic);
}

/*
 * function : aow_essc_deinit
 * ----------------------------------
 * Extended stats (Synchronized): De-Initialize ESS Count Maintenance
 *
 */
void aow_essc_deinit(struct ieee80211com *ic)
{
    AOW_ESSC_LOCK(ic);
   
    memset(ic->ic_aow.ess_count, 0, sizeof(ic->ic_aow.ess_count)); 

    AOW_ESSC_UNLOCK(ic);
}

/* Operations on L2 Packet Error Stats */

/*
 * function: aow_l2pe_init()
 * ---------------------------------
 * Extended stats: Initialize L2 Packet Error Stats measurement.
 *
 */
int aow_l2pe_init(struct ieee80211com *ic)
{
    int retval;

    AOW_L2PE_LOCK(ic);    
    
    if ((retval = aow_l2pe_fifo_init(ic)) < 0) {
       AOW_L2PE_UNLOCK(ic);    
       return retval;
    } 

    OS_MEMSET(&ic->ic_aow.estats.l2pe_curr,
              0,
              sizeof(ic->ic_aow.estats.l2pe_curr));
    
    OS_MEMSET(&ic->ic_aow.estats.l2pe_cumltv,
              0,
              sizeof(ic->ic_aow.estats.l2pe_cumltv));

    ic->ic_aow.estats.l2pe_next_srNo = 1;

    ic->ic_aow.estats.l2pe_curr.isInUse = false;
    ic->ic_aow.estats.l2pe_curr.wasAudStopped = false;

    ic->ic_aow.estats.l2pe_cumltv.start_time =
        OS_GET_TIMESTAMP();
    
    AOW_L2PE_UNLOCK(ic);    
    
    return 0;
}

/*
 * function: aow_l2pe_deinit()
 * ---------------------------------
 * Extended stats: De-Initialize L2 Packet Error Stats measurement.
 *
 */
void aow_l2pe_deinit(struct ieee80211com *ic)
{
    AOW_L2PE_LOCK(ic);    
    
    aow_l2pe_fifo_deinit(ic);

    OS_MEMSET(&ic->ic_aow.estats.l2pe_curr,
              0,
              sizeof(ic->ic_aow.estats.l2pe_curr));
    
    OS_MEMSET(&ic->ic_aow.estats.l2pe_cumltv,
              0,
              sizeof(ic->ic_aow.estats.l2pe_cumltv));

    ic->ic_aow.estats.l2pe_next_srNo = 0;
    
    ic->ic_aow.estats.l2pe_curr.isInUse = false;
    ic->ic_aow.estats.l2pe_curr.wasAudStopped = false;
    
    AOW_L2PE_UNLOCK(ic);    
}


/*
 * function: aow_l2pe_reset()
 * ---------------------------------
 * Extended stats: Re-Set L2 Packet Error Stats measurement.
 *
 */
void aow_l2pe_reset(struct ieee80211com *ic)
{
    AOW_L2PE_LOCK(ic);    
    
    aow_l2pe_fifo_reset(ic);
    
    OS_MEMSET(&ic->ic_aow.estats.l2pe_curr,
              0,
              sizeof(ic->ic_aow.estats.l2pe_curr));
    
    OS_MEMSET(&ic->ic_aow.estats.l2pe_cumltv,
              0,
              sizeof(ic->ic_aow.estats.l2pe_cumltv));

    ic->ic_aow.estats.l2pe_next_srNo = 1;
    
    ic->ic_aow.estats.l2pe_curr.isInUse = false;
    ic->ic_aow.estats.l2pe_curr.wasAudStopped = false;
    
    AOW_L2PE_UNLOCK(ic);    

}

/*
 * function: aow_l2pe_audstopped()
 * ---------------------------------
 * Extended stats: Inform the L2 Packet Error Stats measurement
 * module that audio stop was detected.
 *
 */
void aow_l2pe_audstopped(struct ieee80211com *ic)
{
    ic->ic_aow.estats.l2pe_curr.wasAudStopped = true;
}

/*
 * function: aow_l2pe_record()
 * ---------------------------------
 * Extended stats: Record an L2 Packet Error/Success case.
 *
 */
int aow_l2pe_record(struct ieee80211com *ic, bool is_success)
{
    systime_t curr_time;
    systime_t prev_start_time; 

    if (NULL == ic->ic_aow.estats.aow_l2pe_stats) {
        return -EINVAL;
    }
    
    AOW_L2PE_LOCK(ic); 
    
    curr_time = OS_GET_TIMESTAMP();

    prev_start_time = ic->ic_aow.estats.l2pe_curr.start_time;

    if (!ic->ic_aow.estats.l2pe_curr.isInUse) {
        aow_l2peinst_init(&ic->ic_aow.estats.l2pe_curr,
                          ic->ic_aow.estats.l2pe_next_srNo++,
                          curr_time);

    } else if (ic->ic_aow.estats.l2pe_curr.wasAudStopped) { 
        
        aow_l2pe_fifo_enqueue(ic, &ic->ic_aow.estats.l2pe_curr);

        aow_l2peinst_init(&ic->ic_aow.estats.l2pe_curr,
                          ic->ic_aow.estats.l2pe_next_srNo++,
                          curr_time);

    } else if (curr_time - prev_start_time >= ATH_AOW_ES_GROUP_INTERVAL_MS) {
        aow_l2pe_fifo_enqueue(ic, &ic->ic_aow.estats.l2pe_curr);
        aow_l2peinst_init(&ic->ic_aow.estats.l2pe_curr,
                          ic->ic_aow.estats.l2pe_next_srNo++,
                          prev_start_time + ATH_AOW_ES_GROUP_INTERVAL_MS);
    }
    
    if (is_success) {
        ic->ic_aow.estats.l2pe_curr.fcs_ok++;
        ic->ic_aow.estats.l2pe_cumltv.fcs_ok++;
    } else {
        ic->ic_aow.estats.l2pe_curr.fcs_nok++;
        ic->ic_aow.estats.l2pe_cumltv.fcs_nok++;
    
    }
    
    ic->ic_aow.estats.l2pe_cumltv.end_time = curr_time;
    
    AOW_L2PE_UNLOCK(ic);    

    return 0;
}

/*
 * function: aow_l2pe_print_inst()
 * ---------------------------------
 * Extended stats: Print L2 Packet Error Rate statistics for 
 * an L2PE group instance.
 *
 */
static inline int aow_l2pe_print_inst(struct ieee80211com *ic,
                                      struct l2_pkt_err_stats *l2pe_inst)
{
    u_int32_t ratio_int_part;
    u_int32_t ratio_frac_part;
    int retval;
    
    IEEE80211_AOW_DPRINTF("No. of MPDUs with FCS passed = %u\n",
                          l2pe_inst->fcs_ok);
    IEEE80211_AOW_DPRINTF("No. of MPDUs with FCS failed = %u\n",
                          l2pe_inst->fcs_nok);

    if (0 == l2pe_inst->fcs_ok && 0 == l2pe_inst->fcs_nok)
    {
       IEEE80211_AOW_DPRINTF("L2 Packet Error Rate : NA\n");

    } else {

        if ((retval = uint_floatpt_division(l2pe_inst->fcs_nok,
                                            l2pe_inst->fcs_ok + l2pe_inst->fcs_nok,
                                            &ratio_int_part,
                                            &ratio_frac_part,
                                            ATH_AOW_ES_PRECISION_MULT)) < 0) {
            return retval;
        }
    
       IEEE80211_AOW_DPRINTF("L2 Packet Error Rate = %u.%0*u\n",
                             ratio_int_part,
                             ATH_AOW_ES_PRECISION,
                             ratio_frac_part);
    }

    return 0;
}

/*
 * function: aow_l2pe_print()
 * ---------------------------------
 * Extended stats: Print L2 Packet Error Rate statistics.
 *
 */
int aow_l2pe_print(struct ieee80211com *ic)
{
    struct l2_pkt_err_stats l2pe_stats[ATH_AOW_MAX_ES_GROUPS - 1];
    int i = 0;
    int count = 0;
    int retval = 0;

    AOW_L2PE_LOCK(ic);
    
    if (NULL == ic->ic_aow.estats.aow_l2pe_stats) {
        IEEE80211_AOW_DPRINTF("\n------------------------------------------\n");
        IEEE80211_AOW_DPRINTF("\nL2 Packet Error Statistics not available\n");
        IEEE80211_AOW_DPRINTF("------------------------------------------\n");
        AOW_L2PE_UNLOCK(ic);
        return -1;
    }

    IEEE80211_AOW_DPRINTF("\n--------------------------------------------\n");
    IEEE80211_AOW_DPRINTF("\nL2 Packet Error Statistics\n");
    IEEE80211_AOW_DPRINTF("--------------------------------------------\n");
        
    retval = aow_l2pe_fifo_copy(ic, l2pe_stats, &count);

    if (retval >= 0 || ic->ic_aow.estats.l2pe_curr.isInUse) {
        
        IEEE80211_AOW_DPRINTF("\nStats at %ds time intervals"
                          " (from oldest to latest):\n\n",
                          ATH_AOW_ES_GROUP_INTERVAL);

        if (retval >= 0)
        {
            for (i = 0; i < count; i++) {
                IEEE80211_AOW_DPRINTF("Sr. No. %u\n", l2pe_stats[i].srNo);
                aow_l2pe_print_inst(ic, &l2pe_stats[i]);
                IEEE80211_AOW_DPRINTF("\n");
            }
        }

        if (ic->ic_aow.estats.l2pe_curr.isInUse) {
            IEEE80211_AOW_DPRINTF("Sr. No. %d\n",
                                  ic->ic_aow.estats.l2pe_curr.srNo);
            aow_l2pe_print_inst(ic, &ic->ic_aow.estats.l2pe_curr);
            IEEE80211_AOW_DPRINTF("\n");
        }
    }

    IEEE80211_AOW_DPRINTF("Cumulative Stats:\n");
    aow_l2pe_print_inst(ic, &ic->ic_aow.estats.l2pe_cumltv);
    IEEE80211_AOW_DPRINTF("\n");
    
    AOW_L2PE_UNLOCK(ic);

    return 0; 
}
 
/* Operations on FIFO for L2 Packet Error Stats */
/* The FIFO will contain (ATH_AOW_MAX_ES_GROUPS - 1) entries */

/* TODO: Low priority: Create a common FIFO framework for 
   L2 Packet Errors and Packet Loss Stats */

/*
 * function: aow_l2pe_fifo_init()
 * ---------------------------------
 * Extended stats: Initialize FIFO for L2 Packet Error Stats.
 *
 * This function is to be called ONLY by the aow_l2pe*() functions.
 *
 */
static int aow_l2pe_fifo_init(struct ieee80211com *ic)
{
   ic->ic_aow.estats.aow_l2pe_stats =
       (struct l2_pkt_err_stats *)OS_MALLOC(ic->ic_osdev,
                                            ATH_AOW_MAX_ES_GROUPS *\
                                               sizeof(struct l2_pkt_err_stats),
                                            GFP_KERNEL);
    
   if (NULL == ic->ic_aow.estats.aow_l2pe_stats)
   {
        IEEE80211_AOW_DPRINTF("%s: Could not allocate memory\n", __func__);
        return -ENOMEM;
   }

   OS_MEMSET(ic->ic_aow.estats.aow_l2pe_stats,
             0,
             ATH_AOW_MAX_ES_GROUPS * sizeof(struct l2_pkt_err_stats));

   ic->ic_aow.estats.aow_l2pe_stats_write = 0;
   ic->ic_aow.estats.aow_l2pe_stats_read = 0;

   return 0; 
}

/*
 * function: aow_l2pe_fifo_reset()
 * ---------------------------------
 * Extended stats: Reset FIFO for L2 Packet Error Stats.
 *
 * This function is to be called ONLY by the aow_l2pe*() functions.
 *
 */
static int aow_l2pe_fifo_reset(struct ieee80211com *ic)
{
    if (NULL == ic->ic_aow.estats.aow_l2pe_stats) {
        return -EINVAL;
    }
    
    ic->ic_aow.estats.aow_l2pe_stats_write = 0;
    ic->ic_aow.estats.aow_l2pe_stats_read = 0;
    
    OS_MEMSET(ic->ic_aow.estats.aow_l2pe_stats,
             0,
             ATH_AOW_MAX_ES_GROUPS * sizeof(struct l2_pkt_err_stats));

    return 0;
}

/*
 * function: aow_l2pe_fifo_enqueue()
 * ---------------------------------
 * Extended stats: Enqueue an L2 Packet Error Stats Group instance into
 * into the FIFO.
 *
 * This function is to be called ONLY by the aow_l2pe*() functions.
 */
static int aow_l2pe_fifo_enqueue(struct ieee80211com *ic,
                                 struct l2_pkt_err_stats *l2pe_instnce)
{
    int aow_l2pe_stats_write;

    if (NULL == ic->ic_aow.estats.aow_l2pe_stats) {
        return -EINVAL;
    }

    aow_l2pe_stats_write = ic->ic_aow.estats.aow_l2pe_stats_write;
  
    /* We have the design option of directly storing pointers into the FIFO, or
       querying the FIFO for the pointer for writing information.
       However, the OS_MEMCPY() involves very few bytes, and that too, mostly
       every ATH_AOW_ES_GROUP_INTERVAL seconds, which is very large.
       Hence, we adopt a cleaner approach. */

    OS_MEMCPY(&ic->ic_aow.estats.aow_l2pe_stats[aow_l2pe_stats_write],
              l2pe_instnce,
              sizeof(struct l2_pkt_err_stats));

    ic->ic_aow.estats.aow_l2pe_stats_write++;
    if (ic->ic_aow.estats.aow_l2pe_stats_write == ATH_AOW_MAX_ES_GROUPS) {
        ic->ic_aow.estats.aow_l2pe_stats_write = 0;
    }
    
    if (ic->ic_aow.estats.aow_l2pe_stats_write == ic->ic_aow.estats.aow_l2pe_stats_read) {
        ic->ic_aow.estats.aow_l2pe_stats_read++;
        if (ic->ic_aow.estats.aow_l2pe_stats_read == ATH_AOW_MAX_ES_GROUPS) {
            ic->ic_aow.estats.aow_l2pe_stats_read = 0;
        }
    }

    return 0;
}

/*
 * function : aow_l2pe_fifo_copy()
 * ----------------------------------
 * Extended stats: Copy contents of L2 Packet Error Stats FIFO into an
 * array, in increasing order of age.
 * The FIFO contents are not deleted.
 * -EOVERFLOW is returned if the FIFO is empty.
 * (Note: EOVERFLOW is the closest OS independant error definition that seems
 * to be currently available).
 *
 * This function is to be called ONLY by the aow_l2pe*() functions.
 *
 */
static int aow_l2pe_fifo_copy(struct ieee80211com *ic,
                              struct l2_pkt_err_stats *aow_l2pe_stats,
                              int *count)
{
    int writepos = ic->ic_aow.estats.aow_l2pe_stats_write;
    int readpos = ic->ic_aow.estats.aow_l2pe_stats_read;
    
    int ph1copy_start = -1; /* Starting position for phase 1 copy */
    int ph1copy_count = 0;  /* Byte count for phase 1 copy */
    int ph2copy_start = -1; /* Starting position for phase 2 copy */
    int ph2copy_count = 0;  /* Byte count for phase 2 copy */

    if (writepos == readpos) {
        return -EOVERFLOW;
    }

    *count = 0;
    
    ph1copy_start = readpos;
    
    if (readpos < writepos) {
        /* writepos itself contains no valid data */
        ph1copy_count = (writepos - readpos);
        /* Phase 2 copy not required */
        ph2copy_count = 0;
    } else { 
        ph1copy_count = ATH_AOW_MAX_ES_GROUPS - readpos;
        if (writepos == 0) {
            ph2copy_count = 0;
        } else {
            ph2copy_start = 0;
            ph2copy_count = writepos;
        }
    }

    OS_MEMCPY(aow_l2pe_stats,
              ic->ic_aow.estats.aow_l2pe_stats + ph1copy_start,
              ph1copy_count * sizeof(struct l2_pkt_err_stats));

    if (ph2copy_count > 0)
    {
        OS_MEMCPY(aow_l2pe_stats + ph1copy_count,
                  ic->ic_aow.estats.aow_l2pe_stats + ph2copy_start,
                  ph2copy_count * sizeof(struct l2_pkt_err_stats));
    }
    
    *count = ph1copy_count + ph2copy_count;

    return 0;
}

/*
 * function: aow_l2pe_fifo_deinit()
 * ---------------------------------
 * Extended stats: De-Initialize FIFO for L2 Packet Error Stats.
 *
 * This function is to be called ONLY by the aow_l2pe*() functions.
 *
 */
static void aow_l2pe_fifo_deinit(struct ieee80211com *ic)
{
    if (ic->ic_aow.estats.aow_l2pe_stats != NULL)
    {    
        OS_FREE(ic->ic_aow.estats.aow_l2pe_stats);
        ic->ic_aow.estats.aow_l2pe_stats = NULL;
        ic->ic_aow.estats.aow_l2pe_stats_write = 0;
        ic->ic_aow.estats.aow_l2pe_stats_read = 0;
    }
}

/*
 * function: aow_l2peinst_init()
 * ---------------------------------
 * Extended stats: Initialize an L2 Packet Error Stats Group instance.
 *
 * This function is to be called ONLY by the aow_l2pe*() functions.
 *
 */
static inline void aow_l2peinst_init(struct l2_pkt_err_stats *l2pe_inst,
                                     u_int32_t srNo,
                                     systime_t start_time)
{
    l2pe_inst->srNo = srNo;
    l2pe_inst->start_time = start_time;
    l2pe_inst->fcs_ok = 0;
    l2pe_inst->fcs_nok = 0;
    l2pe_inst->isInUse = true;
    l2pe_inst->wasAudStopped = false;
}

/* Operations on Packet Lost Rate Records */

/*
 * function: aow_plr_init()
 * ---------------------------------
 * Extended stats: Initialize Packet Lost Rate measurement.
 *
 */
int aow_plr_init(struct ieee80211com *ic)
{
    int retval;

    AOW_PLR_LOCK(ic);    
    
    if ((retval = aow_plr_fifo_init(ic)) < 0) {
       AOW_PLR_UNLOCK(ic);    
       return retval;
    } 

    OS_MEMSET(&ic->ic_aow.estats.plr_curr,
              0,
              sizeof(ic->ic_aow.estats.plr_curr));
    
    OS_MEMSET(&ic->ic_aow.estats.plr_cumltv,
              0,
              sizeof(ic->ic_aow.estats.plr_cumltv));

    ic->ic_aow.estats.plr_next_srNo = 1;

    ic->ic_aow.estats.plr_curr.isInUse = false;
    ic->ic_aow.estats.plr_curr.wasAudStopped = false;

    ic->ic_aow.estats.plr_cumltv.isInUse = false;
    
    ic->ic_aow.estats.plr_cumltv.start_time =
        OS_GET_TIMESTAMP();
    
    AOW_PLR_UNLOCK(ic);    
    
    return 0;
}

/*
 * function: aow_plr_deinit()
 * ---------------------------------
 * Extended stats: De-Initialize Packet Lost Rate measurement.
 *
 */
void aow_plr_deinit(struct ieee80211com *ic)
{
    AOW_PLR_LOCK(ic);    
    
    aow_plr_fifo_deinit(ic);

    OS_MEMSET(&ic->ic_aow.estats.plr_curr,
              0,
              sizeof(ic->ic_aow.estats.plr_curr));
    
    OS_MEMSET(&ic->ic_aow.estats.plr_cumltv,
              0,
              sizeof(ic->ic_aow.estats.plr_cumltv));

    ic->ic_aow.estats.plr_next_srNo = 0;
    
    ic->ic_aow.estats.plr_curr.isInUse = false;
    ic->ic_aow.estats.plr_curr.wasAudStopped = false;
    
    ic->ic_aow.estats.plr_cumltv.isInUse = false;
    
    AOW_PLR_UNLOCK(ic);    
}


/*
 * function: aow_plr_reset()
 * ---------------------------------
 * Extended stats: Re-Set Packet Lost Rate measurement.
 *
 */
void aow_plr_reset(struct ieee80211com *ic)
{
    AOW_PLR_LOCK(ic);    
    
    aow_plr_fifo_reset(ic);
    
    OS_MEMSET(&ic->ic_aow.estats.plr_curr,
              0,
              sizeof(ic->ic_aow.estats.plr_curr));
    
    OS_MEMSET(&ic->ic_aow.estats.plr_cumltv,
              0,
              sizeof(ic->ic_aow.estats.plr_cumltv));

    ic->ic_aow.estats.plr_next_srNo = 1;
    
    ic->ic_aow.estats.plr_curr.isInUse = false;
    ic->ic_aow.estats.plr_curr.wasAudStopped = false;
    
    ic->ic_aow.estats.plr_cumltv.isInUse = false;
    
    AOW_PLR_UNLOCK(ic);    

}

/*
 * function: aow_plr_audstopped()
 * ---------------------------------
 * Extended stats: Inform the Packet Lost Rate measurement
 * module that audio stop was detected.
 *
 */
void aow_plr_audstopped(struct ieee80211com *ic)
{
    ic->ic_aow.estats.plr_curr.wasAudStopped = true;
}

/*
 * function: aow_plr_record()
 * ---------------------------------
 * Extended stats: Record an increment in a Packet Lost Rate related
 * statistic.
 *
 */
int aow_plr_record(struct ieee80211com *ic,
                   u_int32_t seqno,
                   u_int16_t subseqno,
                   u_int8_t type)
{
    systime_t curr_time;
    systime_t prev_start_time;

    if (NULL == ic->ic_aow.estats.aow_plr_stats) {
        return -EINVAL;
    }
    
    AOW_PLR_LOCK(ic); 
    
    curr_time = OS_GET_TIMESTAMP();

    prev_start_time = ic->ic_aow.estats.plr_curr.start_time;

    if (!ic->ic_aow.estats.plr_cumltv.isInUse) {
        aow_plrinst_init(&ic->ic_aow.estats.plr_cumltv,
                          0,
                          seqno,
                          subseqno,
                          ic->ic_aow.estats.plr_cumltv.start_time);
    }

    if (!ic->ic_aow.estats.plr_curr.isInUse) {
        aow_plrinst_init(&ic->ic_aow.estats.plr_curr,
                          ic->ic_aow.estats.plr_next_srNo++,
                          seqno,
                          subseqno,
                          curr_time);

    } else if (ic->ic_aow.estats.plr_curr.wasAudStopped) { 
        
        aow_plr_fifo_enqueue(ic, &ic->ic_aow.estats.plr_curr);

        aow_plrinst_init(&ic->ic_aow.estats.plr_curr,
                          ic->ic_aow.estats.plr_next_srNo++,
                          seqno,
                          subseqno,
                          curr_time);

    } else if (curr_time - prev_start_time >= ATH_AOW_ES_GROUP_INTERVAL_MS) {
        aow_plr_fifo_enqueue(ic, &ic->ic_aow.estats.plr_curr);
        aow_plrinst_init(&ic->ic_aow.estats.plr_curr,
                          ic->ic_aow.estats.plr_next_srNo++,
                          seqno,
                          subseqno,
                          prev_start_time + ATH_AOW_ES_GROUP_INTERVAL_MS);
    }
   
    
    switch(type)
    {
        case ATH_AOW_PLR_TYPE_NUM_MPDUS:
            ic->ic_aow.estats.plr_curr.num_mpdus++;
            ic->ic_aow.estats.plr_cumltv.num_mpdus++;
            break;

        case ATH_AOW_PLR_TYPE_NOK_FRMCNT:
            ic->ic_aow.estats.plr_curr.nok_framecount++;
            ic->ic_aow.estats.plr_cumltv.nok_framecount++;
            break;

        case ATH_AOW_PLR_TYPE_LATE_MPDU:
            ic->ic_aow.estats.plr_curr.late_mpdu++;
            ic->ic_aow.estats.plr_cumltv.late_mpdu++;
            break;
    }
    
    
    ic->ic_aow.estats.plr_curr.end_seqno = seqno;
    ic->ic_aow.estats.plr_curr.end_subseqno = subseqno;

    ic->ic_aow.estats.plr_cumltv.end_time = curr_time;
    ic->ic_aow.estats.plr_cumltv.end_seqno = seqno;
    ic->ic_aow.estats.plr_cumltv.end_subseqno = subseqno;
    
    AOW_PLR_UNLOCK(ic);    

    return 0;
}

/*
 * function: aow_plr_print_inst()
 * ---------------------------------
 * Extended stats: Print Packet Lost Rate statistics for 
 * a PLR group instance.
 *
 */
static inline int aow_plr_print_inst(struct ieee80211com *ic,
                                     struct pkt_lost_rate_stats *plr_inst)
{
    u_int32_t ratio_int_part;
    u_int32_t ratio_frac_part;
    long all_mpdus = 0;
    u_int32_t lost_mpdus = 0;
    u_int32_t retval;
    
    IEEE80211_AOW_DPRINTF("Starting sequence no = %u\n",
                          plr_inst->start_seqno);
    
    IEEE80211_AOW_DPRINTF("Starting subsequence no = %hu\n",
                          plr_inst->start_subseqno);
    
    IEEE80211_AOW_DPRINTF("Ending sequence no = %u\n",
                          plr_inst->end_seqno);
    
    IEEE80211_AOW_DPRINTF("Ending subsequence no = %hu\n",
                          plr_inst->end_subseqno);
 
    if (!plr_inst->isInUse) {
        all_mpdus = 0;
    } else if ((all_mpdus = aow_compute_nummpdus(plr_inst->start_seqno,
                                                 plr_inst->start_subseqno,
                                                 plr_inst->end_seqno,
                                                 plr_inst->end_subseqno)) < 0) {
        IEEE80211_AOW_DPRINTF("Sequence no. roll-over detected\n");
        return -1;
    }

    lost_mpdus = all_mpdus - plr_inst->num_mpdus;

    IEEE80211_AOW_DPRINTF("Number of MPDUs received without fail = %u\n",
                          plr_inst->num_mpdus);
    
    IEEE80211_AOW_DPRINTF("Delayed frames (Receive stats) = %u\n",
                          plr_inst->nok_framecount);
    
    IEEE80211_AOW_DPRINTF("Number of MPDUs that came late = %u\n",
                          plr_inst->late_mpdu);
    
    IEEE80211_AOW_DPRINTF("Number of all MPDUs = %u\n",
                          (u_int32_t)all_mpdus);
    
    IEEE80211_AOW_DPRINTF("Number of lost MPDUs = %u\n",
                          lost_mpdus);

    
    if (0 == all_mpdus)
    {
       IEEE80211_AOW_DPRINTF("Packet Lost Rate : NA\n");

    } else {

        if ((retval = uint_floatpt_division((u_int32_t)lost_mpdus,
                                            all_mpdus,
                                            &ratio_int_part,
                                            &ratio_frac_part,
                                            ATH_AOW_ES_PRECISION_MULT)) < 0) {
            return retval;
        }
    
       IEEE80211_AOW_DPRINTF("Packet Lost Rate = %u.%0*u\n",
                             ratio_int_part,
                             ATH_AOW_ES_PRECISION,
                             ratio_frac_part);
    }

    return 0;
}

/*
 * function: aow_plr_print()
 * ---------------------------------
 * Extended stats: Print Packet Lost Rate statistics.
 *
 */
int aow_plr_print(struct ieee80211com *ic)
{
    struct pkt_lost_rate_stats plr_stats[ATH_AOW_MAX_ES_GROUPS - 1];
    int i = 0;
    int count = 0;
    int retval = 0;

    AOW_PLR_LOCK(ic);
    
    if (NULL == ic->ic_aow.estats.aow_plr_stats) {
        IEEE80211_AOW_DPRINTF("\n------------------------------------------\n");
        IEEE80211_AOW_DPRINTF("\nPacket Lost Rate Statistics not available\n");
        IEEE80211_AOW_DPRINTF("------------------------------------------\n");
        AOW_PLR_UNLOCK(ic);
        return -1;
    }

    IEEE80211_AOW_DPRINTF("\n--------------------------------------------\n");
    IEEE80211_AOW_DPRINTF("\nPacket Lost Rate Statistics\n");
    IEEE80211_AOW_DPRINTF("--------------------------------------------\n");
        
    retval = aow_plr_fifo_copy(ic, plr_stats, &count);

    if (retval >= 0 || ic->ic_aow.estats.plr_curr.isInUse) {
        
        IEEE80211_AOW_DPRINTF("\nStats at %ds time intervals"
                          " (from oldest to latest):\n\n",
                          ATH_AOW_ES_GROUP_INTERVAL);

        if (retval >= 0)
        {
            for (i = 0; i < count; i++) {
                IEEE80211_AOW_DPRINTF("Sr. No. %u\n", plr_stats[i].srNo);
                aow_plr_print_inst(ic, &plr_stats[i]);
                IEEE80211_AOW_DPRINTF("\n");
            }
        }

        if (ic->ic_aow.estats.plr_curr.isInUse) {
            IEEE80211_AOW_DPRINTF("Sr. No. %d\n",
                                  ic->ic_aow.estats.plr_curr.srNo);
            aow_plr_print_inst(ic, &ic->ic_aow.estats.plr_curr);
            IEEE80211_AOW_DPRINTF("\n");
        }
    }

    IEEE80211_AOW_DPRINTF("Cumulative Stats:\n");
    aow_plr_print_inst(ic, &ic->ic_aow.estats.plr_cumltv);
    IEEE80211_AOW_DPRINTF("\n");
    
    AOW_PLR_UNLOCK(ic);

    return 0; 
}
 
/* Operations on FIFO for Packet Lost Rate */
/* The FIFO will contain (ATH_AOW_MAX_ES_GROUPS - 1) entries */

/* TODO: Low priority: Create a common FIFO framework for 
   L2 Packet Errors and Packet Loss Stats */

/*
 * function: aow_plr_fifo_init()
 * ---------------------------------
 * Extended stats: Initialize FIFO for Packet Lost Rate.
 *
 * This function is to be called ONLY by the aow_plr*() functions.
 *
 */
static int aow_plr_fifo_init(struct ieee80211com *ic)
{
   ic->ic_aow.estats.aow_plr_stats =
       (struct pkt_lost_rate_stats *)OS_MALLOC(ic->ic_osdev,
                                            ATH_AOW_MAX_ES_GROUPS *\
                                               sizeof(struct pkt_lost_rate_stats),
                                            GFP_KERNEL);
    
   if (NULL == ic->ic_aow.estats.aow_plr_stats)
   {
        IEEE80211_AOW_DPRINTF("%s: Could not allocate memory\n", __func__);
        return -ENOMEM;
   }

   OS_MEMSET(ic->ic_aow.estats.aow_plr_stats,
             0,
             ATH_AOW_MAX_ES_GROUPS * sizeof(struct pkt_lost_rate_stats));

   ic->ic_aow.estats.aow_plr_stats_write = 0;
   ic->ic_aow.estats.aow_plr_stats_read = 0;

   return 0; 
}

/*
 * function: aow_plr_fifo_reset()
 * ---------------------------------
 * Extended stats: Reset FIFO for Packet Lost Rate.
 *
 * This function is to be called ONLY by the aow_plr*() functions.
 *
 */
static int aow_plr_fifo_reset(struct ieee80211com *ic)
{
    if (NULL == ic->ic_aow.estats.aow_plr_stats) {
        return -EINVAL;
    }
    
    ic->ic_aow.estats.aow_plr_stats_write = 0;
    ic->ic_aow.estats.aow_plr_stats_read = 0;
    
    OS_MEMSET(ic->ic_aow.estats.aow_plr_stats,
             0,
             ATH_AOW_MAX_ES_GROUPS * sizeof(struct pkt_lost_rate_stats));

    return 0;
}

/*
 * function: aow_plr_fifo_enqueue()
 * ---------------------------------
 * Extended stats: Enqueue an Packet Lost Rate Group instance into
 * into the FIFO.
 *
 * This function is to be called ONLY by the aow_plr*() functions.
 */
static int aow_plr_fifo_enqueue(struct ieee80211com *ic,
                                struct pkt_lost_rate_stats *plr_instnce)
{
    int aow_plr_stats_write;

    if (NULL == ic->ic_aow.estats.aow_plr_stats) {
        return -EINVAL;
    }

    aow_plr_stats_write = ic->ic_aow.estats.aow_plr_stats_write;
  
    /* We have the design option of directly storing pointers into the FIFO, or
       querying the FIFO for the pointer for writing information.
       However, the OS_MEMCPY() involves very few bytes, and that too, mostly
       every ATH_AOW_ES_GROUP_INTERVAL seconds, which is very large.
       Hence, we adopt a cleaner approach. */

    OS_MEMCPY(&ic->ic_aow.estats.aow_plr_stats[aow_plr_stats_write],
              plr_instnce,
              sizeof(struct pkt_lost_rate_stats));

    ic->ic_aow.estats.aow_plr_stats_write++;
    if (ic->ic_aow.estats.aow_plr_stats_write == ATH_AOW_MAX_ES_GROUPS) {
        ic->ic_aow.estats.aow_plr_stats_write = 0;
    }
    
    if (ic->ic_aow.estats.aow_plr_stats_write == ic->ic_aow.estats.aow_plr_stats_read) {
        ic->ic_aow.estats.aow_plr_stats_read++;
        if (ic->ic_aow.estats.aow_plr_stats_read == ATH_AOW_MAX_ES_GROUPS) {
            ic->ic_aow.estats.aow_plr_stats_read = 0;
        }
    }

    return 0;
}

/*
 * function : aow_plr_fifo_copy()
 * ----------------------------------
 * Extended stats: Copy contents of Packet Lost Rate FIFO into an
 * array, in increasing order of age.
 * The FIFO contents are not deleted.
 * -EOVERFLOW is returned if the FIFO is empty.
 * (Note: EOVERFLOW is the closest OS independant error definition that seems
 * to be currently available).
 *
 * This function is to be called ONLY by the aow_plr*() functions.
 *
 */
static int aow_plr_fifo_copy(struct ieee80211com *ic,
                             struct pkt_lost_rate_stats *aow_plr_stats,
                             int *count)
{
    int writepos = ic->ic_aow.estats.aow_plr_stats_write;
    int readpos = ic->ic_aow.estats.aow_plr_stats_read;
    
    int ph1copy_start = -1; /* Starting position for phase 1 copy */
    int ph1copy_count = 0;  /* Byte count for phase 1 copy */
    int ph2copy_start = -1; /* Starting position for phase 2 copy */
    int ph2copy_count = 0;  /* Byte count for phase 2 copy */

    if (writepos == readpos) {
        return -EOVERFLOW;
    }

    *count = 0;
    
    ph1copy_start = readpos;
    
    if (readpos < writepos) {
        /* writepos itself contains no valid data */
        ph1copy_count = (writepos - readpos);
        /* Phase 2 copy not required */
        ph2copy_count = 0;
    } else { 
        ph1copy_count = ATH_AOW_MAX_ES_GROUPS - readpos;
        if (writepos == 0) {
            ph2copy_count = 0;
        } else {
            ph2copy_start = 0;
            ph2copy_count = writepos;
        }
    }

    OS_MEMCPY(aow_plr_stats,
              ic->ic_aow.estats.aow_plr_stats + ph1copy_start,
              ph1copy_count * sizeof(struct pkt_lost_rate_stats));

    if (ph2copy_count > 0)
    {
        OS_MEMCPY(aow_plr_stats + ph1copy_count,
                  ic->ic_aow.estats.aow_plr_stats + ph2copy_start,
                  ph2copy_count * sizeof(struct pkt_lost_rate_stats));
    }
    
    *count = ph1copy_count + ph2copy_count;

    return 0;
}

/*
 * function: aow_plr_fifo_deinit()
 * ---------------------------------
 * Extended stats: De-Initialize FIFO for Packet Lost Rate.
 *
 * This function is to be called ONLY by the aow_plr*() functions.
 *
 */
static void aow_plr_fifo_deinit(struct ieee80211com *ic)
{
    if (ic->ic_aow.estats.aow_plr_stats != NULL)
    {    
        OS_FREE(ic->ic_aow.estats.aow_plr_stats);
        ic->ic_aow.estats.aow_plr_stats = NULL;
        ic->ic_aow.estats.aow_plr_stats_write = 0;
        ic->ic_aow.estats.aow_plr_stats_read = 0;
    }
}

/*
 * function: aow_plrinst_init()
 * ---------------------------------
 * Extended stats: Initialize an Packet Lost Rate Group instance.
 *
 * This function is to be called ONLY by the aow_plr*() functions.
 *
 */
static inline void aow_plrinst_init(struct pkt_lost_rate_stats *plr_inst,
                                    u_int32_t srNo,
                                    u_int32_t seqno,
                                    u_int16_t subseqno,
                                    systime_t start_time)
{
    plr_inst->srNo = srNo;
    plr_inst->start_seqno = seqno;
    plr_inst->start_subseqno = subseqno;
    plr_inst->start_time = start_time;
    plr_inst->num_mpdus = 0;
    plr_inst->nok_framecount = 0;
    plr_inst->late_mpdu = 0;
    plr_inst->isInUse = true;
    plr_inst->wasAudStopped = false;
}

/*
 * function: aow_mcss_print()
 * ---------------------------------
 * Extended stats: Print MCS statistics.
 *
 */
void aow_mcss_print(struct ieee80211com *ic)
{
    int i, j;
    struct mcs_stats *mstats = &ic->ic_aow.estats.aow_mcs_stats;
    
    IEEE80211_AOW_DPRINTF("\n--------------------------------------------\n");
    IEEE80211_AOW_DPRINTF("\nMCS Statistics\n");
    IEEE80211_AOW_DPRINTF("--------------------------------------------\n");
 
    for (i = 0; i < ATH_AOW_NUM_MCS; i++)
    {
        IEEE80211_AOW_DPRINTF("\nMCS%d:\n", i);
        IEEE80211_AOW_DPRINTF("Attempt No    Successes        Failures\n");

        for (j = 0; j < ATH_AOW_TXMAXTRY; j++) {
            IEEE80211_AOW_DPRINTF("%-13u %-16u %-u\n",
                                  j + 1,
                                  mstats->mcs_info[i].ok_txcount[j],
                                  mstats->mcs_info[i].nok_txcount[j]);
        }
    }
}

/*
 * function: uint_floatpt_division
 * ---------------------------------
 * Carry out floating point division on unsigned integer
 * dividend and divisor, to obtain unsigned int integer
 * component, and fraction component expressed as an
 * unsigned int by multiplying it by a power of 10, 'precision_mult'
 *
 * Note: This is a simple function which will not handle large
 * values. The values it can handle will depend upon precision_mult.
 * It suffices for use in ES/ESS extended stat reporting.
 * We avoid use of softfloat packages for efficiency.
 *
 */
static inline int uint_floatpt_division(const u_int32_t dividend,
                                        const u_int32_t divisor,
                                        u_int32_t *integer,
                                        u_int32_t *fraction,
                                        const u_int32_t precision_mult)
{
    
#define ATH_AOW_UINT_MAX_VAL 4294967295U
    
    if (0 == divisor || dividend > (ATH_AOW_UINT_MAX_VAL/precision_mult)) {
        return -EINVAL;
    }

    *integer = dividend / divisor;

    *fraction = (dividend * precision_mult) / divisor ;

    return 0;
}

/*
 * function: aow_compute_nummpdus()
 * ---------------------------------
 * Compute the number of MPDUs given the 
 * starting and ending AoW sequence+subsequence numbers.
 * 
 * This is a simple function intended primarily for use in
 * ES/ESS stats. It does not handle sequence number roll-over,
 * as at present (it merely returns -1 in this case).
 * TODO: Handle rollover, if required in the future.
 *
 */
static inline long aow_compute_nummpdus(u_int32_t start_seqno,
                                        u_int8_t start_subseqno,
                                        u_int32_t end_seqno,
                                        u_int8_t end_subseqno)
{
    long num_mpdus = 0;

    num_mpdus = (end_subseqno - start_subseqno) + 
                (end_seqno - start_seqno)* ATH_AOW_NUM_MPDU
                + 1;
                

    if (num_mpdus < 0) {
        return -1;
    } else {
        return num_mpdus;
    }
}


#if ATH_SUPPORT_AOW_DEBUG

/*
 * function : aow_dbg_init
 * ----------------------------------
 * Initialize the stats members, currently used to debug Clock Sync
 *
 */
static int aow_dbg_init(struct aow_dbg_stats* p)
{
    int i = 0;

    p->index = 0;
    p->sum = 0;
    p->max = 0;
    p->min = 0xffffffff;
    p->pretsf = 0;
    p->prnt_count = 0;
    p->wait = TRUE;

    for (i = 0; i < AOW_MAX_TSF_COUNT; i++) {
        p->tsf[i] = 0;
    }        

    return 0;
}

/*
 * function : aow_update_dbg_stats
 * ----------------------------------
 * Update the time related stats
 *
 */

static int aow_update_dbg_stats(struct aow_dbg_stats *p, u_int64_t val)
{
    p->pretsf = p->tsf[p->index];
    p->tsf[p->index] = val;
    p->sum = p->sum + val - p->pretsf;
    p->avg = p->sum >> 8;


    /* wait for one complete cycle to remove the wrong
     * values
     */
    if (!p->wait) {
        if (p->min > p->tsf[p->index])
            p->min = p->tsf[p->index];

        if (p->max < p->tsf[p->index])
            p->max = p->tsf[p->index];
    }        

    p->index = ((p->index + 1) & 0xff);
    p->prnt_count++;

    if (p->prnt_count == AOW_DBG_PRINT_LIMIT) {
        IEEE80211_AOW_DPRINTF("avg : %llu  min : %llu  max : %llu \n", 
            p->avg, p->min, p->max);
        p->prnt_count = 0;
        p->wait = FALSE;
    }        
}

#endif  /* ATH_SUPPORT_AOW_DEBUG */
#endif  /* ATH_SUPPORT_AOW */
