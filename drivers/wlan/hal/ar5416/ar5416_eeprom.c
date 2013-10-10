/*
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2005 Atheros Communications, Inc.
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

#ifdef AH_SUPPORT_AR5416

#include "ah.h"
#include "ah_internal.h"
#include "ah_devid.h"
#ifdef AH_DEBUG
#include "ah_desc.h"                    /* NB: for HAL_PHYERR* */
#endif
#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"
#include "ar5416/ar5416phy.h"

static u_int16_t ar5416EepromGetSpurChan(struct ath_hal *ah, u_int16_t spurChan,HAL_BOOL is2GHz);
static inline HAL_BOOL ar5416FillEeprom(struct ath_hal *ah);
static inline HAL_STATUS ar5416CheckEeprom(struct ath_hal *ah);

#ifdef AH_PRIVATE_DIAG
static inline void ar5416FillEmuEeprom(struct ath_hal_5416 *ahp);
#else
#define ar5416FillEmuEeprom(_ahp)
#endif /* AH_PRIVATE_DIAG */

#define FIXED_CCA_THRESHOLD 15

/*****************************
 * Eeprom APIs for CB/XB only
 ****************************/

/*
 * Read 16 bits of data from offset into *data
 */
HAL_BOOL
ar5416EepromRead(struct ath_hal *ah, u_int off, u_int16_t *data)
{
    (void)OS_REG_READ(ah, AR5416_EEPROM_OFFSET + (off << AR5416_EEPROM_S));
    if (!ath_hal_wait(ah, AR_EEPROM_STATUS_DATA, AR_EEPROM_STATUS_DATA_BUSY
        | AR_EEPROM_STATUS_DATA_PROT_ACCESS, 0, AH_WAIT_TIMEOUT))
    {
        return AH_FALSE;
    }

    *data = MS(OS_REG_READ(ah, AR_EEPROM_STATUS_DATA), AR_EEPROM_STATUS_DATA_VAL);
    return AH_TRUE;
}

#ifdef AH_SUPPORT_WRITE_EEPROM
/*
 * Write 16 bits of data from data to the specified EEPROM offset.
 */
HAL_BOOL
ar5416EepromWrite(struct ath_hal *ah, u_int off, u_int16_t data)
{
    u_int32_t status;
    u_int32_t write_to = 50000;      /* write timeout */
    u_int32_t eepromValue;

    eepromValue = (u_int32_t)data & 0xffff;

    /* Setup EEPROM device to write */
    OS_REG_RMW(ah, AR_GPIO_OUTPUT_MUX1, 0, 0x1f << 15);   /* Mux GPIO-3 as GPIO */
    OS_DELAY(1);
    OS_REG_RMW(ah, AR_GPIO_OE_OUT, 0xc0, 0xc0);     /* Configure GPIO-3 as output */
    OS_DELAY(1);
    OS_REG_RMW(ah, AR_GPIO_IN_OUT, 0, 1 << 3);       /* drive GPIO-3 low */
    OS_DELAY(1);

    /* Send write data, as 32 bit data */
    OS_REG_WRITE(ah, AR5416_EEPROM_OFFSET + (off << AR5416_EEPROM_S), eepromValue);

    /* check busy bit to see if eeprom write succeeded */
    while (write_to > 0) {
        status = OS_REG_READ(ah, AR_EEPROM_STATUS_DATA) &
                                    (AR_EEPROM_STATUS_DATA_BUSY |
                                     AR_EEPROM_STATUS_DATA_BUSY_ACCESS |
                                     AR_EEPROM_STATUS_DATA_PROT_ACCESS |
                                     AR_EEPROM_STATUS_DATA_ABSENT_ACCESS);
        if (status == 0) {
            OS_REG_RMW(ah, AR_GPIO_IN_OUT, 1<<3, 1<<3);       /* drive GPIO-3 hi */
            return AH_TRUE;
        }
        OS_DELAY(1);
        write_to--;
    }

    OS_REG_RMW(ah, AR_GPIO_IN_OUT, 1<<3, 1<<3);       /* drive GPIO-3 hi */
    return AH_FALSE;
}
#endif /* AH_SUPPORT_WRITE_EEPROM */

