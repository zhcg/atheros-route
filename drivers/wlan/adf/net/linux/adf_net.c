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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/slab.h>

#if LINUX_VERSION_CODE  <= KERNEL_VERSION(2,6,19)
#include <linux/vmalloc.h>
#endif

#include <asm/system.h>
#include <linux/netdevice.h>
#include <linux/stddef.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#ifdef LIMIT_MTU_SIZE
#include <net/icmp.h>
#include <linux/icmp.h>
#include <net/route.h>
#endif 
#include <asm/checksum.h>
#include <linux/if_vlan.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <net/sch_generic.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <net/iw_handler.h>
#include <adf_net.h>
//#include <asf_queue.h>


/**
 * @brief define the ioctl number
 */
#define SIOCGADFWIOCTL  SIOCDEVPRIVATE+1
#define SIOCSADFWIOCTL  SIOCDEVPRIVATE+2

#ifdef LIMIT_MTU_SIZE
#define LIMITED_MTU 1300
static struct net_device __fake_net_device = {
    .hard_header_len    = ETH_HLEN
};

static struct rtable __fake_rtable = {
    .u = {
        .dst = {
            .__refcnt       = ATOMIC_INIT(1),
            .dev            = &__fake_net_device,
            .path           = &__fake_rtable.u.dst,
            .metrics        = {[RTAX_MTU - 1] = 1500},
        }
    },
    .rt_flags   = 0,
};
#endif 

/**
 * List of drivers getting registered
 */

/*******************Exported Symbols for other modules***************/

EXPORT_SYMBOL(__adf_net_dev_create);
EXPORT_SYMBOL(__adf_net_dev_delete);
EXPORT_SYMBOL(__adf_net_indicate_packet);
EXPORT_SYMBOL(__adf_net_dev_tx);
EXPORT_SYMBOL(__adf_net_dev_open);
EXPORT_SYMBOL(__adf_net_dev_close);
EXPORT_SYMBOL(__adf_net_ifname);
EXPORT_SYMBOL(__adf_net_fw_mgmt_to_app);
EXPORT_SYMBOL(__adf_net_send_wireless_event);
EXPORT_SYMBOL(__adf_net_indicate_vlanpkt);
EXPORT_SYMBOL(__adf_net_poll_schedule);
EXPORT_SYMBOL(__adf_net_poll_schedule_cpu);
EXPORT_SYMBOL(__adf_net_get_vlan_tag);
EXPORT_SYMBOL(__adf_net_register_drv);
EXPORT_SYMBOL(__adf_net_unregister_drv);

/***************************Internal Functions********************************/


/**
 * @brief open the device
 * 
 * @param netdev
 * 
 * @return int
 */
static int
__adf_net_open(struct net_device *netdev)
{
    a_status_t status;
    
    if((status = adf_drv_open(netdev)))
        return __a_status_to_os(status);

    netdev->flags |= IFF_RUNNING;
    return 0;
}


/**
 * @brief
 * @param netdev
 * 
 * @return int
 */
static int
__adf_net_stop(struct net_device *netdev)
{
    adf_drv_close(netdev);

    netdev->flags &= ~IFF_RUNNING;
    //netif_carrier_off(netdev);
    netif_stop_queue(netdev);
    return 0;
}

static int
__adf_net_start_tx(struct sk_buff  *skb, struct net_device  *netdev)
{
    __adf_softc_t  *sc = netdev_to_softc(netdev);
    int err = 0;
    adf_net_vlanhdr_t   hdr = {0};

    if(unlikely((skb->len <= ETH_HLEN))) {
        printk("ADF_NET:Bad skb len %d\n", skb->len);
        dev_kfree_skb_any(skb);
        err = NETDEV_TX_OK;
        goto bail_tx;
    }

    if(likely(sc->vid && __adf_net_get_vlan_tag(skb, &hdr) == A_STATUS_OK ))
        if(unlikely(!sc->vlgrp || ((sc->vid & VLAN_VID_MASK) != hdr.vid)))
            err = A_STATUS_FAILED;

    //    adf_os_print("TX %#x \n", skb);
    if(!err)
	    err = adf_drv_tx(netdev, skb);

    switch (err) {
    case A_STATUS_OK:
        netdev->trans_start = jiffies;
        break;
    case A_STATUS_EBUSY:
        err = NETDEV_TX_BUSY;
        break;
    default:
        dev_kfree_skb_any(skb);
        err = NETDEV_TX_OK;
        break;
    }

bail_tx:
    return err;
}

