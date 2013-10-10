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
 * Wireless extensions support for 802.11 common code.
 */
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
#include <linux/config.h>
#endif
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/utsname.h>
#include <linux/if_arp.h>       /* XXX for ARPHRD_ETHER */
#include <net/iw_handler.h>

#include <asm/uaccess.h>

#include "if_media.h"
#include "_ieee80211.h"
#include <osif_private.h>
#include <wlan_opts.h>
#include <ieee80211_var.h>
#include "ieee80211_rateset.h"
#include "ieee80211_vi_dbg.h"
#if ATH_SUPPORT_IBSS_DFS
#include <ieee80211_regdmn.h>
#endif
#include "ieee80211_power_priv.h"

#define NETDEV_TO_VAP(_dev) (((osif_dev *)netdev_priv(_dev))->os_if)

#define IS_UP(_dev) \
    (((_dev)->flags & (IFF_RUNNING|IFF_UP)) == (IFF_RUNNING|IFF_UP))
#define IS_UP_AUTO(_vap) \
    (IS_UP((_vap)->iv_dev) && \
    (_vap)->iv_ic->ic_roaming == IEEE80211_ROAMING_AUTO)
#define RESCAN  1

#define IS_CONNECTED(osp) ((((osp)->os_opmode==IEEE80211_M_STA) || \
                        ((u_int8_t)(osp)->os_opmode == IEEE80211_M_P2P_CLIENT))? \
                        wlan_connection_sm_is_connected((osp)->sm_handle): \
                        (osp)->is_bss_started )

#define IEEE80211_MSG_IOCTL   IEEE80211_MSG_DEBUG

extern unsigned long ath_ioctl_debug;       /* defined in ah_osdep.c  */

#define IOCTL_DPRINTF(_fmt, ...) do {   \
    if ((ath_ioctl_debug))          \
        printk(_fmt, __VA_ARGS__);  \
    } while (0)

#ifdef ATH_SUPPORT_HTC
static int ieee80211_ioctl_wrapper(struct net_device *dev,
                                   struct iw_request_info *info,
                                   union iwreq_data *wrqu, char *extra);
#endif
static char *find_ieee_priv_ioctl_name(int param, int set_flag);  /* for debugging use */
static void debug_print_ioctl(char *dev_name, int ioctl, char *ioctl_name) ;  /* for debugging */

#ifdef __CARRIER_PLATFORM__
extern int wlan_pltfrm_set_param(wlan_if_t vaphandle, u_int32_t val);
extern int wlan_pltfrm_get_param(wlan_if_t vaphandle);
#endif

#define IEEE80211_BINTVAL_IWMAX       3500   /* max beacon interval */
#define IEEE80211_BINTVAL_IWMIN       40     /* min beacon interval */

static const u_int mopts[] = {
        IFM_AUTO,
        IFM_IEEE80211_11A,
        IFM_IEEE80211_11B,
        IFM_IEEE80211_11G,
        IFM_IEEE80211_FH,
        IFM_IEEE80211_11A | IFM_IEEE80211_TURBO,
        IFM_IEEE80211_11G | IFM_IEEE80211_TURBO,
        IFM_IEEE80211_11NA,
        IFM_IEEE80211_11NG,
};

#if UMAC_SUPPORT_INTERNALANTENNA            

/* set default rate for all rate sets */
void smartantenna_setdefault_antenna(struct ieee80211_node *ni, u_int8_t antenna)
{
#define MAX_RATESETS 3
    int i = 0;
    struct ieee80211com *ic = ni->ni_ic;
    /* printk(" setting TX antenna combination to :%d [%s] \n", antenna , ant_buf[antenna]); */
    for (i = 0; i < MAX_RATESETS; i++)
    {
        ni->rtset[i].selected_antenna = antenna;
        ic->ic_set_selected_smantennas(ni, antenna, i);
    }
}

void ieee80211_set_current_antenna(struct ieee80211vap *vap, u_int32_t antenna)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node_table  *nt = &ic->ic_sta;
    struct ieee80211_node *ni;

    if (vap->iv_opmode == IEEE80211_M_HOSTAP)
    {
        TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
            
            if (IEEE80211_ADDR_EQ(ni->ni_macaddr, vap->iv_myaddr))
            {
                continue;
            }
             smartantenna_setdefault_antenna(ni, antenna);
        }
    }
    else if (vap->iv_opmode == IEEE80211_M_STA)
    {    /* in sta mode we will transmit to AP only */
         smartantenna_setdefault_antenna(vap->iv_bss, antenna);
    }
}

void ieee80211_display_current_antenna(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node_table  *nt = &ic->ic_sta;
    struct ieee80211_node *ni;

    TAILQ_FOREACH(ni, &nt->nt_node, ni_list)
    {
        if (IEEE80211_ADDR_EQ(ni->ni_macaddr, vap->iv_myaddr)) {
                continue;
        }
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY," Tx Antennas for  node: %s > %d \n"
                              ,ether_sprintf(ni->ni_macaddr), ni->rtset[0].selected_antenna);
    }
}
#endif    

static void
preempt_scan(struct net_device *dev, int max_grace, int max_wait)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    int total_delay = 0;
    int canceled = 0, ready = 0;

    while (!ready && total_delay < max_grace + max_wait) {
    if (!wlan_scan_in_progress(vap)) {
        ready = 1;
    } else {
        if (!canceled && total_delay > max_grace) {
        /*
        Cancel any existing active scan, so that any new parameters
        in this scan ioctl (or the defaults) can be honored, then
        wait around a while to see if the scan cancels properly.
        */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
                "%s: cancel pending scan request\n", __func__);
        wlan_scan_cancel(vap, osifp->scan_requestor, IEEE80211_ALL_SCANS, true);
        canceled = 1;
        }
        OS_DELAY (1000);
        total_delay += 1;
    }
    }
    if (!ready) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
                "%s: Timeout canceling current scan.\n",
                __func__);
    }
}

static const int legacy_rate_idx[][2] = {
    {1, 0x1b},
    {2, 0x1a},
    {5, 0x19},
    {6, 0xb},
    {9, 0xf},
    {11, 0x18},
    {12, 0xa},
    {18, 0xe},
    {24, 0x9},
    {36, 0xd},
    {48, 0x8},
    {54, 0xc},
};
static int
ieee80211_ioctl_siwrate(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_param *rrq, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    int retv, value;

    if (rrq->fixed) {
        unsigned int rate = rrq->value;
        if (rate >= 1000) {
            /* convert rate to index */
            int i;
            int array_size = sizeof(legacy_rate_idx)/sizeof(legacy_rate_idx[0]);
            rate /= 1000000;
            for (i = 0; i < array_size; i++) {
            /* Array Index 0 has the rate and 1 has the rate code. 
               The variable rate has the rate code which must be converted to actual rate*/

                if (rate == legacy_rate_idx[i][1]) {
                    rate = legacy_rate_idx[i][0];
                    break;
                }
            }
            if (i == array_size) return -EINVAL;
        }
        value = rate;
    } else {
        value = IEEE80211_FIXED_RATE_NONE;
    }
    retv = wlan_set_param(vap, IEEE80211_FIXED_RATE, value);
    if (EOK == retv) {
        if (value != IEEE80211_FIXED_RATE_NONE) {
            /* set default retries when setting fixed rate */
            retv = wlan_set_param(vap, IEEE80211_FIXED_RETRIES, 4);
        }
        else {
            retv = wlan_set_param(vap, IEEE80211_FIXED_RETRIES, 0);
        }
    }
    return -retv;
}

static int
ieee80211_ioctl_giwrate(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_param *rrq, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = NETDEV_TO_VAP(dev);
    struct ifmediareq imr;
    int args[2];

    args[0] = IEEE80211_FIXED_RATE;

    memset(&imr, 0, sizeof(imr));
    osifp->os_media.ifm_status(dev, &imr);

    rrq->fixed = IFM_SUBTYPE(imr.ifm_active) != IFM_AUTO;
    rrq->value = wlan_get_maxphyrate(vap);
    return 0;
}

#ifdef notyet
#ifndef ifr_media
#define ifr_media   ifr_ifru.ifru_ivalue
#endif

#ifdef ATH_SUPPORT_LINUX_STA
static int ieee80211_ht_rix2rate(struct ieee80211vap *vap, unsigned int rix)
{
    struct ieee80211com *ic = vap->iv_ic;
    enum ieee80211_phymode mode= ieee80211_chan2mode(ic->ic_curchan);
    const int ar5416_11ng_table[] =
    {
        /*                Multi    Single    Single   */
        /*                strm      strm      strm                                                       short   dot11 ctrl RssiAck  RssiAck  base cw40  sgi   ht   4ms tx  */
        /*               valid     valid      STBC                                  Kbps    uKbps   RC   Preamb  Rate  Rate ValidMin DeltaMin Idx  Idx   Idx   Idx   limit  */
    2,   /*    1 Mb  {  TRUE_ALL,  TRUE_ALL,  TRUE_ALL,      WLAN_PHY_CCK,            1000,    900,  0x1b,  0x00,    2,   0,    0,       1,     0,    0,    0,    0,      0},*/
    4,   /*    2 Mb  {  TRUE_ALL,  TRUE_ALL,  TRUE_ALL,      WLAN_PHY_CCK,            2000,   1900,  0x1a,  0x04,    4,   1,    1,       1,     1,    1,    1,    1,      0},*/
    11,  /*  5.5 Mb  {  TRUE_ALL,  TRUE_ALL,  TRUE_ALL,      WLAN_PHY_CCK,            5500,   4900,  0x19,  0x04,   11,   2,    2,       2,     2,    2,    2,    2,      0},*/
    22,  /*   11 Mb  {  TRUE_ALL,  TRUE_ALL,  TRUE_ALL,      WLAN_PHY_CCK,           11000,   8100,  0x18,  0x04,   22,   3,    3,       2,     3,    3,    3,    3,      0},*/
    12,  /*    6 Mb  {  FALSE,     FALSE,     FALSE,         WLAN_PHY_OFDM,           6000,   5400,  0x0b,  0x00,   12,   4,    2,       1,     4,    4,    4,    4,      0},*/
    18,  /*    9 Mb  {  FALSE,     FALSE,     FALSE,         WLAN_PHY_OFDM,           9000,   7800,  0x0f,  0x00,   18,   4,    3,       1,     5,    5,    5,    5,      0},*/
    24,  /*   12 Mb  {  TRUE,      TRUE,      TRUE,          WLAN_PHY_OFDM,          12000,  10100,  0x0a,  0x00,   24,   6,    4,       1,     6,    6,    6,    6,      0},*/
    36,  /*   18 Mb  {  TRUE,      TRUE,      TRUE,          WLAN_PHY_OFDM,          18000,  14100,  0x0e,  0x00,   36,   6,    6,       2,     7,    7,    7,    7,      0},*/
    48,  /*   24 Mb  {  TRUE,      TRUE,      TRUE,          WLAN_PHY_OFDM,          24000,  17700,  0x09,  0x00,   48,   8,   10,       3,     8,    8,    8,    8,      0},*/
    72,  /*   36 Mb  {  TRUE,      TRUE,      TRUE,          WLAN_PHY_OFDM,          36000,  23700,  0x0d,  0x00,   72,   8,   14,       3,     9,    9,    9,    9,      0},*/
    96,  /*   48 Mb  {  TRUE,      TRUE,      TRUE,          WLAN_PHY_OFDM,          48000,  27400,  0x08,  0x00,   96,   8,   20,       3,    10,   10,   10,   10,      0},*/
    108, /*   54 Mb  {  TRUE,      TRUE,      TRUE,          WLAN_PHY_OFDM,          54000,  30900,  0x0c,  0x00,  108,   8,   23,       3,    11,   11,   11,   11,      0},*/
    13,  /*  6.5 Mb  {  FALSE,     FALSE,     FALSE,         WLAN_PHY_HT_20_SS,       6500,   6400,  0x80,  0x00,    0,   4,    2,       3,    12,   28,   12,   28,   3216},*/
    26,  /*   13 Mb  {  TRUE_20,   TRUE_20,   TRUE_20,       WLAN_PHY_HT_20_SS,      13000,  12700,  0x81,  0x00,    1,   6,    4,       3,    13,   29,   13,   29,   6434},*/
    39,  /* 19.5 Mb  {  TRUE_20,   TRUE_20,   TRUE_20,       WLAN_PHY_HT_20_SS,      19500,  18800,  0x82,  0x00,    2,   6,    6,       3,    14,   30,   14,   30,   9650},*/
    52,  /*   26 Mb  {  TRUE_20,   TRUE_20,   TRUE_20,       WLAN_PHY_HT_20_SS,      26000,  25000,  0x83,  0x00,    3,   8,   10,       3,    15,   31,   15,   31,  12868},*/
    78,  /*   39 Mb  {  TRUE_20,   TRUE_20,   TRUE_20,       WLAN_PHY_HT_20_SS,      39000,  36700,  0x84,  0x00,    4,   8,   14,       3,    16,   32,   16,   32,  19304},*/
    104, /*   52 Mb  {  FALSE,     TRUE_20,   TRUE_20,       WLAN_PHY_HT_20_SS,      52000,  48100,  0x85,  0x00,    5,   8,   20,       3,    17,   33,   17,   33,  25740},*/
    117, /* 58.5 Mb  {  FALSE,     TRUE_20,   TRUE_20,       WLAN_PHY_HT_20_SS,      58500,  53500,  0x86,  0x00,    6,   8,   23,       3,    18,   34,   18,   34,  28956},*/
    130, /*   65 Mb  {  FALSE,     TRUE_20,   FALSE,         WLAN_PHY_HT_20_SS,      65000,  59000,  0x87,  0x00,    7,   8,   25,       3,    19,   35,   19,   36,  32180},*/
    26,  /*   13 Mb  {  FALSE,     FALSE,     FALSE,         WLAN_PHY_HT_20_DS,      13000,  12700,  0x88,  0x00,    8,   4,    2,       3,    20,   37,   20,   37,   6430},*/
    52,  /*   26 Mb  {  FALSE,     FALSE,     FALSE,         WLAN_PHY_HT_20_DS,      26000,  24800,  0x89,  0x00,    9,   6,    4,       3,    21,   38,   21,   38,  12860},*/
    78,  /*   39 Mb  {  FALSE,     FALSE,     FALSE,         WLAN_PHY_HT_20_DS,      39000,  36600,  0x8a,  0x00,   10,   6,    6,       3,    22,   39,   22,   39,  19300},*/
    104, /*   52 Mb  {  TRUE_20,   FALSE,     FALSE,         WLAN_PHY_HT_20_DS,      52000,  48100,  0x8b,  0x00,   11,   8,   10,       3,    23,   40,   23,   40,  25736},*/
    156, /*   78 Mb  {  TRUE_20,   FALSE,     TRUE_20,       WLAN_PHY_HT_20_DS,      78000,  69500,  0x8c,  0x00,   12,   8,   14,       3,    24,   41,   24,   41,  38600},*/
    208, /*  104 Mb  {  TRUE_20,   FALSE,     TRUE_20,       WLAN_PHY_HT_20_DS,     104000,  89500,  0x8d,  0x00,   13,   8,   20,       3,    25,   42,   25,   42,  51472},*/
    234, /*  117 Mb  {  TRUE_20,   FALSE,     TRUE_20,       WLAN_PHY_HT_20_DS,     117000,  98900,  0x8e,  0x00,   14,   8,   23,       3,    26,   43,   26,   44,  57890},*/
    260, /*  130 Mb  {  TRUE_20,   FALSE,     TRUE_20,       WLAN_PHY_HT_20_DS,     130000, 108300,  0x8f,  0x00,   15,   8,   25,       3,    27,   44,   27,   45,  64320},*/
    27,  /* 13.5 Mb  {  TRUE_40,   TRUE_40,   TRUE_40,       WLAN_PHY_HT_40_SS,      13500,  13200,  0x80,  0x00,    0,   8,    2,       3,    12,   28,   28,   28,   6684},*/
    54,  /* 27.0 Mb  {  TRUE_40,   TRUE_40,   TRUE_40,       WLAN_PHY_HT_40_SS,      27500,  25900,  0x81,  0x00,    1,   8,    4,       3,    13,   29,   29,   29,  13368},*/
    81,  /* 40.5 Mb  {  TRUE_40,   TRUE_40,   TRUE_40,       WLAN_PHY_HT_40_SS,      40500,  38600,  0x82,  0x00,    2,   8,    6,       3,    14,   30,   30,   30,  20052},*/
    108, /*   54 Mb  {  TRUE_40,   TRUE_40,   TRUE_40,       WLAN_PHY_HT_40_SS,      54000,  49800,  0x83,  0x00,    3,   8,   10,       3,    15,   31,   31,   31,  26738},*/
    162, /*   81 Mb  {  TRUE_40,   TRUE_40,   TRUE_40,       WLAN_PHY_HT_40_SS,      81500,  72200,  0x84,  0x00,    4,   8,   14,       3,    16,   32,   32,   32,  40104},*/
    216, /*  108 Mb  {  FALSE,     TRUE_40,   TRUE_40,       WLAN_PHY_HT_40_SS,     108000,  92900,  0x85,  0x00,    5,   8,   20,       3,    17,   33,   33,   33,  53476},*/
    243, /* 121.5Mb  {  FALSE,     TRUE_40,   TRUE_40,       WLAN_PHY_HT_40_SS,     121500, 102700,  0x86,  0x00,    6,   8,   23,       3,    18,   34,   34,   34,  60156},*/
    270, /*  135 Mb  {  FALSE,     TRUE_40,   FALSE,         WLAN_PHY_HT_40_SS,     135000, 112000,  0x87,  0x00,    7,   8,   23,       3,    19,   35,   36,   36,  66840},*/
    300, /*  150 Mb  {  FALSE,     TRUE_40,   FALSE,         WLAN_PHY_HT_40_SS_HGI, 150000, 122000,  0x87,  0x00,    7,   8,   25,       3,    19,   35,   36,   36,  74200},*/
    54,  /*   27 Mb  {  FALSE,     FALSE,     FALSE,         WLAN_PHY_HT_40_DS,      27000,  25800,  0x88,  0x00,    8,   8,    2,       3,    20,   37,   37,   37,  13360},*/
    108, /*   54 Mb  {  FALSE,     FALSE,     FALSE,         WLAN_PHY_HT_40_DS,      54000,  49800,  0x89,  0x00,    9,   8,    4,       3,    21,   38,   38,   38,  26720},*/
    162, /*   81 Mb  {  FALSE,     FALSE,     FALSE,         WLAN_PHY_HT_40_DS,      81000,  71900,  0x8a,  0x00,   10,   8,    6,       3,    22,   39,   39,   39,  40080},*/
    216, /*  108 Mb  {  TRUE_40,   FALSE,     FALSE,         WLAN_PHY_HT_40_DS,     108000,  92500,  0x8b,  0x00,   11,   8,   10,       3,    23,   40,   40,   40,  53440},*/
    324, /*  162 Mb  {  TRUE_40,   FALSE,     TRUE_40,       WLAN_PHY_HT_40_DS,     162000, 130300,  0x8c,  0x00,   12,   8,   14,       3,    24,   41,   41,   41,  80160},*/
    432, /*  216 Mb  {  TRUE_40,   FALSE,     TRUE_40,       WLAN_PHY_HT_40_DS,     216000, 162800,  0x8d,  0x00,   13,   8,   20,       3,    25,   42,   42,   42, 106880},*/
    486, /*  243 Mb  {  TRUE_40,   FALSE,     TRUE_40,       WLAN_PHY_HT_40_DS,     243000, 178200,  0x8e,  0x00,   14,   8,   23,       3,    26,   43,   43,   43, 120240},*/
    540, /*  270 Mb  {  TRUE_40,   FALSE,     TRUE_40,       WLAN_PHY_HT_40_DS,     270000, 192100,  0x8f,  0x00,   15,   8,   23,       3,    27,   44,   45,   45, 133600},*/
    600, /*  300 Mb  {  TRUE_40,   FALSE,     TRUE_40,       WLAN_PHY_HT_40_DS_HGI, 300000, 207000,  0x8f,  0x00,   15,   8,   25,       3,    27,   44,   45,   45, 148400},*/
    };

    const int ar5416_11na_table[] =
    {
        /*              Multi     Single   Single    */
        /*               strm       strm     strm                                                  rate  short   dot11 ctrl RssiAck  RssiAck  base  cw40  sgi    ht  4ms tx */
        /*              valid      valid     STBC                                   Kbps   uKbps   Code  Preamb  Rate  Rate ValidMin DeltaMin Idx   Idx   Idx   Idx   limit */
    12,  /*    6 Mb  {  TRUE,      TRUE,    TRUE,         WLAN_PHY_OFDM,           6000,   5400,  0x0b,  0x00,   12,   0,    2,       1,     0,    0,    0,    0,      0},*/
    18,  /*    9 Mb  {  TRUE,      TRUE,    TRUE,         WLAN_PHY_OFDM,           9000,   7800,  0x0f,  0x00,   18,   0,    3,       1,     1,    1,    1,    1,      0},*/
    24,  /*   12 Mb  {  TRUE,      TRUE,    TRUE,         WLAN_PHY_OFDM,          12000,  10000,  0x0a,  0x00,   24,   2,    4,       2,     2,    2,    2,    2,      0},*/
    36,  /*   18 Mb  {  TRUE,      TRUE,    TRUE,         WLAN_PHY_OFDM,          18000,  13900,  0x0e,  0x00,   36,   2,    6,       2,     3,    3,    3,    3,      0},*/
    48,  /*   24 Mb  {  TRUE,      TRUE,    TRUE,         WLAN_PHY_OFDM,          24000,  17300,  0x09,  0x00,   48,   4,   10,       3,     4,    4,    4,    4,      0},*/
    72,  /*   36 Mb  {  TRUE,      TRUE,    TRUE,         WLAN_PHY_OFDM,          36000,  23000,  0x0d,  0x00,   72,   4,   14,       3,     5,    5,    5,    5,      0},*/
    96,  /*   48 Mb  {  TRUE,      TRUE,    TRUE,         WLAN_PHY_OFDM,          48000,  27400,  0x08,  0x00,   96,   4,   20,       3,     6,    6,    6,    6,      0},*/
    108, /*   54 Mb  {  TRUE,      TRUE,    TRUE,         WLAN_PHY_OFDM,          54000,  29300,  0x0c,  0x00,  108,   4,   23,       3,     7,    7,    7,    7,      0},*/
    13,  /*  6.5 Mb  {  TRUE_20,   TRUE_20, TRUE_20,      WLAN_PHY_HT_20_SS,       6500,   6400,  0x80,  0x00,    0,   0,    2,       3,     8,   24,    8,   24,   3216},*/
    26,  /*   13 Mb  {  TRUE_20,   TRUE_20, TRUE_20,      WLAN_PHY_HT_20_SS,      13000,  12700,  0x81,  0x00,    1,   2,    4,       3,     9,   25,    9,   25,   6434},*/
    39,  /* 19.5 Mb  {  TRUE_20,   TRUE_20, TRUE_20,      WLAN_PHY_HT_20_SS,      19500,  18800,  0x82,  0x00,    2,   2,    6,       3,    10,   26,   10,   26,   9650},*/
    52,  /*   26 Mb  {  TRUE_20,   TRUE_20, TRUE_20,      WLAN_PHY_HT_20_SS,      26000,  25000,  0x83,  0x00,    3,   4,   10,       3,    11,   27,   11,   27,  12868},*/
    78,  /*   39 Mb  {  TRUE_20,   TRUE_20, TRUE_20,      WLAN_PHY_HT_20_SS,      39000,  36700,  0x84,  0x00,    4,   4,   14,       3,    12,   28,   12,   28,  19304},*/
    104, /*   52 Mb  {  FALSE,     TRUE_20, TRUE_20,      WLAN_PHY_HT_20_SS,      52000,  48100,  0x85,  0x00,    5,   4,   20,       3,    13,   29,   13,   29,  25740},*/
    117, /* 58.5 Mb  {  FALSE,     TRUE_20, TRUE_20,      WLAN_PHY_HT_20_SS,      58500,  53500,  0x86,  0x00,    6,   4,   23,       3,    14,   30,   14,   30,  28956},*/
    130, /*   65 Mb  {  FALSE,     TRUE_20, FALSE,        WLAN_PHY_HT_20_SS,      65000,  59000,  0x87,  0x00,    7,   4,   25,       3,    15,   31,   15,   32,  32180},*/
    26,  /*   13 Mb  {  FALSE,     FALSE,   FALSE,        WLAN_PHY_HT_20_DS,      13000,  12700,  0x88,  0x00,    8,   0,    2,       3,    16,   33,   16,   33,   6430},*/
    52,  /*   26 Mb  {  FALSE,     FALSE,   FALSE,        WLAN_PHY_HT_20_DS,      26000,  24800,  0x89,  0x00,    9,   2,    4,       3,    17,   34,   17,   34,  12860},*/
    78,  /*   39 Mb  {  FALSE,     FALSE,   FALSE,        WLAN_PHY_HT_20_DS,      39000,  36600,  0x8a,  0x00,   10,   2,    6,       3,    18,   35,   18,   35,  19300},*/
    104, /*   52 Mb  {  TRUE_20,   FALSE,   FALSE,        WLAN_PHY_HT_20_DS,      52000,  48100,  0x8b,  0x00,   11,   4,   10,       3,    19,   36,   19,   36,  25736},*/
    156, /*   78 Mb  {  TRUE_20,   FALSE,   TRUE_20,      WLAN_PHY_HT_20_DS,      78000,  69500,  0x8c,  0x00,   12,   4,   14,       3,    20,   37,   20,   37,  38600},*/
    208, /*  104 Mb  {  TRUE_20,   FALSE,   TRUE_20,      WLAN_PHY_HT_20_DS,     104000,  89500,  0x8d,  0x00,   13,   4,   20,       3,    21,   38,   21,   38,  51472},*/
    234, /*  117 Mb  {  TRUE_20,   FALSE,   TRUE_20,      WLAN_PHY_HT_20_DS,     117000,  98900,  0x8e,  0x00,   14,   4,   23,       3,    22,   39,   22,   39,  57890},*/
    260, /*  130 Mb  {  TRUE_20,   FALSE,   TRUE_20,      WLAN_PHY_HT_20_DS,     130000, 108300,  0x8f,  0x00,   15,   4,   25,       3,    23,   40,   23,   41,  64320},*/
    27,  /* 13.5 Mb  {  TRUE_40,   TRUE_40, TRUE_40,      WLAN_PHY_HT_40_SS,      13500,  13200,  0x80,  0x00,    0,   0,    2,       3,     8,   24,   24,   24,   6684},*/
    54,  /* 27.0 Mb  {  TRUE_40,   TRUE_40, TRUE_40,      WLAN_PHY_HT_40_SS,      27500,  25900,  0x81,  0x00,    1,   2,    4,       3,     9,   25,   25,   25,  13368},*/
    81,  /* 40.5 Mb  {  TRUE_40,   TRUE_40, TRUE_40,      WLAN_PHY_HT_40_SS,      40500,  38600,  0x82,  0x00,    2,   2,    6,       3,    10,   26,   26,   26,  20052},*/
    108, /*   54 Mb  {  TRUE_40,   TRUE_40, TRUE_40,      WLAN_PHY_HT_40_SS,      54000,  49800,  0x83,  0x00,    3,   4,   10,       3,    11,   27,   27,   27,  26738},*/
    162, /*   81 Mb  {  TRUE_40,   TRUE_40, TRUE_40,      WLAN_PHY_HT_40_SS,      81500,  72200,  0x84,  0x00,    4,   4,   14,       3,    12,   28,   28,   28,  40104},*/
    216, /*  108 Mb  {  FALSE,     TRUE_40, TRUE_40,      WLAN_PHY_HT_40_SS,     108000,  92900,  0x85,  0x00,    5,   4,   20,       3,    13,   29,   29,   29,  53476},*/
    243, /* 121.5Mb  {  FALSE,     TRUE_40, TRUE_40,      WLAN_PHY_HT_40_SS,     121500, 102700,  0x86,  0x00,    6,   4,   23,       3,    14,   30,   30,   30,  60156},*/
    270, /*  135 Mb  {  FALSE,     TRUE_40, FALSE,        WLAN_PHY_HT_40_SS,     135000, 112000,  0x87,  0x00,    7,   4,   25,       3,    15,   31,   32,   32,  66840},*/
    300, /*  150 Mb  {  FALSE,     TRUE_40, FALSE,        WLAN_PHY_HT_40_SS_HGI, 150000, 122000,  0x87,  0x00,    7,   4,   25,       3,    15,   31,   32,   32,  74200},*/
    54,  /*   27 Mb  {  FALSE,     FALSE,   FALSE,        WLAN_PHY_HT_40_DS,      27000,  25800,  0x88,  0x00,    8,   0,    2,       3,    16,   33,   33,   33,  13360},*/
    108, /*   54 Mb  {  FALSE,     FALSE,   FALSE,        WLAN_PHY_HT_40_DS,      54000,  49800,  0x89,  0x00,    9,   2,    4,       3,    17,   34,   34,   34,  26720},*/
    162, /*   81 Mb  {  FALSE,     FALSE,   FALSE,        WLAN_PHY_HT_40_DS,      81000,  71900,  0x8a,  0x00,   10,   2,    6,       3,    18,   35,   35,   35,  40080},*/
    216, /*  108 Mb  {  TRUE_40,   FALSE,   FALSE,        WLAN_PHY_HT_40_DS,     108000,  92500,  0x8b,  0x00,   11,   4,   10,       3,    19,   36,   36,   36,  53440},*/
    324, /*  162 Mb  {  TRUE_40,   FALSE,   TRUE_40,      WLAN_PHY_HT_40_DS,     162000, 130300,  0x8c,  0x00,   12,   4,   14,       3,    20,   37,   37,   37,  80160},*/
    432, /*  216 Mb  {  TRUE_40,   FALSE,   TRUE_40,      WLAN_PHY_HT_40_DS,     216000, 162800,  0x8d,  0x00,   13,   4,   20,       3,    21,   38,   38,   38, 106880},*/
    486, /*  243 Mb  {  TRUE_40,   FALSE,   TRUE_40,      WLAN_PHY_HT_40_DS,     243000, 178200,  0x8e,  0x00,   14,   4,   23,       3,    22,   39,   39,   39, 120240},*/
    540, /*  270 Mb  {  TRUE_40,   FALSE,   TRUE_40,      WLAN_PHY_HT_40_DS,     270000, 192100,  0x8f,  0x00,   15,   4,   25,       3,    23,   40,   41,   41, 133600},*/
    600, /*  300 Mb  {  TRUE_40,   FALSE,   TRUE_40,      WLAN_PHY_HT_40_DS_HGI, 300000, 207000,  0x8f,  0x00,   15,   4,   25,       3,    23,   40,   41,   41, 148400},*/
    };


    switch (mode)
    {
    case IEEE80211_MODE_11NA_HT20:
    case IEEE80211_MODE_11NA_HT40:
    case IEEE80211_MODE_11NA_HT40MINUS:
    case IEEE80211_MODE_11NA_HT40PLUS:
    return ar5416_11na_table[rix] * 1000000 / 2;
    case IEEE80211_MODE_11NG_HT20:
    case IEEE80211_MODE_11NG_HT40:
    case IEEE80211_MODE_11NG_HT40MINUS:
    case IEEE80211_MODE_11NG_HT40PLUS:
    return ar5416_11ng_table[rix] * 1000000 / 2;
    case IEEE80211_MODE_AUTO:
    default:
        break;
    }
    return 0;
}
#endif /* ATH_SUPPORT_LINUX_STA */


static int
ieee80211_ioctl_siwsens(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_param *sens, char *extra)
{
    debug_print_ioctl(dev->name, SIOCSIWSENS, "siwsens") ;
    return -EOPNOTSUPP;
}

static int
ieee80211_ioctl_giwsens(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_param *sens, char *extra)
{
    debug_print_ioctl(dev->name, SIOCGIWSENS, "giwsens") ;
    sens->value = 1;
    sens->fixed = 1;

    return 0;
}

static int
ieee80211_ioctl_siwnickn(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_point *data, char *nickname)
{
    struct ieee80211vap *vap = NETDEV_TO_VAP(dev);

    if (data->length > IEEE80211_NWID_LEN)
        return -EINVAL;

    memset(vap->iv_nickname, 0, IEEE80211_NWID_LEN);
    memcpy(vap->iv_nickname, nickname, data->length);
    vap->iv_nicknamelen = data->length;

    return 0;
}

static int
ieee80211_ioctl_giwnickn(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_point *data, char *nickname)
{
    struct ieee80211vap *vap = NETDEV_TO_VAP(dev);

    if (data->length > vap->iv_nicknamelen + 1)
        data->length = vap->iv_nicknamelen + 1;
    if (data->length > 0)
    {
        memcpy(nickname, vap->iv_nickname, data->length-1);
        nickname[data->length-1] = '\0';
    }
    return 0;
}
#endif /* notyet */

static int
ieee80211_ioctl_siwfrag(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_param *param, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    u_int32_t val, curval;

    debug_print_ioctl(dev->name, SIOCSIWFRAG, "siwfrag") ;
    if (param->disabled)
        val = IEEE80211_FRAGMT_THRESHOLD_MAX;
    else if (param->value < 256 || param->value > IEEE80211_FRAGMT_THRESHOLD_MAX)
        return -EINVAL;
    else
        val = (param->value & ~0x1);

    if( wlan_get_desired_phymode(vap) < IEEE80211_MODE_11NA_HT20 )
    {
        curval = wlan_get_param(vap, IEEE80211_FRAG_THRESHOLD);
        if (val != curval)
        {
            wlan_set_param(vap, IEEE80211_FRAG_THRESHOLD, val);
            if (IS_UP(dev))
                return -osif_vap_init(dev, RESCAN);
        }
    }
    else
    {
        printk("WARNING: Fragmentation with HT mode NOT ALLOWED!!\n");
        return -EINVAL;
    }
    return 0;
}

static int
ieee80211_ioctl_giwfrag(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_param *param, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;

    debug_print_ioctl(dev->name, SIOCGIWFRAG, "giwfrag") ;
    param->value = wlan_get_param(vap, IEEE80211_FRAG_THRESHOLD);
    param->disabled = (param->value == IEEE80211_FRAGMT_THRESHOLD_MAX);
    param->fixed = 1;

    return 0;
}

static int
ieee80211_ioctl_siwrts(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_param *rts, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    u_int32_t val, curval;

    debug_print_ioctl(dev->name, SIOCSIWRTS, "siwrts") ;
    if (rts->disabled)
        val = IEEE80211_RTS_MAX;
    else if (IEEE80211_RTS_MIN <= rts->value &&
        rts->value <= IEEE80211_RTS_MAX)
        val = rts->value;
    else
        return -EINVAL;

    curval = wlan_get_param(vap, IEEE80211_RTS_THRESHOLD);
    if (val != curval)
    {
        wlan_set_param(vap, IEEE80211_RTS_THRESHOLD, rts->value);
        if (IS_UP(dev))
            return osif_vap_init(dev, RESCAN);
    }
    return 0;
}

static int
ieee80211_ioctl_giwrts(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_param *rts, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;

    debug_print_ioctl(dev->name, SIOCGIWRTS, "giwrts") ;
    rts->value = wlan_get_param(vap, IEEE80211_RTS_THRESHOLD);
    rts->disabled = (rts->value == IEEE80211_RTS_MAX);
    rts->fixed = 1;

    return 0;
}

static int
ieee80211_ioctl_giwap(struct net_device *dev,
    struct iw_request_info *info,
    struct sockaddr *ap_addr, char *extra)
{
    osif_dev *osnetdev = ath_netdev_priv(dev);
    wlan_if_t vap = osnetdev->os_if;
    u_int8_t bssid[IEEE80211_ADDR_LEN];

    debug_print_ioctl(dev->name, SIOCGIWAP, "giwap") ;
#ifdef notyet
    if (vap->iv_flags & IEEE80211_F_DESBSSID)
        IEEE80211_ADDR_COPY(&ap_addr->sa_data, vap->iv_des_bssid);
    else
#endif
    {
        static const u_int8_t zero_bssid[IEEE80211_ADDR_LEN];
        if(osnetdev->is_up) {
            wlan_vap_get_bssid(vap, bssid);
            IEEE80211_ADDR_COPY(&ap_addr->sa_data, bssid);
        } else {
            IEEE80211_ADDR_COPY(&ap_addr->sa_data, zero_bssid);
        }
    }
    ap_addr->sa_family = ARPHRD_ETHER;
    return 0;
}

static int
ieee80211_ioctl_siwap(struct net_device *dev,
    struct iw_request_info *info,
    struct sockaddr *ap_addr, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    u_int8_t des_bssid[IEEE80211_ADDR_LEN];
    u_int8_t zero_bssid[] = { 0,0,0,0,0,0 };

    debug_print_ioctl(dev->name, SIOCSIWAP, "siwap") ;

    /* NB: should only be set when in STA mode */
    if (wlan_vap_get_opmode(vap) != IEEE80211_M_STA &&
        (u_int8_t)osifp->os_opmode != IEEE80211_M_P2P_DEVICE &&
        (u_int8_t)osifp->os_opmode != IEEE80211_M_P2P_CLIENT) {
        return -EINVAL;
    }

    IEEE80211_ADDR_COPY(des_bssid, &ap_addr->sa_data);

    if (IEEE80211_ADDR_EQ(des_bssid, zero_bssid)) {
        wlan_aplist_init(vap);
    } else {
        wlan_aplist_set_desired_bssidlist(vap, 1, &des_bssid);
    }


    if (IS_UP(dev))
        return osif_vap_init(dev, RESCAN);
    return 0;
}

static int
ieee80211_ioctl_giwname(struct net_device *dev,
    struct iw_request_info *info,
    char *name, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    wlan_chan_t c = wlan_get_current_channel(vap, true);

    debug_print_ioctl(dev->name, SIOCGIWNAME, "giwname") ;
    if (IEEE80211_IS_CHAN_108G(c))
        strncpy(name, "IEEE 802.11Tg", IFNAMSIZ);
    else if (IEEE80211_IS_CHAN_108A(c))
        strncpy(name, "IEEE 802.11Ta", IFNAMSIZ);
    else if (IEEE80211_IS_CHAN_TURBO(c))
        strncpy(name, "IEEE 802.11T", IFNAMSIZ);
    else if (IEEE80211_IS_CHAN_ANYG(c))
        strncpy(name, "IEEE 802.11g", IFNAMSIZ);
    else if (IEEE80211_IS_CHAN_A(c))
        strncpy(name, "IEEE 802.11a", IFNAMSIZ);
    else if (IEEE80211_IS_CHAN_B(c))
        strncpy(name, "IEEE 802.11b", IFNAMSIZ);
    else if (IEEE80211_IS_CHAN_11NG(c))
        strncpy(name, "IEEE 802.11ng", IFNAMSIZ);
    else if (IEEE80211_IS_CHAN_11NA(c))
        strncpy(name, "IEEE 802.11na", IFNAMSIZ);
    else
        strncpy(name, "IEEE 802.11", IFNAMSIZ);
    /* XXX FHSS */
    return 0;
}

/*
* Units are in db above the noise floor. That means the
* rssi values reported in the tx/rx descriptors in the
* driver are the SNR expressed in db.
*
* If you assume that the noise floor is -95, which is an
* excellent assumption 99.5 % of the time, then you can
* derive the absolute signal level (i.e. -95 + rssi).
* There are some other slight factors to take into account
* depending on whether the rssi measurement is from 11b,
* 11g, or 11a.   These differences are at most 2db and
* can be documented.
*
* NB: various calculations are based on the orinoco/wavelan
*     drivers for compatibility
*/
static void
set_quality(struct iw_quality *iq, u_int rssi)
{
    if(rssi >= 42)
        iq->qual = 94 ;
    else if(rssi >= 30)
        iq->qual = 85 + ((94-85)*(rssi-30) + (42-30)/2) / (42-30) ;
    else if(rssi >= 5)
        iq->qual = 5 + ((rssi - 5) * (85-5) + (30-5)/2) / (30-5) ;
    else if(rssi >= 1)
        iq->qual = rssi ;
    else
        iq->qual = 0 ;

    iq->noise = 161;        /* -95dBm */
    iq->level = iq->noise + rssi ;
    iq->updated = 0xf;
}

static struct iw_statistics *
    ieee80211_iw_getstats(struct net_device *dev)
{
    osif_dev *osnetdev = ath_netdev_priv(dev);
    wlan_if_t vap = osnetdev->os_if;
    struct iw_statistics *is = &osnetdev->os_iwstats;
    wlan_rssi_info rssi_info;
    struct ieee80211_stats *iv_stats = wlan_get_stats(vap);
    const struct ieee80211_mac_stats *iv_mac_stats = wlan_mac_stats(vap, 0);

#ifdef ATH_SUPPORT_HTC
    struct ieee80211com *ic = vap->iv_ic;

    if ((!ic) || 
        (ic && ic->ic_delete_in_progress)){
        printk("%s : #### delete is in progress, ic %p \n", __func__, ic);

        if (ic)
            return is;
        else
            return NULL;
    }
#endif    

    wlan_getrssi(vap, &rssi_info, WLAN_RSSI_RX);
    set_quality(&is->qual, rssi_info.avg_rssi);
    is->status = osnetdev->is_up;
    is->discard.nwid = iv_stats->is_rx_wrongbss
        + iv_stats->is_rx_ssidmismatch;
    is->discard.code = iv_mac_stats->ims_rx_wepfail
        + iv_mac_stats->ims_rx_decryptcrc;
    is->discard.fragment = 0;
    is->discard.retries = 0;
    is->discard.misc = 0;

    is->miss.beacon = 0;

    return is;
}

#ifdef notyet
static int
finddot11channel(struct ieee80211com *ic, int i, int freq, u_int flags)
{
    struct ieee80211_channel *c;
    int j;

    for (j = i+1; j < ic->ic_nchans; j++)
    {
        c = &ic->ic_channels[j];
        if ((c->ic_freq == freq) && ((c->ic_flags & flags) == flags))
            return 1;
    }
    for (j = 0; j < i; j++)
    {
        c = &ic->ic_channels[j];
        if ((c->ic_freq == freq) && ((c->ic_flags & flags) == flags))
            return 1;
    }
    return 0;
}

static int
find11gchannel(struct ieee80211com *ic, int i, int freq)
{
    for (; i < ic->ic_nchans; i++)
    {
        const struct ieee80211_channel *c = &ic->ic_channels[i];
        if (c->ic_freq == freq && IEEE80211_IS_CHAN_ANYG(c))
            return 1;
    }
    return 0;
}

static struct ieee80211_channel *
    findchannel(struct ieee80211com *ic, int ieee, int mode)
{
    static const u_int chanflags[] = {
        0,          /* IEEE80211_MODE_AUTO */
        IEEE80211_CHAN_A,   /* IEEE80211_MODE_11A */
        IEEE80211_CHAN_B,   /* IEEE80211_MODE_11B */
        IEEE80211_CHAN_PUREG,   /* IEEE80211_MODE_11G */
        IEEE80211_CHAN_FHSS,    /* IEEE80211_MODE_FH */
        IEEE80211_CHAN_108A,    /* IEEE80211_MODE_TURBO_A */
        IEEE80211_CHAN_108G,    /* IEEE80211_MODE_TURBO_G */
        IEEE80211_CHAN_11NA_HT20,      /* IEEE80211_MODE_11NA_HT20 */
        IEEE80211_CHAN_11NG_HT20,      /* IEEE80211_MODE_11NG_HT20 */
        IEEE80211_CHAN_11NA_HT40PLUS,  /* IEEE80211_MODE_11NA_HT40PLUS */
        IEEE80211_CHAN_11NA_HT40MINUS, /* IEEE80211_MODE_11NA_HT40MINUS */
        IEEE80211_CHAN_11NG_HT40PLUS,  /* IEEE80211_MODE_11NG_HT40PLUS */
        IEEE80211_CHAN_11NG_HT40MINUS, /* IEEE80211_MODE_11NG_HT40MINUS */
    };
    u_int modeflags;
    int i;

    modeflags = chanflags[mode];
    for (i = 0; i < ic->ic_nchans; i++)
    {
        struct ieee80211_channel *c = &ic->ic_channels[i];

        if (c->ic_ieee != ieee)
            continue;

        /* Skip channels if required channel spacing (half/quater)
        * is not matched.
        */
        if (ic->ic_chanbwflag != 0)
        {
            if ((ic->ic_chanbwflag & c->ic_flags) == 0)
                continue;
        }

        if (mode == IEEE80211_MODE_AUTO)
        {
            /* ignore turbo channels for autoselect */
            if (IEEE80211_IS_CHAN_TURBO(c))
                continue;
            if (IEEE80211_IS_CHAN_ANYG(c) &&
                (finddot11channel(ic, i, c->ic_freq, IEEE80211_CHAN_11NG_HT20) ||
                finddot11channel(ic, i, c->ic_freq, IEEE80211_CHAN_11NG_HT40PLUS) ||
                finddot11channel(ic, i, c->ic_freq, IEEE80211_CHAN_11NG_HT40MINUS)))
                continue;
            if (IEEE80211_IS_CHAN_A(c) &&
                (finddot11channel(ic, i, c->ic_freq, IEEE80211_CHAN_11NA_HT20) ||
                finddot11channel(ic, i, c->ic_freq, IEEE80211_CHAN_11NA_HT40PLUS) ||
                finddot11channel(ic, i, c->ic_freq, IEEE80211_CHAN_11NA_HT40MINUS)))
                continue;
            if (IEEE80211_IS_CHAN_B(c) &&
                finddot11channel(ic, i, c->ic_freq, IEEE80211_CHAN_11NG_HT20))
                continue;
            if (IEEE80211_IS_CHAN_B(c) &&
                find11gchannel(ic, i, c->ic_freq))
                continue;
            return c;
        }
        else
        {
            if ((c->ic_flags & modeflags) == modeflags)
                return c;
        }
    }
    return NULL;
}

#define IEEE80211_MODE_TURBO_STATIC_A   IEEE80211_MODE_MAX
static int
ieee80211_check_mode_consistency(struct ieee80211com *ic,int mode,struct ieee80211_channel *c)
{
    if (c == IEEE80211_CHAN_ANYC) return 0;
    switch (mode)
    {
    case IEEE80211_MODE_11B:
        if(IEEE80211_IS_CHAN_B(c))
            return 0;
        else
            return 1;
        break;

    case IEEE80211_MODE_11G:
        if(IEEE80211_IS_CHAN_ANYG(c))
            return 0;
        else
            return 1;
        break;

    case IEEE80211_MODE_11A:
        if(IEEE80211_IS_CHAN_A(c))
            return 0;
        else
            return 1;
        break;

    case IEEE80211_MODE_TURBO_STATIC_A:
        if(IEEE80211_IS_CHAN_A(c) && IEEE80211_IS_CHAN_STURBO(c) )
            return 0;
        else
            return 1;
        break;

    case IEEE80211_MODE_AUTO:
        return 0;
        break;

    case IEEE80211_MODE_11NG_HT20:
        if(IEEE80211_IS_CHAN_11NG_HT20(c))
            return 0;
        else
            return 1;
        break;

    case IEEE80211_MODE_11NG_HT40PLUS:
        if(IEEE80211_IS_CHAN_11NG_HT40PLUS(c))
            return 0;
        else
            return 1;
        break;

    case IEEE80211_MODE_11NG_HT40MINUS:
        if(IEEE80211_IS_CHAN_11NG_HT40MINUS(c))
            return 0;
        else
            return 1;
        break;

    case IEEE80211_MODE_11NG_HT40:
        if(IEEE80211_IS_CHAN_11NG_HT40MINUS(c) || IEEE80211_IS_CHAN_11NG_HT40PLUS(c))
            return 0;
        else
            return 1;
        break;

    case IEEE80211_MODE_11NA_HT20:
        if(IEEE80211_IS_CHAN_11NA_HT20(c))
            return 0;
        else
            return 1;
        break;

    case IEEE80211_MODE_11NA_HT40PLUS:
        if(IEEE80211_IS_CHAN_11NA_HT40PLUS(c))
            return 0;
        else
            return 1;
        break;

    case IEEE80211_MODE_11NA_HT40MINUS:
        if(IEEE80211_IS_CHAN_11NA_HT40MINUS(c))
            return 0;
        else
            return 1;
        break;

    case IEEE80211_MODE_11NA_HT40:
        if(IEEE80211_IS_CHAN_11NA_HT40MINUS(c) || IEEE80211_IS_CHAN_11NA_HT40PLUS(c))
            return 0;
        else
            return 1;
        break;
    }
    return 1;

}
#undef  IEEE80211_MODE_TURBO_STATIC_A
#endif

/******************************************************************************/
/*!
**  \brief Set Operating mode and frequency
**
**  -- Enter Detailed Description --
**
**  \param param1 Describe Parameter 1
**  \param param2 Describe Parameter 2
**  \return Describe return value, or N/A for void
*/

static int
ieee80211_ioctl_siwfreq(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_freq *freq, char *extra)
{
    osif_dev *osnetdev = ath_netdev_priv(dev);
    wlan_if_t vap = osnetdev->os_if;
    int i;
    int retval = 0;
    debug_print_ioctl(dev->name, SIOCSIWFREQ, "siwfreq") ;

    if (freq->e > 1)
        return -EINVAL;
    /*
    * Necessary to cast, to properly interpret negative channel numbers
    */
    if (freq->e == 1)
        i = (u_int8_t)wlan_mhz2ieee(osnetdev->os_devhandle, freq->m / 100000, 0);
    else
        i = freq->m;

    if (i == 0)
        i = IEEE80211_CHAN_ANY;
    
    if (vap->iv_opmode == IEEE80211_M_IBSS) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s : IBSS desired channel(%d)\n",
                                                     __func__, i);        
        return wlan_set_desired_ibsschan(vap, i);        
    } else if(vap->iv_opmode == IEEE80211_M_HOSTAP) {
        wlan_mlme_stop_bss(vap, 0);
        retval = wlan_set_channel(vap, i);
        if(!retval) {
            return IS_UP(dev) ? -osif_vap_init(dev, RESCAN) : 0;
        }
            return retval;
    } else {
        retval = wlan_set_channel(vap, i);
        return retval;
    }
}

static int
ieee80211_ioctl_giwfreq(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_freq *freq, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    wlan_chan_t chan;

    debug_print_ioctl(dev->name, SIOCGIWFREQ, "giwfreq") ;
    if (dev->flags & (IFF_UP | IFF_RUNNING)) {
        chan = wlan_get_bss_channel(vap);
    } else {
        chan = wlan_get_current_channel(vap, true);
    }

    if (chan != IEEE80211_CHAN_ANYC) {
        freq->m = chan->ic_freq * 100000;
    } else {
        freq->m = 0;
    }
    freq->e = 1;


    return 0;
}

#ifdef ATH_SUPERG_XR
/*
* Copy desired ssid state from one vap to another.
*/
static void
copy_des_ssid(struct ieee80211vap *dst, const struct ieee80211vap *src)
{
    dst->iv_des_nssid = src->iv_des_nssid;
    memcpy(dst->iv_des_ssid, src->iv_des_ssid,
        src->iv_des_nssid * sizeof(src->iv_des_ssid[0]));
}
#endif /* ATH_SUPERG_XR */

static int
ieee80211_ioctl_siwessid(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_point *data, char *ssid)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    enum ieee80211_opmode opmode = wlan_vap_get_opmode(vap);
    ieee80211_ssid   tmpssid;

    debug_print_ioctl(dev->name, SIOCSIWESSID, "siwessid") ;
    if (opmode == IEEE80211_M_WDS)
        return -EOPNOTSUPP;

    OS_MEMZERO(&tmpssid, sizeof(ieee80211_ssid));

    if (data->flags == 0)
    {     /* ANY */
        tmpssid.ssid[0] = '\0';
        tmpssid.len = 0;
    }
    else
    {
        if (data->length > IEEE80211_NWID_LEN)
            data->length = IEEE80211_NWID_LEN;
        tmpssid.len = data->length;
        OS_MEMCPY(tmpssid.ssid, ssid, data->length);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG, "set SIOC80211NWID, %d characters\n", data->length);
        /*
        * Deduct a trailing \0 since iwconfig passes a string
        * length that includes this.  Unfortunately this means
        * that specifying a string with multiple trailing \0's
        * won't be handled correctly.  Not sure there's a good
        * solution; the API is botched (the length should be
        * exactly those bytes that are meaningful and not include
        * extraneous stuff).
        */
        if (data->length > 0 &&
            tmpssid.ssid[data->length-1] == '\0')
            tmpssid.len--;
    }
#ifdef ATH_SUPERG_XR
    if (vap->iv_xrvap != NULL && !(vap->iv_flags & IEEE80211_F_XR))
    {
        if (data->flags == 0)
            vap->iv_des_nssid = 0;
        else
            copy_des_ssid(vap->iv_xrvap, vap);
    }
#endif
    wlan_set_desired_ssidlist(vap,1,&tmpssid);

    // printk(" \n DES SSID SET=%s \n", tmpssid.ssid);

#ifdef ATH_SUPPORT_P2P
        /* For P2P supplicant we do not want start connnection as soon as ssid is set */
        /* The difference in behavior between non p2p supplicant and p2p supplicant need to be fixed */
        /* see EV 73753 for more details */
    if (((u_int8_t)osifp->os_opmode == IEEE80211_M_P2P_CLIENT
         || osifp->os_opmode == IEEE80211_M_STA
         || (u_int8_t)osifp->os_opmode == IEEE80211_M_P2P_GO) && !vap->auto_assoc)
        return 0;
#endif

    return (IS_UP(dev) && (vap->iv_ic->ic_roaming != IEEE80211_ROAMING_MANUAL)) ? -osif_vap_init(dev, RESCAN) : 0;
}

static int
ieee80211_ioctl_giwessid(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_point *data, char *essid)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    enum ieee80211_opmode opmode = wlan_vap_get_opmode(vap);
    ieee80211_ssid ssidlist[1];
    int des_nssid;

    debug_print_ioctl(dev->name, SIOCGIWESSID, "giwessid") ;
    if (opmode == IEEE80211_M_WDS)
        return -EOPNOTSUPP;
    des_nssid = wlan_get_desired_ssidlist(vap, ssidlist, 1);
    data->flags = 1;        /* active */
    if (des_nssid > 0)
    {
        data->length = ssidlist[0].len;
        OS_MEMCPY(essid, ssidlist[0].ssid, ssidlist[0].len);
    }
    else
    {
        if (opmode == IEEE80211_M_HOSTAP) data->length = 0;
        else
        {
            wlan_get_bss_essid(vap, ssidlist);
            data->length = ssidlist[0].len;
            OS_MEMCPY(essid, ssidlist[0].ssid, ssidlist[0].len);
        }
    }
    return 0;
}

/*
* Get a key index from a request.  If nothing is
* specified in the request we use the current xmit
* key index.  Otherwise we just convert the index
* to be base zero.
*/
static int
getiwkeyix(wlan_if_t vap, const struct iw_point* erq, u_int16_t *kix)
{
    int kid;

    kid = erq->flags & IW_ENCODE_INDEX;
    if (kid < 1 || kid > IEEE80211_WEP_NKID)
    {
        kid = wlan_get_default_keyid(vap);
        if (kid == IEEE80211_KEYIX_NONE)
            kid = 0;
    }
    else
        --kid;
    if (0 <= kid && kid < IEEE80211_WEP_NKID)
    {
        *kix = kid;
        return 0;
    }
    else
        return -EINVAL;
}

static const u_int8_t ieee80211broadcastaddr[IEEE80211_ADDR_LEN] =
{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/*
 * If authmode = IEEE80211_AUTH_OPEN, script apup would skip authmode setup.
 * Do default authmode setup here for OPEN mode.
 */
static int siwencode_wep(struct net_device *dev)
{
    osif_dev            *osifp = ath_netdev_priv(dev);
    wlan_if_t           vap    = osifp->os_if;
    int                 error  = 0;
    u_int               nmodes = 1;
    ieee80211_auth_mode modes[1];
        
    osifp->authmode = IEEE80211_AUTH_OPEN;

    modes[0] = IEEE80211_AUTH_OPEN;
    error = wlan_set_authmodes(vap, modes, nmodes);
    if (error == 0 ) {
        error = wlan_set_param(vap, IEEE80211_FEATURE_PRIVACY, 0);
        osifp->uciphers[0] = osifp->mciphers[0] = IEEE80211_CIPHER_NONE;
        osifp->u_count = osifp->m_count = 1;
    }
    
    return IS_UP(dev) ? -osif_vap_init(dev, RESCAN) : 0;
}

static int
ieee80211_ioctl_siwencode(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_point *erq, char *keybuf)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    ieee80211_keyval key_val;
    u_int16_t kid;
    int error = -EOPNOTSUPP;
    u_int8_t keydata[IEEE80211_KEYBUF_SIZE];
    int wepchange = 0;

    debug_print_ioctl(dev->name, SIOCSIWENCODE, "siwencode") ;

    if (ieee80211_crypto_wep_mbssid_enabled())
        wlan_set_param(vap, IEEE80211_WEP_MBSSID, 1);  /* wep keys will start from 4 in keycache for support wep multi-bssid */
    else
        wlan_set_param(vap, IEEE80211_WEP_MBSSID, 0);  /* wep keys will allocate index 0-3 in keycache */

    if ((erq->flags & IW_ENCODE_DISABLED) == 0)
    {
        /*
        * Enable crypto, set key contents, and
        * set the default transmit key.
        */
        error = getiwkeyix(vap, erq, &kid);
        if (error)
            return error;
        if (erq->length > IEEE80211_KEYBUF_SIZE)
            return -EINVAL;

        /* XXX no way to install 0-length key */
            if (erq->length > 0)
            {
                /*
                 * ieee80211_match_rsn_info() IBSS mode need.
                 * Otherwise, it caused crash when tx frame find tx rate 
                 *   by node RateControl info not update.
                 */
                if (osifp->os_opmode == IEEE80211_M_IBSS) {
                    /* set authmode to IEEE80211_AUTH_OPEN */
                    siwencode_wep(dev);

                    /* set keymgmtset to WPA_ASE_NONE */
                    wlan_set_rsn_cipher_param(vap, IEEE80211_KEYMGT_ALGS, WPA_ASE_NONE);
                }

                OS_MEMCPY(keydata, keybuf, erq->length);
                memset(&key_val, 0, sizeof(ieee80211_keyval));
                key_val.keytype = IEEE80211_CIPHER_WEP;
                key_val.keydir = IEEE80211_KEY_DIR_BOTH;
                key_val.keylen = erq->length;
                key_val.keydata = keydata;
                key_val.macaddr = (u_int8_t *)ieee80211broadcastaddr;

                if (wlan_set_key(vap,kid,&key_val) != 0)
                    return -EINVAL;
            }
            else
            {
                /*
                * When the length is zero the request only changes
                * the default transmit key.  Verify the new key has
                * a non-zero length.
                */
                wlan_set_default_keyid(vap,kid);
            }
            if (error == 0)
            {
                /*
                * The default transmit key is only changed when:
                * 1. Privacy is enabled and no key matter is
                *    specified.
                * 2. Privacy is currently disabled.
                * This is deduced from the iwconfig man page.
                */
                if (erq->length == 0 ||
                    (wlan_get_param(vap,IEEE80211_FEATURE_PRIVACY)) == 0)
                    wlan_set_default_keyid(vap,kid);
                    wepchange = (wlan_get_param(vap,IEEE80211_FEATURE_PRIVACY)) == 0;
                    wlan_set_param(vap,IEEE80211_FEATURE_PRIVACY, 1);
                }
    }
    else
    {
        if (wlan_get_param(vap,IEEE80211_FEATURE_PRIVACY) == 0)
            return 0;
        wlan_set_param(vap,IEEE80211_FEATURE_PRIVACY, 0);
        wepchange = 1;
        error = 0;
    }
    if (error == 0)
    {
        /* Set policy for unencrypted frames */
        if ((erq->flags & IW_ENCODE_OPEN) &&
            (!(erq->flags & IW_ENCODE_RESTRICTED)))
        {
            wlan_set_param(vap,IEEE80211_FEATURE_DROP_UNENC, 0);
        }
        else if (!(erq->flags & IW_ENCODE_OPEN) &&
            (erq->flags & IW_ENCODE_RESTRICTED))
        {
            wlan_set_param(vap,IEEE80211_FEATURE_DROP_UNENC, 1);
        }
        else
        {
            /* Default policy */
            if (wlan_get_param(vap,IEEE80211_FEATURE_PRIVACY))
                wlan_set_param(vap,IEEE80211_FEATURE_DROP_UNENC, 1);
            else
                wlan_set_param(vap,IEEE80211_FEATURE_DROP_UNENC, 0);
        }
    }
    if (error == 0 && IS_UP(dev) && wepchange)
    {
        /*
        * Device is up and running; we must kick it to
        * effect the change.  If we're enabling/disabling
        * crypto use then we must re-initialize the device
        * so the 802.11 state machine is reset.  Otherwise
        * the key state should have been updated above.
        */

        error = -osif_vap_init(dev, RESCAN);
    }
#ifdef ATH_SUPERG_XR
    /* set the same params on the xr vap device if exists */
    if(!error && vap->iv_xrvap && !(vap->iv_flags & IEEE80211_F_XR) )
        ieee80211_ioctl_siwencode(vap->iv_xrvap->iv_dev,info,erq,keybuf);
#endif
    return error;
}

static int
ieee80211_ioctl_giwencode(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_point *erq, char *key)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    ieee80211_keyval k;
    u_int16_t kid;
    int error;
    u_int8_t *macaddr;
    debug_print_ioctl(dev->name, SIOCGIWENCODE, "giwencode") ;
    if (wlan_get_param(vap,IEEE80211_FEATURE_PRIVACY))
    {
        error = getiwkeyix(vap, erq, &kid);
        if (error != 0)
            return error;
        macaddr = (u_int8_t *)ieee80211broadcastaddr;
        k.keydata = key;
        error = wlan_get_key(vap, kid, macaddr, &k, erq->length);
        if (error != 0)
            return error;
        /* XXX no way to return cipher/key type */

        erq->flags = kid + 1;           /* NB: base 1 */
        if (erq->length > k.keylen)
            erq->length = k.keylen;
        erq->flags |= IW_ENCODE_ENABLED;
    }
    else
    {
        erq->length = 0;
        erq->flags = IW_ENCODE_DISABLED;
    }
    if (wlan_get_param(vap,IEEE80211_FEATURE_DROP_UNENC))
        erq->flags |= IW_ENCODE_RESTRICTED;
    else
        erq->flags |= IW_ENCODE_OPEN;
    return 0;
}

static int
ieee80211_ioctl_giwrange(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_point *data, char *extra)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    wlan_dev_t ic = wlan_vap_get_devhandle(vap);
    struct iw_range *range = (struct iw_range *) extra;
    wlan_chan_t *chans;
    int nchans,  nrates;
    u_int8_t reported[IEEE80211_CHAN_BYTES];    /* XXX stack usage? */
    u_int8_t *rates;
    int i;
    int step = 0;
    u_int32_t txpowlimit;

    debug_print_ioctl(dev->name, SIOCGIWRANGE, "giwrange") ;
    data->length = sizeof(struct iw_range);
    memset(range, 0, sizeof(struct iw_range));

    /* TODO: could fill num_txpower and txpower array with
    * something; however, there are 128 different values.. */
    /* txpower (128 values, but will print out only IW_MAX_TXPOWER) */
#if WIRELESS_EXT >= 10
        txpowlimit = wlan_get_param(vap, IEEE80211_TXPOWER);
    range->num_txpower = (txpowlimit >= 8) ? IW_MAX_TXPOWER : txpowlimit;
    step = txpowlimit / (2 * (IW_MAX_TXPOWER - 1));

    range->txpower[0] = 0;
    for (i = 1; i < IW_MAX_TXPOWER; i++)
        range->txpower[i] = (txpowlimit/2)
            - (IW_MAX_TXPOWER - i - 1) * step;
#endif


    range->txpower_capa = IW_TXPOW_DBM;

    if (osifp->os_opmode == IEEE80211_M_STA ||
        osifp->os_opmode == IEEE80211_M_IBSS ||
        (u_int8_t)osifp->os_opmode == IEEE80211_M_P2P_CLIENT) {
        range->min_pmp = 1 * 1024;
        range->max_pmp = 65535 * 1024;
        range->min_pmt = 1 * 1024;
        range->max_pmt = 1000 * 1024;
        range->pmp_flags = IW_POWER_PERIOD;
        range->pmt_flags = IW_POWER_TIMEOUT;
        range->pm_capa = IW_POWER_PERIOD | IW_POWER_TIMEOUT |
            IW_POWER_UNICAST_R | IW_POWER_ALL_R;
    }

    range->we_version_compiled = WIRELESS_EXT;
    range->we_version_source = 13;

    range->retry_capa = IW_RETRY_LIMIT;
    range->retry_flags = IW_RETRY_LIMIT;
    range->min_retry = 0;
    range->max_retry = 255;

    chans = (wlan_chan_t *)OS_MALLOC(osifp->os_handle,
                        sizeof(wlan_chan_t) * IEEE80211_CHAN_MAX, GFP_KERNEL);
    if (chans == NULL)
        return ENOMEM;
    nchans = wlan_get_channel_list(ic, IEEE80211_MODE_AUTO,
                                    chans, IEEE80211_CHAN_MAX);
    range->num_channels = nchans;
    range->num_frequency = 0;

    memset(reported, 0, sizeof(reported));
    for (i = 0; i < nchans; i++)
    {
        const wlan_chan_t c = chans[i];
        /* discard if previously reported (e.g. b/g) */
        if (isclr(reported, c->ic_ieee))
        {
            setbit(reported, c->ic_ieee);
            range->freq[range->num_frequency].i = c->ic_ieee;
            range->freq[range->num_frequency].m =
                c->ic_freq * 100000;
            range->freq[range->num_frequency].e = 1;
            if (++range->num_frequency == IW_MAX_FREQUENCIES) {
                break;
            }
        }
    }
    OS_FREE(chans);

    /* Max quality is max field value minus noise floor */
    range->max_qual.qual  = 0xff - 161;
#if WIRELESS_EXT >= 19
    /* XXX: This should be updated to use the current noise floor. */
    /* These are negative full bytes.
    * Min. quality is noise + 1 */
    range->max_qual.updated |= IW_QUAL_DBM;
    range->max_qual.level = -95 + 1;
    range->max_qual.noise = -95;
#else
    /* Values larger than the maximum are assumed to be absolute */
    range->max_qual.level = 0;
    range->max_qual.noise = 0;
#endif
    range->sensitivity = 3;

    range->max_encoding_tokens = IEEE80211_WEP_NKID;
    /* XXX query driver to find out supported key sizes */
    range->num_encoding_sizes = 3;
    range->encoding_size[0] = 5;        /* 40-bit */
    range->encoding_size[1] = 13;       /* 104-bit */
    range->encoding_size[2] = 16;       /* 128-bit */

    rates = (u_int8_t *)OS_MALLOC(osifp->os_handle, IEEE80211_RATE_MAXSIZE, GFP_KERNEL);
    if (rates == NULL)
        return ENOMEM;
    /* XXX this only works for station mode */
    if (wlan_get_bss_rates(vap, rates, IEEE80211_RATE_MAXSIZE, &nrates) == 0) {
        range->num_bitrates = nrates;
        if (range->num_bitrates > IW_MAX_BITRATES)
            range->num_bitrates = IW_MAX_BITRATES;
        for (i = 0; i < range->num_bitrates; i++)
        {
            range->bitrate[i] = (rates[i] * 1000000) / 2;
        }
    }
    OS_FREE(rates);


    /* estimated maximum TCP throughput values (bps) */
    range->throughput = 5500000;

    range->min_rts = 0;
    range->max_rts = 2347;
    range->min_frag = 256;
    range->max_frag = IEEE80211_FRAGMT_THRESHOLD_MAX;

#if WIRELESS_EXT >= 17
    /* Event capability (kernel) */
    IW_EVENT_CAPA_SET_KERNEL(range->event_capa);

    /* Event capability (driver) */
    if (osifp->os_opmode == IEEE80211_M_STA ||
        osifp->os_opmode == IEEE80211_M_IBSS ||
        osifp->os_opmode == IEEE80211_M_AHDEMO ||
        (u_int8_t)osifp->os_opmode == IEEE80211_M_P2P_CLIENT) {
        /* for now, only ibss, ahdemo, sta has this cap */
        IW_EVENT_CAPA_SET(range->event_capa, SIOCGIWSCAN);
    }

    if (osifp->os_opmode == IEEE80211_M_STA ||
        (u_int8_t)osifp->os_opmode == IEEE80211_M_P2P_CLIENT) {
        /* for sta only */
        IW_EVENT_CAPA_SET(range->event_capa, SIOCGIWAP);
        IW_EVENT_CAPA_SET(range->event_capa, IWEVREGISTERED);
        IW_EVENT_CAPA_SET(range->event_capa, IWEVEXPIRED);
    }

    /* this is used for reporting replay failure, which is used by the different encoding schemes */
    IW_EVENT_CAPA_SET(range->event_capa, IWEVCUSTOM);
#endif

#if WIRELESS_EXT >= 18
    /* report supported WPA/WPA2 capabilities to userspace */
    range->enc_capa = IW_ENC_CAPA_WPA | IW_ENC_CAPA_WPA2 |
                IW_ENC_CAPA_CIPHER_TKIP | IW_ENC_CAPA_CIPHER_CCMP;
#endif

    return 0;
}

#if ATH_SUPPORT_IWSPY
/* Add handler for SIOCSIWSPY/SIOCGIWSPY */
/*
 * Return the pointer to the spy data in the driver.
 * Because this is called on the Rx path via wireless_spy_update(),
 * we want it to be efficient...
 */
static struct iw_spy_data *ieee80211_get_spydata(struct net_device *dev)
{
	osif_dev  *osifp = ath_netdev_priv(dev);
    
	return &osifp->spy_data;
}

static int
ieee80211_ioctl_siwspy(struct net_device *dev,
		       struct iw_request_info *info,
		       union iwreq_data *wrqu, char *extra)
{
	struct iw_spy_data  *spydata = ieee80211_get_spydata(dev);
	struct sockaddr     *address = (struct sockaddr *) extra;

	/* Make sure driver is not buggy or using the old API */
	if(!spydata)
		return -EOPNOTSUPP;

	/* Disable spy collection while we copy the addresses.
	 * While we copy addresses, any call to wireless_spy_update()
	 * will NOP. This is OK, as anyway the addresses are changing. */
	spydata->spy_number = 0;

	/* Are there are addresses to copy? */
	if(wrqu->data.length > 0) {
		int i;

		/* Copy addresses */
		for(i = 0; i < wrqu->data.length; i++)
			memcpy(spydata->spy_address[i], address[i].sa_data,
			       ETH_ALEN);
		/* Reset stats */
		memset(spydata->spy_stat, 0,
		       sizeof(struct iw_quality) * IW_MAX_SPY);

#if 0
		printk("%s() spydata %p, num %d\n", __func__, spydata, wrqu->data.length);
		for (i = 0; i < wrqu->data.length; i++)
			printk(KERN_DEBUG
			       "%02X:%02X:%02X:%02X:%02X:%02X \n",
			       spydata->spy_address[i][0],
			       spydata->spy_address[i][1],
			       spydata->spy_address[i][2],
			       spydata->spy_address[i][3],
			       spydata->spy_address[i][4],
			       spydata->spy_address[i][5]);
#endif
	}

	/* Enable addresses */
	spydata->spy_number = wrqu->data.length;

	return 0;
}

static int
ieee80211_ioctl_giwspy(struct net_device *dev,
		       struct iw_request_info *info,
		       union iwreq_data *wrqu, char *extra)
{
    struct iw_spy_data  *spydata = ieee80211_get_spydata(dev);
	struct sockaddr     *address = (struct sockaddr *) extra;
	int			        i;

	/* Make sure driver is not buggy or using the old API */
	if(!spydata)
		return -EOPNOTSUPP;

	wrqu->data.length = spydata->spy_number;

	/* Copy addresses. */
	for(i = 0; i < spydata->spy_number; i++) 	{
		memcpy(address[i].sa_data, spydata->spy_address[i], ETH_ALEN);
		address[i].sa_family = AF_UNIX;
	}
	/* Copy stats to the user buffer (just after). */
	if(spydata->spy_number > 0)
		memcpy(extra  + (sizeof(struct sockaddr) *spydata->spy_number),
		       spydata->spy_stat,
		       sizeof(struct iw_quality) * spydata->spy_number);
	/* Reset updated flags. */
	for(i = 0; i < spydata->spy_number; i++)
		spydata->spy_stat[i].updated &= ~IW_QUAL_ALL_UPDATED;
	return 0;
}
#endif

static int
ieee80211_ioctl_siwmode(struct net_device *dev,
    struct iw_request_info *info,
    __u32 *mode, char *extra)
{
    //struct ieee80211vap *vap = ath_netdev_priv(dev);
    osif_dev *osnetdev = ath_netdev_priv(dev);
    struct ifmediareq imr;
    int valid = 0;

    debug_print_ioctl(dev->name, SIOCSIWMODE, "siwmode") ;
    memset(&imr, 0, sizeof(imr));
    osnetdev->os_media.ifm_status(dev, &imr);

    if (imr.ifm_active & IFM_IEEE80211_HOSTAP)
        valid = (*mode == IW_MODE_MASTER);
#if WIRELESS_EXT >= 15
    else if (imr.ifm_active & IFM_IEEE80211_MONITOR)
        valid = (*mode == IW_MODE_MONITOR);
#endif
    else if (imr.ifm_active & IFM_IEEE80211_ADHOC)
        valid = (*mode == IW_MODE_ADHOC);
    else if (imr.ifm_active & IFM_IEEE80211_WDS)
        valid = (*mode == IW_MODE_REPEAT);
    else
        valid = (*mode == IW_MODE_INFRA);

    printk(" %s: imr.ifm_active=%d, new mode=%d, valid=%d \n",
            __func__, imr.ifm_active, *mode, valid);

    return valid ? 0 : -EINVAL;
}

static int
ieee80211_ioctl_giwmode(struct net_device *dev,
    struct iw_request_info *info,
    __u32 *mode, char *extra)
{
    struct ifmediareq imr;
    osif_dev *osnetdev = ath_netdev_priv(dev);

    debug_print_ioctl(dev->name, SIOCGIWMODE, "giwmode") ;
    memset(&imr, 0, sizeof(imr));
    osnetdev->os_media.ifm_status(dev, &imr);

    if (imr.ifm_active & IFM_IEEE80211_HOSTAP)
        *mode = IW_MODE_MASTER;
#if WIRELESS_EXT >= 15
    else if (imr.ifm_active & IFM_IEEE80211_MONITOR)
        *mode = IW_MODE_MONITOR;
#endif
    else if (imr.ifm_active & IFM_IEEE80211_ADHOC)
        *mode = IW_MODE_ADHOC;
    else if (imr.ifm_active & IFM_IEEE80211_WDS)
        *mode = IW_MODE_REPEAT;
    else
        *mode = IW_MODE_INFRA;
    return 0;
}

static int
ieee80211_ioctl_siwpower(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_param *wrq, char *extra)
{
    struct ieee80211vap *vap = NETDEV_TO_VAP(dev);
    struct ieee80211com *ic = vap->iv_ic;
    int ret;

    if (wrq->disabled)
    {
        if (ic->ic_flags & IEEE80211_F_PMGTON)
        {
            ic->ic_flags &= ~IEEE80211_F_PMGTON;
            goto done;
        }
        return 0;
    }

    if ((ic->ic_caps & IEEE80211_C_PMGT) == 0)
        return -EOPNOTSUPP;
    switch (wrq->flags & IW_POWER_MODE)
    {
    case IW_POWER_UNICAST_R:
    case IW_POWER_ALL_R:
    case IW_POWER_ON:
        ic->ic_flags |= IEEE80211_F_PMGTON;
        break;
    default:
        return -EINVAL;
    }
    if (wrq->flags & IW_POWER_TIMEOUT)
    {
        ic->ic_holdover = IEEE80211_MS_TO_TU(wrq->value);
        ic->ic_flags |= IEEE80211_F_PMGTON;
    }
    if (wrq->flags & IW_POWER_PERIOD)
    {
        ic->ic_lintval = IEEE80211_MS_TO_TU(wrq->value);
        ic->ic_flags |= IEEE80211_F_PMGTON;
    }
done:
    if (IS_UP(dev)) {
        ic->ic_reset_start(ic, 0);
        ret = ic->ic_reset(ic);
        ic->ic_reset_end(ic, 0);
        return -ret;
    }
    return 0;
}

static int
ieee80211_ioctl_giwpower(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_param *rrq, char *extra)
{
    struct ieee80211vap *vap = NETDEV_TO_VAP(dev);
    struct ieee80211com *ic = vap->iv_ic;

    rrq->disabled = (ic->ic_flags & IEEE80211_F_PMGTON) == 0;
    if (!rrq->disabled)
    {
        switch (rrq->flags & IW_POWER_TYPE)
        {
        case IW_POWER_TIMEOUT:
            rrq->flags = IW_POWER_TIMEOUT;
            rrq->value = IEEE80211_TU_TO_MS(ic->ic_holdover);
            break;
        case IW_POWER_PERIOD:
            rrq->flags = IW_POWER_PERIOD;
            rrq->value = IEEE80211_TU_TO_MS(ic->ic_lintval);
            break;
        }
        rrq->flags |= IW_POWER_ALL_R;
    }
    return 0;
}

#ifdef notyet
static int
ieee80211_ioctl_siwretry(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_param *rrq, char *extra)
{
    struct ieee80211vap *vap = NETDEV_TO_VAP(dev);
    struct ieee80211com *ic = vap->iv_ic;
    int ret;

    if (rrq->disabled)
    {
        if (vap->iv_flags & IEEE80211_F_SWRETRY)
        {
            vap->iv_flags &= ~IEEE80211_F_SWRETRY;
            goto done;
        }
            return 0;
    }

    if ((vap->iv_caps & IEEE80211_C_SWRETRY) == 0)
        return -EOPNOTSUPP;
    if (rrq->flags == IW_RETRY_LIMIT) {
        if (rrq->value >= 0) {
            vap->iv_txmin = rrq->value;
            vap->iv_txmax = rrq->value; /* XXX */
            vap->iv_txlifetime = 0;     /* XXX */
            vap->iv_flags |= IEEE80211_F_SWRETRY;
        } else {
            vap->iv_flags &= ~IEEE80211_F_SWRETRY;
        }
        return 0;
    }
done:
    if (IS_UP(vap->iv_dev)) {
        ic->ic_reset_start(vap->iv_ic, 0);
        ret = ic->ic_reset(vap->iv_ic);
        ic->ic_reset_end(vap->iv_ic, 0);
        return -ret;
    }
    return 0;
}

static int
ieee80211_ioctl_giwretry(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_param *rrq, char *extra)
{
    struct ieee80211vap *vap = NETDEV_TO_VAP(dev);

    rrq->disabled = (vap->iv_flags & IEEE80211_F_SWRETRY) == 0;
    if (!rrq->disabled)
    {
        switch (rrq->flags & IW_RETRY_TYPE)
        {
        case IW_RETRY_LIFETIME:
            rrq->flags = IW_RETRY_LIFETIME;
            rrq->value = IEEE80211_TU_TO_MS(vap->iv_txlifetime);
            break;
        case IW_RETRY_LIMIT:
            rrq->flags = IW_RETRY_LIMIT;
            switch (rrq->flags & IW_RETRY_MODIFIER)
            {
            case IW_RETRY_MIN:
                rrq->flags |= IW_RETRY_MAX;
                rrq->value = vap->iv_fixed_rate.retries;
                break;
            case IW_RETRY_MAX:
                rrq->flags |= IW_RETRY_MAX;
                rrq->value = vap->iv_fixed_rate.retries;
                break;
            }
            break;
        }
    }
    return 0;
}

#endif /* notyet */


struct waplistreq
{
    wlan_if_t vap;
    struct sockaddr addr[IW_MAX_AP];
    struct iw_quality qual[IW_MAX_AP];
    int i;
};

static int
waplist_cb(void *arg, wlan_scan_entry_t se)
{
    struct waplistreq *req = arg;
    int i = req->i;
    wlan_if_t vap = req->vap;
    enum ieee80211_opmode opmode = wlan_vap_get_opmode(vap);
    u_int8_t *se_macaddr = wlan_scan_entry_macaddr(se);
    u_int8_t *se_bssid = wlan_scan_entry_bssid(se);
    u_int8_t se_rssi = wlan_scan_entry_rssi(se);

    if (i >= IW_MAX_AP)
        return 0;
    req->addr[i].sa_family = ARPHRD_ETHER;
    if (opmode == IEEE80211_M_HOSTAP)
        IEEE80211_ADDR_COPY(req->addr[i].sa_data, se_macaddr);
    else
        IEEE80211_ADDR_COPY(req->addr[i].sa_data, se_bssid);

    set_quality(&req->qual[i], se_rssi);

    req->i = i+1;

    return 0;
}

static int
ieee80211_ioctl_iwaplist(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_point *data, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct waplistreq *req;
    debug_print_ioctl(dev->name, SIOCGIWAPLIST, "iwaplist") ;

    req = (struct waplistreq *)OS_MALLOC(osifp->os_handle, sizeof(*req), GFP_KERNEL);
    if (req == NULL)
        return -ENOMEM;
    req->vap = vap;
    req->i = 0;
    wlan_scan_table_iterate(vap, waplist_cb, req);

    data->length = req->i;
    OS_MEMCPY(extra, &req->addr, req->i*sizeof(req->addr[0]));
    data->flags = 1;        /* signal quality present (sort of) */
    OS_MEMCPY(extra + req->i*sizeof(req->addr[0]), &req->qual,
        req->i*sizeof(req->qual[0]));
    OS_FREE(req);

    return 0;

}

#ifdef SIOCGIWSCAN
static int
ieee80211_ioctl_siwscan(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_point *data, char *extra)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    ieee80211_scan_params *scan_params;
#ifdef ATH_SUPPORT_P2P
    enum ieee80211_opmode opmode = osifp->os_opmode;
#else
    enum ieee80211_opmode opmode = wlan_vap_get_opmode(vap);
#endif
    ieee80211_ssid    ssid_list[IEEE80211_SCAN_MAX_SSID];
    int               n_ssid;
    u_int8_t          opt_ie[IEEE80211_MAX_OPT_IE];
    u_int32_t         length=0;


    debug_print_ioctl(dev->name, SIOCSIWSCAN, "siwscan") ;
    /*
    * XXX don't permit a scan to be started unless we
    * know the device is ready.  For the moment this means
    * the device is marked up as this is the required to
    * initialize the hardware.  It would be better to permit
    * scanning prior to being up but that'll require some
    * changes to the infrastructure.
    */
    if (!(dev->flags & IFF_UP)) {
        return -EINVAL;     /* XXX */
    }

#ifdef notyet
    /* Do we still need this for new UMAC? */
    /* wlan_set_channel not support channel 0 in STA mode */
    if ((vap->iv_state == IEEE80211_S_SCAN) && (ic->ic_opmode == IEEE80211_M_STA))
        vap->iv_des_chan=IEEE80211_CHAN_ANYC;
#endif /* notyet */
    /* XXX always manual... */
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
        "%s: active scan request\n", __func__);
    preempt_scan(dev, 100, 100);

    if ((time_after(OS_GET_TICKS(), osifp->os_last_siwscan + OS_SIWSCAN_TIMEOUT)) && (osifp->os_giwscan_count == 0)) {
        osifp->os_last_siwscan = OS_GET_TICKS();
    }

#if WIRELESS_EXT > 17
    if (data && (data->flags & IW_SCAN_THIS_ESSID)) {
        struct iw_scan_req req;
        int copyLength;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
            "%s: SCAN_THIS_ESSID requested\n", __func__);
        if (data->length > sizeof req) {
            copyLength = sizeof req;
        } else {
            copyLength = data->length;
        }
        OS_MEMZERO(&req, sizeof req);
        if (_copy_from_user(&req, data->pointer, copyLength))
            return -EFAULT;
        OS_MEMCPY(&ssid_list[0].ssid, req.essid, sizeof(ssid_list[0].ssid));
        ssid_list[0].len = req.essid_len;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
                "%s: requesting scan of essid '%s'\n", __func__, ssid_list[0].ssid);
        n_ssid = 1;
    } else
#endif
    {
        n_ssid = wlan_get_desired_ssidlist(vap, ssid_list, IEEE80211_SCAN_MAX_SSID);
    }

    /* Fill scan parameter */
    scan_params = (ieee80211_scan_params *)
    OS_MALLOC(osifp->os_handle, sizeof(*scan_params), GFP_KERNEL);
    if (scan_params == NULL)
        return -ENOMEM;
    OS_MEMZERO(scan_params,sizeof(ieee80211_scan_params));
#if defined(UMAC_SUPPORT_RESMGR) && defined(ATH_SUPPORT_P2P) && defined(ATH_SUPPORT_HTC)
    /* If resource management intervene scan behavior in P2P concurrent and will force back to home 
       channel even in disconnect state and have large scan time for split driver. Force beacon 
       check count to 0 to pass beacon count check to reduce total scan time. */
    {
        bool connected_flag = false;

        if(osifp->sm_handle && wlan_connection_sm_is_connected(osifp->sm_handle))
            connected_flag = true;
        wlan_set_default_scan_parameters(vap,scan_params,opmode,true,true,
                                         connected_flag,true,n_ssid,ssid_list,0);
    }
#else
    wlan_set_default_scan_parameters(vap,scan_params,opmode,true,true,true,true,n_ssid,ssid_list,0);
#endif

    switch ((u_int8_t)opmode)
    {
    case IEEE80211_M_HOSTAP:
        scan_params->flags = IEEE80211_SCAN_PASSIVE | IEEE80211_SCAN_ALLBANDS;
        scan_params->type = IEEE80211_SCAN_FOREGROUND;
        /* XXX tunables */
        scan_params->min_dwell_time_passive = 200;
        scan_params->max_dwell_time_passive = 300;

        break;

    /* TODO:properly scan parameter depend on opmode */
    case IEEE80211_M_P2P_DEVICE:
    default:
        if ((u_int8_t)opmode == IEEE80211_M_P2P_CLIENT) {
            scan_params->min_dwell_time_active = scan_params->max_dwell_time_active = 104;
            scan_params->repeat_probe_time =  30;
        } else {
            scan_params->min_dwell_time_active = scan_params->max_dwell_time_active = 100;
        }

        if (ieee80211_vap_vap_ind_is_set(vap)) {
            scan_params->type = IEEE80211_SCAN_REPEATER_BACKGROUND;
        } else if (osifp->sm_handle && wlan_connection_sm_is_connected(osifp->sm_handle)) {
            scan_params->type = IEEE80211_SCAN_BACKGROUND;
        } else {
            scan_params->type = IEEE80211_SCAN_FOREGROUND;
        }
        scan_params->flags = IEEE80211_SCAN_ALLBANDS | IEEE80211_SCAN_ACTIVE;
        if (!wlan_mlme_get_optie(vap, opt_ie, &length, IEEE80211_MAX_OPT_IE)) {
            scan_params->ie_data = opt_ie;
            scan_params->ie_len = length;
        }
        break;
    }

    if (osifp->os_scan_band != OSIF_SCAN_BAND_ALL) {
        scan_params->flags &= ~IEEE80211_SCAN_ALLBANDS;
        if (osifp->os_scan_band == OSIF_SCAN_BAND_2G_ONLY)
            scan_params->flags |= IEEE80211_SCAN_2GHZ;
        else if (osifp->os_scan_band == OSIF_SCAN_BAND_5G_ONLY)
            scan_params->flags |= IEEE80211_SCAN_5GHZ;
        else {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
                                "%s: unknow scan band, scan all bands.\n", __func__);
            scan_params->flags |= IEEE80211_SCAN_ALLBANDS;
        }
    }

    if (wlan_scan_start(vap, scan_params, osifp->scan_requestor, IEEE80211_SCAN_PRIORITY_LOW, &(osifp->scan_id)) != 0 ) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
            "%s: Issue a scan fail.\n",
            __func__);
    }

    OS_FREE(scan_params);

    return 0;
}
#if ATH_SUPPORT_WAPI
static const char wapi_leader[] = "wapi_ie=";
#endif

#if WIRELESS_EXT > 14
/*
* Encode a WPA or RSN information element as a custom
* element using the hostap format.
*/
static u_int
encode_ie(void *buf, size_t bufsize,
    const u_int8_t *ie, size_t ielen,
    const char *leader, size_t leader_len)
{
    u_int8_t *p;
    int i;

    if (bufsize < leader_len)
        return 0;
    p = buf;
    OS_MEMCPY(p, leader, leader_len);
    bufsize -= leader_len;
    p += leader_len;
    for (i = 0; i < ielen && bufsize > 2; i++) {
        /* %02x might not be exactly 2 chars but could be 3 or more. 
         * Use snprintf instead. */
        p += snprintf(p, 3, "%02x", ie[i]); 
        bufsize -= 2;
    }
    return (i == ielen ? p - (u_int8_t *)buf : 0);
}
#endif /* WIRELESS_EXT > 14 */

#ifdef ATH_SUPPORT_P2P
#define WLAN_EID_VENDOR_SPECIFIC 221
#define P2P_IE_VENDOR_TYPE 0x506f9a09
#define WPA_GET_BE32(a) ((((u32) (a)[0]) << 24) | (((u32) (a)[1]) << 16) | \
                        (((u32) (a)[2]) << 8) | ((u32) (a)[3]))

int get_p2p_ie(const u8 *ies, u_int16_t ies_len, u_int8_t *buf,
                u_int16_t *buf_len)
{
    const u8 *end, *pos, *ie;

    *buf_len = 0;
    pos = ies;
    end = ies + ies_len;
    ie = NULL;

    while (pos + 1 < end) {
        if (pos + 2 + pos[1] > end)
            return -1;
        if (pos[0] == WLAN_EID_VENDOR_SPECIFIC && pos[1] >= 4 &&
            WPA_GET_BE32(&pos[2]) == P2P_IE_VENDOR_TYPE) {
            ie = pos;
            break;
        }
        pos += 2 + pos[1];
    }

    if (ie == NULL)
        return -1; /* No specified vendor IE found */

    /*
    * There may be multiple vendor IEs in the message, so need to
    * concatenate their data fields.
    */
    while (pos + 1 < end) {
        if (pos + 2 + pos[1] > end)
            break;
        if (pos[0] == WLAN_EID_VENDOR_SPECIFIC && pos[1] >= 4 &&
            WPA_GET_BE32(&pos[2]) == P2P_IE_VENDOR_TYPE) {
            OS_MEMCPY(buf,  pos, pos[1]);
            *buf_len += pos[1];
            buf += pos[1];
        }
        pos += 2 + pos[1];
    }

    return 0;
}


#endif /* ATH_SUPPORT_P2P */
#endif  /* SIOCGIWSCAN */

struct iwscanreq
{
    struct net_device *dev;
    char            *current_ev;
    char            *end_buf;
    int         mode;
    struct iw_request_info *info;
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,27)
#define CURRENT_EV info, current_ev
#else
#define CURRENT_EV current_ev
#endif

static int
giwscan_cb(void *arg, wlan_scan_entry_t se)
{
    struct iwscanreq *req = arg;
    struct net_device *dev = req->dev;
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    char *current_ev = req->current_ev;
    char *end_buf = req->end_buf;
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,27)
    struct iw_request_info *info = req->info;
#endif

#if WIRELESS_EXT > 14
#define MAX_IE_LENGTH 30 + 255 * 2 /* "xxx_ie=" + encoded IE */
    char buf[MAX_IE_LENGTH];
#ifndef IWEVGENIE
    static const char rsn_leader[] = "rsn_ie=";
    static const char wpa_leader[] = "wpa_ie=";
#endif
#endif
#ifdef ATH_WPS_IE
#if WIRELESS_EXT <= 20
    static const char wps_leader[] = "wps_ie=";
#endif /* WIRELESS_EXT <= 20 */

#ifdef ATH_SUPPORT_P2P
    static const char p2p_ie_leader[] = "p2p_ie=";
    u_int8_t *p2p_buf, *ie_buf=NULL;
    u_int16_t p2p_buf_len = 0, ie_buf_len=0;
#endif
    u_int8_t *se_wps_ie = wlan_scan_entry_wps(se);
#endif /* ATH_WPS_IE */
    char *last_ev;
    struct iw_event iwe;
    char *current_val;
    int j;
    u_int8_t *se_wpa_ie = wlan_scan_entry_wpa(se);
#if ATH_SUPPORT_WAPI
    u_int8_t *se_wapi_ie = wlan_scan_entry_wapi(se);
#endif
    u_int8_t *se_rsn_ie = wlan_scan_entry_rsn(se);
    u_int8_t *se_macaddr = wlan_scan_entry_macaddr(se);
    u_int8_t *se_bssid = wlan_scan_entry_bssid(se);
    enum ieee80211_opmode opmode = wlan_vap_get_opmode(vap);
    u_int8_t se_ssid_len;
    u_int8_t *se_ssid = wlan_scan_entry_ssid(se, &se_ssid_len);
    enum ieee80211_opmode se_opmode = wlan_scan_entry_bss_type(se);
    wlan_chan_t se_chan = wlan_scan_entry_channel(se);
    u_int8_t se_rssi = wlan_scan_entry_rssi(se);
    u_int8_t se_privacy = wlan_scan_entry_privacy(se);
    u_int8_t *se_rates = wlan_scan_entry_rates(se);
    u_int8_t *se_xrates = wlan_scan_entry_xrates(se);
    u_int16_t se_intval = wlan_scan_entry_beacon_interval(se);
    u_int8_t *se_wmeparam = wlan_scan_entry_wmeparam_ie(se);
    u_int8_t *se_wmeinfo = wlan_scan_entry_wmeinfo_ie(se);
    u_int8_t *se_ath_ie = wlan_scan_entry_athcaps(se);


    if (current_ev >= end_buf) {
        return E2BIG;
    }

#define AGING_TIMEOUT_SEC 60
    if ((long unsigned int)wlan_scan_entry_age(se)> 1000* AGING_TIMEOUT_SEC){ 
        return 0;
    }

    /* WPA/!WPA sort criteria */


#if ATH_SUPPORT_WAPI
    if ((req->mode != 0) ^ ((se_wpa_ie != NULL) || (se_rsn_ie != NULL) || (se_wapi_ie != NULL))) {
        return 0;
    }
#else
    if ((req->mode != 0) ^ ((se_wpa_ie != NULL) || (se_rsn_ie != NULL) )) {
        return 0;
    }
#endif

    OS_MEMZERO(&iwe, sizeof(iwe));
    last_ev = current_ev;
    iwe.cmd = SIOCGIWAP;
    iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
    if (opmode == IEEE80211_M_HOSTAP) {
        IEEE80211_ADDR_COPY(iwe.u.ap_addr.sa_data, se_macaddr);
    } else {
        IEEE80211_ADDR_COPY(iwe.u.ap_addr.sa_data, se_bssid);
    }
    current_ev = iwe_stream_add_event(CURRENT_EV,
        end_buf, &iwe, IW_EV_ADDR_LEN);

    /* We ran out of space in the buffer. */
    if (last_ev == current_ev) {
        return E2BIG;
    }

    OS_MEMZERO(&iwe, sizeof(iwe));
    last_ev = current_ev;
    iwe.cmd = SIOCGIWESSID;
    iwe.u.data.flags = 1;
#ifdef notyet
    /* This is not available for AP Scan */
    if (opmode == IEEE80211_M_HOSTAP) {
        ieee80211_ssid    ssid_list[IEEE80211_SCAN_MAX_SSID];
        int               n_ssid;

        n_ssid = wlan_get_desired_ssidlist(vap, ssid_list, 1);
        iwe.u.data.length = n_ssid > 0 ?
            ssid_list[0].len : 0;
        current_ev = iwe_stream_add_point(CURRENT_EV,
            end_buf, &iwe, ssid_list[0].ssid);
    } else
#endif /* notyet */
    {
        iwe.u.data.length = se_ssid_len;
        current_ev = iwe_stream_add_point(CURRENT_EV,
            end_buf, &iwe, (char *) se_ssid);
    }

    /* We ran out of space in the buffer. */
    if (last_ev == current_ev) {
        return E2BIG;
    }

    if ((se_opmode == IEEE80211_M_STA) || (se_opmode == IEEE80211_M_IBSS)) {
        OS_MEMZERO(&iwe, sizeof(iwe));
        last_ev = current_ev;
        iwe.cmd = SIOCGIWMODE;
        iwe.u.mode = se_opmode == IEEE80211_M_STA ?
            IW_MODE_MASTER : IW_MODE_ADHOC;
        current_ev = iwe_stream_add_event(CURRENT_EV,
            end_buf, &iwe, IW_EV_UINT_LEN);
        /* We ran out of space in the buffer. */
        if (last_ev == current_ev) {
            return E2BIG;
        }
    }

    OS_MEMZERO(&iwe, sizeof(iwe));
    last_ev = current_ev;
    iwe.cmd = SIOCGIWFREQ;
    iwe.u.freq.m = wlan_channel_frequency(se_chan) * 100000;
    iwe.u.freq.e = 1;
    current_ev = iwe_stream_add_event(CURRENT_EV,
        end_buf, &iwe, IW_EV_FREQ_LEN);

    /* We ran out of space in the buffer. */
    if (last_ev == current_ev) {
        return E2BIG;
    }

    OS_MEMZERO(&iwe, sizeof(iwe));
    last_ev = current_ev;
    iwe.cmd = IWEVQUAL;
    set_quality(&iwe.u.qual, se_rssi);
    current_ev = iwe_stream_add_event(CURRENT_EV,
        end_buf, &iwe, IW_EV_QUAL_LEN);

    /* We ran out of space in the buffer. */
    if (last_ev == current_ev) {
        return E2BIG;
    }

    OS_MEMZERO(&iwe, sizeof(iwe));
    last_ev = current_ev;
    iwe.cmd = SIOCGIWENCODE;
    if (se_privacy) {
        iwe.u.data.flags = IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
    } else {
        iwe.u.data.flags = IW_ENCODE_DISABLED;
    }
    iwe.u.data.length = 0;
    current_ev = iwe_stream_add_point(CURRENT_EV, end_buf, &iwe, "");

    /* We ran out of space in the buffer. */
    if (last_ev == current_ev) {
        return E2BIG;
    }

    OS_MEMZERO(&iwe, sizeof(iwe));
    last_ev = current_ev;
    iwe.cmd = SIOCGIWRATE;
    current_val = current_ev + IW_EV_LCP_LEN;
    /* NB: not sorted, does it matter? */
    if (se_rates != NULL) {
        for (j = 0; j < se_rates[1]; j++) {
            int r = se_rates[2+j] & IEEE80211_RATE_VAL;
            if (r != 0) {
                iwe.u.bitrate.value = r * (1000000 / 2);
                current_val = iwe_stream_add_value(CURRENT_EV,
                    current_val, end_buf, &iwe,
                    IW_EV_PARAM_LEN);
            }
        }
    }
    if (se_xrates != NULL) {
        for (j = 0; j < se_xrates[1]; j++) {
            int r = se_xrates[2+j] & IEEE80211_RATE_VAL;
            if (r != 0) {
                iwe.u.bitrate.value = r * (1000000 / 2);
                current_val = iwe_stream_add_value(CURRENT_EV,
                    current_val, end_buf, &iwe,
                    IW_EV_PARAM_LEN);
            }
        }
    }
    /* remove fixed header if no rates were added */
    if ((current_val - current_ev) > IW_EV_LCP_LEN) {
        current_ev = current_val;
    } else {
        /* We ran out of space in the buffer. */
        if (last_ev == current_ev)
            return E2BIG;
    }

#if WIRELESS_EXT > 14
    OS_MEMZERO(&iwe, sizeof(iwe));
    last_ev = current_ev;
    iwe.cmd = IWEVCUSTOM;
    snprintf(buf, sizeof(buf), "bcn_int=%d", se_intval);
    iwe.u.data.length = strlen(buf);
    current_ev = iwe_stream_add_point(CURRENT_EV, end_buf, &iwe, buf);

    /* We ran out of space in the buffer. */
    if (last_ev == current_ev) {
        return E2BIG;
    }

    if (se_rsn_ie != NULL) {
    last_ev = current_ev;

#ifdef IWEVGENIE

        OS_MEMZERO(&iwe, sizeof(iwe));
        if ((se_rsn_ie[1] + 2) > MAX_IE_LENGTH) {
            return E2BIG;
        }
        OS_MEMCPY(buf, se_rsn_ie, se_rsn_ie[1] + 2);
        iwe.cmd = IWEVGENIE;
        iwe.u.data.length = se_rsn_ie[1] + 2;

#else
        OS_MEMZERO(&iwe, sizeof(iwe));
        iwe.cmd = IWEVCUSTOM;
        if (se_rsn_ie[0] == IEEE80211_ELEMID_RSN) {
            iwe.u.data.length = encode_ie(buf, sizeof(buf),
                se_rsn_ie, se_rsn_ie[1] + 2,
                rsn_leader, sizeof(rsn_leader)-1);
        }
#endif
        if (iwe.u.data.length != 0) {
            current_ev = iwe_stream_add_point(CURRENT_EV, end_buf,
                &iwe, buf);

            /* We ran out of space in the buffer */
            if (last_ev == current_ev) {
            return E2BIG;
            }
        }

    }
#ifdef ATH_SUPPORT_WAPI
    if (se_wapi_ie != NULL) {
      last_ev = current_ev;

        OS_MEMZERO(&iwe, sizeof(iwe));
        iwe.cmd = IWEVCUSTOM;
        iwe.u.data.length = encode_ie(buf, sizeof(buf), se_wapi_ie, se_wapi_ie[1] + 2, wapi_leader, sizeof(wapi_leader)-1);

        if (iwe.u.data.length != 0) {
            current_ev = iwe_stream_add_point(CURRENT_EV, end_buf,
                &iwe, buf);
            
            /* We ran out of space in the buffer */
            if (last_ev == current_ev) {
              return E2BIG;
            }
        }

    } 
#endif /*ATH_SUPPORT_WAPI*/

    if (se_wpa_ie != NULL) {
    last_ev = current_ev;
#ifdef IWEVGENIE
        OS_MEMZERO(&iwe, sizeof(iwe));
        if ((se_wpa_ie[1] + 2) > MAX_IE_LENGTH) {
            return E2BIG;
        }
        OS_MEMCPY(buf, se_wpa_ie, se_wpa_ie[1] + 2);
        iwe.cmd = IWEVGENIE;
        iwe.u.data.length = se_wpa_ie[1] + 2;
#else
        OS_MEMZERO(&iwe, sizeof(iwe));
        iwe.cmd = IWEVCUSTOM;

            iwe.u.data.length = encode_ie(buf, sizeof(buf),
                se_wpa_ie, se_wpa_ie[1]+2,
                wpa_leader, sizeof(wpa_leader)-1);
#endif
        if (iwe.u.data.length != 0) {
            current_ev = iwe_stream_add_point(CURRENT_EV, end_buf,
                &iwe, buf);

            /* We ran out of space in the buffer. */
            if (last_ev == current_ev) {
            return E2BIG;
            }
        }

    }
    if (se_wmeparam != NULL) {
        static const char wme_leader[] = "wme_ie=";

        OS_MEMZERO(&iwe, sizeof(iwe));
        last_ev = current_ev;
        iwe.cmd = IWEVCUSTOM;
        iwe.u.data.length = encode_ie(buf, sizeof(buf),
            se_wmeparam, se_wmeparam[1]+2,
            wme_leader, sizeof(wme_leader)-1);
        if (iwe.u.data.length != 0) {
            current_ev = iwe_stream_add_point(CURRENT_EV, end_buf,
                &iwe, buf);
        }

        /* We ran out of space in the buffer. */
        if (last_ev == current_ev) {
            return E2BIG;
        }
    } else if (se_wmeinfo != NULL) {
        static const char wme_leader[] = "wme_ie=";

        OS_MEMZERO(&iwe, sizeof(iwe));
        last_ev = current_ev;
        iwe.cmd = IWEVCUSTOM;
        iwe.u.data.length = encode_ie(buf, sizeof(buf),
            se_wmeinfo, se_wmeinfo[1]+2,
            wme_leader, sizeof(wme_leader)-1);
        if (iwe.u.data.length != 0) {
            current_ev = iwe_stream_add_point(CURRENT_EV, end_buf,
                &iwe, buf);
        }

        /* We ran out of space in the buffer. */
        if (last_ev == current_ev) {
            return E2BIG;
        }
    }
    if (se_ath_ie != NULL) {
        static const char ath_leader[] = "ath_ie=";

        OS_MEMZERO(&iwe, sizeof(iwe));
        last_ev = current_ev;
        iwe.cmd = IWEVCUSTOM;
        iwe.u.data.length = encode_ie(buf, sizeof(buf),
            se_ath_ie, se_ath_ie[1]+2,
            ath_leader, sizeof(ath_leader)-1);
        if (iwe.u.data.length != 0) {
            current_ev = iwe_stream_add_point(CURRENT_EV, end_buf,
                &iwe, buf);
        }

        /* We ran out of space in the buffer. */
        if (last_ev == current_ev) {
            return E2BIG;
        }
    }
#ifdef ATH_WPS_IE
    if (se_wps_ie != NULL) {
        last_ev = current_ev;
#if WIRELESS_EXT > 20
        OS_MEMZERO(&iwe, sizeof(iwe));
        if ((se_wps_ie[1] + 2) > MAX_IE_LENGTH) {
            return E2BIG;
        }
        OS_MEMCPY(buf, se_wps_ie, se_wps_ie[1] + 2);
        iwe.cmd = IWEVGENIE;
        iwe.u.data.length = se_wps_ie[1] + 2;
#else
        OS_MEMZERO(&iwe, sizeof(iwe));
        iwe.cmd = IWEVCUSTOM;
        iwe.u.data.length = encode_ie(buf, sizeof(buf),
        se_wps_ie, se_wps_ie[1] + 2,
        wps_leader, sizeof(wps_leader) - 1);
#endif  /* WIRELESS_EXT > 20 */
        if (iwe.u.data.length != 0) {
            current_ev = iwe_stream_add_point(CURRENT_EV, end_buf,
                &iwe, buf);

            /* We ran out of space in the buffer */
            if (last_ev == current_ev) {
                return E2BIG;
            }
        }
    }

#endif /* ATH_WPS_IE */

#ifdef ATH_SUPPORT_P2P
        ie_buf_len = wlan_scan_entry_ie_len(se);

        if (ie_buf_len)
            ie_buf = OS_MALLOC(osifp->os_handle, ie_buf_len,GFP_KERNEL);

        if (ie_buf_len &&
            wlan_scan_entry_copy_ie_data(se, ie_buf, &ie_buf_len) == EOK) {
            p2p_buf = OS_MALLOC(osifp->os_handle, MAX_IE_LENGTH, GFP_KERNEL);
            if (p2p_buf == NULL) {
                if (ie_buf_len)
                    OS_FREE(ie_buf);
                return ENOMEM;
            }
            if (get_p2p_ie(ie_buf, ie_buf_len, p2p_buf, &p2p_buf_len) == 0) {

                last_ev = current_ev;
                OS_MEMZERO(&iwe, sizeof(iwe));
                iwe.cmd = IWEVCUSTOM;
                iwe.u.data.length = encode_ie(buf, sizeof(buf),
                                            p2p_buf, p2p_buf_len,
                                            p2p_ie_leader,
                                            sizeof(p2p_ie_leader) - 1);
                if (iwe.u.data.length != 0) {

                    current_ev = iwe_stream_add_point(CURRENT_EV, end_buf,
                                                    &iwe, buf);

                    if (last_ev == current_ev) {
                        if (ie_buf_len)
                            OS_FREE(ie_buf);
                        OS_FREE(p2p_buf);
                        return E2BIG;
                    }
                }
            }
            OS_FREE(p2p_buf);
        }

        if (ie_buf_len)
            OS_FREE(ie_buf);


#endif /* ATH_SUPPORT_P2P */

    req->current_ev = current_ev;

    return 0;
}

static int
ieee80211_ioctl_giwscan(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_point *data, char *extra)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct iwscanreq req;
    int res = 0;
    ieee80211_auth_mode modes[IEEE80211_AUTH_MAX];
    int count, i;

    debug_print_ioctl(dev->name, SIOCGIWSCAN, "giwscan") ;

    if (osifp->os_opmode != IEEE80211_M_STA ||
        (u_int8_t)osifp->os_opmode != IEEE80211_M_P2P_DEVICE ||
        (u_int8_t)osifp->os_opmode != IEEE80211_M_P2P_CLIENT) {
        /* For station mode - umac connection sm always runs SCAN */
        if (wlan_scan_in_progress(vap) &&
            (time_after(osifp->os_last_siwscan + OS_SIWSCAN_TIMEOUT, OS_GET_TICKS()))) {
            osifp->os_giwscan_count++;
            return -EAGAIN;
        }
    }

    req.dev = dev;
    req.current_ev = extra;
    req.info = info;
    if (data->length == 0) {
        req.end_buf = extra + IW_SCAN_MAX_DATA;
    } else {
        req.end_buf = extra + data->length;
    }

    /*
    * Do two passes to insure WPA/non-WPA scan candidates
    * are sorted to the front.  This is a hack to deal with
    * the wireless extensions capping scan results at
    * IW_SCAN_MAX_DATA bytes.  In densely populated environments
    * it's easy to overflow this buffer (especially with WPA/RSN
    * information elements).  Note this sorting hack does not
    * guarantee we won't overflow anyway.
    */
       count = wlan_get_auth_modes(vap, modes, IEEE80211_AUTH_MAX);
    for (i = 0; i < count; i++) {
        if (modes[i] == IEEE80211_AUTH_WPA) {
            req.mode |= 0x1;
        }
        if (modes[i] == IEEE80211_AUTH_RSNA) {
            req.mode |= 0x2;
        }
    }
    res = wlan_scan_table_iterate(vap, giwscan_cb, &req);

    if (res == 0) {
        req.mode = req.mode ? 0 : 0x3;
        res = wlan_scan_table_iterate(vap, giwscan_cb, &req);
    }

    data->length = req.current_ev - extra;

    if (res != 0) {
        return -res;
    }

    osifp->os_giwscan_count = 0;

    return res;
}
#endif /* SIOCGIWSCAN */

#ifdef notyet
static int
cipher2cap(int cipher)
{
    switch (cipher)
    {
    case IEEE80211_CIPHER_WEP:  return IEEE80211_C_WEP;
    case IEEE80211_CIPHER_AES_OCB:  return IEEE80211_C_AES;
    case IEEE80211_CIPHER_AES_CCM:  return IEEE80211_C_AES_CCM;
    case IEEE80211_CIPHER_CKIP: return IEEE80211_C_CKIP;
    case IEEE80211_CIPHER_TKIP: return IEEE80211_C_TKIP;
    }
    return 0;
}

#endif /* notyet */

#define IEEE80211_MODE_TURBO_STATIC_A   IEEE80211_MODE_MAX

static int
ieee80211_convert_mode(const char *mode)
{
#define TOUPPER(c) ((((c) > 0x60) && ((c) < 0x7b)) ? ((c) - 0x20) : (c))
    static const struct
    {
        char *name;
        int mode;
    } mappings[] = {
        /* NB: need to order longest strings first for overlaps */
        { "11AST" , IEEE80211_MODE_TURBO_STATIC_A },
        { "AUTO"  , IEEE80211_MODE_AUTO },
        { "11A"   , IEEE80211_MODE_11A },
        { "11B"   , IEEE80211_MODE_11B },
        { "11G"   , IEEE80211_MODE_11G },
        { "FH"    , IEEE80211_MODE_FH },
        { "0"     , IEEE80211_MODE_AUTO },
        { "1"     , IEEE80211_MODE_11A },
        { "2"     , IEEE80211_MODE_11B },
        { "3"     , IEEE80211_MODE_11G },
        { "4"     , IEEE80211_MODE_FH },
        { "5"     , IEEE80211_MODE_TURBO_STATIC_A },
        { "TA"      , IEEE80211_MODE_TURBO_A },
        { "TG"      , IEEE80211_MODE_TURBO_G },
        { "11NAHT20"      , IEEE80211_MODE_11NA_HT20 },
        { "11NGHT20"      , IEEE80211_MODE_11NG_HT20 },
        { "11NAHT40PLUS"  , IEEE80211_MODE_11NA_HT40PLUS },
        { "11NAHT40MINUS" , IEEE80211_MODE_11NA_HT40MINUS },
        { "11NGHT40PLUS"  , IEEE80211_MODE_11NG_HT40PLUS },
        { "11NGHT40MINUS" , IEEE80211_MODE_11NG_HT40MINUS },
        { "11NGHT40" , IEEE80211_MODE_11NG_HT40 },
        { "11NAHT40" , IEEE80211_MODE_11NA_HT40 },
        { NULL }
    };
    int i, j;
    const char *cp;

    for (i = 0; mappings[i].name != NULL; i++) {
        cp = mappings[i].name;
        for (j = 0; j < strlen(mode) + 1; j++) {
            /* convert user-specified string to upper case */
            if (TOUPPER(mode[j]) != cp[j])
                break;
            if (cp[j] == '\0')
                return mappings[i].mode;
        }
    }
    return -1;
#undef TOUPPER
}
/*
    * Convert mode to a media specification.
    */
static int
mode2media(enum ieee80211_phymode mode)
{
        static const u_int media[] = {
            IFM_AUTO,       /* IEEE80211_MODE_AUTO    */
            IFM_IEEE80211_11A,  /* IEEE80211_MODE_11A     */
            IFM_IEEE80211_11B,  /* IEEE80211_MODE_11B     */
            IFM_IEEE80211_11G,  /* IEEE80211_MODE_11G     */
            IFM_IEEE80211_FH,       /* IEEE80211_MODE_FH      */
            0,          /* IEEE80211_MODE_TURBO_A */
            0,          /* IEEE80211_MODE_TURBO_G */
            IFM_IEEE80211_11NA, /* IEEE80211_MODE_11NA_HT20        */
            IFM_IEEE80211_11NG, /* IEEE80211_MODE_11NG_HT20        */
            IFM_IEEE80211_11NA, /* IEEE80211_MODE_11NA_HT40PLUS    */
            IFM_IEEE80211_11NA, /* IEEE80211_MODE_11NA_HT40MINUS   */
            IFM_IEEE80211_11NG, /* IEEE80211_MODE_11NG_HT40PLUS    */
            IFM_IEEE80211_11NG, /* IEEE80211_MODE_11NG_HT40MINUS   */
            };
            return media[mode];
}

/******************************************************************************/
/*!
**  \brief Set operating mode
**
**  -- Enter Detailed Description --
**
**  \param param1 Describe Parameter 1
**  \param param2 Describe Parameter 2
**  \return Describe return value, or N/A for void
*/


static int
ieee80211_ioctl_setmode(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct iw_point *wri;
#ifdef notyet
    struct ifreq ifr;
#endif
    char s[24];      /* big enough for ``11nght40plus'' */
    int mode;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_SETMODE, "setmode") ;
#ifdef notyet
    if (ic->ic_media.ifm_cur == NULL)
        return -EINVAL;
#endif
    wri = (struct iw_point *) w;
    if (wri->length > sizeof(s))        /* silently truncate */
        wri->length = sizeof(s);
    if (_copy_from_user(s, wri->pointer, wri->length))
        return -EFAULT;
    s[sizeof(s)-1] = '\0';          /* insure null termination */

    /*
    ** Convert mode name into a specific mode
    */

    mode = ieee80211_convert_mode(s);
    if (mode < 0)
        return -EINVAL;

    /* TBD: Should be pushed to umac */
#ifdef notyet
    if(ieee80211_check_mode_consistency(ic,mode,vap->iv_des_chan)) {
        /*
        * error in AP mode.
        * overwrite channel selection in other modes.
        */
        if (vap->iv_opmode == IEEE80211_M_HOSTAP)
            return -EINVAL;
        else
            vap->iv_des_chan=IEEE80211_CHAN_ANYC;
    }

    //ifr_mode = mode;
    ifr_mode = mode2media(mode);
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_media = ic->ic_media.ifm_cur->ifm_media &~ IFM_MMASK;
    if (mode ==  IEEE80211_MODE_TURBO_STATIC_A)
    {
        ifr_mode =  IEEE80211_MODE_11A;
    }
    ifr.ifr_media |= IFM_MAKEMODE(ifr_mode);
    retv = ifmedia_ioctl(ic->ic_dev, &ifr, &ic->ic_media, SIOCSIFMEDIA);
    if ((!retv &&  mode != wlan_get_desired_phymode(vap)) || retv == ENETRESET)
    {
        ieee80211_scan_flush(ic);   /* NB: could optimize */
        vap->iv_des_mode = mode;
        if (IS_UP_AUTO(vap))
            ieee80211_new_state(vap, IEEE80211_S_SCAN, 0);
        retv = 0;
    }
#ifdef ATH_SUPERG_XR
    /* set the same params on the xr vap device if exists */
    if(vap->iv_xrvap && !(vap->iv_flags & IEEE80211_F_XR) )
    {
        vap->iv_xrvap->iv_des_mode = mode;
    }
#endif
    return -retv;
#endif /* notyet */

#if ATH_SUPPORT_IBSS_HT
    /*
     * config ic adhoc ht capability
     */
    if (vap->iv_opmode == IEEE80211_M_IBSS) {

        wlan_dev_t ic = wlan_vap_get_devhandle(vap);    
        
        switch (mode) {
        case IEEE80211_MODE_11NA_HT20:
        case IEEE80211_MODE_11NG_HT20:
            /* enable adhoc ht20 and aggr */
            wlan_set_device_param(ic, IEEE80211_DEVICE_HT20ADHOC, 1); 
            wlan_set_device_param(ic, IEEE80211_DEVICE_HT40ADHOC, 0); 
            break;
        case IEEE80211_MODE_11NA_HT40PLUS:
        case IEEE80211_MODE_11NA_HT40MINUS:
        case IEEE80211_MODE_11NG_HT40PLUS:                                        
        case IEEE80211_MODE_11NG_HT40MINUS:
        case IEEE80211_MODE_11NG_HT40:
        case IEEE80211_MODE_11NA_HT40:
            /* enable adhoc ht40 and aggr */
            wlan_set_device_param(ic, IEEE80211_DEVICE_HT20ADHOC, 1); 
            wlan_set_device_param(ic, IEEE80211_DEVICE_HT40ADHOC, 1); 
            break;
        default:
            /* clear adhoc ht20, ht40, aggr */  
            wlan_set_device_param(ic, IEEE80211_DEVICE_HT20ADHOC, 0); 
            wlan_set_device_param(ic, IEEE80211_DEVICE_HT40ADHOC, 0); 
            break;
        } /* end of switch (mode) */
    }
#endif /* end of #if ATH_SUPPORT_IBSS_HT */    

    return wlan_set_desired_phymode(vap, mode);
}
#undef IEEE80211_MODE_TURBO_STATIC_A

#ifdef notyet
#ifdef ATH_SUPERG_XR
static void
ieee80211_setupxr(struct ieee80211vap *vap)
{
    struct net_device *dev = vap->iv_dev;
    struct ieee80211com *ic = vap->iv_ic;
    if((vap->iv_opmode == IEEE80211_M_HOSTAP)  &&
        !(vap->iv_flags & IEEE80211_F_XR) )
    {
        if((vap->iv_ath_cap  & IEEE80211_ATHC_XR) && !vap->iv_xrvap )
        {
            char name[IFNAMSIZ];
            strncpy(name, dev->name, IFNAMSIZ-1);
            strncat(name, "-xr", IFNAMSIZ-strlen(name)-1);
            /*
            * create a new XR vap. if the normal VAP is already up , bring
            * up the XR vap aswell.
            */
            ieee80211_scan_flush(ic);   /* NB: could optimize */

            vap->iv_xrvap = ic->ic_vap_create(ic, name, vap->iv_unit,
                IEEE80211_M_HOSTAP,IEEE80211_VAP_XR | IEEE80211_CLONE_BSSID,
                NULL);
            vap->iv_xrvap->iv_fragthreshold = IEEE80211_XR_FRAG_THRESHOLD;
            copy_des_ssid(vap->iv_xrvap, vap);
            vap->iv_xrvap->iv_des_mode = vap->iv_des_mode;
        }
        else if(!(vap->iv_ath_cap  & IEEE80211_ATHC_XR) && vap->iv_xrvap )
        {
            /*
            * destroy the XR vap. if the XR VAP is up , bring
            * it down before destroying.
            */
            if(vap->iv_xrvap)
            {
                ieee80211_stop(vap->iv_xrvap->iv_dev);
                ic->ic_vap_delete(vap->iv_xrvap);
            }
            vap->iv_xrvap = NULL;
        }
    }
}
#endif

static int
ieee80211_setathcap(struct ieee80211vap *vap, int cap, int setting)
{
    struct ieee80211com *ic = vap->iv_ic;
    int ocap;

    if ((ic->ic_ath_cap & cap) == 0)
        return -EINVAL;
    ocap = vap->iv_ath_cap;
    if (setting)
        vap->iv_ath_cap |= cap;
    else
        vap->iv_ath_cap &= ~cap;

    /*
    ** Copy the status to the ATH layer (buggy kludge, but whatever)
    */
    ic->ic_ath_cap = vap->iv_ath_cap;
    return (vap->iv_ath_cap != ocap ? ENETRESET : 0);
}

static int
ieee80211_set_turbo(struct net_device *dev, int flag)
{
    struct ieee80211vap *vap = NETDEV_TO_VAP(dev);
    struct ieee80211com *ic = vap->iv_ic;
    struct ifreq ifr;
    struct ieee80211vap *tmpvap = NETDEV_TO_VAP(dev);
    int nvap=0;

    TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next)
        nvap++;
    if (nvap > 2 && flag ) return -EINVAL; /* This will take care of more than 2 vaps */
        /* Reject more than 1 vap without XR configured */
    if (nvap == 2 && !(vap->iv_ath_cap  & IEEE80211_ATHC_XR)  && flag) return -EINVAL;

    ifr.ifr_media = ic->ic_media.ifm_cur->ifm_media &~ IFM_MMASK;
    if(flag)
        ifr.ifr_media |= IFM_IEEE80211_TURBO;
    else
        ifr.ifr_media &= ~IFM_IEEE80211_TURBO;
    (void) ifmedia_ioctl(ic->ic_dev, &ifr, &ic->ic_media, SIOCSIFMEDIA);

    return 0;

}

#endif /* notyet */
struct mlmeop {
    struct ieee80211req_mlme *mlme;
    wlan_if_t vap;
};

static void
domlme(void *arg, wlan_node_t node)
{
    struct mlmeop *op = arg;
    switch (op->mlme->im_op) {
    case IEEE80211_MLME_DISASSOC:
        wlan_mlme_disassoc_request(op->vap,wlan_node_getmacaddr(node),op->mlme->im_reason);
        break;
    case IEEE80211_MLME_DEAUTH:
        wlan_mlme_deauth_request(op->vap,wlan_node_getmacaddr(node),op->mlme->im_reason);
        break;
    default:
        break;
    }
}

static int
ieee80211_ioctl_setmlme(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    struct ieee80211req_mlme *mlme = (struct ieee80211req_mlme *)extra;
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
#if UMAC_SUPPORT_SMARTANTENNA    
    struct ieee80211_node *ni = NULL;
#endif    
    //int force_scan = 0;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_SETMLME, "setmlme") ;

    if (!(dev->flags & IFF_UP)) {
        printk(" DEVICE IS DOWN ifname=%s\n", dev->name);
        return -EINVAL;     /* XXX */
    }

    switch (mlme->im_op) {
    case IEEE80211_MLME_ASSOC:
        printk(" %s: os_opmode=%d \n", __func__, osifp->os_opmode);
        /* set dessired bssid when in STA mode accordingly */
        if (wlan_vap_get_opmode(vap) != IEEE80211_M_STA &&
            (u_int8_t)osifp->os_opmode != IEEE80211_M_P2P_DEVICE &&
            (u_int8_t)osifp->os_opmode != IEEE80211_M_P2P_CLIENT) {
            printk("[%s] non sta mode, skip to set bssid\n", __func__); 
        } else {
            u_int8_t des_bssid[IEEE80211_ADDR_LEN];
            #define is_null_addr(_a)            \
                ((_a)[0] == 0x00 &&             \
                 (_a)[1] == 0x00 &&             \
                 (_a)[2] == 0x00 &&             \
                 (_a)[3] == 0x00 &&             \
                 (_a)[4] == 0x00 &&             \
                 (_a)[5] == 0x00)

            if (!is_null_addr(mlme->im_macaddr)) {
                IEEE80211_ADDR_COPY(des_bssid, &mlme->im_macaddr[0]);
                wlan_aplist_set_desired_bssidlist(vap, 1, &des_bssid);
                printk("[%s] set desired bssid %02x:%02x:%02x:%02x:%02x:%02x\n",__func__,des_bssid[0],
                        des_bssid[1],des_bssid[2],des_bssid[3],des_bssid[4],des_bssid[5]);
            }
        }

        if (osifp->os_opmode == IEEE80211_M_STA ||
            (u_int8_t)osifp->os_opmode == IEEE80211_M_P2P_GO ||
            (u_int8_t)osifp->os_opmode == IEEE80211_M_P2P_CLIENT ||
	    osifp->os_opmode == IEEE80211_M_IBSS) {
            vap->iv_mlmeconnect=1;
            osif_vap_init(dev, 0);
        }
        else
            return -EINVAL;
        break;
    case IEEE80211_MLME_DISASSOC:
    case IEEE80211_MLME_DEAUTH:
        switch ((u_int8_t)osifp->os_opmode) {
        case IEEE80211_M_STA:
        case IEEE80211_M_P2P_CLIENT:
        //    if (mlme->im_op == IEEE80211_MLME_DISASSOC && !osifp->is_p2p_interface) {
        //        return -EINVAL; /*fixme darwin does this, but linux did not before? */
        //    }
            osif_vap_stop(dev);
            break;
        case IEEE80211_M_HOSTAP:
        case IEEE80211_M_P2P_GO:
        case IEEE80211_M_IBSS:
            /* NB: the broadcast address means do 'em all */
        if (!IEEE80211_ADDR_EQ(mlme->im_macaddr, ieee80211broadcastaddr)) {
                wlan_mlme_disassoc_request(vap,mlme->im_macaddr,mlme->im_reason);
        } else {
                struct mlmeop iter_arg;
                iter_arg.mlme = mlme;
                iter_arg.vap = vap;
                wlan_iterate_station_list(vap,domlme,&iter_arg);
            }
            break;
        default:
            return -EINVAL;
        }
        break;
    case IEEE80211_MLME_AUTHORIZE:
    case IEEE80211_MLME_UNAUTHORIZE:
        if (osifp->os_opmode != IEEE80211_M_HOSTAP &&
            (u_int8_t)osifp->os_opmode != IEEE80211_M_P2P_GO) {
            return -EINVAL;
        }
        if (mlme->im_op == IEEE80211_MLME_AUTHORIZE)
        {
            wlan_node_authorize(vap, 1, mlme->im_macaddr);
#if UMAC_SUPPORT_SMARTANTENNA
            ni = ieee80211_find_node(&vap->iv_ic->ic_sta, mlme->im_macaddr);
            if (ni)
            {
                if (vap->iv_ic->ic_get_smartatenna_enable(vap->iv_ic) && (ni->ni_flags & IEEE80211_NODE_HT))
                {

                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_SMARTANT, "%s: Training for Node: %s in Security Mode\n", __func__, ether_sprintf(ni->ni_macaddr));
                    if (ni->is_training == 0) {
                        ni->is_training = 1;
                        ieee80211_smartantenna_start_training(ni, vap->iv_ic);
                    }
                    /* start retrain state m/c */
                    if (vap->iv_ic->ic_smartantennaparams->retraining_enable)
                    OS_SET_TIMER(&vap->iv_ic->ic_smartant_retrain_timer, RETRAIN_INTERVEL); /* ms */
                }
                ieee80211_free_node(ni);
            }
#endif            
        }
        else
            wlan_node_authorize(vap, 0, mlme->im_macaddr);
        break;
    case IEEE80211_MLME_CLEAR_STATS:
#ifdef notyet
        if (vap->iv_opmode != IEEE80211_M_HOSTAP)
                return -EINVAL;
        ni = ieee80211_find_node(&ic->ic_sta, mlme->im_macaddr);
        if (ni == NULL)
                return -ENOENT;

        /* clear statistics */
        memset(&ni->ni_stats, 0, sizeof(struct ieee80211_nodestats));
        ieee80211_free_node(ni);
#endif
        break;

    case IEEE80211_MLME_STOP_BSS:
        osif_vap_stop(dev);
        break;
    default:
        return -EINVAL;
    }
    return 0;
}
#if ATHEROS_LINUX_P2P_DRIVER

static int
ifr_name2unit(const char *name, int *unit)
{
    const char *cp;

    for (cp = name; *cp != '\0' && !('0' <= *cp && *cp <= '9'); cp++)
        ;
    if (*cp != '\0')
    {
        *unit = 0;
        for (; *cp != '\0'; cp++)
        {
            if (!('0' <= *cp && *cp <= '9'))
                return -EINVAL;
            *unit = (*unit * 10) + (*cp - '0');
        }
    }
    else
        *unit = -1;
    return 0;
}

void ieee80211_ioctl_send_action_frame_cb(wlan_if_t vaphandle, wbuf_t wbuf,
        void *arg, u_int8_t *dst_addr,
        u_int8_t *src_addr, u_int8_t *bssid, ieee80211_xmit_status *ts)
{
    osif_dev *osifp = (void *) arg;
    struct net_device *dev = osifp->netdev;
    wlan_if_t vap = osifp->os_if;
    u_int8_t *buf;

    struct ieee80211_send_action_cb *act_cb;
    u_int8_t *frm;
    u_int16_t frm_len;
    size_t tlen;
    union iwreq_data wrqu;

    (void) vap; //if IEEE80211_DPRINTF is not defined, shut up errors.

    frm = wbuf_header(wbuf);
    frm_len = wbuf_get_pktlen(wbuf);
    tlen = frm_len + sizeof(*act_cb);

    buf = OS_MALLOC(osifp->os_handle, tlen, GFP_KERNEL);
    if (!buf) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                              "%s - cb alloc failed\n", __func__);
        return;
    }
    act_cb = (void *) buf;

    memcpy(act_cb->dst_addr, dst_addr, IEEE80211_ADDR_LEN);
    memcpy(act_cb->src_addr, src_addr, IEEE80211_ADDR_LEN);
    memcpy(act_cb->bssid, bssid, IEEE80211_ADDR_LEN);
    act_cb->ack = ts->ts_flags == 0;

    memcpy(&buf[sizeof(*act_cb)], frm, frm_len);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s len=%d ack=%d dst=%s", 
                      __func__, tlen,act_cb->ack, ether_sprintf(dst_addr));
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                              "src=%s ",ether_sprintf(src_addr));
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                              "bssid=%s \n ",ether_sprintf(bssid));
    /*
     * here we queue the data for later user delivery
     */
    {
        wlan_p2p_event event;
        event.u.rx_frame.frame_len = tlen;
        event.u.rx_frame.frame_buf = buf;
        osif_p2p_rx_frame_handler(osifp, &event, IEEE80211_EV_P2P_SEND_ACTION_CB);
    }
    memset(&wrqu, 0, sizeof(wrqu));
    wrqu.data.flags = IEEE80211_EV_P2P_SEND_ACTION_CB;

    wireless_send_event(dev, IWEVCUSTOM, &wrqu, buf);
    OS_FREE(buf);
}
/* returns a positive error code or 0 for success */
static int
ieee80211_ioctl_setp2p(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    union iwreq_data *iwd = w;
    int subioctl = iwd->mode;
    int retv = EINVAL;
    int *inparams = (int *) extra;
    int value = inparams[1];       /* NB: most values are TYPE_INT */

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
            " %s ifname:%s, subioctl=%d len=%d set to %i \n",
            __func__, osifp->netdev->name, subioctl, iwd->data.length, value);

    switch (subioctl) {

    case IEEE80211_IOC_P2P_GO_OPPPS:
        if (osifp->p2p_go_handle) {
            /* set oppPS with driver */

            retv = (wlan_p2p_go_set_param(osifp->p2p_go_handle,
                        WLAN_P2PGO_OPP_PS, value)) ? EINVAL : 0;

        } else  if (osifp->p2p_client_handle) {

            retv = (wlan_p2p_client_set_param(osifp->p2p_client_handle,
                    WLAN_P2P_CLIENT_OPPPS, value)) ? EINVAL : 0;

        } else {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                "%s: set IEEE80211_IOC_P2P_GO_OPPPS but GO handle is NULL\n", __func__);
        }
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set IEEE80211_IOC_P2P_GO_OPPPS to %s, ret=%d\n",
                            value ? "on" : "off", retv);
        break;
    case IEEE80211_IOC_P2P_GO_CTWINDOW:
        if (osifp->p2p_go_handle) {
            /* set oppPS with driver */

            retv = (wlan_p2p_go_set_param(osifp->p2p_go_handle, WLAN_P2PGO_CTWIN,
                                            value)) ? EINVAL : 0;
        } else  if (osifp->p2p_client_handle) {

            retv = (wlan_p2p_client_set_param(osifp->p2p_client_handle,
                                WLAN_P2P_CLIENT_CTWINDOW, value)) ? EINVAL : 0;
        } else {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                "%s: set IEEE80211_IOC_P2P_GO_CTWINDOW but GO handle is NULL\n", __func__);
        }
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                "set IEEE80211_IOC_P2P_GO_CTWINDOW to %d, ret=%d\n", value, retv);
        break;

    case IEEE80211_IOC_P2P_CANCEL_CHANNEL:
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
            "%s: IEEE80211_IOC_P2P_CANCEL_CHANNEL", __func__);
  #ifdef ATH_SUPPORT_HTC
    if (osifp->p2p_handle && wlan_p2p_get_vap_handle(osifp->p2p_handle) &&
        wlan_p2p_cancel_channel(osifp->p2p_handle, IEEE80211_SCAN_CANCEL_WAIT) == EOK)
  #else
    if (osifp->p2p_handle && wlan_p2p_get_vap_handle(osifp->p2p_handle) &&
        wlan_p2p_cancel_channel(osifp->p2p_handle, IEEE80211_SCAN_CANCEL_ASYNC) == EOK)
  #endif
        retv = 0;
    break;

    case IEEE80211_IOC_START_HOSTAP:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s IEEE80211_IOC_START_HOSTAP\n", __func__);
        wlan_mlme_stop_bss(vap, 0);

        retv = wlan_mlme_start_bss(vap);
        if (retv != 0) {
            int waitcnt=0;
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s :mlme returned error %d \n", __func__, retv);
            if (retv == EAGAIN) {
                   /* Radio resource is busy on scanning, try later */
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s :mlme busy mostly scanning \n", __func__);
                osifp->is_vap_pending = 1;
                osifp->is_up = 0;
            } else if (retv == EBUSY) {
                /* resource manager is asynchronously bringing up the vap */
                /* Wait for the connection up */
                waitcnt = 0;
                while(!osifp->is_up && waitcnt < 3) {
                    schedule_timeout_interruptible(HZ) ;
                    waitcnt++;
                }

                if (!osifp->is_up) {
                   IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s : timed out waitinfor AP to come up \n", __func__);
                } else {
                    retv=0;
                }
            }
        }
        if (retv == 0 ) {
            osifp->is_up = 1;
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s :vap up \n", __func__);
            osifp->is_bss_started = 1; //fixme who clears this?
            wlan_mlme_connection_up(vap);
        }
        break;
    case IEEE80211_IOC_P2P_OPMODE:
        retv = osif_ioctl_switch_vap(dev , (enum ieee80211_opmode) value);
        break;

    default:
        retv = EOPNOTSUPP;
    }
    return retv;
}
#endif

static int
ieee80211_ioctl_setparam(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{

    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    wlan_if_t tmpvap;
    wlan_dev_t ic = wlan_vap_get_devhandle(vap);
    int *i = (int *) extra;
    int param = i[0];       /* parameter id is 1st */
    int value = i[1];       /* NB: most values are TYPE_INT */
    int retv = 0;
    int error = 0;


    debug_print_ioctl(dev->name, param, find_ieee_priv_ioctl_name(param, 1));

    switch (param)
    {
    case IEEE80211_PARAM_MAXSTA: //set max stations allowed
        if (value > IEEE80211_AID_DEF || value < 1) { // At least one station can associate with.
            return EINVAL;
        }
        if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
            u_int32_t old_max_aid = vap->iv_max_aid;
            if (value < vap->iv_sta_assoc) {
                printk("%d station associated with vap%d! refuse this request\n", 
                            vap->iv_sta_assoc, vap->iv_unit);
                return EINVAL;
            }

            vap->iv_max_aid = value + 1;
            /* The interface is up, we may need to reallocation bitmap(tim, aid) */
            if (IS_UP(dev)) {
                error = ieee80211_power_alloc_tim_bitmap(vap);
                if(!error)
                	error = wlan_node_alloc_aid_bitmap(vap, old_max_aid);
            }
            if(!error)
            	printk("Setting Max Stations:%d\n", value);
           	else {
          		printk("Setting Max Stations fail\n");
          		vap->iv_max_aid = old_max_aid;
          		return ENOMEM;
          	} 		 	
        }
        else {
            printk("This command only support on Host AP mode.\n");
        }
        break;
    case IEEE80211_PARAM_AUTO_ASSOC:
        wlan_set_param(vap, IEEE80211_AUTO_ASSOC, value);
        break;
    case IEEE80211_PARAM_VAP_COUNTRY_IE:
        wlan_set_param(vap, IEEE80211_FEATURE_COUNTRY_IE, value);
        break;
    case IEEE80211_PARAM_VAP_DOTH:
        wlan_set_param(vap, IEEE80211_FEATURE_DOTH, value);
        break;
    case IEEE80211_PARAM_HT40_INTOLERANT:
        wlan_set_param(vap, IEEE80211_HT40_INTOLERANT, value);
        break;

    case IEEE80211_PARAM_CHWIDTH:
        wlan_set_param(vap, IEEE80211_CHWIDTH, value);
        break;

    case IEEE80211_PARAM_CHEXTOFFSET:
        wlan_set_param(vap, IEEE80211_CHEXTOFFSET, value);
        break;
#ifdef ATH_SUPPORT_QUICK_KICKOUT
    case IEEE80211_PARAM_STA_QUICKKICKOUT:
        wlan_set_param(vap, IEEE80211_STA_QUICKKICKOUT, value);
        break;
#endif
    case IEEE80211_PARAM_CHSCANINIT:
        wlan_set_param(vap, IEEE80211_CHSCANINIT, value);
        break;

    case IEEE80211_PARAM_COEXT_DISABLE:
        osifp->is_up = 0;
        if (value)
        {
            ic->ic_flags |= IEEE80211_F_COEXT_DISABLE;
        }
        else
        {
            ic->ic_flags &= ~IEEE80211_F_COEXT_DISABLE;
        }
        TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
            struct net_device *tmpdev = ((osif_dev *)tmpvap->iv_ifp)->netdev;
            osifp = ath_netdev_priv(tmpdev);
            osifp->is_up = 0;
        }
        TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
            struct net_device *tmpdev = ((osif_dev *)tmpvap->iv_ifp)->netdev;
            if( IS_UP(tmpdev) )
                osif_vap_init(tmpdev, RESCAN);
        }
        break;

    case IEEE80211_PARAM_AUTHMODE:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set IEEE80211_IOC_AUTHMODE to %s\n",
        (value == IEEE80211_AUTH_WPA) ? "WPA" : (value == IEEE80211_AUTH_8021X) ? "802.1x" :
        (value == IEEE80211_AUTH_OPEN) ? "open" : (value == IEEE80211_AUTH_SHARED) ? "shared" :
        (value == IEEE80211_AUTH_AUTO) ? "auto" : "unknown" );
        osifp->authmode = value;

        if (value != IEEE80211_AUTH_WPA) {
            ieee80211_auth_mode modes[1];
            u_int nmodes=1;
            modes[0] = value;
            error = wlan_set_authmodes(vap,modes,nmodes);
            if (error == 0 ) {
                if ((value == IEEE80211_AUTH_OPEN) || (value == IEEE80211_AUTH_SHARED)) {
                    error = wlan_set_param(vap,IEEE80211_FEATURE_PRIVACY, 0);
                    osifp->uciphers[0] = osifp->mciphers[0] = IEEE80211_CIPHER_NONE;
                    osifp->u_count = osifp->m_count = 1;
                } else {
                    error = wlan_set_param(vap,IEEE80211_FEATURE_PRIVACY, 1);
                }
            }
        }
        /*
        * set_auth_mode will reset the ucast and mcast cipher set to defaults,
        * we will reset them from our cached values for non-open mode.
        */
        if ((value != IEEE80211_AUTH_OPEN) && (value != IEEE80211_AUTH_SHARED) 
                && (value != IEEE80211_AUTH_AUTO)) 
        {
            if (osifp->m_count)
                error = wlan_set_mcast_ciphers(vap,osifp->mciphers,osifp->m_count);
            if (osifp->u_count)
                error = wlan_set_ucast_ciphers(vap,osifp->uciphers,osifp->u_count);
        } 

#ifdef ATH_SUPPORT_P2P
        /* For P2P supplicant we do not want start connnection as soon as auth mode is set */
        /* The difference in behavior between non p2p supplicant and p2p supplicant need to be fixed */
        /* see EV 73753 for more details */
        if (error == 0 && (u_int8_t)osifp->os_opmode != IEEE80211_M_P2P_CLIENT && 
                osifp->os_opmode != IEEE80211_M_STA) {
            retv = ENETRESET;
        }
#else
        if (error == 0 )
  	{
	    if(osifp->os_opmode == IEEE80211_M_IBSS )
	    {
	        if(value != IEEE80211_AUTH_WPA)
		    retv = ENETRESET;
		 else
                    retv = error;
	    }
	    else
	    {
            retv = ENETRESET;
        }
	}
#endif /* ATH_SUPPORT_P2P */
        else {
            retv = error;
        }
        break;
    case IEEE80211_PARAM_MCASTKEYLEN:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set IEEE80211_IOC_MCASTKEYLEN to %d\n", value);
        if (!(0 < value && value < IEEE80211_KEYBUF_SIZE)) {
            error = EINVAL;
            break;
        }
        error = wlan_set_rsn_cipher_param(vap,IEEE80211_MCAST_CIPHER_LEN,value);
        retv = error;
        break;
    case IEEE80211_PARAM_UCASTCIPHERS:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set IEEE80211_IOC_UCASTCIPHERS (0x%x) %s %s %s %s %s %s\n",
                value, (value & 1<<IEEE80211_CIPHER_WEP) ? "WEP" : "",
                (value & 1<<IEEE80211_CIPHER_TKIP) ? "TKIP" : "",
                (value & 1<<IEEE80211_CIPHER_AES_OCB) ? "AES-OCB" : "",
                (value & 1<<IEEE80211_CIPHER_AES_CCM) ? "AES-CCMP" : "",
                (value & 1<<IEEE80211_CIPHER_CKIP) ? "CKIP" : "",
                (value & 1<<IEEE80211_CIPHER_NONE) ? "NONE" : "");
        {
            int count=0;
            if (value & 1<<IEEE80211_CIPHER_WEP)
                osifp->uciphers[count++] = IEEE80211_CIPHER_WEP;
            if (value & 1<<IEEE80211_CIPHER_TKIP)
                osifp->uciphers[count++] = IEEE80211_CIPHER_TKIP;
            if (value & 1<<IEEE80211_CIPHER_AES_CCM)
                osifp->uciphers[count++] = IEEE80211_CIPHER_AES_CCM;
            if (value & 1<<IEEE80211_CIPHER_CKIP)
                osifp->uciphers[count++] = IEEE80211_CIPHER_CKIP;
            if (value & 1<<IEEE80211_CIPHER_NONE)
                osifp->uciphers[count++] = IEEE80211_CIPHER_NONE;
            error = wlan_set_ucast_ciphers(vap,osifp->uciphers,count);
            if (error == 0) {
                error = ENETRESET;
            }
            else {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s Warning: wlan_set_ucast_cipher failed. cache the ucast cipher\n", __func__);
                error=0;
            }
            osifp->u_count=count;


        }
        retv = error;
        break;
    case IEEE80211_PARAM_UCASTCIPHER:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set IEEE80211_IOC_UCASTCIPHER to %s\n",
                (value == IEEE80211_CIPHER_WEP) ? "WEP" :
                (value == IEEE80211_CIPHER_TKIP) ? "TKIP" :
                (value == IEEE80211_CIPHER_AES_OCB) ? "AES OCB" :
                (value == IEEE80211_CIPHER_AES_CCM) ? "AES CCM" :
                (value == IEEE80211_CIPHER_CKIP) ? "CKIP" :
                (value == IEEE80211_CIPHER_NONE) ? "NONE" : "unknown");
        {
            ieee80211_cipher_type ctypes[1];
            ctypes[0] = (ieee80211_cipher_type) value;
            error = wlan_set_ucast_ciphers(vap,ctypes,1);
            /* save the ucast cipher info */
            osifp->uciphers[0] = ctypes[0];
            osifp->u_count=1;
            if (error == 0) {
                retv = ENETRESET;
            }
            else {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s Warning: wlan_set_ucast_cipher failed. cache the ucast cipher\n", __func__);
                error=0;
            }
        }
        retv = error;
        break;
    case IEEE80211_IOC_MCASTCIPHER:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set IEEE80211_IOC_MCASTCIPHER to %s\n",
                        (value == IEEE80211_CIPHER_WEP) ? "WEP" :
                        (value == IEEE80211_CIPHER_TKIP) ? "TKIP" :
                        (value == IEEE80211_CIPHER_AES_OCB) ? "AES OCB" :
                        (value == IEEE80211_CIPHER_AES_CCM) ? "AES CCM" :
                        (value == IEEE80211_CIPHER_CKIP) ? "CKIP" :
                        (value == IEEE80211_CIPHER_NONE) ? "NONE" : "unknown");
        {
            ieee80211_cipher_type ctypes[1];
            ctypes[0] = (ieee80211_cipher_type) value;
            error = wlan_set_mcast_ciphers(vap, ctypes, 1);
            /* save the mcast cipher info */
            osifp->mciphers[0] = ctypes[0];
            osifp->m_count=1;
            if (error) {
                /*
                * ignore the error for now.
                * both the ucast and mcast ciphers
                * are set again when auth mode is set.
                */
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"%s", "Warning: wlan_set_mcast_cipher failed. cache the mcast cipher  \n");
                error=0;
            }
        }
        retv = error;
        break;
    case IEEE80211_PARAM_UCASTKEYLEN:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set IEEE80211_IOC_UCASTKEYLEN to %d\n", value);
        if (!(0 < value && value < IEEE80211_KEYBUF_SIZE)) {
            error = EINVAL;
            break;
        }
        error = wlan_set_rsn_cipher_param(vap,IEEE80211_UCAST_CIPHER_LEN,value);
        retv = error;
        break;
    case IEEE80211_PARAM_PRIVACY:
        retv = wlan_set_param(vap,IEEE80211_FEATURE_PRIVACY,value);
        break;
    case IEEE80211_PARAM_COUNTERMEASURES:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_COUNTER_MEASURES, value);
        break;
    case IEEE80211_PARAM_HIDESSID:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_HIDE_SSID, value);
        if (retv == EOK) {
            retv = ENETRESET;
        }
        break;
    case IEEE80211_PARAM_APBRIDGE:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_APBRIDGE, value);
        break;
    case IEEE80211_PARAM_KEYMGTALGS:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set IEEE80211_IOC_KEYMGTALGS (0x%x) %s %s\n",
        value, (value & WPA_ASE_8021X_UNSPEC) ? "802.1x Unspecified" : "",
        (value & WPA_ASE_8021X_PSK) ? "802.1x PSK" : "");
        retv = wlan_set_rsn_cipher_param(vap,IEEE80211_KEYMGT_ALGS,value);
        break;
    case IEEE80211_PARAM_RSNCAPS:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set IEEE80211_IOC_RSNCAPS to 0x%x\n", value);
        error = wlan_set_rsn_cipher_param(vap,IEEE80211_RSN_CAPS,value);
        retv = error;
        break;
    case IEEE80211_PARAM_WPA:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set IEEE80211_IOC_WPA to %s\n",
        (value == 1) ? "WPA" : (value == 2) ? "RSN" :
        (value == 3) ? "WPA and RSN" : (value == 0)? "off" : "unknown");
        if (value > 3) {
            error = EINVAL;
            break;
        } else {
            ieee80211_auth_mode modes[2];
            u_int nmodes=1;
            if (osifp->os_opmode == IEEE80211_M_STA ||
                (u_int8_t)osifp->os_opmode == IEEE80211_M_P2P_CLIENT) {
                error = wlan_set_rsn_cipher_param(vap,IEEE80211_KEYMGT_ALGS,WPA_ASE_8021X_PSK);
                if (!error) {
                    if ((value == 3) || (value == 2)) { /* Mixed mode or WPA2 */
                        modes[0] = IEEE80211_AUTH_RSNA;
                    } else { /* WPA mode */
                        modes[0] = IEEE80211_AUTH_WPA;
                    }
                }
                /* set supported cipher to TKIP and CCM
                * to allow WPA-AES, WPA2-TKIP: and MIXED mode
                */
                osifp->u_count = 2;
                osifp->uciphers[0] = IEEE80211_CIPHER_TKIP;
                osifp->uciphers[1] = IEEE80211_CIPHER_AES_CCM;
                osifp->m_count = 2;
                osifp->mciphers[0] = IEEE80211_CIPHER_TKIP;
                osifp->mciphers[1] = IEEE80211_CIPHER_AES_CCM;
            }
            else {
                if (value == 3) {
                    nmodes = 2;
                    modes[0] = IEEE80211_AUTH_WPA;
                    modes[1] = IEEE80211_AUTH_RSNA;
                } else if (value == 2) {
                    modes[0] = IEEE80211_AUTH_RSNA;
                } else {
                    modes[0] = IEEE80211_AUTH_WPA;
                }
            }
            error = wlan_set_authmodes(vap,modes,nmodes);
            /*
            * set_auth_mode will reset the ucast and mcast cipher set to defaults,
            * we will reset them from our cached values.
            */
            if (osifp->m_count)
                error = wlan_set_mcast_ciphers(vap,osifp->mciphers,osifp->m_count);
            if (osifp->u_count)
                error = wlan_set_ucast_ciphers(vap,osifp->uciphers,osifp->u_count);
        }
        retv = error;
        break;

    case IEEE80211_PARAM_CLR_APPOPT_IE:
        retv = wlan_set_clr_appopt_ie(vap);
        break;

    /*
    ** The setting of the manual rate table parameters and the retries are moved
    ** to here, since they really don't belong in iwconfig
    */

    case IEEE80211_PARAM_11N_RATE:
        retv = wlan_set_param(vap, IEEE80211_FIXED_RATE, value);
        break;

    case IEEE80211_PARAM_11N_RETRIES:
        if (value)
            retv = wlan_set_param(vap, IEEE80211_FIXED_RETRIES, value);
        break;
    case IEEE80211_PARAM_SHORT_GI :
        retv = wlan_set_param(vap, IEEE80211_SHORT_GI, value);
        if (retv == 0)
        retv = ENETRESET;
        break;
    case IEEE80211_PARAM_DBG_LVL:
        retv = wlan_set_param(vap, IEEE80211_MSG_FLAGS, value);
        break;
#if UMAC_SUPPORT_IBSS
    case IEEE80211_PARAM_IBSS_CREATE_DISABLE:
        if (osifp->os_opmode != IEEE80211_M_IBSS) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                              "Can not be used in mode %d\n", osifp->os_opmode);
            return -EINVAL;
        }
        osifp->disable_ibss_create = !!value;
        break;
#endif
	case IEEE80211_PARAM_WEATHER_RADAR_CHANNEL:
        retv = wlan_set_param(vap, IEEE80211_WEATHER_RADAR, value);
		break;
	case IEEE80211_PARAM_WEP_KEYCACHE:
        retv = wlan_set_param(vap, IEEE80211_WEP_KEYCACHE, value);
		break;
    case IEEE80211_PARAM_BEACON_INTERVAL:
        if (value > IEEE80211_BINTVAL_IWMAX || value < IEEE80211_BINTVAL_IWMIN) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                              "BEACON_INTERVAL should be within %d to %d\n",
                              IEEE80211_BINTVAL_IWMIN,
                              IEEE80211_BINTVAL_IWMAX);
            return -EINVAL;
        }
        retv = wlan_set_param(vap, IEEE80211_BEACON_INTVAL, value);
        if (retv == EOK) {
            //retv = ENETRESET;
            wlan_if_t tmpvap;

            TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
                struct net_device *tmpdev = ((osif_dev *)tmpvap->iv_ifp)->netdev;
                retv = IS_UP(tmpdev) ? -osif_vap_init(tmpdev, RESCAN) : 0;
            }
        }
        break;
#if ATH_SUPPORT_AP_WDS_COMBO
    case IEEE80211_PARAM_NO_BEACON:
        retv = wlan_set_param(vap, IEEE80211_NO_BEACON, value);
        break;
#endif
    case IEEE80211_PARAM_PUREG:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_PUREG, value);
        /* NB: reset only if we're operating on an 11g channel */
        if (retv == 0) {
            wlan_chan_t chan = wlan_get_bss_channel(vap);
            if (chan != IEEE80211_CHAN_ANYC &&
                (IEEE80211_IS_CHAN_ANYG(chan) ||
                IEEE80211_IS_CHAN_11NG(chan)))
                retv = ENETRESET;
        }
        break;
    case IEEE80211_PARAM_PUREN:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_PURE11N, value);
        /* Reset only if we're operating on a 11ng channel */
        if (retv == 0) {
            wlan_chan_t chan = wlan_get_bss_channel(vap);
            if (chan != IEEE80211_CHAN_ANYC &&
            IEEE80211_IS_CHAN_11NG(chan))
            retv = ENETRESET;
        }
        break;
    case IEEE80211_PARAM_WDS:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_WDS, value);
        if (retv == 0) {
            /* WAR: set the auto assoc feature also for WDS */
            if (value) {
                wlan_set_param(vap, IEEE80211_AUTO_ASSOC, 1);
            }
        }
        break;
    case IEEE80211_PARAM_VAP_IND:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_VAP_IND, value);
        break;
    case IEEE80211_PARAM_BLOCKDFSCHAN:
        retv = wlan_set_device_param(ic, IEEE80211_DEVICE_BLKDFSCHAN, value);
        if (retv == EOK) {
            retv = ENETRESET;
        }
        break;
#if ATH_SUPPORT_WAPI
    case IEEE80211_PARAM_SETWAPI:
        retv = wlan_setup_wapi(vap, value);
        if (retv == 0) {
            retv = ENETRESET;
        }
        break;
    case IEEE80211_PARAM_WAPIREKEY_USK:
        retv = wlan_set_wapirekey_unicast(vap, value);
        break;
    case IEEE80211_PARAM_WAPIREKEY_MSK:
        retv = wlan_set_wapirekey_multicast(vap, value);
        break;		
    case IEEE80211_PARAM_WAPIREKEY_UPDATE:
        retv = wlan_set_wapirekey_update(vap, (unsigned char*)&extra[4]);
        break;
#endif

    case IEEE80211_IOCTL_GREEN_AP_PS_ENABLE:
        wlan_set_device_param(ic, IEEE80211_DEVICE_GREEN_AP_PS_ENABLE, value?1:0);
        retv = 0;
        break;

    case IEEE80211_IOCTL_GREEN_AP_PS_TIMEOUT:
        wlan_set_device_param(ic, IEEE80211_DEVICE_GREEN_AP_PS_TIMEOUT, value > 20 ? value : 20);
        retv = 0;
        break;

    case IEEE80211_IOCTL_GREEN_AP_PS_ON_TIME:
        wlan_set_device_param(ic, IEEE80211_DEVICE_GREEN_AP_PS_ON_TIME, value >= 0 ? value : 0);
        retv = 0;
        break;

#ifdef ATH_WPS_IE
    case IEEE80211_PARAM_WPS:
        retv = wlan_set_param(vap, IEEE80211_WPS_MODE, value);
        break;
#endif
#ifdef ATH_EXT_AP
    case IEEE80211_PARAM_EXTAP:
        if (value) {
            if (value == 3 /* dbg */) {
                extern void mi_tbl_dump(void *);
                mi_tbl_dump(vap->iv_ic->ic_miroot);
                break;
            }
            if (value == 2 /* dbg */) {
                extern void mi_tbl_purge(void *);
                IEEE80211_VAP_EXT_AP_DISABLE(vap);
                mi_tbl_purge(&vap->iv_ic->ic_miroot);
            }
            IEEE80211_VAP_EXT_AP_ENABLE(vap);
            /* Set the auto assoc feature for Extender Station */
            wlan_set_param(vap, IEEE80211_AUTO_ASSOC, 1);
        } else {
            IEEE80211_VAP_EXT_AP_DISABLE(vap);
        }
        break;
#endif
    case IEEE80211_PARAM_STA_FORWARD:
    retv = wlan_set_param(vap, IEEE80211_FEATURE_STAFWD, value);
    break;

    case IEEE80211_PARAM_CWM_EXTPROTMODE:
        if (value >= 0) {
            retv = wlan_set_device_param(ic,IEEE80211_DEVICE_CWM_EXTPROTMODE, value);
            if (retv == EOK) {
                retv = ENETRESET;
            }
        } else {
            retv =  EINVAL;
        }
        break;
    case IEEE80211_PARAM_CWM_EXTPROTSPACING:
        if (value >= 0) {
            retv = wlan_set_device_param(ic,IEEE80211_DEVICE_CWM_EXTPROTSPACING, value);
            if (retv == EOK) {
                retv = ENETRESET;
            }
        }
        else {
            retv =  EINVAL;
        }
        break;
    case IEEE80211_PARAM_CWM_ENABLE:
        if (value >= 0) {
            retv = wlan_set_device_param(ic,IEEE80211_DEVICE_CWM_ENABLE, value);
            if (retv == EOK) {
                retv = ENETRESET;
            }
        } else {
            retv =  EINVAL;
        }
        break;
    case IEEE80211_PARAM_CWM_EXTBUSYTHRESHOLD:
        if (value >=0 && value <=100) {
            retv = wlan_set_device_param(ic,IEEE80211_DEVICE_CWM_EXTBUSYTHRESHOLD, value);
            if (retv == EOK) {
                retv = ENETRESET;
            }
        } else {
            retv =  EINVAL;
        }
        break;
    case IEEE80211_PARAM_DOTH:
        retv = wlan_set_device_param(ic, IEEE80211_DEVICE_DOTH, value);
        if (retv == EOK) {
            retv = ENETRESET;   /* XXX: need something this drastic? */
        }
        break;
    case IEEE80211_PARAM_SETADDBAOPER:
        if (value > 1 || value < 0) {
            return EINVAL;
        }

        retv = wlan_set_device_param(ic, IEEE80211_DEVICE_ADDBA_MODE, value);
        break;
    case IEEE80211_PARAM_WMM:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_WMM, value);
        wlan_set_param(vap, IEEE80211_FEATURE_AMPDU, value);
        if (retv == EOK) {
            retv = ENETRESET;
        }
        break;
    case IEEE80211_PARAM_PROTMODE:
        retv = wlan_set_device_param(ic, IEEE80211_DEVICE_PROTECTION_MODE, value);
        /* NB: if not operating in 11g this can wait */
        if (retv == EOK) {
            wlan_chan_t chan = wlan_get_bss_channel(vap);
            if (chan != IEEE80211_CHAN_ANYC &&
                (IEEE80211_IS_CHAN_ANYG(chan) ||
                IEEE80211_IS_CHAN_11NG(chan))) {
                retv = ENETRESET;
            }
        }
        break;
    case IEEE80211_PARAM_ROAMING:
        if (!(IEEE80211_ROAMING_DEVICE <= value &&
            value <= IEEE80211_ROAMING_MANUAL))
            return -EINVAL;
        ic->ic_roaming = value;
        if(value == IEEE80211_ROAMING_MANUAL)
            IEEE80211_VAP_AUTOASSOC_DISABLE(vap);
        else
            IEEE80211_VAP_AUTOASSOC_ENABLE(vap);
        break;
    case IEEE80211_PARAM_DROPUNENCRYPTED:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_DROP_UNENC, value);
        break;
    case IEEE80211_PARAM_DRIVER_CAPS:
        retv = wlan_set_param(vap, IEEE80211_DRIVER_CAPS, value); /* NB: for testing */
        break;
/*
* Support for Mcast Enhancement
*/
#if ATH_SUPPORT_IQUE
    case IEEE80211_PARAM_ME:
        wlan_set_param(vap, IEEE80211_ME, value);
        break;
    case IEEE80211_PARAM_MEDEBUG:
        wlan_set_param(vap, IEEE80211_MEDEBUG, value);
        break;
    case IEEE80211_PARAM_ME_SNOOPLENGTH:
        wlan_set_param(vap, IEEE80211_ME_SNOOPLENGTH, value);
        break;
    case IEEE80211_PARAM_ME_TIMER:
        wlan_set_param(vap, IEEE80211_ME_TIMER, value);
        break;
    case IEEE80211_PARAM_ME_TIMEOUT:
        wlan_set_param(vap, IEEE80211_ME_TIMEOUT, value);
        break;
    case IEEE80211_PARAM_HBR_TIMER:
        wlan_set_param(vap, IEEE80211_HBR_TIMER, value);
        break;
    case IEEE80211_PARAM_ME_DROPMCAST:
        wlan_set_param(vap, IEEE80211_ME_DROPMCAST, value);
        break;
    case IEEE80211_PARAM_ME_CLEARDENY:
        wlan_set_param(vap, IEEE80211_ME_CLEARDENY, value);
        break;
#endif

#if  ATH_SUPPORT_AOW
    case IEEE80211_PARAM_SWRETRIES:
        wlan_set_aow_param(vap, IEEE80211_AOW_SWRETRIES, value);
        break;
    case IEEE80211_PARAM_AOW_LATENCY:
        wlan_set_aow_param(vap, IEEE80211_AOW_LATENCY, value);
        break;
    case IEEE80211_PARAM_AOW_PLAY_LOCAL:
        wlan_set_aow_param(vap, IEEE80211_AOW_PLAY_LOCAL, value);
        break;
    case IEEE80211_PARAM_AOW_CLEAR_AUDIO_CHANNELS:
        wlan_set_aow_param(vap, IEEE80211_AOW_CLEAR_AUDIO_CHANNELS, value);
        break;        
    case IEEE80211_PARAM_AOW_STATS:
        wlan_set_aow_param(vap, IEEE80211_AOW_STATS, value);
        break;
    case IEEE80211_PARAM_AOW_ESTATS:
        wlan_set_aow_param(vap, IEEE80211_AOW_ESTATS, value);
        break;
    case IEEE80211_PARAM_AOW_INTERLEAVE:
        wlan_set_aow_param(vap, IEEE80211_AOW_INTERLEAVE, value);
        break;     
   case IEEE80211_PARAM_AOW_ER:
        wlan_set_aow_param(vap, IEEE80211_AOW_ER, value);
        break;
   case IEEE80211_PARAM_AOW_EC:
        wlan_set_aow_param(vap, IEEE80211_AOW_EC, value);
        break;
   case IEEE80211_PARAM_AOW_EC_FMAP:
        wlan_set_aow_param(vap, IEEE80211_AOW_EC_FMAP, value);
        break;
   case IEEE80211_PARAM_AOW_ES:
        wlan_set_aow_param(vap, IEEE80211_AOW_ES, value);
        break;
   case IEEE80211_PARAM_AOW_ESS:
        wlan_set_aow_param(vap, IEEE80211_AOW_ESS, value);
        break;
   case IEEE80211_PARAM_AOW_ESS_COUNT:
        wlan_set_aow_param(vap, IEEE80211_AOW_ESS_COUNT, value);
        break;
   case IEEE80211_PARAM_AOW_ENABLE_CAPTURE:
         wlan_set_aow_param(vap, IEEE80211_AOW_ENABLE_CAPTURE, value);
         break;
   case IEEE80211_PARAM_AOW_FORCE_INPUT:
        wlan_set_aow_param(vap, IEEE80211_AOW_FORCE_INPUT, value);
        break;
    case IEEE80211_PARAM_AOW_PRINT_CAPTURE:
        wlan_set_aow_param(vap, IEEE80211_AOW_PRINT_CAPTURE, value);
        break;
#endif  /* ATH_SUPPORT_AOW */

#if UMAC_SUPPORT_RPTPLACEMENT
        case IEEE80211_PARAM_CUSTPROTO_ENABLE:
            ieee80211_rptplacement_set_param(vap, IEEE80211_RPT_CUSTPROTO_ENABLE, value);
            break;

        case IEEE80211_PARAM_GPUTCALC_ENABLE:
            ieee80211_rptplacement_set_param(vap, IEEE80211_RPT_GPUTCALC_ENABLE, value);
            ieee80211_rptplacement_gput_est_init(vap, 0);
        break;

        case IEEE80211_PARAM_DEVUP:
            ieee80211_rptplacement_set_param(vap, IEEE80211_RPT_DEVUP, value);
            break;

        case IEEE80211_PARAM_MACDEV:
            ieee80211_rptplacement_set_param(vap, IEEE80211_RPT_MACDEV, value);
            ieee80211_rptplacement_get_mac_addr(vap, value);
            break;

        case IEEE80211_PARAM_MACADDR1:
            ieee80211_rptplacement_set_param(vap, IEEE80211_RPT_MACADDR1, value);
            break;

        case IEEE80211_PARAM_MACADDR2:
            ieee80211_rptplacement_set_param(vap, IEEE80211_RPT_MACADDR2, value);
            break;

        case IEEE80211_PARAM_GPUTMODE:
            ieee80211_rptplacement_set_param(vap, IEEE80211_RPT_GPUTMODE, value);
            ieee80211_rptplacement_get_gputmode(ic, value);
            break;

        case IEEE80211_PARAM_TXPROTOMSG:
            ieee80211_rptplacement_set_param(vap, IEEE80211_RPT_TXPROTOMSG, value);
            ieee80211_rptplacement_tx_proto_msg(vap, value);
            break;

        case IEEE80211_PARAM_RXPROTOMSG:
            ieee80211_rptplacement_set_param(vap, IEEE80211_RPT_RXPROTOMSG, value);
            break;

        case IEEE80211_PARAM_STATUS:
            ieee80211_rptplacement_set_param(vap, IEEE80211_RPT_STATUS, value);
            ieee80211_rptplacement_get_status(ic, value);
            break;

        case IEEE80211_PARAM_ASSOC:
            ieee80211_rptplacement_set_param(vap, IEEE80211_RPT_ASSOC, value);
            ieee80211_rptplacement_get_rptassoc(ic, value);
            break;

        case IEEE80211_PARAM_NUMSTAS:
            ieee80211_rptplacement_set_param(vap, IEEE80211_RPT_NUMSTAS, value);
            ieee80211_rptplacement_get_numstas(ic, value);
            break;

        case IEEE80211_PARAM_STA1ROUTE:
            ieee80211_rptplacement_set_param(vap, IEEE80211_RPT_STA1ROUTE, value);
            ieee80211_rptplacement_get_sta1route(ic, value);
            break;

        case IEEE80211_PARAM_STA2ROUTE:
            ieee80211_rptplacement_set_param(vap, IEEE80211_RPT_STA2ROUTE, value);
            ieee80211_rptplacement_get_sta2route(ic, value);
            break;

        case IEEE80211_PARAM_STA3ROUTE:
            ieee80211_rptplacement_set_param(vap, IEEE80211_RPT_STA3ROUTE, value);
            ieee80211_rptplacement_get_sta3route(ic, value);
            break;

        case IEEE80211_PARAM_STA4ROUTE:
            ieee80211_rptplacement_set_param(vap, IEEE80211_RPT_STA4ROUTE, value);
            ieee80211_rptplacement_get_sta4route(ic, value);
#endif

#if UMAC_SUPPORT_TDLS
        case IEEE80211_PARAM_TDLS_MACADDR1:
            wlan_set_param(vap, IEEE80211_TDLS_MACADDR1, value);
            break;

        case IEEE80211_PARAM_TDLS_MACADDR2:
            wlan_set_param(vap, IEEE80211_TDLS_MACADDR2, value);
            break;

        case IEEE80211_PARAM_TDLS_ACTION:
            wlan_set_param(vap, IEEE80211_TDLS_ACTION, value);
            break;
#endif

    case IEEE80211_PARAM_DTIM_PERIOD:
        if (!(osifp->os_opmode == IEEE80211_M_HOSTAP ||
            osifp->os_opmode == IEEE80211_M_IBSS)) {
            return -EINVAL;
        }
        if (value > IEEE80211_DTIM_MAX ||
            value < IEEE80211_DTIM_MIN) {
             
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                              "DTIM_PERIOD should be within %d to %d\n",
                              IEEE80211_DTIM_MIN,
                              IEEE80211_DTIM_MAX);
            return -EINVAL;
        }
        retv = wlan_set_param(vap, IEEE80211_DTIM_INTVAL, value);
        if (retv == EOK) {
            retv = ENETRESET;
        }

        break;
    case IEEE80211_PARAM_MACCMD:
        wlan_set_acl_policy(vap, value);
            break;
    case IEEE80211_PARAM_MCAST_RATE:
        /*
        * value is rate in units of Kbps
        * min: 1Mbps max: 300Mbps
        */
        if (value < 1000 || value > 300000)
            retv = EINVAL;
        else {
        wlan_set_param(vap, IEEE80211_MCAST_RATE, value);
        }
        break;
    case IEEE80211_PARAM_CCMPSW_ENCDEC:
        if (value) {
            IEEE80211_VAP_CCMPSW_ENCDEC_ENABLE(vap);
        } else {
            IEEE80211_VAP_CCMPSW_ENCDEC_DISABLE(vap);
        }
        break;

    case IEEE80211_PARAM_NETWORK_SLEEP:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s set IEEE80211_IOC_POWERSAVE parameter %d \n",
                          __func__,value );
        do {
            ieee80211_pwrsave_mode ps_mode = IEEE80211_PWRSAVE_NONE;
            switch(value) {
            case 0:
                ps_mode = IEEE80211_PWRSAVE_NONE;
                break;
            case 1:
                ps_mode = IEEE80211_PWRSAVE_LOW;
                break;
            case 2:
                ps_mode = IEEE80211_PWRSAVE_NORMAL;
                break;
            case 3:
                ps_mode = IEEE80211_PWRSAVE_MAXIMUM;
                break;
            }
            error= wlan_set_powersave(vap,ps_mode);
        } while(0);
        break;

#ifdef ATHEROS_LINUX_PERIODIC_SCAN
    case IEEE80211_PARAM_PERIODIC_SCAN:
        if (wlan_vap_get_opmode(vap) == IEEE80211_M_STA) {
            if (osifp->os_periodic_scan_period != value){
                if (value && (value < OSIF_PERIODICSCAN_MIN_PERIOD))
                    osifp->os_periodic_scan_period = OSIF_PERIODICSCAN_MIN_PERIOD;
                else 
                    osifp->os_periodic_scan_period = value;

                retv = ENETRESET;                
            }
        }
        break;
#endif        

#if ATH_SW_WOW
    case IEEE80211_PARAM_SW_WOW:
        if (wlan_vap_get_opmode(vap) == IEEE80211_M_STA) {
            retv = wlan_set_wow(vap, value);
        }
        break;
#endif
        
    case IEEE80211_PARAM_UAPSDINFO:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_UAPSD, value);
        if (retv == EOK) {
            retv = ENETRESET;
        }
	break ;

#ifdef ATH_SUPPORT_P2P
    /* WFD Sigma use these two to do reset and some cases. */
    case IEEE80211_PARAM_SLEEP:
        /* XXX: Forced sleep for testing. Does not actually place the
         *      HW in sleep mode yet. this only makes sense for STAs.
         */
        /* enable/disable force  sleep */
        wlan_pwrsave_force_sleep(vap,value);
        break;
    case IEEE80211_PARAM_PSPOLL:
        /* Force a PS-POLL for testing. */
        wlan_set_param(vap, IEEE80211_FEATURE_PSPOLL, value);
        break;
#endif  

     case IEEE80211_PARAM_COUNTRY_IE:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_IC_COUNTRY_IE, value);
        if (retv == EOK) {
            retv = ENETRESET;
        }
        break;
#if ATH_RXBUF_RECYCLE
    case IEEE80211_PARAM_RXBUF_LIFETIME:
        ic->ic_osdev->rxbuf_lifetime = value;
        break;
#endif
#ifdef __CARRIER_PLATFORM__
    case IEEE80211_PARAM_PLTFRM_PRIVATE:
         retv = wlan_pltfrm_set_param(vap, value);
         if ( retv == EOK) {
             retv = ENETRESET;
         }
         break;
#endif 
#if UMAC_SUPPORT_VI_DBG
        case IEEE80211_PARAM_DBG_CFG:
            ieee80211_vi_dbg_set_param(vap, IEEE80211_VI_DBG_CFG, value);
            break;
        case IEEE80211_PARAM_DBG_NUM_STREAMS:
            ieee80211_vi_dbg_set_param(vap, IEEE80211_VI_DBG_NUM_STREAMS, value);
            break;
        case IEEE80211_PARAM_STREAM_NUM:
            ieee80211_vi_dbg_set_param(vap, IEEE80211_VI_STREAM_NUM, value);
	        break;
        case IEEE80211_PARAM_DBG_NUM_MARKERS:
            ieee80211_vi_dbg_set_param(vap, IEEE80211_VI_DBG_NUM_MARKERS, value);
            break;
    	case IEEE80211_PARAM_MARKER_NUM:
            ieee80211_vi_dbg_set_param(vap, IEEE80211_VI_MARKER_NUM, value);
	        break;
        case IEEE80211_PARAM_MARKER_OFFSET_SIZE:
            ieee80211_vi_dbg_set_param(vap, IEEE80211_VI_MARKER_OFFSET_SIZE, value);  
            break;
        case IEEE80211_PARAM_MARKER_MATCH:
            ieee80211_vi_dbg_set_param(vap, IEEE80211_VI_MARKER_MATCH, value); 
	        ieee80211_vi_dbg_get_marker(vap);
            break;
        case IEEE80211_PARAM_RXSEQ_OFFSET_SIZE:
            ieee80211_vi_dbg_set_param(vap, IEEE80211_VI_RXSEQ_OFFSET_SIZE, value);  
            break;
        case IEEE80211_PARAM_RX_SEQ_RSHIFT:
            ieee80211_vi_dbg_set_param(vap, IEEE80211_VI_RX_SEQ_RSHIFT, value);
            break;
        case IEEE80211_PARAM_RX_SEQ_MAX:
            ieee80211_vi_dbg_set_param(vap, IEEE80211_VI_RX_SEQ_MAX, value);
            break;
        case IEEE80211_PARAM_RX_SEQ_DROP:
            ieee80211_vi_dbg_set_param(vap, IEEE80211_VI_RX_SEQ_DROP, value);
            break;
        case IEEE80211_PARAM_TIME_OFFSET_SIZE:
            ieee80211_vi_dbg_set_param(vap, IEEE80211_VI_TIME_OFFSET_SIZE, value);  
            break;
        case IEEE80211_PARAM_RESTART:
            ieee80211_vi_dbg_set_param(vap, IEEE80211_VI_RESTART, value);
            break;
        case IEEE80211_PARAM_RXDROP_STATUS:
            ieee80211_vi_dbg_set_param(vap, IEEE80211_VI_RXDROP_STATUS, value);
            break;
#endif            

    case IEEE80211_IOC_WPS_MODE:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                        "set IEEE80211_IOC_WPS_MODE to 0x%x\n", value);
        retv = wlan_set_param(vap, IEEE80211_WPS_MODE, value);
        break;
    case IEEE80211_IOC_SCAN_FLUSH:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set %s\n",
                        "IEEE80211_IOC_SCAN_FLUSH");
        wlan_scan_table_flush(vap);
        retv = 0; /* success */
        break;
#ifdef ATH_SUPPORT_TxBF
    case IEEE80211_PARAM_TXBF_AUTO_CVUPDATE:
        wlan_set_param(vap, IEEE80211_TXBF_AUTO_CVUPDATE, value);
        ic->ic_set_config(vap);
        break;
    case IEEE80211_PARAM_TXBF_CVUPDATE_PER:
        wlan_set_param(vap, IEEE80211_TXBF_CVUPDATE_PER, value);
        ic->ic_set_config(vap);
        break;
#endif             
    case IEEE80211_PARAM_SCAN_BAND:
        osifp->os_scan_band = value;
        retv = 0;
        break;             
    default:
        retv =
#if ATHEROS_LINUX_P2P_DRIVER
        ieee80211_ioctl_setp2p(dev, info, w, extra);
#else
        EOPNOTSUPP;
#endif
        if (retv) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s parameter 0x%x is "
                            "not supported retv=%d\n", __func__, param, retv);
        }
        break;
    case IEEE80211_PARAM_AMPDU:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_AMPDU, value);
        if (retv == EOK) {
            retv = ENETRESET;
        }
        
#if ATH_SUPPORT_IBSS_HT
        /*
         * config ic adhoc AMPDU capability
         */
        if (vap->iv_opmode == IEEE80211_M_IBSS) {
        
            wlan_dev_t ic = wlan_vap_get_devhandle(vap);    
            
            if (value && 
               (ieee80211_ic_ht20Adhoc_is_set(ic) || ieee80211_ic_ht40Adhoc_is_set(ic))) {
                wlan_set_device_param(ic, IEEE80211_DEVICE_HTADHOCAGGR, 1);
                printk("%s IEEE80211_PARAM_AMPDU = %d and HTADHOC enable\n", __func__, value);
            } else {
                wlan_set_device_param(ic, IEEE80211_DEVICE_HTADHOCAGGR, 0);
                printk("%s IEEE80211_PARAM_AMPDU = %d and HTADHOC disable\n", __func__, value);
            }
        }
        
        // don't reset
        retv = EOK;
#endif /* end of #if ATH_SUPPORT_IBSS_HT */
        
        break;
#if ATH_SUPPORT_WPA_SUPPLICANT_CHECK_TIME
 	case IEEE80211_PARAM_REJOINT_ATTEMP_TIME:
        retv = wlan_set_param(vap,IEEE80211_REJOINT_ATTEMP_TIME,value);
		break;
#endif    
    case IEEE80211_PARAM_SHORTPREAMBLE:
        retv = wlan_set_param(vap, IEEE80211_SHORT_PREAMBLE, value);
        if (retv == EOK) {
            retv = ENETRESET;
        }
       break;

    case IEEE80211_PARAM_CHANBW:
        switch (value)
        {
        case 0:
            ic->ic_chanbwflag = 0;
            break;
        case 1:
            ic->ic_chanbwflag = IEEE80211_CHAN_HALF;
            break;
        case 2:
            ic->ic_chanbwflag = IEEE80211_CHAN_QUARTER;
            break;
        default:
            retv = EINVAL;
            break;
        }
        break;

    case IEEE80211_PARAM_INACT:
        wlan_set_param(vap, IEEE80211_CFG_INACT, value);
        break;
    case IEEE80211_PARAM_INACT_AUTH:
        wlan_set_param(vap, IEEE80211_CFG_INACT_AUTH, value);
        break;
    case IEEE80211_PARAM_INACT_INIT:
        wlan_set_param(vap, IEEE80211_CFG_INACT_INIT, value);
        break;
    case IEEE80211_PARAM_PWRTARGET:
		wlan_set_param(vap, IEEE80211_PWRTARGET, value);
        break;
    case IEEE80211_PARAM_WDS_AUTODETECT:
        wlan_set_param(vap, IEEE80211_WDS_AUTODETECT, value);
        break;
    case IEEE80211_PARAM_WEP_TKIP_HT:
		wlan_set_param(vap, IEEE80211_WEP_TKIP_HT, value);
        retv = ENETRESET;
        break;
    case IEEE80211_PARAM_IGNORE_11DBEACON:
        wlan_set_param(vap, IEEE80211_IGNORE_11DBEACON, value);
        break;
    case IEEE80211_PARAM_MFP_TEST:
        wlan_set_param(vap, IEEE80211_FEATURE_MFP_TEST, value);
        break;
#if UMAC_SUPPORT_TDLS
    case IEEE80211_PARAM_TDLS_ENABLE:
        if (value) {
            printk("Enabling TDLS: ");
            vap->iv_ath_cap |= IEEE80211_ATHC_TDLS;
        } else {
            printk("Disabling TDLS: ");
            vap->iv_ath_cap &= ~IEEE80211_ATHC_TDLS;
        }
        printf("%x\n", vap->iv_ath_cap & IEEE80211_ATHC_TDLS);
        break;
    case IEEE80211_PARAM_SET_TDLS_RMAC: {
        u_int8_t mac[ETH_ALEN];
        char smac[MACSTR_LEN];
		ieee80211_tdls_set_mac_addr(mac, vap->iv_tdls_macaddr1, vap->iv_tdls_macaddr2);
		snprintf(smac, MACSTR_LEN, "%s", ether_sprintf(mac));
    	printk("TDLS set_tdls_rmac ....%s \n", smac);
		ieee80211_ioctl_set_tdls_rmac(dev, info, w, smac);
        break;
        }
    case IEEE80211_PARAM_CLR_TDLS_RMAC: {
        u_int8_t mac[ETH_ALEN];
        char smac[MACSTR_LEN];
		ieee80211_tdls_set_mac_addr(mac, vap->iv_tdls_macaddr1, vap->iv_tdls_macaddr2);
		snprintf(smac, MACSTR_LEN, "%s", ether_sprintf(mac));
    	printk("TDLS clr_tdls_rmac ....%s\n", smac);
		ieee80211_ioctl_clr_tdls_rmac(dev, info, w, smac);
        break;
        }
#ifdef CONFIG_RCPI
    case IEEE80211_PARAM_TDLS_RCPI_HI:
        if (!IEEE80211_TDLS_ENABLED(vap))
            return -EFAULT;

        if ((value >=0) && (value<=300)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, 
                "Setting TDLS:RCPI: Hi Threshold %d dB \n", value);
            vap->iv_ic->ic_tdls->hi_tmp = value;
        } else {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, 
                "Setting TDLS:RCPI: Hi Threshold - Invalid vaule %d dB\n", value);
            printk("Enter any value between 0dB-100dB \n");
        }
        break;
    case IEEE80211_PARAM_TDLS_RCPI_LOW:
        if (!IEEE80211_TDLS_ENABLED(vap))
            return -EFAULT;

        if ((value >=0) && (value<=300)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, 
                "Setting TDLS:RCPI: Low Threshold %d dB \n", value);
            vap->iv_ic->ic_tdls->lo_tmp = value;
        } else {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, 
                "Setting TDLS:RCPI: Low Threshold - Invalid vaule %d dB\n", value);
            printk("Enter any value between 0dB-100dB \n");
        }
        break;
    case IEEE80211_PARAM_TDLS_RCPI_MARGIN:
        if (!IEEE80211_TDLS_ENABLED(vap))
            return -EFAULT;

        if ((value >=0) && (value<=300)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, 
                "Setting TDLS:RCPI: Margin %d dB \n", value);
            vap->iv_ic->ic_tdls->mar_tmp = value;
        } else {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, 
                "Setting TDLS:RCPI: Margin - Invalid vaule %d dB\n", value);
            printk("Enter any value between 0dB-100dB \n");
        }
        break;
    case IEEE80211_PARAM_TDLS_SET_RCPI:
        if (!IEEE80211_TDLS_ENABLED(vap))
            return -EFAULT;
        if (value) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_TDLS, 
                "Enabling TDLS:RCPI: %d \n", value);
            vap->iv_ic->ic_tdls->hithreshold = vap->iv_ic->ic_tdls->hi_tmp;
            vap->iv_ic->ic_tdls->lothreshold = vap->iv_ic->ic_tdls->lo_tmp;
            vap->iv_ic->ic_tdls->margin = vap->iv_ic->ic_tdls->mar_tmp;
        }
        break;
#endif /* CONFIG_RCPI */
#endif /* UMAC_SUPPORT_TDLS */
#if ATH_SUPPORT_FLOWMAC_MODULE
    case IEEE80211_PARAM_FLOWMAC:
        vap->iv_flowmac = value;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                "%s flowmac enabled on this vap 0x%p \n", __func__, vap);
        break;
#endif
#if UMAC_SUPPORT_SMARTANTENNA
        case IEEE80211_PARAM_SMARTANT_TRAIN_MODE:
            ieee80211_smartantenna_set_param(vap, IEEE80211_SMARTANT_TRAIN_MODE, value);
        break;
        case IEEE80211_PARAM_SMARTANT_TRAIN_TYPE:
            ieee80211_smartantenna_set_param(vap, IEEE80211_SMARTANT_TRAIN_TYPE, value);
            break;
        case IEEE80211_PARAM_SMARTANT_PKT_LEN:
            ieee80211_smartantenna_set_param(vap, IEEE80211_SMARTANT_PKT_LEN, value);
            break;
        case IEEE80211_PARAM_SMARTANT_NUM_PKTS:
            ieee80211_smartantenna_set_param(vap, IEEE80211_SMARTANT_NUM_PKTS, value);
            break;
        case IEEE80211_PARAM_SMARTANT_TRAIN_START:
            ieee80211_smartantenna_set_param(vap, IEEE80211_SMARTANT_TRAIN_START, value);
            break;
        case IEEE80211_PARAM_SMARTANT_NUM_ITR:
            ieee80211_smartantenna_set_param(vap, IEEE80211_SMARTANT_NUM_ITR, value);
            break;
#endif
#if UMAC_SUPPORT_INTERNALANTENNA            
        case IEEE80211_PARAM_SMARTANT_CURRENT_ANTENNA:
            ieee80211_set_current_antenna(vap, value); 
            break;
        case IEEE80211_PARAM_SMARTANT_DEFAULT_ANTENNA:
            ic->ic_set_default_antenna(ic, value);
            ic->ic_default_rx_ant = value;            
            break;
#endif
#if UMAC_SUPPORT_SMARTANTENNA            
        case IEEE80211_PARAM_SMARTANT_AUTOMATION:
            ieee80211_smartantenna_set_param(vap, IEEE80211_SMARTANT_AUTOMATION, value);
            break;
        case IEEE80211_PARAM_SMARTANT_RETRAIN:
            ieee80211_smartantenna_set_param(vap, IEEE80211_SMARTANT_RETRAIN, value);
            break;
#endif  

#if ATH_SUPPORT_IBSS_DFS
     case IEEE80211_PARAM_IBSS_DFS_PARAM:
        {
#define IBSSDFS_CSA_TIME_MASK 0x00ff0000
#define IBSSDFS_ACTION_MASK   0x0000ff00
#define IBSSDFS_RECOVER_MASK  0x000000ff
            u_int8_t csa_in_tbtt;
            u_int8_t actions_threshold;
            u_int8_t rec_threshold_in_tbtt;

            csa_in_tbtt = (value & IBSSDFS_CSA_TIME_MASK) >> 16;
            actions_threshold = (value & IBSSDFS_ACTION_MASK) >> 8;
            rec_threshold_in_tbtt = (value & IBSSDFS_RECOVER_MASK);

            if (rec_threshold_in_tbtt > csa_in_tbtt &&
                actions_threshold > 0) {
                vap->iv_ibss_dfs_csa_threshold = csa_in_tbtt;
                vap->iv_ibss_dfs_csa_measrep_limit = actions_threshold;
                vap->iv_ibss_dfs_enter_recovery_threshold_in_tbtt = rec_threshold_in_tbtt;
                ieee80211_ibss_beacon_update_start(ic);
            } else {
                printk("please enter a valid value .ex 0x010102\n");
                printk("Ex.0xaabbcc aa[channel switch time] bb[actions count] cc[recovery time]\n");
                printk("recovery time must be bigger than channel switch time, actions count must > 0\n");
            }
        
#undef IBSSDFS_CSA_TIME_MASK 
#undef IBSSDFS_ACTION_MASK   
#undef IBSSDFS_RECOVER_MASK              
    }
        break;
#endif   
#if ATH_SUPPORT_IBSS_NETLINK_NOTIFICATION
    case IEEE80211_PARAM_IBSS_SET_RSSI_CLASS:
      {
	int i;
	u_int8_t rssi;
	u_int8_t *pvalue = (u_int8_t*)(extra + 4);

	/* 0th idx is 0 dbm(highest) always */
	vap->iv_ibss_rssi_class[0] = (u_int8_t)-1;

	for( i = 1; i < IBSS_RSSI_CLASS_MAX; i++ ) {
	  rssi = pvalue[i - 1];
	  /* Assumes the values in dbm are already sorted.
	   * Convert to rssi and store them */
	  vap->iv_ibss_rssi_class[i] = (rssi > 95 ? 0 : (95 - rssi));
	}
      }
      break;
    case IEEE80211_PARAM_IBSS_START_RSSI_MONITOR:
      vap->iv_ibss_rssi_monitor = value;
      /* set the hysteresis to atleast 1 */
      if (value && !vap->iv_ibss_rssi_hysteresis)
	vap->iv_ibss_rssi_hysteresis++;
      break;
    case IEEE80211_PARAM_IBSS_RSSI_HYSTERESIS:
      vap->iv_ibss_rssi_hysteresis = value;
        break;
#endif        
        break;
    }
    if (retv == ENETRESET)
    {
	if(vap->iv_opmode == IEEE80211_M_IBSS)
	{
            osifp->is_up = 0;
        }
        retv = IS_UP(dev) ? -osif_vap_init(dev, RESCAN) : 0;
    }
    return -retv;

#ifdef notyet
    struct ieee80211vap *vap = NETDEV_TO_VAP(dev);
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_rsnparms *rsn = &vap->iv_bss->ni_rsn;
    int *i = (int *) extra;
    int param = i[0];       /* parameter id is 1st */
    int value = i[1];       /* NB: most values are TYPE_INT */
    int retv = 0;
    int j, caps;
    const struct ieee80211_authenticator *auth;
    const struct ieee80211_aclator *acl;

    switch (param)
    {
    case IEEE80211_PARAM_ROAMING:
        if (!(IEEE80211_ROAMING_DEVICE <= value &&
            value <= IEEE80211_ROAMING_MANUAL))
            return -EINVAL;
        /* Making driver to initiate the roaming in single STA mode */
        if (ic->ic_opmode != IEEE80211_M_STA) {
        ic->ic_roaming = value;
        }
        break;
    case IEEE80211_PARAM_DROPUNENC_EAPOL:
        if (value)
            IEEE80211_VAP_DROPUNENC_EAPOL_ENABLE(vap);
        else
            IEEE80211_VAP_DROPUNENC_EAPOL_DISABLE(vap);
        break;

    case IEEE80211_PARAM_ABOLT:
        caps = 0;
        /*
        * Map abolt settings to capability bits;
        * this also strips unknown/unwanted bits.
        */
        if (value & IEEE80211_ABOLT_TURBO_PRIME)
            caps |= IEEE80211_ATHC_TURBOP;
        if (value & IEEE80211_ABOLT_COMPRESSION)
            caps |= IEEE80211_ATHC_COMP;
        if (value & IEEE80211_ABOLT_FAST_FRAME)
            caps |= IEEE80211_ATHC_FF;
        if (value & IEEE80211_ABOLT_XR)
            caps |= IEEE80211_ATHC_XR;
        if (value & IEEE80211_ABOLT_AR)
            caps |= IEEE80211_ATHC_AR;
        if (value & IEEE80211_ABOLT_BURST)
            caps |= IEEE80211_ATHC_BURST;
        if (value & IEEE80211_ABOLT_WME_ELE)
            caps |= IEEE80211_ATHC_WME;
        /* verify requested capabilities are supported */
        if ((caps & ic->ic_ath_cap) != caps)
            return -EINVAL;
        if (vap->iv_ath_cap != caps)
        {
            if ((vap->iv_ath_cap ^ caps) & IEEE80211_ATHC_TURBOP)
            {
                if (ieee80211_set_turbo(dev,  caps & IEEE80211_ATHC_TURBOP))
                    return -EINVAL;
                ieee80211_scan_flush(ic);
            }
            vap->iv_ath_cap = caps;
#ifdef ATH_SUPERG_XR
            ieee80211_setupxr(vap);
#endif
            /*
            ** Copy the status to the ATH layer (buggy kludge, but whatever)
            */
            ic->ic_ath_cap = vap->iv_ath_cap;
            retv = ENETRESET;
        }
        break;
    case IEEE80211_PARAM_GENREASSOC:
        {
            int arg = 0;
            IEEE80211_SEND_MGMT(vap->iv_bss, IEEE80211_FC0_SUBTYPE_REASSOC_REQ, (void *)&arg);
            break;
        }
    case IEEE80211_PARAM_COMPRESSION:
        retv = ieee80211_setathcap(vap, IEEE80211_ATHC_COMP, value);
        break;
    case IEEE80211_PARAM_WMM_AGGRMODE:
        retv = ieee80211_setathcap(vap, IEEE80211_ATHC_WME, value);
        break;
    case IEEE80211_PARAM_FF:
        retv = ieee80211_setathcap(vap, IEEE80211_ATHC_FF, value);
        break;
    case IEEE80211_PARAM_TURBO:
        retv = ieee80211_setathcap(vap, IEEE80211_ATHC_TURBOP, value);
    if (retv == ENETRESET) {
        if (ieee80211_set_turbo(dev,value))
        {
        retv = ieee80211_setathcap(vap, IEEE80211_ATHC_TURBOP, !value);
        return -EINVAL;
        }
        ieee80211_scan_flush(ic);
    }
        break;
//    case IEEE80211_PARAM_XR:
//        retv = ieee80211_setathcap(vap, IEEE80211_ATHC_XR, value);
//#ifdef ATH_SUPERG_XR
//        ieee80211_setupxr(vap);
//        if (IS_UP(ic->ic_dev))
//            ic->ic_reset(ic);
//#endif
//        break;
    case IEEE80211_PARAM_BURST:
        retv = ieee80211_setathcap(vap, IEEE80211_ATHC_BURST, value);
        break;
    case IEEE80211_PARAM_AR:
        retv = ieee80211_setathcap(vap, IEEE80211_ATHC_AR, value);
        break;
    case IEEE80211_PARAM_VAP_IND:
        if (value)
            vap->iv_flags_ext |= IEEE80211_FEXT_VAP_IND;
        else
            vap->iv_flags_ext &= ~IEEE80211_FEXT_VAP_IND;
        break;
    case IEEE80211_PARAM_BGSCAN:
        if (value)
        {
            if ((vap->iv_caps & IEEE80211_C_BGSCAN) == 0)
                return -EINVAL;
            vap->iv_flags |= IEEE80211_F_BGSCAN;
        }
        else
        {
            /* XXX racey? */
            vap->iv_flags &= ~IEEE80211_F_BGSCAN;
            ieee80211_cancel_scan(vap); /* anything current */
        }
        break;
    case IEEE80211_PARAM_BGSCAN_IDLE:
        if (value >= IEEE80211_BGSCAN_IDLE_MIN)
            vap->iv_bgscanidle = value*HZ/1000;
        else
            retv = EINVAL;
        break;
    case IEEE80211_PARAM_BGSCAN_INTERVAL:
        if (value >= IEEE80211_BGSCAN_INTVAL_MIN)
            vap->iv_bgscanintvl = value*HZ;
        else
            retv = EINVAL;
        break;
    case IEEE80211_PARAM_COVERAGE_CLASS:
        if (value >= 0 && value <= IEEE80211_COVERAGE_CLASS_MAX)
        {
            ic->ic_coverageclass = value;
            if (IS_UP_AUTO(vap))
                ieee80211_new_state(vap, IEEE80211_S_SCAN, 0);
            retv = 0;
        }
        else
            retv = EINVAL;
        break;
    case IEEE80211_PARAM_REGCLASS:
        if (value)
            ic->ic_flags_ext |= IEEE80211_FEXT_REGCLASS;
        else
            ic->ic_flags_ext &= ~IEEE80211_FEXT_REGCLASS;
        retv = ENETRESET;
        break;
    case IEEE80211_PARAM_SCANVALID:
        vap->iv_scanvalid = value*HZ;
        break;
    case IEEE80211_PARAM_ROAM_RSSI_11A:
        vap->iv_roam.rssi11a = value;
        break;
    case IEEE80211_PARAM_ROAM_RSSI_11B:
        vap->iv_roam.rssi11bOnly = value;
        break;
    case IEEE80211_PARAM_ROAM_RSSI_11G:
        vap->iv_roam.rssi11b = value;
        break;
    case IEEE80211_PARAM_ROAM_RATE_11A:
        vap->iv_roam.rate11a = value;
        break;
    case IEEE80211_PARAM_ROAM_RATE_11B:
        vap->iv_roam.rate11bOnly = value;
        break;
    case IEEE80211_PARAM_ROAM_RATE_11G:
        vap->iv_roam.rate11b = value;
        break;
    case IEEE80211_PARAM_QOSNULL:
        /* Force a QoS Null for testing. */
        ieee80211_send_qosnulldata(vap->iv_bss, value);
        break;
    case IEEE80211_PARAM_EOSPDROP:
        if (vap->iv_opmode == IEEE80211_M_HOSTAP)
        {
            if (value) IEEE80211_VAP_EOSPDROP_ENABLE(vap);
            else IEEE80211_VAP_EOSPDROP_DISABLE(vap);
        }
        break;
    case IEEE80211_PARAM_MARKDFS:
        if (value)
            ic->ic_flags_ext |= IEEE80211_FEXT_MARKDFS;
        else
            ic->ic_flags_ext &= ~IEEE80211_FEXT_MARKDFS;
        break;
    case IEEE80211_PARAM_SHORTPREAMBLE:
        if (value)
        {
            ic->ic_caps |= IEEE80211_C_SHPREAMBLE;
        }
        else
        {
            ic->ic_caps &= ~IEEE80211_C_SHPREAMBLE;
        }
        retv = ENETRESET;
        break;
    case IEEE80211_PARAM_BLOCKDFSCHAN:
        if (value)
        {
            ic->ic_flags_ext |= IEEE80211_FEXT_BLKDFSCHAN;
        }
        else
        {
            ic->ic_flags_ext &= ~IEEE80211_FEXT_BLKDFSCHAN;
        }
        retv = ENETRESET;
        break;
    case IEEE80211_PARAM_NETWORK_SLEEP:

        /* NB: should only be set when in single STA mode */
        if (ic->ic_opmode != IEEE80211_M_STA) {
            return -EINVAL;
        }

        switch (value) {
        case 0:
            ieee80211_pwrsave_set_mode(vap, IEEE80211_PWRSAVE_NONE);
            break;
        case 1:
            ieee80211_pwrsave_set_mode(vap, IEEE80211_PWRSAVE_LOW); 
            break;
        case 2:
            ieee80211_pwrsave_set_mode(vap, IEEE80211_PWRSAVE_NORMAL);  
            break;
        case 3:
            ieee80211_pwrsave_set_mode(vap, IEEE80211_PWRSAVE_MAXIMUM); 
            break;
        default:
            retv = EINVAL;
            break;
        }
        break;
    case IEEE80211_PARAM_FAST_CC:
        /*
        * turn off
        */
        if (!value)
        {
            ic->ic_flags_ext &= ~IEEE80211_FAST_CC;
            break;
        }

        /*
        * not capable
        */
        if (!(ic->ic_caps & IEEE80211_C_FASTCC))
        {
            retv = EINVAL;
            break;
        }
        /*
        * turn on
        */
        ic->ic_flags_ext |= IEEE80211_FAST_CC;
        if (IS_UP_AUTO(vap))
            ieee80211_new_state(vap, IEEE80211_S_SCAN, 0);

        break;
    case IEEE80211_PARAM_AMPDU_LIMIT:
        if ((value >= IEEE80211_AMPDU_LIMIT_MIN) &&
            (value <= IEEE80211_AMPDU_LIMIT_MAX))
            ic->ic_ampdu_limit = value;
        else
            retv = EINVAL;
        break;
    case IEEE80211_PARAM_AMPDU_DENSITY:
        ic->ic_mpdudensity = value;
        break;
    case IEEE80211_PARAM_AMPDU_SUBFRAMES:
        if ((value < IEEE80211_AMPDU_SUBFRAME_MIN) ||
            (value > IEEE80211_AMPDU_SUBFRAME_MAX))
        {
            retv = EINVAL;
            break;
        }
        ic->ic_ampdu_subframes = value;
        break;
    case IEEE80211_PARAM_AMSDU:
        if (!value) {
            ic->ic_flags_ext &= ~IEEE80211_FEXT_AMSDU;
            ic->ic_amsdu_enable(ic, 0);
        } else {
            ic->ic_flags_ext |= IEEE80211_FEXT_AMSDU;
            ic->ic_amsdu_limit = IEEE80211_AMSDU_LIMIT_MAX;
            ic->ic_amsdu_enable(ic, 1);
        }
        break;
    case IEEE80211_PARAM_AMSDU_LIMIT:
        if ((value >= IEEE80211_MTU_MAX) &&
            (value <= IEEE80211_AMSDU_LIMIT_MAX))
            ic->ic_amsdu_limit = value;
        else
            retv = EINVAL;
        break;

    case IEEE80211_PARAM_TX_CHAINMASK:
        if (value < IEEE80211_TX_CHAINMASK_MIN ||
            value > IEEE80211_TX_CHAINMASK_MAX)
        {
            retv = EINVAL;
            break;
        }

        /* Set HT transmit chain mask per user selection */
        ic->ic_tx_chainmask = value;

        /* Notify rate control, if necessary */
        if (IS_UP_AUTO(vap))
        {
            ieee80211_new_state(vap, IEEE80211_S_SCAN, 0);
        }
        break;

    case IEEE80211_PARAM_TX_CHAINMASK_LEGACY:
        if (value < IEEE80211_TX_CHAINMASK_MIN ||
            value > IEEE80211_TX_CHAINMASK_MAX)
        {
            retv = EINVAL;
            break;
        }

        /* Set legacy transmit chain mask per user selection */
        ic->ic_tx_chainmask_legacy = value;
        break;

    case IEEE80211_PARAM_RX_CHAINMASK:
        if (value < IEEE80211_RX_CHAINMASK_MIN ||
            value > IEEE80211_RX_CHAINMASK_MAX)
        {
            retv = EINVAL;
            break;
        }

        ic->ic_rx_chainmask = value;

        break;

    case IEEE80211_PARAM_RTSCTS_RATECODE:
        ic->ic_rtscts_ratecode = value;

        break;

    case IEEE80211_PARAM_HT_PROTECTION:
        if (!value)
        {
            ic->ic_flags_ext &= ~IEEE80211_FEXT_HTPROT;
        }
        else
        {
            ic->ic_flags_ext |= IEEE80211_FEXT_HTPROT;
        }
        break;

    case IEEE80211_PARAM_RESET_ONCE:
        if (value) {
            ic->ic_flags_ext |= IEEE80211_FEXT_RESET;
            if (IS_UP(ic->ic_dev)) {
                ic->ic_reset_start(ic, 0);
                ic->ic_reset(ic);
                ic->ic_reset_end(ic, 0);
            }
        }
        break;

    case IEEE80211_PARAM_NO_EDGE_CH:
        vap->iv_flags_ext |= IEEE80211_FEXT_NO_EDGE_CH;
        break;

    case IEEE80211_PARAM_BASICRATES:
        if ((vap->iv_flags_ext & IEEE80211_FEXT_PUREN) ||
            (vap->iv_flags & IEEE80211_F_PUREG)) {
            return -EINVAL;
        }
        retv = ieee80211_set_basicrates(ic, &vap->iv_bss->ni_rates, value);
        if (retv == 0) {
            vap->iv_flags_ext |= IEEE80211_FEXT_BR_UPDATE;
        }
        break;

    case IEEE80211_PARAM_STA_FORWARD:
        if (value)
            vap->iv_flags_ext |= IEEE80211_C_STA_FORWARD;
        else
            vap->iv_flags_ext &= ~IEEE80211_C_STA_FORWARD;
        break;

    case IEEE80211_PARAM_SCAN_PRE_SLEEP:
        if (value >= 0 && value <= IEEE80211_SCAN_PRE_SLEEP_MAX)
            vap->iv_scan_pre_sleep = value;
        else
            retv = EINVAL;
        break;
    case IEEE80211_PARAM_SLEEP_PRE_SCAN:
        if (value >= 0 && value <= IEEE80211_SLEEP_PRE_SCAN_MAX)
            vap->iv_sleep_pre_scan = value;
        else
            retv = EINVAL;
        break;

    default:
        retv = EOPNOTSUPP;
        break;
    }
#ifdef ATH_SUPERG_XR
    /* set the same params on the xr vap device if exists */
    if(vap->iv_xrvap && !(vap->iv_flags & IEEE80211_F_XR) )
    {
        ieee80211_ioctl_setparam(vap->iv_xrvap->iv_dev,info,w,extra);
        vap->iv_xrvap->iv_ath_cap &= IEEE80211_ATHC_XR; /* XR vap does not support  any superG features */
    }
    /*
    * do not reset the xr vap , which is automatically
    * reset by the state machine now.
    */
    if(!vap->iv_xrvap || (vap->iv_xrvap && !(vap->iv_flags & IEEE80211_F_XR)))
    {
        if (retv == ENETRESET)
        {
            retv = IS_UP_AUTO(vap) ? ieee80211_open(vap->iv_dev) : 0;
        }
    }
#else
    /* XXX should any of these cause a rescan? */
    if (retv == ENETRESET)
    {
        retv = IS_UP_AUTO(vap) ? ieee80211_open(vap->iv_dev) : 0;
    }
#endif
    return -retv;
#endif /* notyet */
}

#ifdef notyet
static int
cap2cipher(int flag)
{
    switch (flag)
    {
    case IEEE80211_C_WEP:       return IEEE80211_CIPHER_WEP;
    case IEEE80211_C_AES:       return IEEE80211_CIPHER_AES_OCB;
    case IEEE80211_C_AES_CCM:   return IEEE80211_CIPHER_AES_CCM;
    case IEEE80211_C_CKIP:      return IEEE80211_CIPHER_CKIP;
    case IEEE80211_C_TKIP:      return IEEE80211_CIPHER_TKIP;
    }
    return -1;
}
#endif

#define IEEE80211_MODE_TURBO_STATIC_A   IEEE80211_MODE_MAX
int
ieee80211_ioctl_getmode(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    static const struct
    {
        char *name;
        int mode;
    } mappings[] = {
        /* NB: need to order longest strings first for overlaps */
        { "11AST" , IEEE80211_MODE_TURBO_STATIC_A },
        { "AUTO"  , IEEE80211_MODE_AUTO },
        { "11A"   , IEEE80211_MODE_11A },
        { "11B"   , IEEE80211_MODE_11B },
        { "11G"   , IEEE80211_MODE_11G },
        { "FH"    , IEEE80211_MODE_FH },
	    { "TA"      , IEEE80211_MODE_TURBO_A },
	    { "TG"      , IEEE80211_MODE_TURBO_G },
	    { "11NAHT20"		, IEEE80211_MODE_11NA_HT20 },
	    { "11NGHT20"		, IEEE80211_MODE_11NG_HT20 },
	    { "11NAHT40PLUS"	, IEEE80211_MODE_11NA_HT40PLUS },
	    { "11NAHT40MINUS" 	, IEEE80211_MODE_11NA_HT40MINUS },
	    { "11NGHT40PLUS"	, IEEE80211_MODE_11NG_HT40PLUS },
	    { "11NGHT40MINUS" 	, IEEE80211_MODE_11NG_HT40MINUS },
        { "11NGHT40" 		, IEEE80211_MODE_11NG_HT40},
        { "11NAHT40" 		, IEEE80211_MODE_11NA_HT40},
        { NULL }
    };
	
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
	struct iw_point *wri = (struct iw_point *)w;
	enum ieee80211_phymode	phymode;
    int i;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_GETMODE, "getmode") ;
	
	phymode = wlan_get_desired_phymode(vap);
				
    for (i = 0; mappings[i].name != NULL ; i++) 
	{
		if (phymode == mappings[i].mode)
		{
			wri->length = strlen(mappings[i].name);
                        strncpy(extra, mappings[i].name, wri->length);
			break;
		}
    }
	
    return 0;
}
#undef IEEE80211_MODE_TURBO_STATIC_A



static int
ieee80211_ioctl_p2p_big_param(struct net_device *dev, struct iwreq *iwr)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    union iwreq_data *iwd = &iwr->u;
    int retv = 0; /* default to success */
    ieee80211_scan_params scan_params;
    struct ieee80211_scan_req *scan_extra;

    switch(iwr->u.data.flags) 
    {
#if ATHEROS_LINUX_P2P_DRIVER
    case IEEE80211_IOC_P2P_FIND_BEST_CHANNEL:
     
        { int bestfreq[3]; 
        
            if(iwr->u.data.length < (3*sizeof(int))) {
                retv = -EFAULT;
                break;
            }
        
        
        memset(bestfreq,sizeof(int)*3,0); 
        retv = wlan_acs_find_best_channel(vap,bestfreq,3);
        if(retv < 0) {
           break;
        }
        
        retv = copy_to_user(iwr->u.data.pointer,bestfreq,sizeof(int)*3);
        if(retv < 0)
           break;
        }
        
        break;
    
    case IEEE80211_IOC_P2P_SET_CHANNEL:

        if (osifp->p2p_handle) {
            struct ieee80211_p2p_set_channel sc;
            if (_copy_from_user(&sc, iwr->u.data.pointer, sizeof(sc))) {
                printk(" %s: _copy_from_user fail %p\n",
                        __func__, iwr->u.data.pointer);
                retv = -EFAULT;
                break;
            }
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
            "%s: IEEE80211_IOC_P2P_SET_CHANNEL freq %d reqid %d time %d\n", 
                              __func__, sc.freq, sc.req_id, sc.channel_time);
            if (wlan_p2p_get_vap_handle(osifp->p2p_handle) == NULL) {
                printk(" %s: wlan_p2p_get_vap_handle failed \n", __func__);
                retv = -ENOENT;
                break;
            }

          #ifdef ATH_SUPPORT_HTC
            //For P2P_DEVICE VAP, we need to set it to related listen channel. Not always use channel-1.
            if ((vap->iv_bsschan->ic_freq != sc.freq) &&
                (u_int8_t)(osifp->os_opmode == IEEE80211_M_P2P_DEVICE) &&
                (sc.freq >= 2412 && sc.freq <= 2462)) {
                wlan_reset_iv_chan(vap, ((sc.freq - 2412) / 5 + 1));
                printk("===== Set to desire channel : %d =====\n", vap->iv_bsschan->ic_freq);
            }
          #endif

#if 0
            if (wlan_scan_in_progress(vap)) {
                printk(" %s: scan cancel \n", __func__);
                wlan_scan_cancel(vap, osifp->scan_requestor, 
                                 IEEE80211_ALL_SCANS, 
                                 IEEE80211_SCAN_CANCEL_WAIT);
            }
#endif
            retv = -EINVAL;
            if (wlan_p2p_set_channel(osifp->p2p_handle, true, sc.freq, sc.req_id,
                                    sc.channel_time) == EOK) {
                retv = 0;
            }
        }
        break;
    
    case IEEE80211_IOC_P2P_FETCH_FRAME:
    {
        struct pending_rx_frames_list *pending = osif_fetch_p2p_mgmt(dev);

        if (!pending) {
            return -ENOSPC; /* no more messages are queued */
        }
        /* all the caller wants is the raw frame data, except it needs freq */
        iwr->u.data.length = 
                MIN(iwr->u.data.length, 
                    (pending->rx_frame.frame_len + sizeof(pending->freq) +
                                               sizeof(pending->frame_type)));

        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                "%s: IEEE80211_IOC_P2P_FETCH_FRAME size=%d frame_len=%d\n",
                __func__, iwr->u.data.length, pending->rx_frame.frame_len);
        pending->freq = pending->rx_frame.freq;
        
        retv = _copy_to_user(iwr->u.data.pointer, &pending->freq, 
                            iwr->u.data.length);

        kfree(pending);
    }
    break;
    
    case IEEE80211_IOC_P2P_SEND_ACTION:
    {
        struct ieee80211_p2p_send_action *sa;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                "%s: IEEE80211_IOC_P2P_SEND_ACTION size=%d \n", __func__, 
                iwr->u.data.length);

        if (iwr->u.data.length > MAX_TX_RX_PACKET_SIZE + sizeof(*sa))
                return -EFAULT;

        sa = (struct ieee80211_p2p_send_action *)OS_MALLOC(osifp->os_handle, 
                                                iwr->u.data.length, GFP_KERNEL);
        if (!sa)
            return -ENOMEM;

        if (_copy_from_user(sa, iwr->u.data.pointer, iwr->u.data.length)) {
            OS_FREE(sa);
            return -EFAULT;
        }
                
        if (vap && wlan_vap_send_action_frame(vap, sa->freq,
                            ieee80211_ioctl_send_action_frame_cb,
                            osifp, sa->dst_addr, sa->src_addr,
                            sa->bssid, (u_int8_t *) (sa + 1),
                            iwd->data.length - sizeof(*sa)) == 0) {

            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                          "%s: IEEE80211_IOC_P2P_SEND_ACTION freq=%d dest=%s ",
                          __func__, sa->freq,ether_sprintf(sa->dst_addr));
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                              "src=%s ",ether_sprintf(sa->src_addr));
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                              "bssid=%s \n ",ether_sprintf(sa->bssid));
                    retv = 0;
         } else {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                    "%s: IEEE80211_IOC_P2P_SEND_ACTION FAILED \n", __func__);
                    retv = -EINVAL;
         }
        OS_FREE(sa);
    }
    break;

#endif /* ATHEROS_LINUX_P2P_DRIVER */
    case IEEE80211_IOC_SCAN_REQ:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, 
                "set IEEE80211_IOC_SCAN_REQ, len=%d \n", iwd->data.length);

        /* fixme I have no clue how big this could be, I want to validate it */
        if (iwr->u.data.length > 8192 + sizeof(*scan_extra))
            return -EFAULT;

        scan_extra = (struct ieee80211_scan_req *)OS_MALLOC(osifp->os_handle, iwr->u.data.length, GFP_KERNEL);
        if (!scan_extra)
            return -ENOMEM;

        if (_copy_from_user(scan_extra, iwr->u.data.pointer, iwr->u.data.length))
            return -EFAULT;
            
        OS_MEMZERO(&scan_params,sizeof(scan_params));
#if defined(UMAC_SUPPORT_RESMGR) && defined(ATH_SUPPORT_P2P) && defined(ATH_SUPPORT_HTC)
        {
            bool connected_flag = false;

            if (osifp->sm_handle && wlan_connection_sm_is_connected(osifp->sm_handle))
                connected_flag = true;
        wlan_set_default_scan_parameters(vap, &scan_params, 
                                        wlan_vap_get_opmode(vap),
                                            true, true, connected_flag, true, 0, NULL, 0);
        }
#else
        wlan_set_default_scan_parameters(vap, &scan_params, 
                                        wlan_vap_get_opmode(vap),
                                        true, true, true, true, 0, NULL, 0);
#endif
        scan_params.min_dwell_time_active = scan_params.max_dwell_time_active
                                          = 100;
        if (osifp->sm_handle && 
                        wlan_connection_sm_is_connected(osifp->sm_handle)) {
            scan_params.type = IEEE80211_SCAN_BACKGROUND;
        } else {
            scan_params.type = IEEE80211_SCAN_FOREGROUND;
        }
        scan_params.flags = IEEE80211_SCAN_ALLBANDS | IEEE80211_SCAN_ACTIVE;
        scan_params.flags |= IEEE80211_SCAN_ADD_BCAST_PROBE;
        if (wlan_scan_in_progress(vap)) {
            wlan_scan_cancel(vap, osifp->scan_requestor, IEEE80211_ALL_SCANS, IEEE80211_SCAN_CANCEL_WAIT);
        }
        if (iwd->data.length) {
            int i;

            if ((int) sizeof(*scan_extra) + scan_extra->ie_len > iwd->data.length) {
                printk(" %s: scan data is greater than passed \n", __func__);
                retv = EINVAL;
                goto scanreq_fail;
            }

            if (scan_extra->ie_len) {
                scan_params.ie_len = scan_extra->ie_len;
                scan_params.ie_data = (u_int8_t *) (scan_extra + 1);
            }

            if (scan_extra->num_ssid > MAX_SCANREQ_SSID ||
                scan_extra->num_ssid > IEEE80211_SCAN_PARAMS_MAX_SSID) {
                printk(" %s: num_ssid is greater:%d \n", __func__, scan_extra->num_ssid);
                retv = EINVAL;
                goto scanreq_fail;
            }

            scan_params.num_ssid = scan_extra->num_ssid;
            for (i = 0; i < scan_extra->num_ssid; i++) {
                if (scan_extra->ssid_len[i] > 32) {
                    printk(" %s: ssid is large:%d \n", __func__, scan_extra->ssid_len[i]);
                    retv = EINVAL;
                    goto scanreq_fail;
                }
                scan_params.ssid_list[i].len = scan_extra->ssid_len[i];
                memcpy(scan_params.ssid_list[i].ssid,
                scan_extra->ssid[i], scan_extra->ssid_len[i]);
            }

            if (scan_extra->num_freq > MAX_SCANREQ_FREQ) {
                retv = EINVAL;
                goto scanreq_fail;
            }

            if (scan_extra->num_freq) {
                scan_params.num_channels = scan_extra->num_freq;
                scan_params.chan_list = scan_extra->freq;
                printk("%s frequency list ", __func__);
                for (i = 0; i < scan_extra->num_freq; i++) {
                    printk("%d  ", scan_extra->freq[i]);
                }
                printk("\n");
            }
        }

        if (osifp->os_scan_band != OSIF_SCAN_BAND_ALL) {
            scan_params.flags &= ~IEEE80211_SCAN_ALLBANDS;
            if (osifp->os_scan_band == OSIF_SCAN_BAND_2G_ONLY)
                scan_params.flags |= IEEE80211_SCAN_2GHZ;
            else if (osifp->os_scan_band == OSIF_SCAN_BAND_5G_ONLY)
                scan_params.flags |= IEEE80211_SCAN_5GHZ;
            else {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                                    "%s: unknow scan band, scan all bands.\n", __func__);
                scan_params.flags |= IEEE80211_SCAN_ALLBANDS;
            }
        }
        
        /* start a scan */
        if (ieee80211_vap_vap_ind_is_set(vap)) {
            scan_params.type = IEEE80211_SCAN_REPEATER_BACKGROUND;
        } 
        retv = wlan_scan_start(vap, &scan_params, osifp->scan_requestor, IEEE80211_SCAN_PRIORITY_LOW, &(osifp->scan_id));

scanreq_fail:
        OS_FREE(scan_extra);

        break;

#if ATHEROS_LINUX_P2P_DRIVER
    case IEEE80211_IOC_P2P_GO_NOA:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s\n", 
                            "set IEEE80211_IOC_P2P_GO_NOA ");
        if (osifp->p2p_go_handle || osifp->p2p_client_handle) {
#define MAX_NUM_SET_NOA     2   /* Number of set of NOA schedule to set */
            wlan_p2p_go_noa_req request[MAX_NUM_SET_NOA];
            struct ieee80211_p2p_go_noa *noa;
            u_int8_t    num_schedules;
            int i;
            
            if (iwr->u.data.length < (int) sizeof(*noa))
                break;

            if ((iwr->u.data.length % sizeof(struct ieee80211_p2p_go_noa)) != 0) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "Error: schedules length inconsistent=%d\n",
                                  iwr->u.data.length);
                break;
            }

            num_schedules = iwr->u.data.length/ sizeof(struct ieee80211_p2p_go_noa);

            if (num_schedules > MAX_NUM_SET_NOA) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "Error: too many schedules=%d\n",
                                  num_schedules);
                break;
            }

            noa = (struct ieee80211_p2p_go_noa *)OS_MALLOC(osifp->os_handle, sizeof(*noa)*num_schedules, GFP_KERNEL);
            if (!noa)
                return -ENOMEM;

            if (_copy_from_user(noa, iwr->u.data.pointer, sizeof(*noa)*num_schedules)) {
                OS_FREE(noa);
                return -EFAULT;
            }

            /* set NOA with driver */
            for (i = 0; i < num_schedules; i++) {
                request[i].num_iterations = noa[i].num_iterations;
                request[i].offset_next_tbtt = noa[i].offset_next_tbtt;
                request[i].duration = noa[i].duration;
            }
            retv = 0;
            
            if (osifp->p2p_client_handle) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, 
                      "set NOA for client. num_iterations=%d, Offset=%d, Dur=%d\n",
                      noa->num_iterations, noa->offset_next_tbtt, noa->duration);

                wlan_p2p_client_set_noa_schedule(osifp->p2p_client_handle,1,request);
            } else if(osifp->p2p_go_handle) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, 
                      "set NOA for GO.NOA#1  num_iterations=%d, Offset=%d, Dur=%d\n",
                      noa[0].num_iterations, noa[0].offset_next_tbtt, noa[0].duration);
                if (num_schedules > 1) {
                   IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, 
                      "set NOA for GO.NOA#2  num_iterations=%d, Offset=%d, Dur=%d\n",
                      noa[1].num_iterations, noa[1].offset_next_tbtt, noa[1].duration);
                }
                wlan_p2p_GO_set_noa_schedule(osifp->p2p_go_handle,num_schedules,request);
            } 
            OS_FREE(noa);
        }
        break;
    case IEEE80211_IOC_P2P_NOA_INFO: /* Get NOA info(IE) on a client */
        if (osifp->p2p_client_handle) {
            wlan_p2p_noa_info noa_info;
            retv = 0;
            wlan_p2p_client_get_noa_info(osifp->p2p_client_handle,&noa_info);
            /* all the caller wants is the raw frame data, except it needs freq */
            iwr->u.data.length =
                MIN(iwr->u.data.length, sizeof(noa_info));
            retv = _copy_to_user(iwr->u.data.pointer, &noa_info,
                            iwr->u.data.length);
        } else if (osifp->p2p_go_handle) {
            wlan_p2p_noa_info noa_info;
            retv = 0;
            wlan_p2p_GO_get_noa_info(osifp->p2p_go_handle,&noa_info);
            /* all the caller wants is the raw frame data, except it needs freq */
            iwr->u.data.length =
                MIN(iwr->u.data.length, sizeof(noa_info));
            retv = _copy_to_user(iwr->u.data.pointer, &noa_info,
                            iwr->u.data.length);
        }
        break;

#endif /* ATHEROS_LINUX_P2P_DRIVER */

    case IEEE80211_IOC_CANCEL_SCAN:
        if (wlan_scan_in_progress(vap)) {
            wlan_scan_cancel(vap, osifp->scan_requestor, IEEE80211_ALL_SCANS, IEEE80211_SCAN_CANCEL_WAIT);
        }
    	break; 
    default:
        retv = -EINVAL;
    }
    return retv;
}

#if ATHEROS_LINUX_P2P_DRIVER
/* returns a positive error code or 0 for success */
static int
ieee80211_ioctl_getp2p(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    union iwreq_data *iwd = w;
    int retv = 0; /* default to success */
    int *param = (int *) extra;
    int subioctl = (iwd->data.length <= (IFNAMSIZ - sizeof(iwd->mode)) ) ?
                            iwd->mode : iwd->data.flags;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
            "%s parameter is 0x%x=%d get2 len=%i\n",
            __func__, param[0], subioctl, iwd->data.length);
    switch (subioctl) {
#if not_yet
    case IEEE80211_IOC_ASSOCREQIE_EV:

        printk(" %s: IEEE80211_IOC_ASSOCREQIE_EV Entering \n");
        if (!osifp->assoc_req_data) {
            retv  = EINVAL;
            break;
        }
        iwd->data.length = wbuf_get_pktlen(osifp->assoc_req_data);
        iwd->data.pointer = wbuf_header(osifp->assoc_req_data);
        printk(" %s: IEEE80211_IOC_ASSOCREQIE_EV called, len=%d \n", iwd->data.length);
        /* Clean up the temp holder */
        osifp->assoc_req_data = NULL;
        break;
#endif
    case IEEE80211_IOC_BSSID:
        if (iwd->data.length != IEEE80211_ADDR_LEN) {
            retv = EINVAL;
            break;
        }
        retv = wlan_vap_get_bssid(vap, extra);
        break;

    case IEEE80211_IOC_P2P_OPMODE:
        param[0] = osifp->os_opmode;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                "%s returning 0x%x cmd=0x%x get3 %i\n",
                __func__, param[0], subioctl, param[1]);
        break;

    case IEEE80211_IOC_CONNECTION_STATE:
        param[0] = IS_CONNECTED(osifp);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                "%s CONNECTION_STATE 0x%x\n", __func__, param[0]);
        break;

    case IEEE80211_IOC_P2P_RADIO_IDX:
		if (osifp->os_comdev->name != NULL) { //get radio name and get its' index number
			int unit = -1, error;
            
			error = ifr_name2unit(osifp->os_comdev->name,&unit);
			if (error == 0)
				param[0] = unit;
		    else
				retv =  EOPNOTSUPP;
		} else 
			retv =  EOPNOTSUPP;
		break;
    default:
        retv =  EOPNOTSUPP;
    }
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
            "%s returning 0x%x cmd=0x%x status %i\n",
            __func__, param[0], subioctl, retv);
    return retv; /* success return */
}
#endif
int
ieee80211_ioctl_getparam(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    wlan_dev_t ic = wlan_vap_get_devhandle(vap);
    int *param = (int *) extra;
    int retv = 0;

    debug_print_ioctl(dev->name, param[0], find_ieee_priv_ioctl_name(param[0], 0));
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
            "%s parameter is 0x%x get1 %i\n", __func__, param[0], param[1]);

    switch (param[0])
    {
    case IEEE80211_PARAM_MAXSTA:
        printk("Getting Max Stations: %d\n", vap->iv_max_aid - 1);
        param[0] = vap->iv_max_aid - 1;
        break;
    case IEEE80211_PARAM_AUTO_ASSOC:
        param[0] = wlan_get_param(vap, IEEE80211_AUTO_ASSOC);
        break;
    case IEEE80211_PARAM_VAP_COUNTRY_IE:
        param[0] = wlan_get_param(vap, IEEE80211_FEATURE_COUNTRY_IE);
        break;
    case IEEE80211_PARAM_VAP_DOTH:
        param[0] = wlan_get_param(vap, IEEE80211_FEATURE_DOTH);
        break;
    case IEEE80211_PARAM_HT40_INTOLERANT:
        param[0] = wlan_get_param(vap, IEEE80211_HT40_INTOLERANT);
        break;

    case IEEE80211_PARAM_CHWIDTH:
        param[0] = wlan_get_param(vap, IEEE80211_CHWIDTH);
        break;

    case IEEE80211_PARAM_CHEXTOFFSET:
        param[0] = wlan_get_param(vap, IEEE80211_CHEXTOFFSET);
        break;
#ifdef ATH_SUPPORT_QUICK_KICKOUT
    case IEEE80211_PARAM_STA_QUICKKICKOUT:
        param[0] = wlan_get_param(vap, IEEE80211_STA_QUICKKICKOUT);
        break;
#endif
    case IEEE80211_PARAM_CHSCANINIT:
        param[0] = wlan_get_param(vap, IEEE80211_CHSCANINIT);
        break;

    case IEEE80211_PARAM_COEXT_DISABLE:
        param[0] = ((ic->ic_flags & IEEE80211_F_COEXT_DISABLE) != 0);
        break;

    case IEEE80211_PARAM_AUTHMODE:
        //fixme how it used to be done: param[0] = osifp->authmode;
        {
            ieee80211_auth_mode modes[IEEE80211_AUTH_MAX];
            retv = wlan_get_auth_modes(vap, modes, IEEE80211_AUTH_MAX);
            if (retv > 0)
            {
                param[0] = modes[0];
                if((retv > 1) && (modes[0] == IEEE80211_AUTH_OPEN) && (modes[1] == IEEE80211_AUTH_SHARED))
                    param[0] =  IEEE80211_AUTH_AUTO;
                retv = 0;
            }
        }
        break;
    case IEEE80211_PARAM_MCASTCIPHER:
        {
            ieee80211_cipher_type mciphers[1];
            int count;
            count = wlan_get_mcast_ciphers(vap,mciphers,1);
            if (count == 1)
                param[0] = mciphers[0];
        }
        break;
    case IEEE80211_PARAM_MCASTKEYLEN:
        param[0] = wlan_get_rsn_cipher_param(vap, IEEE80211_MCAST_CIPHER_LEN);
        break;
    case IEEE80211_PARAM_UCASTCIPHERS:
        do {
            ieee80211_cipher_type uciphers[IEEE80211_CIPHER_MAX];
            int i, count;
            count = wlan_get_ucast_ciphers(vap, uciphers, IEEE80211_CIPHER_MAX);
            param[0] = 0;
            for (i = 0; i < count; i++) {
                param[0] |= 1<<uciphers[i];
            }
    } while (0);
        break;
    case IEEE80211_PARAM_UCASTCIPHER:
        do {
            ieee80211_cipher_type uciphers[1];
            int count = 0;
            count = wlan_get_ucast_ciphers(vap, uciphers, 1);
            param[0] = 0;
            if (count == 1)
                param[0] |= 1<<uciphers[0];
        } while (0);
        break;
    case IEEE80211_PARAM_UCASTKEYLEN:
        param[0] = wlan_get_rsn_cipher_param(vap, IEEE80211_UCAST_CIPHER_LEN);
        break;
    case IEEE80211_PARAM_PRIVACY:
        param[0] = wlan_get_param(vap, IEEE80211_FEATURE_PRIVACY);
        break;
    case IEEE80211_PARAM_COUNTERMEASURES:
        param[0] = wlan_get_param(vap, IEEE80211_FEATURE_COUNTER_MEASURES);
        break;
    case IEEE80211_PARAM_HIDESSID:
        param[0] = wlan_get_param(vap, IEEE80211_FEATURE_HIDE_SSID);
        break;
    case IEEE80211_PARAM_APBRIDGE:
        param[0] = wlan_get_param(vap, IEEE80211_FEATURE_APBRIDGE);
        break;
    case IEEE80211_PARAM_KEYMGTALGS:
        param[0] = wlan_get_rsn_cipher_param(vap, IEEE80211_KEYMGT_ALGS);
        break;
    case IEEE80211_PARAM_RSNCAPS:
        param[0] = wlan_get_rsn_cipher_param(vap, IEEE80211_RSN_CAPS);
        break;
    case IEEE80211_PARAM_WPA:
    {
            ieee80211_auth_mode modes[IEEE80211_AUTH_MAX];
            int count, i;
            param[0] = 0;
            count = wlan_get_auth_modes(vap,modes,IEEE80211_AUTH_MAX);
            for (i = 0; i < count; i++) {
            if (modes[i] == IEEE80211_AUTH_WPA)
                param[0] |= 0x1;
            if (modes[i] == IEEE80211_AUTH_RSNA)
                param[0] |= 0x2;
            }
    }
        break;
    case IEEE80211_PARAM_DBG_LVL:
        param[0] = wlan_get_param(vap, IEEE80211_MSG_FLAGS);
        break;
#if UMAC_SUPPORT_IBSS
    case IEEE80211_PARAM_IBSS_CREATE_DISABLE:
        param[0] = osifp->disable_ibss_create;
        break;
#endif
	case IEEE80211_PARAM_WEATHER_RADAR_CHANNEL:
        param[0] = wlan_get_param(vap, IEEE80211_WEATHER_RADAR);
        break;
	case IEEE80211_PARAM_WEP_KEYCACHE:
        param[0] = wlan_get_param(vap, IEEE80211_WEP_KEYCACHE);
		break;
	case IEEE80211_PARAM_GET_ACS:
        param[0] = wlan_autoselect_in_progress(vap);
		break;
	case IEEE80211_PARAM_GET_CAC:
        if(vap->iv_state_info.iv_state == IEEE80211_S_DFS_WAIT)
            param[0] = !!vap->iv_state_info.iv_state;
        else
            param[0] = 0;
		break;
    case IEEE80211_PARAM_BEACON_INTERVAL:
        param[0] = wlan_get_param(vap, IEEE80211_BEACON_INTVAL);
        break;
#if ATH_SUPPORT_AP_WDS_COMBO
    case IEEE80211_PARAM_NO_BEACON:
        param[0] = wlan_get_param(vap, IEEE80211_NO_BEACON);
        break;
#endif
    case IEEE80211_PARAM_PUREG:
        param[0] = wlan_get_param(vap, IEEE80211_FEATURE_PUREG);
        break;
    case IEEE80211_PARAM_PUREN:
        param[0] = wlan_get_param(vap, IEEE80211_FEATURE_PURE11N);
        break;
    case IEEE80211_PARAM_WDS:
        param[0] = wlan_get_param(vap, IEEE80211_FEATURE_WDS);
        break;
    case IEEE80211_PARAM_VAP_IND:
        param[0] = wlan_get_param(vap, IEEE80211_FEATURE_VAP_IND);
        break; 
    case IEEE80211_IOCTL_GREEN_AP_PS_ENABLE: 
        param[0] = (wlan_get_device_param(ic, IEEE80211_DEVICE_GREEN_AP_PS_ENABLE) ? 1:0);
        break;
    case IEEE80211_IOCTL_GREEN_AP_PS_TIMEOUT:
        param[0] = wlan_get_device_param(ic, IEEE80211_DEVICE_GREEN_AP_PS_TIMEOUT);
        break;
    case IEEE80211_IOCTL_GREEN_AP_PS_ON_TIME:
        param[0] = wlan_get_device_param(ic, IEEE80211_DEVICE_GREEN_AP_PS_ON_TIME);
        break;

#ifdef ATH_WPS_IE
    case IEEE80211_PARAM_WPS:
        param[0] = wlan_get_param(vap, IEEE80211_WPS_MODE);
        break;
#endif
#ifdef ATH_EXT_AP
    case IEEE80211_PARAM_EXTAP:
        param[0] = (IEEE80211_VAP_IS_EXT_AP_ENABLED(vap) == IEEE80211_FEXT_AP);
        break;
#endif


    case IEEE80211_PARAM_STA_FORWARD:
    param[0]  = wlan_get_param(vap, IEEE80211_FEATURE_STAFWD);
    break;

    case IEEE80211_PARAM_CWM_EXTPROTMODE:
        param[0] = wlan_get_device_param(ic, IEEE80211_DEVICE_CWM_EXTPROTMODE);
        break;
    case IEEE80211_PARAM_CWM_EXTPROTSPACING:
        param[0] = wlan_get_device_param(ic, IEEE80211_DEVICE_CWM_EXTPROTSPACING);
        break;
    case IEEE80211_PARAM_CWM_ENABLE:
        param[0] = wlan_get_device_param(ic, IEEE80211_DEVICE_CWM_ENABLE);
        break;
    case IEEE80211_PARAM_CWM_EXTBUSYTHRESHOLD:
        param[0] = wlan_get_device_param(ic, IEEE80211_DEVICE_CWM_EXTBUSYTHRESHOLD);
        break;
    case IEEE80211_PARAM_DOTH:
        param[0] = wlan_get_device_param(ic, IEEE80211_DEVICE_DOTH);
        break;
    case IEEE80211_PARAM_WMM:
        param[0] = wlan_get_param(vap, IEEE80211_FEATURE_WMM);
        break;
    case IEEE80211_PARAM_PROTMODE:
        param[0] = wlan_get_device_param(ic, IEEE80211_DEVICE_PROTECTION_MODE);
        break;
    case IEEE80211_PARAM_DRIVER_CAPS:
        param[0] = wlan_get_param(vap, IEEE80211_DRIVER_CAPS);
        break;
    case IEEE80211_PARAM_MACCMD:
        param[0] = wlan_get_acl_policy(vap);
        break;
    case IEEE80211_PARAM_DROPUNENCRYPTED:
        param[0] = wlan_get_param(vap, IEEE80211_FEATURE_DROP_UNENC);
    break;
    case IEEE80211_PARAM_DTIM_PERIOD:
        param[0] = wlan_get_param(vap, IEEE80211_DTIM_INTVAL);
        break;
    case IEEE80211_PARAM_SHORT_GI:
        param[0] = wlan_get_param(vap, IEEE80211_SHORT_GI);
        break;
   case IEEE80211_PARAM_SHORTPREAMBLE:
        param[0] = wlan_get_param(vap, IEEE80211_SHORT_PREAMBLE);
        break;


    /*
    * Support to Mcast Enhancement
    */
#if ATH_SUPPORT_IQUE
    case IEEE80211_PARAM_ME:
        param[0] = wlan_get_param(vap, IEEE80211_ME);
        break;
    case IEEE80211_PARAM_MEDUMP:
        param[0] = wlan_get_param(vap, IEEE80211_MEDUMP);
        break;
    case IEEE80211_PARAM_MEDEBUG:
        param[0] = wlan_get_param(vap, IEEE80211_MEDEBUG);
        break;
    case IEEE80211_PARAM_ME_SNOOPLENGTH:
        param[0] = wlan_get_param(vap, IEEE80211_ME_SNOOPLENGTH);
        break;
    case IEEE80211_PARAM_ME_TIMER:
        param[0] = wlan_get_param(vap, IEEE80211_ME_TIMER);
        break;
    case IEEE80211_PARAM_ME_TIMEOUT:
        param[0] = wlan_get_param(vap, IEEE80211_ME_TIMEOUT);
        break;
    case IEEE80211_PARAM_HBR_TIMER:
        param[0] = wlan_get_param(vap, IEEE80211_HBR_TIMER);
        break;
    case IEEE80211_PARAM_HBR_STATE:
        wlan_get_hbrstate(vap);
        param[0] = 0;
        break;
    case IEEE80211_PARAM_ME_DROPMCAST:
        param[0] = wlan_get_param(vap, IEEE80211_ME_DROPMCAST);
        break;
    case IEEE80211_PARAM_ME_SHOWDENY:
        param[0] = wlan_get_param(vap, IEEE80211_ME_SHOWDENY);
        break;
    case IEEE80211_PARAM_GETIQUECONFIG:
        param[0] = wlan_get_param(vap, IEEE80211_IQUE_CONFIG);
        break;
#endif /*ATH_SUPPORT_IQUE*/

#if  ATH_SUPPORT_AOW
    case IEEE80211_PARAM_SWRETRIES:
        param[0] = wlan_get_aow_param(vap, IEEE80211_AOW_SWRETRIES);
        break;
    case IEEE80211_PARAM_AOW_LATENCY:
        param[0] = wlan_get_aow_param(vap, IEEE80211_AOW_LATENCY);
        break;
    case IEEE80211_PARAM_AOW_PLAY_LOCAL:
        param[0] = wlan_get_aow_param(vap, IEEE80211_AOW_PLAY_LOCAL);
        break;
    case IEEE80211_PARAM_AOW_STATS:
        param[0] = wlan_get_aow_param(vap, IEEE80211_AOW_STATS);
        break;
    case IEEE80211_PARAM_AOW_ESTATS:
        param[0] = wlan_get_aow_param(vap, IEEE80211_AOW_ESTATS);
        break;
    case IEEE80211_PARAM_AOW_INTERLEAVE:
        param[0] = wlan_get_aow_param(vap, IEEE80211_AOW_INTERLEAVE);
        break;
    case IEEE80211_PARAM_AOW_ER:
        param[0] = wlan_get_aow_param(vap, IEEE80211_AOW_ER);
        break;
    case IEEE80211_PARAM_AOW_EC:
        param[0] = wlan_get_aow_param(vap, IEEE80211_AOW_EC);
        break;
    case IEEE80211_PARAM_AOW_EC_FMAP:
        param[0] = wlan_get_aow_param(vap, IEEE80211_AOW_EC_FMAP);
        break;
    case IEEE80211_PARAM_AOW_ES:
        param[0] = wlan_get_aow_param(vap, IEEE80211_AOW_ES);
        break;
    case IEEE80211_PARAM_AOW_ESS:
        param[0] = wlan_get_aow_param(vap, IEEE80211_AOW_ESS);
        break;
    case IEEE80211_PARAM_AOW_LIST_AUDIO_CHANNELS:
        param[0] = wlan_get_aow_param(vap, IEEE80211_AOW_LIST_AUDIO_CHANNELS);
        break;
        
#endif /*ATH_SUPPORT_AOW*/

#if UMAC_SUPPORT_RPTPLACEMENT
    case IEEE80211_PARAM_CUSTPROTO_ENABLE:
        param[0] = ieee80211_rptplacement_get_param(vap, IEEE80211_RPT_CUSTPROTO_ENABLE);
        break;
    case IEEE80211_PARAM_GPUTCALC_ENABLE:
        param[0] = ieee80211_rptplacement_get_param(vap, IEEE80211_RPT_GPUTCALC_ENABLE);
        break;
    case IEEE80211_PARAM_DEVUP:
        param[0] = ieee80211_rptplacement_get_param(vap, IEEE80211_RPT_DEVUP);
        break;
    case IEEE80211_PARAM_MACDEV:
        param[0] = ieee80211_rptplacement_get_param(vap, IEEE80211_RPT_MACDEV);
        break;
    case IEEE80211_PARAM_MACADDR1:
        param[0] = ieee80211_rptplacement_get_param(vap, IEEE80211_RPT_MACADDR1);
        break;
    case IEEE80211_PARAM_MACADDR2:
        param[0] = ieee80211_rptplacement_get_param(vap, IEEE80211_RPT_MACADDR2);
        break;
    case IEEE80211_PARAM_GPUTMODE:
        param[0] = ieee80211_rptplacement_get_param(vap, IEEE80211_RPT_GPUTMODE);
        break;
    case IEEE80211_PARAM_TXPROTOMSG:
        param[0] = ieee80211_rptplacement_get_param(vap, IEEE80211_RPT_TXPROTOMSG);
        break;
    case IEEE80211_PARAM_RXPROTOMSG:
        param[0] = ieee80211_rptplacement_get_param(vap, IEEE80211_RPT_RXPROTOMSG);
        break;
    case IEEE80211_PARAM_STATUS:
        param[0] = ieee80211_rptplacement_get_param(vap, IEEE80211_RPT_STATUS);
        break;
    case IEEE80211_PARAM_ASSOC:
        param[0] = ieee80211_rptplacement_get_param(vap, IEEE80211_RPT_ASSOC);
        break;
    case IEEE80211_PARAM_NUMSTAS:
        param[0] = ieee80211_rptplacement_get_param(vap, IEEE80211_RPT_NUMSTAS);
        break;
    case IEEE80211_PARAM_STA1ROUTE:
        param[0] = ieee80211_rptplacement_get_param(vap, IEEE80211_RPT_STA1ROUTE);
        break;
    case IEEE80211_PARAM_STA2ROUTE:
        param[0] = ieee80211_rptplacement_get_param(vap, IEEE80211_RPT_STA2ROUTE);
        break;
    case IEEE80211_PARAM_STA3ROUTE:
        param[0] = ieee80211_rptplacement_get_param(vap, IEEE80211_RPT_STA3ROUTE);
        break;
    case IEEE80211_PARAM_STA4ROUTE:
        param[0] = ieee80211_rptplacement_get_param(vap, IEEE80211_RPT_STA4ROUTE);
#endif

#if UMAC_SUPPORT_TDLS
    case IEEE80211_PARAM_TDLS_MACADDR1:
        param[0] = wlan_get_param(vap, IEEE80211_TDLS_MACADDR1);
        break;
    case IEEE80211_PARAM_TDLS_MACADDR2:
        param[0] = wlan_get_param(vap, IEEE80211_TDLS_MACADDR2);
        break;
    case IEEE80211_PARAM_TDLS_ACTION:
        param[0] = wlan_get_param(vap, IEEE80211_TDLS_ACTION);
        break;
#endif

    case IEEE80211_PARAM_COUNTRYCODE:
        param[0] = wlan_get_device_param(ic, IEEE80211_DEVICE_COUNTRYCODE);
        break;
    case IEEE80211_PARAM_11N_RATE:
        param[0] = wlan_get_param(vap, IEEE80211_FIXED_RATE);
        printk("Getting Rate Series: %x\n",param[0]);
        break;
    case IEEE80211_PARAM_11N_RETRIES:
        param[0] = wlan_get_param(vap, IEEE80211_FIXED_RETRIES);
        printk("Getting Retry Series: %x\n",param[0]);
        break;
    case IEEE80211_PARAM_MCAST_RATE:
        param[0] = wlan_get_param(vap, IEEE80211_MCAST_RATE);
        break;
    case IEEE80211_PARAM_CCMPSW_ENCDEC:
        param[0] = vap->iv_ccmpsw_seldec;
        break;
    case IEEE80211_PARAM_UAPSDINFO:
        param[0] = wlan_get_param(vap, IEEE80211_FEATURE_UAPSD);
        break;
    case IEEE80211_PARAM_NETWORK_SLEEP:
        param[0]= (u_int32_t)wlan_get_powersave(vap);
        break;
#ifdef ATHEROS_LINUX_PERIODIC_SCAN
    case IEEE80211_PARAM_PERIODIC_SCAN:
        param[0] = osifp->os_periodic_scan_period;
        break;
#endif
#if ATH_SW_WOW
    case IEEE80211_PARAM_SW_WOW:
        param[0] = wlan_get_wow(vap);
        break;
#endif
    case IEEE80211_PARAM_AMPDU:
        param[0] = ((ic->ic_flags_ext & IEEE80211_FEXT_AMPDU) != 0);
        break;

#if ATH_SUPPORT_WPA_SUPPLICANT_CHECK_TIME
	case IEEE80211_PARAM_REJOINT_ATTEMP_TIME:
		param[0] = wlan_get_param(vap,IEEE80211_REJOINT_ATTEMP_TIME);
		break;
#endif

    case IEEE80211_PARAM_COUNTRY_IE:
        param[0] = wlan_get_param(vap, IEEE80211_FEATURE_IC_COUNTRY_IE);
        break;

    case IEEE80211_PARAM_CHANBW:
        switch(ic->ic_chanbwflag)
        {
        case IEEE80211_CHAN_HALF:
            param[0] = 1;
            break;
        case IEEE80211_CHAN_QUARTER:
            param[0] = 2;
            break;
        default:
            param[0] = 0;
            break;
        }
        break;
    case IEEE80211_PARAM_MFP_TEST:
        param[0] = wlan_get_param(vap, IEEE80211_FEATURE_MFP_TEST);
        break;

#if UMAC_SUPPORT_TDLS
    case IEEE80211_PARAM_TDLS_ENABLE:
        param[0] = vap->iv_ath_cap & IEEE80211_ATHC_TDLS?1:0;
        break;
#ifdef CONFIG_RCPI
        case IEEE80211_PARAM_TDLS_GET_RCPI:
            /* write the values from vap */
            param[0] = vap->iv_ic->ic_tdls->hithreshold;
            param[1] = vap->iv_ic->ic_tdls->lothreshold;
            param[2] = vap->iv_ic->ic_tdls->margin;
            printf("getparam:rcpi: hithreshold = %d \n", param[0]);
            printf("getparam:rcpi: lothreshold = %d \n", param[1]);
            printf("getparam:rcpi: margin = %d \n", param[2]);
        break;
#endif /* CONFIG_RCPI */
#endif /* UMAC_SUPPORT_TDLS */
    case IEEE80211_PARAM_INACT:
        param[0] = wlan_get_param(vap, IEEE80211_CFG_INACT);
        break;
    case IEEE80211_PARAM_INACT_AUTH:
        param[0] = wlan_get_param(vap, IEEE80211_CFG_INACT_AUTH);
        break;
    case IEEE80211_PARAM_INACT_INIT:
        param[0] = wlan_get_param(vap, IEEE80211_CFG_INACT_INIT);
        break;
    case IEEE80211_PARAM_PWRTARGET:
        param[0] = wlan_get_param(vap, IEEE80211_PWRTARGET);
        break;
    case IEEE80211_PARAM_ABOLT:
        param[0] = wlan_get_param(vap, IEEE80211_ABOLT);
        break;
    case IEEE80211_PARAM_COMPRESSION:
        param[0] = wlan_get_param(vap, IEEE80211_COMP);
        break;
    case IEEE80211_PARAM_FF:
        param[0] = wlan_get_param(vap, IEEE80211_FF);
        break;
    case IEEE80211_PARAM_TURBO:
        param[0] = wlan_get_param(vap, IEEE80211_TURBO);
        break;
    case IEEE80211_PARAM_BURST:
        param[0] = wlan_get_param(vap, IEEE80211_BURST);
        break;
    case IEEE80211_PARAM_AR:
        param[0] = wlan_get_param(vap, IEEE80211_AR);
        break;
    case IEEE80211_PARAM_SLEEP:
        param[0] = wlan_get_param(vap, IEEE80211_SLEEP);
        break;
    case IEEE80211_PARAM_EOSPDROP:
        param[0] = wlan_get_param(vap, IEEE80211_EOSPDROP);
        break;
    case IEEE80211_PARAM_MARKDFS:
		param[0] = wlan_get_param(vap, IEEE80211_MARKDFS);
        break;
    case IEEE80211_PARAM_WDS_AUTODETECT:
        param[0] = wlan_get_param(vap, IEEE80211_WDS_AUTODETECT);
        break;
    case IEEE80211_PARAM_WEP_TKIP_HT:
        param[0] = wlan_get_param(vap, IEEE80211_WEP_TKIP_HT);
        break;
    /*
    ** Support for returning the radio number
    */
    case IEEE80211_PARAM_ATH_RADIO:
		param[0] = wlan_get_param(vap, IEEE80211_ATH_RADIO);
        break;
    case IEEE80211_PARAM_IGNORE_11DBEACON:
        param[0] = wlan_get_param(vap, IEEE80211_IGNORE_11DBEACON);
        break;

#if ATH_RXBUF_RECYCLE   
    case IEEE80211_PARAM_RXBUF_LIFETIME:
        param[0] = ic->ic_osdev->rxbuf_lifetime;
        break;
#endif

#if ATH_SUPPORT_WAPI
    case IEEE80211_PARAM_WAPIREKEY_USK:
        param[0] = wlan_get_wapirekey_unicast(vap);
        break;
    case IEEE80211_PARAM_WAPIREKEY_MSK:
        param[0] = wlan_get_wapirekey_multicast(vap);
        break;		
#endif
#ifdef __CARRIER_PLATFORM__
    case IEEE80211_PARAM_PLTFRM_PRIVATE:
        param[0] = wlan_pltfrm_get_param(vap);
        break;
#endif 

#if UMAC_SUPPORT_VI_DBG
    case IEEE80211_PARAM_DBG_CFG:
        param[0] = ieee80211_vi_dbg_get_param(vap, IEEE80211_VI_DBG_CFG);
        break;
    case IEEE80211_PARAM_DBG_NUM_STREAMS:
        param[0] = ieee80211_vi_dbg_get_param(vap, IEEE80211_VI_DBG_NUM_STREAMS);
        break;
    case IEEE80211_PARAM_STREAM_NUM:
        param[0] = ieee80211_vi_dbg_get_param(vap, IEEE80211_VI_STREAM_NUM);
	    break;
    case IEEE80211_PARAM_DBG_NUM_MARKERS:
        param[0] = ieee80211_vi_dbg_get_param(vap, IEEE80211_VI_DBG_NUM_MARKERS);
        break;
    case IEEE80211_PARAM_MARKER_NUM:
        param[0] = ieee80211_vi_dbg_get_param(vap, IEEE80211_VI_MARKER_NUM);
	    break;
    case IEEE80211_PARAM_MARKER_OFFSET_SIZE:
        param[0] = ieee80211_vi_dbg_get_param(vap, IEEE80211_VI_MARKER_OFFSET_SIZE);  
        break;
    case IEEE80211_PARAM_MARKER_MATCH:
        param[0] = ieee80211_vi_dbg_get_param(vap, IEEE80211_VI_MARKER_MATCH);  
        break;
    case IEEE80211_PARAM_RXSEQ_OFFSET_SIZE:
        param[0] = ieee80211_vi_dbg_get_param(vap, IEEE80211_VI_RXSEQ_OFFSET_SIZE);  
        break;
    case IEEE80211_PARAM_RX_SEQ_RSHIFT:
        param[0] = ieee80211_vi_dbg_get_param(vap, IEEE80211_VI_RX_SEQ_RSHIFT);
        break;
    case IEEE80211_PARAM_RX_SEQ_MAX:
        param[0] = ieee80211_vi_dbg_get_param(vap, IEEE80211_VI_RX_SEQ_MAX);
        break;
    case IEEE80211_PARAM_RX_SEQ_DROP:
        param[0] = ieee80211_vi_dbg_get_param(vap, IEEE80211_VI_RX_SEQ_DROP);
        break;	        
    case IEEE80211_PARAM_TIME_OFFSET_SIZE:
        param[0] = ieee80211_vi_dbg_get_param(vap, IEEE80211_VI_TIME_OFFSET_SIZE);  
        break;
    case IEEE80211_PARAM_RESTART:
        param[0] = ieee80211_vi_dbg_get_param(vap, IEEE80211_VI_RESTART);
        break;
    case IEEE80211_PARAM_RXDROP_STATUS:
        param[0] = ieee80211_vi_dbg_get_param(vap, IEEE80211_VI_RXDROP_STATUS);
        break;
#endif
#if ATH_SUPPORT_FLOWMAC_MODULE
    case IEEE80211_PARAM_FLOWMAC:
        param[0] = vap->iv_flowmac;
        break;
#endif
#if ATH_SUPPORT_IBSS_DFS
    case IEEE80211_PARAM_IBSS_DFS_PARAM:
        param[0] = vap->iv_ibss_dfs_csa_threshold << 16 |
                   vap->iv_ibss_dfs_csa_measrep_limit << 8 |
                   vap->iv_ibss_dfs_enter_recovery_threshold_in_tbtt;
        printk("channel swith time %d measurement report %d recover time %d \n",
                 vap->iv_ibss_dfs_csa_threshold,
                 vap->iv_ibss_dfs_csa_measrep_limit ,
                 vap->iv_ibss_dfs_enter_recovery_threshold_in_tbtt);
        break;
#endif
#if UMAC_SUPPORT_SMARTANTENNA
        case IEEE80211_PARAM_SMARTANT_TRAIN_MODE:
            param[0] = ieee80211_smartantenna_get_param(vap, IEEE80211_SMARTANT_TRAIN_MODE);
            break;
        case IEEE80211_PARAM_SMARTANT_TRAIN_TYPE:
            param[0] = ieee80211_smartantenna_get_param(vap, IEEE80211_SMARTANT_TRAIN_TYPE);
            break;
        case IEEE80211_PARAM_SMARTANT_PKT_LEN:
            param[0] = ieee80211_smartantenna_get_param(vap, IEEE80211_SMARTANT_PKT_LEN);
            break;
        case IEEE80211_PARAM_SMARTANT_NUM_PKTS:
            param[0] = ieee80211_smartantenna_get_param(vap, IEEE80211_SMARTANT_NUM_PKTS);
            break;
        case IEEE80211_PARAM_SMARTANT_TRAIN_START:
            param[0] = ieee80211_smartantenna_get_param(vap, IEEE80211_SMARTANT_TRAIN_START);
            break;
        case IEEE80211_PARAM_SMARTANT_NUM_ITR:
            param[0] = ieee80211_smartantenna_get_param(vap, IEEE80211_SMARTANT_NUM_ITR);
            break;
#endif            
#if UMAC_SUPPORT_INTERNALANTENNA            
        case IEEE80211_PARAM_SMARTANT_CURRENT_ANTENNA:
            ieee80211_display_current_antenna(vap);            
            param[0]= 255; 
            break;
        case IEEE80211_PARAM_SMARTANT_DEFAULT_ANTENNA:
            param[0] = ic->ic_get_default_antenna(ic);
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,"Default RX Antenna:%d \n", param[0]);
            break;
#endif            
#if UMAC_SUPPORT_SMARTANTENNA            
        case IEEE80211_PARAM_SMARTANT_AUTOMATION:
            param[0] = ieee80211_smartantenna_get_param(vap, IEEE80211_SMARTANT_AUTOMATION);
            break;
        case IEEE80211_PARAM_SMARTANT_RETRAIN:
            param[0] = ieee80211_smartantenna_get_param(vap, IEEE80211_SMARTANT_RETRAIN);
            break;
#endif            
#ifdef ATH_SUPPORT_TxBF
    case IEEE80211_PARAM_TXBF_AUTO_CVUPDATE:
        param[0] = wlan_get_param(vap, IEEE80211_TXBF_AUTO_CVUPDATE);
        break;
    case IEEE80211_PARAM_TXBF_CVUPDATE_PER:
        param[0] = wlan_get_param(vap, IEEE80211_TXBF_CVUPDATE_PER);
        break;
#endif
    case IEEE80211_PARAM_SCAN_BAND:
        param[0] = osifp->os_scan_band;
        break;
    case IEEE80211_PARAM_ROAMING:
        param[0] = ic->ic_roaming;
        break;
    default:
#if ATHEROS_LINUX_P2P_DRIVER
        retv = ieee80211_ioctl_getp2p(dev, info, w, extra);
#else
        retv = EOPNOTSUPP;
#endif
    }

    if (retv) {
        printk("%s : parameter 0x%x not supported \n", __func__, param[0]);
        return -EOPNOTSUPP;
    }

    return retv;
#ifdef notyet
    struct ieee80211vap *vap = NETDEV_TO_VAP(dev);
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_rsnparms *rsn = &vap->iv_bss->ni_rsn;
    int *param = (int *) extra;
    u_int m;
    switch (param[0])
    {
    case IEEE80211_PARAM_ROAMING:
        param[0] = ic->ic_roaming;
        break;
    case IEEE80211_PARAM_DROPUNENC_EAPOL:
    param[0] = IEEE80211_VAP_DROPUNENC_EAPOL(vap);
        break;


    case IEEE80211_PARAM_WMM_AGGRMODE:
        param[0] = (vap->iv_ath_cap & IEEE80211_ATHC_WME) != 0;
        break;
//    case IEEE80211_PARAM_XR:
//        param[0] = (vap->iv_ath_cap & IEEE80211_ATHC_XR) != 0;
//        break;
    case IEEE80211_PARAM_VAP_IND:
        param[0] = ((vap->iv_flags_ext & IEEE80211_FEXT_VAP_IND) == IEEE80211_FEXT_VAP_IND);
        break;
    case IEEE80211_PARAM_BGSCAN_IDLE:
        param[0] = vap->iv_bgscanidle*HZ/1000;  /* ms */
        break;
    case IEEE80211_PARAM_BGSCAN_INTERVAL:
        param[0] = vap->iv_bgscanintvl/HZ;  /* seconds */
        break;
    case IEEE80211_PARAM_COVERAGE_CLASS:
        param[0] = ic->ic_coverageclass;
        break;
    case IEEE80211_PARAM_REGCLASS:
        param[0] = (ic->ic_flags_ext & IEEE80211_FEXT_REGCLASS) != 0;
        break;
    case IEEE80211_PARAM_SCANVALID:
        param[0] = vap->iv_scanvalid/HZ;    /* seconds */
        break;
    case IEEE80211_PARAM_ROAM_RSSI_11A:
        param[0] = vap->iv_roam.rssi11a;
        break;
    case IEEE80211_PARAM_ROAM_RSSI_11B:
        param[0] = vap->iv_roam.rssi11bOnly;
        break;
    case IEEE80211_PARAM_ROAM_RSSI_11G:
        param[0] = vap->iv_roam.rssi11b;
        break;
    case IEEE80211_PARAM_ROAM_RATE_11A:
        param[0] = vap->iv_roam.rate11a;
        break;
    case IEEE80211_PARAM_ROAM_RATE_11B:
        param[0] = vap->iv_roam.rate11bOnly;
        break;
    case IEEE80211_PARAM_ROAM_RATE_11G:
        param[0] = vap->iv_roam.rate11b;
        break;
    case IEEE80211_PARAM_FAST_CC:
        param[0] = ((ic->ic_flags_ext & IEEE80211_FAST_CC) != 0);
        break;
    case IEEE80211_PARAM_AMPDU_LIMIT:
        param[0] = ic->ic_ampdu_limit;
        break;
    case IEEE80211_PARAM_AMPDU_DENSITY:
        param[0] = ic->ic_ampdu_density;
        break;
    case IEEE80211_PARAM_AMPDU_SUBFRAMES:
        param[0] = ic->ic_ampdu_subframes;
        break;
    case IEEE80211_PARAM_AMSDU:
        param[0] = ((ic->ic_flags_ext & IEEE80211_FEXT_AMSDU) != 0);
        break;
    case IEEE80211_PARAM_AMSDU_LIMIT:
        param[0] = ic->ic_amsdu_limit;
        break;
    case IEEE80211_PARAM_TX_CHAINMASK:
        param[0] = ic->ic_tx_chainmask;
        break;
    case IEEE80211_PARAM_TX_CHAINMASK_LEGACY:
        param[0] = ic->ic_tx_chainmask_legacy;
        break;
    case IEEE80211_PARAM_RX_CHAINMASK:
        param[0] =  ic->ic_rx_chainmask;
        break;
    case IEEE80211_PARAM_RTSCTS_RATECODE:
        param[0] = ic->ic_rtscts_ratecode;
        break;
    case IEEE80211_PARAM_HT_PROTECTION:
        param[0] = ((ic->ic_flags_ext & IEEE80211_FEXT_HTPROT) != 0);
        break;
    case IEEE80211_PARAM_SHORTPREAMBLE:
        param[0] = (ic->ic_caps & IEEE80211_C_SHPREAMBLE)!=0;
        break;
#if(0)
    case IEEE80211_PARAM_RADIO:
    if (ic->ic_flags_ext & IEEE80211_FEXT_RADIO)
        param[0] = 1;
    else
        param[0] = 0;
    break;
#endif
    case IEEE80211_PARAM_NETWORK_SLEEP:
    switch(vap->iv_pwrsave.ips_sta_psmode) {
            case IEEE80211_PWRSAVE_LOW:
                param[0] = 1;
            break;
            case IEEE80211_PWRSAVE_NORMAL:
                param[0] = 2;
            break;
            case IEEE80211_PWRSAVE_MAXIMUM:
            param[0] = 3;
            break;
            default:
                param[0] = 0;
            break;
    }
    break;

    case IEEE80211_PARAM_NO_EDGE_CH:
        param[0] = (vap->iv_flags_ext & IEEE80211_FEXT_NO_EDGE_CH);
        break;

    case IEEE80211_PARAM_STA_FORWARD:
        param[0] = ((vap->iv_flags_ext & IEEE80211_C_STA_FORWARD) == IEEE80211_C_STA_FORWARD);
        break;
    case IEEE80211_PARAM_SLEEP_PRE_SCAN:
        param[0] = vap->iv_sleep_pre_scan;
        break;
    case IEEE80211_PARAM_SCAN_PRE_SLEEP:
        param[0] = vap->iv_scan_pre_sleep;
        break;

    default:
        return -EOPNOTSUPP;
    }
    return 0;
#endif /* notyet */
}

static int
ieee80211_ioctl_setoptie(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    union iwreq_data *u = w;
    void *ie;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_SETOPTIE, "setoptie") ;
    /*
    * NB: Doing this for ap operation could be useful (e.g. for
    *     WPA and/or WME) except that it typically is worthless
    *     without being able to intervene when processing
    *     association response frames--so disallow it for now.
    */
#ifndef ATH_SUPPORT_P2P
    if (osifp->os_opmode != IEEE80211_M_STA && osifp->os_opmode != IEEE80211_M_IBSS)
        return -EINVAL;
#endif

    /* NB: data.length is validated by the wireless extensions code */
    ie = OS_MALLOC(osifp->os_handle, u->data.length, GFP_KERNEL);
    if (ie == NULL)
        return -ENOMEM;
    memcpy(ie, extra, u->data.length);
    wlan_mlme_set_optie(vap, ie, u->data.length);
    OS_FREE(ie);
    return 0;
}

static int
ieee80211_ioctl_getoptie(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    u_int32_t opt_ielen;
    union iwreq_data *u = w;
    int rc = 0;
    debug_print_ioctl(dev->name, IEEE80211_IOCTL_GETOPTIE, "getoptie") ;
    opt_ielen = u->data.length;
    rc = wlan_mlme_get_optie(vap, (u_int8_t *)extra, &opt_ielen, opt_ielen);
    if (!rc) {
        u->data.length = opt_ielen;
    }
    return rc;
}

static int
ieee80211_ioctl_setappiebuf(struct net_device *dev,
    struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct ieee80211req_getset_appiebuf *iebuf =      \
            ( struct ieee80211req_getset_appiebuf *)extra;
    int rc = 0;


    debug_print_ioctl(dev->name, IEEE80211_IOCTL_SET_APPIEBUF, "setappiebuf") ;
    if (iebuf->app_buflen > IEEE80211_APPIE_MAX)
        return -EINVAL;

    switch(iebuf->app_frmtype)
    {
    case IEEE80211_APPIE_FRAME_BEACON:
        rc = wlan_mlme_set_appie(vap, IEEE80211_FRAME_TYPE_BEACON, iebuf->app_buf, iebuf->app_buflen);
        /*currently hostapd does not support
        * multiple ie's in application buffer
        */
        if (iebuf->app_buflen && iswpsoui(iebuf->app_buf))
            wlan_set_param(vap,IEEE80211_WPS_MODE,1);
        /* EV : 89216 :
         * WPS2.0 : Reset back WPS Mode value to handle 
         * hostapd abort, termination Ignore MAC Address 
         * Filtering if WPS Enabled
         * return 1 to report success
         */
        else if((iebuf->app_buflen == 0) && vap->iv_wps_mode)
            wlan_set_param(vap,IEEE80211_WPS_MODE,0);
        break;
    case IEEE80211_APPIE_FRAME_PROBE_REQ:
#ifndef ATH_SUPPORT_P2P
        if (osifp->os_opmode != IEEE80211_M_STA &&
            (u_int8_t)osifp->os_opmode != IEEE80211_M_P2P_DEVICE)
            return -EINVAL;
#endif
        rc = wlan_mlme_set_appie(vap, IEEE80211_FRAME_TYPE_PROBEREQ, iebuf->app_buf, iebuf->app_buflen);
        break;
    case IEEE80211_APPIE_FRAME_PROBE_RESP:
#ifndef ATH_SUPPORT_P2P
        if (osifp->os_opmode != IEEE80211_M_HOSTAP  &&
            (u_int8_t)osifp->os_opmode != IEEE80211_M_P2P_DEVICE)
            return -EINVAL;
#endif

#ifdef ATH_SUPPORT_P2P
        if ((u_int8_t)osifp->os_opmode == IEEE80211_M_P2P_GO){
            wlan_p2p_GO_parse_appie(osifp->p2p_go_handle, IEEE80211_FRAME_TYPE_PROBERESP, iebuf->app_buf, iebuf->app_buflen);
            rc = wlan_p2p_GO_set_appie(osifp->p2p_go_handle, IEEE80211_FRAME_TYPE_PROBERESP, iebuf->app_buf, iebuf->app_buflen);
        }else if ((u_int8_t)osifp->os_opmode == IEEE80211_M_P2P_DEVICE){
            wlan_p2p_parse_appie(osifp->p2p_handle, IEEE80211_FRAME_TYPE_PROBERESP, iebuf->app_buf, iebuf->app_buflen);
            rc = wlan_mlme_set_appie(vap, IEEE80211_FRAME_TYPE_PROBERESP, iebuf->app_buf, iebuf->app_buflen);
        }else
#endif
            rc = wlan_mlme_set_appie(vap, IEEE80211_FRAME_TYPE_PROBERESP, iebuf->app_buf, iebuf->app_buflen);

        break;
    case IEEE80211_APPIE_FRAME_ASSOC_REQ:
#ifndef ATH_SUPPORT_P2P
        if(osifp->os_opmode != IEEE80211_M_STA)
            return -EINVAL;
#endif
        rc = wlan_mlme_set_appie(vap, IEEE80211_FRAME_TYPE_ASSOCREQ, iebuf->app_buf, iebuf->app_buflen);
        break;
    case IEEE80211_APPIE_FRAME_ASSOC_RESP:
#ifndef ATH_SUPPORT_P2P
        if(osifp->os_opmode != IEEE80211_M_HOSTAP)
            return -EINVAL;
#endif
        rc = wlan_mlme_set_appie(vap, IEEE80211_FRAME_TYPE_ASSOCRESP, iebuf->app_buf, iebuf->app_buflen);
        break;
#if UMAC_SUPPORT_TDLS
    case IEEE80211_APPIE_FRAME_TDLS_FTIE:
        if(osifp->os_opmode != IEEE80211_M_STA)
            return -EINVAL;
        rc = ieee80211_wpa_tdls_ftie(vap, iebuf->app_buf, iebuf->app_buflen);
        break;
#endif /* UMAC_SUPPORT_TDLS */
    default:
        return -EINVAL;

    }
    return rc;
}

static int
ieee80211_ioctl_getappiebuf(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct ieee80211req_getset_appiebuf *iebuf = \
            ( struct ieee80211req_getset_appiebuf *)extra;
    int rc = 0;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_GET_APPIEBUF, "getappiebuf") ;
    switch(iebuf->app_frmtype)
    {
    case IEEE80211_APPIE_FRAME_BEACON:
#ifndef ATH_SUPPORT_P2P
        if(osifp->os_opmode == IEEE80211_M_STA)
            return -EINVAL;
#endif
        rc = wlan_mlme_get_appie(vap, IEEE80211_FRAME_TYPE_BEACON, iebuf->app_buf, &iebuf->app_buflen, iebuf->app_buflen);
        break;
    case IEEE80211_APPIE_FRAME_PROBE_REQ:
#ifndef ATH_SUPPORT_P2P
        if(osifp->os_opmode != IEEE80211_M_STA)
            return -EINVAL;
#endif
        rc = wlan_mlme_get_appie(vap, IEEE80211_FRAME_TYPE_PROBEREQ, iebuf->app_buf, &iebuf->app_buflen, iebuf->app_buflen);
        break;
    case IEEE80211_APPIE_FRAME_PROBE_RESP:
#ifndef ATH_SUPPORT_P2P
        if(osifp->os_opmode == IEEE80211_M_STA &&
            (u_int8_t)osifp->os_opmode != IEEE80211_M_P2P_DEVICE)
            return -EINVAL;
#endif
        rc = wlan_mlme_get_appie(vap, IEEE80211_FRAME_TYPE_PROBERESP, iebuf->app_buf, &iebuf->app_buflen, iebuf->app_buflen);
        break;
    case IEEE80211_APPIE_FRAME_ASSOC_REQ:
#ifndef ATH_SUPPORT_P2P
        if(osifp->os_opmode != IEEE80211_M_STA)
            return -EINVAL;
#endif
        rc = wlan_mlme_get_appie(vap, IEEE80211_FRAME_TYPE_ASSOCREQ, iebuf->app_buf, &iebuf->app_buflen, iebuf->app_buflen);
        break;
    case IEEE80211_APPIE_FRAME_ASSOC_RESP:
#ifndef ATH_SUPPORT_P2P
        if(osifp->os_opmode == IEEE80211_M_STA)
            return -EINVAL;
#endif
        rc = wlan_mlme_get_appie(vap, IEEE80211_FRAME_TYPE_ASSOCRESP, iebuf->app_buf, &iebuf->app_buflen, iebuf->app_buflen);
        break;
    default:
        return -EINVAL;
    }
    return rc;
}

static int
ieee80211_ioctl_setfilter(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    struct ieee80211req_set_filter *app_filter = ( struct ieee80211req_set_filter *)extra;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_FILTERFRAME, "setfilter") ;
    if ((extra == NULL) || (app_filter->app_filterype & ~IEEE80211_FILTER_TYPE_ALL))
        return -EINVAL;

    osifp->app_filter =  app_filter->app_filterype;

    return 0;
}

#if ATH_SUPPORT_IQUE
static int
ieee80211_ioctl_setrtparams(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    u_int8_t rt_index, per, probe_intvl;
    int *param = (int *) extra;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_SET_RTPARAMS, "setrtparams") ;
    rt_index  = (u_int8_t)param[0];
    per = (u_int8_t)param[1];
    probe_intvl = (u_int8_t)param[2];

    if ((rt_index != 0 && rt_index != 1) || per > 100 ||
        probe_intvl > 100)
    {
        goto error;
    }
    wlan_set_rtparams(vap, rt_index, per, probe_intvl);
    return 0;

error:
    printk("usage: rtparams rt_idx <0|1> per <0..100> probe_intval <0..100>\n");
    return -EINVAL;
}

static int
ieee80211_ioctl_setacparams(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    u_int8_t ac, use_rts, aggrsize_scaling;
    u_int32_t min_kbps;
    int *param = (int *) extra;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_SET_ACPARAMS, "setacparams") ;
    ac  = (u_int8_t)param[0];
    use_rts = (u_int8_t)param[1];
    aggrsize_scaling = (u_int8_t)param[2];
    min_kbps = ((u_int8_t)param[3]) * 1000;

    if (ac > 3 || (use_rts != 0 && use_rts != 1) ||
        aggrsize_scaling > 4 || min_kbps > 250000)
    {
        goto error;
    }

    wlan_set_acparams(vap, ac, use_rts, aggrsize_scaling, min_kbps);
    return 0;

error:
    printk("usage: acparams ac <0|3> RTS <0|1> aggr scaling <0..4> min mbps <0..250>\n");
    return -EINVAL;
}

static int
ieee80211_ioctl_sethbrparams(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    u_int8_t ac, enable, per_low;
    int *param = (int *)extra;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_SET_HBRPARAMS, "sethbrparams") ;
    ac = (u_int8_t)param[0];
    enable = (u_int8_t)param[1];
    per_low = (u_int8_t)param[2];

    if (ac != 2 || (enable != 0 && enable != 1) || per_low >= 50)
    {
        goto hbr_error;
    }
    wlan_set_hbrparams(vap, ac, enable, per_low);
    return 0;

hbr_error:
    printk("usage: hbrparams ac <2> enable <0|1> per_low <0..50>\n");
    return -EINVAL;
}

static int
ieee80211_ioctl_setmedenyentry(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    int *param = (int *)extra;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_SET_MEDENYENTRY, "set_me_denyentry") ;
    wlan_set_me_denyentry(vap, param);
    return 0;
}

#endif /* ATH_SUPPORT_IQUE */

#ifdef notyet
static int
ieee80211_ioctl_setrxtimeout(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct ieee80211com *ic = vap->iv_ic;
    u_int8_t ac, rxtimeout;
    int *param = (int *) extra;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_SET_RXTIMEOUT, "setrxtimeout") ;
    ac  = (u_int8_t)param[0];
    rxtimeout = (u_int8_t)param[1];

    if (ac > 3 || rxtimeout < 10 || rxtimeout > 200)
    {
        goto error;
    }

    ic->ic_set_rxtimeout(ic, ac, rxtimeout);
    return 0;

error:
    printk("usage: rxtimeout ac <0|3> timeout <10..200ms>\n");
    return -EINVAL;
}
#endif /* notyet */

#if UMAC_SUPPORT_NAWDS
static int
ieee80211_ioctl_nawds(struct net_device *dev, struct iwreq *iwr)
{
    wlan_if_t vap = NETDEV_TO_VAP(dev);
    struct ieee80211_wlanconfig config;
    struct ieee80211_wlanconfig_nawds *nawds;

    if (iwr->u.data.length < sizeof(struct ieee80211_wlanconfig))
        return -EFAULT;

    if (_copy_from_user(&config, iwr->u.data.pointer, sizeof(struct ieee80211_wlanconfig))) {
        return -EFAULT;
    }
    nawds = &config.data.nawds;

    switch (config.cmdtype) {
        case IEEE80211_WLANCONFIG_NAWDS_SET_MODE:
            return wlan_nawds_set_param(vap, IEEE80211_NAWDS_PARAM_MODE, &nawds->mode);
        case IEEE80211_WLANCONFIG_NAWDS_SET_DEFCAPS:
            return wlan_nawds_set_param(vap, IEEE80211_NAWDS_PARAM_DEFCAPS, &nawds->defcaps);
        case IEEE80211_WLANCONFIG_NAWDS_SET_OVERRIDE:
            return wlan_nawds_set_param(vap, IEEE80211_NAWDS_PARAM_OVERRIDE, &nawds->override);
        case IEEE80211_WLANCONFIG_NAWDS_SET_ADDR:
            return wlan_nawds_config_mac(vap, nawds->mac, nawds->caps);
        case IEEE80211_WLANCONFIG_NAWDS_CLR_ADDR:
            return wlan_nawds_delete_mac(vap, nawds->mac);
        case IEEE80211_WLANCONFIG_NAWDS_GET:
            wlan_nawds_get_param(vap, IEEE80211_NAWDS_PARAM_MODE, &nawds->mode);
            wlan_nawds_get_param(vap, IEEE80211_NAWDS_PARAM_DEFCAPS, &nawds->defcaps);
            wlan_nawds_get_param(vap, IEEE80211_NAWDS_PARAM_OVERRIDE, &nawds->override);
            if (wlan_nawds_get_mac(vap, nawds->num, &nawds->mac[0], &nawds->caps)) {
                printk("failed to get NAWDS entry %d\n", nawds->num);
            }
            wlan_nawds_get_param(vap, IEEE80211_NAWDS_PARAM_NUM, &nawds->num);
            config.status = IEEE80211_WLANCONFIG_OK;
            if (_copy_to_user(iwr->u.data.pointer, &config, sizeof(struct ieee80211_wlanconfig))) {
                return -EFAULT;
            }
            break;
        default:
            return -ENXIO;
    }
    return 0;
}
#endif

static int
ieee80211_ioctl_config_generic(struct net_device *dev, struct iwreq *iwr)
{
    IEEE80211_WLANCONFIG_CMDTYPE cmdtype;

    /* retrieve sub-command first */
    if (iwr->u.data.length < sizeof(IEEE80211_WLANCONFIG_CMDTYPE))
        return -EFAULT;

    if (_copy_from_user(&cmdtype, iwr->u.data.pointer, sizeof(IEEE80211_WLANCONFIG_CMDTYPE))) {
        return -EFAULT;
    }

    switch (cmdtype) {
#if UMAC_SUPPORT_NAWDS
        case IEEE80211_WLANCONFIG_NAWDS_SET_MODE:
        case IEEE80211_WLANCONFIG_NAWDS_SET_DEFCAPS:
        case IEEE80211_WLANCONFIG_NAWDS_SET_OVERRIDE:
        case IEEE80211_WLANCONFIG_NAWDS_SET_ADDR:
        case IEEE80211_WLANCONFIG_NAWDS_CLR_ADDR:
        case IEEE80211_WLANCONFIG_NAWDS_GET:
            return ieee80211_ioctl_nawds(dev, iwr);
#endif
        default:
            return -EOPNOTSUPP;
    }

    return -EOPNOTSUPP;
}

static int
ieee80211_ioctl_sendaddba(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    wlan_dev_t ic = wlan_vap_get_devhandle(vap);
    struct ieee80211_addba_delba_request ad;
    int *param = (int *) extra;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_SENDADDBA, "sendaddba") ;
    if (wlan_get_device_param(ic, IEEE80211_DEVICE_ADDBA_MODE) == ADDBA_MODE_AUTO) {
        printk("%s(): ADDBA mode is AUTO\n", __func__);
        return -EINVAL;
    }

    /* Valid TID values are 0 through 15 */
    if (param[1] < 0 || param[1] > (IEEE80211_TID_SIZE - 2)) {
        printk("%s(): Invalid TID value\n", __func__);
        return -EINVAL;
    }
    ad.action = ADDBA_SEND;
    ad.ic = ic;
    ad.aid  = param[0];
    ad.tid  = param[1];
    ad.arg1 = param[2];

    wlan_iterate_station_list(vap, wlan_addba_request_handler, &ad);

    return 0;
}

static int
ieee80211_ioctl_senddelba(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    wlan_dev_t ic = wlan_vap_get_devhandle(vap);
    struct ieee80211_addba_delba_request ad;
    int *param = (int *) extra;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_SENDDELBA, "senddelba") ;
    if ((param[2] != 1) && (param[2] != 0)) {
        return -EINVAL;
    }

    if (wlan_get_device_param(ic, IEEE80211_DEVICE_ADDBA_MODE) == ADDBA_MODE_AUTO) {
        printk("%s(): ADDBA mode is AUTO\n", __func__);
        return -EINVAL;
    }

    /* Valid TID values are 0 through 15 */
    if (param[1] < 0 || param[1] > (IEEE80211_TID_SIZE - 2)) {
        printk("%s(): Invalid TID value\n", __func__);
        return -EINVAL;
    }
    ad.action = DELBA_SEND;
    ad.ic = ic;
    ad.aid = param[0];
    ad.tid  = param[1];
    ad.arg1 = param[2];
    ad.arg2 = param[3];

    wlan_iterate_station_list(vap, wlan_addba_request_handler, &ad);

    return 0;
}

static int
ieee80211_ioctl_getaddbastatus(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    wlan_dev_t ic = wlan_vap_get_devhandle(vap);
    struct ieee80211_addba_delba_request ad;
    int *param = (int *) extra;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_GETADDBASTATUS, "getaddbastatus") ;
    /* Valid TID values are 0 through 15 */
    if (param[1] < 0 || param[1] > (IEEE80211_TID_SIZE - 2)) {
        printk("%s(): Invalid TID value\n", __func__);
        return -EINVAL;
    }
    memset(&ad, 0, sizeof(ad));

    ad.action = ADDBA_STATUS;
    ad.ic = ic;
    ad.aid = param[0];
    ad.tid = param[1];

    wlan_iterate_station_list(vap, wlan_addba_request_handler, &ad);

    param[0] = ad.status;
    if (ad.status == 0xFFFF) {
        printk("Addba status IDLE\n");
    }

    return 0;
}

static int
ieee80211_ioctl_setaddbaresponse(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    wlan_dev_t ic = wlan_vap_get_devhandle(vap);
    struct ieee80211_addba_delba_request ad;
    int *param = (int *) extra;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_SET_ADDBARESP, "setaddbaresponse") ;
    if (wlan_get_device_param(ic, IEEE80211_DEVICE_ADDBA_MODE) == ADDBA_MODE_AUTO) {
        printk("%s(): ADDBA mode is AUTO\n", __func__);
        return -EINVAL;
    }

    /* Valid TID values are 0 through 15 */
    if (param[1] < 0 || param[1] > (IEEE80211_TID_SIZE - 2)) {
        printk("%s(): Invalid TID value\n", __func__);
        return -EINVAL;
    }

    ad.action = ADDBA_RESP;
    ad.ic = ic;
    ad.aid = param[0];
    ad.tid  = param[1];
    ad.arg1 = param[2];

    wlan_iterate_station_list(vap, wlan_addba_request_handler, &ad);
    return 0;
}

static int
ieee80211_ioctl_setkey(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct ieee80211req_key *ik = (struct ieee80211req_key *)extra;
    ieee80211_keyval key_val;
    u_int16_t kid;
    int error, i;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_SETKEY, "setkey") ;

    /* NB: cipher support is verified by ieee80211_crypt_newkey */
    if (ik->ik_keylen > sizeof(ik->ik_keydata))
    {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"%s: KeyLen too Big\n", __func__);
        return -E2BIG;
    }
    if (ik->ik_keylen == 0) {
        /* zero length keys will only set default key id if flags are set*/
        if ((ik->ik_flags & IEEE80211_KEY_DEFAULT) && (ik->ik_keyix != IEEE80211_KEYIX_NONE)) {
            /* default xmit key */
            wlan_set_default_keyid(vap, ik->ik_keyix);
            return 0;
        }
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"%s: Zero length key\n", __func__);
        return -EINVAL;
    }
    kid = ik->ik_keyix;
    if (kid == IEEE80211_KEYIX_NONE)
    {
        /* XXX unicast keys currently must be tx/rx */
        if (ik->ik_flags != (IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV))
        {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"%s: Too Many Flags: %x\n",
                                    __func__,
                                    ik->ik_flags);
			ik->ik_flags = (IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV);
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"%s: Too Many Flags: %x\n",
                                   __func__,
                                   ik->ik_flags);
            //return -EINVAL;
        }

        if (osifp->os_opmode == IEEE80211_M_STA ||
            (u_int8_t)osifp->os_opmode == IEEE80211_M_P2P_CLIENT)
        {
            for (i = 0; i < IEEE80211_ADDR_LEN; i++) {
                if (ik->ik_macaddr[i] != 0) {
                    break;
                }
            }
            if (i == IEEE80211_ADDR_LEN) {
                memset(ik->ik_macaddr, 0xFF, IEEE80211_ADDR_LEN);
            }
        }
    }
    else
    {
        if (kid >= IEEE80211_WEP_NKID)
            return -EINVAL;
        /* XXX auto-add group key flag until applications are updated */
        if ((ik->ik_flags & IEEE80211_KEY_XMIT) == 0)   /* XXX */ {
            ik->ik_flags |= IEEE80211_KEY_GROUP;    /* XXX */
        }
        else {
            if (!IEEE80211_IS_MULTICAST(ik->ik_macaddr) &&
                ((ik->ik_type == IEEE80211_CIPHER_TKIP) ||
                (ik->ik_type == IEEE80211_CIPHER_AES_CCM))) {
            kid = IEEE80211_KEYIX_NONE;
            }
        }
    }
    memset(&key_val,0, sizeof(ieee80211_keyval));


    if ( (ik->ik_flags & (IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV))
        == (IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV)) {
        key_val.keydir = IEEE80211_KEY_DIR_BOTH;
    } else if (ik->ik_flags & IEEE80211_KEY_XMIT) {
        key_val.keydir = IEEE80211_KEY_DIR_TX;
    } else if (ik->ik_flags & IEEE80211_KEY_RECV) {
        key_val.keydir = IEEE80211_KEY_DIR_RX;
    }
    key_val.keylen  = ik->ik_keylen;
    if (key_val.keylen > IEEE80211_KEYBUF_SIZE)
        key_val.keylen  = IEEE80211_KEYBUF_SIZE;
    key_val.rxmic_offset = IEEE80211_KEYBUF_SIZE + 8;
    key_val.txmic_offset =  IEEE80211_KEYBUF_SIZE;
    key_val.keytype = ik->ik_type;
    key_val.macaddr = ik->ik_macaddr;
    key_val.keydata = ik->ik_keydata;
    key_val.keyrsc  = ik->ik_keyrsc;
    key_val.keytsc  = ik->ik_keytsc;
#if ATH_SUPPORT_WAPI
    key_val.key_used = ik->ik_pad & 0x01;
#endif
    if(key_val.keytype == IEEE80211_CIPHER_WEP
            && vap->iv_wep_keycache) { 
        wlan_set_param(vap, IEEE80211_WEP_MBSSID, 0);
        /* only static wep keys will allocate index 0-3 in keycache 
         *if we are using 802.1x with WEP then it should go to else part 
         *to mandate this new iwpriv commnad wepkaycache is used 
         */
    }
    else {
        wlan_set_param(vap, IEEE80211_WEP_MBSSID, 1); 
        /* allow keys to allocate anywhere in key cache */
    }

    error = wlan_set_key(vap,kid,&key_val);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
            "\n\n%s: ******** SETKEY : error: %d , key_type=%d, macaddr=%s, key_len=%d\n\n",
            __func__, error, key_val.keytype, ether_sprintf(key_val.macaddr), key_val.keylen);

    wlan_set_param(vap, IEEE80211_WEP_MBSSID, 0) ;/* put it back to default */

    if ((ik->ik_flags & IEEE80211_KEY_DEFAULT) && (kid != IEEE80211_KEYIX_NONE)) {
        /* default xmit key */
        wlan_set_default_keyid(vap,kid);
    }
#ifdef ATH_SUPERG_XR
    /* set the same params on the xr vap device if exists */
    if(vap->iv_xrvap && !(vap->iv_flags & IEEE80211_F_XR) )
        ieee80211_ioctl_setkey(vap->iv_xrvap->iv_dev,info,w,extra);
#endif
    return error;
}

static int
ieee80211_ioctl_getkey(struct net_device *dev, struct iwreq *iwr)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct ieee80211req_key ik;
    ieee80211_keyval kval;
    u_int16_t kid;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_GETKEY, "getkey") ;

    if (iwr->u.data.length != sizeof(ik))
        return -EINVAL;
    if (_copy_from_user(&ik, iwr->u.data.pointer, sizeof(ik)))
        return -EFAULT;
    kid = ik.ik_keyix;
    kval.keydata = ik.ik_keydata;
    if (kid != IEEE80211_KEYIX_NONE)
    {
        if (kid >= IEEE80211_WEP_NKID)
            return -EINVAL;
    }
    if (wlan_get_key(vap,kid,ik.ik_macaddr, &kval, IEEE80211_KEYBUF_SIZE+IEEE80211_MICBUF_SIZE) != 0)
        return EINVAL;

    ik.ik_type = kval.keytype;
    ik.ik_keylen = kval.keylen;
    if (kval.keydir == IEEE80211_KEY_DIR_BOTH)
        ik.ik_flags =  (IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV);
    else if (kval.keydir == IEEE80211_KEY_DIR_TX)
        ik.ik_flags =  IEEE80211_KEY_XMIT ;
    else if (kval.keydir == IEEE80211_KEY_DIR_RX)
        ik.ik_flags =  IEEE80211_KEY_RECV;

    if (wlan_get_default_keyid(vap) == kid)
        ik.ik_flags |= IEEE80211_KEY_DEFAULT;
    if (capable(CAP_NET_ADMIN))
    {
        /* NB: only root can read key data */
        ik.ik_keyrsc = kval.keyrsc;
        ik.ik_keytsc = kval.keytsc;
        if (kval.keytype == IEEE80211_CIPHER_TKIP)
        {
            ik.ik_keylen += IEEE80211_MICBUF_SIZE;
        }
    }
    else
    {
        ik.ik_keyrsc = 0;
        ik.ik_keytsc = 0;
        memset(ik.ik_keydata, 0, sizeof(ik.ik_keydata));
    }
    return (_copy_to_user(iwr->u.data.pointer, &ik, sizeof(ik)) ?
        -EFAULT : 0);
}

static int
ieee80211_ioctl_delkey(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct ieee80211req_del_key *dk = (struct ieee80211req_del_key *)extra;
    debug_print_ioctl(dev->name, IEEE80211_IOCTL_DELKEY, "delkey") ;
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"DELETE CRYPTO KEY index %d, addr %s\n",
                    dk->idk_keyix, ether_sprintf(dk->idk_macaddr));
    if (dk->idk_keyix == 255) {
        wlan_del_key(vap,IEEE80211_KEYIX_NONE,dk->idk_macaddr);
    } else {
        wlan_del_key(vap,dk->idk_keyix,dk->idk_macaddr);
    }
    return 0;
}

static int
ieee80211_ioctl_getchanlist(struct net_device *dev,
    struct iw_request_info *info, void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    debug_print_ioctl(dev->name, IEEE80211_IOCTL_GETCHANLIST, "getchanlist") ;
    return wlan_get_chanlist(vap, extra);
}

static int
ieee80211_ioctl_getchaninfo(struct net_device *dev,
    struct iw_request_info *info, void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct ieee80211req_chaninfo *channel = (struct ieee80211req_chaninfo *)extra;
    debug_print_ioctl(dev->name, IEEE80211_IOCTL_GETCHANINFO, "getchaninfo") ;
    return wlan_get_chaninfo(vap, channel->ic_chans, &channel->ic_nchans);
}

static int
ieee80211_ioctl_giwtxpow(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_param *rrq, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    int txpow, fixed;
    debug_print_ioctl(dev->name, SIOCGIWTXPOW, "giwtxpow") ;
    wlan_get_txpow(vap, &txpow, &fixed);
    rrq->value = txpow;
    rrq->fixed = fixed;
    rrq->disabled = (rrq->fixed && rrq->value == 0);
    rrq->flags = IW_TXPOW_DBM;
    return 0;
}

static int
ieee80211_ioctl_siwtxpow(struct net_device *dev,
    struct iw_request_info *info,
    struct iw_param *rrq, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    int fixed, txpow, retv = EOK;
    debug_print_ioctl(dev->name, SIOCSIWTXPOW, "siwtxpow") ;

    wlan_get_txpow(vap, &txpow, &fixed);

    /*
    * for AP operation, we don't allow radio to be turned off.
    * for STA, /proc/sys/dev/wifiN/radio_on may be used.
    * see ath_hw_phystate_change(...)
    */
    if (rrq->disabled) {
        return -EOPNOTSUPP;
    }

    if (rrq->fixed) {
        if (rrq->flags != IW_TXPOW_DBM)
            return -EOPNOTSUPP;
        retv = wlan_set_txpow(vap, rrq->value);
    }
    else {
        retv = wlan_set_txpow(vap, 0);
    }
    return (retv != EOK) ? -EOPNOTSUPP : EOK;
}

static int
ieee80211_ioctl_addmac(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct sockaddr *sa = (struct sockaddr *)extra;
    int rc;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_ADDMAC, "addmac") ;

    rc = wlan_set_acl_add(vap, sa->sa_data);
    return rc;
}

static int
ieee80211_ioctl_delmac(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct sockaddr *sa = (struct sockaddr *)extra;
    int rc;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_DELMAC, "delmac") ;

    rc = wlan_set_acl_remove(vap, sa->sa_data);
    return rc;
}

#if UMAC_SUPPORT_WDS
/* 
 * for ipv6 ready logo test, EV#74677
 * need to parse the NS package, because LinklayerAddress 
 * may be NOT equal to Source MAC Address.
 */
extern struct ieee80211_node *
ieee80211_find_wds_node(struct ieee80211_node_table *nt, const u_int8_t *macaddr);
extern int 
ieee80211_add_wds_addr(struct ieee80211_node_table *nt,
                       struct ieee80211_node *ni, const u_int8_t *macaddr,
                                      u_int32_t flags);

int ieee80211_add_wdsaddr(wlan_if_t vap, union iwreq_data *u)
{
    struct ieee80211_node_table *nt;
    struct ieee80211com *ic;
    struct ieee80211_node *ni;
    struct ieee80211_node *ni_wds;
    struct sockaddr *sa = (struct sockaddr *)u->data.pointer;  
    int rc = 0; 

    ic = vap->iv_ic;
    nt = &ic->ic_sta;
    ni = NULL;
    ni_wds = NULL;

    if(vap->iv_opmode == IEEE80211_M_HOSTAP)
    {
        struct sockaddr *sa = ((struct sockaddr *)u->data.pointer)+1;   
        ni = ieee80211_find_wds_node(nt, sa->sa_data);
        if(ni) 
            ieee80211_free_node(ni);
        else 
            return rc = 1; 
    }
    else
    {
        ni = vap->iv_bss;
    }

    ni_wds = ieee80211_find_wds_node(nt, sa->sa_data);
    if(ni_wds)
    {
        ieee80211_free_node(ni_wds); /* Decr ref count */
    }
    else
    {
        rc = ieee80211_add_wds_addr(nt, ni, sa->sa_data, IEEE80211_NODE_F_WDS_BEHIND);
    }   

    return rc;
}
#endif


static int
ieee80211_ioctl_getaclmac(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    union iwreq_data *u = w;
    int rc=0;
	u_int8_t *macList; 
	struct sockaddr *sa = (struct sockaddr *)extra; 
	int	i, num_mac;

#if UMAC_SUPPORT_WDS        
    if(u->data.flags == IEEE80211_PARAM_ADD_WDS_ADDR)
    {
        rc = ieee80211_add_wdsaddr(vap, u);
        if(rc)
            rc = EFAULT;
    }else
#endif
    {
        debug_print_ioctl(dev->name, IEEE80211_IOCTL_GET_MACADDR, "getmac");
		macList = (u_int8_t *)OS_MALLOC(osifp->os_handle, (IEEE80211_ADDR_LEN * 256), GFP_KERNEL);
		if (!macList) { 
			return EFAULT; 
		} 

		rc = wlan_get_acl_list(vap, macList, (IEEE80211_ADDR_LEN * 256), &num_mac); 
		if(rc) { 
			if (macList) { 
				OS_FREE(macList); 
			} 
			return EFAULT; 
		} 

		for (i = 0; i < num_mac; i++) { 
			memcpy(&(sa[i]).sa_data, &macList[i * IEEE80211_ADDR_LEN], IEEE80211_ADDR_LEN); 
			sa[i].sa_family = ARPHRD_ETHER;    
		} 
		u->data.length = num_mac; 

		if (macList) { 
			OS_FREE(macList); 
		}
    }
    return rc;
}

static int
ieee80211_ioctl_kickmac(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    struct sockaddr *sa = (struct sockaddr *)extra;
    struct ieee80211req_mlme mlme;
    debug_print_ioctl(dev->name, IEEE80211_IOCTL_KICKMAC, "kickmac") ;

    if (sa->sa_family != ARPHRD_ETHER)
        return -EINVAL;

    /* Setup a MLME request for disassociation of the given MAC */
    mlme.im_op = IEEE80211_MLME_DISASSOC;
    mlme.im_reason = IEEE80211_REASON_UNSPECIFIED;
    IEEE80211_ADDR_COPY(&(mlme.im_macaddr), sa->sa_data);

    /* Send the MLME request and return the result. */
    return ieee80211_ioctl_setmlme(dev, info, w, (char *)&mlme);
}



static int
ieee80211_ioctl_setchanlist(struct net_device *dev,
    struct iw_request_info *info, void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap;
    
    vap = osifp->os_if;
    (void) extra;
    debug_print_ioctl(dev->name, IEEE80211_IOCTL_SETCHANLIST, "setchanlist") ;
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"%s: Not yet supported \n", __func__);
    return 0;
}


static int
ieee80211_ioctl_setwmmparams(struct net_device *dev,
    struct iw_request_info *info, void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    int *param = (int *) extra;
    int ac = (param[1] < WME_NUM_AC) ? param[1] : WME_AC_BE;
    int bss = param[2];
    int retv = 0;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_SETWMMPARAMS, "setwmmparams") ;
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                    "%s: param %d ac %d bss %d val %d\n",
                    __func__, param[0], ac, bss, param[3]);
    switch (param[0])
    {
    case IEEE80211_WMMPARAMS_CWMIN:
        if (param[3] < 0 ||  param[3] > 15) {
            retv = -EINVAL;
        }
        else {
            retv = wlan_set_wmm_param(vap, WLAN_WME_CWMIN,
                                        bss, ac, param[3]);
        }
        break;
    case IEEE80211_WMMPARAMS_CWMAX:
        if (param[3] < 0 ||  param[3] > 15) {
            retv = -EINVAL;
        }
        else {
            retv = wlan_set_wmm_param(vap, WLAN_WME_CWMAX,
                                        bss, ac, param[3]);
        }
        break;
    case IEEE80211_WMMPARAMS_AIFS:
        if (param[3] < 0 ||  param[3] > 15) {
            retv = -EINVAL;
        }
        else {
            retv = wlan_set_wmm_param(vap, WLAN_WME_AIFS,
                                        bss, ac, param[3]);
        }
        break;
    case IEEE80211_WMMPARAMS_TXOPLIMIT:
        if (param[3] < 0 ||  param[3] > 8192) {
            retv = -EINVAL;
        }
        else {
            retv = wlan_set_wmm_param(vap, WLAN_WME_TXOPLIMIT,
                                        bss, ac, param[3]);
        }
        break;
    case IEEE80211_WMMPARAMS_ACM:
        if (param[3] < 0 ||  param[3] > 1) {
            retv = -EINVAL;
        }
        else {
            retv = wlan_set_wmm_param(vap, WLAN_WME_ACM,
                                        bss, ac, param[3]);
        }
        break;
    case IEEE80211_WMMPARAMS_NOACKPOLICY:
        if (param[3] < 0 ||  param[3] > 1) {
            retv = -EINVAL;
        }
        else {
            retv = wlan_set_wmm_param(vap, WLAN_WME_ACKPOLICY,
                                        bss, ac, param[3]);
        }
        break;
    default:
        break;
    }
    return retv;
}

static int
ieee80211_ioctl_getwmmparams(struct net_device *dev,
    struct iw_request_info *info, void *w, char *extra)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    int *param = (int *) extra;
    int ac = (param[1] < WME_NUM_AC) ? param[1] : WME_AC_BE;
    int bss = param[2];

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_GETWMMPARAMS, "getwmmparams") ;
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                    "%s: param %d ac %d bss %d\n", __func__, param[0], ac, bss);
    switch (param[0])
    {
    case IEEE80211_WMMPARAMS_CWMIN:
        param[0] = wlan_get_wmm_param(vap, WLAN_WME_CWMIN,
                                            bss, ac);
        break;
    case IEEE80211_WMMPARAMS_CWMAX:
        param[0] = wlan_get_wmm_param(vap, WLAN_WME_CWMAX,
                                            bss, ac);
        break;
    case IEEE80211_WMMPARAMS_AIFS:
        param[0] = wlan_get_wmm_param(vap, WLAN_WME_AIFS,
                                            bss, ac);
        break;
    case IEEE80211_WMMPARAMS_TXOPLIMIT:
        param[0] = wlan_get_wmm_param(vap, WLAN_WME_TXOPLIMIT,
                                            bss, ac);
        break;
    case IEEE80211_WMMPARAMS_ACM:
        param[0] = wlan_get_wmm_param(vap, WLAN_WME_ACM,
                                            bss, ac);
        break;
    case IEEE80211_WMMPARAMS_NOACKPOLICY:
        param[0] = wlan_get_wmm_param(vap, WLAN_WME_ACKPOLICY,
                                            bss, ac);
        break;
    default:
        break;
    }
    return 0;
}

static int
ieee80211_ioctl_getwpaie(struct net_device *dev, struct iwreq *iwr)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct ieee80211req_wpaie *wpaie;
    u_int8_t    ni_ie[IEEE80211_MAX_OPT_IE];
    u_int16_t len = IEEE80211_MAX_OPT_IE;
    int ret;

    if (iwr->u.data.length != sizeof(*wpaie))
        return -EINVAL;
    wpaie = (struct ieee80211req_wpaie *)OS_MALLOC(osifp->os_handle, sizeof(*wpaie), GFP_KERNEL);
    if (wpaie == NULL)
    return -ENOMEM;
    if (_copy_from_user(wpaie, iwr->u.data.pointer, IEEE80211_ADDR_LEN)) {
    OS_FREE(wpaie);
        return -EFAULT;
    }
    memset(wpaie->wpa_ie, 0, sizeof(wpaie->wpa_ie));
    memset(wpaie->rsn_ie, 0, sizeof(wpaie->rsn_ie));
    wlan_node_getwpaie(vap,wpaie->wpa_macaddr,ni_ie,&len);
    if (len > 0) {
        if ((*ni_ie) == IEEE80211_ELEMID_RSN) {
            memcpy(wpaie->rsn_ie, ni_ie, len);
        }
        memcpy(wpaie->wpa_ie, ni_ie, len);
    }
#ifdef ATH_WPS_IE
    memset(wpaie->wps_ie, 0, sizeof(wpaie->wps_ie));
    len = IEEE80211_MAX_OPT_IE;
    wlan_node_getwpsie(vap,wpaie->wpa_macaddr,ni_ie,&len);
    if (len > 0) {
        memcpy(wpaie->wps_ie, ni_ie, len);
    }
#endif /* ATH_WPS_IE */
    ret = (_copy_to_user(iwr->u.data.pointer, wpaie, sizeof(*wpaie)) ?
        -EFAULT : 0);
    OS_FREE(wpaie);
    return ret;
}


#if UMAC_SUPPORT_TDLS
/* temporary to be removed later, until _getstastats() is available */
static int
ieee80211_ioctl_getstastats(struct net_device *dev, struct iwreq *iwr)
{
    struct ieee80211vap *vap = ath_netdev_priv(dev);
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node *ni;
    u_int8_t macaddr[IEEE80211_ADDR_LEN];
    const int off = __offsetof(struct ieee80211req_sta_stats, is_stats);
    int error;
    if (iwr->u.data.length < off)
        return -EINVAL;
    if (_copy_from_user(macaddr, iwr->u.data.pointer, IEEE80211_ADDR_LEN))
        return -EFAULT;
    ni = ieee80211_find_node(&ic->ic_sta, macaddr);
    if (ni == NULL)
        return -EINVAL;     /* XXX */
    if (iwr->u.data.length > sizeof(struct ieee80211req_sta_stats))
        iwr->u.data.length = sizeof(struct ieee80211req_sta_stats);
    /* NB: copy out only the statistics */
    error = _copy_to_user(iwr->u.data.pointer + off, &ni->ni_stats,
        iwr->u.data.length - off);
    ieee80211_free_node(ni);
    return (error ? -EFAULT : 0);
}
#endif /* UMAC_SUPPORT_TDLS */

#ifdef notyet

static int
ieee80211_ioctl_getwscie(struct net_device *dev, struct iwreq *iwr)
{
    struct ieee80211vap *vap = NETDEV_TO_VAP(dev);
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node *ni;
    struct ieee80211req_wscie wscie;

    if (iwr->u.data.length != sizeof(wscie))
        return -EINVAL;
    if (_copy_from_user(&wscie, iwr->u.data.pointer, IEEE80211_ADDR_LEN))
        return -EFAULT;
    ni = ieee80211_find_node(&ic->ic_sta, wscie.wsc_macaddr);
    if (ni == NULL)
        return -EINVAL;     /* XXX */
    memset(wscie.wsc_ie, 0, sizeof(wscie.wsc_ie));
    if (ni->ni_wps_ie != NULL)
    {
        int ielen = ni->ni_wps_ie[1] + 2;
        if (ielen > sizeof(wscie.wsc_ie))
            ielen = sizeof(wscie.wsc_ie);
        memcpy(wscie.wsc_ie, ni->ni_wps_ie, ielen);
    }
    ieee80211_free_node(ni);
    return (_copy_to_user(iwr->u.data.pointer, &wscie, sizeof(wscie)) ?
        -EFAULT : 0);
}


static int
ieee80211_ioctl_getstastats(struct net_device *dev, struct iwreq *iwr)
{
    struct ieee80211vap *vap = NETDEV_TO_VAP(dev);
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node *ni;
    u_int8_t macaddr[IEEE80211_ADDR_LEN];
    const int off = __offsetof(struct ieee80211req_sta_stats, is_stats);
    int error;

    if (iwr->u.data.length < off)
        return -EINVAL;
    if (_copy_from_user(macaddr, iwr->u.data.pointer, IEEE80211_ADDR_LEN))
        return -EFAULT;
    ni = ieee80211_find_node(&ic->ic_sta, macaddr);
    if (ni == NULL)
        return -EINVAL;     /* XXX */
    if (iwr->u.data.length > sizeof(struct ieee80211req_sta_stats))
        iwr->u.data.length = sizeof(struct ieee80211req_sta_stats);
    /* NB: copy out only the statistics */
    error = _copy_to_user(iwr->u.data.pointer + off, &ni->ni_stats,
        iwr->u.data.length - off);
    ieee80211_free_node(ni);
    return (error ? -EFAULT : 0);
}
#endif /* notyet */

struct scanreq
{
    struct ieee80211req_scan_result *sr;
    size_t space;
};

static size_t
scan_space(wlan_scan_entry_t se, u_int16_t *ielen)
{
    u_int8_t ssid_len;
    *ielen = 0;

    wlan_scan_entry_ssid(se,&ssid_len);
    *ielen =  wlan_scan_entry_ie_len(se);
    return roundup(sizeof(struct ieee80211req_scan_result) +
                        *ielen +  ssid_len, sizeof(u_int32_t));
}

static int
get_scan_space(void *arg, wlan_scan_entry_t se)
{
    struct scanreq *req = arg;
    u_int16_t ielen;

    /* Do not process Aged scan entries space */
#define AGING_TIMEOUT_SEC 60
    if ((long unsigned int)wlan_scan_entry_age(se)> 1000* AGING_TIMEOUT_SEC){ 
        return 0;
    }
    req->space += scan_space(se, &ielen);
    return 0;
}

static int
get_scan_result(void *arg, wlan_scan_entry_t se)
{
    struct scanreq *req = arg;
    struct ieee80211req_scan_result *sr;
    u_int16_t ielen, len, nr, nxr;
    u_int8_t *cp;
    u_int8_t ssid_len;
    u_int8_t *rates, *ssid;

    /* Do not process Aged scan entries */
#define AGING_TIMEOUT_SEC 60
    if ((long unsigned int)wlan_scan_entry_age(se)> 1000* AGING_TIMEOUT_SEC){ 
        return 0;
    }

    len = scan_space(se, &ielen);
    if (len > req->space)
        return 0;

    sr = req->sr;
    memset(sr, 0, sizeof(*sr));
    ssid = wlan_scan_entry_ssid(se,&ssid_len);
    sr->isr_ssid_len = ssid_len;

    if (ielen > 65534 ) {
        ielen = 0;
    }
    sr->isr_ie_len = ielen;
    sr->isr_len = len;
    sr->isr_freq = wlan_channel_ieee(wlan_scan_entry_channel(se));
    sr->isr_flags = 0;
    sr->isr_rssi = wlan_scan_entry_rssi(se);
    sr->isr_intval =  wlan_scan_entry_beacon_interval(se);
    sr->isr_capinfo = wlan_scan_entry_capinfo(se);
    sr->isr_erp =  wlan_scan_entry_erpinfo(se);
    IEEE80211_ADDR_COPY(sr->isr_bssid, wlan_scan_entry_bssid(se));
    rates = wlan_scan_entry_rates(se);
    nr = min((int)rates[1], IEEE80211_RATE_MAXSIZE);
    memcpy(sr->isr_rates, rates+2, nr);

    rates = wlan_scan_entry_xrates(se);
    nxr=0;
    if (rates) {
        nxr = min((int)rates[1], IEEE80211_RATE_MAXSIZE - nr);
        memcpy(sr->isr_rates+nr, rates+2, nxr);
    }
    sr->isr_nrates = nr + nxr;

    cp = (u_int8_t *)(sr+1);
    if (ssid) {
        memcpy(cp,ssid, sr->isr_ssid_len);
    }
    cp += sr->isr_ssid_len;

    if (ielen) {
        wlan_scan_entry_copy_ie_data(se,cp,&ielen);
        cp += ielen;
    }

    req->space -= len;
    req->sr = (struct ieee80211req_scan_result *)(((u_int8_t *)sr) + len);

    return 0;
}

static int
ieee80211_ioctl_getscanresults(struct net_device *dev, struct iwreq *iwr)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct scanreq req;
    int error = 0;

    debug_print_ioctl(dev->name, 0xffff, "getscanresults") ;
    req.space = 0;
    wlan_scan_table_iterate(vap, get_scan_space, (void *) &req);

    if (req.space > iwr->u.data.length)
        return -E2BIG;
    if (req.space > 0) {
        size_t space;
        void *p;

        space = req.space;
        p = (void *)OS_MALLOC(osifp->os_handle, space, GFP_KERNEL);
        if (p == NULL)
            return -ENOMEM;
        req.sr = p;
        wlan_scan_table_iterate(vap, get_scan_result,(void *) &req);
        iwr->u.data.length = space - req.space;

        error = copy_to_user(iwr->u.data.pointer, p, iwr->u.data.length);
        OS_FREE(p);
    } else
        iwr->u.data.length = 0;
    return error;
}

static int
ieee80211_ioctl_getmac(struct net_device *dev, struct iwreq *iwr)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    uint8_t *macaddr;

    if (iwr->u.data.length < sizeof(IEEE80211_ADDR_LEN))
    {
        return -EFAULT;
    }

    macaddr = wlan_vap_get_macaddr(vap);

    return (_copy_to_user(iwr->u.data.pointer, macaddr, IEEE80211_ADDR_LEN) ? -EFAULT : 0);
}

struct stainforeq
{
    wlan_if_t vap;
    struct ieee80211req_sta_info *si;
    size_t  space;
};

static size_t
sta_space(const wlan_node_t node, size_t *ielen, wlan_if_t vap)
{
    u_int8_t    ni_ie[IEEE80211_MAX_OPT_IE];
    u_int16_t ni_ie_len = IEEE80211_MAX_OPT_IE;
    u_int8_t *macaddr = wlan_node_getmacaddr(node);
    *ielen = 0;

#ifdef notyet
    /* Currently RSN/WPA IE store in the same place */
    if (ni->ni_rsn_ie != NULL)
        *ielen += 2+ni->ni_rsn_ie[1];
#endif /* notyet */
    if(!wlan_node_getwpaie(vap, macaddr, ni_ie, &ni_ie_len)) {
        *ielen += ni_ie_len;
        ni_ie_len = IEEE80211_MAX_OPT_IE;
    }
    if(!wlan_node_getwmeie(vap, macaddr, ni_ie, &ni_ie_len)) {
        *ielen += ni_ie_len;
        ni_ie_len = IEEE80211_MAX_OPT_IE;
    }
    if(!wlan_node_getathie(vap, macaddr, ni_ie, &ni_ie_len)) {
        *ielen += ni_ie_len;
        ni_ie_len = IEEE80211_MAX_OPT_IE;
    }
#ifdef ATH_WPS_IE
    if(!wlan_node_getwpsie(vap, macaddr, ni_ie, &ni_ie_len)) {
        *ielen += ni_ie_len;
        ni_ie_len = IEEE80211_MAX_OPT_IE;
    }
#endif /* ATH_WPS_IE */

    return roundup(sizeof(struct ieee80211req_sta_info) + *ielen,
        sizeof(u_int32_t));
}

static void
get_sta_space(void *arg, wlan_node_t node)
{
    struct stainforeq *req = arg;
    size_t ielen;

    /* already ignore invalid nodes in UMAC */
    req->space += sta_space(node, &ielen, req->vap);
}

static void
get_sta_info(void *arg, wlan_node_t node)
{
    struct stainforeq *req = arg;
    wlan_if_t vap = req->vap;
    struct ieee80211req_sta_info *si;
    size_t ielen, len;
    u_int8_t *cp;
    u_int8_t    ni_ie[IEEE80211_MAX_OPT_IE];
    u_int16_t ni_ie_len = IEEE80211_MAX_OPT_IE;
    u_int8_t *macaddr = wlan_node_getmacaddr(node);
    wlan_rssi_info rssi_info;
    wlan_chan_t chan = wlan_node_get_chan(node);
    ieee80211_rate_info rinfo;

    /* already ignore invalid nodes in UMAC */

    if (chan == IEEE80211_CHAN_ANYC) { /* XXX bogus entry */
        return;
    }

    len = sta_space(node, &ielen, vap);
    if (len > req->space) {
        return;
    }
    si = req->si;
    si->isi_assoc_time = wlan_node_get_assocuptime(node);
    si->isi_len = len;
    si->isi_ie_len = ielen;
    si->isi_freq = wlan_channel_frequency(chan);
    si->isi_flags = wlan_channel_flags(chan);
    si->isi_state = wlan_node_get_state_flag(node);
    si->isi_authmode =  wlan_node_get_authmode(node);
    if (wlan_node_getrssi(node, &rssi_info, WLAN_RSSI_RX) == 0) {
        si->isi_rssi = rssi_info.avg_rssi;
    }
    si->isi_capinfo = wlan_node_getcapinfo(node);
    si->isi_athflags = wlan_node_get_ath_flags(node);
    si->isi_erp = wlan_node_get_erp(node);
    IEEE80211_ADDR_COPY(si->isi_macaddr, macaddr);
    if (wlan_node_txrate_info(node, &rinfo) == 0) {
        si->isi_txratekbps = rinfo.rate;
    }
    si->isi_associd = wlan_node_get_associd(node);
    si->isi_txpower = wlan_node_get_txpower(node);
    si->isi_vlan = wlan_node_get_vlan(node);
    si->isi_cipher = IEEE80211_CIPHER_NONE;
    if (wlan_get_param(vap, IEEE80211_FEATURE_PRIVACY)) {
        do {
            ieee80211_cipher_type uciphers[1];
            int count = 0;
            count = wlan_node_get_ucast_ciphers(node, uciphers, 1);
            if (count == 1) {
                si->isi_cipher |= 1<<uciphers[0];
            }
        } while (0);
    }
    wlan_node_get_txseqs(node, si->isi_txseqs, sizeof(si->isi_txseqs));
    wlan_node_get_rxseqs(node, si->isi_rxseqs, sizeof(si->isi_rxseqs));
    si->isi_uapsd = wlan_node_get_uapsd(node);
    si->isi_opmode = IEEE80211_STA_OPMODE_NORMAL;
    si->isi_inact = wlan_node_get_inact(node);
    /* 11n */
    si->isi_htcap = wlan_node_get_htcap(node);

    cp = (u_int8_t *)(si+1);
#ifdef notyet
    /* Currently RSN/WPA IE store in the same place */
    if (ni->ni_rsn_ie != NULL) {
        memcpy(cp, ni->ni_rsn_ie, 2 + ni->ni_rsn_ie[1]);
        cp += 2 + ni->ni_rsn_ie[1];
    }
#endif /* notyet */

    if(!wlan_node_getwpaie(vap, macaddr, ni_ie, &ni_ie_len)) {
        OS_MEMCPY(cp, ni_ie, ni_ie_len);
        cp += ni_ie_len;
        ni_ie_len = IEEE80211_MAX_OPT_IE;
    }
    if(!wlan_node_getwmeie(vap, macaddr, ni_ie, &ni_ie_len)) {
        OS_MEMCPY(cp, ni_ie, ni_ie_len);
        cp += ni_ie_len;
        ni_ie_len = IEEE80211_MAX_OPT_IE;
    }
    if(!wlan_node_getathie(vap, macaddr, ni_ie, &ni_ie_len)) {
        OS_MEMCPY(cp, ni_ie, ni_ie_len);
        cp += ni_ie_len;
        ni_ie_len = IEEE80211_MAX_OPT_IE;
    }
#ifdef ATH_WPS_IE
    if(!wlan_node_getwpsie(vap, macaddr, ni_ie, &ni_ie_len)) {
        OS_MEMCPY(cp, ni_ie, ni_ie_len);
        cp += ni_ie_len;
        ni_ie_len = IEEE80211_MAX_OPT_IE;
    }
#endif /* ATH_WPS_IE */

    req->si = (
    struct ieee80211req_sta_info *)(((u_int8_t *)si) + len);
    req->space -= len;
}

static int
ieee80211_ioctl_getstainfo(struct net_device *dev, struct iwreq *iwr)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct stainforeq req;
    int error;

    if(osifp->os_opmode == IEEE80211_M_STA ) {
#if UMAC_SUPPORT_TDLS
/* temporary to be removed later, until _getstastats() is available */
	    ;
#else
        return -EPERM;
#endif /* UMAC_SUPPORT_TDLS */
	}
    if (iwr->u.data.length < sizeof(struct stainforeq))
        return -EFAULT;

    /* estimate space required for station info */
    error = 0;
    req.space = sizeof(struct stainforeq);
    req.vap = vap;
    wlan_iterate_station_list(vap, get_sta_space, &req);
    if (req.space > iwr->u.data.length)
        req.space = iwr->u.data.length;
    if (req.space > 0)
    {
        size_t space;
        void *p;

        space = req.space;
        p = (void *)OS_MALLOC(osifp->os_handle, space, GFP_KERNEL);
        if (p == NULL)
            return ENOMEM;
        req.si = (struct ieee80211req_sta_info *)p;
        wlan_iterate_station_list(vap, get_sta_info, &req);
        iwr->u.data.length = space - req.space;
        error = _copy_to_user(iwr->u.data.pointer, p, iwr->u.data.length);
        OS_FREE(p);
    }
    else
        iwr->u.data.length = 0;

    return (error ? -EFAULT : 0);
}

int
ieee80211_ioctl_chanswitch(struct net_device *dev, struct iw_request_info *info,
    void *w, char *extra)
{
    wlan_if_t vap = NETDEV_TO_VAP(dev);

    printk("ieee80211_ioctl_chanswitch()\n");
    wlan_set_chanswitch(vap, extra);
    return 0;
}

static int
ieee80211_ioctl_get_scan_space(struct net_device *dev, struct iwreq *iw)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct scanreq req;

    debug_print_ioctl(dev->name, 0xffff, "getscanspace") ;
    req.space = 0;
    wlan_scan_table_iterate(vap, get_scan_space, (void *) &req);

    iw->u.data.length = req.space;
    return 0;
}

#ifdef notyet

#if WIRELESS_EXT >= 18
static int
ieee80211_ioctl_siwmlme(struct net_device *dev,
    struct iw_request_info *info, struct iw_point *erq, char *data)
{
    struct ieee80211req_mlme mlme;
    struct iw_mlme *wextmlme = (struct iw_mlme *)data;

    debug_print_ioctl(dev->name, SIOCSIWMLME, "siwmlme") ;
    memset(&mlme, 0, sizeof(mlme));

    switch(wextmlme->cmd) {
    case IW_MLME_DEAUTH:
        mlme.im_op = IEEE80211_MLME_DEAUTH;
        break;
    case IW_MLME_DISASSOC:
        mlme.im_op = IEEE80211_MLME_DISASSOC;
        break;
    default:
        return -EINVAL;
    }

    mlme.im_reason = wextmlme->reason_code;

    memcpy(mlme.im_macaddr, wextmlme->addr.sa_data, IEEE80211_ADDR_LEN);

    return ieee80211_ioctl_setmlme(dev, NULL, NULL, (char*)&mlme);
}

static int
ieee80211_ioctl_giwgenie(struct net_device *dev,
    struct iw_request_info *info, struct iw_point *out, char *buf)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t nvap = osifp->os_if;
//fixme this is a layering violation, directly accessing data
    struct ieee80211vap *vap = (void *) nvap;

    if (out->length < vap->iv_opt_ie_len)
        return -E2BIG;

    return ieee80211_ioctl_getoptie(dev, info, out, buf);
}

static int
ieee80211_ioctl_siwgenie(struct net_device *dev,
    struct iw_request_info *info, struct iw_point *erq, char *data)
{
    return ieee80211_ioctl_setoptie(dev, info, erq, data);
}

static int
siwauth_wpa_version(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int ver = erq->value;
    int args[2];

    args[0] = IEEE80211_PARAM_WPA;

    if ((ver & IW_AUTH_WPA_VERSION_WPA) && (ver & IW_AUTH_WPA_VERSION_WPA2))
        args[1] = 3;
    else if (ver & IW_AUTH_WPA_VERSION_WPA2)
        args[1] = 2;
    else if (ver & IW_AUTH_WPA_VERSION_WPA)
        args[1] = 1;
    else
        args[1] = 0;

    return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}

static int
iwcipher2ieee80211cipher(int iwciph)
{
    switch(iwciph) {
    case IW_AUTH_CIPHER_NONE:
        return IEEE80211_CIPHER_NONE;
    case IW_AUTH_CIPHER_WEP40:
    case IW_AUTH_CIPHER_WEP104:
        return IEEE80211_CIPHER_WEP;
    case IW_AUTH_CIPHER_TKIP:
        return IEEE80211_CIPHER_TKIP;
    case IW_AUTH_CIPHER_CCMP:
        return IEEE80211_CIPHER_AES_CCM;
    }
    return -1;
}

static int
ieee80211cipher2iwcipher(int ieee80211ciph)
{
    switch(ieee80211ciph) {
    case IEEE80211_CIPHER_NONE:
        return IW_AUTH_CIPHER_NONE;
    case IEEE80211_CIPHER_WEP:
        return IW_AUTH_CIPHER_WEP104;
    case IEEE80211_CIPHER_TKIP:
        return IW_AUTH_CIPHER_TKIP;
    case IEEE80211_CIPHER_AES_CCM:
        return IW_AUTH_CIPHER_CCMP;
    }
    return -1;
}

/* TODO We don't enforce wep key lengths. */
static int
siwauth_cipher_pairwise(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int iwciph = erq->value;
    int args[2];

    args[0] = IEEE80211_PARAM_UCASTCIPHER;
    args[1] = iwcipher2ieee80211cipher(iwciph);
    if (args[1] < 0) {
        printk(KERN_WARNING "%s: unknown pairwise cipher %d\n",
                dev->name, iwciph);
        return -EINVAL;
    }
    return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}

/* TODO We don't enforce wep key lengths. */
static int
siwauth_cipher_group(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int iwciph = erq->value;
    int args[2];

    args[0] = IEEE80211_PARAM_MCASTCIPHER;
    args[1] = iwcipher2ieee80211cipher(iwciph);
    if (args[1] < 0) {
        printk(KERN_WARNING "%s: unknown group cipher %d\n",
                dev->name, iwciph);
        return -EINVAL;
    }
    return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}

static int
siwauth_key_mgmt(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int iwkm = erq->value;
    int args[2];

    args[0] = IEEE80211_PARAM_KEYMGTALGS;
    args[1] = WPA_ASE_NONE;
    if (iwkm & IW_AUTH_KEY_MGMT_802_1X)
        args[1] |= WPA_ASE_8021X_UNSPEC;
    if (iwkm & IW_AUTH_KEY_MGMT_PSK)
        args[1] |= WPA_ASE_8021X_PSK;

    return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}

static int
siwauth_tkip_countermeasures(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int args[2];
    args[0] = IEEE80211_PARAM_COUNTERMEASURES;
    args[1] = erq->value;
    return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}

static int
siwauth_drop_unencrypted(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int args[2];
    args[0] = IEEE80211_PARAM_DROPUNENCRYPTED;
    args[1] = erq->value;
    return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}


static int
siwauth_80211_auth_alg(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
#define VALID_ALGS_MASK (IW_AUTH_ALG_OPEN_SYSTEM|IW_AUTH_ALG_SHARED_KEY|IW_AUTH_ALG_LEAP)
    int mode = erq->value;
    int args[2];

    args[0] = IEEE80211_PARAM_AUTHMODE;

    if (mode & ~VALID_ALGS_MASK) {
        return -EINVAL;
    }
    if (mode & IW_AUTH_ALG_LEAP) {
        args[1] = IEEE80211_AUTH_8021X;
    } else if ((mode & IW_AUTH_ALG_SHARED_KEY) &&
        (mode & IW_AUTH_ALG_OPEN_SYSTEM)) {
        args[1] = IEEE80211_AUTH_AUTO;
    } else if (mode & IW_AUTH_ALG_SHARED_KEY) {
        args[1] = IEEE80211_AUTH_SHARED;
    } else {
        args[1] = IEEE80211_AUTH_OPEN;
    }
    return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}

static int
siwauth_wpa_enabled(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int enabled = erq->value;
    int args[2];

    args[0] = IEEE80211_PARAM_WPA;
    if (enabled)
        args[1] = 3; /* enable WPA1 and WPA2 */
    else
        args[1] = 0; /* disable WPA1 and WPA2 */

    return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}

static int
siwauth_rx_unencrypted_eapol(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int rxunenc = erq->value;
    int args[2];

    args[0] = IEEE80211_PARAM_DROPUNENC_EAPOL;
    if (rxunenc)
        args[1] = 1;
    else
        args[1] = 0;

    return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}

static int
siwauth_roaming_control(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int roam = erq->value;
    int args[2];

    args[0] = IEEE80211_PARAM_ROAMING;
    switch(roam) {
    case IW_AUTH_ROAMING_ENABLE:
        args[1] = IEEE80211_ROAMING_AUTO;
        break;
    case IW_AUTH_ROAMING_DISABLE:
        args[1] = IEEE80211_ROAMING_MANUAL;
        break;
    default:
        return -EINVAL;
    }
    return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}

static int
siwauth_privacy_invoked(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int args[2];
    args[0] = IEEE80211_PARAM_PRIVACY;
    args[1] = erq->value;
    return ieee80211_ioctl_setparam(dev, NULL, NULL, (char*)args);
}

/*
* If this function is invoked it means someone is using the wireless extensions
* API instead of the private madwifi ioctls.  That's fine.  We translate their
* request into the format used by the private ioctls.  Note that the
* iw_request_info and iw_param structures are not the same ones as the
* private ioctl handler expects.  Luckily, the private ioctl handler doesn't
* do anything with those at the moment.  We pass NULL for those, because in
* case someone does modify the ioctl handler to use those values, a null
* pointer will be easier to debug than other bad behavior.
*/
static int
ieee80211_ioctl_siwauth(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int rc = -EINVAL;

    switch(erq->flags & IW_AUTH_INDEX) {
    case IW_AUTH_WPA_VERSION:
        rc = siwauth_wpa_version(dev, info, erq, buf);
        break;
    case IW_AUTH_CIPHER_PAIRWISE:
        rc = siwauth_cipher_pairwise(dev, info, erq, buf);
        break;
    case IW_AUTH_CIPHER_GROUP:
        rc = siwauth_cipher_group(dev, info, erq, buf);
        break;
    case IW_AUTH_KEY_MGMT:
        rc = siwauth_key_mgmt(dev, info, erq, buf);
        break;
    case IW_AUTH_TKIP_COUNTERMEASURES:
        rc = siwauth_tkip_countermeasures(dev, info, erq, buf);
        break;
    case IW_AUTH_DROP_UNENCRYPTED:
        rc = siwauth_drop_unencrypted(dev, info, erq, buf);
        break;
    case IW_AUTH_80211_AUTH_ALG:
        rc = siwauth_80211_auth_alg(dev, info, erq, buf);
        break;
    case IW_AUTH_WPA_ENABLED:
        rc = siwauth_wpa_enabled(dev, info, erq, buf);
        break;
    case IW_AUTH_RX_UNENCRYPTED_EAPOL:
        rc = siwauth_rx_unencrypted_eapol(dev, info, erq, buf);
        break;
    case IW_AUTH_ROAMING_CONTROL:
        rc = siwauth_roaming_control(dev, info, erq, buf);
        break;
    case IW_AUTH_PRIVACY_INVOKED:
        rc = siwauth_privacy_invoked(dev, info, erq, buf);
        break;
    default:
        printk(KERN_WARNING "%s: unknown SIOCSIWAUTH flag %d\n",
            dev->name, erq->flags);
        break;
    }

    return rc;
}

static int
giwauth_wpa_version(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int ver;
    int rc;
    int arg = IEEE80211_PARAM_WPA;

    rc = ieee80211_ioctl_getparam(dev, NULL, NULL, (char*)&arg);
    if (rc)
        return rc;

    switch(arg) {
    case 1:
            ver = IW_AUTH_WPA_VERSION_WPA;
        break;
    case 2:
            ver = IW_AUTH_WPA_VERSION_WPA2;
        break;
    case 3:
            ver = IW_AUTH_WPA_VERSION|IW_AUTH_WPA_VERSION_WPA2;
        break;
    default:
        ver = IW_AUTH_WPA_VERSION_DISABLED;
        break;
    }

    erq->value = ver;
    return rc;
}

static int
giwauth_cipher_pairwise(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int rc;
    int arg = IEEE80211_PARAM_UCASTCIPHER;

    rc = ieee80211_ioctl_getparam(dev, NULL, NULL, (char*)&arg);
    if (rc)
        return rc;

    erq->value = ieee80211cipher2iwcipher(arg);
    if (erq->value < 0)
        return -EINVAL;
    return 0;
}


static int
giwauth_cipher_group(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int rc;
    int arg = IEEE80211_PARAM_MCASTCIPHER;

    rc = ieee80211_ioctl_getparam(dev, NULL, NULL, (char*)&arg);
    if (rc)
        return rc;

    erq->value = ieee80211cipher2iwcipher(arg);
    if (erq->value < 0)
        return -EINVAL;
    return 0;
}

static int
giwauth_key_mgmt(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int arg;
    int rc;

    arg = IEEE80211_PARAM_KEYMGTALGS;
    rc = ieee80211_ioctl_getparam(dev, NULL, NULL, (char*)&arg);
    if (rc)
        return rc;
    erq->value = 0;
    if (arg & WPA_ASE_8021X_UNSPEC)
        erq->value |= IW_AUTH_KEY_MGMT_802_1X;
    if (arg & WPA_ASE_8021X_PSK)
        erq->value |= IW_AUTH_KEY_MGMT_PSK;
    return 0;
}

static int
giwauth_tkip_countermeasures(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int arg;
    int rc;

    arg = IEEE80211_PARAM_COUNTERMEASURES;
    rc = ieee80211_ioctl_getparam(dev, NULL, NULL, (char*)&arg);
    if (rc)
        return rc;
    erq->value = arg;
    return 0;
}

static int
giwauth_drop_unencrypted(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int arg;
    int rc;
    arg = IEEE80211_PARAM_DROPUNENCRYPTED;
    rc = ieee80211_ioctl_getparam(dev, NULL, NULL, (char*)&arg);
    if (rc)
        return rc;
    erq->value = arg;
    return 0;
}

static int
giwauth_80211_auth_alg(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    return -EOPNOTSUPP;
}

static int
giwauth_wpa_enabled(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int rc;
    int arg = IEEE80211_PARAM_WPA;

    rc = ieee80211_ioctl_getparam(dev, NULL, NULL, (char*)&arg);
    if (rc)
        return rc;

    erq->value = arg;
    return 0;

}

static int
giwauth_rx_unencrypted_eapol(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    return -EOPNOTSUPP;
}

static int
giwauth_roaming_control(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int rc;
    int arg;

    arg = IEEE80211_PARAM_ROAMING;
    rc = ieee80211_ioctl_getparam(dev, NULL, NULL, (char*)&arg);
    if (rc)
        return rc;

    switch(arg) {
    case IEEE80211_ROAMING_DEVICE:
    case IEEE80211_ROAMING_AUTO:
        erq->value = IW_AUTH_ROAMING_ENABLE;
        break;
    default:
        erq->value = IW_AUTH_ROAMING_DISABLE;
        break;
    }

    return 0;
}

static int
giwauth_privacy_invoked(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int rc;
    int arg;
    arg = IEEE80211_PARAM_PRIVACY;
    rc = ieee80211_ioctl_getparam(dev, NULL, NULL, (char*)&arg);
    if (rc)
        return rc;
    erq->value = arg;
    return 0;
}

static int
ieee80211_ioctl_giwauth(struct net_device *dev,
    struct iw_request_info *info, struct iw_param *erq, char *buf)
{
    int rc = -EOPNOTSUPP;

    switch(erq->flags & IW_AUTH_INDEX) {
    case IW_AUTH_WPA_VERSION:
        rc = giwauth_wpa_version(dev, info, erq, buf);
        break;
    case IW_AUTH_CIPHER_PAIRWISE:
        rc = giwauth_cipher_pairwise(dev, info, erq, buf);
        break;
    case IW_AUTH_CIPHER_GROUP:
        rc = giwauth_cipher_group(dev, info, erq, buf);
        break;
    case IW_AUTH_KEY_MGMT:
        rc = giwauth_key_mgmt(dev, info, erq, buf);
        break;
    case IW_AUTH_TKIP_COUNTERMEASURES:
        rc = giwauth_tkip_countermeasures(dev, info, erq, buf);
        break;
    case IW_AUTH_DROP_UNENCRYPTED:
        rc = giwauth_drop_unencrypted(dev, info, erq, buf);
        break;
    case IW_AUTH_80211_AUTH_ALG:
        rc = giwauth_80211_auth_alg(dev, info, erq, buf);
        break;
    case IW_AUTH_WPA_ENABLED:
        rc = giwauth_wpa_enabled(dev, info, erq, buf);
        break;
    case IW_AUTH_RX_UNENCRYPTED_EAPOL:
        rc = giwauth_rx_unencrypted_eapol(dev, info, erq, buf);
        break;
    case IW_AUTH_ROAMING_CONTROL:
        rc = giwauth_roaming_control(dev, info, erq, buf);
        break;
    case IW_AUTH_PRIVACY_INVOKED:
        rc = giwauth_privacy_invoked(dev, info, erq, buf);
        break;
    default:
        printk(KERN_WARNING "%s: unknown SIOCGIWAUTH flag %d\n",
            dev->name, erq->flags);
        break;
    }

    return rc;
}

/*
* Retrieve information about a key.  Open question: should we allow
* callers to retrieve unicast keys based on a supplied MAC address?
* The ipw2200 reference implementation doesn't, so we don't either.
*/
static int
ieee80211_ioctl_giwencodeext(struct net_device *dev,
    struct iw_request_info *info, struct iw_point *erq, char *extra)
{
    struct ieee80211vap *vap = NETDEV_TO_VAP(dev);
    struct iw_encode_ext *ext;
    struct ieee80211_key *wk;
    int error;
    int kid=0;
    int max_key_len;

    if (!capable(CAP_NET_ADMIN))
        return -EPERM;

    max_key_len = erq->length - sizeof(*ext);
    if (max_key_len < 0)
        return -EINVAL;
    ext = (struct iw_encode_ext *)extra;

    error = getiwkeyix(vap, erq, &kid);
    if (error < 0)
        return error;

    wk = &vap->iv_nw_keys[kid];
    if (wk->wk_keylen > max_key_len)
        return -E2BIG;

    erq->flags = kid+1;
    memset(ext, 0, sizeof(*ext));

    ext->key_len = wk->wk_keylen;
    memcpy(ext->key, wk->wk_key, wk->wk_keylen);

    /* flags */
    if (wk->wk_flags & IEEE80211_KEY_GROUP)
        ext->ext_flags |= IW_ENCODE_EXT_GROUP_KEY;

    /* algorithm */
    switch(wk->wk_cipher->ic_cipher) {
    case IEEE80211_CIPHER_NONE:
        ext->alg = IW_ENCODE_ALG_NONE;
        erq->flags |= IW_ENCODE_DISABLED;
        break;
    case IEEE80211_CIPHER_WEP:
        ext->alg = IW_ENCODE_ALG_WEP;
        break;
    case IEEE80211_CIPHER_TKIP:
        ext->alg = IW_ENCODE_ALG_TKIP;
        break;
    case IEEE80211_CIPHER_AES_OCB:
    case IEEE80211_CIPHER_AES_CCM:
    case IEEE80211_CIPHER_CKIP:
        ext->alg = IW_ENCODE_ALG_CCMP;
        break;
    default:
        return -EINVAL;
    }
    return 0;
}

static int
ieee80211_ioctl_siwencodeext(struct net_device *dev,
    struct iw_request_info *info, struct iw_point *erq, char *extra)
{
    struct ieee80211vap *vap = NETDEV_TO_VAP(dev);
    struct iw_encode_ext *ext = (struct iw_encode_ext *)extra;
    struct ieee80211req_key kr;
    int error;
    int kid=0;

    debug_print_ioctl(dev->name, IEEE80211_IOCTL_SIWENCODEEXT, "siwencodeext") ;
    error = getiwkeyix(vap, erq, &kid);
    if (error < 0)
        return error;

    if (ext->key_len > (erq->length - sizeof(struct iw_encode_ext)))
        return -EINVAL;

    if (ext->alg == IW_ENCODE_ALG_NONE) {
        /* convert to the format used by IEEE_80211_IOCTL_DELKEY */
        struct ieee80211req_del_key dk;

        memset(&dk, 0, sizeof(dk));
        dk.idk_keyix = kid;
        memcpy(&dk.idk_macaddr, ext->addr.sa_data, IEEE80211_ADDR_LEN);

        return ieee80211_ioctl_delkey(dev, NULL, NULL, (char*)&dk);
    }

    /* TODO This memcmp for the broadcast address seems hackish, but
    * mimics what wpa supplicant was doing.  The wpa supplicant comments
    * make it sound like they were having trouble with
    * IEEE80211_IOCTL_SETKEY and static WEP keys.  It might be worth
    * figuring out what their trouble was so the rest of this function
    * can be implemented in terms of ieee80211_ioctl_setkey */
    if (ext->alg == IW_ENCODE_ALG_WEP &&
        memcmp(ext->addr.sa_data, "\xff\xff\xff\xff\xff\xff",
            IEEE80211_ADDR_LEN) == 0) {
        /* convert to the format used by SIOCSIWENCODE.  The old
        * format just had the key in the extra buf, whereas the
        * new format has the key tacked on to the end of the
        * iw_encode_ext structure */
        struct iw_request_info oldinfo;
        struct iw_point olderq;
        char *key;

        memset(&oldinfo, 0, sizeof(oldinfo));
        oldinfo.cmd = SIOCSIWENCODE;
        oldinfo.flags = info->flags;

        memset(&olderq, 0, sizeof(olderq));
        olderq.flags = erq->flags;
        olderq.pointer = erq->pointer;
        olderq.length = ext->key_len;

        key = ext->key;

        return ieee80211_ioctl_siwencode(dev, &oldinfo, &olderq, key);
    }

    /* convert to the format used by IEEE_80211_IOCTL_SETKEY */
    memset(&kr, 0, sizeof(kr));

    switch(ext->alg) {
    case IW_ENCODE_ALG_WEP:
        kr.ik_type = IEEE80211_CIPHER_WEP;
        break;
    case IW_ENCODE_ALG_TKIP:
        kr.ik_type = IEEE80211_CIPHER_TKIP;
        break;
    case IW_ENCODE_ALG_CCMP:
        kr.ik_type = IEEE80211_CIPHER_AES_CCM;
        break;
    default:
        printk(KERN_WARNING "%s: unknown algorithm %d\n",
                dev->name, ext->alg);
        return -EINVAL;
    }

    kr.ik_keyix = kid;

    if (ext->key_len > sizeof(kr.ik_keydata)) {
        printk(KERN_WARNING "%s: key size %d is too large\n",
                dev->name, ext->key_len);
        return -E2BIG;
    }
    memcpy(kr.ik_keydata, ext->key, ext->key_len);
    kr.ik_keylen = ext->key_len;

    kr.ik_flags = IEEE80211_KEY_RECV;

    if (ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY)
        kr.ik_flags |= IEEE80211_KEY_GROUP;

    if (ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY) {
        kr.ik_flags |= IEEE80211_KEY_XMIT | IEEE80211_KEY_DEFAULT;
        memcpy(kr.ik_macaddr, ext->addr.sa_data, IEEE80211_ADDR_LEN);
    }

    if (ext->ext_flags & IW_ENCODE_EXT_RX_SEQ_VALID) {
        memcpy(&kr.ik_keyrsc, ext->rx_seq, sizeof(kr.ik_keyrsc));
    }

    return ieee80211_ioctl_setkey(dev, NULL, NULL, (char*)&kr);
}
#endif /* WIRELESS_EXT >= 18 */


static int
ieee80211_ioctl_getmacaddr(struct net_device *dev,
    struct iw_request_info *info,
    void *w, char *extra)
{
    struct ieee80211vap *vap = NETDEV_TO_VAP(dev);
    union iwreq_data *u = w;
    debug_print_ioctl(dev->name, IEEE80211_IOCTL_GET_MACADDR, "getmacaddr") ;
    IEEE80211_ADDR_COPY(&(u->addr.sa_data), vap->iv_myaddr);
    return 0;
}
#endif /* notyet */


#define IW_PRIV_TYPE_OPTIE  IW_PRIV_TYPE_BYTE | IEEE80211_MAX_OPT_IE
#define IW_PRIV_TYPE_KEY \
    IW_PRIV_TYPE_BYTE | sizeof(struct ieee80211req_key)
#define IW_PRIV_TYPE_DELKEY \
    IW_PRIV_TYPE_BYTE | sizeof(struct ieee80211req_del_key)
#define IW_PRIV_TYPE_MLME \
    IW_PRIV_TYPE_BYTE | sizeof(struct ieee80211req_mlme)
#define IW_PRIV_TYPE_CHANLIST \
    IW_PRIV_TYPE_BYTE | sizeof(struct ieee80211req_chanlist)
/*
* There are 11 bits for size.  However, the
* size reqd is 4084 (>er than 11 bits).  Hence...
*/
#define IW_PRIV_TYPE_CHANINFO \
    (IW_PRIV_TYPE_INT | \
    ((sizeof(struct ieee80211req_chaninfo)/sizeof(__u32)) + 1))
#define IW_PRIV_TYPE_APPIEBUF  (IW_PRIV_TYPE_BYTE | IEEE80211_APPIE_MAX)
#define IW_PRIV_TYPE_FILTER \
        IW_PRIV_TYPE_BYTE | sizeof(struct ieee80211req_set_filter)
#define IW_PRIV_TYPE_ACLMACLIST  (IW_PRIV_TYPE_ADDR | 256)

static const struct iw_priv_args ieee80211_priv_args[] = {
    /* NB: setoptie & getoptie are !IW_PRIV_SIZE_FIXED */
    { IEEE80211_IOCTL_SETOPTIE,
    IW_PRIV_TYPE_OPTIE, 0,            "setoptie" },
    { IEEE80211_IOCTL_GETOPTIE,
    0, IW_PRIV_TYPE_OPTIE,            "getoptie" },
    { IEEE80211_IOCTL_SETKEY,
    IW_PRIV_TYPE_KEY | IW_PRIV_SIZE_FIXED, 0, "setkey" },
    { IEEE80211_IOCTL_DELKEY,
    IW_PRIV_TYPE_DELKEY | IW_PRIV_SIZE_FIXED, 0,  "delkey" },
    { IEEE80211_IOCTL_SETMLME,
    IW_PRIV_TYPE_MLME | IW_PRIV_SIZE_FIXED, 0,    "setmlme" },
    { IEEE80211_IOCTL_ADDMAC,
    IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1, 0,"addmac" },
    { IEEE80211_IOCTL_DELMAC,
    IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1, 0,"delmac" },
    { IEEE80211_IOCTL_GET_MACADDR,
    0, IW_PRIV_TYPE_ACLMACLIST,             "getmac" },
    { IEEE80211_IOCTL_KICKMAC,
    IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1, 0,"kickmac"},
    { IEEE80211_IOCTL_SETCHANLIST,
    IW_PRIV_TYPE_CHANLIST | IW_PRIV_SIZE_FIXED, 0,"setchanlist" },
    { IEEE80211_IOCTL_GETCHANLIST,
    0, IW_PRIV_TYPE_CHANLIST | IW_PRIV_SIZE_FIXED,"getchanlist" },
    { IEEE80211_IOCTL_GETCHANINFO,
    0, IW_PRIV_TYPE_CHANINFO | IW_PRIV_SIZE_FIXED,"getchaninfo" },
    { IEEE80211_IOCTL_SETMODE,
    IW_PRIV_TYPE_CHAR |  16, 0, "mode" },
    { IEEE80211_IOCTL_GETMODE,
    0, IW_PRIV_TYPE_CHAR | 16, "get_mode" },
#if WIRELESS_EXT >= 12
    { IEEE80211_IOCTL_SETWMMPARAMS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 4, 0,"setwmmparams" },
    { IEEE80211_IOCTL_GETWMMPARAMS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,   "getwmmparams" },
    /*
    * These depends on sub-ioctl support which added in version 12.
    */
    { IEEE80211_IOCTL_SETWMMPARAMS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0,"" },
    { IEEE80211_IOCTL_GETWMMPARAMS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,   "" },
    /* sub-ioctl handlers */
    { IEEE80211_WMMPARAMS_CWMIN,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0,"cwmin" },
    { IEEE80211_WMMPARAMS_CWMIN,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,   "get_cwmin" },
    { IEEE80211_WMMPARAMS_CWMAX,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0,"cwmax" },
    { IEEE80211_WMMPARAMS_CWMAX,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,   "get_cwmax" },
    { IEEE80211_WMMPARAMS_AIFS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0,"aifs" },
    { IEEE80211_WMMPARAMS_AIFS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,   "get_aifs" },
    { IEEE80211_WMMPARAMS_TXOPLIMIT,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0,"txoplimit" },
    { IEEE80211_WMMPARAMS_TXOPLIMIT,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,   "get_txoplimit" },
    { IEEE80211_WMMPARAMS_ACM,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0,"acm" },
    { IEEE80211_WMMPARAMS_ACM,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,   "get_acm" },
    { IEEE80211_WMMPARAMS_NOACKPOLICY,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0,"noackpolicy" },
    { IEEE80211_WMMPARAMS_NOACKPOLICY,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,   "get_noackpolicy" },

    { IEEE80211_IOCTL_SETPARAM,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "setparam" },
    /*
    * These depends on sub-ioctl support which added in version 12.
    */
    { IEEE80211_IOCTL_GETPARAM,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "getparam" },

    /*
    * sub-ioctl handlers
    * A fairly weak attempt to validate args. Only params below with
    * get/set args that match these are allowed by the user space iwpriv
    * program.
    */
    { IEEE80211_IOCTL_SETPARAM,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "" },
    { IEEE80211_IOCTL_SETPARAM,
    IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | IEEE80211_ADDR_LEN, 0, "" },
    { IEEE80211_IOCTL_GETPARAM,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "" },
    { IEEE80211_IOCTL_GETPARAM, 0,
    IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | IEEE80211_ADDR_LEN, "" },
    /*
    * sub-ioctl definitions
    *
    * IEEE80211_IOCTL_GETPARAM and IEEE80211_IOCTL_SETPARAM can 
    * only return 1 int. Other values below are wrong.
    * fixme - check "g_bssid"
    */
    { IEEE80211_PARAM_AUTHMODE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "authmode" },
    { IEEE80211_PARAM_AUTHMODE,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_authmode" },
    { IEEE80211_PARAM_PROTMODE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "protmode" },
    { IEEE80211_PARAM_PROTMODE,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_protmode" },
    { IEEE80211_PARAM_MCASTCIPHER,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mcastcipher" },
    { IEEE80211_PARAM_MCASTCIPHER,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_mcastcipher" },
    { IEEE80211_PARAM_MCASTKEYLEN,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mcastkeylen" },
    { IEEE80211_PARAM_MCASTKEYLEN,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_mcastkeylen" },
    { IEEE80211_PARAM_UCASTCIPHERS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ucastciphers" },
    { IEEE80211_PARAM_UCASTCIPHERS,
    /*
    * NB: can't use "get_ucastciphers" 'cuz iwpriv command names
    *     must be <IFNAMESIZ which is 16.
    */
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_uciphers" },
    { IEEE80211_PARAM_UCASTCIPHER,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ucastcipher" },
    { IEEE80211_PARAM_UCASTCIPHER,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ucastcipher" },
    { IEEE80211_PARAM_UCASTKEYLEN,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ucastkeylen" },
    { IEEE80211_PARAM_UCASTKEYLEN,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ucastkeylen" },
    { IEEE80211_PARAM_KEYMGTALGS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "keymgtalgs" },
    { IEEE80211_PARAM_KEYMGTALGS,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_keymgtalgs" },
    { IEEE80211_PARAM_RSNCAPS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rsncaps" },
    { IEEE80211_PARAM_RSNCAPS,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rsncaps" },
    { IEEE80211_PARAM_PRIVACY,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "privacy" },
    { IEEE80211_PARAM_PRIVACY,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_privacy" },
    { IEEE80211_PARAM_COUNTERMEASURES,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "countermeasures" },
    { IEEE80211_PARAM_COUNTERMEASURES,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_countermeas" },
    { IEEE80211_PARAM_DROPUNENCRYPTED,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dropunencrypted" },
    { IEEE80211_PARAM_DROPUNENCRYPTED,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_dropunencry" },
    { IEEE80211_PARAM_WPA,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wpa" },
    { IEEE80211_PARAM_WPA,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_wpa" },
    { IEEE80211_PARAM_DRIVER_CAPS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "driver_caps" },
    { IEEE80211_PARAM_DRIVER_CAPS,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_driver_caps" },
    { IEEE80211_PARAM_MACCMD,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "maccmd" },
    { IEEE80211_PARAM_MACCMD,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_maccmd" },
    { IEEE80211_PARAM_WMM,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wmm" },
    { IEEE80211_PARAM_WMM,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_wmm" },
    { IEEE80211_PARAM_HIDESSID,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "hide_ssid" },
    { IEEE80211_PARAM_HIDESSID,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_hide_ssid" },
    { IEEE80211_PARAM_APBRIDGE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ap_bridge" },
    { IEEE80211_PARAM_APBRIDGE,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ap_bridge" },
    { IEEE80211_PARAM_INACT,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "inact" },
    { IEEE80211_PARAM_INACT,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_inact" },
    { IEEE80211_PARAM_INACT_AUTH,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "inact_auth" },
    { IEEE80211_PARAM_INACT_AUTH,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_inact_auth" },
    { IEEE80211_PARAM_INACT_INIT,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "inact_init" },
    { IEEE80211_PARAM_INACT_INIT,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_inact_init" },
    { IEEE80211_PARAM_ABOLT,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "abolt" },
    { IEEE80211_PARAM_ABOLT,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_abolt" },
    { IEEE80211_PARAM_DTIM_PERIOD,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dtim_period" },
    { IEEE80211_PARAM_DTIM_PERIOD,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_dtim_period" },
    /* XXX bintval chosen to avoid 16-char limit */
    { IEEE80211_PARAM_BEACON_INTERVAL,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bintval" },
    { IEEE80211_PARAM_BEACON_INTERVAL,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bintval" },
#if ATH_SUPPORT_AP_WDS_COMBO
    { IEEE80211_PARAM_NO_BEACON,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "nobeacon" },
    { IEEE80211_PARAM_NO_BEACON,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_nobeacon" },
#endif
    { IEEE80211_PARAM_DOTH,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "doth" },
    { IEEE80211_PARAM_DOTH,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_doth" },
    { IEEE80211_PARAM_PWRTARGET,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "doth_pwrtgt" },
    { IEEE80211_PARAM_PWRTARGET,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_doth_pwrtgt" },
    { IEEE80211_PARAM_GENREASSOC,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "doth_reassoc" },
    { IEEE80211_PARAM_COMPRESSION,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "compression" },
    { IEEE80211_PARAM_COMPRESSION,0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_compression" },
    { IEEE80211_PARAM_FF,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ff" },
    { IEEE80211_PARAM_FF,0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ff" },
    { IEEE80211_PARAM_TURBO,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "turbo" },
    { IEEE80211_PARAM_TURBO,0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_turbo" },
    { IEEE80211_PARAM_BURST,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "burst" },
    { IEEE80211_PARAM_BURST,0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_burst" },
    { IEEE80211_IOCTL_CHANSWITCH,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "doth_chanswitch" },
    { IEEE80211_PARAM_PUREG,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "pureg" },
    { IEEE80211_PARAM_PUREG,0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_pureg" },
    { IEEE80211_PARAM_AR,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ar" },
    { IEEE80211_PARAM_AR,0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ar" },
    { IEEE80211_PARAM_WDS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wds" },
    { IEEE80211_PARAM_WDS,0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_wds" },
    { IEEE80211_PARAM_VAP_IND,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "vap_ind" },
    { IEEE80211_PARAM_VAP_IND,0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_vap_ind" },
#ifdef notyet
    { IEEE80211_PARAM_BGSCAN,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bgscan" },
    { IEEE80211_PARAM_BGSCAN,0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bgscan" },
    { IEEE80211_PARAM_BGSCAN_IDLE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bgscanidle" },
    { IEEE80211_PARAM_BGSCAN_IDLE,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bgscanidle" },
    { IEEE80211_PARAM_BGSCAN_INTERVAL,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bgscanintvl" },
    { IEEE80211_PARAM_BGSCAN_INTERVAL,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bgscanintvl" },
#endif
    { IEEE80211_PARAM_MCAST_RATE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mcast_rate" },
    { IEEE80211_PARAM_MCAST_RATE,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_mcast_rate" },
#ifdef notyet
    { IEEE80211_PARAM_COVERAGE_CLASS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "coverageclass" },
    { IEEE80211_PARAM_COVERAGE_CLASS,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_coveragecls" },
#endif
    { IEEE80211_PARAM_COUNTRY_IE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "countryie" },
    { IEEE80211_PARAM_COUNTRY_IE,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_countryie" },
#ifdef notyet
    { IEEE80211_PARAM_SCANVALID,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "scanvalid" },
    { IEEE80211_PARAM_SCANVALID,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_scanvalid" },
    { IEEE80211_PARAM_REGCLASS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "regclass" },
    { IEEE80211_PARAM_REGCLASS,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_regclass" },
#endif
    /*
    * NB: these should be roamrssi* etc, but iwpriv usurps all
    *     strings that start with roam!
    */
#ifdef notyet
    { IEEE80211_PARAM_ROAM_RSSI_11A,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rssi11a" },
    { IEEE80211_PARAM_ROAM_RSSI_11A,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rssi11a" },
    { IEEE80211_PARAM_ROAM_RSSI_11B,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rssi11b" },
    { IEEE80211_PARAM_ROAM_RSSI_11B,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rssi11b" },
    { IEEE80211_PARAM_ROAM_RSSI_11G,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rssi11g" },
    { IEEE80211_PARAM_ROAM_RSSI_11G,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rssi11g" },
    { IEEE80211_PARAM_ROAM_RATE_11A,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rate11a" },
    { IEEE80211_PARAM_ROAM_RATE_11A,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rate11a" },
    { IEEE80211_PARAM_ROAM_RATE_11B,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rate11b" },
    { IEEE80211_PARAM_ROAM_RATE_11B,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rate11b" },
    { IEEE80211_PARAM_ROAM_RATE_11G,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rate11g" },
    { IEEE80211_PARAM_ROAM_RATE_11G,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rate11g" },
#endif
    { IEEE80211_PARAM_UAPSDINFO,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "uapsd" },
    { IEEE80211_PARAM_UAPSDINFO,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_uapsd" },
    { IEEE80211_PARAM_SLEEP,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "sleep" },
    { IEEE80211_PARAM_SLEEP,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_sleep" },
    { IEEE80211_PARAM_QOSNULL,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "qosnull" },
    { IEEE80211_PARAM_PSPOLL,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "pspoll" },
    { IEEE80211_PARAM_EOSPDROP,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "eospdrop" },
    { IEEE80211_PARAM_EOSPDROP,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_eospdrop" },
    { IEEE80211_PARAM_MARKDFS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "markdfs" },
    { IEEE80211_PARAM_MARKDFS,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_markdfs" },
    { IEEE80211_PARAM_CHANBW,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "chanbw" },
    { IEEE80211_PARAM_CHANBW,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_chanbw" },
    { IEEE80211_PARAM_SHORTPREAMBLE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "shpreamble" },
    { IEEE80211_PARAM_SHORTPREAMBLE,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_shpreamble" },
    { IEEE80211_PARAM_BLOCKDFSCHAN,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "blockdfschan" },
    { IEEE80211_IOCTL_SET_APPIEBUF,
    IW_PRIV_TYPE_APPIEBUF, 0,                     "setiebuf" },
    { IEEE80211_IOCTL_GET_APPIEBUF,
    0, IW_PRIV_TYPE_APPIEBUF,                     "getiebuf" },
    { IEEE80211_IOCTL_FILTERFRAME,
    IW_PRIV_TYPE_FILTER , 0,                      "setfilter" },
#if(0)
    { IEEE80211_PARAM_RADIO,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "radio" },
    { IEEE80211_PARAM_RADIO,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_radio" },
#endif
    { IEEE80211_PARAM_NETWORK_SLEEP,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "powersave" },
    { IEEE80211_PARAM_NETWORK_SLEEP,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_powersave" },
    { IEEE80211_PARAM_CWM_EXTPROTMODE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "extprotmode" },
    { IEEE80211_PARAM_CWM_EXTPROTMODE,0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_extprotmode" },
    { IEEE80211_PARAM_CWM_EXTPROTSPACING,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "extprotspac" },
    { IEEE80211_PARAM_CWM_EXTPROTSPACING,0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_extprotspac" },
    { IEEE80211_PARAM_CWM_ENABLE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "cwmenable" },
    { IEEE80211_PARAM_CWM_ENABLE,0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_cwmenable" },
    { IEEE80211_PARAM_CWM_EXTBUSYTHRESHOLD,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "extbusythres" },
    { IEEE80211_PARAM_CWM_EXTBUSYTHRESHOLD,0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_extbusythres" },
    { IEEE80211_PARAM_SHORT_GI,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "shortgi" },
    { IEEE80211_PARAM_SHORT_GI,0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_shortgi" },
    /*
    * 11n A-MPDU, A-MSDU support
    */
    { IEEE80211_PARAM_AMPDU,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ampdu" },
    { IEEE80211_PARAM_AMPDU,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ampdu" },
    { IEEE80211_PARAM_RESET_ONCE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "reset" },

    { IEEE80211_PARAM_COUNTRYCODE,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_countrycode" },
#if ATH_SUPPORT_IQUE
    /*
    * Mcast enhancement
    */
    { IEEE80211_PARAM_ME,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mcastenhance" },
    { IEEE80211_PARAM_ME, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_mcastenhance" },
    { IEEE80211_PARAM_MEDUMP,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "medump_dummy" },
    { IEEE80211_PARAM_MEDUMP, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "medump" },
    { IEEE80211_PARAM_MEDEBUG,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "medebug" },
    { IEEE80211_PARAM_MEDEBUG, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_medebug" },
    { IEEE80211_PARAM_ME_SNOOPLENGTH,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "me_length" },
    { IEEE80211_PARAM_ME_SNOOPLENGTH, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_me_length" },
    { IEEE80211_PARAM_ME_TIMER,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "metimer" },
    { IEEE80211_PARAM_ME_TIMER, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_metimer" },
    { IEEE80211_PARAM_ME_TIMEOUT,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "metimeout" },
    { IEEE80211_PARAM_ME_TIMEOUT, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_metimeout" },
    { IEEE80211_PARAM_ME_DROPMCAST,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "medropmcast" },
    { IEEE80211_PARAM_ME_DROPMCAST, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_medropmcast" },
    { IEEE80211_PARAM_ME_SHOWDENY, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "me_showdeny" },
    { IEEE80211_PARAM_ME_CLEARDENY,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "me_cleardeny" },

    /*
    *  Headline block removal
    */
    { IEEE80211_PARAM_HBR_TIMER,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "hbrtimer" },
    { IEEE80211_PARAM_HBR_TIMER, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_hbrtimer" },
    { IEEE80211_PARAM_HBR_STATE, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_hbrstate" },

    /*
    * rate control / AC parameters for IQUE
    */
    { IEEE80211_PARAM_GETIQUECONFIG, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_iqueconfig" },
    { IEEE80211_IOCTL_SET_ACPARAMS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 4, 0, "acparams" },
    { IEEE80211_IOCTL_SET_RTPARAMS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "rtparams" },
    { IEEE80211_IOCTL_SET_HBRPARAMS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "hbrparams" },
    { IEEE80211_IOCTL_SET_MEDENYENTRY,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 4, 0, "me_adddeny" },
#endif

#if UMAC_SUPPORT_RPTPLACEMENT
    /*
    * Repeater placement
    */
    { IEEE80211_PARAM_CUSTPROTO_ENABLE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rptcustproto" },
    { IEEE80211_PARAM_CUSTPROTO_ENABLE, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getrptcustproto" },
    { IEEE80211_PARAM_GPUTCALC_ENABLE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rptgputcalc" },
    { IEEE80211_PARAM_GPUTCALC_ENABLE, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getrptgputcalc" },
    { IEEE80211_PARAM_DEVUP,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rptdevup" },
    { IEEE80211_PARAM_DEVUP, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getrptdevup" },
    { IEEE80211_PARAM_MACDEV,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rptmacdev" },
    { IEEE80211_PARAM_MACDEV, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getrptmacdev" },
    { IEEE80211_PARAM_MACADDR1,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rptmacaddr1" },
    { IEEE80211_PARAM_MACADDR1, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getrptmacaddr1" },
    { IEEE80211_PARAM_MACADDR2,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rptmacaddr2" },
    { IEEE80211_PARAM_MACADDR2, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getrptmacaddr2" },
    { IEEE80211_PARAM_GPUTMODE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rptgputmode" },
    { IEEE80211_PARAM_GPUTMODE, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getrptgputmode" },

    { IEEE80211_PARAM_TXPROTOMSG,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rpttxprotomsg" },
    { IEEE80211_PARAM_TXPROTOMSG, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getrpttxprotomsg" },
    { IEEE80211_PARAM_RXPROTOMSG,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rptrxprotomsg" },
    { IEEE80211_PARAM_RXPROTOMSG, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getrptrxprotomsg" },
    { IEEE80211_PARAM_STATUS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rptstatus" },
    { IEEE80211_PARAM_STATUS, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getrptstatus" },
    { IEEE80211_PARAM_ASSOC,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rptassoc" },
    { IEEE80211_PARAM_ASSOC, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getrptassoc" },

    { IEEE80211_PARAM_NUMSTAS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rptnumstas" },
    { IEEE80211_PARAM_NUMSTAS, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getrptnumstas" },
    { IEEE80211_PARAM_STA1ROUTE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rptsta1route" },
    { IEEE80211_PARAM_STA1ROUTE, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getrptsta1route" },
    { IEEE80211_PARAM_STA2ROUTE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rptsta2route" },
    { IEEE80211_PARAM_STA2ROUTE, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getrptsta2route" },
    { IEEE80211_PARAM_STA3ROUTE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rptsta3route" },
    { IEEE80211_PARAM_STA3ROUTE, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getrptsta3route" },
    { IEEE80211_PARAM_STA4ROUTE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rptsta4route" },
    { IEEE80211_PARAM_STA4ROUTE, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getrptsta4route" },

#endif

#if UMAC_SUPPORT_TDLS
    { IEEE80211_PARAM_TDLS_MACADDR1,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tdlsmacaddr1" },
    { IEEE80211_PARAM_TDLS_MACADDR1, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "gettdlsmacaddr1" },
    { IEEE80211_PARAM_TDLS_MACADDR2,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tdlsmacaddr2" },
    { IEEE80211_PARAM_TDLS_MACADDR2, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "gettdlsmacaddr2" },
    { IEEE80211_PARAM_TDLS_ACTION,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tdlsaction" },
    { IEEE80211_PARAM_TDLS_ACTION, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "gettdlsaction" },
#endif

    { IEEE80211_IOCTL_SET_RXTIMEOUT,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "rxtimeout" },
    { IEEE80211_IOCTL_SENDADDBA,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "addba" },
    { IEEE80211_IOCTL_GETADDBASTATUS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getaddbastatus" },
    { IEEE80211_IOCTL_SENDDELBA,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 4, 0, "delba" },
    { IEEE80211_PARAM_SETADDBAOPER,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setaddbaoper" },
    { IEEE80211_IOCTL_SET_ADDBARESP,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "addbaresp" },
    { IEEE80211_PARAM_11N_RATE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set11NRates" },
    { IEEE80211_PARAM_11N_RATE,
    0,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get11NRates" },
    { IEEE80211_PARAM_11N_RETRIES,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set11NRetries" },
    { IEEE80211_PARAM_11N_RETRIES,
    0,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get11NRetries" },
    { IEEE80211_PARAM_DBG_LVL,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dbgLVL" },
    { IEEE80211_PARAM_DBG_LVL,
    0,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getdbgLVL" },
#if UMAC_SUPPORT_IBSS
    { IEEE80211_PARAM_IBSS_CREATE_DISABLE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "noIBSSCreate" },
    { IEEE80211_PARAM_IBSS_CREATE_DISABLE,
    0,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_noIBSSCreate" },
#endif
    { IEEE80211_PARAM_WEATHER_RADAR_CHANNEL,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "no_wradar" },
    { IEEE80211_PARAM_WEATHER_RADAR_CHANNEL,
    0,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_no_wradar" },
    { IEEE80211_PARAM_WDS_AUTODETECT,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wdsdetect" },
    { IEEE80211_PARAM_WDS_AUTODETECT,0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_wdsdetect" },
    { IEEE80211_PARAM_WEP_TKIP_HT,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "htweptkip" },
    { IEEE80211_PARAM_WEP_TKIP_HT, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_htweptkip" },
    { IEEE80211_PARAM_WEP_KEYCACHE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wepkeycache" },
    { IEEE80211_PARAM_WEP_KEYCACHE,
    0,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_wepkeycache" },
    /*
    ** This is a "get" only
    */
#ifdef notyet
    { IEEE80211_PARAM_ATH_RADIO,0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getRadio" },
#endif
    { IEEE80211_PARAM_PUREN,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "puren" },
    { IEEE80211_PARAM_PUREN,0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_puren" },
    { IEEE80211_PARAM_BASICRATES,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "basicrates" },
    /*
    * 11d Beacon processing
    */
    { IEEE80211_PARAM_IGNORE_11DBEACON,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ignore11d"},
    { IEEE80211_PARAM_IGNORE_11DBEACON,0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ignore11d"},
    { IEEE80211_PARAM_STA_FORWARD,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "stafwd" },
    { IEEE80211_PARAM_STA_FORWARD,0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_stafwd" },
#ifdef ATH_EXT_AP
    { IEEE80211_PARAM_EXTAP,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "extap" },
    { IEEE80211_PARAM_EXTAP, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_extap" },
#endif
    { IEEE80211_PARAM_CLR_APPOPT_IE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "clrappoptie" },
    { IEEE80211_PARAM_AUTO_ASSOC,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "autoassoc" },
    { IEEE80211_PARAM_AUTO_ASSOC,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_autoassoc" }, 
    { IEEE80211_PARAM_VAP_COUNTRY_IE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "vap_contryie" },
    { IEEE80211_PARAM_VAP_COUNTRY_IE,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_vapcontryie" },
    { IEEE80211_PARAM_VAP_DOTH,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "vap_doth" },
    { IEEE80211_PARAM_VAP_DOTH,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_vap_doth" },
    { IEEE80211_PARAM_HT40_INTOLERANT,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ht40intol" },
    { IEEE80211_PARAM_HT40_INTOLERANT,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ht40intol" },
    { IEEE80211_PARAM_CHWIDTH,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "chwidth" },
    { IEEE80211_PARAM_CHWIDTH,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_chwidth" },
    { IEEE80211_PARAM_CHEXTOFFSET,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "chextoffset" },
    { IEEE80211_PARAM_CHEXTOFFSET,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_chextoffset" },
    { IEEE80211_PARAM_STA_QUICKKICKOUT,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "sko" },
    { IEEE80211_PARAM_STA_QUICKKICKOUT,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_sko" },		  
    { IEEE80211_PARAM_CHSCANINIT,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "chscaninit" },
    { IEEE80211_PARAM_CHSCANINIT,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_chscaninit" },
    { IEEE80211_PARAM_COEXT_DISABLE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "disablecoext" },
    { IEEE80211_PARAM_COEXT_DISABLE,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_disablecoext" },
    { IEEE80211_PARAM_MFP_TEST,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "mfptest" },
    { IEEE80211_PARAM_MFP_TEST,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_mfptest" },
#if ATH_RXBUF_RECYCLE
    { IEEE80211_PARAM_RXBUF_LIFETIME,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rxbuf_lifetime" },
    { IEEE80211_PARAM_RXBUF_LIFETIME,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rxbuflife" },
#endif

#if  ATH_SUPPORT_AOW
    { IEEE80211_PARAM_SWRETRIES,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "sw_retries"},
    { IEEE80211_PARAM_SWRETRIES,0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_sw_retries"},
    { IEEE80211_PARAM_AOW_LATENCY,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "aow_latency"},
    { IEEE80211_PARAM_AOW_LATENCY,0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_aow_latency"},
    { IEEE80211_PARAM_AOW_STATS,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "aow_clearstats"},
    { IEEE80211_PARAM_AOW_STATS,0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_aow_stats"},
    { IEEE80211_PARAM_AOW_ESTATS,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "aow_clearestats"},
    { IEEE80211_PARAM_AOW_ESTATS,0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_aow_estats"},
    { IEEE80211_PARAM_AOW_INTERLEAVE,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_aow_inter"},
    { IEEE80211_PARAM_AOW_INTERLEAVE,0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_aow_inter"},
    { IEEE80211_PARAM_AOW_ER,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_aow_er"},
    { IEEE80211_PARAM_AOW_ER,0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_aow_er"},
    { IEEE80211_PARAM_AOW_EC,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_aow_ec"},
    { IEEE80211_PARAM_AOW_EC,0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_aow_ec"},
    { IEEE80211_PARAM_AOW_EC_FMAP,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_aow_ec_fmap"},
    { IEEE80211_PARAM_AOW_EC_FMAP,0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_aow_ec_fmap"},
    { IEEE80211_PARAM_AOW_ENABLE_CAPTURE,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_aow_capture"},
    { IEEE80211_PARAM_AOW_PRINT_CAPTURE,0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "print_aow_cptr"},
    { IEEE80211_PARAM_AOW_FORCE_INPUT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "aow_force_inp"},
    { IEEE80211_PARAM_AOW_LIST_AUDIO_CHANNELS,0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_aow_channels"},
    { IEEE80211_PARAM_AOW_PLAY_LOCAL,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "aow_playlocal"},
    { IEEE80211_PARAM_AOW_PLAY_LOCAL,0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "g_aow_playlocal"},
    { IEEE80211_PARAM_AOW_CLEAR_AUDIO_CHANNELS,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "aow_clear_ch"},
    { IEEE80211_PARAM_AOW_ES,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_aow_es"},
    { IEEE80211_PARAM_AOW_ES,0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_aow_es"},
    { IEEE80211_PARAM_AOW_ESS,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_aow_ess"},
    { IEEE80211_PARAM_AOW_ESS,0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_aow_ess"},
    { IEEE80211_PARAM_AOW_ESS_COUNT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "aow_ess_count"},

#endif  /* ATH_SUPPORT_AOW */

#ifdef ATH_SUPPORT_GREEN_AP
      { IEEE80211_IOCTL_GREEN_AP_PS_ENABLE, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ant_ps_on" }, 
      { IEEE80211_IOCTL_GREEN_AP_PS_ENABLE, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ant_ps_on" }, 
      { IEEE80211_IOCTL_GREEN_AP_PS_TIMEOUT, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ps_timeout" }, 
      { IEEE80211_IOCTL_GREEN_AP_PS_TIMEOUT, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ps_timeout" }, 
#if 0 /* No cycling for now EV 69649*/
      { IEEE80211_IOCTL_GREEN_AP_PS_ON_TIME, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ps_on_time" }, 
      { IEEE80211_IOCTL_GREEN_AP_PS_ON_TIME, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ps_on_time" }, 
#endif /* No cycling for now */
#endif /*ATH_SUPPORT_GREEN_AP */

#endif /* WIRELESS_EXT >= 12 */

#if ATH_SUPPORT_WAPI
    { IEEE80211_PARAM_SETWAPI,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setwapi" },
    { IEEE80211_PARAM_WAPIREKEY_USK,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wapi_rkupkt" },
    { IEEE80211_PARAM_WAPIREKEY_USK, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_wapi_rkupkt" },
    { IEEE80211_PARAM_WAPIREKEY_MSK,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wapi_rkmpkt" },
    { IEEE80211_PARAM_WAPIREKEY_MSK, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_wapi_rkmpkt" },  
    { IEEE80211_PARAM_WAPIREKEY_UPDATE,
    IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | IEEE80211_ADDR_LEN, 0, "wapi_rkupdate" },
#endif /*ATH_SUPPORT_WAPI*/
#ifdef ATH_WPS_IE
    { IEEE80211_PARAM_WPS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wps" },
    { IEEE80211_PARAM_WPS, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_wps" },
#endif
    { IEEE80211_PARAM_CCMPSW_ENCDEC,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ccmpSwSelEn" },
    { IEEE80211_PARAM_CCMPSW_ENCDEC, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ccmpSwSelEn" },
#if ATHEROS_LINUX_P2P_DRIVER
    { IEEE80211_IOC_MCASTCIPHER,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "s_mcastcipher" },
    { IEEE80211_IOC_MCASTCIPHER,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "g_mcastcipher" },
    { IEEE80211_IOC_P2P_OPMODE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "s_p2p_opmode" },
    { IEEE80211_IOC_P2P_OPMODE, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "g_p2p_opmode" },
    { IEEE80211_IOC_BSSID, 0,
    IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | IEEE80211_ADDR_LEN, "g_bssid"},
    { IEEE80211_IOC_SCAN_FLUSH,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "s_scan_flush"},
    { IEEE80211_IOC_WPS_MODE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "s_wps_mode"},
    { IEEE80211_IOC_P2P_GO_OPPPS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "s_oppps"},
    { IEEE80211_IOC_P2P_GO_CTWINDOW,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "s_ctwin"},
    { IEEE80211_IOC_P2P_CANCEL_CHANNEL,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "s_cancel_chan"},
    { IEEE80211_IOC_CHANNEL,
    IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | sizeof(struct ieee80211_ioc_channel),
                                                0, "s_channel2"},
    { IEEE80211_IOC_START_HOSTAP,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "s_start_hostap"},
    { IEEE80211_IOC_CONNECTION_STATE, 0,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "g_conn_state" },
    { IEEE80211_IOC_P2P_RADIO_IDX,0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_radio_idx"},
#endif
#if UMAC_SUPPORT_TDLS
    { IEEE80211_PARAM_TDLS_ENABLE,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "tdls" },
    { IEEE80211_PARAM_TDLS_ENABLE, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_tdls" },
    { IEEE80211_PARAM_SET_TDLS_RMAC,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_tdls_rmac" },
    { IEEE80211_PARAM_CLR_TDLS_RMAC,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "clr_tdls_rmac" },
#ifdef CONFIG_RCPI
    { IEEE80211_PARAM_TDLS_GET_RCPI, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rcpiparam" },
    { IEEE80211_PARAM_TDLS_SET_RCPI,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_rcpi" },
    { IEEE80211_PARAM_TDLS_RCPI_HI,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_rcpihi" },
    { IEEE80211_PARAM_TDLS_RCPI_LOW,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_rcpilo" },
    { IEEE80211_PARAM_TDLS_RCPI_MARGIN,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_rcpimargin" },
#endif /* CONFIG_RCPI */
#endif /* UMAC_SUPPORT_TDLS */
#ifdef ATHEROS_LINUX_PERIODIC_SCAN
    { IEEE80211_PARAM_PERIODIC_SCAN,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "periodicScan" },
    { IEEE80211_PARAM_PERIODIC_SCAN,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_periodicScan" },
#endif
#ifdef ATH_SW_WOW
    { IEEE80211_PARAM_SW_WOW,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "sw_wow" },
    { IEEE80211_PARAM_SW_WOW,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_sw_wow" },
#endif
#if UMAC_SUPPORT_WDS    
    { IEEE80211_IOCTL_GET_MACADDR,
      IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 2, 0, "" },
    { IEEE80211_PARAM_ADD_WDS_ADDR,
      IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 2, 0, "wdsaddr" },
#endif
#ifdef __CARRIER_PLATFORM__
      { IEEE80211_PARAM_PLTFRM_PRIVATE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, CARRIER_PLTFRM_PRIVATE_SET },
      { IEEE80211_PARAM_PLTFRM_PRIVATE, 0, 
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, CARRIER_PLTFRM_PRIVATE_GET },
#endif 
#if UMAC_SUPPORT_VI_DBG
   /* Video debug parameters */   
    { IEEE80211_PARAM_DBG_CFG,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dbgcfg" },
    { IEEE80211_PARAM_DBG_CFG, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getdbgcfg" },
    { IEEE80211_PARAM_DBG_NUM_STREAMS,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "numstreams" },
    { IEEE80211_PARAM_DBG_NUM_STREAMS, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getnumstreams" },
    { IEEE80211_PARAM_STREAM_NUM,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "streamnum" },
    { IEEE80211_PARAM_STREAM_NUM, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getstreamnum" },
    { IEEE80211_PARAM_DBG_NUM_MARKERS,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "nummarkers" },
    { IEEE80211_PARAM_DBG_NUM_MARKERS, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getnummarkers" },
    { IEEE80211_PARAM_MARKER_NUM,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "markernum" },
    { IEEE80211_PARAM_MARKER_NUM, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getmarkernum" },
    { IEEE80211_PARAM_MARKER_OFFSET_SIZE,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "markeroffsize" },
    { IEEE80211_PARAM_MARKER_OFFSET_SIZE, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getmarkeroffsize" },
    { IEEE80211_PARAM_MARKER_MATCH,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "markermatch" },
    { IEEE80211_PARAM_MARKER_MATCH, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getmarkermatch" },
    { IEEE80211_PARAM_RXSEQ_OFFSET_SIZE,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rxseqnum" },
    { IEEE80211_PARAM_RXSEQ_OFFSET_SIZE, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getrxseqnum" },
    { IEEE80211_PARAM_TIME_OFFSET_SIZE,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "timestamp" },
    { IEEE80211_PARAM_TIME_OFFSET_SIZE, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "gettimestamp" },
    { IEEE80211_PARAM_RESTART,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dbgrestart" },
    { IEEE80211_PARAM_RESTART, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getdbgrestart" },
    { IEEE80211_PARAM_RX_SEQ_RSHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rxseqrshift" },
    { IEEE80211_PARAM_RX_SEQ_RSHIFT, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getrxseqrshift" },
    { IEEE80211_PARAM_RX_SEQ_MAX,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rxseqmax" },
    { IEEE80211_PARAM_RX_SEQ_MAX, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getrxseqmax" },
    { IEEE80211_PARAM_RX_SEQ_DROP,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rxseqdrop" },
    { IEEE80211_PARAM_RXDROP_STATUS,
          IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rxdropstats" },
    { IEEE80211_PARAM_RXDROP_STATUS, 0,
          IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getrxdropstats" },

#endif    

#if ATH_SUPPORT_IBSS_DFS
    { IEEE80211_PARAM_IBSS_DFS_PARAM,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setibssdfsparam" },
    { IEEE80211_PARAM_IBSS_DFS_PARAM,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getibssdfsparam" },
#endif

#if ATH_SUPPORT_FLOWMAC_MODULE
    { IEEE80211_PARAM_FLOWMAC,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "flowmac" },
    { IEEE80211_PARAM_FLOWMAC,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_flowmac" },
#endif
#if UMAC_SUPPORT_SMARTANTENNA
    { IEEE80211_PARAM_SMARTANT_TRAIN_MODE,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ant_trainmode" },
    { IEEE80211_PARAM_SMARTANT_TRAIN_MODE, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getant_trainmode" },
    { IEEE80211_PARAM_SMARTANT_TRAIN_TYPE,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ant_traintype" },
    { IEEE80211_PARAM_SMARTANT_TRAIN_TYPE, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getant_traintype" },
    { IEEE80211_PARAM_SMARTANT_PKT_LEN,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ant_pktlen" },
    { IEEE80211_PARAM_SMARTANT_PKT_LEN, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getant_pktlen" },
    { IEEE80211_PARAM_SMARTANT_NUM_PKTS,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ant_numpkts" },
    { IEEE80211_PARAM_SMARTANT_NUM_PKTS, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getant_numpkts" },
    { IEEE80211_PARAM_SMARTANT_TRAIN_START,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ant_train" },
    { IEEE80211_PARAM_SMARTANT_TRAIN_START, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getant_train" },
    { IEEE80211_PARAM_SMARTANT_NUM_ITR,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ant_numitr" },
    { IEEE80211_PARAM_SMARTANT_NUM_ITR, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getant_numitr" },
#endif
#if UMAC_SUPPORT_INTERNALANTENNA    
    { IEEE80211_PARAM_SMARTANT_CURRENT_ANTENNA,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "current_ant" },
    { IEEE80211_PARAM_SMARTANT_CURRENT_ANTENNA, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getcurrent_ant" },
    { IEEE80211_PARAM_SMARTANT_DEFAULT_ANTENNA,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "default_ant" },
    { IEEE80211_PARAM_SMARTANT_DEFAULT_ANTENNA, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getdefault_ant" },
#endif    

#if UMAC_SUPPORT_SMARTANTENNA
    { IEEE80211_PARAM_SMARTANT_AUTOMATION,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ant_automation" },
    { IEEE80211_PARAM_SMARTANT_AUTOMATION, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "gant_automation" },
    { IEEE80211_PARAM_SMARTANT_RETRAIN,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ant_retrain" },
    { IEEE80211_PARAM_SMARTANT_RETRAIN, 0, 
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getant_retrain" },

#endif    
    { IEEE80211_PARAM_MAXSTA,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "maxsta"},
    { IEEE80211_PARAM_MAXSTA,0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_maxsta"}, 
#if ATH_SUPPORT_IBSS_NETLINK_NOTIFICATION
    { IEEE80211_PARAM_IBSS_SET_RSSI_CLASS,
    IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | IEEE80211_ADDR_LEN, 0, "setibssrssiclass"},
    { IEEE80211_PARAM_IBSS_START_RSSI_MONITOR,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "startibssrssimon" },
    { IEEE80211_PARAM_IBSS_RSSI_HYSTERESIS,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setibssrssihyst" },
#endif
#ifdef ATH_SUPPORT_TxBF
    { IEEE80211_PARAM_TXBF_AUTO_CVUPDATE,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "autocvupdate" },
    { IEEE80211_PARAM_TXBF_AUTO_CVUPDATE,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_autocvupdate" },
    { IEEE80211_PARAM_TXBF_CVUPDATE_PER,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "cvupdateper" },
    { IEEE80211_PARAM_TXBF_CVUPDATE_PER,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_cvupdateper" },
#endif
    { IEEE80211_PARAM_SCAN_BAND,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "scanband" },
    { IEEE80211_PARAM_SCAN_BAND,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_scanband" },
#if ATH_SUPPORT_WPA_SUPPLICANT_CHECK_TIME
	{ IEEE80211_PARAM_REJOINT_ATTEMP_TIME,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rejoingtime" },
    { IEEE80211_PARAM_REJOINT_ATTEMP_TIME,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rejoingtime" },    
#endif    
    { IEEE80211_PARAM_GET_ACS,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_acs_state" },
    { IEEE80211_PARAM_GET_CAC,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_cac_state" },

};


static const iw_handler ieee80211_handlers[] = {
    (iw_handler) NULL,              /* SIOCSIWCOMMIT */
    (iw_handler) ieee80211_ioctl_giwname,       /* SIOCGIWNAME */
    (iw_handler) NULL,              /* SIOCSIWNWID */
    (iw_handler) NULL,              /* SIOCGIWNWID */
    (iw_handler) ieee80211_ioctl_siwfreq,       /* SIOCSIWFREQ */
    (iw_handler) ieee80211_ioctl_giwfreq,       /* SIOCGIWFREQ */
    (iw_handler) ieee80211_ioctl_siwmode,       /* SIOCSIWMODE */
    (iw_handler) ieee80211_ioctl_giwmode,       /* SIOCGIWMODE */
    (iw_handler) NULL,       /* SIOCSIWSENS */
    (iw_handler) NULL,       /* SIOCGIWSENS */
    (iw_handler) NULL /* not used */,       /* SIOCSIWRANGE */
    (iw_handler) ieee80211_ioctl_giwrange,  /* SIOCGIWRANGE */
    (iw_handler) NULL /* not used */,       /* SIOCSIWPRIV */
    (iw_handler) NULL /* kernel code */,        /* SIOCGIWPRIV */
    (iw_handler) NULL /* not used */,       /* SIOCSIWSTATS */
    (iw_handler) NULL /* kernel code */,        /* SIOCGIWSTATS */
#if ATH_SUPPORT_IWSPY
    (iw_handler) ieee80211_ioctl_siwspy,              /* SIOCSIWSPY */
    (iw_handler) ieee80211_ioctl_giwspy,              /* SIOCGIWSPY */
#else
    (iw_handler) NULL,              /* SIOCSIWSPY */
    (iw_handler) NULL,              /* SIOCGIWSPY */
#endif
    (iw_handler) NULL,              /* -- hole -- */
    (iw_handler) NULL,              /* -- hole -- */
    (iw_handler) ieee80211_ioctl_siwap,     /* SIOCSIWAP */
    (iw_handler) ieee80211_ioctl_giwap,     /* SIOCGIWAP */
#ifdef SIOCSIWMLME
    (iw_handler) NULL,      /* SIOCSIWMLME */
#else
    (iw_handler) NULL,              /* -- hole -- */
#endif
    (iw_handler) ieee80211_ioctl_iwaplist,      /* SIOCGIWAPLIST */
#ifdef SIOCGIWSCAN
    (iw_handler) ieee80211_ioctl_siwscan,       /* SIOCSIWSCAN */
    (iw_handler) ieee80211_ioctl_giwscan,       /* SIOCGIWSCAN */
#else
    (iw_handler) NULL,              /* SIOCSIWSCAN */
    (iw_handler) NULL,              /* SIOCGIWSCAN */
#endif /* SIOCGIWSCAN */
    (iw_handler) ieee80211_ioctl_siwessid,      /* SIOCSIWESSID */
    (iw_handler) ieee80211_ioctl_giwessid,      /* SIOCGIWESSID */
    (iw_handler) NULL,      /* SIOCSIWNICKN */
    (iw_handler) NULL,      /* SIOCGIWNICKN */
    (iw_handler) NULL,              /* -- hole -- */
    (iw_handler) NULL,              /* -- hole -- */
    (iw_handler) ieee80211_ioctl_siwrate,       /* SIOCSIWRATE */
    (iw_handler) ieee80211_ioctl_giwrate,       /* SIOCGIWRATE */
    (iw_handler) ieee80211_ioctl_siwrts,        /* SIOCSIWRTS */
    (iw_handler) ieee80211_ioctl_giwrts,        /* SIOCGIWRTS */
    (iw_handler) ieee80211_ioctl_siwfrag,       /* SIOCSIWFRAG */
    (iw_handler) ieee80211_ioctl_giwfrag,       /* SIOCGIWFRAG */
    (iw_handler) ieee80211_ioctl_siwtxpow,      /* SIOCSIWTXPOW */
    (iw_handler) ieee80211_ioctl_giwtxpow,      /* SIOCGIWTXPOW */
    (iw_handler) NULL,      /* SIOCSIWRETRY */
    (iw_handler) NULL,      /* SIOCGIWRETRY */
    (iw_handler) ieee80211_ioctl_siwencode,     /* SIOCSIWENCODE */
    (iw_handler) ieee80211_ioctl_giwencode,     /* SIOCGIWENCODE */
    (iw_handler) ieee80211_ioctl_siwpower,      /* SIOCSIWPOWER */
    (iw_handler) ieee80211_ioctl_giwpower,      /* SIOCGIWPOWER */
    (iw_handler) NULL,              /* -- hole -- */
    (iw_handler) NULL,              /* -- hole -- */
#if WIRELESS_EXT >= 18
    (iw_handler) NULL,      /* SIOCSIWGENIE */
    (iw_handler) NULL,      /* SIOCGIWGENIE */
    (iw_handler) NULL,      /* SIOCSIWAUTH */
    (iw_handler) NULL,      /* SIOCGIWAUTH */
    (iw_handler) NULL,  /* SIOCSIWENCODEEXT */
    (iw_handler) NULL,  /* SIOCGIWENCODEEXT */
#endif /* WIRELESS_EXT >= 18 */
};

static const iw_handler ieee80211_priv_handlers[] = {
    (iw_handler) ieee80211_ioctl_setparam,      /* SIOCWFIRSTPRIV+0 */
    (iw_handler) ieee80211_ioctl_getparam,      /* SIOCWFIRSTPRIV+1 */
    (iw_handler) ieee80211_ioctl_setkey,        /* SIOCWFIRSTPRIV+2 */
    (iw_handler) ieee80211_ioctl_setwmmparams,  /* SIOCWFIRSTPRIV+3 */
    (iw_handler) ieee80211_ioctl_delkey,        /* SIOCWFIRSTPRIV+4 */
    (iw_handler) ieee80211_ioctl_getwmmparams,  /* SIOCWFIRSTPRIV+5 */
    (iw_handler) ieee80211_ioctl_setmlme,       /* SIOCWFIRSTPRIV+6 */
    (iw_handler) ieee80211_ioctl_getchaninfo,   /* SIOCWFIRSTPRIV+7 */
    (iw_handler) ieee80211_ioctl_setoptie,      /* SIOCWFIRSTPRIV+8 */
    (iw_handler) ieee80211_ioctl_getoptie,      /* SIOCWFIRSTPRIV+9 */
    (iw_handler) ieee80211_ioctl_addmac,        /* SIOCWFIRSTPRIV+10 */
    (iw_handler) ieee80211_ioctl_getscanresults,/* SIOCWFIRSTPRIV+11 */
    (iw_handler) ieee80211_ioctl_delmac,        /* SIOCWFIRSTPRIV+12 */
    (iw_handler) ieee80211_ioctl_getchanlist,   /* SIOCWFIRSTPRIV+13 */
    (iw_handler) ieee80211_ioctl_setchanlist,   /* SIOCWFIRSTPRIV+14 */
    (iw_handler) ieee80211_ioctl_kickmac,       /* SIOCWFIRSTPRIV+15 */
    (iw_handler) ieee80211_ioctl_chanswitch,    /* SIOCWFIRSTPRIV+16 */
    (iw_handler) ieee80211_ioctl_getmode,       /* SIOCWFIRSTPRIV+17 */
    (iw_handler) ieee80211_ioctl_setmode,       /* SIOCWFIRSTPRIV+18 */
    (iw_handler) ieee80211_ioctl_getappiebuf,   /* SIOCWFIRSTPRIV+19 */
    (iw_handler) ieee80211_ioctl_setappiebuf,   /* SIOCWFIRSTPRIV+20 */
#if ATH_SUPPORT_IQUE
    (iw_handler) ieee80211_ioctl_setacparams,   /* SIOCWFIRSTPRIV+21 */
#else
    (iw_handler) NULL,                          /* SIOCWFIRSTPRIV+21 */
#endif
    (iw_handler) ieee80211_ioctl_setfilter,     /* SIOCWFIRSTPRIV+22 */
#if ATH_SUPPORT_IQUE
    (iw_handler) ieee80211_ioctl_setrtparams,   /* SIOCWFIRSTPRIV+23 */
#else
    (iw_handler) NULL,                          /* SIOCWFIRSTPRIV+23 */
#endif
    (iw_handler) ieee80211_ioctl_sendaddba,     /* SIOCWFIRSTPRIV+24 */
    (iw_handler) ieee80211_ioctl_getaddbastatus,/* SIOCWFIRSTPRIV+25 */
    (iw_handler) ieee80211_ioctl_senddelba,     /* SIOCWFIRSTPRIV+26 */
#if ATH_SUPPORT_IQUE
    (iw_handler) ieee80211_ioctl_setmedenyentry, /* SIOCWFIRSTPRIV+27 */
#else
    (iw_handler) NULL,                          /* SIOCWFIRSTPRIV+27 */
#endif
    (iw_handler) ieee80211_ioctl_setaddbaresponse,  /* SIOCWFIRSTPRIV+28 */
    (iw_handler) ieee80211_ioctl_getaclmac,  /* SIOCWFIRSTPRIV+29 */
#if ATH_SUPPORT_IQUE
    (iw_handler) ieee80211_ioctl_sethbrparams,  /* SIOCWFIRSTPRIV+30 */
#else
    (iw_handler) NULL,
#endif
#ifdef notyet
    (iw_handler) ieee80211_ioctl_setrxtimeout,  /* SIOCWFIRSTPRIV+31 */
#else
    (iw_handler) NULL,
#endif /* notyet */

};

#ifdef ATH_SUPPORT_HTC
#define N(a)    (sizeof (a) / sizeof (a[0]))
static iw_handler ieee80211_wrapper_handlers[N(ieee80211_handlers)];
static iw_handler ieee80211_priv_wrapper_handlers[N(ieee80211_priv_handlers)];

static struct iw_handler_def ieee80211_iw_handler_def = {
    .standard       = (iw_handler *) ieee80211_wrapper_handlers,
    .num_standard       = N(ieee80211_wrapper_handlers),
    .private        = (iw_handler *) ieee80211_priv_wrapper_handlers,
    .num_private        = N(ieee80211_priv_wrapper_handlers),
    .private_args       = (struct iw_priv_args *) ieee80211_priv_args,
    .num_private_args   = N(ieee80211_priv_args),
#if WIRELESS_EXT > 18
    .get_wireless_stats =   ieee80211_iw_getstats
#endif
};
#undef N
#else
static struct iw_handler_def ieee80211_iw_handler_def = {
#define N(a)    (sizeof (a) / sizeof (a[0]))
    .standard       = (iw_handler *) ieee80211_handlers,
    .num_standard       = N(ieee80211_handlers),
    .private        = (iw_handler *) ieee80211_priv_handlers,
    .num_private        = N(ieee80211_priv_handlers),
    .private_args       = (struct iw_priv_args *) ieee80211_priv_args,
    .num_private_args   = N(ieee80211_priv_args),
#if WIRELESS_EXT > 18
    .get_wireless_stats =   ieee80211_iw_getstats
#endif
#undef N
};
#endif

struct ioctl_name_tbl
{
    unsigned int ioctl;
    char *name;
};

static struct ioctl_name_tbl ioctl_names[] =
{
    {SIOCG80211STATS, "SIOCG80211STATS"},
    {SIOC80211IFDESTROY,"SIOC80211IFDESTROY"},
    {IEEE80211_IOCTL_GETKEY, "IEEE80211_IOCTL_GETKEY"},
    {IEEE80211_IOCTL_GETWPAIE,"IEEE80211_IOCTL_GETWPAIE"},
    {IEEE80211_IOCTL_GETWSCIE,"IEEE80211_IOCTL_GETWSCIE"},
    {IEEE80211_IOCTL_STA_STATS, "IEEE80211_IOCTL_STA_STATS"},
    {IEEE80211_IOCTL_SCAN_RESULTS,"IEEE80211_IOCTL_SCAN_RESULTS"},
    {IEEE80211_IOCTL_STA_INFO, "IEEE80211_IOCTL_STA_INFO"},
    {IEEE80211_IOCTL_GETMAC, "IEEE80211_IOCTL_GETMAC"},
    {IEEE80211_IOCTL_P2P_BIG_PARAM, "IEEE80211_IOCTL_P2P_BIG_PARAM"},
    {IEEE80211_IOCTL_GET_SCAN_SPACE, "IEEE80211_IOCTL_GET_SCAN_SPACE"},
    {0, NULL}
};

static char *find_std_ioctl_name(int param)
{
    int i = 0;
    for (i = 0; ioctl_names[i].ioctl; i++) {
        if (ioctl_names[i].ioctl == param)
            return(ioctl_names[i].name ? ioctl_names[i].name : "UNNAMED");

    }
    return("Unknown IOCTL");
}

#ifdef ATH_SUPPORT_HTC
/*
 * Wrapper functions for all standard and private IOCTL functions
*/
static int
ieee80211_ioctl_wrapper(struct net_device *dev,
    struct iw_request_info *info,
    union iwreq_data *wrqu, char *extra)
{
#define N(a)    (sizeof (a) / sizeof (a[0]))
    osif_dev *osnetdev = ath_netdev_priv(dev);
    wlan_if_t vap = osnetdev->os_if;
    struct ieee80211com *ic = vap->iv_ic;
    int cmd_id;
    iw_handler handler;

    if ((!ic) ||
        (ic && ic->ic_delete_in_progress)) {
        printk("%s : #### delete is in progress, ic %p \n", __func__, ic);
        return -EINVAL;
    }

    /* private ioctls */
    if (info->cmd >= SIOCIWFIRSTPRIV) {
        cmd_id = (info->cmd - SIOCIWFIRSTPRIV);
        if (cmd_id < 0 || cmd_id >= N(ieee80211_priv_handlers)) {
            printk("%s : #### wrong private ioctl 0x%x\n", __func__, info->cmd);
            return -EINVAL;
        }
        
        handler = ieee80211_priv_handlers[cmd_id];
        if (handler)
            return handler(dev, info, wrqu, extra);

        printk("%s : #### no registered private ioctl function for cmd 0x%x\n", __func__, info->cmd);
    } 
    else { /* standard ioctls */
        cmd_id = (info->cmd - SIOCSIWCOMMIT);
        if (cmd_id < 0 || cmd_id >= N(ieee80211_handlers)) {
            printk("%s : #### wrong standard ioctl 0x%x\n", __func__, info->cmd);
            return -EINVAL;
        }
        
        handler = ieee80211_handlers[cmd_id];
        if (handler)
            return handler(dev, info, wrqu, extra);

        printk("%s : #### no registered standard ioctl function for cmd 0x%x\n", __func__, info->cmd);
    }

    return -EINVAL;
#undef N
}
#endif

/*
* Handle private ioctl requests.
*/
int
ieee80211_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
    wlan_if_t vap = NETDEV_TO_VAP(dev);
#ifdef ATH_SUPPORT_HTC
    struct ieee80211com *ic;
#endif
    if (!vap) {
       return -EINVAL;
    }
#ifdef ATH_SUPPORT_HTC
    ic = vap->iv_ic;
    if ((!ic) ||
        (ic && ic->ic_delete_in_progress)) {
        printk("%s : #### delete is in progress, ic %p \n", __func__, ic);
        return -EINVAL;
    }
#endif

    debug_print_ioctl(dev->name, cmd, find_std_ioctl_name(cmd));
    switch (cmd)
    {

    case SIOCG80211STATS:
        return _copy_to_user(ifr->ifr_data, &vap->iv_stats,
            sizeof (vap->iv_stats) +
            sizeof(vap->iv_unicast_stats) +
            sizeof(vap->iv_multicast_stats)) ? -EFAULT : 0;

    case SIOC80211IFDESTROY:
        if (!capable(CAP_NET_ADMIN))
            return -EPERM;
        return osif_ioctl_delete_vap(dev);
    case IEEE80211_IOCTL_GETKEY:
        return ieee80211_ioctl_getkey(dev, (struct iwreq *) ifr);
    case IEEE80211_IOCTL_GETWPAIE:
        return ieee80211_ioctl_getwpaie(dev, (struct iwreq *) ifr);
#ifdef notyet
    case IEEE80211_IOCTL_GETWSCIE:
        return ieee80211_ioctl_getwscie(dev, (struct iwreq *) ifr);
    case IEEE80211_IOCTL_STA_STATS:
        return ieee80211_ioctl_getstastats(dev, (struct iwreq *) ifr);
#endif
    case IEEE80211_IOCTL_SCAN_RESULTS:
        return ieee80211_ioctl_getscanresults(dev, (struct iwreq *)ifr);
#if UMAC_SUPPORT_TDLS
/* temporary to be removed later, until _getstastats() is available */
    case IEEE80211_IOCTL_STA_STATS:
        return ieee80211_ioctl_getstastats(dev, (struct iwreq *) ifr);
#endif /* UMAC_SUPPORT_TDLS */
    case IEEE80211_IOCTL_STA_INFO:
        return ieee80211_ioctl_getstainfo(dev, (struct iwreq *) ifr);
    case IEEE80211_IOCTL_GETMAC:
        return ieee80211_ioctl_getmac(dev, (struct iwreq *) ifr);
    case IEEE80211_IOCTL_CONFIG_GENERIC:
        return ieee80211_ioctl_config_generic(dev, (struct iwreq *) ifr);
    case IEEE80211_IOCTL_P2P_BIG_PARAM:
        return ieee80211_ioctl_p2p_big_param(dev, (struct iwreq *) ifr);
#ifdef ATH_SUPPORT_LINUX_VENDOR
    case SIOCDEVVENDOR:
        return osif_ioctl_vendor(dev, ifr, 1);
#endif
    case IEEE80211_IOCTL_GET_SCAN_SPACE:
        return ieee80211_ioctl_get_scan_space(dev, (struct iwreq *)ifr);
    }
    return -EOPNOTSUPP;
}

void ieee80211_ioctl_vattach(struct net_device *dev)
{
#ifdef ATH_SUPPORT_HTC
#define N(a)    (sizeof (a) / sizeof (a[0]))    
    int index;

    /* initialize handler array */
    for(index = 0; index < N(ieee80211_wrapper_handlers); index++) {
        if(ieee80211_handlers[index])
            ieee80211_wrapper_handlers[index] = ieee80211_ioctl_wrapper;
        else
            ieee80211_wrapper_handlers[index] = NULL;		 	
    }
    
    for(index = 0; index < N(ieee80211_priv_wrapper_handlers); index++) {
        if(ieee80211_priv_handlers[index])
            ieee80211_priv_wrapper_handlers[index] = ieee80211_ioctl_wrapper;
        else
            ieee80211_priv_wrapper_handlers[index] = NULL; 	
    }
#endif    
#if WIRELESS_EXT <= 18
    dev->get_wireless_stats = ieee80211_iw_getstats;
#endif
    dev->wireless_handlers = &ieee80211_iw_handler_def;
#ifdef ATH_SUPPORT_HTC
#undef N
#endif
}

void
ieee80211_ioctl_vdetach(struct net_device *dev)
{
}


/*
*
*  Display information to the console
*/

static void debug_print_ioctl(char *dev_name, int ioctl, char *ioctl_name)
{
    IOCTL_DPRINTF("***dev=%s  ioctl=0x%04x  name=%s\n", dev_name, ioctl, ioctl_name);
}

/*
*
* Search the ieee80211_priv_arg table for IEEE_IOCTL_SETPARAM defines to
* display the param name.
*
* Input:
*  int param - the ioctl number to look for
*      int set_flag - 1 if ioctl is set, 0 if a get
*
* Returns a string to display.
*/
static char *find_ieee_priv_ioctl_name(int param, int set_flag)
{

    struct iw_priv_args *pa = (struct iw_priv_args *) ieee80211_priv_args;
    int num_args  = ieee80211_iw_handler_def.num_private_args;
    int i;
    int found = 0;

    /* sub-ioctls will be after the IEEEE_IOCTL_SETPARAM - skip duplicate defines */
    for (i = 0; i < num_args; i++, pa++) {
        if (pa->cmd == IEEE80211_IOCTL_SETPARAM) {
            found = 1;
            break;
        }
    }

    /* found IEEEE_IOCTL_SETPARAM  */
    if (found) {
        /* now look for the sub-ioctl number */

        for (; i < num_args; i++, pa++) {
            if (pa->cmd == param) {
                if (set_flag) {
                    if (pa->set_args)
                        return(pa->name ? pa->name : "UNNAMED");
                } else if (pa->get_args)
                    return(pa->name ? pa->name : "UNNAMED");
            }
        }
    }
    return("Unknown IOCTL");
}