#ifndef WIN32
/*************************
 * Flash APIs for AP only
 *************************/

static HAL_STATUS
ar5416FlashMap(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
#ifdef AR9100
    ahp->ah_cal_mem = OS_REMAP(ah, AR5416_EEPROM_START_ADDR, AR5416_EEPROM_MAX);
#else
    ahp->ah_cal_mem = OS_REMAP((uintptr_t)ah->ah_st, AR5416_EEPROM_MAX);
#endif
    if (!ahp->ah_cal_mem)
    {
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: cannot remap eeprom region \n", __func__);
        return HAL_EIO;
    }

    return HAL_OK;
}

HAL_BOOL
ar5416FlashRead(struct ath_hal *ah, u_int off, u_int16_t *data)
{
    struct ath_hal_5416 *ahp = AH5416(ah);

    *data = ((u_int16_t *)ahp->ah_cal_mem)[off];
    return AH_TRUE;
}

HAL_BOOL
ar5416FlashWrite(struct ath_hal *ah, u_int off, u_int16_t data)
{
    struct ath_hal_5416 *ahp = AH5416(ah);

    ((u_int16_t *)ahp->ah_cal_mem)[off] = data;
    return AH_TRUE;
}
#endif /* WIN32 */



/***************************
 * Common APIs for AP/CB/XB
 ***************************/

HAL_STATUS
ar5416EepromAttach(struct ath_hal *ah)
{
	struct ath_hal_5416 *ahp = AH5416(ah);

#ifndef WIN32
    if (ar5416EepDataInFlash(ah))
        ar5416FlashMap(ah);
#endif

	if (AR_SREV_KIWI(ah) )
		ahp->ah_eep_map = EEP_MAP_AR9287;
	else if (AR_SREV_KITE(ah) || AR_SREV_K2(ah)) 
		ahp->ah_eep_map = EEP_MAP_4KBITS;
	else
		ahp->ah_eep_map = EEP_MAP_DEFAULT;

    AH_PRIVATE(ah)->ah_eepromGetSpurChan = ar5416EepromGetSpurChan;
    if (!ar5416FillEeprom(ah)) {
        /* eeprom read failure => assume emulation board */
        if (ahp->ah_priv.priv.ah_config.ath_hal_soft_eeprom) {
            ar5416FillEmuEeprom(ahp);
            ahp->ah_emu_eeprom = 1;
            return HAL_OK;
        } else {
            return HAL_EIO;
        }
    }
#ifndef ART_BUILD
    return ar5416CheckEeprom(ah);
#else
    if (ar5416CheckEeprom(ah) == HAL_OK)
    {
        return HAL_OK;
    }
    else
    {   
        /* The data of EEPROM is unavailible */
        if (AR_SREV_KIWI(ah))
        {
            /* Use default eeprom's data */
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: EEPROM is unavaible. Load EEPROM default value\n", __func__);
            ar9287EepromLoadDefaults(ah);
            return HAL_OK;
        }
        else
        {
            return HAL_EEBADSUM;
        }
    }
#endif     
}

u_int32_t
ar5416EepromGet(struct ath_hal_5416 *ahp, EEPROM_PARAM param)
{
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		return ar9287EepromGet(ahp, param);
	else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		return ar5416Eeprom4kGet(ahp, param);
	else
		return ar5416EepromDefGet(ahp, param);
}

#ifdef AH_SUPPORT_WRITE_EEPROM
/**************************************************************
 * ar5416EepromSetParam
 */
HAL_BOOL
ar5416EepromSetParam(struct ath_hal *ah, EEPROM_PARAM param, u_int32_t value)
{
	struct ath_hal_5416 *ahp = AH5416(ah);
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		return ar9287EepromSetParam(ah, param, value);
	else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		return ar5416Eeprom4kSetParam(ah, param, value);
	else
		return ar5416EepromDefSetParam(ah, param, value);
}
#endif //#ifdef AH_SUPPORT_WRITE_EEPROM

/*
 * Read EEPROM header info and program the device for correct operation
 * given the channel value.
 */
