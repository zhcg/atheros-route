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
 * Public Interface for AoW control module
 */

#ifndef _DEV_ATH_AOW_H
#define _DEV_ATH_AOW_H

#if  ATH_SUPPORT_AOW

#include "ath_hwtimer.h"

/* defines */

/*
 * TODO : The shared macros between the app (mplay) and
 *        the driver should be moved to a sperate header file
 */

/**
 * @brief   General define section
 */

/* Keep the below in sync with AOW_MAX_RECVRS in 
   umac/include/ieee80211_aow_shared.h */
#define AOW_MAX_RECEIVER_COUNT  10

#define ATH_AOW_MAGIC_NUMBER    0xdeadbeef
#define MAX_AOW_PKT_SIZE        840*2
#define IOCTL_AOW_DATA          (1)
#define AOW_TSF_TIMER_OFFSET     8000

#define ATH_SW_RETRY_LIMIT(_sc) (_sc->sc_aow.sw_retry_limit)
#define ATH_ENAB_AOW_ER(_sc)    (_sc->sc_aow.er)
#define ATH_ENAB_AOW_EC(_sc)    (_sc->sc_aow.ec)
#define ATH_ENAB_AOW_ES(_sc)    (_sc->sc_aow.es)
#define ATH_ENAB_AOW_ESS(_sc)   (_sc->sc_aow.ess)

#define ATH_ENAB_AOW(_sc)       (1)


/**
 * @brief   data structures 
 */

/**
 * @brief   Mapping between the AoW audio channel and respective
 *          destination MAC address.
 * 
 *          The following information is stored
 *          - Channel 
 *          - Valid Flag
 *          - Respective AoW sequence number
 *          - Destination MAC Address
 *
 */

typedef struct aow_chandst {
    int channel;                                        /* Logical audio channel */
    int valid;                                          /* Valid if channel set */
    unsigned int dst_cnt;                               /* Number of destination */
    unsigned int seqno;                                 /* Seq no unique to channel */
    struct ether_addr addr[AOW_MAX_RECEIVER_COUNT];     /* Destination Mac address */
}aow_chandst_t;    

/**
 * @brief   AoW data structure for lmac
 */

typedef struct ath_aow {

    u_int    sw_retry_limit;    /* controls the software retries */
    u_int    latency;       /* controls the end2end latency */
    u_int    er;            /* error recovery feature flag */
    u_int    ec;            /* error concelement feature flag */
    u_int    ec_fmap;       /* test handle, indicates the failmap */
    u_int    es;            /* extended statistics feature flag */
    u_int    ess;           /* extended statistics (synchronized) feature flag */
    
    u_int64_t latency_us;   /* latency in micro seconds */
    u_int32_t seq_no;           /* aow sequence number, map it to Node later */
    u_int32_t playlocal;    /* local playback flag */

    u_int16_t tgl_state;
    u_int16_t overflow_state;
    u_int16_t int_started;
    u_int16_t tmr_init_done;
    u_int16_t tmr_running;
    u_int32_t tmr_tsf;

    /* HW timer for I2S Start */
    struct ath_gen_timer *hwtimer;

    /* channel, mac address mapping */
    struct aow_chandst   chan_addr[AOW_MAX_AUDIO_CHANNELS];

    /* total number of channel mapped */
    int mapped_recv_cnt;
    
    /* AoW timer */
    struct ath_timer timer;

}ath_aow_t;   


/**
 * @brief   functions section
 */


/**
 * @brief       AoW function to transmit audio packets to all associated STAs
 * @param[in]   dev : handle to LMAC device
 * @param[in]   pkt : handle to input packet structure
 * @param[in]   len : Length of the packet
 * @param[in]   seqno :  Packet sequence number
 * @param[in]   tsf   : 64 bit TSF value
 * @return      Success (1) or Failure (0)
 */

int ath_aow_send2all(ath_dev_t dev,
                     void *pkt,
                     int len,
                     u_int32_t seqno,
                     u_int64_t tsf);

/**
 * @brief       Callback to handle the AoW control data, this function 
 *              does two functions, sets the channel/address mapping for
 *              if the received data is command data, else it sends the
 *              data to all the associated nodes
 * @param[in]   dev     : handle to LMAC device
 * @param[in]   id      : Index
 * @param[in]   indata  : handle to input buffer
 * @param[in]   insize  : Size of the input buffer 
 * @param[in]   outdata : handle to output buffer
 * @param[in]   outsize : Size of the output buffer
 * @param[in]   seqno   : Sequence number
 * @param[in]   tsf     : 64 bit TSF value
 * @return      Success (1)
 */

int ath_aow_control(ath_dev_t dev,
                    u_int id,
                    void *indata,
                    u_int32_t insize,
                    void *outdata,
                    u_int32_t *outsize,
                    u_int32_t seqno,
                    u_int64_t tsf);


/**
 * @brief       Get the current sequence number
 * @param[in]   dev : handle to LMAC device
 */

u_int32_t ath_get_aow_seqno(ath_dev_t dev);

/**
 * @brief   Initialize the AR7240 GPIO
 */

void ath_init_ar7240_gpio_configure(void);

/**
 * @brief       Toggle the current AR7240 GPIO state
 * @param[in]   dev     : handle to LMAC device
 * @param[in]   flag    : Flag indicating the toggle state
 */

void ath_gpio12_toggle(ath_dev_t dev, u_int16_t flag);

/**
 * @brief       AoW HW timer interrupt handler
 * @param[in]   arg  : void pointer interpreted as handle to LMAC data structure
 */

void ath_hwtimer_comp_int(void *arg);

/**
 * @brief       AoW HW Timer overflow interrupt handler
 * @param[in]   arg  : void pointer interpreted as handle to LMAC data structure
 */

