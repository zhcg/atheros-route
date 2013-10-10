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

#include "ath_internal.h"
#include "ath_dev.h"

#include "ath_green_ap.h"


#define GREEN_AP_DISABLE (0)
#define GREEN_AP_ENABLE  (1)
#define GREEN_AP_SUSPEND (2)
#define ATH_PS_TRANSITON_TIME (20)


void ath_green_ap_start( struct ath_softc *sc);
static int ath_green_ap_ant_ps_mode_on_tmr_fun( void *sc_ptr);

/* The attach for the green AP */
int ath_green_ap_attach( struct ath_softc *sc)
{
    struct ath_green_ap* green_ap = sc->sc_green_ap;
             
    /* Sanity check */
    if ( green_ap != NULL ) {
        printk("%s","ath_green_ap struct is not NULL\n");
        return -1;            
    }
        
    /* allocate memmory */
    green_ap = (struct ath_green_ap *)
            OS_MALLOC(sc->sc_osdev, sizeof(struct ath_green_ap), GFP_KERNEL);
    /* Check if the memory was allocated */
    if ( green_ap == NULL ) {
        printk("%s","Memory allocation for ath_green_ap structure failed\n");
        return -1;    
    }
    
    /* set up the pointer in the sc structure */
    sc->sc_green_ap = green_ap;

    /* Init. the state */
    green_ap->power_save_enable=GREEN_AP_DISABLE;
    green_ap->num_nodes = 0;
    green_ap->ps_state = ATH_PWR_SAVE_IDLE;
    green_ap->ps_trans_time = ATH_PS_TRANSITON_TIME;
    green_ap->ps_on_time = 0; /* Force it to zero for the time being EV 69649 */
    /* Reset the timer active flag */
    green_ap->ps_timer.active_flag = 0;

    return 0;    
}

/* Detach function for green_ap */
int ath_green_ap_detach( struct ath_softc *sc)
{
    struct ath_green_ap* green_ap = sc->sc_green_ap;
        
    /* Sanity check */
    if ( green_ap == NULL ) {
        DPRINTF(sc,ATH_DEBUG_GREEN_AP,"%s",
                "green_ap structure already freed\n");
        return -1;    
    }
    /* Take the lock - The delete may be called when something else 
     * is going on*/
    spin_lock(&sc->green_ap_ps_lock);
    /* Delete the timer if it is on */

	if ( ath_timer_is_initialized(&green_ap->ps_timer))
	{
    ath_cancel_timer( &green_ap->ps_timer, CANCEL_NO_SLEEP);
		ath_free_timer( &green_ap->ps_timer );
	}
    
    /* Free spectral memmory and point to null */
    OS_FREE(green_ap);
    sc->sc_green_ap = NULL;
    /* This is fine, because the spinlock is in the sc struct */
    spin_unlock(&sc->green_ap_ps_lock);
    return 0;
}

/* This is used to delay the transition from power save off to power save on. 
 * Just calls the state machine with given argument 
 */
static int ath_green_ap_ant_ps_mode_on_tmr_fun( void *sc_ptr)
{
    struct ath_softc *sc = (struct ath_softc *)sc_ptr;
    struct ath_green_ap* green_ap = sc->sc_green_ap;
    
    ath_green_ap_state_mc( sc, green_ap->timer_event);
    return 0;
}

/* Reset function so that the Rx antenna mask takes effect */
static void ath_green_ap_ant_ps_reset(struct ath_softc *sc)
{
	/*
	** Add protection against green AP enabling interrupts
	** when not valid or no VAPs exist 
	*/
	
    if( (!sc->sc_invalid) && sc->sc_nvaps ) {
            ath_reset_start(sc, 0, 0, 0);
            ath_reset(sc);
            ath_reset_end(sc, 0);
	}
	else
	{
		printk("CHH(%s) Green AP tried to enable IRQs when invalid\n",__func__);
	}
}

/* This is used to start the GREEN AP from the command line. When it is  
 * enabled, depending on if any stations are associated, it goes to power save 
 *  on or off state 
 */
