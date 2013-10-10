#include <osdep.h>
#include <ieee80211_smartantenna_priv.h>
#include <ieee80211_smartantenna.h>

#define SMARTANTENNA_DEBUG 1
static void smartantenna_training_sm(void *data);
#define WQNAME(name) #name

#if UMAC_SUPPORT_SMARTANTENNA
char ant_buf[8][4] = {"024","124","034","134","025","125","035","135"};
u_int8_t txantenna = 0, rxantenna = 0;
u_int8_t rxratecode=0;
u_int32_t rx_smframes[8][8][MAX_HT_RATES];
struct per_ratetable rxpermap[8];
struct per_ratetable receiver_rxpermap[8];
u_int32_t train_ts = 0, temp_ts =0;

/* Function that acts as an interface to packet log. Used to log packet losss information */
void 
ieee80211_smartantenna_logtext(struct ieee80211com *ic, const char *fmt, ...) 
{
     char                   tmp_buf[OS_TEMP_BUF_SIZE];  
     va_list                ap;             
     va_start(ap, fmt);                                         
     vsnprintf (tmp_buf, OS_TEMP_BUF_SIZE, fmt, ap);            
     va_end(ap);                                                                                  
     ic->ic_log_text(ic, tmp_buf);             
}
void
ieee80211_smartantenna_set_param(wlan_if_t vaphandle, ieee80211_param param, u_int32_t val)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic; 

    switch (param) {
        case  IEEE80211_SMARTANT_TRAIN_MODE:
            ic->ic_smartantennaparams->training_mode = val;            
            break;
        case IEEE80211_SMARTANT_TRAIN_TYPE:
            ic->ic_smartantennaparams->training_type = val;
            break;
        case IEEE80211_SMARTANT_PKT_LEN:
             ic->ic_smartantennaparams->packetlen = val;
             break;
        case IEEE80211_SMARTANT_NUM_PKTS:
             ic->ic_smartantennaparams->num_of_packets = val;
             break;
        case IEEE80211_SMARTANT_TRAIN_START:
             if (ic->ic_get_smartatenna_enable(ic))
             {
                 ic->ic_smartantennaparams->training_start = val;
                 ieee80211_smartantenna_training_init(vap);
             }
             else
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,"%s:smart antenna is disabled !!! \n", __func__);

             break;
        case IEEE80211_SMARTANT_NUM_ITR:
             ic->ic_smartantennaparams->num_of_iterations = val;
             break;
        case IEEE80211_SMARTANT_CURRENT_ANTENNA:
             ieee80211_set_current_antenna(vap, val);
             break;
        case IEEE80211_SMARTANT_DEFAULT_ANTENNA:
            /* printk(" setting RX antenna combination to :%d [%s] \n", val , ant_buf[val]);*/
             ic->ic_set_default_antenna(ic, val);
             ic->ic_default_rx_ant= val;
             
             /*if we are running in STA Mode then default antenna is current antenna */
             if ((vap->iv_opmode == IEEE80211_M_STA))
                 ieee80211_set_current_antenna(vap, val);
             break;
        case IEEE80211_SMARTANT_AUTOMATION:
             ic->ic_smartantennaparams->automation_enable = val;
             break;
        case IEEE80211_SMARTANT_RETRAIN:
             if (ic->ic_get_smartatenna_enable(ic))
                 ic->ic_smartantennaparams->retraining_enable = val;
             else
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,"%s: smart antenna is disabled !!! \n", __func__);
             break;
        default:
            break;
    }
}

u_int32_t
ieee80211_smartantenna_get_param(wlan_if_t vaphandle, ieee80211_param param)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;  
    u_int32_t val = 0;

    switch (param) {
        case  IEEE80211_SMARTANT_TRAIN_MODE:
            val = ic->ic_smartantennaparams->training_mode;
            break;
        case IEEE80211_SMARTANT_TRAIN_TYPE:
            val = ic->ic_smartantennaparams->training_type;
            break;
        case IEEE80211_SMARTANT_PKT_LEN:
             val = ic->ic_smartantennaparams->packetlen;
             break;
        case IEEE80211_SMARTANT_NUM_PKTS:
             val = ic->ic_smartantennaparams->num_of_packets;
             break;
        case IEEE80211_SMARTANT_TRAIN_START:
             val = ic->ic_smartantennaparams->training_start;
             break;
        case IEEE80211_SMARTANT_NUM_ITR:
             val = ic->ic_smartantennaparams->num_of_iterations;
             break;
        case IEEE80211_SMARTANT_CURRENT_ANTENNA:
             /* TODO : display the current antenna*/
             break;
        case IEEE80211_SMARTANT_DEFAULT_ANTENNA:
             /* TODO display the default antenna*/
             break;
        case IEEE80211_SMARTANT_AUTOMATION:
             val = ic->ic_smartantennaparams->automation_enable;
             break;
        case IEEE80211_SMARTANT_RETRAIN:
             val = ic->ic_smartantennaparams->retraining_enable;
             break;
        default:
            break;
    }
    return val;
}

/* init smart antenna training state for node */
void smartantenna_state_init(struct ieee80211_node *ni,u_int8_t rateCode)
{
    u_int8_t rtset = rateCode&0x7f;
    int i=0;
    u_int8_t max_rtsets;
    rtset = (rtset & 070)>>3;
    max_rtsets = sm_get_maxrtsets(ni);
    /* Find rate index for the given rate */
    for (i=0;i<MAX_RATES_PERSET;i++)
    {
        if (rateCode == ni->rtset[rtset].rates[i].ratecode)
            break;
    }

    /* If we are in the edge of rate for a rateset then try the above rate set's last rate*/
    if (i == 0)
    {
        if(rtset < max_rtsets-1)
        {
            rtset = rtset+1;
            i = ni->rtset[rtset].num_of_rates;
        }
    }

    ni->train_state.rateset = rtset;
    ni->train_state.rateidx = i>0?i-1:0;
    ni->train_state.antenna = 0;
    ni->train_state.iteration = 0;
    ni->train_state.pending_packets=0;
    ni->smartantenna_state = SMARTANTENNA_TRAIN_INPROGRESS;
}

/* save  smart antenna training state for node */
void smartantenna_state_save(struct ieee80211_node *ni , u_int8_t antenna , u_int32_t itr, u_int32_t pending)
{
    ni->train_state.antenna = antenna;
    ni->train_state.iteration = itr;
    ni->train_state.pending_packets = pending;
}

/* restore smart antenna training state for node */
void smartantenna_state_restore(struct ieee80211_node *ni)
{
    ni->current_rate_index = ni->rtset[ni->train_state.rateset].rates[ni->train_state.rateidx].rateindex;
    ni->current_tx_antenna = ni->train_state.antenna;
    ni->iteration = ni->train_state.iteration;
    ni->pending_pkts = ni->train_state.pending_packets;
}


