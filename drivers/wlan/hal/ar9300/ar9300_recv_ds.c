/*
 * Copyright (c) 2008-2010, Atheros Communications Inc.
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

#include "opt_ah.h"

#ifdef AH_SUPPORT_AR9300

#include "ah.h"
#include "ah_desc.h"
#include "ah_internal.h"

#include "ar9300/ar9300.h"
#include "ar9300/ar9300reg.h"
#include "ar9300/ar9300desc.h"

#if ATH_SUPPORT_WIRESHARK
#include "ah_radiotap.h" /* ah_rx_radiotap_header */
#endif /* ATH_SUPPORT_WIRESHARK */


/*
 * Process an RX descriptor, and return the status to the caller.
 * Copy some hardware specific items into the software portion
 * of the descriptor.
 *
 * NB: the caller is responsible for validating the memory contents
 *     of the descriptor (e.g. flushing any cached copy).
 */
HAL_STATUS
ar9300ProcRxDescFast(struct ath_hal *ah, struct ath_desc *ds,
    u_int32_t pa, struct ath_desc *nds, struct ath_rx_status *rxs, void *buf_addr)
{
	struct ar9300_rxs *rxsp = AR9300RXS(buf_addr);

//	ath_hal_printf(ah,"CHH=RX: ds_info 0x%x  status1: 0x%x  status11: 0x%x\n",
//						rxsp->ds_info,rxsp->status1,rxsp->status11);

    if ((rxsp->status11 & AR_RxDone) == 0)
        return HAL_EINPROGRESS;

    if (MS(rxsp->ds_info, AR_DescId) != 0x168c) {
#if __PKT_SERIOUS_ERRORS__
       /*BUG: 63564-HT */
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "%s: Rx Descriptor error 0x%x\n",
                 __func__,rxsp->ds_info);
#endif
        return HAL_EINVAL;
    }

    if ((rxsp->ds_info & (AR_TxRxDesc|AR_CtrlStat)) != 0) {
#if __PKT_SERIOUS_ERRORS__
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "%s: Rx Descriptor wrong info 0x%x\n",
                 __func__, rxsp->ds_info);
#endif
        return HAL_EINPROGRESS;
    }

    rxs->rs_status = 0;
    rxs->rs_flags =  0;

    rxs->rs_datalen = rxsp->status2 & AR_DataLen;
    rxs->rs_tstamp =  rxsp->status3;

    /* XXX what about KeyCacheMiss? */
    rxs->rs_rssi = MS(rxsp->status5, AR_RxRSSICombined);
    rxs->rs_rssi_ctl0 = MS(rxsp->status1, AR_RxRSSIAnt00);
    rxs->rs_rssi_ctl1 = MS(rxsp->status1, AR_RxRSSIAnt01);
    rxs->rs_rssi_ctl2 = MS(rxsp->status1, AR_RxRSSIAnt02);
    rxs->rs_rssi_ext0 = MS(rxsp->status5, AR_RxRSSIAnt10);
    rxs->rs_rssi_ext1 = MS(rxsp->status5, AR_RxRSSIAnt11);
    rxs->rs_rssi_ext2 = MS(rxsp->status5, AR_RxRSSIAnt12);
    if (rxsp->status11 & AR_RxKeyIdxValid)
        rxs->rs_keyix = MS(rxsp->status11, AR_KeyIdx);
    else
        rxs->rs_keyix = HAL_RXKEYIX_INVALID;
    /* NB: caller expected to do rate table mapping */
    rxs->rs_rate = MS(rxsp->status1, AR_RxRate);
    rxs->rs_more = (rxsp->status2 & AR_RxMore) ? 1 : 0;

    rxs->rs_isaggr = (rxsp->status11 & AR_RxAggr) ? 1 : 0;
    rxs->rs_moreaggr = (rxsp->status11 & AR_RxMoreAggr) ? 1 : 0;
    rxs->rs_antenna = (MS(rxsp->status4, AR_RxAntenna) & 0x7);
    rxs->rs_isapsd = (rxsp->status11 & AR_ApsdTrig) ? 1 : 0;
    rxs->rs_flags  = (rxsp->status4 & AR_GI) ? HAL_RX_GI : 0;
    rxs->rs_flags  |= (rxsp->status4 & AR_2040) ? HAL_RX_2040 : 0;

