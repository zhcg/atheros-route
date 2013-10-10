/*
 * Copyright (c) 2005 Atheros Communications, Inc.
 * All rights reserved.
 *
 */



#if  ATH_SUPPORT_AOW

#include "if_athvar.h"
#include "if_ath_aow.h"

#define DEFAULT_AOW_LATENCY_INMSEC  17

int ath_get_aow_macaddr(struct ieee80211com *ic, int channel, int index, struct ether_addr *macaddr)
{

    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    int ret = AH_FALSE;

    if ((channel < 0) || (channel > (AOW_MAX_AUDIO_CHANNELS - 1))) {
        return AH_FALSE;
    }        

    if (sc->sc_aow.chan_addr[channel].valid)  {
        memcpy(macaddr, &sc->sc_aow.chan_addr[channel].addr[index], sizeof(struct ether_addr));
        ret = AH_TRUE;
    }        

    return ret;
}

int ath_get_aow_chan_seqno(struct ieee80211com *ic, int channel, int* seqno)
{
    
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);

    if ((channel < 0) || (channel > (AOW_MAX_AUDIO_CHANNELS - 1))) {
        return AH_FALSE;
    }        

    if (sc->sc_aow.chan_addr[channel].valid) {
        *seqno = sc->sc_aow.chan_addr[channel].seqno++;
    }        

    return AH_TRUE;
}    


void ath_set_audio_channel(struct ieee80211com *ic, int channel, struct ether_addr *macaddr)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    int index = 0;

    if ((channel < 0) || (channel > (AOW_MAX_AUDIO_CHANNELS - 1))) {
        return;
    }        

    sc->sc_aow.chan_addr[channel].channel = channel;
    sc->sc_aow.chan_addr[channel].valid   = AH_TRUE;

    if ((sc->sc_aow.mapped_recv_cnt >= AOW_MAX_RECEIVER_COUNT) ||
        (sc->sc_aow.chan_addr[channel].dst_cnt >= AOW_MAX_RECEIVER_COUNT)) {
        printk("\nSet error : Max Limit reached\n");
        ath_list_audio_channel(ic);
        return;
    }

    index = sc->sc_aow.chan_addr[channel].dst_cnt;
    memcpy(&sc->sc_aow.chan_addr[channel].addr[index], macaddr, sizeof(struct ether_addr));
    sc->sc_aow.chan_addr[channel].dst_cnt++;
    sc->sc_aow.mapped_recv_cnt++;

    /* set the channel bit flag to optimize the check on transmit */
    ic->ic_aow.channel_set_flag = ic->ic_aow.channel_set_flag | (1 << channel);
}

void ath_list_audio_channel(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    int i = 0;
    int j = 0;

    for (i = 0; i < AOW_MAX_AUDIO_CHANNELS; i++) {
        if (sc->sc_aow.chan_addr[i].valid) {
            printk("\nAudio channel = %d\n", sc->sc_aow.chan_addr[i].channel);
            printk("-------------------\n");
            for (j = 0; j < sc->sc_aow.chan_addr[i].dst_cnt; j++) {
                printk("%s\n",ether_sprintf((char*)&sc->sc_aow.chan_addr[i].addr[j]));
            }                     
        }                    
    }                    
}

int ath_get_num_mapped_dst(struct ieee80211com *ic, int channel)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);

    return sc->sc_aow.chan_addr[channel].dst_cnt;
}    
    

void ath_clear_audio_channel_list(struct ieee80211com *ic, u_int32_t val)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    int i = 0;
    int j = 0;

    /* clear the mapped receiver count */
    sc->sc_aow.mapped_recv_cnt = 0;

    for (i = 0; i < AOW_MAX_AUDIO_CHANNELS; i++) {
        sc->sc_aow.chan_addr[i].channel = 0;
        sc->sc_aow.chan_addr[i].valid = AH_FALSE;
        sc->sc_aow.chan_addr[i].seqno = 0;
        sc->sc_aow.chan_addr[i].dst_cnt = 0;
        for (j = 0; j < AOW_MAX_RECEIVER_COUNT; j++)
            memset(&sc->sc_aow.chan_addr[i].addr[j], 0x00, sizeof(struct ether_addr));
    }        


    /* clear the set channel map */
    ic->ic_aow.channel_set_flag = 0;

}

void ath_set_swretries(struct ieee80211com *ic, u_int32_t val)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    sc->sc_aow.sw_retry_limit = val;
}

