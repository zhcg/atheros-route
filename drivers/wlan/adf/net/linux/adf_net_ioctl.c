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
#include <linux/sockios.h>
#include <linux/if.h>
#include <linux/netdevice.h>

#if LINUX_VERSION_CODE  <= KERNEL_VERSION(2,6,19)
#include <linux/vmalloc.h>
#endif

#include <adf_net_wcmd.h>
#include <adf_net.h>
//#include <pktlog.h>

int
__adf_net_set_wioctl(struct net_device *netdev, adf_net_wcmd_t   *wcmd)
{
    a_status_t status = 0;
    adf_net_wcmd_t  data = {{0}};

    if(copy_from_user(&data, wcmd, sizeof(struct adf_net_wcmd)))
        return EINVAL;
    
    switch (data.type) {
    case ADF_NET_WCMD_SET_DEV_VAP_CREATE:
        /**
         * The RTNL locks already held & we use a common register netdev for
         * creating the device hence we have unlock momentarily 
         */
        rtnl_unlock();

        status = adf_drv_wcmd(netdev, data.type, &data.data);

        rtnl_lock();

        break;
    case ADF_NET_WCMD_SET_VAPDELETE:
        /**
         * The RTNL locks already held & we use a common register netdev for
         * creating the device hence we have unlock momentarily 
         */
        rtnl_unlock();

        status = adf_drv_wcmd(netdev, data.type, &data.data);

        rtnl_lock();

        break;
    default:
        
        status = adf_drv_wcmd(netdev, data.type, &data.data);

        break;

    }

    return __a_status_to_os(status);
}

int
__adf_net_get_wioctl(struct net_device  *netdev, adf_net_wcmd_t   *wcmd)
{
    int error = 0;
    adf_net_wcmd_t  data = {{0}};
    void    *uptr = NULL;/*User space pointer*/
    void    **kptr = NULL;
    size_t   ubytes = 0;
    
    if(copy_from_user(&data, wcmd, sizeof(struct adf_net_wcmd)))
        return EINVAL;

    switch (data.type){
    case ADF_NET_WCMD_GET_SCAN:
        ubytes = sizeof(struct adf_net_wcmd_scan);
        uptr   = data.d_scan;
        break;

    case ADF_NET_WCMD_GET_STA_STATS:
        ubytes = sizeof(struct adf_net_wcmd_stastats);
        uptr   = data.d_stastats;
      break;

    case ADF_NET_WCMD_GET_VAP_STATS:
        ubytes = sizeof(struct adf_net_wcmd_vapstats);
        uptr   = data.d_vapstats;
        break;

    case ADF_NET_WCMD_GET_RANGE:
        ubytes = sizeof(struct adf_net_wcmd_vapparam_range);
        uptr   = data.d_range;
        break;

    case ADF_NET_WCMD_GET_DEV_STATUS:
        ubytes = sizeof(struct adf_net_wcmd_phystats);
        uptr   = data.d_phystats;
        break;

    case ADF_NET_WCMD_GET_DEVSTATS:
        ubytes = sizeof(struct adf_net_wcmd_devstats);
        uptr   = data.d_devstats;
        break;

    case ADF_NET_WCMD_GET_STATION_LIST:
        ubytes = sizeof(struct adf_net_wcmd_stainfo);
        uptr   = data.d_station;
        break;

    case ADF_NET_WCMD_GET_DEV_HST_STATS:
    case ADF_NET_WCMD_GET_DEV_HST_11N_STATS:
        ubytes = sizeof(struct adf_net_wcmd_hst_phystats);
        uptr   = data.d_hststats;
        break;
        
    case ADF_NET_WCMD_GET_DEV_TGT_STATS:
    case ADF_NET_WCMD_GET_DEV_TGT_11N_STATS:
        ubytes = sizeof(struct adf_net_wcmd_tgt_phystats);
        uptr   = data.d_tgtstats;
        break;

    case ADF_NET_WCMD_PKTLOG_GET_DATA:
	//ath_pktlog_ioctl_get_data();
	break;
        
    default:
        ubytes = 0;
    }

    if(ubytes){
        kptr  = (void *)&data.data;
        *kptr = vmalloc(ubytes);
        error = (*kptr == NULL);
    }
    
    if(!error && ubytes)
        error = copy_from_user(*kptr, uptr, ubytes);
    
    if(!error)
        error = adf_drv_wcmd(netdev, data.type, &data.data);
    
    
    if(!error && ubytes)
        error = copy_to_user(uptr, *kptr, ubytes);

    if(kptr){
        vfree (*kptr);
        *kptr = uptr; /*Rollback the user space dude*/
    }
 
    if(!error)
        error = copy_to_user(wcmd, &data, sizeof(struct adf_net_wcmd));

    return (error ? EINVAL : 0);
        
}
