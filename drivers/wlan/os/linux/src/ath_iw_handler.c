/*! \file
**  \brief 
**
** Copyright (c) 2004-2010, Atheros Communications Inc.
**
** Permission to use, copy, modify, and/or distribute this software for any
** purpose with or without fee is hereby granted, provided that the above
** copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
** WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
** ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
** WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
** ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
** OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
**
** This module is the Atheros specific ioctl/iwconfig/iwpriv interface
** to the ATH object, normally instantiated as wifiX, where X is the
** instance number (e.g. wifi0, wifi1).
**
** This provides a mechanism to configure the ATH object within the
** Linux OS enviornment.  This file is OS specific.
**
*/


/*
 * Wireless extensions support for 802.11 common code.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)
#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#endif
#include <linux/version.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/utsname.h>
#include <linux/if_arp.h>       /* XXX for ARPHRD_ETHER */
#include <net/iw_handler.h>

#include "if_athvar.h"
#include "ah.h"
#include "asf_amem.h"

#ifdef ATH_SUPPORT_HTC
static int ath_iw_wrapper_handler(struct net_device *dev,
                                  struct iw_request_info *info,
                                  union iwreq_data *wrqu, char *extra);
#endif
static char *find_ath_ioctl_name(int param, int set_flag);
extern unsigned long ath_ioctl_debug;      /* defined in ah_osdep.c  */

/*
** Defines
*/

enum {
    SPECIAL_PARAM_COUNTRY_ID,
    SPECIAL_PARAM_ASF_AMEM_PRINT,
    SPECIAL_PARAM_DISP_TPC,
};

#define TABLE_SIZE(a)    (sizeof (a) / sizeof (a[0]))