/* This function is used to check the incoming packets for ATH_ETH_TYPE packets (0x88BD) & look
   for custom smart antenna packets 
*/
void
ieee80211_smartantenna_input(struct ieee80211vap *vap, wbuf_t wbuf, struct ether_header *eh, struct ieee80211_rx_status *rs)
{
    struct athl2p_tunnel_hdr *tunHdr;
    struct ieee80211com *ic = vap->iv_ic;
    if (!ic->ic_smartantenna_init_done) return;

    if (eh->ether_type == ATH_ETH_TYPE) 
    {
        wbuf_pull(wbuf, (u_int16_t) (sizeof(*eh)));
        tunHdr = (struct athl2p_tunnel_hdr *)(wbuf_header(wbuf));
        switch (tunHdr->proto)
        {
            case ATH_ETH_TYPE_SMARTANTENNA_PROTO:
                ieee80211_smartantenna_rx_proto_msg(vap, wbuf, rs);
                break;
    
            case ATH_ETH_TYPE_SMARTANTENNA_TRAINING:
                                
            if (ic->ic_smartantennaparams->automation_enable)
            {
                rx_smframes[rxantenna][txantenna][rxratecode&0x7f]++;
                rxpermap[txantenna].nframes++;
                if ((rs->rs_rssictl[0]!= 0) && (rs->rs_rssictl[1]!= 0) && (rs->rs_rssictl[2]!= 0))
                {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_SMARTANT,"SenderTXAntenna: %s RecieverTXAntenna: %s RecieverDefaultAntenna: %s RateCode: 0x%02x RSSI: %02d %02d %02d TotalRecieved: %04d\n" 
                                                                    ,ant_buf[txantenna],ant_buf[vap->iv_bss->current_tx_antenna], ant_buf[rxantenna], rxratecode
                                                                    , rs->rs_rssictl[0]
                                                                    , rs->rs_rssictl[1]
                                                                    , rs->rs_rssictl[2],rx_smframes[rxantenna][txantenna][rxratecode&0x7f]);

                    rxpermap[txantenna].chain0_rssi += rs->rs_rssictl[0];
                    rxpermap[txantenna].chain1_rssi += rs->rs_rssictl[1];
                    rxpermap[txantenna].chain2_rssi += rs->rs_rssictl[2];
                }
            }
                break;

              default:
                break;     
        }

        wbuf_push(wbuf, (u_int16_t) (sizeof(*eh)));
    }
}

void ieee80211_smartantenna_rx_proto_msg(struct ieee80211vap *vap, wbuf_t mywbuf, struct ieee80211_rx_status *rs)
{
    u_int8_t *payload;

#if DBG_PROTO               
    u_int8_t i
#endif        
    struct ieee80211com *ic = vap->iv_ic;
    static u_int8_t prev_rxantenna = 111;
    u_int8_t command;
    payload = (u_int8_t *)wbuf_raw_data(mywbuf);
    command = payload[4];

    switch(command)
    {
        case SAMRTANT_SET_ANTENNA:
        
            txantenna  = payload[5];
            rxantenna  = payload[6];
            rxratecode = payload[7];
       
            /* In automation we need to change RX antenna of receiver */ 
            if (ic->ic_smartantennaparams->automation_enable)
            {
    
                if (prev_rxantenna != rxantenna)
                {
                    ic->ic_set_default_antenna(ic, rxantenna);
                    prev_rxantenna = rxantenna;
                }
                OS_DELAY(HZ/4);
            }
                break;
            
        case SAMRTANT_SEND_RXSTATS:
             /* recieved send stats msg from sender*/       
#if DBG_PROTO               
             printk(" Sending RX stats %s \n",__func__);
             for(i=0;i<8;i++)
             {
                  printk("Ant: %d : frms: %d : rssi: %d %d %d \n",
                  i ,rxpermap[i].nframes,rxpermap[i].chain0_rssi,rxpermap[i].chain1_rssi,rxpermap[i].chain2_rssi);
             }
#endif             
             /* Send receive stats msg to sender*/       
             ieee80211_smartantenna_tx_proto_msg(vap,vap->iv_bss,SAMRTANT_RECV_RXSTATS,0,0,0);
             /* clear rxpermap to collecte stats for other combinations*/
             OS_MEMSET(&rxpermap, 0, sizeof(rxpermap));  
             break;

        case SAMRTANT_RECV_RXSTATS:
             /* recieved stats from Receiver */
            /* vicks: todo need o optimize for memory etc ...
               copy the date in temporary Global buffer for now */
           OS_MEMSET(&receiver_rxpermap, 0, sizeof(receiver_rxpermap));  
           OS_MEMCPY((u_int8_t *)&receiver_rxpermap,(u_int8_t *)&payload[5],sizeof(receiver_rxpermap)); 
#if DBG_PROTO
           printk(" ==== Receving RX stats %s ====\n",__func__);
           for(i=0;i<8;i++)
               printk("Ant: %d : frms: %d : rssi: %d %d %d \n",
                   i ,receiver_rxpermap[i].nframes,receiver_rxpermap[i].chain0_rssi,receiver_rxpermap[i].chain1_rssi,receiver_rxpermap[i].chain2_rssi);
#endif           
           break;
          
        default:
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: recieved UNKNOWN Message \n",__func__);
    }
}

int ieee80211_smartantenna_tx_custom_proto_pkt(struct ieee80211vap *vap, struct custom_pkt_params *cpp)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ether_header *eh;			
    struct athl2p_tunnel_hdr *tunHdr;		
    int    msg_len;
    wbuf_t mywbuf;
    struct net_device *netd;
    struct ieee80211_node *ni;
    int ret;
    ni = ieee80211_find_node(&ic->ic_sta, cpp->dst_mac_address);
    
    if (ni == NULL) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: Not able to find node for tx \n", __func__);
        ieee80211_free_node(ni); 
        return -1;
    }

    netd = (struct net_device *) wlan_vap_get_registered_handle(vap);

    /* Set msg length to the largest protocol msg payload size */
    msg_len = cpp->num_data_words; 

    mywbuf = wbuf_alloc(ic->ic_osdev,  WBUF_TX_DATA, msg_len);

    if (!mywbuf) {
        printk("wbuf allocation failed in tx_custom_pkt\n");
        ieee80211_free_node(ni); 
        return -1;
    }
    /* Copy protocol messsage data */
    wbuf_append(mywbuf,msg_len);
    OS_MEMCPY((u_int8_t *)(wbuf_raw_data(mywbuf)), cpp->msg_data, msg_len); 

    tunHdr = (struct athl2p_tunnel_hdr *) wbuf_push(mywbuf, sizeof(struct athl2p_tunnel_hdr));
    eh     = (struct ether_header *) wbuf_push(mywbuf, sizeof(struct ether_header));

    /* ATH_ETH_TYPE protocol subtype */
    tunHdr->proto = ATH_ETH_TYPE_SMARTANTENNA_PROTO;
    /* Copy the SRC & DST MAC addresses into ethernet header*/ 
    IEEE80211_ADDR_COPY(&eh->ether_shost[0], cpp->src_mac_address);
    IEEE80211_ADDR_COPY(&eh->ether_dhost[0], cpp->dst_mac_address);
    /* copy ethertype */
    eh->ether_type = htons(ATH_ETH_TYPE);
    mywbuf->dev = netd;
    ret = ieee80211_send_wbuf(vap, ni, mywbuf);
    ieee80211_free_node(ni); 
    return 0;
}   