void ath_green_ap_start( struct ath_softc *sc)
{   
    struct ath_green_ap* green_ap = sc->sc_green_ap;
    
    /* Sanity check */
    if ( green_ap == NULL ) return;
    /* Take the lock, just in case */
    if ( !green_ap->power_save_enable) return;
    
    /* Make sure the start function does not get called 2 times */
    /* safer to check inside the lock once - to avoid
     * the start function from being called more than
     * once  by 2 threads of user ioctls 
     */
    spin_lock(&sc->green_ap_ps_lock);
    if ( green_ap->ps_state != ATH_PWR_SAVE_IDLE ) {
        DPRINTF(sc, ATH_DEBUG_GREEN_AP, "%s","Green AP already initialised   \n");
        spin_unlock(&sc->green_ap_ps_lock);
        return ;
    }
    else {
        ath_initialize_timer( sc->sc_osdev, 
                            &green_ap->ps_timer, 
                            green_ap->ps_trans_time * 1000 , 
                            ath_green_ap_ant_ps_mode_on_tmr_fun,
                            (void *)sc);
        DPRINTF(sc, ATH_DEBUG_GREEN_AP, "%s","Green AP timer initialised   \n");
        /* Force transition to the correct mode depending on node count being 
        * zero or not */
        if ( green_ap->num_nodes ) {
            /* There are stations associated. Power save mode should be off */
            green_ap->ps_state = ATH_PWR_SAVE_OFF;
            /* init timer for later use but do not start it */
        } else {
            /* There are no stations associated. Power save mode should be on */
            green_ap->ps_state = ATH_PWR_SAVE_WAIT;
            DPRINTF(sc, ATH_DEBUG_GREEN_AP, "%s","Transition to power save Wait\n");
            green_ap->timer_event = ATH_PS_EVENT_PS_ON;
            ath_start_timer(&green_ap->ps_timer);
        }
        spin_unlock(&sc->green_ap_ps_lock);
    }
}

/* Used when disable power save is called from command line.
 * Put the state in idle mode and turn off power save */
void ath_green_ap_stop( struct ath_softc *sc)
{
    struct ath_green_ap* green_ap = sc->sc_green_ap;
    
    /* Sanity check */
    if ( green_ap == NULL) return;
    
    /* Take the spin lock */
    spin_lock(&sc->green_ap_ps_lock);
    /* Delete the timer just to be sure */
    ath_cancel_timer( &green_ap->ps_timer, CANCEL_NO_SLEEP);
	ath_free_timer( &green_ap->ps_timer );
    
    green_ap->power_save_enable=0;
    if ( green_ap->ps_state == ATH_PWR_SAVE_ON) { 
        /* In the power save mode now. Force the Rx chain out of the power save
         * off mod 
         */
        ath_hal_setGreenApPsOnOff(sc->sc_ah, 0);
        ath_green_ap_ant_ps_reset(sc);
    }
    green_ap->ps_state = ATH_PWR_SAVE_IDLE;
    /* Giveup the spin lock */
    spin_unlock(&sc->green_ap_ps_lock);
}

/* Check if Power save mode is on. If so, return 1, else return 0 */
u_int16_t ath_green_ap_is_powersave_on( struct ath_softc *sc)
{
    struct ath_green_ap* green_ap = sc->sc_green_ap;
    if ( green_ap == NULL ) return 0;    
    return ((green_ap->ps_state == ATH_PWR_SAVE_ON) && 
            green_ap->power_save_enable);   
}

