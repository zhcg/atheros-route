/*
 * Copyright (c) 2008, Atheros Communications Inc.
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

#include <linux/stddef.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <asm/byteorder.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/bitops.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <net/sch_generic.h>
#include <linux/if_pppox.h>
#include <linux/ip.h>
#include <net/checksum.h>
#include <linux/version.h>
#include "ag7240.h"
#include "ag7240_trc.h"
#include "ethdebug.h"

#if CONFIG_FLOWMAC_MODULE
#include "../flowmac/flowmac_api.h"
int8_t ag7240_rx_pause(struct net_device *dev, u_int8_t pause,
                        u_int32_t duration);

int8_t ag7240_rx_rate_limit(struct net_device *dev, flow_dir_t dir,
                        void *rate);

int ag7240_notify_event (struct net_device *dev, flowmac_event_t event,
                        void *event_data);

int ag7240_flowmac_register(struct net_device *dev);
int ag7240_flowmac_un_register(struct net_device *dev);
#endif

struct eth_dbg_params eth_vi_dbg_params;
u_int32_t eth_rx_drop = 0; 
void ethdbg_check_rxdrop(struct sk_buff *skb);

#define MODULE_NAME "AG7240"
MODULE_LICENSE("Dual BSD/GPL");


ag7240_mac_t *ag7240_macs[2];
ATH_LED_CONTROL    PLedCtrl;
atomic_t Ledstatus;
int phy_in_reset = 0;
int rg_phy_speed = -1 , rg_phy_duplex = -1;
char *spd_str[] = {"10Mbps", "100Mbps", "1000Mbps"};
char *dup_str[] = {"half duplex", "full duplex"}; 


module_param(tx_len_per_ds, int, 0);
MODULE_PARM_DESC(tx_len_per_ds, "Size of DMA chunk");

int fifo_3 = 0x1f00140;
module_param(fifo_3, int, 0);
MODULE_PARM_DESC(fifo_3, "fifo cfg 3 settings");

int mii0_if = AG7240_MII0_INTERFACE;
module_param(mii0_if, int, 0);
MODULE_PARM_DESC(mii0_if, "mii0 connect");

int mii1_if = AG7240_MII1_INTERFACE;
module_param(mii1_if, int, 0);
MODULE_PARM_DESC(mii1_if, "mii1 connect");

int gige_pll = 0x1a000000;
module_param(gige_pll, int, 0);
MODULE_PARM_DESC(gige_pll, "Pll for (R)GMII if");

int fifo_5 = 0xbefff;
module_param(fifo_5, int, 0);
MODULE_PARM_DESC(fifo_5, "fifo cfg 5 settings");

int eth0_flow_control = 0;
module_param(eth0_flow_control, int, 0);
MODULE_PARM_DESC(eth0_flow_control, "PAUSE enable/disable on eth0");
int eth1_flow_control = 0;
module_param(eth1_flow_control, int, 0);
MODULE_PARM_DESC(eth1_flow_control, "PAUSE enable/disable on eth1");
int flowmac_on=0;
module_param(flowmac_on, int, 0);
MODULE_PARM_DESC(flowmac_on, "Enable/Disable Flowcontrol between LAN & WLAN macs");
int xmii_val = 0x16000000;

int ignore_packet_inspection = 0;

void set_packet_inspection_flag(int flag)
{
    if(flag==0)
        ignore_packet_inspection = 0;
    else
        ignore_packet_inspection = 1;
}

void hdr_dump(char *str,int unit,struct ethhdr *ehdr,int qos,int ac)
{
    printk("\n[%s:%x][%x:%x][MAC1:%x,%x,%x,%x,%x,%x MAC2: %x,%x,%x,%x,%x,%x Prot:%x]\n",str,unit,qos,ac,
    ehdr->h_dest[0],ehdr->h_dest[1],ehdr->h_dest[2],ehdr->h_dest[3],ehdr->h_dest[4],ehdr->h_dest[5],
    ehdr->h_source[0],ehdr->h_source[1],ehdr->h_source[2],ehdr->h_source[3],ehdr->h_source[4],
        ehdr->h_source[5],ehdr->h_proto);

}
        
static int
ag7240_open(struct net_device *dev)
{
    unsigned int w1 = 0, w2 = 0;
    ag7240_mac_t *mac = (ag7240_mac_t *)dev->priv;
    int st,flags;
    uint32_t mask;

    assert(mac);
    if (mac_has_flag(mac,WAN_QOS_SOFT_CLASS))
        mac->mac_noacs = 4;
    else
        mac->mac_noacs = 1;

    st = request_irq(mac->mac_irq, ag7240_intr, 0, dev->name, dev);
    if (st < 0)
    {
        printk(MODULE_NAME ": request irq %d failed %d\n", mac->mac_irq, st);
        return 1;
    }
    if (ag7240_tx_alloc(mac)) goto tx_failed;
    if (ag7240_rx_alloc(mac)) goto rx_failed;

    udelay(20);
    mask = ag7240_reset_mask(mac->mac_unit);

    /*
    * put into reset, hold, pull out.
    */
    spin_lock_irqsave(&mac->mac_lock, flags);
    ar7240_reg_rmw_set(AR7240_RESET, mask);
    udelay(10);
    ar7240_reg_rmw_clear(AR7240_RESET, mask);
    udelay(10);
    spin_unlock_irqrestore(&mac->mac_lock, flags);

    ag7240_hw_setup(mac);

   /* MI93 changes */

#ifdef CONFIG_AR7242_VIR_PHY
 #ifndef SWITCH_AHB_FREQ
    u32 tmp_pll ;
 #endif
    tmp_pll = 0x62000000 ;
    ar7240_reg_wr_nf(AR7242_ETH_XMII_CONFIG, tmp_pll);
    udelay(100*1000);
#endif

    mac->mac_speed = -1;

    ag7240_phy_reg_init(mac->mac_unit);

    printk("Setting PHY...\n");
   
    if (mac_has_flag(mac,ETH_SOFT_LED)) {
    /* Resetting PHY will disable MDIO access needed by soft LED task.
     * Hence, Do not reset PHY until Soft LED task get completed.
     */
        while(atomic_read(&Ledstatus) == 1);
    }
    phy_in_reset = 1;

#ifndef CONFIG_AR7242_VIR_PHY
     ag7240_phy_setup(mac->mac_unit);
#else
     athr_vir_phy_setup(mac->mac_unit);
#endif 
    phy_in_reset = 0;

    /*
    * set the mac addr
    */
    addr_to_words(dev->dev_addr, w1, w2);
    ag7240_reg_wr(mac, AG7240_GE_MAC_ADDR1, w1);
    ag7240_reg_wr(mac, AG7240_GE_MAC_ADDR2, w2);

    dev->trans_start = jiffies;

    /*
     * Keep carrier off while initialization and switch it once the link is up.
     */
    netif_carrier_off(dev);
    netif_stop_queue(dev);
 
    mac->mac_ifup = 1;
    ag7240_int_enable(mac);

    if (is_ar7240())
        ag7240_intr_enable_rxovf(mac);

#if defined(CONFIG_AR7242_RGMII_PHY)||defined(CONFIG_AR7242_S16_PHY)||defined(CONFIG_AR7242_VIR_PHY)   
    
    if(is_ar7242() && mac->mac_unit == 0) {

        init_timer(&mac->mac_phy_timer);
        mac->mac_phy_timer.data     = (unsigned long)mac;
        mac->mac_phy_timer.function = (void *)ag7242_check_link;
        ag7242_check_link(mac);

    }
#endif

    if (is_ar7240() || is_ar7241() || (is_ar7242() && mac->mac_unit == 1))
       athrs26_enable_linkIntrs(mac->mac_unit);

    ag7240_rx_start(mac);
   
    return 0;

rx_failed:
    ag7240_tx_free(mac);
tx_failed:
    free_irq(mac->mac_irq, dev);
    return 1;
}

static int
ag7240_stop(struct net_device *dev)
{
    ag7240_mac_t *mac = (ag7240_mac_t *)dev->priv;
    int flags,i;

    spin_lock_irqsave(&mac->mac_lock, flags);
    mac->mac_ifup = 0;
    netif_stop_queue(dev);
    netif_carrier_off(dev);

    ag7240_hw_stop(mac);
    free_irq(mac->mac_irq, dev);

    spin_unlock_irqrestore(&mac->mac_lock, flags);

    ag7240_tx_free(mac);
    ag7240_rx_free(mac);

/* During interface up on PHY reset the link status will be updated.
 * Clearing the software link status while bringing the interface down
 * will avoid the race condition during PHY RESET.
 */
    if (mac_has_flag(mac,ETH_SOFT_LED)) {
        if (mac->mac_unit == ENET_UNIT_LAN) {
            for(i = 0;i < 4; i++)
                PLedCtrl.ledlink[i] = 0;
        }
        else {
            PLedCtrl.ledlink[4] = 0;
        }
    }
    if (is_ar7242())
        del_timer(&mac->mac_phy_timer);

    /*ag7240_trc_dump();*/

    return 0;
}