void ath_set_aow_playlocal(struct ieee80211com *ic, u_int32_t val)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    sc->sc_aow.playlocal = val;

    if (val) {
        aow_i2s_init(ic);
    } else {
        aow_i2s_deinit(ic);
        CLR_I2S_STOP_FLAG(ic->ic_aow.i2s_flags);
    }
}

u_int32_t ath_get_swretries(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    return sc->sc_aow.sw_retry_limit;

}

void ath_set_aow_er(struct ieee80211com *ic, u_int32_t val)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    
    sc->sc_aow.er = val ? 1:0;

    /* ER not supported in this release */
    printk("Error Recovery : Not supported\n");
    sc->sc_aow.er = 0;
}

u_int32_t ath_get_aow_er(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    return sc->sc_aow.er;
}

void ath_set_aow_ec(struct ieee80211com *ic, u_int32_t val)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    sc->sc_aow.ec = val ? 1:0;
}

u_int32_t ath_get_aow_ec(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    return sc->sc_aow.ec;
}

void ath_set_aow_ec_fmap(struct ieee80211com *ic, u_int32_t val)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    sc->sc_aow.ec_fmap = val;
}

u_int32_t ath_get_aow_ec_fmap(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    return sc->sc_aow.ec_fmap;
}

void ath_set_aow_es(struct ieee80211com *ic, u_int32_t val)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    int retval;

    if (val) {
        if (sc->sc_aow.ess) {
            ic->ic_set_aow_ess(ic, 0);
        }

        if ((retval = aow_es_base_init(ic)) < 0) {
            IEEE80211_AOW_DPRINTF("Ext Stats Init failed. Turning off ES\n");
            sc->sc_aow.es = 0;
        } else {
            sc->sc_aow.es = 1;
        }

    } else {
        aow_es_base_deinit(ic);
        sc->sc_aow.es = 0;
    }
}

u_int32_t ath_get_aow_es(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    return sc->sc_aow.es;
}

void ath_set_aow_ess(struct ieee80211com *ic, u_int32_t val)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    int retval;

    if (val) {
        if (sc->sc_aow.es) {
            ic->ic_set_aow_es(ic, 0);
        }

        if ((retval = aow_es_base_init(ic)) < 0) {
            IEEE80211_AOW_DPRINTF("Ext Stats Init failed. Turning off ESS\n");
            sc->sc_aow.ess = 0;
        } else {
            aow_essc_init(ic);
            sc->sc_aow.ess = 1;
        }
    } else {
        aow_es_base_deinit(ic);
        aow_essc_init(ic);
        sc->sc_aow.ess = 0;
    }
}

u_int32_t ath_get_aow_ess(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    return sc->sc_aow.ess;
}

void ath_set_aow_ess_count(struct ieee80211com *ic, u_int32_t val)
{
    aow_essc_setall(ic, val);
}

void ath_aow_clear_estats(struct ieee80211com *ic)
{
    aow_clear_ext_stats(ic);
}

u_int32_t ath_aow_get_estats(struct ieee80211com *ic)
{
    aow_print_ext_stats(ic);
    return 0;
}

void ath_aow_record_txstatus(struct ieee80211com *ic, struct ath_buf *bf, struct ath_tx_status *ts)
{
    aow_update_txstatus(ic, bf, ts);
}

void ath_set_aow_latency(struct ieee80211com *ic, u_int32_t val)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);

    if (!ic->ic_aow.ctrl.audStopped) {
       IEEE80211_AOW_DPRINTF("Device busy\n");
       return; 
    }

    sc->sc_aow.latency = val;
    sc->sc_aow.latency_us = val * 1000;
    
    if (AOW_ES_ENAB(ic)) {
        //Reset ES
        ic->ic_set_aow_es(ic, 0);
        ic->ic_set_aow_es(ic, 1);
    } else if (AOW_ESS_ENAB(ic)) {
        //Reset ESS
        ic->ic_set_aow_ess(ic, 0);
        ic->ic_set_aow_ess(ic, 1);
    }
}

u_int32_t ath_get_aow_playlocal(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    return sc->sc_aow.playlocal;
}

u_int32_t ath_get_aow_latency(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    return sc->sc_aow.latency;
}

u_int64_t ath_get_aow_latency_us(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    return sc->sc_aow.latency_us;
}

u_int64_t
ath_get_aow_tsf_64(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    return scn->sc_ops->ath_get_tsf64(scn->sc_dev);
}    

void if_ath_start_aow_timer(struct ieee80211com *ic, u_int32_t startTime, u_int32_t period)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->ath_start_aow_timer(scn->sc_dev, startTime, period); 
}

void if_ath_stop_aow_timer(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->ath_stop_aow_timer(scn->sc_dev);
}