/* This function is used to send training data packets using the Atheros custom
   88bd protocol. Packet structure:
    802.3 HDR + ATHHDR +  Payload 
        14        4        1500 
*/
int 
ieee80211_smartantenna_tx_custom_data_pkt(struct ieee80211_node *ni , struct ieee80211vap *vap, struct custom_pkt_params *cpp,
                                          u_int8_t antenna, u_int8_t rateidx, u_int8_t is_last)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ether_header *eh;			
    struct athl2p_tunnel_hdr *tunHdr;		
    u_int  msg_len;
    wbuf_t mywbuf;
    struct net_device *netd;

    netd = (struct net_device *) wlan_vap_get_registered_handle(vap);

    msg_len = ic->ic_smartantennaparams->packetlen; 

    mywbuf = wbuf_alloc(ic->ic_osdev,  WBUF_TX_DATA, msg_len);

    if (!mywbuf) {
        printk("wbuf allocation failed in tx_custom_pkt\n");
        return -1;
    }

    IEEE80211_ADDR_COPY(cpp->dst_mac_address,ni->ni_macaddr);

    /* In AP Mode */
    if ((vap->iv_opmode == IEEE80211_M_HOSTAP))
    {
        IEEE80211_ADDR_COPY(cpp->src_mac_address, ni->ni_bssid);
    }
    else if ((vap->iv_opmode == IEEE80211_M_STA))
    {
        /* In STA Mode */
        IEEE80211_ADDR_COPY(cpp->src_mac_address,vap->iv_myaddr);
    }


    /* Initialize */
    wbuf_append(mywbuf, msg_len);
    memset((u_int8_t *)wbuf_raw_data(mywbuf), 0, msg_len);

    tunHdr = (struct athl2p_tunnel_hdr *) wbuf_push(mywbuf, sizeof(struct athl2p_tunnel_hdr));
    eh     = (struct ether_header *) wbuf_push(mywbuf, sizeof(struct ether_header));

    tunHdr->proto = ATH_ETH_TYPE_SMARTANTENNA_TRAINING;

    IEEE80211_ADDR_COPY(&eh->ether_shost[0], cpp->src_mac_address);
    IEEE80211_ADDR_COPY(&eh->ether_dhost[0], cpp->dst_mac_address);

    eh->ether_type = htons(ATH_ETH_TYPE);
    mywbuf->dev = netd;
    if (rateidx)
    {
        /* set training parameters in wbuf */
        wbuf_sa_set_antenna(mywbuf, antenna);
        wbuf_sa_set_rateidx(mywbuf, rateidx);
        wbuf_sa_set_train_packet(mywbuf);
        if (is_last)
        {
            wbuf_sa_set_train_lastpacket(mywbuf);
        }

    }
    wbuf_set_priority(mywbuf,WME_AC_BK);
    wbuf_set_tid(mywbuf,SMARTANT_TRAIN_TID);

    ieee80211_send_wbuf(vap, ni, mywbuf);  
    return 0;
}

/* This function is used to construct a custom protocol message for transmission.
   It populates the custom_pkt_params structure and hands it over to the
   ieee80211_smartantenna_tx_custom_proto_pkt function */
int ieee80211_smartantenna_tx_proto_msg(struct ieee80211vap *vap, struct ieee80211_node *ni , u_int8_t msg_num, u_int8_t txantenna, u_int8_t rxantenna, u_int8_t ratecode)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct custom_pkt_params cpp; 
    /* In AP Mode */
    if ((vap->iv_opmode == IEEE80211_M_HOSTAP))
    {
        IEEE80211_ADDR_COPY(cpp.dst_mac_address,ni->ni_macaddr);
        IEEE80211_ADDR_COPY(cpp.src_mac_address, ni->ni_bssid);
    }
    else if ((vap->iv_opmode == IEEE80211_M_STA))
    {
        /* In STA Mode */
        IEEE80211_ADDR_COPY(cpp.dst_mac_address,ni->ni_macaddr);
        IEEE80211_ADDR_COPY(cpp.src_mac_address,vap->iv_myaddr);
    }

    cpp.msg_num = msg_num; 

    switch (msg_num) 
    {
        case SAMRTANT_SET_ANTENNA: 
         cpp.antenna = rxantenna;
         cpp.num_data_words = 6;
         cpp.msg_data    = (u_int8_t *) OS_MALLOC(ic->ic_osdev, sizeof(u_int8_t)*cpp.num_data_words, GFP_KERNEL);
         cpp.msg_data[0] = msg_num;
         cpp.msg_data[1] = txantenna;
         cpp.msg_data[2] = rxantenna;
         cpp.msg_data[3] = ratecode;
            break;
        case SAMRTANT_RECV_RXSTATS: 
         cpp.num_data_words = sizeof(rxpermap)+sizeof(u_int32_t);
         cpp.msg_data    = (u_int8_t *) OS_MALLOC(ic->ic_osdev, sizeof(u_int8_t)*cpp.num_data_words, GFP_KERNEL);
         cpp.msg_data[0] = msg_num;
         OS_MEMCPY((u_int8_t *)&cpp.msg_data[1],(u_int8_t *)&rxpermap,sizeof(rxpermap)); 
            break;
        case SAMRTANT_SEND_RXSTATS: 
         cpp.antenna = rxantenna;
         cpp.num_data_words = 6;
         cpp.msg_data    = (u_int8_t *) OS_MALLOC(ic->ic_osdev, sizeof(u_int8_t)*cpp.num_data_words, GFP_KERNEL);
         cpp.msg_data[0] = msg_num;
         cpp.msg_data[1] = txantenna;
         cpp.msg_data[2] = rxantenna;
         cpp.msg_data[3] = ratecode;
            break;
        default:
            /* need to exit from the func */
            cpp.num_data_words = 0;
            break;
    }
    if (cpp.num_data_words == 0)
        return -1;

    /*
     * send protocol packet with rate control reliable rate
     */
    ieee80211_smartantenna_tx_custom_proto_pkt(vap, &cpp);

    if (cpp.msg_data != NULL) {
         OS_FREE(cpp.msg_data);
    }

    return 0;
}

int ieee80211_smartantenna_train_node(struct ieee80211_node *ni, u_int8_t antenna ,u_int8_t rateidx)
{
    struct ieee80211com *ic = ni->ni_ic;
    u_int32_t loop_count = 0, temp = 0 ,tosend = 0; 
    struct custom_pkt_params cpp;
    IEEE80211_ADDR_COPY(cpp.dst_mac_address,ni->ni_macaddr);
    IEEE80211_ADDR_COPY(cpp.src_mac_address, ni->ni_bssid);

    ni->smartantenna_state = SMARTANTENNA_TRAIN_INPROGRESS;

    /* TODO: number of training packets need to be queued in hardware queue can be tuned
     *       currently sending 25% of current txbuf_free available 
     */
    tosend = (ic->ic_get_txbuf_free(ic))/NUM_PKTS_TOFLUSH_FACTOR;

     /* Send training pkts*/
    for (loop_count = ni->pending_pkts; loop_count < ic->ic_smartantennaparams->num_of_packets-1; loop_count++) {

            if(temp == tosend-1)
                  break;

            ieee80211_smartantenna_tx_custom_data_pkt(ni, ni->ni_vap, &cpp, antenna, rateidx, 0);
           temp++;
    }

    ieee80211_smartantenna_tx_custom_data_pkt(ni, ni->ni_vap, &cpp, antenna, rateidx, 1);
    temp++;
    loop_count++;

    if (loop_count == ic->ic_smartantennaparams->num_of_packets)
        return 0;

    if (temp >= tosend)
       return tosend;

    return 0;
}