static void
ag7240_hw_setup(ag7240_mac_t *mac)
{
    ag7240_ring_t *tx, *rx = &mac->mac_rxring;
    ag7240_desc_t *r0, *t0;
    uint32_t mgmt_cfg_val,ac;
    u32 check_cnt;
    u32 fcntl_bits= AG7240_MAC_CFG1_TX_FCTL | AG7240_MAC_CFG1_RX_FCTL;

#if CONFIG_FLOWMAC_MODULE
    switch ( mac->mac_unit ? eth1_flow_control : eth0_flow_control) {
    case 1: /* TX FLOW ENABLE */
        fcntl_bits = AG7240_MAC_CFG1_TX_FCTL;
        break;
    case 2: /* RX FLOW ENABLE */
        fcntl_bits = AG7240_MAC_CFG1_RX_FCTL;
        break;
    case 3: /* tx and rx enable */
        fcntl_bits = AG7240_MAC_CFG1_RX_FCTL | AG7240_MAC_CFG1_TX_FCTL;
        break;
    default: /* tx and rx enable */
        fcntl_bits = AG7240_MAC_CFG1_RX_FCTL | AG7240_MAC_CFG1_TX_FCTL;
        break;
    }
#endif
    if(mac->mac_unit)
    {
        ag7240_reg_wr(mac, AG7240_MAC_CFG1, (AG7240_MAC_CFG1_RX_EN |
            AG7240_MAC_CFG1_TX_EN | fcntl_bits));
        ag7240_reg_rmw_set(mac, AG7240_MAC_CFG2, (AG7240_MAC_CFG2_PAD_CRC_EN |
            AG7240_MAC_CFG2_LEN_CHECK | AG7240_MAC_CFG2_IF_1000));
    } 
    else 
    {

        ag7240_reg_wr(mac, AG7240_MAC_CFG1,
                        (AG7240_MAC_CFG1_RX_EN | AG7240_MAC_CFG1_TX_EN
                            | fcntl_bits));

        ag7240_reg_rmw_set(mac, AG7240_MAC_CFG2, (AG7240_MAC_CFG2_PAD_CRC_EN |
            AG7240_MAC_CFG2_LEN_CHECK));
    }
    ag7240_reg_wr(mac, AG7240_MAC_FIFO_CFG_0, 0x1f00);
    
    /* Disable AG7240_MAC_CFG2_LEN_CHECK to fix the bug that 
     * the mac address is mistaken as length when enabling Atheros header 
     */
    if (mac_has_flag(mac,ATHR_S26_HEADER) || mac_has_flag(mac,ATHR_S16_HEADER))
        ag7240_reg_rmw_clear(mac, AG7240_MAC_CFG2, AG7240_MAC_CFG2_LEN_CHECK)
    /*
     * set the mii if type - NB reg not in the gigE space
     */
    if ((ar7240_reg_rd(AR7240_REV_ID) & AR7240_REV_ID_MASK) == AR7240_REV_1_2) {
        mgmt_cfg_val = 0x2;
        if (mac->mac_unit == 0) {
            ag7240_reg_wr(mac, AG7240_MAC_MII_MGMT_CFG, mgmt_cfg_val | (1 << 31));
            ag7240_reg_wr(mac, AG7240_MAC_MII_MGMT_CFG, mgmt_cfg_val);
        }
    }
    else {
        switch (ar7240_ahb_freq/1000000) {
            case 150:
                mgmt_cfg_val = 0x7;
                break;
            case 175: 
                mgmt_cfg_val = 0x5;
                break;
            case 200: 
                mgmt_cfg_val = 0x4;
                break;
            case 210: 
                mgmt_cfg_val = 0x9;
                break;
            case 220: 
                mgmt_cfg_val = 0x9;
                break;
            default:
                mgmt_cfg_val = 0x7;
        }
        if ((is_ar7241() || is_ar7242())) {

            /* External MII mode */
            if (mac->mac_unit == 0 && is_ar7242()) {
                mgmt_cfg_val = 0x6;
                ar7240_reg_rmw_set(AG7240_ETH_CFG, AG7240_ETH_CFG_RGMII_GE0); 
                ag7240_reg_wr(mac, AG7240_MAC_MII_MGMT_CFG, mgmt_cfg_val | (1 << 31));
                ag7240_reg_wr(mac, AG7240_MAC_MII_MGMT_CFG, mgmt_cfg_val);
            }
            /* Virian */
            mgmt_cfg_val = 0x4;

            if (mac_has_flag(mac,ETH_SWONLY_MODE)) {
                ar7240_reg_rmw_set(AG7240_ETH_CFG, AG7240_ETH_CFG_SW_ONLY_MODE); 
                ag7240_reg_rmw_set(ag7240_macs[0], AG7240_MAC_CFG1, AG7240_MAC_CFG1_SOFT_RST);;
            }
            ag7240_reg_wr(ag7240_macs[1], AG7240_MAC_MII_MGMT_CFG, mgmt_cfg_val | (1 << 31));
            ag7240_reg_wr(ag7240_macs[1], AG7240_MAC_MII_MGMT_CFG, mgmt_cfg_val);
            printk("Virian MDC CFG Value ==> %x\n",mgmt_cfg_val);
        }
        else { /* Python 1.0 & 1.1 */
            if (mac->mac_unit == 0) {
                check_cnt = 0;
                while (check_cnt++ < 10) {
                    ag7240_reg_wr(mac, AG7240_MAC_MII_MGMT_CFG, mgmt_cfg_val | (1 << 31));
                    ag7240_reg_wr(mac, AG7240_MAC_MII_MGMT_CFG, mgmt_cfg_val);
                    if(athrs26_mdc_check() == 0) 
                        break;
                }
                if(check_cnt == 11)
                    printk("%s: MDC check failed\n", __func__);
            }
        }
    }
    ag7240_reg_wr(mac, AG7240_MAC_FIFO_CFG_1, 0x10ffff);
    ag7240_reg_wr(mac, AG7240_MAC_FIFO_CFG_2, 0x015500aa);
    /*
     * Weed out junk frames (CRC errored, short collision'ed frames etc.)
     */
    ag7240_reg_wr(mac, AG7240_MAC_FIFO_CFG_4, 0x3ffff);

    /*
     * Drop CRC Errors, Pause Frames ,Length Error frames, Truncated Frames
     * dribble nibble and rxdv error frames.
     */
    DPRINTF("Setting Drop CRC Errors, Pause Frames and Length Error frames \n");
    if(mac_has_flag(mac,ATHR_S26_HEADER)){
        ag7240_reg_wr(mac, AG7240_MAC_FIFO_CFG_5, 0xe6bc0);
    }else{
        ag7240_reg_wr(mac, AG7240_MAC_FIFO_CFG_5, 0x66b82);
    }
    if (mac->mac_unit == 0 && is_ar7242()){
       ag7240_reg_wr(mac, AG7240_MAC_FIFO_CFG_5, 0xe6be2);
    }
    if (mac_has_flag(mac,WAN_QOS_SOFT_CLASS)) {
    /* Enable Fixed priority */
#if 1
        ag7240_reg_wr(mac,AG7240_DMA_TX_ARB_CFG,AG7240_TX_QOS_MODE_WEIGHTED
                    | AG7240_TX_QOS_WGT_0(0x7)
                    | AG7240_TX_QOS_WGT_1(0x5) 
                    | AG7240_TX_QOS_WGT_2(0x3)
                    | AG7240_TX_QOS_WGT_3(0x1));
#else
        ag7240_reg_wr(mac,AG7240_DMA_TX_ARB_CFG,AG7240_TX_QOS_MODE_FIXED);
#endif

        for(ac = 0;ac < mac->mac_noacs; ac++) {
            tx = &mac->mac_txring[ac];
            t0  =  &tx->ring_desc[0];
            switch(ac) {
                case ENET_AC_VO:
                    ag7240_reg_wr(mac, AG7240_DMA_TX_DESC_Q0, ag7240_desc_dma_addr(tx, t0));
                    break;
                case ENET_AC_VI:
                    ag7240_reg_wr(mac, AG7240_DMA_TX_DESC_Q1, ag7240_desc_dma_addr(tx, t0));
                    break;
                case ENET_AC_BK:
                    ag7240_reg_wr(mac, AG7240_DMA_TX_DESC_Q2, ag7240_desc_dma_addr(tx, t0));
                    break;
                case ENET_AC_BE:
                    ag7240_reg_wr(mac, AG7240_DMA_TX_DESC_Q3, ag7240_desc_dma_addr(tx, t0));
                    break;
            }
        }
    }
    else {
        tx = &mac->mac_txring[0];
        t0  =  &tx->ring_desc[0];
        ag7240_reg_wr(mac, AG7240_DMA_TX_DESC_Q0, ag7240_desc_dma_addr(tx, t0));
    }

    r0  =  &rx->ring_desc[0];
    ag7240_reg_wr(mac, AG7240_DMA_RX_DESC, ag7240_desc_dma_addr(rx, r0));

    DPRINTF(MODULE_NAME ": cfg1 %#x cfg2 %#x\n", ag7240_reg_rd(mac, AG7240_MAC_CFG1),
        ag7240_reg_rd(mac, AG7240_MAC_CFG2));
}

static void
ag7240_hw_stop(ag7240_mac_t *mac)
{
    ag7240_rx_stop(mac);
    ag7240_tx_stop(mac);
    ag7240_int_disable(mac);
    athrs26_disable_linkIntrs(mac->mac_unit);
    /*
     * put everything into reset.
     * Dont Reset WAN MAC as we are using eth0 MDIO to access S26 Registers.
     */
    if(mac->mac_unit == 1 || is_ar7241() || is_ar7242())
        ag7240_reg_rmw_set(mac, AG7240_MAC_CFG1, AG7240_MAC_CFG1_SOFT_RST);
}

/*
 * Several fields need to be programmed based on what the PHY negotiated
 * Ideally we should quiesce everything before touching the pll, but:
 * 1. If its a linkup/linkdown, we dont care about quiescing the traffic.
 * 2. If its a single gigE PHY, this can only happen on lup/ldown.
 * 3. If its a 100Mpbs switch, the link will always remain at 100 (or nothing)
 * 4. If its a gigE switch then the speed should always be set at 1000Mpbs, 
 *    and the switch should provide buffering for slower devices.
 *
 * XXX Only gigE PLL can be changed as a parameter for now. 100/10 is hardcoded.
 * XXX Need defines for them -
 * XXX FIFO settings based on the mode
 */
static void
ag7240_set_mac_from_link(ag7240_mac_t *mac, ag7240_phy_speed_t speed, int fdx)
{
    if(mac->mac_unit == 1)
    speed = AG7240_PHY_SPEED_1000T;

    mac->mac_speed =  speed;
    mac->mac_fdx   =  fdx;

    ag7240_set_mac_duplex(mac, fdx);
    ag7240_reg_wr(mac, AG7240_MAC_FIFO_CFG_3, fifo_3);

    switch (speed)
    {
    case AG7240_PHY_SPEED_1000T:
        ag7240_set_mac_if(mac, 1);
        ag7240_reg_rmw_set(mac, AG7240_MAC_FIFO_CFG_5, (1 << 19));
        if (is_ar7242() &&( mac->mac_unit == 0))
            ar7240_reg_wr(AR7242_ETH_XMII_CONFIG,xmii_val);
        break;

    case AG7240_PHY_SPEED_100TX:
        ag7240_set_mac_if(mac, 0);
        ag7240_set_mac_speed(mac, 1);
        ag7240_reg_rmw_clear(mac, AG7240_MAC_FIFO_CFG_5, (1 << 19));
        if (is_ar7242() &&( mac->mac_unit == 0))
            ar7240_reg_wr(AR7242_ETH_XMII_CONFIG,0x0101);
        break;

    case AG7240_PHY_SPEED_10T:
        ag7240_set_mac_if(mac, 0);
        ag7240_set_mac_speed(mac, 0);
        ag7240_reg_rmw_clear(mac, AG7240_MAC_FIFO_CFG_5, (1 << 19));
        if (is_ar7242() &&( mac->mac_unit == 0))
            ar7240_reg_wr(AR7242_ETH_XMII_CONFIG,0x1616);
        break;

    default:
        assert(0);
    }
    DPRINTF(MODULE_NAME ": cfg_1: %#x\n", ag7240_reg_rd(mac, AG7240_MAC_FIFO_CFG_1));
    DPRINTF(MODULE_NAME ": cfg_2: %#x\n", ag7240_reg_rd(mac, AG7240_MAC_FIFO_CFG_2));
    DPRINTF(MODULE_NAME ": cfg_3: %#x\n", ag7240_reg_rd(mac, AG7240_MAC_FIFO_CFG_3));
    DPRINTF(MODULE_NAME ": cfg_4: %#x\n", ag7240_reg_rd(mac, AG7240_MAC_FIFO_CFG_4));
    DPRINTF(MODULE_NAME ": cfg_5: %#x\n", ag7240_reg_rd(mac, AG7240_MAC_FIFO_CFG_5));
}