void if_ath_gpio11_toggle(struct ieee80211com *ic, u_int16_t flg)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->ath_gpio11_toggle_ptr(scn->sc_dev, flg);
}


void if_ath_aow_proc_timer_start(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->ath_aow_proc_timer_start(scn->sc_dev);    
}

void if_ath_aow_proc_timer_stop(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    scn->sc_ops->ath_aow_proc_timer_stop(scn->sc_dev);    
}

void ath_aow_reset(struct ieee80211com *ic)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    ath_internal_reset(sc);
}

extern void ieee80211_send2all_nodes(void *reqvap, void *data, int len, u_int32_t seqno, u_int64_t tsf);

void ath_aow_attach(struct ieee80211com* ic, struct ath_softc_net80211 *scn)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
    ic->ic_set_swretries = ath_set_swretries;
    ic->ic_get_swretries = ath_get_swretries;
    ic->ic_set_aow_latency = ath_set_aow_latency;
    ic->ic_get_aow_latency = ath_get_aow_latency;
    ic->ic_get_aow_latency_us = ath_get_aow_latency_us;
    ic->ic_get_aow_tsf_64 = ath_get_aow_tsf_64;
    ic->ic_start_aow_inter = if_ath_start_aow_timer;
    ic->ic_stop_aow_inter = if_ath_stop_aow_timer;
    ic->ic_gpio11_toggle = if_ath_gpio11_toggle;
    ic->ic_set_audio_channel = ath_set_audio_channel;
    ic->ic_get_num_mapped_dst = ath_get_num_mapped_dst;
    ic->ic_get_aow_macaddr = ath_get_aow_macaddr;
    ic->ic_get_aow_chan_seqno = ath_get_aow_chan_seqno;
    ic->ic_list_audio_channel = ath_list_audio_channel;
    ic->ic_set_aow_playlocal = ath_set_aow_playlocal;
    ic->ic_get_aow_playlocal = ath_get_aow_playlocal;
    ic->ic_aow_clear_audio_channels = ath_clear_audio_channel_list;
    ic->ic_aow_proc_timer_start = if_ath_aow_proc_timer_start;
    ic->ic_aow_proc_timer_stop = if_ath_aow_proc_timer_stop;
    ic->ic_set_aow_er = ath_set_aow_er;
    ic->ic_get_aow_er = ath_get_aow_er;
    ic->ic_set_aow_ec = ath_set_aow_ec;
    ic->ic_get_aow_ec = ath_get_aow_ec;
    ic->ic_set_aow_ec_fmap = ath_set_aow_ec_fmap;
    ic->ic_get_aow_ec_fmap = ath_get_aow_ec_fmap;
    ic->ic_set_aow_es = ath_set_aow_es;
    ic->ic_get_aow_es = ath_get_aow_es;
    ic->ic_set_aow_ess = ath_set_aow_ess;
    ic->ic_get_aow_ess = ath_get_aow_ess;
    ic->ic_set_aow_ess_count = ath_set_aow_ess_count;
    ic->ic_aow_clear_estats = ath_aow_clear_estats;
    ic->ic_aow_get_estats = ath_aow_get_estats;
    ic->ic_aow_record_txstatus = ath_aow_record_txstatus;
    ic->ic_aow_reset = ath_aow_reset;
    sc->sc_aow.latency = DEFAULT_AOW_LATENCY_INMSEC;
    sc->sc_aow.latency_us = sc->sc_aow.latency * 1000;

}


void ath_net80211_aow_send2all_nodes(void *reqvap,
                                     void *data, int len, u_int32_t seqno,  u_int64_t tsf)
{
    ieee80211_send2all_nodes(reqvap, data, len, seqno, tsf);
}

int ath_net80211_aow_consume_audio_data(ieee80211_handle_t ieee, u_int64_t tsf)
{
    struct ieee80211com *ic = NET80211_HANDLE(ieee);
    return ieee80211_consume_audio_data( ic, tsf);
}

void ath_aow_l2pe_record(struct ieee80211com *ic, bool is_success)
{
    aow_l2pe_record(ic, is_success);
}


u_int32_t ath_aow_chksum_crc32 (unsigned char *block, unsigned int length)
{
    return chksum_crc32(block, length);
}

u_int32_t ath_aow_cumultv_chksum_crc32(u_int32_t crc_prev,
                                       unsigned char *block,
                                       unsigned int length)
{
    return cumultv_chksum_crc32(crc_prev, block, length);
}
#endif  /* ATH_SUPPORT_AOW */