/**
 * @brief add the VLAN ID
 * 
 * @param dev
 * @param vid
 */
static void
__adf_net_vlan_add(struct net_device  *dev, unsigned short vid)
{
    adf_net_cmd_data_t   data ={{0}};
    __adf_softc_t   *sc = netdev_to_softc(dev);

    if(!sc->vlgrp)
        return;

    sc->vid = vid & VLAN_VID_MASK;	
    //data.vid.val  = vid & VLAN_VID_MASK;
    if(adf_drv_cmd(dev,ADF_NET_CMD_ADD_VID, &data))
        adf_os_print("ADF_NET: vid addition failed\n");
}
/**
 * @brief delete the VLAN ID
 * 
 * @param dev
 * @param vid
 */
static void
__adf_net_vlan_del(struct net_device  *dev, unsigned short vid)
{
    adf_net_cmd_data_t   data = {{0}};
    __adf_softc_t        *sc = netdev_to_softc(dev);

    if(!sc->vlgrp)
        return;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,20)
    sc->vlgrp->vlan_devices[vid] = NULL;
#else
    vlan_group_set_device(sc->vlgrp, vid, NULL);
#endif
    sc->vid = 0;
    /*Flush the vid from the driver*/
    if(adf_drv_cmd(dev,ADF_NET_CMD_DEL_VID, &data))
        adf_os_print("ADF_NET: vid addition failed\n");
}
/**
 * @brief retrieve the vlan tag from skb
 * 
 * @param skb
 * @param tag
 * 
 * @return a_status_t (ENOTSUPP for tag not present)
 */
#define VLAN_PRI_SHIFT  13
#define VLAN_PRI_MASK   7
a_status_t
__adf_net_get_vlan_tag(struct sk_buff *skb, adf_net_vlanhdr_t *hdr)
{
    uint32_t tag;

    if(!vlan_tx_tag_present(skb))
        return A_STATUS_ENOTSUPP;

    tag = vlan_tx_tag_get(skb);

    hdr->vid  = tag & VLAN_VID_MASK;
    hdr->prio = (tag >> VLAN_PRI_SHIFT) & VLAN_PRI_MASK;
    
    return A_STATUS_OK;
}


static int
__adf_net_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
    int error = EINVAL;

    switch (cmd) {
    case SIOCGADFWIOCTL:
         //error =  __adf_net_get_wioctl(netdev, ifr->ifr_data);
         break;
    case SIOCSADFWIOCTL:
         //error =  __adf_net_set_wioctl(netdev, ifr->ifr_data);
         break;
    }

    return error;
}
static void
__adf_net_vlan_register(struct net_device *dev, struct vlan_group *grp)
{
    __adf_softc_t  *sc = netdev_to_softc(dev);

    sc->vlgrp  = grp;

}

static void
__adf_net_set_multi(struct net_device *netdev)
{
#if 0    
    adf_net_cmd_data_t data = {{0}};
    /* Change to avoid by pass of Network Stack while doing */
    if(__adf_os_warn(netdev->mc_count > ADF_NET_MAX_MCAST_ADDR))
        return;

    if(adf_drv_cmd(netdev, ADF_NET_CMD_SET_MCAST, &data))
        printk("ADF_NET: Failed to setup MCAST\n");
#endif    
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
static const struct net_device_ops __adf_net_ops = {
    .ndo_open                = __adf_net_open,
    .ndo_stop                = __adf_net_stop,
    .ndo_start_xmit          = __adf_net_start_tx,
    .ndo_do_ioctl            = __adf_net_ioctl,
    .ndo_vlan_rx_register    = __adf_net_vlan_register,
    .ndo_vlan_rx_add_vid     = __adf_net_vlan_add,
    .ndo_vlan_rx_kill_vid    = __adf_net_vlan_del,
    .ndo_set_multicast_list  = __adf_net_set_multi,
};
#endif
/**
 * @brief netdevice specific initializers
 * 
 * @param info
 * 
 * @return __adf_softc_t*
 */
static __adf_softc_t *
__adf_net_dev_init(adf_net_dev_info_t *info)
{
    struct net_device   *netdev = NULL;
    int                 error   = 0;
    __adf_softc_t       *sc;

    /**
     * strlen cannot be greater (ADF_NET_IF_NAME_SIZE - 3)
     */
    adf_os_assert(strlen(info->if_name) <= (ADF_NET_IF_NAME_SIZE - 3));
    strcat(info->if_name, "%d");

    netdev = alloc_netdev(sizeof(struct __adf_softc), info->if_name,
                          ether_setup);

    sc                          = netdev_to_softc(netdev);
    sc->netdev                  = netdev;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
    netdev->netdev_ops          = &__adf_net_ops;
#else
    netdev->open                = __adf_net_open;
    netdev->stop                = __adf_net_stop;
    netdev->hard_start_xmit     = __adf_net_start_tx;
    netdev->do_ioctl            = __adf_net_ioctl;
    netdev->vlan_rx_register    = __adf_net_vlan_register;
    netdev->vlan_rx_add_vid     = __adf_net_vlan_add;
    netdev->vlan_rx_kill_vid    = __adf_net_vlan_del;
    netdev->set_multicast_list  = __adf_net_set_multi;
#endif

    netdev->watchdog_timeo      = ADF_DEF_TX_TIMEOUT * HZ;
    netdev->features            = 0;

    netdev->features            |= (NETIF_F_HW_VLAN_TX |
                                    NETIF_F_HW_VLAN_RX |
                                    NETIF_F_HW_VLAN_FILTER);


    if(!is_valid_ether_addr(info->dev_addr)){
        printk("ADF_NET:invalid MAC address\n");
        error = EINVAL;
    }

    memcpy(netdev->dev_addr, info->dev_addr, ADF_NET_MAC_ADDR_MAX_LEN);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12)
    memcpy(netdev->perm_addr, info->dev_addr, ADF_NET_MAC_ADDR_MAX_LEN);
#endif

    /**
     * make sure nothing's on before open
     */
    netif_stop_queue(netdev);
        
    if((error = register_netdev(netdev)))
        printk("ADF_NET:Netdevice registeration failed\n");

    return sc;
}