static int 
led_control_func(ATH_LED_CONTROL *pledctrl) 
{
    uint32_t i=0,cnt,reg_addr;
    const LED_BLINK_RATES  *bRateTab; 
    static uint32_t pkt_count;
    ag7240_mac_t *mac = ag7240_macs[1];

    if (mac_has_flag(mac,ETH_SOFT_LED)) {
        atomic_inc(&Ledstatus);
  
        /* 
         *  MDIO access will fail While PHY is in RESET phase.
         */
        if(phy_in_reset)
            goto done;

        if (mac->mac_ifup) {
            for (i = 0 ; i < 4 ; i ++) {
                bRateTab = BlinkRateTable_100M;
                if (pledctrl->ledlink[i]) {   
                    reg_addr = 0x2003c + ((i + 1) << 8);
                    /* Rx good byte count */
                    cnt = athrs26_reg_read(reg_addr);
                    
                    reg_addr = 0x20084 + ((i + 1) << 8);
                    /* Tx good byte count */
                    cnt = cnt + athrs26_reg_read(reg_addr);

                    if (cnt == 0) {
                        s26_wr_phy(i,0x19,(s26_rd_phy(i,0x19) | (0x3c0)));
                        continue;
                    }
                    if (pledctrl->speed[i] == AG7240_PHY_SPEED_10T) {
                        bRateTab = BlinkRateTable_10M;
                    }
                    while (bRateTab++) {
                        if (cnt <= bRateTab->rate) {
                            s26_wr_phy(i,0x18,((bRateTab->timeOn << 12)|
                                (bRateTab->timeOff << 8)));
                            s26_wr_phy(i,0x19,(s26_rd_phy(i,0x19) & ~(0x280)));
                            break;
                        }
                    }
                } else {
                    s26_wr_phy(i,0x19,0x0);
                }
            }
            /* Flush all LAN MIB counters */
            athrs26_reg_write(0x80,((1 << 17) | (1 << 24))); 
            while ((athrs26_reg_read(0x80) & 0x20000) == 0x20000);
            athrs26_reg_write(0x80,0);
        }

        mac = ag7240_macs[0];
        bRateTab = BlinkRateTable_100M;
        cnt = 0;
        if (mac->mac_ifup) {
            if (pledctrl->ledlink[4]) {
                cnt += ag7240_reg_rd(mac,AG7240_RX_BYTES_CNTR) +
                               ag7240_reg_rd(mac,AG7240_TX_BYTES_CNTR);

                if (ag7240_get_diff(pkt_count,cnt) == 0) {
                    s26_wr_phy(4,0x19,(s26_rd_phy(4,0x19) | (0x3c0)));
                    goto done;
                }
                if (pledctrl->speed[4] == AG7240_PHY_SPEED_10T) {
                    bRateTab = BlinkRateTable_10M;
                }
                while (bRateTab++) {
                    if (ag7240_get_diff(pkt_count,cnt) <= bRateTab->rate) {
                        s26_wr_phy(4,0x18,((bRateTab->timeOn << 12)|
                            (bRateTab->timeOff << 8)));
                        s26_wr_phy(4,0x19,(s26_rd_phy(4,0x19) & ~(0x280)));
                        break;
                    }
                }
                pkt_count = cnt;
            } else {
                s26_wr_phy(4,0x19,0x0);
            }
        }
    }
done:
    if (mac_has_flag(mac,CHECK_DMA_STATUS)) { 
        if(ag7240_get_diff(prev_dma_chk_ts,jiffies) >= (1*HZ / 2)) {
            if (ag7240_macs[0]->mac_ifup) check_for_dma_status(ag7240_macs[0],0);      
            if (ag7240_macs[1]->mac_ifup) check_for_dma_status(ag7240_macs[1],0);
            prev_dma_chk_ts = jiffies;
        }
    }
    mod_timer(&PLedCtrl.led_timer,(jiffies + AG7240_LED_POLL_SECONDS));
    atomic_dec(&Ledstatus);
    return 0;
}

static int check_dma_status_pause(ag7240_mac_t *mac) { 

    int RxFsm,TxFsm,RxFD,RxCtrl,TxCtrl;

    RxFsm = ag7240_reg_rd(mac,AG7240_DMA_RXFSM);
    TxFsm = ag7240_reg_rd(mac,AG7240_DMA_TXFSM);
    RxFD  = ag7240_reg_rd(mac,AG7240_DMA_XFIFO_DEPTH);
    RxCtrl = ag7240_reg_rd(mac,AG7240_DMA_RX_CTRL);
    TxCtrl = ag7240_reg_rd(mac,AG7240_DMA_TX_CTRL);


    if ((RxFsm & AG7240_DMA_DMA_STATE) == 0x3 
        && ((RxFsm >> 4) & AG7240_DMA_AHB_STATE) == 0x6) {
        return 0;
    }
    else if ((((TxFsm >> 4) & AG7240_DMA_AHB_STATE) <= 0x0) && 
             ((RxFsm & AG7240_DMA_DMA_STATE) == 0x0) && 
             (((RxFsm >> 4) & AG7240_DMA_AHB_STATE) == 0x0) && 
             (RxFD  == 0x0) && (RxCtrl == 1) && (TxCtrl == 1)) {
        return 0;
    }
    else if (((((TxFsm >> 4) & AG7240_DMA_AHB_STATE) <= 0x4) && 
            ((RxFsm & AG7240_DMA_DMA_STATE) == 0x0) && 
            (((RxFsm >> 4) & AG7240_DMA_AHB_STATE) == 0x0)) || 
            (((RxFD >> 16) <= 0x20) && (RxCtrl == 1)) ) {
        return 1;
    }
    else {
        DPRINTF(" FIFO DEPTH = %x",RxFD);
        DPRINTF(" RX DMA CTRL = %x",RxCtrl);
        DPRINTF("mac:%d RxFsm:%x TxFsm:%x\n",mac->mac_unit,RxFsm,TxFsm);
        return 2;
    }
}

static int check_for_dma_status(ag7240_mac_t *mac,int ac) {

    ag7240_ring_t   *r     = &mac->mac_txring[ac];
    int              head  = r->ring_head, tail = r->ring_tail;
    ag7240_desc_t   *ds;
    uint32_t rx_ds;
    ag7240_buffer_t *bp;
    int flags,mask,int_mask;
    unsigned int w1 = 0, w2 = 0;

    /* If Tx hang is asserted reset the MAC and restore the descriptors
     * and interrupt state.
     */
    while (tail != head)
    {
        ds   = &r->ring_desc[tail];
        bp   =  &r->ring_buffer[tail];

        if(ag7240_tx_owned_by_dma(ds)) {
            if ((ag7240_get_diff(bp->trans_start,jiffies)) > ((1 * HZ/10))) {

                 /*
                  * If the DMA is in pause state reset kernel watchdog timer
                  */
        
                if(check_dma_status_pause(mac)) {
                    mac->mac_dev->trans_start = jiffies;
                    return 0;
                } 
                printk(MODULE_NAME ": Tx Dma status eth%d : %s\n",mac->mac_unit,
                            ag7240_tx_stopped(mac) ? "inactive" : "active");                               

                spin_lock_irqsave(&mac->mac_lock, flags);
                                                                                                
                int_mask = ag7240_reg_rd(mac,AG7240_DMA_INTR_MASK);

                ag7240_tx_stop(mac);
                ag7240_rx_stop(mac);

                rx_ds = ag7240_reg_rd(mac,AG7240_DMA_RX_DESC);
                mask = ag7240_reset_mask(mac->mac_unit);

                /*
                 * put into reset, hold, pull out.
                 */
                ar7240_reg_rmw_set(AR7240_RESET, mask);
                udelay(10);
                ar7240_reg_rmw_clear(AR7240_RESET, mask);
                udelay(10);

                ag7240_hw_setup(mac);

                ag7240_reg_wr(mac,AG7240_DMA_TX_DESC,ag7240_desc_dma_addr(r,ds));
                ag7240_reg_wr(mac,AG7240_DMA_RX_DESC,rx_ds);
                /*
                 * set the mac addr
                 */
                 
                addr_to_words(mac->mac_dev->dev_addr, w1, w2);
                ag7240_reg_wr(mac, AG7240_GE_MAC_ADDR1, w1);
                ag7240_reg_wr(mac, AG7240_GE_MAC_ADDR2, w2);

                ag7240_set_mac_from_link(mac, mac->mac_speed, mac->mac_fdx);
                
                mac->dma_check = 1;

                if (mac_has_flag(mac,WAN_QOS_SOFT_CLASS)) {
                    ag7240_tx_start_qos(mac,ac);
                }
                else {
                    ag7240_tx_start(mac);
                }

                ag7240_rx_start(mac);
               
                /*
                 * Restore interrupts
                 */
                ag7240_reg_wr(mac,AG7240_DMA_INTR_MASK,int_mask);

                spin_unlock_irqrestore(&mac->mac_lock,flags);
                break;
            }
        }
        ag7240_ring_incr(tail);
    }
    return 0;
}



/*
 *
 * phy link state management
 */
int
ag7240_check_link(ag7240_mac_t *mac,int phyUnit)
{
    struct net_device  *dev     = mac->mac_dev;
    int                 carrier = netif_carrier_ok(dev), fdx, phy_up;
    ag7240_phy_speed_t  speed;
    int                 rc;

    /* The vitesse switch uses an indirect method to communicate phy status
     * so it is best to limit the number of calls to what is necessary.
     * However a single call returns all three pieces of status information.
     * 
     * This is a trivial change to the other PHYs ergo this change.
     *
     */
  
    rc = ag7240_get_link_status(mac->mac_unit, &phy_up, &fdx, &speed, phyUnit);

    athrs26_phy_stab_wr(phyUnit,phy_up,speed);

    if (rc < 0)
        goto done;

    if (!phy_up)
    {
        if (carrier)
        {
            printk(MODULE_NAME ":unit %d: phy %0d not up carrier %d\n", mac->mac_unit, phyUnit, carrier);

            /* A race condition is hit when the queue is switched on while tx interrupts are enabled.
             * To avoid that disable tx interrupts when phy is down.
             */
            ag7240_intr_disable_tx(mac);

            netif_carrier_off(dev);
            netif_stop_queue(dev);
            if (mac_has_flag(mac,ETH_SOFT_LED)) {
                PLedCtrl.ledlink[phyUnit] = 0;
                s26_wr_phy(phyUnit,0x19,0x0);
            }
        }
        goto done;
    }
   
    if(!mac->mac_ifup)
        goto done; 
    /*
     * phy is up. Either nothing changed or phy setttings changed while we 
     * were sleeping.
     */

    if ((fdx < 0) || (speed < 0))
    {
        printk(MODULE_NAME ": phy not connected?\n");
        return 0;
    }

    if (carrier && (speed == mac->mac_speed) && (fdx == mac->mac_fdx)) 
        goto done;

    if (athrs26_phy_is_link_alive(phyUnit)) 
    {
        printk(MODULE_NAME ": enet unit:%d phy:%d is up...", mac->mac_unit,phyUnit);
        printk("%s %s %s\n", mii_str[mac->mac_unit][mii_if(mac)], 
           spd_str[speed], dup_str[fdx]);

        ag7240_set_mac_from_link(mac, speed, fdx);

        printk(MODULE_NAME ": done cfg2 %#x ifctl %#x miictrl  \n", 
           ag7240_reg_rd(mac, AG7240_MAC_CFG2), 
           ag7240_reg_rd(mac, AG7240_MAC_IFCTL));
        /*
         * in business
         */
        netif_carrier_on(dev);
        netif_start_queue(dev);
        /* 
         * WAR: Enable link LED to glow if speed is negotiated as 10 Mbps 
         */
        if (mac_has_flag(mac,ETH_SOFT_LED)) {
            PLedCtrl.ledlink[phyUnit] = 1;
            PLedCtrl.speed[phyUnit] = speed;

            s26_wr_phy(phyUnit,0x19,0x3c0);
        }
    }
    else {
        if (mac_has_flag(mac,ETH_SOFT_LED)) {
            PLedCtrl.ledlink[phyUnit] = 0;
            s26_wr_phy(phyUnit,0x19,0x0);
        }
        printk(MODULE_NAME ": enet unit:%d phy:%d is down...\n", mac->mac_unit,phyUnit);
    }

done:
    return 0;
}




