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
#include "ah_internal.h"

#include "ar9300/ar9300.h"
#include "ar9300/ar9300reg.h"
#include "ar9300/ar9300phy.h"
#include "ar9300/ar9330_11.ini"
#include "ar9300/ar9485.ini"

/*
 * Take the MHz channel value and set the Channel value
 *
 * ASSUMES: Writes enabled to analog bus
 *
 * Actual Expression,
 *
 * For 2GHz channel,
 * Channel Frequency = (3/4) * freq_ref * (chansel[8:0] + chanfrac[16:0]/2^17)
 * (freq_ref = 40MHz)
 *
 * For 5GHz channel,
 * Channel Frequency = (3/2) * freq_ref * (chansel[8:0] + chanfrac[16:0]/2^10)
 * (freq_ref = 40MHz/(24>>amodeRefSel))
 *
 * For 5GHz channels which are 5MHz spaced,
 * Channel Frequency = (3/2) * freq_ref * (chansel[8:0] + chanfrac[16:0]/2^17)
 * (freq_ref = 40MHz)
 */
static HAL_BOOL
ar9300SetChannel(struct ath_hal *ah,  HAL_CHANNEL_INTERNAL *chan)
{
    u_int16_t bMode, fracMode = 0, aModeRefSel = 0;
    u_int32_t freq, channelSel = 0, reg32 = 0;
#ifndef AR9300_EMULATION_BB
    u_int8_t clk_25mhz = AH9300(ah)->clk_25mhz;
#else
	u_int8_t clk_25mhz = 0;
#endif
    CHAN_CENTERS centers;
#ifdef AR9300_EMULATION_BB
#if !defined(AR9330_EMULATION) && !defined(AR9485_EMULATION) /* To avoid compilation warnings for unused variables */
    u_int32_t ndiv, channelFrac = 0;
    u_int32_t refDivA = 24;
#endif
#else // Silicon
    int loadSynthChannel;
#endif // AR9300_EMULATION_BB

    OS_MARK(ah, AH_MARK_SETCHANNEL, chan->channel);

    ar9300GetChannelCenters(ah, chan, &centers);
    freq = centers.synth_center;

#ifdef AR9300_EMULATION_BB
    reg32 = OS_REG_READ(ah, AR_PHY_SYNTH_CONTROL);
    reg32 &= 0xc0000000;
#endif // AR9300_EMULATION_BB

    if (freq < 4800) {     /* 2 GHz, fractional mode */
#ifdef AR9300_EMULATION_BB

        bMode = 1;
        fracMode = 1;
        aModeRefSel = 0;
        channelSel = CHANSEL_2G(freq); 

#else // Silicon

        if (AR_SREV_HORNET(ah)) {
            u_int32_t ichan = ath_hal_mhz2ieee(ah, freq, chan->channelFlags);
            HALASSERT(ichan > 0 && ichan <= 14);
            if (clk_25mhz) {
                channelSel = ar9331_hornet1_1_chansel_xtal_25M[ichan - 1];
            } else {
                channelSel = ar9331_hornet1_1_chansel_xtal_40M[ichan - 1];
            }
        }else if (AR_SREV_POSEIDON(ah)) {
            u_int32_t channelFrac;
            /* 
             * freq_ref = (40 / (refdiva >> amoderefsel)); (refdiva = 1, amoderefsel = 0)
             * ndiv = ((chan_mhz * 4) / 3) / freq_ref;
             * chansel = int(ndiv),  chanfrac = (ndiv - chansel) * 0x20000
             */
            channelSel = (freq * 4) / 120;
            channelFrac = (((freq * 4) % 120) * 0x20000) / 120;
            channelSel = (channelSel << 17) | (channelFrac);
        } else if (AR_SREV_WASP(ah)) {
            if (clk_25mhz) {
                u_int32_t channelFrac;
                /* 
                 * freq_ref = (50 / (refdiva >> amoderefsel)); (refdiva = 1, amoderefsel = 0)
                 * ndiv = ((chan_mhz * 4) / 3) / freq_ref;
                 * chansel = int(ndiv),  chanfrac = (ndiv - chansel) * 0x20000
                 */
                channelSel = (freq * 2) / 75;
                channelFrac = (((freq * 2) % 75) * 0x20000) / 75;
                channelSel = (channelSel << 17) | (channelFrac);
            } else {
	            u_int32_t channelFrac;
	            /* 
	             * freq_ref = (50 / (refdiva >> amoderefsel)); (refdiva = 1, amoderefsel = 0)
	             * ndiv = ((chan_mhz * 4) / 3) / freq_ref;
	             * chansel = int(ndiv),  chanfrac = (ndiv - chansel) * 0x20000
	             */
	            channelSel = (freq * 2) / 120;
	            channelFrac = (((freq * 2) % 120) * 0x20000) / 120;
	            channelSel = (channelSel << 17) | (channelFrac);
            }
        } else {
            channelSel = CHANSEL_2G(freq); 
        }

        /* Set to 2G mode */
        bMode = 1;

#endif // AR9300_EMULATION_BB
    } 
    else {
#ifdef AR9340_EMULATION
        if (!fracMode) {
            if ((freq % 20) == 0) {
                aModeRefSel = 3;
            } else if ((freq % 10) == 0) {
                aModeRefSel = 2;
            }
            ndiv = (freq * (refDivA >> aModeRefSel))/60;
            channelSel =  ndiv & 0x1ff;         
            channelFrac = (ndiv & 0xfffffe00) * 2;
            channelSel = (channelSel << 17) | channelFrac;
        }
#else
        if (AR_SREV_WASP(ah) && clk_25mhz){
            u_int32_t channelFrac;
            /* 
             * freq_ref = (50 / (refdiva >> amoderefsel)); (refdiva = 1, amoderefsel = 0)
             * ndiv = ((chan_mhz * 2) / 3) / freq_ref;
             * chansel = int(ndiv),  chanfrac = (ndiv - chansel) * 0x20000
             */
            channelSel = freq / 75 ;
            channelFrac = ((freq % 75) * 0x20000) / 75;
            channelSel = (channelSel << 17) | (channelFrac);
        } else {
        channelSel = CHANSEL_5G(freq);
        // Doubler is ON, so, divide channelSel by 2.
        channelSel >>= 1;
        }
#endif
        /* Set to 5G mode */
        bMode = 0;
    }

#ifdef AR9300_EMULATION_BB

    reg32 = reg32 |
           (bMode << 29) |
           (fracMode << 28) |
           (aModeRefSel << 26) |
           channelSel;

    // Set short shift
    reg32 |= (1 << 30);
    OS_REG_WRITE(ah, AR_PHY_SYNTH_CONTROL, reg32);

#else // Silicon

	/* Enable fractional mode for all channels */
	fracMode = 1;
	aModeRefSel = 0;
	loadSynthChannel=0;
    
        reg32 = (bMode << 29);
        OS_REG_WRITE(ah, AR_PHY_SYNTH_CONTROL, reg32);


	/* Enable Long shift Select for Synthesizer */
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_SYNTH4, AR_PHY_SYNTH4_LONG_SHIFT_SELECT, 1);

    /* program synth. setting */
	reg32 = (channelSel<<2) | (fracMode<<30) | (aModeRefSel<<28) | (loadSynthChannel<<31);
        if (IS_CHAN_QUARTER_RATE(chan)) {
            reg32 += CHANSEL_5G_DOT5MHZ; 
        }
	OS_REG_WRITE(ah, AR_PHY_65NM_CH0_SYNTH7, reg32);

	/* Toggle Load Synth channel bit */
	loadSynthChannel=1;
	reg32= (channelSel<<2) | (fracMode<<30) | (aModeRefSel<<28) | (loadSynthChannel<<31);
        if (IS_CHAN_QUARTER_RATE(chan)) {
            reg32 += CHANSEL_5G_DOT5MHZ; 
        }
	OS_REG_WRITE(ah, AR_PHY_65NM_CH0_SYNTH7, reg32);