HAL_BOOL
ar5416EepromSetBoardValues(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{
	struct ath_hal_5416 *ahp = AH5416(ah);
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		return ar9287EepromSetBoardValues(ah, chan);
	else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		return ar5416Eeprom4kSetBoardValues(ah, chan);
	else
		return ar5416EepromDefSetBoardValues(ah, chan);
}

/******************************************************************************/
/*!
**  \brief EEPROM fixup code for INI values
** 
** This routine provides a place to insert "fixup" code for specific devices
** that need to modify INI values based on EEPROM values, BEFORE the INI values
** are written.  Certain registers in the INI file can only be written once without
** undesired side effects, and this provides a place for EEPROM overrides in these
** cases.
**
** This is called at attach time once.  It should not affect run time performance
** at all
**
**  \param ah       Pointer to HAL object (this)
**  \param pEepData Pointer to (filled in) eeprom data structure
**  \param reg      register being inspected on this call
**  \param value    value in INI file
**
**  \return Updated value for INI file.
*/

u_int32_t
ar5416INIFixup(struct ath_hal *ah,ar5416_eeprom_t *pEepData, u_int32_t reg, u_int32_t value)
{
	struct ath_hal_5416 *ahp = AH5416(ah);
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		return ar9287EepromINIFixup(ah, &pEepData->map.mapAr9287, reg, value);
	else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		return ar5416Eeprom4kINIFixup(ah, &pEepData->map.map4k, reg, value);
	else
		return ar5416EepromDefINIFixup(ah, &pEepData->map.def, reg, value);
}

/**************************************************************
 * ar5416EepromSetTransmitPower
 *
 * Set the transmit power in the baseband for the given
 * operating channel and mode.
 */
HAL_STATUS
ar5416EepromSetTransmitPower(struct ath_hal *ah,
    ar5416_eeprom_t *pEepData, HAL_CHANNEL_INTERNAL *chan, u_int16_t cfgCtl,
    u_int16_t twiceAntennaReduction, u_int16_t twiceMaxRegulatoryPower,
    u_int16_t powerLimit)
{
	struct ath_hal_5416 *ahp = AH5416(ah);
	if (ahp->ah_eep_map == EEP_MAP_AR9287) 
		return ar9287EepromSetTransmitPower(ah, &pEepData->map.mapAr9287, 
					chan, cfgCtl, twiceAntennaReduction, twiceMaxRegulatoryPower, powerLimit);

	else if (ahp->ah_eep_map == EEP_MAP_4KBITS) 
		return ar5416Eeprom4kSetTransmitPower(ah, &pEepData->map.map4k, 
					chan, cfgCtl, twiceAntennaReduction, twiceMaxRegulatoryPower, powerLimit);
	else
		return ar5416EepromDefSetTransmitPower(ah, &pEepData->map.def, 
					chan, cfgCtl, twiceAntennaReduction, twiceMaxRegulatoryPower, powerLimit);
}

/**************************************************************
 * ar5416EepromSetAddac
 *
 * Set the ADDAC from eeprom for Sowl.
 */
void
ar5416EepromSetAddac(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{
	struct ath_hal_5416 *ahp = AH5416(ah);
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		ar9287EepromSetAddac(ah, chan);
	else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		ar5416Eeprom4kSetAddac(ah, chan);
	else
		ar5416EepromDefSetAddac(ah, chan);
}

u_int
ar5416EepromDumpSupport(struct ath_hal *ah, void **ppE)
{
    *ppE = &(AH5416(ah)->ah_eeprom);
    return sizeof(ar5416_eeprom_t);
}

u_int8_t
ar5416EepromGetNumAntConfig(struct ath_hal_5416 *ahp, HAL_FREQ_BAND freq_band)
{
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		return ar9287EepromGetNumAntConfig(ahp, freq_band);
	else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		return ar5416Eeprom4kGetNumAntConfig(ahp, freq_band);
	else
		return ar5416EepromDefGetNumAntConfig(ahp, freq_band);
}

HAL_STATUS
ar5416EepromGetAntCfg(struct ath_hal_5416 *ahp, HAL_CHANNEL_INTERNAL *chan,
                   u_int8_t index, u_int32_t *config)
{
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		return ar9287EepromGetAntCfg(ahp, chan, index, config);
	else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		return ar5416Eeprom4kGetAntCfg(ahp, chan, index, config);
	else
		return ar5416EepromDefGetAntCfg(ahp, chan, index, config);
}

u_int8_t* 
ar5416EepromGetCustData(struct ath_hal_5416 *ahp)
{
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		return ahp->ah_eeprom.map.mapAr9287.custData;
	else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		return ahp->ah_eeprom.map.map4k.custData;
	else
		return ahp->ah_eeprom.map.def.custData;
}

/***************************************
 * Helper functions common for AP/CB/XB
 **************************************/

/**************************************************************
 * ar5416GetTargetPowers
 *
 * Return the rates of target power for the given target power table
 * channel, and number of channels
 */
void
ar5416GetTargetPowers(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan,
    CAL_TARGET_POWER_HT *powInfo, u_int16_t numChannels,
    CAL_TARGET_POWER_HT *pNewPower, u_int16_t numRates,
    HAL_BOOL isHt40Target)
{
    u_int16_t clo, chi;
    int i;
    int matchIndex = -1, lowIndex = -1;
    u_int16_t freq;
    CHAN_CENTERS centers;

    ar5416GetChannelCenters(ah, chan, &centers);
    freq = isHt40Target ? centers.synth_center : centers.ctl_center;

    /* Copy the target powers into the temp channel list */
    if (freq <= fbin2freq(powInfo[0].bChannel, IS_CHAN_2GHZ(chan)))
    {
        matchIndex = 0;
    }
    else
    {
        for (i = 0; (i < numChannels) && (powInfo[i].bChannel != AR5416_BCHAN_UNUSED); i++)
        {
            if (freq == fbin2freq(powInfo[i].bChannel, IS_CHAN_2GHZ(chan)))
            {
                matchIndex = i;
                break;
            }
            else if ((freq < fbin2freq(powInfo[i].bChannel, IS_CHAN_2GHZ(chan))) &&
                (freq > fbin2freq(powInfo[i - 1].bChannel, IS_CHAN_2GHZ(chan))))
            {
                lowIndex = i - 1;
                break;
            }
        }
        if ((matchIndex == -1) && (lowIndex == -1))
        {
            HALASSERT(freq > fbin2freq(powInfo[i - 1].bChannel, IS_CHAN_2GHZ(chan)));
            matchIndex = i - 1;
        }
    }

    if (matchIndex != -1)
    {
        *pNewPower = powInfo[matchIndex];
    }
    else
    {
        HALASSERT(lowIndex != -1);
        /*
        * Get the lower and upper channels, target powers,
        * and interpolate between them.
        */
        clo = fbin2freq(powInfo[lowIndex].bChannel, IS_CHAN_2GHZ(chan));
        chi = fbin2freq(powInfo[lowIndex + 1].bChannel, IS_CHAN_2GHZ(chan));

        for (i = 0; i < numRates; i++)
        {
            pNewPower->tPow2x[i] = (u_int8_t)interpolate(freq, clo, chi,
                powInfo[lowIndex].tPow2x[i], powInfo[lowIndex + 1].tPow2x[i]);
        }
    }
}

/**************************************************************
 * ar5416GetTargetPowersLeg
 *
 * Return the four rates of target power for the given target power table
 * channel, and number of channels
 */
void
ar5416GetTargetPowersLeg(struct ath_hal *ah,
    HAL_CHANNEL_INTERNAL *chan,
    CAL_TARGET_POWER_LEG *powInfo, u_int16_t numChannels,
    CAL_TARGET_POWER_LEG *pNewPower, u_int16_t numRates,
    HAL_BOOL isExtTarget)
{
    u_int16_t clo, chi;
    int i;
    int matchIndex = -1, lowIndex = -1;
    u_int16_t freq;
    CHAN_CENTERS centers;

    ar5416GetChannelCenters(ah, chan, &centers);
    freq = (isExtTarget) ? centers.ext_center : centers.ctl_center;

    /* Copy the target powers into the temp channel list */
    if (freq <= fbin2freq(powInfo[0].bChannel, IS_CHAN_2GHZ(chan)))
    {
        matchIndex = 0;
    }
    else
    {
        for (i = 0; (i < numChannels) && (powInfo[i].bChannel != AR5416_BCHAN_UNUSED); i++)
        {
            if (freq == fbin2freq(powInfo[i].bChannel, IS_CHAN_2GHZ(chan)))
            {
                matchIndex = i;
                break;
            }
            else if ((freq < fbin2freq(powInfo[i].bChannel, IS_CHAN_2GHZ(chan))) &&
                (freq > fbin2freq(powInfo[i - 1].bChannel, IS_CHAN_2GHZ(chan))))
            {
                lowIndex = i - 1;
                break;
            }
        }
        if ((matchIndex == -1) && (lowIndex == -1))
        {
            HALASSERT(freq > fbin2freq(powInfo[i - 1].bChannel, IS_CHAN_2GHZ(chan)));
            matchIndex = i - 1;
        }
    }

    if (matchIndex != -1)
    {
        *pNewPower = powInfo[matchIndex];
    }
    else
    {
        HALASSERT(lowIndex != -1);
        /*
        * Get the lower and upper channels, target powers,
        * and interpolate between them.
        */
        clo = fbin2freq(powInfo[lowIndex].bChannel, IS_CHAN_2GHZ(chan));
        chi = fbin2freq(powInfo[lowIndex + 1].bChannel, IS_CHAN_2GHZ(chan));

        for (i = 0; i < numRates; i++)
        {
            pNewPower->tPow2x[i] = (u_int8_t)interpolate(freq, clo, chi,
                powInfo[lowIndex].tPow2x[i], powInfo[lowIndex + 1].tPow2x[i]);
        }
    }
}

static inline HAL_STATUS
ar5416CheckEeprom(struct ath_hal *ah)
{
	struct ath_hal_5416 *ahp = AH5416(ah);
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		return ar9287CheckEeprom(ah);
	else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		return ar5416CheckEeprom4k(ah);
	else
		return ar5416CheckEepromDef(ah);
}

static u_int16_t 
ar5416EepromGetSpurChan(struct ath_hal *ah, u_int16_t i,HAL_BOOL is2GHz)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar5416_eeprom_t *eep = (ar5416_eeprom_t *)&ahp->ah_eeprom;
    u_int16_t   spur_val = AR_NO_SPUR;

    HALASSERT(i <  AR_EEPROM_MODAL_SPURS );
    
    HDPRINTF(ah, HAL_DBG_ANI, 
             "Getting spur idx %d is2Ghz. %d val %x\n",
             i, is2GHz, AH_PRIVATE(ah)->ah_config.ath_hal_spurChans[i][is2GHz]);

    switch(AH_PRIVATE(ah)->ah_config.ath_hal_spurMode)
    {
    case SPUR_DISABLE:
        /* returns AR_NO_SPUR */
        break;
    case SPUR_ENABLE_IOCTL:
        spur_val = AH_PRIVATE(ah)->ah_config.ath_hal_spurChans[i][is2GHz];
        HDPRINTF(ah, HAL_DBG_ANI, "Getting spur val from new loc. %d\n", spur_val);
        break;
    case SPUR_ENABLE_EEPROM:
		if (ahp->ah_eep_map == EEP_MAP_AR9287)
			spur_val = eep->map.mapAr9287.modalHeader.spurChans[i].spurChan;
		else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
			spur_val = eep->map.map4k.modalHeader.spurChans[i].spurChan;
		else
			spur_val = eep->map.def.modalHeader[is2GHz].spurChans[i].spurChan;
        break;

    }
    return spur_val;
}

#ifdef AH_PRIVATE_DIAG
static inline void
ar5416FillEmuEeprom(struct ath_hal_5416 *ahp)
{
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		ar9287FillEmuEeprom(ahp);
    else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		ar5416FillEmuEeprom4k(ahp);
	else ar5416FillEmuEepromDef(ahp);
}
#endif /* AH_PRIVATE_DIAG */

static inline HAL_BOOL
ar5416FillEeprom(struct ath_hal *ah)
{
	struct ath_hal_5416 *ahp = AH5416(ah);
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		return ar9287FillEeprom(ah);
    else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		return ar5416FillEeprom4k(ah);
	else
		return ar5416FillEepromDef(ah);
}

#endif /* AH_SUPPORT_AR5416 */