/********************************EXPORTED***********************/




/**
 * @brief Create a real netwroking device
 * 
 * @param hdl
 * @param op
 * @param info
 * 
 * @return adf_net_handle_t
 */
adf_net_handle_t
__adf_net_dev_create(adf_drv_handle_t hdl, adf_dev_sw_t *op,
                     adf_net_dev_info_t *info)
{
    __adf_softc_t     *sc = NULL;

    if((sc = __adf_net_dev_init(info)) == NULL)
        return NULL;

    sc->sw       = *op;
    sc->drv_hdl  = hdl;
    sc->vlgrp    = NULL; /*Not part of any VLAN*/
    sc->vid      = 0;	
    
    return (adf_net_handle_t)sc;
}
  
/**
 * @brief remove the device
 * @param hdl
 */
a_status_t
__adf_net_dev_delete(adf_net_handle_t hdl)
{    
    __adf_softc_t *sc = hdl_to_softc(hdl);

    unregister_netdev(sc->netdev);
    return A_STATUS_OK;
}


/**
 * @brief this adds a IP ckecksum in the IP header of the packet
 * @param skb
 */
static void
__adf_net_ip_cksum(struct sk_buff *skb)
{
    struct iphdr   *ih  = {0};
    struct skb_shared_info  *sh = skb_shinfo(skb);

    adf_os_assert(sh->nr_frags == 0);

    ih = (struct iphdr *)(skb->data + sizeof(struct ethhdr));
    ih->check = 0;
    ih->check = ip_fast_csum((unsigned char *)ih, ih->ihl);
}

a_status_t
__adf_net_indicate_packet(adf_net_handle_t hdl, struct sk_buff *skb,
                          uint32_t len)
{
    struct net_device *netdev = hdl_to_netdev(hdl);

    /**
     * For pseudo devices IP checksum has to computed
     */
    if(adf_os_unlikely(skb->ip_summed == CHECKSUM_UNNECESSARY))
        __adf_net_ip_cksum(skb);

    /**
     * also pulls the ether header
     */
    skb->protocol           =   eth_type_trans(skb, netdev);
    skb->dev                =   netdev;
    netdev->last_rx         =   jiffies;

#ifdef LIMIT_MTU_SIZE
    if (skb->len > LIMITED_MTU) {
        skb->h.raw = skb->nh.raw = skb->data;

        skb->dst = (struct dst_entry *)&__fake_rtable;
        skb->pkt_type = PACKET_HOST;
        dst_hold(skb->dst);

#if 0
        printk("addrs : sa : %x : da:%x\n", skb->nh.iph->saddr, skb->nh.iph->daddr);
        printk("head : %p tail : %p iph %p %p\n", skb->head, skb->tail,
                skb->nh.iph, skb->mac.raw);
#endif 

        icmp_send(skb, ICMP_DEST_UNREACH, ICMP_FRAG_NEEDED, htonl(LIMITED_MTU));
        dev_kfree_skb_any(skb);
        return A_STATUS_OK;
    }
#endif 

    if (in_irq()) 
        netif_rx(skb);
    else
        netif_receive_skb(skb);

    return A_STATUS_OK;
}