#if defined(CONFIG_AR7242_RGMII_PHY)||defined(CONFIG_AR7242_S16_PHY)||defined(CONFIG_AR7242_VIR_PHY)   
/*
 * phy link state management
 */
int
ag7242_check_link(ag7240_mac_t *mac)
{
    struct net_device  *dev     = mac->mac_dev;
    int                 carrier = netif_carrier_ok(dev), fdx, phy_up;
    ag7240_phy_speed_t  speed;
    int                 rc,phyUnit = 0;


    rc = ag7240_get_link_status(mac->mac_unit, &phy_up, &fdx, &speed, phyUnit);

    if (rc < 0)
        goto done;

    if (!phy_up)
    {
        if (carrier)
        {
            printk(MODULE_NAME ": unit %d: phy %0d not up carrier %d\n", mac->mac_unit, phyUnit, carrier);

            /* A race condition is hit when the queue is switched on while tx interrupts are enabled.
             * To avoid that disable tx interrupts when phy is down.
             */
            ag7240_intr_disable_tx(mac);

            netif_carrier_off(dev);
            netif_stop_queue(dev);
       }
       goto done;
    }

    if(!mac->mac_ifup) {
        goto done;
    }

    if ((fdx < 0) || (speed < 0))
    {
        printk(MODULE_NAME ": phy not connected?\n");
        return 0;
    }

    if (carrier && (speed == rg_phy_speed ) && (fdx == rg_phy_duplex)) {
        goto done;
    }
#ifdef CONFIG_AR7242_RGMII_PHY
    if (athr_phy_is_link_alive(phyUnit)) 
#endif
    {
        printk(MODULE_NAME ": enet unit:%d is up...\n", mac->mac_unit);
        printk("%s %s %s\n", mii_str[mac->mac_unit][mii_if(mac)],
           spd_str[speed], dup_str[fdx]);

        rg_phy_speed = speed;
        rg_phy_duplex = fdx;
        /* Set GEO to be always at Giga Bit */
        speed = AG7240_PHY_SPEED_1000T;
        ag7240_set_mac_from_link(mac, speed, fdx);

        printk(MODULE_NAME ": done cfg2 %#x ifctl %#x miictrl  \n",
           ag7240_reg_rd(mac, AG7240_MAC_CFG2),
           ag7240_reg_rd(mac, AG7240_MAC_IFCTL));
        /*
         * in business
         */
        netif_carrier_on(dev);
        netif_start_queue(dev);
    }
    
done:
    mod_timer(&mac->mac_phy_timer, jiffies + AG7240_PHY_POLL_SECONDS*HZ);
    return 0;
}

#endif

uint16_t
ag7240_mii_read(int unit, uint32_t phy_addr, uint8_t reg)
{
    ag7240_mac_t *mac   = ag7240_unit2mac(unit);
    uint16_t      addr  = (phy_addr << AG7240_ADDR_SHIFT) | reg, val;
    volatile int           rddata;
    uint16_t      ii = 0x1000;

    ag7240_reg_wr(mac, AG7240_MII_MGMT_CMD, 0x0);
    ag7240_reg_wr(mac, AG7240_MII_MGMT_ADDRESS, addr);
    ag7240_reg_wr(mac, AG7240_MII_MGMT_CMD, AG7240_MGMT_CMD_READ);

    do
    {
        udelay(5);
        rddata = ag7240_reg_rd(mac, AG7240_MII_MGMT_IND) & 0x1;
    }while(rddata && --ii);

    val = ag7240_reg_rd(mac, AG7240_MII_MGMT_STATUS);
    ag7240_reg_wr(mac, AG7240_MII_MGMT_CMD, 0x0);

    return val;
}

void
ag7240_mii_write(int unit, uint32_t phy_addr, uint8_t reg, uint16_t data)
{
    ag7240_mac_t *mac   = ag7240_unit2mac(unit);
    uint16_t      addr  = (phy_addr << AG7240_ADDR_SHIFT) | reg;
    volatile int rddata;
    uint16_t      ii = 0x1000;

    ag7240_reg_wr(mac, AG7240_MII_MGMT_ADDRESS, addr);
    ag7240_reg_wr(mac, AG7240_MII_MGMT_CTRL, data);

    do
    {
        rddata = ag7240_reg_rd(mac, AG7240_MII_MGMT_IND) & 0x1;
    }while(rddata && --ii);
}

/*
 * Tx operation:
 * We do lazy reaping - only when the ring is "thresh" full. If the ring is 
 * full and the hardware is not even done with the first pkt we q'd, we turn
 * on the tx interrupt, stop all q's and wait for h/w to
 * tell us when its done with a "few" pkts, and then turn the Qs on again.
 *
 * Locking:
 * The interrupt only touches the ring when Q's stopped  => Tx is lockless, 
 * except when handling ring full.
 *
 * Desc Flushing: Flushing needs to be handled at various levels, broadly:
 * - The DDr FIFOs for desc reads.
 * - WB's for desc writes.
 */
static void
ag7240_handle_tx_full(ag7240_mac_t *mac)
{
    u32         flags;

    assert(!netif_queue_stopped(mac->mac_dev));

    mac->mac_net_stats.tx_fifo_errors ++;

    netif_stop_queue(mac->mac_dev);

    spin_lock_irqsave(&mac->mac_lock, flags);
    ag7240_intr_enable_tx(mac);
    spin_unlock_irqrestore(&mac->mac_lock, flags);
}

/* ******************************
 * 
 * Code under test - do not use
 *
 * ******************************
 */
static ag7240_desc_t *
ag7240_get_tx_ds(ag7240_mac_t *mac, int *len, unsigned char **start,int ac)
{
    ag7240_desc_t      *ds;
    int                len_this_ds;
    ag7240_ring_t      *r   = &mac->mac_txring[ac];
    ag7240_buffer_t    *bp;

    /* force extra pkt if remainder less than 4 bytes */
    if (*len > tx_len_per_ds)
        if (*len < (tx_len_per_ds + 4))
            len_this_ds = tx_len_per_ds - 4;
        else
            len_this_ds = tx_len_per_ds;
    else
        len_this_ds    = *len;

    ds = &r->ring_desc[r->ring_head];

    ag7240_trc_new(ds,"ds addr");
    ag7240_trc_new(ds,"ds len");
    assert(!ag7240_tx_owned_by_dma(ds));

    ds->pkt_size       = len_this_ds;
    ds->pkt_start_addr = virt_to_phys(*start);
    ds->more           = 1;

    *len   -= len_this_ds;
    *start += len_this_ds;

    /*
     * Time stamp each packet 
     */
    if (mac_has_flag(mac,CHECK_DMA_STATUS)) {
        bp = &r->ring_buffer[r->ring_head];
        bp->trans_start = jiffies; 
    }

    ag7240_ring_incr(r->ring_head);

    return ds;
}

int
ag7240_hard_start(struct sk_buff *skb, struct net_device *dev)
{
    struct ethhdr      *eh;
    ag7240_mac_t       *mac = (ag7240_mac_t *)dev->priv;
    ag7240_ring_t      *r;
    ag7240_buffer_t    *bp;
    ag7240_desc_t      *ds, *fds;
    unsigned char      *start;
    int                len;
    int                nds_this_pkt;
    int                ac = 0;
#ifdef CONFIG_AR7240_S26_VLAN_IGMP
    if(unlikely((skb->len <= 0) || (skb->len > (dev->mtu + ETH_VLAN_HLEN +6 ))))
#else
    if(unlikely((skb->len <= 0) || (skb->len > (dev->mtu + ETH_HLEN + 4))))
#endif
    {
        printk(MODULE_NAME ": bad skb, len %d\n", skb->len);
        goto dropit;
    }

    for (ac = 0;ac < mac->mac_noacs; ac++) {
        if (ag7240_tx_reap_thresh(mac,ac)) 
            ag7240_tx_reap(mac,ac);
    }

    /* 
     * Select the TX based on access category 
     */
    ac = ENET_AC_BE;
    if ( (mac_has_flag(mac,WAN_QOS_SOFT_CLASS)) || (mac_has_flag(mac,ATHR_S26_HEADER))
        || (mac_has_flag(mac,ATHR_S16_HEADER)))  { 
        /* Default priority */
        eh = (struct ethhdr *) skb->data;

        if (eh->h_proto  == __constant_htons(ETHERTYPE_IP))
        {
            const struct iphdr *ip = (struct iphdr *)
                        (skb->data + sizeof (struct ethhdr));
            /*
             * IP frame: exclude ECN bits 0-1 and map DSCP bits 2-7
             * from TOS byte.
             */
            ac = TOS_TO_ENET_AC ((ip->tos >> TOS_ECN_SHIFT) & 0x3F);
        }
    }
    skb->priority=ac;
    /* add header to normal frames sent to LAN*/
    if (mac_has_flag(mac,ATHR_S26_HEADER))
    {
        skb_push(skb, ATHR_HEADER_LEN);
        skb->data[0] = 0x30; /* broadcast = 0; from_cpu = 0; reserved = 1; port_num = 0 */
        skb->data[1] = (0x40 | (ac << HDR_PRIORITY_SHIFT)); /* reserved = 0b10; priority = 0; type = 0 (normal) */
        skb->priority=ENET_AC_BE;
    }

    if (mac_has_flag(mac,ATHR_S16_HEADER))
    {
        skb_push(skb, ATHR_HEADER_LEN);
        memcpy(skb->data,skb->data + ATHR_HEADER_LEN, 12);

        skb->data[12] = 0x30; /* broadcast = 0; from_cpu = 0; reserved = 1; port_num = 0 */
        skb->data[13] = 0x40 | ((ac << HDR_PRIORITY_SHIFT)); /* reserved = 0b10; priority = 0; type = 0 (normal) */
        skb->priority=ENET_AC_BE;
    }
    /*hdr_dump("Tx",mac->mac_unit,skb->data,ac,0);*/
    r = &mac->mac_txring[skb->priority];

    assert(r);

    ag7240_trc_new(r->ring_head,"hard-stop hd");
    ag7240_trc_new(r->ring_tail,"hard-stop tl");

    ag7240_trc_new(skb->len,    "len this pkt");
    ag7240_trc_new(skb->data,   "ptr 2 pkt");

    dma_cache_wback((unsigned long)skb->data, skb->len);

    bp          = &r->ring_buffer[r->ring_head];

    assert(bp);

    bp->buf_pkt = skb;
    len         = skb->len;
    start       = skb->data;

    assert(len>4);

    nds_this_pkt = 1;

    fds = ds = ag7240_get_tx_ds(mac, &len, &start, skb->priority);

    while (len>0)
    {
        ds = ag7240_get_tx_ds(mac, &len, &start, skb->priority);
        nds_this_pkt++;
        ag7240_tx_give_to_dma(ds);
    }

    ds->res1           = 0;
    ds->res2           = 0;
    ds->ftpp_override  = 0;
    ds->res3           = 0;

    ds->more        = 0;
    ag7240_tx_give_to_dma(fds);

    bp->buf_lastds  = ds;
    bp->buf_nds     = nds_this_pkt;

    ag7240_trc_new(ds,"last ds");
    ag7240_trc_new(nds_this_pkt,"nmbr ds for this pkt");

    wmb();

    mac->net_tx_packets ++;
    mac->net_tx_bytes += skb->len;

    ag7240_trc(ag7240_reg_rd(mac, AG7240_DMA_TX_CTRL),"dma idle");
    
    if (mac_has_flag(mac,WAN_QOS_SOFT_CLASS)) {
        ag7240_tx_start_qos(mac,skb->priority);
    }
    else {
        ag7240_tx_start(mac);
    }

    for ( ac = 0;ac < mac->mac_noacs; ac++) {
        if (unlikely(ag7240_tx_ring_full(mac,ac))) 
            ag7240_handle_tx_full(mac);
    }

    dev->trans_start = jiffies;

    return NETDEV_TX_OK;

dropit:
    printk(MODULE_NAME ": dropping skb %08x\n", (unsigned int)skb);
    kfree_skb(skb);
    return NETDEV_TX_OK;
}