void smartantenna_reset_permap(struct ieee80211_node * ni)
{
    int i = 0,j = 0;
    for (i=0;i< MAX_SMART_ANTENNAS;i++)
    {
        for (j=0;j<MAX_HT_RATES;j++)
        {
            ni->permap[i][j].nframes = 0;
            ni->permap[i][j].nbad = 0;
            ni->permap[i][j].chain0_rssi = 0;
            ni->permap[i][j].chain1_rssi = 0;
            ni->permap[i][j].chain2_rssi = 0;
        }
    }   
}

u_int8_t sm_get_maxrtsets(struct ieee80211_node *ni)
{   int i=0;
    /* currently we have max 3 ratesets*/
    for(i=SMARTANT_MAX_RATESETS-1;i>=0;i--)
    {
        if(ni->rtset[i].num_of_rates)
            break;
            
    }
    return i+1;
}

/* Pre training retruns the rate code on which current node can operate with assistance from RCA*/
u_int8_t ieee80211_smartantenna_pretraining(struct ieee80211_node *ni)
{
    struct custom_pkt_params cpp;
    int i=0;
    u_int8_t rateCode;
    u_int32_t  ratestats[MAX_HT_RATES];
    ni->ni_ic->ic_get_smartantenna_ratestats(ni, (void *)&ratestats[0]);

    IEEE80211_ADDR_COPY(cpp.dst_mac_address,ni->ni_macaddr);
    IEEE80211_ADDR_COPY(cpp.src_mac_address, ni->ni_bssid);

    IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_SMARTANT,"Pre Training start:%s 0x%08x \n",__func__ ,ni->ni_ic->ic_get_TSF32(ni->ni_ic));
    for (i = 0; i < 5; i++) {
        ieee80211_smartantenna_tx_custom_data_pkt(ni, ni->ni_vap, &cpp, 0, 0, 0);
    }
    /* Fix for EV#95249. Inserting delay to get BA negotiation done before sending BA window
       size of packets.*/ 
    for (i = 0; i < 8; i++) {
        OS_SLEEP(PRETRAIN_DELAY);
    }
    smartantenna_retrain_check(ni, 0);

    /* Send training pkts*/
    for (i = 0; i < PRETRAIN_PACKETS; i++) {
        ieee80211_smartantenna_tx_custom_data_pkt(ni, ni->ni_vap, &cpp, 0, 0, 0);
    } 
    OS_SLEEP(PRETRAIN_DELAY);
    smartantenna_retrain_check(ni, 0);
    
    for (i = 0; i < PRETRAIN_PACKETS; i++) {
        ieee80211_smartantenna_tx_custom_data_pkt(ni, ni->ni_vap, &cpp, 0, 0, 0);
    }  
    OS_SLEEP(PRETRAIN_DELAY);
    smartantenna_retrain_check(ni, 0);

    for (i = 0; i < PRETRAIN_PACKETS; i++) {
        ieee80211_smartantenna_tx_custom_data_pkt(ni, ni->ni_vap, &cpp, 0, 0, 0);
    }  
    OS_SLEEP(PRETRAIN_DELAY);
    rateCode = smartantenna_retrain_check(ni, 0);
    IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_SMARTANT,"Pre Training End:%s 0x%08x \n",__func__ ,ni->ni_ic->ic_get_TSF32(ni->ni_ic));
    return rateCode;
}

void ieee80211_smartantenna_training_init(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node_table  *nt = &ic->ic_sta;
    struct ieee80211_node *ni;

    TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
        
        if (IEEE80211_ADDR_EQ(ni->ni_macaddr, vap->iv_myaddr)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG, "%s : Skipping Training for Own VAP: %s \n", __func__ , ether_sprintf(ni->ni_macaddr));
            continue;
        }

        if (ni->is_training == 0) {
            ni->is_training = 1;
            ieee80211_smartantenna_start_training(ni, ic);
        }
    }  /* end of loop*/

   if (ic->ic_smartantennaparams->retraining_enable)
       OS_SET_TIMER(&ic->ic_smartant_retrain_timer, RETRAIN_INTERVEL);

}

/* Check for traiining optmization posibility
 *   Optimization:
 *   1) If < 90 PER in all antenna combinations in first iteration 
 *      skip training for that rate.
 *   2) If we see  < 90 PER in few antenna combinations for 2 or 3 iterations
 *      blacklist those antennas for the next iterations     
 */     
     
int8_t smartantenna_check_optimize(struct ieee80211_node *ni, u_int8_t rateCode)
{

    int  per = 100;
    int8_t max=0, min = 0, i=0;
    u_int8_t rtset = rateCode&0x7f;
    rtset = (rtset & 070)>>3;
    if (ni->permap[0][rateCode&0x7f].nframes == 0)
    {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_SMARTANT, "%s: Number of frames trasmited by STA are Zero !!! \n",__func__);
        ni->smartantenna_state = SMARTANTENNA_NODE_BAD;
        return 2;
    }

    if (ni->permap[0][rateCode&0x7f].nframes > 0)
    {
       per = (100*ni->permap[i][rateCode&0x7f].nbad/ni->permap[i][rateCode&0x7f].nframes); 
    }
    max = per;
    min = per;

    for(i=1 ; i <= ni->ni_ic->ic_smartantennaparams->num_tx_antennas;i++)
    {
        if (ni->permap[i][rateCode&0x7f].nframes > 0)
        {
            per = (100*ni->permap[i][rateCode&0x7f].nbad/ni->permap[i][rateCode&0x7f].nframes);
        }

        if(max < per)
        {
            max= per;
        }
        if(min > per)
        {
            min = per;
        }
    }
    if (min >= 90)
    {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_SMARTANT,"PER for all antennas is more than 90 for rate: 0x%02x \n", rateCode);
        return 1;
    }


    return 0;
}

static inline int find_min(int a, int b, int c)
{
    int m = a;
    if ((m > b) && (b > 0)) m = b;
    if ((m > c) && (c > 0)) return c;
    return m;
}

static inline int find_max(int a, int b, int c)
{
    int mx = a;
    if (mx < b) mx = b;
    if (mx < c) return c;
    return mx;
}

/* using permap find out the best antenna for rateCode 
 * if we are not able to find out the best antenna return error so that we can try
 * new rate from the set 
 * returns the selected antenna else returns -1 to report failure
 * retruns selected antenna if matches criteria
 */

