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

#ifdef AH_SUPPORT_AR5212

#include "ah.h"
#include "ah_internal.h"
#include "ah_devid.h"
#ifdef AH_DEBUG
#include "ah_desc.h"			/* NB: for HAL_PHYERR* */
#endif
#include "ah_eeprom.h"

#include "ar5212/ar5212.h"
#include "ar5212/ar5212reg.h"
#include "ar5212/ar5212phy.h"
#ifdef AH_SUPPORT_AR5311
#include "ar5212/ar5311reg.h"
#endif


#define AR5212_EEPROM_S
#define AR5212_EEPROM_OFFSET        0x2000
#ifdef AR9100
#define AR5212_EEPROM_START_ADDR    0x1fff1000
#else
#define AR5212_EEPROM_START_ADDR    0xb07a1000
#endif
#define AR5212_EEPROM_MAX           0xae0


/*
 * Read 16 bits of data from offset into *data
 */
HAL_BOOL
ar5212EepromRead(struct ath_hal *ah, u_int off, u_int16_t *data)
{
	OS_REG_WRITE(ah, AR_EEPROM_ADDR, off);
	OS_REG_WRITE(ah, AR_EEPROM_CMD, AR_EEPROM_CMD_READ);

	if (!ath_hal_wait(ah, AR_EEPROM_STS,
	    AR_EEPROM_STS_READ_COMPLETE | AR_EEPROM_STS_READ_ERROR,
	    AR_EEPROM_STS_READ_COMPLETE, AH_WAIT_TIMEOUT)) {
		HDPRINTF(ah, HAL_DBG_EEPROM, "%s: read failed for entry 0x%x\n", __func__, off);
		return AH_FALSE;
	}
	*data = OS_REG_READ(ah, AR_EEPROM_DATA) & 0xffff;
	return AH_TRUE;
}

#ifdef AH_SUPPORT_WRITE_EEPROM
/*
 * Write 16 bits of data from data to the specified EEPROM offset.
 */
HAL_BOOL
ar5212EepromWrite(struct ath_hal *ah, u_int off, u_int16_t data)
{
    u_int32_t status;
    int to = 15000;     /* 15ms timeout */
    /* Add Nala's Mac Version (2006/10/30) */
    u_int32_t reg = 0;

    if (AH_PRIVATE(ah)->ah_isPciExpress) {
        /* Set GPIO 3 to output and then to 0 (ACTIVE_LOW) */
        if(IS_2425(ah)) {
            reg = OS_REG_READ(ah, AR_GPIOCR);
            OS_REG_WRITE(ah, AR_GPIOCR, 0xFFFF);
        } else {
            ar5212GpioCfgOutput(ah, 3, HAL_GPIO_OUTPUT_MUX_AS_OUTPUT);
            ar5212GpioSet(ah, 3, 0);
        }
        
    } else if (IS_2413(ah) || IS_5413(ah)) {
        /* Set GPIO 4 to output and then to 0 (ACTIVE_LOW) */
        ar5212GpioCfgOutput(ah, 4, HAL_GPIO_OUTPUT_MUX_AS_OUTPUT);
        ar5212GpioSet(ah, 4, 0);
    } else if (IS_2417(ah)){
        /* Add for Nala but not sure (2006/10/30) */
        reg = OS_REG_READ(ah, AR_GPIOCR);
        OS_REG_WRITE(ah, AR_GPIOCR, 0xFFFF);    
    }

    /* Send write data */
    OS_REG_WRITE(ah, AR_EEPROM_ADDR, off);
    OS_REG_WRITE(ah, AR_EEPROM_DATA, data);
    OS_REG_WRITE(ah, AR_EEPROM_CMD, AR_EEPROM_CMD_WRITE);

    while (to > 0) {
        OS_DELAY(1);
        status = OS_REG_READ(ah, AR_EEPROM_STS);
        if (status & AR_EEPROM_STS_WRITE_COMPLETE) {
            if (AH_PRIVATE(ah)->ah_isPciExpress) {
                if(IS_2425(ah)) {
                    OS_REG_WRITE(ah, AR_GPIOCR, reg);
                } else {
                    ar5212GpioSet(ah, 3, 1);
                }
            } else if (IS_2413(ah) || IS_5413(ah)) {
                ar5212GpioSet(ah, 4, 1);
            } else if (IS_2417(ah)){
                /* Add for Nala but not sure (2006/10/30) */
                OS_REG_WRITE(ah, AR_GPIOCR, reg);
            }
            return AH_TRUE;
        }

        if (status & AR_EEPROM_STS_WRITE_ERROR)    {
		    HDPRINTF(ah, HAL_DBG_EEPROM, "%s: write failed for entry 0x%x, data 0x%x\n",
			    __func__, off, data);
            return AH_FALSE;;
        }
        to--;
    }

	HDPRINTF(ah, HAL_DBG_EEPROM, "%s: write timeout for entry 0x%x, data 0x%x\n",
		__func__, off, data);

    if (AH_PRIVATE(ah)->ah_isPciExpress) {
        if(IS_2425(ah)) {
            OS_REG_WRITE(ah, AR_GPIOCR, reg);
        } else {
            ar5212GpioSet(ah, 3, 1);
        }
    } else if (IS_2413(ah) || IS_5413(ah)) {
        ar5212GpioSet(ah, 4, 1);
    } else if (IS_2417(ah)){
        /* Add for Nala but not sure (2006/10/30) */
        OS_REG_WRITE(ah, AR_GPIOCR, reg);
    }

    return AH_FALSE;
}
#endif /* AH_SUPPORT_WRITE_EEPROM */

#ifndef WIN32
/*************************
 * Flash APIs for AP only
 *************************/

HAL_STATUS
ar5212FlashMap(struct ath_hal *ah)
{
    struct ath_hal_5212 *ahp = AH5212(ah);

#ifdef __CARRIER_PLATFORM__
    ath_carr_get_cal_mem_legacy(ahp->ah_cal_mem);
#else /* __CARRIER_PLATFORM__ */

    #ifdef AR9100
        ahp->ah_cal_mem = OS_REMAP(
            ah, AR5212_EEPROM_START_ADDR, AR5212_EEPROM_MAX);
    #else
        ahp->ah_cal_mem = OS_REMAP(
            (uintptr_t) AR5212_EEPROM_START_ADDR, AR5212_EEPROM_MAX);
    #endif

#endif  /* __CARRIER_PLATFORM__ */

    if (!ahp->ah_cal_mem) {
        HDPRINTF(ah, HAL_DBG_EEPROM,
            "%s: cannot remap eeprom region \n", __func__);
        return HAL_EIO;
    }

    return HAL_OK;
}

HAL_BOOL
ar5212FlashRead(struct ath_hal *ah, u_int off, u_int16_t *data)
{
    struct ath_hal_5212 *ahp = AH5212(ah);

    *data = ((u_int16_t *)ahp->ah_cal_mem)[off];
    return AH_TRUE;

}

HAL_BOOL
ar5212FlashWrite(struct ath_hal *ah, u_int off, u_int16_t data)
{
    struct ath_hal_5212 *ahp = AH5212(ah);

    ((u_int16_t *)ahp->ah_cal_mem)[off] = data;
    return AH_TRUE;
}
#endif /* WIN32 */

#endif /* AH_SUPPORT_AR5212 */