static const struct iw_priv_args ath_iw_priv_args[] = {
    /*
    ** HAL interface routines and parameters
    */

    { ATH_HAL_IOCTL_SETPARAM,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
       0, "setHALparam" },
    { ATH_HAL_IOCTL_GETPARAM,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "getHALparam" },

    /* sub-ioctl handlers */
    { ATH_HAL_IOCTL_SETPARAM,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "" },
    { ATH_HAL_IOCTL_GETPARAM,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "" },

    { HAL_CONFIG_DMA_BEACON_RESPONSE_TIME,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "DMABcnRespT" },
    { HAL_CONFIG_DMA_BEACON_RESPONSE_TIME,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetDMABcnRespT" },

    { HAL_CONFIG_SW_BEACON_RESPONSE_TIME,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "SWBcnRespT" },
    { HAL_CONFIG_SW_BEACON_RESPONSE_TIME,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetSWBcnRespT" },

    { HAL_CONFIG_ADDITIONAL_SWBA_BACKOFF,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "AddSWBbo" },
    { HAL_CONFIG_ADDITIONAL_SWBA_BACKOFF,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetAddSWBAbo" },

    { HAL_CONFIG_6MB_ACK,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "6MBAck" },
    { HAL_CONFIG_6MB_ACK,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "Get5MBAck" },

    { HAL_CONFIG_CWMIGNOREEXTCCA,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "CWMIgnExCCA" },
    { HAL_CONFIG_CWMIGNOREEXTCCA,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetCWMIgnExCCA" },

    { HAL_CONFIG_FORCEBIAS,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ForceBias" },
    { HAL_CONFIG_FORCEBIAS,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetForceBias" },

    { HAL_CONFIG_FORCEBIASAUTO,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ForBiasAuto" },
    { HAL_CONFIG_FORCEBIASAUTO,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetForBiasAuto" },

    { HAL_CONFIG_PCIEPOWERSAVEENABLE,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "PCIEPwrSvEn" },
    { HAL_CONFIG_PCIEPOWERSAVEENABLE,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetPCIEPwrSvEn" },

    { HAL_CONFIG_PCIEL1SKPENABLE,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "PCIEL1SKPEn" },
    { HAL_CONFIG_PCIEL1SKPENABLE,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetPCIEL1SKPEn" },

    { HAL_CONFIG_PCIECLOCKREQ,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "PCIEClkReq" },
    { HAL_CONFIG_PCIECLOCKREQ,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetPCIEClkReq" },

    { HAL_CONFIG_PCIEWAEN,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "PCIEWAEN" },
    { HAL_CONFIG_PCIEWAEN,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetPCIEWAEN" },

    { HAL_CONFIG_PCIEDETACH,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "PCIEDETACH" },
    { HAL_CONFIG_PCIEDETACH,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetPCIEDETACH" },

    { HAL_CONFIG_PCIEPOWERRESET,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "PCIEPwRset" },
    { HAL_CONFIG_PCIEPOWERRESET,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetPCIEPwRset" },

    { HAL_CONFIG_PCIERESTORE,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "PCIERestore" },
    { HAL_CONFIG_PCIERESTORE,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetPCIERestore" },

    { HAL_CONFIG_HTENABLE,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "HTEna" },
    { HAL_CONFIG_HTENABLE,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetHTEna" },

    { HAL_CONFIG_DISABLETURBOG,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "DisTurboG" },
    { HAL_CONFIG_DISABLETURBOG,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetDisTurboG" },

    { HAL_CONFIG_OFDMTRIGLOW,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "OFDMTrgLow" },
    { HAL_CONFIG_OFDMTRIGLOW,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetOFDMTrgLow" },

    { HAL_CONFIG_OFDMTRIGHIGH,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "OFDMTrgHi" },
    { HAL_CONFIG_OFDMTRIGHIGH,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetOFDMTrgHi" },

    { HAL_CONFIG_CCKTRIGHIGH,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "CCKTrgHi" },
    { HAL_CONFIG_CCKTRIGHIGH,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetCCKTrgHi" },

    { HAL_CONFIG_CCKTRIGLOW,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "CCKTrgLow" },
    { HAL_CONFIG_CCKTRIGLOW,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetCCKTrgLow" },

    { HAL_CONFIG_ENABLEANI,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ANIEna" },
    { HAL_CONFIG_ENABLEANI,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetANIEna" },

    { HAL_CONFIG_NOISEIMMUNITYLVL,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "NoiseImmLvl" },
    { HAL_CONFIG_NOISEIMMUNITYLVL,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetNoiseImmLvl" },

    { HAL_CONFIG_OFDMWEAKSIGDET,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "OFDMWeakDet" },
    { HAL_CONFIG_OFDMWEAKSIGDET,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetOFDMWeakDet" },

    { HAL_CONFIG_CCKWEAKSIGTHR,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "CCKWeakThr" },
    { HAL_CONFIG_CCKWEAKSIGTHR,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetCCKWeakThr" },

    { HAL_CONFIG_SPURIMMUNITYLVL,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "SpurImmLvl" },
    { HAL_CONFIG_SPURIMMUNITYLVL,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetSpurImmLvl" },

    { HAL_CONFIG_FIRSTEPLVL,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "FIRStepLvl" },
    { HAL_CONFIG_FIRSTEPLVL,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetFIRStepLvl" },

    { HAL_CONFIG_RSSITHRHIGH,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "RSSIThrHi" },
    { HAL_CONFIG_RSSITHRHIGH,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetRSSIThrHi" },

    { HAL_CONFIG_RSSITHRLOW,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "RSSIThrLow" },
    { HAL_CONFIG_RSSITHRLOW,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetRSSIThrLow" },

    { HAL_CONFIG_DIVERSITYCONTROL,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "DivtyCtl" },
    { HAL_CONFIG_DIVERSITYCONTROL,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetDivtyCtl" },

    { HAL_CONFIG_ANTENNASWITCHSWAP,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "AntSwap" },
    { HAL_CONFIG_ANTENNASWITCHSWAP,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetAntSwap" },

    { HAL_CONFIG_DISABLEPERIODICPACAL,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "DisPACal" },
    { HAL_CONFIG_DISABLEPERIODICPACAL,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetDisPACal" },

    { HAL_CONFIG_DEBUG,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "HALDbg" },
    { HAL_CONFIG_DEBUG,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetHALDbg" },
    
    { HAL_CONFIG_REGREAD_BASE,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "RegRead_base" },
    { HAL_CONFIG_REGREAD_BASE,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetRegReads" },

#if ATH_SUPPORT_IBSS_WPA2
    { HAL_CONFIG_KEYCACHE_PRINT,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "keycache" },
    { HAL_CONFIG_KEYCACHE_PRINT,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_keycache" },
#endif

#ifdef ATH_SUPPORT_TxBF
    { HAL_CONFIG_TxBF_CTL,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "TxBFCTL" },
    { HAL_CONFIG_TxBF_CTL,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "GetTxBFCTL" },
#endif

    { HAL_CONFIG_SHOW_BB_PANIC, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "printBBDebug" },
    { HAL_CONFIG_SHOW_BB_PANIC, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_printBBDebug" },

    /*
    ** ATH interface routines and parameters
    ** This is for integer parameters
    */

    { ATH_PARAM_TXCHAINMASK | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "txchainmask" },
    { ATH_PARAM_TXCHAINMASK | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_txchainmask" },

    { ATH_PARAM_RXCHAINMASK | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "rxchainmask" },
    { ATH_PARAM_RXCHAINMASK | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_rxchainmask" },

    { ATH_PARAM_TXCHAINMASKLEGACY | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "txchmaskleg" },
    { ATH_PARAM_TXCHAINMASKLEGACY | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_txchmaskleg" },

    { ATH_PARAM_RXCHAINMASKLEGACY | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "rxchmaskleg" },
    { ATH_PARAM_RXCHAINMASKLEGACY | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_rxchmaskleg" },

    { ATH_PARAM_CHAINMASK_SEL | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "chainmasksel" },
    { ATH_PARAM_CHAINMASK_SEL | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_chainmasksel" },

    { ATH_PARAM_AMPDU | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "AMPDU" },
    { ATH_PARAM_AMPDU | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "getAMPDU" },

    { ATH_PARAM_AMPDU_LIMIT | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "AMPDULim" },
    { ATH_PARAM_AMPDU_LIMIT | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getAMPDULim" },

    { ATH_PARAM_AMPDU_SUBFRAMES | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "AMPDUFrames" },
    { ATH_PARAM_AMPDU_SUBFRAMES | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getAMPDUFrames" },
    { ATH_PARAM_LDPC | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "LDPC" },
    { ATH_PARAM_LDPC | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "getLDPC" },
#if ATH_SUPPORT_AGGR_BURST
    { ATH_PARAM_AGGR_BURST | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "burst" },
    { ATH_PARAM_AGGR_BURST | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "get_burst" },
    { ATH_PARAM_AGGR_BURST_DURATION | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "burst_dur" },
    { ATH_PARAM_AGGR_BURST_DURATION | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "get_burst_dur" },
#endif
#ifdef ATH_RB
    { ATH_PARAM_RX_RB | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "rb" },
    { ATH_PARAM_RX_RB | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_rb" },

    { ATH_PARAM_RX_RB_DETECT | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "rbdetect" },
    { ATH_PARAM_RX_RB_DETECT | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_rbdetect" },

    { ATH_PARAM_RX_RB_TIMEOUT | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "rbto" },
    { ATH_PARAM_RX_RB_TIMEOUT | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_rbto" },

    { ATH_PARAM_RX_RB_SKIPTHRESH | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,      "rbskipthresh" },
    { ATH_PARAM_RX_RB_SKIPTHRESH | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,      "get_rbskipthresh" },
#endif
    { ATH_PARAM_AGGR_PROT | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "AggrProt" },
    { ATH_PARAM_AGGR_PROT | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getAggrProt" },

    { ATH_PARAM_AGGR_PROT_DUR | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "AggrProtDur" },
    { ATH_PARAM_AGGR_PROT_DUR | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getAggrProtDur" },

    { ATH_PARAM_AGGR_PROT_MAX | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "AggrProtMax" },
    { ATH_PARAM_AGGR_PROT_MAX | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getAddrProtMax" },

    { ATH_PARAM_TXPOWER_LIMIT2G | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "TXPowLim2G" },
    { ATH_PARAM_TXPOWER_LIMIT2G | ATH_PARAM_SHIFT
        ,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,      "getTxPowLim2G" },

    { ATH_PARAM_TXPOWER_LIMIT5G | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "TXPowLim5G" },
    { ATH_PARAM_TXPOWER_LIMIT5G | ATH_PARAM_SHIFT
        ,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,      "getTxPowLim5G" },

    { ATH_PARAM_TXPOWER_OVERRIDE | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "TXPwrOvr" },
    { ATH_PARAM_TXPOWER_OVERRIDE | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getTXPwrOvr" },

    { ATH_PARAM_PCIE_DISABLE_ASPM_WK | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "DisASPMWk" },
    { ATH_PARAM_PCIE_DISABLE_ASPM_WK | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getDisASPMWk" },

    { ATH_PARAM_PCID_ASPM | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "EnaASPM" },
    { ATH_PARAM_PCID_ASPM | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getEnaASPM" },

    { ATH_PARAM_BEACON_NORESET | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "BcnNoReset" },
    { ATH_PARAM_BEACON_NORESET | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getBcnNoReset" },
    { ATH_PARAM_CAB_CONFIG | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "CABlevel" },
    { ATH_PARAM_CAB_CONFIG | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getCABlevel" },
    { ATH_PARAM_ATH_DEBUG | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "ATHDebug" },
    { ATH_PARAM_ATH_DEBUG | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getATHDebug" },

    { ATH_PARAM_ATH_TPSCALE | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "tpscale" },
    { ATH_PARAM_ATH_TPSCALE | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_tpscale" },

    { ATH_PARAM_ACKTIMEOUT | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "acktimeout" },
    { ATH_PARAM_ACKTIMEOUT | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "get_acktimeout" },

    { ATH_PARAM_RIMT_FIRST | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "rximt_first" },
    { ATH_PARAM_RIMT_FIRST | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "get_rximt_first" },

    { ATH_PARAM_RIMT_LAST | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "rximt_last" },
    { ATH_PARAM_RIMT_LAST | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "get_rximt_last" },
    
    { ATH_PARAM_TIMT_FIRST | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "tximt_first" },
    { ATH_PARAM_TIMT_FIRST | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "get_tximt_first" },

    { ATH_PARAM_TIMT_LAST | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "tximt_last" },
    { ATH_PARAM_TIMT_LAST | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "get_tximt_last" },

    { ATH_PARAM_AMSDU_ENABLE | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "AMSDU" },
    { ATH_PARAM_AMSDU_ENABLE | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "getAMSDU" },
	
#if ATH_SUPPORT_IQUE
    { ATH_PARAM_RETRY_DURATION | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "retrydur" },
    { ATH_PARAM_RETRY_DURATION | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_retrydur" },
    { ATH_PARAM_HBR_HIGHPER | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "hbrPER_high" },
    { ATH_PARAM_HBR_HIGHPER | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_hbrPER_high" },
    { ATH_PARAM_HBR_LOWPER | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "hbrPER_low" },
    { ATH_PARAM_HBR_LOWPER | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_hbrPER_low" },
#endif


#if  ATH_SUPPORT_AOW
    { ATH_PARAM_SW_RETRY_LIMIT | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "sw_retries" },
    { ATH_PARAM_SW_RETRY_LIMIT | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_sw_retries" },
    { ATH_PARAM_AOW_LATENCY | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "aow_latency" },
    { ATH_PARAM_AOW_LATENCY | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_aow_latency" },
    { ATH_PARAM_AOW_STATS | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "aow_clearstats" },
    { ATH_PARAM_AOW_STATS | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_aow_stats" },
    { ATH_PARAM_AOW_LIST_AUDIO_CHANNELS | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_aow_channels" },
    { ATH_PARAM_AOW_PLAY_LOCAL | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "aow_playlocal" },
    { ATH_PARAM_AOW_PLAY_LOCAL | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "g_aow_playlocal" },
    { ATH_PARAM_AOW_CLEAR_AUDIO_CHANNELS | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "aow_clear_ch" },
    { ATH_PARAM_AOW_ER | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "aow_er" },
    { ATH_PARAM_AOW_ER | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_aow_er" },
    { ATH_PARAM_AOW_EC | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "aow_ec" },
    { ATH_PARAM_AOW_EC | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_aow_ec" },
    { ATH_PARAM_AOW_EC_FMAP | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "aow_ec_fmap" },
    { ATH_PARAM_AOW_EC_FMAP | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_aow_ec_fmap" },
#endif  /* ATH_SUPPORT_AOW */


    { ATH_PARAM_TX_STBC | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "txstbc" },
    { ATH_PARAM_TX_STBC | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_txstbc" },

    { ATH_PARAM_RX_STBC | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "rxstbc" },
    { ATH_PARAM_RX_STBC | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_rxstbc" },
    { ATH_PARAM_TOGGLE_IMMUNITY | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "immunity" },
    { ATH_PARAM_TOGGLE_IMMUNITY | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_immunity" },

    { ATH_PARAM_LIMIT_LEGACY_FRM | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "limit_legacy" },
    { ATH_PARAM_LIMIT_LEGACY_FRM | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_limit_legacy" },

    { ATH_PARAM_GPIO_LED_CUSTOM | ATH_PARAM_SHIFT,
	 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,      "get_ledcustom" },
    { ATH_PARAM_GPIO_LED_CUSTOM | ATH_PARAM_SHIFT,
	 IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,      "set_ledcustom" },

    { ATH_PARAM_SWAP_DEFAULT_LED | ATH_PARAM_SHIFT,
	 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,      "get_swapled" },
    { ATH_PARAM_SWAP_DEFAULT_LED | ATH_PARAM_SHIFT,
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "set_swapled" },

#if ATH_SUPPORT_VOWEXT
    { ATH_PARAM_VOWEXT | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getVowExt"},
    { ATH_PARAM_VOWEXT | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setVowExt"},
    { ATH_PARAM_RCA | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rcaparam"},
    { ATH_PARAM_RCA | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_rcaparam"},
    { ATH_PARAM_VSP_ENABLE | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_vsp_enable"},
    { ATH_PARAM_VSP_ENABLE | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_vsp_enable"},
	{ ATH_PARAM_VSP_THRESHOLD | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_vsp_thresh"},
    { ATH_PARAM_VSP_THRESHOLD | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_vsp_thresh"},
	{ ATH_PARAM_VSP_EVALINTERVAL | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_vsp_evaldur"},
    { ATH_PARAM_VSP_EVALINTERVAL | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_vsp_evaldur"},
#endif
#if ATH_VOW_EXT_STATS
    { ATH_PARAM_VOWEXT_STATS | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getVowExtStats"},
    { ATH_PARAM_VOWEXT_STATS | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setVowExtStats"},
#endif
#ifdef VOW_TIDSCHED
    { ATH_PARAM_TIDSCHED | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_tidsched"},
    { ATH_PARAM_TIDSCHED | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_tidsched"},
    { ATH_PARAM_TIDSCHED_VIQW | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_tids_viqw"},
    { ATH_PARAM_TIDSCHED_VIQW | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_tids_viqw"},
    { ATH_PARAM_TIDSCHED_VOQW | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_tids_voqw"},
    { ATH_PARAM_TIDSCHED_VOQW | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_tids_voqw"},
    { ATH_PARAM_TIDSCHED_BEQW | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_tids_beqw"},
    { ATH_PARAM_TIDSCHED_BEQW | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_tids_beqw"},
    { ATH_PARAM_TIDSCHED_BKQW | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_tids_bkqw"},
    { ATH_PARAM_TIDSCHED_BKQW | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_tids_bkqw"},
    { ATH_PARAM_TIDSCHED_VITO | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_tids_vito"},
    { ATH_PARAM_TIDSCHED_VITO | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_tids_vito"},
    { ATH_PARAM_TIDSCHED_VOTO | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_tids_voto"},
    { ATH_PARAM_TIDSCHED_VOTO | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_tids_voto"},
    { ATH_PARAM_TIDSCHED_BETO | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_tids_beto"},
    { ATH_PARAM_TIDSCHED_BETO | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_tids_beto"},
    { ATH_PARAM_TIDSCHED_BKTO | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_tids_bkto"},
    { ATH_PARAM_TIDSCHED_BKTO | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_tids_bkto"},
#endif
#ifdef VOW_LOGLATENCY
    { ATH_PARAM_LOGLATENCY | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_loglatency"},
    { ATH_PARAM_LOGLATENCY | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_loglatency"},
#endif
#if ATH_SUPPORT_VOW_DCS
    { ATH_PARAM_DCS | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_dcs_enable"},
    { ATH_PARAM_DCS | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_dcs_enable"},
    { ATH_PARAM_DCS_COCH | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_dcs_cochintr"},
    { ATH_PARAM_DCS_COCH | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_dcs_cochintr"},
    { ATH_PARAM_DCS_TXERR | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_dcs_txerr"},
    { ATH_PARAM_DCS_TXERR | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_dcs_txerr"},

#endif
#ifdef ATH_SUPPORT_TxBF
    { ATH_PARAM_TXBF_SW_TIMER| ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_cvtimeout" },
    { ATH_PARAM_TXBF_SW_TIMER| ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_cvtimeout" },
#endif
    { ATH_PARAM_BCN_BURST | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_bcnburst" },
    { ATH_PARAM_BCN_BURST | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bcnburst" },

/*
** "special" parameters
** processed in this funcion
*/

    { SPECIAL_PARAM_COUNTRY_ID | SPECIAL_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "setCountryID" },
    { SPECIAL_PARAM_COUNTRY_ID | SPECIAL_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getCountryID" },

    /*
    ** The Get MAC address/Set MAC address interface
    */

    { ATH_IOCTL_SETHWADDR,
        IW_PRIV_TYPE_CHAR | ((IEEE80211_ADDR_LEN * 3) - 1), 0,  "setHwaddr" },
    { ATH_IOCTL_GETHWADDR,
        0, IW_PRIV_TYPE_CHAR | ((IEEE80211_ADDR_LEN * 3) - 1),  "getHwaddr" },

    /*
    ** The Get Country/Set Country interface
    */

    { ATH_IOCTL_SETCOUNTRY, IW_PRIV_TYPE_CHAR | 3, 0,       "setCountry" },
    { ATH_IOCTL_GETCOUNTRY, 0, IW_PRIV_TYPE_CHAR | 3,       "getCountry" },


    { SPECIAL_PARAM_ASF_AMEM_PRINT | SPECIAL_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "amemPrint" },

    { ATH_PARAM_PHYRESTART_WAR | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getPhyRestartWar"},
    { ATH_PARAM_PHYRESTART_WAR | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setPhyRestartWar"},
#if ATH_SUPPORT_VOWEXT
    { ATH_PARAM_KEYSEARCH_ALWAYS_WAR | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getKeySrchAlways"},
    { ATH_PARAM_KEYSEARCH_ALWAYS_WAR | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setKeySrchAlways"},
#endif
      
#if ATH_SUPPORT_DYN_TX_CHAINMASK      
    { ATH_PARAM_DYN_TX_CHAINMASK | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dyntxchain" },
    { ATH_PARAM_DYN_TX_CHAINMASK| ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_dyntxchain" },
#endif /* ATH_SUPPORT_DYN_TX_CHAINMASK */

#if UMAC_SUPPORT_INTERNALANTENNA
    { ATH_PARAM_SMARTANTENNA | ATH_PARAM_SHIFT,
          0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getSmartAntenna"},
    { ATH_PARAM_SMARTANTENNA | ATH_PARAM_SHIFT,
          IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setSmartAntenna"},
#endif

#if ATH_SUPPORT_FLOWMAC_MODULE
    { ATH_PARAM_FLOWMAC | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "flowmac" },
    { ATH_PARAM_FLOWMAC| ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_flowmac" },
#endif
#if ATH_ANI_NOISE_SPUR_OPT
    { ATH_PARAM_NOISE_SPUR_OPT | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "noisespuropt"},
#endif /* ATH_ANI_NOISE_SPUR_OPT */
    { SPECIAL_PARAM_DISP_TPC | SPECIAL_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "disp_tpc" },
    { ATH_PARAM_RTS_CTS_RATE | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setctsrate" },
    { ATH_PARAM_RTS_CTS_RATE| ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_ctsrate" },
#if ATH_SUPPORT_RX_PROC_QUOTA
    { ATH_PARAM_CNTRX_NUM | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "cntrx" },
    { ATH_PARAM_CNTRX_NUM | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_cntrx"},
#endif      
};

static short int staOnlyCountryCodes[]= {
     36, /*  CRTY_AUSTRALIA */
    124, /*  CTRY_CANADA */
    410, /*  CTRY_KOREA_ROC */
    840, /*  CTRY_UNITED_STATES */
};
/******************************************************************************/
/*!
**  \brief Set ATH/HAL parameter value
**
**  Interface routine called by the iwpriv handler to directly set specific
**  HAL parameters.  Parameter ID is stored in the above iw_priv_args structure
**
**  \param dev Net Device pointer, Linux driver handle
**  \param info Request Info Stucture containing type info, etc.
**  \param w Pointer to a word value, not used here
**  \param extra Pointer to the actual data buffer.
**  \return 0 for success
**  \return Non Zero on error
*/

static int ath_iw_setparam(struct net_device *dev,
                           struct iw_request_info *info,
                           void *w,
                           char *extra)
{

    struct ath_softc_net80211 *scn =  ath_netdev_priv(dev);
    struct ath_softc          *sc  =  ATH_DEV_TO_SC(scn->sc_dev);
    struct ath_hal            *ah =   sc->sc_ah;
    int *i = (int *) extra;
    int param = i[0];       /* parameter id is 1st */
    int value = i[1];       /* NB: most values are TYPE_INT */
    int retval = 0;
    
    /*
    ** Code Begins
    ** Since the parameter passed is the value of the parameter ID, we can call directly
    */
    if (ath_ioctl_debug)
        printk("***%s: dev=%s ioctl=0x%04x  name=%s\n", __func__, dev->name, param, find_ath_ioctl_name(param, 1));


    if ( param & ATH_PARAM_SHIFT )
    {
        /*
        ** It's an ATH value.  Call the  ATH configuration interface
        */
        
        param -= ATH_PARAM_SHIFT;
        retval = scn->sc_ops->ath_set_config_param(scn->sc_dev,
                                                   (ath_param_ID_t)param,
                                                   &value);
    }
    else if ( param & SPECIAL_PARAM_SHIFT )
    {
        if ( param == (SPECIAL_PARAM_SHIFT | SPECIAL_PARAM_COUNTRY_ID) ) {
            int j; 
            for(j=0; j < sizeof(staOnlyCountryCodes)/sizeof(staOnlyCountryCodes[0]); j++) {
                if(staOnlyCountryCodes[j] == value) {
                    printk("***%s: dev=%s ioctl=0x%04x  name=%s The country code %d is a STA only country code\n", __func__, dev->name, param, find_ath_ioctl_name(param, 1),value);
                    retval = -EOPNOTSUPP;
                    return (retval);
                }
            }
            if (sc->sc_ieee_ops->set_countrycode) {
                retval = sc->sc_ieee_ops->set_countrycode(
                    sc->sc_ieee, NULL, value, CLIST_NEW_COUNTRY);
            }
        } else if ( param  == (SPECIAL_PARAM_SHIFT | SPECIAL_PARAM_ASF_AMEM_PRINT) ) {
            asf_amem_status_print();
            if ( value ) {
                asf_amem_allocs_print(asf_amem_alloc_all, value == 1);
            }
        } else if (param  == (SPECIAL_PARAM_SHIFT | SPECIAL_PARAM_DISP_TPC) ) {
            ath_hal_display_tpctables(ah);
        } else {
            retval = -EOPNOTSUPP;
        }
    }
    else
    {
        retval = (int) ath_hal_set_config_param(
            ah, (HAL_CONFIG_OPS_PARAMS_TYPE)param, &value);
    }

    return (retval);    
}

/******************************************************************************/
/*!
**  \brief Returns value of HAL parameter
**
**  This returns the current value of the indicated HAL parameter, used by
**  iwpriv interface.
**
**  \param dev Net Device pointer, Linux driver handle
**  \param info Request Info Stucture containing type info, etc.
**  \param w Pointer to a word value, not used here
**  \param extra Pointer to the actual data buffer.
**  \return 0 for success
**  \return -EOPNOTSUPP for invalue request
*/

static int ath_iw_getparam(struct net_device *dev, struct iw_request_info *info, void *w, char *extra)
{
    struct ath_softc_net80211 *scn  = ath_netdev_priv(dev);
    struct ath_softc          *sc   = ATH_DEV_TO_SC(scn->sc_dev);
    struct ath_hal            *ah   = sc->sc_ah;
    int                     param   = *(int *)extra;
    int                      *val   = (int *)extra; 
    int                    retval   = 0;
    
    /*
    ** Code Begins
    ** Since the parameter passed is the value of the parameter ID, we can call directly
    */
    if (ath_ioctl_debug)
        printk("***%s: dev=%s ioctl=0x%04x  name=%s \n", __func__, dev->name, param, find_ath_ioctl_name(param, 0));

    if ( param & ATH_PARAM_SHIFT )
    {
        /*
        ** It's an ATH value.  Call the  ATH configuration interface
        */
        
        param -= ATH_PARAM_SHIFT;
        if ( scn->sc_ops->ath_get_config_param(scn->sc_dev,(ath_param_ID_t)param,extra) )
        {
            retval = -EOPNOTSUPP;
        }
    }
    else if ( param & SPECIAL_PARAM_SHIFT )
    {
        if ( param == (SPECIAL_PARAM_SHIFT | SPECIAL_PARAM_COUNTRY_ID) ) {
            HAL_COUNTRY_ENTRY         cval;
    
            scn->sc_ops->get_current_country(scn->sc_dev, &cval);
            val[0] = cval.countryCode;
        } else {
            retval = -EOPNOTSUPP;
        }
    }
    else
    {
        if ( !ath_hal_get_config_param(ah, (HAL_CONFIG_OPS_PARAMS_TYPE)param, extra) )
        {
            retval = -EOPNOTSUPP;
        }
    }

    return (retval);
}



/******************************************************************************/
/*!
**  \brief Set country
**
**  Interface routine called by the iwpriv handler to directly set specific
**  HAL parameters.  Parameter ID is stored in the above iw_priv_args structure
**
**  \param dev Net Device pointer, Linux driver handle
**  \param info Request Info Stucture containing type info, etc.
**  \param w Pointer to a word value, not used here
**  \param extra Pointer to the actual data buffer.
**  \return 0 for success
**  \return Non Zero on error
*/

static int ath_iw_setcountry(struct net_device *dev,
                           struct iw_request_info *info,
                           void *w,
                           char *extra)
{

    struct ath_softc_net80211 *scn =  ath_netdev_priv(dev);
    struct ath_softc          *sc  = ATH_DEV_TO_SC(scn->sc_dev);
    char *p = (char *) extra;
    int retval;

    if (sc->sc_ieee_ops->set_countrycode) {
        retval = sc->sc_ieee_ops->set_countrycode(sc->sc_ieee, p, 0, CLIST_NEW_COUNTRY);
    } else {
        retval = -EOPNOTSUPP;
    }

    return retval;
}

/******************************************************************************/
/*!
**  \brief Returns country string
**
**  This returns the current value of the indicated HAL parameter, used by
**  iwpriv interface.
**
**  \param dev Net Device pointer, Linux driver handle
**  \param info Request Info Stucture containing type info, etc.
**  \param w Pointer to a word value, not used here
**  \param extra Pointer to the actual data buffer.
**  \return 0 for success
*/

static int ath_iw_getcountry(struct net_device *dev, struct iw_request_info *info, void *w, char *extra)
{
    struct ath_softc_net80211 *scn  = ath_netdev_priv(dev);
    struct iw_point           *wri  = (struct iw_point *)w;
    HAL_COUNTRY_ENTRY         cval;
    char                      *str  = (char *)extra;
    
    /*
    ** Code Begins
    */

    scn->sc_ops->get_current_country(scn->sc_dev, &cval);
    str[0] = cval.iso[0];
    str[1] = cval.iso[1];
    str[2] = cval.iso[2];
    str[3] = 0;
    wri->length = 3;
    
    return (0);
}


static int char_to_num(char c)
{
    if ( c >= 'A' && c <= 'F' )
        return c - 'A' + 10;
    if ( c >= 'a' && c <= 'f' )
        return c - 'a' + 10;
    if ( c >= '0' && c <= '9' )
        return c - '0';
    return -EINVAL;
}

static char num_to_char(int n)
{
    if ( n >= 10 && n <= 15 )
        return n  - 10 + 'A';
    if ( n >= 0 && n <= 9 )
        return n  + '0';
    return ' '; //Blank space 
}

/******************************************************************************/
/*!
**  \brief Set MAC address
**
**  Interface routine called by the iwpriv handler to directly set MAC
**  address.
**
**  \param dev Net Device pointer, Linux driver handle
**  \param info Request Info Stucture containing type info, etc.
**  \param w Pointer to a word value, not used here
**  \param extra Pointer to the actual data buffer.
**  \return 0 for success
**  \return Non Zero on error
*/

static int ath_iw_sethwaddr(struct net_device *dev, struct iw_request_info *info, void *w, char *extra)
{
    int retval;
    unsigned char addr[IEEE80211_ADDR_LEN];
    struct sockaddr sa;
    int i = 0;
    char *buf = extra;
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    struct ieee80211com *ic = &scn->sc_ic;

    do {
        retval = char_to_num(*buf++);
        if ( retval < 0 )
            break;

        addr[i] = retval << 4;

        retval = char_to_num(*buf++);
        if ( retval < 0 )
            break;

        addr[i++] += retval;

        if ( i < IEEE80211_ADDR_LEN && *buf++ != ':' ) {
            retval = -EINVAL;
            break;
        }
        retval = 0;
    } while ( i < IEEE80211_ADDR_LEN );

    if ( !retval ) {
        if ( !TAILQ_EMPTY(&ic->ic_vaps) ) {
            retval = -EBUSY; //We do not set the MAC address if there are VAPs present
        } else {
            IEEE80211_ADDR_COPY(&sa.sa_data, addr);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
            retval = dev->netdev_ops->ndo_set_mac_address(dev, &sa);
#else
            retval = dev->set_mac_address(dev, &sa);
#endif
        }
    }

    return retval;
}

/******************************************************************************/
/*!
**  \brief Returns MAC address
**
**  This returns the current value of the MAC address, used by
**  iwpriv interface.
**
**  \param dev Net Device pointer, Linux driver handle
**  \param info Request Info Stucture containing type info, etc.
**  \param w Pointer to a word value, not used here
**  \param extra Pointer to the actual data buffer.
**  \return 0 for success
*/

static int ath_iw_gethwaddr(struct net_device *dev, struct iw_request_info *info, void *w, char *extra)
{
    struct iw_point *wri = (struct iw_point *)w;
    char *buf = extra;
    int i, j = 0;

    for ( i = 0; i < IEEE80211_ADDR_LEN; i++ ) {
        buf[j++] = num_to_char(dev->dev_addr[i] >> 4);
        buf[j++] = num_to_char(dev->dev_addr[i] & 0xF);

        if ( i < IEEE80211_ADDR_LEN - 1 )
            buf[j++] = ':';
        else
            buf[j++] = '\0';
    }

    wri->length = (IEEE80211_ADDR_LEN * 3) - 1;

    return 0;
}


/*
** iwpriv Handlers
** This table contains the references to the routines that actually get/set
** the parameters in the args table.
*/

static const iw_handler ath_iw_priv_handlers[] = {
    (iw_handler) ath_iw_setparam,          /* SIOCWFIRSTPRIV+0 */
    (iw_handler) ath_iw_getparam,          /* SIOCWFIRSTPRIV+1 */
    (iw_handler) ath_iw_setcountry,        /* SIOCWFIRSTPRIV+2 */
    (iw_handler) ath_iw_getcountry,        /* SIOCWFIRSTPRIV+3 */
    (iw_handler) ath_iw_sethwaddr,         /* SIOCWFIRSTPRIV+4 */
    (iw_handler) ath_iw_gethwaddr,         /* SIOCWFIRSTPRIV+5 */
};

/*
** Wireless Handler Structure
** This table provides the Wireless tools the interface required to access
** parameters in the HAL.  Each sub table contains the definition of various
** control tables that reference functions this module
*/
#ifdef ATH_SUPPORT_HTC
static iw_handler ath_iw_priv_wrapper_handlers[TABLE_SIZE(ath_iw_priv_handlers)];

static struct iw_handler_def ath_iw_handler_def = {
    .standard           = (iw_handler *) NULL,
    .num_standard       = 0,
    .private            = (iw_handler *) ath_iw_priv_wrapper_handlers,
    .num_private        = TABLE_SIZE(ath_iw_priv_wrapper_handlers),
    .private_args       = (struct iw_priv_args *) ath_iw_priv_args,
    .num_private_args   = TABLE_SIZE(ath_iw_priv_args),
    .get_wireless_stats = NULL,
};
#else
static struct iw_handler_def ath_iw_handler_def = {
    .standard           = (iw_handler *) NULL,
    .num_standard       = 0,
    .private            = (iw_handler *) ath_iw_priv_handlers,
    .num_private        = TABLE_SIZE(ath_iw_priv_handlers),
    .private_args       = (struct iw_priv_args *) ath_iw_priv_args,
    .num_private_args   = TABLE_SIZE(ath_iw_priv_args),
    .get_wireless_stats	= NULL,
};
#endif

void ath_iw_attach(struct net_device *dev)
{
#ifdef ATH_SUPPORT_HTC
    int index;

    /* initialize handler array */
    for(index = 0; index < TABLE_SIZE(ath_iw_priv_wrapper_handlers); index++) {
    	if(ath_iw_priv_handlers[index])
       	    ath_iw_priv_wrapper_handlers[index] = ath_iw_wrapper_handler;
        else
    	    ath_iw_priv_wrapper_handlers[index] = NULL;		  	
    }
#endif

    dev->wireless_handlers = &ath_iw_handler_def;
}
EXPORT_SYMBOL(ath_iw_attach);

/*
 *
 * Search the ath_iw_priv_arg table for ATH_HAL_IOCTL_SETPARAM defines to 
 * display the param name
 */
static char *find_ath_ioctl_name(int param, int set_flag)
{
    struct iw_priv_args *pa = (struct iw_priv_args *) ath_iw_priv_args;
    int num_args  = ath_iw_handler_def.num_private_args;
    int i;

    /* now look for the sub-ioctl number */

    for (i = 0; i < num_args; i++, pa++) {
        if (pa->cmd == param) {
            if (set_flag) {
                if (pa->set_args)
                    return(pa->name ? pa->name : "UNNAMED");
            } else if (pa->get_args) 
                    return(pa->name ? pa->name : "UNNAMED");
        }
    }
    return("Unknown IOCTL");
}

#ifdef ATH_SUPPORT_HTC
/*
 * Wrapper functions for all standard and private IOCTL functions
 */
static int ath_iw_wrapper_handler(struct net_device *dev,
                                  struct iw_request_info *info,
                                  union iwreq_data *wrqu, char *extra)
{
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    int cmd_id;
    iw_handler handler;

    /* reject ioctls */
    if ((!scn) ||
        (scn && scn->sc_htc_delete_in_progress)){
        printk("%s : #### delete is in progress, scn %p \n", __func__, scn);
        return -EINVAL;
    }
    
    /* private ioctls */
    if (info->cmd >= SIOCIWFIRSTPRIV) {
        cmd_id = (info->cmd - SIOCIWFIRSTPRIV);
        if (cmd_id < 0 || cmd_id >= TABLE_SIZE(ath_iw_priv_handlers)) {
            printk("%s : #### wrong private ioctl 0x%x\n", __func__, info->cmd);
            return -EINVAL;
        }
        
        handler = ath_iw_priv_handlers[cmd_id];
        if (handler)
            return handler(dev, info, wrqu, extra);
        
        printk("%s : #### no registered private ioctl function for cmd 0x%x\n", __func__, info->cmd);
    }
   
    printk("%s : #### unknown command 0x%x\n", __func__, info->cmd);
    
    return -EINVAL;
}
#endif