int smartantenna_choose_antenna(struct ieee80211_node *ni, u_int8_t rateCode)
{
    int i = 0, per = 100;
    int8_t max=0, min=0, maxidx=0, minidx=0,current_rssi= 0, min_rssi=0, max_rssi = 0;
    u_int8_t rtset = rateCode&0x7f;

    rtset = (rtset & 070)>>3;
    if (ni->train_type == TRAIN_TYPE_PROTOCOL)
    {
        ieee80211_smartantenna_tx_proto_msg(ni->ni_vap,ni,SAMRTANT_SEND_RXSTATS,0,0,0);
        OS_SLEEP(1000);
    }

    if (ni->permap[0][rateCode&0x7f].nframes > 0)
            per = (100*ni->permap[i][rateCode&0x7f].nbad/ni->permap[i][rateCode&0x7f].nframes); 

     max = per;
     min = per;
     /* get BA */
     min_rssi = find_min(ni->permap[i][rateCode&0x7f].chain0_rssi, ni->permap[i][rateCode&0x7f].chain1_rssi, ni->permap[i][rateCode&0x7f].chain2_rssi);

#if SMARTANTENNA_DEBUG

    IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_SMARTANT,"SenderTXAntenna: %s SenderDefaultAntenna: %s RecieverDefaultAntenna: %s RateCode: 0x%02x nFrames: %04d nBad: %04d Per: %03d BARSSI: %02d %02d %02d Receiver RSSI: %02d %02d %02d\n"        ,ant_buf[i], ant_buf[ni->ni_ic->ic_default_rx_ant],ant_buf[ni->reciever_rx_antenna]
                                            ,rateCode, ni->permap[i][rateCode&0x7f].nframes,ni->permap[i][rateCode&0x7f].nbad, per
                                            ,ni->permap[i][rateCode&0x7f].chain0_rssi
                                            ,ni->permap[i][rateCode&0x7f].chain1_rssi
                                            ,ni->permap[i][rateCode&0x7f].chain2_rssi
                                            ,((receiver_rxpermap[i].nframes == 0) ? 0 : receiver_rxpermap[i].chain0_rssi/receiver_rxpermap[i].nframes)
                                            ,((receiver_rxpermap[i].nframes == 0) ? 0 : receiver_rxpermap[i].chain1_rssi/receiver_rxpermap[i].nframes)
                                            ,((receiver_rxpermap[i].nframes == 0) ? 0 : receiver_rxpermap[i].chain2_rssi/receiver_rxpermap[i].nframes)
                                            );

#endif    

    for(i=1 ; i<= ni->ni_ic->ic_smartantennaparams->num_tx_antennas;i++)
    {
        if (ni->permap[i][rateCode&0x7f].nframes > 0)
            per = (100*ni->permap[i][rateCode&0x7f].nbad/ni->permap[i][rateCode&0x7f].nframes); 

        if(max < per)
        {
            max= per;
            maxidx = i;
        }

        if (min >= per-SA_PERDIFF_THRESHOLD)  /* If difference between PER is less than or equal to SA_PERDIFF_THRESHOLD */
        {
            if ((min-per > SA_PERDIFF_THRESHOLD))
            {
                min = per;
                minidx = i;
                min_rssi = find_min(ni->permap[i][rateCode&0x7f].chain0_rssi, ni->permap[i][rateCode&0x7f].chain1_rssi, ni->permap[i][rateCode&0x7f].chain2_rssi);
                
            } else {          /* 
                               *  minper and current PER are same or almost same (per <=5) 
                               *  now we need to apply secondary metric
                               *  maxmizing the minimum interms of BlockACK
                               */
                current_rssi = find_min(ni->permap[i][rateCode&0x7f].chain0_rssi, ni->permap[i][rateCode&0x7f].chain1_rssi, ni->permap[i][rateCode&0x7f].chain2_rssi);
                IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_SMARTANT,"applying Secondry Metric, current_rssi: %d ;min_rssi: %d \n", current_rssi, min_rssi);
                if (current_rssi > min_rssi)  /* Maximizing the Minimum */
                {
                    minidx = i;
                    min_rssi = current_rssi;
                    min = per;
                } 
                else if (current_rssi == min_rssi)
                {
                   /* if we have same min RSSI values then check for MAX RSSI in that BA RSSI SET */
                    min_rssi = current_rssi;
                    max_rssi = find_max(ni->permap[minidx][rateCode&0x7f].chain0_rssi, ni->permap[minidx][rateCode&0x7f].chain1_rssi,
                                        ni->permap[minidx][rateCode&0x7f].chain2_rssi);

                   current_rssi = find_max(ni->permap[i][rateCode&0x7f].chain0_rssi, ni->permap[i][rateCode&0x7f].chain1_rssi, ni->permap[i][rateCode&0x7f].chain2_rssi);
                    IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_SMARTANT,"conflit in secondary metric, max_rssi %d ;current_rssi: %d \n", max_rssi,current_rssi);
                    if(current_rssi > max_rssi)
                    {
                        minidx = i;
                        min = per;
                    }
                }
                    IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_SMARTANT, "current Seleted Index : %d \n", minidx);

            }
        }

#if SMARTANTENNA_DEBUG
    IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_SMARTANT,"SenderTXAntenna: %s SenderDefaultAntenna: %s RecieverDefaultAntenna: %s RateCode: 0x%02x nFrames: %04d nBad: %04d Per: %03d BARSSI: %02d %02d %02d Receiver RSSI: %02d %02d %02d\n"        ,ant_buf[i], ant_buf[ni->ni_ic->ic_default_rx_ant],ant_buf[ni->reciever_rx_antenna]
                                            ,rateCode, ni->permap[i][rateCode&0x7f].nframes,ni->permap[i][rateCode&0x7f].nbad, per
                                            ,ni->permap[i][rateCode&0x7f].chain0_rssi
                                            ,ni->permap[i][rateCode&0x7f].chain1_rssi
                                            ,ni->permap[i][rateCode&0x7f].chain2_rssi
                                            ,((receiver_rxpermap[i].nframes == 0) ? 0 : receiver_rxpermap[i].chain0_rssi/receiver_rxpermap[i].nframes)
                                            ,((receiver_rxpermap[i].nframes == 0) ? 0 : receiver_rxpermap[i].chain1_rssi/receiver_rxpermap[i].nframes)
                                            ,((receiver_rxpermap[i].nframes == 0) ? 0 : receiver_rxpermap[i].chain2_rssi/receiver_rxpermap[i].nframes)
                                            );

#endif        
    }

    /* all antennas PER is more than 60 */
    if (min >= 90)
    {
        return -1;
    }

    return minidx;

}

/*  
 *  Single State m/c for whole training process
 *    a) ni_smartant_list  contains the list of nodes need to be trained
 *    b) Node structure contains all state information and per_map table.
 *    c) At any point of time we can stop the training process and restart the process after certain time
 *       or when ever buffer requirements match ...; Idea is stop the current work queue and restart with some dealy or queue the work again
 *    d) Once training is done we can remove the node from the list 
      e) For protocol based training and frame based training use the same work queue
 */