#endif // AR9300_EMULATION_BB

    AH_PRIVATE(ah)->ah_curchan = chan;

#ifdef AH_SUPPORT_DFS
    if (chan->privFlags & CHANNEL_DFS) {
        struct ar9300RadarState *rs;
        u_int8_t index;

        rs = ar9300GetRadarChanState(ah, &index);
        if (rs != AH_NULL) {
            AH9300(ah)->ah_curchanRadIndex = (int16_t) index;
        } else {
            HDPRINTF(ah, HAL_DBG_DFS, "%s: Couldn't find radar state information\n",
                 __func__);
            return AH_FALSE;
        }
    } else
#endif
        AH9300(ah)->ah_curchanRadIndex = -1;

    return AH_TRUE;
}


static HAL_BOOL
ar9300GetChipPowerLimits(struct ath_hal *ah, HAL_CHANNEL *chans, u_int32_t nchans)
{
    HAL_BOOL retVal = AH_TRUE;
    int i;

    for (i=0; i < nchans; i ++) {
        chans[i].maxTxPower = AR9300_MAX_RATE_POWER;
        chans[i].minTxPower = AR9300_MAX_RATE_POWER;
    }
    return (retVal);
}

HAL_BOOL
ar9300RfAttach(struct ath_hal *ah, HAL_STATUS *status)
{
    struct ath_hal_9300 *ahp = AH9300(ah);

    ahp->ah_rfHal.setChannel    = ar9300SetChannel;
    ahp->ah_rfHal.getChipPowerLim   = ar9300GetChipPowerLimits;

	*status = HAL_OK;

    return AH_TRUE;
}

#endif /* AH_SUPPORT_AR9300 */
