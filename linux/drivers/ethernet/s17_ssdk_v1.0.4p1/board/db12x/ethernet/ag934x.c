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

#include "athrs_mac.h"

#define MODULE_NAME "AG934X_MAC"

extern void athr_gmac_fast_reset(athr_gmac_t *mac,athr_gmac_desc_t *ds,int ac);

static int
ar934x_set_gmac_caps(void *arg)
{
   athr_gmac_t *mac = (athr_gmac_t *)arg;

#ifdef CONFIG_ATHEROS_HEADER_EN
    if (mac->mac_unit == 1) {
        if (is_s26())
           mac_set_flag(mac,ATHR_S26_HEADER);
        if (is_s27())
           mac_set_flag(mac,ATHR_S27_HEADER);
    }

    if (is_s16())
        mac_set_flag(mac,ATHR_S16_HEADER);
#endif        

#ifdef CONFIG_ATHRS_QOS
    if (mac->mac_unit == 0)
        mac_set_flag(mac,WAN_QOS_SOFT_CLASS);

    if ((is_s27() || is_s26()) && mac->mac_unit == 1)
         mac_set_flag(mac,ATHR_SWITCH_QOS);

    if (is_s16())
         mac_set_flag(mac,ATHR_SWITCH_QOS);
#endif

#ifdef CONFIG_ATHR_VLAN_IGMP
         mac_set_flag(mac,ATHR_VLAN_IGMP);
#endif

#ifdef CONFIG_CHECK_DMA_STATUS
    mac_set_flag(mac,CHECK_DMA_STATUS);
#endif


#ifdef CONFIG_GMAC0_RXFCTL
    if (mac->mac_unit == 0)
        mac_set_flag(mac,ATHR_RX_FLCTL);
#endif

#ifdef CONFIG_GMAC0_TXFCTL
    if (mac->mac_unit == 0)
        mac_set_flag(mac,ATHR_TX_FLCTL);
#endif

#ifdef CONFIG_GMAC1_RXFCTL
    if (mac->mac_unit == 1)
        mac_set_flag(mac,ATHR_RX_FLCTL);
#endif

#ifdef CONFIG_ATHR_SWITCH_ONLY_MODE
        mac_set_flag(mac,ETH_SWONLY_MODE);
#endif

#ifdef CONFIG_ATHR_SUPPORT_DUAL_PHY
	mac_set_flag(mac,ATHR_DUAL_PHY);
#endif

#ifdef CONFIG_GMAC1_TXFCTL
    if (mac->mac_unit == 1)
        mac_set_flag(mac,ATHR_TX_FLCTL);
#endif

#ifdef CONFIG_ATHR_RX_TASK
    mac_set_flag(mac,ATHR_RX_TASK);
#else
    mac_set_flag(mac,ATHR_RX_POLL);
#endif
    mac_set_flag(mac,ETH_LINK_INTERRUPT);

    if (mac->mac_unit == 1) {
       mac_set_flag(mac,ETH_FORCE_SPEED);
       mac->forced_speed = ATHR_PHY_SPEED_1000T;
    }
    if (mac->mac_unit == 0 && (is_f1e() ||  is_emu() 
        || is_s16() || is_vir_phy()) )  {
        mac_set_flag(mac,ETH_LINK_POLL);
        mac_clear_flag(mac,ETH_LINK_INTERRUPT);
    }
    if (mac_has_flag(mac,ATHR_S26_HEADER)  ||
         mac_has_flag(mac,ATHR_S27_HEADER) ||
         mac_has_flag(mac,ATHR_S16_HEADER)) {

         mac_set_flag(mac,ATHR_HEADER_ENABLED);

    }
    return 0;
}

#ifdef CONFIG_CHECK_DMA_STATUS
static int 
check_dma_status_pause(athr_gmac_t *mac) { 

    /*
     * No hang signatures found in this chip.
     */
       return 1;
}

static int 
check_for_dma_status(void *arg,int ac) {

    athr_gmac_t *mac         = (athr_gmac_t *)arg;
    athr_gmac_ring_t   *r    = &mac->mac_txring[ac];
    int                head  = r->ring_head, tail = r->ring_tail;
    athr_gmac_desc_t   *ds;
    athr_gmac_buffer_t *bp;

    /* If Tx hang is asserted reset the MAC and restore the descriptors
     * and interrupt state.
     */
    while (tail != head)
    {
        ds   = &r->ring_desc[tail];
        bp   =  &r->ring_buffer[tail];

        if(athr_gmac_tx_owned_by_dma(ds)) {
            if ((athr_gmac_get_diff(bp->trans_start,jiffies)) > ((1 * HZ/10))) {

                 /*
                  * If the DMA is in pause state reset kernel watchdog timer
                  */
                if(check_dma_status_pause(mac)) {
                    mac->mac_dev->trans_start = jiffies;
                    return 0;
                }
 
                printk(MODULE_NAME ": Tx Dma status eth%d : %s\n",mac->mac_unit,
                            athr_gmac_tx_stopped(mac) ? "inactive" : "active");

                athr_gmac_fast_reset(mac,ds,ac);

                break;
            }
        }
        athr_gmac_ring_incr(tail);
    }
    return 0;
}
#endif

int ar934x_gmac_attach(void *arg )
{
    athr_gmac_t *mac = (athr_gmac_t *)arg;
    athr_gmac_ops_t *ops = mac->ops;

    if (!ops) {
       ops = (athr_gmac_ops_t *)kmalloc(sizeof(athr_gmac_ops_t), GFP_KERNEL);
    }
    
    memset(ops,0,sizeof(athr_gmac_ops_t));

#ifdef CONFIG_CHECK_DMA_STATUS
    ops->check_dma_st = check_for_dma_status;
#endif

    ops->soft_led  = NULL;
    ops->set_pll   = NULL;

    ops->set_caps = ar934x_set_gmac_caps;
    mac->ops = ops;

    return 0;
}