void smartantenna_training_sm(void *data)
{
   struct ieee80211com *ic = (struct ieee80211com *)data;
   struct ieee80211_node *ni_current = NULL, *ni_free = NULL;
   struct ieee80211_node_table *nt = &ic->ic_sta;
   int pending = 0, retval;
   u_int8_t i;   

   if (TAILQ_EMPTY(&nt->nt_smartant_node))
   {
       return;
   }

   ni_current = TAILQ_FIRST(&nt->nt_smartant_node);

   ni_free = ieee80211_find_node(&ic->ic_sta, ni_current->ni_macaddr);
   if (ni_free == NULL) {
       IEEE80211_DPRINTF(ni_current->ni_vap, IEEE80211_MSG_ANY,"__func__ : Bug !!! we are not suppose to reach here \n", __func__);
       ATH_QUEUE_DELAYED_WORK(ic->ic_smart_workqueue,&ic->smartant_task,HZ/100);
       return;
    }
       
   /* Pre Training and Initilaztion */
   if ( 0 == (ni_current->smartantenna_state&SMARTANTENNA_NODE_PRETRAIN))
   {
       if(ni_current->smartantenna_state == SMARTANTENNA_NODE_PRETRAIN_INPROGRESS)
       {
           ATH_QUEUE_DELAYED_WORK(ic->ic_smart_workqueue,&ic->smartant_task,HZ/ic->ic_smartantennaparams->schedule_delay);
           ieee80211_free_node(ni_free);
           return;
       }else
       {
           ieee80211_smartantenna_init_training(ni_current, ic);
       }

   }

   /* Find out the antenna from permap table */
   if (ni_current->smartantenna_state == SMARTANTENNA_TRAIN_MOVE_NEXTRATE)
   {
       retval = smartantenna_choose_antenna(ni_current,ni_current->rtset[ni_current->train_state.rateset].rates[ni_current->train_state.rateidx].ratecode);
       if(-1 == retval)
       {    /* 
             * failed to choose antenna so move to next rate in set
             */
           ni_current->train_state.rateidx++; /* got to next index in the set */
           if (ni_current->train_state.rateidx < ni_current->rtset[ni_current->train_state.rateset].num_of_rates)
           {
                /* reset training paramters */
                smartantenna_state_save(ni_current, 0, 0, 0);
           }
           else
           {   /* Not able to find antenna from this rate set so set selected antenna to default one */
               ni_current->rtset[ni_current->train_state.rateset].selected_antenna = 0; /* default antenna VICKS: TBD read from register*/
               ic->ic_set_selected_smantennas(ni_current, ni_current->rtset[ni_current->train_state.rateset].selected_antenna, ni_current->train_state.rateset);
               temp_ts = ic->ic_get_TSF32(ic);
               IEEE80211_DPRINTF(ni_current->ni_vap, IEEE80211_MSG_SMARTANT,"Antenna Selected for rateset <fail> %d : %d : Total Train Time : %d \n"
                                                                          ,ni_current->train_state.rateset
                                                                          ,ni_current->rtset[ni_current->train_state.rateset].selected_antenna , temp_ts-train_ts);

               /* Move to next set ; start from index 0 */
               ni_current->train_state.rateset--;
               if (ni_current->train_state.rateset < 0) /* if all rate sets are done then  mark node as trained */
               {
                     ni_current->smartantenna_state = SMARTANTENNA_TRAIN_DONE; 
                     ATH_QUEUE_DELAYED_WORK(ic->ic_smart_workqueue,&ic->smartant_task,HZ/ic->ic_smartantennaparams->schedule_delay);
#if not_yet
                     ieee80211_free_node(ni_free);
                     ATH_QUEUE_DELAYED_WORK(ic->ic_smart_workqueue,&ic->smartant_task,HZ/ic->ic_smartantennaparams->schedule_delay);
                     return;
#endif                     
               }
              if (ni_current->train_state.rateset == 0)
              {
                  /* for single stream rate schedule delay factor will be lowered to make sure that all packets are trasmited */
                  ic->ic_smartantennaparams->schedule_delay = DEFAULT_SCHEDULE_DELAY_FACTOR/2; // 2 millsec
              }
                  ni_current->train_state.rateidx = 0; /* first rate from the new set */
                  smartantenna_state_save(ni_current, 0, 0, 0);
           }        

       }
       else
       {    /*
             *  able to find antenna ; move to next rate set ???
             *  Optimization: If we are able to find the Antenna for that rateset 
             *  No need to go down to further rate set because Currently we will operate on the same rate set, If we change rate set
             *  mostly we need to do training again !!! bcz previous data may not be true.
             */

             /* same Antenna for all rate sets */
            for (i = 0; i < SMARTANT_MAX_RATESETS; i++)
            {
                ni_current->rtset[i].selected_antenna = retval;
                ic->ic_set_selected_smantennas(ni_current, retval, i);
            }


             temp_ts = ic->ic_get_TSF32(ic);
             IEEE80211_DPRINTF(ni_current->ni_vap, IEEE80211_MSG_SMARTANT,"Antenna Selected for rateset %d : %d : Total Train Time : %d \n", ni_current->train_state.rateset
                                                                          ,ni_current->rtset[ni_current->train_state.rateset].selected_antenna , temp_ts-train_ts);
             ni_current->smartantenna_state = SMARTANTENNA_TRAIN_DONE;
             ATH_QUEUE_DELAYED_WORK(ic->ic_smart_workqueue,&ic->smartant_task,HZ/ic->ic_smartantennaparams->schedule_delay);
#if not_yet
             ieee80211_free_node(ni_free);   
             ATH_QUEUE_DELAYED_WORK(ic->ic_smart_workqueue,&ic->smartant_task,HZ/ic->ic_smartantennaparams->schedule_delay);
             return;
#endif             

             /* Move to next set ; start from index 0 */
#if DONOT_OPTIMIZAE             
             ni_current->rtset[ni_current->train_state.rateset].selected_antenna = retval;
             ic->ic_set_selected_smantennas(ni_current, ni_current->rtset[ni_current->train_state.rateset].selected_antenna, ni_current->train_state.rateset);
             ni_current->train_state.rateset--;
             if (ni_current->train_state.rateset < 0) /*if all rate sets are done then quit mark node as trained */
             {
                ni_current->smartantenna_state = SMARTANTENNA_TRAIN_DONE;
                ieee80211_free_node(ni_free);   
                ATH_QUEUE_DELAYED_WORK(ic->ic_smart_workqueue,&ic->smartant_task,HZ/ic->ic_smartantennaparams->schedule_delay);
                return;
             }

             if (ni_current->train_state.rateset == 0)
             {
                /* for single stream rate schedule delay factor will be lowered to make sure that all packets are trasmited */
                ic->ic_smartantennaparams->schedule_delay = DEFAULT_SCHEDULE_DELAY_FACTOR/2; // 2 millsec
             }
             ni_current->train_state.rateidx = 0; 
             smartantenna_state_save(ni_current, 0, 0, 0);
#endif             

       }

   }

   if (ni_current->smartantenna_state == SMARTANTENNA_TRAIN_DONE)
   {
       /* remove the node once training is done */
       if(!ic->ic_smartantennaparams->automation_enable)       
       {
           IEEE80211_DPRINTF(ni_current->ni_vap, IEEE80211_MSG_ANY,"Training Done for Node : %s \n",ether_sprintf(ni_current->ni_macaddr));
           for (i=0; i<SMARTANT_MAX_RATESETS; i++)
           {
                IEEE80211_DPRINTF(ni_current->ni_vap, IEEE80211_MSG_ANY,"Antenna Selected for rateset %d : %d \n", i ,ni_current->rtset[i].selected_antenna);
           }

           TAILQ_REMOVE(&nt->nt_smartant_node, ni_current, smartant_nodelist);
           ni_current->smartantenna_state = SMARTANTENNA_NODE_DEFAULT;
           ni_current->ni_consecutive_xretries = 0;
           ni_current->is_training = 0;
           ieee80211_free_node(ni_free);   
           ATH_QUEUE_DELAYED_WORK(ic->ic_smart_workqueue,&ic->smartant_task,HZ);
           return;
       }
       /* If automation is enabled then sweep through all RX antennas on receiver side */
       ni_current->reciever_rx_antenna++;
       ni_current->maxrtsets = SMARTANT_MAX_RATESETS;
       smartantenna_state_init(ni_current, 0x97);
       /*
        *  reset nFrame and nBad for all permap for each recieve antenna changes
        */
       smartantenna_reset_permap(ni_current);
       ni_current->current_tx_antenna = 0;
       if (ni_current->reciever_rx_antenna >= 8)
       {
           ni_current->smartantenna_state = SMARTANTENNA_TRAIN_DONE;
           TAILQ_REMOVE(&nt->nt_smartant_node, ni_current, smartant_nodelist);
           ieee80211_free_node(ni_free);   
           ATH_QUEUE_DELAYED_WORK(ic->ic_smart_workqueue,&ic->smartant_task,5*HZ);
           return;
       }
       ni_current->smartantenna_state = SMARTANTENNA_TRAIN_INPROGRESS;
#if 0 
       printk(">>> rxant: %d : rtset: %d : rtindex: %d rtcode: %d \n", ni_current->reciever_rx_antenna
               ,ni_current->train_state.rateset
               ,ni_current->train_state.rateidx
               ,ni_current->rtset[ni_current->train_state.rateset].rates[ni_current->train_state.rateidx].ratecode);
#endif       
       ieee80211_smartantenna_tx_proto_msg(ni_current->ni_vap, ni_current,SAMRTANT_SET_ANTENNA,ni_current->current_tx_antenna ,ni_current->reciever_rx_antenna
               ,ni_current->rtset[ni_current->train_state.rateset].rates[ni_current->train_state.rateidx].ratecode);
       ieee80211_free_node(ni_free);   
       ATH_QUEUE_DELAYED_WORK(ic->ic_smart_workqueue,&ic->smartant_task,HZ/2);
       return; 
   }

   /* RESTORE PREVIOUS STATE IF ANY or INITILIZE STATE */
   smartantenna_state_restore(ni_current);


#if 0 
  
   printk("Training for Node: %s : rateIdx: %d : ant:%d  itr: %d :pending : %d \n" 
                                                              ,ether_sprintf(ni_current->ni_macaddr)
                                                              ,ni_current->current_rate_index
                                                              ,ni_current->current_tx_antenna,ni_current->iteration
                                                              ,ic->ic_smartantennaparams->num_of_packets - ni_current->pending_pkts);
#endif   

   while(ni_current->iteration < ni_current->ni_ic->ic_smartantennaparams->num_of_iterations)
   {
    
       while(ni_current->current_tx_antenna <= ni_current->ni_ic->ic_smartantennaparams->num_tx_antennas)
       {
          
           if (ni_current->train_type == TRAIN_TYPE_PROTOCOL)
           {
                ieee80211_smartantenna_tx_proto_msg(ni_current->ni_vap, ni_current,SAMRTANT_SET_ANTENNA,ni_current->current_tx_antenna,ni_current->reciever_rx_antenna
                            , ni_current->rtset[ni_current->train_state.rateset].rates[ni_current->train_state.rateidx].ratecode);
                OS_DELAY(100);
           }
           pending = ieee80211_smartantenna_train_node(ni_current, ni_current->current_tx_antenna, ni_current->current_rate_index);
           if(pending) 
           {    
              /* we have pedning packets that are still not trasmited */
              /* stop the current training process and restart again in next work queue item */
              ni_current->pending_pkts += pending; /* update the total transmited packets in the current train */
              smartantenna_state_save(ni_current, ni_current->current_tx_antenna, ni_current->iteration, ni_current->pending_pkts);
              ieee80211_free_node(ni_free);
              return;
           }
           else
           {
                ni_current->pending_pkts = 0;
                
                ni_current->current_tx_antenna++;
                if (ni_current->current_tx_antenna > ni_current->ni_ic->ic_smartantennaparams->num_tx_antennas)
                {    
                   ni_current->current_tx_antenna = 0;
                   if(smartantenna_check_optimize(ni_current,ni_current->rtset[ni_current->train_state.rateset].rates[ni_current->train_state.rateidx].ratecode))
                   {
                        if( ni_current->train_state.rateset >= 0)
                             ni_current->smartantenna_state = SMARTANTENNA_TRAIN_MOVE_NEXTRATE;
                         else
                             ni_current->smartantenna_state = SMARTANTENNA_TRAIN_DONE;
                         /* Mark this one as last iteration */
                         ni_current->iteration = ni_current->ni_ic->ic_smartantennaparams->num_of_iterations;
                   }
                   else
                   {
                       ni_current->iteration++;
                   }

                }   
                /* TODO: Check for optmize train time */
                smartantenna_state_save(ni_current, ni_current->current_tx_antenna, ni_current->iteration, ni_current->pending_pkts);
                ieee80211_free_node(ni_free);
                return;
           }

       }
   }
    
   /*
    * at this point training is done for one rate in a set, mark state to Next rate
    * Move to next rate if required or move to next state 
    * If all sets are done ; then Mark state to TRAIN_DONE
    */

  if( ni_current->train_state.rateset >= 0)
     ni_current->smartantenna_state = SMARTANTENNA_TRAIN_MOVE_NEXTRATE;
  else
     ni_current->smartantenna_state = SMARTANTENNA_TRAIN_DONE;

   ieee80211_free_node(ni_free);   
   ATH_QUEUE_DELAYED_WORK(ic->ic_smart_workqueue,&ic->smartant_task,HZ/ic->ic_smartantennaparams->schedule_delay);

}

