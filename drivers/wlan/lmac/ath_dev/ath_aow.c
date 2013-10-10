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
 * 
 */

#if  ATH_SUPPORT_AOW
#include "ath_internal.h"
#include "osdep.h"

#if  ATH_SUPPORT_AOW_GPIO
#include <ar7240.h>
#endif  /* ATH_SUPPORT_AOW_GPIO */

#define AOW_TIMER_VALUE 4   /* Timer kicks in 4ms interval */

/**
 * @brief       AoW timer function
 * @param[in]   sc_ptr  : handle to LMAC data structure
 * @return      Always return 0
 */

int ath_aow_tmr_func(void *sc_ptr)
{
    struct ath_softc *sc = (struct ath_softc *)sc_ptr;

    /* Process the timer data */
    sc->sc_ieee_ops->ieee80211_consume_audio_data((struct ieee80211com *)(sc->sc_ieee),
                                                   ath_hal_gettsf64(sc->sc_ah));    
    return 0;
}
                 
/**
 * @brief       Starts timer to process the received audio data
 * @param[in]   sc  : handle to LMAC data structure
 */
                 
void ath_aow_proc_timer_init(struct ath_softc *sc)
{
    sc->sc_aow.timer.active_flag = 0;
    ath_initialize_timer( sc->sc_osdev, 
                          &sc->sc_aow.timer, 
                          AOW_TIMER_VALUE, 
                          ath_aow_tmr_func,
                          (void *)sc);
}
                 
/**
 * @brief       Free the AoW Proc timer
 * @param[in]   sc : handle to LMAC data structure   
 */

void ath_aow_proc_timer_free(struct ath_softc *sc)
{
    if ( ath_timer_is_initialized(&sc->sc_aow.timer))
    {
        ath_cancel_timer( &sc->sc_aow.timer, CANCEL_NO_SLEEP);
        ath_free_timer( &sc->sc_aow.timer );
    }
}
                 
/**
 * @brief       Start the AoW Proc timer. This starts the timer function that
 *              triggers processing of received audio data
 * @param[in]   dev :   handle to LMAC device
 */

void ath_aow_proc_timer_start(ath_dev_t dev) 
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    if ( !ath_timer_is_active( &sc->sc_aow.timer) ) {
        /* start the timer */
        ath_start_timer(&sc->sc_aow.timer);
    }
}
                 
/**
 * @brief       Stop the AoW Proc timer, this stops the timer function that
 *              triggers processing of received audio data
 * @param   dev : handle to LMAC device
 */

void ath_aow_proc_timer_stop(ath_dev_t dev) 
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    if ( ath_timer_is_active( &sc->sc_aow.timer) ) {
        ath_cancel_timer( &sc->sc_aow.timer, CANCEL_NO_SLEEP);
    }
}

/**
 * @brief   Initialize the AR7240 GPIO
 */

void ath_init_ar7240_gpio_configure(void)
{
#if ATH_SUPPORT_AOW_GPIO
    u_int32_t val = ar7240_reg_rd(AR7240_GPIO_OE);
    /* Set the GPIO 6 and 11 a output */
    val |= ((1<<12)|(1<<13));
    ar7240_reg_wr(AR7240_GPIO_OE, val);
#endif /* ATH_SUPPORT_AOW_GPIO */   
}


/**
 * @brief       Toggle the current AR7240 GPIO state
 * @param[in]   dev     : handle to LMAC device
 * @param[in]   flag    :   Flag indicating the toggle state
 */

void ath_gpio12_toggle(ath_dev_t dev, u_int16_t flag)
{
#if ATH_SUPPORT_AOW_GPIO    
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    /*
     * XXX : I2S uses some of the GPIOs so avoid
     * fiddling with them for the time being
     *
     */
    if (flag) {
        ar7240_reg_wr(AR7240_GPIO_SET, 1<< 12);    
    } else {
        ar7240_reg_wr(AR7240_GPIO_CLEAR, 1<< 12);
    }         
    __11nstats(sc,wd_spurious);
#endif   /* ATH_SUPPORT_AOW_GPIO */ 
}

/**
 * @brief       AoW HW timer interrupt handler
 * @param[in]   arg void pointer interpreted as handle to LMAC data structure
 */