/* State machine for power save mode */
void ath_green_ap_state_mc( struct ath_softc *sc, ath_ps_event event)
{
    struct ath_green_ap* green_ap = sc->sc_green_ap;

    if ( green_ap == NULL ) return;
     
    /* Take the lock */
    spin_lock(&sc->green_ap_ps_lock);
    
    /* Handle the increment and decrement events first */
    switch(event) {
        case ATH_PS_EVENT_INC_STA:
            green_ap->num_nodes++;
            DPRINTF(sc,ATH_DEBUG_GREEN_AP,
                    "Incr nodes. Number of nodes are %d\n", 
                    green_ap->num_nodes);            
//             printk("incr node with nodes %d\n",green_ap->num_nodes); 
            break;
                       
        case ATH_PS_EVENT_DEC_STA:
            if ( green_ap->num_nodes ) 
                green_ap->num_nodes--;
            DPRINTF(sc, ATH_DEBUG_GREEN_AP, 
                    "Decr nodes. Number of nodes are %d\n", 
                    green_ap->num_nodes);
//             printk("incr node with nodes %d\n",green_ap->num_nodes); 
            break;            
        
        case ATH_PS_EVENT_PS_ON:
        case ATH_PS_EVENT_PS_WAIT:
            break;
            /* This event will be handled inside the state machine */
        default:
            break;
    }
    
    /* Confirm that power save is enabled  before doing state transitions */
    if ( !green_ap->power_save_enable ) { 
        spin_unlock(&sc->green_ap_ps_lock);
        return;
    }
    
    /* Now, Handle the state transitions */
    switch( green_ap->ps_state ) {
        case ATH_PWR_SAVE_IDLE:
            /* Nothing to be done in ths state, just keep track of the nodes*/
            break;
                  
        case  ATH_PWR_SAVE_OFF:
            /* Check if all the nodes have been removed. If so, go to power
             * save mode */
            if ( !green_ap->num_nodes ) {
                /* All nodes have been removed. Trun on power save mode */
                green_ap->ps_state = ATH_PWR_SAVE_WAIT;
                /* TODO turn off the antennas through the timer*/    
                DPRINTF(sc, ATH_DEBUG_GREEN_AP, 
                        "%s","Transition to power save Wait\n");
//                 printk("Transition to power save Wait\n");
                green_ap->timer_event = ATH_PS_EVENT_PS_ON;
				ath_set_timer_period( &green_ap->ps_timer,green_ap->ps_trans_time * 1000);
                ath_start_timer(&green_ap->ps_timer);
            }
            break;
              
        case ATH_PWR_SAVE_WAIT:
            /* Make sure no new nodes have been added */
            if ( !green_ap->num_nodes && (event == ATH_PS_EVENT_PS_ON)) {
                /* All nodes have been removed. Turn on power save mode */
                /* This is a fix for bug 62668: Kernel panic caused by  
                 * invalid sc->sc_curcohan bootup.This condition is 
                 * detected by checking if channel or channelFlags field in  
                 * sc_curchan structure is zero. 
                 * If this is the case, wait for some more time till a valid 
                 * sc_curchan exists */
                if( (sc->sc_curchan.channel == 0) ||
                     (sc->sc_curchan.channelFlags == 0) ) {
                    DPRINTF(sc, ATH_DEBUG_GREEN_AP, "%s %s",
                        "Current channel is not yet fixed.", 
                        " Wait till it is set\n");
                    /* Stay in the current state and restart the timer to 
                     * check later */
					ath_set_timer_period( &green_ap->ps_timer,green_ap->ps_trans_time * 1000);
                    ath_start_timer(&green_ap->ps_timer);
                } else {
                    green_ap->ps_state = ATH_PWR_SAVE_ON;
                    ath_hal_setGreenApPsOnOff(sc->sc_ah, 1);
                    ath_green_ap_ant_ps_reset(sc);
                    if ( green_ap->ps_on_time ) {
                        green_ap->timer_event = ATH_PS_EVENT_PS_WAIT;
                        ath_set_timer_period( &green_ap->ps_timer,green_ap->ps_on_time * 1000);
                        ath_start_timer(&green_ap->ps_timer);
                    }
                    DPRINTF(sc, ATH_DEBUG_GREEN_AP, "%s",
                            "Transition to power save On\n");
//                     printk("Transition to power save On\n");

                }
            } else if (green_ap->num_nodes) {
                /* Some new node has been added, move out of Power save mode */
                /* Delete the timer just to be sure */
                ath_cancel_timer( &green_ap->ps_timer, CANCEL_NO_SLEEP);
                green_ap->ps_state =ATH_PWR_SAVE_OFF;
                DPRINTF(sc, ATH_DEBUG_GREEN_AP, "%s","Transition to power save Off\n");
//                 printk("Transition to power save Off\n");
            }
            break;
            
        case  ATH_PWR_SAVE_ON:
            /* Check if a node has been added. If so, move come out of power 
             * save mode*/
            if ( green_ap->num_nodes ) {
                /* A node has been added. Need to turn off power save */
                green_ap->ps_state =ATH_PWR_SAVE_OFF;
                /* Delete the timer if it is on and reset the green AP state*/
                ath_cancel_timer( &green_ap->ps_timer, CANCEL_NO_SLEEP);
                ath_hal_setGreenApPsOnOff(sc->sc_ah, 0);
                ath_green_ap_ant_ps_reset(sc);
                DPRINTF(sc, ATH_DEBUG_GREEN_AP ,"%s",
                        "Transition to power save Off\n");
//                 printk("Transition to power save Off\n");
            } else if ((green_ap->timer_event == ATH_PS_EVENT_PS_WAIT) && 
                        (green_ap->ps_on_time) ) {
                green_ap->timer_event = ATH_PS_EVENT_PS_ON;
                /*
                ** Do NOT reinitialize the timer, just set the period!
                */

                ath_set_timer_period( &green_ap->ps_timer,green_ap->ps_trans_time * 1000);
                ath_start_timer(&green_ap->ps_timer);
                green_ap->ps_state = ATH_PWR_SAVE_WAIT;
                ath_hal_setGreenApPsOnOff(sc->sc_ah, 0);
                ath_green_ap_ant_ps_reset(sc);
                DPRINTF(sc, ATH_DEBUG_GREEN_AP ,"%s",
                        "Transition to power save Wait\n");
//                 printk("Transition to power save Wait\n");
            }
            break;  
        
        default:
            break;
    }
    spin_unlock(&sc->green_ap_ps_lock);
}