/*
 * Check for various parameters for retraining
 * Returns TRUE if we need to retrain the STA
 *         False if retraining is not required 
 */

int smartantenna_retrain_check(struct ieee80211_node *ni, int mode)
{
   u_int32_t  ratestats[MAX_HT_RATES],max = 0;
   int i=0, nsend =0;
   u_int8_t maxidx=0;
   ni->ni_ic->ic_get_smartantenna_ratestats(ni, (void *)&ratestats[0]);
   for(i=0;i<MAX_HT_RATES;i++)
   {
#if SMARTANTENNA_DEBUG       
        if(ratestats[i])
        {
            IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_SMARTANT,"RateCode: 0x%02x - stats: %d \n",(i|0x80), ratestats[i]);
            nsend=1;
        }
#endif
        if(max < ratestats[i])
        {
            max=ratestats[i];
            maxidx=i;
        }
   }
   if (nsend)
       maxidx = (maxidx | 0x80);
   else
       maxidx = 0x80;

#if SMARTANTENNA_DEBUG       
   IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_SMARTANT,"MAX Rate Code : 0x%02x :pkts: %d \n", maxidx , max);
#endif
   if (mode == 0)
       return maxidx;

   if (ni->smartant_prev_ratemax > maxidx)
   {
       ni->smartant_prev_ratemax = maxidx;
       return 1; 
   }
   else
   {
       ni->smartant_prev_ratemax = maxidx;
   }

   return 0;
}

/*  Time Based Periodic Retraining 
 *  Walk through all associated STA's
 *    a) If it is in TRAIN_PROGRESS state do not queue into training Q
 *    b) If gput is not lowered by Threshold value then do not add to training Q
 *    c) If see gput loss add node to Training Q and Schedule it
 *    d) If no traffic is going donot do training for some time i.e we can train every 5 intervels
 */
static OS_TIMER_FUNC(smartantenna_retrain_handler)
{
    struct ieee80211com *ic;
    struct ieee80211_node *ni;
    int diff;
    struct ieee80211_node_table  *nt;
    OS_GET_TIMER_ARG(ic, struct ieee80211com *);

    nt  = &ic->ic_sta;
    TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
       if (IEEE80211_ADDR_EQ(ni->ni_macaddr, ni->ni_vap->iv_myaddr))
       {
           continue;
       }
       if (ni->smartantenna_state == SMARTANTENNA_TRAIN_INPROGRESS)
       {
           continue;
       }
#if SMARTANTENNA_DEBUG
       IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_SMARTANT, "Retrain check for MAC : %s \n",ether_sprintf(ni->ni_macaddr));
       IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_SMARTANT, "Tx unicast frames (previous): %d  Tx Unicast Frames(current):%d \n"
                                           ,ni->ns_prev_tx_ucast, ni->ni_stats.ns_tx_ucast);