void ath_hwtimer_comp_int(void *arg)
{
    struct ath_softc *sc = (struct ath_softc *)arg;
    struct ieee80211com *ic = (struct ieee80211com*)(sc->sc_ieee);

    /* 
     * XXX : I2S uses some of the GPIOs so avoid
     * fiddling with them for the time being
     *
     */
    if (sc->sc_aow.tgl_state) {
#if  ATH_SUPPORT_AOW_GPIO
        ar7240_reg_wr(AR7240_GPIO_SET, 1<< 13);    
#endif  /* ATH_SUPPORT_AOW_GPIO */        
    } else {
#if  ATH_SUPPORT_AOW_GPIO
        ar7240_reg_wr(AR7240_GPIO_CLEAR, 1<< 13);
#endif  /* ATH_SUPPORT_AOW_GPIO */        
    }         
    __11nstats(sc,wd_tx_hung);
    sc->sc_aow.tgl_state = !sc->sc_aow.tgl_state;
    sc->sc_aow.tmr_tsf += AOW_TSF_TIMER_OFFSET;
    ath_gen_timer_start(sc, sc->sc_aow.hwtimer, sc->sc_aow.tmr_tsf, AOW_TSF_TIMER_OFFSET);    


    /*
     * XXX : The DMA start is triggered by the first packet 
     *       that we receive, after the START control packet
     */

    if (ic->ic_aow.i2s_dmastart) {
        ic->ic_aow.i2s_dmastart = AH_FALSE;
        CLR_I2S_DMA_START(ic->ic_aow.i2s_flags);
        ar7240_i2s_fill_dma();
        ar7240_i2sound_dma_start(0);
        ic->ic_aow.i2s_dma_start++;
    } else {
        ar7240_i2sound_dma_resume(0);
    }

}

/**
 * @brief       AoW HW Timer overflow interrupt handler
 * @param[in]   arg void pointer interpreted as handle to LMAC data structure
 */

void ath_hwtimer_overflow_int(void *arg)
{
    struct ath_softc *sc = (struct ath_softc *)arg;

    /* Hope it does not overflow by more then 4 ms */
    sc->sc_aow.tmr_tsf += AOW_TSF_TIMER_OFFSET;
    ath_gen_timer_start(sc, sc->sc_aow.hwtimer, sc->sc_aow.tmr_tsf, AOW_TSF_TIMER_OFFSET);    
}

/**
 * @brief       Starts the HW Timer
 * @param[in]   dev         :  handle to LMAC device 
 * @param[in]   tsfStart    :  32 Bit TSF value
 * @param[in]   period      :  Timer period
 */

void ath_hwtimer_start(ath_dev_t dev, u_int32_t tsfStart, u_int32_t period)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    /* Check if the timer has been attached */
    if (!ath_hal_hasgentimer(sc->sc_ah)) return;
    if (!sc->sc_aow.tmr_init_done) {
        sc->sc_hasgentimer = 1; 
        ath_gen_timer_attach(sc);


        /* 
         * ATH_SUPPORT_AOW : Original call to ath_gen_timer_alloc
         * XXX : Fix the trigger of compare interrupt 
         */

        sc->sc_aow.hwtimer =  ath_gen_timer_alloc(sc,
                                           HAL_GEN_TIMER_TSF,
                                           ath_hwtimer_comp_int,
                                           ath_hwtimer_overflow_int,
                                           NULL,
                                           sc);

        
        /*
         * FIXME : AOW
         * ATH_SUPPORT_AOW : Temp hack, we pass comp_int function 
         * for both compare and overflow conditions
         * 
         * XXX : We cannot use ath_hwtimer_comp_int for overflow
         * condiftions. This has fatal effect on the I2S.
         *
         */


        if( sc->sc_aow.hwtimer == NULL ) return;
        sc->sc_aow.tmr_init_done = 1;
    } 
    if( !sc->sc_aow.tmr_running ) {
        sc->sc_aow.tmr_running = 1;
        ath_init_ar7240_gpio_configure();
        sc->sc_aow.tmr_tsf = tsfStart;
        ath_gen_timer_start(sc, sc->sc_aow.hwtimer, sc->sc_aow.tmr_tsf, period);
    }
}

/**
 * @brief       Stop the HW timer
 * @param[in]   dev     :   handle to LMAC device
 */

void ath_hwtimer_stop(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    if( sc->sc_aow.tmr_running ) {
        ath_gen_timer_stop( sc, sc->sc_aow.hwtimer);
        sc->sc_aow.tmr_running = 0;
    }
}

/*
 * Channel set command consists of
 * - Channel Set command (4 bytes)
 * - Channel (4 bytes)
 * - MAC Address (6 bytes)
 */
#define AOW_CHAN_SET_COMMAND_SIZE (4 + 4 + 6)

/**
 * @brief       Set the mapping between the audio channel and mac address
 * @param[in]   dev     : handle to LMAC device
 * @param[in]   pkt     : handle to input packet structure
 * @param[in]   len     : Length of the packet
 * @return      Sucess (1) or Failure (0)
 */
 