a_status_t
__adf_net_dev_tx(adf_net_handle_t hdl, struct sk_buff  *skb)
{
    struct net_device   *netdev = hdl_to_netdev(hdl);
    a_status_t      retval = A_STATUS_OK;

    if(unlikely(!netdev)){
        printk("ADF_NET:netdev not found\n");
        adf_os_assert(0);
    }

    skb->dev = netdev ;

#ifdef LIMIT_MTU_SIZE
    if (skb->len > (LIMITED_MTU + ETH_HLEN)) {
        skb->h.raw = skb->nh.raw = skb->data + ETH_HLEN;

        skb->dst = (struct dst_entry *)&__fake_rtable;
        skb->pkt_type = PACKET_HOST;
        dst_hold(skb->dst);

#if 0
        printk("tx addrs : sa : %x : da:%x\n", skb->nh.iph->saddr, skb->nh.iph->daddr);
        printk("tx head : %p tail : %p iph %p %p\n", skb->head, skb->tail,
                skb->nh.iph, skb->mac.raw);
#endif 
        icmp_send(skb, ICMP_DEST_UNREACH, ICMP_FRAG_NEEDED, htonl(LIMITED_MTU));
        dev_kfree_skb_any(skb);
        return A_STATUS_OK;
    }
#undef LIMITED_MTU
#endif 
    dev_queue_xmit(skb);

    return retval;
}

/**
 * @brief call the init of the hdl passed on
 * 
 * @param hdl
 * 
 * @return a_uint32_t
 */
a_status_t
__adf_net_dev_open(adf_net_handle_t hdl)
{
    dev_open(hdl_to_netdev(hdl));
    return A_STATUS_OK;
}
a_status_t
__adf_net_dev_close(adf_net_handle_t hdl)
{
    dev_close(hdl_to_netdev(hdl));
    
    return A_STATUS_OK;
}

const uint8_t *
__adf_net_ifname(adf_net_handle_t  hdl)
{
    struct net_device *netdev = hdl_to_netdev(hdl);

    return (netdev->name);
}

a_status_t             
__adf_net_fw_mgmt_to_app(adf_net_handle_t hdl, struct sk_buff *skb, 
                         uint32_t len)
{
    struct net_device *netdev = hdl_to_netdev(hdl);
    skb->dev = netdev;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,20)
    skb->mac.raw = skb->data;
#else
    skb_reset_mac_header(skb);
#endif
    skb->ip_summed = CHECKSUM_NONE;
    skb->pkt_type = PACKET_OTHERHOST;
    skb->protocol = __constant_htons(0x0019);  /* ETH_P_80211_RAW */
         
    netif_rx(skb);

    return A_STATUS_OK;
}

a_status_t
__adf_net_send_wireless_event(adf_net_handle_t hdl, adf_net_wireless_event_t what, 
                              void *data, size_t data_len)
{
    wireless_send_event(hdl_to_netdev(hdl),
                        what,
                        (union iwreq_data *)data,
                        NULL);
    return A_STATUS_OK;
}

a_status_t 
__adf_net_indicate_vlanpkt(adf_net_handle_t hdl, struct sk_buff *skb, 
                           uint32_t len, adf_net_vid_t *vid)
{
    __adf_softc_t   *sc = hdl_to_softc(hdl);
    struct net_device *netdev = hdl_to_netdev(hdl);

    skb->protocol           =   eth_type_trans(skb, netdev);
    skb->dev                =   netdev;
    netdev->last_rx         =   jiffies;
    if(sc->vlgrp) {
        vlan_hwaccel_receive_skb(skb,sc->vlgrp, vid->val);
    } else {
        (in_irq() ? netif_rx(skb) : netif_receive_skb(skb));
    }

    return A_STATUS_OK;
}

void 
__adf_net_poll_schedule(adf_net_handle_t hdl)
{
    return;
}

a_status_t 
__adf_net_poll_schedule_cpu(adf_net_handle_t hdl, uint32_t cpu_msk, void *arg)
{
    return A_STATUS_OK;
}

a_status_t
__adf_net_register_drv(adf_drv_info_t *drv)
{
    return A_STATUS_OK;
}

void
__adf_net_unregister_drv(a_uint8_t *drv_name)
{
    return;
}

MODULE_LICENSE("Proprietary");