#endif       
       /* If there is no traffic train the node after 5 retrain intervels */
       diff = ni->ni_stats.ns_tx_ucast-ni->ns_prev_tx_ucast;
       diff = (diff>0) ? diff:-diff;
       if(diff > RETRAIN_PKT_THRESHOLD)
       {
           /*
            *  Data traffic is going; Let us check whether we are meeting retraining condition 
            */
           if (smartantenna_retrain_check(ni, 1))
           {
                 /* need to retrain */
           }
           else
           {
               /* update the total trasmited packets */ 
               ni->ns_prev_tx_ucast = ni->ni_stats.ns_tx_ucast;
               continue;
           }
            
       }
       else
       {  /* skip the training for now */
          if(ni->retrain_miss >= RETRAIN_MISS_THRESHOLD)
          {
              ni->retrain_miss =0 ;
          }
          else
          {
              ni->retrain_miss++;
              continue;   
          }
          ni->ns_prev_tx_ucast = ni->ni_stats.ns_tx_ucast = 0;

       }

       ni->ns_prev_tx_ucast = ni->ni_stats.ns_tx_ucast;
       /* start training */
       if (ni->is_training == 0) {
           ni->is_training = 1;
           ieee80211_smartantenna_start_training(ni, ic);
       }
   }

   if (ic->ic_smartantennaparams->retraining_enable)
       OS_SET_TIMER(&ic->ic_smartant_retrain_timer, RETRAIN_INTERVEL); /* ms */

}

/*
 * Sequence of operations need to be done for a node to initiate training process
 */

int ieee80211_smartantenna_start_training(struct ieee80211_node *ni, struct ieee80211com *ic)
{
    struct ieee80211_node_table  *nt = &ic->ic_sta;
    
    ni->smartantenna_state = SMARTANTENNA_NODE_DEFAULT ; 
    /* Add node to the training Queue */
    TAILQ_INSERT_TAIL(&nt->nt_smartant_node, ni, smartant_nodelist);
    /* schedule work queue */
    ATH_QUEUE_DELAYED_WORK(ic->ic_smart_workqueue,&ic->smartant_task,HZ/100);
    return 0;
}

int ieee80211_smartantenna_init_training(struct ieee80211_node *ni, struct ieee80211com *ic)
{
    u_int8_t rateCode;

    train_ts = ic->ic_get_TSF32(ic);
    IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_SMARTANT, "Training start:%s 0x%08x \n",__func__ ,train_ts);
    ni->smartantenna_state = SMARTANTENNA_NODE_PRETRAIN_INPROGRESS;
    rateCode = ieee80211_smartantenna_pretraining(ni);
    smartantenna_reset_permap(ni);
    /* start training on the rate set given by pre training only */
    ni->current_tx_antenna = ic->ic_get_default_antenna(ic);
    ic->ic_prepare_rateset(ic, ni);
    ni->maxrtsets = sm_get_maxrtsets(ni);
    ni->reciever_rx_antenna = 0;
    ni->retrain_miss = 0;
    smartantenna_setdefault_antenna(ni, ni->current_tx_antenna);
    smartantenna_state_init(ni, rateCode);
    ic->ic_smartantennaparams->schedule_delay = DEFAULT_SCHEDULE_DELAY_FACTOR;
    /*
     * TODO: vicks For testing only !!!, ni->train_type should be derived from 
     * IE of assoc request
     */
    ni->train_type = ic->ic_smartantennaparams->training_type; 
    if (ni->train_type == TRAIN_TYPE_PROTOCOL)
    {
        ieee80211_smartantenna_tx_proto_msg(ni->ni_vap, ni,SAMRTANT_SET_ANTENNA,ni->current_tx_antenna,ni->reciever_rx_antenna
            , ni->rtset[ni->train_state.rateset].rates[ni->train_state.rateidx].ratecode);
    }

    /* Add node to the training Queue 
     *  TAILQ_INSERT_TAIL(&nt->nt_smartant_node, ni, smartant_nodelist);
     */

    /* schedule work queue */
    ATH_QUEUE_DELAYED_WORK(ic->ic_smart_workqueue,&ic->smartant_task,HZ/100);
    ni->smartantenna_state = SMARTANTENNA_TRAIN_INPROGRESS;

    return 1;
}
int ieee80211_smartantenna_attach(struct ieee80211com *ic)
{
    
    if (!ic) return -EFAULT; 

    ic->ic_smartantenna_init_done=0;

    ic->ic_smartantennaparams = (struct ieee80211_smartantenna_params *) 
                     OS_MALLOC(ic->ic_osdev, sizeof(struct ieee80211_smartantenna_params), GFP_KERNEL); 
    
    if (!ic->ic_smartantennaparams) {
        printk("Memory not allocated for ic->ic_smartantennaparams\n"); /* Not expected */
        return -ENOMEM;
    }

    OS_MEMSET(ic->ic_smartantennaparams, 0, sizeof(struct ieee80211_smartantenna_params));  
    /* init the default values */
    ic->ic_smartantennaparams->training_mode = 0;
    ic->ic_smartantennaparams->training_type = 1;
    ic->ic_smartantennaparams->packetlen = 1536;
    ic->ic_smartantennaparams->num_of_packets = 350;
    ic->ic_smartantennaparams->num_of_iterations = 2;
    ic->ic_smartantennaparams->automation_enable = 0;
    ic->ic_smartantennaparams->retraining_enable = 0;
    ic->ic_smartantennaparams->num_tx_antennas = (1 << ic->ic_num_tx_chain)-1;

    /* Create Work Queue */
    ic->ic_smart_workqueue =  ATH_CREATE_WQUEUE(WQNAME(smartantena_workqueue));
    ATH_CREATE_DELAYED_WORK(&ic->smartant_task, smartantenna_training_sm, ic);
    OS_INIT_TIMER(ic->ic_osdev, &ic->ic_smartant_retrain_timer, smartantenna_retrain_handler, ic);
    ic->ic_smartantenna_init_done=1;
    ic->ic_smartantennaparams->schedule_delay = DEFAULT_SCHEDULE_DELAY_FACTOR;
    return 0;
}

int
ieee80211_smartantenna_detach(struct ieee80211com *ic)
{
    int err=0;

    if (!ic) return -EFAULT;

    if (!ic->ic_smartantenna_init_done) return 0;

    if (!ic->ic_smartantennaparams) {
        printk("Memory not allocated for ic->ic_rptplacementparams\n"); 
        err=-ENOMEM;
        goto bad;
    } else {
        printk("%s ic_smartantennaparams is freed \n", __func__);
        OS_FREE(ic->ic_smartantennaparams); 
        ic->ic_smartantennaparams = NULL;
    }

bad:
   ATH_FLUSH_WQUEUE(ic->ic_smart_workqueue);
   ATH_DESTROY_WQUEUE(ic->ic_smart_workqueue);
   OS_FREE_TIMER(&ic->ic_smartant_retrain_timer);
   ic->ic_smartantenna_init_done=0;

   return err;
}

#endif