/*
 * Interrupt handling:
 * - Recv NAPI style (refer to Documentation/networking/NAPI)
 *
 *   2 Rx interrupts: RX and Overflow (OVF).
 *   - If we get RX and/or OVF, schedule a poll. Turn off _both_ interurpts. 
 *
 *   - When our poll's called, we
 *     a) Have one or more packets to process and replenish
 *     b) The hardware may have stopped because of an OVF.
 *
 *   - We process and replenish as much as we can. For every rcvd pkt 
 *     indicated up the stack, the head moves. For every such slot that we
 *     replenish with an skb, the tail moves. If head catches up with the tail
 *     we're OOM. When all's done, we consider where we're at:
 *
 *      if no OOM:
 *      - if we're out of quota, let the ints be disabled and poll scheduled.
 *      - If we've processed everything, enable ints and cancel poll.
 *
 *      If OOM:
 *      - Start a timer. Cancel poll. Ints still disabled. 
 *        If the hardware's stopped, no point in restarting yet. 
 *
 *      Note that in general, whether we're OOM or not, we still try to
 *      indicate everything recvd, up.
 *
 * Locking: 
 * The interrupt doesnt touch the ring => Rx is lockless
 *
 */
static irqreturn_t
ag7240_intr(int cpl, void *dev_id, struct pt_regs *regs)
{
    struct net_device *dev  = (struct net_device *)dev_id;
    ag7240_mac_t      *mac  = (ag7240_mac_t *)dev->priv;
    int   isr, imr, ac, handled = 0;

    isr   = ag7240_get_isr(mac);
    imr   = ag7240_reg_rd(mac, AG7240_DMA_INTR_MASK);

    ag7240_trc(isr,"isr");
    ag7240_trc(imr,"imr");

    assert(isr == (isr & imr));
    if (isr & (AG7240_INTR_RX_OVF))
    {
        handled = 1;

    if (is_ar7240()) 
        ag7240_reg_wr(mac,AG7240_MAC_CFG1,(ag7240_reg_rd(mac,AG7240_MAC_CFG1)&0xfffffff3));

        ag7240_intr_ack_rxovf(mac);
    }
    if (likely(isr & AG7240_INTR_RX))
    {
        handled = 1;
        if (likely(netif_rx_schedule_prep(dev)))
        {
            ag7240_intr_disable_recv(mac);
            __netif_rx_schedule(dev);
        }
        else
        {
            printk(MODULE_NAME ": driver bug! interrupt while in poll\n");
            assert(0);
            ag7240_intr_disable_recv(mac);
        }
        /*ag7240_recv_packets(dev, mac, 200, &budget);*/
    }
    if (likely(isr & AG7240_INTR_TX))
    {
        handled = 1;
        ag7240_intr_ack_tx(mac);
        /* Which queue to reap ??????????? */
        for(ac = 0; ac < mac->mac_noacs; ac++)
            ag7240_tx_reap(mac,ac);
    }
    if (unlikely(isr & AG7240_INTR_RX_BUS_ERROR))
    {
        assert(0);
        handled = 1;
        ag7240_intr_ack_rxbe(mac);
    }
    if (unlikely(isr & AG7240_INTR_TX_BUS_ERROR))
    {
        assert(0);
        handled = 1;
        ag7240_intr_ack_txbe(mac);
    }

    if (!handled)
    {
        assert(0);
        printk(MODULE_NAME ": unhandled intr isr %#x\n", isr);
    }

    return IRQ_HANDLED;
}

static irqreturn_t
ag7240_link_intr(int cpl, void *dev_id, struct pt_regs *regs) {
    ar7240_s26_intr();
    return IRQ_HANDLED;
}

static int
ag7240_poll(struct net_device *dev, int *budget)
{
    ag7240_mac_t       *mac       = (ag7240_mac_t *)dev->priv;
    int work_done,      max_work  = min(*budget, dev->quota), status = 0;
    ag7240_rx_status_t  ret;
    u32                 flags;

    ret = ag7240_recv_packets(dev, mac, max_work, &work_done);

    dev->quota  -= work_done;
    *budget     -= work_done;

    if (likely(ret == AG7240_RX_STATUS_DONE))
    {
        netif_rx_complete(dev);
        spin_lock_irqsave(&mac->mac_lock, flags);
        ag7240_intr_enable_recv(mac);
        spin_unlock_irqrestore(&mac->mac_lock, flags);
    }
    else if (likely(ret == AG7240_RX_STATUS_NOT_DONE))
    {
        /*
        * We have work left
        */
        status = 1;
    }
    else if (ret == AG7240_RX_STATUS_OOM)
    {
        printk(MODULE_NAME ": oom..?\n");
        /* 
        * Start timer, stop polling, but do not enable rx interrupts.
        */
        mod_timer(&mac->mac_oom_timer, jiffies+1);
        netif_rx_complete(dev);
    }

    return status;
}

static void
ath_remove_hdr(ag7240_mac_t *mac, struct sk_buff *skb) 
{

   if (mac_has_flag(mac,ATHR_S26_HEADER)) {
       skb_pull(skb, 2); /* remove attansic header */
   }
   else if (mac_has_flag(mac,ATHR_S16_HEADER)) {
       int i;

       for (i = 13 ; i > 1; i --) {
           skb->data[i] = skb->data[i - ATHR_HEADER_LEN]; 
       }
       skb_pull(skb,2);
   }

}

static int
ag7240_recv_packets(struct net_device *dev, ag7240_mac_t *mac, 
    int quota, int *work_done)
{
    ag7240_ring_t       *r     = &mac->mac_rxring;
    ag7240_desc_t       *ds;
    ag7240_buffer_t     *bp;
    struct sk_buff      *skb;
    ag7240_rx_status_t   ret   = AG7240_RX_STATUS_DONE;
    int head = r->ring_head, len, status, iquota = quota, more_pkts, rep;

    ag7240_trc(iquota,"iquota");
    status = ag7240_reg_rd(mac, AG7240_DMA_RX_STATUS);

process_pkts:
    ag7240_trc(status,"status");

    /* Dont assert these bits if the DMA check has passed. The DMA
     * check will clear these bits if a hang condition is detected
     * on resetting the MAC.
     */ 
    if(mac->dma_check == 0) {
        assert((status & AG7240_RX_STATUS_PKT_RCVD));
        assert((status >> 16));
    }
    else
        mac->dma_check = 0;
    /*
     * Flush the DDR FIFOs for our gmac
     */
    ar7240_flush_ge(mac->mac_unit);

    assert(quota > 0); /* WCL */


    while(quota)
    {
        ds    = &r->ring_desc[head];

        ag7240_trc(head,"hd");
        ag7240_trc(ds,  "ds");

        if (ag7240_rx_owned_by_dma(ds))
        {
            assert(quota != iquota); /* WCL */
            break;
        }
        ag7240_intr_ack_rx(mac);

        bp                  = &r->ring_buffer[head];
        len                 = ds->pkt_size;
        skb                 = bp->buf_pkt;

        assert(skb);
        skb_put(skb, len - ETHERNET_FCS_SIZE);
        /* 01:00:5e:X:X:X is a multicast MAC address.*/
        if(mac_has_flag(mac,ATHR_S26_HEADER) && (mac->mac_unit == 1)){
            int offset = 14;
            if(ignore_packet_inspection){
                if((skb->data[2]==0x01)&&(skb->data[3]==0x00)&&(skb->data[4]==0x5e)){
        
                    /*If vlan, calculate the offset of the vlan header. */
                    if((skb->data[14]==0x81)&&(skb->data[15]==0x00)){
                        offset +=4;             
                    }

                    /* If pppoe exists, calculate the offset of the pppoe header. */
                    if((skb->data[offset]==0x88)&&((skb->data[offset+1]==0x63)|(skb->data[offset+1]==0x64))){
                        offset += sizeof(struct pppoe_hdr);             
                    }

                    /* Parse IP layer.  */
                    if((skb->data[offset]==0x08)&&(skb->data[offset+1]==0x00)){
                        struct iphdr * iph;
                        struct igmphdr *igh;

                        offset +=2;
                        iph =(struct iphdr *)(skb->data+offset);
                        offset += (iph->ihl<<2);
                        igh =(struct igmphdr *)(skb->data + offset); 

                        /* Sanity Check and insert port information into igmp checksum. */
                        if((iph->ihl>4)&&(iph->version==4)&&(iph->protocol==2)){
                            igh->csum = 0x7d50|(skb->data[0]&0xf);
                            printk("[eth1] Type %x port %x.\n",igh->type,skb->data[0]&0xf);
                        }
            
                    }                               
                }
            }
            if(mac_has_flag(mac,ATHR_S26_HEADER))
            {
                int tos;
                struct ethhdr       *eh;
                unsigned short diffs[2];
     
                if ((skb->data[1] & HDR_PACKET_TYPE_MASK) == NORMAL_PACKET) {
                    /* Default priority */
                    //printk("[R] head ac: %d\n", ((skb->data[1] >> HDR_PRIORITY_SHIFT) & HDR_PRIORITY_MASK));
                    tos = AC_TO_ENET_TOS (((skb->data[1] >> HDR_PRIORITY_SHIFT) & HDR_PRIORITY_MASK));
                    skb_pull(skb, 2);
                    eh = (struct ethhdr *)skb->data;
                    if (eh->h_proto  == __constant_htons(ETHERTYPE_IP))
                    {
                        struct iphdr *ip = (struct iphdr *)
                                (skb->data + sizeof (struct ethhdr));
                        /*
                         * IP frame: exclude ECN bits 0-1 and map DSCP bits 2-7
                         * from TOS byte.
                         */
                        tos = (tos << TOS_ECN_SHIFT ) & TOS_ECN_MASK;
                        if ( tos != ip->tos ) {
                            diffs[0] = htons(ip->tos) ^ 0xFFFF;
                            ip->tos = ((ip->tos &  ~(TOS_ECN_MASK)) | tos);
                            diffs[1] = htons(ip->tos);
                            ip->check = csum_fold(csum_partial((char *)diffs,
                                      sizeof(diffs),ip->check ^ 0xFFFF));
                        }  
                    }
                    skb->priority = tos;
                }else{
                    skb_pull(skb,2);
                }
            }   
        }
#ifdef CONFIG_AR7240_S26_VLAN_IGMP  
    /* For swith S26, if vlan_id == 0, we will remove the tag header. */
        if(mac->mac_unit==1)
        {
            if((skb->data[12]==0x81)&&(skb->data[13]==0x00)&&((skb->data[14]&0xf)==0x00)&&(skb->data[15]==0x00))
            {
                memmove(&skb->data[4],&skb->data[0],12);
                skb_pull(skb,4);
            }
        }  
#endif

        mac->net_rx_packets ++;
        mac->net_rx_bytes += skb->len;
        /*
         * also pulls the ether header
         */
        skb->protocol       = eth_type_trans(skb, dev);
        skb->dev            = dev;
        bp->buf_pkt         = NULL;
        dev->last_rx        = jiffies;
        quota--;
        ethdbg_check_rxdrop(skb);
        netif_receive_skb(skb);
        ag7240_ring_incr(head);
    }

    assert(iquota != quota);
    r->ring_head   =  head;

    rep = ag7240_rx_replenish(mac);

    /*
    * let's see what changed while we were slogging.
    * ack Rx in the loop above is no flush version. It will get flushed now.
    */
    status       =  ag7240_reg_rd(mac, AG7240_DMA_RX_STATUS);
    more_pkts    =  (status & AG7240_RX_STATUS_PKT_RCVD);

    ag7240_trc(more_pkts,"more_pkts");

    if (!more_pkts) goto done;
    /*
    * more pkts arrived; if we have quota left, get rolling again
    */
    if (quota)      goto process_pkts;
    /*
    * out of quota
    */
    ret = AG7240_RX_STATUS_NOT_DONE;

done:
    *work_done   = (iquota - quota);

    if (unlikely(ag7240_rx_ring_full(mac))) 
        return AG7240_RX_STATUS_OOM;
    /*
    * !oom; if stopped, restart h/w
    */

    if (unlikely(status & AG7240_RX_STATUS_OVF))
    {
        mac->net_rx_over_errors ++;
        ag7240_intr_ack_rxovf(mac);
        ag7240_rx_start(mac);
    }

    return ret;
}

