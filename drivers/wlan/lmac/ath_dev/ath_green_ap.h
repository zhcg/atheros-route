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

#ifndef _IF_ATH_GREEN_AP_H
#define _IF_ATH_GREEN_AP_H

typedef enum {
    ATH_PS_EVENT_INC_STA,
    ATH_PS_EVENT_DEC_STA,
    ATH_PS_EVENT_PS_ON,
    ATH_PS_EVENT_PS_WAIT,
}ath_ps_event;


#ifdef ATH_SUPPORT_GREEN_AP /* Right definations */
/*
 *  Copyright (c) 2008 Atheros Communications Inc.  All rights reserved.
 */

/*
 * Public Interface for Green AP module
 */
 
typedef enum {
    ATH_PWR_SAVE_IDLE,
    ATH_PWR_SAVE_OFF,
    ATH_PWR_SAVE_WAIT,
    ATH_PWR_SAVE_ON,
} power_save_state;


struct ath_green_ap {
    /* Variables for single antenna powersave */
    u_int16_t power_save_enable;
    power_save_state ps_state;
    u_int16_t num_nodes;        
    struct ath_timer	ps_timer;
    ath_ps_event timer_event;    
    u_int16_t ps_trans_time; /* In seconds */
    u_int16_t ps_on_time;
};

#define PS_RX_MASK (0x1)
#define PS_TX_MASK (0x1)

int ath_green_ap_attach( struct ath_softc *sc);
int ath_green_ap_detach( struct ath_softc *sc);
void ath_green_ap_stop( struct ath_softc *sc);
void ath_green_ap_start( struct ath_softc *sc);
u_int16_t ath_green_ap_is_powersave_on( struct ath_softc *sc);
void ath_green_ap_state_mc( struct ath_softc *sc, ath_ps_event event);
/* Config. functions called from command line */
void ath_green_ap_sc_set_enable(struct ath_softc *sc, int32_t val );
int32_t ath_green_ap_sc_get_enable(struct ath_softc *sc  );
void ath_green_ap_sc_set_transition_time(struct ath_softc *sc, int32_t val );
int32_t ath_green_ap_sc_get_transition_time(struct ath_softc *sc);
void ath_green_ap_sc_set_on_time(struct ath_softc *sc, int32_t val );
int32_t ath_green_ap_sc_get_on_time(struct ath_softc *sc);
void ath_green_ap_suspend( struct ath_softc *sc);
#else

struct ath_green_ap {
    int dummy;
};

/* Dummy defines so that the if_ath.c compiles without warning */
#define ath_green_ap_attach( sc)(0)
#define ath_green_ap_detach( sc) do{}while(0)
#define ath_green_ap_stop(dev)do{}while(0)
#define ath_green_ap_start( sc)do{}while(0)
#define ath_green_ap_is_powersave_on(dev) (0)
#define ath_green_ap_state_mc( dev, node_add_remove) do{}while(0)
#define ath_green_ap_sc_set_enable(dev, val )do{}while(0)
#define ath_green_ap_sc_get_enable(dev) (0)
#define ath_green_ap_sc_set_transition_time(dev, val) do{}while(0)
#define ath_green_ap_sc_get_transition_time(dev) (0)
#define ath_green_ap_sc_set_on_time(dev, val) do{}while(0)
#define ath_green_ap_sc_get_on_time(dev) (0)
#define ath_green_ap_suspend(sc) (0)

#endif // ATH_GREEN_AP


#endif //_IF_ATH_GREEN_AP_H