#ifdef ATH_SUPPORT_TxBF
    rxs->rx_hw_upload_data = (rxsp->status2 & AR_HwUploadData) ? 1 : 0;
    rxs->rx_not_sounding = (rxsp->status4 & AR_RxNotSounding) ? 1 : 0;
    rxs->rx_Ness =MS(rxsp->status4,AR_RxNess);
    rxs->rx_hw_upload_data_valid = (rxsp->status4 & AR_HwUploadDataValid) ? 1 : 0;	
    /*Due to HW timing issue, rx_hw_upload_data_type field not work-able
    EV [69462] Chip::Osprey Wrong value at "hw_upload_data_type" of RXS when uploading V/CV report */   
    rxs->rx_hw_upload_data_type = MS(rxsp->status11,AR_HwUploadDataType);
    //if (((rxsp->status11 & AR_RxFrameOK) == 0)&((rxsp->status4 & AR_RxNotSounding) ==0))
    //{
    //   HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"=>%s:sounding, Ness= %d,Rx_2:%x,Rx4:%x, Rx_11:%x,rate %d\n",__func__
    //        ,MS(rxsp->status4,AR_RxNess),rxsp->status2,rxsp->status4,rxsp->status11,rxs->rs_rate); //hcl
    //   HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,"===>%s:data length %d, Rx rate %d\n",__func__,rxs->rs_datalen,rxs->rs_rate);
    // }
#endif
    /* Copy EVM information */
    rxs->evm0 = rxsp->status6;
    rxs->evm1 = rxsp->status7;
    rxs->evm2 = rxsp->status8;
    rxs->evm3 = rxsp->status9;
    rxs->evm4 = (rxsp->status10 & 0xffff);

    if (rxsp->status11 & AR_PreDelimCRCErr) {
        rxs->rs_flags |= HAL_RX_DELIM_CRC_PRE;
    }
    if (rxsp->status11 & AR_PostDelimCRCErr) {
        rxs->rs_flags |= HAL_RX_DELIM_CRC_POST;
    }
    if (rxsp->status11 & AR_DecryptBusyErr) {
        rxs->rs_flags |= HAL_RX_DECRYPT_BUSY;
    }
    if (rxsp->status11 & AR_HiRxChain) {
        rxs->rs_flags |= HAL_RX_HI_RX_CHAIN;
    }
    if (rxsp->status11 & AR_KeyMiss) {
        rxs->rs_status |= HAL_RXERR_KEYMISS;
    }

    if ((rxsp->status11 & AR_RxFrameOK) == 0) {
        /*
         * These four bits should not be set together.  The
         * 9300 spec states a Michael error can only occur if
         * DecryptCRCErr not set (and TKIP is used).  Experience
         * indicates however that you can also get Michael errors
         * when a CRC error is detected, but these are specious.
         * Consequently we filter them out here so we don't
         * confuse and/or complicate drivers.
         */
        if (rxsp->status11 & AR_CRCErr)
            rxs->rs_status |= HAL_RXERR_CRC;
        else if (rxsp->status11 & AR_PHYErr) {
            u_int phyerr;

         /*
          * Packets with OFDM_RESTART on post delimiter are CRC OK and usable and
          * MAC ACKs them. To avoid packet from being lost, we remove the PHY Err flag
          * so that lmac layer does not drop them - EV 70071
          */
            phyerr = MS(rxsp->status11, AR_PHYErrCode);
	        if ((phyerr == HAL_PHYERR_OFDM_RESTART) && 
                    (rxsp->status11 & AR_PostDelimCRCErr)) {
                rxs->rs_phyerr = 0;
            } else {
                rxs->rs_status |= HAL_RXERR_PHY;
                rxs->rs_phyerr = phyerr;
            }
        } else if (rxsp->status11 & AR_DecryptCRCErr)
            rxs->rs_status |= HAL_RXERR_DECRYPT;
        else if (rxsp->status11 & AR_MichaelErr)
            rxs->rs_status |= HAL_RXERR_MIC;
    }

    return HAL_OK;
}

HAL_STATUS
ar9300ProcRxDesc(struct ath_hal *ah, struct ath_desc *ds,
    u_int32_t pa, struct ath_desc *nds, u_int64_t tsf)
{
    return HAL_ENOTSUPP;
}

/*
 * rx path in ISR is different for ar9300 from ar5416, and ath_rx_proc_descfast will not 
 * be called if edmasupport is true. So this function ath_hal_get_rxkeyidx will not be 
 * called for ar9300. This function in ar9300's HAL is just a stub one because we need 
 * to link something to the callback interface of the HAL module.
 */