static struct sk_buff *
    ag7240_buffer_alloc(void)
{
    struct sk_buff *skb;

    skb = dev_alloc_skb(AG7240_RX_BUF_SIZE);
    if (unlikely(!skb))
        return NULL;
    skb_reserve(skb, AG7240_RX_RESERVE);

    return skb;
}

static void
ag7240_buffer_free(struct sk_buff *skb)
{
    if (in_irq())
        dev_kfree_skb_irq(skb);
    else
        dev_kfree_skb(skb);
}

/*
 * Head is the first slot with a valid buffer. Tail is the last slot 
 * replenished. Tries to refill buffers from tail to head.
 */
static int
ag7240_rx_replenish(ag7240_mac_t *mac)
{
    ag7240_ring_t   *r     = &mac->mac_rxring;
    int              head  = r->ring_head, tail = r->ring_tail, refilled = 0;
    ag7240_desc_t   *ds;
    ag7240_buffer_t *bf;

    ag7240_trc(head,"hd");
    ag7240_trc(tail,"tl");

    while ( tail != head )
    {
        bf                  = &r->ring_buffer[tail];
        ds                  = &r->ring_desc[tail];

        ag7240_trc(ds,"ds");

        assert(!ag7240_rx_owned_by_dma(ds));

        assert(!bf->buf_pkt);

        bf->buf_pkt         = ag7240_buffer_alloc();
        if (!bf->buf_pkt)
        {
            printk(MODULE_NAME ": outta skbs!\n");
            break;
        }
        dma_cache_inv((unsigned long)bf->buf_pkt->data, AG7240_RX_BUF_SIZE);
        ds->pkt_start_addr  = virt_to_phys(bf->buf_pkt->data);

        ag7240_rx_give_to_dma(ds);
        refilled ++;

        ag7240_ring_incr(tail);

    }
    /*
     * Flush descriptors
     */
    wmb();
    if (is_ar7240()) {
        ag7240_reg_wr(mac,AG7240_MAC_CFG1,(ag7240_reg_rd(mac,AG7240_MAC_CFG1)|0xc));
        ag7240_rx_start(mac);
    }

    r->ring_tail = tail;
    ag7240_trc(refilled,"refilled");

    return refilled;
}

/* 
 * Reap from tail till the head or whenever we encounter an unxmited packet.
 */
static int
ag7240_tx_reap(ag7240_mac_t *mac,int ac)
{    
    ag7240_ring_t   *r;
    ag7240_desc_t   *ds;
    ag7240_buffer_t *bf;
    int head, tail, reaped = 0, i;
    uint32_t flags;

    r = &mac->mac_txring[ac];
    head = r->ring_head;
    tail = r->ring_tail;

    ag7240_trc_new(head,"hd");
    ag7240_trc_new(tail,"tl");

    ar7240_flush_ge(mac->mac_unit);

    while(tail != head)
    {
        ds   = &r->ring_desc[tail];

        ag7240_trc_new(ds,"ds");

        if(ag7240_tx_owned_by_dma(ds))
            break;

        bf      = &r->ring_buffer[tail];
        assert(bf->buf_pkt);

        ag7240_trc_new(bf->buf_lastds,"lastds");

        if(ag7240_tx_owned_by_dma(bf->buf_lastds))
            break;

        for(i = 0; i < bf->buf_nds; i++)
        {
            ag7240_intr_ack_tx(mac);
            ag7240_ring_incr(tail);
        }

        ag7240_buffer_free(bf->buf_pkt);
        bf->buf_pkt = NULL;

        reaped ++;
    }

    r->ring_tail = tail;

    if (netif_queue_stopped(mac->mac_dev) &&
        (ag7240_ndesc_unused(mac, r) >= AG7240_TX_QSTART_THRESH) &&
        netif_carrier_ok(mac->mac_dev))
    {
        if (ag7240_reg_rd(mac, AG7240_DMA_INTR_MASK) & AG7240_INTR_TX)
        {
            spin_lock_irqsave(&mac->mac_lock, flags);
            ag7240_intr_disable_tx(mac);
            spin_unlock_irqrestore(&mac->mac_lock, flags);
        }
        netif_wake_queue(mac->mac_dev);
    }

    return reaped;
}

/*
 * allocate and init rings, descriptors etc.
 */
static int
ag7240_tx_alloc(ag7240_mac_t *mac)
{
    ag7240_ring_t *r ;
    ag7240_desc_t *ds;
    int ac, i, next;

    for(ac = 0;ac < mac->mac_noacs; ac++) {

       r  = &mac->mac_txring[ac];
       if (ag7240_ring_alloc(r, AG7240_TX_DESC_CNT))
           return 1;

       ag7240_trc(r->ring_desc,"ring_desc");

       ds = r->ring_desc;
       for(i = 0; i < r->ring_nelem; i++ )
       {
            ag7240_trc_new(ds,"tx alloc ds");
            next                =   (i == (r->ring_nelem - 1)) ? 0 : (i + 1);
            ds[i].next_desc     =   ag7240_desc_dma_addr(r, &ds[next]);
            ag7240_tx_own(&ds[i]);
       }
   }
   return 0;
}

static int
ag7240_rx_alloc(ag7240_mac_t *mac)
{
    ag7240_ring_t *r  = &mac->mac_rxring;
    ag7240_desc_t *ds;
    int i, next, tail = r->ring_tail;
    ag7240_buffer_t *bf;

    if (ag7240_ring_alloc(r, AG7240_RX_DESC_CNT))
        return 1;

    ag7240_trc(r->ring_desc,"ring_desc");

    ds = r->ring_desc;
    for(i = 0; i < r->ring_nelem; i++ )
    {
        next                =   (i == (r->ring_nelem - 1)) ? 0 : (i + 1);
        ds[i].next_desc     =   ag7240_desc_dma_addr(r, &ds[next]);
    }

    for (i = 0; i < AG7240_RX_DESC_CNT; i++)
    {
        bf                  = &r->ring_buffer[tail];
        ds                  = &r->ring_desc[tail];

        bf->buf_pkt         = ag7240_buffer_alloc();
        if (!bf->buf_pkt) 
            goto error;

        dma_cache_inv((unsigned long)bf->buf_pkt->data, AG7240_RX_BUF_SIZE);
        ds->pkt_start_addr  = virt_to_phys(bf->buf_pkt->data);

        ds->res1           = 0;
        ds->res2           = 0;
        ds->ftpp_override  = 0;
        ds->res3           = 0;
    ds->more    = 0;
        ag7240_rx_give_to_dma(ds);
        ag7240_ring_incr(tail);
    }

    return 0;
error:
    printk(MODULE_NAME ": unable to allocate rx\n");
    ag7240_rx_free(mac);
    return 1;
}

static void
ag7240_tx_free(ag7240_mac_t *mac)
{
    int ac;
    
    for(ac = 0;ac < mac->mac_noacs; ac++) {
        ag7240_ring_release(mac, &mac->mac_txring[ac]);
        ag7240_ring_free(&mac->mac_txring[ac]);
    }
}

static void
ag7240_rx_free(ag7240_mac_t *mac)
{
    ag7240_ring_release(mac, &mac->mac_rxring);
    ag7240_ring_free(&mac->mac_rxring);
}

static int
ag7240_ring_alloc(ag7240_ring_t *r, int count)
{
    int desc_alloc_size, buf_alloc_size;

    desc_alloc_size = sizeof(ag7240_desc_t)   * count;
    buf_alloc_size  = sizeof(ag7240_buffer_t) * count;

    memset(r, 0, sizeof(ag7240_ring_t));

    r->ring_buffer = (ag7240_buffer_t *)kmalloc(buf_alloc_size, GFP_KERNEL);
    printk("%s Allocated %d at 0x%lx\n",__func__,buf_alloc_size,(unsigned long) r->ring_buffer);
    if (!r->ring_buffer)
    {
        printk(MODULE_NAME ": unable to allocate buffers\n");
        return 1;
    }

    r->ring_desc  =  (ag7240_desc_t *)dma_alloc_coherent(NULL, 
        desc_alloc_size,
        &r->ring_desc_dma, 
        GFP_DMA);
    if (! r->ring_desc)
    {
        printk(MODULE_NAME ": unable to allocate coherent descs\n");
        kfree(r->ring_buffer);
        printk("%s Freeing at 0x%lx\n",__func__,(unsigned long) r->ring_buffer);
        return 1;
    }

    memset(r->ring_buffer, 0, buf_alloc_size);
    memset(r->ring_desc,   0, desc_alloc_size);
    r->ring_nelem   = count;

    return 0;
}

static void
ag7240_ring_release(ag7240_mac_t *mac, ag7240_ring_t  *r)
{
    int i;

    for(i = 0; i < r->ring_nelem; i++)
        if (r->ring_buffer[i].buf_pkt)
            ag7240_buffer_free(r->ring_buffer[i].buf_pkt);
}

static void
ag7240_ring_free(ag7240_ring_t *r)
{
    dma_free_coherent(NULL, sizeof(ag7240_desc_t)*r->ring_nelem, r->ring_desc,
        r->ring_desc_dma);
    kfree(r->ring_buffer);
    printk("%s Freeing at 0x%lx\n",__func__,(unsigned long) r->ring_buffer);
}

/*
 * Error timers
 */
static void
ag7240_oom_timer(unsigned long data)
{
    ag7240_mac_t *mac = (ag7240_mac_t *)data;
    int val;

    ag7240_trc(data,"data");
    ag7240_rx_replenish(mac);
    if (ag7240_rx_ring_full(mac))
    {
        val = mod_timer(&mac->mac_oom_timer, jiffies+1);
        assert(!val);
    }
    else
        netif_rx_schedule(mac->mac_dev);
}