int ath_aow_set_audio_channel(ath_dev_t dev, void *pkt, int len)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ieee80211com *ic = (struct ieee80211com*)(sc->sc_ieee);
    struct ether_addr macaddr;
    int channel;
    int command;
    int ret = AH_FALSE;

    KASSERT(pkt, ("AOW, Null Packet"));


    /*
     * Command format
     * | Command | Channel | Mac Address |
     * |    4    |    4    |    6        |
     *
     */

    if (len == AOW_CHAN_SET_COMMAND_SIZE) {
        memcpy(&command, pkt, sizeof(int));
        
        if (command == ATH_AOW_SET_CHANNEL_COMMAND) {
          memcpy(&channel, pkt + sizeof(int), sizeof(int));
          memcpy(&macaddr, pkt + (2 * sizeof(int)), sizeof(struct ether_addr));
          ic->ic_set_audio_channel(ic, channel, &macaddr);
          ret = AH_TRUE;
        }
    }
    return ret;
}

/**
 * @brief       AoW function to transmit audio packets to all associated STAs
 * @param[in]   dev     :   handle to LMAC device 
 * @param[in]   pkt     :   handle to input packet structure
 * @param[in]   len     :   Length of the packet
 * @param[in]   seqno   :   Packet sequence number
 * @param[in]   tsf     :   64 bit TSF value
 * @return      Success (1) or Failure (0)
 */

int ath_aow_send2all(ath_dev_t dev, void *pkt, int len, u_int32_t seqno, u_int64_t tsf)
{

    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ieee80211com *ic = (struct ieee80211com *)(sc->sc_ieee);
    struct ieee80211vap *vap;

    if(!pkt)
      return -1;

    TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
        if (vap) {
            if (sc->sc_ieee_ops->ieee80211_send2all_nodes)
                sc->sc_ieee_ops->ieee80211_send2all_nodes(vap, pkt, len, seqno, tsf);
        }            
    }

    return 0;
}

/**
 * @brief       Callback to handle the AoW control data, this function 
 *              does two functions, sets the channel/address mapping for
 *              if the received data is command data, else it sends the
 *              data to all the associated nodes
 * @param[in]   dev     :   handle to LMAC device
 * @param[in]   id      :   Index
 * @param[in]   indata  :   handle to input buffer
 * @param[in]   insize  :   Size of the input buffer 
 * @param[in]   outdata :   handle to output buffer
 * @param[in]   outsize :   Size of the output buffer
 * @param[in]   seqno   :   Sequence number
 * @param[in]   tsf     :   64 bit TSF value
 * @return      Success (1)
 */

int ath_aow_control(ath_dev_t dev, u_int id,
                void *indata, u_int32_t insize,
                void *outdata, u_int32_t *outsize,
                u_int32_t seqno, u_int64_t tsf)
{

    /*
     * Handle the channel settings here
     * XXX : Not the best of the ways, but serves the
     *        purpose
     *
     */

    if (insize == AOW_CHAN_SET_COMMAND_SIZE) {
        if (ath_aow_set_audio_channel(dev, indata, insize))
            return 0;
    }

    ath_aow_send2all(dev, indata, insize, seqno, tsf);
    return 0;
}

/**
 * @brief       Get the current sequence number
 * @param[in]   dev :  handle to LMAC device 
 * @return      current sequence number
 */

u_int32_t
ath_get_aow_seqno(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    sc->sc_aow.seq_no++;
    return sc->sc_aow.seq_no;
}    

/**
 * @brief       AoW init function
 * @param[in]   sc : handle to LMAC data structure
 */

void ath_aow_init(struct ath_softc *sc)
{
    sc->sc_aow.sw_retry_limit = ATH_MAX_SW_RETRIES;
    ath_aow_proc_timer_init(sc);
}

/**
 * @brief       Timer close handler
 * @param[in]   sc : handle to LMAC data structure
 */

void ath_aow_tmr_cleanup(struct ath_softc *sc)
{
    if (sc->sc_hasgentimer) {
        if( sc->sc_aow.tmr_init_done ) {
            if (sc->sc_aow.tmr_running) { 
                ath_gen_timer_stop(sc, sc->sc_aow.hwtimer);
            }
            ath_gen_timer_free(sc, sc->sc_aow.hwtimer);
            ath_gen_timer_detach(sc);
        }
    }
    ath_aow_proc_timer_free(sc);
}

/**
 * @brief       Update the Tx related AoW extended stats information
 * @param[in]   sc : handle to LMAC data structure
 * @param[in]   bf : handle to ath_buf
 * @param[in]   ts : Pointer to TX status
 */
void ath_aow_update_txstatus(struct ath_softc *sc,
                             struct ath_buf *bf,
                             struct ath_tx_status *ts)
{
    struct ieee80211com *ic = (struct ieee80211com*)(sc->sc_ieee);
    ic->ic_aow_record_txstatus(ic, bf, ts);
}

#endif /* ATH_SUPPORT_AOW */
