#ifndef _IEEE80211_SMARTANTENNA_H_
#define _IEEE80211_SMARTANTENNA_H_

#ifndef UMAC_SUPPORT_SMARTANTENNA
#define UMAC_SUPPORT_SMARTANTENNA 0
#endif

#define MAX_SMART_ANTENNAS 8
#define SMART_ANTENNA_TX 0
#define SMART_ANTENNA_RX 1

#define NUM_PKTS_TOFLUSH_FACTOR 4   /* use 1/4 of total free buffers */
#define SMARTANT_MAX_RATESETS 3  /* Max rate sets based on number of stream rates*/
#define MAX_RATES_PERSET 7  
#define MAX_HT_RATES 24 
#define SMARTANT_TRAIN_TID 1 /* Use BK acces category for Training and use TID 1 */
#define RETRAIN_INTERVEL 30000  /* 30 sec */
#define RETRAIN_PKT_THRESHOLD 1000
#define RETRAIN_MISS_THRESHOLD 4
#define DEFAULT_SCHEDULE_DELAY_FACTOR 100
#define PRETRAIN_DELAY 75000 /* micro sec */
#define PRETRAIN_PACKETS 200 
#define SA_PERDIFF_THRESHOLD 3

#define TRAIN_TYPE_FRAME    1
#define TRAIN_TYPE_PROTOCOL 2  
/* State Variable */
#define SMARTANTENNA_NODE_DEFAULT 0x00
#define SMARTANTENNA_TRAIN_INPROGRESS 0x81 
#define SMARTANTENNA_TRAIN_MOVE_NEXTRATE 0x82
#define SMARTANTENNA_TRAIN_DONE 0x83
#define SMARTANTENNA_NODE_BAD 0x84 /* BAD STA where we are not able to send any packet i.e nFrames it self is Zero */
#define SMARTANTENNA_NODE_PRETRAIN 0x80  /* pretrain */
#define SMARTANTENNA_NODE_PRETRAIN_INPROGRESS 0x85  /* If d to do pretrain */


/* protocol numbers */
#define SAMRTANT_SET_ANTENNA 3
#define SAMRTANT_SEND_RXSTATS 4
#define SAMRTANT_RECV_RXSTATS 5


struct ieee80211_smartantenna_params {
    u_int8_t training_mode; /* Implicit & Explicit */
    u_int8_t training_type; /* Frame based or Protocol based */
    u_int32_t num_of_packets;
    u_int32_t packetlen;
    u_int8_t training_start;
    u_int8_t num_of_iterations;
    u_int8_t automation_enable;
    u_int8_t retraining_enable;
    u_int8_t schedule_delay;
    u_int8_t num_tx_antennas;
};

#if UMAC_SUPPORT_SMARTANTENNA
void ieee80211_smartantenna_set_param(wlan_if_t vaphandle, ieee80211_param param, u_int32_t val);
int  ieee80211_smartantenna_attach(struct ieee80211com *ic);
int  ieee80211_smartantenna_detach(struct ieee80211com *ic);
void ieee80211_smartantenna_training_init(struct ieee80211vap *vap);
int ieee80211_smartantenna_start_training(struct ieee80211_node *ni, struct ieee80211com *ic);
u_int32_t ieee80211_smartantenna_get_param(wlan_if_t vaphandle, ieee80211_param param);
int ieee80211_smartantenna_train_node(struct ieee80211_node *ni, u_int8_t antenna ,u_int8_t rateidx);
void smartantenna_sm(struct ieee80211_node *ni);
void ieee80211_set_current_antenna(struct ieee80211vap *vap, u_int32_t antenna);
void smartantenna_prepare_rateset (struct ieee80211_node *ni);
void smartantenna_state_init(struct ieee80211_node *ni, u_int8_t rateCode);
void smartantenna_state_save(struct ieee80211_node *ni , u_int8_t antenna , u_int32_t itr, u_int32_t pending);
void smartantenna_state_restore(struct ieee80211_node *ni);
void smartantenna_setdefault_antenna(struct ieee80211_node *ni, u_int8_t antenna);
void ieee80211_smartantenna_training_init(struct ieee80211vap *vap);
void ieee80211_smartantenna_input(struct ieee80211vap *vap, wbuf_t wbuf, struct ether_header *eh, struct ieee80211_rx_status *rs);
void ieee80211_smartantenna_rx_proto_msg(struct ieee80211vap *vap, wbuf_t mywbuf, struct ieee80211_rx_status *rs);
void smartantenna_automation_sm(struct ieee80211_node *ni);
int ieee80211_smartantenna_tx_proto_msg(struct ieee80211vap *vap ,struct ieee80211_node *ni , u_int8_t msg_num, u_int8_t txantenna, u_int8_t rxantenna, u_int8_t ratecode);
void smartantenna_display_goodput(struct ieee80211vap *vap);
int8_t smartantenna_check_optimize(struct ieee80211_node *ni, u_int8_t rateCode);
u_int8_t sm_get_maxrtsets(struct ieee80211_node * ni);
int smartantenna_retrain_check(struct ieee80211_node *ni, int mode);
int ieee80211_smartantenna_init_training(struct ieee80211_node *ni, struct ieee80211com *ic);
#else
static inline int  ieee80211_smartantenna_attach(struct ieee80211com *ic)
{
    return 0;
}
static inline int  ieee80211_smartantenna_detach(struct ieee80211com *ic)
{
    return 0;
}

#define ieee80211_smartantenna_set_param(vaphandle, param, val) /**/
#define ieee80211_smartantenna_get_param(vaphandle, param) /**/
#define ieee80211_smartantenna_input(vap, wbuf, eh, rs) /**/
#endif

#endif