static void
ag7240_tx_timeout(struct net_device *dev)
{
    ag7240_mac_t *mac = (ag7240_mac_t *)dev->priv;
    ag7240_trc(dev,"dev");
    printk("%s\n",__func__);
    /* 
     * Do the reset outside of interrupt context 
     */
    schedule_work(&mac->mac_tx_timeout);
}

static void
ag7240_tx_timeout_task(ag7240_mac_t *mac)
{
    ag7240_trc(mac,"mac");
    check_for_dma_status(mac,0);
}

static void
ag7240_get_default_macaddr(ag7240_mac_t *mac, u8 *mac_addr)
{
    /* Use MAC address stored in Flash */
    
#ifdef CONFIG_AG7240_MAC_LOCATION
    u8 *eep_mac_addr = (u8 *)( CONFIG_AG7240_MAC_LOCATION + (mac->mac_unit)*6);
#else
    u8 *eep_mac_addr = (u8 *)((mac->mac_unit) ? AR7240_EEPROM_GE1_MAC_ADDR:
        AR7240_EEPROM_GE0_MAC_ADDR);
#endif

    printk(MODULE_NAME "CHH: Mac address for unit %d\n",mac->mac_unit);
    printk(MODULE_NAME "CHH: %02x:%02x:%02x:%02x:%02x:%02x \n",
        eep_mac_addr[0],eep_mac_addr[1],eep_mac_addr[2],
        eep_mac_addr[3],eep_mac_addr[4],eep_mac_addr[5]);
        
    /*
    ** Check for a valid manufacturer prefix.  If not, then use the defaults
    */
    
    if(eep_mac_addr[0] == 0x00 && 
       eep_mac_addr[1] == 0x03 && 
       eep_mac_addr[2] == 0x7f)
    {
        memcpy(mac_addr, eep_mac_addr, 6);
    }
    else
    {
        /* Use Default address at top of range */
        mac_addr[0] = 0x00;
        mac_addr[1] = 0x03;
        mac_addr[2] = 0x7F;
        mac_addr[3] = 0xFF;
        mac_addr[4] = 0xFF;
        mac_addr[5] = 0xFF - mac->mac_unit;
    }
}

static int
ag7240_do_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
    return (athrs_do_ioctl(dev,ifr,cmd));
}

static struct net_device_stats 
    *ag7240_get_stats(struct net_device *dev)
{

    ag7240_mac_t *mac = dev->priv;
    struct Qdisc *sch;
    int carrier = netif_carrier_ok(dev);

    if (mac->mac_speed < 1) {
        sch = rcu_dereference(dev->qdisc);
        mac->mac_net_stats.tx_dropped = sch->qstats.drops;
        return &mac->mac_net_stats;
    }

    /* 
     *  MIB registers reads will fail if link is lost while resetting PHY.
     */
    if (carrier && !phy_in_reset)
    {

        mac->mac_net_stats.rx_packets = ag7240_reg_rd(mac,AG7240_RX_PKT_CNTR);
        mac->mac_net_stats.tx_packets = ag7240_reg_rd(mac,AG7240_TX_PKT_CNTR);
        mac->mac_net_stats.rx_bytes   = ag7240_reg_rd(mac,AG7240_RX_BYTES_CNTR);
        mac->mac_net_stats.tx_bytes   = ag7240_reg_rd(mac,AG7240_TX_BYTES_CNTR);
        mac->mac_net_stats.tx_errors  = ag7240_reg_rd(mac,AG7240_TX_CRC_ERR_CNTR);
        mac->mac_net_stats.rx_dropped = ag7240_reg_rd(mac,AG7240_RX_DROP_CNTR);
        mac->mac_net_stats.tx_dropped = ag7240_reg_rd(mac,AG7240_TX_DROP_CNTR);
        mac->mac_net_stats.collisions = ag7240_reg_rd(mac,AG7240_TOTAL_COL_CNTR);
    
            /* detailed rx_errors: */
        mac->mac_net_stats.rx_length_errors = ag7240_reg_rd(mac,AG7240_RX_LEN_ERR_CNTR);
        mac->mac_net_stats.rx_over_errors   = ag7240_reg_rd(mac,AG7240_RX_OVL_ERR_CNTR);
        mac->mac_net_stats.rx_crc_errors    = ag7240_reg_rd(mac,AG7240_RX_CRC_ERR_CNTR);
        mac->mac_net_stats.rx_frame_errors  = ag7240_reg_rd(mac,AG7240_RX_FRM_ERR_CNTR);
    
        mac->mac_net_stats.rx_errors  = ag7240_reg_rd(mac,AG7240_RX_CODE_ERR_CNTR) + 
                                        ag7240_reg_rd(mac,AG7240_RX_CRS_ERR_CNTR) +      
                                        mac->mac_net_stats.rx_length_errors +
                                        mac->mac_net_stats.rx_over_errors +
                                        mac->mac_net_stats.rx_crc_errors +
                                        mac->mac_net_stats.rx_frame_errors; 
    
        mac->mac_net_stats.multicast  = ag7240_reg_rd(mac,AG7240_RX_MULT_CNTR) +
                                        ag7240_reg_rd(mac,AG7240_TX_MULT_CNTR);

    }
    return &mac->mac_net_stats;
}

static void
ag7240_vet_tx_len_per_pkt(unsigned int *len)
{
    unsigned int l;

    /* make it into words */
    l = *len & ~3;

    /* 
    * Not too small 
    */
    if (l < AG7240_TX_MIN_DS_LEN) {
        l = AG7240_TX_MIN_DS_LEN;
    }
    else {
    /* Avoid len where we know we will deadlock, that
    * is the range between fif_len/2 and the MTU size
    */
        if (l > AG7240_TX_FIFO_LEN/2) {
            if (l < AG7240_TX_MTU_LEN)
                l = AG7240_TX_MTU_LEN;
            else if (l > AG7240_TX_MAX_DS_LEN)
                l = AG7240_TX_MAX_DS_LEN;
                *len = l;
        }
    }
}

/* This function is used to collect stats from the received skbuff after the marker
 * filtering has been completed 
 */
void athr_vi_dbg_stats(struct sk_buff *skb, int stream_num)
{
    u_int8_t *payload;
    u_int32_t i;
    u_int32_t rx_seq_num = 0;
    int32_t   diff;
    static u_int32_t prev_rx_seq_num[MAX_NUM_STREAMS] = {0, 0, 0, 0}; 
    static u_int32_t current_seq_num[MAX_NUM_STREAMS]   = {0, 0, 0, 0}; 
    static u_int32_t last_seq_num[MAX_NUM_STREAMS]    = {0, 0, 0, 0};
    static u_int8_t start[MAX_NUM_STREAMS]           = {0, 0, 0, 0};
    static u_int16_t offset, num_bytes;
	
    if (!(eth_vi_dbg_params.vi_dbg_cfg & 1)) {
        return;  
    }    
   /* If restart is set, re-initialize internal variables & reset the restart parameter */
    if (eth_vi_dbg_params.vi_dbg_restart) 	{
	    for (i = 0; i < MAX_NUM_STREAMS; i++) {
	        start[i]           = 0;
	        prev_rx_seq_num[i] = 0;
	        current_seq_num[i]   = 0;
	        last_seq_num[i]    = 0;
	    }	
        eth_rx_drop = 0;
        eth_vi_dbg_params.vi_dbg_restart = 0;
    }

    payload   = (u_int8_t *)skb->data;
    offset    =  eth_vi_dbg_params.vi_dbg.rxseq_offset;
    num_bytes =  eth_vi_dbg_params.vi_dbg.rxseq_num_bytes;
	
    for (i = 0; i < num_bytes; i++) {
	    rx_seq_num += (payload[offset + i] << (8 * (3 - i)));
    }

    rx_seq_num = rx_seq_num >> eth_vi_dbg_params.vi_rx_seq_rshift;
    if (start[stream_num] == 0)
    { // 31st bit is set for iperf 1st packet
        printk("stream :%d  start seq no : 0x%08x \n",stream_num+1,rx_seq_num);
        if (rx_seq_num & 0x80000000)
            rx_seq_num &=0x7fffffff;
      
    }
    current_seq_num[stream_num] = rx_seq_num;
    diff = current_seq_num[stream_num] - prev_rx_seq_num[stream_num];

    if ((diff < 0) && start[stream_num] == 1)
    {  // wrap case & Loss in Wrap
        diff = eth_vi_dbg_params.vi_rx_seq_max-prev_rx_seq_num[stream_num]+current_seq_num[stream_num];
        // not handling out of order case
    }
        
    if ((diff > 1) && start[stream_num] == 1) 
    {
        eth_rx_drop += diff;
        eth_vi_dbg_params.vi_rx_seq_drop = 1;
    }

    if (diff == 0 && start[stream_num] == 1) {
       //Rx duplicate pkt
    }
    prev_rx_seq_num[stream_num] = current_seq_num[stream_num];
    start[stream_num] = 1;

}

/* This function is used to filter the received skbuff based on the defined markers.
 * Normally we will give UDP source/dest port as a marker, In specified location we will check for
 * patteren, right now we are supporting 4 streams with 4 different markers.
 */ 
u_int32_t
athr_vi_dbg_check_markers(struct sk_buff *skb)
{
    u_int8_t *payload;
    u_int8_t match;
    u_int8_t i, j, s; 
    payload = (u_int8_t *)skb->data;
    u_int32_t mfail;

    for (s = 0; s < eth_vi_dbg_params.vi_num_streams; s++) {
	    mfail = 0;  /* Reset marker failures for new stream. mfail is used to reduce checks if maker checks
	                 * fail for any stream. If a filure is detected immediately move on to the markers for
	  	             * the next stream */ 
	    for (i = 0; i < eth_vi_dbg_params.vi_num_markers; i++) {
	        for (j = 0; j < eth_vi_dbg_params.vi_dbg.markers[s][i].num_bytes; j++) {
	            match = ((eth_vi_dbg_params.vi_dbg.markers[s][i].match) >> (8*(eth_vi_dbg_params.vi_dbg.markers[s][i].num_bytes-1-j))) & 0x000000FF;
		        if (payload[ eth_vi_dbg_params.vi_dbg.markers[s][i].offset + j] != match) {
		            mfail = 1;  
		            break;
		        }
	        }
	        if (mfail == 1) {
		        break;       
	        }
	    }
	    
	    if (!mfail) {
		   return s;
	    }
    }
    return -1;
}
/*
 * This function used to debug iperf3/chariot/TestMepegStreamer streams using match patteren
 * configured by the application.
 * Get sequence number from specified location and compare the with the previous sequence number 
 * for packet drops.
 */

void ethdbg_check_rxdrop(struct sk_buff *skb)
{
    int stream;

    if (!(eth_vi_dbg_params.vi_dbg_cfg & 1)) {
        return;  
    }    
    stream = athr_vi_dbg_check_markers(skb);

    if (stream != -1) {
	    athr_vi_dbg_stats(skb, stream);	
    }

}
/*
 * All allocations (except irq and rings).
 */