void ath_hwtimer_overflow_int(void *arg);

/**
 * @brief       Starts the HW Timer
 * @param[in]   dev : handle to LMAC device
 * @param[in]   tsfStart 32 Bit TSF value
 * @param[in]   period Timer period
 */

void ath_hwtimer_start(ath_dev_t dev, u_int32_t tsfStart, u_int32_t period);

/**
 * @brief       Stop the HW timer
 * @param[in]   dev : handle to LMAC device
 */

void ath_hwtimer_stop(ath_dev_t dev);

/**
 * @brief       AoW function to transmit audio packets to all associated STAs
 * @param[in]   dev : handle to LMAC device
 * @param[in]   pkt : handle to packet
 * @param[in]   len : Length of the packet
 * @param[in]   seqno :  Packet sequence number
 * @param[in]   tsf   : 64 bit TSF value
 * @return      Success (1) or Failure (0)
 */

int ath_aow_send2all(ath_dev_t dev, void *pkt, int len, u_int32_t seqno, u_int64_t tsf);

/**
 * @brief   Handles the AoW control commands
 * @param[in]   dev : handle to LMAC device
 * @param[in]   id  : Index
 * @param[in]   indata : handle to input buffer
 * @param[in]   insize : Size of the input buffer 
 * @param[in]   outdata  : handle to output buffer
 * @param[in]   outsize  : Size of the output buffer
 * @param[in]   seqno    : Sequence number
 * @param[in]   tsf      : 64 bit TSF value
 * @return      Success (1)
 */

int ath_aow_control(ath_dev_t dev, u_int id,
                void *indata, u_int32_t insize,
                void *outdata, u_int32_t *outsize,
                u_int32_t seqno, u_int64_t tsf);

/**
 * @brief       Get the current sequence number
 * @param[in]   dev : handle to LMAC device
 */

u_int32_t ath_get_aow_seqno(ath_dev_t dev);
                 
/**
 * @brief       Starts timer to process the received audio data
 * @param[in]   sc  : handle to LMAC data structure
 */
                 
void ath_aow_proc_timer_init(struct ath_softc *sc);


/**
 * @brief       Free the AoW Proc timer
 * @param[in]   sc : handle to LMAC data structure
 */

void ath_aow_proc_timer_free( struct ath_softc *sc);


/**
 * @brief       Start the AoW Proc timer. This starts the timer function that
 *              triggers processing of received audio data
 * @param[in]   dev : handle to LMAC device
 */

void ath_aow_proc_timer_start( ath_dev_t dev); 


/**
 * @brief       Stop the AoW Proc timer, this stops the timer function that
 *              triggers processing of received audio data
 * @param   dev : handle to LMAC device
 */

void ath_aow_proc_timer_stop( ath_dev_t dev);

/**
 * @brief       AoW init function
 * @param[in]   sc  : handle to LMAC data structure
 */

void ath_aow_init(struct ath_softc *sc);

/**
 * @brief       Timer attach handler
 * @param[in]   sc  : handle to LMAC data structure
 */

int ath_gen_timer_attach(struct ath_softc *sc);

/**
 * @brief       Timer detach handler
 * @param[in]   sc  : handle to LMAC data structure
 */

void ath_gen_timer_detach(struct ath_softc *sc);

/**
 * @brief       Free the AoW Proc timer
 * @param[in]   sc  : handle to LMAC data structure
 */

void ath_gen_timer_free(struct ath_softc *sc, struct ath_gen_timer *timer);

/**
 * @brief       Starts the HW Timer
 * @param[in]   sc      : handle to LMAC data structure
 * @param[in]   timer   : handle to timer
 * @param[in]   timer_next : Next index
 * @param[in]   period     : Timer period
 */

void ath_gen_timer_start(struct ath_softc *sc, struct ath_gen_timer *timer,
                         u_int32_t timer_next, u_int32_t period);

/**
 * @brief       stop timer
 * @param[in]   sc  : handle to LMAC data structure
 * @param[in]   timer  : handle to timer
 */

void ath_gen_timer_stop(struct ath_softc *sc, struct ath_gen_timer *timer);

/**
 * @brief       Timer interrupt handler
 * @param[in]   sc  : handle to LMAC data structure
 */

void ath_gen_timer_isr(struct ath_softc *sc);

/**
 * @brief       Timer close handler
 * @param[in]   sc  : handle to LMAC data structure
 */

void ath_aow_tmr_cleanup(struct ath_softc *sc);

/**
 * @brief       Update the Tx related AoW extended stats information
 * @param[in]   sc  : handle to LMAC data structure
 * @param[in]   bf  : handle to ath_buf
 * @param[in]   ts  : handle to TX status
 */

void ath_aow_update_txstatus(struct ath_softc *sc,
                             struct ath_buf *bf,
                             struct ath_tx_status *ts);


#else   /* ATH_SUPPORT_AOW */

/**
 * @breief  Macros and functions definitions when 
 *          the AOW feature is not defined
 */

#define ATH_ENAB_AOW(_sc)       (0) 
#define ATH_ENAB_AOW_ER(_sc)    (0)
#define ATH_ENAB_AOW_ES(_sc)    (0)
#define ATH_ENAB_AOW_ESS(_sc)   (0)
#define ATH_SW_RETRY_LIMIT(_sc) ATH_MAX_SW_RETRIES

#define ath_aow_init(a)                 do{}while(0)
#define ath_aow_tmr_attach(a)           do{}while(0)
#define ath_aow_tmr_cleanup(a)          do{}while(0)
#define ath_aow_update_txstatus(a,b,c)  do{}while(0)

#endif  /* ATH_SUPPORT_AOW */

#endif  /* _DEV_ATH_AOW_H */