HAL_STATUS
ar9300GetRxKeyIdx(struct ath_hal *ah, struct ath_desc *ds, u_int8_t *keyix, u_int8_t *status)
{
    *status = 0;
    *keyix = HAL_RXKEYIX_INVALID;    
    return HAL_ENOTSUPP;
}

#if ATH_SUPPORT_WIRESHARK

void ar9300FillRadiotapHdr(struct ath_hal *ah,
                           struct ah_rx_radiotap_header *rh,
                           struct ah_ppi_data *ppi,
                           struct ath_desc *ds,
                           void *buf_addr)
{
    struct ar9300_rxs *rxsp = AR9300RXS(buf_addr);
    int i, j;

    if (rh != NULL) {
        // Fill out Radiotap header
        OS_MEMZERO(rh, sizeof(struct ah_rx_radiotap_header));

        rh->tsf = ar9300GetTsf64(ah);

        rh->wr_rate = MS(rxsp->status1, AR_RxRate);
        rh->wr_chan_freq = (AH_PRIVATE(ah)->ah_curchan) ? AH_PRIVATE(ah)->ah_curchan->channel : 0;
        // Fill out extended channel info.
        rh->wr_xchannel.freq = rh->wr_chan_freq;
        rh->wr_xchannel.flags = AH_PRIVATE(ah)->ah_curchan->channelFlags;
        rh->wr_xchannel.maxpower = AH_PRIVATE(ah)->ah_curchan->maxRegTxPower;
        rh->wr_xchannel.chan = ath_hal_mhz2ieee(ah, rh->wr_chan_freq, rh->wr_xchannel.flags);

        rh->wr_antsignal = MS(rxsp->status5, AR_RxRSSICombined);

        /* Radiotap Rx flags */
        if (AH9300(ah)->ah_getPlcpHdr) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_PLCP;
        }
        if (rxsp->status11 & AR_CRCErr) {
            /* for now set both */
            rh->wr_flags |= AH_RADIOTAP_F_BADFCS;
            rh->wr_rx_flags |= AH_RADIOTAP_F_RX_BADFCS;
        }
        if (rxsp->status4 & AR_GI) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_HALFGI;
            rh->wr_mcs.flags |= RADIOTAP_MCS_FLAGS_SHORT_GI;
        }
        if (rxsp->status4 & AR_2040) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_40;
            rh->wr_mcs.flags |= RADIOTAP_MCS_FLAGS_40MHZ;
        }
        if (rxsp->status4 & AR_Parallel40) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_40P;
        }
        if (rxsp->status11 & AR_RxAggr) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_AGGR;
        }
        if (rxsp->status11 & AR_RxFirstAggr) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_FIRSTAGGR;
        }
        if (rxsp->status11 & AR_RxMoreAggr) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_MOREAGGR;
        }
        if (rxsp->status2 & AR_RxMore) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_MORE;
        }
        if (rxsp->status11 & AR_PreDelimCRCErr) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_PREDELIM_CRCERR;
        }
        if (rxsp->status11 & AR_PostDelimCRCErr) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_POSTDELIM_CRCERR;
        }
        if (rxsp->status11 & AR_DecryptCRCErr) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_DECRYPTCRCERR;
        }
        if (rxsp->status4 & AR_RxStbc) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_STBC;
        }
        if (rxsp->status11 & AR_PHYErr) {
            rh->wr_rx_flags |= AH_RADIOTAP_11NF_RX_PHYERR;
            rh->ath_ext.phyerr_code = MS(rxsp->status11, AR_PHYErrCode);
        }

        /* add the extension part */
        rh->vendor_ext.oui[0] = (ATH_OUI & 0xff);
        rh->vendor_ext.oui[1] = ((ATH_OUI >> 8) & 0xff);
        rh->vendor_ext.oui[2] = ((ATH_OUI >> 16) & 0xff);
        rh->vendor_ext.skip_length = sizeof(struct ah_rx_vendor_radiotap_header);
        /* sub_namespace - simply set to 0 since we don't use multiple namespaces */
        rh->vendor_ext.sub_namespace = 0;

        rh->ath_ext.version = AH_RADIOTAP_VERSION;

        rh->ath_ext.ch_info.num_chains =
            AR9300_NUM_CHAINS(AH9300(ah)->ah_rxchainmask);

        /*
         * Two-pass strategy to fill in RSSI and EVM:
         * First pass: copy the info from each chain from the
         * descriptor into the corresponding index of the
         * radiotap header's ch_info array.
         * Second pass: use the rxchainmask to determine
         * which chains are valid.  Shift ch_info array
         * elements down to fill in indices that were off
         * in the rxchainmask.
         */
        if (rxsp->status11 & AR_PostDelimCRCErr) {
            for (i = 0; i < RADIOTAP_MAX_CHAINS ; i++) {
               rh->ath_ext.ch_info.rssi_ctl[i] = HAL_RSSI_BAD;
               rh->ath_ext.ch_info.rssi_ext[i] = HAL_RSSI_BAD;
            }
        } else {
            rh->ath_ext.ch_info.rssi_ctl[0] = MS(rxsp->status1, AR_RxRSSIAnt00);
            rh->ath_ext.ch_info.rssi_ctl[1] = MS(rxsp->status1, AR_RxRSSIAnt01);
            rh->ath_ext.ch_info.rssi_ctl[2] = MS(rxsp->status1, AR_RxRSSIAnt02);

            rh->ath_ext.ch_info.rssi_ext[0] = MS(rxsp->status5, AR_RxRSSIAnt10);
            rh->ath_ext.ch_info.rssi_ext[1] = MS(rxsp->status5, AR_RxRSSIAnt11);
            rh->ath_ext.ch_info.rssi_ext[2] = MS(rxsp->status5, AR_RxRSSIAnt12);

            rh->ath_ext.ch_info.evm[0] = rxsp->AR_RxEVM0;
            rh->ath_ext.ch_info.evm[1] = rxsp->AR_RxEVM1;
            rh->ath_ext.ch_info.evm[2] = rxsp->AR_RxEVM2;
        }

        for (i = 0, j = 0; i < RADIOTAP_MAX_CHAINS ; i++) {
            if (AH9300(ah)->ah_rxchainmask & (0x1 << i)) {
                /*
                 * This chain is enabled.  Does its data need
                 * to be shifted down into a lower array index?
                 */
                if (i != j) {
                    rh->ath_ext.ch_info.rssi_ctl[j] =
                        rh->ath_ext.ch_info.rssi_ctl[i];
                    rh->ath_ext.ch_info.rssi_ext[j] =
                        rh->ath_ext.ch_info.rssi_ext[i];
                    /*
                     * EVM fields only need to be shifted if PCU_SEL_EVM bit is set.
                     * Otherwise EVM fields hold the value of PLCP headers which
                     * are not related to chainmask.
                     */
                    if (!(AH9300(ah)->ah_getPlcpHdr)) {
                        rh->ath_ext.ch_info.evm[j] = rh->ath_ext.ch_info.evm[i];
                    }
                }
                j++;
            }
        }

        rh->ath_ext.n_delims = MS(rxsp->status2, AR_NumDelim);
    }// if rh != NULL

    if (ppi != NULL) {        
        struct ah_ppi_pfield_common             *pcommon;
        struct ah_ppi_pfield_mac_extensions     *pmac;
        struct ah_ppi_pfield_macphy_extensions  *pmacphy;
        
        // Fill out PPI Data
        pcommon = ppi->ppi_common;
        pmac = ppi->ppi_mac_ext;
        pmacphy = ppi->ppi_macphy_ext;

        // Zero out all fields
        OS_MEMZERO(pcommon, sizeof(struct ah_ppi_pfield_common));

        /* Common Fields */
        // Grab the tsf
        pcommon->common_tsft =  ar9300GetTsf64(ah);
        // Fill out common flags
        if (rxsp->status11 & AR_CRCErr) {
            pcommon->common_flags |= 4;
        }
        if (rxsp->status11 & AR_PHYErr) {
            pcommon->common_flags |= 8;
        }
        // Set the rate in the calling layer - it's already translated etc there.        
        // Channel frequency
        pcommon->common_chanFreq = (AH_PRIVATE(ah)->ah_curchan)?AH_PRIVATE(ah)->ah_curchan->channel:0;
        // Channel flags
        pcommon->common_chanFlags = AH_PRIVATE(ah)->ah_curchan->channelFlags;
        pcommon->common_dBmAntSignal = MS(rxsp->status5, AR_RxRSSICombined) - 96;

        /* Mac Extensions */

        if (pmac != NULL) {            
            OS_MEMZERO(pmac, sizeof(struct ah_ppi_pfield_mac_extensions));
        // Mac Flag bits : 0 = Greenfield, 1 = HT20[0]/40[1],  2 = SGI, 3 = DupRx, 4 = Aggregate
        
            if (rxsp->status4 & AR_2040) {
                pmac->mac_flags |= 1 << 1;            
            }
            if (rxsp->status4 & AR_GI) {            
                pmac->mac_flags |= 1 << 2;
            }
            if (rxsp->status11 & AR_RxAggr) {
                pmac->mac_flags |= 1 << 4;
            }

            if (rxsp->status11 & AR_RxMoreAggr) {            
                pmac->mac_flags |= 1 << 5;
            }

            if (rxsp->status11 & AR_PostDelimCRCErr) {
                pmac->mac_flags |= 1 << 6;
            }
            
            // pmac->ampduId - get this in the caller where we have all the prototypes.
            pmac->mac_delimiters = MS(rxsp->status2, AR_NumDelim);

        } // if pmac != NULL

        if (pmacphy != NULL) {            
            OS_MEMZERO(pmacphy, sizeof(struct ah_ppi_pfield_macphy_extensions));
            // MacPhy Flag bits : 0=GreenField, 1=HT20[0]/HT40[1], 2=SGI, 3=DupRX, 4=Aggr, 5=MoreAggr, 6=PostDelimError            
            if (rxsp->status4 & AR_2040) {
                pmacphy->macphy_flags |= 1 << 1;
            }
            if (rxsp->status4 & AR_GI) {            
                pmacphy->macphy_flags |= 1 << 2;
            }
            if (rxsp->status11 & AR_RxAggr) {
                pmacphy->macphy_flags |= 1 << 4;
            }

            if (rxsp->status11 & AR_RxMoreAggr) {            
                pmacphy->macphy_flags |= 1 << 5;
            }

            if (rxsp->status11 & AR_PostDelimCRCErr) {
                pmacphy->macphy_flags |= 1 << 6;
            }

            pmacphy->macphy_delimiters = MS(rxsp->status2, AR_NumDelim);

            // macphy_ampduId : Get the AMPDU ID at the upper layer
            // macphy_mcs : Get in calling function - it's available in the ieee layer.
            // Fill macphy_numStreams based on mcs rate : do in caller.

            
            pmacphy->macphy_rssiCombined = MS(rxsp->status5, AR_RxRSSICombined);
            pmacphy->macphy_rssiAnt0Ctl = MS(rxsp->status1, AR_RxRSSIAnt00);
            pmacphy->macphy_rssiAnt1Ctl = MS(rxsp->status1, AR_RxRSSIAnt01);
            pmacphy->macphy_rssiAnt2Ctl = MS(rxsp->status1, AR_RxRSSIAnt02);
            pmacphy->macphy_rssiAnt0Ext = MS(rxsp->status5, AR_RxRSSIAnt10);
            pmacphy->macphy_rssiAnt1Ext = MS(rxsp->status5, AR_RxRSSIAnt11);
            pmacphy->macphy_rssiAnt2Ext = MS(rxsp->status5, AR_RxRSSIAnt12);

            pmacphy->macphy_chanFreq = (AH_PRIVATE(ah)->ah_curchan) ? AH_PRIVATE(ah)->ah_curchan->channel : 0;
            pmacphy->macphy_chanFreq =  AH_PRIVATE(ah)->ah_curchan->channelFlags;

            // Until we are able to combine CTL and EXT rssi's indicate control.
            
            pmacphy->macphy_ant0Signal = MS(rxsp->status1, AR_RxRSSIAnt00)-96;
            pmacphy->macphy_ant1Signal = MS(rxsp->status1, AR_RxRSSIAnt01)-96;
            pmacphy->macphy_ant2Signal = MS(rxsp->status1, AR_RxRSSIAnt02)-96;
            
            pmacphy->macphy_ant0Noise = -96;
            pmacphy->macphy_ant1Noise = -96;
            pmacphy->macphy_ant2Noise = -96;
            
            pmacphy->macphy_evm0 = rxsp->AR_RxEVM0;
            pmacphy->macphy_evm1 = rxsp->AR_RxEVM1;
            pmacphy->macphy_evm2 = rxsp->AR_RxEVM2;
        }

    }

}
#endif /* ATH_SUPPORT_WIRESHARK */

#endif