void ath_green_ap_suspend( struct ath_softc *sc)
{
    struct ath_green_ap* green_ap = sc->sc_green_ap;
    green_ap->power_save_enable = GREEN_AP_SUSPEND ;
    return ;
}

/* Config. functions called from command line */
void ath_green_ap_sc_set_enable(struct ath_softc *sc, int32_t val )
{
    struct ath_green_ap* green_ap = sc->sc_green_ap;
    if ( green_ap == NULL ) return;
    if ( val) {
        green_ap->power_save_enable = 1;   
        if ( (!sc->sc_invalid) && sc->sc_nvaps ) ath_green_ap_start(sc);
    }
    else
        ath_green_ap_stop(sc);    
}

int32_t ath_green_ap_sc_get_enable(struct ath_softc *sc)
{
    struct ath_green_ap* green_ap = sc->sc_green_ap;
    if ( green_ap == NULL ) return 0;
    return green_ap->power_save_enable;            
}

void ath_green_ap_sc_set_transition_time(struct ath_softc *sc, int32_t val )
{
    struct ath_green_ap* green_ap = sc->sc_green_ap;
    if ( green_ap == NULL ) return;
    green_ap->ps_trans_time = val;
}

int32_t ath_green_ap_sc_get_transition_time(struct ath_softc *sc)
{
    struct ath_green_ap* green_ap = sc->sc_green_ap;
    if ( green_ap == NULL ) return 0;
    return green_ap->ps_trans_time; 
}

void ath_green_ap_sc_set_on_time(struct ath_softc *sc, int32_t val )
{
    struct ath_green_ap* green_ap = sc->sc_green_ap;
    if ( green_ap == NULL ) return;
    green_ap->ps_on_time = val;
}

int32_t ath_green_ap_sc_get_on_time(struct ath_softc *sc)
{
    struct ath_green_ap* green_ap = sc->sc_green_ap;
    if ( green_ap == NULL ) return 0;
    return green_ap->ps_on_time; 
}