static int __init
ag7240_init(void)
{
    int i,st;
    struct net_device *dev;
    ag7240_mac_t      *mac;
    uint32_t mask;

    /* 
    * tx_len_per_ds is the number of bytes per data transfer in word increments.
    * 
    * If the value is 0 than we default the value to a known good value based
    * on benchmarks. Otherwise we use the value specified - within some 
    * cosntraints of course.
    *
    * Tested working values are 256, 512, 768, 1024 & 1536.
    *
    * A value of 256 worked best in all benchmarks. That is the default.
    *
    */

    /* Tested 256, 512, 768, 1024, 1536 OK, 1152 and 1280 failed*/
    if (0 == tx_len_per_ds)
        tx_len_per_ds = CONFIG_AG7240_LEN_PER_TX_DS;

    ag7240_vet_tx_len_per_pkt( &tx_len_per_ds);

    printk(MODULE_NAME ": Length per segment %d\n", tx_len_per_ds);

    /* 
    * Compute the number of descriptors for an MTU 
    */
    tx_max_desc_per_ds_pkt =1;

    printk(MODULE_NAME ": Max segments per packet %d\n", tx_max_desc_per_ds_pkt);
    printk(MODULE_NAME ": Max tx descriptor count    %d\n", AG7240_TX_DESC_CNT);
    printk(MODULE_NAME ": Max rx descriptor count    %d\n", AG7240_RX_DESC_CNT);

    /* 
    * Let hydra know how much to put into the fifo in words (for tx) 
    */
    if (0 == fifo_3)
        fifo_3 = 0x000001ff | ((AG7240_TX_FIFO_LEN-tx_len_per_ds)/4)<<16;

    printk(MODULE_NAME ": fifo cfg 3 %08x\n", fifo_3);

    /* 
    ** Do the rest of the initializations 
    */

    for(i = 0; i < AG7240_NMACS; i++)
    {
        mac = kmalloc(sizeof(ag7240_mac_t), GFP_KERNEL);
        if (!mac)
        {
            printk(MODULE_NAME ": unable to allocate mac\n");
            return 1;
        }
        memset(mac, 0, sizeof(ag7240_mac_t));

        mac->mac_unit               =  i;
        mac->mac_base               =  ag7240_mac_base(i);
        mac->mac_irq                =  ag7240_mac_irq(i);
        mac->mac_noacs              =  1;
        ag7240_macs[i]              =  mac;
#ifdef CONFIG_ATHEROS_HEADER_EN
        if (mac->mac_unit == ENET_UNIT_LAN)    
            mac_set_flag(mac,ATHR_S26_HEADER);
#endif 
#ifdef CONFIG_ETH_SOFT_LED
        if (is_ar7240())
            mac_set_flag(mac,ETH_SOFT_LED);
#endif
#ifdef CONFIG_CHECK_DMA_STATUS 
        mac_set_flag(mac,CHECK_DMA_STATUS);
#endif
#ifdef CONFIG_S26_SWITCH_ONLY_MODE
        if (is_ar7241()) {
            if(i) {
                mac_set_flag(mac,ETH_SWONLY_MODE);
                }
            else {
                continue;
            }
        }
#endif
        spin_lock_init(&mac->mac_lock);
        /*
        * out of memory timer
        */
        init_timer(&mac->mac_oom_timer);
        mac->mac_oom_timer.data     = (unsigned long)mac;
        mac->mac_oom_timer.function = (void *)ag7240_oom_timer;
        /*
        * watchdog task
        */

        INIT_WORK(&mac->mac_tx_timeout, (void *)ag7240_tx_timeout_task, mac);

        dev = alloc_etherdev(0);
        if (!dev)
        {
            kfree(mac);
            printk("%s Freeing at 0x%lx\n",__func__,(unsigned long) mac);
            printk(MODULE_NAME ": unable to allocate etherdev\n");
            return 1;
        }

        mac->mac_dev         =  dev;
        dev->get_stats       =  ag7240_get_stats;
        dev->open            =  ag7240_open;
        dev->stop            =  ag7240_stop;
        dev->hard_start_xmit =  ag7240_hard_start;
        dev->do_ioctl        =  ag7240_do_ioctl;
        dev->poll            =  ag7240_poll;
        dev->weight          =  AG7240_NAPI_WEIGHT;
        dev->tx_timeout      =  ag7240_tx_timeout;
        dev->priv            =  mac;

        ag7240_get_default_macaddr(mac, dev->dev_addr);

        if (register_netdev(dev))
        {
            printk(MODULE_NAME ": register netdev failed\n");
            goto failed;
        }
#ifdef CONFIG_ATHRS_QOS
        athr_register_qos(mac);
#endif

    netif_carrier_off(dev);
        ag7240_reg_rmw_set(mac, AG7240_MAC_CFG1, AG7240_MAC_CFG1_SOFT_RST 
                | AG7240_MAC_CFG1_RX_RST | AG7240_MAC_CFG1_TX_RST);

        udelay(20);
        mask = ag7240_reset_mask(mac->mac_unit);

        /*
        * put into reset, hold, pull out.
        */
        ar7240_reg_rmw_set(AR7240_RESET, mask);
        mdelay(100);
        ar7240_reg_rmw_clear(AR7240_RESET, mask);
        mdelay(100);
#if CONFIG_FLOWMAC_MODULE
        printk("%s i %d eth0_flow_control %d eth1_flow_control %d flowmac_on %d\n",
                __func__, i, eth0_flow_control, eth1_flow_control, flowmac_on);
        switch (i) {
            case 0:
                if (eth0_flow_control && flowmac_on) 
                    ag7240_flowmac_register(dev);
                break;
            case 1:
                if (eth1_flow_control && flowmac_on)
                    ag7240_flowmac_register(dev);
                break;
            default:
                printk("Warning: in corrent mac unit number \n");
                break;
        }
#endif
    }

    /*
     * Enable link interrupts for PHYs 
     */
    dev = ag7240_macs[0]->mac_dev;
    st = request_irq(AR7240_MISC_IRQ_ENET_LINK, ag7240_link_intr, 0, dev->name, dev);

    if (st < 0)
    {
        printk(MODULE_NAME ": request irq %d failed %d\n", AR7240_MISC_IRQ_ENET_LINK, st);
        goto failed;
    }

    ag7240_trc_init();

    athrs_reg_dev(ag7240_macs);

    if (mac_has_flag(mac,CHECK_DMA_STATUS))
        prev_dma_chk_ts = jiffies;

    if (mac_has_flag(mac,ETH_SOFT_LED)) {
        init_timer(&PLedCtrl.led_timer);
        PLedCtrl.led_timer.data     = (unsigned long)(&PLedCtrl);
        PLedCtrl.led_timer.function = (void *)led_control_func;
        mod_timer(&PLedCtrl.led_timer,(jiffies + AG7240_LED_POLL_SECONDS));
    }
    return 0;
failed:
    free_irq(AR7240_MISC_IRQ_ENET_LINK, ag7240_macs[0]->mac_dev);
    for(i = 0; i < AG7240_NMACS; i++)
    {
#ifdef CONFIG_S26_SWITCH_ONLY_MODE
        if (is_ar7241() && i == 0)
            continue;
#endif
        if (!ag7240_macs[i]) 
            continue;
        if (ag7240_macs[i]->mac_dev) 
            free_netdev(ag7240_macs[i]->mac_dev);
        kfree(ag7240_macs[i]);
        printk("%s Freeing at 0x%lx\n",__func__,(unsigned long) ag7240_macs[i]);
    }
    return 1;
}

static void __exit
ag7240_cleanup(void)
{
    int i;

    free_irq(AR7240_MISC_IRQ_ENET_LINK, ag7240_macs[0]->mac_dev);
    for(i = 0; i < AG7240_NMACS; i++)
    {
        if (mac_has_flag(ag7240_macs[1],ETH_SWONLY_MODE) && i == 0) {
            kfree(ag7240_macs[i]);
            continue;
        }
        unregister_netdev(ag7240_macs[i]->mac_dev);
        free_netdev(ag7240_macs[i]->mac_dev);
        kfree(ag7240_macs[i]);
        printk("%s Freeing at 0x%lx\n",__func__,(unsigned long) ag7240_macs[i]);
    }
    if (mac_has_flag(ag7240_macs[0],ETH_SOFT_LED)) 
        del_timer(&PLedCtrl.led_timer);
    printk(MODULE_NAME ": cleanup done\n");
}

module_init(ag7240_init);
module_exit(ag7240_cleanup);

#if CONFIG_FLOWMAC_MODULE
/*
 * Function: ag7240_rx_pause
 *
 * Pause the RX for the duration.
 *
 * net_device: the pointer to this device
 * pause:      whether to puase or resume. If the PAUSE is specified, pause
 *             action can be supplied with a time for which time the pause
 *             need to be done. In such a case, start a timer, if the resume
 *             is not hapenning within that duration, do auto resume. If
 *             duration is specified is zero, it is indifinite pause and
 *             some one need to unpause this. 
 *             ** TODO timer based recovery for pause TODO *8
 *  duration: for what time the pause is required. 
 */
int8_t
ag7240_rx_pause(struct net_device *dev, u_int8_t pause, u_int32_t duration)
{
    if (!dev) return -EINVAL;

    if (pause) {
        /* stall the rx for this Ethernet device */
        ag7240_rx_stop((ag7240_mac_t*)dev->priv);
    } else {
        ag7240_rx_start((ag7240_mac_t*)dev->priv);
    }
    return 0;
}
/*
 * Function: ag7240_rx_rate_limit
 *
 * dev: Pointer to net_device, to access the mac structure. 
 * dir: The direction in which rate need to be limited. In most practical
 *      scenario with Ethernet, it always in rx side, i.e., ingress. There
 *      would be occassion ( could there be ? ) where egress rate can be
 *      modified, for example, the wifi driver can be chagned from auto rate
 *      control to manual rate control. Right now not defining any thing
 *      here and leaving all details in void pointer. 
 * rate: Pointer to rate structure, understood by only acting driver. Unless
 *       we define a common rate codes, it is not going to be of much use.
 */
int8_t
ag7240_rx_rate_limit(struct net_device *dev, flow_dir_t dir, void *rate)
{
    if (!dev) return 1;
    if (dir != DIR_INGRESS) return 1;
    /* add any necessary rate limiting code here */
    return 0;
}
/*
 * act any of the notifications from the flowmac driver 
 * No specific events are coded as on now
 */
int
ag7240_notify_event (struct net_device *dev, flowmac_event_t event,
        void *event_data)
{
    return 0;
}

flow_ops_t ag7240_flow_ops = { ag7240_rx_pause, ag7240_rx_rate_limit,
    ag7240_notify_event };
 
int
ag7240_flowmac_register(struct net_device *dev)
{
    ag7240_mac_t      *mac;
    
    if (!dev) return -EINVAL;
    mac = (ag7240_mac_t*)dev->priv;

    mac->flow_mac_handle = flowmac_register(dev, &ag7240_flow_ops, 1, 0, 1, 0);
    if (!mac->flow_mac_handle) {
        return -EINVAL;
    }
    printk("%s registeration good handle %p \n", __func__, mac->flow_mac_handle);
    return 0;
}

/* unregister with flowmac module
 */
int
ag7240_flowmac_un_register(struct net_device *dev)
{
    ag7240_mac_t *mac;

    if (!dev) return -EINVAL;

    mac = (ag7240_mac_t*)dev->priv;
    if (mac->flow_mac_handle) {
        flowmac_unregister(mac->flow_mac_handle);
        /* memory would be freed by the other driver */
        printk("%s unregistered with flowmac %p\n", __func__, mac->flow_mac_handle);
        mac->flow_mac_handle = NULL;
    }
    return 0;
}
#endif
